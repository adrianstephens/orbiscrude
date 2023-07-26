#include "webassembly.h"
#include "extra/text_stream.h"
#include "extra/identifier.h"

using namespace iso;
using namespace wabt;

#define CHECK_RESULT(expr)	do { if (!expr) return false; } while (0)

//-----------------------------------------------------------------------------
// NameGenerator
//-----------------------------------------------------------------------------

class NameGenerator : public ExprVisitor::Delegate {
	Module*		module_ = nullptr;
	ExprVisitor	visitor_;
	Index		label_count_			= 0;
	Index		num_func_imports_		= 0;
	Index		num_table_imports_		= 0;
	Index		num_memory_imports_		= 0;
	Index		num_global_imports_		= 0;
	Index		num_exception_imports_	= 0;

	// Generate a name with the given prefix, followed by the index and optionally a disambiguating number. If index == kInvalidIndex, the index is not appended
	static string GenerateName(const char* prefix, Index index, unsigned disambiguator);

	// Like GenerateName, but only generates a name if |out_str| is empty
	static void MaybeGenerateName(const char* prefix, Index index, string &str);

	// Generate a name via GenerateName and bind it to the given binding hash. If the name already exists, the name will be disambiguated until it can be added
	static string GenerateAndBindName(BindingHash* bindings, const char* prefix, Index index);

	// Like GenerateAndBindName, but only generates a name if |out_str| is empty
	static void MaybeGenerateAndBindName(BindingHash* bindings, const char* prefix, Index index, string &str);

	// Like MaybeGenerateAndBindName but uses the name directly, without appending the index. If the name already exists, a disambiguating suffix is added
	static void MaybeUseAndBindName(BindingHash* bindings, const char* name, Index index, string &str);

	void GenerateAndBindLocalNames(dynamic_array<string> &&index_to_name, BindingHash* bindings, const char* prefix);

public:
	NameGenerator() : visitor_(*this) {}

	bool VisitModule(Module* module);

	// Implementation of ExprVisitor::DelegateNop
	bool BeginBlockExpr(BlockExpr* expr) override {
		MaybeGenerateName("$B", label_count_++, expr->block.label);
		return true;
	}
	bool BeginLoopExpr(LoopExpr* expr) override {
		MaybeGenerateName("$L", label_count_++, expr->block.label);
		return true;
	}
	bool BeginIfExpr(IfExpr* expr) override {
		MaybeGenerateName("$I", label_count_++, expr->true_.label);
		return true;
	}
	bool BeginIfExceptExpr(IfExceptExpr* expr) override {
		MaybeGenerateName("$E", label_count_++, expr->true_.label);
		return true;
	}
};


// static
string NameGenerator::GenerateName(const char* prefix, Index index, unsigned disambiguator) {
	string str = prefix;
	if (index != kInvalidIndex)
		str += to_string(index);
	if (disambiguator != 0)
		str << '_' << to_string(disambiguator);
	return str;
}

// static
void NameGenerator::MaybeGenerateName(const char* prefix, Index index, string &str) {
	if (str.empty())
		// There's no bindings hash, so the name can't be a duplicate, and so doesn't need a disambiguating number
		str = GenerateName(prefix, index, 0);
}

// static
string NameGenerator::GenerateAndBindName(BindingHash* bindings, const char* prefix, Index index) {
	for (unsigned disambiguator = 0;;disambiguator++) {
		auto s = GenerateName(prefix, index, disambiguator);
		if (bindings->find(s) == bindings->end()) {
			bindings->put(s, Binding(index));
			return s;
		}
	}
}

// static
void NameGenerator::MaybeGenerateAndBindName(BindingHash* bindings, const char* prefix, Index index, string &str) {
	if (str.empty())
		str = GenerateAndBindName(bindings, prefix, index);
}

// static
void NameGenerator::MaybeUseAndBindName(BindingHash* bindings, const char* name, Index index, string &str) {
	if (str.empty()) {
		for (unsigned disambiguator = 0;;disambiguator++) {
			str = GenerateName(name, kInvalidIndex, disambiguator);
			if (bindings->find(str) == bindings->end()) {
				bindings->put(str, Binding(index));
				break;
			}
		}
	}
}

void NameGenerator::GenerateAndBindLocalNames(dynamic_array<string> &&index_to_name, BindingHash* bindings, const char* prefix) {
	for (auto &i : index_to_name)
		MaybeGenerateAndBindName(bindings, prefix, index_to_name.index_of(i), i);
}



bool NameGenerator::VisitModule(Module* module) {
	module_ = module;
	// Visit imports and exports first to give better names, derived from the
	// import/export name.
	for (auto import : module->imports) {
		BindingHash* bindings	= nullptr;
		string*		 name		= nullptr;
		Index		 index		= kInvalidIndex;

		switch (import->kind) {
			case ExternalKind::Func:
				if (auto* func_import = cast<FuncImport>(import)) {
					bindings = &module_->func_bindings;
					name	 = &func_import->func.name;
					index	 = num_func_imports_++;
				}
				break;

			case ExternalKind::Table:
				if (auto* table_import = cast<TableImport>(import)) {
					bindings = &module_->table_bindings;
					name	 = &table_import->table.name;
					index	 = num_table_imports_++;
				}
				break;

			case ExternalKind::Memory:
				if (auto* memory_import = cast<MemoryImport>(import)) {
					bindings = &module_->memory_bindings;
					name	 = &memory_import->memory.name;
					index	 = num_memory_imports_++;
				}
				break;

			case ExternalKind::Global:
				if (auto* global_import = cast<GlobalImport>(import)) {
					bindings = &module_->global_bindings;
					name	 = &global_import->global.name;
					index	 = num_global_imports_++;
				}
				break;

			case ExternalKind::Except:
				if (auto* except_import = cast<ExceptionImport>(import)) {
					bindings = &module_->except_bindings;
					name	 = &except_import->except.name;
					index	 = num_exception_imports_++;
				}
				break;
		}

		if (bindings && name) {
			ISO_ASSERT(index != kInvalidIndex);
			string new_name = str('$') + import->module_name + str('.') + import->field_name;
			MaybeUseAndBindName(bindings, new_name, index, *name);
		}
	}

	for (auto export_ : module->exports) {
		BindingHash* bindings	= nullptr;
		string*		 name		= nullptr;
		Index		 index		= kInvalidIndex;

		switch (export_->kind) {
			case ExternalKind::Func:
				if (Func* func = module_->GetFunc(export_->var)) {
					index	 = module_->GetFuncIndex(export_->var);
					bindings = &module_->func_bindings;
					name	 = &func->name;
				}
				break;

			case ExternalKind::Table:
				if (Table* table = module_->GetTable(export_->var)) {
					index	 = module_->GetTableIndex(export_->var);
					bindings = &module_->table_bindings;
					name	 = &table->name;
				}
				break;

			case ExternalKind::Memory:
				if (Memory* memory = module_->GetMemory(export_->var)) {
					index	 = module_->GetMemoryIndex(export_->var);
					bindings = &module_->memory_bindings;
					name	 = &memory->name;
				}
				break;

			case ExternalKind::Global:
				if (Global* global = module_->GetGlobal(export_->var)) {
					index	 = module_->GetGlobalIndex(export_->var);
					bindings = &module_->global_bindings;
					name	 = &global->name;
				}
				break;

			case ExternalKind::Except:
				if (Exception* except = module_->GetExcept(export_->var)) {
					index	 = module_->GetExceptIndex(export_->var);
					bindings = &module_->except_bindings;
					name	 = &except->name;
				}
				break;
		}

		if (bindings && name) {
			string new_name = str('$') + export_->name;
			MaybeUseAndBindName(bindings, new_name, index, *name);
		}
	}

	for (auto &i : module->globals)
		MaybeGenerateAndBindName(&module_->global_bindings, "$g", module->globals.index_of(i), i->name);

	for (auto &i : module->func_types)
		MaybeGenerateAndBindName(&module_->func_type_bindings, "$t", module->func_types.index_of(i), i->name);

	for (auto &func : module->funcs) {
		MaybeGenerateAndBindName(&module_->func_bindings, "$f", module->funcs.index_of(func), func->name);
		GenerateAndBindLocalNames(func->param_bindings.MakeReverseMapping(func->decl.sig.param_types.size()), &func->param_bindings, "$p");
		GenerateAndBindLocalNames(func->local_bindings.MakeReverseMapping(func->local_types.size()), &func->local_bindings, "$l");
		label_count_ = 0;
		CHECK_RESULT(visitor_.VisitFunc(func));
	}

	for (auto &i : module->tables)
		MaybeGenerateAndBindName(&module_->table_bindings, "$T", module->tables.index_of(i), i->name);

	for (auto &i : module->memories)
		MaybeGenerateAndBindName(&module_->memory_bindings, "$M", module->memories.index_of(i), i->name);

	for (auto &i : module->excepts)
		MaybeGenerateAndBindName(&module_->except_bindings, "$e", module->excepts.index_of(i), i->name);

	module_ = nullptr;
	return true;
}

//-----------------------------------------------------------------------------
// NameApplier
//-----------------------------------------------------------------------------

class NameApplier : public ExprVisitor::Delegate {
	Module*		module_			= nullptr;
	Func*		current_func_	= nullptr;
	ExprVisitor visitor_;
	/* mapping from param index to its name, if any, for the current func */
	dynamic_array<string> param_index_to_name_;
	dynamic_array<string> local_index_to_name_;
	dynamic_array<string> labels_;

	void		PushLabel(const string& label) { labels_.push_back(label); }
	void		PopLabel() { labels_.pop_back(); }

	const char*	FindLabelByVar(Var* var);

	void UseNameForVar(const char* name, Var* var) {
		if (var->is_name()) {
			ISO_ASSERT(name == var->name());
			return;
		}
		if (name)
			var->set_name(name);
	}

	bool UseNameForFuncTypeVar(Var* var) {
		if (FuncType* func_type = module_->GetFuncType(*var)) {
			UseNameForVar(func_type->name, var);
			return true;
		}
		return false;
	}

	bool UseNameForFuncVar(Var* var) {
		if (Func* func = module_->GetFunc(*var)) {
			UseNameForVar(func->name, var);
			return true;
		}
		return false;
	}

	bool UseNameForGlobalVar(Var* var) {
		if (Global* global = module_->GetGlobal(*var)) {
			UseNameForVar(global->name, var);
			return true;
		}
		return false;
	}

	bool UseNameForTableVar(Var* var) {
		if (Table* table = module_->GetTable(*var)) {
			UseNameForVar(table->name, var);
			return true;
		}
		return false;
	}

	bool UseNameForMemoryVar(Var* var) {
		if (Memory* memory = module_->GetMemory(*var)) {
			UseNameForVar(memory->name, var);
			return true;
		}
		return false;
	}

	bool UseNameForExceptVar(Var* var) {
		if (Exception* except = module_->GetExcept(*var)) {
			UseNameForVar(except->name, var);
			return true;
		}
		return false;
	}

	bool		UseNameForParamAndLocalVar(Func* func, Var* var);
	bool		VisitElemSegment(Index elem_segment_index, ElemSegment* segment);
	bool		VisitDataSegment(Index data_segment_index, DataSegment* segment);

public:
	NameApplier() : visitor_(*this) {}

	bool VisitModule(Module* module);

