#ifndef MAPPER_H
#define MAPPER_H

class Mapper
{
private:
  volatile fujiROMType_t _type = ROM_TYPE_UNKNOWN;
  volatile uint16_t bankSize = ROM_SEG_SIZE;
  volatile uint16_t bankCount = 1;
  std::array<uint32_t, ROM_MAX_SEGS> bankOffset;

public:
  void setROMType(fujiROMType_t type) {
    _type = type;
    bankSize = _type & 0x80 ? SIZE_16K : SIZE_8K;
    return;
  }

  void resetBankOffsets(void) {
    unsigned idx, offset;


    for (idx = offset = 0; idx < ROM_MAX_SEGS; idx++, offset += bankSize)
      bankOffset[idx] = offset;
    return;
  }

  void setROMSize(size_t size) {
    bankCount = std::max<uint16_t>(1, (size + bankSize - 1) / bankSize);
    return;
  }

  inline bool isRemap(BusSignals bus) {
    return bus.rd && (0x4000 <= bus.addr) && (bus.addr < 0xC000);
  }

  inline void remap(BusSignals bus, UserROM &user_rom) {
    bool switch_bank = false;
    uint8_t bank = 0;


    switch (_type) {
    case ROM_TYPE_MSX_ASCII8:
      if ((0x6000 <= bus.addr) && (bus.addr < 0x8000)) {
        // 0x6000 = 0, 0x6800 = 1, 0x7000 = 2, 0x7800 = 3
        bank = (bus.addr >> 11) & 3;
        switch_bank = true;
      }
      break;
    case ROM_TYPE_MSX_ASCII16:
      if ((0x6000 <= bus.addr) && (bus.addr < 0x7800) && !(bus.addr & 0x0800)) {
        // 0x6000 = 0, 0x7000 = 1
        bank = (bus.addr >> 12) & 1;
        switch_bank = true;
      }
      break;
    case ROM_TYPE_MSX_KONAMI:
      // [0x4000..0x6000) is fixed at segment 0.
      if (0x6000 <= bus.addr && bus.addr < 0xC000) {
        // 0x6000 = 3, 0x8000 = 4, 0xA000 = 5
        // subtract 2 because ROM starts at 0x4000
        bank = (bus.addr >> 13) - 2;
        switch_bank = true;
      }
      break;
    case ROM_TYPE_MSX_KONAMI_SCC:
      if (0x5000 <= bus.addr && bus.addr < 0xC000 && (bus.addr & 0x1800) == 0x1000) {
        // 0x5000 = 2, 0x7000 = 3, 0x9000 = 4, 0xB000 = 5
        // subtract 2 because ROM starts at 0x4000
        bank = (bus.addr >> 13) - 2;
        switch_bank = true;
        // TODO: if bank = 4 clear SCC cache
      }
      break;
    default:
      break;
    }
    if (switch_bank)
      bankOffset[bank] = (bus.data % bankCount) * bankSize;
  }

  inline unsigned calculateOffset(BusSignals bus, UserROM &user_rom) {
    unsigned rom_offset = bus.addr;
    uint8_t bank;


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

    rom_offset -= BUS_ROM_BASE;
    bank = rom_offset >> (12 + (bankSize >> 13));

    return rom_offset + bankOffset[bank] - bank * bankSize;
  }
};

#endif /* MAPPER_H */
