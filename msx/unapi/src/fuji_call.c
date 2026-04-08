#include <stdio.h> // debug

#include "fuji_call.h"
#include "portio.h"
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#undef DEBUG
#define HEXDUMP 1

#if defined(DEBUG) && defined(HEXDUMP)
#define COLUMNS 16

static void hexdump(uint8_t *buffer, int count)
{
  int outer, inner;
  uint8_t c;


  for (outer = 0; outer < count; outer += COLUMNS) {
    for (inner = 0; inner < COLUMNS; inner++) {
      if (inner + outer < count) {
	c = buffer[inner + outer];
	printf("%02x ", c);
      }
      else
	printf("   ");
    }
    printf(" |");
    for (inner = 0; inner < COLUMNS && inner + outer < count; inner++) {
      c = buffer[inner + outer];
      if (c >= ' ' && c <= 0x7f)
	printf("%c", c);
      else
	printf(".");
    }
    printf("|\n");
  }

  return;
}
#endif /* HEXDUMP */

// Yes, using SIO convention internally! Legacy FTW! ;-)
typedef enum {
    SIO_DIRECTION_NONE    = 0x00,
    SIO_DIRECTION_READ    = 0x40,
    SIO_DIRECTION_WRITE   = 0x80,
    SIO_DIRECTION_INVALID = 0xFF,
} AtariSIODirection;

#define milliseconds_to_jiffy(millis) ((millis) / (VDP_IS_PAL ? 20 : 1000 / 60))

#define TIMEOUT         milliseconds_to_jiffy(100)
#define TIMEOUT_SLOW	milliseconds_to_jiffy(15 * 1000)

#define false 0
#define true 1

enum {
  PACKET_ACK = 6, // ASCII ACK
  PACKET_NAK = 21, // ASCII NAK
};

enum {
  SLIP_END     = 0xC0,
  SLIP_ESCAPE  = 0xDB,
  SLIP_ESC_END = 0xDC,
  SLIP_ESC_ESC = 0xDD,
};

typedef struct {
  uint8_t device;   /* Destination Device */
  uint8_t command;  /* Command */
  uint16_t length;  /* Total length of packet including header */
  uint8_t checksum; /* Checksum of entire packet */
  uint8_t fields;   /* Describes the fields that follow */
} fujibus_header;

typedef struct {
  fujibus_header header;
  uint8_t data[4]; // max 4 aux bytes
} fujibus_packet;

static const uint8_t fuji_field_numbytes_table[] = {0, 1, 2, 3, 4, 2, 4, 4};
#define fuji_field_numbytes(descr) fuji_field_numbytes_table[descr]

static uint8_t msx_get_page_slot(uint8_t page)
{
  uint8_t v = inp(0xA8);


  return (v >> (page * 2)) & 0x03;
}

static void msx_set_page_slot(uint8_t page, uint8_t slot)
{
  uint8_t v = inp(0xA8);
  uint8_t shift = page * 2;
  uint8_t mask = 0x03 << shift;


  v = (v & ~mask) | ((slot & 0x03) << shift);
  outp(0xA8, v);
}

static uint16_t fuji_calc_checksum(const void *ptr, uint16_t len, uint16_t seed)
{
  uint16_t idx, chk;
  uint8_t *buf = (uint8_t *) ptr;


  for (idx = 0, chk = seed; idx < len; idx++)
    chk = ((chk + buf[idx]) >> 8) + ((chk + buf[idx]) & 0xFF);
  return chk;
}

