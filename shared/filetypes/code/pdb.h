#ifndef PDB_H
#define PDB_H

#include "msf.h"
#include "cvinfo.h"
#include "crc32.h"
#include "base/list.h"
#include "base/hash.h"
#include "base/sparse_array.h"

namespace iso {

// stream numbers
enum {
	snPDB			= 1,
	snTpi			= 2,
	snDbi			= 3,
	snIpi			= 4,
	snSpecialMax	= 5,
};

typedef int32	OFF;		// offset
typedef uint16	IFILE;		// file index
typedef uint16	IMOD;		// module index
typedef uint16	ISECT;		// section index
typedef uint32	NI;			// name index
typedef uint32	TI;			// type index
typedef uint16	TI16;		// type index

size_t			simple_size(TI ti);
const char*		dump_simple(string_accum &sa, TI ti);

template<typename R>	bool pdb_read(R &r, malloc_block &b)	{ return b.read(r, r.template get<uint32>()); }
template<typename W>	bool pdb_write(W &w, malloc_block &b)	{ return w.write(b.size32()) && check_writebuff(b, b.length()); }

template<typename R, typename K, typename V> bool pdb_read(R &r, hash_map<K, V> &map) {
	unsigned cdr	= r.template get<unsigned>();
	unsigned size	= r.template get<unsigned>();

	dynamic_bitarray<unsigned> isetPresent(r.template get<unsigned>() * 32);
	if (!read(r, isetPresent.raw()))
		return false;

	dynamic_bitarray<unsigned> isetDeleted(r.template get<unsigned>() * 32);
	if (!read(r, isetDeleted.raw()))
		return false;

	unsigned	cbitsPresent = isetPresent.count_set();
	ISO_ASSERT(cbitsPresent == cdr);

	if (cbitsPresent > size || r.remaining() < cbitsPresent * (sizeof(K) + sizeof(V)))
		return false;

	for (auto i : isetPresent.where(true)) {
		K	k;
		V	v;
		r.read(k);
		r.read(v);
		map[k] = v;
	}

	return true;
}

template<typename R, typename K, typename P, typename V> bool pdb_read(R &r, hash_map_with_key<param_element<K, P>, V> &map, P &&param) {
	unsigned cdr	= r.template get<unsigned>();
	unsigned size	= r.template get<unsigned>();

	dynamic_bitarray<unsigned> isetPresent(r.template get<unsigned>() * 32);
	if (!read(r, isetPresent.raw()))
		return false;

	dynamic_bitarray<unsigned> isetDeleted(r.template get<unsigned>() * 32);
	if (!read(r, isetDeleted.raw()))
		return false;

	unsigned	cbitsPresent = isetPresent.count_set();
	ISO_ASSERT(cbitsPresent == cdr);

	if (cbitsPresent > size || r.remaining() < cbitsPresent * (sizeof(K) + sizeof(V)))
		return false;

	for (auto i : isetPresent.where(true)) {
		typename T_noref<K>::type	k;
		V	v;
		r.read(k);
		r.read(v);
		map[make_param_element(move(k), param)] = v;
	}

	return true;
}

//-----------------------------------------------------------------------------
// hashing
//-----------------------------------------------------------------------------

template<typename P> uint32 _msft_hash1(P *p, size_t cb) {
	uint32	h	= 0;
	size_t	cl	= cb >> 2;
	P*		end	= p + cl;
	size_t	d	= cl & 7;

	switch (d) {
		do {
			d = 8;
			h ^= p[7];
		case 7: h ^= p[6];
		case 6: h ^= p[5];
		case 5: h ^= p[4];
		case 4: h ^= p[3];
		case 3: h ^= p[2];
		case 2: h ^= p[1];
		case 1: h ^= p[0];
		case 0:;
		} while ((p += d) < end);
	}

	// hash possible odd word
	uint8	*pb	= (uint8*)p;
	if (cb & 2) {
		h	^= *p & 0xffff;
		pb	+= 2;
	}

	// hash possible odd byte
	if (cb & 1)
		h ^= *pb;

	const uint32 toLowerMask = 0x20202020;
	h |= toLowerMask;
	h ^= (h >> 11);
	return h ^ (h >> 16);
}
inline uint32 msft_hash1(const void *p, size_t cb) {
	return is_aligned(p, 4)
		? _msft_hash1((const uint32*)p, cb)
		: _msft_hash1((const packed<uint32>*)p, cb);
}
inline uint32 msft_hash1(const char *sz) {
	return msft_hash1(sz, strlen(sz));
}
inline uint32 msft_hash1(const count_string &s) {
	return msft_hash1(s.begin(), s.length());
}

template<typename P> uint32 _msft_hash2(P *p, size_t cb) {
	uint32 hash = 0xb170a1bf;

	// Hash 4 characters/one uint32 at a time.
	while (cb >= 4) {
		cb -= 4;
		hash += *p++;
		hash += (hash << 10);
		hash ^= (hash >> 6);
	}

	uint8	*pb = (uint8*)p;
	// Hash the rest 1 by 1.
	while (cb > 0) {
		cb -= 1;
		hash += *p++;
		hash += (hash << 10);
		hash ^= (hash >> 6);
	}

	return hash * 1664525L + 1013904223L;
}
inline uint32 msft_hash2(const void *p, size_t cb) {
	return is_aligned(p, 4)
		? _msft_hash2((const uint32*)p, cb)
		: _msft_hash2((const packed<uint32>*)p, cb);
}
inline uint32 msft_hash2(const char *sz) {
	return msft_hash2(sz, strlen(sz));
}
inline uint32 msft_hash2(const count_string &s) {
	return msft_hash2(s.begin(), s.length());
}

//-----------------------------------------------------------------------------
// NMT: name table where hash is offset
//-----------------------------------------------------------------------------

class NMT {
	struct HDR {
		enum {
			MAGIC			= 0xeffeeffe,
			verLongHash		= 1,
			verLongHashV2	= 2,
		};
		uint32	magic;
		uint32	version;
		bool	valid() const { return magic == MAGIC && is_any(version, verLongHash, verLongHashV2); }
	};

	HDR					hdr;
	malloc_block		buf;
	dynamic_array<NI>	map;

public:
	template<typename W> bool write(W& w) {
		return w.write(hdr)
			&& pdb_write(w, buf)
			&& w.write(map.size32()) && w.write(map);
	}
	template<typename R> bool read(R &r) {
		return r.read(hdr) && hdr.valid()
			&& pdb_read(r, buf)
			&& map.read(r, r.template get<uint32>());
	}
	const char *lookup(NI ni) const {
		return ni < buf.length() ? (const char*)buf + ni : 0;
	}
	NI lookup(const char *sz) const {
		uint32	hash = hdr.version == HDR::verLongHash ? msft_hash1(sz) : msft_hash2(sz);
		return map[hash % map.size()];
	}
};

//-----------------------------------------------------------------------------
// NMTNI: name table with user-defined NIs
//-----------------------------------------------------------------------------



class NMTNI {
protected:
	struct SZO {
		uint32	off;
		auto	get_string(const memory_block &buf)	const { return str((const char*)buf + off); }
		bool	operator==(const SZO& szo)			const { return off == szo.off; }
		bool	isValid(const memory_block *buf)	const { return off < buf->length(); }

