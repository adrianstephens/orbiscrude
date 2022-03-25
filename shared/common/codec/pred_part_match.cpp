#include "pred_part_match.h"

using namespace iso;

uint8	ModelPPM::NS2BSIndx[256], ModelPPM::NS2Indx[256];

bool ModelPPM::InitTables() {
	NS2BSIndx[0] = 2 * 0;
	NS2BSIndx[1] = 2 * 1;

	memset(NS2BSIndx + 2, 2 * 2, 9);
	memset(NS2BSIndx + 11, 2 * 3, 256 - 11);

	for (int i = 0; i < 3; i++)
		NS2Indx[i] = i;

	for (int i = 4, m = i, k = 1, Step = 1; i < 256; i++) {
		NS2Indx[i] = m;
		if (!--k) {
			k = ++Step;
			m++;
		}
	}
	return true;
}

void ModelPPM::RestartModel() {
	clear(CharMask);
	RunLength	= InitRL	= -min(MaxOrder, 12) - 1;
	OrderFall	= MaxOrder;
	MinContext	= MaxContext = new CONTEXT;
	MinContext->NumStats	= 256;
	MinContext->SummFreq	= 257;
	FoundState	= MinContext->Stats = new STATE[256];
	PrevSuccess = 0;

	for (int i = 0; i < 256; i++) {
		FoundState[i].Symbol	= i;
		FoundState[i].Freq		= 1;
		FoundState[i].Successor	= NULL;
	}

	static const uint16 InitBinEsc[] = {0x3CDD, 0x1F3F, 0x59BF, 0x48F3, 0x64A1, 0x5ABC, 0x6632, 0x6051};

	for (int i = 0; i < 128; i++)
		for (int k = 0; k < 8; k++)
			for (int m = 0; m < 64; m += 8)
				BinSumm[i][k + m] = BIN_SCALE - InitBinEsc[k] / (i + 2);
}

void ModelPPM::StartModel(int _MaxOrder) {
	static bool	init_tables = InitTables();
	EscCount = 1;
	MaxOrder = _MaxOrder;
	RestartModel();
}

void ModelPPM::rescale(CONTEXT *c) {
	int		ns = c->NumStats;
	STATE	*p;
	for (p = FoundState; p != c->Stats; p--)
		swap(p[0], p[-1]);

	c->Stats->Freq	+= 4;
	c->SummFreq		+= 4;

	int		EscFreq	= c->SummFreq - p->Freq;
	int		Adder	= OrderFall != 0;
	c->SummFreq		= (p->Freq = (p->Freq + Adder) >> 1);

	for (int i = c->NumStats; --i;) {
		EscFreq -= (++p)->Freq;
		c->SummFreq += (p->Freq = (p->Freq + Adder) >> 1);
		if (p[0].Freq > p[-1].Freq) {
			STATE	*p1 =  p;
			STATE	tmp = *p1;
			do
				p1[0] = p1[-1];
			while (--p1 != c->Stats && tmp.Freq > p1[-1].Freq);
			*p1 = tmp;
		}
	}

	if (p->Freq == 0) {
		int	i = 0;
		do
			i++;
		while ((--p)->Freq == 0);

		EscFreq += i;

		if ((c->NumStats -= i) == 1) {
			STATE tmp = *c->Stats;
			do {
				tmp.Freq -= (tmp.Freq >> 1);
				EscFreq >>= 1;
			} while (EscFreq > 1);

			FreeUnits(c->Stats, (ns + 1) >> 1);
			*(FoundState = &c->OneState) = tmp;
			return;
		}
	}

	
	c->SummFreq	= EscFreq - (EscFreq >> 1);
	int n0 = (ns + 1) >> 1;
	int	n1 = (c->NumStats + 1) >> 1;
	if (n0 != n1)
		c->Stats = (STATE*)ShrinkUnits(c->Stats, n0, n1);
	FoundState = c->Stats;
}

