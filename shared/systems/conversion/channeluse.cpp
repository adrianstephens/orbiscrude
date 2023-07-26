#include "channeluse.h"
#include "filename.h"

using namespace iso;

//-----------------------------------------------------------------------------
//	ChannelUse
//-----------------------------------------------------------------------------

//	0x01	==0
//	0x02	==1	(+/-)
//	0x20	not 0 or +-1
//	0x40	mag > 1
//	0x80	negative

inline uint8 Classify(float f) {
	if (f == 0)
		return 1;
	iorf	i(f);
	return (i.s << 7)
		| (int(i.e > 127 || (i.e == 127 && i.m)) << 6)
		| (int(i.e < 127) << 5)
		| 2;
}

inline uint8 Classify(int i) {
	if (i == 0)
		return 1;
	return (int(i < 0) << 7)
		| (int(abs(i) > 1) << 6)
		| 2;
}

inline ChannelUse::chans CalcSame(bool rg,bool rb,bool ra,bool gb,bool ga,bool ba) {
	return ChannelUse::chans(
		0,
		rg ? 0 : 1,
		rb ? 0 : gb ? 1 : 2,
		ra ? 0 : ga ? 1 : ba ? 2 : 3
	);
}

enum {RG,RB,GB,RA,GA,BA};

uint8 Chans2Same(const ChannelUse::chans rc) {
	return	(int(rc[0] == rc[1]) << RG)
		|	(int(rc[0] == rc[2]) << RB)
		|	(int(rc[1] == rc[2]) << GB)
		|	(int(rc[0] == rc[3]) << RA)
		|	(int(rc[1] == rc[3]) << GA)
		|	(int(rc[2] == rc[3]) << BA);
}
inline ChannelUse::chans Same2Chans(uint8 m) {
	return ChannelUse::chans(
		0,
		m & (1<<RG) ? 0 : 1,
		m & (1<<RB) ? 0 : m & (1<<GB) ? 1 : 2,
		m & (1<<RA) ? 0 : m & (1<<GA) ? 1 : m & (1<<BA) ? 2 : 3
	);
}

void ChannelUse::Clear() {
	nc		= 0;
	analog	= 0;
	rc		= all(ZERO);
	ch		= all(UNUSED);
}

int ChannelUse::Set(chans c, chans same) {
	nc		= 0;
	analog	= c & ~all(3);

	for (int i = 0; i < 4; i++) {
		if (c[i] == 1)
			rc[i] = ZERO;
		else if (c[i] == 2)
			rc[i] = ONE;
		else if (same[i] != i)
			rc[i] = same[i] < 4 ? rc[same[i]] : same[i];
		else
			ch[rc[i] = nc++] = i;
	}

	return nc;
}

int ChannelUse::Scan(const block<ISO_rgba, 2> &rect) {
	chans	z	= ~rc.same(all(ZERO)),
			o	= ~z | rc.same(all(ONE)),
			a	= analog;
	uint8	s	= Chans2Same(rc);

	for (int y = 0, w = rect.size<1>(), h = rect.size<2>(); y < h; y++) {
		ISO_rgba	*tex	= rect[y].begin();
		for (int x = w; x--; tex++) {
			chans	c	= *(uint32*)tex;
			z	|= c;
			o	&= c;
			a	|=	(c & 0xfefefefe) ^ ((c & 0x7f7f7f7f) << 1);
			s	&=	(int(c.r == c.g) << RG)
				|	(int(c.r == c.b) << RB)
				|	(int(c.r == c.a) << RA)
				|	(int(c.g == c.b) << GB)
				|	(int(c.g == c.a) << GA)
				|	(int(c.b == c.a) << BA);
		}
	}
	return Set((a & all(NOT01)) | (o.same(all(0xff)) & all(2)) | (z.same(0) & all(1)), Same2Chans(s));
}

int ChannelUse::Scan(const block<HDRpixel, 2> &rect) {
	chans	c	= analog;
	uint8	s	= Chans2Same(rc);

	for (int y = 0, w = rect.size<1>(), h = rect.size<2>(); y < h; y++) {
		HDRpixel	*tex	= rect[y].begin();
		for (int x = w; x--; tex++) {
			c.r	|= Classify(tex->r);
			c.g	|= Classify(tex->g);
			c.b	|= Classify(tex->b);
			c.a	|= Classify(tex->a);
			s	&=	(int(tex->r == tex->g) << RG)
				|	(int(tex->r == tex->b) << RB)
				|	(int(tex->r == tex->a) << RA)
				|	(int(tex->g == tex->b) << GB)
				|	(int(tex->g == tex->a) << GA)
				|	(int(tex->b == tex->a) << BA);
		}
	}

	return Set(c, Same2Chans(s));
}

