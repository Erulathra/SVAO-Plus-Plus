add_plugin(Branch)

target_sources(Branch PRIVATE
        DepthBranchPass.cpp
        DepthBranchPass.h
)

target_copy_shaders(Branch RenderPasses/Branch)
target_source_group(Branch "RenderPasses")
