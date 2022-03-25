#ifndef APPLE_COMPRESSION_H
#define APPLE_COMPRESSION_H

#include "window.h"
#include "utilities.h"

#undef ERROR

namespace iso {

namespace PackBits {
	struct decoder {
		static const uint8*	process(uint8 *&dst, uint8 *dst_end, const uint8 *src, const uint8 *src_end, TRANSCODE_FLAGS flags);
	};
	struct encoder {
		static const uint8*	process(uint8 *&dst, uint8 *dst_end, const uint8 *src, const uint8 *src_end, TRANSCODE_FLAGS flags);
	};
}

namespace ADC {
	struct decoder {
		static const uint8*	process(uint8 *&dst, uint8 *dst_end, const uint8 *src, const uint8 *src_end, TRANSCODE_FLAGS flags);
	};
}

namespace LZVN {
// L: is the number of literal bytes
// M: is the number of match bytes
// D: is the match "distance"; the distance in bytes between the current pointer and the start of the match
	enum {
		L_MAX		= 271,
		WSIZE	= 1 << 16,
	};

	class _decoder : public codec_defaults {
		int			L, M, D;		// Partially expanded match, or 0,0,0, in which case, src points to the next literal to copy, or the next op-code if L==0
		void		save(int _L, int _M) { L = _L; M = _M; }
	protected:
		template<typename WIN> const uint8*	process(uint8 *&dst, uint8 *dst_end, const uint8 *src, const uint8 *src_end, TRANSCODE_FLAGS flags, const WIN win);

	public:
		bool		end_of_stream;	// Did we decode end-of-stream?
		const uint8*	process(uint8 *&dst, uint8 *dst_end, const uint8 *src, const uint8 *src_end, TRANSCODE_FLAGS flags);
		void		init() { save(0, 0); D = 0; end_of_stream = false; }
		_decoder() : L(0), M(0), D(0), end_of_stream(false) {}
	};

	using decoder = decoder_with_window<_decoder, WSIZE>;

	class _encoder : public codec_defaults {
		enum {
			HASH_BITS			= 14,
			MIN_MARGIN			= 8,	// min number of bytes required between current and end during encoding, MUST be >= 8
			MAX_LITERAL_BACKLOG	= 400	// if the number of pending literals exceeds this size, emit a long literal, MUST be >= 271
		};
		struct match {
			const uint8 *begin, *end;	// range of match
			int		D;					// match distance D
			int		K;					// match gain: M - distance storage (L not included)
			static int32 cost(const uint8 *begin, const uint8 *end, int D) {
				return int32(end - begin) - (D == 0 ? 1 : D < 0x600 ? 2 : 3);
			}
			match()	: K(0)	{}
			explicit operator bool() const { return K != 0; }
			void	clear()	{ K = 0; }
			void	set(const uint8 *_begin, const uint8 *_end, int32 _D, int32 _K)	{
				begin = _begin; end = _end; K = _K; D = _D;
			}
		};
		struct hash_entry {
			struct entry {
				int32	index;		// signed index in source buffer
				uint32	value;		// corresponding 32-bit value
			} entries[4];
			void	push(int32 i, uint32 v) {
				// rotate values, so we will replace the oldest
				entries[3]	= entries[2];
				entries[2]	= entries[1];
				entries[1]	= entries[0];
				entries[0].index	= i;
				entries[0].value	= v;
			}
		};

		uint32		offset;
		int			lit_offset;
		match		pending;
		int			d_prev;								// Distance for last emitted match, or 0
//		hash_entry	hash_table[1 << HASH_BITS];
		temp_var<hash_entry[1 << HASH_BITS]>	hash_table;

		static inline uint32 hash(uint32 i) { return (((i & 0xffffff) * (1 + (1 << 6) + (1 << 12))) >> 12) & ((1 << HASH_BITS) - 1); }

		static void		find_better_match(const uint8 *src, int D, const uint8 *src_low, const uint8 *src_end, uint32 match0, match &m, bool use_prevd = false);
		static uint8*	emit_match(match match, uint8 *dst, uint8 *dst_end, const uint8 *lit, int prevd);

	protected:
		template<typename WIN> const uint8*	process(uint8 *&dst, uint8 *dst_end, const uint8 *src, const uint8 *src_end, TRANSCODE_FLAGS flags, WIN win);
	public:
		const uint8*	process(uint8 *&dst, uint8 *dst_end, const uint8 *src, const uint8 *src_end, TRANSCODE_FLAGS flags);
		_encoder();
	};

	using encoder = encoder_with_window<_encoder, WSIZE>;

}

namespace LZFSE {
	enum {
		L_STATES	= 64,		L_SYMBOLS	= 20,		L_MAX	= 315,
		M_STATES	= 64,		M_SYMBOLS	= 20,		M_MAX	= 2359,
		D_STATES	= 256,		D_SYMBOLS	= 64,		D_MAX	= 262139,
		LIT_STATES	= 1024,		LIT_SYMBOLS	= 256,
		MATCHES_PER_BLOCK	= 10000,
	};

	struct bit_stream {
		uint64	accum;			// Input bits
		int		accum_nbits;	// Number of valid bits in ACCUM, other bits are 0
		bit_stream() : accum(0), accum_nbits(0) {}

		inline bool		input(int n, const uint8 *&pbuf);
		inline void		flush_in(const uint8 *&pbuf);
		inline uint64	pull(int n);

		inline void		flush_out(uint8 *&pbuf);
		inline void		finish(uint8 *&pbuf);
		inline void		push(int n, uint64 b);
	};

