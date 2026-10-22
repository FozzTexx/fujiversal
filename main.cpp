#include "setup_sm.h"
#include "board_defs.h"
#include "UserROM.h"
#include "Mapper.h"
#include "FujiBusPacket.h"
#include "fujiDeviceID.h"
#include "fujiCommandID.h"
#include "diag_uart.h"
#include <cstddef>
#include <cstdint>

#define VERBOSE_DEBUG 0

#ifdef USE_STDIO
#include <stdio.h>
#define DEBUG_PRINTF printf
#else
#pragma GCC poison printf putchar getchar
#endif // USE_STDIO

#include <array>
#include <string.h>
#include <pico/stdlib.h>
#include <pico/multicore.h>
#include <hardware/pio.h>
#include <hardware/irq.h>
#include <hardware/watchdog.h>
#include <hardware/clocks.h>
#include <hardware/vreg.h>
#include <hardware/sync.h>
#include <tusb.h>

#include <string>

#ifdef RW_PIN
#define IO_TOP     (IO_BASE + 2)
#else
#define IO_TOP     (IO_BASE + 4)
#endif

#define IO_FLAG_USERROM_READY    0x40
#define IO_FLAG_ROM_MODE_CMD     0b00000100
#define IO_FLAG_USERROM_ENABLE   0b00000001
#define IO_FLAG_AUTOSTART_ENABLE 0b00000010
#define IO_FLAG_ROM_BANK_CMD     0b10000000
#define IO_MASK_ROM_BANK         0b00001111

#define DEFAULT_ROM disk_rom

// A ROM must not drive the bus while the CPU is writing.
#if defined(BOARD_coco_proto_260402) || defined(BOARD_picorom_coco)
// CTS also covers $8000-$BFFF in the CoCo 3's 32K external ROM mode; $FF00 up
// is I/O whichever mode. Constants: the read path cannot afford loads.
#define IS_ROM_READ(b) \
  (!Mapper::isWrite(b) && 0x8000 <= (b).addr && (b).addr < BUS_ROM_TOP)
#else
#define IS_ROM_READ(b) (!Mapper::isWrite(b) && mapper.isMapped(b, user_rom))
#endif

#define USE_IRQ 0

#define PSM_WAITSEL 0
#define PSM_READ    1
#if !defined(BOARD_picorom_coco) && !defined(BOARD_picorom_msx) && !defined(BOARD_msxrp2350)
#define PSM_SENDBUS 2
#endif

pio_sm_t state_machine[3];
#define pio_get_fifo(n) pio_sm_get_blocking(state_machine[n].pio, state_machine[n].sm)
#define pio_put_fifo(n, d) pio_sm_put(state_machine[n].pio, state_machine[n].sm, d)

#define POW2_CEIL(x_) ({      \
    unsigned int x = x_; \
    x -= 1;              \
    x = x | (x >> 1);    \
    x = x | (x >> 2);    \
    x = x | (x >> 4);    \
    x = x | (x >> 8);    \
    x = x | (x >>16);    \
    x + 1; })

#define RING_SIZE 1024
#define ring_append(buf, in, x) ({buf[in] = x; in = (in + 1) % sizeof(buf); })
#define check_tx() ({ \
      if (multicore_fifo_rvalid()) {                    \
        bus.combined = multicore_fifo_pop_blocking();   \
        ring_append(ring_tx, ring_tx_in, bus.data);     \
      }                                                 \
    })

UserROM user_rom;
Mapper mapper;

#if defined(BOARD_coco_proto_260402) || defined(BOARD_picorom_coco)
// Reads served since the cart was switched on. A BASIC signature probe reads
// a handful; real execution reads thousands.
volatile uint32_t served_reads = 0;
volatile uint32_t user_rom_len = 0;
#endif

#ifdef BOARD_coco_proto_260402
// Power-on-like Program Pak boot: on user-ROM enable we point rom_ptr at the
// .CCC (so $C000 != "DK" and the reset routine can't reload HDB-DOS), pulse
// RESET_PIN low for a clean hardware reset, then toggle CART_PIN at ~60Hz like
// a real Program Pak's CART line so the reset routine autostarts the cartridge.
volatile bool reset_active = false;          // RESET asserted, pending release
volatile uint32_t reset_assert_ms = 0;       // when core 0 asserted RESET
volatile bool cart_toggle_active = false;    // core 0 toggles CART_PIN at ~60Hz
volatile uint32_t cart_toggle_start_ms = 0;  // when the CART toggle started
#define RESET_PULSE_MS 50                   // hold RESET low this long
#define CART_TOGGLE_MS 1500                 // toggle CART this long after autostart
#endif // BOARD_coco_proto_260402

