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

FetchContent_MakeAvailable(Ende Canta fastgltf meshopt)