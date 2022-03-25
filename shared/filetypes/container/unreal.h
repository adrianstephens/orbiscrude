#include "base/defs.h"
#include "base/strings.h"
#include "stream.h"
#include "iso/iso.h"
#include "hashes/fnv.h"

namespace unreal {
using namespace iso;

enum EObjectFlags {
	RF_NoFlags					= 0x00000000,	///< No flags, used to avoid a cast

	// This first group of flags mostly has to do with what kind of object it is. Other than transient, these are the persistent object flags.
	// The garbage collector also tends to look at these.
	RF_Public					=0x00000001,	///< Object is visible outside its package.
	RF_Standalone				=0x00000002,	///< Keep object around for editing even if unreferenced.
	RF_MarkAsNative				=0x00000004,	///< Object (UField) will be marked as native on construction (DO NOT USE THIS FLAG in HasAnyFlags() etc)
	RF_Transactional			=0x00000008,	///< Object is transactional.
	RF_ClassDefaultObject		=0x00000010,	///< This object is its class's default object
	RF_ArchetypeObject			=0x00000020,	///< This object is a template for another object - treat like a class default object
	RF_Transient				=0x00000040,	///< Don't save object.

	// This group of flags is primarily concerned with garbage collection.
	RF_MarkAsRootSet			=0x00000080,	///< Object will be marked as root set on construction and not be garbage collected, even if unreferenced (DO NOT USE THIS FLAG in HasAnyFlags() etc)
	RF_TagGarbageTemp			=0x00000100,	///< This is a temp user flag for various utilities that need to use the garbage collector. The garbage collector itself does not interpret it.

	// The group of flags tracks the stages of the lifetime of a uobject
	RF_NeedInitialization		=0x00000200,	///< This object has not completed its initialization process. Cleared when ~FObjectInitializer completes
	RF_NeedLoad					=0x00000400,	///< During load, indicates object needs loading.
	RF_KeepForCooker			=0x00000800,	///< Keep this object during garbage collection because it's still being used by the cooker
	RF_NeedPostLoad				=0x00001000,	///< Object needs to be postloaded.
	RF_NeedPostLoadSubobjects	=0x00002000,	///< During load, indicates that the object still needs to instance subobjects and fixup serialized component references
	RF_NewerVersionExists		=0x00004000,	///< Object has been consigned to oblivion due to its owner package being reloaded, and a newer version currently exists
	RF_BeginDestroyed			=0x00008000,	///< BeginDestroy has been called on the object.
	RF_FinishDestroyed			=0x00010000,	///< FinishDestroy has been called on the object.

	// Misc. Flags
	RF_BeingRegenerated			=0x00020000,	///< Flagged on UObjects that are used to create UClasses (e.g. Blueprints) while they are regenerating their UClass on load (See FLinkerLoad::CreateExport())
	RF_DefaultSubObject			=0x00040000,	///< Flagged on subobjects that are defaults
	RF_WasLoaded				=0x00080000,	///< Flagged on UObjects that were loaded
	RF_TextExportTransient		=0x00100000,	///< Do not export object to text form (e.g. copy/paste). Generally used for sub-objects that can be regenerated from data in their parent object.
	RF_LoadCompleted			=0x00200000,	///< Object has been completely serialized by linkerload at least once. DO NOT USE THIS FLAG, It should be replaced with RF_WasLoaded.
	RF_InheritableComponentTemplate = 0x00400000, ///< Archetype of the object can be in its super class
	RF_DuplicateTransient		=0x00800000,	///< Object should not be included in any type of duplication (copy/paste, binary duplication, etc.)
	RF_StrongRefOnFrame			=0x01000000,	///< References to this object from persistent function frame are handled as strong ones.
	RF_NonPIEDuplicateTransient	=0x02000000,	///< Object should not be included for duplication unless it's being duplicated for a PIE session
	RF_Dynamic					=0x04000000,	///< Field Only. Dynamic field - doesn't get constructed during static initialization, can be constructed multiple times
	RF_WillBeLoaded				=0x08000000,	///< This object was constructed during load and will be loaded shortly

