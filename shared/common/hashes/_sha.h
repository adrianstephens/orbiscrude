#ifndef _SHA_H
#define _SHA_H

#include "base/bits.h"

namespace iso {

class SHA1_const {
protected:
	static constexpr int block_size = 64;

	static constexpr uint32 ch(uint32 x, uint32 y, uint32 z)	{ return (x & y) ^ (~x & z); }
	static constexpr uint32 py(uint32 x, uint32 y, uint32 z)	{ return x ^ y ^ z; }
	static constexpr uint32 mj(uint32 x, uint32 y, uint32 z)	{ return (x & y) ^ (x & z) ^ (y & z); }
	static constexpr uint32 rl30(uint32 x)						{ return rotate_left<30>(x); }

	static constexpr uint32 r0(uint32 v, uint32 w, uint32 x, uint32 y, uint32 z, uint32 i) { return z + ch(w, x, y) + i + 0x5A827999 + rotate_left(v, 5); }
	static constexpr uint32 r1(uint32 v, uint32 w, uint32 x, uint32 y, uint32 z, uint32 i) { return z + py(w, x, y) + i + 0x6ED9EBA1 + rotate_left(v, 5); }
	static constexpr uint32 r2(uint32 v, uint32 w, uint32 x, uint32 y, uint32 z, uint32 i) { return z + mj(w, x, y) + i + 0x8F1BBCDC + rotate_left(v, 5); }
	static constexpr uint32 r3(uint32 v, uint32 w, uint32 x, uint32 y, uint32 z, uint32 i) { return z + py(w, x, y) + i + 0xCA62C1D6 + rotate_left(v, 5); }

	static constexpr uint32 r0s(uint32 v, uint32 w, uint32 x, uint32 y, uint32 z, uint32 i) { return r0(v, w, rl30(x), rl30(y), rl30(z), i); }
	static constexpr uint32 r1s(uint32 v, uint32 w, uint32 x, uint32 y, uint32 z, uint32 i) { return r1(v, w, rl30(x), rl30(y), rl30(z), i); }
	static constexpr uint32 r2s(uint32 v, uint32 w, uint32 x, uint32 y, uint32 z, uint32 i) { return r2(v, w, rl30(x), rl30(y), rl30(z), i); }
	static constexpr uint32 r3s(uint32 v, uint32 w, uint32 x, uint32 y, uint32 z, uint32 i) { return r3(v, w, rl30(x), rl30(y), rl30(z), i); }

	struct State {
		uint32	a, b, c, d, e;

		constexpr State	R0(uint32 w) const { return { r0(a,b,c,d,e,w),	a, rl30(b), c, d }; }
		constexpr State	R1(uint32 w) const { return { r1(a,b,c,d,e,w),	a, rl30(b), c, d }; }
		constexpr State	R2(uint32 w) const { return { r2(a,b,c,d,e,w),	a, rl30(b), c, d }; }
		constexpr State	R3(uint32 w) const { return { r3(a,b,c,d,e,w),	a, rl30(b), c, d }; }

		friend constexpr State operator+(const State &x, const State &y) { return {x.a + y.a, x.b + y.b, x.c + y.c, x.d + y.d, x.e + y.e}; }
	};

	static constexpr State	init_state() { return {0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476, 0xc3d2e1f0}; }

	typedef	meta::array<uint32, 80> w_array_all;

	struct w_array {
		meta::array<uint32, 16> t;

		constexpr uint32 at(size_t i) const noexcept {
			return i < 16 ? t.t[i] : rotate_left(at(i - 3) ^ at(i - 8) ^ at(i - 14) ^ at(i - 16), 1);
		}

		template<size_t N, size_t... I> static constexpr meta::array<uint32, 16> get(meta::array<uint8, N> const &input, uint32 start_pos, const index_list<I...>) noexcept {
			return {{bytes_to_u4(input[start_pos + I * 4 + 3], input[start_pos + I * 4 + 2], input[start_pos + I * 4 + 1], input[start_pos + I * 4 + 0])...}};
		}
		template<size_t N> static constexpr uint32 at(const meta::array<uint32, N> &prev, int i) {
			return i < 0 ? prev[N + i] : rotate_left(at(prev, i - 3) ^ at(prev, i - 8) ^ at(prev, i - 14) ^ at(prev, i - 16), 1);
		}
		template<size_t... I, size_t N> static constexpr meta::array<uint32, 16> next(const meta::array<uint32, N> &prev, const index_list<I...>) {
			return {{at(prev, I)...}};
		}
		template<size_t... I, size_t N> static constexpr auto next_chunk(const meta::array<uint32, N> &input, const index_list<I...> il) {
			return next_chunk(input + w_array::next(input, il), il);
		}
		template<size_t... I> static constexpr auto next_chunk(const meta::array<uint32, 64> &input, const index_list<I...> il) {
			return input + next(input, il);
		}
		constexpr auto all() const {
			return next_chunk(t, meta::make_index_list<16>());
		}
		template<size_t N> constexpr w_array(const meta::array<uint8, N> &input, uint32 start_pos) : t(get(input, start_pos, meta::make_index_list<16>())) {}
	};

