#include "base/strings.h"
#include "bitmap.h"
#include "iso/iso_files.h"
#include "iso/iso_convert.h"
#include "codec/texels/dxt.h"

//-----------------------------------------------------------------------------
//	BIF (Binary Information File)
//	Text-based file format associated with Image Alchemy v1.5+ which describes the format of a particular raw bitmap file
//-----------------------------------------------------------------------------

using namespace iso;

class BIFFileHandler : FileHandler {
	static const char*	tags[];

	static char*	ReadLine(istream_ref file, char *line, int maxlen);
	static size_t	Read(istream_ref file, void *buffer, size_t size, int bitspersample, bool ascii, bool msb);
	static void		Write(ostream_ref file, const void *buffer, size_t size, int bitspersample, bool ascii);

	enum {
		TAG_FILENAME,		// The name of the file containing the binary data.
		TAG_WIDTH,			// The width of the image data, in pixels.
		TAG_HEIGHT,			// The height of the image data, in pixels.
		TAG_PLANES,			// The number of planes of image data (1, 2, 3, or 4). A 1 plane image is assumed to be gray-scale,
							// a 2 plane image is a gray-scale image with an alpha channel,
							// a 3 plane image is a RGB image, and a 4 plane image is a RGB image with an alpha channel.
		TAG_CHANNELS,		// as above
		TAG_HEADER,			// The size of the header, in bytes.  This many bytes will be skipped when reading the file.
		TAG_LEFTPADDING,	// The number of bytes to remove from the beginning of each scan line.
		TAG_RIGHTPADDING,	// The number of bytes to remove from the end of each scan line.
		TAG_ORDER,			// The order of the pixels. For 3 channel images, this can be any sequence of r, g, and b: rgb, rbg, grb, gbr, brg, or bgr
							// For 4 channel images, this can be any sequence of a, r, g, and b. Either ga or ag for 2 channel images (g=gray, a=alpha).
							// The defaults are g, ga, rgb, and rgba, depending on the number of planes.
		TAG_UPSIDEDOWN,		// The presence of this tag indicates that the data in the file is recorded from the bottom of the screen up to the top of the screen.

		TAG_INTERLEAVE,		// 0 = byte interleave (RGBRGBRGBRGB). 1 = line interleaving RRR..GGG..BBB..RRR... 2 = plane interleaving (RRRRRRRR GGGGGGGG BBBBBBBB).
		TAG_FORMAT,			// Format of the Raw file. "ascii" = using comma-delimited values in an ASCII format. "group3" = compressed using CCITT Group 3 encoding. "group4" = CCITT Group 4-compressed data.
		TAG_FRAMES,			// # frames for animation
		TAG_FRAMEGAP,		// gap between frames

//unsupported
		TAG_FAXOPTIONS,		// "bitreversal" indicates that facsimile-compressed data are stored with the most-significant bit in each byte first.
		TAG_BITSPERSAMPLE,	// Valid tag values are 1 (1-bit per sample, 8 samples per byte), 8 (one byte per sample), and 16 (two bytes per sample). The default is 8.
		TAG_BITORDER,		// The order of bits are stored within a byte. "msb" (default) or "lsb".
		TAG_BYTEORDER,		// indicates whether a 16-bit pixel value is stored using the little-endian (tag value of "intel") or the big-endian (tag value of "motorola") byte order. The default is "motorola".
		TAG_SIGNED,			// indicates that 16-bit pixels are stored as signed values. The default is 16-bit pixels stored as unsigned values. There is no associated value with this tag.

		TAG_LAST,
	};

	const char*		GetExt() override { return "bif"; }
	ISO_ptr<void>	ReadWithFilename(tag id, const filename &fn) override;
	bool			WriteWithFilename(ISO_ptr<void> p, const filename &fn) override;
} bif;

const char*	BIFFileHandler::tags[] = {
	"filename",
	"width",
	"height",
	"planes",
	"channels",
	"header",
	"leftpadding",
	"rightpadding",
	"order",
	"upsidedown",
	"interleave",
	"format",
	"frames",
	"framegap",
	"faxoptions",
	"bitspersample",
	"bitorder",
	"byteorder",
	"signed",
};

char* BIFFileHandler::ReadLine(istream_ref file, char *line, int maxlen) {
	int	i, c;
	do c = file.getc(); while (c == '\r' || c == '\n');

	if (c == EOF)
		return NULL;

	for (i = 0; c != EOF && c != '\n' && c != '\r'; c = file.getc(), i++) {
		if (i == maxlen)
			return NULL;
		line[i] = c;
	}
	line[i] = 0;
	return line;
}