	RF_AllFlags					= 0x0fffffff,
	RF_Load						= RF_Public | RF_Standalone | RF_Transactional | RF_ClassDefaultObject | RF_ArchetypeObject | RF_DefaultSubObject | RF_TextExportTransient | RF_InheritableComponentTemplate | RF_DuplicateTransient | RF_NonPIEDuplicateTransient,
	RF_PropagateToSubObjects	= RF_Public | RF_ArchetypeObject | RF_Transactional | RF_Transient,
};

//-----------------------------------------------------------------------------
//	simple types
//-----------------------------------------------------------------------------

struct bool32 {
	bool	v;
	operator bool() const { return v; }
	bool read(istream_ref file) {
		auto	x = file.get<uint32>();
		v = !!x;
		return x < 2;
	}
};

struct FVector2D	{ float	X, Y; };
struct FVector		{ float	X, Y, Z; };
struct FVector4		{ float	X, Y, Z, W; };
struct FQuat		{ float	X, Y, Z, W; };
struct FIntPoint	{ int	X, Y; };
struct FIntVector	{ int	X, Y, Z; };
struct FColor		{ uint8	B, G, R, A; };
struct FLinearColor	{ float	R, G, B, A; };
struct FRotator		{ float Pitch, Yaw, Roll; };
struct FMatrix		{ FVector4	X, Y, Z, W; };

typedef FVector4	FPlane;

// note: no Raw serialiser
struct FTransform {
	FQuat	Rotation;
	FVector	Translation;
	FVector	Scale3D;
};

struct FBox {
	FVector Min;
	FVector Max;
	uint8 IsValid;
	bool read(istream_ref file) { return iso::read(file, Min, Max, IsValid); }
};

struct FSphere {
	FVector Center;
	float	W;
};

// note: no Raw serialiser
struct FBoxSphereBounds {
	FVector	Origin;
	FVector BoxExtent;
	float	SphereRadius;
};

struct FGuid {
	xint32 A, B, C, D;
	FGuid() : A(0), B(0), C(0), D(0) {}
	FGuid(uint32 A, uint32 B, uint32 C, uint32 D) : A(A), B(B), C(C), D(D) {}

	friend bool operator==(const FGuid& X, const FGuid& Y) {
		return X.A == Y.A && X.B == Y.B && X.C == Y.C && X.D == Y.D;
	}
	friend bool operator!=(const FGuid& X, const FGuid& Y) {
		return !(X == Y);
	}
	friend bool operator<(const FGuid& X, const FGuid& Y) {
		return	(X.A < Y.A || !(X.A > Y.A)
			&&	(X.B < Y.B || !(X.B > Y.B)
			&&	(X.C < Y.C || !(X.C > Y.C)
			&&	(X.D < Y.D || !(X.D > Y.D)
		))));
	}
};

//-----------------------------------------------------------------------------
//	strings
//-----------------------------------------------------------------------------

class FString : comparisons<FString> {
	char *p;	// using char so debugger will show something

	static char* make(const char *s, const char *e)	{ size_t len = e - s; char *p = (char*)iso::malloc(len + 2) + 1; memcpy(p, s, len); p[len] = 0; return p; }
	static char* make(const char *s)				{ size_t len = string_len(s) + 1; char *p = (char*)iso::malloc(len + 1) + 1; memcpy(p, s, len); return p; }
	static char* make(const char16 *s)				{ size_t len = string_len(s) + 1; char *p = (char*)iso::malloc(len * 2); memcpy(p, s, len * 2); return p; }

	void	clear()					{ if (p) iso::free(align_down(p, 2)); p = nullptr; }
public:
	FString()						: p(0) {}
	FString(FString &&s)			: p(exchange(s.p, nullptr))	{}
	FString(const char *s)			: p(s ? make(s) : nullptr) {}
	FString(const char *s, const char *e)	: p(make(s, e)) {}
	FString(const count_string &s)	: p(make(s.begin(), s.end())) {}
	FString(const char16 *s)		: p(s ? make(s) : nullptr) {}
	FString(const FString &s)		: p(!s ? nullptr : s.is_wide() ? make((char16*)s.p) : make((char*)s.p)) {}
	~FString()						{ clear(); }

