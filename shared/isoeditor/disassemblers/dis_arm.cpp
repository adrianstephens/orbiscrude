#include "disassembler.h"

using namespace iso;
/*
Field {field}
_c Control field mask bit 3
_f Flags field mask bit 0
_s Status field mask bit 1
_x Extension field mask bit 2
*/

enum REG {
	SP	= 13,
	LR,
	PC,
};
enum COND {
	EQ,		// Z set equal
	NE,		// Z clear not equal
	CS,		// C set unsigned higher or same
	CC,		// C clear unsigned lower
	MI,		// N set negative
	PL,		// N clear positive or zero
	VS,		// V set overflow
	VC,		// V clear no overflow
	HI,		// C set and Z clear unsigned higher
	LS,		// C clear or Z set unsigned lower or same
	GE,		// N equals V greater or equal
	LT,		// N not equal to V less than
	GT,		// Z clear AND (N equals V) greater than
	LE,		// Z set OR (N not equal to V) less than or equal
	AL,		// (ignored) always
	EXT,
};
enum SHIFTOP {
	LSL,	// logical left
	LSR,	// logical right
	ASR,	// arithmetic right
	ROR,	// rotate right
};
struct shift	{
	uint32 rm:4, r:1, type:2, count:5;
	shift(uint32 u) { *(uint32*)this = u; }
};
static const char conditions[]	= "eq" "ne" "cs" "cc" "mi" "pl" "vs" "vc" "hi" "ls" "ge" "lt" "gt" "le" "al" "nv";
static const char shifts[]		= "lsl" "lsr" "asr" "ror";

enum REG_MODE {
	RM_GENERAL,
	RM_FLOAT,			// interpret regs with extra bit at bottom
	RM_SIMD_DOUBLE,		// interpret regs with extra bit at top
	RM_SIMD_QUAD,		// interpret regs with extra bit at top
	RM_COP,
	RM_GENERAL64,
	RM_GENERAL64_32,
	RM_GENERAL64_SP,
	RM_GENERAL64_32_SP,
	RM_SIMD64,
	RM_SIMD64_8		= RM_SIMD64,
	RM_SIMD64_16,
	RM_SIMD64_32,
	RM_SIMD64_64,
	RM_SIMD64_128,
};

enum MODE {
//shared
	UNDEF,
	REG		= UNDEF,
	FLT,
	SIMD,
	BRANCH,
	SPECIFIC,
	TABLE	= 15,

//32bit
	SHIFT	= SPECIFIC,
	AUTO,
	ADDR8,	// offset in 0:8
	ADDR8S,	// offset in 0:4 & 8:4 (implies SH)
	ADDR12,	// offset in 0:12

//thumb
	THUMB	= SPECIFIC,
	TBRANCH,
	THUMB32,
	T32BRANCH,

//64bit
	XBASE	= SPECIFIC,
	XOFFSET,
	XOFFSET_UNSCALED,
	XPRE_INDEX,
	XPOST_INDEX,
	XADDR,
	XSRX,
	XMULTI1,
	XMULTI2,
	XMULTI3,
	XMULTI4,
};

enum FLAGS {
//32bit
	COP1	= 1 << 0,
	COP2	= 1 << 1,
	MASK	= 1 << 2,
	S20		= 1 << 3,
	S22		= 1 << 4,
	I22		= 1 << 5,
	LIST	= 1 << 6,
//thumb
	THILO	= 1 << 0,
	THI		= 1 << 1,
	TSHIFT	= 1 << 2,
//64bit
	XSIZE31		= 1 << 0,	// size is in bit 31
	XL			= 1 << 1,	// L bit (load) is in bit 22
	XS			= 1 << 2,	// S bit in bit 29
	X32			= 1 << 3,	//32 bit
	XSIMDSIZE	= 1 << 4,	//simd, size in 30:2
	XSIMDSIZE1	= 1 << 5,	//simd, size in 30:2->8,16,32,64
	XSIMD128	= XSIMDSIZE | XSIMDSIZE1,
	XSIMD		= XSIMDSIZE | XSIMDSIZE1,
};
enum DEST {
//32bit
	RD12	= 1,
	RD16,
	DPSR,
//thumb
	TRD0	= 1,
	TRD8,
//64bit
	XRD0	= 1,	//0:5
	XRD5,			//5:5
	XRDx2,			//0:5,10:5
};
enum SOURCE {
//32bit
	R0		= 1,
	R4,
	R8,
	R12,
	R16,
	R16I,
	SPSR,
	IMM4,
	IMM5,
	IMM12,
	IMM24,
	IMM5_7,
	IMM5_16,
	IMM5_16_M1,
	IMM3_5,
//thumb
	TR0,
	TR3,
	TR6,
	TSP,
	TIMM8,
	TIMM6_3,
	TIMM6_5,
	TIMM3_5,
	TIMM7,
	TLIST,
//64bit
	XR0	= 1,
	XR5,
	XR10,
	XR16,
	XIMM,
	XIMM7	= XIMM,	//15:7
	XIMMS,			//10:6
	XIMMR,			//16:6
	XIMM9,			//12:9
	XIMM12,			//10:12
	XIMM14,			//5:14
	XIMM16,			//5:16
	XIMM19,			//5:19
	XIMM26,			//0:26
	XIMM16_5,		//16:5
	XIMM16_7,		//16:7
	XFLAGS,			//0:4
	XIMMF,			//s1e3m4 in 13:8
	XFPBITS,		//10:6

	XEXTENDED,
	XSHIFTED,
	XCOND,
	XQOFFSET,
	XSHIFT_IMM,		// in 18,17,16,9,8,7,6,5
};

struct OP {
	struct SPEC {
		uint32	mode:4, flags:8, dest:2, src1:5, src2:5, src3:5;
		DEST	get_dest()			const { return DEST(dest); }
		SOURCE	get_source(int i)	const { return SOURCE(i == 0 ? src1 : i == 1 ? src2 : src3); }
	};
	const char	*name;
	uint32		mask, val;
	SPEC		spec;
};

//-----------------------------------------------------------------------------
//	DisassemblerARMbase
//-----------------------------------------------------------------------------

class DisassemblerARMbase : public Disassembler {
public:
	string_accum&	put_address(string_accum &sa, uint32 address, SymbolFinder sym_finder) {
		uint64			sym_addr;
		string_param	sym_name;
		if (sym_finder && sym_finder(address, sym_addr, sym_name)) {
			sa << sym_name;
			return sym_addr != address ? sa << "+" << address - sym_addr : sa;
		}
		return sa.format("0x%08x", address);
	}

	string_accum&	put_reg(string_accum &sa, uint32 reg, REG_MODE mode = RM_GENERAL, int extrabit = 0) {
		switch (mode) {
			default:
				switch (reg) {
					case 13: return sa << "sp";
					case 14: return sa << "lr";
					case 15: return sa << "pc";
					default: return sa << 'r' << reg;
				}
			case RM_COP:
				return sa << 'c' << reg;

			case RM_FLOAT:
				return sa << 's' << ((reg << 1) | extrabit);

			case RM_SIMD_DOUBLE:
				return sa << 'd' << (reg | (extrabit << 4));

			case RM_SIMD_QUAD:
				return sa << 'q' << ((reg >> 1) | (extrabit << 3));

			case RM_GENERAL64:
			case RM_GENERAL64_32:
			case RM_GENERAL64_SP:
			case RM_GENERAL64_32_SP:
				if (reg == 31) {
					switch (mode) {
						case RM_GENERAL64:		return sa << "xzr";
						case RM_GENERAL64_32:	return sa << "wzr";
						case RM_GENERAL64_SP:	return sa << "sp";
						case RM_GENERAL64_32_SP:return sa << "wsp";
					}
				}
				return sa << (mode == RM_GENERAL64_32 || mode == RM_GENERAL64_32_SP ? 'w' : 'x') << reg;

			case RM_SIMD64_8:
				return sa << 'b' << reg;

			case RM_SIMD64_16:
				return sa << 'h' << reg;

			case RM_SIMD64_32:
				return sa << 's' << reg;

			case RM_SIMD64_64:
				return sa << 'd' << reg;

			case RM_SIMD64_128:
				return sa << 'q' << reg;

		}
	}
	string_accum&	put_coreg(string_accum &sa, uint32 reg) {
		return sa << 'c' << reg;
	}
	string_accum&	put_imm(string_accum &sa, uint32 v) {
		return sa << '#' << v;
	}
	template<int N> string_accum&	put_imm(string_accum &sa, uint32 v) {
		return sa << '#' << (v & bits(N));
	}

	string_accum&	put_shift(string_accum &sa, uint8 count, SHIFTOP op) {
		if (count == 0) switch (op) {
			case LSL:	return sa;
			case ROR:	return sa << ",rrx";
			default:	count = 32;
		}
		return sa << ',' << str(shifts + op * 3, 3) << ' ';
	}

	string_accum&	put_shift(string_accum &sa, shift s) {
		sa << 'r' << s.rm;
		if (s.r || s.count) {
			sa << ',' << str(shifts + s.type * 3, 3) << ' ';
			if (s.r)
				sa << 'r' << (s.count >> 1);
			else
				sa << '#' << s.count;
		} else if (s.type == ROR) {
			sa << ",rrx";
		}
		return sa;
	}

	string_accum&	put_list(string_accum &sa, uint32 m) {
		sa << '{';
		while (m) {
			uint8	a = lowest_set_index(m);
			uint8	b = lowest_clear_index(m >> a);
			if (b < 2) {
				put_reg(sa, a);
				b = 1;
			} else {
				put_reg(put_reg(sa, a) << '-', a + b - 1);
			}
			sa << ",";
			m &= ~bits(a + b);
		}
		return sa.move(-1) << "}";
	}

	static OP *get_op(OP *p, uint32 u) {
		while (p->mask) {
			if ((u & p->mask) == p->val) {
				if (p->spec.mode != TABLE)
					break;
				p = (OP*)p->name;
			} else {
				++p;
			}
		}
		return p;
	}
	static bool validate(OP *p, uint32 mask = 0, uint32 val = 0) {
		bool	ret = true;
		while (p->mask) {
			if (p->val & ~p->mask) {
				ISO_TRACEF("Impossible test:") << hex(p->mask) << ":" << hex(p->val) << '\n';
				ret = false;
			}
			if (((val & p->mask) ^ p->val) & mask) {
				ISO_TRACEF("Impossible test in table:") << hex(p->mask) << ":" << hex(p->val) << "(table has " << hex(mask) << ':' << hex(val) << ")\n";
				ret = false;
			}
			if (p->mask & mask)
				ISO_TRACEF("Redundant mask bits:") << hex(p->mask) << ":" << hex(p->val) << "(" << hex(p->mask & mask) << ")\n";

			if (p->spec.mode == TABLE)
				ret &= validate((OP*)p->name, mask | p->mask, val | p->val);
			++p;
		}
		return ret;
	}

};

//-----------------------------------------------------------------------------
//	DisassemblerARM
//-----------------------------------------------------------------------------

class DisassemblerARM : public DisassemblerARMbase {
	union opcode32 {//0									12			16			20						24				28
		struct { uint32 r0:4,	r4:4, r8:4,				r12:4,		r16:4,		b20:1, alu:4,			b25:1,:2,		cond:4;		};
		struct { uint32 :4, cop2:4, cop:4,				:4,			:4,			:1, cop1:3,								:12;		};
		struct { uint32 list:16,									mask:4,		:2,	psr:1,								:9;			};
		struct { uint32 offset12:12,					:4,			:4,			:2, b22:1,								:9;			};
		struct { uint32 off1:4, :1, M:1,Q:1,N:1,off2:4,	:8,						load:1, wb:1, D:1, up:1,pre:1,			:7;			};
		struct { uint32 offset24:24,																	link:1,			:7;			};
		struct { uint32 offset8:8, cpn:4,				:8,						copop:4,								:8;			};
		uint32	u;
		opcode32(uint32 _u) : u(_u) {}
	};

	static OP ops[];
public:
	virtual	const char*	GetDescription() { return "ARM"; }
	virtual State*		Disassemble(const iso::memory_block &block, uint64 addr, SymbolFinder sym_finder) {
		StateDefault	*state = new StateDefault;

		for (uint32 *p = block, *end = (uint32*)block.end(); p < end; ++p) {
			uint32	offset		= uint32(addr + (uint8*)p - (uint8*)block);

			uint64			sym_addr;
			string_param	sym_name;
			if (sym_finder && sym_finder(offset, sym_addr, sym_name) && sym_addr == offset)
				state->lines.push_back(sym_name);

			opcode32	op		= *p;
			buffer_accum<1024>	ba("%08x ", offset);
			ba.format("%08x ", op.u);

			OP			*op2	= get_op(ops, op.u);
			OP::SPEC	spec	= op2->spec;
			MODE		mode	= MODE(spec.mode);
			uint8		flags	= spec.flags;
			REG_MODE	rm		= (flags & COP1) || (flags & COP2) ? RM_COP
				: mode == FLT ? RM_FLOAT
				: mode == SIMD ? (op.Q ? RM_SIMD_QUAD : RM_SIMD_DOUBLE)
				: RM_GENERAL;

			ba << op2->name;;

			if (mode == AUTO)
				ba << (op.up ? 'i' : 'd') << (op.pre ? 'b' : 'a');

			if (op.cond < AL)
				ba << str(conditions + op.cond * 2, 2);

			if (((flags & S20) && op.b20) || ((flags & S22) && op.b22))
				ba << 's';

			ba << ' ';

			const char *sep = ", ";
			if (flags & COP1)
				ba << op.cop << ", #" << op.cop1 << sep;
			else if (flags & COP2)
				ba << op.cop << ", #" << op.cop2 << sep;

			switch (DEST d = DEST(spec.dest)) {
				case RD12:			put_reg(ba, op.r12, rm, op.D);		break;
				case RD16:			put_reg(ba, op.r16, rm);			break;
				case DPSR:			ba << (op.psr ? "spsr" : "cpsr");	break;
				default:			sep = "";
			}

			if (mode == AUTO && op.wb)
				ba << '!';

			for (int i = 0; i < 3; i++) {
				if (SOURCE s = spec.get_source(i)) {
					ba << sep;
					sep = ", ";
					switch (s) {
						case R0:		put_reg(ba, op.r0, rm, op.M);		break;
						case R4:		put_reg(ba, op.r4, rm);				break;
						case R8:		put_reg(ba, op.r8, rm);				break;
						case R12:		put_reg(ba, op.r12, rm);			break;
						case R16:		put_reg(ba, op.r16, rm, op.N);		break;
						case R16I:		put_reg(ba << '[', op.r16) << ']';	break;
						case SPSR:		ba << (op.psr ? "spsr" : "cpsr");	break;

						case IMM4:		put_imm<4>(ba, op.u);				break;
						case IMM5:		put_imm<5>(ba, op.u);				break;
						case IMM12:		put_imm<12>(ba, op.u);				break;
						case IMM24:		put_imm<24>(ba, op.u);				break;
						case IMM5_7:	put_imm<5>(ba, op.u >> 7);			break;
						case IMM5_16:	put_imm<5>(ba, op.u >> 16);			break;
						case IMM5_16_M1:put_imm(ba, ((op.u >> 16) & bits(5)) + 1); break;
						case IMM3_5:	put_imm<3>(ba, op.u >> 5);			break;
					}
				}
			}

			if (flags & LIST)
				put_list(ba, op.list) << onlyif(op.b22, '^');

			int	x = -1;
			switch (mode) {
				case SHIFT:
					if (op.b25)
						put_imm(ba, rotate_right(op.offset12 & 0xff, op.offset12 >> 8));
					else
						put_shift(ba, op.offset12);
					break;

				case BRANCH:
					put_address(ba, offset + 8 + (sign_extend<24>(op.offset24) << 2), sym_finder);
					break;

				//case IMM24:
				//	ba << "0x" << hex(op.offset24);
				//	break;

				case ADDR8S:
					x = op.off1 | (op.off2 << 4);
					break;
				case ADDR12:
					x = op.offset12;
					break;
				case ADDR8:
					x = op.offset8 * 4;
					break;
			}

			if (x >= 0) {
				bool	imm = flags & I22 ? op.b22 : op.b25;
				if (imm && op.r16 == PC) {
					put_address(ba, offset + 8 + (op.up ? x : -x), sym_finder);
				} else {
					put_reg(ba << '[', op.r16) << onlyif(!op.pre, ']');
					if (imm) {
						if (offset)
							ba << ",#" << (op.up ? x : -x);
					} else {
						put_shift(ba << "," << onlyif(!op.up, '-'), x);
					}
				}
				if (op.pre)
					ba << ']' << onlyif(op.wb, '!');
			}

			state->lines.push_back((const char*)ba);
		}
		return state;
	}
} dis_arm;

OP floating_point[] = {
	{ "vmla",	0x00b00000,	0x0000000,	SIMD,	0,	RD12, R16, R0},
	{ "vmls",	0x00b00040,	0x0020000,	SIMD,	0,	RD12, R16, R0},
	{ "vnmla",	0x00b00040,	0x0010040,	SIMD,	0,	RD12, R16, R0},
	{ "vnmls",	0x00b00040,	0x0010000,	SIMD,	0,	RD12, R16, R0},
	{ "vnmul",	0x00b00040,	0x0020040,	SIMD,	0,	RD12, R16, R0},
	{ "vmul",	0x00b00040,	0x0020000,	SIMD,	0,	RD12, R16, R0},
	{ "vadd",	0x00b00040,	0x0030000,	SIMD,	0,	RD12, R16, R0},
	{ "vsub",	0x00b00040,	0x0030040,	SIMD,	0,	RD12, R16, R0},
	{ "vdiv",	0x00b00040,	0x0080000,	SIMD,	0,	RD12, R16, R0},
	{ "vfnma",	0x00b00040,	0x0090040,	SIMD,	0,	RD12, R16, R0},
	{ "vfnms",	0x00b00040,	0x0090000,	SIMD,	0,	RD12, R16, R0},
	{ "vfma",	0x00b00040,	0x00a0000,	SIMD,	0,	RD12, R16, R0},
	{ "vfms",	0x00b00040,	0x00a0040,	SIMD,	0,	RD12, R16, R0},

	{ "vmov",	0x00bf0040,	0x00b0000,	SIMD,	0,	RD12, R16, R0},
	{ "vmov",	0x00bf00c0,	0x00b0040,	SIMD,	0,	RD12, R16, R0},
	{ "vabs",	0x00bf00c0,	0x00b00c0,	SIMD,	0,	RD12, R16, R0},
	{ "vneg",	0x00bf00c0,	0x00b1040,	SIMD,	0,	RD12, R16, R0},
	{ "vsqrt",	0x00bf00c0,	0x00b10c0,	SIMD,	0,	RD12, R16, R0},
	{ "vcvtb",	0x00be00c0,	0x00b2040,	SIMD,	0,	RD12, R16, R0},
	{ "vcvtt",	0x00be00c0,	0x00b20c0,	SIMD,	0,	RD12, R16, R0},
	{ "vcmp",	0x00be00c0,	0x00b4040,	SIMD,	0,	RD12, R16, R0},
	{ "vcmpe",	0x00be00c0,	0x00b40c0,	SIMD,	0,	RD12, R16, R0},
	{ "vrintr",	0x00bf00c0,	0x00b6040,	SIMD,	0,	RD12, R16, R0},
	{ "vrintz",	0x00bf00c0,	0x00b60c0,	SIMD,	0,	RD12, R16, R0},
	{ "vrintx",	0x00bf00c0,	0x00b7040,	SIMD,	0,	RD12, R16, R0},
	{ "vcvt",	0x00bf00c0,	0x00b70c0,	SIMD,	0,	RD12, R16, R0},
	{ "vcvt",	0x00bf0040,	0x00b8040,	SIMD,	0,	RD12, R16, R0},
	{ "vcvt",	0x00ba0040,	0x00ba040,	SIMD,	0,	RD12, R16, R0},
	{ "vcvtr",	0x00be00c0,	0x00bc040,	SIMD,	0,	RD12, R16, R0},
	{ "vcvt",	0x00be00c0,	0x00bc0c0,	SIMD,	0,	RD12, R16, R0},
	0,
};

//	{ "vsel",	0x008f0040,	0x0000000,},
//	{ "vmaxnm",	0x00bf0040,	0x00a0040,},
//	0

// 0xf0000000, 0xf0000000,		TABLE													},
OP unconditional[] = {
	{"",		0xf8000000, 0xf0000000,													},//memory hints
	{"srs",		0xfe500000, 0xf8400000,	AUTO,	0,		0,IMM5							},//store return state
	{"rfe",		0xfe500000, 0xf8100000,	AUTO,	0,		RD16							},//return from exception
	{"blx",		0xf7000000, 0xfa000000,	BRANCH											},//branch with link & exchange
	{"mcrr2",	0xfff00000, 0xfc400000,	REG,	COP2,	RD12,R16,R0						},//mcrr, mcrr2
	{"mrrc2",	0xfff00000, 0xfc500000,	REG,	COP2,	RD12,R16,R0						},//mrrc, mrrc2
	{"stc2",	0xf7100000, 0xfc000000,	REG,	COP2,	RD12,R16,R0						},//store coprocessor
	{"ldc2",	0xf7100000, 0xfc100000,	REG,	COP2,	RD12,R16,R0						},//load coprocessor
	{"cdp2",	0xff000010, 0xfe000000,	REG,	COP2,	RD12,R16,R0						},//cdp, cdp2
	{"mcr2",	0xff100010, 0xfe000010,	REG,	COP2,	RD12,R16,R0						},//mcr, mcr2
	{"mrc2",	0xff100010, 0xfe100010,	REG,	COP2,	RD12,R16,R0						},//mrc, mrc2
	0
};

OP miscellaneous[] = {
//	{"",		0x0f900080, 0x01000000,													},
	{"mrs",		0x0fb002f0, 0x01000000,													},//mrs
	{"msr",		0x0ff302f0, 0x01200000,													},//msr (reg) app
	{"msr",		0x0ff302f0, 0x01210000,													},//msr (reg) sys
	{"msr",		0x0ff202f0, 0x01220000,													},//msr (reg) sys

	{"mrs",		0x0fb002f0, 0x01000200,													},//mrs (banked)
	{"msr",		0x0fb002f0, 0x01200200,													},//msr (banked)

	{"bx", 		0x0ff000f0, 0x01200010,	REG,	0,		0,R0							},//bx
	{"clz",		0x0ff000f0, 0x01600010,	REG,	0,		RD12,R0							},//clz
	{"bxj",		0x0ff000f0, 0x01200020,	REG,	0,		0,R0							},//bxj
	{"blx",		0x0ff000f0, 0x01200030,	REG,	0,		0,R0							},//blx

//	{"",		0x0f9002f0, 0x01000040,													},//crc32
	{"crc32b",	0x0ff002f0, 0x01000040,	REG,	0,		RD12,R16,R0						},
	{"crc32h",	0x0ff002f0, 0x01200040,	REG,	0,		RD12,R16,R0						},
	{"crc32w",	0x0ff002f0, 0x01400040,	REG,	0,		RD12,R16,R0						},
//	{"",		0x0f9002f0, 0x01000240,													},//crc32c
	{"crc32cb",	0x0ff002f0, 0x01000240,	REG,	0,		RD12,R16,R0						},
	{"crc32ch",	0x0ff002f0, 0x01200240,	REG,	0,		RD12,R16,R0						},
	{"crc32cw",	0x0ff002f0, 0x01400240,	REG,	0,		RD12,R16,R0						},

//	{"",		0x0f9000f0, 0x01000050,													},//saturating add/sub
	{"qadd",	0x0ff000f0, 0x01000050,	REG,	0,		RD12,R16,R0						},
	{"qsub",	0x0ff000f0, 0x01200050,	REG,	0,		RD12,R16,R0						},
	{"qdadd",	0x0ff000f0, 0x01400050,	REG,	0,		RD12,R16,R0						},
	{"qdsub",	0x0ff000f0, 0x01600050,	REG,	0,		RD12,R16,R0						},

	{"",		0x0ff000f0, 0x01600060,													},//exception return

	{"hlt",		0x0ff000f0, 0x01000070,	REG,	0,		0,IMM12							},
	{"bkpt",	0x0ff000f0, 0x01200070,	REG,	0,		0,IMM12							},
	{"hvc",		0x0ff000f0, 0x01400070,	REG,	0,		0,IMM12							},
	{"smc",		0x0ff000f0, 0x01600070,	REG,	0,		0,IMM4							},
	0,
};

