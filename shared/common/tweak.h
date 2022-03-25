#ifndef	TWEAK_H
#define TWEAK_H

#if USE_TWEAKS

#include "base/strings.h"
#include "base/vector.h"

//-----------------------------------------------------------------------------
// tweak - a value which adds itself to the tweak menu, allowing itself to be
//         edited at runtime, but which compiles to a constant in a RELEASE
//		   build.
// bool_tweak   - a tweak which acts like a bool
// int_tweak    - a tweak which acts like an int
// float_tweak  - a tweak which acts like a float
// float3_tweak - a tweak which acts like a float3
//
// Invoked through the the following macros:
//   TWEAK: acts like a variable of type bool, int, or float
//	 TWICK: acts like a pointer to a variable in code
//	 TWACK: acts like a lambda function, evaluating to a pointer to a piece of
//          data you would like to tweak
//	 GTWEAK, XTWEAK: if you need a tweak to be accessible in more than once
//			source file.  Like a global and extern variable.  Both must be
//			declared in source (not header) files, and both must have the same
//			default value assigned.
//	 TWUCK: acts like a TWEAK, but allows you to use a type similar to bool,
//			int, or float which has extra functionality.
//
// The implementation of data storage in tweak is determined by the most
// complicated case, the TWACK, and all other implementations are done so using
// this mechanism.
//
// The type, name, and value/variable/lambda_fn are required arguments.
// Optional arguments include minimum and maximum values, a comment about how
// the tuning parameter is used, labels about the meaning of each value
// (int_tweak only), and a trigger_call function callback allowing different
// functions to be invoked when the value changes.
// You must examine the constructors to see which arguments are set where.
//
// The tweak name should be specified in all-caps which underscores, like a
// macro variable.  The name will get parsed so that the first word becomes
// the submenu, and remainder of the name becomes the name of the item in the
// menu.  Underscores get converted to spaces, and the capitalization is changed
// so that only the first letter of each word is capitalized.
//-----------------------------------------------------------------------------

class tweak;

enum tweak_type { tw_bool, tw_int, tw_float, tw_float3 };
template<typename T> struct tweak_base_type { typedef T type; };

struct tweak_spec {
	tweak_type		type;

	typedef	void	updatefn_t(tweak *me, float d);
	typedef void	printfn_t(const tweak *me, iso::string_accum &buffer);
	typedef void*	getptrfn_t(tweak *me);

	updatefn_t		*update;
	printfn_t		*print;
	getptrfn_t		*getptr;
	tweak_spec(tweak_type _type, updatefn_t *_update, printfn_t *_print, getptrfn_t *_getptr)
		: type(_type), update(_update), print(_print), getptr(_getptr)	{}
};

template<typename T> struct tweak_spec2 : tweak_spec {
	static void		update(tweak *me, float d)							{ static_cast<T*>(me)->update(d); }
	static void		print(const tweak *me, iso::string_accum &buffer)	{ static_cast<const T*>(me)->print(buffer); }
	static void*	getptr(tweak *me)									{ return static_cast<T*>(me)->getptr(); }
public:
	tweak_spec2() : tweak_spec(tweak_type(T::type), update, print, getptr) {}
};
template<typename T> tweak_spec *tweak_get_spec(const T *t)	{ static tweak_spec2<T> spec; return &spec; }

class tweak {
	const char	*name, *comment;
	tweak_spec	*spec;

public:
	tweak(tweak_spec *_spec, const char *_name, const char *_comment);

	const char*	GetItemName()	const	{ return name;		}
	const char*	GetComment()	const	{ return comment;	}

	void		UpdateVal(float d)		{ spec->update(this, d); }
	void*		GetPtr()				{ return spec->getptr(this); }
	tweak_type	GetType()		const	{ return spec->type; }
	friend iso::string_accum& operator<<(iso::string_accum &buffer, const tweak &me)	{ me.spec->print(&me, buffer); return buffer; }
};

// tweak acessors
template<typename T> T *tweak_ptr(T &t)	{ return &t; }

template<typename T> class tweak_triggered {
	friend T *tweak_ptr(const tweak_triggered<T> &t)	{ return &t.t; };
	void (*trigger)(T*);
	T	t;
public:
	operator T() const			{ return t; }
	void operator=(const T &v)	{ t = v; (*trigger)(&t); }
	tweak_triggered(void (*_trigger)(T*)) : trigger(_trigger) {}
};
template<typename T> struct tweak_base_type<tweak_triggered<T> > { typedef T type; };

template<typename T> class tweak_ref {
	friend T *tweak_ptr(tweak_ref<T> &t)	{ return t.t; };
	T	*t;
public:
	operator T() const			{ return *t; }
	void operator=(const T &v)	{ *t = v; }
	T*	get_ptr()				{ return t; }
	tweak_ref(T *_t) : t(_t)	{}
};
template<typename T> struct tweak_base_type<tweak_ref<T> > { typedef T type; };

