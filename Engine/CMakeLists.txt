add_library(Engine STATIC)

target_sources(Engine
    PRIVATE
        src/Core/Application.cpp
        src/ECS/Registry.cpp
        src/ECS/System.cpp
)

target_include_directories(Engine
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(Engine
    PUBLIC
        glad
        glfw
        OpenGL::GL
)

target_compile_features(Engine PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(Engine PRIVATE /W4)
else()
    target_compile_options(Engine PRIVATE -Wall -Wextra -Wpedantic)
endif()
