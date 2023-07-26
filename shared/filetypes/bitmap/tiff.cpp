#include "tiff.h"
#include "bitmapfile.h"
#include "codec/lzw.h"
#include "codec/fax3.h"
#include "codec/apple_compression.h"
#include "codec/codec_stream.h"

//-----------------------------------------------------------------------------
//	Tag Image File Format
//-----------------------------------------------------------------------------

namespace iso {
	inline uint32	abs(uint32 x) { return x; }
}

namespace tiff {

template<bool BE>	static constexpr uint16 header	= 0x4949;
template<> static constexpr uint16 header<true>	= 0x4D4D;

enum class FILETYPE : uint16 {
	REDUCEDIMAGE			= 0x1,		// reduced resolution version
	PAGE					= 0x2,		// one page of many
	MASK					= 0x4,		// transparency mask
};
enum class OFILETYPE : uint16 {
	IMAGE					= 1,		// full resolution image data
	REDUCEDIMAGE			= 2,		// reduced size image data
	PAGE					= 3,		// one page of many
};
enum class COMPRESSION : uint16 {
	NONE					= 1,		// dump mode
	CCITTRLE				= 2,		// CCITT modified Huffman RLE
	CCITTFAX3				= 3,		// CCITT Group 3 fax encoding
	CCITT_T4				= 3,		// CCITT T.4 (TIFF 6 name)
	CCITTFAX4				= 4,		// CCITT Group 4 fax encoding
	CCITT_T6				= 4,		// CCITT T.6 (TIFF 6 name)
	LZW						= 5,		// Lempel-Ziv	& Welch
	OJPEG					= 6,		// !6.0 JPEG
	JPEG					= 7,		// JPEG DCT compression
	ADOBE_DEFLATE			= 8,		// Deflate compression, as recognized by Adobe
	T85						= 9,		// TIFF/FX T.85 JBIG compression
	T43						= 10,		// TIFF/FX T.43 colour by layered JBIG compression
	NEXT					= 32766,	// NeXT 2-bit RLE
	CCITTRLEW				= 32771,	// #1 w/ word alignment
	PACKBITS				= 32773,	// Macintosh RLE
	THUNDERSCAN				= 32809,	// ThunderScan RLE
	// codes 32895-32898 are reserved for ANSI IT8 TIFF/IT <dkellyapago.com)
	IT8CTPAD				= 32895,		// IT8 CT w/padding
	IT8LW					= 32896,		// IT8 Linework RLE
	IT8MP					= 32897,		// IT8 Monochrome picture
	IT8BL					= 32898,		// IT8 Binary line art
	// compression codes 32908-32911 are reserved for Pixar
	PIXARFILM				= 32908,		// Pixar companded 10bit LZW
	PIXARLOG				= 32909,		// Pixar companded 11bit ZIP
	DEFLATE					= 32946,	// Deflate compression
	// compression code 32947 is reserved for Oceana Matrix <devoceana.com>
	DCS						= 32947,		// Kodak DCS encoding
	JBIG					= 34661,	// ISO JBIG
	SGILOG					= 34676,	// SGI Log Luminance RLE
	SGILOG24				= 34677,	// SGI Log 24-bit packed
	JP2000					= 34712,		// Leadtools JPEG2000
	LERC					= 34887,		// ESRI Lerc codec: https://github.com/Esri/lerc
	// compression codes 34887-34889 are reserved for ESRI
	LZMA					= 34925,	// LZMA2
	ZSTD					= 50000,	// ZSTD: WARNING not registered in Adobe-maintained registry
	WEBP					= 50001,	// WEBP: WARNING not registered in Adobe-maintained registry
};
enum class PHOTOMETRIC : uint16 {
	MINISWHITE				= 0,		// min value is white
	MINISBLACK				= 1,		// min value is black
	RGB						= 2,		// RGB color model
	PALETTE					= 3,		// color map indexed
	MASK					= 4,		// $holdout mask
	SEPARATED				= 5,		// !color separations
	YCBCR					= 6,		// !CCIR 601
	CIELAB					= 8,		// !1976 CIE L*a*b*
	ICCLAB					= 9,		// ICC L*a*b* [Adobe TIFF Technote 4]
	ITULAB					= 10,		// ITU L*a*b*
	CFA						= 32803,	// color filter array
	LOGL					= 32844,	// CIE Log2(L)
	LOGLUV					= 32845,	// CIE Log2(L) (u',v')
};
enum class THRESHHOLD : uint16 {
	BILEVEL					= 1,		// b&w art scan
	HALFTNE					= 2,		// or dithered scan
	ERRORDIFFUSE			= 3,		// usually floyd-steinberg
};
enum class FILLORDER : uint16 {
	MSB2LSB					= 1,		// most significant -> least
	LSB2MSB					= 2,		// least significant -> most
};
enum class ORIENTATION : uint16 {
	TOPLEFT					= 1,		// row 0 top, col 0 lhs
	TOPRIGHT				= 2,		// row 0 top, col 0 rhs
	BOTRIGHT				= 3,		// row 0 bottom, col 0 rhs
	BOTLEFT					= 4,		// row 0 bottom, col 0 lhs
	LEFTTOP					= 5,		// row 0 lhs, col 0 top
	RIGHTTOP				= 6,		// row 0 rhs, col 0 top
	RIGHTBOT				= 7,		// row 0 rhs, col 0 bottom
	LEFTBOT					= 8,		// row 0 lhs, col 0 bottom
};
enum class PLANARCONFIG : uint16 {
	CONTIG					= 1,		// single image plane
	SEPARATE				= 2,		// separate planes of data
};
enum class RESPONSEUNIT : uint16 {
	R10S					= 1,		// tenths of a unit
	R100S					= 2,		// hundredths of a unit
	R1000S					= 3,		// thousandths of a unit
	R10000S					= 4,		// ten-thousandths of a unit
	R100000S				= 5,		// hundred-thousandths
};
enum class GROUP3OPT : uint32 {
	NONE					= 0,
	ENCODING2D				= 0x1,		// 2-dimensional coding
	UNCOMPRESSED			= 0x2,		// data not compressed
	FILLBITS				= 0x4,		// fill to byte boundary
};
//enum class GROUP4OPT : uint32 {
//	UNCOMPRESSED			= 0x2,		// data not compressed
//};
enum class RESUNIT : uint16 {
	NONE					= 1,		// no meaningful units
	INCH					= 2,		// english
	CENTIMETER				= 3,		// metric
};
enum class PREDICTOR : uint16 {
	NONE					= 1,		// no prediction scheme used
	HORIZONTAL				= 2,		// horizontal differencing
	FLOATINGPOINT			= 3,		// floating point predictor
};
enum class CLEANFAXDATA : uint16 {
	CLEAN					= 0,		// no errors detected
	REGENERATED				= 1,		// receiver regenerated lines
	UNCLEAN					= 2,		// uncorrected errors exist
};
enum class INKSET : uint16 {
	CMYK					= 1,		// cyan-magenta-yellow-black color
	MULTIINK				= 2,		// multi-ink or hi-fi color
};
enum class EXTRASAMPLE : uint16 {
	UNSPECIFIED				= 0,		// unspecified data
	ASSOCALPHA				= 1,		// associated alpha data
	UNASSALPHA				= 2,		// unassociated alpha data
};
enum class SAMPLEFORMAT : uint16 {
	UINT					= 1,		// unsigned integer data
	INT						= 2,		// signed integer data
	IEEEFP					= 3,		// IEEE floating point data
	UNTYPED					= 4,		// untyped data
	COMPLEXINT				= 5,		// complex signed int
	COMPLEXIEEEFP			= 6,		// complex ieee floating
};
enum class PROFILETYPE : uint16 {
	UNSPECIFIED				= 0,
	G3_FAX					= 1,
};
enum class FAXPROFILE : uint16 {
	S						= 1,		// TIFF/FX FAX profile S
	F						= 2,		// TIFF/FX FAX profile F
	J						= 3,		// TIFF/FX FAX profile J
	C						= 4,		// TIFF/FX FAX profile C
	L						= 5,		// TIFF/FX FAX profile L
	M						= 6,		// TIFF/FX FAX profile LM
};
enum class CODINGMETHODS : uint16 {
	T4_1D					= (1 << 1),	// T.4 1D
	T4_2D					= (1 << 2),	// T.4 2D
	T6						= (1 << 3),	// T.6
	T85 					= (1 << 4),	// T.85 JBIG
	T42 					= (1 << 5),	// T.42 JPEG
	T43						= (1 << 6),	// T.43 colour by layered JBIG
};
enum class YCBCRPOSITION : uint16 {
	CENTERED		= 1,		// as in PostScript Level 2
	COSITED					= 2,		// as in CCIR 601-1
};

template<typename T, bool be, typename R>	T	get(R &reader) {
	return reader.template get<endian_t<T, be>>();
}
template<bool be, typename R>	int	get_int(R &reader, TYPE type) {
	switch (type) {
		case TYPE::BYTE:
		case TYPE::ASCII:	return reader.getc();
		case TYPE::SHORT:	return get<uint16, be>(reader);
		case TYPE::LONG:	return get<uint32, be>(reader);
		case TYPE::SBYTE:	return (int8)reader.getc();
		case TYPE::SSHORT:	return get<int16, be>(reader);
		case TYPE::SLONG:	return get<int32, be>(reader);
		default:			throw("unrecognised type");
	}
}
template<bool be, typename R> double	get_float(R &reader, TYPE type) {
	switch (type) {
		case TYPE::FLOAT:	return reader.template get<endian_t<float, be>>();
		case TYPE::DOUBLE:	return reader.template get<endian_t<double, be>>();
		default:			return get_int(reader, type);
	}
}

template<bool be, typename T, typename R>	 void	read(R &&reader, T &t, TYPE type, uint32 count) {
	t = (T)get_int<be>(reader, type);
}
template<bool be, typename T, typename R>	 void	read(R &&reader, float &t, TYPE type, uint32 count) {
	t = get_float<be>(reader, type);
}
template<bool be, typename T, typename R>	 void	read(R &&reader, rational<T> &t, TYPE type, uint32 count) {
	t.n = get<T, be>(reader);
	t.d = get<T, be>(reader);
}
template<bool be, typename T, typename R>	 void	read(R &&reader, dynamic_array<T> &t, TYPE type, uint32 count) {
	t.resize(count);
	for (auto &i : t)
		read<be>(reader, i, type, 1);
}

template<bool be, typename T, typename W>	 void	write(W &&writer, const T &t) {
	writer.write((endian_t<T, be>)t);
}
template<bool be, typename T, typename W>	 void	write(W &&writer, const rational<T> &t) {
	write<be>(writer, t.n);
	write<be>(writer, t.d);
}
template<bool be, typename T, typename W>	 void	write(W &&writer, const dynamic_array<T> &t) {
	for (auto &i : t)
		writer.write((endian_t<T, be>)i);
}

class TIFF {
public:
	struct Reader {
		virtual bool	get_strip(TIFF*, istream_ref file, int length, const block<ISO_rgba, 2> &strip)	= 0;
		virtual void	end(TIFF*, istream_ref file) {}
	};
	struct Writer {
		virtual bool	put_strip(TIFF*, ostream_ref file, const block<ISO_rgba, 2> &strip)				= 0;
		virtual void	end(TIFF*, ostream_ref file) {}
	};

