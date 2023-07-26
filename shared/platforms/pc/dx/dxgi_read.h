#ifndef DXGI_READ_H
#define DXGI_READ_H

#include "dxgi_helpers.h"
#include "base/block.h"
#include "codec/texels/dxt.h"

namespace iso {

template<typename R> static R get_component(uint32 v, uint8 nb, DXGI_COMPONENTS::TYPE nf);
template<typename R> static uint32 put_component(R v, uint8 nb, DXGI_COMPONENTS::TYPE nf);

template<> float get_component<float>(uint32 v, uint8 nb, DXGI_COMPONENTS::TYPE nf) {
	if (nb <= 2)
		return nb == 1 ? float(v & 1) : float(v & 3);

	uint32	m	= bits(nb);
	switch (nf) {
		case DXGI_COMPONENTS::UNORM:	return float(v & m) / m;
		case DXGI_COMPONENTS::SNORM:	return max(float(sign_extend(v & m, nb)) / (m >> 1), -1.f);
		case DXGI_COMPONENTS::UINT:		return float(v & m);
		case DXGI_COMPONENTS::SINT:		return float(sign_extend(v & m, nb));
		default:
		case DXGI_COMPONENTS::FLOAT:
			switch (nb) {
				case 10: return decode_float(v, 5, 5, 0);
				case 11: return decode_float(v, 6, 5, 0);
				case 16: return decode_float(v,10, 5, 1);
				default: return (float&)v;
			}
		case DXGI_COMPONENTS::SRGB: {
			float	s = float(v & m) / m;
			return s * (s * (s * 0.305306011f + 0.682171111f) + 0.012522878f);
		}
//		default:
//			return 0.f;
	}
}

template<> uint32 put_component<float>(float v, uint8 nb, DXGI_COMPONENTS::TYPE nf) {
	if (nb <= 2)
		return uint32(v);

	uint32	m	= bits(nb);
	switch (nf) {
		case DXGI_COMPONENTS::UNORM:	return uint32(clamp(v, 0, 1) * m);
		case DXGI_COMPONENTS::SNORM:	return uint32(clamp(v * (m >> 1) + .5f, -int(m >> 1), m >> 1));
		case DXGI_COMPONENTS::UINT:		return uint32(clamp(v, 0, m));
		case DXGI_COMPONENTS::SINT:		return uint32(clamp(v, -(1 << (nb - 1)), m >> 1));
		case DXGI_COMPONENTS::FLOAT:
			switch (nb) {
				case 10: return encode_float(v, 5, 5, 0);
				case 11: return encode_float(v, 6, 5, 0);
				case 16: return encode_float(v, 10, 5, 1);
				default: return (uint32&)v;
			}
		case DXGI_COMPONENTS::SRGB:
			return uint32(clamp(1.055f * iso::pow(v, 0.416666667f) - 0.055f, 0, 1) * m);

		default:
			return 0;
	}
}

// just sign/zero extend
template<> int get_component<int>(uint32 v, uint8 nb, DXGI_COMPONENTS::TYPE nf) {
	if (nb <= 2)
		return nb == 1 ? v & 1 : v & 3;
	uint32	m	= bits(nb);
	switch (nf) {
		case DXGI_COMPONENTS::SNORM:
		case DXGI_COMPONENTS::SINT:	return sign_extend(v & m, nb);
		default:	return v & m;
	}
}
template<> uint32 put_component<int>(int v, uint8 nb, DXGI_COMPONENTS::TYPE nf) {
	if (nb <= 2)
		return uint32(v);
	uint32	m	= bits(nb);
	switch (nf) {
		case DXGI_COMPONENTS::SNORM:
		case DXGI_COMPONENTS::SINT:	return clamp(v, int(~m), m) & m;
		default:	return v & m;
	}
}

template<int S, int N> struct process_component_s {
	template<typename R> static void f(bitfield<uint32,S,N> &v, DXGI_COMPONENTS::TYPE nf, const R &r)	{ v = put_component<R>(r, N, nf); }
	template<typename R> static void f(bitfield<uint32,S,N> &v, DXGI_COMPONENTS::TYPE nf, R &r)			{ r = get_component<R>(v, N, nf); }
};
template<int S> struct process_component_s<S, 0> {
	template<typename R> static void f(bitfield<uint32,S,0> &v, DXGI_COMPONENTS::TYPE nf, R r)		{}
};

template<int S, int N, typename R> static void process_component(bitfield<uint32,S,N> &v, DXGI_COMPONENTS::TYPE nf, R &r) {
	process_component_s<S,N>::f(v, nf, r);
}

template<int X, int Y = 0, int Z = 0, int W = 0> struct components {
	enum {N = (X != 0) + (Y != 0) + (Z != 0) + (W != 0)};
	union {
		uint8	t[(X + Y + Z + W + 7) / 8];
		bitfield<uint32,	0,		X>	x;
		bitfield<uint32,	X,		Y>	y;
		bitfield<uint32,	X+Y,	Z>	z;
		bitfield<uint32,	X+Y+Z,	W>	w;
	};
	template<typename T> int process(T (&f)[4], DXGI_COMPONENTS::TYPE nf) {
		process_component(x, nf, f[0]);
		process_component(y, nf, f[1]);
		process_component(z, nf, f[2]);
		process_component(w, nf, f[3]);
		return N;
	}
};

template<int M, int E> struct shared_exponent {
	uint32 x:M, y:M, z:M, e:E;

	int	process(float (&f)[4], DXGI_COMPONENTS::TYPE nf) {
		float	m	= iorf(128 + (e - (1 << (E - 1)) - M + 1), 0, 0).f();
		f[0] = x * m;
		f[1] = y * m;
		f[2] = z * m;
		return 3;
	}
	int	process(const float (&f)[4], DXGI_COMPONENTS::TYPE nf) {
		e = clamp(max(iorf(f[0]).e, max(iorf(f[1]).e, iorf(f[2]).e)) - 128 + (1 << (E - 1)), 0, (1 << E) - 1);
		float	m	= iorf(128 - (e - (1 << (E - 1)) - M + 1), 0, 0).f();
		x = int(f[0] * m + 0.5f);
		y = int(f[1] * m + 0.5f);
		z = int(f[2] * m + 0.5f);
		return 3;
	}
	int	process(int (&f)[4], DXGI_COMPONENTS::TYPE nf) {
		return 0;
	}
	int	process(const int (&f)[4], DXGI_COMPONENTS::TYPE nf) {
		return 0;
	}
};

template<typename S> struct shared_chroma {
	uint8	x, y;
	template<typename T>	int	process(T (&f)[4], DXGI_COMPONENTS::TYPE nf) {
		S	*t	= (S*)(uintptr_t(this) & ~3);
		f[0] = get_component<T>(t->r, 8, nf);
		f[1] = get_component<T>(uintptr_t(this) & 2 ? t->g2 : t->g1, 8, nf);
		f[2] = get_component<T>(t->b, 8, nf);
		return 3;
	}
	template<typename T>	int	process(const T (&f)[4], DXGI_COMPONENTS::TYPE nf) {
		S	*t	= (S*)(uintptr_t(this) & ~3);
		if (uintptr_t(this) & 2) {
			t->g2	= put_component<T>(f[1], 8, nf);
		} else {
			t->r	= put_component<T>(f[0], 8, nf);
			t->g1	= put_component<T>(f[1], 8, nf);
			t->b	= put_component<T>(f[2], 8, nf);
		}
		return 3;
	}
};

struct bg_rg {uint8 b, g1, r, g2;};
struct gb_gr {uint8 g1, b, g2, r;};

template<int I> struct _block_compressed;

struct BCrgb {
	union {
		uint16	w;
		struct { uint16 r:5, g:6, b:5; };
	}			c0, c1;
	uint32		cbits;

