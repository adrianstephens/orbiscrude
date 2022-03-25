#include <stdio.h>
#include <windows.h>

char	start_msg[] = "Started scanning for changes\r\n";

int main(int argc, char *argv[]) {
	const char	*dir		= argv[1];
	const char	*file		= argv[2];
	DWORD		ret_size;
	HANDLE		hFile;

	DWORD		change_mask	= FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE;

//	HANDLE		hChange		= FindFirstChangeNotification(dir, TRUE, change_mask);
//	if (hChange == INVALID_HANDLE_VALUE)
//		ExitProcess(GetLastError());

	hFile = CreateFile(file, 
		FILE_APPEND_DATA, 
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		OPEN_ALWAYS,
		FILE_ATTRIBUTE_NORMAL,
		NULL
	);
	WriteFile(hFile, start_msg, strlen(start_msg), &ret_size, NULL);
	CloseHandle(hFile);

	HANDLE hDir = CreateFile(dir, 
		FILE_LIST_DIRECTORY, 
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING,
		FILE_FLAG_BACKUP_SEMANTICS,
		NULL
	);

	for (;;) {//WaitForSingleObject(hChange, INFINITE) == WAIT_OBJECT_0;) {

		DWORD	buffer[1024];

		if (ReadDirectoryChangesW(hDir, buffer, 4096, TRUE, change_mask, &ret_size, NULL, NULL)) {

			hFile = CreateFile(file, 
				FILE_APPEND_DATA, 
				FILE_SHARE_READ | FILE_SHARE_WRITE,
				NULL,
				OPEN_ALWAYS,
				FILE_ATTRIBUTE_NORMAL,
				NULL
			);

			for (FILE_NOTIFY_INFORMATION *fn = (FILE_NOTIFY_INFORMATION*)buffer;;fn = (FILE_NOTIFY_INFORMATION*)((char*)fn + fn->NextEntryOffset)) {
				char	out[1024], filename[1024];
				for (int i = 0, n = fn->FileNameLength / 2; i < n; i++)
					filename[i] = char(fn->FileName[i]);
				filename[fn->FileNameLength / 2] = 0;

				if (!strstr(filename, ".svn") && !strchr(filename, '~')) {
					switch (fn->Action) {
						case FILE_ACTION_ADDED:
							sprintf(out, "Added: %s\r\n", filename);
							break;
						case FILE_ACTION_REMOVED:
							sprintf(out, "Removed: %s\r\n", filename);
							break;
						case FILE_ACTION_MODIFIED:
							sprintf(out, "Modified: %s\r\n", filename);
							break;
						case FILE_ACTION_RENAMED_OLD_NAME:
							sprintf(out, "Renamed: %s", filename);
							break;
						case FILE_ACTION_RENAMED_NEW_NAME:
							sprintf(out, " to %s\r\n", filename);
							break;
					}
					WriteFile(hFile, out, strlen(out), &ret_size, NULL);

				}
				if (fn->NextEntryOffset == 0)
					break;
			}
//			WriteFile(hFile, buffer, ret_size, &ret_size, NULL);

			CloseHandle(hFile);

		}

//		CloseHandle(hDir);
//		if (!FindNextChangeNotification(hChange))
//			break; 
	}

	printf("isowatch terminating\n");

//	FindCloseChangeNotification(hChange);
	return 0;
}
