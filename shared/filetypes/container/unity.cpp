#include "base/tree.h"
#include "iso/iso_files.h"
#include "filetypes/bitmap/bitmap.h"
#include "codec/texels/dxt.h"
#include "codec/texels/pvrtc.h"
#include "archive_help.h"
#include "extra/yaml.h"
#include "hashes/md5.h"
#include "unity_types.h"

using namespace iso;

const char	kAssetBundleVersionNumber[]	= "1";
const char	UNITY_VERSION[]				= "4.1.2f1";

void WriteCHeader(const ISO::Type *type, ostream_ref file);
void WriteCHeader(const ISO::Type **types, uint32 num, ostream_ref file);

template<bool be, typename R> struct endian_reader {
	R	&r;
	endian_reader(R &_r) : r(_r) {}
	template<typename T>bool	read(T &t) {
		endian_t<T,be> t2;
		if (r.read(t2)) {
			t = t2;
			return true;
		}
		return false;
	}
	template<typename T>T		get()		{ return r.template get<endian_t<T,be>>(); }
	getter<endian_reader>		get()		{ return *this;	}
};

template<bool be, typename R> endian_reader<be, R> make_endian_reader(R &r) {
	return r;
}

count_string get_string(const void *p, bool be) {
	uint32	len = be ? uint32(*(uint32be*)p) : uint32(*(uint32le*)p);
	return str((const char*)p + 4, len);
}

namespace unity {

//Given a type t, the fileID is equal to the first four bytes of the MD4 of the string `"s\0\0\0" + t.Namespace + t.Name' as a little endian 32-byte integer.

uint32 GetFileID(const char *ns, const char *name) {
	MD4	md4;
	md4.write("s\0\0\0");//, ns, name);
	return *(uint32*)&md4.digest()[0];

}

static Class::id Remap(Class::id id) {
	switch (id) {
		case 1012: return Class::AvatarMask; // AvatarSkeletonMask -> AvatarMask
		default: return id;
	}
}

//-----------------------------------------------------------------------------
//	TypeTree
//-----------------------------------------------------------------------------

const ISO::Type	*GetBuiltinType(const char *name);
const ISO::Type	*GetStandardType(Class::id id);

const char		*GetString(uint32 offset, const char *strings);

struct TypeTree : hierarchy<TypeTree> {
	enum FLAGS {
		kNoTransferFlags						= 0,
		kHideInEditorMask						= 1 << 0,
		kNotEditableMask						= 1 << 4,
		kStrongPPtrMask							= 1 << 6,
		kEditorDisplaysCheckBoxMask				= 1 << 8,
		kSimpleEditorMask						= 1 << 11,
		kDebugPropertyMask						= 1 << 12,
		kAlignBytesFlag							= 1 << 14,
		kAnyChildUsesAlignBytesFlag				= 1 << 15,
		kIgnoreWithInspectorUndoMask			= 1 << 16,
		kIgnoreInMetaFiles						= 1 << 19,
		kTransferAsArrayEntryNameInMetaFiles	= 1 << 20,
		kTransferUsingFlowMappingStyle			= 1 << 21,
		kGenerateBitwiseDifferences				= 1 << 22,
	};
	string			type;			// The type of the variable (eg. "Vector3f", "int")
	string			name;			// The name of the property (eg. "m_LocalPosition")
	int				size;			//= -1 if its not determinable (arrays)
	int				index;			// The index of the property (Prefabs use this index in the override bitset)
	bool			is_array;		// Is the TypeTree an array (first child is the size, second child is the type of the array elements)
	flags<FLAGS>	flags;

	TypeTree() : size(-1), index(-1), is_array(false), flags(kNoTransferFlags) {}
	TypeTree(const char *name, const char *type, int size, uint32 flags, bool is_array) : type(type), name(name), size(size), index(-1), is_array(is_array), flags(flags) {}
	bool						IsBasicDataType()	const { return children.empty() && size > 0; }
	template<typename R> void	read(R &file, int version);
	friend bool	operator==(const TypeTree& lhs, const TypeTree& rhs);
	friend bool	operator!=(const TypeTree& lhs, const TypeTree& rhs) { return !(lhs == rhs); }
};

void FixTemplates(TypeTree *tt) {
	for (auto &i : tt->depth_first()) {
		if (i.type == "vector")
			i.type += format_string("<%s>", i.children[0].children[1].type.begin());
		else if (i.type == "pair")
			i.type += format_string("<%s, %s>", i.children[0].type.begin(), i.children[1].type.begin());
		else if (i.type == "map")
			i.type += format_string("<%s, %s>", i.children[0].children[1].children[0].type.begin(), i.children[0].children[1].children[1].type.begin());
	}
}

bool operator==(const TypeTree& lhs, const TypeTree& rhs) {
	if (lhs.size != rhs.size || lhs.name != rhs.name || lhs.type != rhs.type)
		return false;

	if (lhs.flags.test(TypeTree::kAlignBytesFlag) != rhs.flags.test(TypeTree::kAlignBytesFlag))
		return false;

	for (TypeTree::const_iterator i(&lhs), k(&rhs); ; i.skip(), k.skip()) {
		if (!i && !k)
			return true;
		if (!i || !k || *i != *k)
			return false;
	}
}

struct Type {
	TypeTree		*type;
	GUID			class_guid;
	Class::id		class_id;
	GUID			type_guid;
	Class::id		type_id;
	const ISO::Type	*iso_type;

	Type() : type(0), iso_type(0)	{}
	Type(TypeTree* t, Class::id cid, const GUID &cguid, Class::id tid, const GUID &tguid) : type(t), class_guid(cguid), class_id(cid), type_guid(tguid), type_id(tid), iso_type(0) {}
	~Type()				{ delete type; }

