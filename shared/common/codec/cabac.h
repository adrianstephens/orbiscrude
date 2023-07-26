#ifndef CABAC_H
#define CABAC_H

#include "base/defs.h"
#include "stream.h"

namespace iso {

struct context_model {
	uint8	MPSbit:1, state:7;

	void	set(int v) {
		MPSbit	= v >= 64;
		state	= v >= 64 ? (v - 64) : (63 - v);
	}

	bool operator==(context_model b) const { return state==b.state && MPSbit==b.MPSbit; }
	bool operator!=(context_model b) const { return state!=b.state || MPSbit!=b.MPSbit; }
};

inline uint8 make_value(uint32 QPY, uint8 init) {
	int m	= (init >> 4) * 5 - 45;
	int n	= (init & 15) * 8 - 16;
	return min(max(((m * (int)QPY) >> 4) + n, 1), 126);
}

template<typename C> static void init_context(uint32 QPY, context_model* model, const C &init) {
	for (auto &i : init)
		model++->set(make_value(QPY, i));
}
inline void init_context(uint32 QPY, context_model* model, const uint8 &init) {
	model->set(make_value(QPY, init));
}

inline void init_contexts(uint32 QPY, context_model* model) {}

template<typename T, typename...P> static void init_contexts(uint32 QPY, context_model* model, uint32 offset, const T &init, const P&...params) {
	init_context(QPY, model + offset, init);
	init_contexts(QPY, model, params...);
}

//-----------------------------------------------------------------------------
//	decoder
//-----------------------------------------------------------------------------

static const uint8 LPS_table[64][4] = {
	{128, 176, 208, 240},
	{128, 167, 197, 227},
	{128, 158, 187, 216},
	{123, 150, 178, 205},
	{116, 142, 169, 195},
	{111, 135, 160, 185},
	{105, 128, 152, 175},
	{100, 122, 144, 166},
	{ 95, 116, 137, 158},
	{ 90, 110, 130, 150},
	{ 85, 104, 123, 142},
	{ 81,  99, 117, 135},
	{ 77,  94, 111, 128},
	{ 73,  89, 105, 122},
	{ 69,  85, 100, 116},
	{ 66,  80,  95, 110},
	{ 62,  76,  90, 104},
	{ 59,  72,  86,  99},
	{ 56,  69,  81,  94},
	{ 53,  65,  77,  89},
	{ 51,  62,  73,  85},
	{ 48,  59,  69,  80},
	{ 46,  56,  66,  76},
	{ 43,  53,  63,  72},
	{ 41,  50,  59,  69},
	{ 39,  48,  56,  65},
	{ 37,  45,  54,  62},
	{ 35,  43,  51,  59},
	{ 33,  41,  48,  56},
	{ 32,  39,  46,  53},
	{ 30,  37,  43,  50},
	{ 29,  35,  41,  48},
	{ 27,  33,  39,  45},
	{ 26,  31,  37,  43},
	{ 24,  30,  35,  41},
	{ 23,  28,  33,  39},
	{ 22,  27,  32,  37},
	{ 21,  26,  30,  35},
	{ 20,  24,  29,  33},
	{ 19,  23,  27,  31},
	{ 18,  22,  26,  30},
	{ 17,  21,  25,  28},
	{ 16,  20,  23,  27},
	{ 15,  19,  22,  25},
	{ 14,  18,  21,  24},
	{ 14,  17,  20,  23},
	{ 13,  16,  19,  22},
	{ 12,  15,  18,  21},
	{ 12,  14,  17,  20},
	{ 11,  14,  16,  19},
	{ 11,  13,  15,  18},
	{ 10,  12,  15,  17},
	{ 10,  12,  14,  16},
	{  9,  11,  13,  15},
	{  9,  11,  12,  14},
	{  8,  10,  12,  14},
	{  8,   9,  11,  13},
	{  7,   9,  11,  12},
	{  7,   9,  10,  12},
	{  7,   8,  10,  11},
	{  6,   8,   9,  11},
	{  6,   7,   9,  10},
	{  6,   7,   8,   9},
	{  2,   2,   2,   2}
};

static const uint8 renorm_table[32] = {
	6, 5, 4, 4,
	3, 3, 3, 3,
	2, 2, 2, 2,
	2, 2, 2, 2,
	1, 1, 1, 1,
	1, 1, 1, 1,
	1, 1, 1, 1,
	1, 1, 1, 1
};

static const uint8 next_state_MPS[64] = {
	 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16,
	17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,
	33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,
	49,50,51,52,53,54,55,56,57,58,59,60,61,62,62,63
};

static const uint8 next_state_LPS[64] = {
	 0, 0, 1, 2, 2, 4, 4, 5, 6, 7, 8, 9, 9,11,11,12,
	13,13,15,15,16,16,18,18,19,19,21,21,22,22,23,24,
	24,25,26,26,27,27,28,29,29,30,30,30,31,32,32,33,
	33,33,34,34,35,35,35,36,36,36,37,37,37,38,38,63
};

template<typename S> class CABAC_decoder {
	S			file;
	uint32		range;
	uint32		value;
	int16		bits_needed;
public:
	template<typename P> CABAC_decoder(const P &file) : file(file) { reset(); }
	template<typename P> void init(const P &_file) { file = _file; reset(); }

	void reset() {
		bits_needed = 8;
		range		= 510;
		value		= 0;

		auto	c = file.getc();
		if (c != EOF) {
			value		= c << 8;
			bits_needed -= 8;
			c = file.getc();
			if (c != EOF) {
				value		|= c;
				bits_needed -= 8;
			}
		}
	}

	S&		get_stream() { return file; }

	bool	bit(context_model* model) {
		auto	LPS = LPS_table[model->state][(range >> 6) - 4];
		range		-= LPS;
		uint32 scaled_range = range << 7;

		if (value < scaled_range) {
			// MPS path
			bool	decoded_bit	 = model->MPSbit;
			model->state = next_state_MPS[model->state];

			if (scaled_range < (256 << 7)) {
				// scaled range, highest bit (15) not set
				range	= scaled_range >> 6;	// shift range by one bit
				value	<<= 1;					// shift value by one bit
				bits_needed++;

				if (bits_needed == 0) {
					bits_needed = -8;
					value |= file.getc();
				}
			}
			return decoded_bit;

		} else {
			// LPS path
			int num_bits	= renorm_table[LPS >> 3];
			value			= (value - scaled_range) << num_bits;
			range			= LPS << num_bits; // this is always >= 0x100 except for state 63, but state 63 is never used

			//int num_bitsTab = renorm_table[LPS >> 3];
			//ISO_ASSERT(num_bits == num_bitsTab);

			bool	decoded_bit = !model->MPSbit;

			if (model->state == 0)
				model->MPSbit = 1 - model->MPSbit;

			model->state = next_state_LPS[model->state];
			bits_needed += num_bits;

			if (bits_needed >= 0) {
				value |= file.getc() << bits_needed;
				bits_needed -= 8;
			}
			return decoded_bit;
		}

	}

	bool term_bit() {
		range	-= 2;
		uint32 scaledRange = range << 7;

		if (value >= scaledRange)
			return true;

		// there is a while loop in the standard, but it will always be executed only once
		if (scaledRange < (256 << 7)) {
			range	= scaledRange >> 6;
			value	<<= 1;
			bits_needed++;

			if (bits_needed == 0) {
				bits_needed	= -8;
				value		|= file.getc();
			}
		}
		return false;
	}

	bool bypass() {
		value	<<= 1;
		bits_needed++;

		if (bits_needed >= 0) {
			bits_needed = -8;
			value		|= file.getc();
		}

		uint32 scaled_range = range << 7;
		if (value >= scaled_range) {
			value -= scaled_range;
			return true;
		}
		return false;
	}

	int count_ones(int max) {
		for (int i = 0; i < max; i++) {
			if (!bypass())
				return i;
		}
		return max;
	}

	int count_ones(int max, context_model* model) {
		for (int i = 0; i < max; i++) {
			if (!bit(model))
				return i;
		}
		return max;
	}

	int bypass_parallel(int nBits) {
		value		<<= nBits;
		bits_needed += nBits;

		if (bits_needed >= 0) {
			value	|= file.getc() << bits_needed;
			bits_needed -= 8;
		}

		uint32	scaled_range = range << 7;
		int		v	= value / scaled_range;
		if (unlikely(v >= (1 << nBits)))	// may happen with broken bitstreams
			v = (1 << nBits) - 1;
		
		value -= v * scaled_range;
		return v;
	}

	uint32 bypass(int nBits) {
	#if 1
		uint32	v	= 0;
		switch (nBits >> 3) {
			case 3:	v = bypass_parallel(8);
			case 2: v = (v << 8) | bypass_parallel(8);
			case 1: v = (v << 8) | bypass_parallel(8);
			default:
				if (nBits &= 7)
					v = (v << nBits) | bypass_parallel(nBits);
				return v;
		}
	#else
		if (likely(nBits <= 8))
			return nBits == 0 ? 0 : bypass_parallel(nBits);
		uint32	v = bypass_parallel(8);
		if (nBits >= 16) {
			v = (v << 8) | bypass_parallel(8);
			if (nBits >= 24)
				v = (v << 8) | bypass_parallel(8);
		}
		if (nBits &= 7)
			v = (v << nBits) | bypass_parallel(nBits);
		return v;
	#endif
	}

	int EGk_bypass(int k) {
		auto	n = count_ones(32) + k;
		return (1 << n) - (1 << k) + bypass(n);
	}

};

//-----------------------------------------------------------------------------
//	encoder
//-----------------------------------------------------------------------------

static const uint32 entropy_table[128] = {
	// -------------------- 200 --------------------
	/* state= 0 */ 0x07d13 /* 0.977164 */, 0x08255 /* 1.018237 */,
	/* state= 1 */ 0x07738 /* 0.931417 */, 0x086ef /* 1.054179 */,
	/* state= 2 */ 0x0702b /* 0.876323 */, 0x0935a /* 1.151195 */,
	/* state= 3 */ 0x069e6 /* 0.827333 */, 0x09c7f /* 1.222650 */,
	/* state= 4 */ 0x062e8 /* 0.772716 */, 0x0a2c7 /* 1.271708 */,
	/* state= 5 */ 0x05c18 /* 0.719488 */, 0x0ae25 /* 1.360532 */,
	/* state= 6 */ 0x05632 /* 0.673414 */, 0x0b724 /* 1.430793 */,
	/* state= 7 */ 0x05144 /* 0.634904 */, 0x0c05d /* 1.502850 */,
	/* state= 8 */ 0x04bdf /* 0.592754 */, 0x0ccf2 /* 1.601145 */,
	/* state= 9 */ 0x0478d /* 0.559012 */, 0x0d57b /* 1.667843 */,
	/* state=10 */ 0x042ad /* 0.520924 */, 0x0de81 /* 1.738336 */,
	/* state=11 */ 0x03f4d /* 0.494564 */, 0x0e4b8 /* 1.786871 */,
	/* state=12 */ 0x03a9d /* 0.457945 */, 0x0f471 /* 1.909721 */,
	/* state=13 */ 0x037d5 /* 0.436201 */, 0x0fc56 /* 1.971385 */,
	/* state=14 */ 0x034c2 /* 0.412177 */, 0x10236 /* 2.017284 */,
	/* state=15 */ 0x031a6 /* 0.387895 */, 0x10d5c /* 2.104394 */,
	/* state=16 */ 0x02e62 /* 0.362383 */, 0x11b34 /* 2.212552 */,
	/* state=17 */ 0x02c20 /* 0.344752 */, 0x120b4 /* 2.255512 */,
	/* state=18 */ 0x029b8 /* 0.325943 */, 0x1294d /* 2.322672 */,
	/* state=19 */ 0x02791 /* 0.309143 */, 0x135e1 /* 2.420959 */,
	/* state=20 */ 0x02562 /* 0.292057 */, 0x13e37 /* 2.486077 */,
	/* state=21 */ 0x0230d /* 0.273846 */, 0x144fd /* 2.539000 */,
	/* state=22 */ 0x02193 /* 0.262308 */, 0x150c9 /* 2.631150 */,
	/* state=23 */ 0x01f5d /* 0.245026 */, 0x15ca0 /* 2.723641 */,
	/* state=24 */ 0x01de7 /* 0.233617 */, 0x162f9 /* 2.773246 */,
	/* state=25 */ 0x01c2f /* 0.220208 */, 0x16d99 /* 2.856259 */,
	/* state=26 */ 0x01a8e /* 0.207459 */, 0x17a93 /* 2.957634 */,
	/* state=27 */ 0x0195a /* 0.198065 */, 0x18051 /* 3.002477 */,
	/* state=28 */ 0x01809 /* 0.187778 */, 0x18764 /* 3.057759 */,
	/* state=29 */ 0x0164a /* 0.174144 */, 0x19460 /* 3.159206 */,
	/* state=30 */ 0x01539 /* 0.165824 */, 0x19f20 /* 3.243181 */,
	/* state=31 */ 0x01452 /* 0.158756 */, 0x1a465 /* 3.284334 */,
	/* state=32 */ 0x0133b /* 0.150261 */, 0x1b422 /* 3.407303 */,
	/* state=33 */ 0x0120c /* 0.140995 */, 0x1bce5 /* 3.475767 */,
	/* state=34 */ 0x01110 /* 0.133315 */, 0x1c394 /* 3.527962 */,
	/* state=35 */ 0x0104d /* 0.127371 */, 0x1d059 /* 3.627736 */,
	/* state=36 */ 0x00f8b /* 0.121451 */, 0x1d74b /* 3.681983 */,
	/* state=37 */ 0x00ef4 /* 0.116829 */, 0x1dfd0 /* 3.748540 */,
	/* state=38 */ 0x00e10 /* 0.109864 */, 0x1e6d3 /* 3.803335 */,
	/* state=39 */ 0x00d3f /* 0.103507 */, 0x1f925 /* 3.946462 */,
	/* state=40 */ 0x00cc4 /* 0.099758 */, 0x1fda7 /* 3.981667 */,
	/* state=41 */ 0x00c42 /* 0.095792 */, 0x203f8 /* 4.031012 */,
	/* state=42 */ 0x00b78 /* 0.089610 */, 0x20f7d /* 4.121014 */,
	/* state=43 */ 0x00afc /* 0.085830 */, 0x21dd6 /* 4.233102 */,
	/* state=44 */ 0x00a5e /* 0.081009 */, 0x22419 /* 4.282016 */,
	/* state=45 */ 0x00a1b /* 0.078950 */, 0x22a5e /* 4.331015 */,
	/* state=46 */ 0x00989 /* 0.074514 */, 0x23756 /* 4.432323 */,
	/* state=47 */ 0x0091b /* 0.071166 */, 0x24225 /* 4.516775 */,
	/* state=48 */ 0x008cf /* 0.068837 */, 0x2471a /* 4.555487 */,
	/* state=49 */ 0x00859 /* 0.065234 */, 0x25313 /* 4.649048 */,
	/* state=50 */ 0x00814 /* 0.063140 */, 0x25d67 /* 4.729721 */,
	/* state=51 */ 0x007b6 /* 0.060272 */, 0x2651f /* 4.790028 */,
	/* state=52 */ 0x0076e /* 0.058057 */, 0x2687c /* 4.816294 */,
	/* state=53 */ 0x00707 /* 0.054924 */, 0x27da7 /* 4.981661 */,
	/* state=54 */ 0x006d5 /* 0.053378 */, 0x28172 /* 5.011294 */,
	/* state=55 */ 0x00659 /* 0.049617 */, 0x28948 /* 5.072512 */,
	/* state=56 */ 0x00617 /* 0.047598 */, 0x297c5 /* 5.185722 */,
	/* state=57 */ 0x005dd /* 0.045814 */, 0x2a2df /* 5.272434 */,
	/* state=58 */ 0x005c1 /* 0.044965 */, 0x2a581 /* 5.293019 */,
	/* state=59 */ 0x00574 /* 0.042619 */, 0x2ad59 /* 5.354304 */,
	/* state=60 */ 0x0053b /* 0.040882 */, 0x2bba5 /* 5.465973 */,
	/* state=61 */ 0x0050c /* 0.039448 */, 0x2c596 /* 5.543651 */,
	/* state=62 */ 0x004e9 /* 0.038377 */, 0x2cd88 /* 5.605741 */,
	0x00400, 0x2d000 /* dummy, should never be used */
};

class x265_writer {
	dynamic_memory_writer	mem;
	uint8	state		= 0;
public:
	void putc(int byte) {
		// These byte sequences may never occur in the bitstream:	0,0,0 / 0,0,1 / 0,0,2
		// Hence, we have to add a 0x03 before the third byte
		// We also have to add a 0x03 for the sequence 0,0,3, because the escape byte itself also has to be escaped
		if (byte <= 3) {
			if (state < 2 && byte == 0) {
				++state;
			} else if (state == 2) {
				mem.putc(3);
				state = (byte == 0);
			} else {
				state = 0;
			}
		} else {
			state = 0;
		}
		mem.putc(byte);
	}
};

template<typename S> class CABAC_encoder {
protected:
	S			stream;
	uint32		range;
	uint32		low;
	int8		bits_left;
	uint8		buffered_byte;
	uint16		num_buffered_bytes;

	void write_out() {
		int leadByte = low >> (24 - bits_left);
		bits_left	+= 8;
		low			&= 0xffffffffu >> bits_left;

		if (leadByte == 0xff) {
			num_buffered_bytes++;

		} else if (num_buffered_bytes) {
			int carry		= leadByte >> 8;
			int byte		= buffered_byte + carry;
			buffered_byte	= leadByte & 0xff;
			stream.putc(byte);

			byte = (0xff + carry) & 0xff;
			while (--num_buffered_bytes)
				stream.putc(byte);

		} else {
			num_buffered_bytes = 1;
			buffered_byte	= leadByte;
		}
	}
	void testAndWriteOut() {
		if (bits_left < 12)
			write_out();
	}
	void reset() {
		range				= 510;
		low					= 0;
		bits_left			= 23;
		buffered_byte		= 0xFF;
		num_buffered_bytes	= 0;
	}

public:
	void	bit(context_model* model, bool bit) {
		uint32	LPS		= LPS_table[model->state][(range >> 6) - 4];
		range -= LPS;

		if (bit != model->MPSbit) {
			int num_bits = renorm_table[LPS >> 3];
			low			 = (low + range) << num_bits;
			range		 = LPS << num_bits;

			if (model->state == 0)
				model->MPSbit = 1 - model->MPSbit;

			model->state = next_state_LPS[model->state];
			bits_left -= num_bits;

		} else {
			model->state = next_state_MPS[model->state];
			if (range >= 256)
				return;

			low		<<= 1;
			range	<<= 1;
			bits_left--;
		}

		testAndWriteOut();
	}
	void	bypass(bool bit) {
		low <<= 1;
		if (bit)
			low += range;
		bits_left--;
		testAndWriteOut();
	}
	void	term_bit(bool bit) {
		range -= 2;

		if (bit) {
			low			= (low + range) << 7;
			range		= 2 << 7;
			bits_left	-= 7;
		} else if (range >= 256) {
			return;
		} else {
			low			<<= 1;
			range		<<= 1;
			bits_left--;
		}
		testAndWriteOut();
	}
	void	flush() {
		if (low >> (32 - bits_left)) {
			stream.putc(buffered_byte + 1);
			while (num_buffered_bytes > 1) {
				stream.putc(0x00);
				num_buffered_bytes--;
			}
			low -= 1 << (32 - bits_left);
		} else {
			if (num_buffered_bytes)
				stream.putc(buffered_byte);

			while (num_buffered_bytes > 1) {
				stream.putc(0xff);
				num_buffered_bytes--;
			}
		}

		//write_bits(low >> 8, 24 - bits_left);
	}

	void	write_ones(int value, int max) {
		for (int i = 0; i < value; i++)
			bypass(true);
		if (value < max) 
			bypass(false);
	}
	void	bypass(int value, int n) {
		while (n-- > 0)
			bypass(value & (1 << n));
	}
	void	EGk_bypass(int val, int k) {
		while (val >= (1 << k)) {
			bypass(true);
			val -= 1 << k++;
		}
		bypass(false);
		while (k--)
			bypass((val >> k) & 1);
	}
};

#if 0
//-----------------------------------------------------------------------------
//	virtual encoder with vlc
//-----------------------------------------------------------------------------

class CABAC_encoder2 {
public:
	virtual ~CABAC_encoder2() {}

