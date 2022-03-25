#include "iso/iso_files.h"
#include "iso/iso_convert.h"
#include "base/algorithm.h"
#include "extra/gpu_helpers.h"
#include "extra/indexer.h"
#include "model_utils.h"
#include "bin.h"

using namespace iso;

struct Blender {
	struct type;

	struct field {
		type		*type;
		const char *name;
		uint16		offset, len;
	};

	struct struc : dynamic_array<field> {
		type		*type;
	};

	struct type {
		const char *name;
		uint16		len;
		struc		*struc = 0;
	};

	struct header {
		enum PTR_SIZE	: uint8 { PTR4 = '_', PTR8 = '-' };
		enum ENDIANNESS	: uint8 { LITTLE = 'v', BIG = 'V' };
		char	id[7];
		uint8	ptr_size;
		uint8	endianness;
		char	version[3];

		bool	valid() const {
			return str(id) == cstr("BLENDER") && (ptr_size == PTR4 || ptr_size == PTR8) && (endianness == LITTLE || endianness == BIG);
		}
		bool	bigendian() const { return endianness == BIG; }
		bool	ptr8() const { return ptr_size == PTR8; }
	};

	struct block {
		uint32			tag;
		xint64			oldptr;
		uint32			sdna;
		uint32			count;
		struc			*struc = 0;
		malloc_block	data;

		uint64	old_end()	const	{ return oldptr + data.length(); }

		bool	read(istream_ref file, bool be, bool ptr8) {
			uint32	size;
			uint32	oldptr32;

			if (file.read(tag)
			&& file.read(size)
			&& (ptr8 ? file.read(oldptr) : file.read(oldptr32))
			&& file.read(sdna)
			&& file.read(count)
			) {
				if (be != iso_bigendian)
					swap_endian_inplace(tag, size, oldptr, oldptr32, sdna, count);
				if (!ptr8)
					oldptr = oldptr32;
				return data.read(file, size);
			}

			return false;
		}
	};

	dynamic_array<type>		types;
	dynamic_array<struc>	strucs;
	dynamic_array<block>	blocks;
	dynamic_array<uint32>	sorted_blocks;

	bool		init_dna(byte_reader r);

	const block*	block_by_addr(uint64 p) const {
		if (p) {
			auto	i	= lower_boundc(sorted_blocks, p, [this](uint32 i, uint64 p) { return blocks[i].old_end() < p; });
			if (i != sorted_blocks.end() && p >= blocks[*i].oldptr)
				return blocks + *i;;
		}
		return nullptr;
	}


	auto	browse() const { return with_param(blocks, this); }
};

bool Blender::init_dna(byte_reader r) {
	if (r.get<uint32>() != "SDNA"_u32)
		return false;

	if (r.get<uint32>() != "NAME"_u32)
		return false;

	dynamic_array<const char*>	names(r.get<uint32>());
	for (auto& i : names) {
		i = r.get_ptr<char>(0);
		r = string_end(i) + 1;
	}

	r.align(4);
	if (r.get<uint32>() != "TYPE"_u32)
		return false;

	types.resize(r.get<uint32>());
	for (auto& i : types) {
		i.name = r.get_ptr<char>(0);
		r = string_end(i.name) + 1;
	}

	r.align(4);
	if (r.get<uint32>() != "TLEN"_u32)
		return false;

	for (auto& i : types)
		i.len = r.get<uint16>();

	r.align(4);
	if (r.get<uint32>() != "STRC"_u32)
		return false;

	strucs.resize(r.get<uint32>());
	for (auto& i : strucs) {
		auto	ti = r.get<uint16>();
		i.type			= &types[ti];
		types[ti].struc	= &i;
		i.resize(r.get<uint16>());
		uint16	offset	= 0;
		for (auto& j : i) {
			j.offset	= offset;
			j.type		= &types[r.get<uint16>()];
			j.name		= names[r.get<uint16>()];

			if (j.name[0] == '*') {
				j.len	= 8;
			} else {
				j.len	= j.type->len;
			}

			for (const char *p = j.name; p = string_find(p, '[');)
				j.len *= from_string<uint16>(++p);

			offset		+= j.len;
		}
	}

	for (auto& i : blocks) {
		if (i.sdna) {
			i.struc = &strucs[i.sdna];
			//ISO_ASSERT(i.count == 1);
		}
	}

	sorted_blocks = int_range(blocks.size32());
	sort(sorted_blocks, [this](uint32 i, uint32 j) { return blocks[i].oldptr < blocks[j].oldptr; });

	return true;
}

