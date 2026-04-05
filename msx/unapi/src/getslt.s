;--- FujiNet MSX UNAPI implementation
;    Based on unapi-rom.asm by Konamiman, 5-2019 (MIT License)

	PUBLIC GETSLT
	INCLUDE "const.inc"

;--- Get slot connected on page 1
;    Input:  -
;    Output: A = Slot number
;    Modifies: AF, HL, E, BC

GETSLT:
  di
  exx
  in  a,(0A8h)
  ld  e,a
  and  00001100b
  sra  a
  sra  a
  ld  c,a  ;C = Slot
  ld  b,0
  ld  hl,EXPTBL
  add  hl,bc
  bit  7,(hl)
  jr  z,NOEXP1
EXP1:  inc  hl
  inc  hl
  inc  hl
  inc  hl
  ld  a,(hl)
  and  00001100b
  or  c
  or  80h
  ld  c,a
NOEXP1:  ld  a,c
  exx
  ei
  ret
