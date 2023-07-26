#include "extra/identifier.h"
#include "dis_bitcode.h"

namespace bitcode {

DisassemblerBitcode	bitcode_dis;

static const auto	illegal	= ~(char_set::wordchar + char_set(".-"));
static const auto	escape	= char_set("\r\n\t\"\\") | ~char_set::print;


auto escapedString(const string &s) {
	return [s](string_accum &a) {
		a << '"';
		for (auto c : s) {
			if (escape(c))
				(a << '\\').format("%02X", c);
			else
				a << c;
		}
		return a << '"';
	};
}

auto escapeStringIfNeeded(const string &s) {
	return [s](string_accum &a) {
		if (s.find(illegal))
			a << escapedString(s);
		else
			a << s;
	};
}

string_accum& operator<<(string_accum &a, const Type *t) {
	if (t->name)
		return a << '%' << escapeStringIfNeeded(t->name);

	switch(t->type) {
		case Type::Void:		return a << "void";
		case Type::Int:			return a << 'i' << t->size;
		case Type::Float:
			switch(t->size) {
				case 16:		return a << "half";
				case 32:		return a << "float";
				case 64:		return a << "double";
				default:		return a << "fp" << t->size;
			}

		case Type::Vector:		return a << '<' << t->size << " x " << t->subtype << '>';
		case Type::Pointer:
			a << t->subtype;
			if (t->addrSpace != Type::AddrSpace::Default)
				a << " addrspace(" << t->addrSpace << ')';
			return a << '*';

		case Type::Array:		return a << '[' << t->size << " x " << t->subtype << ']';
		case Type::Function:	return a << t->subtype << '(' << separated_list(t->members) << ')';
		case Type::Struct:		return a << onlyif(t->packed, '<') << "{ " << separated_list(t->members) << " }" << onlyif(t->packed, '>');
		case Type::Metadata:	return a << "metadata";
		case Type::Label:		return a << "label";
		default:				return a << "unknown_type";
	}
}

string_accum& operator<<(string_accum &a, const AttributeGroup *g) {
	int	j = 0;
	for (uint64 p = g->params; p; p = clear_lowest(p)) {
		auto	i = AttributeIndex(lowest_set_index(p));
		a << onlyif(j++, ' ') << i;
		if (g->values[i].exists())
			a << '=' << g->values[i];
	}

	for (auto &i : g->strs) {
		a << onlyif(j++, ' ') << escapedString(i.a);
		if (i.b)
			a << '=' << escapedString(i.b);
	}

	return a;
}

void nameOrNumber(string_accum &a, const string &name, uint32 id) {
	if (name)
		a << '%' << escapeStringIfNeeded(name);
	else if (id != ~0)
		a << '%' << id;
}
string_accum& operator<<(string_accum &a, const Block *b) {
	nameOrNumber(a, b->name, b->id);
	return a;
}

string_accum& operator<<(string_accum &a, const Instruction *i) {
	nameOrNumber(a, i->name, i->id);
	return a;
}

string_accum& DumpConstant(string_accum &a, const Constant *c, bool with_types);

string_accum& operator<<(string_accum &a, const Value &v) {
	switch (v.kind()) {
		default:
			return a << "???";

		case Value::best_index<uint64>:
			return a << v.get<uint64>();

		case Value::best_index<const Metadata*>:
			ISO_ASSERT(0);

		case Value::best_index<const Function*>:
			return a << '@' << escapeStringIfNeeded(v.get_known<const Function*>()->name);

		case Value::best_index<const GlobalVar*>:
			return a << '@' << escapeStringIfNeeded(v.get_known<const GlobalVar*>()->name);

		case Value::best_index<const Constant*>:
			return DumpConstant(a, v.get_known<const Constant*>(), false);

		case Value::best_index<const Instruction*>:
			return a << v.get_known<const Instruction*>();

		case Value::best_index<const Block*>:
			return a << v.get_known<const Block*>();
	}
}

string_accum& DumpScalar(string_accum &a, const Type *type, const Value &v, bool with_types) {
	if (v.is<const Constant*>()) {
		auto	c = v.get_known<const Constant*>();
		DumpConstant(a << onlyif(with_types, make_pair(c->type, ' ')), c, with_types);
		return a;
	}

	if (with_types)
		a << type << ' ';

	if (auto name = v.GetName(false))
		return a << '@' << escapeStringIfNeeded(*name);

	switch (type->type) {
		case Type::Float:
			return a << type->get<float>(v.get<uint64>().or_default());
		case Type::Int: {
			uint64	val = v.get<uint64>().or_default();
			if (type->size == 1)
				return a << (val ? "true" : "false");
			return a << (int64)val;
		}
		default:
			return a << "null";
	}
}

string_accum& DumpArray(string_accum &a, const Type *type, const Value &value, bool with_types) {
	int	j = 0;
	for (auto &v : value.get<ValueArray>())
		DumpScalar(a << onlyif(j++, ", "), type, v, with_types);
	return a;
}

string_accum& DumpConstant(string_accum &a, const Constant *c, bool with_types) {
	if (c->value.is<string>())
		return a << c->value.get_known<string>();

	if (c->value.is<_none>())
		return a << "undef";

	if (c->value.is<const Eval*>()) {
		auto	eval = c->value.get_known<const Eval*>();

		if (is_cast(eval->op)) {
			return a << CastOperation(eval->op) << " (" << eval->arg << " to " << c->type << ')';

		} else if (is_unary(eval->op)) {
			return a << UnaryOperation(eval->op) << " (" << eval->arg << ')';

		} else if (is_binary(eval->op)) {
			return a << BinaryOperation(eval->op) << " (" << separated_list(eval->args()) << ')';

		} else switch (eval->op) {
			case Operation::GetElementPtr: {
				auto &args	= eval->args();
				return a << "getelementptr " << onlyif(eval->flags, "inbounds ") << '(' << args[0].GetType()->subtype << separated_list(args, 1) << ')';
			}
			case Operation::Select:			return a << "select (" << separated_list(eval->args()) << ')';
			case Operation::ExtractElement:	return a << "extractelement (" << separated_list(eval->args()) << ')';
			case Operation::InsertElement:	return a << "insertelement (" << separated_list(eval->args()) << ')';
			case Operation::ShuffleVector:	return a << "shufflevector (" << separated_list(eval->args()) << ')';
			case Operation::ICmp :			return a << "icmp " << IPredicate(eval->flags) << " (" << separated_list(eval->args()) << ')';
			case Operation::FCmp:			return a << "fcmp " << FPredicate(eval->flags) << " (" << separated_list(eval->args()) << ')';
			default:						ISO_ASSERT(0);
		}
	}

	if (c->type->is_scalar())
		return DumpScalar(a, c->type, c->value, false);

	if (c->value.is<_zero>())
		return a << "zeroinitializer";

	switch (c->type->type) {
		case Type::Vector:
			return DumpArray(a << '<', c->type->subtype, c->value, with_types) << '>';

		case Type::Array:
			return DumpArray(a << '[', c->type->subtype, c->value, with_types) << ']';

		case Type::Struct: {
			int	j = 0;
			a << "{ ";
			for (auto &v : c->value.get<ValueArray>()) {
				a << onlyif(j, ", ");
				DumpScalar(a, c->type->members[j], v, with_types);
				++j;
			}
			return a << " }";
		}

		default:
			return a.format("unsupported type %u", c->type->type);
	}
}

} // namespace bitcode

