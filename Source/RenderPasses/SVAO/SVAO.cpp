/***************************************************************************
 # Copyright (c) 2015-23, NVIDIA CORPORATION. All rights reserved.
 #
 # Redistribution and use in source and binary forms, with or without
 # modification, are permitted provided that the following conditions
 # are met:
 #  * Redistributions of source code must retain the above copyright
 #    notice, this list of conditions and the following disclaimer.
 #  * Redistributions in binary form must reproduce the above copyright
 #    notice, this list of conditions and the following disclaimer in the
 #    documentation and/or other materials provided with the distribution.
 #  * Neither the name of NVIDIA CORPORATION nor the names of its
 #    contributors may be used to endorse or promote products derived
 #    from this software without specific prior written permission.
 #
 # THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS "AS IS" AND ANY
 # EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 # IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 # PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 # CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 # EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 # PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 # PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 # OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 # (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 # OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 **************************************************************************/
#include "SVAO.h"

namespace
{
    const std::string kLinearDepthIn = "linearDepthIn";
    const std::string kLinearStochasticDepthIn = "linearSDIn";
    const std::string kNormalsViewIn = "normalViewIn";

    const std::string kAOMaskOut = "aoMaskOut";

    namespace VAO
    {
        const std::string kRadius = "kVaoRadius";
        const std::string kExponent = "kVaoExponent";
        const std::string kSampleCount = "kSampleCount";
    }

    namespace Shaders
    {
        const std::string kSVAOPass = "RenderPasses/SVAO/SVAORaster.ps.slang";
    }
}

extern "C" FALCOR_API_EXPORT void registerPlugin(Falcor::PluginRegistry& registry)
{
    registry.registerClass<RenderPass, SVAO>();
}

SVAO::SVAO(ref<Device> pDevice, const Properties& props) : RenderPass(pDevice)
{
    Sampler::Desc samplerDesc;
    samplerDesc.setFilterMode(TextureFilteringMode::Point, TextureFilteringMode::Point, TextureFilteringMode::Point);
    samplerDesc.setAddressingMode(TextureAddressingMode::Wrap, TextureAddressingMode::Wrap, TextureAddressingMode::Wrap);
    mpPointSampler = pDevice->createSampler(samplerDesc);

    samplerDesc.setFilterMode(TextureFilteringMode::Linear, TextureFilteringMode::Linear, TextureFilteringMode::Linear);
    samplerDesc.setAddressingMode(TextureAddressingMode::Clamp, TextureAddressingMode::Clamp, TextureAddressingMode::Clamp);
    mpLinearSampler = pDevice->createSampler(samplerDesc);

    mpDitherTexture = GenerateDitherTexture(pDevice);

    ApplyProperties(props);
}

void SVAO::ApplyProperties(const Properties& props)
{
    for (const auto& [key, value] : props)
    {
        if (key == VAO::kRadius) mData.radius = value;
        else if (key == VAO::kExponent) mData.exponent = value;
        else if (key == VAO::kSampleCount) mSampleCount = value;
    }
}

ref<SVAO> SVAO::create(ref<Device> pDevice, const Properties& props)
{
    return make_ref<SVAO>(pDevice, props);
}

Properties SVAO::getProperties() const
{
    Properties properties{};

    properties[VAO::kRadius] = mData.radius;
    properties[VAO::kExponent] = mData.exponent;
    properties[VAO::kSampleCount] = mSampleCount;

    return properties;
}

RenderPassReflection SVAO::reflect(const CompileData& compileData)
{
    RenderPassReflection reflector {};

    reflector.addInput(kLinearDepthIn, "Linear Depth");
    reflector.addInput(kLinearStochasticDepthIn, "Linear Normalized Stochastic Depth [0-1]");
    reflector.addInput(kNormalsViewIn, "Normal texture in view space (Uncompressed)");

    reflector.addOutput(kAOMaskOut, "Result AO Mask")
                .bindFlags(ResourceBindFlags::AllColorViews)
                .format(ResourceFormat::R8Unorm);


    return reflector;
}