	struct w_array2 {
		uint32	t[16];

		uint32 operator[](size_t i) {
			return i < 16 ? t[i] : t[i & 15] = rotate_left<1>(t[(i - 3) & 15] ^ t[(i - 8) & 15] ^ t[(i - 14) & 15] ^ t[i & 15]);
		}
		w_array2(const void *data) {
			for (int i = 0; i < 16; i++)
				t[i] = ((uint32be*)data)[i];
		}
	};

	template<int I> static constexpr State R0x(const w_array_all &w, const State &blk)	{ return R0x<I + 1>(w, blk.R0(w[I +  0])); }
	template<int I> static constexpr State R1x(const w_array_all &w, const State &blk)	{ return R1x<I + 1>(w, blk.R1(w[I + 20])); }
	template<int I> static constexpr State R2x(const w_array_all &w, const State &blk)	{ return R2x<I + 1>(w, blk.R2(w[I + 40])); }
	template<int I> static constexpr State R3x(const w_array_all &w, const State &blk)	{ return R3x<I + 1>(w, blk.R3(w[I + 60])); }

	template<typename W> constexpr static State process_block(const W &w, const State &blk);
	constexpr static State process_block(const w_array &W, const State &blk);

	template<size_t N> static constexpr State finalize(const meta::array<uint8, N> &input, const State &blk) noexcept;

	template<size_t N> static constexpr State process_input(const meta::array<uint8, N> &input, uint32 i, const State &blk) noexcept {
		return i + block_size <= N
			? process_input(input, i + block_size, process_block(w_array(input, i), blk))
			: finalize(
				meta::slice<N % block_size>(input, i) + uint8(0x80) + meta::array<uint8, (block_size << int(N % block_size + 9 > block_size)) - (N % block_size + 9)>{} + be_byte_array(uint64(N * 8)),
				blk
			);
	}

	static constexpr auto to_byte_array(const State &blk) {
		return be_byte_array(blk.a) + be_byte_array(blk.b) + be_byte_array(blk.c) + be_byte_array(blk.d) + be_byte_array(blk.e);
	}
public:

	template<size_t N> static constexpr auto calculate(const meta::array<uint8, N> &input) {
		return to_byte_array(process_input(input, 0, init_state()));
	}
//	template<size_t N> static constexpr auto calculate(const char (&input)[N]) {
//		return to_byte_array(process_input(make_constexpr_string<uint8>(input), 0, init_state()));
//	}
};

template<> constexpr SHA1_const::State SHA1_const::R0x<19>(const w_array_all &w, const State &blk)	{ return blk.R0(w[19 +  0]); }
template<> constexpr SHA1_const::State SHA1_const::R1x<19>(const w_array_all &w, const State &blk)	{ return blk.R1(w[19 + 20]); }
template<> constexpr SHA1_const::State SHA1_const::R2x<19>(const w_array_all &w, const State &blk)	{ return blk.R2(w[19 + 40]); }
template<> constexpr SHA1_const::State SHA1_const::R3x<19>(const w_array_all &w, const State &blk)	{ return blk.R3(w[19 + 60]); }

template<typename W> constexpr SHA1_const::State SHA1_const::process_block(const W &w, const State &blk) {
	return R3x<0>(w, R2x<0>(w, R1x<0>(w, R0x<0>(w, blk)))) + blk;
}
constexpr SHA1_const::State SHA1_const::process_block(const SHA1_const::w_array &W, const State &blk)	{
	return process_block(W.all(), blk);
}

template<> specialised(static) constexpr SHA1_const::State SHA1_const::finalize<SHA1_const::block_size>(const meta::array<uint8, SHA1_const::block_size> &input, const SHA1_const::State &blk) noexcept {
	return process_block(w_array(input, 0).all(), blk);
}
template<> specialised(static) constexpr SHA1_const::State SHA1_const::finalize<SHA1_const::block_size * 2>(const meta::array<uint8, SHA1_const::block_size * 2> &input, const SHA1_const::State &blk) noexcept {
	return process_block(w_array(input, block_size).all(), process_block(w_array(input, 0).all(), blk));
}

} //namespace iso

#endif// _SHA_H

