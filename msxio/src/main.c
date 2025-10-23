#include "portio.h"
#include "hexdump.h"
#include <stdio.h>
#include <input.h>

char buffer[512];

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

    rlen = port_getbuf(buffer, sizeof(buffer), 60);
    if (rlen) {
      printf("count: %u\n", rlen);
#if 0
      for (idx = 0; idx < rlen; idx++)
        printf("$%02X ", buffer[idx]);
      printf("\n");
#endif
      if (rlen <= 16)
        hexdump(buffer, rlen);
      printf("sending back\n");
      port_putbuf(buffer, rlen);
    }
    else
      printf("timeout\n");
  }

  return;
}