#define SERIAL_BEGIN_DELAY 100

#ifndef USE_STDIO
#include <stdarg.h>
#include <stdio.h>
#include "tusb.h"

#define DEBUG_PRINTF tusb_printf

void tusb_printf(const char *format, ...)
{
  char buf[256];
  va_list args;


  va_start(args, format);
  int len = vsnprintf(buf, sizeof(buf), format, args);
  va_end(args);

  if (len) {
    tud_cdc_write(buf, (uint32_t)len);
    tud_cdc_write_flush();
    while (tud_cdc_write_available() < CFG_TUD_CDC_TX_BUFSIZE)
      tud_task();
  }

  return;
}
#endif // ! USE_STDIO

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


  // Init all GPIO pins to inputs with no pulls
  for (uint pin = 0; pin < NUM_BANK0_GPIOS; pin++) {
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_IN);
    gpio_disable_pulls(pin);
  }

  // Setup state machine that checks when we are selected
  {
    sm_setup_t waitsel_setup {};
    waitsel_init_setup(&waitsel_setup);
    setup_state_machine(&state_machine[PSM_WAITSEL], &waitsel_setup);
  }

#ifdef PSM_SENDBUS
  // Setup state machine that sends addr/data bus signals
  {
    sm_setup_t send_bus_setup {};
    send_bus_init_setup(&send_bus_setup);
    setup_state_machine(&state_machine[PSM_SENDBUS], &send_bus_setup);
  }
#endif // PSM_SENDBUS

  // Setup state machine that handles CPU read by putting byte on bus
  {
    sm_setup_t read_setup {};
    read_init_setup(&read_setup);
    setup_state_machine(&state_machine[PSM_READ], &read_setup);
  }

#ifdef BOARD_coco_proto_260402
  // FIXME - doesn't belong here
  gpio_pull_up(IGNORE_PIN); // unused middle pin needs to be inverted to avoid false zero

  // Park CART_PIN high (deasserted) at boot; the autostart toggle drives it.
  gpio_init(CART_PIN);
  gpio_set_dir(CART_PIN, GPIO_OUT);
  gpio_put(CART_PIN, 1);

  // RESET_PIN is open-drain: latch 0 so driving it OUT asserts RESET low, and
  // start as input (released - the CoCo's pull-up holds RESET high).
  gpio_init(RESET_PIN);
  gpio_put(RESET_PIN, 0);
  gpio_set_dir(RESET_PIN, GPIO_IN);
#endif

#if USE_IRQ
  pio_set_irq0_source_enabled(pio0, pis_interrupt0, true);
  irq_set_exclusive_handler(PIO0_IRQ_0, pio_irq_handler);
  irq_set_enabled(PIO0_IRQ_0, true);
#endif

  return;
}

