#ifndef MAPPER_H
#define MAPPER_H

#include "fujiROMType.h"

#include <array>
#include <vector>
#include <climits>

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
#define MAX_PAGES 64                    // finest page the table can hold: 1K

class Mapper
{
private:
  struct MapperWindow {
    uint32_t reg_base, reg_top;
    uint32_t window_base, window_top;
    uint8_t bank;
    uint32_t reg_mask = 0, reg_value = 0; // register decodes (addr & mask) == value
    int8_t reset_bank = -1;               // bank presented at activation; -1 = index
  };

  struct MapperMirror {
    uint32_t mirror_base, mirror_top;
    uint32_t source;
  };

  std::array<MapperWindow, MAX_WINDOWS> windows; // std::vector is too slow
  unsigned windowCount = 0;
  uint16_t windowSize = ROM_SEG_SIZE;
  uint16_t windowShift = __builtin_ctz(ROM_SEG_SIZE);

  std::array<MapperMirror, MAX_WINDOWS> mirrors;
  unsigned mirrorCount = 0;

  unsigned windowBase = UINT_MAX, windowTop = 0;
  unsigned registerBase = UINT_MAX, registerTop = 0;
  unsigned mappedBase = UINT_MAX, mappedTop = 0;

  uint16_t bankCount = 1;
  // An image smaller than its windows mirrors, like a ROM with unconnected
  // upper address lines.
  uint32_t sizeMask = ~0u;

  // Windows, mirrors and banks resolved to one delta per page, so a read
  // costs one table lookup. Refreshed whenever the mapping changes; the CoCo
  // cannot afford more work than that per cycle, and a bank write has to be
  // done before the fetch that follows it. The page is the coarsest that
  // still lands every window and mirror boundary.
  int32_t delta[MAX_PAGES] {};
  unsigned pageShift = 16;

  fujiROMType_t _type = ROM_TYPE_UNKNOWN;

  inline void resolveWindow(unsigned idx) {
    const MapperWindow &w = windows[idx];
    int32_t d = (w.bank - (int)idx) * windowSize - windowBase;
    for (unsigned page = w.window_base >> pageShift; page < w.window_top >> pageShift; page++)
      delta[page] = d;
    return;
  }

  inline void resolveMirrors(void) {
    for (unsigned idx = 0; idx < mirrorCount; idx++) {
      const MapperMirror &m = mirrors[idx];
      int32_t shift = m.source - m.mirror_base;
      for (unsigned page = m.mirror_base >> pageShift; page < m.mirror_top >> pageShift; page++)
        delta[page] = shift + delta[(page << pageShift) + shift >> pageShift];
    }
    return;
  }

  void resolve(void) {
    unsigned bounds = 0x10000;
    for (unsigned idx = 0; idx < windowCount; idx++)
      bounds |= windows[idx].window_base | windows[idx].window_top;
    for (unsigned idx = 0; idx < mirrorCount; idx++)
      bounds |= mirrors[idx].mirror_base | mirrors[idx].mirror_top | mirrors[idx].source;
    pageShift = std::max<unsigned>(__builtin_ctz(bounds), 16 - __builtin_ctz(MAX_PAGES));
    for (unsigned idx = 0; idx < windowCount; idx++)
      resolveWindow(idx);
    resolveMirrors();
    return;
  }

public:
  Mapper() { deactivate(); }

  fujiROMType_t type(void) const { return _type; }
  uint16_t banks(void) const { return bankCount; }
  uint32_t mask(void) const { return sizeMask; }

