#include "iso/iso_files.h"
#include "iso/iso_convert.h"
#include "extra/json.h"
#include "bitmap.h"

using namespace iso;

//-----------------------------------------------------------------------------
//	ICNS
//-----------------------------------------------------------------------------

void RLE(ostream_ref file, const_memory_block buffer) {
	const uint8 *srce = buffer;
	int 	length = buffer.size32();
	uint8	c = srce[0];
	for (int i = 0; i < length;) {
		int		s = i, run;
		uint8	p;
		do {
			p	= c;
			run	= 1;
			while (++i < length && (c = srce[i]) == p && run < 0x82)
				run++;
		} while (i < length && run <= 2);

		if (run < 3)
			run = 0;

		while (i - run > s) {
			int	t = min(i - run - s, 0x80);
			file.putc(t - 1);
			file.writebuff(srce + s, t);
			s = s + t;
		}

		if (run) {
			file.putc(run + 0x80 - 3);
			file.putc(p);
		}
	}
}

malloc_block RLE(istream_ref file, size_t length) {
	malloc_block	buffer(length);
	for (uint8 *dest = buffer, *end = dest + length; dest < end;) {
		uint8	c = file.getc();
		if (c >= 128) {
			c -= 125;
			memset(dest, file.getc(), c);
		} else {
			c++;
			file.readbuff(dest, c);
		}
		dest += c;
	}
	return buffer;
}

tag make_id(uint32be b) {
	char	id[5];
	memcpy(id, &b, 4);
	id[4] = 0;
	return id;
}

enum ICONmode {
	ICON_RAW1	= 1,
	ICON_RAW4	= 2,
	ICON_RAW8	= 3,
	ICON_RLE24	= 4,
	ICON_RLE32	= 5,
	ICON_PNG	= 6,
	
	ICON_MASK1	= 16,
	ICON_MASK8	= 32,
	
	ICON_RETINA	= 128,
};
struct ICONInfo {
	uint32	type;
	uint32	width, height;
	uint32	mode;
};

