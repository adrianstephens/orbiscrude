#ifndef VIEWER_H
#define VIEWER_H

#include "iso/iso.h"
#include "iso/iso_files.h"
#include "jobs.h"

#ifdef PLAT_WINRT
#include "winrt/window.h"
struct SeparateWindow {};
#else
#include "windows/window.h"
#include "windows/docker.h"
#endif

#ifdef PLAT_PC
#include "windows/common.rc.h"
#endif

namespace app {
using namespace iso;
using namespace win;

#if defined PLAT_WIN32
enum {
	WM_ISO_SET		= WM_USER,
	WM_ISO_DRAG,
	WM_ISO_ERROR,
	WM_ISO_CONTEXTMENU,
	WM_ISO_SELECT,
	WM_ISO_JOB,
};
#endif

ISO::Browser2 GetSettings(const char *path);

class MainWindow : public SeparateWindow {
protected:
	typedef	SeparateWindow	Super;
	static MainWindow*	me;
	static multi_string_alloc<char>	load_filters;
public:
	static MainWindow*	Get()				{ return me; }
#ifdef PLAT_WIN32
	static MainWindow*	Cast(Control c)		{ return static_cast<MainWindow*>(SeparateWindow::Cast(c)); }
#endif

	static void	AddLoadFilters(cmulti_string filters);
	MainWindow();
	~MainWindow();
	void	AddView(Control control);
	void	SetTitle(const char *title);
	void	SetFilename(const char *fn);
#ifdef PLAT_WINRT
	Control	AddView(const Interop::TypeName& sourcePageType, object param);
#endif
};

//-----------------------------------------------------------------------------
//	Editor
//-----------------------------------------------------------------------------

struct ISO_VirtualTarget : ISO::Browser2	{
	ISO::Browser	v;
	string			spec;
	uint32			bin;

	explicit ISO_VirtualTarget(const ISO::Browser2 &b) : ISO::Browser2(b), bin(0) {}
	ISO_VirtualTarget(const ISO::Browser2 &b, ISO::Browser &_v, const char *_spec, uint32 _bin) : ISO::Browser2(b), v(_v), spec(_spec), bin(_bin) {}

	void operator=(const ISO::Browser2 &b) {
		ISO::Browser2::operator=(b);
	}
	bool Update() {
		return !v || v.Update(spec);
	}
	bool IsPtr() const {
		return SkipUser().GetType() == ISO::REFERENCE || SkipUser().IsVirtPtr();
	}
	ISO::Browser2 GetData() const {
		if (const char *spec = External())
			return FileHandler::ReadExternal(spec);
		if (IsPtr()) {
			if (ISO::Browser2 b2 = **this)
				return b2;
		}
		return *this;
	}
	ISO_ptr_machine<void> GetPtr() const {
		if (const char *spec = External())
			return FileHandler::ReadExternal(spec);
		return *this;
	}
};

class Editor : public static_list<Editor> {
public:
	enum MODE {
		MODE_create,
		MODE_home,
		MODE_command,
		MODE_click,
	};
protected:
	virtual bool		Matches(const ISO::Type *type)				{ return false; }
	virtual bool		Matches(const ISO::Browser &b)				{ return b.GetTypeDef() && Matches(b.GetTypeDef()); }
	virtual bool		Command(MainWindow &main, ID id, MODE mode)	{ return false; }

public:
	static	bool		CommandAll(MainWindow &main, ID id, MODE mode);
	static	Editor*		Find(ISO::Browser2 &b);
	static	Control		FindOpen(const char *name);

	virtual Control		Create(MainWindow &main, const WindowPos &pos, const ISO_ptr<void,32> &p) {
		return Create(main, pos, ISO::GetPtr<64>((void*)p));
	}
	virtual Control		Create(MainWindow &main, const WindowPos &pos, const ISO_ptr<void,64> &p) {
		ISO_ASSERT(0);
		return Control();
	}
	virtual Control		Create(MainWindow &main, const WindowPos &pos, const ISO_VirtualTarget &v) {
		return Create(main, pos, v.GetPtr());
	}
	virtual ID			GetIcon(const ISO_ptr<void> &p)				{ return ID(); }
	virtual ID			GetIcon(const ISO_VirtualTarget &v)			{ return GetIcon(v.GetPtr()); }
};

} //namespace app

#endif //VIEWER_H
