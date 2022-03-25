#include "async_stream.h"

using namespace iso;

#if defined PLAT_X360 || defined PLAT_WIN32

win32_async_filereader::win32_async_filereader(HANDLE hFile) : hFile(hFile), read(-1) {
	if (hFile != INVALID_HANDLE_VALUE) {
		clear(*(OVERLAPPED*)this);
		init_read();
		read	= 0;
		ReadFileEx(hFile, put(), SIZE, this, &_completion);
	}
}

win32_async_filereader::win32_async_filereader(const filename &fn)
	: win32_async_filereader(CreateFileA(fn, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED | FILE_FLAG_NO_BUFFERING, NULL)) {}

win32_async_filereader::~win32_async_filereader() {
	while (read == 0)
		SleepEx(INFINITE, TRUE);
	CloseHandle(hFile);
}

bool win32_async_filereader::GetMore() {
	while (read == 0)
		SleepEx(INFINITE, TRUE);

	if (read == 0xffffffff)
		return false;

	Offset	+= read;
	move_put(read);
	read = 0;

	if (!ReadFileEx(hFile, put(), SIZE, this, &_completion))
		read = 0xffffffff;
	return true;
}

#ifdef PLAT_PC
win32_async_filewriter::win32_async_filewriter(const filename &fn) : wrote(SIZE) {
	hFile = CreateFileA(fn, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED | FILE_FLAG_NO_BUFFERING, NULL);
	if (hFile != INVALID_HANDLE_VALUE) {
		clear(*(OVERLAPPED*)this);
		Offset	-= wrote;
		init_write();
	}
}

win32_async_filewriter::~win32_async_filewriter() {
	while (wrote == 0)
		SleepEx(INFINITE, TRUE);
	if (uint32 write = (uint32)available()) {
		Offset	+= wrote;
		if (WriteFileEx(hFile, get(), write, this, &_completion))
			SleepEx(INFINITE, TRUE);
	}
	CloseHandle(hFile);
}

bool win32_async_filewriter::PutMore() {
	while (wrote == 0)
		SleepEx(INFINITE, TRUE);

	if (wrote == 0xffffffff)
		return false;

	Offset	+= wrote;
	move_get(wrote);
	wrote = 0;

	if (!WriteFileEx(hFile, get(), (uint32)available(), this, &_completion))
		wrote = 0xffffffff;
	return true;
}
#endif

#elif defined PLAT_PS3

ps3_async_filereader::ps3_async_filereader(const filename &fn) : fd(-1), read(0xffffffff) {
	if (cellFsAioInit(fn.dir()) != CELL_OK || cellFsOpen(fn, CELL_FS_O_RDONLY, &fd, NULL, 0) != CELL_OK)
		return;

	init_read();

	offset	= 0;
	buf		= put();
	size	= SIZE;
	read	= 0;
	if (cellFsAioRead(this, &id, &_completion) != CELL_OK) {
		clear_buffers();
		read	= 0xffffffff;
	}
}

ps3_async_filereader::~ps3_async_filereader() {
	while (read == 0)
		sys_timer_usleep(10);
	cellFsClose(fd);
}

bool ps3_async_filereader::GetMore() {
	while (read == 0)
		sys_timer_usleep(10);

	if (read == 0xffffffff)
		return false;

	offset	+= read;
	move_put(read);

	buf		= put();
	size	= SIZE;
	read	= 0;
	if (cellFsAioRead(this, &id, &_completion) == CELL_OK)
		return true;

	read	= 0xffffffff;
	return false;
}

#elif defined PLAT_WII

wii_async_filereader::wii_async_filereader(const filename &fn) {
	if (!DVDOpen(fn, this))
		DVDFileInfo::length = 0;
	init_read();
	state	= STATE_READING;
	offset	= 0;
	read	= min(SIZE, DVDFileInfo::length & ~31);
	DVDReadAsync(this, put(), read, offset, _callback);
}

wii_async_filereader::~wii_async_filereader() {
	while (state == STATE_READING)
		OSYieldThread();
	if (DVDFileInfo::length)
		DVDClose(this);
}

bool wii_async_filereader::GetMore() {
	while (state == STATE_READING || Application::IsError())
		OSYieldThread();

	if (read == 0)
		return false;

	offset		+= read;
	move_put(read + (-read & 28));
	g += -read & 28;

	if (read = min(DVDFileInfo::length - offset, SIZE)) {
		uint32	reada = read + (-read & 31), offseta = offset - (-read & 28);
		state = STATE_READING;
		DVDReadAsync(this, put(), reada, offseta, _callback);
	}
	return true;
}

#endif
