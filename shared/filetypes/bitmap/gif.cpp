#include "bitmapfile.h"
#include "codec/lzw.h"
#include "codec/codec_stream.h"
#include "base/bits.h"

#if 0//def ISO_EDITOR
#include "E:\Github\giflib\lib\gif_lib.h"
#pragma comment(lib, "D:\dev\shared\x64\Debug\giflib2.lib")
#endif

//-----------------------------------------------------------------------------
//	CompuServe GIF
//-----------------------------------------------------------------------------

using namespace iso;

//-----------------------------------------------------------------------------
//	GIFblock
//-----------------------------------------------------------------------------

class GIFostream : public writer_mixin<GIFostream> {
	ostream_ref	file;
	int				ptr;
	unsigned char	buffer[256];
	void			flush() {
		file.putc(ptr);
		file.writebuff(buffer, ptr);
		ptr = 0;
	}
public:
	GIFostream(ostream_ref file) : file(file), ptr(0) {}
	int	putc(int c) {
		if (ptr == 255)
			flush();
		return buffer[ptr++] = c;
	}
	~GIFostream()	{ if (ptr) flush(); file.putc(0); }
};

//-----------------------------------------------------------------------------
//	GIFFileHandler
//-----------------------------------------------------------------------------

class GIFFileHandler : BitmapFileHandler {
	enum {
		START_EXT	= 0x21,
		START_IMAGE	= 0x2c,
		END_FILE	= 0x3b,
	};
#pragma pack(1)
	struct GIFHEAD {
		uint8	signature[3];
		uint8	version[3];

		uint16	screenwidth;
		uint16	screenheight;
		uint8	ct_bpp:3, sorted:1, bpp:3, global_ct:1;
		uint8	backgroundcolor;
		uint8	aspectratio;
		
		GIFHEAD()	{}
		GIFHEAD(int width, int height, int bpp, int ct_bpp) : screenwidth(width), screenheight(height), ct_bpp(ct_bpp - 1), sorted(true), bpp(bpp - 1), global_ct(ct_bpp != 0), backgroundcolor(0), aspectratio(0) {
			signature[0]	= 'G';
			signature[1]	= 'I';
			signature[2]	= 'F';

			version[0]		= '8';
			version[1]		= '9';
			version[2]		= 'a';
		}
		constexpr bool valid() const {
			return signature[0] == 'G' && signature[1] == 'I' && signature[2] == 'F';
		}
	};
	struct GIFIMAGE {
		uint16	left;
		uint16	top;
		uint16	width;
		uint16	height;
		uint8	ct_bpp:3, :2, sorted:1, interleaved:1, local_ct:1;
		GIFIMAGE() {}
		GIFIMAGE(uint16 left, uint16 top, uint16 width, uint16 height, int ct_bpp, bool interleaved) :
			left(left), top(top), width(width), height(height), ct_bpp(ct_bpp - 1), sorted(true), interleaved(interleaved), local_ct(ct_bpp != 0) {}
	};
	struct GIFTEXT {
		enum {CODE = 1};
		uint16	left, top, width, height;
		uint8	cell_width, cell_height;
		uint8	foreground, background;
	};
	struct GIFCONTROL {
		enum {CODE = 0xf9};
		enum DISPOSAL {
			NONE,			//0: No disposal specified. The decoder is not required to take any action.
			NO_DISPOSE,		//1: Do not dispose. The graphic is to be left in place.
			RESTORE_BG,		//2: Restore to background color. The area used by the graphic must be restored to the background color.
			RESTORE_PREV,	//3: Restore to previous. The decoder is required to restore the area overwritten by the graphic with what was there prior to rendering the graphic.
			//4–7: To be defined.
		};
		uint8	has_transparent:1, userinput:1, disposal:3, :3;
		uint16	delay;
		uint8	transparent;
		GIFCONTROL() { clear(*this);  }
		GIFCONTROL(float delay) : has_transparent(false), userinput(false), delay(uint16(delay * 100.f)), transparent(0) {}
		GIFCONTROL(float delay, uint8 transparent, DISPOSAL disposal) : has_transparent(true), userinput(false), disposal(disposal), delay(uint16(delay * 100.f)), transparent(transparent) {}
	};
	struct GIFCOMMENT {
		enum {CODE = 0xfe};
	};
	struct GIFAPP {
		enum {CODE = 0xff};
		char	app[8];
		char	authentication[3];
		GIFAPP()	{}
		GIFAPP(const char *_app, const char *_authentication) { memcpy(app, _app, 8); memcpy(authentication, _authentication, 3); }
	};
	struct GIFCOLOR {
		uint8	r, g, b;
		GIFCOLOR()					{}
		GIFCOLOR(const ISO_rgba &c) : r(c.r), g(c.g), b(c.b)	{}
		operator ISO_rgba()			{ return ISO_rgba(r, g, b); }
	};
#pragma pack()

