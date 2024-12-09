add_plugin(NormalsToViewSpace)

target_sources(NormalsToViewSpace PRIVATE
    NormalsToViewSpace.cpp
    NormalsToViewSpace.h

    NormalsToViewSpace.slang
)

target_copy_shaders(NormalsToViewSpace RenderPasses/NormalsToViewSpace)
target_source_group(NormalsToViewSpace "RenderPasses")
