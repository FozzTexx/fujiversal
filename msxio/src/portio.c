#include "portio.h"
#include <arch/z80.h>

#define IOPORT 0xD0
#define PORTA IOPORT
#define PORTB IOPORT+1
#define PORTC IOPORT+2
#define PCTRL IOPORT+3

#define OUTBUF_FULL 0x80 // i8255 wants to send, /OBF, output active low
#define OUTBUF_ACK  0x40 // ESP32 received the byte, /ACK, input active low
#define INBUF_FULL  0x20 // i8255 received the byte, IBF, output active high
#define INBUF_GET   0x10 // ESP32 wants to send, /STB, input active low

void port_init()
{
  z80_outp(PCTRL,0xC0);
  z80_outp(PORTC,0x06);
  z80_outp(PORTC,0x02);
  z80_outp(PORTC,0x06);
  msleep(200);
  z80_inp(PORTA); // Flush input

  z80_outp(PCTRL,0x00);
  z80_outp(PCTRL,0x02);
  return;
}

int port_getc()
{
  int b = -1;
  int c = z80_inp(PORTC);

  if (c & INBUF_FULL) {
    b = z80_inp(PORTA);
  }

  return b;
}

int port_getc_timeout(uint16_t timeout)
{
  int b;

  // FIXME - actually timeout
  while ((b = port_getc()) < 0)
    ;
  return b;
}

uint16_t port_getbuf(void *buf, uint16_t len, uint16_t timeout)
{
  uint16_t idx;
  int b;
  uint8_t *ptr = (uint8_t *) buf;

`
  for (idx = 0; idx < len; idx++) {
    b = port_getc_timeout(timeout);
    if (b < 0)
      break;
    ptr[idx] = b;
  }

  return idx;
}

void port_putc(uint8_t c)
{
  while (z80_inp(PORTC) & OUTBUF_ACK); // Wait for ready to handle byte
  z80_outp(PORTA,c);
  return;
}

uint16_t port_putbuf(void *buf, uint16_t len)
{
  uint16_t idx;
  uint8_t *ptr = (uint8_t *) buf;


  for (idx = 0; idx < len; idx++)
    port_putc(ptr[idx]);
  return idx;
}
