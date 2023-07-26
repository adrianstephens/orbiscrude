#include "regex.h"
#include "base/hash.h"
#include "base/sparse_array.h"
#include "base/tree.h"
#include "base/algorithm.h"
#include "unicode_tables.h"

//#define REGEX_DIAGNOSTICS

namespace iso { namespace re2 {
//-----------------------------------------------------------------------------
//	EmptyOp
//-----------------------------------------------------------------------------

// Bit flags for empty-width specials
enum EmptyOp {
	kEmptyNone 				= 0,
	kEmptyWordBoundary 		= 1 << 0,		// \b - word boundary
	kEmptyNonWordBoundary	= 1 << 1,		// \B - not \b
	kEmptyBeginText 		= 1 << 2,		// \A - beginning of text
	kEmptyEndText 			= 1 << 3,		// \z - end of text
	kEmptyBeginLine 		= 1 << 4,		// ^  - beginning of line
	kEmptyEndLine 			= 1 << 5,		// $  - end of line
	kEmptyBeginWord 		= 1 << 6,		// \< - beginning of word
	kEmptyEndWord 			= 1 << 7,		// \> - end of word
	kEmptyBits 				= 8,

	// search only
	kEmptyAnchor			= 1 << 8,
	kEmptyLongest			= 1 << 9,
//	kEmptyNoEndLines		= 1 << 10,		// do not match $ to \n

	//DFA states only
	kFlagMatch				= kEmptyAnchor,	// this is a matching state
	kFlagLastWord			= 1 << 10,		// last byte was a word char

	kFlagBits 				= 11,

	kEmptyNever		= kEmptyWordBoundary | kEmptyNonWordBoundary,
	kEmptyBegins	= kEmptyBeginText | kEmptyBeginLine | kEmptyBeginWord,
	kEmptyEnds		= kEmptyBegins << 1,
};

ENUM_FLAGOPS(EmptyOp);
/*
inline EmptyOp	operator&(EmptyOp a, EmptyOp b)		{ return EmptyOp(int(a) & int(b)); }
inline EmptyOp	operator|(EmptyOp a, EmptyOp b)		{ return EmptyOp(int(a) | int(b)); }
inline EmptyOp	operator-(EmptyOp a, EmptyOp b)		{ return EmptyOp(int(a) & ~int(b)); }
inline EmptyOp&	operator&=(EmptyOp &a, EmptyOp b)	{ return a = a & b; }
inline EmptyOp&	operator|=(EmptyOp &a, EmptyOp b)	{ return a = a | b; }
inline EmptyOp&	operator-=(EmptyOp &a, EmptyOp b)	{ return a = a - b; }
*/
//reverse begin/end flags
inline EmptyOp	reverse(EmptyOp op)					{ return EmptyOp(op & ~(kEmptyBegins|kEmptyEnds) | ((op & kEmptyBegins) << 1) | ((op & kEmptyEnds) >> 1)); }
inline bool		check(EmptyOp a, EmptyOp b)			{ return !(a & ~b); }

// Returns the set of kEmpty flags that are in effect at position p
inline EmptyOp EmptyFlags(EmptyOp enable, const char* p) {
	EmptyOp		flags		= kEmptyNone;
	bool		was_word	= false;
	bool		is_word		= false;

	// ^ \A \<
	if (enable & kEmptyBeginText) {
		flags |= kEmptyBeginText | kEmptyBeginLine;
	} else {
		if ((enable & kEmptyBeginWord) && p) {
			char	c = p[-1];
			if ((enable & kEmptyBeginLine) && c == '\n')
				flags |= kEmptyBeginLine;
			was_word = is_wordchar(c);
		}
	}

	// $ \z \>
	if (enable & kEmptyEndText) {
		flags |= kEmptyEndText | kEmptyEndLine;
	} else {
		if ((enable & kEmptyEndWord) && p) {
			char	c = p[0];
			if ((enable & kEmptyEndLine) && (c == '\r' || c == '\n'))
				flags |= kEmptyEndLine;
			is_word	= is_wordchar(c);
		}
	}

	// \b \B
	return flags | (is_word == was_word ? kEmptyNonWordBoundary : (kEmptyWordBoundary | (is_word ? kEmptyBeginWord : kEmptyEndWord)));
}

//-----------------------------------------------------------------------------
//	Prog - Compiled form of regexp program
//-----------------------------------------------------------------------------

// Opcodes for Inst
enum InstOp {
	kInstAlt = 0,		// choose between out and out1 (removed by Prog::Flatten)
	kInstAltMatch,		// out is AnyByte and back, out1 is match; or vice versa (inserted by Prog::Flatten)
	kInstByteRange,		// next (possible case-folded) byte must be in [lo, hi]
	kInstCapture,		// capturing parenthesis number cap
	kInstEmptyWidth,	// empty-width special (^ $ ...); bit(s) set in empty
	kInstMatch,			// found a match
	kNumInst,
};

class Prog {
public:
	struct Inst {
		uint32 op:3, last:1, extra:4, out:24;
		union {
			uint32	out1;						// opcode == kInstAlt - alternate next instruction
			int32	cap;						// opcode == kInstCapture
			int32	match_id;					// opcode == kInstMatch
			uint32	mask;						// opcode == kInstByteRange; mask >> (extra * 32) (extra & 8 -> foldcase)
			struct { uint16 lobank, hibank; };	//		extra = 15: between(c>>5, lobank, hibank)
			EmptyOp empty;						// opcode == kInstEmptyWidth
		};
		Inst(InstOp op, uint32 out = 0) : op(op), last(0), out(out) {}

		bool	IsHead()		const	{ return this[-1].last; }
		bool	IsNop()			const	{ return op == kInstEmptyWidth && empty == kEmptyNone; }
		bool	IsAnyByte()		const	{ return op == kInstByteRange && extra == 15 && lobank == 0 && hibank >= 7; }
		bool	Matches(int c)	const	{ return extra == 15 ? iso::between(c >> 5, lobank, hibank) : (extra & 7) == ((c >> 5) & ~(extra >> 3)) && (mask & (1 << (c & 31))); }
		int		SingleChar()	const	{ return (extra & 8) || !is_pow2(mask) ? -1 : lowest_set_index(mask) + extra * 32; }
		bool	operator==(const Inst &b) const { return op == b.op && extra == b.extra && mask == b.mask; }
	};

	dynamic_array<Inst>	inst;
	int				inst_count[kNumInst];	// count of instructions by opcode

	union {
		uint32	flags;
		struct { uint32
			anchor_start	:1,		// regexp has explicit start anchor
			anchor_end		:1,		// regexp has explicit end anchor
			reversed		:1,		// program runs backward over input
			many_match		:1,
			flattened		:1;
		};
	};

	uint32			start;					// entry point for program
	uint32			start_unanchored;		// unanchored entry point for program
	uint32			max_capture;
	int				first_byte;				// required first byte for match, or -1 if none
	uint8			bytemap[256];
	int				bytemap_entries;
	malloc_block	onepass_states;
	class DFA		*dfa;

	static int		ComputeFirstByte(Inst *inst, size_t count, uint32 start);

	void	Flatten();

	Inst	*SkipCapNop(Inst* ip) {
		while (ip->op == kInstCapture || ip->IsNop())
			ip = &inst[ip->out];
		return ip;
	}

public:
	Prog() : flags(0), start(0), start_unanchored(0), max_capture(0), first_byte(-2), dfa(nullptr) {}
	~Prog();

	int FirstByte() {
		if (first_byte == -2)
			first_byte = ComputeFirstByte(inst, inst.size(), start);
		return first_byte;
	}
	intptr_t SkipToFirstByte(const char *p, const char *end) {
		if (first_byte == -2)
			first_byte = ComputeFirstByte(inst, inst.size(), start);

		if (first_byte < 0)
			return 0;

		const void *p2 = memchr(p, first_byte, end - p);
		return p2 ? (const char*)p2 - p : end - p;
	}
	uint32	NumCaptures()	const { return max_capture + 1; }
	int32	Search(const char **matches, uint32 nmatches, const count_string& text, EmptyOp enable);
};

// Computes whether all successful matches have a common first byte, and if so, returns that byte (else -1)
int Prog::ComputeFirstByte(Inst *inst, size_t count, uint32 start) {
	sparse_set<int, uint8> q(count);
	q.insert(start);

	int b = -1;

	for (auto id : make_dynamic(q)) {
		Prog::Inst	&ip = inst[id];
		switch (ip.op) {
			default:
				ISO_ASSERT(0);
				break;

			case kInstMatch:
				// empty string matches: no first byte
				return -1;

			case kInstByteRange: {
				// Must match only a single byte, and match current one (if any)
				int	c = ip.SingleChar();
				if (c == -1 || (b != -1 && b != c))
					return -1;
				b = c;
				if (!ip.last)
					q.insert(id + 1);
				break;
			}
			case kInstEmptyWidth:
				// Ignore ip.empty flags in order to be as conservative as possible (assume all possible empty-width flags are true)
			case kInstCapture:
				if (!ip.last)
					q.insert(id + 1);
				// Continue on
				if (ip.out)
					q.insert(ip.out);
				break;

			case kInstAltMatch:
				q.insert(id + 1);
				break;
		}
	}
	return b;
}

void Prog::Flatten() {
	if (flattened)
		return;
	flattened = true;

	sparse_set<uint32, uint16>	rootmap(inst.size());
	sparse_set<uint32, uint16>	reachable(inst.size());
	dynamic_array<uint32>		stk;
	stk.reserve(inst.size());

	// First pass: Mark "successor roots" and predecessors

	sparse_map<dynamic_array<int>, uint32, uint16>	predmap(inst.size());

	rootmap.insert(0);
	rootmap.insert(start_unanchored);
	rootmap.insert(start);

	uint32	id = start_unanchored;

	for (;;) {
		if (!reachable.check_insert(id)) {
			Inst	&ip = inst[id];
			switch (ip.op) {
				default:
					break;

				case kInstAltMatch:				// (shouldn't exist yet)
				case kInstAlt: {
					// Mark this instruction as a predecessor of each out
					predmap[ip.out]->push_back(id);
					predmap[ip.out1]->push_back(id);
					stk.push_back(ip.out1);
					id = ip.out;
					continue;
				}

				case kInstEmptyWidth:
					id = ip.out;
					if (ip.empty)
						rootmap.insert(id);
					continue;

				case kInstByteRange:
				case kInstCapture:
					id = ip.out;
					rootmap.insert(id);
					continue;
			}
		}

		if (stk.empty())
			break;

		id = stk.pop_back_value();
	}

	// Second pass: Mark "dominator roots"

	dynamic_array<int> sorted	= rootmap;
	sort<greater>(sorted);

	for (auto root : sorted) {
		if (root != start_unanchored && root != start) {
			reachable.clear();

			for (int id = root;;) {
				if (!reachable.check_insert(id) || id == root || !rootmap.count(id)) {
					// reached another "tree" via epsilon transition
					Inst	&ip = inst[id];
					switch (ip.op) {
						default:
							break;

						case kInstEmptyWidth:	// added for nop
							if (ip.empty)
								break;
							id = ip.out;
							continue;

						case kInstAltMatch:		// (shouldn't exist yet)
						case kInstAlt:
							stk.push_back(ip.out1);
							id = ip.out;
							continue;
					}
				}
				if (stk.empty())
					break;

				id = stk.pop_back_value();
			}

			for (auto id : reachable) {
				if (auto *p = predmap.check(id)) {
					for (int pred : *p) {
						// if id has a predecessor that cannot be reached from root, id must be a "root" too
						if (!reachable.count(pred))
							rootmap.insert(id);
					}
				}
			}
		}
	}

	// Third pass: Emit "lists"; remaps outs to root-ids, builds the mapping from root-ids to flat-ids

	dynamic_array<int>	flatmap(rootmap.size());
	dynamic_array<Inst>	flat;
	flat.reserve(inst.size());

	for (auto &i : rootmap) {
		flatmap[rootmap.index_of(i)] = flat.size32();
		reachable.clear();

		for (int root = i, id = root;;) {
			if (!reachable.check_insert(id)) {
				if (id != root && rootmap.count(id)) {
					// reached another "tree" via epsilon transition, so emit a Nop so we don't become quadratically larger
					flat.emplace_back(kInstEmptyWidth, rootmap.get_index(id)).empty	= kEmptyNone;

				} else {
					Inst	&ip = inst[id];
					switch (ip.op) {
						default:
							flat.push_back(ip);
							break;

						case kInstAlt: {
							// change to kInstAltMatch if
							//	ip: Alt -> j, k
							//	j: AnyByte -> ip
							//	k: Match
							// or the reverse (the above is the greedy one)
#if 1
							Inst&	jp	= inst[ip.out];
							Inst&	kp	= inst[ip.out1];
							if ((jp.IsAnyByte() && jp.out == id && SkipCapNop(&kp)->op == kInstMatch)	//greedy
							||	(kp.IsAnyByte() && kp.out == id && SkipCapNop(&jp)->op == kInstMatch)	//non-greedy
							) {
								auto &ip2	= flat.push_back(kInstAltMatch);
								ip2.out		= flat.size32();
								ip2.out1	= flat.size32() + 1;
							}
#endif
							stk.push_back(ip.out1);
							id = ip.out;
							continue;
						}

						case kInstEmptyWidth:
							if (ip.empty == kEmptyNone) { //kInstNop
								id = ip.out;
								continue;
							}
						//FALLTHROUGH_INTENDED;
						case kInstByteRange:
						case kInstCapture:
							flat.push_back(ip).out = rootmap.get_index(ip.out);
							break;
					}
				}
			}
			if (stk.empty())
				break;

			id = stk.pop_back_value();
		}

		flat.back().last = true;
	}

	// Fourth pass: Remaps outs to flat-ids & counts instructions by opcode

	clear(inst_count);

	for (auto &ip : flat) {
		if (ip.op != kInstAltMatch)	// handled in pass 3
			ip.out = flatmap[ip.out];
		++inst_count[ip.op];
	}

	// Remap start_unanchored and start
	if (start_unanchored) {
		start				= flatmap[start == start_unanchored ? 1 : 2];
		start_unanchored	= flatmap[1];
	} else {
		ISO_ASSERT(!start);
	}

	inst = move(flat);
}

//-----------------------------------------------------------------------------
//	ByteMapBuilder - implements a colouring algorithm
//-----------------------------------------------------------------------------

// The first phase is a series of "mark and merge" batches: we mark one or more [lo-hi] ranges, then merge them into our internal state
// Internally, the ranges are represented using a bitmap that stores the splits and a vector that stores the colours; both of them are indexed by the ranges' last bytes
// In order to merge a [lo-hi] range, we split at lo-1 and at hi (if not already split), then recolour each range inbetween
// The colour map (i.e. from the old colour to the new colour) is maintained for the lifetime of the batch and so underpins this somewhat obscure approach to set operations
//
// The second phase builds the bytemap from our internal state: we recolour each range, then store the new colour (which is now the byte class) in each of the corresponding array elements

class ByteMapBuilder {
	bitarray<256>					splits;
	dynamic_array<int>				colours;
	dynamic_array<pair<int, int>>	colour_map;
	dynamic_array<pair<int, int>>	ranges;
	int								next_colour;

	int Recolour(int old_colour) {
		if (auto i = find_if_check(colour_map, [=](const pair<int, int> &kv) { return kv.a == old_colour || kv.b == old_colour; }))
			return i->b;
		colour_map.emplace_back(old_colour, next_colour);
		return next_colour++;
	}

public:
	ByteMapBuilder() : colours(256), next_colour(257) {
		// initially the [0-255] range has color 256 to avoid problems during the second phase in which we assign byte classes numbered from 0
		splits.set(255);
		colours[255] = 256;
	}

	bool Mark(int lo, int hi) {
		if (lo <= hi && (lo != 0 || hi != 255)) {
			ranges.emplace_back(lo, hi);
			return true;
		}
		return false;
	}
	void	Merge();
	int		Build(uint8 bytemap[256]);
};

void ByteMapBuilder::Merge() {
	for (auto &i : ranges) {
		int		lo = i.a - 1;
		int		hi = i.b;

		if (lo >= 0 && !splits[lo]++)
			colours[lo] = colours[splits.next(lo + 1, true)];

		if (!splits[hi]++)
			colours[hi] = colours[splits.next(hi + 1, true)];

		while (lo < hi) {
			lo			= splits.next(lo + 1, true);
			colours[lo]	= Recolour(colours[lo]);
		}
	}
	colour_map.clear();
	ranges.clear();
}

// Assign byte classes numbered from 0
int ByteMapBuilder::Build(uint8 bytemap[256]) {
	next_colour = 0;
	for (int c = 0; c < 256;) {
		int		next	= splits.next(c, true);
		uint8	b		= (uint8)Recolour(colours[next]);
		while (c <= next)
			bytemap[c++] = b;
	}

	return next_colour;
}

// Ranges of bytes that are treated indistinguishably will be mapped to a single byte class
int ComputeByteMap(const dynamic_array<Prog::Inst> &inst, uint8 bytemap[256]) {
	ByteMapBuilder builder;

	bool marked_line_boundaries = false;	// Don't repeat the work for ^ and $
	bool marked_word_boundaries = false;	// Don't repeat the work for \b and \B

	for (auto &ip : inst) {
		if (ip.op == kInstByteRange) {
			bool	merge = false;
			if (ip.extra == 15) {
				if (ip.lobank < 4) {
					merge = builder.Mark(ip.lobank << 5, min((ip.hibank << 5) | 31, 255));
				} else {
					// add prefix bytes for all the banks
				}

			} else {
				uint32	base	= (ip.extra & 7) << 5;
				if (base >= 128) {
					// add prefix bytes
				}
				bool	fold	= !!(ip.extra & 8);
				for (uint32 m = ip.mask; m; ) {
					int	lo = lowest_set_index(m), hi = lowest_clear_index(m | (lowest_set(m) - 1));
					if (hi < 0)
						hi = 32;

					merge |= builder.Mark(base + lo, base + hi - 1);

					if (fold) {
						uint32	base2 = base ^ 0x20;
						merge |= builder.Mark(max(lo, ('A' & 31)) + base2, min(hi - 1, ('Z' & ~31)) + base2);
					}

					m &= ~bits(hi);
				}
			}

			// If this Inst is not the last Inst in its list AND the next Inst is also a ByteRange AND the Insts have the same out, defer the merge
			if (merge && (ip.last || (&ip + 1)->op != kInstByteRange || ip.out != (&ip + 1)->out))
				builder.Merge();

		} else if (ip.op == kInstEmptyWidth && ip.empty != kEmptyNever) {
			if ((ip.empty & (kEmptyBeginLine | kEmptyEndLine)) && !marked_line_boundaries) {
				builder.Mark('\n', '\n');
				builder.Merge();
				marked_line_boundaries = true;
			}
			if ((ip.empty & (kEmptyWordBoundary | kEmptyNonWordBoundary)) && !marked_word_boundaries) {
				// We require two batches here: the first for ranges that are word characters, the second for ranges that are not word characters
				for (int is_word = 0; is_word < 2; ++is_word) {
					for (int i = 0; i < 256;) {
						int		i0	= i;
						bool	w	= is_wordchar((char)i0);
						while (++i < 256 && is_wordchar((char)i) == w)
							;
						if (w == !!is_word)
							builder.Mark(i0, i - 1);
					}
					builder.Merge();
				}
				marked_word_boundaries = true;
			}
		}
	}

	return builder.Build(bytemap);
}

//-----------------------------------------------------------------------------
//	OnePass
//-----------------------------------------------------------------------------

struct OnePass {
	enum {
		bits_empty	= 6, bits_wins = 1, bits_cap = 12, bits_index = 13,

		mask_empty	= bits(bits_empty),
		mask_wins	= bit(bits_empty),
		mask_cap	= bits(bits_cap, bits_empty + bits_wins),
		mask_index	= bits(bits_index, bits_empty + bits_wins + bits_cap),
	};
	static uint32 get_bits(uint32 cond, uint32 mask) {
		return (cond & mask) / lowest_set(mask);
	}
	static uint32 set_bits(uint32 val, uint32 mask) {
		return val * lowest_set(mask);
	}

	static bool Satisfy(uint32 cond, EmptyOp curr) {
		return (cond & mask_empty) == 0 || check(EmptyOp(get_bits(cond, mask_empty)), curr);
	}
	static void ApplyCaptures(uint32 cond, const char* p, const char** caps, int ncap) {
		for (uint32 i = get_bits(cond, mask_cap); i && ncap--; i >>= 1, ++caps)
			if (i & 1)
				*caps = p;
	}

	static void GetCaptures(uint32 cond, const char* p, const char** match, const char** caps, int ncap) {
		match[1] = p;
		match += 2;
		for (uint32 i = get_bits(cond, mask_cap); ncap--; i >>= 1, ++match, ++caps)
			*match = i & 1 ? p : *caps;
	}

	struct State {
		uint32	match_cond;
		uint32	action[];
		void reset(int n) {
			match_cond = kEmptyNever;
			for (int b = 0; b < n; b++)
				action[b] = kEmptyNever;
		}
	};

	static bool	AddRange(State *node, uint32 newact, uint32 ignore, const uint8 *bytemap, int lo, int hi) {
		for (int c = lo; c <= hi; c++) {
			int		b	= bytemap[c];

			// Skip any bytes immediately after c that are also in b
			while (c < 256 - 1 && bytemap[c + 1] == b)
				c++;

			uint32 oldact	= node->action[b];
			if ((oldact & kEmptyNever) == kEmptyNever/* || oldact == ignore*/)
				node->action[b] = newact;
			else if (oldact != newact || newact == ignore)
				return false;
		}
		return true;
	}

