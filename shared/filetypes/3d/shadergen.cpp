#include "iso/iso_files.h"
#include "iso/iso_convert.h"
#include "base/algorithm.h"
#include "extra/indexer.h"
#include "extra/text_stream.h"
#include "extra/perlin_noise.h"
#include "bitmap/bitmap.h"
#include "fx.h"
#include "scenegraph.h"
#include "model_utils.h"
#include "zlib_stream.h"


namespace iso {
	typedef	soft_vector2<double>	double2;
	typedef	soft_vector3<double>	double3;

	template<class R> bool read(R &r, string &s) {
		if (size_t len	= r.getc()) {
			r.readbuff(s.alloc(len).begin(), len);
			s.begin()[len] = 0;
		}
		return true;
	}
	template<class W> bool write(W &w, const string &s) {
		size_t len = s.length();
		w.putc(len);
		w.writebuff(s, len);
		return true;
	}

	
	template<typename S> struct read_array_s {
		template<typename D> static void f(istream &file, D *d, int n) {
			while (n--)
				*d++ = file.get<S>();
		}
		static void f(istream &file, S *d, int n) {
			read(file, d, n);
		}
	};
	template<typename S, typename D> void read_array(istream &file, D *d, int n) {
		read_array_s<S>::f(file, d, n);
	}
}

using namespace iso;

ISO_browser2 GetProperties(const ISO_browser2 &b) {
	if (ISO_browser2 b2 = b["Properties70"])
		return b2;
	return b["Properties60"];
}

struct BFBXheader {
	char	magic[21];
	uint8	unknown[6];
	bool	valid()	const { return str(magic) == "Kaydara FBX Binary  "; }
};
struct BFBXentity {
	uint32le	next;
	uint32le	count;
	uint32le	size;
};

struct ValueReaderBase {
	enum TYPE {
		BFBX_NONE			= 0,

		BFBX_CHAR			= 'C',
		BFBX_INT16			= 'Y',
		BFBX_INT32			= 'I',
		BFBX_INT64			= 'L',
		BFBX_FLOAT32		= 'F',
		BFBX_FLOAT64		= 'D',

		BFBX_ARRAY_CHAR		= 'c',
		BFBX_ARRAY_INT16	= 'y',
		BFBX_ARRAY_INT32	= 'i',
		BFBX_ARRAY_INT64	= 'l',
		BFBX_ARRAY_FLOAT32	= 'f',
		BFBX_ARRAY_FLOAT64	= 'd',

		BFBX_STRING			= 'S',
		BFBX_RAW			= 'R',
	};
	istream		&file;
	ValueReaderBase(istream &_file) : file(_file) {}

	static bool		is_scalar(TYPE type)	{ return type && strchr("CYILFD", type); }
	static bool		is_array(TYPE type)		{ return type && strchr("cyilfd", type); }
	static bool		is_int(TYPE type)		{ return type && strchr("CYIL", type); }
	static bool		is_string(TYPE type)	{ return type == BFBX_STRING; }

	int64			read_int(TYPE type);
	double			read_float(TYPE type);
	string&			read_string(string &s);
	ISO_ptr<void>	read_iso(TYPE type, tag id);
	template<typename D, typename S> ISO_ptr<ISO_openarray<D> > read_iso_array(tag id);
};

int64 ValueReaderBase::read_int(TYPE type) {
	switch (type) {
		case 'C':	return file.get<char>();
		case 'Y':	return file.get<int16le>();
		case 'I':	return file.get<int32le>();
		case 'L':	return file.get<int64le>();
		case 'F':	return file.get<float32le>();
		case 'D':	return file.get<float64le>();
	}
	return 0;
}

double ValueReaderBase::read_float(TYPE type) {
	switch (type) {
		case 'C':	return file.get<char>();
		case 'Y':	return file.get<int16le>();
		case 'I':	return file.get<int32le>();
		case 'L':	return file.get<int64le>();
		case 'F':	return file.get<float32le>();
		case 'D':	return file.get<float64le>();
	}
	return 0;
}

string& ValueReaderBase::read_string(string &s) {
	if (size_t len = file.get<uint32le>()) {
		file.readbuff(s.alloc(len).begin(), len);
		s.begin()[len] = 0;
	}
	return s;
}

template<typename D, typename S> ISO_ptr<ISO_openarray<D> > ValueReaderBase::read_iso_array(tag id) {
	uint32	n	= file.get<uint32le>();
	uint32	m	= file.get<uint32le>();
	uint32	x	= file.get<uint32le>();

	ISO_ptr<ISO_openarray<D> >	p(id, n);
	switch (m) {
		case 0:	read_array<S>(file, p->begin(), n); break;
		case 1: read_array<S>(ZLIBistream(file).me(), p->begin(), n); break;
	}
	return p;
}

ISO_ptr<void> ValueReaderBase::read_iso(TYPE type, tag id) {
	switch (type) {
		case 'C':	return ISO_ptr<char>(id, file.get<char>());
		case 'Y':	return ISO_ptr<char>(id, file.get<int16le>());
		case 'I':	return ISO_ptr<int32>(id, file.get<int32le>());
		case 'L':	return ISO_ptr<int64>(id, file.get<int64le>());
		case 'F':	return ISO_ptr<float32>(id, file.get<float32le>());
		case 'D':	return ISO_ptr<float64>(id, file.get<float64le>());

		case 'c':	return read_iso_array<char, char>(id);
		case 'y': 	return read_iso_array<int16, int16le>(id);
		case 'i': 	return read_iso_array<int32, int32le>(id);
		case 'l': 	return read_iso_array<int64, int64le>(id);
		case 'f': 	return read_iso_array<float32, float32le>(id);
		case 'd':	return read_iso_array<float64, float64le>(id);

		case 'S': {
			ISO_ptr<string>	p(id);
			read_string(*p);
			return p;
		}
		case 'R': {
			uint32 len = file.get<uint32le>();
			ISO_ptr<ISO_openarray<uint8> >	p(id, len);
			file.readbuff(*p, len);
			return p;
		}
		default:
			return ISO_NULL;
	}
}

class ValueReader : public ValueReaderBase {
	TYPE		type;
	int			num;

	void	read_type() {
		type = num ? (TYPE)file.getc() : BFBX_NONE;
	}
	template<typename T> const T& next(const T &t) { --num; read_type(); return t; }

public:
	int				remaining()		const	{ return num; }

	bool			is_scalar()		const	{ return ValueReaderBase::is_scalar(type); }
	bool			is_array()		const	{ return ValueReaderBase::is_array(type); }
	bool			is_int()		const	{ return ValueReaderBase::is_int(type); }
	bool			is_string()		const	{ return ValueReaderBase::is_string(type); }

	int64			read_int()				{ return next(ValueReaderBase::read_int(type)); }
	double			read_float()			{ return next(ValueReaderBase::read_float(type)); }
	string			read_string()			{ string s; return next(ValueReaderBase::read_string(s)); }
	ISO_ptr<void>	read_iso(tag id = 0)	{ return next(ValueReaderBase::read_iso(type, id)); }

	void	discard() {
		if (type) {
			if (type == BFBX_STRING) {
				string	s;
				ValueReaderBase::read_string(s);
			} else {
				ValueReaderBase::read_int(type);
			}
			--num;
			read_type();
		}
	}
	void	discard_all() {
		while (num)
			discard();
	}
	ValueReader(istream &_file, int _num) : ValueReaderBase(_file), num(_num) {
		read_type();
	}
};

string fix_name(const char *name) {
	string	id;
	char	c = *name;

	if (c != '_' && !is_alpha(c))
		id = "_";

	for (string_builder b(id); c = *name; ++name) {
		if (is_alphanum(c)) {
			b << c;
		} else if (is_whitespace(c)) {
			b << '_';
			name = skip_whitespace(name) - 1;
		}
	}
	return id;
}

struct FBXBinaryReader {
	istream				&file;
	bool				raw;
	ISO_ptr<anything>	root;
	hash_map<uint32, ISO_ptr<anything> >	objects;
	hash_map<string, ISO_ptr<void> >		definitions;
	hash_map<string, ISO_type*>				types;

	static uint64	make_id(const char *s) {
		if (!s || str(s) == "Null")
			return 0;
		return iso::crc32(s);
	}

	ISO_ptr<anything>	ReadBlock(tag id, streamptr end);
	ISO_ptr<void>		ReadEntity(tag id, BFBXentity &ent);
	ISO_ptr<void>		ReadProperties(tag id, BFBXentity &ent);

	void				ReadDefinitions	(tag id, BFBXentity &ent);
	void				ReadObjects		(tag id, BFBXentity &ent);
	void				ReadConnections	(tag id, BFBXentity &ent);

	FBXBinaryReader(istream &_file, ISO_ptr<anything> _root, bool _raw = false) : file(_file), root(_root), raw(_raw) {
		objects[0] = root;
		objects[make_id("Scene")] = root;
		types["Compound"] = ISO_getdef<anything>();
	}
};

struct FBXAsciiReader {
	struct number {
		uint64	u;
		int16	exp;
		uint16	neg:1, flt:1;
		number(uint64 _u, int _exp, bool _neg, bool _flt) : u(_u), exp(_exp), neg(_neg), flt(_flt) {}
		operator int64()	const { return neg ? -int64(u) : int64(u); }
		operator double()	const { return operator int64() * pow(10.0, exp); }
	};

	text_reader<istream> file;
	bool				raw;

	int		skip_comments() {
		int		c;
		while ((c = file.skip_whitespace()) == ';') {
			while ((c = file.getc()) != EOF && c != '\n');
		}
		return c;
	}
	bool	TestEnd() {
		int	c = skip_comments();
		file.put_back(c);
		return c == EOF;
	}

