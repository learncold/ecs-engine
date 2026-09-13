find_package(GTest CONFIG REQUIRED)

include(GoogleTest)

function(configure_test_target target_name)
    target_compile_features(${target_name} PRIVATE cxx_std_20)

    if(MSVC)
        target_compile_options(${target_name} PRIVATE /W4)
    else()
        target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

add_executable(EngineBenchmarkTests
    unit/benchmarker_tests.cpp
)
target_link_libraries(EngineBenchmarkTests PRIVATE Engine::Benchmark GTest::gtest_main)
configure_test_target(EngineBenchmarkTests)
gtest_discover_tests(EngineBenchmarkTests)

add_executable(AppCommandLineOptionsTests
    unit/command_line_options_tests.cpp
    ${PROJECT_SOURCE_DIR}/App/command_line_options.cpp
)
target_link_libraries(AppCommandLineOptionsTests
    PRIVATE Engine::Benchmark Boids::Common GTest::gtest_main
)
configure_test_target(AppCommandLineOptionsTests)
gtest_discover_tests(AppCommandLineOptionsTests)

add_test(NAME AppBenchmarkSmoke COMMAND App --benchmark --agents 10 --warmup 1 --steps 3)
set_tests_properties(AppBenchmarkSmoke PROPERTIES
    PASS_REGULAR_EXPRESSION "Measured steps: 3" TIMEOUT 30
)

add_executable(EngineEcsTests
    unit/registry_tests.cpp
    unit/view_tests.cpp
)

target_link_libraries(EngineEcsTests
    PRIVATE
        Engine::ECS
        GTest::gtest_main
)

configure_test_target(EngineEcsTests)
gtest_discover_tests(EngineEcsTests)

add_executable(EngineRenderDataTests
    unit/transform_tests.cpp
)

target_link_libraries(EngineRenderDataTests
    PRIVATE
        Engine::RenderData
        GTest::gtest_main
)

configure_test_target(EngineRenderDataTests)
gtest_discover_tests(EngineRenderDataTests)

add_executable(EngineRenderingTests
    unit/ecs_render_system_tests.cpp
    unit/orbit_camera_controller_tests.cpp
)

target_link_libraries(EngineRenderingTests
    PRIVATE
        Engine::ECSRenderer
        GTest::gtest_main
)

configure_test_target(EngineRenderingTests)
gtest_discover_tests(EngineRenderingTests)

add_executable(BoidsEcsTests
    integration/boid_orientation_system_tests.cpp
    integration/boid_system_tests.cpp
    integration/boid_simulation_tests.cpp
    integration/movement_system_tests.cpp
)

target_link_libraries(BoidsEcsTests
    PRIVATE
        Boids::ECS
        GTest::gtest_main
)

configure_test_target(BoidsEcsTests)
gtest_discover_tests(BoidsEcsTests)

add_executable(BoidsComparisonTests
    integration/boid_equivalence_tests.cpp
)
target_link_libraries(BoidsComparisonTests
    PRIVATE Boids::ECS Boids::OOP GTest::gtest_main
)
configure_test_target(BoidsComparisonTests)
gtest_discover_tests(BoidsComparisonTests)
