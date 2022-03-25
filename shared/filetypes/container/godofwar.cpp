#include "base/vector.h"
#include "base/algorithm.h"
#include "bitmap/bitmap.h"
#include "iso/iso_files.h"
#include "iso/iso_script.h"
#include "systems/mesh/model_iso.h"
#include "soft_float_iso.h"
#include "platforms/ps3/shared/edge.h"
#include "3d/model_utils.h"
#include "vm.h"

#undef DC_VERSION

using namespace iso;

ISO_ptr<bitmap> SetMips(ISO_ptr<bitmap> bm0, ISO_ptr<bitmap> bm1);

struct memory_block2 : memory_block {
	memory_block2() : memory_block(0,0) {}
	memory_block2(void *_start, size_t _size) : memory_block(_start, _size) {}
};

typedef xint32be	uint128be[4];

//-------------------------------------
// bitfields
//-------------------------------------

template<typename T> tag2 GetName() {
	const ISO_type	*t = ISO_getdef<T>();
	if (t->Type() == ISO_USER)
		return ((ISO_type_user*)t)->ID();

	char			name[256];
	MemoryOutput	m(name);
	ISO_script_writer(m).DumpType(t);
	m.putc(0);
	return name;
}

template<typename T> struct array_unspec : ISO_virtual_defaults {
	T				*t;
	ISO_type_array	type;
	ISO_browser		Deref()		const	{ return ISO_browser(&type, t); }
	array_unspec(T *_t, uint32 n)		: t(_t), type(ISO_getdef<T>(), n) {}
	array_unspec(const _range<T*> &r)	: t(r.begin()), type(ISO_getdef<T>(), r.size()) {}
};
template<typename T> struct ISO_def<array_unspec<T> > : TISO_virtual<array_unspec<T> >{};

#define BITFIELDS_SPACE(T,n)	_uint_type<sizeof(T)>::type _[(n+sizeof(T)*8-1)/(sizeof(T)*8)]
#define BITFIELDS2(T, A,a, B,b)									union {BITFIELDS_SPACE(T,a+b); bitfield<T,0,a> A; bitfield<T,a,b> B;}
#define BITFIELDS3(T, A,a, B,b, C,c)							union {BITFIELDS_SPACE(T,a+b+c); bitfield<T,0,a> A; bitfield<T,a,b> B; bitfield<T,a+b,c> C;}
#define BITFIELDS4(T, A,a, B,b, C,c, D,d)						union {BITFIELDS_SPACE(T,a+b+c+d); bitfield<T,0,a> A; bitfield<T,a,b> B; bitfield<T,a+b,c> C; bitfield<T,a+b+c,d> D;}
#define BITFIELDS5(T, A,a, B,b, C,c, D,d, E,e)					union {BITFIELDS_SPACE(T,a+b+c+d+e); bitfield<T,0,a> A; bitfield<T,a,b> B; bitfield<T,a+b,c> C; bitfield<T,a+b+c,d> D; bitfield<T,a+b+c+d,e> E;}
#define BITFIELDS6(T, A,a, B,b, C,c, D,d, E,e, F,f)				union {BITFIELDS_SPACE(T,a+b+c+d+e+f); bitfield<T,0,a> A; bitfield<T,a,b> B; bitfield<T,a+b,c> C; bitfield<T,a+b+c,d> D; bitfield<T,a+b+c+d,e> E; bitfield<T,a+b+c+d+e,f> F;}
#define BITFIELDS7(T, A,a, B,b, C,c, D,d, E,e, F,f, G,g)		union {BITFIELDS_SPACE(T,a+b+c+d+e+f+g); bitfield<T,0,a> A; bitfield<T,a,b> B; bitfield<T,a+b,c> C; bitfield<T,a+b+c,d> D; bitfield<T,a+b+c+d,e> E; bitfield<T,a+b+c+d+e,f> F; bitfield<T,a+b+c+d+e+f,g> G;}
#define BITFIELDS8(T, A,a, B,b, C,c, D,d, E,e, F,f, G,g, H,h)	union {BITFIELDS_SPACE(T,a+b+c+d+e+f+g+h); bitfield<T,0,a> A; bitfield<T,a,b> B; bitfield<T,a+b,c> C; bitfield<T,a+b+c,d> D; bitfield<T,a+b+c+d,e> E; bitfield<T,a+b+c+d+e,f> F; bitfield<T,a+b+c+d+e+f,g> G; bitfield<T,a+b+c+d+e+f+g,h> H;}

template<typename T, int S, int N> struct ISO_def<bitfield<T,S,N> > : CISO_type_user {
	struct V : TISO_virtual2<bitfield<T,S,N>, V> {
		static ISO_browser2	Deref(bitfield<T,S,N> &a)	{ return MakePtr(tag(), a.get()); }
	} v;
	ISO_def() : CISO_type_user(GetName<T>(), &v)	{}
};

//-----------------------------------------------------------------------------
//	containers
//-----------------------------------------------------------------------------

template<typename T> struct off_ptr16_be : offset_pointer<T, uint16be> {};
template<typename T> struct off_ptr32_be : offset_pointer<T, uint32be> {};
template<typename T> struct off_ptr64_be : offset_pointer<T, uint64be> {};

template<typename T> struct rel_ptr_be : soft_pointer<T, base_relative<int32be> > {
	rel_ptr_be()					{}
	rel_ptr_be(T *_t) : soft_pointer<T, base_relative<int32be> >(_t) {}
	rel_ptr_be&	operator=(T *_t)	{ soft_pointer<T, base_relative<int32be> >::operator=(_t); return *this; }
};

template<typename T> struct next_iterator {
	typedef forward_iterator_t iterator_category;
	typedef const T	element, *pointer, &reference;
	const T	*p;
	int		i;
	next_iterator(const T *_p) : p(_p), i(0)	{}
	next_iterator(int _i) : p(0), i(_i)			{}
	next_iterator&		operator++()					{ p = p->next(); ++i; return *this; }
	bool	operator==(const next_iterator &b) const	{ return i == b.i; }
	bool	operator!=(const next_iterator &b) const	{ return i != b.i; }
	operator const T*() const							{ return p; }
};

struct PS3PixelShader : array_unspec<uint128be> {
	PS3PixelShader(const _range<uint128be*> &r) : array_unspec<uint128be>(r) {}
};
struct PS3VertexShader : array_unspec<uint128be> {
	PS3VertexShader(const _range<uint128be*> &r) : array_unspec<uint128be>(r) {}
};

//-----------------------------------------------------------------------------
//	SMC stuff
//-----------------------------------------------------------------------------

namespace smc {
typedef char dynastring[56];

typedef float32be Vector3[3];
typedef float32be Vector[4];

struct AABB {
	Vector	min, max;
};

template<typename T> struct array : bigendian {
	uint32				size;
	rel_ptr_be<T>		buffer;
	rel_ptr_be<void>	heap;
	T&	operator[](int i) { return buffer[i]; }
};

template<typename T> struct arrayW : array<T> {
	uint32	max_size;
	uint32	grow_size;
};

struct node {
	rel_ptr_be<node>	next, prev;
};
template<typename T> struct list : node {};

class snode {
	rel_ptr_be<snode>	next;
};
class slist : snode {};

template<bool be> struct _chunk : endian_types<be> {
	enum {
		MEM_ACK				= 0x1000,
		MEM_AVAIL_FOR_USE	= 0x2000,
		MEM_IN_USE			= 0x4000,
		MEM_VRAM			= 0x8000, // flag indicates data needs to end up in VRAM
	};
	enum ID {
		DYNASTRING				= 0,
		CLIENTPARM				= 1,
		GROUPSTART				= 2,
		GROUPEND				= 3,
		ACTIVATE				= 4,
		PUSH_CONTEXT			= 5,
		POP_CONTEXT				= 6,
		DATA_BLOCK				= 7,
		DATA_BLOCK_W_LENGTH		= 8,
		DATA_BLOCK_ALIGN16		= 9,
		VFS_TWEAK_FILE			= 10,
		DC_VERSION				= 11,
		DC_DATA					= 12,
		DC_EXPORTTABLE			= 13,
		DC_IMPORTTABLE			= 14,
		DC_DBG_STRINGTABLE		= 15,
		DC_DBG_SYMBOLTABLE		= 16,
		RESOURCE_NAMES			= 17,
		ACTIVATE_WAD_CONTEXT	= 18,
		POP_WAD_CONTEXT			= 19,
		PUSH_HEAP				= 20,
		POP_HEAP				= 21,
		PREFETCH_NAMES			= 22,
		PREFETCH_GRAPH			= 23,
		PADDING					= 24,
		PUSH_DEBUG				= 25,
		POP_DEBUG				= 26,
		DEBUG_FRAGMENT_SHADERS	= 27,
		TEXEL_CHUNK				= 28,
		_INVALID				= 65535,
	}; 
	uint16		id;
	uint16		version;
	uint32		length;
	dynastring	name;
};
typedef _chunk<true> chunk;

struct Client : bigendian {
	enum ServerID {
		RootServerID			= 0,	
		GameObjectServerID		= 1,
		MasterPrimServerID		= 2,
		AnimationServerID		= 3,
		ScriptServerID			= 4,
		LightServerID			= 6,
		TextureServerID			= 7,
		MaterialServerID		= 8,
		CameraServerID			= 9,

		MasterRenderServerID	= 12,
		ModelServerID			= 13,
		CollisionServerID		= 14,
		ParticleServerID		= 15,
		WaypointServerID		= 16,
		EventServerID			= 17,
		BhvrServerID			= 18,
		SoundServerID			= 19,
		WadServerID				= 20,
		EffectsServerID			= 21,
		ParticlePrimServerID	= 22,
		PS3TriServerID			= 23,
		OcclusionServerID		= 24,
		NetObjectServerID		= 25,
		LineServerID			= 26,
	};
	enum TypeFlags {
		//kServerMask		=	0x0000FFFF,
		kServerMask			=	0x000000FF,		// some code assumes we don't have more than 256 servers - should be plentyseeing as we only have 22 now...
		kClientMask			=	0x0FFF0000,
		kFlagsMask			=	0xF0000000,
		kIsContextFlg		=	0x80000000,		// set if this is a context
		kIsClientFlg		=	0x40000000,		// set if this is a client, otherwise, it's a parm
		kIsWadContextFlg	=	0x20000000,		// set if this is a wad context (the root context for this server in this wad)
	};
	uint32		id;

	Client()	{}
	ServerID	server()		const { return ServerID(id & kServerMask); }
	iso::uint32	client()		const { return (id & ~kFlagsMask) >> 16; }
	bool		is_client()		const { return id & kIsClientFlg; }
	bool		is_parm()		const { return !is_client(); }
	bool		is_context()	const { return id & kIsContextFlg; }
	bool		is_wadcontext() const { return id & kIsWadContextFlg; }
};

struct ClientList : Client, node {
};

struct ClientParmList : list<Client*> {
	rel_ptr_be<void>			pool;
};

struct MultiClient : Client {
	rel_ptr_be<ClientParmList>	list; 
};

struct HeapInfo : bigendian {
	uint32	size;
	uint32	vram_size;
};

struct WAD_Loader {
	static ISO_ptr<void>	LoadClientRaw(const Client *client, size_t size);
	static ISO_ptr<void>	LoadClient(const Client *client, size_t size, bool &keep);
	template<typename T> static ISO_ptr<void> LoadRaw(tag id, const void *data, size_t size);

	template<Client::ServerID ID> static ISO_ptr<void> LoadClient(const Client *client, size_t size, bool &keep) {
		keep = false;
		return LoadClientRaw(client, size);
	}
};

struct WAD_Context : WAD_Loader {
	istream				*ts;
	ISO_ptr<anything>	stack[32], *sp;

	uint32			Process(chunk &chunk, void *data, bool &keep);
	ISO_ptr<void>	LoadClient(const Client *client, size_t size, bool &keep);
	template<Client::ServerID ID>	ISO_ptr<void> LoadClient(const Client *client, size_t size, bool &keep) {
		return WAD_Loader::LoadClient<ID>(client, size, keep);
	}

	WAD_Context() : sp(stack), ts(0) {}
};

struct WAD_Writer {
	static bool	Process(ostream &file, const ISO_browser2 &b);
	static void	ProcessChildren(ostream &file, anything *a) {
		for (int i = 0, n = a->Count(); i < n; i++)
			Process(file, (*a)[i]);
	}
};

} // namespace smc;

//-------------------------------------
// ISO defs
//-------------------------------------

template<> struct ISO_def<smc::dynastring> : TISO_virtual2<smc::dynastring> {
	static ISO_browser2	Deref(const smc::dynastring &a)	{ return ISO_ptr<string>(0,a); }
};

template<typename T> struct ISO_def<rel_ptr_be<T> > : ISO_def<soft_pointer<T,base_relative<int32be> > > {};

template<typename T> struct ISO_def<smc::arrayW<T> > : TISO_virtual2<smc::arrayW<T> > {
	typedef smc::arrayW<T> type;
	static uint32		Count(type &a)					{ return a.size;	}
	static ISO_browser	Index(type &a, int i)			{ return MakeBrowser(a[i]);	}
	static tag2			GetName(type &a, int i)			{ return __GetName(a[i]);	}
};

//-------------------------------------
// Texture Client
//-------------------------------------

namespace smc {
struct TextureServerHeader : Client {
	dynastring	name;
	uint32		ts_offset;
	uint32		stored_mips;
	uint8		checksum[8];	//?
	TextureServerHeader() {
		id = TextureServerID;
		clear(name);
		clear(checksum);
		ts_offset	= 0;
		stored_mips	= ~0;
	}
};

template<> ISO_ptr<void> WAD_Context::LoadClient<Client::TextureServerID>(const Client *client, size_t size, bool &keep) {
	keep = false;

	TextureServerHeader *th	= (TextureServerHeader*)client;
	FileHandler			*fh	= FileHandler::Get("dds");
	uint32				mips = th->stored_mips;
	if (mips != ~0) {
		struct dds_header {
			uint32	DDS;
			iso::uint32	size, flags, height, width, dwPitchOrLinearSize, dwDepth, dwMipMapCount, dwReserved1[11], ddspf[8], dwCaps[4];
		} *dds = (dds_header*)(th + 1);

		memory_block	head		= memory_block(dds, dds->size + 4);
		memory_block	data		= memory_block(head.end(), size);
		uint32			mip_size	= dds->dwPitchOrLinearSize;
		uint32			total_size	= 0;
		for (int i = mips; i < dds->dwMipMapCount; i++) {
			total_size += mip_size;
			mip_size >>= 2;
		}
		ts->seek(th->ts_offset);
		return fh->Read(th->name, combined_istream(combined_istream(MemoryInput(head), istream_offset(*ts, total_size)), MemoryInput(data)));
	}
	return fh->Read(th->name, MemoryInput(memory_block(th + 1, size - sizeof(*th))));
}

} // namespace smc;

//-------------------------------------
// Model Client
//-------------------------------------

namespace smc {

enum {
	kMaxSpheres		= 16,
	kMaxGroups		= 0x2,
};

enum {	
	kRenMinFilter_Mask		= 0x000F,
	kRenMagFilter_Mask		= 0x00F0,
	kRenWrap_SMask			= 0x0F00,
	kRenWrap_TMask			= 0xF000, 
	
	kRenMinFilter_Offset	= 0,
	kRenMagFilter_Offset	= 4,
	kRenWrapS_Offset		= 8,
	kRenWrapT_Offset		= 12,
};

enum {
	TEX_FLAG_HAS_CUTOUT_ALPHA		=1, 
	TEX_FLAG_HAS_ARBITRARY_ALPHA	=2,
	TEX_FLAG_IS_CUBE_MAP			=4
};

struct Texture;

struct TextureRef : bigendian {
	uint8			flags;
	uint8			nummips;
	uint16			type;
	
	uint16			width;
	uint16			height;
	
	float32			lodbias;
	float32			aniso;
	uint8			minfilter;
	uint8			magfilter;
	uint8			wraps;
	uint8			wrapt;
	uint32			gfxhandle;
	
	pointer32<void>	data;
	TextureRef		*next;
	Texture		*srctex;
};

struct StreamDescriptor : bigendian {
	enum {
		// NAME									#	#elem	Dynamic	Edge#	SpuFormat				RSX format	Notes
		UNUSED_MESH_TYPE,					//	0	0		false			0						0			 
		PS3_Pos,							//	1	3		false	1		kSpuAttr_F32			F32
		PS3_Normal,							//	2	3		false	2		kSpuAttr_X11Y11Z10N		X11Y11Z10N
		PS3_DisplacementNormal,				//	3	3		false			0						F32			not used
		PS3_Tangent,						//	4	4		false	3		kSpuAttr_I16N			I16N
		PS3_BiTangent,						//	5	3		false	4		kSpuAttr_X11Y11Z10N		X11Y11Z10N
		PS3_Diffuse,						//	6	4		false			0						F32			not used
		PS3_Specular,						//	7	4		false			0						F32			not used
		PS3_SkinWeights0,					//	8	4		false			0						F32			not used
		PS3_SkinWeights1,					//	9	4		false			0						F32			not used
		PS3_SkinIndices0,					//	10	4		false			0						F32			not used
		PS3_SkinIndices1,					//	11	4		false			0						F32			not used
		PS3_F32Diffuse,						//	12	3		false			0						F32
		PS3_F32Specular,					//	13	4		false			0						F32			not used
		PS3_F32VertexAlpha,					//	14	3		false			0						F16			kRsxAttr_U8N
		Generic_UV0,						//	15	2		false	5		0						F32	
		Generic_UV1,						//	16	2		false	6		0						F32	
		PS3_SkinningFlipFactor,				//	17	1		false			0						F32			not used
		PS3_VertexLightPos,					//	18	3		false			0						F16
		PS3_F32ExtraDiffuse,				//	19	3		false			0						F16
		PS3_F32SimpleDiffuse,				//	20	3		false			0						F32			should not be used, used in simple.fx
		PS3_PS2_F32Diffuse,					//	21	4		false			0						F16			should not be used, only in ps2shader.fx
		PS3_NMblendData,					//	22	3		false			0						F32
		PS3_F32VertexAO,					//	23	1		false			0						F32