	string	ReadName() {
		string	s;
		int		c = skip_comments();
		for (string_builder b(s); is_alphanum(c) || c == '_'; c = file.getc())
			b.putc(c);
		ISO_ASSERT(s.length() && c == ':');
		return s;
	}
	number			ReadNumber();
	ISO_ptr<void>	ReadValue();
	ISO_ptr<void>	ReadEntity(tag id);

	ISO_ptr<void> ReadDefinitions		(tag id)	{ return ReadEntity(id); }
	ISO_ptr<void> ReadObjects			(tag id)	{ return ReadEntity(id); }
	ISO_ptr<void> ReadConnections		(tag id)	{ return ReadEntity(id); }
	ISO_ptr<void> ReadProperties		(tag id)	{ return ReadEntity(id); }

	FBXAsciiReader(istream &_file, bool _raw = false) : file(_file), raw(_raw) {}
};

// Property types encountered
//ColorRGB, enum, Vector3D, Vector, int, 'Lcl Translation', ' Lcl Rotation', 'Lcl Scaling'
//KString, KTime, double, bool, ULongLong
//Compound. Number, DateTime,

ISO_ptr<void> FBXBinaryReader::ReadProperties(tag id, BFBXentity &_ent) {
	ISO_ptr<anything>	p(id);
	int					version = from_string<int>(id.substr(sizeof("Properties") - 1).begin());

	streamptr			end = _ent.next;
	BFBXentity			ent;
	while (file.tell() < end && file.read(ent) && ent.next) {
		streamptr	start	= file.tell();
		string		tok		= file.get();
		string		name;
		string		type;

		ValueReader	val(file, ent.count);

		if (val.is_string())
			name = val.read_string();

		if (val.is_string())
			type = val.read_string();

		val.discard();
		if (version >= 70)
			val.discard();

		ISO_ptr<anything>	p1 = p;
		ISO_ptr<void>		p2;

		if (name.find('|')) {
			string_scan	ss(name);
			for (;;) {
				auto t = ss.get_token(~char_set('|'));
				if (!ss.remaining()) {
					name	= t;
					break;
				}
				ss.move(1);
				ISO_ptr<anything>	p2 = (*p1)[t];
				if (!p2)
					p1->Append(p2.Create(t));
				p1 = p2;
			}
		}

		if (val.remaining() == 1) {
			p2 = val.read_iso(name);

		} else {
			anything	a;
			while (val.remaining())
				a.Append(val.read_iso());

			ISO_type *&t = types[type];
			if (!t) {
				p2	= AnythingToNamedStruct(type, a, name);
				t	= unconst(p2.Type());
			} else {
				p2	= MakePtr(t, name);
				ISO_browser		b(p2);
				for (int i = 0, n = a.Count(); i < n; ++i)
					b[i].Set(a[i]);
			}
		}
		p1->Append(p2);

		file.seek(ent.next);
	}
	return p;
}

void FBXBinaryReader::ReadDefinitions(tag id, BFBXentity &_ent) {
	streamptr			end = _ent.next;
	BFBXentity			ent;
	while (file.tell() < end && file.read(ent) && ent.next) {
		string	tok = file.get();
		if (tok == "ObjectType") {
			ValueReader	val(file, ent.count);

			string	type	= val.is_string() ? val.read_string() : 0;
			ISO_ptr<anything>	p2 = ReadBlock(tok, ent.next);
			definitions[type] = p2;
		}

		file.seek(ent.next);
	}
}

void FBXBinaryReader::ReadObjects(tag id, BFBXentity &_ent)	{
	streamptr			end		= _ent.next;
	BFBXentity			ent;
	while (file.tell() < end && file.read(ent) && ent.next) {
		streamptr	start	= file.tell();
		string		tok		= file.get();
		uint64		id		= 0;
		string		name;
		string		type;

		ValueReader	val(file, ent.count);

		bool		has_id = val.is_int();
		if (has_id)
			id = val.read_int();

		if (val.is_string())
			name = val.read_string();
		
		if (val.is_string())
			type = val.read_string();
		
		val.discard_all();

		ISO_ptr<anything>	p2 = ReadBlock(tok, ent.next);
		p2->Append(ISO_ptr<string>("TYPE", type));
		p2->Append(ISO_ptr<string>("NAME", name));

		if (!has_id)
			id = make_id(name);
		objects[id] = p2;

		file.seek(ent.next);
	}
}

void FBXBinaryReader::ReadConnections(tag id, BFBXentity &_ent) {
	streamptr			end = _ent.next;
	BFBXentity			ent;
	while (file.tell() < end && file.read(ent) && ent.next) {
		string	tok = file.get();	// always "C"
		string	type;
		string	to_prop, from_prop;
		uint64	to_id = 0, from_id = 0;

		ValueReader	val(file, ent.count);

		if (val.is_string())
			type = val.read_string();

		if (val.is_int())
			to_id = val.read_int();
		else if (val.is_string())
			to_id = make_id(val.read_string());

		if (val.is_int())
			from_id = val.read_int();
		else if (val.is_string())
			from_id = make_id(val.read_string());

		if (val.is_string())
			to_prop = val.read_string();

		if (val.is_string())
			from_prop = val.read_string();

		ISO_ptr<anything>	&to		= objects[to_id];
		ISO_ptr<anything>	&from	= objects[from_id];

		if (!to)
			ISO_TRACEF("Can't find object %i\n", to_id);
		if (!from)
			ISO_TRACEF("Can't find object %i\n", from_id);

		if (to && from) {
			if (type == "OO") {
				ISO_browser(from).Append().Set(to);
			} else if (type == "OP") {
				ISO_browser	b = GetProperties(from);
				string_scan	ss(to_prop);
				for (;;) {
					auto t = ss.get_token(~char_set('|'));
					if (!ss.remaining()) {
						b.SetMember(t, to);
						break;
					}
					ss.move(1);
					b	= b[t];
				}
				//ISO_browser(from)["Properties70"].SetMember(to_prop, to);
			} else if (type == "PO") {
			} else if (type == "PP") {
			}
		}
		file.seek(ent.next);
	}
}

ISO_ptr<void> FBXAsciiReader::ReadEntity(tag id) {
	ISO_ptr<anything>	p(id);

	int	c = skip_comments();

	if (c != '{' && c != '*') {
		file.put_back(c);
		do {
			p->Append(ReadValue());
		} while ((c = skip_comments()) == ',');
	}

	if (c == '*') {
		int count = 0;
		while (is_digit(c = file.getc()))
			count = count * 10 + c - '0';
		c = skip_comments();
		if (c == '{') {
			string	entry = ReadName();
			ISO_ASSERT(entry == "a");
			c = skip_comments();

			if (is_digit(c) || c == '-' || c == '+' || c == '.') {
				ISO_ptr<ISO_openarray<double> >	d(0, count);
				for (int i = 0; i < count; i++) {
					(*d)[i] = ReadNumber();
					c = skip_comments();
					ISO_ASSERT(c == i == count -1 ? '}' : ',');
				}
				p->Append(d);
			}

		}

	} else if (c == '{') {
		ISO_ptr<anything>	a(0);
		while ((c = skip_comments()) != '}') {
			file.put_back(c);
			string	tok = ReadName();
			if (!raw && tok.begins("Properties")) {
				p->Append(ReadProperties(tok));
			} else {
				p->Append(ReadEntity(tok));
			}
		}
	} else {
		file.put_back(c);
	}

	if (p->Count() == 1) {
		ISO_ptr<void>	p1 = (*p)[0];
		p1.SetID(id);
		return p1;
	}
	return p;
}

FBXAsciiReader::number FBXAsciiReader::ReadNumber() {
	int		c	= skip_comments();
	bool	neg	= c == '-';
	bool	flt	= false;

	uint64	i	= 0;
	int		e	= 0;

	if (c == '-' || c == '+')
		c = file.getc();

	while (is_digit(c)) {
		i = i * 10 + c - '0';
		c = file.getc();
	}
	if (c == '.') {
		flt = true;
		while (is_digit(c = file.getc())) {
			i = i * 10 + c - '0';
			e--;
		}
	}
	if (c == 'e' || c == 'E') {
		bool	nege = false;
		c = file.getc();
		if (c == '+' || (nege = (c == '-')))
			c = file.getc();

		int	d = 0;
		while (is_digit(c = file.getc()))
			d = d * 10 + c - '0';
		e	= nege ? e - d : e + d;
		flt	= true;
	}
	file.put_back(c);
	return number(i, e, neg, flt);
}

ISO_ptr<void> FBXAsciiReader::ReadValue() {
	int	c = skip_comments();
	if (c == '"') {
		string	name;
		for (string_builder b(name); (c = file.getc()) != '"'; )
			b.putc(c);
		return ISO_ptr<string>(0, name);

	} else if (is_digit(c) || c == '-' || c == '+' || c == '.') {
		number	n = ReadNumber();
		if (n.flt)
			return ISO_ptr<double>(0, n);
		int64	i = n;
		if (i == int32(i))
			return ISO_ptr<int>(0, i);
		return ISO_ptr<int64>(0, i);

	} else {
		string	name;
		for (string_builder b(name); is_alphanum(c) || c == '_'; c = file.getc())
			b.putc(c);
		file.put_back(c);
		return ISO_ptr<string>(0, name);
	}
}

template<typename D, typename S> ISO_ptr<ISO_openarray<D> > ReadArray(tag id, istream &file) {
	uint32	n	= file.get<uint32le>();
	uint32	m	= file.get<uint32le>();
	uint32	x	= file.get<uint32le>();

	ISO_ptr<ISO_openarray<D> >	p(id, n);
	switch (m) {
		case 0:	read_array<S>(file, p->begin(), n); break;
		case 1: read_array<D,S>(ZLIBistream(file).me(), p->begin(), n); break;
	}
	return p;
}

