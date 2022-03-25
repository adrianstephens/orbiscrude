#include "iso/iso_files.h"
#include "filetypes/3d/model_utils.h"
#include "extra/json.h"
#include "codec/mesh/draco.h"
#include "utilities.h"
#include "GL/GL.h"

//-----------------------------------------------------------------------------
//	glTF	GL Transmission Format
//-----------------------------------------------------------------------------

using namespace iso;

//template<int N, typename E> vec<E, N>	load(const E *p) {
//	vec<E, N>	v;
//	simd::load_vec(v, p);
//	return v;
//}

template<typename E, int N> vec<E, N>	load_vec(optional<array<E, N>> p) {
	return load<N>(p.or_default().begin());
}
template<typename E, int N> vec<E, N>	load_vec(array<E, N> p) {
	return load<N>(p.begin());
}

#if 1
struct PBRParameters {
	ISOTexture	baseColorTexture;
	float4		baseColorFactor;

	ISOTexture	metallicRoughnessTexture;
	float		metallicFactor;
	float		roughnessFactor;

	ISOTexture	normalTexture;
	float		normalScale;
	
	ISOTexture	occlusionTexture;
	float		occlusionStrength;

	ISOTexture	emissiveTexture;
	float3		emissiveFactor;
};
ISO_DEFUSERCOMPV(PBRParameters, baseColorTexture, baseColorFactor, metallicRoughnessTexture, metallicFactor, roughnessFactor, normalTexture, normalScale, occlusionTexture, occlusionStrength, emissiveTexture);
#else
struct MaterialParams {
	float3p	DiffuseColor;
	float	Roughness;
	float3p	SpecularColor;
	float	Metalness;
	float	Anisotropy;
};
struct PBRParameters {
	MaterialParams	material;
	ISOTexture	baseColorTexture;
	ISOTexture	metallicRoughnessTexture;
	ISOTexture	normalTexture;
	float		normalScale;
	ISOTexture	occlusionTexture;
	float		occlusionStrength;
	ISOTexture	emissiveTexture;
	float3		emissiveFactor;
};
ISO_DEFUSERCOMPV(MaterialParams, DiffuseColor, Roughness, SpecularColor, Metalness, Anisotropy);
ISO_DEFUSERCOMPV(PBRParameters, material, baseColorTexture, metallicRoughnessTexture, normalTexture, normalScale, occlusionTexture, occlusionStrength, emissiveTexture);
#endif


struct GLTF_loader {
	JSONval	json;

	struct Buffer {
		size_t		bytelength;
		filename	fn;
		Buffer(const JSONval& j) : bytelength((j/"bytelength").get(-1)) {
			if (auto x = j / "uri") {
				fn = FileHandler::FindAbsolute(filename(x.get<const char*>()));
			}
		}
	};

	struct BufferView {
		int		buffer;
		size_t	byteLength;
		size_t	byteOffset;
		BufferView(const JSONval& j) : buffer((j/"buffer").get(-1)), byteLength((j/"byteLength").get(-1)), byteOffset((j/"byteOffset").get(0)) {}
	};

	auto	GetData(const BufferView &v) {
		Buffer		b(json/"buffers"/v.buffer);
		return make_reader_offset(FileInput(b.fn), v.byteOffset, v.byteLength);
	}
	auto	GetData(int v) {
		return GetData(json/"bufferViews"/v);
	}

	struct Accessor {
		enum Component {
			BYTE,
			UNSIGNED_BYTE,
			SHORT,
			UNSIGNED_SHORT,
			FLOAT,
			MAX_COMP,
		};
		enum Type {
			SCALAR,		//1
			VEC2,		//2
			VEC3,		//3
			VEC4,		//4
			MAT2,		//4
			MAT3,		//9
			MAT4,		//16
			MAX_TYPE,
		};
		Component	comp;
		Type		type;
		uint32		count;
		int			bufferView, byteOffset;
		bool		normalised;
		int			draco_id;

		Accessor(const JSONval& j) : draco_id(-1) {
			switch ((j / "componentType").get(0)) {
				case GL_BYTE:			comp = BYTE;			break;
				case GL_UNSIGNED_BYTE:	comp = UNSIGNED_BYTE;	break;
				case GL_SHORT:			comp = SHORT;			break;
				case GL_UNSIGNED_SHORT:	comp = UNSIGNED_SHORT;	break;
				case GL_FLOAT:			comp = FLOAT;			break;
				default:				comp = MAX_COMP;		break;
			}
			switch (string_hash((j / "type").get((const char*)0))) {
				case "SCALAR"_fnv:		type = SCALAR;			break;
				case "VEC2"_fnv:		type = VEC2;			break;
				case "VEC3"_fnv:		type = VEC3;			break;
				case "VEC4"_fnv:		type = VEC4;			break;
				case "MAT2"_fnv:		type = MAT2;			break;
				case "MAT3"_fnv:		type = MAT3;			break;
				case "MAT4"_fnv:		type = MAT4;			break;
				default:				type = MAX_TYPE;		break;
			}
			count			= (j / "count").get(0);
			bufferView		= (j/"bufferView").get(-1);
			byteOffset		= (j/"byteOffset").get(-1);
			normalised		= (j/"normalised").get(false);
		}

