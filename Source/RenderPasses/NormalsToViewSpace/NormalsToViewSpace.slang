import Utils.Math.PackedFormats;

Texture2D<float3> gNormalsWorldIn;

cbuffer PerFrameCB
{
    float4x4 gViewMat;
}

uint main(float2 uv : TEXCOORD, float4 svPos : SV_POSITION) : SV_TARGET
{
    float3 normal = gNormalsWorldIn[uint2(svPos.xy)].xyz;
    normal = mul(float3x3(gViewMat), normal);

    return encodeNormal2x8(normal);
}
