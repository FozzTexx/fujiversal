; MSX mapper test, assembled once per mapper (MAPPER, SEGS). Every segment is
; filled with its own number. Results: C000h windows, C002h mirrors (P/F/-).

MAP_PLAIN	equ	0
MAP_KONAMI	equ	1
MAP_KONAMI_SCC	equ	2
MAP_ASCII8	equ	3
MAP_ASCII16	equ	4

IF MAPPER = MAP_KONAMI || MAPPER = MAP_KONAMI_SCC
MIRRORS		equ	1
P3_SIG		equ	8000h		; "P3", then the routine
P3_ENTRY	equ	8002h
P3_SEG		equ	SEGS - 1
ELSE
MIRRORS		equ	0
ENDIF

; BIOS
ENASLT		equ	0024h
INITXT		equ	006Ch
CHPUT		equ	00A2h
POSIT		equ	00C6h
RSLREG		equ	0138h
LINL40		equ	0F3AEh
EXPTBL		equ	0FCC1h

	include	"probe.inc"

RAM		equ	0C000h
PROBE_MIN	equ	0800h		; segment 0's code must end below this
ROW_FIRST	equ	3

	org	4000h

	defb	"AB"
	defw	init
	defw	0, 0, 0
	defs	6, 0

init:
	di
	ld	a, 40
	ld	(LINL40), a
	call	INITXT

	; Select this slot in page 2 too.
	call	RSLREG
	rrca
	rrca
	and	03h
	ld	c, a
	ld	hl, EXPTBL
	add	a, l
	ld	l, a
	ld	a, (hl)
	and	80h
	or	c
	ld	c, a
	inc	hl
	inc	hl
	inc	hl
	inc	hl
	ld	a, (hl)
	and	0Ch
	or	c
	push	af
	ld	h, 80h
	call	ENASLT
	di
	pop	af

IF MIRRORS
	; Expanded: give pages 0 and 3 this subslot too. No stack while page 3
	; is switched.
	or	a
	jp	p, init_copy		; not expanded
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
	ld	a, (0FFFFh)
	cpl
	ld	e, a
	and	0Ch
	ld	d, a
	rrca
	rrca
	or	d
	ld	b, a
	ld	a, d
	rlca
	rlca
	rlca
	rlca
	or	b
	ld	b, a
	ld	a, e
	and	30h
	or	b
	ld	(0FFFFh), a
	ld	a, c
	out	(0A8h), a
ENDIF

init_copy:
	ld	hl, tester
	ld	de, RAM
	ld	bc, tester_end - tester
	ldir
	jp	main

; Runs from RAM, tables and strings included.

tester:
	phase	RAM

result:		defb	0
nfail:		defb	0
mresult:	defb	0
nmfail:		defb	0

w_hdr:
w_base:		defb	0
w_row:		defb	0
w_fixed:	defb	0		; nonzero: no register, never switches
w_fixseg:	defb	0
w_canon:	defw	0
w_probes:	defw	0, 0, 0
W_HDR_LEN	equ	$ - w_hdr
w_alt:		defw	0
w_out:		defw	0

wptr:		defw	0
cur_seg:	defb	0
other:		defb	0
expect:		defb	0

wr_addr:	defw	0
wr_val:		defb	0
rd_addr:	defw	0
rd_val:		defb	0

IF MIRRORS
m_hdr:
m_base:		defb	0
m_row:		defb	0
m_page:		defb	0
m_p2reg:	defw	0		; maps P3_SEG in at 8000h first; 0 = no
m_loreg:	defw	0		; selects what the page's lower 8K mirrors
m_lofix:	defb	0		;   ... or, with no register, what it shows
m_hireg:	defw	0
m_hifix:	defb	0
M_HDR_LEN	equ	$ - m_hdr

lo_wr:		defw	0
lo_exp:		defb	0
hi_wr:		defw	0
hi_exp:		defb	0
ENDIF

