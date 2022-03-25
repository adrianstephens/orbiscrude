#include "base/defs.h"
#include "base/array.h"
#include "base/list.h"
#include "base/hash.h"
#include "base/tree.h"
#include "base/strings.h"
#include "base/algorithm.h"
#include "comms/leb128.h"
#include "stream.h"
#include "extra/text_stream.h"
#include "extra/identifier.h"

using namespace iso;

#define WABT_PAGE_SIZE					0x10000		// 64k
#define WABT_MAX_PAGES					0x10000		// # of pages that fit in 32-bit address space
#define WABT_BYTES_TO_PAGES(x)			((x) >> 16)
#define WABT_ALIGN_UP_TO_PAGE(x)		(((x) + WABT_PAGE_SIZE - 1) & ~(WABT_PAGE_SIZE - 1))

#define PRIzd			"I64d"
#define PRIzx			"I64x"
#define PRIstringview	"%.*s"
#define PRItypecode		"%s%#x"
#define PRIindex		"u"
#define PRIaddress		"u"
#define PRIoffset		PRIzx

#define CHECK_RESULT(expr)	do { if (!expr) return false; } while (0)

template<typename T, typename B> bool				isa(const B* base)				{ ISO_COMPILEASSERT((T_is_base_of<B, T>::value)); return T::classof(base);}
template<typename T, typename B> const T*			cast(const B* base)				{ ISO_ASSERT(isa<T>(base)); return static_cast<const T*>(base); }
template<typename T, typename B> T*					cast(B* base)					{ ISO_ASSERT(isa<T>(base)); return static_cast<T*>(base); }
template<typename T, typename B> const T*			dyn_cast(const B* base)			{ return isa<T>(base) ? static_cast<const T*>(base) : nullptr; }
template<typename T, typename B> T*					dyn_cast(B* base)				{ return isa<T>(base) ? static_cast<T*>(base) : nullptr; }
template<typename T, typename B> unique_ptr<const T>cast(unique_ptr<const B>&& base){ ISO_ASSERT(isa<T>(base.get())); return unique_ptr<T>(static_cast<const T*>(base.detach())); };
template<typename T, typename B> unique_ptr<T>		cast(unique_ptr<B>&& base)		{ ISO_ASSERT(isa<T>(base.get())); return unique_ptr<T>(static_cast<T*>(base.detach())); };

enum class BinarySection {
  Custom	= 0,
  Type		= 1,
  Import	= 2,
  Function	= 3,
  Table		= 4,
  Memory	= 5,
  Global	= 6,
  Export	= 7,
  Start		= 8,
  Elem		= 9,
  Code		= 10,
  Data		= 11,
  Invalid,
  size		= Invalid,
};
enum class NameSectionSubsection {
	Module		= 0,
	Function	= 1,
	Local		= 2,
};
template<> const char *field_names<BinarySection>::s[]	= {
	"Custom",	"Type",		"Import",	"Function",
	"Table",	"Memory",	"Global",	"Export",
	"Start",	"Elem",		"Code",		"Data",
};

struct Features {
	bool	exceptions			= false,
			mutable_globals		= false,
			sat_float_to_int	= false,
			sign_extension		= false,
			simd				= false,
			threads				= false,
			multi_value			= false;
};

enum class ErrorLevel {
	Warning,
	Error,
};

typedef uint32 Index;		// An index into one of the many index spaces
typedef uint32 Address;		// An address or size in linear memory
typedef size_t Offset;		// An offset into a host's file or memory buffer

static const Address	kInvalidAddress	= ~0;
static const Index		kInvalidIndex	= ~0;
static const Offset		kInvalidOffset	= ~0;

enum class LabelType {
	Func,
	Block,
	Loop,
	If,
	Else,
	IfExcept,
	IfExceptElse,
	Try,
	Catch,
};

struct Location {
	count_string filename;
	union {
		// For text files.
		struct {
			int line;
			int first_col;
			int last_col;
		};
		// For binary files.
		struct {
			size_t offset;
		};
	};

	Location()																: line(0), first_col(0), last_col(0) {}
	Location(count_string filename, int line, int first_col, int last_col)	: filename(filename), line(line), first_col(first_col), last_col(last_col) {}
	Location(count_string filename, size_t offset)							: filename(filename), offset(offset) {}
};

struct Reloc {
	enum Type {
		FuncIndexLEB		= 0,	// e.g. Immediate of call instruction
		TableIndexSLEB		= 1,	// e.g. Loading address of function
		TableIndexI32		= 2,	// e.g. Function address in DATA
		MemoryAddressLEB	= 3,	// e.g. Memory address in load/store offset immediate
		MemoryAddressSLEB	= 4,	// e.g. Memory address in i32.const
		MemoryAddressI32	= 5,	// e.g. Memory address in DATA
		TypeIndexLEB		= 6,	// e.g. Immediate type in call_indirect
		GlobalIndexLEB		= 7,	// e.g. Immediate of get_global inst
		FunctionOffsetI32	= 8,	// e.g. Code offset in DWARF metadata
		SectionOffsetI32	= 9,	// e.g. Section offset in DWARF metadata
	};
	Type	type;
	size_t	offset;
	Index	index;
	int32	addend;
	Reloc(Type type, size_t offset, Index index, int32 addend = 0) : type(type), offset(offset), index(index), addend(addend) {}
};

enum class LinkingEntryType {
	SegmentInfo		= 5,
	InitFunctions	= 6,
	ComdatInfo		= 7,
	SymbolTable		= 8,
};

struct Symbol {
	enum Flag {
		Undefined	= 0x10,
		Visibility	= 0x4,
		Binding		= 0x3,
	};

	enum class Type {
		Function	= 0,
		Data		= 1,
		Global		= 2,
		Section		= 3,
	};

	enum class Visibility {
		Default		= 0,
		Hidden		= 4,
	};

	enum class Binding {
		Global		= 0,
		Weak		= 1,
		Local		= 2,
	};
};

// matches binary format, do not change
enum class ExternalKind {
	Func	= 0,
	Table	= 1,
	Memory	= 2,
	Global	= 3,
	Except	= 4,
	size,
};

struct Limits {
	uint64	initial		= 0;
	uint64	max			= 0;
	bool	has_max		= false;
	bool	is_shared	= false;
};

enum { WABT_USE_NATURAL_ALIGNMENT = 0xFFFFFFFF };

template<> const char *field_names<ExternalKind>::s[]	= {"func", "table", "memory", "global", "except"};

template<> const char *field_names<Reloc::Type>::s[]	= {
    "R_WEBASSEMBLY_FUNCTION_INDEX_LEB",  "R_WEBASSEMBLY_TABLE_INDEX_SLEB",
    "R_WEBASSEMBLY_TABLE_INDEX_I32",     "R_WEBASSEMBLY_MEMORY_ADDR_LEB",
    "R_WEBASSEMBLY_MEMORY_ADDR_SLEB",    "R_WEBASSEMBLY_MEMORY_ADDR_I32",
    "R_WEBASSEMBLY_TYPE_INDEX_LEB",      "R_WEBASSEMBLY_GLOBAL_INDEX_LEB",
    "R_WEBASSEMBLY_FUNCTION_OFFSET_I32", "R_WEBASSEMBLY_SECTION_OFFSET_I32",
};

template<> const char *field_names<Symbol::Type>::s[]	= {	"func", "global", "data", "section" };

// type
enum class Type : int32 {
	I32			= -0x01,		// 0x7f
	I64			= -0x02,		// 0x7e
	F32			= -0x03,		// 0x7d
	F64			= -0x04,		// 0x7c
	V128		= -0x05,		// 0x7b
	Anyfunc		= -0x10,		// 0x70
	ExceptRef	= -0x18,		// 0x68
	Func		= -0x20,		// 0x60
	Void		= -0x40,		// 0x40
	___			= Void,			// Convenient for the opcode table in opcode.h
	Any			= 0,			// Not actually specified, but useful for type-checking
};
typedef dynamic_array<Type> TypeVector;

template<> struct field_names<Type>		{ static field_value s[]; };
field_value field_names<Type>::s[]	= {
	{"i32",			(int)Type::I32		},
	{"i64",			(int)Type::I64		},
	{"f32",			(int)Type::F32		},
	{"f64",			(int)Type::F64		},
	{"uint128",		(int)Type::V128		},
	{"anyfunc",		(int)Type::Anyfunc	},
	{"func",		(int)Type::Func		},
	{"except_ref",	(int)Type::ExceptRef},
	{"void",		(int)Type::Void		},
	{"any",			(int)Type::Any		},
};

static inline bool IsTypeIndex(Type type) {
	return static_cast<int32>(type) >= 0;
}

static inline Index GetTypeIndex(Type type) {
	ISO_ASSERT(IsTypeIndex(type));
	return static_cast<Index>(type);
}

static inline TypeVector GetInlineTypeVector(Type type) {
	ISO_ASSERT(!IsTypeIndex(type));
	switch (type) {
		case Type::Void:
			return TypeVector();

		case Type::I32:
		case Type::I64:
		case Type::F32:
		case Type::F64:
		case Type::V128:
			//return TypeVector(&type, &type + 1);

		default:
			unreachable();
	}
}

struct Var {
	enum class Type {
		Index,
		Name,
	};
	Type type;
	union {
		Index	i;
		string	n;
	};

	void Destroy() {
		if (is_name())
			destruct(n);
	}
public:
	Location loc;

	explicit Var(Index index = kInvalidIndex, const Location& loc = Location()) : type(Var::Type::Index), i(index), loc(loc) {}
	explicit Var(count_string name, const Location& loc = Location()) : type(Var::Type::Name), n(name), loc(loc) {}
	Var(Var &&rhs) : Var(kInvalidIndex) { *this = move(rhs); }
	Var(const Var &rhs) : Var(kInvalidIndex) { *this = rhs; }
	Var& operator=(const Var& rhs) {
		loc = rhs.loc;
		if (rhs.is_index())
			set_index(rhs.i);
		else
			set_name(string(rhs.n));
		return *this;
	}
	Var& operator=(Var&& rhs) {
		loc = rhs.loc;
		if (rhs.is_index())
			set_index(rhs.i);
		else
			set_name(move(rhs.n));
		return *this;
	}
	~Var() {
		Destroy();
	}
	bool			is_index()	const { return type == Type::Index; }
	bool			is_name()	const { return type == Type::Name; }
	Index			index()		const { ISO_ASSERT(is_index()); return i; }
	const string&	name()		const { ISO_ASSERT(is_name()); return n; }

	void set_index(Index index) {
		Destroy();
		type	= Type::Index;
		i		= index;
	}
	void set_name(string&& name) {
		Destroy();
		type = Type::Name;
		construct(n, move(name));
	}
	void set_name(count_string name) {
		set_name(string(name));
	}
};
typedef dynamic_array<Var> VarVector;

struct Const {
	Location	loc;
	Type		type;
	union {
		uint32	u32;
		uint64	u64;
		uint32	f32_bits;
		uint64	f64_bits;
		uint128 v128_bits;
	};

	Const(Type type = Type::I32, uint64 val = 0, const Location& loc = Location()) : type(type), u64(val), loc(loc) {}
	Const(uint128 val = uint128(0), const Location& loc = Location()) : type(Type::V128), v128_bits(val), loc(loc) {}

	static Const I32(uint32 val = 0, const Location& loc = Location())	{ return Const(Type::I32, val, loc); }
	static Const I64(uint64 val = 0, const Location& loc = Location())	{ return Const(Type::I64, val, loc); }
	static Const F32(uint32 val = 0, const Location& loc = Location())	{ return Const(Type::F32, val, loc); }
	static Const F64(uint64 val = 0, const Location& loc = Location())	{ return Const(Type::F64, val, loc); }
	static Const V128(uint128 val, const Location& loc = Location())	{ return Const(val, loc); }
};
typedef dynamic_array<Const> ConstVector;

struct FuncSignature {
	TypeVector param_types;
	TypeVector result_types;

	Index	GetNumParams()				const { return param_types.size32(); }
	Index	GetNumResults()				const { return result_types.size32(); }
	Type	GetParamType(Index index)	const { return param_types[index]; }
	Type	GetResultType(Index index)	const { return result_types[index]; }
	bool operator==(const FuncSignature &rhs) const { return param_types == rhs.param_types && result_types == rhs.result_types; }
};

struct FuncType {
	string			name;
	FuncSignature	sig;

	explicit FuncType(count_string name) : name(name) {}

	Index	GetNumParams()				const { return sig.GetNumParams(); }
	Index	GetNumResults()				const { return sig.GetNumResults(); }
	Type	GetParamType(Index index)	const { return sig.GetParamType(index); }
	Type	GetResultType(Index index)	const { return sig.GetResultType(index); }
};

struct FuncDeclaration {
	bool			has_func_type = false;
	Var				type_var;
	FuncSignature	sig;

	Index	GetNumParams()				const { return sig.GetNumParams(); }
	Index	GetNumResults()				const { return sig.GetNumResults(); }
	Type	GetParamType(Index index)	const { return sig.GetParamType(index); }
	Type	GetResultType(Index index)	const { return sig.GetResultType(index); }
};

struct Opcode {
	enum Enum {
		Unreachable,
		Nop,
		Block,
		Loop,
		If,
		Else,
		Try,
		Catch,
		Throw,
		Rethrow,
		IfExcept,
		End,
		Br,
		BrIf,
		BrTable,
		Return,
		Call,
		CallIndirect,
		Drop,
		Select,
		GetLocal,
		SetLocal,
		TeeLocal,
		GetGlobal,
		SetGlobal,
		I32Load,
		I64Load,
		F32Load,
		F64Load,
		I32Load8S,
		I32Load8U,
		I32Load16S,
		I32Load16U,
		I64Load8S,
		I64Load8U,
		I64Load16S,
		I64Load16U,
		I64Load32S,
		I64Load32U,
		I32Store,
		I64Store,
		F32Store,
		F64Store,
		I32Store8,
		I32Store16,
		I64Store8,
		I64Store16,
		I64Store32,
		MemorySize,
		MemoryGrow,
		I32Const,
		I64Const,
		F32Const,
		F64Const,
		I32Eqz,
		I32Eq,
		I32Ne,
		I32LtS,
		I32LtU,
		I32GtS,
		I32GtU,
		I32LeS,
		I32LeU,
		I32GeS,
		I32GeU,
		I64Eqz,
		I64Eq,
		I64Ne,
		I64LtS,
		I64LtU,
		I64GtS,
		I64GtU,
		I64LeS,
		I64LeU,
		I64GeS,
		I64GeU,
		F32Eq,
		F32Ne,
		F32Lt,
		F32Gt,
		F32Le,
		F32Ge,
		F64Eq,
		F64Ne,
		F64Lt,
		F64Gt,
		F64Le,
		F64Ge,
		I32Clz,
		I32Ctz,
		I32Popcnt,
		I32Add,
		I32Sub,
		I32Mul,
		I32DivS,
		I32DivU,
		I32RemS,
		I32RemU,
		I32And,
		I32Or,
		I32Xor,
		I32Shl,
		I32ShrS,
		I32ShrU,
		I32Rotl,
		I32Rotr,
		I64Clz,
		I64Ctz,
		I64Popcnt,
		I64Add,
		I64Sub,
		I64Mul,
		I64DivS,
		I64DivU,
		I64RemS,
		I64RemU,
		I64And,
		I64Or,
		I64Xor,
		I64Shl,
		I64ShrS,
		I64ShrU,
		I64Rotl,
		I64Rotr,
		F32Abs,
		F32Neg,
		F32Ceil,
		F32Floor,
		F32Trunc,
		F32Nearest,
		F32Sqrt,
		F32Add,
		F32Sub,
		F32Mul,
		F32Div,
		F32Min,
		F32Max,
		F32Copysign,
		F64Abs,
		F64Neg,
		F64Ceil,
		F64Floor,
		F64Trunc,
		F64Nearest,
		F64Sqrt,
		F64Add,
		F64Sub,
		F64Mul,
		F64Div,
		F64Min,
		F64Max,
		F64Copysign,
		I32WrapI64,
		I32TruncSF32,
		I32TruncUF32,
		I32TruncSF64,
		I32TruncUF64,
		I64ExtendSI32,
		I64ExtendUI32,
		I64TruncSF32,
		I64TruncUF32,
		I64TruncSF64,
		I64TruncUF64,
		F32ConvertSI32,
		F32ConvertUI32,
		F32ConvertSI64,
		F32ConvertUI64,
		F32DemoteF64,
		F64ConvertSI32,
		F64ConvertUI32,
		F64ConvertSI64,
		F64ConvertUI64,
		F64PromoteF32,
		I32ReinterpretF32,
		I64ReinterpretF64,
		F32ReinterpretI32,
		F64ReinterpretI64,

		I32Extend8S,
		I32Extend16S,
		I64Extend8S,
		I64Extend16S,
		I64Extend32S,

		InterpAlloca,
		InterpBrUnless,
		InterpCallHost,
		InterpData,
		InterpDropKeep,

		I32TruncSSatF32,
		I32TruncUSatF32,
		I32TruncSSatF64,
		I32TruncUSatF64,
		I64TruncSSatF32,
		I64TruncUSatF32,
		I64TruncSSatF64,
		I64TruncUSatF64,

		V128Const,
		V128Load,
		V128Store,
		I8X16Splat,
		I16X8Splat,
		I32X4Splat,
		I64X2Splat,
		F32X4Splat,
		F64X2Splat,
		I8X16ExtractLaneS,
		I8X16ExtractLaneU,
		I16X8ExtractLaneS,
		I16X8ExtractLaneU,
		I32X4ExtractLane,
		I64X2ExtractLane,
		F32X4ExtractLane,
		F64X2ExtractLane,
		I8X16ReplaceLane,
		I16X8ReplaceLane,
		I32X4ReplaceLane,
		I64X2ReplaceLane,
		F32X4ReplaceLane,
		F64X2ReplaceLane,
		V8X16Shuffle,
		I8X16Add,
		I16X8Add,
		I32X4Add,
		I64X2Add,
		I8X16Sub,
		I16X8Sub,
		I32X4Sub,
		I64X2Sub,
		I8X16Mul,
		I16X8Mul,
		I32X4Mul,
		I8X16Neg,
		I16X8Neg,
		I32X4Neg,
		I64X2Neg,
		I8X16AddSaturateS,
		I8X16AddSaturateU,
		I16X8AddSaturateS,
		I16X8AddSaturateU,
		I8X16SubSaturateS,
		I8X16SubSaturateU,
		I16X8SubSaturateS,
		I16X8SubSaturateU,
		I8X16Shl,
		I16X8Shl,
		I32X4Shl,
		I64X2Shl,
		I8X16ShrS,
		I8X16ShrU,
		I16X8ShrS,
		I16X8ShrU,
		I32X4ShrS,
		I32X4ShrU,
		I64X2ShrS,
		I64X2ShrU,
		V128And,
		V128Or,
		V128Xor,
		V128Not,
		V128BitSelect,
		I8X16AnyTrue,
		I16X8AnyTrue,
		I32X4AnyTrue,
		I64X2AnyTrue,
		I8X16AllTrue,
		I16X8AllTrue,
		I32X4AllTrue,
		I64X2AllTrue,
		I8X16Eq,
		I16X8Eq,
		I32X4Eq,
		F32X4Eq,
		F64X2Eq,
		I8X16Ne,
		I16X8Ne,
		I32X4Ne,
		F32X4Ne,
		F64X2Ne,
		I8X16LtS,
		I8X16LtU,
		I16X8LtS,
		I16X8LtU,
		I32X4LtS,
		I32X4LtU,
		F32X4Lt,
		F64X2Lt,
		I8X16LeS,
		I8X16LeU,
		I16X8LeS,
		I16X8LeU,
		I32X4LeS,
		I32X4LeU,
		F32X4Le,
		F64X2Le,
		I8X16GtS,
		I8X16GtU,
		I16X8GtS,
		I16X8GtU,
		I32X4GtS,
		I32X4GtU,
		F32X4Gt,
		F64X2Gt,
		I8X16GeS,
		I8X16GeU,
		I16X8GeS,
		I16X8GeU,
		I32X4GeS,
		I32X4GeU,
		F32X4Ge,
		F64X2Ge,
		F32X4Neg,
		F64X2Neg,
		F32X4Abs,
		F64X2Abs,
		F32X4Min,
		F64X2Min,
		F32X4Max,
		F64X2Max,
		F32X4Add,
		F64X2Add,
		F32X4Sub,
		F64X2Sub,
		F32X4Div,
		F64X2Div,
		F32X4Mul,
		F64X2Mul,
		F32X4Sqrt,
		F64X2Sqrt,
		F32X4ConvertSI32X4,
		F32X4ConvertUI32X4,
		F64X2ConvertSI64X2,
		F64X2ConvertUI64X2,
		I32X4TruncSF32X4Sat,
		I32X4TruncUF32X4Sat,
		I64X2TruncSF64X2Sat,
		I64X2TruncUF64X2Sat,

		AtomicWake,
		I32AtomicWait,
		I64AtomicWait,
		I32AtomicLoad,
		I64AtomicLoad,
		I32AtomicLoad8U,
		I32AtomicLoad16U,
		I64AtomicLoad8U,
		I64AtomicLoad16U,
		I64AtomicLoad32U,
		I32AtomicStore,
		I64AtomicStore,
		I32AtomicStore8,
		I32AtomicStore16,
		I64AtomicStore8,
		I64AtomicStore16,
		I64AtomicStore32,
		I32AtomicRmwAdd,
		I64AtomicRmwAdd,
		I32AtomicRmw8UAdd,
		I32AtomicRmw16UAdd,
		I64AtomicRmw8UAdd,
		I64AtomicRmw16UAdd,
		I64AtomicRmw32UAdd,
		I32AtomicRmwSub,
		I64AtomicRmwSub,
		I32AtomicRmw8USub,
		I32AtomicRmw16USub,
		I64AtomicRmw8USub,
		I64AtomicRmw16USub,
		I64AtomicRmw32USub,
		I32AtomicRmwAnd,
		I64AtomicRmwAnd,
		I32AtomicRmw8UAnd,
		I32AtomicRmw16UAnd,
		I64AtomicRmw8UAnd,
		I64AtomicRmw16UAnd,
		I64AtomicRmw32UAnd,
		I32AtomicRmwOr,
		I64AtomicRmwOr,
		I32AtomicRmw8UOr,
		I32AtomicRmw16UOr,
		I64AtomicRmw8UOr,
		I64AtomicRmw16UOr,
		I64AtomicRmw32UOr,
		I32AtomicRmwXor,
		I64AtomicRmwXor,
		I32AtomicRmw8UXor,
		I32AtomicRmw16UXor,
		I64AtomicRmw8UXor,
		I64AtomicRmw16UXor,
		I64AtomicRmw32UXor,
		I32AtomicRmwXchg,
		I64AtomicRmwXchg,
		I32AtomicRmw8UXchg,
		I32AtomicRmw16UXchg,
		I64AtomicRmw8UXchg,
		I64AtomicRmw16UXchg,
		I64AtomicRmw32UXchg,
		I32AtomicRmwCmpxchg,
		I64AtomicRmwCmpxchg,
		I32AtomicRmw8UCmpxchg,
		I32AtomicRmw16UCmpxchg,
		I64AtomicRmw8UCmpxchg,
		I64AtomicRmw16UCmpxchg,
		I64AtomicRmw32UCmpxchg,
		Invalid,
	};
private:
	struct Info {
		const char*	name;
		Type		result_type;
		Type		param1_type;
		Type		param2_type;
		Type		param3_type;
		Address		memory_size;
		uint16		prefix_code;
	};
	static Info infos[Invalid];

	Enum e;

	static const uint32 kMathPrefix		= 0xfc;
	static const uint32 kThreadsPrefix	= 0xfe;
	static const uint32 kSimdPrefix		= 0xfd;

	static Info GetInfo(Enum e) {
		if (e < Invalid)
			return infos[e];
		uint16	prefix_code = ~static_cast<uint16>(e) + 1;
		return {"<invalid>", Type::Void, Type::Void, Type::Void, Type::Void, 0, prefix_code };
	}

	Info GetInfo() const {
		return GetInfo(e);
	}
public:
	static Opcode	FromCode(uint8 prefix, uint32 code) {
		uint32 prefix_code = (prefix << 8) | code;
		auto iter = lower_boundc(infos, prefix_code, [](const Info& info, uint32 prefix_code) {
			return info.prefix_code < prefix_code;
		});
		if (iter == end(infos) || iter->prefix_code != prefix_code)
			return Opcode(Enum(~prefix_code + 1));

		return Opcode(static_cast<Enum>(iter - infos));
	}

	static Opcode	FromCode(uint32 code)		{ return FromCode(0, code); }
	static bool		IsPrefixByte(uint8 byte)	{ return byte == kMathPrefix || byte == kThreadsPrefix || byte == kSimdPrefix; }

	friend const char*	GetName(Enum e)			{ return GetInfo(e).name; }

	Opcode() {}
	Opcode(Enum e) : e(e) {}
	operator Enum() const { return e; }

	bool		IsInvalid()		const { return e >= 0x100; }
	uint8		GetPrefix()		const { return GetInfo().prefix_code >> 8; }
	uint32		GetCode()		const { return GetInfo().prefix_code & 0xff; }
	size_t		GetLength()		const { return GetPrefix() ? 2 : 1; }
	const char*	GetName()		const { return GetInfo().name; }
	Type		GetResultType()	const { return GetInfo().result_type; }
	Type		GetParamType1()	const { return GetInfo().param1_type; }
	Type		GetParamType2()	const { return GetInfo().param2_type; }
	Type		GetParamType3()	const { return GetInfo().param3_type; }
	Address		GetMemorySize()	const { return GetInfo().memory_size; }

	// Get the lane count of an extract/replace simd op.
	uint32 GetSimdLaneCount()	const {
		switch (e) {
			case I8X16ExtractLaneS:	case I8X16ExtractLaneU:	case I8X16ReplaceLane:
				return 16;
			case I16X8ExtractLaneS:	case I16X8ExtractLaneU:	case I16X8ReplaceLane:
				return 8;
			case F32X4ExtractLane:	case F32X4ReplaceLane:	case I32X4ExtractLane:	case I32X4ReplaceLane:
				return 4;
			case F64X2ExtractLane:	case F64X2ReplaceLane:	case I64X2ExtractLane:	case I64X2ReplaceLane:
				return 2;
			default:
				unreachable();
		}
	}

	// Return 1 if |alignment| matches the alignment of |opcode|, or if |alignment| is WABT_USE_NATURAL_ALIGNMENT
	bool IsNaturallyAligned(Address alignment) const {
		return alignment == WABT_USE_NATURAL_ALIGNMENT || alignment == GetMemorySize();
	}
	// If |alignment| is WABT_USE_NATURAL_ALIGNMENT, return the alignment of |opcode|, else return |alignment|
	Address GetAlignment(Address alignment) const {
		return alignment == WABT_USE_NATURAL_ALIGNMENT ? GetMemorySize() : alignment;
	}
	bool IsEnabled(const Features& features) const {
		switch (e) {
			case Try:			case Catch:			case Throw:			case Rethrow:			case IfExcept:
				return features.exceptions;

			case I32TruncSSatF32:		case I32TruncUSatF32:		case I32TruncSSatF64:		case I32TruncUSatF64:
			case I64TruncSSatF32:		case I64TruncUSatF32:		case I64TruncSSatF64:		case I64TruncUSatF64:
				return features.sat_float_to_int;

			case I32Extend8S:			case I32Extend16S:			case I64Extend8S:			case I64Extend16S:			case I64Extend32S:
				return features.sign_extension;

			case AtomicWake:
			case I32AtomicWait:			case I64AtomicWait:			case I32AtomicLoad:			case I64AtomicLoad:
			case I32AtomicLoad8U:		case I32AtomicLoad16U:		case I64AtomicLoad8U:		case I64AtomicLoad16U:			case I64AtomicLoad32U:
			case I32AtomicStore:		case I64AtomicStore:		case I32AtomicStore8:		case I32AtomicStore16:			case I64AtomicStore8:		case I64AtomicStore16:			case I64AtomicStore32:
			case I32AtomicRmwAdd:		case I64AtomicRmwAdd:		case I32AtomicRmw8UAdd:		case I32AtomicRmw16UAdd:		case I64AtomicRmw8UAdd:		case I64AtomicRmw16UAdd:		case I64AtomicRmw32UAdd:
			case I32AtomicRmwSub:		case I64AtomicRmwSub:		case I32AtomicRmw8USub:		case I32AtomicRmw16USub:		case I64AtomicRmw8USub:		case I64AtomicRmw16USub:		case I64AtomicRmw32USub:
			case I32AtomicRmwAnd:		case I64AtomicRmwAnd:		case I32AtomicRmw8UAnd:		case I32AtomicRmw16UAnd:		case I64AtomicRmw8UAnd:		case I64AtomicRmw16UAnd:		case I64AtomicRmw32UAnd:
			case I32AtomicRmwOr:		case I64AtomicRmwOr:		case I32AtomicRmw8UOr:		case I32AtomicRmw16UOr:			case I64AtomicRmw8UOr:		case I64AtomicRmw16UOr:			case I64AtomicRmw32UOr:
			case I32AtomicRmwXor:		case I64AtomicRmwXor:		case I32AtomicRmw8UXor:		case I32AtomicRmw16UXor:		case I64AtomicRmw8UXor:		case I64AtomicRmw16UXor:		case I64AtomicRmw32UXor:
			case I32AtomicRmwXchg:		case I64AtomicRmwXchg:		case I32AtomicRmw8UXchg:	case I32AtomicRmw16UXchg:		case I64AtomicRmw8UXchg:	case I64AtomicRmw16UXchg:		case I64AtomicRmw32UXchg:
			case I32AtomicRmwCmpxchg:	case I64AtomicRmwCmpxchg:	case I32AtomicRmw8UCmpxchg:	case I32AtomicRmw16UCmpxchg:	case I64AtomicRmw8UCmpxchg:	case I64AtomicRmw16UCmpxchg:	case I64AtomicRmw32UCmpxchg:
				return features.threads;

			case V128Const:				case V128Load:				case V128Store:
			case I8X16Splat:			case I16X8Splat:			case I32X4Splat:			case I64X2Splat:				case F32X4Splat:			case F64X2Splat:
			case I8X16ExtractLaneS:		case I8X16ExtractLaneU:		case I16X8ExtractLaneS:		case I16X8ExtractLaneU:			case I32X4ExtractLane:		case I64X2ExtractLane:			case F32X4ExtractLane:		case F64X2ExtractLane:
			case I8X16ReplaceLane:		case I16X8ReplaceLane:		case I32X4ReplaceLane:		case I64X2ReplaceLane:			case F32X4ReplaceLane:		case F64X2ReplaceLane:
			case V8X16Shuffle:
			case I8X16Add:				case I16X8Add:				case I32X4Add:				case I64X2Add:					case I8X16Sub:
			case I16X8Sub:				case I32X4Sub:				case I64X2Sub:
			case I8X16Mul:				case I16X8Mul:				case I32X4Mul:
			case I8X16Neg:				case I16X8Neg:				case I32X4Neg:				case I64X2Neg:
			case I8X16AddSaturateS:		case I8X16AddSaturateU:		case I16X8AddSaturateS:		case I16X8AddSaturateU:			case I8X16SubSaturateS:		case I8X16SubSaturateU:			case I16X8SubSaturateS:		case I16X8SubSaturateU:
			case I8X16Shl:				case I16X8Shl:				case I32X4Shl:				case I64X2Shl:
			case I8X16ShrS:				case I8X16ShrU:				case I16X8ShrS:				case I16X8ShrU:					case I32X4ShrS:				case I32X4ShrU:					case I64X2ShrS:				case I64X2ShrU:
			case V128And:				case V128Or:				case V128Xor:				case V128Not:					case V128BitSelect:
			case I8X16AnyTrue:			case I16X8AnyTrue:			case I32X4AnyTrue:			case I64X2AnyTrue:				case I8X16AllTrue:			case I16X8AllTrue:				case I32X4AllTrue:			case I64X2AllTrue:
			case I8X16Eq:				case I16X8Eq:				case I32X4Eq:				case F32X4Eq:					case F64X2Eq:
			case I8X16Ne:				case I16X8Ne:				case I32X4Ne:				case F32X4Ne:					case F64X2Ne:
			case I8X16LtS:				case I8X16LtU:				case I16X8LtS:				case I16X8LtU:					case I32X4LtS:				case I32X4LtU:					case F32X4Lt:				case F64X2Lt:
			case I8X16LeS:				case I8X16LeU:				case I16X8LeS:				case I16X8LeU:					case I32X4LeS:				case I32X4LeU:					case F32X4Le:				case F64X2Le:
			case I8X16GtS:				case I8X16GtU:				case I16X8GtS:				case I16X8GtU:					case I32X4GtS:				case I32X4GtU:					case F32X4Gt:				case F64X2Gt:
			case I8X16GeS:				case I8X16GeU:				case I16X8GeS:				case I16X8GeU:					case I32X4GeS:				case I32X4GeU:					case F32X4Ge:				case F64X2Ge:
			case F32X4Neg:				case F64X2Neg:
			case F32X4Abs:				case F64X2Abs:
			case F32X4Min:				case F64X2Min:
			case F32X4Max:				case F64X2Max:
			case F32X4Add:				case F64X2Add:
			case F32X4Sub:				case F64X2Sub:
			case F32X4Div:				case F64X2Div:
			case F32X4Mul:				case F64X2Mul:
			case F32X4Sqrt:				case F64X2Sqrt:
			case F32X4ConvertSI32X4:	case F32X4ConvertUI32X4:			case F64X2ConvertSI64X2:			case F64X2ConvertUI64X2:
			case I32X4TruncSF32X4Sat:	case I32X4TruncUF32X4Sat:			case I64X2TruncSF64X2Sat:			case I64X2TruncUF64X2Sat:
				return features.simd;

				// Interpreter opcodes are never "enabled".
			case InterpAlloca:
			case InterpBrUnless:
			case InterpCallHost:
			case InterpData:
			case InterpDropKeep:
				return false;

			default:
				return true;
		}
	}
};

