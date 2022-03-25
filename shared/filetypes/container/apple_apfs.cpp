#include "stream.h"
#include "base/hash.h"
#include "base/algorithm.h"
#include "base/interval.h"
#include "extra/date.h"
#include "extra/disk.h"
#include "codec/apple_compression.h"
#include "hashes/simple.h"
#include "zlib/zlib.h"

using namespace iso;

//-----------------------------------------------------------------------------
//	APFS structs
//-----------------------------------------------------------------------------
#define APFS_GPT_PARTITION_UUID "7C3457EF-0000-11AA-AA11-00306543ECAC"

//can end with a ':' or '#' + more stuff
#define XATTR_FS_SYMLINK				"com.apple.fs.symlink"
#define XATTR_QUARANTINE				"com.apple.quarantine"						// a quarantine status, the time when a file was quarantined, and the application that downloaded a file
#define XATTR_METADATA					"com.apple.metadata"
#define XATTR_DECMPFS					"com.apple.decmpfs"							// HFS+ compression
#define XATTR_DISKIMAGES				"com.apple.diskimages.fsck"					// 20 verification status for DMG files.
#define XATTR_FINDERINFO				"com.apple.FinderInfo"						// 32 file flags, which are not actually stored as an extended attribute.
#define XATTR_LASTUSEDDATE				"com.apple.lastuseddate"
#define XATTR_PROGRESS_COMPLETED		"com.apple.progress.fractionCompleted"		// 1,4 for a .download bundle.
#define XATTR_RESOURCEFORK				"com.apple.ResourceFork"					// 286 which is not actually stored as an extended attribute.
#define XATTR_SYSTEM_SECURITY			"com.apple.system.Security"					// implement ACLs
#define XATTR_TEXTENCODING				"com.apple.TextEncoding"					// for a file saved with an application like TextEdit
#define XATTR_UBD_PRSID					"com.apple.ubd.prsid"
#define XATTR_PREVIEW_UISTATE_V1		"com.apple.Preview.UIstate.v1"
#define XATTR_DISKIMAGES_RECENTCKSUM	"com.apple.diskimages.recentcksum"
#define XATTR_DISKIMAGES_FSCK			"com.apple.diskimages.fsck"

// General-PurposeTypes
typedef int64			paddr;		// -ve not valid
typedef GUID			uuid;
typedef uint64			oid;		// object id
typedef uint64			xid;		// transaction id

struct prange {
	paddr		start;			// The first block in the range
	uint64		block_count;	// The number of blocks in the range.
	paddr	end() const { return start + block_count; }
};

struct type_phys {
	enum {
		OID_NX_SUPERBLOCK	= 1,
		OID_INVALID			= 0,
		OID_RESERVED_COUNT	= 1024,
	};
	enum TYPE : uint16 {
		TYPE_INVALID			= 0x0000,	//invalid type, or no subtype
		TYPE_NX_SUPERBLOCK		= 0x0001,
		TYPE_BTREE				= 0x0002,
		TYPE_BTREE_NODE			= 0x0003,
		TYPE_SPACEMAN			= 0x0005,
		TYPE_SPACEMAN_CAB		= 0x0006,
		TYPE_SPACEMAN_CIB		= 0x0007,
		TYPE_SPACEMAN_BITMAP	= 0x0008,
		TYPE_SPACEMAN_FREE_QUEUE= 0x0009,	//subtype only
		TYPE_EXTENT_LIST_TREE	= 0x000a,	//subtype only
		TYPE_OMAP				= 0x000b,	//also subtype of btree
		TYPE_CHECKPOINT_MAP		= 0x000c,
		TYPE_FS					= 0x000d,
		TYPE_FSTREE				= 0x000e,	//subtype only
		TYPE_BLOCKREFTREE		= 0x000f,	//subtype only
		TYPE_SNAPMETATREE		= 0x0010,	//subtype only
		TYPE_NX_REAPER			= 0x0011,
		TYPE_NX_REAP_LIST		= 0x0012,
		TYPE_OMAP_SNAPSHOT		= 0x0013,	//subtype only
		TYPE_EFI_JUMPSTART		= 0x0014,
		TYPE_FUSION_MIDDLE_TREE	= 0x0015,	//subtype only
		TYPE_NX_FUSION_WBC		= 0x0016,
		TYPE_NX_FUSION_WBC_LIST	= 0x0017,
		TYPE_ER_STATE			= 0x0018,
		TYPE_GBITMAP			= 0x0019,
		TYPE_GBITMAP_TREE		= 0x001a,	//subtype only
		TYPE_GBITMAP_BLOCK		= 0x001b,
		TYPE_MAX				= TYPE_GBITMAP_BLOCK,
		TYPE_TEST				= 0x00ff,
//		TYPE_CONTAINER_KEYBAG	= 'keys',
//		TYPE_VOLUME_KEYBAG		= 'recs',
	};

	enum STORAGE {
		VIRTUAL					= 0,
		PHYSICAL				= 1,
		EPHEMERAL				= 2,
	};
	enum FLAGS {
		NOHEADER				= 0x2000,
		ENCRYPTED				= 0x1000,
		NONPERSISTENT			= 0x0800,
	};

	TYPE	type;
	uint16	flags:14, storage:2;

	bool	is_virtual() const { return storage == VIRTUAL; }
};

struct header_phys {
	uint64		cksum;
	oid			o;
	xid			x;
	bool check_checksum(size_t block_size) const {
		return fletcher((uint32*)this, 2, fletcher((uint32*)this + 2, block_size / sizeof(uint32) - 2)) == 0;
	}
};

// header used at the beginning of all objects
struct obj_phys : header_phys, type_phys {
	uint32		subtype;

	bool valid(size_t block_size, TYPE t, TYPE sub = TYPE_INVALID) const {
		return type == t && subtype == sub && check_checksum(block_size);
	}
	bool valid(size_t block_size) const {
		return type && type < TYPE_MAX && cksum && check_checksum(block_size);
	}
};

auto read_blocks(istream_ref file, size_t block_size, void *dest, paddr start, uint32 count) {
	file.seek(start * block_size);
	return file.readbuff(dest, block_size * count);
}
auto read_blocks(istream_ref file, size_t block_size, paddr start, uint32 count) {
	file.seek(start * block_size);
	return malloc_block(file, block_size * count);
}

//-----------------------------------------------------------------------------
// BTREES
//-----------------------------------------------------------------------------

struct btree_info_fixed {
	enum FLAGS {
		UINT64_KEYS			= 0x00000001,
		SEQUENTIAL_INSERT	= 0x00000002,
		ALLOW_GHOSTS		= 0x00000004,
		EPHEMERAL			= 0x00000008,
		PHYSICAL			= 0x00000010,
		NONPERSISTENT		= 0x00000020,
		KV_NONALIGNED		= 0x00000040,
	};
	enum {
		TOC_ENTRY_INCREMENT		= 8,
		TOC_ENTRY_MAX_UNUSED	= 2 * TOC_ENTRY_INCREMENT,
		BTOFF_INVALID			= 0xffff,
	};
	uint32		flags;
	uint32		node_size;
	uint32		key_size;
	uint32		val_size;
};

struct btree_info : btree_info_fixed {
	uint32		longest_key;
	uint32		longest_val;
	uint64		key_count;
	uint64		node_count;
};

struct btree_node_phys : obj_phys {
	enum {
		SIZE_DEFAULT		= 4096,
		MIN_ENTRY_COUNT		= 4,
	};
	enum FLAGS {
		ROOT				= 0x0001,
		LEAF				= 0x0002,
		FIXED_KV_SIZE		= 0x0004,
		CHECK_KOFF_INVAL	= 0x8000,
	};
	struct nloc		{ uint16	off, len; };
	struct kvloc	{ nloc		k, v; };
	struct kvoff	{ uint16	k, v; };

	template<typename T> friend T&	get(param_element<kvoff&, T*> &&a)	{ return *(T*)((char*)a.p + a.t.k); }
	template<typename T> friend T&	get(param_element<kvloc&, T*> &&a)	{ return *(T*)((char*)a.p + a.t.k.off); }

	friend void*		get_key(const kvoff *loc, void *keys)	{ return (char*)keys + loc->k; }
	friend memory_block	get_key(const kvloc *loc, void *keys)	{ return memory_block((char*)keys + loc->k.off, loc->k.len); }
	friend void*		get_val(const kvoff *loc, void *vals)	{ return (char*)vals - loc->v; }
	friend memory_block	get_val(const kvloc *loc, void *vals)	{ return memory_block((char*)vals - loc->v.off, loc->v.len); }

	uint16		flags;
	uint16		level;
	uint32		nkeys;
	nloc		table_space;
	nloc		free_space;
	nloc		key_free_list;
	nloc		val_free_list;
	uint64		data[];

	// root only
	btree_info*	get_tree_info(size_t node_size) {
		ISO_ASSERT(flags & ROOT);
		return (btree_info*)((uint8*)this + node_size) - 1;
	}

	auto	get_space(nloc loc)				{ return memory_block((uint8*)data + loc.off, loc.len); }
	void*	keys_start()					{ return (uint8*)data + table_space.off + table_space.len; }
	void*	vals_start(size_t node_size)	{ return flags & ROOT ? (uint8*)this + node_size - sizeof(btree_info) : (uint8*)this + node_size; }
	auto	toc()							{ ISO_ASSERT(flags & FIXED_KV_SIZE); return make_range_n((kvoff*)get_space(table_space), nkeys); }
	auto	toc_var()						{ ISO_ASSERT(!(flags & FIXED_KV_SIZE)); return make_range_n((kvloc*)get_space(table_space), nkeys); }

	template<typename K> auto find(const K &k)		{ return upper_boundc(with_param2(toc(), (K*)keys_start()), k) - 1; }
	template<typename K> auto find_var(const K &k)	{ return upper_boundc(with_param2(toc_var(), (K*)keys_start()), k) - 1; }

