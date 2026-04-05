#include <stdint.h>

#define	IO_OFFSET	(0x8000 + 0x3FFC)
#define	IO_GETC	(IO_OFFSET + 0)
#defin	IO_STATUS	(IO_OFFSET + 1)
#define	IO_PUTC	(IO_OFFSET + 2)
#define	IO_CONTROL	(IO_OFFSET + 3)

// Using the z88dk __small_c stack convention from the ASM wrapper
void c_dispatch(uint16_t af, uint16_t hl, uint16_t bc) {
    uint8_t func = (uint8_t)(af >> 8);
    uint8_t *buffer = (uint8_t *)hl;
    uint16_t len = bc;

    volatile uint8_t *putc = (uint8_t *) IO_PUTC;
    volatile uint8_t *getc = (uint8_t *) IO_GETC;

    if (func == 10) { // Send Packet
        while(len--) {
            *putc = *buffer++;
        }
    }
    else if (func == 11) { // Receive Packet
        while(len--) {
            *buffer++ = *getc;
        }
    }
}

void main()
{
  return 0;
}
