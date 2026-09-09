add_library(BoidsCommon INTERFACE)
add_library(Boids::Common ALIAS BoidsCommon)

target_include_directories(BoidsCommon
    INTERFACE
        ${PROJECT_SOURCE_DIR}
)

target_compile_features(BoidsCommon INTERFACE cxx_std_20)
