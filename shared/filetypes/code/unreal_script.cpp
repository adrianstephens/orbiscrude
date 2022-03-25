#include "container/unreal.h"
#include "disassembler.h"

using namespace unreal;

//-----------------------------------------------------------------------------
//	script types
//-----------------------------------------------------------------------------

struct FScriptArray {
	void	*p;
};

struct FScriptBitArray : FScriptArray {
	int32	NumBits;
	int32	MaxBits;
};

struct FScriptSparseArray {
	struct Layout {
		// ElementOffset is at zero offset from the TSparseArrayElementOrFreeListLink - not stored here
		int32	Alignment;
		int32	Size;
	};
	FScriptArray	Data;
	FScriptBitArray	AllocationFlags;
	int32			FirstFreeIndex;
	int32			NumFreeIndices;
};
struct FScriptSet {
	struct Layout {
		// int32 ElementOffset = 0; // always at zero offset from the TSetElement - not stored here
		int32 HashNextIdOffset;
		int32 HashIndexOffset;
		int32 Size;
		FScriptSparseArray::Layout SparseArrayLayout;
	};
	FScriptSparseArray Elements;
	//typename Allocator::HashAllocator::template ForElementType<FSetElementId> Hash;
	int32    HashSize;
};

struct FScriptMap {
	struct Layout {
		// int32 KeyOffset; // is always at zero offset from the TPair - not stored here
		int32 ValueOffset;
		FScriptSet::Layout SetLayout;
	};

	FScriptSet Pairs;
};

//-----------------------------------------------------------------------------
// unreal bytecode
//-----------------------------------------------------------------------------

enum { MAX_STRING_CONST_SIZE = 1024, MAX_SIMPLE_RETURN_VALUE_SIZE = 64 };

typedef uint16 VariableSizeType;
typedef uint32 CodeSkipSizeType;


