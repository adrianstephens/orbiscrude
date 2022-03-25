#include "iso/iso_files.h"
#include "iso/iso_convert.h"
#include "base/algorithm.h"
#include "base/vector.h"
#include "maths/geometry.h"
#include "codec/lz4.h"
#include "vector_iso.h"
#include "3d/model_utils.h"

//------------------------------------------------------------------------------
// ThatGameCompany files
//------------------------------------------------------------------------------

using namespace iso;

struct LoToc {
	static const uint32 kMagic			= 0x4C434754;	// "TGCL" in little endian
	static const uint32 kInvalidValue	= 0xffffffff;

	struct Var {
		enum Type : uint32 {
			Pod = 0,
			String,
			ObjectPtr,
			ObjectPtrArray
		};
		Type	type;
		uint32	nameOffset;
		uint32	size;
		// For arrays that allocate their own objects, the Idx of the LoClass for the array elements
		// For arrays of pointers to objects already in the data stream, kLoInvalidValue
		uint32	arrayType;
	};

	struct Class {
		uint32 nameOffset;
		uint32 memVarIdx;
		uint32 memVarCount;
		auto vars(const LoToc *toc) const {
			return make_range_n((const Var*)((uint8*)toc + toc->memVarOffset) + memVarIdx, memVarCount);
		}
	};

	uint32 magic;
	uint32 version;

	uint32 classCount;
	uint32 memVarCount;
	uint32 objectCount;
	uint32 objectPtrCount;

	uint32 classOffset;
	uint32 memVarOffset;
	uint32 stringOffset;
	uint32 dataOffset;

	uint32 fileSize;

	bool	valid(streamptr size) const {
		return magic == kMagic && fileSize == size;
	}
	auto	classes() const {
		return make_range_n((const Class*)((uint8*)this + classOffset), classCount);
	}
	auto	data() const {
		return const_memory_block((const char*)this + dataOffset, (const char*)this + fileSize);
	}
	auto	get_string(uint32 offset) {
		return (const char*)this + stringOffset + offset;
	}
};

// known types:
// Clump
// Float

static const char *size16_fields[] = {
	"scale", "quat", "pos", "color", "vecValue",
};

static const char *size64_fields[] = {
	"transform",
};

struct TGCLoader {
	struct ObjectPtr {
		ISO_ptr<void>*	dest;
		uint32			obj;
		ObjectPtr(ISO_ptr<void> *dest, uint32 obj) : dest(dest), obj(obj) {}
	};

	dynamic_array<ObjectPtr>	patches;
	dynamic_array<ISO::TypeUserComp*>	classes;

	void	GetClasses(LoToc &toc) {
		hash_map<uint32, ISO::Type*>	pods;

		for (auto &c : toc.classes()) {
			ISO::TypeUserComp	*comp = new(c.memVarCount) ISO::TypeUserComp(toc.get_string(c.nameOffset));
			for (auto &f : c.vars(&toc)) {
				const char *fname		= toc.get_string(f.nameOffset);
				const ISO::Type	*type	= nullptr;

				switch (f.type) {
					case LoToc::Var::Pod: {
						switch (f.size) {
							case 1:
								type = ISO::getdef<uint8>();
								break;

							case 4:
								type = ISO::getdef<uint32>();
								break;

							case 16:
								if (find_check(size16_fields, str(fname)))
									type = ISO::getdef<float[4]>();
								break;

							case 64:
								if (find_check(size64_fields, str(fname)))
									type = ISO::getdef<float[4][4]>();
								break;
						}
						if (!type) {
							auto&	t = pods[f.size].put();
							if (!t)
								t = new ISO::TypeArrayT<uint8>(f.size);
							type = t;
						}
						break;
					}
					case LoToc::Var::String:
						type = ISO::getdef<string>();
						break;

					case LoToc::Var::ObjectPtr:
						type = ISO::getdef<ISO::ptr<void>>();
						break;

					case LoToc::Var::ObjectPtrArray:
						//if (f.arrayType == LoToc::kInvalidValue) {
							type = ISO::getdef<anything>();
						//}
						break;
				}
				comp->Add(type, toc.get_string(f.nameOffset));
			}
			classes.push_back(comp);
		}
	}

	ISO_ptr<void> LoadObject(LoToc &toc, byte_reader &r, uint32 type, const char *name) {
		auto	&c			= toc.classes()[type];
		auto	t			= classes[type];

		auto	obj			= ISO::MakePtr(t, name);
		uint8	*dest		= (uint8*)(void*)obj;

		// Load all the object's mem vars
		auto	e = t->begin();
		for (auto &f : c.vars(&toc)) {
			uint8	*dest	= (uint8*)(void*)obj + e->offset;

			switch (f.type) {
				case LoToc::Var::Pod:
					r.readbuff(dest, f.size);
					break;

				case LoToc::Var::String:
					*(string*)dest = r.get<string>();
					break;

				case LoToc::Var::ObjectPtr: {
					uint32	obj = r.get();
					if (obj != LoToc::kInvalidValue)
						patches.emplace_back((ISO_ptr<void>*)dest, obj);
					break;
				}

				case LoToc::Var::ObjectPtrArray: {
					uint32		n	= r.get();
					anything	*a	= new(dest) anything(n);
					for (auto &i : *a) {
						if (f.arrayType == LoToc::kInvalidValue) {
							uint32	obj = r.get();
							if (obj != LoToc::kInvalidValue)
								patches.emplace_back(&i, obj);
						} else {
							i = LoadObject(toc, r, f.arrayType, 0);
						}
					}
					break;
				}
			}
			++e;
		}

		return obj;
	}

	void Patch(ISO_ptr<void> *p) {
		for (auto &i : patches)
			*i.dest = p[i.obj];
	}

};

class TGCFileHandler : public FileHandler {
//	const char*		GetExt() override { return "bin"; }

	int				Check(istream_ref file) override {
		file.seek(0);
		LoToc	toc;
		return file.read(toc) && toc.valid(file.length()) ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}
	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		LoToc	toc0;
		if (!file.read(toc0) || !toc0.valid(file.length()))
			return ISO_NULL;

		malloc_block	mb(toc0.fileSize);
		*mb = toc0;
		mb.slice(sizeof(toc0)).read(file);

		LoToc		&toc	= *mb;
		TGCLoader	loader;

		loader.GetClasses(toc);

		byte_reader	r(toc.data());

		ISO_ptr<anything>	p(id, toc.objectCount);

		// Chew through the data blob, instantiating objects and filling their mem vars
		for (auto &i : *p) {
			uint32	type	= r.get();
			string	name	= r.get<string>();
			i = loader.LoadObject(toc, r, type, name);
		}

		// Now that all the objects have been instantiated, we can patch pointers
		loader.Patch(p->begin());
		return p;
	}
} tgc;


//--------------------------------------------------------------------------------
// BinaryFile class
//--------------------------------------------------------------------------------

uint32 CompressionULerp( float x, uint32 a, uint32 b ) {
	uint64 a64 = a;
	uint64 b64 = b;
	return (uint32)(a64 + (b64 - a64) * x + 0.5f);
}

struct TGCBinaryFile {
	static const uint32 kMaxSize		= 16 * 1024 * 1024;

	struct Header {
		uint32	fourCC;
		uint32	version;
	};

	struct Section {
		char	name[4];
		uint32	offset;
		uint32	length;
	};

	struct TableOfContents {
		static const uint32 kTOCSectionCount = 8;
		uint8	entryCount;
		Section entries[kTOCSectionCount];
	};

	Header			header;
	anything		sections;

	void Load(istream_ref file);

};

void TGCBinaryFile::Load(istream_ref file) {
	file.read(header);

	TableOfContents	toc;
	file.read(toc);

	sections.Create(toc.entryCount);

	// Load table of contents
	for (uint32 i = 0; i < toc.entryCount; ++i) {
		file.seek(toc.entries[i].offset);
		malloc_block compressedBuffer(file, toc.entries[i].length);
		malloc_block buffer(kMaxSize);
		size_t	written;
		transcode(LZ4::decoder(), buffer, compressedBuffer, &written);
		sections[i] = ISO::MakePtr(str(toc.entries[i].name), move(buffer.resize(written)));
	}
}


template<typename R> string read_string(R &r) {
	uint32	len = r.template get<uint32>();
	string	s(len);
	r.readbuff(s, len);
	return s;
}

//--------------------------------------------------------------------------------
// Animation
//--------------------------------------------------------------------------------

// Used by AnimationInstance and AnimationPack
struct SQT {
	float3		scale;
	quaternion	quat;
	position3	trans;
	bool read(istream_ref file) { return iso::read(file, scale, quat, trans); }
};

struct AnimTrackRange {
	static constexpr float kMaxRange = 30.0f;  // bigger than this range will use full 32-bit precision, checked in meshCompiler
	float3	min;
	float3	range;
	float3	rangeToFloat;

	AnimTrackRange() {
		range = float3(kMaxRange);
		min	  = -range * 0.5f;
	}
	AnimTrackRange(float3 _min, float3 _max) {
		min	  = _min;
		range = _max - _min;
	}

	void ExpandVec3(float3& v) const { v = v * range + min; }

	float3 NormalizeRangeVec3(const float3& v) const {
		float3 invRange{1.0f / range[0], 1.0f / range[1], 1.0f / range[2]};
		return (v - min) * invRange;
	}
};

struct AnimHalfVec {
	static constexpr float toFloat = 1.0f / 65535.0f;
	static constexpr float toUint  = 65535.0f;

	uint16 x, y, z;

