#ifndef VECTOR_ISO_H
#define VECTOR_ISO_H

#include "base/vector.h"
#include "base/colour.h"
#include "iso/iso.h"

namespace ISO {

#ifdef SOFT_FLOAT_H
template<uint32 M, uint32 E, bool S> struct def<soft_float_imp<M,E,S> > : ISO::TypeFloat {
	def() : ISO::TypeFloat(M + E + int(S), E, S ? SIGN : NONE)	{}
};

template<typename I, uint64 ONE, int MODE> struct ISO_def_scaled;
template<typename I, uint64 ONE> struct ISO_def_scaled<I,ONE,1> : ISO::TypeInt {
	constexpr ISO_def_scaled() : ISO::TypeInt(sizeof(I) * 8, klog2<ONE>, (num_traits<I>::is_signed ? SIGN : NONE))	{}
};
template<typename I, uint64 ONE> struct ISO_def_scaled<I,ONE,2> : ISO::TypeInt {
	constexpr ISO_def_scaled() : ISO::TypeInt(sizeof(I) * 8, klog2<ONE + 1>, (num_traits<I>::is_signed ? SIGN : NONE) | FRAC_ADJUST)	{}
};
template<typename I, uint64 ONE> struct def<scaled<I,ONE> > : ISO_def_scaled<I, ONE, ((ONE & (ONE - 1))==0 ? 1 : (ONE & (uint64(ONE) + 1))==0 ? 2 : 0)> {};
template<int whole, int frac> struct def<fixed<whole,frac> > : def<typename fixed<whole,frac>::base> {};
template<int whole, int frac> struct def<ufixed<whole,frac> > : def<typename ufixed<whole,frac>::base> {};

template<typename T> struct arg_type;
template<typename T, typename U> struct arg_type<T(U)> { typedef U type; };
#define ARG_TYPE(X) arg_type<void(X)>::type
#endif

#ifdef PACKED_TYPES_H
ISO_DEFUSER(compressed_quaternion,	uint32);
ISO_DEFUSER(compressed_normal3,		uint32);
#endif

ISO_DEFUSER(float2p,	float[2]);
ISO_DEFUSER(float3p,	float[3]);
ISO_DEFUSER(float4p,	float[4]);
ISO_DEFUSERX(float3x4p,	float3p[4], "Matrix");
ISO_DEFUSERX(float3x3p,	float3p[3], "Matrix3");
ISO_DEFUSERX(float4x4p,	float4p[4], "Matrix4");

template<typename T, int N> _ISO_DEFSAME(simple_vec<T COMMA N>, T[N]);

template<typename X> _ISO_DEFCOMP(array_vec<X COMMA 2>, 2, NONE) { ISO_SETFIELDS(0, x, y); }};
template<typename X> _ISO_DEFCOMP(array_vec<X COMMA 3>, 3, NONE) { ISO_SETFIELDS(0, x, y, z); }};
template<typename X> _ISO_DEFCOMP(array_vec<X COMMA 4>, 4, NONE) { ISO_SETFIELDS(0, x, y, z, w); }};

template<typename X, typename Y> _ISO_DEFCOMP(field_vec<X COMMA Y>, 2, NONE) { ISO_SETFIELDS(0, x, y); }};
template<typename X, typename Y, typename Z> _ISO_DEFCOMP(field_vec<X COMMA Y COMMA Z>, 3, NONE) { ISO_SETFIELDS(0, x, y, z); }};
template<typename X, typename Y, typename Z, typename W> _ISO_DEFCOMP(field_vec<X COMMA Y COMMA Z COMMA W>, 4, NONE) { ISO_SETFIELDS(0, x, y, z, w); }};

