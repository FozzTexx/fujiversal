#include <stdio.h> // debug

#include "fuji_call.h"
#include "portio.h"
#include <string.h>

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

typedef struct {
  uint8_t device;   /* Destination Device */
  uint8_t command;  /* Command */
  uint16_t length;  /* Total length of packet including header */
  uint8_t checksum; /* Checksum of entire packet */
  uint8_t fields;   /* Describes the fields that follow */
} fujibus_header;

typedef struct {
  fujibus_header header;
  uint8_t data[];
} fujibus_packet;

// FIXME - use dual buffer slip codec and get rid of MAX_PACKET
#define MAX_PACKET      (512 + sizeof(fujibus_header) + 4) // sector + header + secnum
static uint8_t fb_buffer[MAX_PACKET * 2 + 2];              // Enough room for SLIP encoding
static fujibus_packet *fb_packet;

static const uint8_t fuji_field_numbytes_table[] = {0, 1, 2, 3, 4, 2, 4, 4};
#define fuji_field_numbytes(descr) fuji_field_numbytes_table[descr]

#include <stdint.h>

uint8_t msx_get_page_slot(uint8_t page)
{
  uint8_t v = inp(0xA8);


  return (v >> (page * 2)) & 0x03;
}

void msx_set_page_slot(uint8_t page, uint8_t slot)
{
  uint8_t v = inp(0xA8);
  uint8_t shift = page * 2;
  uint8_t mask = 0x03 << shift;


  v = (v & ~mask) | ((slot & 0x03) << shift);
  outp(0xA8, v);
}

// FIXME - get rid of these and use dual buffer slip codec during read/write

enum {
  SLIP_END     = 0xC0,
  SLIP_ESCAPE  = 0xDB,
  SLIP_ESC_END = 0xDC,
  SLIP_ESC_ESC = 0xDD,
};

/* This function expects that fb_packet is one byte into fb_buffer so
   that there's already room at the front for the SLIP_END framing
   byte. This allows skipping moving all the bytes if no escaping is
   needed. */
uint16_t fuji_slip_encode()
{
  uint16_t idx, len, enc_idx, esc_count, esc_remain;
  uint8_t ch, *ptr;


  // Count how many bytes need to be escaped
  len = fb_packet->header.length;
  ptr = (uint8_t *) fb_packet;
  for (idx = esc_count = 0; idx < len; idx++) {
    if (ptr[idx] == SLIP_END || ptr[idx] == SLIP_ESCAPE)
      esc_count++;
  }

#ifdef DEBUG
  printf("ESC count: %d %d\n", esc_count, len);
#endif /* DEBUG */
  if (esc_count) {
    // Encode buffer in place working from back to front
    for (esc_remain = esc_count, enc_idx = esc_count + (idx = len - 1);
         esc_remain;
         idx--, enc_idx--) {
      ch = ptr[idx];
      if (ch == SLIP_END) {
        ptr[enc_idx--] = SLIP_ESC_END;
        ch = SLIP_ESCAPE;
        esc_remain--;
      }
      else if (ch == SLIP_ESCAPE) {
        ptr[enc_idx--] = SLIP_ESC_ESC;
        ch = SLIP_ESCAPE;
        esc_remain--;
      }

      ptr[enc_idx] = ch;
    }
  }

  // FIXME - this byte probably never changes, maybe we should init fb_buffer with it?
  fb_buffer[0] = SLIP_END;
  fb_buffer[1 + len + esc_count] = SLIP_END;
  return 2 + len + esc_count;
}

uint16_t fuji_slip_decode(uint16_t len)
{
  uint16_t idx, dec_idx, esc_count;
  uint8_t *ptr;


  ptr = (uint8_t *) fb_packet;
  for (idx = dec_idx = 0; idx < len; idx++, dec_idx++) {
    if (ptr[idx] == SLIP_END)
      break;

    if (ptr[idx] == SLIP_ESCAPE) {
      idx++;
      if (ptr[idx] == SLIP_ESC_END)
        ptr[dec_idx] = SLIP_END;
      else if (ptr[idx] == SLIP_ESC_ESC)
        ptr[dec_idx] = SLIP_ESCAPE;
    }
    else if (idx != dec_idx) {
      // Only need to move byte if there were escapes decoded
      ptr[dec_idx] = ptr[idx];
    }
  }

  return dec_idx;
}

static uint8_t fuji_calc_checksum(void *ptr, uint16_t len)
{
  uint16_t idx, chk;
  uint8_t *buf = (uint8_t *) ptr;


  for (idx = chk = 0; idx < len; idx++)
    chk = ((chk + buf[idx]) >> 8) + ((chk + buf[idx]) & 0xFF);
  return (uint8_t) chk;
}

