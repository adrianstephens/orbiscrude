#include <Windows.h>

struct Shared {
	HHOOK		hook[3];
	HINSTANCE	hInstance;
	HWND		hWnd;

	void	Unhook() {
		UnhookWindowsHookEx(hook[0]);
		UnhookWindowsHookEx(hook[1]);
		UnhookWindowsHookEx(hook[2]);
	}

};

struct HEVENT {
	enum { QUEUE, CALL, RET};
	int		type;
	WPARAM	mode;
	HWND	hwnd;
	UINT	message;
	WPARAM	wParam;
	LPARAM	lParam;
	union {
		struct {
			DWORD	time;	// GetMsg only
			POINT	pt;		// GetMsg only
		};
		LRESULT		result;	// CallWndRet only
	};
	HEVENT(const MSG *msg, WPARAM mode) :
		type	(QUEUE), mode(mode),
		hwnd	(msg->hwnd),
		message	(msg->message),
		wParam	(msg->wParam),
		lParam	(msg->lParam),
		time	(msg->time),
		pt		(msg->pt)
	{}
	HEVENT(const CWPSTRUCT *msg, WPARAM mode) :
		type	(CALL), mode(mode),
		hwnd	(msg->hwnd),
		message	(msg->message),
		wParam	(msg->wParam),
		lParam	(msg->lParam)
	{}
	HEVENT(const CWPRETSTRUCT *msg, WPARAM mode) :
		type	(RET), mode(mode),
		hwnd	(msg->hwnd),
		message	(msg->message),
		wParam	(msg->wParam),
		lParam	(msg->lParam),
		result	(msg->lResult)
	{}

};

extern "C" __declspec(dllexport) Shared *SetHook(HWND hWnd, int thread);
