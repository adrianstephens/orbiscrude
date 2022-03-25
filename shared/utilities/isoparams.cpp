#include <stdio.h>
#include <windows.h>

int main(int argc, char *argv[])
{
	char		*line	= GetCommandLine();
	printf("command line: %s\n", line);

	char		*line2	= (char*)malloc(strlen(line) + 2);
	line2[0] = '-';
	strcpy(line2 + 1, line);

	PROCESS_INFORMATION	pi;
	STARTUPINFO			sui;
	memset(&sui, 0, sizeof(STARTUPINFO));
	sui.cb = sizeof(STARTUPINFO);

#if 0
	for (int i = 10; i; i--) {
		printf("%i...", i);
		Sleep(1000);
	}
#endif
	
	CreateProcess(NULL, line2, NULL, NULL, TRUE, 0, NULL, NULL, &sui, &pi);
	WaitForSingleObject(pi.hProcess, INFINITE);
	return EXIT_SUCCESS;
}