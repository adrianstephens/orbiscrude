#include "iso/iso_files.h"
#include "extra/date.h"

using namespace iso;

#if defined(__GNUC__)
#define ALIGN(n) __attribute__((aligned(n)))
#elif defined(_MSC_VER)
#define ALIGN(n) __declspec(align(n))
#else
#error unsupported compiler
#endif

struct UnixFileTime { uint64 t; };
template<> struct ISO::def<UnixFileTime> : ISO::VirtualT2<UnixFileTime> {
	static ISO_ptr<void>	Deref(const UnixFileTime &t) {
		return ISO_ptr<string>(0, to_string(DateTime::FromUnixTime(DateTime::Secs(t.t))));
	}
};

namespace t2 {

template<typename E, typename S> class FrozenEnum {
private:
	S		value;
public:
	operator E() const { return static_cast<E>(value); }
};

template<typename T> class FrozenPtr {
	int32	offset;
public:
	const T* Get() const {
		uintptr_t base		= (uintptr_t) this;
		uintptr_t target	= base + offset;
		uintptr_t null_mask	= (0 == offset) ? 0 : ~uintptr_t(0);
		return reinterpret_cast<T*>(target & null_mask);
	}
	operator const T* () const { return Get(); }
};

typedef FrozenPtr<const char> FrozenString;

template<typename T, int N> class FrozenArrayN {
	int32			count[N];
	FrozenPtr<T>	pointer;
public:
	int32		GetCount()	const { return count[0]; }
	const T*	GetArray()	const { return pointer; }
	const T*	begin()		const { return pointer; }
	const T*	end()		const { return pointer + count[0]; }
	const T& operator[](int32 i) const { return pointer[i];	}
};

template<typename T> class FrozenArray : FrozenArrayN<T,1> {};

template<typename T1, typename T2> class FrozenArray2 : FrozenArrayN<T2,2>  {
public:
	const FrozenArrayN<T1,1>&	GetArray1()	const { return *(FrozenArrayN<T1,1>*)this; }
	const FrozenArrayN<T2,2>&	GetArray2()	const { return *this; }
};

struct FrozenFileAndHash {
	FrozenString	filename;
	xint32			hash;
};

enum {
	kTundraHashMagic = 0x7810221e
};

#pragma pack(push, 4)
union HashDigest {
	uint64	words64[2];
	uint32	words32[4];
	uint8	data[16];
};
#pragma pack(pop)

inline int CompareHashDigests(const HashDigest& lhs, const HashDigest& rhs) {
	const uint64 l0 = lhs.words64[0];
	const uint64 r0 = rhs.words64[0];

	const int res0 = (l0 > r0) - (l0 < r0);

	const uint64 l1 = lhs.words64[1];
	const uint64 r1 = rhs.words64[1];

	const int res1 = (l1 > r1) - (l1 < r1);
	return res0 ? res0 : res1;
}

inline bool operator==(const HashDigest& lhs, const HashDigest& rhs)	{ return CompareHashDigests(lhs, rhs) == 0;	}
inline bool operator!=(const HashDigest& lhs, const HashDigest& rhs)	{ return CompareHashDigests(lhs, rhs) != 0;	}
inline bool operator<=(const HashDigest& lhs, const HashDigest& rhs)	{ return CompareHashDigests(lhs, rhs) <= 0;	}
inline bool operator<(const HashDigest& lhs, const HashDigest& rhs)		{ return CompareHashDigests(lhs, rhs) < 0;	}

// 4*xxhash hashing state
struct ALIGN(16) HashStateImpl {
	xint32	V[4][4];
};

struct ALIGN(16) HashState {
	HashStateImpl	stateImpl;
	uint64			msgSize;
	size_t			bufUsed;
	uint8			buffer[64];
	void*			debugFile;
};

namespace ScannerType {
	enum Enum {
		kCpp	= 0,
		kGeneric = 1
	};
}

struct ScannerData {
	FrozenEnum<ScannerType::Enum, int32>	type;
	FrozenArray<FrozenString>				includePaths;
	HashDigest								guid;
};

struct KeywordData {
	FrozenString	string;
	int16			stringLength;
	int8			shouldFollow;
	int8			padding;
};

struct GenericScannerData : ScannerData {
	enum {
		kFlagRequireWhitespace	= 1 << 0,
		kFlagUseSeparators		= 1 << 1,
		kFlagBareMeansSystem	= 1 << 2
	};
	uint32						flags;
	FrozenArray<KeywordData>	keywords;
};

struct NamedNodeData {
	FrozenString	name;
	int32			nodeIndex;
};

struct BuildTupleData {
	int32				configIndex;
	int32				variantIndex;
	int32				subVariantIndex;
	FrozenArray<int32>	defaultNodes;
	FrozenArray<int32>	alwaysNodes;
	FrozenArray<NamedNodeData> namedNodes;
};

struct DagFileSignature {
	FrozenString	path;
	uint8			padding[4];
	UnixFileTime	timestamp;
};

struct DagGlobSignature {
	FrozenString	path;
	HashDigest		digest;
};

struct EnvVarData {
	FrozenString	name;
	FrozenString	value;
};

struct NodeData {
	enum {
		kFlagOverwriteOutputs	= 1 << 0,
		kFlagPreciousOutputs	= 1 << 1
	};
	FrozenString					action;
	FrozenString					preAction;
	FrozenString					annotation;
	int32							passIndex;
	FrozenArray<int32>				dependencies;
	FrozenArray<int32>				backLinks;
	FrozenArray<FrozenFileAndHash>	inputFiles;
	FrozenArray<FrozenFileAndHash>	outputFiles;
	FrozenArray<FrozenFileAndHash>	auxOutputFiles;
	FrozenArray<EnvVarData>			envVars;
	FrozenPtr<ScannerData>			scanner;
	uint32							flags;
};

struct PassData {
	FrozenString PassName;
};

struct DagData {
	static const uint32	MagicNumber	= 0x1589010b ^ kTundraHashMagic;

