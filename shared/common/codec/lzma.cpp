#include "lzma.h"

using namespace lzma;

const uint8 *Decoder::process1(uint8 *&_dst, uint8 *dst_end, const uint8 *src, const uint8 *src_end, prefix_window win) {
	uint32	rep0	= reps[0], rep1 = reps[1], rep2 = reps[2], rep3 = reps[3];
	uint32	pbMask	= prop.pb_mask();

	LiteralHelper	lit(prop);

	prob_t	*probs	= probability + ProbOffset;
	uint32	state	= this->state;
	int		clip	= 0;
	uint8	*dst	= _dst;
	uint8	*base	= dst - processed;

	prob_decoder1	rc(src, range, code);
	do {
		auto	processed	= dst - base;
		int		posState	= (processed & pbMask) * NUM_STATES2;//<< 4;

		if (!rc.bit(probs + IsMatch + posState + state)) {
			//literal
			auto	prob = probs + Literal;
			if (processed)
				prob += lit(processed, *win.adjust_ref(dst - 1));

			uint32 symbol;
			if (state < LIT_STATES) {
				state	= state < MATCH_LIT ? LIT : state - 3;
				symbol	= rc.tree<8>(prob);

			} else {
				state	= state < MATCH2 ? state - 3 : state - 6;
				symbol	= rc.tree_matched(prob, 8, *win.adjust_ref(dst - rep0));
			}

			*dst++ = symbol;

		} else {
			bool	rep = rc.bit(probs + IsRep + state);
			if (rep) {
				if (!rc.bit(probs + IsRepGT0 + state)) {
					if (!rc.bit(probs + IsRep0Long + posState + state)) {
						uint8 b = *win.adjust_ref(dst - rep0);
						*dst++	= b;
						state	= NextShortRepState(state);
						continue;
					}
				} else {
					uint32 dist;
					if (!rc.bit(probs + IsRepGT1 + state)) {
						dist	= rep1;
					} else {
						if (!rc.bit(probs + IsRepGT2 + state)) {
							dist	= rep2;
						} else {
							dist	= rep3;
							rep3	= rep2;
						}
						rep2	= rep1;
					}
					rep1	= rep0;
					rep0	= dist;
				}
				state	= NextRepState(state);
			}
			
			prob_t	*prob	= probs + (rep ? RepLenCoder : LenCoder);
			uint32	len	=	!rc.bit(prob + LenLow)	? rc.tree<LenLowBits>(prob + LenLow + posState)
					:		!rc.bit(prob + LenMid)	? rc.tree<LenLowBits>(prob + LenMid + posState) + LenLowSymbols
					:		rc.tree<LenHighBits>(prob + LenHigh) + LenLowSymbols * 2;

			if (!rep) {
				uint32 dist = rc.tree<PosSlotBits>(probs + PosSlot + (min(len, LenToPosStates - 1) << PosSlotBits));

				if (dist >= StartPosModelIndex) {
					uint32 pos_slot		= dist;
					uint32 direct_bits	= (dist >> 1) - 1;
					dist				= (dist & 1) | 2;

					if (pos_slot < EndPosModelIndex) {
						dist <<= direct_bits;
						dist += rc.tree_reverse(probs + SpecPos + dist, direct_bits);

					} else {
						for (int i = direct_bits - AlignBits; i--;)
							dist = (dist << 1) | rc.bit_half();

						dist = (dist << AlignBits) | rc.tree_reverse<AlignBits>(probs + PosAlign);//, AlignBits);
						if (dist == 0xFFFFFFFFu) {
							clip	= LenMax;	// indicate finished
							break;
						}
					}
				}

				rep3	= rep2;
				rep2	= rep1;
				rep1	= rep0;
				rep0	= dist + 1;
				state	= NextMatchState(state);
			}

			len		+= LenMin;
			clip	= win.copy(dst, dst - rep0, dst + len, dst_end);
			dst		+= len - clip;
			if (clip)
				break;
		}

	} while (rc.file.p < src_end);

	rc.normalise();
	range		= rc.range;
	code		= rc.code;

	reps[0]		= rep0;
	reps[1]		= rep1;
	reps[2]		= rep2;
	reps[3]		= rep3;

	this->state	= state;
	remain_len	= clip;
	_dst		= dst;
	processed	= dst - base;

	return rc.file.p;
}

