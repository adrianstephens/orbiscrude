#ifndef	PROB_CODER_H
#define	PROB_CODER_H

#include "base/defs.h"
#include "base/interval.h"

namespace iso {
//-----------------------------------------------------------------------------
//	PROBS
//-----------------------------------------------------------------------------

struct prob_code {
	typedef uint8	prob;
	typedef int8	tree_index;
	typedef const tree_index tree[];
	enum {
		PROB_MAX	= 255,
		PROB_HALF	= 128
	};
	static uint8	get_norm(uint8 i) {
		static const uint8 norm[256] = {
			0, 7, 6, 6, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4,
			3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
			2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
			2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
			1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
			1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
			1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
			1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
		};
		return norm[i];
	}
	static uint16	get_cost(uint8 i) {
		static const uint16 cost[257] = {
			4096, 4096, 3584, 3284, 3072, 2907, 2772, 2659, 2560, 2473, 2395, 2325,	2260, 2201, 2147, 2096,
			2048, 2003, 1961, 1921, 1883, 1847, 1813, 1780,	1748, 1718, 1689, 1661, 1635, 1609, 1584, 1559,
			1536, 1513, 1491, 1470,	1449, 1429, 1409, 1390, 1371, 1353, 1335, 1318, 1301, 1284, 1268, 1252,
			1236, 1221, 1206, 1192, 1177, 1163, 1149, 1136, 1123, 1110, 1097, 1084,	1072, 1059, 1047, 1036,
			1024, 1013, 1001, 990,  979,  968,  958,  947,	937,  927,  917,  907,  897,  887,  878,  868,
			859,  850,  841,  832,	823,  814,  806,  797,  789,  780,  772,  764,  756,  748,  740,  732,
			724,  717,  709,  702,  694,  687,  680,  673,  665,  658,  651,  644,	637,  631,  624,  617,
			611,  604,  598,  591,  585,  578,  572,  566,	560,  554,  547,  541,  535,  530,  524,  518,
			512,  506,  501,  495,	489,  484,  478,  473,  467,  462,  456,  451,  446,  441,  435,  430,
			425,  420,  415,  410,  405,  400,  395,  390,  385,  380,  375,  371,	366,  361,  356,  352,
			347,  343,  338,  333,  329,  324,  320,  316,	311,  307,  302,  298,  294,  289,  285,  281,
			277,  273,  268,  264,	260,  256,  252,  248,  244,  240,  236,  232,  228,  224,  220,  216,
			212,  209,  205,  201,  197,  194,  190,  186,  182,  179,  175,  171,	168,  164,  161,  157,
			153,  150,  146,  143,  139,  136,  132,  129,	125,  122,  119,  115,  112,  109,  105,  102,
			99,   95,   92,   89,	86,   82,   79,   76,   73,   70,   66,   63,   60,   57,   54,   51,
			48,   45,   42,   38,   35,   32,   29,   26,   23,   20,   18,   15,	12,   9,    6,    3,
			3
		};
		return cost[i];
	}

	static force_inline prob		get_prob(int num, int den)		{ return den == 0 ? 128 : clamp(((int64)num * 256 + (den >> 1)) / den, 1, PROB_MAX); }
	static force_inline prob		get_binary_prob(int n0, int n1) { return get_prob(n0, n0 + n1);	}

	static force_inline prob		weighted_prob(prob prob1, prob prob2, int factor) {
		return round_pow2(prob1 * (256 - factor) + prob2 * factor, 8);
	}
	static force_inline prob		merge_probs(prob prev, uint32 ct0, uint32 ct1, uint32 count_sat, uint32 max_update_factor) {
		return weighted_prob(prev, get_binary_prob(ct0, ct1), max_update_factor * min(ct0 + ct1, count_sat) / count_sat);
	}

