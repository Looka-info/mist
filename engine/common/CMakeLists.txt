add_library(mist_common
    src/Error.cpp
    src/Logger.cpp
    src/Config.cpp
)

target_include_directories(mist_common PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(mist_common PUBLIC
    mist_warnings
)
