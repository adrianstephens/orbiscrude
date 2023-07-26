#include "iso/iso_files.h"
#include "pdb.h"
#include "coff.h"	// jut for SECTION_HEADER
#include "cvinfo_iso.h"
#include "maths/graph.h"
#include "extra/regex.h"

//-----------------------------------------------------------------------------
//	Mircosoft debug information
//-----------------------------------------------------------------------------

using namespace iso;

//-----------------------------------------------------------------------------
// symbol_gatherer: find dependencies
//-----------------------------------------------------------------------------

struct symbol_gatherer {
	struct dep_node : graph_edges<dep_node> {
		TI				ti;
		TI				udt;
		TI				nested;
		int				visited;
		const CV::SYMTYPE* sym;

		union {
			uint32		flags;
			struct {
				uint32	ref : 1, done_forward : 1;
			};
		};

		dep_node() : udt(0), nested(0), visited(0), sym(0), flags(0) {}

		// set if new sym2 is 'better' than sym
		void setsym(const CV::SYMTYPE* sym2, const char* tag) {
			if (sym) {
				auto	name = get_name(sym);
				if (tag == name)
					return;

				auto	name2 = get_name(sym2);
				if (name2 != tag && (!name.find(':') || name2.find(':')))
					return;
			}
			sym = sym2;
		}
	};

	PDB& pdb;
	TI							minTI;
	TI							ignoreTI		= 1;
	dep_node*					current_node	= nullptr;
	dynamic_array<dep_node>		entries;
	hash_map<uint32, const CV::SYMTYPE*>	symbol_hash;
	bitarray<CV::LF_ID_LAST>	unhandled_type;
	bitarray<CV::S_RECTYPE_MAX>	unhandled_sym;

	void	add_edge(dep_node* n, dep_node* p) {
		for (auto& i : n->outgoing) {
			if (i == p)
				return;
		}
		n->add_edge(p);
	}

	dep_node* get_node(TI ti) {
		return ti >= minTI ? &entries[ti - minTI] : 0;
	}

	void	procTI(TI ti) {
		if (ti == ignoreTI)
			return;

		if (auto node = get_node(ti)) {
			if (current_node && (!node->nested || node->nested != ignoreTI))
				add_edge(current_node, node);

			if (node->visited++ == 0) {
				auto	type = pdb.GetType(ti);
				if (UDT udt = type) {
					if (TI ti2 = pdb.LookupUDT(udt.name)) {
						node->udt = ti2;
						if (ti2 != ti) {
							auto node2 = get_node(ti2);
							add_edge(node, node2);
							if (node2->visited++ == 0)
								save(ignoreTI, ti2), save(current_node, node2), CV::process<void>(pdb.GetType(ti2), *this);
						}
					}
				}
				save(current_node, node), CV::process<void>(type, *this);
			}
		}
	}

	void	proc_fieldlist(TI ti) {
		if (ti) {
			auto* list = pdb.GetType(ti)->as<CV::FieldList>();
			ISO_ASSERT(list->leaf == CV::LF_FIELDLIST || list->leaf == CV::LF_FIELDLIST_16t);
			for (auto& i : list->list())
				CV::process<void>(&i, *this);
		}
	}

	template<typename T> void	operator()(const T& t, bool) {}

	//---------------------------------
	// SYMBOLS
	//---------------------------------

	void	operator()(const CV::UDTSYM& t) {
		TI	ti = t.typind;
		if (auto node = get_node(ti)) {
			if (!node->udt) {
				++node->visited;
				auto	type = pdb.GetType(ti);
				if (UDT udt = type) {
					if (TI ti2 = pdb.LookupUDT(udt.name)) {
						if (ti2 != ti) {
							node->udt = ti2;
							auto	node2 = get_node(ti2);
							add_edge(node2, node);
							node2->setsym(&t, udt.name);
						} else {
							node->udt = ti;
							node->setsym(&t, udt.name);
						}
					}
				}
				save(current_node, node), CV::process<void>(type, *this);
			}
		}
	}

	void	operator()(const CV::REFSYM2& t)		{ CV::process<void>(pdb.GetModule(t.imod).GetSymbol(t.ibSym), *this); }
	void	operator()(const CV::PROCSYM32& t)		{ if (!between(t.rectyp, CV::S_LPROC32_ID, CV::S_GPROCIA64_ID)) procTI(t.typind); } // else t.typind is ID
	void	operator()(const CV::CONSTSYM& t)		{ procTI(t.typind); }
	void	operator()(const CV::DATASYM32& t)		{ procTI(t.typind); }

	//catchall
	void	operator()(const CV::SYMTYPE& t)		{ unhandled_sym.set(t.rectyp); }

	//---------------------------------
	// TYPES
	//---------------------------------

	void	operator()(const CV::Enum& t)			{ proc_fieldlist(t.field); }
	void	operator()(const CV::Pointer& t)		{ current_node->ref = true; procTI(t.utype); }
	void	operator()(const CV::Proc& t)			{ procTI(t.rvtype); procTI(t.arglist); }
	void	operator()(const CV::MFunc& t)		{ procTI(t.rvtype); procTI(t.arglist); }
	void	operator()(const CV::BClass& t)		{ procTI(t.index); }
	void	operator()(const CV::Array& t)		{ procTI(t.elemtype); }
	void	operator()(const CV::Union& t)		{ proc_fieldlist(t.field); }
	void	operator()(const CV::Member& t)		{ procTI(t.index); }
	void	operator()(const CV::STMember& t)		{ procTI(t.index); }
	void	operator()(const CV::OneMethod& t)	{ procTI(t.index); }
	void	operator()(const CV::Modifier& t)		{ procTI(t.type); }
	void	operator()(const CV::VBClass& t)		{ procTI(t.index); }

