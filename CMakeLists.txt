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
        include/Cen/Camera.h)

add_subdirectory(third_party)
target_include_directories(Cen
    PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/include
    res/shaders
    PRIVATE
    src
)
target_link_libraries(Cen Ende Canta fastgltf meshoptimizer)

add_subdirectory(examples)