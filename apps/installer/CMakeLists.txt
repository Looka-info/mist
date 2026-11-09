configure_file(
    resources/installer.rc.in
    ${CMAKE_CURRENT_BINARY_DIR}/installer.rc.tmp
    @ONLY
)

file(GENERATE
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/installer-$<CONFIG>.rc"
    INPUT "${CMAKE_CURRENT_BINARY_DIR}/installer.rc.tmp"
)

add_executable(mist_installer WIN32
    src/main.cpp
    "${CMAKE_CURRENT_BINARY_DIR}/installer-$<CONFIG>.rc"
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

add_dependencies(mist_installer
    mist_desktop
    mist_host
    mist_client
)

set_target_properties(mist_installer PROPERTIES
    OUTPUT_NAME "MistSetup"
    WIN32_EXECUTABLE TRUE
)
