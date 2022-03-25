#ifndef LZMA_H
#define LZMA_H

#include "codec.h"
#include "window.h"
#include "base/array.h"
#include "crc32.h"
#include "utilities.h"

namespace lzma {
using namespace iso;

//-----------------------------------------------------------------------------
// prob_decoder
//-----------------------------------------------------------------------------

template<int PN, typename T, typename S> struct prob_decoder_base {
	typedef uint_bits_t<PN>	prob_t;
	enum {
		top		= 1 << ((sizeof(T) - 1) * 8),
		half	= 1 << (PN - 1),
		move	= sizeof(prob_t) * 8 - PN,
	};

	S	file;
	T	range,	code;

	void	init() {
		while (range < top) {
			range <<= 8;
			code = (code << 8) | file.getc();
		}
	}
	void	normalise() {
		if (range < top) {
			range <<= 8;
			code = (code << 8) | file.getc();
		}
	}
	bool	_bit(prob_t t) {
		normalise();
		T	bound = (range >> PN) * t;
		if (code < bound) {
			range	= bound;
			return false;
		} else {
			range	-= bound;
			code	-= bound;
			return true;
		}
	}
	bool	bit_half() {
		normalise();
		range >>= 1;
		if (code < range) {
			return false;
		} else {
			code -= range;
			return true;
		}
	}
	template<typename S2> prob_decoder_base(S2 &&file, T range, T code) : file(forward<S2>(file)), range(range), code(code) {}
};

template<int PN, typename T, typename S, bool CHECK> struct prob_decoder_base2;

template<int PN, typename T, typename S> struct prob_decoder_base2<PN, T, S, false> : prob_decoder_base<PN, T, S> {
	typedef	prob_decoder_base<PN, T, S>	B;
	using typename B::prob_t;
	using B::B;

	bool	bit(prob_t *p) {
		static const int NumMoveBits = sizeof(prob_t) * 8 - PN;
		auto	t = *p;
		if (B::_bit(t)) {
			*p	= prob_t(t - (t >> NumMoveBits));
			return true;
		} else {
			*p	= prob_t(t + (((1 << PN) - t) >> NumMoveBits));
			return false;
		}
	}
};

template<int PN, typename T, typename S> struct prob_decoder_base2<PN, T, S, true> : prob_decoder_base<PN, T, S> {
	typedef	prob_decoder_base<PN, T, S>	B;
	typedef const typename B::prob_t	prob_t;
	using B::B;

	bool	bit(prob_t *p) {
		return B::_bit(*p);
	}
};

template<int PN, typename T, typename S, bool CHECK> struct prob_decoder : prob_decoder_base2<PN, T, S, CHECK> {
	typedef	prob_decoder_base2<PN, T, S, CHECK> B;
	using typename B::prob_t;
	using B::B;

	T	tree(prob_t *probs, int n) {
		T	limit	= T(1) << n;
		T	i		= 1;
		do
			i = (i << 1) | B::bit(probs + i);
		while (i < limit);
		return i - limit;
	}

	template<int N> T	_tree(prob_t *probs, T i)		{ return _tree<N - 1>(probs, (i << 1) | B::bit(probs + i)); }
	template<>		T	_tree<1>(prob_t *probs, T i)	{ return (i << 1) | B::bit(probs + i); }
	template<int N> T	tree(prob_t *probs)				{ return _tree<N>(probs, 1) - (1 << N); }

	T	tree_reverse(prob_t *probs, int n) {
		T	i = 1, m = 1;
		while (n--) {
			if (B::bit(probs + i)) {
				m <<= 1;
				i += m;
			} else {
				i += m;
				m <<= 1;
			}
		}
		return i - m;
	}

	template<int N> T	_tree_reverse(prob_t *probs, T i, T m)		{ return _tree_reverse<N - 1>(probs, i + (m << B::bit(probs + i)), m << 1); }
	template<>		T	_tree_reverse<1>(prob_t *probs, T i, T m)	{ return i + (m << B::bit(probs + i)); }
	template<int N> T	tree_reverse(prob_t *probs)					{ return _tree_reverse<N>(probs, 1, 1) - (1 << N); }

	T	tree_matched(prob_t* probs, uint32 n, T match) {
		T		offs	= 1 << n;
		T		sym		= 1;
		while (n--) {
			match	<<= 1;
			T	bit = offs;
			offs	&= match;
			bool b	= B::bit(probs + offs + bit + sym);
			sym		= (sym << 1) + b;
			if (!b)
				offs ^= bit;
		}
		return sym;
	}
};

//-----------------------------------------------------------------------------
// prob_encoder
//-----------------------------------------------------------------------------

template<int PN, typename T, typename S> struct prob_encoder {
	typedef uint_bits_t<PN>	prob_t;
	enum {
		top		= 1 << ((sizeof(T) - 1) * 8),
		half	= 1 << (PN - 1),
		move	= sizeof(prob_t) * 8 - PN,
	};

	S				file;
	T				range;
	T				cache;
	uint32			cacheSize;
	uint64			low;

	template<typename S2> prob_encoder(S2 &&file) : file(forward<S2>(file)) {
		range		= 0xFFFFFFFF;
		cache		= 0;
		low			= 0;
		cacheSize	= 0;
	}

	auto	tell()		{
		return file.tell() + cacheSize;
	}

	void	shift() {
		uint32	low1	= (uint32)low;
		uint32	high1	= low >> 32;
		low	= low1 << 8;
		if (low1 < 0xFF000000u || high1 != 0) {
			file.putc(cache + high1);
			cache	= low1 >> 24;
			high1	+= 0xFF;
			while (cacheSize) {
				file.putc(high1);
				--cacheSize;
			}
		} else {
			++cacheSize;
		}
	}

	void	flush() {
		for (int i = 0; i < 5; i++)
			shift();
	}

	void	normalise() {
		if (range < top) {
			range <<= 8;
			shift();
		}
	}

	void	bit_half(bool bit) {
		range	>>= 1;
		if (bit)
			low	+= range;
		normalise();
	}

	bool	bit(prob_t *p, bool bit) {
		prob_t	t		= *p;
		uint32	bound	= (range >> PN) * t;

		if (bit) {
			low		+= bound;
			range	-= bound;
			*p		= t - (t >> move);
		} else {
			range	= bound;
			*p		= t + (((1 << PN) - t) >> move);
		}
		normalise();
		return bit;
	}

	void	bit0(prob_t* p) {
		prob_t	t	= *p;
		range		= (range >> PN) * t;;
		*p			= t + (((1 << PN) - t) >> move);
		normalise();
	}
	void	bit1(prob_t* p) {
		prob_t	t		= *p;
		uint32	bound	= (range >> PN) * t;
		low		+= bound;
		range	-= bound;
		*p		= t - (t >> move);
		normalise();
	}

	void tree(prob_t* probs, uint32 n, uint32 sym) {
		sym = (sym << (8 - n)) | 0x100;
		while (n--) {
			bit(probs + (sym >> 8), (sym >> 7) & 1);
			sym <<= 1;
		}
	}
	void tree_reverse(prob_t* probs, uint32 n, uint32 sym) {
		T m = 1;
		while (n--) {
			uint32 b = sym & 1;
			bit(probs + m, b);
			sym >>= 1;
			m = (m << 1) | b;
		}
	}
	void tree_matched(prob_t* probs, uint32 n, uint32 sym, uint32 match) {
		T	offs	= 1 << n;
		T	test	= offs >> 1;
		sym				|= offs;
		for (uint32 i = n; i--;) {
			match	<<= 1;
			bit(probs + offs + (match & offs) + (sym >> n), sym & test);
			sym		<<= 1;
			offs	&= ~(match ^ sym);
		}
	}
};

//-----------------------------------------------------------------------------
// range coder cost
//-----------------------------------------------------------------------------

template<typename T, int PN, int MR, int C> struct cost_table {
	typedef uint_bits_t<PN>	prob_t;
	static const T infinity = 1 << 30;

	T	prices[1 << (PN - MR)];

	cost_table();
	T	price(prob_t prob, bool bit)	const	{ return prices[(prob ^ uint32(-int(bit) & ((1 << PN) - 1))) >> MR]; }
	T	price1(prob_t prob)				const 	{ return prices[(prob ^ ((1 << PN) - 1)) >> MR]; }
	T	price0(prob_t prob)				const	{ return prices[prob >> MR]; }

	T	tree(const prob_t* probs, uint32 n, uint32 sym) const {
		T	res		= 0;
		sym			|= 1 << n;
		while (n--) {
			uint32 bit = sym & 1;
			sym >>= 1;
			res	+= price(probs[sym], bit);
		};
		return res;
	}

	T	tree_matched(const prob_t* probs, uint32 n, uint32 sym, uint32 match) const {
		T	res		= 0;
		T	offs	= 1 << n;
		T	test	= offs >> 1;
		sym			|= offs;
		for (uint32 i = n; i--;) {
			match	<<= 1;
			res		+= price(probs[offs + (match & offs) + (sym >> n)], sym & test);
			sym		<<= 1;
			offs	&= ~(match ^ sym);
		}
		return res;
	}

	void	tree(const prob_t* probs, uint32 n, uint32 sym, T &res0, T &res1) const {
		sym |= 1 << n;
		auto	prob	= probs[sym];
		T		res		= 0;
		while (n--) {// while (sym)?
			uint32	b	= sym & 1;
			sym		>>= 1;
			res	+= price(probs[sym], b);
		}
		res0	= res + price0(prob);
		res1	= res + price1(prob);
	}
	void	tree_fill(T *prices, const prob_t* probs, uint32 n) const {
		--n;
		for (uint32 i = 0, total = 1 << n; i < total; i++)
			tree(probs, n, i, prices[i * 2 + 0], prices[i * 2 + 1]);
	}
	void	fill_tree(T *prices, const prob_t* probs, uint32 n, T base_price) const {
		--n;
		T	res0	= base_price + price0(probs[1 << n]);
		T	res1	= base_price + price1(probs[1 << n]);
		if (n == 0) {
			prices[0]	= res0;
			prices[1]	= res1;
		} else {
			fill_tree(prices, probs, n, res0);
			fill_tree(prices + (1 << n), probs + (1 << n), n, res1);
		}
	}
	void	tree_fill2(T *prices, const prob_t* probs, uint32 n) {
		fill_tree(prices, probs, n, 0);
	}

	void	tree_reverse(const prob_t* probs, uint32 n, uint32 sym, T &res0, T &res1) const {
		T		res	= 0;
		uint32	m	= 1;
		while (n--) {
			uint32 b = sym & 1;
			res += price(probs[m], b);
			sym >>= 1;
			m = (m << 1) | b;
		}

		auto	prob	= probs[m];
		res0	= res + price0(prob);
		res1	= res + price1(prob);
	}
	void	fill_tree_reverse(T *prices, const prob_t* probs, uint32 n) const {
		--n;
		for (uint32 i = 0, total = 1 << n; i < total; i++)
			tree_reverse(probs, n, i, prices[i], prices[i + total]);
	}
	void	_fill_tree_reverse(T *prices, const prob_t* probs, uint32 n, uint32 offset, T base_price) const {
		T	res0	= base_price + price0(probs[1]);
		T	res1	= base_price + price1(probs[1]);
		if (n == 1) {
			prices[0]		= res0;
			prices[offset]	= res1;
		} else {
			--n;
			offset >>= 1;
			_fill_tree_reverse(prices + offset * 2, probs + (2 << (n - 2)), n, offset, res0);
			_fill_tree_reverse(prices + offset * 3, probs + (3 << (n - 2)), n, offset, res1);
		}
	}
	void	fill_tree_reverse2(T *prices, const prob_t* probs, uint32 n) const {
		_fill_tree_reverse(prices, probs, n, 1 << n, 0);
	}
};

//-----------------------------------------------------------------------------
// LZMA State
//-----------------------------------------------------------------------------

struct State {
	enum STATES {//			on lit			on match	on rep		on shortrep
		LIT,				//LIT			MATCH		REP			SHORTREP
		MATCH_LIT_LIT,		//LIT			MATCH		REP			SHORTREP
		REP_LIT_LIT,		//LIT			MATCH		REP			SHORTREP
		SHORTREP_LIT_LIT,	//LIT			MATCH		REP			SHORTREP
		MATCH_LIT,			//MATCH_LIT_LIT	MATCH		REP			SHORTREP
		REP_LIT,			//REP1_LIT_LIT	MATCH		REP			SHORTREP
		SHORTREP_LIT,		//REP0_LIT_LIT	MATCH		REP			SHORTREP
		MATCH,				//MATCH_LIT		MATCH2		REP2		REP2
		REP,				//REP1_LIT		MATCH2		REP2		REP2
		SHORTREP,			//REP0_LIT		MATCH2		REP2		REP2
		MATCH2,				//MATCH_LIT		MATCH2		REP2		REP2
		REP2,				//REP1_LIT		MATCH2		REP2		REP2
		NUM_STATES,
		NUM_STATES2			= 16,
		LIT_STATES			= MATCH,
	};

