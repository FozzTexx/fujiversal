	include	"portio.inc"

;; extern int __CALLEE__ port_discard_until(uint8_t c, uint16_t timeout);
;; reads data until c is seen or timeout occurs, returns c or -1 on timeout
;; timeout *does not reset* when a character is received
_port_discard_until:
	pop	ix		; save return address
	pop	hl		; timeout
	pop	de		; char to look for
	push	ix		; restore return address

	call	timeout_init

not_yet:
	call	_port_getc	; get byte if there is one
	jr	nz,check_c	; zero flag clear if data
	push	de
	call	timeout_check
	pop	de
	jr	nc,not_yet	; if carry is clear then haven't timed out

	ld	hl,0xFFFF	; timed out, return -1
	jr	discard_done

check_c:
	ld	a,l		; check low byte
	cp	e
	jr	nz,not_yet	; try again

discard_done:
	call	timeout_cleanup
	ret