	struct Codec : static_list<Codec> {
		COMPRESSION		scheme;
		Codec(COMPRESSION scheme) : scheme(scheme) {}
		virtual Reader*	get_reader(TIFF*)				= 0;
		virtual Writer*	get_writer(TIFF*)				= 0;
	};
	template<COMPRESSION c> struct CodecT : Codec { CodecT() : Codec(c) {} };

	struct DE {
		TAG			tag;
		TYPE		type;
		uint32		count;
		union {
			uint32		pos;
			uint8		data[4];
		};

		bool	fits()		const { return type_length(type) * count <= 4; }

		template<bool be, typename R> void	load(R &reader) {
			tag		= (TAG)tiff::get<uint16, be>(reader);
			type	= (TYPE)tiff::get<uint16, be>(reader);
			count	= get<uint32, be>(reader);
			reader.read(data);
		}

		template<bool be, typename W> void	save(W &writer) const {
			write<be>(writer, (uint16)tag);
			write<be>(writer, (uint16)type);
			write<be>(writer, count);
			if (fits())
				writer.write(data);
			else
				write<be>(writer, pos);

		}

		template<bool be, typename T, typename W>	void init(W &writer, TAG _tag, const T &t) {
			tag		= _tag;
			type	= get_type<T>();
			count	= 1;
			if (fits()) {
				clear(data);
				tiff::write<be>(memory_writer(data), t);
			} else {
				pos = writer.tell32();
				tiff::write<be>(writer, t);
			}
		}
		template<bool be, typename T, typename W>	void init(W &writer, TAG _tag, const dynamic_array<T> &t) {
			tag		= _tag;
			type	= get_type<T>();
			count	= t.size32();
			if (fits()) {
				clear(data);
				tiff::write<be>(memory_writer(data), t);
			} else {
				pos = writer.tell32();
				tiff::write<be>(writer, t);
			}
		}