		PS3_DynamicDirectLightPos,			//	24	3		true	29		0						F32
		PS3_DynamicDirectUnShadowedColor,	//	25	3		true	30		0						F16
		PS3_DynamicDirectShadowedColor,		//	26	3		true	31		0						F16
		PS3_DynamicIndirectColor,			//	27	3		true	32		0						F16
		PS3_PrevFramePosition,				//	28	3		true	33		0						F32

		Generic_UV2,						//	29	2		false	7		0						F32	
		Generic_UV3,						//	30	2		false	8		0						F32	
		Generic_UV4,						//	31	2		false			0						F32	
		Generic_UV5,						//	32	2		false			0						F32	
	};

	rel_ptr_be<void>	stream_start;
	BITFIELDS3(uint16,
		MeshParameterIndex,			8,
		EdgeStreamIndex,			4,
		VertexBufferID,				4
	);
	BITFIELDS7(uint16,
		StreamStride,				10,
		IsCutoutUVStreamForTex1,	1,
		IsCutoutUVStreamForTex0,	1,
		IsInstanced,				1,
		AlignDiscontiguous,			1,
		InterleavedHead,			1,
		IsEdgeStream,				1
	);
	uint32				instance_offset;

	template<typename D, typename T> static ISO_ptr<void>	make(T *p) {
		ISO_ptr<D>	d(0);
		*d = *p;
		return d;
	}
	ISO_ptr<void>	index(int i) const {
		void *p = (uint8*)stream_start.get() + StreamStride.get() * i;
		switch (MeshParameterIndex.get()) {
			default: return ISO_NULL;
			case PS3_Pos:				return make<float3p>((soft_vector3<float32be>	*)p);
			case PS3_Normal:			return make<float3p>((norm3_11_11_10_be			*)p);
			case PS3_DisplacementNormal:return make<float3p>((soft_vector3<float32>		*)p);
			case PS3_Tangent:			return make<float4p>((soft_vector4<norm16be>	*)p);
			case PS3_BiTangent:			return make<float3p>((norm3_11_11_10_be			*)p);
			case PS3_Diffuse:			return make<float4p>((soft_vector4<float32be>	*)p);
			case PS3_Specular:			return make<float4p>((soft_vector4<float32be>	*)p);
			case PS3_SkinWeights0:		return make<float4p>((soft_vector4<float32be>	*)p);
			case PS3_SkinWeights1:		return make<float4p>((soft_vector4<float32be>	*)p);
			case PS3_SkinIndices0:		return make<float4p>((soft_vector4<float32be>	*)p);
			case PS3_SkinIndices1:		return make<float4p>((soft_vector4<float32be>	*)p);
			case PS3_F32Diffuse:		return make<float3p>((soft_vector3<float32be>	*)p);
			case PS3_F32Specular:		return make<float4p>((soft_vector4<float32be>	*)p);
			case PS3_F32VertexAlpha:	return make<float3p>((soft_vector3<float16be>	*)p);
			case Generic_UV0:			return make<float2p>((soft_vector2<float32be>	*)p);
			case Generic_UV1:			return make<float2p>((soft_vector2<float32be>	*)p);
			case PS3_SkinningFlipFactor:return make<float>	((float32be					*)p);
			case PS3_VertexLightPos:	return make<float3p>((soft_vector3<float16be>	*)p);
			case PS3_F32ExtraDiffuse:	return make<float3p>((soft_vector3<float16be>	*)p);
			case PS3_F32SimpleDiffuse:	return make<float3p>((soft_vector3<float32be>	*)p);
			case PS3_PS2_F32Diffuse:	return make<float4p>((soft_vector4<float16be>	*)p);
			case PS3_NMblendData:		return make<float3p>((soft_vector3<float32be>	*)p);
			case PS3_F32VertexAO:		return make<float>	((float32be					*)p);
			case Generic_UV2:			return make<float2p>((soft_vector2<float32be>	*)p);
			case Generic_UV3:			return make<float2p>((soft_vector2<float32be>	*)p);
			case Generic_UV4:			return make<float2p>((soft_vector2<float32be>	*)p);
			case Generic_UV5:			return make<float2p>((soft_vector2<float32be>	*)p);
		}
	}
	struct stream : ISO_virtual_defaults {
		const StreamDescriptor	*desc;
		uint32					nv;
		stream(const StreamDescriptor *_desc) : desc(_desc), nv(10) {}
		uint32			Count()			const { return nv; }
		ISO_browser2	Index(int i)	const { return desc->index(i); }
	};
	ISO_ptr<void> get_stream() const {
		return ISO_ptr<stream>(0, this);
	}
};

struct ChunkBoundInfo : bigendian {
	float32	sphere_center[3];
	uint8	bone_index_low;
	uint8	exp;
	uint16	mantissa;
	
	int		GetBoneIndex() const {
		return bone_index_low | ((mantissa >> 6) & 0x0300);
	}
	float	GetRadius() const {
		iso::uint32	u = (exp << 23) | ((mantissa & 0x3FFF) << 9);
		return (float&)u;
	}
};

struct MeshChunk : bigendian {
	uint32		num_verts;
	BITFIELDS2(uint32,
		num_prims,	24,
		hole_size,	8
	);
	rel_ptr_be<EdgeGeom::CompressedIndices>	indices;
	//these two are QW offsets into the prim's bounds info indicating where our bounds info is and how big it is...
	uint16				uQWBoundsOffset;
	uint16				uQWBoundsSize;
	ISO_ptr<ISO_openarray<iso::uint16> > get_indices() const {
		ISO_ptr<ISO_openarray<iso::uint16> > a;
		iso::uint32	n = num_prims.get() * 3;
		indices->Decompress(a.Create(0)->Create(n), n);
		return a;
	}
};

static const char *edge_attribute_names[] = {
	0,
	"position",
	"normal",
	"tangent",
	"binormal",
	"uv0",
	"uv1",
	"uv2",
	"uv3",
	"color",
};

struct SPU_vertex : ISO_virtual_defaults {
	EdgeGeom::VertexStreamDescription	*desc;
	void	*data;
	SPU_vertex(EdgeGeom::VertexStreamDescription *_desc, void *_data) : desc(_desc), data(_data) {}
	uint32			Count()	const {
		return desc->numAttributes;
	}
	ISO_browser2	Index(int i) const {
		float	v[4];
		switch (desc->blocks[i].Decompress(data, v)) {
			case 1:	return MakePtr(0, (fixed_array<float,1>&)v);
			case 2:	return MakePtr(0, (fixed_array<float,2>&)v);
			case 3:	return MakePtr(0, (fixed_array<float,3>&)v);
			case 4:	return MakePtr(0, (fixed_array<float,4>&)v);
			default:
				return ISO_browser2();
		}
	}
	tag2			GetName(int i) const {
		int		a	= desc->blocks[i].edgeAttributeId;
		return a < num_elements(edge_attribute_names) ? edge_attribute_names[a] : 0;
	}
};

struct SPU_vertex_stream : ISO_virtual_defaults {
	EdgeGeom::VertexStreamDescription	*desc;
	void	*data;
	uint32	nv;
	SPU_vertex_stream(EdgeGeom::VertexStreamDescription *_desc, void *_data, uint32 _nv) : desc(_desc), data(_data), nv(_nv) {}
	uint32			Count()	const {
		return nv;
	}
	ISO_browser2	Index(int i) const {
		return MakePtr(tag2(), SPU_vertex(desc, (uint8*)data + desc->stride * i));
	}
};

struct MeshChunkEdgeData : bigendian {
	enum {
		kEdgeSPUBufferSize		= 48 * 1024,
		kEdgeDataSPUBufferSize	= kEdgeSPUBufferSize - 256,
		kNumLODs				= 2,
	};

	EdgeGeom::SpuConfigInfo	spuConfigInfo;
	rel_ptr_be<EdgeGeom::CompressedIndices>			SpuIndices[kNumLODs];
	uint16					SpuIndicesSizes[2][kNumLODs];
	rel_ptr_be<uint8>		SpuVertices[2];
	uint16					SpuVerticesSizes[6];
	uint16					SkinMatricesByteOffsets[2];
	uint16					SkinMatricesSizes[2];
	uint16					SkinIndicesAndWeightsSizes[2];
	rel_ptr_be<uint8>		SkinIndicesAndWeights;
	rel_ptr_be<EdgeGeom::VertexStreamDescription>	SpuInputStreamDescs[2];
	rel_ptr_be<EdgeGeom::VertexStreamDescription>	SpuOutputStreamDesc;
	uint16					SpuInputStreamDescSizes[2];
	uint16					SpuOutputStreamDescSize;
	uint16					LODNumIndices[kNumLODs];
	uint16					toolTimeIOBufferSize;
	uint16					pad[2];

	fixed_array<ISO_ptr<ISO_openarray<iso::uint16> >, 2> get_spu_indices()	const	{
		fixed_array<ISO_ptr<ISO_openarray<iso::uint16> >, 2>	a;
		iso::uint16	ni = spuConfigInfo.numIndexes;

		if (SpuIndicesSizes[0][0])
			SpuIndices[0]->Decompress(a[0].Create(0)->Create(ni), ni);
		
		if (SpuIndicesSizes[0][1])
			SpuIndices[1]->Decompress(a[1].Create(0)->Create(ni), ni);
		return a;
	}
	fixed_array<ISO_ptr<void>, 2> get_spu_vertices()	const	{
		fixed_array<ISO_ptr<void>, 2>	a;
		iso::uint16	nv = spuConfigInfo.numVertexes;

		if (SpuVerticesSizes[0])
			a[0] = MakePtr(0, SPU_vertex_stream(SpuInputStreamDescs[0], SpuVertices[0], nv));

		if (SpuVerticesSizes[3])
			a[1] = MakePtr(0, SPU_vertex_stream(SpuInputStreamDescs[1], SpuVertices[1], nv));
		return a;
	}
};

struct Prim {
	enum {
		kPrimSkinned					= 0x0001, 
		kPrimHasTangentsAndBiTangents	= 0x0002,
		kPrimHasNonUnitVertexAlpha		= 0x0004,
		kPrimHasTransparency			= 0x0008,
		kPrimCastsShadows				= 0x0010,
		kPrimReceivesShadows			= 0x0020,
		kPrimCastShadowMask				= 0x03C0,
		kPrimCastShadowSelf				= 0x0040,
		kPrimCastShadowPrimary			= 0x0080,
		kPrimCastShadowSecondary		= 0x0100,
		kPrimCastShadowTertiary			= 0x0200,
		kPrimShadowClassMask			= 0x0C00,
		kPrimShadowClassPrimary			= 0x0400,
		kPrimShadowClassSecondary		= 0x0800,
		kPrimShadowClassTertiary		= 0x0C00,
		kPrimShadowProxy				= 0x1000,
		kPrimHasVertexAlphaStream		= 0x2000,
		kSpecialHRCaster				= 0x4000,
	};
	uint16be						num_streams;
	uint16be						num_chunks;
	uint16be						num_instances;
	uint16be						material_id;
	uint16be						total_bounds;
	uint16be						num_matrices;
	uint16be						prim_flags;
	uint16be						max_num_bones_per_vert;
	rel_ptr_be<uint16be>			matrices;
	rel_ptr_be<StreamDescriptor>	streams;
	rel_ptr_be<MeshChunk>			chunks;
	rel_ptr_be<MeshChunkEdgeData>	chunks_edgedata;
	rel_ptr_be<ChunkBoundInfo>		bounds;

	static uint32 GetMemSizeNoData(uint32 numchunks, uint32 numstreams, uint32 nummatrices) {
		uint32	size = sizeof(Prim)
					+ sizeof(StreamDescriptor)	* numstreams
					+ sizeof(MeshChunk)			* numchunks
					+ sizeof(uint16)			* ((nummatrices + 1) & ~1);
		return (size + 15) & ~15;
	}

	//this gets the size of the RenPrim and its non-common embedded data - includes padding to align to 16 bytes...
	uint32	GetSize()	const { return GetMemSizeNoData(num_chunks, num_streams, num_matrices); }
	Prim	*next()		const { return (Prim*)((uint8*)this + GetSize()); }

	_range<uint16be*>			get_matrices()			const	{ return _range<uint16be*>			(matrices,	num_matrices);	}
	_range<StreamDescriptor*>	get_streams()			const	{ return _range<StreamDescriptor*>	(streams,	num_streams);	}
	_range<MeshChunk*>			get_chunks()			const	{ return _range<MeshChunk*>			(chunks,	num_chunks);		}
	_range<MeshChunkEdgeData*>	get_chunkedgedata()		const	{ return _range<MeshChunkEdgeData*>	(chunks_edgedata, num_chunks);	}
	_range<ChunkBoundInfo*>		get_bounds()			const	{ return _range<ChunkBoundInfo*>	(bounds,	total_bounds);}
};

struct PrimParm : Client {
	enum Type {
		kPS3Parm		= 0,
		kOccluderParm	= 1,
	};
	uint32		num_dma_chunks;
	uint16		material_id;
	uint16		num_transforms;
	float32		mipmap_constant;
#if defined(USING_SWAPPABLE_MATERIALS)
	int16		swap_chain_ids[MAX_SWAPPABLE_MATS];
	uint16		num_swappables;
	uint16		copy_of_material_id; // Is set in tools when we set the swappables
#endif
	ISO_ptr<void>	operator*() const {
		bool	keep = true;
		return WAD_Loader::LoadClient(this, 0, keep);
	}
};

struct TriParm : PrimParm {
	uint32				num_prims;
	off_ptr64_be<Prim>	prim;
	next_iterator<Prim>	begin()	const { return prim.get(this); }
	next_iterator<Prim>	end()	const { return (int)num_prims; }
};

} // namespace smc;

//-------------------------------------
// PS3Tri Client
//-------------------------------------

ISO_DEFVIRT(smc::StreamDescriptor::stream);
ISO_DEFVIRT(smc::SPU_vertex);
ISO_DEFVIRT(smc::SPU_vertex_stream);
ISO_DEFSAME(memory_block2, memory_block);

ISO_DEFUSERCOMP(smc::ChunkBoundInfo,3) {
	ISO_SETFIELD(0,sphere_center);
	ISO_SETACCESSORX(1,GetBoneIndex,"bone");
	ISO_SETACCESSORX(2,GetRadius,"radius");
}};

ISO_DEFUSERCOMP(smc::StreamDescriptor,12) {
	ISO_SETFIELDS7(0,MeshParameterIndex,EdgeStreamIndex,VertexBufferID,StreamStride,IsCutoutUVStreamForTex1,IsCutoutUVStreamForTex0,IsInstanced);
	ISO_SETFIELDS4(7,AlignDiscontiguous,InterleavedHead,IsEdgeStream,instance_offset);
	ISO_SETACCESSORX(11,get_stream, "data");
}};

ISO_DEFUSERCOMP(smc::MeshChunk,6) {
	ISO_SETFIELDS5(0, num_verts, num_prims, hole_size, uQWBoundsOffset, uQWBoundsSize);
	ISO_SETACCESSORX(5,get_indices,"indices");
}};


ISO_DEFUSERCOMP7(EdgeGeom::AttributeBlock,offset,format,componentCount,edgeAttributeId,size,vertexProgramSlotIndex,fixedBlockOffset);
ISO_DEFUSERCOMP8(EdgeGeom::AttributeFixedBlock,integer0,mantissa0,integer1,mantissa1,integer2,mantissa2,integer3,mantissa3);
ISO_DEFUSERCOMP(EdgeGeom::VertexStreamDescription,3) {
	ISO_SETFIELDS2(0, numAttributes,stride);
	ISO_SETACCESSORX(2,get_blocks,"blocks");
}};

ISO_DEFUSERCOMP(EdgeGeom::SpuConfigInfo,11) {
	ISO_SETFIELDS8(0,flagsAndUniformTableCount,commandBufferHoleSize,inputVertexFormatId,secondaryInputVertexFormatId,outputVertexFormatId,vertexDeltaFormatId,indexesFlavorAndSkinningFlavor,skinningMatrixFormat);
	ISO_SETFIELDS3(8,numVertexes,numIndexes,indexesOffset);
}};

