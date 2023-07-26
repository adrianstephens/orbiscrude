#include "disassembler.h"

using namespace iso;

static const char * const MIPS_REGISTER_NAMES[32] = {
	"zero",    // Hardware constant 0
	"at",      // Reserved for assembler
	"v0",      // Return values
	"v1",
	"a0",      // Arguments
	"a1",
	"a2",
	"a3",
	"t0",      // Temporaries
	"t1",
	"t2",
	"t3",
	"t4",
	"t5",
	"t6",
	"t7",
	"s0",      // Saved values
	"s1",
	"s2",
	"s3",
	"s4",
	"s5",
	"s6",
	"s7",
	"t8",      // Cont. Saved values
	"t9",
	"k0",      // Reserved for OS
	"k1",
	"gp",      // Global pointer
	"sp",      // Stack Pointer
	"fp",      // Frame Pointer
	"ra"       // Return Adress
};

static const char * const MIPS_REGISTER_RT_INSTRUCTION_NAMES[4][8] = {
	{"bltz", "bgez"},
	{"tgei", "tgeiu", "tlti", "tltiu", "teqi", NULL, "tnei"},
	{"bltzal", "bgezal"}
};

static const char * const MIPS_REGISTER_C_INSTRUCTION_NAMES[8][8] = {
	{"madd", "maddu", "mul", NULL, "msub", "msubu"},
	{},
	{},
	{},
	{"clz", "clo"}
};

static const char * const MIPS_REGISTER_INSTRUCTION_NAMES[8][8] = {
	{"sll", NULL, "srl", "sra", "sllv", NULL, "srlv", "srav"},
	{"jr", "jalr"},
	{"mfhi", "mthi", "mflo", "mtlo"},
	{"mult", "multu", "div", "divu"},
	{"add", "addu", "sub", "subu", "and", "or", "xor", "nor"},
	{NULL, NULL, "slt", "sltu"}
};

static const char * const MIPS_ROOT_INSTRUCTION_NAMES[8][8] = {
	{NULL, NULL, "j", "jal", "beq", "bne", "blez", "bgtz"},
	{"addi", "addiu", "slti", "sltiu", "andi", "ori", "xori", "lui"},
	{},
	{"llo", "lhi", "trap"},
	{"lb", "lh", "lwl", "lw", "lbu", "lhu", "lwr"},
	{"sb", "sh", "swl", "sw", NULL, NULL, "swr"},
	{"ll"},
	{"sc"}
};

enum {
	MIPS_REG_C_TYPE_MULTIPLY		= 0,
	MIPS_REG_C_TYPE_COUNT			= 4,

	MIPS_REG_TYPE_SHIFT_OR_SHIFTV	= 0,
	MIPS_REG_TYPE_JUMPR				= 1,
	MIPS_REG_TYPE_MOVE				= 2,
	MIPS_REG_TYPE_DIVMULT			= 3,
	MIPS_REG_TYPE_ARITHLOG_GTE		= 4,

	MIPS_ROOT_TYPE_JUMP_OR_BRANCH	= 0,
	MIPS_ROOT_TYPE_ARITHLOGI		= 1,
	MIPS_ROOT_TYPE_LOADI_OR_TRAP	= 3,
	MIPS_ROOT_TYPE_LOADSTORE_GTE	= 4,
};

class DisassemblerMIPS : public Disassembler {
public:
	virtual	const char*	GetDescription() { return "MIPS"; }
	virtual State*		Disassemble(const iso::memory_block &block, uint64 addr, SymbolFinder sym_finder);
} mips;

