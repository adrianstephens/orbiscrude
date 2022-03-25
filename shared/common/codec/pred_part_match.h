#include "codec.h"

//-----------------------------------------------------------------------------
//	Prediction by partial matching
//-----------------------------------------------------------------------------

namespace iso {

struct RangeCoder {
	uint32 low, code, range;
	uint32 LowCount, HighCount, scale;

	void NormalizeDec(istream_ref file) {
		while ((low ^ (low + range)) < (1 << 24) || range < (1 << 15) && ((range = -(int)low & ((1 << 15) - 1)), 1)) {
			code	= (code << 8) | file.getc();
			range	<<= 8;
			low		<<= 8;
		}
	}

	int		GetCurrentCount() {
		range /= scale;
		return (code - low) / range;
	}
	uint32	GetCurrentShiftCount(uint32 SHIFT) {
		return (code - low) / (range >>= SHIFT);
	}
	void	Decode() {
		low		+= range * LowCount;
		range	*= HighCount - LowCount;
	}
	void	InitDecoder(istream_ref file) {
		code	= file.get<uint32be>();
		low		= code = 0;
		range	= uint32(-1);
	}

	void	InitEncoder() {
		low	= 0;
		range=uint32(-1);
	}

	void NormalizeEnc(ostream_ref file) {
		while ((low ^ (low+range)) < (1 << 24) || range < (1 << 15) && ((range= -low & ((1 << 15) - 1)), 1)) {
			file.putc(low >> 24);
			range	<<= 8;
			low		<<= 8;
		}
	}
	void Encode() {
		low		+= LowCount * (range /= scale);
		range	*= HighCount - LowCount;
	}
	void FlushEncoder(ostream_ref file) {
		file.write<uint32be>(low);
	}

	uint32 BinStart(uint32 f0, uint32 shift)  {
		return f0 * (range >>= shift);
	}
};

class ModelPPM {
	enum {
		INT_BITS	= 7,
		PERIOD_BITS	= 7,
		TOT_BITS	= INT_BITS + PERIOD_BITS,
		INTERVAL	= 1 << INT_BITS,
		BIN_SCALE	= 1 << TOT_BITS,
		MAX_FREQ	= 124,
		MAX_O		= 64,
	};

	struct CONTEXT;

	struct STATE {
		uint8		Symbol;
		uint8		Freq;
		CONTEXT*	Successor;
	};

	// Notes:
	// 1. NumStats & NumMasked contain number of symbols minus 1
	// 2. contexts example:
	// MaxOrder:
	//  ABCD    context
	//   BCD    suffix
	//   BCDE   successor
	// other orders:
	//   BCD    context
	//   CD     suffix
	//   BCDE   successor

	struct CONTEXT {
		union {
			struct {
				uint16	SummFreq;
				STATE*	Stats;
			};
			STATE		OneState;
		};
		uint16			NumStats;
		CONTEXT*		Suffix;

		CONTEXT() : NumStats(0), Suffix(0) {}
		CONTEXT(CONTEXT* Suffix, STATE& FirstState) : OneState(FirstState), NumStats(1), Suffix(Suffix) {}
	};

	void* AllocUnits(int NU)							{ return nullptr; }
	void* ExpandUnits(void* ptr,int OldNU)				{ return nullptr; }
	void* ShrinkUnits(void* ptr,int OldNU,int NewNU)	{ return nullptr; }
	void  FreeUnits(void* ptr,int OldNU)				{}

	uint8	scratch[64], *pText = scratch;
	CONTEXT *MinContext = 0, *MedContext = 0, *MaxContext = 0;
	STATE*	FoundState;						// found next state transition

	static uint8	NS2BSIndx[256], NS2Indx[256];

	int				MaxOrder, NumMasked, InitEsc, OrderFall, RunLength, InitRL;
	uint8			CharMask[256];
	uint8			EscCount, PrevSuccess;
	uint16			BinSumm[128][64];               // binary SEE-contexts
	RangeCoder		Coder;

	static bool		InitTables();

	CONTEXT*		CreateSuccessors(bool Skip, STATE* p1);
	void			UpdateModel();

	void	update1(CONTEXT* c, STATE* p) {
		(FoundState = p)->Freq += 4;
		c->SummFreq += 4;
		if (p[0].Freq > p[-1].Freq) {
			swap(p[0], p[-1]);
			FoundState = --p;
			if (p->Freq > MAX_FREQ)
				rescale(c);
		}
	}

	void	update2(CONTEXT* c, STATE* p) {
		(FoundState = p)->Freq += 4;
		c->SummFreq += 4;
		if (p->Freq > MAX_FREQ)
			rescale(c);
		EscCount++;
		RunLength = InitRL;
	}
	void	rescale(CONTEXT *c);

	void	decodeBinSymbol(CONTEXT *c);
	bool	decodeSymbol1(CONTEXT *c);
	bool	decodeSymbol2(CONTEXT *c);

	void	encodeBinSymbol(CONTEXT *c, int symbol);
	void	encodeSymbol1(CONTEXT *c, int symbol);
	void	encodeSymbol2(CONTEXT *c, int symbol);

	CONTEXT*createChild(CONTEXT *c, STATE* pStats, STATE& FirstState);

public:
	void	RestartModel();
	void	StartModel(int MaxOrder);

	bool	DecodeInit(istream_ref file) {
		Coder.InitDecoder(file);
		return !!MinContext;
	}

	int		DecodeChar(istream_ref file) {
		if (MinContext->NumStats != 1) {
			if (!decodeSymbol1(MinContext))
				return -1;
		} else {
			decodeBinSymbol(MinContext);
		}

		Coder.Decode();

		while (!FoundState) {
			Coder.NormalizeDec(file);
			do {
				OrderFall++;
				MinContext = MinContext->Suffix;
			} while (MinContext->NumStats == NumMasked);
			
			if (!decodeSymbol2(MinContext))
				return -1;
			Coder.Decode();
		}

		int Symbol = FoundState->Symbol;

		if (!OrderFall && (uint8*)FoundState->Successor > pText) {
			MinContext = MaxContext = FoundState->Successor;

		} else {
			UpdateModel();
			if (EscCount == 0) {
				clear(CharMask);
				EscCount = 1;
			}
		}
		Coder.NormalizeDec(file);
		return Symbol;
	}


	bool	EncodeChar(ostream_ref file, int c) {
		if (MinContext->NumStats != 1) {
			encodeSymbol1(MinContext, c);
			Coder.Encode();
		} else {
			encodeBinSymbol(MinContext, c);
		}
		while (!FoundState) {
			Coder.NormalizeEnc(file);
			do {
				if (!MinContext->Suffix )
					return false;	//error
				OrderFall++;
				MinContext=MinContext->Suffix;
			} while (MinContext->NumStats == NumMasked);

			encodeSymbol2(MinContext, c);
			Coder.Encode();
		}
		if (!OrderFall && (uint8*)FoundState->Successor > pText) {
			MaxContext = FoundState->Successor;

		} else {
			UpdateModel();
			if (EscCount == 0) {
				clear(CharMask);
				EscCount = 1;
			}
		}
		Coder.NormalizeEnc(file);
		MinContext = MaxContext;
		return true;
	}
};

} //namespace iso