	enum {
		ProbBits			= 11,
		NUM_REPS			= 4,

		//char
		CharBitsMax			= 8,

		//pos
		PosBitsMax			= 4,
		PosStatesMax		= 1 << PosBitsMax,
		LenToPosStates		= 4,
		PosSlotBits			= 6,
		AlignBits			= 4,

		StartPosModelIndex	= 4,
		EndPosModelIndex	= 14,
		NumFullDistances	= 1 << (EndPosModelIndex >> 1),

		//length
		LenLowBits			= 3,
		LenLowSymbols		= 1 << LenLowBits,
		LenHighBits			= 8,
		LenHighSymbols		= 1 << LenHighBits,
		LenSymbolsTotal		= LenLowSymbols * 2 + LenHighSymbols,
		LenMin				= 2,
		LenMax				= LenMin + LenSymbolsTotal,

		// LZMA2
		CharPosBitsMax		= 4,
	};

	// ofsets in probs table
	enum PROB : int {
		LenLow				= 0,
		LenMid				= LenLow + LenLowSymbols,
		LenHigh				= LenLow + 2 * (PosStatesMax << LenLowBits),
		LenTotal			= LenHigh + LenHighSymbols,

		ProbOffset			= 1664,
		SpecPos				= -ProbOffset,
		IsRep0Long			= SpecPos		+ NumFullDistances,
		RepLenCoder			= IsRep0Long	+ (NUM_STATES2 << PosBitsMax),
		LenCoder			= RepLenCoder	+ LenTotal,
		IsMatch				= LenCoder		+ LenTotal,
		PosAlign			= IsMatch		+ (NUM_STATES2 << PosBitsMax),
		IsRep				= PosAlign		+ (1 << AlignBits),
		IsRepGT0			= IsRep			+ NUM_STATES,
		IsRepGT1			= IsRepGT0		+ NUM_STATES,
		IsRepGT2			= IsRepGT1		+ NUM_STATES,
		PosSlot				= IsRepGT2		+ NUM_STATES,
		Literal				= PosSlot		+ (LenToPosStates << PosSlotBits),
	};

