#include "disassembler.h"

using namespace iso;

#undef IN
#undef OUT

enum MNEMONIC {
	rsvd,
	NOP,	MOVW,	MULS,	MULSU,	FMUL,	FMULS,	FMULU,	CPC,
	CP,		SBC,	SUB,	ADD,	ADC,	CPSE,	AND,	EOR,
	OR,		MOV,	CPI,	SBCI,	SUBI,	ORI,	ANDI,
	LD,		ST,		LPM,	ELPM,	XCH,	LAS,
	LAC,	LAT,	POP,	PUSH,	COM,	NEG,	SWAP,	INC,
	ASR,	LSR,	ROR,	RET,	RETI,	SLEEP,
	BREAK,	WDR,	SPM,	DEC,	DES,	JMP,	CALL,	ADIW,
	SBIW,	CBI,	SBI,	SBIC,	SBIS,	MUL,	IN,		OUT,
	LDI,	BLD,	BST,	SBRC,	SBRS,
	BRCC,	BRCS,	BRNE,	BREQ,	BRPL,	BRMI,	BRVC,	BRVS,
	BRGE,	BRLT,	BRHC,	BRHS,	BRTC,	BRTS,	BRID,	BRIE,
	SEC,	SEZ,	SEN,	SEV,	SES,	SEH,	SET,	SEI,
	CLC,	CLZ,	CLN,	CLV,	CLS,	CLH,	CLT,	CLI,
};

static const char *mnemonics[] = {
	"reserved",
	"nop",	"movw",	"muls",	"mulsu","fmul",	"fmuls","fmulu","cpc",
	"cp",	"sbc",	"sub",	"add",	"adc",	"cpse",	"and",	"eor",
	"or",	"mov",	"cpi",	"sbci",	"subi",	"ori",	"andi",
	"ld",	"st",	"lpm",	"elpm",	"xch",	"las",
	"lac",	"lat",	"pop",	"push",	"com",	"neg",	"swap",	"inc",
	"asr",	"lsr",	"ror",	"ret",	"reti",	"sleep",
	"break","wdr",	"spm",	"dec",	"des",	"jmp",	"call",	"adiw",
	"sbiw",	"cbi",	"sbi",	"sbic",	"sbis",	"mul",	"in",	"out",
	"ldi",	"bld",	"bst",	"sbrc",	"sbrs",
	"brcc",	"brcs",	"brne",	"breq",	"brpl",	"brmi",	"brvc",	"brvs",	"brge",	"brlt",	"brhc",	"brhs",	"brtc",	"brts",	"brid",	"brie",
	"sec",	"sez",	"sen",	"sev",	"ses",	"seh",	"set",	"sei",
	"clc",	"clz",	"cln",	"clv",	"cls",	"clh",	"clt",	"cli",
};

enum MODE {
	NONE,
	Rd,			//rd
	RdRr,		//rd, rr
	RdRr4,		//rd, rr
	RdRr3,		//rd, rr
	RdRrW,		//rd, rr
	RpK,		//
	Rd4K,		//
	RdK3,		//
	YZK,		//Y+k, Z+k
	RdX,
	RdXinc,
	RdXdec,
	YZ,
	YZinc,
	YZdec,
	Z,
	Zinc,
	IOK3,
	IO,			//io,io
	K4,
	BRANCH,
	OFF12,
	ADDR16,
	ADDR22,
};

struct OP {
	uint16		mask, val;
	uint8		mnemonic, mode;
};

