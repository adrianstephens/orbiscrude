#include "cpp.h"
#include "extra/expression.h"
#include "extra/text_stream.h"

namespace iso {

CPP::CPP(istream_ref file) : nestin(0), nestout(0), backup(0), macro(0), identifier(0), incstack(0), macstack(0) {
	AddSource(malloc_block(file, file.length()));
	outbuffer	= outptr = outend = (char*)malloc(4096);
}

CPP::~CPP() {
	free(outbuffer);
}

int CPP::GetChar() {
	int	c;
	if (c = backup) {
		backup = 0;
	} else for (;;) {
		if (macro) {
			if (c = *macro++)
				break;
			if (c = macstack->backup) {
				macstack->backup = 0;
				macro--;
				break;
			} else {
				macro	= macstack->macro;
				definition	*def = macstack->def;
				for (int i = 0; i < def->nparams; i++)
					delete FindDefine(def->params[i])->unlink();
				macrosub	*m = macstack;
				macstack	= m->next;
				delete m;
			}
		} else {
			if (!incstack)
				return -1;

			c = incstack->getc();
			if (c == -1) {
				include	*i	= incstack;
				incstack	= i->next;
				delete i;
				return '\n';
			}
			return c;
		}
	}
	return c;
}

int CPP::GetCharSubs() {
	int	c;
	for (;;) {
		if (identifier) {
			if (c = *identifier++)
				break;
			identifier = NULL;
		}
		c = GetChar();

		if (c == '/') {
			c = GetChar();
			if (c == '/') {
				do c = GetChar(); while (c != -1 && c != '\r' && c != '\n');
			} else if (c == '*') {
				do {
					do c = GetChar(); while (c != '*' && c != -1);
					c = GetChar();
				} while (c != '/' && c != -1);
				c = GetChar();
			} else {
				backup = c;
				c = '/';
			}
		}

		if (!is_alpha(c) && c != '_')
			break;

		char	*p = identifier_buffer;
		do {
			*p++ = c;
			c = GetChar();
		} while (is_alphanum(c) || c == '_');
		*p		= 0;

		if (definition *def = FindDefine(identifier_buffer)) {
			bool	used = false;
			for (macrosub *m = macstack; !used && m; m = m->next)
				used = (m->def == def);

			if (!used) {
				if (def->nparams) {
					for (int i = 0; i < def->nparams; i++) {
						backup	= c;
						c = SkipWhitespace();
						char	sep = i ? ',' : '(';
						if (c != sep)
							throw(format_string("expected %c", sep));
						char	*p = outend;

						int		depth = 0;
						while (((c = GetCharSubs()), depth) || (c != ',' && c != ')')) {
							*p++ = c;
							if (c == '(')
								++depth;
							else if (c == ')')
								--depth;
						}

						backup	= c;
						*p++	= 0;
						defines.push_front(new definition(def->params[i], outend));
					}
					c = SkipWhitespace();	// verify ')'
					c = GetChar();
				}
				if (def->value) {
					macrosub	*m = new macrosub;
					m->next		= macstack;
					m->def		= def;
					m->backup	= c;
					m->macro	= macro;
					macro		= def->value;
					macstack	= m;
				} else {
					backup		= c;
				}
				continue;
			}
		}
		backup 		= c;
		identifier	= identifier_buffer;
	}
	return c;
}

int CPP::SkipWhitespace() {
	int	c;
	do c = GetChar(); while (c == ' ' || c == '\t');
	return c;
}

int CPP::SkipToEOL() {
	int	c;
	do c = GetChar(); while (c != -1 && c != '\n');
	return c;
}

char *CPP::GetIdentifier() {
	char	*start	= outend;
	int		c 		= SkipWhitespace();
	if (is_alpha(c) || c == '_') {
		while (is_alphanum(c) || c == '_') {
			*outend++ = c;
			c = GetChar();
		}
	}
	*outend++ 	= 0;
	backup 		= c;
	return start;
}

int64 CPP::EvaluateExpression() {
	struct context : default_get_op<context> {
		CPP		&cpp;
		int		bckup;
		int		getc() {
			int c = bckup;
			if (c)
				bckup	= 0;
			else
				c = cpp.GetCharSubs();
			return c;
		}
		void	put_back(int c) {
			bckup = c;
		}
		void	backup() {

		}

		uint64	get_value(bool discard)	{
			char c = getc();
			if (is_digit(c))
				return read_unsigned(*this, c);

			if (is_alpha(c) || c == '_') {
				char	identifier[256];
				read_token(*this, identifier, char_set::wordchar, c);

				if (strcmp(identifier, "defined") == 0) {
					c = skip_chars(*this, char_set(" \t"));
					bool	par = c == '(';
					if (par)
						c = skip_chars(*this, char_set(" \t"));

					read_token(*this, identifier, char_set::wordchar, c);

					if (par) {
						c = skip_chars(*this, char_set(" \t"));
						if (c != ')')
							throw("Missing \")\"");
					}
					return cpp.FindDefine(identifier) ? 1 : 0;
				}
				put_back(c);
			}
			//FAIL!
			return 0;
		}

		context(CPP	&cpp) : cpp(cpp), bckup(0) {}
	};
	context	ctx(*this);
	return expression::evaluate<int64>(ctx);
}

CPP::definition *CPP::FindDefine(const char *name) {
	for (e_list<definition>::iterator i = defines.begin(); i != defines.end(); ++i) {
		if (strcmp(i->name, name) == 0)
			return i.get();
	}
	return nullptr;
}

void CPP::ProcessDefine() {
	outend = outbuffer;
	GetIdentifier();
	definition	*def = new definition(outbuffer);
	defines.push_back(def);

	int	c = GetChar();
	if (c == '(') {
		do {
			def->addparam(GetIdentifier());
			c = SkipWhitespace();
		} while (c ==',');
		if (c != ')')
			throw("Missing \")\"");
		c = SkipWhitespace();
	}

	const char *value = outend;
	int	pc = c;
	for (;;) {
		while (c != -1 && c != '\r' && c != '\n') {
			*outend++ = c;
			pc	= c;
			c	= GetChar();
		}
		if (pc == '\\') {
			if (outend[-1] == pc)
				outend--;
			*outend++ = c;
			c	= GetChar();
		}
		if (pc != '\\')
			break;
	}

	*outend++ = 0;
	def->setvalue(value);
	backup = c;
}

void CPP::ProcessUndef() {
	outend = outbuffer;
	if (definition *def = FindDefine(GetIdentifier()))
		delete def->unlink();
}

void CPP::AddFile(const char *f) {
	filename	f2(f);
	if (f2.is_relative() && fn)
		f2 = filename(fn).rem_dir().add_dir(f2);

	include *i	= new include(malloc_block(FileInput(f2), filelength(f)));
	i->next		= incstack;
	incstack	= i;
	i->fn		= fn;
}

void CPP::AddSource(malloc_block &&blob) {
	include *i	= new include(move(blob));
	i->next		= incstack;
	incstack	= i;
}

void CPP::ProcessInclude() {
	int		c = SkipWhitespace();
	int		end;
	if (c == '"')
		end = c;
	else if (c == '<')
		end = '>';
	else
		throw("Expected '\"' or '<'");

	filename	f2;
	char		*p = f2;
	while ((c = GetChar()) != end) {
		if (c == -1 || c == '\n' || c == '\r')
			break;
		*p++ = c;
	};
	*p = 0;
	SkipToEOL();
	AddFile(f2);
}

bool CPP::ProcessDirective(const char *directive) {
	static const char *directives[] = {
		"if",
		"ifdef",
		"ifndef",
		"elif",
		"else",
		"endif",
		"define",
		"undef",
		"line",
		"include",
	};
	for (int i = 0; i < num_elements(directives); i++) {
		size_t	len = strlen(directives[i]);
		if (outend - directive == len && strncmp(directives[i], directive, len) == 0) {
			switch (i) {
				case 0:	//if
					if (nestout || !EvaluateExpression())
						nestout++;
					else
						nestin++;
					SkipToEOL();
					break;
				case 1:	//ifdef
					if (nestout || !FindDefine(GetIdentifier()))
						nestout++;
					else
						nestin++;
					SkipToEOL();
					break;
				case 2:	//ifndef
					if (nestout || FindDefine(GetIdentifier()))
						nestout++;
					else
						nestin++;
					SkipToEOL();
					break;
				case 3:	//elif
					if (nestout == 0) {
						if (nestin == 1) {
							nestin--;
							nestout++;
						}//else error
					} else if (nestout == 1) {
						if (EvaluateExpression()) {
							nestin++;
							nestout--;
						}
					}
					SkipToEOL();
					break;
				case 4:	//else
					if (nestout == 0) {
						if (nestin == 1) {
							nestin--;
							nestout++;
						}//else error
					} else if (nestout == 1) {
						nestin++;
						nestout--;
					}
					SkipToEOL();
					break;
				case 5:	//endif
					if (nestout)
						nestout--;
					else if (nestin)
						nestin--;
					//else error
					SkipToEOL();
					break;
				case 6:	//define
					if (nestout == 0)
						ProcessDefine();
					break;
				case 7:	//undef
					if (nestout == 0)
						ProcessUndef();
					break;
				case 8:	//line
					SkipToEOL();
					break;
				case 9:	//include
					if (nestout == 0)
						ProcessInclude();
					break;
			}
			break;
		}
	}
	return false;
}

void CPP::GetMore() {
	outptr = outend = outbuffer;

	char	*directive	= NULL;
	bool	startofline = true;

	for (;;) {
		int		c 		= GetCharSubs();
		bool	white	= c == ' ' || c == '\t';
		bool	alpha	= is_alpha(c) || c == '_';

		if (directive) {
			if (white && directive == outend) {
				directive++;
			} else if (!alpha) {
				backup		= c;
				ProcessDirective(directive);
				directive	= NULL;
				outend		= outbuffer;
				startofline	= true;
				continue;
			}
		}
		if (c == '#') {
			if (startofline) {
				directive = outend + 1;
			} else if ((c = GetChar()) == '#') {
				continue;
			} else {
				backup = c;
				c = '#';
			}
		}

		if (!white)
			startofline = false;

		if (c == -1)
			return;

		if (directive || nestout == 0) {
			*outend++ = c;
			if (c == '\n' || c == '\r')
				break;
		}

		if (c == '\n' || c == '\r')
			startofline = true;
	}
}

size_t CPP::readbuff(void *buffer, size_t size) {
	size_t	total = 0;
	while (total < size) {
		size_t	n = outend - outptr;

		if (n == 0) {
			GetMore();
			n = outend - outptr;
			if (n == 0)
				break;
		}

		if (n > size - total)
			n = size - total;

		memcpy((uint8*)buffer + total, outptr, n);
		total	+= n;
		outptr	+= n;
	}
	return int(total);
}

int CPP::getc() {
	if (outend == outptr) {
		GetMore();
		if (outend == outptr)
			return -1;
	}
	return *outptr++;
}

} //namespace iso
