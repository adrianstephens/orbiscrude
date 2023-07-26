#ifndef FILEDIALOGS_H
#define FILEDIALOGS_H

#include "window.h"
#include "filename.h"

namespace iso {namespace win {

iso_export bool OpenExplorer(const filename &fn);
iso_export int	GetOpen(HWND hWnd, filename &fn, text title, string_ref filter);
iso_export int	GetSave(HWND hWnd, filename &fn, text title, string_ref filter);
iso_export bool	GetDirectory(HWND hWnd, filename &fn, const char *title);
iso_export bool	GetFont(HWND hWnd, Font::Params &font);
iso_export bool GetColour(HWND hWnd, Colour &col);

} } // namespace iso::win

#endif // FILEDIALOGS_H
