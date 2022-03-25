#include "iso/iso_files.h"
#include "base/algorithm.h"
#include "extra/ast.h"
#include "jobs.h"
#include "com.h"
#include <OleAuto.h>

//#include <OCIdl.h>
//#import "C:\Program Files (x86)\Reference Assemblies\Microsoft\MSBuild\v14.0\Microsoft.Build.dll"
//#import "C:\Program Files (x86)\Reference Assemblies\Microsoft\MSBuild\v14.0\Microsoft.Build.tlb" raw_interfaces_only
//#import "C:\Program Files (x86)\Reference Assemblies\Microsoft\MSBuild\v14.0\Microsoft.Build.Engine.tlb"
//#import "C:\Program Files (x86)\Reference Assemblies\Microsoft\MSBuild\v14.0\Microsoft.Build.Framework.tlb" raw_interfaces_only
//#import "C:\Program Files (x86)\Reference Assemblies\Microsoft\MSBuild\v14.0\Microsoft.Build.Conversion.Core.tlb"
//#import "C:\Program Files (x86)\Reference Assemblies\Microsoft\MSBuild\v14.0\Microsoft.Build.Tasks.Core.dll"
//#import "C:\Program Files (x86)\Reference Assemblies\Microsoft\MSBuild\v14.0\Microsoft.Build.Utilities.Core.tlb"

//#include <OCIdl.h>
//#include "dteinternal.h"

namespace iso {
struct packed_string_len {
	static const int SHIFT = (sizeof(intptr_t) - 1) << 3;
	static const intptr_t	MASK = (intptr_t(1) << SHIFT) - 1;
	intptr_t	v;

	packed_string_len() {}
	packed_string_len(const char *s, size_t len) : v((intptr_t(s) & MASK) | (len << SHIFT)) {}
	const char*			begin()	const { return (const char*)(v & MASK); }
	constexpr size_t	len()	const { return v >> SHIFT; }
};

template<> struct string_traits<packed_string_len> {
	typedef const char element;
	typedef element	*iterator;
	typedef _none	start_type;
	static constexpr start_type		start(const packed_string_len &s)		{ return none; }
	static iterator					begin(const packed_string_len &s)		{ return s.begin();	}
	static iterator					end(const packed_string_len &s)			{ return s.begin() + s.len();	}
	static constexpr _none			terminator(const packed_string_len &s)	{ return none; }
	static constexpr size_t			len(const packed_string_len &s)			{ return s.len(); }
};
template<> struct string_traits<const packed_string_len> : string_traits<packed_string_len> {};

struct compact_string : string_base<packed_string_len> {
	compact_string() {}
	template<typename S> compact_string(const string_base<S> &s) : string_base<packed_string_len>(packed_string_len(s.begin(), s.length())) {}
};

}

using namespace iso;

class MakefileHandler : public FileHandler {
	const char*		GetExt() override { return "mak"; }
	const char*		GetDescription() override { return "makefile";	}
	bool			Write(ISO_ptr<void> p, ostream_ref file) override;
} makefile;

struct Parser : string_scan {
	enum TOK {
		TOK_EOF					= -1,

		TOK_SEMICOLON			= ';',
		TOK_MINUS				= '-',
		TOK_PLUS				= '+',
		TOK_DOT					= '.',
		TOK_NOT					= '!',
		TOK_EQUALS				= '=',
		TOK_LESS				= '<',
		TOK_GREATER				= '>',
		TOK_COMMA				= ',',
		TOK_ASTERIX				= '*',
		TOK_DIVIDE				= '/',
		TOK_OPEN_BRACE			= '{',
		TOK_CLOSE_BRACE			= '}',
		TOK_OPEN_BRACKET		= '[',
		TOK_CLOSE_BRACKET		= ']',
		TOK_OPEN_PARENTHESIS	= '(',
		TOK_CLOSE_PARENTHESIS	= ')',

		TOK_IDENTIFIER			= 256,
		TOK_NUMBER,
		TOK_STRINGLITERAL,
		TOK_COMMENT,
		TOK_LINECOMMENT,

		TOK_ELLIPSIS,
		TOK_EQUIV,
		TOK_NEQ,
		TOK_LE,
		TOK_GE,
		TOK_SHL,
		TOK_SHR,
		TOK_OR,
		TOK_BITOR,
		TOK_AND,
		TOK_BITAND,
	};

	TOK				token;
	compact_string	value;

