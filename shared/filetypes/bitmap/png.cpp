#include "bitmapfile.h"
#include "codec/deflate.h"
#include "codec/codec_stream.h"

using namespace iso;
/*

Critical chunks (must appear in this order, except PLTE is optional):
	Name  Multiple  Ordering constraints
	IHDR    No      Must be first
	PLTE    No      Before IDAT
	IDAT    Yes     Multiple IDATs must be consecutive
	IEND    No      Must be last

Ancillary chunks (need not appear in this order):
	Name  Multiple  Ordering constraints
	cHRM    No      Before PLTE and IDAT
	gAMA    No      Before PLTE and IDAT
	iCCP    No      Before PLTE and IDAT
	sBIT    No      Before PLTE and IDAT
	sRGB    No      Before PLTE and IDAT
	bKGD    No      After PLTE; before IDAT
	hIST    No      After PLTE; before IDAT
	tRNS    No      After PLTE; before IDAT
	pHYs    No      Before IDAT
	sPLT    Yes     Before IDAT
	tIME    No      None
	iTXt    Yes     None
	tEXt    Yes     None
	zTXt    Yes     None

Standard keywords for text chunks:
	Title            Short (one line) title or caption for image
	Author           Name of image's creator
	Description      Description of image (possibly long)
	Copyright        Copyright notice
	Creation Time    Time of original image creation
	Software         Software used to create the image
	Disclaimer       Legal disclaimer
	Warning          Warning of nature of content
	Source           Device used to create the image
	Comment          Miscellaneous comment; conversion from GIF comment
*/

uint8	signature[] = {0x89, 'P', 'N', 'G', 13, 10, 26, 10};

struct Chunk {
	uint32			type;
	malloc_block	data;

	uint32	calc_crc() const {
		crc32	c;
		c.write(type);
		c.write(data);
		return c;
	}

	Chunk()	{}
	Chunk(uint32 type, malloc_block&& data) : type(type), data(data) {}

	bool	read(istream_ref file) {
		uint32be	length;
		uint32be	crc;
		return file.read(length, type) && data.read(file, length) && file.read(crc) && calc_crc() == crc;
	}

	bool	write(ostream_ref file) const {
		return file.write(uint32be(data.size32()), type, data, uint32be(calc_crc()));
	}
};

struct ia8 {
	uint8		i, a;
};
struct rgba8 {
	uint8		r, g, b, a;
	operator ISO_rgba() const { return {r,g,b,a}; }
};
struct rgb8 {
	uint8		r, g, b;
	operator rgba8() const { return {r,g,b,0}; }
};
struct rgb16 {
	uint16be	r, g, b;
};
struct chrom {
	uint32be	x, y;
};

union col16 {
	uint16be	Gray;
	rgb16		Col;
	col16()	{}
};

enum Type : uint8 {	//Bit Depths
	gray	= 0,	// 1,2,4,8,16
	rgb		= 2,	// 8,16
	indexed	= 3,	// 1,2,4,8   
	graya	= 4,	// 8,16      
	rgba	= 6,	// 8,16

	noalpha	= 0,
	alpha	= 2,
};
enum Filter : uint8 {
	None	= 0,
	Sub		= 1,
	Up		= 2,
	Average	= 3,
	Paeth	= 4,
};
enum Interlace : uint8 {
	NoInterlace	= 0,
	Adam7		= 1,
};
enum Compression : uint8 {
	zlib	= 0
};

enum Intent : uint8 {
	Perceptual				= 0,
	RelativeColorimetric	= 1,
	Saturation				= 2,
	AbsoluteColorimetric	= 3,
};

uint32	num_components(Type type) {
	static uint8 ncomps[] = {1, 0, 3, 1, 2, 0, 4};
	return ncomps[type];
}
uint32	bytes_per_texel(Type type, uint8 bitdepth) {
	return (bitdepth * num_components(type) + 7) / 8;
}

