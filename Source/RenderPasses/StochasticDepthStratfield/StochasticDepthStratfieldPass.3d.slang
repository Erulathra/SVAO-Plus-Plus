/***************************************************************************
 # Copyright (c) 2015-23, NVIDIA CORPORATION. All rights reserved.
 #
 # Redistribution and use in source and binary forms, with or without
 # modification, are permitted provided that the following conditions
 # are met:
 #  * Redistributions of source code must retain the above copyright
 #    notice, this list of conditions and the following disclaimer.
 #  * Redistributions in binary form must reproduce the above copyright
 #    notice, this list of conditions and the following disclaimer in the
 #    documentation and/or other materials provided with the distribution.
 #  * Neither the name of NVIDIA CORPORATION nor the names of its
 #    contributors may be used to endorse or promote products derived
 #    from this software without specific prior written permission.
 #
 # THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS "AS IS" AND ANY
 # EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 # IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 # PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 # CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 # EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 # PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 # PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 # OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 # (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 # OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 **************************************************************************/
import Scene.Raster;
import Utils.Math.HashUtils;

#ifndef ALPHA
#define ALPHA 0.2
#endif // ALPHA

#ifndef NUM_SAMPLES
#define NUM_SAMPLES 4
#endif // ALPHA

Texture2D<float> gDepthTexture;
SamplerState gSampler;

uniform float2 gInvResolution;

StructuredBuffer<uint> gStratifiedIndices;
StructuredBuffer<uint> gStratifiedLookUp;

uniform float gNear;
uniform float gFar;

VSOut vsMain(VSIn vIn)
{
    return defaultVS(vIn);
}

struct PSOut
{
    uint SampleMask : SV_Coverage;
};

float linearizeDepth(float depth)
{
    return gNear * gFar / (gFar + depth * (gNear - gFar));
}

PSOut psMain(VSOut vsOut, uint triangleIndex: SV_PrimitiveID) : SV_TARGET
{
    PSOut output;

    uint2 svPos = uint2(vsOut.posH.xy);
    float2 screenSpaceUV = vsOut.posH.xy * gInvResolution;

    // discard fragments with the first pass depth
    float firstDepth = gDepthTexture.Sample(gSampler, screenSpaceUV);
    if (linearizeDepth(vsOut.posH.z) - 1e-3 <= linearizeDepth(firstDepth))
        discard;

    float randomFloat = hash3d(vsOut.posW.xyz);

    int R = int(floor(ALPHA * NUM_SAMPLES + randomFloat));

    if (R >= NUM_SAMPLES)
    {
        output.SampleMask = 0xffff;
    }
    else if (R != 0)
    {
        float secondRandomFloat = hash3d(vsOut.posW.zyx);
        int index = int(lerp(gStratifiedIndices[R], gStratifiedIndices[R + 1], secondRandomFloat));
        uint mask = gStratifiedLookUp[index];
        output.SampleMask = int(mask);
    }
    else
    {
        discard;
    }

    return output;
}
