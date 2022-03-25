#ifndef SWIZZLE_H
#define SWIZZLE_H

#include "base/defs.h"

namespace iso {


template<typename P> struct swizzle_padding {};
template<typename X, int O> struct swizzle_padding<offset_type<X, O>> : spacer<O> {};
template<typename P> struct swizzle_type_s { typedef P type; };
template<typename X, int O> struct swizzle_type_s<offset_type<X, O>> { typedef X type; };
template<typename P> using swizzle_type = typename swizzle_type_s<P>::type;

//-----------------------------------------------------------------------------
// struct swizzles<N, V, P>
//-----------------------------------------------------------------------------

// generic case
// defines lo, hi, even, odd, x, y, z, w, xy, xyz, xyzw

template<int N, typename _V, template<int...> class P> struct swizzles {
	typedef _V	V;
	template<typename I>	struct gen;
	template<size_t... I>	struct gen<meta::value_list<int, I...>> { typedef P<I...> type; };
	union {
		V				v;
		typename gen<meta::make_value_sequence<N / 2>>::type						lo;
		typename gen<meta::muladd<1, N/2, meta::make_value_sequence<N / 2>>>::type	hi;
		typename gen<meta::muladd<2, 0, meta::make_value_sequence<N / 2>>>::type	even;
		typename gen<meta::muladd<2, 1, meta::make_value_sequence<N / 2>>>::type	odd;

		P<0>			x;
		P<1>			y;
		P<2>			z;
		P<3>			w;

		P<0, 1>			xy;
		P<0, 1, 2>		xyz;
		P<0, 1, 2, 3>	xyzw;
	};

	swizzles() {}
	constexpr swizzles(V v) : v(v) {}
	constexpr swizzles(const swizzles &b) : v(b.v) {}
	constexpr	operator V() const	{ return v; }
	template<int...I> friend force_inline auto& swizzle(const swizzles &p)	{ return reinterpret_cast<const P<I...>&>(p); }
	template<int...I> friend force_inline auto& swizzle(swizzles &p)		{ return reinterpret_cast<P<I...>&>(p); }
};

//-----------------------------------------------------------------------------
// swizzles for 1 component
//-----------------------------------------------------------------------------

template<typename _V, template<int...> class P> struct swizzles<1, _V, P> {
	typedef	_V	V;
	template<int... I> using PC = const P<I...>;
	union {
		V				v;
		P<0>			x;
		PC<0, 0>		xx;
		PC<0, 0, 0>		xxx;
		PC<0, 0, 0, 0>	xxxx;
	};
	swizzles() {}
	constexpr swizzles(V v)	: v(v) {}
	constexpr swizzles(const swizzles &b) : v(b.v) {}
	constexpr operator V() const { return v; }
	template<int...I> friend force_inline auto& swizzle(const swizzles &p)	{ return reinterpret_cast<const P<I...>&>(p); }
	template<int...I> friend force_inline auto& swizzle(swizzles &p)		{ return reinterpret_cast<P<I...>&>(p); }
};

//-----------------------------------------------------------------------------
// swizzles for 2 components
//-----------------------------------------------------------------------------

template<typename _V, template<int...> class P> struct swizzles<2, _V, P> {
	typedef	_V	V;
	template<int... I> using PC = const P<I...>;
	union {
		V				v;
		// LHS + RHS
		P<0>			x;			P<1>			y;
		P<0>			lo;			P<1>			hi;
		P<0, 1>			xy;			P<1, 0>			yx;

		// RHS only
		PC<0, 0>		xx;			PC<1, 1>		yy;

		PC<0, 0, 0>		xxx;		PC<0, 0, 1>		xxy;		PC<0, 1, 0>		xyx;		PC<0, 1, 1>		xyy;
		PC<1, 0, 0>		yxx;		PC<1, 0, 1>		yxy;		PC<1, 1, 0>		yyx;		PC<1, 1, 1>		yyy;

		PC<0, 0, 0, 0>	xxxx;		PC<0, 0, 0, 1>	xxxy;		PC<0, 0, 1, 0>	xxyx;		PC<0, 0, 1, 1>	xxyy;
		PC<0, 1, 0, 0>	xyxx;		PC<0, 1, 0, 1>	xyxy;		PC<0, 1, 1, 0>	xyyx;		PC<0, 1, 1, 1>	xyyy;
		PC<1, 0, 0, 0>	yxxx;		PC<1, 0, 0, 1>	yxxy;		PC<1, 0, 1, 0>	yxyx;		PC<1, 0, 1, 1>	yxyy;
		PC<1, 1, 0, 0>	yyxx;		PC<1, 1, 0, 1>	yyxy;		PC<1, 1, 1, 0>	yyyx;		PC<1, 1, 1, 1>	yyyy;
	};

	swizzles() {}
	constexpr swizzles(V v)	: v(v) {}
	constexpr swizzles(const swizzles &b) : v(b.v) {}
	constexpr	operator V() const	{ return v; }
	template<int...I> friend force_inline auto& swizzle(const swizzles &p)	{ return reinterpret_cast<const P<I...>&>(p); }
	template<int...I> friend force_inline auto& swizzle(swizzles &p)		{ return reinterpret_cast<P<I...>&>(p); }
};

//-----------------------------------------------------------------------------
// swizzles for 3 components
//-----------------------------------------------------------------------------

template<typename _V, template<int...> class P> struct swizzles<3, _V, P> {
	typedef	_V	V;
	template<int... I> using PC = const P<I...>;
	union {
		V				v;
		// LHS + RHS
		P<0>			x;			P<1>			y;			P<2>		z;

		P<0, 1>			xy;			P<0, 2>			xz;
		P<1, 0>			yx;			P<1, 2>			yz;
		P<2, 0>			zx;			P<2, 1>			zy;

		P<0, 1, 2>		xyz;		P<0, 2, 1>		xzy;
		P<1, 0, 2>		yxz;		P<1, 2, 0>		yzx;
		P<2, 0, 1>		zxy;		P<2, 1, 0>		zyx;

		// RHS only
		PC<0, 0>		xx;			PC<1, 1>		yy;			PC<2, 2>		zz;

		PC<0, 0, 0>		xxx;		PC<0, 0, 1>		xxy;		PC<0, 0, 2>		xxz;		PC<0, 1, 0>		xyx;
		PC<0, 1, 1>		xyy;		PC<0, 2, 0>		xzx;		PC<0, 2, 2>		xzz;

		PC<1, 0, 0>		yxx;		PC<1, 0, 1>		yxy;		PC<1, 1, 0>		yyx;		PC<1, 1, 1>		yyy;
		PC<1, 1, 2>		yyz;		PC<1, 2, 1>		yzy;		PC<1, 2, 2>		yzz;

		PC<2, 0, 0>		zxx;		PC<2, 0, 2>		zxz;		PC<2, 1, 1>		zyy;		PC<2, 1, 2>		zyz;
		PC<2, 2, 0>		zzx;		PC<2, 2, 1>		zzy;		PC<2, 2, 2>		zzz;

		PC<0, 0, 0, 0>	xxxx;		PC<0, 0, 0, 1>	xxxy;		PC<0, 0, 0, 2>	xxxz;		PC<0, 0, 1, 0>	xxyx;
		PC<0, 0, 1, 1>	xxyy;		PC<0, 0, 1, 2>	xxyz;		PC<0, 0, 2, 0>	xxzx;		PC<0, 0, 2, 1>	xxzy;
		PC<0, 0, 2, 2>	xxzz;		PC<0, 1, 0, 0>	xyxx;		PC<0, 1, 0, 1>	xyxy;		PC<0, 1, 0, 2>	xyxz;
		PC<0, 1, 1, 0>	xyyx;		PC<0, 1, 1, 1>	xyyy;		PC<0, 1, 1, 2>	xyyz;		PC<0, 1, 2, 0>	xyzx;
		PC<0, 1, 2, 1>	xyzy;		PC<0, 1, 2, 2>	xyzz;		PC<0, 2, 0, 0>	xzxx;		PC<0, 2, 0, 1>	xzxy;
		PC<0, 2, 0, 2>	xzxz;		PC<0, 2, 1, 0>	xzyx;		PC<0, 2, 1, 1>	xzyy;		PC<0, 2, 1, 2>	xzyz;
		PC<0, 2, 2, 0>	xzzx;		PC<0, 2, 2, 1>	xzzy;		PC<0, 2, 2, 2>	xzzz;

		PC<1, 0, 0, 0>	yxxx;		PC<1, 0, 0, 1>	yxxy;		PC<1, 0, 0, 2>	yxxz;		PC<1, 0, 1, 0>	yxyx;
		PC<1, 0, 1, 1>	yxyy;		PC<1, 0, 1, 2>	yxyz;		PC<1, 0, 2, 0>	yxzx;		PC<1, 0, 2, 1>	yxzy;
		PC<1, 0, 2, 2>	yxzz;		PC<1, 1, 0, 0>	yyxx;		PC<1, 1, 0, 1>	yyxy;		PC<1, 1, 0, 2>	yyxz;
		PC<1, 1, 1, 0>	yyyx;		PC<1, 1, 1, 1>	yyyy;		PC<1, 1, 1, 2>	yyyz;		PC<1, 1, 2, 0>	yyzx;
		PC<1, 1, 2, 1>	yyzy;		PC<1, 1, 2, 2>	yyzz;		PC<1, 2, 0, 0>	yzxx;		PC<1, 2, 0, 1>	yzxy;
		PC<1, 2, 0, 2>	yzxz;		PC<1, 2, 1, 0>	yzyx;		PC<1, 2, 1, 1>	yzyy;		PC<1, 2, 1, 2>	yzyz;
		PC<1, 2, 2, 0>	yzzx;		PC<1, 2, 2, 1>	yzzy;		PC<1, 2, 2, 2>	yzzz;

		PC<2, 0, 0, 0>	zxxx;		PC<2, 0, 0, 1>	zxxy;		PC<2, 0, 0, 2>	zxxz;		PC<2, 0, 1, 0>	zxyx;
		PC<2, 0, 1, 1>	zxyy;		PC<2, 0, 1, 2>	zxyz;		PC<2, 0, 2, 0>	zxzx;		PC<2, 0, 2, 1>	zxzy;
		PC<2, 0, 2, 2>	zxzz;		PC<2, 1, 0, 0>	zyxx;		PC<2, 1, 0, 1>	zyxy;		PC<2, 1, 0, 2>	zyxz;
		PC<2, 1, 1, 0>	zyyx;		PC<2, 1, 1, 1>	zyyy;		PC<2, 1, 1, 2>	zyyz;		PC<2, 1, 2, 0>	zyzx;
		PC<2, 1, 2, 1>	zyzy;		PC<2, 1, 2, 2>	zyzz;		PC<2, 2, 0, 0>	zzxx;		PC<2, 2, 0, 1>	zzxy;
		PC<2, 2, 0, 2>	zzxz;		PC<2, 2, 1, 0>	zzyx;		PC<2, 2, 1, 1>	zzyy;		PC<2, 2, 1, 2>	zzyz;
		PC<2, 2, 2, 0>	zzzx;		PC<2, 2, 2, 1>	zzzy;		PC<2, 2, 2, 2>	zzzz;
	};
	swizzles() {}
	constexpr swizzles(V v)	: v(v) {}
	constexpr swizzles(const swizzles &b) : v(b.v) {}
	constexpr	operator V() const	{ return v; }
	template<int...I> friend force_inline auto& swizzle(const swizzles &p)	{ return reinterpret_cast<const P<I...>&>(p); }
	template<int...I> friend force_inline auto& swizzle(swizzles &p)		{ return reinterpret_cast<P<I...>&>(p); }
};

//-----------------------------------------------------------------------------
// swizzles for 4 components
//-----------------------------------------------------------------------------

template<typename _V, template<int...> class P> struct swizzles<4, _V, P> {
	typedef	_V	V;
	template<int... I> using PC = const P<I...>;
	union {
		V				v;

		// LHS + RHS
		P<0, 2>			even;		P<1, 3>			odd;
		P<0, 1>			lo;			P<2, 3>			hi;

		P<0>			x;			P<1>			y;			P<2>			z;			P<3>			w;

		P<0, 1>			xy;			P<0, 2>			xz;			P<0, 3>			xw;
		P<1, 0>			yx;			P<1, 2>			yz;			P<1, 3>			yw;
		P<2, 0>			zx;			P<2, 1>			zy;			P<2, 3>			zw;
		P<3, 0>			wx;			P<3, 1>			wy;			P<3, 2>			wz;

		P<0, 1, 2>		xyz;		P<0, 1, 3>		xyw;		P<0, 2, 1>		xzy;		P<0, 2, 3>		xzw;
		P<0, 3, 1>		xwy;		P<0, 3, 2>		xwz;		P<1, 0, 2>		yxz;		P<1, 0, 3>		yxw;
		P<1, 2, 0>		yzx;		P<1, 2, 3>		yzw;		P<1, 3, 0>		ywx;		P<1, 3, 2>		ywz;
		P<2, 0, 1>		zxy;		P<2, 0, 3>		zxw;		P<2, 1, 0>		zyx;		P<2, 1, 3>		zyw;
		P<2, 3, 0>		zwx;		P<2, 3, 1>		zwy;		P<3, 0, 1>		wxy;		P<3, 0, 2>		wxz;
		P<3, 1, 0>		wyx;		P<3, 1, 2>		wyz;		P<3, 2, 0>		wzx;		P<3, 2, 1>		wzy;

		P<0, 1, 2, 3>	xyzw;		P<0, 1, 3, 2>	xywz;		P<0, 2, 1, 3>	xzyw;		P<0, 2, 3, 1>	xzwy;
		P<0, 3, 1, 2>	xwyz;		P<0, 3, 2, 1>	xwzy;		P<1, 0, 2, 3>	yxzw;		P<1, 0, 3, 2>	yxwz;
		P<1, 2, 0, 3>	yzxw;		P<1, 2, 3, 0>	yzwx;		P<1, 3, 0, 2>	ywxz;		P<1, 3, 2, 0>	ywzw;
		P<2, 0, 1, 3>	zxyw;		P<2, 0, 3, 1>	zxwy;		P<2, 1, 0, 3>	zyxw;		P<2, 1, 3, 0>	zywx;
		P<2, 3, 0, 1>	zwxy;		P<2, 3, 1, 0>	zwyx;		P<3, 0, 1, 2>	wxyz;		P<3, 0, 2, 1>	wxzy;
		P<3, 1, 0, 2>	wyxz;		P<3, 1, 2, 0>	wyzx;		P<3, 2, 0, 1>	wzxy;		P<3, 2, 1, 0>	wzyx;

		// RHS only
		PC<0, 0>		xx;			PC<1, 1>		yy;			PC<2, 2>		zz;			PC<3, 3>	ww;

		PC<0, 0, 0>		xxx;		PC<0, 1, 0>		xyx;		PC<0, 2, 0>		xzx;		PC<0, 3, 0>		xwx;
		PC<0, 0, 1>		xxy;		PC<0, 1, 1>		xyy;		PC<0, 0, 2>		xxz;		PC<0, 2, 2>		xzz;
		PC<0, 0, 3>		xxw;		PC<0, 3, 3>		xww;

		PC<1, 0, 0>		yxx;		PC<1, 1, 0>		yyx;		PC<1, 0, 1>		yxy;		PC<1, 1, 1>		yyy;
		PC<1, 2, 1>		yzy;		PC<1, 3, 1>		ywy;		PC<1, 1, 2>		yyz;		PC<1, 2, 2>		yzz;
		PC<1, 1, 3>		yyw;		PC<1, 3, 3>		yww;

		PC<2, 0, 0>		zxx;		PC<2, 2, 0>		zzx;		PC<2, 1, 1>		zyy;		PC<2, 2, 1>		zzy;
		PC<2, 0, 2>		zxz;		PC<2, 1, 2>		zyz;		PC<2, 2, 2>		zzz;		PC<2, 3, 2>		zwz;
		PC<2, 2, 3>		zzw;		PC<2, 3, 3>		zww;

		PC<3, 0, 0>		wxx;		PC<3, 3, 0>		wwx;		PC<3, 1, 1>		wyy;		PC<3, 3, 1>		wwy;
		PC<3, 2, 2>		wzz;		PC<3, 3, 2>		wwz;		PC<3, 0, 3>		wxw;		PC<3, 1, 3>		wyw;
		PC<3, 2, 3>		wzw;		PC<3, 3, 3>		www;

		PC<0, 0, 0, 0>	xxxx;		PC<0, 0, 0, 1>	xxxy;		PC<0, 0, 0, 2>	xxxz;		PC<0, 0, 0, 3>	xxxw;
		PC<0, 0, 1, 0>	xxyx;		PC<0, 0, 1, 1>	xxyy;		PC<0, 0, 1, 2>	xxyz;		PC<0, 0, 1, 3>	xxyw;
		PC<0, 0, 2, 0>	xxzx;		PC<0, 0, 2, 1>	xxzy;		PC<0, 0, 2, 2>	xxzz;		PC<0, 0, 2, 3>	xxzw;
		PC<0, 0, 3, 0>	xxwx;		PC<0, 0, 3, 1>	xxwy;		PC<0, 0, 3, 2>	xxwz;		PC<0, 0, 3, 3>	xxww;
		PC<0, 1, 0, 0>	xyxx;		PC<0, 1, 0, 1>	xyxy;		PC<0, 1, 0, 2>	xyxz;		PC<0, 1, 0, 3>	xyxw;
		PC<0, 1, 1, 0>	xyyx;		PC<0, 1, 1, 1>	xyyy;		PC<0, 1, 1, 2>	xyyz;		PC<0, 1, 1, 3>	xyyw;
		PC<0, 1, 2, 0>	xyzx;		PC<0, 1, 2, 1>	xyzy;		PC<0, 1, 2, 2>	xyzz;		PC<0, 1, 3, 0>	xywx;
		PC<0, 1, 3, 1>	xywy;		PC<0, 1, 3, 3>	xyww;		PC<0, 2, 0, 0>	xzxx;		PC<0, 2, 0, 1>	xzxy;
		PC<0, 2, 0, 2>	xzxz;		PC<0, 2, 0, 3>	xzxw;		PC<0, 2, 1, 0>	xzyx;		PC<0, 2, 1, 1>	xzyy;
		PC<0, 2, 1, 2>	xzyz;		PC<0, 2, 2, 0>	xzzx;		PC<0, 2, 2, 1>	xzzy;		PC<0, 2, 2, 2>	xzzz;
		PC<0, 2, 2, 3>	xzzw;		PC<0, 2, 3, 0>	xzwx;		PC<0, 2, 3, 2>	xzwz;		PC<0, 2, 3, 3>	xzww;
		PC<0, 3, 0, 0>	xwxx;		PC<0, 3, 0, 1>	xwxy;		PC<0, 3, 0, 2>	xwxz;		PC<0, 3, 0, 3>	xwxw;
		PC<0, 3, 1, 0>	xwyx;		PC<0, 3, 1, 1>	xwyy;		PC<0, 3, 1, 3>	xwyw;		PC<0, 3, 2, 0>	xwzx;
		PC<0, 3, 2, 2>	xwzz;		PC<0, 3, 2, 3>	xwzw;		PC<0, 3, 3, 0>	xwwx;		PC<0, 3, 3, 1>	xwwy;
		PC<0, 3, 3, 2>	xwwz;		PC<0, 3, 3, 3>	xwww;

		PC<1, 0, 0, 0>	yxxx;		PC<1, 0, 0, 1>	yxxy;		PC<1, 0, 0, 2>	yxxz;		PC<1, 0, 0, 3>	yxxw;
		PC<1, 0, 1, 0>	yxyx;		PC<1, 0, 1, 1>	yxyy;		PC<1, 0, 1, 2>	yxyz;		PC<1, 0, 1, 3>	yxyw;
		PC<1, 0, 2, 0>	yxzx;		PC<1, 0, 2, 1>	yxzy;		PC<1, 0, 2, 2>	yxzz;		PC<1, 0, 3, 0>	yxwx;
		PC<1, 0, 3, 1>	yxwy;		PC<1, 0, 3, 3>	yxww;		PC<1, 1, 0, 0>	yyxx;		PC<1, 1, 0, 1>	yyxy;
		PC<1, 1, 0, 2>	yyxz;		PC<1, 1, 0, 3>	yyxw;		PC<1, 1, 1, 0>	yyyx;		PC<1, 1, 1, 1>	yyyy;
		PC<1, 1, 1, 2>	yyyz;		PC<1, 1, 1, 3>	yyyw;		PC<1, 1, 2, 0>	yyzx;		PC<1, 1, 2, 1>	yyzy;
		PC<1, 1, 2, 2>	yyzz;		PC<1, 1, 2, 3>	yyzw;		PC<1, 1, 3, 0>	yywx;		PC<1, 1, 3, 1>	yywy;
		PC<1, 1, 3, 2>	yywz;		PC<1, 1, 3, 3>	yyww;		PC<1, 2, 0, 0>	yzxx;		PC<1, 2, 0, 1>	yzxy;
		PC<1, 2, 0, 2>	yzxz;		PC<1, 2, 1, 0>	yzyx;		PC<1, 2, 1, 1>	yzyy;		PC<1, 2, 1, 2>	yzyz;
		PC<1, 2, 1, 3>	yzyw;		PC<1, 2, 2, 0>	yzzx;		PC<1, 2, 2, 1>	yzzy;		PC<1, 2, 2, 2>	yzzz;
		PC<1, 2, 2, 3>	yzzw;		PC<1, 2, 3, 1>	yzwy;		PC<1, 2, 3, 2>	yzwz;		PC<1, 2, 3, 3>	yzww;
		PC<1, 3, 0, 0>	ywxx;		PC<1, 3, 0, 1>	ywxy;		PC<1, 3, 0, 3>	ywxw;		PC<1, 3, 1, 0>	ywyx;
		PC<1, 3, 1, 1>	ywyy;		PC<1, 3, 1, 2>	ywyz;		PC<1, 3, 1, 3>	ywyw;		PC<1, 3, 2, 0>	ywzx;
		PC<1, 3, 2, 1>	ywzy;		PC<1, 3, 2, 2>	ywzz;		PC<1, 3, 3, 0>	ywwx;		PC<1, 3, 3, 1>	ywwy;
		PC<1, 3, 3, 2>	ywwz;		PC<1, 3, 3, 3>	ywww;

		PC<2, 0, 0, 0>	zxxx;		PC<2, 0, 0, 1>	zxxy;		PC<2, 0, 0, 2>	zxxz;		PC<2, 0, 0, 3>	zxxw;
		PC<2, 0, 1, 0>	zxyx;		PC<2, 0, 1, 1>	zxyy;		PC<2, 0, 1, 2>	zxyz;		PC<2, 0, 2, 0>	zxzx;
		PC<2, 0, 2, 1>	zxzy;		PC<2, 0, 2, 2>	zxzz;		PC<2, 0, 2, 3>	zxzw;		PC<2, 0, 3, 0>	zxwx;
		PC<2, 0, 3, 2>	zxwz;		PC<2, 0, 3, 3>	zxww;		PC<2, 1, 0, 0>	zyxx;		PC<2, 1, 0, 1>	zyxy;
		PC<2, 1, 0, 2>	zyxz;		PC<2, 1, 1, 0>	zyyx;		PC<2, 1, 1, 1>	zyyy;		PC<2, 1, 1, 2>	zyyz;
		PC<2, 1, 1, 3>	zyyw;		PC<2, 1, 2, 0>	zyzx;		PC<2, 1, 2, 1>	zyzy;		PC<2, 1, 2, 2>	zyzz;
		PC<2, 1, 2, 3>	zyzw;		PC<2, 1, 3, 1>	zywy;		PC<2, 1, 3, 2>	zywz;		PC<2, 1, 3, 3>	zyww;
		PC<2, 2, 0, 0>	zzxx;		PC<2, 2, 0, 1>	zzxy;		PC<2, 2, 0, 2>	zzxz;		PC<2, 2, 0, 3>	zzxw;
		PC<2, 2, 1, 0>	zzyx;		PC<2, 2, 1, 1>	zzyy;		PC<2, 2, 1, 2>	zzyz;		PC<2, 2, 1, 3>	zzyw;
		PC<2, 2, 2, 0>	zzzx;		PC<2, 2, 2, 1>	zzzy;		PC<2, 2, 2, 2>	zzzz;		PC<2, 2, 2, 3>	zzzw;
		PC<2, 2, 3, 0>	zzwx;		PC<2, 2, 3, 1>	zzwy;		PC<2, 2, 3, 2>	zzwz;		PC<2, 2, 3, 3>	zzww;
		PC<2, 3, 0, 0>	zwxx;		PC<2, 3, 0, 2>	zwxz;		PC<2, 3, 0, 3>	zwxw;		PC<2, 3, 1, 1>	zwyy;
		PC<2, 3, 1, 2>	zwyz;		PC<2, 3, 1, 3>	zwyw;		PC<2, 3, 2, 0>	zwzx;		PC<2, 3, 2, 1>	zwzy;
		PC<2, 3, 2, 2>	zwzz;		PC<2, 3, 2, 3>	zwzw;		PC<2, 3, 3, 0>	zwwx;		PC<2, 3, 3, 1>	zwwy;
		PC<2, 3, 3, 2>	zwwz;		PC<2, 3, 3, 3>	zwww;

		PC<3, 0, 0, 0>	wxxx;		PC<3, 0, 0, 1>	wxxy;		PC<3, 0, 0, 2>	wxxz;		PC<3, 0, 0, 3>	wxxw;
		PC<3, 0, 1, 0>	wxyx;		PC<3, 0, 1, 1>	wxyy;		PC<3, 0, 1, 3>	wxyw;		PC<3, 0, 2, 0>	wxzx;
		PC<3, 0, 2, 2>	wxzz;		PC<3, 0, 2, 3>	wxzw;		PC<3, 0, 3, 0>	wxwx;		PC<3, 0, 3, 1>	wxwy;
		PC<3, 0, 3, 2>	wxwz;		PC<3, 0, 3, 3>	wxww;		PC<3, 1, 0, 0>	wyxx;		PC<3, 1, 0, 1>	wyxy;
		PC<3, 1, 0, 3>	wyxw;		PC<3, 1, 1, 0>	wyyx;		PC<3, 1, 1, 1>	wyyy;		PC<3, 1, 1, 2>	wyyz;
		PC<3, 1, 1, 3>	wyyw;		PC<3, 1, 2, 1>	wyzy;		PC<3, 1, 2, 2>	wyzz;		PC<3, 1, 2, 3>	wyzw;
		PC<3, 1, 3, 0>	wywx;		PC<3, 1, 3, 1>	wywy;		PC<3, 1, 3, 2>	wywz;		PC<3, 1, 3, 3>	wyww;
		PC<3, 2, 0, 0>	wzxx;		PC<3, 2, 0, 2>	wzxz;		PC<3, 2, 0, 3>	wzxw;		PC<3, 2, 1, 1>	wzyy;
		PC<3, 2, 1, 2>	wzyz;		PC<3, 2, 1, 3>	wzyw;		PC<3, 2, 2, 0>	wzzx;		PC<3, 2, 2, 1>	wzzy;
		PC<3, 2, 2, 2>	wzzz;		PC<3, 2, 2, 3>	wzzw;		PC<3, 2, 3, 0>	wzwx;		PC<3, 2, 3, 1>	wzwy;
		PC<3, 2, 3, 2>	wzwz;		PC<3, 2, 3, 3>	wzww;		PC<3, 3, 0, 0>	wwxx;		PC<3, 3, 0, 1>	wwxy;
		PC<3, 3, 0, 2>	wwxz;		PC<3, 3, 0, 3>	wwxw;		PC<3, 3, 1, 0>	wwyx;		PC<3, 3, 1, 1>	wwyy;
		PC<3, 3, 1, 2>	wwyz;		PC<3, 3, 1, 3>	wwyw;		PC<3, 3, 2, 0>	wwzx;		PC<3, 3, 2, 1>	wwzy;
		PC<3, 3, 2, 2>	wwzz;		PC<3, 3, 2, 3>	wwzw;		PC<3, 3, 3, 0>	wwwx;		PC<3, 3, 3, 1>	wwwy;
		PC<3, 3, 3, 2>	wwwz;		PC<3, 3, 3, 3>	wwww;

		//colour components
	#if 0
		P<0> r;
		struct { swizzle_padding<P<1>> _g; swizzle_type<P<1>> g; };
		struct { swizzle_padding<P<2>> _b; swizzle_type<P<2>> b; };
		struct { swizzle_padding<P<3>> _a; swizzle_type<P<3>> a; };
	#else
		P<0>			r;			P<1>			g;			P<2>			b;			P<3>			a;
	#endif

		P<0, 1, 2>		rgb;
	};
	swizzles() {}
	constexpr swizzles(V v)	: v(v) {}
	constexpr swizzles(const swizzles &b) : v(b.v) {}
	constexpr	operator V() const	{ return v; }
	template<int...I> friend force_inline auto& swizzle(const swizzles &p)	{ return reinterpret_cast<const P<I...>&>(p); }
	template<int...I> friend force_inline auto& swizzle(swizzles &p)		{ return reinterpret_cast<P<I...>&>(p); }
};

} // namespace iso

#endif
