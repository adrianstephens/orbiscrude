#include "utilities.h"
#include "base/hash.h"
#include "comms/leb128.h"

#undef DIFFERENCE

#define DRACO_ENABLE_READER
#define DRACO_ENABLE_WRITER
#define DRACO_ENABLE_POINT_CLOUD
#define DRACO_ENABLE_POINT_CLOUD_KD
#define DRACO_ENABLE_MESH
#define DRACO_ENABLE_MESH_EDGEBREAKER

namespace draco {
using namespace iso;

// flags to control writing
enum MODE0 {
	POINT_CLOUD_SEQUENTIAL			= 0,
	POINT_CLOUD_KD					= 1,
	TRIANGULAR_MESH_SEQUENTIAL		= 2,
	TRIANGULAR_MESH_EDGEBREAKER		= 3,
};

enum MODE {
	MODE0_MASK				= 3,
	IS_TRIANGULAR_MESH		= 2,
	USE_SINGLE_CONNECTIVITY	= 1 << 2,
	COMPRESS_INTEGERS		= 1 << 3,
	COMPRESS_INDICES		= 1 << 4,
	USE_TRAV_MAXDEGREE		= 1 << 5,
	USE_PRED_NORMAL			= 1 << 6,
	USE_PRED_TEXCOORD		= 1 << 7,
	USE_PRED_PARALLELOGRAM	= 1 << 8,
	USE_PRED_MPARALLELOGRAM	= 1 << 9,
	_SYMBOL_BITS_ADJUST		= 7 << 10,
	_KD_COMPRESSION			= 7 << 13,
};
constexpr MODE	operator|(MODE0 a, MODE b)	{ return MODE((int)a | (int)b); }
constexpr MODE	operator|(MODE a, MODE b)	{ return MODE((int)a | (int)b); }
constexpr MODE	operator-(MODE a, MODE b)	{ return MODE(a & ~b); }
constexpr MODE	operator*(MODE a, bool b)	{ return b ? a : MODE(0); }
constexpr MODE	SYMBOL_BITS_ADJUST(int i)	{ return MODE((clamp(i, -4, 3) * lowest_set((int)_SYMBOL_BITS_ADJUST)) & _SYMBOL_BITS_ADJUST); }
constexpr int	SYMBOL_BITS_ADJUST(MODE m)	{ return mask_sign_extend<3>(m / lowest_set((int)_SYMBOL_BITS_ADJUST)); }
constexpr MODE	KD_COMPRESSION(int i)		{ return MODE(clamp(i, 0, 7) * lowest_set((int)_KD_COMPRESSION)); }
constexpr int	KD_COMPRESSION(MODE m)		{ return (m & _KD_COMPRESSION) / lowest_set((int)_KD_COMPRESSION); }

constexpr MODE DefaultMode(MODE0 mode, uint32 compression_level = 7) {
	return mode
	| SYMBOL_BITS_ADJUST(int(compression_level > 7) + int(compression_level > 9) - int(compression_level < 4) - int(compression_level < 6))
	| KD_COMPRESSION(compression_level)
	| COMPRESS_INTEGERS			* (compression_level > 0)
	| COMPRESS_INDICES			* (compression_level > 0)
	| USE_TRAV_MAXDEGREE		* (compression_level > 9)
	| USE_PRED_NORMAL			* (compression_level > 6)		
	| USE_PRED_TEXCOORD			* (compression_level > 6)	
	| USE_PRED_PARALLELOGRAM	* (compression_level > 2)	
	| USE_PRED_MPARALLELOGRAM	* (compression_level > 8);
}

enum DataType : uint8 {
	DT_INVALID = 0,
	DT_INT8,
	DT_UINT8,
	DT_INT16,
	DT_UINT16,
	DT_INT32,
	DT_UINT32,
	DT_INT64,
	DT_UINT64,
	DT_FLOAT32,
	DT_FLOAT64,
	DT_BOOL,
	DT_TYPES_COUNT
};
inline size_t GetSize(DataType type) {
	static constexpr uint8	sizes[] = {
		0, //DT_INVALID = 0,
		1, //DT_INT8,
		1, //DT_UINT8,
		2, //DT_INT16,
		2, //DT_UINT16,
		4, //DT_INT32,
		4, //DT_UINT32,
		8, //DT_INT64,
		8, //DT_UINT64,
		4, //DT_FLOAT32,
		8, //DT_FLOAT64,
		1, //DT_BOOL,
	};
	return sizes[type];
}
inline bool IsFloat(DataType type) {
	return type == DT_FLOAT32 || type == DT_FLOAT64;
}

struct Header {
	enum {
		// Latest Draco bit-stream versions
		POINTCLOUD_MAJOR	= 2,
		POINTCLOUD_MINOR	= 3,
		MESH_MAJOR			= 2,
		MESH_MINOR			= 2,

