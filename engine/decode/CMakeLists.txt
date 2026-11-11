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
endif()