	TOK GetToken() {
		switch (skip_whitespace().getc()) {
			case '_':
			case 'a': case 'b': case 'c': case 'd': case 'e': case 'f': case 'g': case 'h':
			case 'i': case 'j': case 'k': case 'l': case 'm': case 'n': case 'o': case 'p':
			case 'q': case 'r': case 's': case 't': case 'u': case 'v': case 'w': case 'x':
			case 'y': case 'z':
			case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G': case 'H':
			case 'I': case 'J': case 'K': case 'L': case 'M': case 'N': case 'O': case 'P':
			case 'Q': case 'R': case 'S': case 'T': case 'U': case 'V': case 'W': case 'X':
			case 'Y': case 'Z': {
				value = move(-1).get_token(char_set::identifier);
				return TOK_IDENTIFIER;
			}

			case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
				value = move(-1).get_token(char_set::digit);
				return TOK_NUMBER;

			case '\'':
				value = get_token(~char_set('\''));
				move(1);
				return TOK_STRINGLITERAL;

			case '.':
				if (peekc() == '.') {
					move(1);
					return TOK_ELLIPSIS;
				}
				return TOK_DOT;

			case '=':
				if (peekc() == '=') {
					move(1);
					return TOK_EQUIV;
				}
				return TOK_EQUALS;

			case '<':
				if (peekc() == '=') {
					move(1);
					return TOK_LE;
				} else if (peekc() == '<') {
					move(1);
					return TOK_SHL;
				}
				return TOK_LESS;

			case '>':
				if (peekc() == '=') {
					move(1);
					return TOK_GE;
				} else if (peekc() == '>') {
					move(1);
					return TOK_SHR;
				}
				return TOK_GREATER;

			case '!':
				if (peekc(1) == '=') {
					move(2);
					return TOK_NEQ;
				}
				move(1);
				return TOK_NOT;

			case '|':
				if (peekc(1) == '|') {
					move(2);
					return TOK_OR;
				}
				move(1);
				return TOK_BITOR;

			case '&':
				if (peekc() == '&') {
					move(1);
					return TOK_AND;
				}
				return TOK_BITAND;

			case '/':
				return TOK_DIVIDE;

			default:
				return (TOK)getc();
		}
	}

	ref_ptr<ast::node> parse0() {
		if (token == TOK_STRINGLITERAL)
			return new ast::lit_node(0, (const uint64&)value);
		return nullptr;
	}

	ref_ptr<ast::node> parse() {
		auto	n0 = parse0();
		if (!n0)
			return n0;

		switch (token = GetToken()) {
			case TOK_EQUIV:
				token = GetToken();
				return new ast::binary_node(ast::eq, n0, parse0());
			case TOK_NEQ:
				token = GetToken();
				return new ast::binary_node(ast::eq, n0, parse0());
			default:
				return n0;
		}
	}

	Parser(const char *exp) : string_scan(exp) {
		token = GetToken();
	}
};

template<typename F> struct closer {
	F	f;
	closer(F &&f) : f(f) {}
	~closer() { f(); }
};

template<typename F> closer<F> make_close(F f) { return f; }

struct MakeWriter : stream_accum<ostream_ref, 256> {
	MakeWriter(ostream_ref file) : stream_accum<ostream_ref, 256>(file) {}

	template<typename T> MakeWriter& put_line(const T &value) {
		*this << value << '\n';
		return *this;
	}

	template<typename T> void put_macro(const char *name, const T &value) {
		*this << name << "\t= " << value << '\n';
	}

	template<typename T> void put_macro(const char *name, const dynamic_array<T> &value) {
		*this << name << "\t=";
		for (auto &i : value)
			*this << "\\\n\t$(call s2@," << i << ')';
		*this << "\n\n";
	}

	void put_rule(const char *target, const char *depend, const char *command) {
		*this << target;
		if (depend)
			*this << " : " << depend<< "\n";
		else
			*this << ":\n";

		if (command)
			*this << '\t' << command << "\n";
		*this << '\n';
	}

	void put_ast(ast::node *n) {
		switch (n->kind) {
			case ast::literal: {
				ast::lit_node	*b = (ast::lit_node*)n;
				compact_string	s = (compact_string&)b->v;
				*this << s;
				break;
			}
		}
	}

	closer<job> put_cond(const char *cond, bool flip) {
		if (cond) {
			Parser	parser(cond);
			auto	n = parser.parse();
			if (flip)
				n = n->flip_condition();
			switch (n->kind) {
				case ast::eq: {
					ast::binary_node	*b = (ast::binary_node*)n.get();
					*this << "ifeq (";
					put_ast(b->left);
					*this << ',';
					put_ast(b->right);
					*this << ")\n";
					return job(make_job([this]() {
						*this << "endif\n";
					}));
				}
				case ast::ne: {
					ast::binary_node	*b = (ast::binary_node*)n.get();
					*this << "ifne (";
					put_ast(b->left);
					*this << ',';
					put_ast(b->right);
					*this << ")\n";
					return job(make_job([this]() {
						*this << "endif\n";
					}));
				}
			}
		}
		return job(make_job([]() {}));
	}

};