Decoder::DUMMY Decoder::Try(const uint8* src, const uint8* src_end, uint8 *dst, prefix_window win) const {
	prob_decoder<11, uint32, byte_reader, true>	rc(src, range, code);
	const prob_t*	probs = probability + ProbOffset;
	uint32			state = this->state;
	DUMMY			res;

	int		posState	= (processed & prop.pb_mask()) * NUM_STATES2;// << 4;

	if (!rc.bit(probs + IsMatch + posState + state)) {
		// if (src_end - buf >= 7) return DUMMY_LIT;
		auto	prob = probs + Literal;

		if (processed)
			prob += LiteralHelper(prop)(processed, *win.adjust_ref(dst - 1));

		if (state < LIT_STATES)
			rc.tree<8>(prob);
		else
			rc.tree_matched(prob, 8, *win.adjust_ref(dst - reps[0]));

		res = DUMMY_LIT;

	} else {
		const prob_t	*prob;

		if (!rc.bit(probs + IsRep + state)) {
			prob	= probs + LenCoder;
			res		= DUMMY_MATCH;

		} else {
			if (!rc.bit(probs + IsRepGT0 + state)) {
				if (!rc.bit(probs + IsRep0Long + posState + state)) {
					rc.normalise();
					return rc.file.p > src_end ? DUMMY_ERROR : DUMMY_REP;
				}
			} else {
				if (rc.bit(probs + IsRepGT1 + state))
					rc.bit(probs + IsRepGT2 + state);
			}
			prob	= probs + RepLenCoder;
			res		= DUMMY_REP;
		}

		uint32	len =	!rc.bit(prob + LenLow)	? rc.tree<LenLowBits>(prob + LenLow + posState)
				:		!rc.bit(prob + LenMid)	? rc.tree<LenLowBits>(prob + LenMid + posState) + LenLowSymbols
				:		rc.tree<LenHighBits>(prob + LenHigh) + LenLowSymbols * 2;

		if (res == DUMMY_MATCH) {
			uint32	pos_slot = rc.tree<PosSlotBits>(probs + PosSlot + (min(len, LenToPosStates - 1) << PosSlotBits));
			if (pos_slot >= StartPosModelIndex) {
				uint32 direct_bits = (pos_slot >> 1) - 1;
				// if (src_end - buf >= 8) return DUMMY_MATCH;
				if (pos_slot < EndPosModelIndex) {
					prob = probs + SpecPos + ((2 | (pos_slot & 1)) << direct_bits);

				} else {
					for (int i = direct_bits - AlignBits; i--;)
						rc.bit_half();

					prob		= probs + PosAlign;
					direct_bits	= AlignBits;
				}

				rc.tree_reverse(prob, direct_bits);
			}
		}
	}
	rc.normalise();
	return rc.file.p > src_end ? DUMMY_ERROR : res;
}

//	remain_len:
//	< LenMax	: normal remain
//	= LenMax	: finished
//	= LenMax + 1 : Flush marker

const uint8* Decoder::process(uint8*& dst, uint8* dst_end, const uint8* src, const uint8* src_end, prefix_window win) {
	if (remain_len > LenMax) {
		// reading rc header
		temp_len = min(src_end - src, INIT_SIZE);
		memcpy(temp, src, temp_len);
		src		+= temp_len;

		if (temp_len && temp[0])
			return nullptr;

		if (temp_len < INIT_SIZE)
			return src;

		code		= load_packed<uint32be>(temp + 1);
		range		= 0xFFFFFFFF;
		temp_len = 0;
		remain_len	= 0;
	}

	if (remain_len) {
		auto	clip = prefix_window::copy(dst, dst - reps[0], dst + remain_len, dst_end);
		processed	+= remain_len - clip;
		remain_len	= clip;
	}

	while (remain_len != LenMax && src != src_end) {
		if (temp_len == 0) {
			const uint8* bufLimit = src_end - sizeof(temp);

			if (bufLimit < src) {
				if (Try(src, src_end, dst, win) == DUMMY_ERROR) {
					memcpy(temp, src, temp_len = src_end - src);
					return src;
				}
				//only one symbol
				bufLimit = src;
			}

			src = process1(dst, dst_end, src, bufLimit, win);
			if (!src)
				return nullptr;

		} else {
			uint8	*d = temp + temp_len;
			while (d < end(temp) && src < src_end)
				*d++ = *src++;

			temp_len = d - temp;
			if (d < end(temp) && Try(temp, d, dst, win) == DUMMY_ERROR)
				return src;

			auto	p = process1(dst, dst_end, temp, temp, win);
			if (!p || p > d)
				return nullptr;
			
			temp_len = 0;
		}
	}

//	if (code)
//		return nullptr;

	return src;
}

const uint8* Decoder::process(uint8*& dst, uint8* dst_end, const uint8* src, const uint8* src_end, TRANSCODE_FLAGS flags) {
	return process(dst, dst_end, src, src_end, prefix_window());
}

//-----------------------------------------------------------------------------
// LZMA2 decoder
//-----------------------------------------------------------------------------

Decoder2::HeaderState Decoder2::UpdateHeaderState(uint8 b) {
	switch (header_state) {
		case HEAD_CONTROL:
			if (b == 0)
				return HEAD_FINISHED;

			control.u = b;
			if (control.lzma) {
				unpack_size = (uint32)control.size << 16;
			} else {
				if (b > 2)
					return HEAD_ERROR;
				unpack_size = 0;
			}
			return HEAD_UNPACK0;

		case HEAD_UNPACK0:
			unpack_size |= (uint32)b << 8;
			return HEAD_UNPACK1;

		case HEAD_UNPACK1:
			unpack_size += (uint32)b + 1;
			if (control.lzma)
				return HEAD_PACK0;
			Init(control.reset_dic);
			return HEAD_DATA;

		case HEAD_PACK0:
			pack_size = (uint32)b << 8;
			return HEAD_PACK1;

		case HEAD_PACK1:
			pack_size += (uint32)b + 1;
			if (control.prop) {
				return HEAD_PROP;

			case HEAD_PROP:
				prop = Props(b);
				if (prop.lc + prop.lp > CharPosBitsMax || prop.pb > 4)
					return HEAD_ERROR;
			}
//		case HEAD_DATA:
			Init(control.prop && control.reset);
			if (control.reset)
				State::Reset();
			return HEAD_DATA;

		default:
			return HEAD_ERROR;
	}
}