	template<typename K> bool contains(const K &k)		{ return !(k < with_param2(toc(), (K*)keys_start()).front()); }
	template<typename K> bool contains_var(const K &k)	{ return !(k < with_param2(toc_var(), (K*)keys_start()).front()); }

	struct traverser {
		xid				x;
		size_t			toc_stride;
		void			*vals, *keys, *toc, *toc_end;

		traverser(btree_node_phys *node, uint32 node_size)
			: x(node->x)
			, toc_stride(node->flags & FIXED_KV_SIZE ? sizeof(kvoff) : sizeof(kvloc))
			, vals(node->vals_start(node_size))
			, keys(node->keys_start())
			, toc(node->get_space(node->table_space))
			, toc_end((char*)toc + node->nkeys * toc_stride)
		{}

		size_t			size()							const	{ return ((char*)toc_end - (char*)toc) / toc_stride; }
		bool			next(size_t i = 1)						{ return (toc = (char*)toc + toc_stride * i) < toc_end; }
		memory_block	key(uint32 key_size, int i = 0) const	{ return toc_stride == sizeof(kvoff) ? memory_block(get_key((kvoff*)toc + i, keys), key_size) : get_key((kvloc*)toc + i, keys); }
		memory_block	val(uint32 val_size, int i = 0) const	{ return toc_stride == sizeof(kvoff) ? memory_block(get_val((kvoff*)toc + i, vals), val_size) : get_val((kvloc*)toc + i, vals); }
	};

};


struct omap_tree;

template<typename K, typename V> struct btree : btree_info_fixed {
	istream_ref						file;
	size_t						block_size;
	xid							x;
	obj_phys::TYPE				subtype;
	omap_tree					*omap;
	unique_ptr<btree_node_phys>	root;
	mutable hash_map<paddr, unique_ptr<btree_node_phys>>	nodes;

	struct iterator {
		const btree	*tree;
		dynamic_array<btree_node_phys::traverser> stack;

		void	*get_ptr() const { return stack.empty() ? nullptr : stack.back().toc; }
		iterator() {}

		iterator(const btree *tree) : tree(tree) {
			btree_node_phys	*n = tree->root;
			while (!(n->flags & n->LEAF)) {
				auto	&e = stack.emplace_back(n, tree->node_size);
				n = tree->get_node(*e.val(0));
			}
			stack.emplace_back(n, tree->node_size);
		}

		iterator(const btree *tree, const K &k) : tree(tree) {
			btree_node_phys	*n = tree->root;
			while (!(n->flags & n->LEAF)) {
				auto	&e	= stack.emplace_back(n, tree->node_size);
				e.toc		= n->flags & n->FIXED_KV_SIZE ? (void*)n->find(k).inner() : (void*)n->find_var(k).inner();
				n = tree->get_node(*e.val(0));
			}

			auto	&e	= stack.emplace_back(n, tree->node_size);
			e.toc		= n->flags & n->FIXED_KV_SIZE ? (void*)n->find(k).inner() : (void*)n->find_var(k).inner();
		}
		explicit operator bool()			const { return !stack.empty(); }
		auto	xid()						const { return stack.back().x; }
		auto	raw_key()					const { return stack.back().key(tree->key_size); }
		auto	raw()						const { return stack.back().val(tree->val_size); }
		K&		key()						const { return *(K*)raw_key(); }
		auto	operator*()					const { return *(V*)raw(); }
		auto	operator->()				const { return (V*)raw(); }
		bool operator==(const iterator &b)	const { return get_ptr() == b.get_ptr(); }
		bool operator!=(const iterator &b)	const { return get_ptr() != b.get_ptr(); }

		iterator& operator++() {
			bool	popped = false;
			while (!stack.empty() && !stack.back().next()) {
				stack.pop_back();
				popped = true;
			}

			if (popped && !stack.empty()) {
				auto	n = tree->get_node(*stack.back().val(0));
				while (!(n->flags & n->LEAF)) {
					auto	&e = stack.emplace_back(n, tree->node_size);
					n = tree->get_node(*e.val(0));
				}
				stack.emplace_back(n, tree->node_size);
			}

			return *this;
		}
	};

	btree_node_phys	*get_node(oid start) const {
		auto	i = nodes[start];
		if (i.exists())
			return *i;
		if (omap)
			start = omap->find(omap_phys::key(start, x)).addr;
		unique_ptr<btree_node_phys> n = read_blocks(file, block_size, start, 1);
		ISO_ASSERT(!n || n->valid(block_size, obj_phys::TYPE_BTREE_NODE, subtype));
		return i = move(n);
	}

	btree(istream_ref file, size_t block_size, omap_tree *omap, oid o, xid x, obj_phys::TYPE subtype) : file(file), block_size(block_size), x(x), subtype(subtype) {
		if (omap)
			o = omap->find(omap_phys::key(o, x)).addr;
		root	= read_blocks(file, block_size, o, 1);
		clear(*(btree_info_fixed*)this);
		if (root->valid(block_size, obj_phys::TYPE_BTREE, subtype) && (root->flags & root->ROOT)) {
			*(btree_info_fixed*)this = *root->get_tree_info(block_size);
			this->omap = flags & PHYSICAL ? nullptr : omap;
		}
	}

	V	find(const K &k) {
		btree_node_phys	*n = root;
		while (!(n->flags & n->LEAF)) {
			auto	x	= n->find(k);
			n = get_node(*(oid*)get_val(x.inner(), n->vals_start(node_size)));
		}

		auto	x	= n->find(k);
		return *(V*)get_val(x.inner(), n->vals_start(node_size));
	}

	bool		contains(const K &k)	const { return root->contains(k); }
	iterator	find_it(const K &k)		const { return iterator(this, k); }
	iterator	begin()					const { return iterator(this); }
	iterator	end()					const { return iterator(); }
};

//-----------------------------------------------------------------------------
// OBJECT MAPS
//-----------------------------------------------------------------------------

struct omap_phys : obj_phys {
	enum FLAGS {
		MANUALLY_MANAGED		= 0x00000001,
		ENCRYPTING				= 0x00000002,
		DECRYPTING				= 0x00000004,
		KEYROLLING				= 0x00000008,
		CRYPTO_GENERATION		= 0x00000010,
	};
	enum PHASE {
		REAP_PHASE_MAP_TREE			= 1,
		REAP_PHASE_SNAPSHOT_TREE	= 2,
	};

	struct key {
		oid			o;
		xid			x;
		key(oid o, oid x) : o(o), x(x) {}
		bool operator<(const key &b) const { return o == b.o ? x < b.x : o < b.o; }
	};
	struct val {
		enum FLAGS {
			DELETED				= 0x00000001,
			SAVED				= 0x00000002,
			ENCRYPTED			= 0x00000004,
			NOHEADER			= 0x00000008,
			CRYPTO_GENERATION	= 0x00000010,
		};
		uint32		flags;
		uint32		size;
		paddr		addr;
	};
	typedef btree<key, val> tree;

	struct snapshot {
		enum FLAGS {
			DELETED		= 0x00000001,
			REVERTED	= 0x00000002,
		};
		uint32		flags;
		uint32		pad;
		oid			o;
	};
	struct reap_state {
		enum {
			PHASE_START			= 0,
			PHASE_SNAPSHOTS		= 1,
			PHASE_ACTIVE_FS		= 2,
			PHASE_DESTROY_OMAP	= 3,
			PHASE_DONE			= 4
		};
		uint32		phase;
		key			ok;
	};
	struct cleanup_state {
		uint32	cleaning;
		uint32	omsflags;
		xid		sxidprev;
		xid		sxidstart;
		xid		sxidend;
		xid		sxidnext;
		key		curkey;
	};

	uint32		flags;
	uint32		snap_count;
	type_phys	tree_type;
	type_phys	snapshot_tree_type;
	oid			tree_oid;
	oid			snapshot_tree_oid;
	xid			most_recent_snap;
	xid			pending_revert_min;
	xid			pending_revert_max;

	bool		valid(size_t block_size)	const { return obj_phys::valid(block_size, TYPE_OMAP); }
	omap_tree	get_tree(istream_ref file, size_t block_size) const;
};

struct omap_tree : omap_phys::tree {
	omap_tree(omap_phys::tree &&b) : omap_phys::tree(move(b)) {}

	auto	read_blocks(istream_ref file, size_t block_size, oid o, xid x) {
		auto	v = find(omap_phys::key(o, x));
		return ::read_blocks(file, block_size, v.addr, uint32(v.size / block_size));
	}
	bool	contains(oid o, xid x) const {
		return omap_phys::tree::contains(omap_phys::key(o, x));
	}
};

omap_tree omap_phys::get_tree(istream_ref file, size_t block_size) const {
	return tree(file, block_size, nullptr, tree_oid, x, TYPE_OMAP);
}

//-----------------------------------------------------------------------------
// NX
//-----------------------------------------------------------------------------

struct nx_efi_jumpstart : obj_phys {
	enum {
		MAGIC		= "JSDR"_u32,
		VERSION		= 1,
	};
	uint32		magic;
	uint32		version;
	uint32		efi_file_len;
	uint32		num_extents;
	uint64		reserved[16];
	prange		extents[];

	bool	valid(size_t block_size)	const { return version == VERSION && obj_phys::valid(block_size, TYPE_EFI_JUMPSTART); }
	auto	get_extents()				const { return make_range_n(extents, num_extents); }
};

