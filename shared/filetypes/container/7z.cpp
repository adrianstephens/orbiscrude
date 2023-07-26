#include "iso/iso_files.h"
#include "base/array.h"
#include "base/bits.h"
#include "base/algorithm.h"
#include "codec/lzma.h"
#include "codec/branch.h"
#include "crc32.h"
#include "archive_help.h"


/*
Archive {
	SignatureHeader
	[Streams PackedStreams]
	[Streams PackedStreamsForHeaders]
	{
		NID::Header
		[NID::ArchiveProperties			ArchiveProperties]
		[NID::AdditionalStreamsInfo		StreamsInfo ]
		[NID::MainStreamsInfo			StreamsInfo ]
		[NID::FilesInfo					FilesInfo]
		NID::End
	}
	OR
	{
		NID::EncodedHeader
		StreamsInfo for Encoded Header
	}
}

ArchiveProperties {
	while (NID nid != NID::End) {
		UINT64	size;
		size * BYTE
	}
}

~~~~~~~~~~~~
PackInfo {
	UINT64 PackPos
	UINT64 NumPackStreams
	[NID::Size	NumPackStreams * UINT64]
	[NID::CRC	NumPackStreams * UINT32]
	NID::End
}

~~~~~~
Folder {
  UINT64 NumCoders;
  for (NumCoders) {
    BYTE {
      0:3 CodecIdSize
      4:  Is Complex Coder
      5:  There Are Attributes
      6:  Reserved
      7:  There are more alternative methods (not used anymore, must be 0)
    } 
    BYTE CodecId[CodecIdSize]
    if (Is Complex Coder) {
      UINT64 NumInStreams;
      UINT64 NumOutStreams;
    }
    if (There Are Attributes) {
      UINT64 PropertiesSize
      BYTE Properties[PropertiesSize]
    }
  }
    
  NumBindPairs = NumOutStreamsTotal - 1;

  for (NumBindPairs) {
    UINT64 InIndex;
    UINT64 OutIndex;
  }

  NumPackedStreams = NumInStreamsTotal - NumBindPairs;
  if (NumPackedStreams > 1)
    NumPackedStreams * UINT64
}

~~~~~~~~~~~
CodersInfo {
	NID::Folder {
		UINT64 NumFolders
		BYTE External
		if (External != 0)
			UINT64 DataStreamIndex
		NumFolders	* Folder
	}

	NID::CodersUnPackSize  Folders * Folder.NumOutStreams * UINT64
	[NID::CRC	NumFolders * UINT32]
	NID::End
}

~~~~~~~~~~~~~~
SubStreamsInfo {
	[NID::NumUnPackStream	NumFolders * UINT64]
	[NID::Size				NumPackStreams * UINT64]
	[NID::CRC				Digests[Number of streams with unknown CRC]]
	NID::End
}

~~~~~~~~~~~~
StreamsInfo {
	[NID::PackInfo			PackInfo]
	[NID::UnPackInfo		CodersInfo]
	[NID::SubStreamsInfo	SubStreamsInfo]
	NID::End
}

~~~~~~~~~
FilesInfo {
  UINT64 NumFiles

  while (NID nid != NID::End) {
    UINT64	size;
	[NID::EmptyStream	NumFiles * bit]
	[NID::EmptyFile:	EmptyStreams * bit]
	[NID::Anti:			EmptyStreams * bit]
	[NID::kCTime | NID::kATime | NID::kMTime:
        BYTE AllAreDefined
        if (AllAreDefined == 0)
          NumFiles * bit
		BYTE External;
		if (External != 0)
			UINT64 DataIndex
		NumDefined * REAL_UINT64
	]
      
    [NID::Name:
        BYTE External;
        if (External != 0)
			UINT64 DataIndex
		for (NumFiles) {
			wchar_t Names[NameSize];
			wchar_t 0;
		}
	]
	[NID::WinAttrib:
        BYTE AllAreDefined
        if (AllAreDefined == 0)
			NumFiles	* bit
        BYTE External;
        if (External != 0)
			UINT64 DataIndex
		NumDefined * UINT32
    }
  }
}

*/

using namespace iso;

namespace sevenz {

static const uint32	kMaxCodersPerFolder	= 64;
static const uint32	kMaxStreamsPerCoder	= 64;
static const uint32	kNumNoIndex			= 0xFFFFFFFF;
static const uint8	kSignature[]		= {'7', 'z', 0xBC, 0xAF, 0x27, 0x1C};

struct StartHeader {
	packed<uint64>	NextHeaderOffset;
	packed<uint64>	NextHeaderSize;
	uint32			NextHeaderCRC;
};

struct SignatureHeader {
	struct CArchiveVersion {
		static const uint8 kMajorVersion = 0;
		uint8			Major;
		uint8			Minor;
	};
	uint8			signature[6];
	CArchiveVersion	ver;
	uint32			crc32;
	StartHeader		next;

