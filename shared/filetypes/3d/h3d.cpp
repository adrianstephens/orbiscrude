#include "iso/iso_files.h"
#include "maths/geometry.h"
#include "model_utils.h"
#include "directory.h"

using namespace iso;

class H3DFileHandler : FileHandler {
public:
	struct Header {
		uint32	meshCount;
		uint32	materialCount;
		uint32	vertexDataByteSize;
		uint32	indexDataByteSize;
		uint32	vertexDataByteSizeDepth;
		cuboid	box;
	};

	struct Mesh {
		struct Attrib {
			enum {
				format_none = 0,
				format_ubyte,
				format_byte,
				format_ushort,
				format_short,
				format_float,
				formats
			};
			uint16 offset; // byte offset from the start of the vertex
			uint16 normalized; // if true, integer formats are interpreted as [-1, 1] or [0, 1]
			uint16 components; // 1-4
			uint16 format;
		};
		enum {
			attrib_position = 0,
			attrib_texcoord0 = 1,
			attrib_normal = 2,
			attrib_tangent = 3,
			attrib_bitangent = 4,
			maxAttribs = 16,
		};
		cuboid		boundingBox;
		uint32		materialIndex;

		uint32		attribsEnabled;
		uint32		attribsEnabledDepth;
		uint32		vertexStride;
		uint32		vertexStrideDepth;
		Attrib		attrib[maxAttribs];
		Attrib		attribDepth[maxAttribs];

		uint32		vertexDataByteOffset;
		uint32		vertexCount;
		uint32		indexDataByteOffset;
		uint32		indexCount;

		uint32		vertexDataByteOffsetDepth;
		uint32		vertexCountDepth;
	};

	struct Material {
		enum { maxTexPath = 128, texCount = 6, maxMaterialName = 128 };
		float3	diffuse;
		float3	specular;
		float3	ambient;
		float3	emissive;
		float3	transparent; // light passing through a transparent surface is multiplied by this filter color
		float	opacity;
		float	shininess; // specular exponent
		float	specularStrength; // multiplier on top of specular color

		char	texDiffusePath[maxTexPath];
		char	texSpecularPath[maxTexPath];
		char	texEmissivePath[maxTexPath];
		char	texNormalPath[maxTexPath];
		char	texLightmapPath[maxTexPath];
		char	texReflectionPath[maxTexPath];

		char	name[maxMaterialName];
	};

	const char*		GetExt() override { return "h3d";				}
//	int				Check(istream_ref file) override { file.seek(0); return file.get<BFBXheader>().valid() ? CHECK_PROBABLE : CHECK_NO_OPINION; }

	ISO_ptr<void>	ReadWithFilename(tag id, const filename &fn) override;
//	bool			Write(ISO_ptr<void> p, ostream_ref file) override;
} h3d;

static const ISO::Type* types[H3DFileHandler::Mesh::Attrib::formats - 1][2][4] = {
	{
		{ISO::getdef<uint8>(),	ISO::getdef<uint8[2]>(),	ISO::getdef<uint8[3]>(),	ISO::getdef<uint8[4]>(), },
		{ISO::getdef<unorm8>(),	ISO::getdef<unorm8[2]>(),	ISO::getdef<unorm8[3]>(),	ISO::getdef<unorm8[4]>(), },
	}, {
		{ISO::getdef<int8>(),	ISO::getdef<int8[2]>(),		ISO::getdef<int8[3]>(),		ISO::getdef<int8[4]>(), },
		{ISO::getdef<norm8>(),	ISO::getdef<norm8[2]>(),	ISO::getdef<norm8[3]>(),	ISO::getdef<norm8[4]>(), },
	}, {
		{ISO::getdef<uint16>(),	ISO::getdef<uint16[2]>(),	ISO::getdef<uint16[3]>(),	ISO::getdef<uint16[4]>(), },
		{ISO::getdef<unorm16>(),ISO::getdef<unorm16[2]>(),	ISO::getdef<unorm16[3]>(),	ISO::getdef<unorm16[4]>(), },
	}, {
		{ISO::getdef<int16>(),	ISO::getdef<int16[2]>(),	ISO::getdef<int16[3]>(),	ISO::getdef<int16[4]>(), },
		{ISO::getdef<norm16>(),	ISO::getdef<norm16[2]>(),	ISO::getdef<norm16[3]>(),	ISO::getdef<norm16[4]>(), },
	}, {
		{ISO::getdef<float>(),	ISO::getdef<float[2]>(),	ISO::getdef<float[3]>(),	ISO::getdef<float[4]>(), },
		{ISO::getdef<float>(),	ISO::getdef<float[2]>(),	ISO::getdef<float[3]>(),	ISO::getdef<float[4]>(), },
	},
};
static const char *names[] = {
	"position",
	"texcoord0",
	"normal",
	"tangent",
	"binormal",
	"attrib5",
	"attrib6",
	"attrib7",
	"attrib8",
	"attrib9",
	"attrib10",
	"attrib11",
	"attrib12",
	"attrib13",
	"attrib14",
	"attrib15",
};

ISO::TypeComposite *GetVertType(const H3DFileHandler::Mesh &mesh) {
	ISO::TypeComposite	*comp = new(count_bits(mesh.attribsEnabled)) ISO::TypeComposite;

	for (uint32 m = mesh.attribsEnabled; m; m = clear_lowest(m)) {
		int				i		= lowest_set_index(m);
		auto			&a		= mesh.attrib[i];
		const ISO::Type	*type	= types[a.format - 1][!!a.normalized][a.components - 1];
		comp->Add(type, names[i]);
	}

	return comp;
}