	// copy to this to vector param v
	void CopyTo(float3& v, const AnimTrackRange& range) const {
		v = float3{x, y, z} * toFloat;
		range.ExpandVec3(v);
	}

	void CopyTo(float3p& v, const AnimTrackRange& range) const {
		float3 temp = float3{x, y, z} * toFloat;
		range.ExpandVec3(temp);
		v = temp;
	}

	void Set(const float3& v, const AnimTrackRange& range) {
		float3 temp	= range.NormalizeRangeVec3(v) * toUint;
		x	= clamp((int)floor(temp.x + 0.5f), 0, 65535);
		y	= clamp((int)floor(temp.y + 0.5f), 0, 65535);
		z	= clamp((int)floor(temp.z + 0.5f), 0, 65535);
	}
};

static uint64 CompressQuat64bits(const quaternion& q) {
	static const float kMult = 65535.0f / 2.0f;
	// 16 bits each for x,y,z,w
	// Storing w-component because calculating it on sample is a noticeable speed hit with the quat interpolation
	uint64 x	= clamp((int)floor((q.v.x + 1.0f) * kMult + 0.5f), 0, 65535);
	uint64 y	= clamp((int)floor((q.v.y + 1.0f) * kMult + 0.5f), 0, 65535);
	uint64 z	= clamp((int)floor((q.v.z + 1.0f) * kMult + 0.5f), 0, 65535);
	uint64 w	= clamp((int)floor((q.v.w + 1.0f) * kMult + 0.5f), 0, 65535);
	uint64 compValue = x + (y << 16) + (z << 32) + (w << 48);

	return compValue;
}

static void inline DecompressQuatUint64(quaternion& quat, const uint64& compQuat) {
	static const float kMult = 2.0f / 65535.0f;

	// uncompress quat
	float x = (compQuat & 65535);
	float y = ((compQuat >> 16) & 65535);
	float z = ((compQuat >> 32) & 65535);
	float w = (compQuat >> 48);

	quat = quaternion(float4{x, y, z, w} * kMult - float4(1.0f));
}

//--------------------------------------------------------------------------------
// AnimationPack
//--------------------------------------------------------------------------------

class AnimPackData {
	static const uint32 kVersion		= 11;
	static const uint32 kMinVersion		= 7;
	static const uint32 kMaxSize		= 3 * 1024 * 1024;
	static const uint32 kMaxBones		= 256;
	static const uint8	kInvalidBone	= 255;

	struct BoneMirrorInfo {
		float4x4	converterMat;
		quaternion	srcQuat;
		quaternion	srcParentConj;
		quaternion	dstParent;
		quaternion	dstConj;
		int			srcBone;
		int			dstBone;
		bool		hasParents;
	};
	const BoneMirrorInfo* m_boneMirrorInfo;

public:
	enum Type			{ ANIMS_STAND_ALONE, ANIMS_IN_MESH };
	enum Compression	{ COMP_NONE, COMP_LZ4_FILE, COMP_HALF_LZ4_FILE };
	enum animFlags {
		AnimFlags_HalfSizeTrans = 0x01,
	};

	char					name[64];
	Type					type;

	int32					frameCount;
	uint8					compression;
	int32					frameRate;

	// Skeleton variables
	uint32					boneCount;
	dynamic_array<SQT>		restKeys;
	dynamic_array<int32>	boneParents;
	dynamic_array<float4x4> boneLinkMats;
	dynamic_array<string>	boneNames;

	struct AnimPackKeys {
		// defaultKeys get memcpy'd into result SQTs, then animation changes are applied
		dynamic_array<SQT>			defaultKeys;
		dynamic_array<SQT>			mirroredDefaultKeys;

		// Key stores - animations have indices into the keys stored here
		dynamic_array<quaternion>	quatKeysFloat;
		dynamic_array<uint64>		quatKeysUint;
		dynamic_array<float3p>		scaleKeys;
		dynamic_array<float3p>		transKeysFloat;
		dynamic_array<AnimHalfVec>	transKeysHalf;
		malloc_block				boneInfo;

		void Allocate(uint32 totalScaleCount, uint32 totalQuatCount, uint32 totalTransFloatCount, uint32 totalTransHalfCount, uint32 boneInfoPoolSize, uint8 compression) {
			scaleKeys.resize(totalScaleCount);
			if (compression == AnimPackData::COMP_HALF_LZ4_FILE)
				quatKeysUint.reserve(totalQuatCount);
			else
				quatKeysFloat.reserve(totalQuatCount);

			transKeysFloat.reserve(totalTransFloatCount);
			transKeysHalf.reserve(totalTransHalfCount);

			boneInfo.create(boneInfoPoolSize);
		}
		void ReadScaleKeys(istream_ref file, uint32 count) {
			readn(file, scaleKeys.expand(count), count);
		}
		void ReadTransKeys(istream_ref file, uint32 count, uint8 compression, int32 flags) {
			if (compression == COMP_HALF_LZ4_FILE && (flags & AnimFlags_HalfSizeTrans))
				readn(file, transKeysHalf.expand(count), count);
			else
				readn(file, transKeysFloat.expand(count), count);
		}
		void ReadQuatKeys(istream_ref file, uint32 count, uint8 compression) {
			if (compression == COMP_HALF_LZ4_FILE)
				readn(file, quatKeysUint.expand(count), count);
			else
				readn(file, quatKeysFloat.expand(count), count);
		}
	} keys;

	class AnimationData {
		enum keyedFlags {
			FLAG_scale		= 0x01,
			FLAG_quat		= 0x02,
			FLAG_trans		= 0x04,
			FLAG_firstScale = 0x08,
			FLAG_firstQuat	= 0x10,
			FLAG_firstTrans = 0x20,
		};
	public:
		// using start & end frames to find the matching animation
		int32	startFrame;
		int32	endFrame;
		int32	flags;
		float3	boundsMin, boundsMax;
		AnimTrackRange transRange;	// would make more sense to be per bone, but would need time to make sure that gives worthwhile benefit

		// indices into channel keys
		uint32	quatKeysStart;
		uint32	scaleKeysStart;
		uint32	transKeysStart;

		// indices into bone info
		uint32	boneInfoStart;

		// number of keyed bones of each channel type
		uint16	firstKeyScaleBoneCount;
		uint16	firstKeyQuatBoneCount;
		uint16	firstKeyTransBoneCount;
		uint16	keyedScaleBoneCount;
		uint16	keyedQuatBoneCount;
		uint16	keyedTransBoneCount;

		uint8*	Load(istream_ref stream, int boneCount, AnimPackKeys &keys, uint8 *out, int version, uint8 compression);

		uint8*	ComputeKeyedChannelFlags(int boneCount, uint8* boneKeyFlags, uint8 *out, keyedFlags flag, uint16& keyedBoneCount) {
			keyedBoneCount = 0;
			for (int bone = 0; bone < boneCount; bone++) {
				if (boneKeyFlags[bone] & flag) {
					*out++ = bone;
					keyedBoneCount++;
				}
			}
			return out;
		}
	};

	dynamic_array<AnimationData>	animations;

	AnimPackData(istream_ref file, Type type);
	void	LoadAnimations(istream_ref file, int boneCount, int version);
};

uint8 *AnimPackData::AnimationData::Load(istream_ref file, int boneCount, AnimPackKeys &keys, uint8 *out, int version, uint8 compression) {
	// read animation data
	file.read(startFrame);
	file.read(endFrame);
	file.read(flags);
	if (version >= 9) {
		file.read(boundsMin);
		file.read(boundsMax);
	}

	transRange = AnimTrackRange(float3(-20.0f), float3(20.0f));
	if (version >= 11) {
		file.read(transRange.min);
		file.read(transRange.range);
	}
	const bool useHalfTrans = (compression == COMP_HALF_LZ4_FILE && (flags & AnimFlags_HalfSizeTrans));

	// read keyed flags.
	malloc_block	boneKeyFlags(file, boneCount);

	// reads from bone info to set bone mapping from keyed bones <=> all bones, and also counts (eg. keyedQuatBoneCount, etc.)
	boneInfoStart = out - keys.boneInfo;

	// Fills bone info structure with the maps for each channel type

	// Scale
	out = ComputeKeyedChannelFlags(boneCount, boneKeyFlags, out, FLAG_firstScale, firstKeyScaleBoneCount);
	out = ComputeKeyedChannelFlags(boneCount, boneKeyFlags, out, FLAG_scale, keyedScaleBoneCount);

	// Quat
	out = ComputeKeyedChannelFlags(boneCount, boneKeyFlags, out, FLAG_firstQuat, firstKeyQuatBoneCount);
	out = ComputeKeyedChannelFlags(boneCount, boneKeyFlags, out, FLAG_quat, keyedQuatBoneCount);

	// Translation
	out = ComputeKeyedChannelFlags(boneCount, boneKeyFlags, out, FLAG_firstTrans, firstKeyTransBoneCount);
	out = ComputeKeyedChannelFlags(boneCount, boneKeyFlags, out, FLAG_trans, keyedTransBoneCount);

	//out = ComputeAllKeyedIndices(boneCount, keyedBones, out);

	// store where our key channel indices start in animation pack key store
	scaleKeysStart = keys.scaleKeys.size32();
	quatKeysStart  = compression ? keys.quatKeysUint.size32() : keys.quatKeysFloat.size32();
	transKeysStart = useHalfTrans ? keys.transKeysHalf.size32() : keys.transKeysFloat.size32();

	// read channel defaults for this animation (only ones that are different from pack defaultKeys)
	keys.ReadScaleKeys(file, firstKeyScaleBoneCount);
	keys.ReadQuatKeys(file, firstKeyQuatBoneCount, compression);
	keys.ReadTransKeys(file, firstKeyTransBoneCount, compression, flags);

	// Read any channels that are fully keyed in this animation
	int32 frameCount = (endFrame - startFrame) + 1;
	for (int f = 0; f < frameCount; f++) {
		keys.ReadScaleKeys(file, keyedScaleBoneCount);
		keys.ReadQuatKeys(file, keyedQuatBoneCount, compression);
		keys.ReadTransKeys(file, keyedTransBoneCount, compression, flags);
	}
	return out;
}