Opcode::Info Opcode::infos[] = {
#define WABT_OPCODE(rtype, type1, type2, type3, mem_size, prefix_code, Name, text)  {text, Type::rtype, Type::type1, Type::type2, Type::type3, mem_size, prefix_code},

/* *** NOTE *** This list must be kept sorted so it can be binary searched */

/*
 *     tr: result type
 *     t1: type of the 1st parameter
 *     t2: type of the 2nd parameter
 *     t3: type of the 3rd parameter
 *      m: memory size of the operation, if any
 *   code: opcode
 *   Name: used to generate the opcode enum
 *   text: a string of the opcode name in the text format
 *
 *          tr    t1    t2    t3    m  prefix_code  Name text
 * ==========================================================  */

WABT_OPCODE(___,  ___,  ___,  ___,  0,  0x0000, Unreachable, "unreachable")
WABT_OPCODE(___,  ___,  ___,  ___,  0,  0x0001, Nop, "nop")
WABT_OPCODE(___,  ___,  ___,  ___,  0,  0x0002, Block, "block")
WABT_OPCODE(___,  ___,  ___,  ___,  0,  0x0003, Loop, "loop")
WABT_OPCODE(___,  ___,  ___,  ___,  0,  0x0004, If, "if")
WABT_OPCODE(___,  ___,  ___,  ___,  0,  0x0005, Else, "else")
WABT_OPCODE(___,  ___,  ___,  ___,  0,  0x0006, Try, "try")
WABT_OPCODE(___,  ___,  ___,  ___,  0,  0x0007, Catch, "catch")
WABT_OPCODE(___,  ___,  ___,  ___,  0,  0x0008, Throw, "throw")
WABT_OPCODE(___,  ___,  ___,  ___,  0,  0x0009, Rethrow, "rethrow")
WABT_OPCODE(___,  ___,  ___,  ___,  0,  0x000a, IfExcept, "if_except")
WABT_OPCODE(___,  ___,  ___,  ___,  0,  0x000b, End, "end")
WABT_OPCODE(___,  ___,  ___,  ___,  0,  0x000c, Br, "br")
WABT_OPCODE(___,  ___,  ___,  ___,  0,  0x000d, BrIf, "br_if")
WABT_OPCODE(___,  ___,  ___,  ___,  0,  0x000e, BrTable, "br_table")
WABT_OPCODE(___,  ___,  ___,  ___,  0,  0x000f, Return, "return")
WABT_OPCODE(___,  ___,  ___,  ___,  0,  0x0010, Call, "call")
WABT_OPCODE(___,  ___,  ___,  ___,  0,  0x0011, CallIndirect, "call_indirect")
WABT_OPCODE(___,  ___,  ___,  ___,  0,  0x001a, Drop, "drop")
WABT_OPCODE(___,  ___,  ___,  ___,  0,  0x001b, Select, "select")
WABT_OPCODE(___,  ___,  ___,  ___,  0,  0x0020, GetLocal, "get_local")
WABT_OPCODE(___,  ___,  ___,  ___,  0,  0x0021, SetLocal, "set_local")
WABT_OPCODE(___,  ___,  ___,  ___,  0,  0x0022, TeeLocal, "tee_local")
WABT_OPCODE(___,  ___,  ___,  ___,  0,  0x0023, GetGlobal, "get_global")
WABT_OPCODE(___,  ___,  ___,  ___,  0,  0x0024, SetGlobal, "set_global")
WABT_OPCODE(I32,  I32,  ___,  ___,  4,  0x0028, I32Load, "i32.load")
WABT_OPCODE(I64,  I32,  ___,  ___,  8,  0x0029, I64Load, "i64.load")
WABT_OPCODE(F32,  I32,  ___,  ___,  4,  0x002a, F32Load, "f32.load")
WABT_OPCODE(F64,  I32,  ___,  ___,  8,  0x002b, F64Load, "f64.load")
WABT_OPCODE(I32,  I32,  ___,  ___,  1,  0x002c, I32Load8S, "i32.load8_s")
WABT_OPCODE(I32,  I32,  ___,  ___,  1,  0x002d, I32Load8U, "i32.load8_u")
WABT_OPCODE(I32,  I32,  ___,  ___,  2,  0x002e, I32Load16S, "i32.load16_s")
WABT_OPCODE(I32,  I32,  ___,  ___,  2,  0x002f, I32Load16U, "i32.load16_u")
WABT_OPCODE(I64,  I32,  ___,  ___,  1,  0x0030, I64Load8S, "i64.load8_s")
WABT_OPCODE(I64,  I32,  ___,  ___,  1,  0x0031, I64Load8U, "i64.load8_u")
WABT_OPCODE(I64,  I32,  ___,  ___,  2,  0x0032, I64Load16S, "i64.load16_s")
WABT_OPCODE(I64,  I32,  ___,  ___,  2,  0x0033, I64Load16U, "i64.load16_u")
WABT_OPCODE(I64,  I32,  ___,  ___,  4,  0x0034, I64Load32S, "i64.load32_s")
WABT_OPCODE(I64,  I32,  ___,  ___,  4,  0x0035, I64Load32U, "i64.load32_u")
WABT_OPCODE(___,  I32,  I32,  ___,  4,  0x0036, I32Store, "i32.store")
WABT_OPCODE(___,  I32,  I64,  ___,  8,  0x0037, I64Store, "i64.store")
WABT_OPCODE(___,  I32,  F32,  ___,  4,  0x0038, F32Store, "f32.store")
WABT_OPCODE(___,  I32,  F64,  ___,  8,  0x0039, F64Store, "f64.store")
WABT_OPCODE(___,  I32,  I32,  ___,  1,  0x003a, I32Store8, "i32.store8")
WABT_OPCODE(___,  I32,  I32,  ___,  2,  0x003b, I32Store16, "i32.store16")
WABT_OPCODE(___,  I32,  I64,  ___,  1,  0x003c, I64Store8, "i64.store8")
WABT_OPCODE(___,  I32,  I64,  ___,  2,  0x003d, I64Store16, "i64.store16")
WABT_OPCODE(___,  I32,  I64,  ___,  4,  0x003e, I64Store32, "i64.store32")
WABT_OPCODE(I32,  ___,  ___,  ___,  0,  0x003f, MemorySize, "memory.size")
WABT_OPCODE(I32,  I32,  ___,  ___,  0,  0x0040, MemoryGrow, "memory.grow")
WABT_OPCODE(I32,  ___,  ___,  ___,  0,  0x0041, I32Const, "i32.const")
WABT_OPCODE(I64,  ___,  ___,  ___,  0,  0x0042, I64Const, "i64.const")
WABT_OPCODE(F32,  ___,  ___,  ___,  0,  0x0043, F32Const, "f32.const")
WABT_OPCODE(F64,  ___,  ___,  ___,  0,  0x0044, F64Const, "f64.const")
WABT_OPCODE(I32,  I32,  ___,  ___,  0,  0x0045, I32Eqz, "i32.eqz")
WABT_OPCODE(I32,  I32,  I32,  ___,  0,  0x0046, I32Eq, "i32.eq")
WABT_OPCODE(I32,  I32,  I32,  ___,  0,  0x0047, I32Ne, "i32.ne")
WABT_OPCODE(I32,  I32,  I32,  ___,  0,  0x0048, I32LtS, "i32.lt_s")
WABT_OPCODE(I32,  I32,  I32,  ___,  0,  0x0049, I32LtU, "i32.lt_u")
WABT_OPCODE(I32,  I32,  I32,  ___,  0,  0x004a, I32GtS, "i32.gt_s")
WABT_OPCODE(I32,  I32,  I32,  ___,  0,  0x004b, I32GtU, "i32.gt_u")
WABT_OPCODE(I32,  I32,  I32,  ___,  0,  0x004c, I32LeS, "i32.le_s")
WABT_OPCODE(I32,  I32,  I32,  ___,  0,  0x004d, I32LeU, "i32.le_u")
WABT_OPCODE(I32,  I32,  I32,  ___,  0,  0x004e, I32GeS, "i32.ge_s")
WABT_OPCODE(I32,  I32,  I32,  ___,  0,  0x004f, I32GeU, "i32.ge_u")
WABT_OPCODE(I32,  I64,  ___,  ___,  0,  0x0050, I64Eqz, "i64.eqz")
WABT_OPCODE(I32,  I64,  I64,  ___,  0,  0x0051, I64Eq, "i64.eq")
WABT_OPCODE(I32,  I64,  I64,  ___,  0,  0x0052, I64Ne, "i64.ne")
WABT_OPCODE(I32,  I64,  I64,  ___,  0,  0x0053, I64LtS, "i64.lt_s")
WABT_OPCODE(I32,  I64,  I64,  ___,  0,  0x0054, I64LtU, "i64.lt_u")
WABT_OPCODE(I32,  I64,  I64,  ___,  0,  0x0055, I64GtS, "i64.gt_s")
WABT_OPCODE(I32,  I64,  I64,  ___,  0,  0x0056, I64GtU, "i64.gt_u")
WABT_OPCODE(I32,  I64,  I64,  ___,  0,  0x0057, I64LeS, "i64.le_s")
WABT_OPCODE(I32,  I64,  I64,  ___,  0,  0x0058, I64LeU, "i64.le_u")
WABT_OPCODE(I32,  I64,  I64,  ___,  0,  0x0059, I64GeS, "i64.ge_s")
WABT_OPCODE(I32,  I64,  I64,  ___,  0,  0x005a, I64GeU, "i64.ge_u")
WABT_OPCODE(I32,  F32,  F32,  ___,  0,  0x005b, F32Eq, "f32.eq")
WABT_OPCODE(I32,  F32,  F32,  ___,  0,  0x005c, F32Ne, "f32.ne")
WABT_OPCODE(I32,  F32,  F32,  ___,  0,  0x005d, F32Lt, "f32.lt")
WABT_OPCODE(I32,  F32,  F32,  ___,  0,  0x005e, F32Gt, "f32.gt")
WABT_OPCODE(I32,  F32,  F32,  ___,  0,  0x005f, F32Le, "f32.le")
WABT_OPCODE(I32,  F32,  F32,  ___,  0,  0x0060, F32Ge, "f32.ge")
WABT_OPCODE(I32,  F64,  F64,  ___,  0,  0x0061, F64Eq, "f64.eq")
WABT_OPCODE(I32,  F64,  F64,  ___,  0,  0x0062, F64Ne, "f64.ne")
WABT_OPCODE(I32,  F64,  F64,  ___,  0,  0x0063, F64Lt, "f64.lt")
WABT_OPCODE(I32,  F64,  F64,  ___,  0,  0x0064, F64Gt, "f64.gt")
WABT_OPCODE(I32,  F64,  F64,  ___,  0,  0x0065, F64Le, "f64.le")
WABT_OPCODE(I32,  F64,  F64,  ___,  0,  0x0066, F64Ge, "f64.ge")
WABT_OPCODE(I32,  I32,  ___,  ___,  0,  0x0067, I32Clz, "i32.clz")
WABT_OPCODE(I32,  I32,  ___,  ___,  0,  0x0068, I32Ctz, "i32.ctz")
WABT_OPCODE(I32,  I32,  ___,  ___,  0,  0x0069, I32Popcnt, "i32.popcnt")
WABT_OPCODE(I32,  I32,  I32,  ___,  0,  0x006a, I32Add, "i32.add")
WABT_OPCODE(I32,  I32,  I32,  ___,  0,  0x006b, I32Sub, "i32.sub")
WABT_OPCODE(I32,  I32,  I32,  ___,  0,  0x006c, I32Mul, "i32.mul")
WABT_OPCODE(I32,  I32,  I32,  ___,  0,  0x006d, I32DivS, "i32.div_s")
WABT_OPCODE(I32,  I32,  I32,  ___,  0,  0x006e, I32DivU, "i32.div_u")
WABT_OPCODE(I32,  I32,  I32,  ___,  0,  0x006f, I32RemS, "i32.rem_s")
WABT_OPCODE(I32,  I32,  I32,  ___,  0,  0x0070, I32RemU, "i32.rem_u")
WABT_OPCODE(I32,  I32,  I32,  ___,  0,  0x0071, I32And, "i32.and")
WABT_OPCODE(I32,  I32,  I32,  ___,  0,  0x0072, I32Or, "i32.or")
WABT_OPCODE(I32,  I32,  I32,  ___,  0,  0x0073, I32Xor, "i32.xor")
WABT_OPCODE(I32,  I32,  I32,  ___,  0,  0x0074, I32Shl, "i32.shl")
WABT_OPCODE(I32,  I32,  I32,  ___,  0,  0x0075, I32ShrS, "i32.shr_s")
WABT_OPCODE(I32,  I32,  I32,  ___,  0,  0x0076, I32ShrU, "i32.shr_u")
WABT_OPCODE(I32,  I32,  I32,  ___,  0,  0x0077, I32Rotl, "i32.rotl")
WABT_OPCODE(I32,  I32,  I32,  ___,  0,  0x0078, I32Rotr, "i32.rotr")
WABT_OPCODE(I64,  I64,  ___,  ___,  0,  0x0079, I64Clz, "i64.clz")
WABT_OPCODE(I64,  I64,  ___,  ___,  0,  0x007a, I64Ctz, "i64.ctz")
WABT_OPCODE(I64,  I64,  ___,  ___,  0,  0x007b, I64Popcnt, "i64.popcnt")
WABT_OPCODE(I64,  I64,  I64,  ___,  0,  0x007c, I64Add, "i64.add")
WABT_OPCODE(I64,  I64,  I64,  ___,  0,  0x007d, I64Sub, "i64.sub")
WABT_OPCODE(I64,  I64,  I64,  ___,  0,  0x007e, I64Mul, "i64.mul")
WABT_OPCODE(I64,  I64,  I64,  ___,  0,  0x007f, I64DivS, "i64.div_s")
WABT_OPCODE(I64,  I64,  I64,  ___,  0,  0x0080, I64DivU, "i64.div_u")
WABT_OPCODE(I64,  I64,  I64,  ___,  0,  0x0081, I64RemS, "i64.rem_s")
WABT_OPCODE(I64,  I64,  I64,  ___,  0,  0x0082, I64RemU, "i64.rem_u")
WABT_OPCODE(I64,  I64,  I64,  ___,  0,  0x0083, I64And, "i64.and")
WABT_OPCODE(I64,  I64,  I64,  ___,  0,  0x0084, I64Or, "i64.or")
WABT_OPCODE(I64,  I64,  I64,  ___,  0,  0x0085, I64Xor, "i64.xor")
WABT_OPCODE(I64,  I64,  I64,  ___,  0,  0x0086, I64Shl, "i64.shl")
WABT_OPCODE(I64,  I64,  I64,  ___,  0,  0x0087, I64ShrS, "i64.shr_s")
WABT_OPCODE(I64,  I64,  I64,  ___,  0,  0x0088, I64ShrU, "i64.shr_u")
WABT_OPCODE(I64,  I64,  I64,  ___,  0,  0x0089, I64Rotl, "i64.rotl")
WABT_OPCODE(I64,  I64,  I64,  ___,  0,  0x008a, I64Rotr, "i64.rotr")
WABT_OPCODE(F32,  F32,  F32,  ___,  0,  0x008b, F32Abs, "f32.abs")
WABT_OPCODE(F32,  F32,  F32,  ___,  0,  0x008c, F32Neg, "f32.neg")
WABT_OPCODE(F32,  F32,  F32,  ___,  0,  0x008d, F32Ceil, "f32.ceil")
WABT_OPCODE(F32,  F32,  F32,  ___,  0,  0x008e, F32Floor, "f32.floor")
WABT_OPCODE(F32,  F32,  F32,  ___,  0,  0x008f, F32Trunc, "f32.trunc")
WABT_OPCODE(F32,  F32,  F32,  ___,  0,  0x0090, F32Nearest, "f32.nearest")
WABT_OPCODE(F32,  F32,  F32,  ___,  0,  0x0091, F32Sqrt, "f32.sqrt")
WABT_OPCODE(F32,  F32,  F32,  ___,  0,  0x0092, F32Add, "f32.add")
WABT_OPCODE(F32,  F32,  F32,  ___,  0,  0x0093, F32Sub, "f32.sub")
WABT_OPCODE(F32,  F32,  F32,  ___,  0,  0x0094, F32Mul, "f32.mul")
WABT_OPCODE(F32,  F32,  F32,  ___,  0,  0x0095, F32Div, "f32.div")
WABT_OPCODE(F32,  F32,  F32,  ___,  0,  0x0096, F32Min, "f32.min")
WABT_OPCODE(F32,  F32,  F32,  ___,  0,  0x0097, F32Max, "f32.max")
WABT_OPCODE(F32,  F32,  F32,  ___,  0,  0x0098, F32Copysign, "f32.copysign")
WABT_OPCODE(F64,  F64,  F64,  ___,  0,  0x0099, F64Abs, "f64.abs")
WABT_OPCODE(F64,  F64,  F64,  ___,  0,  0x009a, F64Neg, "f64.neg")
WABT_OPCODE(F64,  F64,  F64,  ___,  0,  0x009b, F64Ceil, "f64.ceil")
WABT_OPCODE(F64,  F64,  F64,  ___,  0,  0x009c, F64Floor, "f64.floor")
WABT_OPCODE(F64,  F64,  F64,  ___,  0,  0x009d, F64Trunc, "f64.trunc")
WABT_OPCODE(F64,  F64,  F64,  ___,  0,  0x009e, F64Nearest, "f64.nearest")
WABT_OPCODE(F64,  F64,  F64,  ___,  0,  0x009f, F64Sqrt, "f64.sqrt")
WABT_OPCODE(F64,  F64,  F64,  ___,  0,  0x00a0, F64Add, "f64.add")
WABT_OPCODE(F64,  F64,  F64,  ___,  0,  0x00a1, F64Sub, "f64.sub")
WABT_OPCODE(F64,  F64,  F64,  ___,  0,  0x00a2, F64Mul, "f64.mul")
WABT_OPCODE(F64,  F64,  F64,  ___,  0,  0x00a3, F64Div, "f64.div")
WABT_OPCODE(F64,  F64,  F64,  ___,  0,  0x00a4, F64Min, "f64.min")
WABT_OPCODE(F64,  F64,  F64,  ___,  0,  0x00a5, F64Max, "f64.max")
WABT_OPCODE(F64,  F64,  F64,  ___,  0,  0x00a6, F64Copysign, "f64.copysign")
WABT_OPCODE(I32,  I64,  ___,  ___,  0,  0x00a7, I32WrapI64, "i32.wrap/i64")
WABT_OPCODE(I32,  F32,  ___,  ___,  0,  0x00a8, I32TruncSF32, "i32.trunc_s/f32")
WABT_OPCODE(I32,  F32,  ___,  ___,  0,  0x00a9, I32TruncUF32, "i32.trunc_u/f32")
WABT_OPCODE(I32,  F64,  ___,  ___,  0,  0x00aa, I32TruncSF64, "i32.trunc_s/f64")
WABT_OPCODE(I32,  F64,  ___,  ___,  0,  0x00ab, I32TruncUF64, "i32.trunc_u/f64")
WABT_OPCODE(I64,  I32,  ___,  ___,  0,  0x00ac, I64ExtendSI32, "i64.extend_s/i32")
WABT_OPCODE(I64,  I32,  ___,  ___,  0,  0x00ad, I64ExtendUI32, "i64.extend_u/i32")
WABT_OPCODE(I64,  F32,  ___,  ___,  0,  0x00ae, I64TruncSF32, "i64.trunc_s/f32")
WABT_OPCODE(I64,  F32,  ___,  ___,  0,  0x00af, I64TruncUF32, "i64.trunc_u/f32")
WABT_OPCODE(I64,  F64,  ___,  ___,  0,  0x00b0, I64TruncSF64, "i64.trunc_s/f64")
WABT_OPCODE(I64,  F64,  ___,  ___,  0,  0x00b1, I64TruncUF64, "i64.trunc_u/f64")
WABT_OPCODE(F32,  I32,  ___,  ___,  0,  0x00b2, F32ConvertSI32, "f32.convert_s/i32")
WABT_OPCODE(F32,  I32,  ___,  ___,  0,  0x00b3, F32ConvertUI32, "f32.convert_u/i32")
WABT_OPCODE(F32,  I64,  ___,  ___,  0,  0x00b4, F32ConvertSI64, "f32.convert_s/i64")
WABT_OPCODE(F32,  I64,  ___,  ___,  0,  0x00b5, F32ConvertUI64, "f32.convert_u/i64")
WABT_OPCODE(F32,  F64,  ___,  ___,  0,  0x00b6, F32DemoteF64, "f32.demote/f64")
WABT_OPCODE(F64,  I32,  ___,  ___,  0,  0x00b7, F64ConvertSI32, "f64.convert_s/i32")
WABT_OPCODE(F64,  I32,  ___,  ___,  0,  0x00b8, F64ConvertUI32, "f64.convert_u/i32")
WABT_OPCODE(F64,  I64,  ___,  ___,  0,  0x00b9, F64ConvertSI64, "f64.convert_s/i64")
WABT_OPCODE(F64,  I64,  ___,  ___,  0,  0x00ba, F64ConvertUI64, "f64.convert_u/i64")
WABT_OPCODE(F64,  F32,  ___,  ___,  0,  0x00bb, F64PromoteF32, "f64.promote/f32")
WABT_OPCODE(I32,  F32,  ___,  ___,  0,  0x00bc, I32ReinterpretF32, "i32.reinterpret/f32")
WABT_OPCODE(I64,  F64,  ___,  ___,  0,  0x00bd, I64ReinterpretF64, "i64.reinterpret/f64")
WABT_OPCODE(F32,  I32,  ___,  ___,  0,  0x00be, F32ReinterpretI32, "f32.reinterpret/i32")
WABT_OPCODE(F64,  I64,  ___,  ___,  0,  0x00bf, F64ReinterpretI64, "f64.reinterpret/i64")

/* Sign-extension opcodes (--enable-sign-extension) */
WABT_OPCODE(I32,  I32,  ___,  ___,  0,  0x00C0, I32Extend8S, "i32.extend8_s")
WABT_OPCODE(I32,  I32,  ___,  ___,  0,  0x00C1, I32Extend16S, "i32.extend16_s")
WABT_OPCODE(I64,  I64,  ___,  ___,  0,  0x00C2, I64Extend8S, "i64.extend8_s")
WABT_OPCODE(I64,  I64,  ___,  ___,  0,  0x00C3, I64Extend16S, "i64.extend16_s")
WABT_OPCODE(I64,  I64,  ___,  ___,  0,  0x00C4, I64Extend32S, "i64.extend32_s")

/* Interpreter-only opcodes */
WABT_OPCODE(___,  ___,  ___,  ___,  0,  0x00e0, InterpAlloca, "alloca")
WABT_OPCODE(___,  ___,  ___,  ___,  0,  0x00e1, InterpBrUnless, "br_unless")
WABT_OPCODE(___,  ___,  ___,  ___,  0,  0x00e2, InterpCallHost, "call_host")
WABT_OPCODE(___,  ___,  ___,  ___,  0,  0x00e3, InterpData, "data")
WABT_OPCODE(___,  ___,  ___,  ___,  0,  0x00e4, InterpDropKeep, "drop_keep")

/* Saturating float-to-int opcodes (--enable-saturating-float-to-int) */
WABT_OPCODE(I32,  F32,  ___,  ___,  0,  0xfc00, I32TruncSSatF32, "i32.trunc_s:sat/f32")
WABT_OPCODE(I32,  F32,  ___,  ___,  0,  0xfc01, I32TruncUSatF32, "i32.trunc_u:sat/f32")
WABT_OPCODE(I32,  F64,  ___,  ___,  0,  0xfc02, I32TruncSSatF64, "i32.trunc_s:sat/f64")
WABT_OPCODE(I32,  F64,  ___,  ___,  0,  0xfc03, I32TruncUSatF64, "i32.trunc_u:sat/f64")
WABT_OPCODE(I64,  F32,  ___,  ___,  0,  0xfc04, I64TruncSSatF32, "i64.trunc_s:sat/f32")
WABT_OPCODE(I64,  F32,  ___,  ___,  0,  0xfc05, I64TruncUSatF32, "i64.trunc_u:sat/f32")
WABT_OPCODE(I64,  F64,  ___,  ___,  0,  0xfc06, I64TruncSSatF64, "i64.trunc_s:sat/f64")
WABT_OPCODE(I64,  F64,  ___,  ___,  0,  0xfc07, I64TruncUSatF64, "i64.trunc_u:sat/f64")

/* Simd opcodes (--enable-simd) */
WABT_OPCODE(V128, ___,  ___,  ___,  0,  0xfd00, V128Const, "uint128.const")
WABT_OPCODE(V128, I32,  ___,  ___,  16, 0xfd01, V128Load,  "uint128.load")
WABT_OPCODE(___,  I32,  V128, ___,  16, 0xfd02, V128Store, "uint128.store")
WABT_OPCODE(V128, I32,  ___,  ___,  0,  0xfd03, I8X16Splat, "i8x16.splat")
WABT_OPCODE(V128, I32,  ___,  ___,  0,  0xfd04, I16X8Splat, "i16x8.splat")
WABT_OPCODE(V128, I32,  ___,  ___,  0,  0xfd05, I32X4Splat, "i32x4.splat")
WABT_OPCODE(V128, I64,  ___,  ___,  0,  0xfd06, I64X2Splat, "i64x2.splat")
WABT_OPCODE(V128, F32,  ___,  ___,  0,  0xfd07, F32X4Splat, "f32x4.splat")
WABT_OPCODE(V128, F64,  ___,  ___,  0,  0xfd08, F64X2Splat, "f64x2.splat")
WABT_OPCODE(I32,  V128, ___,  ___,  0,  0xfd09, I8X16ExtractLaneS, "i8x16.extract_lane_s")
WABT_OPCODE(I32,  V128, ___,  ___,  0,  0xfd0a, I8X16ExtractLaneU, "i8x16.extract_lane_u")
WABT_OPCODE(I32,  V128, ___,  ___,  0,  0xfd0b, I16X8ExtractLaneS, "i16x8.extract_lane_s")
WABT_OPCODE(I32,  V128, ___,  ___,  0,  0xfd0c, I16X8ExtractLaneU, "i16x8.extract_lane_u")
WABT_OPCODE(I32,  V128, ___,  ___,  0,  0xfd0d, I32X4ExtractLane, "i32x4.extract_lane")
WABT_OPCODE(I64,  V128, ___,  ___,  0,  0xfd0e, I64X2ExtractLane, "i64x2.extract_lane")
WABT_OPCODE(F32,  V128, ___,  ___,  0,  0xfd0f, F32X4ExtractLane, "f32x4.extract_lane")
WABT_OPCODE(F64,  V128, ___,  ___,  0,  0xfd10, F64X2ExtractLane, "f64x2.extract_lane")
WABT_OPCODE(V128, V128, I32,  ___,  0,  0xfd11, I8X16ReplaceLane, "i8x16.replace_lane")
WABT_OPCODE(V128, V128, I32,  ___,  0,  0xfd12, I16X8ReplaceLane, "i16x8.replace_lane")
WABT_OPCODE(V128, V128, I32,  ___,  0,  0xfd13, I32X4ReplaceLane, "i32x4.replace_lane")
WABT_OPCODE(V128, V128, I64,  ___,  0,  0xfd14, I64X2ReplaceLane, "i64x2.replace_lane")
WABT_OPCODE(V128, V128, F32,  ___,  0,  0xfd15, F32X4ReplaceLane, "f32x4.replace_lane")
WABT_OPCODE(V128, V128, F64,  ___,  0,  0xfd16, F64X2ReplaceLane, "f64x2.replace_lane")
WABT_OPCODE(V128, V128, V128, ___,  0,  0xfd17, V8X16Shuffle, "v8x16.shuffle")
WABT_OPCODE(V128, V128, V128, ___,  0,  0xfd18, I8X16Add, "i8x16.add")
WABT_OPCODE(V128, V128, V128, ___,  0,  0xfd19, I16X8Add, "i16x8.add")
WABT_OPCODE(V128, V128, V128, ___,  0,  0xfd1a, I32X4Add, "i32x4.add")
WABT_OPCODE(V128, V128, V128, ___,  0,  0xfd1b, I64X2Add, "i64x2.add")
WABT_OPCODE(V128, V128, V128, ___,  0,  0xfd1c, I8X16Sub, "i8x16.sub")
WABT_OPCODE(V128, V128, V128, ___,  0,  0xfd1d, I16X8Sub, "i16x8.sub")
WABT_OPCODE(V128, V128, V128, ___,  0,  0xfd1e, I32X4Sub, "i32x4.sub")
WABT_OPCODE(V128, V128, V128, ___,  0,  0xfd1f, I64X2Sub, "i64x2.sub")
WABT_OPCODE(V128, V128, V128, ___,  0,  0xfd20, I8X16Mul, "i8x16.mul")
WABT_OPCODE(V128, V128, V128, ___,  0,  0xfd21, I16X8Mul, "i16x8.mul")
WABT_OPCODE(V128, V128, V128, ___,  0,  0xfd22, I32X4Mul, "i32x4.mul")
WABT_OPCODE(V128, V128, ___,  ___,  0,  0xfd23, I8X16Neg, "i8x16.neg")
WABT_OPCODE(V128, V128, ___,  ___,  0,  0xfd24, I16X8Neg, "i16x8.neg")
WABT_OPCODE(V128, V128, ___,  ___,  0,  0xfd25, I32X4Neg, "i32x4.neg")
WABT_OPCODE(V128, V128, ___,  ___,  0,  0xfd26, I64X2Neg, "i64x2.neg")
WABT_OPCODE(V128, V128, V128, ___,  0,  0xfd27, I8X16AddSaturateS, "i8x16.add_saturate_s")
WABT_OPCODE(V128, V128, V128, ___,  0,  0xfd28, I8X16AddSaturateU, "i8x16.add_saturate_u")
WABT_OPCODE(V128, V128, V128, ___,  0,  0xfd29, I16X8AddSaturateS, "i16x8.add_saturate_s")
WABT_OPCODE(V128, V128, V128, ___,  0,  0xfd2a, I16X8AddSaturateU, "i16x8.add_saturate_u")
WABT_OPCODE(V128, V128, V128, ___,  0,  0xfd2b, I8X16SubSaturateS, "i8x16.sub_saturate_s")
WABT_OPCODE(V128, V128, V128, ___,  0,  0xfd2c, I8X16SubSaturateU, "i8x16.sub_saturate_u")
WABT_OPCODE(V128, V128, V128, ___,  0,  0xfd2d, I16X8SubSaturateS, "i16x8.sub_saturate_s")
WABT_OPCODE(V128, V128, V128, ___,  0,  0xfd2e, I16X8SubSaturateU, "i16x8.sub_saturate_u")
WABT_OPCODE(V128, V128, I32,  ___,  0,  0xfd2f, I8X16Shl, "i8x16.shl")
WABT_OPCODE(V128, V128, I32,  ___,  0,  0xfd30, I16X8Shl, "i16x8.shl")
WABT_OPCODE(V128, V128, I32,  ___,  0,  0xfd31, I32X4Shl, "i32x4.shl")
WABT_OPCODE(V128, V128, I32,  ___,  0,  0xfd32, I64X2Shl, "i64x2.shl")
WABT_OPCODE(V128, V128, I32,  ___,  0,  0xfd33, I8X16ShrS, "i8x16.shr_s")
WABT_OPCODE(V128, V128, I32,  ___,  0,  0xfd34, I8X16ShrU, "i8x16.shr_u")
WABT_OPCODE(V128, V128, I32,  ___,  0,  0xfd35, I16X8ShrS, "i16x8.shr_s")
WABT_OPCODE(V128, V128, I32,  ___,  0,  0xfd36, I16X8ShrU, "i16x8.shr_u")
WABT_OPCODE(V128, V128, I32,  ___,  0,  0xfd37, I32X4ShrS, "i32x4.shr_s")
WABT_OPCODE(V128, V128, I32,  ___,  0,  0xfd38, I32X4ShrU, "i32x4.shr_u")
WABT_OPCODE(V128, V128, I32,  ___,  0,  0xfd39, I64X2ShrS, "i64x2.shr_s")
WABT_OPCODE(V128, V128, I32,  ___,  0,  0xfd3a, I64X2ShrU, "i64x2.shr_u")
WABT_OPCODE(V128, V128, V128, ___,  0,  0xfd3b, V128And, "uint128.and")
WABT_OPCODE(V128, V128, V128, ___,  0,  0xfd3c, V128Or,  "uint128.or")
WABT_OPCODE(V128, V128, V128, ___,  0,  0xfd3d, V128Xor, "uint128.xor")
WABT_OPCODE(V128, V128, ___,  ___,  0,  0xfd3e, V128Not, "uint128.not")
WABT_OPCODE(V128, V128, V128, V128, 0,  0xfd3f, V128BitSelect, "uint128.bitselect")
WABT_OPCODE(I32,  V128, ___,  ___,  0,  0xfd40, I8X16AnyTrue, "i8x16.any_true")
WABT_OPCODE(I32,  V128, ___,  ___,  0,  0xfd41, I16X8AnyTrue, "i16x8.any_true")
WABT_OPCODE(I32,  V128, ___,  ___,  0,  0xfd42, I32X4AnyTrue, "i32x4.any_true")
WABT_OPCODE(I32,  V128, ___,  ___,  0,  0xfd43, I64X2AnyTrue, "i64x2.any_true")
WABT_OPCODE(I32,  V128, ___,  ___,  0,  0xfd44, I8X16AllTrue, "i8x16.all_true")
WABT_OPCODE(I32,  V128, ___,  ___,  0,  0xfd45, I16X8AllTrue, "i16x8.all_true")
WABT_OPCODE(I32,  V128, ___,  ___,  0,  0xfd46, I32X4AllTrue, "i32x4.all_true")
WABT_OPCODE(I32,  V128, ___,  ___,  0,  0xfd47, I64X2AllTrue, "i64x2.all_true")
WABT_OPCODE(V128, V128, V128, ___,  0,  0xfd48, I8X16Eq, "i8x16.eq")
WABT_OPCODE(V128, V128, V128, ___,  0,  0xfd49, I16X8Eq, "i16x8.eq")
WABT_OPCODE(V128, V128, V128, ___,  0,  0xfd4a, I32X4Eq, "i32x4.eq")
WABT_OPCODE(V128, V128, V128, ___,  0,  0xfd4b, F32X4Eq, "f32x4.eq")
WABT_OPCODE(V128, V128, V128, ___,  0,  0xfd4c, F64X2Eq, "f64x2.eq")
WABT_OPCODE(V128, V128, V128, ___,  0,  0xfd4d, I8X16Ne, "i8x16.ne")
WABT_OPCODE(V128, V128, V128, ___,  0,  0xfd4e, I16X8Ne, "i16x8.ne")
WABT_OPCODE(V128, V128, V128, ___,  0,  0xfd4f, I32X4Ne, "i32x4.ne")
WABT_OPCODE(V128, V128, V128, ___,  0,  0xfd50, F32X4Ne, "f32x4.ne")
WABT_OPCODE(V128, V128, V128, ___,  0,  0xfd51, F64X2Ne, "f64x2.ne")
WABT_OPCODE(V128, V128, V128, ___,  0,  0xfd52, I8X16LtS, "i8x16.lt_s")
WABT_OPCODE(V128, V128, V128, ___,  0,  0xfd53, I8X16LtU, "i8x16.lt_u")
WABT_OPCODE(V128, V128, V128, ___,  0,  0xfd54, I16X8LtS, "i16x8.lt_s")
WABT_OPCODE(V128, V128, V128, ___,  0,  0xfd55, I16X8LtU, "i16x8.lt_u")
WABT_OPCODE(V128, V128, V128, ___,  0,  0xfd56, I32X4LtS, "i32x4.lt_s")
WABT_OPCODE(V128, V128, V128, ___,  0,  0xfd57, I32X4LtU, "i32x4.lt_u")
WABT_OPCODE(V128, V128, V128, ___,  0,  0xfd58, F32X4Lt, "f32x4.lt")
WABT_OPCODE(V128, V128, V128, ___,  0,  0xfd59, F64X2Lt, "f64x2.lt")
WABT_OPCODE(V128, V128, V128, ___,  0,  0xfd5a, I8X16LeS, "i8x16.le_s")
WABT_OPCODE(V128, V128, V128, ___,  0,  0xfd5b, I8X16LeU, "i8x16.le_u")
WABT_OPCODE(V128, V128, V128, ___,  0,  0xfd5c, I16X8LeS, "i16x8.le_s")
WABT_OPCODE(V128, V128, V128, ___,  0,  0xfd5d, I16X8LeU, "i16x8.le_u")
WABT_OPCODE(V128, V128, V128, ___,  0,  0xfd5e, I32X4LeS, "i32x4.le_s")
WABT_OPCODE(V128, V128, V128, ___,  0,  0xfd5f, I32X4LeU, "i32x4.le_u")
WABT_OPCODE(V128, V128, V128, ___,  0,  0xfd60, F32X4Le, "f32x4.le")
WABT_OPCODE(V128, V128, V128, ___,  0,  0xfd61, F64X2Le, "f64x2.le")
WABT_OPCODE(V128, V128, V128, ___,  0,  0xfd62, I8X16GtS, "i8x16.gt_s")
WABT_OPCODE(V128, V128, V128, ___,  0,  0xfd63, I8X16GtU, "i8x16.gt_u")
WABT_OPCODE(V128, V128, V128, ___,  0,  0xfd64, I16X8GtS, "i16x8.gt_s")
WABT_OPCODE(V128, V128, V128, ___,  0,  0xfd65, I16X8GtU, "i16x8.gt_u")
WABT_OPCODE(V128, V128, V128, ___,  0,  0xfd66, I32X4GtS, "i32x4.gt_s")
WABT_OPCODE(V128, V128, V128, ___,  0,  0xfd67, I32X4GtU, "i32x4.gt_u")
WABT_OPCODE(V128, V128, V128, ___,  0,  0xfd68, F32X4Gt, "f32x4.gt")
WABT_OPCODE(V128, V128, V128, ___,  0,  0xfd69, F64X2Gt, "f64x2.gt")
WABT_OPCODE(V128, V128, V128, ___,  0,  0xfd6a, I8X16GeS, "i8x16.ge_s")
WABT_OPCODE(V128, V128, V128, ___,  0,  0xfd6b, I8X16GeU, "i8x16.ge_u")
WABT_OPCODE(V128, V128, V128, ___,  0,  0xfd6c, I16X8GeS, "i16x8.ge_s")
WABT_OPCODE(V128, V128, V128, ___,  0,  0xfd6d, I16X8GeU, "i16x8.ge_u")
WABT_OPCODE(V128, V128, V128, ___,  0,  0xfd6e, I32X4GeS, "i32x4.ge_s")
WABT_OPCODE(V128, V128, V128, ___,  0,  0xfd6f, I32X4GeU, "i32x4.ge_u")
WABT_OPCODE(V128, V128, V128, ___,  0,  0xfd70, F32X4Ge, "f32x4.ge")
WABT_OPCODE(V128, V128, V128, ___,  0,  0xfd71, F64X2Ge, "f64x2.ge")
WABT_OPCODE(V128, V128, ___,  ___,  0,  0xfd72, F32X4Neg, "f32x4.neg")
WABT_OPCODE(V128, V128, ___,  ___,  0,  0xfd73, F64X2Neg, "f64x2.neg")
WABT_OPCODE(V128, V128, ___,  ___,  0,  0xfd74, F32X4Abs, "f32x4.abs")
WABT_OPCODE(V128, V128, ___,  ___,  0,  0xfd75, F64X2Abs, "f64x2.abs")
WABT_OPCODE(V128, V128, V128, ___,  0,  0xfd76, F32X4Min, "f32x4.min")
WABT_OPCODE(V128, V128, V128, ___,  0,  0xfd77, F64X2Min, "f64x2.min")
WABT_OPCODE(V128, V128, V128, ___,  0,  0xfd78, F32X4Max, "f32x4.max")
WABT_OPCODE(V128, V128, V128, ___,  0,  0xfd79, F64X2Max, "f64x2.max")
WABT_OPCODE(V128, V128, V128, ___,  0,  0xfd7a, F32X4Add, "f32x4.add")
WABT_OPCODE(V128, V128, V128, ___,  0,  0xfd7b, F64X2Add, "f64x2.add")
WABT_OPCODE(V128, V128, V128, ___,  0,  0xfd7c, F32X4Sub, "f32x4.sub")
WABT_OPCODE(V128, V128, V128, ___,  0,  0xfd7d, F64X2Sub, "f64x2.sub")
WABT_OPCODE(V128, V128, V128, ___,  0,  0xfd7e, F32X4Div, "f32x4.div")
WABT_OPCODE(V128, V128, V128, ___,  0,  0xfd7f, F64X2Div, "f64x2.div")
WABT_OPCODE(V128, V128, V128, ___,  0,  0xfd80, F32X4Mul, "f32x4.mul")
WABT_OPCODE(V128, V128, V128, ___,  0,  0xfd81, F64X2Mul, "f64x2.mul")
WABT_OPCODE(V128, V128, ___,  ___,  0,  0xfd82, F32X4Sqrt, "f32x4.sqrt")
WABT_OPCODE(V128, V128, ___,  ___,  0,  0xfd83, F64X2Sqrt, "f64x2.sqrt")
WABT_OPCODE(V128, V128, ___,  ___,  0,  0xfd84, F32X4ConvertSI32X4, "f32x4.convert_s/i32x4")
WABT_OPCODE(V128, V128, ___,  ___,  0,  0xfd85, F32X4ConvertUI32X4, "f32x4.convert_u/i32x4")
WABT_OPCODE(V128, V128, ___,  ___,  0,  0xfd86, F64X2ConvertSI64X2, "f64x2.convert_s/i64x2")
WABT_OPCODE(V128, V128, ___,  ___,  0,  0xfd87, F64X2ConvertUI64X2, "f64x2.convert_u/i64x2")
WABT_OPCODE(V128, V128, ___,  ___,  0,  0xfd88, I32X4TruncSF32X4Sat,"i32x4.trunc_s/f32x4:sat")
WABT_OPCODE(V128, V128, ___,  ___,  0,  0xfd89, I32X4TruncUF32X4Sat,"i32x4.trunc_u/f32x4:sat")
WABT_OPCODE(V128, V128, ___,  ___,  0,  0xfd8a, I64X2TruncSF64X2Sat,"i64x2.trunc_s/f64x2:sat")
WABT_OPCODE(V128, V128, ___,  ___,  0,  0xfd8b, I64X2TruncUF64X2Sat,"i64x2.trunc_u/f64x2:sat")

/* Thread opcodes (--enable-threads) */
WABT_OPCODE(I32,  I32,  I32,  ___,  4,  0xfe00, AtomicWake, "atomic.wake")
WABT_OPCODE(I32,  I32,  I32,  I64,  4,  0xfe01, I32AtomicWait, "i32.atomic.wait")
WABT_OPCODE(I32,  I32,  I64,  I64,  8,  0xfe02, I64AtomicWait, "i64.atomic.wait")
WABT_OPCODE(I32,  I32,  ___,  ___,  4,  0xfe10, I32AtomicLoad, "i32.atomic.load")
WABT_OPCODE(I64,  I32,  ___,  ___,  8,  0xfe11, I64AtomicLoad, "i64.atomic.load")
WABT_OPCODE(I32,  I32,  ___,  ___,  1,  0xfe12, I32AtomicLoad8U, "i32.atomic.load8_u")
WABT_OPCODE(I32,  I32,  ___,  ___,  2,  0xfe13, I32AtomicLoad16U, "i32.atomic.load16_u")
WABT_OPCODE(I64,  I32,  ___,  ___,  1,  0xfe14, I64AtomicLoad8U, "i64.atomic.load8_u")
WABT_OPCODE(I64,  I32,  ___,  ___,  2,  0xfe15, I64AtomicLoad16U, "i64.atomic.load16_u")
WABT_OPCODE(I64,  I32,  ___,  ___,  4,  0xfe16, I64AtomicLoad32U, "i64.atomic.load32_u")
WABT_OPCODE(___,  I32,  I32,  ___,  4,  0xfe17, I32AtomicStore, "i32.atomic.store")
WABT_OPCODE(___,  I32,  I64,  ___,  8,  0xfe18, I64AtomicStore, "i64.atomic.store")
WABT_OPCODE(___,  I32,  I32,  ___,  1,  0xfe19, I32AtomicStore8, "i32.atomic.store8")
WABT_OPCODE(___,  I32,  I32,  ___,  2,  0xfe1a, I32AtomicStore16, "i32.atomic.store16")
WABT_OPCODE(___,  I32,  I64,  ___,  1,  0xfe1b, I64AtomicStore8, "i64.atomic.store8")
WABT_OPCODE(___,  I32,  I64,  ___,  2,  0xfe1c, I64AtomicStore16, "i64.atomic.store16")
WABT_OPCODE(___,  I32,  I64,  ___,  4,  0xfe1d, I64AtomicStore32, "i64.atomic.store32")
WABT_OPCODE(I32,  I32,  I32,  ___,  4,  0xfe1e, I32AtomicRmwAdd, "i32.atomic.rmw.add")
WABT_OPCODE(I64,  I32,  I64,  ___,  8,  0xfe1f, I64AtomicRmwAdd, "i64.atomic.rmw.add")
WABT_OPCODE(I32,  I32,  I32,  ___,  1,  0xfe20, I32AtomicRmw8UAdd, "i32.atomic.rmw8_u.add")
WABT_OPCODE(I32,  I32,  I32,  ___,  2,  0xfe21, I32AtomicRmw16UAdd, "i32.atomic.rmw16_u.add")
WABT_OPCODE(I64,  I32,  I64,  ___,  1,  0xfe22, I64AtomicRmw8UAdd, "i64.atomic.rmw8_u.add")
WABT_OPCODE(I64,  I32,  I64,  ___,  2,  0xfe23, I64AtomicRmw16UAdd, "i64.atomic.rmw16_u.add")
WABT_OPCODE(I64,  I32,  I64,  ___,  4,  0xfe24, I64AtomicRmw32UAdd, "i64.atomic.rmw32_u.add")
WABT_OPCODE(I32,  I32,  I32,  ___,  4,  0xfe25, I32AtomicRmwSub, "i32.atomic.rmw.sub")
WABT_OPCODE(I64,  I32,  I64,  ___,  8,  0xfe26, I64AtomicRmwSub, "i64.atomic.rmw.sub")
WABT_OPCODE(I32,  I32,  I32,  ___,  1,  0xfe27, I32AtomicRmw8USub, "i32.atomic.rmw8_u.sub")
WABT_OPCODE(I32,  I32,  I32,  ___,  2,  0xfe28, I32AtomicRmw16USub, "i32.atomic.rmw16_u.sub")
WABT_OPCODE(I64,  I32,  I64,  ___,  1,  0xfe29, I64AtomicRmw8USub, "i64.atomic.rmw8_u.sub")
WABT_OPCODE(I64,  I32,  I64,  ___,  2,  0xfe2a, I64AtomicRmw16USub, "i64.atomic.rmw16_u.sub")
WABT_OPCODE(I64,  I32,  I64,  ___,  4,  0xfe2b, I64AtomicRmw32USub, "i64.atomic.rmw32_u.sub")
WABT_OPCODE(I32,  I32,  I32,  ___,  4,  0xfe2c, I32AtomicRmwAnd, "i32.atomic.rmw.and")
WABT_OPCODE(I64,  I32,  I64,  ___,  8,  0xfe2d, I64AtomicRmwAnd, "i64.atomic.rmw.and")
WABT_OPCODE(I32,  I32,  I32,  ___,  1,  0xfe2e, I32AtomicRmw8UAnd, "i32.atomic.rmw8_u.and")
WABT_OPCODE(I32,  I32,  I32,  ___,  2,  0xfe2f, I32AtomicRmw16UAnd, "i32.atomic.rmw16_u.and")
WABT_OPCODE(I64,  I32,  I64,  ___,  1,  0xfe30, I64AtomicRmw8UAnd, "i64.atomic.rmw8_u.and")
WABT_OPCODE(I64,  I32,  I64,  ___,  2,  0xfe31, I64AtomicRmw16UAnd, "i64.atomic.rmw16_u.and")
WABT_OPCODE(I64,  I32,  I64,  ___,  4,  0xfe32, I64AtomicRmw32UAnd, "i64.atomic.rmw32_u.and")
WABT_OPCODE(I32,  I32,  I32,  ___,  4,  0xfe33, I32AtomicRmwOr, "i32.atomic.rmw.or")
WABT_OPCODE(I64,  I32,  I64,  ___,  8,  0xfe34, I64AtomicRmwOr, "i64.atomic.rmw.or")
WABT_OPCODE(I32,  I32,  I32,  ___,  1,  0xfe35, I32AtomicRmw8UOr, "i32.atomic.rmw8_u.or")
WABT_OPCODE(I32,  I32,  I32,  ___,  2,  0xfe36, I32AtomicRmw16UOr, "i32.atomic.rmw16_u.or")
WABT_OPCODE(I64,  I32,  I64,  ___,  1,  0xfe37, I64AtomicRmw8UOr, "i64.atomic.rmw8_u.or")
WABT_OPCODE(I64,  I32,  I64,  ___,  2,  0xfe38, I64AtomicRmw16UOr, "i64.atomic.rmw16_u.or")
WABT_OPCODE(I64,  I32,  I64,  ___,  4,  0xfe39, I64AtomicRmw32UOr, "i64.atomic.rmw32_u.or")
WABT_OPCODE(I32,  I32,  I32,  ___,  4,  0xfe3a, I32AtomicRmwXor, "i32.atomic.rmw.xor")
WABT_OPCODE(I64,  I32,  I64,  ___,  8,  0xfe3b, I64AtomicRmwXor, "i64.atomic.rmw.xor")
WABT_OPCODE(I32,  I32,  I32,  ___,  1,  0xfe3c, I32AtomicRmw8UXor, "i32.atomic.rmw8_u.xor")
WABT_OPCODE(I32,  I32,  I32,  ___,  2,  0xfe3d, I32AtomicRmw16UXor, "i32.atomic.rmw16_u.xor")
WABT_OPCODE(I64,  I32,  I64,  ___,  1,  0xfe3e, I64AtomicRmw8UXor, "i64.atomic.rmw8_u.xor")
WABT_OPCODE(I64,  I32,  I64,  ___,  2,  0xfe3f, I64AtomicRmw16UXor, "i64.atomic.rmw16_u.xor")
WABT_OPCODE(I64,  I32,  I64,  ___,  4,  0xfe40, I64AtomicRmw32UXor, "i64.atomic.rmw32_u.xor")
WABT_OPCODE(I32,  I32,  I32,  ___,  4,  0xfe41, I32AtomicRmwXchg, "i32.atomic.rmw.xchg")
WABT_OPCODE(I64,  I32,  I64,  ___,  8,  0xfe42, I64AtomicRmwXchg, "i64.atomic.rmw.xchg")
WABT_OPCODE(I32,  I32,  I32,  ___,  1,  0xfe43, I32AtomicRmw8UXchg, "i32.atomic.rmw8_u.xchg")
WABT_OPCODE(I32,  I32,  I32,  ___,  2,  0xfe44, I32AtomicRmw16UXchg, "i32.atomic.rmw16_u.xchg")
WABT_OPCODE(I64,  I32,  I64,  ___,  1,  0xfe45, I64AtomicRmw8UXchg, "i64.atomic.rmw8_u.xchg")
WABT_OPCODE(I64,  I32,  I64,  ___,  2,  0xfe46, I64AtomicRmw16UXchg, "i64.atomic.rmw16_u.xchg")
WABT_OPCODE(I64,  I32,  I64,  ___,  4,  0xfe47, I64AtomicRmw32UXchg, "i64.atomic.rmw32_u.xchg")
WABT_OPCODE(I32,  I32,  I32,  I32,  4,  0xfe48, I32AtomicRmwCmpxchg, "i32.atomic.rmw.cmpxchg")
WABT_OPCODE(I64,  I32,  I64,  I64,  8,  0xfe49, I64AtomicRmwCmpxchg, "i64.atomic.rmw.cmpxchg")
WABT_OPCODE(I32,  I32,  I32,  I32,  1,  0xfe4a, I32AtomicRmw8UCmpxchg, "i32.atomic.rmw8_u.cmpxchg")
WABT_OPCODE(I32,  I32,  I32,  I32,  2,  0xfe4b, I32AtomicRmw16UCmpxchg, "i32.atomic.rmw16_u.cmpxchg")
WABT_OPCODE(I64,  I32,  I64,  I64,  1,  0xfe4c, I64AtomicRmw8UCmpxchg, "i64.atomic.rmw8_u.cmpxchg")
WABT_OPCODE(I64,  I32,  I64,  I64,  2,  0xfe4d, I64AtomicRmw16UCmpxchg, "i64.atomic.rmw16_u.cmpxchg")
WABT_OPCODE(I64,  I32,  I64,  I64,  4,  0xfe4e, I64AtomicRmw32UCmpxchg, "i64.atomic.rmw32_u.cmpxchg")
#undef WABT_OPCODE
};