	struct Props {
		uint8	lc, lp, pb;
		Props() : lc(3), lp(0), pb(2) {}
		Props(uint8 lc, uint8 lp, uint8 pb) : lc(lc), lp(lp), pb(pb) {}
		Props(uint8 b) {
			lc	= b % (CharBitsMax + 1);
			b	/= (CharBitsMax + 1);
			lp	= b % (PosBitsMax + 1);
			pb	= b / (PosBitsMax + 1);
		}
		uint32		GetNumProbs()	const	{ return Literal + ProbOffset + (0x300 << (lc + lp)); }
		uint32		lp_mask()		const	{ return (0x100 << lp) - (0x100 >> lc); }
		uint32		pb_mask()		const	{ return bits(pb); }
		uint8		encode()		const	{ return uint8((pb * (PosBitsMax + 1) + lp) * (CharBitsMax + 1) + lc); }
	};
	
	typedef uint_bits_t<ProbBits>	prob_t;

	struct LiteralHelper {
		uint32			lp_mask, lc;
		LiteralHelper(Props props) : lp_mask(props.lp_mask()), lc(props.lc) {}
		auto	operator()(uint32 pos, uint8 prev) const { return ((((pos << 8) + prev) & lp_mask) << lc) * 3; }
	};

	static constexpr uint32	GetLenToPosState(uint32 len)		{ return min(len - 2, LenToPosStates - 1); }
	static	uint32			GetPosSlot(uint32 pos)				{ auto i = highest_set_index(pos); return (i + i) + ((pos >> (i - 1)) & 1); }
	static constexpr int	PosState(uint32 state, uint32 pos)	{ return pos * NUM_STATES2 + state; }