ModelPPM::CONTEXT* ModelPPM::CreateSuccessors(bool Skip, STATE* p1) {
	STATE	UpState;
	CONTEXT *pc = MinContext, *UpBranch = FoundState->Successor;
	STATE	*p, *ps[MAX_O], **pps = ps;
	if (!Skip) {
		*pps++ = FoundState;
		if (!pc->Suffix)
			goto NO_LOOP;
	}

	if (p = p1) {
		pc = pc->Suffix;
		goto LOOP_ENTRY;
	}

	do {
		pc = pc->Suffix;
		if (pc->NumStats != 1) {
			p = pc->Stats;
			while (p->Symbol != FoundState->Symbol)
				p++;
		} else
			p = &(pc->OneState);
	LOOP_ENTRY:
		if (p->Successor != UpBranch) {
			pc = p->Successor;
			break;
		}
		// We ensure that PPM order input parameter does not exceed MAX_O (64), so we do not really need this check and added it for extra safety
		if (pps >= end(ps))
			return NULL;

		*pps++ = p;
	} while (pc->Suffix);

NO_LOOP:
	if (pps == ps)
		return pc;

	UpState.Symbol	  = *(uint8*)UpBranch;
	UpState.Successor = (CONTEXT*)(((uint8*)UpBranch) + 1);
	if (pc->NumStats != 1) {
		p = pc->Stats;
		while (p->Symbol != UpState.Symbol)
			p++;
		uint32 cf	 = p->Freq - 1;
		uint32 s0	 = pc->SummFreq - pc->NumStats - cf;
		UpState.Freq = 1 + ((2 * cf <= s0) ? (5 * cf > s0) : ((2 * cf + 3 * s0 - 1) / (2 * s0)));
	} else {
		UpState.Freq = pc->OneState.Freq;
	}

	do {
		pc = new CONTEXT(pc, UpState);
		(*--pps)->Successor = pc;
		if (!pc)
			return NULL;
	} while (pps != ps);

	return pc;
}

void ModelPPM::UpdateModel() {
	STATE	fs	= *FoundState, *p = NULL;

	if (fs.Freq < MAX_FREQ / 4) {
		if (CONTEXT *pc = MinContext->Suffix) {
			if (pc->NumStats != 1) {
				p = pc->Stats;
				while (p->Symbol != fs.Symbol)
					p++;

				if (p[0].Freq >= p[-1].Freq) {
					swap(p[0], p[-1]);
					p--;
				}

				if (p->Freq < MAX_FREQ - 9) {
					p->Freq			+= 2;
					pc->SummFreq	+= 2;
				}

			} else {
				p		= &pc->OneState;
				p->Freq += (p->Freq < 32);
			}
		}
	}

	if (!OrderFall) {
		MinContext = MaxContext = FoundState->Successor = CreateSuccessors(true, p);
		return;
	}

	*pText++	= fs.Symbol;
	if (pText >= end(scratch)) {
		RestartModel();
		EscCount = 0;
		return;
	}
	
	CONTEXT	*Successor;

	if (fs.Successor) {
		if (!--OrderFall) {
			Successor			= fs.Successor;
			pText				-= (MaxContext != MinContext);
		}
	} else {
		FoundState->Successor	= Successor = (CONTEXT*)pText;
		fs.Successor			= MinContext;
	}

	uint32	ns = MinContext->NumStats;
	uint32	s0 = MinContext->SummFreq - ns - (fs.Freq - 1);

	for (CONTEXT *pc = MaxContext; pc != MinContext; pc = pc->Suffix) {
		uint32	ns1 = pc->NumStats;
		if (ns1 != 1) {
			if (!(ns1 & 1))
				pc->Stats = (STATE*)ExpandUnits(pc->Stats, ns1 >> 1);

			pc->SummFreq += (2 * ns1 < ns) + 2 * ((4 * ns1 <= ns) & (pc->SummFreq <= 8 * ns1));

		} else {
			p	= (STATE*)AllocUnits(1);
			*p	= pc->OneState;
			pc->Stats = p;
			if (p->Freq < MAX_FREQ / 4 - 1)
				p->Freq += p->Freq;
			else
				p->Freq = MAX_FREQ - 4;
			pc->SummFreq = p->Freq + InitEsc + (ns > 3);
		}

		uint32	cf = 2 * fs.Freq * (pc->SummFreq + 6);
		uint32	sf = s0 + pc->SummFreq;
		if (cf < 6 * sf) {
			cf = 1 + (cf > sf) + (cf >= 4 * sf);
			pc->SummFreq += 3;
		} else {
			cf = 4 + (cf >= 9 * sf) + (cf >= 12 * sf) + (cf >= 15 * sf);
			pc->SummFreq += cf;
		}

		p			 = pc->Stats + ns1;
		p->Successor = Successor;
		p->Symbol	 = fs.Symbol;
		p->Freq		 = cf;
		pc->NumStats = ++ns1;
	}

	MaxContext = MinContext = fs.Successor;
	return;
}

