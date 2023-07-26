#ifndef CLR_H
#define CLR_H

#include "pe.h"
#include "base/algorithm.h"
#include "stream.h"

//-----------------------------------------------------------------------------
//	CLR
//-----------------------------------------------------------------------------

namespace clr {
using namespace iso;

enum HEAP {
	HEAP_String,
	HEAP_GUID,
	HEAP_Blob,
	HEAP_UserString,
	NumHeaps
};

struct HEADER : littleendian_types {
	enum FLAGS {
		FLAGS_ILONLY			= 0x00000001,
		FLAGS_32BITREQUIRED		= 0x00000002,
		FLAGS_IL_LIBRARY		= 0x00000004,
		FLAGS_STRONGNAMESIGNED	= 0x00000008,
		FLAGS_NATIVE_ENTRYPOINT	= 0x00000010,
		FLAGS_TRACKDEBUGDATA	= 0x00010000,
	};

	uint32	cb;
	uint16	MajorRuntimeVersion;
	uint16	MinorRuntimeVersion;

	// Symbol table and startup information
	pe::DATA_DIRECTORY	MetaData;
	uint32				Flags;

	// If FLAGS_NATIVE_ENTRYPOINT is not set, EntryPointToken represents a managed entrypoint.
	// If FLAGS_NATIVE_ENTRYPOINT is set, EntryPointRVA represents an RVA to a native entrypoint.
	uint32				EntryPoint;

	// Binding information
	pe::DATA_DIRECTORY	Resources;
	pe::DATA_DIRECTORY	StrongNameSignature;

	// Regular fixup and binding information
	pe::DATA_DIRECTORY	CodeManagerTable;
	pe::DATA_DIRECTORY	VTableFixups;
	pe::DATA_DIRECTORY	ExportAddressTableJumps;

	// Precompiled image info (internal use only - set to zero)
	pe::DATA_DIRECTORY	ManagedNativeHeader;
};

struct StreamHdr {
	uint32		Offset;		// Memory offset to start of this stream from start of the metadata root (§II.24.2.1)
	uint32		Size;		// Size of this stream in bytes, shall be a multiple of 4.
	embedded_string	Name;	// Name of the stream as null-terminated variable length array of ASCII characters, padded to the next 4-byte boundary with \0 characters. The name is limited to 32 characters.
	StreamHdr	*next()	const { return (StreamHdr*)(Name.begin() + align(Name.length() + 1, 4)); }
};

struct METADATA_ROOT : littleendian_types {
	enum {SIGNATURE = 'BSJB'};
	uint32		Signature;
	uint16		MajorVersion;
	uint16		MinorVersion;
	uint32		Reserved;		// always 0
	uint32		Length;
	char		Version[];		// UTF8-encoded null-terminated version string of length Length
//	uint16		Reserved;		// always 0
//	uint16		NumberStream;	// Number of streams
//	StreamHdr	StreamHeaders[];
	int			NumHeaders()	const { return *(uint16*)(Version	+ Length + 2); }
	StreamHdr	*GetFirstHdr()	const { return (StreamHdr*)(Version	+ Length + 4); }
	range<next_iterator<StreamHdr> >	Headers() const { return make_range_n(make_next_iterator(GetFirstHdr()), NumHeaders()); }
};

enum TABLETYPE {
	Module						= 0x00,
	TypeRef						= 0x01,
	TypeDef						= 0x02,
	// Unused					= 0x03,
	Field						= 0x04,
	// Unused					= 0x05,
	MethodDef					= 0x06,
	// Unused					= 0x07,
	Param						= 0x08,
	InterfaceImpl				= 0x09,
	MemberRef					= 0x0a,
	Constant					= 0x0b,
	CustomAttribute				= 0x0c,
	FieldMarshal				= 0x0d,
	DeclSecurity				= 0x0e,
	ClassLayout					= 0x0f,

	FieldLayout					= 0x10,
	StandAloneSig				= 0x11,
	EventMap					= 0x12,
	// Unused					= 0x13,
	Event						= 0x14,
	PropertyMap					= 0x15,
	// Unused					= 0x16,
	Property					= 0x17,
	MethodSemantics				= 0x18,
	MethodImpl					= 0x19,
	ModuleRef					= 0x1a,
	TypeSpec					= 0x1b,
	ImplMap						= 0x1c,
	FieldRVA					= 0x1d,
	// Unused					= 0x1e,
	// Unused					= 0x1f,

	Assembly					= 0x20,
	AssemblyProcessor			= 0x21,
	AssemblyOS					= 0x22,
	AssemblyRef					= 0x23,
	AssemblyRefProcessor		= 0x24,
	AssemblyRefOS				= 0x25,
	File						= 0x26,
	ExportedType				= 0x27,
	ManifestResource			= 0x28,
	NestedClass					= 0x29,
	GenericParam				= 0x2a,
	MethodSpec					= 0x2b,
	GenericParamConstraint		= 0x2c,

	NumTables,

