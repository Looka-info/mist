add_library(mist_audio INTERFACE)

target_include_directories(mist_audio INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(mist_audio INTERFACE
    mist_common
    mist_warnings
)
