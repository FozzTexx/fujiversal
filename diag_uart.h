#ifndef DIAG_UART_H
#define DIAG_UART_H

#include <cstdint>

// Bit-banged diagnostic serial on a spare GPIO, 9600 8N1. Transmitting blocks
// for ~1ms per character with interrupts off, so only call it when the bus is
// idle. Comment out RP2350_LOGGING to compile it all away.
//#define RP2350_LOGGING

#ifdef RP2350_LOGGING

void diag_open(void);
void diag_putc(char c);
void diag_puts(const char *s);
void diag_hex(uint32_t v, int digits);

#else

static inline void diag_open(void) {}
static inline void diag_putc(char) {}
static inline void diag_puts(const char *) {}
static inline void diag_hex(uint32_t, int) {}

#endif // RP2350_LOGGING

#endif // DIAG_UART_H
