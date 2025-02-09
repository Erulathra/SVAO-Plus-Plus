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

#include "RenderGraph/RenderPass.h"
#include "VAOData.slang"

using namespace Falcor;

class VAOBase : public RenderPass {
public:
    explicit VAOBase(ref<Device> pDevice, const Properties& props);

protected:
    DefineList GetCommonDefines(const CompileData& compileData);

    void SetCommonVars(ShaderVar& vars, Scene* pScene);

public:
    void compile(RenderContext* pRenderContext, const CompileData& compileData) override;
    void execute(RenderContext* pRenderContext, const RenderData& renderData) override;
    void renderUI(Gui::Widgets& widget) override;

    Properties getProperties() const override;

    void setScene(RenderContext* pRenderContext, const ref<Scene>& pScene) override;

protected:
    uint2 getStochMapSize(uint2 fullRes, bool includeGuard = false) const;

protected:
    ref<Scene> mpScene;

    ref<Sampler> mpPointSampler;
    ref<Sampler> mpLinearSampler;

    ref<Texture> mpDitherTexture;

    VAOData mVaoData;

    bool mEnableGuardBand = true;
    bool mEnableAdaptiveSampling = false;
    bool mUseDitherTexture = true;

    int32_t mStochMapGuardBand = 512;
    uint mSDMapResolutionDivisor = 4;

    uint32_t mSampleCount = 8;
    int32_t mFrameIndex = 0;


private:
    static ref<Texture> generateDitherTexture(const ref<Device>& pDevice);

    int32_t getExtraGuardBand() const;
};