	void		Set(TypeTree* t) {
		delete type;
		type	= t;
	}
	void		Set(TypeTree* t, const GUID	&g) {
		delete type;
		type		= t;
		type_guid	= g;
	}
	ISO_ptr<void>	CreateISO(tag id, malloc_block &&src);
};

const ISO::Type *MakeISOType(TypeTree *type) {
	if (type->is_array) {
		TypeTree	&sub_type	= type->children[1];
		return new ISO::TypeOpenArray(MakeISOType(&sub_type));
	}
	if (int	n = type->children.size32()) {
		ISO::TypeUserComp	*comp = new(n) ISO::TypeUserComp(type->type);
		for (auto c = type->children.begin(), ce = type->children.end(); c != ce; ++c) {
			if (const char *p = c->name.find('[')) {
				string	name	= c->name.slice_to((char*)p);
				int		i0		= from_string<int>(p + 1);
				while (c != ce && (c + 1)->name.begins(name))
					++c;
				int		i1		= from_string<int>(c->name.find('[') + 1);
				comp->Add(new ISO::TypeArray(MakeISOType(&*c), i1 - i0 + 1), name, !c->flags.test(TypeTree::kAlignBytesFlag));
			} else {
				comp->Add(MakeISOType(&*c), c->name, !c->flags.test(TypeTree::kAlignBytesFlag));
			}
		}

		return comp;
	}
	return GetBuiltinType(type->type);
}

void CopyISO(ISO::Browser dst, byte_reader &src, TypeTree *type) {
	if (type->is_array) {
		TypeTree	&sub_type	= type->children[1];
		int			size		= src.get<int>();
//		size_t		subsize		= ((ISO::TypeOpenArray*)dst.GetTypeDef())->subsize;
		dst.Resize(size);
		for (int i = 0; i < size; i++)
			CopyISO(dst[i], src, &sub_type);
	} else if (!type->children.empty()) {
		int	i = 0;
		for (auto c = type->children.begin(), ce = type->children.end(); c != ce; ++c) {
			CopyISO(dst[i++], src, &*c);
			if (c->flags.test(TypeTree::kAlignBytesFlag)) {
				src.p = align(src.p, 4);
			}
		}
	} else {
		src.get_block(dst.GetSize()).copy_to(dst);
	}
}
ISO_ptr<void> Type::CreateISO(tag id, malloc_block &&src) {
	if (!iso_type && type)
		iso_type = MakeISOType(type);

	if (iso_type) {
		void	*dst	= ISO::MakeRawPtr<32>(iso_type, id);
		CopyISO(ISO::Browser(iso_type, dst), byte_reader(src).me(), type);
		return ISO::GetPtr(dst);
	}

	return ISO_ptr<malloc_block>(id, move(src));
}

template<typename R> void TypeTree::read(R &file, int version) {
	type = file.template get<string>();
	name = file.template get<string>();
	size	= file.get();

	if (version == 2) {
		int variableCount = file.get();
	}

	if (version != 3)
		index = file.get();

	is_array	= file.get();
	version		= file.get();

	if (version != 3)
		flags = file.get();

	int childrenCount = file.get();

	// Read children
	for (int i = 0; i < childrenCount; i++) {
		TypeTree *newType	= new TypeTree;
		push_back(newType);
		newType->read(file, version);
	}
}

struct ObjectInfo {
	int32					byteStart;
	int32					byteSize;
	Class::id				typeID;
	compact<Class::id,16>	classID;
	uint16					isDestroyed;
};

struct FileIdentifier {
	enum TYPE { kNonAssetType = 0, kDeprecatedCachedAssetType = 1, kSerializedAssetType = 2, kMetaAssetType = 3 };
	string		path;
	TYPE		type;
	GUID		guid;

	FileIdentifier() : type(kNonAssetType)	{}
	FileIdentifier(const char *p, const GUID &g, TYPE t) : path(p), type(t), guid(g) {}

	bool operator<(const FileIdentifier &b) const {
		return guid < b.guid || (guid == b.guid && type < b.type);
	}
};

enum BuildTargetPlatform {
	kBuildNoTargetPlatform			= -2,
	kBuildAnyPlayerData				= -1,
	kBuildValidPlayer				= 1,
	kBuildStandaloneOSXUniversal	= 2,
	kBuildStandaloneOSXPPC			= 3,
	kBuildStandaloneOSXIntel		= 4,
	kBuildStandaloneWinPlayer		= 5,
	kBuildWebPlayerLZMA				= 6,
	kBuildWebPlayerLZMAStreamed		= 7,
	kBuildWii						= 8,
	kBuild_iPhone					= 9,
	kBuildPS3						= 10,
	kBuildXBOX360					= 11,
	kBuild_Android					= 13,
	kBuildWinGLESEmu				= 14,
	kBuildNaCl						= 16,
	kBuildStandaloneLinux			= 17,
	kBuildFlash						= 18,
	kBuildStandaloneWin64Player		= 19,
	kBuildWebGL						= 20,
	kBuildMetroPlayerX86			= 21,
	kBuildMetroPlayerX64			= 22,
	kBuildMetroPlayerARM			= 23,
	kBuildStandaloneLinux64			= 24,
	kBuildStandaloneLinuxUniversal	= 25,
	kBuildWP8Player					= 26,
	kBuildPS4						= 27,
	kBuildPlayerTypeCount			= 28,
};

struct LoadedObject {
	Class::id				type_id;
	Class::id				class_id;
	ISO_openarray<xint8>	data;

