add_library(mist_render STATIC
    src/D3D11Renderer.cpp
)

target_include_directories(mist_render PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(mist_render PUBLIC
    mist_common
    mist_decode
    mist_platform_windows
    mist_telemetry
    mist_warnings
)

if (WIN32)
    target_link_libraries(mist_render PRIVATE
        d3d11
        dxgi
        user32
        gdi32
    )
endif()