using namespace bitcode;

template<> const char *field_names<AttributeIndex>::s[]	= {
	"",
	"alignment",		"alwaysinline",			"byval",			"inlinehint",			"inreg",			"minsize",			"naked",			"nest",
	"noalias",			"nobuiltin",			"nocapture",		"noduplicate",			"noimplicitfloat",	"noinline",			"nonlazybind",		"noredzone",
	"noreturn",			"nounwind",				"optimizeforsize",	"readnone",				"readonly",			"returned",			"returnstwice",		"sext",
	"stackalignment",	"stackprotect",			"stackprotectreq",	"stackprotectstrong",	"structret",		"sanitizeaddress",	"sanitizethread",	"sanitizememory",
	"uwtable",			"zext",					"builtin",			"cold",					"optimizenone",		"inalloca",			"nonnull",			"jumptable",
	"dereferenceable",	"dereferenceableornull","convergent",		"safestack",			"argmemonly",
};

template<> struct field_names<FastMathFlags> { static field_bit_sep<' '> s[]; };
field_bit_sep<' '> field_names<FastMathFlags>::s[] = {
	{"",				(uint32)FastMathFlags::None					},
	{"fast",			(uint32)FastMathFlags::Fast					},
	{"fast",			(uint32)FastMathFlags::FastNan				},
	{"nnan",			(uint32)FastMathFlags::NoNaNs				},
	{"ninf",			(uint32)FastMathFlags::NoInfs				},
	{"nsz",				(uint32)FastMathFlags::NoSignedZeros		},
	{"arcp",			(uint32)FastMathFlags::AllowReciprocal		},
	{"contract",		(uint32)FastMathFlags::AllowContract		},
	{"afn",				(uint32)FastMathFlags::ApproxFunc			},
	{"reassoc",			(uint32)FastMathFlags::AllowReassoc			},
	{0}
};

template<> const char *field_names<UnaryOpcodes>::s[]	= {
	"fneg",
};

template<> const char *field_names<BinaryOpcodes>::s[]	= {
	"add",				"sub",			"mul",			"udiv",			"sdiv",			"urem",			"srem",				"shl",
	"lshr",				"ashr",			"and",			"or",			"xor",
	"div",				"rem",
};


template<> const char *field_names<FPredicate>::s[]	= {
	"false",			"oeq",			"ogt",			"oge",			"olt",			"ole",			"one",				"ord",
	"uno",				"ueq",			"ugt",			"uge",			"ult",			"ule",			"une",				"true",
};
template<> const char *field_names<IPredicate>::s[]	= {
	"eq",				"ne",			"ugt",			"uge",			"ult",			"ule",			"sgt",				"sge",			"slt",		"sle",
};

template<> const char *field_names<CastOpcodes>::s[]	= {
	"trunc",			"zext",			"sext",			"fptoui",		"fptosi",		"uitofp",		"sitofp",			"fptrunc",
	"fpext",			"ptrtoi",		"itoptr",		"bitcast",		"addrspacecast",
};


template<> const char *field_names<AtomicOrderingCodes>::s[]	= {
	"",
	"unordered",
	"monotonic",
	"acquire",
	"release",
	"acq_rel",
	"seq_cst",
	"<bad>",
};

template<> const char *field_names<RMWOperations>::s[]	= {
	"xchg",	"add",	"sub",	"and",	"nand",	"or",	"xor",	"max",
	"min",	"umax",	"umin",	"fadd",	"fsub",
};

template<> const char *field_names<Type::AddrSpace>::s[]	= {
	"Default",
	"DeviceMemory",
	"CBuffer",
	"GroupShared",
	"GenericPointer",
	"ImmediateCBuffer",
};

DECLARE_VALUE_ENUMS(Linkage);
field_value field_names<Linkage>::s[]	= {
	{"external",			(uint32)Linkage::External},
	{"private",				(uint32)Linkage::Private},
	{"internal",			(uint32)Linkage::Internal},
	{"linkonce",			(uint32)Linkage::LinkOnceAny},
	{"linkonce_odr",		(uint32)Linkage::LinkOnceODR},
	{"weak",				(uint32)Linkage::WeakAny},
	{"weak_odr",			(uint32)Linkage::WeakODR},
	{"common",				(uint32)Linkage::Common},
	{"appending",			(uint32)Linkage::Appending},
	{"extern_weak",			(uint32)Linkage::ExternalWeak},
	{"available_externally",(uint32)Linkage::AvailableExternally},
};

template<> const char *field_names<UnnamedAddr>::s[]	= {
	"",
	"local_unnamed_addr ",
	"unnamed_addr ",
};

Disassembler::State* DisassemblerBitcode::Disassemble(const_memory_block block, uint64 addr, SymbolFinder sym_finder) {
	StateDefault2	*state	= new StateDefault2;

	if (!~addr) {
		auto&			mod = *unconst((const Module*)block);
		Context(mod).Disassemble(state);
	} else {
		Module			mod;
		if (mod.read(memory_reader(block)))
			Context(mod).Disassemble(state);
	}
	return state;
}

DisassemblerBitcode::Context::Context(const Module& mod, uint32 flags, async_callback<custom_t> &&custom) : mod(mod), flags(flags), custom(move(custom)) {
}