	FString& operator=(FString &&s)	{ swap(p, s.p); return *this; }
	explicit operator bool() const	{ return !!p; }
	explicit operator const char*() const	{ return is_wide() ? 0 : (const char*)p; }
//	operator tag2()	const			{ return operator const char*(); }

	friend const char*	get(const FString &s)	{ return (const char*)s; }

	bool	is_wide()	const		{ return p && !(intptr_t(p) & 1); }
	size_t	length()	const		{ return is_wide() ? string_len((char16*)p) : string_len((char*)p); }

	template<typename T> friend int	compare(const FString &s1, const T &s2) {
		return	!s1	? (!s2 ? 0 : -1)
			:	!s2	? 1
			:	s1.is_wide() ? -s2.compare_to((char16*)s1.p)
			:	-s2.compare_to((char*)s1.p);
	}

	template<typename C> int	compare_to(const C *s) const {
		return is_wide()
			? -str(s).compare_to((char16*)p)
			: -str(s).compare_to((char*)p);
	}
	bool read(istream_ref file) {
		clear();
		int len;
		if (!file.read(len))
			return false;
		if (len < 0) {
			p = (char*)iso::malloc(-len * 2);
			return check_readbuff(file, p, -len * 2);
		} else if (len > 0) {
			p = (char*)iso::malloc(len + 1) + 1;
			return check_readbuff(file, p, len);
		}
		return true;
	}
	friend uint64	hash(const FString &s) {
		return s.is_wide()
			? FNV1<64>((char16*)s.p, string_len((char16*)s.p))
			: FNV1<64>((char*)s.p);
	}
	friend tag2	_GetName(const FString &s)		{ return (const char*)s; }
	friend string_accum& operator<<(string_accum& sa, const FString& s) { return sa << get(s); }
};


typedef FString FText;

struct FNameEntryHeader {
	uint16 bIsWide : 1;
#if WITH_CASE_PRESERVING_NAME
	uint16 Len : 15;
#else
	uint16 LowercaseProbeHash:5, Len:10;
#endif
};

struct FNameEntry : FNameEntryHeader {
	enum { NAME_SIZE = 1024 };
	union {
		char   AnsiName[NAME_SIZE];
		char16 WideName[NAME_SIZE];
	};
};

struct FName {
	uint32	ComparisonIndex;
	uint32	Number;
	FName(uint32 ComparisonIndex = ~0, uint32 Number = ~0) : ComparisonIndex(ComparisonIndex), Number(Number) {}
	explicit constexpr operator bool() const { return ~ComparisonIndex; }
	bool operator==(const FName &b)	const { return ComparisonIndex == b.ComparisonIndex && Number == b.Number; }
	bool operator!=(const FName &b)	const { return !(*this == b); }
	bool operator<(const FName &b)	const { return ComparisonIndex < b.ComparisonIndex ||  (ComparisonIndex == b.ComparisonIndex && Number == b.Number); }
};


//-----------------------------------------------------------------------------
//	loading
//-----------------------------------------------------------------------------

struct FPackageIndex {
	int32			Index;

	FPackageIndex() : Index(0) {}
	FPackageIndex(int32 Index, bool import) : Index(import ? -Index - 1 : Index + 1) {}
	bool operator<(FPackageIndex b) const { return Index < b.Index; }

	bool	IsImport()	const	{ return Index < 0; }
	bool	IsExport()	const	{ return Index > 0; }
	bool	IsNull()	const	{ return Index == 0; }
	int32	ToImport()	const	{ ISO_ASSERT(IsImport()); return -Index - 1; }
	int32	ToExport()	const	{ ISO_ASSERT(IsExport()); return Index - 1; }
};

struct FObjectResource {
	FName			ObjectName;
	FPackageIndex	OuterIndex;
	ISO_ptr<ISO_ptr<void>>	p;
};

struct FLinkerTables;

struct istream_linker : reader<istream_linker>, reader_ref<istream_ref> {
	istream_ref		bulk_file;
	FLinkerTables	*linker;
	istream_linker(istream_ref file, istream_ref bulk_file, FLinkerTables *linker) : reader_ref<istream_ref>(file), bulk_file(bulk_file), linker(linker) {}

