#include "draco.h"
#include "codec/vlc.h"
#include "base/vector.h"
#include "base/algorithm.h"
#include "base/strings.h"

namespace draco {

enum {
	VER_TRAVERSAL_METHOD	= 0x0102,
	VER_COMPRESS_SPLITS		= 0x0102,
	VER_LEB128_ID			= 0x0103,
	VER_LEB128				= 0x0200,
	VER_CODING_AFTER_INTS	= 0x0200,
	VER_REMOVE_HOLES		= 0x0201,
	VER_REMOVE_UNUSED		= 0x0202,
	VER_LEB128_ANS			= 0x0202,
	VER_START_ANS			= 0x0202,
	VER_SPLITS_AFTER_CONN	= 0x0202,
	VER_VALENCE_ONLYPRED	= 0x0202,
	VER_KD_UPDATE			= 0x0203,
};

uint32	get_uint32(istream_ref file, bool not_leb) {
	return not_leb ? file.get<uint32>() : get_leb128<uint32>(file);
}
uint64	get_uint64(istream_ref file, bool not_leb) {
	return not_leb ? file.get<uint64>() : get_leb128<uint64>(file);
}

struct file_version : reader_intf {
	uint16		version;
	file_version(istream_ref file, uint16 version) : reader_intf(file), version(version) {}
};

//-----------------------------------------------------------------------------
//	OctahedralCoord
//-----------------------------------------------------------------------------

struct Canonical3 {
	int32x3	v;

	static int32x3 Canonicalize(int32x3 v, int max_value) {
		// -max_value <= v <= max_value
		auto	abs_sum	= reduce_add(abs(v));
		if (abs_sum == 0)
			return {max_value, zero, zero};

		auto	xy	= abs_sum > (1 << 29) ? v.xy / (abs_sum / max_value) : (v.xy * max_value) / abs_sum;
		return concat(xy, sign1(v.z) * (max_value - reduce_add(abs(xy))));
	}

	static int32x3 Canonicalize(float3 v, int max_value) {
		auto	abs_sum	= reduce_add(abs(v));
		if (abs_sum < 1e-6)
			return {max_value, zero, zero};

		auto	xy	= to<int>(floor(v.xy / abs_sum * max_value + half));
		auto	z	= max_value - reduce_add(abs(xy));
		if (z < 0) { // if the sum of first two coordinates is too large, we need to decrease the length of one of the coordinates
			xy.y += sign1(xy.y) * z;
			z = 0;
		}
		return concat(xy, sign1(v.z) * z);
	}

	Canonical3(int32x3 v, uint32 max_value) : v(Canonicalize(v, max_value)) {}
	Canonical3(float3 v, int max_value)		: v(Canonicalize(v, max_value)) {}
};

bool	InDiamond(int32x2 v, int centre)	{ return reduce_add(abs(v)) <= centre;	}
bool	InBottomLeft(int32x2 v)				{ return all(v == zero) || (v.x < 0 && v.y <= 0); }
int32x2 MakePositive(int32x2 x, int maxq)	{ return select(x < zero, x + maxq, x); }
int32x2 ModMax(int32x2 v, uint32 centre)	{ return select(v > centre, v - centre * 2 - 1, select(v < -centre, v + centre * 2 + 1, v)); }

int32x2 InvertDiamond(int32x2 v, int centre) {
	int32x2	signs	= all(v >= 0) ? int32x2(one) : all(v <= 0) ? int32x2(-one) : select(v > zero, int32x2(one), -one);
	int32x2 corner = signs * centre;
	auto	r		= (v * 2 - corner).yx;
	if (signs.x * signs.y >= 0)
		r = -r;

	return (r + corner) / 2;
}
int		RotationCount(int32x2 v) {
	return	v.x == 0	? (v.y == 0 ? 0 : v.y > 0 ? 3 : 1)
		:	v.x > 0		? (v.y >= 0 ? 2 : 1)
		:	v.y > 0		? 3
		:	0;
}
int32x2 RotatePoint(int32x2 v, uint32 rotation_count) {
	switch (rotation_count) {
		default:	return v;
		case 1:		return -perp(v);
		case 2:		return -v;
		case 3:		return perp(v);
	}
}

struct OctahedralCoord {
	int32x2 st;	// 0 <= st <= max_value

	static int32x2 Canonicalize(int32x2 v, uint32 centre) {
		uint32	max_value = centre * 2;
	#if 0
		return  all(v == zero | v == int32x2{zero, max_value} | v == int32x2{max_value, zero})	? int32x2(max_value)
			:	(v.x == max_value && v.y < centre)	|| (v.x == zero && v.y > centre)			? int32x2{v.x, max_value - v.y}
			:	(v.y == max_value && v.x < centre)	|| (v.y == zero && v.x > centre)			? int32x2{max_value - v.x, v.y}
			:	v;
	#else
		auto	v2 = max_value - v;
		return all(v == zero) ? int32x2(max_value) : select(v.yx == zero, min(v, v2), select(v.yx == max_value, max(v, v2), v));
	#endif

	}
	static int32x2 To2(Canonical3 v, uint32 centre) {
		return v.v.x >= 0 ? v.v.yz + centre : select(v.v.yz < zero, abs(v.v.zy), centre * 2 - abs(v.v.zy));
	}

	OctahedralCoord(int32x3 v, uint32 centre)		: st(Canonicalize(To2(Canonical3(v, centre), centre), centre)) {}
	OctahedralCoord(Canonical3 v, uint32 centre)	: st(Canonicalize(To2(v, centre), centre)) {}

	float3 ToUnitVector(uint32 centre) {
		auto	v = to<float>(st) / float(centre) - 1;
		float	x = 1 - abs(v.x) - abs(v.y);
		v += sign1(v) * min(x, 0);
		return safe_normalise(concat(x, v));
	}
};

//-----------------------------------------------------------------------------
// Traversal
//
// Corners of face F are defined by unique indices { 3 * F, 3 * F + 1, 3 * F + 2 }
// allowing easy retrieval of Next and Prev corners on any face (see corners N and P)
//
// For every corner we store the index of the opposite corner in the neighbouring face (if it exists)
// (see corner C and its opposite corner O)
//     *
//    /C\
//   / F \
//  /N   P\
// *-------*
//  \     /
//   \   /
//    \O/
//     *
//
// Using the Next, Prev, and Opposite corners then enables traversal of any 2-manifold surface
// For a non-manifold surface, the input non-manifold edges and vertices are automatically split
//-----------------------------------------------------------------------------

inline int Next(int corner)	{ return corner < 0 ? corner : corner % 3 == 2 ? corner - 2 : corner + 1; }
inline int Prev(int corner) { return corner < 0 ? corner : corner % 3 == 0 ? corner + 2 : corner - 1; }

// *-------*-------*
//  \L    /C\    R/
//   \   /   \   /
//    \ /     \ /
//     *-------*
// Get opposite corners on the left and right faces respectively (see image, where L and R are the left and right corners of corner C)
template<typename T> int GetLeftCorner(const T &traversal, int corner)	{ return traversal.Opposite(Prev(corner)); }
template<typename T> int GetRightCorner(const T &traversal, int corner) { return traversal.Opposite(Next(corner)); }

//     *-------*
//    / \     / \
//   /   \   /   \
//  /    L\C/R    \
// *-------*-------*
// Returns the corner on the adjacent face to the left or right respectively that maps to the same vertex as corner C
template<typename T> int SwingLeft(const T &traversal, int corner)		{ return Next(GetRightCorner(traversal, corner)); }
template<typename T> int SwingRight(const T &traversal, int corner)		{ return Prev(GetLeftCorner(traversal, corner)); }

template<typename T> struct CornersT {
	const T&	traversal;
	uint32		corner;

	struct iterator {
		const T&	traversal;
		int32		corner, start_corner;
		bool		right;

		iterator(const T& traversal, int32 corner) : traversal(traversal), corner(corner), start_corner(corner), right(false) {}
		uint32 operator*() const { return corner; }
		bool   operator!=(const iterator& b) const { return corner != b.corner; }
		auto&  operator++() {
			if (right) {
				corner = SwingRight(traversal, corner);
			} else {
				corner = SwingLeft(traversal, corner);
				if (corner < 0) {
					corner = SwingRight(traversal, start_corner);
					right	= true;
				} else if (corner == start_corner) {
					corner = -1;
				}
			}
			return *this;
		}
	};
	CornersT(const T& traversal, uint32 corner) : traversal(traversal), corner(corner) {}
	iterator begin()	const { return {traversal, corner}; }
	iterator end()		const { return {traversal, -1}; }
};

template<typename T> CornersT<T> Corners(const T& traversal, uint32 corner) { return {traversal, corner}; }

//-----------------------------------------------------------------------------
// ANS
//-----------------------------------------------------------------------------

struct Probability {
	uint32	prob, cum_prob;
};

struct ANS {
	enum {BASE	= 4096};
	uint32	state;
	ANS() {}
	ANS(uint32 state) : state(state) {}
};

#ifdef DRACO_ENABLE_READER
struct ANSdecoder : ANS {
	const uint8	*p, *begin;

	ANSdecoder(const const_memory_block &buf, uint32 base = BASE) : begin(buf) {
		const uint8	*end	= buf.end();
		uint8	n			= *--end >> 6;
		end		-=	n;
		state	= base + (
			  n == 0	?	*end & 0x3F
			: n == 1	?	*(uintn<2, false>*)end & 0x3FFF
			: n == 2	?	*(uintn<3, false>*)end & 0x3FFFFF
			:				*(uintn<4, false>*)end & 0x3FFFFFFF
		);
		p	= end;
	}
	bool get_bit(uint32 prob0) {
		uint32	prob = 256 - prob0;
		if (state < BASE)
			state = (state << 8) + *--p;

		uint32	x	= state;
		uint32	quo	= x >> 8;
		uint32	rem	= x & 255;

		if (rem < prob) {
			state = quo * prob + rem;
			return true;
		}
		state = x - quo * prob - prob;
		return false;
	}
	uint32 read(uint32 base, uint32 precision, const uint32 *lut_table, const Probability *probability_table) {
		while (state < base && p > begin)
			state = (state << 8) + *--p;

		uint32	x	= state;
		uint32	quo	= x >> precision;
		uint32	rem = x & bits(precision);

		uint32	symbol = lut_table[rem];
		state = quo * probability_table[symbol].prob + rem - probability_table[symbol].cum_prob;
		return symbol;
	}
};
#endif

#ifdef DRACO_ENABLE_WRITER
struct ANSencoder : ANS {
	uint8	*p, *endp;

	ANSencoder(const memory_block &buf, uint32 base = BASE) : ANS(base), p(buf), endp(buf.end()) {}

	uint8	*end(uint32 base = BASE) {
		state -= base;
		if (state < (1 << 6)) {
			*p++ = state;
		} else if (state < (1 << 14)) {
			*(uintn<2, false>*)p = state + (1 << 14);
			p += 2;
		} else if (state < (1 << 22)) {
			*(uintn<3, false>*)p = state + (2 << 22);
			p += 3;
		} else {
			*(uintn<4, false>*)p = state + (3 << 30);
			p += 4;
		}
		return p;
	}
	void put_bit(bool val, uint32 prob0) {
		uint32	prob	= val ? 256 - prob0 : prob0;

		if (state >= (BASE >> 8) * 256 * prob) {
			*p++ = uint8(state);
			state >>= 8;
		}

		uint32	quo	= state / prob;
		uint32	rem	= state % prob;
		state = (quo << 8) + rem + (val ? 0 : 256 - prob0);
	}
	void write(Probability sym, uint32 base, uint32 precision) {
		const uint32	prob = sym.prob;

		while (state >= (base >> precision) * 256 * prob) {
			*p++ = uint8(state);
			state >>= 8;
		}

		uint32	quo	= state / prob;
		uint32	rem	= state % prob;
		state = (quo << precision) + rem + sym.cum_prob;
	}
};
#endif

struct ProbBuffer {
	malloc_block	buffer;
	uint8			prob_zero;

#ifdef DRACO_ENABLE_READER
	ProbBuffer()	{}
	ProbBuffer(file_version &file) { read((istream_ref&)file, file.version); }
	ProbBuffer(istream_ref file, uint16 version) { read(file, version); }

	bool	read(istream_ref file, uint16 version) {
		prob_zero	= file.getc();
		buffer		= malloc_block(file, get_uint32(file, version < VER_LEB128_ANS));
		return true;
	}

	dynamic_bitarray<>	decode(uint32 num_values) {
		ANSdecoder		ans_decoder(buffer);
		dynamic_bitarray<>	bits(num_values);
		for (auto &&i : bits)
			i = ans_decoder.get_bit(prob_zero);
		return bits;
	}
#endif

#ifdef DRACO_ENABLE_WRITER
	ProbBuffer(const range<bit_pointer<const uint32>> &r) : buffer(r.size() * 8 + 8), prob_zero(clamp((r.count_clear() * 256 + 128) / max(r.size(), 1), 1, 255)) {
		ANSencoder	encoder(buffer);
		for (bool bit : reversed(r))
			encoder.put_bit(bit, prob_zero);
		buffer.resize(encoder.end() - buffer.begin());
	}

	bool	write(ostream_ref file) const {
		file.putc(prob_zero);
		write_leb128(file, buffer.length());
		return buffer.write(file);
	}
#endif
};

//-----------------------------------------------------------------------------
// Symbol coding
//-----------------------------------------------------------------------------

enum {
	TAGGED_SYMBOLS			= 0,
	RAW_SYMBOLS				= 1,
	RAW_MAX_BITS			= 18,
	TAGGED_RANS_BASE		= 16384,
	TAGGED_RANS_PRECISION	= 12,
};

#ifdef DRACO_ENABLE_READER
temp_array<Probability> BuildSymbolTables(istream_ref file, uint32 num_symbols, uint32 *lut_table) {
	temp_array<Probability>	probability_table(num_symbols);

	for (auto *p = probability_table.begin(), *e = probability_table.end(); p < e;) {
		uint32	prob	= file.getc();
		auto	token	= prob & 3;
		if (token == 3) {
			for (uint32 n = (prob >> 2) + 1; n--;)
				p++->prob = 0;

		} else {
			for (uint32 j = 0; j < token; ++j)
				prob |= file.getc() << (8 * (j + 1));
			p++->prob = prob >> 2;
		}
	}

	uint32	cum_prob = 0;
	for (auto &i : probability_table) {
		i.cum_prob	= cum_prob;
		cum_prob	+= i.prob;
		for (uint32	j = i.cum_prob, x = probability_table.index_of(i); j < cum_prob; ++j)
			lut_table[j] = x;
	}
	return probability_table;
}

dynamic_array<uint32> DecodeSymbols(istream_ref file, uint32 num_symbols, uint32 num_components, uint16 version) {
	dynamic_array<uint32>	result(num_symbols);

	if (num_symbols) {
		switch (file.getc()) {
			case TAGGED_SYMBOLS: {
				temp_array<uint32>	lut_table(1 << TAGGED_RANS_PRECISION);
				auto				probability_table = BuildSymbolTables(file, get_uint32(file, version < VER_LEB128), lut_table);
				malloc_block		encoded_data(file, get_uint64(file, version < VER_LEB128));
				ANSdecoder			ans_decoder(encoded_data, TAGGED_RANS_BASE);

				vlc_in<uint32, false>	vlc(file);
				for (auto p = result.begin(), e = result.end(); p != e; ) {
					uint32	size = ans_decoder.read(TAGGED_RANS_BASE, TAGGED_RANS_PRECISION, lut_table, probability_table);
					for (int j = 0; j < num_components; ++j)
						*p++ = vlc.get(size);
				}
				vlc.restore_unused();
				break;
			}
			case RAW_SYMBOLS: {
				auto	max_bit_length	= file.getc();
				auto	rans_precision	= clamp((3 * max_bit_length) / 2, 12, 20);
				auto	rans_base		= 4 << rans_precision;

				temp_array<uint32>	lut_table(1 << rans_precision);
				auto				probability_table	= BuildSymbolTables(file, get_uint32(file, version < VER_LEB128), lut_table);
				malloc_block		buffer(file, get_leb128<uint64>(file));
				ANSdecoder			ans_decoder(buffer, rans_base);

				for (auto &i : result)
					i = ans_decoder.read(rans_base, rans_precision, lut_table, probability_table);
				break;
			}
		}
	}
	return result;
}

template<class T> constexpr auto SymbolToSigned(T i) { return signed_t<T>((i >> 1) ^ -(i & 1)); }

dynamic_array<int32> DecodeIntegerValues(istream_ref file, uint32 num_values, uint32 num_components, bool is_signed, uint16 version) {
	auto	total_values	= num_values * num_components;
	dynamic_array<int32>	values;

	if (file.getc()) {
		// Compressed
		values = DecodeSymbols(file, total_values, num_components, version);

	} else {
		// Decode the integer data directly
		switch (file.getc()) {
			case 1:	values = temp_array<uint8>		(file, total_values); break;
			case 2:	values = temp_array<uint16le>	(file, total_values); break;
			case 3:	values = temp_array<uint24le>	(file, total_values); break;
			case 4:	values = temp_array<uint32le>	(file, total_values); break;
		}
	}

	if (is_signed) {
		// Convert the values back to the original signed format
		for (auto &i : values)
			i = SymbolToSigned(uint32(i));
	}
	return values;
}
#endif

#ifdef DRACO_ENABLE_WRITER
temp_array<Probability> BuildSymbolTables(ostream_ref file, uint32 log2precision, uint64 *frequencies, uint32 num_symbols) {
	while (num_symbols && frequencies[num_symbols - 1] == 0)
		--num_symbols;

	// compute total of the input frequencies
	uint64	total_freq			= 0;
	for (int i = 0; i < num_symbols; ++i)
		total_freq += frequencies[i];
	
	temp_array<Probability>	probability_table(num_symbols);

	// compute probabilities by rescaling the frequencies into interval [1, precision - 1]
	uint32 total_prob = 0;
	for (int i = 0; i < num_symbols; ++i) {
		const uint64 freq = frequencies[i];
		uint32 prob = div_round(freq << log2precision, total_freq);
		if (prob == 0 && freq > 0)
			prob = 1;

		probability_table[i].prob = prob;
		total_prob += prob;
	}

	// because of rounding errors, the total precision may not be exactly accurate and we may need to adjust the entries a little bit
	auto	precision	= 1 << log2precision;
	if (total_prob != precision) {
		temp_array<int> sorted_probabilities = int_range(num_symbols);
		sort(sorted_probabilities, [&probs = probability_table](int i, int j) { return probs[i].prob < probs[j].prob; });

		if (total_prob < precision) {
			// under-allocated: add the extra needed precision to the most frequent symbol
			probability_table[sorted_probabilities.back()].prob += precision - total_prob;

		} else {
			// over-allocated; rescale the probabilities of all symbols
			for (uint32 error = total_prob - precision; error;) {
				for (int j = num_symbols; error && j--;) {
					auto&	prob		= probability_table[sorted_probabilities[j]].prob;
					int32	new_prob	= (prob << log2precision) / total_prob;
					int32	fix			= clamp(prob - new_prob, 1, min(prob - 1, error));
					prob	-= fix;
					error	-= fix;
				}
			}
		}
	}

	// cumulative probability
	total_prob = 0;
	for (auto &i : probability_table) {
		i.cum_prob = total_prob;
		total_prob += i.prob;
	}

	// write table
	write_leb128(file, num_symbols);
	for (auto *p = probability_table.begin(), *e = probability_table.end(); p < e;) {
		uint32 prob = p->prob;
		if (prob == 0) {
			auto	p0 = p++;
			while (p - p0 < (1 << 6) && p->prob == 0)
				++p;
			file.putc(((p - p0 - 1) << 2) | 3);

		} else {
			for (uint32 j = 0, n = (prob >= (1 << 6)) + (prob >= (1 << 14)), v = (prob << 2) + n; j <= n; ++j, v >>= 8)
				file.putc(v);
			++p;
		}
	}

	return probability_table;
}

class log2accum {
	int		i;
	uint32	f;
	constexpr log2accum(int i, uint32 f) : i(i), f(f) {}
public:
	constexpr log2accum(uint32 x) : i(highest_set_index(x)), f(x << (31 - i)) {}
	constexpr log2accum	operator-()			const { return f == (1 << 31) ? log2accum(-i, f) : log2accum(~i, uint32((1ull << 63) / f)); }
	constexpr log2accum	operator+(int p)	const { return {i + p, f}; }
	constexpr log2accum	operator-(int p)	const { return {i - p, f}; }
	constexpr int		approx()			const { return i; }
	constexpr int		ceil()				const { return i + int(f != (1 << 31)); }