	LoadedObject(ObjectInfo &o, void *p) : type_id(o.typeID), class_id(o.classID) {
		memcpy(data.Create(o.byteSize, false), p, o.byteSize);
	}
	LoadedObject(ObjectInfo &o, istream_ref file) : type_id(o.typeID), class_id(o.classID) {
		file.seek(o.byteStart);
		file.readbuff(data.Create(o.byteSize, false), o.byteSize);
	}
};

struct Scene {
	BuildTargetPlatform	platform;
	iso::map<int, Type>						types;
	typedef	pair<int, ObjectInfo>			object;
	dynamic_array<object>					objects;
	dynamic_array<FileIdentifier>			externals;
	template<bool be> bool	ReadMetadata(int version, memory_block &meta, size_t dataOffset, size_t dataEnd);
	bool					ReadBinary(istream_ref file, bool &bigendian);
};

struct meta15_field {
	uint16	a;				//0,1,2,4,8
	uint8	depth;
	uint8	is_array;		//0,1
	uint32	type_offset;	//top bit ->global table
	uint32	name_offset;	//top bit ->global table
	uint32	size;
	uint32	index;
	uint32	flags;
};

template<bool be> bool Scene::ReadMetadata(int version, memory_block &mb, size_t dataOffset, size_t dataEnd) {
	byte_reader		meta(mb);
	auto			meta2	= make_endian_reader<be>(meta);

	string			unityVersion;
	if (version >= 7)
		unityVersion = meta.get<string>();

	if (version >= 8)
		platform = meta2.get();

	// Read types
	int				typeCount;
	if (version >= 17) {	//don't know when
		uint8	unknown		= meta.getc();
		meta2.read(typeCount);

		//uint32	vector_count = 0, map_count = 0, pair_count = 0;
		for (int i = 0; i < typeCount; i++) {
			int32		type_id		= meta2.get();
			uint8		unknown1	= meta.getc();
			uint16		class_id	= meta2.get();
			GUID		class_guid;
			if (class_id != 0xffff)
				class_guid = meta.get();

			GUID		type_guid	= meta.get();
			types[i] = Type(0, (Class::id)class_id, class_guid, (Class::id)type_id, type_guid);
		}

	} else if (version >= 14) {	//don't know when
		uint8	unknown		= meta.getc();
		meta2.read(typeCount);

		//uint32	vector_count = 0, map_count = 0, pair_count = 0;
		for (int i = 0; i < typeCount; i++) {
			int32		class_id	= meta2.get();
			GUID		class_guid;
			if (class_id < 0)
				class_guid	= meta.get();
			GUID		guid			= meta.get();
			uint32		num_fields		= meta2.get();
			uint32		strings_size	= meta2.get();
			const meta15_field	*fields	= meta.get_ptr(num_fields);
			const_memory_block	strings	= meta.get_block(strings_size);

			TypeTree	*stack[32];
			for (int i = 0; i < num_fields; i++) {
				int	depth = fields->depth;
				TypeTree	*tt = new TypeTree(
					GetString(fields->name_offset, strings),
					GetString(fields->type_offset, strings),
					fields->size, fields->flags, !!fields->is_array
				);
				/*if (tt->type == "vector")
					tt->type += to_string(vector_count++);
				else if (tt->type == "map")
					tt->type += to_string(map_count++);
				else if (tt->type == "pair")
					tt->type += to_string(pair_count++);*/
				stack[depth] = tt;
				if (depth)
					stack[depth - 1]->push_back(tt);

				++fields;
			}

			TypeTree*	readType = stack[0];
			FixTemplates(readType);
			types[class_id < 0 ? class_id : Remap((Class::id)class_id)].Set(readType, class_guid);
		}
	} else {
		meta2.read(typeCount);
		for (int i = 0; i < typeCount; i++) {
			TypeTree*	readType = new TypeTree;
			Class::id	id;
			GUID		guid;
			meta2.read(id);
			if (version >= 15)
				meta.read(guid);
			readType->read(meta2, version);
			types[Remap(id)].Set(readType);
		}
	}

	int bigIDEnabled = 0;
	if (version >= 14)
		bigIDEnabled = 1;
	else if (version >= 7)
		meta2.read(bigIDEnabled);

	// Read objects
	int				objectCount = meta2.get();
	meta.p = align(meta.p, 4);	//?
	for (int i = 0; i < objectCount; i++) {
		uint32		fileID	= bigIDEnabled ? uint32(meta2.template get<uint64>()) : meta2.template get<uint32>();

		ObjectInfo	value;
		value.byteStart = meta2.template get<uint32>() + uint32(dataOffset);
		value.byteSize	= meta2.template get<uint32>();

		if (version >= 17) {
			value.typeID		= types[meta2.template get<uint16>()].type_id;
			meta2.template get<uint16>();
		} else {
			meta2.read(value.typeID);
			meta2.read(value.classID);
			meta2.read(value.isDestroyed);
		}

		if (version >= 14 && version < 17)
			meta2.template get<uint32>();

		value.typeID	= Remap(value.typeID);
		value.classID	= Remap(value.classID);

		if (value.byteStart < 0 || value.byteSize < 0 || value.byteStart + value.byteSize < value.byteStart || value.byteStart + value.byteSize > dataEnd)
			return false;

		objects.push_back(make_pair(fileID, value));
	}

	if (!objects.empty() && typeCount == 0) {
		bool versionPasses;
		const char *newLinePosition = unityVersion.find('\n');
		if (!newLinePosition)
			versionPasses = unityVersion == UNITY_VERSION;
		else
			versionPasses = string(newLinePosition + 1) == kAssetBundleVersionNumber;

		if (!versionPasses)
			return false;
	}
#if 0
	// Read externals
	int			externalsCount;
	meta2.read(externalsCount);
	for (int i = 0; i < externalsCount; i++) {
		FileIdentifier	*f = new (externals) FileIdentifier;
		if (version >= 5) {
			if (version >= 6) {
				string	temp = meta.get();
			}
			for (int g = 0; g < 4; g++)
				meta2.read(f->guid.data[g]);

			meta2.read(f->type);
		}
		f->path = meta.get<string>();
	}

	if (version >= 5) {
		string user = meta.get();
	}
#endif
	return true;
}


struct SerializedFileHeader8 {
	uint32be	m_MetadataSize;
	uint32be	m_FileSize;
	uint32be	version;
};
struct SerializedFileHeader : SerializedFileHeader8 {
	enum { kCurrentSerializeVersion = 17 };//15
	uint32be	m_DataOffset;
	uint8		m_Endianess;
	uint8		m_Reserved[3];

	int Version() const {
		if (m_MetadataSize == -1 || version == 1 || version > SerializedFileHeader::kCurrentSerializeVersion)
			return 0;

		if (version >= 9) {
			if (m_Endianess > 1 || m_DataOffset > m_FileSize)
				return 0;
		} else {
			if (m_MetadataSize == 0)
				return 0;
		}
		return version;
	}
};

bool Scene::ReadBinary(istream_ref file, bool &bigendian) {
	SerializedFileHeader	h;
	if (!file.read(h) || h.Version() == 0|| h.m_FileSize > file.length())
		return false;

	size_t	metadataOffset, metadataSize;
	size_t	dataOffset, dataSize, dataEnd;
	bigendian	= false;

	if (h.version >= 9) {
		metadataOffset	= sizeof(SerializedFileHeader);
		metadataSize	= h.m_MetadataSize;
		bigendian		= !!h.m_Endianess;
		dataOffset		= h.m_DataOffset;
		dataSize		= h.m_FileSize - h.m_DataOffset;
		dataEnd			= dataOffset + dataSize;

	} else {
		dataOffset		= 0;
		dataSize		= h.m_FileSize - h.m_MetadataSize - sizeof(SerializedFileHeader8);
		dataEnd			= h.m_FileSize - h.m_MetadataSize;
		metadataSize	= h.m_MetadataSize - 1;
		metadataOffset	= h.m_FileSize - metadataSize;
		bigendian		= !!file.getc();
	}

	file.seek(metadataOffset);
	malloc_block	meta(file, metadataSize);

	return bigendian
	?	ReadMetadata<true>(h.version, meta, dataOffset, dataEnd)
	:	ReadMetadata<false>(h.version, meta, dataOffset, dataEnd);
}


template<typename D, typename S> static const void *Get(const block<D, 2> &dest, const S *srce) {
	uint32	width = dest.template size<1>(), height = dest.template size<2>();
	copy(make_block((const S*)srce, width, height), dest);
	return srce + width * height;
}
template<typename S> static const void *GetBC(const block<ISO_rgba, 2> &dest, const S *srce) {
	uint32	width	= max(dest.size<1>() / 4, 1), height = max(dest.size<2>() / 4, 1);
	copy(make_block((const S*)srce, width, height), dest);
	return srce + width * height;
}

ISO_ptr<bitmap>	LoadBitmap(tag2 id, int format, int width, int height, int stride, const void *data) {
	ISO_ptr<bitmap>	bm(id);
	bm->Create(width, height);
	switch (format) {
		case kTexFormatAlpha8:		data = Get(bm->All(),	(Texel<A8>			*)data);	break;
		case kTexFormatARGB4444:	data = Get(bm->All(),	(Texel<A4R4G4B4>	*)data);	break;
		case kTexFormatRGB24:		data = Get(bm->All(),	(Texel<R8G8B8>		*)data);	break;
		case kTexFormatRGBA32:		data = Get(bm->All(),	(Texel<R8G8B8A8>	*)data);	break;
		case kTexFormatARGB32:		data = Get(bm->All(),	(Texel<A8R8G8B8>	*)data);	break;
//		case kTexFormatARGBFloat:
		case kTexFormatRGB565:		data = Get(bm->All(),	(Texel<R5G6B5>		*)data);	break;
		case kTexFormatBGR24:		data = Get(bm->All(),	(Texel<B8G8R8>		*)data);	break;
//		case kTexFormatAlphaLum16:
		case kTexFormatDXT1:		data = GetBC(bm->All(), (DXT1rec			*)data);	break;
		case kTexFormatDXT3:		data = GetBC(bm->All(), (DXT23rec			*)data);	break;
		case kTexFormatDXT5:		data = GetBC(bm->All(), (DXT45rec			*)data);	break;
		case kTexFormatRGBA4444:	data = Get(bm->All(),	(Texel<R4G4B4A4>	*)data);	break;
		case kTexFormatPVRTC_RGB2:
		case kTexFormatPVRTC_RGBA2:
		case kTexFormatPVRTC_RGB4:
		case kTexFormatPVRTC_RGBA4:	data = (uint8*)data + PVRTCDecompress((const PVRTCrec*)data, (pixel8*)bm->ScanLine(0), width, height, width, format < kTexFormatPVRTC_RGB4); break;
//		case kTexFormatETC_RGB4:	data = (uint8*)data + ETCDecompress((const ETCrec*)data, (pixel8*)bm->ScanLine(0), width, height); break;
//		case kTexFormatATC_RGB4:	data = GetBC(bm->All(), (ATTrec				*)data);	break;
//		case kTexFormatATC_RGBA8:	data = GetBC(bm->All(), (ATTArec			*)data);	break;
		case kTexFormatBGRA32:		data = Get(bm->All(),	(Texel<B8G8R8A8>	*)data);	break;
//		case kTexFormatFlashATF_RGB_DXT1:
//		case kTexFormatFlashATF_RGBA_JPG:
//		case kTexFormatFlashATF_RGB_JPG:
	}
	return bm;
}

};

