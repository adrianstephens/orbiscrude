#include "bitmap.h"
#include "iso/iso_files.h"
#include "iso/iso_convert.h"

//-----------------------------------------------------------------------------
//	Netpbm portable pixmap format (PPM)
//-----------------------------------------------------------------------------

using namespace iso;

struct PPM {
	static bool		IsWhitespace(int c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n';}
	static int		getc(istream_ref file);
	static int		GetInt(istream_ref file);
	static bool		GetFloat(istream_ref file, float &f);

	static bool Write(ostream_ref file, const HDRbitmap* bm) {
		char	header[64];
		int		width	= bm->Width(), height = bm->Height();

		sprintf(header,"PF\n%d %d\n255\n", width, height);
		file.writebuff(header, strlen(header));

		for (int y = 0; y < height; y++) {
			auto	*srce = bm->ScanLine(y);
			for (int x = 0; x < width; x++, srce++) {
				file.write(srce->r);
				file.write(srce->g);
				file.write(srce->b);
			}
		}
		return true;
	}

	static bool Write(ostream_ref file, const bitmap *bm, char type) {
		char	header[64];
		int		width	= bm->Width(), height = bm->Height();

		sprintf(header,"P%c\n%d %d\n255\n", type, width, height);
		file.writebuff(header, strlen(header));

		for (int y = 0; y < height; y++) {
			auto	*srce = bm->ScanLine(y);
			switch (type) {
				case '2':
					for (int x = 0; x < width; x++, srce++) {
						sprintf(header, "%4d", srce->r);
						file.writebuff(header, strlen(header));
						if ((x & 15) == 15) file.putc('\n');
					}
					break;
				case '3':
					for (int x = 0; x < width; x++, srce++) {
						sprintf(header, "%4d%4d%4d%s", srce->r, srce->g, srce->b, (x % 5) == 4 ? "\n" : "  ");
						file.writebuff(header, strlen(header));
					}
					break;
				case '5':
					for (int x = 0; x < width; x++, srce++)
						file.putc(srce->r);
					break;
				case '6':
					for (int x = 0; x < width; x++, srce++) {
						file.putc(srce->r);
						file.putc(srce->g);
						file.putc(srce->b);
					}
					break;
				case '8':
					file.writebuff(srce, width * 4);
					break;
			}
		}
		return true;
	}

	static bool ReadHeader(istream_ref file, char &type, int &width, int &height) {
		if (file.getc() != 'P')
			return false;

		type = getc(file);
		if ((type < '1' || type > '8' || type == '7') && type != 'F')
			return false;

		if (!IsWhitespace(getc(file)))
			return false;

		if ((width = GetInt(file)) < 0 || (height = GetInt(file)) < 0)
			return false;

		return true;
	}

	static bool ReadImage(istream_ref file, HDRbitmap* bm) {
		float	fmax;
		if (!GetFloat(file, fmax))
			return false;

		int		width	= bm->Width(), height = bm->Height();
		for (int i = 0; i < height; i++) {
			auto	*p	= bm->ScanLine(i);
			for (int x = 0; x < width; x++, p++) {
				p->r = file.get();
				p->g = file.get();
				p->b = file.get();
				p->a = 1.f;
			}
		}
		return true;
	}
	static bool ReadImage(istream_ref file, bitmap *bm, char type) {
		int	max;
		if (type != '1' && type != '4' && (max = GetInt(file)) < 0)
			return false;
		
		int		width	= bm->Width(), height = bm->Height();
		for (int i = 0; i < height; i++) {
			ISO_rgba	*dest	= bm->ScanLine(i);
			switch (type) {
				case '1':
					for (int x = 0; x < width; x++)
						*dest++ = GetInt(file) ? 255 : 0;
					break;
				case '2':
					for (int x = 0; x < width; x++)
						*dest++ = GetInt(file);
					break;
				case '3':
					for (int x = 0; x < width; x++) {
						int r = GetInt(file), g = GetInt(file), b = GetInt(file);
						*dest++ = ISO_rgba(r,g,b,255);
					}
					break;
				case '4':
					for (int x = 0; x < width; x += 8) {
						int byte = file.getc();
						for (int i = x < 8 ? x : 8; i--; byte <<= 1)
							*dest++ = byte & 0x80 ? 255 : 0;
					}
					break;
				case '5':
					for (int x = 0; x < width; x++)
						*dest++ = file.getc();
					break;
				case '6':
					for (int x = 0; x < width; x++) {
						int r = file.getc(), g = file.getc(), b = file.getc();
						*dest++ = ISO_rgba(r,g,b,255);
					}
					break;
				case '8':
					file.readbuff(dest, width * 4);
					break;
			}
		}

		return true;
	}

};

int PPM::getc(istream_ref file) {
	int	c;
	while ((c = file.getc()) == '#') {
		do
			c = file.getc();
		while (c != -1 && c != '\n');
	}
	return c;
}

int PPM::GetInt(istream_ref file) {
	int		c, x;

	while (IsWhitespace(c = getc(file)));
	x = 0;
	while (c >= '0' && c <= '9') {
		x = x * 10 + c - '0';
		c = getc(file);
	}
	return IsWhitespace(c) ? x : -1;
}

bool PPM::GetFloat(istream_ref file, float &f) {
	char	s[64];
	int		c, i;

	while (IsWhitespace(c = getc(file)));

	for (i = 0; i < 64 && !IsWhitespace(c); i++, c = getc(file))
		s[i] = c;

	s[i] = 0;
	return sscanf(s, "%f", &f) > 0;
}


class PPMFileHandler : public FileHandler, public PPM {
	const char*		GetExt() override { return "ppm"; }

	ISO_ptr<void>	ReadImage(istream_ref file);

	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		ISO_ptr<void>	p	= ReadImage(file);
		if (!p)
			return ISO_NULL;

		ISO_ptr<void>	p2	= ReadImage(file);
		if (!p2) {
			p.SetID(id);
			return p;
		}
		ISO_ptr<anything>	anim(id);
		anim->Append(p);
		do {
			anim->Append(p2);
			p2	= ReadImage(file);
		} while (p2);
		return anim;
	}

	bool Write(ISO_ptr<void> p, ostream_ref file) override {
		char	header[64];

		if (p.GetType()->SameAs<HDRbitmap>()) {
			return PPM::Write(file, (const HDRbitmap*)p);

		} else if (ISO_ptr<bitmap> bm = ISO_conversion::convert<bitmap>(p)) {
			return PPM::Write(file, bm, '6');
		}
		return false;
	}
} ppm;

ISO_ptr<void> PPMFileHandler::ReadImage(istream_ref file) {
	char	type;
	int		width, height;

	if (!ReadHeader(file, type, width, height))
		return ISO_NULL;

	if (type == 'F') {
		ISO_ptr<HDRbitmap>	bm(NULL, width, height);
		if (PPM::ReadImage(file, bm))
			return bm;
	} else {
		ISO_ptr<bitmap>	bm(NULL, width, height);
		if (PPM::ReadImage(file, bm, type))
			return bm;
	}
	return ISO_NULL;
}

class PGMFileHandler : public PPMFileHandler {
	const char*		GetExt() override{ return "pgm"; }

	bool Write(ISO_ptr<void> p, ostream_ref file) override {
		if (ISO_ptr<bitmap> bm = ISO_conversion::convert<bitmap>(p)) {
			return PPM::Write(file, bm, '5');
		}
		return false;
	}
} pgm;
