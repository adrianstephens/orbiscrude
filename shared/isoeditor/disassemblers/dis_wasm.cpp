#include "code/webassembly.h"
#include "extra/text_stream.h"
#include "extra/identifier.h"
#include "disassembler.h"

using namespace wabt;

template<> const char *field_names<wabt::ExternalKind>::s[]	= {"func", "table", "memory", "global", "except"};

template<> const char *field_names<wabt::Reloc::Type>::s[]	= {
	"R_WEBASSEMBLY_FUNCTION_INDEX_LEB",		"R_WEBASSEMBLY_TABLE_INDEX_SLEB",
	"R_WEBASSEMBLY_TABLE_INDEX_I32",		"R_WEBASSEMBLY_MEMORY_ADDR_LEB",
	"R_WEBASSEMBLY_MEMORY_ADDR_SLEB",		"R_WEBASSEMBLY_MEMORY_ADDR_I32",
	"R_WEBASSEMBLY_TYPE_INDEX_LEB",			"R_WEBASSEMBLY_GLOBAL_INDEX_LEB",
	"R_WEBASSEMBLY_FUNCTION_OFFSET_I32",	"R_WEBASSEMBLY_SECTION_OFFSET_I32",
};

template<> const char *field_names<wabt::Symbol::Type>::s[]	= {	"func", "global", "data", "section" };

template<> const char *field_names<wabt::BinarySection>::s[]	= {
	"Custom",	"Type",		"Import",	"Function",
	"Table",	"Memory",	"Global",	"Export",
	"Start",	"Elem",		"Code",		"Data",
};
template<> struct field_names<wabt::Type>		{ static field_value s[]; };
field_value field_names<wabt::Type>::s[]	= {
	{"i32",			(int)wabt::Type::I32		},
	{"i64",			(int)wabt::Type::I64		},
	{"f32",			(int)wabt::Type::F32		},
	{"f64",			(int)wabt::Type::F64		},
	{"uint128",		(int)wabt::Type::V128		},
	{"anyfunc",		(int)wabt::Type::Anyfunc	},
	{"func",		(int)wabt::Type::Func		},
	{"except_ref",	(int)wabt::Type::ExceptRef},
	{"void",		(int)wabt::Type::Void		},
	{"any",			(int)wabt::Type::Any		},
};

template<> const char *field_names<wabt::Expr::Type>::s[]	= {
	"AtomicLoad",
	"AtomicRmw",
	"AtomicRmwCmpxchg",
	"AtomicStore",
	"AtomicWait",
	"AtomicWake",
	"Binary",
	"Block",
	"Br",
	"BrIf",
	"BrTable",
	"Call",
	"CallIndirect",
	"Compare",
	"Const",
	"Convert",
	"CurrentMemory",
	"Drop",
	"GetGlobal",
	"GetLocal",
	"GrowMemory",
	"If",
	"IfExcept",
	"Load",
	"Loop",
	"Nop",
	"Rethrow",
	"Return",
	"Select",
	"SetGlobal",
	"SetLocal",
	"SimdLaneOp",
	"SimdShuffleOp",
	"Store",
	"TeeLocal",
	"Ternary",
	"Throw",
	"Try",
	"Unary",
	"Unreachable",
};

//-----------------------------------------------------------------------------
// writer
//-----------------------------------------------------------------------------

template<typename T> string_accum& WriteFloatHex(string_accum &out, float_components<T> bits) {
	if (bits.s)
		out << '-';

	if (bits.is_special()) {
		// Infinity or nan.
		if (bits.is_inf()) {
			out << "inf";
		} else {
			out << "nan";
			if (!bits.is_quiet_nan())
				out << ":0x" << hex(bits.m);
		}
	} else if (bits.is_zero()) {
		out << "0x0p+0";
	} else {
		out << "0x1";
		if (auto sig = bits.m) {
			out << '.';
			while (sig) {
				out << to_digit(sig >> (bits.M - 4), 'a');
				sig = (sig << 4) & bits64(bits.M);
			}
		}
		int		exp	= bits.get_dexp();
		out << 'p' << (exp < 0 ? '-' : '+') << abs(exp);
	}
	return out;
}

static const char_set s_is_char_escaped		= ~char_set::ascii + char_set::cntrl + '"' + '\\';
static const char_set s_valid_name_chars	= char_set::ascii - char_set(" (),;[]{}");


class WatWriter : public ExprVisitor::Delegate {
	enum NextChar {
		None,
		Space,
		Newline,
		ForceNewline,
	};

	struct ExprTree {
		const Expr* expr;
		dynamic_array<ExprTree> children;
		explicit ExprTree(const Expr* expr = nullptr) : expr(expr) {}
	};

	struct Label {
		string		name;
		LabelType	label_type;
		TypeVector	param_types;
		TypeVector	result_types;
		Label(LabelType label_type,const string& name,const TypeVector& param_types,const TypeVector& result_types) : name(name),label_type(label_type),param_types(param_types),result_types(result_types) {}
	};
	Disassembler::StateDefault2	*state;
	string_builder				acc;

	const Module*	module			= nullptr;
	const Func*		current_func	= nullptr;
	bool			result			= true;
	int				indent			= 0;
	bool			inline_import	= false;
	bool			inline_export	= false;
	bool			fold_exprs		= false;
	int							indent_size	= 2;
	NextChar					next_char	= None;
	dynamic_array<string>		index_to_name;
	dynamic_array<Label>		label_stack;
	dynamic_array<ExprTree>		expr_tree_stack;
	multimap<pair<ExternalKind, Index>, const Export*>	inline_export_map;
	dynamic_array<const Import*> inline_import_map[(int)ExternalKind::size];

	Index	func_index		= 0;
	Index	global_index	= 0;
	Index	table_index		= 0;
	Index	memory_index	= 0;
	Index	func_type_index	= 0;
	Index	except_index	= 0;

	template<typename T> void	Write(const T &t) {
		WriteNextChar();
		acc << t;
		next_char = Space;
	}
	void	Indent()		{ ++indent; }
	void	Dedent()		{ ISO_ASSERT(indent > 0); --indent; }