	static bool Search(const char **matches, uint32 nmatches, const State *states, const uint8 *bytemap, int bytemap_entries, const count_string& text, EmptyOp enable);
	static malloc_block	Check(const dynamic_array<Prog::Inst> &inst, uint32 start, const uint8 *bytemap, int bytemap_entries);
};

// Returns whether this is a one-pass program
// These conditions must be true for any instruction ip:
//	(1) for any other Inst nip, there is at most one input-free path from ip to nip
//	(2) there is at most one kInstByte instruction reachable from ip that matches any particular byte c
//	(3) there is at most one input-free path from ip to a kInstMatch instruction

malloc_block OnePass::Check(const dynamic_array<Prog::Inst> &inst, uint32 start, const uint8 *bytemap, int bytemap_entries) {
	struct InstCond {
		uint32	id;
		uint32	cond;
		InstCond(uint32 id, uint32 cond) : id(id), cond(cond) {}
	};

	if (start == 0)
		return none;

	int		size		= inst.size32();
	int		nalloc		= 0, nassign = 1;

	dynamic_array<int>		nodebyid(size, -1);
	sparse_set<int, uint16>	tovisit(size), workq(size);

	tovisit.insert(start);
	nodebyid[start] = 0;

	uint32					state_size = bytemap_entries + 1;
	dynamic_array<uint32>	states;
	dynamic_array<InstCond> stack;

	for (auto id : make_dynamic(tovisit)) {
		int		nodeindex	= nodebyid[id];
		if (nodeindex >= nalloc)
			states.resize(state_size * (nalloc = nassign));
		
		State*	node		= (State*)(states + state_size * nodeindex);
		node->reset(bytemap_entries);

		workq.clear();
		bool	matched = false;
		uint32	cond	= 0;

		for (;;) {
			auto &ip = inst[id];

			switch (ip.op) {
				default:
					break;

				case kInstAltMatch:
					if (workq.check_insert(++id))
						return none;
					continue;

				case kInstByteRange: {
					int nextindex = nodebyid[ip.out];
					if (nextindex == -1) {
						tovisit.insert(ip.out);
						nodebyid[ip.out] = nextindex = nassign++;
					}

					uint32	newact	= set_bits(nextindex, mask_index) | cond | (matched ? mask_wins : 0);
					uint32	ignore	= set_bits(nodeindex, mask_index) | cond | (matched ? mask_wins : 0);

					if (ip.extra == 15) {
						if (ip.lobank < 8 && !AddRange(node, newact, ignore, bytemap, ip.lobank << 5, min((ip.hibank << 5) | 31, 255)))
							return none;

					} else {
						uint32	base	= (ip.extra & 7) << 5;
						bool	fold	= !!(ip.extra & 8);
						for (uint32 m = ip.mask; m; ) {
							int	lo = lowest_set_index(m), hi = lowest_clear_index(m | (lowest_set(m) - 1));
							if (hi < 0)
								hi = 32;

							if (!AddRange(node, newact, ignore, bytemap, base + lo, base + hi - 1))
								return none;

							if (fold && !AddRange(node, newact, ignore, bytemap, max(('A' & ~31) + lo, 'A'), min(('A' & ~31) + hi - 1, 'Z')))
								return none;

							m &= ~bits(hi);
						}
					}

					if (ip.last)
						break;

					if (workq.check_insert(++id))
						return none;
					continue;
				}

				case kInstCapture:
				case kInstEmptyWidth:
					if (!ip.last) {
						if (workq.check_insert(id + 1))
							break;//return none;
						stack.emplace_back(id + 1, cond);
					}

					ISO_ASSERT(ip.op != kInstCapture || ip.cap < bits_cap + 2);
					if (ip.op == kInstCapture && iso::between(ip.cap, 2, bits_cap + 1))
						cond |= lowest_set((int)mask_cap) << (ip.cap - 2);

					if (ip.op == kInstEmptyWidth)
						cond |= ip.empty;

					// kInstEmptyWidth only sometimes proceeds to ip.out, but as a conservative approximation we assume it always does
					id = ip.out;
					if (workq.check_insert(id))
						return none;
					continue;

				case kInstMatch:
					if (matched)	// (3) is violated
						return none;

					matched				= true;
					node->match_cond	= cond;

					if (ip.last)
						break;

					if (workq.check_insert(++id))
						return none;
					continue;
			}

			if (stack.empty())
				break;

			id		= stack.back().id;
			cond	= stack.back().cond;
			stack.pop_back();
		}
	}

	return const_memory_block(states.begin(), states.size() * sizeof(uint32));
}

bool OnePass::Search(const char **matches, uint32 nmatches, const State *states, const uint8 *bytemap, int bytemap_entries, const count_string& text, EmptyOp enable) {
	int			ncap = clamp((int(nmatches) - 1) * 2, 0, bits_cap);
	const char* cap[bits_cap];

	clear(cap);
	matches[0] = text.begin();

	uint32		state_size	= bytemap_entries + 1;
	const State	*state		= states;
	uint32		next_match	= state->match_cond;
	bool		longest		= enable & kEmptyLongest;
	bool		matched		= false;
	EmptyOp		enable1		= (enable - kEmptyEndText) | kEmptyEndWord | kEmptyEndLine;

	for (const char* p = text.begin(); p != text.end(); ++p) {
		uint32	match_cond	= next_match;
		uint32	cond		= state->action[bytemap[*p & 0xFF]];
		auto	flags		= EmptyFlags(enable1, p);

		if (Satisfy(cond, flags)) {
			state		= states + state_size * get_bits(cond, mask_index);
			next_match	= state->match_cond;
		} else {
			state		= 0;
			next_match	= kEmptyNever;
		}

		if (((cond & mask_wins) || (next_match & mask_empty)) && Satisfy(match_cond, flags)) {
			GetCaptures(match_cond, p, matches, cap, ncap);
			matched		= true;
			if (!longest && (cond & mask_wins))
				return true;
		}

		if (!state)
			return matched;

		ApplyCaptures(cond, p, cap, ncap);
		enable1 = (enable1 - kEmptyBeginText) | kEmptyBeginWord | kEmptyBeginLine;
	}

	if (!matched || longest) {
		// Look for match at end of input
		if (Satisfy(next_match, EmptyFlags(enable, text.end()))) {
			GetCaptures(next_match, text.end(), matches, cap, ncap);
			matched		= true;
		}
	}

	return matched;
}

//-----------------------------------------------------------------------------
//	NFA - non-deterministic finite state machine
//-----------------------------------------------------------------------------

#define CHEAP_THREAD

class NFA {
#ifdef CHEAP_THREAD
	struct Thread {
		int			ref;
		Thread*		next;
		int			index;
		const char* capture;
	};
#else
	struct Thread {
		union {
			int		ref;
			Thread* next;	// when on free list
		};
		const char** capture;
		Thread(int ncap) : capture(new const char*[ncap]) {}
		~Thread() { delete[] capture; }
	};
#endif

	// explicit stack entry for AddToThreadq
	struct StackEntry {
		int		id;
		Thread* t;		// if not null, set t0 = t before processing id
		StackEntry(int id = 0, Thread* t = nullptr)	: id(id), t(t) {}
	};

	// list of threads sorted by the order Perl would explore that particular state - earlier choices appear first
	typedef sparse_map<Thread*, uint32, uint8> Threadq;

	Prog*			prog;
	const char**	matches;		// best matches so far
	uint32			nmatches;
	Threadq			q0, q1;
	StackEntry*		astack;
	Thread*			free_threads;

#ifdef CHEAP_THREAD
	Thread* AllocThread(Thread *prev, int i, const char *p) {
		Thread* t = free_threads;
		if (t)
			free_threads = t->next;
		else
			t = new Thread;

		while (prev && prev->index >= i)
			prev = prev->next;

		if (prev)
			++prev->ref;

		t->ref		= 1;
		t->next		= prev;
		t->index	= i;
		t->capture	= p;
		return t;
	}
	static Thread*	addref(Thread* t) {
		t->ref++;
		return t;
	}
	void	release(Thread* t) {
		while (t && --t->ref <= 0) {
			Thread	*n	= t->next;
			t->next		= free_threads;
			free_threads = t;
			t			= n;
		}
	}
	static const char* GetStart(const Thread* t) {
		while (t) {
			if (t->index == 0)
				return t->capture;
			t = t->next;
		}
		return nullptr;
	}
	void	GetMatches(const char** dst, const Thread *t) {
		while (t) {
			dst[t->index] = t->capture;
			t = t->next;
		}
	}
#else
	Thread* AllocThread() {
		Thread* t = free_threads;
		if (t)
			free_threads = t->next;
		else
			t = new Thread(nmatches);
		t->ref = 1;
		return t;
	}
	static Thread*	addref(Thread* t) {
		t->ref++;
		return t;
	}
	void	release(Thread* t) {
		if (t && --t->ref <= 0) {
			t->next = free_threads;
			free_threads = t;
		}
	}
	static const char* GetStart(const Thread* t) {
		return t->capture[0];
	}
	void	CopyCapture(const char** dst, const char** src) {
		for (int i = 0; i < nmatches; i++)
			dst[i] = src[i];
	}
#endif
	void	AddToThreadq(Threadq* q, int id, int c, EmptyOp empty_flags, const char* p, Thread* t0);
	int32	QuickMatch(uint32 id, const char *p);

public:
	NFA(Prog* prog, const char **matches, uint32 nmatches) : prog(prog), matches(matches), nmatches(nmatches), q0(prog->inst.size()), q1(prog->inst.size()), free_threads(nullptr) {
		astack = new StackEntry[2 * prog->inst_count[kInstCapture] + prog->inst_count[kInstEmptyWidth] + 1];
	}
	~NFA() {
		for (Thread* t = free_threads, *n; t; t = n) {
			n = t->next;
			delete t;
		}
		delete[] astack;
	}
	int32	Search(const count_string& text, EmptyOp enable);
};

int32 NFA::QuickMatch(uint32 id, const char *p) {
	for (;;) {
		Prog::Inst &ip = prog->inst[id];
		switch (ip.op) {
			case kInstCapture:
				if (ip.cap < nmatches * 2)
					matches[ip.cap] = p;
			//case kInstNop:
				id = ip.out;
				continue;

			case kInstEmptyWidth:	// added for nop
				ISO_ASSERT(ip.empty == kEmptyNone);
				id = ip.out;
				continue;

			case kInstMatch:
				matches[1]	= p;
				return ip.match_id;

			default:
				return 0;
		}
	}
}

// Follow all empty arrows from id and enqueues all the states reached that match byte c and empty_flags
void NFA::AddToThreadq(Threadq* q, int id, int c, EmptyOp empty_flags, const char* p, Thread* t0) {
	if (id == 0)
		return;

	StackEntry*	sp	= astack;

	for (;;) {
		if (id && !q->check(id)) {
			// Create entry in q no matter what - even if we don't fill it in below, it is necessary to have it, so that we don't revisit id0 during the recursion
			Thread*&	tp	= q->put(id) = NULL;
			Prog::Inst	&ip	= prog->inst[id];

			switch (ip.op) {
				default:
					ISO_ASSERT(0);
					break;

				case kInstAltMatch:
					// Save state; will pick up at next byte
					tp	= addref(t0);
					++id;
					continue;

				case kInstCapture: {
					if (!ip.last)
						*sp++ = id + 1;

					int j = ip.cap;
					if (j < nmatches * 2) {
						// Push a dummy whose only job is to restore t0 once we finish exploring this possibility
						*sp++ = StackEntry(0, t0);

						// Record capture
#ifdef CHEAP_THREAD
						t0 = AllocThread(t0, j, p);
#else
						Thread* t = AllocThread();
						CopyCapture(t->capture, t0->capture);
						t->capture[j] = p;
						t0 = t;
#endif
					}
					id = ip.out;
					continue;
				}

				case kInstByteRange:
					if (ip.Matches(c))
						tp = addref(t0);
					if (ip.last)
						break;
					++id;
					continue;

				case kInstMatch:
					// Save state; will pick up at next byte
					tp = addref(t0);
					if (ip.last)
						break;
					++id;
					continue;

				case kInstEmptyWidth:
					// Continue on if we have all the right flag bits
					if (check(ip.empty, empty_flags)) {
						if (!ip.last)
							*sp++ = id + 1;
						id = ip.out;
					} else {
						if (ip.last)
							break;
						++id;
					}
					continue;
			}
		}

		if (sp == astack)
			break;

		StackEntry	a = *--sp;

		if (a.t) {
			// t0 was a thread that we allocated and copied in order to record the capture, so we must now release it
			release(t0);
			t0 = a.t;
		}
		id = a.id;
	}
}

int32 NFA::Search(const count_string& text, EmptyOp enable) {
	EmptyOp match_flags = prog->anchor_start || (enable & kEmptyAnchor) ? kEmptyBeginText : kEmptyNone;
	if (!check(match_flags, enable))
		return false;

	bool longest	= enable & kEmptyLongest;
	if (prog->anchor_end) {
		if (!(enable & kEmptyEndText))
			return false;
		longest	= true;
	}

	Threadq*	runq	= &q0;
	Threadq*	nextq	= &q1;
	int32		matched	= 0;
	EmptyOp		enable1	= (enable - kEmptyEndText) | kEmptyEndWord | kEmptyEndLine;

	memset(matches, 0, sizeof(matches[0]) * nmatches * 2);

	// Loop over the text, stepping the machine
	for (const char* p = text.begin();; p++) {
		if (p >= text.end())
			enable1 = enable - kEmptyBeginText;

		EmptyOp	empty_flags = EmptyFlags(enable1, p);
		int		c			= p < text.end() ? p[0] : -1;
		bool	skip		= false;
		bool	check_start	= longest && matched;

		//STEP: run runq on byte c, appending new states to nextq

		for (auto &i : *runq) {
			Thread* t = *i;
			if (!t)
				continue;

			// Can skip any threads started after our current best match
			if (!skip && (!check_start || GetStart(t) <= matches[0])) {

				Prog::Inst	&ip = prog->inst[i.i];
				switch (ip.op) {
					default:
						ISO_ASSERT(0);
						break;

					case kInstByteRange:
						AddToThreadq(nextq, ip.out, c, empty_flags, p, t);
						break;

					case kInstAltMatch:
						if (&i == runq->begin()) {
							// The match is ours if we want it
							bool	greedy = prog->SkipCapNop(&prog->inst[ip.out])->op == kInstByteRange;
							if (greedy || longest) {
							#ifdef CHEAP_THREAD
								GetMatches(matches, t);
							#else
								// We're done - full match ahead
								CopyCapture(matches, t->capture);
							#endif
								skip	= true;
								p		= text.end();
								matched = QuickMatch(greedy ? ip.out1 : ip.out, p);
								break;
							}
						}
						break;

					case kInstMatch: {
						if ((!prog->anchor_end || p - 1 == text.end()) && (!longest || !matched || GetStart(t) < matches[0] || p - 1 > matches[1])) {
							auto	prev_match0	= matches[0];
						#ifdef CHEAP_THREAD
							GetMatches(matches, t);
						#else
							CopyCapture(matches, t->capture);
						#endif
							matches[1]	= p - 1;
							matched		= ip.match_id;
							skip		= !longest;		// Cut off the threads that can only find matches worse than the one we just found
							check_start	= longest && matches[0] != prev_match0;
						}
						break;
					}
				}

			}
			release(t);
		}

		runq->clear();
		swap(nextq, runq);

		if (p > text.end())
			break;

		enable1 = (enable1 - kEmptyBeginText) | kEmptyBeginWord | kEmptyBeginLine;

		// Start a new thread if there have not been any matches (no point in starting a new thread if there have been matches, since it would be to the right of the match we already found)
		if (!matched && check(match_flags, empty_flags)) {
			// If there's a required first byte for an unanchored search and we're not in the middle of any possible matches, use memchr to search for the byte quickly
			if (!(match_flags & kEmptyBeginText) && runq->empty()) {
				if (auto skip = prog->SkipToFirstByte(p, text.end())) {
					p += skip;
					empty_flags = EmptyFlags(enable1, p);
				}
			}

		#ifdef CHEAP_THREAD
			Thread* t = AllocThread(0, 0, p);
		#else
			Thread* t = AllocThread();
			CopyCapture(t->capture, matches);
			t->capture[0] = p;
		#endif
			AddToThreadq(runq, prog->start, p < text.end() ? p[0] : -1, empty_flags, p, t);
			release(t);
		}

		// If all the threads have died, stop early
		if (runq->empty())
			break;
	}

	for (auto &i : *runq)
		release(*i);
	runq->clear();

	return matched;
}

//-----------------------------------------------------------------------------
//	DFA
//-----------------------------------------------------------------------------

class HashMix {
	size_t hash;
public:
	HashMix()			: hash(1) {}
	HashMix(size_t val)	: hash(val + 83) {}

	void Mix(size_t val) {
		static const size_t kMul = static_cast<size_t>(0xdc3eb94af8ab4c93ULL);
		hash *= kMul;
		hash = rotate_left(hash, 19) + val;
	}
	operator size_t() const { return hash; }
};

#define DeadState		reinterpret_cast<DFA::State*>(1)	// signals s->next[c] is a state that can never lead to a match (and thus the search can be called off)
#define FullMatchState	reinterpret_cast<DFA::State*>(2)	// Signals that the rest of the string matches no matter what it is
#define SpecialStateMax FullMatchState

class DFA {
public:
	enum {
		kByteEndText	= 256,							// imaginary byte at end of text
		kFlagNeedShift	= kFlagBits,					// needed kEmpty bits are or'ed in shifted left
		kFlagCapShift	= kFlagNeedShift + kEmptyBits,	// capture is in top bits
	};

	struct State {
		enum {
			Mark		= -1,	// Marks separate thread groups of different priority in the work queue when in leftmost-longest matching mode
			MatchSep	= -2	// Separates the match IDs from the instructions in inst; used only for "many match" DFA states
		};
		uint32		flag;		// Empty string bitfield flags in effect on the way into this state, along with kFlagMatch if this is a matching state
		int			ninst;		// # of inst pointers
		const int	*inst;		// Instruction pointers in the state

		State(uint32 flag, const dynamic_array<int> &inst)				: flag(flag), ninst(inst.size32()), inst(inst.begin()) {}
		State(uint32 flag, const dynamic_array<int> &inst, int nnext)	: flag(flag), ninst(inst.size32()), inst((int*)&next(nnext)) {
			memset(&next(0), 0, nnext * sizeof(State*));
			memcpy((void*)(this->inst), inst.begin(), ninst * sizeof(int));
		}

		State	*&next(int c)			{ return ((State**)(this + 1))[c]; }		// Outgoing arrows from State
		State	*next(int c)	const	{ return ((State**)(this + 1))[c]; }		// Outgoing arrows from State
		auto	insts()			const	{ return make_range_n(inst, ninst); }
		bool	is_match()		const	{ return (flag & kFlagMatch) != 0; }
		EmptyOp	needs()			const	{ return EmptyOp((flag >> kFlagNeedShift) & bits(kEmptyBits)); }
		EmptyOp	before()		const	{ return EmptyOp(flag & bits(kFlagBits)); }

		void	get_matches(sparse_set<uint32> &match_set) const {
			for (auto id : reversed(insts())) {
				if (id == MatchSep)
					break;
				match_set.insert(id);
			}
		}
		void	get_captures(const char **captures, uint32 ncaptures, const char *p) const {
			for (uint32 m = flag >> kFlagCapShift; m; m = clear_lowest(m)) {
				int	i = lowest_set_index(m);
				if (i < ncaptures * 2)
					captures[i] = p;
			}
		}
		friend bool operator==(const State& a, const State& b) {
			if (&a == &b)
				return true;
			if (a.flag != b.flag || a.ninst != b.ninst)
				return false;
			for (int i = 0; i < a.ninst; i++)
				if (a.inst[i] != b.inst[i])
					return false;
			return true;
		}
		friend size_t hash(State *state) {
			HashMix mix(state->flag);
			for (auto i : state->insts())
				mix.Mix(i);
			mix.Mix(0);
			return mix;
		}
	};

//private:
	typedef sparse_set<uint32, uint16>	workq;

	struct Workq : public workq {
#if 0
		int		n;				// size excluding marks
		int		max_mark;		// maximum number of marks
		int		next_mark;		// id of next mark
		bool	last_was_mark;	// last inserted was mark

		Workq(int n, int max_mark) : workq(n + max_mark), n(n), max_mark(max_mark), next_mark(n), last_was_mark(true) {}
		void	clear()					{ workq::clear(); next_mark = n; }
		void	insert(int id)			{ last_was_mark = false; workq::insert(id); }
		void	mark()					{ if (!last_was_mark) insert(next_mark++); }
		bool	is_mark(int i)	const	{ return i >= n; }
		int		size()			const	{ return n + max_mark; }
#else
		Workq(int n, int max_mark) : workq(n) {}
		void	clear()					{ workq::clear(); }
		void	insert(int id)			{ workq::insert(id); }
		void	mark()					{ if (!empty() && back() != State::Mark) push_back(State::Mark); }
		bool	is_mark(int i)	const	{ return i == State::Mark; }
#endif
	};

	struct StartInfo {
		State*	start;
		int		first_byte;
		StartInfo() : start(NULL), first_byte(-2) {}
	};

	class StateSaver {
		uint32				flag;
		dynamic_array<int>	inst;
		State				*special;
	public:
		StateSaver(State* state) {
			if (state <= SpecialStateMax) {
				special = state;
			} else {
				special = nullptr;
				flag	= state->flag;
				inst	= state->insts();
			}
		}
		State* Restore(DFA *dfa) {
			return special ? special : dfa->CachedState(inst, flag);
		}
	};

	Prog				*prog;
	Workq				_q0, _q1, *q0, *q1;
	dynamic_array<int>	astack;

	hash_set_with_key<State*>	state_cache;
	hash_map<uint32, StartInfo>	starts;

