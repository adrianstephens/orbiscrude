#ifndef LZW_H
#define LZW_H

#include "window.h"
#include "stream.h"
#include "vlc.h"

namespace iso {

//-----------------------------------------------------------------------------
//	LZW_decoder
//-----------------------------------------------------------------------------

#define SENTINEL_VLC
template<bool be, bool EARLY_CHANGE, int MAXBITS = 12> class LZW_decoder : public codec_defaults {
	malloc_block	window;
	enum { WSIZE = 1 << MAXBITS };

	struct {
		int	start, len;
	} table[WSIZE];
	
	const int		num_bits;

#ifdef SENTINEL_VLC
	bit_stack_sentinel<uint32, be>	vlc0;
#else
	bit_stack_count<uint32, be>		vlc0;
#endif
	uint8			code_len;
	int				prev_code;
	int				next_code;
	int				clip		= 0;
	size_t			offset		= 0;

	size_t	window_size() const {
		return table[next_code - 1].start + table[next_code - 1].len + table[prev_code].len;
	}

public:
	LZW_decoder(int num_bits) : num_bits(num_bits), code_len(num_bits + 1), prev_code((1 << num_bits) + 1), next_code((1 << num_bits) + 2) {
		for (int i = 0; i < 256; i++)
			table[i].len = 1;
		table[prev_code] = {0, 0};
	}

	template<typename WIN> const uint8*	process(uint8 *&_dst, uint8 *dst_end, const uint8 *src, const uint8 *src_end, uint8 *&_base, WIN win) {
#ifdef SENTINEL_VLC
		vlc_in_sentinel<uint32, be, byte_reader>	vlc(src);
#else
		vlc_in<uint32, be, byte_reader>	vlc(src);
#endif
		vlc.set_state(vlc0);

		uint8		*dst		= _dst;
		uint8		*base		= _base;
		uint8		code_len	= this->code_len;
		int			next_code	= this->next_code;
		int			prev_code	= this->prev_code;
		int			clip		= this->clip;

		const int	restart		= 1 << num_bits;

		if (clip) {
			dst += clip;
			clip = win.copy((uint8*)_dst, base, table[prev_code].start + table[prev_code].len - clip, dst, dst_end);
		}

		while (dst < dst_end) {
			int		code = vlc.get(code_len);

			if (code == restart + 1)
				break;

			if (code == restart) {
				_base		= base = dst;
				win.reset();
				code_len	= num_bits + 1;
				next_code	= restart + 2;

				code = vlc.get(code_len);
				if (code == restart + 1)
					break;

				*dst++ = code;

			} else {
				int	prev_len	= table[prev_code].len;
				int	this_len	= 1;

				if (code < next_code) {
					if (code < restart) {
						*dst = code;
					} else {
						this_len	= table[code].len;
						clip = win.copy(dst, base, table[code].start, dst + this_len, dst_end);
					}

				} else {
					ISO_ASSERT(code == next_code);
					if (prev_code < restart) {
						*dst = prev_code;
					} else {
						clip = win.copy(dst, base, table[prev_code].start, dst + prev_len, dst_end);
					}

					dst += prev_len;
					if (dst < dst_end)
						dst[0] = dst[-prev_len];
					else
						++clip;
				}

				if (next_code < (1 << MAXBITS)) {
					table[next_code].start	= int(dst - base - prev_len);
					table[next_code].len	= prev_len + 1;
					next_code++;
					if (next_code == (1 << code_len) - int(EARLY_CHANGE) && code_len < MAXBITS)
						code_len++;
				}

				dst += this_len;
			}

			prev_code = code;
		}

		vlc0			= vlc.get_state();
		this->code_len	= code_len;
		this->next_code	= next_code;
		this->prev_code	= prev_code;
		this->clip		= clip;

		_dst  = dst - clip;
		return vlc.get_stream().p;
	}

	const uint8*	process(uint8 *&dst, uint8 *dst_end, const uint8 *src, const uint8 *src_end, TRANSCODE_FLAGS flags) {
		if (src == src_end || dst == dst_end)
			return src;

		size_t	written;
		uint8	*base	= dst - offset;

		if (window) {
			uint8	*dst0 = dst;
			src = process(dst, dst_end, src, src_end, base, external_stationary_window(window));
			if ((flags & TRANSCODE_PARTIAL) || src < src_end) {
				if (base < dst0)
					window += memory_block(dst0, dst - base - window.length());
				else
					window = memory_block(base, dst - base);//window_size());
			} else {
				window = none;
			}

		} else {
			src = process(dst, dst_end, src, src_end, base, stationary_window());
			if ((flags & TRANSCODE_DST_VOLATILE) && ((flags & TRANSCODE_PARTIAL) || src < src_end))
				window = memory_block(base, (uint8*)dst - base);//window_size());
		}

		if (base >= dst)
			offset = (uint8*)dst - base;

		offset	= dst - base;
		return src;
	}
#endif
};

//-----------------------------------------------------------------------------
//	LZW_encoder
//-----------------------------------------------------------------------------

template<bool be, bool EARLY_CHANGE, int MAXBITS = 12> class LZW_encoder {
	struct Node {
		int		side[2], root;
		int		c;
		void	reset(int _c) {
			c		= _c;
			side[0]	= side[1] = root = -1;
		}
	};

	const int	num_bits;

	bit_stack_count<uint32, !be>	vlc0;
	int			code_len;
	int			prev_code;
	int			next_code;

	Node		nodes[1 << MAXBITS];

	void		reset() {
		for (int i = 0; i < 1 << num_bits; i++)
			nodes[i].reset(i);
	}

public:
	LZW_encoder(int num_bits = 8) : num_bits(num_bits), code_len(num_bits + 1), prev_code(-1), next_code((1 << num_bits) + 2) {
		reset();
		vlc0.push(1 << num_bits, code_len);
	}

	const uint8*	process(uint8 *&_dst, uint8 *dst_end, const uint8 *src, const uint8 *src_end, TRANSCODE_FLAGS flags) {
		vlc_out<uint32, be, byte_writer>	vlc(_dst);
		vlc.set_state(vlc0);

		uint8*		dst			= _dst;
		uint8		code_len	= this->code_len;
		int			next_code	= this->next_code;
		int			prev_code	= this->prev_code;
		bool		last		= !(flags & TRANSCODE_PARTIAL);

		while (src < src_end) {
			uint8	c = *src++;

			if (prev_code != -1) {
				Node	*node	= &nodes[prev_code];
				int		code	= node->root;

				if (code == -1) {
					node->root	= next_code;

				} else {
					while (code != -1) {
						node = &nodes[code];
						if (node->c == c)
							break;
						code = node->side[c > node->c];
					}
					if (code != -1) {
						prev_code = code;
						continue;
					}

					node->side[c > node->c] = next_code;
				}

				vlc.put(prev_code, code_len);

				if (next_code == (1 << code_len) - int(EARLY_CHANGE))
					code_len++;

				nodes[next_code].reset(c);

				if (++next_code == (1 << MAXBITS)) {
					vlc.put(1 << num_bits, code_len);
					reset();
					code_len	= num_bits + 1;
					next_code	= (1 << num_bits) + 2;
				}
			}
			prev_code = c;
		}
		
		if (last) {
			vlc.put(prev_code, code_len);
			vlc.put((1 << num_bits) + 1, code_len);
			vlc.flush();
		}

		vlc0			= vlc.get_state();
		this->code_len	= code_len;
		this->next_code	= next_code;
		this->prev_code	= prev_code;

		_dst = dst;
		return vlc.get_stream().p;
	}

	const uint8	*process(ostream_ref dst, const uint8 *src, const uint8	*src_end, TRANSCODE_FLAGS flags) {
		vlc_out<uint32, be, ostream_ref>	vlc(dst);
		vlc.set_state(vlc0);

		uint8		code_len	= this->code_len;
		int			next_code	= this->next_code;
		int			prev_code	= this->prev_code;
		bool		last		= !(flags & TRANSCODE_PARTIAL);

		while (src < src_end) {
			uint8	c = *src++;

			if (prev_code != -1) {
				Node	*node	= &nodes[prev_code];
				int		code	= node->root;

				if (code == -1) {
					node->root	= next_code;

				} else {
					while (code != -1) {
						node = &nodes[code];
						if (node->c == c)
							break;
						code = node->side[c > node->c];
					}
					if (code != -1) {
						prev_code = code;
						continue;
					}

					node->side[c > node->c] = next_code;
				}

				vlc.put(prev_code, code_len);

				if (next_code == (1 << code_len) - int(EARLY_CHANGE))
					code_len++;

				nodes[next_code].reset(c);

				if (++next_code == (1 << MAXBITS)) {
					vlc.put(1 << num_bits, code_len);
					reset();
					code_len	= num_bits + 1;
					next_code	= (1 << num_bits) + 2;
				}
			}
			prev_code = c;
		}

		if (last) {
			vlc.put(prev_code, code_len);
			vlc.put((1 << num_bits) + 1, code_len);
			vlc.flush();
		}

		vlc0			= vlc.get_state();
		this->code_len	= code_len;
		this->next_code	= next_code;
		this->prev_code	= prev_code;

		return src;
	}

};

}
