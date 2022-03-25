#include "iso/iso_files.h"
#include "extra/xml.h"
#include "base/algorithm.h"
#include "base/list.h"
#include "base/bits.h"

using namespace iso;

namespace crossword {
	typedef pair<char16,uint16>					cell;
	typedef ISO_openarray<cell>					row;
	typedef ISO_openarray<row>					grid;

	typedef	pair<int, string>					clue;
	typedef pair<string, ISO_openarray<clue> >	clue_set;
	typedef ISO_openarray<clue_set>				clues;

	struct crossword {
		grid		grid;
		clues		clues;
		anything	meta;
	};
}

ISO_DEFUSERCOMPXV(crossword::crossword, "crossword", grid, clues, meta);

//-----------------------------------------------------------------------------
//	JPZ
//-----------------------------------------------------------------------------

class JPZFileHandler : public FileHandler {

	struct puzzle {
		int		width, height;
		struct cell {
			char16	val;	//0 is block
			uint16	num;
			cell() : val(0), num(0) {}
		} *grid;

		typedef	pair<int, string>					clue;
		typedef pair<string, dynamic_array<clue> >	clue_set;
		dynamic_array<clue_set>						clues;

		puzzle(int _width, int _height) : width(_width), height(_height) {
			grid = new cell[_width * _height];
		}
		~puzzle() {
			delete[] grid;
		}
		cell*	get_cell(int x, int y)				{ return grid + (y - 1) * width + x - 1;	}
		void	add_clues(const count_string &s)	{ clues.push_back().a			= s;	}
		void	add_clue(int clue_no)				{ clues.back().b.push_back().a	= clue_no;	}
		void	set_clue(const char *clue)			{ clues.back().b.back().b		= clue;		}
		void	set_clue(const count_string &clue)	{ clues.back().b.back().b		= clue;		}
	};

	const char*		GetExt() override { return "jpz"; }
	ISO_ptr<void>	Read(tag id, istream_ref file) override;
public:
	JPZFileHandler()	{ ISO::getdef<crossword::crossword>(); }
} jpz;

