	include	"portio.inc"

;; extern int __FASTCALL__ port_putc(uint8_t c);
;; writes data in L to port, no return value
_port_putc:
	ld	a,l		; can't write L directly to address
	ld	(IO_PUTC),a	; send data out port
	ret