inline int get_mean(uint16 SUMM, int SHIFT, int ROUND) { return (SUMM + (1 << (SHIFT - ROUND))) >> SHIFT; }

void ModelPPM::decodeBinSymbol(CONTEXT* c) {
	// Tabulated escapes for exponential symbol distribution
	static const uint8 ExpEscape[16] = {25, 14, 9, 7, 5, 5, 4, 4, 4, 3, 3, 3, 2, 2, 2, 2};

	STATE&	rs	= c->OneState;
	uint16& bs	= BinSumm[rs.Freq - 1][
		PrevSuccess
		+ NS2BSIndx[c->Suffix->NumStats - 1]
		+ (FoundState->Symbol >= 64	? 8		: 0)
		+ (rs.Symbol >= 64			? 16	: 0)
		+ ((RunLength >> 26) & 0x20)
	];

	if (Coder.GetCurrentShiftCount(TOT_BITS) < bs) {
		FoundState		= &rs;
		rs.Freq			+= (rs.Freq < 128);
		Coder.LowCount	= 0;
		Coder.HighCount	= bs;
		bs				= uint16(bs + INTERVAL - get_mean(bs, PERIOD_BITS, 2));
		PrevSuccess		= 1;
		++RunLength;

	} else {
		Coder.LowCount	= bs;
		Coder.HighCount	= BIN_SCALE;
		bs				= uint16(bs - get_mean(bs, PERIOD_BITS, 2));
		InitEsc			= ExpEscape[bs >> 10];
		CharMask[rs.Symbol]	= EscCount;
		NumMasked		= 1;
		PrevSuccess		= 0;
		FoundState		= NULL;
	}
}

void ModelPPM::encodeBinSymbol(CONTEXT* c, int symbol) {
	STATE&	rs	= c->OneState;
	uint16&	bs	= BinSumm[rs.Freq][
		PrevSuccess
			+ NS2BSIndx[c->Suffix->NumStats - 1]
			+ (FoundState->Symbol >= 64	? 8		: 0)
			+ (rs.Symbol >= 64			? 16	: 0)
			+ ((RunLength >> 26) & 0x20)
	];

	if (rs.Symbol == symbol) {
		FoundState	= &rs;
		rs.Freq		+= (rs.Freq < 128);
		Coder.LowCount	= 0;
		Coder.HighCount	= bs;
		bs				= uint16(bs + INTERVAL - get_mean(bs, PERIOD_BITS, 2));
		PrevSuccess = 1;
		++RunLength;

	} else {
		Coder.LowCount	= bs;
		Coder.HighCount	= BIN_SCALE;
		bs				= uint16(bs - get_mean(bs, PERIOD_BITS, 2));
		CharMask[rs.Symbol] = EscCount;
		NumMasked		= 1;
		PrevSuccess		= 0;
		FoundState		= NULL;
	}
}

