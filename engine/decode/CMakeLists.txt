add_library(mist_decode INTERFACE)

target_include_directories(mist_decode INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(mist_decode INTERFACE
    mist_common
    mist_encode
    mist_warnings
)
