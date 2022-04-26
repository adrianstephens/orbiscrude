typedef unsigned int		uint, dword;
typedef unsigned long		ulong;
typedef short float			half;
typedef long				int64;
typedef unsigned long		uint64;

// float
typedef float	float1[1];
typedef float	float2[2];
typedef float	float3[3];
typedef float	float4[4];

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
typedef int		int1[1];
typedef int		int2[2];
typedef int		int3[3];
typedef int		int4[4];

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
typedef uint	uint1[1];
typedef uint	uint2[2];
typedef uint	uint3[3];
typedef uint	uint4[4];

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
typedef half	half1[1];
typedef half	half2[2];
typedef half	half3[3];
typedef half	half4[4];

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
typedef double	double1[1];
typedef double	double2[2];
typedef double	double3[3];
typedef double	double4[4];

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
typedef bool	bool1[1];
typedef bool	bool2[2];
typedef bool	bool3[3];
typedef bool	bool4[4];

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

// structs

struct buffer	{ unsigned int dummy[4]; };
struct sampler	{ unsigned int dummy[4]; };
struct texture	{ unsigned int dummy[8]; };

typedef texture	Texture2D;
typedef texture	Texture1D;
typedef texture	Texture1DArray;
typedef texture	Texture2D;
typedef texture	Texture2DArray;
typedef texture	Texture3D;
typedef texture	TextureCube;

typedef sampler sampler1D;
typedef sampler sampler2D;
typedef sampler sampler3D;
typedef sampler samplerCUBE;
typedef sampler sampler_state;
typedef sampler SamplerState;
