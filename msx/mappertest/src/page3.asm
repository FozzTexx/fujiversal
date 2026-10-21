; Page 3 mirror check, at the start of the last segment and run at 8000h:
; page 3 RAM is gone while it runs, so no stack. In: D, E = expected
; C000h/E000h halves, IX = return. Out: A = 0, or A = 1, HL = addr, B = value.

	include	"probe.inc"

	org	8000h

	defm	"P3"

p3_check:
	in	a, (0A8h)
	ld	c, a
	and	0Ch
	rlca
	rlca
	rlca
	rlca
	ld	b, a
	ld	a, c
	and	3Fh
	or	b
	out	(0A8h), a

	PROBE	0C800h, d, p3_bad
	PROBE	0D555h, d, p3_bad
	PROBE	0DFF0h, d, p3_bad
	PROBE	0E800h, e, p3_bad
	PROBE	0F555h, e, p3_bad
	PROBE	0FFF0h, e, p3_bad

	ld	a, c
	out	(0A8h), a
	xor	a
	jp	(ix)

p3_bad:
	ld	b, a
	ld	a, c
	out	(0A8h), a
	ld	a, 1
	jp	(ix)
