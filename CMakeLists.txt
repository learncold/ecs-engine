cmake_minimum_required(VERSION 3.20)

project(ECS_ENGINE
    VERSION 0.1.0
    DESCRIPTION "ECS-based 3D game engine research project"
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenGL REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)

add_subdirectory(third_party/glad)
add_subdirectory(Engine)
add_subdirectory(Sandbox)

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(Tests)
endif()