_lister<>& operator<<(_lister<> &&a, const DITag *c) {
	if (c->tag != dwarf::TAG_base_type)
		a >> "tag: " << c->tag;
	if (c->name)
		a >> "name: " << escapedString(c->name);
	return a;
}

struct DebugInfoPrinter {
	const DisassemblerBitcode::Context	&ctx;
	string_accum &a;

	template<typename T> void put(string_accum& a, const T &t)	const { a << t; }
	void put(string_accum& a, const char *s)		const	{ a << escapedString(s);}
	void put(string_accum& a, MetaString s)			const	{ a << escapedString(*s);}
	void put(string_accum& a, MetadataRef m)		const	{ ctx.DumpMetaRef(a, m);}
	void put(string_accum& a, MetadataRef1 m)		const	{ ctx.DumpMetaRef(a, m);}

	template<typename T> auto getOpt(const char *tag, const T &v) const {
		return [this, tag, v](string_accum& a) {
			if (v)
				put(a << tag << ": ", v);
		};
	}

	template<typename T> auto notOpt(const char *tag, const T &v) const {
		return [this, tag, v](string_accum& a) {
			put(a << tag << ": ", v);
		};
	}

	void operator()(const DILocation *p) const {
		a	<< "!DILocation" << start_list()
			<< notOpt("line", p->line)
			>> getOpt("column", p->col)
			>> notOpt("scope", p->scope)
			>> getOpt("inlinedAt", p->inlined_at);
	}

	void operator()(const DIFile *p) const {
		a	<< "!DIFile" << start_list()
			<< notOpt("filename", p->file)
			>> notOpt("directory", p->dir);
	}

	void operator()(const DICompileUnit *p) const {
		a	<< "!DICompileUnit" << start_list()
			<< notOpt("language", p->lang)
			>> notOpt("file", p->file)
			>> getOpt("producer", p->producer)
			>> notOpt("isOptimized", p->optimised)
			>> getOpt("flags", p->flags)
			>> notOpt("runtimeVersion", p->runtime_version)
			>> getOpt("splitDebugFilename", p->split_debug_filename)
			>> notOpt("emissionKind", p->emission_kind)
			>> getOpt("enums", p->enums)
			>> getOpt("retainedTypes", p->retained_types)
			>> getOpt("subprograms", p->subprograms)
			>> getOpt("globals", p->globals)
			>> getOpt("imports", p->imports);
	}

	void operator()(const DIBasicType *p) const {
		a	<< "!DIBasicType" << start_list() << (const DITag*)p
			>> notOpt("size", p->size_bits)
			>> notOpt("align", p->align_bits)
			>> notOpt("encoding", p->encoding);
	}

	void operator()(const DIDerivedType *p) const {
		a	<< "!DIDerivedType" << start_list() << (const DITag*)p
			>> getOpt("scope", p->scope)
			>> getOpt("file", p->file)
			>> getOpt("line", p->line)
			>> notOpt("baseType", p->base)
			>> getOpt("size", p->size_bits)
			>> getOpt("align", p->align_bits)
			>> getOpt("offset", p->offset_bits)
			>> getOpt("flags", p->flags)
			>> getOpt("extraData", p->extra);
	}

	void operator()(const DICompositeType *p) const {
		a	<< "!DICompositeType" << start_list() << (const DITag*)p
			>> getOpt("scope", p->scope)
			>> getOpt("file", p->file)
			>> getOpt("line", p->line)
			>> getOpt("baseType", p->base)
			>> getOpt("size", p->size_bits)
			>> getOpt("align", p->align_bits)
			>> getOpt("offset", p->offset_bits)
			>> getOpt("flags", p->flags)
			>> getOpt("elements", p->elements)
			>> getOpt("templateParams", p->template_params);
	}

	void operator()(const DITemplateTypeParameter *p) const {
		a	<< "!DITemplateTypeParameter" << start_list()
			<< notOpt("name", p->name)
			>> notOpt("type", p->type);
	}

	void operator()(const DITemplateValueParameter *p) const {
		a	<< "!DITemplateValueParameter" << start_list()
			<< notOpt("name", p->name)
			>> notOpt("type", p->type)
			>> notOpt("value", p->value);
	}

	void operator()(const DISubprogram *p) const {
		auto	b	= a	<< "!DISubprogram" << start_list()
			<< getOpt("name", p->name)
			>> getOpt("linkageName", p->linkage_name)
			>> getOpt("scope", p->scope)
			>> getOpt("file", p->file)
			>> getOpt("line", p->line)
			>> getOpt("type", p->type)
			>> notOpt("isLocal", p->local)
			>> notOpt("isDefinition", p->definition)
			>> getOpt("scopeLine", p->scopeLine)
			>> getOpt("containingType", p->containing_type);
		if (p->virtuality)
			b << notOpt("virtuality", p->virtuality) << getOpt("virtualIndex", p->virtualIndex);

		b	<< getOpt("flags", p->flags)
			>> notOpt("isOptimized", p->optimised)
			>> getOpt("function", p->function)
			>> getOpt("templateParams", p->template_params)
			>> getOpt("declaration", p->declaration)
			>> getOpt("variables", p->variables);
	}

	void operator()(const DISubroutineType *p) const {
		a	<< "!DISubroutineType" << start_list()
			<< notOpt("types", p->types);
	}

	void operator()(const DIGlobalVariable *p) const {
		a	<< "!DIGlobalVariable" << start_list()
			<< notOpt("name", p->name)
			>> getOpt("linkageName", p->linkage_name)
			>> getOpt("scope", p->scope)
			>> getOpt("file", p->file)
			>> getOpt("line", p->line)
			>> getOpt("type", p->type)
			>> notOpt("isLocal", p->local)
			>> notOpt("isDefinition", p->definition)
			>> getOpt("variable", p->variable);
	}

	void operator()(const DILocalVariable *p) const {
		a	<< "!DILocalVariable" << start_list()
			<< notOpt("tag", p->tag)
			>> notOpt("name", p->name)
			>> getOpt("arg", p->arg)
			>> getOpt("scope", p->scope)
			>> getOpt("file", p->file)
			>> getOpt("line", p->line)
			>> getOpt("type", p->type)
			>> getOpt("flags", p->flags)
			>> getOpt("align", p->align_bits);
	}

