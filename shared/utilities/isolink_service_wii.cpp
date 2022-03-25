#include <winsock2.h>
#include <ws2tcpip.h>
#include "..\platforms\wii\utils\link_driver.h"
#include "..\platforms\pc\windows\service.h"

class IsolinkService : public Service<IsolinkService> {
	HIO_host	hio;

public:
	void CtrlHandler(DWORD op) {
		switch (op) { 
			case SERVICE_CONTROL_PAUSE:
				UpdateService(SERVICE_PAUSE_PENDING);
				//--
				UpdateService(SERVICE_PAUSED);
				break; 

			case SERVICE_CONTROL_CONTINUE:
				UpdateService(SERVICE_CONTINUE_PENDING);
				//--
				UpdateService(SERVICE_RUNNING);
				break;

			case SERVICE_CONTROL_SHUTDOWN:
				//--
				break;

			case SERVICE_CONTROL_STOP:
				UpdateService(SERVICE_STOP_PENDING);
				hio.stop();
				UpdateService(SERVICE_STOPPED);
				break;

			default:
				break;
		}
	}
	void Start() {
		if (hio.start() && UpdateService(SERVICE_RUNNING))
			hio.threadproc();
	}


	IsolinkService() : Service(
		"isolink_wii",
		SERVICE_WIN32_OWN_PROCESS,
		SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_PAUSE_CONTINUE | SERVICE_ACCEPT_SHUTDOWN,
		0
	) {}
};


int main(int argc, char *argv[], char *envp[])
{
	if (argc > 1 && strcmp(argv[1], "install") == 0) {
		char		path[_MAX_PATH + 1];
		GetModuleFileName(0, path, sizeof(path));

		if (SC_HANDLE scm = OpenSCManager(0, 0, SC_MANAGER_CREATE_SERVICE)) {
			if (SC_HANDLE service = CreateService(
				scm,
				"isolink_wii", "isolink_wii",
				SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
				SERVICE_AUTO_START, SERVICE_ERROR_IGNORE, path,
				0, 0, 0, 0, 0
			)) {
				CloseServiceHandle(service);
				CloseServiceHandle(scm);
				printf("Service Was Installed Successfully\n");
				return 0;
			}
			CloseServiceHandle(scm);
		}
		if (GetLastError() == ERROR_SERVICE_EXISTS) {
			printf("Service Already Exists.\n");
		} else {
			printf("Service Was not Installed Successfully. Error Code %d\n", GetLastError());
		}

	} else {
		IsolinkService().Dispatch();
	}

	return -1;
}