ISO_DEFUSERCOMPV(unity::LoadedObject, type_id, class_id, data);

const ISO::Type	*unity::GetStandardType(Class::id id) {
const static struct {
	int				id;
	const ISO::Type	*type;
	bool operator<(int _id) const { return id < _id; }
} table[] = {
//#define ENTRY(x) {unity::Class::x, 0}
#define ENTRY(x) {unity::Class::x, ISO::getdef<unity::x>()}
	ENTRY(GameObject),
	ENTRY(Component),
	ENTRY(LevelGameManager),
	ENTRY(Transform),
	ENTRY(TimeManager),
	ENTRY(GlobalGameManager),
	ENTRY(Behaviour),
	ENTRY(GameManager),
	ENTRY(AudioManager),
	ENTRY(ParticleAnimator),
	ENTRY(InputManager),
	ENTRY(EllipsoidParticleEmitter),
	ENTRY(Pipeline),
	ENTRY(EditorExtension),
	ENTRY(Physics2DSettings),
	ENTRY(Camera),
	ENTRY(Material),
	ENTRY(MeshRenderer),
	ENTRY(Renderer),
	ENTRY(ParticleRenderer),
	ENTRY(Texture),
	ENTRY(Texture2D),
	ENTRY(SceneSettings),
	ENTRY(GraphicsSettings),
	ENTRY(MeshFilter),
	ENTRY(OcclusionPortal),
	ENTRY(Mesh),
	ENTRY(Skybox),
	ENTRY(QualitySettings),
	ENTRY(Shader),
	ENTRY(TextAsset),
	ENTRY(Rigidbody2D),
	ENTRY(Physics2DManager),
	ENTRY(Collider2D),
	ENTRY(Rigidbody),
	ENTRY(PhysicsManager),
	ENTRY(Collider),
	ENTRY(Joint),
	ENTRY(CircleCollider2D),
	ENTRY(HingeJoint),
	ENTRY(PolygonCollider2D),
	ENTRY(BoxCollider2D),
	ENTRY(PhysicsMaterial2D),
	ENTRY(MeshCollider),
	ENTRY(BoxCollider),
	ENTRY(SpriteCollider2D),
	ENTRY(EdgeCollider2D),
	ENTRY(ComputeShader),
	ENTRY(AnimationClip),
	ENTRY(ConstantForce),
	ENTRY(WorldParticleCollider),
	ENTRY(TagManager),
	ENTRY(AudioListener),
	ENTRY(AudioSource),
	ENTRY(AudioClip),
	ENTRY(RenderTexture),
	ENTRY(MeshParticleEmitter),
	ENTRY(ParticleEmitter),
	ENTRY(Cubemap),
	ENTRY(Avatar),
	ENTRY(AnimatorController),
	ENTRY(GUILayer),
	ENTRY(RuntimeAnimatorController),
	ENTRY(ScriptMapper),
	ENTRY(Animator),
	ENTRY(TrailRenderer),
	ENTRY(DelayedCallManager),
	ENTRY(TextMesh),
	ENTRY(RenderSettings),
	ENTRY(Light),
	ENTRY(CGProgram),
	ENTRY(BaseAnimationTrack),
	ENTRY(Animation),
	ENTRY(MonoBehaviour),
	ENTRY(MonoScript),
	ENTRY(MonoManager),
	ENTRY(Texture3D),
	ENTRY(NewAnimationTrack),
	ENTRY(Projector),
	ENTRY(LineRenderer),
	ENTRY(Flare),
	ENTRY(Halo),
	ENTRY(LensFlare),
	ENTRY(FlareLayer),
	ENTRY(HaloLayer),
	ENTRY(NavMeshAreas),
	ENTRY(HaloManager),
	ENTRY(Font),
	ENTRY(PlayerSettings),
	ENTRY(NamedObject),
	ENTRY(GUITexture),
	ENTRY(GUIText),
	ENTRY(GUIElement),
	ENTRY(PhysicMaterial),
	ENTRY(SphereCollider),
	ENTRY(CapsuleCollider),
	ENTRY(SkinnedMeshRenderer),
	ENTRY(FixedJoint),
	ENTRY(RaycastCollider),
	ENTRY(BuildSettings),
	ENTRY(AssetBundle),
	ENTRY(CharacterController),
	ENTRY(CharacterJoint),
	ENTRY(SpringJoint),
	ENTRY(WheelCollider),
	ENTRY(ResourceManager),
	ENTRY(NetworkView),
	ENTRY(NetworkManager),
	ENTRY(PreloadData),
	ENTRY(MovieTexture),
	ENTRY(ConfigurableJoint),
	ENTRY(TerrainCollider),
	ENTRY(MasterServerInterface),
	ENTRY(TerrainData),
	ENTRY(LightmapSettings),
	ENTRY(WebCamTexture),
	ENTRY(EditorSettings),
	ENTRY(InteractiveCloth),
	ENTRY(ClothRenderer),
	ENTRY(EditorUserSettings),
	ENTRY(SkinnedCloth),
	ENTRY(AudioReverbFilter),
	ENTRY(AudioHighPassFilter),
	ENTRY(AudioChorusFilter),
	ENTRY(AudioReverbZone),
	ENTRY(AudioEchoFilter),
	ENTRY(AudioLowPassFilter),
	ENTRY(AudioDistortionFilter),
	ENTRY(SparseTexture),
	ENTRY(AudioBehaviour),
	ENTRY(AudioFilter),
	ENTRY(WindZone),
	ENTRY(Cloth),
	ENTRY(SubstanceArchive),
	ENTRY(ProceduralMaterial),
	ENTRY(ProceduralTexture),
	ENTRY(OffMeshLink),
	ENTRY(OcclusionArea),
	ENTRY(Tree),
	ENTRY(NavMeshObsolete),
	ENTRY(NavMeshAgent),
	ENTRY(NavMeshSettings),
	ENTRY(LightProbesLegacy),
	ENTRY(ParticleSystem),
	ENTRY(ParticleSystemRenderer),
	ENTRY(ShaderVariantCollection),
	ENTRY(LODGroup),
	ENTRY(BlendTree),
	ENTRY(Motion),
	ENTRY(NavMeshObstacle),
	ENTRY(TerrainInstance),
	ENTRY(SpriteRenderer),
	ENTRY(Sprite),
	ENTRY(CachedSpriteAtlas),
	ENTRY(ReflectionProbe),
	ENTRY(ReflectionProbes),
	ENTRY(Terrain),
	ENTRY(LightProbeGroup),
	ENTRY(AnimatorOverrideController),
	ENTRY(CanvasRenderer),
	ENTRY(Canvas),
	ENTRY(RectTransform),
	ENTRY(CanvasGroup),
	ENTRY(BillboardAsset),
	ENTRY(BillboardRenderer),
	ENTRY(SpeedTreeWindAsset),
	ENTRY(AnchoredJoint2D),
	ENTRY(Joint2D),
	ENTRY(SpringJoint2D),
	ENTRY(DistanceJoint2D),
	ENTRY(HingeJoint2D),
	ENTRY(SliderJoint2D),
	ENTRY(WheelJoint2D),
	ENTRY(NavMeshData),
	ENTRY(AudioMixer),
	ENTRY(AudioMixerController),
	ENTRY(AudioMixerGroupController),
	ENTRY(AudioMixerEffectController),
	ENTRY(AudioMixerSnapshotController),
	ENTRY(PhysicsUpdateBehaviour2D),
	ENTRY(ConstantForce2D),
	ENTRY(Effector2D),
	ENTRY(AreaEffector2D),
	ENTRY(PointEffector2D),
	ENTRY(PlatformEffector2D),
	ENTRY(SurfaceEffector2D),
	ENTRY(LightProbes),
	ENTRY(SampleClip),
	ENTRY(AudioMixerSnapshot),
	ENTRY(AudioMixerGroup),
	ENTRY(AssetBundleManifest),
	ENTRY(Prefab),
	ENTRY(EditorExtensionImpl),
	ENTRY(AssetImporter),
	ENTRY(AssetDatabase),
	ENTRY(Mesh3DSImporter),
	ENTRY(TextureImporter),
	ENTRY(ShaderImporter),
	ENTRY(ComputeShaderImporter),
	ENTRY(AvatarMask),
	ENTRY(AudioImporter),
	ENTRY(HierarchyState),
	ENTRY(GUIDSerializer),
	ENTRY(AssetMetaData),
	ENTRY(DefaultAsset),
	ENTRY(DefaultImporter),
	ENTRY(TextScriptImporter),
	ENTRY(SceneAsset),
	ENTRY(NativeFormatImporter),
	ENTRY(MonoImporter),
	ENTRY(AssetServerCache),
	ENTRY(LibraryAssetImporter),
	ENTRY(ModelImporter),
	ENTRY(FBXImporter),
	ENTRY(TrueTypeFontImporter),
	ENTRY(MovieImporter),
	ENTRY(EditorBuildSettings),
	ENTRY(DDSImporter),
	ENTRY(InspectorExpandedState),
	ENTRY(AnnotationManager),
	ENTRY(PluginImporter),
	ENTRY(EditorUserBuildSettings),
	ENTRY(PVRImporter),
	ENTRY(ASTCImporter),
	ENTRY(KTXImporter),
	ENTRY(AnimatorStateTransition),
	ENTRY(AnimatorState),
	ENTRY(HumanTemplate),
	ENTRY(AnimatorStateMachine),
	ENTRY(PreviewAssetType),
	ENTRY(AnimatorTransition),
	ENTRY(SpeedTreeImporter),
	ENTRY(AnimatorTransitionBase),
	ENTRY(SubstanceImporter),
	ENTRY(LightmapParameters),
	ENTRY(LightmapSnapshot),
	ENTRY(Int),
	ENTRY(Bool),
	ENTRY(Float),
	ENTRY(MonoObject),
	ENTRY(Collision),
	ENTRY(Vector3f),
	ENTRY(RootMotionData),
#undef ENTRY
};
	auto *i = lower_boundc(table, id);
	return i->id == id ? i->type : 0;
}

