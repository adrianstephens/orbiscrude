#include "iso/iso_files.h"
#include "hashes/spooky.h"
#include "base/algorithm.h"

using namespace iso;


namespace HDF5 {

struct reader : iso::reader<reader>, reader_ref<istream_ref> {
	uint8		size_offsets = 0, size_lengths = 0;
	reader(istream_ref file) : reader_ref<istream_ref>(file) {}
};

struct offset {
	size_t t	= 0;
	constexpr operator size_t() const { return t; }
	bool	read(reader& r) {
		t = 0;
		return check_readbuff(r, &t, r.size_offsets);
	}
};

struct length {
	size_t t	= 0;
	constexpr operator size_t() const { return t; }
	bool	read(reader& r) {
		t = 0;
		return check_readbuff(r, &t, r.size_lengths);
	}
};

struct Group {
	offset	tree;
	offset	local_heap;
	bool	read(reader& r) {
		return r.read(tree, local_heap);
	}
};

struct Heap {
	malloc_block	heap;
	length			free;

	bool	read(reader& r) {
		uint32	signature;
		uint32	ver;
		length	size;
		offset	data;
		return r.read(signature, ver, size, free, data)
			&& signature == "HEAP"_u32
			&& heap.read((r.seek(data), r), size);
	}

	const char* get_string(size_t offset) const {
		ISO_ASSERT(offset < heap.size());
		return heap + offset;
	}
};

struct Driver {
	static const uint64	ID_MULTI	= "NCSAmult"_u64;
	static const uint64	ID_FAMILY	= "NCSAfami"_u64;
	uint32	ver;
	uint32	size;
	uint64	id;
};

struct SymbolTableEntry {
	enum CACHE : uint32 {
		NONE	= 0,	//No data is cached by the group entry. This is guaranteed to be the case when an object header has a link count greater than one.
		META	= 1,	//Group object header metadata is cached in the scratch-pad space. This implies that the symbol table entry refers to another group.
		SYM		= 2,	//The entry is a symbolic link. The first four bytes of the scratch-pad space are the offset into the local heap for the link value. The object header address will be undefined.
	};

	offset	name;
	offset	header;
	CACHE	cache_type;
	union {
		Group	group;
		uint32	link;
	};

	SymbolTableEntry() {}

	bool	read(reader& r) {
		uint32	unused;
		r.read(name, header, cache_type, unused);
		auto	skip = make_skip_size(r, 16);
		switch (cache_type) {
			case META:
				r.read(group);
				break;
			case SYM:
				r.read(link);
				break;
		}
		return true;
	}
};

struct GroupSymbolTableNode {
	dynamic_array<SymbolTableEntry>	entries;

	bool	read(reader& r) {
		uint32	signature;
		uint8	ver, unused;
		uint16	num_symbols;
		return r.read(signature, ver, unused, num_symbols)
			&& signature == "SNOD"_u32
			&& entries.read(r, num_symbols);
	}

	const SymbolTableEntry*	find(string_ref k, const Heap &heap) const {
		return first_not(entries, [&heap, &k](const auto& e) { return heap.get_string(e.name) < k; });
	}
};

//-----------------------------------------------------------------------------
// Superblock
//-----------------------------------------------------------------------------

struct Superblock : reader {
	static const uint64 SIGNATURE = "\x89HDF\r\n\x1a\n"_u64;

	struct Ver0 {
		uint8	ver, ver_free, ver_root, unused1, ver_shared;
		uint8	size_offsets, size_lengths, unused2;
		uint16	leaf_k;		//Each leaf node of a group B-tree will have at least this many entries but not more than twice this many
		uint16	internal_k;	//Each internal node of a group B-tree will have at least this many entries but not more than twice this many
		uint32	flags;		//ignored
	};
	struct Ver1 : Ver0 {
		uint16	index_k;	//Each internal node of an indexed storage B-tree will have at least this many entries but not more than twice this many
		uint16	unused3;
	};

	enum FLAGS {
		opened_write	= 1 << 0,
		opened_single	= 1 << 2,
	};
	
	offset	base, extension, end, driver;
	Group	root;

