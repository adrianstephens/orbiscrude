#include "extra/identifier.h"
#include "hook_com.h"
#include "base\tuple.h"
#include <d3d11.h>

namespace iso {

DECLARE_PREFIXED_ENUMS(DXGI_FORMAT);
DECLARE_PREFIXED_ENUMS(D3D_SHADER_VARIABLE_CLASS);
DECLARE_PREFIXED_ENUMS(D3D_SHADER_VARIABLE_TYPE);
DECLARE_PREFIXED_ENUMS(D3D_SHADER_INPUT_TYPE);
DECLARE_PREFIXED_ENUMS(D3D_RESOURCE_RETURN_TYPE);
DECLARE_PREFIXED_ENUMS(D3D_REGISTER_COMPONENT_TYPE);
DECLARE_PREFIXED_ENUMS(D3D_TESSELLATOR_DOMAIN);
DECLARE_PREFIXED_ENUMS(D3D_TESSELLATOR_PARTITIONING);
DECLARE_PREFIXED_ENUMS(D3D_TESSELLATOR_OUTPUT_PRIMITIVE);
DECLARE_PREFIXED_ENUMS(D3D_INTERPOLATION_MODE);
DECLARE_PREFIXED_ENUMS(D3D_PARAMETER_FLAGS);
DECLARE_PREFIXED_ENUMS(D3D_SRV_DIMENSION);

DECLARE_PREFIXED_BIT_ENUMS(D3D_SHADER_VARIABLE_FLAGS);
DECLARE_PREFIXED_VALUE_ENUMS(D3D_SHADER_INPUT_FLAGS);
DECLARE_PREFIXED_VALUE_ENUMS(D3D_NAME);
DECLARE_PREFIXED_VALUE_ENUMS(D3D_MIN_PRECISION);
DECLARE_PREFIXED_VALUE_ENUMS(D3D_FEATURE_LEVEL);

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

template<bool COM> struct make_ptr_field_s {
	template<typename T> static constexpr field f(const char *name, uint32 start) { return field::make(name, start, sizeof(void*) * 8, field::MODE_CUSTOM, 0, sCustom);	}
};
template<> struct make_ptr_field_s<false> {
	template<typename T> static constexpr field f(const char *name, uint32 start) { return field::call<field::MODE_POINTER>(name, start, fields<T>::f);	}
};
template<typename T> struct field_maker<T*, false> {
	static constexpr field f(const char *name, uint32 start) { return make_ptr_field_s<is_com<T>::value>::template f<T>(name, start); }
};
template<int I, typename T, size_t A> struct field_maker<rel_ptr<next_array<I, T, A>>, true> : field_maker<rel_ptr<void>> {};

template<typename T>	struct field_maker<dup_pointer<T>, true>	: field_maker<T>				{};
template<typename T>	struct fields<dup_pointer<T>>				: value_field<dup_pointer<T>>	{};

template<typename T>	struct field_maker<save_location<T>, true>	: field_maker<T>				{};
template<typename T>	struct fields<save_location<T>>				: value_field<save_location<T>>	{};

#define	_MAKE_FIELD_IDX(S,I,X)	make_field_idx<I,S>(#X, &S::X)
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