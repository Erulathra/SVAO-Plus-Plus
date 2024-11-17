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
#include "NNAO.h"

#include "Core/Pass/FullScreenPass.h"

namespace
{
    const std::string kDepth = "depth";
    const std::string kStochasticDepth = "stochasticDepth";
    const std::string kNormalBuffer = "normalBuffer";

    const std::string kAmbientOcclusionMask = "ambientOcclusionMask";

    const std::string kWeight0Texture = "weightTexture0";
    const std::string kWeight1Texture = "weightTexture1";
    const std::string kWeight2Texture = "weightTexture2";
    const std::string kWeight3Texture = "weightTexture3";

    const std::string kBias = "bias";
    const std::string kRadius = "radius";
    const std::string kEnableSDF = "enableSDF";

    const std::string kNNAOShaderPath = "RenderPasses/NNAO/NNAO.slang";

    const std::string kNNAOWeight_F0 = "data/nnao_weights/nnao_f0.tga";
    const std::string kNNAOWeight_F1 = "data/nnao_weights/nnao_f1.tga";
    const std::string kNNAOWeight_F2 = "data/nnao_weights/nnao_f2.tga";
    const std::string kNNAOWeight_F3 = "data/nnao_weights/nnao_f3.tga";

    const std::vector<std::string> kNNAOWeightsFilesArray = {
        kNNAOWeight_F0,
        kNNAOWeight_F1,
        kNNAOWeight_F2,
        kNNAOWeight_F3,
    };

    const uint32_t NumWeightTextures = 4;
}

extern "C" FALCOR_API_EXPORT void registerPlugin(Falcor::PluginRegistry& registry)
{
    registry.registerClass<RenderPass, NNAO>();
}

NNAO::NNAO(ref<Device> pDevice, const Properties& props)
    : RenderPass(pDevice)
    , frameNum(0)
{
    Sampler::Desc SamplerDesc;
    SamplerDesc.setFilterMode(TextureFilteringMode::Point, TextureFilteringMode::Point, TextureFilteringMode::Point);
    pPointSampler = pDevice->createSampler(SamplerDesc);

    mpFbo = Fbo::create(pDevice);

    for (uint32_t TextureIndex = 0; TextureIndex < NumWeightTextures; ++TextureIndex)
    {
        pWeightTex[TextureIndex] = Texture::createFromFile(
            pDevice,
            kNNAOWeightsFilesArray[TextureIndex],
            true,
            false);
    }

    SamplerDesc.setFilterMode(TextureFilteringMode::Linear, TextureFilteringMode::Linear, TextureFilteringMode::Linear);
    SamplerDesc.setAddressingMode(TextureAddressingMode::Wrap, TextureAddressingMode::Wrap, TextureAddressingMode::Wrap);
    pWeightsSampler = pDevice->createSampler(SamplerDesc);

    parseProperties(props);
}

Properties NNAO::getProperties() const
{
    Properties props;
    props[kBias] = mCurrentState.bias;
    props[kRadius] = mCurrentState.radius;
    props[kEnableSDF] = mCurrentState.enableSDF;

    return props;
}

RenderPassReflection NNAO::reflect(const CompileData& compileData)
{
    RenderPassReflection reflector;

    reflector.addInput(kDepth, "Depth Texture");
    reflector.addInput(kStochasticDepth, "Stochastic Depth Texture")
        .format(ResourceFormat::D32Float)
        .texture2D(0, 0, mCurrentState.numStochasticDepthSamples);
    reflector.addInput(kNormalBuffer, "Normal buffer Texture");

    reflector.addOutput(kAmbientOcclusionMask, "Ambient Mask")
        // .format(ResourceFormat::R32Float)
        .bindFlags(ResourceBindFlags::ShaderResource | ResourceBindFlags::RenderTarget);

    return reflector;
}

void NNAO::compile(RenderContext* pRenderContext, const CompileData& compileData)
{
    mpNNAOPass = FullScreenPass::create(pRenderContext->getDevice(), kNNAOShaderPath);

    mpNNAOPass->addDefine("USE_STOCHASTIC_DEPTH", "0");
    mpNNAOPass->addDefine("STOCHASTIC_DEPTH_SAMPLES", std::to_string(mCurrentState.numSamples));
    mpNNAOPass->addDefine("NNAO_SAMPLES", std::to_string(mCurrentState.numSamples));
    mpNNAOPass->addDefine("ENABLE_SDF", mCurrentState.enableSDF ? "1" : "0");
    mpNNAOPass->addDefine("SHOW_PROBLEMATIC_SAMPLES", mCurrentState.showProblematicSamples ? "1" : "0");
}

void NNAO::execute(RenderContext* pRenderContext, const RenderData& renderData)
{
    ref<Texture> pInDepthTexture = renderData.getTexture(kDepth);
    ref<Texture> pInStochasticDepthTexture = renderData.getTexture(kStochasticDepth);
    ref<Texture> pInNormalBufferTexture = renderData.getTexture(kNormalBuffer);

    ref<Texture> pAmbientOcclusionMask = renderData.getTexture(kAmbientOcclusionMask);

    if (!mpScene)
    {
        return;
    }

    if (is_set(mpScene->getUpdates(), IScene::UpdateFlags::RecompileNeeded))
    {
        requestRecompile();
        return;
    }

    if (mpNNAOPass && mpFbo)
    {
        ShaderVar var = mpNNAOPass->getRootVar();

        var["gInvProjMat"] = inverse(mpScene->getCamera()->getProjMatrix());
        var["gProjMat"] = mpScene->getCamera()->getProjMatrix();
        var["gViewMat"] = float4x4(mpScene->getCamera()->getViewMatrix());

        var["gDepthResolution"] = renderData.getDefaultTextureDims();

        var["gRadius"] = mCurrentState.radius;
        var["gBias"] = mCurrentState.bias;

        var["gDepth"] = pInDepthTexture;
        var["gStochasticDepth"] = pInStochasticDepthTexture;
        var["gNormalBuffer"] = pInNormalBufferTexture;

        var["gPointSampler"] = pPointSampler;
        var["gWeightSampler"] = pWeightsSampler;

        var["gFrameNum"] = frameNum;

        for (uint32_t TextureIndex = 0; TextureIndex < NumWeightTextures; ++TextureIndex)
        {
            const std::string parameterName = "gWeightF" + std::to_string(TextureIndex);
            var[parameterName] = pWeightTex[TextureIndex];
        }

        mpFbo->attachColorTarget(pAmbientOcclusionMask, 0);
        mpNNAOPass->execute(pRenderContext, mpFbo);
    }

    ++frameNum;
}

void NNAO::renderUI(Gui::Widgets& widget)
{
    widget.var("Bias", mCurrentState.bias, 0.f);
    widget.var("Radius", mCurrentState.radius, 0.f);
    if (widget.checkbox("Enable SDF", mCurrentState.enableSDF))
    {
        requestRecompile();
    }
    if (widget.checkbox("Show problematic samples", mCurrentState.showProblematicSamples))
    {
        requestRecompile();
    }
}

void NNAO::setScene(RenderContext* pRenderContext, const ref<Scene>& pScene)
{
    mpScene = pScene;

    requestRecompile();
}

void NNAO::parseProperties(const Properties& props)
{
    for (const auto& [key, value] : props)
    {
        if (key == kBias)
        {
            mCurrentState.bias = value;
        }
        else if (key == kRadius)
        {
            mCurrentState.radius = value;
        }
        else if (key == kEnableSDF)
        {
            mCurrentState.enableSDF = value;
        }
    }
}