//Image header 
struct IHDR {
	uint32be	Width;
	uint32be	Height;
	uint8		BitDepth;
	Type		ColorType;
	Compression	CompressionMethod;
	Filter		FilterMethod;
	Interlace	InterlaceMethod;

	IHDR()	{}
	IHDR(uint32 Width, uint32 Height, uint8 BitDepth, Type ColorType, bool interlaced)
		: Width(Width), Height(Height), BitDepth(BitDepth), ColorType(ColorType)
		, CompressionMethod(zlib), FilterMethod(None), InterlaceMethod(interlaced ? Adam7 : NoInterlace) {}
} __attribute__((packed));

//Transparency
//If present specifies that the image uses simple transparency: either alpha values associated with palette entries (for indexed-color images) or a single transparent color (for grayscale and truecolor images)
union tRNS {
	uint8	alpha[];//palette entry alphas
	col16	trans;	//Pixels of this colour are to be treated as transparent (equivalent to alpha value 0)
};

//Image gamma
struct gAMA {
	uint32be	Gamma;
};

//Primary chromaticities
struct  cHRM {
	chrom		white, red, green, blue;
};

//Standard RGB color space
struct sRGB {
	Intent	RenderingIntent;
};

//Embedded ICC profile
struct iCCP {
	embedded_string	ProfileName;
	//Compression	CompressionMethod;
	//CompressedProfile: n bytes
};

struct tEXt {
	embedded_string	Keyword;
	//embedded_string	Text;
};
struct zTXt {
	embedded_string	Keyword;
	//Compression		CompressionMethod;
	//embedded_string	Text;
};
struct iTXt {
	embedded_string	Keyword;
	//uint8				CompressionFlag;
	//Compression		CompressionMethod;
	//embedded_string	LanguageTag;
	//embedded_string	TranslatedKeyword;
	//embedded_string	Text;
};

//Background color
union bKGD {
	uint8		index;	//3
	uint16be	Gray;	//0, 4
	rgb16		col;	//2, 6
	bKGD() {}
};

//Physical pixel dimensions
// When the unit specifier is 0, the pHYs chunk defines pixel aspect ratio only; the actual size of the pixels remains unspecified.
struct pHYs {
	enum {
		unknown	= 0,
		meters	= 1,
	};
	uint32be	PixelsPerUnitX;
	uint32be	PixelsPerUnitY;
	uint8		UnitSpecifier;
};

//Significant bits
// Each depth specified in sBIT must be greater than zero and less than or equal to the sample depth (which is 8 for indexed-color images, and the bit depth given in IHDR for other color types).
union sBIT {
	uint8	gray_bits;		//0
	rgb8	col_bits;		//2,3
	ia8		gray_alpha_bits;//4
	rgba8	col_alpha_bits;	//6
};

//Suggested palette
// Used to suggest a reduced palette to be used when the display device is not capable of displaying the full range of colors present in the image
// If present, it provides a recommended set of colors, with alpha and frequency information, that can be used to construct a reduced palette to which the PNG image can be quantized.
struct sPLT {
	embedded_string	PaletteName;
	uint8			SampleDepth;
	struct Entry8 { rgb8	col8; uint16 freq; };
	struct Entry16 { rgb16	col16; uint16 freq; };
	union {
		Entry8	entries8[];
		Entry16	entries16[];
	};
};

//Palette histogram
struct hIST {
};

//Image last-modification time
struct  tIME {
	uint16be	Year;	// (complete; for example, 1995, not 95)
	uint8		Month;	// (1-12)
	uint8		Day;	// (1-31)
	uint8		Hour;	// (0-23)
	uint8		Minute;	// (0-59)
	uint8		Second;	// (0-60) 60 for leap seconds
};

//CGBI - apple extension
// if present causes:
//	byteswapped (RGBA -> BGRA) pixel data
//	zlib header, footer, and CRC removed from the IDAT chunk
//	premultiplied alpha (color' = color * alpha / 255)
struct  CgBI {
	//CGBitmapInfo bitmask.
	uint32be	cgbi;	// 0x50 0x00 0x20 0x06 or 0x50 0x00 0x20 0x02 
};


