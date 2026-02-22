cmake_minimum_required(VERSION 3.18)
project(cpu_core_instrument C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_compile_definitions(_GNU_SOURCE)

add_executable(cpu_core_instrument
    src/main.c
    src/cpu_metrics.c
    src/mapper.c
    src/synth.c
    src/cli_view.c
)

if(APPLE)
    target_sources(cpu_core_instrument PRIVATE
        platform/macos/cpu_sampler_mach.c
        platform/macos/audio_engine_au.c
    )
    target_include_directories(cpu_core_instrument PRIVATE
        include
        platform/macos
    )
    target_link_libraries(cpu_core_instrument
        "-framework CoreAudio"
        "-framework AudioUnit"
        "-framework AudioToolbox"
    )
elseif(UNIX AND NOT APPLE)
    target_sources(cpu_core_instrument PRIVATE
        platform/linux/cpu_sampler_linux.c
        platform/linux/audio_engine_alsa.c
    )
    target_include_directories(cpu_core_instrument PRIVATE
        include
        platform/linux
    )
    target_link_libraries(cpu_core_instrument
        asound
        pthread
        m
    )
endif()
