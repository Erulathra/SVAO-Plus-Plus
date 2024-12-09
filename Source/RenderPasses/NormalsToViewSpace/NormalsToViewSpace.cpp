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
#include "NormalsToViewSpace.h"

#include "Core/Pass/FullScreenPass.h"

namespace
{
    const std::string kNormalsWorldIn = "normalsWorldIn";
    const std::string kNormalsViewOut = "normalsViewOut";

    const std::string kShaderFilePath = "RenderPasses/NormalsToViewSpace/NormalsToViewSpace.slang";
}

extern "C" FALCOR_API_EXPORT void registerPlugin(Falcor::PluginRegistry& registry)
{
    registry.registerClass<RenderPass, NormalsToViewSpace>();
}

NormalsToViewSpace::NormalsToViewSpace(ref<Device> pDevice, const Properties& props) : RenderPass(pDevice) {}

Properties NormalsToViewSpace::getProperties() const
{
    return {};
}

RenderPassReflection NormalsToViewSpace::reflect(const CompileData& compileData)
{
    // Define the required resources here
    RenderPassReflection reflector;
    reflector.addInput(kNormalsWorldIn, "Normals in World Space");
    reflector.addOutput(kNormalsViewOut, "Normals In View Space");
    return reflector;
}

void NormalsToViewSpace::compile(RenderContext* pRenderContext, const CompileData& compileData)
{
    mpPass = FullScreenPass::create(mpDevice, kShaderFilePath);
    mpFbo = Fbo::create(mpDevice);
}

void NormalsToViewSpace::execute(RenderContext* pRenderContext, const RenderData& renderData)
{
    if (!mpPass || !mpFbo || !mpScene)
    {
        return;
    }

    ref<Texture> pNormalsWorldIn = renderData.getTexture(kNormalsWorldIn);
    ref<Texture> pNormalsViewOut = renderData.getTexture(kNormalsViewOut);

    mpFbo->attachColorTarget(pNormalsViewOut, 0);

    ShaderVar vars = mpPass->getRootVar();
    vars["gNormalsWorldIn"] = pNormalsWorldIn;
    vars["PerFrameCB"]["gViewMat"] = mpScene->getCamera()->getViewMatrix();

    mpPass->execute(pRenderContext, mpFbo);
}

void NormalsToViewSpace::renderUI(Gui::Widgets& widget) {}
void NormalsToViewSpace::setScene(RenderContext* pRenderContext, const ref<Scene>& pScene)
{
    mpScene = pScene;
    requestRecompile();
}
