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
    const std::string kUseQuarterRes = "kUseQuarterRes";
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

    const uint2 SDFResolution = float2{compileData.defaultTexDims} * 0.25f;

    reflector.addOutput(kStochasticDepthName, "Stochastic Depth")
        .format(ResourceFormat::D32Float)
        .bindFlags(ResourceBindFlags::AllDepthViews)
        .texture2D(SDFResolution.x, SDFResolution.y, mCurrentState.numSamples);
        // .texture2D(0, 0, mCurrentState.numSamples);

    return reflector;
}
void StochasticDepthStratfield::compile(RenderContext* pRenderContext, const CompileData& compileData)
{
    static std::array<Fbo::SamplePosition, 16> samplePos = {};
    mpFbo->setSamplePositions(mCurrentState.numSamples, 1, samplePos.data());

    std::vector<int32_t> indices, lookUp;
    generateLookUpTable(indices, lookUp);

    mpStratifiedIndices = mpDevice->createStructuredBuffer(sizeof(int32_t), indices.size(), ResourceBindFlags::ShaderResource, MemoryType::DeviceLocal, indices.data(), false);
    mpStratifiedLookUpBuffer = mpDevice->createStructuredBuffer(sizeof(int32_t), lookUp.size(), ResourceBindFlags::ShaderResource, MemoryType::DeviceLocal, lookUp.data(), false);
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

        var["gNear"] = mpScene->getCamera()->getNearPlane();
        var["gFar"] = mpScene->getCamera()->getFarPlane();

        var["gStratifiedIndices"] = mpStratifiedIndices;
        var["gStratifiedLookUp"] = mpStratifiedLookUpBuffer;

        mpFbo->attachDepthStencilTarget(pStochasticDepth);
        mStochasticDepthPass.pState->setFbo(mpFbo);

        mpScene->rasterize(pRenderContext, mStochasticDepthPass.pState.get(), mStochasticDepthPass.pVars.get(), RasterizerState::CullMode::Back);
    }
}

void StochasticDepthStratfield::renderUI(Gui::Widgets& widget)
{
    if (widget.var("Alpha", mCurrentState.alpha, 0.1f, 1.f))
    {
        recreatePrograms();
    }

    if (widget.var("Num Samples", mCurrentState.numSamples, 4, 32))
    {
        recreatePrograms();
        requestRecompile();
    }
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

    mpStratifiedIndices = nullptr;
    mpStratifiedLookUpBuffer = nullptr;
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

// from https://www.geeksforgeeks.org/binomial-coefficient-dp-9/
static int32_t Binomial(int32_t n, int32_t k)
{
    std::vector<int32_t> C(k + 1);
    C[0] = 1;

    for (int32_t i = 1; i <= n; i++)
    {
        for (int32_t j = std::min(i, k); j > 0; j--)
            C[j] = C[j] + C[j - 1];
    }
    return C[k];
}

static uint8_t count_bits(uint32_t v)
{
    uint8_t bits = 0;
    for (; v; ++bits) { v &= v - 1; }
    return bits;
}

void StochasticDepthStratfield::generateLookUpTable(std::vector<int32_t>& indices, std::vector<int32_t>& lookUp) const
{
    uint32_t maxEntries = uint32_t(std::pow(2, mCurrentState.numSamples));
    indices.resize(mCurrentState.numSamples + 1);
    lookUp.resize(maxEntries);

    // Generate index list
    indices[0] = 0;
    for (int32_t i = 1; i <= mCurrentState.numSamples; i++) {
        indices[i] = Binomial(mCurrentState.numSamples, i - 1) + indices[i - 1];
    }

    // Generate lookup table
    std::vector<int32_t> currentIndices(indices);
    lookUp[0] = 0;
    for (uint32_t i = 1; i < maxEntries; i++) {
        int32_t popCount = count_bits(i);
        int32_t index = currentIndices[popCount];
        lookUp[index] = static_cast<int32_t>(i);
        currentIndices[popCount]++;
    }
}

