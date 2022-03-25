#ifndef DWM_H
#define DWM_H

#include "window.h"
#include <dwmapi.h>

#pragma comment(lib, "dwmapi.lib")
#undef small
#undef min
#undef max

namespace iso { namespace win {

template<DWMWINDOWATTRIBUTE A> struct DWM_TYPE;
template<> struct DWM_TYPE<DWMWA_NCRENDERING_ENABLED>			{typedef BOOL type; };
template<> struct DWM_TYPE<DWMWA_NCRENDERING_POLICY>			{typedef DWMNCRENDERINGPOLICY type; };
template<> struct DWM_TYPE<DWMWA_TRANSITIONS_FORCEDISABLED>		{typedef BOOL type; };
template<> struct DWM_TYPE<DWMWA_ALLOW_NCPAINT>					{typedef BOOL type; };
template<> struct DWM_TYPE<DWMWA_CAPTION_BUTTON_BOUNDS>			{typedef RECT type; };
template<> struct DWM_TYPE<DWMWA_NONCLIENT_RTL_LAYOUT>			{typedef BOOL type; };
template<> struct DWM_TYPE<DWMWA_FORCE_ICONIC_REPRESENTATION>	{typedef BOOL type; };
template<> struct DWM_TYPE<DWMWA_FLIP3D_POLICY>					{typedef DWMFLIP3DWINDOWPOLICY type; };
template<> struct DWM_TYPE<DWMWA_EXTENDED_FRAME_BOUNDS>			{typedef RECT type; };
//windows 7+
template<> struct DWM_TYPE<DWMWA_HAS_ICONIC_BITMAP>				{typedef BOOL type; };
template<> struct DWM_TYPE<DWMWA_DISALLOW_PEEK>					{typedef BOOL type; };
template<> struct DWM_TYPE<DWMWA_EXCLUDED_FROM_PEEK>			{typedef BOOL type; };
//windows 8+
//template<> struct DWM_TYPE<DWMWA_CLOAK>						{typedef BOOL type; };
//template<> struct DWM_TYPE<DWMWA_CLOAKED>						{typedef DWMCLOAKEDENUM type; };
//template<> struct DWM_TYPE<DWMWA_FREEZE_REPRESENTATION>		{typedef BOOL type; };
/*
template<typename T> bool SetDWM(HWND hWnd, DWMWINDOWATTRIBUTE attribute, const T &value) {
	return SUCCEEDED(DwmSetWindowAttribute(hWnd, attribute, &value, sizeof(T)));
}
template<typename T> bool GetDWM(HWND hWnd, DWMWINDOWATTRIBUTE attribute, T &value) {
	return SUCCEEDED(DwmGetWindowAttribute(hWnd, attribute, &value, sizeof(T)));
}
*/

enum WCAATTRIBUTE {
	WCA_NCRENDERING_ENABLED = 1,      // [get] Is non-client rendering enabled/disabled
	WCA_NCRENDERING_POLICY,           // [set] Non-client rendering policy
	WCA_TRANSITIONS_FORCEDISABLED,    // [set] Potentially enable/forcibly disable transitions
	WCA_ALLOW_NCPAINT,                // [set] Allow contents rendered in the non-client area to be visible on the DWM-drawn frame.
	WCA_CAPTION_BUTTON_BOUNDS,        // [get] Bounds of the caption button area in window-relative space.
	WCA_NONCLIENT_RTL_LAYOUT,         // [set] Is non-client content RTL mirrored
	WCA_FORCE_ICONIC_REPRESENTATION,  // [set] Force this window to display iconic thumbnails.
	WCA_FLIP3D_POLICY,                // [set] Designates how Flip3D will treat the window.
	WCA_EXTENDED_FRAME_BOUNDS,        // [get] Gets the extended frame bounds rectangle in screen space
	WCA_HAS_ICONIC_BITMAP,            // [set] Indicates an available bitmap when there is no better thumbnail representation.
	WCA_DISALLOW_PEEK,                // [set] Don't invoke Peek on the window.
	WCA_EXCLUDED_FROM_PEEK,           // [set] LivePreview exclusion information
	WCA_CLOAK,                        // [set] Cloak or uncloak the window
	WCA_CLOAKED,                      // [get] Gets the cloaked state of the window
	WCA_FREEZE_REPRESENTATION,        // [set] Force this window to freeze the thumbnail without live update

