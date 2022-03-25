#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "cgclib.h"
#include "cg.h"
#include "printutils.h"
#include "errors.h"


namespace cgc {
int yylex(YYSTYPE *lvalp, CG *cg);
}

using namespace cgc;
int yyparse(CG *cg);

#include "parser.hpp"

#ifdef _MSC_VER
 #ifdef _M_X64 
  #pragma comment(linker, "/include:generic_reg")
 #else
  #pragma comment(linker, "/include:_generic_reg")
 #endif
#endif

#ifdef __APPLE__
#include "stdlib.cg.c"
#else
extern "C" char stdlib[], stdlib_end[];
#endif

namespace cgc {
void GLSLPrintMainFunction(CG *cg, Symbol *symb, bool pixel);
}

namespace cgclib {

void Outputter::write(const char *fmt, va_list args) {
	char	buffer[1024];
	vput(this, buffer, vsprintf(buffer, fmt, args));
}

void Outputter::write(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	write(fmt, args);
}

item::item(TYPE _type, const char *_name) : type(_type), next(0), name(_name ? strdup(_name) : 0) {
}

item::~item()	{ free((void*)name); }

item *AddItem(item **first, TYPE type, size_t size, const char *name) {
	item *i = new(size) item(type, name);
	memset(i + 1, 0, size - sizeof(item));
	while (item *p = *first)
		first = &p->next;
	*first = i;
	return i;
}

item *AddItemFront(item **first, TYPE type, size_t size, const char *name) {
	item *i = new(size) item(type, name);
	memset(i + 1, 0, size - sizeof(item));
	i->next = *first;
	*first	= i;
	return i;
}

CG *create() {
    CG	*cg = new CG;
	Profile *generic = cg->GetProfile("generic");
	cg->RegisterProfile(generic->CreateHAL, "ps3", 0);
	//cg->RegisterProfile(generic->CreateHAL, "glsl", 0);
	return cg;
}

void destroy(CG *cg) {
	delete cg;
}

void set_output(CG *cg, Outputter *o) {
	cg->list = o;
}

void set_errors(CG *cg, Outputter *o) {
	cg->errors = o;
}

Symbol *getsymbol(CG *cg, const char *name) {
	int atom = cg->AddAtom(name);
	if (!atom)
		return 0;
	return LookUpSymbol(cg, cg->current_scope, atom);
}


void getbindings(CG *cg, item **bindings, SymbolList *list) {
	while (list) {
		Symbol *s = list->p;
		if (s) {
			Binding *b = s->var.bind;
			if (b && ((b->properties & BIND_IS_BOUND) || cg->hal->BindUniformUnbound(cg, s->loc, s, b))) {
				binding	*cgcb = AddItem<binding>(bindings, cg->GetAtomString(s->name));
				cgcb->type	= b->kind;
				switch (b->kind) {
					case BK_REGARRAY: {
						cgcb->size	= b->reg.count;
						cgcb->reg	= b->regno;
						//printf("binding: %s @0x%04x x %i\n", cgcb->item.name, cgcb->reg, cgcb->size);
						break;
					}
					case BK_TEXUNIT: {
						cgcb->size	= 1;
						cgcb->reg	= b->regno;
						//printf("binding: %s @sampler %i x %i\n", cgcb->item.name, cgcb->reg, cgcb->size);
						break;
					}
					case BK_SEMANTIC: {
						cgcb->size	= b->size;
						cgcb->reg	= b->regno;
						//printf("binding: %s @0x%04x x %i\n", cgcb->item.name, cgcb->reg, cgcb->size);
						break;
					}
				}
			}
		}
		list = list->next;
	}
}


void print(CG *cg, Symbol *sym, int flags) {
	if (sym) switch (sym->kind) {
		case FUNCTION_S:
			if (flags)
				GLSLPrintMainFunction(cg, sym, flags > 1);
			else
				PrintFunction(cg, sym);
			break;
		case VARIABLE_S:
		case TYPEDEF_S:
		case CONSTANT_S:
		case TAG_S:
		case MACRO_S:
		case TECHNIQUE_S:
			break;
	}
}

bool get_opts(Options &options, int argc, const char **argv) {
	const char **macro = (const char**)options.macro;

	for (int i = 0; i < argc; i++) {
		if (argv[i][0] == '-') {
			if (!strcmp(argv[i], "-debug")) {
				options.opts |= cgclib::DEBUG_ENABLE;
			} else if (!strcmp(argv[i], "-nowarn")) {
				options.opts |= cgclib::NO_WARNINGS;
			} else if (!strcmp(argv[i], "-parseonly")) {
				options.opts |= cgclib::PARSE_ONLY;
			} else if (!strcmp(argv[i], "-nostdlib")) {
				options.opts |= cgclib::NO_STDLIB;
			} else if (!strcmp(argv[i], "-implicitdown")) {
				options.opts |= cgclib::IMPLICIT_DOWNCASTS;
			} else if (!strcmp(argv[i], "-profile")) {
				i++;
				if (i < argc) {
					if (options.profile) {
//						printf(OPENSL_TAG ": multiple profiles\n");
						return false;
					}
					options.profile = argv[i];
				} else {
//					printf(OPENSL_TAG ": missing profile name after \"-profile\"\n");
					return false;
				}
			} else if (!strcmp(argv[i], "-entry")) {
				i++;
				if (i < argc) {
					if (options.entry) {
//						printf(OPENSL_TAG ": multiple entries\n");
						return false;
					}
					options.entry = argv[i];
				} else {
//					printf(OPENSL_TAG ": missing entry name after \"-entry\"\n");
					return false;
				}
			} else if (argv[i][1] == 'D') {
				*macro++ = argv[i] + 2;
			} else if (!strcmp(argv[i], "-fx")) {
				options.opts |= cgclib::FX;
			} else if (!strcmp(argv[i], "-stages")) {
				options.opts |= cgclib::DUMP_STAGES;
			} else if (!strcmp(argv[i], "-tree")) {
				options.opts |= cgclib::DUMP_PARSETREE;
			} else if (!strcmp(argv[i], "-node")) {
				options.opts |= cgclib::DUMP_NODETREE;
			} else if (!strcmp(argv[i], "-final")) {
				options.opts |= cgclib::DUMP_FINALTREE;
			} else if (!strcmp(argv[i], "-name")) {
				options.name = argv[++i];
			}
		}
	}
	options.num_macro = uint32_t(macro - options.macro);
	return true;
}

item *compile(CG *cg, const Blob &source, Options &options) {
	cg->opts = options.opts;
	for (int i = 0; i < options.num_macro; i++) {
		if (!cg->DefineMacro(options.macro[i])) {
			printf(OPENSL_TAG ": bad macro: \"%s\"\n", options.macro[i]);
			return 0;
		}
	}
	cg->includer	= options.includer;

	if (options.profile == NULL)
		options.profile = "generic";

	if (options.entry == NULL)
		options.entry = "main";

	if (!cg->InitHAL(cg->GetProfile(options.profile)))
		return 0;

	if (options.opts & PREPROCESS_ONLY) {
		ScanFromBlob(cg, source, source.size, options.name ? cg->AddAtom(options.name) : 0);
		YYSTYPE yylval;
		while(int tok = yylex(&yylval, cg)) {
			const char *s = GetTokenString(cg, yylval, tok);
			cg->list->put(s, strlen(s));
			cg->list->putc(' ');
		}
		return 0;
	}

	/*
	char	buffer[64] = "PROFILE_", *t = buffer + strlen(buffer);
	for (const char  *f = options.profile; (isalnum(*f) || *f == '_') && t < buffer + sizeof(buffer) - 1; *t++ = toupper(*f++));
	*t = 0;
	cg->DefineMacro(buffer);
	*/
	// Create the super-global scope and add predefined types and symbols:
	cg->super_scope		= cg->current_scope;
	if (!InitSymbolTable(cg))
		return 0;

	ScanFromBlob(cg, source, source.size, options.name ? cg->AddAtom(options.name) : 0);

	struct StartGlobalInputSrc : InputSrc {
		int		getch(CG *cg) {
			cg->global_scope = NewScopeInPool(mem_CreatePool(0, 0));
			cg->PushScope(cg->global_scope);
			cg->currentInput = prev;
			return '\n';
		}
		void	ungetch(CG *cg, int ch)			{}
		StartGlobalInputSrc() : InputSrc(this)	{}
	} startglobal_inputsrc;

	cg->PushInput(&startglobal_inputsrc);

	if (!(options.opts & NO_STDLIB)) {
#ifdef __APPLE__
//		const section_64	*sect = getsectbyname("__DATA", "__stdlib");
//		ScanFromBlob(cg, (char*)sect->addr, sect->size, cg->AddAtom("<stdlib>"));
		ScanFromBlob(cg, (char*)stdlib, sizeof(stdlib), cg->AddAtom("<stdlib>"));
#else
		ScanFromBlob(cg, stdlib, stdlib_end - stdlib, cg->AddAtom("<stdlib>"));
#endif
	}

	try {
		yyparse(cg);
		if (options.opts & FX) {
			cg->CompileTechniques();
		} else {
			Symbol *prog = LookUpSymbol(cg, cg->current_scope, cg->AddAtom(options.entry));
			if (prog)
				cg->CompileProgram2(prog, cgclib::AddItem<cgclib::shader>(&cg->items, cg->GetAtomString(prog->name)));
			else
				SemanticError(cg, cg->tokenLoc, ERROR___NO_PROGRAM);
		}
	} catch (...) {
		//FatalError(cg, "*** exception during compilation ***");
	}

	return cg->items;
}

item *compile(CG *cg, int argc, const char **argv, const char *source) {
	Options		options;
	const char	*macro[64];
	options.macro = macro;
	if (get_opts(options, argc, argv))
		return compile(cg, Blob(source, strlen(source)), options);
	return 0;
}

}
