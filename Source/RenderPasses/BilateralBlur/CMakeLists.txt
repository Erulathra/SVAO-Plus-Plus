add_plugin(BilateralBlur)

target_sources(BilateralBlur PRIVATE
    BilateralBlur.cpp
    BilateralBlur.h

    BilateralBlur.slang
)

target_copy_shaders(BilateralBlur RenderPasses/BilateralBlur)
target_source_group(BilateralBlur "RenderPasses")
