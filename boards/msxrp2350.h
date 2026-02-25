#ifndef BOARD_msxrp2350_H
#define BOARD_msxrp2350_H

#include <stdint.h>

// Board identification
#define PICO_BOARD_NAME "msxrp2350"

// a struct to hold both address and data
typedef struct {
    uint32_t addr;
    uint8_t data;
} AddrData;

// Extract address and data from PIO addrdata word
// Layout: [31:22]=data, [21:18]=control, [17:0]=address
static inline AddrData decode_addrdata(uint32_t addrdata) {
    AddrData result;
    result.addr = (addrdata >> 4) & 0xFFFF;
    result.data = (addrdata >> (18 + 4)) & 0xFF;
    return result;
}

#endif // BOARD_msxrp2350_H
