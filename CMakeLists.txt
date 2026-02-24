cmake_minimum_required(VERSION 3.18)

# Board selection (default: picorom)
if(NOT DEFINED BOARD)
    set(BOARD "picorom")
endif()

# Set board type for RP2350-based boards BEFORE importing SDK
if(BOARD STREQUAL "msxrp2350")
    set(PICO_BOARD "pico2" CACHE STRING "Pico board type" FORCE)
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
)

# Include board-specific configuration
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/boards)
target_compile_definitions(${PROJECT_NAME} PRIVATE BOARD_${BOARD})

# Concatenate board-specific .pio with bus.pio
set(BOARD_PIO_FILE "${CMAKE_CURRENT_SOURCE_DIR}/boards/${BOARD}.pio")
set(BUS_PIO_FILE "${CMAKE_CURRENT_SOURCE_DIR}/bus.pio")
set(GENERATED_PIO_FILE "${CMAKE_CURRENT_BINARY_DIR}/${BOARD}.pio")

add_custom_command(
    OUTPUT ${GENERATED_PIO_FILE}
    COMMAND ${CMAKE_COMMAND} -E cat ${BOARD_PIO_FILE} ${BUS_PIO_FILE} > ${GENERATED_PIO_FILE}
    DEPENDS ${BOARD_PIO_FILE} ${BUS_PIO_FILE}
    COMMENT "Generating ${BOARD}.pio"
)

add_custom_target(generate_pio DEPENDS ${GENERATED_PIO_FILE})
add_dependencies(${PROJECT_NAME} generate_pio)

# Generate PIO header from concatenated file
pico_generate_pio_header(${PROJECT_NAME}
    ${GENERATED_PIO_FILE}
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
