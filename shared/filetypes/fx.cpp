#include "systems/conversion/platformdata.h"
#include "iso/iso_files.h"
#include "common/shader.h"
#include "fx.h"

using namespace iso;

//-----------------------------------------------------------------------------
//	quick parser to find fx stuff
//-----------------------------------------------------------------------------

struct string_scan_lines : string_scan {
	uint32			lastline	= 0;
	filename		lastfile;
	using string_scan::string_scan;

	string_scan_lines(string_scan s) : string_scan(s) {}

	string_scan_lines&	skip_whitespace();
	count_string		get_token(const char_set &set);

};

string_scan_lines& string_scan_lines::skip_whitespace() {
	while (string_scan::skip_whitespace().peekc() == '#') {
		if (move(1).get_token() == "line") {
			*this >> lastline;
			if (peekc() != '\n') {
				const char	*p	= string_scan::skip_whitespace().getp();
				if (skip_to(0))
					lastfile	= count_string(p, getp());
			}
		}
	}
	return *this;
}
count_string string_scan_lines::get_token(const char_set& set) {
	for (;;) {
		switch (char c = skip_whitespace().peekc()) {
			case 0:
				return count_string();
			case '<':
				if (move(1).skip_to('>'))
					break;
				ISO_ASSERT(0);
			case '[':	case '{': case '(': case '"': case '\'': {
				auto p = getp();
				if (skip_to(0))
					return {p, getp()};
//					break;
				ISO_ASSERT(0);
			}
			case ']': case '}': case ')':
				throw_accum("mismatched brackets at " << lastfile << '(' << lastline << ')');
				break;

			default:
				if (set.test(c))
					return string_scan::get_token(set);
				return get_n(1);
		}
	}
}



#if 0

struct mismatch_exception {
	const char *p;
	mismatch_exception(const char *_p) : p(_p) {}
	uint32	getline(const char *s) const {
		uint32	line = 0;
		while (s = string_find(s + 1, p, '\n'))
			++line;
		return line;
	}
};

count_string GetBlock(string_scan &s) {
	const char	*p	= s.skip_whitespace().getp();
	if (s.skip_to(0))
		return count_string(p, s.getp());
	return count_string();
}

count_string GetToken(string_scan &s, const char_set &tokspec, uint32 &lastline, filename &lastfile) {
	for (;;) {
		switch (char c = s.skip_whitespace().peekc()) {
			case 0:
				return count_string();
			case '<':
				if (s.move(1).skip_to('>'))
					break;
				ISO_ASSERT(0);
			case '[':	case '{': case '(': case '"': case '\'':
			{
				auto p = s.getp();
				if (s.skip_to(0))
					break;
				ISO_ASSERT(0);
			}
			case ']': case '}': case ')':
			 	throw(mismatch_exception(s.getp()));
				break;

			case '#':
				if (s.move(1).get_token(tokspec) == "line") {
					s >> lastline;
					if (s.peekc() != '\n')
						lastfile = GetBlock(s);
				}
				break;

			default:
				if (is_alpha(c))
					return s.get_token(tokspec);
				return s.get_n(1);
		}
	}
}
#endif
char *dup_token(const count_string &name) {
	char *m = 0;
	if (size_t n = name.length()) {
		memcpy(m = (char*)::malloc(n + 1), name.begin(), n);
		m[n] = 0;
	}
	return m;
}

int SkipAnnotations(string_scan &s) {
	int	c;
	while ((c = s.skip_whitespace().getc()) == '<')
		s.skip_to('>');
	return c;
}

