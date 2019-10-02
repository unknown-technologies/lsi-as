#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "as.h"

void ASInit(AS* as)
{
	as->buf = (char*) malloc(256);
	as->mnemonic = as->buf;
	as->labels = NULL;
	as->size = 32;
	as->code = (u16*) malloc(as->size * sizeof(u16));
	as->wr = 0;
	as->rd = 0;
	as->state = 0;
	as->pc = 0;
}

void ASDestroy(AS* as)
{
	free(as->code);
	free(as->buf);
	while(as->labels) {
		LABEL* lbl = as->labels;
		as->labels = lbl->next;
		free((char*) lbl->name);
		free(lbl);
	}
}

void ASSetSource(AS* as, const char* source)
{
	as->source = source;
}

#define	STATE_BOL	0
#define	STATE_LABEL	1
#define	STATE_SPACE	2
#define	STATE_MNEMONIC	3
#define	STATE_SEPARATOR	4
#define	STATE_ARG1	5
#define	STATE_SEP	6
#define	STATE_ARG2	7
#define	STATE_COMMENT	8
#define	STATE_DIRECTIVE	9
#define	STATE_DIR_SEP	10
#define	STATE_DIR_ARG	11
#define	STATE_EOL	12
#define	STATE_END	13
#define	STATE_ERROR	14

void ASError(AS* as, const char* msg)
{
	as->state = STATE_ERROR;
	printf("Error: %s\n", msg);
}

#define	MATCH(mnemo)	!strcmp(as->mnemonic, mnemo)
#define	WRITE(opcd)	as->code[as->wr++] = (opcd), as->pc += 2

#define	OP1()		if(!as->arg1) { \
	ASError(as, "Missing operand"); \
	return; \
} \
as->code[wr_save] |= ASWriteOperand(as, as->arg1)

#define	OP2()		if(!as->arg2) { \
	ASError(as, "Missing operand"); \
	return; \
} \
as->code[wr_save] |= (ASWriteOperand(as, as->arg1) << 6); \
as->code[wr_save] |= ASWriteOperand(as, as->arg2)

#define	WRITE0(opcd)	WRITE(opcd)

#define	WRITE1(opcd)	{ \
	u16 wr_save = as->wr; \
	WRITE(opcd); \
	OP1(); \
}

#define	WRITE2(opcd)	{ \
	u16 wr_save = as->wr; \
	WRITE(opcd); \
	OP2(); \
}

#define	WRITEBR(opcd)	if(!as->arg1) { \
	ASError(as, "missing operand"); \
	return; \
} \
WRITE(opcd | ASGetOffset(as, as->arg1))

#define	WRITEJSR(opcd)	{ \
	u16 wr_save = as->wr; \
	if(!as->arg1 || !as->arg2) { \
		ASError(as, "missing operand"); \
		return; \
	} \
	WRITE(opcd | (ASGetRegister(as, as->arg1) << 6)); \
	as->code[wr_save] |= ASWriteOperand(as, as->arg2); \
}

#define	STATE_DEF	20
#define	STATE_IMM	21
#define	STATE_IMM0	22
#define	STATE_IMMLBL	23
#define	STATE_AD	24
#define	STATE_R		25
#define	STATE_MEM	26
#define	STATE_IDX	27
#define	STATE_LBL	28

int ASCheckLabel(const char* s)
{
	if(!(((*s >= 'A') && (*s <= 'Z'))
			|| ((*s >= 'a') && (*s <= 'z'))
			|| (*s == '_'))) {
		return 0;
	}
	for(; *s; s++) {
		const char c = *s;
		if(!(((c >= 'A') && (c <= 'Z'))
				|| ((c >= 'a') && (c <= 'z'))
				|| ((c >= '0') && (c <= '9'))
				|| (c == '_'))) {
			return 0;
		}
	}
	return 1;
}

