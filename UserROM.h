#ifndef USERROM_H
#define USERROM_H

#include "types.h"
#include "fujiROMType.h"
#include "FujiBusPacket.h" // for ByteBuffer

#include <array>
#include <cstring>

#define ROM_SEG_SIZE 16384
#ifdef PICO_RP2040
#define ROM_MAX_SEGS 8
#else
#define ROM_MAX_SEGS 16
#endif // PICO_RP2040

#define SIZE_8K   0x2000
#define SIZE_16K  0x4000

class UserROM
{
private:
  uint8_t _romData[ROM_MAX_SEGS * ROM_SEG_SIZE];
  size_t _segmentSize[ROM_MAX_SEGS] {};
  int _writePos = -1, _openSegment = -1;
  unsigned _selectedSegment = 0;

public:
  uint8_t *activeBase = nullptr;

  void open(unsigned seg, fujiROMType_t type);
  error_is_true write(const ByteBuffer &data);
  error_is_true close(void);
  void reset(void);

  inline void activate(void) {
      activeBase = &_romData[_selectedSegment * ROM_SEG_SIZE];
      return;
  }
  inline void deactivate(void) {
      activeBase = nullptr;
      return;
  }
  inline void selectSegment(unsigned seg) {
    _selectedSegment = seg % ROM_MAX_SEGS;
    if (activeBase)
      activate();
    return;
  }

  inline bool isClosed(void) { return _writePos < 0; }
  inline bool isActive(void) { return activeBase != nullptr; }

  // Mapper things
  volatile fujiROMType_t _type = ROM_TYPE_UNKNOWN;
  std::array<uint32_t, ROM_MAX_SEGS> bankOffset;
  volatile uint16_t bankSize = ROM_SEG_SIZE;
  volatile uint16_t bankCount = 1;
  void resetBankOffsets(void);
};

#endif /* USERROM_H */
