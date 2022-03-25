#include "main.h"
#include "iso/iso.h"
#include "iso/iso_binary.h"
#include "windows/window.h"
#include "base/hash.h"
#include "com.h"
#include "filename.h"
#include "hook.h"
#include "utilities/hook.h"

#include <psapi.h>
#include <Wbemidl.h>
#pragma comment(lib, "wbemuuid.lib")

/*

string GetProcessCommandLine(HANDLE hProc) {
	Process	proc(hProc);
	PROCESS_BASIC_INFORMATION pbi = proc.GetBasicInformation();
	return proc.GetString(&proc.Get(&pbi.PebBaseAddress->ProcessParameters)->CommandLine);
}

*/
using namespace iso;

//-----------------------------------------------------------------------------
//	wmi for remote processes
//-----------------------------------------------------------------------------

struct RemoteProcess {
	uint32	id;
	string	name;
	RemoteProcess(IWbemClassObject *obj) {
		com_variant	var_id;
		com_variant	var_name;
		obj->Get(L"ProcessId", 0, &var_id, NULL, NULL);
		obj->Get(L"Name", 0, &var_name, NULL, NULL);
		id		= var_id;
		name	= com_string(var_name);
	}
};
ISO_DEFUSERCOMPV(RemoteProcess, id, name);

class RemoteProcesses : public ISO::VirtualDefaults {
	dynamic_array<RemoteProcess>	processes;
public:
	RemoteProcesses(const char *machine);
	int				Count()			{ return processes.size32();	}
	tag2			GetName(int i)	{ return processes[i].name; }
	ISO::Browser2	Index(int i)	{ return ISO::MakeBrowser(processes[i]); }
};
ISO_DEFUSERVIRT(RemoteProcesses);

RemoteProcesses::RemoteProcesses(const char *machine) {
	HRESULT hr;
	hr	= CoInitializeEx(NULL, COINIT_MULTITHREADED);
#if 0
	hr = CoInitializeSecurity(
		NULL,
		-1,								// COM negotiates service
		NULL,							// Authentication services
		NULL,							// Reserved
		RPC_C_AUTHN_LEVEL_CONNECT,		// Default authentication
		RPC_C_IMP_LEVEL_IMPERSONATE,	// Default Impersonation
		NULL,							// Authentication info
		EOAC_NONE,						// Additional capabilities
		NULL							// Reserved
	);
	if (FAILED(hr))
		return;
#endif

	// create WBEM locator object
	com_ptr<IWbemLocator>	locator;
	locator.create<WbemLocator>();
	if (FAILED(hr))
		return;

	com_string	server = machine ? format_string("\\\\%s\\root\\cimv2", machine).begin() : "root\\cimv2";

	// connect with the specified machine
	com_ptr<IWbemServices> services;
//	hr = locator->ConnectServer(server, com_string(L"adrian"), com_string(L"cameron"), NULL, 0, NULL, NULL, &services);
	hr = locator->ConnectServer(server, NULL, NULL, NULL, 0, NULL, NULL, &services);
	if (FAILED(hr))
		return;

	// create enumerator
	com_ptr<IEnumWbemClassObject> enumerator;
	hr = services->CreateInstanceEnum(com_string(L"Win32_Process"), WBEM_FLAG_SHALLOW|WBEM_FLAG_FORWARD_ONLY, NULL, &enumerator);
	if (FAILED(hr))
		return;

	for (;;) {
		com_ptr<IWbemClassObject>	obj;
		ULONG						count;
		if (enumerator->Next(WBEM_INFINITE, 1, &obj, &count) != S_OK)
			break;

		new(processes) RemoteProcess(obj);
	}
}

//-----------------------------------------------------------------------------
//	local only
//-----------------------------------------------------------------------------

struct ISOProcessMemory : public Process, public ISO::VirtualDefaults {
	static const uint64 BLOCK_SIZE = 0x10000;
	void		*start;
	uint64		size;

	ISOProcessMemory(uint32 process_id, void *_start, uint64 _size) : Process(OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, process_id)), start(_start), size(_size) {}
	ISOProcessMemory(const triple<uint32, void*, uint64> &t)		: Process(OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, t.a)), start(t.b), size(t.c) {}

	ISOProcessMemory(const MODULEENTRY32 &m) : Process(OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, m.th32ProcessID)), start(m.modBaseAddr), size(m.modBaseSize) {}
	ISOProcessMemory(const HEAPENTRY32 &m) : Process(OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, m.th32ProcessID)), start((void*)m.dwAddress), size(m.dwBlockSize) {}

	uint32			Count() {
		return (size + BLOCK_SIZE - 1) / BLOCK_SIZE;
	}
	ISO::Browser2	Index(int i) {
		const uint8*	a	= (const uint8*)start + i * BLOCK_SIZE;
		uint32			len	= (uint32)min(size - i, BLOCK_SIZE);
		ISO_ptr<ISO_openarray<xint8> >	p(0, len);
		ReadMemory(*p, a, len);
		return p;
	}
};

ISO_DEFUSERVIRTX(ISOProcessMemory, "BigBin");


struct ISOModule {
	string				name, path;
	ISO::VStartBin<ISOProcessMemory>	mem;
	ISOModule(const MODULEENTRY32 &m) : name(m.szModule), path(m.szExePath), mem((uint64)m.modBaseAddr, m) {}
};

ISO_DEFUSERCOMPXV(ISOModule, "Module", name, path, mem);

struct ISOHeapEntry : ISO::VStartBin<ISOProcessMemory> {
	ISOHeapEntry(const HEAPENTRY32 &he) : VStartBin<ISOProcessMemory>(he.dwAddress, he) {}
};
ISO_DEFUSERX(ISOHeapEntry, VStartBin<ISOProcessMemory>, "HeapEntry");

struct ISOHeap : ISO::VirtualDefaults  {
	HEAPLIST32	hl;
	dynamic_array<ISOHeapEntry>	entries;
	ISOHeap(const HEAPLIST32 &_hl) : hl(_hl) {}
	void Init() {
		HEAPENTRY32 he;
		iso::clear(he);
		he.dwSize = sizeof(HEAPENTRY32);

		if (Heap32First(&he, hl.th32ProcessID, hl.th32HeapID)) {
			do
				entries.push_back(he);
			while (Heap32Next(&he));
		}
	}
	uint32			Count() {
		if (!entries)
			Init();
		return entries.size32();
	}
	ISO::Browser	Index(int i) {
		if (!entries)
			Init();
		return ISO::MakeBrowser(entries[i]);
	}
};

ISO_DEFUSERVIRTX(ISOHeap, "Heap");
//ISO_DEFUSERX(ISOHeap, dynamic_array<ISOHeapEntry>, "Heap");


struct ISOMemory : ISO::VStartBin<ISOProcessMemory>  {
	ISOMemory(uint32 id, uint8 *start, uint8 *end) : VStartBin<ISOProcessMemory>((uint64)start, make_triple(id, (void*)start, uint64(end - start))) {
	}
};
ISO_DEFUSERX(ISOMemory, VStartBin<ISOProcessMemory>, "Memory");

struct ISOProcess {
	int		id;
	int		parent_id;
	void	*image_base;
	string	cmdline, image_path, window_title;
	ModuleSnapshot				_snapshot;
	dynamic_array<ISOModule>	_modules;
	dynamic_array<ISOMemory>	_memory;

	ISOProcess(DWORD id) : id(id), parent_id(0), image_base(0), _snapshot(id, TH32CS_SNAPMODULE) {
		if (Process2 proc = id) {
			PROCESS_BASIC_INFORMATION pbi = proc.GetBasicInformation();
			parent_id		= pbi.ParentProcessId;
			NT::_PEB peb	= proc.Read(pbi.PebBaseAddress);
			//cmdline			= proc.ReadString(&proc.Get(&pbi.PebBaseAddress->ProcessParameters)->CommandLine);
			cmdline			= proc.ReadString(&peb.ProcessParameters->CommandLine);
			image_path		= proc.ReadString(&peb.ProcessParameters->ImagePathName);
			window_title	= proc.ReadString(&peb.ProcessParameters->WindowTitle);
			image_base		= peb.ImageBaseAddress;

/*			_modules	= _snapshot.modules();
			MEMORY_BASIC_INFORMATION info;
			for (uint8 *p = NULL, *start = NULL, *end = NULL; VirtualQueryEx(proc, p, &info, sizeof(info)) == sizeof(info); p += info.RegionSize) {
				if (info.State == MEM_COMMIT && info.Type == MEM_PRIVATE) {
					if (p != end) {
						if (start)
							new(_memory) ISOMemory(id, start, end);
						start = p;
					}
					end = p + info.RegionSize;
				}
			}
*/
		}
	}
	tag2 GetName() {
		if (window_title)
			return filename(window_title).name();
		if (image_path)
			return filename(image_path).name();
		return 0;
	}

	dynamic_array<ISOModule>	&modules() {
		if (!_modules)
			_modules	= _snapshot.modules();
		return _modules;
	}
	dynamic_array<ISOMemory>	&memory() {
		if (!_memory) {
			if (Process2 proc = id) {
				MEMORY_BASIC_INFORMATION info;
				for (uint8 *p = NULL, *start = NULL, *end = NULL; VirtualQueryEx(proc, p, &info, sizeof(info)) == sizeof(info); p += info.RegionSize) {
					if (info.State == MEM_COMMIT && info.Type == MEM_PRIVATE) {
						if (p != end) {
							if (start)
								new(_memory) ISOMemory(id, start, end);
							start = p;
						}
						end = p + info.RegionSize;
					}
				}
			}
		}
		return _memory;
	}

};

ISO_DEFUSERCOMPXV(ISOProcess, "Process", id, parent_id, cmdline, image_path, window_title, image_base, modules, memory);