	log2accum&	operator+=(const log2accum &b) {
		i += b.i;
		uint64	f2 = fullmul(f, b.f);
		if (f2 >= (1ull << 63)) {
			++i;
			f2 >>= 1;
		}
		f = f2 >> 31;
		return *this;
	}
	log2accum	operator*(uint32 p) const {
		log2accum	v = *this;
		log2accum	r = p & 1 ? v : 1;
		while (p >>= 1) {
			v += v;
			if (p & 1)
				r += v;
		}
		return r;
	}
	constexpr log2accum	operator+(const log2accum &b)	const { return log2accum(*this) += b; }
	constexpr log2accum	operator-(const log2accum &b)	const { return log2accum(*this) += -b; }
};

uint64 CalcExpectedBits(const Probability *probs, const uint64 *frequencies, uint32 num_freq, uint32 log2precision) {
	log2accum	total_bits(1);
	for (int i = 0; i < num_freq; ++i) {
		if (probs[i].prob)
			total_bits += (log2accum(probs[i].prob) - log2precision) * frequencies[i];
	}
	return -total_bits.approx();
}

int64 ComputeShannonEntropyBits(const uint64 *frequencies, uint32 num_freq, uint32 *out_num_unique) {
	uint32		num_unique = 0, num_symbols = 0;
	log2accum	norm(1);
	for (int i = 0; i < num_freq; ++i) {
		if (auto freq = frequencies[i]) {
			++num_unique;
			num_symbols += freq;
			norm		+= log2accum(frequencies[i]) * freq;
		}
	}
	if (out_num_unique)
		*out_num_unique = num_unique;

	return -(norm - log2accum(num_symbols) * num_symbols).approx();
}

int64 ComputeBinaryShannonEntropyBits(uint32 num_values, uint32 num_true) {
	if (num_values == 0 || num_true == 0 || num_values == num_true)
		return 0;
	uint32		num_false	= num_values - num_true;
	log2accum	log2_rnum	= -log2accum(num_values);
	return -((log2accum(num_true) + log2_rnum) * num_true + (log2accum(num_false) + log2_rnum) * num_false).approx();
}

int64 ApproximateRAnsFrequencyTableBits(int32 max_value, int num_unique) {
	return (num_unique * 2 + (max_value - num_unique) / 64) * 8;
}

class EntropyData {
	log2accum	entropy_norm		= 1;
	uint32		num_values			= 0;
	uint32		max_symbol			= 0;
	uint32		num_unique_symbols	= 0;
public:
	void	UpdateSymbols(const uint32 *symbols, int num_symbols, dynamic_array<uint32> &frequencies);
	int64	GetNumberOfDataBits()		const { return num_values < 2 ? 0 : -(entropy_norm - log2accum(num_values) * num_values).approx(); }
	int64	GetNumberOfRAnsTableBits()	const { return ApproximateRAnsFrequencyTableBits(max_symbol + 1, num_unique_symbols); }
};
void EntropyData::UpdateSymbols(const uint32 *symbols, int num_symbols, dynamic_array<uint32> &frequencies) {
	num_values += num_symbols;

	for (uint32 i = 0; i < num_symbols; ++i) {
		const uint32 symbol = symbols[i];
		if (symbol >= frequencies.size())
			frequencies.resize(symbol + 1, 0);

		if (auto oldfreq = frequencies[symbol]++) {
			entropy_norm += log2accum(oldfreq + 1) * (oldfreq + 1) - log2accum(oldfreq) * oldfreq;

		} else {
			++num_unique_symbols;
			max_symbol = max(max_symbol, symbol);
		}
	}
}

class ShannonEntropyTracker : public EntropyData {
	dynamic_array<uint32>	frequencies;
public:
	EntropyData Push(const uint32* symbols, uint32 num_symbols) {
		UpdateSymbols(symbols, num_symbols, frequencies);
		return *this;
	}
	EntropyData Peek(const uint32 *symbols, uint32 num_symbols) {
		EntropyData	ret = *this;
		ret.UpdateSymbols(symbols, num_symbols, frequencies);
		// revert changes in the frequency table
		for (uint32 i = 0; i < num_symbols; ++i)
			--frequencies[symbols[i]];
		return ret;
	}
};

bool EncodeSymbols(ostream_ref file, const dynamic_array<uint32> &symbols, uint32 num_components, MODE mode) {
	if (auto num_symbols = symbols.size32()) {
		if (num_components <= 0)
			num_components = 1;

		uint32		num_values	= num_symbols / num_components;
		uint32		max_value	= 0;

		temp_array<uint32> bit_lengths(num_values);
		auto		*p = symbols.begin();
		for (auto &i : bit_lengths) {
			uint32 max_component = *p++;
			for (int n = num_components; --n;)
				max_component = max(max_component, *p++);

			max_value	= max(max_value, max_component);
			i			= highest_set_index(max(max_component, 1)) + 1;
		}

		uint64	bit_length_frequencies[32] = {0};
		for (auto i : bit_lengths)
			++bit_length_frequencies[i];

		temp_array<uint64> symbol_frequencies(max_value + 1, 0);
		for (auto i : symbols)
			++symbol_frequencies[i];

		uint32	num_unique_symbols;
		// Approximate number of bits needed for storing the symbols using the tagged scheme
		auto	tagged_bits	= ComputeShannonEntropyBits(bit_length_frequencies, 32, &num_unique_symbols)
							+ reduce<op_add>(bit_lengths) * num_components
							+ ApproximateRAnsFrequencyTableBits(num_unique_symbols, num_unique_symbols);
		// Approximate number of bits needed for storing the symbols using the raw scheme
		auto	raw_bits	= ComputeShannonEntropyBits(symbol_frequencies, max_value + 1, &num_unique_symbols)
							+ ApproximateRAnsFrequencyTableBits(max_value, num_unique_symbols);

		if (tagged_bits < raw_bits || highest_set_index(max(max_value, 1)) + 1 > RAW_MAX_BITS) {
			file.putc(TAGGED_SYMBOLS);

			auto		probs		= BuildSymbolTables(file, TAGGED_RANS_PRECISION, bit_length_frequencies, 32);
			temp_block	buffer((2 * CalcExpectedBits(probs, bit_length_frequencies, 32, TAGGED_RANS_PRECISION) + 32 + 7) / 8);
			ANSencoder	ans_coder(buffer, TAGGED_RANS_BASE);

			for (auto i : reversed(bit_lengths))
				ans_coder.write(probs[i], TAGGED_RANS_BASE, TAGGED_RANS_PRECISION);

			auto bytes_written = uint32(ans_coder.end(TAGGED_RANS_BASE) - buffer);
			write_leb128(file, bytes_written);
			file.writebuff(buffer, bytes_written);

			vlc_out<uint32, false>	vlc(file);
			auto p = symbols.begin();
			for (auto i : bit_lengths) {
				for (int c = 0; c < num_components; ++c)
					vlc.put(*p++, i);
			}
			vlc.flush();

		} else {
			// Adjust the bit_length based on compression level
			uint32		symbol_bits		= clamp(highest_set_index(max(num_unique_symbols, 1)) + 1 + SYMBOL_BITS_ADJUST(mode), 1, RAW_MAX_BITS);

			file.putc(RAW_SYMBOLS);
			file.putc(symbol_bits);

			auto		rans_precision	= clamp((3 * symbol_bits) / 2, 12, 20);
			auto		rans_base		= 4 << rans_precision;
			auto		probs			= BuildSymbolTables(file, rans_precision, symbol_frequencies, max_value + 1);
			temp_block	buffer((2 * CalcExpectedBits(probs, symbol_frequencies, max_value + 1, rans_precision) + 32 + 7) / 8);
			ANSencoder	ans_coder(buffer, rans_base);

			for (auto i : reversed(symbols))
				ans_coder.write(probs[i], rans_base, rans_precision);

			auto bytes_written = uint32(ans_coder.end(rans_base) - buffer);
			write_leb128(file, bytes_written);
			file.writebuff(buffer, bytes_written);
		}
	}
	return true;
}

template<class T> constexpr auto SignedToSymbol(T i) { return unsigned_t<T>(i < 0 ? ~(i << 1) : (i << 1)); }

bool EncodeIntegerValues(ostream_ref file, range<int32*> values, uint32 num_components, bool is_signed, MODE mode) {
	dynamic_array<uint32>	symbols;
	if (is_signed)
		symbols = transformc(values, [](int i) { return SignedToSymbol(i); });
	else
		symbols = values;

	bool	compressed = mode & COMPRESS_INTEGERS;
	file.putc(compressed);
	if (compressed)
		return EncodeSymbols(file, symbols, num_components, mode);

	// Encode the integer data directly
	auto	max = reduce<op_or>(symbols);
	return	max < (1 << 8)	?	file.putc(1) && temp_array<uint8>		(values).write(file)
		:	max < (1 << 16)	?	file.putc(2) && temp_array<uint16le>	(values).write(file)
		:	max < (1 << 24) ?	file.putc(3) && temp_array<uint24le>	(values).write(file)
		:						file.putc(4) && temp_array<uint32le>	(values).write(file);
}
#endif

//-----------------------------------------------------------------------------
// Coding - Sequential attribute encoding methods
//-----------------------------------------------------------------------------

// INTEGER

template<> struct Coding::T<Coding::INTEGER> {
	template<typename D> struct T2 : Coding {
		uint32	num_components;
	#ifdef DRACO_ENABLE_READER
		void	Decode(void *out, const int32 *in, uint32 num_values) override {
			copy_n(in, (D*)out, num_values * num_components);
		}
	#endif
	#ifdef DRACO_ENABLE_WRITER
		void	Encode(int32 *out, const void *in, uint32 num_values) override {
			copy_n((D*)in, out, num_values * num_components);
		}
	#endif
		T2(uint32 num_components) : num_components(num_components) {}
	};
	static Coding *Get(uint32 num_components, DataType type) {
		switch (type) {
			case DT_INT8:	return new T2<int8	>(num_components); break;
			case DT_UINT8:	return new T2<uint8	>(num_components); break;
			case DT_INT16:	return new T2<int16	>(num_components); break;
			case DT_UINT16:	return new T2<uint16>(num_components); break;
			case DT_INT32:	return new T2<int32	>(num_components); break;
			case DT_UINT32:	return new T2<uint32>(num_components); break;
			case DT_INT64:	return new T2<int64	>(num_components); break;
			case DT_UINT64:	return new T2<uint64>(num_components); break;
			case DT_BOOL:	return new T2<bool8	>(num_components); break;
			default:		return nullptr;
		}
	}
};

// for PointCloud KD-tree

template<> struct Coding::T<Coding::KD_INTEGER> : Coding {
	dynamic_array<int>	min_values;
	uint32				used_bits;

	static Coding *Get(uint32 num_components, DataType type);

#ifdef DRACO_ENABLE_READER
	bool	Read(istream_ref file) override {
		for (auto &i : min_values)
			i = get_leb128<int>(file);
		return true;
	}
#endif
#ifdef DRACO_ENABLE_WRITER
	bool	Write(ostream_ref file) override {
		for (auto &i : min_values)
			write_leb128(file, uint32(i));
		return true;
	}
#endif
	T(uint32 num_components) : min_values(num_components, maximum) {}
};

template<typename D> struct KDIntCodingType : Coding::T<Coding::KD_INTEGER> {
	uint32	num_components;
#ifdef DRACO_ENABLE_READER
	void	Decode(void *out, const int32 *in, uint32 num_values) override {
		D	*out2	= (D*)out;
		for (uint32 i = 0; i < num_values; ++i) {
			for (auto min : min_values)
				*out2++ = D(*in++ + min);
		}
	}
#endif
#ifdef DRACO_ENABLE_WRITER
	void	Encode(int32 *out, const void *in, uint32 num_values) override {
		auto in2 = (const D*)in;
		for (uint32 i = 0; i < num_values; ++i) {
			for (auto &i : min_values)
				i = min(i, *in2++);
		}
		in2		= (const D*)in;
		uint32	bits = 0;
		for (uint32 i = 0; i < num_values; ++i) {
			for (auto min : min_values)
				bits |= (*out++ = *in2++ - min);
		}
		used_bits = bits;
	}
#endif
	KDIntCodingType(uint32 num_components) : Coding::T<Coding::KD_INTEGER>(num_components) {}
};

Coding *Coding::T<Coding::KD_INTEGER>::Get(uint32 num_components, DataType type) {
	switch (type) {
		case DT_INT8:	return new KDIntCodingType<int8>(num_components);
		case DT_UINT8:	return new KDIntCodingType<uint8>(num_components);
		case DT_INT16:	return new KDIntCodingType<int16>(num_components);
		case DT_UINT16:	return new KDIntCodingType<uint16>(num_components);
		case DT_INT32:	return new KDIntCodingType<int32>(num_components);
		case DT_UINT32:	return new KDIntCodingType<uint32>(num_components);
		case DT_INT64:	return new KDIntCodingType<int64>(num_components);
		case DT_UINT64:	return new KDIntCodingType<uint64>(num_components);
		case DT_BOOL:	return new KDIntCodingType<bool8>(num_components);
		default:		return nullptr;
	}
}

// QUANTIZATION

template<> struct Coding::T<Coding::QUANTIZATION> : Coding {
	template<int N> struct T2 : Coding {
		float	min_values[N];
		float	range;
		uint8	quantization_bits;
	#ifdef DRACO_ENABLE_READER
		bool	Read(istream_ref file) override {
			return read(file, min_values, range, quantization_bits);
		}
		void	Decode(void *out, const int32 *in, uint32 num_values) override {
			auto	vmin	= load<vec<float, N>>(min_values);
			float	factor	= range / bits(quantization_bits);
			for (auto out2 = (float*)out, end = out2 + num_values * N; out2 < end; out2 += N, in += N)
				store(to<float>(load<vec<int32, N>>(in)) * factor + vmin, out2);
		}
	#endif
	#ifdef DRACO_ENABLE_WRITER
		bool	Write(ostream_ref file) override {
			return file.write(min_values, range, quantization_bits);
		}
		void	Encode(int32 *out, const void *in, uint32 num_values) override {
			vec<float, N>	vmin(infinity), vmax(-infinity);
			for (auto in2 = (float*)in, end = in2 + num_values * N; in2 < end; in2 += N) {
				auto	v = load<N>(in2);
				vmin = min(vmin, v);
				vmax = max(vmax, v);
			}
			store(vmin, min_values);
			range	= reduce_max(vmax - vmin);

			float	rfactor	= bits(quantization_bits) / range;
			for (auto in2 = (float*)in, end = in2 + num_values * N; in2 < end; in2 += N, out += N)
				store(to<int32>((load<N>(in2) - vmin) * rfactor + half), out);
		}
	#endif
		T2(int quantisation) : quantization_bits(quantisation) {}
	};

	static Coding *Get(uint32 num_components, int quantisation) {
		switch (num_components) {
			case 1:		return new T2<1>(quantisation); break;
			case 2:		return new T2<2>(quantisation); break;
			case 3:		return new T2<3>(quantisation); break;
			case 4:		return new T2<4>(quantisation); break;
			default:	return new T(num_components, quantisation);
		}
	}

	dynamic_array<float>	min_values;
	float	range;
	uint8	quantization_bits;
	
	T(uint32 num_components, int quantisation) : min_values(num_components, infinity) {}

#ifdef DRACO_ENABLE_READER
	bool	Read(istream_ref file) override {
		return read(file, min_values, range, quantization_bits);
	}
	void	Decode(void *out, const int32 *in, uint32 num_values) override {
		float	factor	= range / bits(quantization_bits);
		float	*out2	= (float*)out;
		for (uint32 i = 0; i < num_values; ++i) {
			for (auto min : min_values)
				*out2++ = *in++ * factor + min;
		}
	}
#endif
#ifdef DRACO_ENABLE_WRITER
	bool	Write(ostream_ref file) override {
		return file.write(min_values, range, quantization_bits);
	}
	void	Encode(int32 *out, const void *in, uint32 num_values) override {
		uint32	num_components = min_values.size32();
		dynamic_array<float>	max_values(num_components, -infinity);
		auto	in2 = (float*)in;
		for (uint32 i = 0; i < num_values; ++i) {
			for (uint32 i = 0; i < num_components; i++) {
				float	v = *in2++;
				min_values[i] = min(min_values[i], v);
				max_values[i] = max(max_values[i], v);
			}
		}
		range = 0;
		for (uint32 i = 0; i < num_components; i++)
			range = max(range, max_values[i] - min_values[1]);

		float	rfactor	= bits(quantization_bits) / range;
		in2		= (float*)in;
		for (uint32 i = 0; i < num_values; ++i) {
			for (auto min : min_values)
				*out++ = (*in2++ - min)* rfactor;
		}
	}
#endif
};

// for legacy PointCloud KD-tree

template<> struct Coding::T<Coding::KD_FLOAT3> : Coding {
	float	range;
	uint32	quantization_bits;

#ifdef DRACO_ENABLE_READER
	bool	Read(istream_ref file) override {
		return file.read(quantization_bits) && quantization_bits < 31 && file.read(range);
	}
	void	Decode(void* out, const int32* in, uint32 num_values) override {
		int		offset	= bits(quantization_bits);
		float	factor	= range / offset;
		float	*out2	= (float*)out;
		for (uint32 i = 0; i < num_values * 3; ++i)
			*out2++ = ((int)*in++ - offset) * factor;
	}
#endif
};

// NORMALS

template<> struct Coding::T<Coding::NORMALS> : Coding {
	uint8	quantization_bits;

#ifdef DRACO_ENABLE_READER
	bool	Read(istream_ref file) override {
		return file.read(quantization_bits);
	}
	void	Decode(void* out, const int32* in, uint32 num_values) override {
		auto	in2		= (OctahedralCoord*)in;
		uint32	centre	= bits(quantization_bits - 1);
		for (auto out2 = (float*)out, end = out2 + num_values * 3; out2 != end; ++in2, out2 += 3)
			store(in2->ToUnitVector(centre), out2);
	}
#endif
#ifdef DRACO_ENABLE_WRITER
	bool	Write(ostream_ref file) override {
		return file.write(quantization_bits);
	}
	void	Encode(int32 *out, const void *in, uint32 num_values) override {
		uint32	centre	= bits(quantization_bits - 1);
		auto	out2	= (OctahedralCoord*)out;
		for (auto in1 = (float*)in, end = in1 + num_values * 3; in1 < end; in1 += 3, ++out2)
			*out2	= OctahedralCoord(Canonical3(load<3>(in1), centre), centre);
	}
#endif
	T(int quantisation) : quantization_bits(quantisation) {}
};

Coding* Coding::Get(Method method, uint32 num_components, DataType type, int quantisation) {
	switch (method) {
		default:			return nullptr;
		case INTEGER:		return T<INTEGER>::Get(num_components, type);
		case QUANTIZATION:	ISO_ASSERT(type == DT_FLOAT32);	return T<QUANTIZATION>::Get(num_components, quantisation);
		case NORMALS:		ISO_ASSERT(type == DT_FLOAT32);	return new T<NORMALS>(quantisation);
		case KD_INTEGER:	return T<KD_INTEGER>::Get(num_components, type);
		case KD_FLOAT3:		return new T<KD_FLOAT3>;
	}
}

//-----------------------------------------------------------------------------
// PredictionTransform - prediction scheme transform methods
//-----------------------------------------------------------------------------

// DELTA -  Basic delta transform where the prediction is computed as difference between the predicted and original value

template<> struct PredictionTransform::T<PredictionTransform::DELTA> : PredictionTransform {
	uint32 num_components;
	T(uint32 num_components) : num_components(num_components) {}
#ifdef DRACO_ENABLE_READER
	void Decode(const int32 *pred, const int32 *corr, int32 *out) override {
		for (uint32 i = 0; i < num_components; ++i)
			out[i] = pred[i] + corr[i];
	}
#endif
#ifdef DRACO_ENABLE_WRITER
	void Encode(const int32 *pred, const int32 *in, int32 *out) override {
		for (uint32 i = 0; i < num_components; ++i)
			out[i] = in[i] - pred[i];
	}
#endif
};

// WRAP	- An improved delta transform where all computed delta values are wrapped around a fixed interval which lowers the entropy

template<> struct PredictionTransform::T<PredictionTransform::WRAP> : PredictionTransform {
	int	wrap_min, wrap_max;
	uint32 num_components;

	T(uint32 num_components) : num_components(num_components) {}

#ifdef DRACO_ENABLE_READER
	bool Read(istream_ref file, uint16 version) override {
		return read(file, wrap_min, wrap_max);
	}
	void Decode(const int32 *pred, const int32 *corr, int32 *out) override {
		auto	dif = wrap_max - wrap_min + 1;
		for (uint32 i = 0; i < num_components; ++i) {
			auto	x = clamp(pred[i], wrap_min, wrap_max) + corr[i];
			out[i] = x > wrap_max ? x - dif : x < wrap_min ? x + dif : x;
		}
	}
#endif
#ifdef DRACO_ENABLE_WRITER
	bool Write(ostream_ref file) override {
		return file.write(wrap_min, wrap_max);
	}
	void Encode(const int32 *pred, const int32 *in, int32 *out) override {
		auto	dif = wrap_max - wrap_min + 1;
		auto	max_correction = dif / 2;
		auto	min_correction = ~max_correction + (dif & 1);

		for (uint32 i = 0; i < num_components; ++i) {
			auto	x = in[i] - clamp(pred[i], wrap_min, wrap_max);
			out[i] = x > max_correction ? x - dif : x < min_correction ? x + dif : x;
		}
	}
	void Init(const int32 *in, uint32 num_values) override {
		int32	min_value = in[0], max_value = min_value;
		for (int i = 1; i < num_components * num_values; ++i) {
			min_value	=	min(min_value, in[i]);
			max_value	=	max(max_value, in[i]);
		}
		wrap_min	= min_value;
		wrap_max	= max_value;
	}
#endif
};

// NORMAL_OCTAHEDRON (deprecated)

template<> struct PredictionTransform::T<PredictionTransform::NORMAL_OCTAHEDRON> : PredictionTransform {
	int32	maxq;
	uint32	centre;
#ifdef DRACO_ENABLE_READER
	bool Read(istream_ref file, uint16 version) override {
		file.read(maxq);
		if (version < VER_REMOVE_UNUSED)
			file.read(centre);
		centre	= bits(highest_set_index(maxq));
		(void)file.get<int32>();
		return true;
	}
	void Decode(const int32 *pred, const int32 *corr, int32 *out) override {
		auto	pred1	= load<int32x2>(pred) - centre;

		int32x2	corr1	= load<int32x2>(corr);
		int32x2	result	= InDiamond(pred1, centre)
			? ModMax(pred1 + corr1, centre)
			: InvertDiamond(ModMax(InvertDiamond(pred1, centre) + corr1, centre), centre);

		store(result + centre, out);
	}
#endif
};

// NORMAL_OCTAHEDRON_CANONICALIZED

template<> struct PredictionTransform::T<PredictionTransform::NORMAL_OCTAHEDRON_CANONICALIZED> : PredictionTransform {
	int32	maxq;
	uint32	centre;

#ifdef DRACO_ENABLE_READER
	static int32x2 DecodeInDiamond(int32x2 corr, int32x2 pred, uint32 centre) {
		if (InBottomLeft(pred))
			return ModMax(pred + corr, centre);
		auto	rot	= RotationCount(pred);
		return RotatePoint(ModMax(RotatePoint(pred, rot) + corr, centre), -rot & 3);
	}

	bool Read(istream_ref file, uint16 version) override {
		file.read(maxq);
		centre	= bits(highest_set_index(maxq));
		(void)file.get<int32>();
		return true;
	}
	void Decode(const int32 *pred, const int32 *corr, int32 *out) override {
		auto	pred1	= load<int32x2>(pred) - centre;
		int32x2	corr1	= load<int32x2>(corr);
		int32x2	result	= InDiamond(pred1, centre)
			? DecodeInDiamond(corr1, pred1, centre)
			: InvertDiamond(DecodeInDiamond(corr1, InvertDiamond(pred1, centre), centre), centre);

		store(result + centre, out);
	}
#endif
#ifdef DRACO_ENABLE_WRITER
	bool Write(ostream_ref file) override {
		file.write(maxq);
		file.write(0);
		return true;
	}
	void Encode(const int32 *pred, const int32 *in, int32 *out) override {
		auto	pred1	= load<int32x2>(pred) - centre;
		auto	orig1	= load<int32x2>(in) - centre;
		if (!InDiamond(pred1, centre)) {
			orig1 = InvertDiamond(orig1, centre);
			pred1 = InvertDiamond(pred1, centre);
		}
		if (!InBottomLeft(pred1)) {
			auto	rot = RotationCount(pred1);
			orig1 = RotatePoint(orig1, rot);
			pred1 = RotatePoint(pred1, rot);
		}
		store(MakePositive(orig1 - pred1, maxq), out);
	}
	void Init(const int32 *in, uint32 num_values) override {
		maxq	= reduce<op_or>(make_range_n(in, num_values * 2));
		centre	= bits(highest_set_index(maxq));
	}
#endif
};

PredictionTransform* PredictionTransform::Get(Method method, uint32 num_components) {
	switch (method) {
		case DELTA:								return new T<DELTA>(num_components);
		case WRAP:								return new T<WRAP>(num_components);
		case NORMAL_OCTAHEDRON:					return new T<NORMAL_OCTAHEDRON>();
		case NORMAL_OCTAHEDRON_CANONICALIZED:	return new T<NORMAL_OCTAHEDRON_CANONICALIZED>();
		default:	return nullptr;
	}
}

//-----------------------------------------------------------------------------
// Prediction - prediction encoding methods
//-----------------------------------------------------------------------------

struct Traversal {
	const int32		*opposite_corners;
	bit_pointer<const uint32>	is_edge_on_seam;
	const int32		*corner_to_value;
	const uint32	*value_to_corner;
	const int32		*corner_to_pos;
	const int32		*pos_values;

