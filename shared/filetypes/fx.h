#ifndef FX_H
#define FX_H

#include "cgc/cgclib.h"
#include "iso/iso_files.h"

namespace iso {

struct inline_pass : ISO_openarray<ISO_ptr<string> > {};
struct inline_technique : ISO_openarray<ISO_ptr<inline_pass> > {};

struct CgStructHolder : holder<cgc::CG*> {
	CgStructHolder()	{ t = cgclib::create(); }
	~CgStructHolder()	{ cgclib::destroy(t); }
};

struct BufferOutputData : cgclib::Outputter {
	void	*buffer;
	size_t	size, space;
	void	put(const void *p, size_t len) {
		if (size + len > space) {
			space	= space ? space * 2 : len < 512 ? 1024 : len * 2;
			buffer	= realloc(buffer, space);
		}
		memcpy((char*)buffer + size, p, len);
		size += len;
	}
	bool			empty()		const	{ return size == 0; }
	size_t			length()	const	{ return size;		}
	operator		void*()		const	{ return buffer;	}
	void			reset()				{ size = 0;			}

	BufferOutputData() : cgclib::Outputter(this), buffer(0), size(0), space(0) {}
	~BufferOutputData() { free(buffer); }
};

struct PrintOutputData : cgclib::Outputter {
	FILE 		*file;
	bool		triggered;
	void		put(const void *p, size_t len)	{ triggered = true; fwrite(p, 1, len, file); }
	PrintOutputData(FILE *file = stdout) : cgclib::Outputter(this), file(file), triggered(false) {}
};

struct CGCLIBIncludeHandler : cgclib::Includer, IncludeHandler {
	cgclib::Blob open(const char *fn) {
		void	*data;
		if (size_t r = IncludeHandler::open(fn, (const void**)&data))
			return cgclib::Blob(data, r);
		return cgclib::Blob();
	}
	void close(const cgclib::Blob &blob) {
		IncludeHandler::close(blob);
	}
	CGCLIBIncludeHandler(const filename *_fn, const char *_incs) : cgclib::Includer(this), IncludeHandler(_fn, _incs)	{}
};

cgclib::item *ParseFX(string_scan s);
cgclib::item *Find(cgclib::item *s, const char *name, cgclib::TYPE type);

struct ErrorCollector {
	struct entry {
		uint32	code;
		crc32	fn;
		uint32	line, col;
		entry(uint32 _code, const char *_fn, uint32 _line, uint32 _col) : code(_code), fn(_fn), line(_line), col(_col) {}
		bool operator<(const entry &e) const {
			return	code	< e.code	|| (code	== e.code
				&&	(fn		< e.fn		|| (fn		== e.fn
				&&	(line	< e.line	|| (line	== e.line
				&&	col		< e.col
			)))));
		}
	};
	set<entry>		had;
	toggle_accum	error_builder;
	bool			have_errors;
	bool			have_warnings;

	ErrorCollector() : have_errors(false), have_warnings(false) {}

	static void Error(string_accum &b, uint32 code, const count_string &msg, const char *fn = 0, uint32 line = 0) {
		if (line)
			b << fn << '(' << line << "): ";
		b << code << ": " << msg << '\n';
	}

	void	Error(uint32 code, const count_string &msg, const char *fn = 0, uint32 line = 0, uint32 col = 0) {
		if (had.insert(entry(code, fn, line, col)))
			Error(error_builder, code, msg, fn, line);
	}

	void	Throw() {
		throw(error_builder.detach()->begin());
	}
};

}

ISO_DEFUSER(iso::inline_pass, ISO_openarray<ISO_ptr<string> >);
ISO_DEFUSER(iso::inline_technique, ISO_openarray<ISO_ptr<inline_pass> >);

#endif	// FX_H