		const ISO::Type *ISOType() const {
			static const ISO::Type * types[][MAX_COMP] = {
				//BYTE						UNSIGNED_BYTE				SHORT						UNSIGNED_SHORT					FLOAT
				{ISO::getdef<int8>(),		ISO::getdef<uint8>(),		ISO::getdef<int16>(),		ISO::getdef<uint16>(),			ISO::getdef<float>(),		},	//SCALAR
				{ISO::getdef<int8[2]>(),	ISO::getdef<uint8[2]>(),	ISO::getdef<int16[2]>(),	ISO::getdef<uint16[2]>(),		ISO::getdef<float[2]>(),	},	//VEC2
				{ISO::getdef<int8[3]>(),	ISO::getdef<uint8[3]>(),	ISO::getdef<int16[3]>(),	ISO::getdef<uint16[3]>(),		ISO::getdef<float[3]>(),	},	//VEC3
				{ISO::getdef<int8[4]>(),	ISO::getdef<uint8[4]>(),	ISO::getdef<int16[4]>(),	ISO::getdef<uint16[4]>(),		ISO::getdef<float[4]>(),	},	//VEC4
				{ISO::getdef<int8[2][3]>(),	ISO::getdef<uint8[2][3]>(),	ISO::getdef<int16[2][3]>(),	ISO::getdef<uint16[2][3]>(),	ISO::getdef<float[2][3]>(),	},	//MAT2
				{ISO::getdef<int8[3][3]>(),	ISO::getdef<uint8[3][3]>(),	ISO::getdef<int16[3][3]>(),	ISO::getdef<uint16[3][3]>(),	ISO::getdef<float[3][3]>(),	},	//MAT3
				{ISO::getdef<int8[4][4]>(),	ISO::getdef<uint8[4][4]>(),	ISO::getdef<int16[4][4]>(),	ISO::getdef<uint16[4][4]>(),	ISO::getdef<float[4][4]>(),	},	//MAT4
			};
			return types[type][comp];
		}

	};

	struct Image {
		string	uri;		//The URI (or IRI) of the image
		string	mimeType;	//The image’s media type. This field MUST be defined when bufferView is defined
		int		bufferView;	//The index of the bufferView that contains the image. This field MUST NOT be defined when uri is defined
		Image(const JSONval& j) :
			uri			((j/"uri").get("")),
			mimeType	((j/"minFilter").get("")),
			bufferView	((j/"bufferView").get(-1))
		{}
	};
	struct Sampler {
		int	magFilter;	//Magnification filter
		int	minFilter;	//Minification filter
		int	wrapS;		//S (U) wrapping mode
		int	wrapT;		//T (V) wrapping mode
		Sampler(const JSONval& j) :
			magFilter	((j/"magFilter").get(0)),
			minFilter	((j/"minFilter").get(0)),	
			wrapS		((j/"wrapS").get(GL_REPEAT)),
			wrapT		((j/"wrapT").get(GL_REPEAT))
		{}
	};
	struct Texture {
		int	sampler;	//The index of the sampler used by this texture. When undefined, a sampler with repeat wrapping and auto filtering SHOULD be used
		int	source;		//The index of the image used by this texture. When undefined, an extension or other mechanism SHOULD supply an alternate texture source, otherwise behavior is undefined
		Texture(const JSONval& j) : sampler((j/"sampler").get(-1)), source((j/"source").get(-1)) {}
	};
	struct TextureInfo {
		int	index;		//The index of the texture.@ Yes
		int	texcoord;	//The set index of texture’s TEXCOORD attribute used for texture coordinate mapping.@No, default: 0
		TextureInfo(const JSONval& j) : index((j/"index").get(0)), texcoord((j/"texcoord").get(0)) {}
	};

	auto	GetTexture(const TextureInfo& info) {
		Texture	tex(json/"textures"/info.index);
		Sampler	samp(json/"samplers"/tex.sampler);
		Image	im(json/"images"/tex.source);
		return ISO::MakePtrExternal(0, FileHandler::FindAbsolute(im.uri));
	}