OP halfword_multiply[] = {
//	{"",		0x0f900090, 0x01000080,													}
	{"smlabb",	0x0ff000f0, 0x01000080,	REG,	0,		RD16,R0,R8,R12					},	//Signed 16-bit multiply, 32-bit accumulate SMLABB, SMLABT, SMLATB, SMLATT
	{"smlabt",	0x0ff000f0, 0x010000a0,	REG,	0,		RD16,R0,R8,R12					},	//Signed 16-bit multiply, 32-bit accumulate SMLABB, SMLABT, SMLATB, SMLATT
	{"smlatb",	0x0ff000f0, 0x010000c0,	REG,	0,		RD16,R0,R8,R12					},	//Signed 16-bit multiply, 32-bit accumulate SMLABB, SMLABT, SMLATB, SMLATT
	{"smlatt",	0x0ff000f0, 0x010000e0,	REG,	0,		RD16,R0,R8,R12					},	//Signed 16-bit multiply, 32-bit accumulate SMLABB, SMLABT, SMLATB, SMLATT
	{"smlawb",	0x0ff000f0, 0x01200080,	REG,	0,		RD16,R0,R8,R12					},	//Signed 16-bit × 32-bit multiply, 32-bit accumulate SMLAWB, SMLAWT
	{"smlawt",	0x0ff000f0, 0x012000c0,	REG,	0,		RD16,R0,R8,R12					},	//Signed 16-bit × 32-bit multiply, 32-bit accumulate SMLAWB, SMLAWT
	{"smulwb",	0x0ff000f0, 0x01200080,	REG,	0,		RD16,R0,R8,R12					},	//Signed 16-bit × 32-bit multiply, 32-bit result SMULWB, SMULWT
	{"smulwt",	0x0ff000f0, 0x012000c0,	REG,	0,		RD16,R0,R8,R12					},	//Signed 16-bit × 32-bit multiply, 32-bit result SMULWB, SMULWT
	{"smlalbb",	0x0ff000f0, 0x01400080,	REG,	0,		RD16,R0,R8,R12					},	//Signed 16-bit multiply, 64-bit accumulate SMLALBB, SMLALBT, SMLALTB, SMLALTT
	{"smlalbt",	0x0ff000f0, 0x014000a0,	REG,	0,		RD16,R0,R8,R12					},	//Signed 16-bit multiply, 64-bit accumulate SMLALBB, SMLALBT, SMLALTB, SMLALTT
	{"smlaltb",	0x0ff000f0, 0x014000c0,	REG,	0,		RD16,R0,R8,R12					},	//Signed 16-bit multiply, 64-bit accumulate SMLALBB, SMLALBT, SMLALTB, SMLALTT
	{"smlaltt",	0x0ff000f0, 0x014000e0,	REG,	0,		RD16,R0,R8,R12					},	//Signed 16-bit multiply, 64-bit accumulate SMLALBB, SMLALBT, SMLALTB, SMLALTT
	{"smulbb",	0x0ff000f0, 0x01600080,	REG,	0,		RD16,R0,R8,R12					},	//Signed 16-bit multiply, 32-bit result SMULBB, SMULBT, SMULTB, SMULTT
	{"smulbt",	0x0ff000f0, 0x016000a0,	REG,	0,		RD16,R0,R8,R12					},	//Signed 16-bit multiply, 32-bit result SMULBB, SMULBT, SMULTB, SMULTT
	{"smultb",	0x0ff000f0, 0x016000c0,	REG,	0,		RD16,R0,R8,R12					},	//Signed 16-bit multiply, 32-bit result SMULBB, SMULBT, SMULTB, SMULTT
	{"smultt",	0x0ff000f0, 0x016000e0,	REG,	0,		RD16,R0,R8,R12					},	//Signed 16-bit multiply, 32-bit result SMULBB, SMULBT, SMULTB, SMULTT
	0,
};

OP multiply[] = {
//	{"",		0x0f0000f0, 0x00000090,													}//multiply
	{"mul",		0x0fe000f0, 0x00000090, REG,	S20,	RD16,R0,R8						},
	{"mla",		0x0fe000f0, 0x00200090, REG,	S20,	RD16,R0,R8,R12					},
	{"umaal",	0x0ff000f0, 0x00400090, REG,	S20,	RD16,R0,R8,R12					},
	{"",		0x0ff000f0, 0x00500090, UNDEF											},
	{"mls",		0x0ff000f0, 0x00600090, REG,	0,		RD16,R0,R8						},
	{"",		0x0ff000f0, 0x00700090, UNDEF											},
	{"umull",	0x0fe000f0, 0x00800090, REG,	S20,	RD16,R0,R8						},
	{"umlal",	0x0fe000f0, 0x00a00090, REG,	S20,	RD16,R0,R8,R12					},
	{"smull",	0x0fe000f0, 0x00c00090, REG,	S20,	RD16,R0,R8						},
	{"smlal",	0x0fe000f0, 0x00e00090, REG,	S20,	RD16,R0,R8,R12					},
	0,
};


OP synch_prims[] = {
//	{"",		0x0f0000f0, 0x01000090,													}
	{"stl",		0x0ff003f0, 0x01800090,	REG,	0,		RD12,R16						},
	{"stlex",	0x0ff003f0, 0x01800290,	REG,	0,		RD12,R0,R16						},
	{"strex",	0x0ff003f0, 0x01800390,	REG,	0,		RD12,R0,R16						},

	{"lda",		0x0ff003f0, 0x01900090,	REG,	0,		RD12,R16						},
	{"ldaex",	0x0ff003f0, 0x01900290,	REG,	0,		RD12,R16						},
	{"ldrex",	0x0ff003f0, 0x01900390,	REG,	0,		RD12,R16						},

	{"stlexd",	0x0ff003f0, 0x01a00290,	REG,	0,		RD12,R0,R16						},
	{"strexd",	0x0ff003f0, 0x01a00390,	REG,	0,		RD12,R0,R16						},

	{"ldaexd",	0x0ff003f0, 0x01b00290,	REG,	0,		RD12,R16						},
	{"ldrexd",	0x0ff003f0, 0x01b00390,	REG,	0,		RD12,R16						},

	{"stlb",	0x0ff003f0, 0x01c00090,	REG,	0,		RD12,R16						},
	{"stlexb",	0x0ff003f0, 0x01c00290,	REG,	0,		RD12,R0,R16						},
	{"strexb",	0x0ff003f0, 0x01c00390,	REG,	0,		RD12,R0,R16						},

	{"ldab",	0x0ff003f0, 0x01d00090,	REG,	0,		RD12,R16						},
	{"ldaexb",	0x0ff003f0, 0x01d00290,	REG,	0,		RD12,R16						},
	{"ldrexb",	0x0ff003f0, 0x01d00390,	REG,	0,		RD12,R16						},

	{"stlh",	0x0ff003f0, 0x01e00090,	REG,	0,		RD12,R16						},
	{"stlexh",	0x0ff003f0, 0x01e00290,	REG,	0,		RD12,R0,R16						},
	{"strexh",	0x0ff003f0, 0x01e00390,	REG,	0,		RD12,R0,R16						},

	{"ldah",	0x0ff003f0, 0x01f00090,	REG,	0,		RD12,R16						},
	{"ldaexh",	0x0ff003f0, 0x01f00290,	REG,	0,		RD12,R16						},
	{"ldrexh",	0x0ff003f0, 0x01f00390,	REG,	0,		RD12,R16						},
	0,
};

OP extra_load_store[] = {
//	{"",		0x0f3000d0, 0x002000d0,													},
	{"strh",	0x0e1000f0, 0x000000b0,	ADDR8S,	I22,		RD12,						},
	{"ldrh",	0x0e1000f0, 0x001000b0,	ADDR8S,	I22,		RD12,						},

	{"ldrd",	0x0e1000f0, 0x000000d0,	ADDR8S,	I22,		RD12,						},
	{"ldrsb",	0x0e1000f0, 0x001000d0,	ADDR8S,	I22,		RD12,						},

	{"strd",	0x0e1000f0, 0x000000f0,	ADDR8S,	I22,		RD12,						},
	{"ldrsh",	0x0e1000f0, 0x001000f0,	ADDR8S,	I22,		RD12,						},

	{"strht",	0x0f3000f0, 0x002000b0,	ADDR8S,	I22,		RD12,						},
	{"ldrht",	0x0f3000f0, 0x003000b0,	ADDR8S,	I22,		RD12,						},
	{"ldrsbt",	0x0f3000f0, 0x003000d0,	ADDR8S,	I22,		RD12,						},
	{"ldrsht",	0x0f3000f0, 0x003000f0,	ADDR8S,	I22,		RD12,						},
	0,
};

OP msr_immediate_hints[] = {
//	{"",		0x0fb00000, 0x032000d0,													},
	{"nop",		0x0fff00ff, 0x03200000,													},
	{"yield",	0x0fff00ff, 0x03200001,													},
	{"wfe",		0x0fff00ff, 0x03200002,													},
	{"wfi",		0x0fff00ff, 0x03200003,													},
	{"sev",		0x0fff00ff, 0x03200004,													},
	{"sevl",	0x0fff00ff, 0x03200005,													},
	{"dbg",		0x0fff00f0, 0x032000f0,	IMM4											},

	{"msr",		0x0fff0000, 0x03240000,													},//imm app
	{"msr",		0x0ffb0000, 0x03280000,													},//imm app
	{"msr",		0x0ff30000, 0x03210000,													},//imm sys
	{"msr",		0x0ff20000, 0x03220000,													},//imm sys
	0,
};

OP alu_misc[] = {
	{(char*)miscellaneous,			0x0f900080, 0x01000000,		TABLE					},
	{(char*)halfword_multiply,		0x0f900090, 0x01000080,		TABLE					},
	{(char*)multiply,				0x0f0000f0, 0x00000090,		TABLE					},
	{(char*)synch_prims,			0x0f0000f0, 0x01000090,		TABLE					},
	{(char*)extra_load_store,		0x0f3000d0, 0x002000d0,		TABLE					},

	{"",		0x0ff00000, 0x030000d0,													},//16-bit immediate load
	{"",		0x0ff00000, 0x034000d0,													},//high halfword immediate load

	{(char*)msr_immediate_hints,	0x0fb00000, 0x032000d0,		TABLE					},

	{"and",		0x0d700000, 0x00000000, SHIFT,	S20,	RD12,	R16						},
	{"eor",		0x0d700000, 0x00020000, SHIFT,	S20,	RD12,	R16						},
	{"sub",		0x0d700000, 0x00040000, SHIFT,	S20,	RD12,	R16						},
	{"rsb",		0x0d700000, 0x00060000, SHIFT,	S20,	RD12,	R16						},
	{"add",		0x0d700000, 0x00080000, SHIFT,	S20,	RD12,	R16						},
	{"adc",		0x0d700000, 0x000a0000, SHIFT,	S20,	RD12,	R16						},
	{"sbc",		0x0d700000, 0x000c0000, SHIFT,	S20,	RD12,	R16						},
	{"rsc",		0x0d700000, 0x000e0000, SHIFT,	S20,	RD12,	R16						},
	{"tst",		0x0d700000, 0x00100000, SHIFT,	0,		0,		R16						},
	{"teq",		0x0d700000, 0x00120000, SHIFT,	0,		0,		R16						},
	{"cmp",		0x0d700000, 0x00140000, SHIFT,	0,		0,		R16						},
	{"cmn",		0x0d700000, 0x00160000, SHIFT,	0,		0,		R16						},
	{"orr",		0x0d700000, 0x00180000, SHIFT,	S20,	RD12,	R16						},
	{"mov",		0x0d700000, 0x001a0000, SHIFT,	S20,	RD12,							},
	{"bic",		0x0d700000, 0x001c0000, SHIFT,	S20,	RD12,	R16						},
	{"mvn",		0x0d700000, 0x001e0000, SHIFT,	S20,	RD12,							},
	0,
};

OP parallel_addsub[] = {
	{"sadd16",	0x0ff000f0, 0x06100010,	REG,	0,		RD12,R16,R0						},
	{"sasx",	0x0ff000f0, 0x06100030,	REG,	0,		RD12,R16,R0						},
	{"ssax",	0x0ff000f0, 0x06100050,	REG,	0,		RD12,R16,R0						},
	{"ssub16",	0x0ff000f0, 0x06100070,	REG,	0,		RD12,R16,R0						},
	{"sadd8",	0x0ff000f0, 0x06100090,	REG,	0,		RD12,R16,R0						},
	{"ssub8",	0x0ff000f0, 0x061000f0,	REG,	0,		RD12,R16,R0						},

	{"qadd16",	0x0ff000f0, 0x06200010,	REG,	0,		RD12,R16,R0						},
	{"qasx",	0x0ff000f0, 0x06200030,	REG,	0,		RD12,R16,R0						},
	{"qsax",	0x0ff000f0, 0x06200050,	REG,	0,		RD12,R16,R0						},
	{"qsub16",	0x0ff000f0, 0x06200070,	REG,	0,		RD12,R16,R0						},
	{"qadd8",	0x0ff000f0, 0x06200090,	REG,	0,		RD12,R16,R0						},
	{"qsub8",	0x0ff000f0, 0x062000f0,	REG,	0,		RD12,R16,R0						},

	{"shadd16",	0x0ff000f0, 0x06300010,	REG,	0,		RD12,R16,R0						},
	{"shasx",	0x0ff000f0, 0x06300030,	REG,	0,		RD12,R16,R0						},
	{"shsax",	0x0ff000f0, 0x06300050,	REG,	0,		RD12,R16,R0						},
	{"shsub16",	0x0ff000f0, 0x06300070,	REG,	0,		RD12,R16,R0						},
	{"shadd8",	0x0ff000f0, 0x06300090,	REG,	0,		RD12,R16,R0						},
	{"shsub8",	0x0ff000f0, 0x063000f0,	REG,	0,		RD12,R16,R0						},

	{"uadd16",	0x0ff000f0, 0x06500010,	REG,	0,		RD12,R16,R0						},
	{"uasx",	0x0ff000f0, 0x06500030,	REG,	0,		RD12,R16,R0						},
	{"usax",	0x0ff000f0, 0x06500050,	REG,	0,		RD12,R16,R0						},
	{"usub16",	0x0ff000f0, 0x06500070,	REG,	0,		RD12,R16,R0						},
	{"uadd8",	0x0ff000f0, 0x06500090,	REG,	0,		RD12,R16,R0						},
	{"usub8",	0x0ff000f0, 0x065000f0,	REG,	0,		RD12,R16,R0						},

	{"uqadd16",	0x0ff000f0, 0x06600010,	REG,	0,		RD12,R16,R0						},
	{"uqasx",	0x0ff000f0, 0x06600030,	REG,	0,		RD12,R16,R0						},
	{"uqsax",	0x0ff000f0, 0x06600050,	REG,	0,		RD12,R16,R0						},
	{"uqsub16",	0x0ff000f0, 0x06600070,	REG,	0,		RD12,R16,R0						},
	{"uqadd8",	0x0ff000f0, 0x06600090,	REG,	0,		RD12,R16,R0						},
	{"uqsub8",	0x0ff000f0, 0x066000f0,	REG,	0,		RD12,R16,R0						},

	{"uhadd16",	0x0ff000f0, 0x06700010,	REG,	0,		RD12,R16,R0						},
	{"uhasx",	0x0ff000f0, 0x06700030,	REG,	0,		RD12,R16,R0						},
	{"uhsax",	0x0ff000f0, 0x06700050,	REG,	0,		RD12,R16,R0						},
	{"uhsub16",	0x0ff000f0, 0x06700070,	REG,	0,		RD12,R16,R0						},
	{"uhadd8",	0x0ff000f0, 0x06700090,	REG,	0,		RD12,R16,R0						},
	{"uhsub8",	0x0ff000f0, 0x067000f0,	REG,	0,		RD12,R16,R0						},
	0,
};
OP packing[] = {
	0,
};
OP signed_mul_div[] = {
	0,
};

OP media[] = {
//	{"",		0x0e000010, 0x06000010,													},
	{(char*)parallel_addsub,		0x0fe00010, 0x06000010,		TABLE					},//parallel add/sub signed
	{(char*)packing,				0x0f800010, 0x06800010,		TABLE					},//pack/unpack
	{(char*)signed_mul_div,			0x0f800010, 0x07000010,		TABLE					},//signed mult, etc

	{"usad8",	0x0ff0f0f0, 0x0780f010,	REG,	0,		RD16,R0,R8						},//usad8
	{"usada8",	0x0ff000f0, 0x07800010,	REG,	0,		RD16,R0,R8,R12					},//usada8
	{"sbfx",	0x0fe00070, 0x07a00050,	REG,	0,		RD16,R16,IMM5_7,IMM5_16_M1		},//sbfx
	{"bfc",		0x0fe0007f, 0x07c0001f,	REG,	0,		RD16,IMM5_7,IMM5_16				},//bfc
	{"bfi",		0x0fe00070, 0x07c00010,	REG,	0,		RD16,R16,IMM5_7,IMM5_16			},//bfi
	{"ubfx",	0x0fe00070, 0x07e00050,	REG,	0,		RD16,R16,IMM5_7,IMM5_16_M1		},//ubfx
	{"",		0x0ff000f0, 0x07f000f0,	UNDEF											},//udf
	0,
};

OP DisassemblerARM::ops[] = {
	{(char*)unconditional,			0xf0000000, 0xf0000000,		TABLE					},
	{(char*)alu_misc,				0x0c000000, 0x00000000,		TABLE					},
	{(char*)media,					0x0e000010, 0x06000010,		TABLE					},

	{"swp",		0x0ff00ff0,	0x01000090, REG,	0,		RD12,R0, R16I					},
	{"swpb",	0x0ff00ff0,	0x01400090, REG,	0,		RD12,R0, R16I					},

	{"str",		0x0c500000, 0x04000000, ADDR12,	0,		RD12,							},
	{"ldrt",	0x0d700000, 0x04300000, ADDR12,	0,		RD12,							},
	{"ldr",		0x0c500000, 0x04100000, ADDR12,	0,		RD12,							},
	{"strb",	0x0c500000, 0x04400000, ADDR12,	0,		RD12,							},
	{"ldrbt",	0x0d700000, 0x04700000, ADDR12,	0,		RD12,							},
	{"ldrb",	0x0c500000, 0x04500000, ADDR12,	0,		RD12,							},

	{"ldm",		0x0e100000, 0x08100000, AUTO,	S22|LIST,RD16							},
	{"stm",		0x0e100000, 0x08000000, AUTO,	S22|LIST,RD16							},
	{"b",		0x0f000000, 0x0a000000, BRANCH											},
	{"bl",		0x0f000000, 0x0b000000, BRANCH											},

//coprocessor stuff
	{"stc",		0x0e300000, 0x0c000000, ADDR8,	COP1,	RD12							},
	{"ldc",		0x0e300000, 0x0c100000, ADDR8,	COP1,	RD12							},
	{"stcl",	0x0e300000, 0x0c200000, ADDR8,	COP1,	RD12							},
	{"ldcl",	0x0e300000, 0x0c300000, ADDR8,	COP1,	RD12							},

	{(char*)floating_point,			0x0f000e10, 0x0e000a00,		TABLE					},

	{"cdp",		0x0f000010, 0x0e000000, REG,	COP1,	RD12,R16,IMM3_5					},
	{"mrc",		0x0f100010, 0x0e100010, REG,	COP1,	RD12,R16,IMM3_5					},
	{"mcr",		0x0f100010, 0x0e000010, REG,	COP1,	RD12,R16,IMM3_5					},
	{"swi",		0x0f000000, 0x0f000000, IMM24											},
	0,
};

//-----------------------------------------------------------------------------
//	DisassemblerARMThumb
//-----------------------------------------------------------------------------

class DisassemblerARMThumb : public DisassemblerARMbase {
	union opcode16 {//0				//4		//8			12
		struct { uint16 r0:3,	r3:3,	r6:3,	:7;			};
		struct { uint16 :6,			shift:5, shiftop:2, :3;	};
		struct { uint16 imm8:8,				r8:4,	:4;		};
		struct { uint16 list:8,		listlr:1,	:7;			};
		struct { uint16 offset8:8,			:8;				};
		uint16	u;
		opcode16(uint16 _u) : u(_u) {}

		uint8	get_r0(uint8 flags) const {
			return (u & 7) | (flags & THI ? ((u & (1 << 7)) >> 4) : 0);
		}
		uint8	get_r3(uint8 flags) const {
			return (u >> 3) & (flags & THI ? 15 : 7);
		}
		uint8	get_r6(uint8 flags) const {
			return (u >> 6) & 7;
		}
	};

	union opcodethumb32 {//0		12					//16
		struct constant_t	{ uint32 imm8:8,	:4,	mod:3,				:11,		i:1, :5;
			operator uint32() const {
				if (i == 0) switch (mod) {
					case 0: return imm8;
					case 1: return imm8 * 0x00010001;
					case 2: return imm8 * 0x01000100;
					case 3: return imm8 * 0x01010101;
				}
				uint8	shift = (mod << 1) + (imm8 >> 7) + (i << 4);
				return (imm8 | 0x80) << (32 - shift);
			}
		} constant;
		struct branch_t		{ uint32 imm11:11,	j2:1, :1, j1:1, :2, imm10:10,	s:1, :5;
			operator uint32() const {
				return ((j1 ^ s) << 23) | ((j2 ^ s) << 22) | (imm10 << 12) | (imm11 << 1);
			}
		} branch;

		uint32	u;
		opcodethumb32(uint32 _u) : u(_u) {}
	};

	static OP	ops32[], ops_thumb[];
public:
	virtual	const char*	GetDescription() { return "ARM(thumb)"; }
	virtual State*		Disassemble(const iso::memory_block &block, uint64 addr, SymbolFinder sym_finder) {
		StateDefault	*state = new StateDefault;

		for (uint16 *p = block, *end = (uint16*)block.end(); p < end; ) {
			uint32	offset		= uint32(addr + (uint8*)p - (uint8*)block);
			const char *const *s;

			uint64			sym_addr;
			string_param	sym_name;
			if (sym_finder && sym_finder(offset, sym_addr, sym_name) && sym_addr == offset)
				state->lines.push_back(sym_name);

			uint32	u = *p++;
			buffer_accum<1024>	ba("%08x ", offset);
			ba.format("%04x ", u);

			if ((u >> 11) > 0x1c) {
				u = (u << 16) | *p;
				ba.format("%04x ", *p++);
				opcodethumb32	op	= u;
				OP			*op2	= get_op(ops32, u);
				OP::SPEC	spec	= op2->spec;
				MODE		mode	= MODE(spec.mode);
				uint8		flags	= spec.flags;
				REG_MODE	rm		= RM_GENERAL;

				ba << op2->name << ' ';

				const char *sep = ", ";
				switch (DEST d = DEST(spec.dest)) {
					default:			sep = "";
				}

				for (int i = 0; i < 3; i++) {
					if (SOURCE s = spec.get_source(i)) {
						ba << sep;
						sep = ", ";
						switch (s) {
							default:		ISO_ASSERT("uhoh");
						}
					}
				}

				switch (mode) {
					case T32BRANCH:
						put_address(ba, offset + 4 + op.branch, sym_finder);
						break;
				}

			} else {
				ba << "     ";

				opcode16	op		= u;
				OP			*op2	= get_op(ops_thumb, u);
				OP::SPEC	spec	= op2->spec;
				MODE		mode	= MODE(spec.mode);
				uint8		flags	= spec.flags;
				REG_MODE	rm		= RM_GENERAL;

				ba << op2->name << ' ';

				const char *sep = ", ";
				switch (DEST d = DEST(spec.dest)) {
					case TRD0:			put_reg(ba, op.get_r0(flags), rm);		break;
					case TRD8:			put_reg(ba, op.r8, rm);					break;
					default:			sep = "";
				}

				for (int i = 0; i < 3; i++) {
					if (SOURCE s = spec.get_source(i)) {
						ba << sep;
						sep = ", ";
						switch (s) {
							case TR3:		put_reg(ba, op.get_r3(flags), rm);		break;
							case TR6:		put_reg(ba, op.get_r6(flags), rm);		break;

							case TIMM8:		put_imm<4>(ba, op.u);		break;
							case TIMM6_3:	put_imm<5>(ba, op.u);		break;

							case TLIST:		put_list(ba, op.list | (op.listlr ? (1 << LR) : 0));	break;
							default:		ISO_ASSERT("uhoh");
						}
					}
				}
				if (flags & TSHIFT)
					put_shift(ba, op.shift, SHIFTOP(op.shiftop));
			}

			state->lines.push_back((const char*)ba);
		}
		return state;
	}

} dis_arm_thumb;

