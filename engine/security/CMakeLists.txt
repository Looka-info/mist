add_library(mist_security
    src/Security.cpp
    src/PairingManager.cpp
)

target_include_directories(mist_security PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(mist_security PUBLIC
    mist_common
)

target_link_libraries(mist_security PRIVATE
    mist_warnings
)