struct DNA_array : ISO::VirtualDefaults {
	const Blender			*blender;
	const void				*data;
	const Blender::struc	*struc;
	int						count;
	DNA_array(const Blender *blender, const void *data, const Blender::struc *struc, int count) : blender(blender), data(data), struc(struc), count(count) {}

	uint32			Count()			{ return count; }
	ISO::Browser2	Index(int i);
};

struct DNA_struc : ISO::VirtualDefaults {
	const Blender			*blender;
	const void				*data;
	const Blender::struc	*struc;

	template<typename T> static ISO::Browser2 MakeBrowser(T* data, int count) {
		if (count > 1)
			return ISO::MakeBrowser(make_array_unspec(data, count));
		return ISO::MakeBrowser(data);
	}

	DNA_struc(const Blender *blender, const void *data, const Blender::struc *struc) : blender(blender), data(data), struc(struc) {
		ISO_ASSERT(struc);
	}

	uint32			Count()			{ return struc->size32(); }
	tag2			GetName(int i)	{ return (*struc)[i].name; }
	ISO::Browser2	Deref()			{ return ISO::MakeBrowser((const uint8*)data); }
	int				GetIndex(const tag2& id, int from) {
		for (auto& i : *struc) {
			auto	name = i.name;
			while (*name == '*')
				++name;
			auto p = string_find(name, '[');
			if (!p)
				p = string_end(name);

			if (id == str(name, p))
				return struc->index_of(i);
		}
		return -1;
	}
	ISO::Browser2	Index(int i);
	const char *	Type()	const	{ return struc->type->name; }
};

ISO::Browser2	DNA_struc::Index(int i)	{
	auto		&field	= (*struc)[i];
	auto		ftype	= field.type;
	auto		ftname	= field.type->name;
	const void	*fdata	= (const uint8*)data + field.offset;
	uint16		flen	= field.len;
	int			array	= 1;

	for (const char *p = field.name; p = string_find(p, '[');)
		array *= from_string<uint16>(++p);

	const char *fname	= field.name;
	while (fname[0] == '*') {
		++fname;
		auto	addr = *(xint64*)fdata;
		if (auto b = blender->block_by_addr(addr)) {
			auto	offset	= addr - b->oldptr;
			fdata	= (uint8*)b->data + offset;
			flen	= field.type->len;

			if (offset == 0) {
				flen	= b->data.length();
				if (b->struc) {
					ftype = b->struc->type;
					if (b->count > 1)
						array = b->count;
				} else {
					array = flen / field.type->len;
				}
			}

		} else {
			return {};
		}
	}
		
	if (ftype->struc) {
		if (array > 1)
			return MakePtr(0, DNA_array(blender, fdata, ftype->struc, array));
		else
			return MakePtr(0, DNA_struc(blender, fdata, ftype->struc));

	} else {
		if (ftname == cstr("char"))		return MakeBrowser((char	*)fdata, array);
		if (ftname == cstr("uchar"))	return MakeBrowser((uint8	*)fdata, array);
		if (ftname == cstr("short"))	return MakeBrowser((int16	*)fdata, array);
		if (ftname == cstr("ushort"))	return MakeBrowser((uint16	*)fdata, array);
		if (ftname == cstr("int"))		return MakeBrowser((int		*)fdata, array);
		if (ftname == cstr("long"))		return MakeBrowser((long	*)fdata, array);
		if (ftname == cstr("ulong"))	return MakeBrowser((uint32	*)fdata, array);
		if (ftname == cstr("float"))	return MakeBrowser((float	*)fdata, array);
		if (ftname == cstr("double"))	return MakeBrowser((double	*)fdata, array);
		if (ftname == cstr("int64_t"))	return MakeBrowser((int64	*)fdata, array);
		if (ftname == cstr("uint64_t"))	return MakeBrowser((uint64	*)fdata, array);

		return ISO::MakePtr(0, const_memory_block(fdata, flen));

	}
}

