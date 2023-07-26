#include "iso/iso_files.h"
#include "comms/zlib_stream.h"
#include "filetypes/bitmap/bitmap.h"
#include "base/bits.h"
#include "extra/text_stream.h"

using namespace iso;

class PDFFileHandler : public FileHandler {
	const char*		GetExt() override { return "pdf";				}
	const char*		GetDescription() override { return "Portable Document Formt";	}
	int				Check(istream_ref file) override {
		file.seek(0);
		return file.get<fixed_string<16> >().begins("%PDF") ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}

	ISO_ptr<void>	Read(tag id, istream_ref file) override;
	bool			Write(ISO_ptr<void> p, ostream_ref file) override { return false;			}
} pdf;

struct PDFReader {
	istream_ref					file;
	int						lastchar;
	ISO_ptr<void>			root;

	struct xrefentry {
		streamptr		offset;
		int				gen;
		ISO_ptr<void>	ptr;
	};
	struct xrefblock : dynamic_array<xrefentry> {
		int			first;
		xrefblock(int _first, int _count) : dynamic_array<xrefentry>(_count), first(_first) {}
	};
	dynamic_array<xrefblock>	xrefs;

	int						GetChar();
	void					PutBack(int c)		{ lastchar = c;	}
	int						SkipWhiteSpace();
	char*					ReadLine(char *line, int maxlen);
	char*					GetName(char *name);
//	ISO_ptr<void>			ReadValue(char *name);
	bool					ReadValue(tag name, ISO_ptr<void> &p0);
	bool					GetXRefs(streamptr pos);
	ISO_ptr<void>			Reference(int v);

	PDFReader(istream_ref _file) : file(_file), lastchar(0)	{};
};

int PDFReader::GetChar() {
	int	c;
	if (c = lastchar)
		lastchar = 0;
	else
		c = file.getc();
	return c;
}

int PDFReader::SkipWhiteSpace() {
	int	c;
	do
		c = GetChar();
	while (c == ' ' || c == '\t' || c == '\n' || c == '\r');
	return c;
}

