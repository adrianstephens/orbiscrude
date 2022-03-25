#include "lzma.h"

using namespace lzma;

enum {
	kTopValue			= 1 << 24,

	kNumBitModelTotalBits= 11,
	kBitModelTotal		= 1 << kNumBitModelTotalBits,
	kNumMoveBits		= 5,
	kNumPosBitsMax		= 4,
	kNumPosStatesMax	= 1 << kNumPosBitsMax,

	kLenNumLowBits		= 3,
	kLenNumLowSymbols	= 1 << kLenNumLowBits,
	kLenNumMidBits		= 3,
	kLenNumMidSymbols	= 1 << kLenNumMidBits,
	kLenNumHighBits		= 8,
	kLenNumHighSymbols	= 1 << kLenNumHighBits,

	LenChoice			= 0,
	LenChoice2			= LenChoice + 1,
	LenLow				= LenChoice2 + 1,
	LenMid				= LenLow + (kNumPosStatesMax << kLenNumLowBits),
	LenHigh				= LenMid + (kNumPosStatesMax << kLenNumMidBits),
	kNumLenProbs		= LenHigh + kLenNumHighSymbols,


	kNumStates			= 12,
	kNumLitStates		= 7,

	kStartPosModelIndex	= 4,
	kEndPosModelIndex	= 14,
	kNumFullDistances	= 1 << (kEndPosModelIndex >> 1),

	kNumPosSlotBits		= 6,
	kNumLenToPosStates	= 4,

	kNumAlignBits		= 4,
	kAlignTableSize		= 1 << kNumAlignBits,

	kMatchMinLen		= 2,
	kMatchSpecLenStart	= kMatchMinLen + kLenNumLowSymbols + kLenNumMidSymbols + kLenNumHighSymbols,

	IsMatch				= 0,
	IsRep				= IsMatch + (kNumStates << kNumPosBitsMax),
	IsRepG0				= IsRep + kNumStates,
	IsRepG1				= IsRepG0 + kNumStates,
	IsRepG2				= IsRepG1 + kNumStates,
	IsRep0Long			= IsRepG2 + kNumStates,
	PosSlot				= IsRep0Long + (kNumStates << kNumPosBitsMax),
	SpecPos				= PosSlot + (kNumLenToPosStates << kNumPosSlotBits),
	Align				= SpecPos + kNumFullDistances - kEndPosModelIndex,
	LenCoder			= Align + kAlignTableSize,
	RepLenCoder			= LenCoder + kNumLenProbs,
	Literal				= RepLenCoder + kNumLenProbs,

	LZMA_BASE_SIZE		= 1846,
	LZMA_LIT_SIZE		= 768,
	LZMA_DIC_MIN		= 1 << 12,
	LZMA_PROPS_SIZE		= 5,

	RC_INIT_SIZE		= 5,
};

#define NORMALIZE							if (range < kTopValue) { range <<= 8; code = (code << 8) | (*buf++); }

#define IF_BIT_0(p)							ttt = *(p); NORMALIZE; bound = (range >> kNumBitModelTotalBits) * ttt; if (code < bound)
#define UPDATE_0(p)							range = bound; *(p) = (CLzmaProb)(ttt + ((kBitModelTotal - ttt) >> kNumMoveBits));
#define UPDATE_1(p)							range -= bound; code -= bound; *(p) = (CLzmaProb)(ttt - (ttt >> kNumMoveBits));

#define GET_BIT2(p, i, A0, A1)				IF_BIT_0(p)	{ UPDATE_0(p); i = (i + i); A0; } else { UPDATE_1(p); i = (i + i) + 1; A1; }
#define GET_BIT(p, i)						GET_BIT2(p, i,; ,;)
#define TREE_GET_BIT(probs, i)				{ GET_BIT((probs + i), i); }
#define TREE_DECODE(probs, limit, i)		{ i = 1; do { TREE_GET_BIT(probs, i); } while (i < limit); i -= limit; }

// #define _LZMA_SIZE_OPT
#ifdef _LZMA_SIZE_OPT
#define TREE_6_DECODE(probs, i) TREE_DECODE(probs, (1 << 6), i)
#else
#define TREE_6_DECODE(probs, i) { i = 1; TREE_GET_BIT(probs, i); TREE_GET_BIT(probs, i); TREE_GET_BIT(probs, i); TREE_GET_BIT(probs, i); TREE_GET_BIT(probs, i); TREE_GET_BIT(probs, i); i -= 0x40; }
#endif


#define NORMALIZE_CHECK						if (range < kTopValue) { if (buf >= bufLimit) return DUMMY_ERROR; range <<= 8; code = (code << 8) | (*buf++); }
#define IF_BIT_0_CHECK(p)					ttt = *(p); NORMALIZE_CHECK; bound = (range >> kNumBitModelTotalBits) * ttt; if (code < bound)
#define UPDATE_0_CHECK						range = bound;
#define UPDATE_1_CHECK						range -= bound; code -= bound;
#define GET_BIT2_CHECK(p, i, A0, A1)		IF_BIT_0_CHECK(p) { UPDATE_0_CHECK; i = (i + i); A0; } else { UPDATE_1_CHECK; i = (i + i) + 1; A1; }
#define GET_BIT_CHECK(p, i)					GET_BIT2_CHECK(p, i,; ,;)
#define TREE_DECODE_CHECK(probs, limit, i) { i = 1; do { GET_BIT_CHECK(probs + i, i) } while (i < limit); i -= limit; }

