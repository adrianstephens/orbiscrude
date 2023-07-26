#include "base/algorithm.h"
#include "base/array.h"
#include "base/strings.h"
#include "base/tuple.h"
#include "allocators/allocator.h"

using namespace iso;

enum class Qualifiers		: uint8 { None = 0, Const = 1 << 0, Volatile = 1 << 1, Far = 1 << 2, Huge = 1 << 3, Unaligned = 1 << 4, Restrict = 1 << 5, Pointer64 = 1 << 6 };
enum class StorageClass		: uint8 { Global, PublicStatic, ProtectedStatic, PrivateStatic, FunctionLocalStatic };
enum class PointerAffinity	: uint8 { None, Pointer, Reference, RValueReference };
enum class CallingConv		: uint8 { Cdecl, Pascal, Thiscall, Stdcall, Fastcall, None, Clrcall, Eabi, Vectorcall, Regcall, DllExport = 1 << 7 };
enum class TagKind			: uint8	{ Class, Struct, Union, Enum };
enum class FuncClass		: uint16 {
	Far		= 1 << 0,
	_Kind	= 1 << 1,	Kind	= 3 * _Kind,	None	= 0 * _Kind,	Static	= 1 * _Kind,	Virtual		= 2 * _Kind,	StaticThisAdjust	= 3 * _Kind,
	_Access	= 1 << 3,	Access	= 3 * _Access,	Global	= 0 * _Access,	Public	= 1 * _Access,	Protected	= 2 * _Access,	Private				= 3 * _Access,
	ExternC = 1 << 5,	NoParameterList = 1 << 6,
	VirtualThisAdjust = 1 << 7, VirtualThisAdjustEx = 1 << 8,
};

inline FuncClass	operator*(FuncClass a, bool b)			{ return b ? a : FuncClass::None; }
inline FuncClass	operator|(FuncClass a, FuncClass b)		{ return FuncClass(int(a) | int(b)); }
inline bool			operator&(FuncClass a, FuncClass b)		{ return !!(int(a) & int(b)); }
inline FuncClass	operator/(FuncClass a, FuncClass b)		{ return FuncClass(int(a) & int(b)); }

inline Qualifiers	operator*(Qualifiers a, bool b)			{ return b ? a : Qualifiers::None; }
inline Qualifiers	operator|(Qualifiers a, Qualifiers b)	{ return Qualifiers(int(a) | int(b)); }
inline Qualifiers	operator-(Qualifiers a, Qualifiers b)	{ return Qualifiers(int(a) & ~int(b)); }
inline bool			operator&(Qualifiers a, Qualifiers b)	{ return !!(int(a) & int(b)); }

const char *Access_names[]	= {
	0,
	"public",
	"protected",
	"private",
};

enum OutputFlags {
	OF_NoCallingConvention	= 1 << 0,
	OF_NoTag				= 1 << 1,
	OF_NoMemberType			= 1 << 2,
	OF_NoThisQualifiers		= 1 << 4,
	OF_NoParameters			= 1 << 5,
	OF_NoReturn				= 1 << 6,
	OF_NoPtr64				= 1 << 7,
	OF_AccessSpecifiers		= 1 << 8,

	OF_Default				= OF_NoTag,
	OF_NameOnly				= OF_NoCallingConvention|OF_NoTag|OF_NoMemberType|OF_NoThisQualifiers|OF_NoParameters|OF_NoReturn,
};

enum class CharKind {
	Unknown,
	Char,
	Char16,
	Char32,
	Wchar,
};

enum class NodeKind {
	Unknown,
	QualifiedName,
	TemplateParameterReference,
	IntegerLiteral,

	//TypeNode
	PrimitiveType,
	FunctionSignature,
		ThunkSignature,
	PointerType,
	TagType,
	ArrayType,
	Custom,

	//IdentifierNode
	NamedIdentifier,
	VcallThunkIdentifier,
	LocalStaticGuardIdentifier,
	ConversionOperatorIdentifier,
	DynamicStructorIdentifier,
	StructorIdentifier,
	LiteralOperatorIdentifier,
	RttiBaseClassDescriptor,

	//SymbolNode
	Symbol,
	EncodedStringLiteral,
	LocalStaticGuardVariable,
	FunctionSymbol,
	VariableSymbol,
	SpecialTableSymbol,
};

void separate(string_accum &a) {
	char c = a.back();
	if (is_alphanum(c) || c == '>')
		a << ' ';
}

string_accum &operator<<(string_accum &a, CallingConv cc) {
	static const char *names[]	= {
		"__cdecl",
		"__pascal",
		"__thiscall",
		"__stdcall",
		"__fastcall",
		"",
		"__clrcall",
		"__eabi",
		"__vectorcall",
		"__regcall",
	};
	return a << names[(int)cc & 15];
}

string_accum &operator<<(string_accum &a, Qualifiers Q) {
	return a
		<< onlyif(Q & Qualifiers::Const,		" const")
		<< onlyif(Q & Qualifiers::Volatile,		" volatile")
		<< onlyif(Q & Qualifiers::Restrict,		" __restrict")
		<< onlyif(Q & Qualifiers::Pointer64,	" __ptr64");
}

class Node {
	NodeKind Kind;
public:
	explicit Node(NodeKind K) : Kind(K) {}
	NodeKind kind() const { return Kind; }
	virtual void output(string_accum& OS, OutputFlags Flags) const = 0;
};

struct NodeLink {
	Node		*N;
	NodeLink	*Next;
	NodeLink(Node *N) : N(N), Next(nullptr) {}
};

struct NodeList {
	NodeLink	*Head;
	NodeList(NodeLink *Head = nullptr) : Head(Head) {}

	explicit operator bool() const { return !!Head; }
	void output(string_accum& OS, OutputFlags Flags, const char *Separator) const {
		int	j = 0;
		for (const NodeLink *i = Head; i; i = i->Next)
			i->N->output(OS << onlyif(j++, Separator), Flags);
	}
	Node *back() const {
		const NodeLink *i = Head;
		while (auto *next = i->Next)
			i = next;
		return i->N;
	}
	size_t	size() const {
		size_t	n = 0;
		for (const NodeLink *i = Head; i; i = i->Next)
			++n;
		return n;
	}
	Node*	operator[](size_t i) const {
		const NodeLink *n = Head;
		while (i--)
			n = n->Next;
		return n->N;
	}
};

struct IntegerLiteralNode : Node {
	uint64	Value		= 0;
	bool	IsNegative	= false;
	IntegerLiteralNode() : Node(NodeKind::IntegerLiteral) {}
	void output(string_accum& OS, OutputFlags Flags) const override { OS << onlyif(IsNegative, '-') << Value; }
};

struct TypeNode : Node {
	Qualifiers Quals = Qualifiers::None;
	explicit TypeNode(NodeKind K) : Node(K) {}
	virtual void outputPre(string_accum& OS, OutputFlags Flags) const = 0;
	virtual void outputPost(string_accum& OS, OutputFlags Flags) const {};

	void output(string_accum& OS, OutputFlags Flags) const override {
		outputPre(OS, Flags);
		outputPost(OS, Flags);
	}
};

struct IdentifierNode : Node {
	NodeList	TemplateParams;
	explicit IdentifierNode(NodeKind K) : Node(K) {}
protected:
	void outputTemplateParameters(string_accum& OS, OutputFlags Flags) const {
		if (TemplateParams) {
			TemplateParams.output(OS << '<', OutputFlags(Flags & ~(OF_NoReturn|OF_NoParameters|OF_NoCallingConvention|OF_NoThisQualifiers)), ",");
			OS << '>';
		}
	}
};

struct QualifiedNameNode : Node {
	NodeList	Components;
	QualifiedNameNode(NodeLink *Components) : Node(NodeKind::QualifiedName), Components(Components) {}
	void			output(string_accum& OS, OutputFlags Flags) const override { Components.output(OS, Flags, "::"); }
	IdentifierNode* get_unqualified() { return static_cast<IdentifierNode*>(Components.back()); }
};

