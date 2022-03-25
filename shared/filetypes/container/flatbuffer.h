#include "base/strings.h"

namespace flatbuffers {
using namespace iso;

#define FLATBUFFERS_LITTLEENDIAN 1

#define FLATBUFFERS_VERSION_MAJOR 1
#define FLATBUFFERS_VERSION_MINOR 10
#define FLATBUFFERS_VERSION_REVISION 0
#define FLATBUFFERS_STRING_EXPAND(X) #X
#define FLATBUFFERS_STRING(X) FLATBUFFERS_STRING_EXPAND(X)


typedef uint32 uoffset_t;
typedef int32  soffset_t;  // Signed offsets for references that can go in both directions.
typedef uint16 voffset_t;  // Offset/index used in v-tables, can be changed to uint8 in format forks to save a bit of space if desired.
typedef uint64 largest_scalar_t;

#define FLATBUFFERS_MAX_BUFFER_SIZE ((1ULL << (sizeof(soffset_t) * 8 - 1)) - 1)
#define FLATBUFFERS_MAX_ALIGNMENT 16

template<typename T> T EndianSwap(T t) { return endian_swap(t); }
#if FLATBUFFERS_LITTLEENDIAN
template<typename T> T EndianScalar(T t) { return t; }
#else
template<typename T> T EndianScalar(T t) { return EndianSwap(t); }
#endif

template<typename T> T		ReadScalar(const void* p)	{ return EndianScalar(*reinterpret_cast<const T*>(p)); }
template<typename T> void	WriteScalar(void* p, T t)	{ *reinterpret_cast<T*>(p) = EndianScalar(t); }

// Convert an integer or floating point value to a string
template<typename T> string NumToString(T t)	{ return to_string(t); }
// Avoid char types used as character data.
inline string NumToString(signed char t)		{ return NumToString(static_cast<int>(t)); }
inline string NumToString(unsigned char t)		{ return NumToString(static_cast<int>(t)); }

// Wrapper for uoffset_t to allow safe template specialization
template<typename T> struct Offset {
	uoffset_t o;
	operator T*()	const { return (T*)((const uint8*)this + o); }
	T* operator->() const { return *this; }
};

inline void EndianCheck() {
	int endiantest = 1;
	// If this fails, see FLATBUFFERS_LITTLEENDIAN above.
	ISO_ASSERT(*reinterpret_cast<char*>(&endiantest) == FLATBUFFERS_LITTLEENDIAN);
	(void)endiantest;
}

struct String;

template<typename T> class Vector {
	Vector(const Vector&) = delete;

protected:
	uoffset_t	length;

public:
	typedef T*			iterator;
	typedef const T*	const_iterator;

	// The raw data in little endian format. Use with care.
	const uint8*	Data() const	{ return reinterpret_cast<const uint8*>(&length + 1); }
	uint8*			Data()			{ return reinterpret_cast<uint8*>(&length + 1); }
	const T*		data() const	{ return reinterpret_cast<const T*>(Data()); }
	T*				data()			{ return reinterpret_cast<T*>(Data()); }

	const_memory_block	block() const	{ return const_memory_block(&length + 1, length * sizeof(T)); }
	memory_block		block()			{ return memory_block(&length + 1, length * sizeof(T)); }

	const T&	Get(uoffset_t i) const {
		ISO_ASSERT(i < size());
		return data()[i];
	}

	const T&	operator[](uoffset_t i) const { return Get(i); }

	uoffset_t	size()	const	{ return EndianScalar(length); }

	auto		begin()			{ return data(); }
	auto		begin() const	{ return data(); }

	auto		end()			{ return data() + size(); }
	auto		end()	const	{ return data() + size(); }
};


// Represent a vector much like the template above, but in this case we don't know what the element types are (used with reflection.h).
class VectorOfAny {
	VectorOfAny(const VectorOfAny&) = delete;
protected:
	uoffset_t	length;
public:
	uoffset_t	size() const	{ return EndianScalar(length); }
	const uint8* Data() const	{ return reinterpret_cast<const uint8*>(&length + 1); }
	uint8*		 Data()			{ return reinterpret_cast<uint8*>(&length + 1); }
};

template<typename T, typename U> Vector<Offset<T>>* VectorCast(Vector<Offset<U>>* ptr) {
	static_assert(T_is_base_of<T, U>::value, "Unrelated types");
	return reinterpret_cast<Vector<Offset<T>>*>(ptr);
}

template<typename T, typename U> const Vector<Offset<T>>* VectorCast(const Vector<Offset<U>>* ptr) {
	static_assert(T_is_base_of<T, U>::value, "Unrelated types");
	return reinterpret_cast<const Vector<Offset<T>>*>(ptr);
}

// Convenient helper function to get the length of any vector, regardless of whether it is null or not (the field is not set).
template<typename T> static inline size_t VectorLength(const Vector<T>* v) { return v ? v->Length() : 0; }

// Lexicographically compare two strings (possibly containing nulls), and return true if the first is less than the second.
static inline bool StringLessThan(const char* a_data, uoffset_t a_size, const char* b_data, uoffset_t b_size) {
	const auto cmp = memcmp(a_data, b_data, min(a_size, b_size));
	return cmp == 0 ? a_size < b_size : cmp < 0;
}

struct String : public Vector<char> {
	operator const char*() const { return reinterpret_cast<const char*>(Data()); }
	bool operator<(const String& o) const { return StringLessThan(this->data(), this->size(), o.data(), o.size()); }
};

//-----------------------------------------------------------------------------
//	Verifier - Helper class to verify the integrity of a FlatBuffer
//-----------------------------------------------------------------------------

class Verifier {
	const uint8*	buf;
	size_t			size;
	uoffset_t		depth;
	uoffset_t		max_depth;
	uoffset_t		num_tables;
	uoffset_t		max_tables;
	bool			check_alignment;
#ifdef FLATBUFFERS_TRACK_VERIFIER_BUFFER_SIZE
	mutable size_t	upper_bound;
#endif

public:
	Verifier(const uint8* buf, size_t buf_len, uoffset_t max_depth = 64, uoffset_t max_tables = 1000000, bool check_alignment = true)
		: buf(buf), size(buf_len), depth(0), max_depth(max_depth), num_tables(0), max_tables(max_tables),  check_alignment(check_alignment)
#ifdef FLATBUFFERS_TRACK_VERIFIER_BUFFER_SIZE
		, upper_bound(0)
#endif
	{
		ISO_ASSERT(size < FLATBUFFERS_MAX_BUFFER_SIZE);
	}

	// Central location where any verification failures register
	bool Check(bool ok) const {
#ifdef FLATBUFFERS_DEBUG_VERIFICATION_FAILURE
		ISO_ASSERT(ok);
#endif
#ifdef FLATBUFFERS_TRACK_VERIFIER_BUFFER_SIZE
		if (!ok)
			upper_bound = 0;
#endif
		return ok;
	}

	// Verify any range within the buffer.
	bool Verify(size_t elem, size_t elem_len) const {
#ifdef FLATBUFFERS_TRACK_VERIFIER_BUFFER_SIZE
		upper_bound = max(upper_bound, elem + elem_len);
#endif
		return Check(elem_len < size && elem <= size - elem_len);
	}

	template<typename T> bool VerifyAlignment(size_t elem)					const	{ return (elem & (sizeof(T) - 1)) == 0 || !check_alignment; }
	template<typename T> bool Verify(size_t elem)							const	{ return VerifyAlignment<T>(elem) && Verify(elem, sizeof(T)); }
	bool Verify(const uint8* base, voffset_t elem_off, size_t elem_len)		const	{ return Verify(base - buf + elem_off, elem_len); }
	template<typename T> bool Verify(const uint8* base, voffset_t elem_off) const	{ return Verify(base - buf + elem_off, sizeof(T)); }
	template<typename T> bool VerifyVector(const Vector<T>* vec)			const	{ return !vec || VerifyVectorOrString(vec, sizeof(T)); }

	template<typename T> bool VerifyTable(const T* table)							{ return !table || table->Verify(*this); }

	// Verify a pointer (may be NULL) to string.
	bool VerifyString(const String* str) const {
		size_t end;
		return !str
			   || (VerifyVectorOrString(str, 1, &end) && Verify(end, 1) &&  // Must have terminator
				   Check(buf[end] == '\0'));								// Terminating byte must be 0.
	}

	// Common code between vectors and strings.
	bool VerifyVectorOrString(const void* vec, size_t elem_size, size_t* end = nullptr) const {
		auto veco = (uint8*)vec - buf;
		if (!Verify<uoffset_t>(veco))
			return false;

		// Check the whole array. If this is a string, the byte past the array must be 0
		auto size	  = ReadScalar<uoffset_t>(vec);
		auto max_elems = FLATBUFFERS_MAX_BUFFER_SIZE / elem_size;
		if (!Check(size < max_elems))
			return false;  // Protect against byte_size overflowing.
		auto byte_size = sizeof(size) + elem_size * size;
		if (end)
			*end = veco + byte_size;
		return Verify(veco, byte_size);
	}

	// Special case for string contents, after the above has been called.
	bool VerifyVectorOfStrings(const Vector<Offset<String>>* vec) const {
		if (vec) {
			for (uoffset_t i = 0; i < vec->size(); i++) {
				if (!VerifyString(vec->Get(i)))
					return false;
			}
		}
		return true;
	}