		template<bool be, typename T, typename R>	void read(R &reader, T &t) const {
			if (fits())
				return tiff::read<be>(memory_reader(data), t, type, count);
			reader.seek(*(endian_t<uint32, be>*)data);
			return tiff::read<be>(reader, t, type, count);
		}
	};

public:
	PHOTOMETRIC		photometric			= PHOTOMETRIC::MINISWHITE;
	COMPRESSION		compression			= COMPRESSION::NONE;
	PREDICTOR		predictor			= PREDICTOR::NONE;//PREDICTOR::HORIZONTAL;
	ORIENTATION		orientation			= ORIENTATION::TOPLEFT;
	PLANARCONFIG	planarconfig		= PLANARCONFIG::CONTIG;
	RESUNIT			resolution_unit		= RESUNIT::INCH;
	flags<GROUP3OPT>	group3opts;//			= GROUP3OPT::NONE;

	uint16			width				= 0;
	uint16			height				= 0;
	uint16			samplesperpixel		= 1;
	uint16			rowsperstrip		= 0;

	rational<uint32>			xresolution	= {3000000,10000};
	rational<uint32>			yresolution	= {3000000,10000};

	dynamic_array<int16>		bitspersample;
	dynamic_array<uint32>		stripoffsets;
	dynamic_array<uint32>		stripbytecounts;
	dynamic_array<uint16>		colormap;