Disassembler::State *DisassemblerMIPS::Disassemble(const iso::memory_block &block, uint64 addr, SymbolFinder sym_finder) {
	StateDefault	*state = new StateDefault;

	for (uint32be *p = block; p < block.end(); p++) {
		uint32	offset	= (uint8*)p - (uint8*)block;
		uint32	number	= *p;

		uint64			sym_addr;
		string_param	sym_name;
		if (sym_finder && sym_finder(offset, sym_addr, sym_name) && sym_addr == offset)
			state->lines.push_back(sym_name);

		buffer_accum<1024>	ba("%08x    %08x    ", offset, number);

		uint8	op			= number >> 26;
		uint8	op_upper	= (op >> 3) & bits(3);
		uint8	op_lower	= op & bits(3);

		uint8	rs			= (number >> 21) & bits(5);
		uint8	rt			= (number >> 16) & bits(5);
		uint8	rd			= (number >> 11) & bits(5);
		uint8	sa			= (number >> 6) & bits(5);

		uint8	funct		= number & bits(6);
		uint8	funct_upper	= (funct >> 3) & bits(3);
		uint8	funct_lower	= funct & bits(3);

		int16	imm			= number & bits(16);

		const char *name = 0;
		switch (op) {
			case 0:		name = MIPS_REGISTER_INSTRUCTION_NAMES[funct_upper][funct_lower];
			case 1:		name = MIPS_REGISTER_RT_INSTRUCTION_NAMES[rt >> 3][rt & bits(3)];
			case 0x1c:	name = MIPS_REGISTER_C_INSTRUCTION_NAMES[funct_upper][funct_lower];
			default:	name = MIPS_ROOT_INSTRUCTION_NAMES[op_upper][op_lower];
		}

		if (number == 0) {
			ba << "nop";

		} else if (!name) {
			ba << "????";

		} else {
			ba << name << ' ';

			switch (op) {
				case 0:
					switch (funct_upper) {
						case MIPS_REG_TYPE_SHIFT_OR_SHIFTV:
							ba << MIPS_REGISTER_NAMES[rd] << ", " << MIPS_REGISTER_NAMES[rt] << ", ";
							if (funct_lower < 4) //Shift
								ba << sa;
							else //ShiftV
								ba << MIPS_REGISTER_NAMES[rs];
							break;

						case MIPS_REG_TYPE_JUMPR:
							if (funct_lower < 1)
								ba << MIPS_REGISTER_NAMES[rs];
							else
								ba << MIPS_REGISTER_NAMES[rd] << ", " << MIPS_REGISTER_NAMES[rs];
							break;

						case MIPS_REG_TYPE_MOVE:
							if (funct_lower % 2 == 0)
								ba << MIPS_REGISTER_NAMES[rd];
							else
								ba << MIPS_REGISTER_NAMES[rs];
							break;

						case MIPS_REG_TYPE_DIVMULT:
							ba << MIPS_REGISTER_NAMES[rs] << ", " << MIPS_REGISTER_NAMES[rt];
							break;

						case MIPS_REG_TYPE_ARITHLOG_GTE:
						case MIPS_REG_TYPE_ARITHLOG_GTE + 1:
							ba << MIPS_REGISTER_NAMES[rd] << ", " << MIPS_REGISTER_NAMES[rs] << ", " << MIPS_REGISTER_NAMES[rt];
							break;

						default://error
							ba << "????";
							break;
					}
					break;

				case 1:
					ba << MIPS_REGISTER_NAMES[rs] << ", " << imm;
					break;

				case 0x1c:
					switch (funct_upper) {
						case MIPS_REG_C_TYPE_MULTIPLY:
							if (funct_lower == 2)
								ba << MIPS_REGISTER_NAMES[rd] << ", ";
							ba << MIPS_REGISTER_NAMES[rs] << ", " << MIPS_REGISTER_NAMES[rt];
							break;

						case MIPS_REG_C_TYPE_COUNT:
							ba << MIPS_REGISTER_NAMES[rd] << ", " << MIPS_REGISTER_NAMES[rs];
							break;
					}
					break;

				default:
					switch (op_upper) {
						case MIPS_ROOT_TYPE_JUMP_OR_BRANCH: {
							int	target;
							if (op_lower < 4) { // Jump
								target	= mask_sign_extend(number, 26);
							} else {
								target	= offset + imm;
								if (op_lower < 6) { //Branch
									ba << MIPS_REGISTER_NAMES[rs] << ", " << MIPS_REGISTER_NAMES[rt] << ", ";
								} else { //BranchZ
									//dbg("imm", imm);
									ba << MIPS_REGISTER_NAMES[rs] << ", ";
								}
							}
							uint64			sym_addr;
							string_param	sym_name;
							if (sym_finder && sym_finder(target, sym_addr, sym_name) && sym_addr == target)
								ba << sym_addr;
							else
								ba.format("0x%08x", target);
							break;
						}

						case MIPS_ROOT_TYPE_ARITHLOGI:
							if (op_lower < 7)
								ba << MIPS_REGISTER_NAMES[rt] << ", " << MIPS_REGISTER_NAMES[rs] << ", " << imm;
							else
								ba << MIPS_REGISTER_NAMES[rt] << ", " << imm;
							break;

						case MIPS_ROOT_TYPE_LOADSTORE_GTE:
						case MIPS_ROOT_TYPE_LOADSTORE_GTE + 1:
						case MIPS_ROOT_TYPE_LOADSTORE_GTE + 2:
						case MIPS_ROOT_TYPE_LOADSTORE_GTE + 3:
							ba << MIPS_REGISTER_NAMES[rt] << ", " << imm << '(' << MIPS_REGISTER_NAMES[rs] << ')';
							break;

						default://error
							ba << "????";
							break;
					}
					break;
			}
		}

		state->lines.push_back(ba);
	}
	return state;
}
