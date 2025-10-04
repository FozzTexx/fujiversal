	extern	_INIT

	ORG	$4000
	DB	"AB"		; cartridge ID
	DW	_INIT		; pointer to INIT
	DW	0		; pointer to STATEMENT
	DW	0		; pointer to DEVICE
	DW	0		; pointer to TEXT
