include(FetchContent)

# 1. Configure and fetch official Opus v1.5.2
set(OPUS_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(OPUS_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(OPUS_BUILD_SHARED_LIBRARY OFF CACHE BOOL "" FORCE)
set(OPUS_INSTALL_PKG_CONFIG_FILE OFF CACHE BOOL "" FORCE)
set(OPUS_INSTALL_CMAKE_CONFIG_MODULE OFF CACHE BOOL "" FORCE)
set(OPUS_ENABLE_FLOAT_API ON CACHE BOOL "" FORCE)

FetchContent_Declare(
    opus
    GIT_REPOSITORY https://github.com/xiph/opus.git
    GIT_TAG        v1.5.2
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(opus)

# 2. mist_audio library
add_library(mist_audio STATIC
    src/AudioResampler.cpp
    src/OpusAudioEncoder.cpp
    src/OpusAudioDecoder.cpp
    src/WasapiAudioCapture.cpp
    src/WasapiAudioPlayback.cpp
    src/AudioDeviceEnumerator.cpp
)

target_include_directories(mist_audio PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(mist_audio PUBLIC
    mist_common
    mist_warnings
    opus
    ole32
    avrt
)

