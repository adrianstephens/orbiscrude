#include "disassembler.h"

using namespace iso;

/*
mb (21–26) This field is used in rotate instructions to specify the first 1 bit of a 64-bit mask
me (21–26) This field is used in rotate instructions to specify the last 1 bit of a 64-bit mask
sh (16–20) & (30) These fields are used to specify a shift amount
spr (11–20) This field is used to specify a special-purpose register for the mtspr and mfspr instructions
tbr (11–20) This field is used to specify either the time base lower (TBL) or time base upper (TBU)

AA (30)	Absolute address bit.
	0 The immediate field represents an address relative to the current instruction address (CIA).
	1 The immediate field represents an absolute address. The effective address (EA) of the branch is the LI field sign-extended to 64 bits or the BD field sign-extended to 64 bits.
BD (16–29) Immediate field specifying a 14-bit signed two's complement branch displacement that is concatenated on the right with ‘00’ and sign-extended to 64 bits.
BI (11–15) This field is used to specify a bit in the CR to be used as the condition of a branch conditional instruction.
BO (6–10) This field is used to specify options for the branch conditional instructions.
crbA (11–15) This field is used to specify a bit in the CR to be used as a source.
crbB (16–20) This field is used to specify a bit in the CR to be used as a source.
crbD (6–10) This field is used to specify a bit in the CR, or in the FPSCR, as the destination of the result of an instruction.
crfD (6–8) This field is used to specify one of the CR fields, or one of the FPSCR fields, as a destination.
crfS (11–13) This field is used to specify one of the CR fields, or one of the FPSCR fields, as a source.
CRM (12–19) This field mask is used to identify the CR fields that are to be updated by the mtcrf instruction.
d (16–31) Immediate field specifying a 16-bit signed two's complement integer that is sign-extended to 64 bits.
ds (16–29) Immediate field specifying a 14-bit signed two’s complement integer which is concatenated on the right with 00
FM (7–14) This field mask is used to identify the FPSCR fields that are to be updated by the mtfsf instruction.
frA (11–15) This field is used to specify an FPR as a source.
frB (16–20) This field is used to specify an FPR as a source.
frC (21–25) This field is used to specify an FPR as a source.
frD (6–10) This field is used to specify an FPR as the destination.
frS (6–10) This field is used to specify an FPR as a source.
IMM (16–19) Immediate field used as the data to be placed into a field in the FPSCR.
L (9-10) Field used by the synchronize instruction. This field is defined in 64-bit implementations only.
L (10) Field used to specify whether an integer compare instruction is to compare 64-bit numbers or 32-bit numbers.
L (15) As above for  Move To Machine State Register instruction.
LI (6–29) Immediate field specifying a 24-bit signed two's complement integer that is concatenated on the right with ‘00’
LK (31)	Link bit.
	0 Does not update the link register (LR).
	1 Updates the LR. If the instruction is a branch instruction, the address of the instruction following the branch instruction is placed into the LR.
MB (21–25) and
ME (26–30) These fields are used in rotate instructions to specify a -bit mask consisting of ‘1’ bits from bit MB + 32 through bit ME + 32 inclusive, and ‘0’ bits elsewhere
MB (21–26) Field used in the MD-form and MDS-form instructions to specify the first ‘1’ bit of a 64-bit mask
ME (21–26) Field used in the MD-form and MDS-form instructions to specify the last ‘1’ bit of a 64-bit mask
NB (16–20) This field is used to specify the number of bytes to move in an immediate string load or store.
OE (21) This field is used for extended arithmetic to enable setting OV and SO in the XER.
OPCD (0–5) Primary opcode field.
rA (11–15) This field is used to specify a GPR to be used as a source or destination.
rB (16–20) This field is used to specify a GPR to be used as a source.
Rc (31) Record bit.
	0 Does not update the condition register (CR).
	1 Updates the CR to reflect the result of the operation.
rD (6–10) This field is used to specify a GPR to be used as a destination.
rS (6–10) This field is used to specify a GPR to be used as a source.
S (10) Field used by the tlbie instruction that is part of the optional large page facility.
SH (16–20, or 16–20 and 30) This field is used to specify a shift amount.
SIMM (16–31) This immediate field is used to specify a 16-bit signed integer.
SPR (11–20) Field used to specify a Special Purpose Register for the mtspr and mfspr instructions.
SR (12–15) This field is used to specify one of the 16 segment registers in 64-bit implementations that provide the optional mtsr and mfsr instructions.
TBR (11–20) Field used by the move from time base instruction.
TH (9–10) Field used by the optional data stream variant of the dcbt instruction.
TO (6–10) This field is used to specify the conditions on which to trap.
UIMM (16–31) This immediate field is used to specify a 16-bit unsigned integer.
XO (21–29, 21–30, 22–30, 26–30, 27–29, 27–30, or 30–31) Extended opcode field.
*/
enum {
	A		= 1 << 0,
	B		= 1 << 1,
	C		= 1 << 2,
	D		= 1 << 3,
	S		= 1 << 4,
	BO		= 1 << 5,

	FPR		= 1 << 6,
	X		= 1 << 7,

	crb		= 1 << 8,
	spr		= 1 << 9,
	crfD	= 1 << 10,
	SH		= 1 << 11,
	MBE		= 1 << 12,

	L		= 0,
	crfS	= 0,

	sh		= 0,
	mb		= 0,
	me		= 0,
	BH		= 0,
	SR		= 0,
	NB		= 0,
	FM		= 0,

	IMM		= 0,
	SIMM	= 1<<16,
	UIMM	= 1<<17,
	CRM		= 1<<18,