const ISO::Type *unity::GetBuiltinType(const char *name) {
	static struct { const char *name; const ISO::Type *type; } builtin_types[] = {
		{"bool",						ISO::getdef<bool>()							},
		{"char",						ISO::getdef<char>()							},
		{"ColorRGBA",					ISO::getdef<rgba8>()						},
		{"Component",					ISO::getdef<unity::Component>()				},
		{"data",						ISO::getdef<void*>()						},
	//	{"deque",						ISO::getdef<deque>()						},
		{"double",						ISO::getdef<double>()						},
		{"dynamic_array",				ISO::getdef<dynamic_array<void*> >()		},
	//	{"FastPropertyName",			ISO::getdef<FastPropertyName>()				},
		{"float",						ISO::getdef<float>()						},
		{"Font",						ISO::getdef<unity::Font>()					},
		{"GameObject",					ISO::getdef<unity::GameObject>()			},
		{"Generic Mono",				ISO::getdef<void*>()						},
	//	{"GradientNEW",					ISO::getdef<unity::GradientNEW>()			},
		{"GUID",						ISO::getdef<unity::GUID>()					},
	//	{"GUIStyle",					ISO::getdef<unity::GUIStyle>()				},
		{"int",							ISO::getdef<int>()							},
	//	{"list",						ISO::getdef<list>()							},
		{"long long",					ISO::getdef<int64>()						},
	//	{"map",							ISO::getdef<map>()							},
		{"Matrix4x4f",					ISO::getdef<float4x4p>()					},
	//	{"MdFour",						ISO::getdef<MdFour>()						},
		{"MonoBehaviour",				ISO::getdef<unity::MonoBehaviour>()			},
		{"MonoScript",					ISO::getdef<unity::MonoScript>()			},
		{"Object",						ISO::getdef<unity::Object>()				},
	//	{"pair",						ISO::getdef<pair>()							},
		{"PPtr<Component>",				ISO::getdef<pointer<unity::Component>>()	},
		{"PPtr<GameObject>",			ISO::getdef<pointer<unity::GameObject>>()	},
		{"PPtr<Material>",				ISO::getdef<pointer<unity::Material>>()		},
		{"PPtr<MonoBehaviour>",			ISO::getdef<pointer<unity::MonoBehaviour>>()},
		{"PPtr<MonoScript>",			ISO::getdef<pointer<unity::MonoScript>>()	},
		{"PPtr<Object>",				ISO::getdef<pointer<unity::Object>>()		},
		{"PPtr<Prefab>",				ISO::getdef<pointer<unity::Prefab>>()		},
		{"PPtr<Sprite>",				ISO::getdef<pointer<unity::Sprite>>()		},
		{"PPtr<TextAsset>",				ISO::getdef<pointer<unity::TextAsset>>()	},
		{"PPtr<Texture>",				ISO::getdef<pointer<unity::Texture>>()		},
		{"PPtr<Texture2D>",				ISO::getdef<pointer<unity::Texture2D>>()	},
		{"PPtr<Transform>",				ISO::getdef<pointer<unity::Transform>>()	},
		{"Prefab",						ISO::getdef<unity::Prefab>()				},
		{"Quaternionf",					ISO::getdef<float4p>()						},
		{"Rectf",						ISO::getdef<unity::Rectf>()					},
		{"RectInt",						ISO::getdef<unity::RectInt>()				},
	//	{"RectOffset",					ISO::getdef<RectOffset>()					},
	//	{"set",							ISO::getdef<set>()							},
		{"short",						ISO::getdef<short>()						},
		{"SInt16",						ISO::getdef<int16>()						},
		{"SInt32",						ISO::getdef<int32>()						},
		{"SInt64",						ISO::getdef<int64>()						},
		{"SInt8",						ISO::getdef<int8>()							},
	//	{"staticvector",				ISO::getdef<staticvector>()					},
		{"string",						ISO::getdef<string>()						},
		{"TextAsset",					ISO::getdef<unity::TextAsset>()				},
		{"TextMesh",					ISO::getdef<unity::TextMesh>()				},
		{"Texture",						ISO::getdef<unity::Texture>()				},
		{"Texture2D",					ISO::getdef<unity::Texture2D>()				},
		{"Transform",					ISO::getdef<unity::Transform>()				},
		{"TypelessData",				ISO::getdef<void*>()						},
		{"UInt16",						ISO::getdef<uint16>()						},
		{"UInt32",						ISO::getdef<uint32>()						},
		{"UInt64",						ISO::getdef<uint64>()						},
		{"UInt8",						ISO::getdef<uint8>()						},
		{"unsigned int",				ISO::getdef<unsigned int>()					},
		{"unsigned long long",			ISO::getdef<uint64>()						},
		{"unsigned short",				ISO::getdef<unsigned short>()				},
	//	{"vector",						ISO::getdef<vector>()						},
		{"Vector2f",					ISO::getdef<float2p>()						},
		{"Vector3f",					ISO::getdef<float3p>()						},
		{"Vector4f",					ISO::getdef<float4p>()						},
	};

	for (auto &i : builtin_types) {
		if (str(name) == i.name)
			return i.type;
	}
	return 0;
}