const uint8* Decoder2::process(uint8*& dst, uint8* dst_end, const uint8* src, const uint8* src_end, TRANSCODE_FLAGS flags) {
	prefix_window win;

	while (header_state != HEAD_ERROR) {
		if (dst == dst_end)
			return src;

		if (header_state != HEAD_DATA) {
			if (src == src_end)
				return src;

			header_state = UpdateHeaderState(*src++);
			if (header_state == HEAD_FINISHED)
				return src;

		} else if (!control.lzma) {
			//uncompressed

			if (src_end - src)
				return src;

			auto copy	= min(src_end - src, unpack_size);
			copy		-= win.simple_copy(dst, src, dst + copy, dst_end);

			src			+= copy;
			dst			+= copy;
			unpack_size	-= copy;

			if (unpack_size == 0)
				header_state = HEAD_CONTROL;

		} else {
			//lzma

			auto pack_end	= src + pack_size;
			auto unpack_end	= dst + unpack_size;
			
			src	= Decoder::process(dst, min(dst_end, unpack_end), src, min(src_end, pack_end), win);
			if (!src)
				break;

			pack_size	= pack_end - src;
			unpack_size	= unpack_end - dst;

			if (pack_size == 0 && unpack_size == 0)
				header_state = HEAD_CONTROL;

		}
	}

	return src;
}

//-----------------------------------------------------------------------------
// LZMA Encoder
//-----------------------------------------------------------------------------


uint32 MatchFinder::GetMatches(Match *matches, uint32 max_match) {
	Match	*out	= matches;
	uint32	i		= (p - start) % prev.size();
	uint32	value	= load_packed<uint32>(p);
	uint32	maxlen	= 0;

#if 1
	// Knuth multiplicative hash
	uint32	h	= value * 2654435761u;
	uint32	h2	= ((h << 16) >> (32 - HASH_BITS2));
	uint32	h3	= ((h << 8) >> (32 - HASH_BITS3)) + (1 << HASH_BITS2);
	uint32	h4	= (h >> (32 - HASH_BITS4)) + (1 << HASH_BITS2) + (1 << HASH_BITS3);

#else
	// original lzma hash
	const uint32	*crc = crc_table<uint32, 0xEDB88320, true>::t.begin();
	uint32	t	= crc[p[0]] ^ p[1];
	uint32	h2	= t & bits(HASH_BITS2);
	t	^= ((uint32)p[2] << 8);
	uint32	h3	= (t & bits(HASH_BITS3)) + (1 << HASH_BITS2);
	uint32	h4	= ((t ^ (crc[p[3]] << 5)) & bits(HASH_BITS4)) + (1 << HASH_BITS2) + (1 << HASH_BITS3);
#endif
	auto d2 = exchange(head[h2], i);
	if (d2) {
		if (maxlen = match_len(p, start + d2, end)) {
			out->len	= maxlen;
			out->dist	= i - d2 - 1;
			++out;
		}
	}

	auto d3 = exchange(head[h3], i);
	if (d3 && d3 != d2) {
		uint32	len = match_len(p, start + d3, end);
		if (len > maxlen) {
			out->len	= maxlen = len;
			out->dist	= i - d3 - 1;
			++out;
		}
	}

	for (auto d = head[h4]; d && maxlen < max_match; d = prev[d]) {
		uint32	len = match_len(p, start + d, end);
		if (len > maxlen) {
			out->len	= maxlen = len;
			out->dist	= i - d - 1;
			++out;
		}
	}
	prev[i]		= head[h4];
	head[h4]	= i;

	++p;
	return out - matches;
}


template<typename T, int PN, int MR, int C> cost_table<T, PN, MR, C>::cost_table() {
	T		w0		= 1 << (MR - 1);
	for (auto &p : prices) {
		uint32	bits	= 0;
		T		w		= w0;
		w0 += 1 << MR;
		for (uint32 j = C; j--; ) {
			w	*= w;
			bits <<= 1;
			while (w >= 1u << 16) {
				w >>= 1;
				++bits;

			}
		}
		p = T((PN << C) - (bits + 15));
	}
}

Encoder::ProbPrices_t	Encoder::ProbPrices;

void Encoder::UpdateLenPrices(Price prices[PosStatesMax][LenSymbolsTotal], uint32 numPosStates, const prob_t* probs, uint32 num_high) {
	uint32	prob	= probs[LenLow];
	Price	baseLow	= ProbPrices.price0(prob);
	Price	b		= ProbPrices.price1(prob);
	Price	baseMid	= b + ProbPrices.price0(probs[LenMid]);

	for (uint32 posState = 0; posState < numPosStates; posState++) {
		Price*	out		= prices[posState];
		auto	prob	= probs + (posState << (1 + LenLowBits));
		ProbPrices.tree_fill(out, prob, 3);
		ProbPrices.tree_fill(out + LenLowSymbols, prob + LenLowSymbols, 3);

		for (uint32 i = 0; i < LenLowSymbols; i++) {
			out[i] += baseLow;
			out[i + LenLowSymbols] += baseMid;
		}
	}

	if (num_high) {
		Price*	out		= prices[0] + LenLowSymbols * 2;
		auto	prob	= probs + LenHigh;

		for (uint32 i = 0; i < num_high; i += 2)
			ProbPrices.tree(prob, LenHighBits - 1, i >> 1, out[i + 0], out[i + 1]);

		b += ProbPrices.price1(probs[LenMid]);
		for (uint32 i = 0; i < num_high; i++)
			out[i]	+= b;

		for (uint32 posState = 1; posState < numPosStates; posState++)
			memcpy(prices[posState] + LenLowSymbols * 2, prices[0] + LenLowSymbols * 2, num_high * sizeof(Price));
	}
}

