include(FetchContent)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.91.8
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(imgui)

add_library(mist_imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp
)
target_include_directories(mist_imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(mist_imgui PUBLIC
    d3d11
    dxgi
    d3dcompiler
    dwmapi
)

add_executable(mist_desktop
    src/DesktopApplication.cpp
    src/main.cpp
)

target_include_directories(mist_desktop PRIVATE
    include
)

target_link_libraries(mist_desktop PRIVATE
    mist_imgui
    mist_common
    mist_session
    mist_telemetry
    mist_security
    mist_transport
    mist_platform_windows
    mist_warnings
)