	void	ResetCache();
	State*	CachedState(const dynamic_array<int> &inst, uint32 flag);
	State*	WorkqToCachedState(Workq &q, Workq *mq, uint32 flag);
	State*	RunStateOnByte(State *s, int c);
	void	AddToQueue(Workq &q, int id, EmptyOp flag);
	bool	SearchLoop(State *start, int first_byte, const char **captures, uint32 ncaptures, sparse_set<uint32> *match_set, const count_string& text, EmptyOp enable);

	int		ByteMap(int c) const { return c == kByteEndText ? prog->bytemap_entries : prog->bytemap[c]; }

public:
	DFA(Prog* prog) : prog(prog),
		_q0(prog->inst.size32(), prog->inst.size32()),
		_q1(prog->inst.size32(), prog->inst.size32()),
		q0(&_q0), q1(&_q1),
		astack(prog->inst_count[kInstCapture] + prog->inst_count[kInstEmptyWidth] + prog->inst.size32() + 1)	// + 1 for start inst
	{}
	~DFA() {
		for (auto i : state_cache)
			free(i);
	}
	bool	Search(const char **matches, uint32 nmatches, const count_string& text, EmptyOp enable);
};

void DFA::ResetCache() {
	for (auto i : state_cache)
		free(i);
	state_cache.clear();
	starts.clear();
}

DFA::State* DFA::CachedState(const dynamic_array<int> &inst, uint32 flag) {
	State	state(flag, inst);
	auto	it = state_cache.find(&state);
	if (it != state_cache.end())
		return *it;

//	if (state_cache.size() > 2)
//		return nullptr;

	int		nnext	= prog->bytemap_entries + 1;	// + 1 for kByteEndText slot
	size_t	mem		= sizeof(State) + nnext * sizeof(State*) + inst.size() * sizeof(int);
	State	*s		= new(malloc(mem)) State(flag, inst, nnext);
	state_cache.insert(s);
	return s;
}

DFA::State* DFA::WorkqToCachedState(Workq &q, Workq *mq, uint32 flag) {
	bool	longest		= !!(flag & kEmptyLongest);
	EmptyOp	need_flags	= kEmptyNone;	// flags needed by kInstEmptyWidth instructions
	bool	saw_match	= false;		// whether queue contains guaranteed kInstMatch
	bool	saw_mark	= false;		// whether queue contains a Mark

	dynamic_array<int>	inst;

	for (auto id : q) {
		if (saw_match && (!longest || id == State::Mark))
			break;

		if (id == State::Mark) {
			if (!inst.empty() && inst.back() != State::Mark) {
				saw_mark	= true;
				inst.push_back(State::Mark);
			}
		} else {
			Prog::Inst	&ip = prog->inst[id];

			// This state will continue to a match no matter what the rest of the input is - if it is the highest priority match being considered, return FullMatchState to indicate that it's all matches from here out
			if (ip.op == kInstAltMatch
				&& (flag & kFlagMatch)
				&& !prog->many_match
				&& (!longest || !saw_mark || (id == q.front() && prog->SkipCapNop(&ip)->op == kInstByteRange))
			)
				return FullMatchState;

			// Record iff id is the head of its list
			if (ip.IsHead())
				inst.push_back(id);

			if (ip.op == kInstEmptyWidth)
				need_flags |= ip.empty;

			if (ip.op == kInstMatch && !prog->anchor_end)
				saw_match = true;
		}
	}

	if (!inst.empty() && inst.back() == State::Mark)
		inst.pop_back();

	// If there are no empty-width instructions waiting to execute the extra flag bits will not be used, so disard them to reduce the number of distinct states
	if (!need_flags)
		flag &= ~bits(kEmptyBits);

	// If there are no Insts in the list, return DeadState so that the execution loop can stop early
	if (inst.empty() && (flag & ~kEmptyLongest) == 0)
		return DeadState;

	// If we're in longest match mode, the state is a sequence of unordered state sets separated by Marks. Sort each set to reduce the number of distinct sets stored
	if (longest) {
		for (int* ip = inst.begin(), *ep = inst.end(); ip < ep; ++ip) {
			int	*ip0 = ip;
			while (ip < ep && *ip != State::Mark)
				ip++;
			sort(ip0, ip);
		}
	}

	// Append MatchSep and the match IDs in mq if necessary
	if (mq) {
		inst.push_back(State::MatchSep);
		for (auto id : *mq) {
			auto &ip = prog->inst[id];
			if (ip.op == kInstMatch)
				inst.push_back(ip.match_id);
		}
	}

	// Save the needed empty-width flags in the top bits for use later
	flag |= need_flags << kFlagNeedShift;

	return CachedState(inst, flag);
}

void DFA::AddToQueue(Workq &q, int id, EmptyOp flag) {
	int*	stk		= astack;
	int		nstk	= 0;
	for (;;) {
		if (id == State::Mark) {
			q.mark();

		} else if (id != 0 && !q.check_insert(id)) {
			Prog::Inst& ip = prog->inst[id];

			switch (ip.op) {
				default:
					break;

				case kInstCapture:
#if 1
				// DFA treats captures as no-ops
					if (!ip.last)
						stk[nstk++] = id + 1;
					id = ip.out;
					continue;
#endif
				case kInstByteRange:
				case kInstMatch:	// just save these on the queue
					if (ip.last)
						break;
					++id;
					continue;

				case kInstAltMatch:
					++id;
					continue;

				case kInstEmptyWidth:
					if (!ip.last)
						stk[nstk++] = id + 1;

					// stop if we don't have all the right flag bits
					if (!check(ip.empty, flag))
						break;

					// If this instruction is the [00-FF]* loop at the beginning of a leftmost-longest unanchored search, separate with a Mark so that future threads (which will start farther to the right in the input string) are lower priority than current threads
					if (ip.empty == kEmptyNone && /*q.max_mark > 0 && */ id == prog->start_unanchored && id != prog->start)
						stk[nstk++] = State::Mark;

					id = ip.out;
					continue;
			}
		}

		if (nstk == 0)
			break;

		id = stk[--nstk];
	}
}

// Processes input byte c in state, returning new state
DFA::State* DFA::RunStateOnByte(State* state, int c) {

	if (state <= SpecialStateMax)
		return state == FullMatchState ? state : nullptr;

	// If someone else already computed this, return it
	if (State* ns = state->next(ByteMap(c)))
		return ns;

	EmptyOp	old_before	= state->before();
	EmptyOp	before		= old_before;
	EmptyOp	after		= kEmptyNone;

	// Insert implicit $ and ^ around \n
	if (c == '\n') {
		before	|= kEmptyEndLine;
		after	|= kEmptyBeginLine;
	}

	// Insert implicit $ and \z before the fake "end text" byte
	if (c == kByteEndText)
		before	|= kEmptyEndLine | kEmptyEndText;

	bool was_word	= (state->flag & kFlagLastWord) != 0;
	bool is_word	= c != kByteEndText && is_wordchar(c);
	before		|= (is_word == was_word ? kEmptyNonWordBoundary : (kEmptyWordBoundary | (is_word ? kEmptyBeginWord : kEmptyEndWord)));

	// Copy insts in state to the work queue
	q0->clear();
	for (auto i : state->insts()) {
		if (i == State::Mark) {
			q0->mark();
		} else if (i == State::MatchSep) {
			// Nothing after this is an instruction!
			break;
		} else {
			// Explore from the head of the list
			AddToQueue(*q0, i, before & old_before);
		}
	}

#if 0
	// Only useful to rerun on empty string if there are new, useful flags
	if (before & ~old_before & state->needs()) {
		q1->clear();
		for (auto &i : *q0)
			AddToQueue(*q1, i, before);
		swap(q0, q1);
	}
#endif

	bool	is_match	= false;
	bool	longest		= !!(state->flag & kEmptyLongest);
	uint32	cap			= 0;
	q1->clear();

	for (auto id : *q0) {
		if (is_match && (!longest || id == State::Mark))
			break;

		if (id == State::Mark) {
			q1->mark();

		} else {
			auto	&ip = prog->inst[id];
			switch (ip.op) {
				default:
				case kInstAltMatch:		// already followed
				case kInstEmptyWidth:	// already followed
					break;
#if 0
				case kInstCapture:
					cap	|= 1 << ip.cap;
					q1->insert(id);
					break;
#endif
				case kInstByteRange:	// can follow if c is in range
					if (ip.Matches(c))
						AddToQueue(*q1, ip.out, after);
					break;

				case kInstMatch:
					if (!prog->anchor_end || c == kByteEndText || prog->many_match)
						is_match = true;
					break;
			}
		}
	}

	swap(q0, q1);

	// Save after along with is_match and is_word in new state
	uint32	flags = after
		| (state->flag & kEmptyLongest)
		| (cap << kFlagCapShift)
		| (is_match ? kFlagMatch : 0)
		| (is_word ? kFlagLastWord : 0);

	return state->next(ByteMap(c)) = WorkqToCachedState(*q0, is_match && prog->many_match ? q0 : nullptr, flags);
}

static const void* memrchr(const void* s, int c, size_t n) {
	const uint8* p = (const uint8*)s + n;
	while (p != s) {
		if (*--p == c)
			return p;
	}
	return NULL;
}

bool DFA::SearchLoop(State *start, int first_byte, const char **captures, uint32 ncaptures, sparse_set<uint32> *match_set, const count_string& text, EmptyOp enable) {
	const uint8*	p			= (uint8*)text.begin();
	const uint8*	ep			= (uint8*)text.end();
	const uint8*	bytemap		= prog->bytemap;
	const uint8*	lastmatch	= NULL;

	bool			longest		= !!(enable & kEmptyLongest);
	bool			forward		= !prog->reversed;
	bool			matched		= false;

	if (!forward)
		iso::swap(p, ep);

	captures[0] = (const char*)p;

	State* s = start;

	if (s <= SpecialStateMax) {
		if (s == DeadState)
			return false;
		// FullMatchState
		captures[1] = (const char*)(longest ? ep : p);
		return true;
	}

	s->get_captures(captures, ncaptures, (const char*)p);

	if (s->is_match()) {
		matched		= true;
		lastmatch	= p;

		if (match_set)
			s->get_matches(*match_set);

		if (!longest) {
			captures[1] = (const char*)lastmatch;
			return true;
		}
	}

	while (p != ep) {
		if (first_byte != -1 && s == start) {
			// In start state, only way out is to find first_byte. If not found, we can skip to the end of the string
			if (forward) {
				if (!(p = (const uint8*)memchr(p, first_byte, ep - p))) {
					p = ep;
					break;
				}
			} else {
				if (!(p = (const uint8*)memrchr(ep, first_byte, p - ep))) {
					p = ep;
					break;
				}
				p++;
			}
		}

		int		c	= forward ? *p++ : *--p;
		State	*ns = s->next(bytemap[c]);

		if (!ns) {
			// the state has not yet been computed
			ns = RunStateOnByte(s, c);
			if (!ns) {
				StateSaver save_start(start), save_s(s);
				ResetCache();
				if (!(start = save_start.Restore(this)) || !(s = save_s.Restore(this)))
					return false;

				ns = RunStateOnByte(s, c);
				if (!ns)
					return false; //out of memory
			}
		}
		if (ns <= SpecialStateMax) {
			if (ns == DeadState) {
				captures[1] = (const char*)lastmatch;
				return matched;
			}
			// FullMatchState
			captures[1] = (const char*)ep;
			return true;
		}

		s = ns;
		s->get_captures(captures, ncaptures, (const char*)p);

		if (s->is_match()) {
			matched		= true;
			lastmatch	= forward ? p - 1 : p + 1;	// The DFA notices the match one byte late, so adjust p before using it in the match

			if (match_set)
				s->get_matches(*match_set);

			if (!longest) {
				captures[1] = (const char*)lastmatch;
				return true;
			}
		}
	}

	// Process one more byte to see if it triggers a match (matches are delayed one byte)
	int c = forward
		? (enable & kEmptyEndText	? kByteEndText : ep[ 0] & 0xFF)
		: (enable & kEmptyBeginText	? kByteEndText : ep[-1] & 0xFF);

	State* ns = s->next(ByteMap(c));
	if (!ns) {
		ns = RunStateOnByte(s, c);
		if (!ns) {
			StateSaver save_s(s);
			ResetCache();
			if (!(s = save_s.Restore(this)))
				return false;
			ns = RunStateOnByte(s, c);
			if (!ns)
				return false;
		}
	}
	if (ns <= SpecialStateMax) {
		if (ns == DeadState) {
			captures[1] = (const char*)lastmatch;
			return matched;
		}
		// FullMatchState
		captures[1] = (const char*)ep;
		return true;
	}

	if (ns->is_match()) {
		matched		= true;
		lastmatch	= p;
		if (match_set)
			ns->get_matches(*match_set);
	}

	captures[1] = (const char*)lastmatch;
	return matched;
}

bool DFA::Search(const char **matches, uint32 nmatches, const count_string& text, EmptyOp enable) {
	EmptyOp		current	= EmptyFlags(enable - kEmptyEndText, text.begin());
	if (prog->reversed)
		current = reverse(current);

	StartInfo	&info	= starts[current | (enable & (kEmptyAnchor | kEmptyLongest))];

	if (info.first_byte == -2) {
		q0->clear();
		AddToQueue(*q0, enable & kEmptyAnchor ? prog->start : prog->start_unanchored, current);

		if (!(info.start = WorkqToCachedState(*q0, NULL, current | (enable & kEmptyLongest))))
			return false;

		// Even if we have a first_byte, we cannot use it when anchored and, less obviously, we cannot use it when we are going to need flags
		info.first_byte = info.start <= SpecialStateMax || (enable & kEmptyAnchor) || (info.start->flag >> kFlagNeedShift) ? -1 : prog->FirstByte();
	}

	if (prog->many_match) {
		sparse_set<uint32> match_set(100);
		return SearchLoop(info.start, info.first_byte, matches, nmatches, &match_set, text, enable);
	}
	return SearchLoop(info.start, info.first_byte, matches, nmatches, nullptr, text, enable);
}

//-----------------------------------------------------------------------------
//	debug
//-----------------------------------------------------------------------------

string_accum&	DumpRegexp(string_accum &sa, Regexp *re, int depth = 0);
string_accum&	DumpProg(string_accum &sa, Prog *prog);
string_accum&	DumpByteMap(string_accum &sa, uint8 bytemap[256]);
string_accum&	DumpOnePass(string_accum &sa, const_memory_block states, int bytemap_entries);
string_accum&	DumpDFAState(string_accum &sa, DFA::State* state, int nnext);
string_accum&	DumpDFA(string_accum &sa, DFA *dfa);

//-----------------------------------------------------------------------------
//	Prog::Search
//-----------------------------------------------------------------------------

int32 Prog::Search(const char **matches, uint32 nmatches, const count_string& text, EmptyOp enable) {
	if ((anchor_start || (enable & kEmptyAnchor)) && !(enable & kEmptyBeginText))
		return false;

	if ((anchor_end || (enable & kEmptyAnchor)) && !(enable & kEmptyEndText))
		return false;

	if (onepass_states) {
		for (auto p = text.begin(), e = text.end(); p < e;) {
			if (OnePass::Search(matches, nmatches, onepass_states, bytemap, bytemap_entries, count_string(p, e), enable)) {
				return !(flags & anchor_end) || matches[1] == text.end() ? 1 : 0;
			}
			if (flags & anchor_start)
				return 0;

			if (auto skip = SkipToFirstByte(p, e))
				p += skip;
			else
				++p;
		}
		return 0;
	}
#if 0
	if (!dfa)
		dfa	= new DFA(this);
//	return dfa->Search(matches, nmatches, text, enable);
	bool	ret = dfa->Search(matches, nmatches, text, enable);
#ifdef REGEX_DIAGNOSTICS
	DumpByteMap(trace_accum(), bytemap);
	DumpDFA(trace_accum(), dfa);
#endif
	return ret;
#endif
	return NFA(this, matches, nmatches).Search(text, enable);
}

Prog::~Prog() { delete dfa; }

//-----------------------------------------------------------------------------
//	ParseFlags
//-----------------------------------------------------------------------------

enum ParseFlags {
	kParseNone 			= 0,
	kParseFoldCase 		= 1 << 0,		// Fold case during matching (case-insensitive)
	kParseLiteral 		= 1 << 1,		// Treat s as literal string instead of a regexp
	kParseClassNL 		= 1 << 2,		// Allow char classes like [^a-z] and \D and \s and [[:space:]] to match newline
	kParseDotNL 		= 1 << 3,		// Allow . to match newline
	kParseMatchNL 		= kParseClassNL | kParseDotNL,
	kParseOneLine		= 1 << 4,		// Treat ^ and $ as only matching at beginning and end of text, not around embedded newlines
	kParseNonGreedy 	= 1 << 6,		// Repetition operators are non-greedy by default
	kParseNeverNL 		= 1 << 11,		// Never match NL, even if the regexp mentions it explicitly
	kParseNeverCapture 	= 1 << 12,		// Parse all parens as non-capturing

	// As close to Perl as we can get
	kParseLikePerl 		= kParseClassNL | kParseOneLine,

	// Encoding
	_kParseEncoding		= 1 << 16,
	kParseEncoding		= _kParseEncoding * 3,
	kParseEncodingD		= _kParseEncoding * 0,	//old perl character rules
	kParseEncodingA		= _kParseEncoding * 1,	//ascii rules
	kParseEncodingU		= _kParseEncoding * 2,	//use unicode rules
	kParseEncodingL		= _kParseEncoding * 3,	//use current locale
	kParseError			= -1,
};

inline ParseFlags operator~(ParseFlags a)				{ return ParseFlags(~int(a)); }
inline ParseFlags operator|(ParseFlags a, ParseFlags b) { return ParseFlags(int(a) | int(b)); }
inline ParseFlags operator&(ParseFlags a, ParseFlags b) { return ParseFlags(int(a) & int(b)); }
inline ParseFlags operator^(ParseFlags a, ParseFlags b) { return ParseFlags(int(a) ^ int(b)); }

//-----------------------------------------------------------------------------
//	CharClass
//-----------------------------------------------------------------------------

struct CharRange {
	char32 lo, hi;
	CharRange() : lo(0), hi(0) {}
	CharRange(uint32 lo, uint32 hi) : lo(lo), hi(hi) {}

	friend bool operator!=(const CharRange& a, const CharRange& b) {
		return a.hi != b.hi || a.lo != b.lo;
	}
	//friend int simple_compare(const CharRange& a, const CharRange& b) {
	//	return a.hi != b.hi ? simple_compare(a.hi, b.hi) : simple_compare(a.lo, b.lo);
	//}
	friend bool operator<(const CharRange& a, const CharRange& b) {
		return a.hi < b.lo;
	}
	friend bool operator<(const CharRange& a, uint32 b) {
		return a.hi < b;
	}
};

class CharClass : public dynamic_array<CharRange> {
	int		nchars;
public:
	CharClass(int nchars = 0) : nchars(nchars) {}
	CharClass(const char_set &set) : nchars(0) {
		for (int i = set.next(0, true), j; i < 256; i = set.next(j, true)) {
			j = set.next(i, false);
			emplace_back(i, j - 1);
			nchars += j - i;
		}
	}

	uint32	num_chars()		const	{ return nchars; }
	bool	empty()			const	{ return nchars == 0; }
	bool	full()			const	{ return nchars == unicode::Runemax + 1; }
	bool	FoldsASCII()	const	{
		static const uint32	A = 'A' / 32 * 32;
		static const uint32	Z = 'Z' / 32 * 32;
		static const uint32	a = 'a' / 32 * 32;
		static const uint32	z = 'z' / 32 * 32;

		uint32	lower = 0, upper = 0;
		for (auto i = lower_boundc(*this, A); i != end() && i->lo <= z; ++i) {
			uint32 lo1 = max(i->lo, A);
			uint32 hi1 = min(i->hi, Z);
			if (lo1 <= hi1)
				upper |= bits(hi1 - lo1 + 1, lo1 - A);

			lo1 = max(i->lo, a);
			hi1 = min(i->hi, z);
			if (lo1 <= hi1)
				lower |= bits(hi1 - lo1 + 1, lo1 - a);
		}

		if (~upper)
			return false;	// all set, will be treated as page range

		static const uint32	alpha = bits('Z' - 'A' + 1, 'A' & 31);
		return (upper & alpha) && !((upper ^ lower) & alpha);
	}

	bool Contains(uint32 r) {
		CharRange*	rr = begin();
		int			n = size32();
		while (n > 0) {
			int m = n / 2;
			if (rr[m].hi < r) {
				rr += m + 1;
				n -= m + 1;
			} else if (r < rr[m].lo) {
				n = m;
			} else {	// rr[m].lo <= r && r <= rr[m].hi
				return true;
			}
		}
		return false;
	}
};

class CharClassBuilder : public set<CharRange> {
	int		nchars;
	auto	find(const CharRange& r) {
		using iso::find;
		return find(*(set<CharRange>*)this, r);
	}
	auto	find(const CharRange& r) const {
		using iso::find;
		return find(*(const set<CharRange>*)this, r);
	}

public:
	CharClassBuilder() : nchars(0) {}

	uint32	num_chars()			const	{ return nchars; }
	bool	empty()				const	{ return nchars == 0; }
	bool	full()				const	{ return nchars == unicode::Runemax + 1; }
	bool	Contains(uint32 r)	const	{ return find(CharRange(r, r)) != end(); }

