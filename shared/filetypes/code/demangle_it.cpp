#include "base/algorithm.h"
#include "base/constants.h"
#include "base/array.h"
#include "base/strings.h"
#include "base/tuple.h"
#include "base/soft_float.h"
#include "allocators/allocator.h"

namespace demangle_itanium {

using namespace iso;

enum Qualifiers {
	QualNone		= 0,
	QualConst		= 1,
	QualVolatile	= 2,
	QualRestrict	= 4,
};

inline Qualifiers operator|=(Qualifiers& Q1, Qualifiers Q2) { return Q1 = static_cast<Qualifiers>(Q1 | Q2); }

string_accum& operator<<(string_accum &a, Qualifiers q) {
	return a	<< onlyif(q & QualConst,	" const")
				<< onlyif(q & QualVolatile, " volatile")
				<< onlyif(q & QualRestrict, " restrict");
}

class Node {
public:
	enum Kind : uint8 {
		kNodeArrayNode,
		kDotSuffix,
		kVendorExtQualType,
		kQualType,
		kConversionOperatorType,
		kPostfixQualifiedType,
		kElaboratedTypeSpefType,
		kNameType,
		kAbiTagAttr,
		kEnableIfAttr,
		kObjCProtoName,
		kPointerType,
		kReferenceType,
		kPointerToMemberType,
		kArrayType,
		kFunctionType,
		kNoexceptSpec,
		kDynamicExceptionSpec,
		kFunctionEncoding,
		kLiteralOperator,
		kSpecialName,
		kCtorVtableSpecialName,
		kQualifiedName,
		kNestedName,
		kLocalName,
		kVectorType,
		kPixelVectorType,
		kParameterPack,
		kTemplateArgumentPack,
		kParameterPackExpansion,
		kTemplateArgs,
		kForwardTemplateReference,
		kNameWithTemplateArgs,
		kGlobalQualifiedName,
		kStdQualifiedName,
		kExpandedSpecialSubstitution,
		kSpecialSubstitution,
		kCtorDtorName,
		kDtorName,
		kUnnamedTypeName,
		kClosureTypeName,
		kStructuredBindingName,
		kBinaryExpr,
		kArraySubscriptExpr,
		kPostfixExpr,
		kConditionalExpr,
		kMemberExpr,
		kEnclosingExpr,
		kCastExpr,
		kSizeofParamPackExpr,
		kCallExpr,
		kNewExpr,
		kDeleteExpr,
		kPrefixExpr,
		kFunctionParam,
		kConversionExpr,
		kInitListExpr,
		kFoldExpr,
		kThrowExpr,
		kBoolExpr,
		kIntegerCastExpr,
		kIntegerLiteral,
		kFloatLiteral,
		kDoubleLiteral,
		kLongDoubleLiteral,
		kBracedExpr,
		kBracedRangeExpr,
	};

	/// Three-way bool to track a cached value. Unknown is possible if this node has an unexpanded parameter pack below it that may affect this cache.
	enum class Cache : uint8 {Yes, No, Unknown };

public:
	Cache	RHSComponentCache;	// Tracks if this node has a component on its right side, in which case we need to call printRight.
	Cache	ArrayCache;			// Track if this node is a (possibly qualified) array type. This can affect how we format the output string.
	Cache	FunctionCache;		// Track if this node is a (possibly qualified) function type. This can affect how we format the output string.
	Kind	kind;

	Node(Kind kind, Cache RHSComponentCache = Cache::No, Cache ArrayCache = Cache::No, Cache FunctionCache = Cache::No) : RHSComponentCache(RHSComponentCache), ArrayCache(ArrayCache), FunctionCache(FunctionCache), kind(kind) {}

	bool hasRHSComponent()	const { return RHSComponentCache	!= Cache::Unknown ? RHSComponentCache	== Cache::Yes : hasRHSComponentSlow(); }
	bool hasArray()			const { return ArrayCache			!= Cache::Unknown ? ArrayCache			== Cache::Yes : hasArraySlow(); }
	bool hasFunction()		const { return FunctionCache		!= Cache::Unknown ? FunctionCache		== Cache::Yes : hasFunctionSlow(); }

	void print(string_accum& S) const {
		printLeft(S);
		if (RHSComponentCache != Cache::No)
			printRight(S);
	}

	virtual ~Node() = default;
	virtual bool			hasRHSComponentSlow()	const { return false; }
	virtual bool			hasArraySlow()			const { return false; }
	virtual bool			hasFunctionSlow()		const { return false; }
	virtual const Node*		getSyntaxNode()			const { return this; }
	virtual count_string	getBaseName()			const { return ""; }
	virtual void			printLeft(string_accum&) const = 0;
	virtual void			printRight(string_accum&) const {}
};

struct NodeArray : ptr_array<Node*> {
	NodeArray() : ptr_array<Node*>() {}
	NodeArray(Node** Elements_, size_t NumElements_) : ptr_array<Node*>(Elements_, NumElements_) {}
	void printWithComma(string_accum& S) const {
		int	j = 0;
		for (auto &i : *this)
			i->print(S << onlyif(j++, ", "));
	}
};

struct NodeArrayNode : Node {
	NodeArray Array;
	NodeArrayNode(NodeArray Array_) : Node(kNodeArrayNode), Array(Array_) {}
	void printLeft(string_accum& S) const override { Array.printWithComma(S); }
};

class DotSuffix final : public Node {
	const Node*		Prefix;
	count_string	Suffix;
public:
	DotSuffix(const Node* Prefix_, count_string Suffix) : Node(kDotSuffix), Prefix(Prefix_), Suffix(Suffix) {}
	void printLeft(string_accum& s) const override {
		Prefix->print(s);
		s << " (" << Suffix << ')';
	}
};

class VendorExtQualType final : public Node {
	const Node*		Ty;
	count_string	Ext;
public:
	VendorExtQualType(const Node* Ty_, count_string Ext) : Node(kVendorExtQualType), Ty(Ty_), Ext(Ext) {}
	void printLeft(string_accum& S) const override {
		Ty->print(S);
		S << ' ' << Ext;
	}
};

enum FunctionRefQual : uint8 {
	FrefQualNone,
	FrefQualLValue,
	FrefQualRValue,
};

class QualType : public Node {
protected:
	const Qualifiers Quals;
	const Node*		 Child;
public:
	QualType(const Node* Child_, Qualifiers Quals_) : Node(kQualType, Child_->RHSComponentCache, Child_->ArrayCache, Child_->FunctionCache), Quals(Quals_), Child(Child_) {}
	bool hasRHSComponentSlow() const override { return Child->hasRHSComponent(); }
	bool hasArraySlow() const override { return Child->hasArray(); }
	bool hasFunctionSlow() const override { return Child->hasFunction(); }
	void printLeft(string_accum& S) const override { Child->printLeft(S); S << Quals; }
	void printRight(string_accum& S) const override { Child->printRight(S); }
};

class ConversionOperatorType final : public Node {
	const Node* Ty;
public:
	ConversionOperatorType(const Node* Ty) : Node(kConversionOperatorType), Ty(Ty) {}
	void printLeft(string_accum& S) const override { Ty->print(S << "operator "); }
};

class PostfixQualifiedType final : public Node {
	const Node*	Ty;
	const char*	Postfix;
public:
	PostfixQualifiedType(Node* Ty_, const char *Postfix_) : Node(kPostfixQualifiedType), Ty(Ty_), Postfix(Postfix_) {}
	void printLeft(string_accum& s) const override { Ty->printLeft(s); s << Postfix; }
};

class NameType final : public Node {
	count_string Name;
public:
	NameType(count_string Name) : Node(kNameType), Name(Name) {}
	count_string getName() const { return Name; }
	count_string getBaseName() const override { return Name; }
	void printLeft(string_accum& s) const override { s << Name; }
};

class ElaboratedTypeSpefType : public Node {
	const char *Kind;
	Node*		Child;
public:
	ElaboratedTypeSpefType(const char *Kind_, Node* Child_) : Node(kElaboratedTypeSpefType), Kind(Kind_), Child(Child_) {}
	void printLeft(string_accum& S) const override { Child->print(S << Kind << ' '); }
};

struct AbiTagAttr : Node {
	Node*		Base;
	count_string Tag;
	AbiTagAttr(Node* Base_, count_string Tag) : Node(kAbiTagAttr, Base_->RHSComponentCache, Base_->ArrayCache, Base_->FunctionCache), Base(Base_), Tag(Tag) {}
	void printLeft(string_accum& S) const override { Base->printLeft(S); S << "[abi:" << Tag << "]"; }
};

class EnableIfAttr : public Node {
	NodeArray Conditions;
public:
	EnableIfAttr(NodeArray Conditions_) : Node(kEnableIfAttr), Conditions(Conditions_) {}
	void printLeft(string_accum& S) const override { Conditions.printWithComma(S << " [enable_if:"); S << ']'; }
};

class ObjCProtoName : public Node {
	const Node* Ty;
	count_string Protocol;
	friend class PointerType;
public:
	ObjCProtoName(const Node* Ty_, count_string Protocol) : Node(kObjCProtoName), Ty(Ty_), Protocol(Protocol) {}
	bool isObjCObject() const { return Ty->kind == kNameType && static_cast<const NameType*>(Ty)->getName() == "objc_object"; }
	void printLeft(string_accum& S) const override { Ty->print(S); S << "<" << Protocol << ">"; }
};

class PointerType final : public Node {
	const Node* Pointee;
public:
	PointerType(const Node* Pointee_) : Node(kPointerType, Pointee_->RHSComponentCache), Pointee(Pointee_) {}
	bool hasRHSComponentSlow() const override { return Pointee->hasRHSComponent(); }

	void printLeft(string_accum& s) const override {
		// We rewrite objc_object<SomeProtocol>* into id<SomeProtocol>.
		if (Pointee->kind != kObjCProtoName || !static_cast<const ObjCProtoName*>(Pointee)->isObjCObject()) {
			Pointee->printLeft(s);
			s << onlyif(Pointee->hasArray(), ' ') << onlyif(Pointee->hasArray() || Pointee->hasFunction(), '(') << "*";
		} else {
			const auto* objcProto = static_cast<const ObjCProtoName*>(Pointee);
			s << "id<" << objcProto->Protocol << '>';
		}
	}
	void printRight(string_accum& s) const override {
		if (Pointee->kind != kObjCProtoName || !static_cast<const ObjCProtoName*>(Pointee)->isObjCObject())
			Pointee->printRight(s << onlyif(Pointee->hasArray() || Pointee->hasFunction(), ')'));
	}
};

enum class ReferenceKind {
	LValue,
	RValue,
};

// Represents either a LValue or an RValue reference type.
class ReferenceType : public Node {
	const Node*		Pointee;
	ReferenceKind	RK;

	mutable bool Printing = false;

	// Dig through any refs to refs, collapsing the ReferenceTypes as we go. The rule here is rvalue ref to rvalue ref collapses to a rvalue ref, and any other combination collapses to a lvalue ref.
	pair<ReferenceKind, const Node*> collapse(string_accum& S) const {
		ReferenceKind SoFarRK = RK;
		const Node* SoFarPointee = Pointee;
		for (;;) {
			const Node* SN = SoFarPointee->getSyntaxNode();
			if (SN->kind != kReferenceType)
				break;
			auto* RT	 = static_cast<const ReferenceType*>(SN);
			SoFarPointee = RT->Pointee;
			if (RT->RK < SoFarRK)
				SoFarRK = RT->RK;
		}
		return {SoFarRK, SoFarPointee};
	}

public:
	ReferenceType(const Node* Pointee, ReferenceKind RK) : Node(kReferenceType, Pointee->RHSComponentCache), Pointee(Pointee), RK(RK) {
	// Dig through any refs to refs, collapsing the ReferenceTypes as we go. The rule here is rvalue ref to rvalue ref collapses to a rvalue ref, and any other combination collapses to a lvalue ref.
		for (;;) {
			const Node* SN = Pointee->getSyntaxNode();
			if (SN->kind != kReferenceType)
				break;
			auto* RT	 = static_cast<const ReferenceType*>(SN);
			Pointee = RT->Pointee;
			if (RT->RK < RK)
				RK = RT->RK;
		}
	}
	bool hasRHSComponentSlow() const override { return Pointee->hasRHSComponent(); }

	void printLeft(string_accum& s) const override {
		if (!Printing) {
			save(Printing, true), Pointee->printLeft(s);
			s << onlyif(Pointee->hasArray(), ' ') << onlyif(Pointee->hasArray() || Pointee->hasFunction(), '(') << ifelse(RK == ReferenceKind::LValue, "&", "&&");
		}
	}
	void printRight(string_accum& s) const override {
		if (!Printing)
			save(Printing, true), Pointee->printRight(s << onlyif(Pointee->hasArray() || Pointee->hasFunction(), ')'));
	}
};

class PointerToMemberType final : public Node {
	const Node* ClassType;
	const Node* MemberType;
public:
	PointerToMemberType(const Node* ClassType_, const Node* MemberType_) : Node(kPointerToMemberType, MemberType_->RHSComponentCache), ClassType(ClassType_), MemberType(MemberType_) {}
	bool hasRHSComponentSlow() const override { return MemberType->hasRHSComponent(); }

	void printLeft(string_accum& s) const override {
		MemberType->printLeft(s);
		ClassType->print(s << ifelse(MemberType->hasArray() || MemberType->hasFunction(), '(', ' '));
		s << "::*";
	}
	void printRight(string_accum& s) const override {
		if (MemberType->hasArray() || MemberType->hasFunction())
			s << ")";
		MemberType->printRight(s);
	}
};

class ArrayType final : public Node {
	const Node*	Base;
	const Node*	Dimension;
public:
	ArrayType(const Node* Base_, const Node* Dimension_)
			: Node(kArrayType, /*RHSComponentCache=*/Cache::Yes, /*ArrayCache=*/Cache::Yes)
			, Base(Base_)
			, Dimension(Dimension_) {}

