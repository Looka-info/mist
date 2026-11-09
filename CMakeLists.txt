cmake_minimum_required(VERSION 3.25)

# Ensure legacy third-party dependencies with cmake_minimum_required < 3.5 configure on CMake 4.x+
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)

project(Mist
    VERSION 0.3.0
    DESCRIPTION "High-performance native Windows game-streaming platform"
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for language servers and tooling
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build options
option(MIST_BUILD_TESTS "Build Mist unit and integration tests" ON)
option(MIST_TREAT_WARNINGS_AS_ERRORS "Treat compiler warnings as errors for Mist targets" ON)

# Common compiler warnings interface library
add_library(mist_warnings INTERFACE)
target_compile_features(mist_warnings INTERFACE cxx_std_20)

if(MSVC)
    target_compile_definitions(mist_warnings INTERFACE
        _CRT_SECURE_NO_WARNINGS
        _CRT_NONSTDC_NO_DEPRECATE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
    )
    target_compile_options(mist_warnings INTERFACE
        /W4
        /permissive-
        /Zc:__cplusplus
        /EHsc
        /utf-8
        /volatile:iso
        /wd4201
        /wd4100
    )
    if(MIST_TREAT_WARNINGS_AS_ERRORS)
        target_compile_options(mist_warnings INTERFACE /WX)
    endif()
    # Consistent runtime configuration
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
else()
    target_compile_options(mist_warnings INTERFACE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wsign-conversion
        -Wshadow
        -Wnon-virtual-dtor
    )
    if(MIST_TREAT_WARNINGS_AS_ERRORS)
        target_compile_options(mist_warnings INTERFACE -Werror)
    endif()
endif()

# Core engine subsystems
add_subdirectory(engine)

# Platform isolation layer
add_subdirectory(platform)

# Application entry points
add_subdirectory(apps)

# Services (signaling and relay boundaries)
add_subdirectory(services)

# Testing
if(MIST_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
