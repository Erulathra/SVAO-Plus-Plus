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
#include "DeferredLighting.h"

#include "Core/Pass/FullScreenPass.h"
#include "RenderGraph/RenderPassHelpers.h"

namespace
{
    const ChannelList kChannels = {
        // clang-format off
        { "posW","gPosW","Position in world space", false /* optional */, ResourceFormat::RGBA32Float },
        { "normW","gNormW","Shading normal in world space",false /* optional */, ResourceFormat::RGBA32Float },
        { "diffuseOpacity","gDiffOpacity","Diffuse reflection albedo and opacity",true /* optional */, ResourceFormat::RGBA32Float },
        { "specRough","gSpecRough","Specular reflectance and roughness",true /* optional */, ResourceFormat::RGBA32Float },
        // clang-format on
    };

    const std::string kColorOut = "kColorOut";

    const std::string kAmbientLight = "ambientLight";

    const std::string kShaderPath = "RenderPasses/DeferredLighting/DeferredLighting.slang";
}

extern "C" FALCOR_API_EXPORT void registerPlugin(Falcor::PluginRegistry& registry)
{
    registry.registerClass<RenderPass, DeferredLighting>();
}

DeferredLighting::DeferredLighting(ref<Device> pDevice, const Properties& props)
    : RenderPass(pDevice)
{
    mpFbo = Fbo::create(pDevice);

    for (const auto& [key, value] : props)
    {
        if (key == kAmbientLight) mAmbientLight = value;
    }
}

Properties DeferredLighting::getProperties() const
{
    Properties properties;
    properties[kAmbientLight];
    return properties;
}

RenderPassReflection DeferredLighting::reflect(const CompileData& compileData)
{
    RenderPassReflection reflector;
    addRenderPassInputs(reflector, kChannels);
    reflector.addOutput(kColorOut, "Out Color texture");
    return reflector;
}
void DeferredLighting::compile(RenderContext* pRenderContext, const CompileData& compileData)
{
    if (!mpScene)
    {
        return;
    }

    RenderPass::compile(pRenderContext, compileData);

    DefineList defines = mpScene->getSceneDefines();

    mpPass = FullScreenPass::create(pRenderContext->getDevice(), kShaderPath, defines);
}

void DeferredLighting::execute(RenderContext* pRenderContext, const RenderData& renderData)
{
    if (!mpScene)
    {
        return;
    }

    ShaderVar rootVar = mpPass->getRootVar();
    mpScene->bindShaderData(rootVar["gScene"]);

    for (const ChannelDesc& channel : kChannels)
    {
        rootVar[channel.texname] = renderData.getTexture(channel.name);
    }

    ShaderVar PerFrameCB = rootVar["PerFrameCB"];
    PerFrameCB["ambientLight"] = mAmbientLight;
    PerFrameCB["linearFalloff"] = mLinearFalloff;
    PerFrameCB["quadraticFalloff"] = mQuadraticFalloff;

    ref<Texture> pColorOut = renderData.getTexture(kColorOut);
    mpFbo->attachColorTarget(pColorOut, 0);

    mpPass->execute(pRenderContext, mpFbo);
}

void DeferredLighting::renderUI(Gui::Widgets& widget)
{
    widget.var("Ambient Light", mAmbientLight, 0.f, 1.f);
    widget.var("Linear Falloff", mLinearFalloff, 0.0014f, 0.7f, 0.0001f);
    widget.var("Quadratic Falloff", mQuadraticFalloff, 0.000007f, 1.8f, 0.0001f);
}

void DeferredLighting::setScene(RenderContext* pRenderContext, const ref<Scene>& pScene)
{
    RenderPass::setScene(pRenderContext, pScene);
    if (mpScene != pScene)
    {
        mpScene = pScene;
        requestRecompile();
    }
}