void BIFFileHandler::Write(ostream_ref file, const void *buffer, size_t size, int bitspersample, bool ascii) {
	if (ascii) {
		char	string[5];
		for (size_t i = 0; i < size; i++) {
			sprintf(string, "%3i,", ((unsigned char*)buffer)[i]);
			file.writebuff(string, i == size - 1 ? 3 : 4);
		}
		file.writebuff("\r\n", 2);
	} else {
		switch (bitspersample) {
			case 1: {
				int		b	= 0;
				char	c	= 0;
				for (int i = 0; i < size; i++) {
					c = (c << 1) | (((char*)buffer)[i] != 0);
					if (++b == 8) {
						file.putc(c);
						b	= 0;
					}
				}
				if (b)
					file.putc(c);
				break;
			}
			case 8:
				file.writebuff(buffer, size);
				break;
			case 16:
				break;
			default: {
				int	b = 0, c = 0;
				for (int i = 0; i < size; i++) {
					c  = (c << bitspersample) | (((unsigned char*)buffer)[i] >> (8 - bitspersample));
					b += bitspersample;
					while (b >= 8) {
						file.putc(c);
						b	-=  8;
						c	>>= 8;
					}
				}
				if (b)
					file.putc(c);
				break;
			}
		}
	}
}

size_t BIFFileHandler::Read(istream_ref file, void *buffer, size_t size, int bitspersample, bool ascii, bool msb) {
	if (ascii) {
		size_t i;
		for (i = 0; i < size; i++) {
			int		c, x;

			c = file.getc();

			while (c == ' ' || c == '\t' || c == '\r' || c == '\n')
				c = file.getc();

			x = 0;
			while (c >= '0' && c <= '9') {
				x = x * 10 + c - '0';
				c = file.getc();
			}

			while (c == ' ' || c == '\t')
				c = file.getc();

			if (c != (i == size - 1 ? '\r' : ','))
				break;

			switch (bitspersample) {
				case 1:		((unsigned char*)buffer)[i] = x ? 0xff : 0;	break;
				case 8:		((unsigned char*)buffer)[i] = x;			break;
				case 16:	((unsigned char*)buffer)[i] = x >> 8;		break;
			}
		}
		return i;

	} else {
		switch (bitspersample) {
			case 1: {
				size_t	bytes	= (size + 7) / 8;
				char	*p		= (char*)buffer + size - bytes;
				int		c		= 0x8000;
				file.readbuff(p, bytes);

				for (int i = 0; i < size; i++) {
					c <<= 1;
					if ((i & 7) == 0)
						c	= *p++;
					((char*)buffer)[i] = c & 0x80 ? 0xff : 0;
				}
				return size;
			}
			case 8:
				return file.readbuff(buffer, size);
			case 16:
				return 0;
			default: {
				size_t			bytes	= (size * bitspersample + 7) / 8;
				unsigned char	*p		= (unsigned char*)buffer + size - bytes;
				int				b		= 0;
				int				m		= (1 << bitspersample) - 1;
				int				c		= 0;
				file.readbuff(p, bytes);

				if (msb) {
					for (int i = 0; i < size; i++) {
						while (b < bitspersample) {
							c	= (c << 8) | *p++;
							b	+= 8;
						}
						((char*)buffer)[i] = ((c >> (b - bitspersample)) & m) * 255 / m;
						b -= bitspersample;
					}
				} else {
					for (int i = 0; i < size; i++) {
						while (b < bitspersample) {
							c	|= *p++ << b;
							b	+= 8;
						}
						((char*)buffer)[i] = (c & m) * 255 / m;
						b -= bitspersample;
						c >>= bitspersample;
					}
				}
				return size;
			}
		}
	}
}

void swap16(void *p, int n) {
	copy_n((uint16be*)p, (uint16*)p, n);
}

