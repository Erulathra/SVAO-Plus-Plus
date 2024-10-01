import Scene.Scene;

Texture2D gDiffuseOpacityTex;
Texture2D gNormalBufferTex;

SamplerState gFullScreenSampler;

float3 gAmbientLight;

float4 main(float2 UV: TEXCOORD) : SV_TARGET0
{
    const float3 diffuse = gDiffuseOpacityTex.Sample(gFullScreenSampler, UV).rgb;
    const float3 normalW = gNormalBufferTex.Sample(gFullScreenSampler, UV).xyz;

    const uint lightCount = gScene.getLightCount();

    float3 outColor = 0.f;

    for (uint lightID = 0; lightID < lightCount; ++lightID)
    {
        LightData light = gScene.getLight(lightID);
        if (light.type == LightType.Directional)
        {
            const float3 normalW = gNormalBufferTex.Sample(gFullScreenSampler, UV).xyz;
            const float lambert = saturate(dot(normalW, light.dirW));

            outColor += diffuse * lambert;
        }
    }

    outColor += gAmbientLight * diffuse;

    return float4(outColor, 1.f);
}

