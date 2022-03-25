#include "apple_compression.h"
#include "base/bits.h"

using namespace iso;

//-----------------------------------------------------------------------------
//	PackBits
//-----------------------------------------------------------------------------

const uint8* PackBits::decoder::process(uint8 *&_dst, uint8 *dst_end, const uint8 *src, const uint8 *src_end, TRANSCODE_FLAGS flags) {
	uint8		*dst	= _dst;

	while (src < src_end && dst < dst_end) {
		uint8	n = *src;

		if (n & 0x80) {
			// replicate next byte -n + 1 times
			if (n == 0x80)	// nop
				continue;

			n = ~n + 2;
			if (src + 1 == src_end || dst + n > dst_end)
				break;

			memset(dst, src[1], n);
			src		+= 2;

		} else {
			// copy next n + 1 bytes literally
			++n;
			if (src + 1 + n > src_end || dst + n > dst_end)
				break;

			memcpy(dst, ++src, n);
			src		+= n;
		}

		dst	+= n;
	}

	_dst = dst;
	return src;
}

const uint8* PackBits::encoder::process(uint8 *&_dst, uint8 *dst_end, const uint8 *src, const uint8 *src_end, TRANSCODE_FLAGS flags) {
	enum { BASE, LITERAL, RUN, LITERAL_RUN } state = BASE;

	uint8		*dst	= _dst;
	uint8*		lastliteral = 0;

	while (src < src_end) {
		// Find the longest string of identical bytes
		uint8	b = *src++;
		int		n = 1;
		for (; src < src_end && b == *src; src++)
			n++;

		for (;;) {
			switch (state) {
				case BASE:			// initial state, set run/literal
					if (n > 1) {
						state = RUN;
						if (n > 128) {
							*dst++ = (uint8)-127;
							*dst++ = b;
							n -= 128;
							continue;
						}
						*dst++ = (uint8)(-(n - 1));
						*dst++ = b;
					} else {
						lastliteral = dst;
						*dst++	= 0;
						*dst++	= b;
						state	= LITERAL;
					}
					break;

				case LITERAL:		// last object was literal string
					if (n > 1) {
						state = LITERAL_RUN;
						if (n > 128) {
							*dst++ = (uint8)-127;
							*dst++ = b;
							n -= 128;
							continue;
						}
						// encode run
						*dst++ = uint8(-(n - 1));
						*dst++ = b;
					} else {
						// extend literal
						if (++(*lastliteral) == 127)
							state = BASE;
						*dst++ = b;
					}
					break;

				case RUN:			// last object was run
					if (n > 1) {
						if (n > 128) {
							*dst++ = (uint8)-127;
							*dst++ = b;
							n -= 128;
							continue;
						}
						*dst++ = (uint8)(-(n - 1));
						*dst++ = b;
					} else {
						lastliteral = dst;
						*dst++	= 0;
						*dst++	= b;
						state	= LITERAL;
					}
					break;

				case LITERAL_RUN:	// literal followed by a run
					//Check to see if previous run should be converted to a literal, in which case we convert literal-run-literal to a single literal
					if (n == 1 && dst[-2] == 0xff && *lastliteral < 126) {
						state = ((*lastliteral) += 2) == 127 ? BASE : LITERAL;
						dst[-2] = dst[-1];	// replicate
					} else {
						state = RUN;
					}
					continue;
			}
			break;
		}
	}

	_dst = dst;
	return src;
}

//static bool _test_packbits = test_codec(PackBits::encoder(), PackBits::decoder());
//-----------------------------------------------------------------------------
//	ADC
//-----------------------------------------------------------------------------

const uint8* ADC::decoder::process(uint8 *&_dst, uint8 *dst_end, const uint8 *src, const uint8 *src_end, TRANSCODE_FLAGS flags) {
	uint8		*dst	= _dst;

	while (src < src_end) {
		uint8	byte = *src;
		int		chunk_size;

		if (byte & 0x80) {
			chunk_size = (byte & 0x7F) + 1;
			if (src + 1 + chunk_size > src_end || dst + chunk_size > dst_end)
				break;
			memcpy(dst, ++src, chunk_size);
			src		+= chunk_size;

		} else {
			int		offset;
			if (byte & 0x40) {
				chunk_size	= (byte & 0x3F) + 4;
				if (src + 3 > src_end || dst + chunk_size > dst_end)
					break;
				offset		= (src[1] << 8) + src[2];
				src			+= 3;

			} else {
				chunk_size	= ((byte & 0x3F) >> 2) + 3;
				if (src + 2 > src_end || dst + chunk_size > dst_end)
					break;
				offset		= ((byte & 0x03) << 8) + src[1];
				src			+= 2;
			}
			memcpy(dst, dst - offset - 1, chunk_size);
		}

		dst	+= chunk_size;
	}
	_dst = dst;
	return src;
}