void AnimPackData::LoadAnimations(istream_ref file, int boneCount, int version) {
	// Read total animation and key counts
	uint32	animationCount				= file.get<uint32>();
	uint32	totalScaleKeysCount			= file.get<uint32>();
	uint32	totalQuatKeysCount			= file.get<uint32>();
	uint32	totalTransFloatKeysCount	= file.get<uint32>();

	uint32	totalTransHalfKeysCount		= version >= 10 ? file.get<uint32>() : 0;
	uint32	totalKeyedChannelCount		= file.get<uint32>();

	// Allocate : pack default keys, animations, key stores for S,Q,T channels, and boneInfo uint8s (of size totalKeyedChannelCount)
	animations.resize(animationCount);
	keys.Allocate(totalScaleKeysCount, totalQuatKeysCount, totalTransFloatKeysCount, totalTransHalfKeysCount, totalKeyedChannelCount, compression);

	// Read pack default keys
	keys.defaultKeys.read(file, boneCount);

	// read the animations, this will fill in the key stores
	uint8	*out = keys.boneInfo;
	for (auto &i : animations)
		out = i.Load(file, boneCount, keys, out, version, compression);
}

AnimPackData::AnimPackData(istream_ref file, Type type) : type(type) {
	// Version check
	uint32 version	= file.get<uint32>();
	file.read(name);

	// Load animation pack data
	file.read(boneCount);
	file.read(frameCount);
	file.read(frameRate);
	file.read(compression);

	if (version >= 10)
		file.get<uint32>();//total size of bone names

	// Load skeleton
	boneLinkMats.resize(boneCount);
	boneParents.resize(boneCount);
	boneNames.resize(boneCount);

	for (int32 i = 0; i < boneCount; i++) {
		char	temp[64];
		file.read(temp);
		boneNames[i] = temp;
		file.read(boneLinkMats[i]);
		file.read(boneParents[i]);
	}

	// Our current use if there are no anims, only bone names are necessary for matching actual skeletons. We have lots of stripped anim skeletons, so this saves > 1 MB
	if (frameCount == 0)
		boneLinkMats.reset();

	// Load animation too if there is one
	if (frameCount > 0) {
		// Load RestKeys. Can these be combined with the default keys? Right now they are different though, restkeys frame 0 defaultkeys frame 1
		restKeys.read(file, boneCount);

		// Load the Key Frames
		if (compression == COMP_LZ4_FILE || compression == COMP_HALF_LZ4_FILE) {
			// Uncompress if needed. Pretty fast. Load actually tested faster with compression because of smaller disk read.
			uint32			compressedSize		= file.get<uint32>();
			uint32			uncompressedSize	= version >= 9 ? file.get<uint32>() : kMaxSize;
			malloc_block	decompBuff(uncompressedSize);
			size_t			written;
			transcode(LZ4::decoder(), decompBuff, malloc_block(file, compressedSize), &written);
			decompBuff.resize(written);
			LoadAnimations(lvalue(memory_reader(decompBuff)), boneCount, version);
		} else {
			LoadAnimations(file, boneCount, version);
		}
	}
}

//--------------------------------------------------------------------------------
// Mesh
//--------------------------------------------------------------------------------

struct Color32 {
	unorm8 r, g, b, a;
};

struct ColorD {
	unorm8 r, g, b, d;
	operator float3() const { return square(float3{(float)r, (float)g, (float)b}) / (float)d; }
	ColorD& operator=(param(float3) x) {
		float	m = max(reduce_max(x), 1);
		float3	s = sqrt(x / m);
		r = (float)s.x;
		g = (float)s.y;
		b = (float)s.z;
		d = reciprocal(m);
		return *this;
	}
};

struct ColorE {
	uint8	e;
	unorm8	r, g, b;
	operator float3() const { return float3{(float)r, (float)g, (float)b} * exp2(e - 128.f); }
};

struct MeshNorm52 {
	array_vec<norm8,4> normal;
	array_vec<norm8,4> tangent;
};

typedef array_vec<norm8,4> MeshNorm;// 4th component for alignment

struct MeshPos {
	float3p	pos;
	ColorD	col;
};

struct MeshUv {
	float16 uv0[4];
	float16 uv1[4];
};

struct MeshUvFixed {
	uint16 uv0[4];
	uint16 uv1[4];
};

struct MeshOffset {
	enum { OFFSET_TESS = 0, OFFSET_DETAIL = 1, OFFSET_START = 2, OFFSET_LODDIST = 3 };
	int8	pos[4];

	static int8		Encode(float val) { return clamp(val * 255.0f - 0.5f, -128.0f, 127.0f); }  // Range is -128 to 127 (inclusive)
	static float	Decode(int32 val) { return float(val) * (1.0f / 256.0f); }
	static float	GetTotal(const int8* offset) { return (int32(offset[0]) + offset[1]) * (1.0f / 256.0f); }
};

struct MeshLight {
	ColorD	light0;
	uint8	light1[4];	// x:sky_sun	y:sqrt(sun)		z:pt exp	w:pt mant
	uint8	light2[4];	// xyz:amb dir	w:occ
	ColorD	light3;
	operator float3() const { return float3{(float)light2[0], (float)light2[1], (float)light2[2]} / 256.f; }
};

struct MeshLight54 {
	ColorD	light0;
	uint8	light1[4];
	uint8	light2[4];
	operator float3() const { return light0; }

	operator MeshLight() const {
		MeshLight	m;
		raw_copy(*this, m);
		clear(m.light3);
		return m;
	}
};

struct MeshLight47 {
	ColorE	light0;
	uint8	light1[4];	//x:sky_sun		y:sqrt(sun)		z:pt exp	w:pt mant
	uint8	light2[4];	//xyz: amb dir	w:occ
	uint8	light3[4];

	//occ	= light2.w;
	//sun	= light1.y * light1.y;
	//ambDir = 2 * (light2.xyz - 0.5);
	//amb	= light0.yzw * exp2(255.0 * light0.x - 128) * mix(u_averageSkyColor, u_sunColor, light1.x);
	//amb	+= light1.w * exp2(255.0 * light1.z - 128) * u_pointLight;

	operator float3() const { return light0; }

	operator MeshLight() const {
		MeshLight	m;
		m.light0	= light0.operator float3();
		raw_copy(light1, m.light1);
		raw_copy(light2, m.light2);
		clear(m.light3);
		return m;
	}
};

struct MeshWeight {
	uint8	b[4];  // boneIndex
	uint8	w[4];  // weight
};

struct MeshColMat {
	uint8	col[4];
	uint8	mtrls[4];
	operator MeshWeight() const {
		MeshWeight	b;
		for (int i = 0; i < 4; i++) {
			b.b[i] = mtrls[i];
			b.w[i] = 0;
		}
		return b;
	}
};

struct MeshUvDiffs {
	float	uvDiff0;
	float	uvDiff1;
};

struct MeshOccluder {
	dynamic_array<float>	vertsPos;
	dynamic_array<uint32>	indices;

	MeshOccluder(istream_ref file) {
		uint32	vertCount	= file.get<uint32>();
		uint32	indexCount	= file.get<uint32>();
		vertsPos.read(file, vertCount * 3);
		indices.read(file, indexCount);
	}
};

struct MeshVert {
	float3p		pos;
	float3p		col;
	MeshNorm	normal;

	MeshVert(const float3p &pos, param(float3) _col, const MeshNorm &normal) : pos(pos), normal(normal) { col = _col; }
	MeshVert(const MeshPos &p, const MeshNorm &normal)		: MeshVert(p.pos, p.col, normal) {}
	MeshVert(const pair<const MeshPos&, const MeshNorm&> p)	: MeshVert(p.a.pos, p.a.col, p.b) {}
};

struct MeshLod {
	float	lodRadius;
	float3p	min, max;			// If there are animations in the mesh, min/max will be expanded
	float3p	minMesh, maxMesh;	// these will just include verts from the static mesh

	dynamic_array<MeshPos>		vertsPos;
	dynamic_array<MeshNorm>		vertsNorm;
	dynamic_array<MeshUv>		vertsUv;

	dynamic_array<MeshWeight>	weights;
	dynamic_array<uint32>		indices;
	dynamic_array<uint32>		adjacency;				// for each half-edge, the opposing half-edge (or ADJ_NONE for none)
	dynamic_array<uint32>		uniquePosIndexMap;		// [vert] -> [unique position]
	dynamic_array<uint32>		uniquePosNormIndexMap;	// [vert] -> [unique position/normal]
	dynamic_array<uint32>		uniqueStripIndices;		// triangle strip description of the mesh indexing on unique postions

	dynamic_array<float>		vertOcclusions;
	dynamic_array<uint32>		uniqueEdgeIndices;