	void	ReadMaterial(SubMesh *mesh, const JSONval& mat) {
		ISO_ptr<PBRParameters>	params(0);

		mesh->parameters	= params;
		mesh->technique		= ISO::root("data")["gltf"]["gltf"];//ISO::MakePtrExternal(0, "gltf.fx");

		auto	name		= (mat/"name ").get("");
		auto	alphaCutoff	= (mat/"alphaCutoff").get(0.5f);
		auto	doubleSided	= (mat/"doubleSided").get(false);

		auto	pbr			= mat / "pbrMetallicRoughness";

		params->baseColorTexture		= GetTexture(pbr/"baseColorTexture");
		params->metallicRoughnessTexture= GetTexture(pbr/"metallicRoughnessTexture");
		params->emissiveFactor			= load_vec((mat/"emissiveFactor").get(array<float,3>(0)));
	#if 1
		params->baseColorFactor			= load_vec((pbr/"baseColorFactor").get(array<float, 4>(1)));
		params->metallicFactor			= (pbr/"metallicFactor").get(1.f);
		params->roughnessFactor			= (pbr/"roughnessFactor").get(1.f);
	#else
		params->material.DiffuseColor		= load_vec((pbr/"baseColorFactor").get(array<float, 4>(1))).xyz;
		params->material.SpecularColor		= one;
		params->material.Metalness			= (pbr/"metallicFactor").get(1.f);
		params->material.Roughness			= (pbr/"roughnessFactor").get(1.f);
		params->material.Anisotropy			= 0;
	#endif
		if (auto x = mat/"normalTexture") {
			params->normalTexture		= GetTexture(x);
			params->normalScale			= (x/"scale").get(1.f);
		}
		if (auto x = mat / "occlusionTexture") {
			params->occlusionTexture	= GetTexture(x);
			params->occlusionStrength	= (x/"strength").get(1.f);
		}
		if (auto x = mat / "emissiveTexture")
			params->emissiveTexture		= GetTexture(x);

		int	alphaMode;
		switch (string_hash((mat / "alphaMode").get(""))) {
			case "OPAQUE"_fnv:	alphaMode = 0; break;
			case "MASK"_fnv:	alphaMode = 1; break;
			case "BLEND"_fnv:	alphaMode = 2; break;
			default:			alphaMode = -1; break;
		}

		for (auto i : (mat / "extensions").items()) {
		}
		for (auto i : (mat / "extras").items()) {
		}
	}

	ISO_ptr<SubMesh>	ReadPrimitive(const JSONval& prim) {
		auto	mode = (prim / "mode").get(4);

		dynamic_array<named<Accessor>>	attributes;
		for (auto i : (prim / "attributes").items())
			attributes.emplace_back(string(i.a), json / "accessors" / i.b);

		Accessor	indices(json / "accessors" /(prim/"indices"));

		int		draco_bufferView	= -1;
		for (auto i : (prim / "extensions").items()) {
			if (i.a == "KHR_draco_mesh_compression") {
				draco_bufferView	= (i.b/"bufferView").get(-1);

				for (auto j : (i.b / "attributes").items()) {
					for (auto& a : attributes) {
						if (a.name() == j.a) {
							a.bufferView	= draco_bufferView;
							a.draco_id		= j.b.get();
							break;
						}
					}
				}

			}
		}

		for (auto i : (prim / "extras").items()) {
		}

		uint32	num_faces	= indices.count / 3;
		uint32	num_verts	= 0;

		ISO::TypeCompositeN<64>	builder(0);
		for (auto& a : attributes) {
			builder.Add(a.ISOType(), a.name());
			num_verts = max(num_verts, a.count);
		}
		
		ISO_ptr<SubMesh>	mesh(0, builder.Duplicate(), num_verts, num_faces);
		if (draco_bufferView >= 0) {
			draco::Reader	dr;
			auto	file	= GetData(draco_bufferView);
			if (dr.read(file)) {
				int32	*i = dr.corner_to_point;
				for (auto& f : mesh->indices) {
					f[0] = i[0];
					f[1] = i[1];
					f[2] = i[2];
					i += 3;
				}

				for (auto& a : attributes) {
					if (auto da = dr.GetAttribute(a.draco_id)) {
						auto	v = mesh->VertComponentData<uint8>(a.name());
						da->CopyValues(v, dr.PointToValue(*da->dec));
					}
				}
			}

		} else {
			auto	file = GetData(indices.bufferView);
			file.seek(indices.byteOffset);
			mesh->indices.read(file, num_faces);

			for (auto& a : attributes) {
				auto	file = GetData(a.bufferView);
				file.seek(a.byteOffset);
				for (auto&& v : mesh->VertComponentBlock(a.name()))
					v.read(file);
			}
		}

		if (auto x = prim / "material")
			ReadMaterial(mesh, (json / "materials")[x]);

		mesh->UpdateExtents();
		return mesh;
	}

	ISO_ptr<Model3>	ReadMesh(const JSONval& mesh) {
		ISO_ptr<Model3>	model((mesh / "name").get(""));

		if (auto x = mesh / "primitives") {
			for (auto&& i : x)
				model->submeshes.Append(ReadPrimitive(i));
			model->UpdateExtents();
		}

		if (auto x = mesh / "extensions")
			;// object	Dictionary object with extension - specific objects.No
		if (auto x = mesh / "extras")
			;// any	Application - specific data.No
		return model;
	}

