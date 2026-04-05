;--- FujiNet MSX UNAPI implementation
;    Based on unapi-rom.asm by Konamiman, 5-2019 (MIT License)

	PUBLIC TOUPPER

;--- Convert a character to upper-case if it is a lower-case letter

TOUPPER:
  cp  'a'
  ret  c
  cp  'z'+1
  ret  nc
  and  0DFh
  ret