	// normal merge
	static force_inline prob		merge_probs(prob prev, uint32 ct0, uint32 ct1, const prob *factors, uint32 nfactors) {
		uint32	den = ct0 + ct1;
		if (den == 0)
			return prev;
		return weighted_prob(prev, get_prob(ct0, den), factors[min(den, nfactors)]);
	}
	template<uint32 N> static force_inline prob merge_probs(prob prev, uint32 ct0, uint32 ct1, const prob (&factors)[N]) {
		return merge_probs(prev, ct0, ct1, factors, N);
	}
	template<uint32 N> static force_inline prob merge_probs(prob prev, const uint32 ct[2], const prob (&factors)[N]) {
		return merge_probs(prev, ct[0], ct[1], factors, N);
	}
	static void force_inline merge_probs(prob *probs, prob *prev, const uint32 (*counts)[2], uint32 count, const prob *factors, uint32 nfactors) {
		for (uint32 i = 0; i < count; i++)
			probs[i] = merge_probs(prev[i], counts[i][0], counts[i][1], factors, nfactors);
	}

	// tree merge
	static uint32	merge_probs_recurse(uint32 i, const tree_index *tree, const prob *prev_probs, const uint32 *counts, prob *probs, const prob *factors, uint32 nfactors) {
		const int		left		= tree[i];
		const uint32	left_count	= left <= 0 ? counts[-left] : merge_probs_recurse(left, tree, prev_probs, counts, probs, factors, nfactors);
		const int		right		= tree[i + 1];
		const uint32	right_count	= right <= 0 ? counts[-right] : merge_probs_recurse(right, tree, prev_probs, counts, probs, factors, nfactors);

		probs[i >> 1]	= merge_probs(prev_probs[i >> 1], left_count, right_count, factors, nfactors);
		return left_count + right_count;
	}
	static force_inline void		merge_probs(const tree_index *tree, const prob *prev_probs, const uint32 *counts, prob *probs, const prob *factors, uint32 nfactors) {
		merge_probs_recurse(0, tree, prev_probs, counts, probs, factors, nfactors);
	}
	template<uint32 N> static force_inline void merge_probs(const tree_index *tree, const prob *prev_probs, const uint32 *counts, prob *probs, const prob (&factors)[N]) {
		merge_probs_recurse(0, tree, prev_probs, counts, probs, factors, N);
	}

	//cost
	static uint32	cost0(prob p)			{ return get_cost(p); }
	static uint32	cost1(prob p)			{ return get_cost(256 - p); }
	static uint32	cost(prob p, bool bit)	{ return get_cost(bit ? 256 - p : p); }
	static uint32	cost_branch256(prob p, const uint32 ct[2]) { return ct[0] * cost0(p) + ct[1] * cost1(p); }

	static uint32	cost(const tree_index *tree, const prob *probs, int bits, int len) {
		int			total	= 0;
		tree_index	i		= 0;
		while (len--) {
			int bit		= (bits >> len) & 1;
			total		+= cost(probs[i >> 1], !!bit);
			i			= tree[i + bit];
		}
		return total;
	}
	static void		cost(uint32 *costs, const tree_index *tree, const prob *probs, tree_index i, int c) {
		const prob p = probs[i / 2];
		for (int b = 0; b < 2; ++b) {
			int			cc = c + cost(p, b != 0);
			tree_index	ii = tree[i + b];
			if (ii <= 0)
				costs[-ii] = cc;
			else
				cost(costs, tree, probs, ii, cc);
		}
	}

	static void		cost(uint32 *costs, const prob *probs, const tree_index *tree) {
		cost(costs, tree, probs, 0, 0);
	}
	static void		cost_skip(uint32 *costs, const prob *probs, const tree_index *tree) {
		costs[-tree[0]] = cost0(probs[0]);
		cost(costs, tree, probs, 2, 0);
	}