	//read
	template<bool be, typename R>	void ReadDE(const DE &de, R &reader);
	bool		GetStrip(istream_ref file, const block<ISO_rgba, 2> &strip);

	//write
	bool		PutStrip(ostream_ref file, const block<ISO_rgba, 2> &strip);

	Codec* GetCodec() const {
		for (auto &i : Codec::all()) {
			if (i.scheme == compression)
				return &i;
		}
		return nullptr;
	}

public:
	template<bool be, typename R> bool ReadIFD(R &reader);
	ISO_ptr<void>	GetBitmap(tag id, istream_ref file);

	template<bool be> bool		WriteIFD(ostream_ref file, const bitmap *bm);
	void			SetBitmap(const bitmap *bm);

};
}

//-----------------------------------------------------------------------------
//	codecs
//-----------------------------------------------------------------------------

using namespace tiff;

struct TIFFCodecNONE	: TIFF::CodecT<COMPRESSION::NONE>, TIFF::Reader, TIFF::Writer	{
	TIFF::Reader*	get_reader(TIFF*) override { return this; }
	TIFF::Writer*	get_writer(TIFF*) override { return this; }

	bool	put_strip(TIFF *tiff, ostream_ref file, const block<ISO_rgba, 2> &strip) override {
		return tiff->PutStrip(file, strip);
	}
	bool	get_strip(TIFF *tiff, istream_ref file, int length, const block<ISO_rgba, 2> &strip) override {
		return tiff->GetStrip(file, strip);
	}
} codec_none;

struct TIFFCodecLZW		: TIFF::CodecT<COMPRESSION::LZW>, TIFF::Reader, TIFF::Writer	{
	TIFF::Reader*	get_reader(TIFF*) override { return this; }
	TIFF::Writer*	get_writer(TIFF*) override { return this; }

	bool	put_strip(TIFF *tiff, ostream_ref file, const block<ISO_rgba, 2> &strip) override {
		return tiff->PutStrip(make_codec_writer<0>(LZW_encoder<true, true>(8), file), strip);
		//return tiff->PutStrip(LZW_out<vlc_out<uint32, true>, true>(file, 8, 0x100000).me(), strip);
	}
	bool	get_strip(TIFF *tiff, istream_ref file, int length, const block<ISO_rgba, 2> &strip)  override{
		auto	data = transcode(LZW_decoder<true, true>(8), malloc_block(file, length));
		return tiff->GetStrip(memory_reader(data), strip);
	}
} codec_lzw;

