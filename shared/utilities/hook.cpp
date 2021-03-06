#include <Windows.h>
#include "hook.h"

#pragma bss_seg("SHARED")
Shared shared;
#pragma bss_seg()
#pragma comment(linker, "/section:SHARED,RWS")  

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
	switch (ul_reason_for_call) {
		case DLL_PROCESS_ATTACH:
			shared.hInstance = hModule;
		case DLL_THREAD_ATTACH:
		case DLL_THREAD_DETACH:
		case DLL_PROCESS_DETACH:
			break;
	}
	return TRUE;
}

template<int I, typename M> LRESULT CALLBACK Proc(int nCode, WPARAM wParam, LPARAM lParam) {
	if (nCode == HC_ACTION) {
		HEVENT          event((M*)lParam, wParam);
		COPYDATASTRUCT  cds = {0, DWORD(sizeof(event)), &event};
		SendMessage(shared.hWnd, WM_COPYDATA, 0, (LPARAM)&cds);
	}
	return CallNextHookEx(shared.hook[I], nCode, wParam, lParam);
}

//extern "C"
__declspec(dllexport) Shared *SetHook(HWND hWnd, int thread) {
	shared.hWnd = hWnd;
	shared.hook[0] = SetWindowsHookEx(WH_GETMESSAGE,		Proc<0, MSG			>, shared.hInstance, thread);
	shared.hook[1] = SetWindowsHookEx(WH_CALLWNDPROC,		Proc<1, CWPSTRUCT	>, shared.hInstance, thread);
	shared.hook[2] = SetWindowsHookEx(WH_CALLWNDPROCRET,	Proc<2, CWPRETSTRUCT>, shared.hInstance, thread);
	return &shared;
}



