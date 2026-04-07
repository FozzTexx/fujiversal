	include	"portio.inc"

;; extern uint16_t __CALLEE__ port_get_until(void *buf, uint16_t maxlen, uint8_t c,
;;					     uint16_t timeout);
;; reads until c is found, timeout, or maxlen is reached
;; returns length of data received, including c
;; timeout resets when a character is received
_port_get_until:
	pop	ix		; save return address
	pop	de		; timeout
	pop	bc		; char to look for
	ld	a,c		; save char in A register
	pop	bc		; maxlen
	pop	iy		; buffer address
	push	ix		; restore return address

	push	af		; save sentinel char we're searching for
	ld	ix,0		; zero out received length

next_until:
	ld 	a,b		; check if length remaining
	or 	c		; merge low byte
	jr	z,getuntil_done

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

	pop	hl		; restore sentinel char we're waiting for
	push	hl		; keep it for next time
	cp	h		; is this the end?
	jr	nz,next_until	; more to receive

getuntil_done:
	pop	hl		; discard sentinel char off stack
	ld	hl,ix		; return length received
	ret

