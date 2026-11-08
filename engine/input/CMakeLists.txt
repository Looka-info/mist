add_library(mist_input STATIC
    src/InputProtocol.cpp
    src/InputState.cpp
    src/InputPermissionManager.cpp
    src/KeyboardInput.cpp
    src/MouseInput.cpp
    src/GamepadInput.cpp
    src/InputDispatcher.cpp
    src/InputReceiver.cpp
    src/ClientInputCapture.cpp
)

target_include_directories(mist_input PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(mist_input PUBLIC
    mist_common
    mist_telemetry
    mist_warnings
)

if(WIN32)
    target_link_libraries(mist_input PRIVATE user32)
endif()
