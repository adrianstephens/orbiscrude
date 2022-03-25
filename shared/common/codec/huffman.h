#ifndef HUFFMAN_H
#define HUFFMAN_H

#include "base/defs.h"
#include "base/algorithm.h"
#include "vlc.h"

namespace iso {

//-----------------------------------------------------------------------------
//	Static Huffman Helpers
//-----------------------------------------------------------------------------

template<typename T> struct HUFF_node {
	T		child[2];//left, right;
	void count_leafs(T nc, int node, int counts[], int depth, int max_depth) const;
};

template<typename T> void HUFF_node<T>::count_leafs(T nc, int node, int counts[], int depth, int max_depth) const {
	if (node < nc) {
		++counts[min(depth, max_depth)];
	} else {
		count_leafs(nc, this[node - nc].child[0], counts, depth + 1, max_depth);
		count_leafs(nc, this[node - nc].child[1], counts, depth + 1, max_depth);
	}
}

template<int CB, typename T, typename V> T HUFF_decode(V &v, T *table, uint8 *codelens, int tablebits) {
	T	j = table[v.peek(tablebits)];
	v.discard(codelens[j]);
	return j;
}

template<int CB, typename T, typename V> T HUFF_decode(V &v, T *table, uint8 *codelens, const HUFF_node<T> *tree, int tablebits, int nc) {
	T	j = table[v.peek(tablebits)];
	if (j < nc) {
		v.discard(codelens[j]);
	} else {
		v.discard(tablebits);
#if 1
		auto	x = v.get_stack(CB - tablebits);
		do
			j = tree[j].child[x.get_bit()];
		while (j >= nc);
		v.discard(codelens[j] - tablebits);
#else
		do
			j = tree[j][v.get_bit()];
		while (j >= nc);
#endif
	}
	return j;
}

template<int CB, typename T, typename C> C *HUFF_make_fixed(const T *ends, T nc, uint8 *codelens, C *codes) {
	C		code	= 0;
	int		codelen	= 1;

	for (int i = 0; i < nc; i++) {
		while (*ends == i) {
			++codelen;
			++ends;
		}
		codelens[i]	= codelen;
		codes[i]	= code >> (CB - codelen);
		code	+= 1 << (CB - codelen);
	}

	return codes;
}

template<int CB, typename T, typename C> C *HUFF_make_codes(T nc, const uint8 *codelens, C *codes, const int *counts) {
	C	start[CB + 1]	= {0};

	// calculate first code for each codelen & check total
	C	total = 0;
	for (int i = 1; i <= CB; i++) {
		start[i] = total >> (CB - i);
		total	+= counts[i] << (CB - i);
	}

	if (total & ((C(1) << CB) - 1))
		return 0;	// error

	for (T i = 0; i < nc; i++)
		codes[i] = start[codelens[i]]++;

	return codes;
}

template<int CB, typename T> int *HUFF_get_counts(T nc, const uint8 *codelens, int *counts) {
	for (int i = 1; i <= CB; i++)
		counts[i] = 0;

	for (T i = 0; i < nc; i++) {
		if (codelens[i] > CB)
			return 0;
		counts[codelens[i]]++;
	}
	return counts;
}

//decoding

template<bool BE> struct HUFF_endian;

template<> struct HUFF_endian<true> {
	template<typename T, typename C> static void set_table(C code, int codelen, int value, T *table, int tablebits) {
		for (int i = code << (tablebits - codelen), e = (code + 1) << (tablebits - codelen); i < e; i++)
			table[i] = value;
	}
	template<typename T, typename C> static T set_tree(C code, int codelen, int value, T *table, int tablebits, HUFF_node<T> *tree, T nextcode) {
		T	*p	= &table[code >> (codelen - tablebits)];
		for (int n = codelen - tablebits; n--;) {
			if (*p == 0) {
				clear(tree[nextcode]);
				*p = nextcode++;
			}
			p = &tree[*p].child[(code >> n) & 1];
		}
		*p = value;
		return nextcode;
	}
};

template<> struct HUFF_endian<false> {
	template<typename T, typename C> static void set_table(C code, int codelen, int value, T *table, int tablebits) {
		for (int i = code, e = 1 << tablebits; i < e; i += 1 << codelen)
			table[i] = value;
	}
	template<typename T, typename C> static T set_tree(C code, int codelen, int value, T *table, int tablebits, HUFF_node<T> *tree, T nextcode) {
		T	*p	= &table[code >> (codelen - tablebits)];
		for (int n = tablebits; n < codelen; ++n) {
			if (*p == 0) {
				clear(tree[nextcode]);
				*p = nextcode++;
			}
			p = &tree[*p].child[(code >> n) & 1];
		}
		*p = value;
		return nextcode;
	}
};

// make tree: for encoding
template<int CB, typename T, typename C> int HUFF_make_tree(T nc, uint16 *freq, uint8 *codelens, C *codes, HUFF_node<T> *tree) {
	int16	*indices	= alloc_auto(int16, nc);
	uint32	heapsize	= 0;

	for (int i = 0; i < nc; i++) {
		codelens[i] = 0;
		if (freq[i])
			indices[heapsize++] = i;
	}

	if (heapsize < 2) {
		codes[indices[0]] = 0;
		return indices[0];
	}

	auto	heap_begin = make_indexed_iterator(freq, indices);
	heap_make(heap_begin, heap_begin + heapsize);

	int		root	= 0;
	int		avail	= nc;
	C		*sort	= codes;
	while (heapsize > 1) {					// while queue has at least two entries
		int	i = indices[0];					// take out least-freq entry
		if (i < nc)
			*sort++ = i;

		heap_pop(heap_begin, heap_begin + heapsize--);

		int	j = indices[0];					// next least-freq entry
		if (j < nc)
			*sort++ = j;

		// generate new node
		root		= avail++;
		freq[root]	= freq[i] + freq[j];
		tree[root - nc].child[0] = i;
		tree[root - nc].child[1] = j;

		// put into queue
		indices[0]		= root;
		heap_siftdown(heap_begin, heap_begin + heapsize, heap_begin);
	}

	int	count[CB + 1] = {0};
	tree->count_leafs(nc, root, count, 0, CB);

	C total = 0;
	for (int i = CB; i > 0; i--)
		total += count[i] << (CB - i);

	// adjust len
	if (total &= ((1 << CB) - 1)) {
		count[CB] -= total;
		while (total--) {
			for (int i = CB; --i; ) {
				if (count[i]) {
					--count[i];
					count[i + 1] += 2;
					break;
				}
			}
		}
	}

	// set code lengths
	sort = codes;
	for (int i = CB; i > 0; i--) {
		for (int k = count[i]; k--;)
			codelens[*sort++] = i;
	}

	HUFF_make_codes<CB>(nc, codelens, codes, count);
	return root;
}

//-----------------------------------------------------------------------------
//	Static Huffman
//-----------------------------------------------------------------------------

// NC: max number of tokens
// CB: bits necessary to hold huffman codes
// IB: prefix bits in table

template<int NC, int IB, int CB> struct THUFF_base {
	enum { TB = LOG2_CEIL(NC) };

	typedef	uint_bits_t<TB>	T;
	typedef	uint_bits_t<CB>	C;

	uint8	bitlen[NC];

	T		last(T nc = NC) {
		while (nc > 0 && bitlen[nc - 1] == 0)
			--nc;
		return nc;
	}
	void	clear_from(T i, T nc = NC) {
		while (i < nc)
			bitlen[i++] = 0;
	}
};

// decoder
template<int NC, int IB, int CB> struct THUFF_decoder_base : THUFF_base<NC, IB, CB> {
	typedef THUFF_base<NC, IB, CB>	B;
	using typename B::T;

	T			table[1 << IB];

	void		fill_table(T c) {
		for (auto &i : table)
			i = c;
	}
	template<typename V> static T	get_raw(V &v) {
		return v.get(B::TB);
	}
};

template<int NC, int IB, int CB = 16, bool BE = true, bool = (CB > IB)> struct THUFF_decoder;

// with tree
template<int NC, int IB, int CB, bool BE> struct THUFF_decoder<NC, IB, CB, BE, true> : THUFF_decoder_base<NC, IB, CB> {
	typedef THUFF_decoder_base<NC, IB, CB>	B;
	using typename B::T;
	using typename B::C;

	HUFF_node<T>	tree[NC];

	bool		make_table(const C *codes, const uint8 *bitlen, T nc = NC) {
		if (!codes)
			return false;

		clear(B::table);
		T		nextcode = nc;
		for (int i = 0; i < nc; i++) {
			C	code	= codes[i];
			int	len		= bitlen[i];
			if (len <= IB) 
				HUFF_endian<BE>::set_table(code, len, i, B::table, IB);									// code in table
			else
				nextcode = HUFF_endian<BE>::set_tree(code, len, i, B::table, IB, tree - nc, nextcode);	// code not in tree
		}
		return true;
	}
	bool		make_table(T nc = NC) {
		int		count[CB + 1]	= {0};
		C		codes[NC];
		return make_table(HUFF_make_codes<CB>(nc, B::bitlen, codes, HUFF_get_counts<CB>(nc, B::bitlen, count)), B::bitlen, nc);
	}
	bool		make_fixed_decoder(const T* ends, T nc = NC) {
		C		codes[NC];
		return make_table(HUFF_make_fixed<CB>(ends, nc, B::bitlen, codes), B::bitlen, nc);
	}
	template<typename V> T	decode(V &v, T nc = NC) {
		return HUFF_decode<CB>(v, B::table, B::bitlen, tree, IB, nc);
	}
};

// without tree
template<int NC, int IB, int CB, bool BE> struct THUFF_decoder<NC, IB, CB, BE, false> : THUFF_decoder_base<NC, IB, CB> {
	typedef THUFF_decoder_base<NC, IB, CB>	B;
	using typename B::T;
	using typename B::C;

	bool		make_table(const C *codes, const uint8 *bitlen, T nc = NC) {
		if (!codes)
			return false;

		for (int i = 0; i < nc; i++)
			HUFF_endian<BE>::set_table(codes[i], bitlen[i], i, B::table, IB);
		return true;
	}
	bool		make_table(T nc = NC) {
		int		count[CB + 1]	= {0};
		C		codes[NC];
		return make_table(HUFF_make_codes<CB>(nc, B::bitlen, codes, HUFF_get_counts<CB>(nc, B::bitlen, count)), B::bitlen, nc);
	}
	bool		make_fixed_decoder(const T* ends, T nc = NC) {
		C		codes[NC];
		return make_table(HUFF_make_fixed<CB>(ends, nc, B::bitlen, codes), B::bitlen, nc);
	}
	template<typename V> T	decode(V &v, T nc = NC) {
		return HUFF_decode<CB>(v, B::table, B::bitlen, IB);
	}
};

// encoder
template<int NC, int IB, int CB = 16, bool BE=true> struct THUFF_encoder : THUFF_base<NC, IB, CB> {
	typedef THUFF_base<NC, IB, CB>	B;
	using typename B::T;
	using typename B::C;

	C		code[NC];
	uint16	freq[2 * NC - 1];

	void		make_fixed_encoder(const T* ends, T nc = NC) {
		HUFF_make_fixed<CB>(ends, nc, B::bitlen, code);
	}
	int			make_tree(T nc = NC) {
		HUFF_node<T>	tree[NC];
		return HUFF_make_tree<CB>(nc, freq, B::bitlen, code, tree);
	}
	template<typename V> static void	put_raw(V &v, T c) {
		v.put(c, B::TB);
	}
	template<typename V> void			encode(V &v, T c) {
		v.put(code[c], B::bitlen[c]);
	}
};

//-----------------------------------------------------------------------------
//	Dynamic Huffman
//-----------------------------------------------------------------------------

class DYNHUFF_base {
protected:
	// block is region of same frequency
	struct ENTRY;
	struct BLOCK {
		union {
			ENTRY	*first;
			BLOCK	*free;
		};
	};

