;--- FujiNet MSX UNAPI implementation
;    Based on unapi-rom.asm by Konamiman, 5-2019 (MIT License)

	PUBLIC GETWRK
	INCLUDE "const.inc"

;--- Obtain slot work area (8 bytes) on SLTWRK
;    Input:  A  = Slot number
;    Output: HL = Work area address
;    Modifies: AF, BC

GETWRK:
  ld  b,a
  rrca
  rrca
  rrca
  and  01100000b
  ld  c,a  ;C = Slot * 32
  ld  a,b
  rlca
  and  00011000b  ;A = Subslot * 8
  or  c
  ld  c,a
  ld  b,0
  ld  hl,SLTWRK
  add  hl,bc
  ret
