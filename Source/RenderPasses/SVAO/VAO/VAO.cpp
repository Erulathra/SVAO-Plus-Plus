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
#include "VAO.h"

namespace
{
    const std::string kLinearDepthIn = "linearDepthIn";
    const std::string kNormalsViewIn = "normalViewIn";

    const std::string kAOOut = "aoOut";
    const std::string kAOMaskOut = "aoMaskOut";

    namespace VAOArgs
    {
        const std::string kSVAOInputMode = "SVAOInputMode";
    }

    namespace Shaders
    {
        const std::string kSVAOPass = "RenderPasses/SVAO/SVAORaster.ps.slang";
    }
}

extern "C" FALCOR_API_EXPORT void registerPlugin(Falcor::PluginRegistry& registry)
{
    registry.registerClass<RenderPass, VAO>();
}

VAO::VAO(ref<Device> pDevice, const Properties& props) : VAOBase(pDevice, props)
{
    for (const auto& [key, value] : props)
    {
        if (key == VAOArgs::kSVAOInputMode) mSVAOInputMode = value;
    }
}

ref<VAO> VAO::create(ref<Device> pDevice, const Properties& props)
{
    return make_ref<VAO>(pDevice, props);
}

Properties VAO::getProperties() const
{
    Properties properties = VAOBase::getProperties();

    properties[VAOArgs::kSVAOInputMode] = mSVAOInputMode;

    return properties;
}

RenderPassReflection VAO::reflect(const CompileData& compileData)
{
    RenderPassReflection reflector {};

    reflector.addInput(kLinearDepthIn, "Linear Depth");
    reflector.addInput(kNormalsViewIn, "Normal texture in view space (Uncompressed)");

    reflector.addOutput(kAOOut, "Result AO")
                .bindFlags(ResourceBindFlags::AllColorViews)
                .format(ResourceFormat::R8Unorm);

    // reflector.addOutput()

    return reflector;
}

void VAO::compile(RenderContext* pRenderContext, const CompileData& compileData)
{
    VAOBase::compile(pRenderContext, compileData);

    if (mpScene)
    {
        DefineList defines = GetCommonDefines(compileData);

        ProgramDesc computeShaderDesc;
        mpComputePass = ComputePass::create(pRenderContext->getDevice(), Shaders::kSVAOPass, "main", defines);
    }
}

void VAO::execute(RenderContext* pRenderContext, const RenderData& renderData)
{
    VAOBase::execute(pRenderContext, renderData);

    if (!mpScene)
    {
        return;
    }

    ref<Texture> pLinearDepthIn = renderData.getTexture(kLinearDepthIn);
    ref<Texture> pNormalIn = renderData.getTexture(kNormalsViewIn);

    ref<Texture> pAOMaskOut = renderData.getTexture(kAOOut);

    if (!pLinearDepthIn || !pNormalIn)
    {
        return;
    }

    pRenderContext->clearTexture(pAOMaskOut.get(), float4(1.f));

    if (mpComputePass)
    {
        FALCOR_PROFILE(pRenderContext, "VAO First Pass");

        auto vars = mpComputePass->getRootVar();
        Camera* pCamera = mpScene->getCamera().get();
        pCamera->bindShaderData(vars["PerFrameCB"]["gCamera"]);
        vars["PerFrameCB"]["invViewMat"] = inverse(pCamera->getViewMatrix());
        vars["PerFrameCB"]["frameIndex"] = mFrameIndex;

        vars["gLinearDepthIn"] = pLinearDepthIn;
        vars["gNormalIn"] = pNormalIn;

        SetCommonVars(vars);

        vars["PerFrameCB"]["guardBand"] = 0;
        vars["gAOOut"] = pAOMaskOut;

        uint2 nThreads = renderData.getDefaultTextureDims();
        nThreads = ((nThreads + 31u) / 32u) * 32u;

        mpComputePass->execute(pRenderContext, nThreads.x, nThreads.y);
    }
}

void VAO::renderUI(Gui::Widgets& widget)
{
    VAOBase::renderUI(widget);

    widget.checkbox("SVAO input Mode", mSVAOInputMode);
}