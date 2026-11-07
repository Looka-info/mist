add_library(mist_render INTERFACE)

target_include_directories(mist_render INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(mist_render INTERFACE
    mist_common
    mist_decode
    mist_warnings
)