class LocalProcesses : public ISO::VirtualDefaults {
	dynamic_array<ISOProcess>	processes;
public:
	LocalProcesses() {
		DWORD	ids[1024], needed;
		EnumProcesses(ids, sizeof(ids), &needed);
		uint32	num		= needed / sizeof(DWORD);
		
		processes = make_range_n(ids, num);

//		processes.reserve(num);
//		for (int i = 0; i < num; i++)
//			new(processes) ISOProcess(ids[i]);
	}
	int				Count()			{ return processes.size32();	}
	tag2			GetName(int i)	{ return processes[i].GetName(); }
	ISO::Browser2	Index(int i)	{ return ISO::MakeBrowser(processes[i]); }
};
ISO_DEFUSERVIRT(LocalProcesses);

//-----------------------------------------------------------------------------
//	ProcessesDevice
//-----------------------------------------------------------------------------

#include "device.h"

struct ProcessesDevice : app::DeviceT<ProcessesDevice>, app::DeviceCreateT<ProcessesDevice> {
	void			operator()(const app::DeviceAdd &add)	{ add("Processes", this, app::LoadPNG("IDB_DEVICE_PROCESSES")); }
//	ISO_ptr<void>	operator()(const win::Control &main)	{ return ISO_ptr<RemoteProcesses>("Processes", "192.168.2.202"); }
//	ISO_ptr<void>	operator()(const win::Control &main)	{ return ISO_ptr<RemoteProcesses>("Processes", "threadripper"); }
	ISO_ptr<void>	operator()(const win::Control &main)	{ return ISO_ptr<RemoteProcesses>("Processes", "inspiron7000"); }
//	ISO_ptr<void>	operator()(const win::Control &main)	{ return ISO_ptr<RemoteProcesses>("Processes", (const char*)0); }
//	ISO_ptr<void>	operator()(const win::Control &main)	{ return ISO_ptr<LocalProcesses>("Processes"); }
} processes_device;


//-----------------------------------------------------------------------------
//	WindowsDevice
//-----------------------------------------------------------------------------
struct ISOControl : win::Control {
	ISO_ptr<ISOProcess>	process;

	ISOControl(win::Control c, ISO_ptr<ISOProcess> process) : win::Control(c), process(process) {}

	auto		class_name()	const	{ return Class().name(); }
	int			thread_id()		const	{ return ThreadID(); }
	win::Rect	rect()			const	{ return GetRect(); }

	ISO_openarray<ISO_ptr<ISOControl> >	children() const {
		ISO_openarray<ISO_ptr<ISOControl> >	p;
		for (auto &&c : Children())
			p.Append(ISO_ptr<ISOControl>(tag2(c.GetText()), c, process));
		return p;
	}
};

template<> struct ISO::def<HWND>		: ISO::def<xint64> {};
template<> struct ISO::def<win::ID>		: ISO::def<xint64> {};

namespace ISO {
inline auto	MakeBrowser(HINSTANCE h)	{ return MakeBrowser(hex(uint64(h))); }
inline auto	MakeBrowser(WNDPROC h)		{ return MakeBrowser(hex(uint64(h))); }
}

ISO_DEFUSERENUMQV(win::Control::Style,
	OVERLAPPED,POPUP,CHILD,MINIMIZE,VISIBLE,DISABLED,CLIPSIBLINGS,CLIPCHILDREN,
	MAXIMIZE,CAPTION,BORDER,DLGFRAME,VSCROLL,HSCROLL,SYSMENU,THICKFRAME,
	GROUP,TABSTOP,MINIMIZEBOX,MAXIMIZEBOX,OVERLAPPEDWINDOW,POPUPWINDOW
);

ISO_DEFUSERENUMQV(win::Control::StyleEx,
	NOEX,DLGMODALFRAME,NOPARENTNOTIFY,TOPMOST,ACCEPTFILES,TRNSPARENT,MDICHILD,TOOLWINDOW,
	WINDOWEDGE, CLIENTEDGE,CONTEXTHELP,RIGHT,LEFT,RTLREADING,LTRREADING,LEFTSCROLLBAR,
	CONTROLPARENT,STATICEDGE,APPWINDOW,OVERLAPPEDWINDOWEX,PALETTEWINDOW,LAYERED,NOINHERITLAYOUT,NOREDIRECTIONBITMAP,
	LAYOUTRTL,COMPOSITED,NOACTIVATE
);

template<int I, typename T> struct ISO::def<win::Control::value<I, T> > : ISO::VirtualT2<win::Control::value<I, T> > {
	ISO::Browser2	Deref(win::Control::value<I, T> &a)		{ return ISO::MakeBrowser(a.get()); }
};

ISO_DEFUSERCOMPV(win::Rect, left,top,right,bottom);

ISO_DEFUSERCOMPXV(ISOControl, "Control", hWnd, class_name, process, thread_id, id, style, exstyle, winproc, hinstance, rect, children);

struct WindowsDevice : app::DeviceT<WindowsDevice>, app::DeviceCreateT<WindowsDevice> {
	void
		operator()(const app::DeviceAdd &add) {
		add("Windows", this, app::LoadPNG("IDB_DEVICE_WINDOWS"));
	}

	ISO_ptr<void>	operator()(const win::Control &main) {
		ISO_ptr<ISO_openarray<ISO_ptr<ISOControl> > >	p("Windows");
		hash_map<DWORD, ISO_ptr<ISOProcess>>			processes;

		win::enum_top_windows([&](win::ChildEnumerator *ce, win::Control c) {
			auto	proc		= c.ProcessID();
			if (!processes[proc].exists())
				processes[proc] = ISO_ptr<ISOProcess>("process", proc);

			p->Append(ISO_ptr<ISOControl>(tag2(c.GetText()), c, processes[proc]));
			return true;
		});

		return p;
	}
} windows_device;

#include "windows/text_control.h"

