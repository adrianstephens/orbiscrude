#ifndef CPP_H
#define CPP_H

#include "base/defs.h"
#include "base/list.h"
#include "stream.h"
#include "filename.h"

namespace iso {

class CPP : public stream_defaults<CPP> {

	struct definition : e_link<definition> {
		string	name, value, params[8];
		int		nparams;
		definition(const char *_name) : name(_name), nparams(0) {}
		definition(const char *_name, const char *_value) : name(_name), value(_value), nparams(0) {}
		void	setvalue(const char *_value)	{ value = _value; }
		void	addparam(const char *param)		{ params[nparams++] = param; }
	};

	struct include {
		include			*next;
		filename		fn;
		malloc_block	blob;
		const char		*ptr;
		int				line_number, col_number;
		int getc() {
			if (ptr == blob.end())
				return -1;
			int	c = *ptr++;
			if (c == '\n') {
				line_number++;
				col_number = 0;
			}
			col_number++;
			return c;
		}
		include(malloc_block &&_blob) : next(0), blob(move(_blob)), ptr(blob), line_number(1), col_number(0) {}
	};

	struct macrosub {
		macrosub	*next;
		definition	*def;
		const char	*args[8];
		int			backup;
		const char	*macro;
	};

	filename			fn;
	int					nestin, nestout;
	int					backup;
	char				*outptr, *outend;
	char				*outbuffer;
	const char			*macro;
	char				*identifier;
	char				identifier_buffer[256];
	e_list<definition>	defines;
	include				*incstack;
	macrosub			*macstack;

	int			GetChar();
	int			GetCharSubs();
	int			SkipWhitespace();
	int			SkipToEOL();
	char*		GetIdentifier();

	int64		EvaluateExpression();
	definition*	FindDefine(const char *name);
	void		ProcessDefine();
	void		ProcessUndef();
	void		ProcessInclude();

	bool		ProcessDirective(const char *directive);
	void		GetMore();
public:
	CPP(istream_ref file);
	~CPP();
	int			getc();
	size_t		readbuff(void *buffer, size_t size);

	int			GetLineNumber()	const								{ return incstack ? incstack->line_number : 0;	}
	void		AddDefine(const char *name, const char *value = 0)	{ defines.push_back(new definition(name, value)); }
	void		AddSource(malloc_block &&blob);
	void		AddFile(const char *fn);
	void		SetFilename(const filename &_fn)	{ fn = _fn; }
	CPP&		me()	{ return *this; }
};

} //namespace iso

#endif//  CPP_H