	int		Opposite(uint32 corner)				const { return is_edge_on_seam && is_edge_on_seam[corner] ? -1 : opposite_corners[corner]; }
	uint32	ValueToCorner(uint32 value)			const { return value_to_corner ? value_to_corner[value] : value; }
	int32x3 GetPositionForCorner(uint32 corner) const { return load<int32x3>(pos_values + corner_to_pos[corner] * 3); }
	int32x3 GetPositionForValue(uint32 value)	const { return GetPositionForCorner(ValueToCorner(value)); }
	int		CornerToValue(uint32 corner)		const { return corner_to_value[corner]; }

	Traversal(
		const int32 *opposite_corners, bit_pointer<const uint32> is_edge_on_seam,
		const int32 *corner_to_value, const uint32* value_to_corner,
		const int32 *corner_to_pos, const int32 *pos_values
	) : opposite_corners(opposite_corners), is_edge_on_seam(is_edge_on_seam),
		corner_to_value(corner_to_value), value_to_corner(value_to_corner),
		corner_to_pos(corner_to_pos), pos_values(pos_values)
	{}
};

// DIFFERENCE

template<> struct Prediction::T<Prediction::DIFFERENCE> : Prediction {
#ifdef DRACO_ENABLE_READER
	void Decode(const Traversal& traversal, int* values, uint32 num_components, uint32 num_values) override {
		temp_array<int32>	zero_vals(num_components, 0);
		transform->Decode(zero_vals, values, values);
		for (uint32 i = num_components; i < num_values * num_components; i += num_components)
			transform->Decode(values + (i - num_components), values + i, values + i);
	}
#endif
#ifdef DRACO_ENABLE_WRITER
	void Encode(const Traversal &traversal, int *values, uint32 num_components, uint32 num_values) override {
		for (uint32 i = num_values * num_components; i -= num_components;)
			transform->Encode(values + (i - num_components), values + i, values + i);
		temp_array<int32>	zero_vals(num_components, 0);
		transform->Encode(zero_vals, values, values);
	}
#endif
};

// PARALLELOGRAM

template<> struct Prediction::T<Prediction::PARALLELOGRAM> : Prediction {
	static bool ComputeParallelogramPrediction(uint32 vert_opp, uint32 vert_next, uint32 vert_prev, uint32 data_id, int32 *data, uint32 num_components, int32 *pred) {
		if (vert_opp < data_id && vert_next < data_id && vert_prev < data_id) {
			auto	opp_ptr	 = data + vert_opp * num_components;
			auto	next_ptr = data + vert_next * num_components;
			auto	prev_ptr = data + vert_prev * num_components;
			for (uint32 c = 0; c < num_components; ++c)
				pred[c] = (next_ptr[c] + prev_ptr[c]) - opp_ptr[c];
			return true;
		}
		return false;
	}
	static bool ComputeParallelogramPrediction(int corner, const int *corner_to_value, uint32 data_id, int32 *data, uint32 num_components, int32 *pred) {
		return corner >= 0
			&& ComputeParallelogramPrediction(corner_to_value[corner], corner_to_value[Next(corner)], corner_to_value[Prev(corner)], data_id, data, num_components, pred);
	}

#ifdef DRACO_ENABLE_READER
	void Decode(const Traversal& traversal, int* values, uint32 num_components, uint32 num_values) override {
		temp_array<int32>	pred_vals(num_components, 0);
		transform->Decode(pred_vals, values, values);

		for (uint32 data_id = 1; data_id < num_values; ++data_id) {
			auto	dst		= values + data_id * num_components;
			if (ComputeParallelogramPrediction(traversal.Opposite(traversal.ValueToCorner(data_id)), traversal.corner_to_value, data_id, values, num_components, pred_vals))
				transform->Decode(pred_vals, dst, dst);
			else
				transform->Decode(dst - num_components, dst, dst);
		}
	}
#endif
#ifdef DRACO_ENABLE_WRITER
	void Encode(const Traversal &traversal, int *values, uint32 num_components, uint32 num_values) override {
		temp_array<int32>	pred_vals(num_components);

		for (uint32 data_id = num_values; --data_id;) {
			auto	dst		= values + data_id * num_components;
			if (ComputeParallelogramPrediction(traversal.Opposite(traversal.ValueToCorner(data_id)), traversal.corner_to_value, data_id, values, num_components, pred_vals))
				transform->Encode(pred_vals, dst, dst);
			else
				transform->Encode(dst - num_components, dst, dst);
		}
		fill(pred_vals, 0);
		transform->Encode(pred_vals, values, values);
	}
#endif
};

// MULTI_PARALLELOGRAM (deprecated)

template<> struct Prediction::T<Prediction::MULTI_PARALLELOGRAM> : Prediction::T<Prediction::PARALLELOGRAM> {
#ifdef DRACO_ENABLE_READER
	void Decode(const Traversal& traversal, int* values, uint32 num_components, uint32 num_values) override {
		temp_array<int32>	pred(num_components, 0);
		temp_array<int32>	parallelogram_pred(num_components, 0);

		transform->Decode(pred, values, values);

		for (uint32 data_id = 1; data_id < num_values; ++data_id) {
			int		num = 0;
			fill(pred, 0);

			for (auto corner : Corners(traversal, traversal.ValueToCorner(data_id))) {
				if (ComputeParallelogramPrediction(traversal.Opposite(corner), traversal.corner_to_value, data_id, values, num_components, parallelogram_pred)) {
					for (int c = 0; c < num_components; ++c)
						pred[c] += parallelogram_pred[c];
					++num;
				}
			}

			auto	dst = values + data_id * num_components;
			if (num) {
				for (int c = 0; c < num_components; ++c)
					pred[c] /= num;
				transform->Decode(pred, dst, dst);
			} else {
				transform->Decode(dst - num_components, dst, dst);	// No parallelogram was valid; we use the last decoded point as a reference
			}
		}
	}
#endif
};

// CONSTRAINED_MULTI_PARALLELOGRAM

template<> struct Prediction::T<Prediction::CONSTRAINED_MULTI_PARALLELOGRAM> : Prediction::T<Prediction::PARALLELOGRAM> {
	enum {
		kMaxNumParallelograms		= 4,
		OPTIMAL_MULTI_PARALLELOGRAM = 0,
	};
	dynamic_bitarray<>	creases[kMaxNumParallelograms];

	static uint32 ComputePredictions(const Traversal& traversal, uint32 data_id, int32 *data, uint32 num_components, int32 *preds, uint32 max) {
		uint32	num		= 0;
		for (auto corner : Corners(traversal, traversal.ValueToCorner(data_id))) {
			if (ComputeParallelogramPrediction(traversal.Opposite(corner), traversal.corner_to_value, data_id, data, num_components, preds + num * num_components) && ++num == max)
				break;
		}
		return num;
	}

#ifdef DRACO_ENABLE_READER
	bool Read(istream_ref file, uint32 num_values, uint16 version) override {
		if (version < VER_REMOVE_UNUSED) {
			uint8 mode = file.getc();
			if (mode != OPTIMAL_MULTI_PARALLELOGRAM)
				return false;
		}
		for (auto &i : creases) {
			if (auto n = get_leb128<uint32>(file))
				i = ProbBuffer(file, version).decode(n);
		}
		return Prediction::Read(file, num_values, version);
	}
	void Decode(const Traversal& traversal, int* values, uint32 num_components, uint32 num_values) override {
		temp_array<int32>	pred(num_components, 0);
		temp_array<int32>	parallelogram_pred(num_components * kMaxNumParallelograms);

		transform->Decode(pred, values, values);

		bit_pointer<uint32>	crease_pos[kMaxNumParallelograms] = {
			creases[0].begin(),
			creases[1].begin(),
			creases[2].begin(),
			creases[3].begin(),
		};

		for (uint32 data_id = 1; data_id < num_values; ++data_id) {
			auto	dst = values + data_id * num_components;

			if (uint32 num_parallelograms = ComputePredictions(traversal, data_id, values, num_components, parallelogram_pred, kMaxNumParallelograms)) {
				fill(pred, 0);
				int32	num_used	= 0;
				auto&	pos			= crease_pos[num_parallelograms - 1];
				int		*p			= parallelogram_pred;
				for (uint32 i = 0; i < num_parallelograms; ++i, p += num_components) {
					ISO_ASSERT(pos < creases[num_parallelograms - 1].end());
					if (!*pos++) {
						++num_used;
						for (uint32 j = 0; j < num_components; ++j)
							pred[j] += p[j];
					}
				}

				if (num_used) {
					for (uint32 j = 0; j < num_components; ++j)
						pred[j] /= num_used;
					transform->Decode(pred, dst, dst);
					continue;
				}
			}

			transform->Decode(dst - num_components, dst, dst);
		}
	}
#endif
#ifdef DRACO_ENABLE_WRITER
	bool Write(ostream_ref file, uint32 num_values) override {
		for (auto &i : creases) {
			write_leb128(file, i.size32());
			if (i.size32())
				ProbBuffer(i).write(file);
		}
		return Prediction::Write(file, num_values);
	}
	void Encode(const Traversal &traversal, int *values, uint32 num_components, uint32 num_values) override {
		struct Context {
			dynamic_array<uint8>	used;
			int64					total_used	= 0;
		} contexts[kMaxNumParallelograms];

		struct Error {
			uint32	num_bits;			// Primary metric:		number of bits required to store the data
			uint32	residual_error;		// Secondary metric:	absolute difference of residuals
			Error(ShannonEntropyTracker &entropy_tracker, const int *pred, const int *value, int num_components) : residual_error(0) {
				temp_array<uint32>	entropy_symbols(num_components);
				for (int i = 0; i < num_components; ++i) {
					auto		dif		= pred[i] - value[i];
					residual_error		+= abs(dif);
					entropy_symbols[i]	= SignedToSymbol(dif);
				}
				auto entropy_data = entropy_tracker.Peek(entropy_symbols, num_components);
				num_bits = uint32(entropy_data.GetNumberOfDataBits() + entropy_data.GetNumberOfRAnsTableBits());
			}
			bool operator<(const Error &e) const { return num_bits < e.num_bits || (num_bits == e.num_bits && residual_error < e.residual_error); }
		};

		struct PredictionConfiguration {
			Error					error;
			uint8					used;
			dynamic_array<int32>	pred;
			PredictionConfiguration(const Error &error) : error(error), used(0) {}
		};

		ShannonEntropyTracker	entropy_tracker;
		temp_array<int32>		parallelogram_pred(num_components * kMaxNumParallelograms);
		temp_array<int32>		pred(num_components);
		temp_array<uint32>		entropy_symbols(num_components);

		for (uint32 data_id = num_values; --data_id;) {
			auto	dst = values + data_id * num_components;
			auto	src	= dst - num_components;
			if (uint32 num_parallelograms = ComputePredictions(traversal, data_id, values, num_components, parallelogram_pred, kMaxNumParallelograms)) {
				auto&	context		= contexts[num_parallelograms - 1];
				uint32	total		= (context.used.size32() + 1) * num_parallelograms;
				uint32	total_used	= context.total_used;

				// compute delta coding error (configuration when no parallelogram is selected)
				PredictionConfiguration best(Error(entropy_tracker, src, dst, num_components));
				best.error.num_bits += ComputeBinaryShannonEntropyBits(total, total_used);
				best.pred			= make_range_n(src, num_components);

				// find best prediction error for different cases of used parallelograms
				for (uint8 used = 1; used < (1 << num_parallelograms); ++used) {
					fill(pred, 0);
					int	*p = parallelogram_pred;
					for (uint8 m = used; m; m >>= 1, p += num_components) {
						if (m & 1) {
							for (int j = 0; j < num_components; ++j)
								pred[j] += p[j];
						}
					}
				
					int	num_used	= count_bits(used);
					for (int j = 0; j < num_components; ++j)
						pred[j] /= num_used;

					Error	error(entropy_tracker, pred, dst, num_components);
					error.num_bits += ComputeBinaryShannonEntropyBits(total, total_used + num_used);

					if (error < best.error) {
						best.error	= error;
						best.used	= used;
						best.pred	= pred;
					}
				}

				context.total_used += count_bits(best.used);
				context.used.push_back(best.used);

				// Update the entropy stream
				for (int i = 0; i < num_components; ++i)
					entropy_symbols[i]	= SignedToSymbol(best.pred[i] - dst[i]);
				entropy_tracker.Push(entropy_symbols, num_components);

				transform->Encode(best.pred, dst, dst);
			} else {
				transform->Encode(src, dst, dst);
			}
		}
		// first element is always fixed because it cannot be predicted
		fill(pred, 0);
		transform->Encode(pred, values, values);

		for (int i = 0; i < kMaxNumParallelograms; i++) {
			creases[i].resize(contexts[i].used.size32() * (i + 1));
			auto	p = creases[i].begin();
			for (auto u : reversed(contexts[i].used)) {
				uint8	crease = ~u;
				*make_range_n(p, i + 1) = make_range_n(bit_pointer<uint32>(&crease), i + 1);
				p += i + 1;
			}
		}
	}
#endif
};

// TEX_COORDS_DEPRECATED - Note that this predictor is not portable and should not be used anymore

template<> struct Prediction::T<Prediction::TEX_COORDS_DEPRECATED> : Prediction {
	dynamic_bitarray<>	orientations;

protected:
	bool ComputePredictedValue(const Traversal& traversal, int32x2* tex_data, uint32 data_id, uint32 next_id, uint32 prev_id, float2 &X_uv, float2 &CX_uv) {
		auto	N_uv	= to<float>(tex_data[next_id]);
		auto	P_uv	= to<float>(tex_data[prev_id]);
		if (all(P_uv == N_uv))
			return false;
		auto	PN_uv	= P_uv - N_uv;

		auto	N		= to<float>(traversal.GetPositionForValue(next_id));
		auto	P		= to<float>(traversal.GetPositionForValue(prev_id));
		auto	PN		= P - N;
		auto	PN_len2 = len2(PN);
		if (PN_len2 == 0)
			return false;

		auto	C		= to<float>(traversal.GetPositionForValue(data_id));
		auto	CN		= C - N;
		float	s		= dot(PN, CN) / PN_len2;
		float	t		= sqrt(len2(CN - PN * s) / PN_len2);

		X_uv	= N_uv + PN_uv * s;
		CX_uv	= perp(PN_uv) * t;
		return true;
	}

#ifdef DRACO_ENABLE_READER
	bool Read(istream_ref file, uint32 num_values, uint16 version) override {
		auto	n		= file.get<uint32>();
		orientations	= ProbBuffer(file, version).decode(n);

		bool	last_orientation = true;
		for (auto &&i : orientations)
			i = (last_orientation ^= !i);

		return Prediction::Read(file, num_values, version);
	}

	void Decode(const Traversal& traversal, int* values, uint32 num_components, uint32 num_values) override {
		auto	tex_data	= (int32x2*)values;

		for (uint32 data_id = 0; data_id < num_values; ++data_id) {
			int32x2	pred;
			auto	corner	= traversal.ValueToCorner(data_id);
			auto	next_id	= traversal.CornerToValue(Next(corner));
			auto	prev_id	= traversal.CornerToValue(Prev(corner));

			if (prev_id < data_id && next_id < data_id) {
				float2	X_uv, CX_uv;
				if (ComputePredictedValue(traversal, tex_data, data_id, next_id, prev_id, X_uv, CX_uv)) {
					auto	orientation		= orientations.pop_back_value();
					pred = to<int>(orientation ? X_uv - CX_uv : X_uv + CX_uv);
				} else {
					pred = tex_data[next_id];
				}
			} else {
				// don't have available textures on both corners; resort to delta coding
				pred = 	next_id < data_id	? tex_data[next_id]
					:	prev_id < data_id	? tex_data[prev_id]
					:	data_id > 0			? tex_data[data_id - 1]
					:	zero;
			}
			auto	dst		= values + data_id * num_components;
			transform->Decode((int32*)&pred, dst, dst);
		}
	}
#endif
};

// TEX_COORDS_PORTABLE
// Use the positions of the triangle to predict the texture coordinate on the tip corner C
//              C
//             /. \
//            / .  \
//           /  .   \
//          N---X----P

template<> struct Prediction::T<Prediction::TEX_COORDS_PORTABLE> : Prediction {
	dynamic_bitarray2<>	orientations;

	int64 ComputePredictedValue(const Traversal& traversal, int32x2* tex_data, uint32 data_id, uint32 next_id, uint32 prev_id, int64x2 &X_uv, int64x2 &CX_uv) {
		auto	N_uv	= to<int64>(tex_data[next_id]);
		auto	P_uv	= to<int64>(tex_data[prev_id]);
		if (all(P_uv == N_uv))
			return 0;
		auto	PN_uv	= P_uv - N_uv;

		auto	N		= to<int64>(traversal.GetPositionForValue(next_id));
		auto	P		= to<int64>(traversal.GetPositionForValue(prev_id));
		auto	PN		= P - N;
		auto	PN_len2	= len2(PN);
		if (PN_len2 == 0)
			return 0;

		auto	C		= to<int64>(traversal.GetPositionForValue(data_id));
		auto	CN		= C - N;
		auto	CNoPN	= dot(CN, PN);
		auto	X		= N + PN * CNoPN / PN_len2;

		// UV coordinates are scaled by PN_len2
		X_uv			= N_uv * PN_len2 + PN_uv * CNoPN;
		CX_uv			= perp(PN_uv) * isqrt(dist2(C, X) * PN_len2);
		return PN_len2;
	}

#ifdef DRACO_ENABLE_READER
	bool Read(istream_ref file, uint32 num_values, uint16 version) override {
		auto	n		= file.get<uint32>();
		orientations	= ProbBuffer(file, version).decode(n);

		bool	last_orientation = true;
		for (auto &&i : orientations)
			i = (last_orientation ^= !i);

		return Prediction::Read(file, num_values, version);
	}
	void Decode(const Traversal& traversal, int* values, uint32 num_components, uint32 num_values) override {
		auto	tex_data	= (int32x2*)values;
		int		num_pred	= 0, num_pred2 = 0;

		for (uint32 data_id = 0; data_id < num_values; ++data_id) {
			int32x2	pred;
			auto	corner	= traversal.ValueToCorner(data_id);
			auto	next_id	= traversal.CornerToValue(Next(corner));
			auto	prev_id	= traversal.CornerToValue(Prev(corner));

			if (prev_id < data_id && next_id < data_id) {
				++num_pred;
				int64x2	X_uv, CX_uv;
				if (auto PN_len2 = ComputePredictedValue(traversal, tex_data, data_id, next_id, prev_id, X_uv, CX_uv)) {
					++num_pred2;
					ISO_ASSERT(!orientations.empty());
					auto	orientation	= orientations.empty() ? false : orientations.pop_back_value();
					pred = to<int>((orientation ? X_uv - CX_uv : X_uv + CX_uv) / PN_len2);
				} else {
					pred = tex_data[next_id];
				}
			} else {
				// don't have available textures on both corners; resort to delta coding
				pred = 	next_id < data_id	? tex_data[next_id]
					:	prev_id < data_id	? tex_data[prev_id]
					:	data_id > 0			? tex_data[data_id - 1]
					:	zero;
			}
			auto	dst		= values + data_id * num_components;
			transform->Decode((int32*)&pred, dst, dst);
		}
	}
#endif
#ifdef DRACO_ENABLE_WRITER
	bool Write(ostream_ref file, uint32 num_values) override {
		file.write(orientations.size32());
		bool	last_orientation = true;
		for (auto &&i : orientations) {
			bool	x = i;
			i = (x == exchange(last_orientation, x));
		}
		
		return ProbBuffer(orientations).write(file) &&  Prediction::Write(file, num_values);
	}
	void Encode(const Traversal &traversal, int *values, uint32 num_components, uint32 num_values) override {
		auto	tex_data	= (int32x2*)values;
		int		num_pred	= 0, num_pred2 = 0;

		for (uint32 data_id = num_values; data_id--;) {
			int32x2	pred;
			auto	corner	= traversal.ValueToCorner(data_id);
			auto	next_id	= traversal.CornerToValue(Next(corner));
			auto	prev_id	= traversal.CornerToValue(Prev(corner));

			if (prev_id < data_id && next_id < data_id) {
				++num_pred;
				int64x2	X_uv, CX_uv;
				if (auto PN_len2 = ComputePredictedValue(traversal, tex_data, data_id, next_id, prev_id, X_uv, CX_uv)) {
					++num_pred2;
					auto	C_uv			= to<int64>(tex_data[data_id]);
					bool	orientation		= dot(C_uv - X_uv, CX_uv) < zero;
					orientations.push_back(orientation);
					pred = to<int>((orientation ? X_uv - CX_uv : X_uv + CX_uv) / PN_len2);
				} else {
					pred = tex_data[next_id];
				}
			} else {
				// don't have available textures on both corners; resort to delta coding
				pred = 	next_id < data_id	? tex_data[next_id]
					:	prev_id < data_id	? tex_data[prev_id]
					:	data_id > 0			? tex_data[data_id - 1]
					:	zero;
			}

			auto	dst		= values + data_id * num_components;
			transform->Encode((int*)&pred, dst, dst);
		}
	}
#endif
};

// GEOMETRIC_NORMAL

template<> struct Prediction::T<Prediction::GEOMETRIC_NORMAL> : Prediction {
	enum NormalPredictionMode : uint8 {
		ONE_TRIANGLE	= 0,  // deprecated
		TRIANGLE_AREA	= 1,
	};
	NormalPredictionMode	prediction_mode = TRIANGLE_AREA;
	dynamic_bitarray<>		flip_normal_bits;

	int32x3 ComputePredictedValue(const Traversal& traversal, int corner) {
		auto	centre	= traversal.GetPositionForCorner(corner);
		if (prediction_mode == ONE_TRIANGLE)
			return cross(traversal.GetPositionForCorner(Next(corner)) - centre, traversal.GetPositionForCorner(Prev(corner)) - centre);

		int32x3	normal	= zero;
		for (auto corner : Corners(traversal, corner))
			normal += cross(traversal.GetPositionForCorner(Next(corner)) - centre, traversal.GetPositionForCorner(Prev(corner)) - centre);
		return normal;
	}

#ifdef DRACO_ENABLE_READER
	bool Read(istream_ref file, uint32 num_values, uint16 version) override {
		if (version < VER_REMOVE_UNUSED)
			file.read(prediction_mode);
		Prediction::Read(file, num_values, version);
		flip_normal_bits = ProbBuffer(file, version).decode(num_values);
		return true;
	}
	void Decode(const Traversal& traversal, int* values, uint32 num_components, uint32 num_values) override {
		uint32	centre	= ((PredictionTransform::T<PredictionTransform::NORMAL_OCTAHEDRON_CANONICALIZED>*)get(transform))->centre;

		for (uint32 i = 0; i < num_values; ++i) {
			Canonical3		pred(ComputePredictedValue(traversal, traversal.ValueToCorner(i)), centre);
			if (flip_normal_bits[i])
				pred.v = -pred.v;

			OctahedralCoord	pred1(pred, centre);
			auto	out		= values + i * 2;
			transform->Decode((int*)&pred1, out, out);
		}
	}
#endif
#ifdef DRACO_ENABLE_WRITER
	bool Write(ostream_ref file, uint32 num_values) override {
		Prediction::Write(file, num_values);
		return ProbBuffer(flip_normal_bits).write(file);
	}
	void Encode(const Traversal &traversal, int *values, uint32 num_components, uint32 num_values) override {
		uint32	centre	= ((PredictionTransform::T<PredictionTransform::NORMAL_OCTAHEDRON_CANONICALIZED>*)get(transform))->centre;
		flip_normal_bits.resize(num_values, false);

		for (uint32 i = 0; i < num_values; ++i) {
			Canonical3	pred(ComputePredictedValue(traversal, traversal.ValueToCorner(i)), centre);
			auto		out		= values + i * 2;
			auto		orig	= ((OctahedralCoord*)out)->ToUnitVector(centre);
			if (dot(orig, pred.v) < zero) {
				flip_normal_bits[i] = true;
				pred.v = -pred.v;
			}
			OctahedralCoord	pred1(pred, centre);
			transform->Encode((int*)&pred1, out, out);
		}
	}
#endif
};

Prediction* Prediction::Get(Method method) {
	switch (method) {
		case DIFFERENCE:						return new T<DIFFERENCE>;
		case PARALLELOGRAM:						return new T<PARALLELOGRAM>;
		case MULTI_PARALLELOGRAM:				return new T<MULTI_PARALLELOGRAM>;
		case TEX_COORDS_DEPRECATED:				return new T<TEX_COORDS_DEPRECATED>;
		case CONSTRAINED_MULTI_PARALLELOGRAM:	return new T<CONSTRAINED_MULTI_PARALLELOGRAM>;
		case TEX_COORDS_PORTABLE:				return new T<TEX_COORDS_PORTABLE>;
		case GEOMETRIC_NORMAL:					return new T<GEOMETRIC_NORMAL>;
		default: return nullptr;
	}
}
//-----------------------------------------------------------------------------
// Attribute
//-----------------------------------------------------------------------------

#ifdef DRACO_ENABLE_READER
bool Attribute::ReadHeader(istream_ref file, uint16 version) {
	if (read(file, type, data_type, num_components, normalised)) {
		unique_id = version < VER_LEB128_ID ? file.get<uint16>() : get_leb128<uint32>(file);
		num_components_coding = num_components;
		return true;
	}
	return false;
}

bool Attribute::ReadPrediction(istream_ref file, uint32 num_values, uint16 version) {
	if (coding) {
		file.read(prediction_method);
		if (pred = Prediction::Get(prediction_method)) {
			file.read(transform_method);
			pred->transform	= PredictionTransform::Get(transform_method, num_components_coding);
			if (version < VER_CODING_AFTER_INTS)
				coding->Read(file);
			values = DecodeIntegerValues(file, num_values, num_components_coding, transform_method != PredictionTransform::NORMAL_OCTAHEDRON_CANONICALIZED, version).raw_data();
			return pred->Read(file, num_values, version);
		}
		values = DecodeIntegerValues(file, num_values, num_components_coding, true, version).raw_data();
		return true;
	}
	return values.read(file, GetSize(data_type) * num_components * num_values);
}

void Decoder::ReadAttributes(istream_ref file, uint16 version) {
	attributes.resize(get_uint32(file, version < VER_LEB128));
	for (auto& a : attributes) {
		a.dec = this;
		a.ReadHeader(file, version);
	}
}

void Decoder::ReadCoding(istream_ref file) {
	for (auto& a : attributes)
		a.SetCoding(file.get());
}

void Decoder::ReadPrediction(istream_ref file, uint32 num_values, uint16 version) {
	for (auto& a : attributes)
		a.ReadPrediction(file, num_values, version);
	if (version >= VER_CODING_AFTER_INTS) {
		for (auto& a : attributes)
			a.ReadCodingData(file);
	}
}

void Decoder::Decode(const Traversal &traversal) {
	for (auto& a : attributes)
		a.Decode(traversal);
}
#endif

#ifdef DRACO_ENABLE_WRITER
bool Attribute::WritePrediction(ostream_ref file, const Traversal &traversal, MODE mode) {
	if (coding) {
		file.write(prediction_method);
		if (pred = Prediction::Get(prediction_method)) {
			file.write(transform_method);
			pred->transform	= PredictionTransform::Get(transform_method, num_components_coding);
			pred->transform->Init(values, NumValues());
			pred->Encode(traversal, values, num_components_coding, NumValues());
			EncodeIntegerValues(file, make_range<int>(values), num_components_coding, transform_method != PredictionTransform::NORMAL_OCTAHEDRON_CANONICALIZED, mode);
			return pred->Write(file, NumValues());
		}
		EncodeIntegerValues(file, values, num_components_coding, true, mode);
		return true;
	}
	return values.write(file);
}

Prediction::Method SelectPredictionMethod(Attribute::Type type, MODE mode) {
	if (type == Attribute::NORMAL)
		return mode & USE_PRED_NORMAL ? Prediction::GEOMETRIC_NORMAL : Prediction::DIFFERENCE;

	if (type == Attribute::TEX_COORD && (mode & USE_PRED_TEXCOORD))
		return Prediction::TEX_COORDS_PORTABLE;

	return	mode & USE_PRED_MPARALLELOGRAM	? Prediction::CONSTRAINED_MULTI_PARALLELOGRAM
		:	mode & USE_PRED_PARALLELOGRAM	? Prediction::PARALLELOGRAM
		:	Prediction::DIFFERENCE;
}

void Decoder::WriteAttributes(ostream_ref file) {
	file.write(make_leb128(attributes.size()));
	for (auto& a : attributes)
		a.WriteHeader(file);
}

void Decoder::WriteCoding(ostream_ref file) {
	for (auto& a : attributes)
		a.WriteCoding(file);
}

void Decoder::WritePrediction(ostream_ref file, const Traversal &traversal, MODE mode) {
	for (auto& a : attributes)
		a.WritePrediction(file, traversal, mode);
	for (auto& a : attributes)
		a.WriteCodingData(file);
}
#endif

struct AttributeFlattener {
	struct entry {
		uint32	*start, num;
		entry(uint32 *start, uint32 num) : start(start), num(num) {}
	};
	struct element : comparisons<element, false> {
		const AttributeFlattener	*c;
		intptr_t		i;
		element()	{}
		constexpr element(const AttributeFlattener *c, intptr_t i) : c(c), i(i) {}
		void	operator=(const element &b) {
			auto	s = b.c->entries.begin();
			for (auto& e : c->entries) {
				ISO_ASSERT(s->num == e.num);
				copy_n(s->start + b.i * e.num, e.start + i * e.num, e.num);
				++s;
			}
		}
		void	operator=(const uint32 *s) {
			for (auto& e : c->entries) {
				copy_n(s, e.start + i * e.num, e.num);
				s += e.num;
			}
		}
		uint32	operator[](int j) const {
			for (auto& e : c->entries) {
				if (j < e.num)
					return (e.start + i * e.num)[j];
				j -= e.num;
			}
			return ~0;
		}
		friend int	compare(const element& a, const element& b) {
			for (auto& e : a.c->entries) {
				if (auto x = memcmp(e.start + a.i * e.num, e.start + b.i * e.num, e.num * sizeof(uint32)))
					return x;
			}
			return 0;
		}
		friend uint64 hash(const element &x) {
			uint64	r = FNV_vals<uint64>::basis;
			for (auto& e : x.c->entries)
				r = _FNVa<uint64>(e.start + x.i * e.num, e.num, r);
			return r;
		}
	};
	struct iterator : comparisons<iterator, false>, private element {
		iterator()	{}
		constexpr iterator(const AttributeFlattener *c, intptr_t i) : element(c, i) {}
		element&	operator*()						{ ISO_ASSERT(i >= 0 && i < c->num_points); return *this; }
		element		operator*()				const	{ ISO_ASSERT(i >= 0 && i < c->num_points); return *this; }
		element		operator[](intptr_t j)	const	{ return *(*this + j); }
		auto&		operator++()					{ ++i; return *this; }
		auto&		operator--()					{ --i; return *this; }
		auto		operator++(int)					{ auto t = *this; ++i; return t; }
		iterator	operator+(intptr_t j)	const	{ return {c, i + j}; }
		auto		operator-(iterator j)	const	{ return i - j.i; }
		friend int	compare(const iterator& a, const iterator& b) { return a.i - b.i; }
	};
	dynamic_array<entry>	entries;
	uint32					num_points;
	uint32					dimension;

