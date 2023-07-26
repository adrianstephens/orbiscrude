#include "iso/iso_files.h"
#include "iso/iso_script.h"
#include "filetypes/3d/model_utils.h"
#include "base/algorithm.h"
#include "utilities.h"

using namespace iso;

class PLYFileHandler : FileHandler {
	static	const char		*type_names[], *old_type_names[];
	static const ISO::Type	*iso_types[];
	static	int				type_size[];

	struct vertex			{ float3p position;	};
	static struct ISO_type_vertex : ISO::TypeCompositeN<1> {
		ISO_type_vertex() { fields[0].set("position", &vertex::position); }
	} iso_type_vertex;


	static int				GetType(const char *s);
	char*					ReadLine(istream_ref file, char *line, int maxlen) const;

	const char*		GetExt() override { return "ply";							}
	const char*		GetDescription() override { return "Stanford Triangle Format";	}
	int				Check(istream_ref file) override;
	ISO_ptr<void>	Read(tag id, istream_ref file) override;

} ply;

PLYFileHandler::ISO_type_vertex PLYFileHandler::iso_type_vertex;

const char *PLYFileHandler::type_names[] = {  // names of scalar types
	"invalid",
	"int8", "int16", "int32",
	"uint8", "uint16", "uint32",
	"float32", "float64",
};

const char *PLYFileHandler::old_type_names[] = {  // old names of types for backward compatability
	"invalid",
	"char", "short", "int",
	"uchar", "ushort", "uint",
	"float", "double",
};

const ISO::Type *PLYFileHandler::iso_types[] = {
	0,
	ISO::getdef<int8>(), ISO::getdef<int16>(), ISO::getdef<int32>(),
	ISO::getdef<uint8>(), ISO::getdef<uint16>(), ISO::getdef<uint32>(),
	ISO::getdef<float>(), ISO::getdef<double>(),
};

int PLYFileHandler::type_size[] = {
	0,
	1, 2, 4,
	1, 2, 4,
	4, 8
};


int PLYFileHandler::Check(istream_ref file) {
	file.seek(0);
	char	line[256];
	return str(ReadLine(file, line, 256)) == "ply" ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
}

char* PLYFileHandler::ReadLine(istream_ref file, char *line, int maxlen) const {
	int	i, c;
	do c = file.getc(); while (c == '\r' || c == '\n');

	if (c == EOF)
		return NULL;

	for (i = 0; i < maxlen && c && c != EOF && c != '\n' && c != '\r'; c = file.getc(), i++) {
		line[i] = c;
	}
	if (i == maxlen)
		return NULL;
	line[i] = 0;
	return line;
}

char* FindSpace(char *s) {
	if (s) {
		char	c;
		while ((c = *s) && c != ' ' && c != '\t')
			s++;
		if (c == 0)
			return 0;
		*s++ = 0;
	}
	return s;
}

int PLYFileHandler::GetType(const char *s) {
	int		i	= iso::find(type_names, str(s)) - type_names;
	if (i == num_elements(type_names))
		i = iso::find(old_type_names, str(s)) - old_type_names;
	if (i == num_elements(old_type_names))
		i = -1;
	return i;
}

