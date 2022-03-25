#include "iso/iso_files.h"
#include "archive_help.h"

using namespace iso;

//-----------------------------------------------------------------------------
//	Acorn DFS
//-----------------------------------------------------------------------------

class AcornDFS : public FileHandler {
	enum {SECTOR_SIZE = 256};

	struct Name {	// 8 bytes
		char	name[7];
		char	directory;
	};
	struct Info {	// 8 bytes
		uint8	load_lo;		// load address, low order bits
		uint8	load_mid;		// load address, middle order bits
		uint8	exec_lo;		// exec address, low order bits
		uint8	exec_mid;		// exec address, middle order bits
		uint8	length_lo;		// length in bytes, low order bits
		uint8	length_mid;		// length in bytes, middle order bits
		uint8	start_hi	:2,	// start sector, two high order bits of 10 bit number
				load_hi		:2,	// load address, high order bits
				length_hi	:2,	// length in bytes, high order bits
				exec_hi		:2;	// exec address, high order bits
		uint8	start_lo;		// start sector, eight low order bits of 10 bit number

		uint16	start()		const { return start_lo + (start_hi << 8); }
		uint16	load()		const { return load_lo + (load_mid << 8) + (load_hi << 16); }
		uint16	exec()		const { return exec_lo + (exec_mid << 8) + (exec_hi << 16); }
		uint16	length()	const { return length_lo + (length_mid << 8) + (length_hi << 16); }
	};

	struct Sector01 {
		//sector 0 (1024 bytes)
		char	titlea[8];		// first eight bytes of the 13-byte disc title
		Name	name[31];

		//sector 1 (1024 bytes)
		char	titleb[4];		// last four bytes of the disc title
		uint8	sequence;
		uint8	entries;		// number of entries multiplied by 8
		uint8	sectors_hi	:2,	// number of sectors on disc (two high order bits of 10 bit number)
							:2,
				boot		:2;	// (bits 4,5) !BOOT start-up option
		uint8	sectors_lo;		// number of sectors on disc (eight low order bits of 10 bit number)
		Info	info[31];

		fixed_string<13> title()	const { fixed_string<13> t; memcpy(t, titlea, 8); memcpy(t + 8, titleb, 4); t[12] = 0; return t; }
		uint16			sectors()	const { return sectors_lo + (sectors_hi << 8); }
	};

	static bool check_alphanum(const char *p, int n) {
		char	c;
		while (n-- && (c = *p++)) {
			if (int8(c) > 0 && c < ' ')
				return false;
		}
		return true;
	}

	template<int N> static bool check_alphanum(const char (&s)[N]) {
		return check_alphanum(s, N);
	}

	static bool				check(const Sector01 &s, uint32 len);
	static ISO_ptr<void>	read(tag id, const Sector01 &s, istream_ref file, uint32 interleave, uint32 offset);

	const char*		GetDescription() override {
		return "Acorn DFS";
	}

	int				Check(istream_ref file) override {
		file.seek(0);
		streamptr	len = file.length();
		return len > sizeof(Sector01) && check(file.get(), len <= 200 * 1024 ? len : len / 2) ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}

	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		streamptr	len		= file.length();
		bool		dsided	= len > 200 * 1024;
		Sector01	side0	= file.get();

		if (!check(side0, len >> int(dsided)))
			return ISO_NULL;

		if (dsided) {
			file.seek(10 * SECTOR_SIZE);
			Sector01	side1 = file.get();

			if (check(side1, len >> 1)) {
				ISO_ptr<anything>	disk(id, 2);
				(*disk)[0] = read(0, side0, file, 10, 0);
				(*disk)[1] = read(0, side1, file, 10, 10 * SECTOR_SIZE);
				return disk;
			}
		}

		return read(id, side0, file, dsided ? 10 : 0, 0);

	}
} acorndfs;

