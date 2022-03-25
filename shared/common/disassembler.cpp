#include "disassembler.h"

using namespace iso;


struct State2 : Disassembler::State {
	malloc_block				mem;
	uint64						base;
	dynamic_array<uint32>		offsets;
	Disassembler				*dis;
	int							addr_bits, code_units;

	State2(const memory_block &block, uint64 addr, Disassembler *dis) : mem(block), base(addr), dis(dis), addr_bits(64), code_units(1) {}

	virtual	int		Count()				{ return offsets.size32(); }
	virtual	uint64	GetAddress(int i)	{ return base + (i ? offsets[min(i, offsets.size32()) - 1] : 0); }
	virtual	void	GetLine(string_accum &a, int i, int flags, Disassembler::SymbolFinder sym_finder) {
		if (i < offsets.size32()) {
			uint32			offset		= i == 0 ? 0 : offsets[i - 1];
			uint64			addr		= base + offset;
			const uint8		*p			= mem + offset;

			uint64			sym_addr;
			string_param	sym_name;
			if (sym_finder && sym_finder(addr, sym_addr, sym_name) && sym_addr == addr)
				a << leftjustify(addr_bits / 4 + 3) << sym_name;
			else if (flags & Disassembler::SHOW_ADDRESS)
				a.format("%0*I64x   ", addr_bits / 4, addr);

			if (flags & Disassembler::SHOW_BYTES) {
				int		n = min(offsets[i] - offset, 8);
				for (int i = 0; i < n; i++)
					a.format(" %02x", p[i]);
				a.putc(' ', (8 - n) * 3 + 1);
			}

			dis->DisassembleLine(a, p, addr, flags & Disassembler::SHOW_SYMBOLS ? sym_finder : none);
		}
	}
};

Disassembler::State* Disassembler::Disassemble(const memory_block &block, uint64 addr, Disassembler::SymbolFinder sym_finder) {
	State2		*state = new State2(block, addr, this);
	uint32		offset = 0;
	uint32		length = block.size32();

	while (offset < length) {
		auto	info	= GetInstructionInfo(block + offset);
		offset = info.offset += offset;
		state->offsets.push_back(offset);
	}
	return state;
}