OP opcodes[] = {
	{0xffff, 0x0000, NOP,	NONE	},
	{0xff00, 0x0100, MOVW,	RdRrW	},
	{0xff00, 0x0200, MULS,	RdRr4	},
	{0xff88, 0x0300, MULSU,	RdRr3	},
	{0xff88, 0x0308, FMUL,	RdRr3	},
	{0xff88, 0x0380, FMULS,	RdRr3	},
	{0xff88, 0x0388, FMULU,	RdRr3	},
	//2-operand instructions
	{0xfc00, 0x0400, CPC,	RdRr	},
	{0xfc00, 0x1400, CP,	RdRr	},
	{0xfc00, 0x0800, SBC,	RdRr	},
	{0xfc00, 0x1800, SUB,	RdRr	},
	{0xf800, 0x0800, ADD,	RdRr	},
	{0xf800, 0x1800, ADC,	RdRr	},
	{0xfc00, 0x1000, CPSE,	RdRr	},
	{0xfc00, 0x2000, AND,	RdRr	},
	{0xfc00, 0x2400, EOR,	RdRr	},
	{0xfc00, 0x2800, OR,	RdRr	},
	{0xfc00, 0x2c00, MOV,	RdRr	},
	{0xf000, 0x3000, CPI,	Rd4K	},
	//Register-immediate operations
	{0xf000, 0x4000, SBCI,	Rd4K	},
	{0xf000, 0x5000, SUBI,	Rd4K	},
	{0xf000, 0x6000, ORI,	Rd4K	},
	{0xf000, 0x7000, ANDI,	Rd4K	},
	{0xd200, 0x8000, LD,	YZK		},
	{0xd200, 0x8200, ST,	YZK	},
	//Load/store operations
	{0xfe0f, 0x9000, LD,	ADDR16	},
	{0xfe0f, 0x9200, ST,	ADDR16	},
	{0xfe07, 0x9001, LD,	YZ		},
	{0xfe07, 0x9201, ST,	YZ		},
	{0xfe07, 0x9002, LD,	YZinc	},
	{0xfe07, 0x9202, ST,	YZinc	},
	{0xfe0f, 0x9004, LPM,	YZ		},
	{0xfe0f, 0x9005, LPM,	YZinc	},
	{0xfe0f, 0x9006, ELPM,	YZ		},
	{0xfe0f, 0x9007, ELPM,	YZinc	},
	{0xfe0f, 0x9204, XCH,	YZ		},
	{0xfe0f, 0x9205, LAS,	YZ		},
	{0xfe0f, 0x9206, LAC,	YZ		},
	{0xfe0f, 0x9207, LAT,	YZ		},
	{0xfe0f, 0x900c, LD,	RdX		},
	{0xfe0f, 0x920c, ST,	RdX		},
	{0xfe0f, 0x900d, LD,	RdXinc	},
	{0xfe0f, 0x920d, ST,	RdXinc	},
	{0xfe0f, 0x900e, LD,	RdXdec	},
	{0xfe0f, 0x920e, ST,	RdXdec	},
	{0xfe0f, 0x900f, POP,	Rd		},
	{0xfe0f, 0x920f, PUSH,	Rd		},
	//One-operand instructions:
	{0xfe0f, 0x9400, COM,	Rd		},
	{0xfe0f, 0x9401, NEG,	Rd		},
	{0xfe0f, 0x9402, SWAP,	Rd		},
	{0xfe0f, 0x9403, INC,	Rd		},
	{0xfe0f, 0x9404, rsvd,	NONE	},
	{0xfe0f, 0x9405, ASR,	Rd		},
	{0xfe0f, 0x9406, LSR,	Rd		},
	{0xfe0f, 0x9407, ROR,	Rd		},

	{0xffff, 0x9408, SEC,	NONE	},
	{0xffff, 0x9418, SEZ,	NONE	},
	{0xffff, 0x9428, SEN,	NONE	},
	{0xffff, 0x9438, SEV,	NONE	},
	{0xffff, 0x9448, SES,	NONE	},
	{0xffff, 0x9458, SEH,	NONE	},
	{0xffff, 0x9468, SET,	NONE	},
	{0xffff, 0x9478, SEI,	NONE	},
	{0xffff, 0x9488, CLC,	NONE	},
	{0xffff, 0x9498, CLZ,	NONE	},
	{0xffff, 0x94a8, CLN,	NONE	},
	{0xffff, 0x94b8, CLV,	NONE	},
	{0xffff, 0x94c8, CLS,	NONE	},
	{0xffff, 0x94d8, CLH,	NONE	},
	{0xffff, 0x94e8, CLT,	NONE	},
	{0xffff, 0x94f8, CLI,	NONE	},

	//Zero-operand instructions
	{0xffff, 0x9508, RET,	NONE	},
	{0xffff, 0x9518, RETI,	NONE	},
	{0xffef, 0x9528, rsvd,	NONE	},
	{0xffcf, 0x9548, rsvd,	NONE	},
	{0xffff, 0x9588, SLEEP,	NONE	},
	{0xffff, 0x9598, BREAK,	NONE	},
	{0xffff, 0x95a8, WDR,	NONE	},
	{0xffff, 0x95b8, rsvd,	NONE	},
	{0xffff, 0x95c8, LPM,	NONE	},//R0,Z
	{0xffff, 0x95d8, ELPM,	NONE	},//R0,Z
	{0xffff, 0x95e8, SPM,	NONE	},//Z
	{0xffff, 0x95f8, SPM,	Zinc	},

	{0xffef, 0x9409, JMP,	Z		},
	{0xffef, 0x9509, CALL,	Z		},
	{0xfe0f, 0x940a, DEC,	Rd		},
	{0xff0f, 0x940b, DES,	K4		},
	{0xfe0e, 0x940c, JMP,	ADDR22	},
	{0xfe0e, 0x940e, CALL,	ADDR22	},
	{0xff00, 0x9600, ADIW,	RpK		},
	{0xff00, 0x9700, SBIW,	RpK		},
	{0xff00, 0x9800, CBI,	IOK3	},
	{0xff00, 0x9a00, SBI,	IOK3	},
	{0xff00, 0x9900, SBIC,	IOK3	},
	{0xff00, 0x9b00, SBIS,	IOK3	},
	{0xfc00, 0x9c00, MUL,	RdRr	},
	{0xf800, 0xb000, IN,	IO		},
	{0xf800, 0xb800, OUT,	IO		},
	{0xf000, 0xc000, JMP,	OFF12	},
	{0xf000, 0xd000, CALL,	OFF12	},
	{0xf000, 0xe000, LDI,	Rd4K	},
	{0xfc07, 0xf400, BRCC,	BRANCH	},
	{0xfc07, 0xf000, BRCS,	BRANCH	},
	{0xfc07, 0xf401, BRNE,	BRANCH	},
	{0xfc07, 0xf001, BREQ,	BRANCH	},
	{0xfc07, 0xf402, BRPL,	BRANCH	},
	{0xfc07, 0xf002, BRMI,	BRANCH	},
	{0xfc07, 0xf403, BRVC,	BRANCH	},
	{0xfc07, 0xf003, BRVS,	BRANCH	},
	{0xfc07, 0xf404, BRGE,	BRANCH	},
	{0xfc07, 0xf004, BRLT,	BRANCH	},
	{0xfc07, 0xf405, BRHC,	BRANCH	},
	{0xfc07, 0xf005, BRHS,	BRANCH	},
	{0xfc07, 0xf406, BRTC,	BRANCH	},
	{0xfc07, 0xf006, BRTS,	BRANCH	},
	{0xfc07, 0xf407, BRID,	BRANCH	},
	{0xfc07, 0xf007, BRIE,	BRANCH	},
	{0xfe08, 0xf800, BLD,	RdK3	},
	{0xfe08, 0xfa00, BST,	RdK3	},
	{0xfe08, 0xfc00, SBRC,	RdK3	},
	{0xfe08, 0xfe00, SBRS,	RdK3	},
	{0xf808, 0xf808, rsvd,	NONE	},

	{0xf800, 0xa800, ST,	Rd4K	},
	{0xf800, 0xa000, LD,	Rd4K	},
};