ISO::Browser2	DNA_array::Index(int i)	{
	return MakePtr(0, DNA_struc(blender, (uint8*)data + i * struc->type->len, struc));
}

template<> struct ISO::def<DNA_array> : public ISO::VirtualT<DNA_array> {};
template<> struct ISO::def<DNA_struc> : public ISO::VirtualT<DNA_struc> {};

ISO::Browser2	get(const param_element<Blender::block&, const Blender*> &a) {
	if (a.t.struc) {
		if (a.t.count > 1)
			return MakePtr(0, DNA_array(a.p, a.t.data, a.t.struc, a.t.count));
		return MakePtr(0, DNA_struc(a.p, a.t.data, a.t.struc));
	}
	return {};
}

tag2	_GetName(const param_element<const Blender::block&, const Blender*> &a) {
	if (a.t.struc) {
		if (a.t.count > 1)
			return a.t.struc->type->name + format_string("[%i]", a.t.count);
		return a.t.struc->type->name;
	}
	return str(reinterpret_cast<const char(&)[4]>(a.t.tag));
}


ISO_DEFUSERCOMPV(Blender::field, offset, type, name);
ISO_DEFUSERCOMPV(Blender::type, name, len, struc);
ISO_DEFUSERCOMPBV(Blender::struc, dynamic_array<Blender::field>, type);
ISO_DEFUSERCOMPV(Blender, types, strucs, blocks, browse);
ISO_DEFUSERCOMPV(Blender::block, oldptr, tag, count, data);

struct mloop_t { int v, e; };
struct mpoly_t { int loopstart, totalloop; int16 mat_nr; int8 flag, _pad; };
struct mvert_t { float co[3]; int16 no[3]; int8 flag, bweight; };
struct medge_t { int v1, v2; int8 crease, bweight; uint16 flag; };

struct BlenderVertex {
	float3p	pos;
	float3p	norm;
};
ISO_DEFUSERCOMPV(BlenderVertex, pos, norm);



template<typename T> range<T*>	get_array(ISO::Browser b) {
	return make_range_n((T*)*b[0], b.Count());
}

ISO_ptr<Model3> GetModel(ISO::Browser2 b) {
	string	name	= b["id"]["name"].get();

	auto	mpoly	= get_array<const mpoly_t>(b["mpoly"]);
	auto	mloop	= get_array<const mloop_t>(b["mloop"]);
	auto	mvert	= get_array<const mvert_t>(b["mvert"]);
	auto	medge	= get_array<const medge_t>(b["medge"]);

	ISO_ptr<Model3>		model(name);
	ISO_ptr<SubMesh>	sm(0);
	model->submeshes.Append(sm);

//	sm->technique	= ISO::root("data")["default"]["specular"];
	sm->technique	= ISO::root("data")["blender"]["simple"];

	auto	dv		= sm->CreateVerts<BlenderVertex>(mvert.size());
	auto	verts	= make_range_n(dv, mvert.size());

	for (auto &i : mvert) {
		dv->pos.x = i.co[0];
		dv->pos.y = i.co[1];
		dv->pos.z = i.co[2];

		dv->norm.x = i.no[0] / 32767.f;
		dv->norm.y = i.no[1] / 32767.f;
		dv->norm.z = i.no[2] / 32767.f;
		++dv;
	}

	uint32	ntris = 0;
	for (auto &i : mpoly)
		ntris += i.totalloop - 2;

	sm->NumFaces(ntris);
	
	auto	*di	= sm->indices.begin();
	auto	ix	= make_field_container(mloop, v);
	for (auto &i : mpoly)
		di = convex_to_tris(di, ix.slice(i.loopstart, i.totalloop));

	sm->UpdateExtents();
	model->UpdateExtents();
	return model;
}

