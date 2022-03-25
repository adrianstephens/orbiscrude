#ifndef FILENAME_H
#define FILENAME_H

#include "base/strings.h"

#if defined PLAT_X360 || defined PLAT_PC || defined PLAT_XONE

#define USE_FILENAME_WINDOWS
#define DIRECTORY_SEP	'\\'
#define DIRECTORY_ALL	"*.*"

#else

#include <sys/stat.h>
#define USE_FILENAME_UNIX
#undef _MAX_PATH
#define _MAX_PATH		260
#define _MAX_DIR		256
#define _MAX_FNAME		256
#define _MAX_EXT		256
#define DIRECTORY_SEP	'/'
#define DIRECTORY_ALL	"*"

#endif
#define _MAX_VOLUME		16

namespace iso {

enum class FILEACCESS : uint8 {
	READ		= 4,	//read
	WRITE		= 2,	//write
	EXECUTE		= 1,	//execute
	RW			= READ | WRITE,
	RWX			= READ | WRITE | EXECUTE,
};

enum class FILEMODE : uint16 {
	FMT			= 0xF000,	//format mask
	FSOCK		= 0xA000,	//socket
	FLNK		= 0xC000,	//symbolic link
	FREG		= 0x8000,	//regular file
	FBLK		= 0x6000,	//block device
	FDIR		= 0x4000,	//directory
	FCHR		= 0x2000,	//character device
	FIFO		= 0x1000,	//fifo

	SUID		= 0x0800,	//SUID
	SGID		= 0x0400,	//SGID
	SVTX		= 0x0200,	//sticky bit

	RWXU		= 0x01C0,	//user mask
	RUSR		= 0x0100,	//read
	WUSR		= 0x0080,	//write
	XUSR		= 0x0040,	//execute

	RWXG		= 0x0038,	//group mask
	RGRP		= 0x0020,	//read
	WGRP		= 0x0010,	//write
	XGRP		= 0x0008,	//execute

	RWXO		= 0x0007,	//other mask
	ROTH		= 0x0004,	//read
	WOTH		= 0x0002,	//write
	XOTH		= 0x0001,	//execute
};


inline FILEMODE	operator|(FILEMODE a, FILEMODE b) { return FILEMODE((int)a | (int)b); }
inline bool		operator&(FILEMODE a, FILEMODE b) { return !!((int)a & (int)b); }
inline FILEMODE other(FILEACCESS a)	{ return FILEMODE((int)a * (int)FILEMODE::XOTH); }
inline FILEMODE group(FILEACCESS a)	{ return FILEMODE((int)a * (int)FILEMODE::XGRP); }
inline FILEMODE user(FILEACCESS a)	{ return FILEMODE((int)a * (int)FILEMODE::XUSR); }
inline FILEMODE all(FILEACCESS a)	{ return FILEMODE((int)a * int(FILEMODE::XOTH | FILEMODE::XGRP | FILEMODE::XUSR)); }

struct filetime_t {
	uint64 v;
	filetime_t()			: v(0) {}
	filetime_t(uint64 v)	: v(v) {}
	operator int64() const { return (int64)v; }
#ifdef _WINDEF_
	filetime_t(const FILETIME &v) : v((uint64&)v) {}
	operator FILETIME() const { return *(FILETIME*)this; }
#endif
};

#ifdef _WINDEF_

struct file_stats : WIN32_FILE_ATTRIBUTE_DATA {
	file_stats(const char* f)	{ GetFileAttributesExA(f, GetFileExInfoStandard, this); }
	bool		exists()		const	{ return dwFileAttributes != INVALID_FILE_ATTRIBUTES; }
	bool		is_dir()		const	{ return dwFileAttributes != INVALID_FILE_ATTRIBUTES && (dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY); }
	bool		is_file()		const	{ return dwFileAttributes != INVALID_FILE_ATTRIBUTES && !(dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY); }
	uint64		length()		const	{ return dwFileAttributes != INVALID_FILE_ATTRIBUTES ? (uint64(nFileSizeHigh) << 32) | nFileSizeLow : 0; }
	filetime_t	write_time()	const	{ return dwFileAttributes != INVALID_FILE_ATTRIBUTES ? filetime_t(ftLastWriteTime) : filetime_t(); }
	filetime_t	create_time()	const	{ return dwFileAttributes != INVALID_FILE_ATTRIBUTES ? filetime_t(ftCreationTime) : filetime_t(); }
	filetime_t	access_time()	const	{ return dwFileAttributes != INVALID_FILE_ATTRIBUTES ? filetime_t(ftCreationTime) : filetime_t(); }
	int			user_mode()		const	{ return dwFileAttributes & FILE_ATTRIBUTE_READONLY ? 4 : 6; }
	int			group_mode()	const	{ return user_mode(); }
	int			other_mode()	const	{ return user_mode(); }
	int			all_modes()		const	{ return user_mode() * 0x49; }
	int			mode()			const	{ return all_modes() | (is_dir() ? (int)FILEMODE::FDIR : 0); }
	int			uid()			const	{ return 0; }
	int			gid()			const	{ return 0; }
};

#elif defined PLAT_PS4

struct file_stats : SceKernelStat {
	bool	valid;
	file_stats(const char* f) : valid(sceKernelStat(f, this) == 0); )
	bool		exists()		const	{ return valid; }
	bool		is_dir()		const	{ return valid && (st_mode & SCE_KERNEL_S_IFDIR); }
	bool		is_file)		const	{ return valid && !(st_mode & SCE_KERNEL_S_IFDIR); }
	uint64		length()		const	{ return valid ? st_size : 0; }
	filetime_t	write_time()	const	{ return valid ? st_mtime : 0; }
};