	bool	valid() const {
		return memcmp(signature, kSignature, 6) == 0
			&& CRC32::calc(&next, sizeof(next)) == crc32;
	}
};

#ifdef _7Z_VOL
uint8 kFinishSignature[] = {'7', 'z', 0xBC, 0xAF, 0x27, 0x1C + 1};
struct CFinishHeader : public StartHeader {
	uint64 ArchiveStartOffset;		  // data offset from end if that struct
	uint64 AdditionalStartBlockSize;  // start  signature & start header size
};
#endif

enum class NID {
	End,

	Header,

	ArchiveProperties,

	AdditionalStreamsInfo,
	MainStreamsInfo,
	FilesInfo,

	PackInfo,
	UnpackInfo,
	SubStreamsInfo,

	Size,
	CRC,

	Folder,

	CodersUnpackSize,
	NumUnpackStream,

	EmptyStream,
	EmptyFile,
	Anti,

	Name,
	CTime,
	ATime,
	MTime,
	WinAttrib,
	Comment,

	EncodedHeader,

	StartPos,
	Dummy
};


struct Coder {
	virtual ~Coder() {}
	virtual bool	code(const_memory_block in, memory_block out)	= 0;
	virtual void	finish() { delete this; }
};

struct Filter : static_hash<Filter, uint64> {
	enum MethodFlags {
		None		= 0,
		IsFilter	= 1,
		Encryption	= 2,
	};
	MethodFlags	flags;
	bool	is_filter()		const { return (uint32)flags & (uint32)MethodFlags::IsFilter; }
	bool	is_encryption()	const { return (uint32)flags & (uint32)MethodFlags::Encryption; }

	virtual Coder*	get_decoder(const_memory_block props) { return 0; }
	virtual Coder*	get_encoder(const_memory_block props) { return 0; }

	Filter(uint64 code, MethodFlags flags) : base(code), flags(flags) {}
};

template<uint64 CODE, Filter::MethodFlags FLAGS = Filter::None> struct FilterT : Filter {
	FilterT() : Filter(CODE, FLAGS) {}
};

struct Reader : memory_reader {
	uint64	ReadNumber()			{
		uint8	b		= getc();
		uint64	value	= 0;

		for (uint32 i = 0, mask = 0x80; mask; i++, mask >>= 1) {
			if ((b & mask) == 0)
				return value | ((b & (mask - 1)) << (i * 8));
			value |= ((uint64)getc() << (i * 8));
		}
		return value;
	}
	uint64	ReadNumber(uint32 n)	{
		uint64	v	= 0;
		while (n--)
			v = (v << 8) | getc();
		return v;
	}

	uint32	ReadNum()				{ return (uint32)ReadNumber(); }
	NID		ReadID()				{ return (NID)ReadNumber(); }
	void	SkipData(uint64 size)	{ seek_cur(size); }
	void	SkipData()				{ SkipData(ReadNumber()); }

	bool	WaitId(NID id) {
		for (;;) {
			auto type = ReadID();
			if (type == id)
				return true;
			if (type == NID::End)
				return false;
			SkipData();
		}
	}
	
	Reader(memory_reader data)			: memory_reader(data) {}
	Reader(const_memory_block data)	: memory_reader(data) {}
};

memory_reader GetStream(Reader& file, const dynamic_array<malloc_block>& dataVector) {
	return memory_reader(dataVector[file.ReadNum()]);
}

void ReadBoolVector(memory_reader &file, unsigned numItems, dynamic_bitarray<uint32>& v) {
	v.read(file, numItems);
	for (auto &i :element_cast<uint8>(v.raw()))
		i = reverse_bits(i);
}

void ReadBoolVector2(memory_reader &file, unsigned numItems, dynamic_bitarray<uint32>& v) {
	if (file.getc() == 0)
		ReadBoolVector(file, numItems, v);
	else
		v.resize(numItems, true);
}

template<typename T> struct MaskedVector {
	dynamic_bitarray<uint32>	mask;
	dynamic_array<T>			vals;

	void clear() {
		mask.clear();
		vals.clear();
	}
	bool ValidAndDefined(uint32 i) const {
		return i < mask.size() && mask[i];
	}
	bool GetItem(uint32 i, T& value) const {
		if (ValidAndDefined(i)) {
			value = vals[i];
			return true;
		}
		value = T();
		return false;
	}
	bool ReadVals(istream_ref file) {
		vals.resize(mask.size(), 0);
		for (auto i : mask.where(true))
			file.read(vals[i]);
		return true;
	}
	bool Read(Reader &file, const dynamic_array<malloc_block>& dataVector, unsigned numItems) {
		ReadBoolVector2(file, numItems, mask);
		return file.getc()
			? ReadVals(GetStream(file, dataVector))
			: ReadVals(file);
	}
};


struct Folder {
	struct CoderInfo {
		uint64			MethodID;
		malloc_block	Props;
		uint32			NumStreams;
		size_t			UnpackSize	= 0;