struct nx_superblock : obj_phys {
	enum {
		MAGIC						= "NXSB"_u32,
		MAX_FILE_SYSTEMS			= 100,
		EPH_INFO_COUNT				= 4,
		EPH_MIN_BLOCK_COUNT			= 8,
		MAX_FILE_SYSTEM_EPH_STRUCTS	= 4,
		TX_MIN_CHECKPOINT_COUNT		= 4,
		EPH_INFO_VERSION_1			= 1,
		MINIMUM_BLOCK_SIZE			= 4096,
		DEFAULT_BLOCK_SIZE			= 4096,
		MAXIMUM_BLOCK_SIZE			= 65536,
		MINIMUM_CONTAINER_SIZE		= 1048576,
	};
	enum FLAGS {				// in flags
		RESERVED_1					= 0x00000001,
		RESERVED_2					= 0x00000002,
		CRYPTO_SW					= 0x00000004,
	};
	enum FEATURES {				// in features
		FEATURE_DEFRAG				= 0x00000001,
		FEATURE_LCFD				= 0x00000002,
		SUPPORTED_FEATURES_MASK		= FEATURE_DEFRAG | FEATURE_LCFD,
	};
	enum RO_FEATURES {			// in readonly_compatible_features
		SUPPORTED_ROCOMPAT_MASK		= 0,
	};
	enum INCOMPAT_FEATURES {	// in incompatible_features
		INCOMPAT_VERSION1			= 0x00000001,
		INCOMPAT_VERSION2			= 0x00000002,
		INCOMPAT_FUSION				= 0x00000100,
		SUPPORTED_INCOMPAT_MASK		= INCOMPAT_VERSION2 | INCOMPAT_FUSION,
	};
	enum COUNTER {
		CNTR_OBJ_CKSUM_SET	= 0,
		CNTR_OBJ_CKSUM_FAIL = 1,
		NUM_COUNTERS		= 32
	};
	struct evict_mapping_val : packed<littleendian_types> {
		paddr		dst_paddr;
		uint64		len;
	};

	uint32		magic;
	uint32		block_size;
	uint64		block_count;
	uint64		features;
	uint64		readonly_compatible_features;
	uint64		incompatible_features;
	uuid		nx_uuid;
	oid			next_oid;
	xid			next_xid;
	uint32		xp_desc_blocks;
	uint32		xp_data_blocks;
	paddr		xp_desc_base;
	paddr		xp_data_base;
	uint32		xp_desc_next;
	uint32		xp_data_next;
	uint32		xp_desc_index;
	uint32		xp_desc_len;
	uint32		xp_data_index;
	uint32		xp_data_len;
	oid			spaceman_oid;
	oid			omap_oid;
	oid			reaper_oid;
	type_phys	test_type;	//should always be 0 for apple's implementation
	uint32		max_file_systems;
#if 1
	oid			fs_oid[MAX_FILE_SYSTEMS];
#else
	xid			fs_x;
	oid			fs_oid[MAX_FILE_SYSTEMS - 1];
#endif
	uint64		counters[NUM_COUNTERS];
	prange		blocked_out_prange;
	oid			evict_mapping_tree_oid;
	uint64		flags;
	paddr		efi_jumpstart;
	uuid		fusion_uuid;
	prange		keylocker;
	uint64		ephemeral_info[EPH_INFO_COUNT];
	oid			test_oid;
	oid			fusion_mt_oid;
	oid			fusion_wbc_oid;
	prange		fusion_wbc;

	bool	valid()		const { return magic == MAGIC && obj_phys::valid(block_size, TYPE_NX_SUPERBLOCK); }
	unique_ptr<omap_phys>	get_omap(istream_ref file, size_t block_size) const {
		return read_blocks(file, block_size, omap_oid, 1);
	}
	auto	get_evict_tree(istream_ref file, size_t block_size, omap_tree *omap) const {
		ISO_ASSERT(blocked_out_prange.block_count);
		return btree<paddr, evict_mapping_val>(file, block_size, omap, evict_mapping_tree_oid, x, TYPE_FSTREE);
	}
};

struct checkpoint_map_phys : obj_phys {
	enum { MAP_LAST = 0x00000001 };
	struct entry {
		type_phys	type;
		uint32		subtype;
		uint32		size;
		uint32		pad;
		oid			fs_oid;
		oid			o;
		paddr		addr;
	};
	uint32		flags;
	uint32		count;
	entry		map[];

	bool	valid(size_t block_size)	const { return obj_phys::valid(block_size, TYPE_CHECKPOINT_MAP); }
	auto	entries()					const { return make_range_n(map, count); }
};


//-----------------------------------------------------------------------------
// ENCRYPTION
//-----------------------------------------------------------------------------

struct media_keybag : obj_phys {
	enum {
		TAG_UNKNOWN					= 0,
		TAG_RESERVED_1				= 1,
		TAG_VOLUME_KEY				= 2,
		TAG_VOLUME_UNLOCK_RECORDS	= 3,
		TAG_VOLUME_PASSPHRASE_HINT	= 4,
		TAG_RESERVED_F8				= 0xF8
	};
	struct entry {
		uuid		id;
		uint16		tag;
		uint16		keylen;
		uint8		padding[4];
		uint8		keydata[1];
	};
	uint16		version;
	uint16		nkeys;
	uint32		nbytes;
	uint8		padding[8];
	entry		entries[];
};

//-----------------------------------------------------------------------------
// SPACEMANAGER
//-----------------------------------------------------------------------------

struct chunk_info_block : obj_phys {
	struct entry {
		xid			x;
		paddr		addr;
		uint32		block_count;
		uint32		free_count;
		paddr		bitmap_addr;
	};
	uint32		index;
	uint32		chunk_info_count;
	entry		chunk_info[];

	bool	valid(size_t block_size)	const { return obj_phys::valid(block_size, TYPE_SPACEMAN_CIB); }
};

struct cib_addr_block : obj_phys {
	uint32		index;
	uint32		count;
	paddr		addr[];

	bool	valid(size_t block_size)	const { return obj_phys::valid(block_size, TYPE_SPACEMAN_CAB); }
};

struct spaceman_phys : obj_phys {
	enum {
		FLAG_VERSIONED			= 0x00000001,
		ALLOCZONE_COUNT			= 8,
		// Internal-PoolBitmap
		IP_BM_TX_MULTIPLIER		= 16,
		IP_BM_INDEX_INVALID		= 0xffff,
		IP_BM_BLOCK_COUNT_MAX	= 0xfffe,
		// ChunkInfoBlockConstants
		CI_COUNT_MASK			= 0x000fffff,
		CI_COUNT_RESERVED_MASK	= 0xfff00000,

		SD_MAIN		= 0,
		SD_TIER2	= 1,
		SD_COUNT	= 2,

		SFQ_IP		= 0,
		SFQ_MAIN	= 1,
		SFQ_TIER2	= 2,
		SFQ_COUNT	= 3,
	};
	struct device {
		uint64		block_count;
		uint64		chunk_count;
		uint32		cib_count;
		uint32		cab_count;
		uint64		free_count;
		uint32		addr_offset;
		uint32		reserved;
		uint64		reserved2;
	};

	struct zone {
		enum {
			INVALID_END_BOUNDARY	= 0,
			NUM_PREVIOUS_BOUNDARIES = 7,
		};
		struct entry {
			uint64		start;
			uint64		end;
		};
		entry		current_boundaries;
		entry		previous_boundaries[NUM_PREVIOUS_BOUNDARIES];
		uint16		zone_id;
		uint16		previous_boundary_index;
		uint32		reserved;
	};
	struct free_queue {
		struct key {
			xid			x;
			paddr		addr;
		};
		uint64		count;
		oid			tree_oid;
		xid			oldest_xid;
		uint16		tree_node_limit;
		uint16		pad16;
		uint32		pad32;
		uint64		reserved;
	};

	uint32		block_size;
	uint32		blocks_per_chunk;
	uint32		chunks_per_cib;
	uint32		cibs_per_cab;
	device		dev[SD_COUNT];
	uint32		flags;
	uint32		ip_bm_tx_multiplier;
	uint64		ip_block_count;
	uint32		ip_bm_size_in_blocks;
	uint32		ip_bm_block_count;
	paddr		ip_bm_base;
	paddr		ip_base;
	uint64		fs_reserve_block_count;
	uint64		fs_reserve_alloc_count;
	free_queue	fq[SFQ_COUNT];
	uint16		ip_bm_free_head;
	uint16		ip_bm_free_tail;
	uint32		ip_bm_xid_offset;
	uint32		ip_bitmap_offset;
	uint32		ip_bm_free_next_offset;
	uint32		version;
	uint32		struct_size;
	zone		zones[SD_COUNT][ALLOCZONE_COUNT];

	bool	valid(size_t block_size)	const { return obj_phys::valid(block_size, TYPE_SPACEMAN); }
};

struct nx_reaper_phys : obj_phys {
	enum FLAGS {
		BHM_FLAG	= 0x00000001,
		CONTINUE	= 0x00000002,
	};
	uint64		next_reap_id;
	uint64		completed_id;
	oid			head;
	oid			tail;
	uint32		flags;
	uint32		rlcount;
	uint32		type;
	uint32		size;
	oid			fs_oid;
	oid			o;
	xid			x;
	uint32		nrle_flags;
	uint32		state_buffer_size;
	uint8		state_buffer[];

	bool	valid(size_t block_size)	const { return obj_phys::valid(block_size, TYPE_NX_REAPER); }
};

struct nx_reap_list_phys : obj_phys {
	struct entry {
		enum FLAGS {
			VALID			= 0x00000001,
			REAP_ID_RECORD	= 0x00000002,
			CALL			= 0x00000004,
			COMPLETION		= 0x00000008,
			CLEANUP			= 0x00000010,
		};
		uint32		next;
		uint32		flags;
		uint32		type;
		uint32		size;
		oid			fs_oid;
		oid			o;
		xid			x;
	};
	// ReaperListFlags
	enum {
		INDEX_INVALID	= 0xffffffff,
	};
	oid			next;
	uint32		flags;
	uint32		max;
	uint32		count;
	uint32		first;
	uint32		last;
	uint32		free;
	entry		entries[];

	bool	valid(size_t block_size)	const { return obj_phys::valid(block_size, TYPE_NX_REAP_LIST); }
};

struct apfs_reap_state : packed_types<littleendian_types> {
	uint64		last_pbn;
	xid			cur_snap_xid;
	uint32		phase;
};

