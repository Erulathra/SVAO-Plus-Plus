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
#pragma once

#include "Falcor.h"
#include "VAOBase.h"
#include "RenderGraph/RenderPass.h"

using namespace Falcor;

enum class VAOPrepassSamplingMode : uint32_t
{
    Griddy = 0,
    Careful
};

FALCOR_ENUM_INFO(
    VAOPrepassSamplingMode,
    {
        {VAOPrepassSamplingMode::Griddy, "Griddy"},
        {VAOPrepassSamplingMode::Careful, "Careful"}
    }
);

class VAO : public VAOBase
{
public:
    FALCOR_PLUGIN_CLASS(VAO, "VAO", "Volumetric Ambient Occasion");

    VAO(ref<Device> pDevice, const Properties& props);
    static ref<VAO> create(ref<Device> pDevice, const Properties& props);

    virtual Properties getProperties() const override;
    virtual RenderPassReflection reflect(const CompileData& compileData) override;
    virtual void compile(RenderContext* pRenderContext, const CompileData& compileData) override;
    virtual void execute(RenderContext* pRenderContext, const RenderData& renderData) override;
    virtual void renderUI(Gui::Widgets& widget) override;

private:
    void ApplyProperties(const Properties& props);

private:
    ref<ComputePass> mpComputePass;

    bool mSVAOInputMode = false;
    bool mUseRayInterval = false;
    bool mUsePrepass = false;
    bool mPrepass = false;
    VAOPrepassSamplingMode mPrepassSamplingMode = VAOPrepassSamplingMode::Careful;
    bool mDebugPrepass = false;
};

FALCOR_ENUM_REGISTER(VAOPrepassSamplingMode)