	struct ENTRY {
		BLOCK	*block;
		ENTRY	*parent;
		int16	child;		//-ve -> leaf of ~child
		uint16	freq;
		BLOCK*	start_block(BLOCK *b) {
			block		= b;
			b->first	= this;
			return b;
		}
		constexpr bool is_leaf() const { return child < 0; }
	};

	BLOCK	*next_avail;

	BLOCK*	alloc_block() {
		return exchange(next_avail, next_avail->free);
	}
	void	free_block(BLOCK *b) {
		b->free = exchange(next_avail, b);
	}

	static void	set_parent(ENTRY *entry, ENTRY **node, int c, ENTRY *parent) {
		if (c < 0)
			node[~c] = parent;
		else
			entry[c].parent = entry[c - 1].parent = parent;
	}


	DYNHUFF_base(BLOCK *pool, int n);

	void	start(ENTRY *entry, ENTRY **node, uint32 nc);
	void	remake(ENTRY *entry, ENTRY **node, uint32 nc);
	ENTRY*	increment(ENTRY *entry, ENTRY **node, ENTRY *p);
	void	add_leaf(ENTRY *entry, ENTRY **node, uint32 c, uint32 nc);

	template<typename V> uint16 decode(V &v, ENTRY *entry) {
		int		buf = v.peek(32);
		int		cnt = 0;
		int		c	= entry[0].child;
		while (c > 0) {
			c	= entry[c - int(buf < 0)].child;
			buf <<= 1;
			++cnt;
		}

		v.discard(cnt);
		return ~c;
	}

