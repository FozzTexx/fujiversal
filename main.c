#include <stdio.h>
#include <pico/stdlib.h>
//#include <pico/multicore.h>
#include <hardware/pio.h>
#include <hardware/irq.h>

#include "bus.pio.h"

#define DIR_PIN 19
#define CE_PIN          21   // Chip Enable
#define OE_PIN          20   // Output Enable
#define ADDR_PIN        0
#define DATA_PIN        22

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

void setup_pio_irq_logic()
{
  uint offset = pio_add_program(pio, &wait_sel_program);


  pio_sm_config c = wait_sel_program_get_default_config(offset);
  sm_config_set_in_pins(&c, 0);
  sm_config_set_in_shift(&c, true, true, 32);

  // Set analog pins as digital
  for (int pin = DATA_PIN + 4; pin < DATA_PIN + 8; pin++) {
    gpio_init(pin);
    gpio_set_dir(pin, false);   // false = input
  }

  pio_sm_set_consecutive_pindirs(pio, sm, ADDR_PIN, 18, false);
  pio_sm_set_consecutive_pindirs(pio, sm, OE_PIN, 2, false);
  pio_sm_set_consecutive_pindirs(pio, sm, DATA_PIN, 8, false);

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
  uint32_t addrdata, addr, data;


  stdio_init_all();

  // Set PicoROM data line buffer to input mode, defaults to output :(
  gpio_init(DIR_PIN);
  gpio_set_dir(DIR_PIN, GPIO_OUT);
  gpio_put(DIR_PIN, true);

  setup_pio_irq_logic();

  while (true) {
#if 1
    if (selected) {
      //printf("IRQ Fired! Both OE (GPIO %d) and CE (GPIO %d) are LOW.\n", OE_PIN, CE_PIN);
      selected = 0;
    }
#endif
    printf("Waiting %u\n", count++);
#if 1
    addrdata = pio_sm_get_blocking(pio, sm);
    addr = addrdata & 0xFFFF;
    data = (addrdata >> (18 + 4)) & 0xFF;
    printf("Received $%04x:$%02x\n", addr, data);
#else
    sleep_ms(1000);
#endif
  }

  return 0;
}