ISO_DEFUSERCOMP(smc::MeshChunkEdgeData,11) {
	ISO_SETFIELD(0,spuConfigInfo);
	ISO_SETACCESSORX(1,get_spu_indices,"indices");
	ISO_SETACCESSORX(2,get_spu_vertices,"vertices");
	ISO_SETFIELDS4(3, SkinMatricesByteOffsets,SkinMatricesSizes,SkinIndicesAndWeightsSizes,SkinIndicesAndWeights);
	ISO_SETFIELD(7,SpuInputStreamDescs);
	ISO_SETFIELD(8,SpuOutputStreamDesc);
	ISO_SETFIELDS2(9, LODNumIndices,toolTimeIOBufferSize);
}};

ISO_DEFUSERCOMP(smc::Prim,7) {
	ISO_SETFIELD(0,material_id);
	ISO_SETFIELD(1,prim_flags);
	ISO_SETACCESSORX(2,get_matrices,"matrices");
	ISO_SETACCESSORX(3,get_streams,"streams");
	ISO_SETACCESSORX(4,get_chunks,"chunks");
	ISO_SETACCESSORX(5,get_chunkedgedata, "edge");
	ISO_SETACCESSORX(6,get_bounds, "bounds");
}};

namespace smc {
template<> ISO_ptr<void> WAD_Loader::LoadClient<Client::PS3TriServerID>(const Client *client, size_t size, bool &keep) {
	keep	= true;
	TriParm	*tri = (TriParm*)client;
	return MakePtr("PS3Tri", range(tri->begin(), tri->end()));
}
} // namespace smc;

//-------------------------------------
// Material Client
//-------------------------------------
namespace smc {

struct GLPassState {
	enum {
		kGLStateAlphaRef,
		kGLStateFlipbookIndex,
		kNumAnimatedGLStates,
	};
	enum STATE0 {
		AlphaToCoverageEnable	= 0x01,
		FREE_TO_USE				= 0x02,
		DisableRBufferWrites	= 0x04,
		DisableGBufferWrites	= 0x08,
		DisableBBufferWrites	= 0x10,
		DisableABufferWrites	= 0x20,
		DisableZBufferWrites	= 0x40,
		DoubleSided				= 0x80,
	};

	enum STATE1 {
		FaceCullDir		= 0x01,
		DepthCull		= 0x02,
		DepthClamp		= 0x04,
		IgnoreDepthW	= 0x08,
		PolyStipple		= 0x10,
		NeverSet		= 0x80,
	};
	struct ConstColor	{ float32 r, g, b, a; };
	struct ConstColor8	{ uint8 r, g, b, a; };
	struct FrontBack	{ uint8 front, back; };
	struct Channel		{ uint8 rgb, a; };

	FrontBack	StencilFunc, StencilFuncRefValue, StencilFuncMask, StencilMask;
	FrontBack	StencilFailOp, StencilDepthFailOp, StencilDepthPassOp;
	uint8		AlphaFunc, DepthFunc;
	Channel		BlendFunc, BlendFactorSrc, BlendFactorDest;
	ConstColor8	const_color;
	uint8		AlphaFuncRefVal;
	uint8		state0, state1;
	uint8		pad[1];
	uint16		MultisampleMask;
};

enum {
	k_bool, k_int, k_float, k_half, k_uint8,
	k_float3, k_half3, k_float3x2
};
#define CONSTANTGL(type,name,rarelyChange,isSRGB,isFeature,def_mask, def_val)	{ #name, 0, 1, sizeof(GLPassState), k_uint8, rarelyChange, isFeature, isSRGB, def_mask, def_val},
#define CONSTANT(type,name,rarelyChange,isSRGB,isFeature,def_mask, def_val)		{ #name, 0, 1, 1, k_##type, rarelyChange, isFeature, isSRGB, true, def_mask, def_val},
#define CONSTANTSRGB(type,name,rarelyChange,isSRGB,isFeature,def_mask, def_val)	{ #name, 0, 1, 1, k_##type, rarelyChange, isFeature, isSRGB, false, def_mask, def_val},

#define V3(A,B,C)		{A,B,C}
#define V6(A,B,C,D,E,F) {A,B,C,E,F}

struct UserConstant {
	static uint64	default_mask;
	static void		Init();

