cmake_minimum_required(VERSION 3.16)
project(PS3CTRL C CXX)

set(CMAKE_CXX_STANDARD 17)

# PlatformIO private configuration if generated
include(CMakeListsPrivate.txt OPTIONAL)

# Collect sources and headers
file(GLOB_RECURSE SRC_FILES "src/*.*" "include/*.*")

# Custom targets for CLion toolbar integration
add_custom_target(PLATFORMIO_BUILD ALL
    COMMAND pio run
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    SOURCES ${SRC_FILES} platformio.ini
    COMMENT "Building ESP32-S3 firmware with PlatformIO"
)

add_custom_target(PLATFORMIO_UPLOAD
    COMMAND pio run -t upload
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    SOURCES ${SRC_FILES} platformio.ini
    COMMENT "Flashing firmware to ESP32-S3 via PlatformIO"
)

add_custom_target(PLATFORMIO_MONITOR
    COMMAND pio device monitor
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Opening PlatformIO Serial Monitor"
)

add_custom_target(PLATFORMIO_CLEAN
    COMMAND pio run -t clean
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Cleaning PlatformIO build artifacts"
)

add_custom_target(PLATFORMIO_COMPILEDB
    COMMAND pio run -t compiledb
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Regenerating compilation database for CLion code insight"
)