	//tokens
	struct token {
		int		value;
		int		len;
	};
	static void	tokens_from_tree(token *tokens, const tree_index *tree, int i, int v, int l) {
		v += v;
		++l;
		do {
			const tree_index j = tree[i++];
			if (j <= 0) {
				tokens[-j].value	= v;
				tokens[-j].len		= l;
			} else {
				tokens_from_tree(tokens, tree, j, v, l);
			}
		} while (++v & 1);
	}
	static void tokens_from_tree(token *tokens, const tree_index *tree) {
		tokens_from_tree(tokens, tree, 0, 0, 0);
	}

	static uint32 convert_distribution(const tree_index *tree, uint32 branch_ct[][2], const uint32 num_events[], uint32 i = 0) {
		uint32 left		= tree[i + 0] <= 0 ? num_events[-tree[i + 0]] : convert_distribution(tree, branch_ct, num_events, tree[i + 0]);
		uint32 right	= tree[i + 1] <= 0 ? num_events[-tree[i + 1]] : convert_distribution(tree, branch_ct, num_events, tree[i + 1]);
		branch_ct[i >> 1][0] = left;
		branch_ct[i >> 1][1] = right;
		return left + right;
	}
};

//-----------------------------------------------------------------------------
//	READER
//-----------------------------------------------------------------------------

template<typename T, typename S> class prob_decoder : prob_code {
	S			file;
public:
	T			bits_buffer	= 0;
	int			count		= -8;
	uint8		range		= PROB_MAX;
protected:
	void		fill();
public:
	prob_decoder(prob_decoder &&b) = default;
	template<typename P> prob_decoder(const P &file) : file(file) {}
	template<typename P> void init(const P &_file) {
		file		= _file;
		bits_buffer = 0;
		count		= -8;
		range		= PROB_MAX;
	}
	S&			reader() { return file; }

	force_inline bool read(int prob) {
		if (count < 0)
			fill();

		uint8	range		= this->range;
		T		bits_buffer	= this->bits_buffer;
		uint8	split		= (range * prob + (256 - prob)) >> 8;
		T		bigsplit	= (T)split << ((sizeof(T) - 1) * 8);
		bool	bit			= bits_buffer >= bigsplit;
		if (bit) {
			range		-= split;
			bits_buffer -= bigsplit;
		} else {
			range		= split;
		}
		uint32	shift	= get_norm(range);
		this->range			= range << shift;
		this->bits_buffer	= bits_buffer <<= shift;
		count				-= shift;
		return bit;
	}

	force_inline int read(const prob *probs, int n) {
		int	r = 0;
		for (int i = 0; i < n; ++i)
			r = (r << 1) | int(read(probs[i]));
		return r;
	}
	template<int N> force_inline int read(const prob (&probs)[N]) {
		return read(probs, N);
	}
#if 0
	bool read_bit() {
		return read(PROB_HALF);
	}
	int read_literal(int bits) {
		int r = 0;
		while (bits--)
			r = (r << 1) | int(read_bit());
		return r;
	}
#else
	force_inline bool read_bit() {
		if (count < 0)
			fill();

		uint32	split		= range + (range & 1);
		T		bigsplit	= (T)split << ((sizeof(T) - 1) * 8 - 1);
		bool	bit			= bits_buffer >= bigsplit;
		if (bit) {
			range		&= ~1;
			bits_buffer	= (bits_buffer - bigsplit) << 1;
			--count;
		} else if (split != 0x100) {
			range		= split;
			bits_buffer	<<= 1;
			--count;
		} else {
			range		= 0x80;
		}

		return bit;
	}
	force_inline int read_literal(int bits) {
		int r = 0;

		T		bits_buffer1	= bits_buffer;
		int		count1			= count;
		uint32	range1			= range;

		if (count1 < bits - 1) {
			T	t(0);
			auto	read	= file.readbuff(&t, ((sizeof(T) - 1) * 8 - count1) / 8);
			bits_buffer1	|= swap_endian(t) >> (count1 + 8);
			count1			+= read * 8;
		}

		while (bits--) {
			r <<= 1;
			uint32	split		= range1 + (range1 & 1);
			T		bigsplit	= (T)split << ((sizeof(T) - 1) * 8 - 1);
			if (bits_buffer1 >= bigsplit) {
				bits_buffer1 -= bigsplit;
				range1	&= ~1;
				++r;
			} else {
				range1	= split;
			}
			bits_buffer1	<<= 1;
			--count1;
		}
		if (range1 == 0x100) {
			bits_buffer1	>>= 1;
			range1			= 0x80;
			++count1;
		}
		bits_buffer	= bits_buffer1;
		count		= count1;
		range		= range1;

		return r;
	}
#endif
	force_inline int	read_tree(const tree_index *t, const prob *p) {
		tree_index i = 0;
		do
			i = t[i + read(p[i >> 1])];
		while (i > 0);
		return -i;
	}
	void restore_unused() {
		file.seek_cur(-(count / 8));
	}
};
template<typename T, typename S> void prob_decoder<T,S>::fill() {
	T	t(0);
	auto read	= file.readbuff(&t, sizeof(T) - 1);
	bits_buffer	|= swap_endian(t) >> (count + 8);
	count		+= read * 8;
}

#if 0
template<typename T, typename S> class prob_decoder_both {
	typedef prob_code::prob			prob;
	typedef prob_code::tree_index	tree_index;
	typedef const tree_index		tree[];

	prob_decoder<T,S>	a;
	prob_decoder2<T,S>	b;
	bool	equiv() const {
		int	shift = 56 - min(a.count, b.count);
		return (a.bits_buffer >> shift) == (b.bits_buffer >> shift) && a.range == b.range;

	}
public:
	prob_decoder_both() {}
	template<typename P> prob_decoder_both(P &_file) : a(_file), b(_file) {}
	template<typename P> void init(P &_file) {
		a.init(_file);
		b.init(_file);
	}
	S&			reader() { return a.reader(); }

	bool read(int prob) {
		auto	savea = a;
		auto	saveb = b;
		for (;;) {
			bool	bita = a.read(prob);
			bool	bitb = b.read(prob);
			if (bita == bitb && equiv())
				return bita;
			a = savea;
			b = saveb;
		}
	}

	int read(const prob *probs, int n) {
		int	r = 0;
		for (int i = 0; i < n; ++i)
			r = (r << 1) | int(read(probs[i]));
		return r;
	}
	template<int N> int read(const prob (&probs)[N]) {
		return read(probs, N);
	}
	bool read_bit() {
		bool	bita = a.read_bit();
		bool	bitb = b.read_bit();
		ISO_ASSERT(bita == bitb);
		return bita;
	}
	int read_literal(int bits) {
		auto	savea = a;
		auto	saveb = b;
		for (;;) {
			int	lita = a.read_literal(bits);
			int	litb = b.read_literal(bits);
			if (lita == litb && equiv())
				return lita;
			a = savea;
			b = saveb;
		}
	}
	int	read_tree(const tree_index *t, const prob *p) {
		tree_index i = 0;
		do
			i = t[i + read(p[i >> 1])];
		while (i > 0);
		return -i;
	}
	void restore_unused() {
		a.restore_unused();
	}
};

// FFVP9 implementation:

struct RangeCoder {
    int			high;
	int			bits; // stored negated (i.e. negative "bits" is a positive number of bits left) in order to eliminate a negate in cache refilling
	const uint8 *buffer;
	const uint8 *end;
	uint32		code_word;

