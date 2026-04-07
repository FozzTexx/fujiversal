	include	"portio.inc"

;; extern uint16_t __CALLEE__ port_putbuf(void *buf, uint16_t len);
;; returns number of bytes sent in HL
_port_putbuf:
	pop	ix		; save return address
	pop	bc		; len
	pop	iy		; buffer address
	push	ix		; restore return address
	ld	ix,bc		; cheat: will always return length

putc:
	ld 	a,b		; check if length remaining
	or 	c		; merge low byte
	jr	z,putbuf_done

	ld	a,(iy)		; load byte to send
	ld	(IO_PUTC),a	; send data out port
	inc 	iy		; increment to next address
	dec 	bc		; decrement length
	jr	putc		; check if more to transmit

putbuf_done:
	ld	hl,ix		; return length transmitted in HL
	ret