	struct BlockWriter {
		ostream_ref		file;
		template<typename T> auto &Add(const T &t) {
			file.putc(uint8(sizeof(t)));
			file.writebuff(&t, sizeof(t));
			return *this;
		}
		BlockWriter(ostream_ref file)		: file(file) {}
		template<typename T> BlockWriter(ostream_ref file, const T &t)	: file(file) { Add(t); }
//		BlockWriter(BlockWriter &&b)	: file(b.file) { b.file = 0; }
		~BlockWriter()	{ file.putc(0); }
	};
	template<typename T> static auto WriteExtension(ostream_ref file, const T& t) {
		file.putc(START_EXT);
		file.putc(T::CODE);
		return BlockWriter(file, t);
	}

	static malloc_block ReadBlocks(istream_ref file) {
		malloc_block2	raw;
		while (int len = file.getc())
			raw.extend(len).read(file);
		return move(raw);
	}


	static size_t ReadLine(ISO_rgba *dest, memory_block temp, memory_block source, LZW_decoder<false, false, 12> &lzw) {
		size_t		written, read = transcode(lzw, temp, source, &written, TRANSCODE_PARTIAL | TRANSCODE_DST_VOLATILE);
		const uint8	*line = temp;
		for (auto c : make_range<uint8>(temp))
			*dest++ = c;
		return read;
	}

	static void ReadLine(ISO_rgba *dest, istream_ref lzw, int width) {
		for (int x = 0; x < width; x++)
			dest[x] = lzw.getc();
	}


	const char*		GetExt()				override { return "gif";	}
	const char*		GetMIME()				override { return "image/gif"; }
	const char*		GetDescription()		override { return "CompuServe GIF";	}
	int				Check(istream_ref file)	override { file.seek(0); return file.get<GIFHEAD>().valid() ? CHECK_PROBABLE : CHECK_DEFINITE_NO; }
	ISO_ptr<void>	Read(tag id, istream_ref file) override;
	bool			Write(ISO_ptr<void> p, ostream_ref file) override;
#if 0//def ISO_EDITOR
	ISO_ptr64<void>	ReadWithFilename64(tag id, const filename &fn) override {
		int	Error;
		auto gif = DGifOpenFileName(fn, &Error);
		DGifSlurp(gif);

		ISO_ptr<bitmap_anim>	anim(id);
		for (auto &i : make_range_n(gif->SavedImages, gif->ImageCount)) {
			ISO_ptr<bitmap>			bm(none, i.ImageDesc.Width, i.ImageDesc.Height);
			auto	p = gif->SColorMap->Colors;
			for (auto& c : bm->CreateClut(1 << gif->SColorMap->BitsPerPixel)) {
				c.r = p->Red;
				c.g = p->Green;
				c.b = p->Blue;
				c.a = 255;
				++p;
			}
			copy(make_block(i.RasterBits, i.ImageDesc.Width, i.ImageDesc.Height), bm->All());
			anim->Append(make_pair(bm, 1));
		}

		return anim;
	}
#endif

} gif;

