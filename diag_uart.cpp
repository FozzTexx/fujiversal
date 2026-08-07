#include "diag_uart.h"

#ifdef RP2350_LOGGING

#include <pico/stdlib.h>
#include <hardware/sync.h>
#include <hardware/watchdog.h>

#define DIAG_PIN 37
#define DIAG_BIT_US 104

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
  // An 80 character line blocks for ~830ms at 9600, well past the watchdog.
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

#endif // RP2350_LOGGING