	template<typename V> void encode(V &v, ENTRY *entry, ENTRY *p) {
		int		cnt		= 0;
		uint32	bits	= 0;
		while (p) {
			bits |= ((p - entry) & 1) << cnt++;
			p = p->parent;
		}

		v.put(bits >> (32 - cnt), cnt);
	}

};

template<int N> struct DYNHUFF : DYNHUFF_base {
	typedef	DYNHUFF_base B;

	BLOCK	block_pool[N * 2];
	ENTRY	entry[N * 2];
	ENTRY	*node[N];			// leaf -> entry

	DYNHUFF() :	B(block_pool, N * 2) {}

	void start(uint32 nc = N) {
		B::start(entry, node, nc);
	}

	void remake(uint32 nc = N) {
		B::remake(entry, node, nc);
	}

	void update(uint16 c) {
		if (entry[0].freq == 0x8000)
			remake();

		++entry[0].freq;
		for (auto p = node[c]; p != entry;)
			p = increment(entry, node, p);
	}

	void add_leaf(uint32 c, uint32 nc) {
		B::add_leaf(entry, node, c, nc);
		update(c);
	}

	template<typename V> uint16 decode(V &v) {
		return B::decode(v, entry);
	}

	template<typename V> void encode(V &v, uint16 c) {
		B::encode(v, entry, node[c]);
	}

};

} // namespace iso
#endif //HUFFMAN_H
