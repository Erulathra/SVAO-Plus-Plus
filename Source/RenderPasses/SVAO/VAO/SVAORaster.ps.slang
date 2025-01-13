import Scene.RaytracingInline;
#include "../Common.slang"

#if SECONDARY_DEPTH_MODE == DEPTH_MODE_SINGLE
// disable all stencil operations if no secondary pass exists
#define STENCIL(x)
#else
#define STENCIL(x) x
#endif

struct PSOut
{
    float ao1;
    STENCIL(uint stencil);
};

RWTexture2D<uint> gRayMinOut;
RWTexture2D<uint> gRayMaxOut;

RWTexture2D<unorm float> gAOOut;
RWTexture2D<uint> gStencil;

void writeOutput(uint2 pixel, PSOut output)
{
    gAOOut[pixel] = output.ao1;
    STENCIL(gStencil[pixel] = output.stencil);
}

[numthreads(16, 16, 1)]
void main(uint3 id : SV_DispatchThreadID, uint3 group : SV_GroupID, uint3 localId : SV_GroupThreadID)
{
    PSOut output;
    output.ao1 = 0.0;
    STENCIL(output.stencil = 0);

    uint2 offset = (group.xy / 2u) * 32u + 2u * localId.xy + (group.xy % 2u); // 2x2 group alignment works better because of the per-pixel rotation. See SVAO::genNoiseTexture()
    uint2 svPos = offset + uint2(guardBand); // pixel position
    float2 texC = (svPos + float2(0.5)) * gData.invResolution; // texture position

    BasicAOData data;
    if (!data.Init(texC))
    {
        output.ao1 = 1.0;
    }
    else
    {
        float visibility = 0.0;

        const uint numSamples = GetNumSamples(-data.posV.z);

#if !ADAPTIVE_SAMPLING
        [unroll]
#endif
        for (uint i = 0; i < numSamples; i++)
        {
            SampleAOData s;
            if (!s.Init(texC, data, i, numSamples))
                continue;

            if (isSamePixel(texC, s.rasterSamplePosUV))
            {
                output.ao1 += (s.sphereStart - s.sphereEnd) / s.pdf;
                continue;
            }

            bool forceRay = false;

            s.evalPrimaryVisibility(data);

            // always add computed visibility from raster
            output.ao1.x += s.visibility; // only on bright

            if (!s.isInScreen && gData.sdGuard > 0)
            {
                forceRay = true; // always shoot rays for screen border
                s.objectSpaceZ = 3.402823466e+38F; // reset object space for new ray interval calculation
            }

            if (s.requireRay(data) || forceRay)
            {
                STENCIL(output.stencil |= 1u << i);

#if (SECONDARY_DEPTH_MODE == DEPTH_MODE_STOCHASTIC)
                uint2 pixel = UVToSDPixel(s.samplePosUV);
#if USE_RAY_INTERVAL
                uint rayMin, rayMax;
                float objectSpaceMin = min(s.objectSpaceZ, data.radius + gData.thickness * data.radius + s.sphereStart);
                rayMin = /*rough estimate of depth*/asuint(max(data.posVLength - objectSpaceMin, 0.0));
                rayMax = /*rough estimate of depth*/asuint(max(data.posVLength - s.sphereEnd, 0.0));
                InterlockedMin(gRayMinOut[pixel], rayMin);
                InterlockedMax(gRayMaxOut[pixel], rayMax);
#else   // no interval (only set flag)
                gRayMaxOut[pixel] = 1u;
#endif  // else USE_RAY_INTERVAL
#endif  // (SECONDARY_DEPTH_MODE == DEPTH_MODE_STOCHASTIC)
            }
        }

        output.ao1 *= 2.0 / float(numSamples);

#if SECONDARY_DEPTH_MODE == DEPTH_MODE_SINGLE
        output.ao1 = data.finalize(output.ao1); // there is no second pass
#else
        // apply exponent
        if (output.stencil == 0)
        {
            output.ao1 = data.finalize(output.ao1);
        }
#endif
    }

    writeOutput(svPos, output);
}
