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
#ifdef OBSOLETE
    uint32_t offset;
#else
    uint8_t bank;
#endif /* OBSOLETE */
  };

  std::array<MapperWindow, MAX_WINDOWS> windows; // std::vector is too slow
  volatile unsigned windowCount = 0;
  volatile unsigned registerBase, registerTop;

  volatile fujiROMType_t _type = ROM_TYPE_UNKNOWN;
  volatile uint16_t windowSize = ROM_SEG_SIZE;
  volatile uint16_t windowShift;
  volatile uint16_t bankCount = 1;

public:
  void setROMType(fujiROMType_t type) {
    _type = type;
    windowSize = _type & 0x80 ? SIZE_16K : SIZE_8K;

    resetWindows();
    switch (type) {
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
          .reg_top     = 0x7000,
          .window_base = 0x4000,
          .window_top  = 0x8000,
        });
        addWindow({
          .reg_base    = 0x7000,
          .reg_top     = 0x8000,
          .window_base = 0x8000,
          .window_top  = 0xC000,
        });
      }
      break;

    case ROM_TYPE_MSX_KONAMI:
    case ROM_TYPE_MSX_KONAMI_SCC:
    default:
      break;
    }

    resetBankOffsets();

    return;
  }

  void resetBankOffsets(void) {
    unsigned idx, offset;


#ifdef OBSOLETE
    for (idx = offset = 0; idx < windows.size(); idx++, offset += windowSize)
      windows[idx].offset = offset;
#else
    for (idx = offset = 0; idx < windows.size(); idx++)
      windows[idx].bank = idx;
#endif /* OBSOLETE */

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
    windowShift = __builtin_ctz(windowSize / windowCount);

    registerBase = windows[0].reg_base;
    registerTop = windows[0].reg_top;
    for (size_t idx = 1; idx < windowCount; idx++) {
      if (windows[idx].reg_base < registerBase)
        registerBase = windows[idx].reg_base;
      if (windows[idx].reg_top > registerTop)
        registerTop = windows[idx].reg_top;
    }
    return;
  }

  static inline bool isWrite(BusSignals bus) {
    return bus.rd;
  }

  inline bool isRemap(BusSignals bus) {
#ifdef OBSOLETE
    return isWrite(bus) && (0x4000 <= bus.addr) && (bus.addr < 0xC000);
#else
    return isWrite(bus) && registerBase <= bus.addr && bus.addr < registerTop;
#endif /* OBSOLETE */
  }

  inline MapperWindow *selectedRegister(BusSignals bus) {
    for (unsigned idx = 0; idx < windowCount; idx++) {
      if (windows[idx].reg_base <= bus.addr && bus.addr < windows[idx].reg_top)
        return &windows[idx];
    }

    return nullptr;
  }

#if 0 //def OBSOLETE
  inline void remap(BusSignals bus) {
    bool switch_bank = false;
    uint8_t window = 0;


    switch (_type) {
    case ROM_TYPE_MSX_ASCII8:
      if ((0x6000 <= bus.addr) && (bus.addr < 0x8000)) {
        // 0x6000 = 0, 0x6800 = 1, 0x7000 = 2, 0x7800 = 3
        window = (bus.addr >> 11) & 3;
        switch_bank = true;
      }
      break;
    case ROM_TYPE_MSX_ASCII16:
      if ((0x6000 <= bus.addr) && (bus.addr < 0x7800) && !(bus.addr & 0x0800)) {
        // 0x6000 = 0, 0x7000 = 1
        window = (bus.addr >> 12) & 1;
        switch_bank = true;
      }
      break;
    case ROM_TYPE_MSX_KONAMI:
      // [0x4000..0x6000) is fixed at segment 0.
      if (0x6000 <= bus.addr && bus.addr < 0xC000) {
        // 0x6000 = 3, 0x8000 = 4, 0xA000 = 5
        // subtract 2 because ROM starts at 0x4000
        window = (bus.addr >> 13) - 2;
        switch_bank = true;
      }
      break;
    case ROM_TYPE_MSX_KONAMI_SCC:
      if (0x5000 <= bus.addr && bus.addr < 0xC000 && (bus.addr & 0x1800) == 0x1000) {
        // 0x5000 = 2, 0x7000 = 3, 0x9000 = 4, 0xB000 = 5
        // subtract 2 because ROM starts at 0x4000
        window = (bus.addr >> 13) - 2;
        switch_bank = true;
        // TODO: if window = 4 clear SCC cache
      }
      break;
    default:
      break;
    }
#ifdef OBSOLETE
    if (switch_bank)
      windows[window].offset = (bus.data % bankCount) * windowSize;
#else
    if (switch_bank)
      windows[window].bank = bus.data % bankCount;
#endif /* OBSOLETE */
  }