OP	thumb_shifts[] = {//0xc000, 0x0000
	{"mov",		0x3000, 0x0000,	THUMB,	TSHIFT,	TRD0,	TR3,			},
	{"mov",		0x3800, 0x1000,	THUMB,	TSHIFT,	TRD0,	TR3,			},
	{"add",		0x3e00, 0x1800,	THUMB,	0,		TRD0,	TR3,	TR6,	},
	{"sub",		0x3e00, 0x1a00,	THUMB,	0,		TRD0,	TR3,	TR6,	},
	{"add",		0x3e00, 0x1c00,	THUMB,	0,		TRD0,	TR3,	TIMM6_3,},
	{"sub",		0x3e00, 0x1e00,	THUMB,	0,		TRD0,	TR3,	TIMM6_3,},
	{"mov",		0x3800, 0x2000,	THUMB,	0,		TRD0,	TIMM8,			},
	{"cmp",		0x3800, 0x2800,	THUMB,	0,		0,		TR0,	TIMM8,	},
	{"add",		0x3800, 0x3000,	THUMB,	0,		TRD0,	TIMM8,			},
	{"sub",		0x3800, 0x3800,	THUMB,	0,		TRD0,	TIMM8,			},
0};
OP	thumb_data[] = {//0xfc00, 0x4000
	{"and",		0x03c0, 0x0000,	THUMB,	0,		TRD0,	TR3,			},
	{"eor",		0x03c0, 0x0040,	THUMB,	0,		TRD0,	TR3,			},
	{"mov",		0x0380, 0x0080,	THUMB,	TSHIFT,	TRD0,	TR3,			},
	{"mov",		0x03c0, 0x0100,	THUMB,	TSHIFT,	TRD0,	TR3,			},
	{"adc",		0x03c0, 0x0140,	THUMB,	0,		TRD0,	TR3,			},
	{"sbc",		0x03c0, 0x0180,	THUMB,	0,		TRD0,	TR3,			},
	{"mov",		0x03c0, 0x01c0,	THUMB,	TSHIFT,	TRD0,	TR3,			},
	{"tst",		0x03c0, 0x0200,	THUMB,	0,		0,		TR0,	TR3,	},
	{"rsb",		0x03c0, 0x0240,	THUMB,	0,		TRD0,	TR3,			},
	{"cmp",		0x03c0, 0x0280,	THUMB,	0,		0,		TR0,	TR3,	},
	{"cmn",		0x03c0, 0x02c0,	THUMB,	0,		0,		TR0,	TR3,	},
	{"orr",		0x03c0, 0x0300,	THUMB,	0,		TRD0,	TR3,			},
	{"mul",		0x03c0, 0x0340,	THUMB,	0,		TRD0,	TR3,			},
	{"bic",		0x03c0, 0x0380,	THUMB,	0,		TRD0,	TR3,			},
	{"mvn",		0x03c0, 0x03c0,	THUMB,	0,		TRD0,	TR3,			},
0};
OP	thumb_special[] = {//0xfc00, 0x4400
	{"add",		0x0300, 0x0000, THUMB,	THILO,	TRD0, 	TR3				},
	{"cmp",		0x0300, 0x0100, THUMB,	THI,	0,		TR0, TR3		},
	{"mov",		0x03c0, 0x0200, THUMB,	THI,	TRD0,	TR3,			},
	{"mov",		0x03c0, 0x0240, THUMB,	THI,	TRD0,	TR3,			},
	{"mov",		0x0380, 0x0280, THUMB,	THI,	TRD0,	TR3,			},
	{"bx",		0x0380, 0x0300, THUMB,	0,		0,		TR3,			},
	{"blx",		0x0380, 0x0380, THUMB,	0,		0,		TR3,			},
0};
OP	thumb_loadstore1[] = {//0xf000, 0x5000 + 0xe000, 0x6000 + 0xe000, 0x8000
	{"str",		0xfe00,	0x5000, THUMB,	0,		TRD0,	TR3,	TR6		},
	{"strh",	0xfe00,	0x5200, THUMB,	0,		TRD0,	TR3,	TR6		},
	{"strb",	0xfe00,	0x5400, THUMB,	0,		TRD0,	TR3,	TR6		},
	{"ldrsb",	0xfe00,	0x5600, THUMB,	0,		TRD0,	TR3,	TR6		},
	{"ldr",		0xfe00,	0x5800, THUMB,	0,		TRD0,	TR3,	TR6		},
	{"ldrh",	0xfe00,	0x5a00, THUMB,	0,		TRD0,	TR3,	TR6		},
	{"ldrb",	0xfe00,	0x5c00, THUMB,	0,		TRD0,	TR3,	TR6		},
	{"ldrsh",	0xfe00,	0x5e00, THUMB,	0,		TRD0,	TR3,	TR6		},
	{"str",		0xf800,	0x6000, THUMB,	0,		TRD0,	TR3,	TIMM6_5	},
	{"ldr",		0xf800,	0x6800, THUMB,	0,		TRD0,	TR3,	TIMM6_5	},
	{"strb",	0xf800,	0x7000, THUMB,	0,		TRD0,	TR3,	TIMM6_5	},
	{"ldrb",	0xf800,	0x7800, THUMB,	0,		TRD0,	TR3,	TIMM6_5	},
	{"strh",	0xf800,	0x8000, THUMB,	0,		TRD0,	TR3,	TIMM6_5	},
	{"ldrh",	0xf800,	0x8800, THUMB,	0,		TRD0,	TR3,	TIMM6_5	},
	{"str",		0xf800,	0x9000, THUMB,	0,		TRD8,	TIMM8			},
	{"ldr",		0xf800,	0x9800, THUMB,	0,		TRD8,	TIMM8			},
0};
OP	thumb_misc16[] = {//0xf000, 0xb000
	{"add",		0x0f80, 0x0000,	THUMB,	TSP,	0,		TIMM7			},
	{"sub",		0x0f80, 0x0000,	THUMB,	TSP,	0,		TIMM7			},
	{"cbz",		0x0f00, 0x0100,	TBRANCH,0,		0,		TIMM3_5			},
	{"sxth",	0x0fc0, 0x0200,	THUMB,	0,		TRD0,	TR3				},
	{"sxtb",	0x0fc0, 0x0200,	THUMB,	0,		TRD0,	TR3				},
	{"uxth",	0x0fc0, 0x0200,	THUMB,	0,		TRD0,	TR3				},
	{"uxtb",	0x0fc0, 0x0200,	THUMB,	0,		TRD0,	TR3				},
	{"cbz",		0x0f00, 0x0300,	TBRANCH,0,		0,		TIMM3_5			},
	{"push",	0x0e00, 0x0400,	THUMB,	0,		0,		TLIST			},
	{"setend 0",0x0fe8, 0x0600,	THUMB									},
	{"setend 1",0x0fe8, 0x0608,	THUMB									},
	{"cps",		0x0fe0, 0x0600,	THUMB,	0,								},
	{"cbnz",	0x0f00, 0x0900,	TBRANCH,0,		0,		TIMM3_5			},
	{"rev",		0x0fc0, 0x0a00,	THUMB,	0,		TRD0,	TR3				},
	{"rev16",	0x0fc0, 0x0a00,	THUMB,	0,		TRD0,	TR3				},
	{"hlt",		0x0fc0, 0x0a00,	THUMB,	0,		0,		TIMM7			},
	{"revsh",	0x0fc0, 0x0a00,	THUMB,	0,		TRD0,	TR3				},
	{"cbnz",	0x0f00, 0x0b00,	TBRANCH,0,		0,		TIMM3_5			},
	{"pop",		0x0e00, 0x0c00,	THUMB,	0,		0,		TLIST,			},
	{"bkpt",	0x0f00, 0x0e00,	THUMB,	0,		0,		TIMM8			},
	{"<hints>",	0x0f00, 0x0f00,	THUMB,	0,								},
0};

OP	DisassemblerARMThumb::ops_thumb[] = {
	{(char*)thumb_shifts,		0xc000, 0x0000,	TABLE	 						},
	{(char*)thumb_data,			0xfc00, 0x4000,	TABLE	 						},
	{(char*)thumb_special,		0xfc00, 0x4400,	TABLE	 						},
	{"ldr",						0xf800, 0x4800,	THUMB,	0,		TRD8, TIMM8		},
	{(char*)thumb_loadstore1,	0xf000, 0x5000,	TABLE							},
	{(char*)thumb_loadstore1,	0xe000, 0x6000,	TABLE							},
	{(char*)thumb_loadstore1,	0xe000, 0x8000,	TABLE							},
	{"adr",						0xf800, 0xa000,	THUMB,	0,		TRD8, TIMM8		},
	{"add",						0xf800, 0xa800,	THUMB,	0,		TRD8, TSP, TIMM8},
	{(char*)thumb_misc16,		0xf000, 0xb000,	TABLE							},
	{"stm",						0xf800, 0xc000,	AUTO,	LIST,	TRD8			},
	{"ldm",						0xf800, 0xc800,	AUTO,	LIST,	TRD8			},
	{"b",						0xf800, 0xe000,	TBRANCH							},
	{"svc",						0xff00, 0xdf00,	TBRANCH							},
	{"udf",						0xff00, 0xde00,	TBRANCH							},
	{"b<cond>",					0xf000, 0xd000,	TBRANCH							},
0};

OP	thumb32_multiple[] = {//0x1e400000, 0x00000000
/*
	00	0 -			Store Return State SRS, SRSDA, SRSDB, SRSIA, SRSIB on
		1 -			Return From Exception RFE, RFEDA, RFEDB, RFEIA, RFEIB on
	01	0 -			Store Multiple (Increment After, Empty Ascending) STM, STMIA, STMEA on page F6-3031
		1 not 11101 Load Multiple (Increment After, Full Descending)a LDM, LDMIA, LDMFD on page F6-2701a
	10	0 not 11101 Store Multiple (Decrement Before, Full Descending)b STMDB, STMFD on page F6-3037b
		1 -			Load Multiple (Decrement Before, Empty Ascending) LDMDB, LDMEA on page F6-2710
	11	0 -			Store Return State SRS, SRSDA, SRSDB, SRSIA, SRSIB on
		1 -			Return From Exception RFE, RFEDA, RFEDB, RFEIA, RFEIB on
*/
	"multiple", 0};
OP	thumb32_loadstore[] = {//0x1e400000, 0x00400000
/*
00	00	-		- Store Register Exclusive STREX on page F6-3060
	01	-		- Load Register Exclusive LDREX on page F6-2739
0x	10	-		- Store Register Dual STRD (immediate) on page F6-3055
1x	x0	-		-
0x	11	-		not 1111 Load Register Dual (immediate) LDRD (immediate) on page F6-2732
1x	x1	-		not 1111
0x	11	-		1111 Load Register Dual (literal) LDRD (literal) on page F6-2735
1x	x1	-		1111
01	00	0100	- Store Register Exclusive Byte STREXB on page F6-3062
		0101	- Store Register Exclusive Halfword STREXH on page F6-3066
		0111	- Store Register Exclusive Doubleword STREXD on page F6-3064
		1000	- Store-Release Byte STLB on page F6-3021
		1001	- Store-Release Halfword STLH on page F6-3030
		1010	- Store-Release Word STL on page F6-3020
		1100	- Store-Release Exclusive Byte STLEXB on page F6-3024
		1101	- Store-Release Exclusive Halfword STLEXH on page F6-3028
		1110	- Store-Release Exclusive Word STLEX on page F6-3022
		1111	- Store-Release Exclusive Doubleword STLEXD on page F6-3026
01	01	0000	- Table Branch Byte TBB, TBH on page F6-3108
		0001	- Table Branch Halfword TBB, TBH on page F6-3108
		0100	- Load Register Exclusive Byte LDREXB on page F6-2741
		0101	- Load Register Exclusive Halfword LDREXH on page F6-2745
		0111	- Load Register Exclusive Doubleword LDREXD on page F6-2743
		1000	- Load-Acquire Byte LDAB on page F6-2683
		1001	- Load-Acquire Halfword LDAH on page F6-2692
		1010	- Load-Acquire Word LDA on page F6-2682
		1100	- Load-Acquire Exclusive Byte LDAEXB on page F6-2686
		1101	- Load-Acquire Exclusive Halfword LDAEXH on page F6-2690
		1110	- Load-Acquire Exclusive Word LDAEX on page F6-2684
		1111	- Load-Acquire Exclusive Doubleword LDAEXD on page F6-2688
*/
	"loadstore", 0};
OP	thumb32_data[] = {//0x1e000000, 0x02000000
/*
0000 - not 11111 Bitwise AND AND, ANDS (register) on page F6-2597
		11111 Test TST (register) on page F6-3116
0001 - - Bitwise Bit Clear BIC, BICS (register) on page F6-2620
0010 not 1111 - Bitwise OR ORR, ORRS (register) on page F6-2849
		1111 - Movea MOV, MOVS (register) on page F6-2804
0011 not 1111 - Bitwise OR NOT ORN, ORNS (register) on page F6-2845
		1111 - Bitwise NOT MVN, MVNS (register) on page F6-2837
0100 - not 11111 Bitwise Exclusive OR EOR, EORS (register) on page F6-2667
		11111 Test Equivalence TEQ (register) on page F6-3111
0110 - - Pack Halfword PKHBT, PKHTB on page F6-2855
1000 - not 11111 Add ADD, ADDS (register) on page F6-2577
		11111 Compare Negative CMN (register) on page F6-2641
1010 - - Add with Carry ADC, ADCS (register) on page F6-2567
1011 - - Subtract with Carry SBC, SBCS (register) on page F6-2941
1101 - not 11111 Subtract SUB, SUBS (register) on page F6-3083
		11111 Compare CMP (register) on page F6-2646
1110 - - Reverse Subtract RSB, RSBS (register) on page F6-2922
*/
	"data", 0};
OP	thumb32_cop[] = {//0x1c000000, 0x04000000
/*
-		00000x - - UNDEFINED -
		11xxxx - - Advanced SIMD Advanced SIMD data-processing instructions on
not 101x 0xxxx0
		not 000x0x	- - Store Coprocessor STC, STC2 on page F6-3016
		0xxxx1
		not 000x0x	- not 1111 Load Coprocessor (immediate) LDC, LDC2 (immediate) on page F6-2694
						1111	Load Coprocessor (literal) LDC, LDC2 (literal) on page F6-2698
		000100 - - Move to Coprocessor from two general-purpose registers
		000101 - - Move to two general-purpose registers from Coprocessor
		10xxxx 0 - Coprocessor data operations CDP, CDP2 on page F6-2635
		10xxx0 1 - Move to Coprocessor from general-purpose register
		10xxx1 1 - Move to general-purpose register from Coprocessor
101x	0xxxxx	- - Advanced SIMD, floating-point load/store instructions
		not 000x0x
		00010x - - Advanced SIMD, floating-point 64-bit transfers accessing the SIMD and floating-point register file on page F5-2561
		10xxxx 0 - Floating-point data processing Floating-point data-processing instructions on
		10xxxx 1 - Advanced SIMD, floating-point 8, 16, and 32-bit transfers accessing the SIMD
*/
	"cop", 0};
OP	thumb32_data_imod[] = {//0x1a008000, 0x10000000
/*
0000 -		not 11111	Bitwise AND AND, ANDS (immediate) on page F6-2595
			11111		Test TST (immediate) on page F6-3114
0001 -			-		Bitwise Bit Clear BIC, BICS (immediate) on page F6-2618
0010 not 1111	-		Bitwise OR ORR, ORRS (immediate) on page F6-2847
		1111	-		Move MOV, MOVS (immediate) on page F6-2801
0011 not 1111	-		Bitwise OR NOT ORN, ORNS (immediate) on page F6-2844
		1111	-		Bitwise NOT MVN, MVNS (immediate) on page F6-2835
0100 -		not 11111	Bitwise Exclusive OR EOR, EORS (immediate) on page F6-2665
				11111	Test Equivalence TEQ (immediate) on page F6-3109
1000 -		not 11111	Add ADD, ADDS (immediate) on page F6-2573
			11111		Compare Negative CMN (immediate) on page F6-2639
1010 -		-			Add with Carry ADC, ADCS (immediate) on page F6-2565
1011 -		-			Subtract with Carry SBC, SBCS (immediate) on page F6-2939
1101 -		not 11111	Subtract SUB, SUBS (immediate) on page F6-3079
			11111		Compare CMP (immediate) on page F6-2644
1110 -		-			Reverse Subtract RSB, RSBS (immediate) on page F6-2919
*/
	"data_imod", 0};
OP	thumb32_data_imm[] = {//0x1a008000, 0x12000000
/*
00000	not 1111	Add Wide (12-bit) ADD, ADDS (immediate) on page F6-2573
		1111		Form PC-relative Address ADR on page F6-2592
00100	-			Move Wide (16-bit) MOV, MOVS (immediate) on page F6-2801
01010	not 1111	Subtract Wide (12-bit) SUB, SUBS (immediate) on page F6-3079
		1111		Form PC-relative Address ADR on page F6-2592
01100	-			Move Top (16-bit) MOVT on page F6-2814
10000
10010	-			Signed Saturate SSAT on page F6-3006
10010	-			Signed Saturate, two 16-bit SSAT16 on page F6-3008
10100	-			Signed BitField Extract SBFX on page F6-2946
10110	not 1111	BitField Insert BFI on page F6-2616
		1111		BitField Clear BFC on page F6-2614
11000
11010	-			Unsigned Saturate USAT on page F6-3165
11010	-			Unsigned Saturate, two 16-bit USAT16 on page F6-3167
11100	-			Unsigned BitField Extract UBFX on page F6-3125
*/
	"data_imm", 0};
OP	thumb32_control[] = {//0x18389000, 0x10388000
/*
0x0 xx1xxxxx	011100x -	 -		Move to Banked or Special-purpose register
	xx0xxxxx	0111000 xx00 -		Move to Special-purpose register, Application level
						xx01 -		Move to Special-purpose register, System level
						xx1x
				0111001 -	 -		Move to Special-purpose register, System level
	-			0111010 -	 -		Change PE State
	-			0111011 -	 -		Miscellaneous control instructions on
	-			0111100 -	 -		Branch and Exchange Jazelle
	00000000	0111101 -	 -		Exception Return ERET on page F6-2673
	not
	00000000	0111101 -	 -		Exception Return SUB, SUBS (immediate) on
	xx1xxxxx	011111x -	 -		Move from Banked or Special-purpose register
	xx0xxxxx	0111110 -	 -		Move from Special-purpose register, Application level
				0111111 -	 -		Move from Special-purpose register, System level
000 000000xx	1111000 0000 1111	Debug Change PE State DCPS1, DCPS2, DCPS3 on
				1111110 -	 -		Hypervisor Call HVC on page F6-2676
				1111111 -	 -		Secure Monitor Call SMC on page F6-2969
010 -			1111111 -	-		Permanently UNDEFINED UDF on page F6-3127
*/
"branch", 0};
OP	thumb32_store1[] = {//0x1f100000, 0x18000000
/*
000 1xx1xx Store Register Byte STRB (immediate) on page F6-3048
	1100xx
100 -
000 000000 Store Register Byte STRB (register) on page F6-3051
	1110xx Store Register Byte Unprivileged STRBT on page F6-3053
001 1xx1xx Store Register Halfword STRH (immediate) on page F6-3068
	1100xx
101 -
001 000000 Store Register Halfword STRH (register) on page F6-3071
	1110xx Store Register Halfword Unprivileged STRHT on page F6-3073
010 1xx1xx Store Register STR (immediate) on page F6-3041
	1100xx
110 -
010 000000 Store Register STR (register) on page F6-3045
	1110xx Store Register Unprivileged STRT on page F6-3075
*/
	"store1", 0};
OP	thumb32_loadbyte[] = {//0x1f700000, 0x18100000
	/*
00	000000	not 1111	not 1111	Load Register Byte LDRB (register) on page F6-2728
						1111		Preload Data PLD, PLDW (register) on page F6-2861
0x	-		1111		not 1111	Load Register Byte LDRB (immediate) on page F6-2723
						1111		Preload Data PLD (literal) on page F6-2859
00	1xx1xx	not 1111	-			Load Register Byte LDRB (immediate) on page F6-2723
	1100xx	not 1111	not 1111	Load Register Byte
						1111		Preload Data PLD, PLDW (register) on page F6-2861
	1110xx	not 1111	-			Load Register Byte Unprivileged LDRBT on page F6-2730
01	-		not 1111	not 1111	Load Register Byte LDRB (immediate) on page F6-2723
						1111		Preload Data PLD, PLDW (immediate) on page F6-2857
10	000000	not 1111	not 1111	Load Register Signed Byte LDRSB (register) on page F6-2761
						1111		Preload Instruction PLI (register) on page F6-2866
1x	-		1111		not 1111	Load Register Signed Byte LDRSB (literal) on page F6-2759
						1111		Preload Instruction PLI (immediate, literal) on page F6-2863
10	1xx1xx	not 1111	-			Load Register Signed Byte LDRSB (immediate) on page F6-2756
	1100xx	not 1111	not 1111	Load Register Signed Byte LDRSB (immediate) on page F6-2756
						1111		Preload Instruction PLI (immediate, literal) on page F6-2863
	1110xx	not 1111	-			Load Register Signed Byte Unprivileged LDRSBT on page F6-2763
11	-		not 1111	not 1111	Load Register Signed Byte LDRSB (immediate) on page F6-2756
						1111		Preload Instruction PLI (immediate, literal) on page F6-2863
*/
	"loadbyte", 0};
OP	thumb32_loafhalf[] = {//0x1f700000, 0x18300000
/*
0x	-		1111		not 1111	Load Register Halfword LDRH (literal) on page F6-2750
							1111	Preload Data PLD (literal) on page F6-2859
00	1xx1xx	not 1111	-			Load Register Halfword LDRH (immediate) on page F6-2747
	1100xx	not 1111	not 1111
01	-		not 1111	not 1111
00	000000	not 1111	not 1111	Load Register Halfword LDRH (register) on page F6-2752
	1110xx	not 1111	-			Load Register Halfword Unprivileged LDRHT on page F6-2754
	000000	not 1111	1111		Preload Data with intent to Write PLD, PLDW (register) on page F6-2861
	1100xx	not 1111	1111		Preload Data with intent to Write PLD, PLDW (immediate) on page F6-2857
01	-		not 1111	1111
10	1xx1xx	not 1111	-			Load Register Signed Halfword LDRSH (immediate) on page F6-2765
	1100xx	not 1111	not 1111
11	-		not 1111	not 1111
1x	-		1111		not 1111	Load Register Signed Halfword LDRSH (literal) on page F6-2768
10	000000	not 1111	not 1111	Load Register Signed Halfword LDRSH (register) on page F6-2770
	1110xx	not 1111	-			Load Register Signed Halfword Unprivileged LDRSHT on page F6-2772
10	000000	not 1111	1111		Unallocated memory hint (treat as NOP) -
	1100xx	not 1111	1111
1x	-		1111		1111
11	-		not 1111	1111		Unallocated memory hint (treat as NOP)
*/
	"loafhalf", 0};
OP	thumb32_loadword[] = {//0x1f700000, 0x18500000
/*
00	000000	not 1111	Load Register LDR (register) on page F6-2720
00	1xx1xx	not 1111	Load Register LDR (immediate) on page F6-2714
	1100xx	not 1111
01	-		not 1111
00	1110xx	not 1111	Load Register Unprivileged LDRT on page F6-2774
0x	-		1111		Load Register LDR (literal) on page F6-2718
	*/
	"loadword", 0};
OP	thumb32_undefined[] = {//0x1f700000, 0x18700000
	"undefined", 0
};
OP	thumb32_simd[] = {//0x1f100000, 0x18000000
	/*
0	0010
	011x
	1010	Vector Store VST1 (multiple single elements) on page F7-3678
	0011
	100x	Vector Store VST2 (multiple 2-element structures) on page F7-3684
	010x	Vector Store VST3 (multiple 3-element structures) on page F7-3690
	000x	Vector Store VST4 (multiple 4-element structures) on page F7-3696
1	0x00
	1000	Vector Store VST1 (single element from one lane) on page F7-3675
	0x01
	1001	Vector Store VST2 (single 2-element structure from one lane) on page F7-3681
	0x10
	1010	Vector Store VST3 (single 3-element structure from one lane) on page F7-3687
	0x11
	1011	Vector Store VST4 (single 4-element structure from one lane) on page F7-3693
0	0010
	011x
	1010	Vector Load VLD1 (multiple single elements) on page F7-3390
	0011
	100x	Vector Load VLD2 (multiple 2-element structures) on page F7-3399
	010x	Vector Load VLD3 (multiple 3-element structures) on page F7-3408
	000x	Vector Load VLD4 (multiple 4-element structures) on page F7-3417
1	0x00
	1000	Vector Load VLD1 (single element to one lane) on page F7-3384
	1100	Vector Load VLD1 (single element to all lanes) on page F7-3387
	0x01
	1001	Vector Load VLD2 (single 2-element structure to one lane) on page F7-3393
	1101	Vector Load VLD2 (single 2-element structure to all lanes) on page F7-3396
	0x10
	1010	Vector Load VLD3 (single 3-element structure to one lane) on page F7-3402
	1110	Vector Load VLD3 (single 3-element structure to all lanes) on page F7-3405
	0x11
	1011	Vector Load VLD4 (single 4-element structure to one lane) on page F7-3411
	1111	Vector Load VLD4 (single 4-element structure to all lanes) on page F7-3414*/
	"simd", 0};
OP	thumb32_data_reg[] = {//0x1f000000, 0x18000000
	/*
000x	0000	-			Movea MOV, MOVS (register-shifted register) on page F6-2810
0000	1xxx	not 1111	Signed Extend and Add Halfword SXTAH on page F6-3100
				1111		Signed Extend Halfword SXTH on page F6-3106
0001	1xxx	not 1111	Unsigned Extend and Add Halfword UXTAH on page F6-3179
				1111		Unsigned Extend Halfword UXTH on page F6-3185
0010	1xxx	not 1111	Signed Extend and Add Byte 16-bit SXTAB16 on page F6-3098
				1111		Signed Extend Byte 16-bit SXTB16 on page F6-3104
0011	1xxx	not 1111	Unsigned Extend and Add Byte 16-bit UXTAB16 on page F6-3177
				1111		Unsigned Extend Byte 16-bit UXTB16 on page F6-3183
0100	1xxx	not 1111	Signed Extend and Add Byte SXTAB on page F6-3096
				1111		Signed Extend Byte SXTB on page F6-3102
0101	1xxx	not 1111	Unsigned Extend and Add Byte UXTAB on page F6-3175
				1111		Unsigned Extend Byte UXTB on page F6-3181
1xxx	00xx	-			Parallel addition and subtraction, signed on page F3-2498
		01xx	-			Parallel addition and subtraction, unsigned on page F3-2499
		10xx	-			Miscellaneous operations on page F3-2500
*/
	"data_reg", 0};
OP	thumb32_mult[] = {//0x1f800000, 0x18000000
/*
000		00	not 1111	Multiply Accumulate MLA, MLAS on page F6-2797
			1111		Multiply MUL, MULS on page F6-2833
		01	-			Multiply and Subtract MLS on page F6-2799
001		-	not 1111	Signed Multiply Accumulate (Halfwords) SMLABB, SMLABT, SMLATB, SMLATT on
			1111		Signed Multiply (Halfwords) SMULBB, SMULBT, SMULTB, SMULTT on
010		0x	not 1111	Signed Multiply Accumulate Dual SMLAD, SMLADX on page F6-2973
			1111		Signed Dual Multiply Add SMUAD, SMUADX on page F6-2993
011		0x	not 1111	Signed Multiply Accumulate (Word by halfword) SMLAWB, SMLAWT on page F6-2981
			1111		Signed Multiply (Word by halfword) SMULWB, SMULWT on page F6-2999
100		0x	not 1111	Signed Multiply Subtract Dual SMLSD, SMLSDX on page F6-2983
			1111		Signed Dual Multiply Subtract SMUSD, SMUSDX on page F6-3001
101		0x	not 1111	Signed Most Significant Word Multiply Accumulate SMMLA, SMMLAR on page F6-2987
			1111		Signed Most Significant Word Multiply SMMUL, SMMULR on page F6-2991
110		0x	-			Signed Most Significant Word Multiply Subtract SMMLS, SMMLSR on page F6-2989
111		00	not 1111	Unsigned Sum of Absolute Differences, Accumulate USADA8 on page F6-3163
			1111		Unsigned Sum of Absolute Differences USAD8 on page F6-3161
*/

	"mult", 0};
