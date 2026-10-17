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

add_executable(EngineMeasurementTests
    unit/timing_recorder_tests.cpp
)
target_link_libraries(EngineMeasurementTests PRIVATE Engine::Measurement GTest::gtest_main)
configure_test_target(EngineMeasurementTests)
gtest_discover_tests(EngineMeasurementTests)

add_executable(AppCommandLineOptionsTests
    unit/command_line_options_tests.cpp
    ${PROJECT_SOURCE_DIR}/App/command_line_options.cpp
)
target_link_libraries(AppCommandLineOptionsTests
    PRIVATE Benchmarker::Experiment GTest::gtest_main
)
configure_test_target(AppCommandLineOptionsTests)
gtest_discover_tests(AppCommandLineOptionsTests)

add_executable(BenchmarkerExperimentTests integration/benchmarker_experiment_tests.cpp)
target_link_libraries(BenchmarkerExperimentTests PRIVATE
    Benchmarker::Experiment Boids::ECS Boids::OOP GTest::gtest_main)
configure_test_target(BenchmarkerExperimentTests)
gtest_discover_tests(BenchmarkerExperimentTests PROPERTIES RUN_SERIAL TRUE)

add_test(NAME AppBenchmarkSmoke COMMAND App --benchmark --agents 10 --warmup 1 --steps 3)
set_tests_properties(AppBenchmarkSmoke PROPERTIES
    PASS_REGULAR_EXPRESSION "Measured steps: 3" TIMEOUT 30
)
add_test(NAME AppOopBenchmarkSmoke COMMAND App --benchmark --implementation oop
    --agents 10 --warmup 1 --steps 3 --output ${CMAKE_CURRENT_BINARY_DIR}/oop-smoke-$<CONFIG>.csv)
set_tests_properties(AppOopBenchmarkSmoke PROPERTIES
    PASS_REGULAR_EXPRESSION "Measured steps: 3" TIMEOUT 30 RUN_SERIAL TRUE)
set_tests_properties(AppBenchmarkSmoke PROPERTIES RUN_SERIAL TRUE)

add_executable(EngineEcsTests
    unit/registry_tests.cpp
    unit/system_manager_tests.cpp
    unit/view_tests.cpp
)

target_link_libraries(EngineEcsTests
    PRIVATE
        Engine::ECS
        GTest::gtest_main
)

configure_test_target(EngineEcsTests)
gtest_discover_tests(EngineEcsTests)

add_executable(EngineArchetypeEcsTests
    unit/archetype_registry_tests.cpp
)
target_link_libraries(EngineArchetypeEcsTests
    PRIVATE Engine::ArchetypeECS GTest::gtest_main)
configure_test_target(EngineArchetypeEcsTests)
gtest_discover_tests(EngineArchetypeEcsTests)

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

add_executable(BoidsCommonTests
    unit/simulation_input_tests.cpp
)
target_link_libraries(BoidsCommonTests PRIVATE Boids::Common GTest::gtest_main)
configure_test_target(BoidsCommonTests)
gtest_discover_tests(BoidsCommonTests)

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

add_executable(BoidsOopTests integration/oop_boid_simulation_tests.cpp)
target_link_libraries(BoidsOopTests PRIVATE Boids::OOP GTest::gtest_main)
configure_test_target(BoidsOopTests)
gtest_discover_tests(BoidsOopTests)

add_executable(BoidsComparisonTests
    integration/boid_equivalence_tests.cpp
)
target_link_libraries(BoidsComparisonTests
    PRIVATE Boids::ECS Boids::OOP GTest::gtest_main
)
configure_test_target(BoidsComparisonTests)
gtest_discover_tests(BoidsComparisonTests)