	TO		= 1<<25,
	ds		= 1<<26,
	d		= 1<<27,
	AA		= 1<<28,
	LK		= 1<<29,
	OE		= 1<<30,
	Rc		= 1<<31,
};
//FM(1), NB(2), BD(1)
static struct ppc_ops {
	const char *name;
	uint32	value, mask; int flags;
} ops[] = {
	{"tdi",			0x08000000, 0xfc000000, 	TO|A|SIMM		},
	{"twi",			0x0c000000, 0xfc000000, 	TO|A|SIMM		},
	{"mulli",		0x1c000000, 0xfc000000, 	D|A|SIMM		},
	{"subfic",		0x20000000, 0xfc000000, 	D|A|SIMM		},
	{"cmpli",		0x28000000, 0xfc400000, 	crfD|L|A|UIMM	},
	{"cmpi",		0x2c000000, 0xfc400000, 	crfD|L|A|SIMM	},
	{"addic",		0x30000000, 0xfc000000, 	D|A|SIMM		},
	{"addic.",		0x34000000, 0xfc000000, 	D|A|SIMM		},
	{"li",			0x38000000, 0xfc1f0000, 	D|SIMM			},
	{"lis",			0x3c000000, 0xfc1f0000, 	D|SIMM			},
	{"addi",		0x38000000, 0xfc000000, 	D|A|SIMM		},
	{"addis",		0x3c000000, 0xfc000000, 	D|A|SIMM		},
	{"b",			0x40000000, 0xfc000000, 	BO|AA|LK		},	//bc
	{"sc",			0x44000002, 0xffffffff, 	0				},
	{"b",			0x48000000, 0xfc000000, 	AA|LK			},
	{"mcrf",		0x4c000000, 0xfc63ffff, 	crfD|crfS		},
	{"b",			0x4c000020, 0xfc00e7fe, 	BO|BH|LK		},	//bclr
	{"rfid",		0x4c000024, 0xffffffff, 	0				},
	{"crnor",		0x4c000042, 0xfc0007ff, 	crb|D|A|B		},
	{"crandc",		0x4c000102, 0xfc0007ff, 	crb|D|A|B		},
	{"isync",		0x4c00012c, 0xffffffff, 	0				},
	{"crxor",		0x4c000182, 0xfc0007ff, 	crb|D|A|B		},
	{"crnand",		0x4c0001c2, 0xfc0007ff, 	crb|D|A|B		},
	{"crand",		0x4c000202, 0xfc0007ff, 	crb|D|A|B		},
	{"creqv",		0x4c000242, 0xfc0007ff, 	crb|D|A|B		},
	{"crorc",		0x4c000342, 0xfc0007ff, 	crb|D|A|B		},
	{"cror",		0x4c000382, 0xfc0007ff, 	crb|D|A|B		},
	{"b",			0x4c000420, 0xfc00e7fe, 	BO|BH|LK		},	//bcctr
	{"rlwimi",		0x50000000, 0xfc000000, 	S|A|SH|MBE|Rc	},
	{"rlwinm",		0x54000000, 0xfc000000, 	S|A|SH|MBE|Rc	},
	{"rlwnm",		0x5c000000, 0xfc000000, 	S|A|B |MBE|Rc	},
	{"ori",			0x60000000, 0xfc000000, 	S|A|UIMM		},
	{"oris",		0x64000000, 0xfc000000, 	S|A|UIMM		},
	{"xori",		0x68000000, 0xfc000000, 	S|A|UIMM		},
	{"xoris",		0x6c000000, 0xfc000000, 	S|A|UIMM		},
	{"andi.",		0x70000000, 0xfc000000, 	S|A|UIMM		},
	{"andis.",		0x74000000, 0xfc000000, 	S|A|UIMM		},
	{"rldicl",		0x78000000, 0xfc00001c, 	S|A|sh|mb|sh|Rc	},
	{"rldicr",		0x78000004, 0xfc00001c, 	S|A|sh|me|sh|Rc	},
	{"rldic",		0x78000008, 0xfc00001c, 	S|A|sh|mb|sh|Rc	},
	{"rldimi",		0x7800000c, 0xfc00001c, 	S|A|sh|mb|sh|Rc	},
	{"rldcl",		0x78000010, 0xfc00003e, 	S|A|B|mb|Rc		},
	{"rldcr",		0x78000012, 0xfc00003e, 	S|A|B|me|Rc		},
	{"cmp",			0x7c000000, 0xfc4007ff, 	crfD|L|A|B		},
	{"tw",			0x7c000008, 0xfc0007ff, 	TO|A|B			},
	{"subfc",		0x7c000010, 0xfc0003fe, 	D|A|B|OE|Rc		},
	{"mulhdu",		0x7c000012, 0xfc0007fe, 	D|A|B|Rc		},
	{"addc",		0x7c000014, 0xfc0003fe, 	D|A|B|OE|Rc		},
	{"mulhwu",		0x7c000016, 0xfc0007fe, 	D|A|B|Rc		},
	{"mfcr",		0x7c000026, 0xfc1fffff, 	D				},
	{"lwarx",		0x7c000028, 0xfc0007ff, 	D|X|B			},
	{"ldx",			0x7c00002a, 0xfc0007ff, 	D|X|B			},
	{"lwzx",		0x7c00002e, 0xfc0007ff, 	D|X|B			},
	{"slw",			0x7c000030, 0xfc0007fe, 	S|A|B|Rc		},
	{"cntlzw",		0x7c000034, 0xfc00fffe, 	S|A|Rc			},
	{"sld",			0x7c000036, 0xfc0007fe, 	S|A|B|Rc		},
	{"and",			0x7c000038, 0xfc0007fe, 	S|A|B|Rc		},
	{"cmpl",		0x7c000040, 0xfc4007ff, 	crfD|L|A|B		},
	{"subf",		0x7c000050, 0xfc0003fe, 	D|A|B|OE|Rc		},
	{"ldux",		0x7c00006a, 0xfc0007ff, 	D|X|B			},
	{"dcbst",		0x7c00006c, 0xffe007ff, 	A|B				},
	{"lwzux",		0x7c00006e, 0xfc0007ff, 	D|X|B			},
	{"cntlzd",		0x7c000074, 0xfc00fffe, 	S|A|Rc			},
	{"andc",		0x7c000078, 0xfc0007fe, 	S|A|B|Rc		},
	{"td",			0x7c000088, 0xfc0007ff, 	TO|A|B			},
	{"mulhd",		0x7c000092, 0xfc0007fe, 	D|A|B|Rc		},
	{"mulhw",		0x7c000096, 0xfc0007fe, 	D|A|B|Rc		},
	{"mfmsr",		0x7c0000a6, 0xfc1fffff, 	D				},
	{"ldarx",		0x7c0000a8, 0xfc0007ff, 	D|X|B			},
	{"dcbf",		0x7c0000ac, 0xffe007ff, 	A|B				},
	{"lbzx",		0x7c0000ae, 0xfc0007ff, 	D|X|B			},
	{"neg",			0x7c0000d0, 0xfc00fbfe, 	D|A|OE|Rc		},
	{"lbzux",		0x7c0000ee, 0xfc0007ff, 	D|X|B			},
	{"nor",			0x7c0000f8, 0xfc0007fe, 	S|A|B|Rc		},
	{"subfe",		0x7c000110, 0xfc0003fe, 	D|A|B|OE|Rc		},
	{"adde",		0x7c000114, 0xfc0003fe, 	D|A|B|OE|Rc		},
	{"mtcrf",		0x7c000120, 0xfc100fff, 	S|CRM			},
	{"mtmsr",		0x7c000124, 0xfc1effff, 	S|L				},
	{"stdx",		0x7c00012a, 0xfc0007ff, 	S|A|B			},
	{"stwcx.",		0x7c00012d, 0xfc0007ff, 	S|X|B			},
	{"stwx",		0x7c00012e, 0xfc0007ff, 	S|X|B			},
	{"mtmsrd",		0x7c000164, 0xfc1effff, 	S|L				},
	{"stdux",		0x7c00016a, 0xfc0007ff, 	S|X|B			},
	{"stwux",		0x7c00016e, 0xfc0007ff, 	S|X|B			},
	{"subfze",		0x7c000190, 0xfc00fbfe, 	D|A|OE|Rc		},
	{"addze",		0x7c000194, 0xfc00fbfe, 	D|A|OE|Rc		},
	{"mtsr",		0x7c0001a4, 0xfc10ffff, 	S|SR			},
	{"stdcx.",		0x7c0001ad, 0xfc0007ff, 	S|X|B			},
	{"stbx",		0x7c0001ae, 0xfc0007ff, 	S|X|B			},
	{"subfme",		0x7c0001d0, 0xfc00fbfe, 	D|A|OE|Rc		},
	{"mulld",		0x7c0001d2, 0xfc0003fe, 	D|A|B|OE|Rc		},
	{"addme",		0x7c0001d4, 0xfc00fbfe, 	D|A|OE|Rc		},
	{"mullw",		0x7c0001d6, 0xfc0003fe, 	D|A|B|OE|Rc		},
	{"mtsrin",		0x7c0001e4, 0xfc1f07ff, 	S|B				},
	{"mtocrf",		0x7c100120, 0xfc100fff, 	S|CRM			},
	{"dcbtst",		0x7c0001ec, 0xffe007ff, 	A|B				},
	{"stbux",		0x7c0001ee, 0xfc0007ff, 	S|A|B			},
	{"add",			0x7c000214, 0xfc0003fe, 	D|A|B|OE|Rc		},
	{"dcbt",		0x7c00022c, 0xffe007ff, 	A|B				},
	{"lhzx",		0x7c00022e, 0xfc0007ff, 	D|X|B			},
	{"eqv",			0x7c000238, 0xfc0007fe, 	S|A|B|Rc		},
	{"tlbiel",		0x7c000224, 0xffdf07ff, 	L|B				},
	{"tlbie",		0x7c000264, 0xffdf07ff, 	L|B				},
	{"eciwx",		0x7c00026c, 0xfc0007ff, 	D|X|B			},
	{"lhzux",		0x7c00026e, 0xfc0007ff, 	D|X|B			},
	{"xor",			0x7c000278, 0xfc0007fe, 	S|A|B|Rc		},
	{"mfspr",		0x7c0002a6, 0xfc0007ff, 	D|spr			},
	{"lwax",		0x7c0002aa, 0xfc0007ff, 	D|X|B			},
	{"lhax",		0x7c0002ae, 0xfc0007ff, 	D|X|B			},
	{"tlbia",		0x7c0002e4, 0xffffffff, 	0				},
	{"mftb",		0x7c0002e6, 0xfc0007ff, 	D|spr			},
	{"lwaux",		0x7c0002ea, 0xfc0007ff, 	D|X|B			},
	{"lhaux",		0x7c0002ee, 0xfc0007ff, 	D|X|B			},
	{"sthx",		0x7c00032e, 0xfc0007ff, 	S|X|B			},
	{"orc",			0x7c000338, 0xfc0007fe, 	S|A|B|Rc		},
	{"sradi",		0x7c000674, 0xfc0007fc, 	S|A|sh|sh|Rc	},
	{"slbie",		0x7c000364, 0xffff07ff, 	B				},
	{"ecowx",		0x7c00036c, 0xfc0007ff, 	S|X|B			},
	{"sthux",		0x7c00036e, 0xfc0007ff, 	S|X|B			},
	{"or",			0x7c000378, 0xfc0007fe, 	S|A|B|Rc		},
	{"divdu",		0x7c000392, 0xfc0003fe, 	D|A|B|OE|Rc		},
	{"divwu",		0x7c000396, 0xfc0003fe, 	D|A|B|OE|Rc		},
	{"mtspr",		0x7c0003a6, 0xfc0007ff, 	S|spr			},
	{"nand",		0x7c0003b8, 0xfc0007fe, 	S|A|B|Rc		},
	{"divd",		0x7c0003d2, 0xfc0003fe, 	D|A|B|OE|Rc		},
	{"divw",		0x7c0003d6, 0xfc0003fe, 	D|A|B|OE|Rc		},
	{"slbia",		0x7c0003e4, 0xffffffff, 	0				},
	{"lswx",		0x7c00042a, 0xfc0007ff, 	D|X|B			},
	{"lwbrx",		0x7c00042c, 0xfc0007ff, 	D|X|B			},
	{"lfsx",		0x7c00042e, 0xfc0007ff, 	FPR|D|X|B		},
	{"srw",			0x7c000430, 0xfc0007fe, 	S|A|B|Rc		},
	{"srd",			0x7c000436, 0xfc0007fe, 	S|A|B|Rc		},
	{"tlbsync",		0x7c00046c, 0xffffffff, 	0				},
	{"lfsux",		0x7c00046e, 0xfc0007ff, 	FPR|D|X|B		},
	{"mfsr",		0x7c0004a6, 0xfc10ffff, 	D|SR			},
	{"lswi",		0x7c0004aa, 0xfc0007ff, 	D|A|NB			},
	{"sync",		0x7c0004ac, 0xff9fffff, 	L				},
	{"lfdx",		0x7c0004ae, 0xfc0007ff, 	FPR|D|A|B		},
	{"lfdux",		0x7c0004ee, 0xfc0007ff, 	FPR|D|X|B		},
	{"mfsrin",		0x7c000526, 0xfc1f07ff, 	D|B				},
	{"slbmfee",		0x7c000726, 0xfc1f07ff, 	D|B				},
	{"slbmfev",		0x7c0006a6, 0xfc1f07ff, 	D|B				},
	{"slbmte",		0x7c000324, 0xfc1f07ff, 	D|B				},
	{"mfocrf",		0x7c100026, 0xfc100fff, 	D|CRM			},
	{"stswx",		0x7c00052a, 0xfc0007ff, 	S|A|B			},
	{"stwbrx",		0x7c00052c, 0xfc0007ff, 	S|A|B			},
	{"stfsx",		0x7c00052e, 0xfc0007ff, 	FPR|S|X|B		},
	{"stfsux",		0x7c00056e, 0xfc0007ff, 	FPR|S|X|B		},
	{"stswi",		0x7c0005aa, 0xfc0007ff, 	S|A|NB			},
	{"stfdx",		0x7c0005ae, 0xfc0007ff, 	FPR|S|X|B		},
	{"stfdux",		0x7c0005ee, 0xfc0007ff, 	FPR|S|X|B		},
	{"lhbrx",		0x7c00062c, 0xfc0007ff, 	D|X|B			},
	{"sraw",		0x7c000630, 0xfc0007fe, 	S|A|B|Rc		},
	{"srad",		0x7c000634, 0xfc0007fe, 	S|A|B|Rc		},
	{"srawi",		0x7c000670, 0xfc0007fe, 	S|A|SH|Rc		},
	{"eieio",		0x7c0006ac, 0xffffffff, 	0				},
	{"sthbrx",		0x7c00072c, 0xfc0007ff, 	S|X|B			},
	{"extsh",		0x7c000734, 0xfc00fffe, 	S|A|Rc			},
	{"extsb",		0x7c000774, 0xfc00fffe, 	S|A|Rc			},
	{"icbi",		0x7c0007ac, 0xffe007ff, 	A|B				},
	{"stfiwx",		0x7c0007ae, 0xfc0007ff, 	S|X|B			},
	{"extsw",		0x7c0007b4, 0xfc00fffe, 	S|A|Rc			},
	{"dcbz",		0x7c0007ec, 0xffe007ff, 	A|B				},
	{"lwz",			0x80000000, 0xfc000000, 	D|d				},
	{"lwzu",		0x84000000, 0xfc000000, 	D|d				},
	{"lbz",			0x88000000, 0xfc000000, 	D|d				},
	{"lbzu",		0x8c000000, 0xfc000000, 	D|d				},
	{"stw",			0x90000000, 0xfc000000, 	D|d				},
	{"stwu",		0x94000000, 0xfc000000, 	D|d				},
	{"stb",			0x98000000, 0xfc000000, 	D|d				},
	{"stbu",		0x9c000000, 0xfc000000, 	D|d				},
	{"lhz",			0xa0000000, 0xfc000000, 	D|d				},
	{"lhzu",		0xa4000000, 0xfc000000, 	D|d				},
	{"lha",			0xa8000000, 0xfc000000, 	D|d				},
	{"lhau",		0xac000000, 0xfc000000, 	D|d				},
	{"sth",			0xb0000000, 0xfc000000, 	D|d				},
	{"sthu",		0xb4000000, 0xfc000000, 	D|d				},
	{"lmw",			0xb8000000, 0xfc000000, 	D|d				},
	{"stmw",		0xbc000000, 0xfc000000, 	D|d				},
	{"lfs",			0xc0000000, 0xfc000000, 	FPR|D|d			},
	{"lfsu",		0xc4000000, 0xfc000000, 	FPR|D|d			},
	{"lfd",			0xc8000000, 0xfc000000, 	FPR|D|d			},
	{"lfdu",		0xcc000000, 0xfc000000, 	FPR|D|d			},
	{"stfs",		0xd0000000, 0xfc000000, 	FPR|D|d			},
	{"stfsu",		0xd4000000, 0xfc000000,		FPR|D|d			},
	{"stfd",		0xd8000000, 0xfc000000, 	FPR|D|d			},
	{"stfdu",		0xdc000000, 0xfc000000, 	FPR|D|d			},
	{"ld",			0xe8000000, 0xfc000003, 	D|ds			},
	{"ldu",			0xe8000001, 0xfc000003, 	D|ds			},
	{"lwa",			0xe8000002, 0xfc000003, 	D|ds			},
	{"fdivs",		0xec000024, 0xfc0007fe, 	FPR|D|A|B|Rc	},
	{"fsubs",		0xec000028, 0xfc0007fe, 	FPR|D|A|B|Rc	},
	{"fadds",		0xec00002a, 0xfc0007fe, 	FPR|D|A|B|Rc	},
	{"fsqrts",		0xec00002c, 0xfc1f07fe, 	FPR|D|B|Rc		},
	{"fres",		0xec000030, 0xfc1f07fe, 	FPR|D|B|Rc		},
	{"fmuls",		0xec000032, 0xfc00f83e, 	FPR|D|A|C|Rc	},
	{"fmsubs",		0xec000038, 0xfc00003e, 	FPR|D|A|B|C|Rc	},
	{"fmadds",		0xec00003a, 0xfc00003e, 	FPR|D|A|B|C|Rc	},
	{"fnmsubs",		0xec00003c, 0xfc00003e, 	FPR|D|A|B|C|Rc	},
	{"fnmadds",		0xec00003e, 0xfc00003e, 	FPR|D|A|B|C|Rc	},
	{"std",			0xf8000000, 0xfc000003, 	D|ds			},
	{"stdu",		0xf8000001, 0xfc000003, 	D|ds			},
	{"fcmpu",		0xfc000000, 0xfc6007ff, 	FPR|crfD|A|B	},
	{"frsp",		0xfc000018, 0xfc1f07fe, 	FPR|D|B|Rc		},
	{"fctiw",		0xfc00001c, 0xfc1f07fe, 	FPR|D|B			},
	{"fctiwz",		0xfc00001e, 0xfc1f07fe, 	FPR|D|B|Rc		},
	{"fdiv",		0xfc000024, 0xfc0007fe, 	FPR|D|A|B|Rc	},
	{"fsub",		0xfc000028, 0xfc0007fe, 	FPR|D|A|B|Rc	},
	{"fadd",		0xfc00002a, 0xfc0007fe, 	FPR|D|A|B|Rc	},
	{"fsqrt",		0xfc00002c, 0xfc1f07fe, 	FPR|D|B|Rc		},
	{"fsel",		0xfc00002e, 0xfc00003e, 	FPR|D|A|B|C|Rc	},
	{"fmul",		0xfc000032, 0xfc00f83e, 	FPR|D|A|C|Rc	},
	{"frsqrte",		0xfc000034, 0xfc1f07fe, 	FPR|D|B|Rc		},
	{"fmsub",		0xfc000038, 0xfc00003e, 	FPR|D|A|B|C|Rc	},
	{"fmadd",		0xfc00003a, 0xfc00003e, 	FPR|D|A|B|C|Rc	},
	{"fnmsub",		0xfc00003c, 0xfc00003e, 	FPR|D|A|B|C|Rc	},
	{"fnmadd",		0xfc00003e, 0xfc00003e, 	FPR|D|A|B|C|Rc	},
	{"fcmpo",		0xfc000040, 0xfc6007ff, 	FPR|crfD|A|B	},
	{"mtfsb1",		0xfc00004c, 0xfc1ffffe, 	crb|D|Rc		},
	{"fneg",		0xfc000050, 0xfc1f07fe, 	FPR|D|B|Rc		},
	{"mcrfs",		0xfc000080, 0xfc63ffff, 	crfD|crfS		},
	{"mtfsb0",		0xfc00008c, 0xfc1ffffe, 	crb|D|Rc		},
	{"fmr",			0xfc000090, 0xfc1f07fe, 	FPR|D|B|Rc		},
	{"mtfsfi",		0xfc00010c, 0xfc7f0ffe, 	crfD|IMM|Rc		},
	{"fnabs",		0xfc000110, 0xfc1f07fe, 	FPR|D|B|Rc		},
	{"fabs",		0xfc000210, 0xfc1f07fe, 	FPR|D|B|Rc		},
	{"mffs",		0xfc00048e, 0xfc1ffffe, 	D|Rc			},
	{"mtfsf",		0xfc00058e, 0xfe0107fe, 	FM|B|Rc			},
	{"fctid",		0xfc00065c, 0xfc1f07fe, 	FPR|D|B|Rc		},
	{"fctidz",		0xfc00065e, 0xfc1f07fe, 	FPR|D|B|Rc		},
	{"fcfid",		0xfc00069c, 0xfc1f07fe, 	FPR|D|B|Rc		},

	{"psq_l",		0xe0000000, 0xfc000000,		FPR|D|d			},
	{"psq_lu",		0xe4000000, 0xfc000000,		FPR|D|d			},
	{"psq_st",		0xf0000000, 0xfc000000,		FPR|D|ds		},
	{"psq_stu",		0xf4000000, 0xfc000000,		FPR|D|ds		},

	{"psq_lux",		0x1000004c, 0xfc00007e,		FPR|D|X|B		},
	{"psq_stux",	0x1000004e, 0xfc00007e,		FPR|S|X|B		},
	{"psq_stx",		0x1000000e, 0xfc00007e,		FPR|S|X|B		},
	{"psq_lx",		0x1000000c, 0xfc00007e,		FPR|D|X|B		},

	{"ps_abs",		0x10000210, 0xfc1f07fe,		FPR|D|B|Rc		},
	{"ps_add",		0x1000002a, 0xfc0007fe,		FPR|D|A|B|Rc	},
	{"ps_cmpo0",	0x10000040, 0xfc0007ff,		FPR|crfD|A|B|Rc	},
	{"ps_cmpo1",	0x100000c0, 0xfc0007ff,		FPR|crfD|A|B|Rc	},
	{"ps_cmpu0",	0x10000000, 0xfc0007ff,		FPR|crfD|A|B|Rc	},
	{"ps_cmpu1",	0x10000080, 0xfc0007ff,		FPR|crfD|A|B|Rc	},
	{"ps_div",		0x10000024, 0xfc0007fe,		FPR|D|A|B|Rc	},
	{"ps_madd",		0x1000003a, 0xfc00003e,		FPR|D|A|B|C|Rc	},
	{"ps_madds0",	0x1000001c, 0xfc00003e,		FPR|D|A|B|C|Rc	},
	{"ps_madds1",	0x1000001e, 0xfc00003e,		FPR|D|A|B|C|Rc	},
	{"ps_merge00",	0x10000420, 0xfc0007fe,		FPR|D|A|B|Rc	},
	{"ps_merge01",	0x10000460, 0xfc0007fe,		FPR|D|A|B|Rc	},
	{"ps_merge10",	0x100004a0, 0xfc0007fe,		FPR|D|A|B|Rc	},
	{"ps_merge11",	0x100004e0, 0xfc0007fe,		FPR|D|A|B|Rc	},
	{"ps_mr",		0x10000090, 0xfc1f07fe,		FPR|D|B|Rc		},
	{"ps_msub",		0x10000038, 0xfc00003e,		FPR|D|A|B|C|Rc	},
	{"ps_mul",		0x10000032, 0xfc00f83e,		FPR|D|A|C|Rc	},
	{"ps_muls0",	0x10000018, 0xfc00f83e,		FPR|D|A|C|Rc	},
	{"ps_muls1",	0x1000001a, 0xfc00f83e,		FPR|D|A|C|Rc	},
	{"ps_nabs",		0x10000110, 0xfc1f07fe,		FPR|D|B|Rc		},
	{"ps_neg",		0x10000050, 0xfc1f07fe,		FPR|D|B|Rc		},
	{"ps_nmadd",	0x1000003e, 0xfc00003e,		FPR|D|A|B|C|Rc	},
	{"ps_nmsub",	0x1000003c, 0xfc00003e,		FPR|D|A|B|C|Rc	},
	{"ps_res",		0x10000030, 0xfc1f07fe,		FPR|D|B|Rc		},
	{"ps_rsqrte",	0x10000034, 0xfc1f07fe,		FPR|D|B|Rc		},
	{"ps_sel",		0x1000002e, 0xfc00003e,		FPR|D|A|B|C|Rc	},
	{"ps_sub",		0x10000028, 0xfc0007fe,		FPR|D|A|B|Rc	},
	{"ps_sum0",		0x10000014, 0xfc00003e,		FPR|D|A|B|C|Rc	},
	{"ps_sum1",		0x10000016, 0xfc00003e,		FPR|D|A|B|C|Rc	},

};