hash_set_with_key<string>	source_dirs(const dynamic_array<string> &files) {
	hash_set_with_key<string>	dirs;
	for (auto &i : files)
		dirs.insert(filename(i).dir());
	return dirs;
}


bool MakefileHandler::Write(ISO_ptr<void> p, ostream_ref file) {
	filename	dir		= CurrentFilename();
	filename	target	= "isocmd.exe";

	struct Target {
		dynamic_array<ISO_ptr<void> >	items;
		dynamic_array<ISO_ptr<void> >	defs;
	};

	hash_map<string, Target>	targets;

	if (p.IsType("VSProject")) {
		for (auto &i : *(anything*)p) {
			if (i.IsID("ItemGroup")) {
				for (auto &j : *(anything*)i)
					targets[j.ID().get_tag()]->items.push_back(j);

			} else if (i.IsID("ItemDefinitionGroup")) {
				for (auto &j : *(anything*)i)
					targets[j.ID().get_tag()]->items.push_back(j);
			}
		}
	}


	dynamic_array<filename>	sysdirs({
		"$(WINDOWS)\\um",
		"$(WINDOWS)\\shared",
		"$(VC)\\include",
		"$(SCE_ORBIS_SDK_DIR)\\target\\include_common",
		"$(SDK)\\Maya\\Maya2018\\include",
		"$(DurangoXDK)xdk\\Include\\um",
		"$(DurangoXDK)171100\\PC\\Include",
		"$(SDK)\\PS3TMAPI\\include",
		"$(X360)\\include\\win32"
		});

	dynamic_array<filename>	incdirs({
		"..",
		"..\\common",
		"..\\platforms\\pc",
		"..\\platforms\\pc\\x64",
		"..\\platforms\\pc\\dx11",
		"$(ZLIB)",
		"$(LIBPNG)",
		"$(LAME)",
		"$(FLAC)",
		"$(BZIP2)",
		"$(SDK)"
		});

	dynamic_array<filename>	libdirs({
		"$(WINDOWS)\\ucrt\\x64",
		"$(VC)\\lib\\x64",
 		"$(SDK)\\Maya\\Maya2018\\LIB",
		"$(SDK)\\xvidcore\\build\\win32\\Debug-static\\x64",
		"$(SCE_ORBIS_SDK_DIR)\\host_tools\\lib",
		"$(XboxOneXDKLatest)PC\\lib\\amd64",
//		"$(SDK)\\directx\\Lib\\x64",
		"$(X360)\\lib\\x64\\vs2010",
		"$(ZLIB)\\x64\\v141\\$(LIBTYPE)",
		"$(LIBPNG)\\x64\\v141\\$(LIBTYPE)",
		"$(LAME)\\x64\\v141\\$(LIBTYPE)",
		"$(FLAC)\\src\\libFLAC\\x64\\v141\\$(LIBTYPE)",
		"$(BZIP2)\\x64\\v141\\$(LIBTYPE)",
		"$(XEDK)\\lib\\x64\\vs2010",
		"$(SDK)\\unrar\\4.2.4\\build\\x64\\$(LIBTYPE)"
	});

	dynamic_array<filename>	libs({
		"ucrtd",
		"vcruntimed",
		"msvcrtd",
		"vfw32",
		"winmm",
		"wininet",
		"dbghelp",
		"kernel32",
		"user32",
		"gdi32",
		"winspool",
		"comdlg32",
		"advapi32",
		"shell32",
		"ole32",
		"oleaut32",
		"uuid",
		"odbc32",
		"odbccp32",
		"libFLAC",
		"libmp3lame",
		"libmpg",
		"libpng",
		"zlib",
		"libbz2",
		"libunrar",
		"d3dcompiler",
		"xmaencoder",
		"xcompress",
		"xgraphics",
		"xbdm",
		"$(SDK)\\directx\\Lib\\x64\\d3d9",
		"$(SDK)\\directx\\Lib\\x64\\d3dx9",
		"$(SDK)\\directx\\Lib\\x64\\dxerr",
		"libSceGpuAddress_debug",
		"$(SDK)\\PowerVR\\Tools\\PVRTexTool\\Library\\Windows_x86_64\\Dynamic\\PVRTexLib",
		"$(SDK)\\PS3TMAPI\\lib\\PS3TMAPIx64.lib"
	});

	MakeWriter	writer(file);

	writer.put_macro("Configuration", "Debug");
	writer.put_macro("Platform", "x64");


	writer.put_line("sp:=")
		.put_line("sp+=")
		.put_line("s2@ = $(subst $(sp),@,$1)")
		.put_line("@2s = $(subst @,$(sp),$1)")
		.put_line("name = $(notdir $(basename $1))");

	writer.put_macro("SDK ",	"d:\\dev\\sdk");
	writer.put_macro("ZLIB  ",	"$(SDK)\\zlib\\1.2.8");
	writer.put_macro("LIBPNG",	"$(SDK)\\libpng\\1.6.16");
	writer.put_macro("LAME",	"$(SDK)\\lame\\3.99.5");
	writer.put_macro("FLAC",	"$(SDK)\\flac\\1.3.1");
	writer.put_macro("BZIP2",	"$(SDK)\\bzip2\\1.0.6");
	writer.put_macro("X360",	"$(SDK)\\Microsoft Xbox 360 SDK");
	writer.put_macro("WINDOWS",	"C:\\Program Files (x86)\\Windows Kits\\10\\Lib\\10.0.16299.0");
	writer.put_macro("VC",		"C:\\Program Files (x86)\\Microsoft Visual Studio\\2017\\Professional\\VC\\Tools\\MSVC\\14.12.25827");


	writer.put_macro("OBJDIR", "$(Configuration)");
	writer.put_macro("LIBTYPE", "$(Configuration)");

	writer.put_macro("SYSDIRS", sysdirs);
	writer.put_macro("INCDIRS", incdirs);
	writer.put_macro("LIBDIRS", libdirs);

	writer.put_macro("LIBS", libs);
	writer.put_macro("DEFS", "WIN32 _CONSOLE _MBCS CROSS_PLATFORM USE_HTTP _DEBUG");

	writer.put_macro("LDFLAGS", "$(call @2s,$(LIBDIRS:%=-L \"%\")) $(LIBS:%=-l%) -nodefaultlibs");
	writer.put_macro("CPPFLAGS", "$(DEFS:%=-D%) $(call @2s,$(INCDIRS:%=-iquote \"%\")) $(call @2s,$(SYSDIRS:%=-I \"%\")) -std=c++14 -mavx2 -fno-operator-names -O0");
	writer.put_macro("CC", "clang");
	writer.put_macro("LD", "clang");

//	writer.get_defs("ClCompile", "PreprocessorDefinitions", "DEFS");

	writer.put_rule("build", target, 0);
	writer.put_rule("%/", 0, "- mkdir $(subst /,\\,$@) 2>NUL");

#if 0
	hash_set_with_key<string>	cdirs = source_dirs(map["ClCompile"]);
	writer << "vpath %.cpp";
	for (auto &i : cdirs)
		writer << "\\\n\t" << filename(i).relative_to(dir);
	writer << "\n\n";

	writer << "OBJS =";
	for (auto &c : map["ClCompile"]) {
		writer << "\\\n\t" << filename(c).name().set_ext("o");
	}
	writer << "\n\n";
	writer.put_rule(target, "$(OBJS:%=$(OBJDIR)/%)", "$(LD) $(LDFLAGS) $(OBJS:%=$(OBJDIR)/%) -o $@");
	writer.put_rule("$(OBJDIR)/%.o $(OBJDIR)/%.d", "%.cpp", "$(CC) $(CPPFLAGS) -c $< -o $(basename $@).o  -MMD -MF $(basename $@).d");
	writer << "include $(OBJS:%.o=$(OBJDIR)/%.d)\n";

#else
//	dynamic_array<const char *>	src;
	writer.put_line("SRC\t:=");
	for (auto &j : targets["ClCompile"]->items) {
		if (j.IsType<anything>()) {
			anything	&k = *(anything*)j;
			if (ISO_ptr<string> include = k["Include"]) {
				ISO::PtrBrowser	cond;
				bool		flip = false;
				if (ISO::PtrBrowser exclude = (ISO::ptr_machine<void>)k["ExcludedFromBuild"]) {
					cond = exclude["Condition"];
					flip = !(cond ? exclude[1].get<bool>() : exclude.get<bool>());
				}
				if (flip && !cond)
					continue;
				auto closer = writer.put_cond(cond.get(), flip);
				writer << "SRC\t+=" << *include << '\n';
//				src.push_back(*include);
			}
		}
	}

//	writer.put_macro("SRC", src);

	writer.put_line("$(foreach i,$(SRC),$(eval $(OBJDIR)/$(call name,$(i)).o $(OBJDIR)/$(call name,$(i)).d: $(i)))");

	writer.put_macro("OBJS", "$(notdir $(basename $(SRC)))");
	writer.put_rule(target, "$(OBJS:%=$(OBJDIR)/%.o)", "$(LD) $(LDFLAGS) $(OBJS:%=$(OBJDIR)/%.o) -o $@");
	writer.put_rule("%.o %.d", "", "$(CC) $(CPPFLAGS) -c $< -o $(basename $@).o  -MMD -MF $(basename $@).d");
	writer.put_line("include $(OBJS:%=$(OBJDIR)/%.d)");
#endif


	return true;
}

