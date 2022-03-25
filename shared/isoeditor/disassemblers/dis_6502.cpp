#include "disassembler.h"

using namespace iso;

enum MODE {
	MODE_SIZE1,
	NONE	= MODE_SIZE1,
	ACC,

	MODE_SIZE2,
	IMM		= MODE_SIZE2,
	ZP,
	ZP_X,
	ZP_Y,
	IND_X,
	IND_Y,
	REL,

	MODE_SIZE3,
	ABS		= MODE_SIZE3,
	ABS_X,
	ABS_Y,
	IND,
};

static const char *mode_strings[] = {
	"",
	" A",
	" #&%02x",
	" &%02x",
	" &%02x,X",
	" &%02x,Y",
	" (&%02x,X)",
	" (&%02x),Y",
	" &%04x",
	" &%04x",
	" &%04x,X",
	" &%04x,Y",
	" (&%04x)",
};

enum MNEMONIC {
	ILLEGAL,
	ADC, AND, ASL, BCC, BCS, BEQ, BIT, BMI,
	BNE, BPL, BRK, BVC, BVS, CLC, CLD, CLI,
	CLV, CMP, CPX, CPY, DEC, DEX, DEY, EOR,
	INC, INX, INY, JMP, JSR, LDA, LDX, LDY,
	LSR, NOP, ORA, PHA, PHP, PLA, PLP, ROL,
	ROR, RTI, RTS, SBC, SEC, SED, SEI, STA,
	STX, STY, TAX, TAY, TSX, TXA, TXS, TYA,
};

static const char mnemonics[] =
	"adc" "and" "asl" "bcc" "bcs" "beq" "bit" "bmi"
	"bne" "bpl" "brk" "bvc" "bvs" "clc" "cld" "cli"
	"clv" "cmp" "cpx" "cpy" "dec" "dex" "dey" "eor"
	"inc" "inx" "iny" "jmp" "jsr" "lda" "ldx" "ldy"
	"lsr" "nop" "ora" "pha" "php" "pla" "plp" "rol"
	"ror" "rti" "rts" "sbc" "sec" "sed" "sei" "sta"
	"stx" "sty" "tax" "tay" "tsx" "txa" "txs" "tya"
;