main:
	ld	hl, 0101h
	call	POSIT
	ld	hl, s_title
	call	puts
	ld	a, SEGS
	call	phex
	ld	hl, s_bank
	call	puts
	ld	hl, 0102h
	call	POSIT
	ld	hl, s_header
	call	puts

	ld	hl, windows
win_next:
	ld	a, (hl)
	or	a
	jr	z, all_done
	ld	de, w_hdr
	ld	bc, W_HDR_LEN
	ldir
	ld	(w_alt), hl
	call	skip_list
	ld	(w_out), hl
	call	skip_list
	ld	(wptr), hl
	call	test_window
	ld	hl, (wptr)
	jr	win_next

all_done:
IF MIRRORS
	ld	hl, mirrors
mir_next:
	ld	a, (hl)
	inc	a
	jr	z, mir_done
	ld	de, m_hdr
	ld	bc, M_HDR_LEN
	ldir
	ld	(wptr), hl
	ld	a, (m_base)
	ld	(w_base), a
	ld	a, (m_row)
	ld	(w_row), a
	call	test_mirror
	ld	hl, (wptr)
	jr	mir_next
mir_done:
ENDIF

	ld	a, (w_row)
	add	a, 2
	ld	l, a
	ld	h, 1
	push	hl
	call	POSIT
	ld	hl, s_windows
	ld	a, (nfail)
	call	report
	ld	(result), a
	pop	hl
IF MIRRORS
	inc	l
	call	POSIT
	ld	hl, s_mirrors
	ld	a, (nmfail)
	call	report
ELSE
	ld	a, '-'
ENDIF
	ld	(mresult), a
	ei
	jr	$

; HL = label, A = failure count. Returns P or F.
report:
	push	af
	call	puts
	pop	af
	or	a
	jr	nz, report_fail
	ld	hl, s_pass
	call	puts
	ld	a, 'P'
	ret
report_fail:
	push	af
	ld	hl, s_fail_n
	call	puts
	pop	af
	call	phex
	ld	a, 'F'
	ret

skip_list:
	ld	a, (hl)
	inc	hl
	add	a, a
	add	a, l
	ld	l, a
	ret	nc
	inc	h
	ret

test_window:
	xor	a
	ld	(cur_seg), a
seg_loop:
	call	show_progress
	ld	a, (cur_seg)
	inc	a
	and	SEGS - 1
	ld	(other), a

	ld	a, (w_fixed)
	or	a
	jr	z, switchable
	ld	a, (w_fixseg)
	ld	(expect), a
	ld	a, (cur_seg)
	jr	outside

switchable:
	ld	a, (cur_seg)
	ld	(expect), a
	ld	de, (w_canon)
	call	write_check
	jr	nz, win_fail

	ld	hl, (w_alt)
	ld	b, (hl)
	inc	hl
alt_loop:
	ld	a, (other)
	ld	(expect), a
	ld	de, (w_canon)
	call	write_check
	jr	nz, win_fail
	ld	e, (hl)
	inc	hl
	ld	d, (hl)
	inc	hl
	ld	a, (cur_seg)
	ld	(expect), a
	call	write_check
	jr	nz, win_fail
	djnz	alt_loop

	ld	a, (other)

outside:
	ld	c, a
	ld	hl, (w_out)
	ld	b, (hl)
	inc	hl
out_loop:
	ld	e, (hl)
	inc	hl
	ld	d, (hl)
	inc	hl
	ld	a, c
	call	write_check
	jr	nz, win_fail
	djnz	out_loop

	ld	a, (cur_seg)
	inc	a
	ld	(cur_seg), a
	cp	SEGS
	jr	c, seg_loop

	ld	hl, s_ok
	jp	puts

win_fail:
	ld	hl, nfail
	inc	(hl)
fail_detail:
	ld	hl, s_fail_w
	call	puts
	ld	hl, (wr_addr)
	call	phex16
	ld	a, '='
	call	CHPUT
	ld	a, (wr_val)
	call	phex
	ld	hl, s_fail_r
	call	puts
	ld	hl, (rd_addr)
	call	phex16
	ld	a, '='
	call	CHPUT
	ld	a, (rd_val)
	call	phex
	ld	hl, s_fail_e
	call	puts
	ld	a, (expect)
	jp	phex