	void	operator()(const CV::Method& t) {
		for (auto& i : pdb.GetType(t.mList)->as<CV::MethodList>()->list())
			procTI(i.index);
	}
	void	operator()(const CV::ArgList& t) {
		for (auto i : t.list())
			procTI(i);
	}
	void	operator()(const CV::Class& t) {
		proc_fieldlist(t.derived);
		proc_fieldlist(t.field);
	}
	void	operator()(const CV::NestType& t) {
		if (auto node = get_node(t.index)) {
			node->nested = ignoreTI;

			if (current_node)
				add_edge(node, current_node);

			procTI(t.index);
			//if (node->visited++ == 0) {
			//	auto	type = pdb.GetType(t.index);
			//	save(current_node, node), process<void>(type, *this);
			//}
		}
	}
	void	operator()(const CV::NestTypeEx& t) {
		if (auto node = get_node(t.index)) {
			node->nested = ignoreTI;

			if (current_node)
				add_edge(node, current_node);

			procTI(t.index);
			//if (node->visited++ == 0) {
			//	auto	type = pdb.GetType(t.index);
			//	save(current_node, node), process<void>(type, *this);
			//}
		}
	}

	//catchall
	void	operator()(const CV::Leaf& t) { unhandled_type.set(t.leaf); };

	//---------------------------------
	// ordering
	//---------------------------------

	enum {
		UNUSED = 0,
		PENDING = -1,
		PROCESSING = -2,
		PROCESSING_REF = -3,
		DONE_FORWARD = -4,
	};

	void visit(dynamic_array<TI>& L, dep_node& n) {
		for (auto m : n.outgoing) {
			if (m->visited < 0) {
				if (m->visited == PENDING) {
					m->visited = m->ref || n.visited == PROCESSING_REF ? PROCESSING_REF : PROCESSING;
					visit(L, *m);

				} else if (m->visited == PROCESSING_REF && !n.ref) {
					m->visited = PROCESSING;
					visit(L, *m);

				} else if (m->visited == PROCESSING && n.visited == PROCESSING_REF) {
					if (!n.done_forward) {
						L.push_back(n.ti | 0x80000000);
						n.done_forward = 1;
						visit(L, *m);
					}

				} else if (m->visited == PROCESSING && n.visited == PROCESSING) {
					if (n.ref)
						m->visited = PROCESSING_REF;
					visit(L, *m);
					//m->visited = PROCESSING;

				} else if (!m->done_forward) {
					// request forward reference
					L.push_back(m->ti | 0x80000000);
					m->done_forward = 1;
				}
			}
		}

		//mark n permanently
		if (n.visited < 0) {
			n.visited = L.size32() + 1;
			L.push_back(n.ti);
		}
	}

	dynamic_array<TI> get_order() {
		dynamic_array<TI>	L;
		for (auto& i : entries)
			if (i.visited)
				i.visited = PENDING;

		for (auto& i : entries) {
			if (i.visited == PENDING) {
				i.visited = i.ref ? PROCESSING_REF : PROCESSING;
				visit(L, i);
			}
		}
		return L;
	}

	//---------------------------------
	// misc
	//---------------------------------

	symbol_gatherer(PDB& pdb) : pdb(pdb), minTI(pdb.MinTI()), entries(pdb.Types().size()) {
		TI	ti = minTI;
		for (auto& i : entries)
			i.ti = ti++;
	}

	const CV::SYMTYPE* GetSym(TI ti, bool check_links) {
		if (auto node = get_node(ti)) {
			if (node->sym)
				return node->sym;

			if (check_links) {
				auto	type = pdb.GetType(ti);
				if (UDT::is(type)) {
					for (auto& i : node->outgoing) {
						if (i->sym)
							return i->sym;
					}
				}
			}
		}
		return 0;
	}

	void	AddSym(const CV::SYMTYPE& sym) {
		if (sym.rectyp == CV::S_CONSTANT)
			symbol_hash[crc32(get_name(&sym))] = &sym;
		CV::process<void>(&sym, *this);
	}

	const CV::TYPTYPE* GetType(TI ti, bool get_udt) {
		if (auto node = get_node(ti))
			return pdb.GetType(get_udt && node->udt ? node->udt : ti);
		return 0;
	}

	const CV::SYMTYPE* LookupSym(const char* name) const {
		return symbol_hash[crc32(name)].or_default();
	}
};

template<typename C, typename I = iterator_t<C>> static range<I> find_union(PDB &pdb, C &&c, int64 &union_max) {
	I		end				= c.end();
	I		union_end		= end;
	int64	union_offset	= maximum;
	int64	start_offset	= -1, prev_offset;

	union_max = 0;
	for (auto i = c.begin(); i != end;) {
		TypeLoc	loc		= get_typeloc(pdb, *i);
		if (loc.type() != TypeLoc::ThisRel) {
			++i;
			continue;
		}

		int64	offset	= loc.byte_offset();
		if (start_offset < 0)
			start_offset = prev_offset = offset;

		if (offset < prev_offset) {
			if (offset < union_offset) {
				union_end		= end;
				union_offset	= offset;
				union_max		= max(union_max, prev_offset);
			}
		}

		++i;
		if (loc.is_bitfield()) {
			while (i != end && get_typeloc(pdb, *i).is_bitfield())
				++i;
		}

		if (offset < union_max)
			union_end = i;

		prev_offset	= loc.byte_end();
	}

	if (union_offset == int64(maximum))
		return range<I>(end, end);

	I	union_begin	= c.begin();
	while (get_typeloc(pdb, *union_begin).byte_offset() != union_offset)
		++union_begin;

	return range<I>(union_begin, union_end);
}

