#include "directory.h"

#if defined PLAT_PS4
#include <kernel.h>
#endif

#if defined(PLAT_IOS) || defined(PLAT_MAC) || defined(PLAT_PS3) || defined(PLAT_WII) || defined PLAT_ANDROID
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#endif

#if defined PLAT_MAC
#include <mach-o/dyld.h>
#endif

namespace iso {

_cwd cwd;

inline bool IsDirSep(char c)					{ return c == '\\' || c == '/';			}
inline const char *GetDirSep(const char *s)		{ return str(s).find(DIRECTORY_SEP);	}
inline const char *GetDirSepR(const char *s)	{ return str(s).rfind(DIRECTORY_SEP);	}

filename::filename(const char *s) {
	char	*d = begin();
	if (!s || !s[0]) {
		*d = 0;
		return;
	}
	s += int(*s == '"');

#ifdef PLAT_MAC
	if (s[1] == ':') {
		strcpy(d, "/Volumes/");
		d	+= strlen(d);
		*d++= s[0];
		s	+= 2;
	}
#endif

	char	sep		= DIRECTORY_SEP;
#if	DIRECTORY_SEP != '/'
	if (str(s).find("://"))
		sep = '/';
#endif
	char	notsep	= '/' + '\\' - sep;
	while (char c = *s++)
		*d++ = c == notsep ? sep : c;
	d[-int(d[-1] == '"')] = 0;
}

bool filename::is_wild() const {
	return find('*') || find('?');
}

bool filename::is_url() const {
	return find(':') > begin() + 1;
}

iso_export filename	filename::cleaned(const char* p) {
	filename	fn;
	char		*d		= fn.begin();
	int			back	= 0;
	char_set	sep("\\/");
	for (const char *n; n = str(p).find(sep); p = n + 1) {
		size_t	len = n - p;
		if (len == 0) {
			if (d > fn.begin() + 1)
				continue;
		} else {
			if (p[0] == '.') {
				if (len == 1)
					continue;
				if (len == 2 && p[1] == '.' && back--) {
					while (--d > fn.begin() && !sep.test(d[-1]));
					continue;
				}
			}
			back++;
			memcpy(d, p, len);
		}
		d += len + 1;
		d[-1] = DIRECTORY_SEP;
	}
	strcpy(d, p);
	return fn;
}

#ifdef USE_FILENAME_WINDOWS

bool filename::is_relative() const {
	if (IsDirSep((*this)[0]))
		return false;
	const char *p = find(~char_set::alphanum);
	return !p || *p != ':';
}

filename::drive_t filename::drive() const {
	drive_t	d;
	if (!blank() && (*this)[1] == ':')
		d = slice(0, 2);
	return d;
}

// Translate path with device name to drive letters.
filename filename::fix(const char *p) {
#if !defined PLAT_WINRT && !defined PLAT_XONE
	if (p[0] == '\\') {
		char	logical[512];
		if (GetLogicalDriveStringsA(sizeof(logical) - 1, logical)) {
			char	name[256];
			for (char *log = logical; *log; log = string_end(log) + 1) {
				const char drive[] = {*log, ':', 0};
				if (QueryDosDeviceA(drive, name, sizeof(name))) {
					size_t	len = string_len(name);
					if (str(p).begins(name) && p[len] == '\\')
						return filename(drive).add_dir(p + len);
				}
			}
		}
	}
#endif
	return p;
}

#else

bool filename::is_relative() const {
	return !IsDirSep((*this)[0]);
}

filename::drive_t filename::drive() const {
	return drive_t();
}

#endif


const char *fn_end(const char *buff) {
	const char *end = string_end(buff);
	if (string_find(buff + 2, ':')) {
		if (const char *query = string_rfind(buff, '?', end))
			return query;
	}
	return end;
}

filename filename::dir() const {
	filename	t;
	const char	*buff	= *this;
	const char	*end	= fn_end(buff);
	if (const char *sep = str(buff, end).rfind(DIRECTORY_SEP)) {
		memcpy(t, buff, sep - buff);
		t[sep - buff] = 0;
	}
	return t;
}

filename filename::name_ext() const {
	filename	t;
	const char	*buff	= *this;
	const char	*end	= fn_end(buff);
	const char	*sep	= str(buff, end).rfind(DIRECTORY_SEP);

	sep = sep ? sep + 1 : buff;
	memcpy(t, sep, end - sep);
	t[end - sep] = 0;
	return t;
}

filename filename::dir_name_ext() const {
	return *this;
}

filename filename::name() const {
	filename 	t;
	const char	*buff	= *this;
	const char	*end	= fn_end(buff);
	const char	*sep	= str(buff, end).rfind(DIRECTORY_SEP);

	sep = sep ? sep + 1 : buff;
	if (const char *dot = str(sep, end).rfind('.'))
		end = dot;

	memcpy(t, sep, end - sep);
	t[end - sep] = 0;
	return t;
}

filename::ext_t filename::ext() const {
	ext_t		t;
	const char	*buff	= *this;
	const char	*end	= fn_end(buff);
	if (const char *sep = str(buff, end).rfind(DIRECTORY_SEP))
		buff = sep;
	if (const char *dot	= str(buff, end).rfind('.')) {
		memcpy(t, dot, end - dot);
		t[end - dot] = 0;
	}
	return t;
}

filename &filename::cleanup() {
	const char	*p		= *this;
	char		*d		= *this;
	int			back	= 0;
	char_set	sep("\\/");
	for (const char *n; n = str(p).find(sep); p = n + 1) {
		size_t	len = n - p;
		if (len == 0) {
			if (d > begin() + 1)
				continue;
		} else {
			if (p[0] == '.') {
				if (len == 1)
					continue;
				if (len == 2 && p[1] == '.' && back--) {
					while (--d > begin() && !sep.test(d[-1]));
					continue;
				}
			}
			back++;
			memcpy(d, p, len);
		}
		d += len + 1;
		d[-1] = DIRECTORY_SEP;
	}
	strcpy(d, p);
	return *this;
}

filename &filename::rem_dir() {
	while (char *p = (char*)GetDirSepR(*this)) {
		p[0] = 0;
		if (p[1])
			return *this;
	}
	clear();
	return *this;
}

filename &filename::rem_first() {
	char *p = (char*)GetDirSep(begin());
	if (p) {
		strcpy(begin(), p + 1);
	} else {
		clear();
	}
	return *this;
}

filename &filename::add_dir(const count_string &s) {
	ISO_ASSERT(length() + s.length() < max_length());
	char	*p = end();
	for (const char *d = s.begin(), *e = s.end(); d < e;) {
		char	c = *d;
		if (IsDirSep(c)) {
			d += 1;
		} else if (c == '.' && IsDirSep(d[1])) {
			d += 2;
		} else if (c == '.' && d[1] == '.' && IsDirSep(d[2]) && p > begin() && p[IsDirSep(p[-1]) ? -2 : -1] != '.') {
			d += 3;
			if (p > begin())
				--p;
			while (p > begin() && !IsDirSep(*--p));
		} else {
			if (p != begin() && !IsDirSep(p[-1]))
				*p++ = DIRECTORY_SEP;
			do
				*p++ = *d++;
			while (d < e && !IsDirSep(*d));
		}
	}
	*p = 0;
	return *this;
}
filename &filename::set_dir(const count_string &s) {
	const char	*buff	= *this;
	const char	*sep	= GetDirSepR(buff);
	if (sep)
		sep++;
	else
		sep = buff;

	filename	n(sep);

	char	*p = *this;
	for (const char *d = s.begin(), *e = s.end(); d < e;)
		*p++ = *d++;
	if (p != operator char*() && !IsDirSep(p[-1]))
		*p++ = DIRECTORY_SEP;

	strcpy(p, n);
	return *this;
}

filename &filename::add_ext(const char *e) {
	if (e) {
		char	*p = end();
		if (e[0] != '.')
			*p++ = '.';
		strcpy(p, e);
	}
	return *this;
}

filename &filename::set_ext(const char *e) {
	if (e) {
		if (e[0] == '.')
			e++;
		if (e[0] == 0)
			e = 0;
	}
	for (char *p = end(); p > begin() && !IsDirSep(p[-1]); p--) {
		if (p[-1] == '.') {
			if (e)
				strcpy(p, e);
			else
				p[-1] = 0;
			return *this;
		}
	}
	if (e)
		*this << "." << e;
	return *this;
}

filename filename::relative(const char *f) const {
	filename	fn(f);
	if (!fn.is_relative())
		return fn;
	return filename(*this).add_dir(f);
}

filename filename::relative_to(const char *f) const {
	const char	*a	= *this;
	const char	*dir = 0;
	while (simple_compare(ichar(*a), *f) == 0) {
		if (IsDirSep(*a))
			dir = a;
		a++;
		f++;
	}
	if (!dir)
		return *this;

	f -= (a - dir);
	a = dir;

	filename	fn;
	char		*d = fn;
	while (f = GetDirSep(f + 1)) {
		d[0] = d[1] = '.';
		d[2] = DIRECTORY_SEP;
		d += 3;
	};
	strcpy(d, a + 1);
	return fn;
}

bool matches(const char *p, const char *s) {
	if (!s || !s[0])
		return true;
	if (!p)
		return false;
	for (char c; c = *s++; p++) {
		if (c == '*') {
			while ((c = *s++) == '?' || c == '*') {
				if (c == '?' && !*p++)
					return false;
			}
			if (c == 0)
				return true;
			while (p = strchr(p, c)) {
				if (matches(++p, s))
					return true;
			}
			return false;
		}
		if (c == '?' ? *p == 0 : *p != c)
			return false;
	}
	return *p == 0;
}

bool matched(char *d, const char *p, const char *s0, const char *s1) {
	char_set	wild("?*");

	while (const char *w0 = str(s0).find(wild)) {
		const char	*w1 = str(s1).find(wild);
		if (!w1) {
			strcpy(d, s1);
			return false;
		}
		char	c = *w0;
		if (c != *w1)
			return false;

		if (w1 - s1) {
			memcpy(d, s1, w1 - s1);
			d += w1 - s1;
		} else {
			memcpy(d, s0, w0 - s0);
			d += w0 - s0;
		}
		p += w0 - s0;

		if (c == '?') {
			*d++ = *p++;
		} else if ((c = w0[1]) != '?' && c != '*') {
			while (*p && *p != c)
				*d++ = *p++;
		}
		s0 = ++w0;
		s1 = ++w1;
	}
	while (const char *w1 = str(s1).find(wild)) {
		char	c = *w1;
		if (c == '?') {
			*d++ = *p++;
		} else if ((c = w1[1]) != '?' && c != '*') {
			while (*p && *p != c)
				*d++ = *p++;
		}
		s1 = ++w1;
	}
	strcpy(d, s1);
	return true;
}

bool filename::matches(const char *s) const {
	return iso::matches(*this, s);
}

bool filename::matched(filename &out, const char *spec0, const char *spec1) const {
	return iso::matched(out, *this, spec0, spec1);
}

filename filename::matched(const char *spec0, const char *spec1) const {
	filename	out;
	matched(out, spec0, spec1);
	return out;
}

bool is_wild(const char *f) {
	return str(f).find('*') || str(f).find('?');
}

#if defined USE_FILENAME_WINDOWS

#ifndef INVALID_FILE_ATTRIBUTES
#define INVALID_FILE_ATTRIBUTES 0xffffffff
#endif

bool exists(const char *f) {
	return GetFileAttributesA(f) != INVALID_FILE_ATTRIBUTES;
}
bool is_dir(const char *f) {
	DWORD	r = GetFileAttributesA(f);
	return r != INVALID_FILE_ATTRIBUTES && (r & FILE_ATTRIBUTE_DIRECTORY);
}
bool is_file(const char *f) {
	DWORD	r = GetFileAttributesA(f);
	return r != INVALID_FILE_ATTRIBUTES && !(r & FILE_ATTRIBUTE_DIRECTORY);
}
uint64 filelength(const char *f) {
	WIN32_FILE_ATTRIBUTE_DATA	find;
	return GetFileAttributesExA(f, GetFileExInfoStandard, &find) ? (uint64(find.nFileSizeHigh) << 32) | find.nFileSizeLow : 0;
}
filetime_t filetime_write(const char *f) {
	WIN32_FILE_ATTRIBUTE_DATA	find;
	return GetFileAttributesExA(f, GetFileExInfoStandard, &find) ? (uint64(find.ftLastWriteTime.dwHighDateTime) << 32) | find.ftLastWriteTime.dwLowDateTime : 0;
}
filetime_t filetime_create(const char *f) {
	WIN32_FILE_ATTRIBUTE_DATA	find;
	return GetFileAttributesExA(f, GetFileExInfoStandard, &find) ? (uint64(find.ftCreationTime.dwHighDateTime) << 32) | find.ftCreationTime.dwLowDateTime : 0;
}
bool create_dir(const char *f) {
	return !f || !f[0]
		|| CreateDirectoryA(f, NULL) || GetLastError() == ERROR_ALREADY_EXISTS
		|| (create_dir(filename(f).rem_dir()) && CreateDirectoryA(f, NULL));
}
bool delete_file(const char *f) {
	return !!DeleteFileA(f);
}
bool delete_dir(const char *f) {
	for (directory_iterator name(filename(f).add_dir("*.*")); name; ++name) {
		if (!name.is_dir())
			delete_file(filename(f).add_dir(name));
		else if (str((const char*)name) != "." && str((const char*)name) != "..")
			delete_dir(filename(f).add_dir(name));
	}
	return !!RemoveDirectoryA(f);
}

#elif defined PLAT_WII

bool exists(const char *f)			{ return false; }
bool is_dir(const char *f)			{ return false; }
uint64 filelength(const char *f)	{ return 0;		}

#elif defined PLAT_PS4

filename get_cwd() {
	return "/app0/";
}
bool exists(const char *f) {
	SceKernelStat	s;
	return sceKernelStat(f, &s) == 0;
}
bool is_dir(const char *f) {
	SceKernelStat	s;
	return sceKernelStat(f, &s) == 0 && (s.st_mode & SCE_KERNEL_S_IFDIR);
}
bool is_file(const char *f) {
	SceKernelStat	s;
	return sceKernelStat(f, &s) == 0 && !(s.st_mode & SCE_KERNEL_S_IFDIR);
}
uint64 filelength(const char *f) {
	SceKernelStat	s;
	return sceKernelStat(f, &s) == 0 ? s.st_size : 0;
}
filetime_t filetime_write(const char *f) {
	SceKernelStat	s;
	return sceKernelStat(f, &s) == 0 ? s.st_mtime : 0;
}
bool create_dir(const char *f) {
	int	ret;
	return !f || !f[0]
		|| (ret = sceKernelMkdir(f, SCE_KERNEL_S_IRWU)) == 0 || (ret == SCE_KERNEL_ERROR_EEXIST)
		|| (create_dir(filename(f).rem_dir()) && sceKernelMkdir(f, SCE_KERNEL_S_IRWU) == 0);
}

#else// PLAT_MAC, PLAT_PS3, PLAT_IOS

bool exists(const char *f) {
	struct stat s;
	return stat(f, &s) == 0;
}
bool is_dir(const char *f) {
	struct stat s;
	return stat(f, &s) == 0 && (s.st_mode & S_IFDIR);
}
bool is_file(const char *f) {
	struct stat s;
	return stat(f, &s) == 0 && !(s.st_mode & S_IFDIR);
}
uint64 filelength(const char *f) {
	struct stat s;
	return stat(f, &s) == 0 ? s.st_size : 0;
}
filetime_t filetime_write(const char *f) {
	struct stat s;
	return stat(f, &s) == 0 ? s.st_mtime : 0;
}
bool create_dir(const char *f) {
	return !f || !f[0]
		|| mkdir(f, S_IRWXU|S_IRWXG|S_IRWXO) == 0 || errno == EEXIST
		|| (create_dir(filename(f).rem_dir()) && mkdir(f, S_IRWXU|S_IRWXG|S_IRWXO) == 0);
}

bool delete_file(const char *f) {
	return unlink(f) == 0;
}
bool delete_dir(const char *f) {
	for (directory_iterator name(filename(f).add_dir("*.*")); name; ++name) {
		if (!name.is_dir())
			delete_file(filename(f).add_dir(name));
		else if (str((const char*)name) != "." && str((const char*)name) != "..")
			delete_dir(filename(f).add_dir(name));
	}
	return rmdir(f) == 0;
}

#endif

#if defined PLAT_WINRT

#elif defined PLAT_PC

filename get_cwd() {
	filename	fn;
	GetCurrentDirectoryA(sizeof(fn), fn);
	strcat(fn, "\\");
	return fn;
}
bool set_cwd(const filename &f) {
	return !!SetCurrentDirectoryA(f);
}

filename get_exec_path() {
	filename	fn;
	GetModuleFileNameA(NULL, fn, sizeof(fn));
	return fn;
}
filename get_exec_dir() {
	return get_exec_path().dir() += '\\';
}

filename get_temp_dir() {
	filename	fn;
	GetTempPathA(sizeof(fn), fn);
	return fn;
}

filename filename::temp(const char *prefix) {
	filename	fn;
	GetTempFileNameA(get_temp_dir(), prefix, 0, fn);
	return fn;
}

HMODULE load_library(const char *name, const char *env, const char *dir) {
	filename	fn	= (env ? filename(getenv(env)).add_dir(dir) : filename(dir)).add_dir(name);
	if (HMODULE h = LoadLibraryA(fn))
		return h;

	if (HMODULE h = LoadLibraryA(name))
		return h;

	string	path	= string(env ? (const char*)filename(getenv(env)).add_dir(dir) : dir) + ";" + getenv("path");
	putenv(str("path=") + path);
	return LoadLibraryA(name);
}

#elif defined PLAT_MAC

filename get_cwd() {
	filename	fn;
	getcwd(fn, fn.max_length());
	fn += DIRECTORY_SEP;
	return fn;
}

bool set_cwd(const filename &f) {
	return chdir(f) == 0;
}

filename get_exec_path() {
	filename	fn;
	uint32		size = sizeof(fn);
	_NSGetExecutablePath(fn, &size);
	return fn;
}

filename get_temp_dir() {
	const char *temp = getenv("TMPDIR");
	if (!temp)
		temp = getenv("TMP");
	if (!temp)
		temp = getenv("TEMP");
	if (!temp)
		temp = getenv("TEMPDIR");
	if (!temp)
		temp = "/tmp";
	return temp;
}

filename filename::temp(const char *prefix) {
	filename	fn = get_temp_dir().add_dir(prefix) + "XXXXXX";
	mktemp(fn);
	return fn;
}

#endif

bool recursive_directory_iterator::next() {
	do {
		stack.push_back(filename(dir).add_dir("*"));//*.*
		bool got_dir = false;
		for (; !stack.empty(); stack.pop_back(), dir = dir.rem_dir()) {
			for (directory_iterator &d = stack.back(); d; ++d) {
				if (d.is_dir() && d[0] != '.') {
					set_pattern(filename(dir.add_dir(d)).add_dir(pattern));
					got_dir	= true;
					++d;
					break;
				}
			}
			if (got_dir)
				break;
		}
		if (!got_dir)
			return false;
	} while (!*this);

	return true;
}

}// namespace iso
