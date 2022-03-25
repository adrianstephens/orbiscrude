#include "maths/geometry.h"
#include "systems/mesh/model_iso.h"
#include "iso/iso_files.h"
#include "iso/iso_convert.h"
#include "iso/iso_script.h"
#include "extra/text_stream.h"
#include "model_utils.h"
#include "directory.h"

using namespace iso;


ISO_ptr<void> MakeComposite(ISO::Browser b) {
	if (int n = b.Count()) {
		ISO::TypeComposite	*comp	= new(n) ISO::TypeComposite(0);
		for (int i = 0; i < n; i++)
			comp->Add((*b[i]).GetTypeDef(), b.GetName(i));

		ISO_ptr<void>	p = MakePtr(comp);
		ISO::Browser	out(p);
		for (int i = 0; i < n; i++)
			out[i].UnsafeSet(*b[i]);
		return p;
	}
	return ISO_NULL;
}

float2 ReadFloat2(char *p) {
	string_scan	ss(p);
	return float2{ss.get(), ss.get()};
}

float3 ReadFloat3(char *p) {
	string_scan	ss(p);
	return float3{ss.get(), ss.get(), ss.get()};
}

template<typename I> auto Deduplicate(I begin, I end, uint32 *indices) {
	typedef noref_t<decltype(*begin)>	T;
	hash_map<T, int>	value_to_index_map;
	dynamic_array<T>	values;

	for (auto i = begin; i != end; ++i, ++indices) {
		auto	value = *i;
		if (value_to_index_map.check(value)) {
			*indices = value_to_index_map[value];
		} else {
			*indices = value_to_index_map[value] = values.size32();
			values.push_back(value);
		}
	}
	return values;
}


//-----------------------------------------------------------------------------
//	WaveFrontReader
//-----------------------------------------------------------------------------

struct WaveFrontReader {
	static const ISO::Type *vertex_types[];
	static const ISO::Type *vertexarray_types[];

	struct combo {
		int	v, n, t, s;
		combo() : v(0), n(0), t(0), s(0) {}
	};

	istream_ref				file;
	const filename			dir;
	fixed_string<256>		line;
	uint32					line_number;

	dynamic_array<float3p>	v;
	dynamic_array<float3p>	vn;
	dynamic_array<float2p>	vt;
	dynamic_array<combo>	faces;

	anything				mtllib;
	ISO_ptr<void>			mtl;
	ISO_ptr<Model3>			model;


	ISO_ptr<SubMesh> AddMesh(tag id, uint32 num_verts, uint32 num_indices, const ISO::TypeOpenArray *vertarray_type, ISO_ptr<void> technique, ISO_ptr<void> parameters) {
		ISO_ptr<SubMesh>	sm(id);
		sm->indices.Create(num_indices);
		sm->verts			= MakePtr(vertarray_type);
		((ISO_openarray<void>*)sm->verts)->Create(vertarray_type->subtype, num_verts);
		sm->technique		= technique;
		sm->parameters		= parameters;

		return sm;
	}
	void			AddMeshes(tag id);

	anything		LoadMaterials(istream_ref file);
	ISO_ptr<void>	Read();


	WaveFrontReader(istream_ref file, const filename &dir) : file(file), dir(dir), line_number(0) {}
};

