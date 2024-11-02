Texture2D<float> gDepth;
Texture2DMSArray<float> gStochasticDepth;
Texture2D<float3> gNormalBuffer;

SamplerState gPointSampler;

uniform float4x4 gInvProjMat;
uniform float4x4 gProjMat;
uniform float4x4 gViewMat;

uniform uint2 gDepthResolution;

uniform float gRadius;
uniform float gBias;

uniform int gFrameNum;


#ifndef SSAO_SAMPLES
#define SSAO_SAMPLES 8
#endif // SSAO_SAMPLES

#ifndef ENABLE_SDF
#define ENABLE_SDF 0
#endif // SSAO_SAMPLES

import Utils.Math.HashUtils;
import Scene.Camera.Camera;

float3 reconstructPosition(float2 uv, float depth)
{
    float2 normalizedScreenPos = uv * 2.f - 1.f;
    float4 ndc = float4(normalizedScreenPos.x, -normalizedScreenPos.y, depth, 1.f);
    float4 position = mul(ndc, gInvProjMat);
    position /= position.w;

    return position.xyz;
}

float2 viewSpaceToScreenUV(float3 position)
{
    float4 positionProj = mul(float4(position, 1.f), gProjMat);
    positionProj /= positionProj.w;

    float2 uv = positionProj.xy * 0.5f + 0.5f;
    uv.y *= -1.f;

    return uv;
}

float isValidSample(float3 viewPosition, float3 viewNormal, float3 sampleViewPosition, float3 bufferViewPosition)
{
    float3 sampleDir = sampleViewPosition - viewPosition;

    float inHemisphereDir = step(0, dot(viewNormal, normalize(sampleDir)));
    float inHemisphereDist = step(distance(viewPosition, bufferViewPosition), gRadius + gBias);

    // return inHemisphereDist;
    // return inHemisphereDist * inHemisphereDir;
    return 1.f;
}

float computeOcclusion(float3 viewPosition, float3 viewNormal, float3 sampleViewPosition, float3 bufferViewPosition)
{
    float bIsOccluded = step(sampleViewPosition.z, bufferViewPosition.z);

    return bIsOccluded * isValidSample(viewPosition, viewNormal, sampleViewPosition, bufferViewPosition);
}

float main(float2 uv: TEXCOORD) : SV_TARGET0
{
    const float rawDepth = gDepth.Sample(gPointSampler, uv);
    const float3 positionView = reconstructPosition(uv, rawDepth);

    float3 normalWorld = gNormalBuffer.Sample(gPointSampler, uv).xyz;
    float3 normalView = mul(float3x3(gViewMat), normalWorld);

    float occlusion = 0.f;
    for (int i = 0; i < SSAO_SAMPLES; ++i)
    {
        const float randomNumber = hash3d(float3(uv * gFrameNum, i));
        const float3 randomDir = randomPointInHemisphere(normalView, gRadius, randomNumber);
        // const float3 randomDir = randomPointInSphere(randomNumber, gRadius);

        const float3 samplePositionView = positionView + randomDir;
        const float2 sampleUV = viewSpaceToScreenUV(samplePositionView);

        const float bufferSampleDepth = gDepth.Sample(gPointSampler, sampleUV);
        const float3 bufferViewPosition = reconstructPosition(sampleUV, bufferSampleDepth);

        occlusion += computeOcclusion(positionView, normalView, samplePositionView, bufferViewPosition);
    }

// #if ENABLE_SDF
//     return rawDepth;
// #else
//     return -positionView.z;
// #endif
    return 1.f - (occlusion / SSAO_SAMPLES);
}

