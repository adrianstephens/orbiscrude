#include "filedialogs.h"
#include "directory.h"
#include "base/functions.h"
#include <shellapi.h>
#include <shlobj.h>
#include <sapi.h>
#include <ole2.h>

namespace iso { namespace win {

bool OpenExplorer(const filename &fn) {
	return SUCCEEDED(SHOpenFolderAndSelectItems(ILCreateFromPathA(fn), 0, 0, 0));
}

int GetOpen(HWND hWnd, filename &fn, const char *title, const char *filter) {
	OPENFILENAMEA	ofn;

	clear(ofn);
	ofn.lStructSize			= sizeof(OPENFILENAME);
    ofn.hwndOwner			= hWnd;
	ofn.hInstance			= (HINSTANCE)UlongToHandle(::GetWindowLongPtr(hWnd, GWLP_HINSTANCE));
    ofn.lpstrFilter			= filter;
    ofn.lpstrFile			= fn;
    ofn.nMaxFile			= sizeof(fn);
    ofn.lpstrTitle			= title;
    ofn.Flags				= OFN_EXPLORER;

	return GetOpenFileNameA(&ofn) ? ofn.nFilterIndex : 0;
}

int GetSave(HWND hWnd, filename &fn, const char *title, const char *filter) {
	OPENFILENAMEA	ofn;
	const char		*defext = fn.ext_ptr();

	if (!defext)
		defext = filter + strlen(filter) + 1;
	defext = strchr(defext, '.');
	if (defext && defext[0] == '.')
		++defext;

	clear(ofn);
	ofn.lStructSize			= sizeof(OPENFILENAME);
	ofn.hwndOwner			= hWnd;
	ofn.hInstance			= (HINSTANCE)UlongToHandle(::GetWindowLongPtr(hWnd, GWLP_HINSTANCE));
	ofn.lpstrFilter			= filter;
	ofn.lpstrFile			= fn;
	ofn.nMaxFile			= sizeof(fn);
	ofn.lpstrTitle			= title;
	ofn.Flags				= OFN_EXPLORER | OFN_HIDEREADONLY | OFN_NOVALIDATE;
	ofn.lpstrDefExt			= defext;
//	ofn.lpfnHook = [](HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
//		return UINT_PTR(0);
//	};

	return GetSaveFileNameA(&ofn) ? ofn.nFilterIndex : 0;
}

bool GetDirectory(HWND hWnd, filename &fn, const char *title) {
	struct browser {
		const char *dir;
		int operator()(HWND hwnd, UINT message, LPARAM lParam) {
			if (message == BFFM_INITIALIZED) {
				SendMessage(hwnd, BFFM_SETSELECTION, TRUE, (LPARAM)dir);
			}
			return 0;
		}
		browser(const char *_dir) : dir(_dir) {}
	} b(fn);

	IMalloc *malloc;

	if (SUCCEEDED(SHGetMalloc(&malloc))) {
		LPITEMIDLIST	pidl;
		BROWSEINFOA		bi = {
			hWnd,
			NULL,
			fn,
			title,
			/*BIF_NEWDIALOGSTYLE |*/ BIF_RETURNONLYFSDIRS,
			(BFFCALLBACK)stdcall_callback_function_end<int(HWND,UINT,LPARAM)>(&b),
			(LPARAM)&b,
			0
		};
		if (pidl = SHBrowseForFolderA(&bi)) {
			SHGetPathFromIDListA(pidl, fn);
			malloc->Free(pidl);
			malloc->Release();
			return true;
		}
		malloc->Release();
	}
	return false;
}

//-----------------------------------------------------------------------------
//	Font Selector
//-----------------------------------------------------------------------------

bool GetFont(HWND hWnd, Font::Params &font) {
	CHOOSEFONT		cf;
	clear(cf);
	cf.lStructSize	= sizeof(CHOOSEFONT);
	cf.hwndOwner	= hWnd;
	cf.lpLogFont	= &font;
	cf.Flags		= CF_INITTOLOGFONTSTRUCT;

	return !!ChooseFont(&cf);
}

//-----------------------------------------------------------------------------
//	Colour Selector
//-----------------------------------------------------------------------------

bool GetColour(HWND hWnd, Colour &col) {
	CHOOSECOLOR	cc;
	COLORREF	custom[16];

	clear(cc);
	cc.lStructSize	= sizeof(cc);
	cc.hwndOwner	= hWnd;
	cc.lpCustColors	= custom;
	cc.Flags		= CC_RGBINIT;
	cc.rgbResult	= col;

	if (ChooseColor(&cc)) {
		col = Colour(cc.rgbResult);
		return true;
	}
	return false;
}

} } // namespace iso::win
