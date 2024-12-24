#include "VAOBase.h"

#include "SVAO.h"
#include "VAO.h"

namespace
{
    namespace VAOArgs
    {
        const std::string kRadius = "kVaoRadius";
        const std::string kExponent = "kVaoExponent";
        const std::string kSampleCount = "kSampleCount";
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

    mpDitherTexture = GenerateDitherTexture(pDevice);

    for (const auto& [key, value] : props)
    {
        if (key == VAOArgs::kRadius) mVaoData.radius = value;
        else if (key == VAOArgs::kExponent) mVaoData.exponent = value;
        else if (key == VAOArgs::kSampleCount) mSampleCount = value;
    }
}

DefineList VAOBase::GetCommonDefines(const CompileData& compileData)
{
    DefineList defines;

    defines.add(mpScene->getSceneDefines());
    auto rayConeSpread = mpScene->getCamera()->computeScreenSpacePixelSpreadAngle(compileData.defaultTexDims.y);
    defines.add("RAY_CONE_SPREAD", std::to_string(rayConeSpread));
    defines.add("NUM_DIRECTIONS", std::to_string(mSampleCount));

    return defines;
}

void VAOBase::SetCommonVars(ShaderVar& vars, Scene* pScene)
{
    vars["StaticCB"].setBlob(mVaoData);
    vars["gNoiseSampler"] = mpPointSampler;
    vars["gTextureSampler"] = mpLinearSampler;
    vars["gNoiseTex"] = mpDitherTexture;

    Camera* pCamera = pScene->getCamera().get();
    pCamera->bindShaderData(vars["PerFrameCB"]["gCamera"]);
    vars["PerFrameCB"]["invViewMat"] = inverse(pCamera->getViewMatrix());
    vars["PerFrameCB"]["frameIndex"] = mFrameIndex;
}
void VAOBase::compile(RenderContext* pRenderContext, const CompileData& compileData)
{
    RenderPass::compile(pRenderContext, compileData);

    mVaoData.resolution = float2(compileData.defaultTexDims.x, compileData.defaultTexDims.y);
    mVaoData.invResolution = float2(1.0f) / mVaoData.resolution;
    mVaoData.sdGuard = 0.f;
    mVaoData.lowResolution = mVaoData.resolution; /** getStochMapSize(compileData.defaultTexDims, false); */ // false = dont include guard band here
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
    bool requiresRecompile = false;

    // VAO
    if (Gui::Group group = widget.group("VAO", true))
    {
        group.var("Radius", mVaoData.radius, 0.f);
        group.var("Exponent", mVaoData.exponent, 1.f);

        const static Gui::DropdownList kVaoSampleCount = {{8, "8"}, {16, "16"}, {32, "32"}};

        requiresRecompile |= group.dropdown("NumSamples", kVaoSampleCount, mSampleCount);
    }

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

    return properties;
}

void VAOBase::setScene(RenderContext* pRenderContext, const ref<Scene>& pScene)
{
    mpScene = pScene;
    requestRecompile();
}

ref<Texture> VAOBase::GenerateDitherTexture(const ref<Device>& pDevice)
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