	bool hasRHSComponentSlow() const override { return true; }
	bool hasArraySlow() const override { return true; }
	void printLeft(string_accum& S) const override { Base->printLeft(S); }
	void printRight(string_accum& S) const override {
		Dimension->print(S << onlyif(S.back() != ']', ' ') << "[");
		Base->printRight(S << "]");
	}
};

class FunctionType final : public Node {
	const Node*		Ret;
	NodeArray		Params;
	Qualifiers		CVQuals;
	FunctionRefQual RefQual;
	const Node*		ExceptionSpec;
public:
	FunctionType(const Node* Ret_, NodeArray Params_, Qualifiers CVQuals_, FunctionRefQual RefQual_, const Node* ExceptionSpec_)
			: Node(kFunctionType, /*RHSComponentCache=*/Cache::Yes, /*ArrayCache=*/Cache::No, /*FunctionCache=*/Cache::Yes)
			, Ret(Ret_)
			, Params(Params_)
			, CVQuals(CVQuals_)
			, RefQual(RefQual_)
			, ExceptionSpec(ExceptionSpec_) {}

	bool hasRHSComponentSlow() const override { return true; }
	bool hasFunctionSlow() const override { return true; }

	// Handle C++'s ... quirky decl grammar by using the left & right distinction. Consider:
	//	int (*f(float))(char) {}
	// f is a function that takes a float and returns a pointer to a function that takes a char and returns an int. If we're trying to print f, start by printing out the return types's left, then print our parameters, then finally print right of the return type.
	void printLeft(string_accum& S) const override {
		Ret->printLeft(S);
		S << " ";
	}
	void printRight(string_accum& S) const override {
		Params.printWithComma(S << '(');
		Ret->printRight(S << ')');

		S	<< CVQuals;
		if (RefQual == FrefQualLValue)
			S << " &";
		else if (RefQual == FrefQualRValue)
			S << " &&";

		if (ExceptionSpec)
			ExceptionSpec->print(S << ' ');
	}
};

class NoexceptSpec : public Node {
	const Node* E;
public:
	NoexceptSpec(const Node* E_) : Node(kNoexceptSpec), E(E_) {}
	void printLeft(string_accum& S) const override {
		E->print(S << "noexcept(");
		S << ")";
	}
};

class DynamicExceptionSpec : public Node {
	NodeArray Types;
public:
	DynamicExceptionSpec(NodeArray Types_) : Node(kDynamicExceptionSpec), Types(Types_) {}
	void printLeft(string_accum& S) const override {
		Types.printWithComma(S << "throw(");
		S << ')';
	}
};

class FunctionEncoding final : public Node {
	const Node*		Ret;
	const Node*		Name;
	NodeArray		Params;
	const Node*		Attrs;
	Qualifiers		CVQuals;
	FunctionRefQual RefQual;
public:
	FunctionEncoding(const Node* Ret_, const Node* Name_, NodeArray Params_, const Node* Attrs_, Qualifiers CVQuals_, FunctionRefQual RefQual_)
		: Node(kFunctionEncoding, /*RHSComponentCache=*/Cache::Yes, /*ArrayCache=*/Cache::No, /*FunctionCache=*/Cache::Yes)
		, Ret(Ret_)
		, Name(Name_)
		, Params(Params_)
		, Attrs(Attrs_)
		, CVQuals(CVQuals_)
		, RefQual(RefQual_) {}
	Qualifiers		getCVQuals() const { return CVQuals; }
	FunctionRefQual getRefQual() const { return RefQual; }
	NodeArray		getParams() const { return Params; }
	const Node*		getReturnType() const { return Ret; }

	bool hasRHSComponentSlow() const override { return true; }
	bool hasFunctionSlow() const override { return true; }

	const Node* getName() const { return Name; }

	void printLeft(string_accum& S) const override {
		if (Ret) {
			Ret->printLeft(S);
			if (!Ret->hasRHSComponent())
				S << ' ';
		}
		Name->print(S);
	}

	void printRight(string_accum& S) const override {
		Params.printWithComma(S << '(');
		S << ')';
		if (Ret)
			Ret->printRight(S);

		S	<< CVQuals;

		if (RefQual == FrefQualLValue)
			S << " &";
		else if (RefQual == FrefQualRValue)
			S << " &&";

		if (Attrs)
			Attrs->print(S);
	}
};

class LiteralOperator : public Node {
	const Node* OpName;
public:
	LiteralOperator(const Node* OpName_) : Node(kLiteralOperator), OpName(OpName_) {}
	void printLeft(string_accum& S) const override { OpName->print(S << "operator\"\" "); }
};

class SpecialName final : public Node {
	const char*	Special;
	const Node*	Child;
public:
	SpecialName(const char *Special_, const Node* Child_) : Node(kSpecialName), Special(Special_), Child(Child_) {}
	void printLeft(string_accum& S) const override { Child->print(S << Special); }
};

class CtorVtableSpecialName final : public Node {
	const Node* FirstType;
	const Node* SecondType;
public:
	CtorVtableSpecialName(const Node* FirstType_, const Node* SecondType_) : Node(kCtorVtableSpecialName), FirstType(FirstType_), SecondType(SecondType_) {}
	void printLeft(string_accum& S) const override {
		FirstType->print(S << "construction vtable for ");
		SecondType->print(S << "-in-");
	}
};

struct NestedName : Node {
	Node* Qual;
	Node* Name;
	NestedName(Node* Qual_, Node* Name_) : Node(kNestedName), Qual(Qual_), Name(Name_) {}
	count_string getBaseName() const override { return Name->getBaseName(); }
	void printLeft(string_accum& S) const override {
		Qual->print(S);
		Name->print(S << "::");
	}
};

struct LocalName : Node {
	Node* Encoding;
	Node* Entity;
	LocalName(Node* Encoding_, Node* Entity_) : Node(kLocalName), Encoding(Encoding_), Entity(Entity_) {}
	void printLeft(string_accum& S) const override {
		Encoding->print(S);
		Entity->print(S << "::");
	}
};

class QualifiedName final : public Node {
	const Node* Qualifier;
	const Node* Name;
public:
	QualifiedName(const Node* Qualifier_, const Node* Name_) : Node(kQualifiedName), Qualifier(Qualifier_), Name(Name_) {}
	count_string getBaseName() const override { return Name->getBaseName(); }
	void printLeft(string_accum& S) const override {
		Qualifier->print(S);
		Name->print(S<< "::");
	}
};

class VectorType final : public Node {
	const Node*	BaseType;
	const Node*	Dimension;
public:
	VectorType(const Node* BaseType_, const Node* Dimension_) : Node(kVectorType), BaseType(BaseType_), Dimension(Dimension_) {}
	void printLeft(string_accum& S) const override {
		BaseType->print(S);
		Dimension->print(S << " vector[");
		S << ']';
	}
};

class PixelVectorType final : public Node {
	count_string Dimension;
public:
	PixelVectorType(count_string Dimension) : Node(kPixelVectorType), Dimension(Dimension) {}
	void printLeft(string_accum& S) const override {
		// FIXME: This should demangle as "vector pixel".
		S << "pixel vector[" << Dimension << "]";
	}
};

// An unexpanded parameter pack (either in the expression or type context).
// If this AST is correct, this node will have a ParameterPackExpansion node aboveit.
// This node is created when some <template-args> are found that apply to an <encoding>, and is stored in the TemplateParams table. In order for this to appear in the final AST, it has to referenced via a <template-param> (ie, T_).

int	CurrentPackIndex;

class ParameterPack final : public Node {
	NodeArray Data;
public:
	ParameterPack(NodeArray Data_) : Node(kParameterPack), Data(Data_) {
		ArrayCache = FunctionCache = RHSComponentCache = Cache::Unknown;
		if (all_of(Data.begin(), Data.end(), [](Node* P) { return P->ArrayCache == Cache::No; }))
			ArrayCache = Cache::No;
		if (all_of(Data.begin(), Data.end(), [](Node* P) { return P->FunctionCache == Cache::No; }))
			FunctionCache = Cache::No;
		if (all_of(Data.begin(), Data.end(), [](Node* P) { return P->RHSComponentCache == Cache::No; }))
			RHSComponentCache = Cache::No;
	}
	bool hasRHSComponentSlow() const override {
		size_t Idx = CurrentPackIndex;
		return Idx < Data.size() && Data[Idx]->hasRHSComponent();
	}
	bool hasArraySlow() const override {
		size_t Idx = CurrentPackIndex;
		return Idx < Data.size() && Data[Idx]->hasArray();
	}
	bool hasFunctionSlow() const override {
		size_t Idx = CurrentPackIndex;
		return Idx < Data.size() && Data[Idx]->hasFunction();
	}
	const Node* getSyntaxNode() const override {
		size_t Idx = CurrentPackIndex;
		return Idx < Data.size() ? Data[Idx]->getSyntaxNode() : this;
	}
	void printLeft(string_accum& S) const override {
		size_t Idx = CurrentPackIndex;
		if (Idx < Data.size())
			Data[Idx]->printLeft(S);
	}
	void printRight(string_accum& S) const override {
		size_t Idx = CurrentPackIndex;
		if (Idx < Data.size())
			Data[Idx]->printRight(S);
	}
};

// A variadic template argument. This node represents an occurrence of J<something>E in some <template-args>. It isn't itself unexpanded, unless one of its Elements is.
// The parser inserts a ParameterPack into the TemplateParams table if the <template-args> this pack belongs to apply to an <encoding>.
class TemplateArgumentPack final : public Node {
	NodeArray Elements;
public:
	TemplateArgumentPack(NodeArray Elements_) : Node(kTemplateArgumentPack), Elements(Elements_) {}
	NodeArray getElements() const { return Elements; }
	void printLeft(string_accum& S) const override { Elements.printWithComma(S); }
};

/// A pack expansion. Below this node, there are some unexpanded ParameterPacks which each have Child->ParameterPackSize elements.
class ParameterPackExpansion final : public Node {
	const Node* Child;
public:
	ParameterPackExpansion(const Node* Child_) : Node(kParameterPackExpansion), Child(Child_) {}
	const Node* getChild() const { return Child; }
	void printLeft(string_accum& S) const override {
		constexpr uint32		 Max = maximum;
#if 0
		saver<uint32> SavePackIdx(S.CurrentPackIndex, Max);
		saver<uint32> SavePackMax(S.CurrentPackMax, Max);
		size_t					 StreamPos = S.getCurrentPosition();

		// Print the first element in the pack. If Child contains a ParameterPack, it will set up S.CurrentPackMax and print the first element.
		Child->print(S);

		// No ParameterPack was found in Child. This can occur if we've found a pack expansion on a <function-param>.
		if (S.CurrentPackMax == Max) {
			S << "...";
			return;
		}

		// We found a ParameterPack, but it has no elements. Erase whatever we may of printed.
		if (S.CurrentPackMax == 0) {
			S.setCurrentPosition(StreamPos);
			return;
		}

		// Else, iterate through the rest of the elements in the pack.
		for (uint32 I = 1, E = S.CurrentPackMax; I < E; ++I) {
			S << ", ";
			S.CurrentPackIndex = I;
			Child->print(S);
		}
#endif
	}
};

class TemplateArgs final : public Node {
	NodeArray Params;
public:
	TemplateArgs(NodeArray Params_) : Node(kTemplateArgs), Params(Params_) {}
	NodeArray getParams() { return Params; }
	void printLeft(string_accum& S) const override {
		Params.printWithComma(S << '<');
		S << '>';
	}
};

// A forward-reference to a template argument that was not known at the point where the template parameter name was parsed in a mangling.
// This is created when demangling the name of a specialization of a conversion function template:
//
// \code
// struct A {
//		template<typename T> operator T*();
// };
// \endcode
//
// When demangling a specialization of the conversion function template, we encounter the name of the template (including the \c T) before we reach the template argument list, so we cannot substitute the parameter name for the corresponding argument while parsing.
// Instead, we create a \c ForwardTemplateReference node that is resolved after we parse the template arguments.
struct ForwardTemplateReference : Node {
	size_t	Index;
	Node*	Ref = nullptr;

	// If we're currently printing this node. It is possible (though invalid) for a forward template reference to refer to itself via a substitution.
	// This creates a cyclic AST, which will stack overflow printing. To fix this, bail out if more than one print* function is active.
	mutable bool Printing = false;

	ForwardTemplateReference(size_t Index_) : Node(kForwardTemplateReference, Cache::Unknown, Cache::Unknown, Cache::Unknown), Index(Index_) {}