	char	name[64];
	uint8	feature_index;
	uint8	elem_size;			//Defines vector size of each element (1, 2, 3, 4) 		
	uint8	elem_num;			//number of elements in the array
	uint8	elem_type;			//type of each element
	uint8	rarely_changing:1,feature:1,is_srgb:1,is_srgb_past_one:1,default_on:1;
	float	default_val[16];
};

UserConstant user_constants[] ={
	CONSTANT	(bool,		UNUSED_CODE_DEPENDENT_TYPE,			0,	0,	0,	0,	false) //{ DO NOT TOUCH THIS BLOCK
	CONSTANTGL	(float,		PS3_CommonGLFragmentData,			0,	0,	0,	0,	0)
	CONSTANT	(float,		PS3_PS2_bTextureAlphaUsed,			0,	0,	0,	0,	0) //----
	CONSTANT	(float,		PS3_PS2_bAlphaMapUsed,				0,	0,	0,	0,	0) //----
	CONSTANT	(float,		PS3_PS2_bSkinning,					0,	0,	0,	0,	0) //----
	CONSTANT	(float,		PS3_PS2_b4To8Weights,				0,	0,	0,	0,	0) //----
	CONSTANT	(float,		PS3_PS2bHasDiffuse,					0,	0,	0,	0,	0) //DO NOT TOUCH THIS BLOCK }
	CONSTANT	(bool,		INVALID_USER_TYPE,					0,	0,	0,	0,	false)
	CONSTANT	(bool,		PS3_RF_Root,						1,	0,	0,	0,	false)
	CONSTANT	(bool,		PS3_PS2_bEnvMap,					0,	0,	0,	0,	false)
	CONSTANT	(bool,		PS3_RF_DynamicVertexLighting,		0,	0,	1,	1,	true)
	CONSTANT	(bool,		PS3_RF_BetterAmbient,				0,	0,	0,	0,	false)
	CONSTANT	(float,		PS3_DefaultAmbientMonoSH4_0,		0,	0,	0,	0,	1)
	CONSTANT	(float,		PS3_DefaultAmbientMonoSH4_1,		0,	0,	0,	0,	0)
	CONSTANT	(float,		PS3_DefaultAmbientMonoSH4_2,		0,	0,	0,	0,	0.5f)
	CONSTANT	(float,		PS3_DefaultAmbientMonoSH4_3,		0,	0,	0,	0,	0)
	CONSTANT	(bool,		PS3_RF_DirBakedVertexLight,			0,	0,	1,	1,	false)
	CONSTANT	(bool,		PS3_RF_AmbBakedVertexLight,			0,	0,	1,	1,	false)
	CONSTANT	(bool,		PS3_RF_SimpleBakedVertexLight,		0,	0,	0,	0,	false)
	CONSTANT	(bool,		PS3_RF_VertexOpacity,				0,	0,	1,	1,	false)
	CONSTANT	(bool,		PS3_RF_DirLightmap,					0,	0,	1,	1,	false)
	CONSTANT	(bool,		PS3_RF_MulLightmap,					0,	0,	0,	0,	false)
	CONSTANT	(bool,		PS3_RF_SimpleLightMap,				0,	0,	0,	0,	false)
	CONSTANT	(bool,		PS3_RF_Emissive,					0,	0,	1,	1,	false)
	CONSTANTSRGB(float3,	PS3_Emissive,						0,	1,	0,	0,	V3(0.1f, 0.1f, 0.1f)) //color
	CONSTANT	(bool,		PS3_RF_VertexLightProbes,			0,	0,	1,	1,	false)
	CONSTANT	(bool,		PS3_RF_PixelLightProbes,			0,	0,	1,	1,	false)
	CONSTANT	(bool,		PS3_RF_DiffuseMap,					0,	0,	1,	1,	false)
	CONSTANTSRGB(half3,		PS3_TerrainDiffTint0,				0,	1,	0,	0,	V3(1, 1, 1)) //color
	CONSTANT	(bool,		PS3_RF_DiffuseMap1,					0,	0,	1,	1,	false)
	CONSTANTSRGB(half3,		PS3_TerrainDiffTint1,				0,	1,	0,	0,	V3(1, 1, 1)) //color
	CONSTANT	(bool,		PS3_RF_DiffuseMap2,					0,	0,	1,	1,	false)
	CONSTANTSRGB(half3,		PS3_TerrainDiffTint2,				0,	1,	0,	0,	V3(1, 1, 1)) //color
	CONSTANTSRGB(half3,		PS3_DiffuseTint,					0,	1,	0,	0,	V3(1, 1, 1)) //color
	CONSTANT	(bool,		PS3_RF_NormalMap,					0,	0,	1,	1,	false)
	CONSTANT	(bool,		PS3_RF_NormalMap1,					0,	0,	1,	1,	false)
	CONSTANT	(bool,		PS3_RF_NonCompressed_NormalMap,		0,	0,	1,	1,	false)
	CONSTANT	(half,		PS3_NormalTransparency,				0,	0,	0,	0,	1)
	CONSTANT	(bool,		PS3_RF_GlossMap,					0,	0,	1,	1,	false)
	CONSTANT	(half,		PS3_Shininess,						0,	0,	0,	1,	0.353f)
	CONSTANTSRGB(half3,		PS3_TerrainSpecTint0,				0,	1,	0,	0,	V3(1, 1, 1)) //color
	CONSTANT	(bool,		PS3_RF_SpecularMap,					0,	0,	1,	1,	false)
	CONSTANT	(bool,		PS3_RF_GlossMap1,					0,	0,	1,	1,	false)
	CONSTANT	(half,		PS3_Shininess1,						0,	0,	0,	0,	0.353f)
	CONSTANTSRGB(half3,		PS3_TerrainSpecTint1,				0,	1,	0,	0,	V3(1, 1, 1)) //color
	CONSTANT	(bool,		PS3_RF_SpecularMap1,				0,	0,	1,	1,	false)
	CONSTANT	(bool,		PS3_RF_TransAffectsSpecular,		0,	0,	1,	1,	false)
	CONSTANT	(bool,		PS3_RF_DualSpecularTints,			0,	0,	1,	1,	false)
	CONSTANTSRGB(half3,		PS3_SpecularTint,					0,	1,	0,	0,	V3(1, 1, 1)) //color
	CONSTANTSRGB(half3,		PS3_SpecularTint2,					0,	1,	0,	0,	V3(1, 1, 1)) //color
	CONSTANT	(half,		PS3_SpecularTintShift,				0,	0,	0,	0,	0.5f)
	CONSTANT	(bool,		PS3_RF_EnvMap,						0,	0,	1,	1,	false)
	CONSTANT	(bool,		PS3_Light_Modulates_Envmap,			0,	0,	1,	1,	false)
	CONSTANT	(bool,		PS3_Ambient_Modulates_Envmap,		0,	0,	1,	1,	false)
	CONSTANT	(bool,		PS3_RF_HDR_Envmap,					0,	0,	1,	1,	true)
	CONSTANT	(bool,		PS3_RF_Gloss_Blurs_Envmap,			0,	0,	1,	1,	true)
	CONSTANT	(float,		PS3_Envmap_Max_Mip,					0,	0,	0,	0,	8.0f)
	CONSTANTSRGB(half3,		PS3_Envmap_Tint,					0,	1,	0,	0,	V3(1, 1, 1)) //color
	CONSTANT	(bool,		PS3_RF_Fresnel2,					0,	0,	1,	1,	false)
	CONSTANT	(bool,		PS3_RF_Fresnel4,					0,	0,	1,	1,	false)
	CONSTANT	(bool,		PS3_RF_Fresnel5,					0,	0,	1,	1,	false)
	CONSTANT	(float,		PS3_BaseMirrorness,					0,	0,	0,	0,	0)
	CONSTANT	(float,		PS3_Fresnel_Cap,					0,	0,	0,	0,	1)
	CONSTANT	(bool,		PS3_Fresnel_Specular,				0,	0,	1,	1,	false)
	CONSTANT	(bool,		PS3_Fresnel_EnvMap,					0,	0,	1,	1,	false)
	CONSTANT	(bool,		PS3_Fresnel_Emissive,				0,	0,	1,	1,	false)
	CONSTANT	(bool,		PS3_Fresnel_Transparency,			0,	0,	1,	1,	false)
	CONSTANT	(bool,		PS3_RF_PixelAmbientOcclusion,		0,	0,	0,	0,	false)
	CONSTANT	(bool,		PS3_RF_AmbientOcclusionMap,			0,	0,	1,	1,	false)
	CONSTANT	(half,		PS3_AmbOC_Specular,					0,	0,	0,	0,	0)
	CONSTANT	(half,		PS3_AmbOC_Diffuse,					0,	0,	0,	0,	0.5f)
	CONSTANT	(bool,		PS3_RF_ParallaxMapping,				0,	0,	1,	1,	false)
	CONSTANT	(float,		PS3_HeightScale,					0,	0,	0,	0,	1)
	CONSTANT	(float,		PS3_HeightBias,						0,	0,	0,	0,	0.5f)
	CONSTANT	(float,		PS3_ParallaxFix,					0,	0,	0,	0,	0.5f)
	CONSTANT	(float,		PS3_ParallaxGuard,					0,	0,	0,	0,	0.05f)
	CONSTANT	(bool,		PS3_RF_AlphaMap,					0,	0,	1,	1,	false)
	CONSTANT	(half,		PS3_Transparency,					0,	0,	0,	0,	0)
	CONSTANT	(bool,		PS3_RF_AlphaMap2,					0,	0,	1,	1,	false)
	CONSTANT	(half,		PS3_Transparency_2_,				0,	0,	0,	0,	1)
	CONSTANT	(float,		PS3_U_Scale,						0,	0,	0,	0,	1)
	CONSTANT	(float,		PS3_V_Scale,						0,	0,	0,	0,	1)
	CONSTANT	(float,		PS3_U_Scale1,						0,	0,	0,	0,	1)
	CONSTANT	(float,		PS3_V_Scale1,						0,	0,	0,	0,	1)
	//CONSTANT	(bool,		PS3_RF_PremultiplyAlpha,			0,	0,	1,	1,	false)
	CONSTANT	(bool,		PS3_RF_SharpAlphaToCoverage,		0,	0,	1,	1,	false)
	CONSTANT	(bool,		PS3_RF_DoubleSidedLighting,			0,	0,	1,	1,	false)
	CONSTANT	(bool,		PS3_RF_Animated_NormalMaps,			0,	0,	1,	1,	false)
	CONSTANT	(bool,		PS3_RF_DepthFade,					0,	0,	1,	1,	false)
	CONSTANT	(float,		PS3_DepthFadeDistance,				0,	0,	0,	0,	1)
	CONSTANT	(int,		PS3_RF_LayerBlendMode0,				0,	0,	0,	0,	0)	//uint32
	CONSTANT	(int,		PS3_RF_LayerBlendMode1,				0,	0,	0,	0,	0)	//uint32
	CONSTANT	(bool,		PS3_RF_Single_AlphaSource,			0,	0,	1,	1,	false)
	CONSTANT	(bool,		PS3_RF_IsOVER_NormalMap1,			0,	0,	1,	1,	false)
	CONSTANT	(float,		PS3_A2C_Width,						0,	0,	0,	1,	256.0f)
	CONSTANT	(float,		PS3_A2C_Height,						0,	0,	0,	1,	256.0f)
	CONSTANT	(float,		layer_blender0,						0,	0,	0,	0,	1)
	CONSTANT	(float,		layer_blender1,						0,	0,	0,	0,	1)
	CONSTANT	(float,		layer_blender2,						0,	0,	0,	0,	1)
	CONSTANT	(float,		layer_blender3,						0,	0,	0,	0,	1)
	CONSTANT	(float,		normalmap_blender,					0,	0,	0,	0,	1)
	CONSTANT	(bool,		PS3_RF_WhiteLines,					0,	0,	0,	0,	false)
	CONSTANT	(bool,		PS3_RF_Text,						0,	0,	1,	1,	false)
	CONSTANT	(bool,		PS3_RF_VertexAmbientOcclusion,		0,	0,	1,	1,	false)
	CONSTANT	(half,		PS3_AmbOC_Indirect,					0,	0,	0,	0,	1)
	CONSTANT	(bool,		PS3_IgnoreDiffuseMapAlpha,			0,	0,	1,	1,	false)
	CONSTANT	(float3x2,	uv_mat0,							0,	0,	0,	0,	V6(1, 0, 0, 1, 0, 0))
	CONSTANT	(float3x2,	uv_mat1,							0,	0,	0,	0,	V6(1, 0, 0, 1, 0, 0))
	CONSTANT	(float3x2,	uv_mat2,							0,	0,	0,	0,	V6(1, 0, 0, 1, 0, 0))
	CONSTANT	(float3x2,	uv_mat3,							0,	0,	0,	0,	V6(1, 0, 0, 1, 0, 0))
	CONSTANT	(float3x2,	uv_mat4,							0,	0,	0,	0,	V6(1, 0, 0, 1, 0, 0))
	CONSTANT	(float3x2,	uv_mat5,							0,	0,	0,	0,	V6(1, 0, 0, 1, 0, 0))
	CONSTANT	(float3x2,	uv_mat6,							0,	0,	0,	0,	V6(1, 0, 0, 1, 0, 0))
	CONSTANT	(float3x2,	uv_mat7,							0,	0,	0,	0,	V6(1, 0, 0, 1, 0, 0))
	CONSTANT	(float3x2,	uv_mat8,							0,	0,	0,	0,	V6(1, 0, 0, 1, 0, 0))
	CONSTANT	(float3x2,	uv_mat9,							0,	0,	0,	0,	V6(1, 0, 0, 1, 0, 0))
	CONSTANT	(float3x2,	uv_mat10,							0,	0,	0,	0,	V6(1, 0, 0, 1, 0, 0))
	CONSTANT	(float3x2,	uv_mat11,							0,	0,	0,	0,	V6(1, 0, 0, 1, 0, 0))
	CONSTANT	(float3x2,	uv_mat12,							0,	0,	0,	0,	V6(1, 0, 0, 1, 0, 0))
	CONSTANT	(float3x2,	uv_mat13,							0,	0,	0,	0,	V6(1, 0, 0, 1, 0, 0))
	CONSTANT	(float3x2,	uv_mat14,							0,	0,	0,	0,	V6(1, 0, 0, 1, 0, 0))
	CONSTANT	(float3x2,	uv_mat15,							0,	0,	0,	0,	V6(1, 0, 0, 1, 0, 0))
	CONSTANT	(bool,		PS3_RF_DiffuseWrap,					0,	0,	1,	1,	false)
	CONSTANT	(half,		PS3_DiffuseWrapR,					0,	0,	0,	0,	0)
	CONSTANT	(half,		PS3_DiffuseWrapG,					0,	0,	0,	0,	0)
	CONSTANT	(half,		PS3_DiffuseWrapB,					0,	0,	0,	0,	0)
	CONSTANT	(bool,		PS3_RF_NormalMap_Blur,				0,	0,	1,	1,	false)
	CONSTANT	(half,		PS3_NormalMap_MipBias,				0,	0,	0,	0,	0)
	CONSTANT	(bool,		PS3_RF_AmbientOcclusionMap1,		0,	0,	1,	1,	false)
	CONSTANT	(bool,		PS3_RF_DiffuseNormalSeparation,		0,	0,	0,	0,	false)
	CONSTANT	(half,		PS3_DiffuseNormalSeparation_N_G,	0,	0,	0,	0,	0.28f)
	CONSTANT	(half,		PS3_DiffuseNormalSeparation_N_B,	0,	0,	0,	0,	0.4f)
	CONSTANT	(half,		PS3_DiffuseNormalSeparation_ClampGB,0,	0,	0,	0,	1.1f)
	CONSTANT	(half,		PS3_DiffuseNormalSeparation_ClampR,	0,	0,	0,	0,	1.4f)
	CONSTANT	(bool,		PS3_RF_DuDvShift,					0,	0,	1,	1,	false)
	CONSTANT	(float,		PS3_DuDvScale,						0,	0,	0,	0,	0)
	CONSTANT	(bool,		PS3_RF_SoftParticles,				0,	0,	0,	0,	false)
	CONSTANT	(float,		PS3_SoftParticlesDistance,			0,	0,	0,	0,	1)
	CONSTANT	(bool,		PS3_RF_Specular,					0,	0,	1,	1,	false)
	CONSTANT	(bool,		PS3_RF_RimLight,					0,	0,	1,	1,	false)
	CONSTANT	(float,		layer_blender4,						0,	0,	0,	0,	1)
	CONSTANT	(float,		layer_blender5,						0,	0,	0,	0,	1)
	CONSTANT	(float,		layer_blender6,						0,	0,	0,	0,	1)
	CONSTANT	(float,		layer_blender7,						0,	0,	0,	0,	1)
	CONSTANT	(bool,		PS3_RF_HeadlightSpecular,			0,	0,	1,	1,	false)
	CONSTANT	(half,		PS3_HeadlightSpecular,				0,	0,	0,	0,	1)
	CONSTANT	(half,		PS3_AmbOCFromNormal,				0,	0,	0,	0,	1)
	CONSTANT	(float,		PS3_Text_Thresh,					0,	0,	0,	0,	0.5f)
	CONSTANT	(float,		PS3_Text_AA,						0,	0,	0,	0,	0.02f)
	CONSTANT	(float,		PS3_Text_ShadowThresh1,				0,	0,	0,	0,	0.1f)
	CONSTANT	(float,		PS3_Text_ShadowOffsetX1,			0,	0,	0,	0,	0)
	CONSTANT	(float,		PS3_Text_ShadowOffsetY1,			0,	0,	0,	0,	0)
	CONSTANT	(half3,		PS3_Text_ShadowAddColor1,			0,	1,	0,	0,	V3(0,0,0)) //color
	CONSTANT	(float,		PS3_Text_ShadowAddTrans1,			0,	0,	0,	0,	0)
	CONSTANT	(half3,		PS3_Text_ShadowMulColor1,			0,	1,	0,	0,	V3(0,0,0)) //color
	CONSTANT	(float,		PS3_Text_ShadowMulTrans1,			0,	0,	0,	0,	0)
	CONSTANT	(float,		PS3_Text_ShadowThresh2,				0,	0,	0,	0,	0.1f)
	CONSTANT	(float,		PS3_Text_ShadowOffsetX2,			0,	0,	0,	0,	0)
	CONSTANT	(float,		PS3_Text_ShadowOffsetY2,			0,	0,	0,	0,	0)
	CONSTANT	(half3,		PS3_Text_ShadowAddColor2,			0,	1,	0,	0,	V3(0,0,0)) //color
	CONSTANT	(float,		PS3_Text_ShadowAddTrans2,			0,	0,	0,	0,	0)
	CONSTANT	(half3,		PS3_Text_ShadowMulColor2,			0,	1,	0,	0,	V3(0,0,0)) //color
	CONSTANT	(float,		PS3_Text_ShadowMulTrans2,			0,	0,	0,	0,	0)
	CONSTANT	(bool,		PS3_RF_EnvMapLighting,				0,	0,	1,	1,	false)
	CONSTANT	(half,		PS3_EnvMapLightingBlur,				0,	0,	0,	0,	1)
	CONSTANT	(half,		PS3_EnvMapLightingScale,			0,	0,	0,	0,	1)
	CONSTANT	(half,		PS3_EnvMapLightingBias,				0,	0,	0,	0,	1)
	CONSTANT	(bool,		PS3_RF_SecondaryLight,				0,	0,	1,	1,	false)
	CONSTANT	(float,		PS3_Distortion,						0,	0,	0,	0,	0.5f)
	CONSTANT	(bool,		PS3_VertexShaderLighting,			0,	0,	0,	0,	false) //it's a RF really,	but it's not permutable because we ran out of features,	so we use it as a constant
	CONSTANT	(int,		PS3_BasicBlendFunc,					0,	0,	0,	0,	0)
	CONSTANT	(bool,		PS3_RF_DuDvShift1,					0,	0,	1,	1,	false)
	CONSTANT	(bool,		PS3_RF_DuDvShift2,					0,	0,	1,	1,	false)
	CONSTANT	(bool,		PS3_DoToneMap,						0,	0,	0,	0,	true)   // Enables tone-mapping - currently off for UI.
	CONSTANT	(bool,		PS3_RF_VertexNearFade,				0,	0,	1,	1,	false)
	CONSTANT	(float,		PS3_VertexNearFadeDistance,			0,	0,	0,	0,	0)
	CONSTANT	(float,		nm_scaler0,							0,	0,	0,	0,	1)
	CONSTANT	(float,		nm_scaler1,							0,	0,	0,	0,	1)
	CONSTANT	(float,		nm_scaler2,							0,	0,	0,	0,	1)
	CONSTANT	(float,		nm_scaler3,							0,	0,	0,	0,	1)
	CONSTANT	(float,		nm_scaler4,							0,	0,	0,	0,	1)
	CONSTANT	(float,		nm_scaler5,							0,	0,	0,	0,	1)
	CONSTANT	(float,		nm_scaler6,							0,	0,	0,	0,	1)
	CONSTANT	(float,		nm_scaler7,							0,	0,	0,	0,	1)
	CONSTANT	(bool,		PS3_Rain,							0,	0,	1,	1,	false)
	CONSTANT	(float,		PS3_DuDvAdd,						0,	0,	0,	0,	0)
	CONSTANT	(bool,		PS3_RF_Reflection_Map,				0,	0,	1,	1,	false)
	CONSTANT	(bool,		PS3_RF_Refraction,					0,	0,	1,	1,	false)
	CONSTANT	(bool,		PS3_RF_VolumetricShape,				0,	0,	0,	0,	false)
	CONSTANT	(float,		PS3_DistortionScale,				0,	0,	0,	0,	1)
	CONSTANT	(bool,		PS3_Transparency_EANIMATE,			0,	0,	0,	0,	false)
	CONSTANT	(bool,		PS3_RF_UIUseAlphaMask,				0,	0,	0,	0,	false)
	CONSTANT	(bool,		PS3_RF_EmissiveMap,					0,	0,	1,	0,	false) //CONSTANT	(bool,	PS3_RF_EmissiveMap) but removed from the default mask
	CONSTANT	(float,		PS3_VolumetricShapeDensity,			0,	0,	0,	0,	0)
	CONSTANT	(float,		PS3_RefractionAmount,				0,	0,	0,	0,	0)
	CONSTANT	(bool,		PS3_UseTransparencyForRefraction,	0,	0,	0,	0,	true)

	//Terrain alpha mask
	CONSTANT	(bool,		PS3_RF_TerrainHeightmapAlphaCut,	0,	0,	1,	1,	false)
	CONSTANT	(int,		PS3_TerrainAlphaFunction,			0,	0,	0,	0,	0)	//uint32
	CONSTANT	(float,		PS3_TerrainAlphaFunctionTransition,	0,	0,	0,	0,	1)
	CONSTANT	(float,		PS3_TerrainAlphaFunctionWidth,		0,	0,	0,	0,	50)
	CONSTANT	(float,		PS3_TerrainAlphaFunctionReference,	0,	0,	0,	0,	0)
	CONSTANT	(bool,		PS3_RF_TerrainHeightmapAlphaCut2,	0,	0,	1,	1,	false)
	CONSTANT	(int,		PS3_TerrainAlphaFunction2,			0,	0,	0,	0,	0)	//uint32
	CONSTANT	(float,		PS3_TerrainAlphaFunctionTransition2,0,	0,	0,	0,	1)
	CONSTANT	(float,		PS3_TerrainAlphaFunctionWidth2,		0,	0,	0,	0,	50)
	CONSTANT	(float,		PS3_TerrainAlphaFunctionReference2,	0,	0,	0,	0,	0)

	CONSTANT	(bool,		PS3_TerrainRefractionNoZWrite,		0,	0,	0,	0,	false)

	CONSTANT	(float,		PS3_RimLightIntensity,				0,	0,	0,	0,	0.5f)
};

uint64	UserConstant::default_mask;

void UserConstant::Init() {
	if (!default_mask) {
		for (int i = 0, f = 0; i < num_elements(user_constants); i++) {
			if (user_constants[i].feature) {
				user_constants[i].feature_index = f;
				default_mask |= uint64(user_constants[i].default_on) << f;
				f++;
			}
		}
	}
}

struct FeatureBits { uint64be bits; };

enum {
	k_sampler1D,
	k_sampler2D,
	k_sampler3D,
	k_samplerCUBE
};
enum {
	k_NO_TYPE = 0,
	k_DXT1,
	k_DXT3,
	k_DXT5,
	k_DXT_NORMAL,
	k_BGRA8888,
	k_BGRA5551,
	k_BGRA4444,
	k_BGR565,
	k_ALPHA8,
	k_ALPHA_INTENSITY88,
	k_DXT5_LIGHTMAP,
	k_BGRA_LIGHTMAP,
	k_ALPHA_16,
	k_MAX_NUM_DDS_DATA_TYPE,
	k_DDS_DEFAULT = k_DXT1
};
struct TexRequirement {
	uint16		type;					// Type eTextureType. Corresponds to the order of appearance in th texture definition file
	uint16		dimensionality;			// Type eTextureDimensionality
	uint16		compress_format;
	bool		is_srgb;				// Is the texture in sRGB space?
	bool		layered;				// Tells if the texture can be layered. For such textures, the code must be specifically generated 
	bool		engine_texture;
	char		name[32];
	char		shader_func[32];
	char		short_name[4];
};
#define SAMPLER(type,name,func_name,short_name,compression,sRGB,layered,engineTexture) { 0, k_##type, k_##compression, sRGB, layered, engineTexture, #name, #func_name , #short_name },
TexRequirement samplers[] = {
	SAMPLER(sampler2D,		DiffuseMapSampler,		ApplyDiffuseMaps,		_DF,	DXT5, 1, 1, 0)
	SAMPLER(sampler2D,		AlphaMapSampler,		ApplyAlphaMaps,			_TM,	DXT5, 0, 0, 0)
	SAMPLER(sampler2D,		DiffuseMapSampler_0_,	SampleDiffuseMap0,		_DX,	DXT5, 1, 0, 0)
	SAMPLER(sampler2D,		DiffuseMapSampler_1_,	SampleDiffuseMap1,		_DY,	DXT5, 1, 0, 0)
	SAMPLER(sampler2D,		DiffuseMapSampler_2_,	SampleDiffuseMap2,		_DZ,	DXT5, 1, 0, 0)
	SAMPLER(sampler2D,		NormalMapSampler,		ApplyNormalMaps,		_NM,	DXT_NORMAL, 0, 1, 0) //keep in sync with x3dscene_ps3.cpp to get the tangent space generated for the correct UV set
	SAMPLER(sampler2D,		NormalMapSampler_dummy, ApplyBlendedNormalMaps, _NB,	DXT_NORMAL, 0, 1, 0) //2 sampler defs same name
	SAMPLER(sampler2D,		NormalSampler_0_,		SampleNormalMap0,		_NO,	DXT_NORMAL, 0, 0, 0) //keep in sync with x3dscene_ps3.cpp to get the tangent space generated for the correct UV set
	SAMPLER(sampler2D,		NormalSampler_1_,		SampleNormalMap1,		_NY,	DXT_NORMAL, 0, 0, 0) //keep in sync with x3dscene_ps3.cpp to get the tangent space generated for the correct UV set
	SAMPLER(sampler2D,		GlossMapSampler,		SampleGlossMap0,		_GX,	DXT5, 0, 0, 0)
	SAMPLER(sampler2D,		GlossMapSampler_1_,		SampleGlossMap1,		_GY,	DXT5, 0, 0, 0)
	SAMPLER(sampler2D,		AmbOcMapSampler,		ApplyAmbientOcclusions, _OM,	DXT5, 1, 0, 0)
	SAMPLER(sampler2D,		AmbOcMapSampler_1_,		SampleAmbOcMap1,		_OY,	DXT5, 1, 0, 0)
	SAMPLER(sampler2D,		HeightMapSampler,		ApplyParallaxShift,		_HM,	DXT5, 0, 0, 0)
	SAMPLER(sampler2D,		AlphaMapSampler_0_,		SampleAlphaMap1,		_AX,	DXT5, 0, 0, 0)
	SAMPLER(sampler2D,		AlphaMapSampler_1_,		SampleAlphaMap2,		_AY,	DXT5, 0, 0, 0)
	SAMPLER(sampler2D,		LightMapSampler,		ApplyLightMaps,			_LM,	DXT5_LIGHTMAP, 0, 0, 0)
	SAMPLER(sampler2D,		SpecularMapSampler,		ApplySpecularMap,		_SP,	DXT5, 1, 0, 0)
	SAMPLER(sampler2D,		SpecularMapSampler_0_,	SampleSpecularMap0,		_SX,	DXT5, 1, 0, 0)
	SAMPLER(sampler2D,		SpecularMapSampler_1_,	SampleSpecularMap1,		_SY,	DXT5, 1, 0, 0)
	SAMPLER(sampler2D,		UserSampler0,			ApplyWrapMaskMaps,		_WA,	DXT5, 0, 0, 0)
	SAMPLER(sampler2D,		DuDvMapSampler,			ApplyDuDvShift,			_TS,	DXT5, 0, 0, 0)
	SAMPLER(samplerCUBE,	EnvMapSampler,			,						_EM,	DXT5, 1, 0, 0)
	SAMPLER(samplerCUBE,	SpatialLightingSampler, ,						,		DXT5, 0, 0, 1)
	SAMPLER(sampler2D,		EffectSampler0,			,						,		DXT5, 0, 0, 0)
	SAMPLER(sampler3D,		ColorLatticeSampler0,	,						,		DXT5, 0, 0, 1)
	SAMPLER(sampler3D,		ColorLatticeSampler1,	,						,		DXT5, 0, 0, 1)
	SAMPLER(sampler2D,		DynamicShadow0,			,						,		DXT5, 0, 0, 1)
	SAMPLER(sampler2D,		DynamicShadow1,			,						,		DXT5, 0, 0, 1)
	SAMPLER(sampler2D,		DynamicShadow2,			,						,		DXT5, 0, 0, 1)
	SAMPLER(sampler2D,		DynamicShadow3,			,						,		DXT5, 0, 0, 1)
	SAMPLER(sampler2D,		GenericSampler0,		,						,		DXT5, 0, 0, 0)
	SAMPLER(sampler2D,		GenericSampler1,		,						,		DXT5, 0, 0, 0)
	SAMPLER(sampler2D,		GenericSampler2,		,						,		DXT5, 0, 0, 0)
	SAMPLER(sampler2D,		GenericSampler3,		,						,		DXT5, 0, 0, 0)
	SAMPLER(sampler2D,		GenericSampler4,		,						,		DXT5, 0, 0, 0)
	SAMPLER(sampler2D,		GenericSampler5,		,						,		DXT5, 0, 0, 0)
	SAMPLER(sampler2D,		GenericSampler6,		,						,		DXT5, 0, 0, 0)
	SAMPLER(sampler2D,		WhiteBuffer,			,						,		DXT5, 0, 0, 1)
	SAMPLER(sampler2D,		WhiteBufferZ,			,						,		DXT5, 0, 0, 1)
	SAMPLER(sampler2D,		A2CSampler,				,						,		DXT5, 0, 0, 1)
	SAMPLER(sampler2D,		FogDepthRemap,			,						,		ALPHA8, 0, 0, 0)
	SAMPLER(sampler3D,		Generic3DSampler0,		,						,		DXT5, 0, 0, 1)
	SAMPLER(sampler2D,		GenericNormalMapSampler0,,						,		DXT_NORMAL, 0, 0, 0)
	SAMPLER(sampler2D,		DepthBuffer,			,						,		DXT5, 0, 0, 1)
	SAMPLER(sampler3D,		FogTextureSampler,		,						,		DXT5, 0, 0, 1)
	SAMPLER(sampler2D,		ReflectionMapSampler,	,						,		DXT5, 0, 0, 1)
	SAMPLER(sampler2D,		RefractionMapSampler,	,						,		DXT5, 0, 0, 1)
	SAMPLER(sampler2D,		EmissiveMapSampler,		ApplyEmissiveMap,		_ES,	DXT5, 1, 0, 0)
};

struct IceVertexProgram : bigendian {
	uint16	instructionCount;
	uint16	instructionSlot;
	uint32	vertexAttributeMask;
	uint32	vertexResultMask;
	uint32	vertexLimits;
	off_ptr16_be<uint16>		patches;
	off_ptr16_be<uint128be>		microcode;
	uint16	patchCount;
	uint16	constantCount;
	uint32	index[1];
			
