add_plugin(StochasticDepthStratfield)

target_sources(StochasticDepthStratfield PRIVATE
    StochasticDepthStratfield.cpp
    StochasticDepthStratfield.h

    StochasticDepthStratfieldPass.3d.slang
)

target_copy_shaders(StochasticDepthStratfield RenderPasses/StochasticDepthStratfield)
target_source_group(StochasticDepthStratfield "RenderPasses")
