#ifndef WIN_FILE_H
#define WIN_FILE_H

#include "stream.h"

namespace iso {

class win_file : public stream_defaults<win_file> {
protected:
	struct win_streamptr {
		LARGE_INTEGER	x;
		win_streamptr(streamptr i)			{ x.QuadPart = i;		}
		LARGE_INTEGER*	operator&() 		{ return &x;			}
		operator LARGE_INTEGER()	const	{ return x;				}
		operator streamptr()		const	{ return x.QuadPart;	}
	};
	HANDLE	h;
public:
	win_file(HANDLE _h)	: h(_h)	{}
	void		join(HANDLE _h)	{ h = _h; }
	bool		exists()		{ return h != INVALID_HANDLE_VALUE;	}
	streamptr	tell()			{ win_streamptr i(0); SetFilePointerEx(h, i, &i, FILE_CURRENT); return i; }
	void		_seek(streamptr offset, int origin)	{ SetFilePointerEx(h, win_streamptr(offset), NULL, origin); }
	void		seek(streamptr offset)		{ _seek(offset, SEEK_SET); }
	void		seek_cur(streamptr offset)	{ _seek(offset, SEEK_CUR); }
	void		seek_end(streamptr offset)	{ _seek(offset, SEEK_END); }
	HANDLE		clone()			{
		HANDLE	h2;
		return DuplicateHandle(GetCurrentProcess(), h, GetCurrentProcess(), &h2, 0, FALSE, DUPLICATE_SAME_ACCESS) ? h2 : INVALID_HANDLE_VALUE;
	}
	operator HANDLE() const	{ return h; }
};

class win_filereader : public win_file  {
public:
	win_filereader(HANDLE h)		: win_file(h)	{}
	win_filereader(const char *filename
		, DWORD					access		= GENERIC_READ
		, DWORD					share		= FILE_SHARE_READ
		, SECURITY_ATTRIBUTES*	security	= 0
		, DWORD					disposition	= OPEN_EXISTING
		, DWORD					flags		= FILE_ATTRIBUTE_NORMAL
		, HANDLE				htemplate	= 0)
		: win_file(CreateFileA(filename, access, share, security, disposition, flags, htemplate)) {}
	win_filereader(const wchar_t *filename
		, DWORD					access		= GENERIC_READ
		, DWORD					share		= FILE_SHARE_READ
		, SECURITY_ATTRIBUTES*	security	= 0
		, DWORD					disposition	= OPEN_EXISTING
		, DWORD					flags		= FILE_ATTRIBUTE_NORMAL
		, HANDLE				htemplate	= 0)
		: win_file(CreateFileW(filename, access, share, security, disposition, flags, htemplate)) {}

	~win_filereader()										{ CloseHandle(h); }
	size_t			readbuff(void *buffer, size_t size)		{ DWORD read; return ReadFile(h, buffer, DWORD(size), &read, NULL) ? read : 0; }
	int				getc()									{ uint8 c; return readbuff(&c, 1) ? c : EOF; }
};

class win_filewriter : public win_file  {
public:
	win_filewriter(HANDLE h)		: win_file(h)	{}
	win_filewriter(const char *filename
		, DWORD					access		= GENERIC_WRITE
		, DWORD					share		= 0
		, SECURITY_ATTRIBUTES*	security	= 0
		, DWORD					disposition	= CREATE_ALWAYS
		, DWORD					flags		= FILE_ATTRIBUTE_NORMAL
		, HANDLE				htemplate	= 0)
		: win_file(CreateFileA(filename, access, share, security, disposition, flags, htemplate)) {}
	win_filewriter(const wchar_t *filename
		, DWORD					access		= GENERIC_WRITE
		, DWORD					share		= 0
		, SECURITY_ATTRIBUTES*	security	= 0
		, DWORD					disposition	= CREATE_ALWAYS
		, DWORD					flags		= FILE_ATTRIBUTE_NORMAL
		, HANDLE				htemplate	= 0)
		: win_file(CreateFileW(filename, access, share, security, disposition, flags, htemplate)) {}
	~win_filewriter()											{ CloseHandle(h); }
	size_t			writebuff(const void *buffer, size_t size)	{ DWORD wrote; return WriteFile(h, buffer, DWORD(size), &wrote, NULL) ? wrote : 0; }
	int				putc(int c)									{ uint8 v = c; return writebuff(&v, 1); }
};

typedef reader_mixout<win_filereader> WinFileInput;
typedef writer_mixout<win_filewriter> WinFileOutput;

} // namespace iso

#endif // WIN_FILE_H