LABEL* ASFindLabel(AS* as, const char* name)
{
	LABEL* lbl = as->labels;
	while(lbl) {
		if(!strcmp(lbl->name, name)) {
			return lbl;
		}
		lbl = lbl->next;
	}
	return NULL;
}

void ASFixup(AS* as, LABEL* lbl)
{
	s16 diff;
	FIXUP* last = NULL;
	FIXUP* fix = as->fixups;
	while(fix) {
		FIXUP* next = fix->next;
		if(fix->label == lbl) {
			diff = lbl->addr - fix->addr;
			switch(fix->type) {
				case FIXUP_BR:
					if(lbl->addr & 1) {
						ASError(as, "invalid target address");
						return;
					}
					diff >>= 1;
					if((diff >= 0177) || (diff <= -0200)) {
						ASError(as, "BTA out of range");
						return;
					}
					as->code[fix->pos] |= diff & 0377;
					break;
				case FIXUP_REL:
					as->code[fix->pos] = diff;
					break;
				case FIXUP_ABS:
					as->code[fix->pos] = lbl->addr;
					break;
				default:
					ASError(as, "unknown fixup type");
					return;
			}

			/* delete fixup */
			if(last) {
				last->next = next;
			} else {
				as->fixups = next;
			}
			free(fix);
		} else {
			last = fix;
		}
		fix = next;
	}
	return;
}

u16 ASGetOffset(AS* as, const char* op)
{
	int i = 0;
	int neg = 0;
	if(op[0] == '.') {
		u16 value;
		for(; op[i] && (op[i] == ' ' || op[i] == '\t'); i++);
		if(op[i] == '-') {
			neg = 1;
		} else if(op[i] == '+') {
			neg = 0;
		} else {
			ASError(as, "invalid offset");
			return 0;
		}

		for(; op[i]; i++) {
			if((op[i] >= '0') && (op[i] <= '7')) {
				value <<= 3;
				value |= op[i] - '0';
			} else {
				ASError(as, "invalid offset");
				return 0;
			}
		}

		if((neg && (value > 0200)) || (!neg && (value > 0177))) {
			ASError(as, "target out of range");
			return 0;
		} else {
			if(neg) {
				value = -value;
			}
			return value & 0377;
		}
	} else {
		FIXUP* fix;
		LABEL* lbl;

		if(!ASCheckLabel(op)) {
			ASError(as, "invalid label");
			return 0;
		}

		lbl = ASFindLabel(as, op);

		if(!lbl) {
			lbl = (LABEL*) malloc(sizeof(LABEL));
			lbl->name = strdup(op);
			lbl->resolved = 0;
			lbl->next = as->labels;
			as->labels = lbl;
		}

		if(!lbl->resolved) {
			fix = (FIXUP*) malloc(sizeof(FIXUP));
			fix->label = lbl;
			fix->addr = as->pc + 2;
			fix->pos = as->wr;
			fix->type = FIXUP_BR;
			fix->next = as->fixups;
			as->fixups = fix;
			return 0;
		} else {
			s16 off = lbl->addr - as->pc - 2;
			if(lbl->addr & 1) {
				ASError(as, "invalid target address");
				return 0;
			}
			off >>= 1;
			if((off >= 0177) || (off <= -0200)) {
				ASError(as, "BTA out of range");
				return 0;
			}
			return off & 0377;
		}
	}
}

u16 ASGetRegister(AS* as, const char* r)
{
	if(r[0] == 0) {
		ASError(as, "invalid register");
		return 0;
	} else if((r[0] == 'R') && (r[1] >= '0') && (r[1] <= '7')) {
		return r[1] - '0';
	} else if((r[0] == 'P') && (r[1] == 'C')) {
		return 7;
	} else if((r[0] == 'S') && (r[1] == 'P')) {
		return 6;
	} else {
		ASError(as, "invalid register");
		return 0;
	}
}