#define LzmaProps_GetNumProbs(p)			((uint32)LZMA_BASE_SIZE + (LZMA_LIT_SIZE << ((p)->lc + (p)->lp)))

#if Literal != LZMA_BASE_SIZE
StopCompilingDueBUG
#endif

// First LZMA-symbol is always decoded.
//And it decodes new LZMA-symbols while (buf < bufLimit), but "buf" is without last normalization
//Out:
//	Result:
//	SZ_OK - OK
//	SZ_ERROR_DATA - Error
//	p->remainLen:
//	< kMatchSpecLenStart : normal remain
//	= kMatchSpecLenStart : finished
//	= kMatchSpecLenStart + 1 : Flush marker
//	= kMatchSpecLenStart + 2 : State Init Marker

int CLzmaDec::DecodeReal(size_t limit, const uint8 *bufLimit) {
	uint32		rep0		= reps[0], rep1 = reps[1], rep2 = reps[2], rep3 = reps[3];
	unsigned	pbMask		= ((unsigned)1 << prop.pb) - 1;
	unsigned	lpMask		= ((unsigned)1 << prop.lp) - 1;
	unsigned	lc			= prop.lc;

	CLzmaProb	*probs		= this->probs;
	unsigned	state		= this->state;

	uint8		*dic		= this->dic;
	size_t		dicBufSize	= this->dicBufSize;
	size_t		pos			= this->dicPos;

	uint32		processedPos= this->processedPos;
	uint32		checkDicSize= this->checkDicSize;

	const uint8 *buf		= this->buf;
	uint32		range		= this->range;
	uint32		code		= this->code;
	
	unsigned	len = 0;

	do {
		uint32 bound;
		unsigned ttt;
		unsigned posState = processedPos & pbMask;

		CLzmaProb *prob = probs + IsMatch + (state << kNumPosBitsMax) + posState;
		IF_BIT_0(prob) {
			unsigned symbol;
			UPDATE_0(prob);
			prob = probs + Literal;
			if (checkDicSize != 0 || processedPos != 0)
				prob += (LZMA_LIT_SIZE * (((processedPos & lpMask) << lc) +
				(dic[(pos == 0 ? dicBufSize : pos) - 1] >> (8 - lc))));

			if (state < kNumLitStates) {
				state -= (state < 4) ? state : 3;
				symbol = 1;
				do { GET_BIT(prob + symbol, symbol) } while (symbol < 0x100);
			} else {
				unsigned matchByte = dic[(pos - rep0) + ((pos < rep0) ? dicBufSize : 0)];
				unsigned offs = 0x100;
				state -= (state < 10) ? 3 : 6;
				symbol = 1;
				do {
					unsigned bit;
					CLzmaProb *probLit;
					matchByte <<= 1;
					bit = (matchByte & offs);
					probLit = prob + offs + bit + symbol;
					GET_BIT2(probLit, symbol, offs &= ~bit, offs &= bit)
				}
				while (symbol < 0x100);
			}
			dic[pos++] = (uint8)symbol;
			processedPos++;
			continue;
		} else {
			UPDATE_1(prob);
			prob = probs + IsRep + state;
			IF_BIT_0(prob) {
				UPDATE_0(prob);
				state += kNumStates;
				prob = probs + LenCoder;
			} else {
				UPDATE_1(prob);
				if (checkDicSize == 0 && processedPos == 0)
					return SZ_ERROR_DATA;
				prob = probs + IsRepG0 + state;
				IF_BIT_0(prob) {
					UPDATE_0(prob);
					prob = probs + IsRep0Long + (state << kNumPosBitsMax) + posState;
					IF_BIT_0(prob) {
						UPDATE_0(prob);
						dic[pos] = dic[(pos - rep0) + ((pos < rep0) ? dicBufSize : 0)];
						pos++;
						processedPos++;
						state = state < kNumLitStates ? 9 : 11;
						continue;
					}
					UPDATE_1(prob);
				} else {
					uint32 distance;
					UPDATE_1(prob);
					prob = probs + IsRepG1 + state;
					IF_BIT_0(prob) {
						UPDATE_0(prob);
						distance = rep1;
					} else {
						UPDATE_1(prob);
						prob = probs + IsRepG2 + state;
						IF_BIT_0(prob) {
							UPDATE_0(prob);
							distance = rep2;
						} else {
							UPDATE_1(prob);
							distance = rep3;
							rep3 = rep2;
						}
						rep2 = rep1;
					}
					rep1 = rep0;
					rep0 = distance;
				}
				state = state < kNumLitStates ? 8 : 11;
				prob = probs + RepLenCoder;
			}

			unsigned limit, offset;
			CLzmaProb *probLen = prob + LenChoice;

			IF_BIT_0(probLen) {
				UPDATE_0(probLen);
				probLen = prob + LenLow + (posState << kLenNumLowBits);
				offset = 0;
				limit = (1 << kLenNumLowBits);
			} else {
				UPDATE_1(probLen);
				probLen = prob + LenChoice2;
				IF_BIT_0(probLen) {
					UPDATE_0(probLen);
					probLen = prob + LenMid + (posState << kLenNumMidBits);
					offset = kLenNumLowSymbols;
					limit = (1 << kLenNumMidBits);
				} else {
					UPDATE_1(probLen);
					probLen = prob + LenHigh;
					offset = kLenNumLowSymbols + kLenNumMidSymbols;
					limit = (1 << kLenNumHighBits);
				}
			}
			TREE_DECODE(probLen, limit, len);
			len += offset;

			if (state >= kNumStates) {
				uint32 distance;
				prob = probs + PosSlot + ((len < kNumLenToPosStates ? len : kNumLenToPosStates - 1) << kNumPosSlotBits);
				TREE_6_DECODE(prob, distance);
				if (distance >= kStartPosModelIndex) {
					unsigned posSlot = (unsigned)distance;
					int numDirectBits = (int)(((distance >> 1) - 1));
					distance = (2 | (distance & 1));
					if (posSlot < kEndPosModelIndex) {
						distance <<= numDirectBits;
						prob = probs + SpecPos + distance - posSlot - 1;
						uint32 mask = 1;
						unsigned i = 1;
						do {
							GET_BIT2(prob + i, i,; , distance |= mask);
							mask <<= 1;
						} while (--numDirectBits != 0);
					} else {
						numDirectBits -= kNumAlignBits;
						do {
							NORMALIZE
							range >>= 1;

							uint32 t;
							code -= range;
							t = (0 - ((uint32)code >> 31)); // (uint32)((Int32)code >> 31)
							distance = (distance << 1) + (t + 1);
							code += range & t;
							//distance <<= 1;
							//if (code >= range)
							//{
							//	code -= range;
							//	distance |= 1;
							//}
						} while (--numDirectBits != 0);

						prob = probs + Align;
						distance <<= kNumAlignBits;
						unsigned i = 1;
						GET_BIT2(prob + i, i,; , distance |= 1);
						GET_BIT2(prob + i, i,; , distance |= 2);
						GET_BIT2(prob + i, i,; , distance |= 4);
						GET_BIT2(prob + i, i,; , distance |= 8);
						if (distance == (uint32)0xFFFFFFFF) {
							len += kMatchSpecLenStart;
							state -= kNumStates;
							break;
						}
					}
				}
				rep3 = rep2;
				rep2 = rep1;
				rep1 = rep0;
				rep0 = distance + 1;
				if (checkDicSize == 0) {
					if (distance >= processedPos)
						return SZ_ERROR_DATA;
				} else if (distance >= checkDicSize)
					return SZ_ERROR_DATA;
				state = (state < kNumStates + kNumLitStates) ? kNumLitStates : kNumLitStates + 3;
			}

			len += kMatchMinLen;

			if (limit == pos)
				return SZ_ERROR_DATA;

			size_t rem = limit - pos;
			unsigned curLen = ((rem < len) ? (unsigned)rem : len);
			size_t pos2 = (pos - rep0) + ((pos < rep0) ? dicBufSize : 0);

			processedPos += curLen;

			len -= curLen;
			if (pos2 + curLen <= dicBufSize) {
				uint8 *dest = dic + pos;
				ptrdiff_t src = (ptrdiff_t)pos2 - (ptrdiff_t)pos;
				const uint8 *lim = dest + curLen;
				pos += curLen;
				do
					*(dest) = (uint8)*(dest + src);
				while (++dest != lim);
			} else {
				do {
					dic[pos++] = dic[pos2];
					if (++pos2 == dicBufSize)
						pos2 = 0;
				} while (--curLen != 0);
			}
		}
	} while (pos < limit && buf < bufLimit);

	NORMALIZE;
	reps[0]				= rep0;
	reps[1]				= rep1;
	reps[2]				= rep2;
	reps[3]				= rep3;
	this->buf			= buf;
	this->range			= range;
	this->code			= code;
	this->remainLen		= len;
	this->dicPos		= pos;
	this->processedPos	= processedPos;
	this->state			= state;

	return SZ_OK;
}

