/*
   LZ4 - Fast LZ compression algorithm
   Copyright (C) 2011-2014, Yann Collet.
   BSD 2-Clause License (http://www.opensource.org/licenses/bsd-license.php)

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:

       * Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
       * Redistributions in binary form must reproduce the above
   copyright notice, this list of conditions and the following disclaimer
   in the documentation and/or other materials provided with the
   distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   You can contact the author at :
   - LZ4 source repository : http://code.google.com/p/lz4/
   - LZ4 public forum : https://groups.google.com/forum/#!forum/lz4c
*/

#include "lz4.h"

using namespace iso;
using namespace LZ4;

//-----------------------------------------------------------------------------
//	LZ4 decoder
//-----------------------------------------------------------------------------

template<typename SRC, typename WIN> const uint8* LZ4_decode(uint8 *&dst, uint8 *dst_end, SRC src, const WIN win) {
	const uint8* restrict	ip		= src.begin();
	uint8*					op		= dst;

	// Main Loop
	for (const uint8* ip0 = ip;; ip = ip0) {
		uint32	token = *ip0++;

		// get lit_length
		size_t	 L = token >> ML_BITS;
		if (L == RUN_MASK) {
			uint8 s;
			do {
				s = *ip0++;
				L += s;
			} while (likely(src.check(ip + L + 2)) && s == 255);
		}

		// copy literals
		auto	end = op + L;
		if (unlikely(end > dst_end || !src.check(ip0 + L + 2)))
			break;

		loose_copy<uint64>(op, ip0, end);
		ip0 += L;

		// get offset
		size_t	D = load_packed<uint16le>(ip0);
		ip0 += 2;
		const uint8	*ref = end - D;

		if (unlikely(win.check_low_limit(ref)))
			return nullptr;	// Error : offset outside destination buffer

		// get matchlength
		size_t	M = token & ML_MASK;
		if (M == ML_MASK) {
			uint8 s;
			do {
				s = *ip0++;
				M += s;
			} while (likely(src.check(ip0)) && s == 255);
			if (unlikely(!src.check(ip0)))
				break;
		}

		M	+= MINMATCH;
		if (end + M > dst_end)
			break;

		op = end + M;
		win.clipped_copy(end, ref, op, dst_end);
	}

	dst = op;
	return ip;
}

template<typename WIN> const uint8* LZ4::_decoder::process(uint8 *&dst, uint8 *dst_end, const uint8 *src, const uint8 *src_end, TRANSCODE_FLAGS flags, const WIN win) {
	if (flags & TRANSCODE_PARTIAL)
		return LZ4_decode(dst, dst_end, src_with_end(src, src_end), win);

	src = LZ4_decode(dst, dst_end - MINMATCH, src_with_end(src, src_end - LASTLITERALS), win);

	//last literals
	uint32		token	= *src++;
	size_t		L		= token >> ML_BITS;
	if (L == RUN_MASK) {
		uint8 s;
		do {
			s = *src++;
			L += s;
		} while (s == 255 && src + L < src_end);
	}
	if (src + L == src_end) {
		if (dst + L <= dst_end) {
			memcpy(dst, src, L);
			dst += L;
			src += L;
		}
	}
	return src;
}

template const uint8* LZ4::_decoder::process(uint8 *&dst, uint8 *dst_end, const uint8 *src, const uint8 *src_end, TRANSCODE_FLAGS flags, const prefix_window);
template const uint8* LZ4::_decoder::process(uint8 *&dst, uint8 *dst_end, const uint8 *src, const uint8 *src_end, TRANSCODE_FLAGS flags, const external_window);