// a = left, b = above, c = upper left
inline int PaethPredictor(int a, int b, int c) {
	int		p	= a + b - c;	// initial estimate
	int		pa	= abs(p - a);	// distances to a, b, c
	int		pb	= abs(p - b);
	int		pc	= abs(p - c);
	// return nearest of a,b,c, breaking ties in order a,b,c.
	return	pa <= pb && pa <= pc ? a
		:	pb <= pc ? b
		:	c;
}

bool readline(istream_ref file, uint8 *prev, uint8 *next, uint32 len, uint32 bpp) {
	auto	filter = (Filter)file.getc();

	if (file.readbuff(next, len) != len)
		return false;

	switch (filter) {
		case None:
			break;

		case Sub:
			for (int i = bpp; i < len; i++)
				next[i] += next[i - bpp];
			break;

		case Up:
			for (int i = 0; i < len; i++)
				next[i] += prev[i];
			break;

		case Average:
			for (int i = 0; i < bpp; i++)
				next[i] += prev[i] / 2;
			for (int i = bpp; i < len; i++)
				next[i] += (next[i - bpp] + prev[i]) / 2;
			break;

		case Paeth:
			for (int i = 0; i < bpp; i++)
				next[i] += PaethPredictor(0, prev[i], 0);
			for (int i = bpp; i < len; i++)
				next[i] += PaethPredictor(next[i - bpp], prev[i], prev[i - bpp]);
			break;

		default:
			return false;
	}
	return true;
}

template<int B, typename T> const uint8 *readbits(const block<T, 1> &blk, const uint8 *src) {
	uint32	b = 1 << 16;
	for (auto& x : blk) {
		if (b >= (1 << 16))
			b = *src++ | 0x100;
		x = (b >> (8 - B)) & bits(B);
		b <<= B;
	}
	return src;
}

template<typename T> bool readblock(const block<T, 2> &blk, istream_ref file, Type type, uint8 bitdepth) {
	uint32	bpp	= bytes_per_texel(type, bitdepth);
	uint32	bpl	= bpp * blk.template size<1>();

	temp_array<uint8>	data(bpl * 2);
	uint8	*prev	= data, *next = prev + bpl;
	memset(prev, 0, bpl);

	for (auto y : blk) {
		readline(file, prev, next, bpl, bpp);
		switch (type) {
			case gray:
				switch (bitdepth) {
					case 1:
						readbits<1>(y, next);
						break;
					case 2:
						readbits<2>(y, next);
						break;
					case 4:
						readbits<4>(y, next);
						break;
					case 8: {
						auto	src	= next;
						for (auto& x : y)
							x = *src++;
						break;
					}
					case 16: {
						auto	src = (const uint16be*)next;
						for (auto& x : y)
							x = (int)*src++;
						break;
					}
				}
				break;

			case rgb:
				switch (bitdepth) {
					case 8: {
						auto	src	= next;
						for (auto& x : y) {
							uint8	r = *src++, g = *src++, b = *src++;
							x = T(r, g, b);
						}
						break;
					}
					case 16: {
						auto	src = (const uint16be*)next;
						for (auto& x : y) {
							uint16	r = *src++, g = *src++, b = *src++;
							x = T(r, g, b);
						}
						break;
					}
				}
				break;

			case indexed:
				switch (bitdepth) {
					case 1:
						readbits<1>(y, next);
						break;
					case 2:
						readbits<2>(y, next);
						break;
					case 4:
						readbits<4>(y, next);
						break;
					case 8: {
						auto	src = next;
						for (auto& x : y)
							x = *src++;
						break;
					}
				}
				break;

			case graya:
				switch (bitdepth) {
					case 8: {
						auto	src	= next;
						for (auto& x : y) {
							uint8	i = *src++, a = *src++;
							x = T(i, a);
						}
						break;
					}
					case 16: {
						auto	src = (const uint16be*)next;
						for (auto& x : y) {
							uint16	i = *src++, a = *src++;
							x = T(i, a);
						}
						break;
					}
				}
				break;

			case rgba:
				switch (bitdepth) {
					case 8: {
						auto	src	= next;
						for (auto& x : y) {
							uint8	r = *src++, g = *src++, b = *src++, a = *src++;
							x = T(r, g, b, a);
						}
						break;
					}
					case 16: {
						auto	src = (const uint16be*)next;
						for (auto& x : y) {
							uint16	r = *src++, g = *src++, b = *src++, a = *src++;
							x = T(r, g, b, a);
						}
						break;
					}
				}
				break;
		}

		swap(prev, next);
	}
	return true;
}