	Superblock(istream_ref file) : reader(file) {
		uint8	ver = file.getc();
		switch (ver) {
			case 0: {
				Ver0	head;
				file.seek_cur(-1);
				file.read(head);
				size_offsets	= head.size_offsets;
				size_lengths	= head.size_lengths;
				break;
			}
			case 1: {
				Ver1	head;
				file.seek_cur(-1);
				file.read(head);
				size_offsets	= head.size_offsets;
				size_lengths	= head.size_lengths;
				break;
			}
			case 2: {
				uint8	flags;
				file.read(size_offsets, size_lengths, flags);
				break;
			}
			case 3: {
				uint8	flags;
				file.read(size_offsets, size_lengths, flags);
				break;
			}
		}

		read(base, extension, end, driver);

		if (ver < 2) {
			SymbolTableEntry	ste;
			read(ste);
			root = ste.group;

		} else {
			offset	root;
			read(root);
			auto ck	= file.get<uint32>();
		} 
	}
};

//-----------------------------------------------------------------------------
// Btree1
//-----------------------------------------------------------------------------

struct Btree1 {
	struct node {
		enum TYPE {
			GROUP	= 0,
			RAW		= 1,
		};
		uint32	signature;
		uint8	type, level;
		uint16	used;
		offset	left, right;

		bool	read(reader& r) {
			return r.read(signature, type, level, used, left, right);
		}
	};
};


struct GroupTree : Btree1 {
	typedef offset key;

	struct node : Btree1::node {
		dynamic_array<pair<key, offset>>	children;
		key		end_key;
		bool	read(reader& r) {
			return Btree1::node::read(r)
				&& type == GROUP
				&& children.read(r, used)
				&& r.read(end_key);
		}
	};

	reader& r;
	node	root;
	Heap	heap;
	hash_map<offset, node>	nodes;
	hash_map<offset, GroupSymbolTableNode>	tables;

	const node*	get_node(offset pos) {
		auto	n = nodes[pos];
		if (!n.exists()) {
			r.seek(pos);
			n.put().read(r);
		}
		return &n.put();
	}
	const GroupSymbolTableNode*	get_table(offset pos) {
		auto	n = tables[pos];
		if (!n.exists()) {
			r.seek(pos);
			n.put().read(r);
		}
		return &n.put();
	}

	const SymbolTableEntry*	find(string_ref k) {
		for (const node	*n = &root;;) {
			auto	i = first_not(n->children.slice(1), [&heap = heap, &k](const auto& c) { return !(k < heap.get_string(c.a)); }) - 1;
			if (n->level == 0)
				return get_table(i->b)->find(k, heap);

			n = get_node(i->b);
		}
	}

	GroupTree(reader& r, const Group& g) : r(r) {
		r.seek(g.tree);
		root.read(r);
		ISO_ASSERT(root.signature == "TREE"_u32);
		r.seek(g.local_heap);
		heap.read(r);
	}
};

struct RawTree : Btree1 {
	typedef range<const uint64*> key0;

	friend bool operator<(const key0& a, const key0& b) {
		return compare_array(a, b) < 0;
	}

	struct key {
		uint32	chunk_size;
		uint32	filter_mask;
		dynamic_array<uint64>	chunk_offsets;
		bool	read(reader& r, int dims) {
			return r.read(chunk_size, filter_mask) && chunk_offsets.read(r, dims);
		}
		operator key0() const { return chunk_offsets; }
	};

	struct node : Btree1::node {
		dynamic_array<pair<key, offset>>	children;
		key		end_key;

		bool	read(reader& r, int dims) {
			if (!Btree1::node::read(r) || type != RAW)
				return false;

			children.resize(used);
			for (auto &i : children) {
				i.a.read(r, dims);
				i.b.read(r);
			}
			return end_key.read(r, dims);
		}
	};
	
	reader& r;
	int		dims;
	node	root;
	hash_map<offset, node>	nodes;

	RawTree(reader& r, size_t pos, int dims) : r(r), dims(dims) {
		r.seek(pos);
		root.read(r, dims);
		ISO_ASSERT(root.signature == "TREE"_u32);
	}

	const node*	get_node(offset pos) {
		auto	n = nodes[pos];
		if (!n.exists()) {
			r.seek(pos);
			n.put().read(r, dims);
		}
		return &n.put();
	}