	StringHeap					= 0x70,
};
enum ELEMENT_TYPE {
	ELEMENT_TYPE_END			= 0x00,	// Marks end of a list
	ELEMENT_TYPE_VOID			= 0x01,
	ELEMENT_TYPE_BOOLEAN		= 0x02,
	ELEMENT_TYPE_CHAR			= 0x03,
	ELEMENT_TYPE_I1				= 0x04,
	ELEMENT_TYPE_U1				= 0x05,
	ELEMENT_TYPE_I2				= 0x06,
	ELEMENT_TYPE_U2				= 0x07,
	ELEMENT_TYPE_I4				= 0x08,
	ELEMENT_TYPE_U4				= 0x09,
	ELEMENT_TYPE_I8				= 0x0a,
	ELEMENT_TYPE_U8				= 0x0b,
	ELEMENT_TYPE_R4				= 0x0c,
	ELEMENT_TYPE_R8				= 0x0d,
	ELEMENT_TYPE_STRING			= 0x0e,
	ELEMENT_TYPE_PTR			= 0x0f,	// Followed by type
	ELEMENT_TYPE_BYREF			= 0x10,	// Followed by type
	ELEMENT_TYPE_VALUETYPE		= 0x11,	// Followed by TypeDefOrRef token
	ELEMENT_TYPE_CLASS			= 0x12,	// Followed by TypeDefOrRef token
	ELEMENT_TYPE_VAR			= 0x13,	// Generic parameter in a generic type definition, represented as number (compressed unsigned integer)
	ELEMENT_TYPE_ARRAY			= 0x14,	// type rank boundsCount bound1 loCount	= lo1
	ELEMENT_TYPE_GENERICINST	= 0x15,	// Generic type instantiation. Followed by type	type-arg-count type-1 type-n
	ELEMENT_TYPE_TYPEDBYREF		= 0x16,	//
	ELEMENT_TYPE_I				= 0x18,	// System.IntPtr
	ELEMENT_TYPE_U				= 0x19,	// System.UIntPtr
	ELEMENT_TYPE_FNPTR			= 0x1b,	// Followed by full method signature
	ELEMENT_TYPE_OBJECT			= 0x1c,	// System.Object
	ELEMENT_TYPE_SZARRAY		= 0x1d,	// Single-dim array with 0 lower bound
	ELEMENT_TYPE_MVAR			= 0x1e,	// Generic parameter in a generic method definition, represented as number (compressed unsigned integer)
	ELEMENT_TYPE_CMOD_REQD		= 0x1f,	// Required modifier : followed by a TypeDef or TypeRef token
	ELEMENT_TYPE_CMOD_OPT		= 0x20,	// Optional modifier : followed by a TypeDef or TypeRef token
	ELEMENT_TYPE_INTERNAL		= 0x21,	// Implemented within the CLI

	ELEMENT_TYPE_MODIFIER		= 0x40,	// Or’d with following element types
	ELEMENT_TYPE_SENTINEL		= 0x41,	// Sentinel for vararg method signature
	ELEMENT_TYPE_PINNED			= 0x45,	// Denotes a local variable that points at a pinned object
	ELEMENT_TYPE_SYSTEMTYPE		= 0x50,	// Indicates an argument of type System.Type.
	ELEMENT_TYPE_BOXED			= 0x51,	// Used in custom attributes to specify a boxed object
	ELEMENT_TYPE_				= 0x52,	// Reserved
	ELEMENT_TYPE_FIELD			= 0x53,	// Used in custom attributes to indicate a FIELD
	ELEMENT_TYPE_PROPERTY		= 0x54,	// Used in custom attributes to indicate a PROPERTY
	ELEMENT_TYPE_ENUM			= 0x55,	// Used in custom attributes to specify an enum
};

enum NATIVE_TYPE {
	NATIVE_TYPE_BOOLEAN			= 0x02,
	NATIVE_TYPE_I1				= 0x03,
	NATIVE_TYPE_U1				= 0x04,
	NATIVE_TYPE_I2				= 0x05,
	NATIVE_TYPE_U2				= 0x06,
	NATIVE_TYPE_I4				= 0x07,
	NATIVE_TYPE_U4				= 0x08,
	NATIVE_TYPE_I8				= 0x09,
	NATIVE_TYPE_U8				= 0x0a,
	NATIVE_TYPE_R4				= 0x0b,
	NATIVE_TYPE_R8				= 0x0c,
	NATIVE_TYPE_LPSTR			= 0x14,
	NATIVE_TYPE_LPWSTR			= 0x15,
	NATIVE_TYPE_INT				= 0x1f,
	NATIVE_TYPE_UINT			= 0x20,
	NATIVE_TYPE_FUNC			= 0x26,
	NATIVE_TYPE_ARRAY			= 0x2a,

	// Microsoft specific
	NATIVE_TYPE_CURRENCY		= 0x0f,
	NATIVE_TYPE_BSTR			= 0x13,
	NATIVE_TYPE_LPTSTR			= 0x16,
	NATIVE_TYPE_FIXEDSYSSTRING	= 0x17,
	NATIVE_TYPE_IUNKNOWN		= 0x19,
	NATIVE_TYPE_IDISPATCH		= 0x1a,
	NATIVE_TYPE_STRUCT			= 0x1b,
	NATIVE_TYPE_INTF			= 0x1c,
	NATIVE_TYPE_SAFEARRAY		= 0x1d,
	NATIVE_TYPE_FIXEDARRAY		= 0x1e,
	NATIVE_TYPE_BYVALSTR		= 0x22,
	NATIVE_TYPE_ANSIBSTR		= 0x23,
	NATIVE_TYPE_TBSTR			= 0x24,
	NATIVE_TYPE_VARIANTBOOL		= 0x25,
	NATIVE_TYPE_ASANY			= 0x28,
	NATIVE_TYPE_LPSTRUCT		= 0x2b,
	NATIVE_TYPE_CUSTOMMARSHALER	= 0x2c,
	NATIVE_TYPE_ERROR			= 0x2d,

	NATIVE_TYPE_MAX				= 0x50,
};

constexpr size_t static GetSize(ELEMENT_TYPE type) {
	switch (type) {
		case ELEMENT_TYPE_BOOLEAN:
		case ELEMENT_TYPE_CHAR:
		case ELEMENT_TYPE_I1:
		case ELEMENT_TYPE_U1:			return 1;
		case ELEMENT_TYPE_I2:
		case ELEMENT_TYPE_U2:			return 2;
		case ELEMENT_TYPE_R4:			return 1;
		case ELEMENT_TYPE_I4:
		case ELEMENT_TYPE_U4:			return 4;
		case ELEMENT_TYPE_I8:
		case ELEMENT_TYPE_U8:
		case ELEMENT_TYPE_R8:			return 8;
		case ELEMENT_TYPE_STRING:
		case ELEMENT_TYPE_PTR:
		case ELEMENT_TYPE_BYREF:		return 4;
		case ELEMENT_TYPE_I:
		case ELEMENT_TYPE_U:			return 4;
		default:						return 0;
	}
}
constexpr bool static IsSigned(ELEMENT_TYPE type) {
	switch (type) {
		//case ELEMENT_TYPE_CHAR:
		case ELEMENT_TYPE_I1:
		case ELEMENT_TYPE_I2:
		case ELEMENT_TYPE_R4:
		case ELEMENT_TYPE_I4:
		case ELEMENT_TYPE_I8:
		case ELEMENT_TYPE_R8:			return true;
		case ELEMENT_TYPE_I:
		default:						return false;
	}
}
constexpr size_t static GetSize(NATIVE_TYPE type) {
	switch (type) {
		case NATIVE_TYPE_BOOLEAN:
		case NATIVE_TYPE_I1:
		case NATIVE_TYPE_U1:			return 1;
		case NATIVE_TYPE_I2:
		case NATIVE_TYPE_U2:			return 2;
		case NATIVE_TYPE_I4:
		case NATIVE_TYPE_U4:
		case NATIVE_TYPE_R4:			return 4;
		case NATIVE_TYPE_I8:
		case NATIVE_TYPE_U8:
		case NATIVE_TYPE_R8:			return 8;
		case NATIVE_TYPE_LPSTR:
		case NATIVE_TYPE_LPWSTR:
		case NATIVE_TYPE_INT:
		case NATIVE_TYPE_UINT:
		case NATIVE_TYPE_FUNC:			return 4;
		case NATIVE_TYPE_ARRAY:
		default:						return 0;
	}
}

enum SIGNATURE {
	HASTHIS			= 0x20,
	EXPLICITTHIS	= 0x40,