	AttributeFlattener(uint32 num_points) : num_points(num_points), dimension(0) {}

	void		add(uint32* values, uint32 num_components) {
		dimension += num_components;
		entries.emplace_back(values, num_components);
	}

	iterator	begin()					const	{ return {this, 0}; }
	iterator	end()					const	{ return {this, num_points}; }
	uint32		size()					const	{ return num_points; }
	element		operator[](intptr_t j)	const	{ ISO_ASSERT(j >= 0 && j < num_points); return {this, j}; }
};

#ifdef DRACO_ENABLE_WRITER

AttributeFlattener GetFlattener(const Decoder &d, uint32 num_points) {
	AttributeFlattener	out(num_points);
	for (auto &a : d.attributes)
		out.add(a.values, a.num_components_coding);
	return out;
}

uint32 NumUniqueValues(const AttributeFlattener &out) {
	hash_set<AttributeFlattener::element>	unique;
	for (auto &&i : out) {
		if (!unique.count(i))
			unique.insert(i);
	}
	return unique.size();
}

uint32 NumUniqueValues(const Attribute &a) {
	AttributeFlattener	out(a.NumValues());
	out.add(a.values, a.num_components_coding);
	return NumUniqueValues(out);
}
#endif

#ifdef DRACO_ENABLE_MESH
//-----------------------------------------------------------------------------
// SequentialMesh
//-----------------------------------------------------------------------------

// Sequential indices encoding methods
enum IndicesType : uint8 {
	COMPRESSED_INDICES,
	UNCOMPRESSED_INDICES,
};

#ifdef DRACO_ENABLE_READER
bool Reader::ReadMeshSequential(istream_ref file) {
	auto	num_faces	= get_uint32(file, version < VER_LEB128_ANS);
	num_points			= get_uint32(file, version < VER_LEB128_ANS);

	switch (file.get<IndicesType>()) {
		case COMPRESSED_INDICES:
			corner_to_point = transformc(DecodeSymbols(file, num_faces * 3, 1, version), [last = 0u](uint32 val) mutable {
				return last += int(val >> 1) ^ -(val & 1);
			});
			break;

		case UNCOMPRESSED_INDICES:
			if (num_points < 256)
				corner_to_point = temp_array<uint8>(file, num_faces * 3);
			else if (num_points < (1 << 16))
				corner_to_point = temp_array<uint16>(file, num_faces * 3);
			else if (num_points < (1 << 21))
				corner_to_point = temp_array<leb128<uint32>>(file, num_faces * 3);
			else
				corner_to_point = temp_array<uint32>(file, num_faces * 3);
			break;

		default:
			return false;
	}

	dec.resize(file.getc());

	for (auto& d : dec) {
		d.ReadAttributes(file, version);
		d.ReadCoding(file);
	}

	for (auto &d : dec) {
		d.ReadPrediction(file, num_points, version);
		d.Decode(Traversal(nullptr, nullptr, nullptr, nullptr, corner_to_point, dec[0].attributes[0].values));
	}

	return true;
}
#endif

#ifdef DRACO_ENABLE_WRITER

bool Writer::WriteMeshSequential(ostream_ref file) {
	auto	num_faces	= NumFaces();
	write_leb128(file, num_faces);
	write_leb128(file, num_points);

	IndicesType	indices = mode & COMPRESS_INDICES ? COMPRESSED_INDICES : UNCOMPRESSED_INDICES;

	file.write(indices);
	switch (indices) {
		case COMPRESSED_INDICES:
			EncodeSymbols(file, transformc(corner_to_point, [last = 0u](uint32 val) mutable {
				int	delta = exchange(last, val) - last;
				return delta < 0 ? ~(delta << 1) : delta << 1;
			}), 1, mode);
			break;

		case UNCOMPRESSED_INDICES:
			if (num_points < 256)
				 file.write(temp_array<uint8>(corner_to_point));
			else if (num_points < (1 << 16))
				file.write(temp_array<uint16>(corner_to_point));
			else if (num_points < (1 << 21))
				file.write(temp_array<leb128<uint32>>(corner_to_point));
			else
				file.write(temp_array<uint32>(corner_to_point));
			break;
	}

	for (auto &d : dec.slice(1))
		dec[0].attributes.append(move_container(d.attributes));
	dec.resize(1);

	file.putc(dec.size());

	for (auto& d : dec) {
		d.WriteAttributes(file);
		d.WriteCoding(file);
	}

	for (auto& d : dec) {
		for (auto &a : d.attributes) {
			a.prediction_method	= SelectPredictionMethod(a.type, mode);
			a.transform_method	= a.type == Attribute::NORMAL ? PredictionTransform::NORMAL_OCTAHEDRON_CANONICALIZED : PredictionTransform::WRAP;;
		}

		d.WritePrediction(file, Traversal(nullptr, nullptr, nullptr, nullptr, corner_to_point, dec[0].attributes[0].values), mode);
	}
	return true;
}
#endif

//-----------------------------------------------------------------------------
// Edgebreaker
//-----------------------------------------------------------------------------
#ifdef DRACO_ENABLE_MESH_EDGEBREAKER

template<typename T, typename C, typename F> void TraversalDepth(const T &connect, C &&corners, F OnNewVertexVisited) {
	dynamic_bitarray<uint32>	visited_face(connect.NumFaces(), false);
	dynamic_bitarray<uint32>	visited_vertex(connect.NumVertices(), false);

	for (int corner : corners) {
		if (visited_face[corner / 3])
			continue;  // already traversed

		auto	next_vert = connect.Vertex(Next(corner));
		dynamic_array<uint32>	stack;
		auto	prev_vert = connect.Vertex(Prev(corner));

		if (!visited_vertex[next_vert].test_set())
			OnNewVertexVisited(next_vert, Next(corner));

		if (!visited_vertex[prev_vert].test_set())
			OnNewVertexVisited(prev_vert, Prev(corner));

		for (;;) {
			visited_face[corner / 3] = true;
			auto	vert = connect.Vertex(corner);

			if (!visited_vertex[vert].test_set()) {
				OnNewVertexVisited(vert, corner);
				// if not on a seam, can just go right and know we'll hit everything
				if (!connect.VertexOnHole(vert)) {
					corner = GetRightCorner(connect, corner);
					continue;
				}
			}

			auto	right_corner	= GetRightCorner(connect, corner);
			auto	left_corner		= GetLeftCorner(connect, corner);

			if (right_corner < 0 || visited_face[right_corner / 3]) {
				if (left_corner < 0 || visited_face[left_corner / 3]) {
					do
						left_corner = stack.empty() ? -1 : stack.pop_back_value();
					while (left_corner >= 0 && visited_face[left_corner / 3]);

					if (left_corner < 0)
						break;
				}
				corner = left_corner;

			} else {
				if (left_corner >= 0 && !visited_face[left_corner / 3])
					stack.push_back(left_corner);
				corner = right_corner;
			}
		}
	}
}

// Traversal following the paper "Multi-way Geometry Encoding" by Cohen-or at al.'02
// Traversal is guided by the prediction degree of the destination vertices, which is the number of possible faces that can be used as source points for traversal to the given destination vertex
// (faces F1 and F2 are already traversed and face F0 is not traversed yet; the prediction degree of vertex V is then equal to two)
//
//            X-----V-----o
//           / \   / \   / \
//          / F0\ /   \ / F2\
//         X-----o-----o-----B
//                \ F1/
//                 \ /
//                  A
//
template<typename T, typename C, typename F> void TraversalMaxDegree(const T &connect, C &&corners, F OnNewVertexVisited) {
	enum { kMaxPriority	= 3 };

	temp_array<uint32>			prediction_degree(connect.NumVertices(), 0);
	dynamic_bitarray<uint32>	visited_face(connect.NumFaces(), false);
	dynamic_bitarray<uint32>	visited_vertex(connect.NumVertices(), false);
	dynamic_array<uint32>		stacks[kMaxPriority];

	for (int corner : corners) {
		stacks[0].push_back(corner);
		auto	vert		= connect.Vertex(corner);
		auto	next_vert	= connect.Vertex(Next(corner));
		auto	prev_vert	= connect.Vertex(Prev(corner));

		if (!visited_vertex[next_vert].test_set())
			OnNewVertexVisited(next_vert, Next(corner));
		if (!visited_vertex[prev_vert].test_set())
			OnNewVertexVisited(prev_vert, Prev(corner));
		if (!visited_vertex[vert].test_set())
			OnNewVertexVisited(vert, corner);

		for (uint32	best_priority = 0;;) {
			while (best_priority < kMaxPriority && stacks[best_priority].empty())
				++best_priority;

			if (best_priority == kMaxPriority)
				break;

			corner = stacks[best_priority].pop_back_value();
			if (visited_face[corner / 3])
				continue;

			for (;;) {
				visited_face[corner / 3] = true;
				auto	vert = connect.Vertex(corner);
				if (!visited_vertex[vert].test_set())
					OnNewVertexVisited(vert, corner);

				auto	right_corner	= GetRightCorner(connect, corner);
				auto	left_corner		= GetLeftCorner(connect, corner);

				if (left_corner >= 0 && !visited_face[left_corner / 3]) {
					auto	vleft		= connect.Vertex(left_corner);
					auto	priority	= visited_vertex[vleft] ? 0 : prediction_degree[vleft]++ ? 1 : 2;
					if ((right_corner < 0 || visited_face[right_corner / 3]) && priority <= best_priority) {
						corner = left_corner;
						continue;
					}
					stacks[priority].push_back(left_corner);
					if (priority < best_priority)
						best_priority = priority;
				}
				if (right_corner >= 0 && !visited_face[right_corner / 3]) {
					auto	vright		= connect.Vertex(right_corner);
					auto	priority	= visited_vertex[vright] ? 0 : prediction_degree[vright]++ ? 1 : 2;
					if (priority <= best_priority) {
						corner = right_corner;
						continue;
					}
					stacks[priority].push_back(right_corner);
					if (priority < best_priority)
						best_priority = priority;
				}
				break;
			}
		}
	}
}

template<typename T, typename C, typename F> void Traverse(const T& connect, Decoder::TraversalMethod method, C &&corners, F OnNewVertexVisited) {
	switch (method) {
		case Decoder::TRAVERSAL_DEPTH_FIRST:
			TraversalDepth(connect, corners, OnNewVertexVisited);
			break;
		case Decoder::TRAVERSAL_MAX_DEGREE:
			TraversalMaxDegree(connect, corners, OnNewVertexVisited);
			break;
	}
}

enum Topology : uint8 {
	TOPOLOGY_C,
	TOPOLOGY_S,
	TOPOLOGY_L,
	TOPOLOGY_R,
	TOPOLOGY_E,
	TOPOLOGY_INVALID,
};

enum {
	MIN_VALENCE	= 2,
	MAX_VALENCE	= 7,
	EDGEBREAKER_VALENCE_MODE_2_7 = 0,
};

enum ConnectivityMethod : uint8 {
	CONN_STANDARD,
	CONN_PREDICTIVE,  // Deprecated
	CONN_VALENCE,
};

// mesh connectivity splits into two
struct SplitEvent {
	uint32	source, split;
	bool	right;
	SplitEvent() {}
	SplitEvent(uint32 source, uint32 split, bool right) : source(source), split(split), right(right) {}
};

struct Splits : dynamic_array<SplitEvent> {
	dynamic_array<uint32>	hole_events;

