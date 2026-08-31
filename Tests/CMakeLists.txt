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

add_executable(SandboxBoidsTests
    integration/boid_system_tests.cpp
    integration/boid_simulation_tests.cpp
    integration/movement_system_tests.cpp
)

target_link_libraries(SandboxBoidsTests
    PRIVATE
        Sandbox::BoidsECS
        GTest::gtest_main
)

configure_test_target(SandboxBoidsTests)
gtest_discover_tests(SandboxBoidsTests)