char* PDFReader::ReadLine(char *line, int maxlen) {
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

char *PDFReader::GetName(char *name) {
	char	*d = name;
	for (;;) {
		char	c = file.getc();
		switch (c) {
			case '(': case '<': case '>': case '[': case ']': case '/':
				PutBack(c);
			case ' ': case '\t': case '\r': case '\n':
				*d++ = 0;
				return name;
			case '#': {
				int	h1 = file.getc(), h2 = file.getc();
				if (is_hex(h1) && is_hex(h2))
					c = from_digit(h1) * 16 + from_digit(h2);
			}
		}
		*d++ = c;
	}
}

bool PDFReader::ReadValue(tag name, ISO_ptr<void> &p0) {
	for (;;) {
		bool	neg = false;
		int		c = GetChar();
		switch (c) {
			case 0: case '%': {
				char	line[256];
				ReadLine(line, 255);
				break;
			}
			case ' ': case '\t': case '\r': case '\n': case '\f':
				break;
			case '<':
				c = GetChar();
				if (c == '<') {
					//dictionary
					ISO_ptr<anything>	p(name);
					p0 = p;
					while ((c = SkipWhiteSpace()) == '/') {
						char	name2[256];
						ReadValue(GetName(name2), p->Append());
					}
					if (c != '>' || GetChar() != c)
						throw("syntax");
					return true;
				} else {
					//hex string
					uint8		buffer[1024], *d = buffer;
					int			n = 1;
					for (;; c = GetChar()) {
						if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f') {
							continue;
						} else if (c == '>') {
							break;
						} else {
							if (!is_hex(c))
								throw("syntax");
							n = (n << 4) | from_digit(c);
							if (n >= 0x100) {
								*d++ = n;
								n = 1;
							}
						}
					}
					ISO_ptr<ISO_openarray<uint8> >	p(name);
					p0 = p;
					memcpy(p->Create(d - buffer), buffer, d - buffer);
					return true;
				}

			case '(': {
				int		nest = 1;
				char	buffer[256], *d = buffer;
				for (; nest;) {
					char c = GetChar();
					switch (c) {
						case '(':
							nest++;
							break;
						case ')':
							if (--nest == 0)
								c = 0;
							break;
						case '\\':
							switch (c = GetChar()) {
								case '\n':
									continue;
								case 'n': c = '\n'; break;
								case 'r': c = '\r'; break;
								case 't': c = '\t'; break;
								case 'b': c = '\b'; break;
								case 'f': c = '\f'; break;
								case '(': case ')': case '\\': c = '\r'; break;
								case '0': case '1': case '2': c = '3';
								case '4': case '5': case '6': c = '7'; {
									c -= '0';
									for (;;) {
										char h = GetChar();
										if (h < '0' || h > '7')
											break;
										c = c * 8 + h - '0';
									}
									break;
								}
							}
					}
					*d++ = c;
				}
				p0 = ISO_ptr<string>(name, buffer);
				return true;
			}

			case '[': {
				//array
				ISO_ptr<anything>	p(name);
				p0 = p;
				ISO_ptr<void>	t;
				while (ReadValue(none, t)) {
					p->Append(t);
				}
				c = GetChar();
				if (c != ']')
					throw("Missing ]");
				return true;
			}

			case '-':
				neg	= true;
			case '+':
				c	= GetChar();
			case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9': {
				int	v = 0;
				while (c >= '0' && c <= '9') {
					v = v * 10 + c - '0';
					c = GetChar();
				}
				if (c == '.') {
					float	f = v, p = 1;
					c = GetChar();
					while (c >= '0' && c <= '9') {
						p = p / 10.f;
						f	+= (c - '0') * p;
						c = GetChar();
					}
					PutBack(c);
					p0 = ISO_ptr<float>(name, f);
					return true;
				}
				if (c == ' ' && (c = SkipWhiteSpace()) >= '0' && c <= '9') {
					streamptr	hack = file.tell();
					int	g = 0;
					while (c >= '0' && c <= '9') {
						g = g * 10 + c - '0';
						c = GetChar();
					}
					if (c == ' ' && (c = SkipWhiteSpace()) == 'R') {
						p0 = ISO_ptr<ISO_ptr<void> >(name, Reference(v));
						return true;
					}
					file.seek(hack - 1);
				} else {
					PutBack(c);
				}
				p0 = ISO_ptr<int>(name, v);
				return true;
			}

			case 'f': {
				const char *t = "alse";
				while (char c = *t++) {
					if (c != GetChar())
						return false;
				}
				p0 = ISO_ptr<bool8>(name, false);
				return true;
			}

			case 't': {
				const char *t = "rue";
				while (char c = *t++) {
					if (c != GetChar())
						return false;
				}
				p0 = ISO_ptr<bool8>(name, true);
				return true;
			}

			case '/': {
				char	name2[256];
//				ISO_ptr<void>	p(GetName(name2));
//				p0 = ISO_ptr<ISO_ptr<void> >(name, p);
				p0 = ISO_ptr<string>(name, GetName(name2));
				return true;
			}

			default: {
				PutBack(c);
				return false;
			}
		}
	}
}

struct xref_record {
	char	start[11];	// inc space
	char	count[6];	// inc space
	char	keyword;	// 'n' or 'f'
	char	eol[2];
};