	uint32 renorm() {
		static const uint8 norm_shift[256]= {
			8,7,6,6,5,5,5,5,4,4,4,4,4,4,4,4,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
			1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
			0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
			0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		};
		int		shift		= norm_shift[high];
		int		bits		= this->bits;
		uint32	code_word	= this->code_word;

		high		<<= shift;
		code_word	<<= shift;
		bits		+= shift;
		if (bits >= 0 && buffer < end) {
			code_word	|= ((buffer[0] << 8) | buffer[1]) << bits;
			bits		-= 16;
			buffer		+= 2;
		}
		this->bits = bits;
		return code_word;
	}

	int get_prob(uint8 prob) {
		uint32	code_word	= renorm();
		uint32	low			= 1 + (((high - 1) * prob) >> 8);
		uint32	low_shift	= low << 16;
		int		bit			= code_word >= low_shift;

		this->high		= bit ? high - low : low;
		this->code_word	= bit ? code_word - low_shift : code_word;
		return bit;
	}
	int get() {
		return get_prob(128);
	}
	int get_uint(int bits) {
		int v = 0;
		while (bits--)
			v = (v << 1) | get();
		return v;
	}
	int get_tree(const int8 (*tree)[2], const uint8 *probs) {
		int i = 0;
		do
			i = tree[i][get_prob(probs[i])];
		while (i > 0);
		return -i;
	}
	int get_coeff(const uint8 *prob) {	//zero terminated (unlike libvpx)
		int v = 0;
		do
			v = (v << 1) + get_prob(*prob++);
		while (*prob);
		return v;
	}