void CLzmaDec::WriteRem(size_t limit) {
	if (remainLen != 0 && remainLen < kMatchSpecLenStart) {
		size_t	pos		= dicPos;
		auto	len		= min(remainLen, limit - pos)
		if (checkDicSize == 0 && prop.dicSize - processedPos <= len)
			checkDicSize = prop.dicSize;

		processedPos	+= len;
		remainLen		-= len;

		uint32	rep0		= reps[0];
		uint8	*dic		= this->dic;
		size_t	dicBufSize	= this->dicBufSize;
		while (len-- != 0) {
			dic[pos] = dic[(pos - rep0) + ((pos < rep0) ? dicBufSize : 0)];
			pos++;
		}
		dicPos = pos;
	}
}

int CLzmaDec::DecodeReal2(size_t limit, const uint8 *bufLimit) {
	do {
		size_t limit2 = limit;
		if (checkDicSize == 0) {
			uint32 rem = prop.dicSize - processedPos;
			if (limit - dicPos > rem)
				limit2 = dicPos + rem;
		}
		if (int r = DecodeReal(limit2, bufLimit))
			return r;
		if (processedPos >= prop.dicSize)
			checkDicSize = prop.dicSize;
		WriteRem(limit);

	} while (dicPos < limit && buf < bufLimit && remainLen < kMatchSpecLenStart);

	if (remainLen > kMatchSpecLenStart)
		remainLen = kMatchSpecLenStart;
	return 0;
}

