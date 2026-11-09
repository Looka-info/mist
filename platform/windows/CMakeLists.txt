add_library(mist_platform_windows
    src/PlatformWindows.cpp
    src/D3D11Device.cpp
    src/VirtualDisplayManager.cpp
)

target_include_directories(mist_platform_windows PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(mist_platform_windows PUBLIC
    mist_common
    mist_warnings
)

if(WIN32)
    target_link_libraries(mist_platform_windows PRIVATE
        user32
        kernel32
        d3d11
        dxgi
        winmm
    )
endif()