anything WaveFrontReader::LoadMaterials(istream_ref file) {
	auto	text = make_text_reader(file);
	char				line[256];
	anything			mtls;
	ISO_ptr<anything>	mtl;

	while (text.read_line(line)) {
		char	*s = line, c;
		if (line[0] == '#')
			continue;

		while ((c = *s) && c != ' ' && c != '\t')
			s++;
		if (c == 0)
			continue;
		*s++ = 0;

		if (line == str("newmtl")) {
			if (mtl)
				mtls.Append(mtl);
			mtl.Create(s);
		} else if (line == str("Ns")) {
			mtl->Append(ISO_ptr<float>("Ns", string_scan(s).get<float>()));
		} else if (line == str("Ni")) {
			mtl->Append(ISO_ptr<float>("Ni", string_scan(s).get<float>()));
		} else if (line == str("d")) {
			mtl->Append(ISO_ptr<float>("d", string_scan(s).get<float>()));
		} else if (line == str("illum")) {
			mtl->Append(ISO_ptr<float>("illum", string_scan(s).get<float>()));
		} else if (line == str("Ka")) {
			mtl->Append(ISO_ptr<float3p>("Ka", ReadFloat3(s)));
		} else if (line == str("Kd")) {
			mtl->Append(ISO_ptr<float3p>("Kd", ReadFloat3(s)));
		} else if (line == str("Ks")) {
			mtl->Append(ISO_ptr<float3p>("Ks", ReadFloat3(s)));
		} else if (line == str("map_Ka")) {
			mtl->Append(ISO_ptr<ISOTexture>("map_Ka", ISO::MakePtrExternal(0, dir.relative(s))));
		} else if (line == str("map_Kd")) {
			mtl->Append(ISO_ptr<ISOTexture>("map_Kd", ISO::MakePtrExternal(0, dir.relative(s))));
		} else if (line == str("map_Ks")) {
			mtl->Append(ISO_ptr<ISOTexture>("map_Ks", ISO::MakePtrExternal(0, dir.relative(s))));
		} else if (line == str("map_Bump")) {
			mtl->Append(ISO_ptr<ISOTexture>("map_Bump", ISO::MakePtrExternal(0, dir.relative(s))));
		} else if (line == str("bump")) {
			ISO_ptr<void>	p = ISO::MakePtr(ISO::user_types.Find("NormalMap"), "map_Bump");
			ISO::Browser2	b(p);
			b[0].Set(ISO::MakePtrExternal(0, dir.relative(s)));
			mtl->Append(ISO_ptr<ISOTexture>("map_Bump", p));
		}
	}

	if (mtl)
		mtls.Append(mtl);
	return mtls;
}

struct vertex0				{ float3p position;	};
struct vertex1 : vertex0	{ float3p normal;	};
struct vertex2 : vertex0	{ float2p texcoord0;};
struct vertex3 : vertex1	{ float2p texcoord0;};
struct vertex4 : vertex3	{ float4p tangent0;	};

struct ISO_type_vertex0 : ISO::TypeCompositeN<1> {
	ISO_type_vertex0() { fields[0].set("position", &vertex0::position); }
} iso_type_vertex0;

struct ISO_type_vertex1 : ISO::TypeCompositeN<2> {
	ISO_type_vertex1()	{ fields[0].set("position", &vertex1::position); fields[1].set("normal", &vertex1::normal); }
} iso_type_vertex1;

struct ISO_type_vertex2 : ISO::TypeCompositeN<2> {
	ISO_type_vertex2()	{ fields[0].set("position", &vertex2::position); fields[1].set("texcoord0", &vertex2::texcoord0); }
} iso_type_vertex2;

struct ISO_type_vertex3 : ISO::TypeCompositeN<3> {
	ISO_type_vertex3()	{ fields[0].set("position", &vertex3::position); fields[1].set("normal", &vertex3::normal);  fields[2].set("texcoord0", &vertex3::texcoord0); }
} iso_type_vertex3;

struct ISO_type_vertex4 : ISO::TypeCompositeN<4> {
	ISO_type_vertex4()	{ fields[0].set("position", &vertex4::position); fields[1].set("normal", &vertex4::normal);  fields[2].set("texcoord0", &vertex4::texcoord0); fields[3].set("tangent0", &vertex4::tangent0); }
} iso_type_vertex4;

ISO::TypeOpenArray	iso_type_vertex0array(&iso_type_vertex0);
ISO::TypeOpenArray	iso_type_vertex1array(&iso_type_vertex1);
ISO::TypeOpenArray	iso_type_vertex2array(&iso_type_vertex2);
ISO::TypeOpenArray	iso_type_vertex3array(&iso_type_vertex3);
ISO::TypeOpenArray	iso_type_vertex4array(&iso_type_vertex4);

const ISO::Type *WaveFrontReader::vertex_types[] = {
	&iso_type_vertex0,
	&iso_type_vertex1,
	&iso_type_vertex2,
	&iso_type_vertex3,
	&iso_type_vertex4,
};
const ISO::Type *WaveFrontReader::vertexarray_types[] = {
	&iso_type_vertex0array,
	&iso_type_vertex1array,
	&iso_type_vertex2array,
	&iso_type_vertex3array,
	&iso_type_vertex4array,
};