// no src_size
template<typename WIN> const uint8* LZ4::_decoder::process(uint8 *&dst, uint8 *dst_end, const uint8 *src, TRANSCODE_FLAGS flags, const WIN win) {
	if (flags & TRANSCODE_PARTIAL)
		return LZ4_decode(dst, dst_end, src_without_end(src), win);

	src = LZ4_decode(dst, dst_end - MINMATCH, src_without_end(src), win);

	//last literals
	uint32		token	= *src++;
	size_t		L		= token >> ML_BITS;
	if (L == RUN_MASK) {
		uint8 s;
		do {
			s = *src++;
			L += s;
		} while (s == 255);
	}
	if (dst + L == dst_end) {
		memcpy(dst, src, L);
		dst += L;
		src	+= L;
	}
	return src;
}

//-----------------------------------------------------------------------------
// LZ4 encoder
//-----------------------------------------------------------------------------

static constexpr uint32 hash(uint32 i)	{ return (i * 2654435761U) >> (MINMATCH * 8 - HASH_BITS); }
static uint32	hash(const void* p)		{ return hash(load_packed<uint32>(p)); }

force_inline uint8 *put_count(uint8 *op, int count) {
	if (count >= RUN_MASK) {
		count -= RUN_MASK;
		for (; count > 254; count -= 255)
			*op++ = 255;
		*op++ = (uint8)count;
	}
	return op;
}

template<typename DST, typename WIN> const uint8* _encoder::process(DST dst, const uint8 *src, const uint8 *src_end, TRANSCODE_FLAGS flags, const WIN win) {
	if (src_end - src > MAX_INPUT_SIZE)
		return nullptr;

	check_overflow();

	const uint8*		base		= src - offset;
	bool				last		= !(flags & TRANSCODE_PARTIAL);

	const uint8*		ip			= src;
	const uint8* const	mflimit		= src_end - MFLIMIT;
	const uint8* const	match_limit	= src_end - LASTLITERALS;

	uint8*				op			= dst.begin();
	uint8*				token		= op++;

	// First Byte
	put(base, hash(ip), ip);

	// Main Loop
	while (ip < mflimit) {
		const uint8*	ref;
		const uint8*	lit_end;
		const uint8*	next_ip		= ip + 1;
		uint32			searchMatch	= 1 << SKIPSTRENGTH;

		// Find a match
		do {
			lit_end		= next_ip;
			next_ip		+= searchMatch++ >> SKIPSTRENGTH;

			if (unlikely(next_ip > mflimit))
				break;

			uint32	h	= hash(lit_end);
			ref			= lookup(base, h);
			put(base, h, lit_end);

		} while (ref + WSIZE <= lit_end || load_packed<uint32>(win.adjust_ref(ref)) != load_packed<uint32>(lit_end));

		if (unlikely(next_ip > mflimit))
			break;

		// Catch up
		const uint8	*begin = win.match_begin(lit_end, ref, ip);
		ref		-= lit_end - begin;
		lit_end	= begin;

		// Encode lit_length
		int lit_length = int(lit_end - ip);
		if (unlikely(!dst.check(op + lit_length + (1 + 2 + LASTLITERALS) + (lit_length / 255)))) {
			last = false;	// dst full
			break;
		}

		*token	= min(lit_length, RUN_MASK) << ML_BITS;
		op		= put_count(op,  lit_length);

		// Copy Literals
		loose_copy<uint64>(op, ip, op + lit_length);
		ip		+= lit_length;
		op		+= lit_length;

		do {
			// Get match_len
			uint32	match_len = (uint32)win.match_len(ip + MINMATCH, ref + MINMATCH, match_limit);

			if (unlikely(!dst.check(op + (3 + LASTLITERALS) + (match_len >> 8)))) {
				// dst full
				offset	+= ip - src;
				return ip;
			}

			// Encode Offset
			store_packed<uint16le>(op, ip - ref);
			op		+= 2;

			// Encode match_len
			*token += min(match_len, ML_MASK);
			op		= put_count(op, match_len);

			// Ready for next
			token	= op++;
			*token	= 0;

			ip		+= MINMATCH + match_len;

			// Test end of chunk
			if (unlikely(ip > mflimit))
				break;

			// Fill table
			put(base, hash(ip - 2), ip - 2);

			// Test next position
			uint32	h = hash(ip);
			ref = lookup(base, h);
			put(base, h, ip);

		} while (ref + WSIZE > ip && load_packed<uint32>(win.adjust_ref(ref)) == load_packed<uint32>(ip));
	}

	if (last) {
		// Encode last literals
		int lit_length = int(src_end - ip);
		if (dst.check(op + lit_length + 1 + (lit_length + 255 - RUN_MASK) / 255)) {
			*token = min(lit_length, RUN_MASK) << ML_BITS;
			op = put_count(op, lit_length);
			memcpy(op, ip, lit_length);
			ip += lit_length;
			op += lit_length;
		}
	}
	offset	+= ip - src;
	return ip;
}

