#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/irq.h"
#include "hardware/clocks.h"

// Include the auto-generated header from the PIO assembly file
#include "bus.pio.h"

// --- Configuration ---
#define DIR_PIN 19
#define OE_PIN          20   // Output Enable Pin (Base/Lower GPIO number)
#define CE_PIN          (OE_PIN+1)   // Chip Enable Pin (OE_PIN + 1)

#define PICO_PIO pio0
#define PICO_SM 0

PIO pio = pio0;
uint sm = 0;

bool selected = 0;
// --- IRQ Handler ---

void __isr pio_irq_handler()
{
  if (pio_interrupt_get(pio, 0)) {
    selected = 1;
    // Clear the PIO IRQ flag
    pio_interrupt_clear(pio, 0);
  }
}

// --- Initialization ---

void setup_pio_irq_logic()
{
  uint offset = pio_add_program(pio, &wait_sel_program);


  pio_sm_config c = wait_sel_program_get_default_config(offset);
  sm_config_set_in_pins(&c, 0);
  pio_sm_set_consecutive_pindirs(pio, sm, OE_PIN, 2, false);
  pio_sm_init(pio, sm, offset, &c);
  pio_sm_set_enabled(pio, sm, true);
  pio_set_irq0_source_enabled(pio, pis_interrupt0, true);
  irq_set_exclusive_handler(PIO0_IRQ_0, pio_irq_handler);
  irq_set_enabled(PIO0_IRQ_0, true);

  return;
}

int main()
{
  unsigned int count = 0;


  stdio_init_all();

  // Set PicoROM data line buffer to input mode, defaults to output :(
  gpio_init(DIR_PIN);
  gpio_set_dir(DIR_PIN, GPIO_OUT);
  gpio_put(DIR_PIN, true);

  setup_pio_irq_logic();

  while (true) {
    if (selected) {
      printf("IRQ Fired! Both OE (GPIO %d) and CE (GPIO %d) are LOW.\n", OE_PIN, CE_PIN);
      selected = 0;
    }
    printf("Waiting %u\n", count++);
    sleep_ms(1000);
  }

  return 0;
}
