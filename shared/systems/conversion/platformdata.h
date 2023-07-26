#ifndef PLATFORMDATA_H
#define PLATFORMDATA_H

#include "maths/geometry.h"
#include "stream.h"
#include "filetypes/bitmap/bitmap.h"
#include "filetypes/sound/sample.h"
#include "iso/iso_binary.h"
#include "iso/iso_convert.h"
#include "filename.h"

namespace iso {

force_inline float squareness(int w, int h) {
	return min(w, h) / float(max(w, h));
}

//-----------------------------------------------------------------------------
//	Platform
//-----------------------------------------------------------------------------

enum PLATFORM_INDEX {
	PLATFORM_INDEX_NONE		= -1,
	PLATFORM_INDEX_X360		= 0,
	PLATFORM_INDEX_PS3		= 1,
	PLATFORM_INDEX_WII		= 2,
	PLATFORM_INDEX_IOS		= 3,
	PLATFORM_INDEX_PC		= 4,
	PLATFORM_INDEX_PS4		= 5,
};

class Platform : public static_list<Platform> {
	static Platform	*current;
public:
	enum type {
		PT_NOTFOUND			= 0,
		_PT_FOUND			= 1 << 0,
		_PT_BE				= 1 << 1,
		_PT_64				= 1 << 2,

		PT_LITTLEENDIAN		= _PT_FOUND,
		PT_LITTLEENDIAN64	= PT_LITTLEENDIAN | _PT_64,
		PT_BIGENDIAN		= _PT_FOUND | _PT_BE,
		PT_BIGENDIAN64		= PT_BIGENDIAN | _PT_BE,
	};
private:
	const char	*platform;
	virtual type			Set()												{ return PT_NOTFOUND;	}
	virtual ISO_ptr<void>	Convert(ISO_ptr<void> p, const ISO::Type *type)		{ return ISO_NULL;		}
public:
	Platform(const char *platform) : platform(platform)							{}
	const char*			Name()	const											{ return platform;		}

	virtual ISO_rgba	DefaultColour()											{ return ISO_rgba(255,255,255,255); }
	virtual uint32		NextTexWidth(uint32 x)									{ return x; }
	virtual uint32		NextTexHeight(uint32 x)									{ return x; }
	virtual bool		BetterTex(uint32 w0, uint32 h0, uint32 w1, uint32 h1)	{
		int	t1 = w1 * h1, t0 = w0 * h0;
		if (t1 < t0 * 7 / 8)
			return true;
		if (t0 < t1 * 7 / 8)
			return false;
		return squareness(w1, h1) > squareness(w0, h0);
	}

	virtual ISO_ptr<void> ReadFX(tag id, istream_ref file, const filename *fn) { return ISO_NULL; }
	virtual ISO_ptr<void> MakeShader(const ISO_ptr<ISO_openarray<ISO_ptr<string>>> &shader) { return ISO_NULL; }

	static type			Set(const char *_platform);
	static Platform*	Get(const char *_platform);
	static const char*	GetCurrent()											{ return current ? current->platform : 0; }

	template<typename F, typename T> static bool Redirect() {
		auto	f = ISO::getdef<F>();
		if (TypeType(f) == ISO::USER) {
			auto&	fu = ((ISO::TypeUser*)f)->subtype;
			if (TypeType(fu) == ISO::REFERENCE) {
				fu	= fu->Is64Bit() ? ISO::getdef<ISO::ptr<T,64> >() : ISO::getdef<ISO::ptr<T,32> >();
				return true;
			}
		}
		return false;
	}
};

//-----------------------------------------------------------------------------
//	PlatformMemory
//-----------------------------------------------------------------------------

struct PlatformMemory : memory_block {
	PlatformMemory(uint32 offset, uint32 size);
	~PlatformMemory();
};

//-----------------------------------------------------------------------------
//	cache
//-----------------------------------------------------------------------------

struct cache_filename {
	filename	source, cached;

	cache_filename(ISO_ptr_machine<void> p);
	cache_filename(const filename &fn);