enum ELzmaDummy {
	DUMMY_ERROR, // unexpected end of input stream
	DUMMY_LIT,
	DUMMY_MATCH,
	DUMMY_REP
};

static ELzmaDummy LzmaDec_TryDummy(const CLzmaDec *p, const uint8 *buf, size_t inSize) {
	uint32		range = p->range;
	uint32		code = p->code;
	const uint8 *bufLimit = buf + inSize;
	CLzmaProb	*probs = p->probs;
	unsigned	state = p->state;
	ELzmaDummy	res;

	uint32		bound;
	unsigned	ttt;
	unsigned	posState	= (p->processedPos) & ((1 << p->prop.pb) - 1);
	CLzmaProb	*prob		= probs + IsMatch + (state << kNumPosBitsMax) + posState;

	IF_BIT_0_CHECK(prob) {
		UPDATE_0_CHECK
		// if (bufLimit - buf >= 7) return DUMMY_LIT;
		prob = probs + Literal;
		if (p->checkDicSize != 0 || p->processedPos != 0)
			prob += (LZMA_LIT_SIZE *
				((((p->processedPos) & ((1 << (p->prop.lp)) - 1)) << p->prop.lc) +
				(p->dic[(p->dicPos == 0 ? p->dicBufSize : p->dicPos) - 1] >> (8 - p->prop.lc))));

		if (state < kNumLitStates) {
			unsigned symbol = 1;
			do {
				GET_BIT_CHECK(prob + symbol, symbol)
			} while (symbol < 0x100);
		} else {
			unsigned matchByte = p->dic[p->dicPos - p->reps[0] + ((p->dicPos < p->reps[0]) ? p->dicBufSize : 0)];
			unsigned offs = 0x100;
			unsigned symbol = 1;
			do {
				unsigned bit;
				CLzmaProb *probLit;
				matchByte <<= 1;
				bit = (matchByte & offs);
				probLit = prob + offs + bit + symbol;
				GET_BIT2_CHECK(probLit, symbol, offs &= ~bit, offs &= bit)
			} while (symbol < 0x100);
		}
		res = DUMMY_LIT;
	} else {
		unsigned len;
		UPDATE_1_CHECK;

		prob = probs + IsRep + state;
		IF_BIT_0_CHECK(prob) {
			UPDATE_0_CHECK;
			state	= 0;
			prob	= probs + LenCoder;
			res		= DUMMY_MATCH;
		} else {
			UPDATE_1_CHECK;
			res		= DUMMY_REP;
			prob	= probs + IsRepG0 + state;
			IF_BIT_0_CHECK(prob) {
				UPDATE_0_CHECK;
				prob = probs + IsRep0Long + (state << kNumPosBitsMax) + posState;
				IF_BIT_0_CHECK(prob) {
					UPDATE_0_CHECK;
					NORMALIZE_CHECK;
					return DUMMY_REP;
				} else {
					UPDATE_1_CHECK;
				}
			} else {
				UPDATE_1_CHECK;
				prob = probs + IsRepG1 + state;
				IF_BIT_0_CHECK(prob) {
					UPDATE_0_CHECK;
				} else {
					UPDATE_1_CHECK;
					prob = probs + IsRepG2 + state;
					IF_BIT_0_CHECK(prob) {
						UPDATE_0_CHECK;
					} else {
						UPDATE_1_CHECK;
					}
				}
			}
			state = kNumStates;
			prob = probs + RepLenCoder;
		}
		unsigned limit, offset;
		CLzmaProb *probLen = prob + LenChoice;
		IF_BIT_0_CHECK(probLen) {
			UPDATE_0_CHECK;
			probLen = prob + LenLow + (posState << kLenNumLowBits);
			offset = 0;
			limit = 1 << kLenNumLowBits;
		} else {
			UPDATE_1_CHECK;
			probLen = prob + LenChoice2;
			IF_BIT_0_CHECK(probLen) {
				UPDATE_0_CHECK;
				probLen = prob + LenMid + (posState << kLenNumMidBits);
				offset = kLenNumLowSymbols;
				limit = 1 << kLenNumMidBits;
			} else {
				UPDATE_1_CHECK;
				probLen = prob + LenHigh;
				offset = kLenNumLowSymbols + kLenNumMidSymbols;
				limit = 1 << kLenNumHighBits;
			}
		}
		TREE_DECODE_CHECK(probLen, limit, len);
		len += offset;

		if (state < 4) {
			unsigned posSlot;
			prob = probs + PosSlot + ((len < kNumLenToPosStates ? len : kNumLenToPosStates - 1) << kNumPosSlotBits);
			TREE_DECODE_CHECK(prob, 1 << kNumPosSlotBits, posSlot);
			if (posSlot >= kStartPosModelIndex) {
				int numDirectBits = ((posSlot >> 1) - 1);

				// if (bufLimit - buf >= 8) return DUMMY_MATCH;

				if (posSlot < kEndPosModelIndex) {
					prob = probs + SpecPos + ((2 | (posSlot & 1)) << numDirectBits) - posSlot - 1;
				} else {
					numDirectBits -= kNumAlignBits;
					do {
						NORMALIZE_CHECK
						range >>= 1;
						code -= range & (((code - range) >> 31) - 1);
						// if (code >= range) code -= range;
					} while (--numDirectBits != 0);
					prob = probs + Align;
					numDirectBits = kNumAlignBits;
				}
				unsigned i = 1;
				do {
					GET_BIT_CHECK(prob + i, i);
				} while (--numDirectBits != 0);
			}
		}
	}
	NORMALIZE_CHECK;
	return res;
}