//-----------------------------------------------------------------------------
// ENCRYPTION ROLLING
//-----------------------------------------------------------------------------

struct er_state_phys_header : obj_phys {
	enum {
		MAGIC	= "BALF"_u32,
		VERSION	= 1,
	};
	enum FLAGS {
		ENCRYPTING			= 0x00000001,
		DECRYPTING			= 0x00000002,
		KEYROLLING			= 0x00000004,
		PAUSED				= 0x00000008,
		FAILED				= 0x00000010,
		CID_IS_TWEAK		= 0x00000020,
		FREE_1				= 0x00000040,
		FREE_2				= 0x00000080,
		BLOCKSIZE_MASK		= 0x00000F00,
		BLOCKSIZE_SHIFT		= 8,
		PHASE_MASK			= 0x00003000,
		PHASE_SHIFT			= 12,
		FROM_ONEKEY			= 0x00004000,
	};
	enum PHASE {
		PHASE_OMAP_ROLL		= 1,
		PHASE_DATA_ROLL		= 2,
		PHASE_SNAP_ROLL		= 3,
	};
	enum BLOCKSIZE {
		ER_512B_BLOCKSIZE	= 0,
		ER_2KiB_BLOCKSIZE	= 1,
		ER_4KiB_BLOCKSIZE	= 2,
		ER_8KiB_BLOCKSIZE	= 3,
		ER_16KiB_BLOCKSIZE	= 4,
		ER_32KiB_BLOCKSIZE	= 5,
		ER_64KiB_BLOCKSIZE	= 6,
	};
	uint32		magic;
	uint32		version;

	bool	valid(size_t block_size)	const { return magic == MAGIC && version == VERSION && obj_phys::valid(block_size, TYPE_ER_STATE); }
};

struct er_state_phys : er_state_phys_header {
	uint64		flags;
	uint64		snap_xid;
	uint64		current_fext_obj_id;
	uint64		file_offset;
	uint64		progress;
	uint64		total_blk_to_encrypt;
	oid			blockmap_oid;
	uint64		tidemark_obj_id;
	uint64		recovery_extents_count;
	oid			recovery_list_oid;
	uint64		recovery_length;
};

struct er_state_phys_v1 : er_state_phys_header {
	enum {
		CHECKSUM_LENGTH				= 8,
		MAX_CHECKSUM_COUNT_SHIFT	= 16,
		CUR_CHECKSUM_COUNT_MASK		= 0x0000FFFF,
	};
	uint64		flags;
	uint64		snap_xid;
	uint64		current_fext_obj_id;
	uint64		file_offset;
	uint64		fext_pbn;
	uint64		paddr;
	uint64		progress;
	uint64		total_blk_to_encrypt;
	uint64		blockmap_oid;
	uint32		checksum_count;
	uint32		reserved;
	uint64		fext_cid;
	uint8		checksum[0];
};

struct er_recovery_block_phys : obj_phys {
	uint64		offset;
	oid			next_oid;
	uint8		data[0];
};

struct gbitmap_block_phys : obj_phys {
	uint64		field[0];

	bool	valid(size_t block_size)	const { return obj_phys::valid(block_size, TYPE_GBITMAP_BLOCK); }
};

struct gbitmap_phys : obj_phys {
	oid			tree_oid;
	uint64		bit_count;
	uint64		flags;

	bool	valid(size_t block_size)	const { return obj_phys::valid(block_size, TYPE_GBITMAP); }
};

//-----------------------------------------------------------------------------
// FUSION
//-----------------------------------------------------------------------------

struct fusion_wbc_phys : obj_phys {
	uint64		version;
	oid			listHeadOid;
	oid			listTailOid;
	uint64		stableHeadOffset;
	uint64		stableTailOffset;
	uint32		listBlocksCount;
	uint32		reserved;
	uint64		usedByRC;
	prange		rcStash;

	bool	valid(size_t block_size)	const { return obj_phys::valid(block_size, TYPE_NX_FUSION_WBC); }
};

struct fusion_wbc_list_phys : obj_phys {
	struct entry {
		paddr		wbcLba;
		paddr		targetLba;
		uint64		length;
	};
	uint64		version;
	uint64		tailOffset;
	uint32		indexBegin;
	uint32		indexEnd;
	uint32		indexMax;
	uint32		reserved;
	entry		listEntries[];

	bool	valid(size_t block_size)	const { return obj_phys::valid(block_size, TYPE_NX_FUSION_WBC_LIST); }
};

// AddressMarkers
inline auto fusion_block(bool tier2, uint64 block, uint32 block_size)	{ return tier2 ? (0x4000000000000000ULL / block_size) + block : block; }

typedef paddr fusion_mt_key;

struct fusion_mt_val {
	enum {
		DIRTY	= (1 << 0),
		TENANT	= (1 << 1),
	};
	paddr		lba;
	uint32		length;
	uint32		flags;
};

//-----------------------------------------------------------------------------
// COMPRESSION
//-----------------------------------------------------------------------------

struct CompressionHeader {
	enum {
		Zlib_Attr = 3,
		Zlib_Rsrc = 4,
		LZVN_Attr = 7,
		LZVN_Rsrc = 8,
	};
	uint32		signature;
	uint32		algo;
	uint64		size;

	bool		valid()			const { return signature == "cmpf"_u32; }
	bool		in_resource()	const { return (algo & 3) == 0; }
	malloc_block Decompress(const_memory_block comp) const;
};

struct RsrcForkHeader {
	uint32be	data_offset;
	uint32be	mgmt_offset;
	uint32be	data_size;
	uint32be	mgmt_size;

	arbitrary_const_ptr	data() const { return (uint8*)this + data_offset + sizeof(uint32); }
};

struct CmpfRsrc {
	// 1 64K-Block
	struct Entry {
		uint32		off;
		uint32		size;
		auto		get_data(const void *start) const { return const_memory_block((const uint8*)start + off, size); }
	};
	uint32		num_entries;
	Entry		entry[32];
	auto		entries() const { return make_range_n(entry, num_entries); }
};

size_t DecompressZLib(void *dst, size_t size, const_memory_block src) {
	if ((((const uint8*)src)[0] & 0x0f) == 0x0F) {
		(src + 1).copy_to(dst);
		return src.length() - 1;
	}

	z_stream strm;
	clear(strm);

	strm.zalloc		= [](void *opaque, uInt items, uInt size)	{ return iso::malloc(items * size); };
	strm.zfree		= [](void *opaque, void *address)			{ iso::free(address); };
	strm.avail_in	= src.size32();
	strm.avail_out	= uint32(size);
	strm.next_in	= (Byte*)(const void*)src;
	strm.next_out	= (Byte*)dst;

	int	ret = inflateInit2(&strm, 15);
	if (ret != Z_OK)
		return 0;

	do {
		ret = inflate(&strm, Z_NO_FLUSH);
		if (ret == Z_NEED_DICT || ret == Z_DATA_ERROR || ret == Z_MEM_ERROR) {
			strm.avail_out = 0;
			break;
		}
	} while (ret != Z_STREAM_END);

	inflateEnd(&strm);

	return size - strm.avail_out;
}

malloc_block CompressionHeader::Decompress(const_memory_block comp) const {
	if (!comp)
		return none;

	malloc_block	decomp(size);

	switch (algo) {
		case Zlib_Rsrc: {
			const RsrcForkHeader *rsrc_hdr = comp;
			if (rsrc_hdr->data_offset > comp.length())
				return none;

			const CmpfRsrc *cmpf	= rsrc_hdr->data();
			uint8			*dst	= decomp;
			for (auto &e : cmpf->entries()) {
				size_t	len = min(0x10000, decomp.end() - dst);
				if (DecompressZLib(dst, len, e.get_data(cmpf)) != len)
					return none;
				dst += len;
			}
			break;
		}
		case LZVN_Rsrc: {
			const uint32 *off_list = comp;
			for (uint8 *dst	= decomp; dst < decomp.end();) {
				size_t	len	= min(0x10000, decomp.end() - dst);
				size_t	written;
				transcode(LZVN::decoder(), memory_block(dst, len), comp.slice(off_list[0], off_list[1] - off_list[0]), &written);
				if (written != len)
					return none;
				dst += len;
			}
			break;
		}
		case Zlib_Attr:
			if (DecompressZLib(decomp, size, comp) != size)
				return none;
			break;

		case LZVN_Attr: {
			size_t	written;
			transcode(LZVN::decoder(), decomp, comp, &written);
			if (written != size)
				return none;
			break;
		}
	}
	return decomp;
}


//-----------------------------------------------------------------------------
// APFS
//-----------------------------------------------------------------------------

enum FILEMODE {
	MODE_UNKNOWN	= 0,
	MODE_FIFO		= 1,
	MODE_CHR		= 2,
	MODE_DIR		= 4,
	MODE_BLK		= 6,
	MODE_REG		= 8,
	MODE_LNK		= 10,
	MODE_SOCK		= 12,
	MODE_WHT		= 14,
};

struct j_key {
	enum {MAX_SIZE = 832};
	enum TYPE {
		TYPE_ANY			= 0,
		TYPE_SNAP_METADATA	= 1,
		TYPE_EXTENT			= 2,
		TYPE_INODE			= 3,
		TYPE_XATTR			= 4,
		TYPE_SIBLING_LINK	= 5,
		TYPE_DSTREAM_ID		= 6,
		TYPE_CRYPTO_STATE	= 7,
		TYPE_FILE_EXTENT	= 8,
		TYPE_DIR_REC		= 9,
		TYPE_DIR_STATS		= 10,
		TYPE_SNAP_NAME		= 11,
		TYPE_SIBLING_MAP	= 12,
		TYPE_MAX_VALID		= 12,
		TYPE_MAX			= 15,
		TYPE_INVALID		= 15,
	};
	union {
		bitfield<uint64, 0, 60, packed<uint64le>>	id;
		bitfield<TYPE, 60, 4, packed<uint64le>>		type;
		with_op<op_rot, packed<uint64le>, int, 4>	sortable;
	};
	j_key(TYPE type, uint64 id) { this->id.set(id); this->type.set(type); }
	template<TYPE type> auto as();
};