	void	WriteNextChar() {
		switch (next_char) {
			default:
				break;
			case Space:
				acc.putc(' ');
				break;
			case Newline:
			case ForceNewline:
				state->lines.emplace_back(acc.detach(), 0);
				acc << repeat(' ', indent * indent_size);
				break;
		}
		next_char = None;
	}
	void Writef(const char* format, ...) {
		va_list valist;
		va_start(valist, format);
		WriteNextChar();
		acc.vformat(format, valist);
		next_char = Space;
	}
	void WritePuts(const char* s, NextChar next_char) {
		WriteNextChar();
		acc << s;
		this->next_char = next_char;
	}
	void WritePutsSpace(const char* s) {
		WritePuts(s, Space);
	}
	void WritePutsNewline(const char* s) {
		WritePuts(s, Newline);
	}
	void WriteNewline(bool force) {
		if (next_char == ForceNewline)
			WriteNextChar();
		next_char = force ? ForceNewline : Newline;
	}
	void WriteOpen(const char* name, NextChar next_char) {
		WritePuts("(", None);
		WritePuts(name, next_char);
		Indent();
	}
	void WriteOpenNewline(const char* name) {
		WriteOpen(name, Newline);
	}
	void WriteOpenSpace(const char* name) {
		WriteOpen(name, Space);
	}
	void WriteClose(NextChar next_char) {
		if (this->next_char != ForceNewline)
			this->next_char = None;
		Dedent();
		WritePuts(")", next_char);
	}
	void WriteCloseNewline() {
		WriteClose(Newline);
	}
	void WriteCloseSpace() {
		WriteClose(Space);
	}
	void WriteString(const string& str, NextChar next_char) {
		WritePuts(str, next_char);
	}

	void	WriteName(count_string str, NextChar next_char);
	void	WriteNameOrIndex(count_string str, Index index, NextChar next_char);
	void	WriteQuotedData(const void* data, size_t length);
	void	WriteQuotedString(count_string str, NextChar next_char);
	void	WriteVar(const Var& var, NextChar next_char);
	void	WriteBrVar(const Var& var, NextChar next_char);
	void	WriteType(Type type, NextChar next_char);
	void	WriteTypes(const TypeVector& types, const char* name);
	void	WriteFuncSigSpace(const FuncSignature& func_sig);
	void	WriteBeginBlock(LabelType label_type,const Block& block,const char* text);
	void	WriteBeginIfExceptBlock(const IfExceptExpr* expr);
	void	WriteEndBlock();
	void	WriteConst(const Const& cnst);
	void	WriteExpr(const Expr* expr) {
		ExprVisitor(*this).VisitExpr(const_cast<Expr*>(expr));
	}
	template<typename T> void WriteLoadStoreExpr(const Expr* expr);
	void	WriteExprList(const ExprList& exprs) {
		ExprVisitor(*this).VisitExprList(const_cast<ExprList&>(exprs));
	}
	void	WriteInitExpr(const ExprList& expr);
	template<typename T> void WriteTypeBindings(const char* prefix,const Func& func,const T& types,const BindingHash& bindings);
	void	WriteBeginFunc(const Func& func);
	void	WriteFunc(const Func& func);
	void	WriteBeginGlobal(const Global& global);
	void	WriteGlobal(const Global& global);
	void	WriteBeginException(const Exception& except);
	void	WriteException(const Exception& except);
	void	WriteLimits(const Limits& limits);
	void	WriteTable(const Table& table);
	void	WriteElemSegment(const ElemSegment& segment);
	void	WriteMemory(const Memory& memory);
	void	WriteDataSegment(const DataSegment& segment);
	void	WriteImport(const Import& import);
	void	WriteExport(const Export& exp);
	void	WriteFuncType(const FuncType& func_type);
	void	WriteStartFunction(const Var& start);

	Index	GetLabelStackSize() { return label_stack.size32(); }
	Label*	GetLabel(const Var& var);
	Index	GetLabelArity(const Var& var);
	Index	GetFuncParamCount(const Var& var);
	Index	GetFuncResultCount(const Var& var);
	void	PushExpr(const Expr* expr, Index operand_count, Index result_count);
	void	FlushExprTree(const ExprTree& expr_tree);
	void	FlushExprTreeVector(const dynamic_array<ExprTree> &expr_trees);
	void	FlushExprTreeStack();
	void	WriteFoldedExpr(const Expr*);
	void	WriteFoldedExprList(const ExprList&);

	void	WriteInlineExports(ExternalKind kind, Index index);
	void	WriteInlineImport(ExternalKind kind, Index index);
public:
	WatWriter(Disassembler::StateDefault2 *state, bool fold_exprs = false) : state(state), fold_exprs(fold_exprs) {}
	bool WriteModule(const Module& module);