	uint32								magicNumber;

	FrozenArray2<HashDigest,NodeData>	nodes;
	FrozenArray<PassData>				passes;
	FrozenArray2<FrozenString,xint32>	configs;
	FrozenArray2<FrozenString,xint32>	variants;
	FrozenArray2<FrozenString,xint32>	subVariants;
	FrozenArray<BuildTupleData>			buildTuples;

	int32								defaultConfigIndex;
	int32								defaultVariantIndex;
	int32								defaultSubVariantIndex;

	FrozenArray<DagFileSignature>		fileSignatures;
	FrozenArray<DagGlobSignature>		globSignatures;

	// Hashes of filename extensions to use SHA-1 digest signing instead of timestamp signing.
	FrozenArray<xint32>					shaExtensionHashes;
};

} // namespace t2

template<typename E, typename S> struct ISO::def<t2::FrozenEnum<E,S> > : ISO::VirtualT2<t2::FrozenEnum<E,S> > {
	static ISO::Browser		Deref(const t2::FrozenEnum<E,S> &t)			{ return ISO::MakeBrowser((S&)t); }
};

template<typename T> struct ISO::def<t2::FrozenPtr<T> > : ISO::VirtualT2<t2::FrozenPtr<T> > {
	static ISO::Browser		Deref(const t2::FrozenPtr<T> &t)			{ return t ? ISO::MakeBrowser(*t) : ISO::Browser(); }
};

template<> struct ISO::def<t2::FrozenString> : ISO::VirtualT2<t2::FrozenString> {
	static ISO_ptr<string>	Deref(const t2::FrozenString &t)			{ return ISO_ptr<string>(0, t); }
};

template<typename T, int N> struct ISO::def<t2::FrozenArrayN<T, N> > : ISO::VirtualT2<t2::FrozenArrayN<T, N> > {
	static uint32			Count(const t2::FrozenArrayN<T, N> &t)			{ return t.GetCount(); }
	static ISO::Browser		Index(const t2::FrozenArrayN<T, N> &t, int i)	{ return ISO::MakeBrowser(t[i]); }
};

template<typename T> struct ISO::def<t2::FrozenArray<T> > : ISO::def<t2::FrozenArrayN<T,1> > {};

template<typename T1, typename T2> struct ISO::def<t2::FrozenArray2<T1,T2> > : ISO::VirtualT2<t2::FrozenArray2<T1, T2> > {
	static uint32			Count(const t2::FrozenArray2<T1,T2> &t)			{ return 2; }
	static ISO::Browser		Index(const t2::FrozenArray2<T1,T2> &t, int i)	{ return i == 0 ? ISO::MakeBrowser(t.GetArray1()) : ISO::MakeBrowser(t.GetArray2()); }
};

ISO_DEFUSER(t2::HashDigest, uint64[2]);
ISO_DEFUSERCOMPV(t2::FrozenFileAndHash, filename, hash);
ISO_DEFUSERCOMPV(t2::EnvVarData, name, value);
ISO_DEFUSERCOMPV(t2::ScannerData, type, includePaths, guid);
ISO_DEFUSER(t2::PassData, t2::FrozenString);
ISO_DEFUSERCOMPV(t2::NamedNodeData, name, nodeIndex);
ISO_DEFUSERCOMPV(t2::BuildTupleData, configIndex, variantIndex, subVariantIndex, defaultNodes, alwaysNodes, namedNodes);
ISO_DEFUSERCOMPV(t2::DagFileSignature, path, timestamp);
ISO_DEFUSERCOMPV(t2::DagGlobSignature, path, digest);

ISO_DEFUSERCOMPV(t2::NodeData, action, preAction, annotation, passIndex, dependencies, backLinks, inputFiles, outputFiles, auxOutputFiles, envVars, scanner, flags);
ISO_DEFUSERCOMPV(t2::DagData, nodes, passes, configs, variants, subVariants, buildTuples, defaultConfigIndex, defaultVariantIndex, defaultSubVariantIndex, fileSignatures, globSignatures, shaExtensionHashes);

class TundraFileHandler : public FileHandler {
	const char*		GetExt() override { return "dag"; }
	const char*		GetDescription() override { return "Tundra DAG"; }
	int				Check(istream_ref file) override {
		file.seek(0);
		t2::DagData	data;
		return file.read(data) && data.magicNumber == t2::DagData::MagicNumber ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}

	ISO_ptr<void>	Read(tag id, istream_ref file) override;
//	bool			Write(ISO_ptr<void> p, ostream_ref file) override;
} tundra;

ISO_ptr<void> TundraFileHandler::Read(tag id, istream_ref file) {
	streamptr	len	= file.length();

	ISO_ptr<void>	p = MakePtr(ISO::getdef<t2::DagData>(), id, uint32(len));
	file.readbuff(p, len);
	return p;
}