	// Implementation of ExprVisitor::DelegateNop.
	bool BeginBlockExpr(BlockExpr* expr) override {
		PushLabel(expr->block.label);
		return true;
	}
	bool EndBlockExpr(BlockExpr* expr) override {
		PopLabel();
		return true;
	}
	bool BeginLoopExpr(LoopExpr* expr) override {
		PushLabel(expr->block.label);
		return true;
	}
	bool EndLoopExpr(LoopExpr* expr) override {
		PopLabel();
		return true;
	}
	bool OnBrExpr(BrExpr* expr) override {
		auto label = FindLabelByVar(&expr->var);
		UseNameForVar(label, &expr->var);
		return true;
	}
	bool OnBrIfExpr(BrIfExpr* expr) override {
		auto label = FindLabelByVar(&expr->var);
		UseNameForVar(label, &expr->var);
		return true;
	}
	bool OnBrTableExpr(BrTableExpr* expr) override {
		for (Var& target : expr->targets) {
			auto label = FindLabelByVar(&target);
			UseNameForVar(label, &target);
		}
		auto label = FindLabelByVar(&expr->default_target);
		UseNameForVar(label, &expr->default_target);
		return true;
	}
	bool BeginTryExpr(TryExpr* expr) override {
		PushLabel(expr->block.label);
		return true;
	}
	bool EndTryExpr(TryExpr*) override {
		PopLabel();
		return true;
	}
	bool OnThrowExpr(ThrowExpr* expr) override {
		return UseNameForExceptVar(&expr->var);
	}
	bool OnCallExpr(CallExpr* expr) override {
		return UseNameForFuncVar(&expr->var);
	}
	bool OnCallIndirectExpr(CallIndirectExpr* expr) override {
		return !expr->decl.has_func_type || UseNameForFuncTypeVar(&expr->decl.type_var);
	}
	bool OnGetGlobalExpr(GetGlobalExpr* expr) override {
		return UseNameForGlobalVar(&expr->var);
	}
	bool OnGetLocalExpr(GetLocalExpr* expr) override {
		return UseNameForParamAndLocalVar(current_func_, &expr->var);
	}
	bool BeginIfExpr(IfExpr* expr) override {
		PushLabel(expr->true_.label);
		return true;
	}
	bool EndIfExpr(IfExpr* expr) override {
		PopLabel();
		return true;
	}
	bool BeginIfExceptExpr(IfExceptExpr* expr) override {
		PushLabel(expr->true_.label);
		return UseNameForExceptVar(&expr->except_var);
	}
	bool EndIfExceptExpr(IfExceptExpr* expr) override {
		PopLabel();
		return true;
	}
	bool OnSetGlobalExpr(SetGlobalExpr* expr) override {
		return UseNameForGlobalVar(&expr->var);
	}
	bool OnSetLocalExpr(SetLocalExpr* expr) override {
		return UseNameForParamAndLocalVar(current_func_, &expr->var);
	}
	bool OnTeeLocalExpr(TeeLocalExpr* expr) override {
		return UseNameForParamAndLocalVar(current_func_, &expr->var);
	}
};

const char* NameApplier::FindLabelByVar(Var* var) {
	if (var->is_name()) {
		for (auto &label : reversed(labels_)) {
			if (label == var->name())
				return label;
		}
		return nullptr;

	} else {
		if (var->index() >= labels_.size())
			return nullptr;

		return labels_[labels_.size() - 1 - var->index()];
	}
}

bool NameApplier::UseNameForParamAndLocalVar(Func* func, Var* var) {
	Index local_index = func->GetLocalIndex(*var);
	if (local_index >= func->GetNumParamsAndLocals())
		return false;

	Index			num_params = func->GetNumParams();
	const char*		name;
	if (local_index < num_params) {
		/* param */
		ISO_ASSERT(local_index < param_index_to_name_.size());
		name = param_index_to_name_[local_index];
	} else {
		/* local */
		local_index -= num_params;
		ISO_ASSERT(local_index < local_index_to_name_.size());
		name = local_index_to_name_[local_index];
	}

	if (var->is_name()) {
		ISO_ASSERT(name == var->name());
		return true;
	}

	if (name)
		var->set_name(name);

	return true;
}

bool NameApplier::VisitModule(Module* module) {
	module_ = module;
	for (Func *func : module->funcs) {
		current_func_ = func;
		if (func->decl.has_func_type)
			CHECK_RESULT(UseNameForFuncTypeVar(&func->decl.type_var));

		param_index_to_name_ = func->param_bindings.MakeReverseMapping(func->decl.sig.param_types.size());
		local_index_to_name_ = func->local_bindings.MakeReverseMapping(func->local_types.size());

		CHECK_RESULT(visitor_.VisitFunc(func));
		current_func_ = nullptr;
	}

	for (auto i : module->globals)
		CHECK_RESULT(visitor_.VisitExprList(i->init_expr));

	for (auto i : module->exports) {
		if (i->kind == ExternalKind::Func)
			UseNameForFuncVar(&i->var);
	}
	for (auto segment : module->elem_segments) {
		CHECK_RESULT(UseNameForTableVar(&segment->table_var));
		CHECK_RESULT(visitor_.VisitExprList(segment->offset));
		for (Var& var : segment->vars)
			CHECK_RESULT(UseNameForFuncVar(&var));
	}

	for (auto segment : module->data_segments) {
		CHECK_RESULT(UseNameForMemoryVar(&segment->memory_var));
		CHECK_RESULT(visitor_.VisitExprList(segment->offset));
	}
	
	module_ = nullptr;
	return true;
}

//-----------------------------------------------------------------------------
//	C output
//-----------------------------------------------------------------------------

template <int> struct Name {
	explicit Name(const string& name) : name(name) {}
	const string& name;
};

typedef Name<0> LocalName;
typedef Name<1> GlobalName;
typedef Name<2> ExternalPtr;
typedef Name<3> ExternalRef;

struct GotoLabel {
	explicit GotoLabel(const Var& var) : var(var) {}
	const Var& var;
};

struct LabelDecl {
	explicit LabelDecl(const string& name) : name(name) {}
	string name;
};

struct GlobalVar {
	explicit GlobalVar(const Var& var) : var(var) {}
	const Var& var;
};

struct StackVar {
	explicit StackVar(Index index, Type type = Type::Any)
		: index(index), type(type) {
	}
	Index index;
	Type type;
};

struct TypeEnum {
	explicit TypeEnum(Type type) : type(type) {}
	Type type;
};

struct SignedType {
	explicit SignedType(Type type) : type(type) {}
	Type type;
};

struct ResultType {
	explicit ResultType(const TypeVector& types) : types(types) {}
	const TypeVector& types;
};

int GetShiftMask(Type type) {
	switch (type) {
		case Type::I32: return 31;
		case Type::I64: return 63;
		default: unreachable(); return 0;
	}
}

class CWriter {
	struct Newline {};
	struct OpenBrace {};
	struct CloseBrace {};
	struct Label {
		LabelType			label_type;
		const string&		name;
		const TypeVector&	sig;
		size_t				type_stack_size;
		bool				used;
		Label(LabelType label_type, const string& name, const TypeVector& sig, size_t type_stack_size, bool used = false) : label_type(label_type), name(name), sig(sig), type_stack_size(type_stack_size), used(used) {}
		bool HasValue() const { return label_type != LabelType::Loop && !sig.empty(); }
	};
	enum class WriteExportsKind {
		Declarations,
		Definitions,
		Initializers,
	};
	enum class AssignOp {
		Disallowed,
		Allowed,
	};

	typedef set<string>					SymbolSet;
	typedef map<string, string>			SymbolMap;
	typedef pair<Index, Type>			StackTypePair;
	typedef map<StackTypePair, string>	StackVarSymbolMap;

	const Module*	module_		= nullptr;
	const Func*		func_		= nullptr;
	string_accum	&c_acc, &h_acc, *acc;
	string			header_name_;
	int				indent_ = 0;
	bool			should_write_indent_next_ = false;

	SymbolMap				global_sym_map_;
	SymbolMap				local_sym_map_;
	StackVarSymbolMap		stack_var_sym_map_;
	SymbolSet				global_syms_;
	SymbolSet				local_syms_;
	SymbolSet				import_syms_;
	TypeVector				type_stack_;
	dynamic_array<Label>	label_stack_;

	static string AddressOf(const string&);
	static string Deref(const string&);

	static char		MangleType(Type);
	static string	MangleTypes(const TypeVector&);
	static string	MangleName(count_string);
	static string	MangleFuncName(count_string, const TypeVector& param_types, const TypeVector& result_types);
	static string	MangleGlobalName(count_string, Type);
	static string	LegalizeName(count_string);
	static string	ExportName(count_string mangled_name);


	void	WriteCHeader();
	void	WriteCSource();

	size_t	MarkTypeStack() const;
	void	ResetTypeStack(size_t mark);
	Type	StackType(Index) const;
	void	PushType(Type);
	void	PushTypes(const TypeVector&);
	void	DropTypes(size_t count);

	void	PushLabel(LabelType, const string& name, const FuncSignature&, bool used = false);
	const Label* FindLabel(const Var& var);
	bool	IsTopLabelUsed() const;
	void	PopLabel();

	string	DefineName(SymbolSet*, count_string);
	string	DefineImportName(const string& name, count_string module_name, count_string mangled_field_name);
	string	DefineGlobalScopeName(const string&);
	string	DefineLocalScopeName(const string&);
	string	DefineStackVarName(Index, Type, count_string);
	string	GetGlobalName(const string&) const;
	string	GenerateHeaderGuard() const;

	void	Indent(int size = INDENT_SIZE) { indent_ += size; }
	void	Dedent(int size = INDENT_SIZE) { 	indent_ -= size; ISO_ASSERT(indent_ >= 0); }
	void	WriteIndent() {
		if (should_write_indent_next_) {
			(*acc) << repeat(' ', indent_);
			should_write_indent_next_ = false;
		}
	}

	void Write() {}

	template<typename T> void Write(const T &t) {
		WriteIndent();
		(*acc) << t;
	}
	void WriteData(const char* src, size_t size) {
		WriteIndent();
		acc->merge(src, size);
	}
	void Writef(const char* format, ...) {
		va_list valist;
		va_start(valist, format);
		WriteIndent();
		acc->vformat(format, valist);
	}
	template <typename T, typename U, typename... Args> void Write(T&& t, U&& u, Args&&... args) {
		Write(forward<T>(t));
		Write(forward<U>(u));
		Write(forward<Args>(args)...);
	}
	void Write(Newline) {
		Write("\n");
		should_write_indent_next_ = true;
	}
	void Write(OpenBrace) {
		Write("{");
		Indent();
		Write(Newline());
	}
	void Write(CloseBrace) {
		Dedent();
		Write("}");
	}
	void	Write(const LocalName&);
	void	Write(const GlobalName&);
	void	Write(const ExternalPtr&);
	void	Write(const ExternalRef&);
	void	Write(Type);
	void	Write(SignedType);
	void	Write(TypeEnum);
	void	Write(const Var&);
	void	Write(const GotoLabel&);
	void	Write(const LabelDecl&);
	void	Write(const GlobalVar&);
	void	Write(const StackVar&);
	void	Write(const ResultType&);
	void	Write(const Const&);
	void	WriteInitExpr(const ExprList&);
	void	InitGlobalSymbols();
	void	WriteSourceTop();
	void	WriteFuncTypes();
	void	WriteImports();
	void	WriteFuncDeclarations();
	void	WriteFuncDeclaration(const FuncDeclaration&, const string&);
	void	WriteGlobals();
	void	WriteGlobal(const Global&, const string&);
	void	WriteMemories();
	void	WriteMemory(const string&);
	void	WriteTables();
	void	WriteTable(const string&);
	void	WriteDataInitializers();
	void	WriteElemInitializers();
	void	WriteInitExports();
	void	WriteExports(WriteExportsKind);
	void	WriteInit();
	void	WriteFuncs();
	void	Write(const Func&);
	void	WriteParams();
	void	WriteLocals();
	void	WriteStackVarDeclarations();
	void	Write(const ExprList&);


	void	WriteSimpleUnaryExpr(Opcode, const char* op);
	void	WriteInfixBinaryExpr(Opcode, const char* op, AssignOp = AssignOp::Allowed);
	void	WritePrefixBinaryExpr(Opcode, const char* op);
	void	WriteSignedBinaryExpr(Opcode, const char* op);
	void	Write(const BinaryExpr&);
	void	Write(const CompareExpr&);
	void	Write(const ConvertExpr&);
	void	Write(const LoadExpr&);
	void	Write(const StoreExpr&);
	void	Write(const UnaryExpr&);
	void	Write(const TernaryExpr&);
	void	Write(const SimdLaneOpExpr&);
	void	Write(const SimdShuffleOpExpr&);
public:
	enum {INDENT_SIZE = 2};

	CWriter(string_accum &c_acc, string_accum &h_acc, const char* header_name) : c_acc(c_acc), h_acc(h_acc), header_name_(header_name) {}

	bool WriteModule(const Module&);
};

static const char kImplicitFuncLabel[] = "$Bfunc";

