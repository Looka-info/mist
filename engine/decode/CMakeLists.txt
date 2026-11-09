add_library(mist_decode STATIC
    src/H264Parser.cpp
    src/HevcParser.cpp
    src/Av1Parser.cpp
    src/AnnexBReader.cpp
    src/D3D11VideoDecoder.cpp
    src/CrossPlatformDecoders.cpp
    src/GuestClientSession.cpp
    src/DecodePresentPipeline.cpp
)

target_include_directories(mist_decode PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(mist_decode PUBLIC
    mist_common
    mist_encode
    mist_input
    mist_render
    mist_telemetry
    mist_warnings
)

if (WIN32)
    target_link_libraries(mist_decode PUBLIC
        mist_platform_windows
    )
    target_link_libraries(mist_decode PRIVATE
        d3d11
        dxgi
        mfplat
        mfuuid
        ole32
    )
elseif (APPLE)
    target_link_libraries(mist_decode PRIVATE
        "-framework VideoToolbox"
        "-framework CoreMedia"
        "-framework CoreVideo"
        "-framework Foundation"
    )
elseif (ANDROID)
    target_link_libraries(mist_decode PRIVATE
        mediandk
        android
    )
elseif (UNIX)
    target_link_libraries(mist_decode PRIVATE
        va
        va-drm
        drm
    )
endif()

if (APPLE)
    find_library(VIDEOTOOLBOX_FRAMEWORK VideoToolbox REQUIRED)
    find_library(COREMEDIA_FRAMEWORK CoreMedia REQUIRED)
    find_library(COREVIDEO_FRAMEWORK CoreVideo REQUIRED)
    find_library(FOUNDATION_FRAMEWORK Foundation REQUIRED)
    target_link_libraries(mist_decode PRIVATE
        ${VIDEOTOOLBOX_FRAMEWORK}
        ${COREMEDIA_FRAMEWORK}
        ${COREVIDEO_FRAMEWORK}
        ${FOUNDATION_FRAMEWORK}
    )
endif()

if (CMAKE_SYSTEM_NAME MATCHES "Linux" AND NOT ANDROID)
    find_package(PkgConfig QUIET)
    pkg_check_modules(LIBVA QUIET libva)
    pkg_check_modules(LIBDRM QUIET libdrm)
    if (LIBVA_FOUND)
        target_include_directories(mist_decode PRIVATE ${LIBVA_INCLUDE_DIRS})
        target_link_libraries(mist_decode PRIVATE ${LIBVA_LIBRARIES})
    else()
        target_link_libraries(mist_decode PRIVATE va va-drm)
    endif()
    if (LIBDRM_FOUND)
        target_include_directories(mist_decode PRIVATE ${LIBDRM_INCLUDE_DIRS})
        target_link_libraries(mist_decode PRIVATE ${LIBDRM_LIBRARIES})
    else()
        target_link_libraries(mist_decode PRIVATE drm)
    endif()
endif()

if (ANDROID)
    target_link_libraries(mist_decode PRIVATE
        mediandk
        android
        log
    )
endif()
