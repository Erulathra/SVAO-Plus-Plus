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
#include "StochasticDepthStratfield.h"

#include "RenderGraph/RenderPassHelpers.h"

namespace
{
    const std::string kStratfieldStochasticDepthPassProgramFile = "RenderPasses/StochasticDepthStratfield/StochasticDepthStratfieldPass.3d.slang";

    const std::string kDepthTextureName = "depthTexture";
    const std::string kStochasticDepthName = "stochasticDepth";

    const std::string kAlpha = "alpha";
    const std::string kNumSamples = "numSamples";
}

extern "C" FALCOR_API_EXPORT void registerPlugin(Falcor::PluginRegistry& registry)
{
    registry.registerClass<RenderPass, StochasticDepthStratfield>();
}

StochasticDepthStratfield::StochasticDepthStratfield(ref<Device> pDevice, const Properties& props)
: RenderPass(pDevice)
{
    mStochasticDepthPass.pState = GraphicsState::create(pDevice);

    Sampler::Desc SamplerDesc;
    SamplerDesc.setFilterMode(TextureFilteringMode::Point, TextureFilteringMode::Point, TextureFilteringMode::Point);
    mStochasticDepthPass.pSampler = pDevice->createSampler(SamplerDesc);

    mpFbo = Fbo::create(pDevice);

    parseProperties(props);
}

Properties StochasticDepthStratfield::getProperties() const
{
    Properties props;

    props[kAlpha] = mCurrentState.alpha;
    props[kNumSamples] = mCurrentState.numSamples;

    return props;
}

RenderPassReflection StochasticDepthStratfield::reflect(const CompileData& compileData)
{
    RenderPassReflection reflector;

    reflector.addInput(kDepthTextureName, "Depth Texture");

    reflector.addOutput(kStochasticDepthName, "Stochastic Depth")
             .format(ResourceFormat::D32Float)
             .bindFlags(ResourceBindFlags::DepthStencil)
             .texture2D();

    return reflector;
}

void StochasticDepthStratfield::execute(RenderContext* pRenderContext, const RenderData& renderData)
{
    // Inputs
    ref<Texture> pInDepthTexture = renderData.getTexture(kDepthTextureName);
    if (!pInDepthTexture)
    {
        logWarning("StochasticDepthStratfield::execute() - missing input depth texture");
        return;
    }

    // Outputs
    ref<Texture> pStochasticDepth = renderData.getTexture(kStochasticDepthName);
    FALCOR_ASSERT(pStochasticDepth)

    pRenderContext->clearDsv(pStochasticDepth->getDSV().get(), 1.f, 0);

    if (!mpScene)
    {
        return;
    }

    if (is_set(mpScene->getUpdates(), IScene::UpdateFlags::RecompileNeeded))
    {
        recreatePrograms();
    }

    // Stochastic Depth Pass
    {
        // create program
        if (!mStochasticDepthPass.pProgram)
        {
            ProgramDesc desc;
            desc.addShaderModules(mpScene->getShaderModules());
            desc.addShaderLibrary(kStratfieldStochasticDepthPassProgramFile).vsEntry("vsMain").psEntry("psMain");
            desc.addTypeConformances(mpScene->getTypeConformances());

            mStochasticDepthPass.pProgram = Program::create(mpDevice, desc, mpScene->getSceneDefines());
            mStochasticDepthPass.pState->setProgram(mStochasticDepthPass.pProgram);
        }

        mStochasticDepthPass.pState->getProgram()->addDefine("ALPHA", std::to_string(mCurrentState.alpha));
        mStochasticDepthPass.pState->getProgram()->addDefine("NUM_SAMPLES", std::to_string(mCurrentState.numSamples));

        // create vars
        if (!mStochasticDepthPass.pVars)
        {
            mStochasticDepthPass.pVars = ProgramVars::create(mpDevice, mStochasticDepthPass.pProgram.get());
        }

        ShaderVar var = mStochasticDepthPass.pVars->getRootVar();
        var["gDepthTexture"] = pInDepthTexture;
        var["gSampler"] = mStochasticDepthPass.pSampler;
        var["gInvResolution"] = 1.f / float2(pInDepthTexture->getWidth(), pInDepthTexture->getHeight());

        mpFbo->attachDepthStencilTarget(pStochasticDepth);
        mStochasticDepthPass.pState->setFbo(mpFbo);

        mpScene->rasterize(pRenderContext, mStochasticDepthPass.pState.get(), mStochasticDepthPass.pVars.get(), RasterizerState::CullMode::Back);
    }
}

void StochasticDepthStratfield::renderUI(Gui::Widgets& widget)
{
    widget.slider("Alpha", mCurrentState.alpha, 0.1f, 1.f);
    widget.slider("Num Samples", mCurrentState.numSamples, 4, 32);
}

void StochasticDepthStratfield::setScene(RenderContext* pRenderContext, const ref<Scene>& pScene)
{
    mpScene = pScene;
    recreatePrograms();
}

void StochasticDepthStratfield::recreatePrograms()
{
    mStochasticDepthPass.pProgram = nullptr;
    mStochasticDepthPass.pVars = nullptr;
}
void StochasticDepthStratfield::parseProperties(const Properties& props)
{
    for (const auto& [key, value] : props)
    {
        if (key == kAlpha)
        {
            mCurrentState.alpha = value;
        }
        else if (key == kNumSamples)
        {
            mCurrentState.numSamples = value;
        }
    }
}