void Encoder::FillDistancesPrices(const prob_t *probs) {
	Price	tempPrices[NumFullDistances];

	for (uint32 i = StartPosModelIndex / 2; i < NumFullDistances / 2; i++) {
		uint32	pos_slot	= GetPosSlot(i);
		uint32	footerBits	= (pos_slot >> 1) - 1;
		uint32	base		= (2 | (pos_slot & 1)) << footerBits;
		ProbPrices.tree_reverse(probs + SpecPos + base * 2, footerBits, i, tempPrices[base + i], tempPrices[base + i + (1 << footerBits)]);
	}

	for (uint32 lps = 0; lps < LenToPosStates; lps++) {
		Price*	prices	= pos_slot_prices[lps];

		ProbPrices.tree_fill(prices, probs + PosSlot + (lps << PosSlotBits), PosSlotBits);

		for (uint32 slot = EndPosModelIndex / 2; slot < 32; slot++) {
			uint32 delta			= (slot - 1 - AlignBits) << kNumBitPriceShiftBits;
			prices[slot * 2 + 0]	+= delta;
			prices[slot * 2 + 1]	+= delta;
		}

		Price* dp = dist_prices[lps];

		dp[0] = prices[0];
		dp[1] = prices[1];
		dp[2] = prices[2];
		dp[3] = prices[3];

		for (uint32 i = 4; i < NumFullDistances; i += 2) {
			Price slotPrice	= prices[GetPosSlot(i)];
			dp[i]		= slotPrice + tempPrices[i];
			dp[i + 1]	= slotPrice + tempPrices[i + 1];
		}
	}
}

