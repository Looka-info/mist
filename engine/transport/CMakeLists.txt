include(FetchContent)

# Enable DTLS-SRTP in MbedTLS for WebRTC media transport
add_compile_definitions(MBEDTLS_SSL_DTLS_SRTP)

# 1. Configure and fetch MbedTLS v3.6.2
set(ENABLE_PROGRAMS OFF CACHE BOOL "" FORCE)
set(ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(MBEDTLS_FATAL_WARNINGS OFF CACHE BOOL "" FORCE)
set(INSTALL_MBEDTLS_HEADERS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    mbedtls
    GIT_REPOSITORY https://github.com/Mbed-TLS/mbedtls.git
    GIT_TAG        v3.6.2
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(mbedtls)

if(TARGET mbedtls AND NOT TARGET MbedTLS::MbedTLS)
    add_library(MbedTLS::MbedTLS INTERFACE IMPORTED)
    target_link_libraries(MbedTLS::MbedTLS INTERFACE mbedtls mbedx509 mbedcrypto)
    add_library(MbedTLS::MbedCrypto ALIAS mbedcrypto)
    add_library(MbedTLS::MbedX509 ALIAS mbedx509)
endif()

# 2. Configure libsrtp inside libdatachannel to use internal crypto (avoids find_package)
set(ENABLE_MBEDTLS OFF CACHE BOOL "" FORCE)
set(ENABLE_OPENSSL OFF CACHE BOOL "" FORCE)
set(ENABLE_NSS OFF CACHE BOOL "" FORCE)
set(TEST_APPS OFF CACHE BOOL "" FORCE)

# 3. Configure and fetch libdatachannel v0.22.4
set(NO_MEDIA OFF CACHE BOOL "" FORCE)
set(NO_WEBSOCKET OFF CACHE BOOL "" FORCE)
set(USE_MBEDTLS ON CACHE BOOL "" FORCE)
set(NO_EXAMPLES ON CACHE BOOL "" FORCE)
set(NO_TESTS ON CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    LibDataChannel
    GIT_REPOSITORY https://github.com/paullouisageneau/libdatachannel.git
    GIT_TAG        v0.22.4
    GIT_SHALLOW    TRUE
    GIT_SUBMODULES_RECURSE TRUE
)
FetchContent_MakeAvailable(LibDataChannel)

if(TARGET datachannel-static)
    if(MSVC)
        target_compile_options(datachannel-static PRIVATE /FIalgorithm)
    else()
        target_compile_options(datachannel-static PRIVATE -include algorithm)
    endif()
endif()

if(TARGET datachannel)
    if(MSVC)
        target_compile_options(datachannel PRIVATE /FIalgorithm)
    else()
        target_compile_options(datachannel PRIVATE -include algorithm)
    endif()
endif()

add_library(mist_transport STATIC
    src/PeerConnectionManager.cpp
    src/VideoTrackSender.cpp
    src/VideoTrackReceiver.cpp
    src/AudioTrackSender.cpp
    src/AudioTrackReceiver.cpp
    src/DataChannelManager.cpp
    src/TransportStatistics.cpp
    src/WebRtcTransport.cpp
    src/MultiGuestFanout.cpp
    src/NatTraversal.cpp
    src/TurnRelayManager.cpp
    src/qos/NetworkQualityEstimator.cpp
    src/qos/CongestionController.cpp
    src/qos/QualityAdaptationManager.cpp
    src/qos/AdaptiveBitrateController.cpp
)

target_include_directories(mist_transport PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_include_directories(mist_transport PRIVATE
    ${mbedtls_SOURCE_DIR}/include
)

add_library(mist_transport_warning_overrides INTERFACE)
if(NOT MSVC)
    target_compile_options(mist_transport_warning_overrides INTERFACE
        -Wno-conversion
        -Wno-sign-conversion
        -Wno-shadow
    )
endif()

target_link_libraries(mist_transport PUBLIC
    datachannel-static
    mist_common
    mist_security
    mist_encode
    mist_decode
    mist_audio
    mist_services
)

target_link_libraries(mist_transport PUBLIC
    mist_transport_warning_overrides
)

if(WIN32)
    target_link_libraries(mist_transport PRIVATE
        ws2_32
        iphlpapi
    )
endif()
