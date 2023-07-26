#include "view_dxbc.h"
#include "dx\spdb.h"
#include "dx\sim_dxbc.h"

using namespace app;

namespace app {
const C_type *to_c_type(const PDB_types &pdb, TI ti);
}

struct hlsl_type_getter {
	PDB_types		&pdb;

	const CV::HLSL *procTI(TI ti) {
		if (ti < pdb.MinTI())
			return 0;
		return process<const CV::HLSL*>(pdb.GetType(ti), *this);
	}
	const CV::HLSL *operator()(const CV::Leaf&, bool = false) {
		return 0;
	}
	const CV::HLSL *operator()(const CV::HLSL &t) {
		return &t;
	}
	const CV::HLSL *operator()(const CV::Alias &t) {
		return procTI(t.utype);
	}
	const CV::HLSL *operator()(const CV::Modifier &t) {
		return procTI(t.type);
	}
	const CV::HLSL *operator()(const CV::ModifierEx &t) {
		return procTI(t.type);
	}
	hlsl_type_getter(PDB_types &pdb) : pdb(pdb) {}
};

const CV::HLSL *get_hlsl_type(PDB_types &pdb, TI ti) {
	return hlsl_type_getter(pdb).procTI(ti);
}

//-----------------------------------------------------------------------------
//	DXBCRegisterWindow
//-----------------------------------------------------------------------------

uint32	DXBCRegisterWindow::AddReg(dx::Operand::Type type, int index, uint32 offset) {
	regspecs.push_back() = {type, index};
	for (int j = 0; j < 4; j++, offset += 4)
		entries.emplace_back(offset, RegisterName(type, index, 1 << j), float_field);
	return offset;
}

uint32	DXBCRegisterWindow::AddInputRegs(const dx::Decls *decl) {
	uint32	offset = 0;

	entries.emplace_back(0, "pc", nullptr, Entry::SIZE64);
	offset += 8;

	for (uint64 m = decl->InputMask(); m; m = clear_lowest(m)) {
		int		i		= lowest_set_index(m);
		auto	type	= dx::Decls::input_type(i, decl->stage);
		offset = AddReg(type, i, offset);
	}

	for (int i = 0; i < decl->NumInputControlPoints(); i++)
		offset = AddReg(dx::Operand::TYPE_INPUT_CONTROL_POINT, i, offset);

	for (uint64 m = decl->OutputMask(); m; m = clear_lowest(m)) {
		int		i		= lowest_set_index(m);
		auto	type	= dx::Decls::output_type(i);
		if (type != dx::Operand::TYPE_OUTPUT)
			i = 0;
		offset = AddReg(decl->output_type(i), i, offset);
	}

	for (uint64 m = decl->PatchConstOutputMask(); m; m = clear_lowest(m))
		offset = AddReg(dx::Operand::TYPE_INPUT_PATCH_CONSTANT, lowest_set_index(m), offset);

	for (int i = 0; i < decl->NumOutputControlPoints(); i++)
		offset = AddReg(dx::Operand::TYPE_OUTPUT_CONTROL_POINT, i, offset);

	return offset;
}

