#include "VAOBase.h"

#include "SVAO.h"
#include "VAO.h"
#include "Utils/Math/SDMath.h"

namespace
{
    namespace VAOArgs
    {
        const std::string kRadius = "kVaoRadius";
        const std::string kExponent = "kVaoExponent";
        const std::string kSampleCount = "kSampleCount";

        const std::string kResolutionDivisor = "resolutionDivisor";
        const std::string kEnableSDGuardBand = "enableGuardBand";
    }
}


extern "C" FALCOR_API_EXPORT void registerPlugin(Falcor::PluginRegistry& registry)
{
    registry.registerClass<RenderPass, VAO>();
    registry.registerClass<RenderPass, SVAO>();
}

VAOBase::VAOBase(ref<Device> pDevice, const Properties& props) : RenderPass(pDevice)
{
    Sampler::Desc samplerDesc;
    samplerDesc.setFilterMode(TextureFilteringMode::Point, TextureFilteringMode::Point, TextureFilteringMode::Point);
    samplerDesc.setAddressingMode(TextureAddressingMode::Wrap, TextureAddressingMode::Wrap, TextureAddressingMode::Wrap);
    mpPointSampler = pDevice->createSampler(samplerDesc);

    samplerDesc.setFilterMode(TextureFilteringMode::Linear, TextureFilteringMode::Linear, TextureFilteringMode::Linear);
    samplerDesc.setAddressingMode(TextureAddressingMode::Clamp, TextureAddressingMode::Clamp, TextureAddressingMode::Clamp);
    mpLinearSampler = pDevice->createSampler(samplerDesc);

    mpDitherTexture = generateDitherTexture(pDevice);

    for (const auto& [key, value] : props)
    {
        if (key == VAOArgs::kRadius) mVaoData.radius = value;
        else if (key == VAOArgs::kExponent) mVaoData.exponent = value;
        else if (key == VAOArgs::kSampleCount) mSampleCount = value;
        else if (key == VAOArgs::kResolutionDivisor) mSDMapResolutionDivisor = value;
        else if (key == VAOArgs::kEnableSDGuardBand) mEnableGuardBand = value;
    }
}

DefineList VAOBase::GetCommonDefines(const CompileData& compileData)
{
    DefineList defines;

    defines.add(mpScene->getSceneDefines());
    auto rayConeSpread = mpScene->getCamera()->computeScreenSpacePixelSpreadAngle(compileData.defaultTexDims.y);
    defines.add("RAY_CONE_SPREAD", std::to_string(rayConeSpread));
    defines.add("NUM_DIRECTIONS", std::to_string(mSampleCount));
    defines.add("ADAPTIVE_SAMPLING", mEnableAdaptiveSampling ? "1" : "0");

    return defines;
}

void VAOBase::SetCommonVars(ShaderVar& vars, Scene* pScene)
{
    vars["StaticCB"].setBlob(mVaoData);
    vars["StaticCB"]["gData"]["adaptiveSamplingDistances"] = mVaoData.adaptiveSamplingDistances;
    vars["gNoiseSampler"] = mpPointSampler;
    vars["gTextureSampler"] = mpLinearSampler;
    vars["gNoiseTex"] = mpDitherTexture;

    Camera* pCamera = pScene->getCamera().get();
    pCamera->bindShaderData(vars["PerFrameCB"]["gCamera"]);
    vars["PerFrameCB"]["invViewMat"] = inverse(pCamera->getViewMatrix());
    vars["PerFrameCB"]["frameIndex"] = mFrameIndex;
    vars["PerFrameCB"]["guardBand"] = getExtraGuardBand();
}
void VAOBase::compile(RenderContext* pRenderContext, const CompileData& compileData)
{
    RenderPass::compile(pRenderContext, compileData);

    mVaoData.resolution = float2(compileData.defaultTexDims.x, compileData.defaultTexDims.y);
    mVaoData.invResolution = float2(1.0f) / mVaoData.resolution;
    mVaoData.sdGuard = mEnableGuardBand ? getExtraGuardBand() : 0;
    mVaoData.lowResolution = getStochMapSize(compileData.defaultTexDims);
    mVaoData.noiseScale = mVaoData.resolution / 4.0f; // noise texture is 4x4 resolution
}

void VAOBase::execute(RenderContext* pRenderContext, const RenderData& renderData)
{
    if (!mpScene)
    {
        return;
    }

    ++mFrameIndex;
}
void VAOBase::renderUI(Gui::Widgets& widget)
{
    const static Gui::DropdownList kResolutionDivisorDropdownList = {
        {1, "1"},
        {2, "2"},
        {3, "3"},
        {4, "4"}
    };

    bool requiresRecompile = false;

    // VAO
    if (Gui::Group group = widget.group("VAO", true))
    {
        group.var("Radius", mVaoData.radius, 0.f);
        group.var("Exponent", mVaoData.exponent, 1.f);
        group.var("Adaptive sampling distances", mVaoData.adaptiveSamplingDistances);

        const static Gui::DropdownList kVaoSampleCount = {{8, "8"}, {16, "16"}, {32, "32"}};

        requiresRecompile |= group.dropdown("NumSamples", kVaoSampleCount, mSampleCount);
    }

    requiresRecompile |= widget.dropdown("SDMap resolution divisor", kResolutionDivisorDropdownList, mSDMapResolutionDivisor);
    requiresRecompile |= widget.checkbox("Enable Guard Band", mEnableGuardBand);
    requiresRecompile |= widget.checkbox("Enable Adaptive Sampling", mEnableAdaptiveSampling);

    if (requiresRecompile)
    {
        requestRecompile();
    }
}

Properties VAOBase::getProperties() const
{
    Properties properties = RenderPass::getProperties();

    properties[VAOArgs::kRadius] = mVaoData.radius;
    properties[VAOArgs::kExponent] = mVaoData.exponent;
    properties[VAOArgs::kSampleCount] = mSampleCount;
    properties[VAOArgs::kResolutionDivisor] = mSDMapResolutionDivisor;
    properties[VAOArgs::kEnableSDGuardBand] = mEnableGuardBand;

    return properties;
}

void VAOBase::setScene(RenderContext* pRenderContext, const ref<Scene>& pScene)
{
    mpScene = pScene;
    requestRecompile();
}

ref<Texture> VAOBase::generateDitherTexture(const ref<Device>& pDevice)
{
    // https://en.wikipedia.org/wiki/Ordered_dithering
    constexpr uint32_t ditherTextureSize = 4;
    constexpr std::array ditherValues = {
        0.0f, 8.0f, 2.0f, 10.0f,
        12.0f, 4.0f, 14.0f, 6.0f,
        3.0f, 11.0f, 1.0f, 9.0f,
        15.0f, 7.0f, 13.0f, 5.0f
    };

    std::vector<uint8_t> data;
    data.resize(ditherTextureSize * ditherTextureSize);

    for (uint32_t i = 0; i < data.size(); i++)
    {
        data[i] = uint8_t(ditherValues[i] / 16.0f * 255.0f);
    }

    return pDevice->createTexture2D(ditherTextureSize, ditherTextureSize, ResourceFormat::R8Unorm, 1, 1, data.data());
}

uint2 VAOBase::getStochMapSize(uint2 fullRes, bool includeGuard) const
{
    return SDMath::getStochMapSize(fullRes, includeGuard, mSDMapResolutionDivisor);
}

int32_t VAOBase::getExtraGuardBand() const
{
    return SDMath::getExtraGuardBand(mSDMapResolutionDivisor, mStochMapGuardBand);
}
