add_executable(mist_unit_tests
    TestCommon.cpp
    TestLogger.cpp
    TestConfig.cpp
    TestSessionState.cpp
    TestApplicationLifecycle.cpp
    TestSecurity.cpp
    TestTelemetry.cpp
    TestPlatformWindows.cpp
    TestCapture.cpp
    TestEncoder.cpp
    TestPipeline.cpp
    TestStability.cpp
)

target_include_directories(mist_unit_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/apps/host/include
    ${CMAKE_SOURCE_DIR}/apps/client/include
    ${CMAKE_SOURCE_DIR}/apps/desktop/include
)

target_link_libraries(mist_unit_tests PRIVATE
    mist_common
    mist_session
    mist_capture
    mist_encode
    mist_decode
    mist_render
    mist_transport
    mist_audio
    mist_input
    mist_telemetry
    mist_security
    mist_platform_windows
    mist_services
    GTest::gtest_main
    mist_warnings
)

# Also compile the host and client application classes for lifecycle unit testing
target_sources(mist_unit_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/apps/host/src/HostApplication.cpp
    ${CMAKE_SOURCE_DIR}/apps/client/src/ClientApplication.cpp
)

include(GoogleTest)
gtest_discover_tests(mist_unit_tests)
