set(MIST_NV_CODEC_SDK_ROOT "$ENV{MIST_NV_CODEC_SDK_ROOT}" CACHE PATH "Path to NVIDIA Video Codec SDK")
if(NOT MIST_NV_CODEC_SDK_ROOT AND EXISTS "D:/tools/nv_codec_sdk")
    set(MIST_NV_CODEC_SDK_ROOT "D:/tools/nv_codec_sdk")
endif()

set(NVENC_INCLUDE_DIRS "")
if(EXISTS "${MIST_NV_CODEC_SDK_ROOT}/include/nvEncodeAPI.h")
    message(STATUS "NVIDIA Video Codec SDK headers found at: ${MIST_NV_CODEC_SDK_ROOT}/include")
    set(MIST_HAS_NVENC_SDK ON)
    set(NVENC_INCLUDE_DIRS "${MIST_NV_CODEC_SDK_ROOT}/include")
else()
    message(STATUS "NVIDIA Video Codec SDK headers NOT found. NVENC will compile in unavailable fallback mode.")
    set(MIST_HAS_NVENC_SDK OFF)
endif()

add_library(mist_encode
    src/NvencVideoEncoder.cpp
)

target_include_directories(mist_encode PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

if(MIST_HAS_NVENC_SDK)
    target_compile_definitions(mist_encode PUBLIC MIST_HAS_NVENC_SDK=1)
    target_include_directories(mist_encode PRIVATE ${NVENC_INCLUDE_DIRS})
endif()

target_link_libraries(mist_encode PUBLIC
    mist_common
    mist_capture
    mist_platform_windows
    mist_warnings
)

if(WIN32)
    target_link_libraries(mist_encode PRIVATE
        d3d11
        dxgi
        version
    )
endif()