	offset	find(const key0& k) {
		for (const node	*n = &root;;) {
			auto	i = first_not(n->children.slice(1), [k](const auto& c) { return !(k < c.a); }) - 1;
			if (n->level == 0)
				return i->b;
			n = get_node(i->b);
		}
	}
};
//-----------------------------------------------------------------------------
// Btree2
//-----------------------------------------------------------------------------

struct Btree2 {
	enum TYPE : uint8 {
		TESTING				= 0,	//testing only
		INDIRECT_UNFILTERED	= 1,	//indexing indirectly accessed, non-filtered ‘huge’ fractal heap objects
		INDIRECT_FILTERED	= 2,	//indexing indirectly accessed, filtered ‘huge’ fractal heap objects
		DIRECT_UNFILTERED	= 3,	//indexing directly accessed, non-filtered ‘huge’ fractal heap objects
		DIRECT_FILTERED		= 4,	//indexing directly accessed, filtered ‘huge’ fractal heap objects
		LINK_NAME			= 5,	//indexing the ‘name’ field for links in indexed groups
		LINK_ORDER			= 6,	//indexing the ‘creation order’ field for links in indexed groups
		SHARED_MESSAGES		= 7,	//indexing shared object header messages
		ATTRIBUTE_NAME		= 8,	//indexing the ‘name’ field for indexed attributes
		ATTRIBUTE_ORDER		= 9,	//indexing the ‘creation order’ field for indexed attributes
		CHUNKS_UNFILTERED	= 10,	//indexing chunks of datasets with no filters and with more than one dimension of unlimited extent
		CHUNKS_FILTERED		= 11,	//indexing chunks of datasets with filters and more than one dimension of unlimited extent
	};
	
	template<TYPE T> struct record;

	struct child {
		offset		pointer;
		uint32		num_records;
		uint32		total_records;
	};

	struct node {
		uint32		signature;
		uint8		ver;
		TYPE		type;
	};

	struct header : node {	//BTHD
		uint16		unused1;
		uint32		node_size;		//This is the size in bytes of all B-tree nodes.
		uint16		record_size;	//This field is the size in bytes of the B-tree record.
		uint16		depth;			//This is the depth of the B-tree.
		uint8		split_pct;		//The percent full that a node needs to increase above before it is split.
		uint8		merge_pct;		//The percent full that a node needs to be decrease below before it is split.
		uint16		unused2;
		offset		root;			//This is the address of the root B-tree node. A B-tree with no records will have the undefined address in this field.
		uint32		records_in_root;
		length		records_in_btree;
		uint32		checksum;	//of header
	};

	struct internal : node {// BTIN
		//records[R];
		//children[N];
		//checksum
	};

