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
    const std::string kStochDepthIn = "stochDepthIn";
    const std::string kNormalViewIn = "normalViewIn";
    const std::string kAOMaskIn = "aoMaskIn";

    const std::string kAOInOut = "aoInOut";

    namespace Shaders
    {
        const std::string kSVAOPass = "RenderPasses/SVAO/SVAO/SVAORaster2.ps.slang";
    }
}

SVAO::SVAO(ref<Device> pDevice, const Properties& props)
    : VAOBase(pDevice, props)
{

}

ref<SVAO> SVAO::create(ref<Device> pDevice, const Properties& props)
{
    return make_ref<SVAO>(pDevice, props);
}
RenderPassReflection SVAO::reflect(const CompileData& compileData)
{
    RenderPassReflection reflector{};

    reflector.addInput(kLinearDepthIn, "Linear Depth");
    reflector.addInput(kStochDepthIn, "Stochastic Normalized Depth");
    reflector.addInput(kNormalViewIn, "Normal texture in view space (Uncompressed)");
    reflector.addInput(kAOMaskIn, "AOMask from first VAO pass");

    reflector.addInputOutput(kAOInOut, "Ambient occlusion UAV").bindFlags(ResourceBindFlags::AllColorViews);

    return reflector;
}

void SVAO::compile(RenderContext* pRenderContext, const CompileData& compileData)
{
    VAOBase::compile(pRenderContext, compileData);

    if (mpScene)
    {
        DefineList defines = GetCommonDefines(compileData);
        defines.add("SECONDARY_DEPTH_MODE", "1");

        ProgramDesc computeShaderDesc;
        mpComputePass = ComputePass::create(pRenderContext->getDevice(), Shaders::kSVAOPass, "main", defines);
    }
}
void SVAO::execute(RenderContext* pRenderContext, const RenderData& renderData)
{
    VAOBase::execute(pRenderContext, renderData);

    if (!mpScene)
    {
        return;
    }

    ref<Texture> pLinearDepthIn = renderData.getTexture(kLinearDepthIn);
    ref<Texture> pStochDepthIn = renderData.getTexture(kStochDepthIn);
    ref<Texture> pNormalViewIn = renderData.getTexture(kNormalViewIn);
    ref<Texture> pAOMaskIn = renderData.getTexture(kAOMaskIn);

    ref<Texture> pAOInOut = renderData.getTexture(kAOInOut);

    if (!pLinearDepthIn || !pStochDepthIn || !pNormalViewIn || !pAOMaskIn || !pAOInOut)
    {
        return;
    }

    if (mpComputePass)
    {
        ShaderVar vars = mpComputePass->getRootVar();
        SetCommonVars(vars, mpScene.get());

        vars["gLinearDepthIn"] = pLinearDepthIn;
        vars["gNormalIn"] = pNormalViewIn;
        vars["gLinearSDIn"] = pStochDepthIn;

        vars["gAOMaskIn"] = pAOMaskIn;
        vars["gAOInOut"] = pAOInOut;

        vars["PerFrameCB"]["guardBand"] = 0;

        uint2 nThreads = renderData.getDefaultTextureDims() /*- uint2(2 * guardBand)*/;
        mpComputePass->execute(pRenderContext, nThreads.x, nThreads.y);
    }
}