#include "codec.h"
#include "vlc.h"
#include "pred_part_match.h"
#include "base/array.h"
#include "crc32.h"

namespace iso {
namespace rar {

typedef vlc_in<uint32, true>	BitInput;

struct DecodeTable {
	enum {
		MAX_QUICK_DECODE_BITS	= 10,
		LARGEST_TABLE_SIZE		= 306,
	};
	uint32	MaxNum;
	uint32	DecodeLen[16];
	uint32	DecodePos[16];
	uint32	QuickBits;
	uint8	QuickLen[1 << MAX_QUICK_DECODE_BITS];
	uint16	QuickNum[1 << MAX_QUICK_DECODE_BITS];
	uint16	DecodeNum[LARGEST_TABLE_SIZE];
	void	Init(uint8* LengthTable, uint32 Size, uint32 Quick = MAX_QUICK_DECODE_BITS - 3);
	uint32	DecodeNumber(BitInput &Inp);
};

struct UnpackBlockTables {
	DecodeTable LD;	  // Decode literals
	DecodeTable DD;	  // Decode distances
	DecodeTable LDD;  // Decode lower bits of distances
	DecodeTable RD;	  // Decode repeating distances
	DecodeTable BD;	  // Decode bit lengths in Huffman table
};

class Unpack {
protected:
	uint32			OldDist[4];
	uint32			LastLength;
	size_t			UnpPtr, WrPtr;
	int64			WrittenFileSize;

	uint8*			Dest;
	int64			DestUnpSize;

	void	CopyString(uint32 Length, uint32 Distance) {
		for (size_t SrcPtr = UnpPtr - Distance; Length-- > 0; UnpPtr = (UnpPtr + 1) & WinMask)
			Window[UnpPtr] = Window[SrcPtr++ & WinMask];
	}

	void	InsertOldDist(uint32 Distance) {
		OldDist[3] = OldDist[2];
		OldDist[2] = OldDist[1];
		OldDist[1] = OldDist[0];
		OldDist[0] = Distance;
	}

	void	Write(uint8 *data, size_t data_size) {
		memcpy(Dest, data, data_size);
		Dest += data_size;
	}

	void	WriteData(uint8 *Data, size_t Size) {
		if (WrittenFileSize < DestUnpSize) {
			Write(Data, min(Size, DestUnpSize - WrittenFileSize));
			WrittenFileSize += Size;
		}
	}

	void	WriteArea(size_t StartPtr, size_t EndPtr) {
		if (EndPtr<StartPtr) {
			WriteData(Window + StartPtr, WinSize - StartPtr);
			WriteData(Window, EndPtr);
		} else {
			WriteData(Window + StartPtr, EndPtr - StartPtr);
		}
	}

public:
	malloc_block	WindowMem;
	uint8*			Window;
	size_t			WinSize;
	size_t			WinMask;

	void	SetDest(memory_block dest) {
		Dest			= dest;
		DestUnpSize		= dest.length();
		LastLength		= 0;
		WrittenFileSize = 0;
		clear(OldDist);
	}
	void	SetWindow(size_t win_size) {
		WinSize		= win_size;
		WinMask		= win_size - 1;
		WindowMem.create(win_size);
		Window		= WindowMem;
	}

	Unpack(memory_block dest, size_t win_size) {
		UnpPtr		= WrPtr = 0;
		SetDest(dest);
		SetWindow(win_size);
	}
};

//-----------------------------------------------------------------------------
//	Unpack v 1.5
//-----------------------------------------------------------------------------

class Unpack15 : public Unpack {
	uint16		ChSet[256], ChSetA[256], ChSetB[256], ChSetC[256];
	uint8		NToPl[256], NToPlB[256], NToPlC[256];
	uint32		FlagBuf, AvrPlc, AvrPlcB, AvrLn1, AvrLn2, AvrLn3;
	int			Buf60, NumHuf, LCount, FlagsCnt;
	uint32		Nhfb, Nlzb, MaxDist3;
	bool		StMode;
	uint32		LastDist, OldDistPtr;

	void	WriteBuf() {
		if (UnpPtr < WrPtr) {
			Write(Window + WrPtr, -(int)WrPtr & WinMask);
			Write(Window, UnpPtr);
		} else {
			Write(Window + WrPtr, UnpPtr - WrPtr);
		}
		WrPtr = UnpPtr;
	}

	void CopyString(uint32 Distance, uint32 Length) {
		DestUnpSize -= Length;
		while (Length--) {
			Window[UnpPtr] = Window[(UnpPtr - Distance) & WinMask];
			UnpPtr		   = (UnpPtr + 1) & WinMask;
		}
	}