	int64					bulk_data();
	cstring					lookup(const FName& name) const;
	const FObjectResource	*lookup(FPackageIndex index) const;
};

struct FName2 : FString {
	template<typename R> bool read(R& file) {
		*(FString*)this = FString(file.lookup(file.template get<FName>()));
		return true;
	}
	friend tag2	_GetName(const FName2 &s)		{ return (const char*)s; }
};

struct FSoftObjectPath {
	FName2	AssetPathName;
	FString	SubPathString;
	bool	read(istream_linker& file) {
		return iso::read(file, AssetPathName, SubPathString);
	}
};


//-----------------------------------------------------------------------------
//	templated types
//-----------------------------------------------------------------------------

template<typename T> class TArray : public dynamic_array<T> {
public:
	using dynamic_array<T>::dynamic_array;
	using dynamic_array<T>::read;
	template<typename R> bool read(R& file) {
		return dynamic_array<T>::read(file, file.template get<int>());
	}
	//	bool read(istream_linker &file) {
	//		return dynamic_array<T>::read(file, file.get<int>());
	//	}
};

template<typename T> struct TBulkArray : TArray<T> {
	template<typename R> enable_if_t<has_read<R, T>, bool> read(R file) {
		return TArray<T>::read(file);
	}
	template<typename R> enable_if_t<!has_read<R, T>, bool> read(R file) {
		auto	size = file.template get<uint32>();
		ISO_ASSERT(size == sizeof(T));
		return TArray<T>::read(file);
	}
};

struct TBitArray : dynamic_bitarray<uint32> {
	bool read(istream_ref file) {
		using iso::read;
		resize(file.get<uint32>());
		return read(file, raw());
	}
};

template<typename T> struct TSparseArray : sparse_array<T, uint32, uint32> {
	typedef	sparse_array<T, uint32, uint32>	B;
	T&	operator[](int i) const	{ return B::get(i); }
	bool read(istream_ref file) {
		TBitArray AllocatedIndices;
		file.read(AllocatedIndices);

		resize(AllocatedIndices.size());
		for (auto i : AllocatedIndices.where(true))
			file.read(B::put(i));
		return true;
	}
};

template<typename K, typename V> struct TMap : public map<K, V> {
	template<typename R> bool read(R&& file) {
		int		len = file.template get<int>();
		while (len-- && !file.eof()) {
			K	k = file.template get<K>();
			V	v = file.template get<V>();
			put(move(k), move(v));
		}
		return true;
	}
};

template<typename K> struct TSet : public set<K> {
	template<typename R> bool read(R&& file) {
		int		len = file.template get<int>();
		while (len-- && !file.eof())
			insert(file.template get<K>());
		return true;
	}
};

struct _Ptr : ISO_ptr<void> {
	bool read(istream_linker& file) {
		auto	i	= file.get<FPackageIndex>();
		if (auto obj = file.lookup(i))
			*((ISO_ptr<void>*)this) = obj->p;
		return true;
	}
};

template<typename T> struct TPtr : _Ptr {};

struct _InlinePtr : ISO_ptr<void> {
	bool read(istream_linker& file);
};

template<typename T> struct TInlinePtr : _InlinePtr {};

template<typename T> struct TKnownPtr : ISO_ptr<T> {
	bool read(istream_linker& file) {
		return file.read(*Create(0));
	}
	template<typename T2> void operator=(const ISO_ptr<T2>& p) {
		ISO_ptr<T>::operator=(p);
	}
};

template<typename T> struct TSoftPtr : FSoftObjectPath {};

//-----------------------------------------------------------------------------
//	properties
//-----------------------------------------------------------------------------

struct UClass;
struct UStruct;
struct UEnum;
struct UFunction;

struct FField {
	FName2					NamePrivate;
	EObjectFlags			FlagsPrivate;
	TMap<FName2, FString>	MetaDataMap;

	bool read(istream_linker& file) {
		if (!file.read(NamePrivate) || !file.read(FlagsPrivate))
			return false;
		if (file.get<bool32>())
			return MetaDataMap.read(file);
		return true;
	}
};

struct FProperty : FField {
	enum EPropertyFlags : uint64 {
		CPF_None = 0,