//-----------------------------------------------------------------------------
//	LZVN decode
//-----------------------------------------------------------------------------
template<typename WIN> const uint8*	LZVN::_decoder::process(uint8 *&_dst, uint8 *dst_end, const uint8 *src, const uint8 *src_end, TRANSCODE_FLAGS flags, const WIN win) {
	if (src == src_end)// || dst == dst_end)
		return 0;


	uint8		*dst	= _dst;

	int		L	= this->L;
	int		M	= this->M;
	int		D	= this->D;
	int		opc_len;
	uint8 	opc;

	// Do we have a partially expanded match saved in state?
	if (L || M) {
		opc_len = 0; // we already skipped the op
		this->L = this->M = 0;
		if (M == 0)
			goto copy_literal;
		if (L == 0)
			goto copy_match;
		goto copy_literal_and_match;

	}

	for (;;) {
		switch (opc = src[0]) {
			// ===============================================================
			// Opcodes encoding both a literal and a match
			//SMALL
			case 0:		case 1:		case 2:		case 3:		case 4:		case 5:		case 8:		case 9:
			case 10:	case 11:	case 12:	case 13:	case 16:	case 17:	case 18:	case 19:
			case 20:	case 21:	case 24:	case 25:	case 26:	case 27:	case 28:	case 29:
			case 32:	case 33:	case 34:	case 35:	case 36:	case 37:	case 40:	case 41:
			case 42:	case 43:	case 44:	case 45:	case 48:	case 49:	case 50:	case 51:
			case 52:	case 53:	case 56:	case 57:	case 58:	case 59:	case 60:	case 61:
			case 64:	case 65:	case 66:	case 67:	case 68:	case 69:	case 72:	case 73:
			case 74:	case 75:	case 76:	case 77:	case 80:	case 81:	case 82:	case 83:
			case 84:	case 85:	case 88:	case 89:	case 90:	case 91:	case 92:	case 93:
			case 96:	case 97:	case 98:	case 99:	case 100:	case 101:	case 104:	case 105:
			case 106:	case 107:	case 108:	case 109:	case 128:	case 129:	case 130:	case 131:
			case 132:	case 133:	case 136:	case 137:	case 138:	case 139:	case 140:	case 141:
			case 144:	case 145:	case 146:	case 147:	case 148:	case 149:	case 152:	case 153:
			case 154:	case 155:	case 156:	case 157:	case 192:	case 193:	case 194:	case 195:
			case 196:	case 197:	case 200:	case 201:	case 202:	case 203:	case 204:	case 205:
				// LLMMMDDD DDDDDDDD LITERAL
				opc_len = 2;
				L	= extract_bits(opc, 6, 2);
				M	= extract_bits(opc, 3, 3) + 3;
				if (src + opc_len + L >= src_end)
					break;
				D	= extract_bits(opc, 0, 3) << 8 | src[1];
				goto copy_literal_and_match;

				//MEDIUM
			case 160:	case 161:	case 162:	case 163:	case 164:	case 165:	case 166:	case 167:
			case 168:	case 169:	case 170:	case 171:	case 172:	case 173:	case 174:	case 175:
			case 176:	case 177:	case 178:	case 179:	case 180:	case 181:	case 182:	case 183:
			case 184:	case 185:	case 186:	case 187:	case 188:	case 189:	case 190:	case 191: {
				// 101LLMMM DDDDDDMM DDDDDDDD LITERAL
				opc_len = 3;
				L	= extract_bits(opc, 3, 2);
				if (src + opc_len + L >= src_end)
					break;
				uint16 opc23 = load_packed<uint16>(src + 1);
				M	= (extract_bits(opc, 0, 3) << 2) + extract_bits(opc23, 0, 2) + 3;
				D	= extract_bits(opc23, 2, 14);
				goto copy_literal_and_match;
			}
					//LARGE
			case 7:		case 15:	case 23:	case 31:	case 39:	case 47:	case 55:	case 63:
			case 71:	case 79:	case 87:	case 95:	case 103:	case 111:	case 135:	case 143:
			case 151:	case 159:	case 199:	case 207:
				// LLMMM111 DDDDDDDD DDDDDDDD LITERAL
				opc_len = 3;
				L	= extract_bits(opc, 6, 2);
				M	= extract_bits(opc, 3, 3) + 3;
				if (src + opc_len + L >= src_end)
					break;
				D	= load_packed<uint16>(src + 1);
				goto copy_literal_and_match;

				//PRE_D:
			case 70:	case 78:	case 86:	case 94:	case 102:	case 110:	case 134:	case 142:
			case 150:	case 158:	case 198:	case 206:
				// LLMMM110
				opc_len = 1;
				L	= extract_bits(opc, 6, 2);
				M	= extract_bits(opc, 3, 3) + 3;
				if (src + opc_len + L >= src_end)
					break;
				goto copy_literal_and_match;

			copy_literal_and_match:
				src += opc_len;
				if (likely(dst + 4 <= dst_end && src + 4 <= src_end)) {
					// The literal is 0-3 bytes; if we are not near the end of the buffer, we can safely just do a 4 byte copy
					copy_packed<uint32>(dst, src);
				} else if (dst + L <= dst_end) {
					// We are too close to the end of either the input or output stream to be able to safely use a four-byte copy
					loose_copy<uint8>(dst, src, dst + L);
				} else {
					// Destination truncated: fill DST, and store partial match
					auto	clip = dst_end - dst;
					loose_copy<uint8>(dst, src, dst_end);
					save(L - clip, M);
					src += clip;
					dst = dst_end;
					break;
				}
				dst += L;
				src += L;
				// Check if the match distance is valid; matches must not reference bytes that preceed the start of the output buffer, nor can the match distance be zero
				//if (D > dst - _dst || D == 0)
				//	return;//goto invalid_match_distance;
			copy_match:
				if (auto clip = (int)win.copy(dst, dst - D, dst + M, dst_end)) {
					save(0, M - clip);
					dst = dst_end;
					break;
				}

				dst += M;
				continue;

				// ===============================================================
				// Opcodes representing only a match (no literal)
				// The match distance is carried over from the previous opcode, so all they need to encode is the match length
				//SML_M:
			case 241:	case 242:	case 243:	case 244:	case 245:	case 246:	case 247:	case 248:
			case 249:	case 250:	case 251:	case 252:	case 253:	case 254:	case 255:
				// 1111MMMM
				opc_len = 1;
				if (src + opc_len >= src_end)
					break;
				M	= extract_bits(opc, 0, 4);
				src += opc_len;
				goto copy_match;

				//LRG_M:
			case 240:
				// 11110000 MMMMMMMM
				opc_len = 2;
				if (src + opc_len >= src_end)
					break;
				M	= 16 + src[1];
				src += opc_len;
				goto copy_match;

				// ===============================================================
				// Opcodes representing only a literal (no match)
				//SML_L:
			case 225:	case 226:	case 227:	case 228:	case 229:	case 230:	case 231:	case 232:
			case 233:	case 234:	case 235:	case 236:	case 237:	case 238:	case 239:
				// 1110LLLL LITERAL
				opc_len = 1;
				L	= extract_bits(opc, 0, 4);
				goto copy_literal;

				//LRG_L:
			case 224:
				// 11100000 LLLLLLLL LITERAL
				opc_len = 2;
				if (src + opc_len >= src_end)
					break;
				L	= 16 + src[1];
				goto copy_literal;

			copy_literal:
				// Check that the source buffer is large enough to hold the complete literal and at least the first byte of the next opcode
				if (src + opc_len + L >= src_end)
					break;
				// If so, advance the source pointer to point to the first byte of the literal and adjust the source length accordingly
				src += opc_len;
				// Now we copy the literal from the source pointer to the destination
				if (dst + L + 7 <= dst_end && src + L + 7 <= src_end) {
					// We are not near the end of the source or destination buffers, so we can safely copy the literal using wide copies, without worrying about reading or writing past the end of either buffer
					loose_copy<uint64>(dst, src, dst + L);
				} else if (dst + L <= dst_end) {
					// We are too close to the end of either the input or output stream to be able to safely use an eight-byte copy
					loose_copy<uint8>(dst, src, dst + L);
				} else {
					// Destination truncated: fill DST, and store partial match
					auto	clip = dst_end - dst;
					loose_copy<uint8>(dst, src, dst_end);
					save(L - clip, 0);
					src += clip;
					dst = dst_end;
					break;
				}
				dst += L;
				src += L;
				continue;

				// ===============================================================
				// Other opcodes
				//nop:
			case 14:	case 22:
				opc_len = 1;
				if (src + opc_len >= src_end)
					break;
				src += opc_len;
				continue;

				//eos:
			case 6:
				opc_len = 8;
				if (src + opc_len <= src_end) {// (here we don't need an extra byte for next op code)
					src += opc_len;
					D	= 0;
					end_of_stream = true;
				}
				break;

				// ===============================================================
				// Return on error
				//udef:
			case 30:	case 38:	case 46:	case 54:	case 62:	case 112:	case 113:	case 114:
			case 115:	case 116:	case 117:	case 118:	case 119:	case 120:	case 121:	case 122:
			case 123:	case 124:	case 125:	case 126:	case 127:	case 208:	case 209:	case 210:
			case 211:	case 212:	case 213:	case 214:	case 215:	case 216:	case 217:	case 218:
			case 219:	case 220:	case 221:	case 222:	case 223:
				return nullptr;//ERROR;
		}

		this->D	= D;
		_dst = dst;
		return src;
	}
}

template const uint8*	LZVN::_decoder::process<external_window>(uint8 *&_dst, uint8 *dst_end, const uint8 *src, const uint8 *src_end, TRANSCODE_FLAGS flags, const external_window win);


const uint8*	LZVN::_decoder::process(uint8 *&dst, uint8 *dst_end, const uint8 *src, const uint8 *src_end, TRANSCODE_FLAGS flags) {
	return process(dst, dst_end, src, src_end, flags, prefix_window());
}

//-----------------------------------------------------------------------------
//	LZVN encode
//-----------------------------------------------------------------------------

// Emit (L,0,0) instructions (final literal or literal backlog becomes too high and there is no pending match)
static inline uint8 *emit_literal(const uint8 *src, uint8 *dst, uint8 *dst_end, int L) {
	while (L > 15) {
		int x = min(L, LZVN::L_MAX);
		if (dst + x + 10 >= dst_end)
			return nullptr;

		store_packed<uint16>(dst, 0xE0 + ((uint16(x) - 16) << 8));
		dst	+= 2;
		L	-= x;
		loose_copy<uint8>(dst, src, dst + x);
		dst	+= x;
		src	+= x;
	}
	if (L > 0) {
		if (dst + L + 10 >= dst_end)
			return nullptr;
		*dst++ = uint8(0xE0 + L); // 1110LLLL
		loose_copy<uint8>(dst, src, dst + L);
		dst	+= L;
	}
	return dst;
}

// Emit (L,M,D) instructions. M>=3
static inline uint8 *emit_lmd(const uint8 *src, uint8 *dst, uint8 *dst_end, int L, int M, int D) {
	while (L > 15) {
		int x = min(L, LZVN::L_MAX);
		if (dst + x + 10 >= dst_end)
			return nullptr;

		store_packed<uint16>(dst, 0xE0 + ((uint16(x) - 16) << 8));
		dst	+= 2;
		L	-= x;
		loose_copy<uint64>(dst, src, dst + x);
		dst	+= x;
		src	+= x;
	}
	if (L > 3) {
		if (dst + L + 10 >= dst_end)
			return nullptr;
		*dst++ = uint8(0xE0 + L); // 1110LLLL
		loose_copy<uint64>(dst, src, dst + L);
		dst	+= L;
		src	+= L;
		L	= 0;
	}
	int x	= min(10 - 2 * L, M);
	M	-= x;
	x	-= 3; // M = (x+3) + M' max value for x is 7-2*L

	// Relaxed capacity test covering all cases
	if (dst + 8 >= dst_end)
		return nullptr;

	if (D == 0) {								// dprev
		*dst++ = L == 0
			? uint8(0xF0 + x + 3)				// XM!
			: uint8((L << 6) + (x << 3) + 6);	// LLxxx110

	} else if (D < 2048 - 2 * 256) {
		// Short dist D>>8 in 0..5
		*dst++ = uint8((D >> 8) + (L << 6) + (x << 3)); // LLxxxDDD
		*dst++ = D & 0xFF;

	} else if (D >= (1 << 14) || M == 0 || (x + 3) + M > 34) {
		// Long dist
		*dst++ = uint8((L << 6) + (x << 3) + 7);
		store_packed<uint16>(dst, uint16(D));
		dst	+= 2;

	} else {
		// Medium distance
		x	+= M;
		M	= 0;
		*dst++ = uint8(0xA0 + (x >> 2) + (L << 3));
		store_packed<uint16>(dst, uint16(D << 2 | (x & 3)));
		dst	+= 2;
	}

	if (L) {
		// Here L<4 literals remaining, we copy them here
		copy_packed<uint32>(dst, src);
		dst	+= L;
	}

	// Issue remaining match
	while (M > 15) {
		if (dst + 2 >= dst_end)
			return nullptr;
		int x	= min(M, LZVN::L_MAX);
		store_packed<uint16>(dst, uint16(0xf0 + ((x - 16) << 8)));
		dst	+= 2;
		M	-= x;
	}
	if (M > 0) {
		if (dst + 1 >= dst_end)
			return nullptr;
		*dst++ = uint8(0xF0 + M); // M = 0..15
	}

	return dst;
}

