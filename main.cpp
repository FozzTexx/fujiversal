#include "bus.pio.h"
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
#include <hardware/watchdog.h>

#include <string>

#define BECKER_REV0 0

#define HEARTBEAT 1

#define IO_BASE    0xFF41
#define IO_GETC    1
#define IO_STATUS  0
#define IO_PUTC    3
#define IO_CONTROL 2
#ifdef RW_PIN
#define IO_TOP     (IO_BASE + 2)
#else
#define IO_TOP     (IO_BASE + 4)
#endif

#define IO_FLAG_AVAIL   0x02

#define COCO_ROM_BASE 0xC000
#define COCO_ROM_TOP  0xFF00
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

#if BECKER_REV0
typedef union {
  struct {
    uint32_t data:DATA_WIDTH;
    uint32_t addr:ADDR_WIDTH;
    uint32_t rw:1;
    uint32_t clk:1;
    uint32_t cts:1;
    uint32_t nmi:1;
  } __attribute__((packed));
  uint32_t combined;
} __attribute__((packed)) BusSignals;
#else // ! BECKER_REV0
typedef union {
  struct {
    uint32_t addr:ADDR_WIDTH;
    uint32_t clk:1;
    uint32_t rw:1;
    uint32_t expand:1;
    uint32_t dir:1;
    uint32_t cts:1;
    uint32_t scs:1;
    uint32_t data:DATA_WIDTH;
  } __attribute__((packed));
  uint32_t combined;
} __attribute__((packed)) BusSignals;
#endif // BECKER_REV0

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


  // Init output pins
  for (int pin = D0_PIN; pin < D0_PIN + 8; pin++)
    pio_gpio_init(pio0, pin);
#ifdef DIR_PIN
  pio_gpio_init(pio0, DIR_PIN);
#endif // DIR_PIN

#if INVERT_PINS
  // Invert /SCS and /CTS pins to make it easer to use JMP in PIO
  gpio_set_inover(CTS_PIN, GPIO_OVERRIDE_INVERT);
#ifdef SCS_PIN
  gpio_set_inover(SCS_PIN, GPIO_OVERRIDE_INVERT);
#else // ! SCS_PIN
  // Invert A15 and A14 pins to make it easer to use JMP in PIO
  gpio_set_inover(A14_PIN, GPIO_OVERRIDE_INVERT);
  gpio_set_inover(A15_PIN, GPIO_OVERRIDE_INVERT);
#endif // SCS_PIN
#endif // INVERT_PINS

  // Setup state machine that checks when we are selected
  offset = pio_add_program(pio0, &wait_sel_program);
  conf = wait_sel_program_get_default_config(offset);
  sm_config_set_in_pins(&conf, 0);
  sm_config_set_in_shift(&conf, true, true, 32);

#if BECKER_REV0
  pio_sm_set_consecutive_pindirs(pio0, SM_WAITSEL, A0_PIN, ADDR_WIDTH, false);
  pio_sm_set_consecutive_pindirs(pio0, SM_WAITSEL, RW_PIN, 4, false);
#else
  pio_sm_set_consecutive_pindirs(pio0, SM_WAITSEL, A0_PIN, 18, false);
  pio_sm_set_consecutive_pindirs(pio0, SM_WAITSEL, CTS_PIN, 2, false);
#endif // BECKER_REV0
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

  sm_config_set_out_pins(&conf, D0_PIN, 8);
  pio_sm_set_consecutive_pindirs(pio0, SM_READ, D0_PIN, DATA_WIDTH, false);
#ifdef DIR_PIN
  pio_sm_set_consecutive_pindirs(pio0, SM_READ, DIR_PIN, 1, true);
  sm_config_set_sideset_pins(&conf, DIR_PIN);
  sm_config_set_sideset(&conf, 2, true, false);  // 1-bit, optional = true, pindirs = false
#endif // DIR_PIN

  sm_config_set_jmp_pin(&conf, A15_PIN);

  pio_sm_init(pio0, SM_READ, offset, &conf);
  pio_sm_set_enabled(pio0, SM_READ, true);

  return;
}