static Filter get_filter(uint8* prev, uint8* next, uint32 len, uint32 bpp) {
	uint32	mins	= maximum;
	Filter	filter	= None;

	//None
	uint32 sum = 0;
	for (int i = 0; i < len; i++) {
		int	v = next[i];
		sum += (v < 128) ? v : 256 - v;
	}
	mins = sum;

	// Sub filter
	sum = 0;
	for (int i = 0; i < bpp; i++) {
		int	v = next[i];
		sum += (v < 128) ? v : 256 - v;
	}
	for (int i = bpp; sum < mins && i < len; i++) {
		int	v = (next[i] - next[i - bpp]) & 0xff;
		sum += (v < 128) ? v : 256 - v;
	}

	if (sum < mins) {
		mins	= sum;
		filter	= Sub;
	}

	// Up filter
	sum	= 0;
	for (int i = 0; sum < mins && i < len; i++) {
		int	v = (next[i] - prev[i]) & 0xff;
		sum += (v < 128) ? v : 256 - v;
	}

	if (sum < mins) {
		mins	= sum;
		filter	= Up;
	}

	// Avg filter
	sum	= 0;
	for (int i = 0; i < bpp; i++) {
		int	v = (next[i] - prev[i] / 2) & 0xff;
		sum += (v < 128) ? v : 256 - v;
	}
	for (int i = bpp; sum < mins && i < len; i++) {
		int	v = (next[i] - (prev[i] + next[i - bpp]) / 2) & 0xff;
		sum += (v < 128) ? v : 256 - v;
	}

	if (sum < mins) {
		mins	= sum;
		filter	= Average;
	}

	// Paeth filter
	sum	= 0;
	for (int i = 0; i < bpp; i++) {
		int	v = (next[i] - prev[i]) & 0xff;
		sum += (v < 128) ? v : 256 - v;
	}
	for (int i = bpp; sum < mins && i < len; i++) {
		int	p = PaethPredictor(next[i - bpp], prev[i], prev[i - bpp]);
		int	v = (next[i] - p) & 0xff;
		sum += (v < 128) ? v : 256 - v;
	}

	if (sum < mins)
		filter = Paeth;


	return filter;
}

static bool writeline(ostream_ref file, uint8 *prev, uint8 *next, uint8 *dest, uint32 len, uint32 bpp, Filter filter) {
	file.putc(filter);

	switch (filter) {
		case None:
			break;

		case Sub:
			for (int i = 0; i < bpp; i++)
				dest[i] = next[i];
			for (int i = bpp; i < len; i++)
				dest[i] = next[i] - next[i - bpp];
			break;

		case Up:
			for (int i = 0; i < len; i++)
				dest[i] = next[i] - prev[i];
			break;

		case Average:
			for (int i = 0; i < bpp; i++)
				dest[i] = next[i] - prev[i] / 2;
			for (int i = bpp; i < len; i++)
				dest[i] = next[i] - (prev[i] + next[i - bpp]) / 2;
			break;

		case Paeth:
			for (int i = 0; i < bpp; i++)
				dest[i] = next[i] - prev[i];
			for (int i = bpp; i < len; i++)
				dest[i] = next[i] - PaethPredictor(next[i - bpp], prev[i], prev[i - bpp]);
			break;
	}

	return file.writebuff(dest, len) == len;
}

