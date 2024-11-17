add_plugin(NNAO)

target_sources(NNAO PRIVATE
    NNAO.cpp
    NNAO.h

    NNAO.slang
)

target_copy_shaders(NNAO RenderPasses/NNAO)
target_source_group(NNAO "RenderPasses")