	TYPE_MASK		= 0x1f,
	DEFAULT			= 0,
	C				= 1,
	STDCALL			= 2,
	THISCALL		= 3,
	FASTCALL		= 4,
	VARARG			= 5,
	FIELD			= 6,
	LOCAL_SIG		= 7,
	PROPERTY		= 8,
	GENRICINST		= 0xa,
	GENERIC			= 0x10,
};

enum ACCESS {
	ACCESS_CompilerControlled	= 0,	// Member not referenceable
	ACCESS_Private				= 1,	// Accessible only by the parent type
	ACCESS_FamANDAssem			= 2,	// Accessible by sub-types only in this Assembly
	ACCESS_Assem				= 3,	// Accessibly by anyone in the Assembly
	ACCESS_Family				= 4,	// Accessible only by type and sub-types
	ACCESS_FamORAssem			= 5,	// Accessibly by sub-types anywhere, plus anyone in assembly
	ACCESS_Public				= 6,	// Accessibly by anyone who has visibility to this scope
};

// internal types
struct CompressedInt {
	int32	t;
	CompressedInt() {}
	template<typename R> CompressedInt(R &r) {
		uint32	b = r.getc();
		if (b & 0x80) {
			b = ((b & 0x7f) << 8) | r.getc();
			if (b & 0x4000) {
				b = ((b & 0x7f) << 8) | r.getc();
				b = (b << 8) | r.getc();
				t = (b >> 1) - ((b & 1) << 28);
			} else {
				t = (b >> 1) - ((b & 1) << 13);
			}
		} else {
			t = (b >> 1) - ((b & 1) << 6);
		}
	}
	operator int32() const { return t; }
};
struct CompressedUInt {
	uint32	t;
	CompressedUInt() {}
	template<typename R> CompressedUInt(R &r){
		uint32	b = r.getc();
		if (b == 0xff) {
			//reserved for null string
			t = ~0u;
			return;
		}
		if (b & 0x80) {
			b = ((b & 0x7f) << 8) | r.getc();
			if (b & 0x4000) {
				b = ((b & 0x7f) << 8) | r.getc();
				b = (b << 8) | r.getc();
			}
		}
		t = b;
	}
	operator uint32() const { return t; }
};
struct SerString : count_string {
	template<typename R> SerString(R &r) : count_string(0) {
		uint32	len = CompressedUInt(r);
		if (~len)
			this->p = make_range<const char>(r.get_block(len));
	}
};
struct String : string_base<const char*> {
//	String(String&)	= default;
//	String(const String&)	= default;
	template<typename R> String(reader<R> &r)	: string_base<const char*>(r.me().GetString())	{}
};
struct Blob : const_memory_block {
	Blob()		{}
	template<typename R> Blob(reader<R> &r)		: const_memory_block(r.me().GetBlob())	{}
};
struct TypeSignature : Blob {
	TypeSignature()	{}
	template<typename R> TypeSignature(R &r) : Blob(r)	{}
};
struct Signature : Blob {
	Signature()	{}
	template<typename R> Signature(R &r) : Blob(r)	{}
	SIGNATURE	flags() { return SIGNATURE(((const uint8*)*this)[0]); }
};
struct CustomAttributeValue : Blob {
	CustomAttributeValue()	{}
	template<typename R> CustomAttributeValue(R &r) : Blob(r)	{}
};
struct Code {
	uint32				start;
	const_memory_block	data;
	template<typename R> Code(reader<R> &r)		: start(r.get())	{}
};
template<TABLETYPE I> struct Indexed : holder<uint32> {
	Indexed(uint32 _i)	: holder<uint32>(_i)	{}
	template<typename R> Indexed(R &r)	: holder<uint32>(r.GetIndex(I))	{}
};
template<TABLETYPE T> struct IndexedList : Indexed<T> {
	template<typename R> IndexedList(R &r) : Indexed<T>(r)	{}
};
template<typename T>	struct CodedIndex {
	static uint8 trans[];
	uint32		i;
	TABLETYPE	type()		const	{ return TABLETYPE(trans[i & bits(T::B)]); }
	uint32		index()		const	{ return i >> T::B; }
	explicit operator bool() const	{ return !!i;}
	CodedIndex(uint32 _i)					: i(_i)		{}
	template<typename R> CodedIndex(R &r)	: i(r.GetCodedIndex(T::N, T::B, trans))	{}
};
struct Token {
	uint32		i;
	TABLETYPE	type()		const	{ return TABLETYPE(i >> 24); }
	uint32		index()		const	{ return i & 0xffffff; }
	explicit operator bool() const	{ return !!index();}
	Token()						{}
	Token(uint32 _i)	: i(_i)	{}
	template<typename T>	Token(const CodedIndex<T> &c) : i((c.type() << 24) + c.index()) {}
	template<TABLETYPE I>	Token(const Indexed<I> &c) : i((I << 24) + c) {}
	friend bool operator==(const Token &a, const Token &b) { return a.i == b.i; }
	friend bool operator!=(const Token &a, const Token &b) { return a.i != b.i; }
};
struct _TypeDefOrRef		{ enum {TypeDef, TypeRef, TypeSpec, N, B=2}; };
struct _HasConstant			{ enum {Field, Param, Property, N, B=2}; };
struct _HasCustomAttribute	{ enum {
	MethodDef, Field, TypeRef, TypeDef, Param, InterfaceImpl, MemberRef, Module, Permission, Property, Event, StandAloneSig,
	ModuleRef, TypeSpec, Assembly, AssemblyRef, File, ExportedType, ManifestResource, GenericParam, GenericParamConstraint, MethodSpec,
	N, B=5
}; };
struct _HasFieldMarshall	{ enum {Field, Param, N, B=1}; };
struct _HasDeclSecurity		{ enum {TypeDef, MethodDef, Assembly, N, B=2}; };
struct _MemberRefParent		{ enum {TypeDef, TypeRef, ModuleRef, MethodDef, TypeSpec, N, B=3}; };
struct _HasSemantics		{ enum {Event, Property, N, B=1}; };
struct _MethodDefOrRef		{ enum {MethodDef, MemberRef, N, B=1}; };
struct _MemberForwarded		{ enum {Field, MethodDef, N, B=1}; };
struct _Implementation		{ enum {File, AssemblyRef, ExportedType, N, B=2}; };
struct _CustomAttributeType	{ enum {MethodDef = 2, MemberRef, N, B=3}; };
struct _TypeOrMethodDef		{ enum {TypeDef, MethodDef, N, B=1}; };
struct _ResolutionScope		{ enum {Module, ModuleRef, AssemblyRef, TypeRef, N, B=2}; };

typedef CodedIndex<_TypeDefOrRef>			TypeDefOrRef;
typedef CodedIndex<_HasConstant>			HasConstant;
typedef CodedIndex<_HasCustomAttribute>		HasCustomAttribute;
typedef CodedIndex<_HasFieldMarshall>		HasFieldMarshall;
typedef CodedIndex<_HasDeclSecurity>		HasDeclSecurity;
typedef CodedIndex<_MemberRefParent>		MemberRefParent;
typedef CodedIndex<_HasSemantics>			HasSemantics;
typedef CodedIndex<_MethodDefOrRef>			MethodDefOrRef;
typedef CodedIndex<_MemberForwarded>		MemberForwarded;
typedef CodedIndex<_Implementation>			Implementation;
typedef CodedIndex<_CustomAttributeType>	CustomAttributeType;
typedef CodedIndex<_TypeOrMethodDef>		TypeOrMethodDef;
typedef CodedIndex<_ResolutionScope>		ResolutionScope;

// table schema
template<TABLETYPE T> struct ENTRY;

struct TABLES : littleendian_types {
	uint32	Reserved;		// Reserved, always 0 (§II.24.1).
	uint8	MajorVersion;	// Major version of table schemata; shall be 2 (§II.24.1).
	uint8	MinorVersion;	// Minor version of table schemata; shall be 0 (§II.24.1).
	uint8	HeapSizes;		// Bit vector for heap sizes.
	uint8	Reserved2;		// Reserved, always 1 (§II.24.1).
	uint64	Valid;			// Bit vector of present tables, let n be the number of bits that are 1.
	uint64	Sorted;			// Bit vector of sorted tables.
	uint32	Rows[];			// Array of n 4-byte unsigned integers indicating the number of rows for each present table.
};

template<> struct ENTRY<Module> {
	uint16				generation;
	String				name;
	GUID				mvid, encid, encbaseid;
	template<typename R> explicit ENTRY(R &r) : generation(r), name(r), mvid(r), encid(r), encbaseid(r)	{}
};
template<> struct ENTRY<TypeRef> {
	ResolutionScope		scope;
	String				name;
	String				namespce;
	template<typename R> explicit ENTRY(R &r) : scope(r), name(r), namespce(r)	{}
};
template<> struct ENTRY<TypeDef> {
	enum Flags {
		//Visibility attributes
		VisibilityMask		= 0x000007,	// Use this mask to retrieve visibility information.
			NotPublic			= 0,	// Class has no public scope
			Public				= 1,	// Class has public scope
			NestedPublic		= 2,	// Class is nested with public visibility
			NestedPrivate		= 3,	// Class is nested with private visibility
			NestedFamily		= 4,	// Class is nested with family visibility
			NestedAssem			= 5,	// Class is nested with assembly visibility
			NestedFamANDAssem	= 6,	// Class is nested with family and assembly visibility
			NestedFamORAssem	= 7,	// Class is nested with family or assembly visibility
		//Class layout attributes
		LayoutMask			= 0x000018,	// Use this mask to retrieve class layout information
			AutoLayout			= 0x00,	// Class fields are auto-laid out
			SequentialLayout	= 0x08,	// Class fields are laid out sequentially
			ExplicitLayout		= 0x10,	// Layout is supplied explicitly
		//Class semantics attributes
		Interface			= 0x000020,	// Type is an interface (else a class)
		//Special semantics in addition to class semantics
		Abstract			= 0x000080,	// Class is abstract
		Sealed				= 0x000100,	// Class cannot be extended
		SpecialName			= 0x000400,	// Class name is special
		//Implementation Attributes
		Import				= 0x001000,	// Class/Interface is imported
		Serializable		= 0x002000,	// Reserved (Class is serializable)
		WinRT				= 0x004000,	// Undocumented
		//String formatting Attributes
		StringFormatMask	= 0x030000,	// Use this mask to retrieve string information for native interop
			AnsiClass			= 0x000000,	// LPSTR is interpreted as ANSI
			UnicodeClass		= 0x010000,	// LPSTR is interpreted as Unicode
			AutoClass			= 0x020000,	// LPSTR is interpreted automatically
			CustomFormatClass	= 0x030000,	// A non-standard encoding specified by
		//CustomStringFormatMask
		CustomStringFormat		=0x100000,
		CustomStringFormatMask	=0xC00000,// Use this mask to retrieve non-standard encoding information for native interop. The meaning of the values of these 2 bits is unspecified.
		//Class Initialization Attributes
		BeforeFieldInit		= 0x100000,	// Initialize the class before first static field access
		//Additional Flags
		RTSpecialName		= 0x000800,	// CLI provides 'special' behavior, depending upon the name of the Type
		HasSecurity			= 0x040000,	// Type has security associate with it
		IsTypeForwarder		= 0x200000,	// This ExportedType entry is a type forwarder
	};
	Flags					flags;
	String					name;
	String					namespce;
	TypeDefOrRef			extends;
	IndexedList<Field>		fields;
	IndexedList<MethodDef>	methods;
	template<typename R> explicit ENTRY(R &r) : flags(r), name(r), namespce(r), extends(r), fields(r), methods(r)	{}
	uint32		visibility() const { return flags & VisibilityMask; }
};
template<> struct ENTRY<Field> {
	enum Flags {
		FieldAccessMask		= 0x0007,	// These 3 bits contain an ACCESS values
		Static				= 0x0010,	// Defined on type, else per instance
		InitOnly			= 0x0020,	// Field can only be initialized, not written to after init
		Literal				= 0x0040,	// Value is compile time constant
		NotSerialized		= 0x0080,	// Reserved (to indicate this field should not be serialized when type is remoted)
		HasFieldRVA			= 0x0100,	// Field has RVA
		SpecialName			= 0x0200,	// Field is special
		RTSpecialName		= 0x0400,	// CLI provides 'special' behavior, depending upon the name of the field
		HasFieldMarshal		= 0x1000,	// Field has marshalling information
		PInvokeImpl			= 0x2000,	// Implementation is forwarded through PInvoke.
		HasDefault			= 0x8000,	// Field has default
	};
	compact<Flags,16>	flags;
	String				name;
	Signature			signature;
	template<typename R> explicit ENTRY(R &r) : flags(r), name(r), signature(r) {}
	ACCESS		access() const { return ACCESS(flags & FieldAccessMask); }
};
template<> struct ENTRY<MethodDef> {
	enum ImplFlags {
		CodeTypeMask		= 0x0003,	// These 2 bits contain one of the following values:
			IL					= 0,	// Method impl is CIL
			Native				= 1,	// Method impl is native
			OPTIL				= 2,	// Reserved: shall be zero in conforming implementations
			Runtime				= 3,	// Method impl is provided by the runtime
		Unmanaged			= 0x0004,	// Method impl is unmanaged, otherwise managed
		NoInlining			= 0x0008,	// Method cannot be inlined
		ForwardRef			= 0x0010,	// Indicates method is defined; used primarily in merge scenarios
		Synchronized		= 0x0020,	// Method is single threaded through the body
		NoOptimization		= 0x0040,	// Method will not be optimized when generating native code
		PreserveSig			= 0x0080,	// Reserved: conforming implementations can ignore
		InternalCall		= 0x1000,	// Reserved: shall be zero in conforming implementations
	};
	enum Flags {
		ACCESS_CompilerControlled,ACCESS_Private,ACCESS_FamANDAssem,ACCESS_Assem,ACCESS_Family,ACCESS_FamORAssem,ACCESS_Public,
		MemberAccessMask	= 0x0007,	// These 3 bits contain an ACCESS value
		UnmanagedExport		= 0x0008,	// Reserved: shall be zero for conforming implementations
		Static				= 0x0010,	// Defined on type, else per instance
		Final				= 0x0020,	// Method cannot be overridden
		Virtual				= 0x0040,	// Method is virtual
		HideBySig			= 0x0080,	// Method hides by name+sig, else just by name
		NewSlot				= 0x0100,	// Method always gets a new slot in the vtable (otherwise reuse)
		Strict				= 0x0200,	// Method can only be overriden if also accessible
		Abstract			= 0x0400,	// Method does not provide an implementation
		SpecialName			= 0x0800,	// Method is special
		RTSpecialName		= 0x1000,	// CLI provides 'special' behavior, depending upon the name of the method
		PInvokeImpl			= 0x2000,	// Implementation is forwarded through PInvoke
		HasSecurity			= 0x4000,	// Method has security associate with it
		RequireSecObject	= 0x8000,	// Method calls another method containing security code.
	};
	Code					code;
	compact<ImplFlags,16>	implflags;
	compact<Flags,16>		flags;
	String					name;
	Signature				signature;
	IndexedList<Param>		paramlist;
	template<typename R> explicit ENTRY(R &r) : code(r), implflags(r), flags(r), name(r), signature(r), paramlist(r) {
		code.data = r.GetCode(code.start, !!(implflags & Unmanaged));
	}
	bool		IsCIL()		const { return (implflags & CodeTypeMask) == IL; }
	ACCESS		access()	const { return ACCESS(flags & MemberAccessMask); }
};
template<> struct ENTRY<Param> {
	enum Flags {
		In					= 0x0001,	// Param is [In]
		Out					= 0x0002,	// Param is [out]
		Optional			= 0x0010,	// Param is optional
		HasDefault			= 0x1000,	// Param has default value
		HasFieldMarshal		= 0x2000,	// Param has FieldMarshal
		Unused				= 0xcfe0,
	};
	compact<Flags,16>	flags;
	uint16				sequence;
	String				name;
	template<typename R> explicit ENTRY(R &r) : flags(r), sequence(r), name(r) {}
};
template<> struct ENTRY<InterfaceImpl> {
	Indexed<TypeDef>	clss;
	TypeDefOrRef		interfce;
	template<typename R> explicit ENTRY(R &r) : clss(r), interfce(r)	{}
};
template<> struct ENTRY<MemberRef> {
	MemberRefParent		clss;
	String				name;
	Signature			signature;
	template<typename R> explicit ENTRY(R &r) : clss(r), name(r), signature(r) {}
};
template<> struct ENTRY<Constant> {
	compact<ELEMENT_TYPE,16>	type;
	HasConstant			parent;
	Blob				value;
	template<typename R> explicit ENTRY(R &r) : type(r.template get<compact<ELEMENT_TYPE,16> >()), parent(r), value(r) {}
};
template<> struct ENTRY<CustomAttribute> {
	HasCustomAttribute	parent;
	CustomAttributeType	type;
	CustomAttributeValue value;
	template<typename R> explicit ENTRY(R &r) : parent(r), type(r), value(r) {}
};
template<> struct ENTRY<FieldMarshal> {
	HasFieldMarshall	parent;
	Blob				native_type;
	template<typename R> explicit ENTRY(R &r) : parent(r), native_type(r) {}
};
template<> struct ENTRY<DeclSecurity> {
	uint16				action;
	HasDeclSecurity		parent;
	Blob				permission_set;
	template<typename R> explicit ENTRY(R &r) : action(r), parent(r), permission_set(r) {}
};
template<> struct ENTRY<ClassLayout> {
	uint16				packing_size;
	uint32				class_size;
	Indexed<TypeDef>	parent;
	template<typename R> explicit ENTRY(R &r) : packing_size(r), class_size(r), parent(r) {}
};
template<> struct ENTRY<FieldLayout> {
	uint32				offset;
	Indexed<Field>		field;
	template<typename R> explicit ENTRY(R &r) : offset(r), field(r) {}
};
template<> struct ENTRY<StandAloneSig> {
	Signature			signature;
	template<typename R> explicit ENTRY(R &r) : signature(r) {}
};
template<> struct ENTRY<EventMap> {
	Indexed<TypeDef>	parent;
	IndexedList<Event>	event_list;
	template<typename R> explicit ENTRY(R &r) : parent(r), event_list(r)	{}
};
template<> struct ENTRY<Event> {
	enum Flags {
		SpecialName		= 0x0200,	// Event is special.
		RTSpecialName	= 0x0400,	// CLI provides 'special' behavior, depending upon the name of the event
	};
	compact<Flags,16>	flags;
	String				name;
	TypeDefOrRef		event_type;
	template<typename R> explicit ENTRY(R &r) : flags(r), name(r), event_type(r) {}
};
template<> struct ENTRY<PropertyMap> {
	Indexed<TypeDef>		parent;
	IndexedList<Property>	property_list;
	template<typename R> explicit ENTRY(R &r) : parent(r), property_list(r) {}
};
template<> struct ENTRY<Property> {
	enum Flags {
		SpecialName		= 0x0200,	// Property is special
		RTSpecialName	= 0x0400,	// Runtime(metadata internal APIs) should check name encoding
		HasDefault		= 0x1000,	// Property has default
		Unused			= 0xe9ff,	// Reserved: shall be zero in a conforming implementation
	};
	compact<Flags,16>	flags;
	String				name;
	Signature			type;
	template<typename R> explicit ENTRY(R &r) : flags(r), name(r), type(r) {}
};
template<> struct ENTRY<MethodSemantics> {
	enum Flags {
		Setter			= 0x0001,	// Setter for property
		Getter			= 0x0002,	// Getter for property
		Other			= 0x0004,	// Other method for property or event
		AddOn			= 0x0008,	// AddOn method for event. This refers to the required add_method for events. (§22.13)
		RemoveOn		= 0x0010,	// RemoveOn method for event. . This refers to the required remove_ method for events. (§22.13)
		Fire			= 0x0020,	// Fire method for event. This refers to the optional raise_method for events. (§22.13)
	};
	compact<Flags,16>	flags;
	Indexed<MethodDef>	method;
	HasSemantics		association;
	template<typename R> explicit ENTRY(R &r) : flags(r), method(r), association(r) {}
};
template<> struct ENTRY<MethodImpl> {
	Indexed<TypeDef>	clss;
	MethodDefOrRef		method_body;
	MethodDefOrRef		method_declaration;
	template<typename R> explicit ENTRY(R &r) : clss(r), method_body(r), method_declaration(r) {}
};
template<> struct ENTRY<ModuleRef> {
	String				name;
	template<typename R> explicit ENTRY(R &r) : name(r) {}
};
template<> struct ENTRY<TypeSpec> {
	TypeSignature		signature;
	template<typename R> explicit ENTRY(R &r) : signature(r) {}
};
template<> struct ENTRY<ImplMap> {
	enum Flags {
		NoMangle			= 0x0001,	// PInvoke is to use the member name as specified Character set
		CharSetMask			= 0x0006,	// This is a resource file or other non-metadata-containing file.
			CharSetNotSpec		= 0,
			CharSetAnsi			= 2,
			CharSetUnicode		= 4,
			CharSetAuto			= 6,
		SupportsLastError	= 0x0040,	// Information about target function. Not relevant for fields
		//Calling convention
		CallConvMask		= 0x0700,
			CallConvPlatformapi	= 0x0100,
			CallConvCdecl		= 0x0200,
			CallConvStdcall		= 0x0300,
			CallConvThiscall	= 0x0400,
			CallConvFastcall	= 0x0500,
	};
	compact<Flags,16>	flags;
	MemberForwarded		member_forwarded;
	String				name;
	Indexed<ModuleRef>	scope;
	template<typename R> explicit ENTRY(R &r) : flags(r), member_forwarded(r), name(r), scope(r) {}
};
template<> struct ENTRY<FieldRVA> {
	uint32				rva;
	Indexed<Field>		field;
	template<typename R> explicit ENTRY(R &r) : rva(r), field(r) {}
};
template<> struct ENTRY<Assembly> {
	enum HashAlgorithm {
		None						= 0x0000,
		Reserved_MD5				= 0x8003,
		SHA1						= 0x8004,
	};
	enum Flags {
		PublicKey					= 0x0001,	// The assembly reference holds the full (unhashed) public key.
		Retargetable				= 0x0100,	// The implementation of this assembly used at runtime is not expected to match the version seen at compile time
		WinRT						= 0x0200,	// Undocumented
		DisableJITcompileOptimizer	= 0x4000,
		EnableJITcompileTracking	= 0x8000,
	};
	HashAlgorithm		hashalg;
	uint16				major, minor, build, rev;
	Flags				flags;
	Blob				publickey;
	String				name;
	String				culture;
	template<typename R> explicit ENTRY(R &r) : hashalg(r),
		major(r), minor(r), build(r), rev(r),
		flags(r), publickey(r), name(r), culture(r)
	{}
};
template<> struct ENTRY<AssemblyProcessor> {
	uint32				processor;
	template<typename R> explicit ENTRY(R &r) : processor(r) {}
};
template<> struct ENTRY<AssemblyOS> {
	uint32				platform, major, minor;
	template<typename R> explicit ENTRY(R &r) : platform(r), major(r), minor(r) 	{}
};
template<> struct ENTRY<AssemblyRef> {
	//using ENTRY<Assembly>::Flags;

	uint16				major, minor, build, rev;
	ENTRY<Assembly>::Flags	flags;
	Blob				publickey;
	String				name;
	String				culture;
	Blob				hashvalue;
	template<typename R> explicit ENTRY(R &r) :
		major(r), minor(r), build(r), rev(r),
		flags(r), publickey(r), name(r), culture(r),
		hashvalue(r)
	{}
};
template<> struct ENTRY<AssemblyRefProcessor> {
	uint32					processor;
	Indexed<AssemblyRef>	assembly;
	template<typename R> explicit ENTRY(R &r) : processor(r), assembly(r) {}
};
template<> struct ENTRY<AssemblyRefOS> {
	uint32					platform, major, minor;
	Indexed<AssemblyRef>	assembly;
	template<typename R> explicit ENTRY(R &r) : platform(r), major(r), minor(r), assembly(r) {}
};
template<> struct ENTRY<File> {
	enum Flags {
		ContainsMetaData	= 0x0000,	// This is not a resource file
		ContainsNoMetaData	= 0x0001,	// This is a resource file or other non-metadata-containing file
	};
	uint32				flags;
	String				name;
	Blob				hash;
	template<typename R> explicit ENTRY(R &r) : flags(r), name(r), hash(r) {}
};
template<> struct ENTRY<ExportedType> {
	//using ENTRY<TypeDef>::Flags;
	ENTRY<TypeDef>::Flags	flags;
	uint32				typedef_id;//(a 4-byte index into a TypeDef table of another module in this Assembly).
	String				name;
	String				namespce;
	Implementation		implementation;
	template<typename R> explicit ENTRY(R &r) : flags(r), typedef_id(r), name(r), namespce(r), implementation(r) {}
};
template<> struct ENTRY<ManifestResource> {
	enum Flags {
		VisibilityMask		= 0x0007,	// These 3 bits contain one of the following values:
		Public				= 0x0001,	// The Resource is exported from the Assembly
		Private				= 0x0002,	// The Resource is private to the Assembly
	};
	uint32				offset;
	Flags				flags;
	String				name;
	Implementation		implementation;
	template<typename R> explicit ENTRY(R &r) : offset(r), flags(r), name(r), implementation(r) {}
};
template<> struct ENTRY<NestedClass> {
	Indexed<TypeDef>	nested_class;
	Indexed<TypeDef>	enclosing_class;
	template<typename R> explicit ENTRY(R &r) : nested_class(r), enclosing_class(r) {}
};
template<> struct ENTRY<GenericParam> {
	enum Flags {
		VarianceMask					= 0x0003,		// These 2 bits contain one of the following values:
			None							= 0,		// The generic parameter is non-variant and has no special constraints
			Covariant						= 1,		// The generic parameter is covariant
			Contravariant					= 2,		// The generic parameter is contravariant
		SpecialConstraintMask			= 0x001C,		// These 3 bits contain one of the following values:
			ReferenceTypeConstraint			= 0x0004,	// The generic parameter has the class special constraint
			NotNullableValueTypeConstraint	= 0x0008,	// The generic parameter has the valuetype special constraint
			DefaultConstructorConstraint	= 0x0010,	// The generic parameter has the .ctor special constraint
	};
	uint16				number;
	compact<Flags,16>	flags;
	TypeOrMethodDef		owner;
	String				name;
	template<typename R> explicit ENTRY(R &r) : number(r), flags(r), owner(r), name(r) 	{}
};
template<> struct ENTRY<MethodSpec> {
	MethodDefOrRef		method;
	Signature			instantiation;
	template<typename R> explicit ENTRY(R &r) : method(r), instantiation(r)	{}
};
template<> struct ENTRY<GenericParamConstraint> {
	Indexed<GenericParam>	owner;
	TypeDefOrRef			constraint;
	template<typename R> explicit ENTRY(R &r) : owner(r), constraint(r) 	{}
};
template<> struct ENTRY<StringHeap> : const_memory_block {
	ENTRY(const void *_start, uint32 _size) : const_memory_block(_start, _size) {}
};

struct METADATA {
	memory_block					heaps[NumHeaps];
	struct {uint32 n; void *p; }	tables[NumTables];