template<typename C> static uint32 get_alignment(PDB &pdb, C &&c) {
	uint32	alignment	= 1;
	for (auto &i : c) {
		TypeLoc	loc		= get_typeloc(pdb, i);
		if (loc.type() == TypeLoc::ThisRel)
			alignment = align(alignment, loc.alignment);
	}
	return alignment;
}

//-----------------------------------------------------------------------------
// symbol_dumper
//-----------------------------------------------------------------------------

struct symbol_dumper {
	enum Mode {
		ROOT, STRUCT, ARGS, ALIGNING
	};
	struct Aligning {
		int				align_to;
		Aligning(int a = 0)	: align_to(a) {}

		const char *begin_line(string_accum &sa) {
			const char *line_start = 0;
			if (align_to) {
				line_start = sa.getp(align_to);
				sa.move(-align_to);
			}
			return line_start;
		}
		string_accum&	tab0(string_accum &sa, const char *line_start) {
			align_to = max(align_to, sa.getp() - line_start);
			return sa;
		}
		string_accum&	tab1(string_accum &sa, const char *line_start) const {
			if (align_to) {
				int	x = sa.getp() - line_start;
				if (x < align_to)
					return sa << repeat('\t', (align_to - x + 3) / 4);
			}
			return sa << ' ';
		}
		void	finish() {
			align_to	= align(align_to + 1, 4);
		}
	};

	PDB				&pdb;
	temp_accum		ta			= temp_accum(8192);
	const char		*name		= nullptr;
	Mode			mode		= ROOT;
	int				depth		= 0;
	int				lines		= 0;

	TI				minTI;
	bool			unhandled;
	symbol_gatherer	&gathered;
	string_accum	&sa;
	Aligning		aligning;
	const char		*line_start;
	uint8			current_access;

	dynamic_array<string>	scope;

	string_accum&	out()									{ return mode == ALIGNING ? ta : sa; }
	template<typename T> string_accum&	out(const T& t)		{ return out() << t; }

	string_accum&	newline()	{
		if (mode == ALIGNING) {
			string_accum &sa2 = ta;
			auto	p = sa2.getp();
			ISO_ASSERT(p >= line_start);
			sa2.move(line_start - p);
			return sa2;
		}

		++lines;
		sa << '\n' << repeat('\t', depth);
		line_start = aligning.begin_line(sa);
		return sa;
	}
	string_accum&	tab_align()	{
		if (mode == ARGS)
			return sa << ' ';
		if (mode == ALIGNING)
			return aligning.tab0(ta, line_start);
		return aligning.tab1(sa, line_start);
	}
	auto			start_align() {
		string_accum &sa2 = ta;
		int	n		= 256;
		line_start	= sa2.getp(n);
		sa2.move(-n);
		mode		= ALIGNING;
		return save(aligning.align_to, 0);
	}
	void			finish_align()	{
		aligning.finish();
		ta.reset();
	}

	string_accum&	open()		{ ++depth; lines = 0; return out(" {"); }
	string_accum&	close()		{ ISO_ASSERT(depth); --depth; if (lines) newline(); return out('}'); }

	void put_name() {
		if (name)
			tab_align() << name;
	}

	string make_scoped_name(const char *name) {
		string_builder	b;
		for (auto &i : scope)
			b << i << "::";
		b << name;
		return b;
	}

	const char *scoped_name(const char *name) {
		auto	s = scope.begin(), e = scope.end();
		while (s != e) {
			const char *colon = namespace_sep(name);
			if (!colon)
				break;
			if (str(name, colon) != *s) {
				if (anonymous_namespace(str(name, colon))) {
					name = colon + 2;
					continue;
				}
				if (s != scope.begin() || str(name, colon) != e[-1])
					break;
				s = e - 1;
			}
			name = colon + 2;
			++s;
		}
		if (str(name).begins("<unnamed-"))
			return 0;
		return name;
	}

	string member_name(const char *name) {
		if (name == scope.back()) {
			if (auto p = string_find(name, '<'))
				return string(name, p);
		}
		return name;
	}

	const char	*set_namespace(const char *name) {
		auto	s = scope.begin(), e = scope.end();
		while (s != e) {
			const char *colon = namespace_sep(name);
			if (!colon)
				break;
			if (str(name, colon) != *s)
				break;
			name = colon + 2;
			++s;
		}

		while (s != e) {
			scope.pop_back();
			--e;
			close();
		}

		while (const char *colon = namespace_sep(name)) {
			auto	&o = newline();
			if (anonymous_namespace(str(name, colon)))
				o << "namespace";
			else
				o << "namespace " << str(name, colon);
			open();
			scope.push_back(str(name, colon));
			name = colon + 2;
		}

		return name;
	}

	static bool is_conversion_operator(const char *name) {
		return str(name).begins("operator ");
	}

	void procTI(TI ti) {
		if (auto *sym = gathered.GetSym(ti, true)) {
			out(scoped_name(get_name(sym).begin()));
			put_name();

		} else if (auto node = gathered.get_node(ti)) {
			if (node->udt == 0 || (node->udt == ti && mode == ROOT)) {
				CV::process<void>(pdb.GetType(ti), *this);

			} else {
				auto	*type = pdb.GetType(node->udt);
				if (auto n = scoped_name(get_name(type).begin())) {
					out(member_name(n));
					put_name();
				} else {
					CV::process<void>(type, *this);
				}
			}

		} else {
			const char *prefix = dump_simple(out(), ti);
			if (name)
				tab_align() << prefix << name;
			else
				out(prefix);
			return;
		}

	}


	CV::FieldList	*get_fieldlist(TI ti) {
		if (ti) {
			CV::FieldList	*list = pdb.GetType(ti)->as<CV::FieldList>();
			ISO_ASSERT(list->leaf == CV::LF_FIELDLIST || list->leaf == CV::LF_FIELDLIST_16t);
			return list;
		}
		return 0;
	}

