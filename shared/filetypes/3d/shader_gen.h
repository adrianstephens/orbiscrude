#ifndef SHADER_GEN_H
#define SHADER_GEN_H

#include "base/strings.h"
#include "base/vector.h"
#include "base/algorithm.h"
#include "iso/iso.h"
#include "fx.h"
#include "render.h"
#include "model_utils.h"

namespace iso {

template<typename T, uint32 CRC> struct unit {
	T		t;
	void	operator=(T _t)	{ t = _t; }
	operator T() const		{ return t; }
};
//template<typename T, uint32 CRC> struct vget<unit<T,CRC> >	: vget<T> {};
typedef unit<float, crc32_const("distance")>	distance_unit;
typedef unit<float, crc32_const("time")>		time_unit;

template<typename M, typename T> void read_prop(M &m, ISO::Browser b, T &t)	{
	t = b.Get(T());
}
template<typename M, typename T> void read_prop(M &m, ISO::Browser b, const char *id, T &t)	{
	read_prop(m, b[id], t);
}

template<typename M, typename T> void read_prop(M &m, ISO::Browser b, const char *id, dynamic_array<T> &t) {
	int	i = 0;
	while (ISO::Browser b2 = m.GetArrayElement(b, id, i)) {
		read_prop(m, b2, t.push_back());
		++i;
	}
}
template<typename M> void read_prop(M &m, ISO::Browser b, colour &t) {
	t = m.GetColour(b);
}
template<typename M> void read_prop(M &m, ISO::Browser b, distance_unit &t) {
	t = m.GetDistance(b);
}
template<typename M> void read_prop(M &m, ISO::Browser b, time_unit &t) {
	t = m.GetTime(b);
}
template<typename M> void read_prop(M &m, ISO::Browser b, bool &t) {
	t = !!b.GetInt();
}

#if 0
template<typename M, typename T0> void read_props(M &m, ISO::Browser b, const char *id0, T0 &t0) {
	read_prop(m, b, id0, t0);
}
template<typename M, typename T0, typename... T> void read_props(M &m, ISO::Browser b, const char *id0, T0 &t0, T& ...t) {
	read_prop(m, b, id0, t0);
	read_props(m, b, t...);
}
#define F(x)	#x, x
#elif 0
template<typename M, typename... T, size_t... I> void read_props(index_list<I...>, M &m, ISO::Browser b, T& ...t) {
	bool	dummy[] = {
		(read_prop(m, b, PP_index<I * 2 + 0>(t...), PP_index<I * 2 + 1>(t...)), true)...
	};
}
template<typename M, typename... T> void read_props(M &m, ISO::Browser b, T& ...t) {
	read_props(meta::make_index_list<sizeof...(T) / 2>(), m, b, t...);
}
#define F(x)	#x, x
#else
template<typename M, typename A, typename B> void read_prop(M &m, ISO::Browser b, pair<A, B> t)	{
	read_prop(m, b, t.a, t.b);
}

template<typename M, typename... T> void read_props(M &m, ISO::Browser b, T ...t) {
	bool	dummy[] = {
		(read_prop(m, b, t.a, t.b), true)...
	};
}
#define F(x)	make_pair(#x, x)
#endif

//-----------------------------------------------------------------------------
//	ShaderWriter
//-----------------------------------------------------------------------------

struct ShaderWriter : string_builder {
	enum CONTEXT {
		CTX_GLOBALS,
		CTX_COLOUR,
		CTX_MONO,
		CTX_NORMAL,
		CTX_WORLDMAT,
	};

	enum STAGE {
		PS, VS,
	} stage;

	enum MODE {
		MODE_OPEN,
		MODE_ARG,
		MODE_STMT,
	} mode;

	int		tabs;

