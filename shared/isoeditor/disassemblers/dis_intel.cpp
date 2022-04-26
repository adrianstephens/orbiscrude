#include "disassembler.h"

using namespace iso;

template<int B, typename T> struct baseint_prefix : constructable<baseint<B,T> > {
	baseint_prefix(T _i) : constructable<baseint<B,T> >(_i)	{}
};
template<typename T> inline baseint_prefix<16, T>	prefix_hex(T t)	{ return t; }

template<int B> size_t put_prefix(char *s);
template<> size_t put_prefix<16>(char *s) { s[0] = '0'; s[1] = 'x'; return 2; }

template<int B, typename T> inline size_t to_string(char *s, baseint_prefix<B,T> v) {
	char	*p = s;
	T		x = v;
	if (x < 0) {
		*p++ = '-';
		x	= -x;
	}
	p += put_prefix<B>(p);
	return (p - s) + put_unsigned_num_base<B>(p, unsigned_t<T>(x), 'a');
}

class IntelOp {
public:
	enum ARCH {	X16, X32, X64,};

private:
	static const char *reg8[], *reg16[], *reg32[], *reg64[], **regs[4];
	static const char *bases16[];
	static const char *sizes[];
	static const char *compares[];

	enum SIZE {
		SIZE_1,
		SIZE_2,
		SIZE_4,
		SIZE_8,
		SIZE_10,
		SIZE_16,
		SIZE_32,
	};

	enum {
		MAP_PRIMARY,
		MAP_SECONDARY,	//0f
		MAP_660f,
		MAP_f30f,
		MAP_f20f,

		MAP_0F38,
		MAP_0F3A,
		MAP_3DNOW,
		MAP_VEX,
		_MAP_TOTAL = MAP_VEX + 3,
	};

	struct PREFIX {
		enum TYPE {
			B				= 1 << 0,	//no move
			X				= 1 << 1,	//no move
			R				= 1 << 2,	//no move
			W				= 1 << 3,	//no move

			EXT				= 1 << 4,
			OperandSize		= 1 << 4,	//66\	extension 00:None, 01:66h, 10:F3h, 11:F2h
			Repe			= 1 << 5,	//f3/	no move
			L				= 1 << 6,	//no move
			AddressSize		= 1 << 7,	//67
			Lock			= 1 << 8,	//f0
			Repne			= 1 << 9,	//f2

			FS				= 1 << 12,	//64
			SS				= 1 << 13,	//36
			GS				= 1 << 14,	//65
			ES				= 1 << 15,	//26
			CS				= 1 << 16,	//2e
			DS				= 1 << 17,	//3e

			BranchTaken		= 1 << 18,
			BranchNotTaken	= 1 << 19,

			X87				= 1 << 20,
		};
		uint32	value;

		bool	test(TYPE t) {
			return !!(value & t);
		}
		void	add(TYPE t) {
			value |= t;
		}
		void	addPP(uint8 x) {
			add(TYPE((x & 3) * EXT));
		}
		void	addVEX(const uint8 *p) {
			add(TYPE(
				((~p[0] >> 5) & (B | X | R))
			|	(p[1] & 0x04	? L : 0)
			|	(p[1] & 0x80	? W : 0)
			));
			addPP(p[1]);
		}
		int		extension() const {
			return value & Repne ? 3 : ((value / EXT) & 3);
		}
	};

	// instruction coding structs:
	struct ModRM {
		enum {INDIRECT, OFFSET8, OFFSET32, REGISTER};
		uint8 rm:3, reg:3, mod:2;
	};
	struct SIB {
		uint8 base:3, index:3, scale:2;
	};

	ARCH		arch;
	PREFIX		prefix;
	uint8		opcode;
	uint8		modrm_rm, modrm_reg, modrm_mod;
	uint8		sib_base, sib_index, sib_scale;
	uint8		vex_reg;
	bool		noregs;
	int64		displacement;
	int64		immediate[2];
	uint64		nextaddress;

	SIZE		GetSize(uint8 arg);
	void		GeneralRegister(string_accum &sa, int reg, uint8 arg);
	void		Address(string_accum &sa, Disassembler::SymbolFinder sym_finder, uint8 arg);
	void		Argument(string_accum &sa, Disassembler::SymbolFinder sym_finder, uint8 arg);

public:
	const char *mnemonic;
	uint32		info;
	uint32		args;

	string_accum&	Dump(string_accum &sa, Disassembler::SymbolFinder sym_finder, uint64 _nextaddress);
	const uint8*	Parse(const uint8 *p);
	uint64			GetDest() const;

	IntelOp(ARCH arch) : arch(arch)	{}
};

string_param CheckSymbol(Disassembler::SymbolFinder sym_finder, uint64 addr) {
	uint64			sym_addr;
	string_paramT<char>	sym_name;
	if (sym_finder && sym_finder(addr, sym_addr, sym_name)) {
		if (sym_addr == addr)
			return move(sym_name);
		return move(string(sym_name) << "+" << addr - sym_addr);
	}
	return 0;
}


void MemoryAddress(string_accum &sa, Disassembler::SymbolFinder sym_finder, uint64 addr) {
	if (auto label = CheckSymbol(sym_finder, addr))
		sa << label << " (0x" << hex(addr) << ')';
	else
		sa << prefix_hex(addr);
}


enum OPFLAGS {
//-----------------	OPERANDS

	OPF_TYPE= 0x3f,
	OPF_0	= 0,
	OPF_1	= 1,

	OPF_reg,
	OPF_rAX	= OPF_reg,
	OPF_rCX,
	OPF_rDX,
	OPF_rBX,

	OPF_seg,
	OPF_es	= OPF_seg,
	OPF_cs,
	OPF_ss,
	OPF_ds,
	OPF_fs,
	OPF_gs,

	OPF_st,				// ST(0)
	OPF_Mp,				// 32 or 48 bit far pointer
	OPF_O,				// offset in ins
	OPF_I,				// immediate
	OPF_J,				// jumps
	OPF_F,				// x87 stack register in bottom 3 bits of op
	OPF_K,				// reg in bottom 3 bits of op (al,cl,dl,bl,ah,ch,dh,bh)
	OPF_B,				// gen reg in VEX.vvvv
	OPF_H,				// ymm or xmm in VEX.vvvv

	OPF_MODRM,
	OPF_E = OPF_MODRM,	// gen reg or mem in modrm.rm
	OPF_G,				// gen reg in modrm.reg
	OPF_S,				// seg reg in modrm.reg
	OPF_C,				// control reg in modrm.reg
	OPF_D,				// debug reg in modrm.reg
	OPF_V,				// xmm in modrm.reg
	OPF_W,				// xmm or mem in modrm
	OPF_P,				// 64 bit mmx reg in modrm.reg
	OPF_Q,				// 64 bit mmx reg or mem in modrm.rm
	OPF_M,				// mem from modrm
	OPF_nop,			// eat modrm

//-----------------	CHANS

	OPF_SIZE= 0xC0,
	OPF_v	= 0,
	OPF_b	= 0x40,
	OPF_w	= 0x80,
	OPF_d	= 0xC0,

//-----------------	INSTRUCTION FLAGS

	OPF_NOT64	= 1 << 24,
	OPF_ONLY64	= 1 << 25,
	OPF_64		= OPF_NOT64 | OPF_ONLY64,
	OPF_GROUP	= 1 << 31,

//-----------------	ALIASES

	OPF_R	= OPF_E,
	OPF_N	= OPF_Q,
	OPF_U	= OPF_W,
	OPF_Uo	= OPF_U,
	OPF_Nq	= OPF_N,
	OPF_Rv	= OPF_R,
	OPF_Pp	= OPF_P,
	OPF_Qp	= OPF_Q,
	OPF_Pq	= OPF_P,
	OPF_Qq	= OPF_Q,
	OPF_Vo	= OPF_V,
	OPF_By	= OPF_B,
	OPF_Ey	= OPF_E,

//----------------- COMBINATIONS

	OPF_al	= OPF_rAX | OPF_b,
	OPF_cl	= OPF_rCX | OPF_b,
	OPF_ax	= OPF_rAX | OPF_w,
	OPF_cx	= OPF_rCX | OPF_w,
	OPF_dx	= OPF_rDX | OPF_w,

	OPF_Eb	= OPF_E | OPF_b,
	OPF_Ev	= OPF_E | OPF_v,
	OPF_Ew	= OPF_E | OPF_w,
	OPF_Ed	= OPF_E | OPF_v,	//**?

	OPF_Gb	= OPF_G | OPF_b,
	OPF_Gv	= OPF_G | OPF_v,
	OPF_Gw	= OPF_G | OPF_w,
	OPF_Gd	= OPF_G | OPF_d,
	OPF_Gy	= OPF_G,

	OPF_Kv	= OPF_K | OPF_v,
	OPF_Kb	= OPF_K | OPF_b,
	OPF_Kd	= OPF_K | OPF_d,

	OPF_Ob	= OPF_O | OPF_b,
	OPF_Ov	= OPF_O | OPF_v,

	OPF_Ib	= OPF_I | OPF_b,
	OPF_Iv	= OPF_I | OPF_v,
	OPF_Iw	= OPF_I | OPF_w,

	OPF_Mb	= OPF_M | OPF_b,
	OPF_Mw	= OPF_M | OPF_w,
	OPF_Ms	= OPF_M,
	OPF_Md	= OPF_M,
	OPF_Mo	= OPF_M,
	OPF_Mq	= OPF_M,
	OPF_Mr	= OPF_M,

	OPF_Jz	= OPF_J,
	OPF_Jb	= OPF_J | OPF_b,
	OPF_Ja	= OPF_J | OPF_d,	//absolute

#define OPF_P2(p1,p2)		OPF_##p1##p2		= OPF_##p1 | (OPF_##p2 << 8)
#define OPF_P3(p1,p2,p3)	OPF_##p1##p2##p3	= OPF_##p1 | (OPF_##p2 << 8) | (OPF_##p3 << 16)
#define OPF_i64(p)			OPF_##p##i64		= OPF_##p | OPF_NOT64
#define OPF_o64(p)			OPF_##p##o64		= OPF_##p | OPF_ONLY64
#define OPF_64(p)			OPF_##p##64			= OPF_##p | OPF_64

	OPF_P2(rAX,Kv),
	OPF_P2(Iw,Ib),

	OPF_P2(Eb,Gb),
	OPF_P2(Ew,Gw),
	OPF_P2(Ev,Gv),
	OPF_P2(Gb,Eb),
	OPF_P2(Gv,Eb),
	OPF_P2(Gv,Ew),
	OPF_P2(Gv,Ev),
	OPF_P2(V,E),
	OPF_P2(E,V),
	OPF_P2(By,Ey),

	OPF_P2(Ib,al),
	OPF_P2(al,Ib),
	OPF_P2(rAX,Iv),
	OPF_P2(Ib,ax),
	OPF_P2(ax,Ib),
	OPF_P2(al,Ob),
	OPF_P2(Ob,al),
	OPF_P2(rAX,Ov),
	OPF_P2(Ov,rAX),

