	INCLUDE	"portio.inc"
	PUBLIC	_port_putbuf_slip

;-----------------------------------------------------------------------------
; uint16_t port_putbuf_slip(const void *buf, uint16_t len)
; Encode and transmit a SLIP-framed packet  (__CALLEE__ calling convention)
;
; Operation:
;   - 0xC0 -> sends 0xDB 0xDC
;   - 0xDB -> sends 0xDB 0xDD
;   - Other -> sends as-is
;
; Parameters (pushed right-to-left, callee cleans stack):
;   stack: buf (pushed second, on top below return address)
;   stack: len (pushed first, below buf)
;
; Stack on entry:
;   (SP+0) = return address
;   (SP+2) = len
;   (SP+4) = buf
;
; Returns:
;   HL = number of encoded bytes transmitted
;
; Register usage:
;   A  = current byte being processed
;   BC = remaining bytes to encode (counts down)
;   DE = encoded byte count (moved to HL on return)
;   HL = source buffer pointer
;-----------------------------------------------------------------------------

_port_putbuf_slip:
	; Pop return address, then parameters
	POP	HL			; HL = return address
	POP	BC			; BC = len
	POP	DE			; DE = buf
	PUSH	HL			; restore return address

	; Set up: HL = buf pointer, DE = encoded count (0)
	EX	DE, HL			; HL = buf, DE = 0 (will use as count)
	LD	DE, 0

	; Check for zero length
	LD	A, B
	OR	C
	JR	Z, slip_put_end

slip_put_loop:
	LD	A, (HL)			; Load byte from buffer
	INC	HL			; Advance pointer

	CP	SLIP_END		; 0xC0?
	JR	Z, slip_put_encode_end
	CP	SLIP_ESC		; 0xDB?
	JR	Z, slip_put_encode_esc

slip_put_send:
	LD	(IO_PUTC), A		; Write to memory-mapped output
	INC	DE			; Count encoded byte
	DEC	BC
	LD	A, B
	OR	C
	JR	NZ, slip_put_loop

slip_put_end:
	EX	DE, HL			; HL = encoded byte count (return value)
	RET

slip_put_encode_end:
	; Encode 0xC0 as 0xDB 0xDC
	LD	A, SLIP_ESC
	LD	(IO_PUTC), A
	INC	DE
	LD	A, SLIP_ESC_END
	JR	slip_put_send

slip_put_encode_esc:
	; Encode 0xDB as 0xDB 0xDD
	LD	A, SLIP_ESC
	LD	(IO_PUTC), A
	INC	DE
	LD	A, SLIP_ESC_ESC
	JR	slip_put_send
