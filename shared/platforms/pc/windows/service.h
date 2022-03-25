#ifndef SERVICE_H
#define SERVICE_H

//-----------------------------------------------------------------------------
//	Service
//-----------------------------------------------------------------------------

template<typename T> class Service {
	char					*name;
	SERVICE_STATUS_HANDLE	handle;			// the handle to the service that is registered
	SERVICE_STATUS			status;			// maintains the status of the function

	static T*&	me()							{ static T *t; return t; }

public:

	static void	WINAPI _CtrlHandler(DWORD op) {
		me()->CtrlHandler(op);
	}

	void Main(DWORD argc, LPTSTR *argv) {
		UpdateService(SERVICE_START_PENDING);
		handle = RegisterServiceCtrlHandler(name, _CtrlHandler);
		if (!handle)
			return;
		static_cast<T*>(this)->Start();
		UpdateService(SERVICE_STOPPED);
	}
	static void	WINAPI _Main(DWORD argc, LPTSTR *argv) {
		me()->Main(argc, argv);
	}

	DWORD Dispatch() {
		SERVICE_TABLE_ENTRY dispatcher[2] = {
			{name,	_Main	},
			{0,		0		}
		};
		return StartServiceCtrlDispatcher(dispatcher) ? 0 : GetLastError();
	}

	bool UpdateService(DWORD s) {
		status.dwCurrentState = s;
		return !!SetServiceStatus(handle, &status);
	}

	Service(char *_name, DWORD dwServiceType, DWORD dwControlsAccepted, DWORD dwWaitHint) : name(_name) {
		me()	= static_cast<T*>(this);

		// setting the service current status - before registering the service
		status.dwCheckPoint				= 0;
		status.dwControlsAccepted		= dwControlsAccepted;
		status.dwCurrentState			= SERVICE_START_PENDING;
		status.dwServiceSpecificExitCode= 0;
		status.dwServiceType			= dwServiceType;
		status.dwWaitHint				= dwWaitHint;
		status.dwWin32ExitCode			= 0;
	}

};

#endif	// SERVICE_H