ISO_ptr<anything> FBXBinaryReader::ReadBlock(tag id, streamptr end) {
	ISO_ptr<anything>	p(id);
	BFBXentity			ent;
	while (file.tell() < end && file.read(ent) && ent.next) {
		string	tok = file.get();
		if (!raw && tok.begins("Properties")) {
			p->Append(ReadProperties(tok, ent));
		} else {
			p->Append(ReadEntity(tok, ent));
		}
		file.seek(ent.next);
	}
	file.seek(end);
	return p;
}

ISO_ptr<void> FBXBinaryReader::ReadEntity(tag id, BFBXentity &ent) {
	if (ent.count == 0)
		return ReadBlock(id, ent.next);

	bool	has_block = file.tell() + ent.size < ent.next;

	ValueReader	val(file, ent.count);
	if (val.remaining() == 1 && !has_block)
		return val.read_iso(id);

	ISO_ptr<anything>	p(id);
	while (val.remaining())
		p->Append(val.read_iso());

	if (has_block)
		p->Append(ReadBlock(0, ent.next));

	return p;
}

//-----------------------------------------------------------------------------
//	MATERIAL MAKER
//-----------------------------------------------------------------------------

class ColourSource;

struct ShaderWriter : string_builder {
	enum CONTEXT {
		CTX_GLOBALS,
		CTX_COLOUR,
		CTX_MONO,
		CTX_NORMAL,
	};
	enum MODE {
		MODE_OPEN,
		MODE_ARG,
		MODE_STMT,
	} mode;

	CONTEXT	context;
	int		tabs;

	ShaderWriter(string &s) : string_builder(s), tabs(0), mode(MODE_OPEN) {
		*this << "#include \"fbx.fxh\"\n\n";
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

	void	Write(ColourSource *cs, CONTEXT context);
};

class ShadeContext {
public:
	bool			filterMaps;
	ISO_ptr<void>	verts;
	float3p			p, dp;
	uint32			face[3];
};

class RefTarget {
};

class ColourSource : public RefTarget {
public:
	void Write(ShaderWriter& shader, ShaderWriter::CONTEXT context, param(colour) col) {
		switch (context) {
			case ShaderWriter::CTX_COLOUR:
				shader << "float4(" << col.r << ", " << col.g << ", " << col.b << ", " << col.a << ")";
				break;
			case ShaderWriter::CTX_MONO:
				shader << col.v;
				break;
		}
	}

	virtual colour	EvalColor(ShadeContext& sc)			=0;
	float	EvalMono(ShadeContext& sc) override { return EvalColor(sc).v;	}
	float3	EvalNormalPerturb(ShadeContext& sc) override { return float3(zero); }
	virtual void	WriteShader(ShaderWriter& shader, ShaderWriter::CONTEXT context)=0;
};

void ShaderWriter::Write(ColourSource *cs, CONTEXT _context) {
	cs->WriteShader(*this, context);
}

class XYZGen : public RefTarget {
public:
	virtual void	GetXYZ(ShadeContext& sc, float3& p, float3& dp) =0;
	float3x3 GetBumpDP(ShadeContext& sc) override { return identity; }; // returns 3 unit vectors for computing differentials
};

class TextureOutput: public RefTarget {
public:
	virtual colour	Filter(param(colour) c)	= 0;
	virtual float	Filter(float f)			= 0;
	virtual float3	Filter(param(float3) p)	= 0;
	virtual float	GetOutputLevel(float t) = 0;
};

struct FBXMaterialMaker {
	float				scale;
	bool				tangents;

	ISO_ptr<inline_technique>	technique;
	ISO_ptr<anything>			parameters;

	ColourSource *GetColourSource(const ISO_browser &b);
	ColourSource *GetColourSource(const ISO_browser &b, const char *id, param(colour) defval = colour(one));

	FBXMaterialMaker(const ISO_browser2 &settings) : parameters(0), tangents(false) {
		scale = settings["UnitScaleFactor"].Get(1.0) / 100;
	}
	void	Make(const ISO_browser2 &mat);
};


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

// Max Materials

/// Super-class IDs for Plug-ins
#define GEOMOBJECT_CLASS_ID				0x000010	//!< Geometric object super-class ID.
#define CAMERA_CLASS_ID					0x000020	//!< Camera object super-class ID.
#define LIGHT_CLASS_ID					0x000030	//!< Light object super-class ID.  
#define SHAPE_CLASS_ID					0x000040	//!< Shape object super-class ID.  
#define HELPER_CLASS_ID					0x000050	//!< Helper object super-class ID.  
#define SYSTEM_CLASS_ID	 				0x000060	//!< System plug-in super-class ID.   
#define REF_MAKER_CLASS_ID				0x000100	//!< ReferenceMaker super-class ID.   	
#define REF_TARGET_CLASS_ID	 			0x000200	//!< ReferenceTarget super-class ID.  
#define OSM_CLASS_ID					0x000810	//!< Object-space modifier (Modifier) super-class ID.  
#define WSM_CLASS_ID					0x000820	//!< World-space modifier (WSModifier) super-class ID.  
#define WSM_OBJECT_CLASS_ID				0x000830	//!< World-space modifier object (WSMObject) super-class ID.  
#define SCENE_IMPORT_CLASS_ID			0x000A10	//!< Scene importer (SceneImport) super-class ID.  
#define SCENE_EXPORT_CLASS_ID			0x000A20	//!< Scene exporter (SceneExport) super-class ID.  
#define BMM_STORAGE_CLASS_ID			0x000B10	//!< Bitmap storage super-class ID.  
#define BMM_FILTER_CLASS_ID				0x000B20	//!< Image filter super-class ID.  
#define BMM_IO_CLASS_ID					0x000B30	//!< Image loading/saving super-class ID.  
#define BMM_DITHER_CLASS_ID				0x000B40	//!< Bitmap dithering super-class ID.  
#define BMM_COLORCUT_CLASS_ID			0x000B50	//!< Bitmap color cut super-class ID.  
#define MATERIAL_CLASS_ID				0x000C00	//!< Materials super-class ID.
#define TEXMAP_CLASS_ID					0x000C10	//!< Texture maps super-class ID.
#define UVGEN_CLASS_ID					0x000C20	//!< UV Generator super-class ID.
#define XYZGEN_CLASS_ID					0x000C30	//!< XYZ Generator super-class ID.
#define TEXOUTPUT_CLASS_ID				0x000C40	//!< Texture output filter super-class ID.
#define SOUNDOBJ_CLASS_ID				0x000D00	//!< Sound object super-class ID.
#define FLT_CLASS_ID					0x000E00	//!< Image processing filter super-class ID. 
#define RENDERER_CLASS_ID				0x000F00	//!< Renderer super-class ID.
#define BEZFONT_LOADER_CLASS_ID			0x001000	//!< Bezier font loader super-class ID.
#define ATMOSPHERIC_CLASS_ID			0x001010	//!< Atmospheric effect super-class ID.
#define UTILITY_CLASS_ID				0x001020	//!< Utility object super-class ID.
#define TRACKVIEW_UTILITY_CLASS_ID		0x001030	//!< Trackview utility super-class ID.
#define MOT_CAP_DEV_CLASS_ID			0x001060	//!< Motion capture device super-class ID.
#define MOT_CAP_DEVBINDING_CLASS_ID		0x001050	//!< Motion capture device binding super-class ID.
#define OSNAP_CLASS_ID					0x001070	//!< Object snap super-class ID.
#define TEXMAP_CONTAINER_CLASS_ID		0x001080	
#define RENDER_EFFECT_CLASS_ID			0x001090	//!< Render post-effects super-class ID.
#define FILTER_KERNEL_CLASS_ID			0x0010a0	//!< Anti-aliasing filter kernel super-class ID.
#define SHADER_CLASS_ID					0x0010b0	//!< Standard material shader super-class ID.
#define COLPICK_CLASS_ID		  		0x0010c0	//!< Color picker super-class ID.
#define SHADOW_TYPE_CLASS_ID			0x0010d0	//!< Shadow generator super-class ID.
#define GUP_CLASS_ID		  			0x0010e0	//!< Global utility plug-in super-class ID.
#define SCHEMATICVIEW_UTILITY_CLASS_ID	0x001100	//!< Schematic view utility super-class ID.
#define SAMPLER_CLASS_ID				0x001110    //!< Sampler super-class ID.
#define IK_SOLVER_CLASS_ID				0x001140    //!< IK solver super-class ID.
#define RENDER_ELEMENT_CLASS_ID			0x001150	//!< Render output element super-class ID.
#define BAKE_ELEMENT_CLASS_ID			0x001151	//!< Texture bake output element super-class ID.
#define CUST_ATTRIB_CLASS_ID			0x001160    //!< Custom attributes super-class ID.
#define RADIOSITY_CLASS_ID				0x001170	//!< Global illumination plugin super-class ID.
#define TONE_OPERATOR_CLASS_ID			0x001180	//!< Tone operator super-class ID.
#define MPASS_CAM_EFFECT_CLASS_ID		0x001190	//!< Multi-pass camera effect super-class ID.
#define MR_SHADER_CLASS_ID_DEFUNCT		0x0011a0	//!< Mental ray shader super-class ID. \note No longer used, kept for file compatibility.
#define Fragment_CLASS_ID				0x0011b0	//!< Fragment super-class ID.

//	Subclass class IDs of MATERIAL_CLASS_ID
#define DMTL_CLASS_ID  					0x0000002	// StdMtl2: standard material.
#define DMTL2_CLASS_ID  				0x0000003	// Was used when migrating from StdMtl to StdMtl2.
#define CMTL_CLASS_ID 					0x0000100	// Top-bottom material 
#define MULTI_CLASS_ID 					0x0000200	// Super class ID multi material
#define DOUBLESIDED_CLASS_ID 			0x0000210	// Double-sided material
#define MIXMAT_CLASS_ID 				0x0000250	// Blend material
#define BAKE_SHELL_CLASS_ID 			0x0000255	// Two material shell for baking
#define MATTE_CLASS_ID 					0x0000260	// Matte material class ID.

//	Subclass class IDs of TEXMAP_CLASS_ID
#define CHECKER_CLASS_ID 				0x0000200
#define MARBLE_CLASS_ID 				0x0000210
#define MASK_CLASS_ID 					0x0000220  //!< Mask texture
#define MIX_CLASS_ID 					0x0000230
#define NOISE_CLASS_ID 					0x0000234
#define GRADIENT_CLASS_ID 				0x0000270
#define TINT_CLASS_ID 					0x0000224	//!< Tint texture
#define BMTEX_CLASS_ID 					0x0000240	//!< Bitmap texture
#define ACUBIC_CLASS_ID 				0x0000250	//!< Reflect/refract
#define MIRROR_CLASS_ID 				0x0000260	//!< Flat mirror
#define COMPOSITE_CLASS_ID 				0x0000280   //!< Composite texture
#define COMPOSITE_MATERIAL_CLASS_ID		Class_ID(0x61dc0cd7, 0x13640af6)  //!< Composite Material
#define RGBMULT_CLASS_ID 				0x0000290   //!< RGB Multiply texture
#define FALLOFF_CLASS_ID 				0x00002A0   //!< Falloff texture
#define OUTPUT_CLASS_ID 				0x00002B0   //!< Output texture
#define PLATET_CLASS_ID 				0x00002C0   //!< Plate glass texture
#define COLORCORRECTION_CLASS_ID		0x00002D0   //!< Color Correction texture

#include "utilities.h"

float threshold(float x, float a, float b) {
	return clamp((x - a) / (b - a), 0, 1);
}
float smooth_threshold(float x, float a, float b, float d) {
	return	lerp(smoothstep(a - d, a + d, x) * threshold(x, a, b), 1, smoothstep(b - d, b + d, x));
}

struct MAXColourSourceCreator : static_list<MAXColourSourceCreator> {
	Class_ID	c;
	MAXColourSourceCreator(Class_ID &_c) : c(_c) {}
	virtual ColourSource *Create(FBXMaterialMaker &maker, const ISO_browser &props)=0;
	static MAXColourSourceCreator *Find(const Class_ID &c, SClass_ID s) {
		for (auto i = begin(); i; ++i) {
			if (i->c == c)
				return i;
		}
		return 0;
	}
};

class SolidColour : public ColourSource {
	colour	col;
public:
	SolidColour(param(colour) _col)	: col(_col)	{}
	colour	EvalColor(ShadeContext& sc) override { return col; }
	float3	EvalNormalPerturb(ShadeContext& sc) override { return col.rgb; }
	
