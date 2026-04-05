	extern	init

	ORG	$4000
	DB	"AB"		; cartridge ID
	DW	init		; pointer to INIT
	DW	0		; pointer to STATEMENT
	DW	0		; pointer to DEVICE
	DW	0		; pointer to TEXT