struct SymbolNode : Node {
	QualifiedNameNode* Name = nullptr;
	explicit SymbolNode(NodeKind K) : Node(K) {}
	SymbolNode(NodeKind K, QualifiedNameNode *Name) : Node(K), Name(Name) {}
	void output(string_accum& OS, OutputFlags Flags) const override { Name->output(OS, Flags); }
};

struct TemplateParameterReferenceNode : Node {
	SymbolNode*		Symbol				= nullptr;
	int				ThunkOffsetCount;
	int64			ThunkOffsets[3];
	PointerAffinity	Affinity;
	bool			IsMemberPointer;

	TemplateParameterReferenceNode(PointerAffinity Affinity, bool IsMemberPointer) : Node(NodeKind::TemplateParameterReference), Affinity(Affinity), IsMemberPointer(IsMemberPointer) {}
	void output(string_accum& OS, OutputFlags Flags) const override {
		if (ThunkOffsetCount > 0)
			OS << '{';
		else if (Affinity == PointerAffinity::Pointer)
			OS << '&';

		if (Symbol) {
			Symbol->output(OS, OutputFlags(Flags | OF_NoReturn | OF_NoParameters | OF_NoThisQualifiers | OF_NoCallingConvention));
			if (ThunkOffsetCount > 0)
				OS << ", ";
		}

		if (ThunkOffsetCount > 0) {
			OS << ThunkOffsets[0];
			for (int I = 1; I < ThunkOffsetCount; ++I)
				OS << ", " << ThunkOffsets[I];
			OS << "}";
		}
	}
};

// Identifier Nodes

struct VcallThunkIdentifierNode : IdentifierNode {
	uint64	OffsetInVTable = 0;
	VcallThunkIdentifierNode() : IdentifierNode(NodeKind::VcallThunkIdentifier) {}
	void	output(string_accum& OS, OutputFlags Flags) const override { OS << "`vcall'{" << OffsetInVTable << ", {flat}}"; }
};

struct NamedIdentifierNode : IdentifierNode {
	const char *Name;
	NamedIdentifierNode(const char *Name) : IdentifierNode(NodeKind::NamedIdentifier), Name(Name) {}
	void output(string_accum& OS, OutputFlags Flags) const override { outputTemplateParameters(OS << Name, Flags); }
};

struct LiteralOperatorIdentifierNode : IdentifierNode {
	const char *Name;
	LiteralOperatorIdentifierNode(const char *Name) : IdentifierNode(NodeKind::LiteralOperatorIdentifier), Name(Name) {}
	void output(string_accum& OS, OutputFlags Flags) const override { outputTemplateParameters(OS << "operator \"\"" << Name, Flags); }
};

struct LocalStaticGuardIdentifierNode : IdentifierNode {
	uint32 ScopeIndex = 0;
	LocalStaticGuardIdentifierNode() : IdentifierNode(NodeKind::LocalStaticGuardIdentifier) {}
	void output(string_accum& OS, OutputFlags Flags) const override {
		OS << "`local static guard'";
		if (ScopeIndex > 0)
			OS << '{' << ScopeIndex << '}';
	}
};

struct ConversionOperatorIdentifierNode : IdentifierNode {
	TypeNode* TargetType = nullptr;		// The type that this operator converts to
	ConversionOperatorIdentifierNode() : IdentifierNode(NodeKind::ConversionOperatorIdentifier) {}
	void output(string_accum& OS, OutputFlags Flags) const override {
		outputTemplateParameters(OS << "operator", Flags);
		TargetType->output(OS << ' ', Flags);
	}
};

struct StructorIdentifierNode : IdentifierNode {
	IdentifierNode* Class		 = nullptr;	// The name of the class that this is a structor of
	bool			IsDestructor = false;
	StructorIdentifierNode(bool IsDestructor) : IdentifierNode(NodeKind::StructorIdentifier), IsDestructor(IsDestructor) {}
	void output(string_accum& OS, OutputFlags Flags) const override {
		Class->output(OS << onlyif(IsDestructor, '~'), Flags);
		outputTemplateParameters(OS, Flags);
	}
};

struct DynamicStructorIdentifierNode : IdentifierNode {
	Node*	Name;
	bool	IsDestructor;
	DynamicStructorIdentifierNode(Node *Name, bool IsDestructor) : IdentifierNode(NodeKind::DynamicStructorIdentifier), Name(Name), IsDestructor(IsDestructor) {}
	void output(string_accum& OS, OutputFlags Flags) const override {
		Name->output(OS << ifelse(IsDestructor, "`dynamic atexit destructor for `", "`dynamic initializer for `"), Flags);
		OS << "''";
	}
};

struct RttiBaseClassDescriptorNode : IdentifierNode {
	uint32	NVOffset		= 0;
	int32	VBPtrOffset		= 0;
	uint32	VBTableOffset	= 0;
	uint32	Flags			= 0;
	RttiBaseClassDescriptorNode() : IdentifierNode(NodeKind::RttiBaseClassDescriptor) {}
	void output(string_accum& OS, OutputFlags Flags) const override {
		OS << "`RTTI Base Class Descriptor at (" << NVOffset << ',' << VBPtrOffset << ',' << VBTableOffset << ',' << this->Flags << ")'";
	}
};

//	Type Nodes

struct PrimitiveTypeNode : TypeNode {
	const char *PrimKind;
	explicit PrimitiveTypeNode(const char * K) : TypeNode(NodeKind::PrimitiveType), PrimKind(K) {}
	void outputPre(string_accum& OS, OutputFlags Flags) const { OS << PrimKind << Quals; }
};

struct TagTypeNode : TypeNode {
	TagKind			Tag;
	QualifiedNameNode* QualifiedName;
	explicit TagTypeNode(TagKind Tag, QualifiedNameNode* QualifiedName) : TypeNode(NodeKind::TagType), Tag(Tag), QualifiedName(QualifiedName) {}
	void outputPre(string_accum& OS, OutputFlags Flags) const {
		static const char *TagKind_names[] = {
			"class",
			"struct",
			"union",
			"enum",
		};
		if (!(Flags & OF_NoTag))
			OS << TagKind_names[(int)Tag] << ' ';
		if (QualifiedName)
			QualifiedName->output(OS, Flags);
		OS << Quals;
	}
};

struct ArrayTypeNode : TypeNode {
	ptr_array<uint64>	Dimensions;
	TypeNode*			ElementType	= nullptr;
	ArrayTypeNode(uint64 *p, uint64 n) : TypeNode(NodeKind::ArrayType), Dimensions(p, n) {}
	void outputPre(string_accum& OS, OutputFlags Flags) const {
		ElementType->outputPre(OS, Flags);
		OS << Quals;
	}
	void outputPost(string_accum& OS, OutputFlags Flags) const {
		if (Dimensions.size() == 0) {
			OS << "[]";
		} else {
			for (auto &i : Dimensions)
				OS << '[' << onlyif(i != 0, i) << ']';
		}
		ElementType->outputPost(OS, Flags);
	}
};

struct CustomTypeNode : TypeNode {
	/*Identifier*/Node* Identifier;
	CustomTypeNode() : TypeNode(NodeKind::Custom) {}
	void outputPre(string_accum& OS, OutputFlags Flags) const override { Identifier->output(OS, Flags); }
};

struct FunctionSignatureNode : TypeNode {
	FuncClass		FunctionClass;
	CallingConv		CallConvention	= CallingConv::None;
	PointerAffinity	RefQualifier	= PointerAffinity::None;
	TypeNode*		ReturnType		= nullptr;
	bool			IsVariadic		= false;
	NodeList		Params;

	explicit FunctionSignatureNode(NodeKind K, FuncClass FunctionClass) : TypeNode(K), FunctionClass(FunctionClass) {}
	FunctionSignatureNode(FuncClass FunctionClass) : TypeNode(NodeKind::FunctionSignature), FunctionClass(FunctionClass) {}