void WaveFrontReader::AddMeshes(tag id) {
	if (!faces)
		return;

#ifdef PLAT_PC
	filename	fx	= "D:\\dev\\shared\\filetypes\\3d\\wavefront.fx";
#else
	filename	fx	= filename(__FILE__).set_ext("fx");
#endif

	int	type	= (line[0] == 's' || vn.size() ? 1 : 0) | (vt.size() ? 2 : 0) | (ISO::Browser(mtl)["map_Bump"] ? 4 : 0);
	type |= 1;	// always want normals

	static const char *techniques[] = {
		"unlit_untextured",
		"lit_untextured",
		"unlit_textured",
		"lit_textured",
		0,
		"bumpmapped_untextured",
		0,
		"bumpmapped",
	};

	auto	technique	= ISO::MakePtrExternal(0, fx + ";" + techniques[type]);
	auto	parameters	= MakeComposite(ISO::Browser(mtl));

	if (!model)
		model.Create(0);

	dynamic_array<uint32>	remap_v(v.size());
	auto	unique_v	= Deduplicate(v.begin(), v.end(), remap_v);
	for (auto& i : faces)
		i.v = remap_v[i.v - 1];

	dynamic_array<float3>	unique_n;
	dynamic_array<float2>	unique_t;

	if (vn) {
		dynamic_array<uint32>	remap_n(vn.size());
		unique_n	= Deduplicate(vn.begin(), vn.end(), remap_n);
		for (auto& i : faces)
			i.n = remap_n[i.n - 1];

	} else if (type & 1) {
		dynamic_array<NormalRecord>	vert_norms(unique_v.size());
		for (auto i = faces.begin(), e = faces.end(); i != e; i += 3) {
			int		v0 = i[0].v, v1 = i[1].v, v2 = i[2].v;
			auto	fn = GetNormal(position3(unique_v[v0]), position3(unique_v[v1]), position3(unique_v[v2]));
			vert_norms[v0].Add(fn, i[0].s);
			vert_norms[v1].Add(fn, i[1].s);
			vert_norms[v2].Add(fn, i[2].s);
		}

		dynamic_array<float3>	vn(faces.size());
		for (auto i : make_pair(faces, vn)) {
			vert_norms[i.a.v].Normalise();
			i.b 	= vert_norms[i.a.v].Get(i.a.s);
		}

		dynamic_array<uint32>	remap_n(faces.size());
		unique_n	= Deduplicate(vn.begin(), vn.end(), remap_n);
		for (auto i : make_pair(faces, remap_n))
			i.a.n = i.b;
	}

	if (type & 2) {
		dynamic_array<uint32>	remap_t(vt.size());
		unique_t	= Deduplicate(vt.begin(), vt.end(), remap_t);
		for (auto& i : faces)
			i.t = remap_t[i.t - 1];
	}

	dynamic_array<uint32>	remap_i(faces.size());
	auto	unique_i	= Deduplicate(faces.begin(), faces.end(), remap_i);

	faces.clear();

	auto	vert_type	= vertex_types[type];
	auto	vert_size	= vert_type->GetSize();
	uint32	num_verts	= unique_i.size32();

	malloc_block	verts(vert_size * num_verts);
	uint8	*p0		= verts;
	for (auto &i : unique_i) {
		void	*p	= p0;
		float3p	*pos = (float3p*)p;
		*pos = unique_v[i.v];
		p	= pos + 1;
		if (type & 1) {
			float3p	*norm = (float3p*)p;
			*norm = unique_n[i.n];
			p	= norm + 1;
		}
		if (type & 2) {
			float2p	*tex = (float2p*)p;
			*tex = unique_t[i.t];
			p	= tex + 1;
		}
		p0 += vert_size;
	}
	ISO_ASSERT(p0 == verts.end());

	if (type & 4) {
		GenerateTangents(
			make_split_range<3>(remap_i),
			make_range_n(GetVertexComponent<float3p>(vert_type, verts, 0), num_verts),
			GetVertexComponent<float3p>(vert_type, verts, "normal"),
			GetVertexComponent<float2p>(vert_type, verts, "texcoord0"),
			GetVertexComponent<float4p>(vert_type, verts, "tangent0")
		);
	}

	uint32	*pi = remap_i.begin();
	VertexCacheOptimizerForsyth(pi, remap_i.size32() / 3, num_verts, pi);

	MeshBuilder	builder;
	builder.ReserveVerts(num_verts);
	auto		vertarray_type	= new ISO::TypeOpenArray(vert_type);

	for (auto i = remap_i.begin(); i < remap_i.end(); i += 3) {
		int		v0	= i[0], v1 = i[1], v2 = i[2];
		int		vb0, vb1, vb2;
		if ((vb0 = builder.TryAdd(v0)) < 0 || (vb1 = builder.TryAdd(v1)) < 0 || (vb2 = builder.TryAdd(v2)) < 0) {
			auto	sm = AddMesh(id, builder.NumVerts(), builder.NumFaces(), vertarray_type, technique, parameters);
			builder.Purge(sm, verts, vert_size);
			sm->UpdateExtents();
			model->submeshes.Append(sm);

			vb0 = builder.TryAdd(v0);
			vb1 = builder.TryAdd(v1);
			vb2 = builder.TryAdd(v2);
		}
		builder.AddFace(vb0, vb1, vb2);
	}

	auto sm = AddMesh(id, builder.NumVerts(), builder.NumFaces(), vertarray_type, technique, parameters);
	builder.Purge(sm, verts, vert_size);
	sm->UpdateExtents();
	model->submeshes.Append(sm);
}