	void Load(istream_ref file, int version, bool hasBones) {
		read(file, lodRadius, min, max);
		if (version >= 28) {
			file.read(minMesh);
			file.read(maxMesh);
		} else {
			minMesh = min;
			maxMesh = max;
		}

		// Set a default cull distance based on this mesh's size. BST uses this lodRadius for default too.
		if (lodRadius == infinity) {
			float diameter = len(float3(max) - float3(min));
			lodRadius  = (100.0f / 1.5f) * diameter;  // note : lodRadius is actually lodDistance. This culls when approximately 1.5% of vertical screen.
		}

		int32	vertCount, indexCount, uniquePosCount, uniquePosNormCount, uniqueStripIndexCount, vertOcclusionCount, uniqueEdgeCount, weightCounts[4];
		read(file, vertCount, indexCount, uniquePosCount, uniquePosNormCount, uniqueStripIndexCount, vertOcclusionCount, uniqueEdgeCount, weightCounts);

		vertsPos.read(file, vertCount);
		vertsNorm.read(file, vertCount);
		vertsUv.read(file, vertCount);

		if (hasBones)
			weights.read(file, vertCount);

		indices.read(file, indexCount);
		adjacency.read(file, indexCount);

		if (uniquePosCount > 0)
			uniquePosIndexMap.read(file, vertCount);

		if (uniquePosNormCount > 0)
			uniquePosNormIndexMap.read(file, vertCount);

		uniqueStripIndices.read(file, uniqueStripIndexCount);
		vertOcclusions.read(file, vertOcclusionCount);
		uniqueEdgeIndices.read(file, 2 * uniqueEdgeCount);
	}

	SubMesh mesh() const {
		SubMesh	m;
		m.minext	= minMesh;
		m.maxext	= maxMesh;
		m.technique	= ISO::root("data")["default"]["specular"];
		m.indices	= make_split_range<3>(indices);
		m.verts		= ISO::MakePtr(0, ISO::OpenArray<MeshVert>(transformc(int_range(vertsPos.size()), [this](size_t i) {
			return MeshVert(vertsPos[i], vertsNorm[i]);
			})));
		return m;
	}

};

struct MeshData {
	static const uint32	kMinVersion	= 25;
	static const uint32	kMaxVersion	= 28;
	static const int32	kVersion	= 28;
	static const uint32	kMaxSize	= 8 * 1024 * 1024;

	char					name[64];
	dynamic_array<MeshLod>	lods;
	const MeshOccluder*		occluder;
	class AnimPackData*		animData;

	void LoadLods(istream_ref file, int version, bool hasBones, bool hasOccluder) {
		for (auto &lod : lods)
			lod.Load(file, version, hasBones);
		occluder = hasOccluder ? new MeshOccluder(file) : nullptr;
	}

	void Load(istream_ref file, uint32 version) {
		uint32		lodCount;
		uint8		hasBones, hasOccluder;
		int32		compression = 0;

		read(file, name, lodCount, hasBones, hasOccluder);
		lods.resize(lodCount);

		if (version >= 27 && file.get<uint32>()) {
			uint32			compressedSize		= file.get<uint32>();
			uint32			uncompressedSize	= file.get<uint32>();
			malloc_block	decompBuff(uncompressedSize);
			size_t			written;
			transcode(LZ4::decoder(), decompBuff, malloc_block(file, compressedSize), &written);
			ISO_ASSERT( uncompressedSize == written );

			LoadLods(lvalue(memory_reader(decompBuff)), version, hasBones, hasOccluder);
		} else {
			LoadLods(file, version, hasBones, hasOccluder);
		}

		animData = hasBones ? new AnimPackData(file, AnimPackData::ANIMS_IN_MESH) : nullptr;
	}
};

//--------------------------------------------------------------------------------
// TerrainBlob
//--------------------------------------------------------------------------------

template<typename IndexType> struct Edge {
	IndexType a;  // Index "a" is always less than index "b"
	IndexType b;  // Both a and b are indices into the vert buffer
};

template<typename IndexType> struct EdgeIndex {
//	bool	  flip : 1;							  // If set, the start and end verts of the edge should be swapped.
	IndexType flip:1, index : sizeof(IndexType) * 8 - 1;  //@TODO: Can we get away with 7 bits? 128 edges per bin?
};

typedef Edge<uint16>		Edge16;
typedef Edge<uint32>		Edge32;
typedef EdgeIndex<uint16>	EdgeIndex16;
typedef EdgeIndex<uint32>	EdgeIndex32;

enum Material : uint8 {
	kMaterial_None			= 0x00, // 0 hard
	kMaterial_Missing		= 0x01, // 1 hard
	kMaterial_Transparent	= 0x02, // 2 no sound
	kMaterial_Void			= 0x03, // 3 void
	kMaterial_Particle		= 0x04, // 4 particles

	kMaterial_Cliff			= 0x10, // 16 rock
	kMaterial_Soil			= 0x11, // 17 rock
	kMaterial_CliffLight	= 0x12, // 18 rock
	kMaterial_WallDamaged	= 0x13, // 19 hard
	kMaterial_Wall			= 0x14, // 20 hard
	kMaterial_Gold			= 0x15, // 21 gold
	kMaterial_Glacier		= 0x16, // 22 glacier
	kMaterial_TileCeiling	= 0x17, // 23 hard
	kMaterial_TileFloor		= 0x18, // 24 hard
	kMaterial_TileWall		= 0x19, // 25 hard
	kMaterial_WallBrick		= 0x1a, // 26 hard
	kMaterial_SoilWet		= 0x1b, // 27 rockwet
	kMaterial_CliffWet		= 0x1c, // 28 rockwet
	kMaterial_Bone			= 0x1d, // 29 bone
	kMaterial_Wood			= 0x1e, // 30 wood
	kMaterial_Ceramics		= 0x1f, // 31 ceramics

	kMaterial_Sand			= 0x20, // 32 sand
	kMaterial_SandWet		= 0x21, // 33 sandwet
	kMaterial_SandLight		= 0x22, // 34 sand
	kMaterial_Snow			= 0x23, // 35 snow
	kMaterial_SandDeep		= 0x24, // 36 sand
	kMaterial_Mud			= 0x25, // 37 mud

	kMaterial_Grass			= 0x30, // 48 grass
	kMaterial_GrassWet		= 0x31, // 49 deadgrass
	kMaterial_GrassLight	= 0x32, // 50 grass
	kMaterial_GrassMoss		= 0x33, // 51 grass
	kMaterial_Darkshroom	= 0x34, // 52 darkshroom

	kMaterial_Cloud			= 0x50,

	kMaterial_Count			= 32,
};

struct TerrainBlob {
	enum BlobShape {
		kBlobShape_Cone,
		kBlobShape_Sphere,
		kBlobShape_Spiral,
		kBlobShape_Cube,
		kBlobShape_Rail,
		kBlobShape_Wedge,
		kBlobShape_Rail_Tube,
	};
	enum BlobType {
		kBlobType_None		=   0,
		kBlobType_Solid		= 100,
		kBlobType_Inverted	= 200,
		kBlobType_Paint		= 300,
		kBlobType_Replace	= 400,
		kBlobType_Erosion	= 500,
		kBlobType_Bump		= 600,
	};
	enum BlobNoise {
		kBlobNoise_Perlin,
		kBlobNoise_Rock,
		kBlobNoise_Ball,
		kBlobNoise_Blocks,
		kBlobNoise_Brick,
		kBlobNoise_Caustic,
		kBlobNoise_Cobblestone,
		kBlobNoise_Coral,
		kBlobNoise_Corroded,
		kBlobNoise_Cracks,
		kBlobNoise_FlatLayers,
		kBlobNoise_HillRock,
		kBlobNoise_ShelfRock,
		kBlobNoise_SoftRock,
		kBlobNoise_Tubes,
		kBlobNoise_Waves,
		kBlobNoise_Cloud,
		kBlobNoise_CloudFluffy,
		kBlobNoise_Pyro,
		kBlobNoise_Square,
		kBlobNoise_Count
	};

	float4x4	transform;
	int			order;
	float		stickiness;
	BlobType	type;
	uint32		bstGuid;

	float		cornerRadius;
	float		density;
	float3		erosionParams;
	float		groundNoiseDepth;
	float3		groundNoiseOffsetX_Z;
	float		groundNoiseScale;
	float		groundSecondaryNoiseDepth;
	float3		groundSecondaryNoiseOffsetX_Z;
	float		groundSecondaryNoiseScale;
	float		sideAngle;
	float		sideNoiseDepth;
	float3		sideNoiseOffsetXYZ;
	float3		sideNoiseScaleXZ_Y;
	float		sideSecondaryNoiseDepth;
	float3		sideSecondaryNoiseOffsetXYZ;
	float3		sideSecondaryNoiseScaleXZ_Y;
	float		spiralCurl;
	float3		spiralWidthBegin_End;
	float3		spiralHeightBegin_End;
	float3		spiralAngleBegin_End;
	float3		spiralTwistBegin_End;
	bool		spiralFlatten;
	BlobShape	shape;
	BlobNoise	groundNoiseType;
	BlobNoise	groundSecondaryNoiseType;
	BlobNoise	sideNoiseType;
	BlobNoise	sideSecondaryNoiseType;
	float		materialAngle;
	float		materialAngleGradient;
	Material	materialTop;
	float3		materialTopColor;
	Material	materialBottom;
	float3		materialBottomColor;
	float4		wedgeBulge;
	float		previewDetail;
	class Rail*	rail			   = 0;
	bool		railUseMarkerShape = false;
	float		railMarkerScaling  = 1.0f;
	float		railBluntness	   = 0.0f;
	float		pyroDepth		   = 1.0f;
	float		hardness		   = 1.0f;
};

class TerrainBlobPrefab {
	dynamic_array<TerrainBlob*> m_blobs;
public:
	string			prefabFilename;
	float4x4		transform;
	int				order;
	float			stickiness;
	float			previewDetail;
	float			previewGridResolution;
	uint32			bstGuid;
};

