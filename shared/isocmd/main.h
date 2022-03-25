#include "iso/iso_files.h"
#include "thread.h"

namespace isocmd {
using namespace iso;

struct Cache {
	typedef triple<string, ISO_ptr<void>, int>	_entry;
	struct entry {
		string			fn;
		ISO_ptr<void>	p;
		int				depth;
		entry()	{}
		entry(const char *fn, int depth) : fn(fn), depth(depth) {}
	};
	order_array<entry>	cache;
	Mutex				mutex;

	ISO_ptr<void>&	operator()(const filename &fn) {
		auto	w	= with(mutex);
		for (int i = 0; i < cache.size(); i++) {
			if (istr(cache[i].fn) == fn)
				return cache[i].p;
		}
		entry&	e = cache.emplace_back(fn, FileHandler::Include::Depth());
		return e.p;
	}
	const char*		operator()(ISO_ptr<void> p) {
		auto	w	= with(mutex);
		for (int i = 0; i < cache.size(); i++) {
			if (cache[i].p == p)
				return cache[i].fn;
		}
		return 0;
	}

	const char *get(int i, int *depth) const {
		if (i < 0 || i >= cache.size())
			return 0;
		if (depth)
			*depth = cache[i].depth - 1;
		return cache[i].fn;
	}

	int			count() const {
		return int(cache.size());
	}

	~Cache() {}
};
}
