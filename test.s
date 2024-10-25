;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; This is a simple program which outputs the ASCII table.            ;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	.ORG	1000

XCSR	=	#177564
XBUF	=	#177566

_START:
LOOP:	TSTB	@#XCSR
	BPL	LOOP
	CMP	R0,	#40
	BLO	NEXT
	CMP	R0,	#177
	BHIS	NEXT
	MOVB	R0,	@#XBUF
NEXT:	INC	R0
	JMP	LOOP