	bool hasRHSComponentSlow()		const override { return !Printing && (save(Printing, true), Ref->hasRHSComponent()); }
	bool hasArraySlow()				const override { return !Printing && (save(Printing, true), Ref->hasArray()); }
	bool hasFunctionSlow()			const override { return !Printing && (save(Printing, true), Ref->hasFunction()); }
	const Node* getSyntaxNode()		const override { return Printing ? this : (save(Printing, true), Ref->getSyntaxNode());	}
	void printLeft(string_accum& S) const override {
		if (!Printing)
			save(Printing, true), Ref->printLeft(S);
	}
	void printRight(string_accum& S) const override {
		if (!Printing)
			save(Printing, true), Ref->printRight(S);
	}
};

struct NameWithTemplateArgs : Node {
	Node* Name;
	Node* TemplateArgs;
	NameWithTemplateArgs(Node* Name_, Node* TemplateArgs_) : Node(kNameWithTemplateArgs), Name(Name_), TemplateArgs(TemplateArgs_) {}
	count_string getBaseName() const override { return Name->getBaseName(); }
	void printLeft(string_accum& S) const override {
		Name->print(S);
		TemplateArgs->print(S);
	}
};

class GlobalQualifiedName final : public Node {
	Node* Child;
public:
	GlobalQualifiedName(Node* Child_) : Node(kGlobalQualifiedName), Child(Child_) {}
	count_string getBaseName() const override { return Child->getBaseName(); }
	void printLeft(string_accum& S) const override { Child->print(S << "::"); }
};

struct StdQualifiedName : Node {
	Node* Child;
	StdQualifiedName(Node* Child_) : Node(kStdQualifiedName), Child(Child_) {}
	count_string getBaseName() const override { return Child->getBaseName(); }
	void printLeft(string_accum& S) const override { Child->print(S); }
};

enum class SpecialSubKind {
	allocator,
	basic_string,
	string,
	istream,
	ostream,
	iostream,
};

class ExpandedSpecialSubstitution final : public Node {
	SpecialSubKind SSK;
public:
	ExpandedSpecialSubstitution(SpecialSubKind SSK_) : Node(kExpandedSpecialSubstitution), SSK(SSK_) {}
	count_string getBaseName() const override {
		switch (SSK) {
			case SpecialSubKind::allocator: return "allocator";
			case SpecialSubKind::basic_string: return "basic_string";
			case SpecialSubKind::string: return "basic_string";
			case SpecialSubKind::istream: return "basic_istream";
			case SpecialSubKind::ostream: return "basic_ostream";
			case SpecialSubKind::iostream: return "basic_iostream";
		}
		unreachable();
	}
	void printLeft(string_accum& S) const override {
		switch (SSK) {
			case SpecialSubKind::allocator: S << "allocator"; break;
			case SpecialSubKind::basic_string: S << "basic_string"; break;
			case SpecialSubKind::string:
				S << "basic_string<char, char_traits<char>, "
					 "allocator<char> >";
				break;
			case SpecialSubKind::istream: S << "basic_istream<char, char_traits<char> >"; break;
			case SpecialSubKind::ostream: S << "basic_ostream<char, char_traits<char> >"; break;
			case SpecialSubKind::iostream: S << "basic_iostream<char, char_traits<char> >"; break;
		}
	}
};

class SpecialSubstitution final : public Node {
public:
	SpecialSubKind SSK;
	SpecialSubstitution(SpecialSubKind SSK_) : Node(kSpecialSubstitution), SSK(SSK_) {}
	count_string getBaseName() const override {
		switch (SSK) {
			case SpecialSubKind::allocator: return "allocator";
			case SpecialSubKind::basic_string: return "basic_string";
			case SpecialSubKind::string: return "string";
			case SpecialSubKind::istream: return "istream";
			case SpecialSubKind::ostream: return "ostream";
			case SpecialSubKind::iostream: return "iostream";
		}
		unreachable();
	}
	void printLeft(string_accum& S) const override {
		switch (SSK) {
			case SpecialSubKind::allocator: S << "allocator"; break;
			case SpecialSubKind::basic_string: S << "basic_string"; break;
			case SpecialSubKind::string: S << "string"; break;
			case SpecialSubKind::istream: S << "istream"; break;
			case SpecialSubKind::ostream: S << "ostream"; break;
			case SpecialSubKind::iostream: S << "iostream"; break;
		}
	}
};

class CtorDtorName final : public Node {
	const Node*	Basename;
	const bool	IsDtor;
	const int	Variant;
public:
	CtorDtorName(const Node* Basename_, bool IsDtor_, int Variant_) : Node(kCtorDtorName), Basename(Basename_), IsDtor(IsDtor_), Variant(Variant_) {}
	void printLeft(string_accum& S) const override { S << onlyif(IsDtor, '~') << Basename->getBaseName(); }
};

class DtorName : public Node {
	const Node* Base;
public:
	DtorName(const Node* Base_) : Node(kDtorName), Base(Base_) {}
	void printLeft(string_accum& S) const override { Base->printLeft(S << '~'); }
};

class UnnamedTypeName : public Node {
	count_string Count;
public:
	UnnamedTypeName(count_string Count) : Node(kUnnamedTypeName), Count(Count) {}
	void printLeft(string_accum& S) const override { S << "'unnamed" << Count << '\''; }
};

class ClosureTypeName : public Node {
	NodeArray	Params;
	count_string Count;
public:
	ClosureTypeName(NodeArray Params_, count_string Count) : Node(kClosureTypeName), Params(Params_), Count(Count) {}
	void printLeft(string_accum& S) const override {
		Params.printWithComma(S << "\'lambda" << Count << "\'(");
		S << ")";
	}
};

class StructuredBindingName : public Node {
	NodeArray Bindings;
public:
	StructuredBindingName(NodeArray Bindings_) : Node(kStructuredBindingName), Bindings(Bindings_) {}
	void printLeft(string_accum& S) const override {
		Bindings.printWithComma(S << '[');
		S << ']';
	}
};

// -- Expression Nodes --

class BinaryExpr : public Node {
	const Node*	LHS;
	const char*	InfixOperator;
	const Node*	RHS;
public:
	BinaryExpr(const Node* LHS_, const char *InfixOperator_, const Node* RHS_) : Node(kBinaryExpr), LHS(LHS_), InfixOperator(InfixOperator_), RHS(RHS_) {}
	void printLeft(string_accum& S) const override {
		// might be a template argument expression, then we need to disambiguate with parens.
		if (InfixOperator == str(">"))
			S << '(';
		LHS->print(S << '(');
		RHS->print(S << ") " << InfixOperator << " (");
		S << ')' << onlyif(InfixOperator == ">"_cstr, ')');
	}
};

class ArraySubscriptExpr : public Node {
	const Node* Op1;
	const Node* Op2;
public:
	ArraySubscriptExpr(const Node* Op1_, const Node* Op2_) : Node(kArraySubscriptExpr), Op1(Op1_), Op2(Op2_) {}
	void printLeft(string_accum& S) const override {
		Op1->print(S << '(');
		Op2->print(S << ")[");
		S << ']';
	}
};

class PostfixExpr : public Node {
	const Node*	Child;
	const char*	Operator;
public:
	PostfixExpr(const Node* Child_, const char *Operator_) : Node(kPostfixExpr), Child(Child_), Operator(Operator_) {}
	void printLeft(string_accum& S) const override {
		Child->print(S << '(');
		S << ')' << Operator;
	}
};

class ConditionalExpr : public Node {
	const Node *Cond, *Then, *Else;
public:
	ConditionalExpr(const Node* Cond_, const Node* Then_, const Node* Else_) : Node(kConditionalExpr), Cond(Cond_), Then(Then_), Else(Else_) {}
	void printLeft(string_accum& S) const override {
		Cond->print(S << '(');
		Then->print(S << ") ? (");
		Else->print(S << ") : (");
		S << ')';
	}
};

class MemberExpr : public Node {
	const char		*Kind;
	const Node		*LHS, *RHS;
public:
	MemberExpr(const Node* LHS_, const char *Kind_, const Node* RHS_) : Node(kMemberExpr), LHS(LHS_), Kind(Kind_), RHS(RHS_) {}
	void printLeft(string_accum& S) const override {
		LHS->print(S);
		RHS->print(S << Kind);
	}
};

class EnclosingExpr : public Node {
	const char	*Prefix;
	const Node	*Infix;
	const char	*Postfix;
public:
	EnclosingExpr(const char *Prefix_, Node* Infix_, const char *Postfix_) : Node(kEnclosingExpr), Prefix(Prefix_), Infix(Infix_), Postfix(Postfix_) {}
	void printLeft(string_accum& S) const override {
		Infix->print(S << Prefix);
		S << Postfix;
	}
};

class CastExpr : public Node {
	// cast_kind<to>(from)
	const char	*CastKind;
	const Node	*To, *From;
public:
	CastExpr(const char *CastKind_, const Node* To_, const Node* From_) : Node(kCastExpr), CastKind(CastKind_), To(To_), From(From_) {}
	void printLeft(string_accum& S) const override {
		To->printLeft(S << CastKind << '<');
		From->printLeft(S << ">(");
		S << ')';
	}
};

class SizeofParamPackExpr : public Node {
	const Node* Pack;
public:
	SizeofParamPackExpr(const Node* Pack_) : Node(kSizeofParamPackExpr), Pack(Pack_) {}
	void printLeft(string_accum& S) const override {
		S << "sizeof...(";
		ParameterPackExpansion PPE(Pack);
		PPE.printLeft(S);
		S << ')';
	}
};

class CallExpr : public Node {
	const Node* Callee;
	NodeArray	Args;
public:
	CallExpr(const Node* Callee_, NodeArray Args_) : Node(kCallExpr), Callee(Callee_), Args(Args_) {}
	void printLeft(string_accum& S) const override {
		Callee->print(S);
		Args.printWithComma(S << '(');
		S << ')';
	}
};

class NewExpr : public Node {
	// new (expr_list) type(init_list)
	NodeArray	ExprList;
	Node*		Type;
	NodeArray	InitList;
	bool		IsGlobal;	// ::operator new ?
	bool		IsArray;	// new[] ?
public:
	NewExpr(NodeArray ExprList_, Node* Type_, NodeArray InitList_, bool IsGlobal_, bool IsArray_) : Node(kNewExpr), ExprList(ExprList_), Type(Type_), InitList(InitList_), IsGlobal(IsGlobal_), IsArray(IsArray_) {}
	void printLeft(string_accum& S) const override {
		if (IsGlobal)
			S << "::operator ";
		S << "new";
		if (IsArray)
			S << "[]";
		S << ' ';
		if (!ExprList.empty()) {
			ExprList.printWithComma(S << '(');
			S << ')';
		}
		Type->print(S);
		if (!InitList.empty()) {
			InitList.printWithComma(S << '(');
			S << ')';
		}
	}
};

class DeleteExpr : public Node {
	Node*	Op;
	bool	IsGlobal, IsArray;
public:
	DeleteExpr(Node* Op_, bool IsGlobal_, bool IsArray_) : Node(kDeleteExpr), Op(Op_), IsGlobal(IsGlobal_), IsArray(IsArray_) {}
	void printLeft(string_accum& S) const override {
		if (IsGlobal)
			S << "::";
		S << "delete";
		if (IsArray)
			S << "[] ";
		Op->print(S);
	}
};

class PrefixExpr : public Node {
	const char *Prefix;
	Node*		Child;
public:
	PrefixExpr(const char *Prefix_, Node* Child_) : Node(kPrefixExpr), Prefix(Prefix_), Child(Child_) {}
	void printLeft(string_accum& S) const override {
		Child->print(S << Prefix << '(');
		S << ')';
	}
};

class FunctionParam : public Node {
	count_string Number;
public:
	FunctionParam(count_string Number) : Node(kFunctionParam), Number(Number) {}
	void printLeft(string_accum& S) const override {
		S << "fp" << Number;
	}
};

class ConversionExpr : public Node {
	const Node*	Type;
	NodeArray	Expressions;
public:
	ConversionExpr(const Node* Type, NodeArray Expressions) : Node(kConversionExpr), Type(Type), Expressions(Expressions) {}
	void printLeft(string_accum& S) const override {
		Type->print(S << '(');
		Expressions.printWithComma(S << ")(");
		S << ')';
	}
};

class InitListExpr : public Node {
	const Node* Ty;
	NodeArray	Inits;
public:
	InitListExpr(const Node* Ty_, NodeArray Inits_) : Node(kInitListExpr), Ty(Ty_), Inits(Inits_) {}
	void printLeft(string_accum& S) const override {
		if (Ty)
			Ty->print(S);
		Inits.printWithComma(S << '{');
		S << '}';
	}
};

class BracedExpr : public Node {
	const Node	*Elem, *Init;
	bool		IsArray;
public:
	BracedExpr(const Node* Elem_, const Node* Init_, bool IsArray_) : Node(kBracedExpr), Elem(Elem_), Init(Init_), IsArray(IsArray_) {}
	void printLeft(string_accum& S) const override {
		if (IsArray) {
			Elem->print(S << '[');
			S << ']';
		} else {
			Elem->print(S << '.');
		}
		if (Init->kind != kBracedExpr && Init->kind != kBracedRangeExpr)
			S << " = ";
		Init->print(S);
	}
};

class BracedRangeExpr : public Node {
	const Node	*First, *Last, *Init;
public:
	BracedRangeExpr(const Node* First_, const Node* Last_, const Node* Init_) : Node(kBracedRangeExpr), First(First_), Last(Last_), Init(Init_) {}
	void printLeft(string_accum& S) const override {
		First->print(S << '[');
		Last->print(S << " ... ");
		S << ']';
		if (Init->kind != kBracedExpr && Init->kind != kBracedRangeExpr)
			S << " = ";
		Init->print(S);
	}
};

class FoldExpr : public Node {
	const Node	*Pack, *Init;
	const char	*OperatorName;
	bool		IsLeftFold;
public:
	FoldExpr(bool IsLeftFold_, const char *OperatorName_, const Node* Pack_, const Node* Init_) : Node(kFoldExpr), Pack(Pack_), Init(Init_), OperatorName(OperatorName_), IsLeftFold(IsLeftFold_) {}
	void printLeft(string_accum& S) const override {
		auto PrintPack = [&] {
			ParameterPackExpansion(Pack).print(S << '(');
			S << ')';
		};
		S << '(';
		if (IsLeftFold) {
			// init op ... op pack
			if (Init) {
				Init->print(S);
				S << ' ' << OperatorName << ' ';
			}
			// ... op pack
			S << "... " << OperatorName << ' ';
			PrintPack();
		} else {	// !IsLeftFold
			// pack op ...
			PrintPack();
			S << ' ' << OperatorName << " ...";
			// pack op ... op init
			if (Init)
				Init->print(S << ' ' << OperatorName << ' ');
		}
		S << ')';
	}
};

class ThrowExpr : public Node {
	const Node* Op;
public:
	ThrowExpr(const Node* Op_) : Node(kThrowExpr), Op(Op_) {}
	void printLeft(string_accum& S) const override { Op->print(S << "throw "); }
};

class BoolExpr : public Node {
	bool Value;
public:
	BoolExpr(bool Value_) : Node(kBoolExpr), Value(Value_) {}
	void printLeft(string_accum& S) const override { S << Value; }
};

class IntegerCastExpr : public Node {
	const Node* Ty;
	count_string Integer;
public:
	IntegerCastExpr(const Node* Ty_, count_string Integer) : Node(kIntegerCastExpr), Ty(Ty_), Integer(Integer) {}
	void printLeft(string_accum& S) const override {
		Ty->print(S << '(');
		S << ")" << Integer;
	}
};

class IntegerLiteral : public Node {
	const char *prefix, *suffix;
	count_string Value;
public:
	IntegerLiteral(const char *prefix, const char *suffix, count_string Value) : Node(kIntegerLiteral), prefix(prefix), suffix(suffix), Value(Value) {}
	void printLeft(string_accum& S) const override {
		if (prefix)
			S << "(" << prefix << ")";

		if (Value[0] == 'n') {
			S << '-' << Value.slice(1);
		} else
			S << Value;

		if (suffix)
			S << suffix;
	}
};


namespace float_literal_impl {
constexpr Node::Kind getFloatLiteralKind(float*)		{ return Node::kFloatLiteral; }
constexpr Node::Kind getFloatLiteralKind(double*)		{ return Node::kDoubleLiteral; }
constexpr Node::Kind getFloatLiteralKind(float80*)		{ return Node::kLongDoubleLiteral; }
} // namespace float_literal_impl

template<class Float> class FloatLiteralImpl : public Node {
	Float	f;
	static constexpr Kind KindForClass = float_literal_impl::getFloatLiteralKind((Float*)nullptr);
public:
	FloatLiteralImpl(Float f) : Node(KindForClass), f(f) {}
	void printLeft(string_accum& s) const override { s << f; }
};

using FloatLiteral		= FloatLiteralImpl<float>;
using DoubleLiteral		= FloatLiteralImpl<double>;
using LongDoubleLiteral = FloatLiteralImpl<float80>;

enum OpType {
	OpUnary, OpBinary, OpCast, OpFun, OpTFun, OpUnres, OpCustom,
};
struct Operator { uint16 chars; OpType type; const char *short_name, *full_name; } Operators[] = {
	'aa', OpBinary,	"&&",			"operator&&",			// &&
	'ad', OpUnary,	"&",			"operator&",			// & (unary)
	'an', OpBinary,	"&",			"operator&",			// &
	'aN', OpBinary,	"&=",			"operator&=",			// &=
	'aS', OpBinary,	"=",			"operator=",			// =
	'at', OpTFun,	0,				"alignof",
	'az', OpFun,	0,				"alignof",
	'cc', OpCast,	0,				"const_cast",			// cc <type> <expression>
	'cl', OpBinary,	"()",			"operator()",			// ()
	'cm', OpBinary,	",",			"operator,",			// ,
	'co', OpBinary,	"~",			"operator~",			// ~
	'cv', OpBinary,	0,				"operator cast",		// <type>(cast)
	'da', OpUnary,	"delete[]",		"operator delete[]",	// delete[]
	'dc', OpCast,	0,				"dynamic_cast",
	'de', OpUnary,	"*",			"operator*",			// * (unary)
	'dl', OpUnary,	"delete",		"operator delete",		// delete
	'dn', OpUnres,	0,				0,
	'ds', OpCustom,	0,				".*",
	'dt', OpCustom,	0,				".",
	'dv', OpBinary,	"/",			"operator/",			// /
	'dV', OpBinary,	"/=",			"operator/=",			// /=
	'eo', OpBinary,	"^",			"operator^",			// ^
	'eO', OpBinary,	"^=",			"operator^=",			// ^=
	'eq', OpBinary,	"==",			"operator==",			// ==
	'ge', OpBinary,	">=",			"operator>=",			// >=
	'gt', OpBinary,	">",			"operator>",			// >
	'il', OpCustom,	0,				"init",					//InitListExpr
	'ix', OpBinary,	"[]",			"operator[]",			// []
	'le', OpBinary,	"<=",			"operator<=",			// <=
	'li', OpBinary,	"\"\"",			"operator\"\"",			// <source-name> operator""
	'ls', OpBinary,	"<<",			"operator<<",			// <<
	'lS', OpBinary,	"<<=",			"operator<<=",			// <<=
	'lt', OpBinary,	"<",			"operator<",			// <
	'mi', OpBinary,	"-",			"operator-",			// -
	'mI', OpBinary,	"-=",			"operator-=",			// -=
	'ml', OpBinary,	"*",			"operator*",			// *
	'mL', OpBinary,	"*=",			"operator*=",			// *=
	'mm', OpUnary,	"--", 			"operator--", 			// -- (postfix in <expression> context)
	'na', OpBinary,	"new[]",		"operator new[]",		// new[]
	'ne', OpBinary,	"!=",			"operator!=",			// !=
	'ng', OpBinary,	"-",			"operator-",			// - (unary)
	'nt', OpBinary,	"!",			"operator!",			// !
	'nw', OpBinary,	"new",			"operator new",			// new
	'nx', OpCustom,	0,				"noexcept",
	'on', OpUnres,	0,				0,
	'oo', OpBinary,	"||",			"operator||",			// ||
	'or', OpBinary,	"|",			"operator|",			// |
	'oR', OpBinary,	"|=",			"operator|=",			// |=
	'pm', OpBinary,	"->*",			"operator->*",			// ->*
	'pl', OpBinary,	"+",			"operator+",			// +
	'pL', OpBinary,	"+=",			"operator+=",			// +=
	'pp', OpUnary,	"++", 			"operator++", 			// ++ (postfix in <expression> context)
	'ps', OpBinary,	"+",			"operator+",			// + (unary)
	'pt', OpBinary,	"->",			"operator->",			// ->
	'qu', OpCustom,	"?",			"operator?",			// ?
	'rm', OpBinary,	"%",			"operator%",			// %
	'rM', OpBinary,	"%=",			"operator%=",			// %=
	'rc', OpCast,	0,				"reinterpret_cast",
	'rs', OpBinary,	">>",			"operator>>",			// >>
	'rS', OpBinary,	">>=",			"operator>>=",			// >>=
	'sc', OpCast,	0,				"static_cast",
	'sp', OpCustom,	0,				0,						//ParameterPackExpansion
	'sr', OpUnres,	0,				0,
	'ss', OpBinary,	"<=>",			"operator<=>",			// <=> C++2a
	'st', OpTFun,	0,				"sizeof",				// type
	'sz', OpFun,	0,				"sizeof",				// exp
	'sZ', OpCustom,	0,				"sizeof...",			// FP
	'sP', OpCustom,	0,				"sizeof...",			// Pack
	'te', OpFun,	0,				"typeid",				// exp
	'ti', OpTFun,	0,				"typeid",				// type
	'tl', OpCustom,	0,				"initlist",
	'tr', OpCustom,	0,				"throw",
	'tw', OpCustom,	0,				"throw",				// exp
};

Operator *parse_operator(string_scan &ss) {
	uint16	c = (ss.peekc() << 8) | ss.peekc(1);
	for (auto &i : Operators) {
		if (i.chars == c) {
			ss.move(2);
			return &i;
		}
	}
	return nullptr;
}

struct DemanglerI {
	arena_allocator<4096>	arena;
	string_scan				ss;