struct TIFFCodecFAX		: TIFF::Codec, TIFF::Reader, TIFF::Writer {
	FAXMODE	mode;
	TIFFCodecFAX(COMPRESSION c, FAXMODE mode) : TIFF::Codec(c), mode(mode) {}
	TIFF::Reader*	get_reader(TIFF*) override { return this; }
	TIFF::Writer*	get_writer(TIFF*) override { return this; }

	bool	get_strip(TIFF *tiff, istream_ref file, int length, const block<ISO_rgba, 2> &strip) override {
		bitmatrix_aligned_own<uint32>	bits(strip.size<2>(), strip.size<1>());
		bool	ret = FaxDecode(malloc_block(file, length), bits, mode | (tiff->group3opts.test(GROUP3OPT::ENCODING2D) ? FAXMODE_2D : FAXMODE_CLASSIC));

		for (int y = 0; y < bits.num_rows(); y++) {
			auto	dest_row	= strip[y];
			auto	srce_row	= bits[y];
			for (int x = 0; x < bits.num_cols(); x++)
				dest_row[x] = srce_row[x] ? 0 : 255;
		}
		return ret;
	}

	bool	put_strip(TIFF *tiff, ostream_ref file, const block<ISO_rgba, 2> &strip) override {
		bitmatrix_aligned_own<uint32>	bits(strip.size<2>(), strip.size<1>());
		for (int y = 0; y < bits.num_rows(); y++) {
			auto	srce_row	= strip[y];
			auto	dest_row	= bits[y];
			for (int x = 0; x < bits.num_cols(); x++)
				dest_row[x] = srce_row[x].a >= 128;
		}

		malloc_block	mem(65536);
		int	length = FaxEncode(mem, bits, mode | (tiff->group3opts.test(GROUP3OPT::ENCODING2D) ? FAXMODE_2D : FAXMODE_CLASSIC));
		return file.writebuff(mem, length) == length;
	}
};

struct TIFFCodecCCITTFAX3		: TIFFCodecFAX	{
	TIFFCodecCCITTFAX3() : TIFFCodecFAX(COMPRESSION::CCITTFAX3, FAXMODE_CLASSIC) {}
} codec_fax3;

struct TIFFCodecCCITTRLE		: TIFFCodecFAX	{
	TIFFCodecCCITTRLE() : TIFFCodecFAX(COMPRESSION::CCITTRLE, FAXMODE_NORTC|FAXMODE_NOEOL|FAXMODE_BYTEALIGN) {}
} codec_faxrle;

struct TIFFCodecCCITTRLEW		: TIFFCodecFAX	{
	TIFFCodecCCITTRLEW() : TIFFCodecFAX(COMPRESSION::CCITTRLEW, FAXMODE_NORTC|FAXMODE_NOEOL|FAXMODE_WORDALIGN) {}
} codec_faxrlew;

struct TIFFCodecCCITTFAX4		: TIFFCodecFAX	{
	TIFFCodecCCITTFAX4() : TIFFCodecFAX(COMPRESSION::CCITTFAX4, FAXMODE_FIXED2D|FAXMODE_NOEOL) {}
} codec_fax4;

struct TIFFCodecPACKBITS		: TIFF::CodecT<COMPRESSION::PACKBITS>, TIFF::Reader, TIFF::Writer {
	TIFF::Reader*	get_reader(TIFF*) override { return this; }
	TIFF::Writer*	get_writer(TIFF*) override { return this; }