void __time_critical_func(romulan)(void)
{
  BusSignals bus;
  uint32_t rom_offset, rom_size = POW2_CEIL(sizeof(ROM));
  uint8_t *rom_ptr = ROM;
  uint32_t last_addr = -1;

  setup_pio_irq_logic();

  while (true) {
    while (pio0->fstat & (1u << (PIO_FSTAT_RXEMPTY_LSB + SM_WAITSEL)))
      tight_loop_contents();

    bus.combined = pio0->rxf[SM_WAITSEL];
#if 0
    if (addr == last_addr)
      continue;
#endif

#if 0
    bool for_us = bus.cts;
    for_us |= IO_BASE <= bus.addr && bus.addr < IO_TOP;
    if (!for_us)
      continue;
#endif

#if 0
    printf("ADDR:%04x DATA:%02x CTS:%d SCS:%d RW:%d COMBINED:0x%08x\r\n",
           bus.addr, bus.data, bus.cts, bus.scs, bus.rw, bus.combined);
#endif

    if (!ramrom_ptr && rom_ptr != ROM)
      rom_ptr == ROM;

    if (ramrom_needs_activate) {// && addr == RAMROM_ACTIVATE_ADDR) {
      printf("Activating RAM\n"); // FIXME - why is this print necessary?
      if (ramrom_ptr)
        rom_ptr = ramrom_ptr;
      ramrom_needs_activate = false;
    }

    // FIXME - only check IO_BASE if rom_ptr == ROM
    if (IO_BASE <= bus.addr && bus.addr < IO_TOP) {
      unsigned io_reg = (bus.addr - IO_BASE) & 0x3;
#ifdef RW_PIN
      if (!bus.rw)
        io_reg |= 2;
#endif // RW_PIN

      switch (io_reg) {
      case IO_GETC: // Read byte
        pio0->txf[SM_READ] = sio_hw->fifo_rd;
        break;
      case IO_STATUS: // Read status reg
        pio0->txf[SM_READ] = sio_hw->fifo_st & SIO_FIFO_ST_VLD_BITS ? IO_FLAG_AVAIL : 0x00;
        break;
      case IO_PUTC: // Write byte
        sio_hw->fifo_wr = bus.combined;
        break;
      case IO_CONTROL: // Write control reg
        break;
      }
#if 0
      printf("ADDR:%04x DATA:%02x REG:%d A16-17:%d\r\n", bus.addr, bus.data, io_reg,
             (bus.combined >> ADDR_WIDTH) & 0x3);
#endif
    }
    else if (COCO_ROM_BASE <= bus.addr && bus.addr < COCO_ROM_TOP) {
      rom_offset = bus.addr - COCO_ROM_BASE;
      //rom_offset &= POW2_CEIL(sizeof(ROM)) - 1;
      bus.data = pio0->txf[SM_READ] = rom_ptr[rom_offset];
      //printf("ADDR:%04x DATA:%02x\r\n", addr, data);
    }
    //printf("ADDR:%04x DATA:%02x\r\n", addr, data);

    last_addr = bus.addr;
  }

  return;
}

void sendReplyPacket(fujiDeviceID_t source, bool ack, void *data, size_t length)
{
    FujiBusPacket packet(source, ack ? FUJICMD_ACK : FUJICMD_NAK,
                         ack ? std::string(static_cast<const char*>(data), length) : "");
    ByteBuffer encoded = packet.serialize();
    printf("Sending reply: dev:%02x cmd:%02x len:%04x\n",
           packet.device(), packet.command(), encoded.size());
    fwrite(encoded.data(), 1, encoded.size(), stdout);
    fflush(stdout);
    printf("Sent\n");
    return;
}

void process_command(ByteBuffer &buffer)
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
  BusSignals bus;
  int input;
  unsigned int count = 0;
  unsigned char ring_buffer[RING_SIZE];
  unsigned ring_in = 0, ring_out = 0;
  uint32_t last_cc_seen = 0, last_hb = 0, now;
  bool our_command = false;
  ByteBuffer command_buf;


  multicore_launch_core1(romulan);
  stdio_init_all();
  stdio_set_translate_crlf(&stdio_usb, false);

  while (!stdio_usb_connected())
    ;

  if (watchdog_caused_reboot())
    printf("Watchdog rebooted!\r\n");

  watchdog_enable(100, 1);

  while (true) {
    watchdog_update();

    now = to_ms_since_boot(get_absolute_time());
#if HEARTBEAT
    if (now - last_hb >= 1000) printf("(1)");
#endif // HEARTBEAT

    if (multicore_fifo_rvalid()) {
      bus.combined = multicore_fifo_pop_blocking();
#if 0
      printf("Received $%04x:$%02x\r\n", addr, data);
#else
      putchar(bus.data);
#endif
    }

#if HEARTBEAT
    if (now - last_hb >= 1000) printf("(2)");
#endif // HEARTBEAT
    if (command_buf.size()) {
      now = to_ms_since_boot(get_absolute_time());
      // Did we timeout waiting for final SLIP_END?
      if (now - last_cc_seen > 50) {
        //printf("Command timeout\r\n");
        for (char c : command_buf)
          ring_append((uint8_t) c);
        command_buf.clear();
      }
    }

#if HEARTBEAT
    if (now - last_hb >= 1000) printf("(3)");
#endif // HEARTBEAT
    input = getchar_timeout_us(0);
    if (input != PICO_ERROR_TIMEOUT) {
#if 0
      printf("Sending 0x%02x\r\n", input);
#endif
      if (1 || (!command_buf.size() && input != SLIP_END))
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
            //printf("Command not us\r\n");
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

#if HEARTBEAT
    if (now - last_hb >= 1000) printf("(4)");
#endif // HEARTBEAT
    if (ring_in != ring_out) {
#if 0
      printf("Ring send...");
#endif
      bool sent = multicore_fifo_push_timeout_us(ring_buffer[ring_out], 0);
      if (sent)
        ring_out = (ring_out + 1) % sizeof(ring_buffer);
#if 0
      printf(" sent %d\r\n", sent);
#endif
    }

    if (now - last_hb >= 1000) {
#if HEARTBEAT
      printf("(E)");
#endif // HEARTBEAT
      last_hb = now;
    }
  }

  return 0;
}
