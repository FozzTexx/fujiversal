#ifndef DIAG_UART_H
#define DIAG_UART_H

#include <cstdint>

// Bit-banged diagnostic serial on a spare GPIO, 9600 8N1. Transmitting blocks
// for ~1ms per character with interrupts off, so only call it when the bus is
// idle. Comment out RP2350_LOGGING to compile it all away.
//#define RP2350_LOGGING

#ifdef RP2350_LOGGING

// Per-cycle recording. Costs a store and two counters on every served read, so
// leave it off unless a capture is actually needed.
#define DIAG_RING 0
#define BF_CAP 128

extern volatile uint32_t diag_bf[BF_CAP];   // addr | rw<<16 | data<<24
extern volatile uint32_t diag_bf_idx;
extern volatile uint32_t diag_lo_reads;
extern volatile uint32_t diag_hi_reads;
// Stops core 1 recording mid-dump, so the ring is a sequence and not samples.
extern volatile bool diag_frozen;

void diag_open(void);
void diag_putc(char c);
void diag_puts(const char *s);
void diag_hex(uint32_t v, int digits);

void diag_count_reset(void);
void diag_count_ioctl(void);
void diag_mount_begin(void);
void diag_poll(uint32_t now, bool active, uint32_t len, uint16_t mask,
               uint16_t banks, uint8_t type);

// Called from the read fast path, so it must stay inline and must not run
// before the bus has been answered.
static inline void diag_record(uint16_t addr, bool rw, uint8_t data, bool low)
{
#if DIAG_RING
  if (diag_frozen)
    return;
  diag_bf[diag_bf_idx & (BF_CAP - 1)] =
    addr | (rw ? 0x10000 : 0) | ((uint32_t)data << 24);
  diag_bf_idx++;
  if (low)
    diag_lo_reads++;
  else
    diag_hi_reads++;
#else
  (void)addr; (void)rw; (void)data; (void)low;
#endif
}

#else // ! RP2350_LOGGING

static inline void diag_open(void) {}
static inline void diag_putc(char) {}
static inline void diag_puts(const char *) {}
static inline void diag_hex(uint32_t, int) {}
static inline void diag_record(uint16_t, bool, uint8_t, bool) {}
static inline void diag_count_reset(void) {}
static inline void diag_count_ioctl(void) {}
static inline void diag_mount_begin(void) {}
static inline void diag_poll(uint32_t, bool, uint32_t, uint16_t, uint16_t,
                             uint8_t) {}

#endif // RP2350_LOGGING

#endif // DIAG_UART_H