  void setROMType(fujiROMType_t type) {
    _type = type;


#ifdef FORCED_ROM_TYPE
    /*****************************/
    _type = FORCED_ROM_TYPE;
    /*****************************/
#endif /* FORCED_ROM_TYPE */


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
            .reg_base    = 0,
            .reg_top     = 0,
            .window_base = 0x4000,
            .window_top  = 0x6000,
          });
        addWindow({
            .reg_base    = 0x6000,
            .reg_top     = 0x8000,
            .window_base = 0x6000,
            .window_top  = 0x8000,
          });
        addWindow({
            .reg_base    = 0x8000,
            .reg_top     = 0xA000,
            .window_base = 0x8000,
            .window_top  = 0xA000,
          });
        addWindow({
            .reg_base    = 0xA000,
            .reg_top     = 0xC000,
            .window_base = 0xA000,
            .window_top  = 0xC000,
          });

        addMirror({
            .mirror_base = 0x0000,
            .mirror_top  = 0x4000,
            .source      = 0x4000,
          });
        addMirror({
            .mirror_base = 0xC000,
            .mirror_top  = 0x10000,
            .source      = 0x8000,
          });
      }
      break;

    case ROM_TYPE_MSX_KONAMI_SCC:
      {
        addWindow({
            .reg_base    = 0x5000,
            .reg_top     = 0x5800,
            .window_base = 0x4000,
            .window_top  = 0x6000,
          });
        addWindow({
            .reg_base    = 0x7000,
            .reg_top     = 0x7800,
            .window_base = 0x6000,
            .window_top  = 0x8000,
          });
        addWindow({
            .reg_base    = 0x9000,
            .reg_top     = 0x9800,
            .window_base = 0x8000,
            .window_top  = 0xA000,
          });
        addWindow({
            .reg_base    = 0xB000,
            .reg_top     = 0xB800,
            .window_base = 0xA000,
            .window_top  = 0xC000,
          });

        addMirror({
            .mirror_base = 0x0000,
            .mirror_top  = 0x4000,
            .source      = 0x8000,
          });
        addMirror({
            .mirror_base = 0xC000,
            .mirror_top  = 0x10000,
            .source      = 0x4000,
          });
      }
      break;

    case ROM_TYPE_COCO_32K_FF90:
      {
        // Flat cart, image in cart order: first 16K at $C000, second at $8000.
        addWindow({
            .window_base = 0x8000,
            .window_top  = 0xC000,
            .reset_bank  = 1,
          });
        addWindow({
            .window_base = 0xC000,
            .window_top  = 0x10000,
            .reset_bank  = 0,
          });
      }
      break;

    case ROM_TYPE_COCO_64K_FF90:
    case ROM_TYPE_COCO_128K_FF90:
      {
        // GMC: 16K banks, one register at the even addresses of $FF40-$FF5F
        // (the odd ones are its sound chip), the same bank visible at $8000.
        addWindow({
            .reg_base    = 0xFF40,
            .reg_top     = 0xFF60,
            .window_base = 0x8000,
            .window_top  = 0xC000,
            .reg_mask    = 1,
            .reset_bank  = 0,
          });
        addWindow({
            .reg_base    = 0xFF40,
            .reg_top     = 0xFF60,
            .window_base = 0xC000,
            .window_top  = 0x10000,
            .reg_mask    = 1,
            .reset_bank  = 0,
          });
      }
      break;

    default:
      break;
    }

    return;
  }

  void setROMSize(size_t size) {
    bankCount = std::max<uint16_t>(1, (size + windowSize - 1) / windowSize);
    sizeMask = size > 1 ? (1u << (32 - __builtin_clz(size - 1))) - 1 : 0;
    return;
  }

  void resetWindows(void) {
    windows.fill({});
    windowCount = 0;
    mirrorCount = 0;

    windowBase = UINT_MAX;
    windowTop = 0;
    registerBase = UINT_MAX;
    registerTop = 0;
    mappedBase = UINT_MAX;
    mappedTop = 0;

    return;
  }

  static void extend(unsigned &base, unsigned &top, unsigned new_base, unsigned new_top) {
    if (new_base < base)
      base = new_base;
    if (new_top > top)
      top = new_top;
    return;
  }

  void addWindow(MapperWindow window) {
    // FIXME - keep windows sorted by window_base
    windows[windowCount] = window;

    extend(windowBase, windowTop,
           windows[windowCount].window_base, windows[windowCount].window_top);
    extend(mappedBase, mappedTop, windowBase, windowTop);
    if (windows[windowCount].reg_base != windows[windowCount].reg_top) {
      extend(registerBase, registerTop,
             windows[windowCount].reg_base, windows[windowCount].reg_top);
    }

    windowCount += 1;
    windowSize = windows[0].window_top - windows[0].window_base;
    windowShift = __builtin_ctz(windowSize);
    return;
  }

  void addMirror(MapperMirror mirror) {
    // FIXME - keep mirrors sorted by mirror_base
    mirrors[mirrorCount] = mirror;

    extend(mappedBase, mappedTop,
           mirrors[mirrorCount].mirror_base, mirrors[mirrorCount].mirror_top);

    mirrorCount += 1;
    return;
  }

  // Called by the bus loop when the user ROM is switched on. The barrier
  // stops the compiler from using values it loaded before core 0 set up
  // the mapper, which it would otherwise be free to hoist out of the loop.
  inline void activate(void) {
    __compiler_memory_barrier();
    for (unsigned idx = 0; idx < windowCount; idx++)
      windows[idx].bank = windows[idx].reset_bank < 0 ? idx : windows[idx].reset_bank;
    resolve();
    return;
  }

  // The default ROM, flat at BUS_ROM_BASE.
  inline void deactivate(void) {
    pageShift = 16;
    delta[0] = -(int32_t)BUS_ROM_BASE;
    sizeMask = ~0u;
    return;
  }

  static inline bool isWrite(BusSignals bus) {
#ifdef RD_PIN
    return bus.rd;
#else
    return !bus.rw;
#endif // RD_PIN
  }

  inline bool isRemap(BusSignals bus) {
    return isWrite(bus) && registerBase <= bus.addr && bus.addr < registerTop;
  }

  inline bool isWindow(BusSignals bus) {
    return windowBase <= bus.addr && bus.addr < windowTop;
  }

  inline bool isMapped(BusSignals bus, UserROM &user_rom) {
    if (user_rom.isActive())
      return mappedBase <= bus.addr && bus.addr < mappedTop;
    return BUS_ROM_BASE <= bus.addr && bus.addr < BUS_ROM_TOP;
  }

  inline bool isRegister(const MapperWindow &w, unsigned addr) {
    return w.reg_base <= addr && addr < w.reg_top && (addr & w.reg_mask) == w.reg_value;
  }

  inline MapperWindow *selectedRegister(BusSignals bus) {
    for (unsigned idx = 0; idx < windowCount; idx++) {
      if (isRegister(windows[idx], bus.addr))
        return &windows[idx];
    }

    return nullptr;
  }

  inline int selectedWindow(unsigned addr) {
    unsigned rom_offset = addr - windowBase;
    return (rom_offset >> windowShift) & (windowCount - 1);
  }

  // Every window the register serves, not just the first.
  inline void remap(BusSignals bus) {
    for (unsigned idx = 0; idx < windowCount; idx++) {
      if (isRegister(windows[idx], bus.addr)) {
        windows[idx].bank = bus.data & (bankCount - 1);
        resolveWindow(idx);
      }
    }
    resolveMirrors();
    return;
  }

  inline unsigned mirrored(unsigned addr) {
    for (unsigned idx = 0; idx < mirrorCount; idx++) {
      if (mirrors[idx].mirror_base <= addr && addr < mirrors[idx].mirror_top)
        return addr + mirrors[idx].source - mirrors[idx].mirror_base;
    }
    return addr;
  }

  inline unsigned calculateOffset(BusSignals bus) {
    return (bus.addr + delta[bus.addr >> pageShift]) & sizeMask;
  }
};

#endif /* MAPPER_H */
