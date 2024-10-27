Texture2D<float> gDepth;
Texture2DMSArray<float> gStochasticDepth;
Texture2D<float3> gNormalBuffer;

SamplerState gPointSampler;

uniform float4x4 gInvProjMat;
uniform float4x4 gProjMat;
uniform float4x4 gViewMat;

uniform float gRadius;
uniform float gBias;

uniform int gFrameNum;

#ifndef SSAO_SAMPLES
#define SSAO_SAMPLES 8
#endif // SSAO_SAMPLES

import Utils.Math.HashUtils;

float3 reconstructPosition(float2 uv, float depth)
{
    float2 normalizedScreenPos = uv * 2.f - 1.f;
    float4 ndc = float4(normalizedScreenPos.x, -normalizedScreenPos.y, depth, 1.f);
    float4 position = mul(ndc, gInvProjMat);
    position /= position.w;

    return position.xyz;
}

float main(float2 uv: TEXCOORD) : SV_TARGET0
{
    float3 normalWorld = gNormalBuffer.Sample(gPointSampler, uv).xyz;
    float3 normalView = mul(float3x3(gViewMat), normalWorld);

    const float rawDepth = gDepth.Sample(gPointSampler, uv);
    const float3 positionView = reconstructPosition(uv, rawDepth);

    float occlusion = 0.f;
    for (int i = 0; i < SSAO_SAMPLES; ++i)
    {
        const float randomNumber = hash3d(float3(uv * gFrameNum, i));
        const float3 randomDir = randomDirectionInHemisphere(normalView, randomNumber);
        // const float3 randomDir = randomDirection(randomNumber);

        const float3 samplePosition = positionView + randomDir * gRadius;
        float4 samplePositionProj = mul(float4(samplePosition, 1.f), gProjMat);
        samplePositionProj /= samplePositionProj.w;

        float2 sampleUV = samplePositionProj.xy * 0.5f + 0.5f;
        sampleUV.y *= -1.f;
        float sampleRawDepth = gDepth.Sample(gPointSampler, sampleUV);

        // occlusion += step(samplePositionProj.z + gBias, sampleRawDepth);
        occlusion += step(samplePositionProj.z, sampleRawDepth);
    }

    return occlusion / SSAO_SAMPLES + gBias;
}

