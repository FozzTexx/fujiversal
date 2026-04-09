#include <stdint.h>

typedef struct {
  uint8_t device;
  uint8_t command;
  uint8_t aux_descr;
  uint8_t aux[4];
  void *buffer;
  uint16_t length;
} FujiNetParams;

extern uint8_t __FASTCALL__ fujiF5_write(FujiNetParams *params);
extern uint8_t __FASTCALL__ fujiF5_read(FujiNetParams *params);
