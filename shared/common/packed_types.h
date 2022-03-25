#ifndef PACKED_TYPES_H
#define PACKED_TYPES_H

#include "base/vector.h"
#include "base/soft_float.h"

namespace iso {

typedef soft_float<6,5,false>	ufloat11;
typedef soft_float<5,5,false>	ufloat10;

template<typename I, int64 S, int B> using scaled_bits = compact<scaled<I,S>, B>;
template<typename I, int64 S, int B> using scaled_bitsA = compact<scaledA<I,S>, B>;
template<int B>				using norm_bits		= scaled_bits<sint_bits_t<B>, ((1 << B) - 1), B>;
template<int B>				using unorm_bits	= scaled_bits<uint_bits_t<B>, ((1 << B) - 1), B>;

typedef bitfield_vec<compact<int,10>, compact<int,10>, compact<int,10>>											int3_10_10_10;
typedef bitfield_vec<compact<uint32,10>, compact<uint32,10>, compact<uint32,10>>								uint3_10_10_10;
typedef bitfield_vec<scaled_bits<int,511,10>, scaled_bits<int,511,10>, scaled_bits<int,511,10>>					norm3_10_10_10;
typedef bitfield_vec<scaled_bits<uint32,1023,10>, scaled_bits<uint32,1023,10>, scaled_bits<uint32,1023,10>>		unorm3_10_10_10;
typedef bitfield_vec<ufloat10, ufloat10, ufloat10>																float3_10_10_10;

typedef bitfield_vec<compact<int,10>, compact<int,10>, compact<int,10>, compact<int,2>>													int4_10_10_10_2;
typedef bitfield_vec<compact<uint32,10>, compact<uint32,10>, compact<uint32,10>, compact<uint32,2>>										uint4_10_10_10_2;
typedef bitfield_vec<scaled_bits<int,511,10>, scaled_bits<int,511,10>, scaled_bits<int,511,10>, scaled_bits<int,1,2>>					norm4_10_10_10_2;
typedef bitfield_vec<scaled_bits<uint32,1023,10>, scaled_bits<uint32,1023,10>, scaled_bits<uint32,1023,10>, scaled_bits<uint32,3,2>>	unorm4_10_10_10_2;
typedef bitfield_vec<ufloat10, ufloat10, ufloat10, compact<uint32,2>>																	float4_10_10_10_2;

typedef bitfield_vec<compact<int,2>, compact<int,10>, compact<int,10>, compact<int,10>>													int4_2_10_10_10;
typedef bitfield_vec<compact<uint32,2>, compact<uint32,10>, compact<uint32,10>, compact<uint32,10>>										uint4_2_10_10_10;
typedef bitfield_vec<scaled_bits<int,1,2>, scaled_bits<int,511,10>, scaled_bits<int,511,10>, scaled_bits<int,511,10>>					norm4_2_10_10_10;
typedef bitfield_vec<scaled_bits<uint32,3,2>, scaled_bits<uint32,1023,10>, scaled_bits<uint32,1023,10>, scaled_bits<uint32,1023,10>>	unorm4_2_10_10_10;
typedef bitfield_vec<compact<uint32,2>, ufloat10, ufloat10, ufloat10>																	float4_2_10_10_10;

typedef bitfield_vec<compact<int,10>, compact<int,11>, compact<int,11>>											int3_10_11_11;
typedef bitfield_vec<compact<uint32,10>, compact<uint32,11>, compact<uint32,11>>								uint3_10_11_11;
typedef bitfield_vec<scaled_bits<int,511,10>, scaled_bits<int,1023,11>, scaled_bits<int,1023,11>>				norm3_10_11_11;
typedef bitfield_vec<scaled_bits<uint32,1023,10>, scaled_bits<uint32,2047,11>, scaled_bits<uint32,2047,11>>		unorm3_10_11_11;
typedef bitfield_vec<ufloat10, ufloat11, ufloat11>																float3_10_11_11;

typedef bitfield_vec<compact<int,11>, compact<int,11>, compact<int,10>>											int3_11_11_10;
typedef bitfield_vec<compact<uint32,11>, compact<uint32,11>, compact<uint32,10>>								uint3_11_11_10;
typedef bitfield_vec<scaled_bits<int,1023,11>, scaled_bits<int,1023,11>, scaled_bits<int,511,10>>				norm3_11_11_10;
typedef bitfield_vec<scaled_bits<uint32,2047,11>, scaled_bits<uint32,2047,11>, scaled_bits<uint32,1023,10>>		unorm3_11_11_10;
typedef bitfield_vec<ufloat11, ufloat11, ufloat10>																float3_11_11_10;

typedef array_vec<unorm8, 3>		rgb8;
typedef array_vec<unorm8, 4>		rgba8;
typedef	bitfield_vec<scaled_bits<uint16,31,5>, scaled_bits<uint16,63,6>, scaled_bits<uint16,31,5>>								r5g6b5;
typedef	bitfield_vec<scaled_bits<uint16,31,5>, scaled_bits<uint16,31,5>, scaled_bits<uint16,31,5>, scaled_bits<uint16,1,1>>		r5g5b5a1;
typedef	bitfield_vec<scaled_bits<uint16,15,4>, scaled_bits<uint16,15,4>, scaled_bits<uint16,15,4>, scaled_bits<uint16,15,4>>	r4g4b4a4;
typedef	bitfield_vec<scaled_bits<uint8,7,3>, scaled_bits<uint8,7,3>, scaled_bits<uint8,3,2>>									r3g3b2;

typedef bitfield_vec<scaled_bits<int32be,511,10>, scaled_bits<int32be,1023,11>, scaled_bits<int32be,1023,11>>				norm3_10_11_11_be;
typedef bitfield_vec<scaled_bits<int32be,1023,11>, scaled_bits<int32be,1023,11>, scaled_bits<int32be,511,10>>				norm3_11_11_10_be;
typedef bitfield_vec<scaled_bitsA<int32be,1023,11>, scaled_bitsA<int32be,1023,11>, scaled_bitsA<int32be,511,10>>			norm3_11_11_10A_be;

typedef	bitfield_vec<norm_bits<24>, uint8>								norm24_uint8;

template<int M> using compressed_vector3 = bitfield_vec<scaled_bits<int,512/M, 10>, scaled_bits<int,512/M, 10>, scaled_bits<int,512/M, 10> >;

struct compressed_normal3 {
	uint32	ISO_BITFIELDS4(s:1, t:2, a:14, b:14);
	inline	compressed_normal3()	{}
	inline	compressed_normal3(param(float3) v) {
		t	= max_component_index(abs(v));
		float3	v2 = rotate(v, t);
		v2	= v2 * 8191.f / abs(v2.x);
		s	= int(v2.x < 0.f);
		a	= int(v2.y);
		b	= int(v2.z);
	}
	inline	operator float3()		const {
		return normalise(rotate(float3{
			float(int(a) - int((a & (1<<13))<<1)),
			float(int(b) - int((a & (1<<13))<<1)),
			float(s ? 1-(1<<13) : (1<<13)-1)
			}, 2 - t));
	}
};

struct compressed_quaternion {
	uint32	ISO_BITFIELDS4(t:2, a:10, b:10, c:10);
	compressed_quaternion()			{}
	inline	compressed_quaternion(param(float4) p) {
		t	= max_component_index(abs(p));
		float4	v = rotate(p, t);
		v	= v * 511.f / v.x;
		a	= int(v.y);
		b	= int(v.z);
		c	= int(v.w);
	}
	inline	compressed_quaternion(param(quaternion) q) {
		t	= max_component_index(abs(q.v));
		float4	v = rotate(q.v, t);
		v	= v * 511.f / v.x;
		a	= int(v.y);
		b	= int(v.z);
		c	= int(v.w);
	}
	inline	operator quaternion()	const {
		return normalise(rotate(float4{
			float(int(a) - int((a & (1<<9))<<1)),
			float(int(b) - int((b & (1<<9))<<1)),
			float(int(c) - int((c & (1<<9))<<1)),
			float((1<<9)-1)
		}, 3 - t));
	}
};

}// namespace iso
#endif // PACKED_TYPES_H