IF MIRRORS
test_mirror:
	xor	a
	ld	(cur_seg), a
mir_seg:
	call	show_progress
	ld	a, (cur_seg)
	inc	a
	and	SEGS - 1
	ld	(other), a

	ld	hl, (m_p2reg)
	ld	a, h
	or	l
	jr	z, mir_lo
	ld	(hl), P3_SEG

mir_lo:
	ld	hl, (m_loreg)
	ld	(lo_wr), hl
	ld	a, h
	or	l
	jr	z, mir_lo_fixed
	ld	a, (cur_seg)
	ld	(hl), a
	ld	(lo_exp), a
	ld	a, (other)
	jr	mir_hi
mir_lo_fixed:
	ld	a, (m_lofix)
	ld	(lo_exp), a
	ld	a, (cur_seg)

mir_hi:
	ld	hl, (m_hireg)
	ld	(hi_wr), hl
	ld	b, a
	ld	a, h
	or	l
	ld	a, b
	jr	z, mir_hi_fixed
	ld	(hl), a
	jr	mir_hi_set
mir_hi_fixed:
	ld	a, (m_hifix)
mir_hi_set:
	ld	(hi_exp), a

	ld	a, (lo_exp)
	ld	d, a
	ld	a, (hi_exp)
	ld	e, a
	ld	ix, mir_back
	ld	a, (m_page)
	or	a
	jp	z, p0_check

	; Only jump if P3_SEG really is at 8000h.
	ld	hl, P3_SIG
	ld	a, (hl)
	ld	c, 'P'
	cp	c
	jr	nz, mir_nop3
	inc	hl
	ld	a, (hl)
	ld	c, '3'
	cp	c
	jp	z, P3_ENTRY
mir_nop3:
	ld	(rd_addr), hl
	ld	(rd_val), a
	ld	a, c
	ld	(expect), a
	ld	hl, (m_p2reg)
	ld	(wr_addr), hl
	ld	a, P3_SEG
	ld	(wr_val), a
	jr	mir_failed

mir_back:
	or	a
	jr	nz, mir_fail

	ld	a, (cur_seg)
	inc	a
	ld	(cur_seg), a
	cp	SEGS
	jp	c, mir_seg

	ld	hl, s_ok
	jp	puts

mir_fail:
	ld	(rd_addr), hl
	ld	a, b
	ld	(rd_val), a
	bit	5, h
	ld	hl, (lo_wr)
	ld	a, (lo_exp)
	jr	z, mir_fail_half
	ld	hl, (hi_wr)
	ld	a, (hi_exp)
mir_fail_half:
	ld	(wr_addr), hl
	ld	(wr_val), a
	ld	(expect), a
mir_failed:
	ld	hl, nmfail
	inc	(hl)
	jp	fail_detail

; D, E = expected lower/upper 8K. Returns via IX: A = 0, or A = 1, HL = addr,
; B = value read. Same contract as page3.asm.
p0_check:
	in	a, (0A8h)
	ld	c, a
	rrca
	rrca
	and	03h
	ld	b, a
	ld	a, c
	and	0FCh
	or	b
	out	(0A8h), a

	PROBE	0800h, d, p0_bad
	PROBE	1555h, d, p0_bad
	PROBE	1FF0h, d, p0_bad
	PROBE	2800h, e, p0_bad
	PROBE	3555h, e, p0_bad
	PROBE	3FF0h, e, p0_bad

	ld	a, c
	out	(0A8h), a
	xor	a
	jp	(ix)

p0_bad:
	ld	b, a
	ld	a, c
	out	(0A8h), a
	ld	a, 1
	jp	(ix)
ENDIF

show_progress:
	ld	a, (w_row)
	ld	l, a
	ld	h, 1
	call	POSIT
	ld	a, (w_base)
	call	phex
	ld	a, '0'
	call	CHPUT
	call	CHPUT
	ld	a, ' '
	call	CHPUT
	ld	a, (cur_seg)
	call	phex
	ld	a, '/'
	call	CHPUT
	ld	a, SEGS - 1
	jp	phex