	// Implementation of ExprVisitor::DelegateNop
	bool OnBinaryExpr(BinaryExpr* expr) {
		WritePutsNewline(expr->opcode.GetName());
		return true;
	}
	bool BeginBlockExpr(BlockExpr* expr) {
		WriteBeginBlock(LabelType::Block, expr->block, GetName(Opcode::Block));
		return true;
	}
	bool EndBlockExpr(BlockExpr* expr) {
		WriteEndBlock();
		return true;
	}
	bool OnBrExpr(BrExpr* expr) {
		WritePutsSpace(GetName(Opcode::Br));
		WriteBrVar(expr->var, Newline);
		return true;
	}
	bool OnBrIfExpr(BrIfExpr* expr) {
		WritePutsSpace(GetName(Opcode::BrIf));
		WriteBrVar(expr->var, Newline);
		return true;
	}
	bool OnBrTableExpr(BrTableExpr* expr) {
		WritePutsSpace(GetName(Opcode::BrTable));
		for (const Var& var : expr->targets)
			WriteBrVar(var, Space);
		WriteBrVar(expr->default_target, Newline);
		return true;
	}
	bool OnCallExpr(CallExpr* expr) {
		WritePutsSpace(GetName(Opcode::Call));
		WriteVar(expr->var, Newline);
		return true;
	}
	bool OnCallIndirectExpr(
		CallIndirectExpr* expr) {
		WritePutsSpace(GetName(Opcode::CallIndirect));
		WriteOpenSpace("type");
		WriteVar(expr->decl.type_var, Space);
		WriteCloseNewline();
		return true;
	}
	bool OnCompareExpr(CompareExpr* expr) {
		WritePutsNewline(expr->opcode.GetName());
		return true;
	}
	bool OnConstExpr(ConstExpr* expr) {
		WriteConst(expr->cnst);
		return true;
	}
	bool OnConvertExpr(ConvertExpr* expr) {
		WritePutsNewline(expr->opcode.GetName());
		return true;
	}
	bool OnDropExpr(DropExpr* expr) {
		WritePutsNewline(GetName(Opcode::Drop));
		return true;
	}
	bool OnGetGlobalExpr(GetGlobalExpr* expr) {
		WritePutsSpace(GetName(Opcode::GetGlobal));
		WriteVar(expr->var, Newline);
		return true;
	}
	bool OnGetLocalExpr(GetLocalExpr* expr) {
		WritePutsSpace(GetName(Opcode::GetLocal));
		WriteVar(expr->var, Newline);
		return true;
	}
	bool BeginIfExpr(IfExpr* expr) {
		WriteBeginBlock(LabelType::If, expr->true_, GetName(Opcode::If));
		return true;
	}
	bool AfterIfTrueExpr(IfExpr* expr) {
		if (!expr->false_.empty()) {
			Dedent();
			WritePutsSpace(GetName(Opcode::Else));
			Indent();
			WriteNewline(true);
		}
		return true;
	}
	bool EndIfExpr(IfExpr* expr) {
		WriteEndBlock();
		return true;
	}
	bool BeginIfExceptExpr(IfExceptExpr* expr) {
		// Can't use WriteBeginBlock because if_except has an additional exception index argument
		WriteBeginIfExceptBlock(expr);
		return true;
	}
	bool AfterIfExceptTrueExpr(
		IfExceptExpr* expr) {
		if (!expr->false_.empty()) {
			Dedent();
			WritePutsSpace(GetName(Opcode::Else));
			Indent();
			WriteNewline(true);
		}
		return true;
	}
	bool EndIfExceptExpr(IfExceptExpr* expr) {
		WriteEndBlock();
		return true;
	}
	bool OnLoadExpr(LoadExpr* expr) {
		WriteLoadStoreExpr<LoadExpr>(expr);
		return true;
	}
	bool BeginLoopExpr(LoopExpr* expr) {
		WriteBeginBlock(LabelType::Loop, expr->block, GetName(Opcode::Loop));
		return true;
	}
	bool EndLoopExpr(LoopExpr* expr) {
		WriteEndBlock();
		return true;
	}
	bool OnMemoryGrowExpr(MemoryGrowExpr* expr) {
		WritePutsNewline(GetName(Opcode::MemoryGrow));
		return true;
	}
	bool OnMemorySizeExpr(MemorySizeExpr* expr) {
		WritePutsNewline(GetName(Opcode::MemorySize));
		return true;
	}
	bool OnNopExpr(NopExpr* expr) {
		WritePutsNewline(GetName(Opcode::Nop));
		return true;
	}
	bool OnReturnExpr(ReturnExpr* expr) {
		WritePutsNewline(GetName(Opcode::Return));
		return true;
	}
	bool OnSelectExpr(SelectExpr* expr) {
		WritePutsNewline(GetName(Opcode::Select));
		return true;
	}
	bool OnSetGlobalExpr(SetGlobalExpr* expr) {
		WritePutsSpace(GetName(Opcode::SetGlobal));
		WriteVar(expr->var, Newline);
		return true;
	}
	bool OnSetLocalExpr(SetLocalExpr* expr) {
		WritePutsSpace(GetName(Opcode::SetLocal));
		WriteVar(expr->var, Newline);
		return true;
	}
	bool OnStoreExpr(StoreExpr* expr) {
		WriteLoadStoreExpr<StoreExpr>(expr);
		return true;
	}
	bool OnTeeLocalExpr(TeeLocalExpr* expr) {
		WritePutsSpace(GetName(Opcode::TeeLocal));
		WriteVar(expr->var, Newline);
		return true;
	}
	bool OnUnaryExpr(UnaryExpr* expr) {
		WritePutsNewline(expr->opcode.GetName());
		return true;
	}
	bool OnUnreachableExpr(UnreachableExpr* expr) {
		WritePutsNewline(GetName(Opcode::Unreachable));
		return true;
	}
	bool BeginTryExpr(TryExpr* expr) {
		WriteBeginBlock(LabelType::Try, expr->block, GetName(Opcode::Try));
		return true;
	}
	bool OnCatchExpr(TryExpr* expr) {
		Dedent();
		WritePutsSpace(GetName(Opcode::Catch));
		Indent();
		label_stack.back().label_type = LabelType::Catch;
		WriteNewline(true);
		return true;
	}
	bool EndTryExpr(TryExpr* expr) {
		WriteEndBlock();
		return true;
	}
	bool OnThrowExpr(ThrowExpr* expr) {
		WritePutsSpace(GetName(Opcode::Throw));
		WriteVar(expr->var, Newline);
		return true;
	}
	bool OnRethrowExpr(RethrowExpr* expr) {
		WritePutsSpace(GetName(Opcode::Rethrow));
		return true;
	}
	bool OnAtomicWaitExpr(AtomicWaitExpr* expr) {
		WriteLoadStoreExpr<AtomicWaitExpr>(expr);
		return true;
	}
	bool OnAtomicWakeExpr(AtomicWakeExpr* expr) {
		WriteLoadStoreExpr<AtomicWakeExpr>(expr);
		return true;
	}
	bool OnAtomicLoadExpr(AtomicLoadExpr* expr) {
		WriteLoadStoreExpr<AtomicLoadExpr>(expr);
		return true;
	}
	bool OnAtomicStoreExpr(
		AtomicStoreExpr* expr) {
		WriteLoadStoreExpr<AtomicStoreExpr>(expr);
		return true;
	}
	bool OnAtomicRmwExpr(AtomicRmwExpr* expr) {
		WriteLoadStoreExpr<AtomicRmwExpr>(expr);
		return true;
	}
	bool OnAtomicRmwCmpxchgExpr(
		AtomicRmwCmpxchgExpr* expr) {
		WriteLoadStoreExpr<AtomicRmwCmpxchgExpr>(expr);
		return true;
	}
	bool OnTernaryExpr(TernaryExpr* expr) {
		WritePutsNewline(expr->opcode.GetName());
		return true;
	}
	bool OnSimdLaneOpExpr(SimdLaneOpExpr* expr) {
		WritePutsSpace(expr->opcode.GetName());
		Write(expr->val);
		WritePutsNewline("");
		return true;
	}
	bool OnSimdShuffleOpExpr(
		SimdShuffleOpExpr* expr) {
		WritePutsSpace(expr->opcode.GetName());
		const uint32	*p = (const uint32*)&expr->val;
		Writef(" 0x%08x 0x%08x 0x%08x 0x%08x", p[0], p[1], p[2], p[3]);
		WritePutsNewline("");
		return true;
	}
};