static uint8_t fuji_bus_call(AtariSIODirection direction, FujiNetParams *params)
{
  int code;
  uint8_t ck1, ck2, pdev;
  uint16_t rlen, plen;
  uint16_t idx, numbytes;
  uint8_t saved_slot, my_slot;
  uint8_t *pbuf;
  fujibus_packet fb_packet;
  bool success = false;


  printf("UNAPI FUJINET BUS CALL 0x%02x PARAMS 0x%04x\n", direction, (uint16_t) params);

  pbuf = params->buffer;
  plen = direction != SIO_DIRECTION_NONE ? params->length : 0;

  fb_packet.header.device = pdev = params->device;
  fb_packet.header.command = params->command;
  fb_packet.header.length = sizeof(fujibus_header);
  fb_packet.header.checksum = 0;
  fb_packet.header.fields = params->aux_descr;

  for (idx = 0, numbytes = fuji_field_numbytes(params->aux_descr); numbytes; numbytes--, idx++)
    fb_packet.data[idx] = params->aux[idx];

  // Data is spread across two buffers: ours and data
  ck1 = fuji_calc_checksum(&fb_packet, sizeof(fb_packet.header) + idx, 0);

  if (direction == SIO_DIRECTION_WRITE)
    ck1 = fuji_calc_checksum(pbuf, plen, ck1);

  fb_packet.header.checksum = ck1;

  fb_packet.header.length += idx;
#ifdef DEBUG
  printf("Packet len %d\n", fb_packet.header.length);
#endif /* DEBUG */

  // Page in memory mapped IO
  my_slot = msx_get_page_slot(1);
  saved_slot = msx_get_page_slot(2);
  msx_set_page_slot(2, my_slot);

  port_putc(SLIP_END);
  port_putbuf_slip(&fb_packet, idx + sizeof(fb_packet.header));
  if (direction == SIO_DIRECTION_WRITE)
    port_putbuf_slip(pbuf, plen);
  port_putc(SLIP_END);

  if (direction != SIO_DIRECTION_READ) {
    pbuf = NULL;
    plen = 0;
  }
  rlen = port_getbuf_slip_dual(&fb_packet, sizeof(fb_packet.header), pbuf, plen, TIMEOUT_SLOW);
  if (rlen < sizeof(fujibus_header) || rlen != fb_packet.header.length) {
#ifdef DEBUG
    printf("Reply length incorrect: %d %d\n", rlen, fb_packet.header.length);
    hexdump((uint8_t *) &fb_packet, sizeof(fujibus_header));
#endif /* DEBUG */
    success = false;
    goto done;
  }
#ifdef DEBUG
  if (rlen - sizeof(fujibus_header) != plen) {
    printf("Expected length incorrect: %d %d\n", rlen - sizeof(fujibus_header), plen);
    hexdump((uint8_t *) params, sizeof(*params));
  }
#endif /* DEBUG */

  // Need to zero out checksum in order to calculate
  ck1 = fb_packet.header.checksum;
  fb_packet.header.checksum = 0;

  // Data is spread across two buffers: ours and reply
  ck2 = fuji_calc_checksum(&fb_packet, sizeof(fb_packet.header), 0);
  if (direction == SIO_DIRECTION_READ)
    ck2 = fuji_calc_checksum(pbuf, rlen - sizeof(fb_packet.header), ck2);
  ck2 = (uint8_t) ck2;

  if (ck1 != ck2) {
#ifdef DEBUG
    printf("Checksum mismatch: 0x%02x 0x%02x\n", ck1, ck2);
#endif /* DEBUG */
    success = false;
    goto done;
  }

  if (fb_packet.header.device != pdev) {
#ifdef DEBUG
    printf("Incorrect device: R:0x%02x E:0x%02x\n", fb_packet.header.device, pdev);
    hexdump((uint8_t *) &fb_packet, sizeof(fb_packet.header));
#endif /* DEBUG */
    success = false;
    goto done;
  }

  if (fb_packet.header.command != PACKET_ACK) {
#ifdef DEBUG
    printf("Not ACK: 0x%02x\n", fb_packet.header.command);
#endif /* DEBUG */
    success = false;
    goto done;
  }

  // FIXME - validate that fb_packet.fields is zero?

  success = true;

 done:
  // Restore whatever was in page 2
  msx_set_page_slot(2, saved_slot);
  return success;
}

uint8_t __FASTCALL__ fujiF5_none(FujiNetParams *params)
{
  return fuji_bus_call(SIO_DIRECTION_NONE, params);
}

uint8_t __FASTCALL__ fujiF5_write(FujiNetParams *params)
{
  return fuji_bus_call(SIO_DIRECTION_WRITE, params);
}

uint8_t __FASTCALL__ fujiF5_read(FujiNetParams *params)
{
  return fuji_bus_call(SIO_DIRECTION_READ, params);
}