u16 ASWriteOperand(AS* as, char* arg)
{
	int i;
	int state = STATE_BOL;
	int def = 0;
	char* name = NULL;
	u16 value = 0;
	for(i = 0; 1; i++) {
		const char c = arg[i];
		switch(state) {
			case STATE_BOL:
				switch(c) {
					case '#':
						state = STATE_IMM0;
						break;
					case '@':
						if(def) {
							ASError(as, "syntax error");
							return 0;
						}
						def = 010;
						state = STATE_BOL;
						break;
					case '-':
						state = STATE_AD;
						break;
					case 'R':
						state = STATE_R;
						break;
					case '(':
						state = STATE_MEM;
						break;
					case '0':
					case '1':
					case '2':
					case '3':
					case '4':
					case '5':
					case '6':
					case '7':
						state = STATE_IDX;
						break;
					default:
						if(((c >= 'A') && (c <= 'Z'))
								|| ((c >= 'a') && (c <= 'z'))
								|| (c == '_')) {
							name = &arg[i];
							state = STATE_LBL;
						} else {
							ASError(as, "unknown operand type");
							return 0;
						}
						break;
				}
				break;
			case STATE_IMM0:
				if(c >= '0' && c <= '7') {
					state = STATE_IMM;
					value = c - '0';
				} else if(((c >= 'A') && (c <= 'Z'))
					|| ((c >= 'a') && (c <= 'z'))
					|| (c == '_')) {
					state = STATE_IMMLBL;
					name = &arg[i];
				} else {
					ASError(as, "invalid operand");
					return 0;
				}
				break;
			case STATE_IMM:
				switch(c) {
					case 0:
						WRITE(value);
						return 027 | def;
					case '0':
					case '1':
					case '2':
					case '3':
					case '4':
					case '5':
					case '6':
					case '7':
						value <<= 3;
						value |= c - '0';
						break;
					default:
						ASError(as, "invalid number");
						return 0;
				}
				break;
			case STATE_IMMLBL:
				if(((c >= 'A') && (c <= 'Z'))
					|| ((c >= 'a') && (c <= 'z'))
					|| (c == '_')) {
					/* continue */
				} else if(c == 0) {
					LABEL* lbl = ASFindLabel(as, name);
					if(!lbl) {
						lbl = (LABEL*) malloc(sizeof(LABEL));
						lbl->name = strdup(name);
						lbl->resolved = 0;
						lbl->next = as->labels;
						as->labels = lbl;
					}
					if(lbl->resolved) {
						WRITE(lbl->addr);
					} else {
						FIXUP* fix = (FIXUP*) malloc(sizeof(FIXUP));
						fix->label = lbl;
						fix->addr = as->pc + 2;
						fix->pos = as->wr;
						fix->type = FIXUP_ABS;
						fix->next = as->fixups;
						as->fixups = fix;
						WRITE(0);
					}
					return 027 | def;
				} else {
					ASError(as, "invalid label");
					return 0;
				}
				break;
			case STATE_R:
				if((c >= '0') && (c <= '7')) {
					if(arg[i + 1] == 0) {
						return (c - '0') | def;
					} else {
						ASError(as, "invalid register");
						return 0;
					}
				} else if(((c >= 'A') && (c <= 'Z'))
						|| ((c >= 'a') && (c <= 'z'))
						|| ((c >= '0') && (c <= '9'))
						|| (c == '_')) {
					name = &arg[i - 1];
					state = STATE_LBL;
				} else {
					ASError(as, "unknown operand type");
					return 0;
				}
				break;
			case STATE_LBL:
				if(c == 0) {
					if(!strcmp(name, "PC")) {
						return 007 | def;
					} else if(!strcmp(name, "PC")) {
						return 006 | def;
					} else {
						LABEL* lbl = ASFindLabel(as, name);
						if(!lbl) {
							lbl = (LABEL*) malloc(sizeof(LABEL));
							lbl->name = strdup(name);
							lbl->resolved = 0;
							lbl->next = as->labels;
							as->labels = lbl;
						}
						if(lbl->resolved) {
							s16 diff = lbl->addr - as->pc - 2;
							WRITE(diff);
						} else {
							FIXUP* fix = (FIXUP*) malloc(sizeof(FIXUP));
							fix->label = lbl;
							fix->addr = as->pc + 2;
							fix->pos = as->wr;
							fix->type = FIXUP_REL;
							fix->next = as->fixups;
							as->fixups = fix;
							WRITE(0);
						}
						return 067 | def;
					}
				} else if(!(((c >= 'A') && (c <= 'Z'))
						|| ((c >= 'a') && (c <= 'z'))
						|| ((c >= '0') && (c <= '9'))
						|| (c == '_'))) {
					ASError(as, "invalid operand");
					return 0;
				}
				break;
		}

		if(!c) {
			break;
		}
	}
	ASError(as, "internal compiler error");
	printf("operand: '%s'\n", arg);
	return 0;
}

