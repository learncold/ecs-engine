add_executable(App
    main.cpp
    command_line_options.cpp
    UI/performance_panel.cpp
)

target_include_directories(App
    PRIVATE
        ${PROJECT_SOURCE_DIR}
)

target_compile_features(App PRIVATE cxx_std_20)

target_link_libraries(App
    PRIVATE
        Boids::ECS
        Engine::Benchmark
        Engine::Core
        Engine::ECSRenderer
        Engine::Renderer
        imgui::imgui
)

if(MSVC)
    target_compile_options(App PRIVATE /W4)
else()
    target_compile_options(App PRIVATE -Wall -Wextra -Wpedantic)
endif()