ISO_ptr<void> BIFFileHandler::ReadWithFilename(tag id, const filename &fn) {
	FileInput	file(fn);
	filename	filenames[3];

	char		line[256];
	if (!ReadLine(file, line, 256) || strcmp(line, "BIF"))
		return ISO_NULL;

	char		order[4] = {0,1,2,3};
	int			files = 0, width = 0, height = 0, planes = 0, header = 0, leftpad = 0, rightpad = 0, interleave = 0, bitspersample = 8, dxt = 0;
	int			frames = 0, framegap = 0;
	bool		upsidedown = false, ascii = false, msb = true, bigendian = true;


//	Path	path(GetFilename());
	while (ReadLine(file, line, 256)) {
		if (line[0] == '#')
			continue;
		unsigned int taglen = 0;
		while (line[taglen] && line[taglen] != ' ' && line[taglen] != '\t')
			taglen++;
		if (taglen == 0)
			continue;

		const char *param = line + taglen;
		while (*param == ' ' || *param == '\t')
			param++;

		int	tag = 0;
		while (tag < TAG_LAST) {
			if (strlen(tags[tag]) == taglen && strncmp(line, tags[tag], taglen) == 0)
				break;
			tag++;
		}

		switch (tag) {
			case TAG_FILENAME:
				if (files == 4)
					return ISO_NULL;
//				path.Absolute(param, fn[files++]);
				filenames[files++] = filename(fn).relative(param);
				break;

			case TAG_WIDTH:
				from_string(param, width);
				break;

			case TAG_HEIGHT:
				from_string(param, height);
				break;

			case TAG_PLANES:
			case TAG_CHANNELS:
				from_string(param, planes);
				break;

			case TAG_HEADER:
				header = from_string<int>(param);//sscanf(param, "%i", &header);//	= atoi(param);
				break;

			case TAG_LEFTPADDING:
				from_string(param, leftpad);
				break;

			case TAG_RIGHTPADDING:
				from_string(param, rightpad);
				break;

			case TAG_ORDER: {
				char	channels[5];
				char	c, *p;
				int		i = 0, maxch = 0;
				strcpy(channels, "rgba");
				while ((c = *param++) && (p = (char*)memchr(channels, c, 4))) {
					int	ch = int(p - channels);
					if (ch > maxch)
						maxch = ch;
					order[i++] = ch;
					*p = 0;
				}
				if (!planes)
					planes = i;
				else if (planes != i)
					return ISO_NULL;
				if (planes < 3) {
					for (i = 0; i < planes; i++) {
						if (order[i] == 3)
							order[i] = 1;
						else if (order[i] == 1)
							order[i] = 0;
						else
							return ISO_NULL;
					}
				} else if (i != maxch + 1)
					return ISO_NULL;
				break;
			}
			case TAG_UPSIDEDOWN:
				upsidedown = true;
				break;

			case TAG_INTERLEAVE:
				from_string(param, interleave);
				break;

			case TAG_FORMAT:
				if (strcmp(param, "ascii") == 0) {
					ascii = true;
				} else if (strcmp(param, "DXT1") == 0) {
					dxt	= 1;
				} else if (strcmp(param, "DXT2") == 0) {
					dxt	= 2;
				} else if (strcmp(param, "DXT4") == 0) {
					dxt	= 3;
				} else {
					return ISO_NULL;
				}
				break;

			case TAG_FRAMES:
				from_string(param, frames);
				break;

			case TAG_FRAMEGAP:
				from_string(param, framegap);
				break;

			case TAG_FAXOPTIONS:
				return ISO_NULL;

			case TAG_BITSPERSAMPLE:
				from_string(param, bitspersample);
//					if (bitspersample != 1 && bitspersample != 8 && bitspersample != 16)
//						return false;
				break;

			case TAG_BITORDER:
				if (strcmp(param, "msb") == 0)
					msb = true;
				else if (strcmp(param, "lsb") == 0)
					msb = false;
				else
					return ISO_NULL;
				break;

			case TAG_BYTEORDER:
				if (strcmp(param, "intel") == 0)
					bigendian = false;
				else if (strcmp(param, "motorola") == 0)
					bigendian = true;
				else
					return ISO_NULL;
				break;

			case TAG_SIGNED:
			case TAG_LAST:
				return ISO_NULL;
		}
	}
	if (files == 0)
		return ISO_NULL;

	if (planes == 0)
		planes = 1;

	FileInput	file2(filenames[0]);
	if (!file2.exists())
		return ISO_NULL;

	if (width == 0 || height == 0) {
		int		fplanes	= files == 1 ? planes : 1;
		int64	length	= file2.length() - header;
		if (width == 0) {
			if (height == 0)
				return ISO_NULL;
			width = int((length / height - leftpad - rightpad) / fplanes);
		} else {
			height = int(length / ((width * bitspersample + 7) / 8 * fplanes + leftpad + rightpad));
		}
	}

	ISO_ptr<ISO_openarray<ISO_ptr<bitmap> > >	array;
	ISO_ptr<bitmap>								bm;

	if (frames > 1) {
		array.Create(id);
	} else {
		frames = 1;
		bm.Create(id);
	}

	for (int frame = 0; frame < frames; frame++) {
		if (array) {
			char	name[64];
			sprintf(name, "%i", frame);
			bm.Create(name);
		}
		bm->Create(width, height);

		switch (dxt) {
			case 1: {
				malloc_block	buffer(width / 4 * 8);
				file2.seek(header + leftpad);
				for (int y = 0; y < height; y += 4) {
					file2.readbuff(buffer, width / 4 * 8);
					void	*p = buffer;
					for (int x = 0; x < width; x += 4, p = (uint8*)p + 8) {
						if (bigendian)
							swap16(p, 4);
						((DXT1rec*)p)->Decode(bm->Block(x, y, 4, 4));
					}
				}
			}
			case 2: {
				malloc_block	buffer(width / 4 * 16);
				file2.seek(header + leftpad);
				for (int y = 0; y < height; y += 4) {
					file2.readbuff(buffer, width / 4 * 16);
					void	*p = buffer;
					for (int x = 0; x < width; x += 4, p = (uint8*)p + 16) {
						if (bigendian)
							swap16(p, 8);
						((DXT23rec*)p)->Decode(bm->Block(x, y, 4, 4));
					}
				}
			}

			case 3: {
				malloc_block	buffer(width / 4 * 16);
				file2.seek(header + leftpad);
				for (int y = 0; y < height; y += 4) {
					file2.readbuff(buffer, width / 4 * 16);
					void	*p = buffer;
					for (int x = 0; x < width; x += 4, p = (uint8*)p + 16) {
						if (bigendian)
							swap16(p, 8);
						((DXT45rec*)p)->Decode(bm->Block(x, y, 4, 4));
					}
				}
			}
			default: {
				// NOT DXT

				if (planes == 4 || planes == 2) {
				//	bm.SetFlag(BMF_ALPHA);
				} else {
					for (int y = 0; y < height; y++) {
						ISO_rgba	*line = bm->ScanLine(y);
						for (int x = 0; x < width; x++)
							line[x].a = 255;
					}
				}
			//	if (planes < 3)
			//		bm.SetFlag(BMF_INTENSITY);

				if (files == 1) {
					unsigned char *buffer = new unsigned char[width * planes];
					file2.seek(header + leftpad);

					switch (interleave) {
						case 0: {// byte interleave
							for (int y = 0; y < height; y++) {
								unsigned char	*line = (unsigned char*)bm->ScanLine(upsidedown ? height - 1 - y : y);
								Read(file2, buffer, width * planes, bitspersample, ascii, msb);
								for (int c = 0; c < planes; c++) {
									unsigned char	*srce = buffer + c;
									unsigned char	*dest = line + (planes == 2 && order[c] == 1 ? 3 : order[c]);
									for (int x = 0; x < width; x++, srce += planes, dest += 4)
										*dest = *srce;
								}
								file2.seek_cur(leftpad + rightpad);
							}
							break;
						}

						case 1: {//line interleave
							for (int y = 0; y < height; y++) {
								unsigned char	*line = (unsigned char*)bm->ScanLine(upsidedown ? height - 1 - y : y);
								for (int c = 0; c < planes; c++) {
									unsigned char	*srce = buffer;
									unsigned char	*dest = line + (planes == 2 && order[c] == 1 ? 3 : order[c]);
									Read(file2, buffer, width, bitspersample, ascii, msb);
									for (int x = 0; x < width; x++, dest += 4)
										*dest = *srce++;
									file2.seek_cur(leftpad + rightpad);
								}
							}
							break;
						}

						case 2: {//plane interleave
							for (int c = 0; c < planes; c++) {
								for (int y = 0; y < height; y++) {
									Read(file2, buffer, width, bitspersample, ascii, msb);
									unsigned char	*srce = buffer;
									unsigned char	*dest = (unsigned char*)bm->ScanLine(upsidedown ? height - 1 - y : y) + (planes == 2 && order[c] == 1 ? 3 : order[c]);
									for (int x = 0; x < width; x++, dest += 4)
										*dest = *srce++;
									file2.seek_cur(leftpad + rightpad);
								}
							}
							break;
						}
					}
					delete[] buffer;

				} else if (files == planes) {
					unsigned char *buffer = new unsigned char[width];
					for (int c = 0; c < planes; c++) {
						FileInput	file2(filenames[c]);
						if (!file2.exists()) {
							delete[] buffer;
							return ISO_NULL;
						}
						file2.seek(header + leftpad);
						for (int y = 0; y < height; y++) {
							Read(file2, buffer, width, bitspersample, ascii, msb);
							unsigned char	*srce = buffer;
							unsigned char	*dest = (unsigned char*)bm->ScanLine(upsidedown ? height - 1 - y : y) + (planes == 2 && order[c] == 1 ? 3 : order[c]);
							for (int x = 0; x < width; x++, dest += 4)
								*dest = *srce++;
							file2.seek_cur(leftpad + rightpad);
						}
					}
					delete[] buffer;
				} else {
					// error
				}
				if (array) {
					header += framegap;
					array->Append(bm);
					bm = ISO_NULL;
				}
			}
		}
	}
	if (array)
		return array;
	return bm;
}

