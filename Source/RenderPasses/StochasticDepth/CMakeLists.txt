add_plugin(StochasticDepthNaive)

target_sources(StochasticDepthNaive PRIVATE
    StochasticDepthNaive.cpp
    StochasticDepthNaive.h

    StochasticDepthNaivePass.3d.slang
)

target_copy_shaders(StochasticDepthNaive RenderPasses/StochasticDepthNaive)
target_source_group(StochasticDepthNaive "RenderPasses")