	string_accum& calltype(uint8 calltype) {
		switch (calltype) {
			case CV_CALL_NEAR_C:
			case CV_CALL_FAR_C:
			case CV_CALL_NEAR_PASCAL:
			case CV_CALL_FAR_PASCAL:
			case CV_CALL_NEAR_FAST:
			case CV_CALL_FAR_FAST:
			case CV_CALL_SKIPPED:
			case CV_CALL_NEAR_STD:
			case CV_CALL_FAR_STD:
			case CV_CALL_NEAR_SYS:
			case CV_CALL_FAR_SYS:
			case CV_CALL_THISCALL:
			case CV_CALL_MIPSCALL:
			case CV_CALL_GENERIC:
			case CV_CALL_ALPHACALL:
			case CV_CALL_PPCCALL:
			case CV_CALL_SHCALL:
			case CV_CALL_ARMCALL:
			case CV_CALL_AM33CALL:
			case CV_CALL_TRICALL:
			case CV_CALL_SH5CALL:
			case CV_CALL_M32RCALL:
			case CV_CALL_CLRCALL:
			case CV_CALL_INLINE:
			case CV_CALL_NEAR_VECTOR:
			case CV_CALL_RESERVED:
				break;
		}
		return out();
	}

	string_accum& access(uint8 a) {
		switch (a) {
			case 1:		return out("private");
			case 2:		return out("protected");
			case 3:		return out("public");
			default:	return out();
		}
	}

	void set_access(uint8 a) {
		if (mode != ALIGNING && a != current_access) {
			--depth;
			newline();
			++depth;
			access(a) << ':';
			current_access = a;
		}
	}

	bool method_props(uint8 m) {
		switch (m) {
			case CV::fldattr_t::Vanilla:
			case CV::fldattr_t::Intro:
				break;
			case CV::fldattr_t::Virtual:
				out("virtual ");
				break;
			case CV::fldattr_t::Purevirt:
			case CV::fldattr_t::Pureintro:
				out("virtual ");
				return true;
			case CV::fldattr_t::Static:
				out("static ");
				break;
			case CV::fldattr_t::Friend:
				out("friend ");
				break;
		}
		return false;
	};

	template<typename T> void	operator()(const T&, bool = false) { unhandled = true; }

	// SYMBOLS
	void	operator()(const CV::CONSTSYM &t) {
		auto	sname	= set_namespace(t.name());
		auto	&o		= newline();
		save(name, sname), procTI(t.typind);
		tab_align() << "= ";
		dump_constant(o, get_simple_type(pdb, t.typind), t.value);
	};

	void	operator()(const CV::UDTSYM &t) {
		TI	ti	= t.typind;

		if (ti < minTI) {
			const char *prefix = dump_simple(out("typedef"), ti);
			tab_align() << prefix << t.name;

		} else {
			auto	type = pdb.GetType(ti);
			if (type->leaf == CV::LF_ENUM) {
				save(name, nullptr), CV::process<void>(type, *this);

			} else {
				UDT		udt(type);
				const char *tag		= udt.name;
				const char *stag	= tag ? set_namespace(tag) : 0;
				const char *sname	= scoped_name(t.name);

				if (str(stag) == t.name) {
					save(name, nullptr), CV::process<void>(type, *this);

				} else if (namespace_sep(sname)) {
					save(name, nullptr), CV::process<void>(type, *this);
					out(';');
					sname = set_namespace(t.name);
					newline() << "typedef " << scoped_name(tag) << ' ' << sname;

				} else if (string_find(stag, '<')) {
					out("template<> ");
					save(name, nullptr), CV::process<void>(type, *this);
					out(';');
					newline() << "typedef " << stag << ' ' << sname;

				} else {
					out("typedef ");
					save(name, sname), CV::process<void>(type, *this);
				}
			}
		}
	}

	void	operator()(const CV::REFSYM2 &t) {
		unhandled = !char_set::wordchar.test(t.name[0]) || !char_set::wordchar.test(rnamespace_sep(t.name)[0]);
		if (!unhandled) {
			auto	&mod	= pdb.GetModule(t.imod);
			auto	sym		= mod.GetSymbol(t.ibSym);
			CV::process<void>(sym, *this);
		}
	}

	void	operator()(const CV::PROCSYM32 &t) {
		const CV::TYPTYPE *type = pdb.GetType(t.typind);
		if (type && type->leaf == CV::LF_MFUNCTION) {
			unhandled = true;
			return;
		}
		const char *sname	= set_namespace(t.name);
		newline();
		save(name, sname), procTI(t.typind);
	}

	// TYPES

	void	operator()(const CV::Pointer &t) {
		if (mode == ALIGNING) {
			procTI(t.utype);
			return;
		}

		static const char * const decorations[] = {
			"*",
			"&",
			"::*",
			"::*",
			"&&",
		};

		string	name2	= str(decorations[t.attr.ptrmode]) + name;

		if (t.attr.isvolatile)
			name2 = "volatile " + name2;
		if (t.attr.isconst)
			name2 = "const " + name2;
		if (t.attr.isunaligned)
			name2 = "unaligned " + name2;

		//save(mode, ARGS),
		save(name, name2), procTI(t.utype);
	}
	void	operator()(const CV::Array &t) {
		if (size_t elemSize = pdb.GetTypeSize(t.elemtype))
			save(name, (buffer_accum<256>() << name << '[' << (int64)t.size / elemSize << ']').term()), procTI(t.elemtype);
		else
			save(name, (buffer_accum<256>() << name << "[]").term()), procTI(t.elemtype);

	}
	void	operator()(const CV::Proc &t) {
		if (mode == ALIGNING) {
			procTI(t.rvtype);
			return;
		}
		if (name && (!char_set::wordchar.test(name[0]) || !char_set::wordchar.test(name[strlen(name) - 1])))
		//if (string_find(name, ~char_set::identifier))
			save(name, str("(") + name + ")"), save(mode, ARGS), procTI(t.rvtype);
		else
			save(mode, ARGS), procTI(t.rvtype);

		calltype(t.calltype);
		procTI(t.arglist);
	}

