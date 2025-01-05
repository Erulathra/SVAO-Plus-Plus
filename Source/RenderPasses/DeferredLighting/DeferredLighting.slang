import Scene.Raster;
import Utils.Sampling.TinyUniformSampleGenerator;
import Rendering.Lights.LightHelpers;
import Rendering.Materials.BSDFs.DiffuseSpecularBRDF;

#include "Utils/Math/MathConstants.slangh"

// Implements GGX based on:
// https://sibras.github.io/OpenGL4-Tutorials/docs/Tutorials/03-Tutorial3/#part-4-ggx-shading-function

Texture2D<float4> gPosW;
Texture2D<float4> gNormW;
Texture2D<float4> gDiffOpacity;
Texture2D<float4> gSpecRough;

struct GBufferData
{
    float3 posW = float3(0.f);
    float3 normW = float3(0.f);
    float3 diff = float3(0.f);
    float opacity = 0.f;
    float3 specular = float3(0.f);
    float roughness = 0.f;
}

cbuffer PerFrameCB
{
    float ambientLight;

    float linearFalloff = 0.045;
    float quadraticFalloff = 0.0075;
}

float3 calculateLightFalloff(LightData light, float3 posW)
{
    const float distance = distance(light.posW, posW);
    return light.intensity / (1.f + linearFalloff * distance + quadraticFalloff * distance * distance);
}

float3 schlickFresnel(float3 lightDir, float3 normW, float3 specular)
{
    const float LH = dot(lightDir, normW);
    return specular + (1.f - specular) * pow(1.f - LH, 5);
}

float TRDistribution(float3 normW, float3 halfVectorW, float roughness)
{
    const float NSq = roughness * roughness;
    const float NH = max(dot(normW, halfVectorW), 0.f);
    const float denom = NH * NH * (NSq - 1.f) + 1.f;

    return NSq / (M_PI * denom * denom);
}

float GGXVisibility(float3 normW, float3 lightDir, float3 viewDir, float roughness)
{
    const float NL = max(dot(normW, lightDir), 0.f);
    const float NV = max(dot(normW, viewDir), 0.f);
    const float RSq = roughness * roughness;
    const float RMod = 1.f - RSq;
    const float RecipG1 = NL + sqrt(RSq + (RMod * NL * NL));
    const float RecipG2 = NV + sqrt(RSq + (RMod * NV * NV));

    return 1.f / (RecipG1 * RecipG2);
}

float3 GGX(LightData light, float3 viewDir, GBufferData gBuffer)
{
    const float3 lightDir = normalize(light.posW - gBuffer.posW);

    const float3 diffuse = gBuffer.diff * M_1_PI;
    const float3 halfVector = normalize(viewDir + lightDir);

    // Toorace-Sparrow components
    const float3 fresnel = schlickFresnel(lightDir, halfVector, gBuffer.specular);
    const float distribution = TRDistribution(gBuffer.normW, halfVector, gBuffer.roughness);
    const float visibility = GGXVisibility(gBuffer.normW, lightDir, viewDir, gBuffer.roughness);

    // diffuse + specular
    float3 result = diffuse + (fresnel * distribution * visibility);

    // mul by view angle
    result *= max(dot(gBuffer.normW, lightDir), 0.f);

    // combine with irradiance
    result *= calculateLightFalloff(light, gBuffer.posW);

    return result;
}

float3 blinPhong(LightData light, float3 viewDir, GBufferData gBuffer)
{
    return (gBuffer.diff + gBuffer.specular) * calculateLightFalloff(light, gBuffer.posW);
}

float4 main(float2 uv : TEXCOORD, float4 svPos : SV_POSITION) : SV_TARGET
{
    const float4 diffOpacity = gDiffOpacity.Load(int3(svPos.xy, 0));
    const float4 specRough = gSpecRough.Load(int3(svPos.xy, 0));

    GBufferData gBuffer;
    gBuffer.posW = gPosW.Load(int3(svPos.xy, 0)).xyz;
    gBuffer.normW = gNormW.Load(int3(svPos.xy, 0)).xyz;
    gBuffer.diff = diffOpacity.rgb;
    gBuffer.opacity = diffOpacity.a;
    gBuffer.specular = specRough.rgb;
    gBuffer.roughness = specRough.a;

    const float3 viewDir = normalize(gScene.camera.getPosition() - gBuffer.posW);

    float3 color = (gBuffer.diff + gBuffer.specular) * ambientLight;

    for (int i = 0; i < gScene.getLightCount(); i++)
    {
        const LightData light = gScene.getLight(i);

        if (light.type != LightType.Point)
            continue;

        color += GGX(light, viewDir, gBuffer);
    }

    return float4(color, 1.f);
}
