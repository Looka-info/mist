add_library(mist_session
    src/SessionStateMachine.cpp
    src/ReconnectionManager.cpp
    src/SessionResumeManager.cpp
    src/PowerStateManager.cpp
)

target_include_directories(mist_session PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(mist_session PUBLIC
    mist_common
    mist_warnings
)
