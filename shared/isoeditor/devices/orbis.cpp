#include "com.h"
#include "windows/text_control.h"
#include "main.h"
#include "device.h"
#include "thread.h"
#include "extra/date.h"
#include "ortmapi.tlh"

using namespace app;

struct ORBISMemory0 : public ISO::VirtualDefaults {
	static const uint64 BLOCK_SIZE = 0x10000;
	IProcess			*process;
	xint64				start, end;

	ORBISMemory0(IProcess *p) : process(p) {}

	uint32			Count() {
		return (end - start + BLOCK_SIZE - 1) / BLOCK_SIZE;
	}
	ISO::Browser2	Index(int i) {
		uint64		a		= start + i * BLOCK_SIZE;
		uint32		size	= (uint32)min(end - a, BLOCK_SIZE);
		uint32		bad;
		com_variant	v;
		if (SUCCEEDED(process->GetMemory(a, size, &bad, &v))) {
			size -= bad;
			ISO_ptr<ISO_openarray<xint8> >	p(0, size);
			memcpy(*p, com_safearray(v).data(), size);
			return p;
		}
		return ISO::Browser2();
	}
};
ISO_DEFVIRT(ORBISMemory0);

//-----------------------------------------------------------------------------
//	ORBISSegment
//-----------------------------------------------------------------------------
struct ORBISSegment {
	ORBISMemory0	bin;
	uint32			attr;
	ORBISSegment(IProcess *p, ISegmentInfo *i) : bin(p) {
		uint64	base, size;
		i->get_BaseAddress(&base);
		i->get_Size(&size);
		i->get_Attributes((eSegmentAttr*)&attr);
		bin.start	= base;
		bin.end		= base + size;
	}
};
//ISO_DEFUSERCOMPV(ORBISSegment, bin, attr);

ISO_DEFUSERCOMPX(ORBISSegment,3,"StartBin") {
	ISO_SETFIELDX1(0, bin.start, "start");
	ISO_SETFIELD(1, bin);
	ISO_SETFIELD(2, attr);
} };

//-----------------------------------------------------------------------------
//	ORBISModule
//-----------------------------------------------------------------------------
class ORBISModule {
public:
	enum {
		ORIGIN = 1,
		SYMBOLIC = 2,
		TEXT_REL = 4,
		BIND_NOW = 8,
		STATIC_TLS = 16
	};

	string			name;
	string			fingerprint;
	string			filename;
	string			load;

	xint64			id;
	xint64			entry;
	xint64			stop;
	uint32			sdk, flags, status, type, version;

	malloc_block	metadata;
	dynamic_array<ORBISSegment>	segments;

	ORBISModule(IProcess *p, IModule *m) {
		com_string	s;
		if (SUCCEEDED(m->get_Name(&s)))
			name = s;
		if (SUCCEEDED(m->get_Fingerprint(&s)))
			fingerprint = s;
		if (SUCCEEDED(m->get_OriginalFile(&s)))
			filename = s;
		if (SUCCEEDED(m->get_LoadPath(&s)))
			load = s;

		m->get_Id(&id.i);
		m->get_StartEntryAddress(&entry.i);
		m->get_StopEntryAddress(&stop.i);

		m->get_Version(&version);
		m->get_SDKVersion(&sdk);
		m->get_Type((eModuleTypes*)&type);
		m->get_Status((eModuleStatus*)&status);
		m->get_Flags((eModuleFlags*)&flags);


		com_variant			result;
		if (SUCCEEDED(m->get_Segments(&result))) {
			com_interface_array<ISegmentInfo>	array(result);
			for (com_interface_array<ISegmentInfo>::iterator i = array.begin(), e = array.end(); i != e; ++i)
				new(segments) ORBISSegment(p, *i);
		}

		m->get_MetaData(&result);
		com_quickarray<uint8>	a(result);
		memcpy(metadata.create(a.count()), a, a.count());
	}
	tag2	ID() const { return name; }
};

ISO_DEFUSERCOMPV(ORBISModule, load, id, entry, fingerprint, version, metadata, segments);

//-----------------------------------------------------------------------------
//	ORBISThread
//-----------------------------------------------------------------------------
class ORBISThread {
public:
	string			name;
	xint32			id;
	xint64			exit_code;
	uint32			priority_class;
	uint32			priority_level;
	uint32			priority_native;
	uint32			priority_user;
	xint64			stack_address_top;
	xint64			stack_size;
//	eThreadState	state;
//	IStopReason		stop_reason;
//	eThreadType		type
//	eThreadWaitState	wait_state;

