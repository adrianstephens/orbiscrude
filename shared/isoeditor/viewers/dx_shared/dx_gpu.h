#ifndef DX_GPU_H
#define DX_GPU_H

#include "..\gpu.h"
#include "dx\dx_shaders.h"
#include "dx\dxgi_helpers.h"
#include "dx\spdb.h"
#include "dx\sim_dxbc.h"
#include "extra\colourise.h"
#include "hook_com.h"

namespace iso { namespace dx {

const C_type *to_c_type(DXGI_FORMAT f);
const C_type *to_c_type(DXGI_COMPONENTS f);

ISO_ptr_machine<void> GetBitmap(const char *name, SimulatorDXBC::Resource &rec);
ISO_ptr_machine<void> GetBitmap(const char *name, const void *srce, DXGI_COMPONENTS format, int width, int height, int depth, int mips, int flags);

typedef mutex_memory_cache<uint64>	cache_type;
typedef cache_type::cache_block		cache_block;

struct memory_interface : iso::memory_interface {
	HANDLE	h;
	memory_interface(HANDLE _h) : h(_h) {}

	bool	_get(void *buffer, uint64 size, uint64 address) {
		ISO_TRACEF("GetMem: start = 0x%I64x size = 0x%x\n", address, size);
		SIZE_T	read;
		ReadProcessMemory(h, (void*)address, buffer, size, &read);
		return read;
	}
};

struct Shader {
	SHADERSTAGE			stage;
	const_memory_block	data;
	uint64				addr;

	Shader() : addr(0) {}
	Shader(SHADERSTAGE stage, const_memory_block data, uint64 addr = 0) : stage(stage), data(data), addr(addr) {}
	void	init(SHADERSTAGE _stage, const_memory_block _data, uint64 _addr = 0) {
		stage	= _stage;
		data	= _data;
		addr	= _addr;
	}
	const dx::DXBC *DXBC() const {
		return data;
	}
	memory_block	GetUCode() const {
		if (auto dxbc = DXBC())
			return dxbc->GetUCode();
		return empty;
	}
	uint64			GetUCodeAddr() const {
		return (GetUCode() - (const char*)data) + addr;
	}
	Disassembler::State *Disassemble() const {
		static Disassembler	*dis = Disassembler::Find("DXBC");
		if (dis)
			return dis->Disassemble(GetUCode(), GetUCodeAddr());
		return 0;
	}
	operator bool() const { return (bool)data; }
};

Topology2 GetTopology(PrimitiveType prim, uint32 chunks = 0);
Topology2 GetTopology(PrimitiveTopology prim);
Topology2 GetTopology(TessellatorOutputPrimitive prim);

Tesselation GetTesselation(TessellatorDomain domain, range<stride_iterator<const SimulatorDXBC::Register>> pc);
SimulatorDXBC::Triangle GetTriangle(SimulatorDXBC &sim, const char *semantic_name, int semantic_index, OSGN *vs_out, OSG5 *gs_out, int i0, int i1, int i2);

} }

namespace app {

const SyntaxColourerRE&	HLSLcolourerRE();
Control			MakeHTMLViewer(const WindowPos &wpos, const char *title, const char *text, size_t len);
int				MakeHeaders(ListViewControl lv, int nc, DXGI_COMPONENTS fmt, const char *prefix = 0, const char *suffix = 0);
void			AddValue(RegisterTree &rt, HTREEITEM h, const char *name, const void *data, D3D_SHADER_VARIABLE_CLASS Class, D3D_SHADER_VARIABLE_TYPE Type, uint32 rows, uint32 cols);
Control			MakeShaderOutput(const WindowPos &wpos, dx::SimulatorDXBC &sim, const dx::Shader &shader, const indices &ix = indices());
Control			MakeDXBCTraceWindow(const WindowPos &wpos, dx::SimulatorDXBC &sim, int thread, int max_steps = 0);

const C_type *to_c_type(PDB_types &pdb, TI ti);
const CV::HLSL *get_hlsl_type(PDB_types &pdb, TI ti);

class DXBCRegisterWindow : public RegisterWindow {
	ListBoxControl			reg_overlay;