	void	operator()(const CV::BClass &t) {
		if (t.attr.access != current_access)
			access(t.attr.access) << ' ';
		procTI(t.index);
	}
	void	operator()(const CV::Enumerate &t) {
		out(t.name());
		tab_align() << "= " << ifelse(t.value.kind() == CV::Value::Int, (int64)t.value, "<something odd>");
	}

	void	operator()(const CV::Member &t) {
		set_access(t.attr.access);
		newline();
		save(name, t.name()), procTI(t.index);
	}

	void	operator()(const CV::STMember &t) {
		set_access(t.attr.access);
		auto	&o = newline() << "static ";
		save(name, t.name), procTI(t.index);

		if (auto *sym = gathered.LookupSym(make_scoped_name(t.name))) {
			auto	csym = sym->as<CV::CONSTSYM>();
			dump_constant(o << " = ", get_simple_type(pdb, csym->typind), csym->value);
		}
	}

	void	operator()(const CV::MFunc &t) {
		if (mode == ALIGNING) {
			procTI(t.rvtype);
			return;
		}
		if (t.funcattr.ctor)
			out(member_name(name));
		else if (name[0] == '~')
			out("~") << member_name(name + 1);
		else if (is_conversion_operator(name))
			out(name);
		else
			save(mode, ARGS), procTI(t.rvtype);

		calltype(t.calltype);
		procTI(t.arglist);

		if (auto *ths = pdb.GetType(t.thistype)) {
			if (ths->leaf == CV::LF_POINTER) {
				ths = pdb.GetType(ths->as<CV::Pointer>()->utype);
				if (ths->leaf == CV::LF_MODIFIER) {
					auto	attr = ths->as<CV::Modifier>()->attr;
					if (attr.MOD_const)
						out(" const");
					if (attr.MOD_volatile)
						out(" volatile");
				}
			}
		}
	}
	void	operator()(const CV::Method &t) {
		int	j = 0;
		for (auto &i : pdb.GetType(t.mList)->as<CV::MethodList>()->list()) {
			if (j++)
				out(';');
			set_access(i.attr.access);
			newline();
			bool	pure = method_props(i.attr.mprop);
			save(name, t.name), procTI(i.index);
			if (pure)
				out(" = 0");
		}
	}

	void	operator()(const CV::OneMethod &t) {
		set_access(t.attr.access);
		newline();
		bool	pure = method_props(t.attr.mprop);
		save(name, t.name()), procTI(t.index);
		if (pure)
			out(" = 0");
	}

	void	operator()(const CV::NestType &t) {
#if 1
		if (t.index < minTI) {
			const char *prefix = dump_simple(newline() << "typedef ", t.index);
			tab_align() << prefix << scoped_name(t.name);

		} else {
			auto	*type = pdb.GetType(t.index);
			UDT		udt(pdb.GetType(t.index));
			if (!udt.kind) {
				newline() << "typedef ";
				save(name, scoped_name(t.name)), CV::process<void>(type, *this);

			} else if (mode == STRUCT) {

				if (!str(udt.name).begins(make_scoped_name(0))) {
					unhandled = true;
					return;
				}

				if (auto node = gathered.get_node(t.index)) {
					mode = ROOT;
					auto	saved_name	= save(name, nullptr);
					newline();
					if (node->udt && node->udt != t.index) {
						type	= pdb.GetType(node->udt);
						if (get_name(type) != udt.name) {
							out("typedef ");
							name = udt.name;
						}
					}
					save(node->udt, 0), CV::process<void>(type, *this);
					mode = STRUCT;
				}
			}
		}
#else
		newline() << "typedef ";
		if (mode == ALIGNING)
			procTI(t.index);
		else
			save(mode, ARGS), save(name, sname), procTI(t.index);
#endif
	}

	void	operator()(const CV::NestTypeEx &t) {
		if (const char *sname = scoped_name(t.name)) {
			set_access(t.attr.access);
			newline();
			save(name, sname), procTI(t.index);
		} else {
			unhandled = true;
		}
	}

	void dump_field(const CV::Leaf *p) {
		unhandled = false;
		CV::process<void>(p, *this);
		if (!unhandled)
			out(';');
	}

	template<typename C> void dump_elements(C &&c, int64 start_offset, int &padding) {
		if (!c.empty()) {
			if (c.size() == 1) {
				dump_field(&c.front());

			} else {
				newline() << "struct";
				open();
				dump_struct(c, start_offset, padding);
				close() << ';';
			}
		}
	}

	template<typename C> void dump_union(C &&c, int &padding) {
		int64	start_offset	= get_typeloc(pdb, c.front()).byte_offset();
		auto	begin			= c.begin(), end = c.end();
		for (auto i = nth(begin, 1); i != end; ++i) {
			if (i->leaf == CV::LF_BCLASS || i->leaf == CV::LF_BINTERFACE)
				continue;
			int64	offset	= get_typeloc(pdb, *i).byte_offset();
			if (offset == start_offset) {
				dump_elements(make_range(begin, i), start_offset, padding);
				begin = i;
			}
		}
		dump_elements(make_range(begin, end), start_offset, padding);
	}

	void struct_padding(int64 offset, uint32 alignment, int64 prev_offset, int &padding) {
		if (alignment) {
			int64	want_offset	= align(prev_offset, alignment);
			if (offset > want_offset) {
				newline() << "char";
				tab_align() << "_pdb_padding" << padding++ << "[" << offset - want_offset << "];";
			}
		}
	}

