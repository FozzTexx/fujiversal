	extern	_port_getc, _port_getc_timeout, _port_discard_until
	extern	_port_getbuf, _port_get_until
	extern	_port_putc, _port_putbuf

	extern	getbuf_done

	extern	timeout_init, timeout_check, timeout_cleanup

	IO_OFFSET	EQU	(0x8000 + 0x3FFC)
	IO_GETC		EQU	(IO_OFFSET + 0)
	IO_STATUS	EQU	(IO_OFFSET + 1)
	IO_PUTC		EQU	(IO_OFFSET + 2)
	IO_CONTROL	EQU	(IO_OFFSET + 3)

	SLIP_END	EQU	0xC0
	SLIP_ESC	EQU	0xDB
	SLIP_ESC_END	EQU	0xDC
	SLIP_ESC_ESC	EQU	0xDD
