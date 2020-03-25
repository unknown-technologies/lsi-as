#ifndef __AS_H__
#define __AS_H__

#include <stdint.h>

/* type definitions */
#define	u8		uint8_t
#define	s8		int8_t
#define	u16		uint16_t
#define	s16		int16_t
#define	u32		uint32_t
#define	s32		int32_t

#define	FIXUP_BR	0
#define	FIXUP_REL	1
#define	FIXUP_ABS	2

#define	INSN_NONE	0
#define	INSN_SINGLE	1
#define	INSN_DOUBLE	2
#define	INSN_BRANCH	3
#define	INSN_JSR	4
#define	INSN_RTS	5
#define	INSN_SOB	6
#define	INSN_TRAP	7
#define	INSN_EIS	8

/* data structures */
typedef struct {
	char*	name;
	u16	opcd;
	u8	type;
} INSN;

typedef struct label LABEL;
struct label {
	LABEL*		next;
	const char*	name;
	u16		addr;
	u16		resolved;
};

typedef struct fixup FIXUP;
struct fixup {
	FIXUP*		next;
	LABEL*		label;
	u16		addr;
	u16		pos;
	u16		type;
};

typedef struct {
	const char*	source;
	char*		buf;
	char*		mnemonic;
	char*		arg1;
	char*		arg2;
	LABEL*		labels;
	FIXUP*		fixups;
	u16*		code;
	u32		size;
	u32		rd;
	u16		wr;
	u16		bufp;
	u16		state;
	u16		pc;
	u16		org;
} AS;

void ASInit(AS* as);
void ASDestroy(AS* as);
void ASSetSource(AS* as, const char* source);
void ASCompile(AS* as);
LABEL* ASFindLabel(AS* as, const char* name);

#endif