	void operator()(const DIExpression *p) const {
		a << "!DIExpression(";
		int		op	= 0;
		int		j	= 0;
		for (auto& i : p->expr) {
			if (j++)
				a << ", ";

			if (op-- == 0) {
				a << (dwarf::OP)i;
				op = num_args((dwarf::OP)i);
			} else {
				a << i;
			}
		}
		a << ')';
	}

	void operator()(const DILexicalBlock *p) const {
		a	<< "!DILexicalBlock" << start_list()
			<< notOpt("scope", p->scope)
			>> getOpt("file", p->file)
			>> getOpt("line", p->line)
			>> getOpt("column", p->column);
	}

	void operator()(const DISubrange *p) const {
		a	<< "!DISubrange" << start_list()
			<< notOpt("count", p->count)
			>> getOpt("lowerBound", p->lower_bound);
	}

	void operator()(const DILexicalBlockFile *p) const {
		a << "!DILexicalBlockFile";
	}
	void operator()(const DINamespace *p) const {
		a << "!DINamespace" << start_list() << notOpt("name", p->name) >> notOpt("scope", p->scope)>> getOpt("file", p->file)>> getOpt("line", p->line);
	}
	void operator()(const DIGenericDebug *p) const {
		a << "!DIGenericDebug";
	}
	void operator()(const DIEnumerator *p) const {
		a << "!DIEnumerator" << start_list() << notOpt("name", p->name) >> notOpt("value", p->value);
	}
	void operator()(const DIObjcProperty *p) const {
		a << "!DIObjcProperty";
	}
	void operator()(const DIImportedEntity *p) const {
		a << "!DIImportedEntity";
	}
	void operator()(const DIModule *p) const {
		a << "!DIModule";
	}
	void operator()(const DIMacro *p) const {
		a << "!DIMacro";
	}
	void operator()(const DIMacroFile *p) const {
		a << "!DIMacroFile";
	}
	void operator()(const DIGlobalVariableExpression *p) const {
		a << "!DIGlobalVariableExpression";
	}
	void operator()(const DILabel *p) const {
		a << "!DILabel";
	}
	void operator()(const DIStringType *p) const {
		a << "!DIStringType";
	}
	void operator()(const DICommonBlock *p) const {
		a << "!DICommonBlock";
	}
	void operator()(const DIGenericSubrange *p) const {
		a << "!DIGenericSubrange";
	}

	void operator()(const DebugInfo *p) const {
		a	<< "???";
	}

	DebugInfoPrinter(const DisassemblerBitcode::Context &ctx, string_accum &a) : ctx(ctx), a(a) {}
};

string_accum& DisassemblerBitcode::Context::DumpMetaVal(string_accum &a, const Metadata *m) const {

	if (m->value.is<const DebugInfo*>()) {
		process<void>(m->value.get_known<const DebugInfo*>(), DebugInfoPrinter(*this, a));
		return a;
	}
	if (m->value.is<string>())
		return a << '!' << escapedString(m->value.get<string>());

	if (m->type) {
		if (const Instruction *i = m->value.get<const Instruction*>().or_default())
			return a << i->type << ' ' << i;

		if (const Constant *c = m->value.get<const Constant*>().or_default())
			return DumpConstant(a << c->type << ' ', c, true);

		if (auto name = m->value.GetName())
			//return a << m->value.GetType() << " @" << escapeStringIfNeeded(*name);
			return a << m->type << " @" << escapeStringIfNeeded(*name);

		return a << "null";
	}

	a << "!{";
	int	j = 0;
	for (auto i : m->value.get<ValueArray>())
		DumpMetaRef(a << onlyif(j++, ", "), i.get<const Metadata*>().or_default());
	return a << '}';
}

string_accum &DisassemblerBitcode::Context::DumpMetaRef(string_accum &a, const Metadata *m) const {
	if (!m)
		return a << "null";
	auto	r = metadata_ids[m];
	if (r.exists())
		return a << '!' << r;
	return DumpMetaVal(a, m);
}

string_accum &DisassemblerBitcode::Context::DumpArg(string_accum& a, const Value &v, bool with_types, const string& attr) {
	switch (v.kind()) {
		default:
			return a << "???";

		case Value::best_index<uint64>:
			return a << onlyif(with_types, "i32 ") << attr << v.get<uint64>();

		case Value::best_index<const Metadata*>: {
			a << onlyif(with_types, "metadata ") << attr;
			auto	m = v.get_known<const Metadata*>();
			if (m->value.is<const Constant*>()) {
				auto	c =  m->value.get_known<const Constant*>();
				if (c->value.is<_none>() || c->value.is<_zero>() || is_any(c->type->type,Type::Void, Type::Int, Type::Float, Type::Vector) || c->type->name.begins("class.matrix."))
					return DumpArg(a, m->value, with_types);

			} else if (m->value.is<const GlobalVar*>()) {
				return DumpArg(a, m->value, with_types);

			}
			return DumpMetaRef(a, m);
		}

		case Value::best_index<const Function*>:
			return a << attr << '@' << escapeStringIfNeeded(v.get_known<const Function*>()->name);

		case Value::best_index<const GlobalVar*>: {
			auto	g = v.get_known<const GlobalVar*>();
			return a << onlyif(with_types, make_pair(g->type, ' ')) << attr << '@' << escapeStringIfNeeded(g->name);
		}
		case Value::best_index<const Constant*>: {
			auto	c = v.get_known<const Constant*>();
			return DumpConstant(a << onlyif(with_types, make_pair(c->type, ' ')) << attr, c, with_types);
		}
		case Value::best_index<const Instruction*>: {
			auto	i = v.get_known<const Instruction*>();
			return a << onlyif(with_types, make_pair(i->type, ' ')) << attr << i;
		}
		case Value::best_index<const Block*>: {
			auto	b = v.get_known<const Block*>();
			return a << onlyif(with_types, "label ") << attr << b;
		}
	}
}

struct MetadataAssigner {
	DisassemblerBitcode::Context	&ctx;
	MetadataAssigner(DisassemblerBitcode::Context &ctx) : ctx(ctx) {}
	template<typename...T>	bool	write(T&&...t) {
		bool dummy[] = { add(t)...};
		return true;
	}
	template<typename T> bool	add(const T& t) { return false; }
	bool	add(const MetadataRef& c) {
		if (c.exists() && !c->is_constant())
			ctx.AssignMetaID(c);
		return true;
	}
	bool	add(const MetadataRef1& c) {
		if (c.exists() && !c->is_constant())
			ctx.AssignMetaID(c);
		return true;
	}
};

