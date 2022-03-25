#include "maths/geometry.h"
#include "iso/iso_files.h"
#include "iso/iso_convert.h"
#include "systems/mesh/model_iso.h"
#include "filetypes/bitmap/bitmap.h"
#include "filetypes/sound/sample.h"

using namespace iso;

struct {const char *usage; const char *name; } usages[] = {
	{ "position",	"POSITION"		},
	{ "normal",		"NORMAL"		},
	{ "colour",		"COLOR"			},
	{ "texcoord0",	"TEXCOORD"		},
	{ "texcoord1",	"TEXCOORD1"		},
	{ "texcoord2",	"TEXCOORD2"		},
	{ "texcoord3",	"TEXCOORD3"		},
	{ "texcoord4",	"TEXCOORD4"		},
	{ "texcoord5",	"TEXCOORD5"		},
	{ "texcoord6",	"TEXCOORD6"		},
	{ "texcoord7",	"TEXCOORD7"		},
	{ "tangent0",	"TANGENT"		},
	{ "tangent1",	"TANGENT1"		},
	{ "tangent2",	"TANGENT2"		},
	{ "tangent3",	"TANGENT3"		},
	{ "tangent4",	"TANGENT4"		},
	{ "tangent5",	"TANGENT5"		},
	{ "tangent6",	"TANGENT6"		},
	{ "tangent7",	"TANGENT7"		},
	{ "binormal0",	"BINORMAL"		},
	{ "binormal1",	"BINORMAL1"		},
	{ "binormal2",	"BINORMAL2"		},
	{ "binormal3",	"BINORMAL3"		},
	{ "binormal4",	"BINORMAL4"		},
	{ "binormal5",	"BINORMAL5"		},
	{ "binormal6",	"BINORMAL6"		},
	{ "binormal7",	"BINORMAL7"		},
	{ "weights",	"BLENDWEIGHT"	},
	{ "bones",		"BLENDINDICES"	},
};