	// Special case for table contents, after the above has been called.
	template<typename T> bool VerifyVectorOfTables(const Vector<Offset<T>>* vec) {
		if (vec) {
			for (uoffset_t i = 0; i < vec->size(); i++) {
				if (!vec->Get(i)->Verify(*this))
					return false;
			}
		}
		return true;
	}

	bool VerifyTableStart(const uint8* table) {
		auto tableo = static_cast<size_t>(table - buf);
		if (!Verify<soffset_t>(tableo))
			return false;

		// This offset may be signed, but doing the sliceaction unsigned always gives the result we want
		auto vtableo = tableo - static_cast<size_t>(ReadScalar<soffset_t>(table));
		// Check the vtable size field, then check vtable fits in its entirety.
		return VerifyComplexity() && Verify<voffset_t>(vtableo) && VerifyAlignment<voffset_t>(ReadScalar<voffset_t>(buf + vtableo)) && Verify(vtableo, ReadScalar<voffset_t>(buf + vtableo));
	}

	template<typename T> bool VerifyBufferFromStart(const char* identifier, size_t start) {
		if (identifier && (size < 2 * sizeof(flatbuffers::uoffset_t) || !BufferHasIdentifier(buf + start, identifier)))
			return false;

		// Call T::Verify, which must be in the generated code for this type
		auto o = VerifyOffset(start);
		return o && reinterpret_cast<const T*>(buf + start + o)->Verify(*this)
#ifdef FLATBUFFERS_TRACK_VERIFIER_BUFFER_SIZE
			   && GetComputedSize()
#endif
				;
	}

	// Verify this whole buffer, starting with root type T.
	template<typename T> bool VerifyBuffer()									{ return VerifyBuffer<T>(nullptr); }
	template<typename T> bool VerifyBuffer(const char* identifier)				{ return VerifyBufferFromStart<T>(identifier, 0); }
	template<typename T> bool VerifySizePrefixedBuffer(const char* identifier)	{ return Verify<uoffset_t>(0U) && ReadScalar<uoffset_t>(buf) == size - sizeof(uoffset_t) && VerifyBufferFromStart<T>(identifier, sizeof(uoffset_t)); }

	uoffset_t VerifyOffset(size_t start) const {
		if (!Verify<uoffset_t>(start))
			return 0;
		auto o = ReadScalar<uoffset_t>(buf + start);
		// May not point to itself.
		Check(o != 0);
		// Can't wrap around / buffers are max 2GB.
		if (!Check(static_cast<soffset_t>(o) >= 0))
			return 0;
		// Must be inside the buffer to create a pointer from it (pointer outside buffer is UB).
		if (!Verify(start + o, 1))
			return 0;
		return o;
	}

	uoffset_t VerifyOffset(const uint8* base, voffset_t start) const {
		return VerifyOffset(static_cast<size_t>(base - buf) + start);
	}

	// Called at the start of a table to increase counters measuring data structure depth and amount, and possibly bails out with false if limits set by the constructor have been hit. Needs to be balanced with EndTable()
	bool VerifyComplexity() {
		depth++;
		num_tables++;
		return Check(depth <= max_depth && num_tables <= max_tables);
	}

	// Called at the end of a table to pop the depth count
	bool EndTable() {
		depth--;
		return true;
	}

#ifdef FLATBUFFERS_TRACK_VERIFIER_BUFFER_SIZE
	// Returns the message size in bytes
	size_t GetComputedSize() const {
		uintptr_t asize = (upper_bound - 1 + sizeof(uoffset_t)) & ~(sizeof(uoffset_t) - 1);
		return asize > size ? 0 : asize;
	}
#endif
};

//-----------------------------------------------------------------------------
//	internal structures
//-----------------------------------------------------------------------------

// Convenient way to bundle a buffer and its length, to pass it around typed by its root
struct BufferRefBase {
	uint8*		buf;
	uoffset_t	len;
	bool		must_free;

	BufferRefBase() : buf(nullptr), len(0), must_free(false) {}
	BufferRefBase(uint8* buf, uoffset_t len) : buf(buf), len(len), must_free(false) {}

	~BufferRefBase() {
		if (must_free)
			iso::free(buf);
	}
};

template<typename T> struct BufferRef : BufferRefBase {
	const T* GetRoot() const {
		return flatbuffers::GetRoot<T>(this->buf);
	}
	bool Verify() {
		Verifier verifier(this->buf, this->len);
		return verifier.VerifyBuffer<T>(nullptr);
	}
};

// "structs" are flat structures that do not have an offset table, thus always have all members present and do not support forwards/backwards compatible extensions
class Struct {
	uint8 data[1];
public:
	template<typename T> T GetField(uoffset_t o)		const	{ return ReadScalar<T>(&data[o]); }
	template<typename T> T GetStruct(uoffset_t o)		const	{ return reinterpret_cast<T>(&data[o]); }
	const uint8*		   GetAddressOf(uoffset_t o)	const	{ return &data[o]; }
	uint8*				   GetAddressOf(uoffset_t o)			{ return &data[o]; }
};

// "tables" use an offset table (possibly shared) that allows fields to be omitted and added at will, but uses an extra indirection to read
class Table {
	uint8 data[1];
	Table(const Table& other)=delete;

	const uint8* GetVTable() const {
		return data - ReadScalar<soffset_t>(data);
	}
	voffset_t GetOptionalFieldOffset(voffset_t field) const {
		auto vtable = GetVTable();
		auto vtsize = ReadScalar<voffset_t>(vtable);
		return field < vtsize ? ReadScalar<voffset_t>(vtable + field) : 0;
	}
public:
	template<typename T> T GetField(voffset_t field, T defaultval) const {
		auto field_offset = GetOptionalFieldOffset(field);
		return field_offset ? ReadScalar<T>(data + field_offset) : defaultval;
	}
	template<typename P> P GetPointer(voffset_t field) const {
		auto field_offset = GetOptionalFieldOffset(field);
		return field_offset ? (P)*(const Offset<deref_t<P>>*)(data + field_offset) : nullptr;
	}
	template<typename P> P GetStruct(voffset_t field) const {
		auto field_offset = GetOptionalFieldOffset(field);
		return field_offset ? reinterpret_cast<P>(data + field_offset) : nullptr;
	}
	template<typename T> bool SetField(voffset_t field, T val, T def) {
		auto field_offset = GetOptionalFieldOffset(field);
		if (!field_offset)
			return val == def;
		WriteScalar(data + field_offset, val);
		return true;
	}
	bool SetPointer(voffset_t field, const uint8* val) {
		auto field_offset = GetOptionalFieldOffset(field);
		if (!field_offset)
			return false;
		WriteScalar(data + field_offset, static_cast<uoffset_t>(val - (data + field_offset)));
		return true;
	}
	uint8* GetAddressOf(voffset_t field) {
		auto field_offset = GetOptionalFieldOffset(field);
		return field_offset ? data + field_offset : nullptr;
	}
	const uint8* GetAddressOf(voffset_t field) const {
		return const_cast<Table*>(this)->GetAddressOf(field);
	}
	bool CheckField(voffset_t field) const {
		return GetOptionalFieldOffset(field) != 0;
	}
	// Verify the vtable of this table. Call this once per table, followed by VerifyField once per field
	bool VerifyTableStart(Verifier& verifier) const {
		return verifier.VerifyTableStart(data);
	}
	// Verify a particular field
	template<typename T> bool VerifyField(const Verifier& verifier, voffset_t field) const {
		auto field_offset = GetOptionalFieldOffset(field);
		return !field_offset || verifier.Verify<T>(data, field_offset);
	}
	// VerifyField for required fields.
	template<typename T> bool VerifyFieldRequired(const Verifier& verifier, voffset_t field) const {
		auto field_offset = GetOptionalFieldOffset(field);
		return verifier.Check(field_offset != 0) && verifier.Verify<T>(data, field_offset);
	}
	// Versions for offsets
	bool VerifyOffset(const Verifier& verifier, voffset_t field) const {
		auto field_offset = GetOptionalFieldOffset(field);
		return !field_offset || verifier.VerifyOffset(data, field_offset);
	}
	bool VerifyOffsetRequired(const Verifier& verifier, voffset_t field) const {
		auto field_offset = GetOptionalFieldOffset(field);
		return verifier.Check(field_offset != 0) && verifier.VerifyOffset(data, field_offset);
	}
};

typedef uint32 uoffset_t;
typedef int32  soffset_t;  // Signed offsets for references that can go in both directions.
typedef uint16 voffset_t;  // Offset/index used in v-tables, can be changed to uint8 in format forks to save a bit of space if desired.
typedef uint64 largest_scalar_t;


template<typename T> T GetField(uint8 *data, uint16 field, T defaultval) {
	auto field_offset = *(uint16*)(data - *(int32*)data + field);
	return field_offset ? *(T*)(data + field_offset) : defaultval;
}


template<typename T, typename S> struct As;
template<typename T, T N> struct Default;

template<uint16 O, typename T> struct Field {
	T	get()			const	{ return ((Table*)this)[-1].GetField<T>(O, T()); }
	T	operator()()	const	{ return get(); }
	operator T()		const	{ return get(); }
};

template<uint16 O, typename T, typename S> struct Field<O, As<T, S>> {
	T	get()			const	{ return static_cast<T>(((Table*)this)[-1].GetField<S>(O, S())); }
	T	operator()()	const	{ return get(); }
	operator T()		const	{ return get(); }
};

template<uint16 O, typename T, T N> struct Field<O, Default<T, N>> {
	T	get()			const	{ return ((Table*)this)[-1].GetField<T>(O, N); }
	T	operator()()	const	{ return get(); }
	operator T()		const	{ return get(); }
};

template<uint16 O, typename T> struct Field<O, T*> {
	const T*	get()			const	{ return ((Table*)this)[-1].GetPointer<const T*>(O); }
	const T*	operator()()	const	{ return get(); }
	operator const T*()			const	{ return get(); }
	const T*	operator->()	const	{ return get(); }
	template<typename T2> const T2*	as() const { return (const T2*)get(); }
};

template<uint16 O, typename T> struct Field<O, Vector<T>> {
	const Vector<T>&	get()			const	{ return *((Table*)this)[-1].GetPointer<const Vector<T>*>(O); }
	const Vector<T>&	operator()()	const	{ return get(); }
	operator const Vector<T>&()			const	{ return get(); }

	const T&	operator[](uoffset_t i) const	{ return get().Get(i); }
	uoffset_t	size()	const	{ return get().size(); }
	auto		begin()			{ return get().begin(); }
	auto		begin() const	{ return get().begin(); }
	auto		end()			{ return get().end(); }
	auto		end()	const	{ return get().end(); }
};

struct NativeTable {};

typedef uint64 hash_value_t;
typedef void (*resolver_function_t)(void** pointer_adr, hash_value_t hash);
typedef hash_value_t (*rehasher_function_t)(void* pointer);

template<typename T> bool IsFieldPresent(const T* table, typename T::FlatBuffersVTableOffset field) {
	return reinterpret_cast<const Table*>(table)->CheckField(static_cast<voffset_t>(field));
}

inline int LookupEnum(const char** names, const char* name) {
	for (const char** p = names; *p; p++)
		if (!strcmp(*p, name))
			return static_cast<int>(p - names);
	return -1;
}

//-----------------------------------------------------------------------------
//	header
//-----------------------------------------------------------------------------

template<typename T> T* GetMutableRoot(void* buf) {
	EndianCheck();
	return (T*)((uint8*)buf + EndianScalar(*(uoffset_t*)buf));
}

template<typename T> const T* GetRoot(const void* buf)				{ return GetMutableRoot<T>(const_cast<void*>(buf)); }
template<typename T> const T* GetSizePrefixedRoot(const void* buf)	{ return GetRoot<T>(reinterpret_cast<const uint8*>(buf) + sizeof(uoffset_t)); }

inline const char*	GetBufferIdentifier(const void* buf, bool size_prefixed = false)							{ return (const char*)buf + (size_prefixed ? 2 * sizeof(uoffset_t) : sizeof(uoffset_t)); }
inline bool			BufferHasIdentifier(const void* buf, const char* identifier, bool size_prefixed = false)	{ return strncmp(GetBufferIdentifier(buf, size_prefixed), identifier, 4) == 0; }

// Get the root, regardless of what type it is.
inline Table*		GetAnyRoot(uint8* flatbuf)			{ return GetMutableRoot<Table>(flatbuf); }
inline const Table* GetAnyRoot(const uint8* flatbuf)	{ return GetRoot<Table>(flatbuf); }

/// This return the prefixed size of a FlatBuffer.
inline uoffset_t	GetPrefixedSize(const uint8* buf) { return ReadScalar<uoffset_t>(buf); }


//-----------------------------------------------------------------------------
//	reflection
//-----------------------------------------------------------------------------
#if 0
namespace reflection {