		METADATA_FLAG_MASK	= 0x8000,
		NUM_ENCODED_GEOMETRY_TYPES = 2,
	};

	int8			draco_string[5];
	uint8			version_major;
	uint8			version_minor;
	uint8			encoder_type;
	uint8			encoder_method;
	packed<uint16>	flags;

	Header()	{}
	Header(MODE mode, bool has_meta) {
		raw_copy("DRACO", draco_string);
		if (mode & IS_TRIANGULAR_MESH) {
			version_major	= MESH_MAJOR;
			version_minor	= MESH_MINOR;
		} else {
			version_major	= POINTCLOUD_MAJOR;
			version_minor	= POINTCLOUD_MINOR;
		}
		encoder_type		= (int(mode) >> 1) & 1;
		encoder_method		= int(mode) & 1;
		flags				= has_meta ? METADATA_FLAG_MASK : 0;
	}

	bool	valid() const {
		return memcmp(draco_string, "DRACO", 5) == 0 && encoder_type < NUM_ENCODED_GEOMETRY_TYPES;
	}
	constexpr MODE0		mode()		const { return MODE0((encoder_type << 1) | encoder_method); }
	constexpr uint16	version()	const { return (version_major << 8) | version_minor; }
	constexpr bool		has_meta()	const { return !!(flags & METADATA_FLAG_MASK); }
};

//-----------------------------------------------------------------------------
// Coding - Sequential attribute encoding methods
//-----------------------------------------------------------------------------

struct Coding {
	enum Method : uint8 {
		GENERIC,
		INTEGER,
		QUANTIZATION,
		NORMALS,
		KD_INTEGER,	// never stored, just for KD pointcloud
		KD_FLOAT3,	// never stored, just for (legacy) KD pointcloud
	};

	template<Method M> struct T;
	static Coding*	Get(Method method, uint32 num_components, DataType type, int quantisation = 0);

	virtual			~Coding() {}
#ifdef DRACO_ENABLE_READER
	virtual bool	Read(istream_ref file)	{ return true; }
	virtual void	Decode(void *out, const int32 *in, uint32 num_values)	{ ISO_ASSERT(0); }
#endif
#ifdef DRACO_ENABLE_WRITER
	virtual bool	Write(ostream_ref file)	{ return true; }
	virtual void	Encode(int32 *out, const void *in, uint32 num_values)	{ ISO_ASSERT(0); }
#endif
};

//-----------------------------------------------------------------------------
// PredictionTransforms - prediction scheme transform methods
//-----------------------------------------------------------------------------

struct PredictionTransform {
	enum Method : int8 {
		NONE							= -1,
		DELTA							= 0,	//deprecated
		WRAP							= 1,
		NORMAL_OCTAHEDRON				= 2,	//deprecated
		NORMAL_OCTAHEDRON_CANONICALIZED	= 3,
	};
	template<Method M> struct T;
	static	PredictionTransform*	Get(Method method, uint32 num_components);

	virtual ~PredictionTransform() {}
#ifdef DRACO_ENABLE_READER
	virtual bool Read(istream_ref file, uint16 version) { return true; }
	virtual void Decode(const int32 *pred, const int32 *corr, int32 *out)	{ ISO_ASSERT(0); }
#endif
#ifdef DRACO_ENABLE_WRITER
	virtual bool Write(ostream_ref file) { return true; }
	virtual void Encode(const int32 *pred, const int32 *in, int32 *out)		{ ISO_ASSERT(0); }
	virtual void Init(const int32 *in, uint32 num_values) {}
#endif
};

//-----------------------------------------------------------------------------
// Predictions - prediction encoding methods
//-----------------------------------------------------------------------------

struct Traversal;

struct Prediction {
	enum Method : int8 {
		NONE							= -2,	// no prediction scheme was used
		UNDEFINED						= -1,	// no specific prediction scheme is required
		DIFFERENCE						= 0,
		PARALLELOGRAM					= 1,
		MULTI_PARALLELOGRAM				= 2,	//deprecated
		TEX_COORDS_DEPRECATED			= 3,	//deprecated
		CONSTRAINED_MULTI_PARALLELOGRAM	= 4,
		TEX_COORDS_PORTABLE				= 5,
		GEOMETRIC_NORMAL				= 6,
	};
	// must pos be decoded prior to this?
	friend constexpr bool needs_pos(Method m) {
		return m == TEX_COORDS_DEPRECATED || m == TEX_COORDS_PORTABLE || m == GEOMETRIC_NORMAL;
	}
	// can TRAVERSAL_MAX_DEGREE help?
	friend constexpr bool maxprediction_helps(Method m) {
		return m == PARALLELOGRAM || m == MULTI_PARALLELOGRAM || m == CONSTRAINED_MULTI_PARALLELOGRAM;// || m == TEX_COORDS_PORTABLE;
	}
	template<Method M> struct T;
	static Prediction*	Get(Method method);

