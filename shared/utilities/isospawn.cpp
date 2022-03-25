#include <stdio.h>
#include <windows.h>
#include <psapi.h>

#include "windows\nt.h"

using namespace iso;

const char usage[] =
"usage: isospawn <options> cmdline\n"
"action: spawns a process\n"
"options:\n"
"    -n<numprocesses>:    limits number of concurrent processes.\n"
"    -w[+|-]              if numprocesses is exceeded, wait or exit.\n"
"    -j[+|-]              specify isospawn was run from a job.\n"
"    -p[+|-]              check cmdline parameters.\n"
"    -k[+|-]              kills the process.\n"
;

typedef NTSTATUS (NTAPI *_NtQueryInformationProcess)(
	HANDLE ProcessHandle,
	DWORD ProcessInformationClass,
	PVOID ProcessInformation,
	DWORD ProcessInformationLength,
	PDWORD ReturnLength
);

struct PROCESS_BASIC_INFORMATION {
	LONG		ExitStatus;
	NT::_PEB	*PebBaseAddress;
	ULONG_PTR	AffinityMask;
	LONG		BasePriority;
	ULONG_PTR	UniqueProcessId;
	ULONG_PTR	ParentProcessId;
};

struct Process {
	HANDLE h;

	Process(HANDLE _h) : h(_h) {}

	bool Read(const void *addr, void *dest, size_t len) {
		return !!ReadProcessMemory(h, addr, dest, len, NULL);
	}

	template<typename T> bool	Read(const void *addr, T &t)	{ return Read(addr, &t, sizeof(T)); }
	template<typename T> T		Get(const T *addr)				{ T t; Read(addr, t); return t; }

	string16 GetString(const NT::_UNICODE_STRING &u) {
		string16		s;
		Read(u.Buffer, s.alloc(u.Length / 2), u.Length);
		s[u.Length / 2] = 0;
		return s;
	}
	string16 GetString(const NT::_UNICODE_STRING *addr) {
		return GetString(Get(addr));
	}

};

string GetProcessCommandLine(HANDLE hProc)
{
	static _NtQueryInformationProcess NtQueryInformationProcess = (_NtQueryInformationProcess)GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtQueryInformationProcess");

	PROCESS_BASIC_INFORMATION pbi;
	NtQueryInformationProcess(hProc, 0, &pbi, sizeof(pbi), NULL);

	Process	proc(hProc);
	return proc.GetString(&proc.Get(&pbi.PebBaseAddress->ProcessParameters)->CommandLine);
}

count_string NextArg(const char *a) {
	const char *p = a = skip_whitespace(a);
	if (a[0] == '"') {
		do {
			++a;
			if (a[0] == '\\' && a[1])
				a += 2;
		} while (*a != 0 && *a != '"');
		a += int(*a != 0);
	} else {
		while (*a && !is_whitespace(*a))
			++a;
	}
	return count_string(p, a);
}

bool CompareCommandlines(const char *a, const char *b) {
	count_string	a1 = NextArg(a);
	count_string	b1 = NextArg(b);

	for (;;) {
		a1 = NextArg(a1.end());
		b1 = NextArg(b1.end());

		if (a1.blank())
			return b1.blank();
		if (b1.blank())
			return false;

		if (a1 != b1)
			return false;
	}
}

int SplitCommandLine(char *input, char *argv[])
{
	int		argc		= 0;

	for (;;) {
		char	c;
		while ((c = *input) && c == ' ')
			input++;

		if (c == 0)
			break;

		char	end = ' ';
		if (c == '"') {
			input++;
			end = c;
		}

		argv[argc++] = input;

		while ((c = *input) && c != end)
			input++;

		if (c == 0)
			break;

		*input++ = 0;
	}
	return argc;
}

char *get_num(char *line, int *out) {
	int	v = 0;
	char	c;
	while ((c = *line++) && c >= '0' && c <= '9')
		v = v * 10 + c - '0';
	*out = v;
	return line;
}

char *get_flag(char *line, bool *out) {
	if (*line == '-') {
		*out = false;
		line++;
	} else {
		*out = true;
		if (*line == '+')
			line++;
	}
	return line;
}

