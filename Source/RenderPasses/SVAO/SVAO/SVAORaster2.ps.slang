import Scene.RaytracingInline;
#include "../Common.slang"

// inputs from previous stage
Texture2D<uint> gAOMaskIn;
RWTexture2D<unorm float> gAOInOut;

[numthreads(16, 16, 1)]
void main(uint3 id : SV_DispatchThreadID, uint3 group : SV_GroupID, uint3 localId : SV_GroupThreadID)
{
    uint2 offset = (group.xy / 2u) * 32u + 2u * localId.xy + (group.xy % 2u);
    //uint2 svPos = offset + uint2(guardBand); // pixel position
    uint2 svPos = id.xy + uint2(guardBand); // pixel position
    float2 texC = (svPos + float2(0.5)) * gData.invResolution; // texture position

    uint mask = gAOMaskIn[uint2(svPos.xy)];
    if(mask == 0)
        return;

    float visibility = calcAO2(svPos, mask);
    visibility += float(gAOInOut[svPos]);
    visibility = BasicAOData.finalize(visibility);
    gAOInOut[svPos] = visibility;
}
