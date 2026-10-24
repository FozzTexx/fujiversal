#include "UserROM.h"

#include <cstring>

#ifdef OBSOLETE
void UserROM::open(unsigned seg, fujiROMType_t type)
#else
void UserROM::open(unsigned seg)
#endif /* OBSOLETE */
{
  size_t offset;

  _openSegment = seg % ROM_MAX_SEGS;
  offset = _openSegment * ROM_SEG_SIZE;
  offset %= sizeof(_romData);
  _writePos = _openSegment * ROM_SEG_SIZE;
#ifdef OBSOLETE
  _type = type;
  bankSize = _type & 0x80 ? SIZE_16K : SIZE_8K;
#endif /* OBSOLETE */

  return;
}

error_is_true UserROM::write(const ByteBuffer &data)
{
  size_t len;
  const uint8_t *ptr;


  if (_writePos < 0)
    RETURN_ERROR_AS_TRUE();

  len = data.size();
  ptr = data.data();

  // Copy into romData and handle wrap around
  while (len) {
    size_t offset = _writePos % sizeof(_romData);
    size_t copy_len = std::min(sizeof(_romData) - offset, len);
    memcpy(&_romData[offset], ptr, copy_len);
    ptr += copy_len;
    _writePos += copy_len;
    len -= copy_len;
  }

  RETURN_SUCCESS_AS_FALSE();
}

#ifdef OBSOLETE
error_is_true UserROM::close(void)
#else
size_t UserROM::close(void)
#endif /* OBSOLETE */
{
  size_t size, offset;


  if (_writePos < 0)
#ifdef OBSOLETE
    RETURN_ERROR_AS_TRUE();
#else
    return 0;
#endif /* OBSOLETE */

  offset = _openSegment * ROM_SEG_SIZE;
  size = _segmentSize[_openSegment] = _writePos - offset;
#ifdef OBSOLETE
  bankCount = std::max<uint16_t>(1, (_segmentSize[_openSegment] + bankSize - 1) / bankSize);
#endif /* OBSOLETE */
  _writePos = _openSegment = -1;

#ifdef OBSOLETE
  RETURN_SUCCESS_AS_FALSE();
#else
  return size;
#endif /* OBSOLETE */
}

void UserROM::reset(void)
{
  deactivate();
  _writePos = _openSegment = -1;
  _selectedSegment = 0;
#ifdef OBSOLETE
  bankCount = 1;
#endif /* OBSOLETE */
  return;
}

#ifdef OBSOLETE
void UserROM::resetBankOffsets(void)
{
  unsigned idx, offset;


  for (idx = offset = 0; idx < ROM_MAX_SEGS; idx++, offset += bankSize)
    bankOffset[idx] = offset;
  return;
}
#endif /* OBSOLETE */