uint32 Encoder::GetOptimum(uint32 position, uint32 &_dist) {
	optCur = optEnd = opt;

	uint32	num_match, main_len;
	const prob_t*	probs	= probability + ProbOffset;

	if (additionalOffset == 0) {
		if (len_counter >= LEN_COUNT) {
			len_counter	= 0;
			ProbPrices.fill_tree_reverse(align_prices, probs + PosAlign, AlignBits);
			FillDistancesPrices(probs);
			UpdateLenPrices(len_prices, 1 << props.pb, probs + LenCoder, props.max_match - LenMin - LenLowSymbols * 2);
		}
		if (replen_counter >= LEN_COUNT) {
			replen_counter = 0;
			UpdateLenPrices(replen_prices, 1 << props.pb, probs + RepLenCoder, props.max_match - LenMin - LenLowSymbols * 2);
		}

		num_match	= ReadMatchDistances();
		main_len	= num_match ? matches[num_match - 1].len : 0;

	} else {
		main_len	= this->longestMatchLen;
		num_match	= this->num_match;
	}
	
	auto	match_end = matches + num_match;

	uint32	num_avail = min(match_finder.remaining(), LenMax - 1);
	if (num_avail < 2) {
		_dist = Optimal::Lit;
		return 1;
	}

	const uint8*	data	= match_finder.p - 1;

	LiteralHelper	lit(props);
	uint32			pbMask	= props.pb_mask();

	uint32			reps[NUM_REPS];
	uint32			repLens[NUM_REPS];
	uint32			repMaxIndex = 0;

	for (uint32 i = 0; i < NUM_REPS; i++) {
		reps[i]		= this->reps[i];
		auto data2	= data - reps[i];
		if (data[0] != data2[0] || data[1] != data2[1]) {
			repLens[i] = 0;
		} else {
			uint32	len = 2;
			while (len < num_avail && data[len] == data2[len])
				++len;
			repLens[i] = len;
			if (len > repLens[repMaxIndex])
				repMaxIndex = i;
		}
	}

	if (repLens[repMaxIndex] >= props.max_match) {
		_dist		= repMaxIndex;
		uint32	len	= repLens[repMaxIndex];
		MovePos(len - 1);
		return len;
	}

	if (main_len >= props.max_match) {
		_dist = match_end[-1].dist + NUM_REPS;
		MovePos(main_len - 1);
		return main_len;
	}

	uint8	cur_byte		= *data;
	uint8	match_byte	= *(data - reps[0]);
	uint32	last		= max(repLens[repMaxIndex], main_len);

	if (last < 2 && cur_byte != match_byte) {
		_dist = Optimal::Lit;
		return 1;
	}

	opt[0].SetState(state, reps);
	uint32	posState		= position & pbMask;
	auto	prob			= probs + Literal + lit(position, *(data - 1));

	opt[1].Set(ProbPrices.price0(probs[IsMatch + PosState(state, posState)])
		+ (IsLitState(state)
			? ProbPrices.tree(prob, 8, cur_byte)
			: ProbPrices.tree_matched(prob, 8, cur_byte, match_byte)
		), 1, Optimal::Lit);

	Price	match_price		= ProbPrices.price1(probs[IsMatch + PosState(state, posState)]);
	Price	repmatch_price	= match_price + ProbPrices.price1(probs[IsRep + state]);

	if (match_byte == cur_byte && repLens[0] == 0) {
		opt[1].SetIfBetter(repmatch_price + price_ShortRep(probs, state, posState), 1, Optimal::ShortRep);
		if (last < 2) {
			_dist = opt[1].dist;
			return 1;
		}
	}

	// ---------- REP ----------

	for (uint32 i = 0; i < NUM_REPS; i++) {
		uint32	repLen = repLens[i];
		if (repLen >= 2) {
			Price	price = repmatch_price + price_Rep(probs, i, state, posState);
			while (repLen >= 2) {
				opt[repLen].SetIfBetter(price + replen_prices[posState][repLen - LenMin], repLen, i);
				--repLen;
			}
		}
	}

	// ---------- MATCH ----------

	uint32 len = repLens[0] + 1;
	if (len <= main_len) {
		uint32	normalMatchPrice = match_price + ProbPrices.price0(probs[IsRep + state]);
		auto	match	= matches;

		if (len < 2) {
			len = 2;
		} else {
			while (len > match->len)
				++match;
		}

		for (;; len++) {
			uint32	dist	= match->dist;
			uint32	lenToPosState = GetLenToPosState(len);

			opt[len].SetIfBetter(normalMatchPrice + len_prices[posState][len - LenMin]
				+ (dist < NumFullDistances
					? dist_prices[lenToPosState][dist & (NumFullDistances - 1)]
					: align_prices[dist & bits(AlignBits)] + pos_slot_prices[lenToPosState][GetPosSlot(dist)]
				), len, dist + NUM_REPS);

			if (len == match->len && ++match == match_end)
				break;
		}
	}

	// ---------- Optimal Parsing ----------

	Optimal	*cur = opt, *opt_last = opt + last;
	while (++cur != opt_last) {
		if (cur >= end(opt) - 64) {
			Optimal	*best	= cur;
			for (auto j = cur + 1; j <= opt_last; j++) {
				if (best->price >= j->price)
					best	= j;
			}
			if (uint32 delta = best - cur)
				MovePos(delta);
			cur = best;
			break;
		}

		num_match		= ReadMatchDistances();
		uint32	newLen	= num_match ? matches[num_match - 1].len : 0;
		if (newLen >= props.max_match) {
			this->num_match			= num_match;
			this->longestMatchLen	= newLen;
			break;
		}

		position++;
		
		auto	prev	= cur - cur->len;
		uint32	state	= prev->state;
		uint32	dist	= cur->dist;

		if (cur->len == 1) {
			state	= dist == Optimal::ShortRep ? NextShortRepState(state) : NextLitState(state);

		} else {
			if (cur->extra) {
				prev	-= cur->extra;
				state	= cur->extra != 1 ? REP : dist < NUM_REPS ? REP : MATCH;
			} else {
				state	= dist < NUM_REPS ? NextRepState(state) : NextMatchState(state);
			}

			uint32	b0		= prev->reps[0];

			if (dist < NUM_REPS) {
				if (dist == 0) {
					reps[0] = b0;
					reps[1] = prev->reps[1];
					reps[2] = prev->reps[2];
					reps[3] = prev->reps[3];
				} else {
					reps[1] = b0;
					b0		= prev->reps[1];
					if (dist == 1) {
						reps[0] = b0;
						reps[2] = prev->reps[2];
						reps[3] = prev->reps[3];
					} else {
						reps[2] = b0;
						reps[0] = prev->reps[dist];
						reps[3] = prev->reps[dist ^ 1];
					}
				}
			} else {
				reps[0] = (dist - NUM_REPS + 1);
				reps[1] = b0;
				reps[2] = prev->reps[1];
				reps[3] = prev->reps[2];
			}
		}

		cur->SetState(state, reps);

		const uint8* data	= match_finder.p - 1;
		uint8	cur_byte	= *data;
		uint8	match_byte	= *(data - reps[0]);
		uint32	posState	= position & pbMask;

		//	The order of Price checks:
		//	<  LIT
		//	<= SHORT_REP
		//	<  LIT : REP_0
		//	<  REP    [ : LIT : REP_0 ]
		//	<  MATCH  [ : LIT : REP_0 ]

		uint32	prob		= probs[IsMatch + PosState(state, posState)];
		uint32	match_price	= cur->price + ProbPrices.price1(prob);
		uint32	lit_price	= cur->price + ProbPrices.price0(prob);

		auto	next		= cur + 1;
		bool	nextIsLit	= false;

		if ((next->price < ProbPrices.infinity && match_byte == cur_byte) || lit_price > next->price) {
			lit_price	= 0;
		} else {
			const prob_t* prob = probs + Literal + lit(position, *(data - 1));
			lit_price	+= !IsLitState(state) ? ProbPrices.tree_matched(prob, 8, cur_byte, match_byte) : ProbPrices.tree(prob, 8, cur_byte);
			nextIsLit	= next->SetIfBetter(lit_price, 1, Optimal::Lit);
		}

		uint32	repmatch_price	= match_price + ProbPrices.price1(probs[IsRep + state]);
		uint32	num_avail_full	= min(num_avail, end(opt) - cur - 1);

		// ---------- SHORT_REP ----------

		if (IsLitState(state) && match_byte == cur_byte && repmatch_price < next->price && (next->len < 2 || next->dist != 0)) {
			if (next->SetIfBetter(repmatch_price + price_ShortRep(probs, state, posState), 1, Optimal::ShortRep))
				nextIsLit = false;
		}

		if (num_avail_full < 2)
			continue;

		// ---------- LIT : REP_0 ----------

		if (!nextIsLit && lit_price != 0 && match_byte != cur_byte && num_avail_full > 2) {
			const uint8* data2 = data - reps[0];
			if (data[1] == data2[1] && data[2] == data2[2]) {
				uint32 limit	= min(props.max_match + 1, num_avail_full);
				uint32 len		= 3;
				while (len < limit && data[len] == data2[len])
					++len;

				Price	price	= lit_price + price_Rep0(probs, NextLitState(state), (position + 1) & pbMask);
				auto	offset	= cur + len;

				if (opt_last < offset)
					opt_last = offset;

				--len;
				offset->SetIfBetter(price + replen_prices[posState][len - LenMin], len, 0, 1);
			}
		}

		uint32	num_avail	= min(num_avail_full, props.max_match);
		uint32	startLen	= 2; /* speed optimization */

		// ---------- REP ----------

		for (uint32 rep = 0; rep < NUM_REPS; rep++) {
			const uint8* data2 = data - reps[rep];
			if (data[0] != data2[0] || data[1] != data2[1])
				continue;

			uint32	len = 2;
			while (len < num_avail && data[len] == data2[len])
				++len;

			if (rep == 0)
				startLen = len + 1;

			opt_last = max(opt_last, cur + len);

			Price	price	= repmatch_price + price_Rep(probs, rep, state, posState);
			for (uint32 len2 = len; len2 >= 2; --len2)
				cur[len2].SetIfBetter(price + replen_prices[posState][len2 - LenMin], len2, rep);


			// ---------- REP : LIT : REP_0 ----------

			uint32 limit	= min(len + 1 + props.max_match, num_avail_full);
			uint32 len2		= len + 3;

			if (len2 <= limit && data[len2 - 2] == data2[len2 - 2] && data[len2 - 1] == data2[len2 - 1]) {
				uint32	posState1	= (position + len) & pbMask;
				uint32	posState2	= (posState1 + 1) & pbMask;
				price	+= replen_prices[posState][len - LenMin]
						+ ProbPrices.price0(probs[IsMatch + PosState(NextRepState(state), posState1)])
						+ ProbPrices.tree_matched(probs + Literal + lit(position + len, data[len - 1]), 8, data[len], data2[len])
						+ price_Rep0(probs, REP_LIT, posState2);

				while (len2 < limit && data[len2] == data2[len2])
					++len2;

				auto	offset = cur + len2;
				if (opt_last < offset)
					opt_last = offset;

				len2 -= len + 1;
				offset->SetIfBetter(price + replen_prices[posState2][len2 - LenMin], len2, rep, len + 1);
			}
		}

		// ---------- MATCH ----------

		match_end = matches + num_match;
		if (newLen > num_avail) {
			newLen			= num_avail;
			auto	match	= matches;
			while (match < match_end && match->len < newLen)
				++match;
			match->len	= (uint32)newLen;
			match_end	= match + 1;
		}

		if (newLen >= startLen) {
			uint32	normalMatchPrice = match_price + ProbPrices.price0(probs[IsRep + state]);

			opt_last = max(opt_last, cur + newLen);

			auto	match = matches;
			while (startLen > match->len)
				++match;

			uint32	dist		= match->dist;
			uint32	pos_slot	= GetPosSlot(dist);

			for (uint32 len = /*2*/ startLen;; len++) {
				uint32	lenNorm	= min(len - 2, LenToPosStates - 1);
				Price	price	= normalMatchPrice + len_prices[posState][len - LenMin] + (dist < NumFullDistances
						? dist_prices[lenNorm][dist & (NumFullDistances - 1)]
						: pos_slot_prices[lenNorm][pos_slot] + align_prices[dist & bits(AlignBits)]
					);

				cur[len].SetIfBetter(price, len, dist + NUM_REPS);

				if (len == match->len) {
					const uint8* data2 = data - dist - 1;
					uint32	limit	= min(len + 1 + props.max_match, num_avail_full);
					uint32	len2	= len + 3;

					if (len2 <= limit && data[len2 - 2] == data2[len2 - 2] && data[len2 - 1] == data2[len2 - 1]) {
						uint32	posState1	= (position + len) & pbMask;
						uint32	posState2	= (posState1 + 1) & pbMask;
						price	+=	ProbPrices.price0(probs[IsMatch + PosState(NextMatchState(state), posState1)])
								+	ProbPrices.tree_matched(probs + Literal + lit(position + len, data[len - 1]), 8, data[len], data2[len])
								+	price_Rep0(probs, MATCH_LIT, posState2);

						while (len2 < limit && data[len2] == data2[len2])
							++len2;

						auto	offset = cur + len2;
						if (opt_last < offset)
							opt_last = offset;

						len2 -= len + 1;
						offset->SetIfBetter(price + replen_prices[posState2][len2 - LenMin], len2, dist + NUM_REPS, len + 1);
					}

					if (++match == match_end)
						break;

					dist		= match->dist;
					pos_slot	= GetPosSlot(dist);
				}
			}
		}
	}

	do
		opt_last->price = ProbPrices.infinity;
	while (--opt_last != opt);

	optEnd		= cur + 1;

	for (Optimal *wr = optEnd;;) {
		uint32	dist	= cur->dist;
		uint32	len		= cur->len;
		uint32	extra	= cur->extra;
		cur -= len;

		if (extra) {
			--wr;
			wr->len = len;
			cur -= extra;
			len	= extra;
			if (extra == 1) {
				wr->dist	= dist;
				dist		= Optimal::Lit;
			} else {
				wr->dist	= 0;
				--len;
				--wr;
				wr->dist	= Optimal::Lit;
				wr->len		= 1;
			}
		}

		if (cur == opt) {
			_dist	= dist;
			optCur	= wr;
			return len;
		}

		--wr;
		wr->dist	= dist;
		wr->len		= len;
	}
}