	struct leaf : node {// BTLF
		//records[N];
		//checksum
	};

};


template<> struct Btree2::record<Btree2::INDIRECT_UNFILTERED> {
	offset	address;
	length	size;
	length	id;
};
template<> struct Btree2::record<Btree2::INDIRECT_FILTERED>	{
	offset	address;
	length	size;
	uint32	filter_mask;
	length	memory_size;
	length	id;
};
template<> struct Btree2::record<Btree2::DIRECT_UNFILTERED>	{
	offset	address;
	length	size;
};
template<> struct Btree2::record<Btree2::DIRECT_FILTERED>	{
	offset	address;
	length	size;
	uint32	filter_mask;
	length	memory_size;
};
template<> struct Btree2::record<Btree2::LINK_NAME>			{
	uint32	hash;
	uint64	id;
};
template<> struct Btree2::record<Btree2::LINK_ORDER>		{
	uint64	order;
	uint64	id;
};
template<> struct Btree2::record<Btree2::SHARED_MESSAGES>	{
	enum LOC : uint8 {
		SHARED	= 0,
		OBJECT	= 1,
	};
	LOC		location;
	uint32	hash;
	union {
		struct {
			uint32	ref_count;
			uint64	heap_id;
		};
		struct {
			uint8	reserved;
			uint8	message_type;
			uint16	index;
			offset	header;
		};
	};
};
template<> struct Btree2::record<Btree2::ATTRIBUTE_NAME>	{
	uint64	heap_id;
	uint8	flags;
	uint32	order;
	uint32	hash;
};
template<> struct Btree2::record<Btree2::ATTRIBUTE_ORDER>	{
	uint64	heap_id;
	uint8	flags;
	uint32	order;
};
template<> struct Btree2::record<Btree2::CHUNKS_UNFILTERED>	{
	offset	address;
	uint64	scaled_offset[];
};
template<> struct Btree2::record<Btree2::CHUNKS_FILTERED>	{
	offset	address;
	//chunk_size
	uint32	filter_mask;
	uint64	scaled_offset[];
};

//-----------------------------------------------------------------------------
// Object
//-----------------------------------------------------------------------------

struct Type {
	enum VER {
		UNUSED		= 0,//	Never used
		COMPOUND	= 1,//	Used by early versions of the library to encode compound datatypes with explicit array fields. See the compound datatype description below for further details.
		ARRAY		= 2,//	Used when an array datatype needs to be encoded.
		VAX			= 3,//	Used when a VAX byte-ordered type needs to be encoded. Packs various other datatype classes more efficiently also.
		REV_REF		= 4,//	Used to encode the revised reference datatype.
	};
	enum TYPE {
		FixedPoint		= 0,
		FloatingPoint	= 1,
		Time			= 2,
		String			= 3,
		BitField		= 4,
		Opaque			= 5,
		Compound		= 6,
		Reference		= 7,
		Enumerated		= 8,
		VariableLength	= 9,
		Array			= 10,
	};
	enum STRING_PAD {
		NULL_TERM	= 0,//	Null Terminate: A zero byte marks the end of the string and is guaranteed to be present after converting a long string to a short string. When converting a short string to a long string the value is padded with additional null characters as necessary.
		NULL_PAD	= 1,//	Null Pad: Null characters are added to the end of the value during conversions from short values to long values but conversion in the opposite direction simply truncates the value.
		SPACE_PAD	= 2,//	Space Pad: Space characters are added to the end of the value during conversions from short values to long values but conversion in the opposite direction simply truncates the value. This is the Fortran representation of the string.
	};
	enum CHAR_SET {
		ASCII	= 0,//	ASCII character set encoding
		UTF8	= 1,//	UTF-8 character set encoding
	};

	struct Common {
		uint32	type:4, ver:4, flags:24;
		uint32	size;
	};

	struct TypeFixedPoint {
		union {
			Common	common;
			struct { uint32	: 8, BIGENDIAN:1, PAD_LO:1, PAD_HI:1, SIGNED:1;};
		};
		uint16	offset, precision;
	};

	struct TypeFloatingPoint {
		union {
			Common	common;
			struct { uint32	: 8, BIGENDIAN:1, PAD_LO:1, PAD_HI:1, PAD_INT:1, NORM:2, VAX:1, :1, SIGN:8;};
		};
		uint16	offset, precision;
		uint8	ExponentLocation, ExponentSize, MantissaLocation, MantissaSize;
		uint32	ExponentBias;
	};
	struct TypeTime {
		union {
			Common	common;
			struct { uint32	: 8, BIGENDIAN:1;};
		};
		uint16	precision;
	};
	struct TypeString {
		union {
			Common	common;
			struct { uint32	: 8, PADDING:4, CHARSET:4; };
		};
	};
	struct TypeBitField {
		union {
			Common	common;
			struct { uint32	: 8,BIGENDIAN:1, PAD_LO:1, PAD_HI:1;};
		};
		uint16	offset, precision;
	};
	struct TypeOpaque {
		union {
			Common	common;
			struct { uint32	: 8, LEN:8;};
		};
		string	tag;
		bool read(reader& r) { return r.read(common) && tag.read(r, LEN); }

	};
	struct TypeCompound {
		struct member1 {
			char	name[8];	//nul padded to multiple of 8 bytes
			uint32	offset;
			uint32	Dimensionality;
			uint32	DimensionPermutation;
			uint32	Reserved;// (zero)
			uint32	size1;
			uint32	size2;
			uint32	size3;
			uint32	size4;
			//Type	type;
		};
		struct member2 {
			char	name[8];	//nul padded to multiple of 8 bytes
			uint32	offset;
		};
		struct member3 {
			char	name[8];	//nul padded to multiple of 8 bytes
			length	offset;
		};
		union {
			Common	common;
			struct { uint32	: 8, NUM:24;};
		};
	};
	struct TypeReference {
		enum TYPE {
			H5R_OBJECT1			= 0,
			H5R_DATASET_REGION1	= 1,
			H5R_OBJECT2			= 2,	// Object Reference (H5R_OBJECT2): A reference to another object in this file or an external file.
			H5R_DATASET_REGION2	= 3,	// Dataset Region Reference (H5R_DATASET_REGION2): A reference to a region within a dataset in this file or an external file.
			H5R_ATTR			= 4,	// Attribute Reference (H5R_ATTR): A reference to an attribute attached to an object in this file or an external file.
		};
		union {
			Common	common;
			struct { uint32	: 8, REF:4, VER:4;};
		};
	};
	struct TypeEnumerated {
		union {
			Common	common;
			struct { uint32	: 8, NUM:24;};
		};
		uint32	base;		// Each enumeration type is based on some parent type, usually an integer. The information for that parent type is described recursively by this field
		dynamic_array<pair<string, uint64>>	values;