template<typename X, typename Y> struct def<bitfield_vec<X, Y> > : TISO_bitpacked<2> {
	def() { Add<X>("x", BIT_COUNT<X>); Add<Y>("y", BIT_COUNT<Y>); }
};
template<typename X, typename Y, typename Z> struct def<bitfield_vec<X, Y, Z> > : TISO_bitpacked<3> {
	def() { Add<X>("x", BIT_COUNT<X>); Add<Y>("y", BIT_COUNT<Y>); Add<Z>("z", BIT_COUNT<Z>);  }
};
template<typename X, typename Y, typename Z, typename W> struct def<bitfield_vec<X, Y, Z, W> > : TISO_bitpacked<4> {
	def() { Add<X>("x", BIT_COUNT<X>); Add<Y>("y", BIT_COUNT<Y>); Add<Z>("z", BIT_COUNT<Z>); Add<W>("w", BIT_COUNT<W>);  }
};

template<typename V> struct def<V, enable_if_t<(is_simd<V> && num_elements_v<V> == 2)>> : TypeCompositeN<2> {
	typedef element_type<V>	E;
	def() {
		fields[0].set(tag("x"), ISO::getdef<E>(), sizeof(E) * 0, sizeof(E));
		fields[1].set(tag("y"), ISO::getdef<E>(), sizeof(E) * 1, sizeof(E));
		SetLog2Align(log2alignment<vec<E, 2>>);
	}
};
template<typename V> struct def<V, enable_if_t<(is_simd<V> && num_elements_v<V> == 3)>> : TypeCompositeN<3> {
	typedef element_type<V>	E;
	def() {
		fields[0].set(tag("x"), ISO::getdef<E>(), sizeof(E) * 0, sizeof(E));
		fields[1].set(tag("y"), ISO::getdef<E>(), sizeof(E) * 1, sizeof(E));
		fields[2].set(tag("z"), ISO::getdef<E>(), sizeof(E) * 2, sizeof(E));
		SetLog2Align(log2alignment<vec<E, 3>>);
	}
};

template<typename V> struct def<V, enable_if_t<(is_simd<V> && num_elements_v<V> == 4)>> : TypeCompositeN<4> {
	typedef element_type<V>	E;
	def() {
		fields[0].set(tag("x"), ISO::getdef<E>(), sizeof(E) * 0, sizeof(E));
		fields[1].set(tag("y"), ISO::getdef<E>(), sizeof(E) * 1, sizeof(E));
		fields[2].set(tag("z"), ISO::getdef<E>(), sizeof(E) * 2, sizeof(E));
		fields[3].set(tag("w"), ISO::getdef<E>(), sizeof(E) * 3, sizeof(E));
		SetLog2Align(log2alignment<vec<E, 4>>);
	}
};

template<typename E, int N> struct def<pos<E, N> > : def<vec<E, N> > {};

template<typename E, int N> struct def<mat<E, N, 2> > : TypeCompositeN<2> {
	typedef mat<E, N, 2>	T;
	def() {
		fields[0].set(tag("x"), &T::x);
		fields[1].set(tag("y"), &T::y);
		SetLog2Align(log2alignment<T>);
	}
};

template<typename E, int N> struct def<mat<E, N, 3> > : TypeCompositeN<3> {
	typedef mat<E, N, 3>	T;
	def() {
		fields[0].set(tag("x"), &T::x);
		fields[1].set(tag("y"), &T::y);
		fields[2].set(tag("z"), &T::z);
		SetLog2Align(log2alignment<T>);
	}
};
template<typename E, int N> struct def<mat<E, N, 4> > : TypeCompositeN<4> {
	typedef mat<E, N, 4>	T;
	def() {
		fields[0].set(tag("x"), &T::x);
		fields[1].set(tag("y"), &T::y);
		fields[2].set(tag("z"), &T::z);
		fields[3].set(tag("w"), &T::w);
		SetLog2Align(log2alignment<T>);
	}
};
template<typename E> struct def<mat<E, 2, 2> > : def<E[2][2]> {};
template<typename E> struct def<mat<E, 2, 3> > : def<E[3][2]> {};

ISO_DEFSAME(quaternion, float4);
ISO_DEFSAME(colour, float4);

}// namespace ISO
#endif	// VECTOR_ISO_H