	struct Type;
	struct KeyValue;
	struct EnumVal;
	struct Enum;
	struct Field;
	struct Object;
	struct RPCCall;
	struct Service;
	struct Schema;

	enum BaseType { None = 0, UType = 1, Bool = 2, Byte = 3, UByte = 4, Short = 5, UShort = 6, Int = 7, UInt = 8, Long = 9, ULong = 10, Float = 11, Double = 12, String = 13, Vector = 14, Obj = 15, Union = 16 };

	inline const BaseType (&EnumValuesBaseType())[17] {
		static const BaseType values[] = {None, UType, Bool, Byte, UByte, Short, UShort, Int, UInt, Long, ULong, Float, Double, String, Vector, Obj, Union};
	return values;
	}

		inline const char* const* EnumNamesBaseType() {
		static const char* const names[] = {"None", "UType", "Bool", "Byte", "UByte", "Short", "UShort", "Int", "UInt", "Long", "ULong", "Float", "Double", "String", "Vector", "Obj", "Union", nullptr};
		return names;
	}

	inline const char* EnumNameBaseType(BaseType e) {
		if (e < None || e > Union)
			return "";
		const size_t index = static_cast<int>(e);
		return EnumNamesBaseType()[index];
	}

	struct Type : private flatbuffers::Table {
		enum FlatBuffersVTableOffset { VT_BASE_TYPE = 4, VT_ELEMENT = 6, VT_INDEX = 8 };
		BaseType	base_type() const { return static_cast<BaseType>(GetField<int8>(VT_BASE_TYPE, 0)); }
		BaseType	element()	const { return static_cast<BaseType>(GetField<int8>(VT_ELEMENT, 0)); }
		int32		index()		const { return GetField<int32>(VT_INDEX, -1); }
		bool		Verify(flatbuffers::Verifier& verifier) const { return VerifyTableStart(verifier) && VerifyField<int8>(verifier, VT_BASE_TYPE) && VerifyField<int8>(verifier, VT_ELEMENT) && VerifyField<int32>(verifier, VT_INDEX) && verifier.EndTable(); }
	};

	struct KeyValue : private flatbuffers::Table {
		enum FlatBuffersVTableOffset { VT_KEY = 4, VT_VALUE = 6 };
		const flatbuffers::String* key()									const { return GetPointer<const flatbuffers::String*>(VT_KEY); }
		bool					   KeyCompareLessThan(const KeyValue* o)	const { return *key() < *o->key(); }
		int						   KeyCompareWithValue(const char* val)		const { return strcmp(*key(), val); }
		const flatbuffers::String* value()									const { return GetPointer<const flatbuffers::String*>(VT_VALUE); }
		bool					   Verify(flatbuffers::Verifier& verifier)	const { return VerifyTableStart(verifier) && VerifyOffsetRequired(verifier, VT_KEY) && verifier.VerifyString(key()) && VerifyOffset(verifier, VT_VALUE) && verifier.VerifyString(value()) && verifier.EndTable(); }
	};

	struct EnumVal : private flatbuffers::Table {
		enum FlatBuffersVTableOffset { VT_NAME = 4, VT_VALUE = 6, VT_OBJECT = 8, VT_UNION_TYPE = 10, VT_DOCUMENTATION = 12 };
		auto	name()									const { return GetPointer<const flatbuffers::String*>(VT_NAME); }
		auto	value()									const { return GetField<int64>(VT_VALUE, 0); }
		auto	KeyCompareLessThan(const EnumVal* o)	const { return value() < o->value(); }
		auto	KeyCompareWithValue(int64 val)			const { return static_cast<int>(value() > val) - static_cast<int>(value() < val); }
		auto	object()								const { return GetPointer<const Object*>(VT_OBJECT); }
		auto	union_type()							const { return GetPointer<const Type*>(VT_UNION_TYPE); }
		auto	documentation()							const { return GetPointer<const flatbuffers::Vector<flatbuffers::Offset<flatbuffers::String>>*>(VT_DOCUMENTATION); }
		bool	Verify(flatbuffers::Verifier& verifier) const {
			return VerifyTableStart(verifier) && VerifyOffsetRequired(verifier, VT_NAME) && verifier.VerifyString(name()) && VerifyField<int64>(verifier, VT_VALUE) && VerifyOffset(verifier, VT_OBJECT) && verifier.VerifyTable(object())
				&& VerifyOffset(verifier, VT_UNION_TYPE) && verifier.VerifyTable(union_type()) && VerifyOffset(verifier, VT_DOCUMENTATION) && verifier.VerifyVector(documentation()) && verifier.VerifyVectorOfStrings(documentation()) && verifier.EndTable();
		}
	};

	struct Enum : private flatbuffers::Table {
		enum FlatBuffersVTableOffset { VT_NAME = 4, VT_VALUES = 6, VT_IS_UNION = 8, VT_UNDERLYING_TYPE = 10, VT_ATTRIBUTES = 12, VT_DOCUMENTATION = 14 };
		auto	name()									const { return GetPointer<const flatbuffers::String*>(VT_NAME); }
		auto	KeyCompareLessThan(const Enum* o)		const { return *name() < *o->name(); }
		auto	KeyCompareWithValue(const char* val)	const { return strcmp(*name(), val); }
		auto	values()								const { return GetPointer<const flatbuffers::Vector<flatbuffers::Offset<EnumVal>>*>(VT_VALUES); }
		auto	is_union()								const { return GetField<uint8>(VT_IS_UNION, 0) != 0; }
		auto	underlying_type()						const { return GetPointer<const Type*>(VT_UNDERLYING_TYPE); }
		auto	attributes()							const { return GetPointer<const flatbuffers::Vector<flatbuffers::Offset<KeyValue>>*>(VT_ATTRIBUTES); }
		auto	documentation()							const { return GetPointer<const flatbuffers::Vector<flatbuffers::Offset<flatbuffers::String>>*>(VT_DOCUMENTATION); }
		auto	Verify(flatbuffers::Verifier& verifier) const {
			return VerifyTableStart(verifier) && VerifyOffsetRequired(verifier, VT_NAME) && verifier.VerifyString(name()) && VerifyOffsetRequired(verifier, VT_VALUES) && verifier.VerifyVector(values()) && verifier.VerifyVectorOfTables(values())
				&& VerifyField<uint8>(verifier, VT_IS_UNION) && VerifyOffsetRequired(verifier, VT_UNDERLYING_TYPE) && verifier.VerifyTable(underlying_type()) && VerifyOffset(verifier, VT_ATTRIBUTES) && verifier.VerifyVector(attributes())
				&& verifier.VerifyVectorOfTables(attributes()) && VerifyOffset(verifier, VT_DOCUMENTATION) && verifier.VerifyVector(documentation()) && verifier.VerifyVectorOfStrings(documentation()) && verifier.EndTable();
		}
	};

