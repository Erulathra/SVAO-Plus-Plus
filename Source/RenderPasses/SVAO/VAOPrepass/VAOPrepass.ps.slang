import Scene.RaytracingInline;
#include "../Common.slang"

RWTexture2D<unorm float> gAOMaskOut;


[numthreads(16, 16, 1)]
void main(uint3 id : SV_DispatchThreadID, uint3 group : SV_GroupID, uint3 localId : SV_GroupThreadID)
{
    float aoMask = 0.f;

    uint2 offset = (group.xy / 2u) * 32u + 2u * localId.xy + (group.xy % 2u); // 2x2 group alignment works better because of the per-pixel rotation. See SVAO::genNoiseTexture()
    uint2 svPos = offset + uint2(guardBand); // pixel position
    float2 texC = (svPos + float2(0.5)) * gData.aoInvResolution; // texture position

    BasicAOData data;
    if (!data.Init(texC))
    {
        aoMask = 1.0;
    }
    else
    {
        float visibility = 0.0;

        [unroll]
        for (uint i = 0; i < NUM_DIRECTIONS; i++)
        {
            SampleAOData s;
            if (!s.Init(texC, data, i, NUM_DIRECTIONS))
                continue;

            if (isSamePixel(texC, s.rasterSamplePosUV))
            {
                aoMask += (s.sphereStart - s.sphereEnd) / s.pdf;
                continue;
            }

            s.evalPrimaryVisibility(data);

            // Don't break SVAO by masking invalid samples 
            if (!s.requireRay(data))
            {
                aoMask += s.visibility;
            }
            else
            {
                aoMask = 0;
                break;
            }
        }

        aoMask *= 2.0 / float(NUM_DIRECTIONS);
        aoMask = data.finalize(aoMask);

        aoMask = step(gAOThreshold, aoMask);
    }

    gAOMaskOut[svPos] = aoMask;
}