OP	thumb32_mult_long[] = {//0x1f800000, 0x18800000
/*
000		0000	Signed Multiply Long SMULL, SMULLS on page F6-2997
001		1111	Signed Divide SDIV on page F6-2948
010		0000	Unsigned Multiply Long UMULL, UMULLS on page F6-3147
011		1111	Unsigned Divide UDIV on page F6-3129
100		0000	Signed Multiply Accumulate Long SMLAL, SMLALS on page F6-2975
		10xx	Signed Multiply Accumulate Long (Halfwords) SMLALBB, SMLALBT, SMLALTB, SMLALTT on page F6-2977
		110x	Signed Multiply Accumulate Long Dual SMLALD, SMLALDX on page F6-2979
101		110x	Signed Multiply Subtract Long Dual SMLSLD, SMLSLDX on page F6-2985
110		0000	Unsigned Multiply Accumulate Long UMLAL, UMLALS on page F6-3145
		0110	Unsigned Multiply Accumulate Accumulate Long UMAAL on page F6-3143
*/
	"mult_long", 0};
OP	thumb32_cop_simd[] = {//0x1c000000, 0x1c000000
/*
-			00000x		-	-			UNDEFINED -
			11xxxx		-	-			Advanced SIMD Advanced SIMD data-	processing instructions on
not 101x	0xxxx0
			not 000x0x	-	-			Store Coprocessor STC, STC2 on page F6-	3016
			0xxxx1
			not 000x0x	-	not 1111	Load Coprocessor (immediate) LDC, LDC2 (immediate) on page F6-	2694
							1111		Load Coprocessor (literal) LDC, LDC2 (literal) on page F6-	2698
			000100		-	-			Move to Coprocessor from two general-purpose registers
			000101		-	-			Move to two general-	purpose registers from Coprocessor
			10xxxx		0	-			Coprocessor data operations CDP, CDP2 on page F6-	2635
			10xxx0		1	-			Move to Coprocessor from general-	purpose register
			10xxx1		1	-			Move to general-	purpose register from Coprocessor
101x		0xxxxx
			not 000x0x	-				Advanced SIMD, floating-point register load/store instructions
			00010x		-	-			Advanced SIMD, 64-	bit transfers accessing the SIMD
			10xxxx		0	-			Floating-	point data processing Floating-	point data-	processing instructions on
			10xxxx		1	-			Advanced SIMD, 8, 16, and 32-	bit transfers accessing the SIMD
*/
	"cop_simd", 0};

OP	DisassemblerARMThumb::ops32[] = {
	{(char*)thumb32_multiple,	0x1e400000, 0x00000000,	TABLE},
	{(char*)thumb32_loadstore,	0x1e400000, 0x00400000,	TABLE},
	{(char*)thumb32_data,		0x1e000000, 0x02000000,	TABLE},
	{(char*)thumb32_cop,		0x1c000000, 0x04000000,	TABLE},
	{(char*)thumb32_data_imod,	0x1a008000, 0x10000000,	TABLE},
	{(char*)thumb32_data_imm,	0x1a008000, 0x12000000,	TABLE},

	{(char*)thumb32_control,	0x18389000, 0x10388000,	TABLE},
	{"b",						0x1800d000, 0x10009000,	T32BRANCH},
	{"b<cond>",					0x1800d000, 0x10008000,	T32BRANCH},
	{"bl",						0x1800d000, 0x1000c000,	T32BRANCH},
	{"blx",						0x1800d000, 0x1000d000,	T32BRANCH},

	{(char*)thumb32_store1,		0x1f100000, 0x18000000,	TABLE},
	{(char*)thumb32_loadbyte,	0x1f700000, 0x18100000,	TABLE},
	{(char*)thumb32_loafhalf,	0x1f700000, 0x18300000,	TABLE},
	{(char*)thumb32_loadword,	0x1f700000, 0x18500000,	TABLE},
	{(char*)thumb32_undefined,	0x1f700000, 0x18700000,	TABLE},
	{(char*)thumb32_simd,		0x1f100000, 0x18000000,	TABLE},
	{(char*)thumb32_data_reg,	0x1f000000, 0x18000000,	TABLE},
	{(char*)thumb32_mult,		0x1f800000, 0x18000000,	TABLE},
	{(char*)thumb32_mult_long,	0x1f800000, 0x18800000,	TABLE},
	{(char*)thumb32_cop_simd,	0x1c000000, 0x1c000000,	TABLE},
0};

//-----------------------------------------------------------------------------
//	DisassemblerARM64
//-----------------------------------------------------------------------------
class DisassemblerARM64 : public DisassemblerARM {
	union opcode {
		struct { uint32 r0:5, r5:5, r10:5, :1,			r16:5, :1, L:1, :6, S:1, Q:1, size:1; };
		struct { uint32 :10, ext_shift:3, ext_opt:3,	:14, simdsize:2; };
		struct { uint32 :10, shift:6,					:6, shift_op:2, :8; };
		struct { uint32 cond0:4, :6, simdtype:2, cond:4,:16; };
		uint32	u;
		int32	s;

		opcode(uint32 _u) : u(_u) {}
		int	get_val(SOURCE source) {
			switch (source) {
				case XR0:		return r0;
				case XR5:		return r5;
				case XR10:		return r10;
				case XR16:		return r16;
				case XIMM7:		return extract_bits<15, 7>(s);
				case XIMMS:		return extract_bits<10, 6>(s);
				case XIMMR:		return extract_bits<16, 6>(s);
				case XIMM9:		return extract_bits<12, 9>(s);
				case XIMM12:	return extract_bits<10,12>(u);
				case XIMM14:	return extract_bits<5, 14>(s);
				case XIMM16:	return extract_bits<5, 16>(s);
				case XIMM19:	return extract_bits<5, 19>(s);
				case XIMM26:	return extract_bits<0, 26>(s);
				case XIMM16_5:	return extract_bits<16, 5>(u);
				case XFLAGS:	return extract_bits<0,  4>(u);
				default:		return 0;
			}
		}
	};
	static OP			ops[];
	static const char*	ext_opts[8];

public:
	DisassemblerARM64() {
		ISO_ASSERT(validate(ops));
	}
	virtual	const char*	GetDescription() { return "ARM64"; }
	virtual State*		Disassemble(const iso::memory_block &block, uint64 addr, SymbolFinder sym_finder) {
		StateDefault	*state = new StateDefault;

		for (uint32 *p = block, *end = (uint32*)block.end(); p < end; ) {
			uint32	offset		= uint32(addr + (uint8*)p - (uint8*)block);

			uint64			sym_addr;
			string_param	sym_name;
			if (sym_finder && sym_finder(offset, sym_addr, sym_name) && sym_addr == offset)
				state->lines.push_back(sym_name);

			uint32	u = *p++;
			buffer_accum<1024>	ba("%08x ", offset);
			ba.format("%04x ", u);

			opcode		op(u);
			OP			*op2	= get_op(ops, u);
			OP::SPEC	spec	= op2->spec;
			MODE		mode	= MODE(spec.mode);
			uint8		flags	= spec.flags;
			uint8		multi	= 0;
			uint8		size	= flags & XSIMDSIZE1 ? (flags & XSIMDSIZE ? 5 : op.simdsize) : flags & XSIMDSIZE ? (op.simdsize + 2) : flags & X32 ? 2 : flags & XSIZE31 ? (op.size + 2) : 3;
			REG_MODE	rm		= flags & XSIMD128 ? REG_MODE(RM_SIMD64 + size) : size == 0 ? RM_GENERAL64_32_SP : RM_GENERAL64_SP;

			switch (mode) {
				case XMULTI1:
				case XMULTI2:
				case XMULTI3:
				case XMULTI4:
					multi	= (mode - XMULTI1) + 1;
					size	= op.Q + 2;
					rm		= REG_MODE(RM_SIMD64 + size);
					break;
			}

			if (flags & XL)
				ba << (op.L ? "ld" : "st");

			ba << op2->name;

			if ((flags & XS) && op.S)
				ba << 's';

			if (str(op2->name).ends("."))
				ba << str(conditions + op.cond0 * 2, 2);

			ba << ' ';

			if (mode == XSRX) {
				put_reg(ba << op.get_val(SOURCE(spec.src1)), RM_GENERAL64_SP) << ", ";
			}

			const char *sep = ", ";
			switch (DEST d = DEST(spec.dest)) {
				case XRD0:
					if (multi) {
						char	type	= "bhsd"[op.simdtype];
						uint8	num		= 1 << (size - op.simdtype);
						for (int i = 0; i < multi; i++)
							put_reg(ba << (i == 0 ? '{' : ','), op.r0 + i, rm) << '.' << num << type;
						ba << '}';
					} else {
						put_reg(ba, op.r0, rm);
					}
					break;
				case XRD5:
					put_reg(ba, op.r5, rm);
					break;
				case XRDx2:
					put_reg(ba, op.r0, rm);
					put_reg(ba << ", ", op.r10, rm);
					break;
				default:
					sep = "";
					break;
			}

			int	i = 0;
			switch (mode) {
				case XBASE:
					put_reg(ba << sep << "[", op.get_val(SOURCE(spec.src1)), RM_GENERAL64_SP) << "]";
					i = 1;
					break;
				case XOFFSET:
					put_reg(ba << sep << "[", op.get_val(SOURCE(spec.src1)), RM_GENERAL64_SP) << ", #" << (op.get_val(SOURCE(spec.src2)) << size) << "]";
					i = 2;
					break;
				case XOFFSET_UNSCALED:
					put_reg(ba << sep << "[", op.get_val(SOURCE(spec.src1)), RM_GENERAL64_SP) << ", #" << op.get_val(SOURCE(spec.src2)) << "]";
					i = 2;
					break;
				case XPRE_INDEX:
					put_reg(ba << sep << "[", op.get_val(SOURCE(spec.src1)), RM_GENERAL64_SP) << ", #" << (op.get_val(SOURCE(spec.src2)) << size) << "]!";
					i = 2;
					break;
				case XPOST_INDEX:
					put_reg(ba << sep << "[", op.get_val(SOURCE(spec.src1)), RM_GENERAL64_SP) << "], #" << (op.get_val(SOURCE(spec.src2)) << size);
					i = 2;
					break;
				case XADDR:
					put_address(ba << sep, offset + (op.get_val(SOURCE(spec.src1)) << 2), sym_finder);
					i = 1;
					break;
				case XSRX:
					i = 1;
					break;
			}

			for (; i < 3; i++) {
				if (SOURCE s = spec.get_source(i)) {
					ba << sep;
					sep = ", ";
					switch (s) {
						case XEXTENDED:
							ba << ext_opts[op.ext_opt] << " #" << op.ext_shift;
							break;
						case XSHIFTED:
							put_shift(ba, op.shift, SHIFTOP(op.shift_op));
							break;
						case XCOND:
							ba << str(conditions + op.cond * 2, 2);
							break;
						case XQOFFSET:
							if (op.r16 == 31)
								ba << '#' << (multi << (size + 1));
							else
								put_reg(ba, op.r16, rm);
							break;
						default:
							int	val = op.get_val(s);
							if (s < XIMM)
								put_reg(ba, val, rm);
							else
								put_imm(ba, val);
					}
				}
			}

			state->lines.push_back((const char*)ba);
		}
		return state;
	}

} dis_arm64;

const char *DisassemblerARM64::ext_opts[8] = {
	"uxtb",
	"uxth",
	"uxtw",	//or lsl
	"uxtx",
	"sxtb",
	"sxth",
	"sxtw",
	"sxtx",
};

//C4.4.1 Add/subtract (immediate):	sf:31, op:30, S:29
OP arm64_addsub_imm[] = {
	{"add",		0x40000000, 0x00000000,	REG,	XSIZE31|XS,		XRD0,	XR5, XIMM12},//0 0 0
	{"cmp",		0x6000001f, 0x6000001f,	REG,	XSIZE31,		0,		XR5, XIMM12},//0 1 0 11111
	{"sub",		0x40000000, 0x40000000,	REG,	XSIZE31|XS,		XRD0,	XR5, XIMM12},//0 1 0
"addsub_imm"};

//C4.4.2 Bitfield: sf:31, opc:29:2, N:22
OP arm64_bitfield[] = {
	{"sbfm",	0x60000000, 0x00000000,	REG,	XSIZE31,		XRD0,	XR5, XIMMS, XIMMR},//0 00 0
	{"bfm",		0x60000000, 0x20000000,	REG,	XSIZE31,		XRD0,	XR5, XIMMS, XIMMR},//0 01 0
	{"ubfm",	0x60000000, 0x40000000,	REG,	XSIZE31,		XRD0,	XR5, XIMMS, XIMMR},//0 10 0
"bitfield"};

//C4.4.4 Logical (immediate): sf:31, opc:29:2, N:22
OP arm64_logical_imm[] = {
	{"and",		0x60000000, 0x00000000,	REG,	XSIZE31,		XRD0,	XR5, XIMMS, XIMMR},//0 00 0
	{"orr",		0x60000000, 0x20000000,	REG,	XSIZE31,		XRD0,	XR5, XIMMS, XIMMR},//0 01 0
	{"eor",		0x60000000, 0x40000000,	REG,	XSIZE31,		XRD0,	XR5, XIMMS, XIMMR},//0 10 0
	{"ands",	0x60000000, 0x60000000,	REG,	XSIZE31,		XRD0,	XR5, XIMMS, XIMMR},//0 11 0
"logical_imm"};

//C4.4.5 Move wide (immediate): sf:31, opc:29:2, hw:21:2
OP arm64_movewide_imm[] = {
	{"movn",	0x60000000, 0x00000000,	REG,	XSIZE31,		XRD0,	XIMM16},//0 00
	{"movz",	0x60000000, 0x40000000,	REG,	XSIZE31,		XRD0,	XIMM16},//0 10
	{"movk",	0x60000000, 0x60000000,	REG,	XSIZE31,		XRD0,	XIMM16},//0 11
"movewide_imm"};

//C4.4 Data processing - immediate
OP arm64_data_imm[] = {
	{"adr",						0x9f000000, 0x10000000, XADDR,	0,	XRD0, XIMM19	},					//- - - 1 0 0 0 0 - - - - - - - - - - - - - -
	{"adrp",					0x9f000000, 0x90000000,	XADDR,	0,	XRD0, XIMM19	},					//- - - 1 0 0 0 0 - - - - - - - - - - - - - -
	{(char*)arm64_addsub_imm,	0x1f000000, 0x11000000, TABLE	},										//- - - 1 0 0 0 1 - - - - - - - - - - - - - -
	{(char*)arm64_logical_imm,	0x1f800000, 0x12000000, TABLE	},										//- - - 1 0 0 1 0 0 - - - - - - - - - - - - -
	{(char*)arm64_movewide_imm,	0x1f800000, 0x12800000, TABLE	},										//- - - 1 0 0 1 0 1 - - - - - - - - - - - - -
	{(char*)arm64_bitfield,		0x1f800000, 0x13000000, TABLE	},										//- - - 1 0 0 1 1 0 - - - - - - - - - - - - -
	{"extr",					0x1f800000, 0x13800000,	REG,	XSIZE31,		XRD0, XR5, XIMMS, XR16},//- - - 1 0 0 1 1 1 - - - - - - - - - - - - -
"data_imm"};

//C4.2.3 Exception generation
OP arm64_except[] = {
	{"svc", 	0x00e0001f, 0x00000001,	},	//000 000 01
	{"hvc", 	0x00e0001f, 0x00000002,	},	//000 000 10
	{"smc", 	0x00e0001f, 0x00000003,	},	//000 000 11
	{"brk", 	0x00e0001f, 0x00200000,	},	//001 000 00
	{"hlt",		0x00e0001f, 0x00400000,	},	//010 000 00
	{"dcps1",	0x00e0001f, 0x00a00001,	},	//101 000 01
	{"dcps2",	0x00e0001f, 0x00a00002,	},	//101 000 10
	{"dcps3",	0x00e0001f, 0x00a00003,	},	//101 000 11
"except"};

//C4.2.4 System
OP arm64_system[] = {
	{"msr",		0x003f001f, 0x0000001f,	},	//0 00 -	0100 -		11111
	{"hint",	0x003f001f, 0x0003001f,	},	//0 00 011	0010 -		11111
	{"clrex",	0x003f00ff, 0x0003005f,	},	//0 00 011	0011 010	11111
	{"dsb",		0x003f00ff, 0x0003009f,	},	//0 00 011	0011 100	11111
	{"dmb",		0x003f00ff, 0x000300bf,	},	//0 00 011	0011 101	11111
	{"isb",		0x003f00ff, 0x000300df,	},	//0 00 011	0011 110	11111
	{"sys",		0x00380000, 0x00080000,	},	//0 01 - - - -
	{"msr",		0x00300000, 0x00100000,	},	//0 1x - - - -
	{"sysl",	0x00380000, 0x00200000,	},	//1 01 - - - -
	{"mrs",		0x00300000, 0x00300000,	},	//1 1x - - - -
"system"};

//C4.2 Branches, exception generating and system instructions
OP arm64_branch_etc[] = {
	{"b",						0xfc000000, 0x14000000, XADDR,	0,			0,	XIMM26	},//- 0 0 1 0 1 - - - - - - - - - - - - - - - -
	{"bl",						0xfc000000, 0x94000000, XADDR,	0,			0,	XIMM26	},//- 0 1 1 0 1 0 - - - - - - - - - - - - - - -
	{"cbz",						0x7e100000, 0x34000000, XADDR,	XSIZE31,	0,	XIMM19	},//- 0 1 1 0 1 1 - - - - - - - - - - - - - - -
	{"cbnz",					0x7e100000, 0x34100000, XADDR,	XSIZE31,	0,	XIMM19	},//0 1 0 1 0 1 0 - - - - - - - - - - - - - - -
	{"tbz",						0x7e100000, 0x36000000, XADDR,	0,			0,	XIMM14	},//1 1 0 1 0 1 0 0 - - - - - - - - - - - - - -
	{"tbnz",					0x7e100000, 0x36100000, XADDR,	0,			0,	XIMM14	},//1 1 0 1 0 1 0 1 0 0 - - - - - - - - - - - -
	{"b.",						0xfe000000, 0x54000000, XADDR,	0,			0,	XIMM19	},//1 1 0 1 0 1 1 - - - - - - - - - - - - - - -
	{(char*)arm64_except,		0xff000000, 0xd4000000, TABLE	},
	{(char*)arm64_system,		0xffc00000, 0xd5000000, TABLE	},
	{"br",						0xfee00000, 0xd6000000,	REG,	0,			0,	XR5		},//0000 -
	{"blr",						0xfee00000, 0xd6200000,	REG,	0,			0,	XR5		},//0001 -
	{"ret",						0xfee00000, 0xd6400000,	REG,	0,			0,	XR5		},//0010 -
	{"eret",					0xfee003e0, 0xd68003e0,	REG,	0,			0,	XR5		},//0100 11111
	{"drps",					0xfee003e0, 0xd6a003e0,	REG,	0,			0,	XR5		},//0101 11111
"branch_etc"};

//C4.3.1 Advanced SIMD load/store multiple structures
OP arm64_SIMD_LS_multi[] = {
	{"4",	0x0000f000, 0x00000000,	XMULTI4,	XL,	XRD0, XR5},//0 11111 0000
	{"1",	0x0000f000, 0x00002000,	XMULTI4,	XL,	XRD0, XR5},//0 11111 0010
	{"3",	0x0000f000, 0x00004000,	XMULTI3,	XL,	XRD0, XR5},//0 11111 0100
	{"1",	0x0000f000, 0x00006000,	XMULTI3,	XL,	XRD0, XR5},//0 11111 0110
	{"1",	0x0000f000, 0x00007000,	XMULTI1,	XL,	XRD0, XR5},//0 11111 0111
	{"2",	0x0000f000, 0x00008000,	XMULTI2,	XL,	XRD0, XR5},//0 11111 1000
	{"1",	0x0000f000, 0x0000a000,	XMULTI2,	XL,	XRD0, XR5},//0 11111 1010
0};

//C4.3.2 Advanced SIMD load/store multiple structures (post-indexed)
OP arm64_SIMD_LS_multi_post[] = {
	{"4",	0x0000f000, 0x00000000,	XMULTI4,	XL,	XRD0, XR5, XQOFFSET},//0 11111 0000
	{"1",	0x0000f000, 0x00002000,	XMULTI4,	XL,	XRD0, XR5, XQOFFSET},//0 11111 0010
	{"3",	0x0000f000, 0x00004000,	XMULTI3,	XL,	XRD0, XR5, XQOFFSET},//0 11111 0100
	{"1",	0x0000f000, 0x00006000,	XMULTI3,	XL,	XRD0, XR5, XQOFFSET},//0 11111 0110
	{"1",	0x0000f000, 0x00007000,	XMULTI1,	XL,	XRD0, XR5, XQOFFSET},//0 11111 0111
	{"2",	0x0000f000, 0x00008000,	XMULTI2,	XL,	XRD0, XR5, XQOFFSET},//0 11111 1000
	{"1",	0x0000f000, 0x0000a000,	XMULTI2,	XL,	XRD0, XR5, XQOFFSET},//0 11111 1010
0};

//C4.3.3 Advanced SIMD load/store single structure
OP arm64_SIMD_LS_single[] = {
	{"1",		0x0020e000, 0x00000000,	XMULTI1,	XL, XRD0, XR5},//0 0 11111 000 - -
	{"3",		0x0020e000, 0x00002000,	XMULTI3,	XL, XRD0, XR5},//0 0 11111 001 - -
	{"1",		0x0020e400, 0x00004000,	XMULTI1,	XL, XRD0, XR5},//0 0 11111 010 - x0
	{"3",		0x0020e400, 0x00006000,	XMULTI3,	XL, XRD0, XR5},//0 0 11111 011 - x0
	{"1",		0x0020ec00, 0x00008000,	XMULTI1,	XL, XRD0, XR5},//0 0 11111 100 - 00
	{"1",		0x0020fc00, 0x00008400,	XMULTI1,	XL, XRD0, XR5},//0 0 11111 100 0 01
	{"3",		0x0020ec00, 0x0000a000,	XMULTI3,	XL, XRD0, XR5},//0 0 11111 101 - 00
	{"3",		0x0020fc00, 0x0000a400,	XMULTI3,	XL, XRD0, XR5},//0 0 11111 101 0 01
	{"2",		0x0020e000, 0x00200000,	XMULTI2,	XL, XRD0, XR5},//0 1 11111 000 - -
	{"4",		0x0020e000, 0x00202000,	XMULTI4,	XL, XRD0, XR5},//0 1 11111 001 - -
	{"2",		0x0020e400, 0x00204000,	XMULTI2,	XL, XRD0, XR5},//0 1 11111 010 - x0
	{"4",		0x0020e400, 0x00206000,	XMULTI4,	XL, XRD0, XR5},//0 1 11111 011 - x0
	{"2",		0x0020ec00, 0x00208000,	XMULTI2,	XL, XRD0, XR5},//0 1 11111 100 - 00
	{"2",		0x0020fc00, 0x00208400,	XMULTI2,	XL, XRD0, XR5},//0 1 11111 100 0 01
	{"4",		0x0020ec00, 0x0020a000,	XMULTI4,	XL, XRD0, XR5},//0 1 11111 101 - 00
	{"4",		0x0020fc00, 0x0020a400,	XMULTI4,	XL, XRD0, XR5},//0 1 11111 101 0 01
	{"ld1r",	0x0060f000, 0x0040c000,	XMULTI1,	XL, XRD0, XR5},//1 0 11111 110 0 -
	{"ld3r",	0x0060f000, 0x0040e000,	XMULTI3,	XL, XRD0, XR5},//1 0 11111 111 0 -
	{"ld2r",	0x0060f000, 0x0060c000,	XMULTI2,	XL, XRD0, XR5},//1 1 11111 110 0 -
	{"ld4r",	0x0060f000, 0x0060e000,	XMULTI4,	XL, XRD0, XR5},//1 1 11111 111 0 -
0};

//C4.3.4 Advanced SIMD load/store single structure (post-indexed)
OP arm64_SIMD_LS_single_post[] = {
	{"1",		0x0020e000, 0x00000000,	XMULTI1,	XL, XRD0, XR5, XQOFFSET},//0 0 11111 000 - -
	{"3",		0x0020e000, 0x00002000,	XMULTI3,	XL, XRD0, XR5, XQOFFSET},//0 0 11111 001 - -
	{"1",		0x0020e400, 0x00004000,	XMULTI1,	XL, XRD0, XR5, XQOFFSET},//0 0 11111 010 - x0
	{"3",		0x0020e400, 0x00006000,	XMULTI3,	XL, XRD0, XR5, XQOFFSET},//0 0 11111 011 - x0
	{"1",		0x0020ec00, 0x00008000,	XMULTI1,	XL, XRD0, XR5, XQOFFSET},//0 0 11111 100 - 00
	{"1",		0x0020fc00, 0x00008400,	XMULTI1,	XL, XRD0, XR5, XQOFFSET},//0 0 11111 100 0 01
	{"3",		0x0020ec00, 0x0000a000,	XMULTI3,	XL, XRD0, XR5, XQOFFSET},//0 0 11111 101 - 00
	{"3",		0x0020fc00, 0x0000a400,	XMULTI3,	XL, XRD0, XR5, XQOFFSET},//0 0 11111 101 0 01
	{"2",		0x0020e000, 0x00200000,	XMULTI2,	XL, XRD0, XR5, XQOFFSET},//0 1 11111 000 - -
	{"4",		0x0020e000, 0x00202000,	XMULTI4,	XL, XRD0, XR5, XQOFFSET},//0 1 11111 001 - -
	{"2",		0x0020e400, 0x00204000,	XMULTI2,	XL, XRD0, XR5, XQOFFSET},//0 1 11111 010 - x0
	{"4",		0x0020e400, 0x00206000,	XMULTI4,	XL, XRD0, XR5, XQOFFSET},//0 1 11111 011 - x0
	{"2",		0x0020ec00, 0x00208000,	XMULTI2,	XL, XRD0, XR5, XQOFFSET},//0 1 11111 100 - 00
	{"2",		0x0020fc00, 0x00208400,	XMULTI2,	XL, XRD0, XR5, XQOFFSET},//0 1 11111 100 0 01
	{"4",		0x0020ec00, 0x0020a000,	XMULTI4,	XL, XRD0, XR5, XQOFFSET},//0 1 11111 101 - 00
	{"4",		0x0020fc00, 0x0020a400,	XMULTI4,	XL, XRD0, XR5, XQOFFSET},//0 1 11111 101 0 01
	{"ld1r",	0x0060f000, 0x0040c000,	XMULTI1,	XL, XRD0, XR5, XQOFFSET},//1 0 11111 110 0 -
	{"ld3r",	0x0060f000, 0x0040e000,	XMULTI3,	XL, XRD0, XR5, XQOFFSET},//1 0 11111 111 0 -
	{"ld2r",	0x0060f000, 0x0060c000,	XMULTI2,	XL, XRD0, XR5, XQOFFSET},//1 1 11111 110 0 -
	{"ld4r",	0x0060f000, 0x0060e000,	XMULTI4,	XL, XRD0, XR5, XQOFFSET},//1 1 11111 111 0 -
0};

