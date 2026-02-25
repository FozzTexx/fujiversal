#include "rom.h"
#include "FujiBusPacket.h"
#include "fujiDeviceID.h"
#include "fujiCommandID.h"

#include <stdio.h>
#include <string.h>
#include <pico/stdlib.h>
#include <pico/multicore.h>
#include <hardware/pio.h>
#include <hardware/irq.h>

#include <string>
#include <stdint.h>

typedef struct {
    uint32_t addr;
    uint8_t data;
} AddrData;

#if defined(BOARD_picorom)
#include "picorom.pio.h"
#elif defined(BOARD_msxrp2350)
#include "msxrp2350.pio.h"
#else
#error "No board defined"
#endif

#define IO_BASE    0xBFFC
#define IO_GETC    0
#define IO_STATUS  1
#define IO_PUTC    2
#define IO_CONTROL 3

#define MSX_PAGE_SIZE 0x4000
#define ROM disk_rom
#define ROM_SEG_SIZE 16384
#define ROM_MAX_SEGS 8
#define RAMROM_ACTIVATE_ADDR 0x4000

#define USE_IRQ 0

#define SM_WAITSEL 0
#define SM_READ    1

#define RING_SIZE 1024

#define POW2_CEIL(x_) ({      \
    unsigned int x = x_; \
    x -= 1;              \
    x = x | (x >> 1);    \
    x = x | (x >> 2);    \
    x = x | (x >> 4);    \
    x = x | (x >> 8);    \
    x = x | (x >>16);    \
    x + 1; })

#define ring_append(x) ({ring_buffer[ring_in] = x; \
      ring_in = (ring_in + 1) % sizeof(ring_buffer); })

uint8_t ramrom[ROM_MAX_SEGS * ROM_SEG_SIZE];
int ramrom_pos = -1;
uint8_t *ramrom_ptr = nullptr;
volatile bool ramrom_needs_activate = false;

#if USE_IRQ
bool selected = 0;

void __isr pio_irq_handler()
{
  if (pio_interrupt_get(pio0, 0)) {
    selected = 1;
    // Clear the PIO IRQ flag
    pio_interrupt_clear(pio0, 0);
  }
}
#endif

void setup_pio_irq_logic()
{
  pio_sm_config conf;
  uint offset;


  // Give control of DIR_PIN to PIO
  pio_gpio_init(pio0, DIR_PIN);

  // Set analog pins as digital
  for (int pin = D0_PIN; pin < D0_PIN + 8; pin++)
    pio_gpio_init(pio0, pin);

  // Invert /CE pin to make it easer to use JMP in PIO
  gpio_set_inover(CE_PIN, GPIO_OVERRIDE_INVERT);

  // Setup state machine that checks when we are selected
  offset = pio_add_program(pio0, &wait_sel_program);
  conf = wait_sel_program_get_default_config(offset);
  sm_config_set_in_pins(&conf, 0);
  sm_config_set_in_shift(&conf, true, true, 32);

  pio_sm_set_consecutive_pindirs(pio0, SM_WAITSEL, A0_PIN, ADDRESS_WIDTH, false);
  pio_sm_set_consecutive_pindirs(pio0, SM_WAITSEL, OE_PIN, 2, false);
  pio_sm_set_consecutive_pindirs(pio0, SM_WAITSEL, D0_PIN, DATA_WIDTH, false);

  pio_sm_init(pio0, SM_WAITSEL, offset, &conf);
  pio_sm_set_enabled(pio0, SM_WAITSEL, true);

#if USE_IRQ
  pio_set_irq0_source_enabled(pio0, pis_interrupt0, true);
  irq_set_exclusive_handler(PIO0_IRQ_0, pio_irq_handler);
  irq_set_enabled(PIO0_IRQ_0, true);
#endif

  // Setup state machine that handles CPU read by putting byte on bus
  offset = pio_add_program(pio0, &read_program);
  conf = read_program_get_default_config(offset);

  sm_config_set_out_pins(&conf, D0_PIN, DATA_WIDTH);
  pio_sm_set_consecutive_pindirs(pio0, SM_READ, DIR_PIN, 1, true);
  pio_sm_set_consecutive_pindirs(pio0, SM_READ, D0_PIN, DATA_WIDTH, false);
  sm_config_set_sideset_pins(&conf, DIR_PIN);
  sm_config_set_sideset(&conf, 2, true, false);  // 1-bit, optional = true, pindirs = false

  // Set JMP pin base to CE (or OE depending on your design)
  sm_config_set_jmp_pin(&conf, CE_PIN);     // e.g. GPIO20

  pio_sm_init(pio0, SM_READ, offset, &conf);
  pio_sm_set_enabled(pio0, SM_READ, true);

  return;
}