void __time_critical_func(romulan)(void)
{
  BusSignals bus;
  uint32_t rom_offset, rom_size = POW2_CEIL(sizeof(DEFAULT_ROM));
  uint8_t *rom_ptr = DEFAULT_ROM;
  uint32_t last_bus_state = -1;


  setup_pio_irq_logic();

  while (true) {
#ifdef PSM_SENDBUS
    bus.combined = pio_get_fifo(PSM_SENDBUS);
#else
    bus.combined = pio_get_fifo(PSM_WAITSEL);
#endif // PSM_SENDBUS
#if !defined(BOARD_picorom_coco) && !defined(BOARD_coco_proto_260402)
    if (bus.combined == last_bus_state)
      continue;
#endif

#if 0
    DEBUG_PRINTF("ADDR:%04x DATA:%02x CTS:%d SCS:%d RW:%d UN:%d COMBINED:0x%08x\r\n",
                 bus.addr, bus.data, 0, bus.scs, bus.rw, bus.unused, bus.combined);
#endif

    bool active = user_rom.isActive();
    if (!active) {
      if (rom_ptr != DEFAULT_ROM) {
        rom_ptr = DEFAULT_ROM;
        mapper.deactivate();
      }
    }
    // Reads come first once the user ROM is on. Inactive, the IO registers
    // take precedence: on MSX they sit inside the ROM window.
    else if (IS_ROM_READ(bus)) {
      rom_offset = mapper.calculateOffset(bus);
      bus.data = rom_ptr[rom_offset];
      pio_put_fifo(PSM_READ, bus.data);
#if defined(BOARD_coco_proto_260402) || defined(BOARD_picorom_coco)
      served_reads = served_reads + 1;
#endif
      last_bus_state = bus.combined;
      continue;
    }

    if (!active && IO_BASE <= bus.addr && bus.addr < IO_TOP) {
      unsigned io_reg = (bus.addr - IO_BASE) & 0x3;
#ifdef RW_PIN
      if (!bus.rw)
        io_reg |= 2;
#endif // RW_PIN

      switch (io_reg) {
      case IO_GETC: // Read byte
        pio_put_fifo(PSM_READ, sio_hw->fifo_rd);
        break;
      case IO_STATUS: // Read status reg
        pio_put_fifo(PSM_READ,
                     (sio_hw->fifo_st & SIO_FIFO_ST_VLD_BITS ? IO_FLAG_AVAIL : 0x00)
                     | (user_rom.isClosed() ? IO_FLAG_USERROM_READY : 0x00)
                     );
        break;
      case IO_PUTC: // Write byte
        sio_hw->fifo_wr = bus.combined;
        break;

      case IO_CONTROL: // Write control reg
        diag_count_ioctl();
        // Command to enable/disable user ROM, or enable/disable ROM autostart
        if (bus.data & IO_FLAG_ROM_MODE_CMD) {
          // Enable/disable user ROM
          if (bus.data & IO_FLAG_USERROM_ENABLE) {
            user_rom.activate();
            mapper.activate();
            rom_ptr = user_rom.activeBase;
#if defined(BOARD_coco_proto_260402) || defined(BOARD_picorom_coco)
            served_reads = 0;
#endif
            diag_mount_begin();
            if (bus.data & IO_FLAG_AUTOSTART_ENABLE) {
#ifdef BOARD_coco_proto_260402
              // Enable auto start (CoCo)
              // Start the ~60Hz CART toggle and pulse RESET low;
              // the CoCo comes out of a clean hardware reset and its own reset
              // routine autostarts the cartridge.
              cart_toggle_active = true;
              cart_toggle_start_ms = to_ms_since_boot(get_absolute_time());
              diag_count_reset();
              gpio_put(RESET_PIN, 0);
              gpio_set_dir(RESET_PIN, GPIO_OUT);   // assert RESET low
              reset_active = true;
              reset_assert_ms = to_ms_since_boot(get_absolute_time());
#endif // BOARD_coco_proto_260402
            }
          }
          else {
            user_rom.deactivate();
            rom_ptr = DEFAULT_ROM;
            mapper.deactivate();
          }
        }
        else if (bus.data & IO_FLAG_ROM_BANK_CMD) {
          user_rom.selectSegment(bus.data);
          rom_ptr = user_rom.activeBase;
        }
        break;
      }
    }
    else if (active && mapper.isRemap(bus)) {
      mapper.remap(bus);
    }
    else if (IS_ROM_READ(bus)) {
      rom_offset = mapper.calculateOffset(bus);
      bus.data = rom_ptr[rom_offset];
      pio_put_fifo(PSM_READ, bus.data);
    }

    last_bus_state = bus.combined;
  }

  return;
}

void sendReplyPacket(fujiDeviceID_t source, bool ack, void *data, size_t length)
{
    FujiBusPacket packet(source, ack ? FUJICMD_ACK : FUJICMD_NAK,
                         ack ? std::string(static_cast<const char*>(data), length) : "");
    ByteBuffer encoded = packet.serialize();
#if VERBOSE_DEBUG
    DEBUG_PRINTF("Sending reply: dev:%02x cmd:%02x len:%04x\n",
           packet.device(), packet.command(), encoded.size());
#endif // VERBOSE_DEBUG
#ifdef USE_STDIO
    fwrite(encoded.data(), 1, encoded.size(), stdout);
    fflush(stdout);
#else
    tud_cdc_write(encoded.data(), (uint32_t) encoded.size());
    tud_cdc_write_flush();
    while (tud_cdc_write_available() < CFG_TUD_CDC_TX_BUFSIZE)
      tud_task();
#endif // USE_STDIO
#if VERBOSE_DEBUG
    DEBUG_PRINTF("Sent\n");
#endif // VERBOSE_DEBUG
    return;
}