	void	WriteShader(ShaderWriter& shader, ShaderWriter::CONTEXT context) override {
		Write(shader, context, col);
	}
};


class SeparateTransparency : public ColourSource {
	ColourSource *rgb, *a;
public:
	SeparateTransparency(ColourSource *_rgb, ColourSource *_a)	: rgb(_rgb), a(_a)	{}
	colour	EvalColor(ShadeContext& sc) override { return colour(rgb->EvalColor(sc).rgb, a->EvalMono(sc)); }
	float	EvalMono(ShadeContext& sc) override { return rgb->EvalColor(sc).v; }
	float3	EvalNormalPerturb(ShadeContext& sc) override { return rgb->EvalColor(sc).rgb; }
	
	void	WriteShader(ShaderWriter& shader, ShaderWriter::CONTEXT context) override {
		switch (context) {
			case ShaderWriter::CTX_GLOBALS:
				rgb->WriteShader(shader, context);
				a->WriteShader(shader, context);
				break;
			case ShaderWriter::CTX_COLOUR:
				shader << "float4(";
				rgb->WriteShader(shader, context);
				shader << ".rgb, ";
				a->WriteShader(shader, ShaderWriter::CTX_MONO);
				shader << ")";
				break;
			case ShaderWriter::CTX_MONO:
				rgb->WriteShader(shader, ShaderWriter::CTX_MONO);
				break;
		}
	}
};

class ColourSourceMulS : public ColourSource {
	ColourSource *v, *s;
public:
	ColourSourceMulS(ColourSource *_v, ColourSource *_s)	: v(_v), s(_s)	{}
	colour	EvalColor(ShadeContext& sc) override { return v->EvalColor(sc) * s->EvalMono(sc); }
	float	EvalMono(ShadeContext& sc) override { return v->EvalMono(sc) * s->EvalMono(sc); }
	float3	EvalNormalPerturb(ShadeContext& sc) override { return v->EvalColor(sc).rgb * s->EvalMono(sc); }
	
	void	WriteShader(ShaderWriter& shader, ShaderWriter::CONTEXT context) override {
		switch (context) {
			case ShaderWriter::CTX_GLOBALS:
				v->WriteShader(shader, context);
				s->WriteShader(shader, context);
				break;
			case ShaderWriter::CTX_COLOUR:
			case ShaderWriter::CTX_MONO:
				v->WriteShader(shader, context);
				shader << " * ";
				s->WriteShader(shader, ShaderWriter::CTX_MONO);
				break;
		}
	}
};

class TextureMap : public ColourSource {
	string			name;
	int				uv_set;
public:
	TextureMap(const char *_name, int _uv_set) : name(_name), uv_set(_uv_set) {}
	colour	EvalColor(ShadeContext& sc) override {
		return colour(zero);
	}
	float	EvalMono(ShadeContext& sc) override {
		return 0;
	}
	float3	EvalNormalPerturb(ShadeContext& sc) override {
		return float3(zero);
	}
	void	WriteShader(ShaderWriter& shader, ShaderWriter::CONTEXT context) override {
	#if 1
		switch (context) {
			case ShaderWriter::CTX_GLOBALS:
				shader << "sampler_def2D(" << name << ",\n";
				shader << "\tFILTER_MIN_MAG_MIP_LINEAR;\n";
				shader << "\tADDRESSU = WRAP;\n";
				shader << "\tADDRESSV = WRAP;\n";
				shader << ");\n";
				break;
			case ShaderWriter::CTX_COLOUR:
				shader << "tex2D(" << name << ", uv" << uv_set << ")";
				break;
			case ShaderWriter::CTX_MONO:
				shader << "tex2D(" << name << ", uv" << uv_set << ").x";
				break;
		}
	#else
		switch (context) {
			case ShaderWriter::CTX_GLOBALS:
				shader << "Texture2D " << name << "_t;\n";
				shader << "SamplerState " << name << "_s;\n";
				break;
			case ShaderWriter::CTX_COLOUR:
				shader << name << "_t.Sample(" << name << "_s, uv" << uv_set << ")";
				break;
			case ShaderWriter::CTX_MONO:
				shader << name << "_t.Sample(" << name << "_s, uv" << uv_set << ").x";
				break;
		}
	#endif
	}
};

template<typename T, uint32 CRC> struct unit {
	T		t;
	void	operator=(T _t)	{ t = _t; }
	operator T() const		{ return t; }
};
template<typename T, uint32 CRC> struct vget<unit<T,CRC> >	: vget<T> {};

typedef unit<float, crc32_const("distance")> distance_unit;

template<typename T> void read_prop(FBXMaterialMaker &maker, ISO_browser b, const char *id, T &t) {
	t = b[id].Get(T());
}

void read_prop(FBXMaterialMaker &maker, ISO_browser b, const char *id, distance_unit &t) {
	t = b[id].GetFloat() * maker.scale;
}

void read_prop(FBXMaterialMaker &maker, ISO_browser b, const char *id, bool &t) {
	t = b[id].GetInt();
}

void read_prop(FBXMaterialMaker &maker, ISO_browser b, const char *id, colour &t) {
	if (double3 *p = b[id])
		t = colour(float3(*p));
	else
		t = colour(zero);
}

void read_prop(FBXMaterialMaker &maker, ISO_browser b, const char *id, ColourSource *&t) {
	t	= maker.GetColourSource(b, id);
}

void read_prop(FBXMaterialMaker &maker, ISO_browser b, const char *id, TextureOutput *&t) {
	t	= 0;
}

void read_prop(FBXMaterialMaker &maker, ISO_browser b, const char *id, XYZGen *&t) {
	t	= 0;
}

#define READ(x)	read_prop(maker, b, #x, x)

//-----------------------------------------------------------------------------
//	mix material
//-----------------------------------------------------------------------------

struct MixParams {
	float			mixAmount;
	float			lower;
	float			upper;
	bool			useCurve;
	colour			color1;
	colour			color2;
	ColourSource	*map1;
	ColourSource	*map2;
	ColourSource	*mask;
	bool			map1Enabled;
	bool			map2Enabled;
	bool			maskEnabled;
	TextureOutput	*output;

	MixParams(FBXMaterialMaker &maker, ISO_browser b) {
		READ(mixAmount);
		READ(lower);
		READ(upper);
		READ(useCurve);
		READ(color1);
		READ(color2);
		READ(map1);
		READ(map2);
		READ(mask);
		READ(map1Enabled);
		READ(map2Enabled);
		READ(maskEnabled);
		READ(output);
	}
};

class MixSource : public ColourSource, MixParams {
	float	GetMix(ShadeContext &sc) {
		float	mix = maskEnabled && mask ? mask->EvalMono(sc) : mixAmount;
		return useCurve ? smoothstep(lower, upper, mix) : mix;
	}
public:
	MixSource(const MixParams &params) : MixParams(params) {}