ISO_ptr<void> GIFFileHandler::Read(tag id, istream_ref file) {
	GIFHEAD		head	= file.get();
	if (!head.valid())
		return ISO_NULL;

	ISO_rgba	gclut[256];
	if (head.global_ct) {
		for (int i = 0, n = 2 << head.ct_bpp; i < n; i++)
			gclut[i] = file.get<GIFCOLOR>();
	}

	ISO_ptr<bitmap_anim>	anim;
	ISO_ptr<bitmap>			bm;
	ISO_ptr<void>			p;
	float					firstdelay;

	timer	time;

	for (;;) {
		GIFCONTROL	cont;

		uint8		separator;
		while ((separator = file.getc()) == START_EXT) {
			int		functioncode	= file.getc();
			auto	data			= ReadBlocks(file);
			if (functioncode == GIFCONTROL::CODE)
				cont = *data;
		}
		if (separator == END_FILE)
			break;

		if (separator != START_IMAGE)
			return ISO_NULL;

		if (bm) {
			if (!anim) {
				p = anim.Create(id);
				bm.SetID(NULL);
				anim->Append(make_pair(bm, firstdelay));
			}
			bm	= Duplicate(bm);
			anim->Append(make_pair(bm, cont.delay / 100.f));
		} else {
			p			= bm.Create(id);
			firstdelay	= cont.delay / 100.f;
			bm->Create(head.screenwidth, head.screenheight);
		}

		GIFIMAGE	image = file.get();

		if (image.local_ct) {
			int		n		= 2 << image.ct_bpp;
			auto	clut	= bm->CreateClut(n);
			for (int i = 0; i < n; i++)
				clut[i] = file.get<GIFCOLOR>();
		} else {
			int		n		= 2 << head.ct_bpp;
			auto	clut	= bm->CreateClut(n);
			for (int i = 0; i < n; i++)
				clut[i] = gclut[i];
		}

		if (cont.has_transparent)
			bm->ClutBlock()[cont.transparent] = ISO_rgba(0,0,0,0);

		int			nbits = file.getc();

		malloc_block	raw		= ReadBlocks(file);
		LZW_decoder<false, false, 12>	lzw(nbits);
		size_t			src_offset = 0;

		if (image.interleaved) {
			malloc_block	line_block(image.width);
			for (int p = 0; p < 4; p++) {
				for (int d = p ? (16 >> p) : 8, y = (8 >> p) & 7; y < image.height; y += d)
					src_offset += ReadLine(bm->ScanLine(y + image.top) + image.left, line_block, raw + src_offset, lzw);
			}
		} else if (false && image.left == 0 && image.width == head.screenwidth) {
			malloc_block	line_block(image.width * image.height);
			ReadLine(bm->ScanLine(image.top), line_block, raw, lzw);

		} else {
			malloc_block	line_block(image.width);
			for (int y = 0; y < image.height; y++)
				src_offset += ReadLine(bm->ScanLine(y + image.top) + image.left, line_block, raw + src_offset, lzw);
		}
	}

	ISO_OUTPUTF("time=") << (float)time << '\n';

	return p;
}

int Quantise(bitmap &bm, int reqcolors, bool floyd);