	template<typename T> void interp(T (&f)[4], DXGI_COMPONENTS::TYPE nf, uint8 f0) {
		uint8	f1 = 255 - f0;
		f[0] = get_component<T>((c0.r * f0 + c1.r * f1) / 31, 8, nf);
		f[1] = get_component<T>((c0.g * f0 + c1.g * f1) / 63, 8, nf);
		f[2] = get_component<T>((c0.b * f0 + c1.b * f1) / 31, 8, nf);
	}
	template<typename T> void get_vals(T (&f)[4], DXGI_COMPONENTS::TYPE nf, uint8 off) {
		static const uint8	table[] = {255, 0, 170, 85};
		interp(f, nf, table[(cbits >> (off * 2)) & 3]);
	}
};
struct BCa1 {
	uint64		abits;
	template<typename T> T get_val(DXGI_COMPONENTS::TYPE nf, uint8 off) {
		return get_component<T>((abits >> (off * 4)) & 15, 4, nf);
	}
};
struct BCa2 {
	uint8		a0, a1;
	uint8		abits[6];
	template<typename T> T get_val(DXGI_COMPONENTS::TYPE nf, uint8 off) {
		uint8	i = (*(uint64*)this >> (off * 3 + 16)) & 3;
		uint8	v;
		if (a0 > a1) { // 8-vals block
			static const uint8 table[] = {7,0,6,5,4,3,2,1};
			v = (a0 * table[i] + a1 * (7 - table[i])) / 7;
		} else {
			static const uint8 table[] = {5,0,4,3,2,1};
			v = i == 6 ? 0 : i == 7 ? 255 : (a0 * table[i] + a1 * (5 - table[i])) / 5;
		}
		return get_component<T>(v, 8, nf);
	}
};

template<> struct _block_compressed<1> : BCrgb {
	template<typename T> int	process(T (&f)[4], DXGI_COMPONENTS::TYPE nf, uint8 off) {
		if (c0.w > c1.w) {	//4 colour
			get_vals(f, nf, off);
			f[3] = 1;
		} else {
			uint8	i = (cbits >> (off * 2)) & 3;
			if (i == 3) {
				f[0] = f[1] = f[2] = f[3] = 0;
			} else {
				static const uint8	table[] = {255, 0, 127};
				interp(f, nf, table[i]);
				f[3] = 1;
			}
		}
		return 4;
	}
};
template<> struct _block_compressed<2> : BCa1, BCrgb {
	template<typename T> int	process(T (&f)[4], DXGI_COMPONENTS::TYPE nf, uint8 off) {
		get_vals(f, nf, off);
		f[3] = get_val<T>(nf, off);
		return 4;
	}
};
template<> struct _block_compressed<3> : BCa2, BCrgb {
	template<typename T> int	process(T (&f)[4], DXGI_COMPONENTS::TYPE nf, uint8 off) {
		get_vals(f, nf, off);
		f[3] = get_val<T>(nf, off);
		return 4;
	}
};
template<> struct _block_compressed<4> : BCa2 {
	template<typename T> int	process(T (&f)[4], DXGI_COMPONENTS::TYPE nf, uint8 off) {
		f[0] = get_val<T>(nf, off);
		return 1;
	}
};
template<> struct _block_compressed<5> {
	BCa2	r, g;
	template<typename T> int	process(T (&f)[4], DXGI_COMPONENTS::TYPE nf, uint8 off) {
		f[0] = r.get_val<T>(nf, off);
		f[1] = g.get_val<T>(nf, off);
		return 2;
	}
};
template<int I> struct block_compressed {
	typedef _block_compressed<I>	S;
	template<typename T> int	process(const T (&f)[4], DXGI_COMPONENTS::TYPE nf) {
		return 0;	// no write
	}
	template<typename T> int	process(T (&f)[4], DXGI_COMPONENTS::TYPE nf) {
		return ((S*)((uintptr_t(this) & ~15) >> int(sizeof(S) == 8)))->process(f, nf, uintptr_t(this) & 15);
	}
};

template<typename A, typename B> static int process(DXGI_COMPONENTS::LAYOUT layout, DXGI_COMPONENTS::TYPE type, A *p, B (&f)[4]) {
	switch (layout) {
		case DXGI_COMPONENTS::R32G32B32A32:	return ((components<32,32,32,32	>*)p)->process(f, type);
		case DXGI_COMPONENTS::R32G32B32:	return ((components<32,32,32	>*)p)->process(f, type);
		case DXGI_COMPONENTS::R16G16B16A16:	return ((components<16,16,16,16	>*)p)->process(f, type);
		case DXGI_COMPONENTS::R32G32:		return ((components<32,32		>*)p)->process(f, type);
//		case DXGI_COMPONENTS::R32G8X24:		return ((components<X24,8,32	>*)p)->process(f, type);
		case DXGI_COMPONENTS::R10G10B10A2:	return ((components<10,10,10,2	>*)p)->process(f, type);
		case DXGI_COMPONENTS::R11G11B10:	return ((components<11,11,10	>*)p)->process(f, type);
		case DXGI_COMPONENTS::R8G8B8A8:		return ((components<8,8,8,8		>*)p)->process(f, type);
		case DXGI_COMPONENTS::R16G16:		return ((components<16,16		>*)p)->process(f, type);
		case DXGI_COMPONENTS::R32:			return ((components<32			>*)p)->process(f, type);
		case DXGI_COMPONENTS::R24G8:		return ((components<8,24		>*)p)->process(f, type);
		case DXGI_COMPONENTS::R8G8:			return ((components<8,8			>*)p)->process(f, type);
		case DXGI_COMPONENTS::R16:			return ((components<16			>*)p)->process(f, type);
		case DXGI_COMPONENTS::R8:			return ((components<8			>*)p)->process(f, type);
		case DXGI_COMPONENTS::R1:			return ((components<1			>*)p)->process(f, type);
		case DXGI_COMPONENTS::B5G6R5:		return ((components<5,6,5		>*)p)->process(f, type);
		case DXGI_COMPONENTS::B5G5R5A1:		return ((components<5,5,5,1		>*)p)->process(f, type);
		case DXGI_COMPONENTS::R4G4B4A4:		return ((components<4,4,4,4		>*)p)->process(f, type);
		case DXGI_COMPONENTS::R9G9B9E5:		return ((shared_exponent<9,5>	 *)p)->process(f, type);
		case DXGI_COMPONENTS::R8G8_B8G8:	return ((shared_chroma<bg_rg	>*)p)->process(f, type);
		case DXGI_COMPONENTS::G8R8_G8B8:	return ((shared_chroma<gb_gr	>*)p)->process(f, type);
		case DXGI_COMPONENTS::BC1:			return ((block_compressed<1		>*)p)->process(f, type);
		case DXGI_COMPONENTS::BC2:			return ((block_compressed<2		>*)p)->process(f, type);
		case DXGI_COMPONENTS::BC3:			return ((block_compressed<3		>*)p)->process(f, type);
		case DXGI_COMPONENTS::BC4:			return ((block_compressed<4		>*)p)->process(f, type);
		case DXGI_COMPONENTS::BC5:			return ((block_compressed<5		>*)p)->process(f, type);
//		case DXGI_COMPONENTS::BC6:			return ((block_compressed<6		>*)p)->process(f, type);
//		case DXGI_COMPONENTS::BC7:			return ((block_compressed<7		>*)p)->process(f, type);
		default: return 0;
	}
}
//template<typename A, typename B> static int process(DXGI_COMPONENTS format, A *p, B (&f)[4]) {
//	return process(format.Layout(), format.Type(), p, f);
//}

template<typename T> static inline T GetChannel(const T *in, DXGI_COMPONENTS::CHANNEL c) {
	switch (c) {
		default:
		case DXGI_COMPONENTS::X:	return in[0];
		case DXGI_COMPONENTS::Y:	return in[1];
		case DXGI_COMPONENTS::Z:	return in[2];
		case DXGI_COMPONENTS::W:	return in[3];
		case DXGI_COMPONENTS::_0:	return 0;
		case DXGI_COMPONENTS::_1:	return 1;
	}
}
template<typename T> static inline void ArrangeComponents(uint16 chans, const T *in, T *out, uint32 mask) {
	for (int i = 0; mask; i++, mask >>= 1, chans >>= 3) {
		if (mask & 1)
			out[i] = GetChannel(in, (DXGI_COMPONENTS::CHANNEL)(chans & 7));
	}
}

inline int GetComponents(DXGI_COMPONENTS::LAYOUT layout, DXGI_COMPONENTS::TYPE type, const void *p, float *f) {
	return p ? process(layout, type, p, (float(&)[4])*f) : 0;
}
inline int GetComponents(DXGI_COMPONENTS::LAYOUT layout, DXGI_COMPONENTS::TYPE type, const void *p, int *f) {
	return p ? process(layout, type, p, (int(&)[4])*f) : 0;
}
inline int PutComponents(DXGI_COMPONENTS::LAYOUT layout, DXGI_COMPONENTS::TYPE type, void *p, const float *f) {
	return p ? process(layout, type, p, (const float(&)[4])*f) : 0;
}
inline int PutComponents(DXGI_COMPONENTS::LAYOUT layout, DXGI_COMPONENTS::TYPE type, void *p, const int *f) {
	return p ? process(layout, type, p, (const int(&)[4])*f) : 0;
}

inline float GetComponent0(DXGI_COMPONENTS format, const void *p) {
	uint32	v = *(uint32*)p;
	switch (format.layout) {
		case DXGI_COMPONENTS::R32G32B32A32:	return get_component<float>(v, 32,	format.Type());
		case DXGI_COMPONENTS::R32G32B32:	return get_component<float>(v, 32,	format.Type());
		case DXGI_COMPONENTS::R16G16B16A16:	return get_component<float>(v, 16,	format.Type());
		case DXGI_COMPONENTS::R32G32:		return get_component<float>(v, 32,	format.Type());
//		case DXGI_COMPONENTS::R32G8X24:		return get_component<float>(v, X24,	format.Type());
		case DXGI_COMPONENTS::R10G10B10A2:	return get_component<float>(v, 2,	format.Type());
		case DXGI_COMPONENTS::R11G11B10:	return get_component<float>(v, 10,	format.Type());
		case DXGI_COMPONENTS::R8G8B8A8:		return get_component<float>(v, 8,	format.Type());
		case DXGI_COMPONENTS::R16G16:		return get_component<float>(v, 16,	format.Type());
		case DXGI_COMPONENTS::R32:			return get_component<float>(v, 32,	format.Type());
		case DXGI_COMPONENTS::R24G8:		return get_component<float>(v, 8,	format.Type());
		case DXGI_COMPONENTS::R8G8:			return get_component<float>(v, 8,	format.Type());
		case DXGI_COMPONENTS::R16:			return get_component<float>(v, 16,	format.Type());
		case DXGI_COMPONENTS::R8:			return get_component<float>(v, 8,	format.Type());
		case DXGI_COMPONENTS::R1:			return get_component<float>(v, 1,	format.Type());
		case DXGI_COMPONENTS::B5G6R5:		return get_component<float>(v, 5,	format.Type());
		case DXGI_COMPONENTS::B5G5R5A1:		return get_component<float>(v, 1,	format.Type());
		case DXGI_COMPONENTS::R4G4B4A4:		return get_component<float>(v, 4,	format.Type());
		default: {
			float	f[4];
			GetComponents(format.Layout(), format.Type(), p, f);
			return f[0];
		}
	}
}

struct DXGI_ConstComponents {
	DXGI_COMPONENTS	format;
	const void	*p;
	DXGI_ConstComponents(DXGI_COMPONENTS format, const void *p) : format(format), p(p) {}
	void	get_swizzled(float *r)	const	{ float f[4]; GetComponents(format.Layout(), format.Type(), p, f); ArrangeComponents(format.chans, f, r, 15); }
	void	get_swizzled(int *r)	const	{ int	i[4]; GetComponents(format.Layout(), format.Type(), p, i); ArrangeComponents(format.chans, i, r, 15); }