struct MessageName {uint16 msg; const char *name; };
MessageName	message_names[] = {
	{0x0000,"WM_NULL"},						{0x0001,"WM_CREATE"},				{0x0002,"WM_DESTROY"},					{0x0003,"WM_MOVE"},						{0x0005,"WM_SIZE"},
	{0x0006,"WM_ACTIVATE"},					{0x0007,"WM_SETFOCUS"},				{0x0008,"WM_KILLFOCUS"},				{0x0009,"WM_SETVISIBLE"},				{0x000A,"WM_ENABLE"},
	{0x000B,"WM_SETREDRAW"},				{0x000C,"WM_SETTEXT"},				{0x000D,"WM_GETTEXT"},					{0x000E,"WM_GETTEXTLENGTH"},			{0x000F,"WM_PAINT"},					{0x0010,"WM_CLOSE"},				{0x0011,"WM_QUERYENDSESSION"},
	{0x0012,"WM_QUIT"},						{0x0013,"WM_QUERYOPEN"},			{0x0014,"WM_ERASEBKGND"},				{0x0015,"WM_SYSCOLORCHANGE"},			{0x0016,"WM_ENDSESSION"},				{0x0017,"WM_SYSTEMERROR"},			{0x0018,"WM_SHOWWINDOW"},			{0x0019,"WM_CTLCOLOR"},
	{0x001A,"WM_SETTINGCHANGE"},			{0x001B,"WM_DEVMODECHANGE"},		{0x001C,"WM_ACTIVATEAPP"},				{0x001D,"WM_FONTCHANGE"},				{0x001E,"WM_TIMECHANGE"},				{0x001F,"WM_CANCELMODE"},			{0x0020,"WM_SETCURSOR"},			{0x0021,"WM_MOUSEACTIVATE"},
	{0x0022,"WM_CHILDACTIVATE"},			{0x0023,"WM_QUEUESYNC"},			{0x0024,"WM_GETMINMAXINFO"},			{0x0025,"WM_LOGOFF"},					{0x0026,"WM_PAINTICON"},				{0x0027,"WM_ICONERASEBKGND"},		{0x0028,"WM_NEXTDLGCTL"},			{0x0029,"WM_ALTTABACTIVE"},
	{0x002A,"WM_SPOOLERSTATUS"},			{0x002B,"WM_DRAWITEM"},				{0x002C,"WM_MEASUREITEM"},				{0x002D,"WM_DELETEITEM"},				{0x002E,"WM_VKEYTOITEM"},				{0x002F,"WM_CHARTOITEM"},			{0x0030,"WM_SETFONT"},				{0x0031,"WM_GETFONT"},
	{0x0032,"WM_SETHOTKEY"},				{0x0033,"WM_GETHOTKEY"},			{0x0034,"WM_FILESYSCHANGE"},			{0x0035,"WM_ISACTIVEICON"},				{0x0036,"WM_UNUSED0036"},				{0x0037,"WM_QUERYDRAGICON"},		{0x0038,"WM_WINHELP"},				{0x0039,"WM_COMPAREITEM"},
	{0x003A,"WM_FULLSCREEN"},				{0x003B,"WM_CLIENTSHUTDOWN"},		{0x003C,"WM_DDEMLEVENT"},				{0x0040,"WM_TESTING"},					{0x0041,"WM_COMPACTING"},				{0x0042,"WM_OTHERWINDOWCREATED"},	{0x0043,"WM_OTHERWINDOWDESTROYED"},
	{0x0044,"WM_COMMNOTIFY"},				{0x0045,"WM_MEDIASTATUSCHANGE"},	{0x0046,"WM_WINDOWPOSCHANGING"},		{0x0047,"WM_WINDOWPOSCHANGED"},			{0x0048,"WM_POWER"},					{0x0049,"WM_COPYGLOBALDATA"},		{0x004A,"WM_COPYDATA"},				{0x004B,"WM_CANCELJOURNAL"},
	{0x004C,"WM_LOGONNOTIFY"},				{0x004D,"WM_KEYF1"},				{0x004E,"WM_NOTIFY"},					{0x004F,"WM_ACCESS_WINDOW"},			{0x0050,"WM_INPUTLANGCHANGEREQUEST"},	{0x0051,"WM_INPUTLANGCHANGE"},		{0x0052,"WM_TCARD"},				{0x0053,"WM_HELP"},
	{0x0054,"WM_USERCHANGED"},				{0x0055,"WM_NOTIFYFORMAT"},			{0x0070,"WM_FINALDESTROY"},				{0x0071,"WM_MEASUREITEM_CLIENTDATA"},	{0x007B,"WM_CONTEXTMENU"},				{0x007C,"WM_STYLECHANGING"},		{0x007D,"WM_STYLECHANGED"},			{0x007E,"WM_DISPLAYCHANGE"},
	{0x007F,"WM_GETICON"},					{0x0080,"WM_SETICON"},				{0x0081,"WM_NCCREATE"},					{0x0082,"WM_NCDESTROY"},				{0x0083,"WM_NCCALCSIZE"},				{0x0084,"WM_NCHITTEST"},			{0x0085,"WM_NCPAINT"},				{0x0086,"WM_NCACTIVATE"},
	{0x0087,"WM_GETDLGCODE"},				{0x0088,"WM_SYNCPAINT"},			{0x0089,"WM_SYNCTASK"},					{0x008B,"WM_MYSTERY"},					{0x00A0,"WM_NCMOUSEMOVE"},				{0x00A1,"WM_NCLBUTTONDOWN"},		{0x00A2,"WM_NCLBUTTONUP"},			{0x00A3,"WM_NCLBUTTONDBLCLK"},
	{0x00A4,"WM_NCRBUTTONDOWN"},			{0x00A5,"WM_NCRBUTTONUP"},			{0x00A6,"WM_NCRBUTTONDBLCLK"},			{0x00A7,"WM_NCMBUTTONDOWN"},			{0x00A8,"WM_NCMBUTTONUP"},				{0x00A9,"WM_NCMBUTTONDBLCLK"},		{0x00AB,"WM_NCXBUTTONDOWN"},		{0x00AC,"WM_NCXBUTTONUP"},
	{0x00AD,"WM_NCXBUTTONDBLCLK"},

	{0x00B0,"EM_GETSEL"},					{0x00B1,"EM_SETSEL"},				{0x00B2,"EM_GETRECT"},					{0x00B3,"EM_SETRECT"},					{0x00B4,"EM_SETRECTNP"},				{0x00B5,"EM_SCROLL"},				{0x00B6,"EM_LINESCROLL"},
	{0x00B7,"EM_SCROLLCARET"},				{0x00B8,"EM_GETMODIFY"},			{0x00B9,"EM_SETMODIFY"},				{0x00BA,"EM_GETLINECOUNT"},				{0x00BB,"EM_LINEINDEX"},				{0x00BC,"EM_SETHANDLE"},			{0x00BD,"EM_GETHANDLE"},			{0x00BE,"EM_GETTHUMB"},
	{0x00C1,"EM_LINELENGTH"},				{0x00C2,"EM_REPLACESEL"},			{0x00C4,"EM_GETLINE"},					{0x00C5,"EM_SETLIMITTEXT"},				{0x00C6,"EM_CANUNDO"},					{0x00C7,"EM_UNDO"},					{0x00C8,"EM_FMTLINES"},				{0x00C9,"EM_LINEFROMCHAR"},
	{0x00CB,"EM_SETTABSTOPS"},				{0x00CC,"EM_SETPASSWORDCHAR"},		{0x00CD,"EM_EMPTYUNDOBUFFER"},			{0x00CE,"EM_GETFIRSTVISIBLELINE"},		{0x00CF,"EM_SETREADONLY"},				{0x00D0,"EM_SETWORDBREAKPROC"},		{0x00D1,"EM_GETWORDBREAKPROC"},		{0x00D2,"EM_GETPASSWORDCHAR"},
	{0x00D3,"EM_SETMARGINS"},				{0x00D4,"EM_GETMARGINS"},			{0x00D5,"EM_GETLIMITTEXT"},				{0x00D6,"EM_POSFROMCHAR"},				{0x00D7,"EM_CHARFROMPOS"},				{0x00D8,"EM_SETIMESTATUS"},			{0x00D9,"EM_GETIMESTATUS"},

	{0x00E0,"SBM_SETPOS"},					{0x00E1,"SBM_GETPOS"},				{0x00E2,"SBM_SETRANGE"},				{0x00E3,"SBM_GETRANGE"},				{0x00E4,"SBM_ENABLE_ARROWS"},			{0x00E6,"SBM_SETRANGEREDRAW"},		{0x00E9,"SBM_SETSCROLLINFO"},		{0x00EA,"SBM_GETSCROLLINFO"},

	{0x00F0,"BM_GETCHECK"},					{0x00F1,"BM_SETCHECK"},				{0x00F2,"BM_GETSTATE"},					{0x00F3,"BM_SETSTATE"},					{0x00F4,"BM_SETSTYLE"},					{0x00F5,"BM_CLICK"},				{0x00F6,"BM_GETIMAGE"},				{0x00F7,"BM_SETIMAGE"},

	{0x00FF,"WM_INPUT"},
	{0x0100,"WM_KEYDOWN"},					{0x0101,"WM_KEYUP"},				{0x0102,"WM_CHAR"},						{0x0103,"WM_DEADCHAR"},					{0x0104,"WM_SYSKEYDOWN"},				{0x0105,"WM_SYSKEYUP"},				{0x0106,"WM_SYSCHAR"},				{0x0107,"WM_SYSDEADCHAR"},
	{0x0108,"WM_YOMICHAR"},					{0x010A,"WM_CONVERTREQUEST"},		{0x010B,"WM_CONVERTRESULT"},			{0x010C,"WM_INTERIM"},					{0x010D,"WM_IME_STARTCOMPOSITION"},		{0x010E,"WM_IME_ENDCOMPOSITION"},	{0x010F,"WM_IME_COMPOSITION"},		{0x0110,"WM_INITDIALOG"},
	{0x0111,"WM_COMMAND"},					{0x0112,"WM_SYSCOMMAND"},			{0x0113,"WM_TIMER"},					{0x0114,"WM_HSCROLL"},					{0x0115,"WM_VSCROLL"},					{0x0116,"WM_INITMENU"},				{0x0117,"WM_INITMENUPOPUP"},		{0x0118,"WM_SYSTIMER"},
	{0x011F,"WM_MENUSELECT"},				{0x0120,"WM_MENUCHAR"},				{0x0121,"WM_ENTERIDLE"},				{0x0122,"WM_MENURBUTTONUP"},			{0x0123,"WM_MENUDRAG"},					{0x0124,"WM_MENUGETOBJECT"},		{0x0125,"WM_UNINITMENUPOPUP"},		{0x0126,"WM_MENUCOMMAND"},
	{0x0127,"WM_CHANGEUISTATE"},			{0x0128,"WM_UPDATEUISTATE"},		{0x0129,"WM_QUERYUISTATE"},				{0x0132,"WM_CTLCOLORMSGBOX"},			{0x0133,"WM_CTLCOLOREDIT"},				{0x0134,"WM_CTLCOLORLISTBOX"},		{0x0135,"WM_CTLCOLORBTN"},			{0x0136,"WM_CTLCOLORDLG"},
	{0x0137,"WM_CTLCOLORSCROLLBAR"},		{0x0138,"WM_CTLCOLORSTATIC"},

	{0x0140,"CB_GETEDITSEL"},				{0x0141,"CB_LIMITTEXT"},			{0x0142,"CB_SETEDITSEL"},				{0x0143,"CB_ADDSTRING"},				{0x0144,"CB_DELETESTRING"},				{0x0145,"CB_DIR"},
	{0x0146,"CB_GETCOUNT"},					{0x0147,"CB_GETCURSEL"},			{0x0148,"CB_GETLBTEXT"},				{0x0149,"CB_GETLBTEXTLEN"},				{0x014A,"CB_INSERTSTRING"},				{0x014B,"CB_RESETCONTENT"},			{0x014C,"CB_FINDSTRING"},			{0x014D,"CB_SELECTSTRING"},
	{0x014E,"CB_SETCURSEL"},				{0x014F,"CB_SHOWDROPDOWN"},			{0x0150,"CB_GETITEMDATA"},				{0x0151,"CB_SETITEMDATA"},				{0x0152,"CB_GETDROPPEDCONTROLRECT"},	{0x0153,"CB_SETITEMHEIGHT"},		{0x0154,"CB_GETITEMHEIGHT"},		{0x0155,"CB_SETEXTENDEDUI"},
	{0x0156,"CB_GETEXTENDEDUI"},			{0x0157,"CB_GETDROPPEDSTATE"},		{0x0158,"CB_FINDSTRINGEXACT"},			{0x0159,"CB_SETLOCALE"},				{0x015A,"CB_GETLOCALE"},				{0x015B,"CB_GETTOPINDEX"},			{0x015C,"CB_SETTOPINDEX"},			{0x015D,"CB_GETHORIZONTALEXTENT"},
	{0x015E,"CB_SETHORIZONTALEXTENT"},		{0x015F,"CB_GETDROPPEDWIDTH"},		{0x0160,"CB_SETDROPPEDWIDTH"},			{0x0161,"CB_INITSTORAGE"},

	{0x0170,"STM_SETICON"},					{0x0171,"STM_GETICON"},				{0x0172,"STM_SETIMAGE"},				{0x0173,"STM_GETIMAGE"},

	{0x0180,"LB_ADDSTRING"},				{0x0181,"LB_INSERTSTRING"},			{0x0182,"LB_DELETESTRING"},				{0x0183,"LB_SELITEMRANGEEX"},			{0x0184,"LB_RESETCONTENT"},				{0x0185,"LB_SETSEL"},				{0x0186,"LB_SETCURSEL"},			{0x0187,"LB_GETSEL"},
	{0x0188,"LB_GETCURSEL"},				{0x0189,"LB_GETTEXT"},				{0x018A,"LB_GETTEXTLEN"},				{0x018B,"LB_GETCOUNT"},					{0x018C,"LB_SELECTSTRING"},				{0x018D,"LB_DIR"},					{0x018E,"LB_GETTOPINDEX"},			{0x018F,"LB_FINDSTRING"},
	{0x0190,"LB_GETSELCOUNT"},				{0x0191,"LB_GETSELITEMS"},			{0x0192,"LB_SETTABSTOPS"},				{0x0193,"LB_GETHORIZONTALEXTENT"},		{0x0194,"LB_SETHORIZONTALEXTENT"},		{0x0195,"LB_SETCOLUMNWIDTH"},		{0x0196,"LB_ADDFILE"},				{0x0197,"LB_SETTOPINDEX"},
	{0x0198,"LB_GETITEMRECT"},				{0x0199,"LB_GETITEMDATA"},			{0x019A,"LB_SETITEMDATA"},				{0x019B,"LB_SELITEMRANGE"},				{0x019C,"LB_SETANCHORINDEX"},			{0x019D,"LB_GETANCHORINDEX"},		{0x019E,"LB_SETCARETINDEX"},		{0x019F,"LB_GETCARETINDEX"},
	{0x01A0,"LB_SETITEMHEIGHT"},			{0x01A1,"LB_GETITEMHEIGHT"},		{0x01A2,"LB_FINDSTRINGEXACT"},			{0x01A3,"LBCB_CARETON"},				{0x01A4,"LBCB_CARETOFF"},				{0x01A5,"LB_SETLOCALE"},			{0x01A6,"LB_GETLOCALE"},			{0x01A7,"LB_SETCOUNT"},
	{0x01A8,"LB_INITSTORAGE"},				{0x01A9,"LB_ITEMFROMPOINT"},		{0x01AA,"LB_INSERTSTRINGUPPER"},		{0x01AB,"LB_INSERTSTRINGLOWER"},		{0x01AC,"LB_ADDSTRINGUPPER"},			{0x01AD,"LB_ADDSTRINGLOWER"},

	{0x01E0,"MN_SETHMENU"},					{0x01E1,"MN_GETHMENU"},
	{0x01E2,"MN_SIZEWINDOW"},				{0x01E3,"MN_OPENHIERARCHY"},		{0x01E4,"MN_CLOSEHIERARCHY"},			{0x01E5,"MN_SELECTITEM"},				{0x01E6,"MN_CANCELMENUS"},				{0x01E7,"MN_SELECTFIRSTVALIDITEM"},	{0x01EA,"MN_GETPPOPUPMENU"},		{0x01EB,"MN_FINDMENUWINDOWFROMPOINT"},
	{0x01EC,"MN_SHOWPOPUPWINDOW"},			{0x01ED,"MN_BUTTONDOWN"},			{0x01EE,"MN_MOUSEMOVE"},				{0x01EF,"MN_BUTTONUP"},					{0x01F0,"MN_SETTIMERTOOPENHIERARCHY"},	{0x01F1,"MN_DBLCLK"},

	{0x0200,"WM_MOUSEMOVE"},				{0x0201,"WM_LBUTTONDOWN"},
	{0x0202,"WM_LBUTTONUP"},				{0x0203,"WM_LBUTTONDBLCLK"},		{0x0204,"WM_RBUTTONDOWN"},				{0x0205,"WM_RBUTTONUP"},				{0x0206,"WM_RBUTTONDBLCLK"},			{0x0207,"WM_MBUTTONDOWN"},			{0x0208,"WM_MBUTTONUP"},			{0x0209,"WM_MBUTTONDBLCLK"},
	{0x020A,"WM_MOUSEWHEEL"},				{0x020B,"WM_XBUTTONDOWN"},			{0x020C,"WM_XBUTTONUP"},				{0x020D,"WM_XBUTTONDBLCLK"},			{0x0210,"WM_PARENTNOTIFY"},				{0x0211,"WM_ENTERMENULOOP"},		{0x0212,"WM_EXITMENULOOP"},			{0x0213,"WM_NEXTMENU"},
	{0x0214,"WM_SIZING"},					{0x0215,"WM_CAPTURECHANGED"},		{0x0216,"WM_MOVING"},					{0x0218,"WM_POWERBROADCAST"},			{0x0219,"WM_DEVICECHANGE"},				{0x0220,"WM_MDICREATE"},			{0x0221,"WM_MDIDESTROY"},			{0x0222,"WM_MDIACTIVATE"},
	{0x0223,"WM_MDIRESTORE"},				{0x0224,"WM_MDINEXT"},				{0x0225,"WM_MDIMAXIMIZE"},				{0x0226,"WM_MDITILE"},					{0x0227,"WM_MDICASCADE"},				{0x0228,"WM_MDIICONARRANGE"},		{0x0229,"WM_MDIGETACTIVE"},			{0x022A,"WM_DROPOBJECT"},
	{0x022B,"WM_QUERYDROPOBJECT"},			{0x022C,"WM_BEGINDRAG"},			{0x022D,"WM_DRAGLOOP"},					{0x022E,"WM_DRAGSELECT"},				{0x022F,"WM_DRAGMOVE"},					{0x0230,"WM_MDISETMENU"},			{0x0231,"WM_ENTERSIZEMOVE"},		{0x0232,"WM_EXITSIZEMOVE"},
	{0x0233,"WM_DROPFILES"},				{0x0234,"WM_MDIREFRESHMENU"},		{0x0281,"WM_IME_SETCONTEXT"},			{0x0282,"WM_IME_NOTIFY"},				{0x0283,"WM_IME_CONTROL"},				{0x0284,"WM_IME_COMPOSITIONFULL"},	{0x0285,"WM_IME_SELECT"},			{0x0286,"WM_IME_CHAR"},
	{0x0288,"WM_IME_REQUEST"},				{0x0290,"WM_IME_KEYDOWN"},			{0x0291,"WM_IME_KEYUP"},				{0x02A0,"WM_NCMOUSEHOVER"},				{0x02A1,"WM_MOUSEHOVER"},				{0x02A2,"WM_NCMOUSELEAVE"},			{0x02A3,"WM_MOUSELEAVE"},			{0x0300,"WM_CUT"},
	{0x0301,"WM_COPY"},						{0x0302,"WM_PASTE"},				{0x0303,"WM_CLEAR"},					{0x0304,"WM_UNDO"},						{0x0305,"WM_RENDERFORMAT"},				{0x0306,"WM_RENDERALLFORMATS"},		{0x0307,"WM_DESTROYCLIPBOARD"},		{0x0308,"WM_DRAWCLIPBOARD"},
	{0x0309,"WM_PAINTCLIPBOARD"},			{0x030A,"WM_VSCROLLCLIPBOARD"},		{0x030B,"WM_SIZECLIPBOARD"},			{0x030C,"WM_ASKCBFORMATNAME"},			{0x030D,"WM_CHANGECBCHAIN"},			{0x030E,"WM_HSCROLLCLIPBOARD"},		{0x030F,"WM_QUERYNEWPALETTE"},		{0x0310,"WM_PALETTEISCHANGING"},
	{0x0311,"WM_PALETTECHANGED"},			{0x0312,"WM_HOTKEY"},				{0x0313,"WM_SYSMENU"},					{0x0314,"WM_HOOKMSG"},					{0x0315,"WM_EXITPROCESS"},				{0x0316,"WM_WAKETHREAD"},			{0x0317,"WM_PRINT"},				{0x0318,"WM_PRINTCLIENT"},
	{0x0319,"WM_APPCOMMAND"},				{0x0380,"WM_PENWINFIRST"},			{0x038F,"WM_PENWINLAST"},				{0x03E0,"WM_DDE_INITIATE"},				{0x03E1,"WM_DDE_TERMINATE"},			{0x03E2,"WM_DDE_ADVISE"},			{0x03E3,"WM_DDE_UNADVISE"},			{0x03E4,"WM_DDE_ACK"},
	{0x03E5,"WM_DDE_DATA"},					{0x03E6,"WM_DDE_REQUEST"},			{0x03E7,"WM_DDE_POKE"},					{0x03E8,"WM_DDE_EXECUTE"},

	{0x0400,"WM_USER"},
	{0x0400,"TBM_GETPOS"},					{0x0400,"DM_GETDEFID"},
	{0x0401,"WM_CHOOSEFONT_GETLOGFONT"},	{0x0401,"DM_SETDEFID"},				{0x0401,"HKM_SETHOTKEY"},				{0x0401,"SB_SETTEXTA"},				{0x0401,"PBM_SETRANGE"},			{0x0401,"TTM_ACTIVATE"},			{0x0401,"TB_ENABLEBUTTON"},			{0x0401,"RB_INSERTBANDA"},				{0x0401,"TBM_GETRANGEMIN"},			{0x0401,"CBEM_INSERTITEMA"},			{0x0401,"RB_INSERTBAND"},
	{0x0402,"DM_REPOSITION"},				{0x0402,"TB_CHECKBUTTON"},			{0x0402,"SB_GETTEXTA"},					{0x0402,"TBM_GETRANGEMAX"},			{0x0402,"CBEM_SETIMAGELIST"},		{0x0402,"PBM_SETPOS"},				{0x0402,"HKM_GETHOTKEY"},			{0x0402,"RB_DELETEBAND"},
	{0x0403,"TTM_SETDELAYTIME"},			{0x0403,"PBM_DELTAPOS"},			{0x0403,"CBEM_GETIMAGELIST"},			{0x0403,"TBM_GETTIC"},				{0x0403,"RB_GETBARINFO"},			{0x0403,"HKM_SETRULES"},			{0x0403,"SB_GETTEXTLENGTHA"},		{0x0403,"TB_PRESSBUTTON"},
	{0x0404,"PBM_SETSTEP"},					{0x0404,"SB_SETPARTS"},				{0x0404,"TTM_ADDTOOLA"},				{0x0404,"TB_HIDEBUTTON"},			{0x0404,"CBEM_GETITEMA"},			{0x0404,"TBM_SETTIC"},				{0x0404,"RB_SETBARINFO"},
	{0x0405,"TBM_SETPOS"},					{0x0405,"RB_GETBANDINFO"},			{0x0405,"TTM_DELTOOLA"},				{0x0405,"TB_INDETERMINATE"},		{0x0405,"CBEM_SETITEMA"},			{0x0405,"PBM_STEPIT"},
	{0x0406,"TB_MARKBUTTON"},				{0x0406,"PBM_SETRANGE32"},			{0x0406,"RB_SETBANDINFO"},				{0x0406,"TBM_SETRANGE"},			{0x0406,"CBEM_GETCOMBOCONTROL"},	{0x0406,"SB_GETPARTS"},				{0x0406,"RB_SETBANDINFOA"},			{0x0406,"TTM_NEWTOOLRECTA"},
	{0x0407,"RB_SETPARENT"},				{0x0407,"TBM_SETRANGEMIN"},			{0x0407,"TTM_RELAYEVENT"},				{0x0407,"CBEM_GETEDITCONTROL"},		{0x0407,"PBM_GETRANGE"},			{0x0407,"SB_GETBORDERS"},
	{0x0408,"TBM_SETRANGEMAX"},				{0x0408,"PBM_GETPOS"},				{0x0408,"RB_HITTEST"},					{0x0408,"TTM_GETTOOLINFOA"},		{0x0408,"CBEM_SETEXSTYLE"},			{0x0408,"SB_SETMINHEIGHT"},
	{0x0409,"CBEM_GETEXSTYLE"},				{0x0409,"CBEM_GETEXTENDEDSTYLE"},	{0x0409,"TB_ISBUTTONENABLED"},			{0x0409,"PBM_SETBARCOLOR"},			{0x0409,"RB_GETRECT"},				{0x0409,"TTM_SETTOOLINFOA"},		{0x0409,"TBM_CLEARTICS"},			{0x0409,"SB_SIMPLE"},
	{0x040A,"CBEM_HASEDITCHANGED"},			{0x040A,"TTM_HITTESTA"},			{0x040A,"RB_INSERTBANDW"},				{0x040A,"SB_GETRECT"},				{0x040A,"TBM_SETSEL"},				{0x040A,"TB_ISBUTTONCHECKED"},
	{0x040B,"SB_SETTEXTW"},					{0x040B,"TTM_GETTEXTA"},			{0x040B,"RB_SETBANDINFOW"},				{0x040B,"TB_ISBUTTONPRESSED"},		{0x040B,"TBM_SETSELSTART"},			{0x040B,"CBEM_INSERTITEMW"},
	{0x040C,"TB_ISBUTTONHIDDEN"},			{0x040C,"RB_GETBANDCOUNT"},			{0x040C,"SB_GETTEXTLENGTHW"},			{0x040C,"TBM_SETSELEND"},			{0x040C,"TTM_UPDATETIPTEXTA"},		{0x040C,"CBEM_SETITEMW"},
	{0x040D,"SB_GETTEXTW"},					{0x040D,"CBEM_GETITEMW"},			{0x040D,"TB_ISBUTTONINDETERMINATE"},	{0x040D,"RB_GETROWCOUNT"},			{0x040D,"TTM_GETTOOLCOUNT"},
	{0x040E,"TB_ISBUTTONHIGHLIGHTED"},		{0x040E,"CBEM_SETEXTENDEDSTYLE"},	{0x040E,"TTM_ENUMTOOLSA"},				{0x040E,"TBM_GETPTICS"},			{0x040E,"SB_ISSIMPLE"},				{0x040E,"RB_GETROWHEIGHT"},
	{0x040F,"TBM_GETTICPOS"},				{0x040F,"TTM_GETCURRENTTOOLA"},		{0x040F,"SB_SETICON"},
	{0x0410,"RB_IDTOINDEX"},				{0x0410,"SB_SETTIPTEXTA"},			{0x0410,"TBM_GETNUMTICS"},				{0x0410,"TTM_WINDOWFROMPOINT"},
	{0x0411,"TB_SETSTATE"},					{0x0411,"SB_SETTIPTEXTW"},			{0x0411,"RB_GETTOOLTIPS"},				{0x0411,"TBM_GETSELSTART"},			{0x0411,"TTM_TRACKACTIVATE"},
	{0x0412,"RB_SETTOOLTIPS"},				{0x0412,"SB_GETTIPTEXTA"},			{0x0412,"TB_GETSTATE"},					{0x0412,"TTM_TRACKPOSITION"},		{0x0412,"TBM_GETSELEND"},
	{0x0413,"TTM_SETTIPBKCOLOR"},			{0x0413,"TBM_CLEARSEL"},			{0x0413,"TB_ADDBITMAP"},				{0x0413,"RB_SETBKCOLOR"},			{0x0413,"SB_GETTIPTEXTW"},
	{0x0414,"SB_GETICON"},					{0x0414,"RB_GETBKCOLOR"},			{0x0414,"TTM_SETTIPTEXTCOLOR"},			{0x0414,"TBM_SETTICFREQ"},			{0x0414,"TB_ADDBUTTONS"},
	{0x0415,"TBM_SETPAGESIZE"},				{0x0415,"TTM_GETDELAYTIME"},		{0x0415,"TB_INSERTBUTTON"},				{0x0415,"RB_SETTEXTCOLOR"},
	{0x0416,"TB_DELETEBUTTON"},				{0x0416,"TTM_GETTIPBKCOLOR"},		{0x0416,"RB_GETTEXTCOLOR"},				{0x0416,"TBM_GETPAGESIZE"},
	{0x0417,"RB_SIZETORECT"},				{0x0417,"TTM_GETTIPTEXTCOLOR"},		{0x0417,"TB_GETBUTTON"},				{0x0417,"TBM_SETLINESIZE"},
	{0x0418,"TTM_SETMAXTIPWIDTH"},			{0x0418,"TBM_GETLINESIZE"},			{0x0418,"RB_BEGINDRAG"},				{0x0418,"TB_BUTTONCOUNT"},
	{0x0419,"RB_ENDDRAG"},					{0x0419,"TB_COMMANDTOINDEX"},		{0x0419,"TBM_GETTHUMBRECT"},			{0x0419,"TTM_GETMAXTIPWIDTH"},
	{0x041A,"TB_SAVERESTORE"},				{0x041A,"TTM_SETMARGIN"},			{0x041A,"TBM_GETCHANNELRECT"},			{0x041A,"RB_DRAGMOVE"},
	{0x041B,"TTM_GETMARGIN"},				{0x041B,"RB_GETBARHEIGHT"},			{0x041B,"TB_CUSTOMIZE"},				{0x041B,"TBM_SETTHUMBLENGTH"},
	{0x041C,"RB_GETBANDINFOW"},				{0x041C,"TBM_GETTHUMBLENGTH"},		{0x041C,"TB_ADDSTRING"},				{0x041C,"TTM_POP"},
	{0x041D,"TBM_SETTOOLTIPS"},				{0x041D,"RB_GETBANDINFOA"},			{0x041D,"TTM_UPDATE"},					{0x041D,"TB_GETITEMRECT"},
	{0x041E,"TBM_GETTOOLTIPS"},				{0x041E,"RB_MINIMIZEBAND"},			{0x041E,"TTM_GETBUBBLESIZE"},			{0x041E,"TB_BUTTONSTRUCTSIZE"},
	{0x041F,"RB_MAXIMIZEBAND"},				{0x041F,"TTM_ADJUSTRECT"},			{0x041F,"TB_SETBUTTONSIZE"},			{0x041F,"TBM_SETTIPSIDE"},
	{0x0420,"TB_SETBITMAPSIZE"},			{0x0420,"TTM_SETTITLEA"},			{0x0420,"TBM_SETBUDDY"},
	{0x0421,"TB_AUTOSIZE"},					{0x0421,"TTM_SETTITLEW"},			{0x0421,"TBM_GETBUDDY"},
	{0x0422,"RB_GETBANDBORDERS"},
	{0x0423,"TB_GETTOOLTIPS"},				{0x0423,"RB_SHOWBAND"},
	{0x0424,"TB_SETTOOLTIPS"},
	{0x0425,"RB_SETPALETTE"},				{0x0425,"TB_SETPARENT"},
	{0x0426,"RB_GETPALETTE"},
	{0x0427,"RB_MOVEBAND"},					{0x0427,"TB_SETROWS"},
	{0x0428,"TB_GETROWS"},
	{0x0429,"TB_GETBITMAPFLAGS"},
	{0x042A,"TB_SETCMDID"},
	{0x042B,"RB_PUSHCHEVRON"},				{0x042B,"TB_CHANGEBITMAP"},
	{0x042C,"TB_GETBITMAP"},
	{0x042D,"TB_GETBUTTONTEXT"},			{0x042E,"TB_REPLACEBITMAP"},		{0x042F,"TB_SETINDENT"},				{0x0430,"TB_SETIMAGELIST"},				{0x0431,"TB_GETIMAGELIST"},

	{0x0432,"TTM_ADDTOOLW"},				{0x0432,"TB_LOADIMAGES"},			{0x0432,"EM_CANPASTE"},					{0x0433,"EM_DISPLAYBAND"},				{0x0433,"TTM_DELTOOLW"},				{0x0433,"TB_GETRECT"},				{0x0434,"TTM_NEWTOOLRECTW"},		{0x0434,"EM_EXGETSEL"},
	{0x0434,"TB_SETHOTIMAGELIST"},			{0x0435,"TTM_GETTOOLINFOW"},		{0x0435,"EM_EXLIMITTEXT"},				{0x0435,"TB_GETHOTIMAGELIST"},			{0x0436,"TB_SETDISABLEDIMAGELIST"},		{0x0436,"EM_EXLINEFROMCHAR"},		{0x0436,"TTM_SETTOOLINFOW"},		{0x0437,"TB_GETDISABLEDIMAGELIST"},
	{0x0437,"TTM_HITTESTW"},				{0x0437,"EM_EXSETSEL"},				{0x0438,"TB_SETSTYLE"},					{0x0438,"TTM_GETTEXTW"},				{0x0438,"EM_FINDTEXT"},					{0x0439,"EM_FORMATRANGE"},			{0x0439,"TTM_UPDATETIPTEXTW"},		{0x0439,"TB_GETSTYLE"},
	{0x043A,"EM_GETCHARFORMAT"},			{0x043A,"TB_GETBUTTONSIZE"},		{0x043A,"TTM_ENUMTOOLSW"},				{0x043B,"EM_GETEVENTMASK"},				{0x043B,"TB_SETBUTTONWIDTH"},			{0x043B,"TTM_GETCURRENTTOOLW"},		{0x043C,"EM_GETOLEINTERFACE"},		{0x043C,"TB_SETMAXTEXTROWS"},
	{0x043D,"TB_GETTEXTROWS"},				{0x043D,"EM_GETPARAFORMAT"},		{0x043E,"TB_GETOBJECT"},				{0x043E,"EM_GETSELTEXT"},				{0x043F,"EM_HIDESELECTION"},			{0x043F,"TB_GETBUTTONINFOW"},		{0x0440,"EM_PASTESPECIAL"},			{0x0440,"TB_SETBUTTONINFOW"},
	{0x0441,"EM_REQUESTRESIZE"},			{0x0441,"TB_GETBUTTONINFOA"},		{0x0442,"TB_SETBUTTONINFOA"},			{0x0442,"EM_SELECTIONTYPE"},			{0x0443,"EM_SETBKGNDCOLOR"},			{0x0444,"EM_SETCHARFORMAT"},		{0x0445,"TB_HITTEST"},				{0x0445,"EM_SETEVENTMASK"},
	{0x0446,"EM_SETOLECALLBACK"},			{0x0446,"TB_SETDRAWTEXTFLAGS"},		{0x0447,"TB_GETHOTITEM"},				{0x0447,"EM_SETPARAFORMAT"},			{0x0448,"EM_SETTARGETDEVICE"},			{0x0448,"TB_SETHOTITEM"},			{0x0449,"TB_SETANCHORHIGHLIGHT"},	{0x0449,"EM_STREAMIN"},
	{0x044A,"EM_STREAMOUT"},				{0x044A,"TB_GETANCHORHIGHLIGHT"},	{0x044B,"EM_GETTEXTRANGE"},				{0x044C,"EM_FINDWORDBREAK"},			{0x044D,"EM_SETOPTIONS"},				{0x044E,"EM_GETOPTIONS"},			{0x044E,"TB_MAPACCELERATORA"},		{0x044F,"EM_FINDTEXTEX"},
	{0x044F,"TB_GETINSERTMARK"},			{0x0450,"TB_SETINSERTMARK"},		{0x0450,"EM_GETWORDBREAKPROCEX"},		{0x0451,"TB_INSERTMARKHITTEST"},		{0x0451,"EM_SETWORDBREAKPROCEX"},		{0x0452,"TB_MOVEBUTTON"},			{0x0452,"EM_SETUNDOLIMIT"},			{0x0453,"TB_GETMAXSIZE"},
	{0x0454,"EM_REDO"},						{0x0454,"TB_SETEXTENDEDSTYLE"},		{0x0455,"TB_GETEXTENDEDSTYLE"},			{0x0455,"EM_CANREDO"},					{0x0456,"TB_GETPADDING"},				{0x0456,"EM_GETUNDONAME"},			{0x0457,"TB_SETPADDING"},			{0x0457,"EM_GETREDONAME"},
	{0x0458,"TB_SETINSERTMARKCOLOR"},		{0x0458,"EM_STOPGROUPTYPING"},		{0x0459,"TB_GETINSERTMARKCOLOR"},		{0x0459,"EM_SETTEXTMODE"},				{0x045A,"TB_MAPACCELERATORW"},			{0x045A,"EM_GETTEXTMODE"},			{0x045B,"EM_AUTOURLDETECT"},		{0x045C,"TB_GETSTRING"},
	{0x045C,"EM_GETAUTOURLDETECT"},			{0x045D,"EM_SETPALETTE"},			{0x045E,"EM_GETTEXTEX"},				{0x045F,"EM_GETTEXTLENGTHEX"},			{0x0460,"EM_SHOWSCROLLBAR"},			{0x0461,"EM_SETTEXTEX"},			{0x0464,"EM_SETPUNCTUATION"},		{0x0464,"IPM_CLEARADDRESS"},
	{0x0464,"CDM_GETSPEC"},					{0x0464,"ACM_OPEN"},				{0x0465,"WM_CHOOSEFONT_SETLOGFONT"},	{0x0465,"CDM_GETFILEPATH"},				{0x0465,"PSM_SETCURSEL"},				{0x0465,"UDM_SETRANGE"},			{0x0465,"ACM_PLAY"},				{0x0465,"EM_GETPUNCTUATION"},
	{0x0465,"IPM_SETADDRESS"},				{0x0466,"PSM_REMOVEPAGE"},			{0x0466,"EM_SETWORDWRAPMODE"},			{0x0466,"ACM_STOP"},					{0x0466,"UDM_GETRANGE"},				{0x0466,"CDM_GETFOLDERPATH"},		{0x0466,"WM_CHOOSEFONT_SETFLAGS"},	{0x0466,"IPM_GETADDRESS"},
	{0x0467,"EM_GETWORDWRAPMODE"},			{0x0467,"CDM_GETFOLDERIDLIST"},		{0x0467,"IPM_SETRANGE"},				{0x0467,"PSM_ADDPAGE"},					{0x0467,"UDM_SETPOS"},					{0x0468,"CDM_SETCONTROLTEXT"},		{0x0468,"PSM_CHANGED"},				{0x0468,"UDM_GETPOS"},
	{0x0468,"IPM_SETFOCUS"},				{0x0468,"EM_SETIMECOLOR"},			{0x0469,"UDM_SETBUDDY"},				{0x0469,"IPM_ISBLANK"},					{0x0469,"PSM_RESTARTWINDOWS"},			{0x0469,"CDM_HIDECONTROL"},			{0x0469,"EM_GETIMECOLOR"},			{0x046A,"CDM_SETDEFEXT"},
	{0x046A,"PSM_REBOOTSYSTEM"},			{0x046A,"EM_SETIMEOPTIONS"},		{0x046A,"UDM_GETBUDDY"},				{0x046B,"UDM_SETACCEL"},				{0x046B,"EM_GETIMEOPTIONS"},			{0x046B,"PSM_CANCELTOCLOSE"},		{0x046C,"PSM_QUERYSIBLINGS"},		{0x046C,"UDM_GETACCEL"},
	{0x046D,"PSM_UNCHANGED"},				{0x046D,"UDM_SETBASE"},				{0x046E,"PSM_APPLY"},					{0x046E,"UDM_GETBASE"},					{0x046F,"PSM_SETTITLE"},				{0x046F,"UDM_SETRANGE32"},			{0x0470,"PSM_SETWIZBUTTONS"},		{0x0470,"UDM_GETRANGE32"},
	{0x0471,"UDM_SETPOS32"},				{0x0471,"PSM_PRESSBUTTON"},			{0x0472,"UDM_GETPOS32"},				{0x0472,"PSM_SETCURSELID"},				{0x0473,"PSM_SETFINISHTEXT"},			{0x0474,"PSM_GETTABCONTROL"},		{0x0475,"PSM_ISDIALOGMESSAGE"},		{0x0476,"PSM_GETCURRENTPAGEHWND"},
	{0x0477,"PSM_INSERTPAGE"},				{0x0478,"EM_SETLANGOPTIONS"},		{0x0479,"EM_GETLANGOPTIONS"},			{0x047A,"EM_GETIMECOMPMODE"},			{0x047B,"EM_FINDTEXTW"},				{0x047C,"EM_FINDTEXTEXW"},			{0x047D,"PSM_SETHEADERTITLEA"},		{0x047D,"EM_RECONVERSION"},
	{0x047E,"PSM_SETHEADERTITLEW"},			{0x047F,"PSM_SETHEADERSUBTITLEA"},	{0x0480,"PSM_SETHEADERSUBTITLEW"},		{0x0481,"PSM_HWNDTOINDEX"},				{0x0482,"PSM_INDEXTOHWND"},				{0x0483,"PSM_PAGETOINDEX"},			{0x0484,"PSM_INDEXTOPAGE"},			{0x0485,"DL_BEGINDRAG"},
	{0x0485,"PSM_IDTOINDEX"},				{0x0486,"DL_DRAGGING"},				{0x0486,"PSM_INDEXTOID"},				{0x0487,"PSM_GETRESULT"},				{0x0487,"DL_DROPPED"},					{0x0488,"DL_CANCELDRAG"},			{0x0488,"PSM_RECALCPAGESIZES"},		{0x04C8,"EM_SETBIDIOPTIONS"},
	{0x04C9,"EM_GETBIDIOPTIONS"},			{0x04CA,"EM_SETTYPOGRAPHYOPTIONS"},	{0x04CB,"EM_GETTYPOGRAPHYOPTIONS"},		{0x04CC,"EM_SETEDITSTYLE"},				{0x04CD,"EM_GETEDITSTYLE"},				{0x04DD,"EM_GETSCROLLPOS"},			{0x04DE,"EM_SETSCROLLPOS"},			{0x04DF,"EM_SETFONTSIZE"},
	{0x04E0,"EM_GETZOOM"},					{0x04E1,"EM_SETZOOM"},

	{0x1000,"LVM_GETBKCOLOR"},				{0x1001,"MCM_GETCURSEL"},			{0x1001,"DTM_GETSYSTEMTIME"},			{0x1001,"LVM_SETBKCOLOR"},				{0x1002,"MCM_SETCURSEL"},				{0x1002,"LVM_GETIMAGELIST"},
	{0x1002,"DTM_SETSYSTEMTIME"},			{0x1003,"LVM_SETIMAGELIST"},		{0x1003,"DTM_GETRANGE"},				{0x1003,"MCM_GETMAXSELCOUNT"},			{0x1004,"DTM_SETRANGE"},				{0x1004,"MCM_SETMAXSELCOUNT"},		{0x1004,"LVM_GETITEMCOUNT"},		{0x1005,"DTM_SETFORMATA"},
	{0x1005,"LVM_GETITEM"},					{0x1005,"MCM_GETSELRANGE"},			{0x1006,"MCM_SETSELRANGE"},				{0x1006,"DTM_SETMCCOLOR"},				{0x1006,"LVM_SETITEM"},					{0x1007,"MCM_GETMONTHRANGE"},		{0x1007,"DTM_GETMCCOLOR"},			{0x1007,"LVM_INSERTITEM"},
	{0x1008,"MCM_SETDAYSTATE"},				{0x1008,"DTM_GETMONTHCAL"},			{0x1008,"LVM_DELETEITEM"},				{0x1009,"MCM_GETMINREQRECT"},			{0x1009,"DTM_SETMCFONT"},				{0x1009,"LVM_DELETEALLITEMS"},		{0x100A,"DTM_GETMCFONT"},			{0x100A,"MCM_SETCOLOR"},
	{0x100A,"LVM_GETCALLBACKMASK"},			{0x100B,"MCM_GETCOLOR"},			{0x100B,"LVM_SETCALLBACKMASK"},			{0x100C,"MCM_SETTODAY"},				{0x100C,"LVM_GETNEXTITEM"},				{0x100D,"MCM_GETTODAY"},			{0x100D,"LVM_FINDITEM"},			{0x100E,"MCM_HITTEST"},
	{0x100E,"LVM_GETITEMRECT"},				{0x100F,"LVM_SETITEMPOSITION"},		{0x100F,"MCM_SETFIRSTDAYOFWEEK"},		{0x1010,"LVM_GETITEMPOSITION"},			{0x1010,"MCM_GETFIRSTDAYOFWEEK"},		{0x1011,"LVM_GETSTRINGWIDTH"},		{0x1011,"MCM_GETRANGE"},			{0x1012,"MCM_SETRANGE"},
	{0x1012,"LVM_HITTEST"},					{0x1013,"LVM_ENSUREVISIBLE"},		{0x1013,"MCM_GETMONTHDELTA"},			{0x1014,"MCM_SETMONTHDELTA"},			{0x1014,"LVM_SCROLL"},					{0x1015,"MCM_GETMAXTODAYWIDTH"},	{0x1015,"LVM_REDRAWITEMS"},			{0x1016,"LVM_ARRANGE"},
	{0x1017,"LVM_EDITLABEL"},				{0x1018,"LVM_GETEDITCONTROL"},		{0x1019,"LVM_GETCOLUMN"},				{0x101A,"LVM_SETCOLUMN"},				{0x101B,"LVM_INSERTCOLUMN"},			{0x101C,"LVM_DELETECOLUMN"},		{0x101D,"LVM_GETCOLUMNWIDTH"},		{0x101E,"LVM_SETCOLUMNWIDTH"},
	{0x101F,"LVM_GETHEADER"},				{0x1021,"LVM_CREATEDRAGIMAGE"},		{0x1022,"LVM_GETVIEWRECT"},				{0x1023,"LVM_GETTEXTCOLOR"},			{0x1024,"LVM_SETTEXTCOLOR"},			{0x1025,"LVM_GETTEXTBKCOLOR"},		{0x1026,"LVM_SETTEXTBKCOLOR"},		{0x1027,"LVM_GETTOPINDEX"},
	{0x1028,"LVM_GETCOUNTPERPAGE"},			{0x1029,"LVM_GETORIGIN"},			{0x102A,"LVM_UPDATE"},					{0x102B,"LVM_SETITEMSTATE"},			{0x102C,"LVM_GETITEMSTATE"},			{0x102D,"LVM_GETITEMTEXT"},			{0x102E,"LVM_SETITEMTEXT"},			{0x102F,"LVM_SETITEMCOUNT"},
	{0x1030,"LVM_SORTITEMS"},				{0x1031,"LVM_SETITEMPOSITION32"},	{0x1032,"LVM_GETSELECTEDCOUNT"},		{0x1032,"DTM_SETFORMATW"},				{0x1033,"LVM_GETITEMSPACING"},			{0x1034,"LVM_GETISEARCHSTRING"},	{0x1035,"LVM_SETICONSPACING"},		{0x1036,"LVM_SETEXTENDEDLISTVIEWSTYLE"},
	{0x1037,"LVM_GETEXTENDEDLISTVIEWSTYLE"},{0x1038,"LVM_GETSUBITEMRECT"},		{0x1039,"LVM_SUBITEMHITTEST"},			{0x103A,"LVM_SETCOLUMNORDERARRAY"},		{0x103B,"LVM_GETCOLUMNORDERARRAY"},		{0x103C,"LVM_SETHOTITEM"},			{0x103D,"LVM_GETHOTITEM"},			{0x103E,"LVM_SETHOTCURSOR"},
	{0x103F,"LVM_GETHOTCURSOR"},			{0x1040,"LVM_APPROXIMATEVIEWRECT"},	{0x1041,"LVM_SETWORKAREAS"},			{0x1042,"LVM_GETSELECTIONMARK"},		{0x1043,"LVM_SETSELECTIONMARK"},		{0x1044,"LVM_SETBKIMAGE"},			{0x1045,"LVM_GETBKIMAGE"},			{0x1046,"LVM_GETWORKAREAS"},
	{0x1047,"LVM_SETHOVERTIME"},			{0x1048,"LVM_GETHOVERTIME"},		{0x1049,"LVM_GETNUMBEROFWORKAREAS"},	{0x104A,"LVM_SETTOOLTIPS"},				{0x104E,"LVM_GETTOOLTIPS"},				{0x1051,"LVM_SORTITEMSEX"},			{0x1091,"LVM_INSERTGROUP"},			{0x1093,"LVM_SETGROUPINFO"},
	{0x1095,"LVM_GETGROUPINFO"},			{0x1096,"LVM_REMOVEGROUP"},			{0x109B,"LVM_SETGROUPMETRICS"},			{0x109C,"LVM_GETGROUPMETRICS"},			{0x109D,"LVM_ENABLEGROUPVIEW"},			{0x109E,"LVM_SORTGROUPS"},			{0x109F,"LVM_INSERTGROUPSORTED"},	{0x10A0,"LVM_REMOVEALLGROUPS"},
	{0x10A1,"LVM_HASGROUP"},				{0x10A2,"LVM_SETTILEVIEWINFO"},		{0x10A3,"LVM_GETTILEVIEWINFO"},			{0x10A4,"LVM_SETTILEINFO"},				{0x10A5,"LVM_GETTILEINFO"},				{0x10A6,"LVM_SETINSERTMARK"},		{0x10A7,"LVM_GETINSERTMARK"},		{0x10A8,"LVM_INSERTMARKHITTEST"},
	{0x10A9,"LVM_GETINSERTMARKRECT"},		{0x10AA,"LVM_SETINSERTMARKCOLOR"},	{0x10AB,"LVM_GETINSERTMARKCOLOR"},		{0x1100,"TVM_INSERTITEM"},				{0x1101,"TVM_DELETEITEM"},				{0x1102,"TVM_EXPAND"},				{0x1104,"TVM_GETITEMRECT"},			{0x1105,"TVM_GETCOUNT"},
	{0x1106,"TVM_GETINDENT"},				{0x1107,"TVM_SETINDENT"},			{0x1108,"TVM_GETIMAGELIST"},			{0x1109,"TVM_SETIMAGELIST"},			{0x110A,"TVM_GETNEXTITEM"},				{0x110B,"TVM_SELECTITEM"},			{0x110C,"TVM_GETITEM"},				{0x110D,"TVM_SETITEM"},
	{0x110E,"TVM_EDITLABEL"},				{0x110F,"TVM_GETEDITCONTROL"},		{0x1110,"TVM_GETVISIBLECOUNT"},			{0x1111,"TVM_HITTEST"},					{0x1112,"TVM_CREATEDRAGIMAGE"},			{0x1113,"TVM_SORTCHILDREN"},		{0x1114,"TVM_ENSUREVISIBLE"},		{0x1115,"TVM_SORTCHILDRENCB"},
	{0x1116,"TVM_ENDEDITLABELNOW"},			{0x1117,"TVM_GETISEARCHSTRING"},	{0x1118,"TVM_SETTOOLTIPS"},				{0x1119,"TVM_GETTOOLTIPS"},				{0x111A,"TVM_SETINSERTMARK"},			{0x111B,"TVM_SETITEMHEIGHT"},		{0x111C,"TVM_GETITEMHEIGHT"},		{0x111D,"TVM_SETBKCOLOR"},
	{0x111E,"TVM_SETTEXTCOLOR"},			{0x111F,"TVM_GETBKCOLOR"},			{0x1120,"TVM_GETTEXTCOLOR"},			{0x1121,"TVM_SETSCROLLTIME"},			{0x1122,"TVM_GETSCROLLTIME"},			{0x1125,"TVM_SETINSERTMARKCOLOR"},	{0x1126,"TVM_GETINSERTMARKCOLOR"},	{0x1127,"TVM_GETITEMSTATE"},
	{0x1128,"TVM_SETLINECOLOR"},			{0x1129,"TVM_GETLINECOLOR"},		{0x1200,"HDM_GETITEMCOUNT"},			{0x1201,"HDM_INSERTITEM"},				{0x1202,"HDM_DELETEITEM"},				{0x1203,"HDM_GETITEM"},				{0x1204,"HDM_SETITEM"},				{0x1205,"HDM_LAYOUT"},
	{0x1206,"HDM_HITTEST"},					{0x1207,"HDM_GETITEMRECT"},			{0x1208,"HDM_SETIMAGELIST"},			{0x1209,"HDM_GETIMAGELIST"},			{0x120F,"HDM_ORDERTOINDEX"},			{0x1210,"HDM_CREATEDRAGIMAGE"},		{0x1211,"HDM_GETORDERARRAY"},		{0x1212,"HDM_SETORDERARRAY"},
	{0x1213,"HDM_SETHOTDIVIDER"},			{0x1214,"HDM_SETBITMAPMARGIN"},		{0x1215,"HDM_GETBITMAPMARGIN"},			{0x1216,"HDM_SETFILTERCHANGETIMEOUT"},	{0x1217,"HDM_EDITFILTER"},				{0x1218,"HDM_CLEARFILTER"},			{0x1302,"TCM_GETIMAGELIST"},		{0x1303,"TCM_SETIMAGELIST"},
	{0x1304,"TCM_GETITEMCOUNT"},			{0x1305,"TCM_GETITEM"},				{0x1306,"TCM_SETITEM"},					{0x1307,"TCM_INSERTITEM"},				{0x1308,"TCM_DELETEITEM"},				{0x1309,"TCM_DELETEALLITEMS"},		{0x130A,"TCM_GETITEMRECT"},			{0x130B,"TCM_GETCURSEL"},
	{0x130C,"TCM_SETCURSEL"},				{0x130D,"TCM_HITTEST"},				{0x130E,"TCM_SETITEMEXTRA"},			{0x1328,"TCM_ADJUSTRECT"},				{0x1329,"TCM_SETITEMSIZE"},				{0x132A,"TCM_REMOVEIMAGE"},			{0x132B,"TCM_SETPADDING"},			{0x132C,"TCM_GETROWCOUNT"},
	{0x132D,"TCM_GETTOOLTIPS"},				{0x132E,"TCM_SETTOOLTIPS"},			{0x132F,"TCM_GETCURFOCUS"},				{0x1330,"TCM_SETCURFOCUS"},				{0x1331,"TCM_SETMINTABWIDTH"},			{0x1332,"TCM_DESELECTALL"},			{0x1333,"TCM_HIGHLIGHTITEM"},		{0x1334,"TCM_SETEXTENDEDSTYLE"},
	{0x1335,"TCM_GETEXTENDEDSTYLE"},		{0x1401,"PGM_SETCHILD"},			{0x1402,"PGM_RECALCSIZE"},				{0x1403,"PGM_FORWARDMOUSE"},			{0x1404,"PGM_SETBKCOLOR"},				{0x1405,"PGM_GETBKCOLOR"},			{0x1406,"PGM_SETBORDER"},			{0x1407,"PGM_GETBORDER"},
	{0x1408,"PGM_SETPOS"},					{0x1409,"PGM_GETPOS"},				{0x140A,"PGM_SETBUTTONSIZE"},			{0x140B,"PGM_GETBUTTONSIZE"},			{0x140C,"PGM_GETBUTTONSTATE"},

	{0x2001,"CCM_SETBKCOLOR"},				{0x2002,"CCM_SETCOLORSCHEME"},		{0x2003,"CCM_GETCOLORSCHEME"},			{0x2004,"CCM_GETDROPTARGET"},			{0x2005,"CCM_SETUNICODEFORMAT"},		{0x2006,"CCM_GETUNICODEFORMAT"},	{0x2007,"CCM_SETVERSION"},			{0x2008,"CCM_GETVERSION"},			{0x202B,"OCM_DRAWITEM"},
	{0x202C,"OCM_MEASUREITEM"},				{0x202D,"OCM_DELETEITEM"},			{0x202E,"OCM_VKEYTOITEM"},				{0x202F,"OCM_CHARTOITEM"},				{0x2039,"OCM_COMPAREITEM"},				{0x204E,"OCM_NOTIFY"},				{0x2111,"OCM_COMMAND"},				{0x2114,"OCM_HSCROLL"},
	{0x2115,"OCM_VSCROLL"},					{0x2132,"OCM_CTLCOLORMSGBOX"},		{0x2133,"OCM_CTLCOLOREDIT"},			{0x2134,"OCM_CTLCOLORLISTBOX"},			{0x2135,"OCM_CTLCOLORBTN"},				{0x2136,"OCM_CTLCOLORDLG"},			{0x2137,"OCM_CTLCOLORSCROLLBAR"},	{0x2138,"OCM_CTLCOLORSTATIC"},
	{0x2210,"OCM_PARENTNOTIFY"},
};