static inline uint32 ChangePair(uint32 smallDist, uint32 bigDist) {
	return (bigDist >> 7) > smallDist;
}

uint32 Encoder::GetOptimumFast(uint32 &_dist) {
	uint32	main_len, num_match;

	if (additionalOffset == 0) {
		num_match	= ReadMatchDistances();
		main_len	= num_match ? matches[num_match - 1].len : 0;
	} else {
		main_len	= longestMatchLen;
		num_match	= this->num_match;
	}

	_dist		= Optimal::Lit;

	uint32	num_avail	= min(match_finder.remaining(), LenMax - 1);
	if (num_avail < 2)
		return 1;

	const uint8*	data	= match_finder.p - 1;
	uint32			repLen	= 0, rep = 0;

	for (uint32 i = 0; i < NUM_REPS; i++) {
		const uint8* data2 = data - reps[i];
		if (data[0] == data2[0] && data[1] == data2[1]) {
			uint32	len	= 2;
			while (len < num_avail && data[len] == data2[len])
				++len;

			if (len >= props.max_match) {
				_dist = i;
				MovePos(len - 1);
				return len;
			}
			if (len > repLen) {
				rep = i;
				repLen	= len;
			}
		}
	}

	if (main_len >= props.max_match) {
		_dist = matches[num_match - 1].dist + NUM_REPS;
		MovePos(main_len - 1);
		return main_len;
	}

	uint32	mainDist = 0;

	if (main_len >= 2) {
		mainDist = matches[num_match - 1].dist;
		while (num_match > 2) {
			if (main_len != matches[num_match - 2].len + 1)
				break;

			uint32	dist2 = matches[num_match - 2].dist;
			if (!ChangePair(dist2, mainDist))
				break;

			--num_match;
			--main_len;
			mainDist = dist2;
		}

		if (main_len == 2 && mainDist >= 0x80)
			main_len = 1;
	}

	if (repLen >= 2 && (repLen + 1 >= main_len || (repLen + 2 >= main_len && mainDist >= (1 << 9)) || (repLen + 3 >= main_len && mainDist >= (1 << 15)))) {
		_dist = rep;
		MovePos(repLen - 1);
		return repLen;
	}

	if (main_len < 2 || num_avail <= 2)
		return 1;

	num_match		= ReadMatchDistances();
	uint32	len1	= num_match ? matches[num_match - 1].len : 0;
	longestMatchLen = len1;

	if (len1 >= 2) {
		uint32 newDist = matches[num_match - 1].dist;
		if (	(len1 >= main_len		&& newDist < mainDist)
			||	(len1 == main_len + 1	&& !ChangePair(mainDist, newDist))
			||	(len1 > main_len + 1)
			||	(len1 > main_len - 2	&& main_len >= 3 && ChangePair(newDist, mainDist))
		)
			return 1;
	}

	data = match_finder.p - 1;

	for (uint32 i = 0; i < NUM_REPS; i++) {
		const uint8* data2 = data - reps[i];
		if (data[0] == data2[0] && data[1] == data2[1]) {
			for (uint32 len = 2, limit = main_len - 1;; len++) {
				if (len >= limit)
					return 1;
				if (data[len] != data2[len])
					break;
			}
		}
	}

	_dist = mainDist + NUM_REPS;
	if (main_len != 2)
		MovePos(main_len - 2);

	return main_len;
}

