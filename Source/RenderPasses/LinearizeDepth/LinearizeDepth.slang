Texture2D<float> gInDepthTex;
SamplerState gFullscreenSampler;

uniform float gNearPlane;
uniform float gFarPlane;

float4 main(float2 UV: TEXCOORD) : SV_TARGET0
{
    float rawDepth = gInDepthTex.SampleLevel(gFullscreenSampler, UV, 0);
    return gNearPlane * gFarPlane / (gFarPlane + rawDepth * (gNearPlane - gFarPlane));
}