	ISO_ptr<Node>	ReadNode(const JSONval& node) {
		ISO_ptr<Node>	n((node / "name").get(""));

		scale_rot_trans	transform(one);
		if (auto x = node / "translation")
			transform.set_trans(position3(load_vec(x.get<array<float, 3>>())));

		if (auto x = node / "rotation")
			transform.set_rot(load_vec(x.get<array<float, 4>>()));

		if (auto x = node / "scale")
			transform.set_scale(load_vec(x.get<array<float, 3>>()));

		if (auto x = node / "matrix")
			n->matrix = (float3x4)force_cast<float4x4>(load_vec(x.get<array<float, 16>>()));
		else
			n->matrix = (float3x4)transform;

		if (auto x = node / "mesh") {
			if (auto mesh = (json / "meshes")[x])
				n->children.Append(ReadMesh(mesh));
		}

		if (auto x = node / "camera")
			;// string	The ID of the camera referenced by this node.No
		if (auto x = node / "skeletons")
			;// string[]	The ID of skeleton nodes.No
		if (auto x = node / "skin")
			;// string	The ID of the skin referenced by this node.No
		if (auto x = node / "jointName")
			;// string	Name used when this node is a joint in a skin.No
		if (auto x = node / "extensions")
			;// object	Dictionary object with extension - specific objects.No
		if (auto x = node / "extras")
			;// any	Application - specific data.No

		if (auto x = node / "children") {
			auto	nodes = json / "nodes";
			for (auto&& i : x) {
				if (JSONval	node = nodes[i])
					n->children.Append(ReadNode(node));
			}
		}
		return n;
	}

	ISO_ptr<Scene>	ReadScene(const JSONval& scene) {
		ISO_ptr<Scene>	p((scene / "name").get(""));
		auto	root	= p->root.Create("root");
		auto	nodes	= json / "nodes";
		for (auto&& i : scene / "nodes") {
			if (JSONval	node = nodes[i])
				root->children.Append(ReadNode(node));
		}
		return p;
	}
};

class GLTFFileHandler : FileHandler {
	const char*		GetExt()				override { return "gltf"; }
	const char*		GetMIME()				override { return "model/gltf+json"; }
	const char*		GetDescription()		override { return "GL Transmission Format"; }

	int			Check(istream_ref file) override {
		switch (JSONreader(file).GetToken().type) {
			case JSONreader::Token::ARRAY_BEGIN:
			case JSONreader::Token::OBJECT_BEGIN:
				return	CHECK_POSSIBLE;
			case JSONreader::Token::BAD:
				return	CHECK_DEFINITE_NO;
			default:
				return CHECK_NO_OPINION;
		}
	}

	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		GLTF_loader	gltf;
		if (!gltf.json.read(file))
			return ISO_NULL;

		auto	scenes	= gltf.json / "scenes";
		switch (scenes.size()) {
			case 0:
				return ISO_NULL;

			case 1:
				return gltf.ReadScene(scenes[0]);

			default: {
				ISO_ptr<anything>	p(id);
				for (auto&& scene : scenes)
					p->Append(gltf.ReadScene(scene));
				return p;
			}
		}
	}
} gltf;



struct GLB_header {
	enum { MAGIC = 0x46546C67 };
	uint32	magic;
	uint32	version;
	uint32	length;
	bool	valid()	const { return magic == MAGIC; }
};

struct GLB_chunk {
	enum { JSON = 0x4E4F534A, BIN = 0x004E4942 };
	uint32	length;
	uint32	type;
	uint8	data[];
};


class GLBFileHandler : FileHandler {
	const char* GetExt()				override { return "glb"; }
	const char* GetDescription()		override { return "GL Binary Transmission Format"; }

	int			Check(istream_ref file) override {
		file.seek(0);
		GLB_header	h;
		return file.read(h) && h.valid() ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}

	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		GLB_header	h;
		if (!file.read(h) || !h.valid())
			return ISO_NULL;

		GLB_chunk	c;
		file.read(c);
		if (c.type != c.JSON)
			return ISO_NULL;

		GLTF_loader	gltf;
		if (!gltf.json.read(make_reader_offset(file, c.length)))
			return ISO_NULL;

		auto	scenes = gltf.json / "scenes";
		switch (scenes.size()) {
			case 0:
				return ISO_NULL;

			case 1:
				return gltf.ReadScene(scenes[0]);

			default: {
				ISO_ptr<anything>	p(id);
				for (auto&& scene : scenes)
					p->Append(gltf.ReadScene(scene));
				return p;
			}
		}
	}
} glb;