		friend	auto	get(const param_element<SZO, malloc_block*> &a)		{ return a.t.get_string(*a.p); }
		friend	uint32	hash(const param_element<SZO, malloc_block*> &a)	{ return hash(get(a)); }
	};

	hash_map_with_key<param_element<SZO, malloc_block*>, NI>	mapSzoNi;	// map from szo to ni
	hash_map<NI, SZO>	mapNiSzo;	// map from ni to szo
	malloc_block		buf;
	NI					niMax;

public:
	NMTNI() : niMax(1) {}

	void reset() {
		niMax = 1;
		mapSzoNi.clear();
		mapNiSzo.clear();
		buf.clear();
	}

	template<typename W> bool write(W& w) {
		return pdb_write(w, buf) && pdb_write(w, mapSzoNi) && w.write(niMax);
	}

	template<typename R> bool read(R &r) {
		if (!pdb_read(r, buf) || !pdb_read(r, mapSzoNi, &buf) || !r.read(niMax))
			return false;

		mapNiSzo.clear();
		for (auto &i : mapSzoNi)
			mapNiSzo[i.b] = i.a;

		return true;
	}

	const char *lookup(NI ni) const {
		if (SZO *szo = mapNiSzo.check(ni))
			return szo->get_string(buf);
		return 0;
	}

	NI lookup(const char *sz) const {
		auto i = mapSzoNi.base().find(hash(str(sz)));
		if (i == mapSzoNi.base().end())
			return 0;
		return i->b;
	}
};

class NMTNI2 : public NMTNI {
//public:
	NI		(*pfnNi)(void*);
	void*	pfnNiArg;
	NI		niMax;

	// append the name to the names buffer
	bool addSzo(const char *sz, SZO* pszo) {
		uint32	off = buf.size32();
		if (buf += const_memory_block(sz)) {
			pszo->off = off;
			return true;
		}
		return false;
	}
	// remove the name recently appended to the names buffer
	bool retractSzo(const SZO& szo) {
		buf.resize(szo.off);
		return true;
	}
	// default NI generator: returns the sequence (1, 2, ...)
	static NI niNext(void* pv) {
		NMTNI2* nmt = (NMTNI2*)pv;
		return nmt->niMax++;
	}
public:
	NMTNI2() {
		pfnNi		= niNext;
		pfnNiArg	= this;
	}
	// create a name table with client defined name index generation
	NMTNI2(NI(*pfnNi_)(void*), void* pfnNiArg_ = 0) {
		pfnNi		= pfnNi_;
		pfnNiArg	= pfnNiArg_;
	}

	NI add(const char *sz) {
		// speculatively add the argument name to the name buffer
		SZO szo;
		if (!addSzo(sz, &szo))
			return 0;

		NI &ni = mapSzoNi[make_param_element(move(szo), &buf)];
		if (ni) {
			// name already in table, remove name we just added to buffer
			retractSzo(szo);
			return ni;
		}

		if (ni = pfnNi(pfnNiArg)) {
			mapNiSzo[ni] = szo;
			// successfully added the name and its new name index
			return ni;
		}

		// failed hard; we'd better not commit these changes!
		retractSzo(szo);
		return 0;
	}

	bool remove(NI ni) {
		auto	szo = mapNiSzo.remove_value(ni);
		return szo.exists() && mapSzoNi.remove(make_param_element(move(szo), &buf));
	}
};

//-----------------------------------------------------------------------------
// PDB:
//-----------------------------------------------------------------------------

struct PDBinfo {
	enum {
		PDBImpvVC2			= 19941610,
		PDBImpvVC4			= 19950623,
		PDBImpvVC41			= 19950814,
		PDBImpvVC50			= 19960307,
		PDBImpvVC98			= 19970604,
		PDBImpvVC70			= 20000404,
		PDBImpvVC70Dep		= 19990604,		// deprecated vc70 implementation version
		PDBImpvVC80			= 20030901,
		PDBImpvVC110		= 20091201,
		PDBImpvVC140		= 20140508,
		PDBImpv				= PDBImpvVC110,
		featNoTypeMerge		= 0x4D544F4E,	// "NOTM"
		featMinimalDbgInfo	= 0x494E494D,	// "MINI"
	};
	struct PDBStream {
		uint32	impv;
		uint32	sig;
		uint32	age;
		bool	valid()	const { return between(impv, PDBImpvVC4, PDBImpvVC140); }

	};
	struct PDBStream70 : PDBStream {
		GUID	sig70;
	};

	PDBStream70	hdr;
	NMTNI		stream_names;
	NMT			names;

	union {
		uint32	flags;
		struct {
			bool	contains_id_stream	: 1;
			bool	no_type_merge		: 1;
			bool	minimal_dbg_info	: 1;
		};
	};

	bool load(MSF::reader *msf, MSF::SN sn) {
		MSF::stream_reader	mr(msf, sn);

		if (!mr.read((PDBStream&)hdr) || !hdr.valid())
			return false;

		if (hdr.impv > PDBImpvVC70Dep && !mr.read(hdr.sig70))
			return false;

		if (!stream_names.read(mr))
			return false;

		bool	stop = false;
		uint32	sig;
		while (!stop && mr.read(sig)) {
			switch (sig) {
				case PDBImpvVC110:
					contains_id_stream	= true;
					stop				= true;	// No other signature appened for vc110 PDB.
					break;

				case PDBImpvVC140:
					contains_id_stream = true;
					break;

				case featNoTypeMerge:
					no_type_merge		= true;
					break;

				case featMinimalDbgInfo:
					minimal_dbg_info	= true;
					break;
			}
		}

		if (auto sn_names = stream_names.lookup("/names"))
			MSF::stream_reader(msf, sn_names).read(names);

		return true;
	}

	PDBinfo() : flags(0) {}

	const char* StreamName(MSF::SN sn)		const	{ return stream_names.lookup(sn); }
	MSF::SN		Stream(const char *name)	const	{ return stream_names.lookup(name); }
	const char*	Name(uint32 offset)			const	{ return names.lookup(offset); }
};

//-----------------------------------------------------------------------------
//	TPI: type info
//-----------------------------------------------------------------------------

struct UDT {
	enum Kind {
		NONE, ALIAS, ENUM, STRUCT, UNION, CLASS
	};
	Kind			kind;
	uint16			count;		// count of number of elements in class
	CV::type16_t	field;		// type index of CV::LF_FIELD descriptor list
	CV::prop_t		property;	// property attribute field (prop_t)
	int64			size;		// size of structure in bytes
	const char		*name;
	const char		*unique_name;

	static bool is(const CV::Leaf *type) {
		switch (type->leaf) {
			case CV::LF_STRUCTURE:		case CV::LF_CLASS:		case CV::LF_UNION:		case CV::LF_ENUM:
			case CV::LF_STRUCTURE_16t:	case CV::LF_CLASS_16t:	case CV::LF_UNION_16t:	case CV::LF_ENUM_16t:
			case CV::LF_ALIAS:			return true;
			default:				return false;
		}
	}
	UDT(Kind kind, uint16 count, CV::type16_t field, CV::prop_t property, int64 size, const char *name) : kind(kind), count(count), field(field), property(property), size(size), name(name) {
		unique_name = property.hasuniquename ? str(name).end() + 1 : 0;
	}