	OPF_P2(E,I),
	OPF_P2(Eb,Ib),
	OPF_P2(Ev,Ib),
	OPF_P2(Ev,Iv),
	OPF_P2(Kb,Ib),
	OPF_P2(Kv,Iv),

	OPF_P2(Ew,S),
	OPF_P2(S,Ew),
	OPF_P2(Gv,M),

	OPF_P2(Eb,cl),
	OPF_P2(Ev,cl),
	OPF_P2(Eb,1),
	OPF_P2(Ev,1),

	OPF_P2(al,dx),
	OPF_P2(dx,al),
	OPF_P2(dx,ax),

	OPF_P2(V,U),
	OPF_P2(V,W),
	OPF_P2(W,V),
	OPF_P2(V,M),
	OPF_P2(M,V),

	OPF_P2(R,C),
	OPF_P2(R,D),
	OPF_P2(C,R),
	OPF_P2(D,R),
	OPF_P2(P,E),
	OPF_P2(E,P),
	OPF_P2(P,Q),
	OPF_P2(Q,P),
	OPF_P2(P,U),
	OPF_P2(Pq,Qq),

	OPF_P2(Md,Gd),
	OPF_P2(Gd,U),
	OPF_P2(G,U),
	OPF_P2(G,W),
	OPF_P2(Mq,P),
	OPF_P2(Mq,V),
	OPF_P2(P,W),
	OPF_P2(V,Q),
	OPF_P2(V,Mo),
	OPF_P2(Mo,V),
	OPF_P2(V,N),

	OPF_P2(Nq,Ib),
	OPF_P2(Uo,Ib),

	OPF_P2(st,F),
	OPF_P2(F,st),

	OPF_P3(G,H,Mq),
	OPF_P3(V,H,W),
	OPF_P3(V,H,Mq),
	OPF_P3(V,H,M),
	OPF_P3(M,H,V),
	OPF_P3(V,H,E),
	OPF_P3(Gv,Ev,Iv),
	OPF_P3(Gv,Ev,Ib),
	OPF_P3(Gy,Ey,By),
	OPF_P3(Gy,By,Ey),
	OPF_P3(Gd,Nq,Ib),
	OPF_P3(Ev,Gv,Ib),
	OPF_P3(Ev,Gv,cl),
	OPF_P3(V,W,Ib),
	OPF_P3(W,V,Ib),
	OPF_P3(Eb,V,Ib),
	OPF_P3(V,Eb,Ib),
	OPF_P3(Ew,V,Ib),
	OPF_P3(V,Ew,Ib),
	OPF_P3(V,Ed,Ib),
	OPF_P3(Ed,V,Ib),
	OPF_P3(E,V,Ib),
	OPF_P3(M,V,Ib),
	OPF_P3(Vo,Ib,Ib),
	OPF_P3(P,Q,Ib),
	OPF_P3(P,Ew,Ib),
	OPF_P3(H,U,Ib),
	OPF_P3(G,U,Ib),
	OPF_P3(V,U,Ib),

	OPF_P3(Mb,V,Ib),
	OPF_P3(Mw,V,Ib),

	OPF_64(Kv),
	OPF_64(M),
	OPF_i64(EwGw),
	OPF_i64(GvM),

	OPF_VHWIb	= OPF_VHW,
	OPF_VHEIb	= OPF_VHE,
	OPF_VHMbIb	= OPF_VHM,
	OPF_VHMdIb	= OPF_VHM,
	OPF_VHWL	= OPF_VHW,
	OPF_VLWH	= OPF_VHW,
	OPF_VHWLm2z	= OPF_VHWL,
	OPF_VUqIbIb	= OPF_VUIb,

	OPF_i64		= OPF_NOT64,
	OPF_o64		= OPF_ONLY64,
};

struct Opcode {
	const char *mnemonic;
	OPFLAGS		flags;
};

#define OP_BAD		{"???", OPF_0}
#define OP(m,f)		{#m, OPF_##f}
#define OPG(g,f)	{(const char*)g, OPFLAGS(OPF_##f | OPF_GROUP)}

//							0					1					2					3					4					5					6					7
Opcode group1[8]	= { OP(add, 0),			OP(or, 0),			OP(adc, 0),			OP(sbb, 0),			OP(and, 0),			OP(sub, 0),			OP(xor, 0),			OP(cmp, 0)			};
Opcode group1a[8]	= { OP(pop, 0),			OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD				};
Opcode group2[8]	= { OP(rol, 0),			OP(ror, 0),			OP(rcl, 0),			OP(rcr, 0),			OP(shl, 0),			OP(shr, 0),			OP(sal, 0),			OP(sar, 0)			};
Opcode group3[8]	= { OP(test, EI),		OP(test, EI),		OP(not, 0),			OP(neg, 0),			OP(mul, 0),			OP(imul, 0),		OP(div, 0),			OP(idiv, 0)			};
Opcode group4[8]	= { OP(inc, 0),			OP(dec, 0),			OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD				};
Opcode group5[8]	= { OP(inc, 0),			OP(dec, 0),			OP(call, 64),		OP(call, Mp),		OP(jmp, 0),			OP(jmp, Mp),		OP(push, 0),		OP_BAD				};
Opcode group11[8]	= { OP(mov, 0),			OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD				};

Opcode group6[8]	= { OP(sldt, Ev),		OP(str, Ev),		OP(lldt, Ew),		OP(ltr, Ew),		OP(verr, Ew),		OP(verw, Ew),		OP_BAD,				OP_BAD				};

Opcode group7[72]	= { OP(sgdt, Ms),		OP(sidt, Ms),		OP(lgdt, Ms),		OP(lidt, Ms),		OP(smsw, Ev),		OP_BAD,				OP(lmsw, Ew),		OP(invlpg, Mb),
/*rm=0*/				OP_BAD,				OP(monitor, 0),		OP(xgetbv, 0),		OP(vmrun, 0),		OP_BAD,				OP_BAD,				OP_BAD,				OP(swapgs, 0),
/*rm=1*/				OP(vmcall, 0),		OP(mwait, 0),		OP(xsetbv, 0),		OP(vmmvall, 0),		OP_BAD,				OP_BAD,				OP_BAD,				OP(rdtscp, 0),
/*rm=2*/				OP(vmlaunch, 0),	OP_BAD,				OP_BAD,				OP(vmload, 0),		OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*rm=3*/				OP(vmresume,0),		OP_BAD,				OP_BAD,				OP(vmsave, Mp),		OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*rm=4*/				OP(vmxoff, 0),		OP_BAD,				OP(vmfence,0),		OP(stgi, 0),		OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*rm=5*/				OP_BAD,				OP_BAD,				OP_BAD,				OP(clgi, Mp),		OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*rm=6*/				OP_BAD,				OP_BAD,				OP_BAD,				OP(skinit, 0),		OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*rm=7*/				OP_BAD,				OP_BAD,				OP_BAD,				OP(invlpga, 0),		OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
};

Opcode group8[8]	= { OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP(bt, EvIb),		OP(bts, EvIb),		OP(btr, EvIb),		OP(btc, EvIb)		};
Opcode group9[8]	= { OP_BAD,				OP(cmpxchg8b,Mq),	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD				};
//CMPXCH G8BMq	CMPXCH G16Mo

Opcode group10[8]	= { OP(UD1,0),			OP(UD1,0),			OP(UD1,0),			OP(UD1,0),			OP(UD1,0),			OP(UD1,0),			OP(UD1,0),			OP(UD1,0)			};
Opcode group12[8]	= { OP_BAD,				OP_BAD,				OP(psrlw, NqIb),	OP_BAD,				OP(psraw, NqIb),	OP_BAD,				OP(psllw, NqIb),	OP_BAD				};
Opcode group12_66[8]= { OP_BAD,				OP_BAD,				OP(psrlw, UoIb),	OP_BAD,				OP(psraw, UoIb),	OP_BAD,				OP(psllw, UoIb),	OP_BAD				};
Opcode group13[8]	= { OP_BAD,				OP_BAD,				OP(psrld, NqIb),	OP_BAD,				OP(psrad, NqIb),	OP_BAD,				OP(pslld, NqIb),	OP_BAD				};
Opcode group13_66[8]= { OP_BAD,				OP_BAD,				OP(psrld, UoIb),	OP_BAD,				OP(psrad, UoIb),	OP_BAD,				OP(pslld, UoIb),	OP_BAD				};
Opcode group14[8]	= { OP_BAD,				OP_BAD,				OP(psrlq, NqIb),	OP_BAD,				OP_BAD,				OP_BAD,				OP(psllq, NqIb),	OP_BAD				};
Opcode group14_66[8]= { OP_BAD,				OP_BAD,				OP(psrlq, UoIb),	OP(psrlo, UoIb),	OP_BAD,				OP_BAD,				OP(psllq, UoIb),	OP(psllo, UoIb)	};
Opcode group15[8]	= { OP(fxsave, M),		OP(fxrstor, M),		OP(ldmxcsr, Md),	OP(stmxcsr, Md),	OP(xsave, M),		OP(lfence, 0),		OP(mfence, 0),		OP(sfence, 0)		};
Opcode group15_1[8]	= { OP(fxsave, M),		OP(fxrstor, M),		OP(ldmxcsr, Md),	OP(stmxcsr, Md),	OP(xsave, M),		OP(xrstor, M),		OP(xsave-opt, M),	OP(clflush, Mb)		};
Opcode group15_f3[8]= { OP(rdfs-base, Rv),	OP(rdgs-base, Rv),	OP(wrfs-base, Rv),	OP(wrgs-base, Rv),	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD				};
Opcode group16[8]	= { OP(prefetch nta, 0),OP(prefetch t0, 0),	OP(prefetch t1, 0),	OP(prefetch t2, 0),	OP(nop, 0),			OP(nop, 0),			OP(nop, 0),			OP(nop, 0)			};
Opcode group17[8]	= { OP(extrq, VoIbIb),	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD				};

Opcode groupP[8]	= { OP(prefetch exclusive, 0),OP(prefetch modified, 0),OP(prefetch reserved4, 0),OP(prefetch modified, 0),OP(prefetch reserved4, 0),OP(prefetch reserved4, 0),OP(prefetch reserved4, 0),OP(prefetch reserved4, 0)};

//	x87 tables
Opcode x87_d9_e0[8] = { OP(fchs,0),			OP(fabs,0),			OP_BAD,				OP_BAD,				OP(ftst,0),			OP(fxam,0),			OP_BAD,				OP_BAD				};
Opcode x87_d9_e8[8] = { OP(fld1,0),			OP(fldl2t,0),		OP(fldl2e,0),		OP(fldpi,0),		OP(fldlg2,0),		OP(fldln2,0),		OP(fldz,0),			OP_BAD				};
Opcode x87_d9_f0[8] = { OP(f2xm1,0),		OP(fyl2x,0),		OP(fptan,0),		OP(fpatan,0),		OP(fxtract,0),		OP(fprem1,0),		OP(fdecstp,0),		OP(fincstp,0)		};
Opcode x87_d9_f8[8] = { OP(fprem,0),		OP(fyl2xp1,0),		OP(fsqrt,0),		OP(fsincos,0),		OP(frndint,0),		OP(fscale,0),		OP(fsin,0),			OP(fcos,0)			};
Opcode x87_db_e0[8] = { OP_BAD,				OP_BAD,				OP(fclex,0),		OP(finit,0),		OP(fnclex,0),		OP_BAD,				OP_BAD,				OP_BAD				};

