#ifndef WIN32_SHARED_H
#define WIN32_SHARED_H

#include "base/strings.h"
#include "base/array.h"
#include "base/vector.h"
#include "extra/colour.h"

namespace iso { namespace win {

struct VersionLink {
	uint16	total_size;
	uint16	data_size;
	uint16	flags;	//0 - binary, 1 - text ?
	char16	name[1];

	const VersionLink	*sibling()	const { return (VersionLink*)((uint8*)this + align(total_size, 4)); }
	const VersionLink	*child()	const { return (VersionLink*)((uint8*)align(string_end(name) + 1, 4) + (data_size << (flags & 1))); }
	memory_block		data()		const { return memory_block((void*)align(string_end(name) + 1, 4), data_size << (flags & 1)); }
};

//-----------------------------------------------------------------------------
//	Handles
//-----------------------------------------------------------------------------

template<typename T> struct H {
protected:
	T		h;
	H()		: h(0)							{}
	H(T h)	: h(h)							{}
	void	Close()							{ CloseHandle(h); h = 0; }
	void	operator=(const T &_h)			{ CloseHandle(h); h = _h; }
public:
	operator T()					const	{ return h; }
//	operator LRESULT()				const	{ return (LRESULT)h;}
	bool	operator!()				const	{ return !h; }
	bool	Valid()					const	{ return !!h; }
	void	Destroy()						{ if (h) DeleteObject(h); h = 0; }
	T*		operator&()						{ return &h; }

	bool operator==(const H<T> &b)	const	{ return h == b.h; }
	bool operator!=(const H<T> &b)	const	{ return h != b.h; }
	friend string_accum& operator<<(string_accum &sa, const H &h) { return sa << hex((intptr_t)h.h); }
};

class global_base {
protected:
	HGLOBAL	h;
	struct temp {
		HGLOBAL h;
		void	*p;
		temp(HGLOBAL h) : h(h), p(GlobalLock(h)) {}
		~temp()					{ GlobalUnlock(h); }
		bool				operator!()		const	{ return !!p; }
		explicit			operator bool() const	{ return !!p; }
		operator			placement()		const	{ return p; }
		template<typename T> operator T*()	const	{ return (T*)p; }
		operator const_memory_block()		const	{ return const_memory_block(p, GlobalSize(h)); }
	};
public:
	global_base()			: h(0)	{}
	global_base(HGLOBAL h)	: h(h)	{}
	global_base(size_t size, uint32 flags = GMEM_FIXED) : h(GlobalAlloc(flags, size))	{}
	operator HGLOBAL()	const	{ return h; }

	size_t			size()		const	{ return GlobalSize(h); }
	arbitrary_ptr	lock()		const	{ return GlobalLock(h); }
	void			unlock()	const	{ GlobalUnlock(h); }
	void			free()		const	{ GlobalFree(h); }
	temp			data()		const	{ return temp(h); }