class Expr : public e_link<Expr> {
public:
	enum class Type {
		AtomicLoad,
		AtomicRmw,
		AtomicRmwCmpxchg,
		AtomicStore,
		AtomicWait,
		AtomicWake,
		Binary,
		Block,
		Br,
		BrIf,
		BrTable,
		Call,
		CallIndirect,
		Compare,
		Const,
		Convert,
		Drop,
		GetGlobal,
		GetLocal,
		If,
		IfExcept,
		Load,
		Loop,
		MemoryGrow,
		MemorySize,
		Nop,
		Rethrow,
		Return,
		Select,
		SetGlobal,
		SetLocal,
		SimdLaneOp,
		SimdShuffleOp,
		Store,
		TeeLocal,
		Ternary,
		Throw,
		Try,
		Unary,
		Unreachable,
	};
	Type	type;
	Location loc;
	explicit Expr(Type type, const Location& loc = Location()) : type(type), loc(loc) {}
	virtual ~Expr() = default;
};

template<> const char *field_names<Expr::Type>::s[]	= {
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

typedef e_list<Expr> ExprList;
typedef FuncDeclaration BlockDeclaration;

struct Block {
	string				label;
	BlockDeclaration	decl;
	ExprList			exprs;
	Location			end_loc;

	Block() = default;
	explicit Block(ExprList exprs) : exprs(move(exprs)) {}
};

template<Expr::Type TypeEnum> class ExprMixin : public Expr {
public:
	explicit ExprMixin(const Location& loc = Location()) : Expr(TypeEnum, loc) {}
	static bool classof(const Expr* expr) { return expr->type == TypeEnum; }
};

typedef ExprMixin<Expr::Type::Drop>			DropExpr;
typedef ExprMixin<Expr::Type::MemoryGrow>	MemoryGrowExpr;
typedef ExprMixin<Expr::Type::MemorySize>	MemorySizeExpr;
typedef ExprMixin<Expr::Type::Nop>			NopExpr;
typedef ExprMixin<Expr::Type::Rethrow>		RethrowExpr;
typedef ExprMixin<Expr::Type::Return>		ReturnExpr;
typedef ExprMixin<Expr::Type::Select>		SelectExpr;
typedef ExprMixin<Expr::Type::Unreachable>	UnreachableExpr;

template<Expr::Type TypeEnum> class OpcodeExpr : public ExprMixin<TypeEnum> {
public:
	Opcode	opcode;
	OpcodeExpr(Opcode opcode, const Location& loc = Location()) : ExprMixin<TypeEnum>(loc), opcode(opcode) {}
};

typedef OpcodeExpr<Expr::Type::Binary>		BinaryExpr;
typedef OpcodeExpr<Expr::Type::Compare>		CompareExpr;
typedef OpcodeExpr<Expr::Type::Convert>		ConvertExpr;
typedef OpcodeExpr<Expr::Type::Unary>		UnaryExpr;
typedef OpcodeExpr<Expr::Type::Ternary>		TernaryExpr;

class SimdLaneOpExpr : public ExprMixin<Expr::Type::SimdLaneOp> {
public:
	Opcode	opcode;
	uint64	val;
	SimdLaneOpExpr(Opcode opcode, uint64 val, const Location& loc = Location()) : ExprMixin<Expr::Type::SimdLaneOp>(loc), opcode(opcode), val(val) {}
};

class SimdShuffleOpExpr : public ExprMixin<Expr::Type::SimdShuffleOp> {
public:
	Opcode	opcode;
	uint128 val;
	SimdShuffleOpExpr(Opcode opcode, uint128 val, const Location& loc = Location()) : ExprMixin<Expr::Type::SimdShuffleOp>(loc), opcode(opcode), val(val) {}
};

template<Expr::Type TypeEnum> class VarExpr : public ExprMixin<TypeEnum> {
public:
	Var		var;
	VarExpr(const Var& var, const Location& loc = Location()) : ExprMixin<TypeEnum>(loc), var(var) {}
};

typedef VarExpr<Expr::Type::Br>				BrExpr;
typedef VarExpr<Expr::Type::BrIf>			BrIfExpr;
typedef VarExpr<Expr::Type::Call>			CallExpr;
typedef VarExpr<Expr::Type::GetGlobal>		GetGlobalExpr;
typedef VarExpr<Expr::Type::GetLocal>		GetLocalExpr;
typedef VarExpr<Expr::Type::SetGlobal>		SetGlobalExpr;
typedef VarExpr<Expr::Type::SetLocal>		SetLocalExpr;
typedef VarExpr<Expr::Type::TeeLocal>		TeeLocalExpr;
typedef VarExpr<Expr::Type::Throw>			ThrowExpr;

class CallIndirectExpr : public ExprMixin<Expr::Type::CallIndirect> {
public:
	FuncDeclaration decl;
	explicit CallIndirectExpr(const Location& loc = Location()) : ExprMixin<Expr::Type::CallIndirect>(loc) {}
};

template<Expr::Type TypeEnum> class BlockExprBase : public ExprMixin<TypeEnum> {
public:
	Block	block;
	explicit BlockExprBase(const Location& loc = Location()) : ExprMixin<TypeEnum>(loc) {}
};

typedef BlockExprBase<Expr::Type::Block> BlockExpr;
typedef BlockExprBase<Expr::Type::Loop> LoopExpr;

class IfExpr : public ExprMixin<Expr::Type::If> {
public:
	Block		true_;
	ExprList	false_;
	Location	false_end_loc;
	explicit IfExpr(const Location& loc = Location()) : ExprMixin<Expr::Type::If>(loc) {}
};

class IfExceptExpr : public ExprMixin<Expr::Type::IfExcept> {
public:
	Block		true_;
	ExprList	false_;
	Location	false_end_loc;
	Var			except_var;
	explicit IfExceptExpr(const Location& loc = Location()) : ExprMixin<Expr::Type::IfExcept>(loc) {}
};

class TryExpr : public ExprMixin<Expr::Type::Try> {
public:
	Block		block;
	ExprList	catch_;
	explicit TryExpr(const Location& loc = Location()) : ExprMixin<Expr::Type::Try>(loc) {}
};

class BrTableExpr : public ExprMixin<Expr::Type::BrTable> {
public:
	VarVector	targets;
	Var			default_target;
	BrTableExpr(const Location& loc = Location()) : ExprMixin<Expr::Type::BrTable>(loc) {}
};

class ConstExpr : public ExprMixin<Expr::Type::Const> {
public:
	Const		cnst;
	ConstExpr(const Const& c, const Location& loc = Location()) : ExprMixin<Expr::Type::Const>(loc), cnst(c) {}
};

// TODO(binji): Rename this, it is used for more than loads/stores now.
template<Expr::Type TypeEnum> class LoadStoreExpr : public ExprMixin<TypeEnum> {
public:
	Opcode		opcode;
	Address		align;
	uint32		offset;
	LoadStoreExpr(Opcode opcode, Address align, uint32 offset, const Location& loc = Location()) : ExprMixin<TypeEnum>(loc), opcode(opcode), align(align), offset(offset) {}
};

typedef LoadStoreExpr<Expr::Type::Load>				LoadExpr;
typedef LoadStoreExpr<Expr::Type::Store>			StoreExpr;
typedef LoadStoreExpr<Expr::Type::AtomicLoad>		AtomicLoadExpr;
typedef LoadStoreExpr<Expr::Type::AtomicStore>		AtomicStoreExpr;
typedef LoadStoreExpr<Expr::Type::AtomicRmw>		AtomicRmwExpr;
typedef LoadStoreExpr<Expr::Type::AtomicRmwCmpxchg> AtomicRmwCmpxchgExpr;
typedef LoadStoreExpr<Expr::Type::AtomicWait>		AtomicWaitExpr;
typedef LoadStoreExpr<Expr::Type::AtomicWake>		AtomicWakeExpr;

struct Exception {
	string		name;
	TypeVector	sig;
	explicit Exception(count_string name) : name(name) {}
};

class LocalTypes {
public:
	typedef pair<Type, Index>	Decl;
	dynamic_array<Decl>			decls;

	struct const_iterator {
		const Decl	*decl;
		Index		index;

		const_iterator(const Decl *decl, Index index) : decl(decl), index(index) {}
		Type operator*() const { return decl->a; }
		const_iterator& operator++() {
			++index;
			if (index >= decl->b) {
				++decl;
				index = 0;
			}
			return *this;
		}
		const_iterator operator++(int) {
			const_iterator result = *this;
			operator++();
			return result;
		}

		friend bool operator==(const const_iterator& lhs, const const_iterator& rhs) { return lhs.decl == rhs.decl && lhs.index == rhs.index; }
		friend bool operator!=(const const_iterator& lhs, const const_iterator& rhs) { return !operator==(lhs, rhs); }
	};

	void Set(const TypeVector &types) {
		decls.clear();
		if (types.empty())
			return;

		Type type = types[0];
		Index count = 1;
		for (Index i = 1; i < types.size(); ++i) {
			if (types[i] != type) {
				decls.emplace_back(type, count);
				type = types[i];
				count = 1;
			} else {
				++count;
			}
		}
		decls.emplace_back(type, count);
	}

	void AppendDecl(Type type, Index count) {
		ISO_ASSERT(count > 0);
		decls.emplace_back(type, count);
	}

	Index size() const {
		Index	x = 0;
		for (auto &i : decls)
			x += i.b;
		return x;
	}

	Type operator[](Index i) const {
		Index count = 0;
		for (auto decl : decls)
			if (i < count + decl.b) {
				return decl.a;
			count += decl.b;
		}
		ISO_ASSERT(i < count);
		return Type::Any;
	}

	const_iterator begin()	const { return {decls.begin(), 0}; }
	const_iterator end()	const { return {decls.end(), 0}; }
};

struct Binding {
	Location	loc;
	Index		index;
	explicit Binding(Index index) : index(index) {}
	Binding(const Location& loc, Index index) : loc(loc), index(index) {}
};

class BindingHash : public hash_map_with_key<string, Binding> {
	typedef callback<void(const Binding&, const Binding&)>	DuplicateCallback;

public:
	void FindDuplicates(DuplicateCallback callback) const {
#if 0
		if (size() > 0) {
			dynamic_array<const Binding*> duplicates;

			// This relies on the fact that in an unordered_multimap, all values with the same key are adjacent in iteration order
			auto first = begin();
			bool is_first = true;
			for (auto iter = std::next(first); iter != end(); ++iter) {
				if (first->first == iter->first) {
					if (is_first) {
						duplicates.push_back(&*first);
						is_first = false;
					}
					duplicates.push_back(&*iter);
				} else {
					is_first = true;
					first = iter;
				}
			}
			sort(
				duplicates,
				[](const Binding* lhs, const Binding* rhs) {
					return lhs->loc.line < rhs->loc.line || (lhs->loc.line == rhs->loc.line && lhs->loc.first_col < rhs->loc.first_col);
				}
			);
			for (auto iter = duplicates.begin(), end = duplicates.end(); iter != end;
				++iter) {
				auto first = find_if(duplicates, [iter](const Binding* x) {
					return x->first == (*iter)->first;
				});
				if (first == iter)
					continue;
				ISO_ASSERT(first != duplicates.end());
				callback(**first, **iter);
			}
		}
#endif
	}

	Index FindIndex(const Var& var) const {
		if (var.is_name())
			return FindIndex(var.name());
		return var.index();
	}

	Index FindIndex(const string& name) const {
		auto iter = find(name);
		return iter != end() ? iter->b.index : kInvalidIndex;
	}
};

struct Func {
	string			name;
	FuncDeclaration decl;
	LocalTypes		local_types;
	BindingHash		param_bindings;
	BindingHash		local_bindings;
	ExprList		exprs;
	explicit Func(count_string name) : name(name) {}

	Type	GetLocalType(Index index)	const {
		Index num_params = decl.GetNumParams();
		if (index < num_params)
			return GetParamType(index);
		index -= num_params;
		ISO_ASSERT(index < local_types.size());
		return local_types[index];
	}

	Index	GetLocalIndex(const Var &var) const {
		if (var.is_index())
			return var.index();

		Index result = param_bindings.FindIndex(var);
		if (result != kInvalidIndex)
			return result;

		result = local_bindings.FindIndex(var);
		if (result == kInvalidIndex)
			return result;

		// The locals start after all the params.
		return decl.GetNumParams() + result;
	}

	Type	GetParamType(Index index)	const { return decl.GetParamType(index); }
	Type	GetResultType(Index index)	const { return decl.GetResultType(index); }
	Type	GetLocalType(const Var& var)const { return GetLocalType(GetLocalIndex(var)); }
	Index	GetNumParams()				const { return decl.GetNumParams(); }
	Index	GetNumLocals()				const { return local_types.size(); }
	Index	GetNumParamsAndLocals()		const { return GetNumParams() + GetNumLocals(); }
	Index	GetNumResults()				const { return decl.GetNumResults(); }
};

class ExprVisitor {
public:
	class Delegate {
	public:
		virtual bool	OnBinaryExpr(BinaryExpr*)						{ return true; }
		virtual bool	BeginBlockExpr(BlockExpr*)						{ return true; }
		virtual bool	EndBlockExpr(BlockExpr*)						{ return true; }
		virtual bool	OnBrExpr(BrExpr*)								{ return true; }
		virtual bool	OnBrIfExpr(BrIfExpr*)							{ return true; }
		virtual bool	OnBrTableExpr(BrTableExpr*)						{ return true; }
		virtual bool	OnCallExpr(CallExpr*)							{ return true; }
		virtual bool	OnCallIndirectExpr(CallIndirectExpr*)			{ return true; }
		virtual bool	OnCompareExpr(CompareExpr*)						{ return true; }
		virtual bool	OnConstExpr(ConstExpr*)							{ return true; }
		virtual bool	OnConvertExpr(ConvertExpr*)						{ return true; }
		virtual bool	OnDropExpr(DropExpr*)							{ return true; }
		virtual bool	OnGetGlobalExpr(GetGlobalExpr*)					{ return true; }
		virtual bool	OnGetLocalExpr(GetLocalExpr*)					{ return true; }
		virtual bool	BeginIfExpr(IfExpr*)							{ return true; }
		virtual bool	AfterIfTrueExpr(IfExpr*)						{ return true; }
		virtual bool	EndIfExpr(IfExpr*)								{ return true; }
		virtual bool	BeginIfExceptExpr(IfExceptExpr*)				{ return true; }
		virtual bool	AfterIfExceptTrueExpr(IfExceptExpr*)			{ return true; }
		virtual bool	EndIfExceptExpr(IfExceptExpr*)					{ return true; }
		virtual bool	OnLoadExpr(LoadExpr*)							{ return true; }
		virtual bool	BeginLoopExpr(LoopExpr*)						{ return true; }
		virtual bool	EndLoopExpr(LoopExpr*)							{ return true; }
		virtual bool	OnMemoryGrowExpr(MemoryGrowExpr*)				{ return true; }
		virtual bool	OnMemorySizeExpr(MemorySizeExpr*)				{ return true; }
		virtual bool	OnNopExpr(NopExpr*)								{ return true; }
		virtual bool	OnReturnExpr(ReturnExpr*)						{ return true; }
		virtual bool	OnSelectExpr(SelectExpr*)						{ return true; }
		virtual bool	OnSetGlobalExpr(SetGlobalExpr*)					{ return true; }
		virtual bool	OnSetLocalExpr(SetLocalExpr*)					{ return true; }
		virtual bool	OnStoreExpr(StoreExpr*)							{ return true; }
		virtual bool	OnTeeLocalExpr(TeeLocalExpr*)					{ return true; }
		virtual bool	OnUnaryExpr(UnaryExpr*)							{ return true; }
		virtual bool	OnUnreachableExpr(UnreachableExpr*)				{ return true; }
		virtual bool	BeginTryExpr(TryExpr*)							{ return true; }
		virtual bool	OnCatchExpr(TryExpr*)							{ return true; }
		virtual bool	EndTryExpr(TryExpr*)							{ return true; }
		virtual bool	OnThrowExpr(ThrowExpr*)							{ return true; }
		virtual bool	OnRethrowExpr(RethrowExpr*)						{ return true; }
		virtual bool	OnAtomicWaitExpr(AtomicWaitExpr*)				{ return true; }
		virtual bool	OnAtomicWakeExpr(AtomicWakeExpr*)				{ return true; }
		virtual bool	OnAtomicLoadExpr(AtomicLoadExpr*)				{ return true; }
		virtual bool	OnAtomicStoreExpr(AtomicStoreExpr*)				{ return true; }
		virtual bool	OnAtomicRmwExpr(AtomicRmwExpr*)					{ return true; }
		virtual bool	OnAtomicRmwCmpxchgExpr(AtomicRmwCmpxchgExpr*)	{ return true; }
		virtual bool	OnTernaryExpr(TernaryExpr*)						{ return true; }
		virtual bool	OnSimdLaneOpExpr(SimdLaneOpExpr*)				{ return true; }
		virtual bool	OnSimdShuffleOpExpr(SimdShuffleOpExpr*)			{ return true; }
	};
private:
	enum class State {
		Default,
		Block,
		IfTrue,
		IfFalse,
		IfExceptTrue,
		IfExceptFalse,
		Loop,
		Try,
		Catch,
	};
	Delegate	&delegate;

	// Use parallel arrays instead of array of structs so we can avoid allocating unneeded objects. ExprList::iterator has no default constructor, so it must only be allocated for states that use it
	dynamic_array<State>				state_stack;
	dynamic_array<Expr*>				expr_stack;
	dynamic_array<ExprList::iterator>	expr_iter_stack;

	bool HandleDefaultState(Expr*);

	void PushDefault(Expr* expr) {
		state_stack.emplace_back(State::Default);
		expr_stack.emplace_back(expr);
	}
	void PopDefault() {
		state_stack.pop_back();
		expr_stack.pop_back();
	}
	void PushExprlist(State state, Expr* expr, ExprList& expr_list) {
		state_stack.emplace_back(state);
		expr_stack.emplace_back(expr);
		expr_iter_stack.emplace_back(expr_list.begin());
	}
	void PopExprlist() {
		state_stack.pop_back();
		expr_stack.pop_back();
		expr_iter_stack.pop_back();
	}

public:
	explicit ExprVisitor(Delegate &delegate) : delegate(delegate) {}

	bool VisitExpr(Expr*);

	bool VisitExprList(ExprList& exprs) {
		for (Expr& expr : exprs)
			CHECK_RESULT(VisitExpr(&expr));
		return true;
	}
	bool VisitFunc(Func *func) { return VisitExprList(func->exprs); }
};

bool ExprVisitor::VisitExpr(Expr* root_expr) {
	state_stack.clear();
	expr_stack.clear();
	expr_iter_stack.clear();

	PushDefault(root_expr);

	while (!state_stack.empty()) {
		State state = state_stack.back();
		auto* expr = expr_stack.back();

		switch (state) {
			case State::Default:
				PopDefault();
				CHECK_RESULT(HandleDefaultState(expr));
				break;

			case State::Block: {
				auto block_expr = cast<BlockExpr>(expr);
				auto& iter = expr_iter_stack.back();
				if (iter != block_expr->block.exprs.end()) {
					PushDefault(&*iter++);
				} else {
					CHECK_RESULT(delegate.EndBlockExpr(block_expr));
					PopExprlist();
				}
				break;
			}
			case State::IfTrue: {
				auto if_expr = cast<IfExpr>(expr);
				auto& iter = expr_iter_stack.back();
				if (iter != if_expr->true_.exprs.end()) {
					PushDefault(&*iter++);
				} else {
					CHECK_RESULT(delegate.AfterIfTrueExpr(if_expr));
					PopExprlist();
					PushExprlist(State::IfFalse, expr, if_expr->false_);
				}
				break;
			}
			case State::IfFalse: {
				auto if_expr = cast<IfExpr>(expr);
				auto& iter = expr_iter_stack.back();
				if (iter != if_expr->false_.end()) {
					PushDefault(&*iter++);
				} else {
					CHECK_RESULT(delegate.EndIfExpr(if_expr));
					PopExprlist();
				}
				break;
			}
			case State::IfExceptTrue: {
				auto if_except_expr = cast<IfExceptExpr>(expr);
				auto& iter = expr_iter_stack.back();
				if (iter != if_except_expr->true_.exprs.end()) {
					PushDefault(&*iter++);
				} else {
					CHECK_RESULT(delegate.AfterIfExceptTrueExpr(if_except_expr));
					PopExprlist();
					PushExprlist(State::IfExceptFalse, expr, if_except_expr->false_);
				}
				break;
			}
			case State::IfExceptFalse: {
				auto if_except_expr = cast<IfExceptExpr>(expr);
				auto& iter = expr_iter_stack.back();
				if (iter != if_except_expr->false_.end()) {
					PushDefault(&*iter++);
				} else {
					CHECK_RESULT(delegate.EndIfExceptExpr(if_except_expr));
					PopExprlist();
				}
				break;
			}
			case State::Loop: {
				auto loop_expr = cast<LoopExpr>(expr);
				auto& iter = expr_iter_stack.back();
				if (iter != loop_expr->block.exprs.end()) {
					PushDefault(&*iter++);
				} else {
					CHECK_RESULT(delegate.EndLoopExpr(loop_expr));
					PopExprlist();
				}
				break;
			}
			case State::Try: {
				auto try_expr = cast<TryExpr>(expr);
				auto& iter = expr_iter_stack.back();
				if (iter != try_expr->block.exprs.end()) {
					PushDefault(&*iter++);
				} else {
					if (try_expr->catch_.empty()) {
						CHECK_RESULT(delegate.EndTryExpr(try_expr));
						PopExprlist();
					} else {
						CHECK_RESULT(delegate.OnCatchExpr(try_expr));
						PopExprlist();
						PushExprlist(State::Catch, expr, try_expr->catch_);
					}
				}
				break;
			}
			case State::Catch: {
				auto try_expr = cast<TryExpr>(expr);
				auto& iter = expr_iter_stack.back();
				if (iter != try_expr->catch_.end()) {
					PushDefault(&*iter++);
				} else {
					CHECK_RESULT(delegate.EndTryExpr(try_expr));
					PopExprlist();
				}
				break;
			}
		}
	}
	return true;
}

bool ExprVisitor::HandleDefaultState(Expr* expr) {
	switch (expr->type) {
		case Expr::Type::AtomicLoad:		return delegate.OnAtomicLoadExpr(cast<AtomicLoadExpr>(expr));
		case Expr::Type::AtomicStore:		return delegate.OnAtomicStoreExpr(cast<AtomicStoreExpr>(expr));
		case Expr::Type::AtomicRmw:			return delegate.OnAtomicRmwExpr(cast<AtomicRmwExpr>(expr));
		case Expr::Type::AtomicRmwCmpxchg:	return delegate.OnAtomicRmwCmpxchgExpr(cast<AtomicRmwCmpxchgExpr>(expr));
		case Expr::Type::AtomicWait:		return delegate.OnAtomicWaitExpr(cast<AtomicWaitExpr>(expr));
		case Expr::Type::AtomicWake:		return delegate.OnAtomicWakeExpr(cast<AtomicWakeExpr>(expr));
		case Expr::Type::Binary:			return delegate.OnBinaryExpr(cast<BinaryExpr>(expr));
		case Expr::Type::Block:         {	auto block_expr = cast<BlockExpr>(expr); return delegate.BeginBlockExpr(block_expr) && (PushExprlist(State::Block, expr, block_expr->block.exprs), true); }
		case Expr::Type::Br:			    return delegate.OnBrExpr(cast<BrExpr>(expr));
		case Expr::Type::BrIf:				return delegate.OnBrIfExpr(cast<BrIfExpr>(expr));
		case Expr::Type::BrTable:			return delegate.OnBrTableExpr(cast<BrTableExpr>(expr));
		case Expr::Type::Call:				return delegate.OnCallExpr(cast<CallExpr>(expr));
		case Expr::Type::CallIndirect:		return delegate.OnCallIndirectExpr(cast<CallIndirectExpr>(expr));
		case Expr::Type::Compare:			return delegate.OnCompareExpr(cast<CompareExpr>(expr));
		case Expr::Type::Const:				return delegate.OnConstExpr(cast<ConstExpr>(expr));
		case Expr::Type::Convert:			return delegate.OnConvertExpr(cast<ConvertExpr>(expr));
		case Expr::Type::Drop:				return delegate.OnDropExpr(cast<DropExpr>(expr));
		case Expr::Type::GetGlobal:			return delegate.OnGetGlobalExpr(cast<GetGlobalExpr>(expr));
		case Expr::Type::GetLocal:			return delegate.OnGetLocalExpr(cast<GetLocalExpr>(expr));
		case Expr::Type::If:            {	auto if_expr = cast<IfExpr>(expr);			            return delegate.BeginIfExpr(if_expr) && (PushExprlist(State::IfTrue, expr, if_expr->true_.exprs), true); }
		case Expr::Type::IfExcept:      {	auto if_except_expr = cast<IfExceptExpr>(expr);			return delegate.BeginIfExceptExpr(if_except_expr) && (PushExprlist(State::IfExceptTrue, expr, if_except_expr->true_.exprs), true); }
		case Expr::Type::Load:				return delegate.OnLoadExpr(cast<LoadExpr>(expr));
		case Expr::Type::Loop:          {	auto loop_expr = cast<LoopExpr>(expr);			        return delegate.BeginLoopExpr(loop_expr) && (PushExprlist(State::Loop, expr, loop_expr->block.exprs), true); }
		case Expr::Type::MemoryGrow:		return delegate.OnMemoryGrowExpr(cast<MemoryGrowExpr>(expr));
		case Expr::Type::MemorySize:		return delegate.OnMemorySizeExpr(cast<MemorySizeExpr>(expr));
		case Expr::Type::Nop:				return delegate.OnNopExpr(cast<NopExpr>(expr));
		case Expr::Type::Rethrow:			return delegate.OnRethrowExpr(cast<RethrowExpr>(expr));
		case Expr::Type::Return:			return delegate.OnReturnExpr(cast<ReturnExpr>(expr));
		case Expr::Type::Select:			return delegate.OnSelectExpr(cast<SelectExpr>(expr));
		case Expr::Type::SetGlobal:			return delegate.OnSetGlobalExpr(cast<SetGlobalExpr>(expr));
		case Expr::Type::SetLocal:			return delegate.OnSetLocalExpr(cast<SetLocalExpr>(expr));
		case Expr::Type::Store:				return delegate.OnStoreExpr(cast<StoreExpr>(expr));
		case Expr::Type::TeeLocal:			return delegate.OnTeeLocalExpr(cast<TeeLocalExpr>(expr));
		case Expr::Type::Throw:				return delegate.OnThrowExpr(cast<ThrowExpr>(expr));
		case Expr::Type::Try:           {	auto try_expr = cast<TryExpr>(expr);			        return delegate.BeginTryExpr(try_expr) && (PushExprlist(State::Try, expr, try_expr->block.exprs), true); }
		case Expr::Type::Unary:				return delegate.OnUnaryExpr(cast<UnaryExpr>(expr));
		case Expr::Type::Ternary:			return delegate.OnTernaryExpr(cast<TernaryExpr>(expr));
		case Expr::Type::SimdLaneOp:		return delegate.OnSimdLaneOpExpr(cast<SimdLaneOpExpr>(expr));
		case Expr::Type::SimdShuffleOp:		return delegate.OnSimdShuffleOpExpr(cast<SimdShuffleOpExpr>(expr));
		case Expr::Type::Unreachable:		return delegate.OnUnreachableExpr(cast<UnreachableExpr>(expr));
		default: return true;
	}
}

//-----------------------------------------------------------------------------
//	module
//-----------------------------------------------------------------------------

struct Global {
	string		name;
	Type		type	= Type::Void;
	bool		mut		= false;
	ExprList	init_expr;
	explicit Global(count_string name) : name(name) {}
};

struct Table {
	string		name;
	Limits		elem_limits;
	explicit Table(count_string name) : name(name) {}
};

struct ElemSegment {
	Var			table_var;
	ExprList	offset;
	VarVector	vars;
};

struct Memory {
	string		name;
	Limits		page_limits;
	explicit Memory(count_string name) : name(name) {}
};

struct DataSegment {
	Var			memory_var;
	ExprList	offset;
	dynamic_array<uint8> data;
};

class Import {
protected:
	Import(ExternalKind kind) : kind(kind) {}
public:
	ExternalKind	kind;
	string			module_name;
	string			field_name;
	virtual ~Import() {}
};

template<ExternalKind TypeEnum> class ImportMixin : public Import {
public:
	static bool classof(const Import* import) { return import->kind == TypeEnum; }
	ImportMixin() : Import(TypeEnum) {}
};

class FuncImport : public ImportMixin<ExternalKind::Func> {
public:
	Func	func;
	explicit FuncImport(count_string name = count_string()) : ImportMixin<ExternalKind::Func>(), func(name) {}
};

class TableImport : public ImportMixin<ExternalKind::Table> {
public:
	Table	table;
	explicit TableImport(count_string name = count_string()) : ImportMixin<ExternalKind::Table>(), table(name) {}
};

class MemoryImport : public ImportMixin<ExternalKind::Memory> {
public:
	Memory	memory;
	explicit MemoryImport(count_string name = count_string()) : ImportMixin<ExternalKind::Memory>(), memory(name) {}
};

class GlobalImport : public ImportMixin<ExternalKind::Global> {
public:
	Global	global;
	explicit GlobalImport(count_string name = count_string()) : ImportMixin<ExternalKind::Global>(), global(name) {}
};

class ExceptionImport : public ImportMixin<ExternalKind::Except> {
public:
	Exception except;
	explicit ExceptionImport(count_string name = count_string()) : ImportMixin<ExternalKind::Except>(), except(name) {}
};

struct Export {
	string			name;
	ExternalKind	kind;
	Var				var;
};

class ModuleField : public e_link<ModuleField> {
public:
	enum class Type {
		Func,
		Global,
		Import,
		Export,
		FuncType,
		Table,
		ElemSegment,
		Memory,
		DataSegment,
		Start,
		Except
	};
	Type			type;
	Location		loc;
	ModuleField(Type type, const Location& loc) : type(type), loc(loc) {}
	virtual ~ModuleField() {}
};

typedef e_list<ModuleField> ModuleFieldList;

template<ModuleField::Type TypeEnum> class ModuleFieldMixin : public ModuleField {
public:
	static bool classof(const ModuleField* field) { return field->type == TypeEnum; }
	explicit ModuleFieldMixin(const Location& loc) : ModuleField(TypeEnum, loc) {}
};

class FuncModuleField : public ModuleFieldMixin<ModuleField::Type::Func> {
public:
	Func func;
	explicit FuncModuleField(const Location& loc = Location(), count_string name = count_string()) : ModuleFieldMixin<ModuleField::Type::Func>(loc), func(name) {}
};

class GlobalModuleField : public ModuleFieldMixin<ModuleField::Type::Global> {
public:
	Global global;
	explicit GlobalModuleField(const Location& loc = Location(), count_string name = count_string()) : ModuleFieldMixin<ModuleField::Type::Global>(loc), global(name) {}
};

class ImportModuleField : public ModuleFieldMixin<ModuleField::Type::Import> {
public:
	unique_ptr<Import> import;
	explicit ImportModuleField(const Location& loc = Location()) : ModuleFieldMixin<ModuleField::Type::Import>(loc) {}
	explicit ImportModuleField(Import *import, const Location& loc = Location()) : ModuleFieldMixin<ModuleField::Type::Import>(loc), import(import) {}
};

class ExportModuleField : public ModuleFieldMixin<ModuleField::Type::Export> {
public:
	Export exp;
	explicit ExportModuleField(const Location& loc = Location()) : ModuleFieldMixin<ModuleField::Type::Export>(loc) {}
};

class FuncTypeModuleField : public ModuleFieldMixin<ModuleField::Type::FuncType> {
public:
	FuncType func_type;
	explicit FuncTypeModuleField(const Location& loc = Location(), count_string name = count_string()) : ModuleFieldMixin<ModuleField::Type::FuncType>(loc), func_type(name) {}
};

class TableModuleField : public ModuleFieldMixin<ModuleField::Type::Table> {
public:
	Table table;
	explicit TableModuleField(const Location& loc = Location(), count_string name = count_string()) : ModuleFieldMixin<ModuleField::Type::Table>(loc), table(name) {}
};

class ElemSegmentModuleField : public ModuleFieldMixin<ModuleField::Type::ElemSegment> {
public:
	ElemSegment elem_segment;
	explicit ElemSegmentModuleField(const Location& loc = Location()) : ModuleFieldMixin<ModuleField::Type::ElemSegment>(loc) {}
};

class MemoryModuleField : public ModuleFieldMixin<ModuleField::Type::Memory> {
public:
	Memory memory;
	explicit MemoryModuleField(const Location& loc = Location(), count_string name = count_string()) : ModuleFieldMixin<ModuleField::Type::Memory>(loc), memory(name) {}
};

class DataSegmentModuleField : public ModuleFieldMixin<ModuleField::Type::DataSegment> {
public:
	DataSegment data_segment;
	explicit DataSegmentModuleField(const Location& loc = Location()) : ModuleFieldMixin<ModuleField::Type::DataSegment>(loc) {}
};

class ExceptionModuleField : public ModuleFieldMixin<ModuleField::Type::Except> {
public:
	Exception except;
	explicit ExceptionModuleField(const Location& loc = Location(), count_string name = count_string()) : ModuleFieldMixin<ModuleField::Type::Except>(loc), except(name) {}
};

class StartModuleField : public ModuleFieldMixin<ModuleField::Type::Start> {
public:
	Var start;
	explicit StartModuleField(Var start = Var(), const Location& loc = Location()) : ModuleFieldMixin<ModuleField::Type::Start>(loc), start(start) {}
};

class Module {
	template<typename T> static T* GetEntry(const dynamic_array<T*> &array, Index index) {
		return  index < array.size() ? array[index] : nullptr;
	}
	template<typename T> static void AddBinding(BindingHash &bindings, dynamic_array<T*> &array, T &t, const Location &loc, bool allow_blank_name = false) {
		Index	index	= array.size32();
		array.push_back(&t);
		if (allow_blank_name || t.name)
			bindings[t.name] = Binding(loc, index);
	}
public:
	Location		loc;
	string			name;
	ModuleFieldList fields;

	Index			num_except_imports	= 0;
	Index			num_func_imports	= 0;
	Index			num_table_imports	= 0;
	Index			num_memory_imports	= 0;
	Index			num_global_imports	= 0;

	// Cached for convenience; the pointers are shared with values that are stored in either ModuleField or Import.
	dynamic_array<Exception*>	excepts;
	dynamic_array<Func*>		funcs;
	dynamic_array<Global*>		globals;
	dynamic_array<Import*>		imports;
	dynamic_array<Export*>		exports;
	dynamic_array<FuncType*>	func_types;
	dynamic_array<Table*>		tables;
	dynamic_array<ElemSegment*> elem_segments;
	dynamic_array<Memory*>		memories;
	dynamic_array<DataSegment*> data_segments;
	dynamic_array<Var*>			starts;

	BindingHash		except_bindings;
	BindingHash		func_bindings;
	BindingHash		global_bindings;
	BindingHash		export_bindings;
	BindingHash		func_type_bindings;
	BindingHash		table_bindings;
	BindingHash		memory_bindings;

	Index			GetFuncTypeIndex(const FuncSignature& sig) const {
		for (auto &i : func_types) {
			if (i->sig == sig)
				return func_types.index_of(i);
		}
		return kInvalidIndex;
	}
	Index			GetFuncTypeIndex(const FuncDeclaration& decl) const {
		return decl.has_func_type ? GetFuncTypeIndex(decl.type_var) : GetFuncTypeIndex(decl.sig);
	}

	Index			GetFuncTypeIndex(const Var& var)const	{ return func_type_bindings.FindIndex(var); }
	FuncType*		GetFuncType(const Var& var)				{ return GetEntry(func_types, GetFuncTypeIndex(var)); }
	const FuncType* GetFuncType(const Var& var)		const	{ return const_cast<Module*>(this)->GetFuncType(var); }

	Index			GetFuncIndex(const Var& var)	const	{ return func_bindings.FindIndex(var); }
	Func*			GetFunc(const Var& var)					{ return GetEntry(funcs, GetFuncIndex(var)); }
	const Func*		GetFunc(const Var& var)			const	{ return const_cast<Module*>(this)->GetFunc(var); }

	Index			GetTableIndex(const Var& var)	const	{ return table_bindings.FindIndex(var);	}
	Table*			GetTable(const Var& var)				{ return GetEntry(tables, GetTableIndex(var)); }
	const Table*	GetTable(const Var& var)		const	{ return const_cast<Module*>(this)->GetTable(var); }

	Index			GetMemoryIndex(const Var& var)	const	{ return memory_bindings.FindIndex(var); }
	Memory*			GetMemory(const Var& var)				{ return GetEntry(memories, GetMemoryIndex(var)); }
	const Memory*	GetMemory(const Var& var)		const	{ return const_cast<Module*>(this)->GetMemory(var);	}

	Index			GetGlobalIndex(const Var& var)	const	{ return global_bindings.FindIndex(var); }
	Global*			GetGlobal(const Var& var)				{ return GetEntry(globals, GetGlobalIndex(var));	}
	const Global*	GetGlobal(const Var& var)		const	{ return const_cast<Module*>(this)->GetGlobal(var);	}

	Index			GetExceptIndex(const Var& var)	const	{ return except_bindings.FindIndex(var); }
	Exception*		GetExcept(const Var& var)		const	{ return GetEntry(excepts, GetExceptIndex(var)); }

	const Export*	GetExport(count_string name)	const	{ return GetEntry(exports, export_bindings.FindIndex(name)); }

	bool IsImport(ExternalKind kind, const Var& var) const {
		switch (kind) {
			case ExternalKind::Func:	return GetFuncIndex(var)	< num_func_imports;
			case ExternalKind::Global:	return GetGlobalIndex(var)	< num_global_imports;
			case ExternalKind::Memory:	return GetMemoryIndex(var)	< num_memory_imports;
			case ExternalKind::Table:	return GetTableIndex(var)	< num_table_imports;
			case ExternalKind::Except:	return GetExceptIndex(var)	< num_except_imports;
			default:					return false;
		}
	}
	bool IsImport(const Export& exp) const {
		return IsImport(exp.kind, exp.var);
	}

	// TODO(binji): move this into a builder class?
	void AppendField(DataSegmentModuleField *field) {
		data_segments.push_back(&field->data_segment);
		fields.push_back(field);
	}
	void AppendField(ElemSegmentModuleField *field) {
		elem_segments.push_back(&field->elem_segment);
		fields.push_back(field);
	}
	void AppendField(ExceptionModuleField *field) {
		AddBinding(except_bindings, excepts, field->except, field->loc);
		fields.push_back(field);
	}
	void AppendField(ExportModuleField *field) {
		AddBinding(export_bindings, exports, field->exp, field->loc, true);
		fields.push_back(field);
	}
	void AppendField(FuncModuleField *field) {
		AddBinding(func_bindings, funcs, field->func, field->loc);
		fields.push_back(field);
	}
	void AppendField(FuncTypeModuleField *field) {
		AddBinding(func_type_bindings, func_types, field->func_type, field->loc);
		fields.push_back(field);
	}
	void AppendField(GlobalModuleField *field) {
		AddBinding(global_bindings, globals, field->global, field->loc);
		fields.push_back(field);
	}
	void AppendField(ImportModuleField *field) {
		Import* import = field->import;

		switch (import->kind) {
			case ExternalKind::Func:
				AddBinding(func_bindings, funcs, cast<FuncImport>(import)->func, field->loc);
				++num_func_imports;
				break;
			case ExternalKind::Table:
				AddBinding(table_bindings, tables, cast<TableImport>(import)->table, field->loc);
				++num_table_imports;
				break;
			case ExternalKind::Memory:
				AddBinding(memory_bindings, memories, cast<MemoryImport>(import)->memory, field->loc);
				++num_memory_imports;
				break;
			case ExternalKind::Global:
				AddBinding(global_bindings, globals, cast<GlobalImport>(import)->global, field->loc);
				++num_global_imports;
				break;
			case ExternalKind::Except:
				AddBinding(except_bindings, excepts, cast<ExceptionImport>(import)->except, field->loc);
				++num_except_imports;
				break;
		}

		imports.push_back(import);
		fields.push_back(field);
	}
	void AppendField(MemoryModuleField *field) {
		AddBinding(memory_bindings, memories, field->memory, field->loc);
		fields.push_back(field);
	}
	void AppendField(StartModuleField *field) {
		starts.push_back(&field->start);
		fields.push_back(field);
	}
	void AppendField(TableModuleField *field) {
		AddBinding(table_bindings, tables, field->table, field->loc);
		fields.push_back(field);
	}

	void AppendField(ModuleField *field) {
		switch (field->type) {
			case ModuleField::Type::Func:		AppendField(cast<FuncModuleField>(field));			break;
			case ModuleField::Type::Global:		AppendField(cast<GlobalModuleField>(field));		break;
			case ModuleField::Type::Import:		AppendField(cast<ImportModuleField>(field));		break;
			case ModuleField::Type::Export:		AppendField(cast<ExportModuleField>(field));		break;
			case ModuleField::Type::FuncType:	AppendField(cast<FuncTypeModuleField>(field));		break;
			case ModuleField::Type::Table:		AppendField(cast<TableModuleField>(field));			break;
			case ModuleField::Type::ElemSegment:AppendField(cast<ElemSegmentModuleField>(field));	break;
			case ModuleField::Type::Memory:		AppendField(cast<MemoryModuleField>(field));		break;
			case ModuleField::Type::DataSegment:AppendField(cast<DataSegmentModuleField>(field));	break;
			case ModuleField::Type::Start:		AppendField(cast<StartModuleField>(field));			break;
			case ModuleField::Type::Except:		AppendField(cast<ExceptionModuleField>(field));		break;
		}
	}
	void AppendFields(ModuleFieldList *fields) {
		while (!fields->empty())
			AppendField(fields->pop_front());
	}
};

//-----------------------------------------------------------------------------
//	ScriptModule
//	a module that may not yet be decoded, allowing for text and binary parsing errors to be deferred until validation time
//-----------------------------------------------------------------------------

class ScriptModule {
public:
	enum class Type {
		Text,
		Binary,
		Quoted,
	};
	Type	type;
	explicit ScriptModule(Type type) : type(type) {}
	virtual ~ScriptModule() {}
	virtual const Location& location() const = 0;
};

template<ScriptModule::Type TypeEnum> class ScriptModuleMixin : public ScriptModule {
public:
	static bool classof(const ScriptModule* script_module) { return script_module->type == TypeEnum; }
	ScriptModuleMixin() : ScriptModule(TypeEnum) {}
};

class TextScriptModule : public ScriptModuleMixin<ScriptModule::Type::Text> {
public:
	Module		module;
	const Location& location() const { return module.loc; }
};

template<ScriptModule::Type TypeEnum> class DataScriptModule : public ScriptModuleMixin<TypeEnum> {
public:
	Location	loc;
	string		name;
	dynamic_array<uint8> data;
	const Location& location() const { return loc; }
};

typedef DataScriptModule<ScriptModule::Type::Binary> BinaryScriptModule;
typedef DataScriptModule<ScriptModule::Type::Quoted> QuotedScriptModule;

//-----------------------------------------------------------------------------
//	Action
//-----------------------------------------------------------------------------

class Action {
public:
	enum class Type {
		Invoke,
		Get,
	};
	Type		type;
	Location	loc;
	Var			module_var;
	string		name;

	explicit Action(Type type, const Location& loc = Location()) : loc(loc), type(type) {}
	virtual ~Action() {}
};

typedef unique_ptr<Action> ActionPtr;

template<Action::Type TypeEnum> class ActionMixin : public Action {
public:
	static bool classof(const Action* action) { return action->type == TypeEnum; }
	explicit ActionMixin(const Location& loc = Location()) : Action(TypeEnum, loc) {}
};

class GetAction : public ActionMixin<Action::Type::Get> {
public:
	explicit GetAction(const Location& loc = Location()) : ActionMixin<Action::Type::Get>(loc) {}
};

class InvokeAction : public ActionMixin<Action::Type::Invoke> {
public:
	ConstVector args;
	explicit InvokeAction(const Location& loc = Location()) : ActionMixin<Action::Type::Invoke>(loc) {}
};

//-----------------------------------------------------------------------------
//	Command
//-----------------------------------------------------------------------------

class Command {
public:
	enum class Type {
		Module,
		Action,
		Register,
		AssertMalformed,
		AssertInvalid,
		AssertUnlinkable,
		AssertUninstantiable,
		AssertReturn,
		AssertReturnCanonicalNan,
		AssertReturnArithmeticNan,
		AssertTrap,
		AssertExhaustion,
	};
	Type type;
	explicit Command(Type type) : type(type) {}
	virtual ~Command() {}
};

template<Command::Type TypeEnum> class CommandMixin : public Command {
public:
	static bool classof(const Command* cmd) { return cmd->type == TypeEnum; }
	CommandMixin() : Command(TypeEnum) {}
};

class ModuleCommand : public CommandMixin<Command::Type::Module> {
public:
	Module module;
};

template<Command::Type TypeEnum> class ActionCommandBase : public CommandMixin<TypeEnum> {
public:
	ActionPtr action;
};

typedef ActionCommandBase<Command::Type::Action>					ActionCommand;
typedef ActionCommandBase<Command::Type::AssertReturnCanonicalNan>	AssertReturnCanonicalNanCommand;
typedef ActionCommandBase<Command::Type::AssertReturnArithmeticNan>	AssertReturnArithmeticNanCommand;

class RegisterCommand : public CommandMixin<Command::Type::Register> {
public:
	string	module_name;
	Var		var;
	RegisterCommand(count_string module_name, const Var& var) : module_name(module_name), var(var) {}
};

class AssertReturnCommand : public CommandMixin<Command::Type::AssertReturn> {
public:
	ActionPtr	action;
	ConstVector expected;
};

template<Command::Type TypeEnum> class AssertTrapCommandBase : public CommandMixin<TypeEnum> {
public:
	ActionPtr	action;
	string		text;
};

typedef AssertTrapCommandBase<Command::Type::AssertTrap>			AssertTrapCommand;
typedef AssertTrapCommandBase<Command::Type::AssertExhaustion>		AssertExhaustionCommand;

template<Command::Type TypeEnum> class AssertModuleCommand : public CommandMixin<TypeEnum> {
public:
	unique_ptr<ScriptModule>	module;
	string					text;
};

typedef AssertModuleCommand<Command::Type::AssertMalformed>			AssertMalformedCommand;
typedef AssertModuleCommand<Command::Type::AssertInvalid>			AssertInvalidCommand;
typedef AssertModuleCommand<Command::Type::AssertUnlinkable>		AssertUnlinkableCommand;
typedef AssertModuleCommand<Command::Type::AssertUninstantiable>	AssertUninstantiableCommand;

struct Script {
	dynamic_array<unique_ptr<Command>>	commands;
	BindingHash			module_bindings;

	Module* GetFirstModule() {
		for (auto& command : commands) {
			if (auto* module_command = dyn_cast<ModuleCommand>(command.get()))
				return &module_command->module;
		}
		return nullptr;
	}
	const Module* GetFirstModule() const {
		return const_cast<Script*>(this)->GetFirstModule();
	}
	const Module* GetModule(const Var& var) const {
		Index index = module_bindings.FindIndex(var);
		if (index >= commands.size())
			return nullptr;
		auto* command = cast<ModuleCommand>(commands[index].get());
		return &command->module;
	}
};

//-----------------------------------------------------------------------------
//	BinaryReader
//-----------------------------------------------------------------------------

void MakeTypeBindingReverseMapping(size_t num_types, const BindingHash& bindings, dynamic_array<string>* out_reverse_mapping) {
	out_reverse_mapping->clear();
	out_reverse_mapping->resize(num_types);
	for (const auto& pair : bindings) {
		ISO_ASSERT(pair.b.index < out_reverse_mapping->size());
		(*out_reverse_mapping)[pair.b.index] = pair.a;
	}
}

struct LabelNode {
	LabelType	label_type;
	ExprList*	exprs;
	Expr*		context;
	LabelNode(LabelType label_type, ExprList* exprs, Expr* context = nullptr) : label_type(label_type), exprs(exprs), context(context) {}
};

class BinaryReaderIR {
	const memory_reader*		state;
	Module&						module;
	Func*						current_func		= nullptr;
	dynamic_array<LabelNode>	label_stack;
	ExprList*					current_init_expr	= nullptr;
	const char*					filename;

	static string MakeDollarName(count_string name) {
		return string("$") + name;
	}

	bool HandleError(ErrorLevel, Offset offset, const char* message) {
		//	return error_handler->OnError(error_level, offset, message);
		return true;
	}
	Location GetLocation() const {
		return Location(count_string(filename), state->tell());
	}
	void PrintError(const char* format, ...) {
		//	WABT_SNPRINTF_ALLOCA(buffer, length, format);
		//	HandleError(ErrorLevel::Error, kInvalidOffset, buffer);
	}
	void PushLabel(LabelType label_type, ExprList* first, Expr* context = nullptr) {
		label_stack.emplace_back(label_type, first, context);
	}
	bool PopLabel() {
		if (label_stack.size() == 0) {
			PrintError("popping empty label stack");
			return false;
		}
		label_stack.pop_back();
		return true;
	}
	bool GetLabelAt(LabelNode** label, Index depth) {
		if (depth >= label_stack.size()) {
			//PrintError("accessing stack depth: %" PRIindex " >= max: %" PRIzd, depth, label_stack.size());
			return false;
		}
		*label = &label_stack[label_stack.size() - depth - 1];
		return true;
	}
	bool TopLabel(LabelNode** label) {
		return GetLabelAt(label, 0);
	}
	bool TopLabelExpr(LabelNode** label, Expr** expr) {
		CHECK_RESULT(TopLabel(label));
		LabelNode* parent_label;
		CHECK_RESULT(GetLabelAt(&parent_label, 1));
		*expr = &parent_label->exprs->back();
		return true;
	}
	bool AppendExpr(Expr *expr) {
		expr->loc = GetLocation();
		LabelNode* label;
		CHECK_RESULT(TopLabel(&label));
		label->exprs->push_back(expr);
		return true;
	}
	void SetBlockDeclaration(BlockDeclaration* decl, Type sig_type) {
		if (IsTypeIndex(sig_type)) {
			Index type_index = GetTypeIndex(sig_type);
			decl->has_func_type = true;
			decl->type_var = Var(type_index);
			decl->sig = module.func_types[type_index]->sig;
		} else {
			decl->has_func_type = false;
			decl->sig.param_types.clear();
			decl->sig.result_types = GetInlineTypeVector(sig_type);
		}
	}

public:
	BinaryReaderIR(Module &module, const char* filename) : module(module), filename(filename) {}

	bool OnError(ErrorLevel error_level, const char* message) {
		return HandleError(error_level, state->tell(), message);
	}
	void OnSetState(const memory_reader* s) {
		state = s;
	}
	bool BeginModule(uint32 version)							{ return true; }
	bool EndModule()											{ return true; }

	bool BeginSection(BinarySection section_type, Offset size)	{ return true; }
	bool BeginCustomSection(Offset size, count_string name)		{ return true; }
	bool EndCustomSection()										{ return true; }

	/* Type section */
	bool BeginTypeSection(Offset size)		{ return true; }

	bool OnTypeCount(Index count) {
		module.func_types.reserve(count);
		return true;
	}
	bool OnType(Index index, Index param_count, Type* param_types, Index result_count, Type* result_types) {
		auto field			= new FuncTypeModuleField(GetLocation());
		FuncType& func_type	= field->func_type;
		func_type.sig.param_types.assign(param_types, param_types + param_count);
		func_type.sig.result_types.assign(result_types, result_types + result_count);
		module.AppendField(field);
		return true;
	}
	bool EndTypeSection()					{ return true; }

	/* Import section */
	bool BeginImportSection(Offset size)	{ return true; }
	bool OnImportCount(Index count) {
		module.imports.reserve(count);
		return true;
	}
	bool OnImport(Index index, count_string module_name, count_string field_name)  { return true; }
	bool OnImportFunc(Index import_index, count_string module_name, count_string field_name, Index func_index, Index sig_index) {
		auto import						= new FuncImport();
		import->module_name				= module_name;
		import->field_name				= field_name;
		import->func.decl.has_func_type	= true;
		import->func.decl.type_var		= Var(sig_index, GetLocation());
		import->func.decl.sig			= module.func_types[sig_index]->sig;
		module.AppendField(new ImportModuleField(import, GetLocation()));
		return true;
	}
	bool OnImportTable(Index import_index, count_string module_name, count_string field_name, Index table_index, Type elem_type, const Limits* elem_limits) {
		auto import					= new TableImport();
		import->module_name			= module_name;
		import->field_name			= field_name;
		import->table.elem_limits	= *elem_limits;
		module.AppendField(new ImportModuleField(import, GetLocation()));
		return true;
	}
	bool OnImportMemory(Index import_index, count_string module_name, count_string field_name, Index memory_index, const Limits* page_limits) {
		auto import					= new MemoryImport();
		import->module_name			= module_name;
		import->field_name			= field_name;
		import->memory.page_limits	= *page_limits;
		module.AppendField(new ImportModuleField(import, GetLocation()));
		return true;
	}
	bool OnImportGlobal(Index import_index, count_string module_name, count_string field_name, Index global_index, Type type, bool mut) {
		auto import					= new GlobalImport();
		import->module_name			= module_name;
		import->field_name			= field_name;
		import->global.type			= type;
		import->global.mut			= mut;
		module.AppendField(new ImportModuleField(import, GetLocation()));
		return true;
	}
	bool OnImportException(Index import_index, count_string module_name, count_string field_name, Index except_index, TypeVector& sig) {
		auto import					= new ExceptionImport();
		import->module_name			= module_name;
		import->field_name			= field_name;
		import->except.sig			= sig;
		module.AppendField(new ImportModuleField(import, GetLocation()));
		return true;
	}
	bool EndImportSection()  { return true; }

	/* Function section */
	bool BeginFunctionSection(Offset size) { return true; }
	bool OnFunctionCount(Index count) {
		module.funcs.reserve(module.num_func_imports + count);
		return true;
	}
	bool OnFunction(Index index, Index sig_index) {
		auto field					= new FuncModuleField(GetLocation());
		Func& func					= field->func;
		func.decl.has_func_type		= true;
		func.decl.type_var			= Var(sig_index, GetLocation());
		func.decl.sig				= module.func_types[sig_index]->sig;
		module.AppendField(field);
		return true;
	}
	bool EndFunctionSection() { return true; }

	/* Table section */
	bool BeginTableSection(Offset size) { return true; }
	bool OnTableCount(Index count) {
		module.tables.reserve(module.num_table_imports + count);
		return true;
	}
	bool OnTable(Index index, Type elem_type, const Limits* elem_limits) {
		auto field					= new TableModuleField(GetLocation());
		field->table.elem_limits	= *elem_limits;
		module.AppendField(field);
		return true;
	}
	bool EndTableSection() { return true; }

	/* Memory section */
	bool BeginMemorySection(Offset size) { return true; }
	bool OnMemoryCount(Index count) {
		module.memories.reserve(module.num_memory_imports + count);
		return true;
	}
	bool OnMemory(Index index, const Limits* limits) {
		auto field					= new MemoryModuleField(GetLocation());
		field->memory.page_limits	= *limits;
		module.AppendField(field);
		return true;
	}
	bool EndMemorySection() { return true; }

	/* Global section */
	bool BeginGlobalSection(Offset size) { return true; }
	bool OnGlobalCount(Index count) {
		module.globals.reserve(module.num_global_imports + count);
		return true;
	}
	bool BeginGlobal(Index index, Type type, bool mut) {
		auto field				= new GlobalModuleField(GetLocation());
		Global& global			= field->global;
		global.type				= type;
		global.mut				= mut;
		module.AppendField(field);
		return true;
	}
	bool BeginGlobalInitExpr(Index index) {
		ISO_ASSERT(index == module.globals.size() - 1);
		Global* global		= module.globals[index];
		current_init_expr	= &global->init_expr;
		return true;
	}
	bool EndGlobalInitExpr(Index index) {
		current_init_expr		= nullptr;
		return true;
	}
	bool EndGlobal(Index index)			{ return true; }
	bool EndGlobalSection()				{ return true; }

	/* Exports section */
	bool BeginExportSection(Offset size){ return true; }
	bool OnExportCount(Index count) {
		module.exports.reserve(count);
		return true;
	}
	bool OnExport(Index index, ExternalKind kind, Index item_index, count_string name) {
		auto field			= new ExportModuleField(GetLocation());
		Export& exp		= field->exp;
		exp.name		= name;
		switch (kind) {
			case ExternalKind::Func:	ISO_ASSERT(item_index < module.funcs.size());		break;
			case ExternalKind::Table:	ISO_ASSERT(item_index < module.tables.size());	break;
			case ExternalKind::Memory:	ISO_ASSERT(item_index < module.memories.size());	break;
			case ExternalKind::Global:	ISO_ASSERT(item_index < module.globals.size());	break;
			case ExternalKind::Except:	/* Note: Can't check if index valid, exceptions section comes later.*/	break;
		}
		exp.var			= Var(item_index, GetLocation());
		exp.kind		= kind;
		module.AppendField(field);
		return true;
	}
	bool EndExportSection()				{ return true; }

	/* Start section */
	bool BeginStartSection(Offset size) { return true; }
	bool OnStartFunction(Index func_index) {
		ISO_ASSERT(func_index < module.funcs.size());
		Var start(func_index, GetLocation());
		module.AppendField(new StartModuleField(start, GetLocation()));
		return true;
	}
	bool EndStartSection()				{ return true; }

	/* Code section */
	bool BeginCodeSection(Offset size)	{ return true; }
	bool OnFunctionBodyCount(Index count) {
		ISO_ASSERT(module.num_func_imports + count == module.funcs.size());
		return true;
	}
	bool BeginFunctionBody(Index index) {
		current_func       = module.funcs[index];
		PushLabel(LabelType::Func, &current_func->exprs);
		return true;
	}
	bool OnLocalDeclCount(Index count)	{ return true; }
	bool OnLocalDecl(Index decl_index, Index count, Type type) {
		current_func->local_types.AppendDecl(type, count);
		return true;
	}

	bool OnOpcode(Opcode Opcode)							{ return true; }
	bool OnOpcodeBare()										{ return true; }
	bool OnOpcodeUint32(uint32 value)						{ return true; }
	bool OnOpcodeIndex(Index value)							{ return true; }
	bool OnOpcodeUint32Uint32(uint32 value, uint32 value2)	{ return true; }
	bool OnOpcodeUint64(uint64 value)						{ return true; }
	bool OnOpcodeF32(uint32 value)							{ return true; }
	bool OnOpcodeF64(uint64 value)							{ return true; }
	bool OnOpcodeV128(uint128 value)						{ return true; }
	bool OnOpcodeBlockSig(Type sig_type)					{ return true; }

	bool OnAtomicLoadExpr(Opcode opcode, uint32 alignment_log2, Address offset)			{ return AppendExpr(new AtomicLoadExpr(opcode, 1 << alignment_log2, offset));	}
	bool OnAtomicStoreExpr(Opcode opcode, uint32 alignment_log2, Address offset)		{ return AppendExpr(new AtomicStoreExpr(opcode, 1 << alignment_log2, offset));	}
	bool OnAtomicRmwExpr(Opcode opcode, uint32 alignment_log2, Address offset)			{ return AppendExpr(new AtomicRmwExpr(opcode, 1 << alignment_log2, offset));	}
	bool OnAtomicRmwCmpxchgExpr(Opcode opcode, uint32 alignment_log2, Address offset)	{ return AppendExpr(new AtomicRmwCmpxchgExpr(opcode, 1 << alignment_log2, offset));	}
	bool OnAtomicWaitExpr(Opcode opcode, uint32 alignment_log2, Address offset)			{ return AppendExpr(new AtomicWaitExpr(opcode, 1 << alignment_log2, offset));	}
	bool OnAtomicWakeExpr(Opcode opcode, uint32 alignment_log2, Address offset)			{ return AppendExpr(new AtomicWakeExpr(opcode, 1 << alignment_log2, offset));	}

	bool OnBinaryExpr(Opcode opcode) {
		return AppendExpr(new BinaryExpr(opcode));
	}
	bool OnBlockExpr(Type sig_type) {
		auto expr			= new BlockExpr();
		SetBlockDeclaration(&expr->block.decl, sig_type);
		ExprList* expr_list	= &expr->block.exprs;
		CHECK_RESULT(AppendExpr(expr));
		PushLabel(LabelType::Block, expr_list);
		return true;
	}
	bool OnBrExpr(Index depth) {
		return AppendExpr(new BrExpr(Var(depth)));
	}
	bool OnBrIfExpr(Index depth) {
		return AppendExpr(new BrIfExpr(Var(depth)));
	}
	bool OnBrTableExpr(Index num_targets, Index* target_depths, Index default_target_depth) {
		auto expr			= new BrTableExpr();
		expr->default_target= Var(default_target_depth);
		expr->targets.resize(num_targets);
		for (Index i = 0; i < num_targets; ++i)
			expr->targets[i]= Var(target_depths[i]);
		return AppendExpr(expr);
	}
	bool OnCallExpr(Index func_index) {
		ISO_ASSERT(func_index < module.funcs.size());
		return AppendExpr(new CallExpr(Var(func_index)));
	}
	bool OnCatchExpr() {
		LabelNode* label;
		CHECK_RESULT(TopLabel(&label));
		if (label->label_type != LabelType::Try) {
			PrintError("catch expression without matching try");
			return false;
		}

		LabelNode* parent_label;
		CHECK_RESULT(GetLabelAt(&parent_label, 1));

		label->label_type	= LabelType::Catch;
		label->exprs		= &cast<TryExpr>(&parent_label->exprs->back())->catch_;
		return true;
	}
	bool OnCallIndirectExpr(Index sig_index) {
		ISO_ASSERT(sig_index < module.func_types.size());
		auto expr					= new CallIndirectExpr();
		expr->decl.has_func_type	= true;
		expr->decl.type_var			= Var(sig_index, GetLocation());
		expr->decl.sig				= module.func_types[sig_index]->sig;
		return AppendExpr(expr);
	}
	bool OnCompareExpr(Opcode opcode) {
		return AppendExpr(new CompareExpr(opcode));
	}
	bool OnConvertExpr(Opcode opcode) {
		return AppendExpr(new ConvertExpr(opcode));
	}
	bool OnDropExpr() {
		return AppendExpr(new DropExpr());
	}
	bool OnElseExpr() {
		LabelNode*	label;
		Expr*		expr;
		CHECK_RESULT(TopLabelExpr(&label, &expr));

		if (label->label_type == LabelType::If) {
			auto* if_expr					= cast<IfExpr>(expr);
			if_expr->true_.end_loc			= GetLocation();
			label->exprs					= &if_expr->false_;
			label->label_type				= LabelType::Else;
		} else if (label->label_type == LabelType::IfExcept) {
			auto* if_except_expr			= cast<IfExceptExpr>(expr);
			if_except_expr->true_.end_loc	= GetLocation();
			label->exprs					= &if_except_expr->false_;
			label->label_type				= LabelType::IfExceptElse;
		} else {
			PrintError("else expression without matching if");
			return false;
		}
		return true;
	}
	bool OnEndExpr() {
		LabelNode* label;
		Expr* expr;
		CHECK_RESULT(TopLabelExpr(&label, &expr));
		switch (label->label_type) {
			case LabelType::Block:			cast<BlockExpr>(expr)->block.end_loc	= GetLocation();	break;
			case LabelType::Loop:			cast<LoopExpr>(expr)->block.end_loc		= GetLocation();	break;
			case LabelType::If:				cast<IfExpr>(expr)->true_.end_loc		= GetLocation();	break;
			case LabelType::Else:			cast<IfExpr>(expr)->false_end_loc		= GetLocation();	break;
			case LabelType::IfExcept:		cast<IfExceptExpr>(expr)->true_.end_loc	= GetLocation();	break;
			case LabelType::IfExceptElse:	cast<IfExceptExpr>(expr)->false_end_loc	= GetLocation();	break;
			case LabelType::Try:			cast<TryExpr>(expr)->block.end_loc		= GetLocation();	break;
			case LabelType::Func:
			case LabelType::Catch:			break;
		}

		return PopLabel();
	}
	bool OnEndFunc()							{ return true; }
	bool OnF32ConstExpr(uint32 value_bits)		{ return AppendExpr(new ConstExpr(Const::F32(value_bits, GetLocation())));	}
	bool OnF64ConstExpr(uint64 value_bits)		{ return AppendExpr(new ConstExpr(Const::F64(value_bits, GetLocation())));	}
	bool OnV128ConstExpr(uint128 value_bits)	{ return AppendExpr(new ConstExpr(Const::V128(value_bits, GetLocation())));	}
	bool OnGetGlobalExpr(Index global_index)	{ return AppendExpr(new GetGlobalExpr(Var(global_index, GetLocation())));	}
	bool OnGetLocalExpr(Index local_index)		{ return AppendExpr(new GetLocalExpr(Var(local_index, GetLocation())));	}
	bool OnI32ConstExpr(uint32 value)			{ return AppendExpr(new ConstExpr(Const::I32(value, GetLocation())));	}
	bool OnI64ConstExpr(uint64 value)			{ return AppendExpr(new ConstExpr(Const::I64(value, GetLocation())));	}
	bool OnIfExpr(Type sig_type) {
		auto expr			= new IfExpr();
		SetBlockDeclaration(&expr->true_.decl, sig_type);
		ExprList* expr_list = &expr->true_.exprs;
		CHECK_RESULT(AppendExpr(expr));
		PushLabel(LabelType::If, expr_list);
		return true;
	}
	bool OnIfExceptExpr(Type sig_type, Index except_index) {
		auto expr			= new IfExceptExpr();
		expr->except_var	= Var(except_index, GetLocation());
		SetBlockDeclaration(&expr->true_.decl, sig_type);
		ExprList* expr_list = &expr->true_.exprs;
		CHECK_RESULT(AppendExpr(expr));
		PushLabel(LabelType::IfExcept, expr_list);
		return true;
	}
	bool OnLoadExpr(Opcode opcode, uint32 alignment_log2, Address offset) {
		return AppendExpr(new LoadExpr(opcode, 1 << alignment_log2, offset));
	}
	bool OnLoopExpr(Type sig_type) {
		auto expr			= new LoopExpr();
		SetBlockDeclaration(&expr->block.decl, sig_type);
		ExprList* expr_list = &expr->block.exprs;
		CHECK_RESULT(AppendExpr(expr));
		PushLabel(LabelType::Loop, expr_list);
		return true;
	}
	bool OnMemoryGrowExpr()						{ return AppendExpr(new MemoryGrowExpr());	}
	bool OnMemorySizeExpr()						{ return AppendExpr(new MemorySizeExpr());	}
	bool OnNopExpr()							{ return AppendExpr(new NopExpr());	}
	bool OnRethrowExpr()						{ return AppendExpr(new RethrowExpr());	}
	bool OnReturnExpr()							{ return AppendExpr(new ReturnExpr());	}
	bool OnSelectExpr()							{ return AppendExpr(new SelectExpr());	}
	bool OnSetGlobalExpr(Index global_index)	{ return AppendExpr(new SetGlobalExpr(Var(global_index, GetLocation())));	}
	bool OnSetLocalExpr(Index local_index)		{ return AppendExpr(new SetLocalExpr(Var(local_index, GetLocation())));	}
	bool OnStoreExpr(Opcode opcode, uint32 alignment_log2, Address offset)	{ return AppendExpr(new StoreExpr(opcode, 1 << alignment_log2, offset));	}
	bool OnThrowExpr(Index except_index)		{ return AppendExpr(new ThrowExpr(Var(except_index, GetLocation())));	}
	bool OnTeeLocalExpr(Index local_index)		{ return AppendExpr(new TeeLocalExpr(Var(local_index, GetLocation())));	}
	bool OnTryExpr(Type sig_type) {
		auto		expr		= new TryExpr;
		ExprList*	expr_list	= &expr->block.exprs;
		SetBlockDeclaration(&expr->block.decl, sig_type);
		CHECK_RESULT(AppendExpr(expr));
		PushLabel(LabelType::Try, expr_list, expr);
		return true;
	}
	bool OnUnaryExpr(Opcode opcode)				{ return AppendExpr(new UnaryExpr(opcode));	}
	bool OnTernaryExpr(Opcode opcode)			{ return AppendExpr(new TernaryExpr(opcode));	}
	bool OnUnreachableExpr()					{ return AppendExpr(new UnreachableExpr());	}
	bool EndFunctionBody(Index index) {
		CHECK_RESULT(PopLabel());
		current_func	= nullptr;
		return true;
	}
	bool EndCodeSection()					{ return true;	}

	bool OnSimdLaneOpExpr(Opcode opcode, uint64 value)		{ return AppendExpr(new SimdLaneOpExpr(opcode, value));	}
	bool OnSimdShuffleOpExpr(Opcode opcode, uint128 value)	{ return AppendExpr(new SimdShuffleOpExpr(opcode, value));	}

	/* Elem section */
	bool BeginElemSection(Offset size)		{ return true; }
	bool OnElemSegmentCount(Index count)	{ module.elem_segments.reserve(count); return true; }
	bool BeginElemSegment(Index index, Index table_index) {
		auto field					= new ElemSegmentModuleField(GetLocation());
		ElemSegment& elem_segment	= field->elem_segment;
		elem_segment.table_var		= Var(table_index, GetLocation());
		module.AppendField(field);
		return true;
	}
	bool BeginElemSegmentInitExpr(Index index) {
		ISO_ASSERT(index == module.elem_segments.size() - 1);
		ElemSegment* segment= module.elem_segments[index];
		current_init_expr	= &segment->offset;
		return true;
	}
	bool EndElemSegmentInitExpr(Index index) {
		current_init_expr	= nullptr;
		return true;
	}
	bool OnElemSegmentFunctionIndexCount(Index index, Index count) {
		ISO_ASSERT(index == module.elem_segments.size() - 1);
		ElemSegment* segment= module.elem_segments[index];
		segment->vars.reserve(count);
		return true;
	}
	bool OnElemSegmentFunctionIndex(Index segment_index, Index func_index) {
		ISO_ASSERT(segment_index == module.elem_segments.size() - 1);
		ElemSegment* segment= module.elem_segments[segment_index];
		segment->vars.emplace_back();
		Var* var			= &segment->vars.back();
		*var				= Var(func_index, GetLocation());
		return true;
	}
	bool EndElemSegment(Index index)		{ return true; }
	bool EndElemSection()					{ return true; }

	/* Data section */
	bool BeginDataSection(Offset size)		{ return true; }
	bool OnDataSegmentCount(Index count)	{ module.data_segments.reserve(count); return true; }
	bool BeginDataSegment(Index index, Index memory_index) {
		auto field					= new DataSegmentModuleField(GetLocation());
		DataSegment& data_segment	= field->data_segment;
		data_segment.memory_var		= Var(memory_index, GetLocation());
		module.AppendField(field);
		return true;
	}
	bool BeginDataSegmentInitExpr(Index index) {
		ISO_ASSERT(index == module.data_segments.size() - 1);
		DataSegment* segment	= module.data_segments[index];
		current_init_expr		= &segment->offset;
		return true;
	}
	bool EndDataSegmentInitExpr(Index index) { current_init_expr = nullptr; return true; }
	bool OnDataSegmentData(Index index, const void* data, Address size) {
		ISO_ASSERT(index == module.data_segments.size() - 1);
		DataSegment* segment= module.data_segments[index];
		segment->data.resize(size);
		if (size > 0)
			memcpy(segment->data, data, size);
		return true;
	}
	bool EndDataSegment(Index index)	{ return true; }
	bool EndDataSection()				{ return true; }

	/* Names section */
	bool BeginNamesSection(Offset size) { return true; }
	bool OnModuleNameSubsection(Index index, uint32 name_type, Offset subsection_size)	{ return true; }

	bool OnModuleName(count_string name) {
		if (!name.empty())
			module.name	= MakeDollarName(name);
		return true;
	}
	bool OnFunctionNameSubsection(Index index, uint32 name_type, Offset subsection_size) { return true; }
	bool OnFunctionNamesCount(Index count) {
		if (count > module.funcs.size()) {
			//PrintError("expected function name count (%" PRIindex ") <= function count (%" PRIzd ")", count, module.funcs.size());
			return false;
		}
		return true;
	}
	bool OnFunctionName(Index index, count_string name) {
		if (name.empty())
			return true;
		Func* func			= module.funcs[index];
		string dollar_name	= MakeDollarName(name);
		int counter			= 1;
		string orig_name	= dollar_name;
		while (module.func_bindings.count(dollar_name) != 0)
			dollar_name		= orig_name + "." + to_string(counter++);
		func->name			= dollar_name;
		module.func_bindings[dollar_name] = Binding(index);
		return true;
	}
	bool OnLocalNameSubsection(Index index, uint32 name_type, Offset subsection_size) { return true; }
	bool OnLocalNameFunctionCount(Index num_functions) { return true; }
	bool OnLocalNameLocalCount(Index index, Index count) {
		ISO_ASSERT(index < module.funcs.size());
		Func* func			= module.funcs[index];
		Index num_params_and_locals = func->GetNumParamsAndLocals();
		if (count > num_params_and_locals) {
			//PrintError("expected local name count (%" PRIindex ") <= local count (%" PRIindex ")", count, num_params_and_locals);
			return false;
		}
		return true;
	}
	bool OnLocalName(Index func_index, Index local_index, count_string name) {
		if (name.empty())
			return true;

		Func* func			= module.funcs[func_index];
		Index num_params	= func->GetNumParams();
		BindingHash* bindings;
		Index index;
		if (local_index < num_params) {
			/* param name */
			bindings		= &func->param_bindings;
			index			= local_index;
		} else {
			/* local name */
			bindings		= &func->local_bindings;
			index			= local_index - num_params;
		}
		(*bindings)[MakeDollarName(name)]	= Binding(index);
		return true;
	}
	bool EndNamesSection()					{ return true; }

	/* Reloc section */
	bool BeginRelocSection(Offset size)		{ return true; }
	bool OnRelocCount(Index count, Index section_index) { return true; }
	bool OnReloc(Reloc::Type type, Offset offset, Index index, uint32 addend) { return true; }
	bool EndRelocSection()					{ return true; }

	/* Linking section */
	bool BeginLinkingSection(Offset size)	{ return true; }
	bool OnSymbolCount(Index count)			{ return true; }
	bool OnSymbol(Index index, Symbol::Type type, uint32 flags) { return true; }
	bool OnDataSymbol(Index index,uint32 flags,count_string name,Index segment,uint32 offset,uint32 size) { return true; }
	bool OnFunctionSymbol(Index index,uint32 flags,count_string name,Index function_index) { return true; }
	bool OnGlobalSymbol(Index index,uint32 flags,count_string name,Index global_index) { return true; }
	bool OnSectionSymbol(Index index,uint32 flags,Index section_index) { return true; }
	bool OnSegmentInfoCount(Index count)	{ return true; }
	bool OnSegmentInfo(Index index,count_string name,uint32 alignment,uint32 flags) { return true; }
	bool OnInitFunctionCount(Index count)	{ return true; }
	bool OnInitFunction(uint32 priority, Index function_index) { return true; }
	bool EndLinkingSection()				{ return true; }

	/* Exception section */
	bool BeginExceptionSection(Offset size) { return true; }
	bool OnExceptionCount(Index count)		{ return true; }
	bool OnExceptionType(Index index, TypeVector& sig) {
		auto field			= new ExceptionModuleField(GetLocation());
		Exception& except	= field->except;
		except.sig			= sig;
		module.AppendField(field);
		return true;
	}
	bool EndExceptionSection()				{ return true; }

	// InitExpr - used by elem, data and global sections; these functions are only called between calls to Begin*InitExpr and End*InitExpr

	bool OnInitExprF32ConstExpr(Index index, uint32 value) {
		Location loc	= GetLocation();
		current_init_expr->push_back(new ConstExpr(Const::F32(value, loc), loc));
		return true;
	}
	bool OnInitExprF64ConstExpr(Index index, uint64 value) {
		Location loc	= GetLocation();
		current_init_expr->push_back(new ConstExpr(Const::F64(value, loc), loc));
		return true;
	}
	bool OnInitExprV128ConstExpr(Index index, uint128 value) {
		Location loc	= GetLocation();
		current_init_expr->push_back(new ConstExpr(Const::V128(value, loc), loc));
		return true;
	}
	bool OnInitExprGetGlobalExpr(Index index, Index global_index) {
		Location loc	= GetLocation();
		current_init_expr->push_back(new GetGlobalExpr(Var(global_index, loc), loc));
		return true;
	}
	bool OnInitExprI32ConstExpr(Index index, uint32 value) {
		Location loc	= GetLocation();
		current_init_expr->push_back(new ConstExpr(Const::I32(value, loc), loc));
		return true;
	}
	bool OnInitExprI64ConstExpr(Index index, uint64 value) {
		Location loc	= GetLocation();
		current_init_expr->push_back(new ConstExpr(Const::I64(value, loc), loc));
		return true;
	}
};

#undef CALLBACK
#define ERROR_UNLESS(expr, ...)				do { if (!(expr)) { PrintError(__VA_ARGS__); return false; } } while (0)
#define CALLBACK0(member)					ERROR_UNLESS(delegate.member(), #member " callback failed")
#define CALLBACK(member, ...)				ERROR_UNLESS(delegate.member(__VA_ARGS__), #member " callback failed")

#define WABT_BINARY_MAGIC					0x6d736100
#define WABT_BINARY_VERSION					1
#define WABT_BINARY_LIMITS_HAS_MAX_FLAG		0x1
#define WABT_BINARY_LIMITS_IS_SHARED_FLAG	0x2

#define WABT_BINARY_SECTION_NAME			"name"
#define WABT_BINARY_SECTION_RELOC			"reloc"
#define WABT_BINARY_SECTION_LINKING			"linking"
#define WABT_BINARY_SECTION_EXCEPTION		"exception"

typedef BinaryReaderIR	BinaryReaderDelegate;

class BinaryReader {
	BinaryReaderDelegate	&delegate;

	memory_reader	r;
	Features		features;
	size_t			read_end;
	TypeVector		param_types;
	TypeVector		result_types;
	dynamic_array<Index> target_depths;
	bool			did_read_names_section	= false;
	Index			num_signatures			= 0;
	Index			num_imports				= 0;
	Index			num_func_imports		= 0;
	Index			num_table_imports		= 0;
	Index			num_memory_imports		= 0;
	Index			num_global_imports		= 0;
	Index			num_exception_imports	= 0;
	Index			num_function_signatures	= 0;
	Index			num_tables				= 0;
	Index			num_memories			= 0;
	Index			num_globals				= 0;
	Index			num_exports				= 0;
	Index			num_function_bodies		= 0;
	Index			num_exceptions			= 0;

	void	PrintError(const char* format, ...) {
	/*	ErrorLevel error_level =
			reading_custom_section && !options->fail_on_custom_section_error
			? ErrorLevel::Warning
			: ErrorLevel::Error;

		WABT_SNPRINTF_ALLOCA(buffer, length, format);
		bool handled = delegate.OnError(error_level, buffer);

		if (!handled) {
			// Not great to just print, but we don't want to eat the error either.
			fprintf(stderr, "%07" PRIzx ": %s: %s\n", r.offset, GetErrorLevelName(error_level), buffer);
		}
		*/
	}
	bool	ReadOpcode(Opcode &out_value) {
		uint8 value = 0;
		if (!r.read(value))
			return false;

		if (Opcode::IsPrefixByte(value)) {
			uint32 code;
			if (!read_leb128(r, code))
				return false;
			out_value = Opcode::FromCode(value, code);
		} else {
			out_value = Opcode::FromCode(value);
		}
		return out_value.IsEnabled(features);
	}
	bool ReadType(Type* out_value) {
		return read_leb128(r, *out_value);
	}
	bool ReadStr(count_string* out_str) {
		uint32 str_len = 0;
		if (!read_leb128(r, str_len))
			return false;
		*out_str = count_string(r.get_ptr<const char>(str_len), str_len);
		return true;
	}
	bool ReadBytes(const void** out_data, Address* out_data_size) {
		uint32 data_size = 0;
		if (!read_leb128(r, data_size))
			return false;
		*out_data		= r.get_ptr<const uint8>(data_size);
		*out_data_size	= data_size;
		return true;
	}
	bool ReadIndex(Index* index) {
		return read_leb128(r, *index);
	}
	bool ReadOffset(Offset* offset) {
		uint32 value;
		if (!read_leb128(r, value))
			return false;
		*offset = value;
		return true;
	}
	bool	ReadCount(Index* count) {
		return ReadIndex(count) && *count <= read_end - r.tell();
	}
	bool	IsConcreteType(Type type) const {
		switch (type) {
			case Type::I32:	case Type::I64:	case Type::F32:	case Type::F64:
				return true;
			case Type::V128:
				return features.simd;
			default:
				return false;
		}
	}
	bool	IsBlockType(Type type) const {
		if (IsConcreteType(type) || type == Type::Void)
			return true;
		if (!(features.multi_value && IsTypeIndex(type)))
			return false;
		return GetTypeIndex(type) < num_signatures;
	}

	Index	NumTotalFuncs()		const	{ return num_func_imports + num_function_signatures;}
	Index	NumTotalTables()	const	{ return num_table_imports + num_tables;}
	Index	NumTotalMemories()	const	{ return num_memory_imports + num_memories;}
	Index	NumTotalGlobals()	const	{ return num_global_imports + num_globals; }

	bool	ReadI32InitExpr(Index index)	{ return ReadInitExpr(index, true); }
	bool	ReadInitExpr(Index index, bool require_i32 = false);
	bool	ReadTable(Type* out_elem_type, Limits* out_elem_limits);
	bool	ReadMemory(Limits* out_page_limits);
	bool	ReadGlobalHeader(Type* out_type, bool* out_mutable);
	bool	ReadExceptionType(TypeVector& sig);
	bool	ReadFunctionBody(Offset end_offset);
	bool	ReadNameSection(Offset section_size);
	bool	ReadRelocSection(Offset section_size);
	bool	ReadLinkingSection(Offset section_size);
	bool	ReadCustomSection(Offset section_size);
	bool	ReadTypeSection(Offset section_size);
	bool	ReadImportSection(Offset section_size);
	bool	ReadFunctionSection(Offset section_size);
	bool	ReadTableSection(Offset section_size);
	bool	ReadMemorySection(Offset section_size);
	bool	ReadGlobalSection(Offset section_size);
	bool	ReadExportSection(Offset section_size);
	bool	ReadStartSection(Offset section_size);
	bool	ReadElemSection(Offset section_size);
	bool	ReadCodeSection(Offset section_size);
	bool	ReadDataSection(Offset section_size);
	bool	ReadExceptionSection(Offset section_size);
	bool	ReadSections();

public:
	BinaryReader(memory_reader r, BinaryReaderIR &delegate) : delegate(delegate), r(r), read_end(r.length()) {
		delegate.OnSetState(&r);
	}

	bool ReadModule() {
		uint32 magic = 0;
		CHECK_RESULT(r.read(magic));
		ERROR_UNLESS(magic == WABT_BINARY_MAGIC, "bad magic value");
		uint32 version = 0;
		CHECK_RESULT(r.read(version));
		ERROR_UNLESS(version == WABT_BINARY_VERSION, "bad wasm file version");

		CALLBACK(BeginModule, version);
		CHECK_RESULT(ReadSections());
		CALLBACK0(EndModule);

		return true;
	}
};

bool BinaryReader::ReadInitExpr(Index index, bool require_i32) {
	Opcode opcode;
	CHECK_RESULT(ReadOpcode(opcode));

	switch (opcode) {
		case Opcode::I32Const: {
			int32 value = 0;
			CHECK_RESULT(read_leb128(r, value));
			CALLBACK(OnInitExprI32ConstExpr, index, value);
			break;
		}
		case Opcode::I64Const: {
			int64 value = 0;
			CHECK_RESULT(read_leb128(r, value));
			CALLBACK(OnInitExprI64ConstExpr, index, value);
			break;
		}
		case Opcode::F32Const: {
			uint32 value_bits = 0;
			CHECK_RESULT(r.read(value_bits));
			CALLBACK(OnInitExprF32ConstExpr, index, value_bits);
			break;
		}
		case Opcode::F64Const: {
			uint64 value_bits = 0;
			CHECK_RESULT(r.read(value_bits));
			CALLBACK(OnInitExprF64ConstExpr, index, value_bits);
			break;
		}
		case Opcode::V128Const: {
			uint128 value_bits;
			clear(value_bits);
			CHECK_RESULT(r.read(value_bits));
			CALLBACK(OnInitExprV128ConstExpr, index, value_bits);
			break;
		}
		case Opcode::GetGlobal: {
			Index global_index;
			CHECK_RESULT(ReadIndex(&global_index));
			CALLBACK(OnInitExprGetGlobalExpr, index, global_index);
			break;
		}
		case Opcode::End:
			return true;

		default:
			return false;//Unexpected Opcode in initializer expression
	}

	if (require_i32 && opcode != Opcode::I32Const && opcode != Opcode::GetGlobal) {
		PrintError("expected i32 init_expr");
		return false;
	}

	CHECK_RESULT(ReadOpcode(opcode));
	ERROR_UNLESS(opcode == Opcode::End, "expected END opcode after initializer expression");
	return true;
}

bool BinaryReader::ReadTable(Type* out_elem_type, Limits* out_elem_limits) {
	CHECK_RESULT(ReadType(out_elem_type));
	ERROR_UNLESS(*out_elem_type == Type::Anyfunc, "table elem type must by anyfunc");

	uint32	flags, initial, max = 0;
	CHECK_RESULT(read_leb128(r, flags));
	CHECK_RESULT(read_leb128(r, initial));
	bool	has_max		= flags & WABT_BINARY_LIMITS_HAS_MAX_FLAG;
	bool	is_shared	= flags & WABT_BINARY_LIMITS_IS_SHARED_FLAG;
	ERROR_UNLESS(!is_shared, "tables may not be shared");
	if (has_max) {
		CHECK_RESULT(read_leb128(r, max));
		ERROR_UNLESS(initial <= max, "table initial elem count must be <= max elem count");
	}

	out_elem_limits->has_max	= has_max;
	out_elem_limits->initial	= initial;
	out_elem_limits->max		= max;
	return true;
}

bool BinaryReader::ReadMemory(Limits* out_page_limits) {
	uint32	flags;
	uint32	initial;
	uint32	max		= 0;
	CHECK_RESULT(read_leb128(r, flags));
	CHECK_RESULT(read_leb128(r, initial));
	ERROR_UNLESS(initial <= WABT_MAX_PAGES, "invalid memory initial size");
	bool	has_max		= flags & WABT_BINARY_LIMITS_HAS_MAX_FLAG;
	bool	is_shared	= flags & WABT_BINARY_LIMITS_IS_SHARED_FLAG;
	ERROR_UNLESS(!is_shared || has_max, "shared memory must have a max size");
	if (has_max) {
		CHECK_RESULT(read_leb128(r, max));
		ERROR_UNLESS(max <= WABT_MAX_PAGES, "invalid memory max size");
		ERROR_UNLESS(initial <= max, "memory initial size must be <= max size");
	}

	out_page_limits->has_max	= has_max;
	out_page_limits->is_shared	= is_shared;
	out_page_limits->initial	= initial;
	out_page_limits->max		= max;
	return true;
}

bool BinaryReader::ReadGlobalHeader(Type* out_type, bool* out_mutable) {
	Type	global_type = Type::Void;
	uint8	mut	= 0;
	CHECK_RESULT(ReadType(&global_type));
	ERROR_UNLESS(IsConcreteType(global_type), "invalid global type");

	CHECK_RESULT(r.read(mut));
	ERROR_UNLESS(mut <= 1, "global mutability must be 0 or 1");

	*out_type		= global_type;
	*out_mutable	= mut;
	return true;
}

bool BinaryReader::ReadFunctionBody(Offset end_offset) {
	bool seen_end_opcode = false;
	while (r.tell() < end_offset) {
		Opcode opcode;
		CHECK_RESULT(ReadOpcode(opcode));
		CALLBACK(OnOpcode, opcode);

		switch (opcode) {
			case Opcode::Unreachable:
				CALLBACK0(OnUnreachableExpr);
				CALLBACK0(OnOpcodeBare);
				break;

			case Opcode::Block: {
				Type sig_type;
				CHECK_RESULT(ReadType(&sig_type));
				ERROR_UNLESS(IsBlockType(sig_type), "expected valid block signature type");
				CALLBACK(OnBlockExpr, sig_type);
				CALLBACK(OnOpcodeBlockSig, sig_type);
				break;
			}
			case Opcode::Loop: {
				Type sig_type;
				CHECK_RESULT(ReadType(&sig_type));
				ERROR_UNLESS(IsBlockType(sig_type), "expected valid block signature type");
				CALLBACK(OnLoopExpr, sig_type);
				CALLBACK(OnOpcodeBlockSig, sig_type);
				break;
			}
			case Opcode::If: {
				Type sig_type;
				CHECK_RESULT(ReadType(&sig_type));
				ERROR_UNLESS(IsBlockType(sig_type), "expected valid block signature type");
				CALLBACK(OnIfExpr, sig_type);
				CALLBACK(OnOpcodeBlockSig, sig_type);
				break;
			}
			case Opcode::Else:
				CALLBACK0(OnElseExpr);
				CALLBACK0(OnOpcodeBare);
				break;

			case Opcode::Select:
				CALLBACK0(OnSelectExpr);
				CALLBACK0(OnOpcodeBare);
				break;

			case Opcode::Br: {
				Index depth;
				CHECK_RESULT(ReadIndex(&depth));
				CALLBACK(OnBrExpr, depth);
				CALLBACK(OnOpcodeIndex, depth);
				break;
			}
			case Opcode::BrIf: {
				Index depth;
				CHECK_RESULT(ReadIndex(&depth));
				CALLBACK(OnBrIfExpr, depth);
				CALLBACK(OnOpcodeIndex, depth);
				break;
			}
			case Opcode::BrTable: {
				Index num_targets;
				CHECK_RESULT(ReadIndex(&num_targets));
				target_depths.resize(num_targets);

				for (Index i = 0; i < num_targets; ++i)
					CHECK_RESULT(ReadIndex(&target_depths[i]));

				Index default_target_depth;
				CHECK_RESULT(ReadIndex(&default_target_depth));
				Index* depths = num_targets ? target_depths.begin() : nullptr;
				CALLBACK(OnBrTableExpr, num_targets, depths, default_target_depth);
				break;
			}
			case Opcode::Return:
				CALLBACK0(OnReturnExpr);
				CALLBACK0(OnOpcodeBare);
				break;

			case Opcode::Nop:
				CALLBACK0(OnNopExpr);
				CALLBACK0(OnOpcodeBare);
				break;

			case Opcode::Drop:
				CALLBACK0(OnDropExpr);
				CALLBACK0(OnOpcodeBare);
				break;

			case Opcode::End:
				if (r.tell() == end_offset) {
					seen_end_opcode = true;
					CALLBACK0(OnEndFunc);
				} else {
					CALLBACK0(OnEndExpr);
				}
				break;

			case Opcode::I32Const: {
				int32 value;
				CHECK_RESULT(read_leb128(r, value));
				CALLBACK(OnI32ConstExpr, value);
				CALLBACK(OnOpcodeUint32, value);
				break;
			}
			case Opcode::I64Const: {
				int64 value;
				CHECK_RESULT(read_leb128(r, value));
				CALLBACK(OnI64ConstExpr, value);
				CALLBACK(OnOpcodeUint64, value);
				break;
			}
			case Opcode::F32Const: {
				uint32 value_bits = 0;
				CHECK_RESULT(r.read(value_bits));
				CALLBACK(OnF32ConstExpr, value_bits);
				CALLBACK(OnOpcodeF32, value_bits);
				break;
			}
			case Opcode::F64Const: {
				uint64 value_bits = 0;
				CHECK_RESULT(r.read(value_bits));
				CALLBACK(OnF64ConstExpr, value_bits);
				CALLBACK(OnOpcodeF64, value_bits);
				break;
			}
			case Opcode::V128Const: {
				uint128 value_bits;
				clear(value_bits);
				CHECK_RESULT(r.read(value_bits));
				CALLBACK(OnV128ConstExpr, value_bits);
				CALLBACK(OnOpcodeV128, value_bits);
				break;
			}
			case Opcode::GetGlobal: {
				Index global_index;
				CHECK_RESULT(ReadIndex(&global_index));
				CALLBACK(OnGetGlobalExpr, global_index);
				CALLBACK(OnOpcodeIndex, global_index);
				break;
			}
			case Opcode::GetLocal: {
				Index local_index;
				CHECK_RESULT(ReadIndex(&local_index));
				CALLBACK(OnGetLocalExpr, local_index);
				CALLBACK(OnOpcodeIndex, local_index);
				break;
			}
			case Opcode::SetGlobal: {
				Index global_index;
				CHECK_RESULT(ReadIndex(&global_index));
				CALLBACK(OnSetGlobalExpr, global_index);
				CALLBACK(OnOpcodeIndex, global_index);
				break;
			}
			case Opcode::SetLocal: {
				Index local_index;
				CHECK_RESULT(ReadIndex(&local_index));
				CALLBACK(OnSetLocalExpr, local_index);
				CALLBACK(OnOpcodeIndex, local_index);
				break;
			}
			case Opcode::Call: {
				Index func_index;
				CHECK_RESULT(ReadIndex(&func_index));
				ERROR_UNLESS(func_index < NumTotalFuncs(), "invalid call function index");
				CALLBACK(OnCallExpr, func_index);
				CALLBACK(OnOpcodeIndex, func_index);
				break;
			}
			case Opcode::CallIndirect: {
				Index sig_index;
				CHECK_RESULT(ReadIndex(&sig_index));
				ERROR_UNLESS(sig_index < num_signatures, "invalid call_indirect signature index");
				uint32 reserved;
				CHECK_RESULT(read_leb128(r, reserved));
				ERROR_UNLESS(reserved == 0, "call_indirect reserved value must be 0");
				CALLBACK(OnCallIndirectExpr, sig_index);
				CALLBACK(OnOpcodeUint32Uint32, sig_index, reserved);
				break;
			}
			case Opcode::TeeLocal: {
				Index local_index;
				CHECK_RESULT(ReadIndex(&local_index));
				CALLBACK(OnTeeLocalExpr, local_index);
				CALLBACK(OnOpcodeIndex, local_index);
				break;
			}
			case Opcode::I32Load8S:			case Opcode::I32Load8U:			case Opcode::I32Load16S:		case Opcode::I32Load16U:
			case Opcode::I64Load8S:			case Opcode::I64Load8U:			case Opcode::I64Load16S:		case Opcode::I64Load16U:
			case Opcode::I64Load32S:		case Opcode::I64Load32U:		case Opcode::I32Load:			case Opcode::I64Load:
			case Opcode::F32Load:			case Opcode::F64Load:			case Opcode::V128Load:
			{
				uint32 alignment_log2;
				CHECK_RESULT(read_leb128(r, alignment_log2));
				Address offset;
				CHECK_RESULT(read_leb128(r, offset));
				CALLBACK(OnLoadExpr, opcode, alignment_log2, offset);
				CALLBACK(OnOpcodeUint32Uint32, alignment_log2, offset);
				break;
			}
			case Opcode::I32Store8:			case Opcode::I32Store16:		case Opcode::I64Store8:			case Opcode::I64Store16:
			case Opcode::I64Store32:		case Opcode::I32Store:			case Opcode::I64Store:			case Opcode::F32Store:
			case Opcode::F64Store:			case Opcode::V128Store:
			{
				uint32 alignment_log2;
				CHECK_RESULT(read_leb128(r, alignment_log2));
				Address offset;
				CHECK_RESULT(read_leb128(r, offset));
				CALLBACK(OnStoreExpr, opcode, alignment_log2, offset);
				CALLBACK(OnOpcodeUint32Uint32, alignment_log2, offset);
				break;
			}
			case Opcode::MemorySize: {
				uint32 reserved;
				CHECK_RESULT(read_leb128(r, reserved));
				ERROR_UNLESS(reserved == 0, "memory.size reserved value must be 0");
				CALLBACK0(OnMemorySizeExpr);
				CALLBACK(OnOpcodeUint32, reserved);
				break;
			}
			case Opcode::MemoryGrow: {
				uint32 reserved;
				CHECK_RESULT(read_leb128(r, reserved));
				ERROR_UNLESS(reserved == 0, "memory.grow reserved value must be 0");
				CALLBACK0(OnMemoryGrowExpr);
				CALLBACK(OnOpcodeUint32, reserved);
				break;
			}
			case Opcode::I32Add:			case Opcode::I32Sub:			case Opcode::I32Mul:			case Opcode::I32DivS:	case Opcode::I32DivU:	case Opcode::I32RemS:	case Opcode::I32RemU:
			case Opcode::I32And:			case Opcode::I32Or:				case Opcode::I32Xor:			case Opcode::I32Shl:	case Opcode::I32ShrU:	case Opcode::I32ShrS:	case Opcode::I32Rotr:		case Opcode::I32Rotl:
			case Opcode::I64Add:			case Opcode::I64Sub:			case Opcode::I64Mul:			case Opcode::I64DivS:	case Opcode::I64DivU:	case Opcode::I64RemS:	case Opcode::I64RemU:
			case Opcode::I64And:			case Opcode::I64Or:				case Opcode::I64Xor:			case Opcode::I64Shl:	case Opcode::I64ShrU:	case Opcode::I64ShrS:	case Opcode::I64Rotr:		case Opcode::I64Rotl:
			case Opcode::F32Add:			case Opcode::F32Sub:			case Opcode::F32Mul:			case Opcode::F32Div:	case Opcode::F32Min:	case Opcode::F32Max:	case Opcode::F32Copysign:
			case Opcode::F64Add:			case Opcode::F64Sub:			case Opcode::F64Mul:			case Opcode::F64Div:	case Opcode::F64Min:	case Opcode::F64Max:	case Opcode::F64Copysign:
			case Opcode::I8X16Add:			case Opcode::I16X8Add:			case Opcode::I32X4Add:			case Opcode::I64X2Add:
			case Opcode::I8X16Sub:			case Opcode::I16X8Sub:			case Opcode::I32X4Sub:			case Opcode::I64X2Sub:
			case Opcode::I8X16Mul:			case Opcode::I16X8Mul:			case Opcode::I32X4Mul:
			case Opcode::I8X16AddSaturateS:	case Opcode::I8X16AddSaturateU:	case Opcode::I16X8AddSaturateS:	case Opcode::I16X8AddSaturateU:
			case Opcode::I8X16SubSaturateS:	case Opcode::I8X16SubSaturateU:	case Opcode::I16X8SubSaturateS:	case Opcode::I16X8SubSaturateU:
			case Opcode::I8X16Shl:			case Opcode::I16X8Shl:			case Opcode::I32X4Shl:			case Opcode::I64X2Shl:
			case Opcode::I8X16ShrS:			case Opcode::I8X16ShrU:			case Opcode::I16X8ShrS:			case Opcode::I16X8ShrU:	case Opcode::I32X4ShrS:	case Opcode::I32X4ShrU:	case Opcode::I64X2ShrS:		case Opcode::I64X2ShrU:
			case Opcode::V128And:			case Opcode::V128Or:			case Opcode::V128Xor:			case Opcode::F32X4Min:
			case Opcode::F64X2Min:			case Opcode::F32X4Max:			case Opcode::F64X2Max:
			case Opcode::F32X4Add:			case Opcode::F64X2Add:			case Opcode::F32X4Sub:			case Opcode::F64X2Sub:
			case Opcode::F32X4Div:			case Opcode::F64X2Div:			case Opcode::F32X4Mul:			case Opcode::F64X2Mul:
				CALLBACK(OnBinaryExpr, opcode);
				CALLBACK0(OnOpcodeBare);
				break;

			case Opcode::I32Eq:		case Opcode::I32Ne:		case Opcode::I32LtS:	case Opcode::I32LeS:	case Opcode::I32LtU:	case Opcode::I32LeU:	case Opcode::I32GtS:	case Opcode::I32GeS:	case Opcode::I32GtU:	case Opcode::I32GeU:
			case Opcode::I64Eq:		case Opcode::I64Ne:		case Opcode::I64LtS:	case Opcode::I64LeS:	case Opcode::I64LtU:	case Opcode::I64LeU:	case Opcode::I64GtS:	case Opcode::I64GeS:	case Opcode::I64GtU:	case Opcode::I64GeU:
			case Opcode::F32Eq:		case Opcode::F32Ne:		case Opcode::F32Lt:		case Opcode::F32Le:		case Opcode::F32Gt:		case Opcode::F32Ge:
			case Opcode::F64Eq:		case Opcode::F64Ne:		case Opcode::F64Lt:		case Opcode::F64Le:		case Opcode::F64Gt:		case Opcode::F64Ge:
			case Opcode::I8X16Eq:	case Opcode::I16X8Eq:	case Opcode::I32X4Eq:	case Opcode::F32X4Eq:	case Opcode::F64X2Eq:
			case Opcode::I8X16Ne:	case Opcode::I16X8Ne:	case Opcode::I32X4Ne:	case Opcode::F32X4Ne:	case Opcode::F64X2Ne:
			case Opcode::I8X16LtS:	case Opcode::I8X16LtU:	case Opcode::I16X8LtS:	case Opcode::I16X8LtU:	case Opcode::I32X4LtS:	case Opcode::I32X4LtU:	case Opcode::F32X4Lt:	case Opcode::F64X2Lt:
			case Opcode::I8X16LeS:	case Opcode::I8X16LeU:	case Opcode::I16X8LeS:	case Opcode::I16X8LeU:	case Opcode::I32X4LeS:	case Opcode::I32X4LeU:	case Opcode::F32X4Le:	case Opcode::F64X2Le:
			case Opcode::I8X16GtS:	case Opcode::I8X16GtU:	case Opcode::I16X8GtS:	case Opcode::I16X8GtU:	case Opcode::I32X4GtS:	case Opcode::I32X4GtU:	case Opcode::F32X4Gt:	case Opcode::F64X2Gt:
			case Opcode::I8X16GeS:	case Opcode::I8X16GeU:	case Opcode::I16X8GeS:	case Opcode::I16X8GeU:	case Opcode::I32X4GeS:	case Opcode::I32X4GeU:	case Opcode::F32X4Ge:	case Opcode::F64X2Ge:
				CALLBACK(OnCompareExpr, opcode);
				CALLBACK0(OnOpcodeBare);
				break;

			case Opcode::I32Clz:		case Opcode::I32Ctz:		case Opcode::I32Popcnt:
			case Opcode::I64Clz:		case Opcode::I64Ctz:		case Opcode::I64Popcnt:
			case Opcode::F32Abs:		case Opcode::F32Neg:		case Opcode::F32Ceil:		case Opcode::F32Floor:		case Opcode::F32Trunc:		case Opcode::F32Nearest:	case Opcode::F32Sqrt:
			case Opcode::F64Abs:		case Opcode::F64Neg:		case Opcode::F64Ceil:		case Opcode::F64Floor:		case Opcode::F64Trunc:		case Opcode::F64Nearest:	case Opcode::F64Sqrt:
			case Opcode::I8X16Splat:	case Opcode::I16X8Splat:	case Opcode::I32X4Splat:	case Opcode::I64X2Splat:	case Opcode::F32X4Splat:	case Opcode::F64X2Splat:
			case Opcode::I8X16Neg:		case Opcode::I16X8Neg:		case Opcode::I32X4Neg:		case Opcode::I64X2Neg:
			case Opcode::V128Not:
			case Opcode::I8X16AnyTrue:	case Opcode::I16X8AnyTrue:	case Opcode::I32X4AnyTrue:	case Opcode::I64X2AnyTrue:	case Opcode::I8X16AllTrue:	case Opcode::I16X8AllTrue:	case Opcode::I32X4AllTrue:	case Opcode::I64X2AllTrue:
			case Opcode::F32X4Neg:		case Opcode::F64X2Neg:		case Opcode::F32X4Abs:		case Opcode::F64X2Abs:		case Opcode::F32X4Sqrt:		case Opcode::F64X2Sqrt:
				CALLBACK(OnUnaryExpr, opcode);
				CALLBACK0(OnOpcodeBare);
				break;

			case Opcode::V128BitSelect:
				CALLBACK(OnTernaryExpr, opcode);
				CALLBACK0(OnOpcodeBare);
				break;

			case Opcode::I8X16ExtractLaneS:	case Opcode::I8X16ExtractLaneU:	case Opcode::I16X8ExtractLaneS:	case Opcode::I16X8ExtractLaneU:	case Opcode::I32X4ExtractLane:	case Opcode::I64X2ExtractLane:	case Opcode::F32X4ExtractLane:	case Opcode::F64X2ExtractLane:
			case Opcode::I8X16ReplaceLane:	case Opcode::I16X8ReplaceLane:	case Opcode::I32X4ReplaceLane:	case Opcode::I64X2ReplaceLane:	case Opcode::F32X4ReplaceLane:	case Opcode::F64X2ReplaceLane:
			{
				uint8 lane_val;
				CHECK_RESULT(r.read(lane_val));
				CALLBACK(OnSimdLaneOpExpr, opcode, lane_val);
				CALLBACK(OnOpcodeUint64, lane_val);
				break;
			}

			case Opcode::V8X16Shuffle: {
				uint128 value;
				CHECK_RESULT(r.read(value));
				CALLBACK(OnSimdShuffleOpExpr, opcode, value);
				CALLBACK(OnOpcodeV128, value);
				break;
			}

			case Opcode::I32TruncSF32:		case Opcode::I32TruncSF64:			case Opcode::I32TruncUF32:			case Opcode::I32TruncUF64:			case Opcode::I32WrapI64:
			case Opcode::I64TruncSF32:		case Opcode::I64TruncSF64:			case Opcode::I64TruncUF32:			case Opcode::I64TruncUF64:
			case Opcode::I64ExtendSI32:		case Opcode::I64ExtendUI32:
			case Opcode::F32ConvertSI32:	case Opcode::F32ConvertUI32:		case Opcode::F32ConvertSI64:		case Opcode::F32ConvertUI64:
			case Opcode::F32DemoteF64:		case Opcode::F32ReinterpretI32:
			case Opcode::F64ConvertSI32:	case Opcode::F64ConvertUI32:		case Opcode::F64ConvertSI64:		case Opcode::F64ConvertUI64:		case Opcode::F64PromoteF32:
			case Opcode::F64ReinterpretI64:	case Opcode::I32ReinterpretF32:		case Opcode::I64ReinterpretF64:
			case Opcode::I32Eqz:			case Opcode::I64Eqz:
			case Opcode::F32X4ConvertSI32X4:case Opcode::F32X4ConvertUI32X4:	case Opcode::F64X2ConvertSI64X2:	case Opcode::F64X2ConvertUI64X2:	case Opcode::I32X4TruncSF32X4Sat:	case Opcode::I32X4TruncUF32X4Sat:	case Opcode::I64X2TruncSF64X2Sat:	case Opcode::I64X2TruncUF64X2Sat:
				CALLBACK(OnConvertExpr, opcode);
				CALLBACK0(OnOpcodeBare);
				break;

			case Opcode::Try: {
				Type sig_type;
				CHECK_RESULT(ReadType(&sig_type));
				ERROR_UNLESS(IsBlockType(sig_type), "expected valid block signature type");
				CALLBACK(OnTryExpr, sig_type);
				CALLBACK(OnOpcodeBlockSig, sig_type);
				break;
			}
			case Opcode::Catch: {
				CALLBACK0(OnCatchExpr);
				CALLBACK0(OnOpcodeBare);
				break;
			}
			case Opcode::Rethrow: {
				CALLBACK0(OnRethrowExpr);
				CALLBACK0(OnOpcodeBare);
				break;
			}
			case Opcode::Throw: {
				Index index;
				CHECK_RESULT(ReadIndex(&index));
				CALLBACK(OnThrowExpr, index);
				CALLBACK(OnOpcodeIndex, index);
				break;
			}
			case Opcode::IfExcept: {
				Type sig_type;
				CHECK_RESULT(ReadType(&sig_type));
				ERROR_UNLESS(IsBlockType(sig_type), "expected valid block signature type");
				Index except_index;
				CHECK_RESULT(ReadIndex(&except_index));
				CALLBACK(OnIfExceptExpr, sig_type, except_index);
				break;
			}
			case Opcode::I32Extend8S:		case Opcode::I32Extend16S:		case Opcode::I64Extend8S:		case Opcode::I64Extend16S:		case Opcode::I64Extend32S:
				CALLBACK(OnUnaryExpr, opcode);
				CALLBACK0(OnOpcodeBare);
				break;

			case Opcode::I32TruncSSatF32:	case Opcode::I32TruncUSatF32:	case Opcode::I32TruncSSatF64:	case Opcode::I32TruncUSatF64:	case Opcode::I64TruncSSatF32:	case Opcode::I64TruncUSatF32:	case Opcode::I64TruncSSatF64:	case Opcode::I64TruncUSatF64:
				CALLBACK(OnConvertExpr, opcode);
				CALLBACK0(OnOpcodeBare);
				break;

			case Opcode::AtomicWake: {
				uint32 alignment_log2;
				CHECK_RESULT(read_leb128(r, alignment_log2));
				Address offset;
				CHECK_RESULT(read_leb128(r, offset));
				CALLBACK(OnAtomicWakeExpr, opcode, alignment_log2, offset);
				CALLBACK(OnOpcodeUint32Uint32, alignment_log2, offset);
				break;
			}
			case Opcode::I32AtomicWait:
			case Opcode::I64AtomicWait: {
				uint32 alignment_log2;
				CHECK_RESULT(read_leb128(r, alignment_log2));
				Address offset;
				CHECK_RESULT(read_leb128(r, offset));
				CALLBACK(OnAtomicWaitExpr, opcode, alignment_log2, offset);
				CALLBACK(OnOpcodeUint32Uint32, alignment_log2, offset);
				break;
			}
			case Opcode::I32AtomicLoad8U:	case Opcode::I32AtomicLoad16U:	case Opcode::I64AtomicLoad8U:	case Opcode::I64AtomicLoad16U:	case Opcode::I64AtomicLoad32U:	case Opcode::I32AtomicLoad:		case Opcode::I64AtomicLoad:
			{
				uint32 alignment_log2;
				CHECK_RESULT(read_leb128(r, alignment_log2));
				Address offset;
				CHECK_RESULT(read_leb128(r, offset));
				CALLBACK(OnAtomicLoadExpr, opcode, alignment_log2, offset);
				CALLBACK(OnOpcodeUint32Uint32, alignment_log2, offset);
				break;
			}
			case Opcode::I32AtomicStore8:	case Opcode::I32AtomicStore16:	case Opcode::I64AtomicStore8:	case Opcode::I64AtomicStore16:	case Opcode::I64AtomicStore32:	case Opcode::I32AtomicStore:	case Opcode::I64AtomicStore:
			{
				uint32 alignment_log2;
				CHECK_RESULT(read_leb128(r, alignment_log2));
				Address offset;
				CHECK_RESULT(read_leb128(r, offset));
				CALLBACK(OnAtomicStoreExpr, opcode, alignment_log2, offset);
				CALLBACK(OnOpcodeUint32Uint32, alignment_log2, offset);
				break;
			}
			case Opcode::I32AtomicRmwAdd:	case Opcode::I64AtomicRmwAdd:	case Opcode::I32AtomicRmw8UAdd:		case Opcode::I32AtomicRmw16UAdd:	case Opcode::I64AtomicRmw8UAdd:		case Opcode::I64AtomicRmw16UAdd:	case Opcode::I64AtomicRmw32UAdd:
			case Opcode::I32AtomicRmwSub:	case Opcode::I64AtomicRmwSub:	case Opcode::I32AtomicRmw8USub:		case Opcode::I32AtomicRmw16USub:	case Opcode::I64AtomicRmw8USub:		case Opcode::I64AtomicRmw16USub:	case Opcode::I64AtomicRmw32USub:
			case Opcode::I32AtomicRmwAnd:	case Opcode::I64AtomicRmwAnd:	case Opcode::I32AtomicRmw8UAnd:		case Opcode::I32AtomicRmw16UAnd:	case Opcode::I64AtomicRmw8UAnd:		case Opcode::I64AtomicRmw16UAnd:	case Opcode::I64AtomicRmw32UAnd:
			case Opcode::I32AtomicRmwOr:	case Opcode::I64AtomicRmwOr:	case Opcode::I32AtomicRmw8UOr:		case Opcode::I32AtomicRmw16UOr:		case Opcode::I64AtomicRmw8UOr:		case Opcode::I64AtomicRmw16UOr:		case Opcode::I64AtomicRmw32UOr:
			case Opcode::I32AtomicRmwXor:	case Opcode::I64AtomicRmwXor:	case Opcode::I32AtomicRmw8UXor:		case Opcode::I32AtomicRmw16UXor:	case Opcode::I64AtomicRmw8UXor:		case Opcode::I64AtomicRmw16UXor:	case Opcode::I64AtomicRmw32UXor:
			case Opcode::I32AtomicRmwXchg:	case Opcode::I64AtomicRmwXchg:	case Opcode::I32AtomicRmw8UXchg:	case Opcode::I32AtomicRmw16UXchg:	case Opcode::I64AtomicRmw8UXchg:	case Opcode::I64AtomicRmw16UXchg:	case Opcode::I64AtomicRmw32UXchg:
			{
				uint32 alignment_log2;
				CHECK_RESULT(read_leb128(r, alignment_log2));
				Address offset;
				CHECK_RESULT(read_leb128(r, offset));
				CALLBACK(OnAtomicRmwExpr, opcode, alignment_log2, offset);
				CALLBACK(OnOpcodeUint32Uint32, alignment_log2, offset);
				break;
			}
			case Opcode::I32AtomicRmwCmpxchg:
			case Opcode::I64AtomicRmwCmpxchg:
			case Opcode::I32AtomicRmw8UCmpxchg:
			case Opcode::I32AtomicRmw16UCmpxchg:
			case Opcode::I64AtomicRmw8UCmpxchg:
			case Opcode::I64AtomicRmw16UCmpxchg:
			case Opcode::I64AtomicRmw32UCmpxchg: {
				uint32 alignment_log2;
				CHECK_RESULT(read_leb128(r, alignment_log2));
				Address offset;
				CHECK_RESULT(read_leb128(r, offset));

				CALLBACK(OnAtomicRmwCmpxchgExpr, opcode, alignment_log2, offset);
				CALLBACK(OnOpcodeUint32Uint32, alignment_log2, offset);
				break;
			}
			default:
				return false;//ReportUnexpectedOpcode(opcode);
		}
	}
	ERROR_UNLESS(r.tell() == end_offset, "function body longer than given size");
	ERROR_UNLESS(seen_end_opcode, "function body must end with END opcode");
	return true;
}

bool BinaryReader::ReadNameSection(Offset section_size) {
	CALLBACK(BeginNamesSection, section_size);
	Index i = 0;
	uint32 previous_subsection_type = 0;
	while (r.tell() < read_end) {
		uint32 name_type;
		Offset subsection_size;
		CHECK_RESULT(read_leb128(r, name_type));
		if (i != 0) {
			ERROR_UNLESS(name_type != previous_subsection_type, "duplicate sub-section");
			ERROR_UNLESS(name_type >= previous_subsection_type, "out-of-order sub-section");
		}
		previous_subsection_type = name_type;
		CHECK_RESULT(ReadOffset(&subsection_size));
		size_t subsection_end = r.tell() + subsection_size;
		ERROR_UNLESS(subsection_end <= read_end, "invalid sub-section size: extends past end");
		auto	guard	= save(read_end, subsection_end);

		switch (static_cast<NameSectionSubsection>(name_type)) {
			case NameSectionSubsection::Module:
				CALLBACK(OnModuleNameSubsection, i, name_type, subsection_size);
				if (subsection_size) {
					count_string name;
					CHECK_RESULT(ReadStr(&name));
					CALLBACK(OnModuleName, name);
				}
				break;
			case NameSectionSubsection::Function:
				CALLBACK(OnFunctionNameSubsection, i, name_type, subsection_size);
				if (subsection_size) {
					Index num_names;
					CHECK_RESULT(ReadCount(&num_names));
					CALLBACK(OnFunctionNamesCount, num_names);
					Index last_function_index = kInvalidIndex;

					for (Index j = 0; j < num_names; ++j) {
						Index			function_index;
						count_string	function_name;
						CHECK_RESULT(ReadIndex(&function_index));
						ERROR_UNLESS(function_index != last_function_index, "duplicate function name");
						ERROR_UNLESS(last_function_index == kInvalidIndex || function_index > last_function_index, "function index out of order");
						last_function_index = function_index;
						ERROR_UNLESS(function_index < NumTotalFuncs(), "invalid function index");
						CHECK_RESULT(ReadStr(&function_name));
						CALLBACK(OnFunctionName, function_index, function_name);
					}
				}
				break;
			case NameSectionSubsection::Local:
				CALLBACK(OnLocalNameSubsection, i, name_type, subsection_size);
				if (subsection_size) {
					Index num_funcs;
					CHECK_RESULT(ReadCount(&num_funcs));
					CALLBACK(OnLocalNameFunctionCount, num_funcs);
					Index last_function_index = kInvalidIndex;
					for (Index j = 0; j < num_funcs; ++j) {
						Index function_index;
						CHECK_RESULT(ReadIndex(&function_index));
						ERROR_UNLESS(function_index < NumTotalFuncs(), "invalid function index");
						ERROR_UNLESS(last_function_index == kInvalidIndex ||
							function_index > last_function_index, "locals function index out of order");
						last_function_index = function_index;
						Index num_locals;
						CHECK_RESULT(ReadCount(&num_locals));
						CALLBACK(OnLocalNameLocalCount, function_index, num_locals);
						Index last_local_index = kInvalidIndex;
						for (Index k = 0; k < num_locals; ++k) {
							Index local_index;
							count_string local_name;

							CHECK_RESULT(ReadIndex(&local_index));
							ERROR_UNLESS(local_index != last_local_index, "duplicate local index");
							ERROR_UNLESS(last_local_index == kInvalidIndex ||
								local_index > last_local_index, "local index out of order");
							last_local_index = local_index;
							CHECK_RESULT(ReadStr(&local_name));
							CALLBACK(OnLocalName, function_index, local_index, local_name);
						}
					}
				}
				break;
			default:
				// Unknown subsection, skip it.
				r.seek(subsection_end);
				break;
		}
		++i;
		ERROR_UNLESS(r.tell() == subsection_end, "unfinished sub-section");
	}
	CALLBACK0(EndNamesSection);
	return true;
}

bool BinaryReader::ReadRelocSection(Offset section_size) {
	CALLBACK(BeginRelocSection, section_size);
	uint32 section_index;
	CHECK_RESULT(read_leb128(r, section_index));
	Index num_relocs;
	CHECK_RESULT(ReadCount(&num_relocs));
	CALLBACK(OnRelocCount, num_relocs, section_index);
	for (Index i = 0; i < num_relocs; ++i) {
		Offset	offset;
		Index	index;
		uint32	reloc_type;
		int32	addend = 0;
		CHECK_RESULT(read_leb128(r, reloc_type));
		CHECK_RESULT(ReadOffset(&offset));
		CHECK_RESULT(ReadIndex(&index));
		Reloc::Type type = static_cast<Reloc::Type>(reloc_type);
		switch (type) {
			case Reloc::MemoryAddressLEB:
			case Reloc::MemoryAddressSLEB:
			case Reloc::MemoryAddressI32:
			case Reloc::FunctionOffsetI32:
			case Reloc::SectionOffsetI32:
				CHECK_RESULT(read_leb128(r, addend));
				break;
			default:
				break;
		}
		CALLBACK(OnReloc, type, offset, index, addend);
	}
	CALLBACK0(EndRelocSection);
	return true;
}

bool BinaryReader::ReadLinkingSection(Offset section_size) {
	CALLBACK(BeginLinkingSection, section_size);
	uint32 version;
	CHECK_RESULT(read_leb128(r, version));
	ERROR_UNLESS(version == 1, "invalid linking metadata version");
	while (r.tell() < read_end) {
		uint32 linking_type;
		Offset subsection_size;
		CHECK_RESULT(read_leb128(r, linking_type));
		CHECK_RESULT(ReadOffset(&subsection_size));
		size_t subsection_end = r.tell() + subsection_size;
		ERROR_UNLESS(subsection_end <= read_end, "invalid sub-section size: extends past end");
		auto	guard	= save(read_end, subsection_end);

		uint32 count;
		switch (static_cast<LinkingEntryType>(linking_type)) {
			case LinkingEntryType::SymbolTable:
				CHECK_RESULT(read_leb128(r, count));
				CALLBACK(OnSymbolCount, count);
				for (Index i = 0; i < count; ++i) {
					count_string name;
					uint32 flags = 0;
					uint32 kind = 0;
					CHECK_RESULT(read_leb128(r, kind));
					CHECK_RESULT(read_leb128(r, flags));
					Symbol::Type sym_type = static_cast<Symbol::Type>(kind);
					CALLBACK(OnSymbol, i, sym_type, flags);
					switch (sym_type) {
						case Symbol::Type::Function:
						case Symbol::Type::Global: {
							uint32 index = 0;
							CHECK_RESULT(read_leb128(r, index));
							if ((flags & Symbol::Undefined) == 0)
								CHECK_RESULT(ReadStr(&name));
							if (sym_type == Symbol::Type::Function) {
								CALLBACK(OnFunctionSymbol, i, flags, name, index);
							} else {
								CALLBACK(OnGlobalSymbol, i, flags, name, index);
							}
							break;
						}
						case Symbol::Type::Data: {
							uint32 segment = 0;
							uint32 offset = 0;
							uint32 size = 0;
							CHECK_RESULT(ReadStr(&name));
							if ((flags & Symbol::Undefined) == 0) {
								CHECK_RESULT(read_leb128(r, segment));
								CHECK_RESULT(read_leb128(r, offset));
								CHECK_RESULT(read_leb128(r, size));
							}
							CALLBACK(OnDataSymbol, i, flags, name, segment, offset, size);
							break;
						}
						case Symbol::Type::Section: {
							uint32 index = 0;
							CHECK_RESULT(read_leb128(r, index));
							CALLBACK(OnSectionSymbol, i, flags, index);
							break;
						}
					}
				}
				break;
			case LinkingEntryType::SegmentInfo:
				CHECK_RESULT(read_leb128(r, count));
				CALLBACK(OnSegmentInfoCount, count);
				for (Index i = 0; i < count; i++) {
					count_string name;
					uint32 alignment;
					uint32 flags;
					CHECK_RESULT(ReadStr(&name));
					CHECK_RESULT(read_leb128(r, alignment));
					CHECK_RESULT(read_leb128(r, flags));
					CALLBACK(OnSegmentInfo, i, name, alignment, flags);
				}
				break;
			case LinkingEntryType::InitFunctions:
				CHECK_RESULT(read_leb128(r, count));
				CALLBACK(OnInitFunctionCount, count);
				while (count--) {
					uint32 priority;
					uint32 func;
					CHECK_RESULT(read_leb128(r, priority));
					CHECK_RESULT(read_leb128(r, func));
					CALLBACK(OnInitFunction, priority, func);
				}
				break;
			default:
				// Unknown subsection, skip it.
				r.seek(subsection_end);
				break;
		}
		ERROR_UNLESS(r.tell() == subsection_end, "unfinished sub-section");
	}
	CALLBACK0(EndLinkingSection);
	return true;
}

bool BinaryReader::ReadExceptionType(TypeVector& sig) {
	Index num_values;
	CHECK_RESULT(ReadCount(&num_values));
	sig.resize(num_values);
	for (Index j = 0; j < num_values; ++j) {
		Type value_type;
		CHECK_RESULT(ReadType(&value_type));
		ERROR_UNLESS(IsConcreteType(value_type), "excepted valid exception value type");
		sig[j] = value_type;
	}
	return true;
}

bool BinaryReader::ReadExceptionSection(Offset section_size) {
	CALLBACK(BeginExceptionSection, section_size);
	CHECK_RESULT(ReadCount(&num_exceptions));
	CALLBACK(OnExceptionCount, num_exceptions);

	for (Index i = 0; i < num_exceptions; ++i) {
		TypeVector sig;
		CHECK_RESULT(ReadExceptionType(sig));
		CALLBACK(OnExceptionType, i, sig);
	}

	CALLBACK(EndExceptionSection);
	return true;
}

bool BinaryReader::ReadCustomSection(Offset section_size) {
	count_string section_name;
	CHECK_RESULT(ReadStr(&section_name));
	CALLBACK(BeginCustomSection, section_size, section_name);

	if (section_name == WABT_BINARY_SECTION_NAME) {
		CHECK_RESULT(ReadNameSection(section_size));
		did_read_names_section = true;
	} else if (section_name.rfind(WABT_BINARY_SECTION_RELOC) == 0) {
		// Reloc sections always begin with "reloc."
		CHECK_RESULT(ReadRelocSection(section_size));
	} else if (section_name == WABT_BINARY_SECTION_LINKING) {
		CHECK_RESULT(ReadLinkingSection(section_size));
	} else if (features.exceptions && section_name == WABT_BINARY_SECTION_EXCEPTION) {
		CHECK_RESULT(ReadExceptionSection(section_size));
	} else {
		// This is an unknown custom section, skip it.
		r.seek(read_end);
	}
	CALLBACK0(EndCustomSection);
	return true;
}

bool BinaryReader::ReadTypeSection(Offset section_size) {
	CALLBACK(BeginTypeSection, section_size);
	CHECK_RESULT(ReadCount(&num_signatures));
	CALLBACK(OnTypeCount, num_signatures);

	for (Index i = 0; i < num_signatures; ++i) {
		Type form;
		CHECK_RESULT(ReadType(&form));
		ERROR_UNLESS(form == Type::Func, "unexpected type form");

		Index num_params;
		CHECK_RESULT(ReadCount(&num_params));

		param_types.resize(num_params);

		for (Index j = 0; j < num_params; ++j) {
			Type param_type;
			CHECK_RESULT(ReadType(&param_type));
			ERROR_UNLESS(IsConcreteType(param_type), "expected valid param type");
			param_types[j] = param_type;
		}

		Index num_results;
		CHECK_RESULT(ReadCount(&num_results));
		ERROR_UNLESS(num_results <= 1 || features.multi_value, "result count must be 0 or 1");

		result_types.resize(num_results);

		for (Index j = 0; j < num_results; ++j) {
			Type result_type;
			CHECK_RESULT(ReadType(&result_type));
			ERROR_UNLESS(IsConcreteType(result_type), "expected valid result type");
			result_types[j] = result_type;
		}

		CALLBACK(OnType, i, num_params, param_types.begin(), num_results, result_types.begin());
	}
	CALLBACK0(EndTypeSection);
	return true;
}

bool BinaryReader::ReadImportSection(Offset section_size) {
	CALLBACK(BeginImportSection, section_size);
	CHECK_RESULT(ReadCount(&num_imports));
	CALLBACK(OnImportCount, num_imports);
	for (Index i = 0; i < num_imports; ++i) {
		count_string module_name;
		CHECK_RESULT(ReadStr(&module_name));
		count_string field_name;
		CHECK_RESULT(ReadStr(&field_name));

		uint8 kind;
		CHECK_RESULT(r.read(kind));
		switch (static_cast<ExternalKind>(kind)) {
			case ExternalKind::Func: {
				Index sig_index;
				CHECK_RESULT(ReadIndex(&sig_index));
				ERROR_UNLESS(sig_index < num_signatures, "invalid import signature index");
				CALLBACK(OnImport, i, module_name, field_name);
				CALLBACK(OnImportFunc, i, module_name, field_name, num_func_imports, sig_index);
				num_func_imports++;
				break;
			}

			case ExternalKind::Table: {
				Type elem_type;
				Limits elem_limits;
				CHECK_RESULT(ReadTable(&elem_type, &elem_limits));
				CALLBACK(OnImport, i, module_name, field_name);
				CALLBACK(OnImportTable, i, module_name, field_name, num_table_imports, elem_type, &elem_limits);
				num_table_imports++;
				break;
			}

			case ExternalKind::Memory: {
				Limits page_limits;
				CHECK_RESULT(ReadMemory(&page_limits));
				CALLBACK(OnImport, i, module_name, field_name);
				CALLBACK(OnImportMemory, i, module_name, field_name, num_memory_imports, &page_limits);
				num_memory_imports++;
				break;
			}

			case ExternalKind::Global: {
				Type type;
				bool mut;
				CHECK_RESULT(ReadGlobalHeader(&type, &mut));
				CALLBACK(OnImport, i, module_name, field_name);
				CALLBACK(OnImportGlobal, i, module_name, field_name, num_global_imports, type, mut);
				num_global_imports++;
				break;
			}

			case ExternalKind::Except: {
				ERROR_UNLESS(features.exceptions, "invalid import exception kind: exceptions not allowed");
				TypeVector sig;
				CHECK_RESULT(ReadExceptionType(sig));
				CALLBACK(OnImport, i, module_name, field_name);
				CALLBACK(OnImportException, i, module_name, field_name, num_exception_imports, sig);
				num_exception_imports++;
				break;
			}
		}
	}
	CALLBACK0(EndImportSection);
	return true;
}

bool BinaryReader::ReadFunctionSection(Offset section_size) {
	CALLBACK(BeginFunctionSection, section_size);
	CHECK_RESULT(ReadCount(&num_function_signatures));
	CALLBACK(OnFunctionCount, num_function_signatures);
	for (Index i = 0; i < num_function_signatures; ++i) {
		Index func_index = num_func_imports + i;
		Index sig_index;
		CHECK_RESULT(ReadIndex(&sig_index));
		ERROR_UNLESS(sig_index < num_signatures, "invalid function signature index");
		CALLBACK(OnFunction, func_index, sig_index);
	}
	CALLBACK0(EndFunctionSection);
	return true;
}

bool BinaryReader::ReadTableSection(Offset section_size) {
	CALLBACK(BeginTableSection, section_size);
	CHECK_RESULT(ReadCount(&num_tables));
	ERROR_UNLESS(num_tables <= 1, "table count must be 0 or 1");
	CALLBACK(OnTableCount, num_tables);
	for (Index i = 0; i < num_tables; ++i) {
		Index table_index = num_table_imports + i;
		Type elem_type;
		Limits elem_limits;
		CHECK_RESULT(ReadTable(&elem_type, &elem_limits));
		CALLBACK(OnTable, table_index, elem_type, &elem_limits);
	}
	CALLBACK0(EndTableSection);
	return true;
}

bool BinaryReader::ReadMemorySection(Offset section_size) {
	CALLBACK(BeginMemorySection, section_size);
	CHECK_RESULT(ReadCount(&num_memories));
	ERROR_UNLESS(num_memories <= 1, "memory count must be 0 or 1");
	CALLBACK(OnMemoryCount, num_memories);
	for (Index i = 0; i < num_memories; ++i) {
		Index memory_index = num_memory_imports + i;
		Limits page_limits;
		CHECK_RESULT(ReadMemory(&page_limits));
		CALLBACK(OnMemory, memory_index, &page_limits);
	}
	CALLBACK0(EndMemorySection);
	return true;
}

bool BinaryReader::ReadGlobalSection(Offset section_size) {
	CALLBACK(BeginGlobalSection, section_size);
	CHECK_RESULT(ReadCount(&num_globals));
	CALLBACK(OnGlobalCount, num_globals);
	for (Index i = 0; i < num_globals; ++i) {
		Index	global_index = num_global_imports + i;
		Type	global_type;
		bool	mut;
		CHECK_RESULT(ReadGlobalHeader(&global_type, &mut));
		CALLBACK(BeginGlobal, global_index, global_type, mut);
		CALLBACK(BeginGlobalInitExpr, global_index);
		CHECK_RESULT(ReadInitExpr(global_index));
		CALLBACK(EndGlobalInitExpr, global_index);
		CALLBACK(EndGlobal, global_index);
	}
	CALLBACK0(EndGlobalSection);
	return true;
}

bool BinaryReader::ReadExportSection(Offset section_size) {
	CALLBACK(BeginExportSection, section_size);
	CHECK_RESULT(ReadCount(&num_exports));
	CALLBACK(OnExportCount, num_exports);
	for (Index i = 0; i < num_exports; ++i) {
		count_string name;
		CHECK_RESULT(ReadStr(&name));

		uint8 kind = 0;
		CHECK_RESULT(r.read(kind));
		ERROR_UNLESS(kind < (int)ExternalKind::size, "invalid export external kind");

		Index item_index;
		CHECK_RESULT(ReadIndex(&item_index));
		switch (static_cast<ExternalKind>(kind)) {
			case ExternalKind::Func:
				ERROR_UNLESS(item_index < NumTotalFuncs(), "invalid export func index");
				break;
			case ExternalKind::Table:
				ERROR_UNLESS(item_index < NumTotalTables(), "invalid export table index");
				break;
			case ExternalKind::Memory:
				ERROR_UNLESS(item_index < NumTotalMemories(), "invalid export memory index");
				break;
			case ExternalKind::Global:
				ERROR_UNLESS(item_index < NumTotalGlobals(), "invalid export global index");
				break;
			case ExternalKind::Except:
				// Note: Can't check if index valid, exceptions section comes later.
				ERROR_UNLESS(features.exceptions, "invalid export exception kind: exceptions not allowed");
				break;
		}

		CALLBACK(OnExport, i, static_cast<ExternalKind>(kind), item_index, name);
	}
	CALLBACK0(EndExportSection);
	return true;
}

bool BinaryReader::ReadStartSection(Offset section_size) {
	CALLBACK(BeginStartSection, section_size);
	Index func_index;
	CHECK_RESULT(ReadIndex(&func_index));
	ERROR_UNLESS(func_index < NumTotalFuncs(), "invalid start function index");
	CALLBACK(OnStartFunction, func_index);
	CALLBACK0(EndStartSection);
	return true;
}

bool BinaryReader::ReadElemSection(Offset section_size) {
	CALLBACK(BeginElemSection, section_size);
	Index num_elem_segments;
	CHECK_RESULT(ReadCount(&num_elem_segments));
	CALLBACK(OnElemSegmentCount, num_elem_segments);
	ERROR_UNLESS(num_elem_segments == 0 || NumTotalTables() > 0, "elem section without table section");
	for (Index i = 0; i < num_elem_segments; ++i) {
		Index table_index;
		CHECK_RESULT(ReadIndex(&table_index));
		CALLBACK(BeginElemSegment, i, table_index);
		CALLBACK(BeginElemSegmentInitExpr, i);
		CHECK_RESULT(ReadI32InitExpr(i));
		CALLBACK(EndElemSegmentInitExpr, i);

		Index num_function_indexes;
		CHECK_RESULT(ReadCount(&num_function_indexes));
		CALLBACK(OnElemSegmentFunctionIndexCount, i, num_function_indexes);
		for (Index j = 0; j < num_function_indexes; ++j) {
			Index func_index;
			CHECK_RESULT(ReadIndex(&func_index));
			CALLBACK(OnElemSegmentFunctionIndex, i, func_index);
		}
		CALLBACK(EndElemSegment, i);
	}
	CALLBACK0(EndElemSection);
	return true;
}

bool BinaryReader::ReadCodeSection(Offset section_size) {
	CALLBACK(BeginCodeSection, section_size);
	CHECK_RESULT(ReadCount(&num_function_bodies));
	ERROR_UNLESS(num_function_signatures == num_function_bodies, "function signature count != function body count");
	CALLBACK(OnFunctionBodyCount, num_function_bodies);
	for (Index i = 0; i < num_function_bodies; ++i) {
		Index func_index = num_func_imports + i;
		Offset func_offset = r.tell();
		r.seek(func_offset);
		CALLBACK(BeginFunctionBody, func_index);
		uint32 body_size;
		CHECK_RESULT(read_leb128(r, body_size));
		Offset body_start_offset = r.tell();
		Offset end_offset = body_start_offset + body_size;

		uint64 total_locals = 0;
		Index num_local_decls;
		CHECK_RESULT(ReadCount(&num_local_decls));
		CALLBACK(OnLocalDeclCount, num_local_decls);
		for (Index k = 0; k < num_local_decls; ++k) {
			Index num_local_types;
			CHECK_RESULT(ReadIndex(&num_local_types));
			ERROR_UNLESS(num_local_types > 0, "local count must be > 0");
			total_locals += num_local_types;
			//ERROR_UNLESS(total_locals < UINT32_MAX, "local count must be < 0x10000000");
			Type local_type;
			CHECK_RESULT(ReadType(&local_type));
			ERROR_UNLESS(IsConcreteType(local_type), "expected valid local type");
			CALLBACK(OnLocalDecl, k, num_local_types, local_type);
		}

		CHECK_RESULT(ReadFunctionBody(end_offset));

		CALLBACK(EndFunctionBody, func_index);
	}
	CALLBACK0(EndCodeSection);
	return true;
}

bool BinaryReader::ReadDataSection(Offset section_size) {
	CALLBACK(BeginDataSection, section_size);
	Index num_data_segments;
	CHECK_RESULT(ReadCount(&num_data_segments));
	CALLBACK(OnDataSegmentCount, num_data_segments);
	ERROR_UNLESS(num_data_segments == 0 || NumTotalMemories() > 0, "data section without memory section");
	for (Index i = 0; i < num_data_segments; ++i) {
		Index memory_index;
		CHECK_RESULT(ReadIndex(&memory_index));
		CALLBACK(BeginDataSegment, i, memory_index);
		CALLBACK(BeginDataSegmentInitExpr, i);
		CHECK_RESULT(ReadI32InitExpr(i));
		CALLBACK(EndDataSegmentInitExpr, i);

		Address data_size;
		const void* data;
		CHECK_RESULT(ReadBytes(&data, &data_size));
		CALLBACK(OnDataSegmentData, i, data, data_size);
		CALLBACK(EndDataSegment, i);
	}
	CALLBACK0(EndDataSection);
	return true;
}

bool BinaryReader::ReadSections() {
	auto last_known_section	= BinarySection::Invalid;

	while (r.remaining()) {
		BinarySection	section;
		Offset			section_size;

		CHECK_RESULT(read_leb128(r, section));
		if (section >= BinarySection::size)
			return false;

		CHECK_RESULT(ReadOffset(&section_size));

		ERROR_UNLESS(read_end <= r.length(), "invalid section size: extends past end");
		ERROR_UNLESS(last_known_section == BinarySection::Invalid || section == BinarySection::Custom || section > last_known_section, "section out of order");
		ERROR_UNLESS(!did_read_names_section || section == BinarySection::Custom, "section can not occur after Name section");

		CALLBACK(BeginSection, section, section_size);

		bool	stop_on_first_error	= true;
		bool	section_result		= false;
		auto	guard				= save(read_end, r.tell() + section_size);

		switch (section) {
			case BinarySection::Custom:		section_result = ReadCustomSection(section_size);	break;
			case BinarySection::Type:		section_result = ReadTypeSection(section_size);		break;
			case BinarySection::Import:		section_result = ReadImportSection(section_size);	break;
			case BinarySection::Function:	section_result = ReadFunctionSection(section_size);	break;
			case BinarySection::Table:		section_result = ReadTableSection(section_size);	break;
			case BinarySection::Memory:		section_result = ReadMemorySection(section_size);	break;
			case BinarySection::Global:		section_result = ReadGlobalSection(section_size);	break;
			case BinarySection::Export:		section_result = ReadExportSection(section_size);	break;
			case BinarySection::Start:		section_result = ReadStartSection(section_size);	break;
			case BinarySection::Elem:		section_result = ReadElemSection(section_size);		break;
			case BinarySection::Code:		section_result = ReadCodeSection(section_size);		break;
			case BinarySection::Data:		section_result = ReadDataSection(section_size);		break;
			case BinarySection::Invalid:	unreachable();
		}

		if (!section_result) {
			if (stop_on_first_error)
				return false;

			r.seek(read_end);
		}

		ERROR_UNLESS(r.tell() == read_end, "unfinished section");

		if (section != BinarySection::Custom)
			last_known_section = section;
	}

	return true;
}

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
	} else {
		auto	sig = bits.get_mant();
		int		exp	= bits.get_exp() + highest_set_index(sig) - bits.M;
		out << 'p';
		if (exp < 0) {
			out << '-';
			exp = -exp;
		} else {
			out << '+';
		}
		out << exp;
	}
	return out;
}