ISO_ptr<void> JPZFileHandler::Read(tag id, istream_ref file) {
	XMLreader::Data	data;
	XMLreader				xml(file);
	puzzle					*cw = 0;
	string					title;
	string					creator;
	string					copyright;
	string					description;

#if 1
	for (XMLiterator it(xml, data); it.Next();) {
		if (data.Is("crossword-compiler-applet") || data.Is("crossword-compiler")) {
			for (it.Enter(); it.Next();) {
				if (data.Is("rectangular-puzzle")) {
					for (it.Enter(); it.Next();) {
						if (data.Is("metadata")) {
							for (it.Enter(); it.Next();) {
								if (data.Is("title"))
									title = it.Content();
								else if (data.Is("creator"))
									creator = it.Content();
								else if (data.Is("copyright"))
									copyright = it.Content();
								else if (data.Is("description"))
									description = it.Content();
							}
						} else if (data.Is("crossword")) {
							for (it.Enter(); it.Next();) {
								if (data.Is("grid")) {
									cw	= new puzzle(from_string<int>(data.Find("width")), from_string<int>(data.Find("height")));
									for (it.Enter(); it.Next();) {
										if (data.Is("cell")) {
											int	x, y;
											from_string(data.Find("x"), x);
											from_string(data.Find("y"), y);
											from_string(data.Find("number"), cw->get_cell(x, y)->num);
											if (const char *p = data.Find("solution"))
												cw->get_cell(x, y)->val = *p;
										}
									}
								} else if (data.Is("clues")) {
									for (it.Enter(); it.Next();) {
										if (data.Is("title")) {
											int	depth	= 0;
											XMLreader::TagType	tag;
											do {
												switch (tag = xml.ReadNext(data)) {
													case XMLreader::TAG_BEGIN:	++depth; break;
													case XMLreader::TAG_END:	--depth; break;
													case XMLreader::TAG_CONTENT: cw->add_clues(data.Content()); break;
												}
											} while (depth);
										} else if (data.Is("clue")) {
											cw->add_clue(from_string<int>(data.Find("number")));
											cw->set_clue(it.Content());
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}

#else
	fixed_string<1024>		path;
	while (XMLreader::TagType tag = xml.ReadNext(data)) {
		switch (tag) {
			case XMLreader::TAG_BEGIN: case XMLreader::TAG_BEGINEND:
				path << '/' << data.name;
				if (path == "/crossword-compiler-applet/rectangular-puzzle/crossword/grid") {
					int	w, h;
					from_string(data.Find("width"), w);
					from_string(data.Find("height"), h);
					cw	= new puzzle(w, h);
				} else if (path == "/crossword-compiler-applet/rectangular-puzzle/crossword/clues/clue") {
					int	n;
					from_string(data.Find("number"), n);
					cw->add_clue(n);
				} else if (path == "/crossword-compiler-applet/rectangular-puzzle/crossword/grid/cell") {
					int	x, y;
					from_string(data.Find("x"), x);
					from_string(data.Find("y"), y);
					from_string(data.Find("number"), cw->get_cell(x, y)->num);
					if (const char *p = data.Find("solution"))
						cw->get_cell(x, y)->val = *p;
				}
				if (tag == XMLreader::TAG_BEGIN)
					break;
			case XMLreader::TAG_END:
				*path.rfind('/') = 0;
				break;

			case XMLreader::TAG_CONTENT:
				if (path.begins("/crossword-compiler-applet/rectangular-puzzle/crossword/clues/title"))
					cw->add_clues(data.name);
				else if (path.begins("/crossword-compiler-applet/rectangular-puzzle/crossword/clues/clue"))
					cw->set_clue(data.name);
				else if (path.begins("/crossword-compiler-applet/rectangular-puzzle/metadata/title"))
					title = data.name;
				else if (path.begins("/crossword-compiler-applet/rectangular-puzzle/metadata/creator"))
					creator = data.name;
				else if (path.begins("/crossword-compiler-applet/rectangular-puzzle/metadata/copyright"))
					copyright = data.name;
				else if (path.begins("/crossword-compiler-applet/rectangular-puzzle/metadata/description"))
					description = data.name;
				break;
		}
	}
#endif

	if (!cw)
		return ISO_NULL;

	ISO_ptr<::crossword::crossword>	p(id);
	p->grid.Create(cw->height);

	puzzle::cell	*c	= cw->grid;
	for (crossword::grid::iterator i = p->grid.begin(), ie = p->grid.end(); i != ie; ++i) {
		crossword::row	&r = i->Create(cw->width);
		for (crossword::row::iterator j = r.begin(), je = r.end(); j != je; ++j) {
			j->a = c->val;
			j->b = c->num;
			++c;
		}
	}

	p->clues.Create(uint32(cw->clues.size()));
	int	j = 0;
	for (crossword::clues::iterator i = p->clues.begin(), ie = p->clues.end(); i != ie; ++i, ++j) {
		puzzle::clue_set	&set = cw->clues[j];
		i->a = set.a;
		copy(set.b, i->b.Create(uint32(set.b.size())));
	}

	p->meta.Append(ISO_ptr<string>("title",		title));
	p->meta.Append(ISO_ptr<string>("author",	creator));
	p->meta.Append(ISO_ptr<string>("copyright",	copyright));
	p->meta.Append(ISO_ptr<string>("notes",		description));

	return p;
}

//-----------------------------------------------------------------------------
//	PUZ
//-----------------------------------------------------------------------------

struct AcrossLite {

	static uint16 checksum(const void *p, size_t n, uint16 c) {
		for (const uint8 *b = (const uint8*)p; n--; ++b)
			c = rotate_right(c, 1) + *b;
		return c;
	}
	static uint16 checksum(const string &s, uint16 c) {
		return checksum(s, s.length(), c);
	}
	static uint16 checksum_z(const string &s, uint16 c) {
		if (size_t len = s.length())
			return checksum(s, len + 1, c);
		return c;
	}

	static string fix_text(const char *srce);

	struct header {
		enum {
			FLAG_UNKNOWN	= 1,
			FLAG_SCRAMBLED	= 0x40000,
		};

		uint16le	cksum_puz;
		char		magic1[12];	// "ACROSS&DOWN"
		uint16le	cksum_cib;
		uint8		magic2[8];
		char		ver[4];		// "1.2"

		uint8		unknown1[2];
		uint16le	scrambled_cksum;
		uint8		unknown2[12];

		uint8		width;
		uint8		height;
		uint16le	clue_count;
		uint32le	flags;		// =1 a bitmask of some sort

		int			size()			const	{ return width * height; }
	};

	struct clue {
		string	s;
		int		n, x, y;
		bool	down;

		void	set(int _n, int _x, int _y, bool _down) {
			n		= _n;
			x		= _x;
			y		= _y;
			down	= _down;
		}
	};

	header*	h		= nullptr;
	char*	solution= nullptr;
	char*	grid	= nullptr;
	char*	rebus	= nullptr;
	char*	meta	= nullptr;

	string	title;
	string	author;
	string	copyright;
	clue*	clues	= nullptr;
	string	notes;

	char*	ltim	= nullptr;
	char*	gext	= nullptr;
	string*	rusr	= nullptr;

	int		rebus_count;

	uint16	cksum_rebus, cksum_rtbl, cksum_ltim, cksum_gext, cksum_rusr;

	~AcrossLite() {
		iso::free(solution);
		iso::free(grid);
		iso::free(rebus);
		iso::free(ltim);
		iso::free(gext);

		delete[] clues;
		delete[] rusr;
		iso::free(h);
	}

	uint16	ChecksumPartial(uint16 c);
	void	LoadBinary(istream_ref file);
	void	WriteBinary(ostream_ref file);
	bool	LoadText(text_reader<istream_ref> text);
	void	Init(int w, int h);
	void	InitGrid();
	void	FixClues();
	int		GetClueIndex(int i, bool down) const;

	int		CountClues()	const;
	int		Width()			const	{ return h->width; }
	int		Height()		const	{ return h->height; }
	int		NumClues()		const	{ return h->clue_count; }
};

uint16 AcrossLite::ChecksumPartial(uint16 c) {
	c = checksum_z(title, c);
	c = checksum_z(author, c);
	c = checksum_z(copyright, c);
	for (int i = 0; i < h->clue_count; i++)
		c = checksum(clues[i].s, c);
	return checksum_z(notes, c);
}

void AcrossLite::Init(int width, int height) {
	h			= (header*)iso::malloc(sizeof(header));
	clear(*h);
	strcpy(h->magic1, "ACROSS&DOWN");
	strcpy(h->ver, "1.3");
	h->flags	= header::FLAG_UNKNOWN;
	h->width	= width;
	h->height	= height;
	h->clue_count = 0;
	int	s		= h->size();

	grid		= (char*)iso::malloc(s);
	solution	= (char*)iso::malloc(s);

	memset(solution, '.', s);
}

void AcrossLite::FixClues() {
	int		n		= 1;
	int		i		= 0;
	int		width	= h->width;
	int		height	= h->height;
	char	*p		= grid;

	for (int x = 0; x < height; x++) {
		for (int y = 0; y < width; y++, p++) {
			if (*p != '.') {
				bool	ticked = false;
				if ((y == 0 || p[-1] == '.') && (y + 1 < width && p[1] != '.')) {
					clues[i++].set(n, y, x, false);
					ticked = true;
				}

				if ((x == 0 || p[-width] == '.') && (x + 1 < height && p[width] != '.')) {
					clues[i++].set(n, y, x, true);
					ticked = true;
				}

				if (ticked)
					n++;
			}
		}
    }
}

int AcrossLite::CountClues() const {
	int		i		= 0;
	int		width	= h->width;
	int		height	= h->height;
	char	*p		= grid;

	for (int x = 0; x < height; x++) {
		for (int y = 0; y < width; y++, p++) {
			if (*p != '.') {
				if ((y == 0 || p[-1] == '.') && (y + 1 < width && p[1] != '.'))
					i++;
				if ((x == 0 || p[-width] == '.') && (x + 1 < height && p[width] != '.'))
					i++;
			}
		}
    }

	return i;
}

int AcrossLite::GetClueIndex(int i, bool down) const {
	for (int j = 0, n = h->clue_count; j < n; j++) {
		if (clues[j].down == down && clues[j].n == i)
			return j;
	}
	return -1;
}

void AcrossLite::InitGrid() {
	for (int s = h->size(); s--; )
		grid[s] = solution[s] == '.' || solution[s] == ':' ? solution[s] : '-';

	h->clue_count = CountClues();
	clues = new clue[h->clue_count];
	FixClues();
}

void AcrossLite::LoadBinary(istream_ref file) {
	h			= new header;
	file.read(*h);

	int		s	= h->size();

	file.readbuff(solution	= (char*)iso::malloc(s), s);
	file.readbuff(grid		= (char*)iso::malloc(s), s);

	file.read(title);
	file.read(author);
	file.read(copyright);

	clues		= new clue[h->clue_count];
	for (int i = 0; i < h->clue_count; i++)
		file.read(clues[i].s);

	file.read(notes);

	while (!file.eof()) {
		uint32	tag = file.get<uint32be>();
		uint16	len	= file.get<uint16le>();

		switch (tag) {
			case 'GRBS': {
				cksum_rebus = file.get<uint16le>();
				file.readbuff(rebus = (char*)iso::malloc(s), s);
				file.getc();
				bool blank	= true;
				for (int i = s; i && (blank = !rebus[--i]); );
				if (blank) {
					iso::free(rebus);
					rebus = 0;
				}
				break;
			}
			case 'RTBL': {
				cksum_rtbl	= file.get<uint16le>();
				malloc_block	mb(file, len);
				int		n	= 0;
				for (char *s = (char*)mb; len--; s++) {
					if (*s == ';')
						n++;
				}
				rebus_count	= n;
				break;
			}
			case 'LTIM':
				cksum_ltim	= file.get<uint16le>();
				file.readbuff(ltim = (char*)iso::malloc(len), len);
				file.getc();
				break;
			case 'GEXT':
				cksum_gext	= file.get<uint16le>();
				file.readbuff(gext	= (char*)iso::malloc(len), len);
				file.getc();
				break;
			case 'RUSR':
				cksum_rusr	= file.get<uint16le>();
				rusr		= new string[s];
				for (int j = 0; j < s; j++)
					file.read(rusr[j]);
				break;
		}
	}

	FixClues();
}

void AcrossLite::WriteBinary(ostream_ref file) {
	uint16	checksums[4];

	int		s		= h->size();
	uint16	c		= checksum(&h->width, 8, 0);
	h->cksum_cib	= c;
	h->cksum_puz	= ChecksumPartial(checksum(grid, s, checksum(solution, s, c)));

	checksums[0]	= c;
	checksums[1]	= checksum(solution, s, 0);
	checksums[2]	= checksum(grid, s, 0);
	checksums[3]	= ChecksumPartial(0);

	uint8	cheated[] = "ICHEATED";
	for (int i = 0; i < 4; i++) {
		h->magic2[i + 0] = cheated[i + 0] ^ checksums[i];
		h->magic2[i + 4] = cheated[i + 4] ^ (checksums[i] >> 8);
	}

	file.write(*h);

	file.writebuff(solution, s);
	file.writebuff(grid, s);

	file.write(title);
	file.write(author);
	file.write(copyright);

	for (int i = 0; i < h->clue_count; i++)
		file.write(clues[i].s);

	file.write(notes);
}

bool AcrossLite::LoadText(text_reader<istream_ref> text) {
	string	line;

	if (!text.read_line(line))
		return false;

	int		ver = 0, flags = 0;
	if (line == "<ACROSS PUZZLE>")
		ver = 1;
	else if (line == "<ACROSS PUZZLE V2>")
		ver = 2;
	else
		return false;

	h	= (header*)iso::malloc(sizeof(header));
	list<string>	across, down;

	while (text.read_line(line)) {
		bool repeat = false;
		do {
			if (line == "<TITLE>") {
				text.read_line(title);

			} else if (line == "<AUTHOR>") {
				text.read_line(author);

			} else if (line == "<COPYRIGHT>") {
				text.read_line(copyright);

			} else if (line == "<SIZE>") {
				text.read_line(line);
				string_scan(line) >> h->width >> "x" >> h->height;

			} else if (line == "<GRID>") {
				int	s = h->size();
				grid		= (char*)iso::malloc(s);
				solution	= (char*)iso::malloc(s);
				for (int y = 0; y < h->height; y++) {
					char	*s = solution	+ y * h->width;
					text.read_line(line);
					memcpy(s, line, h->width);
				}
				InitGrid();

			} else if (line == "<REBUS>") {
				text.read_line(line);
				for (const char *s = line, *e; e = str(s).find(';'); s = e + 1) {
					if (str(s, e) == "MARK")
						flags |= 1;
				}
				while (text.read_line(line)) {
					if (repeat = (line[0] == '<'))
						break;
				}

			} else if (line == "<ACROSS>") {
				while (text.read_line(line)) {
					if (repeat = (line == "<DOWN>"))
						break;
					across.push_back(line);
				}

			} else if (line == "<DOWN>") {
				repeat	= true;
				while (text.read_line(line)) {
					if (repeat = (line == "<NOTEPAD>"))
						break;
					down.push_back(line);
				}

			} else if (line == "<NOTEPAD>") {
				while (text.read_line(line)) {
					notes += "\n";
					notes += line;
				}
			}
		} while (repeat);
	}
	return true;
}

string AcrossLite::fix_text(const char *srce) {
	string	s(chars_count<char32>(srce));
	char	*p	= s.begin();
	while (*srce) {
		char32	c;
		srce += get_char(c, srce);
		if (c >= 0x100 || (c >= 0x80 && c < 0xa0)) switch (c) {
			case 0x20AC: c = 0x80; break;	//euro sign Currency Symbols
			case 0x201A: c = 0x82; break;	//single low-9 quotation mark General Punctuation
			case 0x0192: c = 0x83; break;	//Latin small letter f with hook Latin Extended-B
			case 0x201E: c = 0x84; break;	//double low-9 quotation mark General Punctuation
			case 0x2026: c = 0x85; break;	//horizontal ellipsis General Punctuation
			case 0x2020: c = 0x86; break;	//dagger General Punctuation
			case 0x2021: c = 0x87; break;	//double dagger General Punctuation
			case 0x02C6: c = 0x88; break;	//modifier letter circumflex accent Spacing Modifier Letters
			case 0x2030: c = 0x89; break;	//per mille sign General Punctuation
			case 0x0160: c = 0x8A; break;	//Latin capital letter S with caron Latin Extended-A
			case 0x2039: c = 0x8B; break;	//single left-pointing angle quotation mark General Punctuation
			case 0x0152: c = 0x8C; break;	//Latin capital ligature OE Latin Extended-A
			case 0x017D: c = 0x8E; break;	//Latin capital letter Z with caron Latin Extended-A
			case 0x2018: c = /*0x91*/'\''; break;	//left single quotation mark General Punctuation 	// or '
			case 0x2019: c = /*0x92*/'\''; break;	//right single quotation mark General Punctuation 	// or '
			case 0x201C: c = /*0x93*/'"'; break;	//left double quotation mark General Punctuation 	// or "
			case 0x201D: c = /*0x94*/'"'; break;	//right double quotation mark General Punctuation 	// or "
			case 0x2022: c = 0x95; break;	//bullet General Punctuation
			case 0x2013: c = 0x96; break;	//en dash General Punctuation
			case 0x2014: c = 0x97; break;	//em dash General Punctuation
			case 0x02DC: c = 0x98; break;	//small tilde Spacing Modifier Letters
			case 0x2122: c = 0x99; break;	//trade mark sign Letterlike Symbols
			case 0x0161: c = 0x9A; break;	//Latin small letter s with caron Latin Extended-A
			case 0x203A: c = 0x9B; break;	//single right-pointing angle quotation mark General Punctuation
			case 0x0153: c = 0x9C; break;	//Latin small ligature oe Latin Extended-A
			case 0x017E: c = 0x9E; break;	//Latin small letter z with caron Latin Extended-A
			case 0x0178: c = 0x9F; break;	//Latin capital letter Y with diaeresis Latin Extended-A
			default:	c = '?'; break;
		}
		*p++ = c;
	}
	return s;
}

class PUZFileHandler : public FileHandler {
	const char*		GetExt() override { return "puz"; }
	ISO_ptr<void>	Read(tag id, istream_ref file) override;
	bool			Write(ISO_ptr<void> p, ostream_ref file) override;
} puz;

ISO_ptr<void> PUZFileHandler::Read(tag id, istream_ref file) {
	AcrossLite	ac;
	ac.LoadBinary(file);

	ISO_ptr<::crossword::crossword>	p(id);
	p->grid.Create(ac.Height());

	char	*c	= ac.solution;
	for (crossword::grid::iterator i = p->grid.begin(), ie = p->grid.end(); i != ie; ++i) {
		crossword::row	&r = i->Create(ac.Width());
		for (crossword::row::iterator j = r.begin(), je = r.end(); j != je; ++j) {
			j->a = *c == '.' ? 0 : * c;
			j->b = 0;
			++c;
		}
	}

	int		nc[2];
	for (int i = 0, n = ac.NumClues(); i < n; i++)
		++nc[ac.clues[i].down];

	crossword::clue_set	*sets = p->clues.Create(2);
	sets[0].a	= "ACROSS";
	sets[1].a	= "DOWN";

	for (int i = 0, n = ac.NumClues(); i < n; i++) {
		AcrossLite::clue	&c1 = ac.clues[i];
		crossword::clue		&c2 = sets[c1.down].b.Append();
		c2.a	= c1.n;
		c2.b	= c1.s;

		p->grid[c1.y][c1.x].b = c1.n;
	}

	p->meta.Append(ISO_ptr<string>("title",		ac.title));
	p->meta.Append(ISO_ptr<string>("author",	ac.author));
	p->meta.Append(ISO_ptr<string>("copyright",	ac.copyright));
	p->meta.Append(ISO_ptr<string>("notes",		ac.notes));
	return p;
}

bool PUZFileHandler::Write(ISO_ptr<void> p, ostream_ref file) {
	if (!p.IsType<::crossword::crossword>())
		return false;

	crossword::crossword	*c	= p;
	int			w = c->grid[0].Count(), h = c->grid.Count();

	AcrossLite	ac;
	ac.Init(w, h);

	char	*g	= ac.solution;
	for (crossword::grid::iterator i = c->grid.begin(), ie = c->grid.end(); i != ie; ++i) {
		crossword::row	&r = *i;
		for (crossword::row::iterator j = r.begin(), je = r.end(); j != je; ++j)
			*g++ = j->a ? j->a : '.';
	}

	ac.InitGrid();

	for (crossword::clues::iterator i = c->clues.begin(), ie = c->clues.end(); i != ie; ++i) {
		bool	down = i->a == istr("DOWN");
		for (crossword::clue *j = i->b.begin(), *je = i->b.end(); j != je; ++j) {
			int	n = ac.GetClueIndex(j->a, down);
			ISO_ASSERT(n >= 0);
			ac.clues[n].s = AcrossLite::fix_text(j->b);
		}
	}

	ac.title		= (const char*)c->meta["title"];
	ac.author		= (const char*)c->meta["author"];
	ac.copyright	= (const char*)c->meta["copyright"];
	ac.notes		= (const char*)c->meta["notes"];

	ac.WriteBinary(file);
	return true;
}