	void CopyStringSave(uint32 Distance, uint32 Length) {
		OldDist[OldDistPtr++] = Distance;
		OldDistPtr			  = OldDistPtr & 3;
		CopyString(LastDist = Distance, LastLength = Length);
	}


	uint32 DecodeNum(BitInput &Inp, uint32 StartPos, uint32* DecTab, uint32* PosTab) {
		int I;
		uint32	Num = Inp.peek(12) << 4;
		for (I = 0; DecTab[I] <= Num; I++)
			StartPos++;
		Inp.discard(StartPos);
		return (((Num - (I ? DecTab[I - 1] : 0)) >> (16 - StartPos)) + PosTab[StartPos]);
	}
	void	GetFlagsBuf(BitInput &Inp);

	void	InitHuff() {
		for (uint32 I = 0; I < 256; I++) {
			ChSet[I]	= ChSetB[I] = I << 8;
			ChSetA[I]	= I;
			ChSetC[I]	= ((~I + 1) & 0xff) << 8;
		}
		clear(NToPl);
		clear(NToPlB);
		clear(NToPlC);
		CorrHuff(ChSetB, NToPlB);
	}
	void CorrHuff(uint16* CharSet, uint8* NumToPlace) {
		for (int I = 7; I >= 0; I--)
			for (int J = 0; J < 32; J++, CharSet++)
				*CharSet = (*CharSet & ~0xff) | I;
		memset(NumToPlace, 0, sizeof(NToPl));
		for (int I = 6; I >= 0; I--)
			NumToPlace[I] = (7 - I) * 32;
	}

	void	ShortLZ(BitInput &Inp);
	void	LongLZ(BitInput &Inp);
	void	HuffDecode(BitInput &Inp);

public:
	void	process(istream_ref file, bool Solid);
	Unpack15(memory_block Dest, size_t WinSize) : Unpack(Dest, WinSize) {}
};

//-----------------------------------------------------------------------------
//	Unpack v 2.0
//-----------------------------------------------------------------------------

class Unpack20 : public Unpack {
	enum {
		NC20 = 298, // alphabet = {0, 1, 2, ..., NC - 1}
		DC20 = 48,
		RC20 = 28,
		BC20 = 19,
		MC20 = 257,
	};

	struct AudioVariables {	 // For RAR 2.0 archives only.
		int		K1, K2, K3, K4, K5;
		int		D1, D2, D3, D4;
		int		LastDelta;
		uint32	Dif[11];
		uint32	ByteCount;
		int		LastChar;

		uint8 Decode(int Delta, int ChannelDelta);
	};

	uint32				LastDist, OldDistPtr;

	DecodeTable			MD[4];	// Decode multimedia data, up to 4 channels.
	UnpackBlockTables	BlockTables;
	uint8				UnpOldTable20[MC20 * 4];
	bool				UnpAudioBlock;
	uint32				UnpChannels, UnpCurChannel;
	int					UnpChannelDelta;
	bool				TablesRead2;
	AudioVariables		AudV[4];

	void				CopyString(uint32 Length, uint32 Distance) {
		LastDist		= OldDist[OldDistPtr++] = Distance;
		OldDistPtr		= OldDistPtr & 3; // Needed if RAR 1.5 file is called after RAR 2.0.
		LastLength		= Length;
		DestUnpSize		-= Length;
		Unpack::CopyString(Length, Distance);
	}
	bool				ReadTables(BitInput &Inp);

	void				WriteBuf() {
		if (UnpPtr < WrPtr) {
			Write(&Window[WrPtr], -(int)WrPtr & WinMask);
			Write(Window, UnpPtr);
		} else {
			Write(&Window[WrPtr], UnpPtr - WrPtr);
		}
		WrPtr = UnpPtr;
	}

	void				ReadLastTables(BitInput &Inp);
public:
	void	process(istream_ref file, bool Solid);
	Unpack20(memory_block Dest, size_t WinSize) : Unpack(Dest, WinSize) {}
};

//-----------------------------------------------------------------------------
//	Unpack v 3.0
//-----------------------------------------------------------------------------

class VM {
	typedef vlc_in<uint32, true, memory_reader> _Reader;
	enum COMMAND {
		MOV,  CMP,  ADD,  SUB,  JZ,   JNZ,  INC,  DEC,
		JMP,  XOR,  AND,  OR,   TEST, JS,   JNS,  JB,
		JBE,  JA,   JAE,  PUSH, POP,  CALL, RET,  NOT,
		SHL,  SHR,  SAR,  NEG,  PUSHA,POPA, PUSHF,POPF,
		MOVZX,MOVSX,XCHG, MUL,  DIV,  ADC,  SBB,  PRINT,
		MOVB, MOVD, CMPB, CMPD,
		ADDB, ADDD, SUBB, SUBD, INCB, INCD, DECB, DECD,
		NEGB, NEGD,
		STANDARD,
	};
	enum Flags	{FC = 1, FZ = 2, FS = 0x80000000};

