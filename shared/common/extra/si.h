#ifndef SI_H
#define SI_H

#include "base/defs.h"
#include "base/strings.h"

namespace iso {

template<typename T> T SIsuffix(T v, char *s) {
	T		f	= 1000;
	int		u	= 0;
	T		a	= abs(v);
	if (a > f) {
		static const char units[] = "kMGTPEZY";
		while (u < num_elements(units) - 2 && a > f) {
			f *= 1000;
			u++;
		}
		*s++ = units[u];

	} else if (a < 1 && a != 0) {
		static const char units[] = "m\xBCnpfazy";
		while (u < num_elements(units) - 2 && a * f < T(0.9999)) {
			f *= 1000;
			u++;
		}
		if (u == 1)
			*s++ = '\xCE';
		*s++ = units[u];
		f = reciprocal(f);

	} else {
		f = 1;
	}
	*s = 0;
	return f;
}

template<typename T> T SIsuffix(T v, char16 *s) {
	T		f	= 1000;
	int		u	= 0;
	T		a	= abs(v);
	if (a > f) {
		static const char units[] = "kMGTPEZY";
		while (u < num_elements(units) - 2 && a > f) {
			f *= 1000;
			u++;
		}
		*s++ = units[u];

	} else if (a < 1 && a != 0) {
		static const char16 units[] = u"m\uBCCEnpfazy";
		while (u < num_elements(units) - 2 && a * f < T(0.9999)) {
			f *= 1000;
			u++;
		}
		*s++ = units[u];
		f = reciprocal(f);

	} else {
		f = 1;
	}
	*s = 0;
	return f;
}

template<typename T> class SI {
	T	v;
public:
	SI(T v) : v(v)	{}

	template<typename C> friend size_t to_string(C *s, const SI &si) {
		C		suffix[3];
		size_t	len = to_string(s, si.v / SIsuffix(si.v, suffix));
		for (C *p = suffix, c; c = *p++; )
			s[len++] = c;
		return len;
	}
};

} // namespace iso
#endif // SI_H
