#include "bitmap.h"
#include "iso/iso_files.h"
#include "iso/iso_convert.h"

//-----------------------------------------------------------------------------
//	3DS Max animated bitmaps
//-----------------------------------------------------------------------------

using namespace iso;

class IFLFileHandler : FileHandler {
	const char*		GetExt() override { return "ifl";	}
	ISO_ptr<void>	ReadWithFilename(tag id, const filename &fn) override;
	bool			WriteWithFilename(ISO_ptr<void> p, const filename &fn) override;
} ifl;

ISO_ptr<void> IFLFileHandler::ReadWithFilename(tag id, const filename &fn) {
	FileInput				file(fn);
	ISO_ptr<bitmap_anim>	anim(id);

	char	line[_MAX_PATH];
	int		c;

	for (;;) {

		do c = file.getc(); while (c == '\r' || c == '\n' || c == ' ');

		if (c == EOF)
			break;

		int	i;
		for (i = 0; i < _MAX_PATH && c != EOF && c != '\n' && c != '\r'; i++) {
			line[i] = c;
			c = file.getc();
		}

		while ((c = line[i-1]) == ' ' || c == '\t')
			i--;
		int	endoffn = i;

		while ((c = line[i-1]) >= '0' && c <= '9')
			i--;

		if (c == ' ' || c == '\t')
			endoffn = i - 1;

		while ((c = line[i++]) == ' ');

		int	n = 0;
		while (c >= '0' && c <='9') {
			n = n * 10 + c - '0';
			c = line[i++];
		}
		if (n == 0)
			n = 1;

		line[endoffn] = 0;

		if (line[0] == '"') {
			for (i = 0; (c = line[i+1]) && (c != '"'); i++)
				line[i] = c;
		}

		if (ISO_ptr<bitmap> bm = ISO_conversion::convert<bitmap>(Read(line, filename(fn).rem_dir().add_dir(line))))
			anim->Append(make_pair(bm, n / 30.f));
	}
	return anim;
}

bool IFLFileHandler::WriteWithFilename(ISO_ptr<void> p, const filename &fn) {
	if (!p.GetType()->SameAs<bitmap_anim>()) {
		if (p.GetType()->SameAs<anything>()) {
			anything&				a	= *(anything*)p;
			ISO_ptr<bitmap_anim>	b(NULL);
			for (int i = 0, n = a.Count(); i < n; i++) {
				if (ISO_ptr<bitmap> bm = ISO_conversion::convert<bitmap>(a[i]))
					b->Append(make_pair(bm, 1.f));
				else
					return false;
			}
			p	= b;
		} else {
			return false;
		}
	}

	FileOutput				file(fn);
	ISO_ptr<bitmap_anim>	anim	= p;
	filename				name	= fn.name();
	char					*digits	= name + strlen(name);

	for (int i = 0, n = anim->Count(); i < n; i++) {
		sprintf(digits, "_%03i.tga", i);
		ISO_ptr<bitmap>	&bm = (*anim)[i].a;
		bm->Unpalette();
		if (!Write(bm, filename(fn).rem_dir().add_dir(name)))
			return false;

		int	f = int((*anim)[i].b * 30);
		if (f > 1)
			sprintf(digits + 8, " %i", f);

		file.writebuff(name, strlen(name));
		file.putc('\r');
		file.putc('\n');
	}

	return true;
}