	struct Operand {
		enum TYPE {REG, INT, REGMEM, NONE};
		TYPE		Type	= NONE;
		uint32		Data;
		uint32		Base;
		uint32		*Addr	= nullptr;
	};
	struct Command {
		COMMAND		OpCode;
		bool		ByteMode;
		Operand		Op1, Op2;
		Command(COMMAND OpCode) : OpCode(OpCode) {}
	};

public:
	enum {
		MEMSIZE			= 0x40000,
		GLOBALMEMADDR   = 0x3C000,
		GLOBALMEMSIZE   = 0x2000,
		FIXEDGLOBALSIZE = 64,

		GLOBAL_NEWSIZE	= 0x1c,
		GLOBAL_NEWPOS	= 0x20,
		GLOBAL_GLOBAL	= 0x30,
	};
	struct Reader : _Reader {
		using _Reader::_Reader;
		uint32 get32() {
			switch (get(2)) {
				case 0:
					return get(4);
				case 1: {
					uint32	Data = get(4);
					return Data == 0
						? get(8) | 0xffffff
						: (Data << 4) | get(4);
				}
				case 2:
					return get(16);
				default: {
					uint32	Data = get(16) << 16;
					return Data | get(16);
				}
			}
		}
	};
private:

	malloc_block	mem;
	uint32			R[8];

	void	SetGlobal(uint32 offset, uint32 value) {
		store_packed<uint32le>((uint8*)mem + GLOBALMEMADDR + offset, value);
	}
	uint32	GetValue(bool ByteMode, uint32 *Addr) {
#ifdef BIG_ENDIAN
		if (!mem.contains(Addr))
			return ByteMode ? ((uint8*)Addr)[3] :load_packed<uint32>(Addr);
#endif
		return ByteMode ? *(uint8*)Addr : load_packed<uint32le>(Addr);
	}
	void	SetValue(bool ByteMode,uint32 *Addr,uint32 Value) {
#ifdef BIG_ENDIAN
		if (!mem.contains(Addr)) {
			if (ByteMode)
				((uint8*)Addr)[3] = Value;
			else
				store_packed<uint32>(Addr, Value);
		}
#endif
		if (ByteMode)
			*(uint8 *)Addr=Value;
		else
			store_packed<uint32le>(Addr, Value);
	}
	void	SetLowEndianValue(uint32 *Addr,uint32 Value) {
		store_packed<uint32le>(Addr, Value);
	}
	uint32* GetMem(uint32 offset) {
		return (uint32*)((uint8*)mem + (offset & (MEMSIZE - 1)));
	}
	uint32* GetOperand(const Operand &op) {
		return op.Type == Operand::REGMEM
			? GetMem(*op.Addr + op.Base)
			: op.Addr;
	}
	bool	ExecuteCode(Command *Code, uint32 CodeSize, uint32 InitR[8]);
	void	DecodeArg(Operand &Op, Reader& v, bool ByteMode);

public:

	enum FILTER {
		NONE,
		E8,
		E8E9,
		ITANIUM,
		RGB,
		AUDIO, 
		DELTA
	};
	struct Program {
		dynamic_array<Command> Cmd;
		malloc_block	StaticData; // static data contained in DB operators
	};
	struct ProgramInstance {
		Program*		Prg;
		uint32			InitR[7];
		malloc_block	GlobalData;
	};
	bool	Prepare(const_memory_block code, Program *Prg);
	bool	Execute(ProgramInstance *Inst);

	uint32	GetGlobal(uint32 offset) {
		return load_packed<uint32le>((uint8*)mem + GLOBALMEMADDR + offset);
	}

	memory_block	GetMemory(size_t Pos, size_t Size) {
		return mem.slice(Pos, Size);
	}
	void	SetMemory(size_t Pos, uint8 *Data, size_t DataSize) {
		if (!mem)
			mem.create(MEMSIZE);
		if (Pos < MEMSIZE && Data != mem + Pos) {
			size_t CopySize = min(DataSize, MEMSIZE-Pos);
			if (CopySize)
				memmove(mem + Pos, Data, CopySize);
		}
	}

};

class Unpack30 : public Unpack {
	enum {
		NC						= 299,		// alphabet = {0, 1, 2, ..., NC - 1}
		DC						= 60,
		LDC						= 17,
		RC						= 28,
		BC						= 20,
		MAX3_LZ_MATCH			= 0x101,	// Maximum match length for RAR v3.
		HUFF_TABLE_SIZE			= NC + DC + RC + LDC,
		MAX_UNPACK_FILTERS		= 8192,		// Maximum number of filters per entire data block for RAR3 unpack. Must be at least twice more than v3_MAX_PACK_FILTERS to store filters from two data blocks.
		MAX3_UNPACK_CHANNELS	= 1024,		// Limit maximum number of channels in RAR3 delta filter to some reasonable value to prevent too slow processing of corrupt archives with invalid channels number. Must be equal or larger than v3_MAX_FILTER_CHANNELS.
		LOW_DIST_REP_COUNT		= 16,
	};