bool AcornDFS::check(const Sector01 &s, uint32 len) {
	if (s.sectors() * SECTOR_SIZE == len && check_alphanum(s.titlea) && check_alphanum(s.titleb) && (s.entries & 7) == 0) {
		for (int i = 0, n = s.entries / 8; i < n; i++) {
			if (!check_alphanum(s.name[i].name))
				return false;
		}
		return true;
	}
	return false;
}

ISO_ptr<void> AcornDFS::read(tag id, const Sector01 &s, istream_ref file, uint32 interleave, uint32 offset) {
	if (!id)
		id	= s.title();

	int		n	= s.entries / 8;
	ISO_ptr<anything>	p(id, n);

	for (int i = 0; i < n; i++) {
		const Name	&name	= s.name[i];
		const Info	&info	= s.info[i];

		buffer_accum<16>	ba;
		if (name.directory != '$')
			ba << name.directory << '.';

		int			nlen	= 7;
		while (nlen && name.name[nlen - 1] == ' ')
			nlen--;
		ba << str(name.name, nlen);

		ISO_ptr<ISO_openarray<uint8> >	e((const char*)ba);
		uint32	start	= info.start();
		uint32	length	= info.length();
		uint8	*dest	= e->Create(length, false);

		if (interleave) {
			for (uint32 sector = start % interleave, track = start / interleave; length; sector = 0, track++) {
				file.seek((sector + track * interleave * 2) * SECTOR_SIZE + offset);
				uint32	read	= min((interleave - sector) * SECTOR_SIZE, length);
				file.readbuff(dest, read);
				length	-= read;
				dest	+= read;
			}
		} else {
			file.seek(start * SECTOR_SIZE + offset);
			file.readbuff(dest, length);
		}

		(*p)[i] = e;
	}
	return p;
}

//-----------------------------------------------------------------------------
//	BBC BASIC
//-----------------------------------------------------------------------------

enum TOKENS {
	/*			0/8				1/9				2/A				3/B				4/C				5/D				6/E				7/F*/
    /*0x80*/	TOK_AND = 0x80,	TOK_DIV,		TOK_EOR,		TOK_MOD,		TOK_OR,			TOK_ERROR,		TOK_LINE,		TOK_OFF,
	/*0x88*/	TOK_STEP,		TOK_SPC,		TOK_TAB,		TOK_ELSE,		TOK_THEN,		TOK_LINENUM,	TOK_OPENIN2,	TOK_PTR1,
	/*0x90*/	TOK_PAGE1,		TOK_TIME1,		TOK_LOMEM1,		TOK_HIMEM1,		TOK_ABS,		TOK_ACS,		TOK_ADVAL,		TOK_ASC,
	/*0x98*/	TOK_ASN,		TOK_ATN,		TOK_BGET,		TOK_COS,		TOK_COUNT,		TOK_DEG,		TOK_ERL,		TOK_ERR,
	/*0xA0*/	TOK_EVAL,		TOK_EXP,		TOK_EXT,		TOK_FALSE,		TOK_FN,			TOK_GET,		TOK_INKEY,		TOK_INSTR,
	/*0xA8*/	TOK_INT,		TOK_LEN,		TOK_LN,			TOK_LOG,		TOK_NOT,		TOK_OPENIN1,	TOK_OPENOUT,	TOK_PI,
	/*0xB0*/	TOK_POINT,		TOK_POS,		TOK_RAD,		TOK_RND,		TOK_SGN,		TOK_SIN,		TOK_SQR,		TOK_TAN,
	/*0xB8*/	TOK_TO,			TOK_TRUE,		TOK_USR,		TOK_VAL,		TOK_VPOS,		TOK_CHRs,		TOK_GETs,		TOK_INKEYs,
	/*0xC0*/	TOK_LEFTs,		TOK_MIDs,		TOK_RIGHTs,		TOK_STRs,		TOK_STRINGs,	TOK_EOF,		TOK_AUTO,		TOK_DELETE,
	/*0xC8*/	TOK_LOAD,		TOK_LIST,		TOK_NEW,		TOK_OLD,		TOK_RENUMBER,	TOK_SAVE,		TOK_xCE,		TOK_PTR2,
	/*0xD0*/	TOK_PAGE2,		TOK_TIME2,		TOK_LOMEM2,		TOK_HIMEM2,		TOK_SOUND,		TOK_BPUT,		TOK_CALL,		TOK_CHAIN,
	/*0xD8*/	TOK_CLEAR,		TOK_CLOSE,		TOK_CLG,		TOK_CLS,		TOK_DATA,		TOK_DEF,		TOK_DIM,		TOK_DRAW,
	/*0xE0*/	TOK_END,		TOK_ENDPROC,	TOK_ENVELOPE,	TOK_FOR,		TOK_GOSUB,		TOK_GOTO,		TOK_GCOL,		TOK_IF,
	/*0xE8*/	TOK_INPUT,		TOK_LET,		TOK_LOCAL,		TOK_MODE,		TOK_MOVE,		TOK_NEXT,		TOK_ON,			TOK_VDU,
	/*0xF0*/	TOK_PLOT,		TOK_PRINT,		TOK_PROC,		TOK_READ,		TOK_REM,		TOK_REPEAT,		TOK_REPORT,		TOK_RESTORE,
	/*0xF8*/	TOK_RETURN,		TOK_RUN,		TOK_STOP,		TOK_COLOUR,		TOK_TRACE,		TOK_UNTIL,		TOK_WIDTH,		TOK_OSCLI
};