	const float *GetConstantTable() const {
		iso::uint32	offset = (sizeof(*this) + 11 + constantCount * 4) & ~15;
		return (const float*)((const char*)this + offset);
	}
	_range<float32*>	get_constants() const {
		iso::uint32	offset = (sizeof(*this) + 11 + constantCount * 4) & ~15;
		return range_n((float32*)((const char*)this + offset), constantCount);
	}
	PS3VertexShader /*array_unspec<uint128be>*/	get_microcode() const {
		return range_n(microcode.get(this), instructionCount);
	}
	_range<uint16*>		get_patches() const {
		return range_n(patches.get(this), patchCount);
	}
};

struct PatchData : bigendian {
	uint16 count;
	uint16 index[0];
};

struct IceFragmentProgram : bigendian {
	uint32	microcodeSize;
	uint16	centroidMask;
	uint16	texcoordMask;
	uint32	control;
	uint32	offsetAndContext;
	off_ptr32_be<uint128be>	microcode;
	uint32					patchCount;
	off_ptr32_be<PatchData>	patches[1];
			
	PS3PixelShader /*array_unspec<uint128be>*/	get_microcode() const {
		return range_n(microcode.get(this), microcodeSize / 16);
	}
	_range<offset_iterator<PatchData, uint32> >	get_patches() const {
		return range_n(offset_iterator<PatchData, uint32>(this, patches), patchCount);
	}
};

struct MaterialFlags : bigendian {
	uint64	flags;
	uint64	mask;
};

class UVTransform : bigendian {
public:
	enum {
		kOffsetU		= 1 << 0,
		kOffsetV		= 1 << 1,
		kRotateUV		= 1 << 2,
		kScaleU			= 1 << 3,
		kScaleV			= 1 << 4,
		kFlipbook		= 1 << 5,
		kNumChannels	= 6,
	};
	float32		animoffsetu;
	float32		animoffsetv;
	float32		animrotateuv;
	float32		scaleu; 
	float32		scalev;
	float32		flipbook_page;
};

struct MaterialStageLoadParmPS3 : bigendian {
	struct TexInfo {
		uint8		type;			//Sampler type. This indicates the sampler type derived the texture definition file
		uint8		index;			// This is the logical texture unit or stage index
		uint16		filter_wrap;
		UVTransform	uv_transform;
		float32		lod_bias;
		float32		aniso;
		uint8		user_mat_index;
		uint8		page_index;
		uint8		num_pages;
		uint8		padto64;
		float32		nm_scaler;
		uint32		more_pad;
		dynastring	name;
	};

	// Stage Flags
	enum {
		kAnimUV			= 0x1,
		kAnimUVCoordsMask = kAnimUV,
	};
	enum {
		kMaxNumTexturesPerStage = 16
	};

	MaterialFlags	mat_flags;				// (2*U64) The type of texture we are drawing for this stage 
	uint8			num_uv_transforms;		// number of uv transforms
	uint8			num_flipbooks;			// # of textures with flipbook
	uint16			uv_transform_flags;		// which textures have a uv transform 
	uint32			flipbook_flags;			// which textures have flipbook
	uint8			num_tex_info;
	uint8			cutout_texture_index;	//index of cutout texture or 0xFF if none
	uint8			cutout_texture_channel;	//the cutout alpha can come from any of 4 possible channels - the high bit is used to signal that there are multiple alpha sources(no z-prepass)...
	uint8			dudv_textureindex;
	uint32			padding;
	uint32			flipbook_sizes;
	uint32			rendering_requirements_id;
	dynastring		name;
	float32			layer_blenders[8];		//fixed number of blenders. This is actually the max
	TexInfo			tex_info[];

	MaterialStageLoadParmPS3*	next() const {
		return (MaterialStageLoadParmPS3*)&tex_info[num_tex_info];
	}
	_range<const TexInfo*>	get_texinfos() const {
		return range_n(tex_info, num_tex_info);
	}
};

union DependentConstantInfo {
	uint32		u;
	float32		f;
};

struct TechniqueFeatureInfo : bigendian {
	uint16		UserVarIndex;
	uint16		NumChildrenFeatures;
	uint32		Pad;
	uint64		ReferencedConstantVars;
};

struct ShaderTechniqueInfo : bigendian {
	uint64					relevant_features_mask;
	uint16					linear_feature_var_indices[64];
	DependentConstantInfo	feature_constants[64];
	rel_ptr_be<TechniqueFeatureInfo>	technique_features;
};

struct ShaderInfo : bigendian {
	uint32				num_techniques;
	rel_ptr_be<ShaderTechniqueInfo>	technique_info;

	_range<const ShaderTechniqueInfo*>	get_techniques() const {
		const ShaderTechniqueInfo *p = (ShaderTechniqueInfo*)(this + 1);
		return range_n(p, num_techniques);
	}
};

struct MaterialShaderInfoParm : MultiClient, ShaderInfo {
};

struct MaterialLoadParmPS3 : MultiClient {
	enum {
		kCastOverride_NoOverride = 0, 
		kCastOverride_Proxy, 
		kCastOverride_NoCast, 
	};
	
	enum {// _Flags bits: 8-bits because the copy in the material is only 8 bits!!
		kAnimateOnConnect				= 0x00000001,
		kIsTransparent					= 0x00000002,
		kIsRefractive					= 0x00000004,	
		kIsTerrain						= 0x00000008,
		kDontPause						= 0x00000010,

		//note that the tools don't set this (yet), so this material fills this in at material load time, but we could(should) move this back to the tools...
		kVertexAlphaSet					= 0x00000020,

		kIsText							= 0x00000040,
		kCasterOverrideFlagBitPos		= 7,
		kCasterOverrideFlag0			= 0x00000080, 
		kCasterOverrideFlag1			= 0x00000100, 
		kCasterOverrideFlag2			= 0x00000200, 
		kBelongsToSwapChain				= 0x00000400,
		kUseUIAlphaMask					= 0x00000800,
		kDontLoadRR						= 0x00001000,
		kRefractiveTerrainUseHeightmap	= 0x00002000,
		kRefractiveTerrainNoZWrite		= 0x00004000,
		kUI								= 0x00008000,
	};
	uint32		override_shadow_bias;
	float32		override_shadow_biasscale;
	float32		override_shadow_biasoffset;
	float4p		ambient_diffuse_spec_coeff;
	float4p		emissive_color;
	uint32		blendmode_layer;
	float32		alpha;
	float32		specular;
	float32		specular_focus;
	uint32		flags;
	uint16		num_stages;
	uint16		user_constant_bytesize;
	uint16		uvanim_flags;
	uint16		intermaterial_sort;

	const MaterialStageLoadParmPS3	*first_stage()	const	{ return (const MaterialStageLoadParmPS3*)(this + 1); }
	next_iterator<MaterialStageLoadParmPS3>	begin()	const	{ return first_stage(); }
	next_iterator<MaterialStageLoadParmPS3>	end()	const	{ return (int)num_stages; }
	_range<next_iterator<MaterialStageLoadParmPS3> >	get_stages() const {
		return range(begin(), end());
	}
};


struct ICGParam : bigendian {
	enum {
		ICG_DATA_BINDING_UNUSED = 0, //must exist and must be 0...
		ICG_DATA_BINDING_SAMPLER, 
		ICG_DATA_BINDING_CONSTANT, 
		ICG_DATA_BINDING_ATTR, 
	};
	BITFIELDS6(uint32,
		bEngineVar,			1,
		uVarIndex,			11,
		uParamType,			2,
		uOffset,			9,
		uVarArrayElement,	7,
		uVarParamWidth,		2
	);
};

struct ShaderFeatureDeclaration;
struct RenderingRequirements : bigendian {
	//on the PS3, these have no meaning(although I suppose we could stuff some PS3 specific things here if we wanted)...
	rel_ptr_be<uint8>		VertexProgram;
	rel_ptr_be<uint8>		FragmentProgram;

	uint32					shader_size_vp;
	uint32					shader_size_fp;
	uint32					shader_performance_fp;
	xint32be				material_texturing_technique_hash;
	FeatureBits				feature_bits;
	xint64be				non_feature_bits;
	xint32be				data_hash;
	uint32					technique_index;
	
	rel_ptr_be<uint8>		root;						// ShaderFeatureDeclaration
	
	uint16					num_literals;
	uint16					num_mesh_parameters;
	uint16					num_engine_parameters_vp;
	uint16					num_engine_parameters_fp;
	uint16					num_user_constants_vp;
	uint16					num_user_constants_fp;
	uint16					num_samplers_fp;
	uint16					padding;

	rel_ptr_be<uint16>		MeshParameterIndex;
	rel_ptr_be<uint16>		EngineParameterIndexVP;
	rel_ptr_be<uint16>		EngineParameterIndexFP;
	rel_ptr_be<uint16>		UserConstantIndexVP;
	rel_ptr_be<uint16>		UserConstantIndexFP;
	rel_ptr_be<uint16>		SamplerIndexFP;
	rel_ptr_be<uint16>		LiteralIndices;
	rel_ptr_be<ICGParam>	MeshParams;
	rel_ptr_be<ICGParam>	EngineParamsVP;
	rel_ptr_be<ICGParam>	EngineParamsFP;
	rel_ptr_be<ICGParam>	UserConstantParamsVP;
	rel_ptr_be<ICGParam>	UserConstantParamsFP;
	rel_ptr_be<ICGParam>	SamplerParamsFP;

	rel_ptr_be<float32>		literals;
	
	rel_ptr_be<char>					shader_name;
	rel_ptr_be<IceVertexProgram>		shader_vp;
	rel_ptr_be<IceFragmentProgram>		shader_fp;

	_range<uint16*>		get_MeshParameterIndex()		const { return range_n(MeshParameterIndex.get(),	num_mesh_parameters			); }
	_range<uint16*>		get_EngineParameterIndexVP()	const { return range_n(EngineParameterIndexVP.get(),num_engine_parameters_vp	); }
	_range<uint16*>		get_EngineParameterIndexFP()	const { return range_n(EngineParameterIndexFP.get(),num_engine_parameters_fp	); }
	_range<uint16*>		get_UserConstantIndexVP()		const { return range_n(UserConstantIndexVP.get(),	num_user_constants_vp		); }
	_range<uint16*>		get_UserConstantIndexFP()		const { return range_n(UserConstantIndexFP.get(),	num_user_constants_fp		); }
	_range<uint16*>		get_SamplerIndexFP()			const { return range_n(SamplerIndexFP.get(),		num_samplers_fp				); }
	_range<uint16*>		get_LiteralIndices()			const { return range_n(LiteralIndices.get(),		num_literals				); }
	_range<ICGParam*>	get_MeshParams()				const { return range_n(MeshParams.get(),			num_mesh_parameters			); }
	_range<ICGParam*>	get_EngineParamsVP()			const { return range_n(EngineParamsVP.get(), 		num_engine_parameters_vp	); }
	_range<ICGParam*>	get_EngineParamsFP()			const { return range_n(EngineParamsFP.get(), 		num_engine_parameters_fp	); }
	_range<ICGParam*>	get_UserConstantParamsVP()		const { return range_n(UserConstantParamsVP.get(), 	num_user_constants_vp		); }
	_range<ICGParam*>	get_UserConstantParamsFP()		const { return range_n(UserConstantParamsFP.get(), 	num_user_constants_fp		); }
	_range<ICGParam*>	get_SamplerParamsFP()			const { return range_n(SamplerParamsFP.get(), 		num_samplers_fp				); }

	iso::uint32			TotalSize() const {
		iso::uint32	params = num_mesh_parameters + num_engine_parameters_vp + num_engine_parameters_fp + num_user_constants_vp + num_user_constants_fp + num_samplers_fp;
		iso::uint32	total = align(sizeof(*this), 128) + (!shader_name ? 0 : strlen(shader_name)) + params * (sizeof(uint16) + sizeof(ICGParam)) + num_literals * sizeof(uint16);
		total		= align(align(total, 16) + shader_size_vp, 16) + shader_size_fp + num_literals * sizeof(float);
		return align(total, 128);
	}
};

struct RenderingRequirementsSet : bigendian {
	uint32		num_requirements;
	off_ptr32_be<off_ptr32_be<RenderingRequirements> >	requirements;
	const RenderingRequirements&	operator[](int i)	const	{ return *requirements.get(this)[i].get(this);	}

	iso::uint32	TotalSize() const {
		iso::uint32	total = align(sizeof(*this) + num_requirements * sizeof(off_ptr32_be<RenderingRequirements>), 128);
		for (int i = 0, n = num_requirements; i < n; i++)
			total += (*this)[i].TotalSize();
		return total;
	}
};

struct RenderingRequirmentsParm : Client {
	uint32	total_bytesize;
	uint32	num_parms;
	off_ptr32_be<RenderingRequirementsSet>	sets[1];

	typedef offset_iterator<RenderingRequirementsSet, uint32> iterator;
	_range<iterator>	get_sets() const { return range_n(iterator(this, sets), num_parms); }
};

struct MaterialUserConstants : bigendian {
	struct Info {
		uint16	index;		//this is an offset into the global list of user data element types...
		uint16	offset;		//this is an offset into a parallel structure indicating where this user data exists...
	};
	uint16	byte_size;
	uint16	num_infos;
	off_ptr16_be<Info>	info;
	off_ptr16_be<uint8>	data;

	const float *GetUserConstPtr(int which) const {
		const Info	*p = info.get(this);
		for(int i = num_infos; i--; p++) {
			if (p->index == which)
				return (float*)(data.get(this) + p->offset);
		}
		return NULL;
	}
};

struct MaterialDebugInfo : Client {
	struct Info {
		FeatureBits	features;
		xint64be	permutations;
		xint32be	cycles;
		xint32be	hash;
	};
	uint32		length;
	uint32		numRRs;
	dynastring	name;
	
	_range<const Info*>	get_infos() const {
		return range_n((const Info*)(this + 1), numRRs);
	}
};
} // namespace smc;

ISO_DEFUSER(PS3VertexShader, array_unspec<uint128be>);

ISO_DEFUSERENUMF(smc::FeatureBits, 64, 64, NONE) {
	ISO_enum *e = enums;
	for (int i = 0, f = 0; i < num_elements(smc::user_constants); i++) {
		if (smc::user_constants[i].feature) {
			enums[f].set(smc::user_constants[i].name, swap_endian(uint64(1) << f));
			f++;
		}
	}
}};

ISO_DEFUSERCOMP(smc::IceVertexProgram, 7) {
	ISO_SETFIELDS4(0, instructionSlot, vertexAttributeMask, vertexResultMask, vertexLimits);
	ISO_SETACCESSORX(4, get_microcode,	"microcode");
	ISO_SETACCESSORX(5, get_patches,	"patches");
	ISO_SETACCESSORX(6, get_constants,	"constants");
}};
 
