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
#include "BlinnPhong.h"

#include "Core/Pass/FullScreenPass.h"

namespace
{
    const std::string kSpecRoughIn = "specRough";
    const std::string kNormalBuffer = "normalBuffer";
    const std::string kDiffuseOpacityIn = "diffuseOpacity";
    
    const std::string kColorOut = "diffuseOpacity";

    const std::string kBlingPhongShaderPass = "RenderPasses/BlinnPhong/BlinnPhong.slang";

    // params
    const std::string kParamBrightness = "gBrightness";
    const std::string kAmbientLight = "gAmbientLight";
}

extern "C" FALCOR_API_EXPORT void registerPlugin(Falcor::PluginRegistry& registry)
{
    registry.registerClass<RenderPass, BlinnPhong>();
}

ref<BlinnPhong> BlinnPhong::create(ref<Device> pDevice, const Properties& props)
{
    return make_ref<BlinnPhong>(pDevice, props);
}

BlinnPhong::BlinnPhong(ref<Device> pDevice, const Properties& props)
    : RenderPass(pDevice)
    , mAmbientLight(0.2f)
{
    parseProperties(props);
    
    Sampler::Desc samplerDesc;
    samplerDesc.setFilterMode(TextureFilteringMode::Linear, TextureFilteringMode::Linear, TextureFilteringMode::Point);
    mpFullScreenSampler = mpDevice->createSampler(samplerDesc);
}

Properties BlinnPhong::getProperties() const
{
    Properties props;
    props[kAmbientLight] = mAmbientLight;
    
    return props;
}

RenderPassReflection BlinnPhong::reflect(const CompileData& compileData)
{
    RenderPassReflection reflector;
    
    // Inputs
    reflector.addInput(kDiffuseOpacityIn, "Diffuse (RGB Channels) Opacity (A Channel)");
    reflector.addInput(kSpecRoughIn, "Specular (RGB Channels) Roughness (A Channel)");
    reflector.addInput(kNormalBuffer, "Normal buffer");

    // Outputs
    reflector.addOutput(kColorOut, "Out color");

    // Shaders
    
    
    return reflector;
}

void BlinnPhong::execute(RenderContext* pRenderContext, const RenderData& renderData)
{
    if (!mpBlinnPhongPass)
    {
        return;
    }
    
    ref<Texture> pDiffuseOpacityTexture = renderData.getTexture(kDiffuseOpacityIn);
    ref<Texture> pSpecRoughTexture = renderData.getTexture(kSpecRoughIn);
    ref<Texture> pNormalBufferTexture = renderData.getTexture(kNormalBuffer);

    ref<Texture> pColorTexture = renderData.getTexture(kColorOut);
    
    if (!pDiffuseOpacityTexture || !pSpecRoughTexture || !pNormalBufferTexture)
    {
        logWarning("BlinnPhong::Execute() - missing input texture(s)");
        return;
    }

    ref<Fbo> pFbo = Fbo::create(mpDevice);
    pFbo->attachColorTarget(pColorTexture, 0);


    ShaderVar var = mpBlinnPhongPass->getRootVar();
    mpScene->bindShaderData(var["gScene"]);
    var["gDiffuseOpacityTex"] = pDiffuseOpacityTexture;
    var["gNormalBufferTex"] = pNormalBufferTexture;
    var["gFullScreenSampler"] = mpFullScreenSampler;
    
    var["gAmbientLight"] = mAmbientLight;

    mpBlinnPhongPass->execute(pRenderContext, pFbo);
}

void BlinnPhong::renderUI(Gui::Widgets& widget)
{
    widget.rgbColor("AmbientLight", mAmbientLight);
}

void BlinnPhong::setScene(RenderContext* pRenderContext, const ref<Scene>& pScene)
{
    mpScene = pScene;
    
    mpBlinnPhongPass = FullScreenPass::create(pRenderContext->getDevice(), kBlingPhongShaderPass, mpScene->getSceneDefines());
}

void BlinnPhong::parseProperties(const Properties& props)
{
    for (const auto& [key, value] : props)
    {
        if (key == kAmbientLight)
        {
            mAmbientLight = value;
        }
    }
}