void WatWriter::WriteName(count_string str, NextChar next_char) {
	// Debug names must begin with a $ for for wast file to be valid
	ISO_ASSERT(!str.empty() && str.front() == '$');
	bool has_invalid_chars = str.find(~s_valid_name_chars);

	WriteNextChar();
	if (has_invalid_chars) {
		string valid_str;
		transform(str.begin(), str.end(), back_inserter(valid_str), [](uint8 c) { return s_valid_name_chars.test(c) ? c : '_'; });
		acc << valid_str;
	} else {
		acc << str;
	}

	this->next_char = next_char;
}

void WatWriter::WriteNameOrIndex(count_string str,Index index,NextChar next_char) {
	if (!str.empty())
		WriteName(str, next_char);
	else
		Writef("(;%u;)", index);
}

void WatWriter::WriteQuotedData(const void* data, size_t length) {
	const uint8* u8_data = static_cast<const uint8*>(data);
	WriteNextChar();
	acc.putc('\"');
	for (size_t i = 0; i < length; ++i) {
		uint8 c = u8_data[i];
		if (s_is_char_escaped.test(c)) {
			acc.putc('\\');
			acc.putc(to_digit(c >> 4, 'a'));
			acc.putc(to_digit(c & 0xf, 'a'));
		} else {
			acc.putc(c);
		}
	}
	acc.putc('\"');
	next_char = Space;
}

void WatWriter::WriteQuotedString(count_string str, NextChar next_char) {
	WriteQuotedData(str.begin(), str.length());
	this->next_char = next_char;
}

void WatWriter::WriteVar(const Var& var, NextChar next_char) {
	if (var.is_index()) {
		Writef("%u", var.index());
		this->next_char = next_char;
	} else {
		WriteName(count_string(var.name()), next_char);
	}
}

void WatWriter::WriteBrVar(const Var& var, NextChar next_char) {
	if (var.is_index()) {
		if (var.index() < GetLabelStackSize())
			Writef("%u (;@%u;)", var.index(),GetLabelStackSize() - var.index() - 1);
		else
			Writef("%u (; INVALID ;)", var.index());
		this->next_char = next_char;
	} else {
		WriteString(var.name(), next_char);
	}
}

void WatWriter::WriteType(Type type, NextChar next_char) {
	const char* type_name = get_field_name(type);
	ISO_ASSERT(type_name);
	WritePuts(type_name, next_char);
}

void WatWriter::WriteTypes(const TypeVector& types, const char* name) {
	if (types.size()) {
		if (name)
			WriteOpenSpace(name);
		for (Type type : types)
			WriteType(type, Space);
		if (name)
			WriteCloseSpace();
	}
}

void WatWriter::WriteFuncSigSpace(const FuncSignature& func_sig) {
	WriteTypes(func_sig.param_types, "param");
	WriteTypes(func_sig.result_types, "result");
}

void WatWriter::WriteBeginBlock(LabelType label_type,const Block& block,const char* text) {
	WritePutsSpace(text);
	bool has_label = !block.label.empty();
	if (has_label)
		WriteString(block.label, Space);

	WriteTypes(block.decl.sig.param_types, "param");
	WriteTypes(block.decl.sig.result_types, "result");
	if (!has_label)
		Writef(" ;; label = @%u", GetLabelStackSize());

	WriteNewline(true);
	label_stack.emplace_back(label_type, block.label, block.decl.sig.param_types,block.decl.sig.result_types);
	Indent();
}

void WatWriter::WriteBeginIfExceptBlock(const IfExceptExpr* expr) {
	const Block& block = expr->true_;
	WritePutsSpace(GetName(Opcode::IfExcept));
	bool has_label = !block.label.empty();
	if (has_label)
		WriteString(block.label, Space);

	WriteTypes(block.decl.sig.param_types, "param");
	WriteTypes(block.decl.sig.result_types, "result");
	WriteVar(expr->except_var, Space);
	if (!has_label)
		Writef(" ;; label = @%u", GetLabelStackSize());

	WriteNewline(true);
	label_stack.emplace_back(LabelType::IfExcept, block.label,block.decl.sig.param_types,block.decl.sig.result_types);
	Indent();
}

void WatWriter::WriteEndBlock() {
	Dedent();
	label_stack.pop_back();
	WritePutsNewline(GetName(Opcode::End));
}