template<> struct ISO_def<smc::PatchData> : TISO_virtual2<smc::PatchData> {
	static uint32		Count(smc::PatchData &a)		{ return a.count;	}
	static ISO_browser	Index(smc::PatchData &a, int i)	{ return MakeBrowser(a.index[i]);	}
};
ISO_DEFUSER(PS3PixelShader, array_unspec<uint128be>);

ISO_DEFUSERCOMP(smc::IceFragmentProgram, 6) {
	ISO_SETFIELDS4(0, centroidMask, texcoordMask, control, offsetAndContext);
	ISO_SETACCESSORX(4, get_microcode,	"microcode");
	ISO_SETACCESSORX(5, get_patches,	"patches");
}};

ISO_DEFUSERCOMP6(smc::ICGParam, bEngineVar, uVarIndex, uParamType, uOffset, uVarArrayElement, uVarParamWidth);

ISO_DEFUSERCOMP(smc::RenderingRequirements,25) {
	ISO_SETFIELDS8(0, shader_size_vp, shader_size_fp, shader_performance_fp, material_texturing_technique_hash, feature_bits, non_feature_bits, data_hash, technique_index);
	ISO_SETFIELDS4(8, root, shader_name, shader_vp, shader_fp);
	ISO_SETACCESSORX(12, get_MeshParameterIndex,		"MeshParameterIndex");
	ISO_SETACCESSORX(13, get_EngineParameterIndexVP,	"EngineParameterIndexVP");
	ISO_SETACCESSORX(14, get_EngineParameterIndexFP,	"EngineParameterIndexFP");
	ISO_SETACCESSORX(15, get_UserConstantIndexVP, 		"UserConstantIndexVP");
	ISO_SETACCESSORX(16, get_UserConstantIndexFP, 		"UserConstantIndexFP");
	ISO_SETACCESSORX(17, get_SamplerIndexFP, 			"SamplerIndexFP");
	ISO_SETACCESSORX(18, get_LiteralIndices, 			"LiteralIndices");
	ISO_SETACCESSORX(19, get_MeshParams, 				"MeshParams");
	ISO_SETACCESSORX(20, get_EngineParamsVP, 			"EngineParamsVP");
	ISO_SETACCESSORX(21, get_EngineParamsFP, 			"EngineParamsFP");
	ISO_SETACCESSORX(22, get_UserConstantParamsVP, 		"UserConstantParamsVP");
	ISO_SETACCESSORX(23, get_UserConstantParamsFP, 		"UserConstantParamsFP");
	ISO_SETACCESSORX(24, get_SamplerParamsFP, 			"SamplerParamsFP");
}};
template<> struct ISO_def<smc::RenderingRequirementsSet> : TISO_virtual2<smc::RenderingRequirementsSet> {
	static uint32		Count(smc::RenderingRequirementsSet &a)			{ return a.num_requirements;	}
	static ISO_browser	Index(smc::RenderingRequirementsSet &a, int i)	{ return MakeBrowser(a[i]);	}
};

ISO_DEFUSERCOMP(smc::DependentConstantInfo,2) {
	ISO_SETFIELDT(0, u, uint32be);
	ISO_SETFIELDT(1, f, float32be);
}};

ISO_DEFUSERCOMP3(smc::TechniqueFeatureInfo, UserVarIndex, NumChildrenFeatures, ReferencedConstantVars);
ISO_DEFUSERCOMP4(smc::ShaderTechniqueInfo, relevant_features_mask, linear_feature_var_indices, feature_constants, technique_features);
ISO_DEFUSERCOMP6(smc::UVTransform, animoffsetu, animoffsetv, animrotateuv, scaleu, scalev, flipbook_page);

ISO_DEFUSERCOMP(smc::MaterialStageLoadParmPS3::TexInfo,11) {
	ISO_SETFIELDS8(0, type, index, filter_wrap, uv_transform, lod_bias, aniso, user_mat_index, page_index);
	ISO_SETFIELDS3(8, num_pages, nm_scaler, name);
}};

ISO_DEFUSERCOMP(smc::MaterialStageLoadParmPS3,14) {
	ISO_SETFIELDS8(0, mat_flags, num_uv_transforms, num_flipbooks, uv_transform_flags, flipbook_flags, num_tex_info, cutout_texture_index, cutout_texture_channel);
	ISO_SETFIELDS5(8, dudv_textureindex, flipbook_sizes, rendering_requirements_id, name, layer_blenders);
	ISO_SETACCESSORX(13,get_texinfos,"texinfo");
}};

ISO_DEFUSERCOMP(smc::MaterialLoadParmPS3,13) {
	ISO_SETFIELDS8(0, override_shadow_bias,override_shadow_biasscale,override_shadow_biasoffset,ambient_diffuse_spec_coeff,emissive_color,blendmode_layer,alpha,specular);
	ISO_SETFIELDS4(8, specular_focus,flags,uvanim_flags,intermaterial_sort);
	ISO_SETACCESSORX(12,get_stages,"stages");
}};

ISO_DEFUSERCOMP4(smc::MaterialDebugInfo::Info, features, permutations, cycles, hash);
ISO_DEFUSERCOMP(smc::MaterialDebugInfo, 2) {
	ISO_SETFIELD(0, name);
	ISO_SETACCESSORX(1, get_infos, "infos");
}};

namespace smc {
enum {
	kMaterialParm = 0,
	kRenderingRequirementsParm, 
	kShaderInfoParm,
	kRenderingRequirementsDebugParm,
};

template<> ISO_ptr<void> WAD_Context::LoadClient<Client::MaterialServerID>(const Client *client, size_t size, bool &keep) {
	keep = true;
	switch (client->client()) {
		case kMaterialParm:
			return MakeVirtPtr("Material", (MaterialLoadParmPS3*)client);

		case kShaderInfoParm: {
			MaterialShaderInfoParm	*m = (MaterialShaderInfoParm*)client;
			return MakePtr("MaterialShaderInfo", m->get_techniques());
		}
		case kRenderingRequirementsParm: {
			RenderingRequirmentsParm	*m = (RenderingRequirmentsParm*)client;
			return MakePtr("RenderingRequirments", m->get_sets());
		}
		case kRenderingRequirementsDebugParm:
			return MakeVirtPtr("MaterialDebugInfo", (MaterialDebugInfo*)client);

		default:
			return ISO_NULL;
	}
}
} // namespace smc;

//-------------------------------------
// Model Client
//-------------------------------------

namespace smc {

struct CullSphere : bigendian {
	int16	id;
	int16	instance;
	float32	radius;
	float32	centre[3];
};

struct CullBoundSphere : bigendian {
	float32	sphere_rad[4];
};

struct CullBoundInfo : bigendian {
	uint16						num_bounds;
	uint16						pad;
	rel_ptr_be<uint16>			matrix_indices;
	rel_ptr_be<CullBoundSphere>	bound_spheres;
};

struct PartDetailSet : bigendian {
	enum {
		kMaxMaterialLayers	= 4,
		kHasCullSpheres		= 0x1,
		kSplit				= 0x2,
	};
	float32						max_z;
	uint16						num_prims;
	uint16						flags;
	uint32						cull_bounds_size;
	rel_ptr_be<CullBoundInfo>	cull_bounds;
	rel_ptr_be<rel_ptr_be<PrimParm> >	prims;
	rel_ptr_be<float4p>			cull_spheres;

	typedef rel_ptr_be<PrimParm>*	prim_iterator;
	_range<prim_iterator>			get_prims()		const { return range_n(prims.get(), num_prims); }
	_range<CullBoundInfo*>			get_bounds()	const { return range_n(cull_bounds.get(), cull_bounds_size); }
};

struct ModelPart : bigendian {
	enum {
		kSkinnedSheet	= 1,
		kOccluderQuads	= 2,
	};
	uint16	num_parents;
	uint8	num_detail_levels;
	uint8	type_flags;
	off_ptr32_be<PartDetailSet>	details[1];

	typedef offset_iterator<PartDetailSet, uint32>	iterator;
	iterator			begin()			const { return iterator(this, details); }
	iterator			end()			const { return iterator(this, details + num_detail_levels); }
	_range<iterator>	get_details()	const { return range(begin(), end()); }
	ISO_ptr<Model3>		get_model()		const;
};

struct ModelGroup : Client {
	enum {
		kIsHidden			= 0x0001,
		kVertexAnimGroup	= 0x0002,
		kIsMoveable			= 0x0004,
		kHasPartInstances	= 0x0008,
		kCullParts			= 0x0010,
		kDynamicCullSphere	= 0x0020,
		kCullPartsThresholdInMeters	= 6,
	};
	uint32			filesize;
	uint16			numparts;
	uint16			numcullspheres;
	uint16			groupflags;
	uint16			pad32;
	uint32			boundsinfo;
	uint16			numweights; 
	uint16			cullrootjoint;
	off_ptr32_be<ModelPart>	parts[1];

	_range<const CullSphere*> get_spheres()	const { return range_n((const CullSphere*)(parts + numparts), numcullspheres); }
	_range<const float32*>	get_weights()	const { return range_n((const float32*)((const CullSphere*)(parts + numparts) + numcullspheres), numweights); }

	typedef offset_iterator<ModelPart, uint32>	parts_iterator;
	parts_iterator			parts_begin()	const { return parts_iterator(this, parts); }
	parts_iterator			parts_end()		const { return parts_iterator(this, parts + numparts); }
	_range<parts_iterator>	get_parts()		const { return range(parts_begin(), parts_end()); }
};

struct ModelLoadParm : MultiClient {
	enum {
		MaxJoints					= 512,
		kIsAnimated					= 0x00000008,
		kIsSkin						= 0x00000010,
		kSetLayerID					= 0x00000020,
		kJointVisibilityControl		= 0x00000040,
		kInstanced					= 0x00000080,
		kIsMoveable					= 0x00000100,
		kNotToneMapped				= 0x00000200,
		kPrimsCastMask				= 0x00003C00,
		kPrimsCastShadowSelf		= 0x00000400,
		kPrimsCastShadowPrimary		= 0x00000800,
		kPrimsCastShadowSecondary	= 0x00001000,
		kPrimsCastShadowTertiary	= 0x00002000,
		kSkyBoxBackground			= 0x00004000,
		kSkyBoxForeground			= 0x00008000,
		kSkinnedSheet				= 0x00010000,
		kIsProxyCaster				= 0x00020000,
		kIsUIModel					= 0x00040000,
		kUseSecondaryLightContext	= 0x00080000,
		kSkyLayerBit0				= 0x00100000,
		kSkyLayerMask				= 0x00F00000,
		kDynamicallyLit				= 0x01000000,
		kOutOfAffinitySelfCast		= 0x02000000,
		kForceHighMipsLoaded		= 0x04000000,
		kUsesPixelSpatialLighting	= 0x10000000,
		kUsesVertexSpatialLighting	= 0x20000000,
		kSpecialHRCaster			= 0x40000000,
		kHasDegenerateTriangles		= 0x80000000,
	};
	float32				mipmap_const;
	float32				fadeoff_neardist;
	float32				fadeoff_fardist;
	uint32				num_materials;
	uint32				num_materialsets;
	uint32				num_materialcats;
	uint32				num_chunks;
	uint32				num_joints;
	uint32				model_flags;
	uint32				layer_id;
	MaterialFlags		obj_flags;
	float32				quant_offset[4];
	float32				quant_scale;
	uint32				dummy0;
	uint32				num_uvcoord_anim_buffers;
	float32				shadow_bias_scale;
	float32				shadow_bias_offset;
	float32				min_shadow_atten;
	float32				shadow_blur_width_max;
	float32				shadow_local_filter_scale;
	uint32				light_link_mask;
	uint32				shadow_link_mask;
};
} // namespace smc;

template<> struct ISO_def<smc::PrimParm> : TISO_virtual2<smc::PrimParm> {
	static ISO_browser2	Deref(smc::PrimParm &a)	{ return *a; }
};

ISO_DEFUSERCOMP4(smc::CullSphere,id,instance,radius,centre);
ISO_DEFUSERCOMP1(smc::CullBoundSphere,sphere_rad);
ISO_DEFUSERCOMP3(smc::CullBoundInfo,num_bounds,matrix_indices,bound_spheres);

ISO_DEFUSERCOMP(smc::PartDetailSet,4) {
	ISO_SETFIELDS2(0,max_z,flags);
	ISO_SETACCESSORX(2,get_prims,"prims");
	ISO_SETACCESSORX(3,get_bounds,"bounds");
}};

ISO_DEFUSERCOMP(smc::ModelPart,4) {
	ISO_SETFIELD(0,num_parents); 
	ISO_SETFIELD(1,type_flags);
	ISO_SETACCESSORX(2,get_details,"details");
	ISO_SETACCESSORX(3,get_model,"model");
}};

ISO_DEFUSERCOMP(smc::ModelGroup,7) {
	ISO_SETFIELD(0,filesize);
	ISO_SETFIELD(1,groupflags);
	ISO_SETFIELD(2,boundsinfo);
	ISO_SETFIELD(3,numweights); 
	ISO_SETFIELD(4,cullrootjoint);
	ISO_SETACCESSORX(5,get_spheres,"spheres");
	ISO_SETACCESSORX(6,get_parts,"parts");
}};

ISO_DEFUSERCOMP2(smc::MaterialFlags,flags,mask);

ISO_DEFUSERCOMP(smc::ModelLoadParm,21) {
	ISO_SETFIELDS8(0,mipmap_const,fadeoff_neardist,fadeoff_fardist,num_materials,num_materialsets,num_materialcats,num_chunks,num_joints);
	ISO_SETFIELDS8(8,model_flags,layer_id,obj_flags,quant_offset,quant_scale,num_uvcoord_anim_buffers,shadow_bias_scale,shadow_bias_offset);
	ISO_SETFIELDS5(16,min_shadow_atten,shadow_blur_width_max,shadow_local_filter_scale,light_link_mask,shadow_link_mask);
}};

hash_map<smc::ModelPart*,ISO_ptr<Model3> >	model_hash;

ISO_ptr<Model3> smc::ModelPart::get_model() const {
	ISO_ptr<Model3>	&model = model_hash[this];
	if (model)
		return model;

	ModelBuilder	mb("model", ISO_NULL);
	typedef			iso::uint32	uint32;
	typedef			iso::uint16	uint16;

	for (auto i = begin(), i1 = end(); i != i1; ++i) {
		auto	prims = i->get_prims();
		for (auto j = prims.begin(), j1 = prims.end(); j != j1; ++j) {
			ISO_browser2	b(***j);
			int				n = b.Count();
			for (int i = 0; i < n; i++) {
				Prim		*p	= b[i];
				auto		chunks = p->get_chunkedgedata();
				for (auto e = chunks.begin(), e1 = chunks.end(); e != e1; ++e) {
					auto	indices = e->get_spu_indices();
					auto	verts	= e->get_spu_vertices();

					ISO_browser2	bv(verts[0]);
					ISO_browser2	bi(indices[0]);
					uint32			nv	= bv.Count(), ni = bi.Count(), nt = ni / 3;
					ISO_browser2	v0	= bv[0];
					uint32			nc	= v0.Count();

					TISO_type_composite<64>	builder;
					for (int i = 0; i < nc; i++)
						builder.Add(v0[i].GetTypeDef(), v0.GetName(i));

					if (builder.GetIndex("normal") == -1)
						mb.SetTechnique(iso::root["data"]["simple"]["lite_bt"]);
					else
						mb.SetTechnique(iso::root["data"]["simple"]["lite"]);

					SubMesh			*sm	= mb.AddMesh(builder.Duplicate(), nv, nt);

					ISO_browser2	bd(sm->verts);
					for (int c = 0; c < nc; c++) {
						uint32	cs = bd[0][c].GetSize();
						for (int i = 0; i < nv; i++)
							memcpy(bd[i][c], bv[i][c], cs);
					}

					SubMesh::face	*fs = bi[0];
					for (SubMesh::face *fi = sm->indices, *fe = fi + nt; fi < fe; ++fi, ++fs) {
						(*fi)[0] = (*fs)[0];
						(*fi)[1] = (*fs)[2];
						(*fi)[2] = (*fs)[1];
					}
				}
			}
		}
	}
	mb.CalcExtents();
	model = mb;
	model.Header()->addref();
	return model;
}

namespace smc {

enum {
	kModelGroupID	= 1,
	kModelParmID	= 2,
};

template<> ISO_ptr<void> WAD_Context::LoadClient<Client::ModelServerID>(const Client *client, size_t size, bool &keep) {
	switch (client->client()) {
		case kModelGroupID: {
			keep = true;
			return MakeVirtPtr("ModelGroup", (ModelGroup*)client);
		}
		case kModelParmID: {
			keep = true;
			return MakeVirtPtr("ModelLoadParm", (ModelLoadParm*)client);
		}
		default:
			return ISO_NULL;
	}
}
} // namespace smc;

//-----------------------------------------------------------------------------
//	collision
//-----------------------------------------------------------------------------

