#include <stdint.h>

extern int port_getc();
extern int port_getc_timeout(uint16_t timeout);
extern uint16_t port_getbuf(void *buf, uint16_t len, uint16_t timeout);

extern void port_putc(uint8_t c);
extern uint16_t port_putbuf(void *buf, uint16_t len);