struct TerrainBlobGroup {
	// Attributes loaded from Objects.level
	uint32			bstGuid;
	class Clump*	blobs;
	class Clump*	blobPrefabs;
	uint32			maxVertCount;
	float			gridResolution;
	float			subsurfaceDensity;
	float			cutoffLevel;

	bool			seaLevelCutoff;
	bool			generateOctree;
	bool			enableTessellation;
	bool			fineMode;

	bool			isCloud;
	bool			isBackground;
	bool			isEnabled;
	bool			canPassThrough;

	float			binSize;
	uint32			blobPrefabHash;

	bool			dirty;
	bool			shaderDirty;

	// Bookkeeping used by TerrainBlobBarn
	dynamic_array<TerrainBlob*>			m_blobs;
	dynamic_array<TerrainBlobPrefab*>	m_prefabs;

	TerrainBlobGroup()
		: blobs(0)
		, blobPrefabs(0)
		, gridResolution(1.0f)
		, subsurfaceDensity(200.0f)
		, cutoffLevel(0.0f)
		, seaLevelCutoff(true)
		, generateOctree(true)
		, enableTessellation(false)
		, isCloud(false)
		, isBackground(false)
		, isEnabled(true)
		, canPassThrough(false)
		, dirty(true)
		, shaderDirty(false)
	{}
	void		Initialize(class Heap* heap);
	void		Terminate(class Heap* heap);
	void		HotDate() { dirty = true; };
};

//--------------------------------------------------------------------------------
// Terrain Mesh - After a TerrainBlob is compiled, a TerrainMesh is generated.
// It contains all data needed to render a chunk of terrain
//--------------------------------------------------------------------------------

struct TerrainTessData {
	dynamic_array<Edge16>		uniqueEdges;
	dynamic_array<EdgeIndex16>	triangleEdges;	 // Stores the index of each edge of all the triangles in the mesh.
	dynamic_array<uint32>		binEdgeCounts;

	bool read(byte_reader &levelFile) {
		uint32	edgeCount			= levelFile.get<uint32>();
		uint32	triangleEdgeCount	= levelFile.get<uint32>();
		return uniqueEdges.read(levelFile, edgeCount)
			&& triangleEdges.read(levelFile, triangleEdgeCount);
	}
};

struct Octree {
	struct Bin50 {
		uint32	indicesCount;
		uint32	edgesCount;
		uint8	position[4];
	};
	struct Bin {
		uint32	indicesCount;
		uint8	position[4];
	};

	float3				aabbMin;
	float3				aabbMax;

	float				binSize;
	uint32				gridWidth;
	uint32				gridHeight;
	uint32				gridDepth;

	// Tree data (Bins are leaf nodes)
	dynamic_array<Bin>	bins;

	bool read_header(byte_reader &levelFile) {
		return iso::read(levelFile, aabbMin, aabbMax, binSize, gridWidth, gridHeight, gridDepth);
	}
};

struct Octree34 {
	struct Index {
		uint32 index : 16; // If isBin is true, indexes into octreeBins, otherwise octreeNodes
		bool isBin : 8;
	};

	struct Node {
		uint16	childNodesOffset;	// Offsets into the octreeNodes array. Children are sequential.
		uint16	childNodesValid;	// If a bit is set, that child exists in the child nodes array.
		int32	indicesOffset;		// Into the "indices" and "triangleEdges" arrays
		uint32	indicesCount;
		uint32	edgesOffset;
		uint32	lodData;
	};

	struct Bin {
		uint32	indicesCount; //@TODO: We can use simplification to get these down to 8-bit
		uint32	edgesCount;
		uint8	position[4];
	};

	struct Bin18 {
		uint32	indicesCount;
		uint32	edgesCount;
		uint32	lodData;
		operator Bin() const {
			Bin	b;
			b.indicesCount = indicesCount;
			b.edgesCount = edgesCount;
			clear(b.position);
			return b;
		}
	};

	float3	center;
	float3	extent;
	uint32	depth;
	// Tree data (Bins are leaf nodes)
	dynamic_array<Node>	nodes;
	dynamic_array<Bin>	bins;

	bool read(byte_reader &levelFile, int version) {
		uint32	nodeCount, binCount;
		iso::read(levelFile, center, extent, depth, nodeCount, binCount);
		nodes.read(levelFile, nodeCount);

		if (version >= 19) {
			bins.read(levelFile, nodeCount);
		} else {
			dynamic_array<Bin18>	temp;
			temp.read(levelFile, nodeCount);
			bins = temp;
		}
		return true;
	}

};


template<typename P, typename N, typename M, typename L> struct TerrainVertT {
	P	pos;
	N	normal;
	M	mtrls;
	L	light;

	TerrainVertT()	{}
	template<typename P2, typename N2, typename M2, typename L2> TerrainVertT(const TerrainVertT<P2,N2,M2,L2> &b) : pos(b.pos), normal(b.normal), mtrls(b.mtrls), light(b.light) {}
	operator MeshVert() const { return {pos, light, normal}; }
};

template<int V> struct TerrainVertV;
template<> struct TerrainVertV<53> : TerrainVertT<float3p, MeshNorm, MeshWeight, MeshLight>		{ using TerrainVertT::TerrainVertT; };
template<> struct TerrainVertV<54> : TerrainVertT<float3p, MeshNorm, MeshWeight, MeshLight54>	{ using TerrainVertT::TerrainVertT; };
template<> struct TerrainVertV<47> : TerrainVertT<float3p, MeshNorm, MeshWeight, MeshLight47>	{ using TerrainVertT::TerrainVertT; };
template<> struct TerrainVertV<26> : TerrainVertT<float3p, MeshNorm, MeshColMat, MeshLight47>	{ using TerrainVertT::TerrainVertT; };

using TerrainVert = TerrainVertV<53>;

struct TerrainMesh {
	enum FLAGS {
		isCloud			= 1,
		canPassThrough	= 2,
	};

	uint32				blobGroupGuid;
	float3				localMin;	 // Level-space AABB min
	float3				localMax;	 // Level-space AABB max

	dynamic_array<TerrainVert>	verts;
	dynamic_array<uint16>		indices;
	dynamic_array<Material>		materialList;

	Octree				octree;
	TerrainTessData		tessData;
	uint8				flags;;

	void Load(byte_reader &levelFile, uint32 version) {
		levelFile.read(blobGroupGuid);
		levelFile.read(flags);
		levelFile.read(localMin);
		levelFile.read(localMax);

		if (version > 35)
			flags = (flags & 1) | ((flags & ~3) >> 1);

		uint32	vertCount	= levelFile.get<uint32>();
		uint32	indexCount	= levelFile.get<uint32>();

		if (version >= 55) {
			verts.read(levelFile, vertCount);
		} else if (version >= 48) {
			verts = dynamic_array<TerrainVertV<54>>(levelFile, vertCount);
		} else if (version >= 27) {
			verts = dynamic_array<TerrainVertV<47>>(levelFile, vertCount);
		} else {
			verts = dynamic_array<TerrainVertV<26>>(levelFile, vertCount);
		}
		materialList.read(levelFile, levelFile.get<uint32>());

		if (version >= 51) {
			octree.read_header(levelFile);
			octree.bins.read(levelFile, levelFile.get<uint32>());
			uint32	binCount = levelFile.get<uint32>();
			tessData.read(levelFile);
			tessData.binEdgeCounts.read(levelFile, binCount);

		} else if (version >= 35) {
			dynamic_array<Octree::Bin50>	bins;
			octree.read_header(levelFile);

			if (version < 37) {
				levelFile.skip(sizeof(float3p) * 2 + sizeof(uint32));
				uint32 nodeCount	= levelFile.get<uint32>();
				uint32 binCount		= levelFile.get<uint32>();
				levelFile.skip(20 * nodeCount);
				bins.read(levelFile, binCount);
			} else {
				bins.read(levelFile, levelFile.get<uint32>());
			}
			octree.bins				= transformc(bins, [](const Octree::Bin50 &a) { Octree::Bin b; b.indicesCount = a.indicesCount; raw_copy(a.position, b.position); return b; });
			tessData.binEdgeCounts	= transformc(bins, [](const Octree::Bin50 &a) { return a.edgesCount; });
			tessData.read(levelFile);

		} else {
			Octree34	temp;
			temp.read(levelFile, version);
			tessData.read(levelFile);
		}

		// Reconstruct the index buffer from our edge list
		if (tessData.uniqueEdges) {
			ISO_ASSERT(tessData.triangleEdges.size() == indexCount);

			indices.resize(indexCount);
			// The uniqueEdges buffer is independent for each bin
			uint32 edgeOffset = 0, j = 0;
			for (auto &bin : octree.bins) {
				for (uint32 i = 0; i < bin.indicesCount; i++) {
					EdgeIndex16& index	= tessData.triangleEdges[j];
					Edge16&		 edge	= tessData.uniqueEdges[edgeOffset + index.index];
					indices[j++] = index.flip ? edge.b : edge.a;
				}
				edgeOffset += tessData.binEdgeCounts[octree.bins.index_of(bin)];
			}

			ISO_ASSERT(j = indexCount);
		} else {
			indices.read(levelFile, indexCount);
		}

		if (version <= 41) {
			if (version < 36) {
				if (version >= 34) {
					levelFile.skip(sizeof(uint32) * 2);
					uint32 binCount = levelFile.get<uint32>();
					for( uint32 i = 0; i < binCount; i++ )
						levelFile.skip( levelFile.get<uint16>() + 8 * sizeof(MeshLight47));
				} else if (version >= 33) {
					levelFile.skip(sizeof(uint32));
					uint32 binCount = levelFile.get<uint32>();
					for( uint32 i = 0; i < binCount; i++ )
						levelFile.skip(levelFile.get<uint16>());
				} else if (version >= 31) {
					levelFile.skip(sizeof(uint32));
					uint32 binCount = levelFile.get<uint32>();
					levelFile.skip(binCount * 512 * 2);
				}
			} else {
				levelFile.skip(levelFile.get<uint32>() * sizeof(uint32));
			}
		}
	}