Opcode x87_D8[16]	= { OP(fadd,Mr),		OP(fmul,Mr),		OP(fcom,Mr),		OP(fcomp,Mr),		OP(fsub,Mr),		OP(fsubr,Mr),		OP(fdiv,Mr),		OP(fdivr,Mr),
						OP(fadd,stF),		OP(fmul,stF),		OP(fcom,stF),		OP(fcomp,stF),		OP(fsub,stF),		OP(fsubr,stF),		OP(fdiv,stF),		OP(fdivr,stF)		};
Opcode x87_D9[16]	= { OP(fld,Mr),			OP_BAD,				OP(fst,M),			OP(fstp,M),			OP(fldenv,M),		OP(fldcw,M),		OP(fnstenv,Mr),		OP(fnstcw,Mr),
						OP(fld,stF),		OP(fxch,stF),		OP(fnop,0),			OP_BAD,				OPG(x87_d9_e0,0),	OPG(x87_d9_e8,0),	OPG(x87_d9_f0,0),	OPG(x87_d9_f8,0)	};
Opcode x87_DA[16]	= { OP(fiadd,Mr),		OP(fimul,Mr),		OP(ficom,Mr),		OP(ficomp,Mr),		OP(fisub,Mr),		OP(fisubr,Mr),		OP(fidiv,Mr),		OP(fidivr,Mr),
						OP(fcmovb,stF),		OP(fcmove,stF),		OP(fcmovbe,stF),	OP(fcmovu,stF),		OP_BAD,				OP(fucompp,0),		OP_BAD,				OP_BAD				};
Opcode x87_DB[16]	= { OP(fild,Mr),		OP(fisttp,Mr),		OP(fist,Mr),		OP(fistp,Mr),		OP_BAD,				OP(fld,Mb),			OP_BAD,				OP(fstp,Mb),
						OP(fcmovnb,stF),	OP(fcmovne,stF),	OP(fcmovnbe,stF),	OP(fcmovnu,stF),	OPG(x87_db_e0,0),	OP(fucomi,stF),		OP(fcomi,stF),		OP_BAD				};
Opcode x87_DC[16]	= { OP(fadd,M64),		OP(fmul,M64),		OP(fcom,M64),		OP(fcomp,M64),		OP(fsub,M64),		OP(fsubr,M64),		OP(fdiv,M64),		OP(fdivr,M64),
						OP(fadd,Fst),		OP(fmul,Fst),		OP_BAD,				OP_BAD,				OP(fsubr,Fst),		OP(fsub,Fst),		OP(fdivr,Fst),		OP(fdiv,Fst)		};
Opcode x87_DD[16]	= { OP(fld,Mr),			OP(fisttp,Mr),		OP(fst,Mr),			OP(fstp,Mr),		OP(frstor,Mr),		OP_BAD,				OP(fnsave,Mr),		OP(fstsw,Mr),
						OP(ffree,stF),		OP_BAD,				OP(fst,stF),		OP(fstp,stF),		OP(fucom,stF),		OP(fucomp,stF),		OP_BAD,				OP_BAD				};
Opcode x87_DE[16]	= { OP(fiadd,Mw),		OP(fimul,Mw),		OP(ficom,Mw),		OP(ficomp,Mw),		OP(fisub,Mw),		OP(fisubr,Mw),		OP(fidiv,Mw),		OP(fidivr,Mw),
						OP(faddp,Fst),		OP(fmulp,Fst),		OP_BAD,				OP(fcompp,Fst),		OP(fsubrp,Fst),		OP(fsubp,Fst),		OP(fdivrp,Fst),		OP(fdivp,Fst)		};
Opcode x87_DF[16]	= { OP(fild,Mr),		OP(fisttp,Mr),		OP(fist,Mr),		OP(fistp,Mr),		OP(fbld,Mb),		OP(fild,Mr),		OP(fbstp,Mb),		OP(fistp,Mr),
						OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP(fstsw,rAX),		OP(fucomip,stF),	OP(fcomip,stF),		OP_BAD				};

Opcode vexgroup12[8]= { OP_BAD,				OP_BAD,				OP(vpsrlw, HUIb),	OP_BAD,				OP(vpsraw, HUIb),	OP_BAD,				OP(vpsllw, HUIb),	OP_BAD				};
Opcode vexgroup13[8]= { OP_BAD,				OP_BAD,				OP(vpsrld, HUIb),	OP_BAD,				OP(vpsrad, HUIb),	OP_BAD,				OP(vpslld, HUIb),	OP_BAD				};
Opcode vexgroup14[8]= { OP_BAD,				OP_BAD,				OP(vpsrlq, HUIb),	OP(vpsrldq, HUIb),OP_BAD,				OP_BAD,				OP(vpsllq, HUIb),	OP(vpslldq, HUIb)	};
Opcode vexgroup15[8]= { OP_BAD,				OP_BAD,				OP(vldmxcsr, Md),	OP(vstmxcsr, Md),	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				};
Opcode vexgroup17[8]= { OP_BAD,				OP(blsr, ByEy),		OP(blsmsk, ByEy),	OP(blsi, ByEy),		OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				};

Opcode arpl_movsx[]= {OP(arpl, EwGw), OP(movsx, GvEv)};