	colour EvalColor(ShadeContext& sc) {
		return output->Filter(lerp(
			map1Enabled && map1 ? map1->EvalColor(sc) : color1,
			map2Enabled && map2 ? map2->EvalColor(sc) : color2,
			GetMix(sc)
		));
	}
	 float  EvalMono(ShadeContext& sc) override {
		return output->Filter(lerp(
			map1Enabled && map1 ? map1->EvalMono(sc) : color1.v,
			map2Enabled && map2 ? map2->EvalMono(sc) : color2.v,
			GetMix(sc)
		));
	}
	void	WriteShader(ShaderWriter& shader, ShaderWriter::CONTEXT context) override {
		if (context == ShaderWriter::CTX_GLOBALS) {
			if (map1Enabled && map1)
				map1->WriteShader(shader, context);
			if (map2Enabled && map2)
				map2->WriteShader(shader, context);
			if (maskEnabled && mask)
				map2->WriteShader(shader, context);
		} else {
			shader.Open("lerp");
			if (map1Enabled && map1)
				map1->WriteShader(shader.Arg(), context);
			else
				Write(shader.Arg(), context, color1);

			if (map2Enabled && map2)
				map2->WriteShader(shader.Arg(), context);
			else
				Write(shader.Arg(), context, color2);
		
			if (useCurve)
				shader.Arg().Open("smoothstep").Arg() << lower << ", " << upper;

			if (maskEnabled && mask)
				map2->WriteShader(shader.Arg(), ShaderWriter::CTX_MONO);
			else
				shader.Arg() << mixAmount;

			if (useCurve)
				shader.Close();
			shader.Close();
		}
	}
};

struct MixCreator : MAXColourSourceCreator {
	MixCreator() :MAXColourSourceCreator(Class_ID(MIX_CLASS_ID, 0)) {}
	ColourSource *Create(FBXMaterialMaker &maker, const ISO_browser &props) override {
		MixParams	params(maker, props);
		return new MixSource(params);
	}
} mixcreator;

//-----------------------------------------------------------------------------
//	noise material
//-----------------------------------------------------------------------------
struct NoiseParams {
	enum TYPE {
		REGULAR,
		FRACTAL,
		TURB,
	};
	colour			color1;
	colour			color2;
	ColourSource	*map1;
	ColourSource	*map2;
	bool			map1Enabled;
	bool			map2Enabled;
	distance_unit	size;
	float			phase;
	float			levels;
	float			thresholdLow;
	float			thresholdHigh;
	int				type;
	XYZGen			*coords;
	TextureOutput	*output;

	NoiseParams(FBXMaterialMaker &maker, ISO_browser b) {
		READ(color1);
		READ(color2);
		READ(map1);
		READ(map2);
		READ(map1Enabled);
		READ(map2Enabled);
		READ(size);
		READ(phase);
		READ(levels);
		READ(thresholdLow);
		READ(thresholdHigh);
		READ(type);
		READ(coords);
		READ(output);
	}
};

class NoiseSource : public ColourSource, NoiseParams {
	static float avgAbsNs;

	perlin_noise	perlin;
	float			avgValue;
	XYZGen			*xyzGen;

	float	NoiseFunction(float3 p, float limitLev, float smWidth, bool filter);
	float	LimitLevel(float3 dp, float &smw, bool filter);
	void	ComputeAvgValue();

public:
	NoiseSource(const NoiseParams &params) : NoiseParams(params), perlin(1234) {}

	colour EvalColor(ShadeContext& sc) {
		float3 p, dp;
		xyzGen->GetXYZ(sc, p, dp);
		p		/= size;

		bool	filter	= sc.filterMaps;
		float	smw;
		float	limlev	= LimitLevel(dp, smw, filter);

		return output->Filter(lerp(
			map1Enabled && map1 ? map1->EvalColor(sc) : color1,
			map2Enabled && map2 ? map2->EvalColor(sc) : color2,
			NoiseFunction(p, limlev, smw, filter)
		));
	}

	float EvalMono(ShadeContext& sc) {
		float3 p, dp;
		xyzGen->GetXYZ(sc, p, dp);
		p		/= size;

		bool	filter	= sc.filterMaps;
		float	smw;
		float	limlev	= LimitLevel(dp, smw, filter);

		return output->Filter(lerp(
			map1Enabled && map1 ? map1->EvalMono(sc) : color1.v,
			map2Enabled && map2 ? map2->EvalMono(sc) : color2.v,
			NoiseFunction(p, limlev, smw, filter)
		));
	}

	float3 EvalNormalPerturb(ShadeContext& sc) {
		float3 p, dp;
		xyzGen->GetXYZ(sc, p, dp);
		p		/= size;

		bool	filter	= sc.filterMaps;
		float	smw;
		float	limlev	= LimitLevel(dp, smw, filter);
		float	d		= NoiseFunction(p, limlev, smw, filter);
		float	del		= .1f;

		float3x3 m		= xyzGen->GetBumpDP(sc);
		float3	np = (float3(
			NoiseFunction(p + del * m.x, limlev, smw, filter),
			NoiseFunction(p + del * m.y, limlev, smw, filter),
			NoiseFunction(p + del * m.z, limlev, smw, filter)
		) - d) / del;

	//	np = sc.VectorFromNoScale(np, REF_OBJECT);

		ColourSource *sub0 = map1Enabled ? map1 : NULL;
		ColourSource *sub1 = map2Enabled ? map2 : NULL;
		if (sub0 || sub1) {
			float	a	= sub0 ? sub0->EvalMono(sc) : color1.v;
			float	b	= sub1 ? sub1->EvalMono(sc) : color2.v;
			float3	da	= sub0 ? sub0->EvalNormalPerturb(sc) : float3(zero);
			float3	db	= sub1 ? sub1->EvalNormalPerturb(sc) : float3(zero);

			np = (b - a) * np + lerp(da, db, d);
		} else {
			np *= color2.v - color1.v;
		}
		return output->Filter(np);
	}