static const char* s_global_symbols[] = {
	// keywords
	"_Alignas", "_Alignof", "asm", "_Atomic", "auto", "_Bool", "break", "case",
	"char", "_Complex", "const", "continue", "default", "do", "double", "else",
	"enum", "extern", "float", "for", "_Generic", "goto", "if", "_Imaginary",
	"inline", "int", "long", "_Noreturn", "_Pragma", "register", "restrict",
	"return", "short", "signed", "sizeof", "static", "_Static_assert", "struct",
	"switch", "_Thread_local", "typedef", "union", "unsigned", "void",
	"volatile", "while",

	// ISO_ASSERT.h
	"ISO_ASSERT", "static_assert",

	// math.h
	"abs", "acos", "acosh", "asin", "asinh", "atan", "atan2", "atanh", "cbrt",
	"ceil", "copysign", "cos", "cosh", "double_t", "erf", "erfc", "exp", "exp2",
	"expm1", "fabs", "fdim", "float_t", "floor", "fma", "fmax", "fmin", "fmod",
	"fpclassify", "FP_FAST_FMA", "FP_FAST_FMAF", "FP_FAST_FMAL", "FP_ILOGB0",
	"FP_ILOGBNAN", "FP_INFINITE", "FP_NAN", "FP_NORMAL", "FP_SUBNORMAL",
	"FP_ZERO", "frexp", "HUGE_VAL", "HUGE_VALF", "HUGE_VALL", "hypot", "ilogb",
	"INFINITY", "isfinite", "isgreater", "isgreaterequal", "isinf", "isless",
	"islessequal", "islessgreater", "isnan", "isnormal", "isunordered", "ldexp",
	"lgamma", "llrint", "llround", "log", "log10", "log1p", "log2", "logb",
	"lrint", "lround", "MATH_ERREXCEPT", "math_errhandling", "MATH_ERRNO",
	"modf", "nan", "NAN", "nanf", "nanl", "nearbyint", "nextafter",
	"nexttoward", "pow", "remainder", "remquo", "rint", "round", "scalbln",
	"scalbn", "signbit", "sin", "sinh", "sqrt", "tan", "tanh", "tgamma",
	"trunc",

	// stdint.h
	"INT16_C", "INT16_MAX", "INT16_MIN", "int16_t", "INT32_MAX", "INT32_MIN",
	"int32_t", "INT64_C", "INT64_MAX", "INT64_MIN", "int64_t", "INT8_C",
	"INT8_MAX", "INT8_MIN", "int8_t", "INT_FAST16_MAX", "INT_FAST16_MIN",
	"int_fast16_t", "INT_FAST32_MAX", "INT_FAST32_MIN", "int_fast32_t",
	"INT_FAST64_MAX", "INT_FAST64_MIN", "int_fast64_t", "INT_FAST8_MAX",
	"INT_FAST8_MIN", "int_fast8_t", "INT_LEAST16_MAX", "INT_LEAST16_MIN",
	"int_least16_t", "INT_LEAST32_MAX", "INT_LEAST32_MIN", "int_least32_t",
	"INT_LEAST64_MAX", "INT_LEAST64_MIN", "int_least64_t", "INT_LEAST8_MAX",
	"INT_LEAST8_MIN", "int_least8_t", "INTMAX_C", "INTMAX_MAX", "INTMAX_MIN",
	"intmax_t", "INTPTR_MAX", "INTPTR_MIN", "intptr_t", "PTRDIFF_MAX",
	"PTRDIFF_MIN", "SIG_ATOMIC_MAX", "SIG_ATOMIC_MIN", "SIZE_MAX", "UINT16_C",
	"UINT16_MAX", "uint16_t", "UINT32_C", "UINT32_MAX", "uint32_t", "UINT64_C",
	"UINT64_MAX", "uint64_t", "UINT8_MAX", "uint8_t", "UINT_FAST16_MAX",
	"uint_fast16_t", "UINT_FAST32_MAX", "uint_fast32_t", "UINT_FAST64_MAX",
	"uint_fast64_t", "UINT_FAST8_MAX", "uint_fast8_t", "UINT_LEAST16_MAX",
	"uint_least16_t", "UINT_LEAST32_MAX", "uint_least32_t", "UINT_LEAST64_MAX",
	"uint_least64_t", "UINT_LEAST8_MAX", "uint_least8_t", "UINTMAX_C",
	"UINTMAX_MAX", "uintmax_t", "UINTPTR_MAX", "uintptr_t", "UNT8_C",
	"WCHAR_MAX", "WCHAR_MIN", "WINT_MAX", "WINT_MIN",

	// stdlib.h
	"abort", "abs", "atexit", "atof", "atoi", "atol", "atoll", "at_quick_exit",
	"bsearch", "calloc", "div", "div_t", "exit", "_Exit", "EXIT_FAILURE",
	"EXIT_SUCCESS", "free", "getenv", "labs", "ldiv", "ldiv_t", "llabs",
	"lldiv", "lldiv_t", "malloc", "MB_CUR_MAX", "mblen", "mbstowcs", "mbtowc",
	"qsort", "quick_exit", "rand", "RAND_MAX", "realloc", "size_t", "srand",
	"strtod", "strtof", "strtol", "strtold", "strtoll", "strtoul", "strtoull",
	"system", "wcstombs", "wctomb",

	// string.h
	"memchr", "memcmp", "memcpy", "memmove", "memset", "NULL", "size_t",
	"strcat", "strchr", "strcmp", "strcoll", "strcpy", "strcspn", "strerror",
	"strlen", "strncat", "strncmp", "strncpy", "strpbrk", "strrchr", "strspn",
	"strstr", "strtok", "strxfrm",

	// defined
	"CALL_INDIRECT", "DEFINE_LOAD", "DEFINE_REINTERPRET", "DEFINE_STORE",
	"DIVREM_U", "DIV_S", "DIV_U", "f32", "f32_load", "f32_reinterpret_i32",
	"f32_store", "f64", "f64_load", "f64_reinterpret_i64", "f64_store", "FMAX",
	"FMIN", "FUNC_EPILOGUE", "FUNC_PROLOGUE", "func_types", "I32_CLZ",
	"I32_CLZ", "I32_DIV_S", "i32_load", "i32_load16_s", "i32_load16_u",
	"i32_load8_s", "i32_load8_u", "I32_POPCNT", "i32_reinterpret_f32",
	"I32_REM_S", "I32_ROTL", "I32_ROTR", "i32_store", "i32_store16",
	"i32_store8", "I32_TRUNC_S_F32", "I32_TRUNC_S_F64", "I32_TRUNC_U_F32",
	"I32_TRUNC_U_F64", "I64_CTZ", "I64_CTZ", "I64_DIV_S", "i64_load",
	"i64_load16_s", "i64_load16_u", "i64_load32_s", "i64_load32_u",
	"i64_load8_s", "i64_load8_u", "I64_POPCNT", "i64_reinterpret_f64",
	"I64_REM_S", "I64_ROTL", "I64_ROTR", "i64_store", "i64_store16",
	"i64_store32", "i64_store8", "I64_TRUNC_S_F32", "I64_TRUNC_S_F64",
	"I64_TRUNC_U_F32", "I64_TRUNC_U_F64", "init", "init_elem_segment",
	"init_func_types", "init_globals", "init_memory", "init_table", "LIKELY",
	"MEMCHECK", "REM_S", "REM_U", "ROTL", "ROTR", "s16", "s32", "s64", "s8",
	"TRAP", "TRUNC_S", "TRUNC_U", "Type", "u16", "u32", "u64", "u8", "UNLIKELY",
	"UNREACHABLE", "WASM_RT_ADD_PREFIX", "wasm_rt_allocate_memory",
	"wasm_rt_allocate_table", "wasm_rt_anyfunc_t", "wasm_rt_call_stack_depth",
	"wasm_rt_elem_t", "WASM_RT_F32", "WASM_RT_F64", "wasm_rt_grow_memory",
	"WASM_RT_I32", "WASM_RT_I64", "WASM_RT_INCLUDED_",
	"WASM_RT_MAX_CALL_STACK_DEPTH", "wasm_rt_memory_t", "WASM_RT_MODULE_PREFIX",
	"WASM_RT_PASTE_", "WASM_RT_PASTE", "wasm_rt_register_func_type",
	"wasm_rt_table_t", "wasm_rt_trap", "WASM_RT_TRAP_CALL_INDIRECT",
	"WASM_RT_TRAP_DIV_BY_ZERO", "WASM_RT_TRAP_EXHAUSTION",
	"WASM_RT_TRAP_INT_OVERFLOW", "WASM_RT_TRAP_INVALID_CONVERSION",
	"WASM_RT_TRAP_NONE", "WASM_RT_TRAP_OOB", "wasm_rt_trap_t",
	"WASM_RT_TRAP_UNREACHABLE",

};

const char s_header_top[] =
"#ifdef __cplusplus\n"
"extern \"C\" {\n"
"#endif\n"
"\n"
"#include <stdint.h>\n"
"\n"
"#include \"wasm-rt.h\"\n"
"\n"
"#ifndef WASM_RT_MODULE_PREFIX\n"
"#define WASM_RT_MODULE_PREFIX\n"
"#endif\n"
"\n"
"#define WASM_RT_PASTE_(x, y) x ## y\n"
"#define WASM_RT_PASTE(x, y) WASM_RT_PASTE_(x, y)\n"
"#define WASM_RT_ADD_PREFIX(x) WASM_RT_PASTE(WASM_RT_MODULE_PREFIX, x)\n"
"\n"
"/* TODO(binji): only use stdint.h types in header */\n"
"typedef uint8_t u8;\n"
"typedef int8_t s8;\n"
"typedef uint16_t u16;\n"
"typedef int16_t s16;\n"
"typedef uint32_t u32;\n"
"typedef int32_t s32;\n"
"typedef uint64_t u64;\n"
"typedef int64_t s64;\n"
"typedef float f32;\n"
"typedef double f64;\n"
"\n"
"extern void WASM_RT_ADD_PREFIX(init)(void);\n"
;

const char s_header_bottom[] =
"#ifdef __cplusplus\n"
"}\n"
"#endif\n"
;

const char s_source_includes[] =
"#include <assert.h>\n"
"#include <math.h>\n"
"#include <stdlib.h>\n"
"#include <string.h>\n"
;