	bool	read(istream_ref file, uint16 version) {
		resize(get_uint32(file, version < VER_LEB128));

		if (version < VER_COMPRESS_SPLITS) {
			for (auto &i : *this) {
				SplitEvent	&event = push_back();
				file.read(event.split);
				file.read(event.source);
				event.right = file.getc() & 1;
			}
		} else {
			uint32	last_id		= 0;
			for (auto &i : *this) {
				i.source	= (last_id += get_leb128<uint32>(file));
				i.split		= last_id - get_leb128<uint32>(file);
			}
			dynamic_bitarray<>	edges(file, size32());
			for (auto &&i : make_pair(*this, edges))
				i.a.right = i.b;
		}
		if (version < VER_REMOVE_HOLES) {
			hole_events.resize(get_uint32(file, version < VER_LEB128));
			if (version < VER_COMPRESS_SPLITS) {
				file.read(hole_events);
			} else {
				uint32 last_id = 0;
				for (auto &i : hole_events)
					i = (last_id += get_leb128<uint32>(file));
			}
		}
		return true;
	}

	bool write(ostream_ref file) const {
		write_leb128(file, size());

		uint32	last_id		= 0;
		for (auto &i : *this) {
			write_leb128(file, i.source - last_id);
			last_id = i.source;
			write_leb128(file, last_id - i.split);
		}

		dynamic_bitarray<>	edges = transformc(*this, [](const SplitEvent &i) { return i.right; });
		file.write(edges);
		return true;
	}
	bool	test(uint32 source) {
		return !empty() && back().source == source;
	}
};

struct SeamInfo {
	dynamic_bitarray<>	is_edge_on_seam;		// is the corner's opposite edge on a seam?
	dynamic_bitarray<>	is_vertex_on_seam;

	SeamInfo(uint32 num_corners, uint32 num_vertices) : is_edge_on_seam(num_corners, false), is_vertex_on_seam(num_vertices, false) {}

	void	AddSeam(uint32 corner, const int32 *corner_to_vertex) {
		is_edge_on_seam[corner] = true;
		is_vertex_on_seam[corner_to_vertex[Next(corner)]] = true;
		is_vertex_on_seam[corner_to_vertex[Prev(corner)]] = true;
	}
	bool	IsCornerOppositeToSeamEdge(uint32 corner) const {
		return is_edge_on_seam[corner];
	}
	bool	IsVertexOnSeam(uint32 vert) const {
		return is_vertex_on_seam[vert];
	}

	bool	Decode(const ProbBuffer &pb, const int32 *opposite_corners, const int32 *corner_to_vertex, uint16 version) {
		ANSdecoder	ans_decoder(pb.buffer);
		auto		prob_zero	= pb.prob_zero;

		for (uint32 corner = 0, num_corners = is_edge_on_seam.size32(); corner < num_corners; ++corner) {
			auto	opp_corner	= opposite_corners[corner];
			if (opp_corner >= 0) {
				if ((version < VER_REMOVE_HOLES || opp_corner >= corner) && ans_decoder.get_bit(prob_zero)) {
					AddSeam(corner, corner_to_vertex);
					AddSeam(opp_corner, corner_to_vertex);
				}
			} else {
				AddSeam(corner, corner_to_vertex);
			}
		}
		return true;
	}
};

struct Connectivity {
	dynamic_array<int32>	opposite_corners;
	dynamic_array<int32>	corner_to_vertex;
	dynamic_array<int32>	vertex_to_corner;
	dynamic_bitarray<>		is_vert_hole;

	constexpr uint32	NumVertices()		const	{ return vertex_to_corner.size32(); }
	constexpr uint32	NumCorners()		const	{ return corner_to_vertex.size32(); }
	constexpr uint32	NumFaces()			const	{ return NumCorners() / 3; }
	constexpr uint32	Vertex(uint32 c)	const	{ return corner_to_vertex[c]; }
	constexpr int		Opposite(uint32 c)	const	{ return opposite_corners[c]; }

	constexpr bool		VertexOnHole(uint32 v) const {
		return SwingLeft(*this, vertex_to_corner[v]) < 0;
	}

	constexpr bool		IsDegenerate(uint32 corner) const {
		auto v0	= corner_to_vertex[corner + 0];
		auto v1	= corner_to_vertex[corner + 1];
		auto v2	= corner_to_vertex[corner + 2];
		return v0 == v1 || v0 == v2 || v1 == v2;
	}
	void	SetOppositeCorners(uint32 c1, uint32 c2) {
		opposite_corners[c1] = c2;
		opposite_corners[c2] = c1;
	}
	void	AddFace(uint32 v0, uint32 v1, uint32 v2) {
		corner_to_vertex.push_back(v0);
		corner_to_vertex.push_back(v1);
		corner_to_vertex.push_back(v2);
	}

	uint32	ComputeOppositeCorners();
	void	ComputeVertexCorners();
	void	MarkHoles();
	void	BreakNonManifoldEdges();

	Connectivity(uint32 num_corners) : opposite_corners(num_corners, -1) {}
	Connectivity(const dynamic_array<int32>& corner_to_vertex) : opposite_corners(corner_to_vertex.size32(), -1), corner_to_vertex(corner_to_vertex) {}
};

uint32 Connectivity::ComputeOppositeCorners() {
	struct edge {
		int	v, c;
	};
	struct edges {
		edge	*p;
		int		n;
		edges() : n(0) {}
		auto	begin()		const	{ return p; }
		auto	end()		const	{ return p + n; }
		void	add(int v, int c)	{ p[n++] = {v, c}; }
		void	remove(edge &e)		{ e = p[--n]; }
	};

	uint32	num_corners	= corner_to_vertex.size32();
	temp_array<edge>	vertex_edges(num_corners);

	// compute the number of outgoing half-edges (corners) attached to each vertex
	uint32	num_vertices = reduce<op_max>(corner_to_vertex) + 1;
	dynamic_array<edges>	edge_table(num_vertices);
	for (auto v : corner_to_vertex)
		++edge_table[v].n;

	// compute offsets
	auto	p		= vertex_edges.begin();
	for (auto &i : edge_table)
		i.p = exchange(p, p + exchange(i.n, 0));

	// go over the all half-edges and either insert them to the vertex_edge array or connect them with existing half-edges
	for (uint32 c = 0; c < num_corners; ++c) {
		if (c % 3 == 0 && IsDegenerate(c)) {
			c += 2;	 // ignore the next two corners of the same face
			continue;
		}

		auto	tip_v		= corner_to_vertex[c];
		auto	source_v	= corner_to_vertex[Next(c)];
		auto	sink_v		= corner_to_vertex[Prev(c)];
		bool	found		= false;

		for (auto &i : edge_table[sink_v]) {
			if (found = (i.v == source_v && tip_v != corner_to_vertex[i.c])) {
				SetOppositeCorners(c, i.c);	// a matching half-edge was found on the sink vertex
				edge_table[sink_v].remove(i);
				break;
			}
		}
		if (!found)	// no opposite corner found - insert the new edge
			edge_table[source_v].add(sink_v, c);
	}

	return num_vertices;
}

void Connectivity::BreakNonManifoldEdges() {
	uint32				num_corners = corner_to_vertex.size32();
	dynamic_bitarray<>	visited_corner(num_corners, false);
	dynamic_array<pair<uint32, uint32>> sink_vertices;

	for (bool mesh_connectivity_updated = true; mesh_connectivity_updated;) {
		mesh_connectivity_updated = false;

		for (uint32 c = 0; c < num_corners; ++c) {
			if (visited_corner[c])
				continue;

			sink_vertices.clear();

			// swing all the way to find the left-most corner connected to the corner's vertex
			int32 c1 = c;
			for (int32 c2 = SwingLeft(*this, c1); c2 != c && c2 >= 0 && !visited_corner[c2]; c2 = SwingLeft(*this, c1))
				c1 = c2;

			// swing right from the first corner and check if all visited edges are unique
			int32 c0 = c1;
			do {
				visited_corner[c1] = true;
				auto	sink_c			= Next(c1);
				auto	sink_v			= corner_to_vertex[sink_c];
				auto	edge_corner		= Prev(c1);			// Corner that defines the edge on the face
				bool	vertex_connectivity_updated = false;

				// Go over all processed edges; if the current sink vertex has been already encountered before it may indicate a non-manifold edge that needs to be broken
				for (auto& i : sink_vertices) {
					if (i.a == sink_v) {
						// sink vertex has been processed already
						auto other_c = i.b;
						auto opp_c   = opposite_corners[edge_corner];

						if (opp_c != other_c) {
							// break the connectivity on the non-manifold edge
							auto opp_other_c = opposite_corners[other_c];
							if (opp_c >= 0)
								opposite_corners[opp_c] = -1;
							if (opp_other_c >= 0)
								opposite_corners[opp_other_c] = -1;
							opposite_corners[edge_corner] = opposite_corners[other_c] = -1;

							vertex_connectivity_updated	= mesh_connectivity_updated = true;
							break;
						}
					}
				}
				if (vertex_connectivity_updated)
					break;

				sink_vertices.emplace_back(corner_to_vertex[Prev(c1)], sink_c);
				c1 = SwingRight(*this, c1);

			} while (c1 != c0 && c1 >= 0);
		}
	}
}

void Connectivity::ComputeVertexCorners() {
	uint32	num_corners		= NumCorners();
	dynamic_bitarray<>	visited_corner(num_corners, false);

	for (uint32 c = 0; c < num_corners; ++c) {
		if (!visited_corner[c]) {
			int		v = corner_to_vertex[c];
			// swing all the way to the left and mark all corners on the way
			for (int c1 = c;;) {
				visited_corner[c1] = true;
				vertex_to_corner[v] = c1;	// vertex will eventually point to the left most corner
				c1 = SwingLeft(*this, c1);
				if (c1 == c)
					break;  // full circle reached

				if (c1 < 0) {
					// swing right from the initial corner to mark all corners in the opposite direction
					for (c1 = SwingRight(*this, c); c1 >= 0; c1 = SwingRight(*this, c1))
						visited_corner[c1] = true;
					break;
				}
			}
		}
	}
}

void Connectivity::MarkHoles() {
	is_vert_hole.resize(NumVertices(), false);

	for (uint32 c = 0, num_corners = corner_to_vertex.size32(); c < num_corners; ++c) {
		if (c % 3 == 0 && IsDegenerate(c)) {
			c += 2;
		} else if (Opposite(c) < 0) {
			int		v = corner_to_vertex[Next(c)];
			if (!is_vert_hole[v]) {
				// found a new open boundary; traverse and mark all visited vertices
				for (int c1 = c; !is_vert_hole[v]; v = corner_to_vertex[Next(c1)]) {
					is_vert_hole[v] = true;
					c1 = Next(c1);
					while (Opposite(c1) >= 0)
						c1 = Next(Opposite(c1));
				}
			}
		}
	}
}

struct ConnectivityCornerAttribute {
	dynamic_array<int32>		vertex_to_corner;
	dynamic_array<int32>		corner_to_vertex;
	const int32					*opposite_corners;
	bit_pointer<const uint32>	is_edge_on_seam;

	constexpr uint32	NumVertices()		const	{ return vertex_to_corner.size32(); }
	constexpr uint32	NumFaces()			const	{ return corner_to_vertex.size32() / 3; }
	constexpr uint32	Vertex(uint32 c)	const	{ return corner_to_vertex[c]; }
	constexpr int		Opposite(uint32 c)	const	{ return is_edge_on_seam[c] ? -1 : opposite_corners[c]; }
	
	constexpr bool		VertexOnHole(uint32 v) const {
		return SwingLeft(*this, vertex_to_corner[v]) < 0;
	}
	
	ConnectivityCornerAttribute(const Connectivity &con, const SeamInfo &seams)
		: corner_to_vertex(con.NumCorners(), -1)
		, opposite_corners(con.opposite_corners)
		, is_edge_on_seam(seams.is_edge_on_seam.begin())
	{
		int		v = 0;
		for (auto c : con.vertex_to_corner) {
			if (c >= 0) {
				if (seams.IsVertexOnSeam(v)) {	// get first corner that defines an attribute seam
					while (!seams.is_edge_on_seam[Next(c)])
						c = SwingLeft(con, c);
				}

				auto	id = vertex_to_corner.size32();
				corner_to_vertex[c] = id;
				vertex_to_corner.push_back(c);

				for (auto c2 = SwingRight(con, c); c2 >= 0 && c2 != c; c2 = SwingRight(con, c2)) {
					if (seams.IsCornerOppositeToSeamEdge(Next(c2))) {
						id = vertex_to_corner.size32();
						vertex_to_corner.push_back(c2);
					}
					corner_to_vertex[c2] = id;
				}
			}
			++v;
		}
	}
};

#ifdef DRACO_ENABLE_READER

struct StartFaces : ProbBuffer {
	bool read(istream_ref file, uint16 version) {
		prob_zero	= version < VER_START_ANS ? 0 : file.getc();
		buffer		= malloc_block(file, get_uint64(file, version < VER_LEB128_ANS));
		return true;
	}
};

struct ConnectivityRead : Connectivity {
	dynamic_array<int>			active_corner_stack;
	hash_map<uint32, uint32>	split_active_corners;
	uint32						next_vert;
	uint32						merge_vert;