	struct Field : private flatbuffers::Table {
		enum FlatBuffersVTableOffset { VT_NAME = 4, VT_TYPE = 6, VT_ID = 8, VT_OFFSET = 10, VT_DEFAULT_INTEGER = 12, VT_DEFAULT_REAL = 14, VT_DEPRECATED = 16, VT_REQUIRED = 18, VT_KEY = 20, VT_ATTRIBUTES = 22, VT_DOCUMENTATION = 24 };
		auto	name()									const { return GetPointer<const flatbuffers::String*>(VT_NAME); }
		auto	KeyCompareLessThan(const Field* o)		const { return *name() < *o->name(); }
		auto	KeyCompareWithValue(const char* val)	const { return strcmp(*name(), val); }
		auto	type()									const { return GetPointer<const Type*>(VT_TYPE); }
		auto	id()									const { return GetField<uint16>(VT_ID, 0); }
		auto	offset()								const { return GetField<uint16>(VT_OFFSET, 0); }
		auto	default_integer()						const { return GetField<int64>(VT_DEFAULT_INTEGER, 0); }
		auto	default_real()							const { return GetField<double>(VT_DEFAULT_REAL, 0.0); }
		auto	deprecated()							const { return GetField<uint8>(VT_DEPRECATED, 0) != 0; }
		auto	required()								const { return GetField<uint8>(VT_REQUIRED, 0) != 0; }
		auto	key()									const { return GetField<uint8>(VT_KEY, 0) != 0; }
		auto	attributes()							const { return GetPointer<const flatbuffers::Vector<flatbuffers::Offset<KeyValue>>*>(VT_ATTRIBUTES); }
		auto	documentation()							const { return GetPointer<const flatbuffers::Vector<flatbuffers::Offset<flatbuffers::String>>*>(VT_DOCUMENTATION); }
		bool	Verify(flatbuffers::Verifier& verifier) const {
			return VerifyTableStart(verifier) && VerifyOffsetRequired(verifier, VT_NAME) && verifier.VerifyString(name()) && VerifyOffsetRequired(verifier, VT_TYPE) && verifier.VerifyTable(type()) && VerifyField<uint16>(verifier, VT_ID)
				&& VerifyField<uint16>(verifier, VT_OFFSET) && VerifyField<int64>(verifier, VT_DEFAULT_INTEGER) && VerifyField<double>(verifier, VT_DEFAULT_REAL) && VerifyField<uint8>(verifier, VT_DEPRECATED) && VerifyField<uint8>(verifier, VT_REQUIRED)
				&& VerifyField<uint8>(verifier, VT_KEY) && VerifyOffset(verifier, VT_ATTRIBUTES) && verifier.VerifyVector(attributes()) && verifier.VerifyVectorOfTables(attributes()) && VerifyOffset(verifier, VT_DOCUMENTATION)
				&& verifier.VerifyVector(documentation()) && verifier.VerifyVectorOfStrings(documentation()) && verifier.EndTable();
		}
	};

	struct Object : private flatbuffers::Table {
		enum FlatBuffersVTableOffset { VT_NAME = 4, VT_FIELDS = 6, VT_IS_STRUCT = 8, VT_MINALIGN = 10, VT_BYTESIZE = 12, VT_ATTRIBUTES = 14, VT_DOCUMENTATION = 16 };
		auto	name()									const { return GetPointer<const flatbuffers::String*>(VT_NAME); }
		auto	KeyCompareLessThan(const Object* o)		const { return *name() < *o->name(); }
		auto	KeyCompareWithValue(const char* val)	const { return strcmp(*name(), val); }
		auto	fields()								const { return GetPointer<const flatbuffers::Vector<flatbuffers::Offset<Field>>*>(VT_FIELDS); }
		auto	is_struct()								const { return GetField<uint8>(VT_IS_STRUCT, 0) != 0; }
		auto	minalign()								const { return GetField<int32>(VT_MINALIGN, 0); }
		auto	bytesize()								const { return GetField<int32>(VT_BYTESIZE, 0); }
		auto	attributes()							const { return GetPointer<const flatbuffers::Vector<flatbuffers::Offset<KeyValue>>*>(VT_ATTRIBUTES); }
		auto	documentation()							const { return GetPointer<const flatbuffers::Vector<flatbuffers::Offset<flatbuffers::String>>*>(VT_DOCUMENTATION); }
		auto	Verify(flatbuffers::Verifier& verifier) const {
			return VerifyTableStart(verifier) && VerifyOffsetRequired(verifier, VT_NAME) && verifier.VerifyString(name()) && VerifyOffsetRequired(verifier, VT_FIELDS) && verifier.VerifyVector(fields()) && verifier.VerifyVectorOfTables(fields())
				&& VerifyField<uint8>(verifier, VT_IS_STRUCT) && VerifyField<int32>(verifier, VT_MINALIGN) && VerifyField<int32>(verifier, VT_BYTESIZE) && VerifyOffset(verifier, VT_ATTRIBUTES) && verifier.VerifyVector(attributes())
				&& verifier.VerifyVectorOfTables(attributes()) && VerifyOffset(verifier, VT_DOCUMENTATION) && verifier.VerifyVector(documentation()) && verifier.VerifyVectorOfStrings(documentation()) && verifier.EndTable();
		}
	};

	struct RPCCall : private flatbuffers::Table {
		enum FlatBuffersVTableOffset { VT_NAME = 4, VT_REQUEST = 6, VT_RESPONSE = 8, VT_ATTRIBUTES = 10, VT_DOCUMENTATION = 12 };
		auto	name()									const { return GetPointer<const flatbuffers::String*>(VT_NAME); }
		auto	KeyCompareLessThan(const RPCCall* o)	const { return *name() < *o->name(); }
		auto	KeyCompareWithValue(const char* val)	const { return strcmp(*name(), val); }
		auto	request()								const { return GetPointer<const Object*>(VT_REQUEST); }
		auto	response()								const { return GetPointer<const Object*>(VT_RESPONSE); }
		auto	attributes()							const { return GetPointer<const flatbuffers::Vector<flatbuffers::Offset<KeyValue>>*>(VT_ATTRIBUTES); }
		auto	documentation()							const { return GetPointer<const flatbuffers::Vector<flatbuffers::Offset<flatbuffers::String>>*>(VT_DOCUMENTATION); }
		auto	Verify(flatbuffers::Verifier& verifier) const {
			return VerifyTableStart(verifier) && VerifyOffsetRequired(verifier, VT_NAME) && verifier.VerifyString(name()) && VerifyOffsetRequired(verifier, VT_REQUEST) && verifier.VerifyTable(request()) && VerifyOffsetRequired(verifier, VT_RESPONSE)
				&& verifier.VerifyTable(response()) && VerifyOffset(verifier, VT_ATTRIBUTES) && verifier.VerifyVector(attributes()) && verifier.VerifyVectorOfTables(attributes()) && VerifyOffset(verifier, VT_DOCUMENTATION)
				&& verifier.VerifyVector(documentation()) && verifier.VerifyVectorOfStrings(documentation()) && verifier.EndTable();
		}
	};

	struct Service : private flatbuffers::Table {
		enum FlatBuffersVTableOffset { VT_NAME = 4, VT_CALLS = 6, VT_ATTRIBUTES = 8, VT_DOCUMENTATION = 10 };
		auto	name()									const { return GetPointer<const flatbuffers::String*>(VT_NAME); }
		auto	KeyCompareLessThan(const Service* o)	const { return *name() < *o->name(); }
		auto	KeyCompareWithValue(const char* val)	const { return strcmp(*name(), val); }
		auto	calls()									const { return GetPointer<const flatbuffers::Vector<flatbuffers::Offset<RPCCall>>*>(VT_CALLS); }
		auto	attributes()							const { return GetPointer<const flatbuffers::Vector<flatbuffers::Offset<KeyValue>>*>(VT_ATTRIBUTES); }
		auto	documentation()							const { return GetPointer<const flatbuffers::Vector<flatbuffers::Offset<flatbuffers::String>>*>(VT_DOCUMENTATION); }
		auto	Verify(flatbuffers::Verifier& verifier) const {
			return VerifyTableStart(verifier) && VerifyOffsetRequired(verifier, VT_NAME) && verifier.VerifyString(name()) && VerifyOffset(verifier, VT_CALLS) && verifier.VerifyVector(calls()) && verifier.VerifyVectorOfTables(calls())
				&& VerifyOffset(verifier, VT_ATTRIBUTES) && verifier.VerifyVector(attributes()) && verifier.VerifyVectorOfTables(attributes()) && VerifyOffset(verifier, VT_DOCUMENTATION) && verifier.VerifyVector(documentation())
				&& verifier.VerifyVectorOfStrings(documentation()) && verifier.EndTable();
		}
	};