const char s_source_declarations[] =
"#define UNLIKELY(x) __builtin_expect(!!(x), 0)\n"
"#define LIKELY(x) __builtin_expect(!!(x), 1)\n"
"\n"
"#define TRAP(x) (wasm_rt_trap(WASM_RT_TRAP_##x), 0)\n"
"\n"
"#define FUNC_PROLOGUE                                            \\\n"
"  if (++wasm_rt_call_stack_depth > WASM_RT_MAX_CALL_STACK_DEPTH) \\\n"
"    TRAP(EXHAUSTION)\n"
"\n"
"#define FUNC_EPILOGUE --wasm_rt_call_stack_depth\n"
"\n"
"#define UNREACHABLE TRAP(UNREACHABLE)\n"
"\n"
"#define CALL_INDIRECT(table, t, ft, x, ...)          \\\n"
"  (LIKELY((x) < table.size && table.data[x].func &&  \\\n"
"          table.data[x].func_type == func_types[ft]) \\\n"
"       ? ((t)table.data[x].func)(__VA_ARGS__)        \\\n"
"       : TRAP(CALL_INDIRECT))\n"
"\n"
"#define MEMCHECK(mem, a, t)  \\\n"
"  if (UNLIKELY((a) + sizeof(t) > mem->size)) TRAP(OOB)\n"
"\n"
"#define DEFINE_LOAD(name, t1, t2, t3)              \\\n"
"  static inline t3 name(wasm_rt_memory_t* mem, u64 addr) {   \\\n"
"    MEMCHECK(mem, addr, t1);                       \\\n"
"    t1 result;                                     \\\n"
"    memcpy(&result, &mem->data[addr], sizeof(t1)); \\\n"
"    return (t3)(t2)result;                         \\\n"
"  }\n"
"\n"
"#define DEFINE_STORE(name, t1, t2)                           \\\n"
"  static inline void name(wasm_rt_memory_t* mem, u64 addr, t2 value) { \\\n"
"    MEMCHECK(mem, addr, t1);                                 \\\n"
"    t1 wrapped = (t1)value;                                  \\\n"
"    memcpy(&mem->data[addr], &wrapped, sizeof(t1));          \\\n"
"  }\n"
"\n"
"DEFINE_LOAD(i32_load, u32, u32, u32);\n"
"DEFINE_LOAD(i64_load, u64, u64, u64);\n"
"DEFINE_LOAD(f32_load, f32, f32, f32);\n"
"DEFINE_LOAD(f64_load, f64, f64, f64);\n"
"DEFINE_LOAD(i32_load8_s, s8, s32, u32);\n"
"DEFINE_LOAD(i64_load8_s, s8, s64, u64);\n"
"DEFINE_LOAD(i32_load8_u, u8, u32, u32);\n"
"DEFINE_LOAD(i64_load8_u, u8, u64, u64);\n"
"DEFINE_LOAD(i32_load16_s, s16, s32, u32);\n"
"DEFINE_LOAD(i64_load16_s, s16, s64, u64);\n"
"DEFINE_LOAD(i32_load16_u, u16, u32, u32);\n"
"DEFINE_LOAD(i64_load16_u, u16, u64, u64);\n"
"DEFINE_LOAD(i64_load32_s, s32, s64, u64);\n"
"DEFINE_LOAD(i64_load32_u, u32, u64, u64);\n"
"DEFINE_STORE(i32_store, u32, u32);\n"
"DEFINE_STORE(i64_store, u64, u64);\n"
"DEFINE_STORE(f32_store, f32, f32);\n"
"DEFINE_STORE(f64_store, f64, f64);\n"
"DEFINE_STORE(i32_store8, u8, u32);\n"
"DEFINE_STORE(i32_store16, u16, u32);\n"
"DEFINE_STORE(i64_store8, u8, u64);\n"
"DEFINE_STORE(i64_store16, u16, u64);\n"
"DEFINE_STORE(i64_store32, u32, u64);\n"
"\n"
"#define I32_CLZ(x) ((x) ? __builtin_clz(x) : 32)\n"
"#define I64_CLZ(x) ((x) ? __builtin_clzll(x) : 64)\n"
"#define I32_CTZ(x) ((x) ? __builtin_ctz(x) : 32)\n"
"#define I64_CTZ(x) ((x) ? __builtin_ctzll(x) : 64)\n"
"#define I32_POPCNT(x) (__builtin_popcount(x))\n"
"#define I64_POPCNT(x) (__builtin_popcountll(x))\n"
"\n"
"#define DIV_S(ut, min, x, y)                                 \\\n"
"   ((UNLIKELY((y) == 0)) ?                TRAP(DIV_BY_ZERO)  \\\n"
"  : (UNLIKELY((x) == min && (y) == -1)) ? TRAP(INT_OVERFLOW) \\\n"
"  : (ut)((x) / (y)))\n"
"\n"
"#define REM_S(ut, min, x, y)                                \\\n"
"   ((UNLIKELY((y) == 0)) ?                TRAP(DIV_BY_ZERO) \\\n"
"  : (UNLIKELY((x) == min && (y) == -1)) ? 0                 \\\n"
"  : (ut)((x) % (y)))\n"
"\n"
"#define I32_DIV_S(x, y) DIV_S(u32, INT32_MIN, (s32)x, (s32)y)\n"
"#define I64_DIV_S(x, y) DIV_S(u64, INT64_MIN, (s64)x, (s64)y)\n"
"#define I32_REM_S(x, y) REM_S(u32, INT32_MIN, (s32)x, (s32)y)\n"
"#define I64_REM_S(x, y) REM_S(u64, INT64_MIN, (s64)x, (s64)y)\n"
"\n"
"#define DIVREM_U(op, x, y) \\\n"
"  ((UNLIKELY((y) == 0)) ? TRAP(DIV_BY_ZERO) : ((x) op (y)))\n"
"\n"
"#define DIV_U(x, y) DIVREM_U(/, x, y)\n"
"#define REM_U(x, y) DIVREM_U(%, x, y)\n"
"\n"
"#define ROTL(x, y, mask) \\\n"
"  (((x) << ((y) & (mask))) | ((x) >> (((mask) - (y) + 1) & (mask))))\n"
"#define ROTR(x, y, mask) \\\n"
"  (((x) >> ((y) & (mask))) | ((x) << (((mask) - (y) + 1) & (mask))))\n"
"\n"
"#define I32_ROTL(x, y) ROTL(x, y, 31)\n"
"#define I64_ROTL(x, y) ROTL(x, y, 63)\n"
"#define I32_ROTR(x, y) ROTR(x, y, 31)\n"
"#define I64_ROTR(x, y) ROTR(x, y, 63)\n"
"\n"
"#define FMIN(x, y)                                          \\\n"
"   ((UNLIKELY((x) != (x))) ? NAN                            \\\n"
"  : (UNLIKELY((y) != (y))) ? NAN                            \\\n"
"  : (UNLIKELY((x) == 0 && (y) == 0)) ? (signbit(x) ? x : y) \\\n"
"  : (x < y) ? x : y)\n"
"\n"
"#define FMAX(x, y)                                          \\\n"
"   ((UNLIKELY((x) != (x))) ? NAN                            \\\n"
"  : (UNLIKELY((y) != (y))) ? NAN                            \\\n"
"  : (UNLIKELY((x) == 0 && (y) == 0)) ? (signbit(x) ? y : x) \\\n"
"  : (x > y) ? x : y)\n"
"\n"
"#define TRUNC_S(ut, st, ft, min, max, maxop, x)                             \\\n"
"   ((UNLIKELY((x) != (x))) ? TRAP(INVALID_CONVERSION)                       \\\n"
"  : (UNLIKELY((x) < (ft)(min) || (x) maxop (ft)(max))) ? TRAP(INT_OVERFLOW) \\\n"
"  : (ut)(st)(x))\n"
"\n"
"#define I32_TRUNC_S_F32(x) TRUNC_S(u32, s32, f32, INT32_MIN, INT32_MAX, >=, x)\n"
"#define I64_TRUNC_S_F32(x) TRUNC_S(u64, s64, f32, INT64_MIN, INT64_MAX, >=, x)\n"
"#define I32_TRUNC_S_F64(x) TRUNC_S(u32, s32, f64, INT32_MIN, INT32_MAX, >,  x)\n"
"#define I64_TRUNC_S_F64(x) TRUNC_S(u64, s64, f64, INT64_MIN, INT64_MAX, >=, x)\n"
"\n"
"#define TRUNC_U(ut, ft, max, maxop, x)                                    \\\n"
"   ((UNLIKELY((x) != (x))) ? TRAP(INVALID_CONVERSION)                     \\\n"
"  : (UNLIKELY((x) <= (ft)-1 || (x) maxop (ft)(max))) ? TRAP(INT_OVERFLOW) \\\n"
"  : (ut)(x))\n"
"\n"
"#define I32_TRUNC_U_F32(x) TRUNC_U(u32, f32, UINT32_MAX, >=, x)\n"
"#define I64_TRUNC_U_F32(x) TRUNC_U(u64, f32, UINT64_MAX, >=, x)\n"
"#define I32_TRUNC_U_F64(x) TRUNC_U(u32, f64, UINT32_MAX, >,  x)\n"
"#define I64_TRUNC_U_F64(x) TRUNC_U(u64, f64, UINT64_MAX, >=, x)\n"
"\n"
"#define DEFINE_REINTERPRET(name, t1, t2)  \\\n"
"  static inline t2 name(t1 x) {           \\\n"
"    t2 result;                            \\\n"
"    memcpy(&result, &x, sizeof(result));  \\\n"
"    return result;                        \\\n"
"  }\n"
"\n"
"DEFINE_REINTERPRET(f32_reinterpret_i32, u32, f32)\n"
"DEFINE_REINTERPRET(i32_reinterpret_f32, f32, u32)\n"
"DEFINE_REINTERPRET(f64_reinterpret_i64, u64, f64)\n"
"DEFINE_REINTERPRET(i64_reinterpret_f64, f64, u64)\n"
"\n"
;

size_t CWriter::MarkTypeStack() const {
	return type_stack_.size();
}

void CWriter::ResetTypeStack(size_t mark) {
	ISO_ASSERT(mark <= type_stack_.size());
	type_stack_.erase(type_stack_.begin() + mark, type_stack_.end());
}

Type CWriter::StackType(Index index) const {
	ISO_ASSERT(index < type_stack_.size());
	return *(type_stack_.end() - index - 1);
}

void CWriter::PushType(Type type) {
	type_stack_.push_back(type);
}

void CWriter::PushTypes(const TypeVector& types) {
	type_stack_.insert(type_stack_.end(), types.begin(), types.end());
}

void CWriter::DropTypes(size_t count) {
	ISO_ASSERT(count <= type_stack_.size());
	type_stack_.erase(type_stack_.end() - count, type_stack_.end());
}

void CWriter::PushLabel(LabelType label_type, const string& name, const FuncSignature& sig, bool used) {
	// TODO(binji): Add multi-value support.
	if ((label_type != LabelType::Func && sig.GetNumParams() != 0) || sig.GetNumResults() > 1) {
		//UNIMPLEMENTED("multi value support");
	}

	label_stack_.emplace_back(label_type, name, sig.result_types, type_stack_.size(), used);
}

const CWriter::Label* CWriter::FindLabel(const Var& var) {
	Label* label = nullptr;

	if (var.is_index()) {
		// We've generated names for all labels, so we should only be using an index when branching to the implicit function label, which can't be named
//		ISO_ASSERT(var.index() + 1 == label_stack_.size());
		label = &label_stack_[0];
	} else {
		ISO_ASSERT(var.is_name());
		for (Index i = label_stack_.size32(); i > 0; --i) {
			label = &label_stack_[i - 1];
			if (label->name == var.name())
				break;
		}
	}

	ISO_ASSERT(label);
	label->used = true;
	return label;
}

bool CWriter::IsTopLabelUsed() const {
	ISO_ASSERT(!label_stack_.empty());
	return label_stack_.back().used;
}

void CWriter::PopLabel() {
	label_stack_.pop_back();
}

// static
string CWriter::AddressOf(const string& s) {
	return "(&" + s + ")";
}

// static
string CWriter::Deref(const string& s) {
	return "(*" + s + ")";
}

// static
char CWriter::MangleType(Type type) {
	switch (type) {
		case Type::I32: return 'i';
		case Type::I64: return 'j';
		case Type::F32: return 'f';
		case Type::F64: return 'd';
		default: unreachable();
	}
}

// static
string CWriter::MangleTypes(const TypeVector& types) {
	if (types.empty())
		return string("v");

	string result;
	for (auto type : types)
		result += MangleType(type);
	return result;
}

// static
string CWriter::MangleName(count_string name) {
	const char kPrefix = 'Z';
	string_builder	b;
	b << "Z_";

	if (!name.empty()) {
		for (char c : name) {
			if ((isalnum(c) && c != kPrefix) || c == '_')
				b << c;
			else
				(b << kPrefix).format("%02X", uint8(c));
		}
	}

	return b;
}

// static
string CWriter::MangleFuncName(count_string name, const TypeVector& param_types, const TypeVector& result_types) {
	string sig = MangleTypes(result_types) + MangleTypes(param_types);
	return MangleName(name) + MangleName(count_string(sig));
}

// static
string CWriter::MangleGlobalName(count_string name, Type type) {
	char	sig	= MangleType(type);
	return MangleName(name) + MangleName(count_string(&sig, 1));
}

// static
string CWriter::ExportName(count_string mangled_name) {
	return "WASM_RT_ADD_PREFIX(" + mangled_name + ")";
}

// static
string CWriter::LegalizeName(count_string name) {
	if (name.empty())
		return "_";

	string_builder	b;
	b << (isalpha(name[0]) ? name[0] : '_');
	for (int i = 1; i < name.size32(); ++i)
		b << (isalnum(name[i]) ? name[i] : '_');

	return b;
}

string CWriter::DefineName(SymbolSet* set, count_string name) {
	string legal = LegalizeName(name);
	if (set->find(legal) != set->end()) {
		string base		= legal + "_";
		size_t count	= 0;
		do
			legal = base + to_string(count++);
		while (set->find(legal) != set->end());
	}
	set->insert(legal);
	return legal;
}

count_string StripLeadingDollar(count_string name) {
	if (!name.empty() && name[0] == '$')
		return name.slice(1);
	return name;
}

string CWriter::DefineImportName(const string& name, count_string module, count_string mangled_field_name) {
	string mangled = MangleName(module) + mangled_field_name;
	import_syms_.insert(name);
	global_syms_.insert(mangled);
	global_sym_map_[name] = mangled;
	return "(*" + mangled + ")";
}

string CWriter::DefineGlobalScopeName(const string& name) {
	string unique = DefineName(&global_syms_, StripLeadingDollar(count_string(name)));
	global_sym_map_[name] = unique;
	return unique;
}

string CWriter::DefineLocalScopeName(const string& name) {
	string unique = DefineName(&local_syms_, StripLeadingDollar(count_string(name)));
	local_sym_map_[name] = unique;
	return unique;
}

string CWriter::DefineStackVarName(Index index, Type type, count_string name) {
	string unique = DefineName(&local_syms_, name);
	StackTypePair stp = {index, type};
	stack_var_sym_map_[stp] = unique;
	return unique;
}

void CWriter::Write(const LocalName& name) {
	ISO_ASSERT(local_sym_map_.count(name.name));
	Write(local_sym_map_[name.name]);
}

string CWriter::GetGlobalName(const string& name) const {
	ISO_ASSERT(global_sym_map_.count(name) == 1);
	return global_sym_map_[name];
}