	bool	AddRange(uint32 lo, uint32 hi) {
		if (hi < lo)
			return false;

		{	// Check whether lo, hi is already in the class
			auto it = find(CharRange(lo, lo));
			if (it != end() && it->lo <= lo && hi <= it->hi)
				return false;
		}

		// Look for a range abutting lo on the left - if it exists, take it out and increase our range
		if (lo > 0) {
			auto it = find(CharRange(lo - 1, lo - 1));
			if (it != end()) {
				lo = it->lo;
				if (it->hi > hi)
					hi = it->hi;
				nchars -= it->hi - it->lo + 1;
				remove(move(it));
			}
		}

		// Look for a range abutting hi on the right - if it exists, take it out and increase our range
		if (hi < unicode::Runemax) {
			auto it = find(CharRange(hi + 1, hi + 1));
			if (it != end()) {
				hi = it->hi;
				nchars -= it->hi - it->lo + 1;
				remove(move(it));
			}
		}

		// Look for ranges between lo and hi, and take them out
		for (;;) {
			auto it = find(CharRange(lo, hi));
			if (it == end())
				break;
			nchars -= it->hi - it->lo + 1;
			remove(move(it));
		}

		// Finally, add [lo, hi]
		nchars += hi - lo + 1;
		insert(CharRange(lo, hi));
		return true;
	}

	CharClassBuilder* Copy() {
		CharClassBuilder* cc = new CharClassBuilder;
		for (iterator it = begin(); it != end(); ++it)
			cc->insert(CharRange(it->lo, it->hi));
		cc->nchars = nchars;
		return cc;
	}
	void AddCharClass(CharClassBuilder* cc) {
		for (iterator it = cc->begin(); it != cc->end(); ++it)
			AddRange(it->lo, it->hi);
	}
	void Negate() {
		// Build up negation and then copy in
		dynamic_array<CharRange> v;
		v.reserve(size() + 1);

		iterator it = begin();

		// In negation, first range begins at 0, unless the current class begins at 0
		int nextlo = 0;
		if (it != end() && it->lo == 0) {
			nextlo = it->hi + 1;
			++it;
		}

		for (; it != end(); ++it) {
			v.push_back(CharRange(nextlo, it->lo - 1));
			nextlo = it->hi + 1;
		}
		if (nextlo <= unicode::Runemax)
			v.push_back(CharRange(nextlo, unicode::Runemax));

		clear();
		for (size_t i = 0; i < v.size(); i++)
			insert(v[i]);

		nchars	= unicode::Runemax + 1 - nchars;
	}
	void RemoveAbove(uint32 r) {
		if (r >= unicode::Runemax)
			return;

		for (;;) {
			auto it = find(CharRange(r + 1, unicode::Runemax));
			if (it == end())
				break;
			CharRange rr = *it;
			remove(move(it));
			nchars -= rr.hi - rr.lo + 1;
			if (rr.lo <= r) {
				rr.hi = r;
				insert(rr);
				nchars += rr.hi - rr.lo + 1;
			}
		}
	}
	CharClass *GetCharClass() {
		CharClass	*cc = new CharClass(nchars);
		for (auto &i : *this)
			cc->push_back(i);
		return cc;
	}
};

void AddFoldedRange(CharClassBuilder *cc, char32 lo, char32 hi, int depth) {
	// AddFoldedRange calls itself recursively for each rune in the fold cycle
	ISO_ASSERT(depth < 10);

	if (!cc->AddRange(lo, hi))	// if lo-hi was already there, we're done
		return;

	while (lo <= hi) {
		auto f = unicode::LookupCaseFold(lo);
		if (f == NULL)
			break;		// lo has no fold, nor does anything above lo

		if (lo < f->lo) {
			lo = f->lo;	// lo has no fold; next rune with a fold is f->lo
			continue;
		}

		// Add in the result of folding the range lo - f->hi and that range's fold, recursively
		char32 lo1 = lo;
		char32 hi1 = min(hi, f->hi);
		switch (f->delta) {
			default:
				lo1 += f->delta;
				hi1 += f->delta;
				break;
			case unicode::EvenOdd:
				if (lo & 1) {
					lo1 = (lo1 - 1) | 1;
					hi1 = (hi1 + 1) & ~1;
				} else {
					lo1 &= ~1;
					hi1 |= 1;
				}
				break;
		}
		AddFoldedRange(cc, lo1, hi1, depth + 1);

		// Pick up where this fold left off
		lo = f->hi + 1;
	}
}

void AddRange(CharClassBuilder *cc, char32 lo, char32 hi, ParseFlags parse_flags) {
	// Take out \n if the flags say so
	if ((!(parse_flags & kParseClassNL) || (parse_flags & kParseNeverNL)) && iso::between('\n', lo, hi)) {
		if (lo < '\n')
			AddRange(cc, lo, '\n' - 1, parse_flags);
		if (hi > '\n')
			AddRange(cc, '\n' + 1, hi, parse_flags);
		return;
	}

	// If folding case, add fold-equivalent characters too
	if (parse_flags & kParseFoldCase)
		AddFoldedRange(cc, lo, hi, 0);
	else
		cc->AddRange(lo, hi);
}

// Add a PoxixGroup or its negation to the character class
void AddGroup(CharClassBuilder *cc, const PosixGroup *g, bool negate, ParseFlags parse_flags) {
	if (!negate) {
		for (auto &p : g->r16)
			AddRange(cc, p.a, p.b, parse_flags);
	} else {
		CharClassBuilder ccb1;
		AddGroup(&ccb1, g, false, parse_flags);
		// If the flags say to take out \n, put it in, so that negating will take it out
		if (!(parse_flags & kParseClassNL) || (parse_flags & kParseNeverNL))
			ccb1.AddRange('\n', '\n');
		ccb1.Negate();
		cc->AddCharClass(&ccb1);
	}
}

// Add a unicode::Group or its negation to the character class
void AddGroup(CharClassBuilder *cc, const unicode::Group *g, bool negate, ParseFlags parse_flags) {
	if (!negate) {
	#if 1
		for (auto &p : g->r16)
			AddRange(cc, p.a, p.b, parse_flags);
		for (auto &p : g->r32)
			AddRange(cc, p.a, p.b, parse_flags);
/*
		if (auto *p = g->r16.begin()) {
			while (p->b) {
				AddRange(cc, p->a, p->b, parse_flags);
				++p;
			}
		}
		if (auto *p = g->r32.begin()) {
			while (p->b) {
				AddRange(cc, p->a, p->b, parse_flags);
				++p;
			}
		}*/
	#endif
	} else {
		CharClassBuilder ccb1;
		AddGroup(&ccb1, g, false, parse_flags);
		// If the flags say to take out \n, put it in, so that negating will take it out
		if (!(parse_flags & kParseClassNL) || (parse_flags & kParseNeverNL))
			ccb1.AddRange('\n', '\n');
		ccb1.Negate();
		cc->AddCharClass(&ccb1);
	}
}

//-----------------------------------------------------------------------------
//	Regexp
//-----------------------------------------------------------------------------

enum RegexpOp {
	kRegexpNoMatch = 1,		// Matches no strings
	kRegexpEmptyMatch,		// Matches empty string
	kRegexpLiteral,			// Matches single char
	kRegexpLiteralString,	// Matches chars
	kRegexpConcat,			// Matches concatenation of sub[0..nsub-1]
	kRegexpAlternate, 		// Matches union of sub[0..nsub-1]
	kRegexpRepeat,			// Matches sub[0] at least min times, at most max times; max == -1 means no upper limit
	kRegexpCapture,			// Parenthesized (capturing) subexpression. Index is cap; Optionally, capturing name is name
	kRegexpAnyChar,			// Matches any character
	kRegexpCharClass,		// Matches character class given by cc
	kRegexpHaveMatch,		// Forces match of entire expression right now, with match ID match_id (used by Set)
	kMaxRegexpOp = kRegexpHaveMatch,

// Pseudo-operators - only on parse stack
	kLeftParen,
	kVerticalBar,
};

class Regexp : public refs<Regexp> {
public:
	uint16	op:7, simple:1, cap:8;
	compact<ParseFlags,16>	parse_flags;

	union {
		struct {	// Capture, Repeat
			Regexp*		sub;
			union {
				struct {		// Repeat
					int	min, max;
				};
				char	*name;	// Capture
			};
		};
		struct {	// Concat/Alternate
			uint32		nsub;
			Regexp**	subs;
		};
		struct {	// LiteralString
			uint32		nchars;
			char32*		chars;
		};
		struct {	// CharClass
			CharClass*			cc;
			CharClassBuilder*	ccb;
		};
		char32	rune;			// Literal
		int32	match_id;		// HaveMatch
		EmptyOp	empty_flags;	// EmptyWidth
	};

	// Extra space for parse and teardown stacks
	Regexp* down;

	Regexp(const Regexp&) = delete;
	Regexp& operator=(const Regexp&) = delete;

	Regexp(RegexpOp _op, ParseFlags _parse_flags) {
		clear(*this);
		op = _op;
		parse_flags = _parse_flags;
		addref();
	}
	~Regexp() {
		switch (op) {
			case kRegexpLiteralString:
				deallocate(chars, nchars);
			default:
				return;

			case kRegexpCharClass:
				delete cc;
				delete ccb;
				return;

			case kRegexpCapture:
			case kRegexpRepeat:
			case kRegexpConcat:
			case kRegexpAlternate:
				break;
		}

		down			= nullptr;
		Regexp* stack	= this;
		while (Regexp* re = stack) {
			stack = re->down;
			switch (re->op) {
				case kRegexpCapture:
					free(re->name);
				case kRegexpRepeat:
					if (Regexp* sub = re->sub) {
						if (--sub->nrefs == 0) {
							sub->down = stack;
							stack	= sub;
						}
					}
					re->op	= 0;
					break;

				// subs
				case kRegexpConcat:
				case kRegexpAlternate:
					for (int i = 0; i < re->nsub; i++) {
						if (Regexp* sub = re->subs[i]) {
							if (--sub->nrefs == 0) {
								sub->down = stack;
								stack = sub;
							}
						}
					}
					delete[] re->subs;
					re->op = 0;
					break;
			}
			if (re != this)
				delete re;
		}
	}

	// Computes whether Regexp is already simple
	bool ComputeSimple();

	// Returns the leading regexp in re's top-level concatenation
	Regexp* TopLeading() {
		return op == kRegexpConcat ? subs[0] : this;
	}

	// Returns the regexp that re starts with
	Regexp* Leading() {
		Regexp* re	= this;
		while (re->op == kRegexpConcat && re->nsub > 0)
			re = re->subs[0];
		return re;
	}

	// Removes the first n leading chars from the beginning of re; edits re in place
	void	RemoveLeadingString(int n);

	// Removes LeadingRegexp(re) from re and returns the remainder; might edit re in place
	Regexp* RemoveLeadingRegexp();

	// Simplifies an alternation of literal strings by factoring out common prefixes
	static int FactorAlternation(const range<Regexp**> &sub, ParseFlags flags);

	static bool Equal(Regexp* a, Regexp* b);
	static bool TopEqual(Regexp* a, Regexp* b);

	friend void swap(Regexp &a, Regexp &b) {
		char	t[sizeof(Regexp)];
		memmove(t, &a, sizeof(Regexp));
		memmove(&a, &b, sizeof(Regexp));
		memmove(&b, t, sizeof(Regexp));
	}

	// traversal class
	template<typename T> class Walker;

	range<Regexp**>	Subs() {
		switch (op) {
			default:				return none;
			case kRegexpAlternate:
			case kRegexpConcat:		return make_range_n(subs, nsub);
			case kRegexpRepeat:
			case kRegexpCapture:	return make_range_n(&sub, 1);
		}
	}

	void	SetSub(const range<Regexp**> &new_subs) {
		switch (op) {
			default:
				break;
			case kRegexpAlternate:
			case kRegexpConcat:
				nsub	= new_subs.size32();
				subs	= new Regexp*[nsub];
				for (int i = 0; i < nsub; i++)
					subs[i] = new_subs[i];
				break;
			case kRegexpRepeat:
			case kRegexpCapture:
				sub = new_subs[0];
				break;
		}
	}

	range<char32*> Characters() {
		return	op == kRegexpLiteral		? make_range_n(&rune, 1)
			:	op == kRegexpLiteralString	? make_range_n(chars, nchars)
			:	empty;
	}

	Regexp* NopOut() {
		op			= kRegexpEmptyMatch;
		empty_flags	= kEmptyNone;
		return this;
	}

	static range<Regexp**> DupRange(const range<Regexp**> &sub) {
		int			n		= sub.size32();
		Regexp**	copy	= new Regexp*[n];
		for (auto i : sub)
			*copy++ = i->addref();
		return range<Regexp**>(copy - n, copy);
	}
	static Regexp* Nop(ParseFlags flags) {
		return new Regexp(kRegexpEmptyMatch, flags);
	}
	static Regexp* ConcatQuick(const range<Regexp**> &sub, ParseFlags flags) {
		Regexp* re	= new Regexp(kRegexpConcat, flags);
		re->nsub	= sub.size32();
		re->subs	= sub.begin();
		return re;
	}
	static Regexp* Concat2(Regexp* re1, Regexp* re2, ParseFlags parse_flags) {
		Regexp** subs = new Regexp*[2];
		subs[0] = re1;
		subs[1] = re2;
		return ConcatQuick(make_range_n(subs, 2), parse_flags);
	}
	static Regexp* Alternate(const range<Regexp**> &sub, ParseFlags flags, bool factor = true) {
		int nsub	= sub.size32();
		if (nsub == 0)
			return new Regexp(kRegexpNoMatch, flags);
		if (nsub == 1)
			return sub[0];

		Regexp**	subcopy	= new Regexp*[nsub];
		memcpy(subcopy, sub.begin(), nsub * sizeof(Regexp*));

		if (factor) {
			nsub	= FactorAlternation(make_range_n(subcopy, nsub), flags);
			if (nsub == 1) {
				Regexp* re = subcopy[0];
				delete[] subcopy;
				return re;
			}
		}

		Regexp* re	= new Regexp(kRegexpAlternate, flags);
		re->subs	= subcopy;
		re->nsub	= nsub;
		return re;
	}
	static Regexp* Capture(Regexp* sub, ParseFlags flags, int cap) {
		Regexp* re = new Regexp(kRegexpCapture, flags);
		re->sub = sub;
		re->cap = cap;
		return re;
	}
	static Regexp* Repeat(Regexp* sub, ParseFlags flags, int min, int max);
	static Regexp* Plus(Regexp* sub, ParseFlags flags)	{ return Repeat(sub, flags, 1, -1); }
	static Regexp* Star(Regexp* sub, ParseFlags flags)	{ return Repeat(sub, flags, 0, -1); }
	static Regexp* Quest(Regexp* sub, ParseFlags flags)	{ return Repeat(sub, flags, 0, 1); }

	template<typename C> static Regexp* LiteralString(const C &chars, ParseFlags flags) {
		uint32	nchars = chars.size32();
		ISO_ASSERT(nchars != 0);
		if (nchars == 1) {
			Regexp* re = new Regexp(kRegexpLiteral, flags);
			re->rune = chars[0];
			return re;
		}

		Regexp* re	= new Regexp(kRegexpLiteralString, flags);
		re->nchars	= nchars;
		re->chars	= allocate<char32>(nchars);
		for (int i = 0; i < nchars; i++)
			re->chars[i] = chars[i];
		return re;
	}

	// if every match of this regexp must be anchored and begin with a non-empty fixed string, return the prefix and the sub-regexp that follows it
	Regexp *RequiredPrefix(Regexp** suffix);

	template<typename C> static Regexp* Parse(string_scanT<C> &&t, ParseFlags flags = kParseNone);
	static Regexp* Parse(const count_string &s, ParseFlags flags = kParseNone) {
		return Parse(string_scan(s), flags);
	}

	// rewrite counted repetition in simpler terms and remove all Perl/POSIX features
	Regexp* Simplify();

