#include "iso/iso_files.h"
#include "iso/iso_convert.h"

using namespace iso;

struct text8 : ISO_openarray<char>		{ text8()	{} text8(uint32 n)	: ISO_openarray<char>(n) {} };
struct text16 : ISO_openarray<char16>	{ text16()	{} text16(uint32 n)	: ISO_openarray<char16>(n) {} };
ISO_DEFUSER(text8, ISO_openarray<char>);
ISO_DEFUSER(text16, ISO_openarray<char16>);

class TXTFileHandler : public FileHandler {
	const char*		GetExt() override { return "txt";				}
	const char*		GetDescription() override { return "Text";			}
	int				Check(istream_ref file) override { return CHECK_POSSIBLE;	}
	ISO_ptr<void>	Read(tag id, istream_ref file) override;
	bool			Write(ISO_ptr<void> p, ostream_ref file) override;
	bool			WriteWithFilename(ISO_ptr<void> p, const filename &fn) override {
		if (p.GetType()->SameAs<anything>()) {
			for (auto &i : *(anything*)p) {
				filename	fn2 = fn;
				if (i.ID()) {
					if (!fn2.name().blank())
						fn2.set_ext(i.ID().get_tag() +  fn.ext());
					else
						fn2.rem_dir().add_dir(i.ID().get_tag()).add_ext(fn.ext());
				}

				if (!Write(i, FileOutput(fn2).me()))
					return false;
			}
			return true;
		}
		return Write(p, FileOutput(fn).me());
	}
} txt;

ISO_ptr<void> TXTFileHandler::Read(tag id, istream_ref file) {
	uint32	length = file.size32();
	int		c1 = file.getc(), c2 = file.getc();

	if (c1 == 0xff && c2 == 0xfe) {					// little endian UTF16
		ISO_ptr<text16>	p(id, (length - 2) / 2);
		file.readbuff(*p, length - 2);
		return p;

	} else if (c1 == 0xfe && c2 == 0xff) {			// big endian UTF16
		ISO_ptr<text16>	p(id, (length - 2) / 2);
		char16	*buffer	= *p;
		file.readbuff(buffer, length - 2);
		for (int i = 0; i < (length - 2) / 2; i++)
			buffer[i] = ((char16be*)buffer)[i];
		return p;

	} else if (c1 == 0xef && c2 == 0xbb && file.getc() == 0xbf) {
		length	-= 3;
		char*	srce	= new char[length];
		char16*	dest	= new char16[length];
		file.readbuff(srce, length);
		size_t	len2	= chars_copy(dest, srce, length);
		ISO_ptr<text16>	p(id, uint32(len2));
		memcpy(*p, dest, len2 * sizeof(uint16));

		delete[] srce;
		delete[] dest;
		return p;

	} else if (c1 == 0) {
		ISO_ptr<text16>	p(id, length / 2);
		char16	*buffer	= *p;
		buffer[0] = (c2 << 8) | c1;
		file.readbuff(buffer + 1, length - 2);
		for (int i = 0; i < length / 2; i++)
			buffer[i] = ((char16be*)buffer)[i];
		return p;

	} else if (c2 == 0) {
		ISO_ptr<text16>	p(id, length / 2);
		char16	*buffer	= *p;
		buffer[0] = (c2 << 8) | c1;
		file.readbuff(buffer + 1, length - 2);
		return p;

	} else {
		ISO_ptr<text8>	p(id, length);
		if (length > 0) {
			char	*b	= *p;
			b[0] = c1;
			if (length > 1) {
				b[1] = c2;
				file.readbuff(b + 2, length - 2);
			}
		}
		return p;
	}
}

bool TXTFileHandler::Write(ISO_ptr<void> p, ostream_ref file) {
	if (p.GetType()->SameAs<ISO_openarray<char16> >()) {
		ISO_openarray<char16>	*t		= p;
		uint32					length	= t->Count();
		file.putc(0xef);
		file.putc(0xbb);
		file.putc(0xbf);

		char	chars[8];
		for (char16 *buffer = *t; length--; buffer++)
			file.writebuff(chars, put_char(*buffer, chars, false));
	} else if (ISO_ptr<ISO_openarray<char> > t = ISO_conversion::convert<ISO_openarray<char> >(p)) {
//		p.GetType()->SameAs<ISO_openarray<char> >()) {
//		ISO_openarray<char>		*t = p;
		file.writebuff(*t, t->Count());
		return true;

	}
	return false;
}

ISO_ptr<text8> Dos2Unix(holder<ISO_ptr<text8> > text) {
	int		n = text->Count(), ncr = 0;
	char	*p = *text.t;
	for (int i = 0; i < n; i++)
		ncr += *p++ == '\r';

	ISO_ptr<text8>	text2(text.t.ID(), n - ncr);

	char	*p1 = *text.t;
	char	*p2	= *text2;
	for (int i = 0; i < n; i++) {
		char	c = *p1++;
		if (c != '\r')
			*p2++ = c;
	}
	return text2;
}

ISO_ptr<text8> Unix2Dos(holder<ISO_ptr<text8> > text) {
	int		n = text->Count(), ncr = 0;
	char	*p = *text.t;
	for (int i = 0; i < n; i++)
		ncr += *p++ == '\n';

	ISO_ptr<text8>	text2(text.t.ID(), n + ncr);

	char	*p1 = *text.t;
	char	*p2	= *text2;
	for (int i = 0; i < n; i++) {
		char	c = *p1++;
		if (c == '\n')
			*p2++ = '\r';
		*p2++ = c;
	}
	return text2;
}

static initialise init(
	ISO_get_operation(Dos2Unix),
	ISO_get_operation(Unix2Dos)
);