	virtual int		size() const = 0;
	virtual void	reset() = 0;

	// --- VLC ---
	virtual void	write_bits(uint32 bits,int n) = 0;
	virtual void	write_bit(bool bit) {
		write_bits(bit, 1);
	}
	void write_uvlc(uint32 value) {
		int nLeadingZeros	= 0;
		int base			= 0;
		int range			= 1;

		while (value >= base + range) {
			base += range;
			range <<= 1;
			nLeadingZeros++;
		}

		write_bits((1 << nLeadingZeros) | (value - base), 2 * nLeadingZeros + 1);
	}

	void write_svlc(int value) {
		if (value == 0)
			write_bits(1, 1);
		else if (value > 0)
			write_uvlc(2 * value - 1);
		else
			write_uvlc(-2 * value);
	}

	virtual void	write_startcode()		= 0;
	virtual void	skip_bits(int nBits)	= 0;

	virtual void	add_trailing_bits() {
		write_bit(true);
		int nZeros = number_free_bits_in_byte();
		write_bits(0, nZeros);
	}

	virtual int	number_free_bits_in_byte() const = 0;

	// output all remaining bits and fill with zeros to next byte boundary
	virtual void	flush_VLC() {}


	// --- CABAC ---
	virtual bool	modifies_context() const = 0;
	virtual void	init_CABAC() {}
	virtual void	write_CABAC_bit(context_model* model, bool bit) = 0;
	virtual void	write_CABAC_bypass(bool bit) = 0;
	virtual void	write_CABAC_TU_bypass(int value, int max) {
		for (int i = 0; i < value; i++)
			write_CABAC_bypass(true);
		if (value < max) 
			write_CABAC_bypass(false);
	}
	virtual void	write_CABAC_FL_bypass(int value, int n) {
		while (n-- > 0)
			write_CABAC_bypass(value & (1 << n));
	}
	virtual void	write_CABAC_term_bit(bool bit) = 0;
	virtual void	flush_CABAC() {}