template<int B, typename T> uint8 *writebits(const block<T, 1> &blk, uint8 *dst) {
	uint32	b = 1;
	for (auto& x : blk) {
		b = (b << B) | (x.r & bits(B));
		if (b >= (1 << 8)) {
			*dst++ = b;
			b = 1;
		}
	}
	return dst;
}

template<typename T> bool writeblock(const block<T, 2> &blk, ostream_ref file, Type type, uint8 bitdepth) {
	uint32	bpp	= bytes_per_texel(type, bitdepth);
	uint32	bpl	= bpp * blk.template size<1>();

	temp_array<uint8>	data(bpl * 3);
	uint8	*prev	= data, *next = prev + bpl, *scratch = next + bpl;
	memset(prev, 0, bpl);

	for (auto y : blk) {
		switch (type) {
			case gray:
				switch (bitdepth) {
					case 1:
						writebits<1>(y, next);
						break;
					case 2:
						writebits<2>(y, next);
						break;
					case 4:
						writebits<4>(y, next);
						break;
					case 8: {
						auto	dst	= next;
						for (auto& x : y)
							*dst++ = x.r;
						break;
					}
					case 16: {
						auto	dst = (uint16be*)next;
						for (auto& x : y)
							*dst++ = x.r;
						break;
					}
				}
				break;

			case rgb:
				switch (bitdepth) {
					case 8: {
						auto	dst	= next;
						for (auto& x : y) {
							*dst++ = x.r;
							*dst++ = x.g;
							*dst++ = x.b;
						}
						break;
					}
					case 16: {
						auto	dst = (uint16be*)next;
						for (auto& x : y) {
							*dst++ = x.r;
							*dst++ = x.g;
							*dst++ = x.b;
						}
						break;
					}
				}
				break;

			case indexed:
				switch (bitdepth) {
					case 1:
						writebits<1>(y, next);
						break;
					case 2:
						writebits<2>(y, next);
						break;
					case 4:
						writebits<4>(y, next);
						break;
					case 8: {
						auto	dst = next;
						for (auto& x : y)
							*dst++ = x.r;
						break;
					}
				}
				break;

			case graya:
				switch (bitdepth) {
					case 8: {
						auto	dst	= next;
						for (auto& x : y) {
							*dst++ = x.r;
							*dst++ = x.a;
						}
						break;
					}
					case 16: {
						auto	dst = (uint16be*)next;
						for (auto& x : y) {
							*dst++ = x.r;
							*dst++ = x.a;
						}
						break;
					}
				}
				break;

			case rgba:
				switch (bitdepth) {
					case 8: {
						auto	dst	= next;
						for (auto& x : y) {
							*dst++ = x.r;
							*dst++ = x.g;
							*dst++ = x.b;
							*dst++ = x.a;
						}
						break;
					}
					case 16: {
						auto	dst = (uint16be*)next;
						for (auto& x : y) {
							*dst++ = x.r;
							*dst++ = x.g;
							*dst++ = x.b;
							*dst++ = x.a;
						}
						break;
					}
				}
				break;
		}

		Filter filter = get_filter(prev, next, bpl, bpp);
		writeline(file, prev, next, scratch, bpl, bpp, filter);
		swap(prev, next);
	}
	return true;
}

struct PNG {
	IHDR	ihdr;
	sBIT	sbit;
	bKGD	bkgd;
	pHYs	phys;
	cHRM	chrm;
	tIME	time;

	col16	trans;
	uint32	gamma	= 0;
	Intent	intent	= Perceptual;
	bool	end		= false;
	uint32	cgbi	= 0;	//apple extension

	map<string, malloc_block>	texts;
	dynamic_array<rgba8>		palette;

	string			icc_profile_name;
	malloc_block	icc_profile;
	malloc_block	image_data;

