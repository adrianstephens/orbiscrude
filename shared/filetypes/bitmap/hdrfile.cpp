#include "bitmapfile.h"
#include "utilities.h"

//-----------------------------------------------------------------------------
//	Radiance HDR
//-----------------------------------------------------------------------------

using namespace iso;

#define MINRUN		4		// minimum run length
#define	MAXLINE		512

class HDRFileHandler : public BitmapFileHandler {
	const char*		GetExt()			override { return "hdr"; }
	const char*		GetDescription()	override { return "High Dynamic Range"; }
	ISO_ptr<void>	Read(tag id, istream_ref file) override;
	bool			Write(ISO_ptr<void> p, ostream_ref file) override;
} hdr;


static int GetString(istream_ref file, char *buffer, int len) {
	int	i;
	for (i = 0; i < len; ) {
		char	c = file.getc();
		if (c == '\n')
			break;
		if (c != '\r')
			buffer[i++] = c;
	}
	if (i < len)
		buffer[i] = 0;
	return i;
}

ISO_ptr<void> HDRFileHandler::Read(tag id, istream_ref file) {
	const char	fmt[]	= "32-bit_rle_rgbe";
	float	exposure	= 1.0f;
	char	buf[MAXLINE];

	GetString(file, buf, MAXLINE);
	if (strcmp(buf, "#?RADIANCE"))
		return ISO_NULL;

	while (GetString(file, buf, MAXLINE)) {
		if (buf[0] == '#')
			continue;

		if (strncmp(buf, "FORMAT=", 7) == 0) {
			if (!strncmp(buf + 7, fmt, 15) == 0)
				return ISO_NULL;

		} else if (strncmp(buf, "EXPOSURE=", 9) == 0) {
			from_string(buf + 9, exposure);
		}
	}

	GetString(file, buf, MAXLINE);
	char  *xndx = string_find(buf, 'X'), *yndx = string_find(buf, 'Y');
	if (!xndx || !yndx)
		return ISO_NULL;

	bool	xmaj = yndx > xndx;
	if (xmaj)
		swap(xndx, yndx);

	int		xmax	= from_string(xndx + 1);
	int		ymax	= from_string(yndx + 1);
	int		xinc	= xndx[-1] == '-' ? -1 : 1;
	bool	yflip	= yndx[-1] == '+';

	ISO_ptr<HDRbitmap>	bm(id, xmax, ymax);
	temp_array<rgbe>	buffer(xmax);

	for (int y = 0; y < ymax; y++) {
		int		len			= xmax;
		rgbe	*scanline	= buffer;

		*scanline = file.get();

		if (scanline->v.x != 2 || scanline->v.y != 2 || scanline->v.z & 128) {
			len--;
			scanline++;

			int  rshift	= 0;
			while (len > 0) {
				*scanline = file.get();
				if (scanline->v.x == 1 && scanline->v.y == 1 && scanline->v.z == 1) {
					for (int i = scanline->v.w << rshift; i--; --len, ++scanline)
						scanline[0] = scanline[-1];
					rshift += 8;
				} else {
					++scanline;
					--len;
					rshift = 0;
				}
			}
		} else {
			int	n = (scanline->v.z << 8) | scanline->v.w;
			if (n != len)
				return ISO_NULL;		// length mismatch!

			for (int i = 0; i < 4; i++) {
				for (fixed_stride_iterator<uint8, 4> p((uint8*)scanline + i), e = p + n; p != e; ) {
					int	code = file.getc();
					if (code > 128) {	// run
						code &= 127;
						int	val = file.getc();
						while (code--)
							*p++ = val;
					} else {			// non-run
						while (code--)
							*p++ = file.getc();
					}
				}
			}
		}

		HDRpixel	*dest	= bm->ScanLine(yflip ? ymax - 1 - y : y);
		if (xinc < 0)
			dest += xmax;
		for (auto &p : buffer) {
			assign(*dest, p);
			dest += xinc;
		}

	}
	return bm;
}

bool HDRFileHandler::Write(ISO_ptr<void> p, ostream_ref file) {
	ISO_ptr<HDRbitmap> bm = ISO_conversion::convert<HDRbitmap>(p);
	if (!bm)
		return false;

	file.write(format_string(
		"#?RADIANCE\n"
		"FORMAT=32-bit_rle_rgbe\n"
		"\n"
		"-Y %d +X %d\n",
		bm->Height(), bm->Width()
	));

	int		len	= bm->Width();
	temp_array<rgbe>	buffer(len);

	for (int y = 0; y < bm->Height(); y++) {
		HDRpixel	*p = bm->ScanLine(y);
		for (auto &x : buffer)
			x = rgbe(p++->rgb);

		file.putc(2);
		file.putc(2);
		file.putc(len >> 8);
		file.putc(len & 255);

		for (int i = 0; i < 4; i++) {
			for (fixed_stride_iterator<uint8, 4> p((uint8*)buffer.begin() + i), e = p + len; p < e;) {
				int		run = 0;
				auto	beg = p;
				while (beg < e) {
					for (run = 1; run < 127 && beg + run < e && beg[run] == beg[0]; ++run);
					if (run >= MINRUN)
						break;			// long enough
					beg += run;
				}
				if (beg - p > 1 && beg - p < MINRUN) {
					for (auto	p2 = p + 1; *p2++ == *p;) {
						if (p2 == beg) {	// short run
							file.putc(beg - p + 128);
							file.putc(*p);
							p = beg;
							break;
						}
					}
				}
				// write out non-run
				while (int n = min(beg - p, 128)) {
					file.putc(n);
					while (n--)
						file.putc(*p++);
				}
				if (run >= MINRUN) {
					// write out run
					file.putc(run + 128);
					file.putc(*beg);
					p += run;
				}
			}
		}
	}

	return true;
};