static ICONInfo	infos[] = {								//Length	Size	OS Ver	Description
	{'ICON',	32,		32,		ICON_RAW1},				//128		32		1.0		32×32 1-bit mono icon
	{'ICN#',	32,		32,		ICON_RAW1|ICON_MASK1},	//256		32		6.0		32×32 1-bit mono icon with 1-bit mask
	{'icm#',	16,		12,		ICON_RAW1|ICON_MASK1},	//48		16		6.0		16×12 1 bit mono icon with 1-bit mask
	{'icm4',	16,		12,		ICON_RAW4},				//96		16		7.0		16×12 4 bit icon
	{'icm8',	16,		12,		ICON_RAW8},				//192		16		7.0		16×12 8 bit icon
	{'ics#',	16,		16,		ICON_RAW1|ICON_MASK1},	//64 		16		6.0		16×16 1-bit mask (32 img + 32 mask)
	{'ics4',	16,		16,		ICON_RAW4},				//128		16		7.0		16×16 4-bit icon
	{'ics8',	16,		16,		ICON_RAW8},				//256		16		7.0		16x16 8 bit icon
	{'is32',	16,		16,		ICON_RLE24},			//<=768		16		8.5		16×16 24-bit icon
	{'s8mk',	16,		16,		ICON_MASK8},			//256		16		8.5		16x16 8-bit mask
	{'icl4',	32,		32,		ICON_RAW4},				//512		32		7.0		32×32 4-bit icon
	{'icl8',	32,		32,		ICON_RAW8},				//1,024		32		7.0		32×32 8-bit icon
	{'il32',	32,		32,		ICON_RLE24},			//<=3,072	32		8.5		32x32 24-bit icon
	{'l8mk',	32,		32,		ICON_MASK8},			//1,024		32		8.5		32×32 8-bit mask
	{'ich#',	48,		48,		ICON_MASK1},			//288		48		8.5		48×48 1-bit mask
	{'ich4',	48,		48,		ICON_RAW4},				//1,152		48		8.5		48×48 4-bit icon
	{'ich8',	48,		48,		ICON_RAW8},				//2,304		48		8.5		48×48 8-bit icon
	{'ih32',	48,		48,		ICON_RLE24},			//<=6,912	48		8.5		48×48 24-bit icon
	{'h8mk',	48,		48,		ICON_MASK8},			//2,304		48		8.5		48×48 8-bit mask
	{'it32',	128,	128, 	ICON_RLE24},			//<=49152	128		10.0	128×128 24-bit icon
	{'t8mk',	128,	128, 	ICON_MASK8},			//16,384	128		10.0	128×128 8-bit mask
	{'icp4',	16,		16,		ICON_PNG},				//varies	16		10.7	16x16 icon in JPEG 2000 or PNG format
	{'icp5',	32,		32,		ICON_PNG},				//varies	32		10.7	32x32 icon in JPEG 2000 or PNG format
	{'icp6',	64,		64,		ICON_PNG},				//varies	64		10.7	64x64 icon in JPEG 2000 or PNG format
	{'ic07',	128,	128,	ICON_PNG},				//varies	128		10.7	128x128 icon in JPEG 2000 or PNG format
	{'ic08',	256,	256,	ICON_PNG},				//varies	256		10.5	256×256 icon in JPEG 2000 or PNG format
	{'ic09',	512,	512,	ICON_PNG},				//varies	512		10.5	512×512 icon in JPEG 2000 or PNG format
	{'ic10',	1024,	1024,	ICON_PNG|ICON_RETINA},	//varies	1024	10.7	1024×1024 in 10.7 (or 512x512@2x "retina" in 10.8) icon in JPEG 2000 or PNG format
	{'ic11',	32,		32,		ICON_PNG|ICON_RETINA},	//varies	32		10.8	16x16@2x "retina" icon in JPEG 2000 or PNG format
	{'ic12',	64,		64,		ICON_PNG|ICON_RETINA},	//varies	64		10.8	32x32@2x "retina" icon in JPEG 2000 or PNG format
	{'ic13',	256,	256,	ICON_PNG|ICON_RETINA},	//varies	256		10.8	128x128@2x "retina" icon in JPEG 2000 or PNG format
	{'ic14',	512,	512,	ICON_PNG|ICON_RETINA},	//varies	512		10.8	256x256@2x "retina" icon in JPEG 2000 or PNG format
	{'ic04',	16,		16,		ICON_RLE32},			//varies	16				16x16 ARGB
	{'ic05',	32,		32,		ICON_RLE32},			//varies	32				32x32 ARGB
	{'icsB',	36,		36},							//varies	36				36x36
	{'icsb',	18,		18},							//varies	18				18x18
};

const ICONInfo *GetICONInfo(uint32 type) {
	return find_if_check(infos, [type](const ICONInfo &i){ return i.type == type;});
}

class ICNSFileHandler : public FileHandler {
	struct entry : bigendian_types {
		uint32	type;
		uint32	length;
	};

	ISO_rgba	clut[256];

	static bool WriteIcon(ISO_ptr<bitmap> bm, ostream_ref file);

	const char*		GetExt() override { return "icns";	}
	const char*		GetDescription() override { return "Mac Icons"; }
	int				Check(istream_ref file) override { file.seek(0); return file.get<uint32be>() == 'icns' ? CHECK_PROBABLE : CHECK_DEFINITE_NO; }
	ISO_ptr<void>	Read(tag id, istream_ref file) override;
	bool			Write(ISO_ptr<void> p, ostream_ref file) override;
public:
	ICNSFileHandler() {
		ISO_rgba	*p = clut;
		for (int r = 0; r < 6; r++) {
			int	r1 = (6 - r) * 255 / 6;
			for (int g = 0; g < 6; g++) {
				int	g1 = (6 - g) * 255 / 6;
				for (int b = 0; b < 6; b++) {
					int	b1 = (6 - b) * 255 / 6;
					*p++ = ISO_rgba(r1, g1, b1);
				}
			}
		}
		--p;
		for (int i = 0; i < 10; i++)
			*p++ = ISO_rgba((11 - i) * 255 / 12, 0, 0);
		for (int i = 0; i < 10; i++)
			*p++ = ISO_rgba(0, (11 - i) * 255 / 12, 0);
		for (int i = 0; i < 10; i++)
			*p++ = ISO_rgba(0, 0, (11 - i) * 255 / 12);
		for (int i = 0; i < 10; i++)
			*p++ = ISO_rgba((11 - i) * 255 / 12);
		*p++ = ISO_rgba(0,0,0,0);
		ISO_ASSERT(p == clut + 256);
	}
} icns;

