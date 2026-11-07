add_library(mist_input INTERFACE)

target_include_directories(mist_input INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(mist_input INTERFACE
    mist_common
    mist_warnings
)
