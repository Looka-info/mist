add_executable(mist_desktop
    src/DesktopApplication.cpp
    src/main.cpp
)

target_include_directories(mist_desktop PRIVATE
    include
)

target_link_libraries(mist_desktop PRIVATE
    mist_common
    mist_session
    mist_telemetry
    mist_security
    mist_transport
    mist_platform_windows
    mist_warnings
)
