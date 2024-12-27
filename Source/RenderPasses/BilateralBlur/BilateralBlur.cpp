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

#include "BilateralBlur.h"

#include "Core/Pass/FullScreenPass.h"

namespace
{
    std::string kColorIn = "colorIn";
    std::string kLinearDepthIn = "linearDepthIn";

    std::string kTempInternal = "tempInternal";

    std::string kColorOut = "colorOut";

    namespace Shaders
    {
        std::string kBilateralBlur = "RenderPasses/BilateralBlur/BilateralBlur.slang";
    }

    namespace BlurArgs
    {
        std::string kNumIterations = "numIterations";
        std::string kKernelSize = "kernelSize";
        std::string kBetterSlope = "betterSlope";
    }
}

extern "C" FALCOR_API_EXPORT void registerPlugin(Falcor::PluginRegistry& registry)
{
    registry.registerClass<RenderPass, BilateralBlur>();
}

BilateralBlur::BilateralBlur(ref<Device> pDevice, const Properties& props)
    : RenderPass(pDevice)
{
    mpFbo = Fbo::create(mpDevice);

    Sampler::Desc samplerDesc;
    samplerDesc.setFilterMode(TextureFilteringMode::Point, TextureFilteringMode::Point, TextureFilteringMode::Point);
    samplerDesc.setAddressingMode(TextureAddressingMode::Clamp, TextureAddressingMode::Clamp, TextureAddressingMode::Clamp);
    mpSampler = pDevice->createSampler(samplerDesc);

    for (const auto& [key, value] : props)
    {
        if (key == BlurArgs::kNumIterations) mNumIterations = value;
        else if (key == BlurArgs::kKernelSize) mKernelSize = value;
        else if (key == BlurArgs::kBetterSlope) mBetterSlope = value;
    }
}

Properties BilateralBlur::getProperties() const
{
    Properties properties;

    properties[BlurArgs::kNumIterations] = mNumIterations;
    properties[BlurArgs::kKernelSize] = mKernelSize;
    properties[BlurArgs::kBetterSlope] = mBetterSlope;

    return properties;
}

RenderPassReflection BilateralBlur::reflect(const CompileData& compileData)
{
    RenderPassReflection reflector;

    reflector.addInput(kColorIn, "Input Texture");
    reflector.addInput(kLinearDepthIn, "LinearDepth");

    reflector.addInternal(kTempInternal, "Internal temp texture")
        .format(ResourceFormat::R8Unorm)
        .bindFlags(ResourceBindFlags::AllColorViews);

    reflector.addOutput(kColorOut, "Output Texture")
        .format(ResourceFormat::R8Unorm)
        .bindFlags(ResourceBindFlags::AllColorViews);

    return reflector;
}

void BilateralBlur::compile(RenderContext* pRenderContext, const CompileData& compileData)
{
    DefineList defines;
    defines.add("KERNEL_RADIUS", std::to_string(mKernelSize));
    float2 invResolution = float2(1.f) / float2(compileData.defaultTexDims);
    defines.add("INV_RESOLUTION", fmt::format("float2({},{})", invResolution.x, invResolution.y));
    defines.add("BETTER_SLOPE", std::to_string(static_cast<uint>(mBetterSlope)));

    defines.add("DIRECTION", "float2(1.f, 0.f)");
    mpHorizontalPass = FullScreenPass::create(pRenderContext->getDevice(), Shaders::kBilateralBlur, defines);

    defines["DIRECTION"] = "float2(0.f, 1.f)";
    mpVerticalPass = FullScreenPass::create(pRenderContext->getDevice(), Shaders::kBilateralBlur, defines);
}

void BilateralBlur::execute(RenderContext* pRenderContext, const RenderData& renderData)
{
    ref<Texture> pColorIn = renderData.getTexture(kColorIn);
    ref<Texture> pLinearDepthIn = renderData.getTexture(kLinearDepthIn);

    ref<Texture> pTempTexture = renderData.getTexture(kTempInternal);

    ref<Texture> pColorOut = renderData.getTexture(kColorOut);

    if (!pColorIn || !pLinearDepthIn || !pTempTexture || !pColorOut)
    {
        return;
    }

    {
        ShaderVar varsHorizontal = mpHorizontalPass->getRootVar();
        varsHorizontal["gColorIn"] = pColorIn;
        varsHorizontal["gLinearDepthIn"] = pLinearDepthIn;
        // varsHorizontal["gColorOut"] = pTempTexture;
        varsHorizontal["gSampler"] = mpSampler;

        ShaderVar varsVertical = mpVerticalPass->getRootVar();
        varsVertical["gColorIn"] = pTempTexture;
        varsVertical["gLinearDepthIn"] = pLinearDepthIn;
        // varsVertical["gColorOut"] = pColorOut;
        varsVertical["gSampler"] = mpSampler;
    }

    for (uint i = 0; i < mNumIterations; ++i)
    {
        {
            FALCOR_PROFILE(pRenderContext, "Blur Horizontal");
            mpFbo->attachColorTarget(pTempTexture, 0);
            mpHorizontalPass->execute(pRenderContext, mpFbo);
        }

        {
            FALCOR_PROFILE(pRenderContext, "Blur Vertical");
            mpFbo->attachColorTarget(pColorOut, 0);
            mpVerticalPass->execute(pRenderContext, mpFbo);
        }
    }
}

void BilateralBlur::renderUI(Gui::Widgets& widget)
{
    bool bRequiresRecompile = false;

    bRequiresRecompile |= widget.var("Kernel Radius", mKernelSize, 0u, 20u);
    bRequiresRecompile |= widget.var("Number of iterations", mNumIterations, 0u, 20u);
    bRequiresRecompile |= widget.checkbox("Better slope", mBetterSlope);

    if (bRequiresRecompile)
    {
        requestRecompile();
    }
}