	static bool IsAnchor(Regexp** pre, EmptyOp end, int depth);
};

Regexp* Regexp::Repeat(Regexp* sub, ParseFlags flags, int min, int max) {
	if (sub->op == kRegexpEmptyMatch)
		return sub;

	// Squash ** to *, ++ to +, ?? to ?
	// Squash *+, *?, +*, +?, ?* and ?+ to *

	if (sub->op == kRegexpRepeat && (min <= 1 || sub->min <= 1) && (max <= 1 || sub->max <= 1)) {
		min = min * sub->min;
		max = max < 0 || sub->max < 0 ? -1 : 1;
		Regexp* sub2 = sub->sub->addref();
		sub->release();		// we didn't consume the reference after all
		sub	= sub2;
	}

	if (min == 0 && max == 0)
		return Nop(flags);

	if (min == 1 && max == 1)
		return sub;

	Regexp* re = new Regexp(kRegexpRepeat, flags);
	re->sub = sub;
	re->min = min;
	re->max = max;
	return re;
}

// Removes the first n leading chars from the beginning of re
void Regexp::RemoveLeadingString(int n) {
	Regexp* re		= this;
	Regexp* concat	= 0;
	while (re->op == kRegexpConcat && re->nsub > 0) {
		concat = re;
		re = re->subs[0];
	}

	// Remove leading string from re
	if (re->op == kRegexpLiteral) {
		re->NopOut();

	} else if (re->op == kRegexpLiteralString) {
		switch (re->nchars - n) {
			case 0:
				deallocate(re->chars, re->nchars);
				re->NopOut();
				break;
			case 1: {
				char32 rune = re->chars[re->nchars - 1];
				deallocate(re->chars, re->nchars);
				re->rune	= rune;
				re->op		= kRegexpLiteral;
				return;
			}
			default:
				re->nchars -= n;
				memmove(re->chars, re->chars + n, re->nchars * sizeof(char32));
				return;
		}
	} else {
		ISO_ASSERT(0);
	}

	// If re is now empty, concatenations might simplify too
	if (concat) {
		re->release();
		Regexp	**sub	= concat->subs;
		uint32	nsub	= concat->nsub;
		if (nsub == 2) {
			Regexp	*sub1 = sub[1];
			swap(*concat, *sub1);
			sub1->release();
			deallocate(sub, nsub);
		} else {
			--concat->nsub;
			memmove(sub, sub + 1, nsub * sizeof sub[0]);
		}
	}
}

// Removes LeadingRegexp(re) from re and returns the remainder; might edit re in place
Regexp* Regexp::RemoveLeadingRegexp() {
	if (op == kRegexpConcat && nsub > 1) {
		if (subs[0]->op != kRegexpEmptyMatch || subs[0]->empty_flags) {
			subs[0]->release();
			nsub--;
			memmove(subs, subs + 1, nsub * sizeof subs[0]);
		}
		return this;
	}
	ParseFlags pf = parse_flags;
	release();
	return Nop(pf);
}

// Assuming the simple flags on the children are accurate, is this Regexp* simple?
bool Regexp::ComputeSimple() {
	switch (op) {
		case kRegexpNoMatch:
		case kRegexpEmptyMatch:
		case kRegexpLiteral:
		case kRegexpLiteralString:
		case kRegexpAnyChar:
		case kRegexpHaveMatch:
			return true;

		case kRegexpConcat:
		case kRegexpAlternate:
			// simple if the subpieces are simple
			for (int i = 0; i < nsub; i++)
				if (!subs[i]->simple)
					return false;
			return true;

		case kRegexpCharClass:
			// simple if char class is not empty or full
			return ccb ? (!ccb->empty() && !ccb->full()) : (!cc->empty() && !cc->full());

		case kRegexpCapture:
			return sub->simple;

		case kRegexpRepeat:
			if (min > 1 || max > 1 || !sub->simple)
				return false;
			switch (sub->op) {
				case kRegexpRepeat:
				case kRegexpEmptyMatch:
				case kRegexpNoMatch:
					return false;
				default:
					return true;
			}

		default:
			ISO_ASSERT(0);
			return false;
	}
}

bool Regexp::TopEqual(Regexp* a, Regexp* b) {
	if (a->op != b->op)
		return false;

	switch (a->op) {
		case kRegexpNoMatch:
		case kRegexpAnyChar:
			return true;

		case kRegexpEmptyMatch:
			return a->empty_flags == b->empty_flags;

		case kRegexpLiteral:
			return a->rune == b->rune && ((a->parse_flags ^ b->parse_flags) & kParseFoldCase) == 0;

		case kRegexpLiteralString:
			return a->nchars == b->nchars && ((a->parse_flags ^ b->parse_flags) & kParseFoldCase) == 0 && memcmp(a->chars, b->chars, a->nchars * sizeof(uint32)) == 0;

		case kRegexpAlternate:
		case kRegexpConcat:
			return a->nsub == b->nsub;

		case kRegexpRepeat:
			return ((a->parse_flags ^ b->parse_flags) & kParseNonGreedy) == 0 && a->min == b->min && a->max == b->max;

		case kRegexpCapture:
			return a->cap == b->cap && str(a->name) == b->name;

		case kRegexpHaveMatch:
			return a->match_id == b->match_id;

		case kRegexpCharClass:
			return *a->cc == *b->cc;

		default:
			ISO_ASSERT(0);
			return false;
	}
}

bool Regexp::Equal(Regexp* a, Regexp* b) {
	if (!a || !b)
		return a == b;

	if (!TopEqual(a, b))
		return false;

	dynamic_array<Regexp*> stk;
	for (;;) {
		auto asubs = a->Subs();
		auto bsubs = b->Subs();
		if (asubs.empty()) {
			if (stk.empty())
				return true;

			b = stk.pop_back_value();
			a = stk.pop_back_value();
			continue;
		}
		a = asubs[0];
		b = asubs[1];
		if (!TopEqual(a, b))
			return false;
		for (int i = 1; i < asubs.size(); i++) {
			Regexp* a2 = asubs[i];
			Regexp* b2 = bsubs[i];
			if (!TopEqual(a2, b2))
				return false;
			stk.push_back(a2);
			stk.push_back(b2);
		}
	}
}

// If every match of this regexp must be anchored and begin with a non-empty fixed string, return the prefix and the sub-regexp that follows it
Regexp *Regexp::RequiredPrefix(Regexp** suffix) {
	// No need for a walker: the regexp must be of the form
	// 1. some number of ^ anchors
	// 2. a literal char or string
	// 3. the rest

	*suffix		= NULL;
	if (op != kRegexpConcat)
		return nullptr;

	// Some number of anchors, then a literal or concatenation
	auto	subs	= Subs();
	Regexp	**i		= subs.begin(), **e = subs.end();
	Regexp	*re;

	while (i != e && (re = *i)->op == kRegexpEmptyMatch && (re->empty_flags & kEmptyBeginText))
		i++;

	if (i == subs.begin() || i == e || (re->op != kRegexpLiteralString && re->op != kRegexpLiteral))
		return 0;

	// The rest
	++i;
	switch (e - i) {
		case 0:
			*suffix = Nop(parse_flags);
			break;
		case 1:
			*suffix = (*i)->addref();
			break;
		default:
			*suffix = ConcatQuick(DupRange(make_range(i, e)), parse_flags);
			break;
	}
	return re;
}
#if 0
bool Regexp::IsAnchor(Regexp** pre, EmptyOp end, int depth) {
	Regexp* re = *pre;

	// The depth limit makes sure that we don't overflow the stack on a deeply nested regexp; IsAnchor is conservative, so returning a false negative is okay
	if (re == NULL || depth >= 4)
		return false;

	switch (re->op) {
		default:
			break;
		case kRegexpConcat:
			if (re->nsub > 0) {
				Regexp*	sub = re->subs[0]->addref();
				if (IsAnchor(&sub, end, depth + 1)) {
					Regexp** subcopy = new Regexp*[re->nsub];
					subcopy[0] = sub;	// already have reference
					for (int i = 1; i < re->nsub; i++)
						subcopy[i] = re->subs[i]->addref();
					*pre = ConcatQuick(make_range_n(subcopy, re->nsub), re->parse_flags);
					re->release();
					return true;
				}
				sub->release();
			}
			break;
		case kRegexpCapture: {
			Regexp*	sub = re->sub->addref();
			if (IsAnchor(&sub, end, depth + 1)) {
				*pre = Regexp::Capture(sub, re->parse_flags, re->cap);
				re->release();
				return true;
			}
			sub->release();
			break;
		}
		case kRegexpEmptyMatch:
			if (re->empty_flags & end) {
				*pre = Regexp::Nop(re->parse_flags);
				re->release();
				return true;
			}
			break;
	}
	return false;
}
#else
bool Regexp::IsAnchor(Regexp** pre, EmptyOp end, int depth) {
	Regexp	*re = *pre;

	for (dynamic_array<Regexp*> stack;;) {
		switch (re->op) {
			case kRegexpAlternate:
				stack.append(re->Subs());
				break;

			case kRegexpConcat:
				if (re->nsub == 0)
					return false;
				re = re->subs[end & kEmptyEnds ? re->nsub -1 : 0];
				continue;

			case kRegexpCapture:
				re = re->sub;
				continue;

			case kRegexpEmptyMatch:
				if (!(re->empty_flags & end))
					return false;
				break;

			default:
				return false;
		}
		if (stack.empty())
			break;

		re = stack.pop_back_value();
	}

	// got the anchor, so remove them!

	for (dynamic_array<Regexp**> stack;;) {
		Regexp	*re = *pre;
		switch (re->op) {
			case kRegexpAlternate: {
				auto	newsubs = DupRange(re->Subs());
				Regexp* re2	= new Regexp(kRegexpAlternate, re->parse_flags);
				re2->subs	= newsubs.begin();
				re2->nsub	= re->nsub;
				for (auto &i : newsubs)
					stack.push_back(&i);
				break;
			}
			case kRegexpConcat: {
				auto	newsubs = DupRange(re->Subs());
				*pre = ConcatQuick(newsubs, re->parse_flags);
				re->release();
				pre = end & kEmptyEnds ? &newsubs.back() : &newsubs.front();
				continue;
			}

			case kRegexpCapture:
				*pre = Capture(re->sub->addref(), re->parse_flags, re->cap);
				re->release();
				pre = &(*pre)->sub;
				continue;

			case kRegexpEmptyMatch:
				*pre = Nop(re->parse_flags);
				re->release();
				break;

			default:
				break;
		}
		if (stack.empty())
			break;

		pre = stack.pop_back_value();
	}
	return true;
}
#endif

// Factors common prefixes from alternation
// For example,
//		ABC|ABD|AEF|BCX|BCY
// simplifies to
//		A(B(C|D)|EF)|BC(X|Y)
// and thence to
//		A(B[CD]|EF)|BC[XY]
//
// Rewrites sub to contain simplified list to alternate and returns the new length of sub

int Regexp::FactorAlternation(const range<Regexp**> &sub, ParseFlags flags) {
	struct Splice {
		Regexp*			prefix;		// factored prefix or merged character class computed by one iteration of one round of factoring
		range<Regexp**>	sub;		// the span of subexpressions of the alternation to be "spliced" (i.e. removed and replaced)
		int				nsuffix;	// the number of suffixes after any factoring that might have subsequently been performed on them (ignored for a merged character class)
		range<Regexp**>	suffix() const	{ return sub.slice(0, nsuffix); }
		Splice(Regexp* prefix, const range<Regexp**> &sub) : prefix(prefix), sub(sub), nsuffix(-1) {}
	};

	struct Frame {
		range<Regexp**>			sub;		//the span of subexpressions of the alternation to be factored;
		int						round;		//the current round of factoring
		dynamic_array<Splice>	splices;	//computed Splices
		Splice*					spliceiter;	//for a factored prefix, an iterator to the next Splice to be factored (i.e. in another Frame) because suffixes
		Frame(const range<Regexp**> &sub) : sub(sub), round(0) {}
	};

	dynamic_array<Frame> stk;
	stk.push_back(sub);

	for (;;) {
		auto&	back 	= stk.back();

		if (back.splices.empty()) {
			// Advance to the next round of factoring (note that this covers the initialised state: when splices is empty and round is 0)
			back.round++;

		} else if (back.spliceiter != back.splices.end()) {
			// We have at least one more Splice to factor; recurse logically
			stk.emplace_back(back.spliceiter->sub);
			continue;

		} else {
			// We have no more Splices to factor, so apply them
			auto	*src = back.sub.begin(), *dst = src;

			for (auto &splice : back.splices) {
				// Copy until we reach where the next Splice begins
				while (src < splice.sub.begin())
					*dst++ = *src++;

				switch (back.round) {
					case 1:
					case 2:
						// Assemble the Splice prefix and the suffixes
						*dst++ = Regexp::Concat2(splice.prefix, Regexp::Alternate(splice.suffix(), flags, false), flags);
						break;
					case 3:
						// Just use the Splice prefix
						*dst++ = splice.prefix;
						break;
					default:
						break;
				}
				src = splice.sub.end();
			}

			// copy until the end of sub
			while (src < back.sub.end())
				*dst++ = *src++;

			back.splices.clear();
			back.sub = make_range(back.sub.begin(), dst);

			// Advance to the next round of factoring
			back.round++;
		}

		switch (back.round) {
			case 1: {
				// Round 1: Factor out common literal prefixes
				range<char32*>	chars;
				ParseFlags		runeflags = kParseNone;

				for (int start = 0, i = 0; i <= back.sub.size32(); i++) {
					// Invariant: sub[start:i] consists of regexps that all begin with rune[0:nrune]
					range<char32*>	runes_i;
					ParseFlags		runeflags_i = kParseNone;

					if (i < back.sub.size32()) {
						Regexp	*lead = back.sub[i]->Leading();
						runeflags_i	= lead->parse_flags & kParseFoldCase;
						runes_i		= lead->Characters();
						if (runeflags_i == runeflags) {
							int same = 0;
							while (same < chars.size() && same < runes_i.size() && chars[same] == runes_i[same])
								same++;
							if (same > 0) {
								// Matches at least one rune in current range - keep going around
								chars = chars.slice(0, same);
								continue;
							}
						}
					}

					// Found end of a run with common leading literal string - sub[start:i] all begin with rune[0:nrune], but sub[i] does not even begin with rune[0]
					if (i - start > 1) {
						Regexp* prefix = Regexp::LiteralString(chars, runeflags);
						for (int j = start; j < i; j++)
							back.sub[j]->RemoveLeadingString(chars.size32());
						back.splices.emplace_back(prefix, back.sub.slice(start, i - start));
					}

					// Prepare for next iteration
					start		= i;
					chars		= runes_i;
					runeflags	= runeflags_i;
				}

				back.spliceiter = back.splices.begin();
				break;
			}

			case 2: {
				// Round 2: Factor out common simple prefixes, just the first piece of each concatenation
				// Complex subexpressions (e.g. involving quantifiers) are not safe to factor because that collapses their distinct paths through the automaton, which affects correctness in some cases
				Regexp* first = NULL;
				for (int start = 0, i = 0; i <= back.sub.size32(); i++) {
					// Invariant: sub[start:i] consists of regexps that all begin with first
					Regexp* first_i = NULL;
					if (i < back.sub.size32()) {
						first_i = back.sub[i]->TopLeading();
						// first must be an empty-width op
						// OR a char class, any char or any byte
						// OR a fixed repeat of a literal, char class, any char or any byte
						if (first
							&& (	(first->op == kRegexpEmptyMatch && first->empty_flags)
								||	first->op == kRegexpCharClass
								||	first->op == kRegexpAnyChar
								||	(first->op == kRegexpRepeat && first->min == first->max && (first->sub->op == kRegexpLiteral || first->sub->op == kRegexpCharClass || first->sub->op == kRegexpAnyChar))
							) && Regexp::Equal(first, first_i)
						)
							continue;
					}

					// Found end of a run with common leading regexp - sub[start:i] all begin with first, but sub[i] does not
					if (i - start > 1) {
						Regexp* prefix = first->addref();
						for (int j = start; j < i; j++)
							back.sub[j] = back.sub[j]->RemoveLeadingRegexp();
						back.splices.emplace_back(prefix, back.sub.slice(start, i - start));
					}
					// Prepare for next iteration
					start = i;
					first = first_i;
				}
				back.spliceiter = back.splices.begin();
				break;
			}

			case 3: {
				// Round 3: Merge runs of literals and/or character classes
				Regexp* first = NULL;
				for (int start = 0, i = 0; i <= back.sub.size32(); i++) {
					// Invariant: sub[start:i] consists of regexps that all are either literals or character classes
					Regexp* first_i = NULL;
					if (i < back.sub.size32()) {
						first_i = back.sub[i];
						if (first
							&& (first->op == kRegexpLiteral || first->op == kRegexpCharClass)
							&& (first_i->op == kRegexpLiteral || first_i->op == kRegexpCharClass)
						)
							continue;
					}

					// Found end of a run of Literal/CharClass - sub[start:i] all are either one or the other, but sub[i] is not
					if (i - start > 1) {
						CharClassBuilder ccb;
						for (int j = start; j < i; j++) {
							Regexp* re = back.sub[j];
							if (re->op == kRegexpCharClass) {
								for (auto i : *re->cc)
									ccb.AddRange(i.lo, i.hi);
							} else {
								AddRange(&ccb, re->rune, re->rune, re->parse_flags);
							}
							re->release();
						}
						Regexp* re = new Regexp(kRegexpCharClass, flags);
						re->cc = ccb.GetCharClass();
						back.splices.emplace_back(re, back.sub.slice(start, i - start));
					}
					// Prepare for next iteration
					start = i;
					first = first_i;
				}
				back.spliceiter = back.splices.end();
				break;
			}

			case 4: {
				int	nsuffix = back.sub.size32();
				// We are at the top of the stack - just return
				if (stk.size() == 1)
					return nsuffix;

				// Pop the stack and set the number of suffixes
				stk.pop_back();
				stk.back().spliceiter->nsuffix = nsuffix;
				++stk.back().spliceiter;
				break;
			}

			default:
				break;
		}
	}
}

template<typename T> class Regexp::Walker {
	struct WalkState {
		Regexp* re;
		int		n;			// The index of the next child to process; -1 means PreVisit
		int		child_args;	// index into arg stack of first child
		T		parent_arg;
		T		pre_arg;
		WalkState(Regexp* re, T parent) : re(re), n(-1), child_args(0), parent_arg(parent) {}
	};

public:
	virtual ~Walker() {}
	virtual T PreVisit(Regexp* re, T parent_arg, bool* stop)								{ return parent_arg; }
	virtual T PostVisit(Regexp* re, T parent_arg, T pre_arg, const range<T*> &child_args)	{ return pre_arg; }
	virtual T Copy(T arg)																	{ return arg; }

	T	Walk(Regexp* re, T top_arg, bool use_copy);
};

template<typename T> T Regexp::Walker<T>::Walk(Regexp* re, T top_arg, bool use_copy) {
	dynamic_array<WalkState>	stack;
	dynamic_array<T>			arg_stack;

	stack.push_back(WalkState(re, top_arg));

	for (;;) {
		WalkState	&s		= stack.back();
		int			n		= s.n++;
		Regexp*		re		= s.re;
		auto		subs	= re->Subs();
		T			t;

		if (n < 0) {
			bool	stop	= false;
			t = PreVisit(re, s.parent_arg, &stop);

			if (!stop) {
				s.pre_arg		= t;
				s.child_args	= arg_stack.size32();
				arg_stack.expand(subs.size());
				n	= s.n++;
			}
		}

		if (n >= 0) {
			if (n < subs.size()) {
				if (use_copy && n > 0 && subs[n - 1] == subs[n])
					arg_stack[s.child_args + n] = Copy(arg_stack[s.child_args + n - 1]);
				else
					stack.push_back(WalkState(subs[n], s.pre_arg));

				continue;
			}

			t = PostVisit(re, s.parent_arg, s.pre_arg, n ? make_range_n(&arg_stack[s.child_args], n) : none);
			arg_stack.resize(s.child_args);
		}

		stack.pop_back();
		if (stack.size() == 0)
			return t;

		{
			auto	&s = stack.back();
			arg_stack[s.child_args + s.n - 1] = t;
		}
	}
}

//-----------------------------------------------------------------------------
//	Compiler
//-----------------------------------------------------------------------------

static bool ChildArgsChanged(const range<Regexp**> &subs, const range<Regexp**> &child_args) {
	if (subs.size() != child_args.size() || memcmp(subs.begin(), child_args.begin(), subs.size() * sizeof(Regexp*)) != 0)
		return true;
	for (auto i: child_args)
		i->release();
	return false;
}

class SimplifyWalker : public Regexp::Walker<Regexp*> {

	static bool Coalesce(Regexp** r1p, Regexp* r2) {
		Regexp* r1 = *r1p;

		if (r1->op == kRegexpRepeat && (r1->sub->op == kRegexpLiteral || r1->sub->op == kRegexpCharClass || r1->sub->op == kRegexpAnyChar)) {
			switch (r2->op) {
				case kRegexpRepeat:
					if (Regexp::Equal(r1->sub, r2->sub) && ((r1->parse_flags & kParseNonGreedy) == (r2->parse_flags & kParseNonGreedy))) {
						r1p[0] = Regexp::Repeat(r1->sub, r1->parse_flags, r1->min + r2->min, r1->max == -1 || r2->max == -1 ? -1 : r1->max + r2->max);
						r2->release();
						return true;
					}
					break;

				case kRegexpLiteral:
				case kRegexpCharClass:
				case kRegexpAnyChar:
					if (Regexp::Equal(r1->sub, r2)) {
						r1p[0] = Regexp::Repeat(r1->sub, r1->parse_flags, r1->min + 1, r1->max == -1 ? -1 : r1->max + 1);
						r2->release();
						return true;
					}
					break;

				case kRegexpLiteralString: {
					if (r1->sub->op == kRegexpLiteral && r2->chars[0] == r1->sub->rune && ((r1->sub->parse_flags & kParseFoldCase) == (r2->parse_flags & kParseFoldCase))) {
						uint32 r = r1->sub->rune;
						// Determine how much of the literal string is removed (we know that we have at least one rune)
						int n = 1;
						while (n < r2->nchars && r2->chars[n] == r)
							n++;

						r1p[0] = Regexp::Repeat(r1->sub, r1->parse_flags, r1->min + n, r1->max == -1 ? -1 : r1->max + n);
						if (n == r2->nchars) {
							r2->release();
							return true;
						}
						r1p[1] = Regexp::LiteralString(make_range_n(r2->chars, r2->nchars).slice(n), r2->parse_flags);
						r2->release();
						return false;	// so skips to next pair
					}
					break;
				}

				default:
					break;
			}
		}
		return false;
	}

public:
	SimplifyWalker() {}
	virtual Regexp* PreVisit(Regexp* re, Regexp* parent_arg, bool* stop) {
		if (re->simple) {
			*stop = true;
			return re->addref();
		}
		return NULL;
	}

	virtual Regexp* PostVisit(Regexp* re, Regexp* parent_arg, Regexp* pre_arg, const range<Regexp**> &child_args) {
		switch (re->op) {
			case kRegexpNoMatch:
			case kRegexpEmptyMatch:
			case kRegexpLiteral:
			case kRegexpLiteralString:
			case kRegexpAnyChar:
			case kRegexpHaveMatch:
				// these are always simple
				re->simple = true;
				return re->addref();

			case kRegexpConcat: {
				int	i = 0;
				for (int j = 1; j < child_args.size32(); ++j) {
					if (!Coalesce(&child_args[i], child_args[j]))
						++i;
				}

				auto	args = child_args.slice(0, i + 1);
				if (!ChildArgsChanged(re->Subs(), args)) {
					re->simple = true;
					return re->addref();
				}

				// Something changed - build a new op
				Regexp* nre = new Regexp((RegexpOp)re->op, re->parse_flags);
				nre->SetSub(args);
				nre->simple = true;
				return nre;
			}

			case kRegexpAlternate: {
				// These are simple as long as the subpieces are simple
				if (!ChildArgsChanged(re->Subs(), child_args)) {
					re->simple = true;
					return re->addref();
				}
				Regexp* nre = new Regexp((RegexpOp)re->op, re->parse_flags);
				nre->SetSub(child_args);
				nre->simple = true;
				return nre;
			}

			case kRegexpCapture: {
				Regexp* newsub = child_args[0];
				if (newsub == re->sub) {
					newsub->release();
					re->simple = true;
					return re->addref();
				}
				Regexp* nre = Regexp::Capture(newsub, re->parse_flags, re->cap);
				nre->simple = true;
				return nre;
			}

			case kRegexpRepeat: {
				Regexp* newsub = child_args[0];
				// repeat the empty string as much as you want, but it's still the empty string
				if (newsub->op == kRegexpEmptyMatch)
					return newsub;

				int			min = re->min, max = re->max;
				ParseFlags	parse_flags = re->parse_flags;

				Regexp* nre	= nullptr;

				if (min <= 1 && max <= 1) {
					// These are simple as long as the subpiece is simple
					if (newsub == re->sub) {
						newsub->release();
						re->simple = true;
						return re->addref();
					}
					nre	= Regexp::Repeat(newsub, parse_flags, min, max);
					nre->simple	= true;
					return nre;
				}

				// General case: x{n,m} means n copies of x and m copies of x?
				// The machine will do less work if we nest the final m copies, so that x{2,5} = xx(x(x(x)?)?)?
				if (min > 0) {
					Regexp** nre_subs = new Regexp*[min];
					for (int i = 0; i < min; i++)
						nre_subs[i] = newsub->addref();

					if (max == -1)
						nre_subs[min - 1] = Regexp::Plus(newsub, parse_flags);

					nre = Regexp::ConcatQuick(make_range_n(nre_subs, min), parse_flags);
				}

				// Build and attach suffix: (x(x(x)?)?)?
				if (max > min) {
					Regexp* suf = Regexp::Quest(newsub->addref(), parse_flags);
					for (int i = min + 1; i < max; i++)
						suf = Regexp::Quest(Regexp::Concat2(newsub->addref(), suf, parse_flags), parse_flags);
					nre = nre ? Regexp::Concat2(nre, suf, parse_flags) : suf;
				}

				newsub->release();
				nre->simple = true;
				return nre;
			}

			case kRegexpCharClass: {
				Regexp* nre	= re->cc->empty()	? new Regexp(kRegexpNoMatch, re->parse_flags)
							: re->cc->full()	? new Regexp(kRegexpAnyChar, re->parse_flags)
							: re->addref();
				nre->simple = true;
				return nre;
			}
			default:
				ISO_ASSERT(0);
				return re->addref();
		}

	}
	virtual Regexp* Copy(Regexp* re) { return re->addref(); }
};

Regexp* Regexp::Simplify() {
	return SimplifyWalker().Walk(this, NULL, true);
}

struct PatchList {
	uint32 p;	// refers to p&1 == 0 ? inst[p>>1].out : inst[p>>1].out1

	PatchList(uint32 p = 0) : p(p) {}
	operator bool() const { return p != 0; }

	// Deref returns the next pointer pointed at by p
	PatchList Deref(Prog::Inst *inst0) const {
		Prog::Inst& ip = inst0[p >> 1];
		return p & 1 ? ip.out1 : ip.out;
	}

	// Patches all the entries on l to have value v; caller must not use patch list again
	void Patch(Prog::Inst *inst0, uint32 v) {
		uint32 l	= p;
		while (l != 0) {
			Prog::Inst &ip = inst0[l >> 1];
			if (l & 1) {
				l = ip.out1;
				ip.out1 = v;
			} else {
				l = ip.out;
				ip.out = v;
			}
		}
	}

