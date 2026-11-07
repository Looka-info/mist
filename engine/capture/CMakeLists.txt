add_library(mist_capture
    src/DxgiCaptureEngine.cpp
    src/ColorConverter.cpp
    src/SyntheticFrameGenerator.cpp
    src/CaptureEncodePipeline.cpp
)

target_include_directories(mist_capture PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(mist_capture PUBLIC
    mist_common
    mist_encode
    mist_telemetry
    mist_platform_windows
    mist_warnings
)

if(WIN32)
    target_link_libraries(mist_capture PRIVATE
        d3d11
        dxgi
    )
endif()