	static constexpr bool	IsLitState(uint32 s)				{ return s < LIT_STATES; }
	static constexpr uint32 NextLitState(uint32 s)				{ return s < MATCH_LIT ? LIT : s < MATCH2 ? s - 3 : s - 6; }
	static constexpr uint32 NextMatchState(uint32 s)			{ return s < LIT_STATES ? MATCH : MATCH2; }
	static constexpr uint32 NextRepState(uint32 s)				{ return s < LIT_STATES ? REP : REP2; }
	static constexpr uint32 NextShortRepState(uint32 s)			{ return s < LIT_STATES ? SHORTREP : REP2; }

	dynamic_array<prob_t>	probability;
	uint32					state;
	uint32					reps[NUM_REPS];

	void	Reset() {
		for (auto &i : probability)
			i = 1 << (ProbBits - 1);
		reps[0]	= reps[1] = reps[2] = reps[3] = 1;
		state	= 0;
	}
};

//-----------------------------------------------------------------------------
// LZMA decoder
//-----------------------------------------------------------------------------

struct Decoder : State {
	enum {
		INIT_SIZE		= 5,
	};
	enum DUMMY {
		DUMMY_ERROR, // unexpected end of input stream
		DUMMY_LIT,
		DUMMY_MATCH,
		DUMMY_REP
	};