	PNG()	{}

	bool process(const Chunk &chunk);

	template<typename T> bool read(const block<T, 2> &blk, istream_ref file);
	template<typename T> bool write(const block<T, 2> &blk, ostream_ref file);
};

bool PNG::process(const Chunk &chunk) {
	switch (chunk.type) {
		case "IHDR"_u32: ihdr = *chunk.data; break;
		case "PLTE"_u32:
			palette	= make_range<rgb8>(chunk.data);
			break;
		case "IDAT"_u32:
			image_data	+= chunk.data;
			break;
		case "IEND"_u32:
			end = true;
			break;

		case "cHRM"_u32: chrm = *chunk.data; break;
		case "gAMA"_u32: gamma = ((gAMA*)chunk.data)->Gamma; break;
		case "iCCP"_u32: {
			const char	*p = chunk.data;
			icc_profile_name	= p;
			icc_profile = transcode(zlib_decoder(), chunk.data.slice(string_end(p) + 1));
			break;
		}
		case "sBIT"_u32: sbit = *chunk.data; break;
		case "sRGB"_u32: intent = ((sRGB*)chunk.data)->RenderingIntent; break;
		case "bKGD"_u32: bkgd = *chunk.data; break;
//		case "hIST"_u32: set((hIST*)chunk.data); break;
		case "tRNS"_u32: {
			if (ihdr.ColorType == indexed) {
				auto	p = palette.begin();
				for (auto i : make_range<uint8>(chunk.data))
					(p++)->a = i;
			} else {
				trans = ((tRNS*)chunk.data)->trans;
			}
		}
		case "pHYs"_u32: phys = *chunk.data; break;
//		case "sPLT"_u32: set((sPLT*)chunk.data); break;
		case "tIME"_u32: time = *chunk.data; break;
		case "tEXt"_u32: {
			const char *p = chunk.data;
			texts.put(p, chunk.data.slice(string_end(p) + 1));
			break;
		}
		case "zTXt"_u32: {
			const char *p = chunk.data;
			texts.put(p, transcode(zlib_decoder(), chunk.data.slice(string_end(p) + 1)));
			break;
		}
		case "iTXt"_u32: {
			const char *p			= chunk.data;
			auto		p2			= string_end(p);
			uint8		flag		= p2[1];
			uint8		comp		= p2[2];
			auto		language	= p2 + 3;
			auto		keyword		= string_end(language) + 1;
			auto		text		= string_end(keyword) + 1;
			if (flag)
				texts.put(p, transcode(zlib_decoder(), chunk.data.slice(text + 1)));
			else
				texts.put(p, chunk.data.slice(text + 1));
			break;
		}

		case "CgBI"_u32:	//apple extension
			cgbi	= ((CgBI*)chunk.data)->cgbi;
			break;

	}
	return true;
}

template<typename T> bool PNG::read(const block<T, 2> &blk, istream_ref file) {
	if (!ihdr.InterlaceMethod)
		return readblock(blk, file, ihdr.ColorType, ihdr.BitDepth);

	bool ret = readblock(skip<2>(skip<1>(blk, 8), 8), file, ihdr.ColorType, ihdr.BitDepth);							//1
	ret		&= readblock(skip<2>(skip<1>(blk.template slice<1>(4), 8), 8), file, ihdr.ColorType, ihdr.BitDepth);	//2
	ret		&= readblock(skip<2>(skip<1>(blk.template slice<2>(4), 4), 8), file, ihdr.ColorType, ihdr.BitDepth);	//3
	ret		&= readblock(skip<2>(skip<1>(blk.template slice<1>(2), 4), 4), file, ihdr.ColorType, ihdr.BitDepth);	//4
	ret		&= readblock(skip<2>(skip<1>(blk.template slice<2>(2), 2), 4), file, ihdr.ColorType, ihdr.BitDepth);	//5
	ret		&= readblock(skip<2>(skip<1>(blk.template slice<1>(1), 2), 2), file, ihdr.ColorType, ihdr.BitDepth);	//6
	ret		&= readblock(skip<2>(blk.template slice<2>(1), 2), file, ihdr.ColorType, ihdr.BitDepth);				//7
	return ret;
}

