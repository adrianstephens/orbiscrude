typedef unsigned int	uint, dword;
typedef unsigned long	ulong;
typedef unsigned short	ushort;
typedef short float		half;
typedef long			int64;
typedef unsigned long	uint64;
typedef float			fp32;
typedef double			fp64;

typedef unsigned char	uint8;
typedef signed char		int8;
typedef unsigned short	uint16;
typedef short			int16;
typedef unsigned int	uint32;
typedef int				int32;

// float
struct float1 {float x;};
struct float2 {float x, y;};
struct float3 {float x, y, z;};
struct float4 {float x, y, z, w;};

typedef float1	float1x1[1];
typedef float1	float1x2[2];
typedef float1	float1x3[3];
typedef float1	float1x4[4];

typedef float2	float2x1[1];
typedef float2	float2x2[2];
typedef float2	float2x3[3];
typedef float2	float2x4[4];

typedef float3	float3x1[1];
typedef float3	float3x2[2];
typedef float3	float3x3[3];
typedef float3	float3x4[4];

typedef float4	float4x1[1];
typedef float4	float4x2[2];
typedef float4	float4x3[3];
typedef float4	float4x4[4];

// int
struct int1 {int x;};
struct int2 {int x, y;};
struct int3 {int x, y, z;};
struct int4 {int x, y, z, w;};

typedef int1	int1x1[1];
typedef int1	int1x2[2];
typedef int1	int1x3[3];
typedef int1	int1x4[4];

typedef int2	int2x1[1];
typedef int2	int2x2[2];
typedef int2	int2x3[3];
typedef int2	int2x4[4];

typedef int3	int3x1[1];
typedef int3	int3x2[2];
typedef int3	int3x3[3];
typedef int3	int3x4[4];

typedef int4	int4x1[1];
typedef int4	int4x2[2];
typedef int4	int4x3[3];
typedef int4	int4x4[4];

// uint
struct uint1 {uint x;};
struct uint2 {uint x, y;};
struct uint3 {uint x, y, z;};
struct uint4 {uint x, y, z, w;};

typedef uint1	uint1x1[1];
typedef uint1	uint1x2[2];
typedef uint1	uint1x3[3];
typedef uint1	uint1x4[4];

typedef uint2	uint2x1[1];
typedef uint2	uint2x2[2];
typedef uint2	uint2x3[3];
typedef uint2	uint2x4[4];

typedef uint3	uint3x1[1];
typedef uint3	uint3x2[2];
typedef uint3	uint3x3[3];
typedef uint3	uint3x4[4];

typedef uint4	uint4x1[1];
typedef uint4	uint4x2[2];
typedef uint4	uint4x3[3];
typedef uint4	uint4x4[4];

// half
struct half1 {half x;};
struct half2 {half x, y;};
struct half3 {half x, y, z;};
struct half4 {half x, y, z, w;};

typedef half1	half1x1[1];
typedef half1	half1x2[2];
typedef half1	half1x3[3];
typedef half1	half1x4[4];

typedef half2	half2x1[1];
typedef half2	half2x2[2];
typedef half2	half2x3[3];
typedef half2	half2x4[4];

typedef half3	half3x1[1];
typedef half3	half3x2[2];
typedef half3	half3x3[3];
typedef half3	half3x4[4];

typedef half4	half4x1[1];
typedef half4	half4x2[2];
typedef half4	half4x3[3];
typedef half4	half4x4[4];

// double
struct double1 {double x;};
struct double2 {double x, y;};
struct double3 {double x, y, z;};
struct double4 {double x, y, z, w;};

typedef double1	double1x1[1];
typedef double1	double1x2[2];
typedef double1	double1x3[3];
typedef double1	double1x4[4];

typedef double2	double2x1[1];
typedef double2	double2x2[2];
typedef double2	double2x3[3];
typedef double2	double2x4[4];

typedef double3	double3x1[1];
typedef double3	double3x2[2];
typedef double3	double3x3[3];
typedef double3	double3x4[4];

typedef double4	double4x1[1];
typedef double4	double4x2[2];
typedef double4	double4x3[3];
typedef double4	double4x4[4];

// bool
struct bool1 {bool x;};
struct bool2 {bool x, y;};
struct bool3 {bool x, y, z;};
struct bool4 {bool x, y, z, w;};

typedef bool1	bool1x1[1];
typedef bool1	bool1x2[2];
typedef bool1	bool1x3[3];
typedef bool1	bool1x4[4];

typedef bool2	bool2x1[1];
typedef bool2	bool2x2[2];
typedef bool2	bool2x3[3];
typedef bool2	bool2x4[4];

typedef bool3	bool3x1[1];
typedef bool3	bool3x2[2];
typedef bool3	bool3x3[3];
typedef bool3	bool3x4[4];

typedef bool4	bool4x1[1];
typedef bool4	bool4x2[2];
typedef bool4	bool4x3[3];
typedef bool4	bool4x4[4];

// short
struct short1 {short x;};
struct short2 {short x, y;};
struct short3 {short x, y, z;};
struct short4 {short x, y, z, w;};

typedef short1	short1x1[1];
typedef short1	short1x2[2];
typedef short1	short1x3[3];
typedef short1	short1x4[4];

typedef short2	short2x1[1];
typedef short2	short2x2[2];
typedef short2	short2x3[3];
typedef short2	short2x4[4];

typedef short3	short3x1[1];
typedef short3	short3x2[2];
typedef short3	short3x3[3];
typedef short3	short3x4[4];

typedef short4	short4x1[1];
typedef short4	short4x2[2];
typedef short4	short4x3[3];
typedef short4	short4x4[4];