ISO_ptr<void> WaveFrontReader::Read() {
	tag					id;
	ISO_ptr<anything>	result;
	uint32				smoothing	= 0;
	auto	text = make_text_reader(file);

	while (!file.eof()) {

		if (line[0] == '#' || line[0] == 0) {
		} else if (line[0] == 'g') {
			AddMeshes(id);
			id	= skip_whitespace(line + 1);

		} else if (line[0] == 'o') {
			AddMeshes(id);

			id	= {};
			if (model && model->submeshes.Count()) {
				model->UpdateExtents();
				CheckHasExternals(model, ISO::DUPF_DEEP);
				if (!result)
					result.Create();
				result->Append(model);
			}
			model.Create(skip_whitespace(line + 1));

		} else if (line.begins("mtllib ")) {
			mtllib	= LoadMaterials(FileInput(dir.relative(line + 7)).me());

		} else if (line.begins("usemtl ")) {
			AddMeshes(id);
			mtl		= mtllib[line + 7];

		} else if (line[0] == 'v') {
			switch (line[1]) {
				case ' ':
					v.push_back(ReadFloat3(line + 2));
					break;
				case 't':
					vt.push_back(ReadFloat2(line + 3));
					break;
				case 'n':
					vn.push_back(ReadFloat3(line + 3));
					break;
			}

		} else if (line[0] == 's') {
			string_scan(line + 1) >> smoothing;

		} else if (line[0] == 'f') {
			combo	poly[64], *i = poly;
			for (string_scan ss(line + 1); ss.skip_whitespace().remaining(); i++) {
				i->s	= smoothing;
				ss >> i->v;
				if (ss.getc() == '/') {
					if (ss.peekc() != '/')
						ss >> i->t;
					if (ss.getc() == '/')
						ss >> i->n;
				}
			}
			auto	*f	= faces.expand((i - poly - 2) * 3);
			for (auto j = poly + 1; j < i - 1; j++) {
				*f++ = poly[0];
				*f++ = j[0];
				*f++ = j[1];
			}

		} else {
			break;
		}
		++line_number;
		text.read_line(line);
	}
	
	AddMeshes(id);

	if (model && model->submeshes.Count()) {
		model->UpdateExtents();
		CheckHasExternals(model, ISO::DUPF_DEEP);
		if (!result)
			return model;
		result->Append(model);
	}
	return result;
}

//-----------------------------------------------------------------------------
//	WaveFrontWriter
//-----------------------------------------------------------------------------

struct WaveFrontWriter {
	static const ISO::Type *vertex_types[];
	static const ISO::Type *vertexarray_types[];

	text_writer<writer_intf>	file;
	const filename				fn;
	fixed_string<256>			line;
	uint32						line_number;

	bool		WriteSubmesh(SubMesh *sm);
	bool		WriteComponent(USAGE usage, stride_iterator<void> comp, uint32 nv, const ISO::Type *type);
	bool		Write(Model3 *model);
	void		WriteMaterial(ISO::Browser2 b, string_accum &sa);

	string		GetFilename(ISO_ptr_machine<void> p, const char *category = "bitmap");
	tag			MaterialID(ISO_ptr<SubMesh> sm, int i);