		bool	IsSimpleCoder() const {
			return NumStreams == 1;
		}

		bool	read(Reader &file) {
			uint8 mainByte = file.getc();
			if (mainByte & 0xC0)
				return false;

			unsigned idSize = mainByte & 0xF;
			if (idSize > 8)
				return false;

			MethodID	= file.ReadNumber(idSize);

			if (mainByte & 0x10) {
				NumStreams = file.ReadNum();
				if (NumStreams > kMaxStreamsPerCoder)
					return false;

				if (file.ReadNum() != 1)
					return false;
			} else {
				NumStreams = 1;
			}

			if (mainByte & 0x20)
				Props.read(file, file.ReadNum());
			else
				Props.clear();
			return true;
		}
	};

	struct Binding {
		uint32 PackIndex;	//index of output stream
		uint32 UnpackIndex;	//index of coder that provides input

		bool	read(Reader &file) {
			PackIndex   = file.ReadNum();
			UnpackIndex = file.ReadNum();
			return true;
		}
	};

	dynamic_array<CoderInfo>	Coders;
	dynamic_array<Binding>		Bonds;
	dynamic_array<uint32>		PackStreams;

	uint32	crc					= 0;
	uint32	FirstPackStream		= 0;
	uint32	NumUnpackStreams	= 1;
	uint32	MainStream;

	int			FindPackStreamIndex(uint32 packStream) const {
		for (auto &i : PackStreams)
			if (i == packStream)
				return PackStreams.index_of(i);
		return -1;
	}

	const Binding* FindBond(uint32 packStream) const {
		for (auto &i : Bonds)
			if (i.PackIndex == packStream)
				return &i;
		return 0;
	}

	bool	read(Reader &file);
	bool	Decode(istream_ref in, ostream_ref out, streamptr start, const uint64 *starts) const;
};

bool Folder::read(Reader &file) {
	uint32 numCoders = file.ReadNum();
	if (numCoders == 0 || numCoders > kMaxCodersPerFolder)
		return false;

	if (!Coders.read(file, numCoders))
		return false;

	uint32 numInStreams = 0;
	for (auto &coder : Coders)
		numInStreams += coder.NumStreams;

	uint32 numBonds = numCoders - 1;
	Bonds.read(file, numBonds);

	dynamic_bitarray<uint32>	StreamUsed(numInStreams, false);
	dynamic_bitarray<uint32>	CoderUsed(numCoders, false);
	for (auto &bond : Bonds) {
		if (bond.PackIndex >= numInStreams|| StreamUsed[bond.PackIndex] || bond.UnpackIndex >= numCoders || CoderUsed[bond.UnpackIndex])
			return false;
		StreamUsed[bond.PackIndex] = true;
		CoderUsed[bond.UnpackIndex] = true;
	}

	MainStream	= CoderUsed.lowest(false);

	uint32 numPackStreams = numInStreams - numBonds;
	PackStreams.resize(numPackStreams);

	if (numPackStreams == 1) {
		PackStreams[0] = StreamUsed.lowest(false);
		return PackStreams[0] < numInStreams;
	}

	for (auto &i : PackStreams) {
		i = file.ReadNum();
		if (i >= numInStreams || StreamUsed[i])
			return false;
		StreamUsed[i] = true;
	}

	return true;
}

bool Folder::Decode(istream_ref in, ostream_ref out, streamptr start, const uint64 *starts) const {
	dynamic_array<malloc_block>	outputs(Coders.size());

	uint32	in_index = 0, out_index = 0;

	for (auto &coder : Coders) {
		auto filter = Filter::get(coder.MethodID);
		if (!filter)
			return false;

		auto&	dst	= outputs[out_index].create(coder.UnpackSize);
		++out_index;

		dynamic_array<malloc_block>			src0;
		dynamic_array<const_memory_block>	src(coder.NumStreams);

		for (int i = 0; i < coder.NumStreams; i++) {
			if (auto bond = FindBond(in_index)) {
				src[i]	= outputs[bond->UnpackIndex];

			} else {
				uint32	index	= FindPackStreamIndex(in_index);
				in.seek(start + starts[index]);
				src[i]	= src0.emplace_back(in, starts[index + 1] - starts[index]);
			}

			++in_index;
		}
//		++out_index;
//		in_index	+= coder.NumStreams;

		if (filter->is_filter())
			src[0].copy_to(dst);

		auto	*c = filter->get_decoder(coder.Props);
		if (!c->code(src[0], dst))
			return false;
		c->finish();
	}
	return out.write(outputs[MainStream]);
}

struct CFolders {
	dynamic_array<Folder>	Folders;
	dynamic_array<uint64>	PackStarts;