	template<typename C> void dump_struct(C &&c, int64 start_offset, int &padding) {
		int64 prev_offset = start_offset;
		for (auto i = c.begin(), end = c.end(); i != end;) {

			while (i != end && (get_typeloc(pdb, *i).type() != TypeLoc::ThisRel || i->leaf == CV::LF_BCLASS || i->leaf == CV::LF_BINTERFACE)) {
				if (i->leaf == CV::LF_BCLASS) {
					auto	*base = i->template as<CV::BClass>();
					prev_offset = (int64)base->offset + pdb.GetTypeSize(base->index);
				} else if (i->leaf != CV::LF_BINTERFACE) {
					dump_field(&*i);
				}
				++i;
			}

			int64	union_max;
			auto	u			= find_union(pdb, make_range(i, end), union_max);
			int		prev_bit	= -1;

//			if (prev_offset == 0)
//				prev_offset	= get_typeloc(pdb, *i).byte_offset();

			for (auto &j : make_range(i, u.begin())) {
				if (j.leaf == CV::LF_BCLASS || j.leaf == CV::LF_BINTERFACE)
					continue;

				TypeLoc	loc	= get_typeloc(pdb, j);

				if (loc.type() == TypeLoc::ThisRel) {
					if (loc.is_bitfield()) {
						if (prev_bit < 0)
							prev_bit = 0;
						uint32	bit = loc.bit_offset() % (loc.alignment * 8);
						if (bit > prev_bit) {
							newline() << "uint32";
							tab_align() << "_bit_padding" << padding++ << ":" << bit - prev_bit << ";";
						}
						prev_bit = bit + uint32(loc.size);

					} else {
						struct_padding(loc.byte_offset(), loc.alignment, prev_offset, padding);
						prev_bit = -1;
					}

					prev_offset	= loc.byte_end();
				}

				dump_field(&j);
			}

			if (!u.empty()) {
				struct_padding(get_typeloc(pdb, u.front()).byte_offset(), get_alignment(pdb, u), prev_offset, padding);
				newline() << "union";
				open();
				dump_union(u, padding);
				close() << ';';
				prev_offset = union_max;
			}
			i = u.end();
		}
	}

	void	operator()(const CV::Class &t) {
		auto	old_access	= save(current_access, CV_private);
		switch (t.leaf) {
			case CV::LF_CLASS:		out("class"); break;
			case CV::LF_STRUCTURE:	out("struct"); current_access = CV_public; break;
			case CV::LF_INTERFACE:	out("interface"); break;
		}

		const char *sname = scoped_name(t.name());
		if (sname)
			out(' ') << sname;

		if (mode == ROOT || (mode == STRUCT && !sname)) {
			int	j = 0;
			if (auto *list = get_fieldlist(t.derived)) {
				for (auto &i : list->list()) {
					out(j++ ? ", " : " : ");
					CV::process<void>(&i, *this);
				}
			}

			if (auto *list = get_fieldlist(t.field)) {
				mode	= ARGS;
				for (auto &i : list->list()) {
					if (i.leaf == CV::LF_BCLASS || i.leaf == CV::LF_BINTERFACE) {
						out(j++ ? ", " : " : ");
						save(name, nullptr), CV::process<void>(&i, *this);
					}
				}

				open();
				scope.push_back(sname);

				auto	save_align	= start_align();
				for (auto &i : list->list()) {
					if (i.leaf != CV::LF_BCLASS && i.leaf != CV::LF_BINTERFACE)
						CV::process<void>(&i, *this);
				}
				finish_align();

				mode	= STRUCT;
				int		padding = 0;
				dump_struct(list->list(), 0, padding);
				mode	= ROOT;

				scope.pop_back();
				close();
			} else {
				out(" {}");
			}
		}
		put_name();
	}

	void	operator()(const CV::Union &t) {
		auto	old_access	= save(current_access, CV_public);

		const char *sname = scoped_name(t.name());
		out("union ") << sname;

		if (mode == ROOT || (mode == STRUCT && !sname)) {
			if (auto *list = get_fieldlist(t.field)) {
				open();
				scope.push_back(sname);

				auto	save_align	= start_align();
				for (auto &i : list->list())
					CV::process<void>(&i, *this);
				finish_align();

				mode		= STRUCT;
				int	padding	= 0;
				dump_union(list->list(), padding);
				mode		= ROOT;
				scope.pop_back();
				close();
			}
		}

		put_name();
	}

	void	operator()(const CV::Enum &t) {
		out("enum ") << scoped_name(t.name);

		if (mode == ROOT) {
			open();
			if (auto *list = get_fieldlist(t.field)) {

				auto	save_align	= start_align();
				for (auto &i : list->list()) {
					newline();
					CV::process<void>(&i, *this);
				}
				finish_align();

				mode		= ROOT;
				for (auto &i : list->list()) {
					newline();
					CV::process<void>(&i, *this);
					out(',');
				}
			}
			close();
		}

		put_name();
	}

	void	operator()(const CV::ArgList &t) {
		auto	old_name	= save(name, nullptr);
		auto	old_mode	= save(mode, ARGS);
		auto	&o			= out('(');
		int		j			= 0;
		for (auto i : t.list()) {
			if (j++)
				o << ", ";
			procTI(i);
		}
		o << ')';
	}
	void	operator()(const CV::Modifier &t) {
		if (t.attr.MOD_const)
			out("const ");
		if (t.attr.MOD_volatile)
			out("volatile ");
		if (t.attr.MOD_unaligned)
			out("unaligned ");
		procTI(t.type);
	}
	void	operator()(const CV::Bitfield &t) {
		procTI(t.type);
		out(':') << t.length;
	}

	symbol_dumper(PDB &pdb, symbol_gatherer &gathered, string_accum &sa) : pdb(pdb), minTI(pdb.MinTI()), gathered(gathered), sa(sa) {}
};
//}	// namespace CV

