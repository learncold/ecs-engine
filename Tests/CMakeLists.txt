add_executable(ECSViewTests
    ecs_view_tests.cpp
)

target_link_libraries(ECSViewTests
    PRIVATE
        Engine::ECS
)

target_compile_features(ECSViewTests PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(ECSViewTests PRIVATE /W4)
else()
    target_compile_options(ECSViewTests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME ECSViewTests COMMAND ECSViewTests)
