add_plugin(GBufferLite)

target_sources(GBufferLite PRIVATE
    GBufferLite.cpp
    GBufferLite.h
    
    # Shaders
    DepthPassLite.3d.slang
    GBufferHelpers.slang
    GBufferLite.3d.slang
)

target_copy_shaders(GBufferLite RenderPasses/GBufferLite)
target_source_group(GBufferLite "RenderPasses")
