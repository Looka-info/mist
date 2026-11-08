add_library(mist_services STATIC
    src/WebSocketSignalingClient.cpp
)

target_include_directories(mist_services PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(mist_services PUBLIC
    datachannel-static
    mist_common
    mist_security
)

target_link_libraries(mist_services PRIVATE
    mist_transport_warning_overrides
)
