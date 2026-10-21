#ifndef MAPPER_H
#define MAPPER_H

#include "fujiROMType.h"

#include <array>
#include <vector>

/*
  ## How Mapping Works

  Mapping is another name for bank switching. It allows a system to
  access more data than can fit in the CPU's address space by dividing
  that data into banks and making selected banks accessible through
  windows in the address space.

  A window is a range of CPU addresses that can be mapped to different
  banks of data. By changing the selected bank, the same CPU addresses
  can access different portions of the underlying data.

  To describe a mapping, we need to know three things:

  1. Window:   The CPU address range where a bank of data is accessible.
  2. Register: The address used to select which bank is mapped into a window.
  3. Bank:     The portion of the underlying data that we want to make
               accessible through the window.

  The window's address range is usually straightforward. Register
  addresses, however, may have been chosen to simplify the hardware or
  reduce its cost. As a result, the same register may be accessible at
  multiple addresses.

  Similarly, a window may be able to select any available bank, or
  hardware restrictions may limit it to a specific subset of banks.

  Another hardware quirk is that windows themselves may be mirrored at
  multiple address ranges. This means the same underlying data can be
  accessed through more than one range of CPU addresses.
*/

#define MAX_WINDOWS 8

class Mapper
{
private:
  struct MapperWindow {
    uint32_t reg_base, reg_top;
    uint32_t window_base, window_top;
    uint8_t mirror;
    uint8_t bank;
  };

  std::array<MapperWindow, MAX_WINDOWS> windows; // std::vector is too slow
  unsigned windowCount = 0;
  unsigned windowBase = 0, windowTop = 0;
  unsigned registerBase = 0, registerTop = 0;

  fujiROMType_t _type = ROM_TYPE_UNKNOWN;
  uint16_t windowSize = ROM_SEG_SIZE;
  uint16_t windowShift = __builtin_ctz(ROM_SEG_SIZE);
  uint16_t bankCount = 1;

public:
  void setROMType(fujiROMType_t type) {
    _type = type;


    /*****************************/
    _type = ROM_TYPE_MSX_KONAMI;    // FIXME - HACK TO DEAL WITH TEST ROMS
    /*****************************/


    resetWindows();
    switch (_type) {
    case ROM_TYPE_MSX_ASCII8:
      {
        addWindow({
            .reg_base    = 0x6000,
            .reg_top     = 0x6800,
            .window_base = 0x4000,
            .window_top  = 0x6000,
          });
        addWindow({
            .reg_base    = 0x6800,
            .reg_top     = 0x7000,
            .window_base = 0x6000,
            .window_top  = 0x8000,
          });
        addWindow({
            .reg_base    = 0x7000,
            .reg_top     = 0x7800,
            .window_base = 0x8000,
            .window_top  = 0xA000,
          });
        addWindow({
            .reg_base    = 0x7800,
            .reg_top     = 0x8000,
            .window_base = 0xA000,
            .window_top  = 0xC000,
          });
      }
      break;

    case ROM_TYPE_MSX_ASCII16:
      {
        addWindow({
            .reg_base    = 0x6000,
            .reg_top     = 0x6800,
            .window_base = 0x4000,
            .window_top  = 0x8000,
          });
        addWindow({
            .reg_base    = 0x7000,
            .reg_top     = 0x7800,
            .window_base = 0x8000,
            .window_top  = 0xC000,
          });
      }
      break;

    case ROM_TYPE_MSX_KONAMI:
      {
        addWindow({
            .reg_base = 0,
            .reg_top = 0,
            .window_base = 0x4000,
            .window_top = 0x6000,
          });
        addWindow({
            .reg_base = 0x6000,
            .reg_top = 0x8000,
            .window_base = 0x6000,
            .window_top = 0x8000,
          });
        addWindow({
            .reg_base = 0x8000,
            .reg_top = 0xA000,
            .window_base = 0x8000,
            .window_top = 0xA000,
          });
        addWindow({
            .reg_base = 0xA000,
            .reg_top = 0xC000,
            .window_base = 0xA000,
            .window_top = 0xC000,
          });
      }
      break;

    case ROM_TYPE_MSX_KONAMI_SCC:
    default:
      break;
    }

    return;
  }

  void setROMSize(size_t size) {
    bankCount = std::max<uint16_t>(1, (size + windowSize - 1) / windowSize);
    return;
  }

  void resetWindows(void) {
    windows.fill({});
    windowCount = 0;
    return;
  }

  void addWindow(MapperWindow window) {
    // FIXME - keep windows sorted by window_base
    windows[windowCount] = window;
    windowCount += 1;
    windowSize = windows[0].window_top - windows[0].window_base;
    windowShift = __builtin_ctz(windowSize);

    registerBase = windows[0].reg_base;
    registerTop = windows[0].reg_top;
    windowBase = windows[0].window_base;
    windowTop = windows[0].window_top;
    for (size_t idx = 1; idx < windowCount; idx++) {
      if (windows[idx].reg_base < registerBase)
        registerBase = windows[idx].reg_base;
      if (windows[idx].reg_top > registerTop)
        registerTop = windows[idx].reg_top;
      if (windows[idx].window_base < windowBase)
        windowBase = windows[idx].window_base;
      if (windows[idx].window_top > windowTop)
        windowTop = windows[idx].window_top;
    }
    return;
  }

  // Called by the bus loop when the user ROM is switched on. The barrier
  // stops the compiler from using values it loaded before core 0 set up
  // the mapper, which it would otherwise be free to hoist out of the loop.
  inline void activate(void) {
    __compiler_memory_barrier();
    for (unsigned idx = 0; idx < windowCount; idx++)
      windows[idx].bank = idx;
    return;
  }

  static inline bool isWrite(BusSignals bus) {
    return bus.rd;
  }

  inline bool isRemap(BusSignals bus) {
    return isWrite(bus) && registerBase <= bus.addr && bus.addr < registerTop;
  }

  inline bool isWindow(BusSignals bus) {
    return windowBase <= bus.addr && bus.addr < windowTop;
  }

  inline MapperWindow *selectedRegister(BusSignals bus) {
    for (unsigned idx = 0; idx < windowCount; idx++) {
      if (windows[idx].reg_base <= bus.addr && bus.addr < windows[idx].reg_top)
        return &windows[idx];
    }

    return nullptr;
  }

  inline int selectedWindow(BusSignals bus) {
    unsigned rom_offset = bus.addr - windowBase;
    return (rom_offset >> windowShift) & (windowCount - 1);
  }

  inline void remap(BusSignals bus) {
    MapperWindow *window = selectedRegister(bus);
    if (window != nullptr)
      window->bank = bus.data & (bankCount - 1);
    return;
  }

  inline unsigned calculateOffset(BusSignals bus, UserROM &user_rom) {
    unsigned rom_offset = bus.addr;
    if (user_rom.isActive()) { // && isWindow(bus)) {
      int window = selectedWindow(bus);
      rom_offset += (windows[window].bank - window) * windowSize;
      rom_offset -= windowBase;
    }
    else
      rom_offset -= BUS_ROM_BASE;
    return rom_offset;
  }
};

#endif /* MAPPER_H */