	operator float()	const				{ float f[4]; GetComponents(format.Layout(), format.Type(), p, f); return GetChannel(f, format.GetChan(0)); }
	operator float4()	const				{ float4 r;  get_swizzled((float*)&r); return r; }
	operator int32x4()	const				{ int32x4 r; get_swizzled((int*)&r); return r; }
	operator ISO_rgba()	const				{ int32x4 r; get_swizzled((int*)&r); return ISO_rgba(r.x, r.y, r.z, r.w); }
};

struct DXGI_Components : DXGI_ConstComponents {
	DXGI_Components(DXGI_COMPONENTS format, void *p) : DXGI_ConstComponents(format, p) {}
	void	operator=(const float4p &f)		{ PutComponents(format.Layout(), format.Type(), unconst(p), (float*)&f); }
	void	operator=(const int32x4 &f)		{ PutComponents(format.Layout(), format.Type(), unconst(p), (int*)&f); }
};

template<typename T> void assign(array_vec<T, 4> &f, const DXGI_ConstComponents &c)	{ c.get_swizzled(&f.x); }
template<typename T> void assign(array_vec<T, 3> &f, const DXGI_ConstComponents &c)	{ f = project(c); }

template<> struct param_element<uint8&, DXGI_COMPONENTS> : DXGI_Components {
	param_element(uint8 &t, DXGI_COMPONENTS p) : DXGI_Components(p, &t) {}
};
template<> struct param_element<const uint8&, DXGI_COMPONENTS> : DXGI_ConstComponents {
	param_element(const uint8 &t, DXGI_COMPONENTS p) : DXGI_ConstComponents(p, &t) {}
};

//template<typename T> class byte_stride_iterator : public stride_iterator<T> {
//	using stride_iterator<T>::stride_iterator;
//
//	friend constexpr T*			begin(const byte_stride_iterator& i)				{ return i.i; }
//	friend constexpr int		intra_pitch(const byte_stride_iterator &i)			{ return i.stride(); }
//	friend void					intra_move(byte_stride_iterator &i, intptr_t n)		{ i.i = (T*)((uint8*)i.i + n); }
//	friend constexpr intptr_t	intra_diff(const byte_stride_iterator &a, const byte_stride_iterator &b)	{ return (uint8*)a.i - (uint8*)b.i; }
//};

template<typename D> const void* copy_slices0(const block<D, 3> &dest, const void *srce, DXGI_COMPONENTS format, uint64 depth_stride) {
	uint32		w	= dest.template size<1>(), h = dest.template size<2>(), d = dest.template size<3>();
	uint32		s1	= format.Bytes();
	uint32		s2	= dxgi_align(s1 * w);//, s3 = s2 * h;
	copy(make_strided_iblock(make_param_iterator(strided((const uint8*)srce, s1), move(format)), w, s2, h, depth_stride, d), dest);
	return (const uint8*)srce + s2 * h;//s3 * d;
}


template<typename D, int N> const void* copy_slices(const block<D, 3> &dest, const BC<N> *srce, uint64 depth_stride) {
	uint32		w	= round_pow2(dest.template size<1>(), 2), h = round_pow2(dest.template size<2>(), 2), d = dest.template size<3>();
	uint32		s2	= dxgi_align((uint32)sizeof(BC<N>) * w);//, s3 = s2 * h;
	copy(make_strided_block(srce, w, s2, h, depth_stride, d), dest);
	return (const uint8*)srce + s2 * h;//s3 * d;
}

inline const void *copy_slices(const block<HDRpixel, 3> &dest, const void *srce, DXGI_COMPONENTS format, uint64 depth_stride) {
	return copy_slices0(dest, srce, format, depth_stride);
}

inline const void *copy_slices(const block<ISO_rgba, 3> &dest, const void *srce, DXGI_COMPONENTS format, uint64 depth_stride) {
	switch (format.Layout()) {
		case DXGI_COMPONENTS::BC1:	return copy_slices(dest, (const BC<1>*)srce, depth_stride);
		case DXGI_COMPONENTS::BC2:	return copy_slices(dest, (const BC<2>*)srce, depth_stride);
		case DXGI_COMPONENTS::BC3:	return copy_slices(dest, (const BC<3>*)srce, depth_stride);
		case DXGI_COMPONENTS::BC4:	return copy_slices(dest, (const BC<4>*)srce, depth_stride);
		case DXGI_COMPONENTS::BC5:	return copy_slices(dest, (const BC<5>*)srce, depth_stride);
		default:
			return copy_slices0(dest, srce, format, depth_stride);
	}
}

}

#endif	// DXGI_READ_H