	bool	ReadSubStreamsInfo(Reader &file, dynamic_array<uint64>& unpackSizes, dynamic_array<uint32>& digests);
	bool	ReadStreamsInfo(Reader &file, const dynamic_array<malloc_block>* dataVector, uint64& dataOffset, dynamic_array<uint64>& unpackSizes, dynamic_array<uint32>& digests);
};

bool CFolders::ReadSubStreamsInfo(Reader& file, dynamic_array<uint64>& unpackSizes, dynamic_array<uint32>& digests) {
	NID type;

	for (;;) {
		type = file.ReadID();
		if (type == NID::NumUnpackStream) {
			for (auto &i : Folders)
				i.NumUnpackStreams = file.ReadNum();
			continue;
		}
		if (type == NID::CRC || type == NID::Size || type == NID::End)
			break;
		file.SkipData();
	}

	if (type == NID::Size) {
		for (auto &i : Folders) {
			if (i.NumUnpackStreams) {
				uint64 sum = 0;
				for (uint32 j = 1; j < i.NumUnpackStreams; j++) {
					uint64 size = file.ReadNumber();
					unpackSizes.push_back(size);
					sum += size;
				}
				uint64 folderUnpackSize = i.Coders[i.MainStream].UnpackSize;
				if (folderUnpackSize < sum)
					return false;
				
				unpackSizes.push_back(folderUnpackSize - sum);
			}
		}
		type = file.ReadID();

	} else {
		for (auto &i : Folders) {
			if (i.NumUnpackStreams > 1)
				return false;
			if (i.NumUnpackStreams == 1)
				unpackSizes.push_back(i.Coders[0].UnpackSize);
		}
	}

	uint32 numDigests = 0;
	for (auto &i : Folders) {
		if (i.NumUnpackStreams != 1 || i.crc == 0)
			numDigests += i.NumUnpackStreams;
	}

	while (type != NID::End) {
		if (type == NID::CRC) {
			dynamic_bitarray<uint32> digests2;
			ReadBoolVector2(file, numDigests, digests2);

			digests.resize(unpackSizes.size());

			uint32 k	= 0;
			uint32 k2	= 0;
			for (auto &i : Folders) {
				if (i.NumUnpackStreams == 1 && i.crc) {
					digests[k++] = i.crc;
				} else {
					for (uint32 j = 0; j < i.NumUnpackStreams; j++)
						digests[k++] = digests2[k2++] ? file.get<uint32>() : 0;
				}
			}
		} else {
			file.SkipData();
		}

		type = file.ReadID();
	}

	if (digests.size() != unpackSizes.size()) {
		digests.resize(unpackSizes.size());
		uint32 k = 0;
		for (auto &i : Folders) {
			if (i.NumUnpackStreams == 1 && i.crc) {
				digests[k++] = i.crc;
			} else {
				for (uint32 j = 0; j < i.NumUnpackStreams; j++)
					digests[k++] = 0;
			}
		}
	}
	return true;
}

bool CFolders::ReadStreamsInfo(Reader &file, const dynamic_array<malloc_block>* dataVector, uint64& dataOffset, dynamic_array<uint64>& unpackSizes, dynamic_array<uint32>& digests) {
	auto type = file.ReadID();

	// PackInfo

	if (type == NID::PackInfo) {
		dataOffset = file.ReadNumber();

		uint32 numPackStreams = file.ReadNum();
		PackStarts.resize(numPackStreams + 1);

		if (!file.WaitId(NID::Size))
			return false;

		uint64 sum		 = 0;
		for (int i = 0; i < numPackStreams; i++) {
			PackStarts[i]		= sum;
			uint64 packSize	   = file.ReadNumber();
			sum += packSize;
		}
		PackStarts[numPackStreams] = sum;

		if (!file.WaitId(NID::End))
			return false;

		type = file.ReadID();
	}

	// UnpackInfo

	if (type == NID::UnpackInfo) {
		if (!file.WaitId(NID::Folder))
			return false;

		uint32	numFolders = file.ReadNum();

		bool	r;
		if (file.getc()) {
			Reader	file2((*dataVector)[file.ReadNum()]);
			r = Folders.read(file2, numFolders);
		} else {
			r = Folders.read(file, numFolders);
		}

		if (!r || !file.WaitId(NID::CodersUnpackSize))
			return false;

		for (auto &f : Folders) {
			for (auto &c : f.Coders)
				c.UnpackSize = file.ReadNumber();
		}

		while ((type = file.ReadID()) != NID::End) {
			if (type == NID::CRC) {
				dynamic_bitarray<uint32>	v;
				ReadBoolVector2(file, Folders.size(), v);
				for (auto i : v.where(true))
					file.read(Folders[i].crc);
				continue;
			}
			file.SkipData();
		}

		uint32	pack	= 0;
		for (auto &folder : Folders) {
			folder.FirstPackStream	= pack;
			pack	+= folder.PackStreams.size();

			if (folder.PackStreams.size() > PackStarts.size())
				return false;
		}

		type = file.ReadID();
	}

	// SubStreamsInfo

	if (type == NID::SubStreamsInfo) {
		ReadSubStreamsInfo(file, unpackSizes, digests);
		type = file.ReadID();

	} else {
		for (auto &f : Folders)
			unpackSizes.push_back(f.Coders[f.MainStream].UnpackSize);
	}

	return type == NID::End;
}

//-----------------------------------------------------------------------------
// Filters
//-----------------------------------------------------------------------------

struct CopyFilter	: FilterT<0x00> {
	struct Decoder : Coder {
		bool	code(const_memory_block in, memory_block out)	override {
			in.copy_to(out);
			return true;
		}
	};
	Coder*	get_decoder(const_memory_block props)	override { return new Decoder; }
} copy;

struct DeltaFilter	: FilterT<0x03, Filter::IsFilter> {
	struct CoderBase : Coder {
		enum	{ MAX_DELTA = 256 };
		uint8	state[MAX_DELTA];
		uint32	delta;
		CoderBase(const uint8 *props) : delta(*props) { clear(state); }
	};
	struct Decoder : CoderBase {
		Decoder(const uint8 *props) : CoderBase(props) {}
		bool	code(const_memory_block in, memory_block out)	override {
			auto	delta = this->delta;
			uint8	buf[MAX_DELTA];
			uint32	j = 0;
			memcpy(buf, state, delta);
			for (uint8 *p = out, *e = out.end(); p < e;) {
				for (j = 0; j < delta && p < e; p++, j++)
					buf[j] = *p = uint8(buf[j] + *p);
			}
			if (j == delta)
				j = 0;
			memcpy(state, buf + j, delta - j);
			memcpy(state + delta - j, buf, j);
			return true;
		}

	};
	struct Encoder : CoderBase {
		Encoder(const uint8 *props) : CoderBase(props) {}

