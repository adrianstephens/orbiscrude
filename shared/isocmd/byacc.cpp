#include "base/array.h"
#include "base/list.h"
#include "base/algorithm.h"
#include "base/strings.h"
#include "extra/text_stream.h"
#include "extra/expression.h"
#include "extra/unicode_tables.h"
#include "filename.h"

#define YYBACKTRACKING
#define YYLOCATIONS
#define YYDESTRUCTORS
#define YYARGS

#define YYINT		int

namespace yacc {
using namespace iso;

#define PLURAL(n) ((n) > 1 ? "s" : "")

char_set	name1	= char_set::alpha + char_set("_$");
char_set	name2	= char_set::alphanum + char_set("_$");
char_set	ident	= char_set::alphanum + char_set("_.$");

char line_formati[] = "\n#line %0 \"%1\"\n";

int traverse(const bitmatrix_aligned<unsigned> &F, const dynamic_array<int> *R, int *INDEX, int *VERTICES, int i, int top, int infinity) {
	VERTICES[++top] = i;
	int	height	= INDEX[i] = top;

	for (auto j : R[i]) {
		if (INDEX[j] == 0)
			top = traverse(F, R, INDEX, VERTICES, j, top, infinity);

		if (INDEX[i] > INDEX[j])
			INDEX[i] = INDEX[j];

		F[i] += F[j];
	}

	if (INDEX[i] == height) {
		for (;;) {
			int		j	= VERTICES[top--];
			INDEX[j]	= infinity;

			if (i == j)
				break;

			F[j] += F[i];
		}
	}
	return top;
}

void digraph(const bitmatrix_aligned<unsigned> &F, const dynamic_array<int> *relation) {
	int			n	= F.num_rows();
	dynamic_array<int>	INDEX(n + 1, 0);
	dynamic_array<int>	VERTICES(n + 1);

	for (int i = 0; i < n; i++) {
		if (INDEX[i] == 0 && relation[i])
			traverse(F, relation, INDEX, VERTICES, i, 0, n + 2);
	}
}

dynamic_array<dynamic_array<int>> transpose(const dynamic_array<dynamic_array<int>> &r) {
	int					n = r.size32();
	dynamic_array<int>	nedges(n, 0);

	for (auto i : r) {
		for (auto j : i)
			++nedges[j];
	}

	dynamic_array<int*>					temp(n);
	dynamic_array<dynamic_array<int>>	trans(n);
	for (int i = 0; i < n; i++)
		temp[i] = trans[i].resize(nedges[i]);

	for (int i = 0; i < n; i++) {
		for (auto j : r[i])
			*temp[j]++ = i;
	}

	return trans;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------

struct Precedence {
	enum ASSOC {
		NONE	= 0,
		LEFT	= 1,
		RIGHT	= 2,
	};
	int	prec:30; ASSOC assoc:2;

	Precedence(int prec, ASSOC assoc = NONE) : prec(prec), assoc(assoc) {}

	explicit operator bool() const { return assoc != NONE; }
	bool operator!=(const Precedence &a) const { return *(uint32*)this != *(uint32*)&a; }
	int	compare(const Precedence &a) const {
		return a.prec != prec	? a.prec - prec
			: assoc == NONE		? 0
			: assoc == LEFT		? -1 : 1;
	}
};

enum FLAGS {
	FLAG_SUPPRESS_QUOTED= 1 << 0,	// suppress #define's for quoted names in %token lines
	FLAG_LOCS			= 1 << 1,	// enable position processing, e.g., %locations
	FLAG_BACKTRACK		= 1 << 2,	// create a backtracking parser
	FLAG_GLR			= 1 << 3,	// create a GLR parser
	FLAG_PURE			= 3 << 4,	// create a reentrant parser, e.g., %pure-parser
	FLAG_TOKENTABLE		= 1 << 6,
	FLAG_HAS_DESTRUCTOR	= 1 << 7,	// if at least one %destructor
	FLAG_VERBOSE		= 1 << 9,
	FLAG_DEBUG			= 1 << 10,

// from bison spec
	FLAG_CONSTRUCTOR	= 1 << 16,
	FLAG_AUTOMOVE		= 1 << 17,
	FLAG_UNREACHABLE	= 1 << 18,	//allow unreachable parser states to remain in the parser tables.
	FLAG_ASSERTS		= 1 << 19,
	FLAG_TRACE			= 1 << 20,
	FLAG_LAC			= 1 << 21,	//lookahead correction to improve syntax error handling.
	FLAG_PUSHPULL		= 3 << 22,	//2 bits
		FLAG_PUSHPULL_PULL				= 0 * FLAG_PUSHPULL / 3,
		FLAG_PUSHPULL_PUSH				= 1 * FLAG_PUSHPULL / 3,
		FLAG_PUSHPULL_BOTH				= 2 * FLAG_PUSHPULL / 3,
	FLAG_DEFRED			= 3 << 24,	//2 bits
		FLAG_DEFRED_MOST				= 0 * FLAG_DEFRED / 3,
		FLAG_DEFRED_CONSISTENT			= 1 * FLAG_DEFRED / 3,
		FLAG_DEFRED_ACCEPTING			= 2 * FLAG_DEFRED / 3,
	FLAG_LRTYPE			= 3 << 26,	//2 bits
		FLAG_LRTYPE_LALR				= 0 * FLAG_LRTYPE / 3,
		FLAG_LRTYPE_IELR				= 1 * FLAG_LRTYPE / 3,
		FLAG_LRTYPE_CANONICAL			= 2 * FLAG_LRTYPE / 3,
	FLAG_VALUE_TYPE		= 3 << 28,	//2 bits
		FLAG_VALUE_TYPE_UNION_DIRECTIVE	= 0 * FLAG_VALUE_TYPE / 3,
		FLAG_VALUE_TYPE_UNION			= 1 * FLAG_VALUE_TYPE / 3,
		FLAG_VALUE_TYPE_VARIANT			= 2 * FLAG_VALUE_TYPE / 3,
};

struct Context {
	struct rule {
		int			lhs, rhs, line;
		Precedence	prec;
		string		action;
		rule(int lhs, int rhs, int line = 0, Precedence prec = 0, string &&action = 0) : lhs(lhs), rhs(rhs), line(line), prec(prec), action(move(action)) {}
	};

	struct symbol {
		string		name;
		int			value;
		Precedence	prec;
#ifdef YYDESTRUCTORS
		string		destructor;
		const char	*type_tag;
		int			pval;
#endif
		symbol(const char *name, int value, Precedence prec = 0) : name(name), value(value), prec(prec)
#ifdef YYDESTRUCTORS
			, type_tag(0)
#endif
		{}
	};

	struct parameter : e_slink<parameter> {
		string		name, type, type2;
		parameter(string &&name, string &&type, string &&type2) : name(move(name)), type(move(type)), type2(move(type2)) {}
		parameter(const parameter &p) : name(p.name), type(p.type), type2(p.type2) {}
	};

	uint32	flags;
	int		start_symbol;	// also ntokens
	int		final_state;
	int		SRexpect, RRexpect;

	dynamic_array<int>		items;
	dynamic_array<rule>		rules;
	dynamic_array<symbol>	symbols;
	dynamic_array<string>	tags;

	e_slist<parameter>	lex_param, parse_param;

	string	input_filename, output_filename;
	string	include_source, include_footer;
	string	initial_action;
	string	union_def, union_name;
	string	location_file, location_inc, location_type;
	string	parser_class, value_type, prefix, namespce;

	int		nvars()		const	{ return symbols.size32() - start_symbol; }
	int		nrules()	const	{ return rules.size32(); }
	int		nitems()	const	{ return items.size32(); }

	Context() : flags(0), SRexpect(-1), RRexpect(-1) {}
	~Context() {}
};

enum OFFSET_TYPE {
	OFF_TYPE		= 15 << 28,
	OFF_MASK		= ~OFF_TYPE,
	OFF_STRING		= 8 << 28,
	OFF_INT			= 9 << 28,
};

template<typename C> constexpr uint32 get_offset(const char *C::*t) {
	return (uint32)intptr_t(&(((C*)0)->*t)) | OFF_STRING;
}

template<typename C> constexpr uint32 get_offset(string C::*t) {
	return (uint32)intptr_t(&(((C*)0)->*t)) | OFF_STRING;
}

template<typename C> constexpr uint32 get_offset(int C::*t) {
	return (uint32)intptr_t(&(((C*)0)->*t)) | OFF_INT;
}

//-----------------------------------------------------------------------------
//	CLOSURE.C
//-----------------------------------------------------------------------------

struct CLOSURE {
	bitmatrix_aligned_own<unsigned>	first_base;
	dynamic_bitarray<unsigned>		ruleset;
	dynamic_array<int>				itemset;	// for returning range

	CLOSURE(int r, int c, int n) : first_base(r, c), itemset(n) {}
	range<const int*> closure(const Context &ctx, range<const int*> nucleus);
};

range<const int*> CLOSURE::closure(const Context &ctx, range<const int*> nucleus) {
	ruleset.clear_all();

	for (auto i : nucleus) {
		int	symbol = ctx.items[i] - ctx.start_symbol;
		if (symbol >= 0)
			ruleset += first_base[symbol];
	}

	int		*put	= itemset;
	auto	p		= nucleus.begin(), e = nucleus.end();

	for (auto i : ruleset.where(true)) {
		int itemno = ctx.rules[i].rhs;
		while (p < e && *p < itemno)
			*put++ = *p++;
		*put++ = itemno;
		while (p < e && *p == itemno)
			++p;
	}

	while (p < e)
		*put++ = *p++;

	return {itemset, put};
}

//-----------------------------------------------------------------------------
//	LR0
//-----------------------------------------------------------------------------

struct LR0 {
private:
	dynamic_array<int>			rules;
	dynamic_array<const int*>	derive_starts;
public:
	struct actions : e_slink<actions> {
		int			index;	// which state this belongs to
		int			size;
		int			value[1];
		void *operator new(size_t size, int n) {
			return ::operator new(size + (n - 1) * sizeof(int));
		}
		actions(int index, range<const int*> vals) : index(index), size(vals.size32()) {
			copy(vals, value);
		}
		const int	*begin()	const { return value; }
		const int	*end()		const { return value + size; }
	};

	struct state : e_slink<state> {
		state*		link;
		int			index;
		int			accessing_symbol;
		int			nitems;
		int			data[1];	//indices into ctx.items

		void *operator new(size_t size, int n) {
			return ::operator new(size + (n - 1) * sizeof(int));
		}
		state(int index, int accessing_symbol, range<const int*> init_items) : link(0), index(index), accessing_symbol(accessing_symbol), nitems(init_items.size32()) {
			copy(init_items, data);
		}
		range<const int*> items() const {
			return {data, data + nitems};
		}
		bool matches(range<const int*> b) const {
			if (nitems != b.size32())
				return false;

			auto	j = &data[0];
			for (auto i : b) {
				if (i != *j++)
					return false;
			}
			return true;
		}
	};

	int						nstates;
	e_slist_tail<state>		states;
	e_slist_tail<actions>	shifts, reductions;
	dynamic_array<state*>	state_hash_table;
	dynamic_array<bool>		nullable;
	CLOSURE					closure;

	auto	derives(int symbol)			const {
		return make_range(derive_starts[symbol], derive_starts[symbol + 1]);
	}

	int		get_state(int symbol, range<int*> kernel) {
		state	**spp = state_hash_table + kernel.front();
		while (state *sp = *spp) {
			if (sp->matches(kernel))
				return sp->index;
			spp = &sp->link;
		}
		state	*sp = *spp = new(kernel.size32()) state(nstates++, symbol, kernel);
		states.push_back(sp);
		return sp->index;
	}

	void	add_state(const Context &ctx, range<int*> *kernels, int *temp, int index, range<const int*> itemset);

	LR0(Context &ctx);
	~LR0() {
		states.deleteall();
		shifts.deleteall();
		reductions.deleteall();
	}
};

void LR0::add_state(const Context &ctx, range<int*> *kernels, int *temp, int index, range<const int*> itemset) {
	// collect reductions
	int	n = 0;
	for (auto i : make_indexed_container(ctx.items, itemset)) {
		if (i < 0)
			temp[n++] = -i;
	}

	if (n)
		reductions.push_back(new(n) actions(index, make_range_n(temp, n)));

	// collect shifts
	n = 0;
	for (auto i : itemset) {
		int symbol = ctx.items[i];
		if (symbol > 0) {
			if (kernels[symbol].empty())
				temp[n++] = symbol;
			kernels[symbol].push_back(i + 1);
		}
	}

	if (n) {
		auto	s = make_range_n(temp, n);
		insertion_sort(s);
		for (auto &i : s)
			i = get_state(i, kernels[i]);
		shifts.push_back(new(n) actions(index, s));
	}
}

LR0::LR0(Context &ctx)
	: rules(ctx.rules.size())
	, derive_starts(ctx.symbols.size32() + 1)
	, nstates(1)
	, state_hash_table(ctx.items.size())
	, nullable(ctx.symbols.size32(), false)
	, closure(ctx.nvars(), ctx.nrules(), ctx.nitems())
{
	int	start_symbol	= ctx.start_symbol;
	int nsyms			= ctx.symbols.size32();
	int	nrules			= ctx.rules.size32();
	int	nitems			= ctx.items.size32();
	int	nvars			= nsyms - start_symbol;

	//set_derives
	int	k = 0;
	for (int lhs = start_symbol; lhs < nsyms; lhs++) {
		derive_starts[lhs] = rules + k;
		for (int i = 0; i < nrules; i++) {
			if (ctx.rules[i].lhs == lhs)
				rules[k++] = i;
		}
	}
	derive_starts[nsyms] = rules + k;

	//set_nullable
	for (bool done = false; !done;) {
		done = true;
		for (int i = 1; i < nitems; i++) {
			int		j;
			bool	empty = true;
			while ((j = ctx.items[i]) >= 0) {
				if (!nullable[j])
					empty = false;
				++i;
			}
			if (empty) {
				j = ctx.rules[-j].lhs;
				if (!nullable[j]) {
					nullable[j] = true;
					done = false;
				}
			}
		}
	}

	dynamic_array<int>			temp(max(nsyms, nrules + 1), 0);
	int	count = 0;
	for (auto i : ctx.items) {
		if (i >= 0) {
			++count;
			++temp[i];
		}
	}

	dynamic_array<int>			kernel_items(count);
	dynamic_array<range<int*>>	kernel(ctx.symbols.size32());
	count = 0;
	for (int i = 0; i < ctx.symbols.size32(); i++) {
		kernel[i]	= make_range_n(kernel_items + count, 0);
		count		+= temp[i];
	}

	//initialize_states
	auto	k0	= kernel_items.begin(), k1 = k0;
	for (auto i : derives(start_symbol))
		*k1++ = ctx.rules[i].rhs;

	states.push_back(new(k1 - k0) state(0, 0, make_range(k0, k1)));

	// init closure
	bitmatrix_aligned_own<unsigned> EFF(nvars, nvars);
	for (int i = 0; i < nvars; ++i) {
		auto	row = EFF[i];
		for (auto j : derives(i + start_symbol)) {
			int sym = ctx.items[ctx.rules[j].rhs];
			if (sym >= start_symbol)
				row.set(sym - start_symbol);
		}
	}
	EFF.reflexive_transitive_closure();

	for (int i = 0; i < nvars; i++) {
		auto	row = closure.first_base[i];
		for (auto j : EFF[i].where(true)) {
			for (auto rule : derives(j + start_symbol))
				row.set(rule);
		}
	}

	for (auto s = states.begin(); s != states.end(); ++s) {
		// reset all kernels
		for (auto &i : kernel)
			i.trim_length(0);
		add_state(ctx, kernel, temp, s->index, closure.closure(ctx, s->items()));
	}
}

//-----------------------------------------------------------------------------
//	LALR
//-----------------------------------------------------------------------------

struct LALR {
	struct state {
		int				accessing_symbol;
		LR0::state		*state;
		LR0::actions	*shifts, *reductions;
		int				lookahead_offset;	//offset where reductions start in LA
	};
	dynamic_array<state>			states;
	bitmatrix_aligned_own<unsigned>	LA;
	dynamic_array<int>				goto_map;
	dynamic_array<int>				default_goto;
	dynamic_array<pair<int, int>>	from_to_state;

	//  maps a state/symbol pair into its numeric representation (var = symbol - start_symbol)
	auto	gotos(int var) const {
		return slice(from_to_state, goto_map[var], goto_map[var + 1] - goto_map[var]);
	}
	auto	map_goto(int state, int var) {
		return from_to_state.index_of(lower_boundc(gotos(var), state, [](const pair<int,int> &from_to, int state) { return from_to.a < state;} ));
	}

	LALR(Context &ctx, LR0 &lr0);

	int	get_final_state(int goal) const {
		for (auto s : *states[0].shifts) {
			if (states[s].accessing_symbol == goal)
				return s;
		}
		return -1;
	}

	string_accum& state_label(string_accum &a, Context &ctx, unsigned lookahead_offset, const int *reds, range<const int*> itemsset) const;
	string_accum& print_graph(string_accum &a, Context &ctx, CLOSURE &c) const;
};

LALR::LALR(Context &ctx, LR0 &lr0) : states(lr0.nstates) {
	int	nstates		= lr0.nstates;
	int nsyms		= ctx.symbols.size32();
	int	ntokens		= ctx.start_symbol;
	int	nvars		= nsyms - ntokens;

	goto_map.resize(nvars + 1, 0);
	default_goto.resize(nvars);

	for (auto &p : lr0.states) {
		states[p.index].state				= &p;
		states[p.index].accessing_symbol	= p.accessing_symbol;
	}

	//set shifts and goto_map
	int	ngotos = 0;
	for (auto &p : lr0.shifts) {
		states[p.index].shifts	= &p;
		for (int i = p.size - 1; i >= 0; i--) {
			int	symbol = states[p.value[i]].accessing_symbol;
			if (symbol < ntokens)
				break;

			++ngotos;
			++goto_map[symbol - ntokens];
		}
	}

	//set reductions
	int nreds = 0;
	for (auto &p : lr0.reductions) {
		states[p.index].reductions = &p;
		nreds	+= p.size;
	}

	//set offsets into LA
	LA.init(nreds, ntokens);
	int	k = 0;
	for (auto &e : states) {
		e.lookahead_offset = k;
		if (auto *p = e.reductions)
			k += p->size;
	}

	{
		dynamic_array<int> temp_map(nvars + 1);

		int	k = 0;
		for (int i = 0; i < nvars; i++) {
			temp_map[i] = k;
			k += exchange(goto_map[i], k);
		}

		goto_map[nvars] = ngotos;
		temp_map[nvars] = ngotos;

		from_to_state.resize(ngotos);

		for (auto &sp : lr0.shifts) {
			int state1 = sp.index;
			for (int i = sp.size - 1; i >= 0; i--) {
				int	state2 = sp.value[i];
				int	symbol = states[state2].accessing_symbol;
				if (symbol < ntokens)
					break;

				k = temp_map[symbol - ntokens]++;
				from_to_state[k]	= {state1, state2};
			}
		}

		//default goto
		dynamic_array<int>	state_count(nstates);
		for (int i = 1; i < nvars; i++) {
			int		default_state = 0;
			int		m	= goto_map[i];
			int		n	= goto_map[i + 1];

			if (m != n) {
				for (auto &i : state_count)
					i = 0;

				for (int i = m; i < n; i++)
					state_count[from_to_state[i].b]++;

				int max = 0;
				for (int i = 0; i < nstates; i++) {
					if (state_count[i] > max) {
						max = state_count[i];
						default_state = i;
					}
				}
			}
			default_goto[i] = default_state;
		}
	}

	//initialize_F
	bitmatrix_aligned_own<unsigned> F(ngotos, ntokens);

	{
		dynamic_array<dynamic_array<int>>	reads(ngotos);
		for (int i = 0; i < ngotos; i++) {
			auto	rowp	= F[i];
			int		stateno = from_to_state[i].b;
			if (auto *sp = states[stateno].shifts) {
				for (auto j : *sp) {
					int	symbol = states[j].accessing_symbol;
					if (symbol < ntokens)
						rowp.set(symbol);
					else if (lr0.nullable[symbol])
						reads[i].push_back(map_goto(stateno, symbol - ntokens));
				}
			}

		}

		F[0].set(0);
		digraph(F, reads);
	}

	//build_relations
	{
		dynamic_array<dynamic_array<int>>	includes(ngotos);
		dynamic_array<slist<int>>			lookback(nreds);
		dynamic_array<int>					statenos;

		for (int i = 0; i < ngotos; i++) {
			int		state1	= from_to_state[i].a;
			int		symbol1	= states[from_to_state[i].b].accessing_symbol;

			for (auto ruleno : lr0.derives(symbol1)) {
				statenos.clear();
				int		stateno = state1;
				int		*rp;
				for (rp = ctx.items + ctx.rules[ruleno].rhs; *rp >= 0; rp++) {
					statenos.push_back(stateno);
					int		symbol2 = *rp;
					for (auto j : *states[stateno].shifts) {
						stateno = j;
						if (states[stateno].accessing_symbol == symbol2)
							break;
					}
				}

				if (auto reds = states[stateno].reductions) {
					auto	lookahead_offset	= states[stateno].lookahead_offset;
					bool	found				= false;
					for (auto &j : *reds) {
						if (found = (j == ruleno)) {
							lookback[&j - reds->value + lookahead_offset].push_front(i);
							break;
						}
					}
					ISO_ASSERT(found);
				}

				while (!statenos.empty()) {
					int	symbol	= *--rp;
					if (symbol < ntokens)
						break;

					stateno		= statenos.pop_back_value();
					includes[i].push_back(map_goto(stateno, symbol - ntokens));
					if (!lr0.nullable[symbol])
						break;
				}
			}
		}
		//compute_FOLLOWS
		digraph(F, transpose(includes));

		// process lookbacks
		int	row = 0;
		for (auto &i : lookback) {
			auto	rowp = LA[row];
			for (auto j : i)
				rowp += F[j];
			++row;
		}
	}
	ctx.final_state = get_final_state(ctx.items[1]);
}

void graph_LA(string_accum &a, const range<bit_pointer<unsigned>> &rowp, const Context::symbol* symbols, int ntokens) {
	a << " { ";
	for (int i = ntokens - 1; i >= 0; i--) {
		if (rowp[i])
			a << symbols[i].name << ' ';
	}
	a << '}';
}

string_accum &LALR::state_label(string_accum &a, Context &ctx, unsigned lookahead_offset, const int *reds, range<const int*> itemsset) const {
	return a;
	auto	*r = reds;
	for (auto s : itemsset) {
		int *sp = ctx.items + s, *sp1 = sp;
		while (*sp >= 0)
			++sp;

		auto&	rule = ctx.rules[-*sp];
		a << "  " << ctx.symbols[rule.lhs].name << ' ';
		for (sp = ctx.items + rule.rhs; sp < sp1; sp++)
			a << ctx.symbols[*sp].name << ' ';

		a << '.';
		while (*sp >= 0)
			a << ' ' << ctx.symbols[*sp++].name;

		if (*sp1 < 0) {
			if (-*sp1 == *r) {
				graph_LA(a, LA[r - reds + lookahead_offset], ctx.symbols, ctx.start_symbol);
				++r;
			}
		}

		a << "\\l";
	}
	return a;
}


string_accum &LALR::print_graph(string_accum &a, Context &ctx, CLOSURE &c) const {

	for (auto &e : states) {
		int	i = states.index_of(e);
		state_label(a << "\n\tq" << i << " [label=\"" << i << ":\\l",
			ctx, e.lookahead_offset, e.reductions->begin(),
			c.closure(ctx, e.state->items())
		) << "\"];";
	}

	a << "\n\n";
	for (auto &e : states) {
		if (auto sp = e.shifts) {
			int	stateno = states.index_of(e);
			for (auto sn : *sp) {
				int	as = states[sn].accessing_symbol;
				a << "\tq" << stateno << " -> q" << sn << " [label=\"" << ctx.symbols[as].name << "\"];\n";
			}
		}
	}
	return a;
}

//-----------------------------------------------------------------------------
//	PARSER
//-----------------------------------------------------------------------------

struct PARSER {
	struct action : e_slink<action> {
		enum CODE : uint8 {SHIFT = 1, REDUCE = 2};
		int			symbol;
		int			index;
		Precedence	prec;
		CODE		code;
		uint8		suppressed;

		action(int symbol, int index, Precedence prec, CODE code)
			: symbol(symbol), index(index), prec(prec), code(code), suppressed(0)
		{}
		bool is_shift() const { return code == SHIFT; }
	};

	struct state {
		e_slist<action>	actions;
		int		defred;
		int		SRconflicts;
		int		RRconflicts;

		state() : SRconflicts(0), RRconflicts(0) {}
		~state() { actions.deleteall(); }

		void add_shifts(LR0::actions *sp, const LALR &lalr, Context::symbol *symbols, int start_symbol) {
			for (int i = sp->size - 1; i >= 0; i--) {
				int k		= sp->value[i];
				int symbol	= lalr.states[k].accessing_symbol;
				if (symbol < start_symbol)
					actions.push_front(new action(symbol, k, symbols[symbol].prec, action::SHIFT));
			}
		}
		void add_reductions(LR0::actions *rp, int lookahead_offset, const LALR &lalr, Context::rule *rules, int ntokens) {
			for (int i = 0; i < rp->size; ++i) {
				int		ruleno	= rp->value[i];
				auto	rowp	= lalr.LA[i + lookahead_offset];
				for (int symbol = ntokens - 1; symbol >= 0; symbol--) {
					if (rowp[symbol]) {
						auto	a = lower_bound(actions.beginp(), actions.end(), symbol, [ruleno](const action &i, int symbol) {
							return	i.symbol != symbol ? i.symbol < symbol : i.is_shift() || i.index < ruleno;
						});
						actions.insert(a, new action(symbol, ruleno, rules[ruleno].prec, action::REDUCE));
					}
				}
			}
		}
		int	sole_reduction(bool backtrack) const {
			int	count	= 0;
			int	ruleno	= 0;
			for (auto &p : actions) {
				if (p.suppressed <= int(backtrack)) {
					if (p.is_shift()) {
						return 0;

					} else {
						if (ruleno && p.index != ruleno)
							return 0;

						ruleno = p.index;
						if (p.symbol != 1)
							++count;
					}
				}
			}
			return count == 0 ? 0 : ruleno;
		}
		void	remove_conflicts(bool final, bool backtrack);
	};

	dynamic_array<state>	states;
	int						SRtotal, RRtotal;

	PARSER(Context &ctx, LR0 &lr0, LALR &lalr);

	bool	has_conflicts() const { return SRtotal + RRtotal > 0; }
	void	print_conflicts(int SRexpect, int RRexpect);
};

void PARSER::state::remove_conflicts(bool final, bool backtrack) {
	int		symbol	= -1;
	action*	prefer	= 0;

	for (auto &p : actions) {
		if (p.symbol != symbol) {
			// the first parse action for each symbol is the preferred action
			prefer	= &p;
			symbol	= p.symbol;

		// following conditions handle multiple, i.e., conflicting, parse actions
		} else if (final && symbol == 0) {
			SRconflicts++;
			p.suppressed = 1;
			if (backtrack && prefer && !prefer->suppressed)
				prefer->suppressed = 1;

		} else if (prefer && prefer->is_shift()) {
			if (prefer->prec && p.prec) {
				int	compare = prefer->prec.compare(p.prec);
				if (compare >= 0)
					p.suppressed = 2;
				if (compare <= 0) {
					prefer->suppressed = 2;
					if (compare < 0)
						prefer = &p;
				}

			} else {
				SRconflicts++;
				p.suppressed = 1;
				if (backtrack && prefer && !prefer->suppressed)
					prefer->suppressed = 1;
			}
		} else {
			RRconflicts++;
			p.suppressed = 1;
			if (backtrack && prefer && !prefer->suppressed)
				prefer->suppressed = 1;
		}
	}
}

PARSER::PARSER(Context &ctx, LR0 &lr0, LALR &lalr)
	: states(lr0.nstates)
	, SRtotal(0), RRtotal(0)
{
	bool	backtrack	= !!(ctx.flags & (FLAG_BACKTRACK | FLAG_GLR));

	for (auto &i : states) {
		auto	&e	= lalr.states[states.index_of(i)];

		if (auto *sp = e.shifts)
			i.add_shifts(sp, lalr, ctx.symbols, ctx.start_symbol);

		if (auto *rp = e.reductions)
			i.add_reductions(rp, e.lookahead_offset, lalr, ctx.rules, ctx.start_symbol);

		//remove_conflicts
		i.remove_conflicts(&i == &states[ctx.final_state], backtrack);
		SRtotal	+= i.SRconflicts;
		RRtotal	+= i.RRconflicts;

		//defreds
		i.defred = i.sole_reduction(backtrack);
	}
}

void PARSER::print_conflicts(int SRexpect, int RRexpect) {
	fprintf(stderr, "%d shift/reduce conflict%s, %d reduce/reduce conflicts%s.\n", SRtotal, PLURAL(SRtotal), RRtotal, PLURAL(RRtotal));

	if (SRexpect >= 0 && SRtotal != SRexpect) {
		fprintf(stderr, "expected %d shift/reduce conflict%s.\n", SRexpect, PLURAL(SRexpect));
		//exit_code = EXIT_FAILURE;
	}
	if (RRexpect >= 0 && RRtotal != RRexpect) {
		fprintf(stderr, "expected %d reduce/reduce conflict%s.\n", RRexpect, PLURAL(RRexpect));
		//exit_code = EXIT_FAILURE;
	}
}

//-----------------------------------------------------------------------------
//	LINEREADER
//-----------------------------------------------------------------------------

enum SEVERITY {
	SEV_WARNING = 0,
	SEV_ERROR	= 1,
	SEV_FATAL	= 2,
};

struct LINEREADER : string_scan {
	istream_ref				input;
	int						lineno;
	string					input_filename;
	string					line;
//	string_scan				ss;
	int						last_char;

	//errors

	typedef tagged<int,int>	col_t;
	struct source_pos {
		int			lineno, column;
		string		line;
		source_pos(int lineno) : lineno(lineno) {}
		source_pos(int lineno, const char *line, int column) : lineno(lineno), column(column), line(line) {}
		source_pos(int lineno, const char *line, const char *ptr) : source_pos(lineno, line, int(ptr - line)) {}
	};

	source_pos	current_pos() const { return source_pos(lineno, line, getp()); }
	col_t		current_col() const { return col_t(int(getp() - line.begin())); }

	void _error(SEVERITY severity, int lineno, const char *line, int column, const char *msg) {
		static const char sevs[] = "wef";
		if (lineno)
			fprintf(stderr, "%c - line %d of \"%s\" - %s\n", sevs[severity], lineno, (char*)input_filename, msg);
		else
			fprintf(stderr, "%c - %s\n", sevs[severity], msg);

		if (line) {
			for (const char *s = line; *s; ++s)
				putc(is_print(*s) || *s == '\t' ? *s : '?', stderr);
			putc('\n', stderr);
			for (const char *s = line; s < line + column; ++s)
				putc(*s == '\t' ? '\t' : ' ', stderr);
			putc('^', stderr);
			putc('\n', stderr);
		}
		if (severity)
			exit(severity);
	}

	void error(SEVERITY severity, const source_pos &pos, const char *msg,...) {
		va_list args;
		va_start(args, msg);
		char	temp[256];
		vsprintf(temp, msg, args);
		_error(severity, pos.lineno, pos.line, pos.column, temp);
	}
	void error(SEVERITY severity, const char *msg,...) {
		va_list args;
		va_start(args, msg);
		char	temp[256];
		vsprintf(temp, msg, args);
		_error(severity, lineno, line, current_col(), temp);
	}
	void error(SEVERITY severity, int lineno, const char *msg,...) {
		va_list args;
		va_start(args, msg);
		char	temp[256];
		vsprintf(temp, msg, args);
		_error(severity, lineno, 0, 0, temp);
	}
	void error(SEVERITY severity, col_t column, const char *msg,...) {
		va_list args;
		va_start(args, msg);
		char	temp[256];
		vsprintf(temp, msg, args);
		_error(severity, lineno, line, column, temp);
	}

	void expect(int got, char c) {
		if (got != c)
			error(SEV_ERROR, "expected '%c'", c);
	}

	bool get_line() {
		for (;;) {
			int	c = input.getc();
			if (c == '\r') {
				c = input.getc();
				if (c != '\n')
					++lineno;
			} else if (c == '\n') {
				if (last_char != '\r')
					++lineno;
				c = input.getc();
			}
			if (c == -1)
				return false;

			for (auto b = build(line, true); c && c != -1 && c != '\n' && c != '\r'; c = input.getc())
				b.putc(c);

			last_char	= c;
			++lineno;

			*(string_scan*)this = line;
			if (skip_whitespace().check('#')
				&&	skip_whitespace().check("line")
				&&	skip_whitespace().get(lineno)
				) {
				if (skip_whitespace().scan_skip('"')) {
					auto	name = get_token(~char_set('"'));
					if (getc() == '"')
						input_filename = name;
				}
				continue;
			}

			*(string_scan*)this = line;
			return true;
		}
	}

	LINEREADER(istream_ref file, const char *input_filename) : string_scan(0), input(file), lineno(0), input_filename(input_filename) {
	}
};

//-----------------------------------------------------------------------------
//	READER
//-----------------------------------------------------------------------------

#ifdef YYARGS

struct arg {
	string	name;
	const char *tag;
};

// compare two strings, ignoring whitespace, except between two letters or digits (and treat all of these as equal)
int strnscmp(const char *a, const char *b) {
	while (1) {
		while (is_whitespace(*a))
			a++;
		while (is_whitespace(*b))
			b++;
		while (*a && *a == *b)
			a++, b++;
		if (is_whitespace(*a)) {
			if (is_alphanum(a[-1]) && is_alphanum(*b))
				break;
		} else if (is_whitespace(*b)) {
			if (is_alphanum(b[-1]) && is_alphanum(*a))
				break;
		} else
			break;
	}
	return *a - *b;
}

#endif

struct CharSet : public dynamic_array<interval<char32>> {
	bool add(interval<char32> r) {
		if (r.empty())
			return false;

		auto i = lower_boundc(*this, r.a);
		if (i != end() && i->contains(r))
			return false;

		if (r.b >= i->a) {
			i->a = r.a;
			if (i != begin() && r.a < i[-1].b) {
				i[-1].b = i[0].b;
				erase(i);
			}
		} else {
			insert(i, r);
		}
		return true;
	}
	template<typename C> bool add(interval<C> r) {
		return add(interval<char32>(r));
	}
	bool add(char32 c) {
		auto i = lower_boundc(*this, c);
		if (i != end() && i->contains(c))
			return false;

		insert(i, {c, c});
		return true;
	}
	void negate() {
		char32 nexta = 0;
		for (auto &i : *this)
			nexta = exchange(i.b, exchange(i.a, nexta) - 1) + 1;

		if (front().b == -1)
			erase(begin());
		if (nexta <= unicode::Runemax)
			emplace_back(nexta, (char32)unicode::Runemax);
	}
};


struct READER : LINEREADER {
	using LINEREADER::expect;

	struct SYMBOLS;

	struct bucket : e_slink<bucket> {
		friend SYMBOLS;
		enum {UNDEFINED = -1};
		enum CLASS : uint8 {
			UNKNOWN		= 0,
			TERM		= 1,
			NONTERM		= 2,
			ACTION		= 3,
			ARGUMENT	= 4,
		};

		bucket			*link;
		string			name;
		const char		*tag;
		int				value;	// used in pack, and explicitly set tokens
		int				index;	// used in pack
		Precedence		prec;
		CLASS			clss;
		CharSet			charset;
#ifdef YYARGS
		dynamic_array<arg>	args;
#endif
#ifdef YYDESTRUCTORS
		string				destructor;
#endif

		bool undefined() const { return value == UNDEFINED; }
	private:
		bucket(string &&name, const char *tag, CLASS clss)
			: link(0), name(iso::move(name)), tag(tag)
			, value(UNDEFINED), index(0), prec(0), clss(clss)
		{}
	};

	struct SYMBOLS : e_slist_tail<bucket> {
		enum {TABLE_SIZE = 1024};
		bucket *symbol_table[TABLE_SIZE];
		int		gensym;

		static unsigned hash(const count_string &s) {
			unsigned	h = 0;
			for (auto c : s)
				h = h * 31 + c;
			return h & (TABLE_SIZE - 1);
		}
		SYMBOLS() : gensym(0) {
			iso::clear(symbol_table);
			push_back(symbol_table[hash("error")] = new bucket("error", 0, bucket::TERM));
		}
		~SYMBOLS() {
			deleteall();
		}
		bucket *newbucket(const count_string &name, const char *tag = 0, bucket::CLASS clss = bucket::UNKNOWN) {
			return push_back(new bucket(name, tag, clss));
		}
		bucket *genbucket(const char *tag = 0, bucket::CLASS clss = bucket::UNKNOWN) {
			return push_back(new bucket(format_string("$$%d", ++gensym), tag, clss));
		}

		bucket *lookup(const count_string &name) {
			bucket **bpp = symbol_table + hash(name);
			while (bucket *bp = *bpp) {
				if (name == bp->name)
					return bp;
				bpp = &bp->link;
			}
			return *bpp = newbucket(name);
		}
	};

	struct rule {
		bucket		*lhs;
		dynamic_array<bucket*>	rhs;
		string		action;
		Precedence	prec;
		int			line;
		rule(bucket *lhs = 0, int line = 0) : lhs(lhs), prec(0), line(line) {}
		rule(bucket *lhs, string &&action) : lhs(lhs), action(iso::move(action)), prec(0), line(0) {}

#ifdef YYARGS
		constexpr arg*		get_arg(int i)	const { return between(i + rhs.size32(), 1, lhs->args.size32()) ? lhs->args.end() + (i - 1 + rhs.size()) : 0; }
#endif
		constexpr bucket*	get_item(int i)	const { return between(i, 1 - int(rhs.size32()), 0) ? rhs.end()[i - 1] : 0; }

		const char *get_tag(int i) const {
#ifdef YYARGS
			if (auto arg = get_arg(i))
				return arg->tag;
#endif
			return get_item(i) ? get_item(i)->tag : 0;
		}
		const char *get_name(int i) const {
			return get_item(i) ? get_item(i)->name : 0;
		}
		int	get_offset(READER *reader, int val) const {
			if (val <= 0)
				return val - int(rhs.size32());

			for (auto &i : rhs) {
				if (i->clss != bucket::ARGUMENT && --val == 0)
					return &i - rhs.end() + 1;
			}
			reader->error(SEV_WARNING, "@%d references beyond the end of the current rule\n", val);
			return val;
		}
#ifdef YYARGS
		int	get_offset(READER *reader, const char *id) const {
			for (auto &i : lhs->args) {
				if (i.name == id)
					return &i - lhs->args.end() - int(rhs.size32()) + 1;
			}
			reader->error(SEV_ERROR, "unknown argument $%s", id);
			unreachable();
		}
#endif
	};

	bucket					*goal;
	bool					trial_rules;

	dynamic_array<string>	tags;
	SYMBOLS					symbols;
	dynamic_array<rule*>	rules;

	void unexpected_EOF()			{ error(SEV_ERROR, lineno, "unexpected end-of-file"); }
	void syntax_error(int column)	{ error(SEV_ERROR, column, "syntax error"); }
	void syntax_error()				{ error(SEV_ERROR, "syntax error"); }

	const char *cache_tag(const count_string &tag) {
		for (auto &i : tags) {
			if (i == tag)
				return i;
		}
		return tags.push_back(tag);
	}
	const char *get_id() {
		return cache_tag(get_token(ident));
	}
	const char *get_tag() {
		auto	col	= current_col();
		auto	tag = get_token(name1, name2);
		if (!check('>'))
			error(SEV_ERROR, col, "illegal tag");
		return cache_tag(tag);
	}
	const char *maybe_tag() {
		return check('<') ? get_tag() : 0;
	}

	bucket *get_name() {
		auto	name = get_token(ident);
		if (name == "." || name == "$accept" || name == "$end" || (name[0] == '$' && name[1] == '$' && name.slice(2).check(char_set::digit)))
			error(SEV_ERROR, "illegal use of reserved symbol %s", (const char*)string(name));
		return symbols.lookup(name);
	}

	bucket*	get_literal() {
		source_pos		a	= current_pos();
		string_builder	b;

		int	quote = getc();
		b.putc(quote);

		int	c;
		while ((c = getc()) != quote) {
			if (c == 0)
				error(SEV_ERROR, a, "unterminated string");
			if (c == '\\')
				c = unescape1(*this);
			b.putc(c);
		}
		b.putc(quote);

		bucket *bp = symbols.lookup(b);
		bp->clss = bucket::TERM;
		if (bp->name.length() == 3 && bp->undefined())
			bp->value = bp->name[1];

		return bp;
	}

	bucket*	get_name_or_literal() {
		int	c = nextc();
		return name1.test(c) ? get_name() : c == '\'' || c == '"' ? get_literal() : 0;
	}

	void	set_start(bucket *bp) {
		if (!bp)
			syntax_error();

		if (bp->clss == bucket::TERM)
			error(SEV_ERROR, "the start symbol %s is a token", bp->name.begin());

		if (goal && goal != bp)
			error(SEV_WARNING, "the start symbol has been redeclared");

		goal = bp;
	}

	void	add_sub_action(rule *r, string &&action) {
		bucket *bp = symbols.genbucket(r->lhs->tag, bucket::ACTION);
		r->rhs.push_back(bp);
		rules.push_back(new rule(bp, iso::move(action)));
	}

	bucket *decorators(bucket *bp) {
		if (int c = check(char_set("+*?"))) {
			bucket	*bp2	= symbols.newbucket(count_string(format_string("%s%c", bp->name.begin(), c)), bp->tag, bucket::NONTERM);
			rule	*r1		= rules.push_back(new rule(bp2));
			rule	*r2		= rules.push_back(new rule(bp2));
			if (c == '+')
				r1->rhs.push_back(bp);
			r2->rhs.push_back(bp);
			if (c != '?')
				r2->rhs.push_back(bp2);
			return bp2;
		}
		return	bp;
	}

#ifdef YYDESTRUCTORS
	struct destructor : e_slink<destructor> {
		const char	*tag;
		string		code;
		destructor(const char *tag, string&& code) : tag(tag), code(iso::move(code)) {}
		bool operator==(const char *tag2) const { return tag == tag2; }
	};
	e_slist<destructor> destructors;

	void			declare_destructor();
	string			finalise_destructor(const char *code, const char *tag);
#endif

#ifdef YYARGS
	struct ARGCACHE {
		enum {TABLE_SIZE = 1024};
		struct line {
			line		*next;
			const rule	*r;
			line(const rule *r) : r(r) {}
		} *lines[TABLE_SIZE];

		static unsigned hash(const char *s) {
			unsigned h = 0;
			while (int c = *s++) {
				if (!is_whitespace(c))
					h = h * 31 + c;
			}
			return h & (TABLE_SIZE - 1);
		}
		ARGCACHE() { clear(lines); }
		~ARGCACHE() {
			for (auto &i : lines) {
				for (line *e = exchange(i, nullptr), *t; e; e = t) {
					t = e->next;
					delete e;
				}
			}
		}
		const rule *lookup(char *code) {
			for (auto entry = lines[hash(code)]; entry; entry = entry->next) {
				if (!strnscmp(entry->r->action, code))
					return entry->r;
			}
			return 0;
		}
		void insert(const rule *r) {
			auto *entry = new line(r);
			entry->next	= exchange(lines[hash(r->action)], entry);
		}
	} arg_cache;

	void			get_args(bucket *bp, rule *r);
	void			get_formal_args(bucket *bp);
#endif

	int				next_inline();
	int				nextc();
	void			expect(char c) { expect(nextc(), c); move(1); }

	int				keyword();
	void			process_define(Context &ctx);
	void			declare_tokens(Precedence prec);
	void			declare_types();
	string_accum&	copy_code(string_accum &b, rule *r, bool trial_or_loc);
	string_accum&	copy_code(string_accum &&b, rule *r, bool trial_or_loc) { return copy_code(b, r, trial_or_loc); }
	void			copy_string(string_accum &b, int quote);
	void			copy_string(string_accum &&b, int quote)				{ return copy_string(b, quote); }
	void			copy_comment(string_accum &b);
	void			copy_text(string_accum &b);
	void			copy_text(string_accum &&b)								{ return copy_text(b); }
	e_slist<Context::parameter>	copy_param();
	bucket*			parse_charset();
	void			parse_rule1(rule *r);
	void			parse_rule(rule *r);

	void			pack(Context &ctx);

	READER(Context &ctx, istream_ref file, const char *input_filename);
};

auto maybe_put_tag(const char *tag) {
	return [tag](string_accum &b) { if (tag) b << '.' << tag; return b; };
}

int READER::next_inline() {
	for (;;) {
		int c = peekc();
		if (c == '/') {
			if (peekc(1) == '*') {
				source_pos	a = current_pos();
				while (!scan_skip("*/")) {
					if (!get_line())
						error(SEV_ERROR, a, "unmatched /*");
				}
				continue;

			} else if (peekc(1) == '/') {
				return 0;
			}
		}
		return c;
	}
}

int READER::nextc() {
	for (;;) {
		switch (int ch = next_inline()) {
			case 0:
				if (!get_line())
					return 0;
				break;
			case ' ': case '\t': case '\f': case '\r': case '\v':
				move(1);
				break;
			case '\\':
				return '%';
			default:
				return ch;
		}
	}
}

void READER::copy_string(string_accum &b, int quote) {
	source_pos a	= current_pos();
	b.putc(quote);
	while (int c = getc()) {
		b.putc(c);
		if (c == quote)
			return;

		if (c == '\\') {
			c = getc();
			if (!c) {
				if (!get_line())
					break;
				c = '\n';
			}
			b.putc(c);
		}
	}
	error(SEV_ERROR, a, "unterminated string");
}

void READER::copy_comment(string_accum &b) {
	b.putc('/');

	int c = getc();
	if (c == '/') {
		b.putc('*');
		while (c = getc()) {
			b.putc(c);
			if (c == '*' && peekc() == '/')
				b.putc(' ');
		}
		b << "*/";

	} else if (c == '*') {
		source_pos a = current_pos();

		b.putc(c);
		for (;;) {
			c = getc();
			if (!c) {
				if (!get_line())
					error(SEV_ERROR, a, "unmatched /*");
				c = '\n';
			}
			b.putc(c);
			if (c == '*' && peekc() == '/') {
				b.putc('/');
				move(1);
				break;
			}
		}
	}
}

void READER::copy_text(string_accum &b) {
	bool		need_newline = false;
	source_pos	a	= current_pos();

	if (!remaining()) {
		if (!get_line())
			error(SEV_ERROR, a, "unmatched %%{");
	}
	b << formati(line_formati, lineno, escaped(input_filename));

	for (;;) {
		switch (int	c = getc()) {
			case 0:
				b.putc('\n');
				need_newline = false;
				if (!get_line())
					error(SEV_ERROR, a, "unmatched %%{");
				break;

			case '\'':
			case '"':
				copy_string(b, c);
				need_newline = true;
				break;

			case '/':
				copy_comment(b);
				need_newline = true;
				break;

			case '%':
			case '\\':
				if (peekc() == '}') {
					if (need_newline)
						b.putc('\n');
					move(1);
					return;
				}
				// FALLTHROUGH
			default:
				b.putc(c);
				need_newline = true;
				break;
		}
	}
}

// Bison documents parameters as separated like this:	{type param1} {type2 param2}
// but also accepts commas								{type param1,  type2 param2}

e_slist<Context::parameter> READER::copy_param() {
	e_slist<Context::parameter>	list;

	expect('{');

	do {
		do {
			skip_whitespace();
			auto	type1		= getp();
			scan(~char_set::wordchar);
			auto	type1_end	= getp();
			if (skip_whitespace().peekc() == '<') {
				if (!skip_to('>'))
					syntax_error();
			}

			skip_whitespace().scan_skip(char_set("*&"));
			type1_end = getp();

			auto	name	= get_token(char_set::alphanum, char_set::wordchar);
			auto	type2	= skip_whitespace().getp();
			if (*type2 == '[') {
				if (!skip_to(']'))
					syntax_error();
			}

			list.push_back(new Context::parameter(name, str(type1, type1_end), str(type2, getp())));
		} while (nextc() == ',');

		expect('}');

	} while (nextc() == '{');

	return list;
}

void READER::declare_tokens(Precedence prec) {
	skip_whitespace();
	const char *tag = maybe_tag();

	while (bucket *bp = get_name_or_literal()) {
		if (bp == goal)
			error(SEV_ERROR, "the start symbol %s cannot be declared to be a token", (char*)bp->name);

		bp->clss = bucket::TERM;

		if (tag) {
			if (bp->tag && tag != bp->tag)
				error(SEV_WARNING, "the type of %s has been redeclared", (char*)bp->name);
			bp->tag = tag;
		}

		if (prec) {
			if (bp->prec && prec != bp->prec)
				error(SEV_WARNING, "the precedence of %s has been redeclared", (char*)bp->name);
			bp->prec	= prec;
		}

		if (is_digit(nextc())) {
			int value = get();
			if (!bp->undefined() && value != bp->value)
				error(SEV_WARNING, "the value of %s has been redeclared", (char*)bp->name);
			bp->value = value;
		}

		if (!nextc())
			unexpected_EOF();
	}
}

void READER::declare_types() {
	skip_whitespace();
	const char	*tag	= maybe_tag();

	while (bucket *bp = get_name_or_literal()) {
#ifdef YYARGS
		if (nextc() == '(') {
			if (bp->args)
				error(SEV_WARNING, "the type of %s has been redeclared", bp->name.begin());

			move(1);			// skip open paren
			int	c;
			while ((c = nextc()) != ')') {
				if (!c)
					unexpected_EOF();
				expect('<');
				bp->args.push_back().tag = get_tag();
			}
			move(1);			// skip close paren
		}
#endif

		if (tag) {
			if (bp->tag && tag != bp->tag)
				error(SEV_WARNING, "the type of %s has been redeclared", bp->name.begin());
			bp->tag = tag;
		}

		if (!nextc())
			unexpected_EOF();
	}
}

#ifdef YYARGS

void READER::get_formal_args(bucket *bp) {
	bool	bad = false;
	for (int i = 0, nargs = bp->args.size32(); i < nargs; ++i) {
		if (bad = nextc() != '$')
			break;

		move(1).skip_whitespace();
		if (auto tag = maybe_tag()) {
			if (bp->args[i].tag && tag != bp->args[i].tag)
				error(SEV_WARNING, "the type of argument %d to %s doesn't agree with previous declaration", i + 1, bp->name.begin());
			bp->args[i].tag = tag;
		}

		if (bad = !(bp->args[i].name = get_id()))
			break;
	}
	if (bad || nextc() != ')')
		error(SEV_ERROR, lineno, "bad formal argument list");
}

void READER::get_args(bucket *bp, rule *r) {
	int		elide_cnt	= 1;

	for (int i = 0, nargs = bp->args.size32(); i < nargs; ++i) {
		const char *yyvaltag	= bp->args[i].tag;

		string_builder	b;
		b << "yyval" << maybe_put_tag(yyvaltag) << " = ";

		int		c;
		while ((c = getc()) != ',' && c != ')') {
			if (c == '$') {
				const char *tag = maybe_tag();
				c = skip_whitespace().peekc();

				int	j	= is_digit(c) || c == '-'
					? r->get_offset(this, get<int>())
					: r->get_offset(this, get_id());

				if (!tag)
					tag = r->get_tag(j);

				if ((tag || yyvaltag) && (!tag || !yyvaltag || tag == yyvaltag)) {
					if (elide_cnt == 1)
						elide_cnt = j;
					else if (j != elide_cnt + i)
						elide_cnt = 0;
				}

				b << "yystack.getl(" << j << ')' << maybe_put_tag(tag);

			} else {
				elide_cnt = 0;
				b.putc(c);
				if (char e = end_char(c)) {
					auto	p = getp();
					skip_to(e);
					b << str(p, getp());
				}
			}
		}

		if (elide_cnt == 0) {
			const char *tag	= bp->args[i].tag;
			string	code	= iso::move(b);
			if (auto r2 = arg_cache.lookup(code)) {
				r->rhs.push_back(rules[i]->lhs);
			} else  {
				bucket *bp = symbols.genbucket(tag, bucket::ARGUMENT);
				r->rhs.push_back(bp);
				arg_cache.insert(rules.push_back(new rule(bp, iso::move(code))));
			}
		}
	}
}
#endif

#ifdef YYDESTRUCTORS

static const char *const any_tag = "*", *const no_tag = "";

string process_destructor(string_scan code, const char *tag) {
	string_builder	new_code;

	while (code.remaining()) {
		if (code.check("$$")) {
			new_code << "(*val)" << maybe_put_tag(tag);

		} else if (code.check("/*")) {
			new_code << "/*" << code.get_to_scan("*/");

		} else {
			int	c = code.getc();
			new_code.putc(c);
			if (c == '\'' || c == '"')
				new_code << code.get_to(c);
		}
	}
	return new_code;
}

string READER::finalise_destructor(const char *sym_destructor, const char *tag) {
	if (sym_destructor)
		return process_destructor(sym_destructor, tag);

	if (tag) {
		if (auto d = find_check(destructors, tag))
			return d->code;

		if (auto d = find_check(destructors, any_tag))
			return process_destructor(d->code, tag);

	} else if (auto d = find_check(destructors, no_tag)) {
		return d->code;
	}
	return 0;
}

void READER::declare_destructor() {
	expect(nextc(), '{');

	source_pos	a = current_pos();
	string		code_text;
	copy_code(build(code_text).formati(line_formati, lineno, escaped(input_filename)) << '\t', 0, true);

	for (;;) {
		int c = nextc();
		if (!c)
			unexpected_EOF();

		if (c == '<') {
			move(1);
			const char *tag = check('>') ? no_tag : check("*>") ? any_tag : get_tag();
			if (find_check(destructors, tag))
				error(SEV_WARNING, a, "destructor redeclared");

			destructors.push_front(new destructor(tag, tag == any_tag ? string(code_text) : process_destructor(code_text, tag)));

		} else if (ident.test(c)) {
			// "symbol" destructor
			bucket *bp = get_name();
			if (bp->destructor)
				error(SEV_WARNING, a, "destructor redeclared");
			else
				bp->destructor = code_text;

		} else
			break;
	}
}

#endif // YYDESTRUCTORS

string_accum &READER::copy_code(string_accum &b, rule *r, bool trial_or_loc) {
	source_pos	start	= current_pos();

	for (int depth = 0;;) {
		switch (int c = getc()) {
			case 0:
				b.putc('\n');
				if (!get_line())
					error(SEV_ERROR, start, "unterminated action");
				break;

			case '$': {
				const char *tag = maybe_tag();
				c = peekc();

				if (c == '$') {
					if (!tag && !(tag = r->lhs->tag) && tags)
						error(SEV_ERROR, "$$ is untyped");
					b << "yyval" << maybe_put_tag(tag);
					move(1);

				} else if (r) {
					if (is_digit(c) || (c == '-' && is_digit(peekc(1)))) {
						int		val	= get<int>();
						int		i	= r->get_offset(this, val);
						if (!tag && !(tag = r->get_tag(i)) && tags) {
							if (const char *name = r->get_name(i))
								error(SEV_ERROR, "$%d (%s) is untyped", val, name);
							else
								error(SEV_ERROR, "$%d is untyped", val);
						}
						b << "yystack.getl(" << i << ')' << maybe_put_tag(tag);

#ifdef YYARGS
					} else if (name1.test(c)) {
						auto	id	= get_id();
						auto	i	= r->get_offset(this, id);
						if (!tag && !(tag = r->get_tag(i)) && tags)
							error(SEV_WARNING, "untyped argument $%s", id);
						b << "yystack.getl(" << i << ')' << maybe_put_tag(tag);
#endif
					}
				} else {
					error(SEV_ERROR, "illegal $-name");
				}
				break;
			}

#ifdef YYLOCATIONS
			case '@':
				c = peekc();
				if (c == '$') {
					b.format("yyloc");
					move(1);
				} else if (r && (is_digit(c) || c == '-')) {
					b.format("yystack.p_mark[%d]", r->get_offset(this, get<int>()));
				} else {
					b.putc('@');
				}
				break;
#endif

#ifdef YYBACKTRACKING
			case '[':
				if (r && trial_or_loc && depth++ == 0)
					c = '{';
				b.putc(c);
				break;

			case ']':
				if (r && trial_or_loc && --depth == 0) {
					b.putc('}');
					return b;
				}
				b.putc(c);
				break;
#endif

			case '{':
				b.putc(c);
				++depth;
				break;

			case '}':
				b.putc(c);
				if (--depth == 0)
					return b;
				break;

			case ';':
				b.putc(c);
				if (depth == 0)
					return b;
				break;

			case '\'':
			case '"':
				copy_string(b, c);
				break;

			case '/':
				copy_comment(b);
				break;

			default:
				b.putc(c);
				break;
		}
	}
}

void READER::pack(Context &ctx) {
	int	nsyms	= 2;
	int	ntokens	= 1;

	for (auto &b : symbols) {
		++nsyms;
		if (b.clss == bucket::TERM)
			++ntokens;
	}

	ctx.start_symbol	= ntokens;

	dynamic_array<bucket*>	v(nsyms);
	bucket **tp = v, **sp = v + ntokens;
	*tp++	= 0;
	*sp++	= 0;
	for (auto &b : symbols) {
		if (&b == goal && sp != v + ntokens + 1)			// move goal to start of non terminals
			*sp++ = exchange(v[ntokens + 1], &b);
		else
			*(b.clss == bucket::TERM ? tp++ : sp++) = &b;
	}

	// set indices
	for (int i = 1; i < nsyms; ++i)
		if (v[i])
			v[i]->index = i;

	// collect given token values
	dynamic_array<int>	values;
	for (auto p : slice(v, 1, ntokens - 1)) {
		if (!p->undefined()) {
			int		n = p->value;
			auto	i = lower_boundc(values, n);
			if (i == values.end() || *i != n)
				values.insert(i, n);
		}
	}

	// set nonterm values
	int	n	= 0;
	int	*pv	= values.begin();

	for (auto p : slice(v, ntokens + 1))
		p->value = n++;

	// set token values
	n	= max(n, 256);
	for (auto p : slice(v, 1, ntokens - 1)) {
		if (p->undefined()) {
			while (pv < values.end() && n == *pv) {
				++n;
				++pv;
			}
			p->value = n++;
		}
	}

	// symbols
	ctx.symbols.reserve(nsyms);

	int max_pval = 0;
	for (int i = 0; i < nsyms; ++i) {
		if (auto s0	= v[i]) {
			auto	&s	= ctx.symbols.emplace_back(s0->name, s0->value, s0->prec);
#ifdef YYDESTRUCTORS
			if (i < ntokens) {
				s.pval		= s0->value;
				max_pval	= max(max_pval, s.pval);
			} else {
				s.pval		= max_pval + 1 + s0->value + 1;
			}
			if (ctx.flags & FLAG_HAS_DESTRUCTOR) {
				s.destructor	= finalise_destructor(s0->destructor, s0->tag);
				s.type_tag		= s0->tag;
			}
#endif
		} else if (i == 0) {
			auto	&s	= ctx.symbols.emplace_back("$end", 0);
#ifdef YYDESTRUCTORS
			s.pval	= 0;
#endif
		} else {
			auto	&s	= ctx.symbols.emplace_back("$accept", -1);
#ifdef YYDESTRUCTORS
			s.pval	= max_pval + 1;
#endif
		}
	}

	//rules
	int	nitems = 4;
	for (auto r : rules)
		nitems += r->rhs.size32() + 1;

	ctx.items.reserve(nitems);
	ctx.rules.reserve(rules.size() + 4);

	ctx.items.push_back(-1);
	ctx.items.push_back(ntokens + 1);//goal->index;
	ctx.items.push_back(0);
	ctx.items.push_back(-2);

	ctx.rules.emplace_back(0, 0);
	ctx.rules.emplace_back(0, 0);
	ctx.rules.emplace_back(ntokens, 1);

	for (auto r0 : rules) {
		Precedence	prec	= 0;
		int			rhs		= ctx.items.size32();

		for (auto k : r0->rhs) {
			ctx.items.push_back(k->index);
			if (k->clss == bucket::TERM)
				prec	= k->prec;
		}
		ctx.items.push_back(-int(ctx.rules.size32()));
		ctx.rules.emplace_back(r0->lhs->index, rhs, r0->line, prec, iso::move(r0->action));
	}
	ctx.rules.emplace_back(0, nitems);
}


READER::bucket *READER::parse_charset() {
	switch (nextc()) {
		case '~': {
			if (auto bp = parse_charset()) {
				bp->charset.negate();
				return bp;
			}
			syntax_error();
		}
		case '\'': {
			auto	p = getp();
			move(1);
			char32	c0 = getc();
			if (c0 == '\\')
				c0 = unescape1(*this);

			if (check("'..'")) {
				char32	c1 = getc();
				if (c1 == '\\')
					c1 = unescape1(*this);
				expect('\'');

				bucket *bp	= symbols.lookup(str(p, getp()));
				bp->clss	= bucket::TERM;
				bp->charset.add({c0, c1});
				return bp;

			} else if (check('\'')) {
				bucket *bp	= symbols.lookup(str(p, getp()));
				bp->clss	= bucket::TERM;
				bp->value	= c0;
				return bp;
			}
		}

		case '[':
			move(1);
			bucket *bp = symbols.genbucket(0, bucket::TERM);
			char32	c0;
			while ((c0 = getc()) != ']') {
				if (c0 == '\\') {
					if (to_lower(peekc()) == 'p') {
						move(1).check('{');
						if (check("In")) {
							if (auto block = unicode::LookupBlock(get_to_scan('}').slice(0, -1)))
								bp->charset.add(*block);
							else
								error(SEV_ERROR, "unrecognised unicode block");

						} else if (auto group = unicode::LookupGroup(get_to_scan('}').slice(0, -1))) {
							for (auto &i : group->r16)
								bp->charset.add(i);
							for (auto &i : group->r32)
								bp->charset.add(i);

						} else {
							error(SEV_ERROR, "unrecognised unicode group");
						}
						continue;
					}
					c0 = unescape1(*this);
				}
				if (peekc() == '-' && peekc(1) != ']') {
					char32	c1 = move(1).getc();
					if (c1 == '\\')
						c1 = unescape1(*this);
					bp->charset.add({c0, c1});
				}
				bp->charset.add(c0);
			}
			return bp;
			break;
	}
	return 0;
}

void READER::parse_rule1(rule *r) {
	string	action;
	bool	empty	= false;
	bool	lex		= r->lhs->clss == bucket::TERM;

	for (;;) {
		int		s_lineno = lineno;

		if (bucket *bp = lex ? parse_charset() : 0) {
			if (empty)
				error(SEV_WARNING, "empty conflict");

			if (action)
				add_sub_action(r, iso::move(action));

			r->rhs.push_back(decorators(bp));

		} else  if (bp = get_name_or_literal()) {
			if (empty)
				error(SEV_WARNING, "empty conflict");

			if (action)
				add_sub_action(r, iso::move(action));

	#ifdef YYARGS
			if (!lex && check('('))
				get_args(bp, r);
	#endif

			r->rhs.push_back(decorators(bp));

		} else switch (nextc()) {
			case '{':
			case '=':
			case '[': {
				bool	trial = peekc() == '[';
				check('=');
				copy_code(
					build(action) << onlyif(trial_rules && !trial, "  if (!yytrial)") << formati(line_formati, lineno, escaped(input_filename)),
					r, trial
				);
				break;
			}
			case '(': {
				if (empty)
					error(SEV_WARNING, "empty conflict");

				if (action)
					add_sub_action(r, iso::move(action));

				move(1);
				bucket *bp = symbols.genbucket(0, bucket::NONTERM);
				parse_rule(rules.push_back(new rule(bp, s_lineno)));
				expect(')');

				r->rhs.push_back(decorators(bp));
				break;
			}
			case '%':
				if (check("%empty")) {
					empty = true;
					break;
				}
				if (check("%prec")) {
					if (check(char_set::whitespace) && (bp = get_name_or_literal())) {
						if (r->prec && r->prec != bp->prec)
							error(SEV_WARNING, "conflicting %%prec specifiers");
						r->prec	= bp->prec;
						break;
					}
					syntax_error();
				}
				//fall through
			default:
				if (action)
					r->action = iso::move(action);
				else if (r->lhs->tag && (r->rhs.empty() || r->rhs[0]->tag != r->lhs->tag))
					error(SEV_WARNING, "the default action for %s assigns an undefined value to $$", r->lhs->name.begin());
				return;
		}
	}

}

void READER::parse_rule(rule *r) {
	parse_rule1(r);
	while (nextc() == '|') {
		move(1);
		parse_rule1(rules.push_back(new rule(r->lhs, lineno)));
	}
}

// keyword codes
enum {
	KW_TOKEN			= Precedence::NONE,
	KW_LEFT				= Precedence::LEFT,
	KW_RIGHT			= Precedence::RIGHT,
	KW_NONASSOC			= 3,
	KW_MARK				= 4,
	KW_TEXT				= 5,
	KW_TYPE				= 6,
	KW_START			= 7,
	KW_UNION			= 8,
	KW_DEFINE			= 9,
	KW_EXPECT			= 10,
	KW_EXPECT_RR		= 11,
	KW_PURE_PARSER		= 12,
	KW_PARAM			= 13,
	KW_PARSE_PARAM		= 14,
	KW_LEX_PARAM		= 15,
	KW_POSIX_YACC		= 16,
	KW_TOKEN_TABLE		= 17,
	KW_ERROR_VERBOSE	= 18,
	KW_XXXDEBUG			= 19,
	KW_INITIAL_ACTION	= 20,
	KW_LOCATIONS		= 21,
	KW_DESTRUCTOR		= 22,
};

static struct keyword {
	char name[16];
	int token;
} keywords[] = {
	{ "term",			KW_TOKEN },
	{ "token",			KW_TOKEN },
	{ "left",			KW_LEFT },
	{ "right",			KW_RIGHT },
	{ "binary",			KW_NONASSOC },
	{ "nonassoc",		KW_NONASSOC },
	{ "type",			KW_TYPE },
	{ "start",			KW_START },
	{ "union",			KW_UNION },
	{ "define",			KW_DEFINE },
	{ "expect",			KW_EXPECT },
	{ "expect-rr",		KW_EXPECT_RR },
	{ "pure-parser",	KW_PURE_PARSER },
	{ "param",			KW_PARAM },
	{ "parse-param",	KW_PARSE_PARAM },
	{ "lex-param",		KW_LEX_PARAM },
	{ "yacc",			KW_POSIX_YACC },
	{ "token-table",	KW_TOKEN_TABLE },
	{ "error-verbose",	KW_ERROR_VERBOSE },
	{ "debug",			KW_XXXDEBUG },
	{ "initial-action",	KW_INITIAL_ACTION },
#ifdef YYLOCATIONS
	{ "locations",		KW_LOCATIONS },
#endif
#ifdef YYDESTRUCTORS
	{ "destructor",		KW_DESTRUCTOR },
#endif
};

int READER::keyword() {
	auto	col = current_col();
	move(1);

	if (auto tok = get_token(char_set::alpha, ident)) {
		auto	tok2 = replace(to_lower(tok), "_", "-");
		for (auto &k : keywords) {
			if (tok2 == k.name)
				return k.token;
		}

	} else {
		switch (getc()) {
			case '{':	return KW_TEXT;
			case '%':
			case '\\':	return KW_MARK;
			case '<':	return KW_LEFT;
			case '>':	return KW_RIGHT;
			case '0':	return KW_TOKEN;
			case '2':	return KW_NONASSOC;
		}
	}
	syntax_error(col);
	unreachable();
}

const char *define_bool[]			= {"false", "true", 0};
const char *define_pure[]			= {"false", "true", "full", 0};
const char *define_push_pull[]		= {"pull", "push", "both", 0};
const char *define_value_type[]		= {"union-directive", "union", "variant", 0};
const char *define_def_red[]		= {"most", "consistent", "accepting", 0};
const char *define_lr_type[]		= {"lalr", "ielr", "canonical-lr", 0};
const char *define_error[]			= {"simple" ,"verbose", 0};
const char *define_lac[]			= {"none", "full", 0};

struct define {
	const char	*name;
	const char	**values;
	uint32		offset;
	bool read(READER &reader, Context &ctx);
} defines[] = {
	{"api.namespace",	            0,					get_offset(&Context::namespce)},
	{"namespace",	                0,					get_offset(&Context::namespce)},
	{"api.location.file",			0,					get_offset(&Context::location_file)},
	{"api.location.include",	    0,					get_offset(&Context::location_inc)},
	{"api.location.type",	        0,					get_offset(&Context::location_type)},
	{"location_type",	            0,					get_offset(&Context::location_type)},
	{"api.parser.class",	        0,					get_offset(&Context::parser_class)},
	{"parser_class_name",	        0,					get_offset(&Context::parser_class)},
	{"api.prefix",	                0,					get_offset(&Context::prefix)},
	{"api.pure",	                define_pure,		FLAG_PURE},
	{"api.push-pull",	            define_push_pull,	FLAG_PUSHPULL},
	{"api.token.constructor",	    define_bool,		FLAG_CONSTRUCTOR},
	{"api.token.prefix",	        0,					get_offset(&Context::prefix)},
	{"api.value.automove",	        define_bool,		FLAG_AUTOMOVE},
	{"api.value.type",		        0,					get_offset(&Context::value_type)},
	{"api.value.type",		        define_value_type,	FLAG_VALUE_TYPE},
	{"api.value.union.name",        0,					get_offset(&Context::union_name)},
	{"lr.default-reduction",        define_def_red,		FLAG_DEFRED},
	{"lr.keep-unreachable-state",	define_bool,		FLAG_UNREACHABLE},
	{"lr.type",                     define_lr_type,		FLAG_LRTYPE},
	{"parse.assert",	            define_bool,		FLAG_ASSERTS},
	{"parse.error",	                define_error,		FLAG_VERBOSE},
	{"parse.lac",                   define_lac,			FLAG_LAC},
	{"parse.trace",	                define_bool,		FLAG_TRACE},
};

bool define::read(READER &reader, Context &ctx) {
	if (values) {
		if (!reader.remaining()) {
			ctx.flags |= offset;
			return true;
		}
		for (const char **v = values; *v; ++v) {
			if (reader.check(*v)) {
				ctx.flags = (ctx.flags & ~offset) | (lowest_set(offset) * (v - values));
				return true;
			}
		}
		return false;
	}
	if ((offset & OFF_TYPE) == OFF_INT) {
		int	*dest	= (int*)((char*)&ctx + (offset & OFF_MASK));
		*dest		= reader.get<int>();

	} else {
		string	*dest	= (string*)((char*)&ctx + (offset & OFF_MASK));
		if (reader.peekc() == '{')
			reader.copy_code(build(*dest), 0, false);
		else if (reader.peekc() == '"')
			reader.copy_string(build(*dest), reader.getc());
		else if (reader.check("none"))
			*dest = 0;
		else
			*dest = reader.get_token(ident);
	}
	return true;
}

void READER::process_define(Context &ctx) {
	auto	id	= get_token(name1, ident);
	skip_whitespace();

	for (auto &i : defines) {
		if (id == i.name) {
			if (i.read(*this, ctx))
				return;
		}
	}
	syntax_error();
}

READER::READER(Context &ctx, istream_ref file, const char *input_filename) : LINEREADER(file, input_filename), goal(0), trial_rules(!!(ctx.flags & FLAG_BACKTRACK)) {
	for (int stage = 0, prec = 0; stage < 2;) {
		if (nextc() == '%') {
			switch (int k = keyword()) {
				case KW_MARK:
					++stage;
					break;

				case KW_TEXT:
					copy_text(build(ctx.include_source));
					break;

				case KW_UNION:
					if (ctx.union_def)
						error(SEV_ERROR, col_t(current_col() - 6), " too many %%union declarations");
					if (nextc() != '{')
						ctx.union_name = get_token(ident);
					ctx.union_def = copy_code(string_builder() << formati(line_formati, lineno, escaped(this->input_filename)), 0, false);
					break;

				case KW_LEFT:
				case KW_RIGHT:
				case KW_NONASSOC:
					++prec;
				case KW_TOKEN:
					declare_tokens(Precedence(prec, (Precedence::ASSOC)k));
					break;

				case KW_EXPECT:
				case KW_EXPECT_RR:
					skip_whitespace().get(k == KW_EXPECT ? ctx.SRexpect : ctx.RRexpect);
					break;

				case KW_TYPE:
					declare_types();
					break;

				case KW_DEFINE:
					process_define(ctx);
					break;

				case KW_START:
					set_start(get_name_or_literal());
					break;

				case KW_PURE_PARSER:
					ctx.flags |= FLAG_PURE;
					break;

				case KW_PARAM: {
					auto	params = copy_param();
					for (auto &p : params)
						ctx.lex_param.push_back(dup(p));
					ctx.parse_param.append(iso::move(params));
					break;
				}

				case KW_PARSE_PARAM:
					ctx.parse_param.append(copy_param());
					break;

				case KW_LEX_PARAM:
					ctx.lex_param.append(copy_param());
					break;

				case KW_TOKEN_TABLE:
					ctx.flags |= FLAG_TOKENTABLE;
					break;

				case KW_ERROR_VERBOSE:
					ctx.flags |= FLAG_VERBOSE;
					break;

	#ifdef YYLOCATIONS
				case KW_LOCATIONS:
					ctx.flags |= FLAG_LOCS;
					break;
	#endif
	#ifdef YYDESTRUCTORS
				case KW_DESTRUCTOR:
					ctx.flags |= FLAG_HAS_DESTRUCTOR;
					declare_destructor();
					break;
	#endif
				case KW_INITIAL_ACTION:
					if (skip_whitespace().peekc() != '{')
						syntax_error();
					copy_code(build(ctx.initial_action).formati(line_formati, lineno, escaped(this->input_filename)) << '\t', 0, true);
					break;

				case KW_XXXDEBUG:
					// XXX: FIXME
					break;

				case KW_POSIX_YACC:
					// noop for bison compatibility. byacc is already designed to be posix yacc compatible
					break;
			}

		} else if (bucket *bp = get_name_or_literal()) {
			int	s_lineno = lineno;
#ifdef YYARGS
			if (nextc() == '(') {
				move(1);
				get_formal_args(bp);
			}
	#endif
			expect(':');

			if (!bp->clss)
				bp->clss = is_upper(bp->name[0]) ? bucket::TERM : bucket::NONTERM;

//			if (bp->clss == bucket::TERM)
//				error(SEV_ERROR, s_lineno, "a token appears on the lhs of a production");

			if (!goal && bp->clss == bucket::NONTERM)
				set_start(bp);

			parse_rule(rules.push_back(new rule(bp, s_lineno)));
			check(';');

		} else {
			syntax_error();
		}
	}

	ctx.include_footer = string_builder() << formati(line_formati, lineno, escaped(this->input_filename));
	ctx.include_footer.read(input, input.remaining(), false);

	if (!goal)
		error(SEV_ERROR, "no grammar has been specified");

	if (!goal->clss)
		error(SEV_ERROR, "the start symbol %s is undefined", goal->name.begin());

#ifdef YYARGS
	if (goal->args)
		error(SEV_WARNING, "start symbol %s requires arguments", goal->name.begin());
#endif

	for (auto &b : symbols) {
		if (!b.clss) {
			error(SEV_WARNING, "the symbol %s is undefined", b.name.begin());
			b.clss = bucket::TERM;
		}
	}

	pack(ctx);

	// build the union TBD
	if (!ctx.value_type) {
		switch (ctx.flags & FLAG_VALUE_TYPE) {
			case FLAG_VALUE_TYPE_UNION:
			case FLAG_VALUE_TYPE_VARIANT:
				break;
		}
	}
}

//-----------------------------------------------------------------------------
//	OUTPUT
//-----------------------------------------------------------------------------

#ifdef YYBACKTRACKING

struct CONFLICTS {
	dynamic_array<int>	conflicts;

	int find_conflict_base(int base) {
		int	i;
		for (i = 0; i < base; i++) {
			int	j;
			for (j = 0; j + base < conflicts.size32(); j++) {
				if (conflicts[i + j] != conflicts[base + j])
					break;
			}
			if (j + base >= conflicts.size32())
				break;
		}

		if (i < base) {
			if (conflicts[base] == -1)
				base++;
			conflicts.resize(base);
		}

		return i;
	}

	void get(const PARSER::state &s, int *row, bool glr) {
		int		sym	= -1, base;
		for (auto &p : s.actions) {
			if (sym != -1 && sym != p.symbol) {
				conflicts.push_back(-1);
				row[sym]	= find_conflict_base(base);
				sym			= -1;
			}
			if (p.suppressed == 1) {
				sym		= p.symbol;
				base	= conflicts.size32();

				if (p.is_shift()) {
					conflicts.push_back(p.index);

				} else if (p.index != s.defred) {
					if (!glr) {
						if (base == conflicts.size32()) {
							if (base)
								--base;
							else
								conflicts.push_back(-1);
						}
					}
					conflicts.push_back(p.index - 2);
				}
			}
		}

		if (sym != -1) {
			conflicts.push_back(-1);
			row[sym]	= find_conflict_base(base);
		}
	}

};

#endif

struct PACKER {
	struct vector : dynamic_array<pair<int, int>> {
		int		low, width, pos;
		vector() : pos(0) {}

		template<typename F, typename T> void set(F from, T to, int n, int skip) {
			width		= 0;
			int		v0	= maximum, v1 = 0;
			for (int j = 0; j < n; ++j) {
				if (to[j] != skip) {
					auto v	= from[j];
					v0		= min(v0, v);
					v1		= max(v1, v);
					emplace_back(v, to[j]);
				}
			}
			low		= v0;
			width	= v1 - v0 + 1;
		}
	};

	int		lowzero;
	dynamic_array<vector>	vectors;
	dynamic_array<int>		table, check;

	const vector *matching_vector(const vector *v, range<vector**> done) {
		int		t = v->size32();
		int		w = v->width;

		for (auto v2 : reversed(done)) {
			if (v2->width != w || v2->size32() != t)
				break;

			bool	match = true;
			for (int k = 0; match && k < t; k++)
				match = (*v2)[k] == (*v)[k];

			if (match)
				return v2;
		}

		return 0;
	}

	int pack_vector(vector *v, range<vector**> done) {
		int		t		= v->size32();
		auto	*p		= v->begin();
		int		offset	= lowzero - v->low;

		for (bool ok = false; !ok; ++offset) {
			if (offset == 0)	// don't want zeroes in table
				continue;

			ok = true;
			for (int k = 0; ok && k < t; k++) {
				int loc = offset + p[k].a;
				ok = loc >= table.size() || check[loc] == -1;
			}
			if (ok) {
				for (auto p : done)
					if (!(ok = p->pos != offset))
						break;
			}
		}

		--offset;
		int	expand = offset + v->low + v->width - table.size32();
		if (expand > 0) {
			table.expand(expand);
			check.append(expand, -1);
		}

		for (int k = 0; k < t; k++) {
			int loc = offset + p[k].a;
			table[loc] = p[k].b;
			check[loc] = p[k].a;
		}

		while (check[lowzero] != -1)
			++lowzero;

		return offset;
	}

	void pack() {
		dynamic_array<vector*>	order;
		for (auto &v : vectors) {
			if (int t = v.size32())
				order.push_back(&v);
		}

		//sort vectors by width, then tally
		insertion_sort(order.begin(), order.end(), [](const vector *a, const vector *b) {
			return a->width != b->width ? a->width > b->width : a->size() > b->size();
		});

		//pack_table
		int		done = 0;
		for (auto &v : order) {
			if (const vector *v2 = matching_vector(v, slice(order, 0, done))) {
				v->pos = v2->pos;
			} else {
				v->pos = pack_vector(v, slice(order, 0, done));
				order[done++] = v;
			}
		}
	}

	auto get_offsets(int i, int n) {
		return transformc(slice(vectors, i, n), [](const vector &v) { return v.pos; });
	}

	auto get_tables(int i, int n) {
		if (vectors.size() < i + n)
			vectors.resize(i + n);
		return slice(vectors, i, n);
	}

	PACKER(int nvectors = 0) : lowzero(1), vectors(nvectors) {}
};

template<typename B> struct split_s2 {
	buffer_accum<256>	a;
	B					b;
	int					last;
	split_s2(B &&b) : b(forward<B>(b)), last(0) {}
	~split_s2()			{ b << str(a); }
	template<typename T> auto& operator<<(const T &t) { a << t; return *this; }
	auto& operator<<(const tagged<int,int> &col) {
		int	prev = last;
		last = a.size32();

		if (last > col) {
			auto	s = str(a);
			b << s.slice(0, prev) << '\n';
			memcpy(s, s + prev, last - prev);
			a.move(-prev);
		}
		return *this;
	}
};

template<typename B> inline auto split_at2(B &&b) {
	return split_s2<B>(forward<B>(b));
}

string_accum& operator<<(string_accum &a, const Context::parameter &p) {
	return a << p.type << onlyif(!p.type.ends("*"), ' ') << p.name << p.type2;
}

auto names(const e_slist<Context::parameter> &list) {
	return make_field_container(list, name);
}

string_accum& output_type(string_accum &&a, int *table, int n) {
	int	v0 = table[0], v1 = table[0];
	for (int i = 1; i < n; i++) {
		v0 = min(v0, table[i]);
		v1 = max(v1, table[i]);
	}
	if (v0 < 0) {
		v1 = max(~v0, v1) * 2;
		if (v1 < (1 << 8))
			a << "signed ";
	} else {
		a << "unsigned ";
	}
	return a << (v1 < (1 << 8) ? "char" : v1 < (1 << 16) ? "short" : "int");
}

dynamic_array<int> get_table(count_string macro, const Context &ctx, const PARSER &parser, const LALR &lalr, PACKER &packer, CONFLICTS &conflicts) {
	int		nstates		= parser.states.size32();

	if (!packer.vectors && is_any(macro, "shift_offset_table", "reduce_offset_table", "conflict_offset_table", "goto_offset_table", "packed_table", "packed_check_table")) {
		auto	vector		= packer.get_tables(0, nstates * 3 + ctx.nvars() - 1).begin();
		static const int	EMPTY	= 0x80000000;
		auto				values	= make_field_container(ctx.symbols, value);
		dynamic_array<int>	shifts(ctx.start_symbol);
		dynamic_array<int>	reduces(ctx.start_symbol);

		for (auto &s : parser.states) {
			if (s.actions) {
				bool	got_conflicts = false;
				fill(shifts, EMPTY);
				fill(reduces, EMPTY);

				for (auto &p : s.actions) {
					got_conflicts = got_conflicts || p.suppressed == 1;
					if (p.suppressed == 0 || ((ctx.flags & FLAG_GLR) && p.is_shift())) {
						if (p.is_shift())
							shifts[p.symbol] = p.index;
						else if (p.index != s.defred)
							reduces[p.symbol] = p.index - 2;
					}
				}

				int	stateno = parser.states.index_of(s);
				vector[stateno + nstates * 0].set(values, shifts, ctx.start_symbol, EMPTY);
				vector[stateno + nstates * 1].set(values, reduces, ctx.start_symbol, EMPTY);

#ifdef YYBACKTRACKING
				if (got_conflicts && (ctx.flags & (FLAG_BACKTRACK | FLAG_GLR))) {
					fill(shifts, EMPTY);
					conflicts.get(s, shifts, !!(ctx.flags & FLAG_GLR));
					vector[stateno + nstates * 2].set(values, shifts, ctx.start_symbol, EMPTY);
				}
#endif
			}
		}

		for (int i = 1, nvars = ctx.nvars(); i < nvars; i++) {
			auto	vals	= lalr.gotos(i);
			ISO_ASSERT(ctx.symbols[i + ctx.start_symbol].value == i - 1);
			//vector[ctx.symbols[i + ctx.start_symbol].value + nstates * 3].set(
			vector[i - 1 + nstates * 3].set(
				make_field_container(vals, a), make_field_container(vals, b), vals.size32(), lalr.default_goto[i]
			);
		}
		packer.pack();
	}

	if (macro == "packed_table")
		return packer.table;
	else if (macro == "packed_check_table")
		return packer.check;

	else if (macro == "shift_offset_table")
		return packer.get_offsets(nstates * 0, nstates);
	else if (macro == "reduce_offset_table")
		return packer.get_offsets(nstates * 1, nstates);
	else if (macro == "conflict_offset_table")
		return packer.get_offsets(nstates * 2, nstates);
	else if (macro == "goto_offset_table")
		return packer.get_offsets(nstates * 3, ctx.nvars() - 1);

	else if (macro == "rule_lhs_table")
		return transformc(slice(ctx.rules, 2, -1), [&ctx](const Context::rule &r) { return ctx.symbols[r.lhs].value; });
	else if (macro == "rule_len_table")
		return transformc(slice(ctx.rules, 2, -1), [](const Context::rule &r) { return (&r)[1].rhs - r.rhs - 1; });
	else if (macro == "default_reduce_table")
		return transformc(parser.states, [](const PARSER::state &e) { return e.defred ? e.defred - 2 : 0; });
	else if (macro == "default_goto_table")
		return slice(lalr.default_goto, 1, ctx.nvars() - 1);

#ifdef YYDESTRUCTORS
	else if (macro == "state_symbol_table")
		return transformc(lalr.states, [&ctx](const LALR::state &e) { return ctx.symbols[e.accessing_symbol].pval;});
#endif

#ifdef YYBACKTRACKING
	else if (macro == "conflict_offset_table")
		return packer.get_offsets(nstates * 2, nstates);
	else if (macro == "conflict_table")
		return conflicts.conflicts;
#endif

	else if (macro == "rule_line_table")
		return transformc(slice(ctx.rules, 2, -1), [](const Context::rule &r) { return r.line; });

	else if (macro == "rule_rhs_table" || macro == "rule_rhs_offset_table") {
		int	maxtok = 0;
		for (int i = 0; i < ctx.start_symbol; ++i)
			maxtok = iso::max(maxtok, ctx.symbols[i].value);

		dynamic_array<int>	rule_rhs;
		dynamic_array<int>	rule_rhs_offsets;
		for (auto &r : slice(ctx.rules, 2, -1)) {
			rule_rhs_offsets.push_back(rule_rhs.size32());
			for (int j = r.rhs, i; (i = ctx.items[j]) > 0; ++j)
				rule_rhs.push_back(ctx.symbols[i].value + (i < ctx.start_symbol ? 0 : maxtok + 2));
		}
		if (macro == "rule_rhs_table")
			return rule_rhs;
		else
			return rule_rhs_offsets;
	}
	return none;
}

string_accum &output(string_accum &a, count_string macro, const Context &ctx) {
	if (macro == "symbol_name_table") {
		int	max = 0;
		for (auto &i : ctx.symbols)
			max = iso::max(max, i.pval);

		dynamic_array<const char*> symnam(max + 2, nullptr);
		for (auto &i : ctx.symbols)
			symnam[i.pval] = i.name;
		symnam[max + 1]	= "illegal-symbol";

		auto	b	= split_at2(a);
		for (auto s : symnam) {
			if (s) {
				size_t	len	= string_len(s);
				if ((s[0] == '"' || s[0] == '\'') && !(ctx.flags & FLAG_SUPPRESS_QUOTED)) {
					++s;
					len -= 2;
				}
				b << '"' << escaped(str(s, len)) << '"';
			} else {
				b << 0;
			}
			b << ',' << tagged<int,int>(80);
		}
	} else if (macro == "parse_params")
		a << separated_list(ctx.parse_param);
	else if (macro == "parse_args")
		a << separated_list(names(ctx.parse_param));
	else if (macro == "lex_params")
		a << onlyif(ctx.flags & FLAG_LOCS, ", YYLTYPE *yylloc") << separated_list(ctx.lex_param);
	else if (macro == "lex_args")
		a << onlyif(ctx.flags & FLAG_LOCS, ", yylloc") << separated_list(names(ctx.lex_param));

	return a;
}

string_accum &output(string_accum &&a, count_string macro, const Context &ctx) {
	return output(a, macro, ctx);
}


struct interp {
	const char	*name;
	uint32		offset;
};

template<typename C> struct interps_array { static interp array[]; };

template<> interp interps_array<Context>::array[] = {
	{"api.pure",					FLAG_PURE},
	{"api.glr",						FLAG_GLR},
	{"api.backtrack",				FLAG_BACKTRACK},
	{"include_source",				get_offset(&Context::include_source)},
	{"include_footer",				get_offset(&Context::include_footer)},
	{"initial_action",				get_offset(&Context::initial_action)},
	{"input_filename",				get_offset(&Context::input_filename)},
	{"output_filename",				get_offset(&Context::output_filename)},
	{"final_state",					get_offset(&Context::final_state)},
	{"union",						get_offset(&Context::union_def)},

	{"namespace",					get_offset(&Context::namespce)},
	{"api.location.file",			get_offset(&Context::location_file)},
	{"api.location.include",		get_offset(&Context::location_inc)},
	{"api.location.type",			get_offset(&Context::location_type)},
	{"location_type",				get_offset(&Context::location_type)},
	{"api.parser.class",			get_offset(&Context::parser_class)},
	{"parser_class_name",			get_offset(&Context::parser_class)},
	{"api.prefix",					get_offset(&Context::prefix)},
	{"api.push-pull",				FLAG_PUSHPULL},
	{"api.token.destructor",		FLAG_HAS_DESTRUCTOR},
	{"api.token.constructor",		FLAG_CONSTRUCTOR},
	{"api.token.prefix",			get_offset(&Context::prefix)},
	{"api.value.automove",			FLAG_AUTOMOVE},
	{"api.value.type",				get_offset(&Context::value_type)},
	{"api.value.type",				FLAG_VALUE_TYPE},
	{"api.value.union.name",		get_offset(&Context::union_name)},
	{"union_name",					get_offset(&Context::union_name)},
	{"lr.default-reduction",		FLAG_DEFRED},
	{"lr.keep-unreachable-state",	FLAG_UNREACHABLE},
	{"lr.type",						FLAG_LRTYPE},
	{"parse.assert",				FLAG_ASSERTS},
	{"parse.error",					FLAG_VERBOSE},
	{"parse.lac",					FLAG_LAC},
	{"parse.trace",					FLAG_TRACE},
};

struct interp_token {
	string	name;
	int		value;
	interp_token(string &&name, int value) : name(move(name)), value(value) {}
};
template<> interp interps_array<interp_token>::array[] = {
	{"token_name",		get_offset(&interp_token::name)	},
	{"token_value",		get_offset(&interp_token::value)},
};

struct interp_rule {
	int			index;
	const char*	action;
	interp_rule(int index, const char *action) : index(index), action(action) {}
};
template<> interp interps_array<interp_rule>::array[] = {
	{"rule_index",		get_offset(&interp_rule::index)	},
	{"rule_action",		get_offset(&interp_rule::action)},
};

struct interp_symbol {
	const char *name;
	int			index;
	const char*	destructor;
	interp_symbol(const char *name, int index, const char *destructor) : name(name), index(index), destructor(destructor) {}
};
template<> interp interps_array<interp_symbol>::array[] = {
	{"symbol_index",		get_offset(&interp_symbol::index)	},
	{"symbol_destructor",	get_offset(&interp_symbol::destructor)},
};

struct interps {
	range<interp*>	offsets;
	const uint8*	container;
	uint32			flags;
	string_param lookup(const count_string &token) const {
		for (auto &i : offsets) {
			if (i.name == token) {
				auto t = container + (i.offset & OFF_MASK);
				switch (i.offset & OFF_TYPE) {
					case OFF_STRING:	return *(const char**)t;
					case OFF_INT:		return to_string(*(int*)t);
					default:			return to_string((flags & i.offset) / lowest_set(i.offset));
				}
			}
		}
		return 0;
	}
	template<typename C> interps(const C &c) : offsets(interps_array<C>::array), container((const uint8*)&c), flags(c.flags) {}
	template<typename C> interps(const C &c, uint32 flags) : offsets(interps_array<C>::array), container((const uint8*)&c), flags(flags) {}
};

struct INTERPOLATOR : LINEREADER, default_get_op<INTERPOLATOR> {
	using LINEREADER::expect;

	const Context	&ctx;
	const PARSER	&parser;
	const LALR		&lalr;
	PACKER			packer;
	CONFLICTS		conflicts;
	FileOutput		out;

	const interps	*current_interps;

	void expect(char c) {
		if (!check(c))
			error(SEV_ERROR, "expected '%c'", c);
	}
	count_string get_macro() {
		if (check('(')) {
			auto token = get_token(ident);
			ISO_VERIFY(check(')'));
			return token;
		}
		return get_token(ident);
	}
	int		get_value(bool skip) {
		if (skip_whitespace().check('$')) {
			if (current_interps && check('$')) {
				if (auto value = current_interps->lookup(get_macro()))
					return from_string<int>(value);
				return 0;
			}

			auto	macro = get_macro();
			if (macro == "exists") {
				bool	paren = check('(');

				if (current_interps && check("$$")) {
					macro = get_macro();
					if (paren)
						expect(')');
					return !!current_interps->lookup(macro);
				}

				check('$');
				macro = get_macro();
				if (paren)
					expect(')');

				return interps(ctx).lookup(macro) || get_table(macro, ctx, parser, lalr, packer, conflicts);
			}
			if (auto value = interps(ctx).lookup(macro))
				return from_string<int>(value);

			return 0;
		}
		return get<int>();
	}
	string	interp_string() {
		string_builder	b;
		for (const char *p0 = getp(), *p1; p1 = scan_skip('$'); p0 = getp()) {
			b << str(p0, p1 - p0 - 1);
			if (auto s = interps(ctx).lookup(get_macro()))
				b << s;
		}
		b << remainder();
		return b;
	}

	void	replace(const interps &interps) {
		current_interps = &interps;
		*(string_scan*)this = line;
		if (!check("$if") || expression::evaluate<int>(*this)) {
			process_line();
			out.write("\r\n");
		}
		current_interps = 0;
	}

	bool	process_line();
	void	process();

	INTERPOLATOR(istream_ref file, const char *input_filename, const Context &ctx, const PARSER &parser, const LALR &lalr)
		: LINEREADER(file, input_filename), ctx(ctx), parser(parser), lalr(lalr), current_interps(0) {}
};

void INTERPOLATOR::process() {
	uint32	if_depth	= 0, if_go = 0;
	bool	put_line	= false;
	bool	go			= true;

	while (get_line()) {
		if (!go && if_go == if_depth)
			put_line = true;

		go = if_go == if_depth;

		if (peekc() == '$' && !line.find("$$")) {
			if (check("$if")) {
				if (go)
					if_go += expression::evaluate<int>(*this) ? 2 : 1;
				if_depth += 2;
				continue;

			} else if (check("$else")) {
				if (if_depth == 0)
					error(SEV_ERROR, "$else without $if");
				if (go)
					if_go -= 2;
				else if (if_depth == if_go + 1)
					++if_go;
				continue;

			} else if (check("$elif")) {
				if (if_depth == 0)
					error(SEV_ERROR, "$elif without $if");
				if (go)
					if_go -= 2;
				else if (if_depth == if_go + 1 && expression::evaluate<int>(*this))
					++if_go;
				continue;

			} else if (check("$endif")) {
				if_depth -= 2;
				if_go = min(if_go, if_depth);
				continue;

			} else if (check("$output")) {
				string	fn = trim(interp_string());
				out.open(fn);
				continue;
			}
		}

		if (go) {
			if (put_line)
				out.write(formati(line_formati, lineno, escaped(input_filename)));
			else
				out.write("\r\n");

			if (scan_skip("$$")) {
				check('(');
				if (begins("rule_")) {
					int	index = 0;
					for (auto &i : slice(ctx.rules, 2, -1))
						replace(interps(interp_rule(index++, i.action), 0));
					continue;

				} else if (begins("token_")) {
					replace(interps(interp_token("YYEOF", ctx.symbols[0].value), 0));
					replace(interps(interp_token("YYERRCODE", ctx.symbols[1].value), 0));
					for (auto &i : slice(ctx.symbols, 2, ctx.start_symbol - 2)) {
						const char *s	= i.name;
						size_t		len	= string_len(s);
						if ((s[0] == '"' || s[0] == '\'') && !(ctx.flags & FLAG_SUPPRESS_QUOTED)) {
							++s;
							len -= 2;
						}
						if (name1.test(s[0]) && string_check(s + 1, len - 1, name2))
							replace(interps(interp_token(str(s, len), i.value), 0));
					}
					continue;

				} else if (begins("symbol_")) {
					for (auto &i : slice(ctx.symbols, 2)) {
						replace(interps(interp_symbol(i.name, i.pval, i.destructor), 0));
					}
					continue;
				}
			}

			put_line = process_line();
		}
	}
}

bool INTERPOLATOR::process_line() {
	bool	put_line	= false;

	for (const char *p0 = getp(), *p1; p1 = scan_skip('$'); p0 = getp()) {
		out.writebuff(p0, p1 - p0 - 1);

		if (current_interps && check('$')) {
			auto	macro = get_macro();
			out.write(current_interps->lookup(macro));
			continue;
		}

		auto	macro = get_macro();

		if (macro == "ctype") {
			bool	paren = check('(');
			check('$');
			macro = get_macro();
			if (paren)
				expect(')');
			if (auto t = get_table(macro, ctx, parser, lalr, packer, conflicts))
				output_type(make_stream_accum(out), t, t.size32());
			else
				out.write("int");

		} else if (macro == "separator") {
			bool			paren	= check('(');
			count_string	before	= ", ", after;

			check('$');
			macro = get_macro();
			if (paren) {
				char delim = skip_whitespace().getc();
				if (delim != ')') {
					if (delim == ',') {
						before = none;
					} else {
						before = get_token(~char_set(delim));
						move(1);
					}
					delim = skip_whitespace().getc();
					if (delim == ',') {
						delim = skip_whitespace().getc();
						if (delim != ')') {
							after = get_token(~char_set(delim));
							delim = move(1).skip_whitespace().getc();
						}
					}
					expect(delim, ')');
				}
			}
			string_builder	b;
			output(b, macro, ctx);
			if (b.length()) {
				out.write(before);
				out.write(b.term());
				out.write(after);
			}

		} else if (auto s = interps(ctx).lookup(macro)) {
			out.write(s);
			put_line = s.find('\n');

		} else if (auto t = get_table(macro, ctx, parser, lalr, packer, conflicts)) {
			auto	s = split_at2(make_stream_accum(out));
			for (auto &i : t)
				s << formatted(i, FORMAT::NONE, 5) << ',' << tagged<int,int>(80);
			put_line = true;

		} else {
			output(make_stream_accum(out), macro, ctx);
			put_line = true;
		}
	}

	out.write(remainder());
	return put_line;
}

} // namespace yacc

//-----------------------------------------------------------------------------
//	DRIVER
//-----------------------------------------------------------------------------

#include"iso/iso_convert.h"

using namespace iso;

int YACC(const ISO::unescaped<string> &fn, const ISO::unescaped<string> &out_fn, const ISO::unescaped<string> &template_fn) {
	yacc::Context	ctx;
	ctx.union_name	= "YYSTYPE";
//	ctx.flags |= yacc::FLAG_BACKTRACK;
//	ctx.flags |= yacc::FLAG_GLR;

	FileInput		in(fn);
	yacc::READER	reader(ctx, in, fn);
	yacc::LR0		lr0(ctx);
	yacc::LALR		lalr(ctx, lr0);
	yacc::PARSER	parser(ctx, lr0, lalr);

	ctx.input_filename = escape(fn);
	ctx.output_filename = out_fn;

	FileInput	tmpl(template_fn);
	yacc::INTERPOLATOR	interp(tmpl, template_fn, ctx, parser, lalr);
	interp.process();

	lalr.print_graph(stream_accum<FileOutput>(filename(out_fn).set_ext("gv"))
	<<	"digraph " << filename(ctx.output_filename).name() << " {\n"
		"\tedge [fontsize=10];\n"
		"\tnode [shape=box,fontsize=10];\n"
		"\torientation=landscape;\n"
		"\trankdir=LR;\n"
		"\t/*\n"
		"\tmargin=0.2;\n"
		"\tpage=\"8.27,11.69\"; // for A4 printing\n"
		"\tratio=auto;\n"
		"\t*/\n"
		, ctx, lr0.closure
	) << "}\n";
	;

	return 1;
}

static initialise init(
	ISO_get_operation(YACC)
);