template<typename T> bool PNG::write(const block<T, 2> &blk, ostream_ref file) {
	if (!ihdr.InterlaceMethod)
		return writeblock(blk, file, ihdr.ColorType, ihdr.BitDepth);

	bool ret = writeblock(skip<2>(skip<1>(blk, 8), 8), file, ihdr.ColorType, ihdr.BitDepth);						//1
	ret		&= writeblock(skip<2>(skip<1>(blk.template slice<1>(4), 8), 8), file, ihdr.ColorType, ihdr.BitDepth);	//2
	ret		&= writeblock(skip<2>(skip<1>(blk.template slice<2>(4), 4), 8), file, ihdr.ColorType, ihdr.BitDepth);	//3
	ret		&= writeblock(skip<2>(skip<1>(blk.template slice<1>(2), 4), 4), file, ihdr.ColorType, ihdr.BitDepth);	//4
	ret		&= writeblock(skip<2>(skip<1>(blk.template slice<2>(2), 2), 4), file, ihdr.ColorType, ihdr.BitDepth);	//5
	ret		&= writeblock(skip<2>(skip<1>(blk.template slice<1>(1), 2), 2), file, ihdr.ColorType, ihdr.BitDepth);	//6
	ret		&= writeblock(skip<2>(blk.template slice<2>(1), 2), file, ihdr.ColorType, ihdr.BitDepth);				//7
	return ret;
}

class PNGFileHandler : public BitmapFileHandler {
	const char*		GetExt()	override { return "png";	}
	const char*		GetMIME()	override { return "image/png"; }
	int				Check(istream_ref file) override {
		file.seek(0);
		return file.get<array<uint8, 8>>() == signature ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}

	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		if (compare_array(file.get<array<uint8, 8>>(), signature))
			return ISO_NULL;

		PNG		png;
		Chunk	ch;
		while (!png.end && file.read(ch))
			png.process(ch);

		if (png.end) {
			auto	all		= transcode(zlib_decoder(), png.image_data);
			auto	file2	= memory_reader(all);
			//auto	file2 = make_codec_reader<0>(zlib_decoder(), memory_reader(image_data));

			if (png.ihdr.BitDepth > 8) {
				ISO_ptr<HDRbitmap>	bm(id, png.ihdr.Width, png.ihdr.Height);

				png.read(bm->All(), file2);
				return bm;

			} else {
				 ISO_ptr<bitmap>	bm(id, png.ihdr.Width, png.ihdr.Height);

				 if (png.ihdr.ColorType == indexed)
					 ((bitmap*)bm)->CreateClut(png.palette.size()) = png.palette;
			
				 png.read(bm->All(), file2);
				 return bm;
			}
		}

		return ISO_NULL;
	}

	bool			Write(ISO_ptr<void> p, ostream_ref file) override {
		ISO_ptr<bitmap> bm = ISO_conversion::convert<bitmap>(p);
		if (!bm)
			return false;

		PNG		png;

		auto	flags	= bm->Scan();
		Type	type	= bm->IsPaletted() ? indexed : Type((flags & BMF_GREY ? gray : rgb) | (flags & BMF_ALPHA ? alpha : noalpha));
		png.ihdr = IHDR(bm->Width(), bm->Height(), 8, type, false);

		file.write(signature);
		file.write(Chunk("IHDR"_u32, memory_block(&png.ihdr)));

		dynamic_memory_writer	mem;
		{
			//auto	file2 = make_codec_writer<0>(zlib_encoder(), mem);
			png.write(bm->All(), mem);
		}
		file.write(Chunk("IDAT"_u32, transcode(zlib_encoder(), mem.data())));
		file.write(Chunk("IEND"_u32, none));

		return true;
	}
} png;