//C4.3.5 Load register (literal)
OP arm64_lit[] = {
	{"ldr",		0xc4000000, 0x00000000,	REG,	X32,		XRD0,	XIMM19},//00 0
	{"ldr",		0xc4000000, 0x40000000,	REG,	0,			XRD0,	XIMM19},//00 1
	{"ldrsw",	0xc4000000, 0x80000000,	REG,	0,			XRD0,	XIMM19},//01 0
	{"prfm",	0xc4000000, 0xc0000000,	REG,	0,			XRD0,	XIMM19},//10 0
	{"ldr",		0xc4000000, 0x04000000,	REG,	XSIMDSIZE,	XRD0,	XIMM19},//11 0
0};

//size:30:2, o2:23, L:22, o1:21, o0:15
//C4.3.6 LS_ exclusive
OP arm64_LS_exclusive[] = {
	{"stxrb",	0xc0e08000, 0x00000000,	XSRX,	0,	XRD0, XR16, XR5},	//00 0 0 0 0
	{"ldxrb",	0xc0e08000, 0x00400000,	REG,	0,	XRD0, XR5},			//00 0 1 0 0
	{"stlxrb",	0xc0e08000, 0x00008000,	XSRX,	0,	XRD0, XR16, XR5},	//00 0 0 0 1
	{"ldaxrb",	0xc0e08000, 0x00408000,	REG,	0,	XRD0, XR5},			//00 0 1 0 1
	{"stlrb",	0xc0e08000, 0x00808000,	XSRX,	0,	XRD0, XR16, XR5},	//00 1 0 0 1
	{"ldarb",	0xc0e08000, 0x00c08000,	REG,	0,	XRD0, XR5},			//00 1 1 0 1
	{"stxrh",	0xc0e08000, 0x40000000,	XSRX,	0,	XRD0, XR16, XR5},	//01 0 0 0 0
	{"ldxrh",	0xc0e08000, 0x40400000,	REG,	0,	XRD0, XR5},			//01 0 1 0 0
	{"stlxrh",	0xc0e08000, 0x40008000,	XSRX,	0,	XRD0, XR16, XR5},	//01 0 0 0 1
	{"ldaxrh",	0xc0e08000, 0x40408000,	REG,	0,	XRD0, XR5},			//01 0 1 0 1
	{"stlrh",	0xc0e08000, 0x40008000,	XSRX,	0,	XRD0, XR16, XR5},	//01 1 0 0 1
	{"ldarh",	0xc0e08000, 0x40c08000,	REG,	0,	XRD0, XR5},			//01 1 1 0 1
	{"stxr",	0xc0e08000, 0x80000000,	XSRX,	0,	XRD0, XR16, XR5},	//10 0 0 0 0
	{"ldxr",	0xc0e08000, 0x80400000,	REG,	0,	XRD0, XR5},			//10 0 1 0 0
	{"stlxr",	0xc0e08000, 0x80008000,	XSRX,	0,	XRD0, XR16, XR5},	//10 0 0 0 1
	{"ldaxr",	0xc0e08000, 0x80408000,	REG,	0,	XRD0, XR5},			//10 0 1 0 1
	{"stxp",	0xc0e08000, 0x80000000,	XSRX,	0,	XRD0, XR16, XR5},	//10 0 0 1 0
	{"ldxp",	0xc0e08000, 0x80600000,	REG,	0,	XRD0, XR5},			//10 0 1 1 0
	{"stlxp",	0xc0e08000, 0x80208000,	XSRX,	0,	XRD0, XR16, XR5},	//10 0 0 1 1
	{"ldaxp",	0xc0e08000, 0x80608000,	REG,	0,	XRD0, XR5},			//10 0 1 1 1
	{"stlr",	0xc0e08000, 0x80808000,	XSRX,	0,	XRD0, XR16, XR5},	//10 1 0 0 1
	{"ldar",	0xc0e08000, 0x80008000,	REG,	0,	XRD0, XR5},			//10 1 1 0 1
	{"stxr",	0xc0e08000, 0xc0000000,	XSRX,	0,	XRD0, XR16, XR5},	//11 0 0 0 0
	{"ldxr",	0xc0e08000, 0xc0400000,	REG,	0,	XRD0, XR5},			//11 0 1 0 0
	{"stlxr",	0xc0e08000, 0xc0008000,	XSRX,	0,	XRD0, XR16, XR5},	//11 0 0 0 1
	{"ldaxr",	0xc0e08000, 0xc0408000,	REG,	0,	XRD0, XR5},			//11 0 1 0 1
	{"stxp",	0xc0e08000, 0xc0200000,	XSRX,	0,	XRD0, XR16, XR5},	//11 0 0 1 0
	{"ldxp",	0xc0e08000, 0xc0600000,	REG,	0,	XRD0, XR5},			//11 0 1 1 0
	{"stlxp",	0xc0e08000, 0xc0208000,	XSRX,	0,	XRD0, XR16, XR5},	//11 0 0 1 1
	{"ldaxp",	0xc0e08000, 0xc0608000,	REG,	0,	XRD0, XR5},			//11 0 1 1 1
	{"stlr",	0xc0e08000, 0xc0808000,	XSRX,	0,	XRD0, XR16, XR5},	//11 1 0 0 1
	{"ldar",	0xc0e08000, 0xc0c08000,	REG,	0,	XRD0, XR5},			//11 1 1 0 1
0 };

//C4.3.8 LS_ register (immediate post-indexed)
OP	arm64_LS_reg_imm_post[] = {
	{"rb",		0xc4800000, 0x00000000,	XPOST_INDEX,	XL,				XRD0,	XR5,	XIMM9},//00 0 00
	{"r",		0x04800000, 0x04000000,	XPOST_INDEX,	XL|XSIMDSIZE1,	XRD0,	XR5,	XIMM9},//00 1 00
	{"r",		0xc4800000, 0x04800000,	XPOST_INDEX,	XL|XSIMD128,	XRD0,	XR5,	XIMM9},//00 1 10
	{"rh",		0xc4800000, 0x40000000,	XPOST_INDEX,	XL,				XRD0,	XR5,	XIMM9},//01 0 00
	{"r",		0xc4800000, 0x80000000,	XPOST_INDEX,	XL,				XRD0,	XR5,	XIMM9},//10 0 00
	{"r",		0xc4800000, 0xc0000000,	XPOST_INDEX,	XL,				XRD0,	XR5,	XIMM9},//11 0 00
	{"ldrsb",	0xc4c00000, 0x00800000,	XPOST_INDEX,	0,				XRD0,	XR5,	XIMM9},//00 0 10
	{"ldrsb",	0xc4c00000, 0x00c00000,	XPOST_INDEX,	X32,			XRD0,	XR5,	XIMM9},//00 0 11
	{"ldrsh",	0xc4c00000, 0x40800000,	XPOST_INDEX,	0,				XRD0,	XR5,	XIMM9},//01 0 10
	{"ldrsh",	0xc4c00000, 0x40c00000,	XPOST_INDEX,	0,				XRD0,	XR5,	XIMM9},//01 0 11
	{"ldrsw",	0xc4c00000, 0x80800000,	XPOST_INDEX,	0,				XRD0,	XR5,	XIMM9},//10 0 10
0};

//C4.3.9 LS_ register (immediate pre-indexed)
OP	arm64_LS_reg_imm_pre[] = {
	{"rb",		0xc4800000, 0x00000000,	XPRE_INDEX,		XL,				XRD0,	XR5,	XIMM9},//00 0 00
	{"r",		0x04800000, 0x04000000,	XPRE_INDEX,		XL|XSIMDSIZE1,	XRD0,	XR5,	XIMM9},//00 1 00
	{"r",		0xc4800000, 0x04800000,	XPRE_INDEX,		XL|XSIMD128,	XRD0,	XR5,	XIMM9},//00 1 10
	{"rh",		0xc4800000, 0x40000000,	XPRE_INDEX,		XL,				XRD0,	XR5,	XIMM9},//01 0 00
	{"r",		0xc4800000, 0x80000000,	XPRE_INDEX,		XL,				XRD0,	XR5,	XIMM9},//10 0 00
	{"r",		0xc4800000, 0xc0000000,	XPRE_INDEX,		XL,				XRD0,	XR5,	XIMM9},//11 0 00
	{"ldrsb",	0xc4c00000, 0x00800000,	XPRE_INDEX,		0,				XRD0,	XR5,	XIMM9},//00 0 10
	{"ldrsb",	0xc4c00000, 0x00c00000,	XPRE_INDEX,		X32,			XRD0,	XR5,	XIMM9},//00 0 11
	{"ldrsh",	0xc4c00000, 0x40800000,	XPRE_INDEX,		0,				XRD0,	XR5,	XIMM9},//01 0 10
	{"ldrsh",	0xc4c00000, 0x40c00000,	XPRE_INDEX,		0,				XRD0,	XR5,	XIMM9},//01 0 11
	{"ldrsw",	0xc4c00000, 0x80800000,	XPRE_INDEX,		0,				XRD0,	XR5,	XIMM9},//10 0 10
0};

//C4.3.10 LS_ register (register offset)
OP	arm64_LS_reg_reg_off[] = {
	{"rb",		0xc4800000, 0x00000000,	XOFFSET,	XL,				XRD0,	XR5,	XR16,	XEXTENDED},//00 0 00
	{"r",		0xc4800000, 0x04000000,	XOFFSET,	XL|XSIMDSIZE1,	XRD0,	XR5,	XR16,	XEXTENDED},//00 1 00
	{"r",		0xc4800000, 0x04800000,	XOFFSET,	XL|XSIMD128,	XRD0,	XR5,	XR16,	XEXTENDED},//00 1 10
	{"rh",		0xc4800000, 0x40000000,	XOFFSET,	XL,				XRD0,	XR5,	XR16,	XEXTENDED},//01 0 00
	{"r",		0xc4800000, 0x80000000,	XOFFSET,	XL,				XRD0,	XR5,	XR16,	XEXTENDED},//10 0 00
	{"r",		0xc4800000, 0xc0000000,	XOFFSET,	XL,				XRD0,	XR5,	XR16,	XEXTENDED},//11 0 00
	{"ldrsb",	0xc4c00000, 0x00800000,	XOFFSET,	0,				XRD0,	XR5,	XR16,	XEXTENDED},//00 0 10
	{"ldrsb",	0xc4c00000, 0x00c00000,	XOFFSET,	X32,			XRD0,	XR5,	XR16,	XEXTENDED},//00 0 11
	{"ldrsh",	0xc4c00000, 0x40800000,	XOFFSET,	0,				XRD0,	XR5,	XR16,	XEXTENDED},//01 0 10
	{"ldrsh",	0xc4c00000, 0x40c00000,	XOFFSET,	0,				XRD0,	XR5,	XR16,	XEXTENDED},//01 0 11
	{"ldrsw",	0xc4c00000, 0x80800000,	XOFFSET,	0,				XRD0,	XR5,	XR16,	XEXTENDED},//10 0 10
	{"prfm",	0xc4c00000, 0xc0800000,	XOFFSET,	0,				0,		XR5,	XR16,	XEXTENDED},//11 0 10
0};

//C4.3.11 LS_ register (unprivileged)
OP	arm64_LS_reg_unpriv[] = {
	{"trb",		0xc4800000, 0x00000000,	REG,	XL,		XRD0,	XR5,	XIMM9},//00 0 00
	{"trh",		0xc4800000, 0x40000000,	REG,	XL,		XRD0,	XR5,	XIMM9},//01 0 00
	{"tr",		0xc4800000, 0x80000000,	REG,	XL|X32,	XRD0,	XR5,	XIMM9},//10 0 00
	{"tr",		0xc4800000, 0xc0000000,	REG,	XL,		XRD0,	XR5,	XIMM9},//10 0 01
	{"ldtrsb",	0xc4c00000, 0x00c00000,	REG,	X32,	XRD0,	XR5,	XIMM9},//00 0 10
	{"ldtrsb",	0xc4c00000, 0x00800000,	REG,	0,		XRD0,	XR5,	XIMM9},//00 0 11
	{"ldtrsh",	0xc4c00000, 0x40c00000,	REG,	X32,	XRD0,	XR5,	XIMM9},//01 0 10
	{"ldtrsh",	0xc4c00000, 0x40800000,	REG,	0,		XRD0,	XR5,	XIMM9},//01 0 11
	{"ldtrsw",	0xc4c00000, 0x80800000,	REG,	0,		XRD0,	XR5,	XIMM9},//10 0 10
0};

//C4.3.12 LS_ register (unscaled immediate)
OP arm64_LS_reg_unscaled_imm[] = {
	{"urb",		0xc4800000, 0x00000000,	XOFFSET_UNSCALED,	XL,				XRD0, XR5, XIMM9},	//00 0 00
	{"ur",		0x04800000, 0x04000000,	XOFFSET_UNSCALED,	XL|XSIMDSIZE1,	XRD0, XR5, XIMM9},	//00 1 00
	{"ur",		0xc4800000, 0x04800000,	XOFFSET_UNSCALED,	XL|XSIMD128,	XRD0, XR5, XIMM9},	//00 1 10
	{"urh",		0xc4800000, 0x40000000,	XOFFSET_UNSCALED,	XL,				XRD0, XR5, XIMM9},	//01 0 00
	{"ur",		0xc4800000, 0x80000000,	XOFFSET_UNSCALED,	XL,				XRD0, XR5, XIMM9},	//10 0 00
	{"ur",		0xc4800000, 0xc0000000,	XOFFSET_UNSCALED,	XL,				XRD0, XR5, XIMM9},	//11 0 00
	{"ldursb",	0xc4c00000, 0x00800000,	XOFFSET_UNSCALED,	0,				XRD0, XR5, XIMM9},	//00 0 10
	{"ldursb",	0xc4c00000, 0x00c00000,	XOFFSET_UNSCALED,	0,				XRD0, XR5, XIMM9},	//00 0 11
	{"ldursh",	0xc4c00000, 0x40800000,	XOFFSET_UNSCALED,	0,				XRD0, XR5, XIMM9},	//01 0 10
	{"ldursh",	0xc4c00000, 0x40c00000,	XOFFSET_UNSCALED,	0,				XRD0, XR5, XIMM9},	//01 0 11
	{"ldursw",	0xc4c00000, 0x80800000,	XOFFSET_UNSCALED,	0,				XRD0, XR5, XIMM9},	//10 0 10
	{"prfum",	0xc4c00000, 0xc0800000,	XOFFSET_UNSCALED,	0,				XRD0, XR5, XIMM9},	//11 0 10
"LS_reg_unscaled_imm"};

//C4.3.13 LS_ register (unsigned immediate)
OP arm64_LS_reg_unsigned_imm[] = {
	{"rb",		0xc4800000, 0x00000000,	XOFFSET,	XL,				XRD0, XR5, XIMM12},	//00 0 00
	{"r",		0x04800000, 0x04000000,	XOFFSET,	XL|XSIMDSIZE1,	XRD0, XR5, XIMM12},	//00 1 00
	{"r",		0xc4800000, 0x04800000,	XOFFSET,	XL|XSIMD128,	XRD0, XR5, XIMM12},	//00 1 10
	{"rh",		0xc4800000, 0x40000000,	XOFFSET,	XL,				XRD0, XR5, XIMM12},	//01 0 00
	{"r",		0xc4800000, 0x80000000,	XOFFSET,	XL,				XRD0, XR5, XIMM12},	//10 0 00
	{"r",		0xc4800000, 0xc0000000,	XOFFSET,	XL,				XRD0, XR5, XIMM12},	//11 0 00
	{"ldrsb",	0xc4c00000, 0x00800000,	XOFFSET,	0,				XRD0, XR5, XIMM12},	//00 0 10
	{"ldrsb",	0xc4c00000, 0x00c00000,	XOFFSET,	0,				XRD0, XR5, XIMM12},	//00 0 11
	{"ldrsh",	0xc4c00000, 0x40800000,	XOFFSET,	0,				XRD0, XR5, XIMM12},	//01 0 10
	{"ldrsh",	0xc4c00000, 0x40c00000,	XOFFSET,	0,				XRD0, XR5, XIMM12},	//01 0 11
	{"ldrsw",	0xc4c00000, 0x80800000,	XOFFSET,	0,				XRD0, XR5, XIMM12},	//10 0 10
	{"prfum",	0xc4c00000, 0xc0800000,	XOFFSET,	0,				XRD0, XR5, XIMM12},	//11 0 10
"LS_reg_unsigned_imm"};

//C4.3.7 LS_ no-allocate pair (offset)
OP arm64_LS_noalloc_pair_off[] = {
	{ "np",			0, 0,					XOFFSET,		XL|XSIZE31,	XRDx2,	XR5, XIMM7 },
};

//C4.3.14 LS_ register pair (offset)
OP	arm64_LS_pair_off[] = {
	{ "ldpsw",		0xc4400000, 0x40400000,	XOFFSET,		0,			XRDx2,	XR5, XIMM7 },
	{ "p",			0, 0,					XOFFSET,		XL|XSIZE31,	XRDx2,	XR5, XIMM7 },
};

//C4.3.15 LS_ register pair (post-indexed)
OP	arm64_LS_pair_post[] = {
	{ "ldpsw",		0xc4400000, 0x40400000,	XPOST_INDEX,	0,			XRDx2,	XR5, XIMM7 },
	{ "p",			0, 0,					XPOST_INDEX,	XL|XSIZE31,	XRDx2,	XR5, XIMM7 },
0};

//C4.3.16 LS_ register pair (pre-indexed)
OP	arm64_LS_pair_pre[] = {
	{ "ldpsw",		0xc4400000, 0x40400000,	XPRE_INDEX,		0,			XRDx2,	XR5, XIMM7 },
	{ "p",			0, 0,					XPRE_INDEX,		XL|XSIZE31,	XRDx2,	XR5, XIMM7 },
};

OP arm64_loadstore[] = {
	{(char*)arm64_LS_exclusive,			0x3f000000, 0x08000000,	TABLE	},//- - 0 0 1 0 0 0 - - - - - - - - - - - - - - LS_ exclusive
	{(char*)arm64_lit,					0x3b000000, 0x18000000,	TABLE	},//- - 0 1 1 - 0 0 - - - - - - - - - - - - - - Load register (literal)
	{(char*)arm64_LS_noalloc_pair_off,	0x3b800000, 0x28000000,	TABLE	},//- - 1 0 1 - 0 0 0 - - - - - - - - - - - - - LS_ no-allocate pair (offset)
	{(char*)arm64_LS_pair_post,			0x3b800000, 0x28800000,	TABLE	},//- - 1 0 1 - 0 0 1 - - - - - - - - - - - - - LS_ register pair (post-indexed)
	{(char*)arm64_LS_pair_off,			0x3b800000, 0x29000000,	TABLE,	},//- - 1 0 1 - 0 1 0 - - - - - - - - - - - - - LS_ register pair (offset)
	{(char*)arm64_LS_pair_pre,			0x3b800000, 0x29800000, TABLE	},//- - 1 0 1 - 0 1 1 - - - - - - - - - - - - - LS_ register pair (pre-indexed)
	{(char*)arm64_LS_reg_unscaled_imm,	0x3b200c00, 0x38000000, TABLE	},//- - 1 1 1 - 0 0 - - 0 - - - - - - - - - 0 0 LS_ register (unscaled immediate)
	{(char*)arm64_LS_reg_imm_post,		0x3b200c00, 0x38000400, TABLE	},//- - 1 1 1 - 0 0 - - 0 - - - - - - - - - 0 1 LS_ register (immediate post-indexed)
	{(char*)arm64_LS_reg_unpriv,		0x3b200c00, 0x38000800, TABLE	},//- - 1 1 1 - 0 0 - - 0 - - - - - - - - - 1 0 LS_ register (unprivileged)
	{(char*)arm64_LS_reg_imm_pre,		0x3b200c00, 0x38000c00, TABLE	},//- - 1 1 1 - 0 0 - - 0 - - - - - - - - - 1 1 LS_ register (immediate pre-indexed)
	{(char*)arm64_LS_reg_reg_off,		0x3b200c00, 0x38200800, TABLE	},//- - 1 1 1 - 0 0 - - 1 - - - - - - - - - 1 0 LS_ register (register offset)
	{(char*)arm64_LS_reg_unsigned_imm,	0x3b000000, 0x39000000,	TABLE	},//- - 1 1 1 - 0 1 - - - - - - - - - - - - - - LS_ register (unsigned immediate)
	{(char*)arm64_SIMD_LS_multi,		0xbfbf0000, 0x0c000000,	TABLE	},//0 - 0 0 1 1 0 0 0 - 0 0 0 0 0 0 - - - - - - Advanced SIMD load/store multiple structures
	{(char*)arm64_SIMD_LS_multi_post,	0xbfa00000, 0x0c800000,	TABLE	},//0 - 0 0 1 1 0 0 1 - 0 - - - - - - - - - - - Advanced SIMD load/store multiple structures (post-indexed)
	{(char*)arm64_SIMD_LS_single,		0xbf9f0000, 0x0d000000,	TABLE	},//0 - 0 0 1 1 0 1 0 - - 0 0 0 0 0 - - - - - - Advanced SIMD load/store single structure
	{(char*)arm64_SIMD_LS_single_post,	0xbf800000, 0x0d800000,	TABLE	},//0 - 0 0 1 1 0 1 1 - - - - - - - - - - - - - Advanced SIMD load/store single structure (post-indexed)
"loadstore"};

//C4.5.1 Add/subtract (extended register)
OP arm64_addsub_extended[] = {
	{"add",		0x40000000, 0x00000000,	REG,	XSIZE31|XS,	XRD0,	XR5, XR16, XEXTENDED},//x 0 S
	{"cmp",		0x6000001f, 0x6000001f,	REG,	XSIZE31,	0,		XR5, XR16, XEXTENDED},//x 1 S 11111
	{"sub",		0x40000000, 0x40000000,	REG,	XSIZE31|XS,	XRD0,	XR5, XR16, XEXTENDED},//x 1 S
"addsub_extended"};

//C4.5.2 Add/subtract (shifted register)
OP arm64_addsub_shifted[] = {
	{"add",		0x40000000, 0x00000000,	REG,	XSIZE31|XS,	XRD0,	XR5, XR16, XSHIFTED},//x 0 S
	{"cmp",		0x6000001f, 0x6000001f,	REG,	XSIZE31,	0,		XR5, XR16, XSHIFTED},//x 1 S 11111
	{"sub",		0x40000000, 0x40000000,	REG,	XSIZE31|XS,	XRD0,	XR5, XR16, XSHIFTED},//x 1 S
"addsub_shifted"};

//C4.5.3 Add/subtract (with carry)
OP arm64_addsub_carry[] = {
	{"adc",		0x40000000, 0x00000000,	REG,	XSIZE31|XS,	XRD0, XR5, XR16},//x 0 S 000000
	{"sbc",		0x40000000, 0x40000000,	REG,	XSIZE31|XS,	XRD0, XR5, XR16},//x 1 S 000000
"addsub_carry"};

//C4.5.4 Conditional compare (immediate)
OP arm64_condcmp_imm[] = {
	{"ccmn",	0x00000400, 0x00000000, REG,	XSIZE31|XS,	XRD5, XIMM16_5, XFLAGS, XCOND},//0 0 1 0 0 CCMN
	{"ccmp",	0x00000400, 0x00000400, REG,	XSIZE31|XS,	XRD5, XIMM16_5, XFLAGS, XCOND},//0 1 1 0 0 CCMP
"condcmp_imm"};

//C4.5.5 Conditional compare (register)
OP arm64_condcmp_reg[] = {
	{"ccmn",	0x00000400, 0x00000000, REG,	XSIZE31|XS,	XRD5, XR16, XFLAGS, XCOND},//0 0 1 0 0
	{"ccmp",	0x00000400, 0x00000400, REG,	XSIZE31|XS,	XRD5, XR16, XFLAGS, XCOND},//0 1 1 0 0
"condcmp_reg"};

//C4.5.6 Conditional select
OP arm64_condsel[] = {
	{"csel",	0x40000400, 0x00000000, REG,	XSIZE31,	XRD0, XR5, XR16, XCOND},//x 0 0 00
	{"csinc",	0x40000400, 0x00000400, REG,	XSIZE31,	XRD0, XR5, XR16, XCOND},//x 0 0 01
	{"csinv",	0x40000400, 0x40000000, REG,	XSIZE31,	XRD0, XR5, XR16, XCOND},//x 1 0 00
	{"csneg",	0x40000400, 0x40000400, REG,	XSIZE31,	XRD0, XR5, XR16, XCOND},//x 1 0 01
"condsel"};