struct j_val : packed_types<littleendian_types> {
	enum {MAX_SIZE = 3808};
	template<j_key::TYPE type> auto as();
};

template<j_key::TYPE T> struct j_keyT : j_key {
	j_keyT(uint64 id) : j_key(T, id) {}
};
template<j_key::TYPE type> struct j_valT : j_val {};

struct j_dstream {
	uint64		size;
	uint64		alloced_size;
	uint64		default_crypto_id;
	uint64		total_bytes_written;
	uint64		total_bytes_read;
};

struct j_dirstats {
	uint64		num_children;
	uint64		total_size;
	uint64		chained_key;
	uint64		gen_count;
};

struct crypto_state {
	enum CLASS {
		PROTECTION_CLASS_DIR_NONE	= 0,
		PROTECTION_CLASS_A			= 1,
		PROTECTION_CLASS_B			= 2,
		PROTECTION_CLASS_C			= 3,
		PROTECTION_CLASS_D			= 4,
		PROTECTION_CLASS_F			= 6,
		EFFECTIVE_CLASSMASK			= 0x1f,
	};
	uint16		major_version, minor_version;
	uint32		cpflags;
	CLASS		persistent_class;
	uint32		key_os_version;
	uint16		key_revision;
	uint16		key_len;
};

struct apfs_superblock : obj_phys {
	enum {
		MAGIC				= "APSB"_u32,
		MAX_HIST			= 8,
		VOLNAME_LEN			= 256,
		MIN_DOC_ID			= 3,
		CRYPTO_SW_ID		= 4,
		CRYPTO_RESERVED_5	= 5,
	};
	enum FS_FLAGS {					// in fs_flags
		FS_UNENCRYPTED						= 0x00000001,
		FS_RESERVED_2						= 0x00000002,
		FS_RESERVED_4						= 0x00000004,
		FS_ONEKEY							= 0x00000008,
		FS_SPILLEDOVER						= 0x00000010,
		FS_RUN_SPILLOVER_CLEANER			= 0x00000020,
		FS_ALWAYS_CHECK_EXTENTREF			= 0x00000040,
		FS_FLAGS_VALID_MASK					= FS_UNENCRYPTED | FS_RESERVED_2 | FS_RESERVED_4 | FS_ONEKEY | FS_SPILLEDOVER | FS_RUN_SPILLOVER_CLEANER | FS_ALWAYS_CHECK_EXTENTREF,
		FS_CRYPTOFLAGS						= FS_UNENCRYPTED | FS_RESERVED_2 | FS_ONEKEY,
	};
	enum FEATURES {					// in features
		FEATURE_DEFRAG_PRERELEASE			= 0x00000001,
		FEATURE_HARDLINK_MAP_RECORDS		= 0x00000002,
		FEATURE_DEFRAG						= 0x00000004,
		SUPPORTED_FEATURES_MASK				= FEATURE_DEFRAG | FEATURE_DEFRAG_PRERELEASE | FEATURE_HARDLINK_MAP_RECORDS,
	};
	enum RO_FEATURES {				// in readonly_compatible_features
		SUPPORTED_ROCOMPAT_MASK				= 0,
	};
	enum INCOMPAT_FEATURES {		// in incompatible_features
		INCOMPAT_CASE_INSENSITIVE			= 0x00000001,
		INCOMPAT_DATALESS_SNAPS				= 0x00000002,
		INCOMPAT_ENC_ROLLED					= 0x00000004,
		INCOMPAT_NORMALIZATION_INSENSITIVE	= 0x00000008,
		SUPPORTED_INCOMPAT_MASK				= INCOMPAT_CASE_INSENSITIVE | INCOMPAT_DATALESS_SNAPS | INCOMPAT_ENC_ROLLED | INCOMPAT_NORMALIZATION_INSENSITIVE,
	};
	enum ROLE {
		ROLE_NONE							= 0x0000,
		ROLE_SYSTEM							= 0x0001,
		ROLE_USER							= 0x0002,
		ROLE_RECOVERY						= 0x0004,
		ROLE_VM								= 0x0008,
		ROLE_PREBOOT						= 0x0010,
		ROLE_INSTALLER						= 0x0020,
		ROLE_DATA							= 0x0040,
		ROLE_BASEBAND						= 0x0080,
		ROLE_RESERVED_200					= 0x0200,
	};
	enum INODE {
		INODE_INVALID						= 0,
		INODE_ROOT_DIR_PARENT				= 1,
		INODE_ROOT_DIR						= 2,
		INODE_PRIV_DIR						= 3,
		INODE_SNAP_DIR						= 6,
		INODE_MIN_USER						= 16,
	};

	struct modified_by {
		uint8		id[32];
		uint64		timestamp;
		xid			last_xid;
	};

	uint32				magic;
	uint32				fs_index;
	uint64				features;
	uint64				readonly_compatible_features;
	uint64				incompatible_features;
	uint64				unmount_time;
	uint64				fs_reserve_block_count;
	uint64				fs_quota_block_count;
	uint64				fs_alloc_count;
	crypto_state		meta_crypto;
	type_phys			root_tree_type;			// usually VIRTUAL | TYPE_BTREE, with a subtype of TYPE_FSTREE
	type_phys			extentref_tree_type;	// usually VIRTUAL | TYPE_BTREE, with a subtype of TYPE_BLOCKREF
	type_phys			snap_meta_tree_type;	// usually VIRTUAL | TYPE_BTREE, with a subtype of TYPE_BLOCKREF
	oid					omap_oid;
	oid					root_tree_oid;
	oid					extentref_tree_oid;
	oid					snap_meta_tree_oid;
	xid					revert_to_xid;
	oid					revert_to_sblock_oid;
	uint64				next_obj_id;
	uint64				num_files;
	uint64				num_directories;
	uint64				num_symlinks;
	uint64				num_other_fsobjects;
	uint64				num_snapshots;
	uint64				total_blocks_alloced;
	uint64				total_blocks_freed;
	uuid				vol_uuid;
	uint64				last_mod_time;
	uint64				fs_flags;
	modified_by			formatted_by;
	modified_by			modified_by[MAX_HIST];
	char				volname[VOLNAME_LEN];
	uint32				next_doc_id;
	uint16				role;
	uint16				reserved;
	xid					root_to_xid;
	oid					er_state_oid;

	bool	valid(size_t block_size)	const { return magic == MAGIC && obj_phys::valid(block_size, TYPE_FS); }

	unique_ptr<omap_phys>	get_omap(istream_ref file, size_t block_size) const {
		return read_blocks(file, block_size, omap_oid, 1);
	}
	auto	get_tree(istream_ref file, size_t block_size, omap_tree *omap) const {
		return btree<j_key, j_val>(file, block_size, root_tree_type.is_virtual() ? omap : nullptr, root_tree_oid, x, TYPE_FSTREE);
	}
	auto	get_extentref_tree(istream_ref file, size_t block_size, omap_tree *omap) const {
		return btree<j_key, j_val>(file, block_size, extentref_tree_type.is_virtual() ? omap : nullptr, extentref_tree_oid, x, TYPE_BLOCKREFTREE);
	}
	auto	get_snap_meta_tree(istream_ref file, size_t block_size, omap_tree *omap) const {
		return btree<j_key, j_val>(file, block_size, snap_meta_tree_type.is_virtual() ? omap : nullptr, snap_meta_tree_oid, x, TYPE_SNAPMETATREE);
	}
};

// EXTENDED FIELDS

struct xf_blob {
	enum TYPE : uint8 {
		DIR_TYPE_SIBLING_ID	= 1,
		TYPE_SNAP_XID		= 1,
		TYPE_DELTA_TREE_OID	= 2,
		TYPE_DOCUMENT_ID	= 3,
		TYPE_NAME			= 4,
		TYPE_PREV_FSIZE		= 5,
		TYPE_RESERVED_6		= 6,
		TYPE_FINDER_INFO	= 7,
		TYPE_DSTREAM		= 8,
		TYPE_RESERVED_9		= 9,
		TYPE_DIR_STATS_KEY	= 10,
		TYPE_FS_UUID		= 11,
		TYPE_RESERVED_12	= 12,
		TYPE_SPARSE_BYTES	= 13,
		TYPE_RDEV			= 14,
	};
	enum FLAGS {
		DATA_DEPENDENT		= 0x01,
		DO_NOT_COPY			= 0x02,
		RESERVED_4			= 0x04,
		CHILDREN_INHERIT	= 0x08,
		USER_FIELD			= 0x10,
		SYSTEM_FIELD		= 0x20,
		RESERVED_40			= 0x40,
		RESERVED_80			= 0x80,
	};
	struct field {
		TYPE		type;
		uint8		flags;
		uint16		size;
	};
	uint16		num_exts;
	uint16		used_data;
	field		ext[1];

	template<TYPE T> struct ext_data;

	struct iterator {
		field	*f;
		uint64	*p;
		iterator() : f(0), p(0) {}
		iterator(field *f, uint64 *p) : f(f), p(p) {}
		iterator&	operator++() { p = (uint64*)((uint8*)p + align(f++->size, sizeof(uint64))); return *this; }
		bool		operator==(const iterator &b)	const { return f == b.f; }
		bool		operator!=(const iterator &b)	const { return f != b.f; }
		uint64&		operator*()						const { return *p; }
		TYPE		type()							const { return f->type; }
		template<TYPE T> ext_data<T>* as()			const { return (ext_data<T>*)p; }
	};

	struct fields_t : range<iterator> {
		fields_t(_none &) : range<iterator>(none) {}
		fields_t(iterator a, iterator b) : range<iterator>(a, b) {}
		iterator	find(TYPE type) const {
			for (auto i : with_iterator(*this)) {
				if (i.type() == type)
					return i;
			}
			return {};
		}
		template<TYPE type> auto find() const { return find(type).as<type>(); }
	};