ISO_ptr<void> PLYFileHandler::Read(tag id, istream_ref file) {
	char		line[256];
	int			format	= -1;
	float		version	= 0;

	ISO_ptr<void>	texture;

	ISO_ptr<anything>	a(id);

	struct CompositeBuilder : ISO::TypeCompositeN<64> {
		string		name;
		int			array;

		CompositeBuilder() : ISO::TypeCompositeN<64>(0) {}
		void		SetName(const char *_name)	{ name	= _name; }
		void		SetArray(int _array)		{ array = _array; }

		operator	ISO_ptr<void>() const {
			return MakePtr(new ISO::TypeArray(new ISO::TypeUserSave(name, Duplicate()), array));
		}
	} builder;

	if (str(ReadLine(file, line, 256)) != "ply")
		return ISO_NULL;

	for (;;) {
		char	*s = FindSpace(ReadLine(file, line, 256));

		if (str(line) == "end_header") {
			break;

		} else if (str(line) == "comment") {
			if (str(s).begins("TextureFile ")) {
				if (filename fn	= FindAbsolute(FindSpace(s)))
					texture = ISO::MakePtrExternal(ISO::user_types.Find("bitmap"), fn);
			}
			continue;

		} else if (str(line) == "format") {
			char	*s2 = FindSpace(s);
			format	= str(s) == "binary_little_endian"	? 0
					: str(s) == "binary_big_endian"		? 1
					: str(s) == "ascii"					? 2
					: -1;
			sscanf(s2, "%g", &version);

		} else if (str(line) == "element") {
			if (builder.Count()) {
				a->Append(builder);
				builder.Reset();
			}
			builder.SetArray(from_string<int>(FindSpace(s)));
			builder.SetName(s);

		} else if (str(line) == "property") {
			char	*s2		= FindSpace(s);
			int		list	= -1;
			if (str(s) == "list") {
				s		= s2;
				s2		= FindSpace(s);
				list	= GetType(s);
				s		= s2;
				s2		= FindSpace(s);
			}
			int		t	= GetType(s);
			if (t >= 0) {
				const ISO::Type *type = iso_types[t];
				if (list >= 0)
					type = new ISO::TypeOpenArray(type);
				builder.Add(type, s2, true);
			}

		} else if (str(line) == "obj_info") {
			char	*s2		= FindSpace(s);
			float	v;
			sscanf(s2, "%g", &v);
			a->Append(ISO_ptr<float>(s, v));
		}
	}

	a->Append(builder);

	for (int i = 0, n = a->Count(); i < n; i++) {
		ISO_ptr<void>	p = (*a)[i];
		ISO::Browser		b(p);
		if (b.GetType() != ISO::ARRAY)
			continue;
		if (format < 2) {
			if (p.GetType()->IsPlainData()) {
				file.readbuff(p, p.GetType()->GetSize());
			} else {
				for (int i = 0, n = b.Count(); i < n; i++) {
					ISO::Browser	b2	= b[i];
					if (b2.GetTypeDef()->IsPlainData()) {
						file.readbuff(b2, b2.GetSize());
					} else for (int i2 = 0, n2 = b2.Count(); i2 < n2; i2++) {
						ISO::Browser	b3	= b2[i2];
						if (b3.GetTypeDef()->IsPlainData()) {
							file.readbuff(b3, b3.GetSize());
						} else {
							int	n3 = file.getc();
							b3.Resize(n3);
							file.readbuff(*b3, n3 * b3[0].GetSize());
						}
					}
				}
			}
			if (format == 1)
				p.SetFlags(ISO::Value::ISBIGENDIAN);
			SetBigEndian(p, false);

		} else {
			for (int i = 0, n = b.Count(); i < n; i++) {
				ISO::Browser	b2	= b[i];
				for (int i2 = 0, n2 = b2.Count(); i2 < n2; i2++) {
					ISO::Browser	b3	= b2[i2];
					if (b3.GetType() == ISO::OPENARRAY) {
						int	n3;
						ISO::ScriptRead(ISO::MakeBrowser(n3), file, 0);
						b3.Resize(n3);
						for (int i3 = 0; i3 < n3; i3++)
							ISO::ScriptRead(b3[i3], file, 0);
					} else {
						ISO::ScriptRead(b2[i2], file, 0);
					}
				}
			}
		}
	}

	ISO::Browser	b(a);
	ISO::Browser	vb, ib;
	for (int i = 0, n = b.Count(); i < n; i++) {
		ISO::Browser	b2 = *b[i];
		if (b2[0].Is("vertex"))
			vb = b2;
		else if (b2[0].Is("face"))
			ib = b2;
	}

	if (vb && ib) {
		ISO::TypeComposite	*vt		= (ISO::TypeComposite*)vb[0].GetTypeDef()->SkipUser();
		int					type	= 0;

		builder.Reset();
		for (int i = 0, n = vt->Count(); i < n; i++) {
			const char *id = vt->GetID(i).get_tag();
			if (str(id) == "x") {
				if ((*vt)[i].type->Is<double>()) {
					builder.Add<double[3]>("position");
				} else {
					builder.Add<float[3]>("position");
				}
				i += 2;
			} else if (str(id) == "nx") {
				builder.Add<float[3]>("normal");
				type |= 1;
				i += 2;
			} else if (str(id) == "diffuse_red" || str(id) == "red") {
				builder.Add<uint8[3]>("colour");
				type |= 2;
				i += 2;
			} else if (str(id) == "texture_u") {
				builder.Add<float[2]>("uv");
				type |= 4;

				for (auto &uv : make_range_n(make_stride_iterator((float2p*)((uint8*)vb[0] + (*vt)[i].offset), vb[0].GetSize()), vb.Count()))
					uv.y = 1 - uv.y;

				i += 1;

			} else {
				builder.Add((*vt)[i]);
			}
		}

		const ISO::Type				*it		= ib[0].GetTypeDef();
		const ISO::TypeComposite	*itc	= (const ISO::TypeComposite*)it->SkipUser();
		int							itf		= itc->GetIndex("vertex_indices");

		static const char *techniques[] = {
			"unlit",			"lite",			"col",			"col_lit",
			"tex_unlit",		"tex_lit",		"tex_col",		"tex_col_lit",
		};

		ModelBuilder	model(id, ISO::root("data")["simple"][techniques[type]]);
		model.InitVerts(builder.Duplicate(), vb.Count());

		if (texture) {
			anything	params;
			ISO_ptr<void>	tex = MakePtr(ISO::getdef<Texture>(), "DiffuseTexture");
			*(ISO_ptr<void>*)tex = texture;
			params.Append(tex);
			model.p = AnythingToStruct(params);
		}

#if 1
		uint32	npoly	= ib.Count();
		uint32	nvert	= vb.Count();
		uint32	ntris	= 0;

		for (int i = 0; i < npoly; i++)
			ntris += ib[i][itf].Count() - 2;

		temp_array<uint32>	indices(ntris * 3);
		auto	pi = indices.begin();

		for (int i = 0; i < npoly; i++) {
			ISO::Browser		fb	= ib[i][itf];
			int				n2	= fb.Count();
			int				v0	= fb[0].GetInt(), v1 = fb[1].GetInt();
			for (int i2 = 2; i2 < n2; i2++) {
				int	v2 = fb[i2].GetInt();
				pi[0]	= v0;
				pi[1]	= v1;
				pi[2]	= v2;
				pi		+= 3;
				v1		= v2;
			}
		}
#if 0
		VertexCacheOptimizerForsyth(indices, ntris, nvert, indices);
		uint32	score	= GetCacheCost(indices, ntris);
#else
		uint32	score	= VertexCacheOptimizerHillclimber(indices, nvert, indices, 0);
#endif
		pi = indices.begin();
		for (int i = 0; i < ntris; i++, pi += 3) {
			int		v0	= pi[0], v1 = pi[1], v2 = pi[2];
			int		vb0, vb1, vb2;
			if ((vb0 = model.TryAdd(v0)) < 0 || (vb1 = model.TryAdd(v1)) < 0 || (vb2 = model.TryAdd(v2)) < 0) {
				model.Purge(vb);
				vb0 = model.TryAdd(v0);
				vb1 = model.TryAdd(v1);
				vb2 = model.TryAdd(v2);
			}
			model.AddFace(vb0, vb1, vb2);
		}

#else

		for (int i = 0, n = ib.Count(); i < n; i++) {
			ISO::Browser		fb	= ib[i][itf];
			int				n2	= fb.Count();
			int				v0	= fb[0].GetInt(), v1 = fb[1].GetInt(), vb0, vb1;

			if ((vb0 = model.TryAdd(v0)) < 0 || (vb1 = model.TryAdd(v1)) < 0) {
				model.Purge(vb);
				vb0 = model.TryAdd(v0);
				vb1 = model.TryAdd(v1);
			}

			for (int i2 = 2; i2 < n2; i2++) {
				int	v2 = fb[i2].GetInt(), vb2;
				if ((vb2 = model.TryAdd(v2)) < 0) {
					model.Purge(vb);
					vb0 = model.TryAdd(v0);
					vb1 = model.TryAdd(v1);
					vb2 = model.TryAdd(v2);
				}
				model.AddFace(vb0, vb1, vb2);
				v1 = v2;
			}
		}
#endif

		model.Purge(vb);
		model->UpdateExtents();
		return model;
	}

	return a;
}