	WaveFrontWriter(ostream_ref file, const filename &fn) : file(file), fn(fn), line_number(0) {}
};

tag WaveFrontWriter::MaterialID(ISO_ptr<SubMesh> sm, int i) {
	if (auto id = sm->parameters.ID())
		return id;
	if (auto id = sm.ID())
		return id;
	return "mtl" + to_string(i);
}

string WaveFrontWriter::GetFilename(ISO_ptr_machine<void> p, const char *category) {
	if (const char *ext = p.External())
		return ext;

	if (const char *s = FileHandler::FindInCache(p))
		return s;

	filename	fn2 = fn.dir().add_dir(p.ID().get_tag()).add_ext("*");
	for (auto d = directory_iterator(fn2); d; ++d) {
		filename	fn3 = fn.dir().add_dir(d);
		if (auto fh = FileHandler::Get(fn3.ext())) {
			if (fh->GetCategory() == str("bitmap"))
				return fn3.name_ext_ptr();
		}
	}
	
	fn2 = fn.dir().add_dir(p.ID().get_tag()).add_ext("png");
	FileHandler::Get("png")->WriteWithFilename(p, fn2);

	return fn2.name_ext_ptr();

}

void WaveFrontWriter::WriteMaterial(ISO::Browser2 b, string_accum &sa) {
	if (auto p = b["Ns"])
		sa << "Ns " << p.get<float>() << '\n';

	if (auto p = b["Ni"])
		sa << "Ni " << p.get<float>() << '\n';

	if (auto p = b["d"])
		sa << "d " << p.get<float>() << '\n';

	if (auto p = b["Tr"])
		sa << "Tr " << p.get<float>() << '\n';

	if (auto p = b["illum"])
		sa << "illum " << p.get<int>() << '\n';

	if (auto p = b["Ka"])
		sa << "Ka " << separated_list(*(float(*)[3])p, " ") << '\n';

	if (auto p = b["Kd"])
		sa << "Kd " << separated_list(*(float(*)[3])p, " ") << '\n';

	if (auto p = b["Ks"])
		sa << "Ks " << separated_list(*(float(*)[3])p, " ") << '\n';

	if (auto p = b["Ke"])
		sa << "Ke " << separated_list(*(float(*)[3])p, " ") << '\n';

	if (auto p = b["map_Ka"])
		sa << "map_Ka " << GetFilename(p) << '\n';

	if (auto p = b["map_Kd"])
		sa << "map_Kd " << GetFilename(p) << '\n';

	if (auto p = b["map_Ks"])
		sa << "map_Ks " << GetFilename(p) << '\n';

	if (auto p = b["map_Bump"])
		sa << "map_Bump " << GetFilename(p) << '\n';

	if (auto p = b["bump"]) {
		sa << "map_Bump " << GetFilename(p) << '\n';
	}

}

bool WaveFrontWriter::WriteComponent(USAGE usage, stride_iterator<void> comp, uint32 nv, const ISO::Type *type) {
	const char *prefix = usage == USAGE_POSITION ? "v "
		: usage == USAGE_TEXCOORD	? "vt "
		: usage == USAGE_NORMAL		? "vn "
		: 0;

	auto	array	= (ISO::TypeArray*)type;
	int		count	= array->Count();
	switch (count) {
		case 2: {
			dynamic_array<float2p>	temp(nv);
			ISO::Conversion::batch_convert(comp, type, temp);
			for (auto &i : temp)
				file << prefix << i[0] << ' ' << (usage == USAGE_TEXCOORD ? 1 - i[1] : i[1]) << '\n';
			return true;
		}

		case 3: {
			dynamic_array<float3p>	temp(nv);
			ISO::Conversion::batch_convert(comp, type, temp);
			for (auto &i : temp)
				file << prefix << i[0] << ' ' << i[1] << ' ' << i[2] << '\n';
			return true;
		}
		case 4: {
			dynamic_array<float4p>	temp(nv);
			ISO::Conversion::batch_convert(comp, type, temp);
			for (auto &i : temp)
				file << prefix << i[0] << ' ' << i[1] << ' ' << i[2] << ' ' << i[3] << '\n';
			return true;
		}
	}
	return false;
}