		bool	code(const_memory_block in, memory_block out)	override {
			auto	delta = this->delta;
			uint8	buf[MAX_DELTA];
			uint32	j = 0;
			memcpy(buf, state, delta);
			for (uint8 *p = out, *e = out.end(); p < e;) {
				for (j = 0; j < delta && p < e; p++, j++) {
					uint8 b	= *p;
					*p		= uint8(b - buf[j]);
					buf[j]	= b;
				}
			}
			if (j == delta)
				j = 0;
			memcpy(state, buf + j, delta - j);
			memcpy(state + delta - j, buf, j);
			return true;
		}
	};
	Coder*	get_decoder(const_memory_block props)	override { return new Decoder(props); }
	Coder*	get_encoder(const_memory_block props)	override { return new Encoder(props); }
} delta;

struct LZMA2Filter	: FilterT<0x21	> {
	struct Decoder : Coder {
		virtual bool	code(const_memory_block in, memory_block out)	override {
			lzma::Decoder2	decoder;
			uint8*	out1	= out;
			return !!decoder.process(out1, out.end(), in, in.end(), TRANSCODE_NONE);
		}
	};
	virtual Coder*	get_decoder(const_memory_block props)	override { return new Decoder; }
} lzma2;

struct SWAP2Filter	: FilterT<0x20302, Filter::IsFilter> {};
struct SWAP4Filter	: FilterT<0x20304, Filter::IsFilter> {};

struct LZMAFilter	: FilterT<0x30101> {
	struct Decoder : Coder {
		const_memory_block	props;
		Decoder(const_memory_block props) : props(props) {}
		virtual bool	code(const_memory_block in, memory_block out)	override {
			lzma::Decoder decoder(lzma::Decoder::Props(((const uint8*)props)[0]));
			uint8*	out1	= out;
			return !!decoder.process(out1, out.end(), in, in.end(), TRANSCODE_NONE);
		}
	};
	virtual Coder*	get_decoder(const_memory_block props)	override { return new Decoder(props); }
} lzma;

struct PPMDFilter	: FilterT<0x30401> {};
struct DeflateFilter: FilterT<0x40108> {};
struct BZip2Filter	: FilterT<0x40202> {};

struct BCJFilter	: FilterT<0x3030103, Filter::IsFilter> {
	struct Decoder : Coder {
		uint32	mask = 0;
		virtual bool	code(const_memory_block in, memory_block out)	override {
			mask = branch::BCJ_Convert(out, 0, mask, false);
			return true;
		}
	};
	virtual Coder*	get_decoder(const_memory_block props)	override { return new Decoder; }
} branch;

struct BCJ2Filter : FilterT<0x303011B, Filter::IsFilter> {
	struct Decoder : Coder {
		enum {
			STREAM_MAIN,
			STREAM_CALL,
			STREAM_JUMP,
			STREAM_RC,
			NUM_STREAMS,
		};

