add_library(mist_encode INTERFACE)

target_include_directories(mist_encode INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(mist_encode INTERFACE
    mist_common
    mist_warnings
)
