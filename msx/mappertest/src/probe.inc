; Jump to bad with A = value, HL = addr unless (addr) = want.
PROBE	macro	addr, want, bad
	ld	hl, addr
	ld	a, (hl)
	cp	want
	jr	nz, bad
	endm