	ShaderWriter(STAGE stage) : stage(stage), mode(MODE_OPEN), tabs(0) {}
	void Write(CONTEXT context, param(colour) col) {
		switch (context) {
			case ShaderWriter::CTX_COLOUR:
				*this << "float4(" << col.r << ", " << col.g << ", " << col.b << ", " << col.a << ")";
				break;
			case ShaderWriter::CTX_MONO:
				*this << colour_HSV(col).v;
				break;
		}
	}
	ShaderWriter&	Open(const char *fn, char open = '(') {
		*this << fn << open;
		++tabs;
		mode = MODE_OPEN;
		return *this;
	}
	void			Close(char close, MODE next_mode) {
		--tabs;
		if (mode == MODE_STMT)
			*this << ';';
		if (mode != MODE_OPEN)
			*this << '\n' << repeat('\t', tabs);
		*this << close;
		mode = next_mode;
	}
	void			Close(MODE next_mode = MODE_ARG) {
		Close(mode == MODE_STMT ? '}' : ')', next_mode);
	}
	ShaderWriter&	Arg() {
		*this << onlyif(mode != MODE_OPEN, ',') << "\n" << repeat('\t', tabs);
		mode = MODE_ARG;
		return *this;
	}
	ShaderWriter&	Stmt() {
		*this << onlyif(mode != MODE_OPEN, ';') << "\n" << repeat('\t', tabs);
		mode = MODE_STMT;
		return *this;
	}
};

#if 0
struct RefTarget {
	uint32	nrefs;
	void	(*deleter)(RefTarget*);
	void	addref()		{ ++nrefs; }
	void	release()		{ if (!--nrefs) deleter(this); }
	template<typename T> static void thunk(RefTarget *t) {  delete static_cast<T*>(t); }
	template<typename T> RefTarget(T *t) : deleter(thunk<T>), nrefs(0) {}
};
#else
struct ISOTarget {
	void	addref()							{ ISO::GetHeader(this)->addref(); }
	void	release()							{ ISO::GetHeader(this)->release(); }
	void	*operator new(size_t size)			{ return ISO::MakeRawPtrSize<64>(0, 0, (uint32)size); }
	void	*operator new(size_t size, void*p)	{ return p; }
	template<typename T> ISOTarget(T *t)		{ ISO::GetHeader(this)->type = ISO::getdef<T>(); }
};
typedef ISOTarget RefTarget;
#endif

class ShaderSource : public RefTarget, public virtfunc<void(ShaderWriter&, ShaderWriter::CONTEXT)> {
public:
	template<typename T> ShaderSource(T *t) : RefTarget(t) { virtfunc::bind<T, &T::WriteShader>(); }
	void	WriteShader(ShaderWriter& shader, ShaderWriter::CONTEXT context) { (*this)(shader, context); }
};

class ColourSource : public ShaderSource {
public:
	template<typename T> ColourSource(T *t) : ShaderSource(t) {}
};

class PositionSource : public ShaderSource {
public:
	template<typename T> PositionSource(T *t) : ShaderSource(t) {}
};

class TextureOutput: public ShaderSource {
public:
	template<typename T> TextureOutput(T *t) : ShaderSource(t) {}
};

template<typename M> void read_prop(M &m, ISO::Browser b, ref_ptr<ColourSource> &t) {
	t	= m.GetColourSource(b);
}
template<typename M> void read_prop(M &m, ISO::Browser b, TextureOutput *&t)				{ t	= 0; }
template<typename M> void read_prop(M &m, ISO::Browser b, ref_ptr<PositionSource> &t)	{ t	= 0; }

//-----------------------------------------------------------------------------
//	MaterialMaker
//-----------------------------------------------------------------------------

class ShadingModel;

struct MaterialMaker {
	enum FLAGS {
		_FIRST		= RMASK_SUBVIEW,
		NORMALS		= _FIRST << 0,
		COLOURS		= _FIRST << 1,
		SHADOW		= _FIRST << 2,
		UVDEFAULT	= _FIRST << 3,
		SKIN		= _FIRST << 4,
	};
	uint32				flags;
	const char			*tangent_uvs;

	ISO_ptr<iso::technique>		technique;
	ISO_ptr<anything>			parameters;
	dynamic_array<string>		inputs;

	MaterialMaker(uint32 flags) : flags(flags), tangent_uvs(0) {}
	void					Generate(tag2 id, ShadingModel *sm);
	ShaderSource*			GetVertSource();

