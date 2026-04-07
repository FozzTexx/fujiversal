	include	"portio.inc"

;; extern int __FASTCALL__ port_getc_timeout(uint16_t timeout);
;; timeout is in HL
;; loop until data is available or timeout elapses
_port_getc_timeout:
	push	bc		; save BC and DE because
	push	de		; timeout_check destroys them
	call	timeout_init
wait_rx:
	call	_port_getc	; get byte if there is one
	jr	nz,getc_done	; zero flag clear if data
	call	timeout_check
	jr	nc,wait_rx	; if carry is clear then haven't timed out

	ld	hl,0xFFFF	; timed out, return -1
getc_done:
	call	timeout_cleanup
	pop	de		; restore saved DE
	pop	bc		; restore saved BC
	ret