// Evaluatable expression item types
enum EExprToken : uint8 {
	// Variable references.
	EX_LocalVariable		= 0x00,	// A local variable.
	EX_InstanceVariable		= 0x01,	// An object variable.
	EX_DefaultVariable		= 0x02, // Default variable for a class context.
	//						= 0x03,
	EX_Return				= 0x04,	// Return from function.
	//						= 0x05,
	EX_Jump					= 0x06,	// Goto a local address in code.
	EX_JumpIfNot			= 0x07,	// Goto if not expression.
	//						= 0x08,
	EX_Assert				= 0x09,	// Assertion.
	//						= 0x0A,
	EX_Nothing				= 0x0B,	// No operation.
	//						= 0x0C,
	//						= 0x0D,
	//						= 0x0E,
	EX_Let					= 0x0F,	// Assign an arbitrary size value to a variable.
	//						= 0x10,
	//						= 0x11,
	EX_ClassContext			= 0x12,	// Class default object context.
	EX_MetaCast             = 0x13, // Metaclass cast.
	EX_LetBool				= 0x14, // Let boolean variable.
	EX_EndParmValue			= 0x15,	// end of default value for optional function parameter
	EX_EndFunctionParms		= 0x16,	// End of function call parameters.
	EX_Self					= 0x17,	// Self object.
	EX_Skip					= 0x18,	// Skippable expression.
	EX_Context				= 0x19,	// Call a function through an object context.
	EX_Context_FailSilent	= 0x1A, // Call a function through an object context (can fail silently if the context is NULL; only generated for functions that don't have output or return values).
	EX_VirtualFunction		= 0x1B,	// A function call with parameters.
	EX_FinalFunction		= 0x1C,	// A prebound function call with parameters.
	EX_IntConst				= 0x1D,	// Int constant.
	EX_FloatConst			= 0x1E,	// Floating point constant.
	EX_StringConst			= 0x1F,	// String constant.
	EX_ObjectConst		    = 0x20,	// An object constant.
	EX_NameConst			= 0x21,	// A name constant.
	EX_RotationConst		= 0x22,	// A rotation constant.
	EX_VectorConst			= 0x23,	// A vector constant.
	EX_ByteConst			= 0x24,	// A byte constant.
	EX_IntZero				= 0x25,	// Zero.
	EX_IntOne				= 0x26,	// One.
	EX_True					= 0x27,	// Bool True.
	EX_False				= 0x28,	// Bool False.
	EX_TextConst			= 0x29, // FText constant
	EX_NoObject				= 0x2A,	// NoObject.
	EX_TransformConst		= 0x2B, // A transform constant
	EX_IntConstByte			= 0x2C,	// Int constant that requires 1 byte.
	EX_NoInterface			= 0x2D, // A null interface (similar to EX_NoObject, but for interfaces)
	EX_DynamicCast			= 0x2E,	// Safe dynamic class casting.
	EX_StructConst			= 0x2F, // An arbitrary UStruct constant
	EX_EndStructConst		= 0x30, // End of UStruct constant
	EX_SetArray				= 0x31, // Set the value of arbitrary array
	EX_EndArray				= 0x32,
	//						= 0x33,
	EX_UnicodeStringConst   = 0x34, // Unicode string constant.
	EX_Int64Const			= 0x35,	// 64-bit integer constant.
	EX_UInt64Const			= 0x36,	// 64-bit unsigned integer constant.
	//						= 0x37,
	EX_PrimitiveCast		= 0x38,	// A casting operator for primitives which reads the type as the subsequent byte
	EX_SetSet				= 0x39,
	EX_EndSet				= 0x3A,
	EX_SetMap				= 0x3B,
	EX_EndMap				= 0x3C,
	EX_SetConst				= 0x3D,
	EX_EndSetConst			= 0x3E,
	EX_MapConst				= 0x3F,
	EX_EndMapConst			= 0x40,
	//						= 0x41,
	EX_StructMemberContext	= 0x42, // Context expression to address a property within a struct
	EX_LetMulticastDelegate	= 0x43, // Assignment to a multi-cast delegate
	EX_LetDelegate			= 0x44, // Assignment to a delegate
	EX_LocalVirtualFunction	= 0x45, // Special instructions to quickly call a virtual function that we know is going to run only locally
	EX_LocalFinalFunction	= 0x46, // Special instructions to quickly call a final function that we know is going to run only locally
	//						= 0x47, // CST_ObjectToBool
	EX_LocalOutVariable		= 0x48, // local out (pass by reference) function parameter
	//						= 0x49, // CST_InterfaceToBool
	EX_DeprecatedOp4A		= 0x4A,
	EX_InstanceDelegate		= 0x4B,	// const reference to a delegate or normal function object
	EX_PushExecutionFlow	= 0x4C, // push an address on to the execution flow stack for future execution when a EX_PopExecutionFlow is executed.   Execution continues on normally and doesn't change to the pushed address.
	EX_PopExecutionFlow		= 0x4D, // continue execution at the last address previously pushed onto the execution flow stack.
	EX_ComputedJump			= 0x4E,	// Goto a local address in code, specified by an integer value.
	EX_PopExecutionFlowIfNot = 0x4F, // continue execution at the last address previously pushed onto the execution flow stack, if the condition is not true.
	EX_Breakpoint			= 0x50, // Breakpoint.  Only observed in the editor, otherwise it behaves like EX_Nothing.
	EX_InterfaceContext		= 0x51,	// Call a function through a native interface variable
	EX_ObjToInterfaceCast   = 0x52,	// Converting an object reference to native interface variable
	EX_EndOfScript			= 0x53, // Last byte in script code
	EX_CrossInterfaceCast	= 0x54, // Converting an interface variable reference to native interface variable
	EX_InterfaceToObjCast   = 0x55, // Converting an interface variable reference to an object
	//						= 0x56,
	//						= 0x57,
	//						= 0x58,
	//						= 0x59,
	EX_WireTracepoint		= 0x5A, // Trace point.  Only observed in the editor, otherwise it behaves like EX_Nothing.
	EX_SkipOffsetConst		= 0x5B, // A CodeSizeSkipOffset constant
	EX_AddMulticastDelegate = 0x5C, // Adds a delegate to a multicast delegate's targets
	EX_ClearMulticastDelegate = 0x5D, // Clears all delegates in a multicast target
	EX_Tracepoint			= 0x5E, // Trace point.  Only observed in the editor, otherwise it behaves like EX_Nothing.
	EX_LetObj				= 0x5F,	// assign to any object ref pointer
	EX_LetWeakObjPtr		= 0x60, // assign to a weak object pointer
	EX_BindDelegate			= 0x61, // bind object and name to delegate
	EX_RemoveMulticastDelegate = 0x62, // Remove a delegate from a multicast delegate's targets
	EX_CallMulticastDelegate = 0x63, // Call multicast delegate
	EX_LetValueOnPersistentFrame = 0x64,
	EX_ArrayConst			= 0x65,
	EX_EndArrayConst		= 0x66,
	EX_SoftObjectConst		= 0x67,
	EX_CallMath				= 0x68, // static pure function from on local call space
	EX_SwitchValue			= 0x69,
	EX_InstrumentationEvent	= 0x6A, // Instrumentation event
	EX_ArrayGetByRef		= 0x6B,
	EX_ClassSparseDataVariable = 0x6C, // Sparse data variable
	EX_FieldPathConst		= 0x6D,
};

enum ECastToken {
	CST_ObjectToInterface	= 0x46,
	CST_ObjectToBool		= 0x47,
	CST_InterfaceToBool		= 0x49,
	CST_Max					= 0xFF,
};

// Kinds of text literals
enum class EBlueprintTextLiteralType : uint8 {
	Empty,				// Text is an empty string. The bytecode contains no strings, and you should use FText::GetEmpty() to initialize the FText instance
	LocalizedText,		// Text is localized. The bytecode will contain three strings - source, key, and namespace - and should be loaded via FInternationalization
	InvariantText,		// Text is culture invariant. The bytecode will contain one string, and you should use FText::AsCultureInvariant to initialize the FText instance
	LiteralString,		// Text is a literal FString. The bytecode will contain one string, and you should use FText::FromString to initialize the FText instance
	StringTableEntry,	// Text is from a string table. The bytecode will contain an object pointer (not used) and two strings - the table ID, and key - and should be found via FText::FromStringTable
};

