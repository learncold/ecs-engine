add_library(BoidsECS STATIC
    boid_simulation.cpp
    Systems/boid_orientation_system.cpp
    Systems/boid_system.cpp
    Systems/boundary_system.cpp
    Systems/movement_system.cpp
)

add_library(Boids::ECS ALIAS BoidsECS)

target_include_directories(BoidsECS
    PUBLIC
        ${PROJECT_SOURCE_DIR}
)

target_compile_features(BoidsECS PUBLIC cxx_std_20)

target_link_libraries(BoidsECS
    PUBLIC
        Boids::Common
        Engine::ECS
        Engine::RenderData
        glm::glm
)

if(MSVC)
    target_compile_options(BoidsECS PRIVATE /W4)
else()
    target_compile_options(BoidsECS PRIVATE -Wall -Wextra -Wpedantic)
endif()
