#ifndef ETC_H
#define ETC_H

#include "bitmap/bitmap.h"
#include "base/array.h"

namespace iso {

struct ETC1 {
	union {
		uint32	rgbc;
		struct {
			uint8	r, g, b;
			union {
				bitfield<uint8, 5, 3>	cw0;
				bitfield<uint8, 2, 3>	cw1;
				bitfield<uint8, 1, 1>	diff;
				bitfield<uint8, 0, 1>	flip;
			};
		};
	};
	uint32be	pixels;
	
	void		set(uint32 _rgbc, bool _flip, uint8 _cw0, uint8 _cw1, uint32 _pixels) {
		rgbc	= _rgbc;
		flip	= _flip;
		cw0		= _cw0;
		cw1		= _cw1; 
		pixels	= _pixels;
	}

	void		Decode(const block<ISO_rgba, 2>& block) const;
	void		DecodePunch(const block<ISO_rgba, 2> &block) const;
	void		Encode(const block<ISO_rgba, 2>& block);
	void		EncodePunch(const block<ISO_rgba, 2>& block);
};

struct ETCA {
	union {
		struct { uint8	base, book; };
		uint64be	pixels;
	};
	void		set(uint8 _base, uint8 _book, uint64 _pixels) { pixels = _pixels; base = _base; book = _book; }

	void		Decode(uint8 *alpha) const;
	void		Decode(uint16 *alpha) const;
	void		Decode(int16 *alpha) const;
	void		Encode(const uint8 *alpha, uint32 mask = 0xffff);
	void		Encode(const uint16 *alpha, uint32 mask = 0xffff);
	void		Encode(const int16 *alpha, uint32 mask = 0xffff);
};

struct ETC1_RGBA1 : ETC1 {
	void		Decode(const block<ISO_rgba, 2>& block) const	{ DecodePunch(block); }
	void		Encode(const block<ISO_rgba, 2>& block)			{ EncodePunch(block); }
};

struct ETC2_RGB {
	static uint32 make_overflow(uint8 x) {
		x &= 15;
		return x == 7 || (x >= 10 && x != 12) ? 14 : 1;		// check for following bit sequences: 0111, 1010, 1011, 1101, 1110, 1111
	}
	union {
		ETC1	etc1;
		struct {
			union {
				bitfield_multi<uint32be, 26,1, 29,3>		ovr;
				bitfield_multi<uint32be, 16,10, 27,2>		c0;
				bitfield_multi<uint32be, 4,12>				c1;
				bitfield_multi<uint32be, 0,1, 2,2>			mod;
				bitfield<uint32be, 1, 1>					nopunch;
			};
			void	set(uint32 _c0, uint32 _c1, uint32 _mod, bool punch = false) {
				c0		= _c0;
				c1		= _c1;
				mod		= _mod;
				nopunch	= !punch;
				ovr		= make_overflow(_c0 >> 8);	// make sure that red overflows
			}
		} t;

		struct {
			union {
				bitfield<uint32be, 31,1>					ovr;
				bitfield_multi<uint32be, 18,1, 21,3>		ovg;
				bitfield_multi<uint32be, 15,3, 19,2, 24,7>	c0;
				bitfield_multi<uint32be, 3,12>				c1;
				bitfield_multi<uint32be, 0,1, 2,1>			mod;
				bitfield<uint32be, 1, 1>					nopunch;
			};
			void	set(uint32 _c0, uint32 _c1, uint32 _mod, bool punch = false) {
				c0		= _c0;
				c1		= _c1;
				mod		= _mod;
				nopunch	= !punch;
				ovr		= ~(_c0 >> 11);				// make sure that red does not overflow
				ovg		= make_overflow(_c0 >> 1);	// make sure that green overflows
			}
		} h;

