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

#if 1
void setup_pio_irq_logic()
{
  // 1. Load the PIO program into memory and get its starting address (offset)
  uint offset = pio_add_program(pio, &wait_sel_program);

  // 2. Get the default state machine configuration
  pio_sm_config c = wait_sel_program_get_default_config(offset);

#ifdef UNUSED
  // 3. CONFIGURE SPEED: Set the clock divider to 1.0f for maximum speed (~125 MHz)
  sm_config_set_clkdiv(&c, 1.0f);
#endif /* UNUSED */

  // 4. CONFIGURE PINS: Set the input base pin to first address pin
  sm_config_set_in_pins(&c, 0);

  // 5. Initialize the GPIO pins
  pio_gpio_init(pio, OE_PIN);
  pio_gpio_init(pio, CE_PIN);

  // 6. Tell the PIO both pins (2) are inputs, starting at OE_PIN
  pio_sm_set_consecutive_pindirs(pio, sm, OE_PIN, 2, false);

  // 7. Initialize and Enable the State Machine
  pio_sm_init(pio, sm, offset, &c);
  pio_sm_set_enabled(pio, sm, true);

  // 9. Tell the PIO to route IRQ 0 to the CPU
  pio_set_irq0_source_enabled(pio, pis_interrupt0, true);

  // 8. CONFIGURE IRQ: Set up the CPU interrupt handler
  irq_set_exclusive_handler(PIO0_IRQ_0, pio_irq_handler);
  irq_set_enabled(PIO0_IRQ_0, true);
}
#else
void setup_pio_irq_logic()
{
  uint offset = pio_add_program(PICO_PIO, &wait_sel_program);
  pio_sm_config c = wait_sel_program_get_default_config(offset);

  sm_config_set_in_pins(&c, 0);

#ifdef UNUSED
  pio_gpio_init(pio, OE_PIN);
  pio_gpio_init(pio, CE_PIN);
#elif 0
  gpio_init(OE_PIN);
  gpio_set_dir(OE_PIN, GPIO_IN);
  gpio_init(CE_PIN);
  gpio_set_dir(CE_PIN, GPIO_IN);
#endif /* UNUSED */

  // Initialize the state machine
  pio_sm_init(PICO_PIO, PICO_SM, offset, &c);
  pio_sm_set_enabled(PICO_PIO, PICO_SM, true);

  // **Step 1: Set up the handler in the NVIC**
  // Use `irq_set_exclusive_handler` to register your handler function.
  // PIO0_IRQ_0 handles the first four PIO IRQ flags (0-3).
  // PIO0_IRQ_1 handles the next four (4-7).
  irq_set_exclusive_handler(PIO0_IRQ_0, pio_irq_handler);

  // **Step 2: Enable the interrupt in the NVIC**
  // This allows the IRQ signal from the PIO to reach the processor core.
  irq_set_enabled(PIO0_IRQ_0, true);

  // **Step 3: Enable the interrupt source within the PIO block**
  // Your PIO program uses `IRQ_SEL`, which is 0.
  // We enable the corresponding PIO interrupt line to fire.
  pio_set_irq0_source_enabled(PICO_PIO, pis_interrupt0, true);
}
#endif

int main()
{
  unsigned int count = 0;


  stdio_init_all();

#if 1
  gpio_init(DIR_PIN);
  gpio_set_dir(DIR_PIN, GPIO_OUT);
  gpio_put(DIR_PIN, true);
  //gpio_put(DIR_PIN, false);

  setup_pio_irq_logic();

  printf("PIO program 'wait_sel' running at max speed, watching OE (GPIO %d) and CE (GPIO %d).\n", OE_PIN, CE_PIN);

  while (true) {
    if (selected) {
      printf("IRQ Fired! Both OE (GPIO %d) and CE (GPIO %d) are LOW.\n", OE_PIN, CE_PIN);
      selected = 0;
    }
    printf("Waiting %u\n", count++);
    sleep_ms(1000);
  }
#else
  while (true) {
    printf("Testing USB link...\n");
    sleep_ms(1000);
  }
#endif
}
