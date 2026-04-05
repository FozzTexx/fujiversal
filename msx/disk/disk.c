#include "io.h"

__asm
    ORG $4000
__endasm;

#define BANNER "Hello FujiNet!"

void DISKIO()
{
}

void DISKCHG()
{
}

void GETDPB()
{
}

void CHOICE()
{
}

void DSKFMT()
{
}

void DSKSTP()
{
}

void INIT()
{
  unsigned int idx;


  for (idx = 0; BANNER[idx]; idx++)
    port_putc(BANNER[idx]);
  return;
}

#ifdef UNUSED
int port_getc()
{
  return *((unsigned char *) (0x4000 + 0x3FFC));
}

void port_putc(unsigned char val)
{
  *((unsigned char *) (0x4000 + 0x3FFE)) = val;
  return;
}
#endif /* UNUSED */