void DisassemblerBitcode::Context::AssignMetaID(const Metadata *m) {
	if (m && !m->is_constant() && !metadata_ids[m].exists()) {
		metadata_ids[m] = numbered_meta.size32();
		numbered_meta.push_back(m);

		if (auto di = m->value.get<const DebugInfo*>().or_default()) {
			process<void>(di, [w = MetadataAssigner(*this)](auto d) mutable { d->write(w); });

		} else {
			for (auto v : m->value.get<ValueArray>())
				AssignMetaID(v.get<const Metadata*>().or_default());
		}
	}
}

void DisassemblerBitcode::Context::AssignMetaID(const DILocation *loc) {
	if (!metadata_ids[loc].exists()) {
		metadata_ids[loc] = numbered_meta.size32();
		numbered_meta.push_back(loc);
		if (loc->scope)
			AssignMetaID(loc->scope);
		if (loc->inlined_at)
			AssignMetaID(loc->inlined_at);
	}
}

void put_align(string_accum& a, uint32 align) {
	if (align)
		a << ", align " <<  align;
};
auto put_volatile(bool vol) {
	return onlyif(vol, "volatile ");
}
auto put_syncscope(bool multi) {
	return onlyif(!multi, "syncscope(\"singlethread\") ");
}

void DisassemblerBitcode::Context::Disassemble(string_accum& a, Instruction& inst) {
	switch (auto op = inst.op) {
		case Operation::Nop:
			a << "??? ";
			break;

		case Operation::Call: {
			a << "call " << inst.type << " @" << escapeStringIfNeeded(inst.funcCall->name) << '(';
			size_t	j	= 0;
			for (auto& s : inst.args) {
				a << onlyif(j++, ", ");
				// see if we have param attrs for this param (param attrs start at index 1)
				string attr;
				if (auto at = inst.paramAttrs[j])
					attr << at << ' ';
				DumpArg(a, s, true, attr);
			}
			a << ')';
			if (auto at = inst.paramAttrs[AttributeGroup::FunctionSlot])
				a << " #" << get(attr_group_ids[at]);
			break;
		}
		case Operation::Trunc:
		case Operation::ZExt:
		case Operation::SExt:
		case Operation::FToU:
		case Operation::FToS:
		case Operation::UToF:
		case Operation::SToF:
		case Operation::FPTrunc:
		case Operation::FPExt:
		case Operation::PtrToI:
		case Operation::IToPtr:
		case Operation::Bitcast:
		case Operation::AddrSpaceCast:
			a << CastOperation(op) << ' ' << Arg(inst.args[0], true) << " to " << inst.type;
			break;

		case Operation::ExtractValue:
			a << "extractvalue " << Arg(inst.args[0], true) << separated_list(Args(inst.args.slice(1), false), 1);
			break;

		case Operation::FNeg:
			a	<< UnaryOperation(op) << ' '
				<< Arg(inst.args[0], true);
			break;

		case Operation::Add:
		case Operation::Sub:
		case Operation::Mul:
			a	<< BinaryOperation(op) << ' '
				<< onlyif(test_any(inst.int_flags, OverflowFlags::NoUnsignedWrap), "nuw ")
				<< onlyif(test_any(inst.int_flags, OverflowFlags::NoSignedWrap), "nsw ")
				<< inst.type << ' ' << separated_list(Args(inst.args, false));
			break;

		case Operation::UDiv:
		case Operation::SDiv:
		case Operation::URem:
		case Operation::SRem:
		case Operation::Shl:
		case Operation::Lshr:
		case Operation::Ashr:
			a	<< BinaryOperation(op) << ' '
				<< onlyif(test_any(inst.exact_flags, ExactFlags::Exact), "exact ")
				<< inst.type << ' ' << separated_list(Args(inst.args, false));
			break;

		case Operation::And:
		case Operation::Or:
		case Operation::Xor:
			a	<< BinaryOperation(op) << ' '
				<< inst.type << ' ' << separated_list(Args(inst.args, false));
			break;

		case Operation::FAdd:
		case Operation::FSub:
		case Operation::FMul:
		case Operation::FDiv:
		case Operation::FRem:
			a	<< 'f' << FBinaryOperation(op) << ' '
				<< onlyif(inst.float_flags != FastMathFlags::None, make_pair(inst.float_flags, ' '))
				<< inst.type << ' ' << separated_list(Args(inst.args, false));
			break;

		case Operation::Ret:
			a << "ret";
			if (inst.type)
				a << ' ' << inst.type;
			break;

		case Operation::Unreachable:
			a << "unreachable";
			break;

		case Operation::Alloca: {
			a << "alloca " << inst.type->subtype;
			put_align(a, inst.align);
			break;
		}

		case Operation::GetElementPtr:
			a << "getelementptr " << onlyif(inst.InBounds, "inbounds ") << inst.type << separated_list(Args(inst.args, true), 1);
			break;

		case Operation::Load:
			a << "load " << put_volatile(inst.Volatile) << inst.type << separated_list(Args(inst.args, true), 1);
			put_align(a, inst.align);
			break;

		case Operation::Store: {
			a << "store " << put_volatile(inst.Volatile) << Arg(inst.args[1], true) << ", " << Arg(inst.args[0], true);
			put_align(a, inst.align);
			break;
		}
		case Operation::FCmp:
			a	<< "fcmp "
				<< onlyif(inst.float_flags != FastMathFlags::None, make_pair(inst.float_flags, ' '))
				<< inst.fpredicate << ' ' << Arg(inst.args[0], true) << ", " << Arg(inst.args[1], false);
			break;

		case Operation::ICmp:
			a << "icmp " << inst.ipredicate << ' ' << Arg(inst.args[0], true) << ", " << Arg(inst.args[1], false);
			break;

		case Operation::Select:
			a << "select " << Arg(inst.args[2], true) << ", " << Arg(inst.args[0], true) << ", " << Arg(inst.args[1], true);
			break;

		case Operation::ExtractElement:
			a << "extractelement " << Arg(inst.args[0], true) << ", " << Arg(inst.args[1], true);
			break;

		case Operation::InsertElement:
			a << "insertelement " << Arg(inst.args[0], true) << ", " << Arg(inst.args[1], true) << ", " << Arg(inst.args[2], true);
			break;

		case Operation::ShuffleVector:
			a << "shufflevector " << Arg(inst.args[0], true) << ", " << Arg(inst.args[1], true) << ", " << Arg(inst.args[2], true);
			break;

		case Operation::InsertValue:
			a << "insertvalue " << Arg(inst.args[0], true) << ", " << Arg(inst.args[1], true) << separated_list(Args(inst.args.slice(2), false), 1);
			break;

		case Operation::Br:
			a << "br ";
			if (inst.args.size() > 1)
				a << Arg(inst.args[2], true) << ", " << Arg(inst.args[0], true) << ", " << Arg(inst.args[1], true);
			else
				a << Arg(inst.args[0], true);
			break;

		case Operation::Phi: {
			a << "phi " << inst.type;
			int	j = 0;
			for (auto *p = inst.args.begin(); p != inst.args.end(); p += 2)
				a << onlyif(j++, ',') << " [ " << Arg(p[0], false) << ", " << Arg(p[1], false) << " ]";
			break;
		}
		case Operation::Fence:
			a << "fence " << put_syncscope(inst.SyncScope) << inst.success;
			break;

		case Operation::LoadAtomic:
			a	<< "load atomic " << put_volatile(inst.Volatile) << inst.type
				<< separated_list(Args(inst.args, true), 1)
				<< ' ' << put_syncscope(inst.SyncScope) << inst.success;
			put_align(a, inst.align);
			break;

		case Operation::StoreAtomic:
			a	<< "store atomic " << put_volatile(inst.Volatile)
				<< Arg(inst.args[1], true) << ", " << Arg(inst.args[0], true)
				<< ", align " << inst.align;
			break;

		case Operation::CmpXchg: {
			a	<< "cmpxchg " << onlyif(inst.Weak, "weak ") << put_volatile(inst.Volatile)
				<< separated_list(Args(inst.args, false))
				<< ' ' << put_syncscope(inst.SyncScope) << inst.success << ' ' << inst.failure;
			put_align(a, inst.align);
			break;
		}

		case Operation::AtomicRMW: {
			a	<< "atomicrmw " << put_volatile(inst.Volatile) << inst.rmw << ' '
				<< separated_list(Args(inst.args, true))
				<< ' ' << put_syncscope(inst.SyncScope) << inst.success;
			put_align(a, inst.align);
			break;
		}
	}

	if (!(flags & SKIP_DEBUG)) {
		const DILocation	*loc = inst.debug_loc;
		if (loc)
			a << ", !dbg !" << get(metadata_ids[loc]);

		for (auto &i : inst.attachedMeta)
			a << ", !" << i.kind << " !" << get(metadata_ids[i.metadata]);

		if (inst.funcCall && inst.funcCall->name.begins("llvm.dbg.")) {
			int	index = inst.funcCall->name == "llvm.dbg.value" ? 2 : inst.funcCall->name == "llvm.dbg.declare" || inst.funcCall->name == "llvm.dbg.addr" ? 1 : 0;
			if (index) {
				DumpMetaVal(a << " ; var:" << escapedString(inst.args[index].get_known<const Metadata*>()->value.get_known<const DebugInfo*>()->get_name()) << ' ', inst.args[index + 1].get<const Metadata*>());
				if (loc && loc->scope)
					a << " func:" << escapedString(loc->scope->value.get_known<const DebugInfo*>()->get_name());
			}

		} else if (loc && loc->line) {
			a << " ; line:" << loc->line << " col:" << loc->col;
		}
	}

	if (custom)
		custom(*this, a, inst);
}

