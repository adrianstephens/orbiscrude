#include "iso/iso_files.h"
#include "filetypes/3d/model_utils.h"
#include "extra/text_stream.h"

using namespace iso;
/*

text format:
-------------------

solid <name>

	facet normal ni nj nk
		outer loop
			vertex v1x v1y v1z
			vertex v2x v2y v2z
			vertex v3x v3y v3z
		endloop
	endfacet
	...

endsolid <name>

binary format:
-------------------

UINT8[80]    – Header                 -     80 bytes
UINT32       – Number of triangles    -      4 bytes
foreach triangle                      - 50 bytes:
	REAL32[3] – Normal vector             - 12 bytes
	REAL32[3] – Vertex 1                  - 12 bytes
	REAL32[3] – Vertex 2                  - 12 bytes
	REAL32[3] – Vertex 3                  - 12 bytes
	UINT16    – Attribute byte count      -  2 bytes
end

*/
template<typename T> using unaligned = packed<T>;

class STLFileHandler : FileHandler {
public:
	struct vertex {
		float3p position;
		float3p normal;
		vertex(const float3p &position) : position(position), normal(1,0,0) {}
	};

	struct triangle {
		unaligned<float3p>	normal;
		unaligned<float3p>	verts[3];
		uint16				attr;
	};

	const char*		GetExt()						override { return "stl"; }
	const char*		GetDescription()				override { return "3D Systems STL"; }
	ISO_ptr<void>	Read(tag id, istream_ref file)	override;

} stl;

ISO_DEFCOMPV(STLFileHandler::vertex, position, normal);

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

template<typename T> struct uniquer {
	hash_map<T, uint32>		index_map;
	dynamic_array<T>		unique;
//	dynamic_array<uint32>	indices;

	uint32	add(const T& t) {
		auto i = index_map[t];
		
		if (i.exists())
			return i;

		i = unique.size32();
		unique.push_back(t);

		return i;
	}
};

ISO_ptr<void> STLFileHandler::Read(tag id, istream_ref file) {
	dynamic_array<triangle>	tris;

	char	line[6];
	if (file.read(line) && line == "solid "_cstr) {
		file.seek(0);
		auto	text = make_text_reader(file);
		int		c	= skip_whitespace(text);
		if (read_token(text, char_set::alpha, c) != "solid")
			return ISO_NULL;

		auto	name = read_token(text, ~char_set('\n'));
		string	token;

		dynamic_array<float3p>	verts;

		for (;;) {
			token = read_token(text, char_set::alpha, skip_whitespace(text));
			if (token != "facet" || read_token(text, char_set::alpha, skip_whitespace(text)) != "normal")
				break;

			auto&	tri = tris.push_back();
			float3	normal = {
				read_number(text, skip_whitespace(text)),
				read_number(text, skip_whitespace(text)),
				read_number(text, skip_whitespace(text))
			};

			if (read_token(text, char_set::alpha, skip_whitespace(text)) != "outer" || read_token(text, char_set::alpha, skip_whitespace(text)) != "loop")
				break;

			verts.clear();
			for (;;) {
				token = read_token(text, char_set::alpha, skip_whitespace(text));
				if (token != "vertex")
					break;

				verts.emplace_back(
					read_number(text, skip_whitespace(text)),
					read_number(text, skip_whitespace(text)),
					read_number(text, skip_whitespace(text))
				);
				verts.back() *= float3{-1,1,1};
			}

			ISO_ASSERT(verts.size() == 3);

			//auto	normal2 = GetNormal(position3(verts[0]), position3(verts[1]), position3(verts[2]));
			//auto	d		= dot(normal2, normal);

			tri.normal	= normal;
			tri.verts[0] = verts[0];
			tri.verts[1] = verts[1];
			tri.verts[2] = verts[2];

			if (token != "endloop" || read_token(text, char_set::alpha, skip_whitespace(text)) != "endfacet")
				return ISO_NULL;
		}
		if (token != "endsolid")
			return ISO_NULL;


	} else {
		char	header[80];
		file.seek(0);
		file.read(header);

		uint32	num_tris = file.get<uint32le>();
		tris.read(file, num_tris);
	}

	ISO_ptr<Model3>		model(id);
	ISO_ptr<SubMesh>	sm(0);
	auto	f = sm->indices.Create(tris.size32()).begin();

	uniquer<float3p>	u;

	for (auto &i : tris) {
		(*f)[0] = u.add(i.verts[0]);
		(*f)[1] = u.add(i.verts[1]);
		(*f)[2] = u.add(i.verts[2]);
		++f;
	}

	dynamic_array<NormalRecord>	vert_norms(u.unique.size());
		
	for (auto &f : sm->indices) {
		int		v0 = f[0], v1 = f[1], v2 = f[2];
		auto	fn = GetNormal(position3(u.unique[v0]), position3(u.unique[v1]), position3(u.unique[v2]));
		vert_norms[v0].Add(fn, 0);
		vert_norms[v1].Add(fn, 0);
		vert_norms[v2].Add(fn, 0);
	}

	ISO_ptr<ISO_openarray<vertex>>	verts(none, u.unique);
	for (auto& v : *verts) {
		int	i		= verts->index_of(v);
		v.normal 	= vert_norms[i].Get(0);
	}

	sm->verts = verts;
	sm->UpdateExtent();
	model->submeshes.Append(sm);
	model->UpdateExtents();

	return model;
}
