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
#include "GBufferLite.h"

#include "Utils/SampleGenerators/DxSamplePattern.h"
#include "Utils/SampleGenerators/HaltonSamplePattern.h"
#include "Utils/SampleGenerators/StratifiedSamplePattern.h"

namespace
{
    const std::string kDepthPassProgramFile = "RenderPasses/GBufferLite/DepthPassLite.3d.slang";
    const std::string kGBufferPassProgramFile = "RenderPasses/GBufferLite/GBufferLite.3d.slang";

    const std::string kJitterSamplePattern = "jitterSamplePattern";
    const std::string kJitterSamplesCount = "jitterSamplesCount";

    const RasterizerState::CullMode kDefaultCullMode = RasterizerState::CullMode::Back;
    
    const std::string kDepthName = "depth";
}

const ChannelList GBufferLite::kGBufferChannels = {
    {"posW", "gPosW", "Position in world space", true, ResourceFormat::RGBA32Float},
    {"normW", "gNormW", "Shading normal in world space", true, ResourceFormat::RGBA32Float},
    {"faceNormW", "gFaceNormW", "Face normal in world space", true, ResourceFormat::RGBA32Float},
    {"diffuseOpacity", "gDiffOpacity", "Diffuse reflection albedo and opacity", true, ResourceFormat::RGBA32Float},
    {"specRough", "gSpecRough", "Specular reflectance and roughness", true, ResourceFormat::RGBA32Float},
    {"linearDepth", "gLinearDepth", "LinearDepth", true, ResourceFormat::R16Float},
    {"motionVector", "gMotionVector", "MotionVectors", true, ResourceFormat::R32Float},
};

extern "C" FALCOR_API_EXPORT void registerPlugin(Falcor::PluginRegistry& registry)
{
    registry.registerClass<RenderPass, GBufferLite>();
}

GBufferLite::GBufferLite(ref<Device> pDevice, const Properties& props) : RenderPass(pDevice)
{
    if (!mpDevice->isShaderModelSupported(ShaderModel::SM6_0))
    {
        FALCOR_THROW("GBufferLite requires SM6 or greater");
    }

    mDepthPass.pState = GraphicsState::create(mpDevice);
    mGBufferPass.pState = GraphicsState::create(mpDevice);

    DepthStencilState::Desc depthStencilDesc;
    depthStencilDesc.setDepthFunc(ComparisonFunc::Equal).setDepthWriteMask(false);
    ref<DepthStencilState> depthStencilState = DepthStencilState::create(depthStencilDesc);
    mGBufferPass.pState->setDepthStencilState(depthStencilState);

    mpFbo = Fbo::create(mpDevice);

    for (const auto& [name, value] : props)
    {
        if (name == kJitterSamplePattern) mJitterSamplePattern = value;
        else if (name == kJitterSamplesCount) mJitterSamplesCount = value;
    }
}

Properties GBufferLite::getProperties() const
{
    Properties props;
    props[kJitterSamplePattern] = mJitterSamplePattern;
    props[kJitterSamplesCount] = mJitterSamplesCount;
    return props;
}

RenderPassReflection GBufferLite::reflect(const CompileData& compileData)
{
    RenderPassReflection reflector;

    reflector.addOutput(kDepthName, "DepthBuffer").format(ResourceFormat::D32Float).bindFlags(ResourceBindFlags::DepthStencil).texture2D();

    addRenderPassOutputs(reflector, kGBufferChannels, ResourceBindFlags::RenderTarget);

    return reflector;
}

void GBufferLite::compile(RenderContext* pRenderContext, const CompileData& compileData)
{
    mFrameDim = compileData.defaultTexDims;
    mInvFrameDim = 1.f / float2(mFrameDim);
}

