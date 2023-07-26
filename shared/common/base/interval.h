#ifndef INTERVAL_H
#define INTERVAL_H

#include "defs.h"
#include "constants.h"

namespace iso {

template<typename A, typename B, typename C, typename... TT> constexpr decltype(auto)	min(A a, B b, C c, TT... tt)				{ return min(min(a, b), c, tt...); }
template<typename A, typename B, typename C, typename... TT> constexpr decltype(auto)	max(A a, B b, C c, TT... tt)				{ return max(max(a, b), c, tt...); }
//template<typename T, typename A, typename B>	constexpr auto	clamp(T &&v, A &&a, B &&b)	{ return min(max(forward<T>(v), forward<A>(a)), forward<B>(b)); }
template<typename T, typename A, typename B>	constexpr auto	clamp(const T &v, const A &a, const B &b)	{ return min(max(v, a), b); }

template<typename T> using diff_t = decltype(declval<T>() - declval<T>());

//-----------------------------------------------------------------------------
//	interval
//-----------------------------------------------------------------------------

template<typename T> struct interval {
	using diff_t = iso::diff_t<T>;
	T	a, b;

	static constexpr interval with_length(const T &a, const diff_t &x) { return {a, a + x}; }
	static constexpr interval with_centre(const T &a, const diff_t &x) { return {a - x, a + x}; }

	constexpr interval() {}
	constexpr interval(const _none&)			: a(iso::maximum), b(iso::minimum)	{}
	constexpr interval(const T &a, const T &b)	: a(a), b(b)		{}
	template<typename T2> constexpr interval(const interval<T2> &i)	: a(i.a), b(i.b) {}
	explicit interval(const T &a)				: a(a), b(a)		{}

	constexpr T			clamp(const T &t)		const	{ return iso::clamp(t, a, b); }

//	constexpr bool		overlaps(const interval &i)	const	{ return !(i.b < a) && !(b < i.a); }
	constexpr bool		contains(const interval &i)	const	{ return all(i.a >= a) && all(i.b <= b); }
	constexpr bool		contains(const T &t)		const	{ return all(t >= a) && all(t <= b); }
	constexpr bool		empty()						const	{ return any(a >= b); }
	constexpr T			minimum()					const	{ return min(a, b); }
	constexpr T			maximum()					const	{ return max(a, b); }
//	constexpr diff_t	length()					const	{ return b - a; }
	constexpr diff_t	extent()					const	{ return b - a; }
	constexpr diff_t	half_extent()				const	{ return extent() / 2; }
	constexpr const T&	begin()						const	{ return a; }
	constexpr const T&	end()						const	{ return b; }
	constexpr T			centre()					const	{ return a + (b - a) / 2; }
	constexpr interval	expand(const diff_t &t)		const	{ return {a - t, b + t}; }

	auto&				set_begin(const T &t)			{ a = t; return *this; }
	auto&				set_end(const T &t)				{ b = t; return *this; }

	template<typename A> constexpr interval	sub_interval(const A &aoff)		const { return {(aoff < 0 ? b : a) + aoff, b}; }
	template<typename B> constexpr interval	sub_interval_to(const B &boff)	const {	return {a, (boff <= 0 ? b : a) + boff}; }
	template<typename A, typename B> constexpr interval	sub_interval(const A &aoff, const B &boff) const {
		return sub_interval(aoff).sub_interval_to(boff);
	}

	template<typename F> T	from(F f)	const	{ return a + f * (b - a); }
	template<typename F> T	to(F f)		const	{ return (f - a) / (b - a); }
	template<typename F> constexpr interval operator*(const interval<F> &i) const	{ return {from(i.a), from(i.b)}; }

	constexpr bool				operator<(const T &t)				const { return b <= t; }
	constexpr bool				operator<(const interval &i)		const { return a < i.a || (!(i.a < a) && i.b < b); }// enclosing intervals considered less than enclosees

	constexpr interval			operator|(const interval &i)		const { return {min(a, i.a), max(b, i.b)}; }
	constexpr interval			operator&(const interval &i)		const { return {max(a, i.a), min(b, i.b)}; }
	constexpr interval			operator+(const diff_t &t)			const { return {a + t, b + t}; }
	constexpr interval			operator-(const diff_t &t)			const { return {a - t, b - t}; }
	template<typename X> constexpr interval operator*(const X &x)	const { return {a * x, b * x}; }
	template<typename X> constexpr interval operator/(const X &x)	const { return {a / x, b / x}; }

	interval&					operator|=(const interval &i)	{ a = min(a, i.a); 	b = max(b, i.b); return *this; }
	interval&					operator&=(const interval &i)	{ a = max(a, i.a); 	b = min(b, i.b); return *this; }
	interval&					operator|=(const T &t)			{ a = min(a, t); 	b = max(b, t); return *this; }
	interval&					operator+=(const diff_t &t)		{ a += t; b += t; return *this; }
	interval&					operator-=(const diff_t &t)		{ a -= t; b -= t; return *this; }
	template<typename X> interval&	operator*=(const X &x)		{ a *= x; b *= x; return *this; }
	template<typename X> interval&	operator/=(const X &x)		{ a /= x; b /= x; return *this; }

	friend constexpr bool		operator<(const T &a, const interval &b)				{ return a <= b.b; }
	friend constexpr interval	mul_about(const interval &i, const T &c, const T &t)	{ return (i - c) * t + c; }
	friend constexpr interval	mul_centre(const interval &i, const T &t)				{ return mul_about(i, i.centre(), t); }
	friend constexpr interval	abs(const interval &i)									{ return {i.minimum(), i.maximum()}; }
	template<typename B> friend constexpr bool		overlap(const interval &a, const B &b)			{ return !(any(b.b < a.a) || any(a.b < b.a)); }
	friend constexpr bool		strict_overlap(const interval &a, const interval &b)	{ return all(a.b > b.a) && all(a.a < b.b); }
};

template<typename T> int		max_component_index(const interval<T>& i)	{ return max_component_index(i.extent()); }

template<typename T> interval<T>	make_interval(const T &a)					{ return interval<T>(a); }
template<typename T> interval<T>	make_interval(const interval<T> &a)			{ return a; }
template<typename T> interval<T>	make_interval(const T &a, const T &b)		{ return interval<T>(a, b); }
template<typename T> interval<T>	make_interval_len(const T &a, const T &b)	{ return interval<T>(a, a + b); }
template<typename T> interval<T>	make_interval_abs(const T &a, const T &b)	{ return interval<T>(min(a, b), max(a, b)); }

template<typename T> interval<T>	empty_interval(const T&)					{ return none; }
template<typename T> interval<T>	empty_interval(const interval<T>&)			{ return none; }

template<typename I> auto get_extent(I i0, I i1) {
	if (i0 == i1)
		return empty_interval(*i0);

	auto	r = make_interval(*i0);
	for (++i0; i0 != i1; ++i0)
		r	|= *i0;
	return r;
}

template<typename T, typename I> auto get_extent(I i0, I i1) {
	interval<T>	r(none);
	for (;i0 != i1; ++i0)
		r	|= T(*i0);
	return r;
}

template<typename C> auto get_extent(const C &c) {
	return get_extent(begin(c), end(c));
}

template<typename T, typename C> auto get_extent(const C &c) {
	return get_extent<T>(begin(c), end(c));
}

template<typename T, typename C> auto scale_with_extent(const C &c, interval<T> ext) {
	return transformc(c, [offset = ext.a, scale = select(ext.extent() == zero, one, one / ext.extent())](T i) {
		return (i - offset) * scale;
	});
}

} // namespace iso
#endif // INTERVAL_H