union CODE {
	uint16	u;
	struct { uint16 :4, b:3; }			STATUS;
	struct { uint16 r:3, :1, d:3; }		RdRr3;
	struct { uint16 r:4, d:4; }			RdRrW;
	struct { uint16 r:4, d:4; }			RdRr4;
	struct { uint16 :4, d:5; }			Rd, RdX, RdXinc, RdXdec;
	struct { uint16 k:3, :1, d:5; }		RdK3;
	struct { uint16 k:3, a:5; }			IOK3;
	struct { uint16 :4, k:4; }			K4;
	struct { int16 :3, offset:7; }		BRANCH;
	struct { int16 offset:12; }			OFF12;
	struct { uint16 :4, e:1; }			Z;
	struct { uint16 r0:4, d:5, r1:1;					uint8 r() const { return r0 | (r1 << 4); } }				RdRr;
	struct { uint16 k0:4, d:5, k1:4;					uint8 k() const { return k0 | (k1 << 4); } }				Rd4K;
	struct { uint16 k0:4, p:2, k1:2;					uint8 k() const { return k0 | (k1 << 4); } }				RpK;
	struct { uint16 k0:3, y:1, d:5, :1, k1:2, :1, k2:1;	uint8 k() const { return k0 | (k1 << 3) | (k2 << 5); } }	YZK;
	struct { uint16 a0:4, d:5, a1:2;					uint8 a() const { return a0 | (a1 << 4); } }				IO;
	struct { uint16 a0:1,:3, a1:5;						uint8 a() const { return a0 | (a1 << 1); } }				_ADDR22;

	uint16	ADDR16()	const { return this[1].u & 2; }
	uint32	ADDR22()	const { return (this[1].u | (_ADDR22.a() << 16)) * 2; }
	char	YZ()		const { return YZK.y ? 'Y' : 'Z'; }
};

const char *special_regs[] = {
	"RAMPD",	//0x38	0x58
	"RAMPX",	//0x39	0x59
	"RAMPY",	//0x3A	0x5A
	"RAMPZ",	//0x3B	0x5B
	"EIND",		//0x3C	0x5C
	"SP_lo",	//0x3D	0x5D
	"SP_hi",	//0x3E	0x5E
	"SREG",		//0x3F	0x5F
};

class DisassemblerATMEL : public Disassembler {
public:
	virtual	const char*	GetDescription()	{ return "ATMEL"; }
	virtual State*		Disassemble(const iso::memory_block &block, uint64 addr, SymbolFinder sym_finder);