struct FFrame;

// Information about a blueprint instrumentation signal
struct FScriptInstrumentationSignal {
	enum Type {
		Class = 0,
		ClassScope,
		Instance,
		Event,
		InlineEvent,
		ResumeEvent,
		PureNodeEntry,
		NodeDebugSite,
		NodeEntry,
		NodeExit,
		PushState,
		RestoreState,
		ResetState,
		SuspendState,
		PopState,
		TunnelEndOfThread,
		Stop
	};
	Type EventType;	// The event signal type
	const UObject*			ContextObject;	// The context object the event is from
	const UFunction*		Function;		// The function that emitted this event
	const FName				EventName;		// The event override name
	const FFrame*			StackFramePtr;	// The stack frame for the
	const int32				LatentLinkId;

	FScriptInstrumentationSignal(Type InEventType, const UObject* InContextObject, const struct FFrame& InStackFrame, const FName EventNameIn = {});

	FScriptInstrumentationSignal(Type InEventType, const UObject* InContextObject, UFunction* InFunction, const int32 LinkId = -1)
		: EventType(InEventType)
		, ContextObject(InContextObject)
		, Function(InFunction)
		, StackFramePtr(nullptr)
		, LatentLinkId(LinkId)
	{}

	void			SetType(Type InType)			{ EventType = InType;	}

	Type			GetType()				const	{ return EventType; }
	bool			IsContextObjectValid()	const	{ return ContextObject != nullptr; }
	const UObject*	GetContextObject()		const	{ return ContextObject; }
	bool IsStackFrameValid()				const	{ return StackFramePtr != nullptr; }
	const FFrame&	GetStackFrame()			const	{ return *StackFramePtr; }
	const UClass*	GetClass()				const;
	const UClass*	GetFunctionClassScope() const;
	FName			GetFunctionName()		const;
	int32			GetScriptCodeOffset()	const;
	int32			GetLatentLinkId()		const	{ return LatentLinkId; }
};

struct FScriptName {
	uint32	ComparisonIndex;
	uint32	DisplayIndex;
	uint32	Number;
};

class DisassemblerUnreal : public Disassembler {

	struct State : Disassembler::State {
		struct Line {
			uint64	offset;
			uint32	indent;
			string	dis;
			Line(uint64 offset, uint32 indent, const char *dis) : offset(offset), indent(indent), dis(dis) {}
		};
		dynamic_array<Line>		lines;

		//FLinkerTables	*linker	= nullptr;
		int				indent	= 0;
		memory_reader	reader;

		uint64			op_start;

		cstring			ReadName() {
#if 0
			auto	name = reader.get<FScriptName>();
			return linker ? linker->lookup(FName(name.ComparisonIndex, name.Number)) : "????";
#else
			auto	name = reader.get<FName>();
			//return linker ? linker->lookup(name) : "????";
			return "name";
#endif
		}
		const char*		GetName(const UObject* obj) {
			return (*obj)["NamePrivate"];
		}
		const char*		GetNameSafe(const UObject* obj) {
			if (obj)
				return GetName(obj);
			return "none";
		}
		const char*		GetName(const FField* field) {
			return (const char*)field->NamePrivate;
		}
		const char*		GetNameSafe(const FField* field) {
			if (field)
				return (const char*)field->NamePrivate;
			return "none";
		}

		const char*	GetFullName(const UObject* obj) {
			return GetName(obj);
		}

		FString		ReadString8()	{ string	s; read(reader, s); return s.begin(); }
		FString		ReadString16()	{ string16	s; read(reader, s); return s.begin(); }
		FString		ReadString() {
			switch (reader.get<EExprToken>()) {
				case EX_StringConst:
					return ReadString8();
				case EX_UnicodeStringConst:
					return ReadString16();
				default:
					ISO_ASSERT(0);
					break;
			}
			return FString();
		}

		auto		Read(FField* p) {
			auto	path	= reader.get<TArray<FName>>();
			auto	xtra	= reader.get<uint32>();
			return p;
		}

		auto		Read(UObject* p) {
			auto	i = reader.get<FPackageIndex>();
			return p;
		}

		//template<typename T> T* ReadPointer() { return (T*)reader.get<uint64>(); }
		template<typename T> const T* ReadPointer() { 
			static	T	dummy;
			return (T*)Read(&dummy);
		}

		EExprToken	SerializeExpr();
		void		ProcessCommon(EExprToken Opcode);

		void		AddIndent()		{ ++indent; }
		void		DropIndent()	{ --indent; }

		void		output(const char *fmt, ...) {
			va_list valist;
			va_start(valist, fmt);
			buffer_accum<256>	ba;
			ba.vformat(fmt, valist) << '\n';
			lines.emplace_back(op_start, indent, (const char*)ba);
		}