	bool	put_strip(TIFF *tiff, ostream_ref file, const block<ISO_rgba, 2> &strip) override {
		malloc_block	mem(65536);
		for (auto row : strip) {
			size_t	written;
			transcode(PackBits::encoder(), row.begin(), row.size() * 4, mem, mem.length(), &written);
			file.writebuff(mem, written);
		}
		return true;
	}
	bool	get_strip(TIFF *tiff, istream_ref file, int length, const block<ISO_rgba, 2> &strip) override {
		malloc_block	mem(file, length);
		uint8	*p = mem;
		for (auto row : strip) {
			size_t	written;
			auto read = transcode(PackBits::decoder(), p, mem.end() - p, row.begin(), row.size() * 4, &written);
			p += read;
		}
		return true;
	}
} codec_packbits;
/*
struct TIFFCodecTHUNDERSCAN		: TIFF::CodecT<TIFF::COMPRESSION::THUNDERSCAN>	{};
struct TIFFCodecNEXT			: TIFF::CodecT<TIFF::COMPRESSION::NEXT		>	{};
struct TIFFCodecJPEG			: TIFF::CodecT<TIFF::COMPRESSION::JPEG		>	{};
struct TIFFCodecOJPEG			: TIFF::CodecT<TIFF::COMPRESSION::OJPEG		>	{};
struct TIFFCodecJBIG			: TIFF::CodecT<TIFF::COMPRESSION::JBIG		>	{};
struct TIFFCodecDEFLATE			: TIFF::CodecT<TIFF::COMPRESSION::DEFLATE	>	{};
struct TIFFCodecADOBE_DEFLATE	: TIFF::CodecT<TIFF::COMPRESSION::ADOBE_DEFLATE>{};
struct TIFFCodecPIXARLOG		: TIFF::CodecT<TIFF::COMPRESSION::PIXARLOG	>	{};
struct TIFFCodecSGILOG			: TIFF::CodecT<TIFF::COMPRESSION::SGILOG	>	{};
struct TIFFCodecSGILOG24		: TIFF::CodecT<TIFF::COMPRESSION::SGILOG24	>	{};
struct TIFFCodecLZMA			: TIFF::CodecT<TIFF::COMPRESSION::LZMA		>	{};
struct TIFFCodecZSTD			: TIFF::CodecT<TIFF::COMPRESSION::ZSTD		>	{};
struct TIFFCodecWEBP			: TIFF::CodecT<TIFF::COMPRESSION::WEBP		>	{};
*/
//-----------------------------------------------------------------------------
//	read
//-----------------------------------------------------------------------------

template<bool be, typename R> void TIFF::ReadDE(const DE &de, R &reader) {
	switch (de.tag) {
		case TAG::SubfileType:		break;
		case TAG::OldSubfileType:	break;
		case TAG::ImageWidth:		de.read<be>(reader, width			);	break;
		case TAG::ImageHeight:		de.read<be>(reader, height			);	break;
		case TAG::BitsPerSample:	de.read<be>(reader, bitspersample	);	break;
		case TAG::Compression:		de.read<be>(reader, compression		);	break;
		case TAG::PhotometricInterpretation:de.read<be>(reader, photometric); break;
		case TAG::StripOffsets:		de.read<be>(reader, stripoffsets	);	break;
		case TAG::Orientation:		de.read<be>(reader, orientation		);	break;
		case TAG::SamplesPerPixel:	de.read<be>(reader, samplesperpixel ); 	break;
		case TAG::RowsPerStrip:		de.read<be>(reader, rowsperstrip	); 	break;
		case TAG::StripByteCounts:	de.read<be>(reader, stripbytecounts	);	break;
		case TAG::XResolution:		de.read<be>(reader, xresolution		);	break;
		case TAG::YResolution:		de.read<be>(reader, yresolution		);	break;
		case TAG::PlanarConfiguration:	de.read<be>(reader, planarconfig);	break;
		case TAG::Predictor:		de.read<be>(reader, predictor);			break;
		case TAG::ColorMap:			de.read<be>(reader, colormap);			break;
		case TAG::T4Options:
		case TAG::T6Options:		de.read<be>(reader, group3opts);		break;
	};
}

template<bool be, typename R> bool TIFF::ReadIFD(R &reader) {
	switch (get<uint16, be>(reader)) {
		case 42: {
			auto	start = get<uint32, be>(reader);
			if (!start)
				return false;

			reader.seek(start);

			dynamic_array<DE>	de(get<uint16, be>(reader));
			for (auto &i : de)
				i.load<be>(reader);

			for (auto &i : de) {
				try { ReadDE<be>(i, reader); } catch_all() { return false; }
			}
			return true;
		}
		default:
			return false;
	}
}