//C4.5.7 Data-processing (1 source)
OP arm64_data_1source[] = {
	{"rbit",	0x00070000, 0x00000000, REG,	XSIZE31,	XRD0, XR5},//x 0 00000 000000
	{"rev16",	0x00070000, 0x00010000, REG,	XSIZE31,	XRD0, XR5},//x 0 00000 000001
	{"rev",		0x80070000, 0x00020000, REG,	XSIZE31,	XRD0, XR5},//x 0 00000 000010
	{"clz",		0x00070000, 0x00040000, REG,	XSIZE31,	XRD0, XR5},//x 0 00000 000100
	{"cls",		0x00070000, 0x00050000, REG,	XSIZE31,	XRD0, XR5},//x 0 00000 000101
	{"rev32",	0x80070000, 0x80020000, REG,	XSIZE31,	XRD0, XR5},//1 0 00000 000010
	{"rev",		0x80070000, 0x80030000, REG,	XSIZE31,	XRD0, XR5},//1 0 00000 000011
0};

//C4.5.8 Data-processing (2 source)
OP arm64_data_2source[] = {
	{"udiv",	0x00007c00, 0x00000800, REG,	XSIZE31,	XRD0, XR5, XR16},//x 0 000010
	{"sdiv",	0x00007c00, 0x00000c00, REG,	XSIZE31,	XRD0, XR5, XR16},//x 0 000011
	{"lslv",	0x00007c00, 0x00002000, REG,	XSIZE31,	XRD0, XR5, XR16},//x 0 001000
	{"lsrv",	0x00007c00, 0x00002400, REG,	XSIZE31,	XRD0, XR5, XR16},//x 0 001001
	{"asrv",	0x00007c00, 0x00002800, REG,	XSIZE31,	XRD0, XR5, XR16},//x 0 001010
	{"rorv",	0x00007c00, 0x00002c00, REG,	XSIZE31,	XRD0, XR5, XR16},//x 0 001011
	{"crc32b",	0x00007c00, 0x00004000, REG,	XSIZE31,	XRD0, XR5, XR16},//0 0 010000
	{"crc32h",	0x00007c00, 0x00004400, REG,	XSIZE31,	XRD0, XR5, XR16},//0 0 010001
	{"crc32w",	0x00007c00, 0x00004800, REG,	XSIZE31,	XRD0, XR5, XR16},//0 0 010010
	{"crc32x",	0x80007c00, 0x80004c00, REG,	XSIZE31,	XRD0, XR5, XR16},//0 0 010100
	{"crc32cb",	0x00007c00, 0x00005000, REG,	XSIZE31,	XRD0, XR5, XR16},//0 0 010101
	{"crc32ch",	0x00007c00, 0x00005400, REG,	XSIZE31,	XRD0, XR5, XR16},//0 0 010110
	{"crc32cw",	0x00007c00, 0x00005c00, REG,	XSIZE31,	XRD0, XR5, XR16},//1 0 010011
	{"crc32cx",	0x80007c00, 0x80005c00, REG,	XSIZE31,	XRD0, XR5, XR16},//1 0 010111
0};

//C4.5.9 Data-processing (3 source)
OP arm64_data_3source[] = {
	{"madd",	0x00e08000, 0x00000000, REG,	XSIZE31,	XRD0, XR5, XR16, XR10},//x 00 000 0
	{"msub",	0x00e08000, 0x00008000, REG,	XSIZE31,	XRD0, XR5, XR16, XR10},//x 00 000 1
	{"smaddl",	0x80e08000, 0x80200000, REG,	XSIZE31,	XRD0, XR5, XR16, XR10},//1 00 001 0
	{"smsubl",	0x80e08000, 0x80208000, REG,	XSIZE31,	XRD0, XR5, XR16, XR10},//1 00 001 1
	{"smulh ",	0x80e08000, 0x80400000, REG,	XSIZE31,	XRD0, XR5, XR16, XR10},//1 00 010 0
	{"umaddl",	0x80e08000, 0x80a00000, REG,	XSIZE31,	XRD0, XR5, XR16, XR10},//1 00 101 0
	{"umsubl",	0x80e08000, 0x80a08000, REG,	XSIZE31,	XRD0, XR5, XR16, XR10},//1 00 101 1
	{"umulh",	0x80e08000, 0x80c00000, REG,	XSIZE31,	XRD0, XR5, XR16, XR10},//1 00 110 0
0};

//
//C4.5.10 Logical (shifted register)
OP arm64_logical_shifted[] = {
	{"and",		0x60100000, 0x00000000,	REG,	XSIZE31,	XRD0, XR5, XR16, XSHIFTED},	//x 00 0
	{"bic",		0x60100000, 0x00100000,	REG,	XSIZE31,	XRD0, XR5, XR16, XSHIFTED},	//x 00 1
	{"orr",		0x60100000, 0x20000000,	REG,	XSIZE31,	XRD0, XR5, XR16, XSHIFTED},	//x 01 0
	{"orn",		0x60100000, 0x20100000,	REG,	XSIZE31,	XRD0, XR5, XR16, XSHIFTED},	//x 01 1
	{"eor",		0x60100000, 0x40000000,	REG,	XSIZE31,	XRD0, XR5, XR16, XSHIFTED},	//x 10 0
	{"eon",		0x60100000, 0x40100000,	REG,	XSIZE31,	XRD0, XR5, XR16, XSHIFTED},	//x 10 1
	{"ands",	0x60100000, 0x60000000,	REG,	XSIZE31,	XRD0, XR5, XR16, XSHIFTED},	//x 11 0
	{"bics",	0x60100000, 0x60100000,	REG,	XSIZE31,	XRD0, XR5, XR16, XSHIFTED},	//x 11 1
0};

OP arm64_data_reg[] = {
	{(char*)arm64_logical_shifted,	0x1f000000, 0x0a000000,	TABLE	},//- - - 0 1 0 1 0 - - - - - - - - - - - - - - Logical (shifted register)
	{(char*)arm64_addsub_shifted,	0x1f200000, 0x0b000000,	TABLE	},//- - - 0 1 0 1 1 - - 0 - - - - - - - - - - - Add/subtract (shifted register)
	{(char*)arm64_addsub_extended,	0x1f200000, 0x0b200000,	TABLE	},//- - - 0 1 0 1 1 - - 1 - - - - - - - - - - - Add/subtract (extended register)
	{(char*)arm64_addsub_carry,		0x1fe00000, 0x1a000000,	TABLE	},//- - - 1 1 0 1 0 0 0 0 - - - - - - - - - - - Add/subtract (with carry)
	{(char*)arm64_condcmp_reg,		0x1fe00800, 0x1a400000,	TABLE	},//- - - 1 1 0 1 0 0 1 0 - - - - - - - - - 0 - Conditional compare (register)
	{(char*)arm64_condcmp_imm,		0x1fe00800, 0x1a400800,	TABLE	},//- - - 1 1 0 1 0 0 1 0 - - - - - - - - - 1 - Conditional compare (immediate)
	{(char*)arm64_condsel,			0x1fe00000, 0x1a800000,	TABLE	},//- - - 1 1 0 1 0 1 0 0 - - - - - - - - - - - Conditional select
	{(char*)arm64_data_3source,		0x1f000000, 0x1b000000,	TABLE	},//- - - 1 1 0 1 1 - - - - - - - - - - - - - - Data-processing (3 source)
	{(char*)arm64_data_2source,		0x5fe00000, 0x1ac00000,	TABLE	},//- 0 - 1 1 0 1 0 1 1 0 - - - - - - - - - - - Data-processing (2 source)
	{(char*)arm64_data_1source,		0x5fe00000, 0x5ac00000,	TABLE	},//- 1 - 1 1 0 1 0 1 1 0 - - - - - - - - - - - Data-processing (1 source)
"data_reg",		0};

//-----------------------------------------------------------------------------
//	Advanced SIMD
//-----------------------------------------------------------------------------

//C4.6.1 Advanced SIMD across lanes
OP arm64_SIMD_across[] = {
{"saddlv",		0x2001f000, 0x00003000,	0,	XSIMD,	XRD0,	XR5},	//0 -	0	0011
{"smaxv",		0x2001f000, 0x0001a000,	0,	XSIMD,	XRD0,	XR5},	//0 -	0	1010
{"sminv",		0x2001f000, 0x0001a000,	0,	XSIMD,	XRD0,	XR5},	//0 -	1	1010
{"addv",		0x2001f000, 0x0001b000,	0,	XSIMD,	XRD0,	XR5},	//0 -	1	1011
{"uaddlv",		0x2001f000, 0x20003000,	0,	XSIMD,	XRD0,	XR5},	//1 -	0	0011
{"umaxv",		0x2001f000, 0x2001a000,	0,	XSIMD,	XRD0,	XR5},	//1 -	0	1010
{"uminv",		0x2001f000, 0x2001a000,	0,	XSIMD,	XRD0,	XR5},	//1 -	1	1010
{"fmaxnmv",		0x2081f000, 0x2001c000,	0,	XSIMD,	XRD0,	XR5},	//1 0x	0	1100
{"fmaxv",		0x2081f000, 0x2001f000,	0,	XSIMD,	XRD0,	XR5},	//1 0x	0	1111
{"fminnmv",		0x2081f000, 0x2081c000,	0,	XSIMD,	XRD0,	XR5},	//1 1x	0	1100
{"fminv",		0x2081f000, 0x2081f000,	0,	XSIMD,	XRD0,	XR5},	//1 1x	0	1111
0};

//C4.6.2 Advanced SIMD copy
OP arm64_SIMD_copy[] = {
{"dup",			0x20007800, 0x00000000,	0,	XSIMD,	XRD0,	XR5},	//- 0 - 0000
{"dup",			0x20007800, 0x00000800,	0,	XSIMD,	XRD0,	XR5},	//- 0 - 0001
{"smov",		0x60007800, 0x00002800,	0,	XSIMD,	XRD0,	XR5},	//0 0 - 0101
{"umov",		0x60007800, 0x00003800,	0,	XSIMD,	XRD0,	XR5},	//0 0 - 0111
{"ins",			0x60007800, 0x40001800,	0,	XSIMD,	XRD0,	XR5},	//1 0 - 0011
{"smov",		0x60007800, 0x40002800,	0,	XSIMD,	XRD0,	XR5},	//1 0 - 0101
{"umov",		0x60007800, 0x40003800,	0,	XSIMD,	XRD0,	XR5},	//1 0 - 0111
{"ins",			0x60007800, 0x60000000,	0,	XSIMD,	XRD0,	XR5},	//1 1 - -
0};

//C4.6.4 Advanced SIMD modified immediate
OP arm64_SIMD_mod_imm[] = {
{"movi",		0x0000f000, 0x00000000,	0,	XSIMD,	XRD0,	XSHIFT_IMM},	//- 0 0xx0 0
{"orr",			0x0000f000, 0x00000000,	0,	XSIMD,	XRD0,	XSHIFT_IMM},	//- 0 0xx1 0
{"movi",		0x0000f000, 0x00000000,	0,	XSIMD,	XRD0,	XSHIFT_IMM},	//- 0 10x0 0
{"orr",			0x0000f000, 0x00000000,	0,	XSIMD,	XRD0,	XSHIFT_IMM},	//- 0 10x1 0
{"movi",		0x0000f000, 0x00000000,	0,	XSIMD,	XRD0,	XSHIFT_IMM},	//- 0 110x 0
{"movi",		0x0000f000, 0x00000000,	0,	XSIMD,	XRD0,	XSHIFT_IMM},	//- 0 1110 0
{"fmov",		0x0000f000, 0x00000000,	0,	XSIMD,	XRD0,	XSHIFT_IMM},	//- 0 1111 0
{"mvni",		0x0000f000, 0x00000000,	0,	XSIMD,	XRD0,	XSHIFT_IMM},	//- 1 0xx0 0
{"bic",			0x0000f000, 0x00000000,	0,	XSIMD,	XRD0,	XSHIFT_IMM},	//- 1 0xx1 0
{"mvni",		0x0000f000, 0x00000000,	0,	XSIMD,	XRD0,	XSHIFT_IMM},	//- 1 10x0 0
{"bic",			0x0000f000, 0x00000000,	0,	XSIMD,	XRD0,	XSHIFT_IMM},	//- 1 10x1 0
{"mvni",		0x0000f000, 0x00000000,	0,	XSIMD,	XRD0,	XSHIFT_IMM},	//- 1 110x 0
{"movi",		0x0000f000, 0x00000000,	0,	XSIMD,	XRD0,	XSHIFT_IMM},	//0 1 1110 0
{"movi",		0x0000f000, 0x00000000,	0,	XSIMD,	XRD0,	XSHIFT_IMM},	//1 1 1110 0
{"fmov",		0x0000f000, 0x00000000,	0,	XSIMD,	XRD0,	XSHIFT_IMM},	//1 1 1111 0
0};

//C4.6.5 Advanced SIMD permute
OP arm64_SIMD_permute[] = {
{"uzp1",		0x00007000, 0x00001000,	0,	XSIMD,	XRD0,	XR5},	//001
{"trn1",		0x00007000, 0x00002000,	0,	XSIMD,	XRD0,	XR5},	//010
{"zip1",		0x00007000, 0x00003000,	0,	XSIMD,	XRD0,	XR5},	//011
{"uzp2",		0x00007000, 0x00005000,	0,	XSIMD,	XRD0,	XR5},	//101
{"trn2",		0x00007000, 0x00006000,	0,	XSIMD,	XRD0,	XR5},	//110
{"zip2",		0x00007000, 0x00007000,	0,	XSIMD,	XRD0,	XR5},	//111
0};

//C4.6.7 Advanced SIMD scalar pairwise
OP arm64_SIMD_scalar_pairwise[] = {
{"addp",		0x2001f000, 0x0001f000,	0,	XSIMD,	XRD0,	XR5},	//0 -  1 1011
{"fmaxnmp",		0x2081f000, 0x2000c000,	0,	XSIMD,	XRD0,	XR5},	//1 0x 0 1100
{"faddp",		0x2081f000, 0x2000d000,	0,	XSIMD,	XRD0,	XR5},	//1 0x 0 1101
{"fmaxp",		0x2081f000, 0x2000f000,	0,	XSIMD,	XRD0,	XR5},	//1 0x 0 1111
{"fminnmp",		0x2081f000, 0x2080c000,	0,	XSIMD,	XRD0,	XR5},	//1 1x 0 1100
{"fminp",		0x2081f000, 0x2080f000,	0,	XSIMD,	XRD0,	XR5},	//1 1x 0 1111
0};

//C4.6.8 Advanced SIMD scalar shift by immediate
OP arm64_SIMD_scalar_shift_imm[] = {
{"sshr",		0x2000f800, 0x00000000,	0,	XSIMD,	XRD0,	XR5, XIMM16_7	},	//0 0000 0
{"ssra",		0x2000f800, 0x00001000,	0,	XSIMD,	XRD0,	XR5, XIMM16_7	},	//0 0001 0
{"srshr",		0x2000f800, 0x00002000,	0,	XSIMD,	XRD0,	XR5, XIMM16_7	},	//0 0010 0
{"srsra",		0x2000f800, 0x00003000,	0,	XSIMD,	XRD0,	XR5, XIMM16_7	},	//0 0011 0
{"shl",			0x2000f800, 0x00005000,	0,	XSIMD,	XRD0,	XR5, XIMM16_7	},	//0 0101 0
{"sqshl",		0x2000f800, 0x00007000,	0,	XSIMD,	XRD0,	XR5, XIMM16_7	},	//0 0111 0
{"sqshrn,",		0x2000f800, 0x00009000,	0,	XSIMD,	XRD0,	XR5, XIMM16_7	},	//0 1001 0
{"sqrshrn,",	0x2000f800, 0x00009800,	0,	XSIMD,	XRD0,	XR5, XIMM16_7	},	//0 1001 1
{"scvtf",		0x2000f800, 0x0000e000,	0,	XSIMD,	XRD0,	XR5, XIMM16_7	},	//0 1110 0
{"fcvtzs",		0x2000f800, 0x0000f800,	0,	XSIMD,	XRD0,	XR5, XIMM16_7	},	//0 1111 1
{"ushr",		0x2000f800, 0x20000000,	0,	XSIMD,	XRD0,	XR5, XIMM16_7	},	//1 0000 0
{"usra",		0x2000f800, 0x20001000,	0,	XSIMD,	XRD0,	XR5, XIMM16_7	},	//1 0001 0
{"urshr",		0x2000f800, 0x20002000,	0,	XSIMD,	XRD0,	XR5, XIMM16_7	},	//1 0010 0
{"ursra",		0x2000f800, 0x20003000,	0,	XSIMD,	XRD0,	XR5, XIMM16_7	},	//1 0011 0
{"sri",			0x2000f800, 0x20004000,	0,	XSIMD,	XRD0,	XR5, XIMM16_7	},	//1 0100 0
{"sli",			0x2000f800, 0x20005000,	0,	XSIMD,	XRD0,	XR5, XIMM16_7	},	//1 0101 0
{"sqshlu",		0x2000f800, 0x20006000,	0,	XSIMD,	XRD0,	XR5, XIMM16_7	},	//1 0110 0
{"uqshl",		0x2000f800, 0x20007000,	0,	XSIMD,	XRD0,	XR5, XIMM16_7	},	//1 0111 0
{"sqshrun,",	0x2000f800, 0x20008000,	0,	XSIMD,	XRD0,	XR5, XIMM16_7	},	//1 1000 0
{"sqrshrun, ",	0x2000f800, 0x20009800,	0,	XSIMD,	XRD0,	XR5, XIMM16_7	},	//1 1000 1
{"uqshrn,",		0x2000f800, 0x20009000,	0,	XSIMD,	XRD0,	XR5, XIMM16_7	},	//1 1001 0
{"uqrshrn,",	0x2000f800, 0x20009800,	0,	XSIMD,	XRD0,	XR5, XIMM16_7	},	//1 1001 1
{"ucvtf",		0x2000f800, 0x2000e000,	0,	XSIMD,	XRD0,	XR5, XIMM16_7	},	//1 1110 0
{"fcvtzu",		0x2000f800, 0x2000f800,	0,	XSIMD,	XRD0,	XR5, XIMM16_7	},	//1 1111 1
0};