	void*				GetHeap(HEAP h, uint32 offset)	const {
		return (char*)heaps[h] + offset;
	}

	void*				InitTables(TABLES *_tables) {
		uint32	*p	= _tables->Rows;
		for (uint64 b = _tables->Valid; b; b = clear_lowest(b))
			tables[lowest_set_index(b)].n = *p++;
		return p;
	}

	template<TABLETYPE I> range<ENTRY<I>*>			GetTable() {
		return make_range_n((ENTRY<I>*)tables[I].p, tables[I].n);
	}
	template<TABLETYPE I> range<const ENTRY<I>*>	GetTable() const {
		return make_range_n((const ENTRY<I>*)tables[I].p, tables[I].n);
	}
	template<TABLETYPE I>	bool	IsLast(const ENTRY<I> *const p) const {
		return p == (ENTRY<I>*)tables[I].p + tables[I].n - 1;
	}

	template<TABLETYPE I> ENTRY<I>*			GetEntry(int i)							{ return i ? ((ENTRY<I>*)tables[I].p) + (i - 1) : 0; }
	template<TABLETYPE I> ENTRY<I>*			GetEntry(const Indexed<I> &i)			{ return GetEntry<I>(i.t); }
	template<TABLETYPE I> const ENTRY<I>*	GetEntry(int i)					const	{ return i ? ((const ENTRY<I>*)tables[I].p) + (i - 1) : 0; }
	template<TABLETYPE I> const ENTRY<I>*	GetEntry(const Indexed<I> &i)	const	{ return GetEntry<I>(i.t); }