ISO_ptr<void> ICNSFileHandler::Read(tag id, istream_ref file) {
	entry	h = file.get();
	if (h.type != 'icns')
		return ISO_NULL;

	ISO_ptr<anything>	p(id);
	uint32	length = h.length;
	uint32	offset = 8;

	while (offset < length) {
		file.seek(offset);
		entry	e	= file.get();
		tag		id	= make_id(e.type);
		
		if (auto info = GetICONInfo(e.type)) {
			switch (info->mode & 127) {
				case ICON_MASK8: {
					ISO_ptr<bitmap> bm = p->back();
					if (bm->Width() == info->width && bm->Height() == info->height) {
						uint32	size	= info->width * info->height;
						malloc_block	buffer(file, size);
						ISO_rgba	*i	= bm->ScanLine(0);
						uint8		*s	= (uint8*)buffer;
						while (size--)
							i++->a = *s++;
					}
					break;
				}
			
				case ICON_RAW1:		break;
				case ICON_RAW4:		break;
				case ICON_RAW8: {
					ISO_ptr<bitmap>	bm(id);
					bm->Create(info->width, info->height);
					copy(clut, bm->CreateClut(256));

					uint32	num		= info->width * info->height;
					malloc_block	buffer(file, num);

					ISO_rgba	*i	= bm->ScanLine(0);
					uint8		*r	= (uint8*)buffer;
					while (num--) {
						if (*r == 255)
							*i = ISO_rgba(0,0,0,0);
						else
							*i = *r;
						r++;
						i++;
					}
					p->Append(bm);
					break;
				}
				case ICON_RLE24: {
					ISO_ptr<bitmap>	bm(id);
					bm->Create(info->width, info->height);

					uint32	num		= info->width * info->height;
					auto	buffer	= RLE(file, num * 3);

					ISO_rgba	*i	= bm->ScanLine(0);
					uint8		*r	= buffer, *g = r + num, *b = g + num;
					while (num--) {
						i->r = *r++;
						i->g = *g++;
						i->b = *b++;
						i++;
					}
					p->Append(bm);
					break;
				}
				case ICON_RLE32: {
					ISO_ptr<bitmap>	bm(id);
					bm->Create(info->width, info->height);

					uint32	num		= info->width * info->height;
					auto	buffer	= RLE(file, num * 4);

					ISO_rgba	*i	= bm->ScanLine(0);
					uint8		*a	= buffer, *r	= a + num, *g = r + num, *b = g + num;
					while (num--) {
						i->r = *r++;
						i->g = *g++;
						i->b = *b++;
						i->a = *a++;
						i++;
					}
					p->Append(bm);
					break;
				}
				case ICON_PNG: {
					if (FileHandler *png = FileHandler::Get("png")) {
						if (ISO_ptr<void> t = png->Read(id, file)) {
							p->Append(t);
							break;
						}
					}
					if (FileHandler *jpg = FileHandler::Get("jp2")) {
						file.seek(offset + sizeof(entry));
						if (ISO_ptr<void> t = jpg->Read(id, file)) {
							p->Append(t);
							break;
						}
					}
					break;
				}
			}
		} else switch (e.type) {
			case 'clut':
			case 'TOC ':	// list of all image types in the file, and their sizes (added in Mac OS X 10.7)
			case 'icnV':	// bundle version number
			case 'name':	// unknown
			case 'info':	// Info binary plist. Usage unknown
				break;
		}
		offset	+= e.length;
	}
	return p;
}