void WriteHeaderFile(PDB &pdb, const char *fn) {
	symbol_gatherer	gathered(pdb);

	for (auto &sym : pdb.Symbols())
		gathered.AddSym(sym);

	for (auto &&typ : pdb.Types())
		gathered.procTI(typ.t);

	for (auto i : gathered.unhandled_type.where(true))
		ISO_TRACEF("type: 0x%x\n", i);
	for (auto i : gathered.unhandled_sym.where(true))
		ISO_TRACEF("sym:0x%x\n", i);

	uint32	stop	= ~0;
	stream_accum<FileOutput, 1024>	accum(fn);
	symbol_dumper	dumper(pdb, gathered, accum);

	// UDT
	for (auto i : gathered.get_order()) {
		bool	forward = !!(i & 0x80000000);
		i &= 0x7fffffff;

		ISO_ASSERT(i!=stop);

		auto	type	= pdb.GetType(i);
		auto	node	= gathered.get_node(i);

		if (auto sym = node->sym) {
			UDT		udt(type);
			if (udt.name)
				dumper.set_namespace(udt.name);

			accum << '\n';
			dumper.newline();

			if (forward) {
				accum.format("// TI = 0x%x - forward reference", i);
				dumper.newline();
				dumper.mode = dumper.STRUCT;
				save(node->sym, nullptr), CV::process<void>(sym, dumper);
			} else {
				accum.format("// TI = 0x%x", i);
				dumper.newline();
				dumper.mode = dumper.ROOT;
				save(node->sym, nullptr), CV::process<void>(sym, dumper);
			}
			accum << ";";

		} else if (UDT::is(type) && !node->nested && (forward || node->udt == 0 || node->udt == i)) {
			UDT		udt(type);
			if (udt.name) {
				if (str(udt.name) == "<unnamed-tag>")
					continue;
				const char *sep = rnamespace_sep(udt.name);
				if (sep != udt.name) {
					auto	prefix = count_string(udt.name, sep - 2);
					if (prefix.find("<unnamed-"))
						continue;
					if (pdb.LookupUDT(prefix))
						continue;
				}

				dumper.set_namespace(udt.name);
			}

			accum << '\n';
			dumper.newline();
			if (forward) {
				static const char *tags[] = { 0, "typedef", "enum", "struct", "union", "class" };
				accum.format("// TI = 0x%x - forward reference", i);
				dumper.newline();
				accum << tags[udt.kind] << ' ' << udt.name;
			} else {
				accum.format("// TI = 0x%x", i);
				dumper.newline();
				dumper.mode = dumper.ROOT;
				dumper.procTI(i);
			}
			accum << ";";

/*		} else if (type->leaf == LF_POINTER) {
			if (auto udt = UDT(pdb.GetType(type->as<Pointer>()->utype))) {
				static const char *tags[] = { 0, "typedef", "enum", "struct", "union", "class" };

				accum << '\n';
				dumper.newline();
				accum.format("// TI = 0x%x - forward reference", i);
				dumper.newline();
				accum << tags[udt.kind] << ' ' << udt.name << ";";
			}

*/		} else if (forward && !node->udt) {
			if (auto name = get_name(type)) {
				accum << '\n';
				dumper.newline();
				accum << "// TI = " << formatted(i, FORMAT::CFORMAT|FORMAT::HEX) << ' ' << name << " - forward reference";
			}
		}
	}

	int	i = 0;
	for (auto &sym : pdb.Symbols()) {
		switch (sym.rectyp) {
			case CV::S_UDT:
				break;

			case CV::S_CONSTANT: {
				auto	*cnst = sym.as<CV::CONSTSYM>();
				auto	*type = pdb.GetType(cnst->typind);
				if (type && type->leaf == CV::LF_ENUM)
					break;
				dynamic_array<count_string> matches;
				const char *name = cnst->name();
				const char *sep	 = rnamespace_sep(name);
				if (sep != name && pdb.LookupUDT(count_string(name, sep - 2)))
					break;

			}
			// fall through
			default:
				if (pdb.GetSymbolTI(&sym)) {
					dumper.mode			= dumper.STRUCT;
					dumper.unhandled	= false;
					CV::process<void>(&sym, dumper);
					if (!dumper.unhandled) {
						accum << "; // index = " << i;
						if (sym.rectyp == CV::S_PROCREF || sym.rectyp == CV::S_LPROCREF) {
							auto	t	= sym.as<CV::REFSYM2>();
							auto	sym	= pdb.GetModule(t->imod).GetSymbol(t->ibSym);
							if (is_any(sym->rectyp, CV::S_GPROC32, CV::S_LPROC32, CV::S_GPROC32_ID, CV::S_LPROC32_ID)) {
								auto	t = sym->as<CV::PROCSYM32>();
								accum << " @ " << t->addr.seg << ":0x" << hex(t->addr.off);
							}
						}
					}
				}
		}
		++i;
	}
	/*
	for (auto &mod : pdb.Modules()) {
		for (auto &sym : mod.Symbols()) {
			if (pdb.GetSymbolTI(&sym)) {
				dumper.mode			= dumper.STRUCT;
				dumper.unhandled	= false;
				CV::process<void>(&sym, dumper);
				if (!dumper.unhandled) {
					accum << "; // index = " << i;
					if (sym.rectyp == S_PROCREF || sym.rectyp == S_LPROCREF) {
						auto	t	= sym.as<REFSYM2>();
						auto	sym	= pdb.GetModule(t->imod).GetSymbol(t->ibSym);
						if (is_any(sym->rectyp, S_GPROC32, S_LPROC32, S_GPROC32_ID, S_LPROC32_ID)) {
							auto	t = sym->as<PROCSYM32>();
							accum << " @ " << t->seg << ":0x" << hex(t->off);
						}
					}
				}
			}
		}
	}
	*/
	dumper.set_namespace(nullptr);
}

