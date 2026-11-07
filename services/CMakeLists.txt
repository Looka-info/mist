add_library(mist_services INTERFACE)

target_include_directories(mist_services INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(mist_services INTERFACE
    mist_common
    mist_security
    mist_warnings
)
