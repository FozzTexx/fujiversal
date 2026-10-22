#ifndef USERROM_H
#define USERROM_H

#include "types.h"
#include "FujiBusPacket.h" // for ByteBuffer

#include <cstring>

#define ROM_SEG_SIZE 16384
#ifdef PICO_RP2040
#define ROM_MAX_SEGS 8
#else
#define ROM_MAX_SEGS 16
#endif // PICO_RP2040

#define SIZE_8K   0x2000
#define SIZE_16K  0x4000
#define SIZE_32K  0x8000

class UserROM
{
private:
  uint8_t _romData[ROM_MAX_SEGS * ROM_SEG_SIZE];
  int _writePos = -1, _openSegment = -1;

public:
  uint8_t *activeBase = nullptr;

  // FIXME - should be private
  unsigned _selectedSegment = 0;
  size_t _segmentSize[ROM_MAX_SEGS] {};

  void open(unsigned seg);
  error_is_true write(const ByteBuffer &data);
  size_t close(void);
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
};

#endif /* USERROM_H */
