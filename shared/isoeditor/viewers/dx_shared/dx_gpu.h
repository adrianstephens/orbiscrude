#ifndef DX_GPU_H
#define DX_GPU_H

#include "../gpu.h"
#include "dx/dx_shaders.h"
#include "dx/dxgi_helpers.h"
#include "dx/dx_sim.h"
#include "extra/colourise.h"
#include "hook_com.h"

namespace iso {

struct ParsedSPDB;

namespace dx {

const C_type *to_c_type(DXGI_FORMAT f);
const C_type *to_c_type(DXGI_COMPONENTS f);

ISO_ptr_machine<void> GetBitmap(const char *name, Resource &rec);
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
	
	const	dx::DXBC *DXBC() const {
		return data;
	}
	const_memory_block	GetUCode() const {
		DXBC::UcodeHeader header;
		if (auto dxbc = DXBC())
			return dxbc->GetUCode(header);
		return empty;
	}

	uint64	GetUCodeAddr() const {
		return (GetUCode() - (const char*)data) + addr;
	}

	dx::Signature	GetSignatureIn() const {
		if (auto dxbc = DXBC()) {
			for (auto &blob : dxbc->Blobs()) {
				if (auto* sig = blob.as<dx::DXBC::InputSignature>())
					return sig;

				else if (auto* sig = blob.as<dx::DXBC::InputSignature1>())
					return sig;
			}
		}
		return {};
	}
	dx::Signature	GetSignatureOut() const {
		if (auto dxbc = DXBC()) {
		#if 1
			dx::Signature	sig;
			dxbc->GetBlobM<dx::DXBC::OutputSignature, dx::DXBC::OutputSignature1, dx::DXBC::OutputSignature5>(sig);
			return sig;
		#else
			for (auto blob : dxbc->Blobs()) {
				if (auto* sig = blob.as<dx::DXBC::OutputSignature>())
					return sig;

				else if (auto* sig = blob.as<dx::DXBC::OutputSignature1>())
					return sig;

				else if (auto* sig = blob.as<dx::DXBC::OutputSignature5>())
					return sig;
			}
		#endif
		}
		return {};
	}
	operator bool() const { return (bool)data; }
};

Topology	GetTopology(D3D_PRIMITIVE_TOPOLOGY prim);
Topology	GetTopology(PrimitiveType prim);
Topology	GetTopology(PrimitiveTopology prim);
Topology	GetTopology(TessellatorOutputPrimitive prim);
Tesselation	GetTesselation(TessellatorDomain domain, range<stride_iterator<const float4p>> pc);
Triangle	GetTriangle(SimulatorDX *sim, const char *semantic_name, int semantic_index, const dx::Signature &sig, int i0, int i1, int i2);
Triangle	GetTriangle(SimulatorDX *sim, const SIG::Element *e, int i0, int i1, int i2);

} }

namespace app {

const SyntaxColourerRE&	HLSLcolourerRE();
Control			MakeHTMLViewer(const WindowPos &wpos, const char *title, const char *text, size_t len);
int				MakeHeaders(ListViewControl lv, int nc, DXGI_COMPONENTS fmt, const char *prefix = 0, const char *suffix = 0);
void			AddValue(RegisterTree &rt, HTREEITEM h, const char *name, const void *data, D3D_SHADER_VARIABLE_CLASS Class, D3D_SHADER_VARIABLE_TYPE Type, uint32 rows, uint32 cols);
Control			MakeShaderOutput(const WindowPos &wpos, dx::SimulatorDX *sim, const dx::Shader &shader, dynamic_array<uint32> &indices);
void			AddShaderOutput(Control c, dx::SimulatorDX *sim, const dx::Shader &shader, dynamic_array<uint32> &indices);
const C_type*	to_c_type(D3D_SHADER_VARIABLE_CLASS Class, D3D_SHADER_VARIABLE_TYPE Type, uint32 rows, uint32 cols);

//-----------------------------------------------------------------------------
//	DXRegisterWindow
//-----------------------------------------------------------------------------

class DXRegisterWindow : public RegisterWindow {
protected:
	ListBoxControl			reg_overlay;
public:
	virtual ~DXRegisterWindow()	{}
	virtual void	Update(uint32 relpc, uint64 pc, int thread) = 0;
	virtual void	AddOverlay(ListViewControl lv, int row, int col) = 0;

	void	AddOverlay(NMITEMACTIVATE *nma) {
		RemoveOverlay();
		AddOverlay(nma->hdr.hwndFrom, nma->iItem, nma->iSubItem);
	}
	void	RemoveOverlay() {
		if (reg_overlay) {
			reg_overlay.Destroy();
			reg_overlay = 0;
		}
	}

	LRESULT Proc(MSG_ID message, WPARAM wParam, LPARAM lParam) {
		if (message == WM_NCDESTROY) {
			delete this;
			return 0;
		}
		return RegisterWindow::Proc(message, wParam, lParam);

	}
	DXRegisterWindow(const WindowPos& wpos) : RegisterWindow(wpos) {
		Rebind(this);
	}
};

//-----------------------------------------------------------------------------
//	DXLocalsWindow
//-----------------------------------------------------------------------------

class DXLocalsWindow : public LocalsWindow {
public:
	virtual ~DXLocalsWindow()	{}
	virtual void	Update(uint32 relpc, uint64 pc, int thread) = 0;

	LRESULT Proc(MSG_ID message, WPARAM wParam, LPARAM lParam) {
		if (message == WM_CREATE)
			return 0;
		if (message == WM_NCDESTROY) {
			delete this;
			return 0;
		}
		return LocalsWindow::Proc(message, wParam, lParam);
	}

	DXLocalsWindow(const WindowPos &wpos, const char *title, const C_types &types, memory_interface *mem = 0) : LocalsWindow(wpos, title, types, mem) {
		Rebind(this);
	}
};

//-----------------------------------------------------------------------------

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
		if (!address)
			return false;
		SocketWait	sock	= addr.connect_or_close(IP4::TCP());
		auto		size2	= SocketCallRPC<uint32>(sock, INTF_GetMemory, address, size);
		return readbuff_all(sock, buffer, size2) == size;
	}
};

struct DXConnection : refs<DXConnection> {
	Process2			process;
	bool				paused;
	IP4::socket_addr	addr;

	DXConnection();
	~DXConnection();

	template<typename R, typename...P> auto	Call(int func, const P&...p) {
		return SocketCallRPC<R>(addr.connect_or_close(IP4::TCP()), func, p...);
	}
	ip_memory_interface MemoryInterface() {
		return addr;
	}
	void		ConnectDebugOutput();
	filename	GetDLLPath(const char *dll_name);
	bool		OpenProcess(uint32 id, const char *dll_name);
	bool		OpenProcess(const filename &app, const char *dir, const char *args, const char *dll_name, const char *dll_path);
	bool		OpenProcess(const filename &app, const char *dir, const char *args, const char *dll_name) {
		return OpenProcess(app, dir, args, dll_name, GetDLLPath(dll_name));
	}

};

}

#endif //DX_GPU_H