	ORBISThread(IProcess *p, IThreadInfo *m) {
		com_string	s;
		if (SUCCEEDED(m->get_Name(&s)))
			name = s;

		m->get_Id(&id.i);
		m->get_ExitCode(&exit_code.i);
		m->get_PriorityClass(&priority_class);
		m->get_PriorityLevel(&priority_level);
		m->get_PriorityNative(&priority_native);
		m->get_PriorityUser(&priority_user);
		m->get_StackAddressTop(&stack_address_top.i);
		m->get_StackSize(&stack_size.i);
//		m->get_State(&state);
//		m->get_StopReason(&stop_reason);
//		m->get_Type(&type);
//		m->get_WaitState(&wait_state);
	}
	tag2	ID() const { return name; }
};

ISO_DEFUSERCOMPV(ORBISThread, id, exit_code, priority_class, priority_level, priority_native, priority_user, stack_address_top, stack_size);
//-----------------------------------------------------------------------------
//	ORBISMemory
//-----------------------------------------------------------------------------

enum ORBIS_MEM_TYPE;
ISO_DEFUSERENUMX(eMemoryType, 10, "TYPE") {
#define ENUM(x) #x, MEM_TYPE_##x
	Init(0,
		ENUM(WB_ONION_VOLATILE),
		ENUM(WB_ONION_NONVOLATILE),
		ENUM(WC_GARLIC_VOLATILE),
		ENUM(WC_GARLIC_NONVOLATILE),
		ENUM(WT_ONION_VOLATILE),
		ENUM(WT_ONION_NONVOLATILE),
		ENUM(WP_ONION_VOLATILE),
		ENUM(WP_ONION_NONVOLATILE),
		ENUM(UC_GARLIC_VOLATILE),
		ENUM(UC_GARLIC_NONVOLATILE)
	);
#undef ENUM
}};
ISO_DEFUSERENUMX(eProtectionFlags, 6, "PROTECTION") {
#define ENUM(x) #x, PROTECTION_##x
	Init(0,
		ENUM(CPU_READ),
		ENUM(CPU_WRITE),
		ENUM(CPU_EXECUTE),
		ENUM(GPU_READ),
		ENUM(GPU_WRITE)
	);
#undef ENUM
}};

struct ORBISMemory {
	ORBISMemory0		bin;
	eMemoryType			type;
	eProtectionFlags	prot;

	ORBISMemory(IProcess *p, IVirtualMemory *v) : bin(p) {
		uint64	start, end;
		v->get_StartAddress(&start);
		v->get_EndAddress(&end);
		v->get_Type((uint32*)&type);
		v->get_Protection((uint32*)&prot);
		bin.start	= start;
		bin.end		= end + 1;
	}
};

ISO_DEFUSERCOMPX(ORBISMemory,4,"StartBin") {
	ISO_SETFIELDX1(0, bin.start, "start");
	ISO_SETFIELD(1,  bin);
	ISO_SETFIELD(2, type);
	ISO_SETFIELD(3, prot);
} };


//-----------------------------------------------------------------------------
//	ORBISProcess
//-----------------------------------------------------------------------------

struct PS4Target : public ISO::VirtualDefaults {
	ITarget*				target;
	com_ptr<IProcess>		process;
	PS4Target(ITarget *_target, IProcessInfo *info) : target(_target) { info->get_ProcessInterface(&process); }
};

struct PS4GPUState : public PS4Target {};
struct PS4CPUState : public PS4Target {};

ISO_DEFUSERVIRT(PS4GPUState);
ISO_DEFUSERVIRT(PS4CPUState);

class ORBISProcess : public PS4Target {
public:
	string						name;
	dynamic_array<ORBISMemory>	memory;
	dynamic_array<ORBISModule>	modules;
	dynamic_array<ORBISThread>	threads;
public:
	ORBISProcess(ITarget *_target, IProcessInfo *info);
	tag2			GetName()		{ return name;	}

