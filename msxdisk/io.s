
	public	_port_getc
	public	_port_putc
	public	init

	extern	_INIT

	IO_OFFSET	EQU	(0x4000 + 0x3FFC)
	IO_GETC		EQU	(IO_OFFSET + 0)
	IO_STATUS	EQU	(IO_OFFSET + 1)
	IO_PUTC		EQU	(IO_OFFSET + 2)
	IO_CONTROL	EQU	(IO_OFFSET + 3)

;; Returns data in HL or -1 if no data avail
_port_getc:
	ld	a,(IO_STATUS)
	jp	m,have_data
	ld	hl,0xFFFF
	ret

have_data:
	ld	a,(IO_GETC)
	ld	l,a
	ld	h,0
	ret

;; Writes data in L to port
_port_putc:
	ld	a,l
	ld	(IO_PUTC),a
	ret

H_STKE	EQU	0xFEDA

init:
	ld	a,c		; Get the ROM slot number

	ld	hl,INIT
	ld	de,H_STKE
	ld	bc,4
	ldir			; Copy the routine to execute the ROM to the hook
	ld	(H_STKE+1),a	; Put the ROM slot number to the hook
	ret			; Back to slots scanning
