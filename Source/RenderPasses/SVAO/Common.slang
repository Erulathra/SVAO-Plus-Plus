import Scene.Camera.Camera;
import VAOData;
import Scene.Intersection;
import Scene.Shading;
import Rendering.Materials.TexLODHelpers;
import Rendering.Materials.TexLODTypes;
import Utils.Math.PackedFormats;

#include "Utils/Math/MathConstants.slangh"

// single depth texture
#define DEPTH_MODE_SINGLE 0
// single or dual depth texture + stochastic depth texture
#define DEPTH_MODE_STOCHASTIC 1

#ifndef SECONDARY_DEPTH_MODE
#define SECONDARY_DEPTH_MODE DEPTH_MODE_SINGLE
#endif // SECONDARY_DEPTH_MODE

#ifndef NUM_DIRECTIONS
#define NUM_DIRECTIONS 8
#endif // NUM_DIRECTIONS

#ifndef MSAA_SAMPLES
#define MSAA_SAMPLES 4
#endif // MSAA_SAMPLES

#ifndef RAY_CONE_SPREAD
#define RAY_CONE_SPREAD 0.f
#endif // RAY_CONE_SPREAD

#ifndef SD_JITTER
#define SD_JITTER 0
#endif // SD_JITTER

#ifndef USE_RAY_INTERVAL
#define USE_RAY_INTERVAL 0
#endif //USE_RAY_INTERVAL

#ifndef ADAPTIVE_SAMPLING
#define ADAPTIVE_SAMPLING 0
#endif //USE_RAY_INTERVAL

// area where the halo effect remains constant at 0.0
#define CONST_RADIUS ((1.0 + gData.thickness) * data.radius - sphereStart)
#define HALO_RADIUS sphereStart
#define COMBINE_VIS(a,b) min(a,b)

// normalized radius for each of the NUM_DIRECTION samples (distributed with radical inverse => see SSAO::setKernel() radius)
#if NUM_DIRECTIONS == 8
static const float sampleRadius[8] = { 0.917883, 0.564429, 0.734504, 0.359545, 0.820004, 0.470149, 0.650919, 0.205215 };
#elif NUM_DIRECTIONS == 16
static const float sampleRadius[16] = {0.949098221604059, 0.5865639019441775, 0.7554681720909893, 0.3895439574863043, 0.8425560503012255, 0.4948003867747738, 0.6719196866381647, 0.25203100417434543, 0.8908588816103737, 0.5418210823278604, 0.7136427497994143, 0.32724136087586453, 0.7980920320691521, 0.4445340224611676, 0.6297373536812639, 0.1447182620692375};
#else // 32
static const float sampleRadius[32] = {0.9682458365518543, 0.5974803093982587, 0.7660169295429302, 0.4038472576817624, 0.8541535023444914, 0.5068159098187986, 0.6823727109604635, 0.2726076670970059, 0.904018191941786, 0.5531894754180758, 0.7240656647095169, 0.34372202910162664, 0.8089818132350507, 0.45747336127867605, 0.640354849019649, 0.17748061996818404, 0.9327350969376332, 0.5755500192397054, 0.7449678114312224, 0.37479566486456295, 0.8311856199411515, 0.4825843210309559, 0.6614378277661477, 0.22975243551455923, 0.878233108646881, 0.5303115209931901, 0.7032256306171377, 0.3099952198410562, 0.7873133907642258, 0.43130429537268, 0.6190581352335289, 0.10219580968897692};
#endif

cbuffer StaticCB
{
    VAOData gData;
}

cbuffer PerFrameCB
{
    float4x4 invViewMat;
    Camera gCamera;
    uint guardBand;
    uint frameIndex;
}

SamplerState gNoiseSampler;
SamplerState gTextureSampler;


Texture2D<float> gLinearDepthIn;
Texture2D<float4> gLinearSDIn;

Texture2D<uint> gNormalIn;
Texture2D<float> gNoiseTex;

float3 loadNormal(float2 texC)
{
    // return gNormalIn[texC * gData.resolution];
    uint packedNormal = gNormalIn[texC * gData.resolution];
    return decodeNormal2x8(packedNormal);
}

float2 getScreenClampedUV(float2 uvstart, float2 uvend)
{
    return saturate(uvend);
}