		const uint8	*bufs[NUM_STREAMS], *buf_ends[NUM_STREAMS];
		uint32		ip;
		uint8		prev;
		uint32		range;
		uint32		codeval;
		uint16		probs[2 + 256];

		typedef lzma::prob_decoder<11, uint32, byte_reader, false> prob_decoder1;
		typedef	prob_decoder1::prob_t	prob_t;

		Decoder() {
			ip		= 0;
			prev	= 0;
			range	= 0;
			codeval	= 0;
			for (auto &i : probs)
				i = prob_decoder1::half;
		}

		virtual bool	code(const_memory_block in, memory_block out)	override {
			return true;

			prob_decoder1	probdec(bufs[STREAM_RC], range, codeval);
			probdec.init();

			const uint8* src		= bufs[STREAM_MAIN];
			const uint8* src_end	= buf_ends[STREAM_MAIN];

			uint8	*dst			= out;
			uint8	*dst_end		= out.end();

			while (src != src_end) {
				auto	*src0 = src;

				if (prev == 0x0F && (src[0] & 0xF0) == 0x80) {
					*dst = src[0];

				} else {
					for (;;) {
						uint8 b = *src;
						*dst  = b;

						if (b != 0x0F) {
							if ((b & 0xFE) == 0xE8)
								break;
							dst++;
							if (++src == src_end)
								break;
						} else {
							dst++;
							if (++src == src_end)
								break;
							if ((*src & 0xF0) == 0x80) {
								*dst = *src;
								break;
							}
						}
					}
				}

				if (src != src0)
					prev = src[-1];

				uint8   b	= *src++;
				auto	num	= src - src0;
				ip		+= num;
				dst		+= num;

				if (probdec.bit(probs + (b == 0xE8 ? prev + 2 : int(b == 0xE9)))) {
					uint32	j	= b == 0xE8 ? STREAM_CALL : STREAM_JUMP;
					uint32	val	= load_packed<uint32be>(bufs[j]);
					bufs[j]	+= 4;

					ip		+= 4;
					val		-= ip;
					store_packed<uint32le>(dst, val);
					prev	= uint8(val >> 24);
					dst		+= 4;
				}
			}

			return true;
		}
	};
	virtual Coder*	get_decoder(const_memory_block props)	override { return new Decoder; }
} bcj2;

struct PPCFilter	: FilterT<0x3030205, Filter::IsFilter> {
	struct Decoder : Coder {
		virtual bool	code(const_memory_block in, memory_block out)	override {
			branch::PPC_Convert(out, 0, false);
			return true;
		}
	};
	virtual Coder*	get_decoder(const_memory_block props)	override { return new Decoder; }
} ppc;

struct IA64Filter	: FilterT<0x3030401, Filter::IsFilter> {
	struct Decoder : Coder {
		virtual bool	code(const_memory_block in, memory_block out)	override {
			branch::Itanium_Convert(out, 0, false);
			return true;
		}
	};
	virtual Coder*	get_decoder(const_memory_block props)	override { return new Decoder; }
} ia64;

struct ARMFilter	: FilterT<0x3030501, Filter::IsFilter> {
	struct Decoder : Coder {
		virtual bool	code(const_memory_block in, memory_block out)	override {
			branch::ARM_Convert(out, 0, false);
			return true;
		}
	};
	virtual Coder*	get_decoder(const_memory_block props)	override { return new Decoder; }
} arm;

struct ARMTFilter	: FilterT<0x3030701, Filter::IsFilter> {
	struct Decoder : Coder {
		virtual bool	code(const_memory_block in, memory_block out)	override {
			branch::ARMT_Convert(out, 0, false);
			return true;
		}
	};
	virtual Coder*	get_decoder(const_memory_block props)	override { return new Decoder; }
} armt;

struct SPARCFilter	: FilterT<0x3030805, Filter::IsFilter> {
	struct Decoder : Coder {
		virtual bool	code(const_memory_block in, memory_block out)	override {
			branch::SPARC_Convert(out, 0, false);
			return true;
		}
	};
	virtual Coder*	get_decoder(const_memory_block props)	override { return new Decoder; }
} sparc;

struct AESFilter	: FilterT<0x6F10701, Filter::Encryption> {};

//-----------------------------------------------------------------------------
// Archive
//-----------------------------------------------------------------------------

bool ReadAndDecodePackedStreams(istream_ref file, Reader &reader, uint64 baseOffset, uint64& dataOffset, dynamic_array<malloc_block>& dataVector) {
	CFolders				folders;
	dynamic_array<uint64>	unpackSizes;
	dynamic_array<uint32>	digests;

	folders.ReadStreamsInfo(reader, NULL, dataOffset, unpackSizes, digests);

	for (auto &i : folders.Folders) {
		uint64		 unpackSize = i.Coders[i.MainStream].UnpackSize;
		malloc_block& data		= dataVector.push_back(unpackSize);

		if (!i.Decode(file, memory_writer(data), baseOffset + dataOffset, folders.PackStarts + i.FirstPackStream))
			return false;

		if (i.crc && CRC32::calc(data, data.length()) != i.crc)
			return false;
	}

	return true;
}

struct Archive : public CFolders {
	struct File {
		uint64	Size		= 0;
		uint32	Crc			= 0;
		const char16 *name;
		bool	HasStream;	 // Test it !!! it means that there is stream in some folder. It can be empty stream
		bool	IsDir		= false;
		bool	IsAnti		= false;
		uint32	folder		= kNumNoIndex;
	};