	void outputPre(string_accum& OS, OutputFlags Flags) const override {
		if ((Flags & OF_AccessSpecifiers) && FunctionClass / FuncClass::Access != FuncClass::Global)
			OS	<< Access_names[int(FunctionClass / FuncClass::Access) / (int)FuncClass::_Access] << ": ";

		if (!(Flags & OF_NoMemberType)) {
			OS	<< onlyif(FunctionClass / FuncClass::Access != FuncClass::Global && FunctionClass / FuncClass::Kind == FuncClass::Static, "static ")
				<< onlyif(FunctionClass & FuncClass::ExternC, "extern \"C\" ")
				<< onlyif(FunctionClass / FuncClass::Kind == FuncClass::Virtual, "virtual ");
		}
		if (ReturnType && !(Flags & OF_NoReturn))
			ReturnType->outputPre(OS, Flags);
		if (!(Flags & OF_NoCallingConvention))
			OS << separate << CallConvention;
	}
	void outputPost(string_accum& OS, OutputFlags Flags) const override {
		if (!(FunctionClass & FuncClass::NoParameterList) && !(Flags & OF_NoParameters)) {
			OS << '(';
			if (Params) {
				Params.output(OS, Flags, ", ");
				if (IsVariadic)
					OS << ", ...";
			} else {
				OS << ifelse(IsVariadic, "...", "void");
			}
			OS << ')';
		}
		if (!(Flags & OF_NoThisQualifiers)) {
			OS	<< onlyif(Quals & Qualifiers::Const,	" const")
				<< onlyif(Quals & Qualifiers::Volatile,	" volatile")
				<< onlyif(Quals & Qualifiers::Restrict,	" __restrict")
				<< onlyif(Quals & Qualifiers::Unaligned," __unaligned")
				<< onlyif((Quals & Qualifiers::Pointer64) && !(Flags & OF_NoPtr64)," __ptr64");
		}
		if (RefQualifier == PointerAffinity::Reference)
			OS << " &";
		else if (RefQualifier == PointerAffinity::RValueReference)
			OS << " &&";

		if (ReturnType && !(Flags & OF_NoReturn))
			ReturnType->outputPost(OS, Flags);
	}
};

struct ThunkSignatureNode : FunctionSignatureNode {
	uint32	StaticOffset	= 0;
	int32	VBPtrOffset		= 0;
	int32	VBOffsetOffset	= 0;
	int32	VtordispOffset	= 0;

	ThunkSignatureNode(FuncClass FunctionClass = FuncClass::None) : FunctionSignatureNode(NodeKind::ThunkSignature, FunctionClass) {}
	void outputPre(string_accum& OS, OutputFlags Flags) const override {
		FunctionSignatureNode::outputPre(OS << "[thunk]: ", Flags);
	}
	void outputPost(string_accum& OS, OutputFlags Flags) const override {
		if (FunctionClass / FuncClass::Kind == FuncClass::StaticThisAdjust || (FunctionClass & FuncClass::VirtualThisAdjust)) {
			if (FunctionClass & FuncClass::VirtualThisAdjust) {
				if (FunctionClass & FuncClass::VirtualThisAdjustEx)
					OS << "`vtordispex{" << VBPtrOffset << ',' << VBOffsetOffset << ',';
				else
					OS << "`vtordisp{";
				OS << VtordispOffset << ',';
			} else {
				OS << "`adjustor{";
			}
			OS << StaticOffset << "}'";
		}
		FunctionSignatureNode::outputPost(OS, Flags);
	}
};

struct PointerTypeNode : TypeNode {
	PointerAffinity		Affinity;				// Is this a pointer, reference, or rvalue-reference?
	QualifiedNameNode*	ClassParent	= nullptr;	// If this is a member pointer, this is the class that the member is in
	TypeNode*			Pointee		= nullptr;	// Represents a type X in "a pointer to X", "a reference to X", or "rvalue-reference to X"

	PointerTypeNode(PointerAffinity Affinity) : TypeNode(NodeKind::PointerType), Affinity(Affinity) { }
	void outputPre(string_accum& OS, OutputFlags Flags) const override {
		Pointee->outputPre(OS, Pointee->kind() == NodeKind::FunctionSignature ? OutputFlags(Flags | OF_NoCallingConvention) : Flags);	// don't output the calling convention; it needs to go inside the parentheses
		OS << separate;

		if (Quals & Qualifiers::Unaligned)
			OS << "__unaligned ";

		if (is_any(Pointee->kind(), NodeKind::ArrayType, NodeKind::FunctionSignature)) {
			OS << '(';
			if (Pointee->kind() == NodeKind::FunctionSignature && !(Flags & OF_NoCallingConvention))
				OS << static_cast<const FunctionSignatureNode*>(Pointee)->CallConvention << ' ';
		}

		if (ClassParent) {
			ClassParent->output(OS, Flags);
			OS << "::";
		}

		switch (Affinity) {
			case PointerAffinity::Pointer:			OS << '*'; break;
			case PointerAffinity::Reference:		OS << '&'; break;
			case PointerAffinity::RValueReference:	OS << "&&"; break;
		}
		OS << (Flags & OF_NoPtr64 ? Quals - Qualifiers::Pointer64 : Quals);
	}
	void outputPost(string_accum& OS, OutputFlags Flags) const override {
		if (is_any(Pointee->kind(), NodeKind::ArrayType, NodeKind::FunctionSignature))
			OS << ')';
		Pointee->outputPost(OS, Flags);
	}
};

// Symbol Nodes

struct VariableSymbolNode : SymbolNode {
	StorageClass SC;
	TypeNode*	Type;
	VariableSymbolNode(QualifiedNameNode *Name, TypeNode *Type = 0, StorageClass SC = StorageClass::Global) : SymbolNode(NodeKind::VariableSymbol, Name), SC(SC), Type(Type) {}
	void output(string_accum& OS, OutputFlags Flags) const override {
		if ((Flags & OF_AccessSpecifiers) && ((int)SC & 3))
			OS	<< Access_names[(int)SC & 3] << ": ";
		if (is_any(SC, StorageClass::PrivateStatic, StorageClass::PublicStatic, StorageClass::ProtectedStatic))
			OS << "static ";
		if (Type) {
			Type->outputPre(OS, Flags);
			OS << separate;
		}
		Name->output(OS, Flags);
		if (Type)
			Type->outputPost(OS, Flags);
	}
};

struct SpecialTableSymbolNode : SymbolNode {
	QualifiedNameNode*	TargetName	= nullptr;
	Qualifiers			Quals		= Qualifiers::None;
	explicit SpecialTableSymbolNode(QualifiedNameNode *Name) : SymbolNode(NodeKind::SpecialTableSymbol, Name) {}
	void output(string_accum& OS, OutputFlags Flags) const override {
		Name->output(OS << Quals << separate, Flags);
		if (TargetName) {
			TargetName->output(OS << "{for `", Flags);
			OS << "'}";
		}
		return;
	}
};

struct LocalStaticGuardVariableNode : SymbolNode {
	bool IsVisible = false;
	LocalStaticGuardVariableNode(QualifiedNameNode *Name) : SymbolNode(NodeKind::LocalStaticGuardVariable, Name) {}
	void output(string_accum& OS, OutputFlags Flags) const { Name->output(OS, Flags); }
};

struct EncodedStringLiteralNode : SymbolNode {
	void		*StringBytes;
	uint32		BytesDecoded;
	uint32		StringByteSize;
	CharKind	Char;
	EncodedStringLiteralNode(void *StringBytes, uint32 BytesDecoded, uint32 StringByteSize, CharKind Char) : SymbolNode(NodeKind::EncodedStringLiteral), StringBytes(StringBytes), BytesDecoded(BytesDecoded), StringByteSize(StringByteSize), Char(Char) {}
	template<typename C> void output(string_accum& OS, const char *name, const char *prefix) const {
		uint32	len		= StringByteSize / sizeof(C);
		uint32	decoded	= BytesDecoded / sizeof(C);
		if (decoded == len && ((C*)StringBytes)[decoded - 1] == 0)
			--decoded;

		OS << "const " << name << '[' << len << "] {" << prefix << "\"" << escaped(str((C*)	StringBytes, decoded)) << '"' << onlyif(len - 1 > decoded, "...") << '}';
	}
	void output(string_accum& OS, OutputFlags Flags) const override {
		switch (Char) {
			case CharKind::Wchar:	output<char16be>(OS, "wchar_t", "L");	break;
			case CharKind::Char:	output<char>	(OS, "char",	"");	break;
			case CharKind::Char16:	output<char16>	(OS, "char16_t","u");	break;
			case CharKind::Char32:	output<char32>	(OS, "char32_t","U");	break;
		}
	}
};

