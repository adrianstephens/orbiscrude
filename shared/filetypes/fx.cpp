#include "systems/conversion/platformdata.h"
#include "iso/iso_files.h"
#include "common/shader.h"
#include "fx.h"

using namespace iso;

//-----------------------------------------------------------------------------
//	quick parser to find fx stuff
//-----------------------------------------------------------------------------

struct mismatch_exception {
	const char *p;
	mismatch_exception(const char *_p) : p(_p) {}
	uint32	getline(const char *s) const {
		uint32	line = 0;
		while (s = string_find(s + 1, '\n', p))
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

count_string GetToken(string_scan &s, const char_set &tokspec) {
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
			default:
				if (is_alpha(c))
					return s.get_token(tokspec);
				return s.get_n(1);
		}
	}
}

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

cgclib::item *iso::ParseFX(string_scan s) {
	cgclib::item	*items		= 0;
	char_set		tokspec		= char_set::alphanum + '_';
	uint32			lastline	= 0;
	filename		lastfile;

	count_string	token;
	try { while ((token = GetToken(s, tokspec)).length()) {
		if (token[0] == '#') {
			if (GetToken(s, tokspec) == "line") {
				s >> lastline;
				if (s.peekc() != '\n')
					lastfile = GetBlock(s);
			}
		} else if (token.begins("technique")) {
			cgclib::technique	*cgct = cgclib::AddItem<cgclib::technique>(&items, dup_token(s.get_token(tokspec)));
			SkipAnnotations(s);

			while (s.skip_whitespace().peekc() != '}') {
				token = GetToken(s, tokspec);

				if (token == "pass") {
					cgclib::pass	*cgcp	= cgclib::AddItem<cgclib::pass>((cgclib::item**)&cgct->passes, dup_token(s.get_token(tokspec)));
					SkipAnnotations(s);

					while (s.skip_whitespace().peekc() != '}') {
						token = GetToken(s, tokspec);
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
				token = GetToken(s, tokspec);
				if (token == "register") {
					c = s.skip_whitespace().getc();
					s.skip_to(')');
				}
				c = SkipAnnotations(s);
			}
			if (c == '{') {
				while (s.skip_whitespace().peekc() != '}') {
					token = GetToken(s, tokspec);
					if (is_alphanum(*token.begin())) {
						cgclib::state_symb	*cgcs = cgclib::AddItem<cgclib::state_symb>(&cgsm->states, dup_token(token));
						token = GetToken(s, tokspec);	// '='
						token = s.get_token(tokspec);	// value
						cgcs->value = dup_token(token);
					}
				}
				s.move(1);
			} else {
				s.move(-1);
			}
		}
	} } catch (mismatch_exception ex) {
		unused(ex);
		//throw_accum("mismatched brackets at line " << ex.getline(start));
		throw_accum("mismatched brackets at " << lastfile << '(' << lastline << ')');
	}

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
	ISO_ptr<void>			Read(tag id, istream_ref file, const filename *fn)  {
		Platform	*platform = Platform::Get(ISO::root("variables")["exportfor"].GetString());
		return platform->ReadFX(id, file, fn);
	}

	const char*		GetExt() override { return "fx"; }
	ISO_ptr<void>	Read(tag id, istream_ref file) override { return Read(id, file, NULL); }

	ISO_ptr<void>	ReadWithFilename(tag id, const filename &fn) override {
		FileInput	file(fn);
		if (file.exists())
			return Read(id, file, &fn);
		return ISO_NULL;
	}

public:
	FxFileHandler() {
		ISO_get_cast(MakeTechnique);
	}

} fxf;

