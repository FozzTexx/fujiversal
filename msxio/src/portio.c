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

#define DATA_DIR    0x01
#define DATA_EN     0x02
#define ESP32_EN    0x04

#define I8255_G2_PORTC_INPUT 0x01
#define I8255_G2_PORTB_INPUT 0x02
#define I8255_G2_MODE_1      0x04
#define I8255_G1_PORTC_INPUT 0x08
#define I8255_G1_PORTA_INPUT 0x10
#define I8255_G1_MODE_1      0x20
#define I8255_G1_MODE_2      0x40
#define I8255_MODE_ACTIVE    0x80

void port_init()
{
  z80_outp(PCTRL,I8255_MODE_ACTIVE | I8255_G1_MODE_2);
  z80_outp(PORTC,DATA_EN | ESP32_EN);
  z80_outp(PORTC,DATA_EN);
  z80_outp(PORTC,DATA_EN | ESP32_EN);
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
    int b=-1;
    uint16_t start;


    // Store start time so we can calculate delta even when TIKCNT overflows
    start = wpeek(TIKCNT);

    while ((wpeek(TIKCNT) - start) < timeout)
    {
        b = port_getc();
        if (b > -1)
            return b;
    }
    return b;
}

uint16_t port_getbuf(void *buf, uint16_t len, uint16_t timeout)
{
  uint16_t idx;
  int b;
  uint8_t *ptr = (uint8_t *) buf;

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