void GBufferLite::execute(RenderContext* pRenderContext, const RenderData& renderData)
{
    ref<Texture> pDepth = renderData.getTexture(kDepthName);
    FALCOR_ASSERT(pDepth);

    pRenderContext->clearDsv(pDepth->getDSV().get(), 1.f, 0);

    for (uint32_t channelID = 0; channelID < kGBufferChannels.size(); ++channelID)
    {
        ref<Texture> pTexture = renderData.getTexture(kGBufferChannels[channelID].name);
        mpFbo->attachColorTarget(pTexture, channelID);
    }
    pRenderContext->clearFbo(mpFbo.get(), float4{0.f}, 1.f, 0.f, FboAttachmentType::Color);

    if (mpScene == nullptr)
    {
        return;
    }

    if (is_set(mpScene->getUpdates(), IScene::UpdateFlags::RecompileNeeded))
    {
        recreatePrograms();
    }
    
    // Depth pass
    {
        // Create depth pass program.
        if (!mDepthPass.pProgram)
        {
            ProgramDesc desc;
            desc.addShaderModules(mpScene->getShaderModules());
            desc.addShaderLibrary(kDepthPassProgramFile).vsEntry("vsMain").psEntry("psMain");
            desc.addTypeConformances(mpScene->getTypeConformances());
    
            mDepthPass.pProgram = Program::create(mpDevice, desc, mpScene->getSceneDefines());
            mDepthPass.pState->setProgram(mDepthPass.pProgram);
        }
    
        // Create program vars.
        if (!mDepthPass.pVars)
        {
            mDepthPass.pVars = ProgramVars::create(mpDevice, mDepthPass.pProgram.get());
        }
    
        mpFbo->attachDepthStencilTarget(pDepth);
        mDepthPass.pState->setFbo(mpFbo);
    
        mpScene->rasterize(pRenderContext, mDepthPass.pState.get(), mDepthPass.pVars.get(), kDefaultCullMode);
    }
    
    // GBuffer pass.
    {
        // Create GBuffer pass program.
        if (!mGBufferPass.pProgram)
        {
            ProgramDesc desc;
            desc.addShaderModules(mpScene->getShaderModules());
            desc.addShaderLibrary(kGBufferPassProgramFile).vsEntry("vsMain").psEntry("psMain");
            desc.addTypeConformances(mpScene->getTypeConformances());

            mGBufferPass.pProgram = Program::create(mpDevice, desc, mpScene->getSceneDefines());
            mGBufferPass.pState->setProgram(mGBufferPass.pProgram);
        }

        mGBufferPass.pProgram->addDefines(getValidResourceDefines(kGBufferChannels, renderData));

        // Create program vars.
        if (!mGBufferPass.pVars)
        {
            mGBufferPass.pVars = ProgramVars::create(mpDevice, mGBufferPass.pProgram.get());
        }

        mGBufferPass.pVars->getRootVar()["PerFrameCB"]["gFrameDim"] = mFrameDim;

        mpScene->bindShaderData(mGBufferPass.pVars->getRootVar()["gScene"]);

        mGBufferPass.pState->setFbo(mpFbo); // Sets the viewport

        // Rasterize the scene.
        mpScene->rasterize(pRenderContext, mGBufferPass.pState.get(), mGBufferPass.pVars.get(), kDefaultCullMode);
    }
}

void GBufferLite::renderUI(Gui::Widgets& widget)
{
    bool bUpdateJitterPattern = widget.dropdown("Jitter Pattern", mJitterSamplePattern);

    if (bUpdateJitterPattern)
    {
        updateSamplePattern();
    }
}

void GBufferLite::setScene(RenderContext* pRenderContext, const ref<Scene>& pScene)
{
    mpScene = pScene;

    recreatePrograms();
    updateSamplePattern();
}


void GBufferLite::recreatePrograms()
{
    mDepthPass.pProgram = nullptr;
    mDepthPass.pVars = nullptr;
    mGBufferPass.pProgram = nullptr;
    mGBufferPass.pVars = nullptr;

}

void GBufferLite::updateSamplePattern()
{
    if (mpScene)
    {
        mpCPUJitterSampleGenerator = createSampleGenerator();
        mpScene->getCamera()->setPatternGenerator(mpCPUJitterSampleGenerator, mInvFrameDim);
    }
}

ref<CPUSampleGenerator> GBufferLite::createSampleGenerator() const
{
    switch (mJitterSamplePattern)
    {
    case JitterSamplePattern::Center:
        return nullptr;
    case JitterSamplePattern::DirectX:
        return DxSamplePattern::create(mJitterSamplesCount);
    case JitterSamplePattern::Halton:
        return HaltonSamplePattern::create(mJitterSamplesCount);
    case JitterSamplePattern::Stratified:
        return StratifiedSamplePattern::create(mJitterSamplesCount);
    }

    return nullptr;
}