struct {const ISO::Type *type; const char *name; } types[] = {
	{ ISO::getdef<float>(),				"FLOAT1"	},
	{ ISO::getdef<float[2]>(),			"FLOAT2"	},
	{ ISO::getdef<float[3]>(),			"FLOAT3"	},
	{ ISO::getdef<float[4]>(),			"FLOAT4"	},
	{ ISO::getdef<int32>(),				"INT1"		},
	{ ISO::getdef<int32[2]>(),			"INT2"		},
	{ ISO::getdef<int32[4]>(),			"INT4"		},
	{ ISO::getdef<uint32>(),			"UINT1"		},
	{ ISO::getdef<uint32[2]>(),			"UINT2"		},
	{ ISO::getdef<uint32[4]>(),			"UINT4"		},
	{ ISO::getdef<norm32>(),			"INT1N"		},
	{ ISO::getdef<norm32[2]>(),			"INT2N"		},
	{ ISO::getdef<norm32[4]>(),			"INT4N"		},
	{ ISO::getdef<unorm32>(),			"UINT1N"	},
	{ ISO::getdef<unorm32[2]>(),		"UINT2N"	},
	{ ISO::getdef<unorm32[4]>(),		"UINT4N"	},
	{ ISO::getdef<uint8[4]>(),			"UBYTE4"	},
	{ ISO::getdef<int8[4]>(),			"BYTE4"		},
	{ ISO::getdef<int16[2]>(),			"SHORT2"	},
	{ ISO::getdef<int16[4]>(),			"SHORT4"	},
	{ ISO::getdef<uint16[2]>(),			"USHORT2"	},
	{ ISO::getdef<uint16[4]>(),			"USHORT4"	},
	{ ISO::getdef<norm16[2]>(),			"SHORT2N"	},
	{ ISO::getdef<norm16[4]>(),			"SHORT4N"	},
	{ ISO::getdef<unorm16[2]>(),		"USHORT2N"	},
	{ ISO::getdef<unorm16[4]>(),		"USHORT4N"	},
	{ ISO::getdef<unorm8[4]>(),			"UBYTE4N"	},
#if 0 
	{ ISO::getdef<uint3_10_10_10>(),	"UDEC3"		},
	{ ISO::getdef<int3_10_10_10>(),		"DEC3"		},
	{ ISO::getdef<unorm3_10_10_10>(),	"UDEC3N"	},
	{ ISO::getdef<norm3_10_10_10>(),	"DEC3N"		},
	{ ISO::getdef<uint4_10_10_10_2>(),	"UDEC4"		},
	{ ISO::getdef<int4_10_10_10_2>(),	"DEC4"		},
	{ ISO::getdef<unorm4_10_10_10_2>(),	"UDEC4N"	},
	{ ISO::getdef<norm4_10_10_10_2>(),	"DEC4N"		},
	{ ISO::getdef<uint3_10_11_11>(),	"UHEND3"	},
	{ ISO::getdef<int3_10_11_11>(),		"HEND3"		},
	{ ISO::getdef<unorm3_10_11_11>(),	"UHEND3N"	},
	{ ISO::getdef<norm3_10_11_11>(),	"HEND3N"	},
	{ ISO::getdef<uint3_11_11_10>(),	"UDHEN3"	},
	{ ISO::getdef<int3_11_11_10>(),		"DHEN3"		},
	{ ISO::getdef<unorm3_11_11_10>(),	"UDHEN3N"	},
	{ ISO::getdef<norm3_11_11_10>(),	"DHEN3N"	},
#else
	{ ISO::getdef<uint32>(),			"UDEC3"		},
	{ ISO::getdef<uint32>(),			"DEC3"		},
	{ ISO::getdef<uint32>(),			"UDEC3N"	},
	{ ISO::getdef<uint32>(),			"DEC3N"		},
	{ ISO::getdef<uint32>(),			"UDEC4"		},
	{ ISO::getdef<uint32>(),			"DEC4"		},
	{ ISO::getdef<uint32>(),			"UDEC4N"	},
	{ ISO::getdef<uint32>(),			"DEC4N"		},
	{ ISO::getdef<uint32>(),			"UHEND3"	},
	{ ISO::getdef<uint32>(),			"HEND3"		},
	{ ISO::getdef<uint32>(),			"UHEND3N"	},
	{ ISO::getdef<uint32>(),			"HEND3N"	},
	{ ISO::getdef<uint32>(),			"UDHEN3"	},
	{ ISO::getdef<uint32>(),			"DHEN3"		},
	{ ISO::getdef<uint32>(),			"UDHEN3N"	},
	{ ISO::getdef<uint32>(),			"DHEN3N"	},
#endif
	{ ISO::getdef<hfloat2>(),			"FLOAT16_2"	},
	{ ISO::getdef<hfloat4>(),			"FLOAT16_4"	},
	{ ISO::getdef<float16[2]>(),		"FLOAT16_2"	},
	{ ISO::getdef<float16[4]>(),		"FLOAT16_4"	},
};

class XATG {
	filename		xfn;
	ISO::Browser	root;
	malloc_block	mb;
	ISO_ptr<void>	resources;

	bool			Get(const ISO::Browser &b, float3p &x) {
		const char *s = b.GetString();
		return s && sscanf(s, "%f,%f,%f", &x.x, &x.y, &x.z) == 3;
	}
	bool			Get(const ISO::Browser &b, float4p &v) {
		if (const char *s = b.GetString()) {
			float		x, y, z, w;
			if (sscanf(s, "%f,%f,%f,%f", &x, &y, &z, &w) == 4) {
				v = float4{x,y,z,w};
				return true;
			}
		}
		return false;
	}
	bool			Get(const ISO::Browser &b, float4x4p &m) {
		const char *s = b.GetString();
		return s && sscanf(s, "%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f",
			&m.x.x, &m.x.y, &m.x.z, &m.x.w,
			&m.y.x, &m.y.y, &m.y.z, &m.y.w,
			&m.z.x, &m.z.y, &m.z.z, &m.z.w,
			&m.w.x, &m.w.y, &m.w.z, &m.w.w
		) == 16;
	}

