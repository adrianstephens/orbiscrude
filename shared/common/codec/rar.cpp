#include "rar.h"
#include "base/bits.h"
#include "branch.h"
#include "utilities.h"

using namespace iso;
using namespace iso::rar;
//-----------------------------------------------------------------------------
// Filters
//-----------------------------------------------------------------------------

void FilterDelta(memory_block mem, uint8 *dst, uint32 Channels) {
	uint8	*src		= mem;
	uint32	SrcPos		= 0;
	uint32	DataSize	= mem.size32();
	for (uint32 CurChannel = 0; CurChannel < Channels; CurChannel++) {
		uint8  PrevByte = 0;
		for (uint32 DestPos = CurChannel; DestPos < DataSize; DestPos += Channels)
			dst[DestPos] = (PrevByte -= src[SrcPos++]);
	}
}

void FilterRGB(memory_block mem, uint8 *dst, uint32 Width, uint32 PosR) {
	uint32 DataSize	= mem.size32();
	uint8 *SrcData	= mem, *DestData = dst;
	for (uint32 CurChannel = 0; CurChannel < 3; CurChannel++) {
		uint32 PrevByte = 0;
		for (uint32 i = CurChannel; i < DataSize; i += 3) {
			uint32 Predicted;
			if (i >= Width + 3) {
				uint8* UpperData	 = DestData + i - Width;
				uint32 UpperByte	 = *UpperData;
				uint32 UpperLeftByte = UpperData[-3];
				Predicted			 = PrevByte + UpperByte - UpperLeftByte;
				int pa				 = abs(int(Predicted - PrevByte));
				int pb				 = abs(int(Predicted - UpperByte));
				int pc				 = abs(int(Predicted - UpperLeftByte));
				Predicted			 = pa <= pb && pa <= pc ? PrevByte : pb <= pc ? UpperByte : UpperLeftByte;
			} else {
				Predicted = PrevByte;
			}
			DestData[i] = PrevByte = uint8(Predicted - *SrcData++);
		}
	}
	for (uint32 i = PosR, Border = DataSize - 2; i < Border; i += 3) {
		uint8 G = DestData[i + 1];
		DestData[i] += G;
		DestData[i + 2] += G;
	}
}

