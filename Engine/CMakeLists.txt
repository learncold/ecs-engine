function(configure_engine_target target_name)
    target_include_directories(${target_name}
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    target_compile_features(${target_name} PUBLIC cxx_std_20)

    if(MSVC)
        target_compile_options(${target_name} PRIVATE /W4)
    else()
        target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

add_library(EngineCore STATIC
    src/Core/application.cpp
)

add_library(Engine::Core ALIAS EngineCore)

target_link_libraries(EngineCore
    PUBLIC
        glad
        glfw
        OpenGL::GL
)

configure_engine_target(EngineCore)

add_library(EngineRenderer STATIC
    src/Renderer/agent_renderer.cpp
    src/Renderer/camera.cpp
    src/Renderer/line_renderer.cpp
)

add_library(Engine::Renderer ALIAS EngineRenderer)

target_link_libraries(EngineRenderer
    PUBLIC
        glm::glm
    PRIVATE
        glad
        OpenGL::GL
)

configure_engine_target(EngineRenderer)

add_library(EngineECS STATIC
    src/ECS/registry.cpp
    src/ECS/system.cpp
)

add_library(Engine::ECS ALIAS EngineECS)

configure_engine_target(EngineECS)