	void	WriteShader(ShaderWriter& shader, ShaderWriter::CONTEXT context) override {
		if (context == ShaderWriter::CTX_GLOBALS) {
			if (map1Enabled && map1)
				map1->WriteShader(shader, context);
			if (map2Enabled && map2)
				map2->WriteShader(shader, context);
		} else {
			shader.Open("lerp");
			if (map1Enabled && map1)
				map1->WriteShader(shader.Arg(), context);
			else
				Write(shader.Arg(), context, color1);

			if (map2Enabled && map2)
				map2->WriteShader(shader.Arg(), context);
			else
				Write(shader.Arg(), context, color2);
		
			if (thresholdLow < thresholdHigh)
				shader.Arg().Open("smooth_threshold");

			static const char *fns[] = {
				"noise_regular",
				"noise_fractal",
				"noise_turb",
			};
			shader.Arg() << fns[type] << "(float4(worldpos, " << phase << ") / " << size << ", " << levels << ")";

			if (thresholdLow < thresholdHigh) {
				shader.Arg() << thresholdLow << ", " << thresholdHigh << ", smooth_width(worldpos)";
				shader.Close();
			}
			shader.Close();
		}
	}
};

#define BLENDBAND 4.0f  // blend to average value over this many orders of magnitude
float NoiseSource::avgAbsNs = -1.0f;

#define NAVG	10000
#define NAVGNS	10000
void NoiseSource::ComputeAvgValue() {
	Random	r(1345);
	float sum	= 0;
	for (int i = 0; i < NAVG; i++)
		sum += NoiseFunction(float3(r.to(0.01f), r.to(0.01f), r.to(0.01f)), levels, 0, false);
	avgValue = sum / float(NAVG);
	if (avgAbsNs < 0) {
		sum		= 0;
		for (int i = 0; i < NAVGNS; i++)
			sum += abs(perlin.noise(float4(r.to(0.01f), r.to(0.01f), r.to(0.01f), r.to(0.01f))));
		avgAbsNs = sum / float(NAVGNS);
	}
}

float NoiseSource::NoiseFunction(float3 p, float limitLev, float smWidth, bool filter) {
	if (limitLev < 1 - BLENDBAND)
		return avgValue;

	float res	= 0;
	float lev	= max(limitLev, 1);

	switch (type) {
		case REGULAR:
			res = (perlin.noise(float4(p, phase)) + 1) / 2;
			break;

		case FRACTAL: {
			float sum = 0;
			float l, f = 1;
			for (l = lev; l >= 1; l -= 1) {
				sum += perlin.noise(float4(p * f, phase)) / f;
				f *= 2;
			}
			if (l > 0)
				sum += l * perlin.noise(float4(p * f, phase)) / f;
			res = (sum + 1) / 2;
			break;
		}
		case TURB: {
			float l, f	= 1;
			float ml	= levels;
			for (l = lev; l >= 1; l -= 1, ml -= 1) {
				res += abs(perlin.noise(float4(p * f, phase))) / f;
				f	*= 2;
			}
			if (l > 0)
				res += l * abs(perlin.noise(float4(p * f, phase))) / f;

			if (filter && ml > l) {
				float r = 0;
				if (ml < 1) {
					r += (ml - l) / f;
				} else {
					r	+= (1 - l) / f;
					ml	-= 1;
					f	*= 2;
					for (l = ml; l >= 1; l -= 1) {
						r += 1 / f;
						f *= 2;
					}
					if (l > 0)
						r += l / f;
					res += r * avgAbsNs;
				}
			}
			break;
		}
	}
	if (thresholdLow < thresholdHigh)
		res = smooth_threshold(res, thresholdLow, thresholdHigh, smWidth);

	res = clamp(res, 0, 1);
	return filter && limitLev < 1 ? lerp(avgValue, res, (limitLev + BLENDBAND - 1) / BLENDBAND) : res;
}

float NoiseSource::LimitLevel(float3 dp, float &smw, bool filter) {
	if (filter) {
		float m = max(((abs(dp.x) + abs(dp.y) + abs(dp.z)) / 3) / size, 0.00001f);
		smw		= min(m * .2f, .4f);
		return min(levels, log2(1 / m));
	} else {
		smw		= 0;
		return levels;
	}
}

struct NoiseCreator : MAXColourSourceCreator {
	NoiseCreator() :MAXColourSourceCreator(Class_ID(NOISE_CLASS_ID, 0)) {}
	ColourSource *Create(FBXMaterialMaker &maker, const ISO_browser &props) override {
		NoiseParams	params(maker, props);
		return new NoiseSource(params);
	}
} noisecreator;


//-----------------------------------------------------------------------------
//	Make Material
//-----------------------------------------------------------------------------

ColourSource *FBXMaterialMaker::GetColourSource(const ISO_browser &b) {
	if (b.Is<double>()) {
		double	*p = b;
		float	f	= float(*p);
		return new SolidColour(colour(f,f,f,f));
	}
	if (b.Is("ColorRGB") || b.Is("Color")) {
		double3	*p = b;
		return new SolidColour(colour(float3(*p), one));
	}

	const char *type = b["Type"].GetString();
	if (str(type) == "TextureVideoClip") {
		const char *rel = b["RelativeFilename"].GetString();
		filename	fn	= FileHandler::FindAbsolute(rel);
		if (!fn)
			fn = rel;
		ISO_browser	props = GetProperties(b);

		if (ISO_browser props2 = props["3dsMax"]) {
			int ClassIDa		= props2["ClassIDa"].GetInt();
			int ClassIDb		= props2["ClassIDb"].GetInt();
			int SuperClassID	= props2["SuperClassID"].GetInt();

			if (MAXColourSourceCreator *creator = MAXColourSourceCreator::Find(Class_ID(ClassIDa, ClassIDb), SuperClassID))
				return creator->Create(*this, props2["parameters"]);
		}

		const char *UVSet = props["UVSet"].GetString();//"UVChannel_1"

		string		name		= fix_name(b["TextureName"].GetString());
		ISO_ptr<void>	p		= MakePtr(ISO_getdef<Texture>(), name);
		*(ISO_ptr<void>*)p		= MakePtrExternal(ISO_getdef<bitmap>(), fn);
		parameters->Append(p);
		return new TextureMap(name, 0);
	}
	return 0;
}

ColourSource *FBXMaterialMaker::GetColourSource(const ISO_browser &b0, const char *id, param(colour) defval) {
	ISO_browser b = b0[id];
	if (!b)
		return new SolidColour(defval);

	return GetColourSource(b);
}

void FBXMaterialMaker::Make(const ISO_browser2 &mat) {
	if (!mat)
		return;

	const char *type	= mat["ShadingModel"].GetString();
	ISO_browser	props	= GetProperties(mat);

	technique.Create(0);
	ISO_ptr<inline_pass>	pass(type);
	technique->Append(pass);

	string			ps;
	ShaderWriter	psb(ps);

	ColourSource	*AmbientColor		= GetColourSource(props, "AmbientColor");
	ColourSource	*DiffuseColor		= GetColourSource(props, "DiffuseColor");

	if (ISO_browser b = props["TransparencyFactor"])
		DiffuseColor = new SeparateTransparency(DiffuseColor, GetColourSource(b));

	AmbientColor		->WriteShader(psb, ShaderWriter::CTX_GLOBALS);
	DiffuseColor		->WriteShader(psb, ShaderWriter::CTX_GLOBALS);

	if (str(type) == "lambert") {
		if (ISO_browser b = props["DiffuseFactor"])
			DiffuseColor	= new ColourSourceMulS(DiffuseColor, GetColourSource(b));

		psb.Open("float4 main(float4 screenpos : POSITION_OUT, float3 worldpos : POSITION, float3 normal : NORMAL, float2 uv0 : TEXCOORD0) : OUTPUT0 ", '{');
		psb.Arg().Open("return lambert");
		psb.Arg() << "worldpos, normal";
		
		AmbientColor		->WriteShader(psb.Arg(), ShaderWriter::CTX_COLOUR);
		DiffuseColor		->WriteShader(psb.Arg(), ShaderWriter::CTX_COLOUR);

		psb.Close(ShaderWriter::MODE_STMT);
		psb.Close();

	} else {//if (str(type) == "phong") {
		ColourSource	*SpecularColor		= GetColourSource(props, "SpecularColor");
		if (ISO_browser b = props["SpecularFactor"])
			SpecularColor	= new ColourSourceMulS(SpecularColor, GetColourSource(b));
		ColourSource	*ShininessExponent	= GetColourSource(props, "ShininessExponent");
		//ColourSource	*ReflectionFactor	= GetColourSource(props, "ReflectionFactor");

		ColourSource	*NormalMap	= 0;
		bool			bumpmap		= false;
		if (ISO_browser b = props["NormalMap"]) {
			NormalMap	= GetColourSource(b);
			tangents	= true;
		} else if (ISO_browser b = props["Bump"]) {
			NormalMap = GetColourSource(b);
			if (b = props["BumpFactor"])
				NormalMap = new ColourSourceMulS(NormalMap, GetColourSource(b));
			bumpmap		= true;
		}

		SpecularColor	->WriteShader(psb, ShaderWriter::CTX_GLOBALS);
		ShininessExponent->WriteShader(psb, ShaderWriter::CTX_GLOBALS);
		if (NormalMap)
			NormalMap->WriteShader(psb, ShaderWriter::CTX_GLOBALS);

		psb.Open("float4 main(float4 screenpos : POSITION_OUT, float3 worldpos : POSITION, float3 normal : NORMAL, float2 uv0 : TEXCOORD0) : OUTPUT0 ", '{');
		psb.Arg().Open("return phong");

		psb.Arg() << "worldpos";
		if (NormalMap) {
			if (bumpmap) {
				psb.Arg().Open("GetBump");
				psb.Arg() << "worldpos, normal";
				NormalMap->WriteShader(psb.Arg(), ShaderWriter::CTX_MONO);
				psb.Close();
			} else {
				psb.Arg().Open("GetNormal");
				NormalMap->WriteShader(psb.Arg(), ShaderWriter::CTX_COLOUR);
				psb.Arg() << "normal, tangent";
				psb.Close();
			}
		} else {
			psb.Arg() << "normal";
		}

		AmbientColor		->WriteShader(psb.Arg(), ShaderWriter::CTX_COLOUR);
		DiffuseColor		->WriteShader(psb.Arg(), ShaderWriter::CTX_COLOUR);
		SpecularColor		->WriteShader(psb.Arg(), ShaderWriter::CTX_COLOUR);
		ShininessExponent	->WriteShader(psb.Arg(), ShaderWriter::CTX_MONO);

		psb.Close(ShaderWriter::MODE_STMT);
		psb.Close();
	}

	psb.flush();
	pass->Append(MakePtr("PS", ps));

	string			vs;
	ShaderWriter	vsb(vs);

	vsb.Open("float4 main");
	vsb.Arg() << "float3 position : POSITION, float3 normal : NORMAL, float2 uv0 : TEXCOORD0";
	vsb.Arg() << "out float3 out_position : POSITION, out float3 out_normal : NORMAL, out float2 out_uv0 : TEXCOORD0";
	vsb.Close();
	vsb.Open(" : POSITION_OUT ", '{');
	vsb.Stmt() << "out_position	= mul(float4(position, 1), (float4x3)world)";
	vsb.Stmt() << "out_normal	= normalize(mul(normal, (float3x3)world))";
	vsb.Stmt() << "out_uv0		= uv0";
	vsb.Stmt() << "return mul(float4(out_position, 1.0), ViewProj())";
	vsb.Close();

	vsb.flush();
	pass->Append(MakePtr("VS", vs));
}
//-----------------------------------------------------------------------------
//	MODEL MAKER
//-----------------------------------------------------------------------------

struct FBXindex {
	int	i;
	operator uint32()	const { return uint32(i < 0 ? ~i : i); }
	bool	is_end()	const { return i < 0; }
};

struct FBXmatfilter {
	range<FBXindex*>	vert_indices;
	int					*mat_index;
	int					mat_filter;

	struct iterator {
		FBXindex	*v;
		int			*mat_index;
		int			mat_filter;

		iterator(FBXindex *_v, int *_mat_index, int _mat_filter) : v(_v), mat_index(_mat_index), mat_filter(_mat_filter) {}

		iterator&	operator++() {
			if (v->is_end())
				++mat_index;
			return *this;
		}
		friend bool	operator==(const iterator &a, const iterator &b) { return a.v == b.v; }
		friend bool	operator!=(const iterator &a, const iterator &b) { return a.v != b.v; }
	};

	FBXmatfilter(range<FBXindex*> _vert_indices, int *_mat_index, int _mat_filter)
		: vert_indices(_vert_indices), mat_index(_mat_index), mat_filter(_mat_filter) {}

