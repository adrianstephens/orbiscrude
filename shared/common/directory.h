#ifndef DIRECTORY_H
#define DIRECTORY_H

#include "filename.h"
#include "base/array.h"

#ifdef PLAT_PS3
#include <cell/cell_fs.h>
#elif defined(PLAT_PS4)
#include <_fs.h>
#elif defined PLAT_MAC || defined PLAT_IOS || defined PLAT_ANDROID
#include <dirent.h>
#endif

namespace iso {

#if defined(PLAT_PC) || defined(PLAT_X360) || defined(PLAT_XONE)
class _directory_iterator {
	WIN32_FIND_DATAA	find;
	HANDLE				h;
protected:
	_directory_iterator() : h(INVALID_HANDLE_VALUE) {}
	bool	open(const char *pattern) {
#ifdef PLAT_X360
		h = FindFirstFileA(pattern, &find);
#else
		h = FindFirstFileExA(pattern, FindExInfoBasic, &find, FindExSearchNameMatch, NULL, FIND_FIRST_EX_LARGE_FETCH);
#endif
		return h != INVALID_HANDLE_VALUE;
	}
	void	close() {
		if (h != INVALID_HANDLE_VALUE)
			FindClose(h);
	}
	bool	next() {
		return !!FindNextFileA(h, &find);
	}
	const char*	name()			const { return find.cFileName; }
public:
	bool		is_dir()		const { return !!(find.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY); }
	filetime_t	write_time()	const { return find.ftLastWriteTime; }
	filetime_t	create_time()	const { return find.ftCreationTime; }
	filetime_t	access_time()	const { return find.ftLastAccessTime; }
	uint64		size()			const { return lo_hi(find.nFileSizeLow, find.nFileSizeHigh); }
};

#elif defined(PLAT_PS3)
class _directory_iterator {
	CellFsDirent		dirent;
	filename			match;
	int					fd;
protected:
	_directory_iterator() : fd(-1) {}
	bool	open(const char *pattern) {
		fd = -1;
		if (cellFsOpendir(filename(pattern).dir(), &fd) == CELL_OK) {
			match = filename(pattern).name_ext();
			return next();
		}
		return false;
	}
	void	close() {
		if (fd != -1)
			cellFsClosedir(fd);
	}
	bool	next() {
		uint64	read;
		while (cellFsReaddir(fd, &dirent, &read) == CELL_OK && read) {
			if (filename(dirent.d_name).matches(match))
				return true;
		}
		return false;
	}
	const char* name()		const { return dirent.d_name; }
public:
	bool		is_dir()	const { return !!(dirent.d_type && CELL_FS_TYPE_DIRECTORY); }
};

#elif defined(PLAT_PS4)

class _directory_iterator {
	char*				block;
	blksize_t			blocksize;
	int					blockread;
	SceKernelDirent*	dirent;
	filename			match;
	int					fd;
protected:
	_directory_iterator() : fd(-1) {}
	bool	open(const char *pattern) {
		filename	dir = filename(pattern).dir();
		fd = sceKernelOpen(dir, SCE_KERNEL_O_RDONLY | SCE_KERNEL_O_DIRECTORY, SCE_KERNEL_S_IRU | SCE_KERNEL_S_IFDIR);
		if (fd >= 0) {
			SceKernelStat	s;
			sceKernelStat(dir, &s);
			blocksize	= s.st_blksize;
			block		= (char*)malloc(blocksize);
			blockread	= sceKernelGetdents(fd, block, blocksize);
			dirent		= (SceKernelDirent*)block;
			match		= filename(pattern).name_ext();
			if (blockread > 0)
				return filename(dirent->d_name).matches(match) || next();
		}
		return false;
	}
	void	close() {
		if (fd >= 0) {
			sceKernelClose(fd);
			free(block);
		}
	}
	bool	next() {
		for (;;) {
			dirent = (SceKernelDirent*)((char*)dirent + dirent->d_reclen);
			if ((char*)dirent - block < blockread) {
				if (filename(dirent->d_name).matches(match))
					return true;
			} else {
				if (!(blockread = sceKernelGetdents(fd, block, blocksize)))
					return false;
				dirent = (SceKernelDirent*)block;
			}
		}
	}
	const char*	name()		const { return dirent->d_name; }
public:
	bool		is_dir()	const { return SCE_KERNEL_S_ISDIR(dirent->d_type); }
};

#elif defined PLAT_WII
class _directory_iterator {
	DVDDir		dir;
	DVDDirEntry	entry;
	bool		is_open;
protected:
	_directory_iterator() : is_open(false) {}
	bool	open(const char *pattern) {
		return (is_open = DVDOpenDir(pattern, &dir)) && next();
	}
	void	close() {
		if (is_open)
			DVDCloseDir(&dir);
	}
	bool	next() {
		return DVDReadDir(&dir, &entry);
	}
	const char*	name()		const { return entry.name; }
public:
	bool		is_dir()	const { return !!entry.isDir; }
};

#elif defined PLAT_MAC || defined PLAT_IOS || defined PLAT_ANDROID

class _directory_iterator {
	DIR					*dir;
	struct dirent		*entry;
	filename			match;
protected:
	_directory_iterator() : dir(0) {}
	bool	open(const char *pattern) {
		if (dir = opendir(filename(pattern).dir())) {
			match = filename(pattern).name_ext();
			return next();
		}
		return false;
	}
	void	close() {
		if (dir)
			closedir(dir);
	}