int ChannelUse::Scan(const float *data, int ncomps, int count, int stride) {
	chans	c	= analog;
	chans	r	= rc;
	chans	s	= all(UNUSED);//(0,1,2,3);

	for (int i = 0; i < ncomps; i++) {
		uint8	v	= c[i];
		int		n	= count;
		for (auto p = make_stride_iterator(data + i, stride); n--; ++p)
			v |= Classify(*p);

		c[i] = v;
		s[i] = i;

		for (int j = 0; j < i; j++) {
			bool	same = r[i] == r[j];
			int		n	= count;
			for (auto p = make_stride_iterator(data, stride); same && n--; ++p) {
				auto	*pf = &*p;
				same = pf[i] == pf[j];
			}
			if (same) {
				s[i] = j;
				break;
			}
		}
	}
	return Set(c, s);
}

int ChannelUse::Scan(const int *data, int ncomps, int count, int stride) {
	chans	a	= analog;
	chans	c	= analog;
	chans	r	= rc;
	chans	s	= all(UNUSED);//(0,1,2,3);

	for (int i = 0; i < ncomps; i++) {
		uint8	v	= c[i];
		int		n	= count;
		uint32	m	= 0;
		for (auto p = make_stride_iterator(data + i, stride); n--; ++p) {
			v |= Classify(*p);
			m = max(m, abs(*p));
		}

		c[i] = v;
		s[i] = i;
		a[i] = log2(m);

		for (int j = 0; j < i; j++) {
			bool	same = r[i] == r[j];
			int		n	= count;
			for (auto p = make_stride_iterator(data, stride); same && n--; ++p) {
				auto	*pf = &*p;
				same = pf[i] == pf[j];
			}
			if (same) {
				s[i] = j;
				break;
			}
		}
	}
	int n = Set(c, s);
	analog = (analog & all(uint8(~0x1fu))) | a;
	return n;
}

bool CheckMask(int c, const block<ISO_rgba, 2> &rect) {
	uint32	mask	= 0xff << (c * 8);
	for (int y = 0, w = rect.size<1>(), h = rect.size<2>(); y < h; y++) {
		ISO_rgba	*tex	= rect[y].begin();
		for (int x = w; x--; tex++) {
			uint32	c	= *(uint32*)tex;
			if ((c & mask) == 0 && (c & ~mask) != 0)
				return false;
		}
	}
	return true;
}
int	ChannelUse::FindMaskChannel(const bitmap *bm) const {
	for (int c = 0; c < 4; c++) {
		if (!analog[c]) {
			bool	ok = true;
			if (int n = bm->Mips()) {
				for (int i = 0; ok && i < n; ++i)
					ok = CheckMask(c, bm->Mip(i));
			} else {
				ok = CheckMask(c, *bm);
			}
			if (ok)
				return c;
		}
	}
	return -1;
}

ChannelUse::ChannelUse(const bitmap *bm) {
	Clear();
	if (int n = bm->Mips()) {
		for (int i = 0; i < n; ++i)
			Scan(bm->Mip(i));
	} else {
		Scan(*bm);
	}
}

ChannelUse::ChannelUse(const HDRbitmap *bm) {
	Clear();
	if (int n = bm->Mips()) {
		for (int i = 0; i < n; ++i)
			Scan(bm->Mip(i));
	} else {
		Scan(*bm);
	}
}