	ConnectivityRead(uint32 num_corners, uint32 num_vertices) : Connectivity(num_corners), next_vert(0) {
		vertex_to_corner.resize(num_vertices, -1);
		is_vert_hole.resize(num_vertices, true);
	}

	uint32	DecodeCorner(Topology topology, uint32 symbol_id) {
		uint32	new_corner	= corner_to_vertex.size32();

		switch (topology) {
			case TOPOLOGY_C: {
				// Create a new face between two edges on the open boundary:
				// The first edge is opposite to the corner a; the other edge is opposite to the corner b, which is reached through a CCW traversal around vertex v
				// One new active boundary edge is created, opposite to new corner x
				//     *-------*
				//    / \     / \
				//   /   \   /   \
				//  /     \ /     \
				// *-------v-------*
				//  \b    /x\    a/
				//   \   /   \   /
				//    \ /  C  \ /
				//     *.......*

				auto	corner_a = exchange(active_corner_stack.back(), new_corner);
				auto	corner_b = Prev(corner_a);
				while (opposite_corners[corner_b] >= 0)
					corner_b = Prev(opposite_corners[corner_b]);

				SetOppositeCorners(corner_a, new_corner + 1);
				SetOppositeCorners(corner_b, new_corner + 2);

				uint32	vert = corner_to_vertex[Next(corner_a)];
				AddFace(vert, corner_to_vertex[Next(corner_b)], corner_to_vertex[Prev(corner_a)]);

				is_vert_hole[vert] = false;
				break;
			}
			case TOPOLOGY_S: {
				// Create a new face that merges two last active edges from the active stack
				// vertices at corners p and n need to be merged into a single vertex
				// *-------v-------*
				//  \a   p/x\n   b/
				//   \   /   \   /
				//    \ /  S  \ /
				//     *.......*

				auto	corner_b	= active_corner_stack.pop_back_value();
				auto	split		= split_active_corners[symbol_id];
				if (split.exists())
					active_corner_stack.push_back(split);

				auto	corner_a = active_corner_stack.back();
				SetOppositeCorners(corner_a, new_corner + 2);
				SetOppositeCorners(corner_b, new_corner + 1);
				active_corner_stack.back() = new_corner;

				uint32	vert = corner_to_vertex[Prev(corner_a)];
				AddFace(vert, corner_to_vertex[Next(corner_a)], corner_to_vertex[Prev(corner_b)]);

				merge_vert = corner_to_vertex[Next(corner_b)];
				for (auto c = Next(corner_b); c >= 0; c = SwingLeft(*this, c))
					corner_to_vertex[c] = vert;
				break;
			}
			case TOPOLOGY_R: {
				// Create a new face extending from the open boundary edge opposite to corner a
				// Two new boundary edges are created opposite to corners r and l
				// New active corner is set to either r (l if TOPOLOGY_L)
				// One new vertex is created at the opposite corner to corner a
				//     *-------*
				//    /a\     / \
				//   /   \   /   \
				//  /     \ /     \
				// *-------v-------*
				//  .l   r.
				//   .   .
				//    . .
				//     *

				auto	corner_a	= exchange(active_corner_stack.back(), new_corner);
				SetOppositeCorners(new_corner + 2, corner_a);
				AddFace(corner_to_vertex[Prev(corner_a)], corner_to_vertex[Next(corner_a)], next_vert++);
				break;
			}
			case TOPOLOGY_L: {
				auto	corner_a	= exchange(active_corner_stack.back(), new_corner);
				SetOppositeCorners(new_corner + 1, corner_a);
				AddFace(corner_to_vertex[Next(corner_a)], next_vert++, corner_to_vertex[Prev(corner_a)]);
				break;
			}
			case TOPOLOGY_E: {
				active_corner_stack.push_back(new_corner);
				AddFace(next_vert, next_vert + 1, next_vert + 2);
				next_vert += 3;
				break;
			}
			default:
				ISO_ASSERT(0);
				break;
		}

		return new_corner;
	}

	void ProcessInteriorEdge(uint32 corner_a) {
		auto	corner_b = Prev(corner_a);
		while (opposite_corners[corner_b] >= 0)
			corner_b = Prev(opposite_corners[corner_b]);

		auto	corner_c = Next(corner_a);
		while (opposite_corners[corner_c] >= 0)
			corner_c = Next(opposite_corners[corner_c]);

		auto	new_corner = corner_to_vertex.size();
		SetOppositeCorners(new_corner + 0, corner_a);
		SetOppositeCorners(new_corner + 1, corner_b);
		SetOppositeCorners(new_corner + 2, corner_c);

		auto	next_a	= corner_to_vertex[Next(corner_a)];
		auto	next_b	= corner_to_vertex[Next(corner_b)];
		auto	next_c	= corner_to_vertex[Next(corner_c)];

		AddFace(next_b, next_c, next_a);

		// Mark all three vertices as interior
		is_vert_hole[next_b] = false;
		is_vert_hole[next_c] = false;
		is_vert_hole[next_a] = false;
	}

	void ProcessInteriorEdges(const StartFaces &start_faces) {
		if (start_faces.prob_zero) {
			ANSdecoder	decoder(start_faces.buffer);
			while (!active_corner_stack.empty()) {
				uint32	corner_a = active_corner_stack.pop_back_value();
				if (decoder.get_bit(start_faces.prob_zero))
					ProcessInteriorEdge(corner_a);
			}
		} else {
			bit_pointer<uint32>	bit(start_faces.buffer);
			while (!active_corner_stack.empty()) {
				uint32	corner_a = active_corner_stack.pop_back_value();
				if (*bit++)
					ProcessInteriorEdge(corner_a);
			}
		}
	}

	void AddSplits(Topology topology, uint32 symbol, Splits &splits) {
		if (topology != TOPOLOGY_C && topology != TOPOLOGY_S) {
			while (splits.test(symbol)) {
				auto	event	= splits.pop_back_value();
				auto	top		= active_corner_stack.back();
				split_active_corners[event.split] = event.right ? Next(top) : Prev(top);
			}
		}
	}

	uint32 UpdateValencies(Topology topology, uint32 corner, uint32 *valencies) {
		uint32	vert = corner_to_vertex[corner];
		uint32	next = corner_to_vertex[Next(corner)];
		uint32	prev = corner_to_vertex[Prev(corner)];

		switch (topology) {
			case TOPOLOGY_S:
				valencies[vert]	+= valencies[merge_vert];
			case TOPOLOGY_C:
				++valencies[next];
				++valencies[prev];
				break;
			case TOPOLOGY_R:
				valencies[vert] += 1;
				valencies[next] += 1;
				valencies[prev] += 2;
				break;
			case TOPOLOGY_L:
				valencies[vert] += 1;
				valencies[next] += 2;
				valencies[prev] += 1;
				break;
			case TOPOLOGY_E:
				valencies[vert] += 2;
				valencies[next] += 2;
				valencies[prev] += 2;
				break;
		}
		return valencies[next];
	}
};

struct TopologyReader {
	vlc_in<uint32, false, memory_reader_owner>	vlc;
	TopologyReader(istream_ref file, uint16 version, bool read = true) : vlc(malloc_block(file, read ? get_uint64(file, version < VER_LEB128_ANS) : 0)) {}
	Topology	next()	{
		auto	symbol = vlc.get(1);
		if (symbol != TOPOLOGY_C)
			symbol += vlc.get(2);
		return (Topology)symbol;
	}
};

bool Reader::ReadMeshEdgebreaker(istream_ref file) {
	auto	connectivity		= file.get<ConnectivityMethod>();

	if (version < VER_REMOVE_UNUSED)
		(void)get_uint32(file, version < VER_LEB128);//num_new_verts

	uint32	num_encoded_vertices= get_uint32(file, version < VER_LEB128);
	uint32	num_faces			= get_uint32(file, version < VER_LEB128);
	uint8	num_attribute_data	= file.getc();
	uint32	num_encoded_symbols	= get_uint32(file, version < VER_LEB128);
	uint32	num_split_symbols	= get_uint32(file, version < VER_LEB128);
	uint32	num_vertices		= num_encoded_vertices + num_split_symbols;
	uint32	num_corners			= num_faces * 3;

	ConnectivityRead		con(num_corners, num_vertices);
	Splits					splits;
	StartFaces				start_faces;
	temp_array<ProbBuffer>	connectivity_buffers(num_attribute_data);

	streamptr	split_end	= 0;
	if (version < VER_SPLITS_AFTER_CONN) {
		auto	offset = get_uint32(file, version < VER_LEB128);
		split_end	= file.tell();
		file.seek_cur(offset);
		splits.read(file, version);
		file.seek(exchange(split_end, file.tell()));
	} else {
		splits.read(file, version);
	}

	switch (connectivity) {
		case CONN_STANDARD: {
			TopologyReader		top(file, version);
			start_faces.read(file, version);
			for (auto &i : connectivity_buffers)
				i.read(file, version);

			for (int i = num_encoded_symbols; i--; ) {
				Topology	topology = top.next();
				con.DecodeCorner(topology, i);
				con.AddSplits(topology, i, splits);
			}
			break;
		}
		case CONN_PREDICTIVE: {
			temp_array<uint32>	valencies(num_vertices, 0);
			ProbBuffer			pred_buffer(file, version);
			ANSdecoder			prediction(pred_buffer.buffer);

			TopologyReader		top(file, version);
			start_faces.read(file, version);
			for (auto &i : connectivity_buffers)
				i.read(file, version);

			Topology			topology	= TOPOLOGY_INVALID;
			for (int i = num_encoded_symbols; i--; ) {
				if (topology == TOPOLOGY_INVALID || !prediction.get_bit(pred_buffer.prob_zero))
					topology = top.next();
				uint32	corner			= con.DecodeCorner(topology, i);
				uint32	next_valence	= con.UpdateValencies(topology, corner, valencies);
				con.AddSplits(topology, i, splits);
				topology = topology == TOPOLOGY_C || topology == TOPOLOGY_R ? (next_valence < 6 ? TOPOLOGY_R : TOPOLOGY_C) : TOPOLOGY_INVALID;
			}
			break;
		}
		case CONN_VALENCE: {
			TopologyReader		top(file, version, version < VER_VALENCE_ONLYPRED);
			start_faces.read(file, version);
			for (auto &i : connectivity_buffers)
				i.read(file, version);

			if (version < VER_REMOVE_UNUSED) {
				(void)get_uint32(file, version < VER_LEB128);//num_split_symbols
				if (file.getc() != EDGEBREAKER_VALENCE_MODE_2_7)
					return false;
			}

			temp_array<uint32>		valencies(num_vertices, 0);
			dynamic_array<uint32>	context_symbols[MAX_VALENCE - MIN_VALENCE + 1];
			for (auto &i : context_symbols)
				i = DecodeSymbols(file, get_leb128(file), 1, version);

			for (int i = num_encoded_symbols, context = -1; i--; ) {
				Topology	topology		= context >= 0 ? (Topology)context_symbols[context].pop_back_value()
											: version < VER_VALENCE_ONLYPRED ? top.next()
											: TOPOLOGY_E;
				uint32		corner			= con.DecodeCorner(topology, i);
				uint32		next_valence	= con.UpdateValencies(topology, corner, valencies);
				con.AddSplits(topology, i, splits);
				context	= clamp(next_valence, MIN_VALENCE, MAX_VALENCE) - MIN_VALENCE;
			}
			break;
		}
		default:
			return false;
	}
	
	ISO_ASSERT(con.next_vert == num_vertices);

	con.ProcessInteriorEdges(start_faces);
	con.ComputeVertexCorners();

	if (split_end)
		file.seek(split_end);

	//DecodeAttributeSeams
	dynamic_array<SeamInfo>	seam_info(num_attribute_data, num_corners, num_vertices);
	for (auto &&i : make_pair(connectivity_buffers, seam_info))
		i.b.Decode(i.a, con.opposite_corners, con.corner_to_vertex, version);

	//ReadDecoders
	dec.resize(file.getc());

	for (auto &d : dec) {
		file.read(d.id);
		file.read(d.type);
		if (version >= VER_TRAVERSAL_METHOD)
			file.read(d.traversal);
	}

	for (auto& d : dec) {
		d.ReadAttributes(file, version);
		d.ReadCoding(file);
	}

	for (auto &d : dec) {
		dynamic_array<int32>	vertex_to_value;
		dynamic_array<uint32>	value_to_corner;

		auto	corners = make_deferred(int_range(num_faces)) * 3;

		auto OnNewVertexVisited = [&value_to_corner, &vertex_to_value](uint32 vertex, uint32 corner) {
			vertex_to_value[vertex] = value_to_corner.size32();
			value_to_corner.push_back(corner);
		};

		int	i = dec.index_of(d);
		if (i > 0 && d.type == Decoder::CORNER_ATTRIBUTE) {
			ConnectivityCornerAttribute	con_attr(con, seam_info[i - 1]);

			vertex_to_value.resize(con_attr.NumVertices(), -1);
			Traverse(con_attr, d.traversal, corners, OnNewVertexVisited);
			d.corner_to_value = transformc(con_attr.corner_to_vertex, [&vertex_to_value](int i) {return vertex_to_value[i];});

		} else {
			vertex_to_value.resize(num_vertices, -1);
			Traverse(con, d.traversal, corners, OnNewVertexVisited);
			d.corner_to_value = transformc(con.corner_to_vertex, [&vertex_to_value](int i) {return vertex_to_value[i];});
		}
		
		d.ReadPrediction(file, value_to_corner.size(), version);

		Traversal	traversal(
			con.opposite_corners,
			d.type == Decoder::CORNER_ATTRIBUTE ? make_const(seam_info[i - 1]).is_edge_on_seam.begin() : nullptr,
			d.corner_to_value,	value_to_corner,
			dec[0].corner_to_value, dec[0].attributes[0].values
		);

		d.Decode(traversal);
	}

	//AssignPointsToCorners
	corner_to_point.resize(num_corners, -1);
	uint32	count = 0;

	for (auto &c : con.vertex_to_corner) {
		if (c < 0)
			continue;

		auto	first_corner = c;
		if (!con.is_vert_hole[con.vertex_to_corner.index_of(c)]) {
			auto	c0	= first_corner;
			auto	v	= con.corner_to_vertex[c0];
			for (auto &seams : seam_info) {
				if (seams.IsVertexOnSeam(v)) {
					auto	&corner_to_value	= dec[seam_info.index_of(seams) + 1].corner_to_value;
					auto	val		= corner_to_value[c0];
					bool	seam	= false;
					for (auto c1 = SwingRight(con, c0); c1 != c0; c1 = SwingRight(con, c1)) {
						auto val2 = corner_to_value[c1];
						if (val2 != val) {
							first_corner = c1;
							seam = true;
							break;
						}
					}
					if (seam)
						break;
				}
			}
		}

		corner_to_point[first_corner] = count++;

		for (auto prev = first_corner, next = SwingRight(con, prev); next >= 0 && next != first_corner; prev = exchange(next, SwingRight(con, next))) {
			bool	seam = false;
			for (auto &d : dec.slice(1)) {
				if (d.corner_to_value[next] != d.corner_to_value[prev]) {
					seam = true;
					break;
				}
			}
			corner_to_point[next] = seam ? count++ : corner_to_point[prev];
		}
	}
	num_points = count;
	return true;
}
#endif	//DRACO_ENABLE_READER

#ifdef DRACO_ENABLE_WRITER
struct ConnectivityEncoder {
	virtual	void EncodeCorner(struct ConnectivityWrite *con, Topology symbol, int corner) = 0;
};

struct ConnectivityWrite : Connectivity {
	dynamic_bitarray<>		visited_face;
	dynamic_bitarray2<>		start_faces;
	dynamic_array<int>		processed_corners;
	Splits					splits;

	uint32	num_isolated_vertices;
	uint32	num_degenerated_faces;
	uint32	num_split_symbols;

	constexpr uint32	NumEncodedFaces()		const { return Connectivity::NumFaces() - num_degenerated_faces; }
	constexpr uint32	NumEncodedVertices()	const { return Connectivity::NumVertices() - num_isolated_vertices; }

	void				ComputeVertexCorners(uint32 num_vertices);
	void				EncodeConnectivity(ConnectivityEncoder *enc);
	dynamic_array<ProbBuffer>	EncodeSeams(SeamInfo *seam_info, uint32 nattr);