	dynamic_array<File>		Files;
	MaskedVector<uint64>	CTime;
	MaskedVector<uint64>	ATime;
	MaskedVector<uint64>	MTime;
	MaskedVector<uint64>	StartPos;
	MaskedVector<uint32>	Attrib;

	malloc_block			NamesBuf;
	dynamic_array<uint32>	FolderStartFileIndex;

	uint64	DataStartPosition;
	uint64	DataStartPosition2;
	bool	header_error		= false;
	bool	unsupported_feature	= false;

	bool	Read(istream_ref file, const_memory_block next);
};

bool Archive::Read(istream_ref file, const_memory_block next) {
	Reader		reader(next);

	dynamic_array<malloc_block> dataVector;

	auto type = reader.ReadID();

	if (type == NID::EncodedHeader) {
		return ReadAndDecodePackedStreams(file, reader, sizeof(SignatureHeader), DataStartPosition2, dataVector)
			&& Read(file, dataVector[0]);
	}

	if (type != NID::Header)
		return false;

	type = reader.ReadID();

	if (type == NID::ArchiveProperties) {
		reader.SkipData();
		type = reader.ReadID();
	}

	if (type == NID::AdditionalStreamsInfo) {
		if (!ReadAndDecodePackedStreams(file, reader, sizeof(SignatureHeader), DataStartPosition2, dataVector))
			return false;
		DataStartPosition2 += sizeof(SignatureHeader);
		type = reader.ReadID();
	}

	dynamic_array<uint64>	unpackSizes;
	dynamic_array<uint32>	digests;

	if (type == NID::MainStreamsInfo) {
		ReadStreamsInfo(reader, &dataVector, DataStartPosition, unpackSizes, digests);
		DataStartPosition += sizeof(SignatureHeader);
		type = reader.ReadID();
	}

	if (type == NID::FilesInfo) {
		dynamic_bitarray<uint32> emptyStreamVector;
		dynamic_bitarray<uint32> emptyFileVector;
		dynamic_bitarray<uint32> antiFileVector;

		uint32	numFiles		= reader.ReadNum();
		uint32	numEmptyStreams	= 0;

		Files.resize(numFiles);

		while ((type = reader.ReadID()) != NID::End) {
			uint64	size	= reader.ReadNumber();
			auto	skip	= make_skip_size(reader, size);
			switch (type) {
				case NID::Name: {
					if (reader.getc())
						NamesBuf = dataVector[reader.ReadNum()];
					else
						NamesBuf.read(reader, size - 1);

					char16	*p0 = NamesBuf, *p = p0, *e = NamesBuf.end();
					for (auto &i : Files) {
						i.name	= p;

						while (p < e && *p)
							++p;
						if (p == e)
							return false;//ThrowEndOfData();
						++p;
					}
					if (p != e)
						header_error = true;
					break;
				}

				case NID::WinAttrib:
					Attrib.Read(reader, dataVector, numFiles);
					break;

				case NID::EmptyStream: {
					ReadBoolVector(reader, numFiles, emptyStreamVector);
					numEmptyStreams = emptyStreamVector.count_set();
					break;
				}
				case NID::EmptyFile:	ReadBoolVector(reader, numEmptyStreams, emptyFileVector); break;
				case NID::Anti:			ReadBoolVector(reader, numEmptyStreams, antiFileVector); break;
				case NID::StartPos:		StartPos.Read(reader, dataVector, numFiles); break;
				case NID::CTime:		CTime.Read(reader, dataVector, numFiles); break;
				case NID::ATime:		ATime.Read(reader, dataVector, numFiles); break;
				case NID::MTime:		MTime.Read(reader, dataVector, numFiles); break;

				case NID::Dummy:
					while (size--) {
						if (reader.getc())
							header_error = true;
					}
					break;

				default:
					unsupported_feature = true;
					break;
			}
		}

		type = reader.ReadID();  // Read (NID::End) end of headers
		if (type != NID::End)
			return false;

		if (numFiles - numEmptyStreams != unpackSizes.size())
			return false;

		uint32 sizeIndex		= 0;
		uint32 emptyFileIndex	= 0;
		for (uint32 i = 0; i < numFiles; i++) {
			File& file = Files[i];
			if (i >= numEmptyStreams || !emptyStreamVector[i]) {
				file.HasStream	= true;
				file.Size		= unpackSizes[sizeIndex];
				file.Crc		= digests[sizeIndex];
				sizeIndex++;
			} else {
				file.HasStream = false;
				file.IsDir	   = !(emptyFileIndex < emptyFileVector.size() && emptyFileVector[emptyFileIndex]);
				file.IsAnti	   = emptyFileIndex < antiFileVector.size() && antiFileVector[emptyFileIndex];
				emptyFileIndex++;
			}
		}
	}

	FolderStartFileIndex.resize(Folders.size());

	uint32	 folderIndex   = 0;
	uint32	 indexInFolder = 0;

	for (uint32 i = 0; i < Files.size(); i++) {
		bool emptyStream = !Files[i].HasStream;
		if (indexInFolder == 0) {
			if (emptyStream)
				continue;

			for (;;) {
				if (folderIndex >= Folders.size())
					return false;
				FolderStartFileIndex[folderIndex] = i;
				if (Folders[folderIndex].NumUnpackStreams != 0)
					break;
				folderIndex++;
			}
		}
		Files[i].folder	= folderIndex;

		if (!emptyStream) {
			if (++indexInFolder >= Folders[folderIndex].NumUnpackStreams) {
				folderIndex++;
				indexInFolder = 0;
			}
		}
	}

	if (indexInFolder) {
		header_error = true;
		folderIndex++;
	}

	while (folderIndex < Folders.size()) {
		FolderStartFileIndex[folderIndex] = Files.size();
		if (Folders[folderIndex].NumUnpackStreams)
			header_error = true;
		folderIndex++;
	}
	return true;
}

} // namespace sevenz