	UDT(Kind kind, const CV::Class *type)		: UDT(kind, type->count, type->field, type->property, type->size, type->name()) {}
	UDT(Kind kind, const CV::Union *type)		: UDT(kind, type->count, type->field, type->property, type->size, type->name()) {}
	UDT(Kind kind, const CV::Enum *type)		: UDT(kind, type->count, type->field, type->property, simple_size(type->utype), type->name) {}
	UDT(Kind kind, const CV::Class_16t *type)	: UDT(kind, type->count, type->field, type->property, type->size, type->name()) {}
	UDT(Kind kind, const CV::Union_16t *type)	: UDT(kind, type->count, type->field, type->property, type->size, type->name()) {}
	UDT(Kind kind, const CV::Enum_16t *type)	: UDT(kind, type->count, type->field, type->property, simple_size(type->utype), type->name) {}
	UDT(Kind kind, const CV::Alias *type)		{ clear(*this); name = type->name; }

	UDT(const CV::Leaf *type) {
		if (type) {
			switch (type->leaf) {
				case CV::LF_STRUCTURE:		new(this) UDT(STRUCT,	type->as<CV::Class>()); return;
				case CV::LF_CLASS:			new(this) UDT(CLASS,	type->as<CV::Class>()); return;
				case CV::LF_UNION:			new(this) UDT(UNION,	type->as<CV::Union>()); return;
				case CV::LF_ENUM:			new(this) UDT(ENUM,		type->as<CV::Enum>()); return;
				case CV::LF_STRUCTURE_16t:	new(this) UDT(STRUCT,	type->as<CV::Class_16t>()); return;
				case CV::LF_CLASS_16t:		new(this) UDT(CLASS,	type->as<CV::Class_16t>()); return;
				case CV::LF_UNION_16t:		new(this) UDT(UNION,	type->as<CV::Union_16t>()); return;
				case CV::LF_ENUM_16t:		new(this) UDT(ENUM,		type->as<CV::Enum_16t>()); return;
				case CV::LF_ALIAS:			new(this) UDT(ALIAS,	type->as<CV::Alias>()); return;
				default:				break;
			}
		}
		clear(*this);
	}

	explicit operator bool() const {
		return kind != NONE;
	}
	bool anonymous() const {
		return	name == cstr("<unnamed-tag>")
			||	name == cstr("__unnamed")
			||	str(name).ends("::<unnamed-tag>")
			||	str(name).ends("::__unnamed")
		;
	}
	bool same(const count_string &s, bool case_sensitive) const {
		if (!name || property.fwdref || anonymous() || (property.scoped && !property.hasuniquename))
			return false;
		auto	n = property.scoped ? unique_name : name;
		return case_sensitive ? s == n : istr(s) == n;
	}
	bool same(const char* sz, bool case_sensitive) const {
		return same(count_string(sz), case_sensitive);
	}
};

struct TPI { // type info:
	enum {
		cchnV7			= 0x1000,		// for v7 and previous, we have 4k buckets
		cchnV8			= 0x3ffff,		// default to 256k - 1 buckets
		cprecInit		= 0x1000,		// start with 4k prec pointers
	};
	enum {
		impv40			= 19950410,
		impv41			= 19951122,
		impv50Interim	= 19960307,
		impv50			= 19961031,
		impv70			= 19990903,
		impv80			= 20040203,
		curImpv			= impv80,
		intvVC2			= 920924,
	};

	struct HDR_16t { // type database header, 16-bit types:
		uint32		vers;				// version which created this TypeServer
		TI16		tiMin;				// lowest TI
		TI16		tiMax;				// highest TI + 1
		uint32		cbGprec;			// count of bytes used by the gprec which follows.
		MSF::SN		snHash;				// stream to hold hash values
		// rest of file is "REC gprec[];"
	};

	struct HDR { // type database header:
		struct TpiHash {
			struct OffCb {	// offset, cb pair
				OFF		off;
				uint32	cb;
				uint32	end() const { return off + cb; }
			};
			MSF::SN		sn;				// main hash stream
			MSF::SN		snPad;			// auxilliary hash data if necessary
			uint32		cbHashKey;		// size of hash key
			uint32		cHashBuckets;	// how many buckets we have
			OffCb		offcbHashVals;	// offcb of hashvals
			OffCb		offcbTiOff;		// offcb of (TI,OFF) pairs
			OffCb		offcbHashAdj;	// offcb of hash head list, maps (hashval,ti), where ti is the head of the hashval chain.

			auto	open_chunk(MSF::reader *msf, OffCb &chunk) const {
				return MSF::stream_reader(msf, sn, chunk.off);
			}
			malloc_block		read_chunk(MSF::reader *msf, const OffCb &chunk) const {
				malloc_block	buf(chunk.cb);
				if (msf->ReadStream(sn, chunk.off, buf, chunk.cb) != chunk.cb)
					return empty;
				return buf;
			}

		};
		uint32		vers;				// version which created this TypeServer
		uint32		cbHdr;				// size of the header, allows easier upgrading and backwards compatibility
		TI			tiMin;				// lowest TI
		TI			tiMax;				// highest TI + 1
		uint32		cbGprec;			// count of bytes used by the gprec which follows.
		TpiHash		tpihash;			// hash stream schema
		// rest of file is "REC gprec[];"
	};

	struct TI_OFF {
		TI			ti;					// first ti at this offset
		OFF			off;				// offset of ti in stream
	};

	HDR				hdr;
	malloc_block	tioff_buffer;		// buffer that holds all TI_OFF pairs
	bool			use_v8hash;

	hash_map<NI,TI>				ni2ti;			// map the head of UDT chain to a TI, for when the UDT chain needs a particular TI as the current type schema for a UDT
	hash_map<uint32,TI>			hash2ti;		// map the head of UDT chain to a TI, for when the UDT chain needs a particular TI as the current type schema for a UDT
	dynamic_array<CV::TYPTYPE*>		type_array;		// map from TI to record

	mutable sparse_array<slist<TI>, uint32, uint32>	hash_chains;	// map from record hash to its hash bucket chain
	mutable dynamic_array<malloc_block>				type_blocks;

	static uint32 hash_full(const CV::TYPTYPE *type, bool use_v8hash) {
		switch (type->leaf) {
			case CV::LF_ALIAS:
				return msft_hash1(type->as<CV::Alias>()->name);

			case CV::LF_STRUCTURE:
			case CV::LF_CLASS:
			case CV::LF_UNION:
			case CV::LF_ENUM:
			case CV::LF_STRUCTURE_16t:
			case CV::LF_CLASS_16t:
			case CV::LF_UNION_16t:
			case CV::LF_ENUM_16t: {
				UDT		udt(type);
				if (!udt.property.fwdref && !udt.anonymous()) {
					if (!udt.property.scoped)
						return msft_hash1(udt.name);		// hash on udt name only

					if (udt.property.hasuniquename)
						return msft_hash1(udt.unique_name);	// hash on udt's unique name only
				}
			}
			// fall through
			default:
				if (use_v8hash)
					return CRC_def<uint32, 0xEDB88320, true, false>::calc(type, type->size());

				return msft_hash1(type, type->size());

			case CV::LF_UDT_SRC_LINE:
			case CV::LF_UDT_MOD_SRC_LINE:
				return msft_hash1(&type->as<CV::UdtSrcLine>()->type, sizeof(TI));
		}
	}