void WatWriter::WriteConst(const Const& cnst) {
	switch (cnst.type) {
		case Type::I32:
			WritePutsSpace(GetName(Opcode::I32Const));
			Writef("%d", static_cast<int32>(cnst.u32));
			WriteNewline(false);
			break;

		case Type::I64:
			WritePutsSpace(GetName(Opcode::I64Const));
			Writef("%lld", static_cast<int64>(cnst.u64));
			WriteNewline(false);
			break;

		case Type::F32: {
			WritePutsSpace(GetName(Opcode::F32Const));
			buffer_accum<128>	buffer;
			WriteFloatHex(buffer, iorf(cnst.f32_bits));
			WritePutsSpace(buffer.term());
			float f32;
			memcpy(&f32, &cnst.f32_bits, sizeof(f32));
			Writef("(;=%g;)", f32);
			WriteNewline(false);
			break;
		}
		case Type::F64: {
			WritePutsSpace(GetName(Opcode::F64Const));
			buffer_accum<128>	buffer;
			WriteFloatHex(buffer, iord(cnst.f64_bits));
			WritePutsSpace(buffer.term());
			double f64;
			memcpy(&f64, &cnst.f64_bits, sizeof(f64));
			Writef("(;=%g;)", f64);
			WriteNewline(false);
			break;
		}
		case Type::V128: {
			WritePutsSpace(GetName(Opcode::V128Const));
			const uint32	*p = (const uint32*)&cnst.v128_bits;
			Writef("i32 0x%08x 0x%08x 0x%08x 0x%08x", p[0], p[1], p[2], p[3]);
			WriteNewline(false);
			break;
		}
		default:
			ISO_ASSERT(0);
			break;
	}
}

template<typename T> void WatWriter::WriteLoadStoreExpr(const Expr* expr) {
	auto typed_expr = cast<T>(expr);
	WritePutsSpace(typed_expr->opcode.GetName());
	if (typed_expr->offset)
		Writef("offset=%u", typed_expr->offset);
	if (!typed_expr->opcode.IsNaturallyAligned(typed_expr->align))
		Writef("align=%u", typed_expr->align);
	WriteNewline(false);
}

WatWriter::Label* WatWriter::GetLabel(const Var& var) {
	if (var.is_name()) {
		for (Index i = GetLabelStackSize(); i > 0; --i) {
			Label* label = &label_stack[i - 1];
			if (label->name == var.name())
				return label;
		}
	} else if (var.index() < GetLabelStackSize()) {
		return &label_stack[GetLabelStackSize() - var.index() - 1];
	}
	return nullptr;
}

Index WatWriter::GetLabelArity(const Var& var) {
	Label* label = GetLabel(var);
	return	!label ? 0
		:	label->label_type == LabelType::Loop ? label->param_types.size32()
		:	label->result_types.size32();
}

Index WatWriter::GetFuncParamCount(const Var& var) {
	const Func* func = module->GetFunc(var);
	return func ? func->GetNumParams() : 0;
}

Index WatWriter::GetFuncResultCount(const Var& var) {
	const Func* func = module->GetFunc(var);
	return func ? func->GetNumResults() : 0;
}

void WatWriter::WriteFoldedExpr(const Expr* expr) {
	switch (expr->type) {
		case Expr::Type::AtomicRmw:
		case Expr::Type::AtomicWake:
		case Expr::Type::Binary:
		case Expr::Type::Compare:
			PushExpr(expr, 2, 1);
			break;

		case Expr::Type::AtomicStore:
		case Expr::Type::Store:
			PushExpr(expr, 2, 0);
			break;

		case Expr::Type::Block:
			PushExpr(expr, 0, cast<BlockExpr>(expr)->block.decl.sig.GetNumResults());
			break;

		case Expr::Type::Br:
			PushExpr(expr, GetLabelArity(cast<BrExpr>(expr)->var), 1);
			break;

		case Expr::Type::BrIf: {
			Index arity = GetLabelArity(cast<BrIfExpr>(expr)->var);
			PushExpr(expr, arity + 1, arity);
			break;
		}
		case Expr::Type::BrTable:
			PushExpr(expr, GetLabelArity(cast<BrTableExpr>(expr)->default_target) + 1,1);
			break;

		case Expr::Type::Call: {
			const Var& var = cast<CallExpr>(expr)->var;
			PushExpr(expr, GetFuncParamCount(var), GetFuncResultCount(var));
			break;
		}
		case Expr::Type::CallIndirect: {
			const auto* ci_expr = cast<CallIndirectExpr>(expr);
			PushExpr(expr, ci_expr->decl.GetNumParams() + 1,ci_expr->decl.GetNumResults());
			break;
		}
		case Expr::Type::Const:
		case Expr::Type::MemorySize:
		case Expr::Type::GetGlobal:
		case Expr::Type::GetLocal:
		case Expr::Type::Unreachable:
			PushExpr(expr, 0, 1);
			break;

		case Expr::Type::AtomicLoad:
		case Expr::Type::Convert:
		case Expr::Type::MemoryGrow:
		case Expr::Type::Load:
		case Expr::Type::TeeLocal:
		case Expr::Type::Unary:
			PushExpr(expr, 1, 1);
			break;

		case Expr::Type::Drop:
		case Expr::Type::SetGlobal:
		case Expr::Type::SetLocal:
			PushExpr(expr, 1, 0);
			break;

		case Expr::Type::If:
			PushExpr(expr, 1, cast<IfExpr>(expr)->true_.decl.sig.GetNumResults());
			break;

		case Expr::Type::IfExcept:
			PushExpr(expr, 1,cast<IfExceptExpr>(expr)->true_.decl.sig.GetNumResults());
			break;

		case Expr::Type::Loop:
			PushExpr(expr, 0, cast<LoopExpr>(expr)->block.decl.sig.GetNumResults());
			break;

		case Expr::Type::Nop:
			PushExpr(expr, 0, 0);
			break;

		case Expr::Type::Return:
			PushExpr(expr, current_func->decl.sig.result_types.size32(), 1);
			break;

		case Expr::Type::Rethrow:
			PushExpr(expr, 0, 0);
			break;

		case Expr::Type::AtomicRmwCmpxchg:
		case Expr::Type::AtomicWait:
		case Expr::Type::Select:
			PushExpr(expr, 3, 1);
			break;

		case Expr::Type::Throw: {
			auto throw_ = cast<ThrowExpr>(expr);
			Index operand_count = 0;
			if (Exception* except = module->GetExcept(throw_->var))
				operand_count = except->sig.size32();
			PushExpr(expr, operand_count, 0);
			break;
		}

		case Expr::Type::Try:
			PushExpr(expr, 0, cast<TryExpr>(expr)->block.decl.sig.GetNumResults());
			break;

		case Expr::Type::Ternary:
			PushExpr(expr, 3, 1);
			break;

		case Expr::Type::SimdLaneOp:
			switch (cast<SimdLaneOpExpr>(expr)->opcode) {
				case Opcode::I8X16ExtractLaneS:
				case Opcode::I8X16ExtractLaneU:
				case Opcode::I16X8ExtractLaneS:
				case Opcode::I16X8ExtractLaneU:
				case Opcode::I32X4ExtractLane:
				case Opcode::I64X2ExtractLane:
				case Opcode::F32X4ExtractLane:
				case Opcode::F64X2ExtractLane:
					PushExpr(expr, 1, 1);
					break;

				case Opcode::I8X16ReplaceLane:
				case Opcode::I16X8ReplaceLane:
				case Opcode::I32X4ReplaceLane:
				case Opcode::I64X2ReplaceLane:
				case Opcode::F32X4ReplaceLane:
				case Opcode::F64X2ReplaceLane:
					PushExpr(expr, 2, 1);
					break;

				default:
					fprintf(stderr, "Invalid Opcode for expr type: %s\n",get_field_name(expr->type));
					ISO_ASSERT(0);
			}
			break;

		case Expr::Type::SimdShuffleOp:
			PushExpr(expr, 2, 1);
			break;

		default:
			fprintf(stderr, "bad expr type: %s\n", get_field_name(expr->type));
			ISO_ASSERT(0);
			break;
	}
}

