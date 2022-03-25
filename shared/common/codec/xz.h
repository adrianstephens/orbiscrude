#ifndef LZMA_H
#define LZMA_H

#include "base/defs.h"

namespace lzma {
using namespace iso;

enum errors {
	SZ_OK				= 0,
	SZ_ERROR_DATA		= 1,
	SZ_ERROR_MEM		= 2,
	SZ_ERROR_CRC		= 3,
	SZ_ERROR_UNSUPPORTED= 4,
	SZ_ERROR_PARAM		= 5,
	SZ_ERROR_INPUT_EOF	= 6,
	SZ_ERROR_OUTPUT_EOF	= 7,
	SZ_ERROR_READ		= 8,
	SZ_ERROR_WRITE		= 9,
	SZ_ERROR_PROGRESS	= 10,
	SZ_ERROR_FAIL		= 11,
	SZ_ERROR_THREAD		= 12,
	SZ_ERROR_ARCHIVE	= 16,
	SZ_ERROR_NO_ARCHIVE	= 17,
};

struct ISzAlloc {
	void *(*Alloc)(void *p, size_t size);
	void (*Free)(void *p, void *address); // address can be 0
};

#ifdef _LZMA_PROB32
#define CLzmaProb uint32
#else
#define CLzmaProb uint16
#endif

struct CLzmaProps {
	unsigned	lc, lp, pb;
	uint32		dicSize;
	int			Decode(const uint8 *data, unsigned size);
};

// ---------- LZMA Decoder state ----------

// There are two types of LZMA streams:
//	0) Stream with end mark. That end mark adds about 6 bytes to compressed size.
//	1) Stream without end mark. You must know exact uncompressed size

struct CLzmaDec {
	enum {
		// number of required input bytes for worst case: bits = log2((2^11 / 31) ^ 22) + 26 < 134 + 26 = 160
		REQUIRED_INPUT_MAX = 20,
	};

	// FinishMode has meaning only if the decoding reaches output limit !!!
	enum FinishMode {
		FINISH_ANY,	// finish at any point
		FINISH_END	// block must be finished at the end
	};

	// Status is used only as output value for function call
	enum Status {
		STATUS_NOT_SPECIFIED,				// use main error code instead
		STATUS_FINISHED_WITH_MARK,			// stream was finished with end mark.
		STATUS_NOT_FINISHED,				// stream was not finished
		STATUS_NEEDS_MORE_INPUT,			// you must provide more input bytes
		STATUS_MAYBE_FINISHED_WITHOUT_MARK	// there is probability that stream was finished without end mark
	};

	CLzmaProps	prop;
	CLzmaProb	*probs;
	uint8		*dic;
	const uint8	*buf;
	uint32		range,	code;
	size_t		dicPos;
	size_t		dicBufSize;
	uint32		processedPos;
	uint32		checkDicSize;
	unsigned	state;
	uint32		reps[4];
	unsigned	remainLen;
	bool		needFlush;
	bool		needInitState;
	uint32		numProbs;
	unsigned	tempBufSize;
	uint8		tempBuf[REQUIRED_INPUT_MAX];

	void		WriteRem(size_t limit);
	int			DecodeReal(size_t limit, const uint8 *bufLimit);
	int			DecodeReal2(size_t limit, const uint8 *bufLimit);
	void		InitStateReal();

	void		FreeProbs(ISzAlloc *alloc);
	void		FreeDict(ISzAlloc *alloc);
	void		Free(ISzAlloc *alloc);
	void		InitRc(const uint8 *data);

	int			AllocateProbs2(const CLzmaProps *propNew, ISzAlloc *alloc);
	int			AllocateProbs(const uint8 *props, unsigned propsSize, ISzAlloc *alloc);
	int			Allocate(const uint8 *props, unsigned propsSize, ISzAlloc *alloc);
	void		UpdateWithUncompressed(const uint8 *src, size_t size);

	void		InitDicAndState(bool initDic, bool initState);
	int			DecodeToDic(size_t dicLimit, const uint8 *src, size_t *srcLen, FinishMode finishMode, Status *status);
	int			DecodeToBuf(uint8 *dest, size_t *destLen, const uint8 *src, size_t *srcLen, FinishMode finishMode, Status *status);

	CLzmaDec() : dic(0), probs(0), dicPos(0) {}

	// ---------- One Call Interface ----------
	static int Decode(
		uint8 *dest, size_t *destLen, const uint8 *src, size_t *srcLen,
		const uint8 *propData, unsigned propSize, FinishMode finishMode,
		Status *status, ISzAlloc *alloc
	);
};


// ---------- State Interface ----------

struct CLzma2Dec : CLzmaDec {
	enum State {
		STATE_CONTROL,
		STATE_UNPACK0,
		STATE_UNPACK1,
		STATE_PACK0,
		STATE_PACK1,
		STATE_PROP,
		STATE_DATA,
		STATE_DATA_CONT,
		STATE_FINISHED,
		STATE_ERROR
	};
	enum Control {
		CONTROL_LZMA			= 1 << 7,
		CONTROL_COPY_NO_RESET	= 2,
		CONTROL_COPY_RESET_DIC	= 1,
		CONTROL_EOF				= 0,
		LCLP_MAX				= 4,
	};

	uint32	packSize;
	uint32	unpackSize;
	int		state;
	uint8	control;
	bool	needInitDic;
	bool	needInitState;
	bool	needInitProp;

	static int dic_size_from_prop(int p)	{ return ((p & 1) | 2) << (p / 2 + 11); }

	bool	is_uncompressed_state()	const	{ return control & CONTROL_LZMA; }
	int		get_lzma_mode()			const	{ return (control >> 5) & 3; }
	int		AllocateProbs(uint8 prop, ISzAlloc *alloc);
	int		Allocate(uint8 prop, ISzAlloc *alloc);

	State	UpdateState(uint8 b);
	int		DecodeToDic(size_t dicLimit, const uint8 *src, size_t *srcLen, FinishMode finishMode, Status *status);
	int		DecodeToBuf(uint8 *dest, size_t *destLen, const uint8 *src, size_t *srcLen, FinishMode finishMode, Status *status);

	CLzma2Dec() : state(STATE_CONTROL), needInitDic(true), needInitState(true), needInitProp(true) {
		InitDicAndState(true, true);
	}

	// ---------- One Call Interface ----------
	static int Decode(
		uint8 *dest, size_t *destLen, const uint8 *src, size_t *srcLen,
		uint8 prop, FinishMode finishMode, Status *status,
		ISzAlloc *alloc
	);

};

}// namespace lzma
#endif // LZMA_H