ISO_ptr<Bone> GetBone(ISO::Browser2 b) {
	string		name	= b["name"].get();
	float3		tail	= b["tail"].get();
	float		mat[9];
	b["bone_mat"].Read(mat);
	auto	mat2		= (float3x3)reinterpret_cast<float3x3p&>(mat);

	ISO_ptr<Bone>	bone(name);
	bone->basepose		= float3x4(mat2, tail);
	return bone;
}

class BlenderFileHandler : public FileHandler {
	const char*		GetExt() override { return "blend"; }
	const char*		GetDescription() override { return "Blender";	}
	int				Check(istream_ref file) override {
		file.seek(0);
		return file.get<Blender::header>().valid() ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}
	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		Blender::header	fh;
		if (!file.read(fh) || !fh.valid())
			return ISO_NULL;

		Blender	blender;

		while (!file.eof()) {
			blender.blocks.push_back().read(file, fh.bigendian(), fh.ptr8());
			if (blender.blocks.back().tag == "ENDB"_u32)
				break;
		}

		if (auto dna = find_if_check(blender.blocks, [](auto &b){ return b.tag == "DNA1"_u32; }))
			blender.init_dna((const void*)dna->data);

		ISO_ptr<Scene>		scene(id);

		scene->root.Create("root");
		scene->root->matrix = identity;

		for (auto &b : blender.blocks) {
			if (b.struc) {
				auto	b1 = MakePtr(0, DNA_struc(&blender, b.data, b.struc));

				if (b.struc->type->name == "Object"_cstr) {
					ISO::Browser2	obj		= b1;
					ISO::Browser2	data	= obj["data"];
					string			name	= obj["name"].get();
					float			mat[16];
					obj["obmat"].Read(mat);

					ISO_ptr<Node>	node(name);
					scene->root->children.Append(node);
					node->matrix	= (float3x4)(float4x4)reinterpret_cast<float4x4p&>(mat);

					if (data.Is<DNA_struc>()) {
						auto	type = ((DNA_struc*)data)->Type();

						if (type == "Mesh"_cstr) {
							node->children.Append(GetModel(data));

						} else if (type == "bArmature"_cstr) {
							ISO_ptr<BasePose>	pose(0);
							node->children.Append(pose);
							for (auto bones = data["bonebase"]["first"]; bones; bones = bones["next"]) {
								pose->Append(GetBone(bones));
							}
						}
					}
					ISO_TRACEF("obj");
				}
				
				//if (b.struc->type->name == "Bone"_cstr) {
				//	pose->Append(GetBone(b1));
				//
				//} else if (b.struc->type->name == "Mesh"_cstr) {
				//	scene->root->children.Append(GetModel(b1));
				//}
			}
		}
		return scene;
	}
} blender;

class BlenderFileRawHandler : public FileHandler {
	const char*		GetDescription() override { return "Blender Raw";	}
	int				Check(istream_ref file) override {
		file.seek(0);
		return file.get<Blender::header>().valid() ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}
	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		Blender::header	fh;
		if (!file.read(fh) || !fh.valid())
			return ISO_NULL;

		ISO_ptr<Blender>	p(id);

		while (!file.eof()) {
			p->blocks.push_back().read(file, fh.bigendian(), fh.ptr8());
			if (p->blocks.back().tag == "ENDB"_u32)
				break;
		}

		if (auto dna = find_if_check(p->blocks, [](auto &b){ return b.tag == "DNA1"_u32; }))
			p->init_dna((const void*)dna->data);

		return p;
	}
} blender_raw;