	template<TABLETYPE I, class S> range<ENTRY<I>*> GetEntries(const S *s, const IndexedList<I> &i0) {
		int	i1 = IsLast(s) ? tables[I].n + 1 : *(const IndexedList<I>*)((char*)&i0 + sizeof(S));
		return make_range_n(GetEntry<I>(i0), i1 - i0);
	}
	template<TABLETYPE I, class S> range<const ENTRY<I>*> GetEntries(const S *s, const IndexedList<I> &i0) const {
		int	i1 = IsLast(s) ? tables[I].n + 1 : *(const IndexedList<I>*)((char*)&i0 + sizeof(S));
		return make_range_n(GetEntry<I>(i0), i1 - i0);
	}

	template<TABLETYPE I> uint32 GetIndex(const ENTRY<I> *i) const {
		return i - GetTable<I>().begin() + 1;
	}
	template<TABLETYPE I> Indexed<I> GetIndexed(const ENTRY<I> *i) const {
		return GetIndex(i);
	}
	template<TABLETYPE I, TABLETYPE O> const ENTRY<O>* GetOwner(const ENTRY<I> *i, const IndexedList<I>(ENTRY<O>::*p)) const {
		return lower_boundc(GetTable<O>(), GetIndex(i) + 1, [p](const ENTRY<O> &o, int x) {
			return o.*p < x;
		}) - 1;
	}