static const char_set s_is_char_escaped		= ~char_set::ascii + char_set::control + '"';
static const char_set s_valid_name_chars	= char_set::ascii - char_set(" (),;[]{}");


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

class WatWriter {
	enum NextChar {
		None,
		Space,
		Newline,
		ForceNewline,
	};
	class ExprVisitorDelegate;

	string_accum	&acc;
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
			default:			break;
			case Space:			acc.putc(' '); break;
			case Newline:
			case ForceNewline:	acc.putc('\n') << repeat(' ', indent * indent_size); break;
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
	void	WriteExpr(const Expr* expr);
	template<typename T> void WriteLoadStoreExpr(const Expr* expr);
	void	WriteExprList(const ExprList& exprs);
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

	void	BuildInlineExportMap();
	void	WriteInlineExports(ExternalKind kind, Index index);
	bool	IsInlineExport(const Export& exp);
	void	BuildInlineImportMap();
	void	WriteInlineImport(ExternalKind kind, Index index);
public:
	WatWriter(string_accum &acc, bool fold_exprs = false) : acc(acc), fold_exprs(fold_exprs) {}
	bool WriteModule(const Module& module);
};

class WatWriter::ExprVisitorDelegate : public ExprVisitor::Delegate {
	WatWriter * writer;
public:
	explicit ExprVisitorDelegate(WatWriter* writer) : writer(writer) {}