filename	embedded_dll(const char *dll_name) {
	filename	dll_path = get_exec_dir().add_dir(dll_name);
	win::Resource	r(0, dll_name, "BIN");
	if (!check_writebuff(lvalue(FileOutput(dll_path)), r, r.length())) {
		ISO_OUTPUTF("Cannot write to ") << dll_path << '\n';
	}
	return dll_path;
}

class ViewMessages : public win::Window<ViewMessages> {
	win::ListViewControl	lv;
	Shared					*shared;
	dynamic_array<HEVENT>	events;

	void	GetDispInfo(string_accum &sa, int row, int col) {
		static const char *types[] = {
			"Queue",
			"Call",
			"Return",
		};
		auto	&event = events[row];
		switch (col) {
			case 0:		sa << formatted((uint64)event.hwnd, FORMAT::HEX|FORMAT::ZEROES, 12); break;
			case 1:		sa << types[event.type] << '(' << event.mode << ')'; break;
			case 2: {
				auto	*p = first_not(message_names, [&](const MessageName &a) { return a.msg < event.message; });
				sa << ifelse(p, p->name, event.message);
				break;
			}
			case 3:		sa << formatted(event.wParam, FORMAT::HEX|FORMAT::ZEROES, 12); break;
			case 4:		sa << formatted(event.lParam, FORMAT::HEX|FORMAT::ZEROES, 12); break;
			case 5:
				switch (event.type) {
					case HEVENT::QUEUE:
						sa << event.time;
						break;
					case HEVENT::RET:
						sa << event.result;
						break;
				}
				break;
			case 6:
				switch (event.type) {
					case HEVENT::QUEUE:
						sa << event.pt.x << ", " << event.pt.y;
						break;
				}
				break;
		}
	}

public:
	LRESULT Proc(UINT message, WPARAM wParam, LPARAM lParam) {
		switch (message) {
			case WM_SIZE:
				lv.Resize(win::Point(lParam));
				break;

			case WM_COPYDATA: {
				COPYDATASTRUCT	*cds	= (COPYDATASTRUCT*)lParam;
				if (HEVENT *event = (HEVENT*)cds->lpData)
					events.push_back(*event);
				lv.SetCount(events.size32(), LVSICF_NOINVALIDATEALL);
				if (lv.IsVisible(events.size32() - 2))
					lv.EnsureVisible(events.size32() - 1);
				return 1;
			}

			case WM_NOTIFY: {
				NMHDR	*nmh = (NMHDR*)lParam;
				switch (nmh->code) {
					case LVN_GETDISPINFOA: {
						auto	&item = ((NMLVDISPINFO*)nmh)->item;
						if (item.pszText)
							GetDispInfo(lvalue(fixed_accum(item.pszText, item.cchTextMax)), item.iItem, item.iSubItem);
						return 1;
					}
				}
				break;
			}

			case WM_NCDESTROY:
				delete this;
				return 0;
		}
		return Super(message, wParam, lParam);
	}
	ViewMessages(const win::WindowPos &wpos, ISOControl *c) {
		static dll_function<decltype(SetHook)> _SetHook(LoadLibraryA(embedded_dll("hook.dll")), "SetHook");
		Create(wpos, "Messages", CHILD | CLIPSIBLINGS | VISIBLE);
		lv.Create(GetChildWindowPos(), 0, CHILD | CLIPSIBLINGS | VISIBLE | LVS_REPORT | LVS_AUTOARRANGE | LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_OWNERDATA);
		lv.AddColumns("hWnd", 100, "Type", 100, "Message", 200, "wParam", 100, "lParam", 100, "Time/Result", 100, "Point", 100);
		shared = _SetHook(*this, c->thread_id());
	}
	~ViewMessages() {
		shared->Unhook();
	}
};

class EditorWindows : public app::Editor {
	virtual bool Matches(const ISO::Type *type) {
		return type->Is<ISOControl>();
	}
	virtual win::Control Create(app::MainWindow &main, const win::WindowPos &wpos, const ISO_ptr_machine<void> &p) {
		return *new ViewMessages(wpos, p);
	}
} editorwindows;
