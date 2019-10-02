;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; This is a simple program which outputs the ASCII table once.       ;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	.ORG	1000

_start:
loop:	TSTB	@#177564
	BPL	loop
	CMP	R0,	#40
	BLO	next
	CMP	R0,	#177
	BHIS	next
	MOVB	R0,	@#177566
next:	INC	R0
	JMP	loop

;;_start:
;;	MOV	#0,	R0
;;loop:	CMP	R0,	#40
;;	BLO	next
;;	CMP	R0,	#177
;;	BEQ	rst
;;	BHIS	next
;;	JSR	PC,	outch
;;next:	INC	R0
;;	JMP	loop
;;rst:	MOV	#15,	R0
;;	JSR	PC,	outch
;;	MOV	#12,	R0
;;	JSR	PC,	outch
;;	HALT
;;
;;outch:	TSTB	@#177564
;;	BPL	outch
;;	MOVB	R0,	@#177566
;;	RTS	PC
