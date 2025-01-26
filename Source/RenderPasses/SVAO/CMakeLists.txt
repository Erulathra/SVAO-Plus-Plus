add_plugin(SVAO)

target_sources(SVAO PRIVATE
    VAOBase.cpp
    VAOBase.h

    VAO/VAO.cpp
    VAO/VAO.h
        VAO/VAO.ps.slang

    VAOPRepass/VAOPrepass.cpp
    VAOPRepass/VAOPrepass.h
    VAOPRepass/VAOPrepass.ps.slang

    SVAO/SVAO.cpp
    SVAO/SVAO.h
    SVAO/SVAORaster2.ps.slang

    Common.slang
    VAOData.slang
)

target_include_directories(SVAO PRIVATE
    SVAO
    VAO
    VAOPrepass
    .
)

target_copy_shaders(SVAO RenderPasses/SVAO)
target_source_group(SVAO "RenderPasses")