cgclib::item *iso::ParseFX(string_scan s0) {
	string_scan_lines	s(s0);
	cgclib::item	*items		= 0;
	char_set		tokspec		= char_set::alphanum + '_';

	count_string	token;
	//try {
	while ((token = s.get_token(tokspec)).length()) {
		if (token.begins("technique")) {
			cgclib::technique	*cgct = cgclib::AddItem<cgclib::technique>(&items, dup_token(s.get_token(tokspec)));
			SkipAnnotations(s);

			while (s.skip_whitespace().peekc() != '}') {
				token = s.get_token(tokspec);

				if (token == "pass") {
					cgclib::pass	*cgcp	= cgclib::AddItem<cgclib::pass>((cgclib::item**)&cgct->passes, dup_token(s.get_token(tokspec)));
					SkipAnnotations(s);

					while (s.skip_whitespace().peekc() != '}') {
						token = s.get_token(tokspec);
						if (is_alphanum(*token.begin())) {
							cgclib::state_symb	*cgcs = cgclib::AddItem<cgclib::state_symb>(&cgcp->shaders, dup_token(token));
							int shader = token == istr("VertexShader") || token == istr("VertexProgram") ? 1
									:	token == istr("PixelShader") || token == istr("FragmentProgram") ? 2
									:	0;
							char	c = s.skip_whitespace().getc();

							if (c == '(') {
								const char *a = s.getp();
								s.skip_to(')');
								cgcs->value = dup_token(str(a, s.getp() - 1));

							} else if (c == '=') {
								token = s.get_token(tokspec);
								if (shader) {
									if (token == "compile") {
										token = s.get_token(tokspec);	// profile
										token = s.get_token(tokspec);
									}
								}
								if (token.blank()) {
									const char *start = s.getp();
									switch (char c = s.getc()) {
										case '<':	s.skip_to('>'); token = count_string(start, s.getp()); break;
										case '[':	s.skip_to(']'); token = count_string(start, s.getp()); break;
										case '{':	s.skip_to('}'); token = count_string(start, s.getp()); break;
										case '(':	s.skip_to(')'); token = count_string(start, s.getp()); break;
										case '"': case '\'': s.skip_to(c);token = count_string(start, s.getp()); break;
										default: s.move(-1); break;
									}
								}
								cgcs->value = dup_token(token);
							}
						}
					}
					s.move(1);
				}
			}
			s.move(1);

		} else if (token == "SamplerState") {
			cgclib::sampler	*cgsm = cgclib::AddItem<cgclib::sampler>(&items, dup_token(s.get_token(tokspec)));
			int	c = SkipAnnotations(s);
			if (c == ':') {
				token = s.get_token(tokspec);
				if (token == "register") {
					c = s.skip_whitespace().getc();
					s.skip_to(')');
				}
				c = SkipAnnotations(s);
			}
			if (c == '{') {
				while (s.skip_whitespace().peekc() != '}') {
					token = s.get_token(tokspec);
					if (is_alphanum(*token.begin())) {
						cgclib::state_symb	*cgcs = cgclib::AddItem<cgclib::state_symb>(&cgsm->states, dup_token(token));
						token = token = s.get_token(tokspec);	// '='
						token = s.get_token(tokspec);	// value
						cgcs->value = dup_token(token);
					}
				}
				s.move(1);
			} else {
				s.move(-1);
			}
		}
	}/* } catch (mismatch_exception ex) {
		unused(ex);
		//throw_accum("mismatched brackets at line " << ex.getline(start));
		throw_accum("mismatched brackets at " << s.lastfile << '(' << s.lastline << ')');
	}*/

	return		items;
}

cgclib::item *iso::Find(cgclib::item *s, const char *name, cgclib::TYPE type) {
	while (s && (s->type != type || str(s->name) != name))
		s = s->next;
	return s;
}

//-----------------------------------------------------------------------------
//	inline_technique
//-----------------------------------------------------------------------------

ISO_ptr<technique> MakeTechnique(ISO_ptr<inline_technique> p) {
//	return iso::root("data")["simple"]["lite"];

	Platform	*platform = Platform::Get(ISO::root("variables")["exportfor"].GetString());
	ISO_ptr<technique>	t(0);

	for (auto &pass : *p) {
		t->Append(platform->MakeShader(pass));
	}
	return t;
}

//-----------------------------------------------------------------------------
//	FxFileHandler
//-----------------------------------------------------------------------------

class FxFileHandler : public FileHandler {
	ISO_ptr<void>	Read2(tag id, istream_ref file, const filename *fn)  {
		Platform	*platform = Platform::Get(ISO::root("variables")["exportfor"].GetString());
		return platform->ReadFX(id, file, fn);
	}

	const char*		GetExt() override { return "fx"; }
	ISO_ptr<void>	Read(tag id, istream_ref file) override { return Read2(id, file, NULL); }

	ISO_ptr<void>	ReadWithFilename(tag id, const filename &fn) override {
		FileInput	file(fn);
		if (file.exists())
			return Read2(id, file, &fn);
		return ISO_NULL;
	}

public:
	FxFileHandler() {
		ISO_get_cast(MakeTechnique);
	}

} fxf;