bool BIFFileHandler::WriteWithFilename(ISO_ptr<void> p, const filename &fn) {
	ISO_ptr<bitmap> bm = ISO_conversion::convert<bitmap>(p);
	if (!bm)
		return false;

	FileOutput	file(fn);
	filename	path(fn);

	bool	ascii			= false;//(flags & BMWF_USERFLAG) != 0;
	int		bitspersample	= 8;//bm->IsIntensity() ? BMWF_GETINTENSITYBITS(flags) : BMWF_GETBITS(flags);
	bool	separate		= false;//(flags & BMWF_TWIDDLE) != 0;

	if (bitspersample == 0)
		bitspersample = 8;
	else if (bitspersample > 8)
		bitspersample	= 16;
//		else if (bitspersample > 1)
//			bitspersample	= 8;

	int		width	= bm->Width();
	int		height	= bm->Height();
	int		planes	= (bm->IsPaletted() || bm->IsIntensity() ? 1 : 3) + (int)bm->HasAlpha();
	static const char	*orders[]	= {"g", "ga", "rgb", "rgba"};
	char	string[1024];

	file.writebuff("BIF\r\n", 5);

	if (separate) {
		char	ext[2] = "?";
		unsigned char *buffer = new unsigned char[width];
		for (int c = 0; c < planes; c++) {
			ext[0] = orders[planes-1][c];
			path.set_ext(ext);
			sprintf(string, "filename %s\r\n", (const char*)path);
			file.writebuff(string, strlen(string));

			FileOutput	file2(path);
			if (!file2.exists()) {
				delete[] buffer;
				return false;
			}

			for (int y = 0; y < height; y++) {
				unsigned char	*srce = (unsigned char*)bm->ScanLine(y) + (c && planes == 2 ? 3 : c);
				unsigned char	*dest = buffer;
				for (int x = 0; x < width; x++, srce += 4)
					*dest++ = *srce;
				Write(file2, buffer, width, bitspersample, ascii);
			}
		}
		delete[] buffer;
	} else {
		path.set_ext("raw");
		sprintf(string, "filename %s\r\n", (const char*)path);
		file.writebuff(string, strlen(string));

		FileOutput	file2(path);
		if (!file2.exists())
			return false;

		unsigned char *buffer = new unsigned char[width * planes];
		for (int y = 0; y < height; y++) {
			unsigned char	*line = (unsigned char*)bm->ScanLine(y);
			for (int c = 0; c < planes; c++) {
				unsigned char	*srce = line + (c && planes == 2 ? 3 : c);
				unsigned char	*dest = buffer + c;
				for (int x = 0; x < width; x++, srce += 4, dest += planes)
					*dest = *srce;
			}
			Write(file2, buffer, width * planes, bitspersample, ascii);
		}
		delete[] buffer;
	}

	sprintf(string,
		"width %8i\r\n"
		"height %7i\r\n"
		"header       0\r\n"
		"channels     %i\r\n"
		"bitspersample %i\r\n"
		"order      %3s\r\n"
		"leftpadding  0\r\n"
		"rightpadding 0\r\n",
		width,
		height,
		planes,
		bitspersample,
		orders[planes - 1]
	);
	file.writebuff(string, strlen(string));
	if (ascii)
		file.writebuff("format   ascii\r\n", 16);
	return true;
}