inline uint8 *LZVN::_encoder::emit_match(match m, uint8 *dst, uint8 *dst_end, const uint8 *lit, int prevd) {
	return emit_lmd(lit, dst, dst_end, m.begin - lit, m.end - m.begin, m.D == prevd ? 0 : m.D);
}

#if 0
inline void LZVN::_encoder::find_better_match(const uint8 *src, int D, const uint8 *src_begin, const uint8 *src_end, uint32 match0, match &m, bool use_prevd) {
	// Expand forward
	auto end	= match0 == 0 ? match_end(src + 4, -D, src_end) : src + num_zero_bytes(match0);
	if (end - src >= 3) {
		// Expand backwards over literal
		src = match_begin(src, -D, src_begin);
		auto	K = match::cost(src, end, use_prevd ? 0 : D);
		if (K == m.K ? end > m.end + 1 : K > m.K)
			m.set(src, end, D, K);
	}
}
#endif

template<typename WIN> const uint8* LZVN::_encoder::process(uint8 *&_dst, uint8 *dst_end, const uint8 *src, const uint8 *src_end, TRANSCODE_FLAGS flags, WIN win) {
	bool		last	= !(flags & TRANSCODE_PARTIAL);
	uint8		*dst	= _dst;
	
	if (last) {
		dst_end -= 8;
		src_end -= MIN_MARGIN;
	}

	const uint8	*base	= src - offset;
	const uint8 *lit	= base + lit_offset;

	// First Byte
	//uint32	v		= load_packed<uint32>(src);
	//hash_table[hash(v)].push(src - base, v);

	while (src < src_end) {
		// Get 4 bytes at src
		uint32		v	= load_packed<uint32>(src);
		hash_entry&	e	= hash_table[hash(v)];

		// Do not check matches if still in previously emitted match
		if (src >= lit) {
			match incoming;

			// Check candidates in order (closest first)
			for (auto &i : e.entries) {
				int	D	= src - (base + i.index); // actual distance
				if (D > 0 && D < WSIZE) {
					//find_better_match(src, D, max(lit, base + D), src_end, i.value ^ v, incoming);
					auto	ref		= src - D;
					uint32	match0	= i.value ^ v;
					auto	end		= match0 == 0 ? win.match_end(src + 4, ref + 4, src_end) : src + num_zero_bytes(match0);
					if (end - src >= 3) {
						auto	begin	= win.match_begin(src, ref, lit);
						auto	K		= match::cost(begin, end, D);
						if (K == incoming.K ? end > incoming.end + 1 : K > incoming.K)
							incoming.set(begin, end, D, K);
					}
				}
			}

			// Check candidate at previous distance
			if (d_prev) {
				//find_better_match(src, d_prev, max(lit, base + d_prev), src_end, load_packed<uint32>(win.adjust_ref(src - d_prev)) ^ v, incoming, true);
				auto	ref		= src - d_prev;
				uint32	match0	= load_packed<uint32>(win.adjust_ref(ref)) ^ v;
				auto	end		= match0 == 0 ? win.match_end(src + 4, ref + 4, src_end) : src + num_zero_bytes(match0);
				if (end - src >= 3) {
					auto	begin	= win.match_begin(src, ref, lit);
					auto	K		= match::cost(begin, end, 0);
					if (K == incoming.K ? end > incoming.end + 1 : K > incoming.K)
						incoming.set(src, end, d_prev, K);
				}
			}

			// No incoming match?
			if (!incoming) {
				// If literal backlog becomes too high, emit pending match, or literals if there is no pending match
				if (src - lit >= MAX_LITERAL_BACKLOG) {
					if (pending) {
						if (auto dst2 = emit_match(pending, dst, dst_end, lit, d_prev)) {
							dst			= dst2;
							d_prev		= pending.D;
							lit			= pending.end;
							pending.clear();
						} else {
							last = false;
							break;
						}
					} else {
						if (auto dst2 = emit_literal(lit, dst, dst_end, L_MAX)) {
							dst			= dst2;
							lit			+= L_MAX;
						} else {
							last = false;
							break;
						}
					}
				}

			} else if (!pending) {
				// No pending match, keep incoming
				pending = incoming;

			} else if (pending.end <= incoming.begin) {
				// No overlap, emit pending, keep incoming
				if (auto dst2 = emit_match(pending, dst, dst_end, lit, d_prev)) {
					dst			= dst2;
					d_prev		= pending.D;
					lit			= pending.end;
					pending		= incoming;
				} else {
					last = false;
					break;
				}

			} else {
				// Overlap, emit best
				if (incoming.K > pending.K)
					pending		= incoming;

				if (auto dst2 = emit_match(pending, dst, dst_end, lit, d_prev)) {
					dst			= dst2;
					d_prev		= pending.D;
					lit			= pending.end;
					pending.clear();
				} else {
					last = false;
					break;
				}
			}
		}

		// We commit state changes only after we tried to emit instructions, so we can restart in the same state in case dst was full and we quit the loop
		e.push(src - base, v);
		++src;
	}

	if (last) {
		if (auto dst2 = emit_literal(lit, dst, dst_end, src_end + MIN_MARGIN - lit)) {
			// always safe because we left 8 bytes before end
			store_packed<uint64>(dst2, 0x06);
			dst = dst2 + 8;
			src = src_end + MIN_MARGIN;
		}
	}

	offset		= src - base;
	lit_offset	= lit - base;

	_dst	= dst;
	return src;
}

LZVN::_encoder::_encoder() : offset(0), lit_offset(0), d_prev(0) {
	hash_entry e;
	for (auto &i : e.entries)
		i.index	= -WSIZE;
	for (auto &i : hash_table)
		i = e;
}

const uint8*	LZVN::_encoder::process(uint8 *&dst, uint8 *dst_end, const uint8 *src, const uint8 *src_end, TRANSCODE_FLAGS flags) {
	return process(dst, dst_end, src, src_end, flags, prefix_window());
};

//static bool _test_lzvn = test_codec(LZVN::encoder(), LZVN::decoder());

//-----------------------------------------------------------------------------
//	LZFSE - uses finite state entropy
//-----------------------------------------------------------------------------

inline bool LZFSE::bit_stream::input(int n, const uint8 *&pbuf) {
	if (n) {
		pbuf	-= 8;
		memcpy(&accum, pbuf, 8);
		accum_nbits = n + 64;
	} else {
		accum	= 0;
		pbuf	-= 7;
		memcpy(&accum, pbuf, 7);
		accum_nbits = n + 56;
	}
	return between(accum_nbits, 56, 63) && !(accum >> accum_nbits);
}
inline void LZFSE::bit_stream::flush_in(const uint8 *&pbuf) {
	int		nbits	= (63 - accum_nbits) & -8;
	pbuf	-= nbits >> 3;
	accum	= (accum << nbits) | (load_packed<uint64>(pbuf) & bits<uint64>(nbits));
	accum_nbits += nbits;
}
inline uint64 LZFSE::bit_stream::pull(int n) {
	accum_nbits -= n;
	uint64 result = accum >> accum_nbits;
	accum = accum & bits<uint64>(accum_nbits);
	return result;
}
inline void LZFSE::bit_stream::flush_out(uint8 *&pbuf) {
	int		nbits = accum_nbits & -8;
	memcpy(pbuf, &accum, 8);
	pbuf	+= nbits >> 3;
	accum	>>= nbits;
	accum_nbits -= nbits;
}
inline void LZFSE::bit_stream::finish(uint8 *&pbuf) {
	int		nbits = (accum_nbits + 7) & -8;
	memcpy(pbuf, &accum, 8);
	pbuf	+= nbits >> 3;
	accum	= 0;
	accum_nbits -= nbits;
}
inline void LZFSE::bit_stream::push(int n, uint64 b) {
	accum |= b << accum_nbits;
	accum_nbits += n;
}

//--------------------------------
// LZFSE compressed blocks
//--------------------------------
//The L, M, D data streams are all encoded as a "base" value, which is FSE-encoded, and an "extra bits" value, which is the difference between value and base and is simply represented as a raw bit value
//The following tables represent the number of low-order bits to encode separately and the base values for each of L, M, and D

static const uint8 l_extra_bits[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 3, 5, 8
};
static const int32 l_base_value[] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 20, 28, 60
};
static const uint8 m_extra_bits[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 5, 8, 11
};
static const int32 m_base_value[] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 24, 56, 312
};
static const uint8 d_extra_bits[] = {
	0,  0,  0,  0,  1,  1,  1,  1,  2,  2,  2,  2,  3,  3,  3,  3,
	4,  4,  4,  4,  5,  5,  5,  5,  6,  6,  6,  6,  7,  7,  7,  7,
	8,  8,  8,  8,  9,  9,  9,  9,  10, 10, 10, 10, 11, 11, 11, 11,
	12, 12, 12, 12, 13, 13, 13, 13, 14, 14, 14, 14, 15, 15, 15, 15
};
static const int32 d_base_value[] = {
	0,		1,		2,		3,		4,		6,		8,		10,		12,		16,		20,		24,		28,		36,		44,		52,
	60,		76,		92,		108,	124,	156,	188,	220,	252,	316,	380,	444,	508,	636,	764,	892,
	1020,	1276,	1532,	1788,	2044,	2556,	3068,	3580,	4092,	5116,	6140,	7164,	8188,	10236,	12284,	14332,
	16380,	20476,	24572,	28668,	32764,	40956,	49148,	57340,	65532,	81916,	98300,	114684,	131068,	163836,	196604,	229372
};