ChannelUse::ChannelUse(const char *format) {
	static struct { const char *name; uint32 use; } special[] = {
		"dxt1",			CHANS(5,6,5,1)		| channels::ALL_COMP,
		"dxt2",			CHANS(5,6,5,4)		| channels::RGB_COMP,
		"dxt3",			CHANS(5,6,5,4)		| channels::RGB_COMP,
		"dxt4",			CHANS(5,6,5,8)		| channels::ALL_COMP,
		"dxt5",			CHANS(5,6,5,8)		| channels::ALL_COMP,
		"bc1",			CHANS(5,6,5,1)		| channels::ALL_COMP,
		"bc2",			CHANS(5,6,5,4)		| channels::RGB_COMP,
		"bc3",			CHANS(5,6,5,8)		| channels::ALL_COMP,
		"bc4",			CHANS(8,0,0,0)		| channels::R_COMP,
		"bc5",			CHANS(8,8,0,0)		| channels::RG_COMP,
		"bc6",			CHANS(16,16,16,0)	| channels::RGB_COMP | channels::FLOAT,
		"bc6s",			CHANS(16,16,16,0)	| channels::RGB_COMP | channels::FLOAT | channels::SIGNED,
		"bc7",			CHANS(5,6,5,5)		| channels::ALL_COMP,
		"etc1",			CHANS(4,5,5,0)		| channels::ALL_COMP,
		"etc1a1",		CHANS(4,5,5,1)		| channels::ALL_COMP,
		"etc2",			CHANS(5,5,5,0)		| channels::ALL_COMP,
		"etc2a1",		CHANS(5,5,5,1)		| channels::ALL_COMP,
		"etc2a",		CHANS(5,5,5,8)		| channels::ALL_COMP,
		"etc2r11",		CHANS(11,0,0,0)		| channels::R_COMP,
		"etc2r11s",		CHANS(11,0,0,0)		| channels::R_COMP | channels::SIGNED,
		"etc2rg11",		CHANS(11,11,0,0)	| channels::RG_COMP,
		"etc2rg11s",	CHANS(11,11,0,0)	| channels::RG_COMP | channels::SIGNED,
	};

	Clear();
	rc[ALPHA] = ONE;

	for (auto &i : special) {
		if (istr(format) == i.name) {
			chans	v = i.use;
			if (v & channels::GREY) {	// intensity
				rc.r		= rc.g = rc.b = 0;
				ch[0]		= 0;
				analog[0]	= v.r & ~0x40;
				nc			= 1;
				if (v.a & channels::SIZE_MASK) {
					rc.a		= 1;
					ch[1]		= 3;
					analog[1]	= v.a;
					nc			= 2;
				}
			} else {			// rgb
				for (int c = 0; c < 4; c++) {
					if (v[c] & channels::SIZE_MASK) {
						rc[c]		= nc;
						ch[nc]		= c;
						analog[nc]	 = v[c];
						nc++;
					}
				}
			}
			analog |= v & channels::FLOAT;
			return;
		}
	}

	uint32		default_size = 8;
	string_scan	ss(format);
	if (ss.scan(char_set::digit))
		ss >> default_size;

	bool	any_sizes = false;
	while (format) {
		int	c = -1;
		switch (to_lower(*format++)) {
			case 'r':	c = RED;		break;
			case 'g':	c = GREEN;		break;
			case 'b':	c = BLUE;		break;
			case 'a':	c = ALPHA;		break;
			case 'l':	c = INTENSITY;	break;
			case 'c':
				for (int i = 0; i < nc; i++)
					analog[ch[i]] |= channels::COMPRESSED;
				break;
			case 'f':	analog |= channels::FLOAT; break;
			case 0:
			case '.':	format = 0;		break;
			default:	c = 4;			break;
		}
		if (c >= 0) {
			uint32	i = 0;
			if (!is_digit(*format)) {
				i = default_size;
			} else {
				do {
					i = i * 10 + *format++ - '0';
				} while (is_digit(*format));
			}
			if (i == 0) {
				Clear();
				return;
			}
			switch (c) {
				case RED: case GREEN: case BLUE: case ALPHA:
					rc[c]		= nc;
					ch[nc]		= c;
					analog[nc]	= i;
					break;
				case INTENSITY:
					rc[0]		= rc[1] = rc[2] = nc;
					ch[nc]		= 0;
					analog[nc]	= i;
					break;
			}
			nc++;
		}
	}
}

int	ChannelUse::FindBestMatch(uint32 *sizes, size_t n, int stride) const {
	int		besti		= -1;
	int		bestd		= 0xff;
	bool	intensity	= IsIntensity();

	for (int i = 0; i < n; i++, sizes += stride) {
		uint32	s = *sizes;
		if ((s & channels::GREY) && !intensity)
			continue;
		uint32	d = (s & (all(channels::SIZE_MASK|channels::COMPRESSED) |channels::FLOAT)) - analog;
		if (d == 0)
			return i;
		if (!(d & all(channels::COMPRESSED))) {
			d = d + (d >> 16);
			d = (d + (d >> 8)) & 0xff;
			if (d < bestd) {
				bestd	= d;
				besti	= i;
			}
		}
	}
	return besti;
}

fixed_string<64> iso::GetFormatString(ISO_ptr_machine<void> p) {
	const char *format = ISO::root("variables")["format"].GetString();
	if (!format) {
		if (tag id = p.ID()) {
			filename	fn(id);
			format = filename(fn.name()).ext();
			if (!format[0])
				format = fn.ext();
			format += format[0] == '.';
		}
	}
	return format;
}

//-----------------------------------------------------------------------------
//	rearrange channels
//-----------------------------------------------------------------------------

