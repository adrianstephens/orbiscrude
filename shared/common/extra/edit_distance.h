#include "base/strings.h"
#include "base/block.h"

namespace iso {

// Levenshtein distance
// http://en.wikipedia.org/wiki/Levenshtein_distance

template<typename A, typename B> int edit_distance(const A &a, const B &b) {
	size_t	alen	= a.length(), blen = b.length();

	if (alen == 0)
		return blen;

	if (blen == 0)
		return alen;

	++alen;
	++blen;
	auto_block<int,2>	d = make_auto_block<int>(alen, blen);
//	array_2d<int> d(alen, blen);

	for (size_t i = 0; i < alen; ++i)
		d[i][0] = i;

	for (size_t i = 0; i < blen; ++i)
		d[0][i] = i;

	for (size_t i = 1; i < alen; ++i) {
		for (size_t j = 1; j < blen; ++j) {
			d[i][j] = min(min(
				d[i - 1][j] + 1,
				d[i][j - 1] + 1),
				d[i - 1][j - 1] + int(a[i - 1] != b[j - 1])
			);
		}
	}
	return d[alen - 1][blen - 1];
}

template<typename A, typename B> int edit_distance_swaps(const A &a, const B &b) {
	size_t	alen	= a.length(), blen = b.length();

	if (alen == 0)
		return blen;

	if (blen == 0)
		return alen;

	++alen;
	++blen;

	auto_block<int,2>	d = make_auto_block<int>(alen, blen);
//	array_2d<int> d(alen, blen);

	for (size_t i = 0; i < alen; ++i)
		d[i][0] = i;

	for (size_t i = 0; i < blen; ++i)
		d[0][i] = i;

	for (size_t i = 1; i < alen; ++i) {
		for (size_t j = 1; j < blen; ++j) {
			int	cost	= int(a[i - 1] != b[j - 1]);
			d[i][j] = min(min(
				d[i - 1][j] + 1,
				d[i][j - 1] + 1),
				d[i - 1][j - 1] + cost
			);
			if (i > 1 && j > 1 && a[i] == b[j - 1] && a[i - 1] == b[j])
				d[i][j] = min(d[i][j], d[i - 2][j - 2] + cost);
		}
	}
	return d[alen - 1][blen - 1];
}

}