bool GIFFileHandler::Write(ISO_ptr<void> p, ostream_ref file) {
	ISO_ptr<bitmap_anim>	anim;
	ISO_ptr<bitmap>			bm;

	if (p.GetType()->SameAs<bitmap_anim>()) {
		anim	= p;
		bm		= (*anim)[0].a;
	} else if (p.GetType()->SameAs<anything>()) {
		anything				&a	= *(anything*)p;
		ISO_ptr<bitmap_anim>	b(NULL);
		for (int i = 0, n = a.Count(); i < n; i++) {
			bm = ISO_conversion::convert<bitmap>(a[i]);
			if (!bm)
				return false;
			b->Append(make_pair(bm, 1.f));
		}
		anim	= b;
	} else {
		bm = ISO_conversion::convert<bitmap>(p);
		if (!bm)
			return false;
	}
	int			bpp			= 8;//BMWF_GETBITS(flags);
	int			width		= bm->Width();
	int			height		= bm->Height();
	bool		interleaved	= false;//!!(flags & BMWF_TWIDDLE);
	bool		global_ct	= true;

	if (!bm->IsPaletted())
		Quantise(*bm, 256, true);

	if (anim) {
		for (int i = 1, n = anim->Count(); i < n; i++) {
			bitmap	*bm2 = (*anim)[i].a;
			if (!bm2->IsPaletted())
				Quantise(*bm2, 256, true);
			if (global_ct) {
				for (int i = 0; i < bm2->ClutSize(); i++) {
					if (i > bm->ClutSize() || bm->Clut(i) != bm2->Clut(i)) {
						global_ct = false;
						break;
					}
				}
			}
		}
	}

	if (bpp == 0)
		bpp = iso::log2(bm->ClutSize());

	file.write(GIFHEAD(width, height, bpp, global_ct ? bpp : 0));

	if (global_ct) {
		for (int i = 0, n = 1 << bpp; i < n; i++)
			file.write(GIFCOLOR(i < bm->ClutSize() ? bm->Clut(i) : ISO_rgba(0,0,0)));
	}

	if (anim) {
		struct NumLoops {
			uint8	unknown;
			uint16	num_loops; //0 => forever
		};
		WriteExtension(file, GIFAPP("NETSCAPE", "2.0")).Add(NumLoops{1,0});
	}

	auto	buffer = make_auto_block<uint8>(width, height);

	for (int i = 0, n = anim ? anim->Count() : 1; i < n; i++) {
		int			miny, maxy, minx, maxx, y;

		if (i > 0)
			bm = (*anim)[i].a;

		WriteExtension(file, GIFCONTROL(anim ? (*anim)[i].b : 0, 255, GIFCONTROL::RESTORE_BG));

		if (i == 0) {
			miny = 0;
			maxy = height;
			minx = 0;
			maxx = width;
			copy(bm->All(), element_cast<Texel<R8>>(buffer));

		} else {
			for (y = height; y--;) {
				uint8		*d = buffer[y].begin();
				ISO_rgba	*s = bm->ScanLine(y);
				int			x;
				for (x = 0; x < width; x++) {
					if (d[x] != s[x].r)
						break;
				}
				if (x != width)
					break;
			}
			miny = 0;
			maxy = y + 1;
			minx = maxy == 0 ? 0 : width;
			maxx = 0;
			for (y = 0; y < maxy; y++) {
				uint8		*d = buffer[y].begin();
				ISO_rgba	*s = bm->ScanLine(y);
				int			x;
				for (x = 0; x < minx; x++) {
					if (d[x] != s[x].r)
						break;
				}
				if (x == width) {
					miny = y + 1;
					continue;
				}
				minx = x;
				for (x = width; x-- > maxx;) {
					if (d[x] != s[x].r)
						break;
				}
				maxx = x + 1;
				for (x = minx; x < maxx; x++)
					d[x] = s[x].r;
			}
		}

		file.putc(START_IMAGE);
		file.write(GIFIMAGE(minx, miny, maxx - minx, maxy - miny, global_ct ? 0 : bpp, interleaved));

		if (!global_ct) {
			for (int i = 0, n = 1 << bpp; i < n; i++)
				file.write(GIFCOLOR(i < bm->ClutSize() ? bm->Clut(i) : ISO_rgba(0,0,0)));
		}

		file.putc(bpp);
		{
			GIFostream	blocks(file);
			auto		lzw = make_codec_writer<0>(LZW_encoder<false, false>(bpp), blocks);
			if (interleaved) {
				for (int p = 0; p < 4; p++) {
					for (int d = p ? (16 >> p) : 8, y = miny + ((8 >> p) & 7); y < maxy; y += d)
						lzw.writebuff(buffer[y].begin() + minx, maxx - minx);
				}
			} else {
				for (int y = miny; y < maxy; y++)
					lzw.writebuff(buffer[y].begin() + minx, maxx - minx);
			}
		}

	}
	file.putc(END_FILE);
	return true;
}