void Encoder::Encode_Len(encoder& rc, uint32 len, prob_t *probs, uint32 posState) {
	if (rc.bit(probs, len >= LenLowSymbols)) {
		len		-= LenLowSymbols;
		probs	+= LenLowSymbols;
		
		if (rc.bit(probs, len >= LenLowSymbols)) {
			rc.tree(probs + LenHigh - LenLowSymbols, 8, len - LenLowSymbols);
			return;
		}
	}

	rc.tree(probs + 2 * (posState << LenLowBits), LenLowBits, len);
}

void Encoder::Encode_Dist(encoder& rc, uint32 dist, prob_t *probs, uint32 posState) {
	uint32	pos_slot	= dist < 2 ? dist : GetPosSlot(dist);
	rc.tree(probs + PosSlot + (posState << PosSlotBits), PosSlotBits, pos_slot);

	if (dist >= StartPosModelIndex) {
		uint32 footerBits = (pos_slot >> 1) - 1;
		if (dist < NumFullDistances) {
			uint32 base = (2 | (pos_slot & 1)) << footerBits;
			rc.tree_reverse(probs + SpecPos + base, footerBits, dist);
		} else {
			for (uint32 pos2 = (dist | 0xF) << (32 - footerBits);pos2 != 0xF0000000; pos2 <<= 1)
				rc.bit_half(pos2 >> 31);
			rc.tree_reverse(probs + PosAlign, AlignBits, dist);
		}
	}
}

void Encoder::WriteEndMarker(encoder &rc, prob_t *probs, uint32 state, uint32 posState) {
	rc.bit1(probs + IsMatch + PosState(state, posState));
	rc.bit0(probs + IsRep + state);

	Encode_Len(rc, 0, probs + LenCoder, posState);
	Encode_Dist(rc, 0xffffffff, probs, 0);
}

