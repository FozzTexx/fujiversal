;--- FujiNet MSX UNAPI implementation
;    Based on unapi-rom.asm by Konamiman, 5-2019 (MIT License)

	PUBLIC FN_ADD, FN_MULT

;--- Sample routine 1: adds two 8-bit numbers
;    Input: E, L = Numbers to add
;    Output: HL = Result

FN_ADD:
  ld  h,0
  ld  d,0
  add  hl,de
  ret


;--- Sample routine 2: multiplies two 8-bit numbers
;    Input: E, L = Numbers to multiply
;    Output: HL = Result

FN_MULT:
  ld  b,e
  ld  e,l
  ld  d,0
  ld  hl,0
MULT_LOOP:
  add  hl,de
  djnz  MULT_LOOP
  ret