#elif 1
  inline void remap(BusSignals bus) {
    MapperWindow *window = selectedRegister(bus);
    if (window != nullptr)
      window->bank = bus.data % bankCount;
    return;
  }
#elif 0
  inline void remap(BusSignals bus) {
    int window = -1;


    // This looks ridiculous but it's much much faster than a loop
    if (windowCount > 0 && (windows[0].reg_base <= bus.addr
                            && bus.addr < windows[0].reg_top))
      window = 0;
    else if (windowCount > 1 && (windows[1].reg_base <= bus.addr
                                 && bus.addr < windows[1].reg_top))
      window = 1;
    else if (windowCount > 2 && (windows[2].reg_base <= bus.addr
                                 && bus.addr < windows[2].reg_top))
      window = 2;
    else if (windowCount > 3 && (windows[3].reg_base <= bus.addr
                                 && bus.addr < windows[3].reg_top))
      window = 3;
    else if (windowCount > 4 && (windows[4].reg_base <= bus.addr
                                 && bus.addr < windows[4].reg_top))
      window = 4;
    else if (windowCount > 5 && (windows[5].reg_base <= bus.addr
                                 && bus.addr < windows[5].reg_top))
      window = 5;
    else if (windowCount > 6 && (windows[6].reg_base <= bus.addr
                                 && bus.addr < windows[6].reg_top))
      window = 6;
    else if (windowCount > 7 && (windows[7].reg_base <= bus.addr
                                 && bus.addr < windows[7].reg_top))
      window = 7;

    if (window >= 0)
      windows[window].bank = bus.data % bankCount;
  }
#elif 0
  inline void remap(BusSignals bus) {
    unsigned window = (bus.addr - windows[0].reg_base)
      / ((windows[windowCount].reg_top - windows[0].reg_base) / windowCount);
    windows[window].bank = bus.data % bankCount;
  }
#else
  inline void remap(BusSignals bus) {
    unsigned window = (bus.addr >> windowShift) & (windowCount - 1);
    //unsigned window = (bus.addr >> windowShift) & 3;
    //unsigned window = (bus.addr >> 11) & 3;
    windows[window].bank = bus.data % bankCount;
  }
#endif /* OBSOLETE */

  inline unsigned calculateOffset(BusSignals bus, UserROM &user_rom) {
    unsigned rom_offset = bus.addr;
    uint8_t window;


    if (user_rom.isActive()) {
      if (_type == ROM_TYPE_MSX_KONAMI) {
        // [0x0000, 0x4000) mirrors [0x4000, 0x8000)
        if (bus.addr < 0x4000) rom_offset += 0x4000;
        // [0xC000, 0x10000) mirrors [0x8000, 0xC000)
        else if (bus.addr >= 0xC000) rom_offset -= 0x4000;
      }
      else if (_type == ROM_TYPE_MSX_KONAMI_SCC) {
        // [0x0000, 0x4000) mirrors [0xC000, 0x10000)
        if (bus.addr < 0x4000) rom_offset += 0x8000;
        // [0xC000, 0x10000) mirrors [0x4000, 0x8000)
        else if (bus.addr >= 0xC000) rom_offset -= 0x8000;
      }
    }

    rom_offset -= BUS_ROM_BASE;

    if (user_rom.isActive()) {
      window = rom_offset >> (12 + (windowSize >> 13));
#ifdef OBSOLETE
      rom_offset += windows[window].offset - window * windowSize;
#else
      rom_offset += windows[window].bank * windowSize - window * windowSize;
#endif /* OBSOLETE */
    }

    return rom_offset % user_rom._segmentSize[user_rom._selectedSegment];
  }
};

#endif /* MAPPER_H */