//-----------------------------------------------------------------------------
// FileHandler
//-----------------------------------------------------------------------------

class SevenZFileHandler : public FileHandler {
	const char*		GetExt() override { return "7z"; }
	int				Check(istream_ref file) override {
		using namespace sevenz;
		file.seek(0);
		SignatureHeader	h;
		return file.read(h) && h.valid() ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}

	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		using namespace sevenz;
		SignatureHeader	h;
		if (!file.read(h) || !h.valid())
			return ISO_NULL;

		file.seek_cur(h.next.NextHeaderOffset);
		malloc_block	next(file, h.next.NextHeaderSize);
		if (CRC32::calc(next, next.length()) != h.next.NextHeaderCRC)
			return ISO_NULL;

		Archive		arc;
		if (!arc.Read(file, next))
			return ISO_NULL;


		dynamic_array<malloc_block>	folder_data;

		for (auto &i : arc.Folders) {
			uint64		 unpackSize = i.Coders[i.MainStream].UnpackSize;
			auto& data	= folder_data.push_back(unpackSize);

			if (!i.Decode(file, memory_writer(data), arc.DataStartPosition, arc.PackStarts + i.FirstPackStream))
				return ISO_NULL;

			if (i.crc && CRC32::calc(data, data.length()) != i.crc)
				return ISO_NULL;
		}
//		return MakePtr(id, folder_data);

		ISO_ptr<iso::Folder>	p(id);
		uint64	offset = 0;
		uint32	prev_folder = 0;
		for (auto& i : arc.Files) {
			if (i.folder != kNumNoIndex) {
				if (i.folder != prev_folder) {
					prev_folder = i.folder;
					offset = 0;
				}
#if 0
				p->Append(ISO::MakePtr(string(i.name), malloc_block(folder_data[i.folder].slice(offset, i.Size))));
#else
				string		path	= i.name;
				const char	*name;
				GetDir(p, path, &name)->Append(ISO::MakePtr(name, malloc_block(folder_data[i.folder].slice(offset, i.Size))));
#endif
			} else {
//				p->Append(ISO::MakePtr(string(i.name), i.Size));
			}
			offset += i.Size;
		}
		return p;
	}
} _7z;