namespace iso {
template<typename T, size_t N> inline size_t sum(const T (&table)[N]) {
	size_t sum = 0;
	for (int i = 0; i < N; i++)
		sum += table[i];
	return sum;
}
}

struct LZFSE::state {
	uint32	n_literals;				// Number of literal bytes output by block (*not* the number of literals)
	uint32	n_matches;				// Number of matches in block (which is also the number of literals)
	uint32	literal_payload;		// Number of bytes used to encode literals
	uint32	lmd_payload;			// Number of bytes used to encode matches

// Final encoder states for the block, which will be the initial states for the decoder:
	int32	literal_bits;			// Final accum_nbits for literals stream
	uint16	literal_state[4];		// There are four interleaved streams of literals, so there are four final states
	int32	lmd_bits;				// accum_nbits for the l, m, d stream
	uint16	l_state;				// Final L (literal length) state
	uint16	m_state;				// Final M (match length) state
	uint16	d_state;				// Final D (match distance) state

	state() { clear(*this); }
	state(const struct compressed_state&);

	bool check() const {
		return n_literals		<= MATCHES_PER_BLOCK * 4
			&& n_matches		<= MATCHES_PER_BLOCK

			&& literal_state[0] <  LIT_STATES
			&& literal_state[1] <  LIT_STATES
			&& literal_state[2] <  LIT_STATES
			&& literal_state[3] <  LIT_STATES
								   
			&& l_state			<  L_STATES
			&& m_state			<  M_STATES
			&& d_state			<  D_STATES;
	}

};

struct LZFSE::frequencies {
	// Normalized frequency tables for each stream. Sum of values in each array is the number of states
	uint16	l_freq[L_SYMBOLS];
	uint16	m_freq[M_SYMBOLS];
	uint16	d_freq[D_SYMBOLS];
	uint16	literal_freq[LIT_SYMBOLS];

	bool check() const {
		return sum(l_freq)		<= L_STATES
			&& sum(m_freq)		<= M_STATES
			&& sum(d_freq)		<= D_STATES
			&& sum(literal_freq)<= LIT_STATES;
	}

	static inline int decode_freq(uint32 bits, int &nbits) {
		static const int8 nbits_table[32] = {
			2, 3, 2, 5, 2, 3, 2, 8, 2, 3, 2, 5, 2, 3, 2, 14,
			2, 3, 2, 5, 2, 3, 2, 8, 2, 3, 2, 5, 2, 3, 2, 14
		};
		static const int8 values[32] = {
			0, 2, 1, 4, 0, 3, 1, -1, 0, 2, 1, 5, 0, 3, 1, -1,
			0, 2, 1, 6, 0, 3, 1, -1, 0, 2, 1, 7, 0, 3, 1, -1
		};

		uint32	b = bits & 31; // lower 5 bits
		int		n = nbits_table[b];
		nbits	= n;

		return	n == 8	? 8  + ((bits >> 4) & 0xf)
			:	n == 14	? 24 + ((bits >> 4) & 0x3ff)
			:	values[b];	// <= 5 bits encoding from table
	}

	static inline uint32 encode_freq(int value, int &nbits) {
		switch (value) {
			case 0:	nbits = 2; return 0;	//    0.0
			case 1:	nbits = 2; return 2;	//    1.0
			case 2:	nbits = 3; return 1;	//   0.01
			case 3:	nbits = 3; return 5;	//   1.01
			case 4:	nbits = 5; return 3;	// 00.011
			case 5:	nbits = 5; return 11;	// 01.011
			case 6:	nbits = 5; return 19;	// 10.011
			case 7:	nbits = 5; return 27;	// 11.011
			default:
				if (value < 24) {
					nbits = 8;						// 4+4
					return ((value - 8) << 4) + 7;	// xxxx.0111
				} else {
					nbits = 14;						// 4+10
					return ((value - 24) << 4) + 15;// xxxxxxxxxx.1111
				}
		}
	}

	uint8 *encode(uint8 *dst) const {
		uint32	accum		= 0;
		int		accum_nbits	= 0;
		const uint16 *src	= l_freq;

		for (int i = 0, n = int(num_elements(l_freq) + num_elements(m_freq) + num_elements(d_freq) + num_elements(literal_freq)); i < n; i++) {
			// Encode one value to accum
			int		nbits	= 0;
			uint32	bits	= encode_freq(src[i], nbits);
			accum		|= bits << accum_nbits;
			accum_nbits	+= nbits;

			// Store bytes from accum to output buffer
			while (accum_nbits >= 8) {
				*dst = (uint8)(accum & 0xff);
				accum		>>= 8;
				accum_nbits	-= 8;
				dst++;
			}
		}
		// Store final byte if needed
		if (accum_nbits > 0) {
			*dst = (uint8)(accum & 0xff);
			dst++;
		}
		return dst;
	}

	bool decode(const uint8 *src, const uint8 *src_end) {
		uint32		accum		= 0;
		int			accum_nbits = 0;

		if (src_end > src) {
			for (int i = 0, n = int(num_elements(l_freq) + num_elements(m_freq) + num_elements(d_freq) + num_elements(literal_freq)); i < n; i++) {
				// Refill accum, one byte at a time, until we reach end of header, or accum is full
				while (src < src_end && accum_nbits + 8 <= 32) {
					accum |= uint32(*src++) << accum_nbits;
					accum_nbits += 8;
				}

				// Decode and store value
				int		nbits = 0;
				l_freq[i] = decode_freq(accum, nbits);

				if (nbits > accum_nbits)
					return false; // failed

				// Consume nbits bits
				accum		>>= nbits;
				accum_nbits -= nbits;
			}
		}
		return accum_nbits < 8 && src == src_end;// we need to end up exactly at the end of header, with less than 8 bits in accumulator
	}
};


struct LZFSE::compressed_state {
	// Literal state
	uint64	n_literals:20, literal_payload:20, n_matches:20, literal_bits:3;										//v0
	uint64	literal_state0:10, literal_state1:10, literal_state2:10, literal_state3:10,	lmd_payload:20, lmd_bits:3;	//v1
	uint64	freq_payload:32, l_state:10, m_state:10, d_state:10;													//v2

	compressed_state(const state &p, uint32 freq_payload) :
		n_literals(p.n_literals), literal_payload(p.literal_payload), n_matches(p.n_matches), literal_bits(p.literal_bits + 7),
		literal_state0(p.literal_state[0]), literal_state1(p.literal_state[1]), literal_state2(p.literal_state[2]), literal_state3(p.literal_state[3]),
		lmd_payload(p.lmd_payload), lmd_bits(p.lmd_bits + 7),
		freq_payload(freq_payload), l_state(p.l_state), m_state(p.m_state), d_state(p.d_state)
	{}
};

LZFSE::state::state(const compressed_state &p) :
	n_literals(p.n_literals), n_matches(p.n_matches), literal_payload(p.literal_payload),
	lmd_payload(p.lmd_payload),
	literal_bits(p.literal_bits - 7),
	lmd_bits(p.lmd_bits - 7),
	l_state(p.l_state), m_state(p.m_state), d_state(p.d_state)
{
	literal_state[0] = p.literal_state0;
	literal_state[1] = p.literal_state1;
	literal_state[2] = p.literal_state2;
	literal_state[3] = p.literal_state3;
}

void LZFSE::_decoder::init_decoder(lit_entry *__restrict t, int nstates, const uint16 *__restrict freq, int nsymbols) {
	int n_clz	= leading_zeros(nstates);
	for (int i = 0; i < nsymbols; i++) {
		if (int f = freq[i]) {
			int k	= leading_zeros(f) - n_clz; // shift needed to ensure N <= (F<<K) < 2*N
			int j0	= (2 * nstates) >> k;

			LZFSE::_decoder::lit_entry e;
			e.symbol = (uint8)i;

			// Initialize all states S reached by this symbol: OFFSET <= S < OFFSET + F
			e.k		= (int8)k;
			for (int j = f; j < j0; j++) {
				e.delta	= int16((j << k) - nstates);
				*t++ = e;
			}
			e.k		= (int8)(k - 1);
			for (int j = 0; j < f + f - j0; j++) {
				e.delta	= int16(j << (k - 1));
				*t++ = e;
			}
		}
	}
}

inline uint8 LZFSE::_decoder::decode(bit_stream &in, uint16 &state, const lit_entry *decoder) {
	auto	e	= decoder[state];
	state		= e.delta + (uint16)in.pull(e.k);
	return e.symbol;
}