float2 getSDClampedUV(float2 uvstart, float2 uvend)
{
    float2 offset = float(gData.sdGuard) / gData.lowResolution;
    return clamp(uvend, -offset, 1.0 + offset);
}

float2 getSnappedUV(float2 uv, float2 resolution)
{
    float2 pixelCoord = floor(uv * resolution);
    return float2((pixelCoord.x + 0.5f) / resolution.x, (pixelCoord.y + 0.5f) / resolution.y);
}

float2 getSnappedUV(float2 uv)
{
    return getSnappedUV(uv, gData.resolution);
}

bool isSamePixel(float2 uv1, float2 uv2)
{
    return all(abs(uv1 - uv2) < gData.invResolution * 0.9);
}

// uv: uv coordinates [0, 1]
// viewDepth: linear depth in view space (positive z)
// return: view space position (negative z)
float3 UVToViewSpace(float2 uv, float viewDepth)
{
    float2 ndc = float2(uv.x, 1.0 - uv.y) * 2.0 - 1.0; // normalized device coordinates [-1, 1]
    const float2 imageScale = 0.5 * float2(gCamera.data.frameWidth / gCamera.data.focalLength, gCamera.data.frameHeight / gCamera.data.focalLength);
    return float3(ndc * viewDepth * imageScale, -viewDepth);
}

// posV: view space position (negative z)
// return: texture uv [0, 1]
float2 ViewSpaceToUV(float3 posV)
{
    const float2 imageScale = 0.5 * float2(gCamera.data.frameWidth / gCamera.data.focalLength, gCamera.data.frameHeight / gCamera.data.focalLength);
    float2 ndc = posV.xy / (imageScale * posV.z);
    return ndc * float2(-0.5, 0.5) + 0.5; // since posV.z is negative, the sign order is inversed
}

int2 UVToPixel(float2 uv)
{
    float width, height;
    gLinearDepthIn.GetDimensions(width, height);
    return int2(floor(uv * float2(width, height)));
}

// converts from screen uv coordinates to pixel coordinates of stochastic depth map.
// UV can be negative or greater than one if the guard band for the SD-map is used (gData.sdGuard)
int2 UVToSDPixel(float2 uv)
{
    int2 pixel = int2(floor(uv * gData.lowResolution)) + int2(gData.sdGuard);
    return clamp(pixel, 0, int2(gData.lowResolution) + gData.sdGuard * 2 - 1);
}

float makeNonZero(float value, float epsilon)
{
    const float absValue = max(abs(value), epsilon);

    [flatten] if (value >= 0)
        return absValue;
    else
        return -absValue;
}

// get rid of shadowing around edges
// introduce a linear falloff function that starts with 0.0 when the sample depth intersects the front sphere exactly,
// and falls of to 1.0 when it gets further away from the sphere but closer to the camera.
// this also includes the constant radius, where visibility remains 0
float calcHaloVisibility(float objectSpaceZ, float sphereStart, float sphereEnd, float pdf, float radius)
{
    return saturate((objectSpaceZ - (1.0 + gData.thickness) * radius) / HALO_RADIUS)
        * (sphereStart - sphereEnd) / pdf; // this adjust the visibility to the sampling (hemi-)sphere
}

float calcSphereVisibility(float objectSpaceZ, float sphereStart, float sphereEnd, float pdf)
{
    float sampleRange = max(sphereStart - max(sphereEnd, objectSpaceZ), 0.0);
    return sampleRange / pdf;
}

float calcVisibility(float objectSpaceZ, float sphereStart, float sphereEnd, float pdf, float radius)
{
    return calcSphereVisibility(objectSpaceZ, sphereStart, sphereEnd, pdf)
         + calcHaloVisibility(objectSpaceZ, sphereStart, sphereEnd, pdf, radius);
}

float2 calcGatherUV(float2 uv, Texture2D<float> tex)
{
    float2 resolution;
    tex.GetDimensions(resolution.x, resolution.y);

    return (floor(uv * resolution - 0.5) + 1.0) / resolution;
}