void CLzmaDec::InitRc(const uint8 *data) {
	code		= ((uint32)data[1] << 24) | ((uint32)data[2] << 16) | ((uint32)data[3] << 8) | ((uint32)data[4]);
	range		= 0xFFFFFFFF;
	needFlush	= false;
}

void CLzmaDec::InitDicAndState(bool initDic, bool initState) {
	needFlush = true;
	remainLen = 0;
	tempBufSize = 0;

	if (initDic) {
		processedPos = 0;
		checkDicSize = 0;
		needInitState = true;
	}
	if (initState)
		needInitState = true;
}

void CLzmaDec::InitStateReal() {
	CLzmaProb	*probs = this->probs;
	for (uint32 i = 0, n = Literal + ((uint32)LZMA_LIT_SIZE << (prop.lc + prop.lp)); i < n; i++)
		probs[i] = kBitModelTotal >> 1;
	reps[0]			= reps[1] = reps[2] = reps[3] = 1;
	state			= 0;
	needInitState	= false;
}

int CLzmaDec::DecodeToDic(size_t dicLimit, const uint8 *src, size_t *srcLen, FinishMode finishMode, Status *status) {
	size_t inSize = *srcLen;
	(*srcLen) = 0;
	WriteRem(dicLimit);

	*status = STATUS_NOT_SPECIFIED;

	while (remainLen != kMatchSpecLenStart) {
		int checkEndMarkNow;

		if (needFlush) {
			for (; inSize > 0 && tempBufSize < RC_INIT_SIZE; (*srcLen)++, inSize--)
				tempBuf[tempBufSize++] = *src++;
			if (tempBufSize < RC_INIT_SIZE) {
				*status = STATUS_NEEDS_MORE_INPUT;
				return SZ_OK;
			}
			if (tempBuf[0] != 0)
				return SZ_ERROR_DATA;

			InitRc(tempBuf);
			tempBufSize = 0;
		}

		checkEndMarkNow = 0;
		if (dicPos >= dicLimit) {
			if (remainLen == 0 && code == 0) {
				*status = STATUS_MAYBE_FINISHED_WITHOUT_MARK;
				return SZ_OK;
			}
			if (finishMode == FINISH_ANY) {
				*status = STATUS_NOT_FINISHED;
				return SZ_OK;
			}
			if (remainLen != 0) {
				*status = STATUS_NOT_FINISHED;
				return SZ_ERROR_DATA;
			}
			checkEndMarkNow = 1;
		}

		if (needInitState)
			InitStateReal(this);

		if (tempBufSize == 0) {
			size_t processed;
			const uint8 *bufLimit;
			if (inSize < REQUIRED_INPUT_MAX || checkEndMarkNow) {
				int dummyRes = LzmaDec_TryDummy(this, src, inSize);
				if (dummyRes == DUMMY_ERROR) {
					memcpy(tempBuf, src, inSize);
					tempBufSize = (unsigned)inSize;
					(*srcLen) += inSize;
					*status = STATUS_NEEDS_MORE_INPUT;
					return SZ_OK;
				}
				if (checkEndMarkNow && dummyRes != DUMMY_MATCH) {
					*status = STATUS_NOT_FINISHED;
					return SZ_ERROR_DATA;
				}
				bufLimit = src;
			} else {
				bufLimit = src + inSize - REQUIRED_INPUT_MAX;
			}
			buf = src;
			if (DecodeReal2(dicLimit, bufLimit) != 0)
				return SZ_ERROR_DATA;
			processed = (size_t)(buf - src);
			(*srcLen) += processed;
			src += processed;
			inSize -= processed;
		} else {
			unsigned rem = tempBufSize, lookAhead = 0;
			while (rem < REQUIRED_INPUT_MAX && lookAhead < inSize)
				tempBuf[rem++] = src[lookAhead++];
			tempBufSize = rem;
			if (rem < REQUIRED_INPUT_MAX || checkEndMarkNow) {
				int dummyRes = LzmaDec_TryDummy(this, tempBuf, rem);
				if (dummyRes == DUMMY_ERROR) {
					(*srcLen) += lookAhead;
					*status = STATUS_NEEDS_MORE_INPUT;
					return SZ_OK;
				}
				if (checkEndMarkNow && dummyRes != DUMMY_MATCH) {
					*status = STATUS_NOT_FINISHED;
					return SZ_ERROR_DATA;
				}
			}
			buf = tempBuf;
			if (DecodeReal2(dicLimit, buf) != 0)
				return SZ_ERROR_DATA;
			lookAhead -= (rem - (unsigned)(buf - tempBuf));
			(*srcLen) += lookAhead;
			src += lookAhead;
			inSize -= lookAhead;
			tempBufSize = 0;
		}
	}
	if (code == 0)
		*status = STATUS_FINISHED_WITH_MARK;
	return (code == 0) ? SZ_OK : SZ_ERROR_DATA;
}