bool ModelPPM::decodeSymbol1(CONTEXT* c) {
	Coder.scale			= c->SummFreq;
	STATE*	p			= c->Stats;
	int		count		= Coder.GetCurrentCount();

	if (count >= (int)Coder.scale)
		return false;

	int		HiCnt = p->Freq;
	if (count < HiCnt) {
		Coder.HighCount	= HiCnt;
		PrevSuccess		= 2 * HiCnt > Coder.scale;
		RunLength		+= PrevSuccess;
		FoundState		= p;
		c->SummFreq		+= 4;
		if ((p->Freq = HiCnt + 4) > MAX_FREQ)
			rescale(c);
		Coder.LowCount	= 0;
		return true;
	}

	if (!FoundState) 
		return false;

	PrevSuccess = 0;
	for (int i = c->NumStats; (HiCnt += (++p)->Freq) <= count;) {
		if (--i == 1) {
			Coder.HighCount		= Coder.scale;
			Coder.LowCount		= HiCnt;
			CharMask[p->Symbol]	= EscCount;
			NumMasked			= c->NumStats;
			FoundState			= NULL;
			for (int i = NumMasked; --i;)
				CharMask[(--p)->Symbol] = EscCount;
			return true;
		}
	}
	Coder.HighCount = HiCnt;
	Coder.LowCount	= HiCnt - p->Freq;
	update1(c, p);
	return true;
}

void ModelPPM::encodeSymbol1(CONTEXT* c, int symbol) {
	Coder.scale		= c->SummFreq;
	STATE*	p		= c->Stats;
	uint32	LoCnt	= p->Freq;

	if (p->Symbol == symbol) {
		Coder.HighCount = LoCnt;
		PrevSuccess		= (2 * LoCnt > Coder.scale);
		FoundState		= p;
		c->SummFreq		+= 4;
		if ((p->Freq = LoCnt + 4) >= MAX_FREQ)
			rescale(c);
		Coder.LowCount	= 0;
		return;
	}

	PrevSuccess = 0;
	for (uint32 i = c->NumStats; (++p)->Symbol != symbol;) {
		LoCnt += p->Freq;
		if (--i == 0) {
			Coder.HighCount		= Coder.scale;
			Coder.LowCount		= LoCnt;
			CharMask[p->Symbol] = EscCount;
			NumMasked			= c->NumStats;
			FoundState			= NULL;
			for (int i = NumMasked; --i;)
				CharMask[(--p)->Symbol] = EscCount;
			return;
		}
	}
	Coder.LowCount	= LoCnt;
	Coder.HighCount = LoCnt + p->Freq;
	update1(c, p);
}

bool ModelPPM::decodeSymbol2(CONTEXT* c) {
	STATE	*ps[256], **pps = ps;
	STATE	*p		= c->Stats;
	int		HiCnt	= 0;
	for (int i = c->NumStats - NumMasked; --i;) {
		while (CharMask[p->Symbol] == EscCount)
			p++;

		HiCnt += p->Freq;
		*pps++ = p++;
	}

	Coder.scale += HiCnt;
	int	count = Coder.GetCurrentCount();
	if (count >= (int)Coder.scale)
		return false;

	p = *(pps = ps);
	if (count < HiCnt) {
		HiCnt = 0;
		while ((HiCnt += p->Freq) <= count)
			p = *++pps;

		Coder.LowCount = (Coder.HighCount = HiCnt) - p->Freq;
		update2(c, p);

	} else {
		Coder.LowCount	= HiCnt;
		Coder.HighCount	= Coder.scale;
		for (int i = c->NumStats - NumMasked; --i;) {
			CharMask[(*pps)->Symbol] = EscCount;
			pps++;
		}

		NumMasked = c->NumStats;
	}
	return true;
}

void ModelPPM::encodeSymbol2(CONTEXT* c, int symbol) {
	STATE	*p		= c->Stats;
	uint32	LoCnt	= 0;
	for (uint32 i = c->NumStats - NumMasked; --i;) {
		while (CharMask[p->Symbol] == EscCount)
			p++;

		CharMask[p->Symbol] = EscCount;
		if (p->Symbol == symbol) {
			Coder.LowCount  = LoCnt;
			Coder.HighCount = (LoCnt += p->Freq);
			for (STATE *p1 = p + 1; --i;) {
				while (CharMask[p1->Symbol] == EscCount)
					p1++;
				LoCnt += p1->Freq;
			}
			Coder.scale += LoCnt;
			update2(c, p);
			return;
		}

		LoCnt += p->Freq;
	}

	Coder.LowCount	= LoCnt;
	Coder.HighCount = Coder.scale += LoCnt;
	NumMasked		= c->NumStats;
}