void GetVerts(const H3DFileHandler::Mesh &mesh, ISO::TypeComposite	*comp, void *srce, void *dest) {
	const ISO::Element	*e			= comp->begin();
	uint32				dstride		= comp->GetSize();
	uint32				sstride		= mesh.vertexStride;

	uint32				soffset		= mesh.vertexDataByteOffset;
	uint32				num_verts	= mesh.vertexCount;

	for (uint32 m = mesh.attribsEnabled; m; m = clear_lowest(m), ++e) {
		auto			&a		= mesh.attrib[lowest_set_index(m)];
		uint32			soffset	= a.offset;
		uint32			doffset	= e->offset;
		uint32			size	= e->size;
		for (int j = 0; j < num_verts; j++) {
			memcpy((uint8*)dest + j * dstride, (uint8*)srce + soffset + j * sstride, size);
		}
	}
}

bool FindTexture(const filename &tex_root, filename &tex_fn) {
	if (tex_fn.is_relative())
		tex_fn = tex_root.relative(tex_fn);

	if (!tex_fn.ext().blank())
		return tex_fn.exists();

	for (directory_iterator d(tex_fn.set_ext("*")); d; ++d) {
		if (FileHandler	*fh = FileHandler::Get(filename(d).ext())) {
			if (fh->GetCategory() == str("bitmap")) {
				tex_fn.set_ext(filename(d).ext());
				return true;
			}
		}
	}
	return false;
}

struct H3DMaterial {
	float3p			diffuse;
	float3p			specular;
	float3p			ambient;
	float3p			emissive;
	float3p			transparent;
	float			opacity;
	float			shininess;
	float			specularStrength;

	ISO_ptr<void>	diffuse_tex;
	ISO_ptr<void>	specular_tex;
	ISO_ptr<void>	normal_tex;
};

ISO_DEFCOMPV(H3DMaterial, diffuse, specular, ambient, emissive, transparent, opacity, shininess, specularStrength, diffuse_tex, specular_tex, normal_tex);

ISO_ptr<void> H3DFileHandler::ReadWithFilename(tag id, const filename &fn) {
	FileInput	file(fn);
	Header		h;

	if (!file.read(h))
		return ISO_NULL;

	Mesh		*meshes		= new Mesh[h.meshCount];
	Material	*materials	= new Material[h.materialCount];

	malloc_block	vertices(h.vertexDataByteSize);
	malloc_block	indices(h.indexDataByteSize);

	readn(file, meshes, h.meshCount);
	readn(file, materials, h.materialCount);

	file.readbuff(vertices, h.vertexDataByteSize);
	file.readbuff(indices, h.indexDataByteSize);

	ModelBuilder	mb(id, ISO_NULL);
	mb->minext	= h.box.a.v;
	mb->maxext	= h.box.b.v;

	ISO_ptr<H3DMaterial>	*mats = new ISO_ptr<H3DMaterial>[h.materialCount];
	filename	tex_root	= filename(fn).rem_dir().rem_dir().add_dir("textures/*");

	for (int i = 0; i < h.materialCount; ++i) {
		const Material	&m1	= materials[i];
		H3DMaterial		*m2	= mats[i].Create(m1.name);
		filename		tex_fn;

		m2->diffuse			= m1.diffuse;
		m2->specular		= m1.specular;
		m2->ambient			= m1.ambient;
		m2->emissive		= m1.emissive;
		m2->transparent		= m1.transparent;
		m2->opacity			= m1.opacity;
		m2->shininess		= m1.shininess;
		m2->specularStrength= m1.specularStrength;

		// Load diffuse
		tex_fn	= m1.texDiffusePath;
		if (FindTexture(tex_root, tex_fn))
			m2->diffuse_tex = FileHandler::Read("diffuse", tex_fn);

		// Load specular
		tex_fn = m1.texSpecularPath;
		if (!FindTexture(tex_root, tex_fn)) {
			tex_fn = str(m1.texDiffusePath) + "_specular";
			FindTexture(tex_root, tex_fn);
		}
		m2->specular_tex = FileHandler::Read("specular", tex_fn);

		// Load emissive
		//tex[2] = TextureManager::LoadFromFile(m.texEmissivePath, true);

		// Load normal
		tex_fn = m1.texNormalPath;
		if (!FindTexture(tex_root, tex_fn)) {
			tex_fn = str(m1.texDiffusePath) + "_normal";
			FindTexture(tex_root, tex_fn);
		}
		m2->normal_tex = FileHandler::Read("normal", tex_fn);

		// Load lightmap
		//tex[4] = TextureManager::LoadFromFile(m.texLightmapPath, true);

		// Load reflection
		//tex[5] = TextureManager::LoadFromFile(m.texReflectionPath, true);
	}

	ISO_ptr<iso::technique>	t = ISO::root("data")["h3d"]["h3d"];


	for (int i = 0; i < h.meshCount; i++) {
		const Mesh			&m		= meshes[i];
		ISO::TypeComposite	*vtype	= GetVertType(m);
		SubMesh				*sb		= mb.AddMesh(vtype, m.vertexCount, m.indexCount);
		ISO::TypeOpenArray	*vtype2	= (ISO::TypeOpenArray*)sb->verts.GetType();

		GetVerts(m, vtype, vertices, vtype2->ReadPtr(sb->verts));
		memcpy(sb->indices, (uint16*)(indices + m.indexDataByteOffset), m.indexCount * sizeof(uint16));

		sb->minext		= m.boundingBox.a.v;
		sb->maxext		= m.boundingBox.b.v;
		sb->parameters	= mats[m.materialIndex];
		sb->technique	= t;
	}

	return mb;

}