static uint8_t fuji_bus_call(AtariSIODirection direction, FujiNetParams *params)
{
  int code;
  uint8_t ck1, ck2;
  uint16_t rlen, plen;
  uint16_t idx, numbytes;
  uint8_t saved_slot, my_slot;
  uint8_t *pbuf;
  bool success = false;


  printf("UNAPI FUJINET BUS CALL 0x%02x PARAMS 0x%04x\n", direction, (uint16_t) params);
#ifdef DEBUG
  hexdump(params, sizeof(*params));
#endif /* DEBUG */

  pbuf = params->buffer;
  plen = params->length;

  fb_packet = (fujibus_packet *) (fb_buffer + 1); // +1 for SLIP_END
#ifdef DEBUG
  printf("buf: 0x%04x pak: 0x%04x\n", fb_buffer, fb_packet);
  printf("fields: %d\n", params->aux_descr);
#endif /* DEBUG */
  fb_packet->header.device = params->device;
  fb_packet->header.command = params->command;
  fb_packet->header.length = sizeof(fujibus_header);
  fb_packet->header.checksum = 0;
  fb_packet->header.fields = params->aux_descr;
#if 0 //def DEBUG
  printf("Header len %d %d\n", fb_packet->header.length, sizeof(fujibus_header));
  hexdump((uint8_t *) fb_packet, sizeof(fujibus_header));
#endif /* DEBUG */

  idx = 0;
  numbytes = fuji_field_numbytes(params->aux_descr);
#if 0 //def DEBUG
  printf("numbytes: %d %d\n", fields, numbytes);
  hexdump(fuji_field_numbytes_table, 8);
#endif /* DEBUG */
  if (numbytes) {
    fb_packet->data[idx++] = params->aux1;
    numbytes--;
  }
  if (numbytes) {
    fb_packet->data[idx++] = params->aux2;
    numbytes--;
  }
  if (numbytes) {
    fb_packet->data[idx++] = params->aux3;
    numbytes--;
  }
  if (numbytes) {
    fb_packet->data[idx++] = params->aux4;
    numbytes--;
  }
  if (direction == SIO_DIRECTION_WRITE) {
    memcpy(&fb_packet->data[idx], pbuf, plen);
    idx += plen;
  }
#ifdef DEBUG
  printf("Fields + data %d %d\n", numbytes, idx);
#endif /* DEBUG */

  fb_packet->header.length += idx;
#ifdef DEBUG
  printf("Packet len %d\n", fb_packet->header.length);
#endif /* DEBUG */

  ck1 = fuji_calc_checksum(fb_packet, fb_packet->header.length);
#if 0 //def DEBUG
  printf("Checksum: 0x%02x\n", ck1);
  hexdump((uint8_t *) fb_packet, sizeof(fujibus_header));
#endif /* DEBUG */
  fb_packet->header.checksum = ck1;

  numbytes = fuji_slip_encode();

  // Page in memory mapped IO
  my_slot = msx_get_page_slot(1);
  saved_slot = msx_get_page_slot(2);
  msx_set_page_slot(2, my_slot);

#ifdef DEBUG
  printf("Sending packet %d\n", numbytes);
  //hexdump(fb_buffer, numbytes);
#endif /* DEBUG */
  port_putbuf(fb_buffer, numbytes);
#if 0
  code = port_discard_until(SLIP_END, TIMEOUT_SLOW);
#else
  while (1) {
    code = port_getc_timeout(TIMEOUT_SLOW);
#if 0 //def DEBUG
    printf("%02x ", code);
#endif /* DEBUG */
    if (code < 0 || code == SLIP_END)
      break;
  }
#endif // 0

  if (code != SLIP_END) {
#ifdef DEBUG
    printf("NO SLIP FRAME %d\n", code);
#endif /* DEBUG */
    success = false;
    goto done;
  }

  rlen = port_get_until(fb_packet, (fb_buffer + sizeof(fb_buffer)) - ((uint8_t *) fb_packet),
                 SLIP_END, TIMEOUT_SLOW);
#if 0 //def DEBUG
  printf("Packet reply: %d\n", rlen);
  hexdump((uint8_t *) fb_packet, sizeof(fujibus_header));
#endif /* DEBUG */
  rlen = fuji_slip_decode(rlen);
#if 0 //def DEBUG
  printf("Decode len: %d %d\n", rlen, fb_packet->header.length);
  hexdump((uint8_t *) fb_packet, sizeof(fujibus_header));
#endif /* DEBUG */
  if (rlen != fb_packet->header.length) {
#ifdef DEBUG
    printf("Reply length incorrect: %d %d\n", rlen, fb_packet->header.length);
#endif /* DEBUG */
    success = false;
    goto done;
  }
#ifdef DEBUG
  if (rlen - sizeof(fujibus_header) != plen) {
    printf("Expected length incorrect: %d %d\n", rlen - sizeof(fujibus_header), plen);
    hexdump(params, sizeof(*params));
  }
#endif /* DEBUG */

  // Need to zero out checksum in order to calculate
  ck1 = fb_packet->header.checksum;
  fb_packet->header.checksum = 0;
  ck2 = fuji_calc_checksum(fb_packet, rlen);
  if (ck1 != ck2) {
#ifdef DEBUG
    printf("Checksum mismatch: %02x %02x\n", ck1, ck2);
#endif /* DEBUG */
    success = false;
    goto done;
  }

  if (fb_packet->header.command != PACKET_ACK) {
    success = false;
    goto done;
  }

  // FIXME - validate that fb_packet->fields is zero?

  if (direction == SIO_DIRECTION_READ && rlen) {
    if (plen < rlen)
      rlen = plen;
    memcpy(pbuf, fb_packet->data, rlen);
  }

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