void SVAO::compile(RenderContext* pRenderContext, const CompileData& compileData)
{
    mData.resolution = float2(compileData.defaultTexDims.x, compileData.defaultTexDims.y);
    mData.invResolution = float2(1.0f) / mData.resolution;
    mData.sdGuard = 0.f;
    mData.lowResolution = mData.resolution; /** getStochMapSize(compileData.defaultTexDims, false); */ // false = dont include guard band here
    mData.noiseScale = mData.resolution / 4.0f; // noise texture is 4x4 resolution

    if (mpScene)
    {
        DefineList defines;
        defines.add(mpScene->getSceneDefines());
        auto rayConeSpread = mpScene->getCamera()->computeScreenSpacePixelSpreadAngle(compileData.defaultTexDims.y);
        defines.add("RAY_CONE_SPREAD", std::to_string(rayConeSpread));

        ref<Device> pDevice = pRenderContext->getDevice();

        {
            ProgramDesc computeShaderDesc;
            mpSVAOComputePass = ComputePass::create(pDevice, Shaders::kSVAOPass, "main", defines);
        }
    }
}

void SVAO::execute(RenderContext* pRenderContext, const RenderData& renderData)
{
    if (!mpScene)
    {
        return;
    }

    ref<Texture> pLinearDepthIn = renderData.getTexture(kLinearDepthIn);
    ref<Texture> pLinearStochasticDepthIn = renderData.getTexture(kLinearDepthIn);
    ref<Texture> pNormalIn = renderData.getTexture(kNormalsViewIn);

    ref<Texture> pAOMaskOut = renderData.getTexture(kAOMaskOut);

    if (!pLinearDepthIn || !pLinearStochasticDepthIn || !pNormalIn)
    {
        return;
    }

    pRenderContext->clearTexture(pAOMaskOut.get(), float4(1.f));

    if (mpSVAOComputePass)
    {
        FALCOR_PROFILE(pRenderContext, "VAO First Pass");

        auto vars = mpSVAOComputePass->getRootVar();
        Camera* pCamera = mpScene->getCamera().get();
        pCamera->bindShaderData(vars["PerFrameCB"]["gCamera"]);
        vars["PerFrameCB"]["invViewMat"] = inverse(pCamera->getViewMatrix());
        vars["PerFrameCB"]["frameIndex"] = mFrameIndex;

        vars["gLinearDepthIn"] = pLinearDepthIn;
        vars["gLinearSDIn"] = pLinearStochasticDepthIn;
        vars["gNormalIn"] = pNormalIn;

        if (mDirty)
        {
            vars["StaticCB"].setBlob(mData);
            vars["gNoiseSampler"] = mpPointSampler;
            vars["gTextureSampler"] = mpLinearSampler;
            vars["gNoiseTex"] = mpDitherTexture;
        }

        vars["PerFrameCB"]["guardBand"] = 0;
        vars["gAOOut"] = pAOMaskOut;

        uint2 nThreads = renderData.getDefaultTextureDims();
        nThreads = ((nThreads + 31u) / 32u) * 32u;

        mpSVAOComputePass->execute(pRenderContext, nThreads.x, nThreads.y);
    }

    mDirty = false;

    ++mFrameIndex;
}

void SVAO::renderUI(Gui::Widgets& widget)
{
    bool requiresRecompile = false;

    // VAO
    if (Gui::Group group = widget.group("VAO", true))
    {
        mDirty |= group.var("Radius", mData.radius, 0.f);
        mDirty |= group.var("Exponent", mData.exponent, 1.f);

        const static Gui::DropdownList kVaoSampleCount =
        {
            {8, "8"},
            {16, "16"},
            {32, "32"}
        };

        requiresRecompile |= group.dropdown("NumSamples", kVaoSampleCount, mSampleCount);
    }

    if (requiresRecompile)
    {
        requestRecompile();
    }
}

void SVAO::setScene(RenderContext* pRenderContext, const ref<Scene>& pScene)
{
    mpScene = pScene;
}

ref<Texture> SVAO::GenerateDitherTexture(const ref<Device>& pDevice)
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
