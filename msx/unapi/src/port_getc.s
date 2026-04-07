	include	"portio.inc"

;; extern int __FASTCALL__ port_getc();
;; returns signed int with data or -1 if no data is available, zero flag clear if data
_port_getc:
	ld	a,(IO_STATUS)
	bit	7,a		; set Z flag based on high bit
	jr	nz,get_data	; if high bit is set there is data
	ld	hl,0xFFFF	; no data avail, return -1
	ret

get_data:
	ld	a,(IO_GETC)
	ld	l,a		; transfer data to L register
	ld	h,0		; set high byte to 0
	ret

