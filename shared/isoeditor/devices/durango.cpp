#include "com.h"
#include "windows/control_helpers.h"
#include "main.h"
#include "device.h"
#include "thread.h"
#include "extra/date.h"
#include "filetypes/bitmap/bitmap.h"
#include <XtfConsoleManager.h>
#include <XtfConsoleControl.h>
#include <XtfFileIO.h>

#pragma comment (lib, "XtfConsoleManager")
#pragma comment (lib, "XtfConsoleControl")
#pragma comment (lib, "XtfFileIO")

using namespace app;

//-----------------------------------------------------------------------------
//	DurangoFile
//-----------------------------------------------------------------------------
class DurangoFile : public ISO::VirtualDefaults, com<IXtfCopyFileCallback> {
	IXtfFileIOClient	*file;
	string				spec;
	malloc_block		data;

	HRESULT OnStartFileCopy(LPCWSTR pszRootDirectory, LPCWSTR pszSearchPattern, LPCXTFFILEINFO pSrcFileInfo, LPCWSTR pszDstFileName) { return S_OK; }
	HRESULT OnFileCopyProgress(LPCWSTR pszSrcFileName, LPCWSTR pszDstFileName, ULONGLONG ullFileSize, ULONGLONG ullBytesCopied) { return S_OK; }
	HRESULT OnEndFileCopy(LPCWSTR pszRootDirectory, LPCWSTR pszSearchPattern, LPCXTFFILEINFO pSrcFileInfo, LPCWSTR pszDstFileName, HRESULT hrErrorCode) {
		data = malloc_block::unterminated(FileInput(str8(pszDstFileName)));
		return S_OK;
	}

public:
	DurangoFile(IXtfFileIOClient *file, const char *spec, const XTFFILEINFO *info) : file(file), spec(string("x")+spec) {
	}
	ISO::Browser2	Deref() {
		if (!data) {
			HRESULT hr = file->CopyFiles(str16(spec), 0, 0, 0, L"C:\temp", XTFCOPYFILE_DEFER, this);
		}
		return ISO::MakeBrowser(data);
	}
};
ISO_DEFUSERVIRTX(DurangoFile, "File");

//-----------------------------------------------------------------------------
//	DurangoDir
//-----------------------------------------------------------------------------

class DurangoDir : public ISO::VirtualDefaults, com<IXtfFindFileCallback> {
	IXtfFileIOClient				*file;
	string							spec;
	dynamic_array<ISO_ptr<void> >	ptrs;

