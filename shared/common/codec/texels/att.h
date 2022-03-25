#ifndef ATT_H
#define ATT_H

#include "bitmap/bitmap.h"
#include "base/array.h"

namespace iso {

struct ATTrgb {
	uint16le	v0, v1;
	uint32le	bits;
	void		Decode(ISO_rgba *color) const;
};

struct ATTrec : ATTrgb {
	void		Decode(const block<ISO_rgba, 2> &block) const;
//	void		Encode(const block<ISO_rgba, 2> &block);
};

struct ATTArec {
	uint64le	alpha;
	ATTrgb		rgb;
	void		Decode(const block<ISO_rgba, 2> &block) const;
//	void		Encode(const block<ISO_rgba, 2> &block, int flags);
};

template<typename D> void copy(block<const ATTrec, 2> &srce, D &dest)	{ decode_blocked<4, 4>(srce, dest); }
template<typename D> void copy(block<const ATTArec, 2> &srce, D &dest)	{ decode_blocked<4, 4>(srce, dest); }

} //namespace iso
#endif	// DXT_H
