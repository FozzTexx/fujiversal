#include "setup_sm.h"

//#define LED_PIN 39

void sm_get_min_max_gpio(uint *gpio_base, uint *gpio_top,
                         const pin_range_t *ranges, uint range_count)
{
  uint idx;
  uint base, top;


  for (base = *gpio_base, top = *gpio_top, idx = 0; idx < range_count; idx++) {
    if (ranges[idx].base < base)
      base = ranges[idx].base;
    if (ranges[idx].base + ranges[idx].count > top)
      top = ranges[idx].base + ranges[idx].count;
  }

  *gpio_base = base;
  *gpio_top = top;
  return;
}

void sm_gpio_init(PIO pio, uint sm, uint base, uint count, bool inverted, bool out)
{
  uint idx;


  for (idx = 0; idx < count; idx++) {
    pio_gpio_init(pio, base + idx);
    gpio_disable_pulls(base + idx);
    if (inverted)
      gpio_set_inover(base + idx, GPIO_OVERRIDE_INVERT);
  }
  pio_sm_set_consecutive_pindirs(pio, sm, base, count, out);

  return;
}

int setup_state_machine(pio_sm_t *pio_sm, const sm_setup_t *cfg)
{
  uint idx;
  uint gpio_base, gpio_top;
  uint offset;
  pio_sm_config conf;
  static uint next_sm = 0;


#ifdef LED_PIN
  gpio_init(LED_PIN);
  gpio_set_dir(LED_PIN, GPIO_OUT);
#endif // LED_PIN

  pio_sm->pio = (PIO) -1;

  // Find the lowest and highest GPIO required
  gpio_base = 255;
  gpio_top = 0;
  sm_get_min_max_gpio(&gpio_base, &gpio_top, cfg->input_pins, cfg->input_count);
  sm_get_min_max_gpio(&gpio_base, &gpio_top, cfg->output_pins, cfg->output_count);

  if (gpio_top <= gpio_base)
    return 2;

  if (gpio_base < 16 && gpio_top > 32)
    return 6;

#if PICO_RP2040
  pio_sm->pio = pio0;
  if (!pio_can_add_program(pio_sm->pio, cfg->program)) {
    pio_sm->pio = NULL;
    return 4;
  }

  offset = pio_add_program(pio_sm->pio, cfg->program);
  pio_sm->sm = pio_claim_unused_sm(pio_sm->pio, true);
  if (pio_sm->sm < 0)
    return 5;

#else // ! PICO_RP2040
  if (!pio_claim_free_sm_and_add_program_for_gpio_range(cfg->program,
                                                        &pio_sm->pio, &pio_sm->sm,
                                                        &offset,
                                                        gpio_base, gpio_top - gpio_base,
                                                        true)
      || pio_sm->pio == NULL) {
#ifdef LED_PIN
    gpio_put(LED_PIN, 1);
#endif // LED_PIN
    return 3;
  }
#endif // PICO_RP2040

  conf = cfg->get_default_config(offset);

  for (idx = 0; idx < cfg->input_count; idx++) {
    sm_gpio_init(pio_sm->pio, pio_sm->sm, cfg->input_pins[idx].base,
                 cfg->input_pins[idx].count, cfg->input_pins[idx].inverted,
                 GPIO_IN);
  }

  for (idx = 0; idx < cfg->output_count; idx++) {
    sm_gpio_init(pio_sm->pio, pio_sm->sm, cfg->output_pins[idx].base,
                 cfg->output_pins[idx].count, cfg->output_pins[idx].inverted,
                 GPIO_OUT);
  }

  if (cfg->out_instr_base >= 0)
    sm_config_set_out_pins(&conf, cfg->out_instr_base, cfg->out_count);

  if (cfg->sideset_base >= 0) {
    sm_config_set_sideset_pins(&conf, cfg->sideset_base);
    sm_config_set_sideset(&conf, cfg->sideset_count + 1, cfg->sideset_opt, false);
  }

  if (cfg->jmp_pin >= 0)
    sm_config_set_jmp_pin(&conf, cfg->jmp_pin);

  if (cfg->in_instr_base >= 0)
    sm_config_set_in_pins(&conf, cfg->in_instr_base);

  if (cfg->push_threshold > 0)
    sm_config_set_in_shift(&conf, true, true, cfg->push_threshold);

  int err = pio_sm_init(pio_sm->pio, pio_sm->sm, offset, &conf);
  if (!err)
    pio_sm_set_enabled(pio_sm->pio, pio_sm->sm, true);

  return err;
}
