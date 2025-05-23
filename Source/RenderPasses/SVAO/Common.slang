import Scene.Camera.Camera;
import VAOData;
import Scene.Intersection;
import Scene.Shading;
import Rendering.Materials.TexLODHelpers;
import Rendering.Materials.TexLODTypes;
import Utils.Math.PackedFormats;
import Utils.Math.HashUtils;

#include "Utils/Math/MathConstants.slangh"

#define ERULATHRA_MODIFICATIONS 1

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

#ifndef LOG2_NUM_DIRECTIONS
#define LOG2_NUM_DIRECTIONS 3
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

#ifndef USE_PREPASS
#define USE_PREPASS 1
#endif // ifndef USE_PREPASS

#ifndef USE_DITHER_TEX
#define USE_DITHER_TEX 1
#endif // ifndef USE_DITHER_TEX

cbuffer StaticCB
{
    VAOData gData;
#if USE_PREPASS
    float2 gPrepassResolution; 
    float2 gPrepassInvResolution;
#endif // USE_PREPASS
    float gAOThreshold;
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

float sampleDither(float2 UV)
{
    const float4 DitherValues[] = {
        float4(0.f,  8.f,  2.f,  10.f),
        float4(12.f, 4.f,  14.f, 6.f ),
        float4(3.f,  11.f, 1.f,  9.f ),
        float4(15.f, 7.f,  13.f, 5.f)
    };

    const uint2 Index = fract(UV) * 4.f;
    return DitherValues[Index.y][Index.x] * (1.f / 16.f);
}

uint GetNumSamples(float linearDepth)
{
#if ADAPTIVE_SAMPLING
    for (uint i = 0; i < LOG2_NUM_DIRECTIONS; i++)
    {
        if (linearDepth < gData.adaptiveSamplingDistances[i])
        {
            return 1 << (LOG2_NUM_DIRECTIONS - i);
        }
    }
#endif

    return NUM_DIRECTIONS;
}

float3 loadNormal(float2 texC)
{
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
    return all(abs(uv1 - uv2) < gData.aoInvResolution * 0.9);
}

// uv: uv coordinates [0, 1]
// viewDepth: linear depth in view space (positive z)
// return: view space position (negative z)
float3 UVToViewSpace(float2 uv, float viewDepth)
{
    const float2 ndc = float2(uv.x, 1.f - uv.y) * 2.f - 1.0f; // normalized device coordinates [-1, 1]
    return float3(ndc * viewDepth * gData.cameraImageScale, -viewDepth);
}

// posV: view space position (negative z)
// return: texture uv [0, 1]
float2 ViewSpaceToUV(float3 posV)
{
    float2 ndc = posV.xy / (gData.cameraImageScale * posV.z);
    return ndc * float2(-0.5, 0.5) + 0.5; // since posV.z is negative, the sign order is inversed
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
    return lerp(radiusUV.x * gData.aoResolution.x, radiusUV.y * gData.aoResolution.y, 0.5); // take mean between width and height radii TODO  test
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
    [mutating] bool Init(const float2 texC)
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
    #if USE_DITHER_TEX
        const float randRotation = gNoiseTex.SampleLevel(gNoiseSampler, texC * gData.noiseScale, 0) * M_2PI;
    #else
        const float randRotation = sampleDither(texC * gData.noiseScale) * M_2PI;
    #endif // USE_DITHER_TEX
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
        screenSpaceRadius = length(float2((texC - samplePosUV) * gData.aoResolution));
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

float calcAO2(uint2 svPos, uint mask)
{
    float2 texC = (float2(svPos) + 0.5) * gData.aoInvResolution;

    BasicAOData data;
    data.Init(texC);

    const uint numSamples = GetNumSamples(-data.posV.z);

    float visibility = 0.0;

    uint i = 0;

#if !ADAPTIVE_SAMPLING
    [unroll]
#endif
    for (uint j = 0; j < numSamples; j++)
    {
        if (mask == 0u)
        {
            break; // no bits set anymore
        }

        // modify loop to only go through the set bits in mask
#if !ADAPTIVE_SAMPLING
        [unroll]
#endif
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

        // float2 sdSampleUV = getSnappedUV(s.rasterSamplePosUV, float2(sdwidth, sdheight));
        //  calc uv space position based on primary frame buffer size
#if SD_JITTER
        const float2 jitter = float2(gScene.camera.data.jitterX, gScene.camera.data.jitterY);
        float2 sdSampleUV = (pixelCoord - gData.sdGuard + jitter) / gData.lowResolution;
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