const char *unity::GetString(uint32 offset, const char *strings) {
	static const char builtin_strings[] =
		"AABB\0" "AnimationClip\0" "AnimationCurve\0" "AnimationState\0" "Array\0" "Base\0" "BitField\0" "bitset\0"
		"bool\0" "char\0" "ColorRGBA\0" "Component\0" "data\0" "deque\0" "double\0" "dynamic_array\0" "FastPropertyName\0" "first\0" "float\0" "Font\0" "GameObject\0"
		"Generic Mono\0" "GradientNEW\0" "GUID\0" "GUIStyle\0" "int\0" "list\0" "long long\0" "map\0" "Matrix4x4f\0" "MdFour\0" "MonoBehaviour\0" "MonoScript\0"
		"m_ByteSize\0" "m_Curve\0" "m_EditorClassIdentifier\0" "m_EditorHideFlags\0" "m_Enabled\0" "m_ExtensionPtr\0" "m_GameObject\0" "m_Index\0" "m_IsArray\0"
		"m_IsStatic\0" "m_MetaFlag\0" "m_Name\0" "m_ObjectHideFlags\0" "m_PrefabInternal\0" "m_PrefabParentObject\0" "m_Script\0" "m_StaticEditorFlags\0"
		"m_Type\0" "m_Version\0" "Object\0" "pair\0" "PPtr<Component>\0" "PPtr<GameObject>\0" "PPtr<Material>\0" "PPtr<MonoBehaviour>\0" "PPtr<MonoScript>\0"
		"PPtr<Object>\0" "PPtr<Prefab>\0" "PPtr<Sprite>\0" "PPtr<TextAsset>\0" "PPtr<Texture>\0" "PPtr<Texture2D>\0" "PPtr<Transform>\0" "Prefab\0"
		"Quaternionf\0" "Rectf\0" "RectInt\0" "RectOffset\0" "second\0" "set\0" "short\0" "size\0" "SInt16\0" "SInt32\0" "SInt64\0" "SInt8\0" "staticvector\0"
		"string\0" "TextAsset\0" "TextMesh\0" "Texture\0" "Texture2D\0" "Transform\0" "TypelessData\0" "UInt16\0" "UInt32\0" "UInt64\0" "UInt8\0" "unsigned int\0" "unsigned long long\0" "unsigned short\0"
		"vector\0" "Vector2f\0" "Vector3f\0" "Vector4f\0" "m_ScriptingClassIdentifier"
	;
	return offset & (1 << 31) ? builtin_strings + (offset & 0x7fffffff) : strings + offset;
}

//-----------------------------------------------------------------------------
//	Serialized Binary
//-----------------------------------------------------------------------------

ISO_ptr<void> UNITY_ReadBinary(tag id, istream_ref file) {
	unity::Scene	scene;
	bool			bigendian;
	if (!scene.ReadBinary(file, bigendian))
		return ISO_NULL;

	bool	neutral		= scene.platform == unity::kBuildNoTargetPlatform;
	uint32	id_offset	= scene.platform == unity::kBuildNoTargetPlatform ? 20 : 0;

	ISO_ptr<anything>	p(id);
	for (auto *i = scene.objects.begin(), *e = scene.objects.end(); i != e; ++i) {
		file.seek(i->b.byteStart);

	#if 1
		unity::Type		&type	= scene.types[i->b.typeID];
		p->Append(type.CreateISO(format_string("%i", i->a), malloc_block(file, i->b.byteSize)));
	#else
		switch (i->b.typeID) {
			// named assets
			case unity::Class::AnimationClip: {
				malloc_block	block(file, i->b.byteSize);
				p->Append(MakePtr(get_string((uint8*)block + (neutral ? 12 : 0), bigendian), unity::Object(i->b, block)));
				break;
			}
			case unity::Class::Material:
			case unity::Class::Mesh:
			case unity::Class::Shader:
			case unity::Class::AudioClip:
			case unity::Class::MonoScript:
			{
				malloc_block	block(file, i->b.byteSize);
				p->Append(MakePtr(get_string((uint8*)block + (neutral ? 20 : 0), bigendian), unity::Object(i->b, block)));
				break;
			}
			case unity::Class::Texture2D: {
				if (true) {
					malloc_block	block(file, i->b.byteSize);
					count_string	name	= get_string((uint8*)block + (neutral ? 20 : 0), bigendian);
					memory_block	sub		= block.slice(align(name.end(), 4));
					p->Append(bigendian
						? ((unity::Texture2Dx<true>*)sub)->Read(name)
						: ((unity::Texture2Dx<false>*)sub)->Read(name)
					);
					break;
				}
			}
			default:
				p->Append(MakePtr(format_string("%i", i->a), unity::Object(i->b, file)));
				break;
		}
	#endif
	}

#if 0
	uint32	ntypes = num_elements32(scene.types);
	const ISO::Type **types = alloc_auto(const ISO::Type *, ntypes), **ptypes = types;
	for (auto &i : scene.types)
		*ptypes++ = i.iso_type;

	WriteCHeader(types, ntypes, unconst(FileAppend("Q:\\test.h")));
#endif
	return p;
}


