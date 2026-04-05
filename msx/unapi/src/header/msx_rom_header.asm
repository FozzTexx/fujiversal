	EXTERN INIT

_header_start:
	defb "AB"	; MSX ROM Signature
	defw INIT	; Init routine

	;;  Pad to 16 bytes from the start of the header
	defs 16 - ($ - _header_start), 0