	// Appends two patch lists and returns result
	static PatchList Append(Prog::Inst *inst0, PatchList l1, PatchList l2) {
		if (l1.p == 0)
			return l2;
		if (l2.p == 0)
			return l1;

		PatchList end = l1;
		while (PatchList next = end.Deref(inst0))
			end = next;

		Prog::Inst &ip = inst0[end.p >> 1];
		if (end.p & 1)
			ip.out1 = l2.p;
		else
			ip.out = l2.p;

		return l1;
	}
};

// Compiled program fragment
struct Frag {
	uint32		begin;
	PatchList	end;
	Frag(uint32 begin = 0, PatchList end = PatchList()) : begin(begin), end(end) {}
	bool operator!() const { return begin == 0; }
	friend bool IsNoMatch(Frag a) { return a.begin == 0; }
};

class Compiler : public Regexp::Walker<Frag> {
public:
	enum Encoding {
		ascii,
		utf8,
		wchar,
	};
	class RuneCache;

	bool		reversed;		// Should program run backward over text?
	Encoding	encoding;		// Input encoding
	bool		anchor_end;		// anchor mode for Set
	uint32		max_capture;
	dynamic_array<Prog::Inst>	inst;

public:
	Compiler(Encoding encoding, bool anchor_end) : reversed(false), encoding(encoding), anchor_end(anchor_end), max_capture(0) {
		inst.push_back(kInstEmptyWidth).empty = kEmptyNever;
	}

	static Prog *Compile(Regexp* re, Encoding encoding, bool reversed = false);
	static Prog* CompileSet(Regexp* re, Encoding encoding, bool anchor_start, bool anchor_end);

	Frag PreVisit(Regexp* re, Frag parent_arg, bool* stop) {
		return Frag();		// not used by caller
	}
	Frag PostVisit(Regexp* re, Frag parent_arg, Frag pre_arg, const range<Frag*> &child_args);

	Frag Copy(Frag arg) {
		ISO_ASSERT(0);
		return Frag();
	}

	uint32 AllocAlt(uint32 out, uint32 out1) {
		uint32	id	= inst.size32();
		inst.emplace_back(kInstAlt, out).out1 = out1;
		return id;
	}

	Frag NonGreedyStar(Frag a) {
		int id = AllocAlt(0, a.begin);
		a.end.Patch(inst, id);
		return Frag(id, PatchList(id << 1));
	}

	// Given fragment a, returns (a) capturing as \n
	Frag Capture(Frag a, int n) {
		if (!a)
			return a;

		max_capture = max(max_capture, n);

		uint32	id	= inst.size32();
		inst.emplace_back(kInstCapture, a.begin).cap	= n * 2;
		inst.push_back(kInstCapture).cap				= n * 2 + 1;
		a.end.Patch(inst, id + 1);
		return Frag(id, PatchList((id + 1) << 1));
	}

	// Given fragments a and b, returns ab (returns b if a is NoMatch)
	Frag Cat(Frag a, Frag b) {
		if (!a || !b)
			return b;

		// Elide no-op
		Prog::Inst& begin = inst[a.begin];
		if (begin.IsNop() && a.end.p == (a.begin << 1) && begin.out == 0) {
			a.end.Patch(inst, b.begin);		// in case refs to a somewhere
			return b;
		}
		// To run backward over string, reverse all concatenations
		if (reversed) {
			b.end.Patch(inst, a.begin);
			return Frag(b.begin, a.end);
		} else {
			a.end.Patch(inst, b.begin);
			return Frag(a.begin, b.end);
		}
	}

	// Given fragments a and b, returns a|b
	Frag Alt(Frag a, Frag b) {
		return	!a ? b
			:	!b ? a
			:	Frag(AllocAlt(a.begin, b.begin), PatchList::Append(inst, a.end, b.end));
	}

	// Returns a fragment that matches the empty string
	Frag Match(int32 match_id) {
		uint32	id	= inst.size32();
		inst.push_back(kInstMatch).match_id = match_id;
		return Frag(id, PatchList());
	}

	// Returns a no-op fragment
	Frag Nop() {
		return EmptyWidth(kEmptyNone);
	}

	// Returns a fragment matching the byte range lo-hi
	Frag ByteRange(int lo, int hi, bool foldcase) {
		uint32	id		= inst.size32();
		auto	&ip		= inst.push_back(kInstByteRange);
		int		lobank	= lo >> 5;
		int		hibank	= hi >> 5;
		if (lobank == hibank) {
			ip.mask		= bits(hi - lo + 1, lo % 32);
			ip.extra	= foldcase && (lobank == ('A' >> 5) || lobank == ('a' >> 5)) ? ('A'>> 5) | 8 : lobank;
		} else {
			ISO_ASSERT((lo & 31) == 0 && (hi & 31) == 31);
			ip.lobank	= lobank;
			ip.hibank	= hibank;
			ip.extra	= 15;
		}
		return Frag(id, PatchList(id << 1));
	}

	// Returns a fragment matching an empty-width special op
	Frag EmptyWidth(EmptyOp op) {
		uint32	id	= inst.size32();
		inst.push_back(kInstEmptyWidth).empty = op;
		return Frag(id, PatchList(id << 1));
	}

	// Returns .* where dot = any byte
	Frag	DotStar() {
		return NonGreedyStar(ByteRange(0x00, 0xff, false));
	}

	// Single rune
	Frag	Literal(uint32 r, bool foldcase) {
		if (encoding != utf8 || r < 0x80)
			return ByteRange(r, r, foldcase);

		char	buf[4];
		Frag	f;
		for (int i = 0, n = put_char(r, buf); i < n; i++)
			f = Cat(f, ByteRange((uint8)buf[i], (uint8)buf[i], false));
		return f;
	}

	void	Finish(Prog *prog) {
		if (prog->start == 0 && prog->start_unanchored == 0)
			inst.erase(inst.begin() + 1, inst.end());			// No possible matches; keep Fail instruction only

		// Hand off the array to Prog
		prog->inst			= move(inst);
		prog->max_capture	= max_capture;
#ifdef REGEX_DIAGNOSTICS
		DumpProg(lvalue(trace_accum()), prog);
#endif
		prog->Flatten();
#ifdef REGEX_DIAGNOSTICS
		DumpProg(lvalue(trace_accum()), prog);
#endif
		prog->bytemap_entries	= ComputeByteMap(prog->inst, prog->bytemap);
	}
};

class Compiler::RuneCache {
	struct entry {
		uint16	page, pagehi;
		uint32	mask;
		entry(uint16 page) : page(page), pagehi(page), mask(0) {}
		bool	contiguous() const {
			return page != pagehi || is_pow2((mask | lowest_set(mask) - 1) + 1);
		}
	};

	Compiler					*compiler;
	bool						fold_ascii;
	dynamic_array<entry>		entries;
	hash_map<uint32, uint32>	map;

	void AddRuneRange0(uint32 lo, uint32 hi) {
		if (!entries.empty()) {
			entry &back = entries.back();
			if (back.page == lo >> 5) {
				back.mask |= bits(hi - lo + 1, lo & 31);
				return;
			}
		}
		entry &back	= entries.push_back(lo >> 5);
		back.pagehi	= hi >> 5;
		back.mask	= bits(hi - lo + 1, lo & 31);
	}

	void AddRuneRangeASCII(uint32 lo, uint32 hi) {
		if ((lo & 31) != 0) {
			uint32	m = lo | 31;
			AddRuneRange0(lo, min(m, hi));
			lo = m + 1;
			if (lo > hi)
				return;
		}
		if ((lo & ~31) != (hi & ~31) && (hi & 31) != 31) {
			uint32	m = (hi & ~31) - 1;
			AddRuneRange0(lo, m);
			lo = m + 1;
		}
		AddRuneRange0(lo, hi);
	}

	void	AddRuneRangeUTF8(uint32 lo, uint32 hi);

	Frag	AddPrefix(int lo, int hi) {
		uint32	id	= compiler->inst.size32();
		auto	&ip	= compiler->inst.emplace_back(kInstByteRange);
		int		lobank	= lo >> 5;
		int		hibank	= hi >> 5;
		if (lobank == hibank) {
			ip.mask		= bits(hi - lo + 1, lo % 32);
			ip.extra	= lobank;
		} else {
			ISO_ASSERT((lo & 31) == 0 && (hi & 31) == 31);
			ip.lobank	= lobank;
			ip.hibank	= hibank;
			ip.extra	= 15;
		}
		return Frag(id, PatchList(id << 1));
	}

	Frag	AddMask(uint32 page, uint32 mask) {
		uint32	id	= compiler->inst.size32();
		auto	&ip	= compiler->inst.emplace_back(kInstByteRange);
		ip.extra	= page | (fold_ascii && page == 'a' / 32 ? 8 : 0);
		ip.mask		= mask;
		return Frag(id, PatchList(id << 1));
	}

	Frag	AddBlocks(uint32 lo, uint32 hi) {
		uint32	id	= compiler->inst.size32();
		auto	&ip	= compiler->inst.emplace_back(kInstByteRange);
		ip.extra	= 15;
		ip.lobank	= lo;
		ip.hibank	= hi;
		return Frag(id, PatchList(id << 1));
	}
public:
	operator Frag();
	RuneCache(Compiler *compiler, bool fold_ascii) : compiler(compiler), fold_ascii(fold_ascii) {}
	RuneCache &AddRuneRange(uint32 lo, uint32 hi) {
		if (lo <= hi) {
			if (compiler->encoding == utf8)
				AddRuneRangeUTF8(lo, hi);
			else
				AddRuneRangeASCII(lo, hi);
		}
		return *this;
	}
};

Compiler::RuneCache::operator Frag() {
	Frag	frag;

	for (auto &i : entries) {
		Frag	f;

		if (compiler->encoding == utf8) {
			char	ulo[4], uhi[4];
			int		n		= put_char(i.page << 5, ulo);
			int		m		= put_char((i.pagehi << 5) | 31, uhi);
			ISO_ASSERT(n == m);

			uint32	lo		= read_bytes<uint32le>(ulo, n);
			uint32	hi		= read_bytes<uint32le>(uhi, n);
			uint32	cache	= 0;

			for (int j = 0; j < n - 1; ++j) {
				uint32	&p = map[lo | (uint64(hi) << 32)];
				if (cache = p)
					break;

				Frag	f0 = AddPrefix(lo & 0xff, hi & 0xff);
				p	= f0.begin;
				f	= compiler->Cat(f, f0);
				lo >>= 8;
				hi >>= 8;
			}

			if (cache) {
				f	= compiler->Cat(f, cache);

			} else if (i.contiguous()) {
				uint32	&p = map[lo | (uint64(hi) << 32)];
				if (cache = p) {
					f	= compiler->Cat(f, cache);
				} else {
					Frag	f0 = i.page == i.pagehi ? AddMask(lo >> 5, i.mask) : AddBlocks(lo >> 5, hi >> 5);
					p	= f0.begin;
					f	= compiler->Cat(f, f0);
				}
			} else {
				f	= compiler->Cat(f, AddMask(lo >> 5, i.mask));
			}

		} else {
			f	= i.page == i.pagehi ? AddMask(i.page, i.mask) : AddBlocks(i.page, i.pagehi);
		}

		frag	= compiler->Alt(frag, f);
	}

	return frag;
}

void Compiler::RuneCache::AddRuneRangeUTF8(uint32 lo, uint32 hi) {
	if (lo > hi)
		return;

	if (hi < 0x80) {
		AddRuneRangeASCII(lo, hi);
		return;
	}

	// Split range into same-length sized ranges
	for (int i = 1; i < 4; i++) {
		uint32 max = max_utf8(i);
		if (lo <= max && max < hi) {
			AddRuneRangeUTF8(lo, max);
			AddRuneRangeUTF8(max + 1, hi);
			return;
		}
	}

	// Split range into sections that agree on leading bytes
	for (int i = 1; i < 4; i++) {
		uint32	s	= 6 * i;
		uint32	m5	= bits(s - 1);
		uint32	m6	= bits(s);

		if ((lo & ~m6) != (hi & ~m6)) {
			if ((lo & m6) != 0) {
				AddRuneRangeUTF8(lo, lo | m5);
				AddRuneRangeUTF8((lo | m5) + 1, hi);
				return;
			}
			if ((hi & m6) != m6) {
				AddRuneRangeUTF8(lo, (hi & ~m5) - 1);
				AddRuneRangeUTF8(hi & ~m5, hi);
				return;
			}
		}
	}

	AddRuneRange0(lo, hi);
}

Frag Compiler::PostVisit(Regexp* re, Frag parent_arg, Frag pre_arg, const range<Frag*> &child_args) {
	switch (re->op) {
		default:
			ISO_ASSERT(0);
			return Frag();

		case kRegexpNoMatch:
			return Frag();

		case kRegexpEmptyMatch:
			if (re->empty_flags)
				return EmptyWidth(reversed ? reverse(re->empty_flags) : re->empty_flags);
			return Nop();

		case kRegexpHaveMatch: {
			Frag f = Match(re->match_id);
			// Append \z or else the subexpression will effectively be unanchored; complemented by the kAnchorNone case in CompileSet()
			if (anchor_end)
				f = Cat(EmptyWidth(kEmptyEndText), f);
			return f;
		}
		case kRegexpConcat: {
			Frag f;
			for (int i = 0; i < child_args.size(); i++)
				f = Cat(f, child_args[i]);
			return f;
		}
		case kRegexpAlternate: {
			Frag f = child_args[0];
			for (int i = 1; i < child_args.size(); i++)
				f = Alt(f, child_args[i]);
			return f;
		}
		case kRegexpRepeat: {
			Frag	a	= child_args[0];
			if (!a)
				return Nop();

			uint32		id	= inst.size32();
			auto		&ip	= inst.push_back(kInstAlt);
			PatchList	pl;

			if (re->parse_flags & kParseNonGreedy) {
				ip.out1	= a.begin;
				pl = PatchList(id << 1);
			} else {
				ip.out1	= 0;
				ip.out	= a.begin;
				pl = PatchList((id << 1) | 1);
			}

			if (re->max == -1) {
				a.end.Patch(inst, id);

				if (re->min == 0)
					return Frag(id, pl);		//star

				if (re->min == 1)
					return Frag(a.begin, pl);	// plus

			} else if (re->max == 1) {
				// quest
				return Frag(id, PatchList::Append(inst, pl, a.end));
			}

			ISO_ASSERT(0);
		}

		case kRegexpLiteral:
			return Literal(re->rune, (re->parse_flags & kParseFoldCase) != 0);

		case kRegexpLiteralString: {
			ISO_ASSERT(re->nchars != 0);
			Frag f;
			for (int i = 0; i < re->nchars; i++)
				f = Cat(f, Literal(re->chars[i], !!(re->parse_flags & kParseFoldCase)));
			return f;
		}
		case kRegexpAnyChar:
			if (encoding == ascii)
				return ByteRange(0x00, 0xFF, false);
			return RuneCache(this, false).AddRuneRange(0, unicode::Runemax);

		case kRegexpCharClass: {
			CharClass* cc = re->cc;
			ISO_ASSERT(!cc->empty());
			RuneCache	cache(this, cc->FoldsASCII());
			for (auto &i : *cc)
				cache.AddRuneRange(i.lo, i.hi);
			return cache;
		}
		case kRegexpCapture:
			return re->cap ? Capture(child_args[0], re->cap) : child_args[0];
	}
}

Prog* Compiler::Compile(Regexp* re, Encoding encoding, bool reversed) {
	Compiler c(encoding, false);
	c.reversed = reversed;

	// Simplify to remove things like counted repetitions and character classes like \d
	Regexp* sre = re->Simplify();
#ifdef REGEX_DIAGNOSTICS
	DumpRegexp(lvalue(trace_accum()), sre);
#endif
	if (!sre)
		return NULL;

	// Record whether prog is anchored, removing the anchors (they get in the way of other optimizations)
	bool anchor_start	= Regexp::IsAnchor(&sre, kEmptyBeginText, 0);
	bool anchor_end		= Regexp::IsAnchor(&sre, kEmptyEndText, 0);
	if (reversed)
		iso::swap(anchor_start, anchor_end);

	// Generate fragment for entire regexp
	Frag all = c.Walk(sre, Frag(), false);
	sre->release();

	// Finish by putting Match node at end, and record start
	// Turn off c.reversed to force the remaining concatenations to behave normally
	c.reversed = false;
	all = c.Cat(all, c.Match(1));

	Prog	*prog = new Prog();
	prog->reversed		= reversed;
	prog->anchor_start	= anchor_start;
	prog->anchor_end	= anchor_end;

	prog->start = prog->start_unanchored = all.begin;

	// Also create unanchored version, which starts with a .*? loop
	if (!anchor_start) {
		all = c.Cat(c.DotStar(), all);
		prog->start_unanchored = all.begin;
	}

	c.Finish(prog);

	prog->onepass_states	= OnePass::Check(prog->inst, prog->start, prog->bytemap, prog->bytemap_entries);
#ifdef REGEX_DIAGNOSTICS
	if (prog->onepass_states) {
		ISO_TRACE("Bytemap:\n");
		DumpByteMap(lvalue(trace_accum()), prog->bytemap);
		DumpOnePass(lvalue(trace_accum()), prog->onepass_states, prog->bytemap_entries);
	}
#endif
	return prog;
}

Prog* Compiler::CompileSet(Regexp* re, Encoding encoding, bool anchor_start, bool anchor_end) {
	Compiler c(encoding, anchor_end);

	Regexp* sre = re->Simplify();
	if (!sre)
		return NULL;

	Frag all = c.Walk(sre, Frag(), false);
	sre->release();

	Prog		*prog	= new Prog();
	prog->anchor_start	= anchor_start;
	prog->anchor_end	= anchor_end;
	prog->many_match	= true;

	prog->start = prog->start_unanchored = all.begin;

	// Prepend .* or else the expression will effectively be anchored
	if (!anchor_start) {
		all = c.Cat(c.DotStar(), all);
		prog->start_unanchored = all.begin;
	}

	c.Finish(prog);
	return prog;
}

//-----------------------------------------------------------------------------
//	Parse
//-----------------------------------------------------------------------------

struct ParseState {
	ParseFlags		flags;
	Regexp*			stacktop;
	uint32			ncap;		// number of capturing parens seen

	ParseState(const ParseState&) = delete;
	ParseState& operator=(const ParseState&) = delete;

	// Checks whether a particular regexp op is a marker
	static bool IsMarker(uint32 op) {
		return op >= kLeftParen;
	}

	ParseState(ParseFlags flags) : flags(flags), stacktop(NULL), ncap(0) {}

	~ParseState() {
		Regexp* next;
		for (Regexp* re = stacktop; re != NULL; re = next) {
			next = re->down;
			re->down = NULL;
			if (re->op == kLeftParen)
				free(re->name);
			re->release();
		}
	}

	bool PushRegexp(Regexp* re) {
		MaybeConcatString(-1, kParseNone);
		if (!IsMarker(re->op))
			re->simple = re->ComputeSimple();
		re->down = stacktop;
		stacktop = re;
		return true;
	}

	bool PushEmptyOp(EmptyOp empty_flags) {
		Regexp* re	= new Regexp(kRegexpEmptyMatch, flags);
		re->empty_flags = empty_flags;
		return PushRegexp(re);
	}
	bool PushSimpleOp(RegexpOp op) {
		return PushRegexp(new Regexp(op, flags));
	}
	bool PushClass(CharClassBuilder *ccb) {
		Regexp* re;

		switch (ccb->num_chars()) {
			case 1:
				// A character class of one character is just a literal
				re = new Regexp(kRegexpLiteral, flags);
				re->rune = ccb->begin()->lo;
				break;
			case 2: {
				// [Aa] can be rewritten as a literal A with ASCII case folding
				uint32 r = ccb->begin()->lo;
				if (is_upper(r) && ccb->Contains(to_lower(r))) {
					re = new Regexp(kRegexpLiteral, flags | kParseFoldCase);
					re->rune = to_lower(r);
					break;
				}
			}
				//fall through
			default:
				re		= new Regexp(kRegexpCharClass, flags & ~kParseFoldCase);
				re->ccb	= ccb;
				break;
		}

		return PushRegexp(re);
	}
	bool PushLiteral(uint32 r) {
		// Do case folding if needed
		if ((flags & kParseFoldCase) && unicode::CycleFoldRune(r) != r) {
			auto ccb	= new CharClassBuilder;
			uint32 r1	= r;
			do {
				if (!(flags & kParseNeverNL) || r != '\n')
					ccb->AddRange(r, r);
				r = unicode::CycleFoldRune(r);
			} while (r != r1);
			return PushClass(ccb);
		}
		// Exclude newline if applicable
		if ((flags & kParseNeverNL) && r == '\n')
			return PushRegexp(new Regexp(kRegexpNoMatch, flags));

		// ordinary literal
		if (MaybeConcatString(r, flags))
			return true;

		Regexp* re	= new Regexp(kRegexpLiteral, flags);
		re->rune	= r;
		return PushRegexp(re);
	}

	bool PushDot() {
		if ((flags & kParseDotNL) && !(flags & kParseNeverNL))
			return PushSimpleOp(kRegexpAnyChar);
		// Rewrite . into [^\n]
		auto	ccb	= new CharClassBuilder;
		ccb->AddRange(0, '\n' - 1);
		ccb->AddRange('\n' + 1, unicode::Runemax);
		return PushClass(ccb);
	}

	bool PushRepeat(int min, int max, bool nongreedy) {
		if (max != -1 && max < min)
			return false;

		if (!stacktop || IsMarker(stacktop->op))
			return false;

		Regexp	*down	= stacktop->down;
		Regexp	*re		= Regexp::Repeat(FinishRegexp(stacktop), nongreedy ? flags ^ kParseNonGreedy : flags, min, max);
		re->down		= down;
		re->simple		= re->ComputeSimple();
		stacktop		= re;
		return true;
	}

	bool PushGroup(const unicode::Group *group, bool sign) {
		auto ccb	= new CharClassBuilder;
		AddGroup(ccb, group, sign, flags);
		return PushClass(ccb);
	}

	bool PushGroup(const PosixGroup *group, bool sign) {
		auto ccb	= new CharClassBuilder;
		AddGroup(ccb, group, sign, flags);
		return PushClass(ccb);
	}