	static string_accum &put_reg(string_accum &sa, int r) {
		if (r >= 24)
			return sa << ' ' << "WXYZ"[(r - 24) >> 1] << ifelse(r & 1, "_hi", "_lo");
		return sa << " r" << r;
	}
	static string_accum &put_regW(string_accum &sa, int r) {
		if (r >= 12)
			return sa << ' ' << "WXYZ"[r - 12];
		return sa << " r" << r * 2 + 1 << ":r" << r * 2;
	}
	string_accum&	put_address(string_accum &sa, uint32 address, SymbolFinder sym_finder) {
		uint64			sym_addr;
		string_param	sym_name;
		if (sym_finder && sym_finder(address, sym_addr, sym_name)) {
			sa << sym_name;
			return sym_addr != address ? sa << "+" << address - sym_addr : sa;
		}
		return sa << " 0x" << hex(address);
	}
	static string_accum&	put_io(string_accum &sa, uint8 address) {
		if (address < 0x20)
			return put_reg(sa, address);
		if (address < 0x38)
			return sa << " 0x" << hex(address);
		return sa << ' ' << special_regs[address - 0x38];
	}

} disatmel;

Disassembler::State *DisassemblerATMEL::Disassemble(const memory_block &block, uint64 addr, SymbolFinder sym_finder) {
	StateDefault	*state = new StateDefault;

	for (uint16 *p = block, *end = block.end(); p < end; ) {
		uint16	offset		= uint16((uint8*)p - (uint8*)block + addr);

		uint64			sym_addr;
		string_param	sym_name;
		if (sym_finder && sym_finder(offset, sym_addr, sym_name) && sym_addr == offset)
			state->lines.push_back(sym_name);

		uint16 u = *p;
		OP *op = 0;
		for (auto &i : opcodes) {
			if ((u & i.mask) == i.val) {
				op = &i;
				break;
			}
		}

		int	nb = op && (op->mode == ADDR16 || op->mode == ADDR22) ? 2 : 1;

		buffer_accum<1024>	ba("%04x ", offset);
		ba.format("%04x ", p[0]);

		if (nb == 1)
			ba.putc(' ', 5);
		else
			ba.format("%04x ", p[1]);

		if (op) {
			ba << mnemonics[op->mnemonic];
			CODE &code = *(CODE*)p;
			switch (op->mode) {
				case NONE:		break;
				case Rd:		put_reg(ba, code.Rd.d); break;
				case RdRr:		put_reg(put_reg(ba, code.RdRr.d) << ',', code.RdRr.r()); break;
				case RdRr4:		put_reg(put_reg(ba, code.RdRr4.d) << ',', code.RdRr4.r); break;
				case RdRr3:		put_reg(put_reg(ba, code.RdRr3.d) << ',', code.RdRr3.r); break;
				case RdRrW:		put_regW(put_regW(ba, code.RdRrW.d) << ',', code.RdRrW.r); break;
				case RpK:		put_regW(ba, code.RpK.p + 12) << ", " << code.RpK.k(); break;
				case Rd4K:		put_reg(ba, code.Rd4K.d) << ", " << code.Rd4K.k(); break;
				case RdK3:		put_reg(ba, code.RdK3.d) << ", " << code.RdK3.k; break;
				case YZK:		put_reg(ba, code.YZK.d) << ", " << code.YZ() << "+" << code.YZK.k(); break;
				case RdX:		put_reg(ba, code.Rd.d) << ", X"; break;
				case RdXinc:	put_reg(ba, code.Rd.d) << ", X+"; break;
				case RdXdec:	put_reg(ba, code.Rd.d) << ", -X"; break;
				case YZ:		put_reg(ba, code.Rd.d) << ", " << code.YZ(); break;
				case YZinc:		put_reg(ba, code.Rd.d) << ", " << code.YZ() << '+'; break;
				case YZdec:		put_reg(ba, code.Rd.d) << ", -" << code.YZ(); break;
				case Z:			ba << ifelse(code.Z.e, " EIND:Z", " Z"); break;
				case Zinc:		ba << " Z+"; break;
				case IOK3:		put_io(ba, code.IOK3.a) << ", " << code.IOK3.k; break;
				case IO:		put_io(put_reg(ba, code.IO.d) << ',', code.IO.a()); break;
				case K4:		ba << code.K4.k; break;
				case BRANCH:	put_address(ba, offset + (code.BRANCH.offset + 1) * 2, sym_finder); break;
				case OFF12:		put_address(ba, offset + (code.OFF12.offset + 1) * 2, sym_finder); break;
				case ADDR16:	put_address(ba, code.ADDR16(), sym_finder); break;
				case ADDR22:	put_address(ba, code.ADDR22(), sym_finder); break;
			}
		} else {
			ba << "?";
		}

		p += nb;
		state->lines.push_back((const char*)ba);
	}
	return state;
}