	global_base	Dup(uint32 flags = GMEM_FIXED) const {
		size_t	len		= size();
		global_base	dest(len, flags);
		memcpy(dest.data(), data(), len);
		return dest;
	}
};

template<typename T> struct global_ptr : global_base {
	struct typed_temp : temp {
		typed_temp(HGLOBAL h) : temp(h) {}
		T	*operator&()	const	{ return (T*)p; }
		template<typename T2, typename V = enable_if_t<T_conversion<T&,T2>::exists>> operator T2() const { return *(T*)p; }
	};
	global_ptr(HGLOBAL h)										: global_base(h) {}
//	global_ptr(T &&t, uint32 flags = GMEM_FIXED)				: global_base(sizeof(T), flags) { new(data()) T(move(t)); }
	global_ptr(const T &t, uint32 flags = GMEM_FIXED)			: global_base(sizeof(T), flags) { new(data()) T(t); }
	global_ptr(const T *t, size_t n, uint32 flags = GMEM_FIXED)	: global_base(sizeof(T) * n, flags) { memcpy(data(), t, sizeof(T) * n); }
	auto	operator*() { return typed_temp(h); }
};

template<typename T>	global_ptr<T>	MakeGlobal(const T &t)					{ return global_ptr<T>(t);}
template<typename T>	global_ptr<T>	MakeGlobal(global_ptr<T> &g)			{ return g; }
template<typename T>	global_ptr<T>	MakeGlobal(const T *t, size_t n, uint32 flags = GMEM_FIXED)	{ return global_ptr<T>(t, n, flags); }

inline					global_base		MakeGlobal(const global_base &g)		{ return g; }
inline					global_base		MakeGlobal(const const_memory_block &b, uint32 flags = GMEM_FIXED)	{ global_base g(b.length(), flags); b.copy_to(g.data()); return g; }

//-----------------------------------------------------------------------------
//	Colour
//-----------------------------------------------------------------------------
struct Colour {
	union {
		COLORREF	c;
		struct { uint8 r, g, b, x; };
	};
	enum col {
	//	black	= 0x000000,
	//	white	= 0xffffff,
	//	red		= 0x0000ff,
	//	green	= 0x00ff00,
	//	blue	= 0xff0000,
	//	cyan	= 0xffff00,
	//	magenta = 0xff00ff,
	//	yellow	= 0x00ffff,
		none	= 0xffffffff,
		unspec	= 0xff000000,
	};
#ifndef PLAT_WINRT
	static Colour	SysColor(int i)	{ return Colour(GetSysColor(i)); }
#endif
	static Colour	Grey(int i)		{ return Colour(i, i, i); }
	constexpr Colour() : c(0) {}
	explicit constexpr Colour(COLORREF c)		: c(c) {}
	constexpr Colour(col c)						: c(c) {}
	constexpr Colour(uint8 r, uint8 g, uint8 b, uint8 x = 0) : r(r), g(g), b(b), x(x) {}
	template<typename R, typename G, typename B> constexpr Colour(const colour::colour_const<R, G, B>&) : Colour(R() * 255_k, G() * 255_k, B() * 255_k) {}

	operator COLORREF() const	{ return c; }
	COLORREF rgb()		const	{ return c & 0xffffff; }
};
inline Colour operator*(Colour x, Colour y) { return Colour(x.r * y.r / 255, x.g * y.g / 255, x.b * y.b / 255); }

//-----------------------------------------------------------------------------
//	Point & Rect
//-----------------------------------------------------------------------------

struct xform : XFORM {
	static xform scale(float x, float y)		{ return xform(x, 0, 0, y, 0, 0); }
	static xform translation(float x, float y)	{ return xform(1,0,0,1,x,y); }
	static xform rotation(float c, float s)		{ return xform(c, s, -s, c, 0, 0); }
	static xform identity()						{ return xform(1,0,0,1,0,0); }

	xform(float m00, float m01, float m10, float m11, float tx, float ty) {
		eM11 = m00;
		eM12 = m01;
		eM21 = m10;
		eM22 = m11;
		eDx  = tx;
		eDy  = ty;
	}
	float	det() const {
		return eM11 * eM22 - eM12 * eM21;
	}
	xform	inverse() const {
		float	idet = 1 / det();
		return xform(
			eM22 * idet, -eM12 * idet,
			-eM21 * idet, eM11 * idet,
			(eM12 * eDy - eM22 * eDx) * idet,
			(eM21 * eDx - eM11 * eDy) * idet
		);
	}
};

struct Point : POINT {
	Point()									{}
	Point(const POINT &p)	: POINT(p)		{}
	Point(const POINTL &p)					{ x = p.x; y = p.y; }
	Point(const POINTS &p)					{ x = p.x; y = p.y; }
	Point(const SIZE &s)					{ x = s.cx; y = s.cy; }
	Point(int _x, int _y)					{ x = _x; y = _y; }
	explicit Point(LPARAM lp)				{ x = short(LOWORD(lp)); y = short(HIWORD(lp)); }

	operator	point()			const		{ return {x, y}; }

	Point&	operator+=(const Point &p)		{ x += p.x; y += p.y; return *this; }
	Point&	operator-=(const Point &p)		{ x -= p.x; y -= p.y; return *this; }
	template<typename T> inline Point &operator*=(const T &t)	{ x *= t; y *= t; return *this; }
	template<typename T> inline Point &operator/=(const T &t)	{ x /= t; y /= t; return *this; }
	Point&	operator*=( const xform &xf)	{ int x0 = x; x = int(xf.eM11 * x0 + xf.eM12 * y + xf.eDx); y = int(xf.eM21 * x0 + xf.eM22 * y + xf.eDy); return *this; }
	Point&	operator/=( const xform &xf)	{ return *this *= xf.inverse(); }
	int		operator[](bool v)	const		{ return (&x)[int(v)]; }
	LONG&	operator[](bool v)				{ return (&x)[int(v)]; }