	bool	next() {
		while (entry = readdir(dir)) {
			if (filename(entry->d_name).matches(match))
				return true;
		}
		return false;
	}
	const char*	name()		const { return entry->d_name; }
public:
	bool		is_dir()	const { return entry->d_type & DT_DIR; }
};
#endif

class directory_iterator : public _directory_iterator {
	bool	ok;
	directory_iterator(const directory_iterator &d);
public:
	directory_iterator(directory_iterator &&d)=default;
	directory_iterator() : ok(false)		{}
	directory_iterator(const char *pattern) { ok = open(pattern); }
	~directory_iterator()					{ close(); }
	bool	set_pattern(const char *pattern){ close(); return ok = open(pattern); }
	bool	operator++()					{ return ok = ok && next(); }
	bool	test()			const			{ return ok; }
	operator const char*()	const			{ return ok ? name() : 0; }
};

class directory_iterator2 : public directory_iterator {
protected:
	const char *pattern;
	filename	dir;
	directory_iterator2(const char *pattern) : pattern(pattern) {}
	bool	set_dir(const filename &_dir) {
		return set_pattern(filename(dir = _dir).add_dir(pattern));
	}
public:
	operator filename() const {
		if (const char *n = name())
			return filename(dir).add_dir(filename(pattern).dir()).add_dir(n);
		return filename();
	}
};

class recursive_directory_iterator : public directory_iterator2 {
protected:
	dynamic_array<directory_iterator>	stack;
	bool	next();

	bool	set_dir(const filename &_dir) {
		return directory_iterator2::set_dir(_dir) || next();
	}
	recursive_directory_iterator(const char *pattern) : directory_iterator2(pattern) {}
public:
	recursive_directory_iterator(const filename &fn) : directory_iterator2(fn.name_ext_ptr()) {
		set_dir(fn.dir());
	}
	recursive_directory_iterator(const filename &dir, const char *pattern) : directory_iterator2(pattern) {
		set_dir(dir);
	}
	bool	operator++() {
		return directory_iterator::operator++() || next();
	}
};

template<typename I, typename B> struct directory_helper : B {
	I	i, e;
	bool next_directory_entry() {
		while (i != e) {
			if (B::set_dir(*i++))
				return true;
		}
		return false;
	}
	directory_helper(I begin, I end, const char *pattern) : B(pattern), i(begin), e(end) {
		next_directory_entry();
	}
	bool	operator++() {
		return B::operator++() || next_directory_entry();
	}
};

inline auto directory_recurse(const filename &dir, const char *pattern)				{ return recursive_directory_iterator(dir, pattern); }

template<typename I> auto directories(I begin, I end, const char *pattern)			{ return directory_helper<I, directory_iterator2>(begin, end, pattern); }
template<typename I> auto directories_recurse(I begin, I end, const char *pattern)	{ return directory_helper<I, recursive_directory_iterator>(begin, end, pattern); }
template<typename C> auto directories(const C &c, const char *pattern)				{ return directories(begin(c), end(c), pattern); }
template<typename C> auto directories_recurse(const C &c, const char *pattern)		{ return directories_recurse(begin(c), end(c), pattern); }


}

#endif//DIRECTORY_H