		CPF_Edit							= 0x0000000000000001,	///< Property is user-settable in the editor.
		CPF_ConstParm						= 0x0000000000000002,	///< This is a constant function parameter
		CPF_BlueprintVisible				= 0x0000000000000004,	///< This property can be read by blueprint code
		CPF_ExportObject					= 0x0000000000000008,	///< Object can be exported with actor.
		CPF_BlueprintReadOnly				= 0x0000000000000010,	///< This property cannot be modified by blueprint code
		CPF_Net								= 0x0000000000000020,	///< Property is relevant to network replication.
		CPF_EditFixedSize					= 0x0000000000000040,	///< Indicates that elements of an array can be modified, but its size cannot be changed.
		CPF_Parm							= 0x0000000000000080,	///< Function/When call parameter.
		CPF_OutParm							= 0x0000000000000100,	///< Value is copied out after function call.
		CPF_ZeroConstructor					= 0x0000000000000200,	///< memset is fine for construction
		CPF_ReturnParm						= 0x0000000000000400,	///< Return value.
		CPF_DisableEditOnTemplate			= 0x0000000000000800,	///< Disable editing of this property on an archetype/sub-blueprint
		//CPF_      						= 0x0000000000001000,	///< 
		CPF_Transient   					= 0x0000000000002000,	///< Property is transient: shouldn't be saved or loaded, except for Blueprint CDOs.
		CPF_Config      					= 0x0000000000004000,	///< Property should be loaded/saved as permanent profile.
		//CPF_								= 0x0000000000008000,	///< 
		CPF_DisableEditOnInstance			= 0x0000000000010000,	///< Disable editing on an instance of this class
		CPF_EditConst   					= 0x0000000000020000,	///< Property is uneditable in the editor.
		CPF_GlobalConfig					= 0x0000000000040000,	///< Load config from base class, not subclass.
		CPF_InstancedReference				= 0x0000000000080000,	///< Property is a component references.
		//CPF_								= 0x0000000000100000,	///<
		CPF_DuplicateTransient				= 0x0000000000200000,	///< Property should always be reset to the default value during any type of duplication (copy/paste, binary duplication, etc.)
		CPF_SubobjectReference				= 0x0000000000400000,	///< Property contains subobject references (TSubobjectPtr)
		//CPF_    							= 0x0000000000800000,	///< 
		CPF_SaveGame						= 0x0000000001000000,	///< Property should be serialized for save games, this is only checked for game-specific archives with ArIsSaveGame
		CPF_NoClear							= 0x0000000002000000,	///< Hide clear (and browse) button.
		//CPF_  							= 0x0000000004000000,	///<
		CPF_ReferenceParm					= 0x0000000008000000,	///< Value is passed by reference; CPF_OutParam and CPF_Param should also be set.
		CPF_BlueprintAssignable				= 0x0000000010000000,	///< MC Delegates only.  Property should be exposed for assigning in blueprint code
		CPF_Deprecated  					= 0x0000000020000000,	///< Property is deprecated.  Read it from an archive, but don't save it.
		CPF_IsPlainOldData					= 0x0000000040000000,	///< If this is set, then the property can be memcopied instead of CopyCompleteValue / CopySingleValue
		CPF_RepSkip							= 0x0000000080000000,	///< Not replicated. For non replicated properties in replicated structs 
		CPF_RepNotify						= 0x0000000100000000,	///< Notify actors when a property is replicated
		CPF_Interp							= 0x0000000200000000,	///< interpolatable property for use with matinee
		CPF_NonTransactional				= 0x0000000400000000,	///< Property isn't transacted
		CPF_EditorOnly						= 0x0000000800000000,	///< Property should only be loaded in the editor
		CPF_NoDestructor					= 0x0000001000000000,	///< No destructor
		//CPF_								= 0x0000002000000000,	///<
		CPF_AutoWeak						= 0x0000004000000000,	///< Only used for weak pointers, means the export type is autoweak
		CPF_ContainsInstancedReference		= 0x0000008000000000,	///< Property contains component references.
		CPF_AssetRegistrySearchable			= 0x0000010000000000,	///< asset instances will add properties with this flag to the asset registry automatically
		CPF_SimpleDisplay					= 0x0000020000000000,	///< The property is visible by default in the editor details view
		CPF_AdvancedDisplay					= 0x0000040000000000,	///< The property is advanced and not visible by default in the editor details view
		CPF_Protected						= 0x0000080000000000,	///< property is protected from the perspective of script
		CPF_BlueprintCallable				= 0x0000100000000000,	///< MC Delegates only.  Property should be exposed for calling in blueprint code
		CPF_BlueprintAuthorityOnly			= 0x0000200000000000,	///< MC Delegates only.  This delegate accepts (only in blueprint) only events with BlueprintAuthorityOnly.
		CPF_TextExportTransient				= 0x0000400000000000,	///< Property shouldn't be exported to text format (e.g. copy/paste)
		CPF_NonPIEDuplicateTransient		= 0x0000800000000000,	///< Property should only be copied in PIE
		CPF_ExposeOnSpawn					= 0x0001000000000000,	///< Property is exposed on spawn
		CPF_PersistentInstance				= 0x0002000000000000,	///< A object referenced by the property is duplicated like a component. (Each actor should have an own instance.)
		CPF_UObjectWrapper					= 0x0004000000000000,	///< Property was parsed as a wrapper class like TSubclassOf<T>, FScriptInterface etc., rather than a USomething*
		CPF_HasGetValueTypeHash				= 0x0008000000000000,	///< This property can generate a meaningful hash value.
		CPF_NativeAccessSpecifierPublic		= 0x0010000000000000,	///< Public native access specifier
		CPF_NativeAccessSpecifierProtected	= 0x0020000000000000,	///< Protected native access specifier
		CPF_NativeAccessSpecifierPrivate	= 0x0040000000000000,	///< Private native access specifier
		CPF_SkipSerialization				= 0x0080000000000000,	///< Property shouldn't be serialized, can still be exported to text