		int		Count()												{ return lines.size32(); }
		void	GetLine(string_accum &a, int i, int, SymbolFinder)	{
			if (i < lines.size32())
				a << formatted(lines[i].offset, FORMAT::HEX | FORMAT::ZEROES, 4) << repeat("    ", lines[i].indent) << lines[i].dis;
		}
		uint64	GetAddress(int i)									{ return lines[min(i, lines.size32() - 1)].offset; }

		State(const memory_block &block) : reader(block) {}
	};

public:
	const char*	GetDescription() override { return "unreal bytecode"; }
	virtual Disassembler::State*		Disassemble(const memory_block &block, uint64 addr, SymbolFinder sym_finder);
} dis_unreal;


Disassembler::State *DisassemblerUnreal::Disassemble(const memory_block &block, uint64 addr, SymbolFinder sym_finder) {
	State	*state = new State(block);

	while (!state->reader.eof()) {
		//output("Label_0x%X:");
		state->AddIndent();
		state->SerializeExpr();
		state->DropIndent();
	}
	return state;
}

EExprToken DisassemblerUnreal::State::SerializeExpr() {
	AddIndent();
	EExprToken Opcode = reader.get<EExprToken>();
	ProcessCommon(Opcode);
	DropIndent();
	return Opcode;
}