	iterator	begin() { return iterator(vert_indices.begin(), mat_index, mat_filter); }
	iterator	end()	{ return iterator(vert_indices.end(), mat_index, mat_filter); }
};

template<typename I> struct FBXmatfilterT : FBXmatfilter {
	I			i;
	typedef typename iterator_traits<I>::element element;
	FBXmatfilterT(range<FBXindex*> _vert_indices, int *_mat_index, int _mat_filter, I _i)
		: FBXmatfilter(_vert_indices, _mat_index, _mat_filter), i(_i) {}

	struct iterator : FBXmatfilter::iterator {
		I	i;
		iterator(FBXindex *_v, int *_mat_index, int _mat_filter, I _i) : FBXmatfilter::iterator(_v, _mat_index, _mat_filter), i(_i) {}
		element&		operator*()	{ return *i; }
		friend iterator	operator+(const iterator &a, size_t n)		{ return iterator(a.vert_indices + n, mat_index, mat_filter, i); }
	};
	iterator	begin() const { return iterator(vert_indices.begin(), mat_index, mat_filter, attr_index, t); }
	iterator	end()	const { return iterator(vert_indices.end(), mat_index, mat_filter, attr_index, t); }
};

template<typename I> struct container_traits<FBXmatfilterT<I> > : iterator_traits<I> {};

template<typename I> FBXmatfilterT<I> make_FBXmatfilter(range<FBXindex*> vert_indices, int *mat_index, int mat_filter, I i) {
	return FBXmatfilterT<I>(vert_indices, mat_index, mat_filter, i);
}

struct FBXComponent {
	enum MAP {
		None			= 0,
		AllSame			= 1,
		ByPolygonVertex	= 2,
		ByVertice		= 3,
		ByPolygon		= 4,
	};
	void			*data;
	int				*indices;
	MAP				mapping;
	int				ncomps;

	FBXComponent() : data(0), indices(0) {}

	template<typename T> void _Index(T *vals, Indexer<uint32> &indexer, const FBXindex *vert_indices, int *poly_indices) {
		switch (indices ? mapping : None) {
			case None:				indexer.Process(vals,										equal_vec()); break;
			case AllSame:			indexer.Process(make_value_iterator(*vals),					equal_vec()); break;
			case ByPolygonVertex:	indexer.Process(make_indexed_iterator(vals, indices),		equal_vec()); break;
			case ByVertice:			indexer.Process(make_indexed_iterator(vals, vert_indices),	equal_vec()); break;
			case ByPolygon:			indexer.Process(make_indexed_iterator(vals, poly_indices),	equal_vec()); break;
		}
	}
	template<typename T> void _Index(T *vals, Indexer<uint32> &indexer, const int *mat_filter, const FBXindex *vert_indices, int *poly_indices) {
		switch (indices ? mapping : None) {
			case None:				indexer.Process(make_indexed_iterator(vals,										mat_filter), equal_vec()); break;
			case AllSame:			indexer.Process(make_indexed_iterator(make_value_iterator(*vals),				mat_filter), equal_vec()); break;
			case ByPolygonVertex:	indexer.Process(make_indexed_iterator(make_indexed_iterator(vals, indices),		mat_filter), equal_vec()); break;
			case ByVertice:			indexer.Process(make_indexed_iterator(make_indexed_iterator(vals, vert_indices),mat_filter), equal_vec()); break;
			case ByPolygon:			indexer.Process(make_indexed_iterator(make_indexed_iterator(vals, poly_indices),mat_filter), equal_vec()); break;
		}
	}
	template<typename D, typename T> void _Write(D dest, T *vals, const Indexer<uint32> &indexer, const FBXindex *vert_indices, int *poly_indices) {
		switch (indices ? mapping : None) {
			case None:				copy(make_indexed_array(vals,										indexer.RevIndices()), dest); break;
			case AllSame:			copy(make_indexed_array(make_value_iterator(*vals),					indexer.RevIndices()), dest); break;
			case ByPolygonVertex:	copy(make_indexed_array(make_indexed_iterator(vals, indices),		indexer.RevIndices()), dest); break;
			case ByVertice:			copy(make_indexed_array(make_indexed_iterator(vals, vert_indices),	indexer.RevIndices()), dest); break;
			case ByPolygon:			copy(make_indexed_array(make_indexed_iterator(vals, poly_indices),	indexer.RevIndices()), dest); break;
		}
	}

