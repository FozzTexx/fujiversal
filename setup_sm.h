#ifndef SETUP_SM_H
#define SETUP_SM_H

#include <pico/stdlib.h>
#include <hardware/pio.h>

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

typedef struct {
  uint base;
  uint count;
  bool inverted;
} pin_range_t;

typedef struct {
  const pio_program_t *program;
  pio_sm_config (*get_default_config)(uint offset);
#if 0
  uint sm_num;
#endif

  const pin_range_t *input_pins;
  uint input_count;
  const pin_range_t *output_pins;
  uint output_count;

  int in_instr_base;   // -1 = skip
  int out_instr_base;  // -1 = skip, for sm_config_set_out_pins
  uint out_count;
  uint push_threshold; // 0 = skip

  int sideset_base;    // -1 = skip
  int sideset_count;
  bool sideset_opt;

  int jmp_pin;         // -1 = skip
} sm_setup_t;

typedef struct {
  PIO pio;
  uint sm;
} pio_sm_t;

extern int setup_state_machine(pio_sm_t *pio_sm, const sm_setup_t *cfg);

#endif /* SETUP_SM_H */