	SubMesh mesh() const {
		SubMesh	m;
		m.minext	= localMin;
		m.maxext	= localMax;
		m.technique	= ISO::root("data")["default"]["col_vc"];
		m.indices	= make_split_range<3>(indices);
		m.verts		= ISO::MakePtr(0, ISO::OpenArray<MeshVert>(verts));
		return m;
	}
};

//--------------------------------------------------------------------------------
// CloudData
//--------------------------------------------------------------------------------

struct CloudData {
	struct Vertex {
		float	pos[3];
		norm8	norm[4];
	};

	int32					binMins[3];
	uint32					binDims[3];
	const uint8*			binDists;

	dynamic_array<int16>	activeBins;
	malloc_block			voxDistCompressed;
	malloc_block			voxLightCompressed;
	malloc_block			voxHardnessCompressed;

	float					subsurfaceDensity;
	uint32					binResolution;
	uint32					binResolutionAmb;
	uint8					registerCollision;

	dynamic_array<Vertex>	verts;
	dynamic_array<uint16>	indices;

	CloudData(byte_reader &levelFile, uint32 version) {
		levelFile.read(binMins);
		levelFile.read(binDims);

		uint32 binCount = binDims[0] * binDims[1] * binDims[2];
		binDists	= levelFile.get_ptr<uint8>(binCount);

		activeBins.read(levelFile, levelFile.get<uint32>() * 3);

		//for( uint32 i = 0; i < cloud.binCount; i++ )
		//{
		//	levelFile.Serialize( cloud.bins[ i ].packedSize );
		//	ALLOC( cloud.bins[ i ].packedVoxels, cloud.bins[ i ].packedSize );
		//	levelFile.Serialize( cloud.bins[ i ].packedVoxels, cloud.bins[ i ].packedSize );
		//	levelFile.Serialize( cloud.bins[ i ].voxelsAmb );
		//	if( m_fileVersion >= 40 ) { levelFile.Serialize( cloud.bins[ i ].voxelsPoint ); }
		//	else { memset( cloud.bins[ i ].voxelsPoint, 0, sizeof( cloud.bins[ i ].voxelsPoint ) ); }
		//	if( m_fileVersion >= 41 ) { levelFile.Serialize( cloud.bins[ i ].position ); }
		//}

		//if( m_fileVersion < 41 )
		//{
		//	for( uint32 t = 0; t < m_terrainMeshCount; t++ )
		//		for( uint32 b = 0; b < m_terrainMeshes[ t ].cloudBinCount; b++ )
		//		{
		//			uint8_t* localPos = m_terrainMeshes[ t ].octree.bins[ b ].position;
		//			vmVector3 pos = m_terrainMeshes[ t ].octree.aabbMin / kCloudBinSize + vmVector3( localPos[0], localPos[1], localPos[2] );
		//			cloud.bins[ m_terrainMeshes[ t ].cloudBinMap[ b ] ].position[ 0 ] = round( pos[ 0 ] );
		//			cloud.bins[ m_terrainMeshes[ t ].cloudBinMap[ b ] ].position[ 1 ] = round( pos[ 1 ] );
		//			cloud.bins[ m_terrainMeshes[ t ].cloudBinMap[ b ] ].position[ 2 ] = round( pos[ 2 ] );
		//		}
		//}



		uint32	voxDistCompressedSize		= levelFile.get<uint32>();
		uint32	voxLightCompressedSize		= levelFile.get<uint32>();
		uint32	voxHardnessCompressedSize	= version >= 45 ? levelFile.get<uint32>() : 0;

		voxDistCompressed.read(levelFile, voxDistCompressedSize);
		voxLightCompressed.read(levelFile, voxLightCompressedSize);
		voxHardnessCompressed.read(levelFile, voxHardnessCompressedSize);

		levelFile.read(subsurfaceDensity);

		if (version >= 49) {
			levelFile.read(binResolution);
			levelFile.read(binResolutionAmb);
		}

		if (version >= 50) {
			uint32	vertCount		= levelFile.get<uint32>();
			uint32	indexCount		= levelFile.get<uint32>();
			verts.read(levelFile, vertCount);
			indices.read(levelFile, indexCount);
		}

		registerCollision = 1;
		if (version >= 54)
			levelFile.read(registerCollision);
	}

	SubMesh mesh() const {
		SubMesh	m;
		m.technique	= ISO::root("data")["default"]["specular"];
		m.indices	= make_split_range<3>(indices);
		m.verts		= ISO::MakePtr(0, ISO::OpenArray<Vertex>(verts));
		m.UpdateExtents();
		return m;
	}
};

//--------------------------------------------------------------------------------
// Skirts
//--------------------------------------------------------------------------------

struct SkirtData {
	template<int V> struct VertexV : TerrainVertV<V> {
		int8		nd[4];
		VertexV() {}
		template<int V2> VertexV(const VertexV<V2> &b) : TerrainVertV<V>(b) { raw_copy(b.nd, nd); }

	};
	typedef VertexV<53>	Vertex;
	dynamic_array<Vertex>	verts;
	dynamic_array<uint16>	indices;

	SubMesh mesh() const {
		SubMesh	m;
		m.technique	= ISO::root("data")["default"]["specular"];
		m.indices	= make_split_range<3>(indices);
		m.verts		= ISO::MakePtr(0, ISO::OpenArray<Vertex>(verts));
		m.UpdateExtents();
		return m;
	}
};

//--------------------------------------------------------------------------------
// Occluders
//--------------------------------------------------------------------------------

struct OccluderMesh {
	struct Vertex {
		float	pos[3];
		norm8	norm[4];
	};
	dynamic_array<Vertex>	verts;
	dynamic_array<uint16>	indices;

	SubMesh mesh() const {
		SubMesh	m;
		m.technique	= ISO::root("data")["default"]["specular"];
		m.indices	= make_split_range<3>(indices);
		m.verts		= ISO::MakePtr(0, ISO::OpenArray<Vertex>(verts));
		m.UpdateExtents();
		return m;
	}
};

//--------------------------------------------------------------------------------
// LevelMesh Baked Data - Lighting data for a LevelMesh. The mesh data (verts/indices) is stored in a separate .mesh file, this is only the Lights and Culling info
//--------------------------------------------------------------------------------
struct LevelMeshBakedData {
	struct Lod {
		ISO::OpenArray<MeshLight>	lights;
		ISO::OpenArray<bool>		trisCulled;
	};

	string				meshName;
	uint32				levelMeshGuid;
	bool				simple;
	ISO::OpenArray<Lod>	lods;

	void Load(byte_reader &levelFile, uint32 version) {
		if (version >= 47)
			meshName = read_string(levelFile);

		levelFile.read(levelMeshGuid);

		uint32	lodCount = levelFile.get();
		lods.Resize(lodCount);

		simple = version >=46 && (levelFile.getc() & 1);

		if (version >= 44) {
			for (auto& lod : lods) {
				// Lights are only recorded for verts with unique position/normal combinations
				uint32	uniqueVertCount	= levelFile.get<uint32>();
				uint32	triCount		= levelFile.get<uint32>();

				if (version >= 55) {
					lod.lights.read(levelFile, simple ? 1 : uniqueVertCount);
				} else {
					lod.lights = dynamic_array<MeshLight54>(levelFile, simple ? 1 : uniqueVertCount);
				}
				lod.trisCulled.read(levelFile, triCount);
			}
		} else {
			dynamic_array<uint32>		light_counts;
			light_counts.read(levelFile, lodCount);
			for (auto &&i : make_pair(lods, light_counts)) {
				i.a.lights = new_auto_init(MeshLight47, i.b, levelFile);
			}
		}
	}
};

//--------------------------------------------------------------------------------
// Level Parameters Object - Attributes for baking and rendering a Level.
// Read from a resource file.
//--------------------------------------------------------------------------------
struct LevelParameters {
	// Geometry compilation attributes
	uint32			gridSize;
	float			previewGridResolution;

	// Terrain attributes
	float			defaultCloudDensity;
	float			defaultLandDensity;
	bool			halfResClouds;
	bool			cloudsAsOccluders;

	// Light baking attributes
	uint32			tapCount;
	uint32			bounceCount;
	float			blurPasses;
	class Clump*	lightProbeAABB;

	// World attributes
	string			worldName;
	bool			isWorld;
};

enum BakeType {
	BAKE_FANCY		= 0,
	BAKE_REGULAR	= 1,
	BAKE_QUICK		= 2,
	BAKE_NO_LIGHT	= 3,
	BAKE_ROUNDED	= 4,
	BAKE_GOLD		= 5,
};

struct VolumetricLighting {
	union VolCell {
		uint32 v;
		struct {
			uint32	rle : 7;		// How many of this set of values in the tower, up to 127 cells
			uint32	sh0 : 7;		// M 0, L  1
			uint32	sh1 : 6;		// M 1, L -1
			uint32	sh2 : 6;		// M 1, L  0
			uint32	sh3 : 6;		// M 1, L  1
		};
		bool operator==(const VolCell& b) const {
			return abs((int)sh0 - (int)b.sh0) <= 5
				&& abs((int)sh1 - (int)b.sh1) <= 3
				&& abs((int)sh2 - (int)b.sh2) <= 3
				&& abs((int)sh3 - (int)b.sh3) <= 3;
		}
	};