void DisassemblerBitcode::Context::Disassemble(StateDefault2* state, Function& func) {
	auto fattr = func.attrs[AttributeGroup::FunctionSlot];

	if (fattr)
		state->lines.emplace_back(string_builder() << "; Function Attrs: " << fattr, 0);

	string_builder	a;
	a << ifelse(func.prototype, "declare ", "define ") << func.type->subtype << " @" << escapeStringIfNeeded(func.name) << '(';
	for (size_t i = 0; i < func.type->members.size(); i++) {
		a << onlyif(i, ", ") << func.type->members[i];

		if (auto attr = func.attrs[i + 1])
			a <<  ' ' << attr;

		if (i < func.args.size() && func.args[i].name)
			a << " %" << escapeStringIfNeeded(func.args[i].name);
	}
	a << ')';

	if (fattr)
		a << " #" << get(attr_group_ids[fattr]);

	if (func.prototype) {
		state->lines.emplace_back(a, 0);
		return;
	}

	state->lines.emplace_back(a << " {", 0);

	auto cur_block = func.blocks.begin();

	// if the first block has a name, use it
	if (cur_block->name)
		state->lines.emplace_back(string_builder() << escapeStringIfNeeded(cur_block->name) << ':', 0);

	for (auto &inst : func.instructions) {
		string_builder	a;
		a << "  ";
		if (inst->name || ~inst->id)
			a << inst << " = ";

		if (inst->op == Operation::Switch) {
			a << "switch " << Arg(inst->args[0], true) << ", " << Arg(inst->args[1], true) << " [";
			state->lines.emplace_back(a, func.instructions.index_of(inst) + func.line_offset);

			for (auto *p = inst->args.begin() + 2; p != inst->args.end(); p += 2)
				state->lines.emplace_back(string_builder() << "    " << Arg(p[0], true) << ", " << Arg(p[1], true), 0);

			state->lines.emplace_back("  ]", 0);

		} else {
			if (!(flags & SKIP_DEBUG) || inst->op != Operation::Call || !inst->funcCall->name.begins("llvm.dbg.")) {
				Disassemble(a, *inst);
				//don't allow breakpoints on phis
				state->lines.emplace_back(a, inst->op == Operation::Phi ? 0 : func.instructions.index_of(inst) + func.line_offset);
			}
		}

		// if this is the last instruction don't print the next block's label
		if (inst == func.instructions.back())
			break;

		if (is_terminator(inst->op)) {
			state->lines.emplace_back("", 0);

			cur_block++;

			string_builder	a;
			if (cur_block->name)
				a << escapeStringIfNeeded(cur_block->name);
			else
				a << "label<" << cur_block->id << '>';

			a << ':' << spaceto(50) << "; preds = " << separated_list(cur_block->preds);
			state->lines.emplace_back(a, 0);
		}
	}
	state->lines.emplace_back("}", 0);
}