//-----------------------------------------------------------------------------
//	LZ4HC encoder
//-----------------------------------------------------------------------------

struct HChasher {
	const uint8*	base;
	uint32*			hashes;
	uint16*			chains;
	const uint8*	nextToUpdate;

	HChasher(const uint8* base, uint32* hashes, uint16* chains, const uint8 *src) : base(base), hashes(hashes), chains(chains), nextToUpdate(src)  {}

	const uint8*	lookup(uint16 h)			const			{ return hashes[h] + base; }
	const uint8*	lookup(const uint8 *p)		const			{ return lookup(hash(p)); }
	const uint8*	get_next(const uint8 *p)	const			{ return p - chains[(p - base) & (WSIZE - 1)]; }

	void			set_head(const uint8 *p, uint16 h)			{ hashes[h]	= uint32(p - base); }
	void			set_head(const uint8 *p)					{ set_head(p, hash(p)); }

	void			set_next(const uint8 *p, uint16 d)			{ chains[(p - base) & (WSIZE - 1)] = d; }
	void			set_next(const uint8 *p, const uint8 *n)	{ set_next(p, (uint16)min(p - n, WSIZE - 1)); }

	void			put(const uint8 *p, uint16 h)				{ set_next(p, lookup(h)); set_head(p, h); }
	void			put(const uint8 *p)							{ put(p, hash(p)); }

	template<typename WIN> int	InsertAndFindBestMatch(const uint8* src, const uint8* src_end, const uint8*& match_pos, int attempts, WIN win);
	template<typename WIN> int	InsertAndGetWiderMatch(const uint8* src, const uint8* src_begin, const uint8* src_end, int match_len, const uint8*& match_pos, const uint8*& start_pos, int attempts, WIN win);
};

#define REPEAT_OPTIMIZATION

template<typename WIN> inline int HChasher::InsertAndFindBestMatch(const uint8* src, const uint8* src_end, const uint8*& match_pos, int attempts, WIN win) {
	while (nextToUpdate < src)
		put(nextToUpdate++);

	const uint8*	ref			= lookup(src);
	size_t			match_len	= 0;

#ifdef REPEAT_OPTIMIZATION
	// Detect repetitive sequences of length <= 4
	size_t			repl	= 0;
	size_t			delta	= src - ref;
	if (delta <= 4) {
		match_len	= repl = win.match_len(src, ref, src_end);
		match_pos	= ref;
		ref			= get_next(ref);
	}
#endif

	for (;attempts-- && size_t(src - ref) < WSIZE; ref = get_next(ref)) {
		if (ref[match_len] == src[match_len]) {
			size_t match_len2 = win.match_len(src, ref, src_end);
			if (match_len2 > match_len) {
				match_len	= match_len2;
				match_pos	= ref;
			}
		}
	}

#ifdef REPEAT_OPTIMIZATION
	// Complete table
	if (repl) {
		const uint8* end	= src + repl - MINMATCH;
		const uint8* head	= end - delta - 1;
		while (src < end)
			set_next(src++, (uint16)delta);
		while (head < end)
			set_head(head++);
		nextToUpdate = end;
	}
#endif
	return (int)match_len;
}