	bool DoLeftParen(bool cap = true) {
		Regexp* re	= new Regexp(kLeftParen, flags);
		re->cap		= cap ? ++ncap : 0;
		return PushRegexp(re);
	}

	bool DoVerticalBar() {
		MaybeConcatString(-1, kParseNone);
		DoConcatenation();

		// Below the vertical bar is a list to alternate
		// Above the vertical bar is a list to concatenate
		// We just did the concatenation, so either swap the result below the vertical bar or push a new vertical bar on the stack
		Regexp* r1;
		Regexp* r2;
		if ((r1 = stacktop) && (r2 = r1->down) && r2->op == kVerticalBar) {
			Regexp* r3;
			if ((r3 = r2->down) && (r1->op == kRegexpAnyChar || r3->op == kRegexpAnyChar)) {
				// AnyChar is above or below the vertical bar. Let it subsume the other when the other is Literal, CharClass or AnyChar
				if (r3->op == kRegexpAnyChar && (r1->op == kRegexpLiteral || r1->op == kRegexpCharClass || r1->op == kRegexpAnyChar)) {
					// Discard r1
					stacktop = r2;
					r1->release();
					return true;
				}
				if (r1->op == kRegexpAnyChar && (r3->op == kRegexpLiteral || r3->op == kRegexpCharClass || r3->op == kRegexpAnyChar)) {
					// Rearrange the stack and discard r3
					r1->down = r3->down;
					r2->down = r1;
					stacktop = r2;
					r3->release();
					return true;
				}
			}
			// Swap r1 below vertical bar (r2)
			r1->down = r2->down;
			r2->down = r1;
			stacktop = r2;
			return true;
		}
		return PushSimpleOp(kVerticalBar);
	}

	bool DoRightParen() {
		// Finish the current concatenation and alternation
		DoAlternation();

		// The stack should be: LeftParen regexp; remove the LeftParen, leaving the regexp parenthesized
		Regexp* r1;
		Regexp* re;
		if (!(r1 = stacktop) || !(re = r1->down) || re->op != kLeftParen)
			return false;

		stacktop	= re->down;
		flags		= re->parse_flags;	// restore flags from when paren opened

		// Rewrite LeftParen as capture if needed
		if (re->cap) {
			re->op		= kRegexpCapture;
			re->sub		= FinishRegexp(r1);
			re->simple	= re->ComputeSimple();

		} else {
			switch (re->match_id) {
				case 1:	//positive lookahead
					PushRegexp(r1);
					re->op		= kVerticalBar;
					break;
				case 2:	//negative lookahead
				default:
					re->release();
					re = r1;
			}
		}
		return PushRegexp(re);
	}

	// Processes the end of input, returning the final regexp
	Regexp* DoFinish() {
		DoAlternation();
		Regexp* re = stacktop;
		if (re && re->down)
			return NULL;
		stacktop = NULL;
		return FinishRegexp(re);
	}

	// Finishes the regexp if necessary, preparing it for use in a more complicated expression
	// If it is a CharClassBuilder, converts into a CharClass
	Regexp* FinishRegexp(Regexp *re) {
		if (re) {
			re->down = NULL;
			if (re->op == kRegexpCharClass && re->ccb != NULL) {
				CharClassBuilder* ccb = re->ccb;
				re->ccb = NULL;
				re->cc	= ccb->GetCharClass();
				delete ccb;
			}
		}
		return re;
	}

	// Collapse the regexps on top of the stack, down to the first marker, into a new op node (op == kRegexpAlternate or op == kRegexpConcat)
	void DoCollapse(RegexpOp op);

	// Finishes the current concatenation, collapsing it into a single regexp on the stack
	void DoConcatenation() {
		// empty concatenation is special case
		if (!stacktop || IsMarker(stacktop->op))
			PushRegexp(Regexp::Nop(flags));

		DoCollapse(kRegexpConcat);
	}

	// Finishes the current alternation, collapsing it to a single regexp on the stack
	void DoAlternation() {
		DoVerticalBar();
		Regexp	*r1 = stacktop;
		stacktop = r1->down;
		r1->release();
		DoCollapse(kRegexpAlternate);
	}