; Keeps BC and HL.
write_check:
	ld	(wr_addr), de
	ld	(wr_val), a
	ld	(de), a

check:
	push	hl
	push	bc
	ld	hl, w_probes
	ld	b, 3
ck_loop:
	ld	e, (hl)
	inc	hl
	ld	d, (hl)
	inc	hl
	ld	a, (de)
	ld	c, a
	ld	a, (expect)
	cp	c
	jr	nz, ck_bad
	djnz	ck_loop
ck_out:
	pop	bc
	pop	hl
	ret
ck_bad:
	ld	(rd_addr), de
	ld	a, c
	ld	(rd_val), a
	jr	ck_out

puts:
	ld	a, (hl)
	or	a
	ret	z
	call	CHPUT
	inc	hl
	jr	puts

phex16:
	ld	a, h
	call	phex
	ld	a, l
phex:
	push	af
	rrca
	rrca
	rrca
	rrca
	call	pnib
	pop	af
pnib:
	and	0Fh
	add	a, 90h
	daa
	adc	a, 40h
	daa
	jp	CHPUT

s_header:	defm	"WIN  SEG/LAST", 0
s_ok:		defm	" OK", 0
s_fail_w:	defm	" FAIL W", 0
s_fail_r:	defm	" R", 0
s_fail_e:	defm	" E", 0
s_windows:	defm	"WINDOWS: ", 0
s_mirrors:	defm	"MIRRORS: ", 0
s_pass:		defm	"PASS", 0
s_fail_n:	defm	"FAIL ", 0