	uint32 hash(const CV::TYPTYPE *type) const {
		return hash_full(type, use_v8hash) % hdr.tpihash.cHashBuckets;
	}

	uint32 hash(const count_string &s) const {
		return msft_hash1(s) % hdr.tpihash.cHashBuckets;
	}

	bool load_ti_block(MSF::reader *msf, MSF::SN sn, TI ti) const {
		// find blk that has the ti of interest
		TI_OFF	*p		= first_not(make_range<TI_OFF>(tioff_buffer), [ti](const TI_OFF &x) { return x.ti <= ti; });
		TI		tiBlk	= p[-1].ti;
		TI		tiBlk1	= p == tioff_buffer.end() ? hdr.tiMax : p->ti;

		ISO_ASSERT(ti >= tiBlk && ti < tiBlk1);

		OFF		off		= p[-1].off;
		OFF		off1	= p == tioff_buffer.end() ? hdr.cbGprec : p->off;
		uint32	cb		= off1 - off;

		malloc_block&	block	= type_blocks[p - tioff_buffer - 1];
		if (msf->ReadStream(sn, off + hdr.cbHdr, block.resize(cb), cb) != cb)
			return false;

		auto	*dest = type_array + tiBlk - hdr.tiMin, *end = type_array + tiBlk1 - hdr.tiMin;
		for (auto &type : make_next_range<CV::TYPTYPE>(block)) {
			if (dest >= end)
				return false;
			*dest++ = &type;
		}
		ISO_ASSERT(dest == end);
		return true;
	}

	void init_hash_chains(MSF::reader *msf, MSF::SN sn) const {
		hash_chains.resize(hdr.tpihash.cHashBuckets);

		if (hdr.tpihash.sn != MSF::snNil && hdr.tpihash.offcbHashVals.cb) {

			// read in the previous hash value stream
			malloc_block bufHash = hdr.tpihash.read_chunk(msf, hdr.tpihash.offcbHashVals);
			if (hdr.tpihash.cbHashKey == 2) {
				uint16	*ph	= bufHash;
				for (TI ti = hdr.tiMin; ti < hdr.tiMax; ti++)
					hash_chains[*ph++]->push_front(ti);
			} else {
				uint32	*ph	= bufHash;
				for (TI ti = hdr.tiMin; ti < hdr.tiMax; ti++)
					hash_chains[*ph++]->push_front(ti);
			}

		} else {
			for (TI ti = hdr.tiMin; ti < hdr.tiMax; ti++)
				hash_chains[hash(load_ti(msf, sn, ti))]->push_front(ti);
		}

		// make adjustments for records that need to be at the head of a hash chain
		for (auto i : with_iterator(hash2ti)) {
			uint32	h	= i.hash();

			// find the ti we want to be at the head of the chain and put it there.
			TI      ti	= *i;
			for (auto j : with_iterator(hash_chains[h]->insertable())) {
				if (*j == ti) {
					hash_chains[h]->push_front(j.unlink());
					ti	= 0;
					break;
				}
			}
			ISO_ASSERT(ti == 0);
		}
	}

	CV::TYPTYPE *load_ti(MSF::reader *msf, MSF::SN sn, TI ti) const {
		return between(ti, hdr.tiMin, hdr.tiMax - 1) && (type_array[ti - hdr.tiMin] || load_ti_block(msf, sn, ti)) ? type_array[ti - hdr.tiMin] : 0;
	}

	bool load(const PDBinfo &info, MSF::reader *msf, MSF::SN sn) {
		MSF::stream_reader	mr(msf, sn);

		if (!mr.read(hdr))
			return false;

		switch (hdr.vers) {
			case curImpv:
				if (hdr.tpihash.cbHashKey != 4 || hdr.tpihash.cHashBuckets < cchnV7 || hdr.tpihash.cHashBuckets > cchnV8)
					return false;
				use_v8hash	= true;
				break;

			case impv70:
			case impv50:
				if (hdr.tpihash.cbHashKey != 2 || hdr.tpihash.cHashBuckets != cchnV7)
					return false;
				break;

			case impv50Interim:
			case impv41:
			case impv40:
			case intvVC2:
				break;

			default:
				return false;
		}

		type_array.resize(hdr.tiMax - hdr.tiMin, 0);

		if (hdr.vers >= impv40 && hdr.tpihash.sn != MSF::snNil) {
			tioff_buffer	= hdr.tpihash.read_chunk(msf, hdr.tpihash.offcbTiOff);

			if (hdr.vers <= impv41) {
				for (auto &i : make_range<TI_OFF>(tioff_buffer))
					i.ti &= 0xffff;
			}

			type_blocks.resize(tioff_buffer.size32() / sizeof(TI_OFF));
		}

		// read in any adjustment values that tell us which records need to be at the head of a hash chain.
		if (hdr.tpihash.sn != MSF::snNil && hdr.tpihash.offcbHashAdj.cb) {
			auto	reader = hdr.tpihash.open_chunk(msf, hdr.tpihash.offcbHashAdj);
			pdb_read(reader, ni2ti);
			if (reader.tell() != hdr.tpihash.offcbHashAdj.end())
				return false;

			for (auto i : with_iterator(ni2ti)) {
				// get the hash value from the name
				const char *sz = info.names.lookup(i.hash());
				if (!sz || *sz == 0)
					return false;

				hash2ti[hash(count_string(sz))] = *i;
			}
		}

		// Update the version to impv70 if less than that.
		if (hdr.vers < impv70)
			hdr.vers = impv70;

		return true;
	}

	TPI() : use_v8hash(false) {}

	TI		MinTI()	const { return hdr.tiMin; }
	TI		MaxTI()	const { return hdr.tiMax; }
};

//-----------------------------------------------------------------------------
// GSI: global symbols
//-----------------------------------------------------------------------------

struct GSI {
	struct HR {
		int32	off;
		int32	refs;
		const CV::SYMTYPE	*get(const void *syms) const { return off ? (const CV::SYMTYPE*)((uint8*)syms + (off - 1)) : 0; }
	};
	struct HR32 {
		int32	next;
		int32	off;
		int32	refs;
	};

	struct GSIHashHdr {
		enum {
			hdrSignature		= -1,
			GSIHashSCImpvV70	= 0xeffe0000 + 19990810,
			GSIHashSCImpv		= GSIHashSCImpvV70,
			hdrVersion			= GSIHashSCImpv,
		};
		uint32		verSignature;
		uint32		verHdr;
		uint32		cbHr;
		uint32		cbBuckets;

		bool valid() const { return verSignature == hdrSignature && verHdr == hdrVersion; }
	};

	dynamic_array<HR>		hr;
	dynamic_array<int32>	hr_buckets;
	uint32					bucket_mod;

