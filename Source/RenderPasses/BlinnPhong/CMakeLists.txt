add_plugin(BlinnPhong)

target_sources(BlinnPhong PRIVATE
    BlinnPhong.cpp
    BlinnPhong.h
    BlinnPhong.slang
)

target_copy_shaders(BlinnPhong RenderPasses/BlinnPhong)

target_source_group(BlinnPhong "RenderPasses")