	virtual const char*		GetUVSet(const ISO::Browser2 &b)			= 0;
	virtual ref_ptr<ColourSource> GetColourSource(const ISO::Browser &b)= 0;
	virtual colour			GetColour(const ISO::Browser &b)			= 0;
	virtual float			GetDistance(const ISO::Browser &b)			= 0;
	virtual float			GetTime(const ISO::Browser &b)				= 0;
	virtual ISO::Browser	GetArrayElement(const ISO::Browser &b, const char *id, int i) = 0;
};

//-----------------------------------------------------------------------------
//	ShadingModel
//-----------------------------------------------------------------------------

class ShadingModel : public RefTarget, public virtfunc<void(ShaderWriter&, MaterialMaker&)> {
public:
	static void	OpenMain(ShaderWriter& shader, MaterialMaker &maker);

	template<typename T> ShadingModel(T *t) : RefTarget(t) { virtfunc::bind<T, &T::WriteShader>(); }
	void	WriteShader(ShaderWriter& shader, MaterialMaker &maker) { (*this)(shader, maker); }
	void	WriteVS(ShaderWriter& shader, MaterialMaker &maker, ShaderSource *vert_source);
};

//-----------------------------------------------------------------------------
//	COLOUR SOURCES
//-----------------------------------------------------------------------------

class SolidColour : public ColourSource {
public:
	colour	col;
public:
	SolidColour(param(colour) _col)	: ColourSource(this), col(_col)	{}
	SolidColour(float f)	: ColourSource(this), col(f, f, f)	{}
	void	WriteShader(ShaderWriter& shader, ShaderWriter::CONTEXT context) {
		shader.Write(context, col);
	}
};

class TextureMap : public ColourSource {
	string			name;
	string			uv_set;
public:
	TextureMap(const char *_name, const char *_uv_set) : ColourSource(this), name(_name), uv_set(_uv_set) {}
	void	WriteShader(ShaderWriter& shader, ShaderWriter::CONTEXT context);
};

class ColourSource0 : public ColourSource {};
class ColourSource1 : public ColourSource {};

template<typename M> void read_prop(M &m, ISO::Browser b, ref_ptr<ColourSource0> &t) {
	ref_ptr<ColourSource> p;
	read_prop(m, b, p);
	t = (ColourSource0*)(p ? p.get() : new SolidColour(colour(zero)));
}
template<typename M> void read_prop(M &m, ISO::Browser b, ref_ptr<ColourSource1> &t) {
	ref_ptr<ColourSource> p;
	read_prop(m, b, p);
	t = (ColourSource1*)(p ? p.get() : new SolidColour(colour(one)));
}

//-----------------------------------------------------------------------------
//	Phong Material
//-----------------------------------------------------------------------------

struct Phong : ShadingModel {
	ref_ptr<ShaderSource>	vert_source;
	ref_ptr<ColourSource0>	AmbientColor;
	ref_ptr<ColourSource1>	DiffuseColor;
	ref_ptr<ColourSource>			DiffuseFactor;
	ref_ptr<ColourSource>			TransparencyFactor;
	ref_ptr<ColourSource1>	SpecularColor;
	ref_ptr<ColourSource>			SpecularFactor;
	ref_ptr<ColourSource>			ShininessExponent;
	ref_ptr<ColourSource>			ReflectionFactor;
	ref_ptr<ColourSource>			NormalMap;
	ref_ptr<ColourSource>			Bump;
	ref_ptr<ColourSource>			BumpFactor;

	static void WritePS(ShaderWriter& shader, MaterialMaker &maker, ColourSource *ambient, ColourSource *diffuse, ColourSource *specular, ColourSource *shininess, ColourSource *normalmap, bool bump_map);

	Phong(MaterialMaker &maker, ISO::Browser b, ShaderSource *_vert_source);
	void	WriteShader(ShaderWriter& shader, MaterialMaker &maker);
};

//-----------------------------------------------------------------------------
//	Lambert Material
//-----------------------------------------------------------------------------

struct Lambert : ShadingModel {
	ref_ptr<ShaderSource>	vert_source;
	ref_ptr<ColourSource0>	AmbientColor;
	ref_ptr<ColourSource1>	DiffuseColor;
	ref_ptr<ColourSource>			TransparencyFactor;
	ref_ptr<ColourSource>			DiffuseFactor;

