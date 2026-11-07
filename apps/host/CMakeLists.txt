add_executable(mist_host
    src/HostApplication.cpp
    src/main.cpp
)

target_include_directories(mist_host PRIVATE
    include
)

target_link_libraries(mist_host PRIVATE
    mist_common
    mist_session
    mist_capture
    mist_encode
    mist_telemetry
    mist_security
    mist_platform_windows
    mist_warnings
)
