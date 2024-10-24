add_plugin(SSAO)

target_sources(SSAO PRIVATE
    SSAO.cpp
    SSAO.h

    SSAO.slang
)

target_copy_shaders(SSAO RenderPasses/SSAO)
target_source_group(SSAO "RenderPasses")