	float		x, y, z, size;		// Where does the AABB start? Size of each cell?
	uint32		xs, ys, zs;			// How many cells in each dimension
	uint32		cellCount;			// Since the volume is RLE, this is how many cells there are on disk
	uint32*		offsets;			// Offset to where each tower (X,Y) exists in the grid
	VolCell*	grid;				// SH for each cell in the grid, packed into uint8s

	VolumetricLighting(const memory_block &data) {
		byte_reader	file(data);
		iso::read(file, x, y, z, size, xs, ys, zs, cellCount);
		offsets		= new uint32[xs * zs];
		grid		= new VolCell[cellCount];
		for (size_t c = 0; c < xs * zs; ++c)
			file.read(offsets[c]);
		for (size_t c = 0; c < cellCount; ++c)
			file.read(grid[c].v);
	}

	float4 GetSHLightingAt(int x, int y, int z) const {
		uint32 offset = offsets[z * xs + x];
		uint32 height = 0;
		for (;;) {
			const auto& c = grid[offset++];
			height += c.rle;
			if (height > y) {
				// Found the RLE section that contains the height requested... unpack it
				const float4 minSH		= float4{0.0f, -pi / two, -pi / two, -pi / two};
				const float4 maxSH		= float4{3.55f, pi / two,  pi / two,  pi / two};
				const float4 scaleSH	= float4{127.9f, 63.9f, 63.9f, 63.9f} / (maxSH - minSH);
				return float4{c.sh0, c.sh1, c.sh2, c.sh3} / scaleSH + minSH;
			}
		}
		return float4(1.0f);
	}

	float4 GetSHLightingAt(const float3& pos) const {
		float3	offset	= (pos - float3{x, y, z}) / size;
		float3	index	= floor(offset);
		float3	coord	= offset - (index + float3(0.5f));
		float3	w0		= abs(coord);
		int		dX		= coord[0] < 0 ? -1 : 1;
		int		dY		= coord[1] < 0 ? -1 : 1;
		int		dZ		= coord[2] < 0 ? -1 : 1;
		int		x0		= clamp((int)index[0], 0, (int)xs - 1);
		int		x1		= clamp((int)index[0] + dX, 0, (int)xs - 1);
		int		y0		= clamp((int)index[1], 0, (int)ys - 1);
		int		y1		= clamp((int)index[1] + dY, 0, (int)ys - 1);
		int		z0		= clamp((int)index[2], 0, (int)zs - 1);
		int		z1		= clamp((int)index[2] + dZ, 0, (int)zs - 1);

		float4	x0y0z0	= GetSHLightingAt(x0, y0, z0);
		float4	x1y0z0	= GetSHLightingAt(x1, y0, z0);
		float4	x0y1z0	= GetSHLightingAt(x0, y1, z0);
		float4	x1y1z0	= GetSHLightingAt(x1, y1, z0);
		float4	x0y0z1	= GetSHLightingAt(x0, y0, z1);
		float4	x1y0z1	= GetSHLightingAt(x1, y0, z1);
		float4	x0y1z1	= GetSHLightingAt(x0, y1, z1);
		float4	x1y1z1	= GetSHLightingAt(x1, y1, z1);

		float4	y0z0	= lerp(x0y0z0, x1y0z0,	w0[0]);
		float4	y1z0	= lerp(x0y1z0, x1y1z0,	w0[0]);
		float4	y0z1	= lerp(x0y0z1, x1y0z1,	w0[0]);
		float4	y1z1	= lerp(x0y1z1, x1y1z1,	w0[0]);
		float4	zz0		= lerp(y0z0, y1z0, 		w0[1]);
		float4	zz1		= lerp(y0z1, y1z1, 		w0[1]);

		return lerp(zz0, zz1, w0[2]);
	}

};

struct LevelLod {
	dynamic_array<LevelMeshBakedData>	m_levelMeshBakes;
	dynamic_array<TerrainMesh>			m_terrainMeshes;
	dynamic_array<SkirtData>			m_skirtDatas;
	dynamic_array<OccluderMesh>			m_occluder;
	unique_ptr<CloudData>					m_cloudData;
	LevelLod(const memory_block &data, uint32 version);
	LevelLod() {}
};

LevelLod::LevelLod(const memory_block &data, uint32 version) {
	byte_reader	levelFile(data);

	m_levelMeshBakes.resize(levelFile.get<uint32>());
	for (auto &bake : m_levelMeshBakes)
		bake.Load(levelFile, version);

	m_terrainMeshes.resize(levelFile.get<uint32>());
	for (auto &mesh : m_terrainMeshes)
 		mesh.Load(levelFile, version);


	// Loading clouds
	if (version >= 42) {
		if (uint32 cloudDataCount = levelFile.get<uint32>())
			m_cloudData = new CloudData(levelFile, version);

	} else {
		// ignore data from old versions
		if (uint32 cloudMeshCount = levelFile.get<uint32>()) {
			uint32 binCount = levelFile.get<uint32>();
			for (uint32 i = 0; i < binCount; i++) {
				uint16 packedSize = levelFile.get<uint16>()		// dist / light
					+ (version >= 38 ? sizeof(uint16) * 4 * 2 * 2 * 2 : version >= 36 ? sizeof(MeshLight47) * 8 : 0)			// amb
					+ (version == 40 || version == 41 ? sizeof(uint16) * 2 * 2 * 2 : 0)	// point
					+ (version == 41 ? sizeof(int16) * 3 : 0);	// position
				levelFile.skip(packedSize);
			}
		}
	}

	// Loading skirts
	if (version >= 54) {
		m_skirtDatas.resize(levelFile.get<uint32>());
		for (auto &skirt : m_skirtDatas) {
			skirt.verts = dynamic_array<SkirtData::VertexV<54>>(levelFile, levelFile.get<uint32>());
			skirt.indices.read(levelFile, levelFile.get<uint32>());
		}
	} else if (version >= 43) {
		m_skirtDatas.resize(levelFile.get<uint32>());
		for (auto &skirt : m_skirtDatas) {
			skirt.verts.read(levelFile, levelFile.get<uint32>());
			skirt.indices.read(levelFile, levelFile.get<uint32>());
		}
	} else {
		struct OldSkirtVertex : TerrainVert {
			float4		n;
			float		d;
		};

		uint32 oldSkirtDatasCount = levelFile.get<uint32>();
		for (uint32 i = 0; i < oldSkirtDatasCount; ++i) {
			if (version <= 28)
				levelFile.skip(levelFile.get<uint32>());
			levelFile.skip(levelFile.get<uint32>() * sizeof(OldSkirtVertex));
			levelFile.skip(levelFile.get<uint32>() * sizeof(uint32));
		}
	}

	// Loading occluders
	m_occluder.resize(levelFile.get<uint32>());
	if (m_occluder) {
		uint32	vertCount		= levelFile.get<uint32>();
		uint32	indexCount		= levelFile.get<uint32>();
		m_occluder[0].verts.read(levelFile, vertCount);
		m_occluder[0].indices.read(levelFile, indexCount);
	}
}


class LevelData {
public:
	static const uint32 kLevelMaxSize		= 16 * 1024 * 1024;
	static const uint32 kMinLevelVersion	= 41;
	static const uint32 kLevelVersion		= 54;
	static const uint32 kLevelLodCount		= 5;

	uint32		m_fileVersion;
	uint32		m_fileLodCount;
	uint32		m_fileLodOffsets[kLevelLodCount];
	uint32		m_fileLodLengths[kLevelLodCount];

	BakeType			bakeType;
	float3				m_aabbMin;
	float3				m_aabbMax;
	VolumetricLighting*	m_volumetric;

	dynamic_array<LevelLod>	lods;

	LevelData(): bakeType(BAKE_FANCY), m_volumetric(0) {}

	void	Load(istream_ref file);

	float4	GetSHLightingAt(const float3& pos) const {
		return m_volumetric ? m_volumetric->GetSHLightingAt(pos) : float4(1.0f);
	}
};

void LevelData::Load(istream_ref file) {
	TGCBinaryFile::Header h;
	file.read(h);

	m_fileVersion = h.version;

	TGCBinaryFile::TableOfContents toc;
	file.read(toc);

	// Load table of contents
	m_fileLodCount = 0;
	for (uint32 i = 0; i < toc.entryCount; ++i) {
		file.seek(toc.entries[i].offset);
		malloc_block buffer(kLevelMaxSize*2);
		malloc_block compressedBuffer(file, toc.entries[i].length);

//		LZ4::decoder	decoder;
//		decoder.more(buffer, compressedBuffer, kLevelMaxSize);

		size_t	written;
		transcode(LZ4::decoder(), buffer, compressedBuffer, &written);
		buffer.resize(written);

		if (strstr(toc.entries[i].name, "LOD")) {
			// Add LOD
			lods.emplace_back(buffer, m_fileVersion);

		} else if (strstr(toc.entries[i].name, "VOL")) {
			// Load volumetric data from the level
			m_volumetric = new VolumetricLighting(buffer);
		}
	}

	// Bake type is chilling with level bounds
	if (m_fileVersion >= 53)
		bakeType = (BakeType)file.get<uint32>();

	// Load bounds, however I don't think it should just be chilling in the file without TOC entry...
	file.read(m_aabbMin);
	file.read(m_aabbMax);
}

//--------------------------------------------------------------------------------
// ISO
//--------------------------------------------------------------------------------

