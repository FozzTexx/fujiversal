#include "diag_uart.h"

#ifdef RP2350_LOGGING

#include <pico/stdlib.h>
#include <hardware/sync.h>
#include <hardware/watchdog.h>

#define DIAG_PIN 37
#define DIAG_BIT_US 104

volatile uint32_t diag_bf[BF_CAP];
volatile uint32_t diag_bf_idx = 0;
volatile uint32_t diag_lo_reads = 0;
volatile uint32_t diag_hi_reads = 0;
volatile bool diag_frozen = false;

static volatile uint16_t diag_resets = 0;
static volatile uint16_t diag_ioctl = 0;
// Bumped per user-ROM enable so successive carts in one capture are told apart.
static volatile uint8_t diag_mount = 0;

// setup_pio_irq_logic() resets every GPIO to an input, so claim the pin here
// rather than at startup.
void diag_open(void)
{
  gpio_init(DIAG_PIN);
  gpio_set_dir(DIAG_PIN, GPIO_OUT);
  gpio_put(DIAG_PIN, 1);
  busy_wait_us_32(DIAG_BIT_US * 4);
}

void diag_putc(char c)
{
  // An 80 character line blocks for ~83ms at 9600, most of the watchdog period.
  watchdog_update();
  uint32_t save = save_and_disable_interrupts();

  gpio_put(DIAG_PIN, 0);
  busy_wait_us_32(DIAG_BIT_US);
  for (int bit = 0; bit < 8; bit++) {
    gpio_put(DIAG_PIN, (c >> bit) & 1);
    busy_wait_us_32(DIAG_BIT_US);
  }
  gpio_put(DIAG_PIN, 1);
  restore_interrupts(save);
  busy_wait_us_32(DIAG_BIT_US);
}

void diag_puts(const char *s)
{
  while (*s)
    diag_putc(*s++);
}

void diag_hex(uint32_t v, int digits)
{
  while (--digits >= 0)
    diag_putc("0123456789abcdef"[(v >> (digits * 4)) & 0xF]);
}

void diag_count_reset(void)
{
  diag_resets++;
}

void diag_count_ioctl(void)
{
  diag_ioctl++;
}

void diag_mount_begin(void)
{
  diag_lo_reads = 0;
  diag_hi_reads = 0;
  diag_bf_idx = 0;
  diag_frozen = false;
  diag_mount++;
}

// One character per call so core 0 is never blocked for a whole line at once.
void diag_poll(uint32_t now, bool active, uint32_t len, uint16_t mask,
               uint16_t banks, uint8_t type)
{
  static uint32_t due = 0, snap = 0, base = 0, s_lo = 0, s_hi = 0;
  static int idx = -1;
  static bool sent = false, was_active = false;

  if (active && !was_active) {
    due = now + 5000;
    sent = false;
  }
  was_active = active;

  if (idx >= 0) {
    if ((uint32_t)idx >= snap) {
      diag_puts("\r\n");
      idx = -1;
      diag_frozen = false;      // resume recording
      due = now + 10000;        // and report again
      sent = false;
      return;
    }
    uint32_t e = diag_bf[(base + idx++) & (BF_CAP - 1)];
    diag_hex(e & 0xFFFF, 4);
    diag_putc(e & 0x10000 ? 'r' : 'w');
    diag_putc(':');
    diag_hex(e >> 24, 2);
    diag_putc(' ');
    return;
  }

  if (sent || diag_frozen || !due || (int32_t)(now - due) < 0)
    return;

  sent = true;
  idx = 0;
  diag_frozen = true;           // stop core 1 so the buffer stays coherent
  s_lo = diag_lo_reads;
  s_hi = diag_hi_reads;
  if (diag_bf_idx >= BF_CAP) {
    snap = BF_CAP;
    base = diag_bf_idx & (BF_CAP - 1);
  }
  else {
    snap = diag_bf_idx;
    base = 0;
  }

  diag_open();
  diag_puts("FV m=");
  diag_hex(diag_mount, 2);
  diag_puts(" rs=");
  diag_hex(diag_resets, 2);
  diag_puts(" io=");
  diag_hex(diag_ioctl, 2);
  diag_puts(" a=");
  diag_putc(active ? '1' : '0');
  diag_puts(" len=");
  diag_hex(len, 5);
  diag_puts(" msk=");
  diag_hex(mask, 4);
  diag_puts(" bk=");
  diag_hex(banks, 2);
  diag_puts(" ty=");
  diag_hex(type, 2);
  diag_puts(" lo=");
  diag_hex(s_lo, 6);
  diag_puts(" hi=");
  diag_hex(s_hi, 6);
  diag_puts("\r\n");
}

#endif // RP2350_LOGGING
