;--- FujiNet MSX UNAPI implementation
;    Based on unapi-rom.asm by Konamiman, 5-2019 (MIT License)

	PUBLIC DO_EXTBIO
	INCLUDE "const.inc"

;*******************************
;***  EXTBIO HOOK EXECUTION  ***
;*******************************

DO_EXTBIO:
  push  hl
  push  bc
  push  af
  ld  a,d
  cp  22h
  jr  nz,JUMP_OLD
  cp  e
  jr  nz,JUMP_OLD

  ;Check API ID

  ld  hl,UNAPI_ID
  ld  de,ARG
LOOP:  ld  a,(de)
  call  TOUPPER
  cp  (hl)
  jr  nz,JUMP_OLD2
  inc  hl
  inc  de
  or  a
  jr  nz,LOOP

  ;A=255: Jump to old hook

  pop  af
  push  af
  inc  a
  jr  z,JUMP_OLD2

  ;A=0: B=B+1 and jump to old hook

  call  GETSLT
  call  GETWRK

  if ALLOC_P3

  ld a,(hl)
  inc hl
  ld h,(hl)
  ld l,a

  endif

  pop  af
  pop  bc
  or  a
  jr  nz,DO_EXTBIO2
  inc  b
  ex  (sp),hl
  ld  de,2222h
  ret
DO_EXTBIO2:

  ;A=1: Return A=Slot, B=Segment, HL=UNAPI entry address

  dec  a
  jr  nz,DO_EXTBIO3
  pop  hl
  call  GETSLT
  ld  b,0FFh
  ld  hl,UNAPI_ENTRY
  ld  de,2222h
  ret

  ;A>1: A=A-1, and jump to old hook

DO_EXTBIO3:  ;A=A-1 already done
  ex  (sp),hl
  ld  de,2222h
  ret

  ;--- Jump here to execute old EXTBIO code

JUMP_OLD2:
  ld  de,2222h
JUMP_OLD:  ;Assumes "push hl,bc,af" done
  push  de
  call  GETSLT
  call  GETWRK

  if ALLOC_P3

  ld a,(hl)
  inc hl
  ld h,(hl)
  ld l,a

  endif

  pop  de
  pop  af
  pop  bc
  ex  (sp),hl
  ret