	struct Schema : private flatbuffers::Table {
		enum FlatBuffersVTableOffset { VT_OBJECTS = 4, VT_ENUMS = 6, VT_FILE_IDENT = 8, VT_FILE_EXT = 10, VT_ROOT_TABLE = 12, VT_SERVICES = 14 };
		auto	objects()								const { return GetPointer<const flatbuffers::Vector<flatbuffers::Offset<Object>>*>(VT_OBJECTS); }
		auto	enums()									const { return GetPointer<const flatbuffers::Vector<flatbuffers::Offset<Enum>>*>(VT_ENUMS); }
		auto	file_ident()							const { return GetPointer<const flatbuffers::String*>(VT_FILE_IDENT); }
		auto	file_ext()								const { return GetPointer<const flatbuffers::String*>(VT_FILE_EXT); }
		auto	root_table()							const { return GetPointer<const Object*>(VT_ROOT_TABLE); }
		auto	services()								const { return GetPointer<const flatbuffers::Vector<flatbuffers::Offset<Service>>*>(VT_SERVICES); }
		auto	Verify(flatbuffers::Verifier& verifier) const {
			return VerifyTableStart(verifier)
				&& VerifyOffsetRequired(verifier, VT_OBJECTS) && verifier.VerifyVector(objects()) && verifier.VerifyVectorOfTables(objects())
				&& VerifyOffsetRequired(verifier, VT_ENUMS) && verifier.VerifyVector(enums()) && verifier.VerifyVectorOfTables(enums())
				&& VerifyOffset(verifier, VT_FILE_IDENT) && verifier.VerifyString(file_ident())
				&& VerifyOffset(verifier, VT_FILE_EXT) && verifier.VerifyString(file_ext())
				&& VerifyOffset(verifier, VT_ROOT_TABLE) && verifier.VerifyTable(root_table())
				&& VerifyOffset(verifier, VT_SERVICES) && verifier.VerifyVector(services()) && verifier.VerifyVectorOfTables(services())
				&& verifier.EndTable();
		}
	};

	inline const reflection::Schema* GetSchema(const void* buf)											{ return flatbuffers::GetRoot<reflection::Schema>(buf); }
	inline const reflection::Schema* GetSizePrefixedSchema(const void* buf)								{ return flatbuffers::GetSizePrefixedRoot<reflection::Schema>(buf); }
	inline const char*				 SchemaIdentifier()													{ return "BFBS"; }
	inline bool						 SchemaBufferHasIdentifier(const void* buf)							{ return flatbuffers::BufferHasIdentifier(buf, SchemaIdentifier()); }
	inline bool						 VerifySchemaBuffer(flatbuffers::Verifier& verifier)				{ return verifier.VerifyBuffer<reflection::Schema>(SchemaIdentifier()); }
	inline bool						 VerifySizePrefixedSchemaBuffer(flatbuffers::Verifier& verifier)	{ return verifier.VerifySizePrefixedBuffer<reflection::Schema>(SchemaIdentifier()); }
	inline const char*				 SchemaExtension()													{ return "bfbs"; }

	// ------------------------- getters -------------------------

	inline bool IsScalar(BaseType t)	{ return t >= UType && t <= Double; }
	inline bool IsInteger(BaseType t)	{ return t >= UType && t <= ULong; }
	inline bool IsFloat(BaseType t)		{ return t == Float || t == Double; }
	inline bool IsLong(BaseType t)		{ return t == Long || t == ULong; }

	// Size of a basic type, don't use with structs.
	inline size_t GetTypeSize(BaseType base_type) {
		// This needs to correspond to the BaseType enum.
		static size_t sizes[] = {0, 1, 1, 1, 1, 2, 2, 4, 4, 8, 8, 4, 8, 4, 4, 4, 4};
		return sizes[base_type];
	}

	// Same as above, but now correctly returns the size of a struct if the field (or vector element) is a struct.
	inline size_t GetTypeSizeInline(BaseType base_type, int type_index, const Schema& schema) {
		if (base_type == Obj && schema.objects()->Get(type_index)->is_struct())
			return schema.objects()->Get(type_index)->bytesize();
		return GetTypeSize(base_type);
	}

	// Get a field's default, if you know it's an integer, and its exact type.
	template<typename T> T GetFieldDefaultI(const Field& field) {
		ISO_ASSERT(sizeof(T) == GetTypeSize(field.type()->base_type()));
		return static_cast<T>(field.default_integer());
	}

	// Get a field's default, if you know it's floating point and its exact type.
	template<typename T> T GetFieldDefaultF(const Field& field) {
		ISO_ASSERT(sizeof(T) == GetTypeSize(field.type()->base_type()));
		return static_cast<T>(field.default_real());
	}

	// Get a field, if you know it's an integer, and its exact type.
	template<typename T> T GetFieldI(const Table& table, const Field& field) {
		ISO_ASSERT(sizeof(T) == GetTypeSize(field.type()->base_type()));
		return table.GetField<T>(field.offset(), static_cast<T>(field.default_integer()));
	}

	// Get a field, if you know it's floating point and its exact type.
	template<typename T> T GetFieldF(const Table& table, const Field& field) {
		ISO_ASSERT(sizeof(T) == GetTypeSize(field.type()->base_type()));
		return table.GetField<T>(field.offset(), static_cast<T>(field.default_real()));
	}

	// Get a field, if you know it's a string.
	inline const flatbuffers::String* GetFieldS(const Table& table, const Field& field) {
		ISO_ASSERT(field.type()->base_type() == String);
		return table.GetPointer<const flatbuffers::String*>(field.offset());
	}

	// Get a field, if you know it's a vector.
	template<typename T> flatbuffers::Vector<T>* GetFieldV(const flatbuffers::Table& table, const Field& field) {
		ISO_ASSERT(field.type()->base_type() == Vector && sizeof(T) == GetTypeSize(field.type()->element()));
		return table.GetPointer<flatbuffers::Vector<T>*>(field.offset());
	}

	// Get a field, if you know it's a vector, generically.
	// To actually access elements, use the return value together with field.type()->element() in any of GetAnyVectorElemI below etc.
	inline VectorOfAny* GetFieldAnyV(const flatbuffers::Table& table, const Field& field) { return table.GetPointer<VectorOfAny*>(field.offset()); }

	// Get a field, if you know it's a table.
	inline flatbuffers::Table* GetFieldT(const flatbuffers::Table& table, const Field& field) {
		ISO_ASSERT(field.type()->base_type() == Obj || field.type()->base_type() == Union);
		return table.GetPointer<flatbuffers::Table*>(field.offset());
	}

	// Get a field, if you know it's a struct.
	inline const Struct* GetFieldStruct(const Table& table, const Field& field) {
		// TODO: This does NOT check if the field is a table or struct, but we'd need access to the schema to check the is_struct flag.
		ISO_ASSERT(field.type()->base_type() == Obj);
		return table.GetStruct<const Struct*>(field.offset());
	}

	// Get a structure's field, if you know it's a struct.
	inline const Struct* GetFieldStruct(const Struct& structure, const Field& field) {
		ISO_ASSERT(field.type()->base_type() == Obj);
		return structure.GetStruct<const Struct*>(field.offset());
	}

	int64 GetAnyValueI(BaseType type, const uint8* data) {
#define FLATBUFFERS_GET(T) static_cast<int64>(ReadScalar<T>(data))
		switch (type) {
			case UType:
			case Bool:
			case UByte:		return FLATBUFFERS_GET(uint8);
			case Byte:		return FLATBUFFERS_GET(int8);
			case Short:		return FLATBUFFERS_GET(int16);
			case UShort:	return FLATBUFFERS_GET(uint16);
			case Int:		return FLATBUFFERS_GET(int32);
			case UInt:		return FLATBUFFERS_GET(uint32);
			case Long:		return FLATBUFFERS_GET(int64);
			case ULong:		return FLATBUFFERS_GET(uint64);
			case Float:		return FLATBUFFERS_GET(float);
			case Double:	return FLATBUFFERS_GET(double);
			case String: {
				auto s = reinterpret_cast<const flatbuffers::String*>(ReadScalar<uoffset_t>(data) + data);
				return s ? from_string<int64>(*s) : 0;
			}
			default:
				return 0;  // Tables & vectors do not make sense.
		}
#undef FLATBUFFERS_GET
	}

	double GetAnyValueF(BaseType type, const uint8* data) {
		switch (type) {
			case Float: return static_cast<double>(ReadScalar<float>(data));
			case Double: return ReadScalar<double>(data);
			case String: {
				auto s = reinterpret_cast<const flatbuffers::String*>(ReadScalar<uoffset_t>(data) + data);
				return s ? from_string<double>(*s) : 0.0;
			}
			default: return static_cast<double>(GetAnyValueI(type, data));
		}
	}