	void write_CABAC_EGk(int val, int k) {
		while (val >= (1 << k)) {
			write_CABAC_bypass(true);
			val -= 1 << k++;
		}
		write_CABAC_bypass(false);
		while (k--)
			write_CABAC_bypass((val >> k) & 1);
	}


	float RDBits_for_CABAC_bin(context_model* model, bool bit) {
		int idx = (model->state << 1) + (bit != model->MPSbit);
		return entropy_table[idx] / float(1 << 15);
	}
};

class CABAC_encoder_bitstream : public CABAC_encoder2, CABAC_encoder<x265_writer> {
	typedef CABAC_encoder<x265_writer>	CABAC;

	// VLC
	uint32		vlc_buffer;
	uint32		vlc_buffer_len = 0;

public:
	void reset() override {
		vlc_buffer_len	= 0;
		CABAC::reset();
	}

	// --- VLC ---

	void write_bits(uint32 bits, int n) override {
		vlc_buffer <<= n;
		vlc_buffer |= bits;
		vlc_buffer_len += n;

		while (vlc_buffer_len >= 8) {
			stream.putc((vlc_buffer >> (vlc_buffer_len - 8)) & 0xFF);
			vlc_buffer_len -= 8;
		}
	}
	void write_startcode() override {
		stream.putc(0);
		stream.putc(0);
		stream.putc(1);
	}
	void skip_bits(int n) override {
		while (n >= 8) {
			write_bits(0, 8);
			n -= 8;
		}
		if (n > 0)
			write_bits(0, n);
	}

