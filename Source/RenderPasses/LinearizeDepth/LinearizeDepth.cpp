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
#include "LinearizeDepth.h"

namespace
{
    const std::string kLinearizeDepthShaderPath = "RenderPasses/LinearizeDepth/LinearizeDepth.slang";

    const std::string kDepthIn = "depthIn";
    const std::string kLinearDepthOut = "linearDepthOut";
}

extern "C" FALCOR_API_EXPORT void registerPlugin(Falcor::PluginRegistry& registry)
{
    registry.registerClass<RenderPass, LinearizeDepth>();
}

LinearizeDepth::LinearizeDepth(ref<Device> pDevice, const Properties& props)
: RenderPass(pDevice)
{
    Sampler::Desc samplerDesc;
    samplerDesc.setFilterMode(TextureFilteringMode::Point, TextureFilteringMode::Point, TextureFilteringMode::Point);
    mpFullScreenSampler = pDevice->createSampler(samplerDesc);

    mpFbo = Fbo::create(pDevice);
}

Properties LinearizeDepth::getProperties() const
{
    return {};
}

RenderPassReflection LinearizeDepth::reflect(const CompileData& compileData)
{
    RenderPassReflection reflector;
    reflector.addInput(kDepthIn, "non-linear depth input");
    reflector.addOutput(kLinearDepthOut, "linear depth output").format(ResourceFormat::R32Float);

    return reflector;
}

void LinearizeDepth::execute(RenderContext* pRenderContext, const RenderData& renderData)
{
    if (!mpScene)
    {
        return;
    }

    ref<Texture> pInDepthTex = renderData.getTexture(kDepthIn);
    ref<Texture> pOutLinearDepthTex = renderData.getTexture(kLinearDepthOut);

    if (!pInDepthTex || !pOutLinearDepthTex)
    {
        logWarning("LinearizeDepth::execute() - missing input texture(s)");
        return;
    }

    if (is_set(mpScene->getUpdates(), IScene::UpdateFlags::RecompileNeeded))
    {
        mpLinearizeDepthPass = nullptr;
    }

    if (!mpLinearizeDepthPass)
    {
        mpLinearizeDepthPass = FullScreenPass::create(mpDevice, kLinearizeDepthShaderPath);
    }

    mpFbo->attachColorTarget(pOutLinearDepthTex, 0);

    ShaderVar var = mpLinearizeDepthPass->getRootVar();
    var["gNearPlane"] = mpScene->getCamera()->getNearPlane();
    var["gFarPlane"] = mpScene->getCamera()->getFarPlane();

    var["gInDepthTex"] = pInDepthTex;
    var["gFullscreenSampler"] = mpFullScreenSampler;

    mpLinearizeDepthPass->execute(pRenderContext, mpFbo);
}

void LinearizeDepth::renderUI(Gui::Widgets& widget)
{

}

void LinearizeDepth::setScene(RenderContext* pRenderContext, const ref<Scene>& pScene)
{
    mpScene = pScene;
}