	string GetAnyValueS(BaseType type, const uint8* data, const Schema* schema, int type_index) {
		switch (type) {
			case Float:
			case Double:
				return NumToString(GetAnyValueF(type, data));
			case String: {
				auto s = reinterpret_cast<const flatbuffers::String*>(ReadScalar<uoffset_t>(data) + data);
				return s ? *s : "";
			}
			case Obj:
				if (schema) {
					// Convert the table to a string. This is mostly for debugging purposes, //
					// and does NOT promise to be JSON compliant. Also prefixes the type.
					auto&  objectdef = *schema->objects()->Get(type_index);
					string s		 = *objectdef.name();
					if (objectdef.is_struct()) {
						s += "(struct)";  // TODO: implement this as well.
					} else {
						auto table_field = reinterpret_cast<const Table*>(ReadScalar<uoffset_t>(data) + data);
						s += " { ";
						for (auto fielddef : *objectdef.fields()) {
							if (!table_field->CheckField(fielddef->offset()))
								continue;
							auto field_ptr	= table_field->GetAddressOf(fielddef->offset());
							auto val		= field_ptr ? GetAnyValueS(fielddef->type()->base_type(), field_ptr, schema, fielddef->type()->index()) : "";
							if (fielddef->type()->base_type() == String)
								val = escape(val.begin(), val.end());
							s += *fielddef->name();
							s += ": ";
							s += val;
							s += ", ";
						}
						s += "}";
					}
					return s;
				}
				return "(table)";
			case Vector:
				return "[(elements)]";  // TODO: implement this as well.
			case Union:
				return "(union)";  // TODO: implement this as well.
			default:
				return NumToString(GetAnyValueI(type, data));
		}
	}

	// Get any table field as a 64bit int, regardless of what type it is.
	inline int64 GetAnyFieldI(const Table& table, const Field& field) {
		auto field_ptr = table.GetAddressOf(field.offset());
		return field_ptr ? GetAnyValueI(field.type()->base_type(), field_ptr) : field.default_integer();
	}

	// Get any table field as a double, regardless of what type it is.
	inline double GetAnyFieldF(const Table& table, const Field& field) {
		auto field_ptr = table.GetAddressOf(field.offset());
		return field_ptr ? GetAnyValueF(field.type()->base_type(), field_ptr) : field.default_real();
	}

	// Get any table field as a string, regardless of what type it is.
	// You may pass nullptr for the schema if you don't care to have fields that are of table type pretty-printed.
	inline string GetAnyFieldS(const Table& table, const Field& field, const Schema* schema) {
		auto field_ptr = table.GetAddressOf(field.offset());
		return field_ptr ? GetAnyValueS(field.type()->base_type(), field_ptr, schema, field.type()->index()) : "";
	}

	// Get any struct field as a 64bit int, regardless of what type it is.
	inline int64 GetAnyFieldI(const Struct& st, const Field& field) { return GetAnyValueI(field.type()->base_type(), st.GetAddressOf(field.offset())); }

	// Get any struct field as a double, regardless of what type it is.
	inline double GetAnyFieldF(const Struct& st, const Field& field) { return GetAnyValueF(field.type()->base_type(), st.GetAddressOf(field.offset())); }

	// Get any struct field as a string, regardless of what type it is.
	inline string GetAnyFieldS(const Struct& st, const Field& field) { return GetAnyValueS(field.type()->base_type(), st.GetAddressOf(field.offset()), nullptr, -1); }

	// Get any vector element as a 64bit int, regardless of what type it is.
	inline int64 GetAnyVectorElemI(const VectorOfAny* vec, BaseType elem_type, size_t i) { return GetAnyValueI(elem_type, vec->Data() + GetTypeSize(elem_type) * i); }

	// Get any vector element as a double, regardless of what type it is.
	inline double GetAnyVectorElemF(const VectorOfAny* vec, BaseType elem_type, size_t i) { return GetAnyValueF(elem_type, vec->Data() + GetTypeSize(elem_type) * i); }

	// Get any vector element as a string, regardless of what type it is.
	inline string GetAnyVectorElemS(const VectorOfAny* vec, BaseType elem_type, size_t i) { return GetAnyValueS(elem_type, vec->Data() + GetTypeSize(elem_type) * i, nullptr, -1); }

	// Get a vector element that's a table/string/vector from a generic vector.
	// Pass Table/flatbuffers::String/VectorOfAny as template parameter.
	// Warning: does no typechecking.
	template<typename T> T* GetAnyVectorElemPointer(const VectorOfAny* vec, size_t i) {
		auto elem_ptr = vec->Data() + sizeof(uoffset_t) * i;
		return reinterpret_cast<T*>(elem_ptr + ReadScalar<uoffset_t>(elem_ptr));
	}

	// Get the inline-address of a vector element. Useful for Structs (pass Struct as template arg), or being able to address a range of scalars in-line
	// Get elem_size from GetTypeSizeInline()
	// Note: little-endian data on all platforms, use EndianScalar() instead of raw pointer access with scalars)
	template<typename T> T* GetAnyVectorElemAddressOf(const VectorOfAny* vec, size_t i, size_t elem_size) { return reinterpret_cast<T*>(vec->Data() + elem_size * i); }

	// Similarly, for elements of tables.
	template<typename T> T* GetAnyFieldAddressOf(const Table& table, const Field& field) { return reinterpret_cast<T*>(table.GetAddressOf(field.offset())); }

	// Similarly, for elements of structs.
	template<typename T> T* GetAnyFieldAddressOf(const Struct& st, const Field& field) { return reinterpret_cast<T*>(st.GetAddressOf(field.offset())); }


	// ------------------------- setters -------------------------

	// Set any scalar field, if you know its exact type.
	template<typename T> bool SetField(Table* table, const Field& field, T val) {
		BaseType type = field.type()->base_type();
		if (!IsScalar(type))
			return false;
		ISO_ASSERT(sizeof(T) == GetTypeSize(type));
		T def;
		if (IsInteger(type)) {
			def = GetFieldDefaultI<T>(field);
		} else {
			ISO_ASSERT(IsFloat(type));
			def = GetFieldDefaultF<T>(field);
		}
		return table->SetField(field.offset(), val, def);
	}

	// Raw helper functions used below: set any value in memory as a 64bit int, a double or a string
	// These work for all scalar values, but do nothing for other data types.
	// To set a string, see SetString below.
	void SetAnyValueI(BaseType type, uint8* data, int64 val);
	void SetAnyValueF(BaseType type, uint8* data, double val);
	void SetAnyValueS(BaseType type, uint8* data, const char* val);

	// Set any table field as a 64bit int, regardless of type what it is.
	inline bool SetAnyFieldI(Table* table, const Field& field, int64 val) {
		auto field_ptr = table->GetAddressOf(field.offset());
		if (!field_ptr)
			return val == GetFieldDefaultI<int64>(field);
		SetAnyValueI(field.type()->base_type(), field_ptr, val);
		return true;
	}

	// Set any table field as a double, regardless of what type it is.
	inline bool SetAnyFieldF(Table* table, const Field& field, double val) {
		auto field_ptr = table->GetAddressOf(field.offset());
		if (!field_ptr)
			return val == GetFieldDefaultF<double>(field);
		SetAnyValueF(field.type()->base_type(), field_ptr, val);
		return true;
	}

	// Set any table field as a string, regardless of what type it is.
	inline bool SetAnyFieldS(Table* table, const Field& field, const char* val) {
		auto field_ptr = table->GetAddressOf(field.offset());
		if (!field_ptr)
			return false;
		SetAnyValueS(field.type()->base_type(), field_ptr, val);
		return true;
	}

	// Set any struct field as a 64bit int, regardless of type what it is.
	inline void SetAnyFieldI(Struct* st, const Field& field, int64 val) { SetAnyValueI(field.type()->base_type(), st->GetAddressOf(field.offset()), val); }

	// Set any struct field as a double, regardless of type what it is.
	inline void SetAnyFieldF(Struct* st, const Field& field, double val) { SetAnyValueF(field.type()->base_type(), st->GetAddressOf(field.offset()), val); }

	// Set any struct field as a string, regardless of type what it is.
	inline void SetAnyFieldS(Struct* st, const Field& field, const char* val) { SetAnyValueS(field.type()->base_type(), st->GetAddressOf(field.offset()), val); }

	// Set any vector element as a 64bit int, regardless of type what it is.
	inline void SetAnyVectorElemI(VectorOfAny* vec, BaseType elem_type, size_t i, int64 val) { SetAnyValueI(elem_type, vec->Data() + GetTypeSize(elem_type) * i, val); }

	// Set any vector element as a double, regardless of type what it is.
	inline void SetAnyVectorElemF(VectorOfAny* vec, BaseType elem_type, size_t i, double val) { SetAnyValueF(elem_type, vec->Data() + GetTypeSize(elem_type) * i, val); }

	// Set any vector element as a string, regardless of type what it is.
	inline void SetAnyVectorElemS(VectorOfAny* vec, BaseType elem_type, size_t i, const char* val) { SetAnyValueS(elem_type, vec->Data() + GetTypeSize(elem_type) * i, val); }


	inline bool SetFieldT(Table* table, const Field& field, const uint8* val) {
		ISO_ASSERT(sizeof(uoffset_t) == GetTypeSize(field.type()->base_type()));
		return table->SetPointer(field.offset(), val);
	}

