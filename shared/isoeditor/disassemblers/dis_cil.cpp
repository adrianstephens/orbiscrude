#include "disassembler.h"
#include "filetypes/code/cil.h"
#include "filetypes/code/clr.h"

using namespace iso;

static const char *ops[] = {
	"nop",				// 0x00
	"break",			// 0x01
	"ldarg.0",			// 0x02
	"ldarg.1",			// 0x03
	"ldarg.2",			// 0x04
	"ldarg.3",			// 0x05
	"ldloc.0",			// 0x06
	"ldloc.1",			// 0x07
	"ldloc.2",			// 0x08
	"ldloc.3",			// 0x09
	"stloc.0",			// 0x0A
	"stloc.1",			// 0x0B
	"stloc.2",			// 0x0C
	"stloc.3",			// 0x0D
	"ldarg.s",			// 0x0E
	"ldarga.s",			// 0x0F
	"starg.s",			// 0x10
	"ldloc.s",			// 0x11
	"ldloca.s",			// 0x12
	"stloc.s",			// 0x13
	"ldnull",			// 0x14
	"ldc.i4.m1",		// 0x15
	"ldc.i4.0",			// 0x16
	"ldc.i4.1",			// 0x17
	"ldc.i4.2",			// 0x18
	"ldc.i4.3",			// 0x19
	"ldc.i4.4",			// 0x1A
	"ldc.i4.5",			// 0x1B
	"ldc.i4.6",			// 0x1C
	"ldc.i4.7",			// 0x1D
	"ldc.i4.8",			// 0x1E
	"ldc.i4.s",			// 0x1F
	"ldc.i4",			// 0x20
	"ldc.i8",			// 0x21
	"ldc.r4",			// 0x22
	"ldc.r8",			// 0x23
	"unused_24",
	"dup",				// 0x25
	"pop",				// 0x26
	"jmp",				// 0x27
	"call",				// 0x28
	"calli",			// 0x29
	"ret",				// 0x2A
	"br.s",				// 0x2B
	"brfalse.s",		// 0x2C
	"brtrue.s",			// 0x2D
	"beq.s",			// 0x2E
	"bge.s",			// 0x2F
	"bgt.s",			// 0x30
	"ble.s",			// 0x31
	"blt.s",			// 0x32
	"bne.un.s",			// 0x33
	"bge.un.s",			// 0x34
	"bgt.un.s",			// 0x35
	"ble.un.s",			// 0x36
	"blt.un.s",			// 0x37
	"br",				// 0x38
	"brfalse",			// 0x39
	"brtrue",			// 0x3A
	"beq",				// 0x3B
	"bge",				// 0x3C
	"bgt",				// 0x3D
	"ble",				// 0x3E
	"blt",				// 0x3F
	"bne.un",			// 0x40
	"bge.un",			// 0x41
	"bgt.un",			// 0x42
	"ble.un",			// 0x43
	"blt.un",			// 0x44
	"switch",			// 0x45
	"ldind.i1",			// 0x46
	"ldind.u1",			// 0x47
	"ldind.i2",			// 0x48
	"ldind.u2",			// 0x49
	"ldind.i4",			// 0x4A
	"ldind.u4",			// 0x4B
	"ldind.i8",			// 0x4C
	"ldind.i",			// 0x4D
	"ldind.r4",			// 0x4E
	"ldind.r8",			// 0x4F
	"ldind.ref",		// 0x50
	"stind.ref",		// 0x51
	"stind.i1",			// 0x52
	"stind.i2",			// 0x53
	"stind.i4",			// 0x54
	"stind.i8",			// 0x55
	"stind.r4",			// 0x56
	"stind.r8",			// 0x57
	"add",				// 0x58
	"sub",				// 0x59
	"mul",				// 0x5A
	"div",				// 0x5B
	"div.un",			// 0x5C
	"rem",				// 0x5D
	"rem.un",			// 0x5E
	"and",				// 0x5F
	"or",				// 0x60
	"xor",				// 0x61
	"shl",				// 0x62
	"shr",				// 0x63
	"shr.un",			// 0x64
	"neg",				// 0x65
	"not",				// 0x66
	"conv.i1",			// 0x67
	"conv.i2",			// 0x68
	"conv.i4",			// 0x69
	"conv.i8",			// 0x6A
	"conv.r4",			// 0x6B
	"conv.r8",			// 0x6C
	"conv.u4",			// 0x6D
	"conv.u8",			// 0x6E
	"callvirt",			// 0x6F
	"cpobj",			// 0x70
	"ldobj",			// 0x71
	"ldstr",			// 0x72
	"newobj",			// 0x73
	"castclass",		// 0x74
	"isinst",			// 0x75
	"conv.r.un",		// 0x76
	"unused_77",
	"unused_78",
	"unbox",			// 0x79
	"throw",			// 0x7A
	"ldfld",			// 0x7B
	"ldflda",			// 0x7C
	"stfld",			// 0x7D
	"ldsfld",			// 0x7E
	"ldsflda",			// 0x7F
	"stsfld",			// 0x80
	"stobj",			// 0x81
	"conv.ovf.i1.un",	// 0x82
	"conv.ovf.i2.un",	// 0x83
	"conv.ovf.i4.un",	// 0x84
	"conv.ovf.i8.un",	// 0x85
	"conv.ovf.u1.un",	// 0x86
	"conv.ovf.u2.un",	// 0x87
	"conv.ovf.u4.un",	// 0x88
	"conv.ovf.u8.un",	// 0x89
	"conv.ovf.i.un",	// 0x8A
	"conv.ovf.u.un",	// 0x8B
	"box",				// 0x8C
	"newarr",			// 0x8D
	"ldlen",			// 0x8E
	"ldelema",			// 0x8F
	"ldelem.i1",		// 0x90
	"ldelem.u1",		// 0x91
	"ldelem.i2",		// 0x92
	"ldelem.u2",		// 0x93
	"ldelem.i4",		// 0x94
	"ldelem.u4",		// 0x95
	"ldelem.i8",		// 0x96
	"ldelem.i",			// 0x97
	"ldelem.r4",		// 0x98
	"ldelem.r8",		// 0x99
	"ldelem.ref",		// 0x9A
	"stelem.i",			// 0x9B
	"stelem.i1",		// 0x9C
	"stelem.i2",		// 0x9D
	"stelem.i4",		// 0x9E
	"stelem.i8",		// 0x9F
	"stelem.r4",		// 0xA0
	"stelem.r8",		// 0xA1
	"stelem.ref",		// 0xA2
	"ldelem",			// 0xA3
	"stelem",			// 0xA4
	"unbox.any",		// 0xA5
	"unused_A6",
	"unused_A7",
	"unused_A8",
	"unused_A9",
	"unused_AA",
	"unused_AB",
	"unused_AC",
	"unused_AD",
	"unused_AE",
	"unused_AF",
	"unused_B0",
	"unused_B1",
	"unused_B2",
	"conv.ovf.i1",		// 0xB3
	"conv.ovf.u1",		// 0xB4
	"conv.ovf.i2",		// 0xB5
	"conv.ovf.u2",		// 0xB6
	"conv.ovf.i4",		// 0xB7
	"conv.ovf.u4",		// 0xB8
	"conv.ovf.i8",		// 0xB9
	"conv.ovf.u8",		// 0xBA
	"unused_BB",
	"unused_BC",
	"unused_BD",
	"unused_BE",
	"unused_BF",
	"unused_C0",
	"unused_C1",
	"refanyval",		// 0xC2
	"ckfinite",			// 0xC3
	"unused_C4",
	"unused_C5",
	"mkrefany",			// 0xC6
	"unused_C7",
	"unused_C8",
	"unused_C9",
	"unused_CA",
	"unused_CB",
	"unused_CC",
	"unused_CD",
	"unused_CE",
	"unused_CF",
	"ldtoken",			// 0xD0
	"conv.u2",			// 0xD1
	"conv.u1",			// 0xD2
	"conv.i",			// 0xD3
	"conv.ovf.i",		// 0xD4
	"conv.ovf.u",		// 0xD5
	"add.ovf",			// 0xD6
	"add.ovf.un",		// 0xD7
	"mul.ovf",			// 0xD8
	"mul.ovf.un",		// 0xD9
	"sub.ovf",			// 0xDA
	"sub.ovf.un",		// 0xDB
	"endfinally",		// 0xDC
	"leave",			// 0xDD
	"leave.s",			// 0xDE
	"stind.i",			// 0xDF
	"conv.u",			// 0xE0
};

