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
#include "DepthBranchPass.h"

namespace
{
    const std::string kTextureOne = "textureOne";
    const std::string kTextureTwo = "textureTwo";

    const std::string kResult = "result";

    const std::string kPickFist = "pickFirst";
}

extern "C" FALCOR_API_EXPORT void registerPlugin(Falcor::PluginRegistry& registry)
{
    registry.registerClass<RenderPass, DepthBranchPass>();
}

DepthBranchPass::DepthBranchPass(ref<Device> pDevice, const Properties& props)
: RenderPass(pDevice)
{
    for (const auto& [key, value] : props)
    {
        if (key == kPickFist) mPickFirst = value;
    }
}

Properties DepthBranchPass::getProperties() const
{
    Properties properties;
    properties[kPickFist] = mPickFirst;
    return properties;
}

RenderPassReflection DepthBranchPass::reflect(const CompileData& compileData)
{
    RenderPassReflection reflector;
    reflector.addInput(kTextureOne, "Texture One")
        .format(ResourceFormat::R32Float);
    reflector.addInput(kTextureTwo, "Texture Two")
        .format(ResourceFormat::R32Float);

    reflector.addOutput(kResult, "Result Texture")
        .format(ResourceFormat::R32Float);

    return reflector;
}

void DepthBranchPass::execute(RenderContext* pRenderContext, const RenderData& renderData)
{
    ref<Texture> pTextureOne = renderData.getTexture(kTextureOne);
    ref<Texture> pTextureTwo = renderData.getTexture(kTextureTwo);

    ref<Texture> pResultTexture = renderData.getTexture(kResult);

    ref<Texture>& pSourceTexture = mPickFirst ? pTextureOne : pTextureTwo;
    if (pSourceTexture && pResultTexture)
    {
        pRenderContext->blit(pSourceTexture->getSRV(), pResultTexture->getRTV(), RenderContext::kMaxRect, RenderContext::kMaxRect);
    }
}

void DepthBranchPass::renderUI(Gui::Widgets& widget)
{
    widget.checkbox("Use fist texture", mPickFirst);
}