Opcode maps[][256] = { {
//-----------------------------------------------------------------------------
//0:	PRIMARY		1 BYTE OPCODE MAP		TABLE A1/A2
//			0					1					2					3					4					5					6					7					8					9					A					B					C					D					E					F
/*0*/	OP(add, EbGb),		OP(add, EvGv),		OP(add, GbEb),		OP(add, GvEv),		OP(add, alIb),		OP(add, rAXIv),		OP(push, es),		OP(pop, es),		OP(or, EbGb),		OP(or, EvGv),		OP(or, GbEb),		OP(or, GvEv),		OP(or, alIb),		OP(or, rAXIv),		OP(push, cs),		OP_BAD,
/*1*/	OP(adc, EbGb),		OP(adc, EvGv),		OP(adc, GbEb),		OP(adc, GvEv),		OP(adc, alIb),		OP(adc, rAXIv),		OP(push, ss),		OP(pop, ss),		OP(sbb, EbGb),		OP(sbb, EvGv),		OP(sbb, GbEb),		OP(sbb, GvEv),		OP(sbb, alIb),		OP(sbb, rAXIv),		OP(push, ds),		OP(pop, ds),
/*2*/	OP(and, EbGb),		OP(and, EvGv),		OP(and, GbEb),		OP(and, GvEv),		OP(and, alIb),		OP(and, rAXIv),		OP(seg, es),		OP(daa, i64),		OP(sub, EbGb),		OP(sub, EvGv),		OP(sub, GbEb),		OP(sub, GvEv),		OP(sub, alIb),		OP(sub, rAXIv),		OP(seg, cs),		OP(das, 0),
/*3*/	OP(xor, EbGb),		OP(xor, EvGv),		OP(xor, GbEb),		OP(xor, GvEv),		OP(xor, alIb),		OP(xor, rAXIv),		OP(seg, ss),		OP(aaa, i64),		OP(cmp, EbGb),		OP(cmp, EvGv),		OP(cmp, GbEb),		OP(cmp, GvEv),		OP(cmp, alIb),		OP(cmp, rAXIv),		OP(seg, ds),		OP(aas, 0),
/*4*/	OP(inc, Kv),		OP(inc, Kv),		OP(inc, Kv),		OP(inc, Kv),		OP(inc, Kv),		OP(inc, Kv),		OP(inc, Kv),		OP(inc, Kv),		OP(dec, Kv),		OP(dec, Kv),		OP(dec, Kv),		OP(dec, Kv),		OP(dec, Kv),		OP(dec, Kv),		OP(dec, Kv),		OP(dec, Kv),
/*5*/	OP(push, Kv64),		OP(push, Kv64),		OP(push, Kv64),		OP(push, Kv64),		OP(push, Kv64),		OP(push, Kv64),		OP(push, Kv64),		OP(push, Kv64),		OP(pop, Kv64),		OP(pop, Kv64),		OP(pop, Kv64),		OP(pop, Kv64),		OP(pop, Kv64),		OP(pop, Kv64),		OP(pop, Kv64),		OP(pop, Kv64),
/*6*/	OP(pushad, 0),		OP(popad, 0),		OP(bound, GvMi64),	OPG(arpl_movsx,64),	OP(seg, fs),		OP(seg, gs),		OP_BAD/*OpSize*/,	OP_BAD/*AdSize*/,	OP(push, Iv),		OP(imul, GvEvIv),	OP(push, Ib),		OP(imul, GvEvIb),	OP(Rinsb, 0),		OP(RinsW, 0),		OP(Routsb, 0),		OP(RoutsW, 0),
/*7*/	OP(jo, Jb),			OP(jno, Jb),		OP(jc, Jb),			OP(jnc, Jb),		OP(je, Jb),			OP(jne, Jb),		OP(jbe, Jb),		OP(ja, Jb),			OP(js, Jb),			OP(jns, Jb),		OP(jp, Jb),			OP(jnp, Jb),		OP(jl, Jb),			OP(jge, Jb),		OP(jle, Jb),		OP(jg, Jb),
/*8*/	OPG(group1, EbIb),	OPG(group1, EvIv),	OPG(group1, EbIb),	OPG(group1, EvIb),	OP(test, EbGb),		OP(test, EvGv),		OP(xchg, EbGb),		OP(xchg, EvGv),		OP(mov, EbGb),		OP(mov, EvGv),		OP(mov, GbEb),		OP(mov, GvEv),		OP(mov, EwS),		OP(lea, GvM),		OP(mov, SEw),		OPG(group1a, Ev),
/*9*/	OP(nop, 0),			OP(xchg, rAXKv),	OP(xchg, rAXKv),	OP(xchg, rAXKv),	OP(xchg, rAXKv),	OP(xchg, rAXKv),	OP(xchg, rAXKv),	OP(xchg, rAXKv),	OP(cwde, 0),		OP(cdq, 0),			OP(call, Ja),		OP(fwait, 0),		OP(pushfd, 0),		OP(popfd, 0),		OP(sahf, 0),		OP(lahf, 0),
/*A*/	OP(mov, alOb),		OP(mov, rAXOv),		OP(mov, Obal),		OP(mov, OvrAX),		OP(Rmovsb, 0),		OP(RmovsW, 0),		OP(R?cmpsb, 0),		OP(R?cmpsW, 0),		OP(test, alIb),		OP(test, rAXIv),	OP(Rstosb, 0),		OP(RstosW, 0),		OP(Rlodsb, 0),		OP(RlodsW, 0),		OP(R?scasb, 0),		OP(R?scasW, 0),
/*B*/	OP(mov, KbIb),		OP(mov, KbIb),		OP(mov, KbIb),		OP(mov, KbIb),		OP(mov, KbIb),		OP(mov, KbIb),		OP(mov, KbIb),		OP(mov, KbIb),		OP(mov, KvIv),		OP(mov, KvIv),		OP(mov, KvIv),		OP(mov, KvIv),		OP(mov, KvIv),		OP(mov, KvIv),		OP(mov, KvIv),		OP(mov, KvIv),
/*C*/	OPG(group2, EbIb),	OPG(group2, EvIb),	OP(retn, Iw),		OP(ret, 0),			OP(les, GvM),		OP(lds, GvM),		OPG(group11, EbIb),	OPG(group11, EvIv),	OP(enter, IwIb),	OP(leave, 0),		OP(retf, Iw),		OP(retf, 0),		OP(int3, 0),		OP(int, Ib),		OP(into, 0),		OP(iret, 0),
/*D*/	OPG(group2, Eb1),	OPG(group2, Ev1),	OPG(group2, Ebcl),	OPG(group2, Evcl),	OP(aam, Ib),		OP(aad, Ib),		OP(salc, 0),		OP(xlat, 0),		OPG(x87_D8, 0),		OPG(x87_D9, 0),		OPG(x87_DA, 0),		OPG(x87_DB, 0),		OPG(x87_DC, 0),		OPG(x87_DD, 0),		OPG(x87_DE, 0),		OPG(x87_DF, 0),
/*E*/	OP(loopne, Jb),		OP(loope, Jb),		OP(loop, Jb),		OP(jcxz, Jb),		OP(in, alIb),		OP(in, axIb),		OP(out, Ibal),		OP(out, Ibax),		OP(call, Jz),		OP(jmp, Jz),		OP(jmp, Ja),		OP(jmp, Jb),		OP(in, aldx),		OP(in, ax),			OP(out, dxal),		OP(out, dxax),
/*F*/	OP_BAD/*Lock*/,		OP(int1, 0),		OP_BAD/*REPNE*/,	OP_BAD/*REPE*/,		OP(hlt, 0),			OP(cmc, 0),			OPG(group3, Eb),	OPG(group3, Ev),	OP(clc, 0),			OP(stc, 0),			OP(cli, 0),			OP(sti, 0),			OP(cld, 0),			OP(std, 0),			OPG(group4, Eb),	OPG(group5, Ev),
}, {
//-----------------------------------------------------------------------------
//1:	SECONDARY	2 BYTE OPCODE MAP-->0Fxx	TABLE A3/A4
//			0					1					2					3					4					5					6					7					8					9					A					B					C					D					E					F
/*0*/	OPG(group6, 0),		OPG(group7, nop),	OP(lar, GvEw),		OP(lsl, GvEw),		OP_BAD,				OP(syscall, 0),		OP(clts, 0),		OP(sysret, 0),		OP(invd, 0),		OP(wbinvd, 0),		OP_BAD,				OP(ud2, 0),			OP_BAD,				OP(nop, Ev),		OP(femms, 0),		OP_BAD/*3dnow!*/,
/*1*/	OP(movupS, VW),		OP(movupS, WV),		OP(movlpS, VM),		OP(movlpS, MV),		OP(unpcklpS, VW),	OP(unpckhpS, VW),	OP(movhpS, VM),		OP(movhpS, MV),		OPG(group16, 0),	OP(hint, nop),		OP(hint, nop),		OP(hint, nop),		OP(hint, nop),		OP(hint, nop),		OP(hint, nop),		OP(nop, Ev),
/*2*/	OP(mov, RC),		OP(mov, RD),		OP(mov, CR),		OP(mov, DR),		OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP(movapS, VW),		OP(movapS, WV),		OP(cvtpi2pS, VQ),	OP(movntpS, MoV),	OP(cvttps2pi, PW),	OP(cvtps2pi, PW),	OP(ucomiss, VW),	OP(comiss, VW),
/*3*/	OP(wrmsr, 0),		OP(rdtsc, 0),		OP(rdmsr, 0),		OP(rdpmc, 0),		OP(sysenter, 0),	OP(sysexit, 0),		OP_BAD,				OP_BAD,				OP_BAD/*tableA4*/,	OP_BAD,				OP_BAD/*tableA5*/,	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*4*/	OP(cmovo, GvEv),	OP(cmovno, GvEv),	OP(cmovb, GvEv),	OP(cmovnb, GvEv),	OP(cmove, GvEv),	OP(cmovne, GvEv),	OP(cmovbe, GvEv),	OP(cmovnbe, GvEv),	OP(cmovs, GvEv),	OP(cmovns, GvEv),	OP(cmovp, GvEv),	OP(cmovnp, GvEv),	OP(cmovl, GvEv),	OP(cmovnl, GvEv),	OP(cmovle, GvEv),	OP(cmovnle, GvEv),
/*5*/	OP(movmskpS, GdU),	OP(sqrtpS, VW),		OP(rsqrtpS, VW),	OP(rcppS, VW),		OP(andpS, VW),		OP(andnpS, VW),		OP(orpS, VW),		OP(xorpS, VW),		OP(addpS, VW),		OP(mulpS, VW),		OP(cvtps2pd, VW),	OP(cvtdq2pS, VW),	OP(subpS, VW),		OP(minpS, VW),		OP(divpS, VW),		OP(maxpS, VW),
/*6*/	OP(punpcklbw, PQ),	OP(punpcklwd, PQ),	OP(punpckldq, PQ),	OP(packsswb, PQ),	OP(pcmpgtb, PQ),	OP(pcmpgtw, PQ),	OP(pcmpgtd, PQ),	OP(packuswb, PQ),	OP(punpckhbw, PQ),	OP(punpckhwd, PQ),	OP(punpckhdq, PQ),	OP(packssdw, PQ),	OP(punpcklqdq, VW),	OP(punpckhqdq, VW),	OP(movd, PE),		OP(movq, PQ),
/*7*/	OP(pshufw, PQIb),	OPG(group12, 0),	OPG(group13, 0),	OPG(group14, 0),	OP(pcmpeqb, PQ),	OP(pcmpeqw, PQ),	OP(pcmpeqd, PQ),	OP(emms, 0),		OPG(group17, 0),	OP(vmwrite, 0),		OP_BAD,				OP_BAD,				OP(haddpd, VW),		OP(hsubpd, VW),		OP(movd, EP),		OP(movq, QP),
/*8*/	OP(jo, Jz),			OP(jno, Jz),		OP(jc, Jz),			OP(jnc, Jz),		OP(je, Jz),			OP(jne, Jz),		OP(jbe, Jz),		OP(ja, Jz),			OP(js, Jz),			OP(jns, Jz),		OP(jp, Jz),			OP(jnp, Jz),		OP(jl, Jz),			OP(jge, Jz),		OP(jle, Jz),		OP(jg, Jz),
/*9*/	OP(seto, Eb),		OP(setno, Eb),		OP(setb, Eb),		OP(setnb, Eb),		OP(sete, Eb),		OP(setne, Eb),		OP(setbe, Eb),		OP(setnbe, Eb),		OP(sets, Eb),		OP(setns, Eb),		OP(setp, Eb),		OP(setnp, Eb),		OP(setnge, Eb),		OP(setge, Eb),		OP(setle, Eb),		OP(setnle, Eb),
/*A*/	OP(push, fs),		OP(pop, fs),		OP(cpuid, 0),		OP(bt, EvGv),		OP(shld, EvGvIb),	OP(shld, EvGvcl),	OP_BAD,				OP_BAD,				OP(push, gs),		OP(pop, gs),		OP(rsm, 0),			OP(bts, EvGv),		OP(shrd, EvGvIb),	OP(shrd, EvGvcl),	OPG(group15, 0),	OP(imul, GvEv),
/*B*/	OP(cmpx, EbGb),		OP(cmpx, EvGv),		OP(lss, Mp),		OP(btr, EvGv),		OP(lfs, Mp),		OP(lgs, Mp),		OP(movzx, GvEb),	OP(movzx, GvEw),	OP(popcnt, GvEv),	OPG(group10, 0),	OPG(group8, EvIb),	OP(btc, EvGv),		OP(bsf, GvEv),		OP(bsr, GvEv),		OP(movsx, GvEb),	OP(movsx, GvEw),
/*C*/	OP(xadd, EbGb),		OP(xadd, EvGv),		OP(cmpCpS, VW),		OP(movnti, MdGd),	OP(pinsrw, PEwIb),	OP(pextrw, GdNqIb),	OP(shufpS, VWIb),	OPG(group9, 0),		OP(bswap, Kd),		OP(bswap, Kd),		OP(bswap, Kd),		OP(bswap, Kd),		OP(bswap, Kd),		OP(bswap, Kd),		OP(bswap, Kd),		OP(bswap, Kd),
/*D*/	OP(addsubpd, VW),	OP(psrlw, PQ),		OP(psrld, PQ),		OP(psrlq, PQ),		OP(paddq, PQ),		OP(pmullw, PQ),		OP(movq, WV),		OP(pmovmskb, GdU),	OP(psubusb, PQ),	OP(psubusw, PQ),	OP(pminub, PQ),		OP(pand, PQ),		OP(paddusb, PQ),	OP(paddusw, PQ),	OP(pmaxub, PQ),		OP(pandn, PQ),
/*E*/	OP(pavgb, PQ),		OP(psraw, PQ),		OP(psrad, PQ),		OP(pavgw, PQ),		OP(pmulhuw, PQ),	OP(pmulhw, PQ),		OP(cvtpd2dq, VW),	OP(movntq, MqP),	OP(psubsb, PQ),		OP(psubsw, PQ),		OP(pminsw, PQ),		OP(por, PQ),		OP(paddsb, PQ),		OP(paddsw, PQ),		OP(pmaxsw, PQ),		OP(pxor, PQ),
/*F*/	OP(lddqu, VMo),		OP(psllw, PQ),		OP(pslld, PQ),		OP(psllq, PQ),		OP(pmuludq, PQ),	OP(pmaddwd, PQ),	OP(psadbw, PQ),		OP(maskmovq, PQ),	OP(psubb, PQ),		OP(psubw, PQ),		OP(psubd, PQ),		OP(psubq, PQ),		OP(paddb, PQ),		OP(paddw, PQ),		OP(paddd, PQ),		OP(ud0, 0),
}, {
//2:	SECONDARY	660Fxx	TABLE A3/A4
//			0					1					2					3					4					5					6					7					8					9					A					B					C					D					E					F
/*0*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*1*/	OP(movupd, VW),		OP(movupd, WV),		OP(movlpd, VM),		OP(movlpd, MV),		OP(unpcklpd, VW),	OP(unpckhpd, VW),	OP(movhpd, VM),		OP(movhpd, MV),		OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP(nop, Ev),
/*2*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP(movapd, VW),		OP(movapd, WV),		OP(cvtpi2pd, VQ),	OP(movntpd, MV),	OP(cvttpd2pi, PW),	OP(cvtpd2pi, PW),	OP(ucomisd, VW),	OP(comisd, VW),
/*3*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*4*/	OP(cmovo, GvEv),	OP(cmovno, GvEv),	OP(cmovb, GvEv),	OP(cmovnb, GvEv),	OP(cmove, GvEv),	OP(cmovne, GvEv),	OP(cmovbe, GvEv),	OP(cmovnbe, GvEv),	OP(cmovs, GvEv),	OP(cmovns, GvEv),	OP(cmovp, GvEv),	OP(cmovnp, GvEv),	OP(cmovl, GvEv),	OP(cmovnl, GvEv),	OP(cmovle, GvEv),	OP(cmovnle, GvEv),
/*5*/	OP(movmskpd, GdU),	OP(sqrtpd, VW),		OP_BAD,				OP_BAD,				OP(andpd, VW),		OP(andnpd, VW),		OP(orpd, VW),		OP(xorpd, VW),		OP(addpd, VW),		OP(mulpd, VW),		OP(cvtpd2ps, VW),	OP(cvtps2dq, VW),	OP(subpd, VW),		OP(minpd, VW),		OP(divpd, VW),		OP(maxpd, VW),
/*6*/	OP(punpcklbw, VW),	OP(punpcklwd, VW),	OP(punpckldq, VW),	OP(packsswb, VW),	OP(pcmpgtb, VW),	OP(pcmpgtw, VW),	OP(pcmpgtd, VW),	OP(packuswb, VW),	OP(punpckhbw, VW),	OP(punpckhwd, VW),	OP(punpckhdq, VW),	OP(packssdw, VW),	OP(punpcklqdq, VW),	OP(punpckhqdq, VW),	OP(movd, VE),		OP(movdqa, VW),
/*7*/	OP(pshufd, VWIb),	OPG(group12_66, 0),	OPG(group13_66, 0),	OPG(group14_66, 0),	OP(pcmpeqb, VW),	OP(pcmpeqw, VW),	OP(pcmpeqd, VW),	OP_BAD,				OPG(group17,0),		OP(extrq, VU),		OP_BAD,				OP_BAD,				OP(haddpd, VW),		OP(hsubpd, VW),		OP(movd, EV),		OP(movdqa, WV),
/*8*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*9*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*a*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*b*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*C*/	OP(xadd, EbGb),		OP(xadd, EvGv),		OP(cmppd, VWIb),	OP_BAD,				OP(pinsrw, VEwIb),	OP(pextrw, GUIb),	OP(shufpd, VWIb),	OPG(group9, 0),		OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*D*/	OP(addsubpd, VW),	OP(psrlw, VW),		OP(psrld, VW),		OP(psrlq, VW),		OP(paddq, VW),		OP(pmullw, VW),		OP(movq, WV),		OP(pmovmskb, GdU),	OP(psubusb, VW),	OP(psubusw, VW),	OP(pminub, VW),		OP(pand, VW),		OP(paddusb, VW),	OP(paddusw, VW),	OP(pmaxub, VW),		OP(pandn, VW),
/*E*/	OP(pavgb, VW),		OP(psraw, VW),		OP(psrad, VW),		OP(pavgw, VW),		OP(pmulhuw, VW),	OP(pmulhw, VW),		OP(cvttpd2dq, VW),	OP(movntdq, MV),	OP(psubsb, VW),		OP(psubsw, VW),		OP(pminsw, VW),		OP(por, VW),		OP(paddsb, VW),		OP(paddsw, VW),		OP(pmaxsw, VW),		OP(pxor, VW),
/*F*/	OP_BAD,				OP(psllw, VW),		OP(pslld, VW),		OP(psllq, VW),		OP(pmuludq, VW),	OP(pmaddwd, VW),	OP(psadbw, VW),		OP(maskmovdqu, VU),	OP(psubb, VW),		OP(psubw, VW),		OP(psubd, VW),		OP(psubq, VW),		OP(paddb, VW),		OP(paddw, VW),		OP(paddd, VW),		OP(ud0, 0),
}, {
//3:	SECONDARY	F30Fxx	TABLE A3/A4
//			0					1					2					3					4					5					6					7					8					9					A					B					C					D					E					F
/*0*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*1*/	OP(movss, VW),		OP(movss, WV),		OP(movsldup, VW),	OP_BAD,				OP_BAD,				OP_BAD,				OP(movshdup, VW),	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*2*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP(cvtsi2ss, VE),	OP(movntss, MV),	OP(cvttss2si, GW),	OP(cvtss2si0, GW),	OP_BAD,				OP_BAD,
/*3*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*4*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*5*/	OP_BAD,				OP(sqrtss, VW),		OP(rsqrtss, VW),	OP(rcpss, VW),		OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP(addss, VW),		OP(mulss, VW),		OP(cvtss2sd, VW),	OP(cvttps2dq, VW),	OP(subss, VW),		OP(minss, VW),		OP(divss, VW),		OP(maxss, VW),
/*6*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP(movdqu, VW),
/*7*/	OP(pshufhw, VWIb),	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP(movq, VW),		OP(movdqu, WV),
/*8*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*9*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*a*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*b*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP(popcnt, GvEv),	OP_BAD,				OP_BAD,				OP_BAD,				OP(tzcnt, GvEv),	OP(lzcnt, GvEv),	OP_BAD,				OP_BAD,
/*c*/	OP(xadd, EbGb),		OP(xadd, EvGv),		OP(cmpss, VWIb),	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OPG(group9, 0),		OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*d*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP(movq2dq, VN),	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*e*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP(cvtdq2pd, VW),	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*f*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
}, {
//4:	SECONDARY	F20Fxx	TABLE A3/A4
//			0					1					2					3					4					5					6					7					8					9					A					B					C					D					E					F
/*0*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*1*/	OP(movsd, VW),	OP(movsd, WV),			OP(movddup, VW),	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*2*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP(cvtsi2sd, VE),	OP(movntsd, MV),	OP(cvttsd2si, GW),	OP(cvtsd2si, GW),	OP_BAD,				OP_BAD,
/*3*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*4*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*5*/	OP_BAD,				OP(sqrtsd, VW),		OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP(addsd, VW),		OP(mulsd, VW),		OP(cvtsd2ss, VW),	OP_BAD,				OP(subsd, VW),		OP(minsd, VW),		OP(divsd, VW),		OP(maxsd, VW),
/*6*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*7*/	OP(pshuflw, VWIb),	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP(insertq, VUqIbIb),OP(insertq, VU),	OP_BAD,				OP_BAD,				OP(haddps, VW),		OP(hsubps, VW),		OP_BAD,				OP_BAD,
/*8*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*9*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*a*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*b*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*c*/	OP(xadd, EbGb),		OP(xadd, EvGv),		OP(cmpsd, VWIb),	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OPG(group9, 0),		OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*d*/	OP(addsubps, VW),	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP(movdq2q, PU),	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*e*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP(cvtpd2dq, VW),	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*f*/	OP(lddqu, VM),		OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
}, {
//-----------------------------------------------------------------------------
//5:	3 BYTE OPCODE MAP-->0F38xx	TABLE A9/A10
//			0					1					2					3					4					5					6					7					8					9					A					B					C					D					E					F
/*0*/	OP(pshufb, PQ),		OP(phaddw, PQ),		OP(phaddd, PQ),		OP(phaddsw, PQ),	OP(pmaddubsw, PQ),	OP(phsubw, PQ),		OP(phsubd, PQ),		OP(phsubsw, PQ),	OP(psignb, PQ),	OP(psignw, PQ),			OP(psignd, PQ),		OP(pmulhrsw, PQ),	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*1*/	OP(pblendvb, VW),	OP_BAD,				OP_BAD,				OP_BAD,				OP(blendvps, VW),	OP(blendvpd, VW),	OP_BAD,				OP(ptest, VW),		OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP(pabsb, VW),		OP(pabsw, VW),		OP(pabsd, VW),		OP_BAD,
/*2*/	OP(pmovsxbw, VW),	OP(pmovsxbd, VW),	OP(pmovsxbq, VW),	OP(pmovsxwd, VW),	OP(pmovsxwq, VW),	OP(pmovsxdq, VW),	OP_BAD,				OP_BAD,				OP(pmuldq, VW),		OP(pcmpeqq, VW),	OP(movntdqa, VW),	OP(packusdw, VW),	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*3*/	OP(pmovzxbw, VW),	OP(pmovzxbd, VW),	OP(pmovzxbq, VW),	OP(pmovzxwd, VW),	OP(pmovzxwq, VW),	OP(pmovzxdq, VW),	OP_BAD,				OP(pcmpgtq, VW),	OP(pminsb, VW),		OP(pminsd, VW),		OP(pminuw, VW),		OP(pminud, VW),		OP(pmaxsb, VW),		OP(pmaxsd, VW),		OP(pmaxuw, VW),		OP(pmaxud, VW),
/*4*/	OP(pmulld, VW),		OP(phminposuw, VW),	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*5*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*6*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*7*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*8*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*9*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*A*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*B*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*C*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*D*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP(aesimc, VW),		OP(aesenc, VW),		OP(aesenclast, VW),	OP(aesdec, VW),		OP(aesdeclast, VW),
/*E*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*F*/	OP(crc32, GvEb),	OP(crc32, GvEv),	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
}, {
//6:	3 BYTE OPCODE MAP-->0F3Axx	TABLE A11/A12
//			0					1					2					3					4					5					6					7					8					9					A					B					C					D					E					F
/*0*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP(roundps, VWIb),	OP(roundpd, VWIb),	OP(roundss, VWIb),	OP(roundsd, VWIb),	OP(blendps, VWIb),	OP(blendpd, VWIb),	OP(pblendw, VWIb),	OP(palignr, VWIb),
/*1*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP(pextrb, EbVIb),	OP(pextrw2, EwVIb),	OP(pextrd, EdVIb),	OP(extractps, EdVIb),OP_BAD,			OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*2*/	OP(pinsrb, VEbIb),	OP(insertps, VEdIb),OP(pinsrd, VEdIb),	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*3*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*4*/	OP(dpps, VWIb),		OP(dppd, VWIb),		OP(mpsadbw, VWIb),	OP_BAD,				OP(pclmulqdq, VWIb),OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*5*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*6*/	OP(pcmpestrm, VWIb),OP(pcmpestri, VWIb),OP(pcmpistrm, VWIb),OP(pcmpistri, VWIb),OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*7*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*8*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*9*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*A*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*B*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*C*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*D*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP(aeskeygen, VWIb),
/*E*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*F*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
}, {
//-----------------------------------------------------------------------------
//7:	3DNOW! OPCODE MAP-->0F0Fxx
//			0					1					2					3					4					5					6					7					8					9					A					B					C					D					E					F
/*0*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP(pi2fw, PqQq),	OP(pi2fd, PqQq),	OP_BAD,				OP_BAD,
/*1*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP(pf2iw, PqQq),	OP(pf2id, PqQq),	OP_BAD,				OP_BAD,
/*2*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*3*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*4*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*5*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*6*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*7*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*8*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP(pfnacc, PqQq),	OP_BAD,				OP_BAD,				OP_BAD,				OP(pfpnacc, PqQq),	OP_BAD,
/*9*/	OP(pfcmpge, PqQq),	OP_BAD,				OP_BAD,				OP_BAD,				OP(pfmin, PqQq),	OP_BAD,				OP(pfrcp, PqQq),	OP(pfrsqrt, PqQq),	OP_BAD,				OP_BAD,				OP(pfsub, PqQq),	OP_BAD,				OP_BAD,				OP_BAD,				OP(pfadd, PqQq),	OP_BAD,
/*A*/	OP(pfcmpgt, PqQq),	OP_BAD,				OP_BAD,				OP_BAD,				OP(pfmax, PqQq),	OP_BAD,				OP(pfrcpit1, PqQq),	OP(pfrsqit1, PqQq),	OP_BAD,				OP_BAD,				OP(pfsubr, PqQq),	OP_BAD,				OP_BAD,				OP_BAD,				OP(pfacc, PqQq),	OP_BAD,
/*B*/	OP(pfcmpeq, PqQq),	OP_BAD,				OP_BAD,				OP_BAD,				OP(pfmul, PqQq),	OP_BAD,				OP(pfrcpit2, PqQq),	OP(pmulhrw, PqQq),	OP_BAD,				OP_BAD,				OP_BAD,				OP(pswapd, PqQq),	OP_BAD,				OP_BAD,				OP_BAD,				OP(pavgusb, PqQq),
/*C*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*D*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*E*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*F*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
}, {
//-----------------------------------------------------------------------------
//8:	VEX MAP 1	TABLE A17/A18/A19
//			0					1					2					3					4					5					6					7					8					9					A					B					C					D					E					F
/*0*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*1*/	OP(vmovups, VW),	OP(vmovups, WV),	OP(vmovlps, VHMq),	OP(vmovlps, MqV),	OP(vunpcklps2, VHW),OP(vunpckh2, VHW),	OP(vmovhps, VHMq),	OP(vmovhps, MqV),	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*2*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP(vmovaps, VW),	OP(vmovaps, WV),	OP_BAD,				OP(vmovntps, MV),	OP_BAD,				OP_BAD,				OP(vucomiss, VW),	OP(vcomiss, VW),
/*3*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*4*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*5*/	OP(vmovmskps, GU),	OP(vsqrtps, VW),	OP(vrsqrtps, VW),	OP(vrcpps, VW),		OP(vandps, VHW),	OP(vandnps, VHW),	OP(vorps, VHW),		OP(vxorps, VHW),	OP(vaddPS, VHW),	OP(vmulps, VHW),	OP(vcvtps2pd, VW),	OP(vcvtdq2ps, VW),	OP(vsubps, VHW),	OP(vminps, VHW),	OP(vdivps, VHW),	OP(vmaxps, VHW),
/*6*/	OP(vpunpcklbw, VHW),OP(vpunpcklwd, VHW),OP(vpunpckldq, VHW),OP(vpacksswb, VHW),	OP(vpcmpgtb, VHW),	OP(vpcmpgtw, VHW),	OP(vpcmpgtd, VHW),	OP(vpackuswb, VHW),	OP(vpunpckhbw, VHW),OP(vpunpckhwd, VHW),OP(vpunpckhdq, VHW),OP(vpackssdw, VHW),OP(vpunpcklqdq, VHW),OP(vpunpckhqdq, VHW),OP(vmovd, VE),		OP(vmovdqa, VW),
/*7*/	OP(vpshufd, VWIb),	OPG(vexgroup12,0),	OPG(vexgroup13,0),	OPG(vexgroup14,0),	OP(vpcmpeqb, VHW),	OP(vpcmpeqw, VHW),	OP(vpcmpeqd, VHW),	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP(vhaddpd, VHW),	OP(vhsubpd, VHW),	OP(vmovd, EV),		OP(vmovdqa, WV),
/*8*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*9*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*A*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OPG(vexgroup15,0),	OP_BAD,
/*B*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*C*/	OP_BAD,				OP_BAD,				OP(vcmpCps, VHW),	OP_BAD,				OP_BAD,				OP_BAD,				OP(vshufps, VHWIb),	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*D*/	OP(vaddsubpd, VHW),	OP(vpsrlw, VHW),	OP(vpsrld, VHW),	OP(vpsrlq, VHW),	OP(vpaddq, VHW),	OP(vpmullw, VHW),	OP(vmovq, W),		OP(vpmovmskb, GU),	OP(vpsubusb, VHW),	OP(vpsubusw, VHW),	OP(vpminub, VHW),	OP(vpand, VHW),		OP(vpaddusb, VHW),	OP(vpaddusw, VHW),	OP(vpmaxub, VHW),	OP(vpandn, VHW),
/*E*/	OP(vpavgb, VHW),	OP(vpsraw, VHW),	OP(vpsrad, VHW),	OP(vpavgw, VHW),	OP(vpmulhuw, VHW),	OP(vpmulhw, VHW),	OP(vcvttpd2dq, VW),	OP(vmovntdq, MV),	OP(vpsubsb, VHW),	OP(vpsubsw, VHW),	OP(vpminsw, VHW),	OP(vpor, VHW),		OP(vpaddsb, VHW),	OP(vpaddsw, VHW),	OP(vpmaxsw, VHW),	OP(vpxor, VHW),
/*F*/	OP_BAD,				OP(vpsllw, VHW),	OP(vpslld, VHW),	OP(vpsllq, VHW),	OP(vpmuludq, VHW),	OP(vpmaddwd, VHW),	OP(vpsadbw, VHW),	OP(vmaskmovdqu, VU),OP(vpsubb, VHW),	OP(vpsubw, VHW),	OP(vpsubd, VHW),	OP(vpsubq, VHW),	OP(vpaddb, VHW),	OP(vpaddw, VHW),	OP(vpaddd, VHW),	OP_BAD,
}, {
//9:	VEX MAP 2	TABLE A20/A21
//			0					1					2					3					4					5					6					7					8					9					A					B					C					D					E					F
/*0*/	OP(vpshufb, VHW),	OP(vphaddw, VHW),	OP(vphaddd, VHW),	OP(vphaddsw, VHW),	OP(vpmaddubsw, VHW),OP(vphsubw, VHW),	OP(vphsubd, VHW),	OP(vphsubsw, VHW),	OP(vpsignb, VHW),	OP(vpsignw, VHW),	OP(vpsignd, VHW),	OP(vpmulhrsw, VHW),	OP(vpermilps, VHW),	OP(vpermilpd, VHW),	OP(vtestps, VW),	OP(vtestpd, VW),
/*1*/	OP_BAD,				OP_BAD,				OP_BAD,				OP(vcvtph2ps, VW),	OP_BAD,				OP_BAD,				OP_BAD,				OP(vptest, VW),		OP(vbroadcastss, VM),OP(vbroadcastsd, VM),OP(vbroadcastf128, VM),OP_BAD,		OP(vpabsb, VW),		OP(vpabsw, VW),		OP(vpabsd, VW),		OP_BAD,
/*2*/	OP(vpmovsxbw, VW),	OP(vpmovsxbd, VW),	OP(vpmovsxbq, VW),	OP(vpmovsxwd, VW),	OP(vpmovzxwq, VW),	OP(vpmovsxdq, VW),	OP_BAD,				OP_BAD,				OP(vpmuldq, VHW),	OP(vpcmpeqq, VHW),	OP(vmovntdqa, MV),	OP(vpackusdw, VHW),	OP(vmaskmovps, VHM),OP(vmaskmovpd, VHM),OP(vmaskmovps, MHV),OP(vmaskmovpd, MHV),
/*3*/	OP(vpmovzxbw, VW),	OP(vpmovzxbd, VW),	OP(vpmovzxbq, VW),	OP(vpmovzxwd, VW),	OP(vpmovsxwq, VW),	OP(vpmovzxdq, VW),	OP_BAD,				OP(vpcmpgtq, VHW),	OP(vpminsb, VHW),	OP(vpminsd, VHW),	OP(vpminuw, VHW),	OP(vpminud, VHW),	OP(vpmaxsb, VHW),	OP(vpmaxsd, VHW),	OP(vpmaxuw, VHW),	OP(vpmaxud, VHW),
/*4*/	OP(vpmulld, VHW),	OP(vphminposuw, VW),OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*5*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP(vbroadcasti128, VM),OP_BAD,			OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*6*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*7*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*8*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*9*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP(vfmaddsub132pS, VHW),OP(vfmsubadd132pS, VHW),OP(vfmadd132pS, VHW),OP(vfmadd132pS, VHW),OP(vfmsub132pS, VHW),OP(vfmsub132pS, VHW),OP(vfnmadd132pS, VHW),OP(vfnmadd132pS, VHW),OP(vfnmsub132pS, VHW),OP(vfnmsub132pS, VHW),
/*A*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP(vfmaddsub213pS, VHW),OP(vfmsubadd213pS, VHW),OP(vfmadd213pS, VHW),OP(vfmadd213pS, VHW),OP(vfmsub213pS, VHW),OP(vfmsub213pS, VHW),OP(vfnmadd213pS, VHW),OP(vfnmadd213pS, VHW),OP(vfnmsub213pS, VHW),OP(vfnmsub213pS, VHW),
/*B*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP(vfmaddsub231pS, VHW),OP(vfmsubadd231pS, VHW),OP(vfmadd231pS, VHW),OP(vfmadd231pS, VHW),OP(vfmsub231pS, VHW),OP(vfmsub231pS, VHW),OP(vfnmadd231pS, VHW),OP(vfnmadd231pS, VHW),OP(vfnmsub231pS, VHW),OP(vfnmsub231pS, VHW),
/*C*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*D*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP(vaesimc, VW),	OP(vaesenc, VHW),	OP(vaesenclast, VHW),OP(vaesdec, VHW),	OP(vaesdeclast, VHW),
/*E*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*F*/	OP_BAD,				OP_BAD,				OP(andn, GyByEy),	OPG(vexgroup17,0),	OP_BAD,				OP_BAD,				OP_BAD,				OP(bextr, GyEyBy),OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
}, {
//10:	VEX MAP 3	TABLE A22/A23
//			0					1					2					3					4					5					6					7					8					9					A					B					C					D					E					F
/*0*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP(vpermilps, VWIb),OP(vpermilpd, VWIb),OP(vperm2f128, VHWIb),OP_BAD,			OP(vroundps, VWIb),	OP(vroundpd, VWIb),	OP(vroundss, VHWIb),OP(vroundsd, VHWIb),OP(vblendps, VHWIb),OP(vblendpd, VHWIb),OP(vpblendw, VHWIb),OP(vpalignr, VHWIb),
/*1*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP(vpextrb, MbVIb),	OP(vpextrw, MwVIb),	OP(vpextrd, EVIb),	OP(vpextractps, MVIb),	OP(vinsertf128, VHWIb),OP(vextractf128, WVIb),OP_BAD,	OP_BAD,				OP_BAD,				OP(vcvtps2ph, WVIb),OP_BAD,				OP_BAD,
/*2*/	OP(vpinsrb, VHMbIb),OP(vinsertps, VHMdIb),OP(vpinsrD, VHEIb),OP_BAD,			OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*3*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*4*/	OP(vdpps, VHWIb),	OP(vdppd, VHWIb),OP(vmpsadbw, VHWIb),	OP_BAD,				OP(vpclmulqdq, VHWIb),OP_BAD,			OP_BAD,				OP_BAD,				OP(vpermil2ps, VHWLm2z),OP(vpermil2pd, VHWLm2z),OP(vblendvps, VHWL),OP(vblendvpd, VHWL),OP(vpblendvb, VHWL),OP_BAD,		OP_BAD,					OP_BAD,
/*5*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP(vfmaddsubps, VLWH),OP(vfmaddsubpd, VLWH),OP(vfmsubaddps, VLWH),OP(vfmsubaddpd, VLWH),
/*6*/	OP(vpcmpestrm, VWIb),OP(vpcmpestri, VWIb),OP(vpcmpistrm, VWIb),OP(vpcmpistri, VWIb),OP_BAD,			OP_BAD,				OP_BAD,				OP_BAD,				OP(vfmaddps, VLWH),	OP(vfmaddpd, VLWH),	OP(vfmaddss, VLWH),	OP(vfmaddsd, VLWH),	OP(vfmsubps, VLWH),	OP(vfmsubpd, VLWH),	OP(vfmsubss, VLWH),OP(vfmsubsd, VLWH),
/*7*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP(vfnmaddps, VLWH),OP(vfnmaddpd, VLWH),OP(vfnmaddss, VLWH),OP(vfnmaddsd, VLWH),OP(vfnmsubps, VLWH),OP(vfnmsubpd, VLWH),OP(vfnmsubss, VLWH),OP(vfnmsubsd, VLWH),
/*8*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*9*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*A*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*B*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*C*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*D*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP(vaeskeygenassist, VWIb),
/*E*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
/*F*/	OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,				OP_BAD,
} };

