function(configure_sandbox_target target_name)
    target_compile_features(${target_name} PRIVATE cxx_std_20)

    if(MSVC)
        target_compile_options(${target_name} PRIVATE /W4)
    else()
        target_compile_options(
            ${target_name}
            PRIVATE
                -Wall
                -Wextra
                -Wpedantic
        )
    endif()
endfunction()

add_library(SandboxBoidsECS STATIC
    include/Sandbox/Boids/ECS/boid_simulation.h
    include/Sandbox/Boids/ECS/Components/transform.h
    include/Sandbox/Boids/ECS/Components/velocity.h
    include/Sandbox/Boids/ECS/Systems/boundary_system.h
    include/Sandbox/Boids/ECS/Systems/movement_system.h
    include/Sandbox/Boids/ECS/Systems/boid_system.h
    src/Boids/ECS/boid_simulation.cpp
    src/Boids/ECS/Systems/boundary_system.cpp
    src/Boids/ECS/Systems/movement_system.cpp
    src/Boids/ECS/Systems/boid_system.cpp
)

add_library(Sandbox::BoidsECS ALIAS SandboxBoidsECS)

target_include_directories(SandboxBoidsECS
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(SandboxBoidsECS
    PUBLIC
        Engine::ECS
        glm::glm
)

configure_sandbox_target(SandboxBoidsECS)

add_library(SandboxBoidsRenderer STATIC
    include/Sandbox/Boids/Rendering/boid_renderer.h
    src/Boids/Rendering/boid_renderer.cpp
)

add_library(Sandbox::BoidsRenderer ALIAS SandboxBoidsRenderer)

target_include_directories(SandboxBoidsRenderer
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(SandboxBoidsRenderer
    PUBLIC
        Engine::Renderer
        Sandbox::BoidsECS
)

configure_sandbox_target(SandboxBoidsRenderer)

add_executable(Sandbox
    src/main.cpp
)

target_link_libraries(Sandbox
    PRIVATE
        Engine::Core
        Sandbox::BoidsECS
        Sandbox::BoidsRenderer
)

configure_sandbox_target(Sandbox)