	int number_free_bits_in_byte() const override {
		return -vlc_buffer_len & 7;
	}

	// output all remaining bits and fill with zeros to next byte boundary
	void flush_VLC() override {
		while (vlc_buffer_len >= 8) {
			stream.putc((vlc_buffer >> (vlc_buffer_len - 8)) & 0xFF);
			vlc_buffer_len -= 8;
		}

		if (vlc_buffer_len > 0) {
			stream.putc(vlc_buffer << (8 - vlc_buffer_len));
			vlc_buffer_len = 0;
		}

		vlc_buffer = 0;
	}

	// --- CABAC ---
	bool	modifies_context() const						override { return true; }
	void	init_CABAC()									override { CABAC::reset(); }
	void	write_CABAC_bit(context_model* model, bool bit) override { CABAC::bit(model, bit); }
	void	write_CABAC_bypass(bool bit)					override { CABAC::bypass(bit); }
	void	write_CABAC_term_bit(bool bit)					override { CABAC::term_bit(bit); }
	void	flush_CABAC()									override { CABAC::flush(); }
};


class CABAC_encoder_estim : public CABAC_encoder2 {
protected:
	uint64 frac_bits = 0;
public:
	uint64	getFracBits()	const	{ return frac_bits; }
	float	getRDBits()		const	{ return frac_bits / float(1<<15); }