//-----------------------------------------------------------------------------
//	Serialized Text
//-----------------------------------------------------------------------------

const char	kUnityTextMagicString[]		= "%YAML 1.1";

struct UNITY_YAML {
	dynamic_array<ISO_ptr<void> > stack;
	void		Begin(const char *tag, const void *key, YAMLreader::CONSTRUCT c) {
		if (str(tag).begins("tag:unity3d.com,2011:")) {
			int	id	= from_string<int>(tag + sizeof("tag:unity3d.com,2011:") - 1);
			if (auto type = unity::GetStandardType((unity::Class::id)id)) {
				stack.push_back(MakePtr(type));
				return;
			}

		} else if (ISO_ptr<void>::Ptr(unconst(key)).IsType<string>()) {
			if (ISO::Browser b = ISO::Browser(stack.back())[*(const char**)key]) {
				stack.push_back(MakePtr(b.GetTypeDef()));
				return;
			}
		} else if (stack.back().GetType()->GetType() == ISO::OPENARRAY && !stack.back().IsType<anything>()) {
			stack.push_back(MakePtr(((ISO::TypeOpenArray*)stack.back().GetType())->subtype));
			return;
		}
		stack.push_back(ISO_ptr<anything>(0));
	}
	void*		End(YAMLreader::CONSTRUCT c) {
		return stack.pop_back_retref();
	}
	void*		Value(const char *tag, const void *key, const char *val) {
/*		if (key) {
			ISO_ptr<void>	k = ISO_ptr<void>::Ptr(unconst(key));
			if (k.IsType<string>()) {
				if (ISO::Browser b = ISO::Browser(stack.back())[*(const char**)key]) {
				}
			}
		}
*/
		ISO_ptr<string>	s(0, val);
		ISO::Value		*v	= s.Header();
		v->addref();
		return s;
	}

	void		Entry(const void *key, void *val) {
		ISO_ptr<void>	p = ISO_ptr<void>::Ptr(val);
		if (key) {
			if (ISO_ptr<void>::Ptr(unconst(key)).IsType<string>()) {
				tag2	id = *(const char**)key;
				if (ISO::Browser b = ISO::Browser(stack.back())[id]) {
					b.Set(p);
					return;
				}
				p.SetID(id);
			}
		}
		ISO::Browser(stack.back()).Append().Set(p);
	}

	template<typename T> static void *make_value(const T &t) {
		ISO_ptr<T>	p(0, t);
		p.Header()->addref();
		return p;
	}

	UNITY_YAML(tag id) {
		stack.push_back(ISO_ptr<anything>(id));
	}
};

ISO_ptr<void>	UNITY_ReadText(tag id, istream_ref file) {
	char compare[sizeof(kUnityTextMagicString) - 1];
	if (!file.read(compare) || !str(compare).begins(kUnityTextMagicString))
		return ISO_NULL;

	file.seek(0);
	UNITY_YAML	unity_yaml(id);
	YAMLreader	reader(&unity_yaml, file);
	try {
		reader.Read();
		return unity_yaml.stack[0];
	} catch (const char *error) {
		throw_accum(error << " at line " << reader.GetLineNumber());
		return ISO_ptr<void>();
	}

	return ISO_NULL;
}

//-----------------------------------------------------------------------------
//	File readers
//-----------------------------------------------------------------------------

class UnityBinaryFileHandler : public FileHandler {
	const char*		GetDescription() override { return "Unity Binary File"; }

	int				Check(istream_ref file) override {
		unity::SerializedFileHeader	h;
		file.seek(0);
		int		v = file.read(h) ? h.Version() : 0;
		return	v == 0 ? CHECK_DEFINITE_NO : v == 9 ? CHECK_PROBABLE : CHECK_POSSIBLE;
	}

	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		return UNITY_ReadBinary(id, file);
	}
} unity_binary;

class UnityTextFileHandler : public FileHandler {
	const char*		GetDescription() override { return "Unity Text File"; }

	int				Check(istream_ref file) override {
		file.seek(0);
		char compare[sizeof(kUnityTextMagicString) - 1];
		return file.read(compare) && str(compare).begins(kUnityTextMagicString) ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}

	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		return UNITY_ReadText(id, file);
	}
} unity_text;


class UnityAssetFileHandler : public FileHandler {
	const char*		GetExt() override { return "asset"; }
	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		unity::SerializedFileHeader	h;
		int		v = file.read(h) ? h.Version() : 0;
		file.seek(0);
		return v == 0
			? UNITY_ReadText(id, file)
			: UNITY_ReadBinary(id, file);
	}
} unity_asset;

class UnitySceneFileHandler : public UnityAssetFileHandler {
	const char*		GetExt() override { return "unity"; }
} unity_scene;

class UnityPrefabFileHandler : public UnityAssetFileHandler {
	const char*		GetExt() override { return "prefab"; }
} unity_prefab;

//-----------------------------------------------------------------------------
//	UnityPackage
//-----------------------------------------------------------------------------
#include "comms/zlib_stream.h"
#include "tar.h"
#include "comms/gz.h"

class UnityPackageFileHandler : public FileHandler {
	const char*		GetExt() override { return "unitypackage"; }
	ISO_ptr<void>	Read(tag id, istream_ref file) override;
} unity_package;

