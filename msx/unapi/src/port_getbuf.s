	include	"portio.inc"

;; extern uint16_t __CALLEE__ port_getbuf(void *buf, uint16_t len, uint16_t timeout);
;; returns length of data received in HL
;; timeout resets when a character is received
_port_getbuf:
	pop	ix		; save return address
	pop	de		; timeout
	pop	bc		; len
	pop	iy		; buffer address
	push	ix		; restore return address

	ld	ix,0		; zero out received length

next_char:
	ld 	a,b		; check if length remaining
	or 	c		; merge low byte
	jr	z,getbuf_done

	ld	hl,de		; set timeout
	call	_port_getc_timeout
	ld	a,h		; check high byte
	or	a
	jr	nz,getbuf_done	; if high byte is set then timed out
	ld	a,l		; received byte is in L
	ld	(iy),a		; save byte to pointer
	inc	iy		; increment to next address
	dec	bc		; decrement length
	inc	ix		; increment length received
	jr	next_char	; check if there's more to receive

getbuf_done:
	ld	hl,ix		; return length received
	ret