bool ICNSFileHandler::Write(ISO_ptr<void> p, ostream_ref file) {
	p = FileHandler::ExpandExternals(p);

	file.seek(sizeof(entry));

	if (p.IsType<ISO_openarray<ISO_ptr<bitmap> > >()) {
		ISO_ptr<ISO_openarray<ISO_ptr<bitmap> > >	bms(p);
		int	num = bms->Count();
		for (int i = 0; i < num; i++) {
			if (!WriteIcon((*bms)[i], file)) {
				throw_accum("Unrecognised size " << (*bms)[i]->Width());
				return false;
			}
		}

	} else {
		ISO_ptr<bitmap>	bm = ISO_conversion::convert<bitmap>(p, ISO_conversion::RECURSE | ISO_conversion::CHECK_INSIDE);
		if (!bm)
			return false;

		if (ISO::Browser b = ISO::root("variables")["sizes"]) {
			int	num	= b.Count();
			for (int i = 0; i < num; i++) {
				int	size	= b[i].GetInt();
				ISO_ptr<bitmap>	bm2(0);
				bm2->Create(size, size);
				resample_via<HDRpixel>(bm2->All(), bm->All());
				if (!WriteIcon(bm2, file)) {
					throw_accum("Unrecognised size " << size);
					return false;
				}
			}
		} else {
			if (!WriteIcon(bm, file))
				return false;
		}
	}

	entry	head;
	head.type	= 'icns';
	head.length	= uint32(file.tell());
	file.seek(0);
	file.write(head);
	return true;
}

bool ICNSFileHandler::WriteIcon(ISO_ptr<bitmap> bm, ostream_ref file) {
	streamptr	start	= file.tell();
	file.seek_cur(sizeof(entry));

	uint32	length = 0;
	tag		bmid	= bm.ID();

	const ICONInfo *info = bmid.length() == 4 ? GetICONInfo(*(uint32*)bmid.begin()) : nullptr;

	if (!info) {
		uint32	type 	= 0;
		int		size 	= bm->Width();
		bool	x2 		= bmid.ends("x2");
		bool	png		= x2 || size >= 128;
		
		switch (size) {
			case 16:	type = png ? 'ipc4' : 'is32'; break;
			case 32:	type = png ? (x2 ? 'ic11' : 'icp5') : 'il32'; break;
			case 48:	type = 'ih32'; break;
			case 64:	type = x2 ? 'ic12' : 'icp6'; break;
			case 128:	type = png ? 'ic07' : 'it32'; break;
			case 256:	type = x2 ? 'ic13' : 'ic08'; break;
			case 512:	type = x2 ? 'ic14' : 'ic09'; break;
			case 1024:	type = 'ic10'; break;
			default:	return false;
		}
		info = GetICONInfo(type);
	}
	
	switch (info->mode & 127) {
		case ICON_PNG: {
			Get("png")->Write(bm, file);
			length	= uint32(file.tell() - start);
			break;
		}

		case ICON_RLE24: {
			int		w	= bm->Width(), h = bm->Height(), n = w * h;
			malloc_block	buffer(n * 4);

			uint8	*p = buffer;
			for (int y = 0; y < h; y++) {
				ISO_rgba	*s	= bm->ScanLine(y);
				for (int x = w; x--; ++p, ++s) {
					p[n * 0] = s->r;
					p[n * 1] = s->g;
					p[n * 2] = s->b;
					p[n * 3] = s->a;
				}
			}

			for (int i = 0; i < 3; i++)
				RLE(file, buffer.slice(i * n, n));

			length	= uint32(file.tell() - start);
			
			uint32	mask = 0;
			switch (w) {
				case 16:	mask	= 's8mk'; break;
				case 32:	mask	= 'l8mk'; break;
				case 48:	mask	= 'h8mk'; break;
				case 128:	mask	= 't8mk'; break;
			}
			if (mask) {
				entry	head_mask;
				head_mask.type = mask;
				head_mask.length = int32(n + sizeof(entry));
				file.write(head_mask);
				file.writebuff(buffer + n * 3, n);
			}
			break;
		
		}
		case ICON_RLE32: {
			int		w	= bm->Width(), h = bm->Height(), n = w * h;
			malloc_block	buffer(n * 4);

			uint8	*p = buffer;
			for (int y = 0; y < h; y++) {
				ISO_rgba	*s	= bm->ScanLine(y);
				for (int x = w; x--; ++p, ++s) {
					p[n * 0] = s->r;
					p[n * 1] = s->g;
					p[n * 2] = s->b;
					p[n * 3] = s->a;
				}
			}

			for (int i = 0; i < 4; i++)
				RLE(file, buffer.slice(i * n, n));

			length	= uint32(file.tell() - start);
			break;
		}
	}

	streamptr	end		= file.tell();
	file.seek(start);

	entry		head;
	head.type	= info->type;
	head.length	= length;

	file.write(head);
	file.seek(end);

	return true;
}

