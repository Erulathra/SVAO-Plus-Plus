add_plugin(RTStochasticDepth)

target_sources(RTStochasticDepth PRIVATE
    RTStochasticDepth.cpp
    RTStochasticDepth.h

    Common.slangh
    Stencil.ps.slang
    StochasticDepthMapRT.rt.slang
)

target_copy_shaders(RTStochasticDepth RenderPasses/RTStochasticDepth)
target_source_group(RTStochasticDepth "RenderPasses")
