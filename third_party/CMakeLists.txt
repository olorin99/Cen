include(FetchContent)


FetchContent_Declare(
        Ende
        GIT_REPOSITORY https://github.com/olorin99/Ende
        GIT_TAG        "master"
)

FetchContent_Declare(
        Canta
        GIT_REPOSITORY  https://github.com/olorin99/Canta
        GIT_TAG         "main"
)

FetchContent_Declare(
        fastgltf
        GIT_REPOSITORY  https://github.com/spnda/fastgltf
        GIT_TAG         73fa4209b883f4614fda2f516a53ebc1c53aec70
)

FetchContent_Declare(
        meshopt
        GIT_REPOSITORY  https://github.com/zeux/meshoptimizer
        GIT_TAG         e47e1be6d3d9513153188216455bdbed40a206ef
)

option(KTX_FEATURE_TESTS "" OFF)
option(KTX_FEATURE_VULKAN "" OFF)
option(KTX_FEATURE_GL_UPLOAD "" OFF)
option(KTX_FEATURE_VK_UPLOAD "" OFF)
option(KTX_FEATURE_WRITE "" OFF)
option(KTX_FEATURE_TOOLS "" OFF)
option(KTX_FEATURE_STATIC_LIBRARY "" ON)
FetchContent_Declare(
        ktx
        GIT_REPOSITORY https://github.com/KhronosGroup/KTX-Software.git
        GIT_TAG        v4.3.1
)

FetchContent_MakeAvailable(Ende Canta fastgltf meshopt ktx)

add_library(stb_image STATIC stb_image.h stb_image_write.h stb_image.cpp)
add_library(implot STATIC implot.h implot.cpp implot_internal.h implot_items.cpp)
target_link_libraries(implot imgui)