bool Encoder::CodeOneBlock(encoder &rc, uint32 maxPackSize, uint32 maxUnpackSize) {
	prob_t			*probs	= probability + ProbOffset;
	LiteralHelper	lit(props);
	uint32			pbMask	= props.pb_mask();
	uint32			pos32	= (uint32)position;

	if (position == 0) {
		if (match_finder.remaining() == 0)
			return true;

		rc.bit0(probs + IsMatch);
		rc.tree(probs + Literal, 8, *match_finder.p);
		
		match_finder.GetMatches(matches, props.max_match);
		++pos32;
	}

	for (;;) {
		uint32	len, dist;

		if (props.fast) {
			len		= GetOptimumFast(dist);
		} else if (optEnd == optCur) {
			len		= GetOptimum(pos32, dist);
		} else {
			len		= optCur->len;
			dist	= optCur->dist;
			++optCur;
		}

		uint32	posState	= pos32 & pbMask;

		if (rc.bit(probs + IsMatch + PosState(state, posState), dist != Optimal::Lit)) {
			if (rc.bit(probs + IsRep + state, dist < NUM_REPS)) {
				if (rc.bit(probs + IsRepGT0 + state, dist != 0)) {
					if (rc.bit(probs + IsRepGT1 + state, dist != 1)) {
						if (rc.bit(probs + IsRepGT2 + state, dist != 2)) {
							dist	= reps[3];
							reps[3]	= reps[2];
						} else {
							dist	= reps[2];
						}
						reps[2]	= reps[1];
					} else {
						dist	= reps[1];
					}
					reps[1]	= reps[0];
					reps[0]	= dist;
				} else {
					if (!rc.bit(probs + IsRep0Long + PosState(state, posState), len != 1))
						state = NextShortRepState(state);
				}

				if (len != 1) {
					Encode_Len(rc, len - LenMin, probs + RepLenCoder, posState);
					++replen_counter;
					state = NextRepState(state);
				}

			} else {
				Encode_Len(rc, len - LenMin, probs + LenCoder, posState);
				++len_counter;

				dist	-= NUM_REPS;
				reps[3]	= reps[2];
				reps[2]	= reps[1];
				reps[1]	= reps[0];
				reps[0]	= dist + 1;

				Encode_Dist(rc, dist, probs, GetLenToPosState(len));
				state	= NextMatchState(state);
			}

		} else {
			const uint8* data	= match_finder.p - additionalOffset;
			auto		prob	= probs + Literal + lit(pos32, *(data - 1));
			if (IsLitState(state))
				rc.tree(prob, 8, *data);
			else
				rc.tree_matched(prob, 8, *data, *(data - reps[0]));
			state	= NextLitState(state);
		}

		pos32				+= len;
		additionalOffset	-= len;

		if (additionalOffset == 0) {
			uint32 processed = pos32 - (uint32)position;
			if (match_finder.remaining() == 0 || (maxPackSize
				? processed + kNumOpts + 300 >= maxUnpackSize || rc.tell() + (kNumOpts * 8) >= maxPackSize
				: processed >= 1 << 17
			)) {
				position += processed;
				return true;
			}
		}
	}

}

void Encoder::Init() {
	State::Reset();
	additionalOffset	= 0;

	if (!props.fast) {
		optEnd = optCur = opt;
		for (auto &i : opt)
			i.price = ProbPrices.infinity;
	}
}

const uint8* Encoder::process(uint8*& dst, uint8* dst_end, const uint8* src, const uint8* src_end, TRANSCODE_FLAGS flags) {
	encoder rc(memory_block(dst, dst_end));
	match_finder.SetSource(src, src_end - src);

	do {
		if (!CodeOneBlock(rc, 0, 0))
			return nullptr;

		if (match_finder.remaining() == 0) {
			if (!(flags & TRANSCODE_PARTIAL))
				WriteEndMarker(rc, probability + ProbOffset, state, position & props.pb_mask());
		}
		rc.flush();

	} while (match_finder.remaining());

	dst = (uint8*)rc.file.getp();
	return match_finder.p;
}

//-----------------------------------------------------------------------------
// LZMA2 Encoder
//-----------------------------------------------------------------------------

const uint8 *Encoder2::Coder::process(Encoder2* me, uint8*& dst, uint8* dst_end, const uint8* src, const uint8* src_end) {
	for (;;) {
		const uint8*	block		= src;
		const uint8*	block_end	= me->blocksize == Encoder2::BLOCK_SIZE_SOLID ? src_end : min(src_end, src + me->blocksize);

		match_finder.SetSource(block, block_end - block);//KEEP_WINDOW_SIZE;

		bool	needInitState	= true;
		bool	needInitProp	= true;

		for (;;) {
			size_t	pack_size		= min(dst_end - dst, PACK_SIZE_MAX + 16);
			uint32	lzHeaderSize	= 5 + needInitProp;

			if (pack_size < lzHeaderSize)
				break;

			pack_size -= lzHeaderSize;

			if (needInitState)
				Init();

			InitPrices();

			State	save_state	= *this;
			uint64	start		= position;
			encoder rc(memory_block(dst + lzHeaderSize, pack_size));
			bool	res = CodeOneBlock(rc, PACK_SIZE_MAX, UNPACK_SIZE_MAX);
			rc.flush();

			uint32	unpack_size	= uint32(position - start);
			pack_size	= rc.file.tell();

			if (unpack_size == 0)
				break;

			if (!res || (pack_size + 2 >= unpack_size || pack_size > (1 << 16))) {
				*(State*)this = save_state;

				while (unpack_size > 0) {
					uint32 u = min(unpack_size, COPY_CHUNK_SIZE);
					if (dst_end - dst < u + 3)
						return src;

					*dst++ = Control::Copy(src == block);
					*dst++ = uint8((u - 1) >> 8);
					*dst++ = uint8(u - 1);
					memcpy(dst, src, u);
					src			+= u;
					dst			+= u;
					unpack_size	-= u;
				}

			} else {
				uint32	u	= unpack_size - 1;
				uint32	p	= uint32(pack_size - 1);

				*dst++ = Control::Compress(u >> 16, needInitState, needInitProp);
				*dst++ = uint8(u >> 8);
				*dst++ = uint8(u);
				*dst++ = uint8(p >> 8);
				*dst++ = uint8(p);

				if (needInitProp)
					*dst++ = props.encode();

				needInitProp	= false;
				needInitState	= false;
				src				+= unpack_size;
				dst				+= pack_size;
			}
		}

		if (src != block_end)
			return nullptr;
	}
}

//static bool _test_lzma = test_codec(lzma::Encoder(), lzma::Decoder(), false);