	typedef prob_decoder<11, uint32, byte_reader, false> prob_decoder1;

	Props			prop;
	size_t			processed;
	uint32			range, code;	// saved state of prob_decoder

	uint32			remain_len;
	uint32			temp_len;
	uint8			temp[20];	// number of required input bytes for worst case: bits = log2((2^11 / 31) ^ 22) + 26 < 134 + 26 = 160

	const uint8*	process1(uint8 *&dst, uint8 *dst_end, const uint8 *src, const uint8 *src_end, prefix_window win);
	DUMMY			Try(const uint8* src, const uint8* src_end, uint8 *dst, prefix_window win) const;


	void	SetProps(Props propNew) {
		probability.resize(propNew.GetNumProbs());
		prop = propNew;
		State::Reset();
	}
	
	void	Init(bool initDic) {
		remain_len	= LenMax + 1;		//Flush marker
		temp_len = 0;
		if (initDic)
			processed	= 0;
	}

	const uint8*	process(uint8 *&dst, uint8 *dst_end, const uint8 *src, const uint8 *src_end, prefix_window win);
	const uint8*	process(uint8 *&dst, uint8 *dst_end, const uint8 *src, const uint8 *src_end, TRANSCODE_FLAGS flags);

	Decoder(Props props = Props()) {
		SetProps(props);
		Init(true);
	}
};

//-----------------------------------------------------------------------------
// LZMA2 decoder
//-----------------------------------------------------------------------------

struct LZMA2 {
	union Control {
		//00000000				-	EOS
		//00000001 U U			-	Uncompressed Reset Dic
		//00000010 U U			-	Uncompressed No Reset
		//100uuuuu U U P P		-	LZMA no reset
		//101uuuuu U U P P		-	LZMA reset state
		//110uuuuu U U P P S	-	LZMA reset state + new prop
		//111uuuuu U U P P S	-	LZMA reset state + new prop + reset dic
		//	u, U - Unpack Size
		//	P - Pack Size
		//	S - Props
		uint8	u;
		struct { uint8	size:5, reset:1, prop:1, lzma:1; };	//lzma == 1
		struct { uint8	reset_dic:1; };						//lzma == 0

		static Control Copy(bool reset_dic)							{ Control c; c.reset_dic = reset_dic; return c; }
		static Control Compress(uint8 size, bool reset, bool prop)	{ Control c; c.lzma = 1; c.size = size; c.reset = reset; c.prop = prop; return c; }
		Control()	: u(0) {}
		operator uint8()	const	{ return u; }
	};

	static uint32 dic_size_from_prop(int p)	{ return ((p & 1) | 2) << (p / 2 + 11); }
};

struct Decoder2 : Decoder, LZMA2 {
	enum HeaderState {
		HEAD_CONTROL,
		HEAD_UNPACK0,
		HEAD_UNPACK1,
		HEAD_PACK0,
		HEAD_PACK1,
		HEAD_PROP,
		HEAD_DATA,
		HEAD_DATA_CONT,
		HEAD_FINISHED,
		HEAD_ERROR
	};

	HeaderState	header_state;
	Control		control;
	uint32		pack_size;
	uint32		unpack_size;

	HeaderState		UpdateHeaderState(uint8 b);
	const uint8*	process(uint8 *&dst, uint8 *dst_end, const uint8 *src, const uint8 *src_end, TRANSCODE_FLAGS flags);
	Decoder2() : header_state(HEAD_CONTROL) {
		SetProps(Props(CharPosBitsMax, 0, 0));
	}
};

//-----------------------------------------------------------------------------
// LZMA Encoder
//-----------------------------------------------------------------------------

struct MatchFinder {
	enum {
		HASH_BITS2	= 10,
		HASH_BITS3	= 16,
		HASH_BITS4	= 20,
	};
	struct Match {
		uint32	len, dist;
	};