	void SetAnyValueI(BaseType type, uint8* data, int64 val) {
#define FLATBUFFERS_SET(T) WriteScalar(data, static_cast<T>(val))
		switch (type) {
			case UType:
			case Bool:
			case UByte:		FLATBUFFERS_SET(uint8); break;
			case Byte:		FLATBUFFERS_SET(int8); break;
			case Short:		FLATBUFFERS_SET(int16); break;
			case UShort:	FLATBUFFERS_SET(uint16); break;
			case Int:		FLATBUFFERS_SET(int32); break;
			case UInt:		FLATBUFFERS_SET(uint32); break;
			case Long:		FLATBUFFERS_SET(int64); break;
			case ULong:		FLATBUFFERS_SET(uint64); break;
			case Float:		FLATBUFFERS_SET(float); break;
			case Double:	FLATBUFFERS_SET(double); break;
				// TODO: support strings
			default: break;
		}
#undef FLATBUFFERS_SET
	}

	void SetAnyValueF(BaseType type, uint8* data, double val) {
		switch (type) {
			case Float:		WriteScalar(data, static_cast<float>(val)); break;
			case Double:	WriteScalar(data, val); 	break;
				// TODO: support strings.
			default: SetAnyValueI(type, data, static_cast<int64>(val)); break;
		}
	}

	void SetAnyValueS(BaseType type, uint8* data, const char* val) {
		switch (type) {
			case Float:
			case Double:	SetAnyValueF(type, data, from_string<double>(val)); break;
				// TODO: support strings.
			default: SetAnyValueI(type, data, from_string<int64>(val)); break;
		}
	}

	// ------------------------- resizing setters -------------------------

	// "smart" pointer for use with resizing vectors: turns a pointer inside a vector into a relative offset, such that it is not affected by resizes
	template<typename T, typename U> class pointer_inside_vector {
		size_t			  offset;
		dynamic_array<U>& vec;
	public:
		pointer_inside_vector(T* ptr, dynamic_array<U>& vec) : offset((uint8*)ptr - (uint8*)vec.begin()), vec(vec) {}

		T*   operator*()	const { return(T*)((uint8*)vec.begin() + offset); }
		T*   operator->()	const { return operator*(); }
		void operator=(const pointer_inside_vector& piv);
	};

	// Helper to create the above easily without specifying template args.
	template<typename T, typename U> pointer_inside_vector<T, U> piv(T* ptr, dynamic_array<U>& vec) { return pointer_inside_vector<T, U>(ptr, vec); }

	inline const char* UnionTypeFieldSuffix() { return "_type"; }

	// Helper to figure out the actual table type a union refers to.
	inline const Object& GetUnionType(const Schema& schema, const Object& parent, const Field& unionfield, const Table& table) {
		auto enumdef = schema.enums()->Get(unionfield.type()->index());
		// TODO: this is clumsy and slow, but no other way to find it?
		auto type_field = parent.fields()->LookupByKey(string(*unionfield.name()) + UnionTypeFieldSuffix());
		ISO_ASSERT(type_field);
		auto union_type = GetFieldI<uint8>(table, *type_field);
		auto enumval	= enumdef->values()->LookupByKey(union_type);
		return *enumval->object();
	}

	// Resize a FlatBuffer in-place by iterating through all offsets in the buffer and adjusting them by "delta" if they straddle the start offset.
	// Once that is done, bytes can now be inserted/deleted safely.
	// "delta" may be negative (shrinking).
	// Unless "delta" is a multiple of the largest alignment, you'll create a small amount of garbage space in the buffer (usually 0..7 bytes).
	// If your FlatBuffer's root table is not the schema's root table, you should pass in your root_table type as well.
	class ResizeContext {
		const Schema& schema_;
		uint8*					startptr_;
		int						delta_;
		dynamic_array<uint8>&	buf_;
		dynamic_array<uint8>	dag_check_;
	public:
		ResizeContext(const Schema& schema, uoffset_t start, int delta, dynamic_array<uint8>* flatbuf, const Object* root_table = nullptr)
			: schema_(schema), startptr_((*flatbuf) + start), delta_(delta), buf_(*flatbuf), dag_check_(flatbuf->size() / sizeof(uoffset_t), false)
		{
			auto mask = static_cast<int>(sizeof(largest_scalar_t) - 1);
			delta_	= (delta_ + mask) & ~mask;
			if (!delta_)
				return;  // We can't shrink by less than largest_scalar_t

						 // Now change all the offsets by delta_.
			auto root = GetAnyRoot((buf_));
			Straddle<uoffset_t, 1>((buf_), root, (buf_));
			ResizeTable(root_table ? *root_table : *schema.root_table(), root);
			// We can now add or remove bytes at start.
			if (delta_ > 0)
				buf_.insertc(buf_.begin() + start, repeat(0, delta_));
			else
				buf_.erase(buf_.begin() + start, buf_.begin() + start - delta_);
		}

		// Check if the range between first (lower address) and second straddles the insertion point. If it does, change the offset at offsetloc (of type T, with direction D).
		template<typename T, int D> void Straddle(const void* first, const void* second, void* offsetloc) {
			if (first <= startptr_ && second >= startptr_) {
				WriteScalar<T>(offsetloc, ReadScalar<T>(offsetloc) + delta_ * D);
				DagCheck(offsetloc) = true;
			}
		}

		// This returns a boolean that records if the corresponding offset location has been modified already. If so, we can't even read the corresponding offset, since it is pointing to a location that is illegal until the resize actually happens.
		// This must be checked for every offset, since we can't know which offsets will straddle and which won't.
		uint8& DagCheck(const void* offsetloc) {
			auto dag_idx = reinterpret_cast<const uoffset_t*>(offsetloc) - reinterpret_cast<const uoffset_t*>(buf_.begin());
			return dag_check_[dag_idx];
		}

		void ResizeTable(const Object& objectdef, Table* table) {
			if (DagCheck(table))
				return;  // Table already visited.
			auto vtable = table->GetVTable();
			// Early out: since all fields inside the table must point forwards in memory, if the insertion point is before the table we can stop here.
			auto tableloc = reinterpret_cast<uint8*>(table);
			if (startptr_ <= tableloc) {
				// Check if insertion point is between the table and a vtable that precedes it. This can't happen in current construction code, but check just in case we ever change the way flatbuffers are built.
				Straddle<soffset_t, -1>(vtable, table, table);
			} else {
				// Check each field.
				auto fielddefs = objectdef.fields();
				for (auto it = fielddefs->begin(); it != fielddefs->end(); ++it) {
					auto& fielddef  = **it;
					auto  base_type = fielddef.type()->base_type();
					// Ignore scalars.
					if (base_type <= Double)
						continue;
					// Ignore fields that are not stored.
					auto offset = table->GetOptionalFieldOffset(fielddef.offset());
					if (!offset)
						continue;
					// Ignore structs.
					auto subobjectdef = base_type == Obj ? schema_.objects()->Get(fielddef.type()->index()) : nullptr;
					if (subobjectdef && subobjectdef->is_struct())
						continue;
					// Get this fields' offset, and read it if safe.
					auto offsetloc = tableloc + offset;
					if (DagCheck(offsetloc))
						continue;  // This offset already visited.
					auto ref = offsetloc + ReadScalar<uoffset_t>(offsetloc);
					Straddle<uoffset_t, 1>(offsetloc, ref, offsetloc);
					// Recurse.
					switch (base_type) {
						case Obj: {
							ResizeTable(*subobjectdef, reinterpret_cast<Table*>(ref));
							break;
						}
						case Vector: {
							auto elem_type = fielddef.type()->element();
							if (elem_type != Obj && elem_type != String)
								break;
							auto vec		   = reinterpret_cast<flatbuffers::Vector<uoffset_t>*>(ref);
							auto elemobjectdef = elem_type == Obj ? schema_.objects()->Get(fielddef.type()->index()) : nullptr;
							if (elemobjectdef && elemobjectdef->is_struct())
								break;
							for (uoffset_t i = 0; i < vec->size(); i++) {
								auto loc = vec->Data() + i * sizeof(uoffset_t);
								if (DagCheck(loc))
									continue;  // This offset already visited.
								auto dest = loc + vec->Get(i);
								Straddle<uoffset_t, 1>(loc, dest, loc);
								if (elemobjectdef)
									ResizeTable(*elemobjectdef, reinterpret_cast<Table*>(dest));
							}
							break;
						}
						case Union: {
							ResizeTable(GetUnionType(schema_, objectdef, fielddef, *table), reinterpret_cast<Table*>(ref));
							break;
						}
						case String: break;
						default: ISO_ASSERT(false);
					}
				}
				// Check if the vtable offset points beyond the insertion point.
				// Must do this last, since GetOptionalFieldOffset above still reads this value.
				Straddle<soffset_t, -1>(table, vtable, table);
			}
		}

		//	void operator=(const ResizeContext& rc);
	};