	void	reset()				override { frac_bits = 0; }
	int		size()		const	override { return frac_bits >> (15 + 3); }

	// --- VLC ---
	void	write_bits(uint32 bits,int n)					override { frac_bits += n << 15; }
	void	write_bit(bool bit)								override { frac_bits += 1 << 15; }
	void	write_startcode()								override { frac_bits += 3 << 18; }
	void	skip_bits(int n)								override { frac_bits += n << 15; }
	int		number_free_bits_in_byte() const				override { return 0; } // TODO, good enough for now

	// --- CABAC ---
	void	write_CABAC_bit(context_model* model, bool bit)	override {
		int idx = model->state << 1;

		if (bit == model->MPSbit) {
			model->state = next_state_MPS[model->state];

		} else {
			idx++;
			if (model->state == 0)
				model->MPSbit = 1 - model->MPSbit;
			model->state = next_state_LPS[model->state];
		}
		frac_bits += entropy_table[idx];
	}
	void	write_CABAC_bypass(bool bit)					override { frac_bits += 0x8000; }
	void	write_CABAC_FL_bypass(int value, int n)			override { frac_bits += n << 15; }
	void	write_CABAC_term_bit(bool bit)					override {}
	bool	modifies_context() const						override { return true; }
};


class CABAC_encoder_estim_constant : public CABAC_encoder_estim {
public:
	void	write_CABAC_bit(context_model* model, bool bit) override { frac_bits += entropy_table[(model->state << 1) + (bit != model->MPSbit)]; }
	bool	modifies_context() const						override { return false; }
};
#endif
} //namespace iso

#endif // CABAC_H