	temp_var<uint32[(1 << HASH_BITS2) + (1 << HASH_BITS3) + (1 << HASH_BITS4)]>	head;	// Heads of the hash chains
	dynamic_array<uint32>	prev;		// Link to older string with same hash index
	const uint8	*start, *end, *p;

	void			Skip(uint32 n)		{ p += n; }
	auto			remaining()			{ return end - p; }
	void			SetSource(const uint8* src, size_t src_len) {
		start	= p = src;
		end		= src + src_len;
	}
	uint32			GetMatches(Match *matches, uint32 max_match);
	MatchFinder(uint32 dict_size) : prev(dict_size) {
		for (auto &i : head)
			i = 0;
		for (auto &i : prev)
			i = 0;
	}
};

struct Encoder : State {
	enum {
		kNumOpts				= 1 << 11,
		kNumBitPriceShiftBits	= 4,
	};

	struct EncProps : Props {
		uint32	dict_size;		// (1 << 12) <= dict_size <= (1 << 27) for 32-bit version (1 << 12) <= dict_size <= (3 << 29) for 64-bit version; default = (1 << 24)
		int		max_match;		// 5 <= max_match <= 273, default = 32
		bool	fast;

		uint32	fixed_dictSize() const {
			if (dict_size >= 1 << 22) {
				uint32 kDictMask = (1 << 20) - 1;
				if (dict_size < 0xFFFFFFFFu - kDictMask)
					return (dict_size + kDictMask) & ~kDictMask;
			}
			for (int i = 11; i <= 30; i++) {
				if (dict_size <= 2 << i)
					return 2 << i;
				if (dict_size <= 3 << i)
					return 3 << i;
			}
			return dict_size;
		}
		EncProps(uint32	dict_size = 1 << 24, int max_match = 32, bool fast = false) : dict_size(dict_size), max_match(max_match), fast(fast) {}
	};

	typedef	uint32	Price;

	struct Optimal {
		static const uint32 Lit = -1, ShortRep = 0;
		// extra:
		// 0   : normal
		// 1   : LIT : MATCH
		// > 1 : MATCH (extra-1) : LIT : REP0 (len)

		Price	price;
		uint16	state;	//4 bits
		uint16	extra;	//9 bits
		uint32	len;	//9 bits
		uint32	dist;
		uint32	reps[NUM_REPS];

		void	SetState(uint32 _state, const uint32* _reps) {
			state	= (uint16)_state;
			reps[0]	= _reps[0];
			reps[1]	= _reps[1];
			reps[2]	= _reps[2];
			reps[3]	= _reps[3];
		}
		void	Set(Price _price, uint32 _len, uint32 _dist, uint32 _extra = 0) {
			price	= _price;
			len		= _len;
			dist	= _dist;
			extra	= (uint16)_extra;
		}
		bool	SetIfBetter(Price _price, uint32 _len, uint32 _dist, uint32 _extra = 0) {
			if (_price < price) {
				Set(_price, _len, _dist, _extra);
				return true;
			}
			return false;
		}
	};

	typedef	prob_encoder<11, uint32, memory_writer>	encoder;

	MatchFinder		match_finder;
	EncProps		props;
	uint64			position;

	// prices
	static const int	LEN_COUNT = 64;
	typedef	cost_table<Price, ProbBits, 4, kNumBitPriceShiftBits>	ProbPrices_t;
	static	ProbPrices_t	ProbPrices;

	Price			align_prices[1 << AlignBits];
	Price			pos_slot_prices[LenToPosStates][32 * 2];
	Price			dist_prices[LenToPosStates][NumFullDistances];
	Price			len_prices[PosStatesMax][LenSymbolsTotal];
	Price			replen_prices[PosStatesMax][LenSymbolsTotal];
	uint32			len_counter, replen_counter;

	// optimal
	Optimal			opt[kNumOpts], *optCur, *optEnd;
	uint32			longestMatchLen;
	uint32			num_match;
	uint32			additionalOffset;
	MatchFinder::Match	matches[LenMax + 1];

