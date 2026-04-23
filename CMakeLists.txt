cmake_minimum_required(VERSION 3.18)

# Board selection (default: picorom_msx)
if(NOT DEFINED BOARD)
    set(BOARD "picorom_msx")
endif()

# Set board type for RP2350-based boards BEFORE importing SDK
if(BOARD STREQUAL "msxrp2350"
   OR BOARD STREQUAL "msx_proto_260402"
   OR BOARD STREQUAL "coco_proto_260402")
    set(PICO_BOARD "pimoroni_pga2350" CACHE STRING "Pico board type" FORCE)
    set(PICO_PLATFORM "rp2350-arm-s" CACHE STRING "Pico platform" FORCE)
    set(PICO_CHIP "rp2350" CACHE STRING "Pico chip" FORCE)
endif()

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(fujiversal C CXX ASM)
set(CMAKE_C_STANDARD 11)

pico_sdk_init()

add_executable(${PROJECT_NAME}
    main.cpp
    FujiBusPacket.cpp
    setup_sm.cpp
)

# Include board-specific configuration
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/boards)
target_compile_definitions(${PROJECT_NAME} PRIVATE BOARD_${BOARD})
target_compile_definitions(${PROJECT_NAME} PRIVATE BOARD_NAME=${BOARD})

if(BOARD STREQUAL "msxrp2350"
   OR BOARD STREQUAL "msx_proto_260402"
   OR BOARD STREQUAL "coco_proto_260402")
    target_compile_definitions(${PROJECT_NAME} PRIVATE PICO_PIO_USE_GPIO_BASE=1)
endif()

pico_generate_pio_header(${PROJECT_NAME}
    ${CMAKE_CURRENT_SOURCE_DIR}/boards/${BOARD}.pio
)

target_link_libraries(${PROJECT_NAME}
    pico_stdlib
    pico_multicore
    hardware_pio
)

pico_enable_stdio_usb(${PROJECT_NAME} 1)
pico_enable_stdio_uart(${PROJECT_NAME} 0)

pico_add_extra_outputs(${PROJECT_NAME})

# Set output name to include board type
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "fujiversal_${BOARD}")
