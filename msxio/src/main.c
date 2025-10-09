#include "portio.h"
#include <stdio.h>
#include <input.h>

#define IO_OFFSET  (0x4000 + 0x3FFC)
#define IO_GETC    (IO_OFFSET + 0)
#define IO_STATUS  (IO_OFFSET + 1)
#define IO_PUTC    (IO_OFFSET + 2)
#define IO_CONTROL (IO_OFFSET + 3)

char buffer[32];

void main()
{
  int v;
  unsigned char c;
  unsigned int rlen, idx;


  printf("Waiting for data\n");
  while (1) {
    c = getk();
    if (c) {
      printf("Key $%02X\n", c);
      port_putc(c);
    }

#if 0
    c = port_getc();
    if (c != -1)
      printf("Received: $%02X\n", c);
#elif 0
    c = *((unsigned char *) IO_STATUS);
    if (c & 0x80) {
      printf("S: %02x\n", c);
      c = *((unsigned char *) IO_GETC);
      printf("read: $%02X\n", c);
    }
#elif 0
    c = port_getc_timeout(60);
    if (c != -1)
      printf("RX: $%02X\n", c);
  #if 0
    else
      printf("timeout\n");
  #endif
#elif 1
    rlen = port_getbuf(buffer, 16, 60);
    if (rlen) {
      printf("count: %u\n", rlen);
      for (idx = 0; idx < rlen; idx++)
        printf("$%02X ", buffer[idx]);
      printf("\n");
    }
    else
      printf("timeout\n");
#endif
  }

  return;
}