	// Changes the contents of a string inside a FlatBuffer. FlatBuffer must live inside a dynamic_array so we can resize the buffer if needed.
	// "str" must live inside "flatbuf" and may be invalidated after this call.
	// If your FlatBuffer's root table is not the schema's root table, you should pass in your root_table type as well.
	void SetString(const Schema& schema, const string& val, const flatbuffers::String* str, dynamic_array<uint8>* flatbuf, const Object* root_table) {
		auto delta		= static_cast<int>(val.length()) - static_cast<int>(str->size());
		auto str_start	= static_cast<uoffset_t>(reinterpret_cast<const uint8*>(str) - (*flatbuf));
		auto start		= str_start + static_cast<uoffset_t>(sizeof(uoffset_t));
		if (delta) {
			// Clear the old string, since we don't want parts of it remaining.
			memset((*flatbuf) + start, 0, str->size());
			// Different size, we must expand (or contract).
			ResizeContext(schema, start, delta, flatbuf, root_table);
			// Set the new length.
			WriteScalar((*flatbuf) + str_start, static_cast<uoffset_t>(val.length()));
		}
		// Copy new data. Safe because we created the right amount of space.
		memcpy((*flatbuf) + start, val, val.length() + 1);
	}

	// Resizes a flatbuffers::Vector inside a FlatBuffer. FlatBuffer must live inside a dynamic_array so we can resize the buffer if needed.
	// "vec" must live inside "flatbuf" and may be invalidated after this call.
	// If your FlatBuffer's root table is not the schema's root table, you should pass in your root_table type as well.
	uint8* ResizeAnyVector(const Schema& schema, uoffset_t newsize, const VectorOfAny* vec, uoffset_t num_elems, uoffset_t elem_size, dynamic_array<uint8>* flatbuf, const Object* root_table) {
		auto delta_elem		= static_cast<int>(newsize) - static_cast<int>(num_elems);
		auto delta_bytes	= delta_elem * static_cast<int>(elem_size);
		auto vec_start		= reinterpret_cast<const uint8*>(vec) - (*flatbuf);
		auto start			= static_cast<uoffset_t>(vec_start + sizeof(uoffset_t) + elem_size * num_elems);
		if (delta_bytes) {
			if (delta_elem < 0) {
				// Clear elements we're throwing away, since some might remain in the buffer
				auto size_clear = -delta_elem * elem_size;
				memset((*flatbuf) + start - size_clear, 0, size_clear);
			}
			ResizeContext(schema, start, delta_bytes, flatbuf, root_table);
			WriteScalar((*flatbuf) + vec_start, newsize);  // Length field.
														   // Set new elements to 0.. this can be overwritten by the caller.
			if (delta_elem > 0)
				memset((*flatbuf) + start, 0, delta_elem * elem_size);
		}
		return (*flatbuf) + start;
	}

	template<typename T> void ResizeVector(const Schema& schema, uoffset_t newsize, T val, const flatbuffers::Vector<T>* vec, dynamic_array<uint8>* flatbuf, const Object* root_table = nullptr) {
		auto delta_elem	= static_cast<int>(newsize) - static_cast<int>(vec->size());
		auto newelems	= ResizeAnyVector(schema, newsize, reinterpret_cast<const VectorOfAny*>(vec), vec->size(), static_cast<uoffset_t>(sizeof(T)), flatbuf, root_table);
		// Set new elements to "val".
		for (int i = 0; i < delta_elem; i++) {
			auto loc		= newelems + i * sizeof(T);
			auto is_scalar	= flatbuffers::is_scalar<T>::value;
			if (is_scalar)
				WriteScalar(loc, val);
			else  // struct
				*reinterpret_cast<T*>(loc) = val;
		}
	}

	//-----------------------------------------------------------------------------
	//	verification
	//-----------------------------------------------------------------------------

	bool VerifyStruct(flatbuffers::Verifier& v, const flatbuffers::Table& parent_table, voffset_t field_offset, const Object& obj, bool required) {
		auto offset = parent_table.GetOptionalFieldOffset(field_offset);
		if (required && !offset)
			return false;
		return !offset || v.Verify(reinterpret_cast<const uint8*>(&parent_table), offset, obj.bytesize());
	}

	bool VerifyVectorOfStructs(flatbuffers::Verifier& v, const flatbuffers::Table& parent_table, voffset_t field_offset, const Object& obj, bool required) {
		auto p = parent_table.GetPointer<const uint8*>(field_offset);
		if (required && !p)
			return false;
		return !p || v.VerifyVectorOrString(p, obj.bytesize());
	}

	// forward declare to resolve cyclic deps between VerifyObject and VerifyVector
	bool VerifyObject(flatbuffers::Verifier& v, const Schema& schema, const Object& obj, const flatbuffers::Table* table, bool required);

	bool VerifyVector(flatbuffers::Verifier& v, const Schema& schema, const flatbuffers::Table& table, const Field& vec_field) {
		ISO_ASSERT(vec_field.type()->base_type() == Vector);
		if (!table.VerifyField<uoffset_t>(v, vec_field.offset()))
			return false;

		switch (vec_field.type()->element()) {
			case None:		ISO_ASSERT(false); break;
			case UType:		return v.VerifyVector(GetFieldV<uint8>(table, vec_field));
			case Bool:
			case Byte:
			case UByte:		return v.VerifyVector(GetFieldV<int8>(table, vec_field));
			case Short:
			case UShort:	return v.VerifyVector(GetFieldV<int16>(table, vec_field));
			case Int:
			case UInt:		return v.VerifyVector(GetFieldV<int32>(table, vec_field));
			case Long:
			case ULong:		return v.VerifyVector(GetFieldV<int64>(table, vec_field));
			case Float:		return v.VerifyVector(GetFieldV<float>(table, vec_field));
			case Double:	return v.VerifyVector(GetFieldV<double>(table, vec_field));
			case String: {
				auto vec_string = GetFieldV<Offset<flatbuffers::String>>(table, vec_field);
				return v.VerifyVector(vec_string) && v.VerifyVectorOfStrings(vec_string);
			}
			case Vector: ISO_ASSERT(false); break;
			case Obj: {
				auto obj = schema.objects()->Get(vec_field.type()->index());
				if (obj->is_struct()) {
					if (!VerifyVectorOfStructs(v, table, vec_field.offset(), *obj, vec_field.required()))
						return false;
				} else {
					auto vec = GetFieldV<flatbuffers::Offset<flatbuffers::Table>>(table, vec_field);
					if (!v.VerifyVector(vec))
						return false;
					if (vec) {
						for (uoffset_t j = 0; j < vec->size(); j++) {
							if (!VerifyObject(v, schema, *obj, vec->Get(j), true))
								return false;
						}
					}
				}
				return true;
			}
			case Union: ISO_ASSERT(false); break;
			default: ISO_ASSERT(false); break;
		}

		return false;
	}

	bool VerifyObject(flatbuffers::Verifier& v, const Schema& schema, const Object& obj, const flatbuffers::Table* table, bool required) {
		if (!table)
			return !required;

		if (!table->VerifyTableStart(v))
			return false;

		for (uoffset_t i = 0; i < obj.fields()->size(); i++) {
			auto field_def = obj.fields()->Get(i);
			switch (field_def->type()->base_type()) {
				case None: ISO_ASSERT(false); break;
				case UType:
					if (!table->VerifyField<uint8>(v, field_def->offset()))
						return false;
					break;
				case Bool:
				case Byte:
				case UByte:
					if (!table->VerifyField<int8>(v, field_def->offset()))
						return false;
					break;
				case Short:
				case UShort:
					if (!table->VerifyField<int16>(v, field_def->offset()))
						return false;
					break;
				case Int:
				case UInt:
					if (!table->VerifyField<int32>(v, field_def->offset()))
						return false;
					break;
				case Long:
				case ULong:
					if (!table->VerifyField<int64>(v, field_def->offset()))
						return false;
					break;
				case Float:
					if (!table->VerifyField<float>(v, field_def->offset()))
						return false;
					break;
				case Double:
					if (!table->VerifyField<double>(v, field_def->offset()))
						return false;
					break;
				case String:
					if (!table->VerifyField<uoffset_t>(v, field_def->offset()) || !v.VerifyString(GetFieldS(*table, *field_def))) {
						return false;
					}
					break;
				case Vector:
					if (!VerifyVector(v, schema, *table, *field_def))
						return false;
					break;
				case Obj: {
					auto child_obj = schema.objects()->Get(field_def->type()->index());
					if (child_obj->is_struct()) {
						if (!VerifyStruct(v, *table, field_def->offset(), *child_obj, field_def->required()))
							return false;
					} else {
						if (!VerifyObject(v, schema, *child_obj, GetFieldT(*table, *field_def), field_def->required()))
							return false;
					}
					break;
				}
				case Union: {
					//  get union type from the prev field
					voffset_t utype_offset = field_def->offset() - sizeof(voffset_t);
					auto	  utype		   = table->GetField<uint8>(utype_offset, 0);
					if (utype != 0) {
						// Means we have this union field present
						auto fb_enum   = schema.enums()->Get(field_def->type()->index());
						auto child_obj = fb_enum->values()->Get(utype)->object();
						if (!VerifyObject(v, schema, *child_obj, GetFieldT(*table, *field_def), field_def->required()))
							return false;
					}
					break;
				}
				default: ISO_ASSERT(false); break;
			}
		}

		if (!v.EndTable())
			return false;

		return true;
	}

	// Verifies the provided flatbuffer using reflection.
	// root should point to the root type for this flatbuffer.
	// buf should point to the start of flatbuffer data.
	// length specifies the size of the flatbuffer data.

	bool Verify(const Schema& schema, const Object& root, const uint8* buf, size_t length) {
		Verifier v(buf, length);
		return VerifyObject(v, schema, root, GetAnyRoot(buf), true);
	}

}	// namespace reflection
#endif


} // namespace flatbuffers
