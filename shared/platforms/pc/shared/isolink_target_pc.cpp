#include "base/strings.h"
#include "systems/communication/isolink_target.h"

extern "C" { const char *isolink_platform_pc = "pc"; }

HMODULE start_process(const char *exe) {
	PROCESS_INFORMATION	pi;
	STARTUPINFOA		si;
	iso::clear(si);
	iso::clear(pi);
	si.cb			= sizeof(si);
	si.dwFlags		= STARTF_USESHOWWINDOW;
	si.wShowWindow	= SW_HIDE;

	return CreateProcessA(NULL, (char*)exe, NULL, NULL, FALSE, CREATE_NO_WINDOW | DETACHED_PROCESS, NULL, NULL, &si, &pi) ? (HMODULE)pi.hProcess : 0;
}

HMODULE find_load_library(const char *dll, const char *envvar, const char *sdkpath) {
	bool	exe		= stricmp(strchr(dll, '.'), ".exe") == 0;
	HMODULE	handle	= exe ? start_process(dll) : LoadLibraryA(dll);

	if (!handle) {
		const char* path	= getenv("path");
		const char* sdk		= envvar ? getenv(envvar) :0;
		if ((sdk || sdkpath) && path) {
			iso::string	s("path");
			s << path << ';';
			if (sdk) {
				s << sdk;
				if (sdkpath)
					s << "\\";
			}
			if (sdkpath)
				s <<  sdkpath;
			_putenv(s);
			handle	= exe ? start_process(dll) : LoadLibraryA(dll);
		}
	}
	return handle;
}

//-----------------------------------------------------------------------------

struct getaddrinfo_params {
	const char		*node;
	const char		*service;
	const addrinfo	*hints;
	addrinfo		**res;
	volatile int	*signal;
};

DWORD WINAPI getaddrinfo_async_threadproc(void *param) {
	getaddrinfo_params	*params	= (getaddrinfo_params*)param;

	volatile int signal	= 0;
	params->signal		= &signal;

	addrinfo	**pres	= params->res;
	addrinfo	*res	= 0;
	int			r		= getaddrinfo(params->node, params->service, params->hints, &res);

	if (signal)
		freeaddrinfo(res);
	else
		*pres = res;
	return (DWORD)r;
}

int getaddrinfo_async(const char *node, const char *service, const addrinfo *hints, addrinfo **res, int timeout) {
	getaddrinfo_params	params;
	params.node		= node;
	params.service	= service;
	params.hints	= hints;
	params.res		= res;
	params.signal	= 0;

	HANDLE	thread = CreateThread(NULL, 8192, (LPTHREAD_START_ROUTINE)getaddrinfo_async_threadproc, &params, 0, NULL);
	bool	timedout;
	while ((timedout = (WaitForSingleObject(thread, timeout) != WAIT_OBJECT_0)) && !params.signal)
		Sleep(0);

	if (timedout) {
		*params.signal	= 1;
		CloseHandle(thread);
		return -1;
	}
	DWORD ret = -1;
	GetExitCodeThread(thread, &ret);
	CloseHandle(thread);
	return ret;
}

//-----------------------------------------------------------------------------

static struct isolink_host_pc : isolink_host_enum {

	struct target {
		target		*next;
		const char	*name;
		target(const char *_name, target *_next) : name(strdup(_name)), next(_next)	{}
	};

	virtual	bool enumerate(isolink_enum_host_callback *callback, isolink_enum_flags flags, void *param) {
		if (flags == ISOLINK_ENUM_PLATFORMS)
			return callback("pc", isolink_platform_pc, param);

		static struct discover {
			target *targets;
			static DWORD WINAPI threadproc(discover *me) {
				isolink_handle_t	h = isolink_broadcast(1800, 1801, "pc", 2);
				char	name[64];
				while (_isolink_discover(h, name)) {
					bool	got = false;
					for (target *p = me->targets; !got && p; p = p->next)
						got = strcmp(p->name, name) == 0;
					if (!got)
						me->targets = new target(name, me->targets);
				}
				return 0;
			}
			discover() : targets(0) {
				CreateThread(NULL, 8192, (LPTHREAD_START_ROUTINE)threadproc, this, 0, NULL);
			}
		} discovered;

		for (target *p = discovered.targets; p; p = p->next)
			(*callback)(p->name, 0, param);
		return false;
	}

	virtual isolink_platform_t resolve(const char *target, const char *service, const addrinfo *hints, addrinfo **result) {
		if (strcmp(target, "pc") == 0) {
			target = "localhost";
		} else if (strncmp(target, "pc@", 3) == 0) {
			target += 3;
		} else {
			return isolink_platform_invalid;
		}
		return getaddrinfo_async(target, service, hints, result, 100) ? isolink_platform_invalid : isolink_platform_pc;
//		return getaddrinfo(target, service, hints, result) ? isolink_platform_invalid : isolink_platform_pc;
	}
	isolink_host_pc() {}
} pc;

