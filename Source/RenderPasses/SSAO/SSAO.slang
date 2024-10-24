Texture2D<float> gDepth;
Texture2DMSArray<float> gStochasticDepth;
Texture2D<float3> gNormalBuffer;

SamplerState gPointSampler;

uniform float4x4 gInvProjMat;
uniform float4x4 gProjMat;
uniform float4x4 gViewMat;

uniform float gRadius;
uniform float gBias;

#ifndef SSAO_SAMPLES
#define SSAO_SAMPLES 8
#endif // SSAO_SAMPLES

import Utils.Math.HashUtils;

float3 reconstructPosition(float2 uv, float depth)
{
    float4 ndc = float4(uv.x * 2.f - 1.f, -(uv.y * 2.f - 1.f), depth, 1.f);
    float4 position = mul(gInvProjMat, ndc);

    return position.xyz / position.w;
}

float main(float2 uv: TEXCOORD) : SV_TARGET0
{
    // float3 normalWorld = gNormalBuffer.Sample(gPointSampler, uv).xyz;
    // float3 normalView = mul( normalWorld, float3x3(gViewMat));

    const float rawDepth = gDepth.Sample(gPointSampler, uv);
    const float3 positionView = reconstructPosition(uv, rawDepth);

    float occlusion = 0.f;
    for (int i = 0; i < SSAO_SAMPLES; ++i)
    {
        const float randomNumber = hash4d(float4(uv, rawDepth, i));
        // float3 randomDir = randomDirectionInHemisphere(normalView, randomNumber);
        // const float3 randomDir = randomDirection(randomNumber);
        const float3 randomDir = float3(0.f, 0.f, -1.f);

        const float3 targetPosition = positionView + randomDir * gRadius;
        const float4 targetPositionProj = mul(float4(targetPosition, 1.f), gProjMat);

        float2 targetUV = (targetPositionProj.xy / targetPositionProj.w) * 0.5f + 0.5f;
        targetUV.y *= -1.f;
        float targetDepth = gDepth.Sample(gPointSampler, targetUV);

        occlusion += step(rawDepth, targetDepth);
    }

    return occlusion / SSAO_SAMPLES;
}