	fields_t	fields()	{ return {iterator(ext, (uint64*)(ext + num_exts)), iterator(ext + num_exts, (uint64*)((char*)ext + used_data))}; }
};

template<> struct xf_blob::ext_data<xf_blob::TYPE_SNAP_XID		> { xid x; };
template<> struct xf_blob::ext_data<xf_blob::TYPE_DELTA_TREE_OID> { oid o; };
template<> struct xf_blob::ext_data<xf_blob::TYPE_DOCUMENT_ID	> { uint32 id; };
template<> struct xf_blob::ext_data<xf_blob::TYPE_NAME			> { char name[]; };
template<> struct xf_blob::ext_data<xf_blob::TYPE_PREV_FSIZE	> { uint64 size; };
template<> struct xf_blob::ext_data<xf_blob::TYPE_FINDER_INFO	> { uint8 opaque[32]; };
template<> struct xf_blob::ext_data<xf_blob::TYPE_DSTREAM		> : j_dstream {};
template<> struct xf_blob::ext_data<xf_blob::TYPE_DIR_STATS_KEY	> : j_dirstats {};
template<> struct xf_blob::ext_data<xf_blob::TYPE_FS_UUID		> : uuid {};
template<> struct xf_blob::ext_data<xf_blob::TYPE_SPARSE_BYTES	> { uint64 num_sparse_bytes; };
template<> struct xf_blob::ext_data<xf_blob::TYPE_RDEV			> { uint32 dev_id; };

// FILE-SYSTEM OBJECTS

struct j_name_key : j_key {
	uint16		name_len;
	char		name[256];
	auto		get_name() const { return str(name, name_len - 1); }
	j_name_key(TYPE type, uint64 id, const char *_name) : j_key(type, id), name_len((uint16)(uint64)strlen(_name) + 1) { strcpy(name, _name); }
};

//	TYPE_SNAP_METADATA	- snapshot metadata
template<> struct j_valT<j_key::TYPE_SNAP_METADATA	> : j_val {
	enum FLAGS {
		PENDING_DATALESS = 0x00000001,
	};
	oid			extentref_tree_oid;
	oid			sblock_oid;
	uint64		create_time;
	uint64		change_time;
	uint64		inum;
	type_phys	extentref_tree_type;
	uint32		flags;
	uint16		name_len;
	uint8		name[0];
};

//	TYPE_EXTENT - physical extent record
template<> struct j_valT<j_key::TYPE_EXTENT			> : j_val {
	enum KIND {
		ANY				= 0,
		NEW				= 1,
		UPDATE			= 2,
		DEAD			= 3,
		UPDATE_REFCNT	= 4,
		INVALID			= 255
	};
	static const iso::uint64
		OWNING_OBJ_ID_INVALID	= ~0ULL,
		OWNING_OBJ_ID_UNKNOWN	= ~1ULL;
	union {
		bitfield<uint64, 0, 60>	len;
		bitfield<KIND, 60, 4>	kind;
	};
	uint64		owning_obj_id;
	int32		refcnt;
};

//	TYPE_INODE - inode
template<> struct j_valT<j_key::TYPE_INODE			> : j_val {
	enum INTERNAL_FLAGS {
		IS_APFS_PRIVATE			= 0x00000001,
		MAINTAIN_DIR_STATS		= 0x00000002,
		DIR_STATS_ORIGIN		= 0x00000004,
		PROT_CLASS_EXPLICIT		= 0x00000008,
		WAS_CLONED				= 0x00000010,
		FLAG_UNUSED				= 0x00000020,
		HAS_SECURITY_EA			= 0x00000040,
		BEING_TRUNCATED			= 0x00000080,
		HAS_FINDER_INFO			= 0x00000100,
		IS_SPARSE				= 0x00000200,
		WAS_EVER_CLONED			= 0x00000400,
		ACTIVE_FILE_TRIMMED		= 0x00000800,
		PINNED_TO_MAIN			= 0x00001000,
		PINNED_TO_TIER2			= 0x00002000,
		HAS_RSRC_FORK			= 0x00004000,
		NO_RSRC_FORK			= 0x00008000,
		ALLOCATION_SPILLEDOVER	= 0x00010000,
		INHERITED_INTERNAL_FLAGS= MAINTAIN_DIR_STATS,
		CLONED_INTERNAL_FLAGS	= HAS_RSRC_FORK | NO_RSRC_FORK | HAS_FINDER_INFO,
		VALID_MASK				= IS_APFS_PRIVATE | MAINTAIN_DIR_STATS | DIR_STATS_ORIGIN | PROT_CLASS_EXPLICIT | WAS_CLONED | HAS_SECURITY_EA | BEING_TRUNCATED | HAS_FINDER_INFO | IS_SPARSE | WAS_EVER_CLONED | ACTIVE_FILE_TRIMMED | PINNED_TO_MAIN | PINNED_TO_TIER2 | HAS_RSRC_FORK | NO_RSRC_FORK | ALLOCATION_SPILLEDOVER,
		PINNED_MASK				= PINNED_TO_MAIN | PINNED_TO_TIER2,
	};
	enum BSD_FLAGS {
		BSD_NODUMP				= 0x1,
		BSD_IMMUTABLE			= 0x2,
		BSD_APPEND				= 0x4,
		BSD_OPAQUE				= 0x8,
		BSD_NOUNLINK			= 0x10, // Reserved on macOS
		BSD_COMPRESSED			= 0x20,
		BSD_TRACKED				= 0x40,
		BSD_DATAVAULT			= 0x80,
		BSD_HIDDEN				= 0x8000,
		BSD_SYS_ARCHIVED		= 0x10000,
		BSD_SYS_IMMUTABLE		= 0x20000,
		BSD_SYS_APPEND			= 0x40000,
		BSD_SYS_RESTRICTED		= 0x80000,
		BSD_SYS_NOUNLINK		= 0x100000,
		BSD_SYS_SNAPSHOT		= 0x200000, // Reserved on macOS
	};

	uint64		parent_id;
	uint64		private_id;
	uint64		create_time;
	uint64		mod_time;
	uint64		change_time;
	uint64		access_time;
	uint64		internal_flags;
	union {
		int32	nchildren;
		int32	nlink;
	};
	crypto_state::CLASS default_protection_class;
	uint32		write_generation_counter;
	uint32		bsd_flags;	//see sys/stat.h
	uint32		owner;
	uint32		group;
	union {
		bitfield<FILEMODE, 12, 4>	mode;
	};
	uint16		pad1;
	uint64		pad2;
	xf_blob		blob;

	auto	xfields(size_t size)	{ return size >= sizeof(*this) ? blob.fields() : none; }
};

//	TYPE_XATTR	- extended attribute
template<> struct j_keyT<j_key::TYPE_XATTR			> : j_name_key {
	j_keyT(uint64 id, const char *name) : j_name_key(TYPE_XATTR, id, name) {}
};
template<> struct j_valT<j_key::TYPE_XATTR			> : j_val {
	enum FLAGS {
		DATA_STREAM			= 0x0001,
		DATA_EMBEDDED		= 0x0002,
		FILE_SYSTEM_OWNED	= 0x0004,
		RESERVED_8			= 0x0008,
	};
	enum {
		MAX_EMBEDDED_SIZE	= 3804,
	};
	struct dstream {
		uint64		id;
		j_dstream	stream;
	};
	uint16		flags;
	uint16		xdata_len;
	uint8		xdata[0];

	const_memory_block	get_data() const {
		return const_memory_block(xdata, xdata_len);
	}
	const dstream	*get_dstream() const {
		return flags & DATA_STREAM ? (const dstream*)get_data() : 0;
	}
};

//	TYPE_SIBLING_LINK - mapping from an inode to hardlinks that the inode is the target of
template<> struct j_keyT<j_key::TYPE_SIBLING_LINK	> : j_key {
	uint64		sibling_id;
};
template<> struct j_valT<j_key::TYPE_SIBLING_LINK	> : j_val {
	uint64		parent_id;
	uint16		name_len;
	char		name[0];
	auto		get_name() const { return str(name, name_len - 1); }
};

//	TYPE_DSTREAM_ID - data stream
template<> struct j_valT<j_key::TYPE_DSTREAM_ID		> : j_val {
	uint32		refcnt;
};

//	TYPE_CRYPTO_STATE - per-file encryption state
template<> struct j_valT<j_key::TYPE_CRYPTO_STATE	> : j_val {
	enum { MAX_KEYSIZE = 128 };
	uint32			refcnt;
	crypto_state	state;
	uint8			persistent_key[0];
};

//	TYPE_FILE_EXTENT - physical extent record for a file
template<> struct j_keyT<j_key::TYPE_FILE_EXTENT	> : j_key {
	uint64		logical_addr;
	j_keyT(uint64 id, uint64 logical_addr) : j_key(TYPE_FILE_EXTENT, id), logical_addr(logical_addr) {}
};
template<> struct j_valT<j_key::TYPE_FILE_EXTENT	> : j_val {
	enum FLAGS {
		CRYPTO_ID_IS_TWEAK	= 0x01
	};
	union {
		bitfield<uint64, 0, 56, uint64>		len;
		bitfield<uint64, 56, 8, uint64>		flags;
	};
	uint64		phys_block_num;
	uint64		crypto_id;

	auto		get_range(uint32 block_size) const { return make_interval_len(streamptr(phys_block_num * block_size), streamptr((uint64)len)); }
};