	// Maybe concatenate Literals into LiteralString
	bool MaybeConcatString(int r, ParseFlags flags);
};

void ParseState::DoCollapse(RegexpOp op) {
	// Scan backward to marker, counting children of composite
	uint32	n		= 0;
	Regexp* next	= NULL;
	for (Regexp* sub = stacktop; sub && !IsMarker(sub->op); sub = next) {
		next = sub->down;
		if (sub->op == op)
			n += sub->Subs().size32();
		else
			n++;
	}

	// If there's just one child, leave it alone (concat of one thing is that one thing; alternate of one thing is same)
	if (stacktop && stacktop->down == next)
		return;

	// Construct op (alternation or concatenation), flattening op of op
	Regexp** subs = new Regexp*[n];
	next	= NULL;
	uint32 i = n;
	for (Regexp* sub = stacktop; sub && !IsMarker(sub->op); sub = next) {
		next = sub->down;
		if (sub->op == op) {
			for (auto k : reversed(sub->Subs()))
				subs[--i] = k->addref();
			sub->release();
		} else {
			subs[--i] = FinishRegexp(sub);
		}
	}

	if (n > 1 && op == kRegexpAlternate)
		n = Regexp::FactorAlternation(make_range_n(subs, n), flags);

	Regexp* re;

	if (n == 1) {
		re = subs[0];
		delete[] subs;
	} else {
		re	= new Regexp(op, flags);
		re->subs	= subs;
		re->nsub	= n;
	}

	re->simple = re->ComputeSimple();
	re->down = next;
	stacktop = re;
}

bool ParseState::MaybeConcatString(int r, ParseFlags flags) {
	Regexp* re1;
	Regexp* re2;
	if (
		!(re1 = stacktop)
	||	!(re2 = re1->down)
	||	(re1->op != kRegexpLiteral && re1->op != kRegexpLiteralString)
	||	(re2->op != kRegexpLiteral && re2->op != kRegexpLiteralString)
	||	(re1->parse_flags & kParseFoldCase) != (re2->parse_flags & kParseFoldCase)
		)
		return false;

	uint32	nrunes2 = re2->op == kRegexpLiteral ? 1 : re2->nchars;
	uint32	nchars	= nrunes2 + (re1->op == kRegexpLiteral ? 1 : re1->nchars);

	if (re2->op == kRegexpLiteral) {
		uint32	rune	= re2->rune;
		re2->op			= kRegexpLiteralString;
		re2->chars		= allocate<char32>(nchars);
		re2->chars[0]	= rune;
	} else {
		re2->chars		= reallocate(re2->chars, nrunes2, nchars);
	}
	re2->nchars = nchars;

	// push re1 into re2
	if (re1->op == kRegexpLiteral) {
		re2->chars[nrunes2] = re1->rune;
	} else {
		for (int i = 0; i < re1->nchars; i++)
			re2->chars[nrunes2 + i] = re1->chars[i];
		deallocate(re1->chars, re1->nchars);
		re1->nchars = 0;
		re1->chars	= NULL;
	}

	// reuse re1 if possible
	if (r >= 0) {
		re1->op		= kRegexpLiteral;
		re1->rune	= r;
		re1->parse_flags = flags;
		return true;
	}

	stacktop = re2;
	re1->release();
	return false;
}

bool ParseEscape(string_scan &s, char32 &rp) {
	char32 c, c1;
	if (!s.move(1).get_utf8(c))
		return false;

	switch (c) {
		// Octal
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
			// Single non-zero octal digit is a backreference; not supported
			if (s.remaining() == 0 || !iso::between(s.peekc(), '0', '7'))
				return false;
			//FALLTHROUGH_INTENDED;
		case '0':
			// consume up to three octal digits; already have one
			rp = c - '0';
			if (s.remaining() > 0 && iso::between(c = s.peekc(), '0', '7')) {
				rp = rp * 8 + c - '0';
				s.move(1);
				if (s.remaining() > 0 && iso::between(c = s.peekc(), '0', '7')) {
					rp = rp * 8 + c - '0';
					s.move(1);
				}
			}
			return true;

		// Hexadecimal
		case 'x':
			if (s.remaining() == 0 || !s.get_utf8(c))
				return false;

			if (c == '{') {
				// Any number of digits in braces
				if (!s.get_utf8(c))
					return false;
				int nhex = 0;
				rp = 0;
				while (is_hex(c)) {
					nhex++;
					rp = rp * 16 + from_digit(c);
					if (s.remaining() == 0)
						return false;
					if (!s.get_utf8(c))
						return false;
				}
				if (c != '}' || nhex == 0)
					return false;
				return true;
			}
			//two hex digits
			if (s.remaining() == 0 || !s.get_utf8(c1) || !is_hex(c) || !is_hex(c1))
				return false;
			rp = from_digit(c) * 16 + from_digit(c1);
			return true;

		case 'c':
			if (s.remaining() == 0)
				return false;
			rp = s.getc() & 31;
			return true;

		// C escapes
		case 'n': rp = '\n'; return true;
		case 'r': rp = '\r'; return true;
		case 't': rp = '\t'; return true;
		case 'a': rp = '\a'; return true;
		case 'f': rp = '\f'; return true;
		case 'v': rp = '\v'; return true;

		default:
			// Escaped non-word characters are always themselves
			if (c >= 0x80 || is_wordchar(c))
				return false;
			rp = c;
			return true;
	}
}

static const interval<char16> range_word[] = {
	{ '0', '9' },
	{ 'A', 'Z' },
	{ 'a', 'z' },
	{ '_', '_' },
};
static const interval<char16> range_ascii[] = {
	{ 0x0, 0x7f }, {0,0}
};
static const interval<char16> range_blank[] = {
	{ 0x9, 0x9 },
	{ 0x20, 0x20 }, {0,0}
};
static const interval<char16> range_cntrl[] = {
	{ 0x0, 0x1f },
	{ 0x7f, 0x7f }, {0,0}
};
static const interval<char16> range_graph[] = {
	{ 0x21, 0x7e }, {0,0}
};
static const interval<char16> range_print[] = {
	{ 0x20, 0x7e },
};
static const interval<char16> range_punct[] = {
	{ 0x21, 0x2f },
	{ 0x3a, 0x40 },
	{ 0x5b, 0x60 },
	{ 0x7b, 0x7e }, {0,0}
};
static const interval<char16> range_space[] = {
	{ 0x9, 0xd },
	{ 0x20, 0x20 }, {0,0}
};
static const interval<char16> range_xdigit[] = {
	{ 0x30, 0x39 },
	{ 0x41, 0x46 },
	{ 0x61, 0x66 }, {0,0}
};
static const interval<char16> range_s[] = {
	{0x9, 0xa},
	{0xc, 0xd},
	{0x20, 0x20}, {0,0}
};

const PosixGroup posix_groups[] = {
	{ "d",		make_range_n(range_word,	1) },
	{ "w",		make_range_n(range_word,	4) },
	{ "s",		make_range_n(range_s,		3) },
	{ "alnum", 	make_range_n(range_word,	3) },
	{ "alpha", 	make_range_n(range_word + 1,2) },
	{ "ascii", 	make_range_n(range_ascii,	1) },
	{ "blank", 	make_range_n(range_blank,	2) },
	{ "cntrl", 	make_range_n(range_cntrl,	2) },
	{ "digit", 	make_range_n(range_word,	1) },
	{ "graph", 	make_range_n(range_graph,	1) },
	{ "lower", 	make_range_n(range_word + 2,1) },
	{ "print", 	make_range_n(range_print,	1) },
	{ "punct", 	make_range_n(range_punct,	4) },
	{ "space", 	make_range_n(range_space,	2) },
	{ "upper", 	make_range_n(range_word + 1,1) },
	{ "word", 	make_range_n(range_word,	4) },
	{ "xdigit",	make_range_n(range_xdigit,	3) },
};

const PosixGroup* LookupPosixGroup(const count_string& name) {
	for (auto &i : posix_groups)
		if (name == i.name)
			return &i;
	return NULL;
}

const PosixGroup l_group = {"l", make_range_n(range_word + 2, 1)}, u_group = {"u", make_range_n(range_word + 1, 1)};

// Parses a character inside a character class
bool ParseCCCharacter(string_scan &s, char32 &rp) {
	if (s.remaining() == 0)
		return false;

	// Allow regular escape sequences even though many need not be escaped in this context
	if (s.peekc() == '\\')
		return ParseEscape(s, rp);

	// Otherwise take the next rune
	return s.get_utf8(rp);
}

// Parses a Unicode character group like {Han}
const unicode::Group *ParseUnicodeGroup(string_scan &s, bool &negate) {
	count_string	name;
	char32			c;

	if (!s.get_utf8(c))
		return nullptr;

	if (c == '{') {
		// Name is in braces - look for closing }
		const char* p	= s.getp();
		const char *end	= s.scan('}');
		if (!end)
			return nullptr;
		name = count_string(p, end);	// without '}'
		s.move(1);
	} else {
		name = count_string((char*)&c, 1);
	}
	if (name.begins("^")) {
		negate	= !negate;
		name	= name.slice(1);
	}
	return unicode::LookupGroup(name);
}

CharClassBuilder *ParseCharClass(string_scan &s, ParseFlags parse_flags) {
	CharClassBuilder	*ccb = new CharClassBuilder;

	bool	negated = s.peekc() == '^';
	if (negated)
		s.move(1);

	bool	first = true;	// ] is okay as first char in class
	bool	error = false;

	while (s.remaining() > 0 && (s.peekc() != ']' || first)) {
		// - is only okay unescaped as first or last in class, except that Perl allows - anywhere
		if (s.peekc() == '-' && !first && (s.remaining() == 1 || s.peekc(1) != ']')) {
			error = true;
			break;
		}
		first = false;

		// Look for [:alnum:] etc
		const char* p = s.getp();
		if (s.check("[:")) {
			if (const char* q = s.scan(":]")) {
				bool				negate	= p[2] == '^';
				count_string		name(p + 2 + int(negate), q);
				const PosixGroup	*group	= 0;

				for (auto &i : posix_groups) {
					if (name == i.name) {
						group = &i;
						break;
					}
				}
				if (group) {
					AddGroup(ccb, group, negate, parse_flags);
					s.move(2);
					continue;
				}
			}
			s.move(p - s.getp());
		}

		if (s.remaining() > 2 && s.peekc() == '\\') {
			switch (s.peekc(1)) {
				case 'd': AddGroup(ccb, &posix_groups[0], false, parse_flags); s.move(2); continue;
				case 'D': AddGroup(ccb, &posix_groups[0], true,  parse_flags); s.move(2); continue;
				case 'w': AddGroup(ccb, &posix_groups[1], false, parse_flags); s.move(2); continue;
				case 'W': AddGroup(ccb, &posix_groups[1], true,  parse_flags); s.move(2); continue;
				case 's': AddGroup(ccb, &posix_groups[2], false, parse_flags); s.move(2); continue;
				case 'S': AddGroup(ccb, &posix_groups[2], true,  parse_flags); s.move(2); continue;
				case 'l': AddGroup(ccb, &l_group, false, parse_flags); s.move(2); continue;
				case 'L': AddGroup(ccb, &l_group, true,  parse_flags); s.move(2); continue;
				case 'u': AddGroup(ccb, &u_group, false, parse_flags); s.move(2); continue;
				case 'U': AddGroup(ccb, &u_group, true,  parse_flags); s.move(2); continue;
				case 'p': case 'P': {
					bool	negate = s.peekc(1) == 'P';
					if (auto group = ParseUnicodeGroup(s.move(2), negate)) {
						AddGroup(ccb, group, negate, parse_flags);
						continue;
					}
					return nullptr;
				}
			}
		}

		// Otherwise assume single character or simple range
		CharRange rr;
		if (ParseCCCharacter(s, rr.lo)) {
			rr.hi = rr.lo;
			// [a-] means (a|-), so check for final ]
			if ((s.remaining() < 2 || s.peekc() != '-' || s.peekc(1) == ']') || (ParseCCCharacter(s.move(1), rr.hi) && rr.lo <= rr.hi)) {
				AddRange(ccb, rr.lo, rr.hi, parse_flags | kParseClassNL);
				continue;
			}
		}
		error = true;
		break;
	}
	if (error || s.remaining() == 0) {
		delete ccb;
		return nullptr;
	}

	s.move(1);	// ']'

	if (negated) {
		// If NL can't match implicitly, then pretend negated classes include a leading \n
		if (!(parse_flags & kParseClassNL) || (parse_flags & kParseNeverNL))
			ccb->AddRange('\n', '\n');
		ccb->Negate();
	}

	return ccb;
}

//(?adluimnsx-imnsx:pattern)
ParseFlags ParsePerlFlags(string_scan &s, ParseFlags flags) {
	bool		negated	= false;
	ParseFlags	nflags	= flags;

	for (;;) {
		if (s.peekc() == ':')
			return nflags;

		char32		c;
		if (!s.get_utf8(c))
			return kParseError;

		switch (c) {
			default:
				return kParseError;

			case '-':
				negated = true;
				break;

			case '^':	nflags = nflags & ~(kParseFoldCase | kParseOneLine | kParseNeverCapture | kParseDotNL); break;

			case 'i':	nflags = negated ? (nflags & ~kParseFoldCase)		: (nflags | kParseFoldCase);	break;
			case 'm':	nflags = negated ? (nflags & ~kParseOneLine)		: (nflags | kParseOneLine);		break;
			case 'n':	nflags = negated ? (nflags & ~kParseNeverCapture)	: (nflags | kParseNeverCapture);break;
			case 's':	nflags = negated ? (nflags & ~kParseDotNL)			: (nflags | kParseDotNL);		break;
			case 'U':	nflags = negated ? (nflags & ~kParseNonGreedy)		: (nflags | kParseNonGreedy);	break;

			case 'a':	nflags = (nflags & ~kParseEncoding) | kParseEncodingA; break;
			case 'd':	nflags = (nflags & ~kParseEncoding) | kParseEncodingD; break;
			case 'l':	nflags = (nflags & ~kParseEncoding) | kParseEncodingL; break;
			case 'u':	nflags = (nflags & ~kParseEncoding) | kParseEncodingU; break;

			case ')':
				return nflags;
		}
	}
}

template<typename C> Regexp* Regexp::Parse(string_scanT<C> &&t, ParseFlags parse_flags) {
	ParseState	ps(parse_flags);

	if (parse_flags & kParseLiteral) {
		// Special parse loop for literal string
		while (t.remaining() > 0) {
			char32 r;
			if (!t.get_utf8(r) || !ps.PushLiteral(r))
				return NULL;
		}
		return ps.DoFinish();
	}

	while (t.remaining() > 0) {
		switch (t.peekc()) {
			default: {
				char32 r;
				if (!t.get_utf8(r))
					return NULL;
				if (!ps.PushLiteral(r))
					return NULL;
				break;
			}

			case '(':
				t.move(1);
				if (t.remaining() > 0 && t.peekc() == '?') {
					// Check for named captures
					if (t.check("?<") || t.check("?P<") || t.check("?'")) {
						const char *begin	= t.getp();
						const char *end		= t.scan_skip(t.peekc(-1) == '\'' ? '\'' : '>');
						if (!end)
							return 0;

						for (auto i = begin; i != end; i++) {
							if (!is_wordchar(*i))
								return 0;
						}

						if (!ps.DoLeftParen(true))
							return NULL;
						ps.stacktop->name = string(begin, end).detach();
						break;

					} else if (t.check("?=")) {
						if (!ps.DoLeftParen(false))
							return NULL;
						ps.stacktop->match_id = 1;
						break;

					} else if (t.check("?!")) {
						if (!ps.DoLeftParen(false))
							return NULL;
						ps.stacktop->match_id = 2;
						break;
					}
					ParseFlags nflags = ParsePerlFlags(t.move(1), ps.flags);
					if (nflags == kParseError)
						return NULL;

					ps.flags = nflags;

					if (t.peekc() == ':') {
						if (!ps.DoLeftParen(false))
							return NULL;
						t.move(1);
					}

				} else {
					if (!ps.DoLeftParen(!(ps.flags & kParseNeverCapture)))
						return NULL;
				}
				break;

			case '|':
				if (!ps.DoVerticalBar())
					return NULL;
				t.move(1);
				break;

			case ')':
				if (!ps.DoRightParen())
					return NULL;
				t.move(1);
				break;

			case '^':	// Beginning of line
				if (!ps.PushEmptyOp(ps.flags & kParseOneLine ? kEmptyBeginText : kEmptyBeginLine))
					return NULL;
				t.move(1);
				break;

			case '$':	// End of line
				if (!ps.PushEmptyOp(ps.flags & kParseOneLine ? kEmptyEndText : kEmptyEndLine))
					return NULL;
				t.move(1);
				break;

			case '.':	// Any character (possibly except newline)
				if (!ps.PushDot())
					return NULL;
				t.move(1);
				break;

			case '[':	// Character class
				if (CharClassBuilder* ccb = ParseCharClass(t.move(1), ps.flags)) {
					if (ps.PushClass(ccb))
						break;
				}
				return NULL;

			case '*': {	// Zero or more
				int	min, max;
				min = 0;
				max = -1;
				goto Rep;
			case '+':	// One or more
				min = 1;
				max = -1;
				goto Rep;
			case '?':	// Zero or one
				min = 0;
				max = 1;
				goto Rep;
			Rep:
				bool nongreedy = t.remaining() > 1 && t.peekc(1) == '?';
				t.move(nongreedy ? 2 : 1);
				if (!ps.PushRepeat(min, max, nongreedy))
					return NULL;
				break;
			}

			case '{': {	// Counted repetition
				t.move(1);
				auto	p = t.getp();

				int lo, hi;
				if (t.get(lo)) {
					if (t.peekc() == ',') {
						t.move(1);
						if (t.peekc() == '}')
							hi = -1;
						else
							t.get(hi);
					} else {
						hi = lo;
					}
					if (t.peekc() == '}') {
						bool nongreedy = t.remaining() > 1 && t.peekc(1) == '?';
						t.move(nongreedy ? 2 : 1);
						if (!ps.PushRepeat(lo, hi, nongreedy))
							return NULL;
						break;
					}
				}

				// Treat like a literal
				t.move(p - t.getp());
				if (!ps.PushLiteral('{'))
					return NULL;
				break;
			}

			case '\\':	// Escaped character or special
				if (t.remaining() >= 2) {
					bool	ret;
					switch (t.peekc(1)) {
						case 'z':
						case '\'': t.move(2); ret = ps.PushEmptyOp(kEmptyBeginText); break;
						case 'A':
						case '`': t.move(2); ret = ps.PushEmptyOp(kEmptyEndText); break;
						case '<': t.move(2); ret = ps.PushEmptyOp(kEmptyBeginWord); break;
						case '>': t.move(2); ret = ps.PushEmptyOp(kEmptyEndWord); break;
						case 'b': t.move(2); ret = ps.PushEmptyOp(kEmptyWordBoundary); break;
						case 'B': t.move(2); ret = ps.PushEmptyOp(kEmptyNonWordBoundary); break;
						case 'd': t.move(2); ret = ps.PushGroup(&posix_groups[0], false); break;
						case 'D': t.move(2); ret = ps.PushGroup(&posix_groups[0], true); break;
						case 'w': t.move(2); ret = ps.PushGroup(&posix_groups[1], false); break;
						case 'W': t.move(2); ret = ps.PushGroup(&posix_groups[1], true); break;
						case 's': t.move(2); ret = ps.PushGroup(&posix_groups[2], false); break;
						case 'S': t.move(2); ret = ps.PushGroup(&posix_groups[2], true); break;
						case 'p': case 'P':
							ret = t.peekc(1) == 'P';
							if (auto g = ParseUnicodeGroup(t.move(2), ret)) {
								ret = ps.PushGroup(g, ret);
								break;
							}
							//fall through
						default: {
							char32 r;
							ret = ParseEscape(t, r) && ps.PushLiteral(r);
							break;
						}
					}
					if (ret)
						break;
				}
				return NULL;
		}
	}
	return ps.DoFinish();
}

//-----------------------------------------------------------------------------
//	debug
//-----------------------------------------------------------------------------

#ifdef REGEX_DIAGNOSTICS

string_accum &DumpRegexp(string_accum &sa, Regexp *re, int depth) {
	static const char *names[] = {
		"<bad>",
		"NoMatch",
		"EmptyMatch",
		"Literal",
		"LiteralString",
		"Concat",
		"Alternate",
		"Repeat",
		"Capture",
		"AnyChar",
		"CharClass",
		"HaveMatch",
	};

	sa << '\n' << iso::repeat("  ", depth) << names[re->op] << " {";

	switch (re->op) {
		case kRegexpEmptyMatch:		sa << "0x" << hex(re->empty_flags); break;
		case kRegexpLiteral:		sa << '\'' << char32(re->rune) << '\''; break;
		case kRegexpLiteralString:	sa << '"' << string((char32*)re->chars, re->nchars) << '"'; break;
		case kRegexpRepeat:			sa << onlyif(re->min, re->min) << " .. " << onlyif(re->max >= 0, re->max); break;
		case kRegexpCapture:		sa << re->cap; break;
		case kRegexpHaveMatch:		sa << re->match_id;
		case kRegexpCharClass:
			for (auto &i : *re->cc)
				sa << '\n' << iso::repeat("  ", depth + 1) << i.lo << " .. " << i.hi;
			sa << '\n' << iso::repeat(' ', depth * 2);
			break;
	}

	auto subs = re->Subs();
	if (!subs.empty()) {
		for (auto i : re->Subs())
			DumpRegexp(sa, i, depth + 1);
		sa << '\n' << iso::repeat(' ', depth * 2);
	}
	return sa << "}\n";
}

string_accum&	DumpProg(string_accum &sa, Prog *prog) {
	static const char *names[] = {
		"Alt",
		"AltMatch",
		"ByteRange",
		"Capture",
		"EmptyWidth",
		"Match",
		"Nop",
		"Fail",
	};

	dynamic_array<uint32>		stk;
	sparse_set<uint32, uint16>	reachable(prog->inst.size());

	bool	flat	= prog->flattened;
	int		depth	= 0;
	for (uint32	id = prog->start;;) {
		if (id == 0) {
			--depth;
			//if (depth < 0)
			//	return;
			sa << "\n      " << iso::repeat("  ", depth) << "}";

		} else if (!reachable.check_insert(id)) {
			auto	&ip		= prog->inst[id];
			(sa << '\n').format("%5i ", id) << iso::repeat("  ", depth) << names[ip.op] << " {";

			if (flat && !ip.last)
				stk.push_back(id + 1);

			stk.push_back(0);
			++depth;

			switch (ip.op) {
				case kInstAlt:
				case kInstAltMatch:
					if (ip.out1)
						stk.push_back(ip.out1);
					break;

				case kInstByteRange:
					if (ip.extra == 15) {
						sa << " 0x" << hex(ip.lobank << 5) << " .. 0x" << hex((ip.hibank << 5) | 31);
					} else {
						int		bank	= ip.extra & 7;
						if (bank && bank < 4) {
							sa << " \"";
							for (uint32 m = ip.mask; m; m = clear_lowest(m))
								sa << char(bank * 32 + lowest_set_index(m));
							sa << '\"' << onlyif(ip.extra & 8, 'i');
						} else {
							for (uint32 m = ip.mask; m; ) {
								int	lo = lowest_set_index(m), hi = lowest_clear_index(m | (lowest_set(m) - 1));
								if (hi < 0)
									hi = 32;

								sa << onlyif(m != ip.mask, ',') << " 0x" << hex(bank * 32 + lo);
								if (hi - lo > 1)
									sa << " .. 0x" << hex(bank * 32 + hi - 1);

								m &= ~bits(hi);
							}
						}
					}
					break;

				case kInstCapture:		sa << ip.cap; break;
				case kInstEmptyWidth:	sa << "0x" << hex(ip.empty); break;
				case kInstMatch:		sa << ip.match_id; break;
					break;
			}

			if (id = ip.out)
				continue;

		} else {
			sa << "\n      " << iso::repeat("  ", depth) << "goto " << id;
		}

		if (stk.empty())
			break;

		id = stk.pop_back_value();
	}
	return sa << '\n';
}

string_accum&	DumpByteMap(string_accum &sa, uint8 bytemap[256]) {
	for (int c = 0; c < 256; c++) {
		int b = bytemap[c];
		int lo = c;
		while (c < 256 - 1 && bytemap[c + 1] == b)
			c++;
		int hi = c;
		sa.format("[%02x-%02x] -> %d\n", lo, hi, b);
	}
	return sa;
}

string_accum&	DumpOnePass(string_accum &sa, const_memory_block states, int bytemap_entries) {
	uint32		state_size	= uint32(sizeof(OnePass::State) + bytemap_entries * sizeof(uint32));
	int			node_index	= 0;
	for (const uint8 *p = states; p < states.end(); p += state_size, ++node_index) {
		const OnePass::State	*node = (const OnePass::State*)p;
		sa.format("node %d: match_cond=%#x\n", node_index, node->match_cond);
		for (int i = 0; i < bytemap_entries; i++) {
			uint32	action	= node->action[i];
			if ((action & kEmptyNever) == kEmptyNever)
				continue;
			sa.format("  %d cond %#x -> %d\n", i, node->action[i] & 0xFFFF, OnePass::get_bits(action, OnePass::mask_index));
		}
	}
	return sa;
}

string_accum&	DumpDFAState(string_accum &sa, DFA::State* state, int nnext) {
	if (state == NULL)
		return sa << '_';
	if (state == DeadState)
		return sa << 'X';
	if (state == FullMatchState)
		return sa << '*';

	const char* sep = "";
	sa.format("(%p)", state);
	for (auto i : state->insts()) {
		if (i == DFA::State::Mark) {
			sa << '|';
			sep = "";
		} else if (i == DFA::State::MatchSep) {
			sa << "||";
			sep = "";
		} else {
			sa.format("%s%d", sep, i);
			sep = ",";
		}
	}
	sa.format(" flag=%#x", state->flag);

	if (nnext) {
		sa << '\n';
		for (int i = 0; i < nnext; i++)
			sa.format("  %d -> %p\n", i, state->next(i));
	}

	return sa;
}

string_accum&	DumpDFA(string_accum &sa, DFA *dfa) {
	for (auto &s : dfa->state_cache)
		DumpDFAState(sa, s, dfa->prog->bytemap_entries + 1);
	return sa;
}

#endif

//-----------------------------------------------------------------------------
//	regex
//-----------------------------------------------------------------------------

static EmptyOp get_enable(match_flags flags) {
	return (
			flags & match_prev_avail ? kEmptyBeginLine|kEmptyBeginWord : (
				kEmptyBeginText
			|	(flags & match_not_bol ? kEmptyNone	: kEmptyBeginLine)
			|	(flags & match_not_bow ? kEmptyNone	: kEmptyBeginWord)
			)
		)
		| (flags & match_not_eol	? kEmptyNone	: kEmptyEndLine)
		| (flags & match_not_eow	? kEmptyNone	: kEmptyEndWord)
		| (flags & (match_not_eow|match_not_eol)	? kEmptyNone	: kEmptyEndText)
		| (flags & match_continuous	? kEmptyAnchor	: kEmptyNone);
}

static ParseFlags get_parse_flags(syntax_flags syntax) {
	return	(syntax & icase		? kParseFoldCase		: kParseNone)
		|	(syntax & nosubs	? kParseNeverCapture	: kParseNone)
		|	(syntax & oneline	? kParseOneLine			: kParseNone)
		|	(syntax & nongreedy	? kParseNonGreedy		: kParseNone)
		;
}

static void do_replace(string_builder &b, const count_string &text, const char **matches, uint32 nmatch, const char *repl) {
	while (const char *dollar = string_find(repl, '$')) {
		b.merge(repl, dollar - repl);
		repl = dollar + 1;
		switch (*repl) {
			case '1':case '2':case '3':case '4':case '5':case '6':case '7':case '8':case '9': {
				int		i		= from_digit(repl[0]);
				bool	dig2	= is_digit(repl[1]);
				if (dig2)
					i = i * 10 + from_digit(repl[1]);

				if (i < nmatch) {
					b.merge(matches[i * 2], matches[i * 2 + 1]);
					repl += 1 + int(dig2);
					continue;
				}
				break;
			}
			case '&':	b.merge(matches[0],		matches[1]); ++repl; continue;
			case '`':	b.merge(text.begin(),	matches[0]); ++repl; continue;
			case '\'':	b.merge(matches[1],		text.end()); ++repl; continue;
			case '$':	++repl;
			default:	break;
		}
		b.putc('$');
	}
	b << repl;
}

Regexp *regex::init(const char *begin, const char *end, syntax_flags syntax) {
	return Regexp::Parse(string_scan(begin, end), get_parse_flags(syntax));
}
Regexp *regex::init(const char16 *begin, const char16 *end, syntax_flags syntax) {
//	return Regexp::Parse(str(begin, end), get_parse_flags(syntax));
	return 0;
}
Regexp *regex::literal(range<const char*> chars) {
	return Regexp::LiteralString(chars, kParseNone);
}
Regexp *regex::empty(empty_flags e) {
	auto	re = new Regexp(kRegexpEmptyMatch, kParseNone);
	re->empty_flags = (EmptyOp)e;
	return re;
}

regex::regex(const char_set &set, syntax_flags syntax) : prog(0) {
	auto	flags = get_parse_flags(syntax);

	switch (set.count_set()) {
		case 1:
			// A character class of one character is just a literal
			re = new Regexp(kRegexpLiteral, flags);
			re->rune = set.next(0, true);
			break;
		case 2: {
			// [Aa] can be rewritten as a literal A with ASCII case folding
			uint32 r = set.next(0, true);
			if (is_upper(r) && set.test(to_lower(r))) {
				re = new Regexp(kRegexpLiteral, flags | kParseFoldCase);
				re->rune = to_lower(r);
				break;
			}
		}
			//fall through
		default:
			re		= new Regexp(kRegexpCharClass, flags & ~kParseFoldCase);
			re->cc	= new CharClass(set);
			break;
	}
}

regex::~regex() {
	if (re)
		re->release();
	delete prog;
}

bool regex::check_prog(match_flags flags) const {
	return prog || (prog = Compiler::Compile(re, (flags & _encoding) == encoding_utf8 ? Compiler::utf8 : Compiler::ascii));
}

bool regex::match(const count_string& text, match_flags flags) const {
	const char *matches[2];
	return check_prog(flags) && prog->Search(matches, 1, text, get_enable(flags) | kEmptyAnchor | kEmptyLongest) && matches[1] == text.end();
}

bool regex::search(const count_string& text, match_flags flags) const {
	const char *matches[2];
	return check_prog(flags) && prog->Search(matches, 1, text, get_enable(flags));
}

bool regex::match(const count_string& text, dynamic_array<count_string> &matches, match_flags flags) const {
	if (check_prog(flags)) {
		uint32	nmatch		= prog->NumCaptures();
		auto	matches2	= new_auto(const char*, nmatch * 2);
		if (prog->Search(matches2, nmatch, text, get_enable(flags) | kEmptyAnchor | kEmptyLongest)) {
			matches.resize(nmatch);
			const char **p = matches2;
			for (auto &i : matches) {
				i = count_string(p[0], p[1]);
				p += 2;
			}
			return true;
		}
	}
	return false;
}

bool regex::search(const count_string& text, dynamic_array<count_string> &matches, match_flags flags) const {
	if (check_prog(flags)) {
		uint32	nmatch		= prog->NumCaptures();
		auto	matches2	= new_auto(const char*, nmatch * 2);
		if (prog->Search(matches2, nmatch, text, get_enable(flags))) {
			matches.resize(nmatch);
			const char **p = matches2;
			for (auto &i : matches) {
				i = count_string(p[0], p[1]);
				p += 2;
			}
			return true;
		}
	}
	return false;
}

string regex::replace(const count_string& text, const char *repl, match_flags flags) const {
	if (!check_prog(flags))
		return nullptr;

	string_builder	b;
	auto			section		= text;
	uint32			nmatch		= prog->NumCaptures();
	auto			matches		= new_auto(const char*, nmatch * 2);
	EmptyOp			enable		= get_enable(flags);

	for (NFA nfa(prog, matches, nmatch); nfa.Search(section, enable); ) {
		if (!(flags & format_no_copy))
			b.merge(section.begin(), matches[0]);

		do_replace(b, section, matches, nmatch, repl);

		section = section.slice(matches[1]);
		if (flags & format_first_only)
			break;

		enable = enable - kEmptyBeginText;
	}

	if (!(flags & format_no_copy))
		b << section;

	return b;
}


regex operator+(const regex &a, const regex &b)	{
	auto	ra	= a.re, rb = b.re;
	int		n	= (ra->op == kRegexpConcat ? ra->Subs().size32() : 1)
				+ (rb->op == kRegexpConcat ? rb->Subs().size32() : 1);

	Regexp** subs = new Regexp*[n], **psubs = subs;

	if (ra->op == kRegexpConcat) {
		for (auto i : ra->Subs())
			*psubs++= i->addref();
	} else {
		*psubs++ = ra->addref();
	}
	if (rb->op == kRegexpConcat) {
		for (auto i : rb->Subs())
			*psubs++= i->addref();
	} else {
		*psubs++ = rb->addref();
	}

	Regexp* re	= new Regexp(kRegexpConcat, kParseNone);
	re->subs	= subs;
	re->nsub	= n;
	return re;
}

regex operator|(const regex &a, const regex &b)	{
	auto	ra	= a.re, rb = b.re;
	int		n	= (ra->op == kRegexpAlternate ? ra->Subs().size32() : 1)
				+ (rb->op == kRegexpAlternate ? rb->Subs().size32() : 1);

	Regexp** subs = new Regexp*[n], **psubs = subs;

	if (ra->op == kRegexpAlternate) {
		for (auto i : ra->Subs())
			*psubs++= i->addref();
	} else {
		*psubs++ = ra->addref();
	}
	if (rb->op == kRegexpAlternate) {
		for (auto i : ra->Subs())
			*psubs++= i->addref();
	} else {
		*psubs++ = rb->addref();
	}

	n = Regexp::FactorAlternation(make_range_n(subs, n), kParseNone);
	Regexp* re;

	if (n == 1) {
		re = subs[0];
		delete[] subs;
	} else {
		re	= new Regexp(kRegexpAlternate, kParseNone);
		re->subs	= subs;
		re->nsub	= n;
	}

	return re;
}

regex operator*(const regex &a)			{ return Regexp::Repeat(a.re->addref(), kParseNone, 0, -1); }
regex operator+(const regex &a)			{ return Regexp::Repeat(a.re->addref(), kParseNone, 1, -1); }
regex operator*(const regex &a, int n)	{ return Regexp::Repeat(a.re->addref(), kParseNone, n, n); }
regex between(const regex &a, int min, int max)	{ return Regexp::Repeat(a.re->addref(), kParseNone, min, max); }
regex maybe(const regex &a)				{ return Regexp::Repeat(a.re->addref(), kParseNone, 0, 1); }


//-----------------------------------------------------------------------------
//	regex_set
//-----------------------------------------------------------------------------

bool regex_set::check_prog(match_flags flags) const {
	if (!prog) {
		//sort(elem);	// sort the elements
		for (auto &i : res)
			i->addref();
		Regexp* re	= Regexp::Alternate(unconst(res), get_parse_flags(syntax));
		prog		= Compiler::CompileSet(re, (flags & _encoding) == encoding_utf8 ? Compiler::utf8 : Compiler::ascii, false, false);
		re->release();
	}
	return !!prog;
}

regex_set::~regex_set() {
	for (auto &i : res)
		i->release();
	delete prog;
}

int32 regex_set::add(Regexp *re, int32 match_id) {
	ISO_ASSERT(!prog);
	if (!re)
		return 0;

	auto	parse_flags	= get_parse_flags(syntax);

	Regexp* m = new Regexp(kRegexpHaveMatch, parse_flags);
	m->match_id = match_id;

	if (re->op == kRegexpConcat) {
		int			nsub	= re->nsub;
		Regexp**	sub		= new Regexp*[nsub + 1];
		for (int i = 0; i < nsub; i++)
			sub[i] = re->subs[i]->addref();
		sub[nsub] = m;
		re->release();
		re = Regexp::ConcatQuick(make_range_n(sub, nsub + 1), parse_flags);
	} else {
		re = Regexp::Concat2(re, m, parse_flags);
	}
	res.push_back(re);

	return match_id;
}

int32 regex_set::add(const string_param& pattern, int32 match_id) {
	return add(Regexp::Parse(string_scan(pattern), get_parse_flags(syntax)), match_id);
}
int32 regex_set::add(const string_param& pattern) {
	return add(Regexp::Parse(string_scan(pattern), get_parse_flags(syntax)));
}

int32 regex_set::match(const count_string& text, match_flags flags) const {
	const char *matches[2];
	if (check_prog(flags)) {
		if (int32 matched = prog->Search(matches, 1, text, get_enable(flags) | kEmptyAnchor | kEmptyLongest)) {
			if (matches[1] == text.end())
				return matched;
		}
	}
	return 0;
}

int32 regex_set::search(const count_string& text, match_flags flags) const {
	return check_prog(flags) ? prog->Search(0, 0, text, get_enable(flags)) : 0;
}

int32 regex_set::match(const count_string& text, dynamic_array<count_string> &matches, match_flags flags) const {
	if (check_prog(flags)) {
		uint32	nmatch		= prog->NumCaptures();
		auto	matches2	= new_auto(const char*, nmatch * 2);
		if (int32 matched = prog->Search(matches2, nmatch, text, get_enable(flags) | kEmptyAnchor | kEmptyLongest)) {
			if (matches2[1] == text.end()) {
				matches.resize(nmatch);
				const char **p = matches2;
				for (auto &i : matches) {
					i = count_string(p[0], p[1]);
					p += 2;
				}
				return matched;
			}
		}
	}
	return 0;
}
int32 regex_set::search(const count_string& text, dynamic_array<count_string> &matches, match_flags flags) const {
	if (check_prog(flags)) {
		uint32	nmatch		= prog->NumCaptures();
		auto	matches2	= new_auto(const char*, nmatch * 2);
		if (int32 matched = prog->Search(matches2, nmatch, text, get_enable(flags))) {
			matches.resize(nmatch);
			const char **p = matches2;
			for (auto &i : matches) {
				i = count_string(p[0], p[1]);
				p += 2;
			}
			return matched;
		}
	}
	return 0;
}

string regex_set::replace(const count_string& text, const range<const char**> &repl, match_flags flags) const {
	if (!check_prog(flags))
		return nullptr;

	string_builder	b;
	auto			section		= text;
	uint32			nmatch		= prog->NumCaptures();
	auto			matches		= new_auto(const char*, nmatch * 2);
	EmptyOp			enable		= get_enable(flags);
	int32			match;

	for (NFA nfa(prog, matches, nmatch); match = nfa.Search(section, enable); ) {
		if (!(flags & format_no_copy))
			b.merge(section.begin(), matches[0]);

		do_replace(b, section, matches, nmatch, repl[match - 1]);

		section = section.slice(matches[1]);
		if (flags & format_first_only)
			break;

		enable = enable - kEmptyBeginText;
	}

	if (!(flags & format_no_copy))
		b << section;

	return b;
}

} } // namespace iso::re2