void get_greg(string_accum &a, uint32 r) {
	a << 'r' << r;
}

void get_reg(string_accum &a, uint32 r, uint32 flags) {
	if (flags & FPR) {
		a << "fp";
	} else {
		if (flags & crb)
			a << 'c';
		a << 'r';
	}
	a << r;
}

void get_spr(string_accum &a, uint32 r) {
	switch (r) {
		case 1:	a << "xer"; break;
		case 8:	a << "lr"; break;
		case 9:	a << "ctr"; break;
		default:a << "spr" << r; break;
	}
}

class DisassemblerPPC : public Disassembler {
	string_accum&	put_address(string_accum &sa, uint32 address, SymbolFinder sym_finder) {
		uint64			sym_addr;
		string_param	sym_name;
		if (sym_finder && sym_finder(address, sym_addr, sym_name)) {
			sa << sym_name;
			return sym_addr != address ? sa << "+" << address - sym_addr : sa;
		}
		return sa.format("0x%08x", address);
	}

public:
	virtual	const char*	GetDescription()	{ return "Power PC"; }
	virtual State*		Disassemble(const iso::memory_block &block, uint64 addr, SymbolFinder sym_finder);
} ppc;

Disassembler::State *DisassemblerPPC::Disassemble(const iso::memory_block &block, uint64 addr, SymbolFinder sym_finder) {
	StateDefault	*state = new StateDefault;

	for (uint32be *p = block, *end = (uint32be*)block.end(); p < end; p++) {
		uint32	offset		= (uint8*)p - (uint8*)block;
		uint32	op			= *p;

		uint64			sym_addr;
		string_param	sym_name;
		if (sym_finder && sym_finder(offset, sym_addr, sym_name) && sym_addr == offset)
			state->lines.push_back(sym_name);

		buffer_accum<1024>	ba("%08x    %08x    ", offset, op);

		int	i;
		for (i = 0; i < num_elements(ops); i++) {
			if ((op & ops[i].mask) == ops[i].value)
				break;
		}
		if (i < num_elements(ops)) {
			uint32	flags = ops[i].flags;

			ba << ops[i].name;

			if (flags & BO) {
				uint8	bo		= (op>>21)&31;
				if (!(bo & 4)) {
					ba << 'd';
					if (!(bo & 2))
						ba << 'n';
					ba << 'z';
				}
				if (!(bo & 16)) {
					static const char *conds[] = {
						"ge", "le", "ne", "nu",
						"lt", "gt", "eq", "un",
					};
					ba << conds[((op >> 16) & 3) | ((bo & 8) >> 1)];
				}
				if (ops[i].value == 0x4c000020)
					ba << "lr";
				else if (ops[i].value == 0x4c000420)
					ba << "ctr";
			}

			if ((flags & OE) && (op & (1<<10)))
				ba << 'o';
			if ((flags & Rc) && (op & 1))
				ba << '.';
			if ((flags & LK) && (op & 1))
				ba << 'l';
			if ((flags & AA) && (op & 2))
				ba << 'a';

			if ((flags & BO) && (op & (1<<21)))
				ba << (!(flags & AA) || !(op & (1<<15)) ? '+' : '-');

			ba << ' ';

			if ((flags & crfD) && (op & (7<<23))) {
				ba << "cr" << ((op >> 23) & 7) << ',';
			}
			if ((flags & BO) && (op & (7<<18))) {
				ba << "cr" << ((op >> 18) & 7) << ',';	// BI (check)
			}

			if (flags & TO) {
				ba << ((op >> 21) & 31) << ',';
			}
			if (flags & D) {
				get_reg(ba, (op >> 21) & 31, flags);
				ba << ',';
			}
			if (flags & A) {
				get_reg(ba, (op >> 16) & 31, flags);
				ba << ',';
			}
			if (flags & spr) {
				get_spr(ba, ((op >> 16) & 31) | (((op >> 11) & 31) << 5));
				if (flags & S)
					ba << ',';
			}
			if (flags & CRM) {
				ba << ((op >> 12) &0xff) << ',';
			}
			if (flags & S) {
				get_reg(ba, (op >> 21) & 31, flags);
				ba << ',';
			}
			if (flags & X) {
				if (int r = (op >> 16) & 31) {
					get_greg(ba, r);
					ba << ',';
				}
				get_greg(ba, (op >> 11) & 31);
			} else if (flags & B) {
				get_reg(ba, (op >> 11) & 31, flags);
			}

			if (flags & (d|ds)) {
				ba << int16(op & (flags & ds ? 0xfffc : 0xffff)) << '(';
				get_greg(ba, (op >> 16) & 31);
				ba << ')';
			}

			if (flags & SH)
				ba << ((op >> 16) & 31) << ',';
			if (flags & MBE)
				ba << ((op >> 6) & 31) << ',' << ((op >> 1) & 31);

			if (flags & SIMM)
				ba << int16(op & 0xffff);

			if (flags & UIMM)
				ba << (op & 0xffff);

			if (flags & AA) {
				uint32	target = flags & BO ? sign_extend(op & 0xfffc, 16) : sign_extend(op & 0x03fffffc, 26);
				if (!(op & 2))
					target += offset;
				put_address(ba, target, sym_finder);
			}

		} else {
			ba.format("unknown opcode 0x%08x", op);
		}
		state->lines.push_back(ba);
	}
	return state;
}

