;--- FujiNet MSX UNAPI implementation
;    Based on unapi-rom.asm by Konamiman, 5-2019 (MIT License)

	PUBLIC	UNAPI_ENTRY
	INCLUDE	"const.inc"

UNAPI_ENTRY:
	push	hl
	push	af
	ld	hl,FN_TABLE
	bit	7,a

if	MAX_IMPFN >= 128

	jr	z,IS_STANDARD
	ld	hl,IMPFN_TABLE
	and	01111111b
	cp	MAX_IMPFN-128
	jr	z,OK_FNUM
	jr	nc,UNDEFINED
IS_STANDARD:

else

	jr	nz,UNDEFINED

endif

	cp	MAX_FN
	jr	z,OK_FNUM
	jr	nc,UNDEFINED

OK_FNUM:
	add	a,a
	push	de
	ld	e,a
	ld	d,0
	add	hl,de
	pop	de

	ld	a,(hl)
	inc	hl
	ld	h,(hl)
	ld	l,a

	pop	af
	ex	(sp),hl
	ret

	;--- Undefined function: return with registers unmodified

UNDEFINED:
	jp UNDEFINED
	pop	af
	pop	hl
	ret