	// Name stack, this is used by the parser to hold temporary names that were parsed. The parser collapses multiple names into new nodes to construct the AST. Once the parser is finished, names.size() == 1.
	dynamic_array<Node*, 32>	Names;
	// Substitution table. Itanium supports name substitutions as a means of compression. The string "S42_" refers to the 44nd entry (base-36) in this table.
	dynamic_array<Node*, 32>	Subs;
	// Template parameter table. Like the above, but referenced like "T42_". This has a smaller size compared to Subs and Names because it can be stored on the stack.
	dynamic_array<Node*, 8>		TemplateParams;
	// Set of unresolved forward <template-param> references. These can occur in a conversion operator's type, and are resolved in the enclosing <encoding>.
	dynamic_array<ForwardTemplateReference*, 4> ForwardTemplateRefs;

	bool TryToParseTemplateArgs			 = true;
	bool PermitForwardTemplateReferences = false;
	bool ParsingLambdaParams			 = false;

	DemanglerI(const char* First_, const char* Last_) : ss(First_, Last_) {}

	void reset(const char* First_, const char* Last_) {
		ss = string_scan(First_, Last_);
		Names.clear();
		Subs.clear();
		TemplateParams.clear();
		ParsingLambdaParams				= false;
		TryToParseTemplateArgs			= true;
		PermitForwardTemplateReferences = false;
	}

	template<class It> NodeArray makeNodeArray(It begin, It end) {
		size_t	sz		= static_cast<size_t>(end - begin);
		auto	*data	= arena.make_array<Node*>(sz);
		copy(begin, end, data);
		return NodeArray(data, sz);
	}

	NodeArray popTrailingNodeArray(size_t FromPosition) {
		ISO_ASSERT(FromPosition <= Names.size());
		NodeArray res = makeNodeArray(Names.begin() + (long)FromPosition, Names.end());
		Names.resize(FromPosition);
		return res;
	}

	count_string	parseNumber(bool AllowNegative = false);
	Qualifiers		parseCVQualifiers();
	bool			parsePositiveInteger(size_t* Out);
	count_string	parseBareSourceName();

	bool			parseSeqId(size_t* Out);
	Node*			parseSubstitution();
	Node*			parseTemplateParam();
	Node*			parseTemplateArgs(bool TagTemplates = false);
	Node*			parseTemplateArg();

	/// Parse the <expr> production.
	Node*			parseExpr();
	Node*			parseIntegerLiteral(const char *prefix, const char *suffix) {
		auto Tmp = parseNumber(true);
		return ss.check('E') ? arena.make<IntegerLiteral>(prefix, suffix, Tmp) : nullptr;
	}

	Node*			parseExprPrimary();
	template<class Float> Node* parseFloatingLiteral();
	Node*			parseFunctionParam();
	Node*			parseBracedExpr();

	/// Parse the <type> production.
	Node*			parseType();
	Node*			parseFunctionType();
	Node*			parseDecltype() {
		if (Node* E = parseExpr()) {
			if (ss.check('E'))
				return arena.make<EnclosingExpr>("decltype(", E, ")");
		}
		return nullptr;
	}

	Node*			parseEncoding();
	bool			parseCallOffset();
	Node*			parseSpecialName();

	/// Holds some extra information about a <name> that is being parsed. This information is only pertinent if the <name> refers to an <encoding>.
	struct NameState {
		bool			CtorDtorConversion		= false;
		bool			EndsWithTemplateArgs	= false;
		Qualifiers		CVQualifiers			= QualNone;
		FunctionRefQual ReferenceQualifier		= FrefQualNone;
		size_t			ForwardTemplateRefsBegin;
		NameState(DemanglerI* Enclosing) : ForwardTemplateRefsBegin(Enclosing->ForwardTemplateRefs.size()) {}
	};

	bool resolveForwardTemplateRefs(NameState& State) {
		size_t I = State.ForwardTemplateRefsBegin;
		size_t E = ForwardTemplateRefs.size();
		for (; I < E; ++I) {
			size_t Idx = ForwardTemplateRefs[I]->Index;
			if (Idx >= TemplateParams.size())
				return false;
			ForwardTemplateRefs[I]->Ref = TemplateParams[Idx];
		}
		ForwardTemplateRefs.resize(State.ForwardTemplateRefsBegin);
		return true;
	}

	/// Parse the <name> production>
	Node*			parseName(NameState* State = nullptr);
	Node*			parseOperatorName(NameState* State);
	Node*			parseUnqualifiedName(NameState* State);
	Node*			parseUnnamedTypeName(NameState* State);
	Node*			parseSourceName(NameState* State);
	Node*			parseUnscopedName(NameState* State);
	Node*			parseNestedName(NameState* State);
	Node*			parseCtorDtorName(Node*& SoFar, NameState* State);

	Node*			parseAbiTags(Node* N);

	/// Parse the <unresolved-name> production.
	Node*			parseUnresolvedName();
	Node*			parseSimpleId();
	Node*			parseBaseUnresolvedName();
	Node*			parseUnresolvedType();

