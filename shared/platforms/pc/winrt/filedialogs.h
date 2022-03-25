#ifndef FILEDIALOGS_H
#define FILEDIALOGS_H

#include "window.h"
#include "filename.h"
#include "winrt/coroutine.h"
#include "winrt/Windows.ApplicationModel.Core.h"
#include "winrt/Windows.UI.Core.h"
#include "winrt/Windows.Storage.h"
#include "winrt/Windows.Storage.Pickers.h"
#include "winrt/Windows.Storage.AccessCache.h"

namespace iso {namespace win {
	/*
iso_export bool OpenExplorer(const filename &fn);
iso_export int	GetSave(HWND hWnd, filename &fn, const char *title, const multi_string &filter);
iso_export bool	GetDirectory(HWND hWnd, filename &fn, const char *title);
iso_export bool	GetFont(HWND hWnd, Font::Params &font);
iso_export bool GetColour(HWND hWnd, Colour &col);
*/
hstring GetOpen(const char *title, const multi_string &filter) {
	using namespace Windows;
	using namespace Storage;
	using namespace Pickers;
	using namespace UI::Core;

	
	HANDLE	event = CreateEvent(NULL, FALSE, FALSE, NULL);
	ptr<FileOpenPicker>	picker;
	ptr<Foundation::IAsyncOperation<ptr<StorageFile>>> async;

	ApplicationModel::Core::CoreApplication::MainView->CoreWindow->Dispatcher->RunAsync(CoreDispatcherPriority::Normal, [&]() {
		picker	= ref_new<FileOpenPicker>();

		int		i		= 0;
		for (auto f : filter) {
			if (i++ & 1) {
				for (filename p : parts<';'>(f)) {
					auto	ext = p.ext_ptr();
					if (!string_find(ext, '*'))
						picker->FileTypeFilter->Append(str16(ext));
				}
			}
		}
		picker->SuggestedStartLocation = PickerLocationId::PicturesLibrary;
		async = picker->PickSingleFileAsync();
		SetEvent(event);
	});

	WaitForSingleObject(event, INFINITE);
	if (auto file = wait(async)) {
		filereader::add_access(file);
		return file->Path();
	}

	return nullptr;
}


} } // namespace iso::win

#endif // FILEDIALOGS_H