int CLzmaDec::DecodeToBuf(uint8 *dest, size_t *destLen, const uint8 *src, size_t *srcLen, FinishMode finishMode, Status *status) {
	size_t	outSize	= *destLen;
	size_t	inSize	= *srcLen;
	*srcLen = *destLen = 0;
	for (;;) {
		size_t inSizeCur = inSize, outSizeCur, pos;
		FinishMode curFinishMode;
		int res;
		if (dicPos == dicBufSize)
			dicPos = 0;
		pos = dicPos;
		if (outSize > dicBufSize - pos) {
			outSizeCur = dicBufSize;
			curFinishMode = FINISH_ANY;
		} else {
			outSizeCur = pos + outSize;
			curFinishMode = finishMode;
		}

		res = DecodeToDic(outSizeCur, src, &inSizeCur, curFinishMode, status);
		src += inSizeCur;
		inSize -= inSizeCur;
		*srcLen += inSizeCur;
		outSizeCur = dicPos - pos;
		memcpy(dest, dic + pos, outSizeCur);
		dest += outSizeCur;
		outSize -= outSizeCur;
		*destLen += outSizeCur;
		if (res != 0)
			return res;
		if (outSizeCur == 0 || outSize == 0)
			return SZ_OK;
	}
}

void CLzmaDec::FreeProbs(ISzAlloc *alloc) {
	alloc->Free(alloc, probs);
	probs = 0;
}

void CLzmaDec::FreeDict(ISzAlloc *alloc) {
	alloc->Free(alloc, dic);
	dic = 0;
}

void CLzmaDec::Free(ISzAlloc *alloc) {
	FreeProbs(alloc);
	FreeDict(alloc);
}

int CLzmaProps::Decode(const uint8 *data, unsigned size) {
	if (size < LZMA_PROPS_SIZE)
		return SZ_ERROR_UNSUPPORTED;

	dicSize = max(data[1] | ((uint32)data[2] << 8) | ((uint32)data[3] << 16) | ((uint32)data[4] << 24), LZMA_DIC_MIN);

	uint8	d = data[0];
	if (d >= (9 * 5 * 5))
		return SZ_ERROR_UNSUPPORTED;

	lc = d % 9;
	d /= 9;
	pb = d / 5;
	lp = d % 5;

	return SZ_OK;
}

int CLzmaDec::AllocateProbs2(const CLzmaProps *propNew, ISzAlloc *alloc) {
	uint32 n = LzmaProps_GetNumProbs(propNew);
	if (probs == 0 || n != numProbs) {
		FreeProbs(alloc);
		probs		= (CLzmaProb*)alloc->Alloc(alloc, n * sizeof(CLzmaProb));
		numProbs	= n;
		if (probs == 0)
			return SZ_ERROR_MEM;
	}
	return SZ_OK;
}

int CLzmaDec::AllocateProbs(const uint8 *props, unsigned propsSize, ISzAlloc *alloc) {
	CLzmaProps propNew;
	int			r;
	if (!(r = propNew.Decode(props, propsSize))
	&&	!(r = AllocateProbs2(&propNew, alloc))
	) prop = propNew;
	return r;
}

int CLzmaDec::Allocate(const uint8 *props, unsigned propsSize, ISzAlloc *alloc) {
	CLzmaProps propNew;
	int			r;
	if (!(r = propNew.Decode(props, propsSize))
	&&	!(r = AllocateProbs2(&propNew, alloc))
	) {
		size_t		size = propNew.dicSize;
		if (dic == 0 || size != dicBufSize) {
			FreeDict(alloc);
			dic = (uint8*)alloc->Alloc(alloc, size);
			if (dic == 0) {
				FreeProbs(alloc);
				return SZ_ERROR_MEM;
			}
		}
		dicBufSize = size;
		prop = propNew;
	}
	return r;
}

int CLzmaDec::Decode(
	uint8 *dest, size_t *destLen, const uint8 *src, size_t *srcLen,
	const uint8 *propData, unsigned propSize, FinishMode finishMode,
	Status *status, ISzAlloc *alloc
) {
	CLzmaDec p;
	size_t	inSize = *srcLen;
	size_t	outSize = *destLen;
	*srcLen = *destLen = 0;
	if (inSize < RC_INIT_SIZE)
		return SZ_ERROR_INPUT_EOF;

	int		res = p.AllocateProbs(propData, propSize, alloc);
	if (res != 0)
		return res;

	p.dic			= dest;
	p.dicBufSize	= outSize;

	p.InitDicAndState(true, true);

	*srcLen = inSize;
	res		= p.DecodeToDic(outSize, src, srcLen, finishMode, status);

	if (res == SZ_OK && *status == STATUS_NEEDS_MORE_INPUT)
		res = SZ_ERROR_INPUT_EOF;

	(*destLen) = p.dicPos;
	p.FreeProbs(alloc);
	return res;
}