	struct state;
	struct frequencies;

	class _decoder {
		union lit_entry {
			uint32	u;
			struct {
				int8	k;			// Number of bits to read
				uint8	symbol;		// Emitted symbol
				int16	delta;		// Signed increment used to compute next state (+bias)
			};
		};
		union value_entry {
			uint64	u;
			struct {
				uint8	total_bits; // state bits + extra value bits = shift for next decode
				uint8	value_bits; // extra value bits
				int16	delta;		// state base (delta)
				int32	vbase;		// value base
			};
		};

		bit_stream		in;
		const uint8*	src;						// Offset of L,M,D encoding in the input buffer. Because we read through an FSE stream *backwards* while decoding, this is decremented as we move through a block

		uint32			n_matches;					// Number of matches remaining in the block.
		const uint8*	lit;						// Pointer to the next literal to emit
		int32			L, M, D;					// L, M, D triplet for the match currently being emitted
		uint16			l_state, m_state, d_state;	// The current state of the L, M, and D FSE decoders

		// Internal FSE decoder tables for the current block
		value_entry		l_decoder[L_STATES];
		value_entry		m_decoder[M_STATES];
		value_entry		d_decoder[D_STATES];
		lit_entry		lit_decoder[LIT_STATES];

		// The literal stream for the block, plus padding to allow for faster copy operations
		uint8			literals[MATCHES_PER_BLOCK * 4 + 64];

		inline void		save(int32 _L, int32 _M, int32 _D, const uint8 *_lit, uint32 _n_matches, const uint8 *_src);
		static void		init_decoder(lit_entry *__restrict t, int nstates, const uint16 *__restrict freq, int nsymbols);
		static void		init_decoder(value_entry *__restrict t, int nstates, const uint8 *__restrict bits, const int32 *__restrict base, int nsymbols, const uint16 *__restrict freq);
		static uint8	decode(bit_stream &in, uint16 &state, const lit_entry *decoder);
		static int32	decode(bit_stream &in, uint16 &state, const value_entry *decoder);
	public:
		size_t			process(void *dst0, size_t dst_size);
		bool			init(const state &p, const frequencies &f, const uint8 *lits_end);
	};

	class _encoder {
	protected:
		enum {
			HASH_WIDTH			= 4,
			HASH_BITS			= 14,
			GOOD_MATCH			= 40,
			MAX_MATCH_LENGTH	= 100 * M_MAX,
		};
		struct match {
			int64	pos;		// Offset of the first byte in the match
			int64	ref;		// First byte of the source -- the earlier location in the buffer with the same contents
			uint32	length;		// Length of the match
		};
		struct hash_entry {
			int32	pos[HASH_WIDTH];
			uint32	value[HASH_WIDTH];
		};

		const uint8 *src;
		int64		src_current, src_end;
		int64		src_literal;						// Offset of the first byte of the next literal to encode in the source buffer
		match		pending;							// Pending match; will be emitted unless a better match is found
		uint64		n_matches;							// The number of matches written so far
		uint64		n_literals;							// The number of literals written so far
		uint32		l_values[MATCHES_PER_BLOCK];		// Lengths of found literals
		uint32		m_values[MATCHES_PER_BLOCK];		// Lengths of found matches
		uint32		d_values[MATCHES_PER_BLOCK];		// Distances of found matches
		uint8		literals[MATCHES_PER_BLOCK * 4];	// Concatenated literal bytes

		hash_entry	hash_table[1 << HASH_BITS];

		static inline uint32 hash(uint32 x)			{ return (x * 2654435761u) >> (32 - HASH_BITS); } // Knuth multiplicative hash
		match		make_literals(int64 L)	const	{ return { src_literal + L, src_literal + L - 1, 0}; }

		frequencies get_frequencies();
		uint8*		write_state(state& p, const frequencies &f, uint8* dst, uint8* dst_end);
		uint8*		encode_matches(uint8 *dst, uint8 *dst_end, uint64 src_size);
		bool		push_lmd(uint32 L, uint32 M, uint32 D);
		bool		try_push_match(const match &match);
		uint8*		push_match(const match &match, uint8 *dst, uint8 *dst_end, uint64 src_size);
	public:
		_encoder(const void *src, size_t src_size);//, uint8 *dst, size_t dst_size);

		void		slide(int64 delta, uint64 end);
		uint8*		process(uint8 *dst, uint8 *dst_end);
		uint8*		finish(uint8 *dst, uint8 *dst_end, uint64 src_size);
		uint64		consumed() const { return src_current; }
	};

class decoder : public codec_defaults {
	uint32		block_magic;	// magic number of the current block
	bool		end_of_stream;
	uint32		block_src_size, block_dst_size;

	union {
		uncompressed_decoder	uncompressed;
		LZVN::_decoder			lzvn;
		LZFSE::_decoder			lzfse;
	};

public:
	decoder() : block_magic(0), end_of_stream(false) {}
	const uint8*	process(uint8 *&dst, uint8 *dst_end, const uint8 *src, const uint8 *src_end, TRANSCODE_FLAGS flags);
};

class encoder : public codec_defaults {
	enum {LZVN_THRESHOLD = 4096};
	static const uint8*	finish(uint8 *&dst, uint8 *dst_end, const uint8 *src, TRANSCODE_FLAGS flags);
public:
	static const uint8*	process(uint8 *&dst, uint8 *dst_end, const uint8 *src, const uint8 *src_end, TRANSCODE_FLAGS flags);
};


}

} //namespace iso


#endif //APPLE_COMPRESSION_H