void DXBCRegisterWindow::Update(uint32 relpc, uint64 pc, int thread) {
	if (!entries) {
		uint32	offset = AddInputRegs(sim);

		for (int i = 0; i < sim->NumTemps(); i++)
			offset = AddReg(dx::Operand::TYPE_TEMP, i, offset);

		prev_regs.create(offset);
		SetPane(1, SendMessage(WM_ISO_NEWPANE));
	}

	struct RegUse {
		const CV::LOCALSYM	*parent;
		uint16		offset, size;
	};
	hash_map<const void*, RegUse>	uses;

	const CV::LOCALSYM	*parent = 0;
	if (auto *mod = spdb->HasModule(1)) {
		for (auto &s : mod->Symbols()) {
			switch (s.rectyp) {
				case CV::S_LOCAL:
					parent = s.as<CV::LOCALSYM>();
					break;
				case CV::S_DEFRANGE_HLSL: {
					auto	&r = *s.as<CV::DEFRANGESYMHLSL>();
					if (r.range().test({relpc, 1})) {
						const void	*off2	= sim->reg_data(thread, (dx::Operand::Type)r.regType, r.offsets());
						uses[off2]		= RegUse {parent, r.offsetParent, r.sizeInParent};
					}
				}
			}
		}
	}

	Entry	*y		= entries;
	uint32	*pregs	= prev_regs;

	((uint64*)pregs)[0]	= pc;
	y		+= 1;
	pregs	+= 2;

	uint32	active = sim->ThreadFlags(thread) & sim->THREAD_ACTIVE ? 0 : Entry::DISABLED;

	for (auto &i : regspecs) {
		uint32			offset	= i.index * 16;
		const uint32*	regs	= (const uint32*)sim->reg_data(thread, i.type, make_range_n(&offset, 1));

		for (int j = 0; j < 4; j++, y++) {
			buffer_accum<256>	acc(RegisterName(i.type, i.index, 1 << j));
			if (RegUse *use = uses.check(regs))
				DumpField(acc << '(' << get_name(use->parent), to_c_type(*spdb, use->parent->typind), use->offset, true) << ')';
			y->name		= acc;
			y->flags	= active | (*regs == 0xcdcdcdcd ? Entry::UNSET : *regs != *pregs ? Entry::CHANGED : 0);
			*pregs++	= *regs++;
		}
	}

	RedrawWindow(*this, NULL, NULL, RDW_INVALIDATE | RDW_ALLCHILDREN);
}

void DXBCRegisterWindow::AddOverlay(ListViewControl lv, int row, int col) {
	if (row > 0 && (col == 1 || col == 2)) {
		Rect	rect	= lv.GetSubItemRect(row, col);
		reg_overlay	= ListBoxControl(WindowPos(lv, rect.Grow(0, 0, 0, (sim->NumThreads() - 1) * rect.Height())), none, BORDER | CHILD | CLIPCHILDREN | CLIPSIBLINGS | VISIBLE);
		reg_overlay.SetFont(lv.GetFont());

		auto&	entry	= entries[row];
		auto&	reg		= regspecs[(row - 1) / 4];
		int		field	= (row - 1) & 3;
		for (auto &i : sim->GetRegFile<uint32[4]>(reg.type, reg.index)) {
			string_builder	a;
			reg_overlay.Add(entry.GetValue(a, col, i + field));
		}
	}
}

//-----------------------------------------------------------------------------
//	DXBCLocalsWindow
//-----------------------------------------------------------------------------

void DXBCLocalsWindow::Update(uint32 relpc, uint64 pc, int thread) {
	frame.clear();

	hash_map_with_key<const CV::SYMTYPE*, uint32>	locals;
	const CV::LOCALSYM	*parent	= 0;
	uint32			loc_size;
	uint64			loc_base;

//	entries.clear();
	tc.DeleteItem(TVI_ROOT);

	if (auto *mod = spdb->HasModule(1)) {
		for (auto &s : mod->Symbols()) {
			switch (s.rectyp) {
				case CV::S_LOCAL: {
					auto	*loc	= s.as<CV::LOCALSYM>();
					if (!parent || parent->name != loc->name || parent->typind != loc->typind) {
						parent		= loc;
						loc_size	= uint32(spdb->GetTypeSize(loc->typind));
						loc_base	= frame.add_chunk(loc_size);
					//	entries.emplace_back(loc->name, to_c_type(spdb, loc->typind), loc_base);
						AppendEntry(loc->name, to_c_type(*spdb, loc->typind), loc_base);
					}
					break;
				}
				case CV::S_DEFRANGE_HLSL: {
					auto	&r = *s.as<CV::DEFRANGESYMHLSL>();
					if (r.range().test({relpc, 1})) {

						switch (r.regType) {
							case dx::Operand::TYPE_STREAM:
								continue;
						}

						ISO_ASSERT(r.offsetParent + r.sizeInParent <= loc_size);

						frame.add_block(loc_base + r.offsetParent, sim->reg_data(thread, (dx::Operand::Type)r.regType, r.offsets()), r.sizeInParent);
					}
					break;
				}
				case CV::S_GDATA_HLSL:
				case CV::S_LDATA_HLSL: {
					auto	*data	= s.as<CV::DATASYMHLSL>();
					loc_size	= uint32(spdb->GetTypeSize(data->typind));
					loc_base	= frame.add_chunk(loc_size);
					frame.add_block(loc_base, (uint8*)sim->grp[data->dataslot]->data().begin() + data->dataoff, loc_size);
					//entries.emplace_back(data->name, to_c_type(spdb, data->typind), loc_base);
					AppendEntry(data->name, to_c_type(*spdb, data->typind), loc_base);
					break;
				}
			}
		}
	}

//	Redraw();
}

