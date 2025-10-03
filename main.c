#include <stdio.h>
#include <pico/stdlib.h>
#include <pico/multicore.h>
#include <hardware/pio.h>
#include <hardware/irq.h>

#include "bus.pio.h"

#define USE_IRQ 0

#define SM_WAITSEL 0
#define SM_READ    1

#if USE_IRQ
bool selected = 0;

void __isr pio_irq_handler()
{
  if (pio_interrupt_get(pio0, 0)) {
    selected = 1;
    // Clear the PIO IRQ flag
    pio_interrupt_clear(pio0, 0);
  }
}
#endif

void setup_pio_irq_logic()
{
  pio_sm_config conf;
  uint offset;


  // Give control of DIR_PIN to PIO
  pio_gpio_init(pio0, DIR_PIN);

  // Set analog pins as digital
  for (int pin = D0_PIN; pin < D0_PIN + 8; pin++)
    pio_gpio_init(pio0, pin);

  // Invert /CE pin to make it easer to use JMP in PIO
  gpio_set_inover(CE_PIN, GPIO_OVERRIDE_INVERT);

  // Setup state machine that checks when we are selected
  offset = pio_add_program(pio0, &wait_sel_program);
  conf = wait_sel_program_get_default_config(offset);
  sm_config_set_in_pins(&conf, 0);
  sm_config_set_in_shift(&conf, true, true, 32);

  pio_sm_set_consecutive_pindirs(pio0, SM_WAITSEL, A0_PIN, 18, false);
  pio_sm_set_consecutive_pindirs(pio0, SM_WAITSEL, OE_PIN, 2, false);
  pio_sm_set_consecutive_pindirs(pio0, SM_WAITSEL, D0_PIN, 8, false);

  pio_sm_init(pio0, SM_WAITSEL, offset, &conf);
  pio_sm_set_enabled(pio0, SM_WAITSEL, true);

#if USE_IRQ
  pio_set_irq0_source_enabled(pio0, pis_interrupt0, true);
  irq_set_exclusive_handler(PIO0_IRQ_0, pio_irq_handler);
  irq_set_enabled(PIO0_IRQ_0, true);
#endif

  // Setup state machine that handles CPU read by putting byte on bus
  offset = pio_add_program(pio0, &read_program);
  conf = read_program_get_default_config(offset);

  sm_config_set_out_pins(&conf, D0_PIN, 8);
  pio_sm_set_consecutive_pindirs(pio0, SM_READ, DIR_PIN, 1, true);
  pio_sm_set_consecutive_pindirs(pio0, SM_READ, D0_PIN, 8, false);
  sm_config_set_sideset_pins(&conf, DIR_PIN);
  sm_config_set_sideset(&conf, 2, true, false);  // 1-bit, optional = true, pindirs = false

  // Set JMP pin base to CE (or OE depending on your design)
  sm_config_set_jmp_pin(&conf, CE_PIN);     // e.g. GPIO20

  pio_sm_init(pio0, SM_READ, offset, &conf);
  pio_sm_set_enabled(pio0, SM_READ, true);

  return;
}

void __time_critical_func(romulan)(void)
{
  uint32_t addrdata, addr, data;


  setup_pio_irq_logic();

  while (true) {
    while (pio0->fstat & (1u << (PIO_FSTAT_RXEMPTY_LSB + SM_WAITSEL)))
      tight_loop_contents();

    addrdata = pio0->rxf[SM_WAITSEL];
    addr = addrdata & 0xFFFF;
    data = (addrdata >> (18 + 4)) & 0xFF;
    if ((addr & 0x1fff) < 0x1ffc)
      pio0->txf[SM_READ] = addr & 0xFF;
    else {
      switch (addr & 0x3) {
      case 0: // Read byte
        pio0->txf[SM_READ] = sio_hw->fifo_rd;
        break;
      case 1: // Read status reg
        pio0->txf[SM_READ] = sio_hw->fifo_st & SIO_FIFO_ST_VLD_BITS ? 0x80 : 0x00;
        break;
      case 2: // Write byte
        sio_hw->fifo_wr = addrdata;
        break;
      case 3: // Write control reg
        break;
      }
    }
  }

  return;
}

int main()
{
  uint32_t addrdata, addr, data;
  int input;
  unsigned int count = 0;


  multicore_launch_core1(romulan);
  stdio_init_all();

  while (!stdio_usb_connected())
    ;

  while (true) {
#if 1
    if (multicore_fifo_rvalid()) {
      addrdata = multicore_fifo_pop_blocking();
      addr = addrdata & 0xFFFF;
      data = (addrdata >> (18 + 4)) & 0xFF;
      printf("Received $%04x:$%02x\n", addr, data);
    }
#endif

#if 1
    input = getchar_timeout_us(0);
    if (input != PICO_ERROR_TIMEOUT) {
      printf("Input: $%02x\n", input);
      multicore_fifo_push_blocking(input);
    }
#endif

#if 0
    printf("Waiting %u\n", count++);
    sleep_ms(1000);
#endif
  }

  return 0;
}
