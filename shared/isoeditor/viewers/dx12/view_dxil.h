#ifndef VIEW_DXIL_H
#define VIEW_DXIL_H

#include "../dx_shared/dx_gpu.h"
#include "dx/sim_dxil.h"

namespace app {

const C_type *to_c_type(const char *name, const dxil::Type* type, const dxil::TypeSystem& types);
const C_type *to_c_type(const dxil::Type* type);

bool	ReadDXILModule(bitcode::Module &mod, const dx::DXBC::BlobT<dx::DXBC::DXIL> *dxil);
void	GetDXILSource(const bitcode::Module &mod, Disassembler::Files &files, Disassembler::SharedFiles &shared_files);
void	GetDXILSourceLines(const bitcode::Module &mod, Disassembler::Files &files, Disassembler::Locations &locations, const char *name = nullptr);
string	GetDXILShaderName(const dx::DXBC::BlobT<dx::DXBC::DXIL> *dxil, uint64 addr);
dxil::meta_entryPoint	GetEntryPoint(const bitcode::Module &mod, const char *name = nullptr);

malloc_block SignShader(const dx::DXBC *dxbc);
malloc_block ReplaceDXIL(const dx::DXBC *dxbc, const bitcode::Module &mod, bool sign);


class DXILRegisterWindow : public DXRegisterWindow {
	const dx::SimulatorDXIL *sim;
	uint32	AddInOut(const dx::SimulatorDXIL *sim, int thread, const dxil::meta_entryPoint::signature &sig, dx::Operand::Type type, uint32 offset);

public:
	using DXRegisterWindow::DXRegisterWindow;
	void	AddOverlay(ListViewControl lv, int row, int col) override;
	void	Update(uint32 relpc, uint64 pc, int thread) override;
	DXILRegisterWindow(const WindowPos& wpos, const dx::SimulatorDXIL *sim) : DXRegisterWindow(wpos), sim(sim) {}
};

class DXILLocalsWindow : public DXLocalsWindow {
	const dx::SimulatorDXIL *sim;
	FrameMemoryInterface0	frame;
	dynamic_array<uint8>	zeroes;
	dynamic_array<uint8>	consts;

public:
	DXILLocalsWindow(const WindowPos &wpos, const dx::SimulatorDXIL *sim, const C_types &types) : DXLocalsWindow(wpos, "Locals", types, &frame), sim(sim) {}
	void	Update(uint32 relpc, uint64 pc, int thread) override;
};

class DXILGlobalsWindow : public LocalsWindow {
//	local_memory_interface mem;
public:
	DXILGlobalsWindow(const WindowPos &wpos, const dx::SimulatorDXIL *sim, const C_types &types);
};

class DXILTraceWindow : public TraceWindow {
	struct RegAdder;
public:
	DXILTraceWindow(const WindowPos &wpos, dx::SimulatorDXIL *sim, int thread, int max_steps = 0);
};
}	//namespace app
#endif //VIEW_DXIL_H