	ConnectivityWrite(const dynamic_array<int32>& corner_to_vertex) : Connectivity(corner_to_vertex), num_degenerated_faces(0), num_split_symbols(0) {
		auto	num_original_vertices = ComputeOppositeCorners();
		BreakNonManifoldEdges();
		ComputeVertexCorners(num_original_vertices);
		MarkHoles();
	}
};

// this version checks for non-manifold and degenerate faces
void ConnectivityWrite::ComputeVertexCorners(uint32 num_vertices) {
	vertex_to_corner.resize(num_vertices, -1);
	uint32	num_corners		= NumCorners();
	dynamic_bitarray2<> visited_vertex(num_vertices, false);
	dynamic_bitarray<>	visited_corner(num_corners, false);

	for (uint32 c = 0; c < num_corners; ++c) {
		if (c % 3 == 0 && IsDegenerate(c)) {
			corner_to_vertex[c]		= -1;
			corner_to_vertex[++c]	= -1;
			corner_to_vertex[++c]	= -1;
			++num_degenerated_faces;

		} else if (!visited_corner[c]) {
			int		v = corner_to_vertex[c];
			bool	non_manifold = visited_vertex[v].test_set();
			if (non_manifold) {
				//create a new vertex for non-manifold
				vertex_to_corner.push_back(c);
				visited_vertex.push_back(true);
				v	= num_vertices++;
			}

			// First swing all the way to the left and mark all corners on the way
			for (int c1 = c;;) {
				visited_corner[c1] = true;
				vertex_to_corner[v] = c1;	// vertex will eventually point to the left most corner
				if (non_manifold)
					corner_to_vertex[c1] = v;

				c1 = SwingLeft(*this, c1);
				if (c1 == c)
					break;  // full circle reached; done

				if (c1 < 0) {
					// reached an open boundary; swing right from the initial corner to mark all corners in the opposite direction
					for (c1 = SwingRight(*this, c); c1 >= 0; c1 = SwingRight(*this, c1)) {
						visited_corner[c1] = true;
						if (non_manifold)
							corner_to_vertex[c1] = v;
					}
					break;
				}
			}
		}
	}
	num_isolated_vertices	= visited_vertex.count_clear();
}

void ConnectivityWrite::EncodeConnectivity(ConnectivityEncoder *enc) {
	uint32	num_corners		= corner_to_vertex.size32();
	uint32	num_vertices	= vertex_to_corner.size32();
	uint32	num_faces		= num_corners / 3;

	dynamic_bitarray<uint32>	visited_vertex(num_vertices, false);
	hash_map<uint32, uint32>	face_to_split;	// Map between face_id and symbol_id for faces that were encoded with TOPOLOGY_S

	visited_face.resize(num_faces, false);

	dynamic_array<int>		initial_corners;
	dynamic_array<uint32>	stack;

	int	last_encoded_symbol_id = -1;

	// Traverse the surface starting from each unvisited corner
	for (uint32 f = 0; f < num_faces; ++f) {
		if (visited_face[f] || IsDegenerate(f * 3))
			continue;

		//FindInitFaceConfiguration
		bool	interior	= true;
		int		corner		= f * 3;
		for (int i = 3, c = corner; i--; ++c) {
			if (Opposite(c) < 0) {
				// boundary edge - start from the corner opposite to the first boundary edge
				corner		= c;
				interior	= false;
				break;
			}
			if (is_vert_hole[corner_to_vertex[c]]) {
				// boundary vertex - find the first boundary edge attached to the point and start from the corner opposite to it
				for (int cr = c; cr >= 0; cr = SwingRight(*this, cr))
					c = cr;
				corner		= Prev(c);
				interior	= false;
				break;
			}
		}

		start_faces.push_back(interior);

		if (interior) {
			// mark visited
			visited_vertex[corner_to_vertex[corner]]		= true;
			visited_vertex[corner_to_vertex[Next(corner)]]	= true;
			visited_vertex[corner_to_vertex[Prev(corner)]]	= true;
			visited_face[corner / 3] = true;

			// starting from the opposite face of the next corner; the first encoded corner corresponds to the tip corner of the regular edgebreaker traversal
			// essentially the initial face can be then viewed as a TOPOLOGY_C face
			initial_corners.push_back(Next(corner));
			corner = Opposite(Next(corner));
			if (visited_face[corner / 3])
				continue;

		} else {
			// first encode the hole that's opposite to the start_corner
			int		v0	= corner_to_vertex[Next(corner)];
			for (int c1 = corner;; c1 = Next(c1)) {
				while (Opposite(c1) >= 0)
					c1 = Next(Opposite(c1));
				int v = corner_to_vertex[Prev(c1)];
				visited_vertex[v]	= true;
				if (v == v0)
					break;
			}
		}

		for (;;) {
			++last_encoded_symbol_id;
			visited_face[corner / 3] = true;			// Mark the current face as visited
			processed_corners.push_back(corner);

			const int	vert	= corner_to_vertex[corner];

			if (!visited_vertex[vert].test_set()) {
				if (!is_vert_hole[vert]) {
					enc->EncodeCorner(this, TOPOLOGY_C, corner);
					corner = GetRightCorner(*this, corner);
					continue;
				}

				// encode hole (this corner will be encoded with TOPOLOGY_S symbol below)
				int c = Prev(corner);
				while (Opposite(c) >= 0)
					c = Next(Opposite(c));

				for (const int v0 = corner_to_vertex[corner];; c = Next(c)) {
					while (Opposite(c) >= 0)
						c = Next(Opposite(c));
					int v = corner_to_vertex[Prev(c)];
					if (v == v0)
						break;
					visited_vertex[v] = true;
				}
			}
			// current vertex has been already visited or it was on a boundary; see if we can visit any of its neighboring faces
			auto	right_corner	= GetRightCorner(*this, corner);
			auto	left_corner		= GetLeftCorner(*this, corner);

			if (right_corner < 0 || visited_face[right_corner / 3]) {
				if (right_corner >= 0 && face_to_split.check(right_corner / 3))
					splits.emplace_back(last_encoded_symbol_id, face_to_split[right_corner / 3], true);

				if (left_corner < 0 || visited_face[left_corner / 3]) {
					if (left_corner >= 0 && face_to_split.check(left_corner / 3))
						splits.emplace_back(last_encoded_symbol_id, face_to_split[left_corner / 3], false);

					enc->EncodeCorner(this, TOPOLOGY_E, corner);
					left_corner = -1;
					while (left_corner < 0 && !stack.empty()) {
						left_corner = stack.pop_back_value();
						if (visited_face[left_corner / 3])
							left_corner = -1;
					}
					if (left_corner < 0)
						break;
				} else {
					enc->EncodeCorner(this, TOPOLOGY_R, corner);
				}
				corner = left_corner;

			} else if (left_corner < 0 || visited_face[left_corner / 3]) {
				if (left_corner >= 0 && face_to_split.check(left_corner / 3))
					splits.emplace_back(last_encoded_symbol_id, face_to_split[left_corner / 3], false);

				enc->EncodeCorner(this, TOPOLOGY_L, corner);
				corner = right_corner;

			} else {
				enc->EncodeCorner(this, TOPOLOGY_S, corner);
				++num_split_symbols;
				face_to_split[corner / 3] = last_encoded_symbol_id;
				stack.push_back(left_corner);
				corner = right_corner;
			}
		}
	}

	reverse(processed_corners);					// reverse the order of connectivity corners to match the order in which they are going to be decoded
	processed_corners.append(initial_corners);	// append the init face connectivity corners (which are processed in order by the decoder after the regular corners)
}

dynamic_array<ProbBuffer> ConnectivityWrite::EncodeSeams(SeamInfo *seam_info, uint32 nattr) {
	temp_array<dynamic_bitarray2<>>	bits(nattr);

	visited_face.clear_all();
	for (auto c : processed_corners) {
		visited_face[c / 3] = true;
		for (int i = 3; i--; c = Next(c)) {
			auto o = Opposite(c);
			if (o >= 0 && !visited_face[o / 3]) {
				for (int j = 0; j < nattr; j++)
					bits[j].push_back(seam_info[j].IsCornerOppositeToSeamEdge(c));
			}
		}
	}

	return bits;
}

struct ConnectivityEncoderStandard : ConnectivityEncoder {
	dynamic_array<Topology>		symbols;

	void	EncodeCorner(ConnectivityWrite *con, Topology symbol, int corner) override {
		symbols.push_back(symbol);
	}
	uint32	NumSymbols() const {
		return symbols.size32();
	}
	bool	Write(ostream_ref file) const {
		auto	vlc = make_vlc_out<uint32, false>(dynamic_memory_writer());
		for (auto symbol : reversed(symbols)) {
			vlc.put_bit(symbol != TOPOLOGY_C);
			if (symbol != TOPOLOGY_C)
				vlc.put(symbol - 1, 2);
		}
		vlc.flush();
		write_leb128(file, vlc.get_stream().tell32());
		file.write(vlc.get_stream().data());
		return true;
	}
};

struct ConnectivityEncoderValence : ConnectivityEncoder {
	dynamic_array<int32>	corner_to_vertex; // copy from con because we may need to modify to handle split symbols
	dynamic_array<int>		valences;
	Topology				prev_symbol;
	dynamic_array<uint32>	context_symbols[MAX_VALENCE - MIN_VALENCE + 1];

	uint32	NumSymbols() const {
		uint32	num = 1;
		for (auto& i : context_symbols)
			num += i.size32();
		return num;
	}

	void EncodeCorner(ConnectivityWrite *con, Topology symbol, int corner) {
		// Update valences on the mesh and compute the predicted preceding symbol
		// Note that the valences are computed for the so far unencoded part of the mesh; adding a new symbol either reduces valences on the vertices or leaves the valence unchanged
		int		nextc	= Next(corner);
		int		prevc	= Prev(corner);
		int		lastv	= corner_to_vertex[corner];
		int		nextv	= corner_to_vertex[nextc];
		int		prevv	= corner_to_vertex[prevc];

		const int valence = valences[nextv];

		switch (symbol) {
			case TOPOLOGY_C:
				--valences[nextv];
				--valences[prevv];
				break;
			case TOPOLOGY_S: {
				--valences[nextv];
				--valences[prevv];

				// Whenever we reach a split symbol, we need to split the vertex into two and attach all corners on the left and right sides of the split vertex to the respective vertices (see image below)
				// This is necessary since the decoder works in the reverse order and it merges the two vertices only after the split symbol is processed
				//     * -----
				//    / \--------
				//   /   \--------
				//  /     \-------
				// *-------v-------*
				//  \     /c\     /
				//   \   /   \   /
				//    \ /n S p\ /
				//     *.......*

				// Count the number of faces on the left side of the split vertex and update the valence
				int num_left_faces = 0;
				for (int c = con->Opposite(prevc); c >= 0 && !con->visited_face[c / 3]; c = GetRightCorner(*con, c))
					++num_left_faces;

				valences[lastv] = num_left_faces + 1;
				const int newv = valences.size32();			// Create a new vertex for the right side and count the number of faces that should be attached to this vertex

				int		num_right_faces = 0;
				for (int c = con->Opposite(nextc); c >= 0 && !con->visited_face[c / 3]; c = GetLeftCorner(*con, c)) {
					++num_right_faces;
					corner_to_vertex[Next(c)] = newv;	// Map corners on the right side to the newly created vertex
				}

				valences.push_back(num_right_faces + 1);
				break;
			}
			case TOPOLOGY_R:
				valences[lastv] -= 1;
				valences[nextv] -= 1;
				valences[prevv] -= 2;
				break;
			case TOPOLOGY_L:
				valences[lastv] -= 1;
				valences[nextv] -= 2;
				valences[prevv] -= 1;
				break;
			case TOPOLOGY_E:
				valences[lastv] -= 2;
				valences[nextv] -= 2;
				valences[prevv] -= 2;
				break;
			default:
				break;
		}

		if (prev_symbol != TOPOLOGY_INVALID)
			context_symbols[clamp(valence, MIN_VALENCE, MAX_VALENCE) - MIN_VALENCE].push_back(prev_symbol);

		prev_symbol = symbol;
	}