#if 0
tdi			000010	TO	A	SIMM
twi			000011	TO	A	SIMM
mulli		000111	D	A	SIMM
subfic		001000	D	A	SIMM
cmpli		001010	crfD	0	L	A	UIMM
cmpi		001011	crfD	0	L	A	SIMM
addic		001100	D	A	SIMM
addic.		001101	D	A	SIMM
addi		001110	D	A	SIMM
addis		001111	D	A	SIMM
bcx			010000	BO	BI	BD	AA	LK
sc			010001000000000000000000000000010
bx			010010	LI	AA	LK
mcrf		010011	crfD	00	crfS	000000000000000000
bclrx		010011	BO	BI	000	BH	0000010000 L
rfid		01001100000000000000000000100100
crnor		010011	crbD	crbA	crbB	00001000010
crandc		010011	crbD	crbA	crbB	00100000010
isync		01001100000000000000000100101100
crxor		010011	crbD	crbA	crbB	00110000010
crnand		010011	crbD	crbA	crbB	00111000010
crand		010011	crbD	crbA	crbB	01000000010
creqv		010011	crbD	crbA	crbB	01001000010
crorc		010011	crbD	crbA	crbB	01101000010
cror		010011	crbD	crbA	crbB	01110000010
bcctrx		010011	BO	BI	000	BH	1000010000	LK
rlwimix		010100	S	A	SH	MB	ME	Rc
rlwinmx		010101	S	A	SH	MB	ME	Rc
rlwnmx		010111	S	A	B	MB	ME	Rc
ori			011000	S	A	UIMM
oris		011001	S	A	UIMM
xori		011010	S	A	UIMM
xoris		011011	S	A	UIMM
andi.		011100	S	A	UIMM
andis.		011101	S	A	UIMM
rldiclx		011110	S	A	sh	mb	000	sh	Rc
rldicrx		011110	S	A	sh	me	001	sh	Rc
rldicx		011110	S	A	sh	mb	010	sh	Rc
rldimix		011110	S	A	sh	mb	011	sh	Rc
rldclx		011110	S	A	B	mb	01000	Rc
rldcrx		011110	S	A	B	me	01001	Rc
cmp			011111	crfD	0	L	A	B	00000000000
tw			011111	TO	A	B	00000001000
subfcx		011111	D	A	B	OE	000001000	Rc
mulhdux		011111	D	A	B	0000001001	Rc
addcx		011111	D	A	B	OE	000001010	Rc
mulhwux		011111	D	A	B	0000001011	Rc
mfcr		011111	D	000000000000000100110
lwarx		011111	D	A	B	00000101000
ldx			011111	D	A	B	00000101010
lwzx		011111	D	A	B	00000101110
slwx		011111	S	A	B	0000011000	Rc
cntlzwx		011111	S	A	000000000011010	Rc
sldx		011111	S	A	B	0000011011	Rc
andx		011111	S	A	B	0000011100	Rc
cmpl		011111	crfD	0	L	A	B	00001000000
subfx		011111	D	A	B	OE	000101000	Rc
ldux		011111	D	A	B	00001101010
dcbst		01111100000	A	B	00001101100
lwzux		011111	D	A	B	00001101110
cntlzdx		011111	S	A	000000000111010	Rc
andcx		011111	S	A	B	0000111100	Rc
td			011111	TO	A	B	00010001000
mulhdx		011111	D	A	B	0001001001	Rc
mulhwx		011111	D	A	B	0001001011	Rc
mfmsr		011111	D	000000000000010100110
ldarx		011111	D	A	B	00010101000
dcbf		01111100000	A	B	00010101100
lbzx		011111	D	A	B	00010101110
negx		011111	D	A	00000	OE	001101000	Rc
lbzux		011111	D	A	B	00011101110
norx		011111	S	A	B	0001111100	Rc
subfex		011111	D	A	B	OE	010001000	Rc
addex		011111	D	A	B	OE	010001010	Rc
mtcrf		011111	S	0	CRM	000100100000
mtmsr		011111	S	0000	L	0000000100100100
stdx		011111	S	A	B	00100101010
stwcx.		011111	S	A	B	00100101101
stwx		011111	S	A	B	00100101110
mtmsrd		011111	S	0000	L	0000000101100100
stdux		011111	S	A	B	00101101010
stwux		011111	S	A	B	00101101110
subfzex		011111	D	A	00000	OE	011001000	Rc
addzex		011111	D	A	00000	OE	011001010	Rc
mtsr		011111	S	0	SR	0000000110100100
stdcx.		011111	S	A	B	00110101101
stbx		011111	S	A	B	00110101110
subfmex		011111	D	A	00000	OE	011101000	Rc
mulldx		011111	D	A	B	OE	011101001	Rc
addmex		011111	D	A	00000	OE	011101010	Rc
mullwx		011111	D	A	B	OE	011101011	Rc
mtsrin		011111	S	00000	B	00111100100
mtocrf		011111	S	1	CRM	000100100000
dcbtst		01111100000	A	B	00111101100
stbux		011111	S	A	B	00111101110
addx		011111	D	A	B	OE	100001010	Rc
dcbt		01111100000	A	B	01000101100
lhzx		011111	D	A	B	01000101110
eqvx		011111	S	A	B	0100011100	Rc
tlbiel		0111110000	L	00000	B	01000100100
tlbie		0111110000	L	00000	B	01001100100
eciwx		011111	D	A	B	01001101100
lhzux		011111	D	A	B	01001101110
xorx		011111	S	A	B	0100111100	Rc
mfspr		011111	D	spr	01010100110
lwax		011111	D	A	B	01010101010
lhax		011111	D	A	B	01010101110
tlbia		01111100000000000000001011100100
mftb		011111	D	tbr	01011100110
lwaux		011111	D	A	B	01011101010
lhaux		011111	D	A	B	01011101110
sthx		011111	S	A	B	01100101110
orcx		011111	S	A	B	0110011100	Rc
sradix		011111	S	A	sh	110011101	sh	Rc
slbie		0111110000000000	B	01101100100
ecowx		011111	S	A	B	01101101100
sthux		011111	S	A	B	01101101110
orx			011111	S	A	B	0110111100	Rc
divdux		011111	D	A	B	OE	111001001	Rc
divwux		011111	D	A	B	OE	111001011	Rc
mtspr		011111	S	spr	01110100110
nandx		011111	S	A	B	0111011100	Rc
divdx		011111	D	A	B	OE	111101001	Rc
divwx		011111	D	A	B	OE	111101011	Rc
slbia		01111100000000000000001111100100
lswx		011111	D	A	B	10000101010
lwbrx		011111	D	A	B	10000101100
lfsx		011111	D	A	B	10000101110
srwx		011111	S	A	B	1000011000	Rc
srdx		011111	S	A	B	1000011011	Rc
tlbsync		01111100000000000000010001101100
lfsux		011111	D	A	B	10001101110
mfsr		011111	D	0	SR	0000010010100110
lswi		011111	D	A	NB	10010101010
sync		011111000	L	000000000010010101100
lfdx		011111	D	A	B	10010101110
lfdux		011111	D	A	B	10011101110
mfsrin		011111	D	00000	B	10100100110
slbmfee		011111	D	00000	B	11100100110
slbmfev		011111	D	00000	B	11010100110
slbmte		011111	D	00000	B	01100100100
mfocrf		011111	D	1	CRM	000000100110
stswx		011111	S	A	B	10100101010
stwbrx		011111	S	A	B	10100101100
stfsx		011111	S	A	B	10100101110
stfsux		011111	S	A	B	10101101110
stswi		011111	S	A	NB	10110101010
stfdx		011111	S	A	B	10110101110
stfdux		011111	S	A	B	10111101110
lhbrx		011111	D	A	B	11000101100
srawx		011111	S	A	B	1100011000	Rc
sradx		011111	S	A	B	1100011010	Rc
srawix		011111	S	A	SH	1100111000	Rc
eieio		01111100000000000000011010101100
sthbrx		011111	S	A	B	11100101100
extshx		011111	S	A	000001110011010	Rc
extsbx		011111	S	A	000001110111010	Rc
icbi		01111100000	A	B	11110101100
stfiwx		011111	S	A	B	11110101110
extswx		011111	S	A	000001111011010	Rc
dcbz		01111100000	A	B	11111101100
lwz			100000	D	A	d
lwzu		100001	D	A	d
lbz			100010	D	A	d
lbzu		100011	D	A	d
stw			100100	S	A	d
stwu		100101	S	A	d
stb			100110	S	A	d
stbu		100111	S	A	d
lhz			101000	D	A	d
lhzu		101001	D	A	d
lha			101010	D	A	d
lhau		101011	D	A	d
sth			101100	S	A	d
sthu		101101	S	A	d
lmw			101110	D	A	d
stmw		101111	S	A	d
lfs			110000	D	A	d
lfsu		110001	D	A	d
lfd			110010	D	A	d
lfdu		110011	D	A	d
stfs		110100	S	A	d
stfsu		110101	S	A	d
stfd		110110	S	A	d
stfdu		110111	S	A	d
ld			111010	D	A	ds	00
ldu			111010	D	A	ds	01
lwa			111010	D	A	ds	10
fdivsx		111011	D	A	B	0000010010	Rc
fsubsx		111011	D	A	B	0000010100	Rc
faddsx		111011	D	A	B	0000010101	Rc
fsqrtsx		111011	D	00000	B	0000010110	Rc
fresx		111011	D	00000	B	0000011000	Rc
fmulsx		111011	D	A	00000	C	11001	Rc
fmsubsx		111011	D	A	B	C	11100	Rc
fmaddsx		111011	D	A	B	C	11101	Rc
fnmsubsx	111011	D	A	B	C	11110	Rc
fnmaddsx	111011	D	A	B	C	11111	Rc
std			111110	S	A	ds	00
stdu		111110	S	A	ds	01
fcmpu		111111	crfD	00	A	B	00000000000
frspx		111111	D	00000	B	0000001100	Rc
fctiwx		111111	D	00000	B	0000001110
fctiwzx		111111	D	00000	B	0000001111	Rc
fdivx		111111	D	A	B	0000010010	Rc
fsubx		111111	D	A	B	0000010100	Rc
faddx		111111	D	A	B	0000010101	Rc
fsqrtx		111111	D	00000	B	0000010110	Rc
fselx		111111	D	A	B	C	10111	Rc
fmulx		111111	D	A	00000	C	11001	Rc
frsqrtex	111111	D	00000	B	0000011010	Rc
fmsubx		111111	D	A	B	C	11100	Rc
fmaddx		111111	D	A	B	C	11101	Rc
fnmsubx		111111	D	A	B	C	11110	Rc
fnmaddx		111111	D	A	B	C	11111	Rc
fcmpo		111111	crfD	00	A	B	00001000000
mtfsb1x		11111	crbD	00000000000000100110	Rc
fnegx		111111	D	00000	B	0000101000	Rc
mcrfs		111111	crfD	00	crfS	000000000010000000
mtfsb0x		111111	crbD	00000000000001000110	Rc
fmrx		111111	D	00000	B	0001001000	Rc
mtfsfix		111111	crfD	0000000	IMM	00010000110	Rc
fnabsx		111111	D	00000	B	0010001000	Rc
fabsx		111111	D	00000	B	0100001000	Rc
mffsx		111111	D	00000000001001000111	Rc
mtfsfx		1111110	FM	0	B	1011000111	Rc
fctidx		111111	D	00000	B	1100101110	Rc
fctidzx		111111	D	00000	B	1100101111	Rc
fcfidx		111111	D	00000	B	1101001110	Rc
#endif