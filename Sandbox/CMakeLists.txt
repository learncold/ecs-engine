add_executable(Sandbox)

target_sources(Sandbox
    PRIVATE
        src/main.cpp
)

target_link_libraries(Sandbox
    PRIVATE
        Engine::Core
)

target_compile_features(Sandbox PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(Sandbox PRIVATE /W4)
else()
    target_compile_options(Sandbox PRIVATE -Wall -Wextra -Wpedantic)
endif()