template<typename WIN> inline int HChasher::InsertAndGetWiderMatch(const uint8* src, const uint8* src_begin, const uint8* src_end, int match_len, const uint8*& match_pos, const uint8*& start_pos, int attempts, WIN win) {
	while (nextToUpdate < src)
		put(nextToUpdate++);

	int		delta	= int(src - src_begin);

	for (const uint8 *ref = lookup(src); attempts-- && size_t(src - ref) < WSIZE; ref = get_next(ref)) {
		if (src_begin[match_len] == *win.adjust_ref(ref + (match_len - delta))) {
			const uint8* end1	= win.match_end(src, ref, src_end);
			const uint8* begin1	= win.match_begin(src, ref, src_begin);

			if (end1 - begin1 > match_len) {
				match_len	= int(end1 - begin1);
				match_pos	= begin1 + (ref - src);
				start_pos	= begin1;
			}
		}
	}

	return match_len;
}

force_inline uint8 *put_sequence(const uint8 *src, uint8 *op, const uint8 *anchor, int match_len, const uint8* ref, uint8* dst_end) {
	int		lit_length	= int(src - anchor);
	match_len -= MINMATCH;

	if (dst_end && op + lit_length + (2 + 2 + LASTLITERALS) + (lit_length >> 8) + (match_len >> 8) > dst_end)
		return nullptr;

	uint8	*token  = op++;

	// Encode lit_length
	op = put_count(op, lit_length);

	// Copy literals
	loose_copy<uint64>(op, anchor, op + lit_length);
	op += lit_length;

	// Encode offset
	store_packed<uint16le>(op, uint16(src - ref));
	op += 2;

	// Encode match_len
	op = put_count(op, match_len);

	*token	= (min(lit_length, RUN_MASK) << ML_BITS) + min(match_len, RUN_MASK);
	return op;
}