const char *IntelOp::reg8[]	= {
	"al", "cl", "dl", "bl", "ah", "ch", "dh", "bh",
	"r8b", "r9b", "r10b", "r11b", "r12b", "r13b", "r14b", "r15b",
}, *IntelOp::reg16[]	= {
	"ax", "cx", "dx", "bx", "sp", "bp", "si", "di",
	"r8w", "r9w", "r10w", "r11w", "r12w", "r13w", "r14w", "r15w",
}, *IntelOp::reg32[]	= {
	"eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi",
	"r8d", "r9d", "r10d", "r11d", "r12d", "r13d", "r14d", "r15d",
}, *IntelOp::reg64[]	= {
	"rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi",
	"r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
	"rip",
}, **IntelOp::regs[4] = {
	reg8, reg16, reg32, reg64
};

const char *IntelOp::bases16[] = {
	"bx + si", "bx + di", "bp + si", "bp + di", "si", "di", "bp", "bx"
};
const char *IntelOp::sizes[] = {
	"byte", "word", "dword", "qword", "tword", "oword", "doword",
};

const char *IntelOp::compares[] = {
	"eq", "lt", "le", "unord", "neq", "nlt", "nle", "and ord",
	"eq_uq", "nge", "ngt", "false", "neq_oq", "ge", "gt", "true",
	"eq_os", "lt_oq", "le_oq", "unord_s", "neq_us", "nlt_uq", "nle_uq", "ord_s",
	"eq_us", "nge_uq", "ngt_uq", "false_os", "neq_os", "ge_oq", "gt_oq", "true_us",
};