//
//00000000	-	EOS
//00000001 U U	-	Uncompressed Reset Dic
//00000010 U U	-	Uncompressed No Reset
//100uuuuu U U P P	-	LZMA no reset
//101uuuuu U U P P	-	LZMA reset state
//110uuuuu U U P P S	-	LZMA reset state + new prop
//111uuuuu U U P P S	-	LZMA reset state + new prop + reset dic
//
//	u, U - Unpack Size
//	P - Pack Size
//	S - Props


#ifdef SHOW_DEBUG_INFO
#define PRF(x) x
#else
#define PRF(x)
#endif

static int Lzma2Dec_GetOldProps(uint8 prop, uint8 *props) {
	uint32 dicSize;
	if (prop > 40)
		return SZ_ERROR_UNSUPPORTED;
	dicSize	= prop == 40 ? 0xFFFFFFFF : CLzma2Dec::dic_size_from_prop(prop);
	props[0] = (uint8)CLzma2Dec::LCLP_MAX;
	props[1] = (uint8)(dicSize);
	props[2] = (uint8)(dicSize >> 8);
	props[3] = (uint8)(dicSize >> 16);
	props[4] = (uint8)(dicSize >> 24);
	return SZ_OK;
}

int CLzma2Dec::AllocateProbs(uint8 prop, ISzAlloc *alloc) {
	uint8	props[LZMA_PROPS_SIZE];
	int		r = Lzma2Dec_GetOldProps(prop, props);
	return r ? r : CLzmaDec::AllocateProbs(props, LZMA_PROPS_SIZE, alloc);
}

int CLzma2Dec::Allocate(uint8 prop, ISzAlloc *alloc) {
	uint8	props[LZMA_PROPS_SIZE];
	int		r = Lzma2Dec_GetOldProps(prop, props);
	return r ? r : CLzmaDec::Allocate(props, LZMA_PROPS_SIZE, alloc);
}

CLzma2Dec::State CLzma2Dec::UpdateState(uint8 b) {
	switch(state) {
		case STATE_CONTROL:
			control = b;
			PRF(printf("\n %4X ", dicPos));
			PRF(printf(" %2X", b));
			if (control == 0)
				return STATE_FINISHED;
			if (is_uncompressed_state()) {
				if ((control & 0x7F) > 2)
					return STATE_ERROR;
				unpackSize = 0;
			} else {
				unpackSize = (uint32)(control & 0x1F) << 16;
			}
			return STATE_UNPACK0;

		case STATE_UNPACK0:
			unpackSize |= (uint32)b << 8;
			return STATE_UNPACK1;

		case STATE_UNPACK1:
			unpackSize |= (uint32)b;
			unpackSize++;
			PRF(printf(" %8d", unpackSize));
			return is_uncompressed_state() ? STATE_DATA : STATE_PACK0;

		case STATE_PACK0:
			packSize = (uint32)b << 8;
			return STATE_PACK1;

		case STATE_PACK1:
			packSize |= (uint32)b;
			packSize++;
			PRF(printf(" %8d", packSize));
			return get_lzma_mode() >= 2 ? STATE_PROP : (needInitProp ? STATE_ERROR : STATE_DATA);

		case STATE_PROP: {
			if (b >= (9 * 5 * 5))
				return STATE_ERROR;
			int	lc = b % 9;
			b /= 9;
			prop.pb = b / 5;
			int	lp = b % 5;
			if (lc + lp > CLzma2Dec::LCLP_MAX)
				return STATE_ERROR;
			prop.lc = lc;
			prop.lp = lp;
			needInitProp = false;
			return STATE_DATA;
		}
	}
	return STATE_ERROR;
}

void CLzmaDec::UpdateWithUncompressed(const uint8 *src, size_t size) {
	memcpy(dic + dicPos, src, size);
	dicPos += size;
	if (checkDicSize == 0 && prop.dicSize - processedPos <= size)
		checkDicSize = p->prop.dicSize;
	processedPos += (uint32)size;
}