	struct Program : VM::Program {
		uint32	LastLength;
	};
	struct Filter : VM::ProgramInstance {
		uint32			BlockStart;
		uint32			BlockLength;
		bool			NextWindow;
	};

	struct ModelLZS : UnpackBlockTables {
		int		PrevLowDist, LowDistRepCount;
		uint8	UnpOldTable[HUFF_TABLE_SIZE];
		bool	ReadTables(BitInput &Inp);
	};

	ModelLZS	LZS;
	ModelPPM	PPM;
	int			PPMEscChar;
	bool		PPMFailed;

	bool		UsePPM;
	bool		TablesRead3;

	VM			vm;
	dynamic_array<Program*>		Programs;
	dynamic_array<Filter*>		PrgStack;
	int							LastFilter	= 0;

	int		PPMDecodeChar(istream_ref file) {
		if (!PPMFailed) {
			int	c = PPM.DecodeChar(file);
			PPMFailed = c < 0;
			return c;
		}
		return -1;
	}
	void	InitFilters(bool Solid);
	bool	AddVMCode(uint32 FirstByte, const_memory_block code);
	void	WriteBuf();

public:
	void	process(istream_ref file, bool Solid);
	Unpack30(memory_block Dest, size_t WinSize) : Unpack(Dest, WinSize) {}
};

//-----------------------------------------------------------------------------
//	Unpack v 5.0
//-----------------------------------------------------------------------------

class Unpack50 : public Unpack {
	enum {
		NC					= 306, // alphabet = {0, 1, 2, ..., NC - 1}
		DC					= 64,
		LDC					= 16,
		RC					= 44,
		HUFF_TABLE_SIZE		= NC + DC + RC + LDC,
		BC					= 20,

		MAX_LZ_MATCH		= 0x1001,
		MAX_INC_LZ_MATCH	= MAX_LZ_MATCH + 3,
		MAX_UNPACK_FILTERS	= 8192,		// Maximum number of filters per entire data block. Must be at least twice more than MAX_PACK_FILTERS to store filters from two data blocks
		MAX_FILTER_BLOCK_SIZE = 0x400000,
		UNPACK_MAX_WRITE	= 0x400000,
	};
	struct Filter {
		enum Type {
			DELTA,			E8,			E8E9,			ARM,
			AUDIO,			RGB,		ITANIUM,		PPM,
			NONE,
		};
		uint8	Type;
		uint32	BlockStart;
		uint32	BlockLength;
		uint8	Channels;
		bool	NextWindow;

		uint8* Apply(memory_block mem, malloc_block &dst, uint32 FileOffset);
	};

	struct UnpackBlockHeader {
		int		BlockSize;
		int		BlockBitSize;
		int		BlockStart;
		int		HeaderSize;
		bool	LastBlockInFile;
		bool	TablePresent;
	};

	malloc_block		FilterSrcMemory;
	malloc_block		FilterDstMemory;
	dynamic_array<Filter> Filters;

	UnpackBlockHeader	BlockHeader;
	UnpackBlockTables	BlockTables;

	static uint32	SlotToLength(BitInput& Inp, uint32 Slot) {
		if (Slot < 8)
			return Slot + 2;
		uint32	LBits = Slot / 4 - 1;
		return ((6 + (Slot & 3)) << LBits) + Inp.get(LBits);
	}
	static uint32	ReadFilterData(BitInput& Inp) {
		uint32 ByteCount = Inp.get(2) + 1;
		uint32 Data = 0;
		for (uint32 I = 0; I < ByteCount; I++)
			Data += Inp.get(8) << (I * 8);
		return Data;
	}

	bool	ReadBlockHeader(istream_ref file, UnpackBlockHeader& Header);
	bool	ReadTables(BitInput& Inp, UnpackBlockHeader& Header, UnpackBlockTables& Tables);
	void	WriteBuf();

public:
	void	process(istream_ref file, bool Solid);
	Unpack50(memory_block Dest, size_t WinSize) : Unpack(Dest, WinSize) {}
};

}} //namespace iso::rar