//	TYPE_DIR_REC - directory entry
template<> struct j_keyT<j_key::TYPE_DIR_REC		> : j_key {
	//hash is ~CRC-32C bottom 22 bits
	union {
		bitfield<uint32, 0, 10>		name_len;
		bitfield<uint32, 10,22>		hash;
	};
	uint8		name[0];
	auto		get_name() const { return str(name, name_len.get() - 1); }
	bool operator<(const j_keyT &b) const { return make_pair(sortable, hash.get()) < make_pair(b.sortable, b.hash.get()); }
};
template<> struct j_valT<j_key::TYPE_DIR_REC		> : j_val {
	uint64		file_id;
	uint64		date_added;
	union {
		bitfield<FILEMODE, 0, 4>	mode;
		bitfield<uint16, 4, 12>		flags;
	};
	xf_blob		blob;
	auto		xfields(size_t size)	{ return size < sizeof(*this) ? none : blob.fields(); }
	DateTime	get_date()	const		{ return DateTime::FromUnixTime(date_added / 1000); }
};

//	TYPE_DIR_STATS - information about a directory
template<> struct j_valT<j_key::TYPE_DIR_STATS		> : packed<j_dirstats> {};

//	TYPE_SNAP_NAME - name of a snapshot
template<> struct j_keyT<j_key::TYPE_SNAP_NAME		> : j_name_key {};
template<> struct j_valT<j_key::TYPE_SNAP_NAME		> : j_val {
	xid			snap_xid;
};

//	TYPE_SIBLING_MAP - mapping from a hard link to its target inode
template<> struct j_valT<j_key::TYPE_SIBLING_MAP	> : j_val {
	uint64		file_id;
};

int compare(const j_key &a, const j_key &b) {
	if (int r = simple_compare(a.sortable, b.sortable))
		return r;
	switch (get(a.type)) {
		case j_key::TYPE_XATTR:
		case j_key::TYPE_SNAP_NAME:
			return compare(((const j_name_key&)a).get_name(), ((const j_name_key&)b).get_name());
		case j_key::TYPE_SIBLING_LINK:
			return simple_compare(((const j_keyT<j_key::TYPE_SIBLING_LINK>&)a).sibling_id, ((const j_keyT<j_key::TYPE_SIBLING_LINK>&)b).sibling_id);
		case j_key::TYPE_FILE_EXTENT:
			return simple_compare(((const j_keyT<j_key::TYPE_FILE_EXTENT>&)a).logical_addr, ((const j_keyT<j_key::TYPE_FILE_EXTENT>&)b).logical_addr);
		case j_key::TYPE_DIR_REC:
			return compare(((const j_keyT<j_key::TYPE_DIR_REC>&)a).get_name(), ((const j_keyT<j_key::TYPE_DIR_REC>&)b).get_name());
		default:
			return 0;
	}
}
bool operator<(const j_key &a, const j_key &b) { return compare(a, b) < 0; }
bool operator==(const j_key &a, const j_key &b) { return compare(a, b) == 0; }

template<j_key::TYPE X> auto j_key::as() { return static_cast<j_keyT<X>*>(this); }
template<j_key::TYPE X> auto j_val::as() { return static_cast<j_valT<X>*>(this); }

//-----------------------------------------------------------------------------
// helpers
//-----------------------------------------------------------------------------

unique_ptr<nx_superblock> read_super(istream_ref file) {
	malloc_block	m(file, sizeof(nx_superblock));
	nx_superblock	*super = m;
	if (super && super->magic == super->MAGIC && super->type == super->TYPE_NX_SUPERBLOCK) {
		(m.resize(((nx_superblock*)m)->block_size) + sizeof(nx_superblock)).read(file);
		super = m;
		if (super->valid())
			return move(m);
	}
	return none;
}

//find superblock with highest transaction number
nx_superblock* latest_super(const memory_block &desc_block, uint32 block_size) {
	nx_superblock*	super	= 0;
	for (auto &i : make_strided<nx_superblock>(desc_block, block_size)) {
		if (i.type == obj_phys::OID_NX_SUPERBLOCK && (!super || i.x > super->x)) {
			if (i.valid())
				super = &i;
		}
	}
	return super;
}

//find checkpoint maps with current transaction number
dynamic_array<checkpoint_map_phys*> get_checkpoints(const memory_block &desc_block, uint32 block_size, nx_superblock *super) {
	dynamic_array<checkpoint_map_phys*>	cps;
	for (obj_phys *i = super;;) {
		if (i == desc_block)
			i += desc_block.length();

		i = (obj_phys*)((char*)i - block_size);

		if (i->type == obj_phys::OID_NX_SUPERBLOCK)
			break;

		if (i->type == obj_phys::TYPE_CHECKPOINT_MAP) {
			auto	t = (checkpoint_map_phys*)i;
			if (t->valid(block_size))
				cps.push_back(t);
		}
	}
	return cps;
}

//-----------------------------------------------------------------------------
//	FileHandler
//-----------------------------------------------------------------------------

#include "devices/directories.h"
#include "iso/iso_files.h"
#include "iso/iso_convert.h"
#include "filetypes/bin.h"

class APFSFileHandler : public FileHandler {
	const char*		GetDescription() override { return "APFS Disk image"; }

	int				Check(istream_ref file) override {
		file.seek(0);
		return read_super(file) ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}
	ISO_ptr<void>	Read(tag id, istream_ref file) override;
} apfs;

template<typename V> struct APFSEntry : ISO::VirtualDefaults {
	ISO_ptr<V>	vol;
	uint64		file_id;
	xid			x;


	malloc_block	ReadDataStream(uint64 id, const j_dstream &stream) {
		malloc_block	mem(stream.size);

		for (uint64 log = 0; log < stream.size;) {
			if (auto ext = vol->find(j_keyT<j_key::TYPE_FILE_EXTENT>(id, log), x)->template as<j_key::TYPE_FILE_EXTENT>()) {
				auto	r	= ext->get_range(vol->block_size);
				vol->file.seek(r.a);
				log += vol->file.readbuff(mem + log, min(r.b - r.a, stream.size - log));
			} else {
				return none;
			}
		}
		return move(mem);
	}

	APFSEntry(const ISO_ptr<V> &vol, uint64 file_id, xid x) : vol(vol), file_id(file_id), x(x) {
	}

	malloc_block	GetAttribute(const char *name) {
		if (auto xattr = vol->find(j_keyT<j_key::TYPE_XATTR>(file_id, name), x)->template as<j_key::TYPE_XATTR>()) {
			if (auto ds = xattr->get_dstream())
				return ReadDataStream(ds->id, ds->stream);

			return xattr->get_data();
		}
		return none;
	}

	ISO::Browser2	Deref() {
		auto	k		= j_keyT<j_key::TYPE_INODE>(file_id);
		auto	i		= vol->find_it(k, x);
		if (i.key() == k) {
			auto	inode	= i->template as<j_key::TYPE_INODE>();
			if (inode->bsd_flags & inode->BSD_COMPRESSED) {
				if (auto cmp = GetAttribute("com.apple.decmpfs")) {
					CompressionHeader	*c = cmp;
					return ISO::MakeBrowser(c->Decompress(c->in_resource() ? GetAttribute("com.apple.ResourceFork") : cmp + sizeof(CompressionHeader)));
				}
			}
			if (auto s = inode->xfields(i.raw().length()).template find<xf_blob::TYPE_DSTREAM>()) {
	#if 0
				size_t	size	= s->size;
				malloc_block	mem(size);
				while (i && i.key().id == file_id) {
					if (i.key().type == j_key::TYPE_FILE_EXTENT) {
						auto	log = i.key().as<j_key::TYPE_FILE_EXTENT>()->logical_addr;
						if (log < size) {
							auto	r	= i->as<j_key::TYPE_FILE_EXTENT>()->get_range(vol->block_size);
							vol->file->seek(r.a);
							vol->file->readbuff(mem + log, min(r.b - r.a, size - log));
						}
					}
					++i;
				}
				return ISO::MakeBrowser(move(mem));
	#else
				return ISO::MakeBrowser(ReadDataStream(file_id, *s));
	#endif
			}
		}
		return ISO::Browser();
	}
};

template<typename V> struct APFSDir : ISO::VirtualDefaults {
	struct Entry {
		uint64	file_id;
		xid		x;
		string	name;
		bool	dir;
		Entry(uint64 file_id, xid x, string &&name, bool dir) : file_id(file_id), x(x), name(move(name)), dir(dir) {}
	};
	ISO_ptr<V>	vol;
	uint64		file_id;
	dynamic_array<Entry>	entries;

	APFSDir(const ISO_ptr<V> &vol, uint64 file_id, xid x) : vol(vol), file_id(file_id) {
		auto	k		= j_keyT<j_key::TYPE_INODE>(file_id);
		auto	i		= vol->find_it(k, x);
		if (i.key() == k) {
			auto	inode	= i->template as<j_key::TYPE_INODE>();
			while ((++i).key().id == file_id) {
				if (i.key().type == j_key::TYPE_DIR_REC) {
					auto	k	= i.key().template as<j_key::TYPE_DIR_REC>();
					auto	v	= i->template as<j_key::TYPE_DIR_REC>();
					entries.emplace_back(v->file_id, i.xid(), k->get_name(), !!(v->mode & MODE_DIR));
				}
			}
		}
	}

	uint32	Count() {
		return entries.size32();
	}
	ISO::Browser2 Index(int i) {
		auto	&e = entries[i];
		if (e.dir) {
			return ISO_ptr<APFSDir<V>>(e.name, vol, e.file_id, e.x);
		} else {
			return ISO_ptr<APFSEntry<V>>(e.name, vol, e.file_id, e.x);
		}
	}
	tag2 GetName(int i) {
		return entries[i].name;
	}
};

template<typename V> struct ISO::def<APFSEntry<V>> : ISO::VirtualT<APFSEntry<V>> {};
template<typename V> struct ISO::def<APFSDir<V>> : ISO::VirtualT<APFSDir<V>> {};

struct APFSVol : ISO::VirtualDefaults {
	istream_ptr	file;
	uint32		block_size;
	omap_tree	omapt;
	const btree<j_key, j_val> fs;

