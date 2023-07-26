#include "base/defs.h"
#include "base/vector.h"

namespace meshopt {
using namespace iso;

//-----------------------------------------------------------------------------
// index compression
//-----------------------------------------------------------------------------

size_t	encodeIndexBuffer(memory_block buffer, const uint32* indices, size_t count, int version);
bool	decodeIndexBuffer(const_memory_block buffer, uint32* destination, size_t count);
size_t	encodeIndexSequence(memory_block buffer, const uint32* indices, size_t count, int version);
bool	decodeIndexSequence(const_memory_block buffer, uint32* destination, size_t count);

malloc_block compressIndexStream(const void *data, size_t count, size_t max_index, size_t stride, int version);
malloc_block compressIndexSequence(const void *data, size_t count, size_t max_index, size_t stride, int version);

//-----------------------------------------------------------------------------
// vertex compression
//-----------------------------------------------------------------------------

size_t	encodeVertexBuffer(memory_block buffer, const void* vertices, size_t count, size_t vertex_size, int version);
bool	decodeVertexBuffer(const_memory_block buffer, void* destination, size_t count, size_t vertex_size);

malloc_block compressVertexStream(const void *data, size_t count, size_t vertex_size, int version);

//-----------------------------------------------------------------------------
// vertex filters
//-----------------------------------------------------------------------------

template<int N> inline auto get_exp(vec<float,N> f)	{ return ((as<int>(f) >> 23) & 0x7f) - 128; }
template<int N> inline auto	make_exp(vec<int,N> i)	{ return as<float>((i + 127) << 23); }

inline int32x4 encodeOct(float4 n, int bits, int wbits) {
	// octahedral encoding of a unit vector
	float	nl	= reduce_add(abs(n.xyz));
	auto	xy	= nl == 0 ? float2(zero) : n.xy / nl;

	if (n.z < 0)
		xy = (1 - abs(xy)) * sign1(xy);

	return to<int>(round(concat(concat(xy, one) * iso::bits(bits - 1), n.w * iso::bits(wbits - 1))));
}

inline float3 decodeOct(int32x4 i) {
	float4	n = to<float>(i);
	n.z -= reduce_add(abs(n.xy));
	
	// fixup octahedral coordinates for z<0
	if (n.z < 0)
		n.xy += copysign(n.z, n.xy);

	// compute normal length & scale
	return normalise(n.xyz);
}


inline int32x4 encodeQuat(float4 q, int bits) {
	ISO_ASSERT(bits >= 4 && bits <= 16);

	// establish maximum quaternion component
	int		qc = max_component_index(abs(q));
	
	// we use double-cover properties to discard the sign
	float	scale = copysign(sqrt2, q[qc]) * iso::bits(bits - 1);
	
	auto	i = to<int>(round(rotate(q, qc).xyz * scale));
	return concat(i, iso::bits(bits - 3, 2) | qc);
}

inline float4 decodeQuat(int32x4 i) {
	auto	v	= to<float>(i.xyz) / (sqrt2 * (i.w | 3));
	float	w	= sqrt(one - len2(v));
	int		qc	= i.w & 3;
	return rotate(concat(v, w), qc);
}

inline int32x3 encodeExpShared(float3 v, int bits) {
	// use maximum exponent to encode values, and scale the mantissa to make it a K-bit signed integer
	int32x3 e	= get_exp<3>(v);
	int		exp = reduce_max(e) - (bits - 1);

	// compute renormalized rounded mantissas for each component
	int32x3 m	= to<int>(round(v * make_exp<3>(-exp)));

	// encode exponent & mantissa into each resulting value
	return (m & iso::bits(24)) | (exp << 24);
}

inline float4 decodeExp(int32x4 i) {
	return to<float>((i << 8) >> 8) * make_exp<4>(i.x >> 24);
}

void encodeExpParallel(int32x3 *out, range<const float3*> data, int bits);

bool	defilterVertexBuffer(memory_block buffer, int filter, size_t vertex_size);


} // namespace meshopt