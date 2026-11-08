add_executable(mist_client
    src/ClientApplication.cpp
    src/main.cpp
    ${CMAKE_SOURCE_DIR}/resources/mist.rc
)

target_include_directories(mist_client PRIVATE
    include
)

target_link_libraries(mist_client PRIVATE
    mist_common
    mist_session
    mist_telemetry
    mist_security
    mist_platform_windows
    mist_decode
    mist_render
    mist_audio
    mist_input
    mist_transport
    mist_services
    mist_warnings
)