struct FunctionSymbolNode : SymbolNode {
	FunctionSignatureNode* Signature = nullptr;
	FunctionSymbolNode(QualifiedNameNode *Name, FunctionSignatureNode* Signature) : SymbolNode(NodeKind::FunctionSymbol, Name), Signature(Signature) {}
	void output(string_accum& OS, OutputFlags Flags) const override {
		Signature->outputPre(OS, Flags);
		Name->output(OS << separate, Flags);
		Signature->outputPost(OS, Flags);
	}
};

//-----------------------------------------------------------------------------
//	parser
//-----------------------------------------------------------------------------

static bool get_number(string_scan& mangled, uint64 &val) {
	char c	= mangled.getc();
	if (is_digit(c)) {
		val = c - '0' + 1;
		return true;
	}
	val = 0;
	while (c != '@') {
		if (!between(c, 'A', 'P'))
			return false;
		val = (val << 4) + (c - 'A');
		c = mangled.getc();
	}
	return true;
}
static bool get_number(string_scan& mangled, uint32 &val) {
	uint64	val64;
	return get_number(mangled, val64) && (val = uint32(val64)) == val64;
}
static bool get_number(string_scan& mangled, int64 &val) {
	bool neg = mangled.check('?');
	uint64 uval;
	if (get_number(mangled, uval) && (int64)uval >= 0) {
		val	= neg ? -int64(uval) : int64(uval);
		return true;
	}
	return false;
}
static bool get_number(string_scan& mangled, int32 &val) {
	int64 val64;
	return get_number(mangled, val64) && (val = (int32)val64) == val64;
}

static bool char_literal(string_scan& mangled, char &c) {
	c = mangled.getc();
	if (c != '?')
		return true;

	char	x = mangled.getc();
	if (x == '$') {
		// Two hex digits
		char	c1 = mangled.getc();
		char	c2 = mangled.getc();
		if (between(c1, 'A', 'P') && between(c2, 'A', 'P')) {
			c = ((c1 - 'A') << 4) | (c2 - 'A');
			return true;
		}
	} else if (is_digit(x)) {
		const char* Lookup = ",/\\:. \n\t'-";
		c	= Lookup[x - '0'];
		return true;

	} else if (is_alpha(x)) {
		c	= x - 'A' + 0xc1;
		return true;
	}
	return false;
}

static bool qualifiers(string_scan& mangled, Qualifiers &qual) {
	char	c	= mangled.getc();
	qual		= Qualifiers((c - 'A') & 3);
	return ISO_VERIFY(between(c, 'A', 'D'));
}

static bool maybe_qualifiers(string_scan& mangled, Qualifiers &qual) {
	return !mangled.check('?') || qualifiers(mangled, qual);
}

static Qualifiers pointer_qualifiers(string_scan& mangled) {
	auto  Q = Qualifiers::Pointer64	* mangled.check('E');
	Q	= Q | Qualifiers::Restrict	* mangled.check('I');
	Q	= Q | Qualifiers::Unaligned	* mangled.check('F');
	return Q;
}

static bool calling_convention(string_scan& mangled, CallingConv &cconv) {
	char	c = mangled.getc();
	if (between(c, 'A', 'Q')) {
		cconv = CallingConv((c - 'A') / 2 + (((c - 'A') & 1) * (int)CallingConv::DllExport));
		return true;
	}
	return false;
}

struct BackrefContext {
	TypeNode*				Types[10];
	size_t					TypesCount = 0;
	NamedIdentifierNode*	Names[10];
	size_t					NamesCount = 0;
	NamedIdentifierNode*	Name(int i) const { return i < NamesCount ? Names[i] : nullptr; }
	TypeNode*				Type(int i)	const { return i < TypesCount ? Types[i] : nullptr; }
};

class Demangler {
	BackrefContext			Backrefs;
	arena_allocator<4096>	arena;

	const char *String(const char *p, const char *e) {
		char	*S = (char*)arena.alloc(e - p + 1);
		memcpy(S, p, e - p);
		S[e - p] = 0;
		return S;
	}
	const char *String(const count_string &s) {
		return String(s.begin(), s.end());
	}
	void MemorizeString(const char *s) {
		if (Backrefs.NamesCount < 10) {
			for (size_t i = 0; i < Backrefs.NamesCount; ++i)
				if (strcmp(s, Backrefs.Names[i]->Name) == 0)
					return;
			Backrefs.Names[Backrefs.NamesCount++] = arena.make<NamedIdentifierNode>(s);
		}
	}
	IdentifierNode *MemorizeIdentifier(IdentifierNode* Identifier) {
		if (Identifier) {
			string_builder OS;
			Identifier->output(OS, OF_Default);
			MemorizeString(String(OS));
		}
		return Identifier;
	}
	void MemorizeType(TypeNode *type) {
		if (Backrefs.TypesCount < 10)
			Backrefs.Types[Backrefs.TypesCount++] = type;
	}
	const char *SimpleString(string_scan& mangled) {
		const char *p	= mangled.getp();
		if (const char *e = mangled.scan_skip('@'))
			return String(p, e - 1);
		return nullptr;
	}
	NamedIdentifierNode* SimpleName(string_scan& mangled) {
		if (auto s = SimpleString(mangled)) {
			MemorizeString(s);
			return arena.make<NamedIdentifierNode>(s);
		}
		return nullptr;
	}
	IdentifierNode* UnqualifiedTypeName(string_scan& mangled) {
		return	is_digit(mangled.peekc())	? Backrefs.Name(mangled.getc() - '0')
			:	mangled.check("?$")			? MemorizeIdentifier(TemplateInstantiation(mangled))
			:	SimpleName(mangled);
	}
	IdentifierNode* UnqualifiedSymbolName(string_scan& mangled) {
		return	is_digit(mangled.peekc())	? Backrefs.Name(mangled.getc() - '0')
			:	mangled.check("?$")			? TemplateInstantiation(mangled)
			:	mangled.check('?')			? SpecialIdentifier(mangled)
			:	SimpleName(mangled);
	}
	QualifiedNameNode* SynthesizeQualifiedName(Node* Identifier) {
		return arena.make<QualifiedNameNode>(arena.make<NodeLink>(Identifier));
	}
	QualifiedNameNode* SynthesizeQualifiedName(const char *Name) {
		return SynthesizeQualifiedName(arena.make<NamedIdentifierNode>(Name));
	}
	QualifiedNameNode* FullyQualifiedTypeName(string_scan& mangled) {
		return NameScopeChain(mangled, UnqualifiedTypeName(mangled));
	}
	QualifiedNameNode* FullyQualifiedSymbolName(string_scan& mangled) {
		if (IdentifierNode* Identifier = UnqualifiedSymbolName(mangled)) {
			QualifiedNameNode* QN = NameScopeChain(mangled, Identifier);
			if (QN && Identifier->kind() == NodeKind::StructorIdentifier) {
				ISO_ASSERT(QN->Components.size() >= 2);
				static_cast<StructorIdentifierNode*>(Identifier)->Class = static_cast<IdentifierNode*>(QN->Components[QN->Components.size() - 2]);
			}
			return QN;
		}
		return nullptr;
	}

	bool							ThrowSpecification(string_scan& mangled);
	IdentifierNode*					NameScopePiece(string_scan& mangled);
	QualifiedNameNode*				NameScopeChain(string_scan& mangled, Node* UnqualifiedName);
	TemplateParameterReferenceNode*	TemplateParameter(string_scan& mangled, PointerAffinity Affinity, bool IsMemberPointer, int ThunkOffsetCount);
	IdentifierNode*					TemplateInstantiation(string_scan& mangled);
	EncodedStringLiteralNode*		StringLiteral(string_scan& mangled);
	PointerTypeNode*				PointerType(string_scan& mangled, Qualifiers qual, PointerAffinity aff);
	FunctionSignatureNode*			FunctionType(string_scan& mangled, FunctionSignatureNode *FTy);
	FunctionSignatureNode*			FunctionType(string_scan& mangled, FuncClass FC) { return FunctionType(mangled, new FunctionSignatureNode(FC)); }
	IdentifierNode*					SpecialIdentifier(string_scan& mangled);