// ushort
struct ushort1 {ushort x;};
struct ushort2 {ushort x, y;};
struct ushort3 {ushort x, y, z;};
struct ushort4 {ushort x, y, z, w;};

typedef ushort1	ushort1x1[1];
typedef ushort1	ushort1x2[2];
typedef ushort1	ushort1x3[3];
typedef ushort1	ushort1x4[4];

typedef ushort2	ushort2x1[1];
typedef ushort2	ushort2x2[2];
typedef ushort2	ushort2x3[3];
typedef ushort2	ushort2x4[4];

typedef ushort3	ushort3x1[1];
typedef ushort3	ushort3x2[2];
typedef ushort3	ushort3x3[3];
typedef ushort3	ushort3x4[4];

typedef ushort4	ushort4x1[1];
typedef ushort4	ushort4x2[2];
typedef ushort4	ushort4x3[3];
typedef ushort4	ushort4x4[4];

// long
struct long1 {long x;};
struct long2 {long x, y;};
struct long3 {long x, y, z;};
struct long4 {long x, y, z, w;};

typedef long1	long1x1[1];
typedef long1	long1x2[2];
typedef long1	long1x3[3];
typedef long1	long1x4[4];

typedef long2	long2x1[1];
typedef long2	long2x2[2];
typedef long2	long2x3[3];
typedef long2	long2x4[4];

typedef long3	long3x1[1];
typedef long3	long3x2[2];
typedef long3	long3x3[3];
typedef long3	long3x4[4];

typedef long4	long4x1[1];
typedef long4	long4x2[2];
typedef long4	long4x3[3];
typedef long4	long4x4[4];

// ulong
struct ulong1 {ulong x;};
struct ulong2 {ulong x, y;};
struct ulong3 {ulong x, y, z;};
struct ulong4 {ulong x, y, z, w;};

typedef ulong1	ulong1x1[1];
typedef ulong1	ulong1x2[2];
typedef ulong1	ulong1x3[3];
typedef ulong1	ulong1x4[4];

typedef ulong2	ulong2x1[1];
typedef ulong2	ulong2x2[2];
typedef ulong2	ulong2x3[3];
typedef ulong2	ulong2x4[4];

typedef ulong3	ulong3x1[1];
typedef ulong3	ulong3x2[2];
typedef ulong3	ulong3x3[3];
typedef ulong3	ulong3x4[4];

typedef ulong4	ulong4x1[1];
typedef ulong4	ulong4x2[2];
typedef ulong4	ulong4x3[3];
typedef ulong4	ulong4x4[4];

// structs

struct buffer		{ uint4		m_regs; };
struct sampler		{ uint4		m_regs; };
struct texture		{ ulong4	m_regs; };
struct texture128	{ uint4		m_regs; };

typedef buffer ConstantBuffer;
typedef buffer ByteBuffer;
typedef buffer RW_ByteBuffer;

template<typename T> struct DataBuffer			: buffer { T load(); void store(T &t); };
template<typename T> struct RW_DataBuffer		: DataBuffer<T> {};
template<typename T> struct RegularBuffer		: DataBuffer<T> {};
template<typename T> struct RW_RegularBuffer	: DataBuffer<T> {};
template<typename T> struct AppendRegularBuffer : DataBuffer<T> {};
template<typename T> struct ConsumeRegularBuffer: DataBuffer<T> {};
template<typename T> struct PointBuffer			: DataBuffer<T> {};
template<typename T> struct LineBuffer			: DataBuffer<T> {};
template<typename T> struct TriangleBuffer		: DataBuffer<T> {};

template<typename T> struct Texture2D_R128		: texture128 { T load(); };
template<typename T> struct RW_Texture2D_R128	: texture128 { T load(); };

struct Texture2D			: texture {};
struct Texture1D			: texture {};
struct Texture1D_Array		: texture {};
struct Texture2D			: texture {};
struct Texture2D_Array		: texture {};
struct Texture3D			: texture {};
struct TextureCube			: texture {};

struct RW_Texture2D			: texture {};
struct RW_Texture1D			: texture {};
struct RW_Texture1D_Array	: texture {};
struct RW_Texture2D			: texture {};
struct RW_Texture2D_Array	: texture {};
struct RW_Texture3D			: texture {};

struct sampler1D			: sampler {};
struct sampler2D			: sampler {};
struct sampler3D			: sampler {};
struct samplerCUBE			: sampler {};
struct sampler_state		: sampler {};
struct SamplerState			: sampler {};

//tesselation
struct TessellationData {
	uint32	ls_stride;
	uint32	cp_stride;
	uint32	num_patch;
	uint32	hull_output_base;
	uint32	patch_const_size;
	uint32	patch_const_base;
	uint32	patch_output_size;
	float	off_chip_threshold;
	uint32	first_edge_tess_index;
};

//dispatch draw
struct DispatchDrawTriangleCullV1SharedData {
	buffer			bufferIrb;
	uint16			gdsOffsetOfIrbWptr;
	uint16			cullSettings;
	float			quantErrorScreenX;
	float			quantErrorScreenY;
	float			gbHorizClipAdjust;
	float			gbVertClipAdjust;
};
class DispatchDrawTriangleCullV1Data {
	const DispatchDrawTriangleCullV1SharedData *long shared;
	buffer			bufferInputIndexData;
	uint16			numIndexDataBlocks;
	uint8			numIndexBits;				//(instancing only) The number of index bits
	uint8			numInstancesPerTgMinus1;	//(instancing only) The number of instances which will be evaluated per thread group minus 1
	uint16			instanceStepRate0Minus1;	//(instancing only) vInstance / (1 + m_instanceStepRate0Minus1)
	uint16			instanceStepRate1Minus1;	//(instancing only) vInstance / (1 + m_instanceStepRate1Minus1)
};