void get_name(char *filename, char *name) {
	char	*p		= filename, *dir, *ext = 0;
	char	term	= ' ';
	if (p[0] == '"') {
		term = '"';
		p++;
	}
	dir = p;
	while (*p && *p != term) {
		if (*p == '\\' || *p == '/') {
			dir = p + 1;
			ext = 0;
		} else if (*p == '.') {
			ext = p;
		}
		p++;
	}
	if (!ext)
		ext = p;
	memcpy(name, dir, ext - dir);
	name[ext - dir] = 0;
}

int main(int argc, char *argv[]) {
	// usage
	if (argc < 2) {
		printf(usage);
		return EXIT_SUCCESS;
	}

	int			errors		= 0;
	int			num_processes = -1;
	bool		wait		= true;
	bool		checkparams	= false;
	bool		fromjob		= false;
	bool		kill		= false;
	char		*line		= GetCommandLine();
	char		*start		= NULL;
	char		name[256];


	while (*line++ != ' ');

	while (*line) {
		while (*line == ' ')
			++line;

		if (*line == '-' || *line == '/') {
			switch (line[1]) {
				case 'n':	line = get_num(line+2, &num_processes);	break;
				case 'w':	line = get_flag(line+2, &wait);			break;
				case 'p':	line = get_flag(line+2, &checkparams);	break;
				case 'j':	line = get_flag(line+2, &fromjob);		break;
				case 'k':	line = get_flag(line+2, &kill);			break;
				default:
					errors++;
					break;
			}
			continue;
		}

		start = line;
		break;
	}

	get_name(start, name);
//	printf("%s\n", start);

	if (num_processes < 0) {
		num_processes = 1;

	} else {
		DWORD	ids[256], size;
		if (EnumProcesses(ids, sizeof(ids), &size)) {
			for (int i = 0, n = size / sizeof(DWORD); i < n; i++) {
				if (HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | SYNCHRONIZE, FALSE, ids[i])) {
					HMODULE hMod;
					DWORD	size2;

					if (EnumProcessModules(hProc, &hMod, sizeof(hMod), &size2)) {
						char	name2[MAX_PATH];
						GetModuleBaseName(hProc, hMod, name2, sizeof(name2));
						get_name(name2, name2);

						if (istr(name2) == name) {
							if (checkparams && !CompareCommandlines(GetProcessCommandLine(hProc), start))
								continue;

							if (num_processes > 1) {
								num_processes--;
							} else {
								if (kill) {
									printf("Killing PID %i\n", ids[i]);
									CloseHandle(hProc);
									if (hProc = OpenProcess(PROCESS_ALL_ACCESS, 0, ids[i])) {
										DWORD	exit_code;
										GetExitCodeProcess(hProc, &exit_code);
										TerminateProcess(hProc, exit_code);
									}
									continue;
								}
								if (!wait) {
									num_processes = 0;
									break;
								}
								printf("Waiting for PID %i\n", ids[i]);
								errors = WaitForSingleObject(hProc, INFINITE);
							}
						}
					}
					CloseHandle(hProc);
				}
			}
		}
	}

	if (num_processes > 0 && !kill) {
		const DWORD	flags_job	= CREATE_BREAKAWAY_FROM_JOB | CREATE_SUSPENDED | CREATE_NO_WINDOW;
		const DWORD	flags_nojob = CREATE_SUSPENDED | CREATE_NO_WINDOW | DETACHED_PROCESS;

		PROCESS_INFORMATION	pi;
		STARTUPINFO			sui;
		memset(&sui, 0, sizeof(STARTUPINFO));
		sui.cb = sizeof(STARTUPINFO);


		if (!CreateProcess(NULL, start, NULL, NULL, FALSE, fromjob ? flags_job : flags_nojob, NULL, NULL, &sui, &pi)
		&& (!fromjob || !CreateProcess(NULL, start, NULL, NULL, FALSE, flags_nojob, NULL, NULL, &sui, &pi))
		) {
			errors = GetLastError();
			printf("Failed to create process: error=0x%08x\n", errors);
			return -1;
		}

		if (ResumeThread(pi.hThread) == (DWORD)-1) {
			errors = GetLastError();
			printf("Failed to start process: error=0x%08x\n", errors);
			return -1;
		}
	}
	return EXIT_SUCCESS;
}