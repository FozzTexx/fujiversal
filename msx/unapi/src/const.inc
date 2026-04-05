;--- FujiNet MSX UNAPI implementation
;    Based on unapi-rom.asm by Konamiman, 5-2019 (MIT License)

	EXTERN UNAPI_ID
	EXTERN TOUPPER
	EXTERN INITMSG
	EXTERN GETSLT
	EXTERN GETWRK
	EXTERN DO_EXTBIO
	EXTERN INITMSG
	EXTERN UNAPI_ENTRY
	EXTERN FN_ADD
	EXTERN FN_MULT
	EXTERN FN_TABLE

;*******************
;***  CONSTANTS  ***
;*******************

; 0: Copy the old EXTBIO hook directly to the area for our slot in SLTWRK
;    (enough if we don't need space in page 3 for anything else).
;
; 1: Allocate 5 bytes in page 3, copy old EXTBIO hook there, set allocated address
;    in the area for our slot in SLTWRK.

ALLOC_P3: equ 0

;--- System variables and routines

CHPUT:  equ  00A2h
HOKVLD:  equ  0FB20h
EXPTBL:  equ  0FCC1h
EXTBIO:  equ  0FFCAh
SLTWRK:  equ  0FD09h
ARG:  equ  0F847h

;--- API version and implementation version

;TODO: Adjust for your implementation

API_V_P:  equ  1
API_V_S:  equ  0
ROM_V_P:  equ  1
ROM_V_S:  equ  0

;--- Maximum number of available standard and implementation-specific function numbers

;TODO: Adjust for your implementation

;Must be 0 to 127
MAX_FN:    equ  2

;Must be either zero (if no implementation-specific functions available), or 128 to 254
MAX_IMPFN:  equ  0