	bool OnBinaryExpr(BinaryExpr* expr) {
		writer->WritePutsNewline(expr->opcode.GetName());
		return true;
	}
	bool BeginBlockExpr(BlockExpr* expr) {
		writer->WriteBeginBlock(LabelType::Block, expr->block, GetName(Opcode::Block));
		return true;
	}
	bool EndBlockExpr(BlockExpr* expr) {
		writer->WriteEndBlock();
		return true;
	}
	bool OnBrExpr(BrExpr* expr) {
		writer->WritePutsSpace(GetName(Opcode::Br));
		writer->WriteBrVar(expr->var, Newline);
		return true;
	}
	bool OnBrIfExpr(BrIfExpr* expr) {
		writer->WritePutsSpace(GetName(Opcode::BrIf));
		writer->WriteBrVar(expr->var, Newline);
		return true;
	}
	bool OnBrTableExpr(BrTableExpr* expr) {
		writer->WritePutsSpace(GetName(Opcode::BrTable));
		for (const Var& var : expr->targets)
			writer->WriteBrVar(var, Space);
		writer->WriteBrVar(expr->default_target, Newline);
		return true;
	}
	bool OnCallExpr(CallExpr* expr) {
		writer->WritePutsSpace(GetName(Opcode::Call));
		writer->WriteVar(expr->var, Newline);
		return true;
	}
	bool OnCallIndirectExpr(
		CallIndirectExpr* expr) {
		writer->WritePutsSpace(GetName(Opcode::CallIndirect));
		writer->WriteOpenSpace("type");
		writer->WriteVar(expr->decl.type_var, Space);
		writer->WriteCloseNewline();
		return true;
	}
	bool OnCompareExpr(CompareExpr* expr) {
		writer->WritePutsNewline(expr->opcode.GetName());
		return true;
	}
	bool OnConstExpr(ConstExpr* expr) {
		writer->WriteConst(expr->cnst);
		return true;
	}
	bool OnConvertExpr(ConvertExpr* expr) {
		writer->WritePutsNewline(expr->opcode.GetName());
		return true;
	}
	bool OnDropExpr(DropExpr* expr) {
		writer->WritePutsNewline(GetName(Opcode::Drop));
		return true;
	}
	bool OnGetGlobalExpr(GetGlobalExpr* expr) {
		writer->WritePutsSpace(GetName(Opcode::GetGlobal));
		writer->WriteVar(expr->var, Newline);
		return true;
	}
	bool OnGetLocalExpr(GetLocalExpr* expr) {
		writer->WritePutsSpace(GetName(Opcode::GetLocal));
		writer->WriteVar(expr->var, Newline);
		return true;
	}
	bool BeginIfExpr(IfExpr* expr) {
		writer->WriteBeginBlock(LabelType::If, expr->true_, GetName(Opcode::If));
		return true;
	}
	bool AfterIfTrueExpr(IfExpr* expr) {
		if (!expr->false_.empty()) {
			writer->Dedent();
			writer->WritePutsSpace(GetName(Opcode::Else));
			writer->Indent();
			writer->WriteNewline(true);
		}
		return true;
	}
	bool EndIfExpr(IfExpr* expr) {
		writer->WriteEndBlock();
		return true;
	}
	bool BeginIfExceptExpr(IfExceptExpr* expr) {
		// Can't use WriteBeginBlock because if_except has an additional exception index argument
		writer->WriteBeginIfExceptBlock(expr);
		return true;
	}
	bool AfterIfExceptTrueExpr(
		IfExceptExpr* expr) {
		if (!expr->false_.empty()) {
			writer->Dedent();
			writer->WritePutsSpace(GetName(Opcode::Else));
			writer->Indent();
			writer->WriteNewline(true);
		}
		return true;
	}
	bool EndIfExceptExpr(IfExceptExpr* expr) {
		writer->WriteEndBlock();
		return true;
	}
	bool OnLoadExpr(LoadExpr* expr) {
		writer->WriteLoadStoreExpr<LoadExpr>(expr);
		return true;
	}
	bool BeginLoopExpr(LoopExpr* expr) {
		writer->WriteBeginBlock(LabelType::Loop, expr->block, GetName(Opcode::Loop));
		return true;
	}
	bool EndLoopExpr(LoopExpr* expr) {
		writer->WriteEndBlock();
		return true;
	}
	bool OnMemoryGrowExpr(MemoryGrowExpr* expr) {
		writer->WritePutsNewline(GetName(Opcode::MemoryGrow));
		return true;
	}
	bool OnMemorySizeExpr(MemorySizeExpr* expr) {
		writer->WritePutsNewline(GetName(Opcode::MemorySize));
		return true;
	}
	bool OnNopExpr(NopExpr* expr) {
		writer->WritePutsNewline(GetName(Opcode::Nop));
		return true;
	}
	bool OnReturnExpr(ReturnExpr* expr) {
		writer->WritePutsNewline(GetName(Opcode::Return));
		return true;
	}
	bool OnSelectExpr(SelectExpr* expr) {
		writer->WritePutsNewline(GetName(Opcode::Select));
		return true;
	}
	bool OnSetGlobalExpr(SetGlobalExpr* expr) {
		writer->WritePutsSpace(GetName(Opcode::SetGlobal));
		writer->WriteVar(expr->var, Newline);
		return true;
	}
	bool OnSetLocalExpr(SetLocalExpr* expr) {
		writer->WritePutsSpace(GetName(Opcode::SetLocal));
		writer->WriteVar(expr->var, Newline);
		return true;
	}
	bool OnStoreExpr(StoreExpr* expr) {
		writer->WriteLoadStoreExpr<StoreExpr>(expr);
		return true;
	}
	bool OnTeeLocalExpr(TeeLocalExpr* expr) {
		writer->WritePutsSpace(GetName(Opcode::TeeLocal));
		writer->WriteVar(expr->var, Newline);
		return true;
	}
	bool OnUnaryExpr(UnaryExpr* expr) {
		writer->WritePutsNewline(expr->opcode.GetName());
		return true;
	}
	bool OnUnreachableExpr(UnreachableExpr* expr) {
		writer->WritePutsNewline(GetName(Opcode::Unreachable));
		return true;
	}
	bool BeginTryExpr(TryExpr* expr) {
		writer->WriteBeginBlock(LabelType::Try, expr->block, GetName(Opcode::Try));
		return true;
	}
	bool OnCatchExpr(TryExpr* expr) {
		writer->Dedent();
		writer->WritePutsSpace(GetName(Opcode::Catch));
		writer->Indent();
		writer->label_stack.back().label_type = LabelType::Catch;
		writer->WriteNewline(true);
		return true;
	}
	bool EndTryExpr(TryExpr* expr) {
		writer->WriteEndBlock();
		return true;
	}
	bool OnThrowExpr(ThrowExpr* expr) {
		writer->WritePutsSpace(GetName(Opcode::Throw));
		writer->WriteVar(expr->var, Newline);
		return true;
	}
	bool OnRethrowExpr(RethrowExpr* expr) {
		writer->WritePutsSpace(GetName(Opcode::Rethrow));
		return true;
	}
	bool OnAtomicWaitExpr(AtomicWaitExpr* expr) {
		writer->WriteLoadStoreExpr<AtomicWaitExpr>(expr);
		return true;
	}
	bool OnAtomicWakeExpr(AtomicWakeExpr* expr) {
		writer->WriteLoadStoreExpr<AtomicWakeExpr>(expr);
		return true;
	}
	bool OnAtomicLoadExpr(AtomicLoadExpr* expr) {
		writer->WriteLoadStoreExpr<AtomicLoadExpr>(expr);
		return true;
	}
	bool OnAtomicStoreExpr(
		AtomicStoreExpr* expr) {
		writer->WriteLoadStoreExpr<AtomicStoreExpr>(expr);
		return true;
	}
	bool OnAtomicRmwExpr(AtomicRmwExpr* expr) {
		writer->WriteLoadStoreExpr<AtomicRmwExpr>(expr);
		return true;
	}
	bool OnAtomicRmwCmpxchgExpr(
		AtomicRmwCmpxchgExpr* expr) {
		writer->WriteLoadStoreExpr<AtomicRmwCmpxchgExpr>(expr);
		return true;
	}
	bool OnTernaryExpr(TernaryExpr* expr) {
		writer->WritePutsNewline(expr->opcode.GetName());
		return true;
	}
	bool OnSimdLaneOpExpr(SimdLaneOpExpr* expr) {
		writer->WritePutsSpace(expr->opcode.GetName());
		writer->Write(expr->val);
		writer->WritePutsNewline("");
		return true;
	}
	bool OnSimdShuffleOpExpr(
		SimdShuffleOpExpr* expr) {
		writer->WritePutsSpace(expr->opcode.GetName());
		const uint32	*p = (const uint32*)&expr->val;
		writer->Writef(" 0x%08x 0x%08x 0x%08x 0x%08x", p[0], p[1], p[2], p[3]);
		writer->WritePutsNewline("");
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
			acc.putc(to_digit(c >> 4));
			acc.putc(to_digit(c & 0xf));
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
		Writef("%" PRIindex, var.index());
		this->next_char = next_char;
	} else {
		WriteName(count_string(var.name()), next_char);
	}
}