void WatWriter::WriteFoldedExprList(const ExprList& exprs) {
	for (const Expr& expr : exprs)
		WriteFoldedExpr(&expr);
}

void WatWriter::PushExpr(const Expr* expr,Index operand_count,Index result_count) {
	if (operand_count <= expr_tree_stack.size()) {
		auto last_operand = expr_tree_stack.end();
		auto first_operand = last_operand - operand_count;
		ExprTree tree(expr);
		iso::move_n(first_operand, back_inserter(tree.children), operand_count);
		expr_tree_stack.erase(first_operand, last_operand);
		expr_tree_stack.emplace_back(tree);
		if (result_count == 0)
			FlushExprTreeStack();
	} else {
		expr_tree_stack.emplace_back(expr);
		FlushExprTreeStack();
	}
}

void WatWriter::FlushExprTree(const ExprTree& expr_tree) {
	switch (expr_tree.expr->type) {
		case Expr::Type::Block:
			WritePuts("(", None);
			WriteBeginBlock(LabelType::Block, cast<BlockExpr>(expr_tree.expr)->block,GetName(Opcode::Block));
			WriteFoldedExprList(cast<BlockExpr>(expr_tree.expr)->block.exprs);
			FlushExprTreeStack();
			WriteCloseNewline();
			break;

		case Expr::Type::Loop:
			WritePuts("(", None);
			WriteBeginBlock(LabelType::Loop, cast<LoopExpr>(expr_tree.expr)->block,GetName(Opcode::Loop));
			WriteFoldedExprList(cast<LoopExpr>(expr_tree.expr)->block.exprs);
			FlushExprTreeStack();
			WriteCloseNewline();
			break;

		case Expr::Type::If: {
			auto if_expr = cast<IfExpr>(expr_tree.expr);
			WritePuts("(", None);
			WriteBeginBlock(LabelType::If, if_expr->true_,GetName(Opcode::If));
			FlushExprTreeVector(expr_tree.children);
			WriteOpenNewline("then");
			WriteFoldedExprList(if_expr->true_.exprs);
			FlushExprTreeStack();
			WriteCloseNewline();
			if (!if_expr->false_.empty()) {
				WriteOpenNewline("else");
				WriteFoldedExprList(if_expr->false_);
				FlushExprTreeStack();
				WriteCloseNewline();
			}
			WriteCloseNewline();
			break;
		}
		case Expr::Type::IfExcept: {
			auto if_except_expr = cast<IfExceptExpr>(expr_tree.expr);
			WritePuts("(", None);
			WriteBeginIfExceptBlock(if_except_expr);
			FlushExprTreeVector(expr_tree.children);
			WriteOpenNewline("then");
			WriteFoldedExprList(if_except_expr->true_.exprs);
			FlushExprTreeStack();
			WriteCloseNewline();
			if (!if_except_expr->false_.empty()) {
				WriteOpenNewline("else");
				WriteFoldedExprList(if_except_expr->false_);
				FlushExprTreeStack();
				WriteCloseNewline();
			}
			WriteCloseNewline();
			break;
		}
		case Expr::Type::Try: {
			auto try_expr = cast<TryExpr>(expr_tree.expr);
			WritePuts("(", None);
			WriteBeginBlock(LabelType::Try, try_expr->block,GetName(Opcode::Try));
			FlushExprTreeVector(expr_tree.children);
			WriteFoldedExprList(try_expr->block.exprs);
			FlushExprTreeStack();
			WriteOpenNewline("catch");
			WriteFoldedExprList(try_expr->catch_);
			FlushExprTreeStack();
			WriteCloseNewline();
			WriteCloseNewline();
			break;
		}
		default: {
			WritePuts("(", None);
			WriteExpr(expr_tree.expr);
			Indent();
			FlushExprTreeVector(expr_tree.children);
			WriteCloseNewline();
			break;
		}
	}
}

void WatWriter::FlushExprTreeVector(const dynamic_array<ExprTree> &expr_trees) {
	for (auto expr_tree : expr_trees)
		FlushExprTree(expr_tree);
}

void WatWriter::FlushExprTreeStack() {
	dynamic_array<ExprTree> stack_copy;
	swap(stack_copy, expr_tree_stack);
	FlushExprTreeVector(stack_copy);
}

void WatWriter::WriteInitExpr(const ExprList& expr) {
	if (!expr.empty()) {
		WritePuts("(", None);
		WriteExprList(expr);
		/* clear the next char, so we don't write a newline after the expr */
		next_char = None;
		WritePuts(")", Space);
	}
}