	unique_ptr<PredictionTransform>	transform;

	virtual ~Prediction() {}
#ifdef DRACO_ENABLE_READER
	virtual bool Read(istream_ref file, uint32 num_values, uint16 version) { return transform->Read(file, version); }
	virtual void Decode(const Traversal &traversal, int *values, uint32 num_components, uint32 num_values)	{ ISO_ASSERT(0); }
#endif
#ifdef DRACO_ENABLE_WRITER
	virtual bool Write(ostream_ref file, uint32 num_values) { return transform->Write(file); }
	virtual void Encode(const Traversal &traversal, int *values, uint32 num_components, uint32 num_values)	{ ISO_ASSERT(0); }
#endif
};

//-----------------------------------------------------------------------------
// Attribute
//-----------------------------------------------------------------------------

struct Attribute {
	enum Type : uint8 {
		POSITION,
		NORMAL,
		COLOR,
		TEX_COORD,
		GENERIC,					// attributes that are not assigned to any known predefined use case
		TANGENT,
		MATERIAL,
		JOINTS,
		WEIGHTS,
		NUM_NAMED_ATTRIBUTES,
	};

	struct Decoder			*dec;
	uint32					unique_id;
	Type					type;
	DataType				data_type;
	uint8					num_components;
	uint8					num_components_coding;
	uint8					normalised;
	PredictionTransform::Method	transform_method;
	Coding::Method			coding_method;
	Prediction::Method		prediction_method;
	unique_ptr<Coding>		coding;
	unique_ptr<Prediction>	pred;
//	dynamic_array<int32>	values;
	malloc_block			values;

	Attribute()	{}
	Attribute(uint32 unique_id, Attribute::Type type, DataType data_type, uint32 num_components, bool normalised, Coding::Method coding_method, int quantisation) :
		dec(nullptr), unique_id(unique_id), type(type), data_type(data_type), num_components(num_components), normalised(normalised), coding_method(coding_method), prediction_method(Prediction::DIFFERENCE)
	{
		coding = Coding::Get(coding_method, num_components, data_type, quantisation);
		num_components_coding = coding_method == Coding::NORMALS ? 2 : num_components;
	}
	void	SetCoding(Coding::Method method) {
		coding_method = method;
		if (coding_method == Coding::NORMALS)
			num_components_coding = 2;
		coding = Coding::Get(coding_method, num_components, data_type);
	}
	uint32	NumValues() const {
		return values.length() / (num_components_coding * sizeof(int));
	}

#ifdef DRACO_ENABLE_READER
	bool	ReadHeader(istream_ref file, uint16 version);
	bool	ReadCodingData(istream_ref file) {
		return !coding || coding->Read(file);
	}
	bool	ReadPrediction(istream_ref file, uint32 num_values, uint16 version);

	void	Decode(const Traversal &traversal) {
		if (pred)
			pred->Decode(traversal, values, num_components_coding, NumValues());
	}
	template<typename D, typename I> void CopyValues(D dst, I&& indices) const {
		auto		comp_size	= GetSize(data_type);
		auto		att_size	= comp_size * num_components;
		uint32		num_values	= NumValues();
		if (coding) {
			temp_block	decoded(num_values * att_size);
			coding->Decode(decoded, values, num_values);
			uint8	*src = decoded;
			for (auto i : indices)
				memcpy(&*dst++, src + att_size * i, att_size);
		} else {
			uint8	*src = values;
			for (auto i : indices)
				memcpy(&*dst++, src + att_size * i, att_size);
		}
	}
#endif

#ifdef DRACO_ENABLE_WRITER
	bool	WriteHeader(ostream_ref file) {
		return file.write(type, data_type, num_components, normalised, make_leb128(unique_id));
	}
	bool	WriteCodingData(ostream_ref file) {
		return !coding || coding->Write(file);
	}
	void	WriteCoding(ostream_ref file) {
		file.write(coding_method);
	}
	bool	WritePrediction(ostream_ref file, const Traversal &traversal, MODE mode);
	void	SetValues(const_memory_block decoded) {
		if (coding) {
			uint32		num_values	= decoded.length() / (GetSize(data_type) * num_components);
			values.resize(num_values * num_components_coding * sizeof(int));
			coding->Encode(values, decoded, num_values);
		} else {
			values = decoded;
		}
	}
#endif
};

//-----------------------------------------------------------------------------
// Decoder
//-----------------------------------------------------------------------------

struct Decoder {
	// attribute encoding methods
	enum AttributeEncoding : uint8 {
		VERTEX_ATTRIBUTE,
		CORNER_ATTRIBUTE,
	};
	// traversal methods
	enum TraversalMethod : uint8 {
		TRAVERSAL_DEPTH_FIRST,
		TRAVERSAL_MAX_DEGREE,
	};