	TypeNode*						Type(string_scan& mangled, Qualifiers Quals);
	VariableSymbolNode*				Variable(string_scan& mangled, QualifiedNameNode* Name, StorageClass SC);
	SymbolNode*						Symbol(string_scan& mangled, QualifiedNameNode* QN, OutputFlags Flags = OF_Default);

public:
	SymbolNode* parse(string_scan& mangled, OutputFlags Flags = OF_Default);
};

IdentifierNode* Demangler::SpecialIdentifier(string_scan& mangled) {
	static const char *Basic[36] = {
		0,						// ?0 # Foo::Foo()
		0,						// ?1 # Foo::~Foo()
		"operator new",			// ?2	New,
		"operator delete",		// ?3	Delete,
		"operator=",			// ?4	Assign,
		"operator>>",			// ?5	RightShift,
		"operator<<",			// ?6	LeftShift,
		"operator!",			// ?7	LogicalNot,
		"operator==",			// ?8	Equals,
		"operator!=",			// ?9	NotEquals,
		"operator[]",			// ?A	ArraySubscript,
		"operator",				// ?B	conversion
		"operator->",			// ?C	Pointer,
		"operator*",			// ?D	Dereference,
		"operator++",			// ?E	Increment,
		"operator--",			// ?F	Decrement,
		"operator-",			// ?G	Minus,
		"operator+",			// ?H	Plus,
		"operator&",			// ?I	BitwiseAnd,
		"operator->*",			// ?J	MemberPointer,
		"operator/",			// ?K	Divide,
		"operator%",			// ?L	Modulus,
		"operator<",			// ?M	LessThan,
		"operator<=",			// ?N	LessThanEqual,
		"operator>",			// ?O	GreaterThan,
		"operator>=",			// ?P	GreaterThanEqual,
		"operator,",			// ?Q	Comma,
		"operator()",			// ?R	Parens,
		"operator~",			// ?S	BitwiseNot,
		"operator^",			// ?T	BitwiseXor,
		"operator|",			// ?U	BitwiseOr,
		"operator&&",			// ?V	LogicalAnd,
		"operator||",			// ?W	LogicalOr,
		"operator*=",			// ?X	TimesEqual,
		"operator+=",			// ?Y	PlusEqual,
		"operator-=",			// ?Z	MinusEqual,
	};
	static const char *Under[36] = {
		"operator/=",			// ?_0
		"operator%=",			// ?_1
		"operator>>=",			// ?_2
		"operator<<=",			// ?_3
		"operator&=",			// ?_4
		"operator|=",			// ?_5
		"operator^=",			// ?_6
		"`vftable'",			// ?_7 # vftable
		"`vbtable'",			// ?_8 # vbtable
		0,						// ?_9 # vcall
		0,						// ?_A # typeof
		0,						// ?_B # local static guard
		0,						// ?_C # string literal
		"`vbase dtor'",			// ?_D
		"`vector deleting dtor'",			// ?_E
		"`default ctor closure'",			// ?_F
		"`scalar deleting dtor'",			// ?_G
		"`vector ctor iterator'",			// ?_H
		"`vector dtor iterator'",			// ?_I
		"`vector vbase ctor iterator'",		// ?_J
		"`virtual displacement map'",		// ?_K
		"`eh vector ctor iterator'",		// ?_L
		"`eh vector dtor iterator'",		// ?_M
		"`eh vector vbase ctor iterator'",	// ?_N
		"`copy ctor closure'",				// ?_O
		0,									// ?_P<name> # udt returning <name>
		0,									// ?_Q # <unknown>
		0,									// ?_R0 - ?_R4 # RTTI Codes
		"`local vftable'",					// ?_S # local vftable
		"`local vftable ctor closure'",		// ?_T
		"operator new[]",					// ?_U
		"operator delete[]",				// ?_V
		0,						// ?_W <unused>
		0,						// ?_X <unused>
		0,						// ?_Y <unused>
		0,						// ?_Z <unused>
	};
	static const char *DoubleUnder[36] = {
		0,						// ?__0 <unused>
		0,						// ?__1 <unused>
		0,						// ?__2 <unused>
		0,						// ?__3 <unused>
		0,						// ?__4 <unused>
		0,						// ?__5 <unused>
		0,						// ?__6 <unused>
		0,						// ?__7 <unused>
		0,						// ?__8 <unused>
		0,						// ?__9 <unused>
		"`managed vector ctor iterator'",					// ?__A
		"`managed vector dtor iterator'",					// ?__B
		"`EH vector copy ctor iterator'",					// ?__C
		"`EH vector vbase copy ctor iterator'",				// ?__D
		0,													// ?__E dynamic initializer for `T'
		0,													// ?__F dynamic atexit destructor for `T'
		"`vector copy ctor iterator'",						// ?__G
		"`vector vbase copy constructor iterator'",			// ?__H
		"`managed vector vbase copy constructor iterator'",	// ?__I
		0,						// ?__J local static thread guard
		0,						// ?__K operator""_name
		"co_await",				// ?__L
		0,						// ?__M <unused>
		0,						// ?__N <unused>
		0,						// ?__O <unused>
		0,						// ?__P <unused>
		0,						// ?__Q <unused>
		0,						// ?__R <unused>
		0,						// ?__S <unused>
		0,						// ?__T <unused>
		0,						// ?__U <unused>
		0,						// ?__V <unused>
		0,						// ?__W <unused>
		0,						// ?__X <unused>
		0,						// ?__Y <unused>
		0,						// ?__Z <unused>
	};

	if (mangled.check('_')) {
		if (mangled.check('_')) {
			switch (int i = from_digit(mangled.getc())) {
	/*__K*/		case 20:	return arena.make<LiteralOperatorIdentifierNode>(SimpleString(mangled));
				default:	return arena.make<NamedIdentifierNode>(DoubleUnder[i]);
			}
		}
		switch (int i = from_digit(mangled.getc())) {
	/*_R*/	case 27:
				switch (mangled.getc()) {
					case '0': arena.make<NamedIdentifierNode>("`RTTI Type Descriptor'");
					case '1': {
						RttiBaseClassDescriptorNode* RBCDN = arena.make<RttiBaseClassDescriptorNode>();
						if (get_number(mangled,	RBCDN->NVOffset) && get_number(mangled,	RBCDN->VBPtrOffset) && get_number(mangled, RBCDN->VBTableOffset) &&	get_number(mangled,	RBCDN->Flags))
							return RBCDN;
						return nullptr;
					}
					case '2':	return arena.make<NamedIdentifierNode>("`RTTI Base Class Array'");
					case '3':	return arena.make<NamedIdentifierNode>("`RTTI Class Hierarchy Descriptor'");
					case '4':	return arena.make<NamedIdentifierNode>("`RTTI Complete Object Locator'");
				}
			default:	return arena.make<NamedIdentifierNode>(Under[i]);
		}

	} else {
		switch (int i = from_digit(mangled.getc())) {
			case 0:
			case 1:		return arena.make<StructorIdentifierNode>(i);
	/*B*/	case 11:	return arena.make<ConversionOperatorIdentifierNode>();
			default:	return arena.make<NamedIdentifierNode>(Basic[i]);
		}
	}
	// No Mangling Yet: Spaceship, // operator<=>
}

VariableSymbolNode* Demangler::Variable(string_scan& mangled, QualifiedNameNode* Name, StorageClass SC) {
	TypeNode	*VT			= Type(mangled, Qualifiers::None);

	if (VT->kind() == NodeKind::PointerType) {
		PointerTypeNode*	PTN				= static_cast<PointerTypeNode*>(VT);
		Qualifiers			ExtraChildQuals	= Qualifiers::None;
		PTN->Quals							= VT->Quals | pointer_qualifiers(mangled);

		if (!qualifiers(mangled, ExtraChildQuals))
			return nullptr;

		if (PTN->ClassParent) {
			QualifiedNameNode* BackRefName = FullyQualifiedTypeName(mangled);
			(void)BackRefName;
		}
		PTN->Pointee->Quals = PTN->Pointee->Quals | ExtraChildQuals;

	} else if (!qualifiers(mangled, VT->Quals)) {
		return nullptr;
	}

	return arena.make<VariableSymbolNode>(Name, VT, SC);
}