#else

struct file_stats : stat {
	bool	valid;
	file_stats(const char* f) : valid(lstat(f, this) == 0) {}
	bool		exists()		const	{ return valid; }
	bool		is_dir()		const	{ return valid && (st_mode & S_IFDIR); }
	bool		is_file()		const	{ return valid && !(st_mode & S_IFDIR); }
	uint64		length()		const	{ return valid ? st_size : 0; }
	filetime_t	write_time()	const	{ return valid ? st_mtime : 0; }
};

#endif

iso_export bool			matches(const char *p, const char *s);
iso_export bool			exists(const char *f);
iso_export bool			is_dir(const char *f);
iso_export bool			is_file(const char *f);
iso_export bool			is_wild(const char *f);
iso_export uint64		filelength(const char *f);
iso_export filetime_t	filetime_write(const char *f);
iso_export filetime_t	filetime_create(const char *f);

iso_export bool			delete_file(const char *f);
iso_export bool			create_dir(const char *f);
iso_export bool			delete_dir(const char *f);

class filename : public fixed_string<512> {
	typedef fixed_string<512>	B;
public:
	typedef fixed_string<_MAX_VOLUME>	drive_t;
	typedef fixed_string<_MAX_EXT>		ext_t;

	iso_export static filename	temp(const char *prefix = 0);
	iso_export static filename	cleaned(const char *fn);

	filename() {}
	iso_export filename(const char *s);
	filename(const char16 *s)								: B(s)	{}
	template<typename T> filename(const string_base<T> &s)	: B(s)	{}
	template<typename T> filename(const string_getter<T> &s): B(s)	{}

	iso_export bool			is_url()					const;
	iso_export bool			is_wild()					const;
	iso_export bool			is_relative()				const;
	iso_export drive_t		drive()						const;
	iso_export filename		dir()						const;
	iso_export filename		name()						const;
	iso_export ext_t		ext()						const;
	iso_export filename		name_ext()					const;
	iso_export filename		dir_name_ext()				const;

	const char*				ext_ptr()					const	{ const char *d = rfind('.'); return d && !string_find(d, DIRECTORY_SEP) ? d : 0; }
	const char*				name_ext_ptr()				const	{ const char *d = rfind(DIRECTORY_SEP); return d ? d + 1 : p; }

	iso_export filename&	cleanup();
	iso_export filename&	rem_dir();
	iso_export filename&	rem_first();
	iso_export filename&	add_dir(const count_string &s);
	iso_export filename&	set_dir(const count_string &s);
	iso_export filename&	add_ext(const char *e);
	iso_export filename&	set_ext(const char *e);
	filename&				add_dir(const char *d)				{ if (d) add_dir(count_string(d, strlen(d))); return *this; }
	filename&				set_dir(const char *d)				{ return set_dir(count_string(d, strlen(d))); }

	iso_export filename		relative(const char *f)		const;
	iso_export bool			matches(const char *s)		const;
	iso_export bool			matched(filename &out, const char *spec0, const char *spec1) const;
	iso_export filename		matched(const char *spec0, const char *spec1) const;
	iso_export filename		relative_to(const char *f)	const;
	filename				absolute(const char *f)		const	{ return filename(f).relative(*this); }
	filename				convert_to_backslash()		const	{ filename r; replace(r.begin(), begin(), "/", "\\"); return r; }
	filename				convert_to_fwdslash()		const	{ filename r; replace(r.begin(), begin(), "\\", "/"); return r; }
	bool					exists()					const	{ return iso::exists(*this); }
	static filename			fix(const char *p);
	filename				fix()						const	{ return fix(p); }

	filename				operator/(const char* d)	const	{ return filename(*this).add_dir(d); }
};

iso_export filename		get_cwd();
iso_export bool			set_cwd(const filename &f);
iso_export filename		get_exec_path();
iso_export filename		get_exec_dir();
iso_export filename 	get_temp_dir();

class push_cwd {
	filename prev;
public:
	push_cwd(const char *dir)	{ prev = get_cwd(); set_cwd(dir); }
	~push_cwd()					{ set_cwd(prev); }
};

struct _cwd {
	bool	operator=(const char *dir)	{ return set_cwd(dir); }
	operator filename() const			{ return get_cwd(); }
};
extern _cwd cwd;

#ifdef PLAT_PC
HMODULE load_library(const char *name, const char *env, const char *dir);
#endif

}//namespace iso

#endif//FILENAME_H