		bool read(reader& r) {
			r.read(common);
			values.resize(NUM);
			for (auto &i : values) {
				i.a.read(r);
				r.align(8);
			}
			for (auto &i : values)
				r.read(i.b);
			return true;
		}
	};
	struct TypeVariableLength {
		//Class specific information for the Variable-length class (Class 9):
		enum {
			SEQUENCE	= 0,//	Sequence: A variable-length sequence of any datatype. Variable-length sequences do not have padding or character set information.
			STRING		= 1,//	String: A variable-length sequence of characters. Variable-length strings have padding and character set information.
		};
		union {
			Common	common;
			struct { uint32	:8, TYPE:4, PADDING:4, CHARSET:4;};
		};
	};
	struct TypeArray {
		Common	common;
	};

	union {
		Common				common;
		TypeFixedPoint		fixedpoint;
		TypeFloatingPoint	floatingpoint;
		TypeTime			time;
		TypeString			string;
		TypeBitField		bitfield;
		TypeOpaque			opaque;
		TypeCompound		compound;
		TypeReference		reference;
		TypeEnumerated		enumerated;
		TypeVariableLength	variablelength;
		TypeArray			array;
	};

	bool read(reader& r) {
		make_save_pos(r), r.read(common);
		switch (common.type) {
			case FixedPoint:	return r.read(fixedpoint);
			case FloatingPoint:	return r.read(floatingpoint);
			case Time:			return r.read(time);
			case String:		return r.read(string);
			case BitField:		return r.read(bitfield);
			case Opaque:		return r.read(opaque);
			case Compound:		return r.read(compound);
			case Reference:		return r.read(reference);
			case Enumerated:	return r.read(enumerated);
			case VariableLength:return r.read(variablelength);
			case Array:			return r.read(array);
			default:			return false;
		}
	}

	Type() : common() {}
	~Type()  {}
};

struct Message {
	enum TYPE : uint8 {
		NIL							= 0x00,
		Dataspace					= 0x01,
		LinkInfo					= 0x02,
		Datatype					= 0x03,
		FillValue_old				= 0x04,
		FillValue					= 0x05,
		Link						= 0x06,
		ExternalDataFiles			= 0x07,
		DataLayout					= 0x08,
		Bogus						= 0x09,
		GroupInfo					= 0x0A,
		DataStorageFilterPipeline	= 0x0B,
		Attribute					= 0x0C,
		ObjectComment				= 0x0D,
		ObjectModificationTime_old	= 0x0E,
		SharedMessageTable			= 0x0F,
		ObjectHeaderContinuation	= 0x10,
		SymbolTableMessage			= 0x11,
		ObjectModificationTime		= 0x12,
		BTtreeKValues				= 0x13,
		DriverInfo					= 0x14,
		AttributeInfo				= 0x15,
		ObjectReferenceCount		= 0x16,
		FileSpaceInfo				= 0x17,
	};
	enum FLAGS : uint8 {
		CONSTANT			= 1 << 0,	//the message data is constant
		SHARED				= 1 << 1,	//the message is shared and stored in another location than the object header
		NOTSHARED			= 1 << 2,	//the message should not be shared
		FAIL_UNKNOWN_WRITE	= 1 << 3,	//the decoder should fail to open this object if it does not understand the message’s type and the file is open with permissions allowing write access to the file
		SET_MOD_UNKNOWN		= 1 << 4,	//the HDF5 decoder should set bit 5 of this message’s flags (in other words, this bit field) if it does not understand the message’s type and the object is modified in any way
		MOD_UNKNOWN			= 1 << 5,	//this object was modified by software that did not understand this message
		SHAREABLE			= 1 << 6,	//this message is shareable
		FAIL_UNKNOWN		= 1 << 7,	//the decoder should always fail to open this object if it does not understand the message’s type
	};
	template<TYPE t> struct T;
	template<TYPE t> struct T2;