auto put_face_vert(int id, bool t, bool n) {
	return [=](string_accum &sa) {
		return sa << ' ' << id << onlyif(t, "/") << onlyif(t, id) << onlyif(n, '/') << onlyif(n, id);
	};
}

bool WaveFrontWriter::WriteSubmesh(SubMesh *sm) {
	uint32	nv		= sm->NumVerts();
	bool	havet	= false, haven = false;

	for (auto e : sm->VertComponents()) {
		auto	usage = USAGE2(e.id).usage;
		switch (usage) {
			case USAGE_POSITION:
				break;

			case USAGE_TEXCOORD:
				if (havet)
					continue;
				havet = true;
				break;

			case USAGE_NORMAL:
				if (haven)
					continue;
				haven = true;
				break;
			default:
				continue;
		}
		WriteComponent(usage, sm->_VertComponentData(e.offset), nv, e.type->SkipUser());
	}

	for (auto &i : sm->indices) {
		file << "f"
			<< put_face_vert(i[0] + 1, havet, haven)
			<< put_face_vert(i[1] + 1, havet, haven)
			<< put_face_vert(i[2] + 1, havet, haven)
			<< '\n';
	}

	return true;
}

bool WaveFrontWriter::Write(Model3 *model) {
	uint32	nverts	= 0;
	uint32	nfaces	= 0;

	for (SubMesh *sm : model->submeshes) {
		nverts += sm->NumVerts();
		nfaces += sm->NumFaces();
	}

	file <<
		"# OBJ file generated by Isopod tools\n"
		"#\n"
		"# Vertices: " << nverts << "\n"
		"# Faces: " << nfaces << "\n"
		"# CoordinateSpace: RIGHT_Y_UP\n"
		"#\n";

	if (fn)
		file << "mtllib " << filename(fn).set_ext("mtl") << '\n';

	int	i = 0;
	for (ISO_ptr<SubMesh> sm : model->submeshes) {
		if (sm.ID())
			file	<< "o " << sm.ID() << '\n'
					<< "g " << sm.ID() << '\n';
		file << "usemtl " << MaterialID(sm, i++) << '\n';
		WriteSubmesh(sm);
	}

	if (fn) {
		FileOutput	mtlout(filename(fn).set_ext("mtl"));
		text_writer<writer_intf> mtltxt(mtlout);
		mtltxt << "# OBJ file generated by Isopod tools\n\n";

		i = 0;
		for (ISO_ptr<SubMesh> sm : model->submeshes)
			WriteMaterial(sm->parameters, mtltxt << "newmtl " << MaterialID(sm, i++) << '\n');
	}

	return true;
}

//-----------------------------------------------------------------------------
//	WaveFrontFileHandler
//-----------------------------------------------------------------------------

class WaveFrontFileHandler : FileHandler {
	const char*		GetExt()			override { return "obj"; }
	const char*		GetDescription()	override { return "WaveFront Model"; }
	int				Check(istream_ref file) override {
		file.seek(0);
		char	line[256];
		return make_text_reader(file).read_line(line) && line[0] == '#' || line[0] == 'v' ? CHECK_POSSIBLE : CHECK_DEFINITE_NO;
	}
	ISO_ptr<void>	ReadWithFilename(tag id, const filename &fn)			override { return WaveFrontReader(FileInput(fn).me(), fn.dir()).Read(); }
	ISO_ptr<void>	Read(tag id, istream_ref file)							override { return WaveFrontReader(file, "").Read(); }
	bool			WriteWithFilename(ISO_ptr<void> p, const filename &fn)	override {
		ISO_ptr<Model3> model = ISO_conversion::convert<Model3>(FileHandler::ExpandExternals(p), ISO_conversion::RECURSE | ISO_conversion::CHECK_INSIDE);
		return model && WaveFrontWriter(FileOutput(fn).me(), fn).Write(model);
	}
	bool			Write(ISO_ptr<void> p, ostream_ref file) override {
		ISO_ptr<Model3> model = ISO_conversion::convert<Model3>(FileHandler::ExpandExternals(p), ISO_conversion::RECURSE | ISO_conversion::CHECK_INSIDE);
		return model && WaveFrontWriter(file, "").Write(model);
	}
} wavefront;

class WaveFrontFileHandler2 : WaveFrontFileHandler {
	const char*		GetExt()	override { return "wobj"; }
	const char*		GetMIME()	override { return "model/obj"; }
} wavefront2;
