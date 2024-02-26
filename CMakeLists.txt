cmake_minimum_required(VERSION 3.20)
project(Cen)

set(CMAKE_CXX_STANDARD 23)

#add_compile_options(-fsanitize=address)
#add_link_options(-fsanitize=address)

add_compile_definitions(CEN_SRC_DIR="${CMAKE_SOURCE_DIR}")

add_library(Cen
        src/Engine.cpp
        include/Cen/Engine.h
        src/Camera.cpp
        include/Cen/Camera.h
        src/Scene.cpp
        include/Cen/Scene.h
        src/Model.cpp
        include/Cen/Model.h
        src/AssetManager.cpp
        include/Cen/AssetManager.h
        src/ui/GuiWorkspace.cpp
        include/Cen/ui/GuiWorkspace.h
        include/Cen/ui/Window.h
        src/ui/SettingsWindow.cpp
        include/Cen/ui/SettingsWindow.h
        src/ui/StatisticsWindow.cpp
        include/Cen/ui/StatisticsWindow.h
        src/ui/SceneWindow.cpp
        include/Cen/ui/SceneWindow.h
        src/Transform.cpp
        include/Cen/Transform.h
        src/Renderer.cpp
        include/Cen/Renderer.h
        src/passes/MeshletDrawPass.cpp
        src/passes/MeshletDrawPass.h
        src/passes/MeshletsCullPass.cpp
        src/passes/MeshletsCullPass.h
        src/passes/DebugPasses.cpp
        src/passes/DebugPasses.h
        src/Material.cpp
        include/Cen/Material.h
        src/Light.cpp
        include/Cen/Light.h
        src/ui/ViewportWindow.cpp
        include/Cen/ui/ViewportWindow.h
        src/ui/ImageWidget.cpp
        include/Cen/ui/ImageWidget.h
        src/ui/RenderGraphWindow.cpp
        include/Cen/ui/RenderGraphWindow.h)

set(CANTA_ENABLE_EXAMPLES OFF)
set(CANTA_ENABLE_IMGUI ON)

add_subdirectory(third_party)
target_include_directories(Cen
    PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/include
    res/shaders
    PRIVATE
    src
    third_party
)
target_link_libraries(Cen Ende Canta fastgltf meshoptimizer stb_image)

add_subdirectory(examples)