//-----------------------------------------------------------------------------
//	.appiconset
//-----------------------------------------------------------------------------

#include "extra/text_stream.h"

class ICONSETFileHandler : public FileHandler {
	const char*		GetExt() override { return "appiconset"; }
	const char*		GetDescription() override { return "Mac Icon Set"; }
	ISO_ptr<void>	ReadWithFilename(tag id, const filename &fn) override;
	bool			WriteWithFilename(ISO_ptr<void> p, const filename &fn) override;
} iconset;

ISO_ptr<void> ICONSETFileHandler::ReadWithFilename(tag id, const filename &fn) {

	ISO_ptr<ISO_openarray<ISO_ptr<bitmap>>>	p(id);

	JSONval		json	= FileInput(filename(fn).add_dir("Contents.json")).get();

	for (auto &i : json/"images") {
		auto	idiom 		= (i/"idiom").get("");
		auto	name 		= (i/"filename").get("");
		auto	size 		= (i/"size").get("");
		int		scale 		= from_string((i/"scale").get(""));

		fixed_string<256>	id2;
		id2 << idiom << size << 'x' << scale;
		p->Append(Read(id2, filename(fn).add_dir(name)));

	}
	return p;
}

bool ICONSETFileHandler::WriteWithFilename(ISO_ptr<void> p, const filename &fn) {

	if (p.IsType<ISO_openarray<ISO_ptr<bitmap> > >()) {
		ISO_ptr<ISO_openarray<ISO_ptr<bitmap> > >	bms(p);
		FileHandler	*png	= FileHandler::Get("png");
		FileOutput	file(filename(fn).add_dir("Contents.json"));
		JSONwriter	json(file);
		
		json.Begin("", true);
		json.Begin("images", false);

		int	num	= bms->Count();
		for (int i = 0; i < num; i++) {
			ISO_ptr<bitmap>		bm 	= (*bms)[i];
			int		width 	= bm->Width(), height = bm->Height(), scale = 1;
			tag		bmid	= bm.ID();
			
			const ICONInfo *info = bmid.length() == 4 ? GetICONInfo(*(uint32*)bmid.begin()) : nullptr;
			if (info && info->mode & ICON_RETINA) {
				scale = 2;
				width /= 2;
				height /= 2;
			}
				
			fixed_string<64>	name;
			name.format("icon%ix%ix%i.png", width, height, scale);
			json.Object("")
				.Write("size", format_string("%ix%i", width, height))
				.Write("idiom", "mac")
				.Write("filename", name)
				.Write("scale", format_string("%ix", scale));
			png->Write(bm, filename(fn).add_dir(name));
		}
		json.End();
		json.End();
		return true;
	}

	ISO_ptr<bitmap>	bm = ISO_conversion::convert<bitmap>(p, ISO_conversion::RECURSE | ISO_conversion::CHECK_INSIDE);
	if (!bm)
		return false;

	create_dir(fn);

	if (ISO::Browser b = ISO::root("variables")["sizes"]) {
		FileHandler	*png	= FileHandler::Get("png");
		FileOutput	file(filename(fn).add_dir("Contents.json"));
		JSONwriter	json(file);
		
		json.Begin("", true);
		json.Begin("images", false);

		int	num	= b.Count();
		for (int i = 0; i < num; i++) {
			int				size	= b[i].GetInt();

			ISO_ptr<bitmap>	bm2(0);
			bm2->Create(size, size);
			resample_via<HDRpixel>(bm2->All(), bm->All());

			fixed_string<64>	name;
			name.format("icon%ix%i.png", size, size);
#if 0
			if (i > 0)
				json << ",\n";
			json << "\t\t{\n";
			json << "\t\t\t\"size\" : \"" << size << 'x' << size << "\",\n";
			json << "\t\t\t\"idiom\" : \"mac\",\n";
			json << "\t\t\t\"filename\" : \"" << name << "\",\n";
			json << "\t\t\t\"scale\" : \"1x\"\n";
			json << "\t\t}";
#else
			json.Object("")
				.Write("size", format_string("%ix%i", size, size))
				.Write("idiom", "mac")
				.Write("filename", name)
				.Write("scale", "1x");
#endif
			png->Write(bm2, filename(fn).add_dir(name));
		}
		json.End();
		json.End();
		return true;
	}
	return false;
}