char *get_name(string_scan &ss, char *name) {
	char	*d = name;
	for (;;) {
		char	c = ss.getc();
		switch (c) {
			case '(': case '<': case '>': case '[': case ']': case '/':
				ss.move(-1);
			case ' ': case '\t': case '\r': case '\n':
				*d++ = 0;
				return name;
			case '#': {
				int	h1 = ss.getc(), h2 = ss.getc();
				if (is_hex(h1) && is_hex(h2))
					c = from_digit(h1) * 16 + from_digit(h2);
			}
		}
		*d++ = c;
	}
}
#if 0
bool read_value(string_scan &ss, tag name, ISO_ptr<void> &p0) {
	for (;;) {
		bool	neg = false;
		int		c = ss.getc();
		switch (c) {
			case 0:
			case '%': {
				break;
			}
			case '<':
				c = ss.getc();
				if (c == '<') {
					//dictionary
					ISO_ptr<anything>	p(name);
					p0 = p;
					while ((c = ss.skip_whitespace().getc()) == '/') {
						fixed_string<256>	name2;
						get_name(ss, name2);
						read_value(ss, name2, p->Append());
					}
					if (c != '>' || ss.getc() != '>')
						throw("syntax");
					return true;
				} else {
					//hex string
					uint8		buffer[1024], *d = buffer;
					int			n = 1;
					for (; c != '>'; c = ss.skip_whitespace().getc()) {
						if (!is_hex(c))
							throw("syntax");
						n = (n << 4) | from_digit(c);
						if (n >= 0x100) {
							*d++ = n;
							n = 1;
						}
					}
					ISO_ptr<ISO_openarray<uint8> >	p(name);
					p0 = p;
					memcpy(p->Create(d - buffer), buffer, d - buffer);
					return true;
				}

			case '(': {
				int		nest = 1;
				char	buffer[256], *d = buffer;
				while (nest) {
					char c = ss.getc();
					switch (c) {
						case '(':
							nest++;
							break;
						case ')':
							if (--nest == 0)
								c = 0;
							break;
						case '\\':
							switch (c = ss.getc()) {
								case '\n':
									continue;
								case 'n': c = '\n'; break;
								case 'r': c = '\r'; break;
								case 't': c = '\t'; break;
								case 'b': c = '\b'; break;
								case 'f': c = '\f'; break;
								case '(': case ')': case '\\': c = '\r'; break;
								case '0': case '1': case '2': c = '3';
								case '4': case '5': case '6': c = '7'; {
									c -= '0';
									for (;;) {
										char h = ss.getc();
										if (h < '0' || h > '7')
											break;
										c = c * 8 + h - '0';
									}
									break;
								}
							}
					}
					*d++ = c;
				}
				p0 = ISO_ptr<string>(name, buffer);
				return true;
			}

			case '[': {
				//array
				ISO_ptr<anything>	p(name);
				p0 = p;
				ISO_ptr<void>	t;
				while (read_value(ss, 0, t))
					p->Append(t);
				c = ss.getc();
				if (c != ']')
					throw("Missing ]");
				return true;
			}

			case '-':
				neg	= true;
			case '+':
				c	= ss.getc();
			case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9': {
				uint32	v = ss.get();
				if (ss.peekc() == '.') {
					float	f = ss.get();
					p0 = ISO_ptr<float>(name, f + v);
					return true;
				}
				if (c == ' ' && is_digit(ss.skip_whitespace().peekc())) {
					uint32		g		= ss.get();
					if (c == ' ' && ss.skip_whitespace().peekc() == 'R') {
						p0 = ISO_ptr<ISO_ptr<void> >(name, Reference(v));
						return true;
					}
				}
				p0 = ISO_ptr<int>(name, v);
				return true;
			}

			case 'f': {
				if (ss.check("alse"))
					break;
				p0 = ISO_ptr<bool8>(name, false);
				return true;
			}

			case 't': {
				if (ss.check("rue"))
					break;
				p0 = ISO_ptr<bool8>(name, true);
				return true;
			}

			case '/': {
				fixed_string<256>	name2;
				p0 = ISO_ptr<string>(name, get_name(ss, name2));
				return true;
			}

			default:
				break;
		}
		ss.move(-1);
		return false;
	}
}
#endif

void fix_line_end(istream_ref file) {
	file.seek_cur(-1);
	char	c = file.getc();
	if ((c == '\r' || c == '\n') && file.getc() != '\r' + '\n' - c)
		file.seek_cur(-1);
}

bool PDFReader::GetXRefs(streamptr loc) {
	file.seek(loc);
	auto	text = make_text_reader(file);

	string	line;
	text.read_line(line);

	if (line != "xref")
		return false;

	for (;;) {
		text.read_line(line);
		fix_line_end(file);

		string_scan	ss(line);

		int	start, count;
		if (!ss.get(start) || !ss.get(count))
			break;

		xrefblock &b = xrefs.emplace_back(start, count);
		for (int i = 0; i < count; i++) {
			xref_record	r = file.get();
			if (r.keyword == 'n') {
				get_num_base<10>(r.start, b[i].offset);
				get_num_base<10>(r.count, b[i].gen);
			}
		}
	}

	if (line == "trailer") {
		text.read_line(line);
		string_scan	ss(line);

		ISO_ptr<anything>	p(0);
		uint32				size = 0;

		if (ss.getc() == '<' && ss.getc() == '<') {
			while (ss.scan('/')) {
				ss.move(1);
				fixed_string<256>	name2;
				get_name(ss, name2);

				if (name2 == "Size") {
					ss >> size;

				} else if (name2 == "Prev") {
					uint32	prev = ss.get();
					GetXRefs(prev);

				} else if (name2 == "Root") {
					uint32	v = ss.get();
					if (is_digit(ss.skip_whitespace().peekc())) {
						uint32	g	= ss.get();
						if (ss.skip_whitespace().peekc() == 'R') {
							ss.move(1);
							root = Reference(v);
						}
					}
				}
			}
			//if (c != '>' || ss.getc() != '>')
			//	throw("syntax");
		}
		return p;
	}
	return false;
}