bool process_command(ByteBuffer &buffer)
{
  auto packet = FujiBusPacket::fromSerialized(buffer);


  if (!packet) {
#if VERBOSE_DEBUG
    DEBUG_PRINTF("Failed to decode packet\n");
#endif // VERBOSE_DEBUG
    return false;
  }

  switch (packet->command()) {
  case FUJICMD_OPEN:
    if (packet->paramCount() < 1)
      sendReplyPacket(packet->device(), false, nullptr, 0);
    else {
      user_rom.open(packet->param(0));

      fujiROMType_t type = ROM_TYPE_UNKNOWN;
      if (packet->paramCount() > 1)
        type = (fujiROMType_t) packet->param(1);
      mapper.setROMType(type);

      sendReplyPacket(packet->device(), true, nullptr, 0);
#if VERBOSE_DEBUG
      DEBUG_PRINTF("Opening RAM at 0x%04x\n", offset);
#endif // VERBOSE_DEBUG
    }
    break;

  case FUJICMD_WRITE:
    if (!packet->data().has_value())
      sendReplyPacket(packet->device(), false, nullptr, 0);
    else
      sendReplyPacket(packet->device(), user_rom.write(packet->data().value()).is_success(),
                      nullptr, 0);
    break;

  case FUJICMD_CLOSE:
    {
      size_t size = user_rom.close();
#if defined(BOARD_coco_proto_260402) || defined(BOARD_picorom_coco)
      // The host sends no ROM type, so classify by size. It cannot tell a
      // banked 32K cart from a flat one; none is known.
      if (mapper.type() == ROM_TYPE_UNKNOWN && size > 0) {
        if (size <= SIZE_32K)
          mapper.setROMType(ROM_TYPE_COCO_32K_FF90);
        else if (size <= 2 * SIZE_32K)
          mapper.setROMType(ROM_TYPE_COCO_64K_FF90);
        else
          mapper.setROMType(ROM_TYPE_COCO_128K_FF90);
      }
      user_rom_len = size;
#endif
      if (size > 0)
        mapper.setROMSize(size);
      sendReplyPacket(packet->device(), size > 0, nullptr, 0);
#if VERBOSE_DEBUG
      DEBUG_PRINTF("Closing RAM %d\n", user_rom.closed);
#endif // VERBOSE_DEBUG
    }
    break;

  case FUJICMD_RESET:
    user_rom.reset();
    sendReplyPacket(packet->device(), true, nullptr, 0);
    break;

  default:
    sendReplyPacket(packet->device(), false, nullptr, 0);
    break;
  }

  return true;
}