template<typename T> void WatWriter::WriteTypeBindings(const char* prefix,const Func& func,const T& types,const BindingHash& bindings) {
	index_to_name = bindings.MakeReverseMapping(types.size());

	// named params/locals must be specified by themselves, but nameless params/locals can be compressed, e.g.:
	//	*   (param $foo i32)
	//	*   (param i32 i64 f32)
	bool is_open = false;
	size_t index = 0;
	for (Type type : types) {
		if (!is_open) {
			WriteOpenSpace(prefix);
			is_open = true;
		}

		const string& name = index_to_name[index];
		if (!name.empty())
			WriteString(name, Space);
		WriteType(type, Space);
		if (!name.empty()) {
			WriteCloseSpace();
			is_open = false;
		}
		++index;
	}
	if (is_open)
		WriteCloseSpace();
}

void WatWriter::WriteBeginFunc(const Func& func) {
	WriteOpenSpace("func");
	WriteNameOrIndex(count_string(func.name), func_index, Space);
	WriteInlineExports(ExternalKind::Func, func_index);
	WriteInlineImport(ExternalKind::Func, func_index);
	if (func.decl.has_func_type) {
		WriteOpenSpace("type");
		WriteVar(func.decl.type_var, None);
		WriteCloseSpace();
	}

	if (module->IsImport(ExternalKind::Func, Var(func_index))) {
		// Imported functions can be written a few ways:
		//   1. (import "module" "field" (func (type 0)))
		//   2. (import "module" "field" (func (param i32) (result i32)))
		//   3. (func (import "module" "field") (type 0))
		//   4. (func (import "module" "field") (param i32) (result i32))
		//   5. (func (import "module" "field") (type 0) (param i32) (result i32))
		//
		// Note that the text format does not allow including the param/result explicitly when using the "(import..." syntax (#1 and #2).
		if (inline_import || !func.decl.has_func_type)
			WriteFuncSigSpace(func.decl.sig);
	}
	func_index++;
}

void WatWriter::WriteFunc(const Func& func) {
	WriteBeginFunc(func);
	WriteTypeBindings("param", func, func.decl.sig.param_types,func.param_bindings);
	WriteTypes(func.decl.sig.result_types, "result");
	WriteNewline(false);
	if (func.local_types.size())
		WriteTypeBindings("local", func, func.local_types, func.local_bindings);
	WriteNewline(false);
	label_stack.clear();
	label_stack.emplace_back(LabelType::Func, string(), TypeVector(),func.decl.sig.result_types);
	current_func = &func;
	if (fold_exprs) {
		WriteFoldedExprList(func.exprs);
		FlushExprTreeStack();
	} else {
		WriteExprList(func.exprs);
	}
	current_func = nullptr;
	WriteCloseNewline();
}

void WatWriter::WriteBeginGlobal(const Global& global) {
	WriteOpenSpace("global");
	WriteNameOrIndex(count_string(global.name), global_index, Space);
	WriteInlineExports(ExternalKind::Global, global_index);
	WriteInlineImport(ExternalKind::Global, global_index);
	if (global.mut) {
		WriteOpenSpace("mut");
		WriteType(global.type, Space);
		WriteCloseSpace();
	} else {
		WriteType(global.type, Space);
	}
	global_index++;
}

void WatWriter::WriteGlobal(const Global& global) {
	WriteBeginGlobal(global);
	WriteInitExpr(global.init_expr);
	WriteCloseNewline();
}

void WatWriter::WriteBeginException(const Exception& except) {
	WriteOpenSpace("except");
	WriteNameOrIndex(count_string(except.name), except_index, Space);
	WriteInlineExports(ExternalKind::Except, except_index);
	WriteInlineImport(ExternalKind::Except, except_index);
	WriteTypes(except.sig, nullptr);
	++except_index;
}

void WatWriter::WriteException(const Exception& except) {
	WriteBeginException(except);
	WriteCloseNewline();
}

void WatWriter::WriteLimits(const Limits& limits) {
	Write(limits.initial);
	if (limits.has_max)
		Write(limits.max);
	if (limits.is_shared)
		Writef("shared");
}

void WatWriter::WriteTable(const Table& table) {
	WriteOpenSpace("table");
	WriteNameOrIndex(count_string(table.name), table_index, Space);
	WriteInlineExports(ExternalKind::Table, table_index);
	WriteInlineImport(ExternalKind::Table, table_index);
	WriteLimits(table.elem_limits);
	WritePutsSpace("anyfunc");
	WriteCloseNewline();
	table_index++;
}

void WatWriter::WriteElemSegment(const ElemSegment& segment) {
	WriteOpenSpace("elem");
	WriteInitExpr(segment.offset);
	for (const Var& var : segment.vars)
		WriteVar(var, Space);
	WriteCloseNewline();
}

void WatWriter::WriteMemory(const Memory& memory) {
	WriteOpenSpace("memory");
	WriteNameOrIndex(count_string(memory.name), memory_index, Space);
	WriteInlineExports(ExternalKind::Memory, memory_index);
	WriteInlineImport(ExternalKind::Memory, memory_index);
	WriteLimits(memory.page_limits);
	WriteCloseNewline();
	memory_index++;
}

void WatWriter::WriteDataSegment(const DataSegment& segment) {
	WriteOpenSpace("data");
	WriteInitExpr(segment.offset);
	WriteQuotedData(segment.data.begin(), segment.data.size());
	WriteCloseNewline();
}

void WatWriter::WriteImport(const Import& import) {
	if (!inline_import) {
		WriteOpenSpace("import");
		WriteQuotedString(count_string(import.module_name), Space);
		WriteQuotedString(count_string(import.field_name), Space);
	}

	switch (import.kind) {
		case ExternalKind::Func:	WriteBeginFunc(cast<FuncImport>(&import)->func); WriteCloseSpace(); break;
		case ExternalKind::Table:	WriteTable(cast<TableImport>(&import)->table); break;
		case ExternalKind::Memory:	WriteMemory(cast<MemoryImport>(&import)->memory); break;
		case ExternalKind::Global:	WriteBeginGlobal(cast<GlobalImport>(&import)->global); WriteCloseSpace(); break;
		case ExternalKind::Except:	WriteBeginException(cast<ExceptionImport>(&import)->except); WriteCloseSpace(); break;
	}

	if (inline_import)
		WriteNewline(false);
	else
		WriteCloseNewline();
}