struct TypeOrderer {
	dynamic_array<const Type *> types;
	hash_set<const Type*>		visited_types	= {{nullptr}};
	hash_set<const Metadata*>	visited_meta	= {{nullptr}};

	void accumulate(const Type *t) {
		if (!t || visited_types.count(t))
			return;

		visited_types.insert(t);

		// LLVM doesn't do quite a depth-first search, so we replicate its search order to ensure types are printed in the same order
		dynamic_array<const Type*> workingSet;
		workingSet.push_back(t);
		do {
			const Type *cur = workingSet.pop_back_value();
			types.push_back(cur);

			for (auto &i : reversed(cur->members)) {
				if (!visited_types.check_insert(i))
					workingSet.push_back(i);
			}

			if (!visited_types.check_insert(cur->subtype))
				workingSet.push_back(cur->subtype);

		} while (!workingSet.empty());
	}

	void accumulate(const Metadata *m) {
		if (!visited_meta.check_insert(m)) {
			accumulate(m->type);
			accumulate(m->value.GetType(false));
			for (auto c : m->value.get<ValueArray>())
				accumulate(c.get<const Metadata*>().or_default());
		}
	}
};

template<typename T> auto maybe_separator(const T& t, char sep = ' ') {
	return [=](string_accum& a) {
		auto	curr = a.length();
		a << t;
		if (a.length() != curr)
			a << sep;
	};
}

string_accum& DumpGlobalVar(string_accum& a, const GlobalVar* g) {
	a << '@' << escapeStringIfNeeded(g->name) << " = ";
	if (g->linkage != Linkage::External || !g->value)
		a << g->linkage << ' ';
	a << maybe_separator(g->unnamed_addr, ' ');
	if (g->type->addrSpace != Type::AddrSpace::Default)
		a << "addrspace(" << g->type->addrSpace << ") ";
	a << ifelse(g->is_const, "constant ", "global ");

	if (g->value)
		a << g->value;
	else
		a << g->type->subtype;

	if (g->align)
		a << ", align " << g->align;
	if (g->section)
		a << ", section " << escapedString(g->section);

	return a;
}

void DisassemblerBitcode::Context::Disassemble(StateDefault2 *state) {
	state->lines.emplace_back(string_builder() << "target datalayout = \"" << mod.datalayout << '"', 0);
	state->lines.emplace_back(string_builder() << "target triple = \"" << mod.triple << '"', 0);
	state->lines.emplace_back("", 0);

	// attr_groups

	hash_set<AttributeSet>			attr_sets;
	dynamic_array<AttributeGroup*>	attr_groups;

	auto	collect_attr = [&](const AttributeSet& set) {
		if (set && !attr_sets.check_insert(set)) {
			for (auto &g : *set) {
				if (g && g->slot == AttributeGroup::FunctionSlot && !attr_group_ids[g].exists()) {
					attr_group_ids[g] = attr_groups.size();
					attr_groups.push_back(g);
				}
			}
		}
	};

	for (auto f : mod.functions) {
		collect_attr(f->attrs);
		for (auto inst : f->instructions)
			collect_attr(inst->paramAttrs);
	}


	//	types

	if (!(flags & SKIP_TYPES)) {
		TypeOrderer		typeOrderer;

		for (auto g : mod.globalvars) {
			typeOrderer.accumulate(g->type);
			if (g->value)
				typeOrderer.accumulate(g->value.GetType());
		}

		for (auto f : mod.functions) {
			typeOrderer.accumulate(f->type);
			for (auto inst : f->instructions) {
				typeOrderer.accumulate(inst->type);
				for (auto &a : inst->args)
					typeOrderer.accumulate(a.GetType(false));
				for (auto &m : inst->attachedMeta)
					typeOrderer.accumulate(m.metadata);
			}
		}

		for (auto &i : mod.named_meta)
			for (auto j : i)
				typeOrderer.accumulate(j);

		bool printed = false;
		for (const Type* type : typeOrderer.types) {
			if (type->type == Type::Struct && type->name) {
				state->lines.emplace_back(string_builder() << type << " = type {" << separated_list(type->members) << '}', 0);
				printed = true;
			}
		}
		if (printed)
			state->lines.emplace_back("", 0);
	}

	//	globalvars

	if (!(flags & SKIP_GLOBALS)) {
		for (auto g : mod.globalvars)
			state->lines.emplace_back(DumpGlobalVar(string_builder() << "", g), 0);
		if (mod.globalvars)
			state->lines.emplace_back("", 0);
	}

	// need to assign IDs before any functions get dibs

	if (!(flags & SKIP_META)) {
		for (auto &i : mod.named_meta) {
			for (auto j : i)
				AssignMetaID(j);
		}

		// assign ALL meta IDs

		for (auto f : mod.functions) {
			for (auto inst : f->instructions) {
				for (auto &a : inst->args) {
					if (a.is<const Metadata*>())
						AssignMetaID(a.get_known<const Metadata*>());
				}
				if (inst->debug_loc)
					AssignMetaID(inst->debug_loc);

				for (auto &i : inst->attachedMeta)
					AssignMetaID(i.metadata);
			}
		}
	}


	//	functions

	uint32	line_offset = 1;
	for (auto f : mod.functions) {
		f->line_offset = line_offset;
		Disassemble(state, *f);
		state->lines.emplace_back("", 0);
		line_offset += f->instructions.size32();
	}

	//	funcAttrGroups

	if (attr_groups) {
		for (auto &i : attr_groups)
			state->lines.emplace_back(string_builder("attributes #") << attr_groups.index_of(i) << " = { " << i << " }", 0);
		state->lines.emplace_back("", 0);
	}

	// named meta

	if (!(flags & SKIP_META)) {
		for (auto &i : mod.named_meta) {
			string_builder	a;
			a << '!' << i.name << " = !{";
			int	j = 0;
			for (auto c : i)
				a << onlyif(j++, ", ") << '!' << get(metadata_ids[c.m]);
			a << '}';
			state->lines.emplace_back(a, 0);
		}

		if (mod.named_meta)
			state->lines.emplace_back("", 0);
	}

	// numbered meta

	for (auto &i : numbered_meta) {
		string_builder	a;
		if (auto m = i.get<const Metadata*>().or_default()) {
			a << '!' << get(metadata_ids[m]) << " = " << onlyif(m->distinct, "distinct ");
			DumpMetaVal(a, m);
			//if (auto *d = m->value.get<const DebugInfo*>().or_default()) {
			//	if (auto d2 = d->as<DISubprogram>())
			//		unconst(d2)->setID(m->id);
			//}

		} else {
			auto d = i.get_known<const DebugInfo*>();
			a << '!' << get(metadata_ids[d]) << " = ";// << d;
			process<void>(d, DebugInfoPrinter(*this, a));
		}
		state->lines.emplace_back(a, 0);
	}

	state->lines.emplace_back("", 0);

	uint64	last_addr = 0;
	for (auto& i : state->lines) {
		if (!i.b) {
			i.b = last_addr;
		} else {
			ISO_ASSERT(i.b >= last_addr);
			last_addr = i.b;
		}
	}
}