	bool load_hash(MSF::stream_reader &mr, uint32 hash_size, uint32 num_buckets) {
		GSIHashHdr gsiHdr;
		if (!mr.read(gsiHdr))
			return false;

		if (gsiHdr.valid()) {
			int	cEntries = gsiHdr.cbHr / sizeof(HR);
			hr.read(mr, cEntries);
			bucket_mod	= num_buckets;

//			hr_buckets.resize(num_buckets + 1);
//			int32	*out	= hr_buckets.begin();

			if (gsiHdr.cbBuckets) {
				dynamic_bitarray<unsigned>	set(num_buckets + 1);
				if (!read(mr, set.raw()))
					return false;

				uint32	top_bit = set.highest(true);
				hr_buckets.resize(next_pow2(top_bit));

				dynamic_array<int32>		offs;
				if (!offs.read(mr, set.count_set()))
					return false;

				int32	*in		= offs.begin();
				int32	*out	= hr_buckets.begin();
				for (unsigned i	= 0; ; ++i) {
					*out++ = *in;
					if (set.test(i)) {
						if (++in == offs.end())
							break;
					}
				}
				while (out < hr_buckets.end())
					*out++ = cEntries * sizeof(HR32);
			}


		} else {
			mr.seek_cur(-int(sizeof(gsiHdr)));
			int		cEntries	= (hash_size - sizeof(OFF) * (num_buckets + 1)) / sizeof(HR);
			hr.read(mr, cEntries);
			if (!hr_buckets.read(mr, num_buckets + 1))
				return false;
		}

		for (auto &i : hr_buckets)
			i /= sizeof(HR32);

		return true;
	}

	bool load(MSF::stream_reader &&mr, uint32 num_buckets) {
		return load_hash(mr, mr.length(), num_buckets);
	}

	typedef param_iterator<const HR*, const void*>	iterator;
	friend auto&	get(const param_element<HR&, const void*> &x)		{ return *x.t.get(x.p); }
	friend auto&	get(const param_element<const HR&, const void*> &x)	{ return *x.t.get(x.p); }
	friend auto		get(const param_element<uint32, const GSI*> &x)		{ return x.p->bucket(x.t); }

	const HR*			bucket_begin(uint32 i)	const { return hr_buckets ? hr + hr_buckets[i] : nullptr; }
	range<const HR*>	bucket(uint32 i)		const { return range<const HR*>(bucket_begin(i), bucket_begin(i + 1)); }
	auto				buckets()				const { return with_param(int_range<uint32>(0, hr_buckets.size32() - 1), this); }
	const HR*			end()					const { return hr.end(); }
};

//-----------------------------------------------------------------------------
// PSGS: public symbols
//-----------------------------------------------------------------------------

struct PSGS : GSI {
	struct PSGSIHDR {
		uint32		cbSymHash;
		uint32		cbAddrMap;
		uint32		nThunks;
		uint32		cbSizeOfThunk;
		ISECT		isectThunkTable;
		OFF			offThunkTable;
		uint32		nSects;
	};

	PSGSIHDR			hdr;
	dynamic_array<int>	addr_map;

	bool load(MSF::stream_reader &&mr, uint32 num_buckets) {
		if (!mr.read(hdr))
			return false;

		if (!load_hash(mr, hdr.cbSymHash, num_buckets))
			return false;

		return addr_map.read(mr, hdr.cbAddrMap / sizeof(int));
	}
};

//-----------------------------------------------------------------------------
// MOD: one module's debug info
//-----------------------------------------------------------------------------

struct SC40 {
	ISECT	isect;
	OFF		off;
	uint32	cb;
	uint32	characteristics;
	IMOD	imod;

	SC40() : isect(~0_u16), off(0), cb(~0u), characteristics(0), imod(~0_u16) {}

	CV::segmented32	addr() const { return {uint32(off), isect};  }

	inline int compare(ISECT isect2, OFF off2) const {
		return	isect2 == isect ? (off2 < off ? -1 : off2 - off < cb ? 0 : 1)
			:	isect2 - isect;
	}
	inline int compare(const CV::segmented32 addr) const {
		return compare(addr.seg, addr.off);
	}
	friend int compare(const SC40 &a, const SC40 &b) {
		return b.compare(a.isect, a.off);
	}
};

struct SC : public SC40 {
	uint32	data_crc;
	uint32	reloc_crc;

	SC() : data_crc(0), reloc_crc(0) {}
	SC(const SC40& sc40) : SC40(sc40), data_crc(0), reloc_crc(0) {}

	inline bool match(IMOD imod2, uint32 cb2, uint32 data_crc2, uint32 reloc_crc2) const {
		return imod			== imod2	// only interested in this one imod
			&& data_crc		== data_crc2
			&& reloc_crc	== reloc_crc2
			&& cb			== cb2;
	}
	inline bool match(IMOD imod2, uint32 cb2, uint32 data_crc2, uint32 reloc_crc2, uint32 characteristics2) const {
		return match(imod2, cb2, data_crc2, reloc_crc2) && characteristics == characteristics2;
	}

	friend bool operator==(const SC &sc1, const SC &sc2) { return sc1.match(sc2.imod, sc2.cb, sc2.data_crc, sc2.reloc_crc); }

};

struct SC2 : public SC {
	uint32	isect_coff;
	SC2() : isect_coff(0) {}
	SC2(const SC &sc) : SC(sc), isect_coff(0) {}
};

struct MODI50 {
	uint32		pmod;
	SC40		sc;					// this module's first section contribution
	uint16		fWritten : 1;		// TRUE if mod has been written since DBI opened
	uint16		unused : 7;			// spare
	uint16		iTSM : 8;			// index into TSM list for this mods server
	MSF::SN		sn;					// SN of module debug info (syms, lines, fpo), or snNil
	uint32		cbSyms;				// size of local symbols debug info in stream sn
	uint32		cbLines;			// size of line number debug info in stream sn
	uint32		cbFpo;				// size of frame pointer opt debug info in stream sn
	IFILE		ifileMax;			// number of files contributing to this module
	uint32		mpifileichFile;		// array [0..ifileMax) of offsets into dbi.bufFilenames
	char		rgch[1];			// szModule followed by szObjFile

	const char	*Module()	const	{ return (const char*)rgch; }
	const char	*ObjFile()	const	{ return string_end(Module()) + 1; }
	MODI50		*next()				{ return (MODI50*)align(string_end(ObjFile()) + 1, 4); }
};

struct MODI {
	uint32		pmod;
	SC			sc;					// this module's first section contribution
	uint16		fWritten : 1;		// TRUE if mod has been written since DBI opened
	uint16		fECEnabled : 1;		// TRUE if mod has EC symbolic information
	uint16		unused : 6;			// spare
	uint16		iTSM : 8;			// index into TSM list for this mods server
	MSF::SN		sn;					// SN of module debug info (syms, lines, fpo), or snNil
	uint32		cbSyms;				// size of local symbols debug info in stream sn
	uint32		cbLines;			// size of line number debug info in stream sn
	uint32		cbC13Lines;			// size of C13 style line number info in stream sn
	IFILE		ifileMax;			// number of files contributing to this module
	uint32		mpifileichFile;		// array [0..ifileMax) of offsets into dbi.bufFilenames

	struct ECInfo {
		NI		niSrcFile;			// NI for src file name
		NI		niPdbFile;			// NI for path to compiler PDB
	} ecInfo;

	char rgch[1];              		// szModule followed by szObjFile

	const char	*Module()	const	{ return (const char*)rgch; }
	const char	*ObjFile()	const	{ return string_end(Module()) + 1; }
	MODI		*next()				{ return (MODI*)align(string_end(ObjFile()) + 1, 4); }
	const MODI	*next()		const	{ return (const MODI*)align(string_end(ObjFile()) + 1, 4); }