TemplateParameterReferenceNode* Demangler::TemplateParameter(string_scan& mangled, PointerAffinity Affinity, bool IsMemberPointer, int ThunkOffsetCount) {
	TemplateParameterReferenceNode* TPRN = arena.make<TemplateParameterReferenceNode>(Affinity, IsMemberPointer);
	if (mangled.begins('?')) {
		if (TPRN->Symbol = parse(mangled))
			MemorizeIdentifier(TPRN->Symbol->Name->get_unqualified());
	}
	for (int i = 0; i < ThunkOffsetCount; ++i)
		if (!get_number(mangled, TPRN->ThunkOffsets[i]))
			return nullptr;

	TPRN->ThunkOffsetCount = ThunkOffsetCount;
	return TPRN;
}

IdentifierNode* Demangler::TemplateInstantiation(string_scan& mangled) {
	BackrefContext OuterContext;
	swap(OuterContext, Backrefs);

	IdentifierNode* Identifier = UnqualifiedSymbolName(mangled);
	if (!Identifier)
		return nullptr;

	for (NodeLink **Current = &Identifier->TemplateParams.Head; !mangled.check('@'); Current = &(*Current)->Next) {
		Node	*TN	= nullptr;

		if (mangled.check('$')) {
			switch (char c = mangled.getc()) {
				case '$': {
					if (mangled.check('S') || mangled.check('V') || mangled.check('Z') || mangled.check("$V"))
						continue;								// parameter pack separator

					if (mangled.check('Y')) {
						TN = FullyQualifiedTypeName(mangled);	// Template alias
					} else if (mangled.check('B')) {
						TN = Type(mangled, Qualifiers::None);	// Array
					} else if (mangled.check('C')) {			// Type with qualifiers
						Qualifiers Quals = Qualifiers::None;
						if (qualifiers(mangled, Quals))
							TN = Type(mangled, Quals);
					} else {
						TN = Type(mangled.move(-2), Qualifiers::None);	// other $$ types
					}
					break;
				}

				case '1':	TN = TemplateParameter(mangled, PointerAffinity::Pointer,	true,	0);	break;	// 1 - single inheritance		<name>
				case 'H':	TN = TemplateParameter(mangled, PointerAffinity::Pointer,	true,	1);	break;	// H - multiple inheritance		<name> <number>
				case 'I':	TN = TemplateParameter(mangled, PointerAffinity::Pointer,	true,	2);	break;	// I - virtual inheritance		<name> <number> <number> <number>
				case 'J':	TN = TemplateParameter(mangled, PointerAffinity::Pointer,	true,	3);	break;	// J - unspecified inheritance	<name> <number> <number> <number>
				case 'E':	TN = TemplateParameter(mangled, PointerAffinity::Reference, false,	0);	break;	// Reference to symbol
				case 'F':	TN = TemplateParameter(mangled, PointerAffinity::None,		true,	2);	break;	// Data member pointer
				case 'G':	TN = TemplateParameter(mangled, PointerAffinity::None,		true,	3);	break;	// Data member pointer

				case '0': {	// Integral non-type template parameter
					IntegerLiteralNode	*ILN = arena.make<IntegerLiteralNode>();
					ILN->IsNegative	= mangled.check('?');
					if (get_number(mangled, ILN->Value))
						TN = ILN;
					break;
				}
			}
		} else {
			TN = Type(mangled, Qualifiers::None);
		}

		if (!TN)
			return nullptr;

		*Current = arena.make<NodeLink>(TN);
	}

	swap(OuterContext, Backrefs);
	return Identifier;
}

static CharKind guess_char_kind(const char* StringBytes, uint32 NumChars, uint32 NumBytes) {
	if (NumBytes % 2 == 1) {
		// If the number of bytes is odd, this is guaranteed to be a char string
		return CharKind::Char;
	} else if (NumBytes < 32) {
		// All strings can encode at most 32 bytes of data; if it's less than that, we can check for size of null terminator
		uint32	 TrailingNulls = 0;
		for (const char* e = StringBytes + NumChars; --e >= StringBytes && *e == 0;)
			++TrailingNulls;
		return TrailingNulls >= 4 ? CharKind::Char32 : TrailingNulls >= 2 ? CharKind::Char16 : CharKind::Char;
	} else {
		// guess using number of embedded nulls - if more than 2/3 are null, it's a char32, if more than 1/3 are null, it's a char16, otherwise it's a char8
		uint32 Nulls = 0;
		for (auto p = StringBytes, e = p + NumChars; p != e; ++p)
			Nulls += (*p == 0);
		return	Nulls >= 2 * NumChars / 3 ? CharKind::Char32 :	Nulls >= NumChars / 3 ? CharKind::Char16 : CharKind::Char;
	}
}

EncodedStringLiteralNode* Demangler::StringLiteral(string_scan& mangled) {
	if (!mangled.check("@_"))
		return nullptr;

	CharKind	Char	= CharKind::Unknown;
	switch (mangled.getc()) {
		case '1': Char = CharKind::Wchar; break;
		case '0': break;
		default: return nullptr;
	}

	// Encoded Length
	uint64		StringByteSize;
	if (!get_number(mangled, StringByteSize))
		return nullptr;

	// CRC32 (always 8 characters plus a terminator)
	if (!mangled.scan_skip('@'))
		return nullptr;

	char	*StringBytes	= (char*)arena.alloc(StringByteSize);
	uint32	BytesDecoded	= 0;
	while (!mangled.check('@') && BytesDecoded < StringByteSize) {
		if (!char_literal(mangled, StringBytes[BytesDecoded++]))
			return nullptr;
	}

	if (Char == CharKind::Unknown)
		Char = guess_char_kind(StringBytes, BytesDecoded, StringByteSize);

	return arena.make<EncodedStringLiteralNode>(StringBytes, BytesDecoded, StringByteSize, Char);
}

IdentifierNode* Demangler::NameScopePiece(string_scan& mangled) {
	if (is_digit(mangled.peekc()))
		return Backrefs.Name(mangled.getc() - '0');

	string_scan	save_mangled = mangled;
	uint64		Number;

	if (mangled.check('?')) {
		if (mangled.check('$')) {
			return MemorizeIdentifier(TemplateInstantiation(mangled));

		} else if (mangled.check('A')) {
			// anonymous namespace
			if (const char *s = SimpleString(mangled)) {
				MemorizeString(s);
				return arena.make<NamedIdentifierNode>("`anonymous namespace'");
			}

		} else if (get_number(mangled, Number) && mangled.check('?')) {
			//local scope
			if (Node* Scope = parse(mangled)) {
				string_builder OS;
				Scope->output(OS << '`', OF_Default);
				OS << "'::`" << Number << '\'';
				return arena.make<NamedIdentifierNode>(String(OS));
			}
		}
	}

	mangled = save_mangled;
	return SimpleName(mangled);
}

QualifiedNameNode* Demangler::NameScopeChain(string_scan& mangled, Node* UnqualifiedName) {
	if (!UnqualifiedName)
		return nullptr;

	NodeLink* head = arena.make<NodeLink>(UnqualifiedName);
	while (!mangled.check("@")) {
		if (Node* elem = NameScopePiece(mangled)) {
			NodeLink* newhead	= arena.make<NodeLink>(elem);
			newhead->Next		= head;
			head				= newhead;
		} else {
			return nullptr;
		}
	}
	return arena.make<QualifiedNameNode>(head);
}