void __time_critical_func(romulan)(void)
{
  uint32_t addrdata;
  AddrData ad;
  uint32_t rom_offset, rom_size = POW2_CEIL(sizeof(ROM));
  uint8_t *rom_ptr = ROM;
  uint32_t last_addr = -1;


  setup_pio_irq_logic();

  while (true) {
    while (pio0->fstat & (1u << (PIO_FSTAT_RXEMPTY_LSB + SM_WAITSEL)))
      tight_loop_contents();

    addrdata = pio0->rxf[SM_WAITSEL];
    ad = decode_addrdata(addrdata);
    if (ad.addr == last_addr)
      continue;

    if (!ramrom_ptr && rom_ptr != ROM)
      rom_ptr == ROM;

    if (ramrom_needs_activate) {// && addr == RAMROM_ACTIVATE_ADDR) {
      printf("Activating RAM\n"); // FIXME - why is this print necessary?
      if (ramrom_ptr)
        rom_ptr = ramrom_ptr;
      ramrom_needs_activate = false;
    }

    // FIXME - only check IO_BASE if rom_ptr == ROM
    if (IO_BASE <= ad.addr && ad.addr < IO_BASE + 4) {
      switch (ad.addr & 0x3) {
      case IO_GETC: // Read byte
        pio0->txf[SM_READ] = sio_hw->fifo_rd;
        break;
      case IO_STATUS: // Read status reg
        pio0->txf[SM_READ] = sio_hw->fifo_st & SIO_FIFO_ST_VLD_BITS ? 0x80 : 0x00;
        break;
      case IO_PUTC: // Write byte
        sio_hw->fifo_wr = addrdata;
        break;
      case IO_CONTROL: // Write control reg
        break;
      }
    }
    else if (MSX_PAGE_SIZE <= ad.addr && ad.addr < MSX_PAGE_SIZE * 3) {
      rom_offset = ad.addr - MSX_PAGE_SIZE;
      //rom_offset &= POW2_CEIL(sizeof(ROM)) - 1;
      pio0->txf[SM_READ] = rom_ptr[rom_offset];
    }

    last_addr = ad.addr;
  }

  return;
}

void sendReplyPacket(fujiDeviceID_t source, bool ack, void *data, size_t length)
{
    FujiBusPacket packet(source, ack ? FUJICMD_ACK : FUJICMD_NAK,
                         ack ? std::string(static_cast<const char*>(data), length) : "");
    std::string encoded = packet.serialize();
    printf("Sending reply: dev:%02x cmd:%02x len:%04x\n",
           packet.device(), packet.command(), encoded.size());
    fwrite(encoded.data(), 1, encoded.size(), stdout);
    fflush(stdout);
    printf("Sent\n");
    return;
}

void process_command(std::string &buffer)
{
  auto packet = FujiBusPacket::fromSerialized(buffer);


  if (!packet) {
    printf("Failed to decode packet\n");
    return;
  }

  switch (packet->command()) {
  case FUJICMD_OPEN:
    {
      size_t offset = packet->param(0) * ROM_SEG_SIZE;
      offset %= sizeof(ramrom);
      printf("Opening RAM at 0x%04x\n", offset);
      ramrom_ptr = &ramrom[offset];
      ramrom_pos = 0;
      ramrom_needs_activate = false;
      sendReplyPacket(packet->device(), true, nullptr, 0);
    }
    break;

  case FUJICMD_WRITE:
    {
      if (ramrom_pos < 0 || !ramrom_ptr)
        sendReplyPacket(packet->device(), false, nullptr, 0);

      size_t len = std::min(packet->data()->size(), sizeof(ramrom) - ramrom_pos);
      printf("Writing %d bytes to 0x%04x\n", len, ramrom_pos);
      if (len) {
        memcpy(&ramrom_ptr[ramrom_pos], packet->data()->data(), len);
        ramrom_pos += len;
      }

      sendReplyPacket(packet->device(), true, nullptr, 0);
    }
    break;

  case FUJICMD_CLOSE:
    if (ramrom_pos < 0 || !ramrom_ptr)
      sendReplyPacket(packet->device(), false, nullptr, 0);

    ramrom_pos = -1;
    ramrom_needs_activate = true;
    printf("Closing RAM %d\n", ramrom_needs_activate);
    sendReplyPacket(packet->device(), true, nullptr, 0);
    break;

  case FUJICMD_RESET:
    ramrom_pos = -1;
    ramrom_ptr = nullptr;
    ramrom_needs_activate = false;
    break;

  default:
    // FIXME - nak
    break;
  }

  return;
}

int main()
{
  uint32_t addrdata;
  AddrData ad;
  int input;
  unsigned int count = 0;
  unsigned char ring_buffer[RING_SIZE];
  unsigned ring_in = 0, ring_out = 0;
  uint32_t last_cc_seen = 0, now;
  bool our_command = false;
  std::string command_buf;


  multicore_launch_core1(romulan);
  stdio_init_all();
  stdio_set_translate_crlf(&stdio_usb, false);

  while (!stdio_usb_connected())
    ;

  while (true) {
    if (multicore_fifo_rvalid()) {
      addrdata = multicore_fifo_pop_blocking();
      ad = decode_addrdata(addrdata);
      //printf("Received $%04x:$%02x\n", ad.addr, ad.data);
      putchar(ad.data);
    }

    if (command_buf.size()) {
      now = to_ms_since_boot(get_absolute_time());
      // Did we timeout waiting for final SLIP_END?
      if (now - last_cc_seen > 50) {
        printf("Command timeout\n");
        for (char c : command_buf)
          ring_append((uint8_t) c);
        command_buf.clear();
      }
    }

    input = getchar_timeout_us(0);
    if (input != PICO_ERROR_TIMEOUT) {
      if (!command_buf.size() && input != SLIP_END)
        ring_append(input);
      else {
        // if SLIP_END or already capturing then push to command_buf
        last_cc_seen = to_ms_since_boot(get_absolute_time());

        // Keep track of when last command char was seen so we can timeout
        command_buf.push_back((char) input);

        size_t command_size = command_buf.size();
        if (command_buf.size()) {
          // If second char is not a command for us, send command_buf to RBS
          if (command_size == 2 && input != FUJI_DEVICEID_DBC) {
            printf("Command not us\n");
            for (char c : command_buf)
              ring_append((uint8_t) c);
            command_buf.clear();
          }
          else if (command_size > 1 && input == SLIP_END) {
            process_command(command_buf);
            command_buf.clear();
          }
        }
      }
    }

    if (ring_in != ring_out) {
      bool sent = multicore_fifo_push_timeout_us(ring_buffer[ring_out], 0);
      if (sent)
        ring_out = (ring_out + 1) % sizeof(ring_buffer);
    }
  }

  return 0;
}