float4 getTexelWeights(float2 uv, Texture2D<float> tex)
{
    float2 resolution;
    tex.GetDimensions(resolution.x, resolution.y);

    // calculate bilinear interpolation weights from uv coordinate
    float2 f = frac(uv * resolution - 0.5);
    // (-,+),(+,+),(+,-),(-,-)
    float4 w = float4((1.0 - f.x) * f.y, f.x * f.y, f.x * (1.0 - f.y), (1.0 - f.x) * (1.0 - f.y));
    return w;
}

float4 getTexelPointWeight(float4 w)
{
    int maxIdx = 0;
    float maxVal = w[0];

    [unroll]
    for (int i = 1; i < 4; ++i)
    {
        if (w[i] > maxVal)
        {
            maxIdx = i;
            maxVal = w[i];
        }
    }
    float4 res = 0.0;

    [unroll]
    for (int i = 1; i < 4; ++i)
    {
        if (i == maxIdx)
        {
            res[i] = 1.0;
        }
    }

    return res;
}



float3 RayToViewSpace(RayDesc ray, float t)
{
    return mul(gCamera.data.viewMat, float4(ray.Origin + ray.Direction * t, 1.0f)).xyz;
}

// z: positive linear depth in view space
// r: radius in view/world space
float2 ViewSpaceRadiusToUVRadius(float z, float r)
{
    return float2(r * gCamera.data.focalLength) / (float2(gCamera.data.frameWidth, gCamera.data.frameHeight) * z); // radius in normalized device coordinates
}

float GetAORadiusInPixels(float ViewDepth)
{
    // convert radius to screen pixels
    float2 radiusUV = ViewSpaceRadiusToUVRadius(ViewDepth, gData.radius);
    // convert uv radius to pixel radius
    return lerp(radiusUV.x * gData.resolution.x, radiusUV.y * gData.resolution.y, 0.5); // take mean between width and height radii TODO  test
}

float GetMaxAORadius(float2 texC)
{
    return gData.ssMaxRadius;
}

struct BasicAOData
{
    float3 posV;
    float posVLength;
    float3 normal; // view space: -posV
    float3 tangent; // view space
    float3 bitangent; // view space
    float3 normalO; // sampling space (surface normal)
    float3 normalV; // view space (surface normal)

    float radiusInPixels;
    float radius; // world space radius. Usually gData.radius, but can be smaller if the screen space radius would be too large

    // returns false if sample needs no shading (background)
    [mutating] bool Init(float2 texC)
    {
        float linearDepth = gLinearDepthIn.SampleLevel(gTextureSampler, texC, 0);
        radiusInPixels = GetAORadiusInPixels(linearDepth);
        radius = gData.radius;

        // limit the pixel radius to maxPixelRadius to prevent samples from being distributed over the entire screen (bad for cache)
        float maxRadius = GetMaxAORadius(texC);
        if (radiusInPixels > maxRadius)
        {
            radius = radius / radiusInPixels * maxRadius;
            radiusInPixels = maxRadius;
        }

        if (radiusInPixels < 0.5)
        {
            return false;
        }

        posV = UVToViewSpace(texC, linearDepth);
        posVLength = length(posV);

        // view space normal of current pixel
        normalV = loadNormal(texC);
        if (dot(posV, normalV) > 0.0)
        {
            normalV = -normalV;
        }

        // Calculate tangent space (use random direction for tangent orientation)
        float randRotation = gNoiseTex.SampleLevel(gNoiseSampler, texC * gData.noiseScale, 0) * 2.0 * 3.141;
        float2 randDir = float2(sin(randRotation), cos(randRotation));

        // determine tangent space
        normal = -posV / posVLength;
        bitangent = normalize(cross(normal, float3(randDir, 0.0f)));
        tangent = cross(bitangent, normal);

        // transfer view space normal to normal in object coordinates of the sampling sphere
        normalO = float3(dot(normalV, tangent), dot(normalV, bitangent), dot(normalV, normal));


        return true;
    }

    static float finalize(float avgAO)
    {
        return pow(avgAO, gData.exponent);
    }
};


struct SampleAOData
{
    float sphereStart;
    float sphereEnd;
    float pdf; // 2 * sphereStart
    bool isInScreen;

    float2 samplePosUV; // possible out-of-screen sample location
    float2 rasterSamplePosUV; // raster clamped uv

