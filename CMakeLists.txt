cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS components/arduino-esp32)

# Board-specific sdkconfig defaults
if(DEFINED ENV{IDF_TARGET} AND "$ENV{IDF_TARGET}" STREQUAL "esp32s3")
    if(EXISTS "${CMAKE_SOURCE_DIR}/sdkconfig.defaults.twatch_ultra")
        set(SDKCONFIG_DEFAULTS "sdkconfig.defaults;sdkconfig.defaults.twatch_ultra")
    endif()
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
idf_build_set_property(MINIMAL_BUILD ON)
project(dji_camera_bluetooth_control)