PointerTypeNode* Demangler::PointerType(string_scan& mangled, Qualifiers Quals, PointerAffinity aff) {
	PointerTypeNode* Pointer	= arena.make<PointerTypeNode>(aff);
	Pointer->Quals				= Quals | pointer_qualifiers(mangled);

	switch (char c = mangled.getc()) {
		case '8':										// member function pointer
			Pointer->ClassParent	= FullyQualifiedTypeName(mangled);
			Pointer->Pointee		= FunctionType(mangled, FuncClass::Public);
			break;

		case '6':										// function pointer
			Pointer->Pointee		= FunctionType(mangled, FuncClass::None);
			break;

		case 'A': case 'B': case 'C': case 'D':			//non member
			Pointer->Pointee		= Type(mangled, Qualifiers(c - 'A'));
			break;

		case 'Q': case 'R': case 'S': case 'T':			//member
			Pointer->ClassParent	= FullyQualifiedTypeName(mangled);
			Pointer->Pointee		= Type(mangled, Qualifiers::None);
			Pointer->Pointee->Quals = Qualifiers(c - 'Q');
			break;

		default:
			return nullptr;
	}
	return Pointer;
}

bool Demangler::ThrowSpecification(string_scan& mangled) {
	return mangled.check('Z');
}

FunctionSignatureNode* Demangler::FunctionType(string_scan& mangled, FunctionSignatureNode* FTy) {
	if (FTy->FunctionClass / FuncClass::Access != FuncClass::Global && FTy->FunctionClass / FuncClass::Kind != FuncClass::Static) {
		FTy->Quals			= pointer_qualifiers(mangled);
		FTy->RefQualifier	= mangled.check('G') ? PointerAffinity::Reference
							: mangled.check('H') ? PointerAffinity::RValueReference
							: PointerAffinity::Pointer;

		Qualifiers Quals	= Qualifiers::None;
		if (!qualifiers(mangled, Quals))
			return nullptr;
		FTy->Quals			= FTy->Quals | Quals;
	}

	if (!calling_convention(mangled, FTy->CallConvention))
		return nullptr;

	if (!mangled.check('@')) {
		Qualifiers Quals	= Qualifiers::None;
		if (!maybe_qualifiers(mangled, Quals) || !(FTy->ReturnType = Type(mangled, Quals)))
			return nullptr;
	}

	if (!mangled.check('X')) {
		for (NodeLink** Current = &FTy->Params.Head; !mangled.check('@') && !(FTy->IsVariadic = mangled.check('Z')); Current = &(*Current)->Next) {
			TypeNode* TN;

			if (is_digit(mangled.peekc())) {
				TN	= Backrefs.Type(mangled.getc() - '0');

			} else {
				size_t OldSize = mangled.remaining();
				TN	= Type(mangled, Qualifiers::None);
				if (OldSize - mangled.remaining() > 1)	// don't save single-letter types
					MemorizeType(TN);
			}
			if (!TN)
				return nullptr;

			*Current = arena.make<NodeLink>(TN);
		}
	}

	return ThrowSpecification(mangled) ? FTy : nullptr;
}

TypeNode* Demangler::Type(string_scan& mangled, Qualifiers Quals) {
	TypeNode*	Ty	= nullptr;
	switch (char c = mangled.getc()) {
		case '?': {
			CustomTypeNode* CTN = arena.make<CustomTypeNode>();
			CTN->Identifier		= UnqualifiedTypeName(mangled);
			if (!mangled.check('@'))
				return nullptr;
			Ty = CTN;
			break;
		}
		case '$':
			if (mangled.check("$T"))
				Ty = arena.make<PrimitiveTypeNode>("nullptr_t");
			if (mangled.check("$Q"))	//T&&
				Ty = PointerType(mangled, Qualifiers::None, PointerAffinity::RValueReference);
			else if (mangled.check("$A8@@"))
				Ty = FunctionType(mangled, FuncClass::Public);
			else if (mangled.check("$A6"))
				Ty = FunctionType(mangled, FuncClass::None);
			break;

		//pointer types
		case 'A': Ty = PointerType(mangled, Qualifiers::None, PointerAffinity::Reference); break;
		case 'P': Ty = PointerType(mangled, Qualifiers::None, PointerAffinity::Pointer); break;
		case 'Q': Ty = PointerType(mangled, Qualifiers::Const, PointerAffinity::Pointer); break;
		case 'R': Ty = PointerType(mangled, Qualifiers::Volatile, PointerAffinity::Pointer); break;
		case 'S': Ty = PointerType(mangled, Qualifiers::Const | Qualifiers::Volatile, PointerAffinity::Pointer); break;

		//tag types
		case 'T': Ty = arena.make<TagTypeNode>(TagKind::Union, FullyQualifiedTypeName(mangled)); break;
		case 'U': Ty = arena.make<TagTypeNode>(TagKind::Struct, FullyQualifiedTypeName(mangled)); break;
		case 'V': Ty = arena.make<TagTypeNode>(TagKind::Class, FullyQualifiedTypeName(mangled)); break;
		case 'W':
			if (mangled.getc() == '4')
				Ty = arena.make<TagTypeNode>(TagKind::Enum, FullyQualifiedTypeName(mangled));
			break;

		case 'Y': {// array type
			uint64	Rank	= 0;
			if (!get_number(mangled, Rank) || Rank == 0)
				return nullptr;

			uint64			*dims	= arena.alloc<uint64>(Rank);
			ArrayTypeNode	*ATy	= arena.make<ArrayTypeNode>(dims, Rank);
			for (uint64 I = 0; I < Rank; ++I) {
				if (!get_number(mangled, dims[I]))
					return nullptr;
			}
			if (mangled.check("$$C")) {
				if (!qualifiers(mangled, ATy->Quals))
					return nullptr;
			}

			ATy->ElementType = Type(mangled, Qualifiers::None);
			Ty = ATy;
			break;
		}

		// primitive types
		case 'C': Ty = arena.make<PrimitiveTypeNode>("signed char"); break;
		case 'D': Ty = arena.make<PrimitiveTypeNode>("char"); break;
		case 'E': Ty = arena.make<PrimitiveTypeNode>("unsigned char"); break;
		case 'F': Ty = arena.make<PrimitiveTypeNode>("short"); break;
		case 'G': Ty = arena.make<PrimitiveTypeNode>("unsigned short"); break;
		case 'H': Ty = arena.make<PrimitiveTypeNode>("int"); break;
		case 'I': Ty = arena.make<PrimitiveTypeNode>("unsigned int"); break;
		case 'J': Ty = arena.make<PrimitiveTypeNode>("long"); break;
		case 'K': Ty = arena.make<PrimitiveTypeNode>("unsigned long"); break;
		case 'M': Ty = arena.make<PrimitiveTypeNode>("float"); break;
		case 'N': Ty = arena.make<PrimitiveTypeNode>("double"); break;
		case 'O': Ty = arena.make<PrimitiveTypeNode>("long double"); break;
		case 'X': Ty = arena.make<PrimitiveTypeNode>("void"); break;
		case '_': {
			switch (mangled.getc()) {
				case 'J': Ty = arena.make<PrimitiveTypeNode>("__int64"); break;
				case 'K': Ty = arena.make<PrimitiveTypeNode>("unsigned __int64"); break;
				case 'N': Ty = arena.make<PrimitiveTypeNode>("bool"); break;
				case 'S': Ty = arena.make<PrimitiveTypeNode>("char16_t"); break;
				case 'U': Ty = arena.make<PrimitiveTypeNode>("char32_t"); break;
				case 'W': Ty = arena.make<PrimitiveTypeNode>("wchar_t"); break;
			}
			break;
		}
	}

	if (Ty)
		Ty->Quals = Ty->Quals | Quals;
	return Ty;
}

