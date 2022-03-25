#ifndef CONVERT_H
#define CONVERT_H

#include "iso.h"

namespace ISO {

//-----------------------------------------------------------------------------
//	Conversion
//-----------------------------------------------------------------------------


template<typename T> Type* TL_getdef() { return getdef<tuple<T>>(); }

template<typename T>	constexpr uint32	stride(T *t)					{ return sizeof(T); }
template<typename T>	constexpr uint32	stride(stride_iterator<T> t)	{ return t.stride(); }

struct ConversionFlags {
	enum FLAGS {
		NONE				= 0,
		RECURSE				= 1 << 0,
		CHECK_INSIDE		= 1 << 1,
		FULL_CHECK			= 1 << 2,
		ALLOW_EXTERNALS		= 1 << 3,
		EXPAND_EXTERNALS	= 1 << 4,
		CHANGE				= 1 << 5,	//
		MEMORY32			= 1 << 6,
	};
	friend FLAGS operator|(FLAGS a, FLAGS b) { return FLAGS((int)a | (int)b); }
	friend FLAGS operator-(FLAGS a, FLAGS b) { return FLAGS((int)a & ~(int)b); }
};

typedef virtfunc<ptr_machine<void>(const ptr_machine<void> &p, const Type *type, ConversionFlags::FLAGS flags)>	conversion_vf;

class Conversion : public static_list<Conversion>, public ConversionFlags, public conversion_vf {
	static bool									checkinside(const Browser &b, FLAGS flags, int depth);
	static ptr_machine<void>					checkinside(ptr_machine<void> p, FLAGS flags, int depth);
	static iso_export ptr_machine<void>			_convert(ptr_machine<void> p, const Type *type, FLAGS flags, int depth);
	template<typename T> static ptr_machine<T>	_convert(const ptr_machine<void> &p, FLAGS flags, int depth) {
		return _convert(p, getdef<T>(), flags, depth);
	}

protected:
	template<typename T> struct helper_s {
		static force_inline const Type* def() { return getdef<T>(); }
		static force_inline T& 			get(const ptr_machine<void> &p)	{ return *(T*)p; }
	};

	template<typename T> struct helper_s<T&> : helper_s<T> {};

	template<typename T, int B>	struct helper_s<ptr<T,B>> {
		static force_inline const Type* def() { return getdef<T>(); }
		static force_inline ptr<T,B>	get(const ptr_machine<void> &p)	{ return p; }
	};

	template<typename T> static force_inline const Type*	notptr_def() 					{ return helper_s<T>::def(); }
	template<typename T> static force_inline decltype(auto)	get(const ptr_machine<void> &p)	{ return helper_s<T>::get(p); }

	template<typename R>		static force_inline ptr_machine<void>	ret(tag2 id, R &&r, FLAGS flags) {
		if (flags & MEMORY32)
			return ptr<R,32>(id, forward<R>(r));
		return ptr_machine<R>(id, forward<R>(r));
	}
	template<typename R>		static force_inline ptr_machine<void>	ret(tag2 id, const R &r, FLAGS flags) {
		if (flags & MEMORY32)
			return ptr<R,32>(id, r);
		return ptr_machine<R>(id, r);
	}
	template<typename R, int B>	static force_inline ptr_machine<void>	ret(tag2 id, ptr<R, B> &&r, FLAGS flags)		{ return r; }
	template<typename R, int B>	static force_inline ptr_machine<void>	ret(tag2 id, const ptr<R, B> &r, FLAGS flags)	{ return r; }

public:
	static bool batch_convert(
		const void	*srce, uint32 srce_stride, const Type *srce_type,
		void		*dest, uint32 dest_stride, const Type *dest_type,
		uint32 num, bool convert_ptrs = true, void *physical_ram = NULL
	);