	WCA_ACCENT_POLICY = 19,
};

struct WINCOMPATTRDATA {
	DWORD		attribute;
	void		*data;
	ULONG		dataSize;
};

enum WCAACCENTSTATE {
	ACCENT_DISABLED						= 0,
	ACCENT_ENABLE_GRADIENT				= 1,
	ACCENT_ENABLE_TRANSPARENTGRADIENT	= 2,
	ACCENT_ENABLE_BLURBEHIND			= 3,
	ACCENT_INVALID_STATE				= 4
};

struct WCAACCENTPOLICY {
	WCAACCENTSTATE	state;
	int				flags;
	int				gradient;
	int				animation;
};
template<WCAATTRIBUTE A> struct WCA_TYPE;
template<> struct WCA_TYPE<WCA_ACCENT_POLICY>		{ typedef WCAACCENTPOLICY type; };

struct DWM_static {
	BOOL	composition;
	DWORD	colourisation;
	BOOL	blending;
	dll_function<BOOL WINAPI(HWND, WINCOMPATTRDATA*)>	SetWindowCompositionAttribute;
	dll_function<BOOL WINAPI(HWND, WINCOMPATTRDATA*)>	GetWindowCompositionAttribute;

	DWM_static() : composition(FALSE)
		, SetWindowCompositionAttribute(LoadLibraryA("user32.dll"), "SetWindowCompositionAttribute")
		, GetWindowCompositionAttribute(LoadLibraryA("user32.dll"), "GetWindowCompositionAttribute")
	{
		DwmIsCompositionEnabled(&composition);
		DwmGetColorizationColor(&colourisation, &blending);
	}
	bool	EnableComposition(bool enable = true) {
#if _WIN32_WINNT< _WIN32_WINNT_W
		return SUCCEEDED(DwmEnableComposition(enable ? DWM_EC_ENABLECOMPOSITION : DWM_EC_DISABLECOMPOSITION));
#else
		return false;
#endif
	}
	bool	Flush()	{
		return SUCCEEDED(DwmFlush());
	}
};

struct _DWM : singleton<DWM_static> {
	static bool		Composition()	{ return single().composition; }
	static bool		Blending()		{ return single().blending; }
	static DWORD	Colourisation()	{ return single().colourisation; }
};

template<typename T> class DWM {
	const T	*super() const	{ return static_cast<const T*>(this); }
	template<DWMWINDOWATTRIBUTE A> typename DWM_TYPE<A>::type GetDWM() const {
		typename DWM_TYPE<A>::type value;
		DwmGetWindowAttribute(*super(), A, &value, sizeof(value));
		return value;
	}
	template<DWMWINDOWATTRIBUTE A> bool SetDWM(const typename DWM_TYPE<A>::type &value) const {
		return SUCCEEDED(DwmSetWindowAttribute(*super(), A, &value, sizeof(value)));
	}
	template<WCAATTRIBUTE A> typename WCA_TYPE<A>::type GetWCA() const {
		typename WCA_TYPE<A>::type value;
		WINCOMPATTRDATA	data = {A, &value, sizeof(value)};
		single().GetWindowCompositionAttribute(*super(), &data);
		return value;
	}
	template<WCAATTRIBUTE A> bool SetWCA(const typename WCA_TYPE<A>::type &value) const {
		WINCOMPATTRDATA	data = {A, (void*)&value, sizeof(value)};
		return !!single().SetWindowCompositionAttribute(*super(), &data);
	}
public:

	bool						NCRendering()				const	{ return GetDWM<DWMWA_NCRENDERING_ENABLED>			(); }
	DWMNCRENDERINGPOLICY		NCRenderPolicy()			const	{ return GetDWM<DWMWA_NCRENDERING_POLICY>			(); }
	bool						DisableTransitions()		const	{ return GetDWM<DWMWA_TRANSITIONS_FORCEDISABLED>	(); }
	bool						AllowNCPaint()				const	{ return GetDWM<DWMWA_ALLOW_NCPAINT>				(); }
	Rect						CaptionButtonBounds()		const	{ return GetDWM<DWMWA_CAPTION_BUTTON_BOUNDS>		(); }
	bool						NonClientRTL()				const	{ return GetDWM<DWMWA_NONCLIENT_RTL_LAYOUT>			(); }
	bool						ForceIconic()				const	{ return GetDWM<DWMWA_FORCE_ICONIC_REPRESENTATION>	(); }
	DWMFLIP3DWINDOWPOLICY		Flip3DPolicy()				const	{ return GetDWM<DWMWA_FLIP3D_POLICY>				(); }
	Rect						ExtendedFrameBounds()		const	{ return GetDWM<DWMWA_EXTENDED_FRAME_BOUNDS>		(); }
	bool						IconicBitmap()				const	{ return GetDWM<DWMWA_HAS_ICONIC_BITMAP>			(); }
	bool						DisallowPeek()				const	{ return GetDWM<DWMWA_DISALLOW_PEEK>				(); }
	bool						ExcludeFromPeek()			const	{ return GetDWM<DWMWA_EXCLUDED_FROM_PEEK>			(); }

	bool	NCRendering			(bool					v)	const	{ return SetDWM<DWMWA_NCRENDERING_ENABLED>			(v); }
	bool	NCRenderPolicy		(DWMNCRENDERINGPOLICY	v)	const	{ return SetDWM<DWMWA_NCRENDERING_POLICY>			(v); }
	bool	DisableTransitions	(bool					v)	const	{ return SetDWM<DWMWA_TRANSITIONS_FORCEDISABLED>	(v); }
	bool	AllowNCPaint		(bool					v)	const	{ return SetDWM<DWMWA_ALLOW_NCPAINT>				(v); }
	bool	CaptionButtonBounds	(Rect					v)	const	{ return SetDWM<DWMWA_CAPTION_BUTTON_BOUNDS>		(v); }
	bool	NonClientRTL		(bool					v)	const	{ return SetDWM<DWMWA_NONCLIENT_RTL_LAYOUT>			(v); }
	bool	ForceIconic			(bool					v)	const	{ return SetDWM<DWMWA_FORCE_ICONIC_REPRESENTATION>	(v); }
	bool	Flip3DPolicy		(DWMFLIP3DWINDOWPOLICY	v)	const	{ return SetDWM<DWMWA_FLIP3D_POLICY>				(v); }
	bool	ExtendedFrameBounds	(Rect					v)	const	{ return SetDWM<DWMWA_EXTENDED_FRAME_BOUNDS>		(v); }
	bool	IconicBitmap		(bool					v)	const	{ return SetDWM<DWMWA_HAS_ICONIC_BITMAP>			(v); }
	bool	DisallowPeek		(bool					v)	const	{ return SetDWM<DWMWA_DISALLOW_PEEK>				(v); }
	bool	ExcludeFromPeek		(bool					v)	const	{ return SetDWM<DWMWA_EXCLUDED_FROM_PEEK>			(v); }

#if 1
	bool	BlurBehind(bool b) const {
		WCAACCENTPOLICY	accent = {ACCENT_ENABLE_BLURBEHIND, 0, 0, 0};
		return SetWCA<WCA_ACCENT_POLICY>(accent);
	}
#else
	bool	BlurBehind(bool b) const {
		DWM_BLURBEHIND bb = {DWM_BB_ENABLE, b, 0};
		return SUCCEEDED(DwmEnableBlurBehindWindow(*super(), &bb));
	}
	bool	BlurBehind(HRGN r) const {
		DWM_BLURBEHIND bb = {DWM_BB_ENABLE | DWM_BB_BLURREGION, true, r};
		return SUCCEEDED(DwmEnableBlurBehindWindow(*super(), &bb));
	}
#endif

	bool ExtendIntoClient(int left, int right, int top, int bottom) const {
		MARGINS margins = {left, right, top, bottom};
		return SUCCEEDED(DwmExtendFrameIntoClientArea(*super(), &margins));
	}
	bool ExtendIntoClient(const Rect &frame) const {
		return ExtendIntoClient(-frame.Left(), frame.Right(), -frame.Top(), frame.Bottom());
	}
};

} } // namespace iso::win
#endif	// DWM_H