	MODI(const MODI50 &modi50) {
		pmod				= 0;
		sc					= modi50.sc;
		fWritten			= modi50.fWritten;
		fECEnabled			= 0;
		unused				= 0;
		iTSM				= modi50.iTSM;
		sn					= modi50.sn;
		cbSyms				= modi50.cbSyms;
		cbLines				= modi50.cbLines;
		cbC13Lines			= 0;	// field was formerly cbFpo and was never used in 50 pdbs
		ifileMax			= modi50.ifileMax;
		mpifileichFile		= 0;
		ecInfo.niSrcFile	= 0;
		ecInfo.niPdbFile	= 0;
		memcpy(rgch, modi50.rgch, strlen(modi50.Module()) + strlen(modi50.ObjFile()) + 2);
	}
};

struct MOD {
	MODI			*modi;
	malloc_block	data;

	bool	load(MSF::reader *msf) {
		if (modi->sn != MSF::snNil)
			data = malloc_block::unterminated(MSF::stream_reader(msf, modi->sn).me());
		return true;
	}

	MOD(MODI *modi) : modi(modi) {}
	auto			operator->()			const	{ return modi; }
	auto			Symbols()				const	{ return modi->cbSyms ? make_next_range<const CV::SYMTYPE>(data.slice(4, modi->cbSyms - 4)) : none; }
	auto			Lines()					const	{ return modi->cbLines ? data.slice(modi->cbSyms, modi->cbLines) : none; }
	auto			C13Lines()				const	{ return modi->cbC13Lines ? data.slice(modi->cbSyms + modi->cbLines, modi->cbC13Lines) : none; }
	auto			SubStreams()			const	{ return make_next_range<const CV::SubSection>(C13Lines()); }
	const char		*Module()				const	{ return modi->Module(); }
	const char		*ObjFile()				const	{ return modi->ObjFile(); }
	const CV::SYMTYPE	*GetSymbol(uint32 off)	const	{ ISO_ASSERT(off < modi->cbSyms); return data + off; }
};

//-----------------------------------------------------------------------------
// DBI: Debug Information
//-----------------------------------------------------------------------------

struct DBI {
	enum {
		DBIImpvV41 = 930803,
		DBIImpvV50 = 19960307,
		DBIImpvV60 = 19970606,
		DBIImpvV70 = 19990903,
		DBIImpvV110 = 20091201,
		DBIImpv = DBIImpvV70,
	};

	enum {
		DBISCImpvV60	= 0xeffe0000 + 19970605,
		DBISCImpv		= DBISCImpvV60,
		DBISCImpv2		= 0xeffe0000 + 20140516,
	};

	struct DBIHdr {
		MSF::SN	snGSSyms;
		MSF::SN	snPSSyms;
		MSF::SN	snSymRecs;
		uint32	cbGpModi;	// size of rgmodi sliceeam
		uint32	cbSC;		// size of Section Contribution sliceeam
		uint32	cbSecMap;
		uint32	cbFileInfo;
	};

	struct NewDBIHdr {
		enum {
			hdrSignature = -1,
			hdrVersion = DBIImpv,
		};
		uint32		verSignature;
		uint32		verHdr;
		uint32		age;
		MSF::SN		snGSSyms;

		union {
			struct {
				uint16	usVerPdbDllMin : 8; // minor version and
				uint16	usVerPdbDllMaj : 7; // major version and
				uint16	fNewVerFmt : 1;		// flag telling us we have rbld stored elsewhere (high bit of original major version) that built this pdb last
			} vernew;
			struct {
				uint16	usVerPdbDllRbld : 4;
				uint16	usVerPdbDllMin : 7;
				uint16	usVerPdbDllMaj : 5;
			} verold;
			uint16	usVerAll;
		};

		MSF::SN		snPSSyms;
		uint16		usVerPdbDllBuild;	// build version of the pdb dll that built this pdb last.
		MSF::SN		snSymRecs;
		uint16		usVerPdbDllRBld;	// rbld version of the pdb dll
		uint32		cbGpModi;			// size of rgmodi sliceeam
		uint32		cbSC;				// size of Section Contribution sliceeam
		uint32		cbSecMap;
		uint32		cbFileInfo;
		uint32		cbTSMap;			// size of the Type Server Map sliceeam
		uint32		iMFC;				// index of MFC type server
		uint32		cbDbgHdr;			// size of optional DbgHdr info appended to the end of the stream
		uint32		cbECInfo;			// number of bytes in EC sliceeam, or 0 if EC no EC enabled Mods

		// protected by m_csForHdr
		struct _flags {
			uint16	fIncLink : 1;		// true if linked incrmentally (really just if ilink thunks are present)
			uint16	fStripped : 1;		// true if PDB::CopyTo stripped the private data out
			uint16	fCTypes : 1;		// true if this PDB is using CTypes.
			uint16	unused : 13;		// reserved, must be 0.
		} flags;

		uint16		wMachine;			// machine type
		uint32		rgulReserved[1];	// pad out to 64 bytes for future growth.

		bool	valid()	const { return verSignature == 0xffffffff && verHdr == hdrVersion; }

	};
	struct SectionMapHeader {
		enum {
			Read				= 1 << 0,	// Segment is readable.
			Write				= 1 << 1,	// Segment is writable.
			Execute				= 1 << 2,	// Segment is executable.
			AddressIs32Bit		= 1 << 3,	// Descriptor describes a 32-bit linear address.
			IsSelector			= 1 << 8,	// Frame represents a selector.
			IsAbsoluteAddress	= 1 << 9,	// Frame represents an absolute address.
			IsGroup				= 1 << 10	// If set, descriptor represents a group.
		};
		struct Entry {
			uint16	Flags;
			uint16	Ovl;			// Logical overlay number
			uint16	Group;			// Group index into descriptor array.
			uint16	Frame;
			uint16	SectionName;	// Byte index of segment / group name in string table, or 0xFFFF.
			uint16	ClassName;		// Byte index of class in string table, or 0xFFFF.
			uint32	Offset;			// Byte offset of the logical segment within physical segment.  If group is set in flags, this is the offset of the group.
			uint32	SectionLength;	// Byte count of the segment or group.
		};

		uint16	Count;		// Number of segment descriptors
		uint16	LogCount;	// Number of logical segment descriptors
		Entry	entry[0];
	};

	enum {
		FPOData,					// The data in the referenced stream is a debug data directory of type IMAGE_DEBUG_TYPE_FPO
		ExceptionData,				// The data in the referenced stream is a debug data directory of type IMAGE_DEBUG_TYPE_EXCEPTION.
		FixupData,					// The data in the referenced stream is a debug data directory of type IMAGE_DEBUG_TYPE_FIXUP.
		OmapToSrcData,				// The data in the referenced stream is a debug data directory of type IMAGE_DEBUG_TYPE_OMAP_TO_SRC. This is used for mapping addresses between instrumented and uninstrumented code.
		OmapFromSrcData,			// The data in the referenced stream is a debug data directory of type IMAGE_DEBUG_TYPE_OMAP_FROM_SRC. This is used for mapping addresses between instrumented and uninstrumented code.
		SectionHeaderData,			// A dump of all section headers from the original executable.
		TokenRIDMap,				// The layout of this stream is not understood, but it is assumed to be a mapping from CLR Token to CLR Record ID. Refer to ECMA 335 for more information.
		Xdata,						// A copy of the .xdata section from the executable.
		Pdata,						// This is assumed to be a copy of the .pdata section from the executable, but that would make it identical to DbgStreamArray[1]. The difference between these two indices is not well understood.
		NewFPOData,					// The data in the referenced stream is a debug data directory of type IMAGE_DEBUG_TYPE_FPO. It is not clear how this differs from DbgStreamArray[0], but in practice all observed PDB files have used the “new” format rather than the “old” format.
		OriginalSectionHeaderData,	// Assumed to be similar to DbgStreamArray[5], but has not been observed in practice.
		dbghdr_num
	};