bool TIFF::GetStrip(istream_ref file, const block<ISO_rgba, 2> &strip) {
	for (auto row : strip) {
		bool	alpha	= samplesperpixel > 3;
		if (photometric == PHOTOMETRIC::RGB) {
			ISO_rgba	prev(0, 0, 0, 0);
			for (auto &x : row) {
				x.r = prev.r + (signed char)file.getc();
				x.g = prev.g + (signed char)file.getc();
				x.b = prev.b + (signed char)file.getc();
				x.a = alpha ? prev.a + (signed char)file.getc() : 255;
				if (predictor == PREDICTOR::HORIZONTAL)
					prev = x;

			}
		} else {
			int		bps		= bitspersample[0];
			auto	vlc		= make_vlc_in<uint32, true>(file);
			for (auto &x : row)
				x = vlc.get(bps);
		}
	}
	return true;
}

ISO_ptr<void> TIFF::GetBitmap(tag id, istream_ref file) {

	ISO_ptr<bitmap>	bm(id);
	bm->Create(width, height);

	if (samplesperpixel==1) {
		switch (photometric) {
			case PHOTOMETRIC::PALETTE: {
				int		n	= colormap.size32() / 3;
				auto	*p	= colormap.begin();
				bm->CreateClut(n);
				for (auto &i : bm->ClutBlock()) {
					i = {p[0], p[n], p[n * 2]};
					++p;
				}
				break;
			}
			case PHOTOMETRIC::MINISWHITE: {
				int		n	= 1 << bitspersample[0];
				int		x	= n;
				for (auto &i : bm->CreateClut(n))
					i = ISO_rgba((--x * 255) / (n - 1));
				break;
			}
			case PHOTOMETRIC::MINISBLACK: {
				int		n	= 1 << bitspersample[0];
				int		x	= 0;
				for (auto &i : bm->CreateClut(n))
					i = ISO_rgba((x++ * 255) / (n - 1));
				break;
			}
		}
	} else if (samplesperpixel > 3) {
	//	bm.SetFlag(BMF_ALPHA);
	}

	if (auto codec = GetCodec()) {
		auto	reader = codec->get_reader(this);
		for (int i = 0, y = 0; y < height; i++, y += rowsperstrip ) {
			file.seek(stripoffsets[i]);
			auto	block = bm->Block(0, y, width, rowsperstrip);
			reader->get_strip(this, file,  stripbytecounts[i], block);
		}
		reader->end(this, file);
	}
	return bm;
}

//-----------------------------------------------------------------------------
//	write
//-----------------------------------------------------------------------------

void TIFF::SetBitmap(const bitmap *bm) {
	photometric			= bm->IsPaletted() ? PHOTOMETRIC::PALETTE : bm->IsIntensity() ? PHOTOMETRIC::MINISBLACK : PHOTOMETRIC::RGB;
	samplesperpixel		= (bm->IsIntensity() ? 1 : 3) + int(bm->HasAlpha());
	width				= bm->Width();
	height				= bm->Height();
	rowsperstrip		= 0x1000 / width;
	bitspersample		= repeat(8, samplesperpixel);

	int		num_strips	= div_round_up(height, rowsperstrip);
	stripoffsets.resize(num_strips);
	stripbytecounts.resize(num_strips);

	if (samplesperpixel==1) {
		if (photometric == PHOTOMETRIC::PALETTE) {
			int	n = bm->ClutSize();
			dynamic_array<uint16>	colormap(n * 3);
			auto	p = colormap.begin();
			for (auto &i : bm->ClutBlock()) {
				p[n * 0]	= i.r;
				p[n * 1]	= i.g;
				p[n * 2]	= i.b;
				++p;
			}
		}
	}
}

bool TIFF::PutStrip(ostream_ref file, const block<ISO_rgba, 2> &strip) {
	for (auto row : strip) {
		bool	alpha	= samplesperpixel > 3;
		if (photometric == PHOTOMETRIC::RGB) {
			ISO_rgba	prev(0, 0, 0, 0);
			for (auto &x : row) {
				file.putc(x.r - prev.r);
				file.putc(x.g - prev.g);
				file.putc(x.b - prev.b);
				if (alpha)
					file.putc(x.a - prev.a);
				if (predictor != PREDICTOR::HORIZONTAL)
					prev = x;
			}
		} else {
			int		bps		= bitspersample[0];
			int		mask	= 0xff00>>bps;
			int		bits	= 0;
			int		buffer;
			for (auto &x : row) {
				buffer = (buffer << bps) | (x.r & mask);
				bits -= bps;
				if (bits < 0) {
					file.putc(buffer);
					bits	= 8 - bps;
				}
			}
		}
	}
	return true;
}