//-----------------------------------------------------------------------------
// ISO adaptors
//-----------------------------------------------------------------------------

struct ISO_PDB : PDB {
	ISO_PDB(PDB&& pdb) : PDB(move(pdb)) {}
	auto	GlobalSymbols()	const {	return make_split_range<1000>(PDB::GlobalSymbols()); }
//	auto	PublicSymbols()	const { return make_split_range<1000>(PDB::PublicSymbols()); }
	auto	PublicSymbols()	const { return with_param(psgs.buckets(), (const void*)syms); }
	auto	Symbols()		const { return make_split_range<1000>(PDB::Symbols()); }
	auto	Types()			const { return make_split_range<1000>(PDB::Types()); }
};

namespace ISO {
tag2	_GetName(const MOD &mod) { return mod.Module(); }
}

ISO_DEFCOMPV(coff::SECTION_HEADER, Name,VirtualSize,VirtualAddress,SizeOfRawData,PointerToRawData,PointerToRelocations,PointerToLinenumbers,NumberOfRelocations,NumberOfLinenumbers,Characteristics);
ISO_DEFUSER(IMAGE_SECTION_HEADER, coff::SECTION_HEADER);


//ISO_DEFUSERCOMPV(DBI::DebugHeader,data);
//ISO_DEFUSERCOMPV(DBI::DebugHeader, FPOData, ExceptionData, FixupData, OmapToSrcData, OmapFromSrcData, SectionHeaderData, TokenRIDMap, Xdata, Pdata, NewFPOData, OriginalSectionHeaderData);

ISO_DEFUSERCOMPV(SC,		isect,off,cb,characteristics,imod,data_crc,reloc_crc);
ISO_DEFUSERCOMPV(SC2,		isect,off,cb,characteristics,imod,data_crc,reloc_crc,isect_coff);
ISO_DEFUSERCOMPV(MOD,		Module,ObjFile,Symbols,Lines,SubStreams);
ISO_DEFUSERCOMPV(ISO_PDB,	Modules,GlobalSymbols,PublicSymbols,Symbols,Sections,sec_contribs,secmap,Types);

ISO_ptr<void> MakeISO_PDB(PDB &&pdb) {
	return ISO::MakePtr<ISO_PDB>(0, move(pdb));
}

//-----------------------------------------------------------------------------
// FileHandler
//-----------------------------------------------------------------------------

class MSFFileHandler : public FileHandler {
	const char*		GetDescription() override { return "MSF"; }
	int				Check(istream_ref file) override {
		file.seek(0);
		MSF::header	header;
		return file.read(header) && header.type() ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}
	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		MSF::EC		error;
		ref_ptr<MSF::reader>	msf = new MSF::reader(file, &error);
		if (error)
			return ISO_NULL;


		PDBinfo		info;
		info.load(msf, snPDB);

		ISO_ptr<anything>	p(id);
		for (auto i : msf->Streams()) {
			const char *name = info.StreamName(i);
			MSF::stream_reader	file2(msf, i);
			p->Append(ISO::MakePtr(name, malloc_block::unterminated(file2)));
		}
		return p;
	}
} msf;


class PDB2FileHandler : public FileHandler {
	const char*		GetExt() override { return "pdb"; }
	const char*		GetDescription() override { return "Visual Studio Debug (hacked)"; }
	int				Check(istream_ref file) override {
		file.seek(0);
		MSF::header	header;
		return file.read(header) && header.type() ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}
	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		MSF::EC		error;
		ref_ptr<MSF::reader>	msf = new MSF::reader(file, &error);
		if (error)
			return ISO_NULL;

		PDBinfo		info;
		if (info.load(msf, snPDB)) {
			PDB	pdb;
			if (pdb.load(info, msf))
				return ISO::MakePtr<ISO_PDB>(id, move(pdb));
		}

		ISO_ptr<anything>	p(id);
		for (auto i : msf->Streams()) {
			const char *name = info.StreamName(i);
			MSF::stream_reader	file2(msf, i);
			p->Append(ISO::MakePtr(name, malloc_block::unterminated(file2)));
		}

		return p;
	}

	ISO_ptr<void>	ReadWithFilename(tag id, const filename &fn) override {
		MSF::EC		error;
		ref_ptr<MSF::reader>	msf = new MSF::reader(lvalue(FileInput(fn)), &error);
		if (error)
			return ISO_NULL;

		PDBinfo		info;
		if (info.load(msf, snPDB)) {
			PDB	pdb;
			if (pdb.load(info, msf)) {
			#if 1
				//WriteHeaderFile(pdb, filename(fn).set_ext("h"));
				return ISO::MakePtr<ISO_PDB>(id, move(pdb));
			#else
				ISO_ptr<ISO_openarray<ISO_ptr<uint64> >> labels(id);
				for (auto &mod : pdb.Modules()) {
					for (auto &sym : mod.Symbols()) {
						switch (sym.rectyp) {
							case S_LABEL32: {
								auto	*p = sym.as<LABELSYM32>();
								labels->Append(ISO_ptr<uint64>(p->name, p->off + 0x1000));
								break;
							}

							case S_LPROC32:
							case S_GPROC32: {
								auto	*p = sym.as<PROCSYM32>();
								labels->Append(ISO_ptr<uint64>(p->name, p->off + 0x1000));
								break;
							}
						}
					}
				}
				return labels;
			#endif
			}
		}

		ISO_ptr<anything>	p(id);
		for (auto i : msf->Streams()) {
			const char *name = info.StreamName(i);
			MSF::stream_reader	file2(msf, i);
			p->Append(ISO::MakePtr(name, malloc_block::unterminated(file2)));
		}

		return p;
	}
} pdb;