template<typename DST, typename WIN> const uint8 *_HCencoder::process(DST dst, const uint8 *src, const uint8 *src_end, TRANSCODE_FLAGS flags, const WIN win) {
	if (src_end - src > MAX_INPUT_SIZE)
		return nullptr;

	// check for address space overflow
	check_overflow();

	const uint8*		ip1			= src;
	const uint8*		anchor		= ip1;
	const uint8* const	mflimit		= src_end - MFLIMIT;
	const uint8* const	match_limit	= src_end - LASTLITERALS;

	uint8*				op			= dst.begin();

	HChasher	hasher(ip1 - offset, hashes, chains, ip1);
	ip1++;

	// Main Loop
	while (ip1 < mflimit) {
		const uint8		*ip2	= ip1;
		const uint8*	ref2	= nullptr;
		int				ml2		= hasher.InsertAndFindBestMatch(ip2, match_limit, ref2, attempts, win);
		if (!ml2) {
			++ip1;
			continue;
		}

		const uint8		*ip0	= ip2;
		const uint8		*ref0	= ref2, *ref1;
		int				ml0		= ml2,	ml1;

	_Search2:
		do {
			ip1		= ip2;
			ref1	= ref2;
			ml1		= ml2;
			ml2		= ip1 + ml1 < mflimit ? hasher.InsertAndGetWiderMatch(ip1 + ml1 - 2, ip1 + 1, match_limit, ml1, ref2, ip2, attempts, win) : ml1;

			if (ml2 == ml1)// No better match?
				break;

			if (ip0 < ip1 && ip2 < ip1 + ml0) { // empirical
				ip2		= ip0;
				ref2	= ref0;
				ml2		= ml0;
				continue;
			}

		} while (ip2 - ip1 < 3);

		if (ml2 == ml1) {// No better match
			if (uint8 *op1 = put_sequence(ip1, op, anchor, ml1, ref1, dst.end()))
				op = op1;
			else {
				dst.set(op);
				return ip1;
			}

			anchor = ip1 += ml1;
			continue;	//main loop
		}

		for (;;) {
			// Currently we have ml2 > ml1, and ip1 + 3 <= ip2 (usually < ip1 + ml1)
			int	t = int(ip2 - ip1);
			if (t < OPTIMAL_ML) {
				int new_ml = min(min(ml1, OPTIMAL_ML), t + ml2 - MINMATCH);
				int	correction = new_ml - t;
				if (correction > 0) {
					ip2		+= correction;
					ref2	+= correction;
					ml2		-= correction;
				}
			}

			// Now, we have ip2 = ip1+new_ml, with new_ml = min(ml1, OPTIMAL_ML=18)
			const uint8	*ip3, *ref3;
			int			ml3 = ip2 + ml2 < mflimit ? hasher.InsertAndGetWiderMatch(ip2 + ml2 - 3, ip2, match_limit, ml2, ref3, ip3, attempts, win) : ml2;

			if (ml3 == ml2) {
				// No better match : 2 sequences to encode
				// ip1 & ref1 are known; Now for ml1
				if (ip2 < ip1 + ml1)
					ml1 = int(ip2 - ip1);

				if (uint8 *op1 = put_sequence(ip1, op, anchor, ml1, ref1, dst.end()))
					op = op1;
				else {
					dst.set(op);
					return ip1;
				}

				if (uint8 *op1 = put_sequence(ip2, op, ip1 + ml1, ml2, ref2, dst.end())) {
					op = op1;
				} else {
					dst.set(op);
					return ip1;
				}
				
				anchor	= ip1 = ip2 + ml2;
				break;	// back to main loop
			}

			if (ip3 < ip1 + ml1 + 3) { 
				// Not enough space for match 2 : remove it
				if (ip3 >= ip1 + ml1) {
					// can write Seq1 immediately ==> Seq2 is removed, so Seq3 becomes Seq1
					if (ip2 < ip1 + ml1) {
						int correction = int(ip1 + ml1 - ip2);
						ip2		+= correction;
						ref2	+= correction;
						ml2		-= correction;
						if (ml2 < MINMATCH) {
							ip2		= ip3;
							ref2	= ref3;
							ml2		= ml3;
						}
					}

					if (uint8 *op1 = put_sequence(ip1, op, anchor, ml1, ref1, dst.end())) {
						op = op1;
					} else {
						dst.set(op);
						return ip1;
					}

					anchor	= ip1 + ml1;
				
					ip0		= ip2;
					ref0	= ref2;
					ml0		= ml2;

					ip2		= ip3;
					ref2	= ref3;
					ml2		= ml3;
					goto _Search2;
				}

			} else {
				// now we have 3 ascending matches; write at least the first one; ip1 & ref1 are known; Now for ml1
				if (ip2 < ip1 + ml1) {
					int	t = int(ip2 - ip1);
					if (t < ML_MASK) {
						ml1 = min(min(ml1, OPTIMAL_ML), t + ml2 - MINMATCH);
						int	correction = ml1 - t;
						if (correction > 0) {
							ip2		+= correction;
							ref2	+= correction;
							ml2		-= correction;
						}
					} else {
						ml1 = t;
					}
				}

				if (uint8 *op1 = put_sequence(ip1, op, anchor, ml1, ref1, dst.end())) {
					op = op1;
				} else {
					dst.set(op);
					return ip1;
				}

				anchor	= ip1 + ml1;

				ip1		= ip2;
				ref1	= ref2;
				ml1		= ml2;
			}

			ip2		= ip3;
			ref2	= ref3;
			ml2		= ml3;
		}
	}

	if (!(flags & TRANSCODE_PARTIAL)) {
		// Encode Last Literals
		int last_run = int(src_end - anchor);
		if (dst.check(op + last_run + 1 + (last_run + 255 - RUN_MASK) / 255)) {
			*op++	= min(last_run, RUN_MASK) << ML_BITS;
			op		= put_count(op, last_run);
			memcpy(op, anchor, last_run);
			op		+= last_run;
			ip1		= src_end;
		}
	}

	offset	+= ip1 - src;
	dst.set(op);
	return ip1;
}

//static bool _test = test_codec(LZ4::HCencoder(), LZ4::decoder());
