cmake_minimum_required(VERSION 3.20)
project(Cen)

set(CMAKE_CXX_STANDARD 23)

add_library(Cen
        src/Engine.cpp
        include/Cen/Engine.h)

add_subdirectory(third_party)
target_include_directories(Cen
    PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/include
    PRIVATE
    src
)
target_link_libraries(Cen Ende Canta)

add_subdirectory(examples)