	template<typename S, typename D> static bool batch_convert(S srce, D dest, uint32 num, bool convert_ptrs = true, void *physical_ram = NULL) {
		return batch_convert(
			(void*)srce, stride(srce), getdef<noref_t<decltype(*srce)>>(),
			(void*)dest, stride(dest), getdef<noref_t<decltype(*dest)>>(),
			num, convert_ptrs, physical_ram
		);
	}
	template<typename S, typename D> static bool batch_convert(S&& srce, const Type *srce_type, D&& dest, uint32 num, bool convert_ptrs = true, void *physical_ram = NULL) {
		return batch_convert(
			(void*)srce, stride(srce), srce_type,
			(void*)dest, stride(dest), getdef<noref_t<decltype(*dest)>>(),
			num, convert_ptrs, physical_ram
		);
	}
	template<typename S, typename D> static bool batch_convert(S&& srce, D&& dest, bool convert_ptrs = true, void *physical_ram = NULL) {
		using iso::begin;
		return batch_convert(forward<S>(srce), begin(dest), num_elements(dest), convert_ptrs, physical_ram);
	}
	template<typename S, typename D> static bool batch_convert(S&& srce, const Type *srce_type, D&& dest, bool convert_ptrs = true, void *physical_ram = NULL) {
		using iso::begin;
		return batch_convert(forward<S>(srce), srce_type, begin(dest), num_elements32(dest), convert_ptrs, physical_ram);
	}

	static bool convert(
		const void	*srce, const Type *srce_type,
		void		*dest, const Type *dest_type,
		bool convert_ptrs = true, void *physical_ram = NULL
	) {
		return batch_convert(
			srce, 0, srce_type,
			dest, 0, dest_type,
			1, convert_ptrs, physical_ram
		);
	}
	static bool convert(const Browser &srce, const Browser &dest, bool convert_ptrs = true, void *physical_ram = NULL) {
		return batch_convert(
			srce, 0, srce.GetTypeDef(),
			dest, 0, dest.GetTypeDef(),
			1, convert_ptrs, physical_ram
		);
	}
	iso_export static ptr_machine<void> convert(const ptr_machine<void> &p, const Type *type, FLAGS flags = RECURSE, int depth = 64);
	iso_export static ptr_machine<void> convert(const Browser &b, const Type *type, FLAGS flags = RECURSE, int depth = 64);

	template<typename T> static ptr_machine<T> convert(const ptr_machine<void> &p, FLAGS flags = RECURSE, int depth = 64) {
		return convert(p, getdef<T>(), flags);
	}
	template<typename T> static ptr_machine<T> convert(const Browser &b, FLAGS flags = RECURSE, int depth = 64) {
		return convert(b, getdef<T>(), flags);
	}