void LZFSE::_decoder::init_decoder(value_entry *__restrict t, int nstates, const uint8 *__restrict bits, const int32 *__restrict base, int nsymbols, const uint16 *__restrict freq) {
	int n_clz = leading_zeros(nstates);
	for (int i = 0; i < nsymbols; i++) {
		if (int f = freq[i]) {
			int k	= leading_zeros(f) - n_clz; // shift needed to ensure N <= (F<<K) < 2*N
			int j0	= (2 * nstates) >> k;

			LZFSE::_decoder::value_entry e;
			e.value_bits	= bits[i];
			e.vbase			= base[i];

			// Initialize all states S reached by this symbol: OFFSET <= S < OFFSET + F
			e.total_bits	= (uint8)k + e.value_bits;
			for (int j = f; j < j0; j++) {
				e.delta		= int16((j << k) - nstates);
				*t++ = e;
			}
			e.total_bits	= (uint8)(k - 1) + e.value_bits;
			for (int j = 0; j < f + f - j0; j++) {
				e.delta		= int16(j << (k - 1));
				*t++ = e;
			}
		}
	}
}

inline int32 LZFSE::_decoder::decode(bit_stream &in, uint16 &state, const value_entry *decoder) {
	auto	e	= decoder[state];
	uint32	t	= (uint32)in.pull(e.total_bits);
	state		= uint16(e.delta + (t >> e.value_bits));
	return int32(e.vbase + (t & bits<uint64>(e.value_bits)));
}


inline void LZFSE::_decoder::save(int32 _L, int32 _M, int32 _D, const uint8 *_lit, uint32 _n_matches, const uint8 *_src) {
	L			= _L;
	M			= _M;
	D			= _D;
	n_matches	= _n_matches;
	lit			= _lit;
	src			= _src;
}

bool LZFSE::_decoder::init(const LZFSE::state &p, const LZFSE::frequencies &f, const uint8 *lits_end) {
	init_decoder(lit_decoder, num_elements32(lit_decoder), f.literal_freq, num_elements32(f.literal_freq));
	init_decoder(l_decoder, num_elements32(l_decoder), l_extra_bits, l_base_value, num_elements32(l_base_value), f.l_freq);
	init_decoder(m_decoder, num_elements32(m_decoder), m_extra_bits, m_base_value, num_elements32(m_base_value), f.m_freq);
	init_decoder(d_decoder, num_elements32(d_decoder), d_extra_bits, d_base_value, num_elements32(d_base_value), f.d_freq);

	// Decode literals
	const uint8 *buf = lits_end; // read bits backwards from the end
	if (!in.input(p.literal_bits, buf))
		return false;

	uint16 state0 = p.literal_state[0];
	uint16 state1 = p.literal_state[1];
	uint16 state2 = p.literal_state[2];
	uint16 state3 = p.literal_state[3];

	for (uint32 i = 0; i < p.n_literals; i += 4) { // n_literals is multiple of 4
		in.flush_in(buf);
		literals[i + 0] = decode(in, state0, lit_decoder); // 10b max
		literals[i + 1] = decode(in, state1, lit_decoder); // 10b max
		literals[i + 2] = decode(in, state2, lit_decoder); // 10b max
		literals[i + 3] = decode(in, state3, lit_decoder); // 10b max
	}

	l_state	= p.l_state;
	m_state	= p.m_state;
	d_state	= p.d_state;

	// read bits backwards from the end
	buf = lits_end + p.lmd_payload;
	if (!in.input(p.lmd_bits, buf))
		return false;

	save(0, 0, -1, literals, p.n_matches, buf);
	return true;
}

size_t LZFSE::_decoder::process(void *dst0, size_t dst_size) {
	int32			L		= this->L;
	int32			M		= this->M;
	int32			D		= this->D;
	const uint8		*lit	= this->lit;
	uint32			n_matches = this->n_matches;

	const uint8		*src	= this->src;
	uint8			*dst	= (uint8*)dst0, *dst_end = dst + dst_size;

	// If L or M is non-zero, that means that we have already started decoding this block, and that we needed to interrupt decoding to get more space from the caller
	// There's a pending L, M, D triplet that we weren't able to completely process, so jump ahead to finish executing that symbol before decoding new values
	if (L || M)
		goto ExecuteMatch;

	while (n_matches > 0) {
		// Decode the next L, M, D symbol from the input stream
		in.flush_in(src);

		L = decode(in, l_state, l_decoder);
		if (lit + L >= end(literals))
			break;//return OK;

		M	= decode(in, m_state, m_decoder);
		if (int32 d = decode(in, d_state, d_decoder))
			D = d;

		n_matches--;

	ExecuteMatch:
		// Error if D is out of range, so that we avoid passing through uninitialized data or accesssing memory out of the destination buffer
		//if ((uint8*)dst0 + D > dst + L)
		//	return 0;

		if (dst + L + M + 8 < dst_end) {
			// If we have plenty of space remaining, we're not worried about writing off the end of the buffer
			loose_copy<uint64>(dst, lit, dst + L);
			dst		+= L;
			lit		+= L;

			if (D >= 8 || D >= M)
				loose_copy<uint64>(dst, dst - D, dst + M);
			else
				loose_copy<uint8>(dst, dst - D, dst + M);

		} else {
			// Process the literal
			if (dst + L > dst_end) {
				ptrdiff_t	n = dst_end - dst;
				memcpy(dst, lit, n);
				save(L - n, M, D, lit + n, n_matches, src);
				return dst_size;
			}

			memcpy(dst, lit, L);
			dst		+= L;
			lit		+= L;

			// Process the match
			if (dst + M > dst_end) {
				loose_copy<uint8>(dst, dst - D, dst_end);
				save(0, M - (dst_end - dst), D, lit, n_matches, src);
				return dst_size;
			}

			loose_copy<uint8>(dst, dst - D, dst + M);
		}
		dst		+= M;
	}
	save(0, 0, D, lit, n_matches, src);
	return dst - (uint8*)dst0;
}
//--------------------------------
// headers in compressed stream
//--------------------------------

struct block_header {
	enum {
		ENDOFSTREAM		= 0x24787662, // bvx$	(end of stream)
		UNCOMPRESSED	= 0x2d787662, // bvx-	(raw data)
		COMPRESSEDV1	= 0x31787662, // bvx1	(lzfse compressed, uncompressed tables)
		COMPRESSEDV2	= 0x32787662, // bvx2	(lzfse compressed, compressed tables)
		COMPRESSEDLZVN	= 0x6e787662, // bvxn	(lzvn compressed)
	};
	uint32	magic;
	uint32	uncomp_size;
	block_header(uint32 magic, uint32 uncomp_size) : magic(magic), uncomp_size(uncomp_size) {}
};

struct uncompressed_block : block_header {
	enum {MAGIC = UNCOMPRESSED};
	uncompressed_block(uint32 uncomp_size) : block_header(MAGIC, uncomp_size) {}
};

struct lzvn_block : block_header {
	enum {MAGIC = COMPRESSEDLZVN};
	uint32			comp_size;		// Number of encoded (source) bytes
	lzvn_block(uint32 uncomp_size, uint32 comp_size) : block_header(MAGIC, uncomp_size), comp_size(comp_size) {}
};

struct lzfse_block_v1 : block_header {
	enum {MAGIC = COMPRESSEDV1};
	uint32			comp_size;		// Number of encoded (source) bytes in block
	LZFSE::state	params;
	LZFSE::frequencies		freq;
	lzfse_block_v1(uint32 uncomp_size, const LZFSE::state &p, const LZFSE::frequencies &f, uint32 comp_size) : block_header(MAGIC, uncomp_size), comp_size(comp_size), params(p), freq(f) {}
};

struct lzfse_block_v2 : block_header {
	enum {MAGIC = COMPRESSEDV2};
	LZFSE::compressed_state	params;
	lzfse_block_v2(uint32 uncomp_size, const LZFSE::state &p, uint32 freq_payload) : block_header(MAGIC, uncomp_size), params(p, freq_payload) {}
};

//--------------------------------
// LZFSE::encode
//--------------------------------

static const uint8 l_base_from_value[] = {
	0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 16, 16, 16, 17, 17, 17, 17, 17, 17, 17, 17, 18, 18, 18, 18,
	18, 18, 18, 18,	18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 19, 19, 19, 19,
	19, 19, 19, 19, 19, 19, 19, 19,	19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19,
	19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19,	19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19,
	19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19,	19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19,
	19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19,	19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19,
	19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19,	19, 19, 19, 19, 19, 19, 19, 19,
	19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19,	19, 19, 19, 19,
	19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19,
	19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19
};
static inline uint8 m_base_from_value(int32 value) {
	static const uint8 sym[] = {
		0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 16,	16, 16, 16, 16, 16, 16, 17, 17, 17, 17, 17, 17, 17, 17,
		17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 18, 18, 18, 18, 18, 18, 18, 18,
		18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18,
		18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18,
		18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18,
		18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18,
		18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18,
		18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18,
		18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18,
		18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18
	};
	return value < 312 ? sym[value] : 19;
}

