;--- FujiNet MSX UNAPI implementation
;    Based on unapi-rom.asm by Konamiman, 5-2019 (MIT License)

	PUBLIC INITMSG, UNAPI_ID, APIINFO, FN_TABLE
	INCLUDE "const.inc"

  ;--- Specification identifier (up to 15 chars)

UNAPI_ID:
  db  "SIMPLE_MATH",0

  ;--- Implementation identifier (up to 63 chars and zero terminated)

APIINFO:
  db  "Konamiman's ROM implementation of SIMPLE_MATH UNAPI",0

  ;--- Other data

INITMSG:
  db  13,10,"UNAPI Sample ROM 1.0 (SIMPLE_MATH)",13,10
  db  "(c) 2019 by Konamiman",13,10
  db  13,10
  db  0

;--- Standard routines addresses table

FN_TABLE:
FN_0:  dw  FN_INFO
FN_1:  dw  FN_ADD
FN_2:  dw  FN_MULT

;--- Mandatory routine 0: return API information
;    Input:  A  = 0
;    Output: HL = Descriptive string for this implementation, on this slot, zero terminated
;            DE = API version supported, D.E
;            BC = This implementation version, B.C.
;            A  = 0 and Cy = 0

FN_INFO:
  ld  bc,256*ROM_V_P+ROM_V_S
  ld  de,256*API_V_P+API_V_S
  ld  hl,APIINFO
  xor  a
  ret