	friend Point	operator+(const Point &p1, const Point &p2)				{ return Point(p1.x + p2.x, p1.y + p2.y); }
	friend Point	operator-(const Point &p1, const Point &p2)				{ return Point(p1.x - p2.x, p1.y - p2.y); }
	template<typename T> friend Point operator*(const Point &p, const T &t)	{ return Point(int(p.x * t), int(p.y * t)); }
	template<typename T> friend Point operator/(const Point &p, const T &t)	{ return Point(int(p.x / t), int(p.y / t)); }
	friend Point	operator*(const Point &p, const xform &x)				{ return Point(int(x.eM11 * p.x + x.eM12 * p.y + x.eDx), int(x.eM21 * p.x + x.eM22 * p.y + x.eDy)); }
	friend Point	operator/(const Point &p, const xform &x)				{ return p * x.inverse(); }
	friend bool		operator==(const Point &p1, const Point &p2)			{ return p1.x == p2.x && p1.y == p2.y; }
	friend bool		operator!=(const Point &p1, const Point &p2)			{ return !(p1 == p2); }
	friend Point	align(const Point &p, const Point &a)					{ return Point(iso::align(p.x, a.x), iso::align(p.y, a.y)); }
};

struct Rect : RECT {
	Rect()	{}
	Rect(const RECT &r) : RECT(r)		{}
	Rect(int x, int y, int w, int h)	{ left = x; top = y; right = x + w; bottom = y + h;}
	Rect(const Point &p1, const Point &p2)	{ left = p1.x; top = p1.y; right = p2.x; bottom = p2.y;}

	const Point&operator[](int i) const	{ return ((Point*)this)[i]; }
	Point&		operator[](int i)		{ return ((Point*)this)[i]; }

	operator	rect()			const	{ return rect(TopLeft(), BottomRight()); }

	int			Left()			const	{ return left; }
	int			Top()			const	{ return top; }
	int			Right()			const	{ return right; }
	int			Bottom()		const	{ return bottom; }
	int			Width()			const	{ return right - left; }
	int			Height()		const	{ return bottom - top; }

	Point&		TopLeft()				{ return ((Point*)this)[0]; }
	Point&		BottomRight()			{ return ((Point*)this)[1]; }
	const Point& TopLeft()		const	{ return ((Point*)this)[0]; }
	const Point& BottomRight()	const	{ return ((Point*)this)[1]; }
	Point		BottomLeft()	const	{ return Point(left, bottom); }
	Point		TopRight()		const	{ return Point(right, top); }
	Point		Size()			const	{ return Point(Width(), Height()); }
	Point		Centre()		const	{ return Point((left + right) / 2, (top + bottom) / 2); }

	Rect&		SetPos(int x, int y)	{ right = x + Width(); bottom = y + Height(); left = x; top = y; return *this; }
	Rect&		SetPos(const Point &p)	{ return SetPos(p.x, p.y); }
	Rect&		SetSize(int w, int h)	{ right = left + w; bottom = top + h; return *this; }
	Rect&		SetSize(const Point &p)	{ return SetSize(p.x, p.y); }
	Rect&		SetLeft(int x)			{ left	= x; return *this; }
	Rect&		SetRight(int x)			{ right	= x; return *this; }
	Rect&		SetTop(int y)			{ top	= y; return *this; }
	Rect&		SetBottom(int y)		{ bottom= y; return *this; }

	Rect		Grow(int x0, int y0, int x1, int y1)	const { return Rect(left - x0, top - y0, Width() + x0 + x1, Height() + y0 + y1); }
	Rect		Centre(int w, int h)					const { return Rect(left + (Width() - w) / 2, top + (Height() - h) / 2, w, h); }
	Rect		Centre(const Point &p)					const { return Centre(p.x, p.y); }
	Rect		Subbox(int x, int y, int w, int h)		const {
		Rect r;
		r.left		= x < 0 ? right  + x	: left	 + x;
		r.top		= y < 0 ? bottom + y	: top	 + y;
		r.right		= w > 0 ? r.left + w	: right  + w;
		r.bottom	= h > 0 ? r.top  + h	: bottom + h;
		return r;
	}
	Rect		Adjust(float xs, float ys, float xp, float yp) const {
		int	w	= int(Width() * xs), h = int(Height() * ys);
		return Rect(left + int((right - left - w) * xp), top + int((bottom - top - h) * yp), w, h);
	}