; Window: base, row, fixed, fixed seg, register, 3 probes, alt regs, outside
; addrs. Probes and writes avoid BFFCh-BFFFh (the adapter's IO window).

PROBES8		macro	base
		defw	base + PROBE_MIN, base + 1555h, base + 1FF0h
		endm
PROBES16	macro	base
		defw	base + PROBE_MIN, base + 2AAAh, base + 3FF0h
		endm

IF MAPPER = MAP_PLAIN
s_title:	defm	"Mapper test: Plain   ", 0
s_bank:		defm	"h x 16K", 0
windows:
	defb	40h, ROW_FIRST + 0, 1, 0
	defw	0
	PROBES16 4000h
	defb	0
	defb	8
	defw	4000h, 5000h, 6000h, 6800h, 7000h, 7800h, 7FFFh, 8000h
	defb	80h, ROW_FIRST + 1, 1, 1
	defw	0
	PROBES16 8000h
	defb	0
	defb	8
	defw	6000h, 7000h, 8000h, 9000h, 0A000h, 0B000h, 0B800h, 0BFF0h
ENDIF

IF MAPPER = MAP_KONAMI
s_title:	defm	"Mapper test: Konami  ", 0
s_bank:		defm	"h x 8K", 0
windows:
	defb	40h, ROW_FIRST + 0, 1, 0
	defw	0
	PROBES8	4000h
	defb	0
	defb	4
	defw	4000h, 4FFFh, 5000h, 5FFFh
	defb	60h, ROW_FIRST + 1, 0, 0
	defw	6000h
	PROBES8	6000h
	defb	3
	defw	7FFFh, 6B3Dh, 7000h
	defb	3
	defw	5FFFh, 5000h, 8000h
	defb	80h, ROW_FIRST + 2, 0, 0
	defw	8000h
	PROBES8	8000h
	defb	3
	defw	9FFFh, 9123h, 9000h
	defb	3
	defw	7FFFh, 0A000h, 4000h
	defb	0A0h, ROW_FIRST + 3, 0, 0
	defw	0A000h
	PROBES8	0A000h
	defb	3
	defw	0BFF0h, 0B456h, 0B000h
	defb	3
	defw	9FFFh, 6000h, 5FFFh
ENDIF

IF MAPPER = MAP_KONAMI_SCC
s_title:	defm	"Mapper test: KonamiSCC ", 0
s_bank:		defm	"h x 8K", 0
windows:
	defb	40h, ROW_FIRST + 0, 0, 0
	defw	5000h
	PROBES8	4000h
	defb	2
	defw	57FFh, 5355h
	defb	5
	defw	4000h, 4FFFh, 5800h, 5FFFh, 7000h
	defb	60h, ROW_FIRST + 1, 0, 0
	defw	7000h
	PROBES8	6000h
	defb	2
	defw	77FFh, 7421h
	defb	5
	defw	6000h, 6FFFh, 7800h, 7FFFh, 9000h
	defb	80h, ROW_FIRST + 2, 0, 0
	defw	9000h
	PROBES8	8000h
	defb	2
	defw	97FFh, 9602h
	defb	5
	defw	8000h, 8FFFh, 9800h, 9FFFh, 0B000h
	defb	0A0h, ROW_FIRST + 3, 0, 0
	defw	0B000h
	PROBES8	0A000h
	defb	2
	defw	0B7FFh, 0B1E7h
	defb	5
	defw	0A000h, 0AFFFh, 0B800h, 0BFF0h, 5000h
ENDIF

IF MAPPER = MAP_ASCII8
s_title:	defm	"Mapper test: ASCII8  ", 0
s_bank:		defm	"h x 8K", 0
windows:
	defb	40h, ROW_FIRST + 0, 0, 0
	defw	6000h
	PROBES8	4000h
	defb	2
	defw	67FFh, 6345h
	defb	4
	defw	5FFFh, 6800h, 8000h, 4000h
	defb	60h, ROW_FIRST + 1, 0, 0
	defw	6800h
	PROBES8	6000h
	defb	2
	defw	6FFFh, 6ABCh
	defb	4
	defw	67FFh, 7000h, 4000h, 0A000h
	defb	80h, ROW_FIRST + 2, 0, 0
	defw	7000h
	PROBES8	8000h
	defb	2
	defw	77FFh, 7456h
	defb	4
	defw	6FFFh, 7800h, 8000h, 9000h
	defb	0A0h, ROW_FIRST + 3, 0, 0
	defw	7800h
	PROBES8	0A000h
	defb	2
	defw	7FFFh, 7BCDh
	defb	4
	defw	77FFh, 8000h, 0A000h, 0B000h
ENDIF

IF MAPPER = MAP_ASCII16
s_title:	defm	"Mapper test: ASCII16 ", 0
s_bank:		defm	"h x 16K", 0
windows:
	defb	40h, ROW_FIRST + 0, 0, 0
	defw	6000h
	PROBES16 4000h
	defb	2
	defw	67FFh, 6234h
	defb	5
	defw	5FFFh, 6800h, 6FFFh, 7000h, 8000h
	defb	80h, ROW_FIRST + 1, 0, 0
	defw	7000h
	PROBES16 8000h
	defb	2
	defw	77FFh, 7567h
	defb	5
	defw	6000h, 6FFFh, 7800h, 7FFFh, 0B000h
ENDIF

	defb	0

; Mirror: base, row, page, P3_SEG register, lower 8K reg (0 = fixed) + fixed
; seg, upper 8K the same. Per openMSX RomKonami.cc / RomKonamiSCC.cc.

IF MAPPER = MAP_KONAMI
mirrors:
	defb	00h, ROW_FIRST + 4, 0
	defw	0
	defw	0
	defb	0
	defw	6000h
	defb	0
	defb	0C0h, ROW_FIRST + 5, 3
	defw	8000h
	defw	0
	defb	P3_SEG
	defw	0A000h
	defb	0
	defb	0FFh
ENDIF

IF MAPPER = MAP_KONAMI_SCC
mirrors:
	defb	00h, ROW_FIRST + 4, 0
	defw	0
	defw	9000h
	defb	0
	defw	0B000h
	defb	0
	defb	0C0h, ROW_FIRST + 5, 3
	defw	9000h
	defw	5000h
	defb	0
	defw	7000h
	defb	0
	defb	0FFh
ENDIF

	dephase
tester_end:

