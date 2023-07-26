#include "iso/iso_files.h"
#include "iso/iso_convert.h"
#include "base/algorithm.h"
#include "extra/indexer.h"
#include "extra/text_stream.h"
#include "extra/gpu_helpers.h"
#include "extra/perlin_noise.h"
#include "bitmap/bitmap.h"
#include "bin.h"
#include "fx.h"
#include "render.h"
#include "model_utils.h"
#include "bone.h"
#include "comms/zlib_stream.h"
#include "maths/geometry.h"
#include "shader_gen.h"

using namespace iso;

template<typename S> struct read_array_s {
	template<typename D> static void f(istream_ref file, D *d, int n) {
		while (n--)
			*d++ = file.get<S>();
	}
	static void f(istream_ref file, S *d, int n) {
		readn(file, d, n);
	}
};
template<typename S, typename D> void read_array(istream_ref file, D *d, int n) {
	read_array_s<S>::f(file, d, n);
}

quaternion FBXRotation(param(float3) rot) {
	float3	rot_rad = degrees(rot);
	return rotate_in_z(rot_rad.z) * rotate_in_y(rot_rad.y) * rotate_in_x(rot_rad.x);
}

float3x4 FBXMatrix(param(float3) trans, param(float3) rot) {
	return translate(trans) * FBXRotation(rot);
}

float3x4 FBXMatrix(param(float3) trans, param(float3) rot, param(float3) scale) {
	return (translate(trans) * FBXRotation(rot)) * iso::scale(scale);
}

ISO::Browser2 GetProperties(const ISO::Browser2 &b) {
	if (ISO::Browser2 b2 = b["Properties70"])
		return b2;
	return b["Properties60"];
}

float3 GetAnimationKey0(const ISO::Browser &b) {
	ISO::Browser		props	= GetProperties(b)["d"];
	float	x = props["X"]["KeyValueFloat"][0].GetFloat();
	float	y = props["Y"]["KeyValueFloat"][0].GetFloat();
	float	z = props["Z"]["KeyValueFloat"][0].GetFloat();
	return {x, y, z};
}

struct FBXSettings {
	const ISO::Browser2	settings;
	double				scale;
	double				time_scale;
	int					up_axis, front_axis, orig_up, time_mode;
	hash_map<tag, ISO_ptr<void>>	bone_lookup;
//	ISO_ptr<void>		pose_model;
	BasePose			*pose;

	FBXSettings(const ISO::Browser2 &_settings) : settings(_settings), pose(0) {
		scale		= settings["OriginalUnitScaleFactor"].Get(1.0) * 2.54f / 100;
		up_axis		= settings["UpAxis"].GetInt() * settings["UpAxisSign"].GetInt();
		orig_up		= settings["OriginalUpAxis"].GetInt() * settings["OriginalUpAxisSign"].GetInt();
		front_axis	= settings["FrontAxis"].GetInt() * settings["FrontAxisSign"].GetInt();
		time_mode	= settings["TimeMode"].GetInt();
		time_scale	= 46186158000;
	}

	float3x4 GetGeometryMatrix(const ISO::Browser &b) const {
		float3			trans(zero), rot(zero);
		if (ISO::Browser t = b["GeometricTranslation"])
			trans = to<float>(*(double3p*)t);
		if (ISO::Browser t = b["GeometricRotation"])
			rot = to<float>(*(double3p*)t);
		return iso::scale(float(scale)) * FBXMatrix(trans, rot);
	}

	float3x4 GetLocalMatrix(const ISO::Browser &props) const {
		float3			trans(zero), rot(zero), scale(one);

		if (ISO::Browser t = props["Lcl Translation"]) {
			if (t.Is("Lcl Translation"))
				trans = to<float>(*(double3p*)t);
			else
				trans = GetAnimationKey0(t);
		}
		if (ISO::Browser t = props["Lcl Rotation"]) {
			if (t.Is("Lcl Rotation"))
				rot = to<float>(*(double3p*)t);
			else
				rot = GetAnimationKey0(t);
		}
		if (ISO::Browser t = props["Lcl Scaling"]) {
			if (t.Is("Lcl Scaling"))
				scale = to<float>(*(double3p*)t);
			else
				scale = GetAnimationKey0(t);
		}

		return FBXMatrix(trans * this->scale, rot, scale);
	}

	float	GetDistance(const ISO::Browser &b)	const { return b.GetFloat() * scale; }
	float	GetTime(const ISO::Browser &b)		const { return float(b.Get(uint64(0)) / time_scale); }
	colour	GetColour(const double3p *p)		const { return p ? colour(to<float>(*p)) : colour(zero); }
};

static uint64 make_id(const char *s) {
	if (!s || str(s) == "Null")
		return 0;
	return iso::crc32(s);
}

typedef hash_map<uint64, ISO_ptr<anything> > Objects;

//-----------------------------------------------------------------------------
//	BINARY READER
//-----------------------------------------------------------------------------

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
	istream_ref		file;
	ValueReaderBase(istream_ref _file) : file(_file) {}

	static bool		is_number(TYPE type)	{ return type && strchr("CYILFD", type); }
	static bool		is_array(TYPE type)		{ return type && strchr("cyilfd", type); }
	static bool		is_int(TYPE type)		{ return type && strchr("CYIL", type); }
	static bool		is_string(TYPE type)	{ return type == BFBX_STRING; }

	int64			read_int(TYPE type);
	double			read_float(TYPE type);
	string			read_string();
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

string ValueReaderBase::read_string() {
	if (size_t len = file.get<uint32le>()) {
		string	s(len);
		file.readbuff(s.begin(), len);
		return s;
	}
	return 0;
}

template<typename D, typename S> ISO_ptr<ISO_openarray<D> > ValueReaderBase::read_iso_array(tag id) {
	uint32	n	= file.get<uint32le>();
	uint32	m	= file.get<uint32le>();
	uint32	x	= file.get<uint32le>();

	ISO_ptr<ISO_openarray<D> >	p(id, n);
	switch (m) {
		case 0:	read_array<S>(file, p->begin(), n); break;
		case 1: read_array<S>(zlib_reader(file).me(), p->begin(), n); break;
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
			return ISO_ptr<string>(id, read_string());
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

	void	next_type() {
		type = num ? (TYPE)file.getc() : BFBX_NONE;
	}
	template<typename T> const T& next(const T &t) { --num; next_type(); return t; }

public:
	int				remaining()		const	{ return num; }
	bool			is_number()		const	{ return ValueReaderBase::is_number(type); }
	bool			is_array()		const	{ return ValueReaderBase::is_array(type); }
	bool			is_int()		const	{ return ValueReaderBase::is_int(type); }
	bool			is_string()		const	{ return ValueReaderBase::is_string(type); }

	int64			read_int()				{ return next(ValueReaderBase::read_int(type)); }
	double			read_float()			{ return next(ValueReaderBase::read_float(type)); }
	string			read_string()			{ return next(ValueReaderBase::read_string()); }
	ISO_ptr<void>	read_iso(tag id = {})	{ return next(ValueReaderBase::read_iso(type, id)); }

	void	discard() {
		if (type) {
			if (type == BFBX_STRING) {
				ValueReaderBase::read_string();
			} else {
				ValueReaderBase::read_int(type);
			}
			--num;
			next_type();
		}
	}
	void	discard_all() {
		while (num)
			discard();
	}
	ValueReader(istream_ref _file, int _num) : ValueReaderBase(_file), num(_num) {
		next_type();
	}
};

string fix_name(const char *name) {
	string_builder	b;
	char	c = *name;

	if (c != '_' && !is_alpha(c))
		b << "_";

	for (; c = *name; ++name) {
		if (is_alphanum(c)) {
			b << c;
		} else if (is_whitespace(c)) {
			b << '_';
			name = skip_whitespace(name) - 1;
		}
	}
	return b;
}

struct FBXBinaryReader {
	istream_ref				file;
	bool				raw;
	hash_map<string, ISO_ptr<void> >	definitions;
	hash_map<string, ISO::Type*>			types;

	ISO_ptr<void>		ReadEntity		(tag id, BFBXentity &ent);
	ISO_ptr<anything>	ReadBlock		(tag id, streamptr end);
	ISO_ptr<void>		ReadProperties	(tag id, streamptr end);

	void				ReadDefinitions	(tag id, streamptr end);
	void				ReadObjects		(tag id, streamptr end, Objects &objects);
	void				ReadConnections	(tag id, streamptr end, Objects &objects);

	FBXBinaryReader(istream_ref _file, bool _raw = false) : file(_file), raw(_raw) {
		types["Compound"] = ISO::getdef<anything>();
	}
};

ISO_ptr<void> FBXBinaryReader::ReadEntity(tag id, BFBXentity &ent) {
	if (ent.count == 0) {
		if (ent.size == 0)
			return ReadBlock(id, ent.next);
		return ISO_NULL;
	}

	bool	has_block = file.tell() + ent.size < ent.next;

	ValueReader	val(file, ent.count);
	if (val.remaining() == 1 && !has_block)
		return val.read_iso(id);

	ISO_ptr<anything>	p(id);
	while (val.remaining())
		p->Append(val.read_iso());

	if (has_block)
		p->Append(ReadBlock(none, ent.next));

	return p;
}

ISO_ptr<anything> FBXBinaryReader::ReadBlock(tag id, streamptr end) {
	ISO_ptr<anything>	p(id);
	BFBXentity			ent;
	streamptr			tell;
	while ((tell = file.tell()) < end - sizeof(ent) && file.read(ent) && ent.next) {
		pascal_string	tok = file.get();
		if (!raw && tok.begins("Properties")) {
			p->Append(ReadProperties(tok, ent.next));
		} else {
			p->Append(ReadEntity(tok, ent));
		}
		file.seek(ent.next);
	}
	file.seek(end);
	return p;
}

ISO_ptr<void> FBXBinaryReader::ReadProperties(tag id, streamptr end) {
	ISO_ptr<anything>	p(id);
	int					version = from_string<int>(id.slice(sizeof("Properties") - 1).begin());
	BFBXentity			ent;
	while (file.tell() < end && file.read(ent) && ent.next) {
		streamptr		start	= file.tell();
		pascal_string	tok		= file.get();
		string			name;
		string			type;

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
				if (!p2) {
					p1->Append(p2.Create(t));
				} else if (!p2.IsType<anything>()) {
					tag2	t2 = t + "_";
					p2 = (*p1)[t2];
					if (!p2)
						p1->Append(p2.Create(t2));
				}
				p1 = p2;
			}
		}

		if (val.remaining() == 0) {
			p2 = ISO_ptr<anything>(name);

		} else if (val.remaining() == 1) {
			p2 = val.read_iso(name);

		} else {
			anything	a;
			while (val.remaining())
				a.Append(val.read_iso());

			ISO::Type *&t = types[type];
			if (!t) {
				//p2 = ISO_ptr<anything>(name, a);
				p2	= AnythingToNamedStruct(type, a, name);
				t	= unconst(p2.GetType());
			} else {
				p2	= MakePtr(t, name);
				ISO::Browser		b(p2);
				for (int i = 0, n = a.Count(); i < n; ++i)
					b[i].Set(a[i]);
			}
		}
		ISO_ASSERT(p1.IsType<anything>());
		p1->Append(p2);

		file.seek(ent.next);
	}
	return p;
}

void FBXBinaryReader::ReadDefinitions(tag id, streamptr end) {
	BFBXentity			ent;
	while (file.tell() < end && file.read(ent) && ent.next) {
		pascal_string	tok = file.get();
		if (tok == "ObjectType") {
			ValueReader	val(file, ent.count);

			string	type	= val.is_string() ? val.read_string() : 0;
			ISO_ptr<anything>	p2 = ReadBlock(tok, ent.next);
			definitions[type] = p2;
		}

		file.seek(ent.next);
	}
}

void FBXBinaryReader::ReadObjects(tag id, streamptr	end, Objects &objects)	{
	BFBXentity			ent;
	while (file.tell() < end && file.read(ent) && ent.next) {
		streamptr	start	= file.tell();
		pascal_string	tok		= file.get();
		uint64			id		= 0;
		string			name;
		string			type;

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
		if (type)
			p2->Append(ISO_ptr<string>("TYPE", type));
		if (name)
			p2->Append(ISO_ptr<string>("NAME", name));

		if (!has_id) {
			id = make_id(name ? name : tok);
			if (tok == "Video")
				id += uint64(1) << 32;

			while (objects.check(id))
				id += uint64(1) << 32;
		}
		objects[id] = p2;
	}
}

void FBXBinaryReader::ReadConnections(tag id, streamptr end, Objects &objects) {
	BFBXentity			ent;
	while (file.tell() < end && file.read(ent) && ent.next) {
		pascal_string	tok = file.get();	// always "C"
		string			type;
		string			to_prop, from_prop;
		uint64			to_id = 0, from_id = 0;

		ValueReader	val(file, ent.count);

		if (val.is_string())
			type = val.read_string();

		if (val.is_int())
			to_id = val.read_int();
		else if (val.is_string())
			to_id = make_id(val.read_string());

		if (val.is_int())
			from_id = val.read_int();
		else if (val.is_string()) {
			from_id = make_id(val.read_string());
			if (from_id == to_id)
				to_id += uint64(1) << 32;
		}

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
				ISO::Browser(from).Append().Set(to);
				ISO_ptr<anything>	t	= (*to)["from"];
				if (!t)
					to->Append(t.Create("from"));
				t->Append(from);

			} else if (type == "OP") {
				ISO::Browser	b = GetProperties(from);
				string_scan	ss(to_prop);
				for (;;) {
					auto t = ss.get_token(~char_set('|'));
					if (!ss.remaining()) {
						ISO_ptr<ISO_ptr<anything> >	to2(0, to);
						b.SetMember(t, to2);
						break;
					}
					ss.move(1);
					b	= b[t];
				}
				//ISO::Browser(from)["Properties70"].SetMember(to_prop, to);
			} else if (type == "PO") {
				ISO_TRACEF("PO %i, %i\n", to_id, from_id);

			} else if (type == "PP") {
				ISO_TRACEF("PP %i, %i\n", to_id, from_id);

			}
		}
		file.seek(ent.next);
	}
}

//-----------------------------------------------------------------------------
//	ASCII READER
//-----------------------------------------------------------------------------

int skip_comments(text_reader<reader_intf> &file) {
	int		c;
	while ((c = skip_whitespace(file)) == ';') {
		while ((c = file.getc()) != EOF && c != '\n');
	}
	return c;
}

struct AsciiValueReaderBase {
	enum TYPE {
		FBX_NONE,
		FBX_NUMBER,
		FBX_ARRAY,
		FBX_STRING,
		FBX_UNKNOWN,
	};
	struct number {
		uint64	u;
		int16	exp;
		uint16	neg:1, flt:1;
		number(uint64 _u, int _exp, bool _neg, bool _flt) : u(_u), exp(_exp), neg(_neg), flt(_flt) {}
		operator int64()	const { return neg ? -int64(u) : int64(u); }
		operator double()	const { return operator int64() * pow(10.0, exp); }
	};

	text_reader<reader_intf>	&file;

	static TYPE	get_type(int c) {
		switch (c) {
			case '{':	return FBX_NONE;
			case '*':	return FBX_ARRAY;
			case '"':	return FBX_STRING;
			case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
			case '-': case '+': case '.':
						return FBX_NUMBER;
			default:	return FBX_UNKNOWN;
		}
	}
	number			read_number();
	string&			read_string(string &s);
	string&			read_name(string &s);

	ISO_ptr<void>	read_iso(TYPE type, tag id);
	AsciiValueReaderBase(text_reader<reader_intf> &file) : file(file) {}
};

string&	AsciiValueReaderBase::read_name(string &s) {
	int		c = skip_comments(file);
	for (auto b = build(s); is_alphanum(c) || c == '_'; c = file.getc())
		b.putc(c);
	ISO_ASSERT(s.length() && c == ':');
	return s;
}

string& AsciiValueReaderBase::read_string(string &s) {
	int		c	= skip_comments(file);
	if (c == '"') {
		for (auto b = build(s); (c = file.getc()) != '"'; )
			b.putc(c);

	} else {
		for (auto b = build(s); is_alphanum(c) || c == '_'; c = file.getc())
			b.putc(c);
		file.put_back(c);
	}
	return s;
}

AsciiValueReaderBase::number AsciiValueReaderBase::read_number() {
	int		c	= skip_comments(file);
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

ISO_ptr<void> AsciiValueReaderBase::read_iso(TYPE type, tag id) {
	switch (type) {
		case FBX_NUMBER: {
			number	n = read_number();
			if (n.flt)
				return ISO_ptr<double>(0, n);
			int64	i = n;
			if (i == int32(i))
				return ISO_ptr<int>(0, i);
			return ISO_ptr<int64>(0, i);
		}
		case FBX_ARRAY: {
			int		c	= skip_comments(file);
			ISO_ASSERT(c == '*');
			int		count = 0;
			while (is_digit(c = file.getc()))
				count = count * 10 + c - '0';
			c = skip_comments(file);
			if (c == '{') {
				string	entry;
				ISO_VERIFY(read_name(entry) == "a");
				c = skip_comments(file);
				file.put_back(c);
				switch (get_type(c)) {
					case FBX_NUMBER: {
						ISO_ptr<ISO_openarray<double> >	d(0, count);
						for (int i = 0; i < count; i++) {
							(*d)[i] = read_number();
							c = skip_comments(file);
							ISO_ASSERT(c == i == count -1 ? '}' : ',');
						}
						return d;
					}
					default:
						ISO_ASSERT(0);
				}
			}
			break;
		}

		case FBX_STRING:
		case FBX_UNKNOWN: {
			ISO_ptr<string>	p(id);
			read_string(*p);
			return p;
		}
		default:
			break;
	}
	return ISO_NULL;
}

class AsciiValueReader : public AsciiValueReaderBase {
	TYPE		type;

	void	next_type() {
		int		c = skip_comments(file);
		if (c == ',') {
			c = skip_comments(file);
			type = get_type(c);
		} else {
			type = FBX_NONE;
		}
		file.put_back(c);
	}

	template<typename T> const T& next(const T &t) { next_type(); return t; }

public:
	bool			remaining()		const	{ return type != FBX_NONE; }
	bool			is_number()		const	{ return type == FBX_NUMBER; }
	bool			is_array()		const	{ return type == FBX_ARRAY; }
	bool			is_string()		const	{ return type == FBX_STRING; }

	int64			read_number()			{ return next(AsciiValueReaderBase::read_number()); }
	string			read_string()			{ string s; return next(AsciiValueReaderBase::read_string(s)); }
	ISO_ptr<void>	read_iso(tag id = {})	{ return next(AsciiValueReaderBase::read_iso(type, id)); }

	void	read_type() {
		int	c = skip_comments(file);
		type = get_type(c);
		file.put_back(c);
	}

	void	discard() {
		if (type) {
			if (type == FBX_STRING) {
				string	s;
				AsciiValueReaderBase::read_string(s);
			} else {
				AsciiValueReaderBase::read_number();
			}
			next_type();
		}
	}
	void	discard_all() {
		while (type)
			discard();
	}
	AsciiValueReader(text_reader<reader_intf> &file) : AsciiValueReaderBase(file) {
		read_type();
	}
};

struct FBXAsciiReader {
	text_reader<reader_intf>		file;
	bool							raw;
	hash_map<string, ISO::Type*>	types;

	string	ReadName() {
		string	s;
		return AsciiValueReaderBase(file).read_name(s);
	}

	ISO_ptr<void>	ReadEntity(tag id);
	ISO_ptr<void>	ReadBlock(tag id);

	ISO_ptr<void>	ReadProperties	(tag id);
	void			ReadObjects		(tag id, Objects &objects);
	void			ReadConnections	(tag id, Objects &objects);
	ISO_ptr<void>	ReadDefinitions	(tag id)	{ return ReadEntity(id); }

	FBXAsciiReader(istream_ref file, bool _raw = false) : file(file), raw(_raw) {}
	bool			eof() {
		int c = skip_comments(file);
		if (c < 0)
			return true;
		file.put_back(c);
		return false;
	}
};

ISO_ptr<void> FBXAsciiReader::ReadEntity(tag id) {
	ISO_ptr<anything>	p(id);
	AsciiValueReader	val(file);

	while (val.remaining())
		p->Append(val.read_iso());

	int	c = skip_comments(file);
	if (c == '{') {
		p->Append(ReadBlock(none));
		c = skip_comments(file);
	}
	file.put_back(c);

	if (p->Count() == 1) {
		ISO_ptr<void>	p1 = (*p)[0];
		p1.SetID(id);
		return p1;
	}
	return p;
}

ISO_ptr<void> FBXAsciiReader::ReadBlock(tag id) {
	ISO_ptr<anything>	p(id);
	int	c;
	while ((c = skip_comments(file)) != '}') {
		file.put_back(c);
		string	tok = ReadName();
		if (!raw && tok.begins("Properties")) {
			p->Append(ReadProperties(tok));
		} else {
			p->Append(ReadEntity(tok));
		}
	}
	return p;
}


ISO_ptr<void> FBXAsciiReader::ReadProperties(tag id) {
	ISO_ptr<anything>	p(id);
	int					version = from_string<int>(id.slice(sizeof("Properties") - 1).begin());

	AsciiValueReader	val(file);
	int	c = skip_comments(file);
	if (c == '{') while ((c = skip_comments(file)) != '}') {
		file.put_back(c);
		string		tok;
		string		name;
		string		type;

		val.read_name(tok);

		val.read_type();
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

		anything	a;
		while (val.remaining())
			a.Append(val.read_iso());

		if (a.Count() == 1) {
			p2 = a[0];
			p2.SetID(name);
		} else {
#if 0
			ISO::Type *&t = types[type];
			if (!t) {
				p2	= AnythingToNamedStruct(type, a, name);
				t	= unconst(p2.GetType());
			} else {
				p2	= AssignToStruct(t, a, name);
			}
#endif
		}
		p1->Append(p2);
	}
	return p;
}

void FBXAsciiReader::ReadObjects(tag id, Objects &objects)	{
	AsciiValueReader	val(file);
	int	c = skip_comments(file);
	if (c == '{') while ((c = skip_comments(file)) != '}') {
		file.put_back(c);
		string		tok;
		string		name;
		string		type;
		uint64		id		= 0;

		val.read_name(tok);
		val.read_type();

		bool		has_id = val.is_number();
		if (has_id)
			id = val.read_number();

		if (val.is_string())
			name = val.read_string();

		if (val.is_string())
			type = val.read_string();

		val.discard_all();

		file.getc();
		ISO_ptr<anything>	p2 = ReadBlock(tok);
		p2->Append(ISO_ptr<string>("TYPE", type));
		p2->Append(ISO_ptr<string>("NAME", name));

		if (!has_id)
			id = make_id(name);
		objects[id] = p2;
	}
}

void FBXAsciiReader::ReadConnections(tag id, Objects &objects) {
	AsciiValueReader	val(file);
	int	c = skip_comments(file);
	if (c == '{') while ((c = skip_comments(file)) != '}') {
		file.put_back(c);
		string	tok;
		string	type;
		string	to_prop, from_prop;
		uint64	to_id = 0, from_id = 0;

		val.read_name(tok);
		val.read_type();	// always "C"

		if (val.is_string())
			type = val.read_string();

		if (val.is_number())
			to_id = val.read_number();
		else if (val.is_string())
			to_id = make_id(val.read_string());

		if (val.is_number())
			from_id = val.read_number();
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
				ISO::Browser(from).Append().Set(to);
			} else if (type == "OP") {
				ISO::Browser	b = GetProperties(from);
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
				//ISO::Browser(from)["Properties70"].SetMember(to_prop, to);
			} else if (type == "PO") {
			} else if (type == "PP") {
			}
		}
	}
}

//-----------------------------------------------------------------------------
//	MATERIAL MAKER
//-----------------------------------------------------------------------------

struct FBXMaterialMaker : MaterialMaker, FBXSettings {
	ref_ptr<ColourSource> GetColourSource(const ISO::Browser &b);
	const char*		GetUVSet(const ISO::Browser2 &b) {
		ISO::Browser	props = GetProperties(b);
		return props["UVSet"].GetString();
	}
	float	GetDistance(const ISO::Browser &b) {
		return FBXSettings::GetDistance(b);
	}
	float	GetTime(const ISO::Browser &b) {
		return FBXSettings::GetTime(b);
	}
	colour	GetColour(const ISO::Browser &b) {
		return FBXSettings::GetColour(b);
	}
	ISO::Browser GetArrayElement(const ISO::Browser &b, const char *id, int i) {
		ISO::Browser b2 = b[format_string("%s_", id)];
		if (!b2)
			b2 = b[id];
		return b2[format_string("%s[%i]", id, i)];
	}

	FBXMaterialMaker(const ISO::Browser2 &mat, const FBXSettings &settings, uint32 _flags);
};

FBXMaterialMaker::FBXMaterialMaker(const ISO::Browser2 &mat, const FBXSettings &settings, uint32 _flags) : MaterialMaker(_flags), FBXSettings(settings) {
	if (!mat)
		return;

	const char		*type			= mat["ShadingModel"].GetString();
	ISO::Browser		props			= GetProperties(mat);
	ShaderSource	*vert_source	= GetVertSource();

	ref_ptr<ShadingModel>	sm;

	if (ISO::Browser props2 = props["3dsMax"]) {
		Class_ID	id(props2["ClassIDa"].GetInt(), props2["ClassIDb"].GetInt());
		SClass_ID	super	= props2["SuperClassID"].GetInt();
		ISO::Browser params	= props2["parameters"];

		sm = MAXShadingModelCreator::Create(*this, id, super, mat, params ? params : props2);
/*
		if (id == DXMATERIAL_CLASS_ID) {
			MakeDX(mat, props2["HwShaderParams"]);
			return;
		}

		if (MAXShadingModelCreator *creator = MAXShadingModelCreator::Find(id, super)) {
			ISO::Browser params = props2["parameters"];
			sm = (*creator)(*this, mat, params ? params : props2, vert_source);
		}
		*/
	}

	if (!sm) {
		if (str(type) == "lambert")
			sm = new Lambert(*this, props, vert_source);
		else
			sm = new Phong(*this, props, vert_source);
	}
	Generate(type, sm);
}

ISO_ptr<bitmap> FBXGetBitmap(const FBXSettings &settings, const ISO::Browser &b) {
	ISO::Browser content = b["Content"];
	if (!content)
		return ISO_NULL;

	double	FrameRate, PlaySpeed;
	int		LastFrame, StartFrame, StopFrame;
	int		Width, Height;
	bool	FreeRunning, Loop;
	int		InterlaceMode, AccessMode;

	read_props(settings, GetProperties(b),
		F(FrameRate), F(PlaySpeed),
		F(LastFrame), F(StartFrame), F(StopFrame),
		F(Width), F(Height),
		F(FreeRunning), F(Loop),
		F(InterlaceMode), F(AccessMode)
	);

	filename	fn(b["Filename"].GetString());
	return FileHandler::Read(fn.name(), lvalue(memory_reader(GetRawData(content))), fn.ext());
}

ref_ptr<ColourSource> FBXMaterialMaker::GetColourSource(const ISO::Browser &b) {
	if (b.Is<double>()) {
		double	*p = b;
		float	f	= float(*p);
		return new SolidColour(colour(f,f,f,f));
	}
	if (b.Is("ColorRGB") || b.Is("Color")) {
		double3p	*p = b;
		return new SolidColour(colour(to<float>(*p), one));
	} else if (b.Is("ColorAndAlpha")) {
		double4p	*p = b;
		return new SolidColour(colour(to<float>(*p)));
	}

	const char *type = b["Type"].GetString();
	if (!type)
		type = b["TYPE"].GetString();

	if (str(type) == "TextureVideoClip") {
		ISO::Browser	props = GetProperties(b);

		if (ISO::Browser props2 = props["3dsMax"]) {
			Class_ID	id(props2["ClassIDa"].GetInt(), props2["ClassIDb"].GetInt());
			SClass_ID	super	= props2["SuperClassID"].GetInt();

			if (MAXColourSourceCreator *creator = MAXColourSourceCreator::Find(id, super)) {
				ISO::Browser params = props2["parameters"];
				return (*creator)(*this, params ? params : props2);
			}
		}

		string		name	= fix_name(b["TextureName"].GetString());

		auto	tex	= ISO::MakePtr<64>(ISO::getdef<Texture>(), name);
		ISO_ptr<bitmap>	bm;

		if (ISO::Browser bvid = b["Video"])
			bm = FBXGetBitmap(*this, bvid);

		if (!bm) {
			const char *rel		= b["RelativeFilename"].GetString();
			filename	fn		= FileHandler::FindAbsolute(rel);
			if (!fn)
				fn = rel;
			bm.CreateExternal(fn);
		}
		*(ISO_ptr_machine<void>*)tex	= bm;

		parameters->Append(tex);

		const char *uv_set		= props["UVSet"].GetString();
		if (!uv_set || uv_set == cstr("default")) {
			uv_set = "uv";
			flags |= UVDEFAULT;
		} else {
			if (find(inputs, uv_set) == inputs.end())
				inputs.push_back(uv_set);
		}
		return new TextureMap(name, uv_set);
	}
	return nullptr;
}

//-----------------------------------------------------------------------------
//	MODEL MAKER
//-----------------------------------------------------------------------------

//typedef constructable<soft_vector<4,uint8> >	byte4;
typedef uint8x4	byte4;

struct FBXindex {
	int	i;
	operator uint32()	const { return uint32(i < 0 ? ~i : i); }
	bool	is_end()	const { return i < 0; }
};

struct FBXweight {
	int		bone;
	float	weight;
	FBXweight() : bone(0), weight(0) {}
	FBXweight(int _bone, float _weight) : bone(_bone), weight(_weight) {}
	operator float() const { return weight; }
};

struct FBXweight4 {
	byte4	bones;
	float4	weights;
	FBXweight4(const dynamic_array<FBXweight> &a) {
		clear(*this);
		int	x = 0;
		for (auto &i : a) {
			bones[x]	= i.bone;
			weights[x]	= i.weight;
			++x;
		}
	}
};

struct FBXComponent {
	enum MAP {
		None			= 0,
		AllSame			= 1,
		ByPolygonVertex	= 2,
		ByVertice		= 3,
		ByPolygon		= 4,
	};
	const void		*data;
	const int		*indices;
	MAP				mapping;
	int				ncomps;
	double4p		constant;

	FBXComponent() : data(0) {}
	FBXComponent(const void *data, int ncomps, MAP mapping)	: data(data), indices(0), mapping(mapping), ncomps(ncomps) {}
	template<typename T> FBXComponent(const T &_constant)	: data(&constant), indices(0), mapping(AllSame), ncomps(num_elements_v<T>), constant(_constant) {}

	template<typename F, typename T> void Process2(F f, T *vals, const FBXindex *vert_indices, const int *poly_indices) {
		switch (mapping) {
			case None:
				f(vals);
				break;
			case AllSame:
				f(scalar(*vals));
				break;
			case ByPolygonVertex:
				if (indices)
					f(make_indexed_iterator(vals, indices));
				else
					f(vals);
				break;
			case ByVertice:
				f(make_indexed_iterator(vals, vert_indices));
				break;
			case ByPolygon:
				f(make_indexed_iterator(vals, poly_indices));
				break;
		}
	}
	template<typename F> void Process1(F f, const FBXindex *vert_indices, const int *poly_indices) {
		switch (ncomps) {
			//case 1: Process2(f, (double *)data, vert_indices, poly_indices); break;
			case 2: Process2(f, (double2p*)data, vert_indices, poly_indices); break;
			case 3: Process2(f, (double3p*)data, vert_indices, poly_indices); break;
			case 4: Process2(f, (double4p*)data, vert_indices, poly_indices); break;
		}
	}

	template<typename F, typename I2> struct AddIndex_s {
		F		f;
		I2		i2;
		AddIndex_s(F &&f, I2 i2) : f(forward<F>(f)), i2(i2) {}
		template<typename I> void	operator()(I i) { return f(indexed_iterator<I&,I2>(i, i2)); }
	};
	template<typename F, typename I2> AddIndex_s<F, I2> AddIndex(F &&f, I2 i2) { return AddIndex_s<F, I2>(forward<F>(f), i2); }

	struct Index_s {
		Indexer<uint32> &indexer;
		Index_s(Indexer<uint32> &_indexer) : indexer(_indexer) {}
		template<typename I> void	operator()(I i) { indexer.Process(i, equal_vec()); }
	};
	void	Index(Indexer<uint32> &indexer, const FBXindex *vert_indices, int *poly_indices) {
		Process1(Index_s(indexer), vert_indices, poly_indices);
	}
	void	Index(Indexer<uint32> &indexer, const int *mat_filter, const FBXindex *vert_indices, int *poly_indices) {
		Process1(AddIndex(Index_s(indexer), mat_filter), vert_indices, poly_indices);
	}

	struct Write_s {
		const Indexer<uint32> &indexer;
		stride_iterator<void> dest;
		Write_s(const Indexer<uint32> &_indexer, stride_iterator<void> &_dest) : indexer(_indexer), dest(_dest) {}
		template<typename I> void	operator()(I i) {
			//copy(make_indexed_container(i, indexer.RevIndices()), stride_iterator<array_vec<float, num_elements_v<typename iterator_traits<I>::element>> >(dest));
			copy(make_indexed_container(i, indexer.RevIndices()), stride_iterator<array_vec<float, num_elements_v<it_element_t<I>>> >(dest));
		}
	};
	void	Write(stride_iterator<void> dest, const Indexer<uint32> &indexer, const FBXindex *vert_indices, const int *poly_indices) {
		Process1(Write_s(indexer, dest), vert_indices, poly_indices);
	}
	void	Write(stride_iterator<void> dest, const int *mat_filter, const Indexer<uint32> &indexer, const FBXindex *vert_indices, const int *poly_indices) {
		Process1(AddIndex(Write_s(indexer, dest), mat_filter), vert_indices, poly_indices);
	}
};

void FBXAddSubMesh(const Indexer<uint32> &indexer, ModelBuilder &mb, const ISO::Type *vert_type, const dynamic_array<FBXComponent*> &components, const FBXindex *vert_indices, int *poly_indices, int *mat_filter, dynamic_array<FBXweight> *skin) {
	auto	i0 = make_indexed_iterator(vert_indices, mat_filter);

	if (indexer.NumUnique() < 65536) {
		int	nt	= 0;
		for (auto i = i0, p = i0 - 1, e = i0 + indexer.NumIndices(); i != e; ++i) {
			if (i->is_end()) {
				nt	+= i - p - 2;
				p	= i;
			}
		}

		SubMesh			*mesh = mb.AddMesh(vert_type, indexer.NumUnique(), nt);
		for (int i = 0; i < components.size(); i++)
			components[i]->Write(mesh->VertComponentData<void>(i), mat_filter, indexer, vert_indices, poly_indices);

		if (skin) {
			auto weights	= mesh->VertComponentData<float4p>("weights");
			auto bones		= mesh->VertComponentData<byte4>("bones");
			for (int i = 0; i < indexer.NumUnique(); i++) {
				int			j	= vert_indices[mat_filter[indexer.RevIndex(i)]];
				FBXweight4	w4	= skin[j];
				weights[i]	= w4.weights;
				bones[i]	= w4.bones;
			}
		}

		auto	d = mesh->indices.begin();

		for (auto i = i0, e = i0 + indexer.NumIndices(); i != e; ) {
		#if 1
			int		a = i - i0;
			while (!i++->is_end());
			int		b = i - i0;
			d = convex_to_tris(d, indexer.Indices().begin() + a, indexer.Indices().begin() + b);
		#else
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
		#endif
		}
		mesh->UpdateExtent();
	} else {
		mb.InitVerts(vert_type, indexer.NumUnique());
		malloc_block	verts(mb.vert_size * indexer.NumUnique());
		for (int i = 0; i < components.size(); i++)
			components[i]->Write(mb.GetVertexComponent(verts, i), mat_filter, indexer, vert_indices, poly_indices);

		for (auto i = i0, e = i0 + indexer.NumIndices(); i != e; ) {
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
		mb.Purge(verts);
	}
}

ISO_ptr<Model3> FBXMakeModel(const ISO::Browser2 &bmodel, const char *type, const FBXSettings &settings) {
	int	mat_index	= bmodel.GetIndex("Material");
	if (mat_index < 0)
		return ISO_NULL;

	ISO::Browser b	= bmodel["Geometry"];
	if (!b)
		b = bmodel;

	float3x4	matrix = settings.GetGeometryMatrix(GetProperties(bmodel));

	if (str(type) == "Line") {
		if (ISO::Browser bv = b["Points"]) {
			auto		v		= ISO_conversion::convert<ISO_openarray<double> >(bv);
			int			nv		= v->Count() / 3;
			double3p	*verts	= (double3p*)v->begin();

			for_eachn(verts, nv, [matrix](double3p &a) { a = matrix * position3(to<float>(a)); });

			ISO::Browser	b2 = b["PointsIndex"];
			int						ni		= b2.Count() / 3;
			array<int,3>*		indices	= b2[0];

			ISO::TypeCompositeN<64>	builder(0);
			builder.Add<float[3]>("position");

			ModelBuilder	mb(b.GetName(), ISO_NULL);
			SubMesh			*mesh	= mb.AddMesh(builder.Duplicate(), nv, ni);
			auto			pos		= mesh->VertComponentData<float3p>("position");
//			copy_n(verts, pos, nv);
			for_each2n(verts, pos, nv, [matrix](const double3p &a, float3p &b) { b = matrix * position3(to<float>(a)); });
			copy_n(indices, mesh->indices.begin(), ni);
			mb->UpdateExtents();
			return move(mb);
		}

	} else if (str(type) == "Mesh") {
		if (ISO::Browser bv = b["Vertices"]) {
			ISO::Browser bi	= b["PolygonVertexIndex"];
			int			nv	= bv.Count() / 3;
			int			ni	= bi.Count();

			map<string, FBXComponent*> components;
			unique_ptr<FBXComponent>	comp_pos, comp_norm, comp_col, comp_mat, comp_uvdefault;

			auto		v		= ISO_conversion::convert<ISO_openarray<double> >(bv);
			double3p	*verts	= (double3p*)v->begin();

			for_eachn(verts, nv, [matrix](double3p &a) { a = matrix * position3(to<float>(a)); });

			comp_pos			= new FBXComponent(*v, 3, FBXComponent::ByVertice);
			comp_pos->indices	= *ISO_conversion::convert<ISO_openarray<int> >(bi);

			for (int layer = -1; (layer = b.GetIndex("Layer", layer + 1)) >= 0;) {
				ISO::Browser b2 = b[layer][1];

				for (auto &&i : b2) {
					if (i.GetName() == "LayerElement") {
						const char	*type = i["Type"].GetString();
						int			index = i["TypedIndex"].GetInt();

						int			element = -1;
						for (int i = 0; i <= index; i++)
							element = b.GetIndex(type, element + 1);

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
								comp_norm = new FBXComponent(*ISO_conversion::convert<ISO_openarray<double> >(b2["Normals"]), 3, mapping);
								if (indexed)
									comp_norm->indices = *ISO_conversion::convert<ISO_openarray<int> >(b2["NormalIndex"]);

							} else if (str(type) == "LayerElementUV" || str(type) == "LayerElementBumpUV" || str(type) == "LayerElementReflectionUV") {
								ISO_ptr<ISO_openarray<double> >	uv = ISO_conversion::convert<ISO_openarray<double> >(b2["UV"]);
								for (double *d = uv->begin(), *de = uv->end(); d != de; d += 2)
									d[1] = 1 - d[1];

								FBXComponent	*c = new FBXComponent(*uv, 2, mapping);
								if (indexed)
									c->indices = *ISO_conversion::convert<ISO_openarray<int> >(b2["UVIndex"]);
								components[name] = c;
								if (!comp_uvdefault)
									comp_uvdefault = c;

							} else if (str(type) == "LayerElementColor") {
								ISO_ptr<ISO_openarray<double> >	cols = ISO_conversion::convert<ISO_openarray<double> >(b2["Colors"]);
								FBXComponent	*c = new FBXComponent(*cols, 4, mapping);
								if (indexed)
									c->indices = *ISO_conversion::convert<ISO_openarray<int> >(b2["ColorIndex"]);
								comp_col = c;

							} else if (str(type) == "LayerElementMaterial" && mapping != FBXComponent::AllSame) {
								comp_mat = new FBXComponent(*ISO_conversion::convert<ISO_openarray<int> >(b2["Materials"]), 1, mapping);

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

			if (!comp_col) {
				double4p	col(one);
				comp_col = new FBXComponent(col);
			}

			dynamic_array<dynamic_array<FBXweight> >	skin;
//			if (settings.pose) {
				if (ISO::Browser skinb = b["Deformer"]) {
					skin.resize(nv);
					int	bone = 0;
					for (auto i : skinb) {
						if (i.GetName() == "Deformer") {
							ISO::Browser	indices	= i["Indexes"];
							ISO::Browser	weights	= i["Weights"];
							const char *name = i["Model"]["NAME"].GetString();
							if (settings.pose)
								bone	= settings.pose->GetIndex(name);

							for (int j = 0, n = indices.Count(); j < n; j++) {
								int		index	= indices[j].GetInt();
								float	weight	= weights[j].GetFloat();
								ISO_ASSERT(index < nv);
								new(skin[index]) FBXweight(bone, weight);
							}
							++bone;
						}
					}
					for (auto i : skin) {
						sort(i);
						if (i.size() > 4)
							i.resize(4);
						float	total = reduce<op_add>(i, 0.f);
						for_each(i, [total](FBXweight &w) { w.weight /= total; });
					}
				}
//			}

			const char			*name = bmodel["NAME"].GetString();
			ModelBuilder		mb(name);
			const FBXindex		*vert_indices	= (FBXindex*)comp_pos->indices;
			int					*poly_indices	= new int[ni];
			int					*mat_filter		= new int[ni];

			int		num_tris	= 0;
			int		num_polys	= 0;
			int		*pi			= poly_indices;
			int		*di			= mat_filter;
			for (const FBXindex *i0 = vert_indices, *i = i0, *p = i0 - 1, *e = i + ni; i < e; ++i) {
				*pi++ = num_polys;
				*di++ = i - i0;
				if (i->is_end()) {
					++num_polys;
					num_tris	+= i - p - 2;
					p			= i;
				}
			}

			if (comp_mat) {
				for (int m = 0;;) {
					int		*mi		= (int*)comp_mat->data;
					int		*di		= mat_filter;
					int		nextm	= 1000;
					for (const FBXindex *i0 = vert_indices, *i = i0, *e = i + ni; i < e; mi++) {
						int	m2 = *mi;
						if (m2 == m) {
							do
								*di++ = i - i0;
							while (!i++->is_end());
						} else {
							if (m2 > m)
								nextm = min(nextm, m2);
							while (!i++->is_end());
						}
					}

					if (int nm = di - mat_filter) {
						FBXMaterialMaker				mat(bmodel[mat_index], settings, skin ? FBXMaterialMaker::SKIN : 0);
						dynamic_array<FBXComponent*>	used;
						ISO::TypeCompositeN<64>			builder(0);

						used.push_back(comp_pos);
						builder.Add<float[3]>("position");
						if (mat.flags & mat.NORMALS) {
							used.push_back(comp_norm);
							builder.Add<float[3]>("normal");
						}
						if (mat.flags & mat.UVDEFAULT) {
							used.push_back(comp_uvdefault);
							builder.Add<float[3]>("uv");
						}
						for (auto &i : mat.inputs) {
							used.push_back(components[i]);
							builder.Add<float[2]>(i);
						}
						if (mat.flags & mat.COLOURS) {
							used.push_back(comp_col);
							builder.Add<float[4]>("colour");
						}
						if (mat.tangent_uvs)
							builder.Add<float[4]>("tangent");

						if (skin) {
							builder.Add<float[4]>("weights");
							builder.Add<uint8[4]>("bones");
						}

						Indexer<uint32>	indexer(nm);
						for (int i = 0; i < used.size(); i++)
							used[i]->Index(indexer, mat_filter, vert_indices, poly_indices);

						int		i	= mb->submeshes.Count();
						mb.SetMaterial((ISO_ptr<void>&)mat.technique, AnythingToStruct(*mat.parameters));
						FBXAddSubMesh(indexer, mb, builder.Duplicate(), used, vert_indices, poly_indices, mat_filter, skin);

						for (int end = mb->submeshes.Count(); i < end; i++) {
							SubMesh *submesh = mb->submeshes[i];
							if (mat.tangent_uvs)
								GenerateTangents(submesh, mat.tangent_uvs);
						}
					}

					if (nextm == 1000)
						break;

					while (nextm > m) {
						mat_index	= bmodel.GetIndex("Material", mat_index + 1);
						++m;
					}
				}

			} else {
				FBXMaterialMaker				mat(bmodel[mat_index], settings, skin ? FBXMaterialMaker::SKIN : 0);
				dynamic_array<FBXComponent*>	used;
				ISO::TypeCompositeN<64>			builder(0);

				used.push_back(comp_pos);
				builder.Add<float[3]>("position");
				if (mat.flags & mat.NORMALS) {
					used.push_back(comp_norm);
					builder.Add<float[3]>("normal");
				}
				if (mat.flags & mat.UVDEFAULT) {
					used.push_back(comp_uvdefault);
					builder.Add<float[3]>("uv");
				}
				for (auto &i : mat.inputs) {
					used.push_back(components[i]);
					builder.Add<float[2]>(i);
				}
				if (mat.flags & mat.COLOURS) {
					used.push_back(comp_col);
					builder.Add<float[4]>("colour");
				}
				if (mat.tangent_uvs)
					builder.Add<float[4]>("tangent");

				if (skin) {
					builder.Add<float[4]>("weights");
					builder.Add<uint8[4]>("bones");
				}

				Indexer<uint32>		indexer(ni);
				indexer.SetIndices(vert_indices, nv);
				for (int i = 1; i < used.size(); i++)
					used[i]->Index(indexer, vert_indices, poly_indices);

				int		i	= mb->submeshes.Count();
				mb.SetMaterial((ISO_ptr<void>&)mat.technique, mat.parameters ? AnythingToStruct(*mat.parameters) : ISO_NULL);
				FBXAddSubMesh(indexer, mb, builder.Duplicate(), used, vert_indices, poly_indices, mat_filter, skin);

				if (mat.tangent_uvs) {
					for (int end = mb->submeshes.Count(); i < end; i++) {
						SubMesh *submesh = mb->submeshes[i];
							GenerateTangents(submesh, mat.tangent_uvs);
					}
				}
			}

			delete[] mat_filter;
			delete[] poly_indices;

			mb->UpdateExtents();
			return move(mb);
		}

	} else if (str(type) == "Limb") {
		//joint
	}

	return ISO_NULL;
}

//-----------------------------------------------------------------------------
//	FBXFileHandler
//-----------------------------------------------------------------------------

ISO_ptr<void> GetCollision(ISO_ptr<void> &p, const float3x4 &nodemat, const FBXSettings &settings) {
	ISO::Browser2	b(p);
	const char *name	= b["NAME"].GetString();
	const char *type	= b["TYPE"].GetString();
	float3x4	geommat	= settings.GetGeometryMatrix(GetProperties(b));
	int			mask	= 0;

	if (const char *comma = strchr(name, ','))
		sscanf(comma + 1, "%i", &mask);

	if (str(type) == "Mesh") {
		ISO::Browser2 bg	= b["Geometry"];
		if (!bg)
			bg = b;

		ISO::Browser bv	= bg["Vertices"];
		ISO::Browser bi	= bg["PolygonVertexIndex"];
		int			nv	= bv.Count() / 3;

		auto		vp		= ISO_conversion::convert<ISO_openarray<double> >(bv);
		double3p	*verts	= (double3p*)vp->begin();
		auto		r		= get_extent(verts, verts + nv);

//		if (nv == 8) {
			// probably BOX
			ISO_ptr<Collision_OBB>	p(NULL);
			float3		va = to<float>(r.a), vb = to<float>(r.b);
			p->obb		= nodemat * geommat * cuboid(position3(va), position3(vb)).inv_matrix();
			p->mask		= mask;
			return p;
//		}

	}
	return ISO_NULL;
}

ISO_ptr<void> GetCamera(ISO_ptr<void> &p, const float3x4 &nodemat, const FBXSettings &settings) {
	ISO::Browser2	b(p);
	const char		*name	= b["NAME"].GetString();
	ISO::Browser		attr	= b["NodeAttribute"];
	ISO::Browser		props	= GetProperties(attr);

	ISO_ptr<ent::Camera>	cam(name);

	cam->zoom		= props["FieldOfView"].GetFloat() / props["FocalLength"].GetFloat();
	cam->clip_start = props["NearPlane"].GetFloat() * settings.scale;
	cam->clip_end	= props["FarPlane"].GetFloat() * settings.scale;

	return cam;
}


struct MAXLightID {
	uint32		ida, idb;
	int			type;
	operator Class_ID() const { return Class_ID(ida, idb); }
} light_ids[] = {
	{0x32375fcc, 0xb025cf0,		ent::Light2::OMNI},			// LS_POINT_LIGHT_ID
	{0x78207401, 0x357f1d58,	ent::Light2::DIRECTIONAL},	// LS_LINEAR_LIGHT_ID
	{0x36507d92, 0x105a1a47,	ent::Light2::OMNI},			// LS_AREA_LIGHT_ID
	{0x5bcc6d42, 0xc4f430e,		ent::Light2::OMNI},			// LS_DISC_LIGHT_ID
	{0x7ca93582, 0x1abb6b32,	ent::Light2::OMNI},			// LS_SPHERE_LIGHT_ID
	{0x46f634e3, 0xa327aaf,		ent::Light2::OMNI},			// LS_CYLINDER_LIGHT_ID
};

struct LightParams {
	enum TYPE { OMNI = 0, DIR = 1, SPOT = 2 };
	int		LightType;
	colour	Color;
	float	Intensity;

	bool			EnableFarAttenuation;
	distance_unit	FarAttenuationStart, FarAttenuationEnd;

	bool			EnableNearAttenuation;
	distance_unit	NearAttenuationStart, NearAttenuationEnd;

	int				DecayType;
	distance_unit	DecayStart;
	bool			CastShadows;

	float			InnerAngle;

	LightParams(const FBXSettings &settings, ISO::Browser b) {
		read_props(settings, b,
			F(LightType), F(Color), F(Intensity),
			F(EnableFarAttenuation), F(FarAttenuationStart), F(FarAttenuationEnd),
			F(EnableNearAttenuation), F(NearAttenuationStart), F(NearAttenuationEnd),
			F(DecayType), F(DecayStart), F(CastShadows), F(InnerAngle)
		);
	}
};

ISO_ptr<void> GetLight(ISO_ptr<void> &p, const float3x4 &nodemat, const FBXSettings &settings) {
	static uint8 type_lookup[] = {
		ent::Light2::OMNI,
		ent::Light2::DIRECTIONAL,
		ent::Light2::SPOT
	};

	ISO::Browser2	b(p);
	const char		*name	= b["NAME"].GetString();
	ISO::Browser		attr	= b["NodeAttribute"];
	ISO::Browser		props	= GetProperties(attr);

	LightParams		params(settings, props);

	ISO_ptr<ent::Light2>	light(name);
	light->matrix			= nodemat;
	int				type	= type_lookup[params.LightType];

	if (ISO::Browser props2 = props["3dsMax"]) {
		MAXLightID	*m = find(light_ids, Class_ID(props2["ClassIDa"].GetInt(), props2["ClassIDb"].GetInt()));
		if (m != end(light_ids))
			type = m->type;
	}

	if (params.CastShadows)
		type |= ent::Light2::SHADOW;

	light->colour	= params.Color.rgb * params.Intensity / 100;
	light->type		= ent::Light2::TYPE(type);
	light->range	= params.FarAttenuationEnd;
	light->spread	= degrees(params.InnerAngle);

	return light;
}

BoneMapping kaydara_bones[] = {
	{"Hips",				BONE_HIPS							},
	{"Spine",				BONE_SPINE1							},
	{"Spine1",				BONE_SPINE2							},
	{"Spine2",				BONE_SPINE3							},
	{"Neck",				BONE_NECK1							},
	{"Head",				BONE_HEAD							},
	{"LeftShoulder",		BONE_LEFT_SHOULDER					},
	{"LeftArm",				BONE_LEFT_ARM						},
	{"LeftArmRoll",			BONE_LEFT_ARM |  BONE_ROLL			},
	{"LeftForeArm",			BONE_LEFT_FOREARM					},
	{"LeftForeArmRoll",		BONE_LEFT_FOREARM | BONE_ROLL		},
	{"LeftHand",			BONE_LEFT_HAND						},
	{"LeftThumb1",			BONE_LEFT_THUMB1					},
	{"LeftThumb2",			BONE_LEFT_THUMB2					},
	{"LeftThumb3",			BONE_LEFT_THUMB3					},
	{"LeftIndex1",			BONE_LEFT_INDEX1					},
	{"LeftIndex2",			BONE_LEFT_INDEX2					},
	{"LeftIndex3",			BONE_LEFT_INDEX3					},
	{"LeftMiddle1",			BONE_LEFT_MIDDLE1					},
	{"LeftMiddle2",			BONE_LEFT_MIDDLE2					},
	{"LeftMiddle3",			BONE_LEFT_MIDDLE3					},
	{"LeftRing1",			BONE_LEFT_RING1						},
	{"LeftRing2",			BONE_LEFT_RING2						},
	{"LeftRing3",			BONE_LEFT_RING3						},
	{"LeftPinky1",			BONE_LEFT_PINKY1					},
	{"LeftPinky2",			BONE_LEFT_PINKY2					},
	{"LeftPinky3",			BONE_LEFT_PINKY3					},
	{"RightShoulder",		BONE_RIGHT_SHOULDER					},
	{"RightArm",			BONE_RIGHT_ARM						},
	{"RightArmRoll",		BONE_RIGHT_ARM | BONE_ROLL			},
	{"RightForeArm",		BONE_RIGHT_FOREARM					},
	{"RightForeArmRoll",	BONE_RIGHT_FOREARM | BONE_ROLL		},
	{"RightHand",			BONE_RIGHT_HAND						},
	{"RightThumb1",			BONE_RIGHT_THUMB1					},
	{"RightThumb2",			BONE_RIGHT_THUMB2					},
	{"RightThumb3",			BONE_RIGHT_THUMB3					},
	{"RightIndex1",			BONE_RIGHT_INDEX1					},
	{"RightIndex2",			BONE_RIGHT_INDEX2					},
	{"RightIndex3",			BONE_RIGHT_INDEX3					},
	{"RightMiddle1",		BONE_RIGHT_MIDDLE1					},
	{"RightMiddle2",		BONE_RIGHT_MIDDLE2					},
	{"RightMiddle3",		BONE_RIGHT_MIDDLE3					},
	{"RightRing1",			BONE_RIGHT_RING1					},
	{"RightRing2",			BONE_RIGHT_RING2					},
	{"RightRing3",			BONE_RIGHT_RING3					},
	{"RightPinky1",			BONE_RIGHT_PINKY1					},
	{"RightPinky2",			BONE_RIGHT_PINKY2					},
	{"RightPinky3",			BONE_RIGHT_PINKY3					},
	{"LeftUpLeg",			BONE_LEFT_THIGH						},
	{"LeftUpLegRoll",		BONE_LEFT_THIGH | BONE_ROLL			},
	{"LeftLeg",				BONE_LEFT_LEG						},
	{"LeftLegRoll",			BONE_LEFT_LEG | BONE_ROLL			},
	{"LeftFoot",			BONE_LEFT_FOOT						},
	{"LeftToes",			BONE_LEFT_TOES						},
	{"RightUpLeg",			BONE_RIGHT_THIGH					},
	{"RightUpLegRoll",		BONE_RIGHT_THIGH | BONE_ROLL		},
	{"RightLeg",			BONE_RIGHT_LEG						},
	{"RightLegRoll",		BONE_RIGHT_LEG | BONE_ROLL			},
	{"RightFoot",			BONE_RIGHT_FOOT						},
	{"RightToes",			BONE_RIGHT_TOES						},

	{"LeftHandThumb1",		BONE_LEFT_THUMB1					},
	{"LeftHandThumb2",		BONE_LEFT_THUMB2					},
	{"LeftHandThumb3",		BONE_LEFT_THUMB3					},
	{"LeftHandIndex1",		BONE_LEFT_INDEX1					},
	{"LeftHandIndex2",		BONE_LEFT_INDEX2					},
	{"LeftHandIndex3",		BONE_LEFT_INDEX3					},
	{"LeftHandMiddle1",		BONE_LEFT_MIDDLE1					},
	{"LeftHandMiddle2",		BONE_LEFT_MIDDLE2					},
	{"LeftHandMiddle3",		BONE_LEFT_MIDDLE3					},
	{"LeftHandRing1",		BONE_LEFT_RING1						},
	{"LeftHandRing2",		BONE_LEFT_RING2						},
	{"LeftHandRing3",		BONE_LEFT_RING3						},
	{"LeftHandPinky1",		BONE_LEFT_PINKY1					},
	{"LeftHandPinky2",		BONE_LEFT_PINKY2					},
	{"LeftHandPinky3",		BONE_LEFT_PINKY3					},

	{"RightHandThumb1",		BONE_RIGHT_THUMB1					},
	{"RightHandThumb2",		BONE_RIGHT_THUMB2					},
	{"RightHandThumb3",		BONE_RIGHT_THUMB3					},
	{"RightHandIndex1",		BONE_RIGHT_INDEX1					},
	{"RightHandIndex2",		BONE_RIGHT_INDEX2					},
	{"RightHandIndex3",		BONE_RIGHT_INDEX3					},
	{"RightHandMiddle1",	BONE_RIGHT_MIDDLE1					},
	{"RightHandMiddle2",	BONE_RIGHT_MIDDLE2					},
	{"RightHandMiddle3",	BONE_RIGHT_MIDDLE3					},
	{"RightHandRing1",		BONE_RIGHT_RING1					},
	{"RightHandRing2",		BONE_RIGHT_RING2					},
	{"RightHandRing3",		BONE_RIGHT_RING3					},
	{"RightHandPinky1",		BONE_RIGHT_PINKY1					},
	{"RightHandPinky2",		BONE_RIGHT_PINKY2					},
	{"RightHandPinky3",		BONE_RIGHT_PINKY3					},

};

BoneMapping humanoid_bones[] = {
	{"Pelvis",				BONE_HIPS							},
	{"R_Thigh",				BONE_RIGHT_THIGH					},
	{"R_Leg",				BONE_RIGHT_LEG						},
	{"R_Foot",				BONE_RIGHT_FOOT						},
	{"R_Toe",				BONE_RIGHT_TOES						},
	{"L_Thigh",				BONE_LEFT_THIGH						},
	{"L_Leg",				BONE_LEFT_LEG						},
	{"L_Foot",				BONE_LEFT_FOOT						},
	{"L_Toe",				BONE_LEFT_TOES						},
	{"Spine_0",				BONE_SPINE1							},
	{"Spine_1",				BONE_SPINE2							},
	{"Spine_2",				BONE_SPINE3							},
	{"R_Clavicle",			BONE_RIGHT_CLAVICLE					},
	{"R_Shoulder",			BONE_RIGHT_SHOULDER					},
	{"R_Forearm",			BONE_RIGHT_FOREARM					},
	{"R_ForearmRoll",		BONE_RIGHT_FOREARM | BONE_ROLL		},
	{"R_Hand",				BONE_RIGHT_HAND						},
	{"R_Index_A",			BONE_RIGHT_INDEX1					},
	{"R_Middle_A",			BONE_RIGHT_MIDDLE1					},
	{"R_Thumb_B",			BONE_RIGHT_THUMB2					},
	{"L_Clavicle",			BONE_LEFT_CLAVICLE					},
	{"L_Shoulder",			BONE_LEFT_SHOULDER					},
	{"L_Forearm",			BONE_LEFT_FOREARM					},
	{"L_ForearmRoll",		BONE_LEFT_FOREARM | BONE_ROLL		},
	{"L_Hand",				BONE_LEFT_HAND						},
	{"L_Index_A",			BONE_LEFT_INDEX1					},
	{"L_Middle_A",			BONE_LEFT_MIDDLE1					},
	{"L_Thumb_B",			BONE_LEFT_THUMB2					},
	{"Neck_1",				BONE_NECK1							},
	{"Head",				BONE_HEAD							},
};

void FixBoneName(ISO_ptr<void> p) {
	const char *name = p.ID().get_tag();
	BONE	b = LookupBone(humanoid_bones, name);
	if (!b) {
		if (auto colon = string_find(name, ':')) {
			b = LookupBone(kaydara_bones, colon + 1);
		}
	}

	if (b)
		p.SetID(GetName(b));
}

void GetBones(BasePose *pose, ISO_ptr<Bone> parent, ISO::Browser2 b, FBXSettings &settings) {
	if (double *d = *b["PoseNode"]["Matrix"]) {
		float3x4	nodemat(
			float3{(float)d[ 0], (float)d[ 1], (float)d[2]},
			float3{(float)d[ 4], (float)d[ 5], (float)d[6]},
			float3{(float)d[ 8], (float)d[ 9], (float)d[10]},
			float3{(float)d[12], (float)d[13], (float)d[14]}
		);

		nodemat.w *= settings.scale;

		parent->basepose	= rotate_in_x(pi * half) * nodemat * rotate_in_x(-pi * half);
	} else {
		ISO::Browser	props	= GetProperties(b);
		parent->basepose	= settings.GetLocalMatrix(props);
	}

	pose->Append(parent);
	for (auto &&i : b) {
		if (i.GetName() == "Model") {

			ISO::Browser2	b(i);
			const char *name = b["NAME"].GetString();
			const char *type = b["TYPE"].GetString();

			if (str(type) == "LimbNode") {	// bone
				settings.bone_lookup[name] = b;
				ISO_ptr<Bone>	bone(name);
				bone->parent	= parent;
				GetBones(pose, bone, i, settings);
			}
		}

	}
}

ISO_ptr<void> LookForBasepose(anything &objects, FBXSettings &settings) {
	for (auto &i : objects) {
		if (i.ID() == "Model") {
			const char *type = ISO::Browser2(i)["TYPE"].GetString();
			if (type == "LimbNode"_cstr || type == "Root"_cstr)
				return i;
		}
	}
	return ISO_NULL;
}

void AddNodes(anything &children, anything &objects, float3x4 parentmat, FBXSettings &settings) {
	if (auto i = LookForBasepose(objects, settings)) {
		ISO::Browser2		b(i);
		const char *		name = b["NAME"].GetString();
		ISO_ptr<BasePose>	pose(0);
		settings.bone_lookup[name] = b;
		GetBones(pose, ISO_ptr<Bone>(name), b, settings);
		children.Append(pose);
//		settings.pose_model	= i;
		settings.pose		= pose;
	}

	for (auto &i : objects) {
		if (i.ID() == "Model") {
			ISO::Browser2	b(i);
			const char *name = b["NAME"].GetString();
			const char *type = b["TYPE"].GetString();

			if (istr(name).begins("ref_"))
				continue;

			ISO::Browser	props	= GetProperties(b);
			float3x4		nodemat = settings.GetLocalMatrix(props) * parentmat;

			if (istr(name).begins("coll_")) {
				if (ISO_ptr<void> p = GetCollision(i, nodemat, settings))
					children.Append(p);
				AddNodes(children, *(anything*)i, nodemat, settings);

			} else if (str(type) == "Null") {
				if (istr(name).begins("ATTACH_")) {
					ISO_ptr<ent::Attachment>	p(name);
					const char *d = string_end(name);
					while (is_digit(d[-1]))
						--d;
					if (*d)
						from_string(d, p->id);
					p->matrix = nodemat;
					children.Append(p);
					AddNodes(children, *(anything*)i, nodemat, settings);
				} else {
					ISO_ptr<Node>	node(name);
					node->matrix = nodemat;
					children.Append(node);
					AddNodes(node->children, *(anything*)i, identity, settings);
				}

			} else if (str(type) == "Camera") {
				if (ISO_ptr<void> p = GetCamera(i, nodemat, settings))
					children.Append(p);
				AddNodes(children, *(anything*)i, nodemat, settings);

			} else if (str(type) == "Light") {
				if (ISO_ptr<void> p = GetLight(i, nodemat, settings))
					children.Append(p);
				AddNodes(children, *(anything*)i, nodemat, settings);

			} else if (str(type) == "Root") {	// root of bones

				ISO_TRACE("Root\n");

			} else if (str(type) == "LimbNode") {	// bone

			} else if (str(type) == "Mesh" || str(type) == "Line") {
				if (auto j = LookForBasepose(*(anything*)i, settings)) {
					ISO::Browser2		b(j);
					const char *		name = b["NAME"].GetString();
					ISO_ptr<BasePose>	pose(0);
					settings.bone_lookup[name] = b;
					GetBones(pose, ISO_ptr<Bone>(name), b, settings);
					children.Append(pose);
//					settings.pose_model	= j;
					settings.pose		= pose;
				}

				if (ISO_ptr<Model3> model = FBXMakeModel(b, type, settings)) {
					ISO_ptr<Node>	node(name);
					node->matrix = nodemat;
					children.Append(node);
					node->children.Append(model);
				}

				if (settings.pose) {
					if (ISO::Browser b2 = b["Geometry"])
						b = b2;

					if (ISO::Browser b2 = b["Deformer"]) {
						for (auto i : b2) {
							if (i.GetName() == "Deformer") {
								if (ISO::Browser b3 = i["TransformLink"]) {
									const char *name	= i["Model"]["NAME"].GetString();
									int			bone	= settings.pose->GetIndex(name);

									ISO_ptr<ISO_openarray<double> >	m	= ISO_conversion::convert<ISO_openarray<double> >(b3);
									ISO_ASSERT(bone >= 0 && m);
									double		*d		= *m;
									float3x4	bonemat(
										float3{(float)d[ 0], (float)d[ 1], (float)d[ 2]},
										float3{(float)d[ 4], (float)d[ 5], (float)d[ 6]},
										float3{(float)d[ 8], (float)d[ 9], (float)d[10]},
										float3{(float)d[12], (float)d[13], (float)d[14]}
									);

									bonemat.w *= settings.scale;
									bonemat	= rotate_in_x(pi * half) * bonemat * rotate_in_x(-pi * half);
									bonemat	= bonemat / nodemat;

									(*settings.pose)[bone]->basepose = bonemat;
								}
							}
						}
						for (auto &i : *settings.pose) {
							if (len2(i->basepose.x) == 0) {
								ISO::Browser		props	= GetProperties(settings.bone_lookup[i.ID()].or_default());
								float3x4		nodemat = settings.GetLocalMatrix(props);
								if (i->parent)
									nodemat = float3x4(i->parent->basepose) * nodemat;
								i->basepose	= nodemat;
							}
						}
					}
				}

			} else {
				ISO_ptr<Node>	node(name);
				node->matrix = nodemat;
				children.Append(node);
				AddNodes(node->children, *(anything*)i, identity, settings);
			}
		}
	}
}

dynamic_array<float> GetAnimation1D(ISO::Browser b, const FBXSettings &settings) {
	dynamic_array<float>	out;
	int			key_ver		= b["KeyVer"].GetInt();

	if (key_ver == 4009) {
		ISO::Browser	key			= b["KeyTime"];
		ISO::Browser	key_value	= b["KeyValueFloat"];
		uint64		end_time	= key[key.Count() - 1].Get(uint64(0));
		int			num_keys	= end_time * 30 / settings.time_scale;

		out.resize(num_keys);

		uint64		time0		= key[0].Get(uint64(0));
		float		val0		= key_value[0].GetFloat();
		uint64		time1		= key[1].Get(uint64(0));
		float		val1		= key_value[1].GetFloat();

		for (int i = 1, i2 = 0; i2 < num_keys; i2++) {
			uint64	t	= i2 * settings.time_scale / 30;
			while (t > time1) {
				++i;
				time0	= time1;
				val0	= val1;
				time1	= key[i].Get(uint64(0));
				val1	= key_value[i].GetFloat();
			}
			out[i2] = lerp(val0, val1, float(t - time0) / (time1 - time0));
		}

	} else if (ISO::Browser	key = b["Key"]) {
		int			step		= key_ver == 4001 ? 4 : key_ver == 4005 ? 5 : 0;

		if (step != 0) {
			uint64		end_time	= key[key.Count() - step].Get(uint64(0));
			int			num_keys	= end_time * 30 / settings.time_scale;
			out.resize(num_keys);

			uint64		time0		= key[0].Get(uint64(0));
			float		val0		= key[1].GetFloat();
			uint64		time1		= key[step + 0].Get(uint64(0));
			float		val1		= key[step + 1].GetFloat();

			for (int i = 0, i2 = 0; i2 < num_keys; i2++) {
				uint64	t	= i2 * settings.time_scale / 30;
				while (t > time1) {
					i		+= step;
					time0	= time1;
					val0	= val1;
					time1	= key[i + step + 0].Get(uint64(0));
					val1	= key[i + step + 1].GetFloat();
				}
				out[i2] = lerp(val0, val1, float(t - time0) / (time1 - time0));
			}
		}
	}
	return out;
}

dynamic_array<float3> Interleave3D(const dynamic_array<float>&x, const dynamic_array<float> &y, const dynamic_array<float> &z) {
	dynamic_array<float3>	a(x.size());
	if (auto *xi = x.begin()) {
		for (auto &i : a)
			i.x = *xi++;
	}

	if (auto *yi = y.begin()) {
		for (auto &i : a)
			i.y = *yi++;
	}

	if (auto *zi = z.begin()) {
		for (auto &i : a)
			i.z = *zi++;
	}
	return a;
}

dynamic_array<float3> GetAnimation3D(ISO::Browser b, const FBXSettings &settings) {
	dynamic_array<float>	x, y, z;
	for (auto i : ISO::Browser(b)) {
		if (i.GetName() == "Channel") {
			const char *channel = i[0].GetString();
			if (channel == cstr("X"))
				x = GetAnimation1D(i[1], settings);
			else if (channel == cstr("Y"))
				y = GetAnimation1D(i[1], settings);
			else if (channel == cstr("Z"))
				z = GetAnimation1D(i[1], settings);
		}
	}
	return Interleave3D(x, y, z);
}


void GetAnimationTransform(Animation &anim, ISO::Browser b, const FBXSettings &settings) {

	for (auto i : ISO::Browser(b)) {
		if (i.GetName() == "Channel") {
			const char *channel = i[0].GetString();

			if (channel == cstr("T")) {
				if (dynamic_array<float3> t = GetAnimation3D(i[1], settings)) {
					ISO_ptr<ISO_openarray<float3p> >	p("pos", make_deferred<op_mul>(t, (float)settings.scale));
					anim.Append(p);
				}

			} else if (channel == cstr("R")) {
				if (dynamic_array<float3> r = GetAnimation3D(i[1], settings)) {
//					ISO_ptr<ISO_openarray<compressed_quaternion> >		p("comp_rot", r.size32());
					ISO_ptr<ISO_openarray<float4p> >					p("rot", r.size32());
					transform(r, *p, [](float3 rot){ return FBXRotation(rot).v; });
					anim.Append(p);
				}

			} else if (channel == cstr("S")) {
				if (dynamic_array<float3> s = GetAnimation3D(i[1], settings)) {
					ISO_ptr<ISO_openarray<float3p> >	p("scale", s);
					anim.Append(p);
				}
			}
		}
	}

}

dynamic_array<float3> GetLocalAnimation(const ISO::Browser &b, const FBXSettings &settings) {
	ISO::Browser		props	= GetProperties(b)["d"];
	return Interleave3D(
		GetAnimation1D(props["X"], settings),
		GetAnimation1D(props["Y"], settings),
		GetAnimation1D(props["Z"], settings)
	);
}

ISO_ptr<anything> GetAnimation(ISO_ptr<void> takes, const FBXSettings &settings) {
	ISO_ptr<anything>	all_anim("animations");
	for (auto i : ISO::Browser(takes)) {
		if (i.GetName() == "Take") {
			ISO_ptr<Animation>	take(i[0].GetString());
			for (auto j : i[1]) {
				if (j.GetName() == "Model") {
					const char *name = j[0].GetString();
					ISO_ptr<Animation>	bone(name);

					for (auto k : j[1]) {
						if (k.GetName() == "Channel") {
							const char *channel = k[0].GetString();
							if (channel == cstr("Transform")) {
								GetAnimationTransform(*bone, k[1], settings);
							}
						}
					}
					if (bone->Count())
						take->Append(bone);
				}
			}

			if (!take->Count() && settings.pose) {
				for (auto &i : *settings.pose) {
					ISO_ptr<Animation>	anim(i.ID());
					ISO::Browser			props	= GetProperties(settings.bone_lookup[i.ID()].or_default());
					if (ISO::Browser b = props["Lcl Translation"]) {
						if (dynamic_array<float3> t = GetLocalAnimation(b, settings)) {
							ISO_ptr<ISO_openarray<float3p>>		p("pos", make_deferred<op_mul>(t, (float)settings.scale));
							anim->Append(p);
						}
					}
					if (ISO::Browser b = props["Lcl Rotation"]) {
						if (dynamic_array<float3> t = GetLocalAnimation(b, settings)) {
							ISO_ptr<ISO_openarray<float4p>>		p("rot", t.size32());
							transform(t, *p, [](float3 rot){ return FBXRotation(rot).v; });
							anim->Append(p);
						}
					}
					if (ISO::Browser b = props["Lcl Scaling"]) {
						if (dynamic_array<float3> t = GetLocalAnimation(b, settings)) {
							ISO_ptr<ISO_openarray<float3p>>		p("scale", t);
							anim->Append(p);
						}
					}
					if (anim->Count())
						take->Append(anim);
				}
			}

			if (take->Count())
				all_anim->Append(take);
		}
	}
	return all_anim;
}

class FBXFileHandler : FileHandler {
	const char*		GetExt() override { return "fbx";				}
	const char*		GetDescription() override { return "Kaydara Filmbox";	}
	int				Check(istream_ref file) override {
		file.seek(0);
		BFBXheader	h;
		return file.read(h) && h.valid() ? CHECK_PROBABLE : CHECK_NO_OPINION;
	}

	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		ISO_ptr<anything>	p(id);
		ISO::Browser2		settings;
		Objects				objects;
		ISO_ptr<void>		takes;

		objects[0]			= p;
		objects[make_id("Scene")] = p;
		objects[make_id("Model::Scene")] = p;

		streamptr			end	= file.length();
		BFBXheader			bh	= file.get();

		if (bh.valid()) {
			//binary
			FBXBinaryReader		fbx(file);
			BFBXentity			ent;
			while (file.tell() < end && file.read(ent) && ent.next) {
				pascal_string	tok = file.get();
				if (tok == "GlobalSettings")
					settings = GetProperties(fbx.ReadEntity(tok, ent));
				else if (tok == "Definitions")
					fbx.ReadDefinitions(tok, ent.next);
				else if (tok == "Objects")
					fbx.ReadObjects(tok, ent.next, objects);
				else if (tok == "Connections")
					fbx.ReadConnections(tok, ent.next, objects);
				else if (tok == "Takes")
					takes = fbx.ReadEntity(tok, ent);

				file.seek(ent.next);
			}

		} else {
			//ascii
			file.seek(0);
			FBXAsciiReader		fbx(file);
			while (!fbx.eof()) {
				string			tok = fbx.ReadName();
				if (tok == "GlobalSettings")
					settings = GetProperties(fbx.ReadEntity(tok));
				else if (tok == "Objects")
					fbx.ReadObjects(tok, objects);
				else if (tok == "Connections")
					fbx.ReadConnections(tok, objects);
				else if (tok == "Takes")
					takes = fbx.ReadEntity(tok);
				else
					fbx.ReadEntity(none);	// discard

			}
		}

#if 1
		for (auto &o : objects) {
			if (o.ID() == "Pose") {
				for (auto &i : *o) {
					if (i.ID() == "PoseNode") {
						uint64	nodeid = ISO::Browser(i)["Node"].Get(uint64(0));
						objects[nodeid].put()->Append(i);
					}
				}
			}
		}
#endif
		FBXSettings		fbx_settings(settings);
		ISO_ptr<Scene>	scene(id);
		scene->root.Create(0);
		scene->root->matrix = identity;
		AddNodes(scene->root->children, *p, identity, fbx_settings);

		auto	anim = GetAnimation(takes, fbx_settings);
		if (anim->Count()) {

			for (auto &i : *anim) {
				for (auto &j : *(Animation*)i)
					FixBoneName(j);
			}

			if (scene->root->children.Count() == 0) {
				if (anim->Count() == 1)
					return (*anim)[0];

				return anim;
			}

			scene->root->children.Append(anim);
		}

		if (fbx_settings.pose) {
			for (auto &i : *fbx_settings.pose)
				FixBoneName(i);
		}

#if 0	// dump objects to scene for debugging
		for (auto &i : objects)
			scene->root->children.Append(i);
		scene->root->children.Append(takes);
#endif
		return scene;
	}
} fbx;

class RAWFBXFileHandler : FileHandler {
//	const char*		GetExt() override { return "fbx";				}
	const char*		GetDescription() override { return "RAW Kaydara Filmbox";	}
	int				Check(istream_ref file) override {
		file.seek(0);
		BFBXheader	h;
		if (file.read(h) && h.valid())
			return CHECK_PROBABLE;
		
		file.seek(0);
		text_reader<reader_intf>	text(file);
		if (skip_whitespace(text) == ';') {
			return CHECK_POSSIBLE;
		}
		return CHECK_NO_OPINION;
	}

	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		streamptr			end = file.length();

		BFBXheader	bh = file.get();
		if (bh.valid()) {
			//binary
			FBXBinaryReader		fbx(file, true);
			return fbx.ReadBlock(id, end);
		} else {
			//ascii
			file.seek(0);
			FBXAsciiReader		fbx(file, true);
			ISO_ptr<anything>	p(id);
			while (!file.eof())
				p->Append(fbx.ReadEntity(fbx.ReadName()));
			return p;
		}
	}
} raw_fbx;