namespace smc {
struct GameObject;
struct goClient : Client, slist {
	FLOAT GetTimeStep(INT32 iTimeZone = 0) const;
	GameObject*			go;
	const Client*		parm;
};

struct AtrEntry {
	enum Type {
		Integer,
		Float,
		Boolean,
		String,
		StrHash,
	};
	char name[32];
    union {
		struct { int32		min, max, defval;	} IntType;
		struct { float32	min, max, defval;	} FloatType;
		struct { int32		defval;				} BoolType;
		struct { dynastring name;				} StringType;
	};
	Type	type;
	INT32	offset;
	INT32	isBitField;
	INT32	bitPos;
    INT32	numBits;
};

enum CollisionSet {
	kCollisionSet		= 0,
	kSoundSet			= 1,
	kCameraSet			= 2,
	kEntitySet			= 3,
	kEffectSet			= 4,
	kVisibilitySet		= 5,
	kLightVolumeSet		= 6,
	kSkinnedCollisionSet = 7,
	kNumCollisionSets,
	kClothSet				
};

struct SkinnedCollisionVertex {
	float32	x, y, z;
	uint16	influences[4];
	uint8	weights[4];
};

struct SkinnedCollisionTriangle {
	int32	a, b, c;
	int32	material;
};

struct SkinnedCollisionGroupHeader {
	enum {
		kFlagDisabled = 1,
	};
	
	uint32	flags;
	uint32	triangleCount;
	uint32	vertexCount;
	uint32	flatVertexListOffset;
	
	rel_ptr_be<SkinnedCollisionTriangle>	triangles;	// these come in from disk as offsets, needs to be rebiased. (guaranteed to be readable in a multiple of 16 bytes, might not necessarily line up with the number of tris though)
	rel_ptr_be<SkinnedCollisionVertex>		vertices;	// these come in from disk as offsets, needs to be rebiased. (guaranteed to be readable in a multiple of 16 bytes)
	rel_ptr_be<const char>					name;		// these come in from disk as offsets, needs to be rebiased.
	uint32	padding;
	_range<SkinnedCollisionTriangle*>	get_triangles() const	{ return range_n(triangles.get(), triangleCount); }
	_range<SkinnedCollisionVertex*>		get_vertices() const	{ return range_n(vertices.get(), vertexCount); }
};

struct mCnvxHeader : public Client {
	uint32		totalSize;						// Size of this header and all data that follows.
	BE(CollisionSet) collisionSet;				// Collision, camera, or entity volume?
	uint32		nBalls;							// Balls in this client.
	uint32		nHulls;							// Hulls in this client.
	Vector		localBound;						// Model-space sphere over all animation.
	uint32		rootJoint;						// Joint to use for transforming bound.
	uint32		atrVersion;						// Version of ATR file used for properties.
	uint32		nPropMats;						// Property materials used.
	uint32		sizeofPropMat;					// Bytes (for attribute values) per prop material.
	uint32		nSkinnedGroups;					// Number of triangleGroups if this is skinned.
	uint32		nTotalSkinnedVerticesNeeded;	// How many vec4 do we need to allocate in game on the side?

	// Byte offsets from the beginning of this header:
	off_ptr32_be<uint8>						propMatData;		// Attribute data.
	off_ptr32_be<uint16>					ballJointID;		// Model joint id per vol.
	off_ptr32_be<uint8>						ballMatIndex;		// Prop material per boundary.
	off_ptr32_be<Vector>					ballBoundary;		// Balls in joint space.
	off_ptr32_be<uint8>						nBndsPerHull;		// Bounds per vol.
	off_ptr32_be<uint16>					hullJointID;		// Model joint id per vol.
	off_ptr32_be<uint8>						hullMatIndex;		// Prop material per boundary.
	off_ptr32_be<Vector>					hullBoundary;		// Hull boundaries in joint space.
	off_ptr32_be<SkinnedCollisionGroupHeader> skinnedGroupsTOC;	// Points at the offset where the table of contents for the skinned groups live.
	// Following this header comes data referred to above.
	_range<uint8*>							get_propMatData()	const { return range_n(propMatData.get(this), sizeofPropMat); }
	_range<uint16*>							get_ballJointID()	const { return range_n(ballJointID.get(this), nBalls); }
	_range<uint8*>							get_ballMatIndex()	const { return range_n(ballMatIndex.get(this), nBalls); }
	_range<Vector*>							get_ballBoundary()	const { return range_n(ballBoundary.get(this), nBalls); }
	_range<uint8*>							get_nBndsPerHull()	const { return range_n(nBndsPerHull.get(this), nHulls); }
	_range<uint16*>							get_hullJointID()	const { return range_n(hullJointID.get(this), nHulls); }
	_range<uint8*>							get_hullMatIndex()	const { return range_n(hullMatIndex.get(this), nHulls); }
	_range<Vector*>							get_hullBoundary()	const { return range_n(hullBoundary.get(this), nHulls); }
	_range<SkinnedCollisionGroupHeader*>	get_skinnedGroups()	const { return range_n(skinnedGroupsTOC.get(this), nSkinnedGroups); }
};

struct KDHeader : public Client {
	typedef Vector3	KDVertex;
	struct KDPrim {
		uint32		type;
		Vector3	v[4];
		uint32		mat_index;
	};
	struct KDLeafPrim {
		BITFIELDS2(uint16,
			primType,	1,
			primArrayIndex, 15
		);
	};
	struct KDTriangle {
		BITFIELDS2(uint16,
		primSpecs, 4,
		propertyMaterialIndex, 12
		);
		uint16	vIndex0, vIndex1, vIndex2;
	};
	struct KDQuad {
		BITFIELDS2(uint16,
			primSpecs, 4,
			propertyMaterialIndex, 12
		);
		uint16 vIndex0, vIndex1, vIndex2, vIndex3;
	};
	struct KDLeafNode {
		BITFIELDS2(uint32,
			nodeType, 1,
			leafPrimArrayIndex, 24
		);
		uint32	numPrims;
	};
	struct KDNode {
		BITFIELDS2(uint16,
			nodeType, 1,
			splitType, 2
		);
		uint16	farIndex;
		float32	splitValue;
	};
	uint32		totalSize;				// Size of this header and all data that follows.
	uint32		atrVersion;				// Version of ATR file used for properties
	uint32		sheetVersion;			// Version number for the 'ribbon.exe' tool used
	AABB		bbox;					// encompassing bounding box for ribbon geometry
	uint16		height;					// height of KD-tree
	uint16		firstLeafDepth;			// depth at which first leaf is encountered
	uint16		numTotalNodes;			// total number of nodes in tree
	uint16		numLeaves;				// of which this many are leaves
	uint32		numTotalLeafPrims;		// total number of leafprims
	uint16		maxLeafPrims;			// maximum number of leafprims (in single leaf)
	uint16		numTotalTris;			// total number of triangles
	uint16		numTotalQuads;			// total number of quads
	uint16		numTotalVerts;			// total number of vertices
	uint16		sizeofPropMat;			// need size of propMat to step between entries
	uint16		sizeofPropMatEntry;		// need size of propMat to step between entries
	uint16		numTotalPropMats;		// total number of property materials
	uint16		numTotalAtrEntries;		// total number of AtrEntries
	uint16		numTotalOccluders;		// total number of occluder planes present
	uint16		unused;					// a dummy value so I can add a stat without necessarily having to up version number

	// Pointers to actual data
	off_ptr32_be<KDNode>			KDTree;			// KDTree (nodes and leaves)
	off_ptr32_be<uint8>				PropMatArray;	// property material array [*** KEEP THESE TOGETHER]
	off_ptr32_be<AtrEntry>			AtrEntryArray;	// AtrEntries for on-the-box editing [*** KEEP THESE TOGETHER]
	off_ptr32_be<KDLeafPrim>		LeafPrimArray;	// leafprim array
	off_ptr32_be<KDTriangle>		TriangleArray;	// triangle array
	off_ptr32_be<KDQuad>			QuadArray;		// quad array
	off_ptr32_be<KDVertex>			VertexArray;	// vertex array
	off_ptr32_be<void>				Occluders;		// occluder data
	// ... [data immediately follows this header]
};

struct mCnvxCollisionClientParm : MultiClient {
};
} // namespace smc;

ISO_DEFUSERENUM(smc::CollisionSet, 10) {
	ISO_SETENUMS8(0, smc::kCollisionSet, smc::kSoundSet, smc::kCameraSet, smc::kEntitySet, smc::kEffectSet, smc::kVisibilitySet, smc::kLightVolumeSet, smc::kSkinnedCollisionSet);
	ISO_SETENUMS2(8, smc::kNumCollisionSets, smc::kClothSet);
}};

ISO_DEFUSERCOMP5(smc::SkinnedCollisionVertex, x, y, z, influences, weights);
ISO_DEFUSERCOMP4(smc::SkinnedCollisionTriangle, a, b, c, material);

ISO_DEFUSERCOMP(smc::SkinnedCollisionGroupHeader, 5) {
	ISO_SETFIELDS3(0, flags, name, flatVertexListOffset);
	ISO_SETACCESSORX(3, get_triangles, "triangles");
	ISO_SETACCESSORX(4, get_vertices, "vertices");
}};

ISO_DEFUSERCOMP(smc::mCnvxHeader, 20) {
	ISO_SETFIELDS8(0, totalSize, collisionSet, nBalls, nHulls, localBound, rootJoint, atrVersion, nPropMats);
	ISO_SETFIELDS3(8, sizeofPropMat, nSkinnedGroups, nTotalSkinnedVerticesNeeded);
	ISO_SETACCESSORX(11, get_propMatData,	"propMatData");
	ISO_SETACCESSORX(12, get_ballJointID,	"ballJointID");
	ISO_SETACCESSORX(13, get_ballMatIndex,	"ballMatIndex");
	ISO_SETACCESSORX(14, get_ballBoundary,	"ballBoundary");
	ISO_SETACCESSORX(15, get_nBndsPerHull,	"nBndsPerHull");
	ISO_SETACCESSORX(16, get_hullJointID,	"hullJointID");
	ISO_SETACCESSORX(17, get_hullMatIndex,	"hullMatIndex");
	ISO_SETACCESSORX(18, get_hullBoundary,	"hullBoundary");
	ISO_SETACCESSORX(19, get_skinnedGroups,	"skinnedGroups");
}};

namespace smc {
enum CollisionClientType {
	kCollisionInvalid	= -1,
	kCollisionSheet		= 0,
	kCollisionHulls		= 1,
	kCollisionDebug		= 2,
	kCollisionSkinned	= 3,
	kNumCollisionClientTypes
};

template<> ISO_ptr<void> WAD_Context::LoadClient<Client::CollisionServerID>(const Client *client, size_t size, bool &keep) {
//	static mCnvxCollisionClientParm*	lastMCnvxParm	= NULL;
//	static skinnedCollisionClientParm*	pSkinnedColParm	= NULL;
	switch (client->client()) {
		case kCollisionSkinned:
			break;

		case kCollisionHulls:
			return MakeVirtPtr("mCnvxHeader", (mCnvxHeader*)client);

		case kCollisionDebug:
			break;

		case kCollisionSheet: {
			KDHeader *sheetHdr = (KDHeader*)client;
			break;
		}
	}
	return ISO_NULL;
}

} // namespace smc;

//-----------------------------------------------------------------------------
//	common
//-----------------------------------------------------------------------------

typedef	ISO_openarray<uint8>	raw;

struct DYNASTRING				: xint32		{ DYNASTRING(uint32 v) : xint32(v) {} };
struct CLIENTPARM				: ISO_ptr<void>	{ CLIENTPARM(const ISO_ptr<void> &v) : ISO_ptr<void>(v) {} };
struct GROUPSTART				: anything		{};
struct GROUPEND					{};
struct ACTIVATE					: raw	{};
struct PUSH_CONTEXT				: raw	{};
struct POP_CONTEXT				: raw	{};
struct DATA_BLOCK				: raw	{};
struct DATA_BLOCK_W_LENGTH		: raw	{};
struct DATA_BLOCK_ALIGN16		: raw	{};
struct VFS_TWEAK_FILE			: raw	{};
struct DC_VERSION				: raw	{};
struct DC_DATA					: raw	{};
struct DC_EXPORTTABLE			: raw	{};
struct DC_IMPORTTABLE			: raw	{};
struct DC_DBG_STRINGTABLE		: raw	{};
struct DC_DBG_SYMBOLTABLE		: raw	{};
struct RESOURCE_NAMES			: raw	{};
struct ACTIVATE_WAD_CONTEXT		: raw	{};
struct POP_WAD_CONTEXT			: raw	{};
struct PUSH_HEAP {
	uint32				size, vram_size;
	ISO_ptr<anything>	children;
	PUSH_HEAP(smc::HeapInfo *info) : size(info->size), vram_size(info->vram_size), children(0) {}
	operator smc::HeapInfo() const { smc::HeapInfo h; h.size = size; h.vram_size = vram_size; return h; }
};
struct POP_HEAP					{};
struct PREFETCH_NAMES			: raw	{};
struct PREFETCH_GRAPH			: raw	{};
struct PADDING					: raw	{};
struct PUSH_DEBUG				: anything	{};
struct POP_DEBUG				{};
struct DEBUG_FRAGMENT_SHADERS	: raw	{};
struct TEXEL_CHUNK				: raw	{};

ISO_DEFUSER(DYNASTRING,				xint32				);
ISO_DEFUSER(CLIENTPARM,				ISO_ptr<void>		);
ISO_DEFUSER(GROUPSTART,				anything			);
ISO_DEFUSER(GROUPEND,				void				);
ISO_DEFUSER(ACTIVATE,				raw);
ISO_DEFUSER(PUSH_CONTEXT,			raw);
ISO_DEFUSER(POP_CONTEXT,			raw);
ISO_DEFUSER(DATA_BLOCK,				raw);
ISO_DEFUSER(DATA_BLOCK_W_LENGTH,	raw);
ISO_DEFUSER(DATA_BLOCK_ALIGN16,		raw);
ISO_DEFUSER(VFS_TWEAK_FILE,			raw);
ISO_DEFUSER(DC_VERSION,				raw);
ISO_DEFUSER(DC_DATA,				raw);
ISO_DEFUSER(DC_EXPORTTABLE,			raw);
ISO_DEFUSER(DC_IMPORTTABLE,			raw);
ISO_DEFUSER(DC_DBG_STRINGTABLE,		raw);
ISO_DEFUSER(DC_DBG_SYMBOLTABLE,		raw);
ISO_DEFUSER(RESOURCE_NAMES,			raw);
ISO_DEFUSER(ACTIVATE_WAD_CONTEXT,	raw);
ISO_DEFUSER(POP_WAD_CONTEXT,		raw);
ISO_DEFUSERCOMP3(PUSH_HEAP,			size, vram_size, children);
ISO_DEFUSER(POP_HEAP,				void				);
ISO_DEFUSER(PREFETCH_NAMES,			raw);
ISO_DEFUSER(PREFETCH_GRAPH,			raw);
ISO_DEFUSER(PADDING,				raw);
ISO_DEFUSER(PUSH_DEBUG,				anything			);
ISO_DEFUSER(POP_DEBUG,				void				);
ISO_DEFUSER(DEBUG_FRAGMENT_SHADERS,	raw);
ISO_DEFUSER(TEXEL_CHUNK,			raw);

