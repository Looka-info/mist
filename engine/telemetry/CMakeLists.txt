add_library(mist_telemetry
    src/Metrics.cpp
)

target_include_directories(mist_telemetry PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(mist_telemetry PUBLIC
    mist_common
    mist_warnings
)
