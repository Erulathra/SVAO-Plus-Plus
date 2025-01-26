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
#include "VAOPrepass.h"

#include "Utils/Math/SDMath.h"

namespace
{
    const std::string kLinearDepthIn = "linearDepthIn";
    const std::string kNormalsViewIn = "normalViewIn";

    const std::string kAOMaskOut = "aoMaskOut";

    namespace Shaders
    {
        const std::string kVAOPass = "RenderPasses/SVAO/VAOPrepass/VAOPrepass.ps.slang";
    }

    const std::string kAOThreshold = "aoThreshold";

    constexpr uint32_t kResolutionDivisor = 8;
}


VAOPrepass::VAOPrepass(ref<Device> pDevice, const Properties& props)
: VAOBase(pDevice, props)
{
    for (const auto& [key, value] : props)
    {
        if (key == kAOThreshold) mAOThreshold = value;
    }
}

ref<VAOPrepass> VAOPrepass::create(ref<Device> pDevice, const Properties& props)
{
    return make_ref<VAOPrepass>(pDevice, props);
}

Properties VAOPrepass::getProperties() const
{
    Properties properties = VAOBase::getProperties();

    properties[kAOThreshold] = mAOThreshold;

    return properties;
}

RenderPassReflection VAOPrepass::reflect(const CompileData& compileData)
{
    RenderPassReflection reflector {};

    reflector.addInput(kLinearDepthIn, "Linear Depth");
    reflector.addInput(kNormalsViewIn, "Normal texture in view space (Uncompressed)");

    const uint2 maskSize = SDMath::getStochMapSize(compileData.defaultTexDims, false, kResolutionDivisor);

    reflector.addOutput(kAOMaskOut, "AOMask")
             .bindFlags(ResourceBindFlags::AllColorViews)
             .texture2D(maskSize.x, maskSize.y)
             .format(ResourceFormat::R8Unorm);

    return reflector;
}

void VAOPrepass::compile(RenderContext* pRenderContext, const CompileData& compileData)
{
    VAOBase::compile(pRenderContext, compileData);

    mVaoData.aoResolution = SDMath::getStochMapSize(compileData.defaultTexDims, false, kResolutionDivisor);
    mVaoData.aoInvResolution = float2(1.0f) / mVaoData.aoResolution;
    mVaoData.sdGuard = 0;
    mVaoData.lowResolution = float2(0.f);
    mVaoData.noiseScale = mVaoData.aoResolution / 4.0f; // noise texture is 4x4 resolution

    if (mpScene)
    {
        DefineList defines = GetCommonDefines(compileData);
        defines["SECONDARY_DEPTH_MODE"] = "0";
        defines["USE_RAY_INTERVAL"] = "0";
        defines["ADAPTIVE_SAMPLING"] = "0";
        defines["NUM_DIRECTIONS"] = "8";

        ProgramDesc computeShaderDesc;
        mpComputePass = ComputePass::create(pRenderContext->getDevice(), Shaders::kVAOPass, "main", defines);
    }
}

void VAOPrepass::execute(RenderContext* pRenderContext, const RenderData& renderData)
{
    VAOBase::execute(pRenderContext, renderData);

    if (!mpScene)
    {
        return;
    }

    ref<Texture> pLinearDepthIn = renderData.getTexture(kLinearDepthIn);
    ref<Texture> pNormalIn = renderData.getTexture(kNormalsViewIn);

    ref<Texture> pAOMaskOut = renderData.getTexture(kAOMaskOut);

    pRenderContext->clearTexture(pAOMaskOut.get(), float4(1.f));

    if (mpComputePass)
    {
        ShaderVar vars = mpComputePass->getRootVar();
        SetCommonVars(vars, mpScene.get());

        vars["PerFrameCB"]["guardBand"] = 0;

        vars["StaticCB"]["gAOThreshold"] = mAOThreshold;

        vars["gLinearDepthIn"] = pLinearDepthIn;
        vars["gNormalIn"] = pNormalIn;

        vars["gAOMaskOut"] = pAOMaskOut;

        uint2 nThreads = SDMath::getStochMapSize(renderData.getDefaultTextureDims(), false, kResolutionDivisor);;
        nThreads = ((nThreads + 31u) / 32u) * 32u;

        mpComputePass->execute(pRenderContext, nThreads.x, nThreads.y);
    }
}

void VAOPrepass::renderUI(Gui::Widgets& widget)
{
    VAOBase::renderUI(widget);

    widget.var("AO Threshold", mAOThreshold);
}