const char *token_text[128] = {
	/*			0/8				1/9				2/A				3/B				4/C				5/D				6/E				7/F*/
	/*0x80*/	"AND",			"DIV",			"EOR",			"MOD",			"OR",			"ERROR",		"LINE",			"OFF",
	/*0x88*/	"STEP",			"SPC",			"TAB(",			"ELSE",			"THEN",			"OPENIN",		"x8E",			"PTR",
	/*0x90*/	"PAGE",			"TIME",			"LOMEM",		"HIMEM",		"ABS",			"ACS",			"ADVAL",		"ASC",
	/*0x98*/	"ASN",			"ATN",			"BGET",			"COS",			"COUNT",		"DEG",			"ERL",			"ERR",
	/*0xA0*/	"EVAL",			"EXP",			"EXT",			"FALSE",		"FN",			"GET",			"INKEY",		"INSTR(",
	/*0xA8*/	"INT",			"LEN",			"LN",			"LOG",			"NOT",			"OPENIN",		"OPENOUT",		"PI",
	/*0xB0*/	"POINT(",		"POS",			"RAD",			"RND",			"SGN",			"SIN",			"SQR",			"TAN",
	/*0xB8*/	"TO",			"TRUE",			"USR",			"VAL",			"VPOS",			"CHR$",			"GET$",			"INKEY$",
	/*0xC0*/	"LEFT$(",		"MID$(",		"RIGHT$(",		"STR$",			"STRING$(",		"EOF",			"AUTO",			"DELETE",
	/*0xC8*/	"LOAD",			"LIST",			"NEW",			"OLD",			"RENUMBER",		"SAVE",			"xCE",			"PTR",
	/*0xD0*/	"PAGE",			"TIME",			"LOMEM",		"HIMEM",		"SOUND",		"BPUT",			"CALL",			"CHAIN",
	/*0xD8*/	"CLEAR",		"CLOSE",		"CLG",			"CLS",			"DATA",			"DEF",			"DIM",			"DRAW",
	/*0xE0*/	"END",			"ENDPROC",		"ENVELOPE",		"FOR",			"GOSUB",		"GOTO",			"GCOL",			"IF",
	/*0xE8*/	"INPUT",		"LET",			"LOCAL",		"MODE",			"MOVE",			"NEXT",			"ON",			"VDU",
	/*0xF0*/	"PLOT",			"PRINT",		"PROC",			"READ",			"REM",			"REPEAT",		"REPORT",		"RESTORE",
	/*0xF8*/	"RETURN",		"RUN",			"STOP",			"COLOUR",		"TRACE",		"UNTIL",		"WIDTH",		"OSCLI"
};
/* token_space: how to pretty-print the token:
 * 0 = no space before or after the token
 * 1 = pre space (not for functions)
 * 2 = post space (not for tokens with implicit bracket)
 * 3 = pre + post space
 */