	struct Shared1 {
		uint8	ver;
		uint8	type;
		uint16	reserved1;
		uint32	reserved2;
		offset	address;
	};
	struct Shared2 {
		uint8	ver;
		uint8	type;
		offset	address;
	};
	struct Shared3 {
		uint8	ver;
		uint8	type;
		offset	address;//or fractal heap id
	};

	TYPE	type;
	FLAGS	flags;
	uint16	order	= 0;

	virtual ~Message() {}

	Message*	read_body(reader& r, size_t size);
	template<TYPE t> auto	as() const {
		return type == t ? static_cast<const T2<t>*>(this) : nullptr;
	}
};

typedef uint32	unix_time;

struct Object {
	enum FLAGS : uint8 {
		chunk0_size1	= 0,
		chunk0_size2	= 1,
		chunk0_size4	= 2,
		chunk0_size8	= 3,
		order_tracked	= 1 << 2,	//If set, attribute creation order is tracked.
		order_indexed	= 1 << 3,	//If set, attribute creation order is indexed.
		phase_changes	= 1 << 4,	//If set, non-default attribute storage phase change values are stored.
		times			= 1 << 5,	//If set, access, modification, change and birth times are stored.
	};
	FLAGS		flags;
	unix_time	access, modification, change, birth;
	uint16		max_compact_attributes = 0, min_dense_attributes = 0;
	uint8		chunk_size;//variable size
	uint32		ref_count;
	uint32		header_size;

	dynamic_array<Message*>	messages;

	bool	read_v1(reader& r) {
		uint8	ver;
		uint16	num_messages;
		r.read(ver, skip(1), num_messages, ref_count, header_size, skip(4));

		messages.resize(num_messages);
		for (auto &i : messages) {
			Message	m;
			uint16	size;
			r.read(m.type, skip(1), size, m.flags, skip(3));
			i = m.read_body(r, size);
		}
		return true;
	}

	bool	read_v2(reader& r) {
		uint32	signature;
		uint8	ver;
		uint16	num_messages;

		r.read(signature, ver, flags);

		if (flags & times)
			r.read(access, modification, change, birth);
		r.read(max_compact_attributes, min_dense_attributes, chunk_size, skip(1), num_messages, ref_count, header_size, skip(4));

		messages.resize(num_messages);
		for (auto &i : messages) {
			Message	m;
			uint16	size;
			r.read(m.type, size, m.flags);
			if (flags & order_tracked)
				r.read(m.order);
			i = m.read_body(r, size);
		}
		return true;
	}