	// for edgebreaker
	uint8						id;
	AttributeEncoding			type;
	TraversalMethod				traversal;

	// for KD point
	uint8						kd_compression;

	dynamic_array<Attribute>	attributes;
	dynamic_array<int32>		corner_to_value;

	Decoder()	: type(VERTEX_ATTRIBUTE), traversal(TRAVERSAL_DEPTH_FIRST) {}

	int		CornerToValue(uint32 corner)	const { return corner_to_value[corner]; }

#ifdef DRACO_ENABLE_READER
	void	ReadAttributes(istream_ref file, uint16 version);
	void	ReadCoding(istream_ref file);
	void	ReadPrediction(istream_ref file, uint32 num_values, uint16 version);
	void	Decode(const Traversal &traversal);
#endif
#ifdef DRACO_ENABLE_WRITER
	void	WriteAttributes(ostream_ref file);
	void	WriteCoding(ostream_ref file);
	void	WritePrediction(ostream_ref file, const Traversal &traversal, MODE mode);
#endif
};

//-----------------------------------------------------------------------------
// MetaData
//-----------------------------------------------------------------------------

struct block {
	malloc_block data;
	bool	read(istream_ref file)	{ return data.read(file, file.getc()); }
	bool	write(ostream_ref file) { return file.putc((uint8)data.length()) && data.write(file); }
};
struct MetadataEntry {
	block key, value;
	bool	read(istream_ref file)	{ return key.read(file) && value.read(file); }
	bool	write(ostream_ref file) { return key.write(file) && value.write(file); }
};

struct MetadataSub;

struct MetadataElement : dynamic_array<MetadataEntry> {
	dynamic_array<MetadataSub>	 sub;
	bool	read(istream_ref file) {
		return dynamic_array<MetadataEntry>::read(file, get_leb128<uint32>(file))
			&& sub.read(file, get_leb128<uint32>(file));
	}
	bool	write(ostream_ref file) {
		return file.write(make_leb128(size())) && dynamic_array<MetadataEntry>::write(file)
			&& file.write(make_leb128(sub.size())) && sub.write(file);
	}
};

struct MetadataSub : MetadataElement {
	block	key;
	bool	read(istream_ref file)	{ return key.read(file)	&& MetadataElement::read(file); }
	bool	write(ostream_ref file) { return key.write(file) && MetadataElement::write(file); }
};

//-----------------------------------------------------------------------------
// Draco
//-----------------------------------------------------------------------------

class Common {
public:
	hash_map_with_key<uint32, MetadataElement>	att_metadata;
	MetadataElement						file_metadata;
	dynamic_array<Decoder>				dec;	//0 is pos
	dynamic_array<int32>				corner_to_point;
	dynamic_array<int32>				point_to_corner;
	uint32								num_points;

	Common() : dec(1), num_points(0) {}

	uint32	NumPoints()		const	{ return point_to_corner.size32(); }
	uint32	NumFaces()		const	{ return corner_to_point.size32() / 3; }
	auto&	CornerToPoint()	const	{ return corner_to_point; }
	auto&	PointToCorner()	const	{ return point_to_corner; }
};

#ifdef DRACO_ENABLE_READER
class Reader : public Common {
	uint16	version;

	bool	ReadPointCloudSequential(istream_ref file);
	bool	ReadPointCloudKD(istream_ref file);
	bool	ReadMeshSequential(istream_ref file);
	bool	ReadMeshEdgebreaker(istream_ref file);
	bool	ReadMetaData(istream_ref file);

public:
	dynamic_array<int32>	PointToValue(const Decoder &d) const;
	const Attribute*		GetAttribute(int unique_id) const;
	bool	read(istream_ref file);
};
#endif

#ifdef DRACO_ENABLE_WRITER
class Writer : public Common {
	bool	WritePointCloudSequential(ostream_ref file);
	bool	WritePointCloudKD(ostream_ref file);
	bool	WriteMeshSequential(ostream_ref file);
	bool	WriteMeshEdgebreaker(ostream_ref file);
	bool	WriteMetaData(ostream_ref file);
public:
	MODE	mode;

	Writer(MODE mode) : mode(mode) {}
	Writer(MODE0 mode, uint32 compression_level = 7) : mode(DefaultMode(mode, compression_level)) {}
	template<typename I> void SetIndices(I &&indices) { corner_to_point = indices; }
	Attribute*		AddAttribute(uint32 unique_id, Attribute::Type type, DataType data_type, uint32 num_components, bool normalised, int quantisation = 0);
	bool	write(ostream_ref file);
};
#endif

}	//namespace draco
