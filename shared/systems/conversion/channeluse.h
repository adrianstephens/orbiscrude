#ifndef CHANNELUSE_H
#define CHANNELUSE_H

#include "filetypes/bitmap/bitmap.h"

namespace iso {

//-----------------------------------------------------------------------------
//	ChannelUse
//-----------------------------------------------------------------------------

struct ChannelUse {
	enum {
		RED, GREEN, BLUE, ALPHA, ZERO, ONE, INTENSITY,
		UNUSED		= 0xff,
		// in analog
		NOT01		= 0x20,
		GREATER1	= 0x40,
		NEGATIVE	= 0x80,
	};

	static constexpr uint32	all(uint8 c)	{ return c * 0x01010101; }

	int	nc;
	struct chans : channels {
		using channels::channels;
		constexpr operator uint32()		const	{ return i; }
		operator	uint32&()					{ return i;	}
		const uint8 operator[](int x)	const 	{ return array[x]; }
		uint8&		operator[](int x)			{ return array[x]; }
		uint32		mask() const {
			uint32 x = i | ((i & 0x0f0f0f0f) << 4);
			x = x | ((x & 0x33333333) << 2);
			return x | ((x & 0x55555555) << 1);
		}
		uint32		same(uint32 x) const {
			return ~chans(i ^ x).mask();
		}
		chans		reverse() const {
			chans	c(ZERO,ZERO,ZERO,ONE);
			for (int i = 0; i < 4; i++) {
				if (array[i] < 4)
					c[array[i]] = i;
			}
			return c;
		}
	} ch, rc, analog;

	void	Clear();
	int		Set(chans c, chans same);
	int		Scan(const block<ISO_rgba, 2> &rect);
	int		Scan(const block<HDRpixel, 2> &rect);
	int		Scan(const float *data, int ncomps, int count, int stride);
	int		Scan(const int *data, int ncomps, int count, int stride);

	int		NumChans()		const { return nc; }
	bool	IsIntensity()	const { return rc.r == rc.g && rc.r == rc.b; }
	bool	IsFloat()		const { return !!(analog & chans::FLOAT); }
	bool	IsCompressed()	const { return !!(analog & all(chans::COMPRESSED)); }
//	bool	IsSigned()		const { return !!(analog & all(NEGATIVE)); }
	bool	IsSigned()		const { return !!(analog & chans::SIGNED); }
	int		AnalogBits()	const { chans t = analog & all(chans::SIZE_MASK); return max(max(max(t[0], t[1]), t[2]), t[3]); }

	void	change_chan(int c, int v) {
		for (int i = 0; i < 4; i++)
			if (rc[i] == c)
				rc[i] = v;
	}

	ChannelUse()	{ Clear(); }
	ChannelUse(const bitmap		*bm);
	ChannelUse(const HDRbitmap	*bm);
	template<typename T> ChannelUse(const T *data, int ncomps, int count, int stride) {
		Clear();
		Scan(data, ncomps, count, stride);
	}

	ChannelUse(const char *format);
	operator int()	const	{ return nc; }

	int		FindBestMatch(uint32 *sizes, size_t n, int stride = 1) const;
	int		FindMaskChannel(const bitmap *bm) const;
};

struct ChannelUseIterator {
	struct channel {uint8 offset, bits;} chans[4];
	uint8	bpp;
	uint8	*p;

	ISO_rgba operator*() const {
		ISO_rgba	c;
		uint32		*pu		= (uint32*)align_down(p, 4);
		uint32		offset	= (uintptr_t(p) & 3) * 8;
		for (int i = 0; i < 4; i++) {
			if (int n = chans[i].bits)
				c[i] = read_bits(pu, chans[i].offset + offset, n) * 255 / ((1 << n) - 1);
			else
				c[i] = 0;
		}
		return c;
	}

	ChannelUseIterator &operator++()					{ p += bpp / 8; return *this; }
	bool operator==(const ChannelUseIterator &b) const	{ return p == b.p; }
	bool operator!=(const ChannelUseIterator &b) const	{ return p != b.p; }

	ChannelUseIterator(const ChannelUse &cu, void *_p) : bpp(0), p((uint8*)_p) {
		uint8	starts[4];
		for (int i = 0; i < 4; i++) {
			starts[i] = bpp;
			bpp += (chans[i].bits = cu.analog[i] & channels::SIZE_MASK);
		}
		for (int i = 0; i < 4; i++)
			chans[i].offset = starts[cu.rc[i]];
	}
};
struct HDRChannelUseIterator : ChannelUseIterator {
	HDRpixel operator*() const {
		HDRpixel	c;
		uint32		*pu		= (uint32*)align_down(p, 4);
		uint32		offset	= (uintptr_t(p) & 3) * 8;
		for (int i = 0; i < 4; i++) {
			float	f = 0;
			if (int n = chans[i].bits) {
				uint32	x = read_bits(pu, chans[i].offset + offset, n);
				f = iorf(x).f();
			}
			c[i] = f;
		}
		return c;
	}
	HDRChannelUseIterator(const ChannelUse &cu, void *p) : ChannelUseIterator(cu, p) {}
};

template<typename T> const block<T, 3> &RearrangeChannels(const block<T, 3> &b, ChannelUse::chans c);
template<typename T> const block<T, 2> &RearrangeChannels(const block<T, 2> &b, ChannelUse::chans c) {
	RearrangeChannels(block<T, 3>(b, b.pitch() * b.size(), 1), c);
	return b;
}
template<typename T> const block<T, 3> &RearrangeChannels(const block<T, 3> &src, const block<T, 3> &dst, ChannelUse::chans c);
template<typename T> const block<T, 2> &RearrangeChannels(const block<T, 2> &src, const block<T, 2> &dst, ChannelUse::chans c) {
	RearrangeChannels(block<T, 3>(src, src.pitch() * src.size(), 1), block<T, 3>(dst, dst.pitch() * dst.size(), 1), c);
	return dst;
}

fixed_string<64>	GetFormatString(ISO_ptr_machine<void> p);

} //namespace iso

#endif // CHANNELUSE_H