		struct {
			union {
				bitfield<uint32be, 31,1>						ovr;
				bitfield<uint32be, 23,1>						ovg;
				bitfield_multi<uint32be, 10,1, 13,3>			ovb;
				bitfield<uint32be, 1, 1>						diff;
				bitfield_multi<uint32be, 7,3, 11,2, 16,7, 24,7>	c0;
				bitfield_multi<uint64be, 19,14, 34,5>			ch;
				bitfield_multi<uint64be, 0,19>					cv;
			};
			void	set(uint32 _c0, uint32 _ch, uint32 _cv) {
				c0		= _c0;
				ch		= _ch;
				cv		= _cv;
				diff	= 1;
				ovr		= ~(_c0 >> 18);				// make sure that red does not overflow
				ovg		= ~(_c0 >> 11);				// make sure that green does not overflow
				ovb		= make_overflow(_c0 >> 1);	// make sure that blue overflows
			}

		} p;
	};
	void		Decode(const block<ISO_rgba, 2> &block) const;
	void		DecodePunch(const block<ISO_rgba, 2> &block) const;
	void		Encode(const block<ISO_rgba, 2>& block);
	void		EncodePunch(const block<ISO_rgba, 2>& block);
};

struct ETC2_RGBA1 : ETC2_RGB {
	void		Decode(const block<ISO_rgba, 2>& block) const	{ DecodePunch(block); }
	void		Encode(const block<ISO_rgba, 2>& block)			{ EncodePunch(block); }
};

struct ETC2_RGBA {
	ETCA		alpha;
	ETC2_RGB	rgb;
	void		Decode(const block<ISO_rgba, 2> &block) const;
	void		Encode(const block<ISO_rgba, 2>& block);
};

struct ETC2_R11 {
	ETCA		red;
	void		Decode(const block<HDRpixel, 2>& block) const;
	void		Encode(const block<HDRpixel, 2>& block);
};

struct ETC2_RG11 {
	ETCA		red;
	ETCA		green;
	void		Decode(const block<HDRpixel, 2>& block) const;
	void		Encode(const block<HDRpixel, 2>& block);
};

struct ETC2_R11S {
	ETCA		red;
	void		Decode(const block<HDRpixel, 2>& block) const;
	void		Encode(const block<HDRpixel, 2>& block);
};

struct ETC2_RG11S {
	ETCA		red;
	ETCA		green;
	void		Decode(const block<HDRpixel, 2>& block) const;
	void		Encode(const block<HDRpixel, 2>& block);
};

template<typename D> void copy(block<const ETC1, 2> &srce, D& dest)				{ decode_blocked<4, 4>(srce, dest); }
template<typename D> void copy(block<const ETC2_RGB, 2> &srce, D& dest)			{ decode_blocked<4, 4>(srce, dest); }
template<typename D> void copy(block<const ETC2_RGBA1, 2> &srce, D& dest)		{ decode_blocked<4, 4>(srce, dest); }
template<typename D> void copy(block<const ETC2_RGBA, 2> &srce, D& dest)		{ decode_blocked<4, 4>(srce, dest); }
template<typename D> void copy(block<const ETC2_R11, 2> &srce, D& dest)			{ decode_blocked<4, 4>(srce, dest); }
template<typename D> void copy(block<const ETC2_RG11, 2> &srce, D& dest)		{ decode_blocked<4, 4>(srce, dest); }
template<typename D> void copy(block<const ETC2_R11S, 2> &srce, D& dest)		{ decode_blocked<4, 4>(srce, dest); }
template<typename D> void copy(block<const ETC2_RG11S, 2> &srce, D& dest)		{ decode_blocked<4, 4>(srce, dest); }

template<typename S> void copy(block<S, 2> &srce, block<ETC1, 2> &dest)			{ encode_blocked<4, 4>(srce, dest); }
template<typename S> void copy(block<S, 2> &srce, block<ETC2_RGB, 2> &dest)		{ encode_blocked<4, 4>(srce, dest); }
template<typename S> void copy(block<S, 2> &srce, block<ETC2_RGBA1, 2> &dest)	{ encode_blocked<4, 4>(srce, dest); }
template<typename S> void copy(block<S, 2> &srce, block<ETC2_RGBA, 2> &dest)	{ encode_blocked<4, 4>(srce, dest); }
template<typename S> void copy(block<S, 2> &srce, block<ETC2_R11, 2> &dest)		{ encode_blocked<4, 4>(srce, dest); }
template<typename S> void copy(block<S, 2> &srce, block<ETC2_RG11, 2> &dest)	{ encode_blocked<4, 4>(srce, dest); }
template<typename S> void copy(block<S, 2> &srce, block<ETC2_R11S, 2> &dest)	{ encode_blocked<4, 4>(srce, dest); }
template<typename S> void copy(block<S, 2> &srce, block<ETC2_RG11S, 2> &dest)	{ encode_blocked<4, 4>(srce, dest); }


} //namespace iso
#endif	// ETC_H
