#ifndef CONSTANTS_H
#define CONSTANTS_H

#include "defs.h"

namespace iso {

template<typename K> auto	operator==(const constant<K>, const constant<K>)	{ return true; }
template<typename K> auto	operator!=(const constant<K>, const constant<K>)	{ return false; }
template<typename K> auto	operator< (const constant<K>, const constant<K>)	{ return false; }
template<typename K> auto	operator<=(const constant<K>, const constant<K>)	{ return true; }
template<typename K> auto	operator> (const constant<K>, const constant<K>)	{ return false; }
template<typename K> auto	operator>=(const constant<K>, const constant<K>)	{ return true; }


template<typename K, typename A> auto	operator==(A a, const constant<K> &b)->enable_if_t<use_constants<A>, decltype(a == b.template co<A>())>	{ return a == b.template co<A>(); }
template<typename K, typename A> auto	operator!=(A a, const constant<K> &b)->enable_if_t<use_constants<A>, decltype(a != b.template co<A>())>	{ return a != b.template co<A>(); }
template<typename K, typename A> auto	operator< (A a, const constant<K> &b)->enable_if_t<use_constants<A>, decltype(a <  b.template co<A>())>	{ return a <  b.template co<A>(); }
template<typename K, typename A> auto	operator<=(A a, const constant<K> &b)->enable_if_t<use_constants<A>, decltype(a <= b.template co<A>())>	{ return a <= b.template co<A>(); }
template<typename K, typename A> auto	operator> (A a, const constant<K> &b)->enable_if_t<use_constants<A>, decltype(a >  b.template co<A>())>	{ return a >  b.template co<A>(); }
template<typename K, typename A> auto	operator>=(A a, const constant<K> &b)->enable_if_t<use_constants<A>, decltype(a >= b.template co<A>())>	{ return a >= b.template co<A>(); }

template<typename K, typename B> auto	operator==(const constant<K> &a, B b)->enable_if_t<use_constants<B>, decltype(a.template co<B>() == b)>	{ return a.template co<B>() == b; }
template<typename K, typename B> auto	operator!=(const constant<K> &a, B b)->enable_if_t<use_constants<B>, decltype(a.template co<B>() != b)>	{ return a.template co<B>() != b; }
template<typename K, typename B> auto	operator< (const constant<K> &a, B b)->enable_if_t<use_constants<B>, decltype(a.template co<B>() <  b)>	{ return a.template co<B>() <  b; }
template<typename K, typename B> auto	operator<=(const constant<K> &a, B b)->enable_if_t<use_constants<B>, decltype(a.template co<B>() <= b)>	{ return a.template co<B>() <= b; }
template<typename K, typename B> auto	operator> (const constant<K> &a, B b)->enable_if_t<use_constants<B>, decltype(a.template co<B>() >  b)>	{ return a.template co<B>() >  b; }
template<typename K, typename B> auto	operator>=(const constant<K> &a, B b)->enable_if_t<use_constants<B>, decltype(a.template co<B>() >= b)>	{ return a.template co<B>() >= b; }

template<typename K, typename A> enable_if_t<use_constants<A>, A&>		operator+=(A &a, const constant<K> &b)	{ return a += b.template co<A>(); }
template<typename K, typename A> enable_if_t<use_constants<A>, A&>		operator-=(A &a, const constant<K> &b)	{ return a -= b.template co<A>(); }
template<typename K, typename A> enable_if_t<use_constants<A>, A&>		operator*=(A &a, const constant<K> &b)	{ return a *= b.template co<A>(); }
template<typename K, typename A> enable_if_t<use_constants<A>, A&>		operator/=(A &a, const constant<K> &b)	{ return a /= b.template co<A>(); }

template<typename K, typename B> constexpr auto	operator+(const constant<K> &a, const B &b)->enable_if_t<use_constants<B>, decltype(a.template co<B>() + b)>	{ return a.template co<B>() + b;	}
template<typename K, typename B> constexpr auto	operator-(const constant<K> &a, const B &b)->enable_if_t<use_constants<B>, decltype(a.template co<B>() - b)>	{ return a.template co<B>() - b;	}
template<typename K, typename B> constexpr auto	operator*(const constant<K> &a, const B &b)->enable_if_t<use_constants<B>, decltype(a.template co<B>() * b)>	{ return a.template co<B>() * b;	}
template<typename K, typename B> constexpr auto	operator/(const constant<K> &a, const B &b)->enable_if_t<use_constants<B>, decltype(a.template co<B>() / b)>	{ return a.template co<B>() / b;	}

template<typename A, typename K> constexpr auto	operator+(const A &a, const constant<K> &b)->enable_if_t<use_constants<A>, decltype(a + b.template co<A>())>	{ return a + b.template co<A>();	}
template<typename A, typename K> constexpr auto	operator-(const A &a, const constant<K> &b)->enable_if_t<use_constants<A>, decltype(a - b.template co<A>())>	{ return a - b.template co<A>();	}
template<typename A, typename K> constexpr auto	operator*(const A &a, const constant<K> &b)->enable_if_t<use_constants<A>, decltype(a * b.template co<A>())>	{ return a * b.template co<A>();	}
template<typename A, typename K> constexpr auto	operator/(const A &a, const constant<K> &b)->enable_if_t<use_constants<A>, decltype(a / b.template co<A>())>	{ return a / b.template co<A>();	}

template<typename K>				struct __neg	{ template<typename R> static constexpr R as() { return -K::template as<R>(); } };
template<typename X>				struct __abs	{ template<typename R> static constexpr R as() { return abs(X::template as<R>()); } };
template<typename X, typename Y>	struct __add	{ template<typename R> static constexpr R as() { return X::template as<R>() + Y::template as<R>(); } };
template<typename X, typename Y>	struct __sub	{ template<typename R> static constexpr R as() { return X::template as<R>() - Y::template as<R>(); } };
template<typename X, typename Y>	struct __mul	{ template<typename R> static constexpr R as() { return X::template as<R>() * Y::template as<R>(); } };
template<typename X, typename Y>	struct __div	{ template<typename R> static constexpr R as() { return X::template as<R>() / Y::template as<R>(); } };
//template<typename N>				struct __sqrt;
//template<typename N, typename B>	struct __log;

template<typename N>				struct __sqrt	{ template<typename R> static constexpr R as(); };
template<typename N, typename B>	struct __log	{ template<typename R> static R as(); };

template<typename K> struct constant_type<__abs<K>> : constant_type<K> {};
template<typename K> struct constant_type<__neg<K>> : constant_type<K> {};

struct _maximum		{ template<typename T> static constexpr T as()	{ return num_traits<T>::max(); } };
struct _minimum		{ template<typename T> static constexpr T as()	{ return num_traits<T>::min(); } };
struct _epsilon		{ template<typename F> static constexpr F as()	{ return force_cast<F>(float_components<F>::eps()); } };
struct _infinity	{ template<typename F> static constexpr F as()	{ return force_cast<F>(float_components<F>::inf()); } };
struct _nan 		{ template<typename F> static constexpr F as()	{ return force_cast<F>(float_components<F>::nan()); } };

struct __pi			{ template<typename R> static constexpr R as()	{ return R(3.14159265358979326846); } };
struct __e			{ template<typename R> static constexpr R as()	{ return R(2.71828182845904523536); } };
struct __sign		{ template<typename R> static constexpr R as()	{ return force_cast<R>((uint64(1) << (sizeof(R) * 8 - 1))); } };

typedef constant<__e>				_e;

extern constant<_infinity>			infinity;
extern constant_int<2>				two;
extern constant_int<3>				three;
extern constant_int<4>				four;
extern constant<__pi>				pi;
extern constant<__sign>				sign_mask;
extern constant<_maximum> 			maximum;
extern constant<_minimum> 			minimum;
extern constant<_epsilon>			epsilon;
extern constant<_nan>				nan;

//abs
template<typename X> 				constexpr auto	abs(const constant<X>&)					{ return constant<__abs<X>>(); }
template<int X> 					constexpr auto	abs(const constant_int<X> &x)			{ return x; }
template<typename X> 				constexpr auto	abs(const constant<__neg<X>> &x)		{ return abs(-x); }

//plus
template<typename X>				constexpr auto	operator+(const constant<X> &x) 		{ return x; }

//neg
template<typename X> 				constexpr auto	operator-(const constant<X>&) 			{ return constant<__neg<X>>(); }
template<typename X> 				constexpr auto	operator-(const constant<__neg<X>>&) 	{ return constant<X>(); }
template<typename X, typename Y> 	constexpr auto	operator-(const constant<__sub<X, Y>>&) { return constant<__sub<Y,X>>(); }

//add
template<typename X, typename Y> 	constexpr auto	operator+(const constant<X>&, const constant<Y>&)					{ return constant<__add<X,Y>>(); }
template<typename X, typename Y> 	constexpr auto	operator+(const constant<X> &x, const constant<__neg<Y>> &y)		{ return x - -y; }
template<typename X, typename Y> 	constexpr auto	operator+(const constant<__neg<X>> &x, const constant<Y> &y)		{ return y - -x; }
template<typename X, typename Y> 	constexpr auto	operator+(const constant<__neg<X>> &x, const constant<__neg<Y>> &y)	{ return -(-x + -y); }
template<int X, int Y> 				constexpr auto	operator+(const constant_int<X> &x, const constant_int<Y> &y)		{ return constant_int<X + Y>(); }
template<typename X>				constexpr auto	operator+(const constant<X> &x, decltype(zero))						{ return x; }
template<typename Y>				constexpr auto	operator+(decltype(zero), const constant<Y> &y)						{ return y; }
template<int X>						constexpr auto	operator+(const constant_int<X> &x, decltype(zero))					{ return x; }
template<int Y>						constexpr auto	operator+(decltype(zero), const constant_int<Y> &y)					{ return y; }
constexpr auto 	operator+(decltype(zero), decltype(zero))					{ return zero; }

//sub
template<typename X, typename Y> 	constexpr auto	operator-(const constant<X>&, const constant<Y>&)					{ return constant<__sub<X,Y>>(); }
template<typename X> 				constexpr auto	operator-(const constant<X>&, const constant<X>&)					{ return zero; }
template<typename X, typename Y> 	constexpr auto	operator-(const constant<X> &x, const constant<__neg<Y>> &y)		{ return x + -y; }
template<typename X, typename Y> 	constexpr auto	operator-(const constant<__neg<X>> &x, const constant<Y> &y)		{ return -(-x + y); }
template<typename X, typename Y> 	constexpr auto	operator-(const constant<__neg<X>> &x, const constant<__neg<Y>> &y)	{ return -y - x; }
template<int Y>						constexpr auto	operator-(const _zero&, const constant_int<Y> &y)					{ return -y; }

template<int X>			constexpr constant_int<X>									operator-(const constant_int<X>&, const _zero&)	{ return {}; }
template<int X, int Y>	constexpr enable_if_t<(X > Y), constant_int<X-Y>>			operator-(const constant_int<X>&, const constant_int<Y>&)	{ return {}; }
template<int X, int Y>	constexpr enable_if_t<(X < Y), constant<__neg<__int<Y-X>>>>	operator-(const constant_int<X>&, const constant_int<Y>&)	{ return {}; }

//mul
template<typename X, typename Y>	constexpr auto 	mul0(const constant<X>&, const constant<Y>&)						{ return constant<__mul<X, Y>>(); }
template<typename X, typename Y>	constexpr auto 	mul0(const constant<X>&, const constant<__div<__int<1>, Y>>&)		{ return constant<__div<X, Y>>(); }

template<typename X, typename Y>	constexpr auto 	mul2(const constant<X>&, const constant<Y>&)						{ return constant<__mul<X, Y>>(); }
//template<typename X> 				constexpr auto 	mul2(const constant<X> &x, decltype(one))							{ return x; }
template<typename X> 				constexpr auto 	mul2(const constant<X> &x, decltype(pi))							{ return constant<__mul<__pi, X>>(); }
template<typename X, typename Y> 	constexpr auto 	mul2(const constant<X> &x, const constant<__neg<Y>> &y)				{ return -(x * -y); }
template<typename X, typename YN, typename YD>	constexpr auto 	mul2(const constant<X> &x, const constant<__div<YN,YD>> &y)			{ return (x * constant<YN>()) / constant<YD>(); }

template<typename Y> 				constexpr auto 	operator*(decltype(one), const constant<Y> &y)						{ return y; }
template<typename X> 				constexpr auto 	operator*(const constant<X> &x, decltype(one))						{ return x; }
template<typename Y> 				constexpr auto 	operator*(decltype(zero), const constant<Y> &y)						{ return zero; }
template<typename X> 				constexpr auto 	operator*(const constant<X> &x, decltype(zero))						{ return zero; }
					 				constexpr auto 	operator*(decltype(one), decltype(one))								{ return one; }

template<int Y> 					constexpr auto 	operator*(const decltype(zero)&, const constant_int<Y>&)			{ return zero; }
template<int X> 					constexpr auto 	operator*(const constant_int<X>&, const decltype(zero)&)			{ return zero; }
template<int Y> 					constexpr auto 	operator*(decltype(one), const constant_int<Y>& y)					{ return y; }
template<int X> 					constexpr auto 	operator*(const constant_int<X>& x, decltype(one))					{ return x; }
constexpr auto 	operator*(decltype(zero), decltype(one))					{ return zero; }
constexpr auto 	operator*(decltype(one), decltype(zero))					{ return zero; }
constexpr auto 	operator*(decltype(zero), decltype(zero))					{ return zero; }

template<typename X, typename Y> 	constexpr auto 	operator*(const constant<X> &x, const constant<Y> &y)				{ return mul2(x, y); }
template<int X, int Y> 				constexpr auto 	operator*(const constant_int<X>&, const constant_int<Y>&)			{ return constant_int<X * Y>(); }
//template<typename X> 				constexpr auto 	operator*(const constant<__sqrt<X>>&, const constant<__sqrt<X>>&)	{ return constant<X>(); }
template<typename X, typename Y> 	constexpr auto 	operator*(const constant<__neg<X>> &x, const constant<Y> &y)		{ return -mul2(-x, y); }
template<typename X> 				constexpr auto 	operator*(const constant<__neg<X>> &x, decltype(one))				{ return x; }
template<typename XN, typename XD, typename Y>	constexpr auto 	operator*(const constant<__div<XN,XD>> &x, const constant<Y> &y)	{ return (constant<XN>() * y) / constant<XD>(); }
template<typename XN, typename XD>	constexpr auto 	operator*(const constant<__div<XN,XD>> &x, decltype(one))			{ return x; }

//div
template<typename X, typename Y> 	constexpr auto 	operator/(const constant<X>&, const constant<Y>&)					{ return constant<__div<X, Y>>(); }
template<typename X> 				constexpr auto 	operator/(const constant<X>&, const constant<X>&)					{ return one; }
									constexpr auto 	operator/(const _one&, const _one&)									{ return one; }
template<typename XN, typename XD, typename Y>	constexpr auto 	operator/(const constant<__div<XN,XD>>, const constant<Y>&)	{ return constant<XN>() / (constant<XD>() * constant<Y>()); }
template<typename X, typename YN, typename YD>	constexpr auto 	operator/(const constant<X>, const constant<__div<YN,YD>>&)	{ return constant<X>() * constant<YD>() / constant<YN>(); }
template<typename XN, typename XD, typename YN, typename YD>	constexpr auto 	operator/(const constant<__div<XN,XD>>, const constant<__div<YN,YD>>&)	{ return (constant<XN>() * constant<YD>()) / (constant<XD>() * constant<YN>()); }

template<typename X1, typename X2, typename Y> 	constexpr auto 	operator/(const constant<__mul<X1, X2>>&, const constant<Y> &y)	{ return mul0(constant<X1>(), (constant<X2>() / y)); }
template<int X, int Y> 				constexpr auto 	operator/(const constant_int<X>&, const constant_int<Y>&)		{ return constant<__div<__int<X / constexpr_gcd(X, Y)>, __int<Y / constexpr_gcd(X, Y)>>>(); }

template<typename X, typename Y> 	constexpr auto 	operator/(const constant<__neg<X>> &x, const constant<Y> &y)		{ return -(-x / y); }
template<typename X, typename Y> 	constexpr auto 	operator/(const constant<X> &x, const constant<__neg<Y>> &y)		{ return -(x / -y); }
template<typename X, typename Y> 	constexpr auto 	operator/(const constant<__neg<X>> &x, const constant<__neg<Y>> &y)	{ return -x / -y; }

template<typename A, typename B1, typename B2> constexpr auto	operator+(const constant<__mul<A,B1>>&, const constant<__mul<A,B2>>&)	{
	return constant<A>() * (constant<B1>() + constant<B2>());
}

template<typename N1, typename D1, typename N2, typename D2> constexpr auto	operator+(const constant<__div<N1,D1>>&, const constant<__div<N2,D2>>&)	{
	return (constant<N1>() * constant<D2>() + constant<N2>() *constant<D1>()) / (constant<D1>() * constant<D2>());
}

template<typename N, typename M, typename D> constexpr auto	operator+(const constant<__mul<N,M>>&, const constant<__div<N,D>>&)	{
	return constant<N>() * (constant<M>() + one / constant<D>());
}

//sqrt
template<typename X> constexpr auto	sqrt(const constant<X>&)						{ return constant<__sqrt<X> >(); }
template<typename X> constexpr auto	sqrt(const constant<__mul<X,X>>&)				{ return constant<X>(); }
constexpr auto	sqrt(const decltype(one)&)				{ return one; }
template<typename X, typename Y> constexpr auto	sqrt(const constant<__div<X, Y>>&)	{ return sqrt(constant<X>()) / sqrt(constant<Y>()); }//constant<__div<__sqrt<X>, __sqrt<Y> > >(); }
template<typename X> constexpr auto	rsqrt(const constant<X> &x)						{ return one / sqrt(x); }
template<typename X, typename Y>	constexpr auto 	operator*(const constant<__sqrt<X>>&, const constant<__sqrt<Y>>&)	{ return sqrt(constant<X>() * constant<Y>()); }

//log
template<typename B, typename X> 	constexpr auto	log_const(const constant<X>&)	{ return constant<__log<X, B>>(); }
template<typename X, typename Y, typename B> constexpr auto operator+(const constant<__log<X,B> >&, const constant<__log<Y,B> >&)	{ return log_const<B>(constant<X>() * constant<Y>()); }
template<typename X, typename Y, typename B> constexpr auto operator-(const constant<__log<X,B> >&, const constant<__log<Y,B> >&)	{ return log_const<B>(constant<X>() / constant<Y>()); }

template<typename X> constexpr auto	log(const constant<X>&)		{ return constant<__log<X, __e>>(); }
template<typename X> constexpr auto	log(const _e&)				{ return one; }
template<typename X> constexpr auto	log2(const constant<X>&)	{ return constant<__log<X, __int<2>>>(); }
template<typename X> constexpr auto	log2(decltype(two))			{ return one; }

template<typename X> constexpr auto	exp(const constant<__log<X, __e> >&)		{ return constant<X>(); }
template<typename X> constexpr auto	exp(decltype(one))							{ return _e(); }
template<typename X> constexpr auto	exp2(const constant<__log<X, __int<2> > >&)	{ return constant<X>(); }
template<typename X> constexpr auto	exp2(decltype(one))							{ return two; }

template<typename X, typename Y, Y...y> constexpr auto	operator+(const constant<X> &x, const meta::value_list<Y, y...>&)	{ return type_list<decltype(x + constant_int<y>())...>(); }
template<typename X, typename Y, Y...y> constexpr auto	operator-(const constant<X> &x, const meta::value_list<Y, y...>&)	{ return type_list<decltype(x - constant_int<y>())...>(); }
template<typename X, typename Y, Y...y> constexpr auto	operator*(const constant<X> &x, const meta::value_list<Y, y...>&)	{ return type_list<decltype(x * constant_int<y>())...>(); }
template<typename X, typename Y, Y...y> constexpr auto	operator/(const constant<X> &x, const meta::value_list<Y, y...>&)	{ return type_list<decltype(x / constant_int<y>())...>(); }
template<typename X, typename Y, Y...y> constexpr auto	operator%(const constant<X> &x, const meta::value_list<Y, y...>&)	{ return type_list<decltype(x % constant_int<y>())...>(); }
template<typename X, X...x>				constexpr auto	operator*(const meta::value_list<X, x...>&, const _zero&)			{ return meta::value_list<X, (x, X(0))...>(); }

template<typename X, typename Y, X...x> constexpr auto	operator+(const meta::value_list<X, x...>&, const constant<Y> &y)	{ return type_list<decltype(constant_int<x>() + y)...>(); }
template<typename X, typename Y, X...x> constexpr auto	operator-(const meta::value_list<X, x...>&, const constant<Y> &y)	{ return type_list<decltype(constant_int<x>() - y)...>(); }
template<typename X, typename Y, X...x> constexpr auto	operator*(const meta::value_list<X, x...>&, const constant<Y> &y)	{ return type_list<decltype(constant_int<x>() * y)...>(); }
template<typename X, typename Y, X...x> constexpr auto	operator/(const meta::value_list<X, x...>&, const constant<Y> &y)	{ return type_list<decltype(constant_int<x>() / y)...>(); }
template<typename X, typename Y, X...x> constexpr auto	operator%(const meta::value_list<X, x...>&, const constant<Y> &y)	{ return type_list<decltype(constant_int<x>() % y)...>(); }

template<typename X, typename...Y> 		constexpr auto	operator+(const constant<X> &x, const type_list<Y...>&)				{ return type_list<decltype(x + Y())...>(); }
template<typename X, typename...Y> 		constexpr auto	operator-(const constant<X> &x, const type_list<Y...>&)				{ return type_list<decltype(x - Y())...>(); }
template<typename X, typename...Y> 		constexpr auto	operator*(const constant<X> &x, const type_list<Y...>&)				{ return type_list<decltype(x * Y())...>(); }
template<typename X, typename...Y> 		constexpr auto	operator/(const constant<X> &x, const type_list<Y...>&)				{ return type_list<decltype(x / Y())...>(); }
template<typename X, typename...Y> 		constexpr auto	operator%(const constant<X> &x, const type_list<Y...>&)				{ return type_list<decltype(x % Y())...>(); }
			
template<typename...X, typename Y> 		constexpr auto	operator+(const type_list<X...>&, const constant<Y> &y)				{ return type_list<decltype(X() + y)...>(); }
template<typename...X, typename Y> 		constexpr auto	operator-(const type_list<X...>&, const constant<Y> &y)				{ return type_list<decltype(X() - y)...>(); }
template<typename...X, typename Y> 		constexpr auto	operator*(const type_list<X...>&, const constant<Y> &y)				{ return type_list<decltype(X() * y)...>(); }
template<typename...X, typename Y> 		constexpr auto	operator/(const type_list<X...>&, const constant<Y> &y)				{ return type_list<decltype(X() / y)...>(); }
template<typename...X, typename Y> 		constexpr auto	operator%(const type_list<X...>&, const constant<Y> &y)				{ return type_list<decltype(X() % y)...>(); }

template<typename...X, typename...Y> 	constexpr auto	operator+(const type_list<X...>&, const type_list<Y...>&)			{ return type_list<decltype(X() + Y())...>(); }
template<typename...X, typename...Y> 	constexpr auto	operator-(const type_list<X...>&, const type_list<Y...>&)			{ return type_list<decltype(X() - Y())...>(); }
template<typename...X, typename...Y> 	constexpr auto	operator*(const type_list<X...>&, const type_list<Y...>&)			{ return type_list<decltype(X() * Y())...>(); }
template<typename...X, typename...Y> 	constexpr auto	operator/(const type_list<X...>&, const type_list<Y...>&)			{ return type_list<decltype(X() / Y())...>(); }
template<typename...X, typename...Y> 	constexpr auto	operator%(const type_list<X...>&, const type_list<Y...>&)			{ return type_list<decltype(X() % Y())...>(); }


// axioms:

// multiplies/divides by +-1
template<typename A> constexpr auto 	operator*(const A &a, decltype(one))		{ return  a; }
template<typename A> constexpr auto 	operator*(decltype(one), const A &a)		{ return  a; }
template<typename A> constexpr auto 	operator*(const A &a, decltype(-one))		{ return -a; }
template<typename A> constexpr auto 	operator*(decltype(-one), const A &a)		{ return -a; }
template<typename A> constexpr auto		operator/(const A &a, decltype(one))		{ return  a; }
template<typename A> constexpr auto		operator/(const A &a, decltype(-one))		{ return -a; }

// multiplies by 0
template<typename A> constexpr auto		operator*(const _zero&, const A &a)			{ return zero; }
template<typename A> constexpr auto		operator*(const A&, const _zero &a)			{ return zero; }
constexpr auto							operator*(const _zero&, const _zero&)		{ return zero; }

// adds/subs of 0
template<typename A> constexpr const A&	operator+(const A &a, const _zero&)			{ return  a; }
template<typename A> constexpr const A&	operator+(const _zero&, const A &a)			{ return  a; }
template<typename A> constexpr const A&	operator-(const A &a, const _zero&)			{ return  a; }
template<typename A> constexpr const A	operator-(const _zero&, const A &a)			{ return -a; }

template<typename D>				inline void cast_assign(D &d, decltype(maximum) s)	{ d = s; }
template<typename D>				inline void cast_assign(D &d, decltype(-maximum) s)	{ d = s; }

extern decltype(one / two)			half;
extern decltype(one / three)		third;
extern decltype(one / four)			quarter;
extern decltype(sqrt(two))			sqrt2;
extern decltype(sqrt(three))		sqrt3;
extern decltype(sqrt(half))			rsqrt2;

} // namespace iso

#endif // CONSTANTS_H
