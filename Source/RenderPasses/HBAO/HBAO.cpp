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
#include "HBAO.h"
#include <random>

namespace
{
    const std::string kDepth = "depth";
    const std::string kNormal = "normals";

    const std::string kAmbientMap = "ambientMap";

    const std::string kProgram = "RenderPasses/HBAO/HBAO.ps.slang";

    const std::string kRadius = "radius";
    const std::string kDepthBias = "depthBias";
    const std::string kExponent = "exponent";
}

extern "C" FALCOR_API_EXPORT void registerPlugin(Falcor::PluginRegistry& registry)
{
    registry.registerClass<RenderPass, HBAO>();
}

ref<HBAO> HBAO::create(ref<Device> pDevice, const Properties& props)
{
    auto pass = make_ref<HBAO>(pDevice, props);
    for (const auto& [key, value] : props)
    {
        if (key == kRadius) pass->mData.radius = value;
        else if (key == kDepthBias) pass->mData.NdotVBias = value;
        else if (key == kExponent) pass->mData.powerExponent = value;
    }
    return pass;
}

HBAO::HBAO(ref<Device> pDevice, const Properties& props)
    : RenderPass(pDevice)
{
    mpFbo = Fbo::create(mpDevice);
    mpPass = FullScreenPass::create(mpDevice, kProgram);

    Sampler::Desc samplerDesc;
    samplerDesc.setFilterMode(TextureFilteringMode::Point, TextureFilteringMode::Point, TextureFilteringMode::Point);
    samplerDesc.setAddressingMode(TextureAddressingMode::Clamp, TextureAddressingMode::Clamp, TextureAddressingMode::Clamp);
    mpTextureSampler = mpDevice->createSampler(samplerDesc);

    setRadius(mData.radius);

    Sampler::Desc noiseSamplerDesc;
    samplerDesc.setFilterMode(TextureFilteringMode::Point, TextureFilteringMode::Point, TextureFilteringMode::Point);
    samplerDesc.setAddressingMode(TextureAddressingMode::Wrap, TextureAddressingMode::Wrap, TextureAddressingMode::Wrap);

    mpNoiseSampler = mpDevice->createSampler(noiseSamplerDesc);
    mpNoiseTexture = genNoiseTexture(pDevice);
}

Properties HBAO::getProperties() const
{
    Properties d;
    d[kRadius] = mData.radius;
    d[kDepthBias] = mData.NdotVBias;
    d[kExponent] = mData.powerExponent;
    return d;
}

RenderPassReflection HBAO::reflect(const CompileData& compileData)
{
    // Define the required resources here
    RenderPassReflection reflector;
    reflector.addInput(kDepth, "linear-depth")
        .bindFlags(ResourceBindFlags::ShaderResource);
    reflector.addInput(kNormal, "normals")
        .bindFlags(ResourceBindFlags::ShaderResource);

    reflector.addOutput(kAmbientMap, "ambient occlusion")
        .bindFlags(ResourceBindFlags::RenderTarget | ResourceBindFlags::ShaderResource)
        .format(ResourceFormat::R8Unorm);

    return reflector;
}

void HBAO::compile(RenderContext* pRenderContext, const CompileData& compileData)
{
    mDirty = true;
}

void HBAO::execute(RenderContext* pRenderContext, const RenderData& renderData)
{
    if (!mpScene)
    {
        return;
    }

    auto pDepthIn = renderData.getTexture(kDepth);
    auto pNormal = renderData.getTexture(kNormal);
    auto pAmbientOut = renderData.getTexture(kAmbientMap);

    if (!mEnabled)
    {
        // clear and return
        pRenderContext->clearTexture(pAmbientOut.get(), float4(1.0f));
        return;
    }

    auto vars = mpPass->getRootVar();

    if (mDirty)
    {
        // static data
        mData.resolution = float2(renderData.getDefaultTextureDims().x, renderData.getDefaultTextureDims().y);
        mData.invResolution = float2(1.0f) / mData.resolution;
        mData.noiseScale = mData.resolution / 4.0f; // noise texture is 4x4 resolution
        vars["StaticCB"].setBlob(mData);

        vars["gTextureSampler"] = mpTextureSampler;
        vars["gNoiseSampler"] = mpNoiseSampler;
        mDirty = false;
    }

    Camera* pCamera = mpScene->getCamera().get();
    pCamera->bindShaderData(vars["PerFrameCB"]["gCamera"]);
    vars["gNormalTex"] = pNormal;
    vars["gDepthTex"] = pDepthIn;
    vars["gNoiseTex"] = mpNoiseTexture;

    mpFbo->attachColorTarget(pAmbientOut, 0);
    mpPass->execute(pRenderContext, mpFbo);
}

void HBAO::renderUI(Gui::Widgets& widget)
{
    widget.checkbox("Enabled", mEnabled);
    if (!mEnabled)
    {
        return;
    }

    float radius = mData.radius;
    if (widget.var("Radius", radius, 0.01f, FLT_MAX, 0.01f))
    {
        setRadius(radius);
    }

    if (widget.slider("Depth Bias", mData.NdotVBias, 0.0f, 0.5f)) mDirty = true;
    if (widget.slider("Power Exponent", mData.powerExponent, 1.0f, 20.0f)) mDirty = true;
}

void HBAO::setRadius(float r)
{
    mData.radius = r;
    mData.negInvRsq = -1.0f / (r * r);
    mDirty = true;
}

ref<Texture> HBAO::genNoiseTexture(ref<Device> pDevice)
{
    std::vector<float4> data;
    data.resize(4u * 4u);

    // https://en.wikipedia.org/wiki/Ordered_dithering
    //const float ditherValues[] = { 0.0f, 8.0f, 2.0f, 10.0f, 12.0f, 4.0f, 14.0f, 6.0f, 3.0f, 11.0f, 1.0f, 9.0f, 15.0f, 7.0f, 13.0f, 5.0f };

    auto linearRand = [](float min, float max)
    {
        static std::mt19937 generator(0);
        std::uniform_real_distribution<float> distribution(min, max);
        return distribution(generator);
    };

    for (uint32_t i = 0; i < data.size(); i++)
    {
        // Random directions on the XY plane
        auto theta = linearRand(0.0f, 2.0f * 3.141f);
        //auto theta = ditherValues[i] / 16.0f * 2.0f * glm::pi<float>();
        auto r1 = linearRand(0.0f, 1.0f);
        auto r2 = linearRand(0.0f, 1.0f);
        data[i] = float4(sin(theta), cos(theta), r1, r2);
    }

    return pDevice->createTexture2D(4, 4, ResourceFormat::RGBA32Float, 1, 1, data.data());
}