template<typename T> ISO_DEFUSERCOMPVT(Edge, T, a, b);
template<typename T> struct ISO::def<EdgeIndex<T>> : ISO::def<T> {};

ISO_DEFUSER(Material, uint8);
ISO_DEFUSERCOMPV(SQT, scale, quat, trans);
ISO_DEFUSERCOMPV(AnimHalfVec, x, y, z);

ISO_DEFUSERCOMPV(AnimPackData::AnimPackKeys, defaultKeys, mirroredDefaultKeys, quatKeysFloat, quatKeysUint, scaleKeys, transKeysFloat, transKeysHalf, boneInfo);

ISO_DEFUSERCOMPV(AnimPackData::AnimationData,
	startFrame, endFrame,
	flags,
	boundsMin, boundsMax,
	quatKeysStart, scaleKeysStart, transKeysStart, boneInfoStart,
	firstKeyScaleBoneCount, firstKeyQuatBoneCount, firstKeyTransBoneCount,
	keyedScaleBoneCount, keyedQuatBoneCount, keyedTransBoneCount
);
ISO_DEFUSERCOMPV(AnimPackData,
	name, frameCount, compression, frameRate,
	boneCount,restKeys, boneParents, boneLinkMats,
	keys, animations
);

ISO_DEFUSER(Color32, unorm8[4]);
ISO_DEFUSER(ColorD, unorm8[4]);
ISO_DEFUSERCOMPV(MeshLight, light0, light1, light2);
ISO_DEFUSERCOMPV(MeshPos, pos, col);
ISO_DEFUSERCOMPV(MeshUv, uv0, uv1);
ISO_DEFUSERCOMPV(MeshUvFixed, uv0, uv1);
ISO_DEFUSERCOMPV(MeshOffset, pos);
ISO_DEFUSERCOMPV(MeshWeight, b, w);
ISO_DEFUSERCOMPV(MeshVert, pos, col, normal);

ISO_DEFUSERCOMPV(MeshOccluder, vertsPos, indices);
ISO_DEFUSERCOMPV(MeshLod,
	lodRadius, min, max, minMesh, maxMesh,
	vertsPos, vertsNorm, vertsUv, weights,
	indices, adjacency,
	uniquePosIndexMap, uniquePosNormIndexMap, uniqueStripIndices, vertOcclusions, uniqueEdgeIndices,
	mesh
);
ISO_DEFUSERCOMPV(MeshData, name, lods, occluder, animData);

ISO_DEFUSERCOMPV(LevelMeshBakedData::Lod, lights, trisCulled);
ISO_DEFUSERCOMPV(LevelMeshBakedData, meshName, levelMeshGuid, simple, lods);

ISO_DEFUSERCOMPV(TerrainVert, pos, normal, mtrls, light);
ISO_DEFUSERCOMPV(TerrainTessData, uniqueEdges, triangleEdges, binEdgeCounts);

ISO_DEFUSERCOMPV(Octree::Bin, indicesCount, position);
ISO_DEFUSERCOMPV(Octree, aabbMin, aabbMax, binSize, gridWidth, gridHeight, gridDepth, bins);

ISO_DEFUSERCOMPV(TerrainMesh, localMin, localMax, verts, indices, materialList, octree, tessData, flags, mesh);

ISO_DEFUSERCOMPV(CloudData::Vertex, pos, norm);
ISO_DEFUSERCOMPV(CloudData, binMins, binDims, binDists, activeBins, voxDistCompressed, voxLightCompressed, voxHardnessCompressed, subsurfaceDensity, binResolution, binResolutionAmb, registerCollision, verts,indices, mesh);

ISO_DEFUSERCOMPV(SkirtData::Vertex, pos, normal, mtrls, light, nd);
ISO_DEFUSERCOMPV(SkirtData, verts, indices, mesh);

ISO_DEFUSERCOMPV(OccluderMesh::Vertex, pos, norm);
ISO_DEFUSERCOMPV(OccluderMesh, verts, indices, mesh);

ISO_DEFUSERCOMPV(LevelLod, m_levelMeshBakes, m_terrainMeshes, m_skirtDatas, m_occluder, m_cloudData);
ISO_DEFUSERCOMPV(LevelData, m_fileVersion, m_aabbMin, m_aabbMax, lods);

ISO_DEFUSERCOMPV(TGCBinaryFile::Header, fourCC, version);
ISO_DEFUSERCOMPV(TGCBinaryFile, header, sections);

ISO_ptr<SubMesh> BakeTGC(ISO_ptr<MeshData> mesh, const LevelMeshBakedData &baked, int lod) {

	ISO_ptr<MeshData> mesh2 = FileHandler::ExpandExternals(mesh);

	auto&	a = mesh2->lods[lod];
	auto&	b = baked.lods[lod];

	ISO_ptr<SubMesh>	m(baked.meshName);

	m->minext		= a.minMesh;
	m->maxext		= a.maxMesh;
	//m->technique	= ISO::root("data")["default"]["specular"];
	m->technique	= ISO::root("data")["default"]["col_vc"];
	m->indices		= make_split_range<3>(a.indices);

	auto verts		= ISO::OpenArray<MeshVert>(make_pair(a.vertsPos, a.vertsNorm));
	if (b.lights) {
		for (int j = 0; j < verts.size32(); j++)
			verts[j].col = b.lights[baked.simple ? 0 : a.uniquePosNormIndexMap[j]].light0.operator float3();
	}

	m->verts		= ISO::MakePtr(0, move(verts));

	return m;
}

anything BakeTGC2(ISO_ptr<void> meshes, ISO_ptr<LevelData> baked) {
	anything	a;
	ISO::Browser2		meshes2	= FileHandler::ExpandExternals(meshes);
	ISO_ptr<LevelData>	baked2	= FileHandler::ExpandExternals(baked);

	for (auto &lod : baked2->lods) {
		for (auto &bake : lod.m_levelMeshBakes)
			a.Append(BakeTGC(meshes2[bake.meshName], bake, 0));
	}
	return a;
}

anything BakeTGC3(ISO_ptr<anything> level, ISO_ptr<void> meshes, ISO_ptr<LevelData> baked) {
	anything	a;
	ISO_ptr<anything>	level2	= FileHandler::ExpandExternals(level);
	ISO::Browser2		meshes2	= FileHandler::ExpandExternals(meshes);
	ISO_ptr<LevelData>	baked2	= FileHandler::ExpandExternals(baked);


	for (auto &t : baked2->lods[0].m_terrainMeshes) {
		ISO_ptr<Node>	node(0);
		node->matrix	= identity;
		node->children.Append(ISO::Conversion::convert<Model3>(ISO::MakePtr(0, t.mesh())));
		a.Append(node);
	}

	ISO_ptr<Node>	node(0);
	node->matrix	= identity;
	node->children.Append(ISO::Conversion::convert<Model3>(ISO::MakePtr(0, baked2->lods[0].m_cloudData->mesh())));
	a.Append(node);

	hash_map<uint32, LevelMeshBakedData*>	bake_map;
	for (auto &bake : baked2->lods[0].m_levelMeshBakes)
		bake_map[bake.levelMeshGuid] = &bake;

	for (auto &item : *level2) {
		if (item.IsType("Beamo")) {
			ISO::Browser2	b(item);
			uint32		guid	= b["bstGuid"].Get();

			auto	bake = bake_map[guid];
			if (bake.exists()) {
				float4x4p	*mat	= b["transform"];
				string		name	= b["meshName"].Get();

				ISO_ptr<Node>	node(0);
				node->matrix.x	= mat->x.xyz;
				node->matrix.y	= mat->y.xyz;
				node->matrix.z	= mat->z.xyz;
				node->matrix.w	= mat->w.xyz;

				node->children.Append(ISO::Conversion::convert<Model3>(BakeTGC(meshes2[name], *bake, 0)));
				a.Append(node);
			}

		}

	}
	return a;
}

//------------------------------------------------------------------------------
// TGC Baked Meshes
//------------------------------------------------------------------------------

class TGCBinaryFileHandler : public FileHandler {
	const char*		GetDescription() override { return "TGC binary file"; }

	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		ISO_ptr<TGCBinaryFile>	p(id);
		p->Load(file);
		return p;
	}
} tgcbin;


class TGCMeshesFileHandler : public FileHandler {
	const char*		GetExt() override { return "meshes"; }
	const char*		GetDescription() override { return "TGC baked meshes"; }

	int				Check(istream_ref file) override {
		file.seek(0);
		TGCBinaryFile::Header	h;
		return file.read(h) && h.fourCC == "LVL0"_u32 ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}
	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		ISO_ptr<LevelData>	p(id);
		p->Load(file);
		return p;
	}
} tgcmeshes;

//------------------------------------------------------------------------------
// TGC Mesh
//------------------------------------------------------------------------------

class TGCMeshFileHandler : public FileHandler {
	const char*		GetExt() override { return "mesh"; }
	const char*		GetDescription() override { return "TGC mesh"; }

	int				Check(istream_ref file) override {
		file.seek(0);
		return between(file.get<uint32>(), MeshData::kMinVersion, MeshData::kMaxVersion) ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}
	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		uint32	version = file.get<uint32>();
		if (!between(version, MeshData::kMinVersion, MeshData::kMaxVersion))
			return ISO_NULL;

		ISO_ptr<MeshData>	mesh(id);
		mesh->Load(file, version);
		return mesh;
	}
public:
	TGCMeshFileHandler() {
		ISO_get_operation(BakeTGC);
		ISO_get_operation(BakeTGC2);
		ISO_get_operation(BakeTGC3);
	}

} tgc_mesh;