void FilterAudio(memory_block mem, uint8 *dst, uint32 Channels) {
	uint32 DataSize	= mem.size32();
	uint8 *SrcData	= mem, *DestData = dst;
	for (uint32 CurChannel = 0; CurChannel < Channels; CurChannel++) {
		uint32 PrevByte = 0, PrevDelta = 0, Dif[7];
		int	   D1 = 0, D2 = 0;
		int	   K1 = 0, K2 = 0, K3 = 0;
		clear(Dif);

		for (uint32 i = CurChannel, ByteCount = 0; i < DataSize; i += Channels, ByteCount++) {
			int D3 = D2;
			D2	   = PrevDelta - D1;
			D1	   = PrevDelta;

			uint32 CurByte	 = *SrcData++;
			uint32 Predicted = (((8 * PrevByte + K1 * D1 + K2 * D2 + K3 * D3) >> 3) & 0xff) - CurByte;
			DestData[i]		 = Predicted;
			PrevDelta		 = int8(Predicted - PrevByte);
			PrevByte		 = Predicted;

			int D = ((int8)CurByte) << 3;
			Dif[0] += abs(D);
			Dif[1] += abs(D - D1);
			Dif[2] += abs(D + D1);
			Dif[3] += abs(D - D2);
			Dif[4] += abs(D + D2);
			Dif[5] += abs(D - D3);
			Dif[6] += abs(D + D3);

			if ((ByteCount & 0x1f) == 0) {
				uint32 MinDif = Dif[0], NumMinDif = 0;
				Dif[0] = 0;
				for (auto& j : Dif) {
					if (j < MinDif) {
						MinDif	  = j;
						NumMinDif = &j - Dif;
					}
					j = 0;
				}
				switch (NumMinDif) {
					case 1:	if (K1 >= -16)	--K1; break;
					case 2:	if (K1 < 16)	++K1; break;
					case 3:	if (K2 >= -16)	--K2; break;
					case 4:	if (K2 < 16)	++K2; break;
					case 5:	if (K3 >= -16)	--K3; break;
					case 6:	if (K3 < 16)	++K3; break;
				}
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Unpack
//-----------------------------------------------------------------------------

void DecodeTable::Init(uint8* LengthTable, uint32 Size, uint32 Quick) {
	// Size of alphabet and DecodePos array.
	MaxNum	  = Size;
	QuickBits = Quick;

	// Calculate how many entries for every bit length in LengthTable we have.
	uint32 LengthCount[16];
	memset(LengthCount, 0, sizeof(LengthCount));
	for (size_t i = 0; i < Size; i++)
		LengthCount[LengthTable[i] & 0xf]++;

	// We must not calculate the number of zero length codes.
	LengthCount[0] = 0;

	clear(DecodeNum);  // Set the entire DecodeNum to zero.
	DecodePos[0] = 0;  // Initialize not really used entry for zero length code.
	DecodeLen[0] = 0;  // Start code for bit length 1 is 0.

	// Right aligned upper limit code for current bit length.
	uint32 UpperLimit = 0;

	for (size_t i = 1; i < 16; i++) {
		UpperLimit += LengthCount[i];				  // Adjust the upper limit code.
		uint32 LeftAligned = UpperLimit << (16 - i);  // Left aligned upper limit code.
		UpperLimit *= 2;							  // Prepare the upper limit code for next bit length.
		DecodeLen[i] = (uint32)LeftAligned;			  // Store the left aligned upper limit code.
		DecodePos[i] = DecodePos[i - 1] + LengthCount[i - 1];
	}

	// Prepare the copy of DecodePos. We'll modify this copy below, so we cannot use the original DecodePos
	uint32 CopyDecodePos[16];
	memcpy(CopyDecodePos, DecodePos, sizeof(CopyDecodePos));

	// For every bit length in the bit length table and so for every item of alphabet.
	for (uint32 i = 0; i < Size; i++) {
		// Get the current bit length.
		uint8 CurBitLength = LengthTable[i] & 0xf;

		if (CurBitLength != 0) {
			// Last position in code list for current bit length.
			uint32 LastPos = CopyDecodePos[CurBitLength];
			// Prepare the decode table, so this position in code list will be decoded to current alphabet item number.
			DecodeNum[LastPos] = (uint16)i;
			// We'll use next position number for this bit length next time. So we pass through the entire range of positions available for every bit length.
			CopyDecodePos[CurBitLength]++;
		}
	}

	// Size of tables for quick mode.
	uint32 QuickDataSize = 1 << Quick;

	// Bit length for current code, start from 1 bit codes. It is important to use 1 bit instead of 0 for minimum code length, so we are moving forward even when processing a corrupt archive.
	uint32 CurBitLength = 1;

	// For every right aligned bit string which supports the quick decoding
	for (uint32 Code = 0; Code < QuickDataSize; Code++) {
		// Left align the current code, so it will be in usual bit field format.
		uint32 BitField = Code << (16 - Quick);

		// Prepare the table for quick decoding of bit lengths.
		// Find the upper limit for current bit field and adjust the bit length accordingly if necessary.
		while (CurBitLength < 16 && BitField >= DecodeLen[CurBitLength])
			CurBitLength++;

		// Translation of right aligned bit string to bit length.
		QuickLen[Code] = CurBitLength;

		// Prepare the table for quick translation of position in code list to position in alphabet.
		// Calculate the distance from the start code for current bit length.
		uint32 Dist = BitField - DecodeLen[CurBitLength - 1];

		// Right align the distance.
		Dist >>= 16 - CurBitLength;

		// Now we can calculate the position in the code list. It is the sum of first position for current bit length and right aligned distance between our bit field and start code for current bit length.
		uint32 Pos;
		if (CurBitLength < 16 && (Pos = DecodePos[CurBitLength] + Dist) < Size)
			QuickNum[Code] = DecodeNum[Pos];		// Define the code to alphabet number translation.
		else
			QuickNum[Code] = 0;						// Can be here for length table filled with zeroes only (empty).
	}
}

uint32 DecodeTable::DecodeNumber(BitInput &Inp) {
	// Left aligned 15 bit length raw bit field.
	uint32 BitField = Inp.peek(15) * 2;

	if (BitField < DecodeLen[QuickBits]) {
		uint32 Code = BitField >> (16 - QuickBits);
		Inp.discard(QuickLen[Code]);
		return QuickNum[Code];
	}

	uint32 Bits = 15;
	for (uint32 i = QuickBits + 1; i < 15; i++) {
		if (BitField < DecodeLen[i]) {
			Bits = i;
			break;
		}
	}

	Inp.discard(Bits);

	uint32 Dist = (BitField - DecodeLen[Bits-1]) >> (16 - Bits);
	uint32 Pos	= DecodePos[Bits] + Dist;
	return DecodeNum[Pos];
}


//-----------------------------------------------------------------------------
//	Unpack v 1.5
//-----------------------------------------------------------------------------

#define STARTL1 2
static uint32 DecL1[] = {0x8000, 0xa000, 0xc000, 0xd000, 0xe000, 0xea00, 0xee00, 0xf000, 0xf200, 0xf200, 0xffff};
static uint32 PosL1[] = {0, 0, 0, 2, 3, 5, 7, 11, 16, 20, 24, 32, 32};

#define STARTL2 3
static uint32 DecL2[] = {0xa000, 0xc000, 0xd000, 0xe000, 0xea00, 0xee00, 0xf000, 0xf200, 0xf240, 0xffff};
static uint32 PosL2[] = {0, 0, 0, 0, 5, 7, 9, 13, 18, 22, 26, 34, 36};

#define STARTHF0 4
static uint32 DecHf0[] = {0x8000, 0xc000, 0xe000, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xffff};
static uint32 PosHf0[] = {0, 0, 0, 0, 0, 8, 16, 24, 33, 33, 33, 33, 33};

#define STARTHF1 5
static uint32 DecHf1[] = {0x2000, 0xc000, 0xe000, 0xf000, 0xf200, 0xf200, 0xf7e0, 0xffff};
static uint32 PosHf1[] = {0, 0, 0, 0, 0, 0, 4, 44, 60, 76, 80, 80, 127};

#define STARTHF2 5
static uint32 DecHf2[] = {0x1000, 0x2400, 0x8000, 0xc000, 0xfa00, 0xffff, 0xffff, 0xffff};
static uint32 PosHf2[] = {0, 0, 0, 0, 0, 0, 2, 7, 53, 117, 233, 0, 0};

#define STARTHF3 6
static uint32 DecHf3[] = {0x800, 0x2400, 0xee00, 0xfe80, 0xffff, 0xffff, 0xffff};
static uint32 PosHf3[] = {0, 0, 0, 0, 0, 0, 0, 2, 16, 218, 251, 0, 0};

#define STARTHF4 8
static uint32 DecHf4[] = {0xff00, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff};
static uint32 PosHf4[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 255, 0, 0, 0};


void Unpack15::GetFlagsBuf(BitInput &Inp) {
	uint32 FlagsPlace = DecodeNum(Inp, STARTHF2, DecHf2, PosHf2);

	// Our Huffman table stores 257 items and needs all them in other parts of code such as when StMode is on, so the first item is control item
	// While normally we do not use the last item to code the flags byte here, we need to check for value 256 when unpacking in case we unpack a corrupt archive
	if (FlagsPlace >= 256)
		return;

	uint32 Flags, NewFlagsPlace;
	for (;;) {
		Flags		  = ChSetC[FlagsPlace];
		FlagBuf		  = Flags >> 8;
		NewFlagsPlace = NToPlC[Flags++ & 0xff]++;
		if ((Flags & 0xff) != 0)
			break;
		CorrHuff(ChSetC, NToPlC);
	}

	ChSetC[FlagsPlace]	  = ChSetC[NewFlagsPlace];
	ChSetC[NewFlagsPlace] = Flags;
}

void Unpack15::process(istream_ref file, bool Solid) {
	FlagsCnt = 0;
	FlagBuf	 = 0;
	LCount	 = 0;
	StMode	 = false;

	if (!Solid) {
		AvrPlcB		= AvrLn1 = AvrLn2 = AvrLn3 = NumHuf = Buf60 = 0;
		AvrPlc		= 0x3500;
		MaxDist3	= 0x2001;
		Nhfb		= Nlzb = 0x80;
		InitHuff();
		UnpPtr		= 0;
	} else {
		UnpPtr		= WrPtr;
	}

	BitInput	Inp(file);

	--DestUnpSize;
	if (DestUnpSize >= 0) {
		GetFlagsBuf(Inp);
		FlagsCnt = 8;
	}

	while (DestUnpSize >= 0) {
		UnpPtr &= WinMask;

		if (((WrPtr - UnpPtr) & WinMask) < 270 && WrPtr != UnpPtr)
			WriteBuf();

		if (StMode) {
			HuffDecode(Inp);

		} else {
			if (--FlagsCnt < 0) {
				GetFlagsBuf(Inp);
				FlagsCnt = 7;
			}

			if (FlagBuf & 0x80) {
				FlagBuf <<= 1;
				if (Nlzb > Nhfb)
					LongLZ(Inp);
				else
					HuffDecode(Inp);
			} else {
				FlagBuf <<= 1;
				if (--FlagsCnt < 0) {
					GetFlagsBuf(Inp);
					FlagsCnt = 7;
				}
				if (FlagBuf & 0x80) {
					FlagBuf <<= 1;
					if (Nlzb > Nhfb)
						HuffDecode(Inp);
					else
						LongLZ(Inp);
				} else {
					FlagBuf <<= 1;
					ShortLZ(Inp);
				}
			}
		}
	}
	WriteBuf();
}

void Unpack15::ShortLZ(BitInput &Inp) {
	NumHuf = 0;

	if (LCount == 2) {
		if (Inp.get(1)) {
			CopyString(LastDist, LastLength);
			return;
		}
		LCount = 0;
	}

	uint8	BitField = Inp.peek(8);
	uint32	Length;

	if (AvrLn1 < 37) {
		static const uint8 ShortLen1[] = {1, 3, 4, 4, 5, 6, 7, 8, 8, 4, 4, 5, 6, 6, 4, 0};
		static const uint8 ShortXor1[] = {0, 0xa0, 0xd0, 0xe0, 0xf0, 0xf8, 0xfc, 0xfe, 0xff, 0xc0, 0x80, 0x90, 0x98, 0x9c, 0xb0};
		for (Length = 0;; Length++) {
			uint8	sl = Length == 1 ? Buf60 + 3 : ShortLen1[Length];
			if (((BitField ^ ShortXor1[Length]) & (~(0xff >> sl))) == 0) {
				Inp.discard(sl);
				break;
			}
		}
	} else {
		static const uint8 ShortLen2[] = {2, 3, 3, 3, 4, 4, 5, 6, 6, 4, 4, 5, 6, 6, 4, 0};
		static const uint8 ShortXor2[] = {0, 0x40, 0x60, 0xa0, 0xd0, 0xe0, 0xf0, 0xf8, 0xfc, 0xc0, 0x80, 0x90, 0x98, 0x9c, 0xb0};
		for (Length = 0;; Length++) {
			uint8	sl = Length == 3 ? Buf60 + 3 : ShortLen2[Length];
			if (((BitField ^ ShortXor2[Length]) & (~(0xff >> sl))) == 0) {
				Inp.discard(sl);
				break;
			}
		}
	}

	if (Length < 9) {
		LCount = 0;
		AvrLn1 += Length;
		AvrLn1 -= AvrLn1 >> 4;

		int		DistancePlace = DecodeNum(Inp, STARTHF2, DecHf2, PosHf2) & 0xff;
		uint32	Distance	  = ChSetA[DistancePlace];
		if (DistancePlace)
			ChSetA[DistancePlace] = exchange(ChSetA[DistancePlace - 1], Distance);

		CopyStringSave(Distance + 1, Length + 2);

	} else if (Length == 9) {
		++LCount;
		CopyString(LastDist, LastLength);

	} else if (Length == 14) {
		LCount		= 0;
		LastLength	= DecodeNum(Inp, STARTL2, DecL2, PosL2) + 5;
		LastDist	= Inp.get(15) | 0x8000;
		CopyString(LastDist, LastLength);

	} else {
		LCount	   = 0;
		uint32	SaveLength = Length;
		uint32	Distance   = OldDist[(OldDistPtr - (Length - 9)) & 3];
		Length	   = DecodeNum(Inp, STARTL1, DecL1, PosL1) + 2;
		if (Length == 0x101 && SaveLength == 10) {
			Buf60 ^= 1;
			return;
		}
		if (Distance > 256)
			Length++;
		if (Distance >= MaxDist3)
			Length++;

		CopyStringSave(Distance, Length);
	}
}

void Unpack15::LongLZ(BitInput &Inp) {
	NumHuf	= 0;
	Nlzb	+= 16;
	if (Nlzb > 0xff) {
		Nlzb = 0x90;
		Nhfb >>= 1;
	}
	uint32	Length;
	if (AvrLn2 >= 122)
		Length = DecodeNum(Inp, STARTL2, DecL2, PosL2);
	else if (AvrLn2 >= 64)
		Length = DecodeNum(Inp, STARTL1, DecL1, PosL1);
	else {
		uint16	BitField = Inp.peek(16);
		if (BitField < 0x100) {
			Length = BitField;
			Inp.discard(16);
		} else {
			for (Length = 0; ((BitField << Length) & 0x8000) == 0; Length++)
				;
			Inp.discard(Length + 1);
		}
	}

	uint32	OldAvr2 = AvrLn2;
	AvrLn2 += Length;
	AvrLn2 -= AvrLn2 >> 5;

	uint32 DistancePlace
		=	AvrPlcB > 0x28ff ? DecodeNum(Inp, STARTHF2, DecHf2, PosHf2)
		:	AvrPlcB > 0x6ff ? DecodeNum(Inp, STARTHF1, DecHf1, PosHf1)
		:	DecodeNum(Inp, STARTHF0, DecHf0, PosHf0);

	AvrPlcB += DistancePlace;
	AvrPlcB -= AvrPlcB >> 8;
	uint32 Distance, NewDistancePlace;
	for (;;) {
		Distance		 = ChSetB[DistancePlace & 0xff];
		NewDistancePlace = NToPlB[Distance++ & 0xff]++;
		if (Distance & 0xff)
			break;
		CorrHuff(ChSetB, NToPlB);
	}

	ChSetB[DistancePlace & 0xff] = ChSetB[NewDistancePlace];
	ChSetB[NewDistancePlace]	 = Distance;

	Distance			= ((Distance & 0xff00) >> 1) + Inp.get(7);
	uint32	OldAvr3		= AvrLn3;
	if (Length != 1 && Length != 4) {
		if (Length == 0 && Distance <= MaxDist3) {
			AvrLn3++;
			AvrLn3 -= AvrLn3 >> 8;
		} else if (AvrLn3 > 0)
			AvrLn3--;
	}

	Length += 3;
	if (Distance >= MaxDist3)
		Length++;
	if (Distance <= 256)
		Length += 8;

	MaxDist3 = OldAvr3 > 0xb0 || AvrPlc >= 0x2a00 && OldAvr2 < 0x40 ? 0x7f00 : 0x2001;
	CopyStringSave(Distance, Length);
}

void Unpack15::HuffDecode(BitInput &Inp) {
	uint32	BitField = Inp.peek(16);

	int		BytePlace = 
		(	AvrPlc > 0x75ff	? DecodeNum(Inp, STARTHF4, DecHf4, PosHf4)
		:	AvrPlc > 0x5dff	? DecodeNum(Inp, STARTHF3, DecHf3, PosHf3)
		:	AvrPlc > 0x35ff ? DecodeNum(Inp, STARTHF2, DecHf2, PosHf2)
		:	AvrPlc > 0x0dff	? DecodeNum(Inp, STARTHF1, DecHf1, PosHf1)
		:	DecodeNum(Inp, STARTHF0, DecHf0, PosHf0)
		) & 0xff;

	if (StMode) {
		if (BytePlace == 0 && BitField > 0xfff)
			BytePlace = 0x100;

		if (BytePlace == 0) {
			if (Inp.get(1)) {
				NumHuf	= 0;
				StMode	= false;
			} else {
				uint32	len	= Inp.get(1) ? 4 : 3;
				uint32	d	= DecodeNum(Inp, STARTHF2, DecHf2, PosHf2);
				CopyString((d << 5) | Inp.get(5), len);
			}
			return;
		}
	} else {
		StMode = ++NumHuf > 16 && FlagsCnt == 0;
	}

	AvrPlc += --BytePlace;
	AvrPlc -= AvrPlc >> 8;
	Nhfb += 16;
	if (Nhfb > 0xff) {
		Nhfb = 0x90;
		Nlzb >>= 1;
	}

	Window[UnpPtr++] = (uint8)(ChSet[BytePlace] >> 8);
	--DestUnpSize;

	for (;;) {
		uint32	CurByte		 = ChSet[BytePlace];
		uint32	NewBytePlace = NToPl[CurByte++ & 0xff]++;
		if ((CurByte & 0xff) <= 0xa1) {
			ChSet[BytePlace]	= ChSet[NewBytePlace];
			ChSet[NewBytePlace] = CurByte;
			break;
		}
		CorrHuff(ChSet, NToPl);
	}
}

//-----------------------------------------------------------------------------
//	Unpack v 2.0
//-----------------------------------------------------------------------------

void Unpack20::process(istream_ref file, bool Solid) {
	static uint8  LDecode[]	 = {0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 12, 14, 16, 20, 24, 28, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224};
	static uint8  LBits[]	 = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5};
	static uint32 DDecode[]	 = {
		0,		1,		2,		3,		4,		6,		8,		12,		16,		24,		32,		48,		64,		96,		128,	192,
		256,	384,	512,	768,	1024,	1536,	2048,	3072,	4096,	6144,	8192,	12288,	16384,	24576,	32768U,	49152U,
		65536,	98304,	131072,	196608,	262144,	327680,	393216,	458752,	524288,	589824,	655360,	720896,	786432,	851968,	917504,	983040
	};
	static uint8  DBits[]	 = {
		0,  0,  0,  0,  1,   1,  2,  2,  3,  3,  4,  4,  5,  5,  6,  6,
		7,  7,  8,  8,  9,   9, 10, 10, 11, 11, 12, 12, 13, 13, 14, 14,
		15, 15, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16
	};
	static uint8  SDDecode[] = {0, 4, 8, 16, 32, 64, 128, 192};
	static uint8  SDBits[]	 = {2, 2, 3, 4, 5, 6, 6, 6};

	if (!Solid) {
		TablesRead2		= false;
		UnpAudioBlock	= false;
		UnpChannelDelta = 0;
		UnpCurChannel	= 0;
		UnpChannels		= 1;

		clear(AudV);
		clear(UnpOldTable20);
		clear(MD);
	}

	BitInput	Inp(file);

	if (!TablesRead2) {
		if (!ReadTables(Inp))
			return;
		TablesRead2 = true;
	}

	--DestUnpSize;

	while (DestUnpSize >= 0) {
		UnpPtr &= WinMask;

		if (((WrPtr - UnpPtr) & WinMask) < 270 && WrPtr != UnpPtr)
			WriteBuf();

		if (UnpAudioBlock) {
			uint32 AudioNumber = MD[UnpCurChannel].DecodeNumber(Inp);

			if (AudioNumber == 256) {
				if (!ReadTables(Inp))
					break;

			} else {
				Window[UnpPtr++]	= AudV[UnpCurChannel].Decode((int)AudioNumber, UnpChannelDelta);
				UnpChannelDelta		= AudV[UnpCurChannel].LastDelta;
				if (++UnpCurChannel == UnpChannels)
					UnpCurChannel = 0;
				--DestUnpSize;
			}
			continue;
		}

		uint32 Number = BlockTables.LD.DecodeNumber(Inp);
		if (Number < 256) {
			Window[UnpPtr++] = (uint8)Number;
			--DestUnpSize;

		} else if (Number >= 270) {
			uint32 Length = LDecode[Number -= 270] + 3;
			if (uint32 Bits = LBits[Number])
				Length += Inp.get(Bits);

			uint32 DistNumber = BlockTables.DD.DecodeNumber(Inp);
			uint32 Distance	  = DDecode[DistNumber] + 1;

			if (uint32 Bits = DBits[DistNumber])
				Distance += Inp.get(Bits);

			Length += uint32(Distance >= 0x2000) + uint32(Distance >= 0x40000);
			CopyString(Length, Distance);

		} else if (Number == 269) {
			if (!ReadTables(Inp))
				break;

		} else if (Number == 256) {
			CopyString(LastLength, LastDist);

		} else if (Number < 261) {
			uint32 Distance		= OldDist[(OldDistPtr - (Number - 256)) & 3];
			uint32 LengthNumber = BlockTables.RD.DecodeNumber(Inp);
			uint32 Length		= LDecode[LengthNumber] + 2;
			if (uint32 Bits = LBits[LengthNumber])
				Length += Inp.get(Bits);

			Length += uint32(Distance >= 0x101) + uint32(Distance >= 0x2000) + uint32(Distance >= 0x40000);
			CopyString(Length, Distance);

		} else {//if (Number < 270) {
			uint32 Distance = SDDecode[Number -= 261] + 1;
			if (uint32 Bits = SDBits[Number])
				Distance += Inp.get(Bits);
			CopyString(2, Distance);
		}
	}
	ReadLastTables(Inp);
	WriteBuf();
}

bool Unpack20::ReadTables(BitInput &Inp) {
	UnpAudioBlock	= Inp.get(1);

	if (!Inp.get(1))
		clear(UnpOldTable20);

	uint32 TableSize;
	if (UnpAudioBlock) {
		UnpChannels = Inp.get(2) + 1;
		if (UnpCurChannel >= UnpChannels)
			UnpCurChannel = 0;
		TableSize = MC20 * UnpChannels;
	} else {
		TableSize = NC20 + DC20 + RC20;
	}

	uint8 BitLength[BC20];
	uint8 Table[MC20 * 4];
	for (uint32 i = 0; i < BC20; i++)
		BitLength[i] = Inp.get(4);

	BlockTables.BD.Init(BitLength, BC20);
	for (uint32 i = 0; i < TableSize;) {
		uint32 Number = BlockTables.BD.DecodeNumber(Inp);
		if (Number < 16) {
			Table[i] = (Number + UnpOldTable20[i]) & 0xf;
			i++;

		} else if (Number == 16) {
			uint32 n = Inp.get(2) + 3;
			if (i == 0)
				return false;  // We cannot have "repeat previous" code at the first position.

			while (n-- > 0 && i < TableSize) {
				Table[i] = Table[i - 1];
				i++;
			}

		} else {
			uint32 n = Number == 17 ? Inp.get(3) + 3 : Inp.get(7) + 11;
			while (n-- > 0 && i < TableSize)
				Table[i++] = 0;
		}
	}
	if (UnpAudioBlock) {
		for (uint32 i = 0; i < UnpChannels; i++)
			MD[i].Init(&Table[i * MC20], MC20);

	} else {
		BlockTables.LD.Init(&Table[0], NC20);
		BlockTables.DD.Init(&Table[NC20], DC20);
		BlockTables.RD.Init(&Table[NC20 + DC20], RC20);
	}
	memcpy(UnpOldTable20, Table, TableSize);
	return true;
}

void Unpack20::ReadLastTables(BitInput &Inp) {
	if (UnpAudioBlock) {
		if (MD[UnpCurChannel].DecodeNumber(Inp) == 256)
			ReadTables(Inp);
	} else if (BlockTables.LD.DecodeNumber(Inp) == 269)
		ReadTables(Inp);
}

uint8 Unpack20::AudioVariables::Decode(int Delta, int ChannelDelta) {
	ByteCount++;
	D4			= D3;
	D3			= D2;
	D2			= LastDelta - D1;
	D1			= LastDelta;

	uint32	Ch	= (((8 * LastChar + K1 * D1 + K2 * D2 + K3 * D3 + K4 * D4 + K5 * ChannelDelta) >> 3) & 0xFF) - Delta;
	int		D	= (int8)Delta << 3;

	Dif[0]		+= abs(D);
	Dif[1]		+= abs(D - D1);
	Dif[2]		+= abs(D + D1);
	Dif[3]		+= abs(D - D2);
	Dif[4]		+= abs(D + D2);
	Dif[5]		+= abs(D - D3);
	Dif[6]		+= abs(D + D3);
	Dif[7]		+= abs(D - D4);
	Dif[8]		+= abs(D + D4);
	Dif[9]		+= abs(D - ChannelDelta);
	Dif[10]		+= abs(D + ChannelDelta);

	LastDelta	= int8(Ch - LastChar);
	LastChar	= Ch;

	if ((ByteCount & 0x1F) == 0) {
		uint32 MinDif	= Dif[0], NumMinDif = 0;
		Dif[0]			= 0;
		for (uint32 i = 1; i < num_elements(Dif); i++) {
			if (Dif[i] < MinDif) {
				MinDif	  = Dif[i];
				NumMinDif = i;
			}
			Dif[i] = 0;
		}
		switch (NumMinDif) {
			case 1:		if (K1 >= -16)	--K1; break;
			case 2:		if (K1 < 16)	++K1; break;
			case 3:		if (K2 >= -16)	--K2; break;
			case 4:		if (K2 < 16)	++K2; break;
			case 5:		if (K3 >= -16)	--K3; break;
			case 6:		if (K3 < 16)	++K3; break;
			case 7:		if (K4 >= -16)	--K4; break;
			case 8:		if (K4 < 16)	++K4; break;
			case 9:		if (K5 >= -16)	--K5; break;
			case 10:	if (K5 < 16)	++K5; break;
		}
	}
	return (uint8)Ch;
}


//-----------------------------------------------------------------------------
//	Unpack v 3.0
//-----------------------------------------------------------------------------

void Unpack30::process(istream_ref file, bool Solid) {
	static uint8	LDecode[]			= {0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 12, 14, 16, 20, 24, 28, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224};
	static uint8	LBits[]				= {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5};
	static int		DDecode[DC];
	static uint8	DBits[DC];
	static int		DBitLengthCounts[]	= {4, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 14, 0, 12};
	static uint8	SDDecode[]			= {0, 4, 8, 16, 32, 64, 128, 192};
	static uint8	SDBits[]			= {2, 2, 3, 4, 5, 6, 6, 6};

	if (DDecode[1] == 0) {
		int Dist = 0, BitLength = 0, Slot = 0;
		for (auto i : DBitLengthCounts) {
			for (int j = 0; j < i; j++, Slot++, Dist += (1 << BitLength)) {
				DDecode[Slot] = Dist;
				DBits[Slot]	  = BitLength;
			}
			++BitLength;
		}
	}

	if (!Solid) {
		TablesRead3	= false;
		clear(LZS.UnpOldTable);
		PPMEscChar	= 2;
		UsePPM		= false;
	}
	InitFilters(Solid);
	
	for (bool NewFile = false; !NewFile;) {
		BitInput	Inp(file);

		if ((!Solid || !TablesRead3)) {
			if (Inp.peek(1)) {
				UsePPM = true;

				int		FirstByte	= file.getc();
				int		MaxMB		= 0;
				int		MaxOrder	= 0;

				if (FirstByte & 0x20) {
					MaxMB		= file.getc();
					MaxOrder	= (FirstByte & 0x10) ? 16 + ((FirstByte & 15) + 1) * 3 : FirstByte & 15;
					//SubAlloc.StartSubAllocator(MaxMB + 1);
					PPM.StartModel(MaxOrder);
				}
				if (FirstByte & 0x40)
					PPMEscChar = file.getc();

				if (!PPM.DecodeInit(file))
					return;

			} else {
				UsePPM		= false;
				if (!LZS.ReadTables(Inp))
					return;
				TablesRead3	= true;
			}
		}

		if (UsePPM) {
			//PPM

			for (;;) {
				UnpPtr &= WinMask;

				if (((WrPtr - UnpPtr) & WinMask) < 260 && WrPtr != UnpPtr) {
					WriteBuf();
					if (WrittenFileSize > DestUnpSize)
						return;
				}

				int Ch = PPMDecodeChar(file);
				if (Ch == -1)					// Corrupt PPM data found
					break;

				if (Ch == PPMEscChar) {
					int NextCh = PPMDecodeChar(file);

					NewFile	= NextCh == 2;
					if (PPMFailed || NewFile || NextCh == 0) // Corrupt PPM data or End of PPM encoding or End of file in PPM mode
						break;

					if (NextCh == 3) {  // Read VM code
						uint32 FirstByte = PPMDecodeChar(file);
						uint32 Length = (FirstByte & 7) + 1;
						if (Length == 7) {
							Length = PPMDecodeChar(file) + 7;
						} else if (Length == 8) {
							int hi = PPMDecodeChar(file);
							Length = hi * 256 + PPMDecodeChar(file);
						}

						if (Length) {
							temp_block VMCode(Length);
							for (uint8 *p = VMCode; !PPMFailed && p < VMCode.end(); ++p)
								*p = PPMDecodeChar(file);
							if (!PPMFailed && AddVMCode(FirstByte, VMCode))
								continue;
						}
						break;
					}

					if (NextCh == 4) { // LZ inside of PPM
						uint32	Distance = 0, Length;
						for (int i = 0; i < 4; i++) {
							int Ch = PPMDecodeChar(file);
							if (i == 3)
								Length = (uint8)Ch;
							else
								Distance = (Distance << 8) + (uint8)Ch;
						}
						if (PPMFailed)
							break;

						CopyString(Length + 32, Distance + 2);
						continue;

					}
					if (NextCh == 5) { // One uint8 distance match (RLE) inside of PPM
						int Length = PPMDecodeChar(file);
						if (PPMFailed)
							break;

						CopyString(Length + 4, 1);
						continue;
					}
					// If we are here, NextCh must be 1, what means that current uint8 is equal to our 'escape' uint8, so we just store it to Window
				}
				Window[UnpPtr++] = Ch;
			}

		} else {
			//LZS

			for (;;) {
				UnpPtr &= WinMask;

				if (((WrPtr - UnpPtr) & WinMask) < 260 && WrPtr != UnpPtr) {
					WriteBuf();
					if (WrittenFileSize > DestUnpSize)
						return;
				}

				uint32 Number = LZS.LD.DecodeNumber(Inp);
				if (Number < 256) {
					Window[UnpPtr++] = (uint8)Number;

				} else if (Number >= 271) {
					uint32	Length	= LDecode[Number -= 271] + 3;
					if (uint32 Bits = LBits[Number])
						Length += Inp.get(Bits);

					uint32 DistNumber	= LZS.DD.DecodeNumber(Inp);
					uint32 Distance		= DDecode[DistNumber] + 1;

					if (uint32 Bits = DBits[DistNumber]) {
						if (DistNumber > 9) {
							if (Bits > 4)
								Distance += Inp.get(Bits - 4) << 4;

							if (LZS.LowDistRepCount > 0) {
								LZS.LowDistRepCount--;
								Distance += LZS.PrevLowDist;

							} else {
								uint32 LowDist = LZS.LDD.DecodeNumber(Inp);
								if (LowDist == 16) {
									LZS.LowDistRepCount	= LOW_DIST_REP_COUNT - 1;
									Distance		+= LZS.PrevLowDist;
								} else {
									Distance		+= LowDist;
									LZS.PrevLowDist	= LowDist;
								}
							}
						} else {
							Distance += Inp.get(Bits);
						}
					}

					if (Distance >= 0x2000) {
						Length++;
						if (Distance >= 0x40000)
							Length++;
					}

					InsertOldDist(Distance);
					LastLength = Length;
					CopyString(Length, Distance);

				} else if (Number == 256) {
					// "1"  - no new file, new table just here
					// "00" - new file,    no new table.
					// "01" - new file,    new table (in beginning of next file).
					NewFile		= !Inp.get(1);
					TablesRead3	= NewFile && !Inp.get(1);
					break;

				} else if (Number == 257) {
					uint32 FirstByte	= Inp.get(8);
					uint32 Length		= (FirstByte & 7) == 6 ? Inp.get(8) + 7
										: (FirstByte & 7) == 7 ? Inp.get(16)
										: (FirstByte & 7) + 1;

					if (Length) {
						temp_block VMCode(Length);
						for (uint8 *p = VMCode; p < VMCode.end(); ++p)
							*p = Inp.get(8);
						if (!AddVMCode(FirstByte, VMCode))
							break;
					}

				} else if (Number == 258) {
					if (LastLength != 0)
						CopyString(LastLength, OldDist[0]);

				} else if (Number < 263) {
					uint32 DistNum  = Number - 259;
					uint32 Distance = OldDist[DistNum];
					for (uint32 i = DistNum; i > 0; i--)
						OldDist[i] = OldDist[i - 1];
					OldDist[0] = Distance;

					uint32 LengthNumber = LZS.RD.DecodeNumber(Inp);
					int		Length		= LDecode[LengthNumber] + 2;
					if (uint32 Bits = LBits[LengthNumber])
						Length += Inp.get(Bits);
					LastLength = Length;
					CopyString(Length, Distance);

				} else {//if (Number < 272) {
					uint32	Distance	= SDDecode[Number -= 263] + 1;
					if (uint32 Bits = SDBits[Number])
						Distance += Inp.get(Bits);
					InsertOldDist(Distance);
					LastLength = 2;
					CopyString(2, Distance);
				}
			}
		}

		Inp.restore_unused();
		WriteBuf();
	}
}

bool Unpack30::AddVMCode(uint32 FirstByte, const_memory_block code) {
	VM::Reader	VMCodeInp(code);

	uint32	FiltPos;
	if (FirstByte & 0x80) {
		FiltPos = VMCodeInp.get32();
		if (FiltPos == 0)
			InitFilters(false);
		else
			FiltPos--;
	} else {
		FiltPos = LastFilter;  // Use the same filter as last time
	}

	if (FiltPos > Programs.size())
		return false;

	LastFilter	   = FiltPos;
	bool NewFilter = FiltPos == Programs.size();

	Program* prog;
	if (NewFilter) {
		// New filter code, never used before since VM reset.
		if (FiltPos > MAX_UNPACK_FILTERS)
			return false;

		Programs.push_back(prog = new Program);

	} else {
		// Filter was used in the past
		prog	= Programs[FiltPos];
	}

	uint32 EmptyCount = 0;
	for (uint32 i = 0; i < PrgStack.size(); i++) {
		PrgStack[i - EmptyCount] = PrgStack[i];
		if (!PrgStack[i])
			EmptyCount++;
		if (EmptyCount > 0)
			PrgStack[i] = NULL;
	}
	if (EmptyCount)
		PrgStack.resize(PrgStack.size() - EmptyCount);

	Filter* StackFilter = new Filter;  // New filter for PrgStack
	PrgStack.push_back(StackFilter);

	uint32 BlockStart = VMCodeInp.get32();
	if (FirstByte & 0x40)
		BlockStart += 258;

	StackFilter->BlockStart		= (BlockStart + UnpPtr) & WinMask;
	if (FirstByte & 0x20)
		prog->LastLength =  VMCodeInp.get32();

	// Set the data block size to same value as the previous block size for same filter
	StackFilter->BlockLength	= prog->LastLength;
	StackFilter->NextWindow		= WrPtr != UnpPtr && ((WrPtr - UnpPtr) & WinMask) <= BlockStart;

	clear(StackFilter->InitR);
	StackFilter->InitR[4] = StackFilter->BlockLength;

	if (FirstByte & 0x10) {
		// Set registers to optional parameters if any
		uint32	*R = StackFilter->InitR;
		for (uint32 InitMask = VMCodeInp.get(7); InitMask; InitMask >>= 1, ++R) {
			if (InitMask & 1)
				*R = VMCodeInp.get32();
		}
	}

	if (NewFilter) {
		uint32 VMCodeSize = VMCodeInp.get32();
		if (VMCodeSize >= 0x10000 || VMCodeSize == 0/* || VMCodeInp.InAddr + VMCodeSize > code.length()*/)
			return false;
		temp_block	VMCode(VMCodeSize);
		for (uint8 *p = VMCode; p < VMCode.end(); p++)
			*p = VMCodeInp.get(8);
		vm.Prepare(VMCode, prog);
	}
	StackFilter->Prg = prog;
	return true;
}

void Unpack30::WriteBuf() {
	uint32 WrittenBorder	= (uint32)WrPtr;
	uint32 WriteSize		= (uint32)((UnpPtr - WrittenBorder) & WinMask);

	for (size_t i = 0; i < PrgStack.size(); i++) {
		if (Filter* flt = PrgStack[i]) {
			if (flt->NextWindow) {
				flt->NextWindow = false;

			} else {
				uint32 BlockStart	= flt->BlockStart;
				uint32 BlockLength	= flt->BlockLength;

				if (((BlockStart - WrittenBorder) & WinMask) < WriteSize) {
					if (WrittenBorder != BlockStart) {
						WriteArea(WrittenBorder, BlockStart);
						WrittenBorder = BlockStart;
						WriteSize	  = (uint32)((UnpPtr - WrittenBorder) & WinMask);
					}
					if (BlockLength <= WriteSize) {
						uint32 BlockEnd = (BlockStart + BlockLength) & WinMask;
						if (BlockStart < BlockEnd || BlockEnd == 0) {
							vm.SetMemory(0, Window + BlockStart, BlockLength);
						} else {
							uint32 FirstPartLength = uint32(WinSize - BlockStart);
							vm.SetMemory(0, Window + BlockStart, FirstPartLength);
							vm.SetMemory(FirstPartLength, Window, BlockEnd);
						}

						flt->InitR[6]			= (uint32)WrittenFileSize;
						vm.Execute(flt);
						auto	FilteredData	= vm.GetMemory(vm.GetGlobal(vm.GLOBAL_NEWPOS)  & (vm.MEMSIZE - 1), vm.GetGlobal(vm.GLOBAL_NEWSIZE) & (vm.MEMSIZE - 1));

						delete PrgStack[i];
						PrgStack[i] = NULL;

						while (i + 1 < PrgStack.size()) {
							Filter* NextFilter = PrgStack[i + 1];
							if (!NextFilter || NextFilter->BlockStart != BlockStart || NextFilter->BlockLength != FilteredData.length() || NextFilter->NextWindow)
								break;

							// Apply several filters to same data block
							vm.SetMemory(0, FilteredData, FilteredData.length());

							NextFilter->InitR[6] = (uint32)WrittenFileSize;
							vm.Execute(NextFilter);
							auto	FilteredData	= vm.GetMemory(vm.GetGlobal(vm.GLOBAL_NEWPOS)  & (vm.MEMSIZE - 1), vm.GetGlobal(vm.GLOBAL_NEWSIZE) & (vm.MEMSIZE - 1));

							i++;
							delete PrgStack[i];
							PrgStack[i] = NULL;
						}

						Write(FilteredData, FilteredData.length());
						WrittenFileSize	+= FilteredData.length();
						WrittenBorder	= BlockEnd;
						WriteSize		= uint32((UnpPtr - WrittenBorder) & WinMask);

					} else {
						// Current filter intersects the window write border, so we adjust the window border to process this filter next time, not now.
						for (size_t j = i; j < PrgStack.size(); j++) {
							Filter* flt = PrgStack[j];
							if (flt && flt->NextWindow)
								flt->NextWindow = false;
						}
						WrPtr = WrittenBorder;
						return;
					}
				}
			}
		}
	}

	WriteArea(WrittenBorder, UnpPtr);
	WrPtr = UnpPtr;
}

bool Unpack30::ModelLZS::ReadTables(BitInput &Inp) {
	uint8 BitLength[BC];
	uint8 Table[HUFF_TABLE_SIZE];

	PrevLowDist		= 0;
	LowDistRepCount = 0;

	if (Inp.get(2) == 0)
		clear(UnpOldTable);

	for (uint32 i = 0; i < BC; i++) {
		uint32 Length = Inp.get(4);
		if (Length == 15) {
			uint32 ZeroCount = Inp.get(4);
			if (ZeroCount == 0) {
				BitLength[i] = 15;
			} else {
				ZeroCount += 2;
				while (ZeroCount-- > 0 && i < BC)
					BitLength[i++] = 0;
				i--;
			}
		} else {
			BitLength[i] = Length;
		}
	}

	BD.Init(BitLength, BC);

	const uint32 TableSize = HUFF_TABLE_SIZE;
	for (uint32 i = 0; i < TableSize;) {
		uint32 Number = BD.DecodeNumber(Inp);
		if (Number < 16) {
			Table[i] = (Number + UnpOldTable[i]) & 0xf;
			i++;
		} else if (Number < 18) {
			uint32 n = Number == 16 ? Inp.get(3) + 3 : Inp.get(7) + 11;
			if (i == 0)
				return false;  // We cannot have "repeat previous" code at the first position.

			while (n-- > 0 && i < TableSize) {
				Table[i] = Table[i - 1];
				i++;
			}
		} else {
			uint32 n = Number == 18 ? Inp.get(3) + 3 : Inp.get(7) + 11;
			while (n-- > 0 && i < TableSize)
				Table[i++] = 0;
		}
	}

	LD.	Init(Table + 0,				NC,		DecodeTable::MAX_QUICK_DECODE_BITS);
	DD.	Init(Table + NC,			DC);
	LDD.Init(Table + NC + DC,		LDC);
	RD.	Init(Table + NC + DC + LDC,	RC);
	memcpy(UnpOldTable, Table, sizeof(UnpOldTable));
	return true;
}

void Unpack30::InitFilters(bool Solid) {
	if (!Solid) {
		LastFilter = 0;
		for (auto &i : Programs)
			delete i;
		Programs.clear();
	}
	for (auto &i : PrgStack)
		delete i;
	PrgStack.clear();
}

enum {
	VMCF_OPMASK		=  3,
	VMCF_BYTEMODE	=  4,
	VMCF_JUMP		=  8,
	VMCF_PROC		= 16,
	VMCF_USEFLAGS	= 32,
	VMCF_CHFLAGS	= 64,
};

static uint8 VM_CmdFlags[] = {
	/* MOV	*/ 2 | VMCF_BYTEMODE								,
	/* CMP	*/ 2 | VMCF_BYTEMODE | VMCF_CHFLAGS					,
	/* ADD	*/ 2 | VMCF_BYTEMODE | VMCF_CHFLAGS					,
	/* SUB	*/ 2 | VMCF_BYTEMODE | VMCF_CHFLAGS					,
	/* JZ	*/ 1 | VMCF_JUMP | VMCF_USEFLAGS					,
	/* JNZ	*/ 1 | VMCF_JUMP | VMCF_USEFLAGS					,
	/* INC	*/ 1 | VMCF_BYTEMODE | VMCF_CHFLAGS					,
	/* DEC	*/ 1 | VMCF_BYTEMODE | VMCF_CHFLAGS					,

	/* JMP	*/ 1 | VMCF_JUMP									,
	/* XOR	*/ 2 | VMCF_BYTEMODE | VMCF_CHFLAGS					,
	/* AND	*/ 2 | VMCF_BYTEMODE | VMCF_CHFLAGS					,
	/* OR	*/ 2 | VMCF_BYTEMODE | VMCF_CHFLAGS					,
	/* TEST	*/ 2 | VMCF_BYTEMODE | VMCF_CHFLAGS					,
	/* JS	*/ 1 | VMCF_JUMP | VMCF_USEFLAGS					,
	/* JNS	*/ 1 | VMCF_JUMP | VMCF_USEFLAGS					,
	/* JB	*/ 1 | VMCF_JUMP | VMCF_USEFLAGS					,

	/* JBE	*/ 1 | VMCF_JUMP | VMCF_USEFLAGS					,
	/* JA	*/ 1 | VMCF_JUMP | VMCF_USEFLAGS					,
	/* JAE	*/ 1 | VMCF_JUMP | VMCF_USEFLAGS					,
	/* PUSH	*/ 1												,
	/* POP	*/ 1												,
	/* CALL	*/ 1 | VMCF_PROC									,
	/* RET	*/ 0 | VMCF_PROC									,
	/* NOT	*/ 1 | VMCF_BYTEMODE								,
	
	/* SHL	*/ 2 | VMCF_BYTEMODE | VMCF_CHFLAGS					,
	/* SHR	*/ 2 | VMCF_BYTEMODE | VMCF_CHFLAGS					,
	/* SAR	*/ 2 | VMCF_BYTEMODE | VMCF_CHFLAGS					,
	/* NEG	*/ 1 | VMCF_BYTEMODE | VMCF_CHFLAGS					,
	/* PUSHA*/ 0												,
	/* POPA	*/ 0												,
	/* PUSHF*/ 0 | VMCF_USEFLAGS								,
	/* POPF	*/ 0 | VMCF_CHFLAGS									,
	
	/* MOVZX*/ 2												,
	/* MOVSX*/ 2												,
	/* XCHG	*/ 2 | VMCF_BYTEMODE								,
	/* MUL	*/ 2 | VMCF_BYTEMODE								,
	/* DIV	*/ 2 | VMCF_BYTEMODE								,
	/* ADC	*/ 2 | VMCF_BYTEMODE | VMCF_USEFLAGS | VMCF_CHFLAGS	,
	/* SBB	*/ 2 | VMCF_BYTEMODE | VMCF_USEFLAGS | VMCF_CHFLAGS	,
	/* PRINT*/ 0
};

void VM::DecodeArg(Operand &Op, Reader& v, bool ByteMode) {
	if (v.get(1)) {
		Op.Type	= Op.REG;
		Op.Data	= v.get(3);
		Op.Addr	= &R[Op.Data];

	} else if (!v.get(1)) {
		Op.Type	= Op.INT;
		Op.Data	= ByteMode ? v.get(8) : v.get32();
		Op.Addr	= &Op.Data;

	} else {
		Op.Type = Op.REGMEM;
		if (!v.get(1)) {
			// Base address is zero, just use the address from register
			Op.Data	= v.get(3);
			Op.Addr	= &R[Op.Data];
			Op.Base	= 0;
		} else {
			if (!v.get(1)) {
				// Use both register and base address
				Op.Data	= v.get(3);
				Op.Addr	= &R[Op.Data];
			} else {
				// Use base address only. Access memory by fixed address
				Op.Data=0;
			}
			Op.Base = v.get32();
		}
	}
}

bool VM::Prepare(const_memory_block code, Program* Prg) {
	// Calculate the single uint8 XOR checksum to check validity of VM code
	uint8	XorSum = 0;
	for (auto &i : make_range<uint8>(code))
		XorSum ^= i;

	if (XorSum != 0)
		return false;

	struct StandardFilters {
		uint32 Length, CRC;
		FILTER	Type;
	} static StdList[] = {
		53,		0xad576887, E8,
		57,		0x3cd7e57e, E8E9,
		120,	0x3769893f, ITANIUM,
		29,		0x0e06077d, DELTA,
		149,	0x1c2c5dc8, RGB,
		216,	0xbc85e701, AUDIO
	};
	uint32	CodeCRC		= CRC32::calc(code.begin(), code.length());
	FILTER	Standard	= NONE;
	for (auto &i : StdList) {
		if (i.CRC == CodeCRC && i.Length==code.length()) {
			Standard = i.Type;
			break;
		}
	}

	if (Standard) {
		auto	&cmd = Prg->Cmd.push_back(STANDARD);
		cmd.Op1.Data	= Standard;

	} else {
		Reader	vlc(code);

		// Read static data contained in DB operators
		if (vlc.get(1)) {
			uint32 DataSize = vlc.get32() + 1;
			Prg->StaticData.create(DataSize);
			for (uint8 *p = Prg->StaticData; p != Prg->StaticData.end(); ++p)
				*p = vlc.get(8);
		}

		for (;;) {
			auto&	CurCmd	= Prg->Cmd.push_back((COMMAND)(vlc.get(1) ? vlc.get(5) + 8 : vlc.get(3)));
			CurCmd.ByteMode	= (VM_CmdFlags[CurCmd.OpCode] & VMCF_BYTEMODE) && vlc.get(1);
			int OpNum		= (VM_CmdFlags[CurCmd.OpCode] & VMCF_OPMASK);

			if (OpNum > 0) {
				DecodeArg(CurCmd.Op1, vlc, CurCmd.ByteMode);  // reading the first operand
				if (OpNum == 2) {
					DecodeArg(CurCmd.Op2, vlc, CurCmd.ByteMode);  // reading the second operand

				} else if (CurCmd.Op1.Type == Operand::INT && (VM_CmdFlags[CurCmd.OpCode] & (VMCF_JUMP | VMCF_PROC))) {
					// Calculating jump distance.
					int Distance = CurCmd.Op1.Data;
					if (Distance >= 256) {
						Distance -= 256;
					} else {
						if (Distance >= 136)
							Distance -= 264;
						else if (Distance >= 16)
							Distance -= 8;
						else if (Distance >= 8)
							Distance -= 16;
						Distance += Prg->Cmd.size() - 1;
					}
					CurCmd.Op1.Data = Distance;
				}
			}
		}
	}

	Prg->Cmd.push_back(RET);
	return true;
}

bool VM::Execute(ProgramInstance *Inst) {
	size_t GlobalSize = min(Inst->GlobalData.length(), GLOBALMEMSIZE);
	if (GlobalSize)
		memcpy(mem + GLOBALMEMADDR, Inst->GlobalData, GlobalSize);

	if (size_t StaticSize = min(Inst->Prg->StaticData.length(), GLOBALMEMSIZE - GlobalSize))
		memcpy(mem + GLOBALMEMADDR + GlobalSize, Inst->Prg->StaticData, StaticSize);

	bool	Success		= ExecuteCode(Inst->Prg->Cmd, Inst->Prg->Cmd.size(), Inst->InitR);
	Inst->GlobalData	= mem.slice(GLOBALMEMADDR, min(GetGlobal(GLOBAL_GLOBAL), GLOBALMEMSIZE - FIXEDGLOBALSIZE));

	return Success;
}

#define SET_IP(IP)  if ((IP)>=CodeSize) return true; Cmd=Code+(IP)-1;

bool VM::ExecuteCode(Command* Code, uint32 CodeSize, uint32 InitR[8]) {
	uint32	Flags = 0;
	memcpy(R, InitR, sizeof(R));

	Command* Cmd		= Code;
	for (int MaxOpCount = 25000000; MaxOpCount--;) {
		uint32* Op1 = GetOperand(Cmd->Op1);
		uint32* Op2 = GetOperand(Cmd->Op2);

		switch (Cmd->OpCode) {
			case MOV:	SetValue(Cmd->ByteMode, Op1, GetValue(Cmd->ByteMode, Op2)); break;
			case MOVB:	SetValue(true, Op1, GetValue(true, Op2)); break;
			case MOVD:	SetValue(false, Op1, GetValue(false, Op2)); break;
			case CMP: {
				uint32 Value1 = GetValue(Cmd->ByteMode, Op1);
				uint32 Result = (Value1 - GetValue(Cmd->ByteMode, Op2));
				Flags		  = Result == 0 ? FZ : (Result > Value1) | (Result & FS);
				break;
			}
			case CMPB: {
				uint32 Value1 = GetValue(true, Op1);
				uint32 Result = (Value1 - GetValue(true, Op2));
				Flags		  = Result == 0 ? FZ : (Result > Value1) | (Result & FS);
				break;
			}
			case CMPD: {
				uint32 Value1 = GetValue(false, Op1);
				uint32 Result = (Value1 - GetValue(false, Op2));
				Flags		  = Result == 0 ? FZ : (Result > Value1) | (Result & FS);
				break;
			}
			case ADD: {
				uint32 Value1 = GetValue(Cmd->ByteMode, Op1);
				uint32 Result = (Value1 + GetValue(Cmd->ByteMode, Op2));
				if (Cmd->ByteMode) {
					Result &= 0xff;
					Flags = (Result < Value1) | (Result == 0 ? FZ : ((Result & 0x80) ? FS : 0));
				} else {
					Flags = (Result < Value1) | (Result == 0 ? FZ : (Result & FS));
				}
				SetValue(Cmd->ByteMode, Op1, Result);
				break;
			}
			case ADDB:	SetValue(true, Op1, GetValue(true, Op1) + GetValue(true, Op2)); break;
			case ADDD:	SetValue(false, Op1, GetValue(false, Op1) + GetValue(false, Op2)); break;
			case SUB: {
				uint32 Value1 = GetValue(Cmd->ByteMode, Op1);
				uint32 Result = (Value1 - GetValue(Cmd->ByteMode, Op2));
				Flags		  = Result == 0 ? FZ : (Result > Value1) | (Result & FS);
				SetValue(Cmd->ByteMode, Op1, Result);
				break;
			}
			case SUBB:	SetValue(true, Op1, GetValue(true, Op1) - GetValue(true, Op2)); break;
			case SUBD:	SetValue(false, Op1, GetValue(false, Op1) - GetValue(false, Op2)); break;
			case JZ:
				if (Flags & FZ)
					SET_IP(GetValue(false, Op1));
				break;
			case JNZ:
				if (!(Flags & FZ))
					SET_IP(GetValue(false, Op1));
				break;
			case INC: {
				uint32 Result = GetValue(Cmd->ByteMode, Op1) + 1;
				if (Cmd->ByteMode)
					Result &= 0xff;
				SetValue(Cmd->ByteMode, Op1, Result);
				Flags = Result == 0 ? FZ : Result & FS;
				break;
			}
			case INCB:	SetValue(true, Op1, GetValue(true, Op1) + 1); break;
			case INCD:	SetValue(false, Op1, GetValue(false, Op1) + 1); break;
			case DEC: {
				uint32 Result = GetValue(Cmd->ByteMode, Op1) - 1;
				SetValue(Cmd->ByteMode, Op1, Result);
				Flags = Result == 0 ? FZ : Result & FS;
				break;
			}
			case DECB:	SetValue(true, Op1, GetValue(true, Op1) - 1); break;
			case DECD:	SetValue(false, Op1, GetValue(false, Op1) - 1); break;
			case JMP: SET_IP(GetValue(false, Op1)); break;
			case XOR: {
				uint32 Result = GetValue(Cmd->ByteMode, Op1) ^ GetValue(Cmd->ByteMode, Op2);
				Flags		  = Result == 0 ? FZ : Result & FS;
				SetValue(Cmd->ByteMode, Op1, Result);
				break;
			}
			case AND: {
				uint32 Result = GetValue(Cmd->ByteMode, Op1) & GetValue(Cmd->ByteMode, Op2);
				Flags		  = Result == 0 ? FZ : Result & FS;
				SetValue(Cmd->ByteMode, Op1, Result);
				break;
			}
			case OR: {
				uint32 Result = GetValue(Cmd->ByteMode, Op1) | GetValue(Cmd->ByteMode, Op2);
				Flags		  = Result == 0 ? FZ : Result & FS;
				SetValue(Cmd->ByteMode, Op1, Result);
				break;
			}
			case TEST: {
				uint32 Result = GetValue(Cmd->ByteMode, Op1) & GetValue(Cmd->ByteMode, Op2);
				Flags		  = Result == 0 ? FZ : Result & FS;
				break;
			}
			case JS:
				if (Flags & FS)
					SET_IP(GetValue(false, Op1));
				break;
			case JNS:
				if (Flags & FS)
					SET_IP(GetValue(false, Op1));
				break;
			case JB:
				if (Flags & FC)
					SET_IP(GetValue(false, Op1));
				break;
			case JBE:
				if (Flags & (FC | FZ))
					SET_IP(GetValue(false, Op1));
				break;
			case JA:
				if (!(Flags & (FC | FZ)))
					SET_IP(GetValue(false, Op1));
				break;
			case JAE:
				if (!(Flags & FC))
					SET_IP(GetValue(false, Op1));
				break;
			case PUSH:
				SetValue(false, GetMem(R[7] -= 4), GetValue(false, Op1));
				break;
			case POP:
				SetValue(false, Op1, GetValue(false, GetMem(R[7])));
				R[7] += 4;
				break;
			case CALL:
				SetValue(false, GetMem(R[7] -= 4), Cmd - Code + 1);
				SET_IP(GetValue(false, Op1));
				break;
			case NOT: SetValue(Cmd->ByteMode, Op1, ~GetValue(Cmd->ByteMode, Op1)); break;
			case SHL: {
				uint32 Value1 = GetValue(Cmd->ByteMode, Op1);
				uint32 Value2 = GetValue(Cmd->ByteMode, Op2);
				uint32 Result = Value1 << Value2;
				Flags		  = (Result == 0 ? FZ : (Result & FS)) | ((Value1 << (Value2 - 1)) & 0x80000000 ? FC : 0);
				SetValue(Cmd->ByteMode, Op1, Result);
				break;
			}
			case SHR: {
				uint32 Value1 = GetValue(Cmd->ByteMode, Op1);
				uint32 Value2 = GetValue(Cmd->ByteMode, Op2);
				uint32 Result = Value1 >> Value2;
				Flags		  = (Result == 0 ? FZ : (Result & FS)) | ((Value1 >> (Value2 - 1)) & FC);
				SetValue(Cmd->ByteMode, Op1, Result);
				break;
			}
			case SAR: {
				uint32 Value1 = GetValue(Cmd->ByteMode, Op1);
				uint32 Value2 = GetValue(Cmd->ByteMode, Op2);
				uint32 Result = (int)Value1 >> Value2;
				Flags		  = (Result == 0 ? FZ : (Result & FS)) | ((Value1 >> (Value2 - 1)) & FC);
				SetValue(Cmd->ByteMode, Op1, Result);
				break;
			}
			case NEG: {
				// We use "0-value" expression to suppress "unary minus to unsigned" compiler warning.
				uint32 Result = (0 - GetValue(Cmd->ByteMode, Op1));
				Flags		  = Result == 0 ? FZ : FC | (Result & FS);
				SetValue(Cmd->ByteMode, Op1, Result);
				break;
			}
			case NEGB: SetValue(true, Op1, 0 - GetValue(true, Op1)); break;
			case NEGD: SetValue(false, Op1, 0 - GetValue(false, Op1)); break;
			case PUSHA:
				for (int i = 0, SP = R[7] - 4; i < 8; i++, SP -= 4)
					SetValue(false, GetMem(SP), R[i]);
				R[7] -= 8 * 4;
				break;
			case POPA:
				for (uint32 i = 0, SP = R[7]; i < 8; i++, SP += 4)
					R[7 - i] = GetValue(false, GetMem(SP));
				break;
			case PUSHF:
				SetValue(false, GetMem(R[7] -= 4), Flags);
				break;
			case POPF:
				Flags = GetValue(false, GetMem(R[7]));
				R[7] += 4;
				break;
			case MOVZX:	SetValue(false, Op1, GetValue(true, Op2)); break;
			case MOVSX:	SetValue(false, Op1, (signed char)GetValue(true, Op2)); break;
			case XCHG: {
				uint32 Value1 = GetValue(Cmd->ByteMode, Op1);
				SetValue(Cmd->ByteMode, Op1, GetValue(Cmd->ByteMode, Op2));
				SetValue(Cmd->ByteMode, Op2, Value1);
				break;
			}
			case MUL:
				SetValue(Cmd->ByteMode, Op1, GetValue(Cmd->ByteMode, Op1) * GetValue(Cmd->ByteMode, Op2));
				break;
			case DIV:
				if (uint32 Divider = GetValue(Cmd->ByteMode, Op2))
					SetValue(Cmd->ByteMode, Op1, GetValue(Cmd->ByteMode, Op1) / Divider);
				break;
			case ADC: {
				uint32 Value1 = GetValue(Cmd->ByteMode, Op1);
				uint32 fc	  = Flags & FC;
				uint32 Result = (Value1 + GetValue(Cmd->ByteMode, Op2) + fc);
				if (Cmd->ByteMode)
					Result &= 0xff;
				Flags = (Result < Value1 || Result == Value1 && fc) | (Result == 0 ? FZ : (Result & FS));
				SetValue(Cmd->ByteMode, Op1, Result);
				break;
			}
			case SBB: {
				uint32 Value1 = GetValue(Cmd->ByteMode, Op1);
				uint32 fc	  = (Flags & FC);
				uint32 Result = (Value1 - GetValue(Cmd->ByteMode, Op2) - fc);
				if (Cmd->ByteMode)
					Result &= 0xff;
				Flags = (Result > Value1 || Result == Value1 && fc) | (Result == 0 ? FZ : (Result & FS));
				SetValue(Cmd->ByteMode, Op1, Result);
				break;
			}
			case RET:
				if (R[7] >= MEMSIZE)
					return true;
				SET_IP(GetValue(false, GetMem(R[7])));
				R[7] += 4;
				break;
			case PRINT:
				break;
			case STANDARD:
				switch (Cmd->Op1.Data) {
					case E8:		branch::X86_Convert(mem.slice_to(R[4]), R[6], 0xff, false);		SetGlobal(GLOBAL_NEWPOS, 0); break;
					case E8E9:		branch::X86_Convert(mem.slice_to(R[4]), R[6], 0xfe, false);		SetGlobal(GLOBAL_NEWPOS, 0); break;
					case ITANIUM:	branch::Itanium_Convert(mem.slice_to(R[4]), R[6] >> 4, false);	SetGlobal(GLOBAL_NEWPOS, 0); break;
					case DELTA:		FilterDelta(mem.slice_to(R[4]), mem + R[4], R[0]);				SetGlobal(GLOBAL_NEWPOS, R[4]); break;
					case RGB:		FilterRGB(mem.slice_to(R[4]), mem + R[4], R[0], R[1]);			SetGlobal(GLOBAL_NEWPOS, R[4]); break;
					case AUDIO:		FilterAudio(mem.slice_to(R[4]), mem + R[4], R[0]);				SetGlobal(GLOBAL_NEWPOS, R[4]); break;
				}
				SetGlobal(GLOBAL_NEWSIZE, R[4]);
				break;
		}
		Cmd++;
	}
	return false;
}

//-----------------------------------------------------------------------------
//	Unpack v 5.0
//-----------------------------------------------------------------------------

void Unpack50::process(istream_ref file, bool Solid) {
	Filters.clear();
	if (!ReadBlockHeader(file, BlockHeader))
		return;

	BitInput	Inp(file);
	if (!ReadTables(Inp, BlockHeader, BlockTables))
		return;

	while (true) {
		UnpPtr &= WinMask;
#if 0
		if (Inp.InAddr >= ReadBorder) {
			bool FileDone = false;

			// We use 'while', because for empty block containing only Huffman table, we'll be on the block border once again just after reading the table.
			while (Inp.InAddr > BlockHeader.BlockStart + BlockHeader.BlockSize - 1 || Inp.InAddr == BlockHeader.BlockStart + BlockHeader.BlockSize - 1 && Inp.InBit >= BlockHeader.BlockBitSize) {
				if (BlockHeader.LastBlockInFile) {
					FileDone = true;
					break;
				}
				if (!ReadBlockHeader(file, BlockHeader) || !ReadTables(Inp, BlockHeader, BlockTables))
					return;
			}
			if (FileDone)
				break;
		}
#endif
//		if (((WriteBorder - UnpPtr) & WinMask) < MAX_INC_LZ_MATCH && WriteBorder != UnpPtr) {
//			WriteBuf();
//			if (WrittenFileSize > DestUnpSize)
//				return;
//		}

		uint32 MainSlot = BlockTables.LD.DecodeNumber(Inp);
		if (MainSlot < 256) {
			Window[UnpPtr++] = (uint8)MainSlot;

		} else if (MainSlot >= 262) {
			uint32 Length = SlotToLength(Inp, MainSlot - 262);

			uint32 DBits, Distance = 1, DistSlot = BlockTables.DD.DecodeNumber(Inp);
			if (DistSlot < 4) {
				DBits = 0;
				Distance += DistSlot;
			} else {
				DBits = DistSlot / 2 - 1;
				Distance += (2 | (DistSlot & 1)) << DBits;
			}

			if (DBits > 0) {
				if (DBits >= 4) {
					if (DBits > 4)
						Distance += Inp.get(DBits - 4) << 4;
					uint32 LowDist = BlockTables.LDD.DecodeNumber(Inp);
					Distance += LowDist;
				} else {
					Distance += Inp.get(DBits);
				}
			}

			if (Distance > 0x100) {
				Length++;
				if (Distance > 0x2000) {
					Length++;
					if (Distance > 0x40000)
						Length++;
				}
			}

			InsertOldDist(Distance);
			LastLength = Length;
			CopyString(Length, Distance);

		} else if (MainSlot == 256) {
			Filter Filter;

			Filter.BlockStart  = ReadFilterData(Inp);
			Filter.BlockLength = ReadFilterData(Inp);
			if (Filter.BlockLength > MAX_FILTER_BLOCK_SIZE)
				Filter.BlockLength = 0;

			Filter.Type = Inp.get(3);
			if (Filter.Type == Filter::DELTA)
				Filter.Channels = Inp.get(5) + 1;

			if (Filters.size() >= MAX_UNPACK_FILTERS) {
				WriteBuf();
				if (Filters.size() >= MAX_UNPACK_FILTERS)
					Filters.clear();	// Still too many filters, prevent excessive memory use
			}

			// If distance to filter start is that large that due to circular dictionary mode now it points to old not written yet data, then we set 'NextWindow' flag and process this filter only after processing that older data
			Filter.NextWindow = WrPtr != UnpPtr && ((WrPtr - UnpPtr) & WinMask) <= Filter.BlockStart;
			Filter.BlockStart = uint32((Filter.BlockStart + UnpPtr) & WinMask);
			Filters.push_back(Filter);

		} else if (MainSlot == 257) {
			if (LastLength != 0)
				CopyString(LastLength, OldDist[0]);

		} else {//	if (MainSlot < 262) {
			uint32 DistNum	= MainSlot - 258;
			uint32 Distance = OldDist[DistNum];
			for (uint32 i = DistNum; i > 0; i--)
				OldDist[i] = OldDist[i - 1];
			OldDist[0] = Distance;

			uint32 LengthSlot = BlockTables.RD.DecodeNumber(Inp);
			uint32 Length	  = SlotToLength(Inp, LengthSlot);
			LastLength		  = Length;
			CopyString(Length, Distance);
		}
	}
	WriteBuf();
}

void Unpack50::WriteBuf() {
	size_t WrittenBorder		  = WrPtr;
	size_t FullWriteSize		  = (UnpPtr - WrittenBorder) & WinMask;
	size_t WriteSizeLeft		  = FullWriteSize;
	bool   NotAllFiltersProcessed = false;
	for (auto flt : Filters) {
		if (flt.Type == Filter::NONE)
			continue;

		if (flt.NextWindow) {
			if (((flt.BlockStart - WrPtr) & WinMask) <= FullWriteSize)
				flt.NextWindow = false;
			continue;
		}

		uint32 BlockStart  = flt.BlockStart;
		uint32 BlockLength = flt.BlockLength;
		if (((BlockStart - WrittenBorder) & WinMask) < WriteSizeLeft) {
			if (WrittenBorder != BlockStart) {
				WriteArea(WrittenBorder, BlockStart);
				WrittenBorder = BlockStart;
				WriteSizeLeft = (UnpPtr - WrittenBorder) & WinMask;
			}

			if (BlockLength <= WriteSizeLeft) {
				if (BlockLength > 0) {  // We set it to 0 also for invalid filters.
					uint32 BlockEnd = (BlockStart + BlockLength) & WinMask;

					FilterSrcMemory.resize(BlockLength);
					uint8* Mem = FilterSrcMemory;
					if (BlockStart < BlockEnd || BlockEnd == 0) {
						memcpy(Mem, Window + BlockStart, BlockLength);
					} else {
						size_t FirstPartLength = size_t(WinSize - BlockStart);
						memcpy(Mem, Window + BlockStart, FirstPartLength);
						memcpy(Mem + FirstPartLength, Window, BlockEnd);
					}

					uint8* OutMem = flt.Apply(FilterSrcMemory, FilterDstMemory, WrittenFileSize);

					flt.Type = Filter::NONE;

					if (OutMem)
						Write(OutMem, BlockLength);

					WrittenFileSize += BlockLength;
					WrittenBorder = BlockEnd;
					WriteSizeLeft = (UnpPtr - WrittenBorder) & WinMask;
				}
			} else {
				// Current filter intersects the window write border, so we adjust the window border to process this filter next time, not now.
				WrPtr = WrittenBorder;

				// Since Filter start position can only increase, we quit processing all following filters for this data block and reset 'NextWindow' flag for them.
				for (Filter* flt2 = &flt; flt2 < Filters.end(); ++flt2) {
					if (flt2->Type != Filter::NONE)
						flt2->NextWindow = false;
				}

				// Do not write data left after current filter now
				NotAllFiltersProcessed = true;
				break;
			}
		}
	}

	// Remove processed filters from queue
	size_t EmptyCount = 0;
	for (size_t i = 0; i < Filters.size(); i++) {
		if (EmptyCount > 0)
			Filters[i - EmptyCount] = Filters[i];
		if (Filters[i].Type == Filter::NONE)
			EmptyCount++;
	}
	if (EmptyCount > 0)
		Filters.resize(Filters.size() - EmptyCount);

	if (!NotAllFiltersProcessed) {  // Only if all filters are processed
		// Write data left after last filter.
		WriteArea(WrittenBorder, UnpPtr);
		WrPtr = UnpPtr;
	}

	// We prefer to write data in blocks not exceeding UNPACK_MAX_WRITE/ instead of potentially huge WinSize blocks. It also allows us to keep the size of Filters array reasonable.
	//WriteBorder = (UnpPtr + min(WinSize, UNPACK_MAX_WRITE)) & WinMask;

	// Choose the nearest among WriteBorder and WrPtr actual written border.
	// If border is equal to UnpPtr, it means that we have WinSize data ahead.
//	if (WriteBorder == UnpPtr || WrPtr != UnpPtr && ((WrPtr - UnpPtr) & WinMask) < ((WriteBorder - UnpPtr) & WinMask))
//		WriteBorder = WrPtr;
}

bool Unpack50::ReadBlockHeader(istream_ref file, UnpackBlockHeader& Header) {
	Header.HeaderSize = 0;

	uint8 BlockFlags = file.getc();
	uint32 ByteCount = ((BlockFlags >> 3) & 3) + 1;	 // Block size uint8 count.

	if (ByteCount == 4)
		return false;

	Header.HeaderSize	= 2 + ByteCount;
	Header.BlockBitSize	= (BlockFlags & 7) + 1;

	uint8 SavedCheckSum = file.getc();

	int BlockSize = 0;
	for (uint32 i = 0; i < ByteCount; i++)
		BlockSize += file.getc() << (i * 8);

	Header.BlockSize = BlockSize;
	uint8 CheckSum	 = uint8(0x5a ^ BlockFlags ^ BlockSize ^ (BlockSize >> 8) ^ (BlockSize >> 16));
	if (CheckSum != SavedCheckSum)
		return false;

	Header.BlockStart		= file.tell();
	Header.LastBlockInFile	= !!(BlockFlags & 0x40);
	Header.TablePresent		= !!(BlockFlags & 0x80);
	return true;
}

bool Unpack50::ReadTables(BitInput& Inp, UnpackBlockHeader& Header, UnpackBlockTables& Tables) {
	if (!Header.TablePresent)
		return true;

	uint8 BitLength[BC];
	for (uint32 i = 0; i < BC; i++) {
		uint32 Length = Inp.get(4);
		if (Length == 15) {
			uint32 ZeroCount = Inp.get(4);
			if (ZeroCount == 0)
				BitLength[i] = 15;
			else {
				ZeroCount += 2;
				while (ZeroCount-- > 0 && i < BC)
					BitLength[i++] = 0;
				i--;
			}
		} else {
			BitLength[i] = Length;
		}
	}

	Tables.BD.Init(BitLength, BC);

	uint8		 Table[HUFF_TABLE_SIZE];
	const uint32 TableSize = HUFF_TABLE_SIZE;
	for (uint32 i = 0; i < TableSize;) {
		uint32 Number = Tables.BD.DecodeNumber(Inp);
		if (Number < 16) {
			Table[i] = Number;
			i++;
		} else if (Number < 18) {
			uint32 n = Number == 16 ? Inp.get(3) + 3 :Inp.get(7) + 11;
			if (i == 0)
				return false;
			while (n-- > 0 && i < TableSize) {
				Table[i] = Table[i - 1];
				i++;
			}
		} else {
			uint32 n = Number == 18 ? Inp.get(3) + 3 : Inp.get(7) + 11;
			while (n-- > 0 && i < TableSize)
				Table[i++] = 0;
		}
	}
	Tables.LD.Init(Table + 0,				NC);
	Tables.DD.Init(Table + NC,				DC);
	Tables.LDD.Init(Table + NC + DC,		LDC);
	Tables.RD.Init(Table + NC + DC + LDC,	RC);
	return true;
}

uint8* Unpack50::Filter::Apply(memory_block mem, malloc_block &dst, uint32 FileOffset) {
	switch (Type) {
		case Filter::DELTA:
			dst.create(mem.length());
			FilterDelta(mem, dst, Channels);
			return dst;

		case E8:
			branch::X86_Convert(mem, FileOffset, 0xff, false);
			return mem;

		case E8E9:
			branch::X86_Convert(mem, FileOffset, 0xfe, false);
			return mem;

		case Filter::ARM:
			// 2019-11-15: we turned off ARM filter by default when compressing, because it is inefficient for modern 64 bit ARM binaries
			branch::ARM_Convert(mem, FileOffset, false);
			return mem;

	}
	return NULL;
}