template<typename T> class tweak_lambda {
	friend T *tweak_ptr(tweak_lambda<T> &t)	{ return t.lambda(); };
	T* (*lambda)();
public:
	operator T() const			{ return *((*lambda)()); }
	void operator=(const T &v)	{ *((*lambda)()) = v; }
	tweak_lambda(T* (*_lambda)()) : lambda(_lambda)	{}
};
template<typename T> struct tweak_base_type<tweak_lambda<T> > { typedef T type; };


// tweak types

template<typename T, typename B = typename tweak_base_type<T>::type> class tweak2;

template<class T> class tweak2<T, bool> : public tweak {
	T			t;
public:
	enum {type = tw_bool};
	tweak2(const char *_name, const T &_t, const char *_comment = NULL)
		: tweak(tweak_get_spec(this), _name, _comment), t(_t)
	{}
	operator bool() const				{ return t; }
	void	operator=(const bool &v)	{ t = v;	}

	void	update(float d)				{ if (int(d) & 1) *this = !*this; }
	void	print(iso::string_accum &buffer) const { buffer << (bool)*this; }
	void	*getptr()					{ return tweak_ptr(t); }
};

template<class T> class tweak2<T, int> : public tweak {
	T			t;
	int			minimum, maximum, step;
	const char	**labels;
public:
	enum {type = tw_int};
	tweak2(const char *_name, const T &_t, int _minimum = 0, int _maximum = 100, int _step = 0, const char *_comment = NULL, const char **_labels = NULL)
		: tweak(tweak_get_spec(this), _name, _comment), t(_t)
		, minimum(_minimum), maximum(_maximum)
		, labels(_labels)
	{
		step = _step ? _step : (maximum - minimum) * 0.05f;
	}
	operator int() const				{ return t; }
	void	operator=(const int &v)		{ t = v;	}

	void	update(float d)				{ *this = iso::clamp(*this + (int)(d * step), minimum, maximum); }
	void	print(iso::string_accum &buffer) const { if (labels) buffer << labels[(int)*this]; else buffer << (int)*this; }
	void	*getptr()					{ return tweak_ptr(t); }
};

template<class T> class tweak2<T, float> : public tweak {
	T			t;
	float		minimum, maximum, step;
public:
	enum {type = tw_float};
	tweak2(const char *_name, const T &_t, float _minimum = 0.0f, float _maximum = 100.0f, float _step = 0.0f, const char *_comment = NULL)
		: tweak(tweak_get_spec(this), _name, _comment), t(_t)
		, minimum(_minimum), maximum(_maximum)
	{
		step = _step ? _step : (maximum - minimum) * 0.05f;
	}
	operator float() const				{ return t;	}
	void	operator=(const float &v)	{ t = v;	}

	void	update(float d)				{ *this = iso::clamp(*this + d * step, minimum, maximum); }
	void	print(iso::string_accum &buffer) const { buffer.format("%.4f", (float)*this); }
	void	*getptr()					{ return tweak_ptr(t); }
};

template<class T> class tweak2<T, iso::float3> : public tweak {
	T			t;
public:
	enum {type = tw_float3};
	tweak2(const char *_name, const T &_t, const char *_comment = NULL)
		: tweak(tweak_get_spec(this), _name, _comment), t(_t)
	{}
	operator iso::float3() const			{ return t; }
	void operator=(const iso::float3 &v)	{ v = t; }

	void	update(float d)					{}
	void	print(iso::string_accum &buffer) const {
		iso::float3 f3 = operator iso::float3();
		buffer.format("%.4f, %.4f, %.4f", float(f3.x), float(f3.y), float(f3.z));
	}
	void	*getptr()						{ return tweak_ptr(t); }
};


#define TWEAK(type, name, dfl_val, ...)			tweak2<type> name(#name, (dfl_val), ## __VA_ARGS__)
#define TWICK(type, name, ref_var, ...) 		tweak2<tweak_ref<type> > name(#name, tweak_ref<type>(&(ref_var)), ## __VA_ARGS__)
#define TWACK(type, name, lam_fn, ...)	 		tweak2<tweak_lambda<type> > name(#name, tweak_lambda<type>(lam_fn), ## __VA_ARGS__)
#define GTWEAK(type, name, dfl_val, ...) 		tweak2<type> name(#name, (dfl_val), ## __VA_ARGS__)

#define XTWEAK(type, name, dfl_val)				extern tweak2<type> name;

#else // USE_TWEAKS

#ifdef PLAT_WII
	#define TWEAK(type, name, ...)				static const type  name = VA_ARG_HEAD(__VA_ARGS__)
	#define TWICK(type, name, ...)				static const type& name = VA_ARG_HEAD(__VA_ARGS__)
	#define TWACK(type, name, ...)				static const type& name = *(VA_ARG_HEAD(__VA_ARGS__))
#else
	#define TWEAK(type, name, dfl_val, ...)		static const type  name = dfl_val
	#define TWICK(type, name, ref_var, ...)		static const type& name = ref_var
	#define TWACK(type, name, lam_fn , ...)		static const type& name = *lam_fn()
#endif
#define GTWEAK(type, name, dfl_val)
#define XTWEAK(type, name, dfl_val)				static const type  name = dfl_val

#endif // USE_TWEAKS

#endif // TWEAK_H