	NewDBIHdr		hdr;

	malloc_block	dbghdr[dbghdr_num];
	malloc_block	syms;
	malloc_block	modi_buffer;
	malloc_block	secmap;
	NMT				nmt_files;
	NMT				nmt_EC;
	GSI				gs;
	PSGS			psgs;

	dynamic_array<MOD>		mods;
	hash_map<cstring, NI>	filename_map;
	malloc_block			filename_buf;
	dynamic_array<SC2>		sec_contribs;

	NI	filename_id(const char *fn) const {
		return filename_map[fn].or_default(0);
	}

	NI	add_filename(const char *fn) {
		NI &ni = filename_map[fn];
		if (!ni) {
			ni = filename_buf.size32();
			filename_buf += const_memory_block(fn, strlen(fn) + 1);
		}
		return ni;
	}

	NI	get_filename(const char *fn) const {
		return filename_map[fn];
	}

	bool load_fileinfo(const memory_block &buf) {
		uint8	*pb		= buf;
		uint32	num_mod = mods.size32();

		if (*((IMOD*&)pb)++ != num_mod)
			return false;

		uint32 total_refs = *((uint16*&)pb)++;		// overridden later

		if (num_mod == 0)
			return total_refs == 0;

		uint16* nrefs	= (uint16*)pb + num_mod;

		// total_refs read above could be wrong - if it was >= 0x10000 when writing. Just recompute it with nrefs
		total_refs = 0;
		for (IMOD imod = 0; imod < num_mod; imod++)
			total_refs += nrefs[imod];

		uint32		*files	= (uint32*)(nrefs + num_mod);
		const char	*names	= (const char*)(files + total_refs);

		for (IMOD imod = 0; imod < num_mod; imod++) {
			if (nrefs[imod]) {
				auto	mod_files = CV::GetFirstSubStream<CV::DEBUG_S_FILECHKSMS>(mods[imod].SubStreams())->entries();
				ISO_ASSERT(mod_files.size32() == nrefs[imod]);
				for (auto &i : mod_files)
					unconst(i.name) = add_filename(names + *files++);
			}
		}
		return true;
	}

	bool load(const PDBinfo &info, MSF::reader *msf, MSF::SN sn) {
		MSF::stream_reader	mr(msf, sn);

		if (!mr.read(hdr))
			return true;	// allow for empty stream

		syms = msf->GetStream(hdr.snSymRecs);

		uint32	num_buckets = info.minimal_dbg_info ? 0x3ffff : 0x1000;

		if (!gs.load(MSF::stream_reader(msf, hdr.snGSSyms), num_buckets))
			return false;

		if (!psgs.load(MSF::stream_reader(msf, hdr.snPSSyms), num_buckets))
			return false;

		// read in the gpmodi sliceeam
		if (hdr.cbGpModi > 0) {
			// load gpmodi
			bool fSzModNameCheck = false;

			if (NewDBIHdr::hdrVersion < DBIImpvV60) {
				// read in v5 modi into temp table and do initial alloc of modi which will hold the converted v6 modi
				malloc_block	bufGpV5modi(mr, hdr.cbGpModi);

				uint32	cb = 0;
				for (auto &i : make_next_range<MODI50>(bufGpV5modi))
					cb += align(sizeof(MODI) + strlen(i.Module()) + 1 + strlen(i.ObjFile()) + 1, 4);

				modi_buffer.resize(cb);
				hdr.cbGpModi = cb;

				// pass thru v5 modi table and copy/convert into v6 modi table
				void	*end = modi_buffer;
				for (auto &i : make_next_range<MODI50>(bufGpV5modi)) {
					MODI	*m = new(end) MODI(i);
					end = m->next();
				}

				hdr.cbGpModi = modi_buffer.size32();

			} else {
				modi_buffer.read(mr, hdr.cbGpModi);
			}

			// build mods
			for (auto &i : make_next_range<MODI>(modi_buffer)) {
				mods.push_back(&i);
				i.pmod				= 0;
				i.fWritten			= false;
				i.ifileMax			= 0;
				i.mpifileichFile	= 0;
			}
		}
		ISO_ASSERT(mr.tell() == sizeof(hdr) + hdr.cbGpModi);

		// read in the Section Contribution sliceeam
		uint32	SCversion	= mr.get<uint32>();
		if (SCversion != DBISCImpv && SCversion != DBISCImpv2) {
			dynamic_array<SC40>	sc40;
			sc40.read(mr, hdr.cbSC / sizeof(SC40));
			sec_contribs = sc40;

		} else if (SCversion == DBISCImpv) {
			dynamic_array<SC>	sc;
			sc.read(mr, (hdr.cbSC - sizeof(SCversion)) / sizeof(SC));
			sec_contribs = sc;

		} else {
			sec_contribs.read(mr, (hdr.cbSC - sizeof(SCversion)) / sizeof(SC2));
		}

		// read the Section Map sliceeam
		secmap.read(mr, hdr.cbSecMap);

		// load the modules
		for (auto &i : mods) {
			if (!i.load(msf))
				return false;
		}

		if (hdr.cbFileInfo > 0 && !load_fileinfo(malloc_block(mr, hdr.cbFileInfo)))
			return false;

		// skip the TSM sliceeam
		mr.seek_cur(hdr.cbTSMap);

		// read in the EC sliceeam
		if (hdr.cbECInfo)
			nmt_EC.read(mr);

		uint32	unknown = mr.get<uint32>();

		// read streams from dbghdr
		malloc_block	dbghdr_streams(mr, hdr.cbDbgHdr);
		uint16			*p = dbghdr_streams;
		for (auto &i : dbghdr) {
			if (*p && *p != 0xffff)
				i = msf->GetStream(*p);
			++p;
		}

		return true;
	}

	MOD*			HasModule(uint32 i)		const { return i && i <= mods.size32() ? &mods[i - 1] : 0; }
	MOD&			GetModule(uint32 i)		const { ISO_ASSERT(i && i <= mods.size32()); return mods[i - 1]; }
	const CV::SYMTYPE*	GetSymbol(uint32 offset)const { return syms + offset; }
	const char*		FileName(uint32 offset)	const { return filename_buf + offset; }

	auto			GlobalSymbols()			const { return with_param2(make_range(gs.bucket_begin(0), gs.end()), (const void*)syms); }
	auto			PublicSymbols()			const { return with_param2(make_range(psgs.bucket_begin(0), psgs.end()), (const void*)syms); }
	auto			Modules()				const { return make_rangec(mods); }
	auto			Symbols()				const { return make_next_range<CV::SYMTYPE>(syms); }
	auto			Sections()				const { return make_range<const IMAGE_SECTION_HEADER>(dbghdr[SectionHeaderData]); }
	auto&			Section(int i)			const { return Sections()[i - 1]; }

