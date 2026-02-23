#ifndef BOARD_PICOROM_H
#define BOARD_PICOROM_H

#include <stdint.h>

// GPIO Pin Definitions for picorom board
#define A0_PIN 0
#define DIR_PIN 19
#define OE_PIN 20
#define CE_PIN 21
#define D0_PIN 22
#define PIN_COUNT 32
#define DATA_WIDTH 8
#define ADDRESS_WIDTH 18
#define WAIT_CYCLES 5

// Board identification
#define PICO_BOARD_NAME "picorom"

// a struct to hold both address and data
typedef struct {
    uint32_t addr;
    uint8_t data;
} AddrData;

// Extract address and data from PIO addrdata word
// Layout: [31:22]=data, [21:18]=control, [17:0]=address
static inline AddrData decode_addrdata(uint32_t addrdata) {
    AddrData result;
    result.addr = addrdata & 0x3FFFF;  // 18-bit address mask
    result.data = (addrdata >> (ADDRESS_WIDTH + 4)) & 0xFF;
    return result;
}

#endif // BOARD_PICOROM_H
