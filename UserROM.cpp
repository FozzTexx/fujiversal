#include "UserROM.h"

#include <cstring>

void UserROM::open(unsigned seg, fujiROMType_t type)
{
  size_t offset;

  _openSegment = seg % ROM_MAX_SEGS;
  offset = _openSegment * ROM_SEG_SIZE;
  offset %= sizeof(_romData);
  _writePos = _openSegment * ROM_SEG_SIZE;
  _type = type;
  bankSize = _type & 0x80 ? SIZE_16K : SIZE_8K;

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

error_is_true UserROM::close(void)
{
  if (_writePos < 0)
    RETURN_ERROR_AS_TRUE();

  size_t offset = _openSegment * ROM_SEG_SIZE;
  _segmentSize[_openSegment] = _writePos - offset;
  bankCount = std::max<uint16_t>(1, (_segmentSize[_openSegment] + bankSize - 1) / bankSize);
  _writePos = _openSegment = -1;

  RETURN_SUCCESS_AS_FALSE();
}

void UserROM::reset(void)
{
  deactivate();
  _writePos = _openSegment = -1;
  _selectedSegment = 0;
  bankCount = 1;
  return;
}

void UserROM::resetBankOffsets(void)
{
  unsigned idx, offset;


  for (idx = offset = 0; idx < ROM_MAX_SEGS; idx++, offset += bankSize)
    bankOffset[idx] = offset;
  return;
}
