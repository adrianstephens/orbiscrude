#include <excpt.h>
#include <windows.h>
#include "base/defs.h"
#include "base/pointer.h"

#pragma section(".CRT$XIA", long, read)
#pragma section(".CRT$XIZ", long, read)
#pragma section(".CRT$XCA", long, read)
#pragma section(".CRT$XCZ", long, read)
#pragma section(".CRT$XTA", long, read)
#pragma section(".CRT$XTZ", long, read)

typedef int  (__cdecl *_PIFV)(void);
typedef void (__cdecl *_PVFV)(void);

int execute(_PIFV *i, _PIFV *e) {
	while (i < e) {
		if (_PIFV f = *i++) {
			if (int r = f())
				return r;
		}
	}
	return 0;
}
void execute(_PVFV *i, _PVFV *e) {
	while (i < e) {
		if (_PVFV f = *i++)
			f();
	}
}

extern "C" {
using namespace iso;

extern IMAGE_DOS_HEADER __ImageBase;
extern _PVFV*			__onexitbegin;
extern _PVFV*			__onexitend;

// C initializers
extern __declspec(allocate(".CRT$XIA")) _PIFV __xi_a[];
extern __declspec(allocate(".CRT$XIZ")) _PIFV __xi_z[];

// C++ initializers
extern __declspec(allocate(".CRT$XCA")) _PVFV __xc_a[];
extern __declspec(allocate(".CRT$XCZ")) _PVFV __xc_z[];

// C terminators
extern __declspec(allocate(".CRT$XTA")) _PVFV __xt_a[];
extern __declspec(allocate(".CRT$XTZ")) _PVFV __xt_z[];

struct _startupinfo { int newmode; } ;
extern void	__cdecl __security_init_cookie(void);

void crt_init() {
	static int	static_int;
	__security_init_cookie();
    __onexitbegin = __onexitend = (_PVFV*)EncodePointer((_PVFV*)(-1));

	base_fixed<void>::base = malloc(1);
	base_select<void, 1>::base[0] = &static_int;
	base_select<void, 1>::base[1] = base_fixed<void>::base;
	
	execute(__xi_a, __xi_z);
	execute(__xc_a, __xc_z);
}

#ifdef _CONSOLE
//-------------------------------------------------------------------------------------------------
//	CONSOLE
//-------------------------------------------------------------------------------------------------
extern int __getmainargs(int *argc, char ***argv, char ***envp, int wild, _startupinfo *startup);
extern int main(int argc, char **argv, char **envp);

int iso_startup() {
	crt_init();
	int				argc;
	char			**argv, **envp;
	_startupinfo	startup = {0};
	__getmainargs(&argc, &argv, &envp, 0, &startup);
	exit(main(argc, argv, envp));
}

#else
//-------------------------------------------------------------------------------------------------
//	GUI
//-------------------------------------------------------------------------------------------------
extern int __stdcall WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR cmdline, int nCmdShow);

//int iso_startup() {
int WinMainCRTStartup() {
	crt_init();
	STARTUPINFOW StartupInfo;
	GetStartupInfoW(&StartupInfo);

	LPSTR	cmdline = GetCommandLine();
	for (bool quote = false; *cmdline && (quote || *cmdline > ' '); ++cmdline) {
		if (*cmdline == '"')
			quote = !quote;
	}

	while (*cmdline && *cmdline <= ' ')
		cmdline++;

	exit(WinMain(
		(HINSTANCE)&__ImageBase,
		NULL,
		cmdline,
		StartupInfo.dwFlags & STARTF_USESHOWWINDOW ? StartupInfo.wShowWindow : SW_SHOWDEFAULT
	));
}
#endif
}