int main()
{
  BusSignals bus;
  int input;
  unsigned int count = 0;
  unsigned char ring_rx[RING_SIZE], ring_tx[RING_SIZE];
  unsigned ring_rx_in = 0, ring_rx_out = 0;
  unsigned ring_tx_in = 0, ring_tx_out = 0;
  uint32_t last_cc_seen = 0, last_ring_sent = 0, now, loop_begin;
  bool our_command = false, serial_ready = false;
  ByteBuffer command_buf;


#ifdef BOARD_coco_proto_260402
  // Needed to serve the cart bus at 2MHz. Voltage must come up first and
  // settle; the clock alone leaves the chip unstable. Non-panicking form so a
  // failed request cannot hang before USB is up.
  vreg_set_voltage(VREG_VOLTAGE_1_20);
  sleep_ms(10);
  if (!set_sys_clock_khz(300000, false))
    set_sys_clock_khz(250000, true);
#else
  set_sys_clock_khz(250000, true);
#endif

#ifdef LED_PIN
  gpio_init(LED_PIN);
  gpio_set_dir(LED_PIN, GPIO_OUT);
#endif // LED_PIN

  multicore_launch_core1(romulan);

#ifdef USE_STDIO
  stdio_init_all();
  stdio_set_translate_crlf(&stdio_usb, false);

  while (!stdio_usb_connected())
    ;
#else
  tusb_init();
  while (!tud_cdc_connected()) {
    tud_task();
    check_tx();
  }
#endif // USE_STDIO

#if 0
  if (watchdog_caused_reboot())
    printf("Watchdog rebooted!\r\n");
#endif

  watchdog_enable(100, 1);

  loop_begin = to_ms_since_boot(get_absolute_time());
  while (true) {
    watchdog_update();
    now = to_ms_since_boot(get_absolute_time());

    if (!serial_ready && now - loop_begin > SERIAL_BEGIN_DELAY)
      serial_ready = true;

#if defined(BOARD_coco_proto_260402) || defined(BOARD_picorom_coco)
    diag_poll(now, user_rom.isActive(), user_rom_len, mapper.mask(),
              mapper.banks(), mapper.type());
#endif

#ifdef BOARD_coco_proto_260402
    // Release RESET once the pulse has elapsed (open-drain: back to input) so
    // the CoCo boots from a clean hardware reset. Unsigned delta is
    // wraparound-safe.
    if (reset_active && now - reset_assert_ms >= RESET_PULSE_MS) {
      gpio_set_dir(RESET_PIN, GPIO_IN);
      reset_active = false;
    }
    // Toggle CART at ~60Hz (period ~16ms) like a Program Pak's CART line so the
    // reset routine's cart-FIRQ check fires and autostarts the .CCC. Stop after
    // the autostart window and park CART high (deasserted) so leftover CART
    // FIRQs don't disrupt a later CFGLOAD/CONFIG.BIN boot. Unsigned delta is
    // wraparound-safe.
    if (cart_toggle_active) {
      if (served_reads > 256 || now - cart_toggle_start_ms >= CART_TOGGLE_MS) {
        cart_toggle_active = false;
        gpio_put(CART_PIN, 1);
      }
      else
        gpio_put(CART_PIN, (now >> 3) & 1);
    }
#endif // BOARD_coco_proto_260402

    check_tx();

    if (command_buf.size()) {
      // Did we timeout waiting for final SLIP_END?
      if (now - last_cc_seen > 50) {
#if VERBOSE_DEBUG
        DEBUG_PRINTF("Command timeout %d\r\n", command_buf.size());
#endif // VERBOSE_DEBUG
        for (char c : command_buf)
          ring_append(ring_rx, ring_rx_in, (uint8_t) c);
        command_buf.clear();
      }
    }

    tud_task();
    if ((ring_rx_in + 1) % RING_SIZE != ring_rx_out || now - last_ring_sent > 10) {
#ifdef LED_PIN
      //gpio_put(LED_PIN, 0);
#endif
      input = tud_cdc_available();
      if (input > 0) {
        unsigned char rc;
        tud_cdc_read(&rc, 1);
        input = rc;
        if (!command_buf.size() && input != SLIP_END)
          ring_append(ring_rx, ring_rx_in, input);
        else {
          // if SLIP_END or already capturing then push to command_buf
          last_cc_seen = to_ms_since_boot(get_absolute_time());

          // Keep track of when last command char was seen so we can timeout
          command_buf.push_back((char) input);

          size_t command_size = command_buf.size();
          if (command_buf.size()) {
            // If second char is not a command for us, send command_buf to RBS
            if (command_size == 2 && input != FUJI_DEVICEID_DBC) {
#if VERBOSE_DEBUG
              DEBUG_PRINTF("Command not us\r\n");
#endif // VERBOSE_DEBUG
              for (char c : command_buf)
                ring_append(ring_rx, ring_rx_in, (uint8_t) c);
              command_buf.clear();
            }
            else if (command_size > 1 && input == SLIP_END) {
              if (!process_command(command_buf)) {
                for (char c : command_buf)
                  ring_append(ring_rx, ring_rx_in, (uint8_t) c);
              }
              command_buf.clear();
            }
          }
        }
      }
    }
    else {
      //printf("RING FULL\r\n");
#ifdef LED_PIN
      //gpio_put(LED_PIN, 1);
#endif
    }

    if (ring_rx_in != ring_rx_out) {
      bool sent = multicore_fifo_push_timeout_us(ring_rx[ring_rx_out], 0);
      if (sent) {
        ring_rx_out = (ring_rx_out + 1) % sizeof(ring_rx);
        last_ring_sent = now;
      }
    }

    if (serial_ready && ring_tx_in != ring_tx_out) {
#ifdef USE_STDIO
      putchar(ring_tx[ring_tx_out]);
      ring_tx_out = (ring_tx_out + 1) % sizeof(ring_tx);
#else
      unsigned contig = (ring_tx_in > ring_tx_out)
        ? (ring_tx_in - ring_tx_out)
        : (sizeof(ring_tx) - ring_tx_out);

      while (tud_cdc_write_available() < 1)
        tud_task();

      uint32_t room = tud_cdc_write_available();
      uint32_t to_write = (contig < room) ? contig : room;
      uint32_t written = tud_cdc_write(&ring_tx[ring_tx_out], to_write);
      tud_cdc_write_flush();
      ring_tx_out = (ring_tx_out + written) % sizeof(ring_tx);
#endif // USE_STDIO
    }
  }

  return 0;
}