	uint32	AddReg(dx::Operand::Type type, int index, uint32 offset) {
		regspecs.push_back() = {type, index};
		for (int j = 0; j < 4; j++, offset += 4)
			entries.emplace_back(offset, RegisterName(type, index, 1 << j), float_field);
		return offset;
	}
public:
	dynamic_array<dx::RegSpec>	regspecs;

	DXBCRegisterWindow(const WindowPos &wpos) : RegisterWindow(wpos) {}
	void	Update(const dx::SimulatorDXBC &sim, dx::SHADERSTAGE stage, ParsedSPDB &spdb, uint64 pc, int thread);

	void	AddOverlay(NMITEMACTIVATE *nma, const dx::SimulatorDXBC &sim) {
		if (reg_overlay)
			reg_overlay.Destroy();

		int	item	= nma->iItem;
		if (item > 0 && (nma->iSubItem == 1 || nma->iSubItem == 2)) {
			ListViewControl	lv		= nma->hdr.hwndFrom;
			auto	*rw		= (DXBCRegisterWindow*)lv.FindAncestorByProc<RegisterWindow>();
			auto	&reg	= rw->regspecs[(item - 1) / 4];
			auto	vals	= sim.GetRegFile(reg.type, reg.index);
			int		field	= (item - 1) & 3;
			Rect	rect	= lv.GetSubItemRect(nma->iItem, nma->iSubItem);

			reg_overlay	= ListBoxControl(WindowPos(lv, rect.Grow(0, 0, 0, (sim.NumThreads() - 1) * rect.Height())), 0, BORDER | CHILD | CLIPCHILDREN | CLIPSIBLINGS | VISIBLE);
			reg_overlay.SetFont(lv.GetFont());

			for (auto &i : vals)
				reg_overlay.Add(nma->iSubItem == 1 ? format_string("0x%08x", iorf(i[field])) : format_string("%g", i[field]));
		}
	}

	void	RemoveOverlay() {
		if (reg_overlay) {
			reg_overlay.Destroy();
			reg_overlay = 0;
		}
	}
};

class DXBCLocalsWindow : public LocalsWindow {
	FrameMemoryInterface0	frame;
public:
	DXBCLocalsWindow(const WindowPos &wpos, const C_types &types) : LocalsWindow(wpos, "Locals", types, &frame) {}
	void	Update(const dx::SimulatorDXBC &sim, ParsedSPDB &spdb, uint64 pc, int thread);
};

template<typename S> struct stream_memory_interface : iso::memory_interface {
	S		stream;
	static const int INTF_GetMemory = 0;
	stream_memory_interface(S &&stream) : stream(forward<S>(stream)) {}
	bool	_get(void *buffer, uint64 size, uint64 address) {
		SocketCallRPC<void>(stream, INTF_GetMemory, address, size);
		return readbuff_all(stream, buffer, size) == size;
	}
};

struct ip_memory_interface : iso::memory_interface {
	IP4::socket_addr	addr;
	static const int INTF_GetMemory = 0;
	ip_memory_interface(IP4::socket_addr addr) : addr(addr) {}
	bool	_get(void *buffer, uint64 size, uint64 address) {
		SocketWait	sock = addr.socket();
		SocketCallRPC<void>(sock, INTF_GetMemory, address, size);
		return readbuff_all(sock, buffer, size) == size;
	}
};
struct DXCapturer {
	Process2			process;
	bool				paused;
//	HANDLE				pipe	= 0;
	IP4::socket_addr	addr;

	DXCapturer() : paused(false), addr(IP4::localhost, PORT(4567)) {}

	~DXCapturer();
	filename	GetDLLPath(const char *dll_name);
	bool		OpenProcess(uint32 id, const char *dll_name);
	bool		OpenProcess(const filename &app, const char *dir, const char *args, const char *dll_name, const char *dll_path);
	bool		OpenProcess(const filename &app, const char *dir, const char *args, const char *dll_name) {
		return OpenProcess(app, dir, args, dll_name, GetDLLPath(dll_name));
	}

	template<typename R, typename...P> auto	Call(int func, const P&...p) {
		return SocketCallRPC<R>(addr.socket(), func, p...);
	}

	ip_memory_interface MemoryInterface() {
		return addr;
	}

	void		ConnectDebugOutput();
};

}

#endif //DX_GPU_H