void CWriter::Write(const GlobalName& name) {
	Write(GetGlobalName(name.name));
}

void CWriter::Write(const ExternalPtr& name) {
	bool is_import = import_syms_.count(name.name) != 0;
	if (is_import)
		Write(GetGlobalName(name.name));
	else
		Write(AddressOf(GetGlobalName(name.name)));
}

void CWriter::Write(const ExternalRef& name) {
	bool is_import = import_syms_.count(name.name) != 0;
	if (is_import)
		Write(Deref(GetGlobalName(name.name)));
	else
		Write(GetGlobalName(name.name));
}

void CWriter::Write(const Var& var) {
	if (!var.is_name())
		return;
	ISO_ASSERT(var.is_name());
	Write(LocalName(var.name()));
}

void CWriter::Write(const GotoLabel& goto_label) {
	const Label* label = FindLabel(goto_label.var);
	if (label->HasValue()) {
		ISO_ASSERT(label->sig.size() == 1);
		ISO_ASSERT(type_stack_.size() >= label->type_stack_size);
		Index dst = Index(type_stack_.size() - label->type_stack_size) - 1;
		if (dst != 0)
			Write(StackVar(dst, label->sig[0]), " = ", StackVar(0), "; ");
	}

	if (goto_label.var.is_name())
		Write("goto ", goto_label.var, ";");
	else // We've generated names for all labels, so we should only be using an index when branching to the implicit function label, which can't be named
		Write("goto ", Var(kImplicitFuncLabel), ";");
}

void CWriter::Write(const LabelDecl& label) {
	if (IsTopLabelUsed())
		Write(label.name, ":;", Newline());
}

void CWriter::Write(const GlobalVar& var) {
	if (!var.var.is_name())
		return;
	ISO_ASSERT(var.var.is_name());
	Write(ExternalRef(var.var.name()));
}

void CWriter::Write(const StackVar& sv) {
	Index index = Index(type_stack_.size() - 1 - sv.index);
	Type type = sv.type;
	if (type == Type::Any) {
		ISO_ASSERT(index < type_stack_.size());
		type = type_stack_[index];
	}

	StackTypePair stp = {index, type};
	auto iter = stack_var_sym_map_.find(stp);
	if (iter == stack_var_sym_map_.end())
		Write(DefineStackVarName(index, type, count_string(str(MangleType(type)) + to_string(index))));
	else
		Write(*iter);
}

void CWriter::Write(Type type) {
	switch (type) {
		case Type::I32: Write("u32"); break;
		case Type::I64: Write("u64"); break;
		case Type::F32: Write("f32"); break;
		case Type::F64: Write("f64"); break;
		default:
			unreachable();
	}
}

void CWriter::Write(TypeEnum type) {
	switch (type.type) {
		case Type::I32: Write("WASM_RT_I32"); break;
		case Type::I64: Write("WASM_RT_I64"); break;
		case Type::F32: Write("WASM_RT_F32"); break;
		case Type::F64: Write("WASM_RT_F64"); break;
		default:
			unreachable();
	}
}

void CWriter::Write(SignedType type) {
	switch (type.type) {
		case Type::I32: Write("s32"); break;
		case Type::I64: Write("s64"); break;
		default:
			unreachable();
	}
}

void CWriter::Write(const ResultType& rt) {
	if (!rt.types.empty())
		Write(rt.types[0]);
	else
		Write("void");
}

void CWriter::Write(const Const& const_) {
	switch (const_.type) {
		case Type::I32:
			Write((int32)const_.u32);
			break;

		case Type::I64:
			Write((int64)const_.u64);
			break;

		case Type::F32: {
			// TODO(binji): Share with similar float info in interp.cc and literal.cc
			if ((const_.f32_bits & 0x7f800000u) == 0x7f800000u) {
				const char* sign = (const_.f32_bits & 0x80000000) ? "-" : "";
				uint32 significand = const_.f32_bits & 0x7fffffu;
				if (significand == 0) {
					// Infinity.
					Writef("%sINFINITY", sign);
				} else {
					// Nan.
					Writef("f32_reinterpret_i32(0x%08x) /* %snan:0x%06x */", const_.f32_bits, sign, significand);
				}
			} else if (const_.f32_bits == 0x80000000) {
				// Negative zero. Special-cased so it isn't written as -0 below.
				Writef("-0.f");
			} else {
				Writef("%.9g", iorf(const_.f32_bits).f());
			}
			break;
		}

		case Type::F64:
			// TODO(binji): Share with similar float info in interp.cc and literal.cc
			if ((const_.f64_bits & 0x7ff0000000000000ull) == 0x7ff0000000000000ull) {
				const char* sign = (const_.f64_bits & 0x8000000000000000ull) ? "-" : "";
				uint64 significand = const_.f64_bits & 0xfffffffffffffull;
				if (significand == 0) {
					// Infinity.
					Writef("%sINFINITY", sign);
				} else {
					// Nan.
					Writef("f64_reinterpret_i64(0x%016llx" ") /* %snan:0x%013llx */", const_.f64_bits, sign, significand);
				}
			} else if (const_.f64_bits == 0x8000000000000000ull) {
				// Negative zero. Special-cased so it isn't written as -0 below.
				Writef("-0.0");
			} else {
				Writef("%.17g", iord(const_.f64_bits).f());
			}
			break;

		default:
			unreachable();
	}
}

void CWriter::WriteInitExpr(const ExprList& expr_list) {
	if (expr_list.empty())
		return;

	ISO_ASSERT(expr_list.size() == 1);
	const Expr* expr = &expr_list.front();
	switch (expr_list.front().type) {
		case Expr::Type::Const:			Write(cast<ConstExpr>(expr)->cnst); break;
		case Expr::Type::GetGlobal:		Write(GlobalVar(cast<GetGlobalExpr>(expr)->var)); break;
		default:						unreachable();
	}
}

void CWriter::InitGlobalSymbols() {
	for (const char* symbol : s_global_symbols)
		global_syms_.insert(symbol);
}

string CWriter::GenerateHeaderGuard() const {
	string result;
	for (char c : header_name_) {
		if (isalnum(c) || c == '_')
			result += toupper(c);
		else
			result += '_';
	}
	result += "_GENERATED_";
	return result;
}

void CWriter::WriteSourceTop() {
	Write(s_source_includes);
	Write(Newline(), "#include \"", header_name_, "\"", Newline());
	Write(s_source_declarations);
}

void CWriter::WriteFuncTypes() {
	Write(Newline());
	Writef("static u32 func_types[%u];", module_->func_types.size());
	Write(Newline(), Newline());
	Write("static void init_func_types(void) {", Newline());
	Index func_type_index = 0;
	for (FuncType* func_type : module_->func_types) {
		Index num_params = func_type->GetNumParams();
		Index num_results = func_type->GetNumResults();
		Write("  func_types[", func_type_index, "] = wasm_rt_register_func_type(", num_params, ", ", num_results);
		for (Index i = 0; i < num_params; ++i)
			Write(", ", TypeEnum(func_type->GetParamType(i)));

		for (Index i = 0; i < num_results; ++i)
			Write(", ", TypeEnum(func_type->GetResultType(i)));

		Write(");", Newline());
		++func_type_index;
	}
	Write("}", Newline());
}

void CWriter::WriteImports() {
	if (module_->imports.empty())
		return;

	Write(Newline());

	// TODO(binji): Write imports ordered by type.
	for (const Import* import : module_->imports) {
		Write("/* import: '", import->module_name, "' '", import->field_name, "' */", Newline());
		Write("extern ");
		switch (import->kind) {
			case ExternalKind::Func: {
				const Func& func = cast<FuncImport>(import)->func;
				WriteFuncDeclaration(func.decl, DefineImportName(func.name, count_string(import->module_name), count_string(MangleFuncName(count_string(import->field_name), func.decl.sig.param_types, func.decl.sig.result_types))));
				Write(";");
				break;
			}
			case ExternalKind::Global: {
				const Global& global = cast<GlobalImport>(import)->global;
				WriteGlobal(global, DefineImportName(global.name, count_string(import->module_name), count_string(MangleGlobalName(count_string(import->field_name), global.type))));
				Write(";");
				break;
			}
			case ExternalKind::Memory: {
				const Memory& memory = cast<MemoryImport>(import)->memory;
				WriteMemory(DefineImportName(memory.name, count_string(import->module_name), count_string(MangleName(count_string(import->field_name)))));
				break;
			}
			case ExternalKind::Table: {
				const Table& table = cast<TableImport>(import)->table;
				WriteTable(DefineImportName(table.name, count_string(import->module_name), count_string(MangleName(count_string(import->field_name)))));
				break;
			}
			default:
				unreachable();
		}

		Write(Newline());
	}
}

void CWriter::WriteFuncDeclarations() {
	if (module_->funcs.size() == module_->num_func_imports)
		return;

	Write(Newline());

	Index func_index = 0;
	for (const Func* func : module_->funcs) {
		bool is_import = func_index < module_->num_func_imports;
		if (!is_import) {
			Write("static ");
			WriteFuncDeclaration(func->decl, DefineGlobalScopeName(func->name));
			Write(";", Newline());
		}
		++func_index;
	}
}

void CWriter::WriteFuncDeclaration(const FuncDeclaration& decl, const string& name) {
	Write(ResultType(decl.sig.result_types), " ", name, "(");
	if (decl.GetNumParams() == 0) {
		Write("void");
	} else {
		for (Index i = 0; i < decl.GetNumParams(); ++i) {
			if (i != 0)
				Write(", ");
			Write(decl.GetParamType(i));
		}
	}
	Write(")");
}

void CWriter::WriteGlobals() {
	Index global_index = 0;
	if (module_->globals.size() != module_->num_global_imports) {
		Write(Newline());

		for (const Global* global : module_->globals) {
			bool is_import = global_index < module_->num_global_imports;
			if (!is_import) {
				Write("static ");
				WriteGlobal(*global, DefineGlobalScopeName(global->name));
				Write(";", Newline());
			}
			++global_index;
		}
	}

	Write(Newline(), "static void init_globals(void) ", OpenBrace());
	global_index = 0;
	for (const Global* global : module_->globals) {
		bool is_import = global_index < module_->num_global_imports;
		if (!is_import) {
			ISO_ASSERT(!global->init_expr.empty());
			Write(GlobalName(global->name), " = ");
			WriteInitExpr(global->init_expr);
			Write(";", Newline());
		}
		++global_index;
	}
	Write(CloseBrace(), Newline());
}

void CWriter::WriteGlobal(const Global& global, const string& name) {
	Write(global.type, " ", name);
}

void CWriter::WriteMemories() {
	if (module_->memories.size() == module_->num_memory_imports)
		return;

	Write(Newline());

	ISO_ASSERT(module_->memories.size() <= 1);
	Index memory_index = 0;
	for (const Memory* memory : module_->memories) {
		bool is_import = memory_index < module_->num_memory_imports;
		if (!is_import) {
			Write("static ");
			WriteMemory(DefineGlobalScopeName(memory->name));
			Write(Newline());
		}
		++memory_index;
	}
}

void CWriter::WriteMemory(const string& name) {
	Write("wasm_rt_memory_t ", name, ";");
}

void CWriter::WriteTables() {
	if (module_->tables.size() == module_->num_table_imports)
		return;

	Write(Newline());

	ISO_ASSERT(module_->tables.size() <= 1);
	Index table_index = 0;
	for (const Table* table : module_->tables) {
		bool is_import = table_index < module_->num_table_imports;
		if (!is_import) {
			Write("static ");
			WriteTable(DefineGlobalScopeName(table->name));
			Write(Newline());
		}
		++table_index;
	}
}

void CWriter::WriteTable(const string& name) {
	Write("wasm_rt_table_t ", name, ";");
}

void CWriter::WriteDataInitializers() {
	const Memory* memory = nullptr;
	Index data_segment_index = 0;

	if (!module_->memories.empty()) {
		if (module_->data_segments.empty()) {
			Write(Newline());
		} else {
			for (const DataSegment* data_segment : module_->data_segments) {
				Write(Newline(), "static const u8 data_segment_data_", data_segment_index, "[] = ", OpenBrace());
				size_t i = 0;
				for (uint8 x : make_range<uint8>(data_segment->data)) {
					Writef("0x%02x, ", x);
					if ((++i % 12) == 0)
						Write(Newline());
				}
				if (i > 0)
					Write(Newline());
				Write(CloseBrace(), ";", Newline());
				++data_segment_index;
			}
		}

		memory = module_->memories[0];
	}

	Write(Newline(), "static void init_memory(void) ", OpenBrace());
	if (memory && module_->num_memory_imports == 0) {
		uint32 max = memory->page_limits.has_max ? memory->page_limits.max : 65536;
		Write("wasm_rt_allocate_memory(", ExternalPtr(memory->name), ", ", memory->page_limits.initial, ", ", max, ");", Newline());
	}
	data_segment_index = 0;
	for (const DataSegment* data_segment : module_->data_segments) {
		Write("memcpy(&(", ExternalRef(memory->name), ".data[");
		WriteInitExpr(data_segment->offset);
		Write("]), data_segment_data_", data_segment_index, ", ", data_segment->data.size(), ");", Newline());
		++data_segment_index;
	}

	Write(CloseBrace(), Newline());
}