    float visibility;
    float objectSpaceZ;

    float initialSamplePosLength;
    float radius; // world space radius of sample
    float3 initialSamplePosV;

    float screenSpaceRadius;

    uint numSamples;

    // returns false if the sample is invalid (below hemisphere)
    [mutating] bool Init(float2 texC, BasicAOData data, uint i, uint inNumSamples)
    {
        numSamples = inNumSamples;

        // random angle on view space disc
        float alpha = (float(i) / numSamples) * M_2PI;
        radius = sampleRadius[i] * data.radius; // radius on sampling unit sphere * world space radius
        float2 dir = radius * float2(sin(alpha), cos(alpha)); // world space direction

        const float sphereHeight = sqrt(data.radius * data.radius - radius * radius);
        pdf = 2.0 * sphereHeight;

        // determine distance within [-sphereHeight, +sphereHeight]
        sphereStart = sphereHeight; // in object coordinates (bigger is closer to the camera)
        sphereEnd = -sphereHeight; // in object coordinates (smaller is futher from the camera)

        { // HEMISPHERE SAMPLING
            float zIntersect = -dot(dir.xy, data.normalO.xy) / makeNonZero(data.normalO.z, 0.0001);
            float zIntersectClamped = clamp(zIntersect, -sphereHeight, sphereHeight);
            sphereEnd = zIntersectClamped;
        }

        if ((sphereStart - sphereEnd) / (2.0 * sphereHeight) <= 0.1)
        {
            return false; // skip sample (no visibility)
        }

        // sample position calculate uv position of sample
        initialSamplePosV = data.posV + data.tangent * dir.x + data.bitangent * dir.y;
        initialSamplePosLength = length(initialSamplePosV);
        samplePosUV = ViewSpaceToUV(initialSamplePosV);
        visibility = 0.0;
        objectSpaceZ = 0.0;
        screenSpaceRadius = length(float2((texC - samplePosUV) * gData.resolution));
        // screenSpaceRadius < 1 handled in first ao pass (isSamplePixel)

        float2 screenUv = getScreenClampedUV(texC, samplePosUV); // clip to screen border
        isInScreen = all(samplePosUV == screenUv);

        rasterSamplePosUV = screenUv;
        rasterSamplePosUV = getSnappedUV(rasterSamplePosUV); // snap to pixel center

        return true;
    }

    float3 calcSamplePosV(BasicAOData data, float2 uv, Texture2D<float> depthTex)
    {
        float linearSampleDepth = depthTex.SampleLevel(gTextureSampler, uv, 0.0);

        float3 samplePosV = UVToViewSpace(uv, linearSampleDepth);
        return samplePosV;
    }

    float calcObjectSpaceZ(BasicAOData data, float3 samplePosV)
    {
        float objectSpaceZ = dot(samplePosV - data.posV, data.normal);

        return objectSpaceZ;
    }

    bool requireRay(BasicAOData data)
    {
        return objectSpaceZ > sphereStart + CONST_RADIUS && screenSpaceRadius > gData.ssRadiusCutoff;
    }

    [mutating] void addSample(BasicAOData data, float3 samplePosV, bool init = false)
    {
        // update object space z
        float oz = calcObjectSpaceZ(data, samplePosV);
        if(init) objectSpaceZ = oz;
        else objectSpaceZ = min(objectSpaceZ, oz);

        // update visibility
        float v = calcVisibility(oz, sphereStart, sphereEnd, pdf, data.radius);
        if(init) visibility = v;
        else visibility = min(visibility, v);
    }

    [mutating] void resetSample()
    {
        visibility = 1.0;
        objectSpaceZ = 3.402823466e+38F;
    }

    [mutating] void evalPrimaryVisibility(BasicAOData data)
    {
        float3 samplePosV = calcSamplePosV(data, rasterSamplePosUV, gLinearDepthIn);
        addSample(data, samplePosV, true);
    }
};

struct RayData // cannot be compressed to half floats => no diff in rendering time + insufficient visual quality
{
    float tLastFrontFaceHalo; // ray min
    float tFirstFrontFaceInside; // ray max
    float tConstRadiusStart;
    float tSphereStart;
};

