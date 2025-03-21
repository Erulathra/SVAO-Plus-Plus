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
    const std::string kPrepassMask = "prepassMask";

    const std::string kAOOut = "aoOut";
    const std::string kAOMaskOut = "aoMaskOut";
    const std::string kRayMinOut = "rayMinOut";
    const std::string kRayMaxOut = "rayMaxOut";

    namespace VAOArgs
    {
        const std::string kSVAOInputMode = "SVAOInputMode";
        const std::string kUseRayInterval = "useRayInterval";
        const std::string kUsePrepass = "usePrepass";
        const std::string kPrePassSamplingMode = "prePassSamplingMode";
    }

    namespace Shaders
    {
        const std::string kVAOPass = "RenderPasses/SVAO/VAO/VAO.ps.slang";
    }
}

VAO::VAO(ref<Device> pDevice, const Properties& props) : VAOBase(pDevice, props)
{
    for (const auto& [key, value] : props)
    {
        if (key == VAOArgs::kSVAOInputMode) mSVAOInputMode = value;
        else if (key == VAOArgs::kUseRayInterval) mUseRayInterval = value;
        else if (key == VAOArgs::kUsePrepass) mUsePrepass = value;
        else if (key == VAOArgs::kPrePassSamplingMode) mPrepassSamplingMode = value;
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
    properties[VAOArgs::kUseRayInterval] = mUseRayInterval;
    properties[VAOArgs::kUsePrepass] = mUsePrepass;
    properties[VAOArgs::kPrePassSamplingMode] = mPrepassSamplingMode;

    return properties;
}

RenderPassReflection VAO::reflect(const CompileData& compileData)
{
    RenderPassReflection reflector {};

    reflector.addInput(kLinearDepthIn, "Linear Depth");
    reflector.addInput(kNormalsViewIn, "Normal texture in view space");
    reflector.addInput(kPrepassMask, "Prepass mask")
                .flags(RenderPassReflection::Field::Flags::Optional);

    reflector.addOutput(kAOOut, "Result AO")
                .bindFlags(ResourceBindFlags::AllColorViews)
                .format(ResourceFormat::R8Unorm);

    reflector.addOutput(kAOMaskOut, "AO Mask (optional)").bindFlags(ResourceBindFlags::AllColorViews).format(ResourceFormat::R8Uint);

    uint2 sdResolution = getStochMapSize(compileData.defaultTexDims, mEnableGuardBand);

    reflector.addOutput(kRayMinOut, "Ray start (Ray interval optimization)")
                .bindFlags(ResourceBindFlags::AllColorViews)
                .format(ResourceFormat::R32Int)
                .texture2D(sdResolution.x, sdResolution.y);

    reflector.addOutput(kRayMaxOut, "Ray end (Ray interval optimization)")
                .bindFlags(ResourceBindFlags::AllColorViews)
                .format(ResourceFormat::R32Int)
                .texture2D(sdResolution.x, sdResolution.y);

    return reflector;
}

void VAO::compile(RenderContext* pRenderContext, const CompileData& compileData)
{
    VAOBase::compile(pRenderContext, compileData);

    if (mpScene)
    {
        DefineList defines = GetCommonDefines(compileData);
        defines.add("SECONDARY_DEPTH_MODE", mSVAOInputMode ? "1" : "0");
        defines.add("USE_RAY_INTERVAL", mSVAOInputMode && mUseRayInterval ? "1" : "0");
        defines.add("USE_PREPASS", mUsePrepass ? "1" : "0");
        defines.add("DEBUG_PREPASS", mDebugPrepass ? "1" : "0");
        defines.add("PREPASS_SAMPLING_MODE", std::to_string(static_cast<uint32_t>(mPrepassSamplingMode)));

        ProgramDesc computeShaderDesc;
        mpComputePass = ComputePass::create(pRenderContext->getDevice(), Shaders::kVAOPass, "main", defines);
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
    ref<Texture> pPrepassMask = renderData.getTexture(kPrepassMask);

    ref<Texture> pAOOut = renderData.getTexture(kAOOut);
    ref<Texture> pAOMaskOut = renderData.getTexture(kAOMaskOut);

    ref<Texture> pRayMinOut = renderData.getTexture(kRayMinOut);
    ref<Texture> pRayMaxOut = renderData.getTexture(kRayMaxOut);

    if (!pLinearDepthIn || !pNormalIn)
    {
        return;
    }

    pRenderContext->clearTexture(pAOOut.get(), float4(1.f));

    if (mpComputePass)
    {
        ShaderVar vars = mpComputePass->getRootVar();
        SetCommonVars(vars, mpScene.get());

        vars["gLinearDepthIn"] = pLinearDepthIn;
        vars["gNormalIn"] = pNormalIn;

        vars["PerFrameCB"]["guardBand"] = 0;
        vars["gAOOut"] = pAOOut;

        if (pRayMinOut && pRayMaxOut)
        {
            pRenderContext->clearUAV(pRayMinOut->getUAV().get(), uint4(asuint(std::numeric_limits<float>::max())));
            pRenderContext->clearUAV(pRayMaxOut->getUAV().get(), uint4(0u));

            vars["gRayMaxOut"] = pRayMaxOut;
            vars["gRayMinOut"] = pRayMinOut;
        }

        if (pPrepassMask && mUsePrepass)
        {
            vars["gPrepassMask"] = pPrepassMask;

            const float2 prepassResolution = float2(pPrepassMask->getWidth(), pPrepassMask->getHeight());
            vars["StaticCB"]["gPrepassResolution"] = prepassResolution;
            vars["StaticCB"]["gPrepassInvResolution"] = 1.f / prepassResolution;
        }

        if (mSVAOInputMode)
        {
            vars["gStencil"] = pAOMaskOut;
        }

        uint2 nThreads = renderData.getDefaultTextureDims();
        nThreads = ((nThreads + 31u) / 32u) * 32u;

        mpComputePass->execute(pRenderContext, nThreads.x, nThreads.y);
    }
}

void VAO::renderUI(Gui::Widgets& widget)
{
    VAOBase::renderUI(widget);

    bool requiresRecompile = false;
    requiresRecompile |= widget.checkbox("SVAO input Mode", mSVAOInputMode);
    requiresRecompile |= widget.checkbox("Use Ray Interval", mUseRayInterval);
    requiresRecompile |= widget.checkbox("Use Prepass", mUsePrepass);
    requiresRecompile |= widget.checkbox("Debug Prepass", mDebugPrepass);
    requiresRecompile |= widget.dropdown("Prepass sampling mode", mPrepassSamplingMode);

    if (requiresRecompile)
    {
        requestRecompile();
    }
}