int CLzma2Dec::DecodeToDic(size_t dicLimit, const uint8 *src, size_t *srcLen, FinishMode finishMode, Status *status) {
	size_t inSize = *srcLen;
	*srcLen = 0;
	*status = STATUS_NOT_SPECIFIED;

	while (state != STATE_FINISHED) {
		size_t pos = dicPos;
		if (state == STATE_ERROR)
			return SZ_ERROR_DATA;

		if (pos == dicLimit && finishMode == FINISH_ANY) {
			*status = STATUS_NOT_FINISHED;
			return SZ_OK;
		}

		if (state != STATE_DATA && state != STATE_DATA_CONT) {
			if (*srcLen == inSize) {
				*status = STATUS_NEEDS_MORE_INPUT;
				return SZ_OK;
			}
			(*srcLen)++;
			state = UpdateState(*src++);
			continue;
		}
		size_t destSizeCur = dicLimit - pos;
		size_t srcSizeCur = inSize - *srcLen;
		FinishMode curFinishMode = FINISH_ANY;

		if (unpackSize <= destSizeCur) {
			destSizeCur = (size_t)unpackSize;
			curFinishMode = FINISH_END;
		}

		if (is_uncompressed_state()) {
			if (*srcLen == inSize) {
				*status = STATUS_NEEDS_MORE_INPUT;
				return SZ_OK;
			}

			if (state == STATE_DATA) {
				bool initDic = (control == CONTROL_COPY_RESET_DIC);
				if (initDic)
					needInitProp = needInitState = true;
				else if (needInitDic)
					return SZ_ERROR_DATA;
				needInitDic = false;
				InitDicAndState(initDic, false);
			}

			if (srcSizeCur > destSizeCur)
				srcSizeCur = destSizeCur;

			if (srcSizeCur == 0)
				return SZ_ERROR_DATA;

			UpdateWithUncompressed(src, srcSizeCur);

			src += srcSizeCur;
			*srcLen += srcSizeCur;
			unpackSize -= (uint32)srcSizeCur;
			state = (unpackSize == 0) ? STATE_CONTROL : STATE_DATA_CONT;

		} else {
			if (state == STATE_DATA) {
				int mode = get_lzma_mode();
				bool initDic = (mode == 3);
				bool initState = (mode > 0);
				if ((!initDic && needInitDic) || (!initState && needInitState))
					return SZ_ERROR_DATA;

				InitDicAndState(initDic, initState);
				needInitDic = false;
				needInitState = false;
				state = STATE_DATA_CONT;
			}
			if (srcSizeCur > packSize)
				srcSizeCur = (size_t)packSize;

			int res = DecodeToDic(pos + destSizeCur, src, &srcSizeCur, curFinishMode, status);

			src += srcSizeCur;
			*srcLen += srcSizeCur;
			packSize -= (uint32)srcSizeCur;

			size_t outSizeProcessed = dicPos - pos;
			unpackSize -= (uint32)outSizeProcessed;

			if (res || *status == STATUS_NEEDS_MORE_INPUT)
				return res;

			if (srcSizeCur == 0 && outSizeProcessed == 0) {
				if (*status != STATUS_MAYBE_FINISHED_WITHOUT_MARK || unpackSize != 0 || packSize != 0)
					return SZ_ERROR_DATA;
				state = STATE_CONTROL;
			}
			if (*status == STATUS_MAYBE_FINISHED_WITHOUT_MARK)
				*status = STATUS_NOT_FINISHED;
		}
	}
	*status = STATUS_FINISHED_WITH_MARK;
	return SZ_OK;
}

int CLzma2Dec::DecodeToBuf(uint8 *dest, size_t *destLen, const uint8 *src, size_t *srcLen, FinishMode finishMode, Status *status) {
	size_t outSize = *destLen, inSize = *srcLen;
	*srcLen = *destLen = 0;
	for (;;) {
		size_t		srcSizeCur = inSize, outSizeCur, pos;
		FinishMode	curFinishMode;
		if (dicPos == dicBufSize)
			dicPos = 0;
		pos = dicPos;
		if (outSize > dicBufSize - pos) {
			outSizeCur		= dicBufSize;
			curFinishMode	= FINISH_ANY;
		} else {
			outSizeCur		= pos + outSize;
			curFinishMode	= finishMode;
		}

		int res = DecodeToDic(outSizeCur, src, &srcSizeCur, curFinishMode, status);
		src			+= srcSizeCur;
		inSize		-= srcSizeCur;
		*srcLen		+= srcSizeCur;
		outSizeCur	= dicPos - pos;
		memcpy(dest, dic + pos, outSizeCur);
		dest		+= outSizeCur;
		outSize		-= outSizeCur;
		*destLen	+= outSizeCur;
		if (res != 0)
			return res;
		if (outSizeCur == 0 || outSize == 0)
			return SZ_OK;
	}
}

int CLzma2Dec::Decode(uint8 *dest, size_t *destLen, const uint8 *src, size_t *srcLen, uint8 prop, FinishMode finishMode, Status *status, ISzAlloc *alloc) {
	CLzma2Dec	decoder;
	size_t		outSize = *destLen, inSize = *srcLen;
	uint8		props[LZMA_PROPS_SIZE];

	*destLen	= *srcLen = 0;
	*status		= STATUS_NOT_SPECIFIED;
	decoder.dic = dest;
	decoder.dicBufSize = outSize;

	int		r;
	if (!(r = Lzma2Dec_GetOldProps(prop, props))
	&&	!(r = decoder.CLzmaDec::AllocateProbs(props, LZMA_PROPS_SIZE, alloc))
	) {
		*srcLen		= inSize;
		r = decoder.DecodeToDic(outSize, src, srcLen, finishMode, status);
		*destLen = decoder.dicPos;
		if (r == SZ_OK && *status == STATUS_NEEDS_MORE_INPUT)
			r = SZ_ERROR_INPUT_EOF;

		decoder.FreeProbs(alloc);
	}
	return r;
}
