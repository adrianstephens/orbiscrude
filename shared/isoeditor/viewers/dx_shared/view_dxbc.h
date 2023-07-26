#ifndef VIEW_DXBC_H
#define VIEW_DXBC_H

#include "dx_gpu.h"
#include "dx/sim_dxbc.h"

namespace app {

class DXBCRegisterWindow : public DXRegisterWindow {
	struct RegSpec {
		dx::Operand::Type	type;
		int					index;
	};
	const dx::SimulatorDXBC	*sim;
	shared_ptr<ParsedSPDB>	spdb;
	dynamic_array<RegSpec>	regspecs;

	uint32			AddReg(dx::Operand::Type type, int index, uint32 offset);
	uint32			AddInputRegs(const dx::Decls *decl);

public:
	using DXRegisterWindow::AddOverlay;

	void	AddOverlay(ListViewControl lv, int row, int col) override;
	void	Update(uint32 relpc, uint64 pc, int thread) override;

	DXBCRegisterWindow(const WindowPos& wpos, const dx::SimulatorDXBC *sim, const shared_ptr<ParsedSPDB> &spdb) : DXRegisterWindow(wpos), sim(sim), spdb(spdb) {}
};

class DXBCLocalsWindow : public DXLocalsWindow {
	const dx::SimulatorDXBC	*sim;
	shared_ptr<ParsedSPDB>	spdb;
	FrameMemoryInterface0	frame;
public:
	DXBCLocalsWindow(const WindowPos &wpos, const dx::SimulatorDXBC *sim, const shared_ptr<ParsedSPDB> &spdb, const C_types &types) : DXLocalsWindow(wpos, "Locals", types, &frame), sim(sim), spdb(spdb) {}
	void	Update(uint32 relpc, uint64 pc, int thread) override;
};

class DXBCTraceWindow : public TraceWindow {
	struct RegAdder;
	int InsertRegColumns(int nc, const char *name);
public:
	DXBCTraceWindow(const WindowPos &wpos, dx::SimulatorDXBC *sim, int thread, int max_steps = 0);
};

}	//namespace app

#endif //VIEW_DXBC_H