	static void		Encode_Len(encoder& rc, uint32 sym, prob_t *probs, uint32 posState);
	static void		Encode_Dist(encoder& rc, uint32 dist, prob_t *probs, uint32 posState);
	static void		WriteEndMarker(encoder &rc, prob_t *probs, uint32 state, uint32 posState);

	//prices

	static Price	price_ShortRep(const prob_t *probs, uint32 state, uint32 posState) {
		return	ProbPrices.price0(probs[IsRepGT0	+ state])
			+	ProbPrices.price0(probs[IsRep0Long	+ PosState(state, posState)]);
	}
	static Price	price_Rep0(const prob_t *probs, uint32 state, uint32 posState) {
		return	ProbPrices.price1(probs[IsMatch		+ PosState(state, posState)])
			+	ProbPrices.price1(probs[IsRep		+ state])
			+	ProbPrices.price0(probs[IsRepGT0	+ state])
			+	ProbPrices.price1(probs[IsRep0Long	+ PosState(state, posState)]);
	}
	static Price	price_Rep(const prob_t *probs, uint32 rep, uint32 state, uint32 posState) {
		return ProbPrices.price(probs[IsRepGT0 + state], rep > 0)
			+ (rep == 0
				? ProbPrices.price1(probs[IsRep0Long + PosState(state, posState)])
				: ProbPrices.price(probs[IsRepGT1 + state], rep > 1)
					+ (rep == 1 ? 0 : ProbPrices.price(probs[IsRepGT2 + state], rep - 2))
			);
	}

	static void	UpdateLenPrices(Price prices[PosStatesMax][LenSymbolsTotal], uint32 numPosStates, const prob_t* probs, uint32 tableSize);
	void	FillDistancesPrices(const prob_t *probs);
	
	void	InitPrices() {
		len_counter = replen_counter = LEN_COUNT;
	}

	bool	SetProps(const EncProps& _props) {
		props	= _props;
		probability.resize(props.GetNumProbs());
		return true;
	}

	void	MovePos(uint32 num) {
		additionalOffset += num;
		match_finder.Skip(num);
	}
	uint32	ReadMatchDistances() {
		additionalOffset++;
		return match_finder.GetMatches(matches, props.max_match);
	}

	uint32	GetOptimum(uint32 position, uint32 &_dist);
	uint32	GetOptimumFast(uint32 &_dist);
	bool	CodeOneBlock(encoder &rc, uint32 maxPackSize, uint32 maxUnpackSize);
	void	Init();

	const uint8* process(uint8*& dst, uint8* dst_end, const uint8* src, const uint8* src_end, TRANSCODE_FLAGS flags);

	Encoder(EncProps props = EncProps()) : match_finder(props.dict_size), position(0) {
		SetProps(props);
		Init();
		InitPrices();
	}
};

//-----------------------------------------------------------------------------
// LZMA2 Encoder
//-----------------------------------------------------------------------------

struct Encoder2 : LZMA2 {
	enum : uint32 {
		BLOCK_SIZE_AUTO		= 0,
		BLOCK_SIZE_SOLID	= (uint32)-1,

		PACK_SIZE_MAX		= 1 << 16,
		COPY_CHUNK_SIZE		= PACK_SIZE_MAX,
		UNPACK_SIZE_MAX		= 1 << 21,
		KEEP_WINDOW_SIZE	= UNPACK_SIZE_MAX,
	};

	struct Coder : Encoder {
		using Encoder::Encoder;
		const uint8 *process(Encoder2* me, uint8*& dst, uint8* dst_end, const uint8* src, const uint8* src_end);
	};

	Encoder::EncProps		props;
	dynamic_array<Coder>	coders;
	uint32					blocksize;

	Encoder2(Encoder::EncProps props = Encoder::EncProps(), uint32 blocksize = BLOCK_SIZE_AUTO) : props(props), coders(1, props), blocksize(blocksize) {}

	//uint8 WriteProperties() {
	//	uint32	   dict_size = props.dict_size;
	//	for (int i = 0; i < 40; i++)
	//		if (dict_size <= Decoder2::dic_size_from_prop(i))
	//			return i;
	//	return 40;
	//}

	const uint8* process(uint8*& dst, uint8* dst_end, const uint8* src, const uint8* src_end, TRANSCODE_FLAGS flags) {
		return coders[0].process(this, dst, dst_end, src, src_end);
	}
};
}  // namespace lzma
#endif	// LZMA_H
