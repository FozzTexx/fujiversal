cmake_minimum_required(VERSION 3.18)

# Board selection (default: picorom)
if(NOT DEFINED BOARD)
    set(BOARD "picorom")
endif()

# Set board type for RP2350-based boards BEFORE importing SDK
if(BOARD STREQUAL "msxwaveshare2350b")
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

# Generate board-specific PIO file
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/boards/${BOARD}.h" BOARD_CONFIG)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/bus.pio" PIO_TEMPLATE)
string(REGEX MATCH "#define A0_PIN ([0-9]+)" _ "${BOARD_CONFIG}")
set(A0_PIN_VAL ${CMAKE_MATCH_1})
string(REGEX MATCH "#define DIR_PIN ([0-9]+)" _ "${BOARD_CONFIG}")
set(DIR_PIN_VAL ${CMAKE_MATCH_1})
string(REGEX MATCH "#define OE_PIN ([0-9]+)" _ "${BOARD_CONFIG}")
set(OE_PIN_VAL ${CMAKE_MATCH_1})
string(REGEX MATCH "#define CE_PIN ([0-9]+)" _ "${BOARD_CONFIG}")
set(CE_PIN_VAL ${CMAKE_MATCH_1})
string(REGEX MATCH "#define D0_PIN ([0-9]+)" _ "${BOARD_CONFIG}")
set(D0_PIN_VAL ${CMAKE_MATCH_1})
string(REGEX MATCH "#define PIN_COUNT ([0-9]+)" _ "${BOARD_CONFIG}")
set(PIN_COUNT_VAL ${CMAKE_MATCH_1})
string(REGEX MATCH "#define DATA_WIDTH ([0-9]+)" _ "${BOARD_CONFIG}")
set(DATA_WIDTH_VAL ${CMAKE_MATCH_1})
string(REGEX MATCH "#define WAIT_CYCLES ([0-9]+)" _ "${BOARD_CONFIG}")
set(WAIT_CYCLES_VAL ${CMAKE_MATCH_1})
string(REPLACE "A0_PIN_PLACEHOLDER" "${A0_PIN_VAL}" PIO_TEMPLATE "${PIO_TEMPLATE}")
string(REPLACE "DIR_PIN_PLACEHOLDER" "${DIR_PIN_VAL}" PIO_TEMPLATE "${PIO_TEMPLATE}")
string(REPLACE "OE_PIN_PLACEHOLDER" "${OE_PIN_VAL}" PIO_TEMPLATE "${PIO_TEMPLATE}")
string(REPLACE "CE_PIN_PLACEHOLDER" "${CE_PIN_VAL}" PIO_TEMPLATE "${PIO_TEMPLATE}")
string(REPLACE "D0_PIN_PLACEHOLDER" "${D0_PIN_VAL}" PIO_TEMPLATE "${PIO_TEMPLATE}")
string(REPLACE "PIN_COUNT_PLACEHOLDER" "${PIN_COUNT_VAL}" PIO_TEMPLATE "${PIO_TEMPLATE}")
string(REPLACE "DATA_WIDTH_PLACEHOLDER" "${DATA_WIDTH_VAL}" PIO_TEMPLATE "${PIO_TEMPLATE}")
string(REPLACE "WAIT_PLACEHOLDER" "${WAIT_CYCLES_VAL}" PIO_TEMPLATE "${PIO_TEMPLATE}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/bus_${BOARD}.pio" "${PIO_TEMPLATE}")

pico_generate_pio_header(${PROJECT_NAME}
    ${CMAKE_CURRENT_BINARY_DIR}/bus_${BOARD}.pio
)

# Add build directory to include path for generated PIO header
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

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