	template<Message::TYPE t> auto	get() const {
		for (auto m : messages) {
			if (auto p = m->as<t>())
				return p;
		}
		return (decltype(messages[0]->as<t>()))nullptr;
	}
};


template<> struct Message::T<Message::NIL> {};

template<> struct Message::T<Message::Dataspace> {
	enum FLAGS {
		has_max		= 1 << 0,
		has_perm	= 1 << 1,
	};
	enum TYPE : uint8 {
		SCALAR	= 0,	//A scalar dataspace; in other words, a dataspace with a single, dimensionless element.
		SIMPLE	= 1,	//A simple dataspace; in other words, a dataspace with a rank greater than 0 and an appropriate number of dimensions.
		EMPTY	= 2,	//A null dataspace; in other words, a dataspace with no elements.
	};
	TYPE					type;
	dynamic_array<length>	size;
	dynamic_array<length>	max_size;
	dynamic_array<length>	permutation_index;

	bool	read(reader& r) {
		uint8	ver, dims, flags;
		r.read(ver, dims, flags, type);
		if (ver < 2)
			r.discard<uint32>();
		size.read(r, dims);

		if (flags & has_max)
			max_size.read(r, dims);

		if (flags & has_perm)
			permutation_index.read(r, dims);
		return true;
	}
};

template<> struct Message::T<Message::LinkInfo> {
	enum FLAGS {
		ORDER_TRACKED	= 1 << 0,//	If set, creation order for the links is tracked.
		ORDER_INDEXED	= 1 << 1,//	If set, creation order for the links is indexed.
	};
	uint8	ver, flags;
	uint64	order;
	offset	heap_address;
	offset	btree2_name;
	offset	btree2_order;
};


template<> struct Message::T<Message::Datatype> : Type {};

template<> struct Message::T<Message::FillValue_old> {};
template<> struct Message::T<Message::FillValue> {};
template<> struct Message::T<Message::Link> {};
template<> struct Message::T<Message::ExternalDataFiles> {};

template<> struct Message::T<Message::DataLayout> {
	enum LAYOUT {
		COMPACT		= 0,
		CONTIGUOUS	= 1,
		CHUNKED		= 2,
		VIRTUAL		= 3,
	};
	malloc_block			compact;
	offset					address;
	length					contiguous_size;
	dynamic_array<uint32>	chunked_sizes;

	bool read(reader& r) {
		switch (r.getc()) {	//version
			default:
				return 0;

			case 1:
			case 2: {
				uint8	dims, layout;
				r.read(dims, layout, skip(5));
				if (layout != COMPACT)
					r.read(address);
				chunked_sizes.read(r, dims + 1);	// last is dataset element size
				return true;
			}

			case 3: {
				switch (r.getc()) {//layout
					case COMPACT:
						return compact.read(r, r.get<uint16>());
					case CONTIGUOUS:
						return r.read(address, contiguous_size);
					case CHUNKED: {
						uint8	dims;
						return r.read(dims, address) && chunked_sizes.read(r, dims + 1);	// last is dataset element size
					}
					default:
						return false;
				}
			}
		}
	}
};

template<> struct Message::T<Message::Bogus> {};
template<> struct Message::T<Message::GroupInfo> {};
template<> struct Message::T<Message::DataStorageFilterPipeline> {};
template<> struct Message::T<Message::Attribute> {};
template<> struct Message::T<Message::ObjectComment> {};
template<> struct Message::T<Message::ObjectModificationTime_old> {};
template<> struct Message::T<Message::SharedMessageTable> {};
template<> struct Message::T<Message::ObjectHeaderContinuation> {};

template<> struct Message::T<Message::SymbolTableMessage> : Group {};

template<> struct Message::T<Message::ObjectModificationTime> {};
template<> struct Message::T<Message::BTtreeKValues> {};
template<> struct Message::T<Message::DriverInfo> {};
template<> struct Message::T<Message::AttributeInfo> {};
template<> struct Message::T<Message::ObjectReferenceCount> {};
template<> struct Message::T<Message::FileSpaceInfo> {};


template<Message::TYPE t> struct Message::T2 : Message, T<t> {
	T2(const Message &m, reader &r) : Message(m) { ISO_VERIFY(r.read(*(T<t>*)this)); }
};

Message* Message::read_body(reader& r, size_t size) {
	auto	s = make_skip_size(r, size);
	switch (type) {
		case NIL:						return new T2<NIL>(*this, r);
		case Dataspace:					return new T2<Dataspace>(*this, r);
		case LinkInfo:					return new T2<LinkInfo>(*this, r);
		case Datatype:					return new T2<Datatype>(*this, r);
		case FillValue_old:				return new T2<FillValue_old>(*this, r);
		case FillValue:					return new T2<FillValue>(*this, r);
		case Link:						return new T2<Link>(*this, r);
		case ExternalDataFiles:			return new T2<ExternalDataFiles>(*this, r);
		case DataLayout:				return new T2<DataLayout>(*this, r);
		case Bogus:						return new T2<Bogus>(*this, r);
		case GroupInfo:					return new T2<GroupInfo>(*this, r);
		case DataStorageFilterPipeline:	return new T2<DataStorageFilterPipeline>(*this, r);
		case Attribute:					return new T2<Attribute>(*this, r);
		case ObjectComment:				return new T2<ObjectComment>(*this, r);
		case ObjectModificationTime_old:return new T2<ObjectModificationTime_old>(*this, r);
		case SharedMessageTable:		return new T2<SharedMessageTable>(*this, r);
		case ObjectHeaderContinuation:	return new T2<ObjectHeaderContinuation>(*this, r);
		case SymbolTableMessage:		return new T2<SymbolTableMessage>(*this, r);
		case ObjectModificationTime:	return new T2<ObjectModificationTime>(*this, r);
		case BTtreeKValues:				return new T2<BTtreeKValues>(*this, r);
		case DriverInfo:				return new T2<DriverInfo>(*this, r);
		case AttributeInfo:				return new T2<AttributeInfo>(*this, r);
		case ObjectReferenceCount:		return new T2<ObjectReferenceCount>(*this, r);
		case FileSpaceInfo:				return new T2<FileSpaceInfo>(*this, r);
		default:						return nullptr;
	}
}

}
//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------