	const IMAGE_SECTION_HEADER*	FindSection(uint32 rva) const {
		auto	*sect = lower_boundc(Sections(), rva, [](const IMAGE_SECTION_HEADER &sect, uint32 rva) { return sect.VirtualAddress < rva; });
		return sect > dbghdr[SectionHeaderData] && (sect < dbghdr[SectionHeaderData].end() || rva - sect[-1].VirtualAddress < sect[-1].SizeOfRawData) ? sect - 1 : nullptr;
	}
	CV::segmented32	ToSegmented(uint32 rva) const {
		if (auto sect = FindSection(rva))
			return {rva - sect->VirtualAddress, uint16(sect - dbghdr[SectionHeaderData] + 1)};
		return {rva, 0};
	}
	uint32			FromSegmented(const CV::segmented32 &addr) const {
		return Section(addr.seg).VirtualAddress + addr.off;
	}
};

//-----------------------------------------------------------------------------
//	PDB
//-----------------------------------------------------------------------------

struct PDB_types : TPI {
	ref_ptr<MSF::reader> msf;

	bool		load(const PDBinfo &info, MSF::reader *msf) {
		this->msf = msf;
		return TPI::load(info, msf, snTpi);
	}

	auto		Types()									const	{ return with_param(int_range(MinTI(), MaxTI()), this); }
	CV::TYPTYPE* GetType(TI ti)							const	{ return load_ti(msf, snTpi, ti); }
	size_t		GetTypeSize(TI ti)						const;
	size_t		GetTypeSize(const CV::Leaf &type)		const;
	uint32		GetTypeAlignment(TI ti)					const;
	uint32		GetTypeAlignment(const CV::Leaf &type)	const;

	TI			LookupUDT(const count_string &s, bool case_sensitive = true) const {
		if (!hash_chains)
			init_hash_chains(msf, snTpi);

		for (auto &i : hash_chains[hash(s)].or_default()) {
			if (UDT(load_ti(msf, snTpi, i)).same(s, case_sensitive))
				return i;
		}
		return 0;
	}
	TI			LookupUDT(const char *sz, bool case_sensitive = true) const {
		return LookupUDT(count_string(sz), case_sensitive);
	}

	friend	auto	get(const param_element<TI, const PDB_types*> &a)	{ return a.p->GetType(a.t); }
};


struct PDB : PDB_types, DBI {
	PDB() {}
	PDB(PDB &&b) = default;

	bool	load(const PDBinfo &info, MSF::reader *msf) {
		return PDB_types::load(info, msf) && DBI::load(info, msf, snDbi);
	}

	TI				GetSymbolTI(const CV::SYMTYPE *sym)		const;
	CV::TYPTYPE*	GetSymbolType(const CV::SYMTYPE *sym)	const	{ return GetType(GetSymbolTI(sym)); }

	const CV::SYMTYPE*	LookupSym(const count_string &s, bool case_sensitive = true) const {
		auto	h = (msft_hash1(s) % psgs.bucket_mod) & (psgs.hr_buckets.size32() - 1);
		for (auto &i : psgs.bucket(h)) {
			if (const CV::SYMTYPE *sym = i.get(syms)) {
				auto	n = get_name(sym);
				if (case_sensitive ? s == n : istr(s) == n)
					return sym;
			}
		}
		return 0;
	}
	const CV::SYMTYPE*	LookupSym(const char *sz, bool case_sensitive = true) const {
		return LookupSym(count_string(sz), case_sensitive);
	}
};

string_accum& operator<<(string_accum &sa, const CV::SYMTYPE &sym);
string_accum& operator<<(string_accum &sa, const CV::Leaf &type);

const char *namespace_sep(const char *name);
const char *rnamespace_sep(const char *name);
bool anonymous_namespace(const count_string &s);

//-----------------------------------------------------------------------------
// simple_type
//-----------------------------------------------------------------------------

struct simple_type {
	union {
		TI	ti;
		struct {
			uint16	sub:4, type:4, mode:3;
		};
	};
	simple_type() : ti(0)		{}
	simple_type(TI ti) : ti(ti) {}
	simple_type(CV::type_e type, int sub, CV::prmode_e mode = CV::TM_DIRECT) : sub(sub), type(type), mode(mode) {}
	simple_type&	set_mode(CV::prmode_e _mode) { mode = _mode; return *this; }

	float64		as_float(int64 i) {
		if (type == CV::REAL) {
			switch (sub) {
				case CV::RC_REAL32:	return reinterpret_cast<float32	&>(i);
				case CV::RC_REAL64:	return reinterpret_cast<float64	&>(i);
				case CV::RC_REAL80:	return (float64)reinterpret_cast<float80	&>(i);
				case CV::RC_REAL128:	return (float64)reinterpret_cast<float128	&>(i);
				case CV::RC_REAL48:	return (float64)reinterpret_cast<float48	&>(i);
				case CV::RC_REAL32PP:return reinterpret_cast<float32	&>(i);
				case CV::RC_REAL16:	return reinterpret_cast<float16	&>(i);
			}
		}
		return 0;
	}
};

simple_type		get_simple_type(const PDB_types &pdb, const CV::Leaf &type);
simple_type		get_simple_type(const PDB_types &pdb, TI ti);
string_accum&	dump_constant(string_accum &sa, simple_type type, const CV::Value &value);

//-----------------------------------------------------------------------------
// TypeLoc
//-----------------------------------------------------------------------------

struct TypeLoc {
	enum Type {
		None,
		Static,
		ThisRel,
		Constant,
		TypeDef,
		Method,

		Mask		= 7,
		Bitfield	= 8,	// flag
	};
	Type	fulltype;
	int64	offset;
	size_t	size;
	uint32	alignment;

	TypeLoc(Type type, int64 offset, size_t size = 0, uint32 alignment = 0) : fulltype(type), offset(offset), size(size), alignment(alignment) {}
	TypeLoc() : fulltype(None) {}

	Type	type()			const	{ return Type(fulltype & Mask); }
	bool	is_bitfield()	const	{ return !!(fulltype & Bitfield); }

	int64	byte_offset() const {
		switch (fulltype) {
			case ThisRel:			return offset;
			case ThisRel|Bitfield:	return (offset + 7) >> 3;
			default:				return -1;
		}
	}
	int64	bit_offset() const {
		switch (fulltype) {
			case ThisRel:			return offset << 3;
			case ThisRel|Bitfield:	return offset;
			default:				return -1;
		}
	}
	int64	byte_end() const {
		switch (fulltype) {
			case ThisRel:			return offset + align(size, alignment);
			case ThisRel|Bitfield:	return align((offset >> 3) + 1, alignment);
			default:				return -1;
		}
	}
	int64	bit_end() const {
		switch (fulltype) {
			case ThisRel:			return align(offset + size, alignment) << 3;
			case ThisRel|Bitfield:	return offset + size;
			default:				return -1;
		}
	}
	TypeLoc operator+(const TypeLoc &b) const {
		int64	boff	= b.offset;
		if (fulltype & Bitfield)
			boff <<= 3;
		return TypeLoc(Type(fulltype | b.fulltype), offset + boff, size, alignment);
	}
};

TypeLoc get_typeloc(const PDB_types &pdb, const CV::Leaf &type);

} // namespace iso

#endif // PDB_H
