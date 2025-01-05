add_plugin(DeferredLighting)

target_sources(DeferredLighting PRIVATE
    DeferredLighting.cpp
    DeferredLighting.h

    DeferredLighting.slang
)

target_copy_shaders(DeferredLighting RenderPasses/DeferredLighting)
target_source_group(DeferredLighting "RenderPasses")
