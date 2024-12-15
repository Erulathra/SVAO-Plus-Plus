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
#include "RTStochasticDepth.h"

#include <utility>

namespace
{
    const std::string kLinearDepthIn = "linearDepthIn";
    const std::string kRayMinIn = "rayMinIn";
    const std::string kRayMaxIn = "rayMaxIn";

    const std::string kStochasticDepthOut = "stochasticDepth";
    const int32_t kStochasticSampleCount = 4;

    const std::string kProgramRayShaderPath = "RenderPasses/RTStochasticDepth/StochasticDepthMapRT.rt.slang";

    const std::string kResolutionDivisor = "resolutionDivisor";
}

extern "C" FALCOR_API_EXPORT void registerPlugin(Falcor::PluginRegistry& registry)
{


    registry.registerClass<RenderPass, RTStochasticDepth>();
}

ref<RTStochasticDepth> RTStochasticDepth::create(ref<Device> pDevice, const Properties& props)
{
    return make_ref<RTStochasticDepth>(pDevice, props);
}

RTStochasticDepth::RTStochasticDepth(ref<Device> pDevice, const Properties& props)
: RenderPass(std::move(pDevice))
{
    // Parse dictionary.
    for (const auto& [key, value] : props)
    {
        if (key == kResolutionDivisor)
        {
            mResolutionDivisor = value;
        }
    }
}

Properties RTStochasticDepth::getProperties() const
{
    Properties props;
    props[kResolutionDivisor] = mResolutionDivisor;
    return props;
}

RenderPassReflection RTStochasticDepth::reflect(const CompileData& compileData)
{
    RenderPassReflection reflector;

    reflector.addInput(kLinearDepthIn, "linear depth (linear z values)").bindFlags(ResourceBindFlags::ShaderResource);

    uint2 depthMapResolution = uint2(compileData.defaultTexDims.x / mResolutionDivisor, compileData.defaultTexDims.y / mResolutionDivisor);
    reflector.addOutput(kStochasticDepthOut, "Stochastic depth (packed intro 4 color channels)")
        .bindFlags(ResourceBindFlags::AllColorViews)
        .format(ResourceFormat::RGBA32Float)
        .texture2D(depthMapResolution.x, depthMapResolution.y);

    return reflector;
}

void RTStochasticDepth::execute(RenderContext* pRenderContext, const RenderData& renderData)
{
    if (!mpScene)
    {
        return;
    }

    ref<Texture> pLinearDepthIn = renderData.getTexture(kLinearDepthIn);
    ref<Texture> pStochasticDepthOut = renderData.getTexture(kStochasticDepthOut);

    if (!pLinearDepthIn || !pStochasticDepthOut)
    {
        return;
    }

    if (!mRtProgram.pProgram)
    {
        DefineList defines = mpScene->getSceneDefines();
        const float rayConeSpread = mpScene->getCamera()->computeScreenSpacePixelSpreadAngle(renderData.getDefaultTextureDims().y);
        defines.add("RAY_CONE_SPREAD", std::to_string(rayConeSpread));
        ProgramDesc desc;
        desc.addShaderModules(mpScene->getShaderModules());
        desc.addShaderLibrary(kProgramRayShaderPath);
        desc.setMaxPayloadSize((kStochasticSampleCount + 1) * sizeof(float));
        desc.setMaxAttributeSize(mpScene->getRaytracingMaxAttributeSize());
        desc.setMaxTraceRecursionDepth(1);
        desc.addTypeConformances(mpScene->getTypeConformances());
        desc.setShaderModel(ShaderModel::SM6_5);

        ref<RtBindingTable> sbt = RtBindingTable::create(1, 1, mpScene->getGeometryCount());
        sbt->setRayGen(desc.addRayGen("rayGen"));
        sbt->setMiss(0, desc.addMiss("miss"));
        sbt->setHitGroup(0, mpScene->getGeometryIDs(GeometryType::TriangleMesh), desc.addHitGroup("closestHit", "anyHit"));
        mRtProgram.pBidingTable = sbt;

        mRtProgram.pProgram = Program::create(mpDevice, desc, defines);
        mRtProgram.pVars = RtProgramVars::create(mpDevice, mRtProgram.pProgram, sbt);
    }

    ShaderVar pVars = mRtProgram.pVars->getRootVar();
    pVars["gLinearDepthIn"] = pLinearDepthIn;
    pVars["gStochasticDepthOut"] = pStochasticDepthOut;

    mpScene->raytrace(pRenderContext, mRtProgram.pProgram.get(), mRtProgram.pVars, uint3{pStochasticDepthOut->getWidth(), pStochasticDepthOut->getHeight(), 1});
}

void RTStochasticDepth::renderUI(Gui::Widgets& widget)
{
    const static Gui::DropdownList kResolutionDivisorDropdownList = {
        {1, "1"},
        {2, "2"},
        {3, "3"},
        {4, "4"}
    };

    bool bRequestRecompile = false;

    bRequestRecompile |= widget.dropdown("ResolutionDivisor", kResolutionDivisorDropdownList, mResolutionDivisor);

    if (bRequestRecompile)
    {
        requestRecompile();
    }
}

void RTStochasticDepth::setScene(RenderContext* pRenderContext, const ref<Scene>& pScene)
{
    mpScene = pScene;
    mRtProgram.pProgram.reset();
}