SymbolNode* Demangler::Symbol(string_scan& mangled, QualifiedNameNode* Name, OutputFlags Flags) {
	if (!Name)
		return nullptr;

	FuncClass FC = FuncClass::ExternC * mangled.check("$$J0");
	switch (char c = mangled.getc()) {
		case '0': return Variable(mangled, Name, StorageClass::PrivateStatic);
		case '1': return Variable(mangled, Name, StorageClass::ProtectedStatic);
		case '2': return Variable(mangled, Name, StorageClass::PublicStatic);
		case '3': return Variable(mangled, Name, StorageClass::Global);
		case '4': return Variable(mangled, Name, StorageClass::FunctionLocalStatic);

		case '6':
		case '7': {
			SpecialTableSymbolNode* STSN	= arena.make<SpecialTableSymbolNode>(Name);
			if (!qualifiers(mangled, STSN->Quals))
				return nullptr;
			if (!mangled.check('@'))
				STSN->TargetName = FullyQualifiedTypeName(mangled);
			return STSN;
		}
		case '8'://untyped variable
			return arena.make<VariableSymbolNode>(Name);

		case '9':
			FC = FC | FuncClass::ExternC | FuncClass::NoParameterList;
			break;
		case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G': case 'H': case 'I': case 'J': case 'K': case 'L': case 'M':
		case 'N': case 'O': case 'P': case 'Q': case 'R': case 'S': case 'T': case 'U': case 'V': case 'W': case 'X': case 'Y': case 'Z':
			FC = FC | FuncClass((c - 'A') ^ (int)FuncClass::Access);
			break;
		case '$':
			FC = FC | FuncClass::Virtual | FuncClass::VirtualThisAdjust | FuncClass::VirtualThisAdjustEx * mangled.check('R');
			if (between(c = mangled.getc(), '0', '5')) {
				FC = FC | FuncClass((c - '0') ^ (int)FuncClass::Access);
				break;
			}
			// fall through
		default:
			return nullptr;
	}

	FunctionSignatureNode*	FSN;
	if (FC / FuncClass::Kind == FuncClass::StaticThisAdjust || (FC & FuncClass::VirtualThisAdjust)) {
		ThunkSignatureNode*	TTN = arena.make<ThunkSignatureNode>(FC);
		if ((FC & FuncClass::VirtualThisAdjustEx) && (!get_number(mangled, TTN->VBPtrOffset) || !get_number(mangled, TTN->VBOffsetOffset)))
			return nullptr;
		if ((FC & FuncClass::VirtualThisAdjust) && !get_number(mangled, TTN->VtordispOffset))
			return nullptr;
		if (!get_number(mangled, TTN->StaticOffset))
			return nullptr;
		FSN = TTN;
	} else {
		FSN = arena.make<FunctionSignatureNode>(FC);
	}

	IdentifierNode*	UQN = Name->get_unqualified();
	bool	conv = UQN->kind() == NodeKind::ConversionOperatorIdentifier;

	if (!(FC & FuncClass::ExternC) && (conv || !test_all(Flags, OutputFlags(OF_NoParameters|OF_NoReturn))) && !FunctionType(mangled, FSN))
		return nullptr;

	if (conv)
		static_cast<ConversionOperatorIdentifierNode*>(UQN)->TargetType = exchange(FSN->ReturnType, nullptr);

	return arena.make<FunctionSymbolNode>(Name, FSN);
}

SymbolNode* Demangler::parse(string_scan& mangled, OutputFlags Flags) {
	// MSVC-style mangled symbols must start with '?'
	if (!mangled.check('?'))
		return nullptr;

	// We can't demangle MD5 names
	if (mangled.begins("?@"))
		return nullptr;

	if (mangled.check("?_")) {
		switch (mangled.getc()) {
			case 'C':
				return StringLiteral(mangled);

			case '9': {//vcall
				auto	*Signature						= arena.make<ThunkSignatureNode>(FuncClass::NoParameterList);
				VcallThunkIdentifierNode*		VTIN	= arena.make<VcallThunkIdentifierNode>();
				FunctionSymbolNode*				FSN		= arena.make<FunctionSymbolNode>(NameScopeChain(mangled, VTIN), Signature);
				return	mangled.check("$B")
					&&	get_number(mangled, VTIN->OffsetInVTable)
					&&	mangled.check('A')
					&&	calling_convention(mangled, Signature->CallConvention)
				? FSN : nullptr;
			}
			case 'B': {//local static guard
				LocalStaticGuardIdentifierNode* LSGI	= arena.make<LocalStaticGuardIdentifierNode>();
				LocalStaticGuardVariableNode*	LSGVN	= arena.make<LocalStaticGuardVariableNode>(NameScopeChain(mangled, LSGI));
				if (mangled.check("4IA"))
					LSGVN->IsVisible = false;
				else if (mangled.check("5"))
					LSGVN->IsVisible = true;
				else
					return nullptr;
				return mangled.empty() || get_number(mangled, LSGI->ScopeIndex) ? LSGVN : nullptr;
			}
			case '_':
				if (is_any(mangled.peekc(), 'E', 'F')) {	//dynamic initializer for `T' and dynamic atexit destructor for `T'
					bool		Destructor	= mangled.getc() == 'F';
					bool		GotQuery	= mangled.check('?');
					SymbolNode *Sym			= Symbol(mangled, FullyQualifiedSymbolName(mangled));
					if (Sym) {
						if (Sym->kind() == NodeKind::VariableSymbol) {
							// Older versions of clang incorrectly omitted the leading ? and only emitted a single @ at the end, instead of 2. Handle both cases
							if (!mangled.check('@') || (GotQuery && !mangled.check('@')))
								return nullptr;
							Sym			= Symbol(mangled, SynthesizeQualifiedName(arena.make<DynamicStructorIdentifierNode>(Sym, Destructor)));
						} else {
							Sym->Name	= SynthesizeQualifiedName(arena.make<DynamicStructorIdentifierNode>(Sym->Name, Destructor));
						}
					}
					return Sym;
				}
				break;
		}
		mangled.move(-3);
	}

	return Symbol(mangled, FullyQualifiedSymbolName(mangled), Flags);
}

namespace iso {

string demangle_vc(const char* mangled, uint32 flags) {
	Demangler D;

	string_scan s(str(mangled));
	if (SymbolNode* AST = D.parse(s, (OutputFlags)flags)) {
		string_builder S;
		AST->output(S, (OutputFlags)flags);
		return S;
	}
	return mangled;
}

string_accum& demangle_vc(string_accum &a, const char* mangled, uint32 flags) {
	Demangler D;

	string_scan s(str(mangled));
	if (SymbolNode* AST = D.parse(s, (OutputFlags)flags)) {
		AST->output(a, (OutputFlags)flags);
	} else {
		a << mangled;
	}
	return a;
}

}

#if 0
#include <DbgHelp.h>

auto strip_spaces(char *s, char *dest) {
	for (char *d = dest;;++s) {
		if (*s != ' ' && !(*d++ = *s))
			break;
	}
	return str(dest);
}

struct test_demangler {
	test_demangler() {
//		const char *mangled = "??_R4exception@std@@6B@";
//		const char *mangled = "??_R3exception@std@@8";
//		const char *mangled = "??_R2exception@std@@8";
//		const char *mangled = "??_R1A@?0A@EA@exception@std@@8";
//		const char *mangled = "??0?$lf_array_queue@V?$callback@$$A6AXXZ@iso@@$0BAA@@iso@@QEAA@XZ";
//		const char *mangled = "??$bind@V?$fixed_accumT@D@iso@@$1?getp@12@QEAAPEADAEAH@Z@?$virtfunc@$$A6APEADAEAH@Z@iso@@QEAAXXZ";
//		const char *mangled = "??$thunk@V?$fixed_accumT@D@iso@@P812@EAAPEADAEAH@Z$1?getp@12@QEAAPEAD0@Z@?$virtfunc@$$A6APEADAEAH@Z@iso@@KAPEADPEAXAEAH@Z";
//		const char *mangled = "?c@?1???$CRC_type@UBasePose@iso@@@iso@@YA?AV?$crc@$0CA@@1@XZ@4V21@A";
		const char *mangled = "??_9ID3D11Device@@$BHI@AA";

		char	buffer[1024];
		DWORD	res = UnDecorateSymbolName(mangled, buffer, sizeof(buffer), UNDNAME_NAME_ONLY);//UNDNAME_COMPLETE);
		string s = demangle_vc(mangled, OF_NameOnly);//OF_AccessSpecifiers);//x3f);
		ISO_ASSERT(strip_spaces(s, s) == strip_spaces(buffer, buffer));
	}
} _test_demangler;
#endif