int token_space[128] = {
	/*          0/8   1/9   2/A   3/B   4/C   5/D   6/E   7/F */
	/* 0x80 */    3,    3,    3,    3,    3,    3,    3,    3,
	/* 0x88 */    3,    3,    1,    3,    3,    3,    2,    3,
	/* 0x90 */    0,    0,    0,    0,    2,    2,    2,    2,
	/* 0x98 */    2,    2,    2,    2,    2,    2,    2,    2,
	/* 0xA0 */    2,    2,    2,    2,    0,    2,    2,    1,
	/* 0xA8 */    2,    2,    2,    2,    2,    2,    2,    2,
	/* 0xB0 */    1,    2,    2,    2,    2,    2,    2,    2,
	/* 0xB8 */    3,    2,    2,    2,    2,    2,    2,    2,
	/* 0xC0 */    1,    1,    1,    2,    1,    3,    3,    3,
	/* 0xC8 */    3,    3,    3,    3,    3,    3,    3,    2,
	/* 0xD0 */    0,    0,    0,    0,    3,    3,    3,    3,
	/* 0xD8 */    3,    3,    3,    3,    3,    3,    3,    3,
	/* 0xE0 */    3,    3,    3,    3,    3,    3,    3,    3,
	/* 0xE8 */    3,    3,    3,    3,    3,    3,    3,    3,
	/* 0xF0 */    3,    3,    1,    3,    3,    3,    3,    3,
	/* 0xF8 */    3,    3,    3,    3,    3,    3,    3,    3,
};