const INSN insns[] = {
	{"BPT",		0000003, INSN_NONE},
	{"IOT",		0000004, INSN_NONE},
	{"RTI",		0000002, INSN_NONE},
	{"HALT",	0000000, INSN_NONE},
	{"WAIT",	0000001, INSN_NONE},
	{"RESET",	0000005, INSN_NONE},
	{"CLC",		0000241, INSN_NONE},
	{"CLV",		0000242, INSN_NONE},
	{"CLZ",		0000244, INSN_NONE},
	{"CLN",		0000250, INSN_NONE},
	{"CCC",		0000257, INSN_NONE},
	{"SEC",		0000261, INSN_NONE},
	{"SEV",		0000262, INSN_NONE},
	{"SEZ",		0000264, INSN_NONE},
	{"SEN",		0000270, INSN_NONE},
	{"SCC",		0000277, INSN_NONE},
	{"CLR",		0005000, INSN_SINGLE},
	{"CLRB",	0105000, INSN_SINGLE},
	{"COM",		0005100, INSN_SINGLE},
	{"COMB",	0105100, INSN_SINGLE},
	{"INC",		0005200, INSN_SINGLE},
	{"INCB",	0105200, INSN_SINGLE},
	{"DEC",		0005300, INSN_SINGLE},
	{"DECB",	0105300, INSN_SINGLE},
	{"NEG",		0005400, INSN_SINGLE},
	{"NEGB",	0105400, INSN_SINGLE},
	{"TST",		0005700, INSN_SINGLE},
	{"TSTB",	0105700, INSN_SINGLE},
	{"ASR",		0006200, INSN_SINGLE},
	{"ASRB",	0106200, INSN_SINGLE},
	{"ASL",		0006300, INSN_SINGLE},
	{"ASLB",	0106300, INSN_SINGLE},
	{"ROR",		0006000, INSN_SINGLE},
	{"RORB",	0106000, INSN_SINGLE},
	{"ROL",		0006100, INSN_SINGLE},
	{"ROLB",	0106100, INSN_SINGLE},
	{"SWAB",	0000300, INSN_SINGLE},
	{"ADC",		0005500, INSN_SINGLE},
	{"ADCB",	0105500, INSN_SINGLE},
	{"SBC",		0005600, INSN_SINGLE},
	{"SBCB",	0105600, INSN_SINGLE},
	{"SXT",		0006700, INSN_SINGLE},
	{"MFPS",	0106700, INSN_SINGLE},
	{"MTPS",	0106400, INSN_SINGLE},
	{"JMP",		0000100, INSN_SINGLE},
	{"MOV",		0010000, INSN_DOUBLE},
	{"MOVB",	0110000, INSN_DOUBLE},
	{"CMP",		0020000, INSN_DOUBLE},
	{"CMPB",	0120000, INSN_DOUBLE},
	{"ADD",		0060000, INSN_DOUBLE},
	{"ADDB",	0160000, INSN_DOUBLE},
	{"BIT",		0030000, INSN_DOUBLE},
	{"BITB",	0130000, INSN_DOUBLE},
	{"BIC",		0040000, INSN_DOUBLE},
	{"BICB",	0140000, INSN_DOUBLE},
	{"BIS",		0050000, INSN_DOUBLE},
	{"BISB",	0150000, INSN_DOUBLE},
	{"XOR",		0074000, INSN_JSR},
	{"JSR",		0004000, INSN_JSR},
	{"SOB",		0077000, INSN_JSR},
	{"RTS",		0000200, INSN_RTS},
	{"BR",		0000400, INSN_BRANCH},
	{"BNE",		0001000, INSN_BRANCH},
	{"BEQ",		0001400, INSN_BRANCH},
	{"BPL",		0100000, INSN_BRANCH},
	{"BMI",		0100400, INSN_BRANCH},
	{"BVC",		0102000, INSN_BRANCH},
	{"BVS",		0102400, INSN_BRANCH},
	{"BCC",		0103000, INSN_BRANCH},
	{"BCS",		0103400, INSN_BRANCH},
	{"BGE",		0020000, INSN_BRANCH},
	{"BLT",		0020400, INSN_BRANCH},
	{"BGT",		0030000, INSN_BRANCH},
	{"BLE",		0030400, INSN_BRANCH},
	{"BHI",		0101000, INSN_BRANCH},
	{"BLOS",	0101400, INSN_BRANCH},
	{"BHIS",	0103000, INSN_BRANCH},
	{"BLO",		0103400, INSN_BRANCH},
	{"EMT",		0104000, INSN_TRAP},
	{"TRAP",	0104400, INSN_TRAP},
};

