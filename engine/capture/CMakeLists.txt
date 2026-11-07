add_library(mist_capture INTERFACE)

target_include_directories(mist_capture INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(mist_capture INTERFACE
    mist_common
    mist_warnings
)