		CPF_NativeAccessSpecifiers			= CPF_NativeAccessSpecifierPublic | CPF_NativeAccessSpecifierProtected | CPF_NativeAccessSpecifierPrivate,	/** All Native Access Specifier flags */
		CPF_ParmFlags						= CPF_Parm | CPF_OutParm | CPF_ReturnParm | CPF_ReferenceParm | CPF_ConstParm,	/** All parameter flags */

															/** Flags that are propagated to properties inside containers */
		CPF_PropagateToArrayInner			= CPF_ExportObject | CPF_PersistentInstance | CPF_InstancedReference | CPF_ContainsInstancedReference | CPF_Config | CPF_EditConst | CPF_Deprecated | CPF_EditorOnly | CPF_AutoWeak | CPF_UObjectWrapper,
		CPF_PropagateToMapValue				= CPF_ExportObject | CPF_PersistentInstance | CPF_InstancedReference | CPF_ContainsInstancedReference | CPF_Config | CPF_EditConst | CPF_Deprecated | CPF_EditorOnly | CPF_AutoWeak | CPF_UObjectWrapper | CPF_Edit,
		CPF_PropagateToMapKey				= CPF_ExportObject | CPF_PersistentInstance | CPF_InstancedReference | CPF_ContainsInstancedReference | CPF_Config | CPF_EditConst | CPF_Deprecated | CPF_EditorOnly | CPF_AutoWeak | CPF_UObjectWrapper | CPF_Edit,
		CPF_PropagateToSetElement			= CPF_ExportObject | CPF_PersistentInstance | CPF_InstancedReference | CPF_ContainsInstancedReference | CPF_Config | CPF_EditConst | CPF_Deprecated | CPF_EditorOnly | CPF_AutoWeak | CPF_UObjectWrapper | CPF_Edit,
	