//c4.6.9 Advanced SIMD scalar three different
OP arm64_SIMD_scalar_diff3[] = {
{"sqdmlal",		0x0000f000, 0x00009000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 1001	SQDMLAL2
{"sqdmlsl",		0x0000f000, 0x0000b000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 1011	SQDMLSL2
{"sqdmull",		0x0000f000, 0x0000d000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 1101	SQDMULL2
0};

//C4.6.10 Advanced SIMD scalar three same
OP arm64_SIMD_scalar_same3[] = {
{"sqadd",		0x2000f800, 0x00000800,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 -	0000 1
{"sqsub",		0x2000f800, 0x00002800,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 -	0010 1
{"cmgt",		0x2000f800, 0x00003000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 -	0011 0
{"cmge",		0x2000f800, 0x00003800,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 -	0011 1
{"sshl",		0x2000f800, 0x00004000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 -	0100 0
{"sqshl",		0x2000f800, 0x00004800,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 -	0100 1
{"srshl",		0x2000f800, 0x00005000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 -	0101 0
{"sqrshl",		0x2000f800, 0x00005800,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 -	0101 1
{"add",			0x2000f800, 0x00008000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 -	1000 0
{"cmtst",		0x2000f800, 0x00008800,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 -	1000 1
{"sqdmulh",		0x2000f800, 0x0000b000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 -	1011 0
{"fmulx",		0x2080f800, 0x0000d800,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 0x	1101 1
{"fcmeq",		0x2080f800, 0x0000e000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 0x	1110 0
{"frecps",		0x2080f800, 0x0000f800,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 0x	1111 1
{"frsqrts",		0x2080f800, 0x0080f800,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 1x	1111 1
{"uqadd",		0x2000f800, 0x20000800,	0,	XSIMD,	XRD0,	XR5, XR16	},	//1 -	0000 1
{"uqsub",		0x2000f800, 0x20002800,	0,	XSIMD,	XRD0,	XR5, XR16	},	//1 -	0010 1
{"cmhi",		0x2000f800, 0x20003000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//1 -	0011 0
{"cmhs",		0x2000f800, 0x20003800,	0,	XSIMD,	XRD0,	XR5, XR16	},	//1 -	0011 1
{"ushl",		0x2000f800, 0x20004000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//1 -	0100 0
{"uqshl",		0x2000f800, 0x20004800,	0,	XSIMD,	XRD0,	XR5, XR16	},	//1 -	0100 1
{"urshl",		0x2000f800, 0x20005000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//1 -	0101 0
{"uqrshl",		0x2000f800, 0x20005800,	0,	XSIMD,	XRD0,	XR5, XR16	},	//1 -	0101 1
{"sub",			0x2000f800, 0x20008000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//1 -	1000 0
{"cmeq",		0x2000f800, 0x20008800,	0,	XSIMD,	XRD0,	XR5, XR16	},	//1 -	1000 1
{"sqrdmulh",	0x2000f800, 0x2000b000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//1 -	1011 0
{"fcmge",		0x2080f800, 0x2000e000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//1 0x	1110 0
{"facge",		0x2080f800, 0x2000e800,	0,	XSIMD,	XRD0,	XR5, XR16	},	//1 0x	1110 1
{"fabd",		0x2080f800, 0x2080d000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//1 1x	1101 0
{"fcmgt",		0x2080f800, 0x2080e000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//1 1x	1110 0
{"facgt",		0x2080f800, 0x2080e800,	0,	XSIMD,	XRD0,	XR5, XR16	},	//1 1x	1110 1
0};

//C4.6.11 Advanced SIMD scalar two-register miscellaneous
OP arm64_SIMD_scalar_misc2[] = {
{"suqadd",		0x2001f000, 0x00003000,	0,	XSIMD,	XRD0,	XR5	},	//0 -	0 0011
{"sqabs",		0x2001f000, 0x00007000,	0,	XSIMD,	XRD0,	XR5	},	//0 -	0 0111
{"cmgt",		0x2001f000, 0x00008000,	0,	XSIMD,	XRD0,	XR5	},	//0 -	0 1000
{"cmeq",		0x2001f000, 0x00009000,	0,	XSIMD,	XRD0,	XR5	},	//0 -	0 1001
{"cmlt",		0x2001f000, 0x0000a000,	0,	XSIMD,	XRD0,	XR5	},	//0 -	0 1010
{"abs",			0x2001f000, 0x0000b000,	0,	XSIMD,	XRD0,	XR5	},	//0 -	0 1011
{"sqxtn",		0x2001f000, 0x00014000,	0,	XSIMD,	XRD0,	XR5	},	//0 -	1 0100 SQXTN2
{"fcvtns",		0x2081f000, 0x0001a000,	0,	XSIMD,	XRD0,	XR5	},	//0 0x	1 1010
{"fcvtms",		0x2081f000, 0x0001b000,	0,	XSIMD,	XRD0,	XR5	},	//0 0x	1 1011
{"fcvtas",		0x2081f000, 0x0001c000,	0,	XSIMD,	XRD0,	XR5	},	//0 0x	1 1100
{"scvtf",		0x2081f000, 0x0001d000,	0,	XSIMD,	XRD0,	XR5	},	//0 0x	1 1101
{"fcmgt",		0x2081f000, 0x0080c000,	0,	XSIMD,	XRD0,	XR5	},	//0 1x	0 1100
{"fcmeq",		0x2081f000, 0x0080d000,	0,	XSIMD,	XRD0,	XR5	},	//0 1x	0 1101
{"fcmlt",		0x2081f000, 0x0080e000,	0,	XSIMD,	XRD0,	XR5	},	//0 1x	0 1110
{"fcvtps",		0x2081f000, 0x0081a000,	0,	XSIMD,	XRD0,	XR5	},	//0 1x	1 1010
{"fcvtzs",		0x2081f000, 0x0081b000,	0,	XSIMD,	XRD0,	XR5	},	//0 1x	1 1011
{"frecpe",		0x2081f000, 0x0081d000,	0,	XSIMD,	XRD0,	XR5	},	//0 1x	1 1101
{"frecpx",		0x2081f000, 0x0081f000,	0,	XSIMD,	XRD0,	XR5	},	//0 1x	1 1111
{"usqadd",		0x2001f000, 0x20003000,	0,	XSIMD,	XRD0,	XR5	},	//1 -	0 0011
{"sqneg",		0x2001f000, 0x20007000,	0,	XSIMD,	XRD0,	XR5	},	//1 -	0 0111
{"cmge",		0x2001f000, 0x20008000,	0,	XSIMD,	XRD0,	XR5	},	//1 -	0 1000
{"cmle",		0x2001f000, 0x20009000,	0,	XSIMD,	XRD0,	XR5	},	//1 -	0 1001
{"neg",			0x2001f000, 0x2000b000,	0,	XSIMD,	XRD0,	XR5	},	//1 -	0 1011
{"sqxtun",		0x2001f000, 0x20012000,	0,	XSIMD,	XRD0,	XR5	},	//1 -	1 0010 SQXTUN2
{"uqxtn",		0x2001f000, 0x20014000,	0,	XSIMD,	XRD0,	XR5	},	//1 -	1 0100 UQXTN2
{"fcvtxn",		0x2081f000, 0x20016000,	0,	XSIMD,	XRD0,	XR5	},	//1 0x	1 0110 FCVTXN2
{"fcvtnu",		0x2081f000, 0x2001a000,	0,	XSIMD,	XRD0,	XR5	},	//1 0x	1 1010
{"fcvtmu",		0x2081f000, 0x2001b000,	0,	XSIMD,	XRD0,	XR5	},	//1 0x	1 1011
{"fcvtau",		0x2081f000, 0x2001c000,	0,	XSIMD,	XRD0,	XR5	},	//1 0x	1 1100
{"ucvtf",		0x2081f000, 0x2001d000,	0,	XSIMD,	XRD0,	XR5	},	//1 0x	1 1101
{"fcmge",		0x2081f000, 0x2080c000,	0,	XSIMD,	XRD0,	XR5	},	//1 1x	0 1100
{"fcmle",		0x2081f000, 0x2080d000,	0,	XSIMD,	XRD0,	XR5	},	//1 1x	0 1101
{"fcvtpu",		0x2081f000, 0x2081a000,	0,	XSIMD,	XRD0,	XR5	},	//1 1x	1 1010
{"fcvtzu",		0x2081f000, 0x2081b000,	0,	XSIMD,	XRD0,	XR5	},	//1 1x	1 1011
{"frsqrte",		0x2081f000, 0x2081d000,	0,	XSIMD,	XRD0,	XR5	},	//1 1x	1 1101
0};

//C4.6.12 Advanced SIMD scalar x indexed element
OP arm64_SIMD_scalar_mul_indexed[] = {
{"sqdmlal",		0x2000f000, 0x00003000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 -	0011	SQDMLAL2
{"sqdmlsl",		0x2000f000, 0x00007000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 -	0111	SQDMLSL2
{"sqdmull",		0x2000f000, 0x0000b000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 -	1011	SQDMULL2
{"sqdmulh",		0x2000f000, 0x0000c000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 -	1100
{"sqrdmulh",	0x2000f000, 0x0000d000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 -	1101
{"fmla",		0x2080f000, 0x00801000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 1x	0001
{"fmls",		0x2080f000, 0x00805000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 1x	0101
{"fmul",		0x2080f000, 0x00809000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 1x	1001
{"fmulx",		0x2080f000, 0x20809000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//1 1x	1001
0};

//C4.6.13 Advanced SIMD shift by immediate
OP arm64_SIMD_shift_imm[] = {
{"sshr",		0x2000f800, 0x00000000,	0,	XSIMD,	XRD0,	XR5, XIMM16_7	},	//0 0000 0
{"ssra",		0x2000f800, 0x00001000,	0,	XSIMD,	XRD0,	XR5, XIMM16_7	},	//0 0001 0
{"srshr",		0x2000f800, 0x00002000,	0,	XSIMD,	XRD0,	XR5, XIMM16_7	},	//0 0010 0
{"srsra",		0x2000f800, 0x00003000,	0,	XSIMD,	XRD0,	XR5, XIMM16_7	},	//0 0011 0
{"shl",			0x2000f800, 0x00005000,	0,	XSIMD,	XRD0,	XR5, XIMM16_7	},	//0 0101 0
{"sqshl",		0x2000f800, 0x00007000,	0,	XSIMD,	XRD0,	XR5, XIMM16_7	},	//0 0111 0
{"shrn",		0x2000f800, 0x00008000,	0,	XSIMD,	XRD0,	XR5, XIMM16_7	},	//0 1000 0	SHRN2
{"rshrn",		0x2000f800, 0x00008800,	0,	XSIMD,	XRD0,	XR5, XIMM16_7	},	//0 1000 1	RSHRN2
{"sqshrn",		0x2000f800, 0x00009000,	0,	XSIMD,	XRD0,	XR5, XIMM16_7	},	//0 1001 0	SQSHRN2
{"sqrshrn",		0x2000f800, 0x00009800,	0,	XSIMD,	XRD0,	XR5, XIMM16_7	},	//0 1001 1	SQRSHRN2
{"sshll",		0x2000f800, 0x0000a000,	0,	XSIMD,	XRD0,	XR5, XIMM16_7	},	//0 1010 0	SSHLL2
{"scvtf",		0x2000f800, 0x0000e000,	0,	XSIMD,	XRD0,	XR5, XIMM16_7	},	//0 1110 0
{"fcvtzs",		0x2000f800, 0x0000f800,	0,	XSIMD,	XRD0,	XR5, XIMM16_7	},	//0 1111 1
{"ushr",		0x2000f800, 0x20000000,	0,	XSIMD,	XRD0,	XR5, XIMM16_7	},	//1 0000 0
{"usra",		0x2000f800, 0x20001000,	0,	XSIMD,	XRD0,	XR5, XIMM16_7	},	//1 0001 0
{"urshr",		0x2000f800, 0x20002000,	0,	XSIMD,	XRD0,	XR5, XIMM16_7	},	//1 0010 0
{"ursra",		0x2000f800, 0x20003000,	0,	XSIMD,	XRD0,	XR5, XIMM16_7	},	//1 0011 0
{"sri",			0x2000f800, 0x20004000,	0,	XSIMD,	XRD0,	XR5, XIMM16_7	},	//1 0100 0
{"sli",			0x2000f800, 0x20005000,	0,	XSIMD,	XRD0,	XR5, XIMM16_7	},	//1 0101 0
{"sqshlu",		0x2000f800, 0x20006000,	0,	XSIMD,	XRD0,	XR5, XIMM16_7	},	//1 0110 0
{"uqshl",		0x2000f800, 0x20007000,	0,	XSIMD,	XRD0,	XR5, XIMM16_7	},	//1 0111 0
{"sqshrun",		0x2000f800, 0x20008000,	0,	XSIMD,	XRD0,	XR5, XIMM16_7	},	//1 1000 0	SQSHRUN2
{"sqrshrun",	0x2000f800, 0x20008800,	0,	XSIMD,	XRD0,	XR5, XIMM16_7	},	//1 1000 1	SQRSHRUN2
{"uqshrn",		0x2000f800, 0x20009000,	0,	XSIMD,	XRD0,	XR5, XIMM16_7	},	//1 1001 0	UQSHRN2
{"uqrshrn",		0x2000f800, 0x20009800,	0,	XSIMD,	XRD0,	XR5, XIMM16_7	},	//1 1001 1	UQRSHRN2
{"ushll",		0x2000f800, 0x2000a000,	0,	XSIMD,	XRD0,	XR5, XIMM16_7	},	//1 1010 0	USHLL2
{"ucvtf",		0x2000f800, 0x2000e000,	0,	XSIMD,	XRD0,	XR5, XIMM16_7	},	//1 1110 0
{"fcvtzu",		0x2000f800, 0x2000f800,	0,	XSIMD,	XRD0,	XR5, XIMM16_7	},	//1 1111 1
0};

//C4.6.14 Advanced SIMD table lookup
OP arm64_SIMD_table[] = {
{"tbl",			0x00c07000, 0x00000000,	},	//00 00 0
{"tbx",			0x00c07000, 0x00001000,	},	//00 00 1
{"tbl",			0x00c07000, 0x00002000,	},	//00 01 0
{"tbx",			0x00c07000, 0x00003000,	},	//00 01 1
{"tbl",			0x00c07000, 0x00004000,	},	//00 10 0
{"tbx",			0x00c07000, 0x00005000,	},	//00 10 1
{"tbl",			0x00c07000, 0x00006000,	},	//00 11 0
{"tbx",			0x00c07000, 0x00007000,	},	//00 11 1
0};

//C4.6.15 Advanced SIMD three different
OP arm64_SIMD_diff3[] = {
{"saddl",		0x2000f000, 0x00000000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 0000	SADDL2
{"saddw",		0x2000f000, 0x00001000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 0001	SADDW2
{"ssubl",		0x2000f000, 0x00002000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 0010	SSUBL2
{"ssubw",		0x2000f000, 0x00003000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 0011	SSUBW2
{"addhn",		0x2000f000, 0x00004000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 0100	ADDHN2
{"sabal",		0x2000f000, 0x00005000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 0101	SABAL2
{"subhn",		0x2000f000, 0x00006000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 0110	SUBHN2
{"sabdl",		0x2000f000, 0x00007000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 0111	SABDL2
{"smlal",		0x2000f000, 0x00008000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 1000	SMLAL2
{"sqdmlal",		0x2000f000, 0x00009000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 1001	SQDMLAL2
{"smlsl",		0x2000f000, 0x0000a000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 1010	SMLSL2
{"sqdmlsl",		0x2000f000, 0x0000b000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 1011	SQDMLSL2
{"smull",		0x2000f000, 0x0000c000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 1100	SMULL2
{"sqdmull",		0x2000f000, 0x0000d000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 1101	SQDMULL2
{"pmull",		0x2000f000, 0x0000e000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 1110	PMULL2
{"uaddl",		0x2000f000, 0x20000000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//1 0000	UADDL2
{"uaddw",		0x2000f000, 0x20001000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//1 0001	UADDW2
{"usubl",		0x2000f000, 0x20002000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//1 0010	USUBL2
{"usubw",		0x2000f000, 0x20003000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//1 0011	USUBW2
{"raddhn",		0x2000f000, 0x20004000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//1 0100	RADDHN2
{"uabal",		0x2000f000, 0x20005000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//1 0101	UABAL2
{"rsubhn",		0x2000f000, 0x20006000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//1 0110	RSUBHN2
{"uabdl",		0x2000f000, 0x20007000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//1 0111	UABDL2
{"umlal",		0x2000f000, 0x20008000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//1 1000	UMLAL2
{"umlsl",		0x2000f000, 0x2000a000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//1 1010	UMLSL2
{"umull",		0x2000f000, 0x2000c000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//1 1100	UMULL2
0};

//C4.6.16 Advanced SIMD three same
OP arm64_SIMD_same3[] = {
{"shadd",		0x2000f800, 0x00000000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 -	0000 0
{"sqadd",		0x2000f800, 0x00000800,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 -	0000 1
{"srhadd",		0x2000f800, 0x00001000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 -	0001 0
{"shsub",		0x2000f800, 0x00002000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 -	0010 0
{"sqsub",		0x2000f800, 0x00002800,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 -	0010 1
{"cmgt",		0x2000f800, 0x00003000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 -	0011 0
{"cmge",		0x2000f800, 0x00003800,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 -	0011 1
{"sshl",		0x2000f800, 0x00004000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 -	0100 0
{"sqshl",		0x2000f800, 0x00004800,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 -	0100 1
{"srshl",		0x2000f800, 0x00005000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 -	0101 0
{"sqrshl",		0x2000f800, 0x00005800,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 -	0101 1
{"smax",		0x2000f800, 0x00006000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 -	0110 0
{"smin",		0x2000f800, 0x00006800,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 -	0110 1
{"sabd",		0x2000f800, 0x00007000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 -	0111 0
{"saba",		0x2000f800, 0x00007800,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 -	0111 1
{"add",			0x2000f800, 0x00008000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 -	1000 0
{"cmtst",		0x2000f800, 0x00008800,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 -	1000 1
{"mla",			0x2000f800, 0x00009000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 -	1001 0
{"mul",			0x2000f800, 0x00009800,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 -	1001 1
{"smaxp",		0x2000f800, 0x0000a000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 -	1010 0
{"sminp",		0x2000f800, 0x0000a800,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 -	1010 1
{"sqdmulh",		0x2000f800, 0x0000b000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 -	1011 0
{"addp",		0x2000f800, 0x0000b800,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 -	1011 1
{"fmaxnm",		0x2080f800, 0x0000c000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 0x	1100 0
{"fmla",		0x2080f800, 0x0000c800,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 0x	1100 1
{"fadd",		0x2080f800, 0x0000d000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 0x	1101 0
{"fmulx",		0x2080f800, 0x0000d800,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 0x	1101 1
{"fcmeq",		0x2080f800, 0x0000e000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 0x	1110 0
{"fmax",		0x2080f800, 0x0000f000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 0x	1111 0
{"frecps",		0x2080f800, 0x0000f800,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 0x	1111 1
{"and",			0x20c0f800, 0x00001800,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 00	0001 1
{"bic",			0x20c0f800, 0x00401800,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 01	0001 1
{"fminnm",		0x2080f800, 0x0080c000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 1x	1100 0
{"fmls",		0x2080f800, 0x0080c800,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 1x	1100 1
{"fsub",		0x2080f800, 0x0080d000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 1x	1101 0
{"fmin",		0x2080f800, 0x0080f000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 1x	1111 0
{"frsqrts",		0x2080f800, 0x0080f800,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 1x	1111 1
{"orr",			0x20c0f800, 0x00801800,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 10	0001 1
{"orn",			0x20c0f800, 0x00c01800,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 11	0001 1
{"uhadd",		0x2000f800, 0x20000000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//1 -	0000 0
{"uqadd",		0x2000f800, 0x20000800,	0,	XSIMD,	XRD0,	XR5, XR16	},	//1 -	0000 1
{"urhadd",		0x2000f800, 0x20001000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//1 -	0001 0
{"uhsub",		0x2000f800, 0x20002000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//1 -	0010 0
{"uqsub",		0x2000f800, 0x20002800,	0,	XSIMD,	XRD0,	XR5, XR16	},	//1 -	0010 1
{"cmhi",		0x2000f800, 0x20003000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//1 -	0011 0
{"cmhs",		0x2000f800, 0x20003800,	0,	XSIMD,	XRD0,	XR5, XR16	},	//1 -	0011 1
{"ushl",		0x2000f800, 0x20004000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//1 -	0100 0
{"uqshl",		0x2000f800, 0x20004800,	0,	XSIMD,	XRD0,	XR5, XR16	},	//1 -	0100 1
{"urshl",		0x2000f800, 0x20005000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//1 -	0101 0
{"uqrshl",		0x2000f800, 0x20005800,	0,	XSIMD,	XRD0,	XR5, XR16	},	//1 -	0101 1
{"umax",		0x2000f800, 0x20006000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//1 -	0110 0
{"umin",		0x2000f800, 0x20006800,	0,	XSIMD,	XRD0,	XR5, XR16	},	//1 -	0110 1
{"uabd",		0x2000f800, 0x20007000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//1 -	0111 0
{"uaba",		0x2000f800, 0x20007800,	0,	XSIMD,	XRD0,	XR5, XR16	},	//1 -	0111 1
{"sub",			0x2000f800, 0x20008000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//1 -	1000 0
{"cmeq",		0x2000f800, 0x20008800,	0,	XSIMD,	XRD0,	XR5, XR16	},	//1 -	1000 1
{"mls",			0x2000f800, 0x20009000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//1 -	1001 0
{"pmul",		0x2000f800, 0x20009800,	0,	XSIMD,	XRD0,	XR5, XR16	},	//1 -	1001 1
{"umaxp",		0x2000f800, 0x2000a000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//1 -	1010 0
{"uminp",		0x2000f800, 0x2000a800,	0,	XSIMD,	XRD0,	XR5, XR16	},	//1 -	1010 1
{"sqrdmulh",	0x2000f800, 0x2000b000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//1 -	1011 0
{"fmaxnmp",		0x2080f800, 0x2000c000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//1 0x	1100 0
{"faddp",		0x2080f800, 0x2000d000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//1 0x	1101 0
{"fmul",		0x2080f800, 0x2000d800,	0,	XSIMD,	XRD0,	XR5, XR16	},	//1 0x	1101 1
{"fcmge",		0x2080f800, 0x2000e000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//1 0x	1110 0
{"facge",		0x2080f800, 0x2000e800,	0,	XSIMD,	XRD0,	XR5, XR16	},	//1 0x	1110 1
{"fmaxp",		0x2080f800, 0x2000f000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//1 0x	1111 0
{"fdiv",		0x2080f800, 0x2000f800,	0,	XSIMD,	XRD0,	XR5, XR16	},	//1 0x	1111 1
{"eor",			0x20c0f800, 0x20001800,	0,	XSIMD,	XRD0,	XR5, XR16	},	//1 00	0001 1
{"bsl",			0x20c0f800, 0x20401800,	0,	XSIMD,	XRD0,	XR5, XR16	},	//1 01	0001 1
{"fminnmp",		0x2080f800, 0x2080c000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//1 1x	1100 0
{"fabd",		0x2080f800, 0x2080d000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//1 1x	1101 0
{"fcmgt",		0x2080f800, 0x2080e000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//1 1x	1110 0
{"facgt",		0x2080f800, 0x2080e800,	0,	XSIMD,	XRD0,	XR5, XR16	},	//1 1x	1110 1
{"fminp",		0x2080f800, 0x2080f000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//1 1x	1111 0
{"bit",			0x20c0f800, 0x20801800,	0,	XSIMD,	XRD0,	XR5, XR16	},	//1 10	0001 1
{"bif",			0x20c0f800, 0x20c01800,	0,	XSIMD,	XRD0,	XR5, XR16	},	//1 11	0001 1
0};

//C4.6.17 Advanced SIMD two-register miscellaneous
OP arm64_SIMD_misc2[] = {
{"rev64",		0x2001f000, 0x00000000,	0,	XSIMD,	XRD0,	XR5	},	//0 -	0 0000
{"rev16",		0x2001f000, 0x00001000,	0,	XSIMD,	XRD0,	XR5	},	//0 -	0 0001
{"saddlp",		0x2001f000, 0x00002000,	0,	XSIMD,	XRD0,	XR5	},	//0 -	0 0010
{"suqadd",		0x2001f000, 0x00003000,	0,	XSIMD,	XRD0,	XR5	},	//0 -	0 0011
{"cls",			0x2001f000, 0x00004000,	0,	XSIMD,	XRD0,	XR5	},	//0 -	0 0100
{"cnt",			0x2001f000, 0x00005000,	0,	XSIMD,	XRD0,	XR5	},	//0 -	0 0101
{"sadalp",		0x2001f000, 0x00006000,	0,	XSIMD,	XRD0,	XR5	},	//0 -	0 0110
{"sqabs",		0x2001f000, 0x00007000,	0,	XSIMD,	XRD0,	XR5	},	//0 -	0 0111
{"cmgt",		0x2001f000, 0x00008000,	0,	XSIMD,	XRD0,	XR5	},	//0 -	0 1000
{"cmeq",		0x2001f000, 0x00009000,	0,	XSIMD,	XRD0,	XR5	},	//0 -	0 1001
{"cmlt",		0x2001f000, 0x0000a000,	0,	XSIMD,	XRD0,	XR5	},	//0 -	0 1010
{"abs",			0x2001f000, 0x0000b000,	0,	XSIMD,	XRD0,	XR5	},	//0 -	0 1011
{"tn",			0x2001f000, 0x00012000,	0,	XSIMD,	XRD0,	XR5	},	//0 -	1 0010 X, XTN2
{"sqxtn",		0x2001f000, 0x00014000,	0,	XSIMD,	XRD0,	XR5	},	//0 -	1 0100	SQXTN2
{"fcvtn",		0x2081f000, 0x00016000,	0,	XSIMD,	XRD0,	XR5	},	//0 0x	1 0110	FCVTN2
{"fcvtl",		0x2081f000, 0x00017000,	0,	XSIMD,	XRD0,	XR5	},	//0 0x	1 0111	FCVTL2
{"frintn",		0x2081f000, 0x00018000,	0,	XSIMD,	XRD0,	XR5	},	//0 0x	1 1000
{"frintm",		0x2081f000, 0x00019000,	0,	XSIMD,	XRD0,	XR5	},	//0 0x	1 1001
{"fcvtns",		0x2081f000, 0x0001a000,	0,	XSIMD,	XRD0,	XR5	},	//0 0x	1 1010
{"fcvtms",		0x2081f000, 0x0001b000,	0,	XSIMD,	XRD0,	XR5	},	//0 0x	1 1011
{"fcvtas",		0x2081f000, 0x0001c000,	0,	XSIMD,	XRD0,	XR5	},	//0 0x	1 1100
{"scvtf",		0x2081f000, 0x0001d000,	0,	XSIMD,	XRD0,	XR5	},	//0 0x	1 1101
{"fcmgt",		0x2081f000, 0x0080c000,	0,	XSIMD,	XRD0,	XR5	},	//0 1x	0 1100
{"fcmeq",		0x2081f000, 0x0080d000,	0,	XSIMD,	XRD0,	XR5	},	//0 1x	0 1101
{"fcmlt",		0x2081f000, 0x0080e000,	0,	XSIMD,	XRD0,	XR5	},	//0 1x	0 1110
{"fabs",		0x2081f000, 0x0080f000,	0,	XSIMD,	XRD0,	XR5	},	//0 1x	0 1111
{"frintp",		0x2081f000, 0x00818000,	0,	XSIMD,	XRD0,	XR5	},	//0 1x	1 1000
{"frintz",		0x2081f000, 0x00819000,	0,	XSIMD,	XRD0,	XR5	},	//0 1x	1 1001
{"fcvtps",		0x2081f000, 0x0081a000,	0,	XSIMD,	XRD0,	XR5	},	//0 1x	1 1010
{"fcvtzs",		0x2081f000, 0x0081b000,	0,	XSIMD,	XRD0,	XR5	},	//0 1x	1 1011
{"urecpe",		0x2081f000, 0x0081c000,	0,	XSIMD,	XRD0,	XR5	},	//0 1x	1 1100
{"frecpe",		0x2081f000, 0x0081d000,	0,	XSIMD,	XRD0,	XR5	},	//0 1x	1 1101
{"rev32",		0x2001f000, 0x20000000,	0,	XSIMD,	XRD0,	XR5	},	//1 -	0 0000
{"uaddlp",		0x2001f000, 0x20002000,	0,	XSIMD,	XRD0,	XR5	},	//1 -	0 0010
{"usqadd",		0x2001f000, 0x20003000,	0,	XSIMD,	XRD0,	XR5	},	//1 -	0 0011
{"clz",			0x2001f000, 0x20004000,	0,	XSIMD,	XRD0,	XR5	},	//1 -	0 0100
{"uadalp",		0x2001f000, 0x20005000,	0,	XSIMD,	XRD0,	XR5	},	//1 -	0 0110
{"sqneg",		0x2001f000, 0x20007000,	0,	XSIMD,	XRD0,	XR5	},	//1 -	0 0111
{"cmge",		0x2001f000, 0x20008000,	0,	XSIMD,	XRD0,	XR5	},	//1 -	0 1000
{"cmle",		0x2001f000, 0x20009000,	0,	XSIMD,	XRD0,	XR5	},	//1 -	0 1001
{"neg",			0x2001f000, 0x2000b000,	0,	XSIMD,	XRD0,	XR5	},	//1 -	0 1011
{"sqxtun",		0x2001f000, 0x20012000,	0,	XSIMD,	XRD0,	XR5	},	//1 -	1 0010	SQXTUN2
{"shll",		0x2001f000, 0x20013000,	0,	XSIMD,	XRD0,	XR5	},	//1 -	1 0011	SHLL2
{"uqxtn",		0x2001f000, 0x20014000,	0,	XSIMD,	XRD0,	XR5	},	//1 -	1 0100	UQXTN2
{"fcvtxn",		0x2081f000, 0x20016000,	0,	XSIMD,	XRD0,	XR5	},	//1 0x	1 0110	FCVTXN2
{"frinta",		0x2081f000, 0x20018000,	0,	XSIMD,	XRD0,	XR5	},	//1 0x	1 1000
{"frintx",		0x2081f000, 0x20019000,	0,	XSIMD,	XRD0,	XR5	},	//1 0x	1 1001
{"fcvtnu",		0x2081f000, 0x2001a000,	0,	XSIMD,	XRD0,	XR5	},	//1 0x	1 1010
{"fcvtmu",		0x2081f000, 0x2001b000,	0,	XSIMD,	XRD0,	XR5	},	//1 0x	1 1011
{"fcvtau",		0x2081f000, 0x2001c000,	0,	XSIMD,	XRD0,	XR5	},	//1 0x	1 1100
{"ucvtf",		0x2081f000, 0x2001d000,	0,	XSIMD,	XRD0,	XR5	},	//1 0x	1 1101
{"not",			0x20c1f000, 0x20005000,	0,	XSIMD,	XRD0,	XR5	},	//1 00	0 0101
{"rbit",		0x20c1f000, 0x20405000,	0,	XSIMD,	XRD0,	XR5	},	//1 01	0 0101
{"fcmge",		0x2081f000, 0x2080c000,	0,	XSIMD,	XRD0,	XR5	},	//1 1x	0 1100
{"fcmle",		0x2081f000, 0x2080d000,	0,	XSIMD,	XRD0,	XR5	},	//1 1x	0 1101
{"fneg",		0x2081f000, 0x2080f000,	0,	XSIMD,	XRD0,	XR5	},	//1 1x	0 1111
{"frinti",		0x2081f000, 0x20819000,	0,	XSIMD,	XRD0,	XR5	},	//1 1x	1 1001
{"fcvtpu",		0x2081f000, 0x2081a000,	0,	XSIMD,	XRD0,	XR5	},	//1 1x	1 1010
{"fcvtzu",		0x2081f000, 0x2081b000,	0,	XSIMD,	XRD0,	XR5	},	//1 1x	1 1011
{"ursqrte",		0x2081f000, 0x2081c000,	0,	XSIMD,	XRD0,	XR5	},	//1 1x	1 1100
{"frsqrte",		0x2081f000, 0x2081d000,	0,	XSIMD,	XRD0,	XR5	},	//1 1x	1 1101
{"fsqrt",		0x2081f000, 0x2081f000,	0,	XSIMD,	XRD0,	XR5	},	//1 1x	1 1111
0};

//C4.6.18 Advanced SIMD vector x indexed element
OP arm64_SIMD_mul_indexed[] = {
{"smlal",		0x2000f000, 0x00002000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 -	0010	SMLAL2
{"sqdmlal",		0x2000f000, 0x00003000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 -	0011	SQDMLAL2
{"smlsl",		0x2000f000, 0x00006000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 -	0110	SMLSL2
{"sqdmlsl",		0x2000f000, 0x00007000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 -	0111	SQDMLSL2
{"mul",			0x2000f000, 0x00008000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 -	1000
{"smull",		0x2000f000, 0x0000a000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 -	1010	SMULL2
{"sqdmull",		0x2000f000, 0x0000b000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 -	1011	SQDMULL2
{"sqdmulh",		0x2000f000, 0x0000c000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 -	1100
{"sqrdmulh",	0x2000f000, 0x0000d000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 -	1101
{"fmla",		0x2080f000, 0x00801000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 1x	0001
{"fmls",		0x2080f000, 0x00805000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 1x	0101
{"fmul",		0x2080f000, 0x00809000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 1x	1001
{"mla",			0x2000f000, 0x20000000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//1 -	0000
{"umlal",		0x2000f000, 0x20002000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//1 -	0010	UMLAL2
{"mls",			0x2000f000, 0x20004000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//1 -	0100
{"umlsl",		0x2000f000, 0x20006000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//1 -	0110	UMLSL2
{"umull",		0x2000f000, 0x2000a000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//1 -	1010	UMULL2
{"fmulx",		0x2080f000, 0x20809000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//1 1x	1001
0};

//c4.6.19 cryptographic AES
OP arm64_crypto_AES[] = {
{"aese",		0x00c1f000, 0x00004000,	0,	XSIMD,	XRD0,	XR5	},	//00 0 0100
{"aesd",		0x00c1f000, 0x00005000,	0,	XSIMD,	XRD0,	XR5	},	//00 0 0101
{"aesmc",		0x00c1f000, 0x00006000,	0,	XSIMD,	XRD0,	XR5	},	//00 0 0110
{"aesimc",		0x00c1f000, 0x00007000,	0,	XSIMD,	XRD0,	XR5	},	//00 0 0111
0};

//c4.6.20 cryptographic three-register SHA
OP arm64_crypto_SHA3[] = {
{"sha1c",		0x00c07000, 0x00000000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//00 000
{"sha1p",		0x00c07000, 0x00001000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//00 001
{"sha1m",		0x00c07000, 0x00002000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//00 010
{"sha1su0",		0x00c07000, 0x00003000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//00 011
{"sha256h",		0x00c07000, 0x00004000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//00 100
{"sha256h2",	0x00c07000, 0x00005000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//00 101
{"sha256su1",	0x00c07000, 0x00006000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//00 110
0};

//c4.6.21 cryptographic two-register SHA
OP arm64_crypto_SHA2[] = {
{"sha1h",		0x00c1f000, 0x00000000,	0,	XSIMD,	XRD0,	XR5	},	//00 0 0000
{"sha1su1",		0x00c1f000, 0x00001000,	0,	XSIMD,	XRD0,	XR5	},	//00 0 0001
{"sha256su0",	0x00c1f000, 0x00002000,	0,	XSIMD,	XRD0,	XR5	},	//00 0 0010
0};

//c4.6.22 floating-point compare
OP arm64_float_cmp[] = {
{"fcmp",		0xa0c0c01f, 0x00000000,	0,	XSIMD,	0,	XR5, XR16	},	//0 0 00 00 0 0000
{"fcmp",		0xa0c0c01f, 0x00000008,	0,	XSIMD,	0,	XR5, XR16	},	//0 0 00 00 0 1000
{"fcmpe",		0xa0c0c01f, 0x00000010,	0,	XSIMD,	0,	XR5, XR16	},	//0 0 00 00 1 0000
{"fcmpe",		0xa0c0c01f, 0x00000018,	0,	XSIMD,	0,	XR5, XR16	},	//0 0 00 00 1 1000
{"fcmp",		0xa0c0c01f, 0x00400000,	0,	XSIMD,	0,	XR5, XR16	},	//0 0 01 00 0 0000
{"fcmp",		0xa0c0c01f, 0x00400008,	0,	XSIMD,	0,	XR5, XR16	},	//0 0 01 00 0 1000
{"fcmpe",		0xa0c0c01f, 0x00400010,	0,	XSIMD,	0,	XR5, XR16	},	//0 0 01 00 1 0000
{"fcmpe",		0xa0c0c01f, 0x00400018,	0,	XSIMD,	0,	XR5, XR16	},	//0 0 01 00 1 1000
0};

//c4.6.23 floating-point conditional compare
OP arm64_float_condcmp[] = {
{"fccmp",		0xa0c0c010, 0x00000000,	0,	XSIMD,	XRD5, XR16,	XFLAGS, XCOND	},	//0 0 00 0
{"fccmpe",		0xa0c0c010, 0x00000010,	0,	XSIMD,	XRD5, XR16,	XFLAGS, XCOND	},	//0 0 00 1
{"fccmp",		0xa0c0c010, 0x00400000,	0,	XSIMD,	XRD5, XR16,	XFLAGS, XCOND	},	//0 0 01 0
{"fccmpe",		0xa0c0c010, 0x00400010,	0,	XSIMD,	XRD5, XR16,	XFLAGS, XCOND	},	//0 0 01 1
0};

//c4.6.24 floating-point conditional select
OP arm64_float_condsel[] = {
{"fcsel",		0xa0c00000, 0x00000000,	0,	XSIMD,	XRD0, XR5, XR16,	XCOND	},	//0 00
{"fcsel",		0xa0c00000, 0x00400000,	0,	XSIMD,	XRD0, XR5, XR16,	XCOND	},	//0 0 01
0};

//c4.6.25 floating-point data-processing (1 source)
OP arm64_float_data1[] = {
{"fmov",		0xa0df8000, 0x00000000,	0,	XSIMD,	XRD0,	XR5	},	//0 0 00 0 0000 0
{"fabs",		0xa0df8000, 0x00008000,	0,	XSIMD,	XRD0,	XR5	},	//0 0 00 0 0000 1
{"fneg",		0xa0df8000, 0x00010000,	0,	XSIMD,	XRD0,	XR5	},	//0 0 00 0 0001 0
{"fsqrt",		0xa0df8000, 0x00018000,	0,	XSIMD,	XRD0,	XR5	},	//0 0 00 0 0001 1
{"fcvt",		0xa0df8000, 0x00028000,	0,	XSIMD,	XRD0,	XR5	},	//0 0 00 0 0010 1
{"fcvt",		0xa0df8000, 0x00038000,	0,	XSIMD,	XRD0,	XR5	},	//0 0 00 0 0011 1
{"frintn",		0xa0df8000, 0x00040000,	0,	XSIMD,	XRD0,	XR5	},	//0 0 00 0 0100 0
{"frintp",		0xa0df8000, 0x00048000,	0,	XSIMD,	XRD0,	XR5	},	//0 0 00 0 0100 1
{"frintm",		0xa0df8000, 0x00050000,	0,	XSIMD,	XRD0,	XR5	},	//0 0 00 0 0101 0
{"frintz",		0xa0df8000, 0x00058000,	0,	XSIMD,	XRD0,	XR5	},	//0 0 00 0 0101 1
{"frinta",		0xa0df8000, 0x00060000,	0,	XSIMD,	XRD0,	XR5	},	//0 0 00 0 0110 0
{"frintx",		0xa0df8000, 0x00070000,	0,	XSIMD,	XRD0,	XR5	},	//0 0 00 0 0111 0
{"frinti",		0xa0df8000, 0x00078000,	0,	XSIMD,	XRD0,	XR5	},	//0 0 00 0 0111 1
{"fmov",		0xa0df8000, 0x00400000,	0,	XSIMD,	XRD0,	XR5	},	//0 0 01 0 0000 0
{"fabs",		0xa0df8000, 0x00408000,	0,	XSIMD,	XRD0,	XR5	},	//0 0 01 0 0000 1
{"fneg",		0xa0df8000, 0x00410000,	0,	XSIMD,	XRD0,	XR5	},	//0 0 01 0 0001 0
{"fsqrt",		0xa0df8000, 0x00418000,	0,	XSIMD,	XRD0,	XR5	},	//0 0 01 0 0001 1
{"fcvt",		0xa0df8000, 0x00420000,	0,	XSIMD,	XRD0,	XR5	},	//0 0 01 0 0010 0
{"fcvt",		0xa0df8000, 0x00438000,	0,	XSIMD,	XRD0,	XR5	},	//0 0 01 0 0011 1
{"frintn",		0xa0df8000, 0x00440000,	0,	XSIMD,	XRD0,	XR5	},	//0 0 01 0 0100 0
{"frintp",		0xa0df8000, 0x00448000,	0,	XSIMD,	XRD0,	XR5	},	//0 0 01 0 0100 1
{"frintm",		0xa0df8000, 0x00450000,	0,	XSIMD,	XRD0,	XR5	},	//0 0 01 0 0101 0
{"frintz",		0xa0df8000, 0x00458000,	0,	XSIMD,	XRD0,	XR5	},	//0 0 01 0 0101 1
{"frinta",		0xa0df8000, 0x00460000,	0,	XSIMD,	XRD0,	XR5	},	//0 0 01 0 0110 0
{"frintx",		0xa0df8000, 0x00470000,	0,	XSIMD,	XRD0,	XR5	},	//0 0 01 0 0111 0
{"frinti",		0xa0df8000, 0x00478000,	0,	XSIMD,	XRD0,	XR5	},	//0 0 01 0 0111 1
{"fcvt",		0xa0df8000, 0x00c20000,	0,	XSIMD,	XRD0,	XR5	},	//0 0 11 0 0010 0
{"fcvt",		0xa0df8000, 0x00c28000,	0,	XSIMD,	XRD0,	XR5	},	//0 0 11 0 0010 1
0};

//c4.6.26 floating-point data-processing (2 source)
OP arm64_float_data2[] = {
{"fmul",		0xa0c0f000, 0x00000000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 0 00 0000
{"fdiv",		0xa0c0f000, 0x00001000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 0 00 0001
{"fadd",		0xa0c0f000, 0x00002000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 0 00 0010
{"fsub",		0xa0c0f000, 0x00003000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 0 00 0011
{"fmax",		0xa0c0f000, 0x00004000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 0 00 0100
{"fmin",		0xa0c0f000, 0x00005000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 0 00 0101
{"fmaxnm",		0xa0c0f000, 0x00006000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 0 00 0110
{"fminnm",		0xa0c0f000, 0x00007000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 0 00 0111
{"fnmul",		0xa0c0f000, 0x00008000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 0 00 1000
{"fmul",		0xa0c0f000, 0x00400000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 0 01 0000
{"fdiv",		0xa0c0f000, 0x00401000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 0 01 0001
{"fadd",		0xa0c0f000, 0x00402000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 0 01 0010
{"fsub",		0xa0c0f000, 0x00403000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 0 01 0011
{"fmax",		0xa0c0f000, 0x00404000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 0 01 0100
{"fmin",		0xa0c0f000, 0x00405000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 0 01 0101
{"fmaxnm",		0xa0c0f000, 0x00406000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 0 01 0110
{"fminnm",		0xa0c0f000, 0x00407000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 0 01 0111
{"fnmul",		0xa0c0f000, 0x00408000,	0,	XSIMD,	XRD0,	XR5, XR16	},	//0 0 01 1000
0};

//C4.6.27 Floating-point data-processing (3 source)
OP arm64_float_data3[] = {
{"fmadd",		0xa0e08000, 0x00000000,	0,	XSIMD,	XRD0,	XR5, XR16, XR10	},	//0 0 00 0 0
{"fmsub",		0xa0e08000, 0x00008000,	0,	XSIMD,	XRD0,	XR5, XR16, XR10	},	//0 0 00 0 1
{"fnmadd",		0xa0e08000, 0x00200000,	0,	XSIMD,	XRD0,	XR5, XR16, XR10	},	//0 0 00 1 0
{"fnmsub",		0xa0e08000, 0x00208000,	0,	XSIMD,	XRD0,	XR5, XR16, XR10	},	//0 0 00 1 1
{"fmadd",		0xa0e08000, 0x00400000,	0,	XSIMD,	XRD0,	XR5, XR16, XR10	},	//0 0 01 0 0
{"fmsub",		0xa0e08000, 0x00408000,	0,	XSIMD,	XRD0,	XR5, XR16, XR10	},	//0 0 01 0 1
{"fnmadd",		0xa0e08000, 0x00600000,	0,	XSIMD,	XRD0,	XR5, XR16, XR10	},	//0 0 01 1 0
{"fnmsub",		0xa0e08000, 0x00608000,	0,	XSIMD,	XRD0,	XR5, XR16, XR10	},	//0 0 01 1 1
0};

//c4.6.28 floating-point immediate
OP arm64_float_imm[] = {
{"fmov",		0xa0c003e0, 0x00000000,	0,	XSIMD,	XRD0,	XIMMF},		//0 0 00 00000
{"fmov",		0xa0c003e0, 0x00400000,	0,	XSIMD,	XRD0,	XIMMF},		//0 0 01 00000
0};

//C4.6.29 Conversion between floating-point and fixed-point
OP arm64_float_fixed[] = {
{"scvtf",		0xa0df0000, 0x00020000,	0,	XSIMD,	XRD0,	XR5, XFPBITS},	//0 0 00 00 010
{"ucvtf",		0xa0df0000, 0x00030000,	0,	XSIMD,	XRD0,	XR5, XFPBITS},	//0 0 00 00 011
{"fcvtzs",		0xa0df0000, 0x00180000,	0,	XSIMD,	XRD0,	XR5, XFPBITS},	//0 0 00 11 000
{"fcvtzu",		0xa0df0000, 0x00190000,	0,	XSIMD,	XRD0,	XR5, XFPBITS},	//0 0 00 11 001
{"scvtf",		0xa0df0000, 0x00420000,	0,	XSIMD,	XRD0,	XR5, XFPBITS},	//0 0 01 00 010
{"ucvtf",		0xa0df0000, 0x00430000,	0,	XSIMD,	XRD0,	XR5, XFPBITS},	//0 0 01 00 011
{"fcvtzs",		0xa0df0000, 0x00580000,	0,	XSIMD,	XRD0,	XR5, XFPBITS},	//0 0 01 11 000
{"fcvtzu",		0xa0df0000, 0x00590000,	0,	XSIMD,	XRD0,	XR5, XFPBITS},	//0 0 01 11 001
{"scvtf",		0xa0df0000, 0x80020000,	0,	XSIMD,	XRD0,	XR5, XFPBITS},	//1 0 00 00 010
{"ucvtf",		0xa0df0000, 0x80030000,	0,	XSIMD,	XRD0,	XR5, XFPBITS},	//1 0 00 00 011
{"fcvtzs",		0xa0df0000, 0x80180000,	0,	XSIMD,	XRD0,	XR5, XFPBITS},	//1 0 00 11 000
{"fcvtzu",		0xa0df0000, 0x80190000,	0,	XSIMD,	XRD0,	XR5, XFPBITS},	//1 0 00 11 001
{"scvtf",		0xa0df0000, 0x80420000,	0,	XSIMD,	XRD0,	XR5, XFPBITS},	//1 0 01 00 010
{"ucvtf",		0xa0df0000, 0x80430000,	0,	XSIMD,	XRD0,	XR5, XFPBITS},	//1 0 01 00 011
{"fcvtzs",		0xa0df0000, 0x80580000,	0,	XSIMD,	XRD0,	XR5, XFPBITS},	//1 0 01 11 000
{"fcvtzu",		0xa0df0000, 0x80590000,	0,	XSIMD,	XRD0,	XR5, XFPBITS},	//1 0 01 11 001
0};

//C4.6.30 Conversion between floating-point and integer
OP arm64_float_int[] = {
{"fcvtns",		0xa0df0000, 0x00000000,	0,	XSIMD,	XRD0,	XR5	},	//0 0 00 00 000
{"fcvtnu",		0xa0df0000, 0x00010000,	0,	XSIMD,	XRD0,	XR5	},	//0 0 00 00 001
{"scvtf",		0xa0df0000, 0x00020000,	0,	XSIMD,	XRD0,	XR5	},	//0 0 00 00 010
{"ucvtf",		0xa0df0000, 0x00030000,	0,	XSIMD,	XRD0,	XR5	},	//0 0 00 00 011
{"fcvtas",		0xa0df0000, 0x00040000,	0,	XSIMD,	XRD0,	XR5	},	//0 0 00 00 100
{"fcvtau",		0xa0df0000, 0x00050000,	0,	XSIMD,	XRD0,	XR5	},	//0 0 00 00 101
{"fmov",		0xa0df0000, 0x00060000,	0,	XSIMD,	XRD0,	XR5	},	//0 0 00 00 110
{"fmov",		0xa0df0000, 0x00070000,	0,	XSIMD,	XRD0,	XR5	},	//0 0 00 00 111
{"fcvtps",		0xa0df0000, 0x00080000,	0,	XSIMD,	XRD0,	XR5	},	//0 0 00 01 000
{"fcvtpu",		0xa0df0000, 0x00090000,	0,	XSIMD,	XRD0,	XR5	},	//0 0 00 01 001
{"fcvtms",		0xa0df0000, 0x00100000,	0,	XSIMD,	XRD0,	XR5	},	//0 0 00 10 000
{"fcvtmu",		0xa0df0000, 0x00110000,	0,	XSIMD,	XRD0,	XR5	},	//0 0 00 10 001
{"fcvtzs",		0xa0df0000, 0x00180000,	0,	XSIMD,	XRD0,	XR5	},	//0 0 00 11 000
{"fcvtzu",		0xa0df0000, 0x00190000,	0,	XSIMD,	XRD0,	XR5	},	//0 0 00 11 001
{"fcvtns",		0xa0df0000, 0x00400000,	0,	XSIMD,	XRD0,	XR5	},	//0 0 01 00 000
{"fcvtnu",		0xa0df0000, 0x00410000,	0,	XSIMD,	XRD0,	XR5	},	//0 0 01 00 001
{"scvtf",		0xa0df0000, 0x00420000,	0,	XSIMD,	XRD0,	XR5	},	//0 0 01 00 010
{"ucvtf",		0xa0df0000, 0x00430000,	0,	XSIMD,	XRD0,	XR5	},	//0 0 01 00 011
{"fcvtas",		0xa0df0000, 0x00440000,	0,	XSIMD,	XRD0,	XR5	},	//0 0 01 00 100
{"fcvtau",		0xa0df0000, 0x00450000,	0,	XSIMD,	XRD0,	XR5	},	//0 0 01 00 101
{"fcvtps",		0xa0df0000, 0x00480000,	0,	XSIMD,	XRD0,	XR5	},	//0 0 01 01 000
{"fcvtpu",		0xa0df0000, 0x00490000,	0,	XSIMD,	XRD0,	XR5	},	//0 0 01 01 001
{"fcvtms",		0xa0df0000, 0x00500000,	0,	XSIMD,	XRD0,	XR5	},	//0 0 01 10 000
{"fcvtmu",		0xa0df0000, 0x00510000,	0,	XSIMD,	XRD0,	XR5	},	//0 0 01 10 001
{"fcvtzs",		0xa0df0000, 0x00580000,	0,	XSIMD,	XRD0,	XR5	},	//0 0 01 11 000
{"fcvtzu",		0xa0df0000, 0x00590000,	0,	XSIMD,	XRD0,	XR5	},	//0 0 01 11 001
{"fcvtns",		0xa0df0000, 0x80000000,	0,	XSIMD,	XRD0,	XR5	},	//1 0 00 00 000
{"fcvtnu",		0xa0df0000, 0x80010000,	0,	XSIMD,	XRD0,	XR5	},	//1 0 00 00 001
{"scvtf",		0xa0df0000, 0x80020000,	0,	XSIMD,	XRD0,	XR5	},	//1 0 00 00 010
{"ucvtf",		0xa0df0000, 0x80030000,	0,	XSIMD,	XRD0,	XR5	},	//1 0 00 00 011
{"fcvtas",		0xa0df0000, 0x80040000,	0,	XSIMD,	XRD0,	XR5	},	//1 0 00 00 100
{"fcvtau",		0xa0df0000, 0x80050000,	0,	XSIMD,	XRD0,	XR5	},	//1 0 00 00 101
{"fcvtps",		0xa0df0000, 0x80080000,	0,	XSIMD,	XRD0,	XR5	},	//1 0 00 01 000
{"fcvtpu",		0xa0df0000, 0x80090000,	0,	XSIMD,	XRD0,	XR5	},	//1 0 00 01 001
{"fcvtms",		0xa0df0000, 0x80100000,	0,	XSIMD,	XRD0,	XR5	},	//1 0 00 10 000
{"fcvtmu",		0xa0df0000, 0x80110000,	0,	XSIMD,	XRD0,	XR5	},	//1 0 00 10 001
{"fcvtzs",		0xa0df0000, 0x80180000,	0,	XSIMD,	XRD0,	XR5	},	//1 0 00 11 000
{"fcvtzu",		0xa0df0000, 0x80190000,	0,	XSIMD,	XRD0,	XR5	},	//1 0 00 11 001
{"fcvtns",		0xa0df0000, 0x80400000,	0,	XSIMD,	XRD0,	XR5	},	//1 0 01 00 000
{"fcvtnu",		0xa0df0000, 0x80410000,	0,	XSIMD,	XRD0,	XR5	},	//1 0 01 00 001
{"scvtf",		0xa0df0000, 0x80420000,	0,	XSIMD,	XRD0,	XR5	},	//1 0 01 00 010
{"ucvtf",		0xa0df0000, 0x80430000,	0,	XSIMD,	XRD0,	XR5	},	//1 0 01 00 011
{"fcvtas",		0xa0df0000, 0x80440000,	0,	XSIMD,	XRD0,	XR5	},	//1 0 01 00 100
{"fcvtau",		0xa0df0000, 0x80450000,	0,	XSIMD,	XRD0,	XR5	},	//1 0 01 00 101
{"fmov",		0xa0df0000, 0x80460000,	0,	XSIMD,	XRD0,	XR5	},	//1 0 01 00 110
{"fmov",		0xa0df0000, 0x80470000,	0,	XSIMD,	XRD0,	XR5	},	//1 0 01 00 111
{"fcvtps",		0xa0df0000, 0x80480000,	0,	XSIMD,	XRD0,	XR5	},	//1 0 01 01 000
{"fcvtpu",		0xa0df0000, 0x80490000,	0,	XSIMD,	XRD0,	XR5	},	//1 0 01 01 001
{"fcvtms",		0xa0df0000, 0x80500000,	0,	XSIMD,	XRD0,	XR5	},	//1 0 01 10 000
{"fcvtmu",		0xa0df0000, 0x80510000,	0,	XSIMD,	XRD0,	XR5	},	//1 0 01 10 001
{"fcvtzs",		0xa0df0000, 0x80580000,	0,	XSIMD,	XRD0,	XR5	},	//1 0 01 11 000
{"fcvtzu",		0xa0df0000, 0x80590000,	0,	XSIMD,	XRD0,	XR5	},	//1 0 01 11 001
{"fmov",		0xa0df0000, 0x808e0000,	0,	XSIMD,	XRD0,	XR5	},	//1 0 10 01 110
{"fmov",		0xa0df0000, 0x808f0000,	0,	XSIMD,	XRD0,	XR5	},	//1 0 10 01 111
0};

OP DisassemblerARM64::ops[] = {
	{"unallocated",							0x18000000, 0x00000000},
	{(char*)arm64_data_imm,					0x1c000000, 0x10000000,	TABLE},
	{(char*)arm64_branch_etc,				0x1c000000, 0x14000000,	TABLE},
	{(char*)arm64_loadstore,				0x0a000000, 0x08000000,	TABLE},
	{(char*)arm64_data_reg,					0x0e000000, 0x0a000000,	TABLE},

	{(char*)arm64_float_fixed,				0x5f200000,	 0x1e000000, TABLE},
	{(char*)arm64_float_condcmp,			0x5f200c00,	 0x1e200400, TABLE},
	{(char*)arm64_float_data2,				0x5f200c00,	 0x1e200800, TABLE},
	{(char*)arm64_float_condsel,			0x5f200c00,	 0x1e200c00, TABLE},
	{(char*)arm64_float_imm,				0x5f201c00,	 0x1e201000, TABLE},
	{(char*)arm64_float_cmp,				0x5f203c00,	 0x1e202000, TABLE},
	{(char*)arm64_float_data1,				0x5f207c00,	 0x1e204000, TABLE},
	{(char*)arm64_float_int,				0x5f20fc00,	 0x1e200000, TABLE},
	{(char*)arm64_float_data3,				0x5f000000,	 0x1f000000, TABLE},
	{(char*)arm64_SIMD_same3,				0x9f200400,	 0x0e200400, TABLE},
	{(char*)arm64_SIMD_diff3,				0x9f200c00,	 0x0e200000, TABLE},
	{(char*)arm64_SIMD_misc2,				0x9f3e0c00,	 0x0e200800, TABLE},
	{(char*)arm64_SIMD_across,				0x9f3e0c00,	 0x0e300800, TABLE},
	{(char*)arm64_SIMD_copy,				0x9fe08400,	 0x0e000400, TABLE},
	{(char*)arm64_SIMD_mul_indexed,			0x9f000400,	 0x0f000000, TABLE},
	{(char*)arm64_SIMD_mod_imm,				0x9ff80400,	 0x0f000400, TABLE},
	{(char*)arm64_SIMD_shift_imm,			0x9f800400,	 0x0f000400, TABLE},
	{(char*)arm64_SIMD_table,				0xbf208c00,	 0x0e000000, TABLE},
	{(char*)arm64_SIMD_permute,				0xbf208c00,	 0x0e000800, TABLE},
	{"ext",									0xbf208400,	 0x2e000000, },
	{(char*)arm64_SIMD_scalar_same3,		0xdf200400,	 0x5e200400, TABLE},
	{(char*)arm64_SIMD_scalar_diff3,		0xdf200c00,	 0x5e200000, TABLE},
	{(char*)arm64_SIMD_scalar_misc2,		0xdf3e0c00,	 0x5e200800, TABLE},
	{(char*)arm64_SIMD_scalar_pairwise,		0xdf3e0c00,	 0x5e300800, TABLE},
	{"dup",									0xdfe08400,	 0x5e000400, },
	{(char*)arm64_SIMD_scalar_mul_indexed,	0xdf000400,	 0x5f000000, TABLE},
	{(char*)arm64_SIMD_scalar_shift_imm,	0xdf800400,	 0x5f000400, TABLE},
	{(char*)arm64_crypto_AES,				0xff3e0c00,	 0x4e280800, TABLE},
	{(char*)arm64_crypto_SHA3,				0xff208c00,	 0x5e000000, TABLE},
	{(char*)arm64_crypto_SHA2,				0xff3e0c00,	 0x5e280800, TABLE},
0 };
