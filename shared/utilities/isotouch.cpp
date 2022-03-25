#include <stdio.h>
#include <windows.h>

int main(int argc, char *argv[])
{
	if (argc < 2) {
		printf("isotouch <filename>\nUpdate the modification date of a file\n");
		return 0;
	}

	const char		*file	= argv[1];
	HANDLE			h		= CreateFile(file, FILE_WRITE_ATTRIBUTES, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if (h == INVALID_HANDLE_VALUE) {
		fprintf(stderr, "isotouch: Failed to open %s\n", file);
		return -1;
	}

	FILETIME	ft;
	SYSTEMTIME	st;
  
	GetSystemTime(&st);
	SystemTimeToFileTime(&st, &ft);
	if (!SetFileTime(h, NULL, NULL, &ft)) {
		fprintf(stderr, "isotouch: Failed to set time on %s\n", file);
		return -1;
	}

	return 0;
}