IntelOp::SIZE IntelOp::GetSize(uint8 arg) {
	int	size	= arg & OPF_SIZE;
	return	size == OPF_b						? (prefix.test(PREFIX::X87) ? SIZE_10 : SIZE_1)
		:	size == OPF_w						? SIZE_2
		:	size == OPF_d						? SIZE_4
		:	prefix.test(PREFIX::W)				? SIZE_8
		:	prefix.test(PREFIX::OperandSize)	? SIZE_2
		:	SIZE_4;
}

void IntelOp::GeneralRegister(string_accum &sa, int reg, uint8 arg) {
	sa << regs[GetSize(arg)][reg];
}

void IntelOp::Address(string_accum &sa, Disassembler::SymbolFinder sym_finder, uint8 arg) {
	if (noregs) {
		SIZE	size = GetSize(arg);
		sa << sizes[size] << " ptr ";
	}

	sa << '[';

	int	mod = modrm_mod;
	int	reg = modrm_rm;

	if (arch == X16) {
		if (mod == 0 && reg == 6)
			mod = 2;
		else
			sa << bases16[reg];
		if (mod) {
			if (displacement < 0)
				sa << " - " << -displacement;
			else
				sa << " + " << displacement;
		}
	} else {
		const char **regs	= arch == X64 ? reg64 : reg32;
		bool		rip		= arch == X64;
		bool		add		= false;
		if ((reg & 7) == 4) {	// sib
			if (sib_base != 4) {
				sa << regs[sib_index];
				if (sib_scale)
					sa << " * " << (1 << sib_scale);
				add = true;
			}
			reg = sib_base;
			rip	= false;
		}
		if (mod == 0 && reg == 5) {
			MemoryAddress(sa, sym_finder, rip ? displacement + nextaddress : displacement);
//			mod	= 2;
//			reg	= rip ? 16 : -1;	//rip or absolute
		} else {
			if (regs) {
				if (add)
					sa << " + ";
				sa << regs[reg];
			}
			if (mod) {
				if (displacement < 0)
					sa << " - " << -displacement;
				else
					sa << " + " << displacement;
			}
		}
	}
	sa << ']';
}

