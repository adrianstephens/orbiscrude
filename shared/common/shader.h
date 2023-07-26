#ifndef SHADER_H
#define SHADER_H

#include "iso/iso.h"
#include "base/vector.h"
#include "usage.h"

namespace iso {

struct VertexElement;
class GraphicsContext;

class CONCAT2(ISO_PREFIX,Shader);
typedef CONCAT2(ISO_PREFIX,Shader) pass;

void Init(pass*,void*);
void DeInit(pass*);

struct technique	: ISO_openarray<ISO_ptr<void> >			{};
struct fx			: ISO_openarray<ISO_ptr<technique> >	{};

/*
struct ShaderVal {
	const char *name;
	const void *data;
};

struct ShaderVals {
	const ShaderVal	*m;
	uint32			n;

	uint32		Count()			{ return n;	}
	const char *GetName(int i)	{ return m[i].name;	}
	ISO::Browser	Index(int i)	{ return ISO::Browser(0, const_cast<void*>(m[i].data));	}
	static ISO::Browser2	Deref() { return ISO::Browser2(); }
	static bool	Update(const char *s, bool from) { return false; }

	int			GetIndex(const tag &id, int from) {
		for (const ShaderVal *i = m + from, *e = i + n; i != e; ++i) {
			if (i->name == id)
				return int(i - m);
		}
		return -1;
	}

	template<uint32 N> ShaderVals(const ShaderVal (&_m)[N]) : m(_m), n(N) {}
	operator ISO::Browser() const { return ISO::MakeBrowser(*this); }

	const void*	operator[](int i)	const { return m[i].data; }
	const void*	operator[](const char *id) const {
		for (const ShaderVal *i = m, *e = i + n; i != e; ++i) {
			if (strcmp(i->name, id) == 0)
				return i->data;
		}
		return 0;
	}
};
ISO_DEFVIRT(ShaderVals);
*/

struct ShaderVal {
	const char *name;
	const void *data;
};

struct ShaderVals {
	const ShaderVal	*m;
	uint32			n;
	ShaderVals(const ShaderVal *m, uint32 n) : m(m), n(n) {}
	template<uint32 N> ShaderVals(const ShaderVal (&m)[N]) : m(m), n(N) {}
	const void*	operator[](const char *id) const {
		for (const ShaderVal *i = m, *e = i + n; i != e; ++i) {
			if (strcmp(i->name, id) == 0)
				return i->data;
		}
		return 0;
	}
};

template<int N> struct ShaderValsN : ShaderVals {
	ShaderVal vals[N ? N : 1];

	void Init(int i) {}
	template<typename P0, typename...P> void Init(int i, const char *id, P0 *p0, P...p) {
		vals[i] = {id, p0};
		Init(i + 1, p...);
	}
	template<typename...P> ShaderValsN(P...p) : ShaderVals(vals) { Init(0, p...); }
	operator ISO::Browser()	{ return ISO::MakeBrowser((ShaderVals*)this); }
};

template<typename...P> auto MakeShaderVals(const P&...p) {
	return ShaderValsN<sizeof...(P) / 2>(p...);
}


struct ShaderLoop {
	int	count, start, step, unused;

	ShaderLoop(int n = 0, int s = 0, int b = 1) : count(n), start(s), step(b), unused(0)	{}

	ShaderLoop &operator=(int i){ count = i; return *this;	}
	ShaderLoop &Count(int i)	{ count	= i; return *this;	}
	ShaderLoop &Start(int i)	{ start	= i; return *this;	}
	ShaderLoop &Step(int i)		{ step	= i; return *this;	}
	void		Set(int n, int s = 0, int b = 1) { count = n; start = s; step = b; }
};

struct ConstantDescriptor {
	uintptr_t	reg;
	const void	*srce;
#ifndef ISO_RELEASE
  #ifdef PLAT_WII
	crc32		name;
  #elif defined PLAT_IOS || defined PLAT_PC
	string		name;
  #else
	const char	*name;
  #endif
#endif
};

class ShaderConstants {
	uint8				total;
	ConstantDescriptor	*constants;
	ConstantDescriptor	*CreateConstants(int n) {
		return constants = new_array<ConstantDescriptor>(total = n);
	}
public:
	void	Init(pass *pass, const ISO::Browser &parameters);
#ifdef PLAT_X360
	void	InitSet(GraphicsContext &ctx, pass *pass, const ISO::Browser &parameters, uint32 *stride, VertexElements *ve);
#else
	void	InitSet(GraphicsContext &ctx, pass *pass, const ISO::Browser &parameters);
#endif
	void	Set(GraphicsContext &ctx, pass *pass)	const;
	ShaderConstants() : constants(NULL)	{}
	~ShaderConstants()	{ delete_array(constants, total); }
};

arbitrary_ptr&			GetShaderParameter(crc32 name);
arbitrary_ptr			GetShaderParameter(crc32 name, const ISO::Browser &parameters);

template<typename T> void AddShaderParameter(crc32 name, T* t)							{ GetShaderParameter(name) = t;	}
template<typename T> void AddShaderParameter(crc32 name, const T& t)					{ GetShaderParameter(name) = &t;}
template<typename T> void AddShaderParameter(crc32 name, const T* t)					{ GetShaderParameter(name) = t;	}
template<typename T> void AddShaderParameter(crc32 name, const iso::ISO_ptr<T> &t)		{ GetShaderParameter(name) = (const T*)t;	}
inline void AddShaderParameter(crc32 name, const arbitrary_ptr &t)						{ GetShaderParameter(name) = t;	}

template<typename T> void AddShaderParameter(string_ref name, T&& t)		{ AddShaderParameter(crc32(name), t); }

template<uint32 name> inline arbitrary_ptr&		GetShaderParameter()					{ return GetShaderParameter(name); }
template<uint32 name, typename T> inline void	AddShaderParameter(const T &t)			{ AddShaderParameter(name, t);	}
template<uint32 name> inline arbitrary_ptr		GetShaderParameter(const ISO::Browser &parameters)	{ return GetShaderParameter(name, parameters); }

void Set(GraphicsContext &ctx, pass *pass, const ISO::Browser &parameters = ISO::Browser());
void SetSkinning(const iso::float3x4 *mats, int nmats);

} //namespace iso

namespace ISO {
ISO_DEFCALLBACK(CONCAT2(ISO_PREFIX,Shader), void);
ISO_DEFUSERX(technique,	ISO_openarray<ISO_ptr<void> >,			"technique");
ISO_DEFUSERX(fx,		ISO_openarray<ISO_ptr<technique> >,		"fx");

template<> struct def<ShaderVals> : public ISO::VirtualT2<ShaderVals> {
	static uint32		Count(const ShaderVals &sv)								{ return sv.n;	}
	static const char*	GetName(const ShaderVals &sv, int i)					{ return sv.m[i].name;	}
	static ISO::Browser	Index(const ShaderVals &sv, int i)						{ return ISO::Browser(0, const_cast<void*>(sv.m[i].data)); }
	static ISO::Browser2 Deref(const ShaderVals &sv)							{ return ISO::Browser2(); }
	static bool			Update(const ShaderVals &sv, const char *s, bool from)	{ return false; }
	static int			GetIndex(const ShaderVals &sv, const tag2 &id, int from) {
		for (const ShaderVal *i = sv.m + from, *e = i + sv.n; i != e; ++i) {
			if (i->name == id)
				return int(i - sv.m);
		}
		return -1;
	}
};

}//namespace ISO

#endif	// SHADER_H
