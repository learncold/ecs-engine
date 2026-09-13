add_library(BoidsOOP STATIC
    boid_simulation.cpp
    boid_object.cpp
)

add_library(Boids::OOP ALIAS BoidsOOP)

target_include_directories(BoidsOOP
    PUBLIC
        ${PROJECT_SOURCE_DIR}
)

target_compile_features(BoidsOOP PUBLIC cxx_std_20)

target_link_libraries(BoidsOOP
    PUBLIC
        Boids::Common
        Engine::RenderData
)

if(MSVC)
    target_compile_options(BoidsOOP PRIVATE /W4)
else()
    target_compile_options(BoidsOOP PRIVATE -Wall -Wextra -Wpedantic)
endif()