		CPF_InterfaceClearMask				= CPF_ExportObject|CPF_InstancedReference|CPF_ContainsInstancedReference,	/** The flags that should never be set on interface properties */
		CPF_DevelopmentAssets				= CPF_EditorOnly,	/** All the properties that can be stripped for final release console builds */
		CPF_ComputedFlags					= CPF_IsPlainOldData | CPF_NoDestructor | CPF_ZeroConstructor | CPF_HasGetValueTypeHash,	/** All the properties that should never be loaded or saved */
		CPF_AllFlags						= 0xFFFFFFFFFFFFFFFF,	/** Mask of all property flags */
	};
	enum ELifetimeCondition : uint8 {
		COND_None							= 0,
		COND_InitialOnly					= 1,
		COND_OwnerOnly						= 2,
		COND_SkipOwner						= 3,
		COND_SimulatedOnly					= 4,
		COND_AutonomousOnly					= 5,
		COND_SimulatedOrPhysics				= 6,
		COND_InitialOrOwner					= 7,
		COND_Custom							= 8,
		COND_ReplayOrOwner					= 9,
		COND_ReplayOnly						= 10,	
		COND_SimulatedOnlyNoReplay			= 11,
		COND_SimulatedOrPhysicsNoReplay		= 12,
		COND_SkipReplay						= 13,	
		COND_Never							= 15,			
		COND_Max							= 16,			
	};

	int32			ArrayDim;
	int32			ElementSize;
	EPropertyFlags	PropertyFlags;
	uint16			RepIndex;
	FName2			RepNotifyFunc;
	ELifetimeCondition BlueprintReplicationCondition;

	uint32			GetOffset_ForDebug() const { return 0; }

	bool read(istream_linker& file) {
		return FField::read(file)
			&& iso::read(file, ArrayDim, ElementSize, PropertyFlags, RepIndex, RepNotifyFunc, BlueprintReplicationCondition);
	}
};

struct FBoolProperty : FProperty {
	uint8	FieldSize;
	uint8	ByteOffset;
	uint8	ByteMask;
	uint8	FieldMask;

	bool read(istream_linker& file) {
		uint8 BoolSize, NativeBool;
		return FProperty::read(file) && iso::read(file, FieldSize, ByteOffset, ByteMask, FieldMask, BoolSize, NativeBool);
	}
};

template<typename T> struct TProperty_Numeric : FProperty {};
typedef TProperty_Numeric<int8>		FInt8Property;
typedef TProperty_Numeric<int16>	FInt16Property;
typedef TProperty_Numeric<int>		FIntProperty;
typedef TProperty_Numeric<int64>	FInt64Property;
typedef TProperty_Numeric<uint16>	FUInt16Property;
typedef TProperty_Numeric<uint32>	FUInt32Property;
typedef TProperty_Numeric<uint64>	FUInt64Property;
typedef TProperty_Numeric<float>	FFloatProperty;
typedef TProperty_Numeric<double>	FDoubleProperty;

struct FByteProperty : TProperty_Numeric<uint8> {
	uint32	unknown;
	bool read(istream_linker& file) {
		return FProperty::read(file) && file.read(unknown);
	}
};


struct FEnumProperty	: FProperty {
	TInlinePtr<FProperty>	UnderlyingProp;	// The property which represents the underlying type of the enum
	TPtr<UEnum>				Enum;			// The enum represented by this property
	bool read(istream_linker& file) {
		return FProperty::read(file) && file.read(Enum) && file.read(UnderlyingProp);
	}
};

typedef FProperty	FStrProperty;
typedef FProperty	FNameProperty;
typedef FProperty	FTextProperty;
typedef FProperty	FSoftObjectProperty;

struct FArrayProperty	: FProperty {
	TInlinePtr<FProperty>	Inner;
	bool read(istream_linker& file) {
		return FProperty::read(file) && file.read(Inner);
	}
};

struct FSetProperty		: FProperty {
	TInlinePtr<FProperty>	ElementProp;
	bool read(istream_linker& file) {
		return FProperty::read(file) && file.read(ElementProp);
	}
};

struct FMapProperty		: FProperty {
	TInlinePtr<FProperty>	KeyProp;
	TInlinePtr<FProperty>	ValueProp;
	bool read(istream_linker& file) {
		return FProperty::read(file) && file.read(KeyProp) && file.read(ValueProp);
	}
};

struct FStructProperty : FProperty {
	TPtr<UStruct>			Struct;
	bool read(istream_linker& file) {
		return FProperty::read(file) && file.read(Struct);
	}
};

struct FObjectProperty : FProperty {
	TPtr<UClass>			PropertyClass;
	bool read(istream_linker& file) {
		return FProperty::read(file) && file.read(PropertyClass);
	}
};

struct FClassProperty : FObjectProperty {
	TPtr<UClass>			MetaClass;
	bool read(istream_linker& file) {
		return FObjectProperty::read(file) && file.read(MetaClass);
	}
};


struct FDelegateProperty : FProperty {
	TPtr<UFunction>			SignatureFunction;
	bool read(istream_linker& file) {
		return FProperty::read(file) && file.read(SignatureFunction);
	}
};

typedef FDelegateProperty FMulticastSparseDelegateProperty;
typedef FDelegateProperty FMulticastInlineDelegateProperty;

struct FFieldPathProperty : FProperty {
	TPtr<FName> PropertyClass;
	bool read(istream_linker& file) {
		return FProperty::read(file) && file.read(PropertyClass);
	}
};

struct FInterfaceProperty : FProperty {
	TPtr<UClass>	InterfaceClass;
	bool read(istream_linker& file) {
		return FProperty::read(file) && file.read(InterfaceClass);
	}
};


//-----------------------------------------------------------------------------
//	objects
//-----------------------------------------------------------------------------

struct UObject : anything {
	bool	read(istream_linker& file);
};

struct UStruct : UObject {
	TPtr<UObject>			Super;
	TArray<TPtr<UObject>>	Children;
	TArray<TInlinePtr<FProperty>>	ChildProperties;
	malloc_block			Script;