void dump(HDF5::reader& r, const HDF5::GroupTree::node& node, const HDF5::Heap &heap, int depth = 0) {
	ISO_OUTPUTF() << repeat("  ", depth) << hex(node.signature) << '\n';
	for (auto &c : node.children) {
		r.seek(c.b);

		if (node.level > 0) {
			HDF5::GroupTree::node	cn;
			cn.read(r);
			dump(r, cn, heap, depth + 1);

		} else {
			HDF5::GroupSymbolTableNode	sn;
			sn.read(r);
			for (auto& i : sn.entries) {
				ISO_OUTPUTF() << repeat("  ", depth + 1) << heap.get_string(i.name) << '\n';

			}
		}
	}
}

void to_iso(anything &a, HDF5::reader& r, const HDF5::GroupTree::node& node, const HDF5::Heap &heap) {
	for (auto &c : node.children) {
		r.seek(c.b);

		if (node.level > 0) {
			HDF5::GroupTree::node	cn;
			cn.read(r);
			to_iso(a, r, cn, heap);

		} else {
			HDF5::GroupSymbolTableNode	sn;
			sn.read(r);

			for (auto& i : sn.entries) {
				tag	id = heap.get_string(i.name);

				if (i.cache_type == HDF5::SymbolTableEntry::META) {
					HDF5::GroupTree	tree(r, i.group);
					ISO_ptr<anything>	b(id);
					a.Append(b);
					to_iso(*b, r, tree.root, tree.heap);

				} else {
					HDF5::Object	obj;
					r.seek(i.header);
					obj.read_v1(r);
				
					auto	type	= obj.get<HDF5::Message::Datatype>();

					if (auto layout = obj.get<HDF5::Message::DataLayout>()) {
						malloc_block	data;

						if (layout->compact) {
							data	= move(layout->compact);
						
						} else if (layout->chunked_sizes) {
							auto	space	= obj.get<HDF5::Message::Dataspace>();
							HDF5::RawTree	tree(r, layout->address, layout->chunked_sizes.size() - 1);

							uint64	size	= space->size[0];
							uint32	chunk	= layout->chunked_sizes[0];

							data.resize(size);
						
							for (uint64 pos = 0; pos < size; pos += chunk) {
								uint64	key[] = {pos, 0};
								auto	address = tree.find(key);
								r.seek(address);
							
								data.slice(pos, chunk).read(r);
							}

						} else {
							r.seek(layout->address);
							data.read(r, layout->contiguous_size);
						}

						a.Append(ISO_ptr<malloc_block>(id, move(data)));
					}
				}
			}

		}
	}
}

class HDF5_FileHandler : public FileHandler {
	const char*		GetDescription() override { return "hdf5";}
	int				Check(istream_ref file) override {
		file.seek(0);
		return file.get<uint64>() == HDF5::Superblock::SIGNATURE ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}

	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		if (file.get<uint64>() != HDF5::Superblock::SIGNATURE)
			return ISO_NULL;

		HDF5::Superblock	super(file);
		HDF5::GroupTree		tree(super, super.root);

		ISO_ptr<anything> p(id);
		to_iso(*p, super, tree.root, tree.heap);
		return p;
	}
} hdf5;