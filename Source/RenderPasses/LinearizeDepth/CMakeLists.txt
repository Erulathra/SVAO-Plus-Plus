add_plugin(LinearizeDepth)

target_sources(LinearizeDepth PRIVATE
    LinearizeDepth.cpp
    LinearizeDepth.h

    LinearizeDepth.slang
)

target_copy_shaders(LinearizeDepth RenderPasses/LinearizeDepth)
target_source_group(LinearizeDepth "RenderPasses")
