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
#include "SSAO.h"

#include "Core/Pass/FullScreenPass.h"

namespace
{
    const std::string kDepth = "depth";
    const std::string kStochasticDepth = "stochasticDepth";
    const std::string kNormalBuffer = "normalBuffer";

    const std::string kAmbientOcclusionMask = "ambientOcclusionMask";

    const std::string kBias = "bias";
    const std::string kRadius = "radius";
    const std::string kEnableSDF = "enableSDF";

    const std::string kSSAOShaderPath = "RenderPasses/SSAO/SSAO.slang";
}

extern "C" FALCOR_API_EXPORT void registerPlugin(Falcor::PluginRegistry& registry)
{
    registry.registerClass<RenderPass, SSAO>();
}

SSAO::SSAO(ref<Device> pDevice, const Properties& props)
    : RenderPass(pDevice)
    , frameNum(0)
{
    Sampler::Desc SamplerDesc;
    SamplerDesc.setFilterMode(TextureFilteringMode::Point, TextureFilteringMode::Point, TextureFilteringMode::Point);
    pPointSampler = pDevice->createSampler(SamplerDesc);

    mpFbo = Fbo::create(pDevice);

    parseProperties(props);
}

Properties SSAO::getProperties() const
{
    Properties props;
    props[kBias] = mCurrentState.bias;
    props[kRadius] = mCurrentState.radius;
    props[kEnableSDF] = mCurrentState.enableSDF;

    return props;
}

RenderPassReflection SSAO::reflect(const CompileData& compileData)
{
    RenderPassReflection reflector;

    reflector.addInput(kDepth, "Depth Texture");
    reflector.addInput(kStochasticDepth, "Stochastic Depth Texture")
        .format(ResourceFormat::D32Float)
        .texture2D(0, 0, mCurrentState.numStochasticDepthSamples);
    reflector.addInput(kNormalBuffer, "Normal buffer Texture");

    reflector.addOutput(kAmbientOcclusionMask, "Ambient Mask")
        .format(ResourceFormat::R32Float)
        .bindFlags(ResourceBindFlags::ShaderResource | ResourceBindFlags::RenderTarget);

    return reflector;
}

void SSAO::compile(RenderContext* pRenderContext, const CompileData& compileData)
{
    mpSSAOPass = FullScreenPass::create(pRenderContext->getDevice(), kSSAOShaderPath);

    mpSSAOPass->addDefine("USE_STOCHASTIC_DEPTH", "0");
    mpSSAOPass->addDefine("STOCHASTIC_DEPTH_SAMPLES", std::to_string(mCurrentState.numSSAOSamples));
    mpSSAOPass->addDefine("SSAO_SAMPLES", std::to_string(mCurrentState.numSSAOSamples));
    mpSSAOPass->addDefine("ENABLE_SDF", mCurrentState.enableSDF ? "1" : "0");
}

void SSAO::execute(RenderContext* pRenderContext, const RenderData& renderData)
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

    if (mpSSAOPass && mpFbo)
    {
        ShaderVar var = mpSSAOPass->getRootVar();

        var["gInvProjMat"] = inverse(mpScene->getCamera()->getProjMatrix());
        var["gProjMat"] = mpScene->getCamera()->getProjMatrix();
        var["gViewMat"] = float4x4(transpose(inverse(float3x3(mpScene->getCamera()->getViewMatrix()))));
        // var["gViewMat"] =mpScene->getCamera()->getViewMatrix();

        var["gDepthResolution"] = renderData.getDefaultTextureDims();

        var["gRadius"] = mCurrentState.radius;
        var["gBias"] = mCurrentState.bias;

        var["gDepth"] = pInDepthTexture;
        var["gStochasticDepth"] = pInStochasticDepthTexture;
        var["gNormalBuffer"] = pInNormalBufferTexture;

        var["gPointSampler"] = pPointSampler;

        var["gFrameNum"] = frameNum;

        mpFbo->attachColorTarget(pAmbientOcclusionMask, 0);
        mpSSAOPass->execute(pRenderContext, mpFbo);
    }

    ++frameNum;
}

void SSAO::renderUI(Gui::Widgets& widget)
{
    widget.var("Bias", mCurrentState.bias, 0.f);
    widget.var("Radius", mCurrentState.radius, 0.f);
    if (widget.checkbox("Enable SDF", mCurrentState.enableSDF))
    {
        requestRecompile();
    }
}

void SSAO::setScene(RenderContext* pRenderContext, const ref<Scene>& pScene)
{
    mpScene = pScene;

    requestRecompile();
}

void SSAO::parseProperties(const Properties& props)
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
