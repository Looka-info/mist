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
    TestH264Parser.cpp
    TestAnnexBReader.cpp
    TestDecoder.cpp
    TestRenderer.cpp
    TestClientPipeline.cpp
    TestClientStability.cpp
    TestWebRtcTransport.cpp
    TestSignaling.cpp
    TestNetworkStability.cpp
    TestAudioFormat.cpp
    TestOpusCodec.cpp
    TestWasapiAudio.cpp
    TestAudioWebRtc.cpp
    TestInputProtocol.cpp
    TestInputState.cpp
    TestInputPermission.cpp
    TestInputDispatcher.cpp
    TestGamepadCapture.cpp
    TestCongestionControl.cpp
    TestPairing.cpp
    TestDesktopApp.cpp
    TestNatTraversal.cpp
    TestTurnRelay.cpp
    TestPeerIdentity.cpp
    TestMultiGuestFanout.cpp
    TestPerGuestAdaptiveBitrate.cpp
    TestHostBandwidthBudget.cpp
    TestHevcCodec.cpp
    TestEncoderFallbacks.cpp
    TestLowLatencyTuning.cpp
    TestFecAndRecovery.cpp
    TestD3D11VaPipeline.cpp
    TestMultiGuestGamepad.cpp
    TestRelativeMouseAndClipboard.cpp
)

target_include_directories(mist_unit_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/apps/host/include
    ${CMAKE_SOURCE_DIR}/apps/client/include
    ${CMAKE_SOURCE_DIR}/apps/desktop/include
)

target_link_libraries(mist_unit_tests PRIVATE
    mist_imgui
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
    windowscodecs
    wlanapi
    iphlpapi
    ws2_32
    mist_services
    GTest::gtest_main
    mist_warnings
)

# Also compile the host, client and desktop application classes for lifecycle unit testing
target_sources(mist_unit_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/apps/host/src/HostApplication.cpp
    ${CMAKE_SOURCE_DIR}/apps/client/src/ClientApplication.cpp
    ${CMAKE_SOURCE_DIR}/apps/desktop/src/DesktopApplication.cpp
)

include(GoogleTest)
gtest_discover_tests(mist_unit_tests)