void WatWriter::WriteExport(const Export& exp) {
	if (inline_export) {
		Index index;
		switch (exp.kind) {
			case ExternalKind::Func:	index = module->GetFuncIndex(exp.var);		break;
			case ExternalKind::Table:	index = module->GetTableIndex(exp.var);		break;
			case ExternalKind::Memory:	index = module->GetMemoryIndex(exp.var);	break;
			case ExternalKind::Global:	index = module->GetGlobalIndex(exp.var);	break;
			case ExternalKind::Except:	index = module->GetExceptIndex(exp.var);	break;
		}
		if (inline_export_map.find(make_pair(exp.kind, index)) != inline_export_map.end())
			return;
	}

	WriteOpenSpace("export");
	WriteQuotedString(count_string(exp.name), Space);
	WriteOpenSpace(get_field_name(exp.kind));
	WriteVar(exp.var, Space);
	WriteCloseSpace();
	WriteCloseNewline();
}

void WatWriter::WriteFuncType(const FuncType& func_type) {
	WriteOpenSpace("type");
	WriteNameOrIndex(count_string(func_type.name), func_type_index++, Space);
	WriteOpenSpace("func");
	WriteFuncSigSpace(func_type.sig);
	WriteCloseSpace();
	WriteCloseNewline();
}

void WatWriter::WriteStartFunction(const Var& start) {
	WriteOpenSpace("start");
	WriteVar(start, None);
	WriteCloseNewline();
}

bool WatWriter::WriteModule(const Module& mod) {
	module = &mod;
	if (inline_export) {
		//BuildInlineExportMap()
		for (Export* exp : module->exports) {
			Index index = kInvalidIndex;

			// Exported imports can't be written with inline exports, unless the imports are also inline; e.g. the following is invalid:
			//   (import "module" "field" (func (export "e")))
			// But this is valid:
			//   (func (export "e") (import "module" "field"))
			if (!inline_import && module->IsImport(*exp))
				continue;

			switch (exp->kind) {
				case ExternalKind::Func:	index = module->GetFuncIndex(exp->var);		break;
				case ExternalKind::Table:	index = module->GetTableIndex(exp->var);	break;
				case ExternalKind::Memory:	index = module->GetMemoryIndex(exp->var);	break;
				case ExternalKind::Global:	index = module->GetGlobalIndex(exp->var);	break;
				case ExternalKind::Except:	index = module->GetExceptIndex(exp->var);	break;
			}

			if (index != kInvalidIndex)
				inline_export_map.put(make_pair(exp->kind, index), exp);
		}
	}

	if (inline_import) {
		for (const Import* import : module->imports)
			inline_import_map[static_cast<size_t>(import->kind)].push_back(import);
	}

	WriteOpenSpace("module");
	if (mod.name.empty())
		WriteNewline(false);
	else
		WriteName(count_string(mod.name), Newline);

	for (const ModuleField& field : mod.fields) {
		switch (field.type) {
			case ModuleField::Type::Func:			WriteFunc(cast<FuncModuleField>(&field)->func);							break;
			case ModuleField::Type::Global:			WriteGlobal(cast<GlobalModuleField>(&field)->global);					break;
			case ModuleField::Type::Import:			WriteImport(*cast<ImportModuleField>(&field)->import);					break;
			case ModuleField::Type::Except:			WriteException(cast<ExceptionModuleField>(&field)->except);				break;
			case ModuleField::Type::Export:			WriteExport(cast<ExportModuleField>(&field)->exp);						break;
			case ModuleField::Type::Table:			WriteTable(cast<TableModuleField>(&field)->table);						break;
			case ModuleField::Type::ElemSegment:	WriteElemSegment(cast<ElemSegmentModuleField>(&field)->elem_segment);	break;
			case ModuleField::Type::Memory:			WriteMemory(cast<MemoryModuleField>(&field)->memory);					break;
			case ModuleField::Type::DataSegment:	WriteDataSegment(cast<DataSegmentModuleField>(&field)->data_segment);	break;
			case ModuleField::Type::FuncType:		WriteFuncType(cast<FuncTypeModuleField>(&field)->func_type);			break;
			case ModuleField::Type::Start:			WriteStartFunction(cast<StartModuleField>(&field)->start);				break;
		}
	}
	WriteCloseNewline();
	/* force the newline to be written */
	WriteNextChar();
	return result;
}

void WatWriter::WriteInlineExports(ExternalKind kind, Index index) {
	if (inline_export) {
		for (auto exp : inline_export_map.bounds(make_pair(kind, index))) {
			WriteOpenSpace("export");
			WriteQuotedString(count_string(exp->name), None);
			WriteCloseSpace();
		}
	}
}

void WatWriter::WriteInlineImport(ExternalKind kind, Index index) {
	if (inline_import && index < inline_import_map[(int)kind].size()) {
		const Import* import = inline_import_map[(int)kind][index];
		WriteOpenSpace("import");
		WriteQuotedString(count_string(import->module_name), Space);
		WriteQuotedString(count_string(import->field_name), Space);
		WriteCloseSpace();
	}
}

class DisassemblerWasm : public Disassembler {
public:
	const char*	GetDescription() override { return "WebAssembly"; }
	State*		Disassemble(const_memory_block block, uint64 addr, SymbolFinder sym_finder) override;
} dis_wasm;

Disassembler::State* DisassemblerWasm::Disassemble(const_memory_block block, uint64 addr, SymbolFinder sym_finder) {
	StateDefault2	*state	= new StateDefault2;

	if (!~addr) {
		auto&			mod = *unconst((const Module*)block);
		WatWriter(state, false).WriteModule(mod);
	} else {
		Module			mod;
		if (mod.read(memory_reader(block)))
			WatWriter(state, false).WriteModule(mod);
	}

	return state;
}

