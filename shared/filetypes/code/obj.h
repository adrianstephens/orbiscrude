#include "iso/iso.h"

namespace iso {
class STRINGTABLE : public dynamic_array<char> {
public:
	uint32		add(const char *s);
//	void	write(iso::ostream_ref file)	{ if (size) file.writebuff(data, size); }
};
}

iso::string		demangle(const char *m);
ISO::Browser2	GetItems(iso::ISO_ptr<void> p);