#define	INSNCNT		sizeof(insns) / sizeof(*insns)

void ASAssemble(AS* as)
{
	int i;
	as->buf[as->bufp] = 0;
	for(i = 0; i < INSNCNT; i++) {
		const INSN* insn = &insns[i];
		if(MATCH(insn->name)) {
			switch(insn->type) {
				case INSN_NONE:
					WRITE0(insn->opcd);
					break;
				case INSN_SINGLE:
					WRITE1(insn->opcd);
					break;
				case INSN_DOUBLE:
					WRITE2(insn->opcd);
					break;
				case INSN_BRANCH:
					WRITEBR(insn->opcd);
					break;
				case INSN_JSR:
					WRITEJSR(insn->opcd);
					break;
				default:
					ASError(as, "Unknown instruction type");
					break;
			}
			return;
		}
	}
	ASError(as, "Unknown instruction");
}

void ASDirective(AS* as, const char* name, u16 arg)
{
	if(!strcmp(name, "ORG")) {
		as->pc = arg;
	} else {
		ASError(as, "unknown directive");
	}
}

void ASStep(AS* as)
{
	LABEL* lbl;

	const char c = as->source[as->rd++];

	if(c == 0) {
		as->state = STATE_END;
		return;
	}

	switch(as->state) {
		case STATE_BOL:
			switch(c) {
				case '\r':
				case '\n':
				case '\t':
				case ' ':
					break;
				case ';':
					as->state = STATE_COMMENT;
					break;
				case '.':
					as->state = STATE_DIRECTIVE;
					as->bufp = 0;
					break;
				default:
					if((c >= 'a' && c <= 'z')
						|| (c >= 'A' && c <= 'Z')
						|| (c >= '0' && c <= '9')
						|| (c == '_')) {
						as->state = STATE_LABEL;
						as->bufp = 0;
						as->buf[as->bufp++] = c;
					} else {
						ASError(as, "unexpected character");
					}
					break;
			}
			break;
		case STATE_LABEL:
			switch(c) {
				case '\r':
				case '\n':
					as->state = STATE_BOL;
					ASAssemble(as);
					break;
				case ';':
					as->state = STATE_COMMENT;
					ASAssemble(as);
					break;
				case ':': /* this is really a label */
					as->buf[as->bufp++] = 0;
					lbl = ASFindLabel(as, as->buf);
					if(!lbl) {
						lbl = (LABEL*) malloc(sizeof(LABEL));
						lbl->name = strdup(as->buf);
						lbl->next = as->labels;
						as->labels = lbl;
					}
					lbl->addr = as->pc;
					lbl->resolved = 1;
					ASFixup(as, lbl);
					as->state = STATE_SPACE;
					as->bufp = 0;
					break;
				case '\t':
				case ' ': /* this is an instruction */
					as->arg1 = NULL;
					as->arg2 = NULL;
					as->buf[as->bufp++] = 0;
					as->state = STATE_SEPARATOR;
					break;
				default:
					if((c >= 'a' && c <= 'z')
						|| (c >= 'A' && c <= 'Z')
						|| (c >= '0' && c <= '9')
						|| (c == '_')) {
						as->buf[as->bufp++] = c;
					} else {
						ASError(as, "unexpected character");
					}
					break;
			}
			break;
		case STATE_SPACE:
			switch(c) {
				case '\r':
				case '\n':
					as->state = STATE_BOL;
					break;
				case ';':
					as->state = STATE_COMMENT;
					break;
				case ' ':
				case '\t':
					break;
				default:
					if((c >= 'a' && c <= 'z')
						|| (c >= 'A' && c <= 'Z')
						|| (c == '_')) {
						as->buf[as->bufp++] = c;
						as->state = STATE_MNEMONIC;
					} else {
						ASError(as, "unexpected character");
					}
					break;
			}
			break;
		case STATE_MNEMONIC:
			switch(c) {
				case '\r':
				case '\n':
					as->state = STATE_BOL;
					ASAssemble(as);
					break;
				case ';':
					as->state = STATE_COMMENT;
					ASAssemble(as);
					break;
				case '\t':
				case ' ':
					as->buf[as->bufp++] = 0;
					as->arg1 = NULL;
					as->arg2 = NULL;
					as->state = STATE_SEPARATOR;
					break;
				default:
					if((c >= 'a' && c <= 'z')
						|| (c >= 'A' && c <= 'Z')
						|| (c == '_')) {
						as->buf[as->bufp++] = c;
						as->state = STATE_MNEMONIC;
					} else {
						ASError(as, "unexpected character");
					}
					break;
			}
			break;
		case STATE_SEPARATOR:
			switch(c) {
				case '\r':
				case '\n':
					as->state = STATE_BOL;
					ASAssemble(as);
					break;
				case ';':
					as->state = STATE_COMMENT;
					ASAssemble(as);
					break;
				case '\t':
				case ' ':
					break;
				default:
					as->arg1 = &as->buf[as->bufp];
					as->arg2 = NULL;
					as->state = STATE_ARG1;
					if((c >= 'a' && c <= 'z')
						|| (c >= 'A' && c <= 'Z')
						|| (c >= '0' && c <= '9')
						|| (c == '_')
						|| (c == '.')
						|| (c == '(')
						|| (c == ')')
						|| (c == '@')
						|| (c == '#')) {
						as->buf[as->bufp++] = c;
					} else {
						ASError(as, "unexpected character");
					}
					break;
			}
			break;
		case STATE_ARG1:
			switch(c) {
				case '\r':
				case '\n':
					as->state = STATE_BOL;
					ASAssemble(as);
					break;
				case ';':
					as->state = STATE_COMMENT;
					ASAssemble(as);
					break;
				case ',':
					as->buf[as->bufp++] = 0;
					as->state = STATE_SEP;
					break;
				default:
					if((c >= 'a' && c <= 'z')
						|| (c >= 'A' && c <= 'Z')
						|| (c >= '0' && c <= '9')
						|| (c == '_')
						|| (c == '.')
						|| (c == '(')
						|| (c == ')')
						|| (c == '@')
						|| (c == '#')) {
						as->buf[as->bufp++] = c;
					} else {
						ASError(as, "unexpected character");
					}
					break;
			}
			break;
		case STATE_SEP:
			switch(c) {
				case '\r':
				case '\n':
					ASError(as, "unexpected newline");
					break;
				case '\t':
				case ' ':
					break;
				default:
					if((c >= 'a' && c <= 'z')
						|| (c >= 'A' && c <= 'Z')
						|| (c >= '0' && c <= '9')
						|| (c == '_')
						|| (c == '.')
						|| (c == '(')
						|| (c == ')')
						|| (c == '@')
						|| (c == '#')) {
						as->arg2 = &as->buf[as->bufp];
						as->buf[as->bufp++] = c;
						as->state = STATE_ARG2;
					} else {
						ASError(as, "unexpected character");
					}
					break;
			}
			break;
		case STATE_ARG2:
			switch(c) {
				case '\r':
				case '\n':
					as->state = STATE_BOL;
					ASAssemble(as);
					break;
				case ';':
					as->state = STATE_COMMENT;
					ASAssemble(as);
					break;
				case ' ':
				case '\t':
					as->state = STATE_EOL;
					ASAssemble(as);
					break;
				default:
					if((c >= 'a' && c <= 'z')
						|| (c >= 'A' && c <= 'Z')
						|| (c >= '0' && c <= '9')
						|| (c == '_')
						|| (c == '.')
						|| (c == '(')
						|| (c == ')')
						|| (c == '@')
						|| (c == '#')) {
						as->buf[as->bufp++] = c;
					} else {
						ASError(as, "unexpected character");
					}
					break;
			}
			break;
		case STATE_COMMENT:
			switch(c) {
				case '\r':
				case '\n':
					as->state = STATE_BOL;
					break;
			}
			break;
		case STATE_DIRECTIVE:
			switch(c) {
				case '\r':
				case '\n':
					as->buf[as->bufp++] = 0;
					ASDirective(as, as->buf, 0);
					as->state = STATE_BOL;
					break;
				case ' ':
				case '\t':
					as->buf[as->bufp++] = 0;
					as->state = STATE_DIR_SEP;
					break;
				case ';':
					as->state = STATE_COMMENT;
					break;
				default:
					if(((c >= 'A') && (c <= 'Z'))
							|| ((c >= 'a') && (c <= 'z'))) {
						as->buf[as->bufp++] = c;
					} else {
						ASError(as, "invalid directive");
					}
					break;
			}
			break;
		case STATE_DIR_SEP:
			switch(c) {
				case '\r':
				case '\n':
					ASDirective(as, as->buf, 0);
					as->state = STATE_BOL;
					break;
				case ' ':
				case '\t':
					/* ignore */
					break;
				default:
					if((c >= '0') && (c <= '7')) {
						as->bufp = c - '0';
						as->state = STATE_DIR_ARG;
					} else {
						ASError(as, "invalid operand");
						break;
					}
					break;
			}
			break;
		case STATE_DIR_ARG:
			switch(c) {
				case '\r':
				case '\n':
					ASDirective(as, as->buf, as->bufp);
					as->state = STATE_BOL;
					break;
				case ' ':
				case '\t':
					ASDirective(as, as->buf, as->bufp);
					as->state = STATE_EOL;
					break;
				default:
					if((c >= '0') && (c <= '7')) {
						as->bufp <<= 3;
						as->bufp |= c - '0';
					} else {
						ASError(as, "invalid operand");
						break;
					}
					break;
			}
			break;
		case STATE_EOL:
			switch(c) {
				case '\r':
				case '\n':
					as->state = STATE_BOL;
					break;
				case ' ':
				case '\t':
					/* ignore */
					break;
				case ';':
					as->state = STATE_COMMENT;
					break;
				default:
					ASError(as, "invalid character");
					break;
			}
			break;
	}
}

void ASCompile(AS* as)
{
	while(as->state != STATE_END && as->state != STATE_ERROR) {
		ASStep(as);
	}

	if(as->fixups) {
		FIXUP* fix = as->fixups;
		ASError(as, "unresolved references");
		while(fix) {
			printf("unresolved reference: '%s' [%p]\n", fix->label->name, fix->label);
			fix = fix->next;
		}
	}
}
