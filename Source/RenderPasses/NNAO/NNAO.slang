import Utils.Math.HashUtils;

#ifndef NNAO_SAMPLES
#define NNAO_SAMPLES 64
#endif // SSAO_SAMPLES

#ifndef ENABLE_SDF
#define ENABLE_SDF 0
#endif // ENABLE_SDF

#ifndef MSAA_SAMPLES
#define MSAA_SAMPLES 4
#endif // MSAA_SAMPLES

#define PI 3.141592653589

#define FW 31
#define HW ((FW - 1) / 2)

Texture2D<float> gDepth;
Texture2DMSArray<float> gStochasticDepth;
Texture2D<float3> gNormalBuffer;

SamplerState gPointSampler;

Texture2D<float4> gWeightF0;
Texture2D<float4> gWeightF1;
Texture2D<float4> gWeightF2;
Texture2D<float4> gWeightF3;
SamplerState gWeightSampler;

uniform float4x4 gInvProjMat;
uniform float4x4 gProjMat;
uniform float4x4 gViewMat;

uniform uint2 gDepthResolution;

uniform float gRadius;
uniform float gBias;

uniform int gFrameNum;

const static float4 F0a = float4(2.364370, 2.399485, 0.889055, 4.055205);
const static float4 F0b = float4(-1.296360, -0.926747, -0.441784, -3.308158);
const static float4 F1a = float4(1.418117, 1.505182, 1.105307, 1.728971);
const static float4 F1b = float4(-0.491502, -0.789398, -0.328302, -1.141073);
const static float4 F2a = float4(1.181042, 1.292263, 2.136337, 1.616358);
const static float4 F2b = float4(-0.535625, -0.900996, -0.405372, -1.030838);
const static float4 F3a = float4(1.317336, 2.012828, 1.945621, 5.841383);
const static float4 F3b = float4(-0.530946, -1.091267, -1.413035, -3.908190);

const static float4 Xmean = float4(0.000052, -0.000003, -0.000076, 0.004600);
const static float4 Xstd = float4(0.047157, 0.052956, 0.030938, 0.056321);
const static float Ymean = 0.000000;
const static float Ystd = 0.116180;

const static float4x4 W1 = float4x4(
    -0.147624, -0.150471, 0.154306, -0.006904,
    0.303306, 0.057305, -0.240071, 0.036727,
    0.009158, -0.371759, -0.259837, 0.302215,
    -0.111847, -0.183312, 0.044680, -0.190296
);

const static float4x4 W2 = float4x4(
    0.212815, 0.028991, 0.105671, -0.111834,
    0.316173, -0.166099, 0.058121, -0.170316,
    0.135707, -0.478362, -0.156021, -0.413203,
    -0.097283, 0.189983, 0.019879, -0.260882
);

const static float4 W3 = float4(0.774455, 0.778138, -0.318566, -0.523377);

const static float4 b0 = float4(0.428451, 2.619065, 3.756697, 1.636395);
const static float4 b1 = float4(0.566310, 1.877808, 1.316716, 1.091115);
const static float4 b2 = float4(0.033848, 0.036487, -1.316707, -1.067260);
const static float b3 = 0.151472;

const static float4 alpha0 = float4(0.326746, -0.380245, 0.179183, 0.104307);
const static float4 alpha1 = float4(0.255981, 0.009228, 0.211068, 0.110055);
const static float4 alpha2 = float4(-0.252365, 0.016463, -0.232611, 0.069798);
const static float alpha3 = -0.553760;

const static float4 beta0 = float4(0.482399, 0.562806, 0.947146, 0.460560);
const static float4 beta1 = float4(0.670060, 1.090481, 0.461880, 0.322837);
const static float4 beta2 = float4(0.760696, 1.016398, 1.686991, 1.744554);
const static float beta3 = 0.777760;

float3 UVToViewSpace(float2 uv, float depth)
{
    depth = max(depth, 1e-10);
    float2 normalizedScreenPos = uv * 2.f - 1.f;
    float4 ndc = float4(normalizedScreenPos.x, -normalizedScreenPos.y, depth, 1.f);
    float4 position = mul(gInvProjMat, ndc);
    position /= position.w;

    return position.xyz;
}

float2 viewSpaceToUV(float3 position)
{
    float4 positionProj = mul(gProjMat, float4(position, 1.f));
    positionProj /= positionProj.w;

    float2 uv = positionProj.xy * 0.5f + 0.5f;
    uv.y = (1.f - uv.y);

    return uv;
}

float3 normalWorldToView(float3 normalWorld)
{
    return normalize(mul(float3x3(gViewMat), normalWorld));
}

float3 sampleNormalView(float2 uv, float3 viewPos)
{
    float3 normalWorld = gNormalBuffer.Sample(gPointSampler, uv).xyz;
    float3 normalView = mul(float3x3(gViewMat), normalWorld).xyz;
    normalView *= -sign(dot(viewPos, normalView));

    return normalView * 0.5 + 0.5;
}

float4 prelu(float4 x, float4 alpha, float4 beta)
{
    return beta * max(x, 0.0) + alpha * min(x, 0.0);
}

float preluFloat(float x, float alpha, float beta)
{
    return beta * max(x, 0.f) + alpha * min(x, 0.f);
}

float2 spiralSample(float t, float l, float o)
{
    return t * float2(cos(l * 2.f * PI * (t + o)), sin(l * 2.f * PI * (t + o)));
}

float3 main(float2 uv: TEXCOORD) : SV_TARGET0
{
    float depth = gDepth.Sample(gPointSampler, uv);
    float3 base = UVToViewSpace(uv, depth);
    float3 normalView = sampleNormalView(uv, base);
    float seed = hash3d(base);

    /* First Layer */
    float4 H0 = float4(0.f);

    for (int i = 0; i < NNAO_SAMPLES; i++)
    {
        float scale = (PI / 4.f) * (FW * FW) * ((float(i + 1) / float(NNAO_SAMPLES + 1)) / float(NNAO_SAMPLES / 2));
        float2 indx = spiralSample(float(i + 1) / float(NNAO_SAMPLES + 1), 2.5f, 2.f * PI * seed);

        float2 next = viewSpaceToUV(base + gRadius * float3(indx, 0.f));

        float depth = gDepth.Sample(gPointSampler, next);
        float3 actu = UVToViewSpace(next, depth);
        float2 fltr = (indx * float(HW) + float(HW) + 0.5f) / (float(HW) * 2.f + 2.f);
        float3 norm = sampleNormalView(next, actu);

        float4 X = (1.f - clamp(distance(actu, base) / gRadius, 0.f, 1.f)) * float4(norm - normalView, (actu.z - base.z) / gRadius);

        H0 += scale *
              mul(
                ((X - Xmean) / Xstd),
                transpose(float4x4(
                  gWeightF0.Sample(gWeightSampler, fltr) * F0a + F0b,
                  gWeightF1.Sample(gWeightSampler, fltr) * F1a + F1b,
                  gWeightF2.Sample(gWeightSampler, fltr) * F2a + F2b,
                  gWeightF3.Sample(gWeightSampler, fltr) * F3a + F3b))
                );

    }

    // return H0.xyz;
    // return normalView;

    H0 = prelu(H0 + b0, alpha0, beta0);

    /* Other Layers */

    float4 H1 = prelu(mul(H0, W1) + b1, alpha1, beta1);
    float4 H2 = prelu(mul(H1, W2) + b2, alpha2, beta2);
    float Y = preluFloat(dot(W3, H2) + b3, alpha3, beta3);

    /* Output */

    return 1.f - clamp(Y * Ystd + Ymean, 0.f, 1.f);
}