DECLARE_PREFIXED_ENUMS(dwarf::VIRTUALITY);
MAKE_PREFIXED_ENUMS(dwarf::VIRTUALITY, DW_VIRTUALITY_,
	none,
	virtual,
	pure_virtual,
	);

DECLARE_PREFIXED_ENUMS(dwarf::LANG);
MAKE_PREFIXED_ENUMS(dwarf::LANG, DW_LANG_,
	?,				C89,			C,				Ada83,			C_plus_plus,	Cobol74,		Cobol85,		Fortran77,
	Fortran90,		Pascal83,		Modula2,		Java,			C99,			Ada95,			Fortran95,		PLI,
	ObjC,			ObjC_plus_plus,	UPC,			D,				Python,			OpenCL,			Go,				Modula3,
	Haskell,		C_plus_plus_03,	C_plus_plus_11,	OCaml,			Rust,			C11,			Swift,			Julia,
	Dylan,			C_plus_plus_14,	Fortran03,		Fortran08,		RenderScript,	BLISS,
	);

DECLARE_PREFIXED_ENUMS(dwarf::ATE);
MAKE_PREFIXED_ENUMS(dwarf::ATE, DW_ATE_,
	?,
	address,
	boolean,
	complex_float,
	float,
	signed,
	signed_char,
	unsigned,
	unsigned_char,
	imaginary_float,
	packed_decimal,
	numeric_string,
	edited,
	signed_fixed,
	unsigned_fixed,
	decimal_float,
	UTF,
	);

DECLARE_PREFIXED_VALUE_ENUMS(dwarf::OP);
MAKE_PREFIXED_VALUE_ENUMS2(dwarf::OP, DW_,
	OP_none,			OP_addr,				OP_deref,			OP_const1u,			OP_const1s,			OP_const2u,				OP_const2s,			OP_const4u,
	OP_const4s,			OP_const8u,				OP_const8s,			OP_constu,			OP_consts,			OP_dup,					OP_drop,			OP_over,
	OP_pick,			OP_swap,				OP_rot,				OP_xderef,			OP_abs,				OP_and,					OP_div,				OP_minus,
	OP_mod,				OP_mul,					OP_neg,				OP_not,				OP_or,				OP_plus,				OP_plus_uconst,		OP_shl,
	OP_shr,				OP_shra,				OP_xor,				OP_skip,			OP_bra,				OP_eq,					OP_ge,				OP_gt,
	OP_le,				OP_lt,					OP_ne,				OP_lit0,			OP_reg0,			OP_breg0,				OP_regx,			OP_fbreg,
	OP_bregx,			OP_piece,				OP_deref_size,		OP_xderef_size,		OP_nop,				OP_push_object_address,	OP_call2,			OP_call4,
	OP_call_ref,		OP_form_tls_address,	OP_call_frame_cfa,	OP_bit_piece,		OP_implicit_value,	OP_stack_value
);

DECLARE_PREFIXED_VALUE_ENUMS(dwarf::TAG);


START_PREFIXED_VALUE_ENUMS(dwarf::TAG, DW_)
VA_APPLYP(_MAKE_VALUE2, dwarf::TAG,
		TAG_array_type,			TAG_class_type,					TAG_entry_point,		TAG_enumeration_type,	TAG_formal_parameter,			TAG_imported_declaration,			TAG_label,						TAG_lexical_block,
		TAG_member,				TAG_pointer_type,				TAG_reference_type,		TAG_compile_unit,		TAG_string_type,				TAG_structure_type,					TAG_subroutine_type,			TAG_typedef,
		TAG_union_type,			TAG_unspecified_parameters,		TAG_variant,			TAG_common_block,		TAG_common_inclusion,			TAG_inheritance,					TAG_inlined_subroutine,			TAG_module,
		TAG_ptr_to_member_type,	TAG_set_type,					TAG_subrange_type,		TAG_with_stmt,			TAG_access_declaration,			TAG_base_type,						TAG_catch_block,				TAG_const_type,
		TAG_constant,			TAG_enumerator,	TAG_file_type,	TAG_friend,				TAG_namelist,			TAG_namelist_item,				TAG_packed_type,					TAG_subprogram,					TAG_template_type_parameter,
		TAG_template_value_parameter,	TAG_thrown_type,		TAG_try_block,			TAG_variant_part,		TAG_variable,					TAG_volatile_type,					TAG_dwarf_procedure,			TAG_restrict_type
	)
	VA_APPLYP(_MAKE_VALUE2, dwarf::TAG,
		TAG_interface_type,		TAG_namespace,					TAG_imported_module,	TAG_unspecified_type,	TAG_partial_unit,				TAG_imported_unit,					TAG_condition,					TAG_shared_type,
		TAG_type_unit,			TAG_rvalue_reference_type,		TAG_template_alias,		TAG_coarray_type,		TAG_generic_subrange,			TAG_dynamic_type,					TAG_auto_variable,				TAG_arg_variable,
		TAG_MIPS_loop,			TAG_format_label,				TAG_function_template,	TAG_class_template,		TAG_GNU_template_template_param,TAG_GNU_template_parameter_pack,	TAG_GNU_formal_parameter_pack,	TAG_APPLE_property
	)
0}};

DECLARE_PREFIXED_BIT_ENUMS(DebugInfo::Flags);
MAKE_PREFIXED_BIT_ENUMS2(DebugInfo::Flags, DIFlag,
	None, Private, Protected, Public, FwdDecl, AppleBlock, BlockByrefStruct, Virtual, Artificial, Explicit, Prototyped,
	ObjcClassComplete, ObjectPointer, Vector, StaticMember, LValueReference, RValueReference
);