static const char *ops_FE[] = {
	"arglist",			// 0x00
	"ceq",				// 0x01
	"cgt",				// 0x02
	"cgt.un",			// 0x03
	"clt",				// 0x04
	"clt.un",			// 0x05
	"ldftn",			// 0x06
	"ldvirtftn",		// 0x07
	"unused_FE08",		// 0x08
	"ldarg",			// 0x09
	"ldarga",			// 0x0A
	"starg",			// 0x0B
	"ldloc",			// 0x0C
	"ldloca",			// 0x0D
	"stloc",			// 0x0E
	"localloc",			// 0x0F
	"unused_FE10",		// 0x10
	"endfilter",		// 0x11
	"unaligned.",		// 0x12
	"volatile.",		// 0x13
	"tail.",			// 0x14
	"initobj",			// 0x15
	"constrained.",		// 0x16
	"cpblk",			// 0x17
	"initblk",			// 0x18
	"no.",				// 0x19
	"rethrow",			// 0x1A
	"unused_FE1B",		// 0x1B
	"sizeof",			// 0x1C
	"Refanytype",		// 0x1D
	"readonly.",		// 0x1E
};

class DisassemblerCIL : public Disassembler {
public:
	virtual	const char*	GetDescription()	{ return "CIL"; }
	virtual State*		Disassemble(const iso::memory_block &block, uint64 addr, SymbolFinder sym_finder);
} dis_cil;

