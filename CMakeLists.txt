cmake_minimum_required(VERSION 3.18)
project(cpu_core_instrument C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_executable(cpu_core_instrument
    src/main.c
    src/cpu_metrics.c
    src/mapper.c
    src/synth.c
    src/cli_view.c
    platform/macos/cpu_sampler_mach.c
    platform/macos/audio_engine_au.c
)

target_include_directories(cpu_core_instrument PRIVATE
    include
    platform/macos
)

if(APPLE)
    target_link_libraries(cpu_core_instrument
        "-framework CoreAudio"
        "-framework AudioUnit"
        "-framework AudioToolbox"
    )
endif()
