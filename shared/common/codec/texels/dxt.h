#ifndef DXT_H
#define DXT_H

#include "bitmap/bitmap.h"
#include "base/array.h"

namespace iso {

enum {
	DXTENC_WEIGHT_BY_ALPHA	= 1 << 0,
	DXTENC_PERCEPTUALMETRIC	= 1 << 1,
	DXTENC_ITERATIVE		= 1 << 2,
	DXTENC_WIIFACTORS		= 1 << 3,
};

template<int N> struct BC;

struct DXTrgb {
	uint16le	v0, v1;
	uint32le	bits;
	void set(uint16 _v0, uint16 _v1, uint32 _bits) {
		v0		= _v0;
		v1		= _v1;
		bits	= _bits;
	}
	void		Decode(ISO_rgba *color) const;
};

struct DXTa {
	union {
		struct {
			uint8		v0, v1;
			uint8		bits[6];
		};
		uint64	:16, bits48 : 48;
	};
	void		Decode(uint8 *alpha)	const;
	void		Encode(const uint8 *srce);
};

template<> struct BC<1> : DXTrgb {
	bool		Decode(const block<ISO_rgba, 2> &block, bool wii = false) const;
	void		Encode(const block<ISO_rgba, 2> &block, uint32 flags = 0);
};

template<> struct BC<2> {
	uint64le	alpha;
	DXTrgb		rgb;
	void		Decode(const block<ISO_rgba, 2> &block) const;
	void		Encode(const block<ISO_rgba, 2> &block, uint32 flags = 0);
};

template<> struct BC<3> {
	DXTa		alpha;
	DXTrgb		rgb;
	void		Decode(const block<ISO_rgba, 2> &block) const;
	void		Encode(const block<ISO_rgba, 2> &block, uint32 flags = 0);
};

typedef BC<1> DXT1rec;
typedef BC<2> DXT23rec;
typedef BC<3> DXT45rec;

template<> struct BC<4> {
	DXTa		red;
	void		Decode(const block<ISO_rgba, 2> &block) const;
	void		Encode(const block<ISO_rgba, 2> &block);
};

template<> struct BC<5> {
	DXTa		red, green;
	void		Decode(const block<ISO_rgba, 2> &block) const;
	void		Encode(const block<ISO_rgba, 2> &block);
};

struct BC6 {
	uint64		b0, b1;
	void		Decode(const block<HDRpixel, 2> &block, bool sign) const;
	void		Encode(const block<HDRpixel, 2> &block, bool sign);
};

template<> struct BC<6> : BC6 {
	void		Decode(const block<HDRpixel, 2> &block) const	{ BC6::Decode(block, false); }
	void		Encode(const block<HDRpixel, 2> &block)			{ BC6::Encode(block, false); }
};

template<> struct BC<-6> : BC6 {
	void		Decode(const block<HDRpixel, 2> &block) const	{ BC6::Decode(block, true); }
	void		Encode(const block<HDRpixel, 2> &block)			{ BC6::Encode(block, true); }
};

template<> struct BC<7> {
	uint64		b0, b1;
	void		Decode(const block<ISO_rgba, 2> &block) const;
	void		Encode(const block<ISO_rgba, 2> &block);
};

template<int N> class T_swap_endian<BC<N> > {
	typedef BC<N>								X;
	typedef array<uint16, sizeof(X) / 2>	R;
	array<uint16be, sizeof(X) / 2>		rep;
public:
	operator		X()	const				{ R r = rep; return (X&)r; }
	T_swap_endian	&operator=(const X &b)	{ rep = (R&)b; return *this; }

	template<typename T> void Decode(const block<T, 2>& block) const	{ force_cast<const X>(R(rep)).Decode(block);  }
	template<typename T> void Encode(const block<T, 2>& block)			{ X x; x.Encode(block); rep = reinterpret_cast<const R&>(x); }
};

template<int N, typename D> void copy(block<const BC<N>, 2> &srce, D& dest) {
	decode_blocked<4, 4>(srce, dest);
}
template<typename S, int N> void copy(block<S, 2> &srce, block<BC<N>, 2> &dest) {
	encode_blocked<4, 4>(srce, dest);
}
template<int N, typename D> void copy(block<const T_swap_endian<BC<N>>, 2> &srce, D& dest) {
	decode_blocked<4, 4>(srce, dest);
}
template<typename S, int N> void copy(block<S, 2> &srce, block<T_swap_endian<BC<N>>, 2> &dest) {
	encode_blocked<4, 4>(srce, dest);
}
template<int N, typename D> void copy(block<const constructable<T_swap_endian<BC<N>>>, 2> &srce, D &dest) {
	decode_blocked<4, 4>(srce, dest);
}
template<typename S, int N> void copy(block<S, 2> &srce, block<constructable<T_swap_endian<BC<N>>>, 2> &dest) {
	encode_blocked<4, 4>(srce, dest);
}

void	FixDXT5Alpha(uint8 *srce, uint32 srce_pitch, void *dest, uint32 block_size, uint32 dest_pitch, int width, int height);

} //namespace iso
#endif	// DXT_H