Disassembler::State *DisassemblerCIL::Disassemble(const memory_block &block, uint64 addr, SymbolFinder sym_finder) {
	StateDefault		*state	= new StateDefault;
	const_memory_block	code	= ((const ILMETHOD*)block)->GetCode();

	addr = (const uint8*)block - code;

	for (const uint8		*p = code; p < code.end();) {
		uint32	offset		= uint32(addr) + (p - (uint8*)block);

		uint64			sym_addr;
		string_param	sym_name;
		if (sym_finder && sym_finder(offset, sym_addr, sym_name) && sym_addr == offset)
			state->lines.push_back(sym_name);

		int oplen	= cil::oplen(p);
		buffer_accum<1024>	ba("%08x ", offset);
		for (int i = 0; i < min(oplen, 6); i++)
			ba.format("%02x ", p[i]);
		for (int i = oplen; i < 6; i++)
			ba << "   ";

		cil::FLAGS		flags	= 0;
		const uint8		*pp		= p;
		cil::OP			op		= (cil::OP)*pp++;
		if (op == cil::PREFIX1) {
			cil::OP_FE	op = (cil::OP_FE)*pp++;
			if (op < num_elements(ops_FE)) {
				flags	= cil::get_flags(op);
				ba		<< ops_FE[op];
			} else {
				ba.format("unused_FE%0X", op);
			}
		} else if (op < num_elements(ops)) {
			flags	= cil::get_flags(op);
			ba		<< ops[op];
		} else {
			ba.format("unused_%0X", op);
		}

		if (flags.params())
			ba << "  ";

		switch (flags.params()) {
			case cil::TARGET_8:		ba << "0x" << hex(offset + oplen + *(int8*)pp); break;
			case cil::TARGET_32:	ba << "0x" << hex(offset + oplen + *(packed<int32>*)pp); break;
			case cil::VAR_8:		ba << "0x" << hex(*(uint8*)pp); break;
			case cil::VAR_16:		ba << "0x" << hex(*(packed<uint16>*)pp); break;
			case cil::INLINE_S8:	ba << *(int8*)pp; break;
			case cil::INLINE_S32:	ba << *(packed<int32>*)pp; break;
			case cil::INLINE_S64:	ba << *(packed<int64>*)pp; break;
			case cil::INLINE_F32:	ba << *(packed<float>*)pp; break;
			case cil::INLINE_F64:	ba << *(packed<float64>*)pp; break;

			case cil::TOKEN:
//			case cil::STRING:
//			case cil::TYPE:
//			case cil::METHOD:
//			case cil::FIELD:
//			case cil::SIGNATURE:
			{
				clr::Token	tok = (clr::Token)*(packed<uint32>*)pp;
				ba << tok.type() << '[' << tok.index() << ']';
				//ba << "0x" << hex(*(packed<uint32>*)pp);
				break;
			}

			case cil::SWITCH: {
				uint32 n = *(packed<uint32le>*)pp;
				offset += n * 4 + 5;

				for (uint32 i = 0; i < n; i++) {
					if (i)
						ba << ", ";
					ba << "0x" << hex(offset + ((packed<int32le>*)pp)[i + 1]);
				}
				break;
			}
		}

		p += oplen;
		state->lines.push_back(ba);
	}
	return state;
}