const ISO_type *HEADER_TYPES[] = {
	ISO_getdef<DYNASTRING>(),
	ISO_getdef<CLIENTPARM>(),
	ISO_getdef<GROUPSTART>(),
	ISO_getdef<GROUPEND>(),
	ISO_getdef<ACTIVATE>(),
	ISO_getdef<PUSH_CONTEXT>(),
	ISO_getdef<POP_CONTEXT>(),
	ISO_getdef<DATA_BLOCK>(),
	ISO_getdef<DATA_BLOCK_W_LENGTH>(),
	ISO_getdef<DATA_BLOCK_ALIGN16>(),
	ISO_getdef<VFS_TWEAK_FILE>(),
	ISO_getdef<DC_VERSION>(),
	ISO_getdef<DC_DATA>(),
	ISO_getdef<DC_EXPORTTABLE>(),
	ISO_getdef<DC_IMPORTTABLE>(),
	ISO_getdef<DC_DBG_STRINGTABLE>(),
	ISO_getdef<DC_DBG_SYMBOLTABLE>(),
	ISO_getdef<RESOURCE_NAMES>(),
	ISO_getdef<ACTIVATE_WAD_CONTEXT>(),
	ISO_getdef<POP_WAD_CONTEXT>(),
	ISO_getdef<PUSH_HEAP>(),
	ISO_getdef<POP_HEAP>(),
	ISO_getdef<PREFETCH_NAMES>(),
	ISO_getdef<PREFETCH_GRAPH>(),
	ISO_getdef<PADDING>(),
	ISO_getdef<PUSH_DEBUG>(),
	ISO_getdef<POP_DEBUG>(),
	ISO_getdef<DEBUG_FRAGMENT_SHADERS>(),
	ISO_getdef<TEXEL_CHUNK>(),
};
namespace smc {

template<typename T> ISO_ptr<void> WAD_Loader::LoadRaw(tag id, const void *data, size_t size) {
	ISO_ptr<T>	p(id);
	if (size)
		memcpy(p->Create(size), data, size);
	return p;
}

ISO_ptr<void> WAD_Loader::LoadClientRaw(const Client *client, size_t size) {
	return LoadRaw<raw>(0, client, size);
//	ISO_ptr<CLIENTPARM>	b(0);
//	memcpy(b->Create(size), client, size);
//	return b;
//	ISO_ptr<ISO_openarray<uint8> >	block(CLIENT_STRINGS[client->server()]);
//	memcpy(block->Create(uint32(size)), client, size);
//	return ISO_ptr<ISO_ptr<ISO_openarray<uint8> > >(0, block);
}

ISO_ptr<void> WAD_Loader::LoadClient(const Client *client, size_t size, bool &keep) {
	keep = false;
	if (client->is_context())
		return LoadClientRaw(client, size);

	switch (client->server()) {
		case Client::RootServerID:				return LoadClient<Client::RootServerID			>(client, size, keep);
		case Client::GameObjectServerID:		return LoadClient<Client::GameObjectServerID	>(client, size, keep);
		case Client::MasterPrimServerID:		return LoadClient<Client::MasterPrimServerID	>(client, size, keep);
		case Client::AnimationServerID:			return LoadClient<Client::AnimationServerID		>(client, size, keep);
		case Client::ScriptServerID:			return LoadClient<Client::ScriptServerID		>(client, size, keep);
		case Client::LightServerID:				return LoadClient<Client::LightServerID			>(client, size, keep);
		case Client::TextureServerID:			return LoadClient<Client::TextureServerID		>(client, size, keep);
		case Client::MaterialServerID:			return LoadClient<Client::MaterialServerID		>(client, size, keep);
		case Client::CameraServerID:			return LoadClient<Client::CameraServerID		>(client, size, keep);
		case Client::MasterRenderServerID:		return LoadClient<Client::MasterRenderServerID	>(client, size, keep);
		case Client::ModelServerID:				return LoadClient<Client::ModelServerID			>(client, size, keep);
		case Client::CollisionServerID:			return LoadClient<Client::CollisionServerID		>(client, size, keep);
		case Client::ParticleServerID:			return LoadClient<Client::ParticleServerID		>(client, size, keep);
		case Client::WaypointServerID:			return LoadClient<Client::WaypointServerID		>(client, size, keep);
		case Client::EventServerID:				return LoadClient<Client::EventServerID			>(client, size, keep);
		case Client::BhvrServerID:				return LoadClient<Client::BhvrServerID			>(client, size, keep);
		case Client::SoundServerID:				return LoadClient<Client::SoundServerID			>(client, size, keep);
		case Client::WadServerID:				return LoadClient<Client::WadServerID			>(client, size, keep);
		case Client::EffectsServerID:			return LoadClient<Client::EffectsServerID		>(client, size, keep);
		case Client::ParticlePrimServerID:		return LoadClient<Client::ParticlePrimServerID	>(client, size, keep);
		case Client::PS3TriServerID:			return LoadClient<Client::PS3TriServerID		>(client, size, keep);
		case Client::OcclusionServerID:			return LoadClient<Client::OcclusionServerID		>(client, size, keep);
		case Client::NetObjectServerID:			return LoadClient<Client::NetObjectServerID		>(client, size, keep);
		case Client::LineServerID:				return LoadClient<Client::LineServerID			>(client, size, keep);
		default:								return ISO_NULL;
	}
}

ISO_ptr<void> WAD_Context::LoadClient(const Client *client, size_t size, bool &keep) {
	keep = false;
	if (client->is_context())
		return LoadClientRaw(client, size);

	switch (client->server()) {
		case Client::RootServerID:				return LoadClient<Client::RootServerID			>(client, size, keep);
		case Client::GameObjectServerID:		return LoadClient<Client::GameObjectServerID	>(client, size, keep);
		case Client::MasterPrimServerID:		return LoadClient<Client::MasterPrimServerID	>(client, size, keep);
		case Client::AnimationServerID:			return LoadClient<Client::AnimationServerID		>(client, size, keep);
		case Client::ScriptServerID:			return LoadClient<Client::ScriptServerID		>(client, size, keep);
		case Client::LightServerID:				return LoadClient<Client::LightServerID			>(client, size, keep);
		case Client::TextureServerID:			return LoadClient<Client::TextureServerID		>(client, size, keep);
		case Client::MaterialServerID:			return LoadClient<Client::MaterialServerID		>(client, size, keep);
		case Client::CameraServerID:			return LoadClient<Client::CameraServerID		>(client, size, keep);
		case Client::MasterRenderServerID:		return LoadClient<Client::MasterRenderServerID	>(client, size, keep);
		case Client::ModelServerID:				return LoadClient<Client::ModelServerID			>(client, size, keep);
		case Client::CollisionServerID:			return LoadClient<Client::CollisionServerID		>(client, size, keep);
		case Client::ParticleServerID:			return LoadClient<Client::ParticleServerID		>(client, size, keep);
		case Client::WaypointServerID:			return LoadClient<Client::WaypointServerID		>(client, size, keep);
		case Client::EventServerID:				return LoadClient<Client::EventServerID			>(client, size, keep);
		case Client::BhvrServerID:				return LoadClient<Client::BhvrServerID			>(client, size, keep);
		case Client::SoundServerID:				return LoadClient<Client::SoundServerID			>(client, size, keep);
		case Client::WadServerID:				return LoadClient<Client::WadServerID			>(client, size, keep);
		case Client::EffectsServerID:			return LoadClient<Client::EffectsServerID		>(client, size, keep);
		case Client::ParticlePrimServerID:		return LoadClient<Client::ParticlePrimServerID	>(client, size, keep);
		case Client::PS3TriServerID:			return LoadClient<Client::PS3TriServerID		>(client, size, keep);
		case Client::OcclusionServerID:			return LoadClient<Client::OcclusionServerID		>(client, size, keep);
		case Client::NetObjectServerID:			return LoadClient<Client::NetObjectServerID		>(client, size, keep);
		case Client::LineServerID:				return LoadClient<Client::LineServerID			>(client, size, keep);
		default:								return ISO_NULL;
	}
}

uint32 WAD_Context::Process(chunk &chunk, void *data, bool &keep) {
	keep = false;
	uint32	length	= chunk.length;

	switch (chunk.id) {
		case chunk::DYNASTRING:
			(*sp)->Append(ISO_ptr<DYNASTRING>(chunk.name, length));
			length = 0;
			break;

		case chunk::PUSH_HEAP: {
			ISO_ptr<PUSH_HEAP>	p(chunk.name, (HeapInfo*)data);
			(*sp++)->Append(p);
			*sp = p->children;
			break;
		}

		case chunk::PUSH_DEBUG: {
			ISO_ptr<PUSH_DEBUG>	p(chunk.name);
			(*sp++)->Append(p);
			*sp = p;
			break;
		}
		case chunk::GROUPSTART: {
			ISO_ptr<GROUPSTART>	p(chunk.name);
			(*sp++)->Append(p);
			*sp = p;
			break;
		}
		case chunk::POP_HEAP:
		case chunk::POP_DEBUG:
		case chunk::GROUPEND:
			--sp;
			break;

		case chunk::CLIENTPARM:
			if (length) {
				Client			*client	= (Client*)(&chunk + 1);
				bool			keep	= false;
				ISO_ptr<void>	p		= LoadClient(client, length, keep);
				if (!p)
					p = LoadClientRaw(client, length);
				(*sp)->Append(ISO_ptr<CLIENTPARM>(chunk.name, p));
			} else {
				(*sp)->Append(ISO_ptr<CLIENTPARM>(chunk.name, ISO_NULL));
			}
			break;

		case chunk::ACTIVATE:				(*sp)->Append(LoadRaw<ACTIVATE				>(chunk.name, data, length)); break;
		case chunk::PUSH_CONTEXT:			(*sp)->Append(LoadRaw<PUSH_CONTEXT			>(chunk.name, data, length)); break;
		case chunk::POP_CONTEXT:			(*sp)->Append(LoadRaw<POP_CONTEXT			>(chunk.name, data, length)); break;
		case chunk::DATA_BLOCK:				(*sp)->Append(LoadRaw<DATA_BLOCK			>(chunk.name, data, length)); break;
		case chunk::DATA_BLOCK_W_LENGTH:	(*sp)->Append(LoadRaw<DATA_BLOCK_W_LENGTH	>(chunk.name, data, length)); break;
		case chunk::DATA_BLOCK_ALIGN16:		(*sp)->Append(LoadRaw<DATA_BLOCK_ALIGN16	>(chunk.name, data, length)); break;
		case chunk::VFS_TWEAK_FILE:			(*sp)->Append(LoadRaw<VFS_TWEAK_FILE		>(chunk.name, data, length)); break;
		case chunk::DC_VERSION:				(*sp)->Append(LoadRaw<DC_VERSION			>(chunk.name, data, length)); break;
		case chunk::DC_DATA:				(*sp)->Append(LoadRaw<DC_DATA				>(chunk.name, data, length)); break;
		case chunk::DC_EXPORTTABLE:			(*sp)->Append(LoadRaw<DC_EXPORTTABLE		>(chunk.name, data, length)); break;
		case chunk::DC_IMPORTTABLE:			(*sp)->Append(LoadRaw<DC_IMPORTTABLE		>(chunk.name, data, length)); break;
		case chunk::DC_DBG_STRINGTABLE:		(*sp)->Append(LoadRaw<DC_DBG_STRINGTABLE	>(chunk.name, data, length)); break;
		case chunk::DC_DBG_SYMBOLTABLE:		(*sp)->Append(LoadRaw<DC_DBG_SYMBOLTABLE	>(chunk.name, data, length)); break;
		case chunk::RESOURCE_NAMES:			(*sp)->Append(LoadRaw<RESOURCE_NAMES		>(chunk.name, data, length)); break;
		case chunk::ACTIVATE_WAD_CONTEXT:	(*sp)->Append(LoadRaw<ACTIVATE_WAD_CONTEXT	>(chunk.name, data, length)); break;
		case chunk::POP_WAD_CONTEXT:		(*sp)->Append(LoadRaw<POP_WAD_CONTEXT		>(chunk.name, data, length)); break;
		case chunk::PREFETCH_NAMES:			(*sp)->Append(LoadRaw<PREFETCH_NAMES		>(chunk.name, data, length)); break;
		case chunk::PREFETCH_GRAPH:			(*sp)->Append(LoadRaw<PREFETCH_GRAPH		>(chunk.name, data, length)); break;
		case chunk::PADDING:				(*sp)->Append(LoadRaw<PADDING				>(chunk.name, data, length)); break;
		case chunk::DEBUG_FRAGMENT_SHADERS:	(*sp)->Append(LoadRaw<DEBUG_FRAGMENT_SHADERS>(chunk.name, data, length)); break;
		case chunk::TEXEL_CHUNK:			(*sp)->Append(LoadRaw<TEXEL_CHUNK			>(chunk.name, data, length)); break;
		default:							(*sp)->Append(LoadRaw<raw					>(chunk.name, data, length)); break;
	}
	return length;
}

struct chunk_writer : _chunk<false>, ostream_chain {
	typedef	_chunk<false>	chunk;
	streamptr	start;
	chunk_writer(ostream &_stream, int _id, const char *_name) : ostream_chain(_stream), start(stream.tell()) {
		id		= _id;
		version = 0;
		clear(name);
		strcpy(name, _name);
		stream.seek(sizeof(chunk), SEEK_CUR);
	}
	~chunk_writer() {
		streamptr	end = stream.tell();
		stream.seek(start);
		chunk::length = end - start - sizeof(chunk);
		stream.write(*(chunk*)this);
		stream.seek(end);
		stream.align(16, 0);
	}
};

bool WAD_Writer::Process(ostream &file, const ISO_browser2 &b) {
	const ISO_type	*type = b.GetTypeDef();
	const char		*name = tag(b.GetName());

	int		cid	= chunk::_INVALID;

	for (int i = 0; i < num_elements(HEADER_TYPES); i++) {
		if (HEADER_TYPES[i] == type) {
			cid = i;
			break;
		}
	}

	if (cid != chunk::_INVALID) {
		switch (cid) {
			case chunk::DYNASTRING: {
				chunk		chunk;
				strcpy(chunk.name, name);
				chunk.id		= cid;
				chunk.length	= b.GetInt();
				file.write(chunk);
				break;
			}
			case chunk::CLIENTPARM: {
				chunk_writer	chunk(file, cid, name);
				CLIENTPARM		*cp = b;
				if (*cp) {
					type = cp->Type();
					if (type->Is<raw>()) {
						raw	*r = *cp;
						file.writebuff(*r, r->Count());

					} else if (type->Is<bitmap>()) {
						TextureServerHeader	tsh;
						strcpy(tsh.name, tag(cp->ID()));
						file.write(tsh);
						return FileHandler::Get("dds")->Write(b, file);

					} else if (type->Is<_range<next_iterator<Prim> > >()) {
						TriParm	tp;
						tp.num_prims = b.Count();

					} else if (type->Is<pointer<MaterialLoadParmPS3> >()) {
					} else if (type->Is<_range<const ShaderTechniqueInfo*> >()) {
					} else if (type->Is<_range<RenderingRequirmentsParm::iterator> >()) {
						_range<RenderingRequirmentsParm::iterator>	*r = *cp;

						RenderingRequirmentsParm	parm;
						parm.total_bytesize	= 0;
						parm.num_parms		= r->size();

					} else if (type->Is<pointer<MaterialDebugInfo> >()) {
					} else if (type->Is<pointer<ModelGroup> >()) {
					} else if (type->Is<pointer<ModelLoadParm> >()) {
					} else if (type->Is<pointer<mCnvxHeader> >()) {
					} else {
						return false;
					}
				}
				break;
			}
			case chunk::PUSH_HEAP: {
				PUSH_HEAP	*ph = b;
				HeapInfo	hi	= *ph;
				chunk_writer(file, cid, name).write(hi);
				ProcessChildren(file, ph->children);
				chunk_writer(file, chunk::POP_HEAP, name);
				break;
			}

			case chunk::PUSH_DEBUG:
				chunk_writer(file, cid, name);
				ProcessChildren(file, b);
				chunk_writer(file, chunk::POP_DEBUG, name);
				break;

			case chunk::GROUPSTART: {
				chunk_writer(file, cid, name);
				ProcessChildren(file, b);
				chunk_writer(file, chunk::GROUPEND, name);
				break;
			}

			default: {
				raw	*data = b;
				chunk_writer(file, cid, name).writebuff(*data, data->Count());
				break;
			}
		}
		return true;
	}

	return true;
}

} // namespace smc;

//-----------------------------------------------------------------------------
//	FileHandler
//-----------------------------------------------------------------------------

class WAD_FileHandler : public FileHandler {
	const char*		GetExt() override { return "wad";		}
	const char*		GetDescription() override { return "Sony Santa Monica WAD";}
	ISO_ptr<void>	Read(tag id, istream &file) override;
	ISO_ptr<void>	ReadWithFilename(tag id, const filename &fn) override;
	bool			Write(ISO_ptr<void> p, ostream &file) override;
} smc_wad;

struct anything_map : anything {
	mapped_file		map;
	tag2			GetName(int i)	const			{ return (*this)[i].ID(); }
	ISO_browser2	Deref()			const			{ return ISO_browser2(); }
	bool			Update(const char *s, bool from){ return false; }
};
ISO_DEFVIRT(anything_map);

ISO_ptr<void> WAD_FileHandler::ReadWithFilename(tag id, const filename &fn) {
	ISO_ptr<anything_map>	result(id);
	result->map.open(fn);

	FileInput	ts(filename(fn).set_ext("ts"));

	smc::WAD_Context	context;
	context.stack[0]	= result;
	context.ts			= &ts;

	for (void *ptr = result->map, *end = result->map.end(); ptr < end; ) {
		smc::chunk&	chunk	= *(smc::chunk*)ptr;
		bool		keep	= false;
		uint32 length = context.Process(chunk, &chunk + 1, keep);
		if(chunk.id == smc::chunk::ACTIVATE_WAD_CONTEXT)
		{
			printf("Stuff\n");
		}

		{
			ptr = (char*)(&chunk + 1) + ((length + 15) & ~15);
		}
	}
	return result;
}

ISO_ptr<void> WAD_FileHandler::Read(tag id, istream &file) {
	smc::WAD_Context	context;
	context.stack[0].Create(id);

	smc::chunk	chunk;
	while (file.read(chunk)) {
		streamptr	ptr		= file.tell();
		uint32		length	= chunk.length;
		bool		keep	= false;

		malloc_block	data(length);
		file.readbuff(data, length);
		context.Process(chunk, data, keep);
		if (keep)
			data.start = 0;

		file.seek(ptr + (length + 15) & ~15);
	}
	return context.stack[0];
}

bool WAD_FileHandler::Write(ISO_ptr<void> p, ostream &file) {
	smc::WAD_Writer	writer;

	ISO_browser	b(p);
	for (int i = 0, n = b.Count(); i < n; ++i) {
		writer.Process(file, b[i]);
	}

	return false;
}