uint8 *PrintBBCBASIC(string_accum &&ac, uint8 *data, uint32 len, int basic_version, bool use_pretty_print) {
	bool	assembler 		  	= false;
	bool	assembler_comment 	= false;
	bool	basic_comment 	  	= false;
	bool	quote_on			= false;
	bool	token_post_space  	= false;

	int		line_bytes			= 0;
	int		line_length			= 0;
	int		indent_level 	  	= 0;
	int		last_action_byte	= -1;

	for (uint8 *p = data, *end = data + len; p < end; ++p) {
		uint8	c = *p;
		line_bytes++;

		if (quote_on) {
			ac << char(c);
			if (c == '"')
				quote_on = !quote_on;
			continue;
		}

		if (last_action_byte == -1 && c != 13)
			return 0;

		switch (c) {
			case 13:
				if (last_action_byte != -1)
					ac << '\n';

				if (p[1] >= 128)
					return p + 2;

				if (p + 2 >= end)
					return 0;

				ac << (p[2] | (p[1] << 8));

				line_bytes	= 0;
				line_length	= p[3];
				p += 3;

				if (use_pretty_print)
					ac.putc(' ', indent_level + 1);

				assembler_comment	= false;
				basic_comment		= false;
				quote_on			= false;
				last_action_byte	= c;
				continue;

			case '"':
				if (!assembler_comment && !basic_comment)
					quote_on	= true;
				break;

			case ' ':
				if (use_pretty_print && !assembler && !basic_comment && last_action_byte >= 0x80)
					continue;
				break;

			case 'P':
				if (last_action_byte == TOK_TO)	//TO P
					last_action_byte = c;
				break;

			case ']':
				assembler = false;
				break;

			case '[':
				if (last_action_byte == 13 || last_action_byte == ':')
					assembler = true;
				break;

			case '\\':
				if (assembler)
					assembler_comment = true;
				break;

			case ':':
				last_action_byte = c;
				assembler_comment = false;
				break;

			case ',':
				if (last_action_byte == TOK_NEXT)
					indent_level--;
				break;

			case TOK_FN:
				if (last_action_byte == TOK_DEF)
					indent_level--;
				break;

			case TOK_DEF:
				indent_level++;
				break;

			case TOK_FOR:
			case TOK_REPEAT:
				indent_level++;
				break;

			case TOK_ENDPROC:// setting to 0 takes care of multiple DEF PROC entry points with single ENDPROC */
				indent_level = 0;
				break;

			case TOK_NEXT:
			case TOK_UNTIL:
				indent_level--;
				break;

			case TOK_REM:
				basic_comment = true;
				break;

			case TOK_LINENUM: { // Inline line number
				int line	= (p[3] - 0x40) * 256 + (p[2] - 0x40);
				switch (p[1]) {
					case 0x44: line += 0x40; break;
					case 0x54: line += 0x00; break;
					case 0x64: line += 0xC0; break;
					case 0x74: line += 0x80; break;
					case 0x40: line += 0x4040; break;
					case 0x50: line += 0x4000; break;
					case 0x60: line += 0x40C0; break;
					case 0x70: line += 0x4080; break;
					default://fprintf(stderr, "Bad inline line number at byte #%d (line %d), starting with byte: 0x%02X\n", ptr-2, line_no, n1);
						break;
				}
				ac << line;
				continue;
			}
			default:
				break;
		}
		if (c < 0x80) {
			if (use_pretty_print && (last_action_byte >= 0x80) && token_post_space) {
				if (c >= 0x80 || (c != ':' && c != 13 && c != '(') || (c == '(' && token_space[last_action_byte & 127] == 3))
					ac << ' ';
			}
			ac << char(c);
			if (last_action_byte == TOK_NEXT)
				c = last_action_byte;

		} else {
			const char *token_str = token_text[c - 128];
			if (basic_version == 2) {
				if (c == TOK_OPENIN1)
					token_str = "OPENUP";
			} else {
				//if (token == TOK_OPENIN2)
			}
			int		len = int(strlen(token_str));
			bool	implicit_bracket = token_str[len - 1] == '(';
			ac << ' ' << str(token_str, len - int(implicit_bracket));

			if (!assembler || (last_action_byte != 13 && last_action_byte != ':')) {
				if (use_pretty_print && (token_space[c & 127] & 2))
					token_post_space = true;
			}

			if (implicit_bracket)
				ac << '(';

		}
		last_action_byte = c;
	}
	return 0;
}

class BBC_basic : public FileHandler {
	struct SOL {
		char	eol;	// must be 13
		uint8	line_hi, line_lo;
		uint8	line_len;
	};

	const char*		GetDescription() override { return "BBC BASIC"; }
	int				Check(istream_ref file) override {
		file.seek(0);
		SOL	s	= file.get();
		if (s.eol != 13)
			return CHECK_DEFINITE_NO;
		for (int i = 0; i < s.line_len - sizeof(s); i++) {
			if (file.getc() < ' ')
				return CHECK_DEFINITE_NO;
		}
		s = file.get();
		return s.eol == 13 ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}
	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		streamptr	len = file.length();
		if (len < 65536) {
			malloc_block	in(file, len);
			malloc_block	out(65536);
			if (uint8 *p = PrintBBCBASIC(fixed_accum(out, out.length()), in, len, 1, true)) {
				if (p == in.end())
					return ISO_ptr<string>(id, (const char*)out);
				uint32					extra	= (uint8*)in.end() - p;
				ISO_openarray<xint8>	bin;
				memcpy(bin.Create(extra), p, extra);
				return ISO_ptr<pair<string, ISO_openarray<xint8> > >(id, make_pair((const char*)out, move(bin)));
			}
		}
		return ISO_NULL;
	}
} bbc_basic;