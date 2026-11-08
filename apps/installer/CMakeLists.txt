configure_file(
    resources/installer.rc.in
    ${CMAKE_CURRENT_BINARY_DIR}/installer.rc
    @ONLY
)

add_executable(mist_installer WIN32
    src/main.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/installer.rc
)

target_include_directories(mist_installer PRIVATE
    ${CMAKE_SOURCE_DIR}/engine/common/include
)

target_link_libraries(mist_installer PRIVATE
    ole32
    shell32
    user32
    gdi32
    advapi32
    comctl32
    shlwapi
)

set_property(SOURCE ${CMAKE_CURRENT_BINARY_DIR}/installer.rc
    PROPERTY OBJECT_DEPENDS
        ${CMAKE_BINARY_DIR}/apps/desktop/mist_desktop.exe
        ${CMAKE_BINARY_DIR}/apps/host/mist_host.exe
        ${CMAKE_BINARY_DIR}/apps/client/mist_client.exe
        ${CMAKE_SOURCE_DIR}/bin/mist-signaling.exe
        ${CMAKE_SOURCE_DIR}/resources/mist.ico
)

add_dependencies(mist_installer
    mist_desktop
    mist_host
    mist_client
)

set_target_properties(mist_installer PROPERTIES
    OUTPUT_NAME "MistSetup"
    WIN32_EXECUTABLE TRUE
)