static inline uint8 d_base_from_value(int32 value) {
	static const uint8 sym[] = {
		0,  1,  2,  3,  4,  4,  5,  5,  6,  6,  7,  7,  8,  8,  8,  8,  9,  9,  9,  9,  10, 10, 10, 10, 11, 11, 11, 11, 12, 12, 12, 12,
		12, 12, 12, 12, 13, 13, 13, 13, 13, 13, 13, 13, 14, 14, 14, 14, 14, 14, 14, 14, 15, 15, 15, 15, 15, 15, 15, 15, 16, 16, 16, 16,
		16, 17, 18, 19, 20, 20, 21, 21, 22, 22, 23, 23, 24, 24, 24, 24, 25, 25, 25, 25, 26, 26, 26, 26, 27, 27, 27, 27, 28, 28, 28, 28,
		28, 28, 28, 28, 29, 29, 29, 29, 29, 29, 29, 29, 30, 30, 30, 30, 30, 30, 30, 30, 31, 31, 31, 31, 31, 31, 31, 31, 32, 32, 32, 32,
		32, 33, 34, 35, 36, 36, 37, 37, 38, 38, 39, 39, 40, 40, 40, 40, 41, 41, 41, 41, 42, 42, 42, 42, 43, 43, 43, 43, 44, 44, 44, 44,
		44, 44, 44, 44, 45, 45, 45, 45, 45, 45, 45, 45, 46, 46, 46, 46, 46, 46, 46, 46, 47, 47, 47, 47, 47, 47, 47, 47, 48, 48, 48, 48,
		48, 49, 50, 51, 52, 52, 53, 53, 54, 54, 55, 55, 56, 56, 56, 56, 57, 57, 57, 57, 58, 58, 58, 58, 59, 59, 59, 59, 60, 60, 60, 60,
		60, 60, 60, 60, 61, 61, 61, 61, 61, 61, 61, 61, 62, 62, 62, 62, 62, 62, 62, 62, 63, 63, 63, 63, 63, 63, 63, 63, 0,  0,  0,  0
	};
	return sym[	value < 60		? ((value - 0) >> 0) + 0
			:	value < 1020	? ((value - 60) >> 4) + 64
			:	value < 16380	? ((value - 1020) >> 8) + 128
			:	min(((value - 16380) >> 12) + 192, 255)
			];
}

// Normalize a table T[NSYMBOLS] of occurrences to FREQ[NSYMBOLS]
void normalize_freq(int nstates, int nsymbols, const uint32 *__restrict t, uint16 *__restrict freq) {
	// Compute the total number of symbol occurrences
	uint32	count			= 0;
	for (int i = 0; i < nsymbols; i++)
		count += t[i];

	uint32	highprec_step	= count == 0 ? 0 : ((uint32)1 << 31) / count;
	int		shift			= leading_zeros(nstates) - 1;
	int		max_freq		= 0;
	int		max_freq_sym	= 0;

	count = 0;

	for (int i = 0; i < nsymbols; i++) {
		// Rescale the occurrence count to get the normalized frequency
		int f = (((t[i] * highprec_step) >> shift) + 1) >> 1;

		// If a symbol was used, it must be given a nonzero normalized frequency
		if (f == 0 && t[i] != 0)
			f = 1;

		freq[i]		= f;
		count		+= f;

		// Remember the maximum frequency and which symbol had it
		if (f > max_freq) {
			max_freq		= f;
			max_freq_sym	= i;
		}
	}

	int overrun = int(count) - nstates;

	// If there remain states to be assigned, or there's a small overrun, then just assign (or remove) them to the most frequent symbol
	if (abs(overrun) < max_freq / 4) {
		freq[max_freq_sym] -= overrun;

	} else {
		// Remove states from symbols until the correct number of states is used
		for (int shift = 3; overrun; shift--) {
			for (int sym = 0; sym < nsymbols; sym++) {
				if (freq[sym] > 1) {
					int		n	= min((freq[sym] - 1) >> shift, overrun);
					freq[sym]	-= n;
					overrun		-= n;
					if (overrun == 0)
						break;
				}
			}
		}
	}
}

struct encoder_entry {
	int16	s0;			// First state requiring a K-bit shift
	int16	k;			// States S >= S0 are shifted K bits. States S < S0 are shifted K-1 bits
	int16	delta0;		// Relative increment used to compute next state if S >= S0
	int16	delta1;		// Relative increment used to compute next state if S < S0
	void encode(uint16 &state, LZFSE::bit_stream& out) const {
		bool	hi		= state >= s0;
		int		nbits	= hi ? k : (k - 1);
		out.push(nbits, state & bits<uint64>(nbits));		// Write lower NBITS of state
		state = (hi ? delta0 : delta1) + (state >> nbits);	// Update state with remaining bits and delta
	}
};

template<int N> struct encoder_table {
	encoder_entry	t[N];
	encoder_table(int nstates, const uint16 *__restrict freq) {
		int		offset	= 0; // current offset
		int		n_clz	= leading_zeros(nstates);
		for (auto &i : t) {
			if (int f = *freq++) {
				int k		= leading_zeros(f) - n_clz; // shift needed to ensure N <= (F<<K) < 2*N
				i.s0		= int16((f << k) - nstates);
				i.k			= int16(k);
				i.delta0	= int16(offset - f + (nstates >> k));
				i.delta1	= int16(offset - f + (nstates >> (k - 1)));
				offset		+= f;
			}
		}
	}
	auto&	operator[](size_t i) const { return t[i]; }
};

LZFSE::frequencies LZFSE::_encoder::get_frequencies() {
	frequencies	f;
	// Occurrence tables for all 4 streams (L,M,D,literals)
	uint32	l_occ[L_SYMBOLS]		= {0};
	uint32	m_occ[M_SYMBOLS]		= {0};
	uint32	d_occ[D_SYMBOLS]		= {0};
	uint32	lit_occ[LIT_SYMBOLS]	= {0};

	uint32 uncomp_size = 0;

	for (uint32 i = 0; i < n_matches; i++) {
		uint32 l = l_values[i];
		uint32 m = m_values[i];
		uint32 d = d_values[i];
		uncomp_size += l + m;
		l_occ[l_base_from_value[l]]++;
		m_occ[m_base_from_value(m)]++;
		d_occ[d_base_from_value(d)]++;
	}

	for (uint32 i = 0; i < n_literals; i++)
		lit_occ[literals[i]]++;

	// Normalize occurrence tables to freq tables
	normalize_freq(L_STATES,	L_SYMBOLS,		l_occ,		f.l_freq);
	normalize_freq(M_STATES,	M_SYMBOLS,		m_occ,		f.m_freq);
	normalize_freq(D_STATES,	D_SYMBOLS,		d_occ,		f.d_freq);
	normalize_freq(LIT_STATES,	LIT_SYMBOLS,	lit_occ,	f.literal_freq);

	return f;
}

uint8* LZFSE::_encoder::write_state(state& p, const frequencies& f, uint8* dst, uint8* dst_end) {
	// Encode literals
	{
		encoder_table<LIT_SYMBOLS>	lit_encoder(LIT_STATES, f.literal_freq);
		LZFSE::bit_stream out;
		uint8	*dst0	= dst;

		// We encode starting from the last literal so we can decode starting from the first
		for (uint32 i = n_literals; i > 0; i -= 4) {
			if (dst + 16 > dst_end)
				return nullptr;
			lit_encoder[literals[i - 1]].encode(p.literal_state[3], out); // 10b
			lit_encoder[literals[i - 2]].encode(p.literal_state[2], out); // 10b
			lit_encoder[literals[i - 3]].encode(p.literal_state[1], out); // 10b
			lit_encoder[literals[i - 4]].encode(p.literal_state[0], out); // 10b
			out.flush_out(dst);
		}
		out.finish(dst);

		// Update header with final encoder state
		p.literal_bits		= out.accum_nbits; // [-7, 0]
		p.literal_payload	= uint32(dst - dst0);
	}

	// Encode L,M,D
	{
		encoder_table<L_SYMBOLS>	l_encoder(L_STATES, f.l_freq);
		encoder_table<M_SYMBOLS>	m_encoder(M_STATES, f.m_freq);
		encoder_table<D_SYMBOLS>	d_encoder(D_STATES, f.d_freq);

		LZFSE::bit_stream out;
		uint8 *dst0		= dst;

		// Add 8 padding bytes to the L,M,D payload
		if (dst + 8 > dst_end)
			return nullptr;
		
		store_packed<uint64>(dst, 0);
		dst += 8;

		// We encode starting from the last match so we can decode starting from the first
		for (uint32 i = n_matches; i--;) {
			if (dst + 16 > dst_end)
				return nullptr;

			// D requires 23b max
			int32	d_value		= d_values[i];
			uint8	d_symbol	= d_base_from_value(d_value);
			out.push(d_extra_bits[d_symbol], d_value - d_base_value[d_symbol]);
			d_encoder[d_symbol].encode(p.d_state, out);

			// M requires 17b max
			int32	m_value		= m_values[i];
			uint8	m_symbol	= m_base_from_value(m_value);
			out.push(m_extra_bits[m_symbol], m_value - m_base_value[m_symbol]);
			m_encoder[m_symbol].encode(p.m_state, out);

			// L requires 14b max
			int32	l_value		= l_values[i];
			uint8	l_symbol	= l_base_from_value[l_value];
			out.push(l_extra_bits[l_symbol], l_value - l_base_value[l_symbol]);
			l_encoder[l_symbol].encode(p.l_state, out);
			out.flush_out(dst);
		}
		out.finish(dst);

		// Update header with final encoder state
		p.lmd_bits		= out.accum_nbits; // [-7, 0]
		p.lmd_payload	= uint32(dst - dst0);
	}

	return dst;
}