	METADATA() {
		clear(heaps);
		clear(tables);
	}
	~METADATA() {
		for (auto &i : tables)
			iso::free(i.p);
	}
};
} // namespace clr

#include "stream.h"

namespace clr {

struct METADATA_READER : reader<METADATA_READER> {
	byte_reader	b;
	METADATA	*c;
	uint8		heapsizes;
	callback<iso::const_memory_block(uint32 rva, bool unmanaged)>	GetCode;

	METADATA_READER(const void *_p, METADATA *_c, uint8 _heapsizes) : b(_p), c(_c), heapsizes(_heapsizes) {}

	size_t				readbuff(void *buffer, size_t size)	{ return b.readbuff(buffer, size); }

	template<typename T> operator T()			{ return get<T>(); }
	uint32				GetOffset(bool big)		{ return big ? get<uint32>() : get<uint16>(); }
	void*				GetHeap(HEAP h)			{ return c->GetHeap(h, GetOffset(!!(heapsizes & (1 << h)))); }
	const char*			GetString()				{ return (const char*)GetHeap(HEAP_String); }
	operator			GUID()					{ return *(GUID*)GetHeap(HEAP_GUID); }
	uint32				GetIndex(int i)			{ return GetOffset(c->tables[i].n > 0xffff); }

	iso::const_memory_block	GetBlob() {
		void			*p = GetHeap(HEAP_Blob);
		byte_reader		r(p);
		CompressedUInt	u(r);
		return const_memory_block(r.p, u);
	}
	iso::const_memory_block	GetUserString() {
		void			*p = GetHeap(HEAP_UserString);
		byte_reader		r(p);
		CompressedUInt	u(r);
		return const_memory_block(r.p, u);
	}
//	iso::const_memory_block	GetCode(uint32 rva, bool unmanaged)	{
//		return empty;
//	}