static struct op6502 {
	uint8	mnemonic, mode;
} ops[] = {
	{BRK,			},	{ORA,	IND_X	},	{0,		0		},	{0, 0},	{0,		0		},	{ORA,	ZP		},	{ASL,	ZP		},	{0, 0},
	{PHP,			},	{ORA,	IMM		},	{ASL,	ACC		},	{0, 0},	{0,		0		},	{ORA,	ABS		},	{ASL,	ABS		},	{0, 0},
	{BPL,	REL		},	{ORA,	IND_Y	},	{0,		0		},	{0, 0},	{0,		0		},	{ORA,	ZP_X	},	{ASL,	ZP_X	},	{0, 0},
	{CLC,			},	{ORA,	ABS_Y	},	{0,		0		},	{0, 0},	{0,		0		},	{ORA,	ABS_X	},	{ASL,	ABS_X	},	{0, 0},
	{JSR,	ABS		},	{AND,	IND_X	},	{0,		0		},	{0, 0},	{BIT,	ZP		},	{AND,	ZP		},	{ROL,	ZP		},	{0, 0},
	{PLP,			},	{AND,	IMM		},	{ROL,	ACC		},	{0, 0},	{BIT,	ABS		},	{AND,	ABS		},	{ROL,	ABS		},	{0, 0},
	{BMI,	REL		},	{AND,	IND_Y	},	{0,		0		},	{0, 0},	{0,		0		},	{AND,	ZP_X	},	{ROL,	ZP_X	},	{0, 0},
	{SEC,			},	{AND,	ABS_Y	},	{0,		0		},	{0, 0},	{0,		0		},	{AND,	ABS_X	},	{ROL,	ABS_X	},	{0, 0},
	{RTI,			},	{EOR,	IND_X	},	{0,		0		},	{0, 0},	{0,		0		},	{EOR,	ZP		},	{LSR,	ZP		},	{0, 0},
	{PHA,			},	{EOR,	IMM		},	{LSR,	ACC		},	{0, 0},	{JMP,	ABS		},	{EOR,	ABS		},	{LSR,	ABS		},	{0, 0},
	{BVC,	REL		},	{EOR,	IND_Y	},	{0,		0		},	{0, 0},	{0,		0		},	{EOR,	ZP_X	},	{LSR,	ZP_X	},	{0, 0},
	{CLI,			},	{EOR,	ABS_Y	},	{0,		0		},	{0, 0},	{0,		0		},	{EOR,	ABS_X	},	{LSR,	ABS_X	},	{0, 0},
	{RTS,			},	{ADC,	IND_X	},	{0,		0		},	{0, 0},	{0,		0		},	{ADC,	ZP		},	{ROR,	ZP		},	{0, 0},
	{PLA,			},	{ADC,	IMM		},	{ROR,	ACC		},	{0, 0},	{JMP,	IND		},	{ADC,	ABS		},	{ROR,	ABS		},	{0, 0},
	{BVS,	REL		},	{ADC,	IND_Y	},	{0,		0		},	{0, 0},	{0,		0		},	{ADC,	ZP_X	},	{ROR,	ZP_X	},	{0, 0},
	{SEI,			},	{ADC,	ABS_Y	},	{0,		0		},	{0, 0},	{0,		0		},	{ADC,	ABS_X	},	{ROR,	ABS_X	},	{0, 0},
	{0,		0		},	{STA,	IND_X	},	{0,		0		},	{0, 0},	{STY,	ZP		},	{STA,	ZP		},	{STX,	ZP		},	{0, 0},
	{DEY,			},	{0,		0		},	{TXA,			},	{0, 0},	{STY,	ABS		},	{STA,	ABS		},	{STX,	ABS		},	{0, 0},
	{BCC,	REL		},	{STA,	IND_Y	},	{0,		0		},	{0, 0},	{STY,	ZP_X	},	{STA,	ZP_X	},	{STX,	ZP_Y	},	{0, 0},
	{TYA,			},	{STA,	ABS_Y	},	{TXS,			},	{0, 0},	{0,		0		},	{STA,	ABS_X	},	{0,		0		},	{0, 0},
	{LDY,	IMM		},	{LDA,	IND_X	},	{LDX,	IMM		},	{0, 0},	{LDY,	ZP		},	{LDA,	ZP		},	{LDX,	ZP		},	{0, 0},
	{TAY,			},	{LDA,	IMM		},	{TAX,			},	{0, 0},	{LDY,	ABS		},	{LDA,	ABS		},	{LDX,	ABS		},	{0, 0},
	{BCS,	REL		},	{LDA,	IND_Y	},	{0,		0		},	{0, 0},	{LDY,	ZP_X	},	{LDA,	ZP_X	},	{LDX,	ZP_Y	},	{0, 0},
	{CLV,			},	{LDA,	ABS_Y	},	{TSX,			},	{0, 0},	{LDY,	ABS_X	},	{LDA,	ABS_X	},	{LDX,	ABS_Y	},	{0, 0},
	{CPY,	IMM		},	{CMP,	IND_X	},	{0,		0		},	{0, 0},	{CPY,	ZP		},	{CMP,	ZP		},	{DEC,	ZP		},	{0, 0},
	{INY,			},	{CMP,	IMM		},	{DEX,			},	{0, 0},	{CPY,	ABS		},	{CMP,	ABS		},	{DEC,	ABS		},	{0, 0},
	{BNE,	REL		},	{CMP,	IND_Y	},	{0,		0		},	{0, 0},	{0,		0		},	{CMP,	ZP_X	},	{DEC,	ZP_X	},	{0, 0},
	{CLD,			},	{CMP,	ABS_Y	},	{0,		0		},	{0, 0},	{0,		0		},	{CMP,	ABS_X	},	{DEC,	ABS_X	},	{0, 0},
	{CPX,	IMM		},	{SBC,	IND_X	},	{0,		0		},	{0, 0},	{CPX,	ZP		},	{SBC,	ZP		},	{INC,	ZP		},	{0, 0},
	{INX,			},	{SBC,	IMM		},	{NOP,			},	{0, 0},	{CPX,	ABS		},	{SBC,	ABS		},	{INC,	ABS		},	{0, 0},
	{BEQ,	REL		},	{SBC,	IND_Y	},	{0,		0		},	{0, 0},	{0,		0		},	{SBC,	ZP_X	},	{INC,	ZP_X	},	{0, 0},
	{SED,			},	{SBC,	ABS_Y	},	{0,		0		},	{0, 0},	{0,		0		},	{SBC,	ABS_X	},	{INC,	ABS_X	},	{0, 0},
};

class Disassembler6502 : public Disassembler {
public:
	virtual	const char*	GetDescription()	{ return "6502"; }
	virtual State*		Disassemble(const memory_block &block, uint64 addr, SymbolFinder sym_finder);
} dis6502;

Disassembler::State *Disassembler6502::Disassemble(const memory_block &block, uint64 addr, SymbolFinder sym_finder) {
	StateDefault	*state = new StateDefault;

	for (uint8 *p = block, *end = (uint8*)block.end(); p < end; ) {
		uint16	offset		= uint16(p - (uint8*)block + addr);

		uint64			sym_addr;
		string_param	sym_name;
		if (sym_finder && sym_finder(offset, sym_addr, sym_name) && sym_addr == offset)
			state->lines.push_back(sym_name);

		op6502	&o	= ops[*p];
		int		nb	= o.mode < MODE_SIZE2 ? 1 : o.mode < MODE_SIZE3 ? 2 : 3;
		uint16	val	= nb == 1 ? 0 : p[1];

		if (nb == 3)
			val += p[2] << 8;
		else if (o.mode == REL)
			val = offset + 2 + int8(val);

		buffer_accum<1024>	ba("%04x ", offset);
		for (int i = 0; i < nb; i++)
			ba.format("%02x ", p[i]);
		ba.putc(' ', (4 - nb) * 3);

		if (o.mnemonic == 0) {
			ba << "unknown opcode";

		} else {
			ba << str(mnemonics + (o.mnemonic - 1) * 3, 3);
			ba.format(mode_strings[o.mode], val);
		}

		p += nb;
		state->lines.push_back((const char*)ba);
	}
	return state;
}