// Encode previous distance
static void encode_dprev(uint32 *d_values, uint32 n_matches) {
	uint32 d_prev = 0;
	for (uint32 i = 0; i < n_matches; i++) {
		uint32 d = d_values[i];
		if (d == d_prev)
			d_values[i] = 0;
		else
			d_prev = d;
	}
}

// Revert the d_prev encoding
static void revert_dprev(uint32 *d_values, uint32 n_matches) {
	uint32 d_prev = 0;
	for (uint32 i = 0; i < n_matches; i++) {
		uint32 d = d_values[i];
		if (d == 0)
			d_values[i] = d_prev;
		else
			d_prev = d;
	}
}

#define USE_V2_BLOCK

uint8* LZFSE::_encoder::encode_matches(uint8 *dst, uint8 *dst_end, uint64 src_size) {
	if (n_literals == 0 && n_matches == 0)
		return dst; // nothing to store, OK

#ifdef USE_V2_BLOCK
	// Make sure we have enough room for a _full_ V2 header
	if (dst + sizeof(lzfse_block_v2) + sizeof(frequencies) > dst_end)
		return nullptr;
#else
	// Make sure we have enough room for a V1 header
	if (dst + sizeof(lzfse_block_v1) > dst_end)
		return nullptr;
#endif

	state	params;
	params.n_matches	= n_matches;
	params.n_literals	= n_literals;

	// Add 0x00 literals until n_literals multiple of 4, since we encode 4 interleaved literal streams
	while (params.n_literals & 3)
		literals[params.n_literals++] = 0;

	// Encode previous distance
	encode_dprev(d_values, params.n_matches);

	frequencies	freq = get_frequencies();

	uint8	*block	= dst;
#ifdef USE_V2_BLOCK
	dst		= freq.encode(dst + sizeof(lzfse_block_v2));
	uint32	freq_payload = dst - block;
#endif

	if (dst = write_state(params, freq, dst, dst_end)) {
#ifdef USE_V2_BLOCK
		store_packed(block, lzfse_block_v2(src_size, params, freq_payload));
#else
		store_packed(block, lzfse_block_v1(src_size, params, freq, params.literal_payload + params.lmd_payload));
#endif
		// state update
		n_literals	= 0;
		n_matches	= 0;
		return dst;
	}

	revert_dprev(d_values, params.n_matches);
	return nullptr;
}

// Push a L,M,D match into the STATE
inline bool LZFSE::_encoder::push_lmd(uint32 L, uint32 M, uint32 D) {
	// Check if we have enough space to push the match (we add some margin to copy literals faster here, and round final count later)
	if (n_matches + 1 + 8 > MATCHES_PER_BLOCK || n_literals + L + 16 > MATCHES_PER_BLOCK * 4)
		return false; // state full

	// Store match
	uint32 n = n_matches++;
	l_values[n] = L;
	m_values[n] = M;
	d_values[n] = D;

	// Store literals
	if (src_literal + L + 16 > src_end)
		memcpy(literals + n_literals, src + src_literal, L);
	else
		loose_copy<uint64>(literals + n_literals, src + src_literal, literals + n_literals + L);

	n_literals	+= L;
	src_literal	+= L + M;
	return true;
}

// Split MATCH into one or more L,M,D parts, and push to STATE
bool LZFSE::_encoder::try_push_match(const match &match) {
	// Save the initial state
	uint32	n_matches0		= n_matches;
	uint32	n_literals0		= n_literals;
	auto	src_literals0	= src_literal;

	// L,M,D
	uint32	L	= uint32(match.pos - src_literal);
	uint32	M	= match.length;
	uint32	D	= uint32(match.pos - match.ref);
	bool	ok	= true;

	// Split L if too large
	while (ok && L > L_MAX) {
		ok	= push_lmd(L_MAX, 0, 1);
		L	-= L_MAX;
	}

	// Split if M too large
	while (ok && M > M_MAX) {
		ok	= push_lmd(L, M_MAX, D);
		L	= 0;
		M	-= M_MAX;
	}

	// L,M in range
	if (ok && (L || M))
		ok = push_lmd(L, M, D);

	if (!ok) {
		// Revert state
		n_matches	= n_matches0;
		n_literals	= n_literals0;
		src_literal	= src_literals0;
	}

	return ok;
}

uint8 *LZFSE::_encoder::push_match(const match &match, uint8 *dst, uint8 *dst_end, uint64 src_size) {
	if (!try_push_match(match)) {
		if (dst = encode_matches(dst, dst_end, src_size))
			try_push_match(match);
	}
	return dst;
}

uint8 *LZFSE::_encoder::process(uint8 *dst, uint8 *dst_end) {
	uint8		*dst0	= dst;

	// 8 byte padding at end of buffer
	for (int64 src_current_end = src_end - 8; src_current < src_current_end; ++src_current, dst0 = dst) {
		int64		pos		= src_current;
		uint32		x		= load_packed<uint32>(src + pos);
		hash_entry&	h		= hash_table[hash(x)];

		// Do not look for a match if we are still covered by a previous match
		if (pos >= src_literal) {
			// Search best incoming match
			match incoming = {pos, 0, 0};

			// Check for matches (consider matches of length >= 4 only)
			for (int k = 0; k < HASH_WIDTH; k++) {
				if (h.value[k] == x) {
					int32 ref = h.pos[k];
					if (pos <= ref + D_MAX) {
						const uint8 *src_ref	= src + ref, *src_pos = src + pos;
						uint32		 length		= 4;
						uint32		 maxLength	= uint32(src_end - pos - 8);	 // ensure we don't hit the end of SRC
						while (length < maxLength) {
							uint64 d = load_packed<uint64>(src_ref + length) ^ load_packed<uint64>(src_pos + length);
							if (d == 0) {
								length += 8;
							} else {
								length += num_zero_bytes(d);
								break;
							}
						}

						// keep if longer
						if (length > incoming.length) {
							incoming.length = length;
							incoming.ref	= ref;
						}
					}
				}
			}
			
			// No incoming match?
			if (incoming.length == 0) {
				// We may still want to emit some literals here, to not lag too far behind the current search point, and avoid ending up with a literal block not fitting in the state
				// The threshold here should be larger than a couple of L_MAX, and much smaller than LITERALS_PER_BLOCK
				if ((pos - src_literal) > 8 * L_MAX) {
					// Here, we need to consume some literals; emit pending match if there is one
					if (pending.length) {
						if (!(dst = push_match(pending, dst, dst_end, pos)))
							break;
						clear(pending);
					} else {
						// No pending match, emit a full L_MAX block of literals
						if (!(dst = push_match(make_literals(L_MAX), dst, dst_end, pos)))
							break;
					}
				}
			} else {
				// Limit match length (it may still be expanded backwards, but this is bounded by the limit on literals we tested before)
				if (incoming.length > MAX_MATCH_LENGTH)
					incoming.length = MAX_MATCH_LENGTH;

				// Expand backwards (since this is expensive, we do this for the best match only)
				while (incoming.pos > src_literal && incoming.ref > 0 && src[incoming.ref - 1] == src[incoming.pos - 1]) {
					--incoming.pos;
					--incoming.ref;
				}
				
				// update length after expansion
				incoming.length += pos - incoming.pos;

				if (incoming.length >= GOOD_MATCH) {
					// Incoming is 'good', emit incoming
					if (!(dst = push_match(incoming, dst, dst_end, pos)))
						break;
					clear(pending);

				} else if (pending.length == 0) {
					// No pending, keep incoming
					pending = incoming;

				} else if (pending.pos + pending.length <= incoming.pos) {
					// No overlap, emit pending, keep incoming
					if (!(dst = push_match(pending, dst, dst_end, pos)))
						break;
					pending = incoming;

				} else {
					// Overlap, emit longest
					if (!(dst = push_match(incoming.length > pending.length ? incoming : pending, dst, dst_end, pos)))
						break;
					clear(pending);
				}
			}
		}

		// Update state now (pending has already been updated)
		for (int k = HASH_WIDTH - 1; k--;) {
			h.pos[k + 1]	= h.pos[k];
			h.value[k + 1]	= h.value[k];
		}
		h.pos[0]	= (int32)pos;
		h.value[0]	= x;
	}

	return dst0;
}

