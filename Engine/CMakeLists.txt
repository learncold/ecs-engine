function(configure_engine_target target_name)
    target_include_directories(${target_name}
        PUBLIC
            ${PROJECT_SOURCE_DIR}
    )

    target_compile_features(${target_name} PUBLIC cxx_std_20)

    if(MSVC)
        target_compile_options(${target_name} PRIVATE /W4)
    else()
        target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

add_library(EngineBenchmark STATIC
    Benchmark/benchmarker.cpp
)
add_library(Engine::Benchmark ALIAS EngineBenchmark)
configure_engine_target(EngineBenchmark)

add_library(EngineInput STATIC
    Input/input.cpp
)

add_library(Engine::Input ALIAS EngineInput)

target_link_libraries(EngineInput
    PUBLIC
        glfw
)

configure_engine_target(EngineInput)

add_library(EngineDebugUI STATIC
    DebugUI/debug_ui.cpp
)

add_library(Engine::DebugUI ALIAS EngineDebugUI)

target_link_libraries(EngineDebugUI
    PRIVATE
        glfw
        imgui::imgui
        OpenGL::GL
)

configure_engine_target(EngineDebugUI)

add_library(EngineCore STATIC
    Core/application.cpp
)

add_library(Engine::Core ALIAS EngineCore)

target_link_libraries(EngineCore
    PUBLIC
        glad
        glfw
        OpenGL::GL
        Engine::DebugUI
        Engine::Input
)

configure_engine_target(EngineCore)

add_library(EngineRenderer STATIC
    Renderer/agent_renderer.cpp
    Renderer/agent_render_manager.cpp
    Renderer/camera.cpp
    Renderer/line_renderer.cpp
    Renderer/orbit_camera_controller.cpp
)

add_library(Engine::Renderer ALIAS EngineRenderer)

target_link_libraries(EngineRenderer
    PUBLIC
        Engine::Input
        Engine::RenderData
        glm::glm
    PRIVATE
        glad
        OpenGL::GL
)

configure_engine_target(EngineRenderer)

add_library(EngineRenderData STATIC
    Renderer/transform.cpp
)

add_library(Engine::RenderData ALIAS EngineRenderData)

target_link_libraries(EngineRenderData
    PUBLIC
        glm::glm
)

configure_engine_target(EngineRenderData)

add_library(EngineECS STATIC
    ECS/registry.cpp
    ECS/system.cpp
)

add_library(Engine::ECS ALIAS EngineECS)

configure_engine_target(EngineECS)

add_library(EngineECSRenderer STATIC
    ECSRenderer/ecs_render_system.cpp
)

add_library(Engine::ECSRenderer ALIAS EngineECSRenderer)

target_link_libraries(EngineECSRenderer
    PUBLIC
        Engine::ECS
        Engine::Renderer
        Engine::RenderData
)

configure_engine_target(EngineECSRenderer)