template<bool be> bool TIFF::WriteIFD(ostream_ref file, const bitmap *bm) {
	write<be>(file, (uint16)42);

	static const int		nde	= 16;
	write<be>(file, uint32(8));	//offset to first directory

	write<be>(file, uint16(nde));

	auto	start_de	= file.tell();
	file.seek(start_de + nde * sizeof(DE));
	write<be>(file, 0);			// next directory

	DE	de[nde], *pde = de;;

	pde++->init<be>(file, TAG::SubfileType,		uint32(0));
	pde++->init<be>(file, TAG::ImageWidth,		width);
	pde++->init<be>(file, TAG::ImageHeight,		height);
	pde++->init<be>(file, TAG::BitsPerSample,	bitspersample);
	pde++->init<be>(file, TAG::Compression,		compression);
	pde++->init<be>(file, TAG::PhotometricInterpretation,		photometric);

	auto	deStripOffsets = pde;
	pde++->init<be>(file, TAG::StripOffsets,	stripoffsets);
	pde++->init<be>(file, TAG::Orientation,		orientation);
	pde++->init<be>(file, TAG::SamplesPerPixel,	samplesperpixel);
	pde++->init<be>(file, TAG::RowsPerStrip,	rowsperstrip);

	auto	deStripByteCounts = pde;
	pde++->init<be>(file, TAG::StripByteCounts,	stripbytecounts);
	pde++->init<be>(file, TAG::XResolution,		xresolution);
	pde++->init<be>(file, TAG::YResolution,		yresolution	);
	pde++->init<be>(file, TAG::PlanarConfiguration,	planarconfig);
	pde++->init<be>(file, TAG::ResolutionUnit,	resolution_unit);
	pde++->init<be>(file, TAG::Predictor,		predictor);

	ISO_ASSERT(pde == end(de));

	auto	end		= file.tell();
	file.seek(start_de);
	for (auto &i : de)
		i.save<be>(file);
	file.seek(end);

	if (auto codec = GetCodec()) {
		auto	writer = codec->get_writer(this);
		for (int i = 0, y = 0; y < height; i++, y += rowsperstrip ) {
			stripoffsets[i] = file.tell32();
			auto	block = bm->Block(0, y, width, rowsperstrip);
			writer->put_strip(this, file, block);
			stripbytecounts[i] = file.tell32() - stripoffsets[i];
		}
		writer->end(this, file);
	}

	end	= file.tell();
	
	file.seek(deStripOffsets->pos);
	write<be>(file, stripoffsets);
	
	file.seek(deStripByteCounts->pos);
	write<be>(file, stripbytecounts);

	file.seek(end);

	return true;
}

//-----------------------------------------------------------------------------
//	TIFFFileHandler
//-----------------------------------------------------------------------------

class TIFFFileHandler : public BitmapFileHandler {
	const char*		GetExt()	override { return "tif"; }
	bool			NeedSeek()	override { return false; }

	int				Check(istream_ref file) override {
		file.seek(0);
		uint16	w = file.get();
		return	w == tiff::header<false> || w == tiff::header<true> ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}
	bool			Write(ISO_ptr<void> p, ostream_ref file) override {
		ISO_ptr<bitmap> bm = ISO_conversion::convert<bitmap>(p);
		if (!bm)
			return false;

		file.write(tiff::header<iso_bigendian>);
		TIFF	t;
		t.SetBitmap(bm);
		return t.WriteIFD<iso_bigendian>(file, bm);

	}
	ISO_ptr<void>	Read(tag id, istream_ref file) override {
//		if (!file.canseek())
//			return Read(id, memory_reader(malloc_block::unterminated(file)).me());

		TIFF	t;
		uint16	w = file.get();
		if (  w == tiff::header<false>	? t.ReadIFD<false>(file)
			: w == tiff::header<true>	? t.ReadIFD<true>(file)
			: false
		) {
			return t.GetBitmap(id, file);
		}
		return ISO_NULL;
	}

} tif_fh;

class TIFFFileHandler2 : TIFFFileHandler {
	const char*		GetExt()				override { return "tiff";	}
	const char*		GetMIME()				override { return "image/tiff"; }
	int				Check(istream_ref file)	override { return CHECK_DEFINITE_NO; }
} tiff_fh;