	bool	Write(ostream_ref file, MODE mode) {
		for (auto &i : context_symbols) {
			write_leb128(file, i.size32());
			EncodeSymbols(file, i, 1, mode);
		}
		return true;
	}
	ConnectivityEncoderValence(const ConnectivityWrite &con) : corner_to_vertex(con.corner_to_vertex), valences(con.is_vert_hole), prev_symbol(TOPOLOGY_INVALID) {
		for (auto &v : corner_to_vertex) {
			if (v >= 0)
				++valences[v];
		}
	}
};

bool Writer::WriteMeshEdgebreaker(ostream_ref file) {
	//Assign attributes to decoders
	int		dec_id = -1;
	for (auto *d = dec.begin(); d != dec.end();) {
		for (auto a = d->attributes.begin(); a != d->attributes.end();) {
			a->prediction_method	= SelectPredictionMethod(a->type, mode);
			a->transform_method		= a->type == Attribute::NORMAL ? PredictionTransform::NORMAL_OCTAHEDRON_CANONICALIZED : PredictionTransform::WRAP;

			Decoder	*merge	= nullptr;
			if (d != dec.begin()) {
				// find decoder with most similar connectivity
				uint32	bestu	= 0;//NumUniqueValues(*a) / 2;
				for (auto *d1 = dec.begin(); d1 != d; ++d1) {
					auto	flat	= GetFlattener(*d1, num_points);
					auto	nu0		= NumUniqueValues(flat);
					flat.add(a->values, a->num_components_coding);
					auto	nu	= NumUniqueValues(flat) - nu0;
					if (nu < bestu) {
						bestu	= nu;
						merge	= d1;
					}
				}
			}

			if (merge) {
				swap(merge->attributes.push_back(), *a);
				d->attributes.erase_unordered(a);

			} else {
				++a;
			}
		}

		if (d->attributes.empty()) {
			dec.erase_unordered(d);
		} else {
			d->id			= dec_id++;
			d->type			= d == dec.begin() ? Decoder::VERTEX_ATTRIBUTE : Decoder::CORNER_ATTRIBUTE;
			bool	maxpred	= false;
			if (mode & USE_TRAV_MAXDEGREE) {
				for (auto& i : d->attributes) {
					if (maxpred = maxprediction_helps(i.prediction_method))
						break;
				}
			}
			d->traversal	= maxpred ? Decoder::TRAVERSAL_MAX_DEGREE : Decoder::TRAVERSAL_DEPTH_FIRST;
			++d;
		}
	}

	// make value maps
	if (mode & USE_SINGLE_CONNECTIVITY) {
		dec[0].corner_to_value = corner_to_point;

	} else {
		for (auto &d : dec) {
			auto	flat	= GetFlattener(d, num_points);
			temp_array<uint32>	point_to_value(num_points, -1);
			temp_array<uint32>	value_to_point(num_points, -1);
			hash_map<AttributeFlattener::element, int>	unique;

			uint32	n = 0;
			for (int i = 0; i < num_points; i++) {
				auto	value = flat[i];
				if (unique.check(value)) {
					point_to_value[i] = unique[value];
				} else {
					value_to_point[n] = i;
					point_to_value[i] = unique[value] = n++;
				}
			}
			d.corner_to_value = transformc(corner_to_point, [&point_to_value](int i) {return point_to_value[i];});
		}
	}

	ConnectivityWrite	con(dec[0].corner_to_value);

	// make seam_info
	uint8	num_attribute_data		= dec.size32() - 1;
	dynamic_array<SeamInfo>	seam_info(num_attribute_data, con.NumCorners(), num_points);

	for (auto &seams : seam_info) {
		int32	*corner_to_value = dec[seam_info.index_of(seams) + 1].corner_to_value;
		for (uint32 c = 0; c < con.NumCorners(); ++c) {
			if (c % 3 == 0 && con.IsDegenerate(c)) {
				c += 2;
			} else {
				int o = con.Opposite(c);
				if (o < 0) {
					seams.AddSeam(c, con.corner_to_vertex);	// boundary - mark it as seam edge

				} else if (o >= c && (corner_to_value[Next(c)] != corner_to_value[Prev(o)] || corner_to_value[Prev(c)] != corner_to_value[Next(o)])) {
					seams.AddSeam(c, con.corner_to_vertex);
					seams.AddSeam(o, con.corner_to_vertex);
				}
			}
		}
	}

	auto	connectivity	= con.NumEncodedFaces() < 1000 ? CONN_STANDARD : CONN_VALENCE;

	file.write(connectivity);
	write_leb128(file, con.NumEncodedVertices());
	write_leb128(file, con.NumEncodedFaces());
	file.write(num_attribute_data);

	if (connectivity == CONN_STANDARD) {
		ConnectivityEncoderStandard	cs;
		con.EncodeConnectivity(&cs);
		write_leb128(file, cs.NumSymbols());
		write_leb128(file, con.num_split_symbols);
		con.splits.write(file);
		cs.Write(file);
		ProbBuffer(con.start_faces).write(file);
		con.EncodeSeams(seam_info, num_attribute_data).write(file);

	} else { //CONN_VALENCE
		ConnectivityEncoderValence	cv(con);
		con.EncodeConnectivity(&cv);
		write_leb128(file, cv.NumSymbols());
		write_leb128(file, con.num_split_symbols);
		con.splits.write(file);
		ProbBuffer(con.start_faces).write(file);
		con.EncodeSeams(seam_info, num_attribute_data).write(file);
		cv.Write(file, mode);
	}

	//WriteDecoders
	file.putc(dec.size());

	for (auto &d : dec)
		file.write(d.id, d.type, d.traversal);

	for (auto& d : dec) {
		d.WriteAttributes(file);
		d.WriteCoding(file);
	}

	auto	orig_pos = dec[0].attributes[0].values;

	for (auto& d : dec) {
		dynamic_array<int32>	vertex_to_value;
		dynamic_array<uint32>	value_to_corner;

		auto OnNewVertexVisited = [&value_to_corner, &vertex_to_value](uint32 vertex, uint32 corner) {
			vertex_to_value[vertex] = value_to_corner.size32();
			value_to_corner.push_back(corner);
		};

		int	i = dec.index_of(d);
		if (i > 0 && d.type == Decoder::CORNER_ATTRIBUTE) {
			ConnectivityCornerAttribute	con_attr(con, seam_info[i - 1]);

			vertex_to_value.resize(con_attr.NumVertices(), -1);
			Traverse(con_attr, d.traversal, con.processed_corners, OnNewVertexVisited);
			d.corner_to_value = transformc(con_attr.corner_to_vertex, [&vertex_to_value](int i) { return i >= 0 ? vertex_to_value[i] : -1;});

		} else {
			vertex_to_value.resize(con.NumVertices(), -1);
			Traverse(con, d.traversal, con.processed_corners, OnNewVertexVisited);
			d.corner_to_value = transformc(con.corner_to_vertex, [&vertex_to_value](int i) { return i >= 0 ? vertex_to_value[i] : -1;});
		}
		
		//rearrange values to match traversal order
		temp_array<malloc_block>	rearranged(d.attributes.size());
		AttributeFlattener			f1(num_points);
		AttributeFlattener			f2(value_to_corner.size32());

		auto	pr = rearranged.begin();
		for (auto &a : d.attributes) {
			pr->resize(value_to_corner.size32() * a.num_components_coding * sizeof(int));
			f1.add(a.values, a.num_components_coding);
			f2.add(*pr++, a.num_components_coding);
		}

		int	j = 0;
		for (auto &i : value_to_corner)
			f2[j++] = f1[corner_to_point[i]];

		pr = rearranged.begin();;
		for (auto &a : d.attributes)
			swap(a.values, *pr++);

		d.WritePrediction(file, Traversal(con.opposite_corners, 
			d.type == Decoder::CORNER_ATTRIBUTE ? make_const(seam_info[i - 1]).is_edge_on_seam.begin() : nullptr,
			d.corner_to_value, value_to_corner,
			corner_to_point, orig_pos
		), mode);
	}
	return true;
}
#endif	//DRACO_ENABLE_WRITER
#endif	//DRACO_ENABLE_MESH_EDGEBREAKER
#endif	//DRACO_ENABLE_MESH

#ifdef DRACO_ENABLE_POINT_CLOUD

//-----------------------------------------------------------------------------
// point cloud sequential
//-----------------------------------------------------------------------------

#ifdef DRACO_ENABLE_READER
bool Reader::ReadPointCloudSequential(istream_ref file) {
	num_points = file.get<int>();
	dec.resize(file.getc());

	for (auto& d : dec) {
		d.ReadAttributes(file, version);
		d.ReadCoding(file);
	}

	for (auto &d : dec) {
		d.ReadPrediction(file, num_points, version);
		d.Decode(Traversal(nullptr, nullptr, nullptr, nullptr, corner_to_point, dec[0].attributes[0].values));
	}

	return true;
}
#endif

#ifdef DRACO_ENABLE_WRITER
bool Writer::WritePointCloudSequential(ostream_ref file) {
	file.write(num_points);
	file.putc(dec.size());

	for (auto& d : dec) {
		d.WriteAttributes(file);
		d.WriteCoding(file);
	}

	for (auto& d : dec)
		d.WritePrediction(file, Traversal(nullptr, nullptr, nullptr, nullptr, corner_to_point, dec[0].attributes[0].values), mode);

	return true;
}
#endif

//-----------------------------------------------------------------------------
// point cloud KD
// Olivier Devillers and Pierre-Marie Gandoin: "Geometric compression for interactive transmission"
//
// Keep splitting the point cloud in the middle of an axis and encode the number of points in the first half (point order is not preserved)
// The axis is chosen that keeps the point cloud as clustered as possible, which gives a better compression rate
// The number of points is encoded by the deviation from the half of the points in the smaller half of the two
//-----------------------------------------------------------------------------

#ifdef DRACO_ENABLE_POINT_CLOUD_KD

struct Direct;
struct RAns;
template<typename D> struct Folded32;
template<typename D> struct Axis;

template<int COMP> struct KDCoders;
template<> struct KDCoders<0> : T_type<type_list<Direct, Direct, Direct, Axis<void>>>			{};
template<> struct KDCoders<2> : T_type<type_list<RAns, Direct, Direct, Axis<void>>>				{};
template<> struct KDCoders<4> : T_type<type_list<Folded32<RAns>, Direct, Direct, Axis<void>>>	{};
template<> struct KDCoders<6> : T_type<type_list<Folded32<RAns>, Direct, Direct, Axis<Direct>>>	{};

//-----------------------------------------------------------------------------
// read point cloud KD
//-----------------------------------------------------------------------------

#ifdef DRACO_ENABLE_READER

template<typename T> class KDDecoder;
template<typename T> struct KDDecoders;
template<typename N, typename R, typename H, typename A> struct KDDecoders<type_list<N,R,H,A>> {
	KDDecoder<N>	numbers;
	KDDecoder<R>	remaining;
	KDDecoder<A>	axis;
	KDDecoder<H>	half;
	KDDecoders(file_version file) : numbers(file), remaining(file), axis(file), half(file) {}
};
template<int COMP> using KDDecodersN = KDDecoders<typename KDCoders<COMP>::type>;

template<> class KDDecoder<Direct> {
	dynamic_bitarray<uint32>	bits;
	int							pos;
public:
	KDDecoder(file_version file) : bits(file, file.get<uint32>() * 8), pos(0) {}
	bool	bit() { return bits[pos++ ^ 31]; }
	uint32	get(int nbits) {
		pos += nbits;
		uint32	nlow = pos & 31;
		if (nlow && nlow < nbits) {
			return (read_bits(bits.begin() + ((pos - nlow - 1) ^ 31), nbits - nlow) << nlow)
				|	read_bits(bits.begin() + ((pos - 1) ^ 31), nlow);
		}
		return read_bits(bits.begin() + ((pos - 1) ^ 31), nbits);
	}
};

template<> class KDDecoder<RAns> : ProbBuffer {
	ANSdecoder		decoder;
public:
	KDDecoder(file_version file) : ProbBuffer(file), decoder(buffer) {}
	bool	bit() { return decoder.get_bit(prob_zero) > 0; }
	uint32	get(int nbits) {
		uint32 result = 0;
		while (nbits--)
			result = (result << 1) + bit();
		return result;
	}
};

template<typename D> class KDDecoder<Folded32<D>> : array<KDDecoder<D>,32>, KDDecoder<D> {
public:
	KDDecoder(file_version file) : array<KDDecoder<D>,32>(file), KDDecoder<D>(file) {}
	uint32	get(int nbits) {
		uint32 result = 0;
		for (int i = 0; i < nbits; ++i)
			result = (result << 1) + (*this)[i].bit();
		return result;
	}
};

template<> class KDDecoder<Axis<void>> {
public:
	KDDecoder(file_version file) { file.seek_cur(file.get<uint32>()); }
	uint32	GetAxis(uint32 size, const dynamic_array<uint32> &levels, uint32 last_axis) {
		return inc_mod(last_axis, num_elements(levels));
	}
};

template<typename D> class KDDecoder<Axis<D>> : KDDecoder<D> {
public:
	KDDecoder(file_version file) : KDDecoder<D>(file) {}
	uint32	GetAxis(uint32 size, const dynamic_array<uint32> &levels, uint32 last_axis) {
		if (size >= 64)
			return this->get(4);

		uint32 best_axis = 0;
		for (uint32 axis = 1; axis < levels.size(); ++axis) {
			if (levels[best_axis] > levels[axis])
				best_axis = axis;
		}
		return best_axis;
	}
};

template<class DECODERS, class C> bool KDDecodePoints(DECODERS &&decoders, uint32 dimension, uint32 bit_length, C&& output) {
	struct Entry {
		dynamic_array<uint32>	base, levels;
		uint32					size;
		uint32					axis;
	};

	temp_array<Entry>	stack(32 * dimension + 1);
	Entry				*sp = stack.begin();

	sp->axis				= 0;
	sp->base.resize(dimension, 0);
	sp->levels.resize(dimension, 0);
	sp++->size	= output.size();

	auto	out	= output.begin();

	while (sp != stack.begin()) {
		--sp;
		auto	size	= sp->size;

		const uint32 axis = decoders.axis.GetAxis(size, sp->levels, sp->axis);
		if (axis >= dimension)
			return false;

		const uint32 level = sp->levels[axis];

		if (bit_length == level) {
			// all axes have been fully subdivided, just output points
			while (size--)
				*out++ = sp->base;

		} else if (size <= 2) {
			// fast decoding of remaining bits if number of points is 1 or 2
			ISO_ASSERT(size != 0);
			while (size--) {
				for (uint32 i = dimension, a = axis; i--; a = inc_mod(a, dimension)) {
					if (auto remaining_bits = bit_length - sp->levels[a])
						sp->base[a] |= decoders.remaining.get(remaining_bits);
				}
				*out++ = sp->base;
			}

		} else {
			uint32 number		= decoders.numbers.get(highest_set_index(size));
			uint32 first_half	= size / 2 - number;
			uint32 second_half	= size - first_half;

			if (first_half != second_half && !decoders.half.bit())
				swap(first_half, second_half);
			
			sp->axis			= axis;
			sp->levels[axis]	+= 1;

			if (first_half)
				sp++->size	= first_half;

			if (second_half) {
				if (first_half) {
					sp->base	= sp[-1].base;		// copy
					sp->levels	= sp[-1].levels;	// copy
					sp->axis	= axis;
				}
				sp->base[axis]	+= 1 << (bit_length - level - 1);
				sp++->size		= second_half;
			}
		}
	}
	return true;
}

bool Reader::ReadPointCloudKD(istream_ref file) {
	num_points = file.get<int>();
	dec.resize(file.getc());

	for (auto& d : dec)
		d.ReadAttributes(file, version);

	file_version filev(file, version);

	for (auto& d : dec) {
		uint8		compression_level;
		uint32		bit_length, num_points2;

		if (version < VER_KD_UPDATE) {
			enum KDEncoding : uint8 {
				KD_QUANTIZATION,
				KD_INTEGER
			} method;
			uint32		compression_level0;

			if (!iso::read(file, method, compression_level, num_points2) || num_points2 == 0)
				return false;

			switch (method) {
				case KD_QUANTIZATION: {
					enum PointCloudCompressionMethod : uint8 {
						KDTREE	= 1,
					};
					switch (file.get<uint32>()) {
						case 3:
							if (file.get<PointCloudCompressionMethod>() != KDTREE)
								return false;
							break;
						case 2:
							break;
						default:
							return false;
					}
					auto	&a = dec[0].attributes[0];
					a.SetCoding(Coding::KD_FLOAT3);
					a.coding->Read(file);
					a.values.resize(num_points * 3 * sizeof(int));

					if (!iso::read(file, num_points2) || num_points2 == 0)
						return false;

					if (!iso::read(file, compression_level0, bit_length, num_points2) || compression_level0 > 6 || bit_length > 31 || num_points2 == 0)
						return false;
					break;
				}
				case KD_INTEGER: {
					for (auto &a : d.attributes) {
						a.SetCoding(Coding::KD_INTEGER);
						a.values.resize(a.num_components * num_points * sizeof(int));
					}

					if (!iso::read(file, bit_length, num_points2) || bit_length > 32 || num_points2 == 0)
						return false;
					break;
				}
				default:
					return false;
			}

		} else {
			for (auto &a : d.attributes) {
				a.SetCoding(IsFloat(a.data_type) ? Coding::QUANTIZATION : Coding::KD_INTEGER);
				a.values.resize(a.num_components * num_points * sizeof(int));
			}

			if (!iso::read(file, compression_level, bit_length, num_points2) || bit_length > 32 || num_points2 == 0)
				return false;

		}

		bool	r;
		auto	flat = GetFlattener(d, num_points);
		switch (compression_level) {
			case 0:	case 1:	r = KDDecodePoints(KDDecodersN<0>(filev), flat.dimension, bit_length, flat); break;
			case 2:	case 3: r = KDDecodePoints(KDDecodersN<2>(filev), flat.dimension, bit_length, flat); break;
			case 4: case 5: r = KDDecodePoints(KDDecodersN<4>(filev), flat.dimension, bit_length, flat); break;
			default:		r = KDDecodePoints(KDDecodersN<6>(filev), flat.dimension, bit_length, flat); break;
		}
		if (!r)
			return false;

		if (version >= VER_KD_UPDATE) {
			// read all float coding
			for (auto &a : d.attributes) {
				if (a.data_type == DT_FLOAT32)
					a.ReadCodingData(file);
			}
			// read all int coding
			for (auto &a : d.attributes) {
				if (a.data_type != DT_FLOAT32)
					a.ReadCodingData(file);
			}
		}
	}
	return true;
}
#endif	//DRACO_ENABLE_READER

//-----------------------------------------------------------------------------
// write point cloud KD
//-----------------------------------------------------------------------------
#ifdef DRACO_ENABLE_WRITER
template<typename T> class KDEncoder;
template<typename T> struct KDEncoders;
template<typename N, typename R, typename H, typename A> struct KDEncoders<type_list<N,R,H,A>> {
	ostream_ref		file;
	KDEncoder<N>	numbers;
	KDEncoder<R>	remaining;
	KDEncoder<A>	axis;
	KDEncoder<H>	half;
	KDEncoders(ostream_ref file) : file(file) {}
	~KDEncoders() {
		bool ok = numbers.flush(file) && remaining.flush(file) && axis.flush(file) && half.flush(file);
		ISO_ASSERT(ok);
	}
};
template<int COMP> using KDEncodersN = KDEncoders<typename KDCoders<COMP>::type>;

template<> class KDEncoder<Direct> {
	dynamic_array<uint32>	bits;
	uint32	local_bits	= 0;
	uint8	num_local	= 0;
public:
	void put(bool bit) {
		local_bits |= int(bit) << (31 - num_local);
		if (++num_local == 32) {
			bits.push_back(local_bits);
			num_local = 0;
			local_bits = 0;
		}
	}
	void putn(int nbits, uint32 value) {
		const int remaining = 32 - num_local;
		if (nbits <= remaining) {
			local_bits |= value << (remaining - nbits);
			if ((num_local += nbits) == 32) {
				bits.push_back(local_bits);
				local_bits = 0;
				num_local = 0;
			}
		} else {
			num_local = nbits - remaining;
			bits.push_back(local_bits | value >> num_local);
			local_bits = value << (32 - num_local);
		}
	}
	bool flush(ostream_ref file) {
		bits.push_back(local_bits);
		const uint32 size_in_byte = bits.size32() * 4;
		file.write(size_in_byte);
		return file.writebuff(bits, size_in_byte);
	}
};

template<> class KDEncoder<RAns> {
	dynamic_array<uint32>	bits;
	uint32	local_bits	= 0;
	uint8	num_local	= 0;
public:
	void put(bool bit) {
		local_bits |= int(bit) << num_local;
		if (++num_local == 32) {
			bits.push_back(local_bits);
			num_local = 0;
			local_bits = 0;
		}
	}
	void putn(int nbits, uint32 value) {
		const uint32 reversed = reverse_bits(value);
		const int remaining = 32 - num_local;
		if (nbits <= remaining) {
			local_bits |= reversed >> (remaining - nbits);
			if ((num_local += nbits) == 32) {
				bits.push_back(local_bits);
				local_bits = 0;
				num_local = 0;
			}
		} else {
			num_local = nbits - remaining;
			bits.push_back(local_bits | (reversed << num_local));
			local_bits = reversed >> (32 - remaining);//?check
		}
	}
	bool flush(ostream_ref file) {
		bits.push_back(local_bits);
		return ProbBuffer(make_range(bit_pointer<const uint32>(bits.begin()), bit_pointer<const uint32>(bits.end() - 1) + num_local)).write(file);
	}
};

template<typename D> class KDEncoder<Folded32<D>> : array<KDEncoder<D>,32>, KDEncoder<D> {
public:
	void putn(int nbits, uint32 value) {
		for (int i = 0; i < nbits; ++i)
			(*this)[i].put((value >> (nbits - 1 - i)) & 1);
	}
	bool flush(ostream_ref file) {
		for (auto &i : *this)
			i.flush(file);
		return KDEncoder<D>::flush(file);
	}
};

template<> class KDEncoder<Axis<void>> {
public:
	template<class I> uint32 GetAxis(uint32 size, I begin, const dynamic_array<uint32> &levels, const dynamic_array<uint32> &base, uint32 bit_length, uint32 last_axis) {
		return inc_mod(last_axis, num_elements(levels));
	}
	bool flush(ostream_ref file) {
		return file.write(0);
	}
};

template<typename D> class KDEncoder<Axis<D>> : public KDEncoder<D> {
public:
	template<class I> uint32 GetAxis(uint32 size, I begin, const dynamic_array<uint32> &levels, const dynamic_array<uint32> &base, uint32 bit_length, uint32 last_axis) {
		uint32	best_axis	= 0;
		uint32	dimensions	= levels.size();

		if (size < 64) {
			for (uint32 i = 1; i < dimensions; ++i) {
				if (levels[best_axis] > levels[i])
					best_axis = i;
			}

		} else {
			uint32	max_value = 0;
			for (uint32 i = 0; i < dimensions; ++i) {
				uint32	num_remaining_bits	= bit_length - levels[i];
				if (num_remaining_bits > 0) {
					const uint32 split = base[i] + (1 << (num_remaining_bits - 1));
					uint32	deviations = 0;
					for (auto p = begin, end = p + size; p != end; ++p)
						deviations += ((*p)[i] < split);
					deviations = max(size - deviations, deviations);

					if (max_value < deviations) {
						max_value = deviations;
						best_axis = i;
					}
				}
			}
			this->putn(4, best_axis);
		}
		return best_axis;
	}
};

template<class DECODERS, class C> void KDEncodePoints(DECODERS &&coders, uint32 dimension, uint32 bit_length, C&& input) {
	struct Entry {
		dynamic_array<uint32>	base, levels;
		iterator_t<C>			begin;
		uint32					size;
		uint32					axis;
	};

	dynamic_array<Entry>	stack(32 * dimension + 1);

	Entry					*sp = stack.begin();
	sp->axis				= 0;
	sp->base.resize(dimension, 0);
	sp->levels.resize(dimension, 0);
	sp->begin				= input.begin();
	sp++->size	= input.size();

	while (sp != stack.begin()) {
		--sp;
		auto	size	= sp->size;

		const uint32 axis	= coders.axis.GetAxis(size, sp->begin, sp->levels, sp->base, bit_length, sp->axis);
		const uint32 level	= sp->levels[axis];

		if (bit_length == level) {
			// all axes have been fully subdivided, nothing to do

		} else if (size <= 2) {
			// fast encoding of remaining bits if number of points is 1 or 2
			ISO_ASSERT(size != 0);
			auto	p = sp->begin;
			while (size--) {
				for (uint32 i = dimension, a = axis; i--; a = inc_mod(a, dimension)) {
					if (auto remaining_bits = bit_length - sp->levels[a])
						coders.remaining.putn(remaining_bits, (*p)[a]);
				}
				++p;
			}

		} else {
			auto	base2		= sp->base[axis] + (1 << (bit_length - level - 1));
			auto	split		= partition(sp->begin, sp->begin + size, [axis, base2](auto a) { return a[axis] < base2; });
			uint32	first_half	= split - sp->begin;
			uint32	second_half	= size - first_half;

			if (first_half != second_half)
				coders.half.put(first_half < second_half);

			coders.numbers.putn(highest_set_index(size), size / 2 - min(first_half, second_half));

			sp->axis			= axis;
			sp->levels[axis]	+= 1;

			if (first_half) {
				sp++->size	= first_half;
				if (second_half) {
					sp->base	= sp[-1].base;		// copy
					sp->levels	= sp[-1].levels;	// copy
					sp->axis	= axis;
				}
			}
			if (second_half) {
				sp->base[axis]	= base2;
				sp++->size		= second_half;
			}
		}
	}
}

bool Writer::WritePointCloudKD(ostream_ref file) {
	file.write(num_points);
	file.putc(dec.size());

	for (auto& d : dec)
		d.WriteAttributes(file);

	for (auto& d : dec) {
		AttributeFlattener	flat(num_points);
		uint32				bit_length	= 0;

		for (auto &a : d.attributes) {
			uint32	bits = a.coding_method == Coding::QUANTIZATION
				? ((Coding::T<Coding::QUANTIZATION>*)a.coding.get())->quantization_bits
				: highest_set_index(((Coding::T<Coding::KD_INTEGER>*)a.coding.get())->used_bits);
			bit_length	= max(bit_length, bits);
			flat.add(a.values, a.num_components);
		}

		file.write(uint8(d.kd_compression), bit_length, num_points);

		switch (d.kd_compression) {
			case 0:	case 1: KDEncodePoints(KDEncodersN<0>(file), flat.dimension, bit_length, flat); break;
			case 2:	case 3: KDEncodePoints(KDEncodersN<2>(file), flat.dimension, bit_length, flat); break;
			case 4: case 5: KDEncodePoints(KDEncodersN<4>(file), flat.dimension, bit_length, flat); break;
			default:		KDEncodePoints(KDEncodersN<6>(file), flat.dimension, bit_length, flat); break;
		}

		// write all float coding
		for (auto &a : d.attributes) {
			if (a.coding_method != Coding::KD_INTEGER)
				a.WriteCodingData(file);
		}
		// write all int coding
		for (auto &a : d.attributes) {
			if (a.coding_method == Coding::KD_INTEGER)
				a.WriteCodingData(file);
		}
	}
	return true;
}
#endif	//DRACO_ENABLE_WRITER
#endif	//DRACO_ENABLE_POINT_CLOUD_KD
#endif	//DRACO_ENABLE_POINT_CLOUD

//-----------------------------------------------------------------------------
// Reader
//-----------------------------------------------------------------------------
#ifdef DRACO_ENABLE_READER

const Attribute* Reader::GetAttribute(int unique_id) const {
	for (auto& d : dec) {
		for (auto& a : d.attributes) {
			if (a.unique_id == unique_id)
				return &a;
		}
	}
	return nullptr;
}

dynamic_array<int32> Reader::PointToValue(const Decoder &d) const {
	if (int *corner_to_value = d.type == Decoder::VERTEX_ATTRIBUTE ? dec[0].corner_to_value : d.corner_to_value) {
		dynamic_array<int32>	point_to_value(num_points, -1);
		for (int p = 0; p < num_points; p++)
			point_to_value[p] = corner_to_value[point_to_corner[p]];
		return point_to_value;
	}
	return int_range(num_points);
}

bool Reader::ReadMetaData(istream_ref file) {
	for (auto num = get_leb128<uint32>(file); num--;)
		att_metadata.put(get_leb128<uint32>(file)).read(file);
	return file_metadata.read(file);
}

bool Reader::read(istream_ref file) {
	Header	h;
	if (!file.read(h) || !h.valid())
		return false;

	version = h.version();

	if (h.has_meta() && !ReadMetaData(file))
		return false;

	bool	ret = false;
	switch (h.mode()) {
	#ifdef DRACO_ENABLE_POINT_CLOUD
		case POINT_CLOUD_SEQUENTIAL:		ret = ReadPointCloudSequential(file); break;
		#ifdef DRACO_ENABLE_POINT_CLOUD_KD
		case POINT_CLOUD_KD:				ret = ReadPointCloudKD(file); break;
		#endif
	#endif
	#ifdef DRACO_ENABLE_MESH
		case TRIANGULAR_MESH_SEQUENTIAL:	ret = ReadMeshSequential(file); break;
		#ifdef DRACO_ENABLE_MESH_EDGEBREAKER
		case TRIANGULAR_MESH_EDGEBREAKER:	ret = ReadMeshEdgebreaker(file); break;
		#endif
	#endif
	}
	if (ret) {
		if (h.mode() & IS_TRIANGULAR_MESH) {
			point_to_corner.resize(num_points);
			for (auto &i : corner_to_point)
				point_to_corner[i] = corner_to_point.index_of(i);
		} else {
			point_to_corner = int_range(num_points);
		}
	}
	return ret;
}
#endif

//-----------------------------------------------------------------------------
// Writer
//-----------------------------------------------------------------------------
#ifdef DRACO_ENABLE_WRITER

Attribute* Writer::AddAttribute(uint32 unique_id, Attribute::Type type, DataType data_type, uint32 num_components, bool normalised, int quantisation) {
	Coding::Method	coding_method = (mode & MODE0_MASK) == POINT_CLOUD_KD
		? (!IsFloat(data_type) ? Coding::KD_INTEGER : Coding::QUANTIZATION)
		: (!IsFloat(data_type) ? Coding::INTEGER : type == Attribute::NORMAL ? Coding::NORMALS : Coding::QUANTIZATION);

	if (quantisation == 0) {
		switch (type) {
			case Attribute::POSITION:	quantisation = 11;	break;
			case Attribute::TEX_COORD:	quantisation = 10;	break;
			default:					quantisation = 8;	break;
		}
	}

	int	i = mode & USE_SINGLE_CONNECTIVITY ? 0 : type;

	if (dec.size() <= i) {
		dec.resize(i + 1);
		dec[i].kd_compression = KD_COMPRESSION(mode);
	}

	return &dec[i].attributes.emplace_back(unique_id, type, data_type, num_components, normalised, coding_method, quantisation);
}

bool Writer::WriteMetaData(ostream_ref file) {
	write_leb128(file, att_metadata.size());
	for (auto& i : att_metadata) {
		write_leb128(file, i.a);
		i.b.write(file);
	}
	return file_metadata.write(file);
}

bool Writer::write(ostream_ref file) {
	bool	has_meta	= !file_metadata.empty() || !att_metadata.empty();
	file.write(Header(mode, has_meta));

	if (has_meta && !WriteMetaData(file))
		return false;

	if (mode & IS_TRIANGULAR_MESH) {
		num_points	= reduce<op_max>(corner_to_point) + 1;
		point_to_corner.resize(num_points, -1);
		for (auto &i : corner_to_point)
			point_to_corner[i] = corner_to_point.index_of(i);
	}

	switch (mode & MODE0_MASK) {
	#ifdef DRACO_ENABLE_POINT_CLOUD
		case POINT_CLOUD_SEQUENTIAL:		return WritePointCloudSequential(file);
		#ifdef DRACO_ENABLE_POINT_CLOUD_KD
		case POINT_CLOUD_KD:				return WritePointCloudKD(file);
		#endif
	#endif
	#ifdef DRACO_ENABLE_MESH
		case TRIANGULAR_MESH_SEQUENTIAL:	return WriteMeshSequential(file);
		#ifdef DRACO_ENABLE_MESH_EDGEBREAKER
		case TRIANGULAR_MESH_EDGEBREAKER:	return WriteMeshEdgebreaker(file);
		#endif
	#endif
		default:							return false;
	}
}
#endif
}	//namespace draco