void CWriter::WriteElemInitializers() {
	const Table* table = module_->tables.empty() ? nullptr : module_->tables[0];

	Write(Newline(), "static void init_table(void) ", OpenBrace());
	Write("uint32 offset;", Newline());
	if (table && module_->num_table_imports == 0) {
		auto max = table->elem_limits.has_max ? table->elem_limits.max : maximum;
		Write("wasm_rt_allocate_table(", ExternalPtr(table->name), ", ", table->elem_limits.initial, ", ", max, ");", Newline());
	}
	Index elem_segment_index = 0;
	for (const ElemSegment* elem_segment : module_->elem_segments) {
		Write("offset = ");
		WriteInitExpr(elem_segment->offset);
		Write(";", Newline());

		size_t i = 0;
		for (const Var& var : elem_segment->vars) {
			const Func* func = module_->GetFunc(var);
			Index func_type_index = module_->GetFuncTypeIndex(func->decl.type_var);

			Write(ExternalRef(table->name), ".data[offset + ", i, "] = (wasm_rt_elem_t){func_types[", func_type_index, "], (wasm_rt_anyfunc_t)", ExternalPtr(func->name), "};", Newline());
			++i;
		}
		++elem_segment_index;
	}

	Write(CloseBrace(), Newline());
}

void CWriter::WriteInitExports() {
	Write(Newline(), "static void init_exports(void) ", OpenBrace());
	WriteExports(WriteExportsKind::Initializers);
	Write(CloseBrace(), Newline());
}

void CWriter::WriteExports(WriteExportsKind kind) {
	if (module_->exports.empty())
		return;

	if (kind != WriteExportsKind::Initializers)
		Write(Newline());

	for (const Export* export_ : module_->exports) {
		Write("/* export: '", export_->name, "' */", Newline());
		if (kind == WriteExportsKind::Declarations)
			Write("extern ");

		string mangled_name;
		string internal_name;

		switch (export_->kind) {
			case ExternalKind::Func: {
				const Func* func = module_->GetFunc(export_->var);
				mangled_name = ExportName(count_string(MangleFuncName(count_string(export_->name), func->decl.sig.param_types, func->decl.sig.result_types)));
				internal_name = func->name;
				if (kind != WriteExportsKind::Initializers) {
					WriteFuncDeclaration(func->decl, Deref(mangled_name));
					Write(";");
				}
				break;
			}

			case ExternalKind::Global: {
				const Global* global = module_->GetGlobal(export_->var);
				mangled_name = ExportName(count_string(MangleGlobalName(count_string(export_->name), global->type)));
				internal_name = global->name;
				if (kind != WriteExportsKind::Initializers) {
					WriteGlobal(*global, Deref(mangled_name));
					Write(";");
				}
				break;
			}

			case ExternalKind::Memory: {
				const Memory* memory = module_->GetMemory(export_->var);
				mangled_name = ExportName(count_string(MangleName(count_string(export_->name))));
				internal_name = memory->name;
				if (kind != WriteExportsKind::Initializers)
					WriteMemory(Deref(mangled_name));
				break;
			}

			case ExternalKind::Table: {
				const Table* table = module_->GetTable(export_->var);
				mangled_name = ExportName(count_string(MangleName(count_string(export_->name))));
				internal_name = table->name;
				if (kind != WriteExportsKind::Initializers)
					WriteTable(Deref(mangled_name));
				break;
			}

			default:
				unreachable();
		}

		if (kind == WriteExportsKind::Initializers)
			Write(mangled_name, " = ", ExternalPtr(internal_name), ";");

		Write(Newline());
	}
}

void CWriter::WriteInit() {
	Write(Newline(), "void WASM_RT_ADD_PREFIX(init)(void) ", OpenBrace());
	Write("init_func_types();", Newline());
	Write("init_globals();", Newline());
	Write("init_memory();", Newline());
	Write("init_table();", Newline());
	Write("init_exports();", Newline());
	for (Var* var : module_->starts)
		Write(ExternalRef(module_->GetFunc(*var)->name), "();", Newline());
	Write(CloseBrace(), Newline());
}

void CWriter::WriteFuncs() {
	Index func_index = 0;
	for (const Func* func : module_->funcs) {
		bool is_import = func_index < module_->num_func_imports;
		if (!is_import)
			Write(Newline(), *func, Newline());
		++func_index;
	}
}

void CWriter::Write(const Func& func) {
	func_ = &func;
	// Copy symbols from global symbol table so we don't shadow them.
	local_syms_ = global_syms_;
	local_sym_map_.clear();
	stack_var_sym_map_.clear();

	Write("static ", ResultType(func.decl.sig.result_types), " ", GlobalName(func.name), "(");
	WriteParams();
	WriteLocals();
	Write("FUNC_PROLOGUE;", Newline());

	string_builder	func_acc;
	{
		auto	save_acc = save(acc, &func_acc);

		string label = DefineLocalScopeName(kImplicitFuncLabel);
		ResetTypeStack(0);
		string empty;	// Must not be temporary, since address is taken by Label.
		PushLabel(LabelType::Func, empty, func.decl.sig);
		Write(func.exprs, LabelDecl(label));
		PopLabel();
		ResetTypeStack(0);
		PushTypes(func.decl.sig.result_types);
		Write("FUNC_EPILOGUE;", Newline());

		if (!func.decl.sig.result_types.empty())	// Return the top of the stack implicitly.
			Write("return ", StackVar(0), ";", Newline());
	}

	WriteStackVarDeclarations();
	*acc << func_acc;

	Write(CloseBrace());

	func_ = nullptr;
}

void CWriter::WriteParams() {
	if (func_->decl.sig.param_types.empty()) {
		Write("void");
	} else {
		auto index_to_name = func_->param_bindings.MakeReverseMapping(func_->decl.sig.param_types.size());
		Indent(4);
		for (Index i = 0; i < func_->GetNumParams(); ++i) {
			if (i != 0) {
				Write(", ");
				if ((i % 8) == 0)
					Write(Newline());
			}
			Write(func_->GetParamType(i), " ", DefineLocalScopeName(index_to_name[i]));
		}
		Dedent(4);
	}
	Write(") ", OpenBrace());
}

void CWriter::WriteLocals() {
	auto index_to_name = func_->local_bindings.MakeReverseMapping(func_->local_types.size());
	Type types[] = {Type::I32, Type::I64, Type::F32, Type::F64};
	for (Type type : types) {
		Index local_index = 0;
		size_t count = 0;
		for (Type local_type : func_->local_types) {
			if (local_type == type) {
				if (count == 0) {
					Write(type, " ");
					Indent(4);
				} else {
					Write(", ");
					if ((count % 8) == 0)
						Write(Newline());
				}

				Write(DefineLocalScopeName(index_to_name[local_index]), " = 0");
				++count;
			}
			++local_index;
		}
		if (count != 0) {
			Dedent(4);
			Write(";", Newline());
		}
	}
}

void CWriter::WriteStackVarDeclarations() {
	Type types[] = {Type::I32, Type::I64, Type::F32, Type::F64};
	for (Type type : types) {
		size_t count = 0;
		for (const auto& pair : stack_var_sym_map_.with_keys()) {
			Type stp_type = pair.a.b;
			const string& name = pair.b;

			if (stp_type == type) {
				if (count == 0) {
					Write(type, " ");
					Indent(4);
				} else {
					Write(", ");
					if ((count % 8) == 0)
						Write(Newline());
				}

				Write(name);
				++count;
			}
		}
		if (count != 0) {
			Dedent(4);
			Write(";", Newline());
		}
	}
}