ISO_ptr<void> PDFReader::Reference(int v) {
	for (size_t i = 0, n = xrefs.size(); i < n; i++) {
		int	off = v - xrefs[i].first;
		if (off >= 0 && off < xrefs[i].size()) {
			xrefentry	&e = xrefs[i][off];
			if (!e.ptr) {
				char		line[256], name[64];
				int			obj, gen;
				streamptr	save = file.tell();
				file.seek(e.offset);

				ReadLine(line, 255);
				if (sscanf(line, "%i %i obj", &obj, &gen) == 2) {
					sprintf(name, "obj_%i_%i", obj, gen);
					ReadValue(name, e.ptr);
					ReadLine(line, 255);
					if (strcmp(line, "stream") == 0) {
						file.getc();

						ISO::Browser	b(e.ptr);
						uint32		len		= b["Length"].GetInt();
						const char	*filter	= b["Filter"].GetString();
						const char	*type	= b["Subtype"].GetString();

						if (filter && strcmp(filter, "FlateDecode") == 0) {
							zlib_reader	gz(file);
							if (type && strcmp(type, "Image") == 0) {
								uint32		width		= b["Width"].GetInt();
								uint32		height		= b["Height"].GetInt();
								int			predictor	= b["DecodeParms"]["Predictor"].GetInt();

								ISO_ptr<bitmap>	p(e.ptr.ID());
								p->Create(width, height);
								malloc_block	buffer(width * height * 3);
								char	*srce	= (char*)buffer;
								int		rowb	= (width * 3 + 7) / 8;
								uint8	pred;

								for (int y = 0; y < height; y++) {
									for (int i = 0; i < 8; i++) {
										pred	= gz.getc();
										gz.readbuff(srce, rowb);
										srce += rowb;
									}
								}
								ISO_rgba	*dest = p->ScanLine(0);
								uint8		p0 = 0, p1 = 0, p2 = 0;
								srce	= (char*)buffer;
								for (int n = width * height; n--; dest++, srce+=3) {
									uint32	a = (srce[0] << 24) + (srce[1] << 16) + srce[2];
									uint8	r = 0, g = 0, b = 0;
									for (int i = 0; i < 8; i++) {
										b |= (a & 1) << 7;
										g |= (a & 2) << 6;
										r |= (a & 4) << 5;
										a >>= 3;
										r >>= 1;
										g >>= 1;
										b >>= 1;
									}
									p0 = r;
									p1 = g;
									p2 = b;
									*dest = ISO_rgba(p0,p1,p2);
								}
								e.ptr = p;
								file.seek(save);
								return e.ptr;
							} else {
								ISO_ptr<ISO_openarray<uint8> >	p(e.ptr.ID());
								uint8	buffer[1024];
								uint32	total	= 0;
								while (uint32 len = gz.readbuff(buffer, 1024)) {
									p->Resize(total + len);
									memcpy(*p + total, buffer, len);
									total += len;
								}
								e.ptr = p;
								//gz.restore_unused();
							}
						} else {
							ISO_ptr<ISO_openarray<uint8> >	p(e.ptr.ID());
							file.readbuff(p->Create(len), len);
							e.ptr = p;
						}
						ReadLine(line, 255);
						if (strcmp(line, "endstream") != 0)
							throw("Missing endstream");
						ReadLine(line, 255);
					}
					if (strcmp(line, "endobj") != 0)
						throw("Missing endobj");
				}

				file.seek(save);
			}
			return e.ptr;
		}
	}
	return ISO_NULL;
}

ISO_ptr<void> PDFFileHandler::Read(tag id, istream_ref file) {
	if (!file.get<fixed_string<16> >().begins("%PDF-"))
		return ISO_NULL;

	char		line[2048];
	uint32		xref = 0;

	file.seek_end(-1);
	char	*p = line + num_elements(line);
	for (int i = 0; i < 4; i++) {
		*--p = 0;
		for (char c;;) {
			c = file.getc();
			file.seek_cur(-2);
			if (c == '\r' || c == '\n') {
				file.seek_cur(file.getc() == '\r'+'\n' - c ? -2 : -1);
				break;
			}
			*--p = c;
		}
		bool	ok = true;
		switch (i) {
			case 0:		ok = *p == 0;					break;
			case 1:		ok = str(p) == "%%EOF";			break;
			case 2:		ok = from_string(p, xref) != 0;	break;
			case 3:		ok = str(p) == "startxref";		break;
		}
		if (!ok)
			return ISO_NULL;
	};

	try {
		PDFReader	pdf(file);
		if (pdf.GetXRefs(xref))
			return pdf.root;
	} catch (const char *s) {
		throw_accum(s << " at address 0x" << hex(file.tell()));
	}
	return ISO_NULL;
}
