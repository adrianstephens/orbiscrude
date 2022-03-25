#ifndef CODEC_WINDOW_H
#define CODEC_WINDOW_H

#include "codec.h"

namespace iso {

//-----------------------------------------------------------------------------
//	window
//-----------------------------------------------------------------------------

inline void slide_window(const_memory_block& win, size_t size, int max_size) {
	size_t	size1	= win.length() + size;
	if (size1 > max_size)
		win = const_memory_block((const uint8*)win + (size1 - max_size), max_size);
	else
		win = const_memory_block((const uint8*)win, size1);
}

// assumes max_size allocated at win
inline void extend_window(memory_block& win, size_t size, int max_size) {
	size_t	size1	= win.length() + size;

	if (size1 > max_size) {
		win.shift_down(size1 - max_size);
		size1 = max_size;
	}
	win = memory_block(win, size1);
}

// reallocates if necessary
inline void extend_window(malloc_block& win, size_t size, int max_size) {
	size_t	size1	= win.length() + size;

	if (win.length() < max_size) {
		malloc_block	dict2(min(size1, max_size));
		(win + (size1 < max_size ? 0 : size1 - max_size)).copy_to(dict2);
		win = move(dict2);

	} else if (size1 > max_size) {
		win.shift_down(size1 - max_size);
	}
}

template<typename D> void add_to_window(D& win, const void *p, size_t size, int max_size) {
	if (size > max_size) {
		p		= (const uint8*)p + max_size - size;
		size	= max_size;
	}
	extend_window(win, size, max_size);
	win.end(size).copy_from(p);
}

struct window {
	static constexpr const uint8*	adjust_ref(const uint8* ref) {
		return ref;
	}
	static constexpr bool	check_low_limit(const uint8* ref) {
		return false;
	}
	static const uint8*		match_end(const uint8* src, const uint8* ref, const uint8* src_end) {
		return iso::match_end(src, ref - src, src_end);
	}
	static size_t			match_len(const uint8* src, const uint8* ref, const uint8* src_end) {
		return match_end(src, ref, src_end) - src;
	}
	static const uint8*		match_begin(const uint8* src, const uint8* ref, const uint8* src_begin) {
		return iso::match_begin(src, ref - src, src_begin);
	}

	// if we know there can be no overlap or overrun
	static void clipped_simple_copy(uint8* dst, const uint8* ref, uint8* end, uint8* dst_end) {
		size_t	d = size_t(dst - ref);
		if (likely(end + 8 < dst_end)) {
			loose_copy<uint64>(dst, -d, end);
		} else {
			loose_copy<uint64>(dst, -d, end - 7);
			loose_copy<uint8>(dst, -d, end);
		}
	}
	// if we know there can be no overrun
	static void clipped_copy(uint8* dst, const uint8* ref, uint8* end, uint8* dst_end) {
		size_t	d = size_t(dst - ref);
		if (unlikely(d < 8)) {
			if (unlikely(dst + 12 >= dst_end)) {
				loose_copy<uint8>(dst, -d, end);
				return;
			}
			if (d < 4) {
				*dst++ = ref[0];
				*dst++ = ref[1];
				*dst++ = ref[2];
				if (d != 3)
					*dst++ = ref[3];
			}
			copy_packed<uint64>(dst, ref);
			d	= size_t(dst - ref);
			dst += d;
			d	<<= 1;
		}
		if (likely(end + 8 < dst_end)) {
			loose_copy<uint64>(dst, -d, end);
		} else {
			loose_copy<uint64>(dst, -d, end - 7);
			//dst += (end - dst) & ~7;
			loose_copy<uint8>(dst, -d, end);
		}
	}

	// if we know there can be no overlap
	static size_t	simple_copy(uint8* dst, const uint8* ref, uint8* end, uint8* dst_end) {
		if (end <= dst_end) {
			clipped_simple_copy(dst, ref, end, dst_end);
			return 0;
		} else {
			clipped_simple_copy(dst, ref, dst_end, dst_end);
			return end - dst_end;
		}
	}
	// could be overlap or overrun
	static size_t	copy(uint8* dst, const uint8* ref, uint8* end, uint8* dst_end) {
		if (end <= dst_end) {
			clipped_copy(dst, ref, end, dst_end);
			return 0;
		} else {
			clipped_copy(dst, ref, dst_end, dst_end);
			return end - dst_end;
		}
	}
};

//-----------------------------
// sliding dictionaries
//-----------------------------

struct prefix_window : window {};

struct window_with_base : window {
	const uint8*		base;

	window_with_base(const void* base) : base((const uint8*)base) {}

	constexpr bool	check_low_limit(const uint8* ref) const {
		return ref < base;
	}
};

struct external_window : window_with_base {
	const_memory_block	data;

	external_window(const void* base, const_memory_block data) : window_with_base(base), data(data) {}

	constexpr const uint8*	adjust_ref(const uint8* ref) const {
		return ref < base ? ref - (data.end() - base) : ref;
	}