void CWriter::Write(const ExprList& exprs) {
	for (const Expr& expr : exprs) {
		switch (expr.type) {
			case Expr::Type::Binary:
				Write(*cast<BinaryExpr>(&expr));
				break;

			case Expr::Type::Block: {
				const Block& block = cast<BlockExpr>(&expr)->block;
				string label = DefineLocalScopeName(block.label);
				size_t mark = MarkTypeStack();
				PushLabel(LabelType::Block, block.label, block.decl.sig);
				Write(block.exprs, LabelDecl(label));
				ResetTypeStack(mark);
				PopLabel();
				PushTypes(block.decl.sig.result_types);
				break;
			}
			case Expr::Type::Br:
				Write(GotoLabel(cast<BrExpr>(&expr)->var), Newline());
				// Stop processing this ExprList, since the following are unreachable.
				return;

			case Expr::Type::BrIf:
				Write("if (", StackVar(0), ") {");
				DropTypes(1);
				Write(GotoLabel(cast<BrIfExpr>(&expr)->var), "}", Newline());
				break;

			case Expr::Type::BrTable: {
				const auto* bt_expr = cast<BrTableExpr>(&expr);
				Write("switch (", StackVar(0), ") ", OpenBrace());
				DropTypes(1);
				Index i = 0;
				for (const Var& var : bt_expr->targets)
					Write("case ", i++, ": ", GotoLabel(var), Newline());
				Write("default: ");
				Write(GotoLabel(bt_expr->default_target), Newline(), CloseBrace(), Newline());
				// Stop processing this ExprList, since the following are unreachable.
				return;
			}
			case Expr::Type::Call: {
				const Var& var = cast<CallExpr>(&expr)->var;
				const Func& func = *module_->GetFunc(var);
				Index num_params = func.GetNumParams();
				Index num_results = func.GetNumResults();
				ISO_ASSERT(type_stack_.size() >= num_params);
				if (num_results > 0) {
					ISO_ASSERT(num_results == 1);
					Write(StackVar(num_params - 1, func.GetResultType(0)), " = ");
				}

				Write(GlobalVar(var), "(");
				for (Index i = 0; i < num_params; ++i) {
					if (i != 0)
						Write(", ");
					Write(StackVar(num_params - i - 1));
				}
				Write(");", Newline());
				DropTypes(num_params);
				PushTypes(func.decl.sig.result_types);
				break;
			}
			case Expr::Type::CallIndirect: {
				const FuncDeclaration& decl = cast<CallIndirectExpr>(&expr)->decl;
				Index num_params = decl.GetNumParams();
				Index num_results = decl.GetNumResults();
				ISO_ASSERT(type_stack_.size() > num_params);
				if (num_results > 0) {
					ISO_ASSERT(num_results == 1);
					Write(StackVar(num_params, decl.GetResultType(0)), " = ");
				}

				ISO_ASSERT(module_->tables.size() == 1);
				const Table* table = module_->tables[0];

				ISO_ASSERT(decl.has_func_type);
				Index func_type_index = module_->GetFuncTypeIndex(decl.type_var);

				Write("CALL_INDIRECT(", ExternalRef(table->name), ", ");
				WriteFuncDeclaration(decl, "(*)");
				Write(", ", func_type_index, ", ", StackVar(0));
				for (Index i = 0; i < num_params; ++i)
					Write(", ", StackVar(num_params - i));
				Write(");", Newline());
				DropTypes(num_params + 1);
				PushTypes(decl.sig.result_types);
				break;
			}
			case Expr::Type::Compare:
				Write(*cast<CompareExpr>(&expr));
				break;

			case Expr::Type::Const: {
				const Const& const_ = cast<ConstExpr>(&expr)->cnst;
				PushType(const_.type);
				Write(StackVar(0), " = ", const_, ";", Newline());
				break;
			}
			case Expr::Type::Convert:
				Write(*cast<ConvertExpr>(&expr));
				break;

			case Expr::Type::Drop:
				DropTypes(1);
				break;

			case Expr::Type::GetGlobal: {
				const Var& var = cast<GetGlobalExpr>(&expr)->var;
				PushType(module_->GetGlobal(var)->type);
				Write(StackVar(0), " = ", GlobalVar(var), ";", Newline());
				break;
			}
			case Expr::Type::GetLocal: {
				const Var& var = cast<GetLocalExpr>(&expr)->var;
				PushType(func_->GetLocalType(var));
				Write(StackVar(0), " = ", var, ";", Newline());
				break;
			}
			case Expr::Type::If: {
				const IfExpr& if_ = *cast<IfExpr>(&expr);
				Write("if (", StackVar(0), ") ", OpenBrace());
				DropTypes(1);
				string label = DefineLocalScopeName(if_.true_.label);
				size_t mark = MarkTypeStack();
				PushLabel(LabelType::If, if_.true_.label, if_.true_.decl.sig);
				Write(if_.true_.exprs, CloseBrace());
				if (!if_.false_.empty()) {
					ResetTypeStack(mark);
					Write(" else ", OpenBrace(), if_.false_, CloseBrace());
				}
				ResetTypeStack(mark);
				Write(Newline(), LabelDecl(label));
				PopLabel();
				PushTypes(if_.true_.decl.sig.result_types);
				break;
			}
			case Expr::Type::Load:
				Write(*cast<LoadExpr>(&expr));
				break;

			case Expr::Type::Loop: {
				const Block& block = cast<LoopExpr>(&expr)->block;
				if (!block.exprs.empty()) {
					Write(DefineLocalScopeName(block.label), ": ");
					Indent();
					size_t mark = MarkTypeStack();
					PushLabel(LabelType::Loop, block.label, block.decl.sig);
					Write(Newline(), block.exprs);
					ResetTypeStack(mark);
					PopLabel();
					PushTypes(block.decl.sig.result_types);
					Dedent();
				}
				break;
			}
			case Expr::Type::MemoryGrow: {
				ISO_ASSERT(module_->memories.size() == 1);
				Memory* memory = module_->memories[0];

				Write(StackVar(0), " = wasm_rt_grow_memory(", ExternalPtr(memory->name), ", ", StackVar(0), ");", Newline());
				break;
			}
			case Expr::Type::MemorySize: {
				ISO_ASSERT(module_->memories.size() == 1);
				Memory* memory = module_->memories[0];

				PushType(Type::I32);
				Write(StackVar(0), " = ", ExternalRef(memory->name), ".pages;", Newline());
				break;
			}
			case Expr::Type::Nop:
				break;

			case Expr::Type::Return:
				// Goto the function label instead; this way we can do shared function
				// cleanup code in one place.
				Write(GotoLabel(Var(label_stack_.size32() - 1)), Newline());
				// Stop processing this ExprList, since the following are unreachable.
				return;

			case Expr::Type::Select: {
				Type type = StackType(1);
				Write(StackVar(2), " = ", StackVar(0), " ? ", StackVar(2), " : ", StackVar(1), ";", Newline());
				DropTypes(3);
				PushType(type);
				break;
			}
			case Expr::Type::SetGlobal: {
				const Var& var = cast<SetGlobalExpr>(&expr)->var;
				Write(GlobalVar(var), " = ", StackVar(0), ";", Newline());
				DropTypes(1);
				break;
			}
			case Expr::Type::SetLocal: {
				const Var& var = cast<SetLocalExpr>(&expr)->var;
				Write(var, " = ", StackVar(0), ";", Newline());
				DropTypes(1);
				break;
			}
			case Expr::Type::Store:
				Write(*cast<StoreExpr>(&expr));
				break;

			case Expr::Type::TeeLocal: {
				const Var& var = cast<TeeLocalExpr>(&expr)->var;
				Write(var, " = ", StackVar(0), ";", Newline());
				break;
			}
			case Expr::Type::Unary:
				Write(*cast<UnaryExpr>(&expr));
				break;

			case Expr::Type::Ternary:
				Write(*cast<TernaryExpr>(&expr));
				break;

			case Expr::Type::SimdLaneOp: {
				Write(*cast<SimdLaneOpExpr>(&expr));
				break;
			}
			case Expr::Type::SimdShuffleOp: {
				Write(*cast<SimdShuffleOpExpr>(&expr));
				break;
			}
			case Expr::Type::Unreachable:
				Write("UNREACHABLE;", Newline());
				return;

			case Expr::Type::AtomicWait:
			case Expr::Type::AtomicWake:
			case Expr::Type::AtomicLoad:
			case Expr::Type::AtomicRmw:
			case Expr::Type::AtomicRmwCmpxchg:
			case Expr::Type::AtomicStore:
			case Expr::Type::IfExcept:
			case Expr::Type::Rethrow:
			case Expr::Type::Throw:
			case Expr::Type::Try:
				ISO_ASSERT(0);
				break;
		}
	}
}

void CWriter::WriteSimpleUnaryExpr(Opcode opcode, const char* op) {
	Type result_type = opcode.GetResultType();
	Write(StackVar(0, result_type), " = ", op, "(", StackVar(0), ");", Newline());
	DropTypes(1);
	PushType(opcode.GetResultType());
}

void CWriter::WriteInfixBinaryExpr(Opcode opcode, const char* op, AssignOp assign_op) {
	Type result_type = opcode.GetResultType();
	Write(StackVar(1, result_type));
	if (assign_op == AssignOp::Allowed)
		Write(" ", op, "= ", StackVar(0));
	else
		Write(" = ", StackVar(1), " ", op, " ", StackVar(0));
	Write(";", Newline());
	DropTypes(2);
	PushType(result_type);
}

void CWriter::WritePrefixBinaryExpr(Opcode opcode, const char* op) {
	Type result_type = opcode.GetResultType();
	Write(StackVar(1, result_type), " = ", op, "(", StackVar(1), ", ", StackVar(0), ");", Newline());
	DropTypes(2);
	PushType(result_type);
}

void CWriter::WriteSignedBinaryExpr(Opcode opcode, const char* op) {
	Type result_type = opcode.GetResultType();
	Type type = opcode.GetParamType1();
	ISO_ASSERT(opcode.GetParamType2() == type);
	Write(StackVar(1, result_type), " = (", type, ")((", SignedType(type), ")", StackVar(1), " ", op, " (", SignedType(type), ")", StackVar(0), ");", Newline());
	DropTypes(2);
	PushType(result_type);
}

void CWriter::Write(const BinaryExpr& expr) {
	switch (expr.opcode) {
		case Opcode::I32Add:
		case Opcode::I64Add:
		case Opcode::F32Add:
		case Opcode::F64Add:
			WriteInfixBinaryExpr(expr.opcode, "+");
			break;

		case Opcode::I32Sub:
		case Opcode::I64Sub:
		case Opcode::F32Sub:
		case Opcode::F64Sub:
			WriteInfixBinaryExpr(expr.opcode, "-");
			break;

		case Opcode::I32Mul:
		case Opcode::I64Mul:
		case Opcode::F32Mul:
		case Opcode::F64Mul:
			WriteInfixBinaryExpr(expr.opcode, "*");
			break;

		case Opcode::I32DivS:
			WritePrefixBinaryExpr(expr.opcode, "I32_DIV_S");
			break;

		case Opcode::I64DivS:
			WritePrefixBinaryExpr(expr.opcode, "I64_DIV_S");
			break;

		case Opcode::I32DivU:
		case Opcode::I64DivU:
			WritePrefixBinaryExpr(expr.opcode, "DIV_U");
			break;

		case Opcode::F32Div:
		case Opcode::F64Div:
			WriteInfixBinaryExpr(expr.opcode, "/");
			break;

		case Opcode::I32RemS:
			WritePrefixBinaryExpr(expr.opcode, "I32_REM_S");
			break;

		case Opcode::I64RemS:
			WritePrefixBinaryExpr(expr.opcode, "I64_REM_S");
			break;

		case Opcode::I32RemU:
		case Opcode::I64RemU:
			WritePrefixBinaryExpr(expr.opcode, "REM_U");
			break;

		case Opcode::I32And:
		case Opcode::I64And:
			WriteInfixBinaryExpr(expr.opcode, "&");
			break;

		case Opcode::I32Or:
		case Opcode::I64Or:
			WriteInfixBinaryExpr(expr.opcode, "|");
			break;

		case Opcode::I32Xor:
		case Opcode::I64Xor:
			WriteInfixBinaryExpr(expr.opcode, "^");
			break;

		case Opcode::I32Shl:
		case Opcode::I64Shl:
			Write(StackVar(1), " <<= (", StackVar(0), " & ", GetShiftMask(expr.opcode.GetResultType()), ");", Newline());
			DropTypes(1);
			break;

		case Opcode::I32ShrS:
		case Opcode::I64ShrS: {
			Type type = expr.opcode.GetResultType();
			Write(StackVar(1), " = (", type, ")((", SignedType(type), ")", StackVar(1), " >> (", StackVar(0), " & ", GetShiftMask(type), "));", Newline());
			DropTypes(1);
			break;
		}
		case Opcode::I32ShrU:
		case Opcode::I64ShrU:
			Write(StackVar(1), " >>= (", StackVar(0), " & ", GetShiftMask(expr.opcode.GetResultType()), ");", Newline());
			DropTypes(1);
			break;

		case Opcode::I32Rotl:
			WritePrefixBinaryExpr(expr.opcode, "I32_ROTL");
			break;

		case Opcode::I64Rotl:
			WritePrefixBinaryExpr(expr.opcode, "I64_ROTL");
			break;

		case Opcode::I32Rotr:
			WritePrefixBinaryExpr(expr.opcode, "I32_ROTR");
			break;

		case Opcode::I64Rotr:
			WritePrefixBinaryExpr(expr.opcode, "I64_ROTR");
			break;

		case Opcode::F32Min:
		case Opcode::F64Min:
			WritePrefixBinaryExpr(expr.opcode, "FMIN");
			break;

		case Opcode::F32Max:
		case Opcode::F64Max:
			WritePrefixBinaryExpr(expr.opcode, "FMAX");
			break;

		case Opcode::F32Copysign:
			WritePrefixBinaryExpr(expr.opcode, "copysignf");
			break;

		case Opcode::F64Copysign:
			WritePrefixBinaryExpr(expr.opcode, "copysign");
			break;

		default:
			unreachable();
	}
}

void CWriter::Write(const CompareExpr& expr) {
	switch (expr.opcode) {
		case Opcode::I32Eq:
		case Opcode::I64Eq:
		case Opcode::F32Eq:
		case Opcode::F64Eq:
			WriteInfixBinaryExpr(expr.opcode, "==", AssignOp::Disallowed);
			break;

		case Opcode::I32Ne:
		case Opcode::I64Ne:
		case Opcode::F32Ne:
		case Opcode::F64Ne:
			WriteInfixBinaryExpr(expr.opcode, "!=", AssignOp::Disallowed);
			break;

		case Opcode::I32LtS:
		case Opcode::I64LtS:
			WriteSignedBinaryExpr(expr.opcode, "<");
			break;

		case Opcode::I32LtU:
		case Opcode::I64LtU:
		case Opcode::F32Lt:
		case Opcode::F64Lt:
			WriteInfixBinaryExpr(expr.opcode, "<", AssignOp::Disallowed);
			break;

		case Opcode::I32LeS:
		case Opcode::I64LeS:
			WriteSignedBinaryExpr(expr.opcode, "<=");
			break;

		case Opcode::I32LeU:
		case Opcode::I64LeU:
		case Opcode::F32Le:
		case Opcode::F64Le:
			WriteInfixBinaryExpr(expr.opcode, "<=", AssignOp::Disallowed);
			break;

		case Opcode::I32GtS:
		case Opcode::I64GtS:
			WriteSignedBinaryExpr(expr.opcode, ">");
			break;

		case Opcode::I32GtU:
		case Opcode::I64GtU:
		case Opcode::F32Gt:
		case Opcode::F64Gt:
			WriteInfixBinaryExpr(expr.opcode, ">", AssignOp::Disallowed);
			break;

		case Opcode::I32GeS:
		case Opcode::I64GeS:
			WriteSignedBinaryExpr(expr.opcode, ">=");
			break;

		case Opcode::I32GeU:
		case Opcode::I64GeU:
		case Opcode::F32Ge:
		case Opcode::F64Ge:
			WriteInfixBinaryExpr(expr.opcode, ">=", AssignOp::Disallowed);
			break;

		default:
			unreachable();
	}
}