	void		SplitAtX(int x, Rect &r1, Rect &r2) const {
		int	w = Width(), h = Height(), x0 = left, y0 = top;
		if (x < 0)
			x += w;
		r1	= Rect(x0, y0, x, h);
		r2	= Rect(x0 + x, y0, w - x, h);
	}

	void		SplitAtY(int y, Rect &r1, Rect &r2) const {
		int	w = Width(), h = Height(), x0 = left, y0 = top;
		if (y < 0)
			y += h;
		r1	= Rect(x0, y0, w, y);
		r2	= Rect(x0, y0 + y, w, h - y);
	}

	void		SplitX(float s, Rect &r1, Rect &r2) const { SplitAtX(int(Width()  * s), r1, r2); }
	void		SplitY(float s, Rect &r1, Rect &r2) const { SplitAtY(int(Height() * s), r1, r2); }
	bool		Contains(const Point &p)			const { return p.x >= left && p.x < right && p.y >= top && p.y < bottom; }
	bool		Contains(const Rect &r)				const { return r.left >= left && r.right <= right && r.top >= top && r.bottom <= bottom; }
	bool		Overlaps(const Rect &r)				const { return r.left < Right() && r.right > Left() && r.top < Bottom() && r.bottom > Top(); }

	//operators
	inline Rect& operator+=(const Point &p)	{ left += p.x; right += p.x; top += p.y; bottom += p.y; return *this; }
	inline Rect& operator-=(const Point &p)	{ left -= p.x; right -= p.x; top -= p.y; bottom -= p.y; return *this; }
	inline Rect& operator|=(const Point &p)	{ left = min(left, p.x); right = max(right, p.x); top = min(top, p.y); bottom = max(bottom, p.y); return *this; }

	inline Rect& operator+=(const Rect &r)	{ left += r.left; right += r.right; top += r.top; bottom += r.bottom; return *this; }
	inline Rect& operator-=(const Rect &r)	{ left -= r.left; right -= r.right; top -= r.top; bottom -= r.bottom; return *this; }
	inline Rect& operator|=(const Rect &r)	{ left = min(left, r.left); right = max(right, r.right); top = min(top, r.top); bottom = max(bottom, r.bottom); return *this; }
	inline Rect& operator&=(const Rect &r)	{ left = max(left, r.left); right = min(right, r.right); top = max(top, r.top); bottom = min(bottom, r.bottom); return *this; }

	template<typename T> inline Rect& operator*=(const T &t)		{ TopLeft() *= t; BottomRight() *= t; return *this; }
	template<typename T> inline Rect& operator/=(const T &t)		{ TopLeft() /= t; BottomRight() /= t; return *this; }
	inline Rect& operator*=(const xform &x)	{ TopLeft() *= x; BottomRight() *= x; return *this; }
	inline Rect& operator/=(const xform &x)	{ return *this *= x.inverse(); }

	//friends
	friend Rect	operator+(const Rect &r, const Point &p)	{ return Rect(r) += p; }
	friend Rect	operator-(const Rect &r, const Point &p)	{ return Rect(r) -= p; }
	friend Rect	operator|(const Rect &r, const Point &p)	{ return Rect(r) |= p; }

	friend Rect operator+(const Rect &r1, const Rect &r2)	{ return Rect(r1) += r2; }
	friend Rect operator-(const Rect &r1, const Rect &r2)	{ return Rect(r1) -= r2; }
	friend Rect	operator|(const Rect &r1, const Rect &r2)	{ return Rect(r1) |= r2; }
	friend Rect	operator&(const Rect &r1, const Rect &r2)	{ return Rect(r1) &= r2; }

	template<typename T> friend Rect operator*(const Rect &r, const T &t)	{ return Rect(r) *= t; }
	template<typename T> friend Rect operator/(const Rect &r, const T &t)	{ return Rect(r) /= t; }
	friend Rect operator*(const Rect &r, const xform &x)	{ return Rect(r.TopLeft() * x, r.BottomRight() * x); }
	friend Rect operator/(const Rect &r, const xform &x)	{ return r * x.inverse(); }

	friend bool	operator==(const Rect &r1, const Rect &r2)	{ return r1.TopLeft() == r2.TopLeft() && r1.BottomRight() == r2.BottomRight(); }
	friend bool	operator!=(const Rect &r1, const Rect &r2)	{ return !(r1 == r2); }
};


} } // namespace iso::win

#endif	// WINDOW_H
