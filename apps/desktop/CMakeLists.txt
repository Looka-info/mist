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

option(MIST_BUILD_LEGACY_IMGUI "Build legacy Dear ImGui desktop application (deprecated)" OFF)

if(MIST_BUILD_LEGACY_IMGUI)
    message(STATUS "Building legacy Dear ImGui desktop interface (deprecated). Production UI is Rust + Slint.")
    add_executable(mist_desktop WIN32
        src/DesktopApplication.cpp
        src/main.cpp
        ${CMAKE_SOURCE_DIR}/resources/mist.rc
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
        windowscodecs
        wlanapi
        iphlpapi
        ws2_32
        mist_warnings
    )
else()
    # Modern Rust + Slint Desktop Target
    find_program(CARGO_EXECUTABLE cargo REQUIRED)

    set(RUST_TARGET_SUBDIR "release")
    set(RUST_CONFIG_FLAG "--release")
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(RUST_TARGET_SUBDIR "debug")
        set(RUST_CONFIG_FLAG "")
    endif()

    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/mist_desktop.exe
        COMMAND ${CARGO_EXECUTABLE} build --package mist-desktop --package mist-signaling ${RUST_CONFIG_FLAG}
        COMMAND ${CMAKE_COMMAND}
            -DSOURCE_1=${CMAKE_SOURCE_DIR}/target/${RUST_TARGET_SUBDIR}/mist-desktop.exe
            -DSOURCE_2=D:/rust/target/${RUST_TARGET_SUBDIR}/mist-desktop.exe
            -DSOURCE_3=${CMAKE_SOURCE_DIR}/bin/mist_desktop.exe
            -DDESTINATION=${CMAKE_CURRENT_BINARY_DIR}/mist_desktop.exe
            -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
            -DRUST_TARGET_SUBDIR=${RUST_TARGET_SUBDIR}
            -P ${CMAKE_SOURCE_DIR}/cmake/stage_desktop.cmake
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_BINARY_DIR}/mist_desktop.exe"
            "${CMAKE_SOURCE_DIR}/bin/mist_desktop.exe"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Building and staging Mist Desktop and Signaling -> ${CMAKE_CURRENT_BINARY_DIR}/mist_desktop.exe"
    )

    add_custom_target(mist_desktop ALL
        DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/mist_desktop.exe
    )
endif()