	/// Top-level entry point into the parser.
	Node*			parse();
};

template<class Float> Node* DemanglerI::parseFloatingLiteral() {
	uint64	val = 0;
	char	c;
	while (is_hex(c = ss.getc()) && !is_upper(c))
		val = val * 16 + from_digit(c);

	if (c != 'E')
		return nullptr;

	return arena.make<FloatLiteralImpl<Float>>((Float&)val);
}


// <discriminator>	:= _ <non-negative number>		# when number < 10
//					:= __ <non-negative number> _	# when number >= 10
// extension		:= decimal-digit+				# at the end of string
void parse_discriminator(string_scan &ss) {
	string_scan save_ss = ss;

	// parse but ignore discriminator
	char	c = ss.getc();
	if (c == '_') {
		c = ss.getc();
		if (is_digit(c))
			return;
		if (c == '_') {
			while (is_digit(ss.getc()));
			if (ss.peekc(-1) == '_')
				return;
		}
	} else if (is_digit(c)) {
		while (is_digit(ss.getc()));
		if (!ss.remaining())
			return;
	}
	ss = save_ss;
}

// <name>	::= <nested-name> // N
//			::= <local-name> # See Scope Encoding below // Z
//			::= <unscoped-template-name> <template-args>
//			::= <unscoped-name>
//
// <unscoped-template-name>	::= <unscoped-name>
//							::= <substitution>
Node* DemanglerI::parseName(NameState* State) {
	ss.check('L');		// extension

	if (ss.check('N'))
		return parseNestedName(State);

	if (ss.check('Z')) {
		// <local-name>	:= Z <function encoding> E <entity name> [<discriminator>]
		//				:= Z <function encoding> E s [<discriminator>]
		//				:= Z <function encoding> Ed [ <parameter number> ] _ <entity name>
		if (Node* Encoding = parseEncoding()) {
			if (ss.check('E')) {
				if (ss.check('s')) {
					parse_discriminator(ss);
					if (auto* StringLitName = arena.make<NameType>("string literal"))
						return arena.make<LocalName>(Encoding, StringLitName);

				} else if (ss.check('d')) {
					parseNumber(true);
					if (ss.check('_')) {
						if (Node* N = parseName(State))
							return arena.make<LocalName>(Encoding, N);
					}

				} else if (Node* Entity = parseName(State)) {
					parse_discriminator(ss);
					return arena.make<LocalName>(Encoding, Entity);
				}
			}
		}

	} else if (ss.peekc() == 'S' && ss.peekc(1) != 't') {
		//		::= <unscoped-template-name> <template-args>
		if (Node* S = parseSubstitution()) {
			if (ss.peekc() == 'I') {
				if (Node* TA = parseTemplateArgs(State != nullptr)) {
					if (State)
						State->EndsWithTemplateArgs = true;
					return arena.make<NameWithTemplateArgs>(S, TA);
				}
			}
		}

	} else if (Node* N = parseUnscopedName(State)) {
		//		::= <unscoped-template-name> <template-args>
		if (ss.peekc() == 'I') {
			Subs.push_back(N);
			if (Node* TA = parseTemplateArgs(State != nullptr)) {
				if (State)
					State->EndsWithTemplateArgs = true;
				return arena.make<NameWithTemplateArgs>(N, TA);
			}
		}
		//		::= <unscoped-name>
		return N;
	}
	return nullptr;
}

// <unscoped-name>	::= <unqualified-name>
//					::= St <unqualified-name>   # ::
// extension		::= StL<unqualified-name>
Node* DemanglerI::parseUnscopedName(NameState* State) {
	if (ss.check("St")) {
		ss.check('L');
		if (Node* R = parseUnqualifiedName(State))
			return arena.make<StdQualifiedName>(R);
		return nullptr;
	}
	return parseUnqualifiedName(State);
}

// <unqualified-name>	::= <operator-name> [abi-tags]
//						::= <ctor-dtor-name>
//						::= <source-name>
//						::= <unnamed-type-name>
//						::= DC <source-name>+ E		# structured binding declaration
Node* DemanglerI::parseUnqualifiedName(NameState* State) {
	// <ctor-dtor-name>s are special-cased in parseNestedName().
	Node* Result;
	if (ss.peekc() == 'U') {
		Result = parseUnnamedTypeName(State);
	} else if (between(ss.peekc(), '1', '9')) {
		Result = parseSourceName(State);
	} else if (ss.check("DC")) {
		size_t BindingsBegin = Names.size();
		do {
			Node* Binding = parseSourceName(State);
			if (Binding == nullptr)
				return nullptr;
			Names.push_back(Binding);
		} while (!ss.check('E'));
		Result = arena.make<StructuredBindingName>(popTrailingNodeArray(BindingsBegin));
	} else {
		Result = parseOperatorName(State);
	}

	if (Result)
		Result = parseAbiTags(Result);
	return Result;
}

// <unnamed-type-name>	::= Ut [<nonnegative number>] _
//						::= <closure-type-name>
//
// <closure-type-name> ::= Ul <lambda-sig> E [ <nonnegative number> ] _
//
// <lambda-sig> ::= <parameter type>+	# Parameter types or "v" if the lambda has no parameters
Node* DemanglerI::parseUnnamedTypeName(NameState*) {
	if (ss.check("Ut")) {
		auto	Count = parseNumber();
		if (!ss.check('_'))
			return nullptr;
		return arena.make<UnnamedTypeName>(Count);
	}
	if (ss.check("Ul")) {
		NodeArray			 Params;
		saver<bool> SwapParams(ParsingLambdaParams, true);
		if (!ss.check("vE")) {
			size_t ParamsBegin = Names.size();
			do {
				Node* P = parseType();
				if (P == nullptr)
					return nullptr;
				Names.push_back(P);
			} while (!ss.check('E'));
			Params = popTrailingNodeArray(ParamsBegin);
		}
		auto	Count = parseNumber();
		if (!ss.check('_'))
			return nullptr;
		return arena.make<ClosureTypeName>(Params, Count);
	}
	return nullptr;
}

// <source-name> ::= <positive length number> <identifier>
Node* DemanglerI::parseSourceName(NameState*) {
	size_t Length = 0;
	if (parsePositiveInteger(&Length) && Length && ss.remaining() >= Length) {
		auto	Name = ss.get_n((int)Length);
		return Name.begins("_GLOBAL__N")
			? arena.make<NameType>("(anonymous namespace)")
			: arena.make<NameType>(Name);
	}
	return nullptr;
}

Node* DemanglerI::parseOperatorName(NameState* State) {
	uint16	c = (ss.peekc() << 8) | ss.peekc(1);
	if ((c >> 8) == 'v' && is_digit(c & 0xff)) {
		ss.move(2);
		if (Node *SN = parseSourceName(State))
			return arena.make<ConversionOperatorType>(SN);
	}
	switch (c) {
		case 'cv':
			ss.move(2);
			// If we're parsing an encoding, State != nullptr and the conversion operators' <type> could have a <template-param> that refers to some <template-arg>s further ahead in the mangled name
			if (Node *Ty = (save(TryToParseTemplateArgs, false), save(PermitForwardTemplateReferences, PermitForwardTemplateReferences || State), parseType())) {
				if (State)
					State->CtorDtorConversion = true;
				return arena.make<ConversionOperatorType>(Ty);
			}
			break;

		case 'li':
			ss.move(2);
			if (Node* SN = parseSourceName(State))
				return arena.make<LiteralOperator>(SN);
			break;

		default:
			if (auto *op = parse_operator(ss))
				return arena.make<NameType>(count_string(op->full_name));
			break;
	}
	return nullptr;
}

// <ctor-dtor-name>		::= C1	# complete object constructor
//						::= C2	# base object constructor
//						::= C3	# complete object allocating constructor
//   extension			::= C5	# ?
//						::= D0	# deleting destructor
//						::= D1	# complete object destructor
//						::= D2	# base object destructor
//   extension			::= D5	# ?
Node* DemanglerI::parseCtorDtorName(Node*& SoFar, NameState* State) {
	if (SoFar->kind == Node::kSpecialSubstitution) {
		auto SSK = static_cast<SpecialSubstitution*>(SoFar)->SSK;
		switch (SSK) {
			case SpecialSubKind::string:
			case SpecialSubKind::istream:
			case SpecialSubKind::ostream:
			case SpecialSubKind::iostream:
				SoFar = arena.make<ExpandedSpecialSubstitution>(SSK);
				if (!SoFar)
					return nullptr;
			default: break;
		}
	}

	if (ss.check('C')) {
		bool	IsInherited	= ss.check('I');
		int		Variant		= ss.getc() - '0';
		if (is_any(Variant, 1, 2, 3, 5)) {
			if (State)
				State->CtorDtorConversion = true;
			if (!IsInherited || parseName(State))
				return arena.make<CtorDtorName>(SoFar, false, Variant);
		}

	} else if (ss.check('D')) {
		int Variant = ss.getc() - '0';
		if (is_any(Variant, 0, 1, 2, 5)) {
			if (State)
				State->CtorDtorConversion = true;
			return arena.make<CtorDtorName>(SoFar, true, Variant);
		}
	}

	return nullptr;
}

// <nested-name>::= N [<CV-Qualifiers>] [<ref-qualifier>] <prefix> <unqualified-name> E
//				::= N [<CV-Qualifiers>] [<ref-qualifier>] <template-prefix> <template-args> E
//
// <prefix>		::= <prefix> <unqualified-name>
//				::= <template-prefix> <template-args>
//				::= <template-param>
//				::= <decltype>
//				::= # empty
//				::= <substitution>
//				::= <prefix> <data-member-prefix>
//  extension	::= L
//
// <data-member-prefix> := <member source-name> [<template-args>] M
//
// <template-prefix>	::= <prefix> <template unqualified-name>
//						::= <template-param>
//						::= <substitution>
Node* DemanglerI::parseNestedName(NameState* State) {
	Qualifiers CVTmp = parseCVQualifiers();
	if (State)
		State->CVQualifiers = CVTmp;

	if (ss.check('O')) {
		if (State)
			State->ReferenceQualifier = FrefQualRValue;
	} else if (ss.check('R')) {
		if (State)
			State->ReferenceQualifier = FrefQualLValue;
	} else if (State)
		State->ReferenceQualifier = FrefQualNone;

	Node*	SoFar			= nullptr;
	auto	PushComponent	= [&](Node* Comp) {
		 if (!Comp)
			 return false;
		 if (SoFar)
			 SoFar = arena.make<NestedName>(SoFar, Comp);
		 else
			 SoFar = Comp;
		 if (State)
			 State->EndsWithTemplateArgs = false;
		 return SoFar != nullptr;
	};

	if (ss.check("St")) {
		SoFar = arena.make<NameType>("std");
		if (!SoFar)
			return nullptr;
	}

	while (!ss.check('E')) {
		ss.check('L');		// extension

		// <data-member-prefix> := <member source-name> [<template-args>] M
		if (ss.check('M')) {
			if (SoFar == nullptr)
				return nullptr;
			continue;
		}

		//			::= <template-param>
		if (ss.check('T')) {
			if (!PushComponent(parseTemplateParam()))
				return nullptr;
			Subs.push_back(SoFar);
			continue;
		}

		//			::= <template-prefix> <template-args>
		if (ss.peekc() == 'I') {
			Node* TA = parseTemplateArgs(State != nullptr);
			if (TA == nullptr || SoFar == nullptr)
				return nullptr;
			SoFar = arena.make<NameWithTemplateArgs>(SoFar, TA);
			if (!SoFar)
				return nullptr;
			if (State)
				State->EndsWithTemplateArgs = true;
			Subs.push_back(SoFar);
			continue;
		}

		//			::= <decltype>
		if (ss.check("DT") || ss.check("Dt")) {
			if (!PushComponent(parseDecltype()))
				return nullptr;
			Subs.push_back(SoFar);
			continue;
		}

		//			::= <substitution>
		if (ss.peekc() == 'S' && ss.peekc(1) != 't') {
			Node* S = parseSubstitution();
			if (!PushComponent(S))
				return nullptr;
			if (SoFar != S)
				Subs.push_back(S);
			continue;
		}

		// Parse an <unqualified-name> thats actually a <ctor-dtor-name>.
		if (ss.peekc() == 'C' || (ss.peekc() == 'D' && ss.peekc(1) != 'C')) {
			if (SoFar == nullptr)
				return nullptr;
			if (!PushComponent(parseCtorDtorName(SoFar, State)))
				return nullptr;
			SoFar = parseAbiTags(SoFar);
			if (SoFar == nullptr)
				return nullptr;
			Subs.push_back(SoFar);
			continue;
		}

		//			::= <prefix> <unqualified-name>
		if (!PushComponent(parseUnqualifiedName(State)))
			return nullptr;
		Subs.push_back(SoFar);
	}

	if (!SoFar || Subs.empty())
		return nullptr;

	Subs.pop_back();
	return SoFar;
}

// <simple-id> ::= <source-name> [ <template-args> ]
Node* DemanglerI::parseSimpleId() {
	if (Node* SN = parseSourceName(/*NameState=*/nullptr)) {
		if (ss.peekc() == 'I') {
			if (Node* TA = parseTemplateArgs())
				return arena.make<NameWithTemplateArgs>(SN, TA);
		} else {
			return SN;
		}
	}
	return nullptr;
}

// <unresolved-type>	::= <template-param>
//						::= <decltype>
//						::= <substitution>
Node* DemanglerI::parseUnresolvedType() {
	if (ss.check('T')) {
		if (Node* TP = parseTemplateParam()) {
			Subs.push_back(TP);
			return TP;
		}
		return nullptr;
	}
	if (ss.check("DT") || ss.check("Dt")) {
		if (Node* DT = parseDecltype()) {
			Subs.push_back(DT);
			return DT;
		}
		return nullptr;
	}
	return parseSubstitution();
}

// <base-unresolved-name>	::= <simple-id>									# unresolved name
//			extension		::= <operator-name>								# unresolved operator-function-id
//			extension		::= <operator-name> <template-args>				# unresolved operator template-id
//							::= on <operator-name>							# unresolved operator-function-id
//							::= on <operator-name> <template-args>			# unresolved operator template-id
//							::= dn <destructor-name>						# destructor or pseudo-destructor; e.g. ~X or ~X<N-1>
Node* DemanglerI::parseBaseUnresolvedName() {
	if (is_digit(ss.peekc()))
		return parseSimpleId();

	if (ss.check("dn")) {
		if (Node* Result = is_digit(ss.peekc()) ? parseSimpleId() : parseUnresolvedType())
			return arena.make<DtorName>(Result);
	} else {
		ss.check("on");
		if (Node* Oper = parseOperatorName(/*NameState=*/nullptr)) {
			if (ss.peekc() == 'I') {
				if (Node* TA = parseTemplateArgs())
					return arena.make<NameWithTemplateArgs>(Oper, TA);
			} else {
				return Oper;
			}
		}
	}
	return nullptr;
}

// <unresolved-name>
// extension		::= srN <unresolved-type> [<template-args>] <unresolved-qualifier-level>* E <base-unresolved-name>
//					::= [gs] <base-unresolved-name>							# x or (with "gs") ::x
//					::= [gs] sr <unresolved-qualifier-level>+ E <base-unresolved-name>
//																			# A::x, N::y, A<T>::z; "gs" means leading "::"
//					::= sr <unresolved-type> <base-unresolved-name>			# T::x / decltype(p)::x
// extension		::= sr <unresolved-type> <template-args> <base-unresolved-name>
//																			# T::N::x /decltype(p)::N::x
// (ignored)		::= srN <unresolved-type> <unresolved-qualifier-level>+ E <base-unresolved-name>
//
// <unresolved-qualifier-level> ::= <simple-id>
Node* DemanglerI::parseUnresolvedName() {
	Node* SoFar = nullptr;

	// srN <unresolved-type> [<template-args>]	<unresolved-qualifier-level>* E <base-unresolved-name>
	// srN <unresolved-type>					<unresolved-qualifier-level>+ E <base-unresolved-name>
	if (ss.check("srN")) {
		if (SoFar = parseUnresolvedType()) {
			if (ss.peekc() == 'I') {
				Node* TA = parseTemplateArgs();
				if (TA == nullptr)
					return nullptr;
				SoFar = arena.make<NameWithTemplateArgs>(SoFar, TA);
			}

			while (!ss.check('E')) {
				Node* Qual = parseSimpleId();
				if (Qual == nullptr)
					return nullptr;
				SoFar = arena.make<QualifiedName>(SoFar, Qual);
			}

			if (Node* Base = parseBaseUnresolvedName())
				return arena.make<QualifiedName>(SoFar, Base);
		}
		return nullptr;
	}

	bool Global = ss.check("gs");

	// [gs] <base-unresolved-name>					# x or (with "gs") ::x
	if (!ss.check("sr")) {
		if (SoFar = parseBaseUnresolvedName()) {
			if (Global)
				SoFar = arena.make<GlobalQualifiedName>(SoFar);
			return SoFar;
		}
		return nullptr;
	}

	// [gs] sr <unresolved-qualifier-level>+ E		<base-unresolved-name>
	if (is_digit(ss.peekc())) {
		do {
			Node* Qual = parseSimpleId();
			if (!Qual)
				return nullptr;
			if (SoFar)
				SoFar = arena.make<QualifiedName>(SoFar, Qual);
			else if (Global)
				SoFar = arena.make<GlobalQualifiedName>(Qual);
			else
				SoFar = Qual;
		} while (!ss.check('E'));
	}
	//		sr <unresolved-type>					<base-unresolved-name>
	//		sr <unresolved-type> <template-args>	<base-unresolved-name>
	else {
		if (SoFar = parseUnresolvedType()) {
			if (ss.peekc() == 'I') {
				if (Node* TA = parseTemplateArgs())
					SoFar = arena.make<NameWithTemplateArgs>(SoFar, TA);
				else
					return nullptr;
			}
		} else {
			return nullptr;
		}
	}

	if (Node* Base = parseBaseUnresolvedName())
		return arena.make<QualifiedName>(SoFar, Base);
	return nullptr;
}

// <abi-tags> ::= <abi-tag> [<abi-tags>]
// <abi-tag> ::= B <source-name>
Node* DemanglerI::parseAbiTags(Node* N) {
	while (ss.check('B')) {
		auto SN = parseBareSourceName();
		if (SN.empty())
			return nullptr;
		N = arena.make<AbiTagAttr>(N, SN);
		if (!N)
			return nullptr;
	}
	return N;
}

// <number> ::= [n] <non-negative decimal integer>
count_string DemanglerI::parseNumber(bool AllowNegative) {
	const char* Tmp = ss.getp();
	if (AllowNegative)
		ss.check('n');
	if (ss.remaining() == 0 || !is_digit(ss.peekc()))
		return {};
	while (is_digit(ss.peekc()))
		ss.move(1);
	return {Tmp, ss.getp()};
}

// <positive length number> ::= [0-9]*
bool DemanglerI::parsePositiveInteger(size_t* Out) {
	*Out = 0;
	if (ss.peekc() < '0' || ss.peekc() > '9')
		return false;
	while (ss.peekc() >= '0' && ss.peekc() <= '9') {
		*Out *= 10;
		*Out += static_cast<size_t>(ss.getc() - '0');
	}
	return true;
}

count_string DemanglerI::parseBareSourceName() {
	size_t Int = 0;
	if (!parsePositiveInteger(&Int) || ss.remaining() < Int)
		return {};
	return ss.get_n((int)Int);
}

// <function-type> ::= [<CV-qualifiers>] [<exception-spec>] [Dx] F [Y] <bare-function-type> [<ref-qualifier>] E
//
// <exception-spec>	::= Do					# non-throwing exception-specification (e.g., noexcept, throw())
//					::= DO <expression> E	# computed (instantiation-dependent) noexcept
//					::= Dw <type>+ E		# dynamic exception specification with instantiation-dependent types
//
// <ref-qualifier> ::= R					# & ref-qualifier
// <ref-qualifier> ::= O					# && ref-qualifier
Node* DemanglerI::parseFunctionType() {
	Qualifiers CVQuals = parseCVQualifiers();

	Node* ExceptionSpec = nullptr;
	if (ss.check("Do")) {
		ExceptionSpec = arena.make<NameType>("noexcept");
		if (!ExceptionSpec)
			return nullptr;
	} else if (ss.check("DO")) {
		Node* E = parseExpr();
		if (E == nullptr || !ss.check('E'))
			return nullptr;
		ExceptionSpec = arena.make<NoexceptSpec>(E);
		if (!ExceptionSpec)
			return nullptr;
	} else if (ss.check("Dw")) {
		size_t SpecsBegin = Names.size();
		while (!ss.check('E')) {
			Node* T = parseType();
			if (T == nullptr)
				return nullptr;
			Names.push_back(T);
		}
		ExceptionSpec = arena.make<DynamicExceptionSpec>(popTrailingNodeArray(SpecsBegin));
		if (!ExceptionSpec)
			return nullptr;
	}

	ss.check("Dx");		// transaction safe

	if (!ss.check('F'))
		return nullptr;
	ss.check('Y');		// extern "C"
	Node* ReturnType = parseType();
	if (ReturnType == nullptr)
		return nullptr;

	FunctionRefQual ReferenceQualifier	= FrefQualNone;
	size_t			ParamsBegin			= Names.size();
	while (true) {
		if (ss.check('E'))
			break;
		if (ss.check('v'))
			continue;
		if (ss.check("RE")) {
			ReferenceQualifier = FrefQualLValue;
			break;
		}
		if (ss.check("OE")) {
			ReferenceQualifier = FrefQualRValue;
			break;
		}
		Node* T = parseType();
		if (T == nullptr)
			return nullptr;
		Names.push_back(T);
	}

	NodeArray Params = popTrailingNodeArray(ParamsBegin);
	return arena.make<FunctionType>(ReturnType, Params, CVQuals, ReferenceQualifier, ExceptionSpec);
}

// <type>		::= <builtin-type>
//				::= <qualified-type>
//				::= <function-type>
//				::= <class-enum-type>
//				::= <array-type>
//				::= <pointer-to-member-type>
//				::= <template-param>
//				::= <template-template-param> <template-args>
//				::= <decltype>
//				::= P <type>		# pointer
//				::= R <type>		# l-value reference
//				::= O <type>		# r-value reference (C++11)
//				::= C <type>		# complex pair (C99)
//				::= G <type>		# imaginary (C99)
//				::= <substitution>	# See Compression below
// extension	::= U <objc-name> <objc-type>	# objc-type<identifier>
// extension	::= <vector-type>	# <vector-type> starts with Dv
//
// <objc-name> ::= <k0 number> objcproto <k1 number> <identifier>	# k0 = 9 + <number of digits in k1> + k1
// <objc-type> ::= <source-name>	# PU<11+>objcproto 11objc_object<source-name> 11objc_object -> id<source-name>
Node* DemanglerI::parseType() {
	Node*		Result	= nullptr;
	if (Qualifiers Quals = parseCVQualifiers()) {
		if (Node *Ty = parseType()) {
			Result	= arena.make<QualType>(Ty, Quals);
			Subs.push_back(Result);
			return Result;
		}
		return nullptr;
	}

	switch (ss.getc()) {
		case 'U':
			if (auto Qual = parseBareSourceName()) {
				// FIXME parse the optional <template-args> here!
				// extension			::= U <objc-name> <objc-type>	# objc-type<identifier>
				if (Qual.begins("objcproto")) {
					if (count_string Proto = (save(ss, Qual.slice((int)strlen("objcproto"))), parseBareSourceName())) {
						if (Node* Child = parseType())
							Result = arena.make<ObjCProtoName>(Child, Proto);
					}
				} else if (Node* Child = parseType()) {
					Result = arena.make<VendorExtQualType>(Child, Qual);
				}
			}
			break;
		case 'v': return arena.make<NameType>("void");
		case 'w': return arena.make<NameType>("wchar_t");
		case 'b': return arena.make<NameType>("bool");
		case 'c': return arena.make<NameType>("char");
		case 'a': return arena.make<NameType>("signed char");
		case 'h': return arena.make<NameType>("uint8");
		case 's': return arena.make<NameType>("short");
		case 't': return arena.make<NameType>("unsigned short");
		case 'i': return arena.make<NameType>("int");
		case 'j': return arena.make<NameType>("unsigned int");
		case 'l': return arena.make<NameType>("long");
		case 'm': return arena.make<NameType>("unsigned long");
		case 'x': return arena.make<NameType>("long long");
		case 'y': return arena.make<NameType>("unsigned long long");
		case 'n': return arena.make<NameType>("__int128");
		case 'o': return arena.make<NameType>("unsigned __int128");
		case 'f': return arena.make<NameType>("float");
		case 'd': return arena.make<NameType>("double");
		case 'e': return arena.make<NameType>("long double");
		case 'g': return arena.make<NameType>("__float128");
		case 'z': return arena.make<NameType>("...");

		// <builtin-type> ::= u <source-name>		# vendor extended type
		case 'u': {
			ss.move(1);
			auto	Res = parseBareSourceName();
			if (Res.empty())
				return nullptr;
			return arena.make<NameType>(Res);
		}
		case 'D':
			switch (ss.getc()) {
				case 'd': return arena.make<NameType>("decimal64");
				case 'e': return arena.make<NameType>("decimal128");
				case 'f': return arena.make<NameType>("decimal32");
				case 'h': return arena.make<NameType>("decimal16");
				case 'i': return arena.make<NameType>("char32_t");
				case 's': return arena.make<NameType>("char16_t");
				case 'a': return arena.make<NameType>("auto");
				case 'c': return arena.make<NameType>("decltype(auto)");
				case 'n': return arena.make<NameType>("nullptr_t");

				//             ::= <decltype>
				case 't':
				case 'T':
					Result = parseDecltype();
					break;
				// extension   ::= <vector-type> # <vector-type> starts with Dv
				case 'v':
					if (between(ss.peekc(), '1', '9')) {
						auto	DimensionNumber = parseNumber();
						if (ss.check('_')) {
							if (ss.check('p'))
								Result = arena.make<PixelVectorType>(DimensionNumber);
							else if (Node* ElemType = parseType())
								Result = arena.make<VectorType>(ElemType, arena.make<NameType>(DimensionNumber));
						}
					} else if (!ss.check('_')) {
						if (Node* DimExpr = parseExpr()) {
							if (ss.check('_')) {
								if (Node* ElemType = parseType())
									Result = arena.make<VectorType>(ElemType, DimExpr);
							}
						}
					} else if (Node* ElemType = parseType()) {
						Result = arena.make<VectorType>(ElemType, nullptr);
					}
					break;
				//           ::= Dp <type>       # pack expansion (C++0x)
				case 'p':
					if (Node* Child = parseType())
						Result = arena.make<ParameterPackExpansion>(Child);
					break;
				// Exception specifier on a function type.
				case 'o':
				case 'O':
				case 'w':
				// Transaction safe function type.
				case 'x':
					ss.move(-2);
					Result = parseFunctionType();
					break;
			}
			break;
		case 'F':			Result = parseFunctionType();		break;
		case 'A': {
			// <array-type>		::= A <positive dimension number> _ <element type>
			//					::= A [<dimension expression>] _ <element type>
			const Node* Dimension;
			if (is_digit(ss.peekc())) {
				Dimension = arena.make<NameType>(parseNumber());
			} else if (!ss.check('_')) {
				Dimension = parseExpr();
				if (!Dimension)
					break;
			}
			if (ss.check('_')) {
				if (Node* Ty = parseType())
					Result = arena.make<ArrayType>(Ty, Dimension);
			}
			break;
		}
		case 'M':
		// <pointer-to-member-type> ::= M <class type> <member type>
			if (Node* ClassType = parseType()) {
				if (Node* MemberType = parseType())
					Result = arena.make<PointerToMemberType>(ClassType, MemberType);
			}
			break;
		//             ::= <template-param>
		case 'T': {
			// This could be an elaborate type specifier on a <class-enum-type>.
			const char *ElabSpef = 0;
//			::= Ts <name>	# dependent elaborated type specifier using 'struct' or 'class'
//			::= Tu <name>	# dependent elaborated type specifier using 'union'
//			::= Te <name>	# dependent elaborated type specifier using 'enum'
			if (ss.check('s'))
				ElabSpef = "struct";
			else if (ss.check('u'))
				ElabSpef = "union";
			else if (ss.check('e'))
				ElabSpef = "enum";
			if (ElabSpef) {
				if (Node* Name = parseName())
					Result = arena.make<ElaboratedTypeSpefType>(ElabSpef, Name);
				break;
			}
			if (Result = parseTemplateParam()) {
				// Result could be either of:
				//   <type>        ::= <template-param>
				//   <type>        ::= <template-template-param> <template-args>
				//
				//   <template-template-param> ::= <template-param>
				//                             ::= <substitution>
				//
				// If this is followed by some <template-args>, and we're permitted to parse them, take the second production.
				if (TryToParseTemplateArgs && ss.peekc() == 'I') {
					Node* TA = parseTemplateArgs();
					if (TA == nullptr)
						return nullptr;
					Result = arena.make<NameWithTemplateArgs>(Result, TA);
				}
			}
			break;
		}
		//             ::= P <type>        # pointer
		case 'P':
			if (Node* Ptr = parseType())
				Result = arena.make<PointerType>(Ptr);
			break;
		//             ::= R <type>        # l-value reference
		case 'R':
			if (Node* Ref = parseType())
				Result = arena.make<ReferenceType>(Ref, ReferenceKind::LValue);
			break;
		//             ::= O <type>        # r-value reference (C++11)
		case 'O':
			if (Node* Ref = parseType())
				Result = arena.make<ReferenceType>(Ref, ReferenceKind::RValue);
			break;
		//             ::= C <type>        # complex pair (C99)
		case 'C':
			if (Node* P = parseType())
				Result = arena.make<PostfixQualifiedType>(P, " complex");
			break;
		//             ::= G <type>        # imaginary (C99)
		case 'G':
			if (Node* P = parseType())
				Result = arena.make<PostfixQualifiedType>(P, " imaginary");
			break;
		//             ::= <substitution>  # See Compression below
		case 'S': {
			if (ss.peekc() != 't') {
				Node* Sub = parseSubstitution();
				if (Sub == nullptr)
					return nullptr;

				// Sub could be either of:
				//   <type>        ::= <substitution>
				//   <type>        ::= <template-template-param> <template-args>
				//
				//   <template-template-param> ::= <template-param>
				//                             ::= <substitution>
				//
				// If this is followed by some <template-args>, and we're permitted to
				// parse them, take the second production.

				if (TryToParseTemplateArgs && ss.peekc() == 'I') {
					Node* TA = parseTemplateArgs();
					if (TA == nullptr)
						return nullptr;
					Result = arena.make<NameWithTemplateArgs>(Sub, TA);
					break;
				}

				// If all we parsed was a substitution, don't re-insert into the
				// substitution table.
				return Sub;
			}
			// fall through
		}
		//        ::= <class-enum-type>
		default:
			ss.move(-1);
			if (Node* Name = parseName())
				Result = Name;
			break;
	}

	// If we parsed a type, insert it into the substitution table. Note that all <builtin-type>s and <substitution>s have already bailed out, because they don't get substitutions.
	if (Result)
		Subs.push_back(Result);
	return Result;
}

// <CV-Qualifiers> ::= [r] [V] [K]
Qualifiers DemanglerI::parseCVQualifiers() {
	Qualifiers CVR = QualNone;
	if (ss.check('r'))
		CVR |= QualRestrict;
	if (ss.check('V'))
		CVR |= QualVolatile;
	if (ss.check('K'))
		CVR |= QualConst;
	return CVR;
}

// <function-param> ::= fp <top-level CV-Qualifiers> _                                     # L == 0, first parameter
//                  ::= fp <top-level CV-Qualifiers> <parameter-2 non-negative number> _   # L == 0, second and later parameters
//                  ::= fL <L-1 non-negative number> p <top-level CV-Qualifiers> _         # L > 0, first parameter
//                  ::= fL <L-1 non-negative number> p <top-level CV-Qualifiers> <parameter-2 non-negative number> _   # L > 0, second and later parameters
Node* DemanglerI::parseFunctionParam() {
	if (ss.check("fp")) {
		parseCVQualifiers();
		auto Num = parseNumber();
		if (ss.check('_'))
			return arena.make<FunctionParam>(Num);

	} else if (ss.check("fL")) {
		if (parseNumber() && ss.check('p')) {
			parseCVQualifiers();
			auto Num = parseNumber();
			if (ss.check('_'))
				return arena.make<FunctionParam>(Num);
		}
	}
	return nullptr;
}

// <expr-primary> ::= L <type> <value number> E                          # integer literal
//                ::= L <type> <value float> E                           # floating literal
//                ::= L <string type> E                                  # string literal
//                ::= L <nullptr type> E                                 # nullptr literal (i.e., "LDnE")
// FIXME:         ::= L <type> <real-part float> _ <imag-part float> E   # complex floating point literal (C 2000)
//                ::= L <mangled-name> E                                 # external name
Node* DemanglerI::parseExprPrimary() {
	switch (ss.peekc()) {
		case 'w': ss.move(1); return parseIntegerLiteral("wchar_t", 0);
		case 'b':
			if (ss.check("b0E"))
				return arena.make<BoolExpr>(0);
			if (ss.check("b1E"))
				return arena.make<BoolExpr>(1);
			return nullptr;
		case 'c': ss.move(1); return parseIntegerLiteral("char", 0);
		case 'a': ss.move(1); return parseIntegerLiteral("signed char", 0);
		case 'h': ss.move(1); return parseIntegerLiteral("uint8", 0);
		case 's': ss.move(1); return parseIntegerLiteral("short", 0);
		case 't': ss.move(1); return parseIntegerLiteral("unsigned short", 0);
		case 'i': ss.move(1); return parseIntegerLiteral(0, 0);
		case 'j': ss.move(1); return parseIntegerLiteral(0, "u");
		case 'l': ss.move(1); return parseIntegerLiteral(0, "l");
		case 'm': ss.move(1); return parseIntegerLiteral(0, "ul");
		case 'x': ss.move(1); return parseIntegerLiteral(0, "ll");
		case 'y': ss.move(1); return parseIntegerLiteral(0, "ull");
		case 'n': ss.move(1); return parseIntegerLiteral("__int128", 0);
		case 'o': ss.move(1); return parseIntegerLiteral("unsigned __int128", 0);
		case 'f': ss.move(1); return parseFloatingLiteral<float>();
		case 'd': ss.move(1); return parseFloatingLiteral<double>();
		case 'e': ss.move(1); return parseFloatingLiteral<float80>();
		case '_':
			if (ss.check("_Z")) {
				Node* R = parseEncoding();
				if (R && ss.check('E'))
					return R;
			}
			return nullptr;
		case 'T':
			// Invalid mangled name per
			//   http://sourcerytools.com/pipermail/cxx-abi-dev/2011-August/002422.html
			return nullptr;
		default: {
			// might be named type
			Node* T = parseType();
			if (T == nullptr)
				return nullptr;
			auto N = parseNumber();
			if (!N.empty()) {
				if (!ss.check('E'))
					return nullptr;
				return arena.make<IntegerCastExpr>(T, N);
			}
			if (ss.check('E'))
				return T;
			return nullptr;
		}
	}
}

// <braced-expression> ::= <expression>
//                     ::= di <field source-name> <braced-expression>    # .name = expr
//                     ::= dx <index expression> <braced-expression>     # [expr] = expr
//                     ::= dX <range begin expression> <range end expression> <braced-expression>
Node* DemanglerI::parseBracedExpr() {
	if (ss.peekc() == 'd') {
		switch (ss.peekc(1)) {
			case 'i': {
				ss.move(2);
				Node* Field = parseSourceName(/*NameState=*/nullptr);
				if (Field == nullptr)
					return nullptr;
				Node* Init = parseBracedExpr();
				if (Init == nullptr)
					return nullptr;
				return arena.make<BracedExpr>(Field, Init, /*isArray=*/false);
			}
			case 'x': {
				ss.move(2);
				Node* Index = parseExpr();
				if (Index == nullptr)
					return nullptr;
				Node* Init = parseBracedExpr();
				if (Init == nullptr)
					return nullptr;
				return arena.make<BracedExpr>(Index, Init, /*isArray=*/true);
			}
			case 'X': {
				ss.move(2);
				Node* RangeBegin = parseExpr();
				if (RangeBegin == nullptr)
					return nullptr;
				Node* RangeEnd = parseExpr();
				if (RangeEnd == nullptr)
					return nullptr;
				Node* Init = parseBracedExpr();
				if (Init == nullptr)
					return nullptr;
				return arena.make<BracedRangeExpr>(RangeBegin, RangeEnd, Init);
			}
		}
	}
	return parseExpr();
}

Node* DemanglerI::parseExpr() {
	bool Global = ss.check("gs");

	switch (ss.peekc()) {
		case 'L': ss.move(1); return parseExprPrimary();
		case 'T': ss.move(1); return parseTemplateParam();
		case 'f': {
			// Disambiguate a fold expression from a <function-param>.
			if (ss.peekc(1) == 'p' || (ss.peekc(1) == 'L' && is_digit(ss.peekc(2))))
				return parseFunctionParam();
			// (not yet in the spec)
			// <fold-expr> ::= fL <binary-operator-name> <expression> <expression>
			//             ::= fR <binary-operator-name> <expression> <expression>
			//             ::= fl <binary-operator-name> <expression>
			//             ::= fr <binary-operator-name> <expression>
			char	FoldKind		= ss.getc();
			bool	IsLeftFold		= FoldKind == 'l' || FoldKind == 'L';
			bool	HasInitializer = is_upper(FoldKind);

			if (auto *op = parse_operator(ss)) {
				if (Node *Pack = parseExpr()) {
					Node *Init = HasInitializer ? parseExpr() : nullptr;
					if (IsLeftFold && Init)
						swap(Pack, Init);

					return arena.make<FoldExpr>(IsLeftFold, op->full_name, Pack, Init);
				}
			}
		}
		case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
			return parseUnresolvedName();

		default: break;
	}
	if (auto *op = parse_operator(ss)) {
		switch (op->chars) {
			case 'cl':
				if (Node* Callee = parseExpr()) {
					size_t ExprsBegin = Names.size();
					while (!ss.check('E')) {
						Node* E = parseExpr();
						if (E == nullptr)
							return E;
						Names.push_back(E);
					}
					return arena.make<CallExpr>(Callee, popTrailingNodeArray(ExprsBegin));
				}
				break;
			case 'cv':
				// cv <type> <expression>		# conversion with one argument
				// cv <type> _ <expression>* E	# conversion with a different number of arguments
				if (Node* Ty = (save(TryToParseTemplateArgs, false), parseType())) {
					if (ss.check('_')) {
						size_t ExprsBegin = Names.size();
						while (!ss.check('E')) {
							Node* E = parseExpr();
							if (E == nullptr)
								return E;
							Names.push_back(E);
						}
						NodeArray Exprs = popTrailingNodeArray(ExprsBegin);
						return arena.make<ConversionExpr>(Ty, Exprs);
					} else {
						if (Node* E = parseExpr())
							return arena.make<ConversionExpr>(Ty, makeNodeArray(&E, &E + 1));
					}
				}
				break;
			case 'da':
				if (Node* Ex = parseExpr())
					return arena.make<DeleteExpr>(Ex, Global, true);
				break;
			case 'dl':
				if (Node* E = parseExpr())
					return arena.make<DeleteExpr>(E, Global, /*is_array=*/false);
				break;
			case 'ds':
				if (Node* LHS = parseExpr()) {
					if (Node* RHS = parseExpr())
						return arena.make<MemberExpr>(LHS, ".*", RHS);
				}
				break;
			case 'dt':
				if (Node* LHS = parseExpr()) {
					if (Node* RHS = parseExpr())
						return arena.make<MemberExpr>(LHS, ".", RHS);
				}
				break;
			case 'ix':
				if (Node* Base = parseExpr()) {
					if (Node* Index = parseExpr())
						return arena.make<ArraySubscriptExpr>(Base, Index);
				}
				break;
			case 'il': {
				size_t InitsBegin = Names.size();
				while (!ss.check('E')) {
					Node* E = parseBracedExpr();
					if (E == nullptr)
						return nullptr;
					Names.push_back(E);
				}
				return arena.make<InitListExpr>(nullptr, popTrailingNodeArray(InitsBegin));
			}
			case 'mm':
				if (ss.check('_')) {
					if (Node* E = parseExpr())
						return arena.make<PrefixExpr>("--", E);
				} else if (Node* Ex = parseExpr())
					return arena.make<PostfixExpr>(Ex, "--");
				break;
			case 'na':
			case 'nw': {
			// [gs] nw <expression>* _ <type> E                     # new (expr-list) type
			// [gs] nw <expression>* _ <type> <initializer>         # new (expr-list) type (init)
			// [gs] na <expression>* _ <type> E                     # new[] (expr-list) type
			// [gs] na <expression>* _ <type> <initializer>         # new[] (expr-list) type (init)
			// <initializer> ::= pi <expression>* E                 # parenthesized initialization
				size_t Exprs = Names.size();
				while (!ss.check('_')) {
					Node* Ex = parseExpr();
					if (!Ex)
						return nullptr;
					Names.push_back(Ex);
				}
				NodeArray ExprList = popTrailingNodeArray(Exprs);
				if (Node *Ty = parseType()) {
					if (ss.check("pi")) {
						size_t InitsBegin = Names.size();
						while (!ss.check('E')) {
							Node* Init = parseExpr();
							if (!Init)
								return nullptr;
							Names.push_back(Init);
						}
						NodeArray Inits = popTrailingNodeArray(InitsBegin);
						return arena.make<NewExpr>(ExprList, Ty, Inits, Global, op->chars == 'na');
					} else if (ss.check('E')) {
						return arena.make<NewExpr>(ExprList, Ty, NodeArray(), Global, op->chars == 'na');
					}
				}
				break;
			}
			case 'nx':
				if (Node* Ex = parseExpr())
					return arena.make<EnclosingExpr>("noexcept (", Ex, ")");
				break;
			case 'pp':
				if (ss.check('_')) {
					if (Node* E = parseExpr())
						return arena.make<PrefixExpr>("++", E);
				} else if (Node* Ex = parseExpr())
					return arena.make<PostfixExpr>(Ex, "++");
				break;
			case 'pt':
				if (Node* L = parseExpr()) {
					if (Node* R = parseExpr())
						return arena.make<MemberExpr>(L, "->", R);
				}
				break;
			case 'qu':
				if (Node* Cond = parseExpr()) {
					if (Node* LHS = parseExpr()) {
						if (Node* RHS = parseExpr())
							return arena.make<ConditionalExpr>(Cond, LHS, RHS);
					}
				}
				break;
			case 'sp':
				if (Node* Child = parseExpr())
					return arena.make<ParameterPackExpansion>(Child);
				break;
			case 'sZ':
				if (ss.check('T')) {
					if (Node* R = parseTemplateParam())
						return arena.make<SizeofParamPackExpr>(R);
				} else if (ss.peekc() == 'f') {
					if (Node* FP = parseFunctionParam())
						return arena.make<EnclosingExpr>("sizeof... (", FP, ")");
				}
				break;
			case 'sP': {
				size_t ArgsBegin = Names.size();
				while (!ss.check('E')) {
					Node* Arg = parseTemplateArg();
					if (Arg == nullptr)
						return nullptr;
					Names.push_back(Arg);
				}
				if (auto* Pack = arena.make<NodeArrayNode>(popTrailingNodeArray(ArgsBegin)))
					return arena.make<EnclosingExpr>("sizeof... (", Pack, ")");
				break;
			}
			case 'tl':
				if (Node* Ty = parseType()) {
					size_t InitsBegin = Names.size();
					while (!ss.check('E')) {
						Node* E = parseBracedExpr();
						if (E == nullptr)
							return nullptr;
						Names.push_back(E);
					}
					return arena.make<InitListExpr>(Ty, popTrailingNodeArray(InitsBegin));
				}
				break;
			case 'tr':
				return arena.make<NameType>("throw");
			case 'tw':
				if (Node* Ex = parseExpr())
					return arena.make<ThrowExpr>(Ex);
				break;

			default:
				switch (op->type) {
					case OpUnary:
						if (Node* E = parseExpr())
							return arena.make<PrefixExpr>(op->short_name, E);
						break;
					case OpBinary:
						if (Node* LHS = parseExpr()) {
							if (Node* RHS = parseExpr())
								return arena.make<BinaryExpr>(LHS, op->short_name, RHS);
						}
						break;
					case OpCast:
						if (Node* T = parseType()) {
							if (Node* Ex = parseExpr())
								return arena.make<CastExpr>(op->full_name, T, Ex);
						}
						break;

					case OpFun:
						if (Node* Ty = parseExpr())
							return arena.make<EnclosingExpr>(op->full_name, Ty, ")");
						break;

					case OpTFun:
						if (Node* Ty = parseType())
							return arena.make<EnclosingExpr>(op->full_name, Ty, ")");
						break;

					case OpUnres:
						return parseUnresolvedName();
				}
				break;
		}
	}
	return nullptr;
}

// <call-offset> ::= h <nv-offset> _
//               ::= v <v-offset> _
//
// <nv-offset> ::= <offset number>
//               # non-virtual base override
//
// <v-offset>  ::= <offset number> _ <virtual offset number>
//               # virtual base override, with vcall offset
bool DemanglerI::parseCallOffset() {
	// Just scan through the call offset, we never add this information into the output.
	if (ss.check('h'))
		return parseNumber(true) && ss.check('_');
	if (ss.check('v'))
		return parseNumber(true) && ss.check('_') && parseNumber(true) && ss.check('_');
	return false;
}

// <special-name>	::= TV <type>    # virtual table
//					::= TT <type>    # VTT structure (construction vtable index)
//					::= TI <type>    # typeinfo structure
//					::= TS <type>    # typeinfo name (null-terminated byte string)
//					::= Tc <call-offset> <call-offset> <base encoding>
//							# base is the nominal target function of thunk
//							# first call-offset is 'this' adjustment
//							# second call-offset is result adjustment
//					::= T <call-offset> <base encoding>
//							# base is the nominal target function of thunk
//					::= GV <object name>				# Guard variable for one-time initialization
//														# No <type>
//					::= TW <object name>				# Thread-local wrapper
//					::= TH <object name>				# Thread-local initialization
//					::= GR <object name> _				# First temporary
//					::= GR <object name> <seq-id> _		# Subsequent temporaries
//		extension	::= TC <first type> <number> _ <second type> # construction vtable for second-in-first
//		extension	::= GR <object name> # reference temporary for object
Node* DemanglerI::parseSpecialName() {
	switch (ss.getc()) {
		case 'T':
			switch (ss.getc()) {
				// TV <type>    # virtual table
				case 'V':
					if (Node* Ty = parseType())
						return arena.make<SpecialName>("vtable for ", Ty);
					break;
				// TT <type>    # VTT structure (construction vtable index)
				case 'T':
					if (Node* Ty = parseType())
						return arena.make<SpecialName>("VTT for ", Ty);
					break;
				// TI <type>    # typeinfo structure
				case 'I':
					if (Node* Ty = parseType())
						return arena.make<SpecialName>("typeinfo for ", Ty);
					break;
				// TS <type>    # typeinfo name (null-terminated byte string)
				case 'S':
					if (Node* Ty = parseType())
						return arena.make<SpecialName>("typeinfo name for ", Ty);
					break;
				// Tc <call-offset> <call-offset> <base encoding>
				case 'c':
					if (parseCallOffset() && parseCallOffset()) {
						if (Node* Encoding = parseEncoding())
							return arena.make<SpecialName>("covariant return thunk to ", Encoding);
					}
					break;
				// extension ::= TC <first type> <number> _ <second type>	# construction vtable for second-in-first
				case 'C':
					if (Node* FirstType = parseType()) {
						if (parseNumber(true) && ss.check('_')) {
							if (Node* SecondType = parseType())
								return arena.make<CtorVtableSpecialName>(SecondType, FirstType);
						}
					}
					break;
				// TW <object name> # Thread-local wrapper
				case 'W':
					if (Node* Name = parseName())
						return arena.make<SpecialName>("thread-local wrapper routine for ", Name);
					break;
				// TH <object name> # Thread-local initialization
				case 'H':
					if (Node* Name = parseName())
						return arena.make<SpecialName>("thread-local initialization routine for ", Name);
					break;
				// T <call-offset> <base encoding>
				default:
					ss.move(-1);
					bool IsVirt = ss.peekc() == 'v';
					if (parseCallOffset()) {
						if (Node* BaseEncoding = parseEncoding())
							return arena.make<SpecialName>(IsVirt ? "virtual thunk to " : "non-virtual thunk to ", BaseEncoding);
					}
					break;
			}
		case 'G':
			switch (ss.getc()) {
				// GV <object name> # Guard variable for one-time initialization
				case 'V':
					if (Node* Name = parseName())
						return arena.make<SpecialName>("guard variable for ", Name);
					break;
				// GR <object name> # reference temporary for object
				// GR <object name> _             # First temporary
				// GR <object name> <seq-id> _    # Subsequent temporaries
				case 'R':
					if (Node* Name = parseName()) {
						size_t Count;
						if (parseSeqId(&Count) && ss.check('_'))
							return arena.make<SpecialName>("reference temporary for ", Name);
					}
					break;
			}
	}
	ss.move(-1);
	return nullptr;
}

// <encoding	>::= <function name> <bare-function-type>
//				::= <data name>
//				::= <special-name>
Node* DemanglerI::parseEncoding() {
	if (ss.peekc() == 'G' || ss.peekc() == 'T')
		return parseSpecialName();

	NameState NameInfo(this);
	Node*	 Name = parseName(&NameInfo);
	if (!Name || !resolveForwardTemplateRefs(NameInfo))
		return nullptr;

	if (is_any(ss.peekc(), 0, 'E', '.', '_'))
		return Name;

	Node* Attrs = nullptr;
	if (ss.check("Ua9enable_ifI")) {
		size_t BeforeArgs = Names.size();
		while (!ss.check('E')) {
			Node* Arg = parseTemplateArg();
			if (!Arg)
				return nullptr;
			Names.push_back(Arg);
		}
		Attrs = arena.make<EnableIfAttr>(popTrailingNodeArray(BeforeArgs));
		if (!Attrs)
			return nullptr;
	}

	Node* ReturnType = nullptr;
	if (!NameInfo.CtorDtorConversion && NameInfo.EndsWithTemplateArgs) {
		ReturnType = parseType();
		if (ReturnType == nullptr)
			return nullptr;
	}

	if (ss.check('v'))
		return arena.make<FunctionEncoding>(ReturnType, Name, NodeArray(), Attrs, NameInfo.CVQualifiers, NameInfo.ReferenceQualifier);

	size_t ParamsBegin = Names.size();
	do {
		Node* Ty = parseType();
		if (Ty == nullptr)
			return nullptr;
		Names.push_back(Ty);
	} while (!is_any(ss.peekc(), 0, 'E', '.', '_'));

	return arena.make<FunctionEncoding>(ReturnType, Name, popTrailingNodeArray(ParamsBegin), Attrs, NameInfo.CVQualifiers, NameInfo.ReferenceQualifier);
}

// <seq-id> ::= <0-9A-Z>+
bool DemanglerI::parseSeqId(size_t* Out) {
	char	c = ss.getc();
	if (!is_digit(c) && !is_upper(c))
		return false;

	size_t Id = 0;
	while (is_digit(c) || is_upper(c)) {
		Id = Id * 36 + from_digit(c);
		c = ss.getc();
	}

	ss.move(-1);
	*Out = Id;
	return true;
}

// <substitution>	::= S <seq-id> _
//					::= S_
// <substitution>	::= Sa # ::allocator
// <substitution>	::= Sb # ::basic_string
// <substitution>	::= Ss # ::basic_string < char,
//									::char_traits<char>,
//									::allocator<char> >
// <substitution>	::= Si # ::basic_istream<char,  char_traits<char> >
// <substitution>	::= So # ::basic_ostream<char,  char_traits<char> >
// <substitution>	::= Sd # ::basic_iostream<char, char_traits<char> >
Node* DemanglerI::parseSubstitution() {
	if (!ss.check('S'))
		return nullptr;

	char	c = ss.getc();
	if (is_lower(c)) {
		Node* SpecialSub;
		switch (c) {
			case 'a':	SpecialSub = arena.make<SpecialSubstitution>(SpecialSubKind::allocator);	break;
			case 'b':	SpecialSub = arena.make<SpecialSubstitution>(SpecialSubKind::basic_string);	break;
			case 's':	SpecialSub = arena.make<SpecialSubstitution>(SpecialSubKind::string);		break;
			case 'i':	SpecialSub = arena.make<SpecialSubstitution>(SpecialSubKind::istream);		break;
			case 'o':	SpecialSub = arena.make<SpecialSubstitution>(SpecialSubKind::ostream);		break;
			case 'd':	SpecialSub = arena.make<SpecialSubstitution>(SpecialSubKind::iostream);		break;
			default:	return nullptr;
		}
		// Itanium C++ ABI 5.1.2: If a name that would use a built-in <substitution> has ABI tags, the tags are appended to the substitution; the result is a substitutable component.
		Node* WithTags = parseAbiTags(SpecialSub);
		if (WithTags != SpecialSub) {
			Subs.push_back(WithTags);
			SpecialSub = WithTags;
		}
		return SpecialSub;
	}

	//                ::= S_
	if (ss.check('_'))
		return Subs.empty() ? nullptr : Subs[0];

	//                ::= S <seq-id> _
	size_t Index = 0;
	if (parseSeqId(&Index)) {
		++Index;
		if (ss.check('_') && Index < Subs.size())
			return Subs[Index];
	}
	return nullptr;
}

// <template-param> ::= T_    # first template parameter
//                  ::= T <parameter-2 non-negative number> _
Node* DemanglerI::parseTemplateParam() {
	size_t Index = 0;
	if (!ss.check('_')) {
		if (!parsePositiveInteger(&Index) || !ss.check('_'))
			return nullptr;
		++Index;
	}

	// Itanium ABI 5.1.8: In a generic lambda, uses of auto in the parameter list are mangled as the corresponding artificial template type parameter.
	if (ParsingLambdaParams)
		return arena.make<NameType>("auto");

	// If we're in a context where this <template-param> refers to a <template-arg> further ahead in the mangled name (currently just conversion operator types), then we should only ss.peekc it up in the right context.
	if (PermitForwardTemplateReferences) {
		if (Node* ForwardRef = arena.make<ForwardTemplateReference>(Index)) {
			ISO_ASSERT(ForwardRef->kind == Node::kForwardTemplateReference);
			ForwardTemplateRefs.push_back(static_cast<ForwardTemplateReference*>(ForwardRef));
			return ForwardRef;
		}
		return nullptr;
	}

	return Index < TemplateParams.size() ? TemplateParams[Index] : nullptr;
}

// <template-arg>	::= <type>					# type or template
//					::= X <expression> E		# expression
//					::= <expr-primary>			# simple expressions
//					::= J <template-arg>* E		# argument pack
//					::= LZ <encoding> E			# extension
Node* DemanglerI::parseTemplateArg() {
	switch (ss.getc()) {
		case 'X':
			if (Node* Arg = parseExpr())
				if (ss.check('E'))
					return Arg;
			return nullptr;
		case 'J': {
			size_t ArgsBegin = Names.size();
			while (!ss.check('E')) {
				Node* Arg = parseTemplateArg();
				if (!Arg)
					return nullptr;
				Names.push_back(Arg);
			}
			NodeArray Args = popTrailingNodeArray(ArgsBegin);
			return arena.make<TemplateArgumentPack>(Args);
		}
		case 'L': {
			//                ::= LZ <encoding> E           # extension
			if (ss.getc() == 'Z') {
				if (Node* Arg = parseEncoding()) {
					if (ss.check('E'))
						return Arg;
				}
				return nullptr;
			}
			//                ::= <expr-primary>            # simple expressions
			ss.move(-1);
			return parseExprPrimary();
		}
		default:
			ss.move(-1);
			return parseType();
	}
}

// <template-args> ::= I <template-arg>* E
//     extension, the abi says <template-arg>+
Node* DemanglerI::parseTemplateArgs(bool TagTemplates) {
	if (!ss.check('I'))
		return nullptr;

	// <template-params> refer to the innermost <template-args>. Clear out any outer args that we may have inserted into TemplateParams.
	if (TagTemplates)
		TemplateParams.clear();

	size_t ArgsBegin = Names.size();
	while (!ss.check('E')) {
		if (TagTemplates) {
			auto  OldParams = move(TemplateParams);
			Node* Arg		= parseTemplateArg();
			TemplateParams  = move(OldParams);
			if (!Arg)
				return nullptr;
			Names.push_back(Arg);
			Node* TableEntry = Arg;
			if (Arg->kind == Node::kTemplateArgumentPack) {
				TableEntry = arena.make<ParameterPack>(static_cast<TemplateArgumentPack*>(TableEntry)->getElements());
				if (!TableEntry)
					return nullptr;
			}
			TemplateParams.push_back(TableEntry);
		} else {
			Node* Arg = parseTemplateArg();
			if (!Arg)
				return nullptr;
			Names.push_back(Arg);
		}
	}
	return arena.make<TemplateArgs>(popTrailingNodeArray(ArgsBegin));
}

// <mangled-name>	::= _Z <encoding>
//					::= <type>
// extension		::= ___Z <encoding> _block_invoke
// extension		::= ___Z <encoding> _block_invoke<decimal-digit>+
// extension		::= ___Z <encoding> _block_invoke_<decimal-digit>+
Node* DemanglerI::parse() {
	if (ss.check("_Z")) {
		if (Node* Encoding = parseEncoding()) {
			if (ss.peekc() == '.')
				return arena.make<DotSuffix>(Encoding, ss.remainder());
			if (ss.remaining() == 0)
				return Encoding;
		}

	} else if (ss.check("___Z")) {
		Node* Encoding = parseEncoding();
		if (Encoding && ss.check("_block_invoke")) {
			if (parseNumber(ss.check('_')) && ss.peekc() == '.' && ss.remaining() == 0)
				return arena.make<SpecialName>("invocation function for block in ", Encoding);
		}

	} else {
		Node* Ty = parseType();
		if (ss.remaining() == 0)
			return Ty;
	}
	return nullptr;
}

string Demangle(const char* MangledName) {
	DemanglerI	Parser(MangledName, MangledName + strlen(MangledName));
	string_builder S;

	if (Node* AST = Parser.parse()) {
		ISO_ASSERT(Parser.ForwardTemplateRefs.empty());
		AST->print(S);
		return S.term();
	}
	return "";
}

}