	bool		newer() const {
		return exists(cached) && (source.blank() || filetime_write(cached) > filetime_write(source));
	}
	ISO_ptr<void> load(tag2 id) const {
		return ISO::binary_data.Read(id, FileInput(cached).me());
	}

#ifdef HAS_FILE_WRITER
	void store(ISO_ptr<void> p, void *bin_start, uint32 bin_size) const {
		create_dir(cached.dir());
		ISO_binary(bin_start, bin_size).Write(p, FileOutput(cached).me());
	}
#endif
	operator bool() const {
		return !cached.blank();
	}
};

//-----------------------------------------------------------------------------
//	cast_iterator
//-----------------------------------------------------------------------------
template<typename T> class cast_iterator {
	T		*p;
	struct temp {
		T *t;
		temp(T *t) : t(t)	{}
		template<typename U> const U &operator=(const U &u)	const { *t = T(u); return u;	}
		template<typename U> operator U()					const { return U(*t);	}
		template<typename U> friend void assign(temp&& a, const U &u) { *a.t = T(u); }
	};
public:
	cast_iterator(T *_p) : p(_p)					{}
	cast_iterator(const cast_iterator &i) : p(i.p)	{}

	cast_iterator&	operator++()		{ ++p; return *this;	}
	cast_iterator&	operator--()		{ --p; return *this;	}
	cast_iterator&	operator+=(int i)	{ p += i; return *this;	}
	cast_iterator&	operator-=(int i)	{ p += i; return *this;	}

	temp			operator*()	const	{ return p; }
};
template<typename T> inline cast_iterator<T> casted(T *p)							{ return cast_iterator<T>(p);	}
template<typename T> cast_iterator<T>	operator+(const cast_iterator<T> &s, int i) { return cast_iterator<T>(s) += i;	}
template<typename T> cast_iterator<T>	operator-(const cast_iterator<T> &s, int i) { return cast_iterator<T>(s) -= i;	}

//-----------------------------------------------------------------------------
//	scaled_iterator
//-----------------------------------------------------------------------------

template<typename I> class scaling_iterator : public T_inheritable<I>::type {
	typedef it_element_t<I>	element;
	float	s;
public:
	scaling_iterator(I i, float _s) : T_inheritable<I>::type(i), s(_s)				{}
	scaling_iterator(const scaling_iterator &i) : T_inheritable<I>::type(i), s(i.s)	{}

	element			operator[](int i)	{ return T_inheritable<I>::type::operator[](i) * s; }
	element			operator*()			{ return T_inheritable<I>::type::operator*() * s; }
};
template<typename I> inline scaling_iterator<I> scaleby(I i, float s) { return scaling_iterator<I>(i, s); }

//-----------------------------------------------------------------------------
//	Accuracy Requirements
//-----------------------------------------------------------------------------

template<typename P, typename T> bool CheckAll(P p, T t, size_t count) {
	for (; count--; ++p) {
		if (*p != t)
			return false;
	}
	return true;
}

template<typename V, typename T, typename I> float GetAccuracy(float s, T srce_data, array<I,3> *indices, int ntris) {
	for (int i = 0; i < ntris; i++) {
		V	v0(srce_data[indices[i][0]]);
		V	v1(srce_data[indices[i][1]]);
		V	v2(srce_data[indices[i][2]]);

		if (float s0 = len(v0 - v1))
			s	= min(s0, s);
		if (float s1 = len(v0 - v2))
			s	= min(s1, s);
		if (float s2 = len(v1 - v2))
			s	= min(s2, s);
//		s	= min(min(s0, s1), s2);
	}
	return s;
}

template<typename V, typename T, typename I> uint32 GetAccuracy(V &minv, V &maxv, T srce_data, int nverts, array<I,3> *indices, int ntris) {
	V	v(srce_data[0]);
	minv	= maxv = v;
	for (int i = 1; i < nverts; i++) {
		V	v(srce_data[i]);
		minv	= min(minv, v);
		maxv	= max(maxv, v);
	}

	float	bs	= len(maxv - minv);
	float	s	= bs;
	for (int i = 0; i < ntris; i++) {
		V	v0(srce_data[indices[i][0]]);
		V	v1(srce_data[indices[i][1]]);
		V	v2(srce_data[indices[i][2]]);

		if (float s0 = len(v0 - v1))
			s	= min(s0, s);
		if (float s1 = len(v0 - v2))
			s	= min(s1, s);
		if (float s2 = len(v1 - v2))
			s	= min(s2, s);
//		s	= min(min(s0, s1), s2);
	}

	return uint32(log2(bs / s));
}

//-----------------------------------------------------------------------------
//	bitmap
//-----------------------------------------------------------------------------

enum TextureConv {
	TC_NODITHER		= 1,
	TC_ALPHA1BIT	= 2,
};

template<typename D, typename S> static void MakeTextureData(S *srce, int srce_pitch, int width, int height) {
	copy(
		make_strided_block(srce, width, srce_pitch, height),
		make_block((D*)ISO::iso_bin_allocator().alloc(width * height * sizeof(D)), width, height)
	);
}

bool		IsMonochrome(const block<ISO_rgba, 2> &block, float tolerance, ISO_rgba *col);
void		MakeMonochrome(bitmap &bm, const ISO_rgba &col);
rect		FindBitmapExtent(const bitmap &bm);
int			FindBitmapRegions(const bitmap &bm, rect *rects);

} //namespace iso

#endif // PLATFORMDATA_H