// 0x40000000	copy
// 0x80000000	end of cycle
int FindCycles(const int *p, int n, int *out) {
	int	*whereis	= new int[n];
	int	*pout		= out;

	for (int i = 0; i < n; i++)
		whereis[i] = i;

	for (int i = 0; i < n; i++) {
		for (int j = i;;) {
			int	k		= p[j];
			if (k < 0 || k == whereis[j]) {
				if (i != j)
					*pout++ = j | 0x40000000;
				break;
			}
			*pout++		= j;
			whereis[j]	= k;
			j			= k;
			if (k == i)
				break;
		}
		if (pout != out)
			pout[-1] |= 0x80000000;
	}
	delete[] whereis;
	return int(pout - out);
};

inline uint8 *GetField(ISO_rgba *p, int f) {
	return (uint8*)p + f;
}

inline float *GetField(HDRpixel *p, int f) {
	return (float*)p + f;
}

template<typename T> const block<T, 3> &iso::RearrangeChannels(const block<T, 3> &block, ChannelUse::chans c) {
	int	perm[4];
	int	swaps[8];

	for (int i = 0; i < 4; i++)
		perm[i] = c[i] < 4 ? c[i] : -1;

	int ns = FindCycles(perm, 4, swaps);
	for (int *p = swaps; p < swaps + ns; ++p) {
		int		a	= p[0], b = p[1];
		if (b & 0x40000000) {
			for (auto i = block.begin(), e = block.end(); i != e; ++i) {
				for (auto i1 = i.begin(), e1 = i.end(); i1 != e1; ++i1) {
					T	*p = i1.begin();
					for (auto *ca = GetField(p, a), *cb = GetField(p, b & 3), *e = ca + block.template size<1>() * 4; ca != e; ca += 4, cb += 4)
						*ca = *cb;
				}
			}
		} else {
			for (auto i = block.begin(), e = block.end(); i != e; ++i) {
				for (auto i1 = i.begin(), e1 = i.end(); i1 != e1; ++i1) {
					T	*p = i1.begin();
					for (auto *ca = GetField(p, a), *cb = GetField(p, b & 3), *e = ca + block.template size<1>() * 4; ca != e; ca += 4, cb += 4)
						swap(*ca, *cb);
				}
			}
		}
		if (b & 0x80000000)
			++p;
	}
	for (int a = 0; a < 4; a++) {
		if (c[a] >= ChannelUse::ZERO) {
			uint8	v = c[a] == ChannelUse::ZERO ? 0 : 255;
			for (auto i = block.begin(), e = block.end(); i != e; ++i) {
				for (auto i1 = i.begin(), e1 = i.end(); i1 != e1; ++i1) {
					T	*p = i1.begin();
					for (auto *ca = GetField(p, a), *e = ca + block.template size<1>() * 4; ca != e; ca += 4)
						*ca = v;
				}
			}
		}
	}
	return block;
}

template const block<ISO_rgba, 3> &iso::RearrangeChannels(const block<ISO_rgba, 3> &block, ChannelUse::chans c);
template const block<HDRpixel, 3> &iso::RearrangeChannels(const block<HDRpixel, 3> &block, ChannelUse::chans c);

template<typename T> const block<T, 3> &iso::RearrangeChannels(const block<T, 3> &src, const block<T, 3> &dst, ChannelUse::chans c) {
	for (int a = 0; a < 4; a++) {
		int	b = c[a];
		if (b < ChannelUse::ZERO) {
			for (auto i = dst.begin(), e = dst.end(), s = src.begin(); i != e; ++i, ++s) {
				for (auto i1 = i.begin(), e1 = i.end(), s1 = s.begin(); i1 != e1; ++i1, ++s1) {
					for (auto *ca = GetField(i1.begin(), a), *cb = GetField(s1.begin(), b), *e = ca + dst.template size<1>() * 4; ca != e; ca += 4, cb += 4)
						*ca = *cb;
				}
			}
		} else if (b == ChannelUse::ZERO || b==  ChannelUse::ONE) {
			uint8	v = b == ChannelUse::ZERO ? 0 : 255;
			for (auto i = dst.begin(), e = dst.end(); i != e; ++i) {
				for (auto i1 = i.begin(), e1 = i.end(); i1 != e1; ++i1) {
					T	*p = i1.begin();
					for (auto *ca = GetField(p, a), *e = ca + dst.template size<1>() * 4; ca != e; ca += 4)
						*ca = v;
				}
			}
		}
	}
	return dst;
}
template const block<ISO_rgba, 3> &iso::RearrangeChannels(const block<ISO_rgba, 3> &src, const block<ISO_rgba, 3> &dst, ChannelUse::chans c);
template const block<HDRpixel, 3> &iso::RearrangeChannels(const block<HDRpixel, 3> &src, const block<HDRpixel, 3> &dst, ChannelUse::chans c);