	template<typename T> Conversion(T *t) : conversion_vf(t)	{}
};

template<> struct Conversion::helper_s<const char*> {
	static force_inline const char*	get(const ptr_machine<void> &p)	{
		return (const char*)p.GetType()->SkipUser()->ReadPtr(p);
	}
};

bool Assign(const Browser &b1, const Browser &b2, bool convert_ptrs = true);
bool Fix(const Browser &b);


//	function-like conversions where parameter list already has an def

template<class F> class ConversionConvT;
template<typename R, typename P1>	class ConversionConvT<R(P1)> : public Conversion {
	R(*f)(P1);
public:
	ptr_machine<void> operator()(ptr_machine<void> p, const Type *type, FLAGS flags) {
		if (p.GetType() == notptr_def<P1>())
			return ret(p.ID(), f(get<P1>(p)), flags);
		return ISO_NULL;
	}
	ConversionConvT(R(*f)(P1)) : Conversion(this), f(f)	{}
};

//	cast-like conversions (checks for correct return type)

template<class F> class ConversionCastT;
template<typename R, typename P1>	class ConversionCastT<R(P1)> : public Conversion {
	R(*f)(P1);
public:
	ptr_machine<void> operator()(ptr_machine<void> p, const Type *type, FLAGS flags) {
		if (type == notptr_def<R>() && (!(flags & RECURSE) ? (p.GetType() == notptr_def<P1>()) : (p = convert(p, notptr_def<P1>(), flags - RECURSE))))
			return ret(p.ID(), f(get<P1>(p)), flags);
		return ISO_NULL;
	}
	ConversionCastT(R(*f)(P1)) : Conversion(this), f(f)	{}
};

//	function-like conversions

template<class F> class ConversionT;
template<typename R, typename... PP> class ConversionT<R(PP...)> : public Conversion {
	typedef map_t<T_noref, tuple<PP...>>	_params;
	R				(*f)(PP...);
	TypeUserSave	param_type;

	template<size_t... II> force_inline R call(_params *p, index_list<II...>&&) {
//		return f(get<PP>(p->template get<II>())...);
		return f(p->template get<II>()...);
	}

public:
	ptr_machine<void> operator()(ptr_machine<void> p, const Type *type, FLAGS flags) {
		if (p.GetType() == &param_type)
			return ret(p.ID(), call((_params*)p, meta::make_index_list<sizeof...(PP)>()), flags);
		return ISO_NULL;
	}
	ConversionT(R(*f)(PP...), tag2 id, Type::FLAGS flags) : Conversion(this), f(f), param_type(id, getdef<_params>(), flags) {}
};
template<typename R, typename P> class ConversionT<R(P)> : public Conversion {
	typedef noref_t<P>	_params;
	R				(*f)(P);
	TypeUserSave	param_type;
public:
	ptr_machine<void> operator()(ptr_machine<void> p, const Type *type, FLAGS flags) {
		if (p.GetType() == &param_type)
			return ret(p.ID(), f(*(_params*)p), flags);
		return ISO_NULL;
	}
	ConversionT(R(*f)(P), tag2 id, Type::FLAGS flags) : Conversion(this), f(f), param_type(id, getdef<_params>(), flags) {}
};
template<typename F> struct get_operation_s {
	template<F f> static Conversion *conversion() {
		static ConversionConvT<F> t(f);
		return &t;
	}
	template<F f> static Conversion *cast() {
		static ConversionCastT<F> t(f);
		return &t;
	}
	template<F f> static Conversion *operation(tag2 id, Type::FLAGS flags) {
		static ConversionT<F> t(f, id, flags);
		return &t;
	}
};

#define ISO_get_conversion(f)			T_get_class<ISO::get_operation_s>(f)->conversion<f>()
#define ISO_get_cast(f)					T_get_class<ISO::get_operation_s>(f)->cast<f>()
#define ISO_get_operation(f)			T_get_class<ISO::get_operation_s>(f)->operation<f>(#f,  ISO::TypeUser::CHANGE)
#define ISO_get_operation2(id, f)		T_get_class<ISO::get_operation_s>(f)->operation<f>(#id, ISO::TypeUser::CHANGE)
#define ISO_get_operation_external(f)	T_get_class<ISO::get_operation_s>(f)->operation<f>(#f,  ISO::TypeUser::CHANGE | ISO::TypeUser::WRITETOBIN)

}//namespace ISO

#include "jobs.h"

namespace iso {
	using ISO_conversion = ISO::Conversion;

	template<typename T> bool _Convert(dynamic_array<T>& a, const ISO::Type *srce_type, const void *srce_data) {
		ISO::Browser	b(srce_type, (void*)srce_data);
		uint32			n	= b.Count();
		auto			bi	= b.begin();

		create(&a, n);

		return ISO::Conversion::batch_convert(
			*bi, (uint32)bi.GetStride(), bi.GetType(),
			a.begin(), sizeof32(T), ISO::getdef<T>(),
			n
		);
	}


	template<typename K, typename T> bool _Convert(hash_map_with_key<K, T>& a, const ISO::Type *srce_type, const void *srce_data) {
		create(&a);
		ISO::Browser	b(srce_type, (void*)srce_data);
		Mutex	mutex;
#if 1
		parallel_for(b, [&a, &mutex](const ISO::Browser &i) {
			auto	w	= with(mutex);
			auto	&d	= a[i["a"].get<K>()].put();
			ISO::Conversion::convert(i["b"], ISO::MakeBrowser(d));
		});
#else
		for (auto i : b) {
			auto	&d	= a[i["a"].get<K>()].put();
			ISO::Conversion::convert(i["b"], ISO::MakeBrowser(d));
		}
#endif
		return true;
	}


}

#endif// CONVERT_H