void DisassemblerUnreal::State::ProcessCommon(EExprToken Opcode) {
	op_start = reader.tell();

	switch (Opcode) {
		case EX_PrimitiveCast: {
			uint8 ConversionType = reader.get<uint8>();
			output("$%X: PrimitiveCast of type %d", (int32)Opcode, ConversionType);
			AddIndent();

			output("Argument:");
			//ProcessCastByte(ConversionType);
			SerializeExpr();

			//@TODO:
			// output("Expression:");
			// SerializeExpr(  );
			DropIndent();

			break;
		}
		case EX_SetSet: {
			output("$%X: set set", (int32)Opcode);
			SerializeExpr();
			reader.get<int>();
			while (SerializeExpr() != EX_EndSet) {}
			break;
		}
		case EX_EndSet: {
			output("$%X: EX_EndSet", (int32)Opcode);
			break;
		}
		case EX_SetConst: {
			auto	InnerProp	= ReadPointer<FProperty>();
			int32	Num			= reader.get<int>();
			output("$%X: set set const - elements number: %d, inner property: %s", (int32)Opcode, Num, GetNameSafe(InnerProp));
			while (SerializeExpr() != EX_EndSetConst) {}
			break;
		}
		case EX_EndSetConst: {
			output("$%X: EX_EndSetConst", (int32)Opcode);
			break;
		}
		case EX_SetMap: {
			output("$%X: set map", (int32)Opcode);
			SerializeExpr();
			reader.get<int>();
			while (SerializeExpr() != EX_EndMap) {}
			break;
		}
		case EX_EndMap: {
			output("$%X: EX_EndMap", (int32)Opcode);
			break;
		}
		case EX_MapConst: {
			auto KeyProp = ReadPointer<FProperty>();
			auto ValProp = ReadPointer<FProperty>();
			int32	   Num	   = reader.get<int>();
			output("$%X: set map const - elements number: %d, key property: %s, val property: %s", (int32)Opcode, Num, GetNameSafe(KeyProp), GetNameSafe(ValProp));
			while (SerializeExpr() != EX_EndMapConst) {
				// Map contents
			}
			break;
		}
		case EX_EndMapConst: {
			output("$%X: EX_EndMapConst", (int32)Opcode);
			break;
		}
		case EX_ObjToInterfaceCast: {
			// A conversion from an object variable to a native interface variable
			auto InterfaceClass = ReadPointer<UClass>();
			output("$%X: ObjToInterfaceCast to %s", (int32)Opcode, GetName(InterfaceClass));
			SerializeExpr();
			break;
		}
		case EX_CrossInterfaceCast: {
			// A conversion from one interface variable to a different interface variable
			auto InterfaceClass = ReadPointer<UClass>();
			output("$%X: InterfaceToInterfaceCast to %s", (int32)Opcode, GetName(InterfaceClass));
			SerializeExpr();
			break;
		}
		case EX_InterfaceToObjCast: {
			// A conversion from an interface variable to a object variable
			auto ObjectClass = ReadPointer<UClass>();
			output("$%X: InterfaceToObjCast to %s", (int32)Opcode, GetName(ObjectClass));
			SerializeExpr();
			break;
		}
		case EX_Let: {
			output("$%X: Let (Variable = Expression)", (int32)Opcode);
			AddIndent();
			ReadPointer<FProperty>();
			output("Variable:");	SerializeExpr();
			output("Expression:");	SerializeExpr();
			DropIndent();
			break;
		}
		case EX_LetObj:
		case EX_LetWeakObjPtr: {
			if (Opcode == EX_LetObj)
				output("$%X: Let Obj (Variable = Expression)", (int32)Opcode);
			else
				output("$%X: Let WeakObjPtr (Variable = Expression)", (int32)Opcode);
			AddIndent();
			output("Variable:");	SerializeExpr();
			output("Expression:");	SerializeExpr();
			DropIndent();
			break;
		}
		case EX_LetBool: {
			output("$%X: LetBool (Variable = Expression)", (int32)Opcode);
			AddIndent();
			output("Variable:");	SerializeExpr();
			output("Expression:");	SerializeExpr();
			DropIndent();
			break;
		}
		case EX_LetValueOnPersistentFrame: {
			output("$%X: LetValueOnPersistentFrame", (int32)Opcode);
			AddIndent();
			auto Prop = ReadPointer<FProperty>();
			output("Destination variable: %s, offset: %d", GetNameSafe(Prop), Prop ? Prop->GetOffset_ForDebug() : 0);
			output("Expression:");
			SerializeExpr();
			DropIndent();
			break;
		}
		case EX_StructMemberContext: {
			output("$%X: Struct member context ", (int32)Opcode);
			AddIndent();
			auto Prop = ReadPointer<FProperty>();
			output("Expression within struct %s, offset %d", GetName(Prop), Prop->GetOffset_ForDebug());  // although that isn't a UFunction, we are not going to indirect the props of a struct, so this should be fine
			output("Expression to struct:");	SerializeExpr();
			DropIndent();
			break;
		}
		case EX_LetDelegate: {
			output("$%X: LetDelegate (Variable = Expression)", (int32)Opcode);
			AddIndent();
			output("Variable:");	SerializeExpr();
			output("Expression:");	SerializeExpr();
			DropIndent();
			break;
		}
		case EX_LocalVirtualFunction: {
			auto FunctionName = ReadName();
			output("$%X: Local Virtual Script Function named %s", (int32)Opcode, FunctionName);
			while (SerializeExpr() != EX_EndFunctionParms) {}
			break;
		}
		case EX_LocalFinalFunction: {
			auto StackNode = ReadPointer<UStruct>();
			output("$%X: Local Final Script Function (stack node %s::%s)", (int32)Opcode, StackNode ? GetName(StackNode->GetOuter()) : "(null)", StackNode ? GetName(StackNode) : "(null)");
			while (SerializeExpr() != EX_EndFunctionParms) {}
			break;
		}
		case EX_LetMulticastDelegate: {
			output("$%X: LetMulticastDelegate (Variable = Expression)", (int32)Opcode);
			AddIndent();
			output("Variable:");	SerializeExpr();
			output("Expression:");	SerializeExpr();
			DropIndent();
			break;
		}
		case EX_ComputedJump: {
			output("$%X: Computed Jump, offset specified by expression:", (int32)Opcode);
			AddIndent();
			SerializeExpr();
			DropIndent();
			break;
		}
		case EX_Jump: {
			CodeSkipSizeType SkipCount = reader.get<CodeSkipSizeType>();
			output("$%X: Jump to offset 0x%X", (int32)Opcode, SkipCount);
			break;
		}
		case EX_LocalVariable: {
			auto PropertyPtr = ReadPointer<FProperty>();
			output("$%X: Local variable named %s", (int32)Opcode, PropertyPtr ? GetName(PropertyPtr) : "(null)");
			break;
		}
		case EX_DefaultVariable: {
			auto PropertyPtr = ReadPointer<FProperty>();
			output("$%X: Default variable named %s", (int32)Opcode, PropertyPtr ? GetName(PropertyPtr) : "(null)");
			break;
		}
		case EX_InstanceVariable: {
			auto PropertyPtr = ReadPointer<FProperty>();
			output("$%X: Instance variable named %s", (int32)Opcode, PropertyPtr ? GetName(PropertyPtr) : "(null)");
			break;
		}
		case EX_LocalOutVariable: {
			auto PropertyPtr = ReadPointer<FProperty>();
			output("$%X: Local out variable named %s", (int32)Opcode, PropertyPtr ? GetName(PropertyPtr) : "(null)");
			break;
		}
		case EX_ClassSparseDataVariable: {
			auto PropertyPtr = ReadPointer<FProperty>();
			output("$%X: Class sparse data variable named %s", (int32)Opcode, PropertyPtr ? GetName(PropertyPtr) : "(null)");
			break;
		}
		case EX_InterfaceContext:
			output("$%X: EX_InterfaceContext:", (int32)Opcode);
			SerializeExpr();
			break;

		case EX_DeprecatedOp4A:
			output("$%X: This opcode has been removed and does nothing.", (int32)Opcode);
			break;

		case EX_Nothing:
			output("$%X: EX_Nothing", (int32)Opcode);
			break;

		case EX_EndOfScript:
			output("$%X: EX_EndOfScript", (int32)Opcode);
			break;

		case EX_EndFunctionParms:
			output("$%X: EX_EndFunctionParms", (int32)Opcode);
			break;

		case EX_EndStructConst:
			output("$%X: EX_EndStructConst", (int32)Opcode);
			break;

		case EX_EndArray:
			output("$%X: EX_EndArray", (int32)Opcode);
			break;

		case EX_EndArrayConst:
			output("$%X: EX_EndArrayConst", (int32)Opcode);
			break;

		case EX_IntZero:
			output("$%X: EX_IntZero", (int32)Opcode);
			break;

		case EX_IntOne:
			output("$%X: EX_IntOne", (int32)Opcode);
			break;

		case EX_True:
			output("$%X: EX_True", (int32)Opcode);
			break;

		case EX_False:
			output("$%X: EX_False", (int32)Opcode);
			break;

		case EX_NoObject:
			output("$%X: EX_NoObject", (int32)Opcode);
			break;

		case EX_NoInterface:
			output("$%X: EX_NoObject", (int32)Opcode);
			break;

		case EX_Self:
			output("$%X: EX_Self", (int32)Opcode);
			break;

		case EX_EndParmValue:
			output("$%X: EX_EndParmValue", (int32)Opcode);
			break;

		case EX_Return:
			output("$%X: Return expression", (int32)Opcode);
			SerializeExpr();	 // Return expression.
			break;

		case EX_CallMath: {
			auto StackNode = ReadPointer<UStruct>();
			output("$%X: Call Math (stack node %s::%s)", (int32)Opcode, GetNameSafe(StackNode ? StackNode->GetOuter() : nullptr), GetNameSafe(StackNode));
			while (SerializeExpr() != EX_EndFunctionParms) {}
			break;
		}
		case EX_FinalFunction: {
			auto StackNode = ReadPointer<UStruct>();
			output("$%X: Final Function (stack node %s::%s)", (int32)Opcode, StackNode ? GetName(StackNode->GetOuter()) : "(null)", StackNode ? GetName(StackNode) : "(null)");
			while (SerializeExpr() != EX_EndFunctionParms) {}
			break;
		}
		case EX_CallMulticastDelegate: {
			auto StackNode = ReadPointer<UStruct>();
			output("$%X: CallMulticastDelegate (signature %s::%s) delegate:", (int32)Opcode, StackNode ? GetName(StackNode->GetOuter()) : "(null)", StackNode ? GetName(StackNode) : "(null)");
			SerializeExpr();
			output("Params:");
			while (SerializeExpr() != EX_EndFunctionParms) {}
			break;
		}
		case EX_VirtualFunction: {
			auto FunctionName = ReadName();
			output("$%X: Virtual Function named %s", (int32)Opcode, FunctionName);
			while (SerializeExpr() != EX_EndFunctionParms) {}
			break;
		}
		case EX_ClassContext:
		case EX_Context:
		case EX_Context_FailSilent: {
			output("$%X: %s", (int32)Opcode, Opcode == EX_ClassContext ? "Class Context" : "Context");
			AddIndent();
			output("ObjectExpression:");	SerializeExpr();

			if (Opcode == EX_Context_FailSilent)
				output(" Can fail silently on access none ");

			// Code offset for NULL expressions.
			CodeSkipSizeType SkipCount = reader.get<CodeSkipSizeType>();
			output("Skip Bytes: 0x%X", SkipCount);

			// Property corresponding to the r-value data, in case the l-value needs to be mem-zero'd
			auto Field = ReadPointer<FField>();
			output("R-Value Property: %s", Field ? GetName(Field) : "(null)");
			output("ContextExpression:");	SerializeExpr();
			DropIndent();
			break;
		}
		case EX_IntConst: {
			int32 ConstValue = reader.get<int>();
			output("$%X: literal int32 %d", (int32)Opcode, ConstValue);
			break;
		}
		case EX_SkipOffsetConst: {
			CodeSkipSizeType ConstValue = reader.get<CodeSkipSizeType>();
			output("$%X: literal CodeSkipSizeType 0x%X", (int32)Opcode, ConstValue);
			break;
		}
		case EX_FloatConst: {
			float ConstValue = reader.get<float>();
			output("$%X: literal float %f", (int32)Opcode, ConstValue);
			break;
		}
		case EX_StringConst: {
			auto ConstValue = ReadString8();
			output("$%X: literal ansi string \"%s\"", (int32)Opcode, (const char*)ConstValue);
			break;
		}
		case EX_UnicodeStringConst: {
			auto ConstValue = ReadString16();
			output("$%X: literal unicode string \"%s\"", (int32)Opcode, (const char*)ConstValue);
			break;
		}
		case EX_TextConst: {
			// What kind of text are we dealing with?
			switch (reader.get<EBlueprintTextLiteralType>()) {
				case EBlueprintTextLiteralType::Empty:
					output("$%X: literal text - empty", (int32)Opcode);
					break;

				case EBlueprintTextLiteralType::LocalizedText: {
					auto SourceString	= ReadString();
					auto KeyString		= ReadString();
					auto Namespace		= ReadString();
					output("$%X: literal text - localized text { namespace: \"%s\", key: \"%s\", source: \"%s\" }", (int32)Opcode, (const char*)Namespace, (const char*)KeyString, (const char*)SourceString);
					break;
				}
				case EBlueprintTextLiteralType::InvariantText: {
					auto SourceString	= ReadString();
					output("$%X: literal text - invariant text: \"%s\"", (int32)Opcode, (const char*)SourceString);
					break;
				}
				case EBlueprintTextLiteralType::LiteralString: {
					auto SourceString	= ReadString();
					output("$%X: literal text - literal string: \"%s\"", (int32)Opcode, (const char*)SourceString);
					break;
				}
				case EBlueprintTextLiteralType::StringTableEntry: {
					ReadPointer<UObject>();	// String Table asset (if any)
					auto TableIdString	= ReadString();
					auto KeyString		= ReadString();
					output("$%X: literal text - string table entry { tableid: \"%s\", key: \"%s\" }", (int32)Opcode, (const char*)TableIdString, (const char*)KeyString);
					break;
				}
				default:
					ISO_ASSERT(0);
			}
			break;
		}
		case EX_ObjectConst: {
			auto Pointer = ReadPointer<UObject>();
			output("$%X: EX_ObjectConst (%p:%s)", (int32)Opcode, Pointer, GetFullName(Pointer));
			break;
		}
		case EX_SoftObjectConst:
			output("$%X: EX_SoftObjectConst", (int32)Opcode);
			SerializeExpr();
			break;

		case EX_FieldPathConst:
			output("$%X: EX_FieldPathConst", (int32)Opcode);
			SerializeExpr();
			break;

		case EX_NameConst: {
			auto ConstValue = ReadName();
			output("$%X: literal name %s", (int32)Opcode, &*ConstValue);
			break;
		}
		case EX_RotationConst: {
			auto	ConstValue = reader.get<FRotator>();
			output("$%X: literal rotation (%f,%f,%f)", (int32)Opcode, ConstValue.Pitch, ConstValue.Yaw, ConstValue.Roll);
			break;
		}
		case EX_VectorConst: {
			auto	ConstValue = reader.get<FVector>();
			output("$%X: literal vector (%f,%f,%f)", (int32)Opcode, ConstValue.X, ConstValue.Y, ConstValue.Z);
			break;
		}
		case EX_TransformConst: {
			auto	ConstValue = reader.get<FTransform>();
			output("$%X: literal transform R(%f,%f,%f,%f) T(%f,%f,%f) S(%f,%f,%f)", (int32)Opcode, ConstValue.Translation.X, ConstValue.Translation.Y, ConstValue.Translation.Z, ConstValue.Rotation.X, ConstValue.Rotation.Y, ConstValue.Rotation.Z, ConstValue.Rotation.W, ConstValue.Scale3D.X, ConstValue.Scale3D.Y, ConstValue.Scale3D.Z);
			break;
		}
		case EX_StructConst: {
			auto	Struct		  = ReadPointer<UStruct>();
			int32		   SerializedSize = reader.get<int>();
			output("$%X: literal struct %s (serialized size: %d)", (int32)Opcode, GetName(Struct), SerializedSize);
			while (SerializeExpr() != EX_EndStructConst) {
			}
			break;
		}
		case EX_SetArray: {
			output("$%X: set array", (int32)Opcode);
			SerializeExpr();
			while (SerializeExpr() != EX_EndArray) {
			}
			break;
		}
		case EX_ArrayConst: {
			auto InnerProp = ReadPointer<FProperty>();
			int32	   Num		 = reader.get<int>();
			output("$%X: set array const - elements number: %d, inner property: %s", (int32)Opcode, Num, GetNameSafe(InnerProp));
			while (SerializeExpr() != EX_EndArrayConst) {
			}
			break;
		}
		case EX_ByteConst: {
			uint8 ConstValue = reader.get<uint8>();
			output("$%X: literal byte %d", (int32)Opcode, ConstValue);
			break;
		}
		case EX_IntConstByte: {
			int32 ConstValue = reader.get<uint8>();
			output("$%X: literal int %d", (int32)Opcode, ConstValue);
			break;
		}
		case EX_MetaCast: {
			auto Class = ReadPointer<UClass>();
			output("$%X: MetaCast to %s of expr:", (int32)Opcode, GetName(Class));
			SerializeExpr();
			break;
		}
		case EX_DynamicCast: {
			auto Class = ReadPointer<UClass>();
			output("$%X: DynamicCast to %s of expr:", (int32)Opcode, GetName(Class));
			SerializeExpr();
			break;
		}
		case EX_JumpIfNot: {
			// Code offset.
			CodeSkipSizeType SkipCount = reader.get<CodeSkipSizeType>();
			output("$%X: Jump to offset 0x%X if not expr:", (int32)Opcode, SkipCount);
			// Boolean expr.
			SerializeExpr();
			break;
		}
		case EX_Assert: {
			uint16 LineNumber  = reader.get<uint16>();
			uint8  InDebugMode = reader.get<uint8>();
			output("$%X: assert at line %d, in debug mode = %d with expr:", (int32)Opcode, LineNumber, InDebugMode);
			SerializeExpr();	 // Assert expr.
			break;
		}
		case EX_Skip: {
			CodeSkipSizeType W = reader.get<CodeSkipSizeType>();
			output("$%X: possibly skip 0x%X bytes of expr:", (int32)Opcode, W);
			// Expression to possibly skip.
			SerializeExpr();
			break;
		}
		case EX_InstanceDelegate: {
			// the name of the function assigned to the delegate.
			auto FuncName = ReadName();
			output("$%X: instance delegate function named %s", (int32)Opcode, &*FuncName);
			break;
		}
		case EX_AddMulticastDelegate:
			output("$%X: Add MC delegate", (int32)Opcode);
			SerializeExpr();
			SerializeExpr();
			break;

		case EX_RemoveMulticastDelegate:
			output("$%X: Remove MC delegate", (int32)Opcode);
			SerializeExpr();
			SerializeExpr();
			break;

		case EX_ClearMulticastDelegate:
			output("$%X: Clear MC delegate", (int32)Opcode);
			SerializeExpr();
			break;

		case EX_BindDelegate: {
			// the name of the function assigned to the delegate.
			auto FuncName = ReadName();
			output("$%X: BindDelegate '%s' ", (int32)Opcode, &*FuncName);
			output("Delegate:");	SerializeExpr();
			output("Object:");		SerializeExpr();
			break;
		}
		case EX_PushExecutionFlow: {
			CodeSkipSizeType SkipCount = reader.get<CodeSkipSizeType>();
			output("$%X: FlowStack.Push(0x%X);", (int32)Opcode, SkipCount);
			break;
		}
		case EX_PopExecutionFlow:
			output("$%X: if (FlowStack.Num()) { jump to statement at FlowStack.Pop(); } else { ERROR!!! }", (int32)Opcode);
			break;

		case EX_PopExecutionFlowIfNot:
			output("$%X: if (!condition) { if (FlowStack.Num()) { jump to statement at FlowStack.Pop(); } else { ERROR!!! } }", (int32)Opcode);
			// Boolean expr.
			SerializeExpr();
			break;

		case EX_Breakpoint:
			output("$%X: <<< BREAKPOINT >>>", (int32)Opcode);
			break;

		case EX_WireTracepoint:
			output("$%X: .. wire debug site ..", (int32)Opcode);
			break;

		case EX_InstrumentationEvent:
			switch (reader.get<uint8>()) {
				case FScriptInstrumentationSignal::InlineEvent:			output("$%X: .. instrumented inline event ..", (int32)Opcode); break;
				case FScriptInstrumentationSignal::Stop:				output("$%X: .. instrumented event stop ..", (int32)Opcode); break;
				case FScriptInstrumentationSignal::PureNodeEntry:		output("$%X: .. instrumented pure node entry site ..", (int32)Opcode); break;
				case FScriptInstrumentationSignal::NodeDebugSite:		output("$%X: .. instrumented debug site ..", (int32)Opcode); break;
				case FScriptInstrumentationSignal::NodeEntry:			output("$%X: .. instrumented wire entry site ..", (int32)Opcode); break;
				case FScriptInstrumentationSignal::NodeExit:			output("$%X: .. instrumented wire exit site ..", (int32)Opcode); break;
				case FScriptInstrumentationSignal::PushState:			output("$%X: .. push execution state ..", (int32)Opcode); break;
				case FScriptInstrumentationSignal::RestoreState:		output("$%X: .. restore execution state ..", (int32)Opcode); break;
				case FScriptInstrumentationSignal::ResetState:			output("$%X: .. reset execution state ..", (int32)Opcode); break;
				case FScriptInstrumentationSignal::SuspendState:		output("$%X: .. suspend execution state ..", (int32)Opcode); break;
				case FScriptInstrumentationSignal::PopState:			output("$%X: .. pop execution state ..", (int32)Opcode); break;
				case FScriptInstrumentationSignal::TunnelEndOfThread:	output("$%X: .. tunnel end of thread ..", (int32)Opcode); break;
			}
			break;

		case EX_Tracepoint:
			output("$%X: .. debug site ..", (int32)Opcode);
			break;

		case EX_SwitchValue: {
			const auto NumCases	 = reader.get<uint16>();
			const auto AfterSkip = reader.get<CodeSkipSizeType>();

			output("$%X: Switch Value %d cases, end in 0x%X", (int32)Opcode, NumCases, AfterSkip);
			AddIndent();
			output("Index:");
			SerializeExpr();

			for (uint16 CaseIndex = 0; CaseIndex < NumCases; ++CaseIndex) {
				output("[%d] Case Index (label: 0x%X):", CaseIndex);
				SerializeExpr();	 // case index value term
				const auto OffsetToNextCase = reader.get<CodeSkipSizeType>();
				output("[%d] Offset to the next case: 0x%X", CaseIndex, OffsetToNextCase);
				output("[%d] Case Result:", CaseIndex);
				SerializeExpr();	 // case term
			}

			output("Default result (label: 0x%X):");
			SerializeExpr();
			output("(label: 0x%X)");
			DropIndent();
			break;
		}
		case EX_ArrayGetByRef:
			output("$%X: Array Get-by-Ref Index", (int32)Opcode);
			AddIndent();
			SerializeExpr();
			SerializeExpr();
			DropIndent();
			break;

		default:
			// This should never occur
			ISO_ASSERT(0);
			break;
	}
}