float calcAO2(uint2 svPos, uint mask, uint numSamples)
{
    float2 texC = (float2(svPos) + 0.5) * gData.invResolution;

    BasicAOData data;
    data.Init(texC);

    float visibility = 0.0;

    uint i = 0;

    [unroll]
    for (uint j = 0; j < numSamples; j++)
    {
        if (mask == 0u)
        {
            break; // no bits set anymore
        }

        // modify loop to only go through the set bits in mask
        [unroll]
        for (uint k = 0; k < numSamples && k < numSamples - j && (mask & 1u) == 0u; k++) // first condition is for unrolling, second is for better unrolling
        {
            // shift mask an increase i
            mask = mask >> 1;
            ++i;
        }

        SampleAOData s;
        s.Init(texC, data, i, numSamples);

        // subtract old visibility from raster (will be replaced with new visibility)
        s.evalPrimaryVisibility(data);

        visibility.x -= s.visibility; // subtract old visibility from raster for bright channel

        //int2 pixelCoord = UVToSDPixel(s.rasterSamplePosUV);
        int2 pixelCoord = UVToSDPixel(s.samplePosUV);

        //float2 sdSampleUV = getSnappedUV(s.rasterSamplePosUV, float2(sdwidth, sdheight));
        // calc uv space position based on primary frame buffer size
#if SD_JITTER
        float2 sdSampleUV = (pixelCoord - gData.sdGuard + randomJitter(pixelCoord)) / gData.lowResolution;
#else
        float2 sdSampleUV = (pixelCoord - gData.sdGuard + 0.5f) / gData.lowResolution;
#endif // else SD_JITTER
        const float depthRange = gCamera.data.farZ - gCamera.data.nearZ;
        const float depthOffset = gCamera.data.nearZ;
        bool stochInside = false;
        float4 sddepth = gLinearSDIn[pixelCoord];

        if (!s.isInScreen)
        {
            s.resetSample();
        }

        [unroll]
        for (uint i = 0; i < MSAA_SAMPLES; ++i)
        {
            float linearSampleDepth = sddepth[i];

            // linearSampleDepth is in [0, 1] => scale accordingly
            linearSampleDepth = linearSampleDepth * depthRange + depthOffset;
            float3 samplePosV = UVToViewSpace(sdSampleUV, linearSampleDepth);
            s.addSample(data, samplePosV);
        }

        visibility += s.visibility;

        // advance mask for next iteration
        mask = mask >> 1;
        ++i;
    }

    visibility *= 2.0 / float(numSamples);
    return visibility;
}

#define AO_HIT_IGNORE 0
#define AO_HIT_ACCEPT 1
#define AO_HIT_ACCEPT_AND_END 2

ExplicitRayConesLodTextureSampler computeLod(VertexData v, float3 rayDir, float t)
{
    RayCone rc = RayCone(0.0, RAY_CONE_SPREAD);
    rc = rc.propagateDistance(t);
    float lambda = rc.computeLOD(v.coneTexLODValue, rayDir, v.faceNormalW);
    return ExplicitRayConesLodTextureSampler(lambda);
}


// returns any of the above A0_HIT defines
uint aoAnyHit(inout RayData rayData, float t, const TriangleHit hit, bool frontFace, float3 rayDir)
{
    const uint materialID = gScene.getMaterialID(hit.instanceID);
    const MaterialHeader header = gScene.materials.materialData[materialID].header;

    bool isAlphaTested = header.getAlphaMode() == AlphaMode::Mask;

    frontFace = frontFace || header.isDoubleSided() || isAlphaTested;
    if (!frontFace)
    {
        return AO_HIT_IGNORE; // this is just for rasterizer compability
    }

    if (t <= rayData.tSphereStart)
    {
        rayData.tLastFrontFaceHalo = max(rayData.tLastFrontFaceHalo, t);
        if (t >= rayData.tConstRadiusStart)
        {
            return AO_HIT_ACCEPT_AND_END; // we can stop the query, because this will set the visibility to zero
        }
    }
    else // inside sphere
    {
        rayData.tFirstFrontFaceInside = min(rayData.tFirstFrontFaceInside, t);
        return AO_HIT_ACCEPT; // since we save the min, we can commit TMax here
    }

    return AO_HIT_IGNORE;

}