	const UStruct*	GetOuter() const { return this; }

	bool read(istream_linker& file);
};


struct FImplementedInterface {
	TPtr<UClass>	Class;
	int32			PointerOffset;
	bool32			bImplementedByK2;
	bool read(istream_linker& file);
};

struct UClass : UStruct {
	enum EClassFlags {
		CLASS_None						= 0x00000000u,
		CLASS_Abstract					= 0x00000001u,
		CLASS_DefaultConfig				= 0x00000002u,
		CLASS_Config					= 0x00000004u,
		CLASS_Transient					= 0x00000008u,
		CLASS_Parsed					= 0x00000010u,
		CLASS_MatchedSerializers		= 0x00000020u,
		CLASS_AdvancedDisplay			= 0x00000040u,
		CLASS_Native					= 0x00000080u,
		CLASS_NoExport					= 0x00000100u,
		CLASS_NotPlaceable				= 0x00000200u,
		CLASS_PerObjectConfig			= 0x00000400u,
		CLASS_ReplicationDataIsSetUp	= 0x00000800u,
		CLASS_EditInlineNew				= 0x00001000u,
		CLASS_CollapseCategories		= 0x00002000u,
		CLASS_Interface					= 0x00004000u,
		CLASS_CustomConstructor			= 0x00008000u,
		CLASS_Const						= 0x00010000u,
		CLASS_LayoutChanging			= 0x00020000u,
		CLASS_CompiledFromBlueprint		= 0x00040000u,
		CLASS_MinimalAPI				= 0x00080000u,
		CLASS_RequiredAPI				= 0x00100000u,
		CLASS_DefaultToInstanced		= 0x00200000u,
		CLASS_TokenStreamAssembled		= 0x00400000u,
		CLASS_HasInstancedReference		= 0x00800000u,
		CLASS_Hidden					= 0x01000000u,
		CLASS_Deprecated				= 0x02000000u,
		CLASS_HideDropDown				= 0x04000000u,
		CLASS_GlobalUserConfig			= 0x08000000u,
		CLASS_Intrinsic					= 0x10000000u,
		CLASS_Constructed				= 0x20000000u,
		CLASS_ConfigDoNotCheckDefaults	= 0x40000000u,
		CLASS_NewerVersionExists		= 0x80000000u,
	};
	TMap<FName2, TPtr<UFunction>>	FuncMap;
	EClassFlags						ClassFlags;
	TPtr<UClass>					ClassWithin;
	TPtr<UObject>					ClassGeneratedBy;
	FName2							ClassConfigName;
	TArray<FImplementedInterface>	Interfaces;
	TPtr<UObject>					CDO;

	bool read(istream_linker& file);
};

} // namespace unreal