void WatWriter::WriteBrVar(const Var& var, NextChar next_char) {
	if (var.is_index()) {
		if (var.index() < GetLabelStackSize())
			Writef("%" PRIindex " (;@%" PRIindex ";)", var.index(),GetLabelStackSize() - var.index() - 1);
		else
			Writef("%" PRIindex " (; INVALID ;)", var.index());
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
		Writef(" ;; label = @%" PRIindex, GetLabelStackSize());

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
		Writef(" ;; label = @%" PRIindex, GetLabelStackSize());

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
			WritePutsSpace(buffer);
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
			WritePutsSpace(buffer);
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

void WatWriter::WriteExpr(const Expr* expr) {
	ExprVisitorDelegate delegate(this);
	ExprVisitor visitor(delegate);
	visitor.VisitExpr(const_cast<Expr*>(expr));
}

void WatWriter::WriteExprList(const ExprList& exprs) {
	ExprVisitorDelegate delegate(this);
	ExprVisitor visitor(delegate);
	visitor.VisitExprList(const_cast<ExprList&>(exprs));
}

Label* WatWriter::GetLabel(const Var& var) {
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
	return	label == 0 ? 0
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
	MakeTypeBindingReverseMapping(types.size(), bindings, &index_to_name);

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
	if (inline_export && IsInlineExport(exp))
		return;

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
	BuildInlineExportMap();
	BuildInlineImportMap();
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

void WatWriter::BuildInlineExportMap() {
	if (!inline_export)
		return;

	ISO_ASSERT(module);
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
			inline_export_map[make_pair(exp->kind, index)] = exp;
	}
}

void WatWriter::WriteInlineExports(ExternalKind kind, Index index) {
	if (!inline_export)
		return;

	for (auto exp : inline_export_map.bounds(make_pair(kind, index))) {
		WriteOpenSpace("export");
		WriteQuotedString(count_string(exp->name), None);
		WriteCloseSpace();
	}
}

bool WatWriter::IsInlineExport(const Export& exp) {
	Index index;
	switch (exp.kind) {
		case ExternalKind::Func:	index = module->GetFuncIndex(exp.var);		break;
		case ExternalKind::Table:	index = module->GetTableIndex(exp.var);		break;
		case ExternalKind::Memory:	index = module->GetMemoryIndex(exp.var);	break;
		case ExternalKind::Global:	index = module->GetGlobalIndex(exp.var);	break;
		case ExternalKind::Except:	index = module->GetExceptIndex(exp.var);	break;
	}
	return inline_export_map.find(make_pair(exp.kind, index)) != inline_export_map.end();
}

void WatWriter::BuildInlineImportMap() {
	if (!inline_import)
		return;

	ISO_ASSERT(module);
	for (const Import* import : module->imports)
		inline_import_map[static_cast<size_t>(import->kind)].push_back(import);
}

void WatWriter::WriteInlineImport(ExternalKind kind, Index index) {
	if (!inline_import)
		return;

	if (index >= inline_import_map[(int)kind].size())
		return;

	const Import* import = inline_import_map[(int)kind][index];
	WriteOpenSpace("import");
	WriteQuotedString(count_string(import->module_name), Space);
	WriteQuotedString(count_string(import->field_name), Space);
	WriteCloseSpace();
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
	global_sym_map_[name] =  mangled;
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
		Index dst = Index(type_stack_.size() - label->type_stack_size - 1);
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
	Writef("static u32 func_types[%" PRIzd "];", module_->func_types.size());
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
				for (uint8 x : data_segment->data) {
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
		uint32 max = table->elem_limits.has_max ? table->elem_limits.max : maximum;
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

	string label = DefineLocalScopeName(kImplicitFuncLabel);
	ResetTypeStack(0);
	string empty;  // Must not be temporary, since address is taken by Label.
	PushLabel(LabelType::Func, empty, func.decl.sig);
	Write(func.exprs, LabelDecl(label));
	PopLabel();
	ResetTypeStack(0);
	PushTypes(func.decl.sig.result_types);
	Write("FUNC_EPILOGUE;", Newline());

	if (!func.decl.sig.result_types.empty())	// Return the top of the stack implicitly.
		Write("return ", StackVar(0), ";", Newline());

	WriteStackVarDeclarations();

	Write(CloseBrace());

	func_ = nullptr;
}

void CWriter::WriteParams() {
	if (func_->decl.sig.param_types.empty()) {
		Write("void");
	} else {
		dynamic_array<string> index_to_name;
		MakeTypeBindingReverseMapping(func_->decl.sig.param_types.size(), func_->param_bindings, &index_to_name);
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
	dynamic_array<string> index_to_name;
	MakeTypeBindingReverseMapping(func_->local_types.size(), func_->local_bindings, &index_to_name);
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
			Type stp_type = pair.k.b;
			const string& name = pair.v;

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

//-----------------------------------------------------------------------------
//	FileHandlers
//-----------------------------------------------------------------------------
#include "iso/iso_files.h"

class WASMFileHandler : public FileHandler {
public:
	const char*		GetExt()			override { return "wasm"; }
	const char*		GetDescription()	override { return "WebAssembly file"; }

	int				Check(istream_ref file) override {
		file.seek(0);
		return file.get<uint32>() == WABT_BINARY_MAGIC && file.get<uint32>() == WABT_BINARY_VERSION ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}
	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		malloc_block	mem		= malloc_block::unterminated(file);
		memory_reader	reader(mem);
		Module			module;

		BinaryReaderDelegate	delegate(module, 0);
		BinaryReader			bin(reader, delegate);
		if (bin.ReadModule()) {
			string_builder	s;
//			WatWriter(string_builder(s), false).WriteModule(module);
			string_builder	h;
			CWriter(s, h, "out.h").WriteModule(module);
			return ISO_ptr<string>(id, move(s));

		}
		return ISO_NULL;
	}
} wasm;
