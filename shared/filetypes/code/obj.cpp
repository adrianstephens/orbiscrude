#include "iso/iso_files.h"
#include "iso/iso_convert.h"
#include "obj.h"

using namespace iso;

string Demangle(holder<string> p) {
	return (const char*)demangle(p.t);
};

static initialise init(
	ISO_get_operation(Demangle)
);

uint32 STRINGTABLE::add(const char *s) {
	size_t	len	= strlen(s);
	char	*p	= begin(), *e = end();

	while (p < e) {
		size_t	len2 = strlen(p);
		if (len <= len2 && memcmp(s, p + len2 - len, len) == 0)
			return int(p - begin());
		p += len2 + 1;
	}

	size_t	r = size();
	memcpy(expand(len + 1), s, len + 1);
	return int(r);
}

ISO::Browser2 GetItems(ISO_ptr<void> p) {
	if (!p.GetType()->SameAs<anything>() || ISO::root("variables")["single"].GetInt()) {
		ISO_ptr<anything>	temp(0);
		temp->Append(p);
		return temp;
	}
	return p;
}