uint8* LZFSE::_encoder::finish(uint8 *dst, uint8 *dst_end, uint64 src_size) {
	// Emit pending match
	if (pending.length) {
		if (dst = push_match(pending, dst, dst_end, src_size))
			clear(pending);
	}

	// Emit final literals if any
	if (dst && src_end > src_literal)
		dst = push_match(make_literals(src_end - src_literal), dst, dst_end, src_size);

	// Emit all matches
	if (dst)
		dst = encode_matches(dst, dst_end, src_size);
	return dst;
}

void LZFSE::_encoder::slide(int64 delta, uint64 end) {
	src_end			= end;
	if (delta) {
		src				+= delta;
		src_current		-= delta;
		src_literal		-= delta;

		// Pending match
		pending.pos		-= delta;
		pending.ref		-= delta;

		// table positions, translated, and clamped to invalid pos
		int32 invalidPos = -4 * D_MAX;
		for (auto &i : hash_table) {
			for (auto &j : i.pos)
				j = (int32)max(j - delta, invalidPos);
		}
	}
}

LZFSE::_encoder::_encoder(const void *src, size_t src_size) : src((const uint8*)src), src_current(0), src_end(src_size), src_literal(0), n_matches(0), n_literals(0) {
	hash_entry line;
	for (int i = 0; i < HASH_WIDTH; i++) {
		line.pos[i]		= -4 * D_MAX; // invalid pos
		line.value[i]	= 0;
	}
	for (auto &i : hash_table)
		i = line;
	clear(pending);
}

//-----------------------------------------------------------------------------
//	decoder - blocks of LZVN, LZFSE, or uncompressed
//-----------------------------------------------------------------------------

//--------------------------------
// LZFSE::decoder
//--------------------------------

const uint8*	LZFSE::decoder::process(uint8 *&dst, uint8 *dst_end, const uint8 *src, const uint8 *src_end, TRANSCODE_FLAGS flags) {
	for (;;) {
		switch (block_magic) {
			case 0: {
				// We need at least 4 bytes of magic number to identify next block
				if (src + 4 > src_end)
					break;

				switch (uint32 magic = load_packed<uint32>(src)) {
					case block_header::ENDOFSTREAM:
						src				+= 4;
						end_of_stream	= true;
						break;//return OK; // done

					case block_header::UNCOMPRESSED: {
						if (src + sizeof(uncompressed_block) > src_end)
							break;//return SRC_EMPTY;

						// Setup state for uncompressed block
						block_src_size	= block_dst_size = load_packed<uncompressed_block>(src).uncomp_size;
						src				+= sizeof(uncompressed_block);
						block_magic		= magic;
						continue;
					}

					case block_header::COMPRESSEDLZVN: {
						if (src + sizeof(lzvn_block) > src_end)
							break;//return SRC_EMPTY;

						// Setup state for compressed LZVN block
						auto	block	= load_packed<lzvn_block>(src);
						block_src_size	= block.comp_size;
						block_dst_size	= block.uncomp_size;
						lzvn.init();
						src				+= sizeof(lzvn_block);
						block_magic		= magic;
						continue;
					}

					case block_header::COMPRESSEDV1: {
						if (src + sizeof(lzfse_block_v1) > src_end)
							break;//return SRC_EMPTY;

						auto	v1	= load_packed<lzfse_block_v1>(src);

						// We require the header + entire encoded block to be present in SRC during the entire block decoding
						if (src + sizeof(lzfse_block_v1) + v1.params.literal_payload + v1.params.lmd_payload > src_end)
							break;//return SRC_EMPTY;

						// Skip header && literal payload
						const uint8	*lits_end = src + sizeof(lzfse_block_v1) + v1.params.literal_payload;
						if (!lzfse.init(v1.params, v1.freq, lits_end))
							return nullptr;

						block_magic		= magic;
						block_dst_size	= v1.uncomp_size;
						src				= lits_end + v1.params.lmd_payload;	// after this block
						continue;

					}
					case block_header::COMPRESSEDV2: {
						if (src + sizeof(lzfse_block_v2) > src_end)
							break;//return SRC_EMPTY;

						auto v2 = load_packed<lzfse_block_v2>(src);

						// We require the header + entire encoded block to be present in SRC during the entire block decoding
						if (src + v2.params.freq_payload + v2.params.literal_payload + v2.params.lmd_payload > src_end)
							break;//return SRC_EMPTY;

						state	p(v2.params);
						frequencies		f;
						if (!f.decode(src + sizeof(lzfse_block_v2), src + v2.params.freq_payload))
							return nullptr;

						// Skip header && literal payload
						const uint8	*lits_end = src + v2.params.freq_payload + p.literal_payload;
						if (!lzfse.init(p, f, lits_end))
							return nullptr;

						block_magic		= magic;
						block_dst_size	= v2.uncomp_size;
						src				= lits_end + p.lmd_payload;	// after this block
						continue;
					}

					default:
						return nullptr;
				}
				break;
			}

			case block_header::UNCOMPRESSED: {
				auto	block_end = dst + block_dst_size;
				src = uncompressed.process(dst, min(block_end, dst_end), src, src_end, flags);
				if (block_dst_size = block_end - dst)
					break;

				block_magic = 0;
				continue;
			}

			case block_header::COMPRESSEDLZVN: {
				auto	block_end = dst + block_dst_size;
				src = lzvn.process(dst, min(block_end, dst_end), src, src_end, flags);
				if ((block_dst_size = block_end - dst) || !lzvn.end_of_stream)
					break;

				block_magic = 0;
				continue;
			}

			case block_header::COMPRESSEDV1:
			case block_header::COMPRESSEDV2: {
				size_t	written = lzfse.process(dst, min(block_dst_size, dst_end - dst));
				dst		+= written;
				if (block_dst_size -= (uint32)written)
					break;

				block_magic	= 0;
				continue;
			}

			default:
				return nullptr;
		}
		return src;
	}
}

//--------------------------------
// LZFSE::encoder
//--------------------------------

const uint8* LZFSE::encoder::finish(uint8 *&dst, uint8 *dst_end, const uint8 *src, TRANSCODE_FLAGS flags) {
	if (!(flags & TRANSCODE_PARTIAL) && dst + 4 <= dst_end) {
		//end-of-stream marker
		store_packed<uint32>(dst, block_header::ENDOFSTREAM);
		dst += 4;
	}
	return src;
}

const uint8*	LZFSE::encoder::process(uint8 *&dst, uint8 *dst_end, const uint8 *src, const uint8 *src_end, TRANSCODE_FLAGS flags)  {
	auto	dst_end1	= dst_end - 4;
	auto	src_size	= src_end - src;
	auto	dst_size	= dst_end - dst;

	// If input is too small, try encoding with LZVN
	if (src_size < LZVN_THRESHOLD) {
		if (dst_size > sizeof(lzvn_block)) {
			LZVN::encoder	lzvn;
			uint8*	dst1	= dst + sizeof(lzvn_block);
			auto	src1	= lzvn.process(dst1, dst_end1, src, src_end, flags);
			size_t	written = dst1 - (dst + sizeof(lzvn_block));
			if (written < src_size) {
				// If we could encode, setup header
				store_packed(dst, lzvn_block((uint32)src_size, (uint32)written));
				dst += sizeof(lzvn_block) + written;
				return finish(dst, dst_end, src1, flags);
			}
		}

	} else {
		// Try encoding with LZFSE
		LZFSE::_encoder	encoder(src, src_size);
		bool			ok = true;

		if (src_size >= 0xffffffffu) {
			// lzfse only uses 32 bits for offsets internally, so if the input buffer is really huge, we need to process it in smaller chunks
			const int64	block_size	= 262144;
			encoder.slide(0, block_size * 2);

			// The first chunk is processed normally
			dst		= encoder.process(dst, dst_end1);

			size_t	src_size2	= src_size - block_size * 2;
			while (dst && src_size2 >= block_size) {
				// subsequent chunks require a translation to keep the offsets from getting too big
				// Note that we are always going from block_size up to 2 * block_size so that the offsets remain positive
				encoder.slide(block_size, block_size);
				dst				= encoder.process(dst, dst_end1);
				src_size2		-= block_size;
			}
			// Set the end for the final chunk
			encoder.slide(block_size, src_size2);
		}
		// This is either the trailing chunk (if the source file is huge), or the whole source file
		if (dst && (dst = encoder.process(dst, dst_end1)) && (dst = encoder.finish(dst, dst_end1, src_size)))
			src += src_size;
		else
			src += encoder.consumed();
		return finish(dst, dst_end, src, flags);
	}

	// Compression failed for some reason; try uncompressed
	if (src_size + sizeof(uncompressed_block) <= dst_size && src_size == uint32(src_size)) {
		store_packed(dst, uncompressed_block(uint32(src_size)));
		dst		+= sizeof(uncompressed_block);
		memcpy(dst, src, src_size);
		dst		+= src_size;
		src		+= src_size;
		return finish(dst, dst_end, src, flags);
	}

	// Otherwise, there's nothing we can do, so return zero
	return 0;
}
//static bool _test_lfse = test_codec(LZFSE::encoder(), LZFSE::decoder());