	ISO::Browser		Find(const char *type, const char *name);
	bool			FindPath(filename &fn);

public:
	ISO_ptr<SubMesh>	Submesh(ISO::Browser b);
	ISO_ptr<Node>		Frame(ISO::Browser b);
	ISO_ptr<void>		Material(ISO::Browser b);

	XATG(ISO::Browser &b, const char *fn = 0) : xfn(fn), root(b) {
		filename		xfn(fn);

		if (ISO::Browser b2 = b["PhysicalMemoryFile"]) {
			filename	fn(b2[1].GetString());
			if (FindPath(fn))
				mb = malloc_block::unterminated(FileInput(fn).me());
		}
		if (ISO::Browser b2 = b["BundledResources"]) {
			filename	fn(b2[0].GetString());
			if (FindPath(fn))
				resources = FileHandler::CachedRead(fn);
//			resources = FileHandler::CachedRead(b2[0].GetString());
		}
	}
};

bool XATG::FindPath(filename &fn) {
	if (exists(fn))
		return true;
	if (fn.is_relative()) {
		filename	fn2 = xfn.relative(fn);
		if (!exists(fn2))
			fn2 = xfn.relative(fn.name_ext());
		if (exists(fn2)) {
			fn = fn2;
			return true;
		}
	}
	return false;
}

ISO::Browser XATG::Find(const char *type, const char *name) {
	for (int i = 0, n = root.Count(); i < n; i++) {
		if (root.GetName(i) == type) {
			if (root[i]["Name"].GetString() == str(name))
				return *root[i];
		}
	}
	return ISO::Browser();
}

ISO_ptr<void> XATG::Material(ISO::Browser b) {
	int	nparams = 0;
	int	n = b.Count();
	for (int i = 0; i < n; i++) {
		if (b.GetName(i) == "ParamString")
			nparams++;
	}
	ISO::TypeComposite	*comp	= new(nparams) ISO::TypeComposite(0);
	for (int i = 0; i < n; i++) {
		if (b.GetName(i) == "ParamString") {
			ISO::Browser b2 = b[i];
			const char	*name	= b2["Name"].GetString();
			const char	*type	= b2["Type"].GetString();
			const ISO::Type	*iso_type = ISO::getdef<ISO_ptr<void> >();
			if (type == str("Texture2D"))
				iso_type = ISO::getdef<Texture>();
			comp->Add(iso_type, name);
		}
	}
	ISO_ptr<void>	p = MakePtr(comp);
	ISO::Browser	out(p);

	for (int i = 0; i < n; i++) {
		if (b.GetName(i) == "ParamString") {
			ISO::Browser b2		= b[i];
			const char	*name	= b2["Name"].GetString();
			const char	*value	= b2["Value"][0].GetString();
			ISO_ptr<void> r = ISO::Browser(resources)[value];
			if (!r)
				r = ISO_ptr<string>(0, value);
			out[name].Set(r);
		}
	}
	return p;
}

