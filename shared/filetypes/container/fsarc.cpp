#include "base/vector.h"
#include "iso/iso_files.h"
#include "filetypes/bitmap/bitmap.h"
#include "archive_help.h"

using namespace iso;

class fsARCFileHandler : public FileHandler {

	typedef int8		fsS8;
	typedef int16		fsS16;
	typedef int32		fsS32;
	typedef int64		fsS64;
	typedef long		fsSLong;
	typedef	uint8		fsU8;
	typedef	uint16		fsU16;
	typedef	uint32		fsU32;
	typedef	uint64		fsU64;
	typedef	ulong		fsULong;
	typedef fsU16		fsF16;
	typedef float		fsF32;
	typedef double		fsF64;

	/// Archive File Header
	struct fsHeader {
		enum EnumFsHeader {
			eFsHeader_HeaderValidation = 0x00bff5a7, //* fsar bf(me) 00 (LITTLE_ENDIAN)
		};

		fsU32	Validation;
		fsU16	TocVersion;
		fsU16	TocEntrySize;
		fsU32	TocEntryCount;
		fsU8	Reserved[64 - 4 * 3];
	};

	/// Table of Contents Entry
	struct fsTocEntry {
		enum EnumFsTocFileFlags {
			eFsTocFileFlags_CompressedZlib = 1 << 0, //* File compressed with zlib
		};

		fsU32	FileOffset;
		fsU32	FileLength;
		fsU32	FileFlag;
		char	FileName[128 - 4 * 3];

		fsTocEntry() { clear(*this); }
	};

	const char*		GetExt() override { return "fsa"; }

	int				Check(istream_ref file) override {
		file.seek(0);
		return file.get<fsU32>() == fsHeader::eFsHeader_HeaderValidation ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}

	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		fsHeader	header;
		file.read(header);
		if (header.Validation != fsHeader::eFsHeader_HeaderValidation)
			return ISO_NULL;

		fsTocEntry	*toc = new fsTocEntry[header.TocEntryCount];
		readn(file, toc, header.TocEntryCount);

		streamptr	start = file.tell();

		ISO_ptr<anything>	p(id);
		for (int i = 0; i < header.TocEntryCount; i++) {
			file.seek(start + toc[i].FileOffset);
			p->Append(ReadRaw(toc[i].FileName, file, toc[i].FileLength));
		}

		return p;
	}
} fsARC;

class BCIFileHandler : public FileHandler {

	struct BCIImageInfo {
		enum EnumFlags {
			eFlags_RLECompressed	= 0x00000001,
			eFlags_HasPixelMask		= 0x00000002,
			eFlags_UsesAlphaBlend	= 0x00000002,
		};

		char	m_Marker[4];
		uint16	m_uWidth;
		uint16	m_uHeight;
		uint32	m_uBufferSize;
		uint32	m_uFlags;
	};

	const char*		GetExt() override { return "bci"; }

	int				Check(istream_ref file) override {
		file.seek(0);
		return file.get<uint32be>() == 'BCI ' ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}

	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		BCIImageInfo	header;
		file.read(header);

		if (strncmp(header.m_Marker, "BCI ", 4) != 0)
			return ISO_NULL;

		malloc_block	buffer(header.m_uBufferSize);
		auto	read	= file.readbuff(buffer, header.m_uBufferSize);

		if (header.m_uFlags & BCIImageInfo::eFlags_RLECompressed) {
			// Perform RLE Decompression
			malloc_block	buffer2(header.m_uBufferSize);
			for (uint8 *s = (uint8*)buffer, *d = (uint8*)buffer2, *e = d + header.m_uBufferSize; d < e;) {
				uint8	fill	= *s++;
				uint32	run		= *s++;
				if (run == 255)
					run *= *s++;

				while (run--)
					*d++ = fill;
			}
			swap(buffer, buffer2);
		}

		ISO_ptr<bitmap>	bm(id);
#if 1
		ISO_rgba	*p		= bm->Create(header.m_uWidth * 2, header.m_uHeight);
		ISO_rgba	*end	= p + header.m_uWidth * 2 * header.m_uHeight;

		for (uint8 *s = (uint8*)buffer; p < end; s++) {
			*p++ = *s & 15;
			*p++ = *s >> 4;
		}
#else
		ISO_rgba	*p		= bm->Create(header.m_uWidth, header.m_uHeight);
		ISO_rgba	*end	= p + header.m_uWidth * header.m_uHeight;
		for (uint8 *s = (uint8*)buffer; p < end;)
			*p++ = *s++;
#endif
#if 1
		p = bm->CreateClut(16);
		ISO_rgba	col1(0xff,0xff,0xff), col2(0xcc,0xcc,0xcc);
		p[0] = ISO_rgba(0,0,0,0);
		p[1] = p[2] = p[3] = p[4] = p[5] = col1;
		p[6] = p[7] = p[8] = p[9] = p[10] = col2;
		p[2].a = p[7].a = 0xcc;
		p[3].a = p[8].a = 0x99;
		p[4].a = p[9].a = 0x66;
		p[5].a = p[10].a = 0x33;
		p[11] = lerp(col1, col2, 1.0f/6.0f);
		p[12] = lerp(col1, col2, 2.0f/6.0f);
		p[13] = lerp(col1, col2, 3.0f/6.0f);
		p[14] = lerp(col1, col2, 4.0f/6.0f);
		p[15] = lerp(col1, col2, 5.0f/6.0f);
#else
		int	maxi = 0, mini = 255;
		for (int n = header.m_uWidth * header.m_uHeight; n--; ) {
			int	i = (--p)->r;
			maxi = max(maxi, i);
			mini = min(mini, i);
		}

		p = bm->CreateClut(maxi + 1);
		maxi += int(maxi == mini);
		for (int i = mini; i <= maxi; i++)
			p[i] = (i - mini) * 255 / (maxi - mini);
#endif
		return bm;
	}

} bci;