#ifndef TAR_H
#define TAR_H

#include "base/strings.h"

namespace iso {

namespace {
	static bool check_oct(const char *p, const char *e) {
		bool	got_digit = false;
		while (p != e && *p) {
			if (between(*p, '0', '7'))
				got_digit = true;
			else if (*p != ' ')
				return false;
			++p;
		}
		return got_digit;
	}
	template<int N> static bool check_oct(const char(&p)[N]) {
		return check_oct(p, p + N);
	}
	static bool check_text(const char *p, const char *e) {
		bool	end = false;
		while (p != e) {
			if (*p == 0)
				end = true;
			if (end ? (*p != 0 && *p != ' ') : (*p < 32 || *p >= 127))
				return false;
			++p;
		}
		return end;
	}
	template<int N> static bool check_text(const char(&p)[N]) {
		return check_text(p, p + N);
	}
	template<int N, typename T> static void put_oct(char(&p)[N], T v) {
		put_num_base<8>(p, N - 1, v);
	}
}

struct _TARheader {
	char	filename[100];
	char	mode[8];
	char	owner[8];
	char	group[8];
	char	filesize[12];
	char	modtime[12];
	char	checksum[8];
	char	link;				// or USTAR type
	char	linkedfile[100];

	char	USTAR[6];			// indicator "ustar "
	char	USTAR_version[2];	// "00"
	char	owner_name[32];
	char	group_name[32];
	char	device_major[8];
	char	device_minor[8];
	char	filename_prefix[155];

	bool	valid() const {
		return	check_text(filename)
			&&	check_oct(mode)
			&&	(!owner[0] || check_oct(owner))
			&&	(!group[0] || check_oct(group))
			&&	check_oct(filesize)
			&&	check_oct(modtime)
			&&	check_oct(checksum)
			&&	(USTAR[0] == 0 || (memcmp(USTAR, "ustar", 5) == 0 && USTAR[5] <= ' '))
			&&	check_text(owner_name)
			&&	check_text(group_name)
			&&	check_text(filename_prefix);
	}
};
struct TARheader : _TARheader {
	uint8	padding[512 - sizeof(_TARheader)];

	TARheader()	{}
	TARheader(const char *_filename, uint16 _mode, uint16 _owner, uint16 _group, uint64 _filesize, uint64 _modtime) {
		clear(*this);
		strcpy(filename, _filename);
		put_oct(mode,		_mode);
		put_oct(owner,		_owner);
		put_oct(group,		_group);
		put_oct(filesize,	_filesize);
		put_oct(modtime,	_modtime);
		link = '0';

		memset(checksum, ' ', 8);
		uint32 c = 0;
		for (int i = 0; i < sizeof(_TARheader); i++)
			c += ((uint8*)this)[i];
		put_oct(checksum,	c);
	}
};

} // namespace iso
#endif // TAR_H