void IntelOp::Argument(string_accum &sa, Disassembler::SymbolFinder sym_finder, uint8 arg) {
	static const char *segment[]	= {"es", "cs", "ss", "ds", "fs", "gs", "BADSEG", "BADSEG"};

	int	reg		= modrm_reg;

	switch (arg & OPF_TYPE) {
		case OPF_1:
			sa << '1';
			break;

		case OPF_rAX:case OPF_rCX:case OPF_rDX:case OPF_rBX:
			GeneralRegister(sa, (arg - OPF_reg) & 7, arg);
			break;

		case OPF_es:case OPF_cs:case OPF_ss:case OPF_ds:case OPF_fs:case OPF_gs:
			sa << segment[(arg - OPF_seg) & 7];
			break;

		case OPF_st:
			sa << "st";
			break;
		case OPF_F:
			sa << "st(" << modrm_rm << ")";
			break;

		case OPF_K:	// reg in bottom 3 bits of op (al,cl,dl,bl,ah,ch,dh,bh)
			GeneralRegister(sa, (opcode & 7) | (prefix.test(PREFIX::B) << 3), 0);
			break;

		case OPF_E:	// gen reg or mem in modrm.rm
			if (modrm_mod != 3) {
				Address(sa, sym_finder, arg);
				break;
			}
			reg = modrm_rm;
		case OPF_G:	// gen reg in modrm.reg
			GeneralRegister(sa, reg, arg);
			break;
		case OPF_B:
			GeneralRegister(sa, vex_reg, arg);
			break;

		case OPF_W:	// xmm or mem in modrm
			if (modrm_mod != 3) {
				Address(sa, sym_finder, arg);
				break;
			}
			reg = modrm_rm;
		case OPF_V:	// xmm in modrm.reg
			sa << (prefix.test(PREFIX::L) ? "ymm" : "xmm") << reg;
			break;
		case OPF_H:	// ymm or xmm in VEX.vvvv
			sa << (prefix.test(PREFIX::L) ? "ymm" : "xmm") << vex_reg;
			break;

		case OPF_Q:	// 64 bit mmx reg or mem in modrm.rm
			if (modrm_mod != 3) {
				Address(sa, sym_finder, arg);
				break;
			}
			reg = modrm_rm;
		case OPF_P:	// 64 bit mmx reg in modrm.reg
			//if (prefix.test(PREFIX::OperandSize))
			//	sa << (prefix.test(PREFIX::L) ? 'y' : 'x');
			sa << "mm" << (reg & 7);
			break;

		case OPF_S:	// seg reg in modrm.reg
			sa << segment[reg & 7];
			break;

		case OPF_C:	// control reg in modrm.reg
			sa << "CR" << reg;
			break;

		case OPF_D:	// debug reg in modrm.reg
			sa << "DR" << reg;
			break;

		case OPF_O:	// offset in ins
			return;

		case OPF_I:	// immediate
			sa << prefix_hex(immediate[0]);
			immediate[0] = immediate[1];
			return;

		case OPF_J:
			MemoryAddress(sa, sym_finder, (arg & OPF_SIZE) == OPF_d ? immediate[0] : nextaddress + immediate[0]);
			return;

		case OPF_nop:// eat modrm
			return;

		case OPF_Mp:// 32 or 48 bit far pointer
			sa << prefix_hex(displacement);
			return;

		case OPF_M:	// mem from modrm
			Address(sa, sym_finder, arg);
			return;
	}
}

uint64	IntelOp::GetDest() const {
	switch (args & OPF_TYPE) {
		case OPF_J:
			return immediate[0];

		case OPF_E:
			if (arch == X64 && modrm_mod == 0 && modrm_rm == 5)
				return displacement;
			break;
	}
	return 0;
}

