#include "extra/identifier.h"
#include "hook_com.h"
#include "base\tuple.h"
#include <d3d11.h>

namespace iso {

template<> struct field_names<D3D_SHADER_VARIABLE_FLAGS>	{ static field_bit s[]; };
template<> struct field_names<D3D_SHADER_INPUT_FLAGS>		{ static field_value s[]; };
template<> struct field_names<D3D_NAME>						{ static field_value s[]; };
template<> struct field_names<D3D_FEATURE_LEVEL>			{ static field_value s[]; };
template<> struct field_names<D3D_MIN_PRECISION>			{ static field_value s[]; };
template<> struct fields<D3D_FEATURE_LEVEL> : value_field<D3D_FEATURE_LEVEL> {};

template<typename T> struct get_fields_s {
	static constexpr field *f() { return fields<T>::f; }
};
template<> struct get_fields_s<void*> {
	static constexpr field *f() { return fields<void*>::f; }
};

template<typename T> struct get_fields_s<T*> {
	static constexpr field *f() { return custom_ptr_field; }
};
template<typename T> struct get_fields_s<T* const> {
	static constexpr field *f() { return get_fields_s<T*>::f(); }
};
template<typename T0, typename... T> struct get_fields_s<union_first<T0, T...>> : get_fields_s<T0> {};

template<typename C, typename T> field *get_fields(T C::*t) {
	return get_fields_s<T>::f();
};

template<> struct field_maker<void*, false> {
	static constexpr field f(const char *name, uint32 start) { return field::make(name, start, sizeof(void*) * 8, field::MODE_CUSTOM, 0, sCustom); }
};
template<> struct field_maker<const void*, false> {
	static constexpr field f(const char *name, uint32 start) { return field::make(name, start, sizeof(void*) * 8, 0, 0, sHex); }
};
template<> struct field_maker<rel_ptr<const char>, true> {
	static constexpr field f(const char *name, uint32 start) { return field::make(name, start, 32, 0, 0, sRelString); }
};
template<> struct field_maker<rel_ptr<char>, true> {
	static constexpr field f(const char *name, uint32 start) { return field::make(name, start, 32, 0, 0, sRelString); }
};
template<> struct field_maker<rel_ptr<const wchar_t>, true> {
	static constexpr field f(const char *name, uint32 start) { return field::make(name, start, 32, 1, 0, sRelString); }
};
template<typename T> struct field_maker<rel_ptr<T>, true> {
	static constexpr field f(const char *name, uint32 start) { return field::make(name, start, 0, field::MODE_RELPTR, 0, (const char**)get_fields_s<T>::f()); }
};
template<> struct field_maker<rel_ptr<void*>, false> {
	static /*constexpr*/ field f(const char *name, uint32 start) { return field::make(name, start, 0, field::MODE_RELPTR, 0, (const char**)custom_ptr_field); }
};

template<bool COM> struct make_ptr_field_s {
	template<typename T> static constexpr field f(const char *name, uint32 start) { return field::make(name, start, sizeof(void*) * 8, field::MODE_CUSTOM, 0, sCustom);	}
};
template<> struct make_ptr_field_s<false> {
	template<typename T> static constexpr field f(const char *name, uint32 start) { return field::call<T, field::MODE_POINTER>(name, start);	}
};
template<typename T> struct field_maker<T*, false> {
	static constexpr field f(const char *name, uint32 start) { return make_ptr_field_s<is_com<T>::value>::template f<T>(name, start); }
};
//template<typename T0, typename... T> struct field_maker<union_first<T0, T...>, true> : field_maker<T0> {};
template<int I, typename T, size_t A> struct field_maker<rel_ptr<next_array<I, T, A>>, true> : field_maker<rel_ptr<void>> {};

//template<int N, typename B, typename T> constexpr field make_field_idx(const char *name, T B::*p) {
//	return field::make<B>(name, p);
//}
template<int N, typename B, typename T, int I, typename P> constexpr field make_field_idx(const char *name, counted<T,I,P> B::*p) {
	return field::make(name, T_get_member_offset(p) * 8, 0, field::MODE_RELPTR, N - I, (const char**)get_fields_s<T>::f());
}
template<int N, typename B, typename T, int I> constexpr field make_field_idx(const char *name, counted<T,I> B::*p) {
	return field::call<T, field::MODE_POINTER>(name, T_get_member_offset(p) * 8, N - I);
}
template<int N, typename B, int I, typename...TT> constexpr field make_field_idx(const char *name, selection<I,TT...> B::*p) {
	return field::call_union<TT...>(name, T_get_member_offset(p) * 8, N - I);
}


template<int N, typename B, typename T, int I, typename P> constexpr field make_field_idx_test(const char *name, counted<T,I,P> B::*p) {
	return field::make(name, T_get_member_offset(p) * 8, 0, field::MODE_RELPTR, N - I, (const char**)get_fields_s<T>::f());
}
template<int N, typename B, typename T, int I> constexpr field make_field_idx_test(const char *name, counted<T,I> B::*p) {
	return field::call<T, field::MODE_POINTER>(name, T_get_member_offset(p) * 8, N - I);
}
template<int N, typename B, int I, typename...TT> constexpr field make_field_idx_test(const char *name, selection<I,TT...> B::*p) {
	return field::call_union<TT...>(name, T_get_member_offset(p) * 8, N - I);
}

#define	MAKE_DX_FIELD_IDX(I,X)	make_field_idx<I,S>(#X, &S::X)
#define	MAKE_DX_FIELD(X)		field::make<S>(#X, &S::X)
#define	MAKE_DX_FIELDS(S,...)	VA_APPLYP(_MAKE_FIELD, S, __VA_ARGS__)
#define	MAKE_DX_FIELDT(X,T)		field::make<S,T>(#X, &S::X)
#define	CALL_FIELD(X)			field::call<S>(#X, &S::X)
#define	MAKE_UNION(F,O,N)		{0, iso_offset(S, F) * 8, 0, 0, O, (const char**)N}
#define	TERMINATOR				field::terminator<S>()

template<typename T> struct fp {
	typedef T	type;
	const char *p;
	constexpr fp(const char *p) : p(p) {}
};

template<typename IL> struct ff2;
template<int... II> struct ff2<index_list<II...> > {
	template<typename... PP> static field	*make(const PP&... pp) {
		typedef typename meta::map<RTM, type_list<typename PP::type...>>::type	P;
		static field f[sizeof...(II)+1] = {
			make_field_idx<II>(pp.p, TL_tuple<P>::template field<II>())...,
			0
		};
		return f;
	}
};

field *ff();

template<typename... PP> field *ff(const PP&... pp) {
	return ff2<meta::make_index_list<sizeof...(PP)>>::make(pp...);
}

} // namespace iso