	Lambert(MaterialMaker &maker, ISO::Browser b, ShaderSource *_vert_source);
	void	WriteShader(ShaderWriter& shader, MaterialMaker &maker);
};

//-----------------------------------------------------------------------------
//	MAX
//-----------------------------------------------------------------------------

// SuperClass ID
typedef uint32 SClass_ID;

struct Class_ID {
	uint32 a, b;
	Class_ID()						: a(~0), b(~0)	{}
	Class_ID(uint32 _a, uint32 _b)	: a(_a), b(_b)	{}
	friend bool operator==(const Class_ID &a, const Class_ID &b) {
		return a.a == b.a && a.b == b.b;
	}
};

struct MAXColourSourceCreator : static_list<MAXColourSourceCreator>, virtfunc<ref_ptr<ColourSource>(MaterialMaker&, const ISO::Browser&)>  {
	Class_ID	c;
	template<typename T> MAXColourSourceCreator(T *t, const Class_ID &c) : virtfunc<ref_ptr<ColourSource>(MaterialMaker&, const ISO::Browser&)>(t), c(c) {}
	static MAXColourSourceCreator *Find(const Class_ID &c, SClass_ID s) {
		for (auto i = begin(); i; ++i) {
			if (i->c == c)
				return i;
		}
		return 0;
	}
};

template<typename P> struct MAXColourSourceCreatorT : MAXColourSourceCreator {
	struct Source : ColourSource, P {
		Source(const P &params) : ColourSource(this), P(params) {}
		void	WriteShader(ShaderWriter& shader, ShaderWriter::CONTEXT context);
	};
	MAXColourSourceCreatorT() : MAXColourSourceCreator(this, Class_ID(P::IDA, P::IDB)) {}
	ref_ptr<ColourSource> operator()(MaterialMaker &maker, const ISO::Browser &props) {
		return new Source(P(maker, props));
	}
};

struct MAXShadingModelCreator : static_list<MAXShadingModelCreator>, virtfunc<ShadingModel*(MaterialMaker&, const ISO::Browser&, const ISO::Browser&, ShaderSource*)>  {
	Class_ID	c;
	template<typename T> MAXShadingModelCreator(T *t, const Class_ID &c) : virtfunc<ShadingModel*(MaterialMaker&, const ISO::Browser&, const ISO::Browser&, ShaderSource*)>(t), c(c) {}
	static MAXShadingModelCreator *Find(const Class_ID &c, SClass_ID s) {
		for (auto i = begin(); i; ++i) {
			if (i->c == c)
				return i;
		}
		return 0;
	}
	static ShadingModel *Create(MaterialMaker &maker, Class_ID id, SClass_ID super, const ISO::Browser material, ISO::Browser2 params) {
		if (MAXShadingModelCreator *creator = MAXShadingModelCreator::Find(id, super))
			return (*creator)(maker, material, params, maker.GetVertSource());
		return 0;
	}
};

template<typename P> struct MAXShadingModelCreatorT : MAXShadingModelCreator {
	struct Model : ShadingModel, P {
		ref_ptr<ShaderSource>	vert_source;
		Model(const P &params, ShaderSource *vert_source) : ShadingModel(this), P(params), vert_source(vert_source) {}
		void	WriteShader(ShaderWriter& shader, MaterialMaker &mat);
	};
	MAXShadingModelCreatorT() : MAXShadingModelCreator(this, Class_ID(P::IDA, P::IDB)) {}
	ShadingModel *operator()(MaterialMaker &maker, ISO::Browser material, const ISO::Browser &props, ShaderSource *vert_source) {
		return new Model(P(maker, material, props), vert_source);
	}
};

} // namespace iso

ISO_DEFCOMPV(iso::SolidColour, col);
ISO_DEFCOMP0(iso::TextureMap);
ISO_DEFCOMP0(iso::Phong);
ISO_DEFCOMP0(iso::Lambert);

#endif //SHADER_GEN_H