//-----------------------------------------------------------------------------
//	DXBCTraceWindow
//-----------------------------------------------------------------------------

struct DXBCTraceWindow::RegAdder : buffer_accum<256>, ListViewControl::Item {
	ListViewControl			c;
	row_data				&row;
//	uint8					types[2];
	bool					source;

	void	SetColumn(int col) {
		term();
		Column(col).Set(c);
	}
	void	SetAddress(uint64 addr) {
		format("%010I64x", addr);
		term();
		Insert(c);
	}
	void	AddValue(int i, int reg, int mask, dx::SimulatorDXBC::Register &r) {
		row.fields[i].reg	= reg;
		row.fields[i].write	= !source;
		row.fields[i].mask	= mask;

		float	*f	= (float*)&r;
		for (int j = 0; j < 4; j++) {
			reset() << f[j];
			SetColumn(i * 4 + j + 2);
		}
	}

	RegAdder(ListViewControl c, row_data &row) : ListViewControl::Item(getp()), c(c), row(row) {}
};

int DXBCTraceWindow::InsertRegColumns(int nc, const char *name) {
	for (int i = 0; i < 4; i++)
		ListViewControl::Column(buffer_accum<64>(name) << '.' << "xyzw"[i]).Width(120).Insert(c, nc++);
	return nc;
}

DXBCTraceWindow::DXBCTraceWindow(const WindowPos &wpos, dx::SimulatorDXBC *sim, int thread, int max_steps) : TraceWindow(wpos, 4) {
	static Disassembler	*dis = Disassembler::Find("DXBC");

	int	nc = 2;
	nc = InsertRegColumns(nc, "dest");
	nc = InsertRegColumns(nc, "input 0");
	nc = InsertRegColumns(nc, "input 1");
	nc = InsertRegColumns(nc, "input 2");

	for (auto *p = (const dx::Opcode*)sim->Run(0); p && max_steps--; ) {
		RegAdder	ra(c, rows.push_back());
		ra.SetAddress(sim->Offset(p));
		
		Disassembler::State	*state = dis->Disassemble(memory_block(unconst(p), p->Length * 4), sim->Offset(p));
		state->GetLine(ra.reset(), 0);
		ra.SetColumn(1);
		delete state;

		dx::ASMOperation	op(p);

		ra.source = true;
		for (int i = 1; i < op.ops.size(); i++)
			ra.AddValue(i, sim->reg(op.ops[i]), op.ops[i].Mask(), sim->ref(thread, op.ops[i]));

		p	= (const dx::Opcode*)sim->Continue(p, 1);
		ra.source = false;
		if (op.ops)
			ra.AddValue(0, sim->reg(op.ops[0]), op.ops[0].Mask(), sim->ref(thread, op.ops[0]));
	}

	selected	= 0x100;
}
