#include <stdio.h>
#include <pico/stdlib.h>
#include <pico/multicore.h>
#include <hardware/pio.h>
#include <hardware/irq.h>

#include "bus.pio.h"
#include "rom.h"

#define IO_BASE 0xBFFC

#define MSX_PAGE_SIZE 0x4000
#define ROM disk_rom

#define USE_IRQ 0

#define SM_WAITSEL 0
#define SM_READ    1

#define POW2_CEIL(x_) ({      \
    unsigned int x = x_; \
    x -= 1;              \
    x = x | (x >> 1);    \
    x = x | (x >> 2);    \
    x = x | (x >> 4);    \
    x = x | (x >> 8);    \
    x = x | (x >>16);    \
    x + 1; })

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
  uint32_t rom_offset;
  uint32_t last_addr = -1;


  setup_pio_irq_logic();

  while (true) {
    while (pio0->fstat & (1u << (PIO_FSTAT_RXEMPTY_LSB + SM_WAITSEL)))
      tight_loop_contents();

    addrdata = pio0->rxf[SM_WAITSEL];
    //sio_hw->fifo_wr = addrdata;
    addr = addrdata & 0xFFFF;
    if (addr == last_addr)
      continue;

    data = (addrdata >> (18 + 4)) & 0xFF;

    if (IO_BASE <= addr && addr < IO_BASE + 4) {
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
    else if (MSX_PAGE_SIZE <= addr && addr < MSX_PAGE_SIZE * 3) {
      rom_offset = addr - MSX_PAGE_SIZE;
      rom_offset &= POW2_CEIL(sizeof(ROM)) - 1;
      pio0->txf[SM_READ] = ROM[rom_offset];
    }

    last_addr = addr;
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
  stdio_set_translate_crlf(&stdio_usb, false);

  while (!stdio_usb_connected())
    ;

  while (true) {
    if (multicore_fifo_rvalid()) {
      addrdata = multicore_fifo_pop_blocking();
      addr = addrdata & 0xFFFF;
      data = (addrdata >> (18 + 4)) & 0xFF;
      //printf("Received $%04x:$%02x\n", addr, data);
      putchar(data);
    }

    input = getchar_timeout_us(0);
    if (input != PICO_ERROR_TIMEOUT) {
      multicore_fifo_push_blocking(input);
      //printf("Input: $%02X\n", input);
    }
  }

  return 0;
}
