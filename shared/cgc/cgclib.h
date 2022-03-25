#ifndef CGC_LIB_H
#define CGC_LIB_H

#include <stdarg.h>

template<typename T> struct List;

namespace cgc {
struct CG;
struct Symbol;
typedef List<Symbol> SymbolList;
}

namespace cgclib {
	using namespace cgc;
	struct Outputter {
		void	(*vput)(Outputter*, const void*, size_t);
		template<typename T> static void tput(Outputter *me, const void *p, size_t len)	{ return static_cast<T*>(me)->put(p, len);	}

		void		put(const void *buffer, size_t size)		{ vput(this, buffer, size); }
		void		putc(char c)								{ vput(this, &c, 1); }
		void		write(const char *fmt, va_list args);
		void		write(const char *fmt, ...);
		template<typename T> Outputter(T *t) : vput(tput<T>)	{}
	};

	struct Blob {
		const void	*start;
		size_t		size;
		Blob() : start(0), size(0)	{}
		Blob(const void *_start, size_t _size) : start(_start), size(_size) {}
		template<typename T> operator T*()	const { return (T*)start; }
		bool	operator!()					const { return !start; }
	};

	struct Includer {
		Blob		(*vopen)(Includer*, const char*);
		void		(*vclose)(Includer*, const Blob&);
		template<typename T> static Blob topen(Includer *me, const char *fn)	{ return static_cast<T*>(me)->open(fn);	}
		template<typename T> static void tclose(Includer *me, const Blob &blob)	{ return static_cast<T*>(me)->close(blob);	}

		Blob		open(const char *fn)	{ return vopen(this, fn); return Blob();  }
		void		close(const Blob &blob)	{ vclose(this, blob); }
		template<typename T> Includer(T *t)	: vopen(topen<T>), vclose(tclose<T>) {}
	};

	enum TYPE {
		TECHNIQUE,
		SAMPLER,
		PASS,
		SHADER,
		BINDING,
		STATE_SYM,
		STATE_NUM,
		STATE_VAR,
	};

	struct item {
		TYPE		type;
		item		*next;
		const char	*name;
		item(TYPE type, const char *name = 0);
		~item();
	};

	template<TYPE T> struct itemT : item {
		itemT(const char *name = 0) : item(T, name) {}
		static TYPE get_type() { return T; }
	};

	struct state_symb : itemT<STATE_SYM> {
		const char		*value;
	};

	struct state_num : itemT<STATE_NUM> {
		int				value;
	};

	struct state_var : itemT<STATE_VAR> {
		const char		*var;
	};

	struct sampler : itemT<SAMPLER> {
		item		*states;
	};

	struct binding : itemT<BINDING> {
		unsigned char	type, size;
		unsigned short	reg;
	};

	struct shader : itemT<SHADER> {
		int			stage;
		void		*code;
		size_t		size;
		item		*bindings;
	};

	struct pass : itemT<PASS> {
		item		*shaders;
		item		*states;
	};

	struct technique : itemT<TECHNIQUE> {
		item		item;
		pass		*passes;
	};

	item *AddItem(item **first, TYPE type, size_t size, const char *name);
	item *AddItemFront(item **first, TYPE type, size_t size, const char *name);

	template<typename T> T *AddItem(item **first, const char *name) { return (T*)AddItem(first, T::get_type(), sizeof(T), name); }
	template<typename T> T *AddItemFront(item **first, const char *name) { return (T*)AddItemFront(first, T::get_type(), sizeof(T), name); }

	typedef unsigned int	uint32_t;

	enum OPTION {
		FX					= 1 << 0,
		PARSE_ONLY			= 1 << 1,
		DEBUG_ENABLE		= 1 << 2,
		NO_STDLIB			= 1 << 3,
		NO_WARNINGS			= 1 << 4,
		IMPLICIT_DOWNCASTS	= 1 << 5,
		GENERATE_COMMENTS	= 1 << 6,
		AUTO_WAIT			= 1 << 7,

		DUMP_STAGES			= 1 << 8,
		DUMP_PARSETREE		= 1 << 9,
		DUMP_FINALTREE		= 1 << 10,
		DUMP_NODETREE		= 1 << 11,

		WARN_ERROR			= 1 << 16,
		PREPROCESS_ONLY		= 1 << 31,
	};

	struct Options {
		uint32_t	opts;
		const char*	name;			// The name of the source file
		const char*	entry;			// The name of the entry shader
		const char*	profile;

		uint32_t	num_macro;		// The number of macro definitions provided.
		const char* const *macro;	// The macro definitions in the form: MACRONAME or MACRONAME=VALUE

		uint32_t	num_include;	// The number of files to force include.
		const char* const *include;	// The files to include before the main source file.

		const char* debug_path;		// If blank, no sdb is output
		Includer	*includer;
		
		Options() : opts(0)
			, name(0), entry(0), profile(0)
			, num_macro(0), macro(0)
			, num_include(0), include(0)
			, debug_path(0)
			, includer(0)
		{}
	};

	CG*			create();
	void		destroy(CG *cg);

	void		set_output(CG *cg, Outputter *o);
	void		set_errors(CG *cg, Outputter *o);

	item*		compile(CG *cg, int argc, const char **argv, const char *source);
	Symbol*		getsymbol(CG *cg, const char *name);
	void		print(CG *cg, Symbol *sym, int flags);

	item*		compile(CG *cg, const Blob &source, Options &options);
	void 		getbindings(CG *cg, cgclib::item **bindings, SymbolList *list);

} // namespace cgclib
#endif // CGC_LIB_H