	APFSVol(istream_ptr &&file, uint32 block_size, apfs_superblock *apfs) : file(move(file)), block_size(block_size)
		, omapt(apfs->get_omap(file, block_size)->get_tree(file, block_size))
		, fs(apfs->get_tree(file, block_size, (omap_tree*)&omapt)) {
	}
	auto find_it(const j_key &k, xid x) {
		return fs.find_it(k);
	}
	j_val* find(const j_key &k, xid x) {
		auto	i = find_it(k, x);
		if (i.key() == k)
			return i.raw();
		return 0;
	}

};
ISO_DEFVIRT(APFSVol);

namespace {
struct hashu64 {
	uint64	i;
	hashu64() {}
	template<typename T> hashu64(T i) : i(i) {}
	hashu64(uint64 i) : i(i) {}
	friend uint64 hash(const hashu64 &k) { return k.i * 6364136223846793005ull; }//((uint16)k.i) | (k.i & ~bits<uint64>(16)); }
};

struct entry2 {
	paddr	addr;
	xid		x;
	uint64	k0, k1;
	entry2() {}
	entry2(paddr addr, xid x, uint64 k0, uint64 k1) : addr(addr), x(x), k0(k0), k1(k1) {}
};

}

ISO_DEFCOMPV(entry2, addr, x, k0, k1);

ISO_ptr<void> APFSFileHandler::Read(tag id, istream_ref file) {
	if (0) {
		dynamic_array<entry2>	table;
		uint32		block_size	= 0x1000;
		uint64		test_id		= 0x39;
		for (uint64 i = 0, n = file.length() / block_size; i < n; ++i) {
			unique_ptr<obj_phys>	p = read_blocks(file, block_size, i, 1);
			if (p->valid(block_size, obj_phys::TYPE_BTREE_NODE, obj_phys::TYPE_FSTREE)) {
				btree_node_phys	*bt = (btree_node_phys*)get(p);
				if (bt->flags & bt->LEAF) {
					auto	keys = bt->keys_start();
					auto	vals = bt->vals_start(block_size);
					auto	&t0	= bt->toc_var().front();
					auto	&t1	= bt->toc_var().back();
					auto	k0	= (j_key*)get_key(&t0, keys);
					auto	k1	= (j_key*)get_key(&t1, keys);
					table.emplace_back(i, p->x, k0->sortable, k1->sortable);
				}

			}
		}
		sort(table, [](const entry2 &a, const entry2 &b) { return a.k0 < b.k0;});
		FileHandler::Write(ISO::MakePtr(0, table), filename("E:\\table.ib"));
	}
	auto	super0 = read_super(file);
	if (!super0)
		return ISO_NULL;

	ISO_ptr<anything>	result(id);

	nx_superblock*			super		= 0;
	uint32					block_size	= super0->block_size;

	if (super0->xp_desc_blocks & bit(31)) {
		//btree
	} else {
		//contiguous

		malloc_block	desc_block = read_blocks(file, block_size, super0->xp_desc_base, super0->xp_desc_blocks);

		dynamic_array<nx_superblock*>	supers;
		for (auto &i : make_strided<nx_superblock>(desc_block, block_size)) {
			if (i.type == obj_phys::OID_NX_SUPERBLOCK && i.valid()) {
				unique_ptr<omap_phys>	omap	= read_blocks(file, block_size, i.omap_oid, 1);
				if (omap->valid(block_size)) {
					supers.push_back(&i);
				}
			}
		}

		for (auto super : supers) {
			//super = latest_super(desc_block, block_size);
			auto	cps	= get_checkpoints(desc_block, block_size, super);

			auto	omapt	= super->get_omap(file, block_size)->get_tree(file, block_size);

			for (auto &i : super->fs_oid) {
				if (i && omapt.contains(i, super->x)) {
					unique_ptr<apfs_superblock>	apfs = omapt.read_blocks(file, block_size, i, super->x);

					if (apfs && apfs->valid(block_size)) {
						auto	omapt	= apfs->get_omap(file, block_size)->get_tree(file, block_size);
						auto	fs		= apfs->get_tree(file, block_size, &omapt);
						ISO_ptr<APFSVol>			vol(0, file.clone(), block_size, apfs);
						ISO_ptr<APFSDir<APFSVol>>	root(apfs->volname, vol, apfs_superblock::INODE_ROOT_DIR, 0);
						result->Append(root);
					}
				}
			}
		}
	}


	return result;
}

struct APFSVol2 : ISO::VirtualDefaults {
	dynamic_array<entry2> table;
	istream_ptr			file;
	uint32				block_size;

	mutable hash_map<int, unique_ptr<btree_node_phys>>	nodes;

	btree_node_phys	*get_node(const entry2 *e) const {
		auto	i = nodes[table.index_of(e)];
		if (i.exists())
			return *i;
		unique_ptr<btree_node_phys> n = read_blocks(file, block_size, e->addr, 1);
		return i = move(n);
	}

	struct key_getter {
		const btree_node_phys::traverser	&t;
		key_getter(const btree_node_phys::traverser &t) : t(t) {}
		memory_block	operator[](size_t i)	const	{ return t.key(0, i); }
		friend auto num_elements(key_getter &a) { return a.t.size(); }
	};

	struct iterator {
		APFSVol2	*vol;
		const entry2 *e;
		btree_node_phys::traverser	node;
		iterator(APFSVol2 *vol, const entry2 *e, btree_node_phys::traverser node) : vol(vol), e(e), node(node) {}
		explicit operator bool()			const { return !!vol; }

		auto	xid()						const { return node.x; }
		auto	raw_key()					const { return node.key(0); }
		auto	raw()						const { return node.val(0); }
		j_key&	key()						const { return *(j_key*)raw_key(); }
		auto	operator*()					const { return *(j_val*)raw(); }
		auto	operator->()				const { return (j_val*)raw(); }
		bool operator==(const iterator &b)	const { return node.toc == b.node.toc; }
		bool operator!=(const iterator &b)	const { return node.toc != b.node.toc; }

		iterator& operator++() {
			j_key	&k = *node.key(0);
			if (!node.next() || ((j_key*)node.key(0))->id != k.id) {
				for (int n = 8; n-- && (++e)->k0 > k.sortable;)
					;
				node = {vol->get_node(e), vol->block_size};
				auto	i = lower_boundc(make_indexed_container(key_getter(node)), k, [](j_key *a, const j_key &b) { return *a < b; });
				node.next(i.index());
			}
			return *this;
		}
	};

	APFSVol2(istream_ptr &&file, uint32 block_size, dynamic_array<entry2> &&table) : file(move(file)), block_size(block_size), table(move(table)) {}

	auto find_it(const j_key &k, xid x) {
		uint64	s = k.sortable;
		auto	i = lower_boundc(table, s, [](const entry2 &e, uint64 k) { return e.k1 < k; });

		if (x) {
			while (s >= i[1].k0 && x < i[1].x)
				++i;
		}

		for (;;) {
			btree_node_phys::traverser	tr(get_node(i), block_size);
			auto	i2 = lower_boundc(make_indexed_container(key_getter(tr)), k, [](j_key *a, const j_key &b) { return *a < b; });
			if (tr.next(i2.index()))
				return iterator(this, i, tr);
			++i;
		}
	}
	j_val* find(const j_key &k, xid x) {
		auto	i = find_it(k, x);
		if (i.key() == k)
			return i.raw();
		return 0;
	}

};
ISO_DEFVIRT(APFSVol2);

ISO_ptr<void> RecoverAPFS3(ISO_ptr<dynamic_array<entry2>> table, ISO_ptr<void> raw) {
	static const uint64		block_size	= 0x1000;

	sort(*table, [](const entry2 &a, const entry2 &b) {
		return a.k1 != b.k1 ? a.k1 < b.k1 : a.x < b.x;
	});


	ISO_ptr<APFSVol2>			vol(0, new BigBinStream(raw), block_size, move(*table));
	ISO_ptr<APFSDir<APFSVol2>>	root("recovered", vol, apfs_superblock::INODE_ROOT_DIR, 0);
	return root;
}

ISO_ptr<void> RecoverAPFS4(ISO_ptr<dynamic_array<entry2>> table, ISO_ptr<void> raw) {
	static const uint64		block_size	= 0x1000;

	struct Dir : dirs::Dir {
		entry2	*block;
		Dir		*parent;
		uint64	id;
	};

	hash_map<hashu64, Dir*>	hash;

	BigBinStream	file(raw);
	for (auto &i : *table) {
		unique_ptr<btree_node_phys>	node = read_blocks(file, block_size, i.addr, 1);
		btree_node_phys::traverser	tr(node, block_size);
		for (int j = 0, n = uint32(tr.size()); j < n; j++) {
			j_key			*k	= tr.key(0, j);
			memory_block	v	= tr.val(0, j);
			if (k->type == j_key::TYPE_INODE) {
				j_valT<j_key::TYPE_INODE>	*inode = v;
				if (auto name = inode->xfields(v.length()).find<xf_blob::TYPE_NAME>()) {
					auto	&parent = hash[inode->parent_id].put();
					if (!parent)
						parent = new Dir;

					if (inode->mode & MODE_DIR) {
						auto	&me = hash[k->id].put();
						if (!me)
							me = new Dir;

						me->name	= name->name;
						me->id		= get(k->id);
						me->parent	= parent;
						me->block	= &i;

						parent->subdirs.push_back(me);
					} else {
						ISO_ASSERT(name->name != str("280003.emlx"));
						size_t	size = 0;
						if (auto ds = inode->xfields(v.length()).find<xf_blob::TYPE_DSTREAM>())
							size = ds->size;
						parent->entries.push_back(new dirs::Entry(name->name, size));
					}
				}
			}
		}
	}
	return MakePtr("recovered", (dirs::Dir&)*hash[apfs_superblock::INODE_ROOT_DIR]);
}

static initialise init(
	ISO_get_operation(RecoverAPFS3),
	ISO_get_operation(RecoverAPFS4)
);