	HRESULT STDMETHODCALLTYPE OnFoundFile(LPCWSTR root, LPCWSTR search, const XTFFILEINFO *info) {
		string	name = info->pszFileName;
		if (info->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			new(ptrs) ISO_ptr<DurangoDir>(filename(name).name_ext(), file, name);
		else
			new(ptrs) ISO_ptr<DurangoFile>(filename(name).name_ext(), file, name, info);
		return S_OK;
	}

public:
	DurangoDir(IXtfFileIOClient *file, const char *spec) : file(file), spec(spec) {}

	uint32			Count() {
		if (ptrs.empty())
			file->FindFiles(
				str16(filename(spec).add_dir("*.*")),
				FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_ARCHIVE | FILE_ATTRIBUTE_DEVICE | FILE_ATTRIBUTE_NORMAL,
				0, 0, 0,
				this
			);
		return ptrs.size32();
	}
	tag2			GetName(int i)		{ return ptrs[i].ID();	}
	ISO::Browser2	Index(int i)		{ return ptrs[i];		}
	tag2			ID()	const		{ return spec; }
};
ISO_DEFUSERVIRTX(DurangoDir, "Directory");

//-----------------------------------------------------------------------------
//	DurangoScreenShot
//-----------------------------------------------------------------------------
class DurangoScreenShot : public ISO::VirtualDefaults {
	string	address;
	ISO_ptr<bitmap>	p;
public:
	DurangoScreenShot(const char *_address) : address(_address) {}
	ISO::Browser2	Deref() {
		if (!p) {
			HBITMAP	hbm;
			if (SUCCEEDED(XtfCaptureScreenshot(str16(address), &hbm))) {
				p.Create(0);
				win::Bitmap::Params		bm	= win::Bitmap(hbm).GetParams();
				ISO_rgba				*c	= p->Create(bm.Width(), bm.Height());
				copy_n((Texel<B8G8R8>*)bm.bmBits, c, bm.Width() * bm.Height());
			}
		}
		return p;
	}
};
ISO_DEFVIRT(DurangoScreenShot);

//-----------------------------------------------------------------------------
//	DurangoConsole
//-----------------------------------------------------------------------------
class DurangoConsole : public ISO::VirtualDefaults {
	string	name;
	string	address;
	com_ptr<IXtfConsoleControlClient>	control;
	com_ptr<IXtfFileIOClient>			file;
	dynamic_array<ISO_ptr<string> >		settings;
	dynamic_array<ISO_ptr<xint32> >		processes;
	dynamic_array<DurangoDir>			root;
	ISO_ptr<DurangoScreenShot>			screenshot;
public:
	DurangoConsole(const XTFCONSOLEDATA *p) : name(com_string(p->bstrAlias)), address(com_string(p->bstrAddress)) {
		XtfCreateConsoleControlClient(p->bstrAddress, control.uuid(), (void**)&control);
		ConcurrentJobs::Get().add([&]() {
			BOOL fIsInConnectedStandby, fAllowsInstantOn, fAllowsXboxAppConnections;
			if (SUCCEEDED(control->QueryConnectedStandbyEx(&fIsInConnectedStandby, &fAllowsInstantOn, &fAllowsXboxAppConnections))) {
				XtfCreateFileIOClient(str16(address), file.uuid(), (void**)&file);

				int					count;
				XTFCONFIGSETTING	*set;
				control->GetAllSettings(&set, &count);

				settings.resize(count);
				for (int i = 0; i < count; i++)
					settings[i].Create(str8(set[i].pszSettingName), set[i].pszSettingValue);

				struct processes_getter : com<IXtfRunningProcessCallback> {
					dynamic_array<ISO_ptr<xint32> >	&processes;
					HRESULT STDMETHODCALLTYPE OnFoundProcess(const XTFPROCESSINFO *proc) {
						new(processes) ISO_ptr<xint32>(str8(proc->pszImageFileName), proc->dwProcessId);
						return S_OK;
					}
					processes_getter(dynamic_array<ISO_ptr<xint32> > &_processes) : processes(_processes) {}
					processes_getter*	ptr()	{ return this; }
				};

				control->GetRunningProcesses(processes_getter(processes).ptr());
				for (char i = 'A'; i <= 'Z'; ++i) {
					auto	*dir = new(root) DurangoDir(file, format_string("%c:\\", i));
					if (dir->Count() == 0)
						root.pop_back();
				}

	//			root.Create(0, pair<IXtfFileIOClient*,const char*>(file, "\\"));
				screenshot.Create(0, address);
			}
		});
	}
	const char*		Name()		const	{ return name; }
	uint32			Count()				{ return 4;	}
	tag2			GetName(int i)		{
		switch (i) {
			case 0:		return "Settings";
			case 1:		return "Processes";
			case 2:		return "Files";
			case 3:		return "Screenshot";
			default:	return 0;
		}
	}
	ISO::Browser2	Index(int i) {
		switch (i) {
			case 0:		return ISO::MakeBrowser(settings);
			case 1:		return ISO::MakeBrowser(processes);
			case 2:		return ISO::MakeBrowser(root);//root;
			case 3:		return screenshot;
			default:	return ISO::Browser();
		}
	}
};
ISO_DEFVIRT(DurangoConsole);

//-----------------------------------------------------------------------------
//	DurangoExplorer
//-----------------------------------------------------------------------------

class DurangoExplorer : public ISO::VirtualDefaults, com_list<
	IXtfConsoleManagerCallback,
	IXtfEnumerateConsolesCallback
> {
	dynamic_array<DurangoConsole>		consoles;
	com_ptr<IXtfConsoleManager>			manager;

	//IXtfConsoleManagerCallback
	void STDMETHODCALLTYPE OnAddConsole(const XTFCONSOLEDATA *console) {
	}
	void STDMETHODCALLTYPE OnRemoveConsole(const XTFCONSOLEDATA *console) {
	}
	void STDMETHODCALLTYPE OnChangedDefaultConsole(const XTFCONSOLEDATA *console) {
	}

	//IXtfEnumerateConsolesCallback
	HRESULT STDMETHODCALLTYPE OnConsoleFound(const XTFCONSOLEDATA *console) {
		new(consoles) DurangoConsole(console);
		return S_OK;
	}

public:
	bool			Init();
	uint32			Count()				{ return uint32(consoles.size());	}
	tag				GetName(int i)		{ return consoles[i].Name();		}
	ISO::Browser	Index(int i)		{ return ISO::MakeBrowser(consoles[i]);	}
};

bool DurangoExplorer::Init() {
	init_com();
	ISO_VERIFY(load_library("XtfConsoleManager", "DurangoXDK", "bin"));
	ISO_VERIFY(load_library("XtfConsoleControl", "DurangoXDK", "bin"));

	return SUCCEEDED(XtfCreateConsoleManager(this, manager.uuid(), (void**)&manager))
		&& SUCCEEDED(manager->EnumerateConsoles(this));
}

ISO_DEFVIRT(DurangoExplorer);

//-----------------------------------------------------------------------------
//	DurangoDevice
//-----------------------------------------------------------------------------

struct DurangoDevice : DeviceT<DurangoDevice>, DeviceCreateT<DurangoDevice> {
	void			operator()(const DeviceAdd &add)	{ add("Durango", this); }
	ISO_ptr<void>	operator()(const Control &main)		{
		ISO_ptr<DurangoExplorer>	p("Durango");
		if (p->Init())
			return p;
		return ISO_NULL;
	}
} durango_device;