	uint32			Count()			{ return 5;	}
	tag2			GetName(int i)	{
		switch (i) {
			case 0:		return "Memory";
			case 1:		return "Modules";
			case 2:		return "Threads";
			case 3:		return "GPU";
			case 4:		return "CPU";
			default:	return 0;
		}
	}
	ISO::Browser2	Index(int i) {
		switch (i) {
			case 0:		return ISO::MakeBrowser(memory);
			case 1:		return ISO::MakeBrowser(modules);
			case 2:		return ISO::MakeBrowser(threads);
			case 3:		return ISO::MakeBrowser(*(PS4GPUState*)(PS4Target*)this);
			case 4:		return ISO::MakeBrowser(*(PS4CPUState*)(PS4Target*)this);
			default:	return ISO::Browser();
		}
	}
};
ISO_DEFVIRT(ORBISProcess);

ORBISProcess::ORBISProcess(ITarget *_target, IProcessInfo *info) : PS4Target(_target, info) {
	com_string	_name;
	if (SUCCEEDED(info->get_Name(&_name)))
		name = _name;

	if (name == "System")
		return;

	process->Attach(0);
	if (com_ptr<IProcess2> p2 = process.query<IProcess2>()) {
		com_variant	result;
		if (SUCCEEDED(p2->get_VirtualMemoryInfo(&result)) && memory.empty()) {
			com_interface_array<IVirtualMemory>	array(result);
			for (com_interface_array<IVirtualMemory>::iterator i = array.begin(), e = array.end(); i != e; ++i) {
				uint32	flags;
				if (SUCCEEDED(i->get_Flags(&flags))/* && flags == 1*/)
					new(memory) ORBISMemory(process, *i);
			}
		}
	}
	com_variant	result;
	if (SUCCEEDED(process->get_Modules(&result)) && modules.empty()) {
		com_interface_array<IModule>	array(result);
		for (com_interface_array<IModule>::iterator i = array.begin(), e = array.end(); i != e; ++i)
			new(modules) ORBISModule(process, *i);
	}

	if (SUCCEEDED(process->get_ThreadInfoSnapshot(&result))) {
		com_interface_array<IThreadInfo>	array(result);
		for (com_interface_array<IThreadInfo>::iterator i = array.begin(), e = array.end(); i != e; ++i)
			new(threads) ORBISThread(process, *i);
	}
}

//-----------------------------------------------------------------------------
//	ORBISSetting
//-----------------------------------------------------------------------------

struct ORBISSetting : ISO::VirtualDefaults, com_ptr<ISetting> {
	ORBISSetting(ISetting *_setting) : com_ptr<ISetting>(_setting) {}
	ISO::Browser2	Deref() {
		return ISO::Browser2();
	}
};
ISO_DEFVIRT(ORBISSetting);

//-----------------------------------------------------------------------------
//	ORBISTarget
//-----------------------------------------------------------------------------
#if 0
class ORBISTarget : public ISO::VirtualDefaults {
	com_ptr<ITarget>			target;
	string						name;
	dynamic_array<ORBISProcess>	processes;
public:
	ORBISTarget(ITarget *p) : target(p) {
		com_string	_name;
		if (FAILED(p->get_Name(&_name)) || _name.length() == 0)
			p->get_HostName(&_name);
		name = _name;

		target->Connect();
		com_variant			result;
		if (SUCCEEDED(target->get_ProcessInfoSnapshot(&result)) && processes.empty()) {
			com_interface_array<IProcessInfo>	array(result);
			for (int i = 0, n = array.count(); i < n; i++) {
				new (processes) ORBISProcess(target, array[i]);
			}
		}
	}

	ITarget*		Target()	const	{ return target; }
	const char*		Name()		const	{ return name; }

	uint32			Count()			{ return processes.size();	}
	tag2			GetName(int i)	{ return processes[i].GetName();	}
	ISO::Browser2	Index(int i)	{ return ISO::MakeBrowser(processes[i]);	}
};
ISO_DEFVIRT(ORBISTarget);

#else

class ORBISTarget {
	com_ptr<ITarget>			target;
	string						name;
public:
	dynamic_array<ORBISProcess>	processes;
	dynamic_array<ORBISSetting>	settings;

	ORBISTarget(ITarget *p) : target(p) {
		com_string	_name;
		if (FAILED(p->get_Name(&_name)) || _name.length() == 0)
			p->get_HostName(&_name);
		name = _name;

		target->Connect();
		com_variant			result;
		if (SUCCEEDED(target->get_ProcessInfoSnapshot(&result)) && processes.empty()) {
			com_interface_array<IProcessInfo>	array(result);
			for (int i = 0, n = array.count(); i < n; i++)
				new (processes) ORBISProcess(target, array[i]);
		}

		com_variant			result2;
		if (SUCCEEDED(target->GetSettings(&result2, TRUE)) && settings.empty()) {
			com_interface_array<ISetting>	array(result2);
			for (int i = 0, n = array.count(); i < n; i++)
				new (settings) ORBISSetting(array[i]);
		}
	}