	const uint8*	match_end(const uint8* src, const uint8* ref, const uint8* src_end) const {
		if (ref < base) {
			const uint8*	limit	= min(base + (src - ref), src_end);
			const uint8*	end		= prefix_window::match_end(src, ref + (data.end() - base), limit);
			if (end != limit)
				return end;
			ref = base;
		}
		return prefix_window::match_end(src, ref, src_end);
	}
	size_t			match_len(const uint8* src, const uint8* ref, const uint8* src_end) const {
		return match_end(src, ref, src_end) - src;
	}
	const uint8*	match_begin(const uint8* src, const uint8* ref, const uint8* src_begin) const {
		if (src_begin < base) {
			const uint8*	begin	= prefix_window::match_begin(src, ref, base);
			if (begin != base)
				return begin;
			ref = data.end();
		}
		return prefix_window::match_begin(src, ref, src_begin);
	}

	void	clipped_copy(uint8* dst, const uint8* ref, uint8* end, uint8* dst_end) const {
		if (ref < base) {
			size_t		length0 = base - ref;
			const uint8	*ref0	= data.end() - length0;
			uint8		*end0	= dst + length0;
			if (end <= end0) {
				ref		= ref0;
			} else {
				prefix_window::clipped_copy(dst, ref0, end0, dst_end);
				ref		= base;
				dst		= end0;
			}
		}
		prefix_window::clipped_copy(dst, ref, end, dst_end);
	}
	size_t	copy(uint8* dst, const uint8* ref, uint8* end, uint8* dst_end) const {
		if (end <= dst_end) {
			clipped_copy(dst, ref, end, dst_end);
			return 0;
		} else {
			clipped_copy(dst, ref, dst_end, dst_end);
			return end - dst_end;
		}
	}
};

//-----------------------------
// stationary dictionaries
//-----------------------------

struct stationary_window : window {
	static void		reset()		{}
	static void		clipped_copy(uint8* dst, uint8 *base, intptr_t ref, uint8* end, uint8* dst_end) {
		return window::clipped_copy(dst, base + ref, end, dst_end);
	}
	static size_t	copy(uint8* dst, uint8 *base, intptr_t ref, uint8* end, uint8* dst_end) {
		return window::copy(dst, base + ref, end, dst_end);
	}
};

struct external_stationary_window : stationary_window,  const_memory_block {
	external_stationary_window(const_memory_block ext) : const_memory_block(ext) {}
	void		reset()			{ n = 0; }
	void		clipped_copy(uint8* dst, uint8 *base, intptr_t ref, uint8* end, uint8* dst_end) const {
		if (ref < n) {
			auto	r = n - ref;
			if (end - dst <= r) {
				window::clipped_copy(dst, (uint8*)p + ref, end, dst_end);
				return;
			}
			window::clipped_copy(dst, (uint8*)p + ref, dst + r, dst_end);
			dst += r;
			ref = n;
		}
		window::clipped_copy(dst, base + ref, end, dst_end);
	}
	size_t		copy(uint8* dst, uint8 *base, intptr_t ref, uint8* end, uint8* dst_end) const {
		if (end <= dst_end) {
			clipped_copy(dst, base, ref, end, dst_end);
			return 0;
		} else {
			clipped_copy(dst, base, ref, dst_end, dst_end);
			return end - dst_end;
		}
	}
};

//-----------------------------
// codecs with dictionaries
//-----------------------------

template<typename T, int WSIZE> class decoder_with_window : public T {
	malloc_block	window;
public:
	void		set_dict(memory_block d) { window = d; }
	const uint8*	process(uint8 *&dst, uint8 *dst_end, const uint8 *src, const uint8 *src_end, TRANSCODE_FLAGS flags) {
		uint8	*dst0	= dst;
		if (window) {
			src = T::process(dst, dst_end, src, src_end, flags, external_window(dst, window));
			if ((flags & TRANSCODE_PARTIAL) || src < src_end)
				add_to_window(window, dst0, dst - dst0, WSIZE);
			else
				window = none;
		} else {
			src = T::process(dst, dst_end, src, src_end, flags, prefix_window());
			if ((flags & TRANSCODE_DST_VOLATILE) && ((flags & TRANSCODE_PARTIAL) || src < src_end))
				window = memory_block(dst0, dst).end(WSIZE);
		}
		return src;
	}
};

template<typename T, int WSIZE> class encoder_with_window : public T {
	malloc_block	window;
public:
	void		set_dict(memory_block d) { window = d; }
	const uint8*	process(uint8 *&dst, uint8 *dst_end, const uint8 *src, const uint8 *src_end, TRANSCODE_FLAGS flags) {
		const uint8	*src0	= src;
		if (window) {
			src = T::process(dst, dst_end, src, src_end, flags, external_window(src, window));
			if ((flags & TRANSCODE_PARTIAL) || src < src_end)
				add_to_window(window, src0, src - src0, WSIZE);
			else
				window = none;
		} else {
			src = T::process(dst, dst_end, src, src_end, flags, prefix_window());
			if ((flags & TRANSCODE_SRC_VOLATILE) && ((flags & TRANSCODE_PARTIAL) || src < src_end))
				window = const_memory_block(src0, src).end(WSIZE);
		}
		return src;
	}
};


} //namespace iso
#endif //CODEC_WINDOW_H