void CWriter::Write(const ConvertExpr& expr) {
	switch (expr.opcode) {
		case Opcode::I32Eqz:
		case Opcode::I64Eqz:
			WriteSimpleUnaryExpr(expr.opcode, "!");
			break;

		case Opcode::I64ExtendSI32:
			WriteSimpleUnaryExpr(expr.opcode, "(u64)(s64)(s32)");
			break;

		case Opcode::I64ExtendUI32:
			WriteSimpleUnaryExpr(expr.opcode, "(u64)");
			break;

		case Opcode::I32WrapI64:
			WriteSimpleUnaryExpr(expr.opcode, "(u32)");
			break;

		case Opcode::I32TruncSF32:
			WriteSimpleUnaryExpr(expr.opcode, "I32_TRUNC_S_F32");
			break;

		case Opcode::I64TruncSF32:
			WriteSimpleUnaryExpr(expr.opcode, "I64_TRUNC_S_F32");
			break;

		case Opcode::I32TruncSF64:
			WriteSimpleUnaryExpr(expr.opcode, "I32_TRUNC_S_F64");
			break;

		case Opcode::I64TruncSF64:
			WriteSimpleUnaryExpr(expr.opcode, "I64_TRUNC_S_F64");
			break;

		case Opcode::I32TruncUF32:
			WriteSimpleUnaryExpr(expr.opcode, "I32_TRUNC_U_F32");
			break;

		case Opcode::I64TruncUF32:
			WriteSimpleUnaryExpr(expr.opcode, "I64_TRUNC_U_F32");
			break;

		case Opcode::I32TruncUF64:
			WriteSimpleUnaryExpr(expr.opcode, "I32_TRUNC_U_F64");
			break;

		case Opcode::I64TruncUF64:
			WriteSimpleUnaryExpr(expr.opcode, "I64_TRUNC_U_F64");
			break;

		case Opcode::I32TruncSSatF32:
		case Opcode::I64TruncSSatF32:
		case Opcode::I32TruncSSatF64:
		case Opcode::I64TruncSSatF64:
		case Opcode::I32TruncUSatF32:
		case Opcode::I64TruncUSatF32:
		case Opcode::I32TruncUSatF64:
		case Opcode::I64TruncUSatF64:
			//UNIMPLEMENTED(expr.opcode.GetName());
			break;

		case Opcode::F32ConvertSI32:
			WriteSimpleUnaryExpr(expr.opcode, "(f32)(s32)");
			break;

		case Opcode::F32ConvertSI64:
			WriteSimpleUnaryExpr(expr.opcode, "(f32)(s64)");
			break;

		case Opcode::F32ConvertUI32:
		case Opcode::F32DemoteF64:
			WriteSimpleUnaryExpr(expr.opcode, "(f32)");
			break;

		case Opcode::F32ConvertUI64:
			// TODO(binji): This needs to be handled specially (see
			// wabt_convert_uint64_to_float).
			WriteSimpleUnaryExpr(expr.opcode, "(f32)");
			break;

		case Opcode::F64ConvertSI32:
			WriteSimpleUnaryExpr(expr.opcode, "(f64)(s32)");
			break;

		case Opcode::F64ConvertSI64:
			WriteSimpleUnaryExpr(expr.opcode, "(f64)(s64)");
			break;

		case Opcode::F64ConvertUI32:
		case Opcode::F64PromoteF32:
			WriteSimpleUnaryExpr(expr.opcode, "(f64)");
			break;

		case Opcode::F64ConvertUI64:
			// TODO(binji): This needs to be handled specially (see
			// wabt_convert_uint64_to_double).
			WriteSimpleUnaryExpr(expr.opcode, "(f64)");
			break;

		case Opcode::F32ReinterpretI32:
			WriteSimpleUnaryExpr(expr.opcode, "f32_reinterpret_i32");
			break;

		case Opcode::I32ReinterpretF32:
			WriteSimpleUnaryExpr(expr.opcode, "i32_reinterpret_f32");
			break;

		case Opcode::F64ReinterpretI64:
			WriteSimpleUnaryExpr(expr.opcode, "f64_reinterpret_i64");
			break;

		case Opcode::I64ReinterpretF64:
			WriteSimpleUnaryExpr(expr.opcode, "i64_reinterpret_f64");
			break;

		default:
			unreachable();
	}
}

void CWriter::Write(const LoadExpr& expr) {
	const char* func = nullptr;
	switch (expr.opcode) {
		case Opcode::I32Load: func = "i32_load"; break;
		case Opcode::I64Load: func = "i64_load"; break;
		case Opcode::F32Load: func = "f32_load"; break;
		case Opcode::F64Load: func = "f64_load"; break;
		case Opcode::I32Load8S: func = "i32_load8_s"; break;
		case Opcode::I64Load8S: func = "i64_load8_s"; break;
		case Opcode::I32Load8U: func = "i32_load8_u"; break;
		case Opcode::I64Load8U: func = "i64_load8_u"; break;
		case Opcode::I32Load16S: func = "i32_load16_s"; break;
		case Opcode::I64Load16S: func = "i64_load16_s"; break;
		case Opcode::I32Load16U: func = "i32_load16_u"; break;
		case Opcode::I64Load16U: func = "i32_load16_u"; break;
		case Opcode::I64Load32S: func = "i64_load32_s"; break;
		case Opcode::I64Load32U: func = "i64_load32_u"; break;

		default:
			unreachable();
	}

	ISO_ASSERT(module_->memories.size() == 1);
	Memory* memory = module_->memories[0];

	Type result_type = expr.opcode.GetResultType();
	Write(StackVar(0, result_type), " = ", func, "(", ExternalPtr(memory->name), ", (u64)(", StackVar(0));
	if (expr.offset != 0)
		Write(" + ", expr.offset);
	Write("));", Newline());
	DropTypes(1);
	PushType(result_type);
}

void CWriter::Write(const StoreExpr& expr) {
	const char* func = nullptr;
	switch (expr.opcode) {
		case Opcode::I32Store: func = "i32_store"; break;
		case Opcode::I64Store: func = "i64_store"; break;
		case Opcode::F32Store: func = "f32_store"; break;
		case Opcode::F64Store: func = "f64_store"; break;
		case Opcode::I32Store8: func = "i32_store8"; break;
		case Opcode::I64Store8: func = "i64_store8"; break;
		case Opcode::I32Store16: func = "i32_store16"; break;
		case Opcode::I64Store16: func = "i64_store16"; break;
		case Opcode::I64Store32: func = "i64_store32"; break;

		default:
			unreachable();
	}

	ISO_ASSERT(module_->memories.size() == 1);
	Memory* memory = module_->memories[0];

	Write(func, "(", ExternalPtr(memory->name), ", (u64)(", StackVar(1));
	if (expr.offset != 0)
		Write(" + ", expr.offset);
	Write("), ", StackVar(0), ");", Newline());
	DropTypes(2);
}

void CWriter::Write(const UnaryExpr& expr) {
	switch (expr.opcode) {
		case Opcode::I32Clz:
			WriteSimpleUnaryExpr(expr.opcode, "I32_CLZ");
			break;

		case Opcode::I64Clz:
			WriteSimpleUnaryExpr(expr.opcode, "I64_CLZ");
			break;

		case Opcode::I32Ctz:
			WriteSimpleUnaryExpr(expr.opcode, "I32_CTZ");
			break;

		case Opcode::I64Ctz:
			WriteSimpleUnaryExpr(expr.opcode, "I64_CTZ");
			break;

		case Opcode::I32Popcnt:
			WriteSimpleUnaryExpr(expr.opcode, "I32_POPCNT");
			break;

		case Opcode::I64Popcnt:
			WriteSimpleUnaryExpr(expr.opcode, "I64_POPCNT");
			break;

		case Opcode::F32Neg:
		case Opcode::F64Neg:
			WriteSimpleUnaryExpr(expr.opcode, "-");
			break;

		case Opcode::F32Abs:
			WriteSimpleUnaryExpr(expr.opcode, "fabsf");
			break;

		case Opcode::F64Abs:
			WriteSimpleUnaryExpr(expr.opcode, "fabs");
			break;

		case Opcode::F32Sqrt:
			WriteSimpleUnaryExpr(expr.opcode, "sqrtf");
			break;

		case Opcode::F64Sqrt:
			WriteSimpleUnaryExpr(expr.opcode, "sqrt");
			break;

		case Opcode::F32Ceil:
			WriteSimpleUnaryExpr(expr.opcode, "ceilf");
			break;

		case Opcode::F64Ceil:
			WriteSimpleUnaryExpr(expr.opcode, "ceil");
			break;

		case Opcode::F32Floor:
			WriteSimpleUnaryExpr(expr.opcode, "floorf");
			break;

		case Opcode::F64Floor:
			WriteSimpleUnaryExpr(expr.opcode, "floor");
			break;

		case Opcode::F32Trunc:
			WriteSimpleUnaryExpr(expr.opcode, "truncf");
			break;

		case Opcode::F64Trunc:
			WriteSimpleUnaryExpr(expr.opcode, "trunc");
			break;

		case Opcode::F32Nearest:
			WriteSimpleUnaryExpr(expr.opcode, "nearbyintf");
			break;

		case Opcode::F64Nearest:
			WriteSimpleUnaryExpr(expr.opcode, "nearbyint");
			break;

		case Opcode::I32Extend8S:
		case Opcode::I32Extend16S:
		case Opcode::I64Extend8S:
		case Opcode::I64Extend16S:
		case Opcode::I64Extend32S:
			//UNIMPLEMENTED(expr.opcode.GetName());
			break;

		default:
			unreachable();
	}
}

void CWriter::Write(const TernaryExpr& expr) {
	switch (expr.opcode) {
		case Opcode::V128BitSelect: {
			Type result_type = expr.opcode.GetResultType();
			Write(StackVar(2, result_type), " = ", "v128.bitselect", "(", StackVar(0), ", ", StackVar(1), ", ", StackVar(2), ");", Newline());
			DropTypes(3);
			PushType(result_type);
			break;
		}
		default:
			unreachable();
	}
}

void CWriter::Write(const SimdLaneOpExpr& expr) {
	Type result_type = expr.opcode.GetResultType();

	switch (expr.opcode) {
		case Opcode::I8X16ExtractLaneS:
		case Opcode::I8X16ExtractLaneU:
		case Opcode::I16X8ExtractLaneS:
		case Opcode::I16X8ExtractLaneU:
		case Opcode::I32X4ExtractLane:
		case Opcode::I64X2ExtractLane:
		case Opcode::F32X4ExtractLane:
		case Opcode::F64X2ExtractLane: {
			Write(StackVar(0, result_type), " = ", expr.opcode.GetName(), "(", StackVar(0), ", lane Imm: ", expr.val, ");", Newline());
			DropTypes(1);
			break;
		}
		case Opcode::I8X16ReplaceLane:
		case Opcode::I16X8ReplaceLane:
		case Opcode::I32X4ReplaceLane:
		case Opcode::I64X2ReplaceLane:
		case Opcode::F32X4ReplaceLane:
		case Opcode::F64X2ReplaceLane: {
			Write(StackVar(1, result_type), " = ", expr.opcode.GetName(), "(", StackVar(0), ", ", StackVar(1), ", lane Imm: ", expr.val, ");", Newline());
			DropTypes(2);
			break;
		}
		default:
			unreachable();
	}

	PushType(result_type);
}

void CWriter::Write(const SimdShuffleOpExpr& expr) {
	Type result_type = expr.opcode.GetResultType();
	const uint32	*p = (const uint32*)&expr.val;
	Write(StackVar(1, result_type), " = ", expr.opcode.GetName(), "(",
		StackVar(1), " ", StackVar(0),
		format_string(", lane Imm: $0x%08x %08x %08x %08x", p[0], p[1], p[2], p[3]),
		")",
		Newline()
	);
	DropTypes(2);
	PushType(result_type);
}

void CWriter::WriteCHeader() {
	acc = &h_acc;
	string guard = GenerateHeaderGuard();
	Write("#ifndef ", guard, Newline());
	Write("#define ", guard, Newline());
	Write(s_header_top);
	WriteImports();
	WriteExports(WriteExportsKind::Declarations);
	Write(s_header_bottom);
	Write(Newline(), "#endif  /* ", guard, " */", Newline());
}

void CWriter::WriteCSource() {
	acc = &c_acc;
	WriteSourceTop();
	WriteFuncTypes();
	WriteFuncDeclarations();
	WriteGlobals();
	WriteMemories();
	WriteTables();
	WriteFuncs();
	WriteDataInitializers();
	WriteElemInitializers();
	WriteExports(WriteExportsKind::Definitions);
	WriteInitExports();
	WriteInit();
}

bool CWriter::WriteModule(const Module& module) {
	module_ = &module;
	InitGlobalSymbols();
	WriteCHeader();
	WriteCSource();
	return true;
}

void write_c(Module& module, string_accum &c_acc, string_accum &h_acc, const char* header_name) {
	NameGenerator().VisitModule(&module);
	NameApplier().VisitModule(&module);
	CWriter(c_acc, h_acc, header_name).WriteModule(module);
}