ISO_ptr<SubMesh> XATG::Submesh(ISO::Browser b) {
	ISO_ptr<SubMesh>	mesh(0);

	mesh->technique = ISO::root("data")["simple"]["tex"];

	if (ISO::Browser b2 = b["MeshTopology"]) {
		uint32	count	= b2["VertexBufferCount"].GetInt(1);

		if (ISO::Browser b3 = b2["VertexBuffer"]) {
			uint32	stride = b3["Stride"].GetInt();

			ISO::TypeComposite	*verttype	= 0;
			if (ISO::Browser b4 = b3["VertexDecls"]) {
				int	nfields		= b4["Count"].GetInt();
				verttype		= new(nfields) ISO::TypeComposite;
				for (int i = 0, n = b4.Count(); i < n; i++) {
					ISO::Browser b5 = b4[i];
					if (b5.GetName() == "VertexDecl") {
						uint32		offset	= b5["Offset"].GetInt();
						const char	*type	= b5["Type"].GetString();
						const char	*usage	= b5["Usage"].GetString();
						const ISO::Type	*iso_type	= 0;
						const char		*iso_usage	= 0;
						for (int i = 0; i < num_elements(types); i++) {
							if (str(types[i].name) == type) {
								iso_type = types[i].type;
								break;
							}
						}
						for (int i = 0; i < num_elements(usages); i++) {
							if (str(usages[i].name) == usage) {
								iso_usage = usages[i].usage;
								break;
							}
						}
						verttype->Add(iso_type, iso_usage);
					}
				}
			}
			if (ISO::Browser b4 = b3["PhysicalBinaryData"]) {
				uint32				offset			= b4["Offset"].GetInt();
				uint32				nverts			= b4["Count"].GetInt();
				ISO_openarray<void>	verts2(verttype, nverts);
				memcpy(verts2, (uint8*)mb + offset, nverts * stride);
				mesh->verts	= MakePtr(new ISO::TypeOpenArray(verttype, stride));
				mesh->verts.SetFlags(ISO::Value::ISBIGENDIAN);
				*(iso_ptr32<void>*)mesh->verts	= verts2;
				SetBigEndian(mesh->verts, false);
			}
		}
		if (ISO::Browser b3 = b2["IndexBuffer"]) {
			if (ISO::Browser b4 = b3["PhysicalBinaryData"]) {
				uint32				offset			= b4["Offset"].GetInt();
				uint32				nindices		= b4["Count"].GetInt() / 3;
				copy_n((array<uint16be,3>*)((uint8*)mb + offset), mesh->indices.Create(nindices, false).begin(), nindices);
//				copy_n((soft_vector<3,uint16be>*)((uint8*)mb + offset), (soft_vector<3,uint16>*)(uint16(*)[3])mesh->indices.Create(nindices, false), nindices);
			}
		}
	}
	return mesh;
}


ISO_ptr<Node> XATG::Frame(ISO::Browser b) {
	ISO_ptr<Node>	p(b["Name"].GetString());

	float4x4p	m;
	Get(b["Matrix"], m);
	p->matrix.x = (float3p&)m.x;
	p->matrix.y = (float3p&)m.y;
	p->matrix.z = (float3p&)m.z;
	p->matrix.w = (float3p&)m.w;

	for (int i = 0, n = b.Count(); i < n; i++) {
		tag2	id = b.GetName(i);
		if (id == "Frame") {
			p->children.Append(Frame(*b[i]));
		} else if (id == "Model") {
			ISO::Browser	b2 = b[i];
			ISO_ptr<Model3>		model(0);

			if (ISO::Browser b3 = b2["OrientedBoxBound"]) {
				float3p	centre, extents;
				float4p	orient;
				Get(b3["Center"], centre);
				Get(b3["Extents"], extents);
				Get(b3["Orientation"], orient);
				cuboid	ext	= (float3x4(quaternion(float4(orient))) * cuboid::with_centre(position3(centre), float3(extents))).get_box();
				model->minext = ext.a;
				model->maxext = ext.b;

			} else if (ISO::Browser b3 = b2["AxisAlignedBoxBound"]) {
				float3p	centre, extents;
				Get(b3["Center"], centre);
				Get(b3["Extents"], extents);
				cuboid	ext	= cuboid::with_centre(position3(centre), float3(extents));
				model->minext = ext.a;
				model->maxext = ext.b;
			}

			ISO_ptr<SubMesh>	sm = Submesh(Find("Mesh", b2["Mesh"].GetString()));
			sm->minext = model->minext;
			sm->maxext = model->maxext;

			if (ISO::Browser b3 = b2["SubsetMaterialMapping"])
				sm->parameters = Material(Find("MaterialInstance", b3["MaterialName"].GetString()));

			model->submeshes.Append(sm);

			p->children.Append(model);
		}
	}
	return p;
}

class XATGFileHandler : public FileHandler {
	const char*		GetExt() override { return "xatg"; }

	ISO_ptr<void>	ReadWithFilename(tag id, const filename &fn) override {
		ISO_ptr<void>	p = FileHandler::Get("xml")->ReadWithFilename(id, fn);
		ISO::Browser	b(p);
		if (b.Count() == 1 && b.GetName() == "XFileATG") {
			b = *b[0];
			return XATG(b, fn).Frame(b["Frame"]);
		}
		return p;
	}
} xatg;