	void		Index(Indexer<uint32> &indexer, const FBXindex *vert_indices, int *poly_indices) {
		switch (ncomps) {
			case 2: _Index((double2*)data, indexer, vert_indices, poly_indices); break;
			case 3: _Index((double3*)data, indexer, vert_indices, poly_indices); break;
		}
	}
	void		Index(Indexer<uint32> &indexer, const int *mat_filter, const FBXindex *vert_indices, int *poly_indices) {
		switch (ncomps) {
			case 2: _Index((double2*)data, indexer, mat_filter, vert_indices, poly_indices); break;
			case 3: _Index((double3*)data, indexer, mat_filter, vert_indices, poly_indices); break;
		}
	}
	void		Write(stride_iterator<void> dest, const Indexer<uint32> &indexer, const FBXindex *vert_indices, int *poly_indices) {
		switch (ncomps) {
			case 2: _Write(stride_iterator<float2p>(dest), (double2*)data, indexer, vert_indices, poly_indices); break;
			case 3: _Write(stride_iterator<float3p>(dest), (double3*)data, indexer, vert_indices, poly_indices); break;
		}
	}
};

void FBXAddSubMesh(const Indexer<uint32> &indexer, ModelBuilder &mb, const ISO_type *vert_type, const dynamic_array<FBXComponent> &components, const FBXindex *vert_indices, int *poly_indices) {
	if (indexer.NumUnique() < 65536) {
		int	nt	= 0;
		for (const FBXindex *i = vert_indices, *p = i, *e = i + indexer.NumIndices(); i < e; ++i) {
			if (i->is_end()) {
				nt	+= i - p - 2;
				p	= i;
			}
		}

		SubMesh			*mesh = mb.AddMesh(vert_type, indexer.NumUnique(), nt);
		for (int i = 0; i < components.size(); i++)
			components[i].Write(GetVertexComponent(mesh->verts, i), indexer, vert_indices, poly_indices);

		fixed_array<uint16,3>	*d = mesh->indices.begin();
		for (const FBXindex *i0 = vert_indices, *i = i0, *e = i + indexer.NumIndices(); i < e; ) {
			int		a = i - i0;
			while (!i++->is_end());
			int		b = i - i0 - 1;

			while (a + 1 < b) {
				(*d)[0] = indexer.Index(a);
				(*d)[1] = indexer.Index(++a);
				(*d)[2] = indexer.Index(b);
				d++;

				if (a + 1 == b)
					break;

				(*d)[0] = indexer.Index(b);
				(*d)[1] = indexer.Index(a);
				(*d)[2] = indexer.Index(--b);
				d++;
			}
		}
	} else {
		mb.InitVerts(vert_type, indexer.NumUnique());
		malloc_block	verts(mb.vert_size * indexer.NumUnique());
		for (int i = 0; i < components.size(); i++)
			components[i].Write(mb.GetVertexComponent(verts, i), indexer, vert_indices, poly_indices);

		for (const FBXindex *i0 = vert_indices, *i = i0, *e = i + indexer.NumIndices(); i < e; ) {
			int		a = i - i0;
			while (!i++->is_end());
			int		b = i - i0 - 1;

			int	v0 = indexer.Index(a);
			int	v1 = indexer.Index(b);
			int	vb0, vb1;

			if ((vb0 = mb.TryAdd(v0)) < 0 || (vb1 = mb.TryAdd(v1)) < 0) {
				mb.Purge(verts);
				vb0 = mb.TryAdd(v0);
				vb1 = mb.TryAdd(v1);
			}

			while (a + 1 < b) {
				int	v2 = indexer.Index(++a), vb2;
				if ((vb2 = mb.TryAdd(v2)) < 0) {
					mb.Purge(verts);
					vb0 = mb.TryAdd(v0);
					vb1 = mb.TryAdd(v1);
					vb2 = mb.TryAdd(v2);
				}
				mb.AddFace(vb0, vb2, vb1);

				if (a + 1 == b)
					break;

				v0 = indexer.Index(--b);
				if ((vb0 = mb.TryAdd(v0)) < 0) {
					mb.Purge(verts);
					vb0 = mb.TryAdd(v0);
					vb1 = mb.TryAdd(v1);
					vb2 = mb.TryAdd(v2);
				}
				mb.AddFace(vb1, vb2, vb0);

				v1 = v0; vb1 = vb0;
				v0 = v2; vb0 = vb2;
			}
		}
	}
}

ISO_ptr<Model3> FBXMakeModel(const ISO_browser2 &b, const char *type, const FBXMaterialMaker &mat, const ISO_browser2 &settings) {
	double scale = settings["UnitScaleFactor"].Get(1.0) / 100;

	if (str(type) == "Line") {
		if (ISO_browser b1 = b["Points"]) {
			ISO_openarray<double>	v		= *ISO_conversion::convert<ISO_openarray<double> >(b1);
			int						nv		= v.Count() / 3;
			double3					*verts	= (double3*)v.begin();
			for (auto &i : v)
				i *= scale;

			ISO_browser	b2 = b["PointsIndex"];
			int						ni		= b2.Count() / 3;
			fixed_array<int,3>*		indices	= b2[0];

			TISO_type_composite<64>	builder;
			builder.Add<float[3]>("position");

			ModelBuilder	mb(b.GetName(), ISO_NULL);
			SubMesh			*mesh = mb.AddMesh(builder.Duplicate(), nv, ni);
			stride_iterator<float3p> pos(GetVertexComponent(mesh->verts, "position"));
			copyn(verts, pos, nv);
			copyn(indices, mesh->indices.begin(), ni);
			mb.CalcExtents();
			return mb;
		}

	} else if (str(type) == "Mesh") {
		if (ISO_browser b1 = b["Vertices"]) {
			ISO_browser bi	= b["PolygonVertexIndex"];
			int			nv	= b1.Count() / 3;
			int			ni	= bi.Count();

			dynamic_array<FBXComponent> components;
			FBXComponent	mats;
			FBXComponent	*comp	= new(components) FBXComponent;

			ISO_openarray<double>	v = *ISO_conversion::convert<ISO_openarray<double> >(b1);
			for (auto &i : v)
				i *= scale;

			comp->data		= v;
			comp->indices	= *ISO_conversion::convert<ISO_openarray<int> >(b["PolygonVertexIndex"]);
			comp->ncomps	= 3;
			comp->mapping	= FBXComponent::ByVertice;

			TISO_type_composite<64>	builder;
			builder.Add<float[3]>("position");

			int	inorms	= -1;
			int	iuvs	= -1;
			int layer	= -1;
			while ((layer = b.GetIndex("Layer", layer + 1)) >= 0) {
				ISO_browser b2 = b[layer][1];

//				for (ISO_browser::iterator i = b2.begin(), e = b2.end(); i != e; ++i) {
				for (auto &i : b2) {
					if (i.GetName() == "LayerElement") {
						const char	*type = i["Type"].GetString();
						int			index = i["TypedIndex"].GetInt();

						int			element = -1;
						do
							element = b.GetIndex(type, element + 1);
						while (index--);

						if (b2 = b[element][1]) {
							const char *name			= b2["Name"].GetString();
							const char *map_type		= b2["MappingInformationType"].GetString();	
							const char *ref_type		= b2["ReferenceInformationType"].GetString();	// Direct or IndexToDirect
							bool		indexed			= str(ref_type) == "IndexToDirect";
							FBXComponent::MAP mapping	= str(map_type) == "AllSame"			? FBXComponent::AllSame
														: str(map_type) == "ByPolygonVertex"	? FBXComponent::ByPolygonVertex
														: str(map_type) == "ByVertice"			? FBXComponent::ByVertice 
														: str(map_type) == "ByPolygon"			? FBXComponent::ByPolygon
														: FBXComponent::None;

							if (str(type) == "LayerElementNormal") {
								FBXComponent	*c = new(components) FBXComponent;
								builder.Add<float[3]>(name ? name : "normal");
								//c->data	= b2["Normals"];
								c->data		= *ISO_conversion::convert<ISO_openarray<double> >(b2["Normals"]);
								c->ncomps	= 3;
								c->mapping	= mapping;

							} else if (str(type) == "LayerElementUV" || str(type) == "LayerElementBumpUV") {
								FBXComponent	*c = new(components) FBXComponent;
								builder.Add<float[2]>(name ? name : "texcoord0");
								ISO_ptr<ISO_openarray<double> >	uv = ISO_conversion::convert<ISO_openarray<double> >(b2["UV"]);
								for (double *d = uv->begin(), *de = uv->end(); d != de; d += 2)
									d[1] = 1 - d[1];

								c->data		= *uv;
								if (indexed)
									c->indices = *ISO_conversion::convert<ISO_openarray<int> >(b2["UVIndex"]);
								c->ncomps	= 2;
								c->mapping	= mapping;

							} else if (str(type) == "LayerElementMaterial" && mapping != FBXComponent::AllSame) {
								mats.data		= *ISO_conversion::convert<ISO_openarray<int> >(b2["Materials"]);
								mats.ncomps		= 1;
								mats.mapping	= mapping;

							} else if (str(type) == "LayerElementTextures") {
							} else if (str(type) == "LayerElementTransparentTextures") {
							} else if (str(type) == "LayerElementSpecularFactorTextures") {
							} else if (str(type) == "LayerElementBumpTextures") {
							} else if (str(type) == "LayerElementReflectionTextures") {
							} else {
								continue;
							}

						}
					}
				}
			}
			
			if (mat.tangents)
				builder.Add<float[4]>("tangent");

			ModelBuilder		mb(b.GetName(), (ISO_ptr<void>&)mat.technique, mat.parameters);// iso::root["data"]["simple"]["lite"]);
			const ISO_type		*vert_type		= builder.Duplicate();
			const FBXindex		*vert_indices	= (FBXindex*)components[0].indices;
			int					*poly_indices	= new int[ni];

			int		num_tris	= 0;
			int		num_polys	= 0;
			int		*pi			= poly_indices;
			for (const FBXindex *i = vert_indices, *p = i, *e = i + ni; i < e; ++i) {
				*pi++ = num_polys;
				if (i->is_end()) {
					++num_polys;
					num_tris	+= i - p - 2;
					p			= i;
				}
			}

			if (mats.data) {
				int		*mat_filter = new int[ni];

				for (int m = 0; ; ++m) {
					int		*mi = (int*)mats.data;
					int		*di = mat_filter;

					for (int *i0 = components[0].indices, *i = i0, *e = i + ni; i < e; ) {
						if (*mi == m) {
							do
								*di++ = i - i0;
							while (*i++ >= 0);
						} else {
							while (*i++ >= 0);
						}
						++mi;
					}

					int	nm = di - mat_filter;
					if (nm == 0)
						break;

					Indexer<uint32>	indexer(nm);
					for (int i = 0; i < components.size(); i++)
						components[i].Index(indexer, mat_filter, vert_indices, poly_indices);

					FBXAddSubMesh(indexer, mb, builder.Duplicate(), components, vert_indices, poly_indices);
				}
				delete[] mat_filter;

			} else {
				Indexer<uint32>		indexer(ni);
				indexer.SetIndices(vert_indices, nv);
				for (int i = 1; i < components.size(); i++)
					components[i].Index(indexer, vert_indices, poly_indices);

				FBXAddSubMesh(indexer, mb, vert_type, components, vert_indices, poly_indices);
			}

			delete[] poly_indices;

			if (mat.tangents) {
				for (auto &i : mb->submeshes)
					GenerateTangents((SubMesh*)i);
			}

			mb.CalcExtents();
			return mb;
		}

	} else if (str(type) == "Limb") {
		//joint
	}

	return ISO_NULL;
}

//-----------------------------------------------------------------------------
//	FBXFileHandler
//-----------------------------------------------------------------------------
// GlobalSettings/Properties70
/*
	int UpAxis = 1
	int UpAxisSign = 1
	int FrontAxis = 2
	int FrontAxisSign = 1
	int CoordAxis = 0
	int CoordAxisSign = 1
	int OriginalUpAxis = 2
	int OriginalUpAxisSign = 1
	float{64.11} UnitScaleFactor = 2.54
	float{64.11} OriginalUnitScaleFactor = 2.54
	ColorRGB AmbientColor = {0 0 0}
	string DefaultCamera = "Producer Perspective"
	int TimeMode = 6
	int TimeProtocol = 2
	int SnapOnFrameMode = 0
	int{64} TimeSpanStart = 0
	int{64} TimeSpanStop = 153953860000
	float{64.11} CustomFrameRate = -1
	*[] TimeMarker = { }
	int CurrentTimeMarker = -1
*/

class FBXFileHandler : FileHandler {
	const char*		GetExt() override { return "fbx";				}
	const char*		GetDescription() override { return "Kaydara Filmbox";	}
	int				Check(istream &file) override { file.seek(0); return file.get<BFBXheader>().valid() ? CHECK_PROBABLE : CHECK_NO_OPINION; }

	ISO_ptr<void>	Read(tag id, istream &file) override {
		ISO_ptr<anything>	p(id);
		streamptr			end = file.length();

		BFBXheader	bh = file.get();
		if (bh.valid()) {
			//binary
			FBXBinaryReader		fbx(file, p);
			ISO_browser2		settings;
			BFBXentity			ent;
			while (file.tell() < end && file.read(ent) && ent.next) {
				string			tok = file.get();
				ISO_ptr<void>	p1;
				if (tok == "GlobalSettings")
					settings = GetProperties(fbx.ReadEntity(tok, ent));
				else if (tok == "Definitions")
					fbx.ReadDefinitions(tok, ent);
				else if (tok == "Objects")
					fbx.ReadObjects(tok, ent);
				else if (tok == "Connections")
					fbx.ReadConnections(tok, ent);

				file.seek(ent.next);
			}

			for (auto &i : fbx.objects) {
				if (i.ID() == "Model") {
					ISO_browser2 b(i);
					const char *type = b["TYPE"].GetString();
					if (str(type) == "Mesh" || str(type) == "Line") {
						if (ISO_browser2 bmat = b["Material"]) {
							FBXMaterialMaker	mat2(settings);
							mat2.Make(bmat);

							ISO_browser bgeom	= b["Geometry"];
							if (ISO_ptr<Model3> model = FBXMakeModel(bgeom ? bgeom : b, type, mat2, settings))
								i->Append(model);
						}
					}
				}
			}

		} else {
			//ascii
			file.seek(0);
			FBXAsciiReader		fbx(file);
			while (!fbx.TestEnd()) {
				string			tok = fbx.ReadName();
				ISO_ptr<void>	p1	= fbx.ReadEntity(tok);
				p->Append(p1);
			}
		}

		p.SetID(id);
		return p;
	}
} fbx;

class RAWFBXFileHandler : FileHandler {
//	const char*		GetExt() override { return "fbx";				}
	const char*		GetDescription() override { return "RAW Kaydara Filmbox";	}
	int				Check(istream &file) override { file.seek(0); return file.get<BFBXheader>().valid() ? CHECK_PROBABLE : CHECK_NO_OPINION; }

	ISO_ptr<void>	Read(tag id, istream &file) override {
		ISO_ptr<anything>	p(id);
		streamptr			end = file.length();

		BFBXheader	bh = file.get();
		if (bh.valid()) {
			//binary
			FBXBinaryReader		fbx(file, p, true);
			return fbx.ReadBlock(id, end);
		} else {
			//ascii
			file.seek(0);
			FBXAsciiReader		fbx(file, true);
			while (!fbx.TestEnd())
				p->Append(fbx.ReadEntity(fbx.ReadName()));
		}
		return p;
	}
} raw_fbx;