const uint8 *IntelOp::Parse(const uint8 *p) {
	clear(prefix);

	opcode		= *p;
	info		= 0;
	args		= 0;
	mnemonic	= 0;

	// legacy prefixes (and REX)
	for (bool legacy = true; legacy;) {
		switch (opcode = *p++) {
			case 0x66:	prefix.add(PREFIX::OperandSize); break;
			case 0x67:	prefix.add(PREFIX::AddressSize); break;
			case 0x2E:	prefix.add(PREFIX::CS); break;
			case 0x3E:	prefix.add(PREFIX::DS); break;
			case 0x26:	prefix.add(PREFIX::ES); break;
			case 0x64:	prefix.add(PREFIX::FS); break;
			case 0x65:	prefix.add(PREFIX::GS); break;
			case 0x36:	prefix.add(PREFIX::SS); break;
			case 0xF0:	prefix.add(PREFIX::Lock); break;
			case 0xF3:	prefix.add(PREFIX::Repe); break;
			case 0xF2:	prefix.add(PREFIX::Repne); break;

			case 0x40:case 0x41:case 0x42:case 0x43:case 0x44:case 0x45:case 0x46:case 0x47:
			case 0x48:case 0x49:case 0x4a:case 0x4b:case 0x4c:case 0x4d:case 0x4e:case 0x4f:
				if (arch == X64) {
					prefix.add(PREFIX::TYPE(opcode & 0x0f));
					opcode = *p++;
				}
			default:	legacy = false; break;
		}
	}

	uint8	map	= MAP_PRIMARY;

	// escape sequences
	switch (opcode) {
		case 0x0f:	// legacy
			switch (opcode = *p++) {
				case 0x0f:	//3dNow!
					opcode	= *p++;
					map		= MAP_3DNOW;
					break;
				case 0x38:	//sse
					opcode	= *p++;
					map		= MAP_0F38;
					break;
				case 0x3a:	//more sse
					opcode	= *p++;
					map		= MAP_0F3A;
					break;
				default:
					map		= MAP_SECONDARY + prefix.extension();
					break;
			}
			break;

		case 0x8f:	// XOP
			if ((*p & 0x1f) < 8)
				break;
			prefix.addVEX(p);
			map		= MAP_VEX + (*p++ & 0x1f) - 1;
			vex_reg	= (~*p++ >> 3) & 15;
			opcode	= *p++;
			break;

		case 0xc4:	// VEX c4
			prefix.addVEX(p);
			map		= MAP_VEX + (*p++ & 0x1f) - 1;
			vex_reg	= (~*p++ >> 3) & 15;
			opcode	= *p++;
			break;

		case 0xc5:	// VEX c5
			prefix.add(PREFIX::TYPE((*p & PREFIX::L) | PREFIX::X | PREFIX::B | (*p & 0x80 ? PREFIX::R : 0)));
			prefix.addPP(*p);
			map		= MAP_VEX;
			vex_reg	= (~*p++ >> 3) & 15;
			opcode	= *p++;
			break;

		case 0xd8:case 0xd9:case 0xda:case 0xdb:case 0xdc:case 0xdd:case 0xde:case 0xdf:
			prefix.add(PREFIX::X87);
			break;
	}

	if (map >= _MAP_TOTAL)
		return p;

	Opcode	op		= maps[map][opcode];
	uint32	flags	= op.flags;

	if ((flags & OPF_64) == (arch == X64 ? OPF_NOT64 : OPF_ONLY64))
		return p;

	//if (test_all(flags, OPF_NOT64 | OPF_ONLY64 | OPF_GROUP)) {
	//	op		= ((Opcode*)op.mnemonic)[arch == X64];
	//	flags	= op.flags;
	//}

	bool	need_modrm	= !!(flags & OPF_GROUP);
	for (uint32 f = flags & 0xffffff; f & 0xff; f >>= 8) {
		OPFLAGS	t = OPFLAGS(f & OPF_TYPE);
		if (t >= OPF_MODRM)
			need_modrm = true;
	}

	if (need_modrm) {
		ModRM	modrm	= (ModRM&)*p++;
		modrm_rm		= modrm.rm	| (prefix.test(PREFIX::B) << 3);
		modrm_reg		= modrm.reg	| (prefix.test(PREFIX::R) << 3);
		modrm_mod		= modrm.mod;

		if (arch == X16) {
			displacement = modrm_mod == 1 ? int64(*p++)
				: modrm_mod == 2 || (modrm_mod == 0 && modrm_rm == 6) ? get(*((packed<int16>*&)p)++)
				: 0;
		} else {
			int	r = modrm_rm;
			if (modrm.rm == 4 && modrm.mod != 3) {	// sib
				SIB	sib		= (SIB&)*p++;
				sib_base	= sib.base	| (prefix.test(PREFIX::B) << 3);
				sib_index	= sib.index	| (prefix.test(PREFIX::X) << 3);
				sib_scale	= sib.scale;
				r			= sib_base;
			}
			displacement = modrm_mod == 1 ? int64(*p++)
				: modrm_mod == 2 || (modrm_mod == 0 && r == 5) ? get(*((packed<int32>*&)p)++)
				: 0;
		}
	}

	if (flags & OPF_GROUP) {
		int	i = modrm_reg & 7;

		if (test_all(flags, OPF_64))
			i	= arch == X64;

		if (modrm_mod == ModRM::REGISTER) {
			if ((flags & OPF_TYPE) == OPF_nop)	// e.g. 0f 01 cx
				i += (modrm_rm + 1) * 8;

			else if (prefix.test(PREFIX::X87))	// separate part of table for x87 if mod==11
				i += 8;
		}

		op = ((Opcode*)op.mnemonic)[i];

		if (op.flags & OPF_GROUP)
			op = ((Opcode*)op.mnemonic)[modrm_rm];	//	2nd indirection (x87 only)

		if (op.flags & 0xff)
			flags = (flags & OPF_SIZE) * 0x0101;

		flags |= op.flags;
	}

	if (/*arch == X64 &&*/ test_all(flags, OPF_64))
		prefix.add(PREFIX::W);

	noregs = true;
	for (uint32 f = flags & 0xffffff, i = 0; f & 0xff; f >>= 8) {
		switch (OPFLAGS(f & OPF_TYPE)) {
			case OPF_I: case OPF_J: {
				SIZE	size = GetSize(f);
				immediate[i++]
					= size == SIZE_1 ? int64(int8(*p++))
					: size == SIZE_2 ? int64(*((packed<int16>*&)p)++)
					: size == SIZE_4 || opcode < 0xb8 || opcode >= 0xc0 ? int64(*((packed<int32>*&)p)++)	// only mov can have 64 bit immediate
					: int64(*((packed<int64>*&)p)++);
			}
			case OPF_F:case OPF_K:case OPF_B:case OPF_H:case OPF_G:case OPF_S:case OPF_V:case OPF_P:
				noregs = false;
				break;
		}
	}

	if ((flags & OPF_TYPE) == OPF_J)
		info |= Disassembler::FLAG_JMP | ((flags & OPF_SIZE) == OPF_d ? 0 : Disassembler::FLAG_RELATIVE);

	if (map == MAP_PRIMARY) {
		switch (opcode) {
			case 0x9a: case 0xe8:										//call
			case 0xc2: case 0xc3: case 0xca: case 0xcb: case 0xcf:		//ret
				info |= Disassembler::FLAG_CALLRET;
				break;
			case 0xff:
				switch (modrm_reg & 7) {
					case 2: case 3: info |= Disassembler::FLAG_CALLRET;	//call
					case 4:	case 5: info |= Disassembler::FLAG_INDIRECT | Disassembler::FLAG_JMP | ((modrm_reg & 1) == 0 && modrm_mod == 0 && modrm_rm == 5 ? Disassembler::FLAG_RELATIVE : 0);	//jmp;
				}
				break;
		}
		if (prefix.test(PREFIX::Repne) || prefix.test(PREFIX::Repe))
			info |= Disassembler::FLAG_CONDITIONAL;
	}
	mnemonic	= op.mnemonic;
	args		= flags & 0xffffff;

	if (opcode == 0xc2 && (map == MAP_VEX || map == MAP_SECONDARY)) // vcmpCps and cmpCpS
		args |= *p++ << 24;	// condition code

	return p;
}

string_accum &IntelOp::Dump(string_accum &sa, Disassembler::SymbolFinder sym_finder, uint64 _nextaddress) {
	nextaddress = _nextaddress;
	if (mnemonic) {
		if (prefix.test(PREFIX::Lock))
			sa << "lock ";

		while (char c = *mnemonic++) {
			switch (c) {
				case 'W':
					sa << (arch == X16 ? (prefix.test(PREFIX::OperandSize) ? 'd' : 'w')
						: (arch == X64 && prefix.test(PREFIX::W)) ? 'q'
						: prefix.test(PREFIX::OperandSize) ? 'w' : 'd');
					break;
				case 'R':
					if (*mnemonic == '?') {
						++mnemonic;
						if (prefix.test(PREFIX::Repne))
							sa << "repne ";
						else if (prefix.test(PREFIX::Repe))
							sa << "repe ";
					} else if (prefix.test(PREFIX::Repne) || prefix.test(PREFIX::Repe))
						sa << "rep ";
					break;
				case 'P':
					sa << (prefix.test(PREFIX::AddressSize) ? 's' : 'p');
					break;
				case 'S':
					sa << (prefix.test(PREFIX::OperandSize) ? 'd' : 's');
					break;
				case 'D':
					sa << (prefix.test(PREFIX::W) ? 'q' : 'd');
					break;
				case 'C': {//vcmpCps, cmpCpS
					uint8	cc	= args >> 24;
					sa << (cc < num_elements(compares) ? compares[cc] : "?");
					break;
				}
				default:
					sa << c;
			}
		}

		int	i = 0;
		for (uint32 a = args; a & 0xff; a >>= 8)
			Argument(sa << ifelse(i++, ", ", ' '), sym_finder, a);

	} else {
		sa << "!!!";
	}
	return sa;
}

class DisassemblerIntel : public Disassembler {
public:
	static Disassembler::InstructionInfo GetInstructionInfo(const iso::memory_block &block, IntelOp::ARCH arch) {
		IntelOp		op(arch);
		int			len	= op.Parse(block) - block;
		Disassembler::InstructionInfo	info(len);
		info.flags	= op.info;
		info.dest	= op.GetDest();
		return info;
	}
	static void DisassembleLine(string_accum &a, const void *data, uint64 addr, IntelOp::ARCH arch, SymbolFinder sym_finder) {
		IntelOp	dis(arch);
		auto	p = (const uint8*)data;
		int		n = dis.Parse(p) - p;
		dis.Dump(a, sym_finder, addr + n);
	}

};

class DisassemblerIntel16 : public DisassemblerIntel {
public:
	virtual	const char*	GetDescription()	{ return "Intel 16"; }
	virtual Disassembler::InstructionInfo	GetInstructionInfo(const iso::memory_block &block) {
		return DisassemblerIntel::GetInstructionInfo(block, IntelOp::X16);
	}
	virtual void		DisassembleLine(string_accum &a, const void *data, uint64 addr, SymbolFinder sym_finder) {
		return DisassemblerIntel::DisassembleLine(a, data, addr, IntelOp::X16, sym_finder);
	}
} intel16;

class DisassemblerIntel32 : public DisassemblerIntel {
public:
	virtual	const char*	GetDescription()	{ return "Intel 32"; }
	virtual Disassembler::InstructionInfo	GetInstructionInfo(const iso::memory_block &block) {
		return DisassemblerIntel::GetInstructionInfo(block, IntelOp::X32);
	}
	virtual void		DisassembleLine(string_accum &a, const void *data, uint64 addr, SymbolFinder sym_finder) {
		return DisassemblerIntel::DisassembleLine(a, data, addr, IntelOp::X32, sym_finder);
	}
} intel32;

class DisassemblerIntel64 : public DisassemblerIntel {
public:
	virtual	const char*	GetDescription()	{ return "Intel 64"; }
	virtual Disassembler::InstructionInfo	GetInstructionInfo(const iso::memory_block &block) {
		return DisassemblerIntel::GetInstructionInfo(block, IntelOp::X64);
	}
	virtual void		DisassembleLine(string_accum &a, const void *data, uint64 addr, SymbolFinder sym_finder) {
		return DisassemblerIntel::DisassembleLine(a, data, addr, IntelOp::X64, sym_finder);
	}
} intel64;

void intel_dis_dummy() {}

namespace {
struct tester {
	tester() {
		uint8	bytes[] = {
			0xdb, 0x28,
			0xdf, 0x20,
			0xd8, 0x00,	//fadd        dword ptr [rax]
			0xdc, 0x00,	//fadd        qword ptr [rax]
			0xd8, 0xc2,	//fadd        st,st(3)
			0xdc, 0xc3,	//fadd        st(3),st
			0xde, 0xc3,	//faddp       st(0),st
			0xde, 0xc1,	//faddp       st(1),st
			0xda, 0x00,	//fiadd       dword ptr [rax]
			0xde, 0x00,	//fiadd       word ptr [rax]
		};
		DisassemblerIntel::DisassembleLine(trace_accum() << '\n', bytes, 0, IntelOp::X64, none);
	}
};// _tester;
}