	RangeCoder(const uint8 *buf, int buf_size) {
		high		= 255;
		bits		= -16;
		code_word	= (buf[0] << 16) | (buf[1] << 8) | buf[2];
		buffer		= buf + 3;
		end			= buf + buf_size;
	}
};
#endif
//-----------------------------------------------------------------------------
//	WRITER
//-----------------------------------------------------------------------------

template<typename T, typename S> class prob_coder : prob_code {
	S			file;
	T			low;
	int			count;
	uint8		range;
	uint8		last_byte;
	uint32		num_ff;
protected:
	void	dump(uint32 bits);

	void	flush() {
		for (int i = 0; i < 32; i++)
			write_bit(false);
	}
public:
	template<typename P> prob_coder(P &_file) : file(_file), count(-24), low(0), range(PROB_MAX), num_ff(0) {}
	template<typename P> void init(P &_file) {
		file		= _file;
		count		= -24;
		low			= 0;
		range		= PROB_MAX;
		num_ff		= 0;
	}
	S&			writer() { return file; }

	void write(bool bit, int probability) {
		uint32	range0	= range;
		uint32	split	= 1 + (((range0 - 1) * probability) >> 8);
		if (bit) {
			low		+= split;
			range0	-= split;
		} else {
			range0	= split;
		}

		uint32	shift = get_norm(range0);
		range	= range0 << shift;
		count	+= shift;

		if (count >= 0) {
			uint32	offset = shift - count;
			shift = count;
			dump(offset);
		}

		low	<<= shift;
	}

	void write_bit(int bit) {
		write(bit, PROB_HALF);
	}
	void write_literal(int data, int bits) {
		while (bits--)
			write_bit((data >> bits) & 1);
	}
	void write_prob(uint8 v) {
		write_literal(v, 8);
	}
	void write_tree(const tree_index *tree, const prob *probs, int bits, int len, tree_index i) {
		while (len--) {
			const int bit = (bits >> len) & 1;
			write(!!bit, probs[i >> 1]);
			i = tree[i + bit];
		}
	}
	void write_token(const tree_index *tree, const prob *probs, const token &tok) {
		write_tree(tree, probs, tok.value, tok.len, 0);
	}
};

template<typename T, typename S> void prob_coder<T, S>::dump(uint32 bits) {
	if (low & (1 << (sizeof(T) * 8 - bits))) {
		file.putc(last_byte + 1);
		while (num_ff) {
			file.putc(0);
			--num_ff;
		}
		last_byte = 0;
	}

	T		t		= low << bits;
	uint8	byte	= t >> ((sizeof(T) - 1) * 8);

	low		= t & (~0xff << ((sizeof(T) - 1) * 8));
	count	-= 8;

	if (byte == PROB_MAX) {
		num_ff++;
	} else {
		file.putc(last_byte);
		while (num_ff) {
			file.putc(PROB_MAX);
			--num_ff;
		}
		last_byte = byte;
	}
}


} // namespace iso


#endif	// PROB_CODER_H
