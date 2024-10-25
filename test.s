;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; This is a simple program which outputs the ASCII table once.       ;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	.ORG	1000

_START:
LOOP:	TSTB	@#177564
	BPL	LOOP
	CMP	R0,	#40
	BLO	NEXT
	CMP	R0,	#177
	BHIS	NEXT
	MOVB	R0,	@#177566
NEXT:	INC	R0
	JMP	LOOP