ISO_ptr<void> UnityPackageFileHandler::Read(tag id, istream_ref file) {
	struct entry {
		malloc_block	meta;
		malloc_block	data;
		string			path;
	};
	hash_map<iso::crc32, entry>	hash;

	GZheader	gz;
	if (!file.read(gz))
		return ISO_NULL;

//	GZistream		z(file);
	deflate_reader	z(file);
	ISO_ptr<Folder>	p(id);

	for (TARheader th = z.get(); th.filename[0]; th = z.get()) {
		if (th.filename[0] == 0)
			break;

		z.seek_cur(512 - sizeof(th));

		filename	name	= th.filename;
		uint64		size;
		get_num_base<8>(skip_whitespace(th.filesize), size);
		streamptr	end		= (z.tell() + size + 511) & ~511;

		if (size && !name.find("PaxHeader")) {

			malloc_block	data(z, uint32(size));
			if (name.name_ext() == "asset") {
				hash[crc32(name.dir())].put().data = move(data);

			} else if (name.name() == "pathname") {
				auto	path = str((const char*)data, (const char*)data.end());
				if (auto end = path.find('\n'))
					path = path.slice_to(end);
				hash[crc32(name.dir())].put().path = path;

			} else {
				//hash[name.dir()].data = data;
			}
		}

		z.seek(end);
	}
	for (auto &i : hash) {
		ISO_TRACEF("path=") << i.path << '\n';
		if (i.data) {
			const char	*name;
			ISO_ptr<Folder> f = GetDir(p, i.path, &name);
			ISO_ptr<ISO_openarray<xint8> >r(name);
			memcpy(r->Create(i.data.size32()), i.data, i.data.size32());
			f->Append(r);
		}
	}

	return p;
}

//-----------------------------------------------------------------------------
//	Unity Info
//-----------------------------------------------------------------------------

struct Thumbnail {
	int				m_Format;
	int				m_Width;
	int				m_Height;
	int				m_RowBytes;
	malloc_block	data;
};
struct Representation {
	ISO_ptr<bitmap>	thumbnail;
	GUID			guid;
	string			path;
	int				localIdentifier;
	int				thumbnailClassID;
	int				flags;
	string			scriptClassName;
};
struct Labels {
	ISO_openarray<string>	m_Labels;
};
struct AssetBundleFullName {
	string	m_AssetBundleName;
	string	m_AssetBundleVariant;
};

struct Info {
	Representation					mainRepresentation;
	ISO_openarray<Representation>	representations;

	Labels					labels;
	int						assetImporterClassID;
	AssetBundleFullName		assetBundleFullName;
	ISO_openarray<string>	externalReferencesForValidation;
};

ISO_DEFUSERCOMPV(Thumbnail, m_Format, m_Width, m_Height, m_RowBytes, data);
ISO_DEFUSERCOMPV(Labels, m_Labels);
ISO_DEFUSERCOMPV(AssetBundleFullName, m_AssetBundleName, m_AssetBundleVariant);
ISO_DEFUSERCOMPV(Representation, thumbnail, guid, path, localIdentifier, thumbnailClassID, flags, scriptClassName);
ISO_DEFUSERCOMPV(Info, mainRepresentation, representations, labels, assetImporterClassID, assetBundleFullName, externalReferencesForValidation);

struct UNITY_INFO_YAML {
	dynamic_array<ISO_ptr<void> > stack;
	void		Begin(const char *tag, const void *key, YAMLreader::CONSTRUCT c) {
		if (ISO_ptr<void>::Ptr(unconst(key)).IsType<string>()) {
			tag2	id = *(const char**)key;
			if (id == "thumbnail") {
				stack.push_back(ISO_ptr<Thumbnail>(0));
			} else if (ISO::Browser b = ISO::Browser(stack.back())[id]) {
				stack.push_back(MakePtr(b.GetTypeDef()));
				return;
			}
		} else if (stack.back().GetType()->GetType() == ISO::OPENARRAY && !stack.back().IsType<anything>()) {
			stack.push_back(MakePtr(((ISO::TypeOpenArray*)stack.back().GetType())->subtype));
			return;
		}
		//stack.push_back(ISO_ptr<anything>(0));
	}
	void*		End(YAMLreader::CONSTRUCT c) {
		if (stack.size())
			return stack.pop_back_retref();
		return 0;
	}
	void*		Value(const char *tag, const void *key, const char *val) {
		ISO_ptr<string>	s(0, val);
		ISO::Value		*v	= s.Header();
		v->addref();
		return s;
	}

	void		Entry(const void *key, void *val) {
		ISO_ptr<void>	p = ISO_ptr<void>::Ptr(val);
		if (key) {
			if (ISO_ptr<void>::Ptr(unconst(key)).IsType<string>()) {
				tag2	id = *(const char**)key;
				if (id == "_typelessdata") {
					if (p) {
						const char		*s		= *(const char**)val;
						malloc_block	*data	= ISO::Browser(stack.back())["data"];
						uint8			*p		= data->create(strlen(s) / 2);
						while (p < data->end()) {
							*p++ = from_digit(s[0]) * 16 + from_digit(s[1]);
							s += 2;
						}
						return;
					}
				}
				if (id == "thumbnail") {
					Thumbnail	*th = p;
					p = unity::LoadBitmap(id, th->m_Format, th->m_Width, th->m_Height, th->m_RowBytes, th->data);
				}
				ISO::Browser(stack.back()).SetMember(id, p);
			}
		}
		ISO::Browser(stack.back()).Append().Set(p);
	}

	template<typename T> static void *make_value(const T &t) {
		ISO_ptr<T>	p(0, t);
		p.Header()->addref();
		return p;
	}

	UNITY_INFO_YAML(tag id) {
		stack.push_back(ISO_ptr<Info>(id));
	}
};

class UnityInfoFileHandler : public FileHandler {
	static const char header[];
	static const char trailer[];

	struct block {
		char	id[16];
		uint32	offset;
		bool	is(const char *_id) {
			return memcmp(id, _id, 16) == 0;
		}
	};

	const char*		GetExt() override { return "info"; }
	const char*		GetDescription() override { return "Unity metadata info file"; }
	int				Check(istream_ref file) override {
		file.seek(0);
		block	b;
		if (!file.read(b) || !b.is(header))
			return CHECK_DEFINITE_NO;

		file.seek_end(-sizeof(block));
		if (!file.read(b) || !b.is(trailer))
			return CHECK_DEFINITE_NO;

		return CHECK_PROBABLE;
	}
	ISO_ptr<void>	Read(tag id, istream_ref file) override;
} unity_info;

const char UnityInfoFileHandler::header[]	= "PreviewAssetData";
const char UnityInfoFileHandler::trailer[]	= "AssetInfo_______";

ISO_ptr<void> UnityInfoFileHandler::Read(tag id, istream_ref file) {
	block	b0, b1;
	if (!file.read(b0) || !b0.is(header))
		return ISO_NULL;

	file.seek_end(-sizeof(block));
	if (!file.read(b1) || !b1.is(trailer))
		return ISO_NULL;

	file.seek(sizeof(block) + b0.offset);
	istream_offset	file2(copy(file), b1.offset);

	UNITY_INFO_YAML	unity_yaml(id);
	YAMLreader		reader(&unity_yaml, file2);
	try {
		reader.Read();
		return unity_yaml.stack[0];
	} catch (const char *error) {
		throw_accum(error << " at line " << reader.GetLineNumber());
		return ISO_NULL;
	}

	return ISO_NULL;

	if (FileHandler *yaml = FileHandler::Get("yaml")) {
		file.seek(sizeof(block) + b0.offset);
		return yaml->Read(id, make_reader_offset(file, b1.offset));
	}

	return ISO_NULL;
}