	ITarget*		Target()	const	{ return target; }
	const char*		Name()		const	{ return name; }
};

ISO_DEFUSERCOMPV(ORBISTarget,processes, settings);

#endif

//-----------------------------------------------------------------------------
//	ORBISExplorer
//-----------------------------------------------------------------------------

class ORBISExplorer : public ISO::VirtualDefaults {
	dynamic_array<ORBISTarget>		consoles;
	com_ptr<IORTMAPI>				tm;
public:
	bool			Init();
	uint32			Count()				{ return uint32(consoles.size());	}
	tag				GetName(int i)		{ return consoles[i].Name();		}
	ISO::Browser		Index(int i)		{ return ISO::MakeBrowser(consoles[i]);	}
};

bool ORBISExplorer::Init() {
	init_com();
	if (!tm.create<ORTMAPI>() || FAILED(tm->CheckCompatibility(BuildVersion)))
		return false;
	com_variant						var;
	if (SUCCEEDED(tm->get_Targets(&var))) {
		com_interface_array<ITarget>	targets(var);
		for (com_interface_array<ITarget>::iterator i = targets.begin(), e = targets.end(); i != e; ++i)
			consoles.push_back(*i);
		return true;
	}
	return false;
}

ISO_DEFVIRT(ORBISExplorer);

//-----------------------------------------------------------------------------
//	ORBISDevice
//-----------------------------------------------------------------------------

struct OrbisDevice : DeviceT<OrbisDevice>, DeviceCreateT<OrbisDevice> {
	void			operator()(const DeviceAdd &add)	{ add("ORBIS", this, app::LoadPNG("IDB_DEVICE_PLAYSTATION")); }
	ISO_ptr<void>	operator()(const Control &main)		{
		ISO_ptr<ORBISExplorer>	p("ORBIS");
		if (p->Init())
			return p;
		return ISO_NULL;
	}
} orbis_device;

//-----------------------------------------------------------------------------
//	ORBIS TTY
//-----------------------------------------------------------------------------

class ViewORBISTTY : public TextWindow, public TDispatch<IEventConsoleOutput> {
	com_ptr<ITarget> target;

	STDMETHODIMP OnConsoleOutput(IConsoleOutputEvent* pEvent) {
		SetSelection(CharRange::end());

		com_string				text;
		eConsoleOutputCategory	category;
		eConsoleOutputPort		port;
		uint64					time;
		uint32					pid, tid;

		pEvent->get_Text(		&text);
		pEvent->get_Category(	&category);
		pEvent->get_Port(		&port);
		pEvent->get_Timestamp(	&time);
		pEvent->get_ProcessId(	&pid);
		pEvent->get_ThreadId(	&tid);

		buffer_accum<256>		ba;
		ba << '[' << pid << '-' << tid << ':' << port << /*'@' << time <<*/ "] ";

		ReplaceSelection(ba, false);
		ReplaceSelection(string(text), false);
		SendMessage(WM_VSCROLL, SB_BOTTOM);
		return S_OK;
	}
	STDMETHODIMP OnBufferReady(IBufferReadyEvent* pEvent) {
		return S_OK;
	}

public:
	LRESULT Proc(MSG_ID message, WPARAM wParam, LPARAM lParam) {
		if (message == WM_NCDESTROY) {
			delete this;
			return 0;
		}
		return TextWindow::Proc(message, wParam, lParam);
	}
	ViewORBISTTY(const WindowPos &pos, ITarget *_target) : TextWindow(pos, "TTY"), target(_target) {
		target->AdviseConsoleOutputEvents(this);
		Rebind(this);
	}
	~ViewORBISTTY() {
		target->UnadviseConsoleOutputEvents(this);
	}
};

class EditorORBISTTY : public Editor {
	virtual bool Matches(const ISO::Type *type) {
		return type->Is<ORBISTarget>();
	}
	virtual Control	Create(class MainWindow &main, const WindowPos &wpos, const ISO_ptr<void> &p) { return Control(); }
	virtual Control	Create(class MainWindow &main, const WindowPos &wpos, const ISO_VirtualTarget &b) {
		return *new ViewORBISTTY(wpos, ((ORBISTarget*)b)->Target());
	}
} editororbistty;