	uint32				GetCodedIndex(int n, int b, const uint8 *trans) {
		uint32	thresh = 0xffff >> b;
		for (int i = 0; i < n; i++) {
			if (c->tables[trans[i]].n > thresh)
				return get<uint32>();
		}
		return get<uint16>();
	}

	template<TABLETYPE I>	void ReadTable() {
		int		n	= c->tables[I].n;
		ENTRY<I>	*p	= (ENTRY<I>*)(c->tables[I].p = iso::malloc(sizeof(ENTRY<I>) * n));
		for (int i = 0; i < n; i++)
			new(p + i) ENTRY<I>(*this);
	}
	void ReadTable(TABLETYPE t) {
		switch (t) {
			case Module:				ReadTable<Module				>(); break;
			case TypeRef:				ReadTable<TypeRef				>(); break;
			case TypeDef:				ReadTable<TypeDef				>(); break;
			case Field:					ReadTable<Field					>(); break;
			case MethodDef:				ReadTable<MethodDef				>(); break;
			case Param:					ReadTable<Param					>(); break;
			case InterfaceImpl:			ReadTable<InterfaceImpl			>(); break;
			case MemberRef:				ReadTable<MemberRef				>(); break;
			case Constant:				ReadTable<Constant				>(); break;
			case CustomAttribute:		ReadTable<CustomAttribute		>(); break;
			case FieldMarshal:			ReadTable<FieldMarshal			>(); break;
			case DeclSecurity:			ReadTable<DeclSecurity			>(); break;
			case ClassLayout:			ReadTable<ClassLayout			>(); break;
			case FieldLayout:			ReadTable<FieldLayout			>(); break;
			case StandAloneSig:			ReadTable<StandAloneSig			>(); break;
			case EventMap:				ReadTable<EventMap				>(); break;
			case Event:					ReadTable<Event					>(); break;
			case PropertyMap:			ReadTable<PropertyMap			>(); break;
			case Property:				ReadTable<Property				>(); break;
			case MethodSemantics:		ReadTable<MethodSemantics		>(); break;
			case MethodImpl:			ReadTable<MethodImpl			>(); break;
			case ModuleRef:				ReadTable<ModuleRef				>(); break;
			case TypeSpec:				ReadTable<TypeSpec				>(); break;
			case ImplMap:				ReadTable<ImplMap				>(); break;
			case FieldRVA:				ReadTable<FieldRVA				>(); break;
			case Assembly:				ReadTable<Assembly				>(); break;
			case AssemblyProcessor:		ReadTable<AssemblyProcessor		>(); break;
			case AssemblyOS:			ReadTable<AssemblyOS			>(); break;
			case AssemblyRef:			ReadTable<AssemblyRef			>(); break;
			case AssemblyRefProcessor:	ReadTable<AssemblyRefProcessor	>(); break;
			case AssemblyRefOS:			ReadTable<AssemblyRefOS			>(); break;
			case File:					ReadTable<File					>(); break;
			case ExportedType:			ReadTable<ExportedType			>(); break;
			case ManifestResource:		ReadTable<ManifestResource		>(); break;
			case NestedClass:			ReadTable<NestedClass			>(); break;
			case GenericParam:			ReadTable<GenericParam			>(); break;
			case MethodSpec:			ReadTable<MethodSpec			>(); break;
			case GenericParamConstraint:ReadTable<GenericParamConstraint>(); break;
		}
	}
};

} // namespace clr

namespace iso {
const char *to_string(clr::TABLETYPE t);
}
#endif	//CLR_H
