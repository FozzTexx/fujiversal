cmake_minimum_required(VERSION 3.18)

set(PICO_BOARD pico2)
set(PICO_CHIP rp2350)
set(PICO_PLATFORM rp2350-arm-s)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)

project(fujiversal C CXX ASM)
set(CMAKE_C_STANDARD 11)

pico_sdk_init()

add_executable(${PROJECT_NAME}
    main.cpp
    FujiBusPacket.cpp
)

pico_generate_pio_header(${PROJECT_NAME}
    ${CMAKE_CURRENT_SOURCE_DIR}/bus.pio
)

target_link_libraries(${PROJECT_NAME}
    pico_stdlib
    pico_multicore
    hardware_pio
)

pico_enable_stdio_usb(${PROJECT_NAME} 1)
pico_enable_stdio_uart(${PROJECT_NAME} 0)

pico_add_extra_outputs(${PROJECT_NAME})
