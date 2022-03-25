#include "movie.h"
#include "matroska.h"
#include "base/algorithm.h"
#include "iso/iso_files.h"
#include "thread.h"
#include "jobs.h"
#include "D:\dev\Github\libvpx\vpx/vpx_decoder.h"
#include "D:\dev\Github\libvpx\vpx/vp8dx.h"

#include "vpx_decode.h"

extern "C" {
_Check_return_ _ACRTIMP long long __cdecl llrint(_In_ double _X);
_Check_return_ _ACRTIMP float     __cdecl exp2f(_In_ float _X);
#include "libavcodec\avcodec.h"
extern AVCodec			ff_vp9_decoder;
extern AVCodecParser	ff_vp9_parser;
}

using namespace iso;

uint32 BlockCRC(const uint8 *p, int stride, int w, int h) {
	uint32	t = 0;
	while (h--) {
		t = CRC32(p, w, t);
		p += stride;
	}
	return t;
}

template<typename T> struct Tester {
	Semaphore	used_count, free_count;
	T			last_t;

	int			num_put, num_check;

	Tester() : used_count(0), free_count(1), num_put(0), num_check(0) {}

	void	Put(const T &t) {
		free_count.Lock();
		++num_put;
		last_t	= t;
		used_count.UnLock();
	}
	bool	Check(const T &t) {
		used_count.Lock();
		++num_check;
		bool	ret = t == last_t;
		free_count.UnLock();
		return ret;
	}
};

bool	use_tester;

extern "C" {

void TestBlockCRC(int decoder, const void *p, int stride, int x, int y, int w, int h) {
	struct Data {
		int			x, y, w, h;
		uint32		crc;
		Data() {}
		Data(int _x, int _y, int _w, int _h, uint32 _crc) : x(_x), y(_y), w(_w), h(_h), crc(_crc) {}
		bool operator==(const Data &b) const {
			return x == b.x && y == b.y && w == b.w && h == b.h && crc == b.crc;
		}
	};
	static Tester<Data> tester;
	if (use_tester) {
		uint32	crc = BlockCRC((const uint8*)p, stride, w, h);
		if (decoder == 0) {
			tester.Put(Data(x, y, w, h, crc));
			//ISO_TRACEF("CRC @%i, %i: %08x\n", x, y, crc);
		} else {
			if (!tester.Check(Data(x, y, w, h, crc)))
				ISO_TRACEF("!FAIL:CRC @%i, %i: %08x\n", x, y, crc);
		}
	}
}

void CheckReader(int decoder, int x, int y, uint64 value, uint8 bits, uint8 range) {
	struct Data {
		int			x, y;
		uint64		value;
		uint8		bits, range;
		Data() {}
		Data(int _x, int _y, uint64 _value, uint8 _bits, uint8 _range) : x(_x), y(_y), value(_value), bits(_bits), range(_range) {}
		bool operator==(const Data &b) const {
			int		min_bits	= min(bits, b.bits);
			return x == b.x && y == b.y && range == b.range && (value >> (64 - min_bits)) == (b.value >> (64 - min_bits));
		}
	};
	static Tester<Data> tester;
	if (use_tester) {
		if (decoder == 0) {
			tester.Put(Data(x, y, value, bits, range));
			//ISO_TRACEF("Reader @%i, %i: %02x:%02x\n", x, y, value, range);
		} else {
			if (!tester.Check(Data(x, y, value, bits, range)))
				ISO_TRACEF("!FAIL:Reader @%i, %i: %02x:%02x\n", x, y, value, range);
		}
	}
}

void CheckMode(int decoder, int x, int y, uint32 mode) {
	struct Data {
		int			x, y;
		uint32		mode;
		Data() {}
		Data(int _x, int _y, uint32 _mode) : x(_x), y(_y), mode(_mode) {}
		bool operator==(const Data &b) const {
			ISO_ASSERT(x == b.x && y == b.y);
			return mode == b.mode;
		}
	};
	static Tester<Data> tester;
	if (0 && use_tester) {
		if (decoder == 0) {
			tester.Put(Data(x, y, mode));
			//ISO_TRACEF("Mode @%i, %i: %02x:%02x\n", x, y, value, range);
		} else {
			if (!tester.Check(Data(x, y, mode)))
				ISO_TRACEF("!FAIL:Mode @%i, %i\n", x, y);
		}
	}
}

}


struct vbitmap_yuv : vbitmap {
	void	*data;
	uint8	*planes[4];
	uint32	strides[4];
	uint8	x_chroma_shift, y_chroma_shift, bitdepth;

	vbitmap_yuv() : vbitmap(this), data(0)	{}
	~vbitmap_yuv() { free(data); }

	bool	get(const vbitmap_loc &in, vbitmap_format fmt, void *dest, uint32 stride, uint32 width, uint32 height) {
		if (in.m)
			return false;
		
		int		x_uv		= in.x >> x_chroma_shift;
		int		y_uv		= in.y >> y_chroma_shift;
		int		stride_y	= strides[VPX_PLANE_Y];
		int		stride_u	= strides[VPX_PLANE_U];
		int		stride_v	= strides[VPX_PLANE_V];

		if (fmt.is<YUYV>()) {
			for (int y = in.y, y1 = y + height; y < y1; y++, dest = (uint8*)dest + stride) {
				const uint8	*sy	= planes[VPX_PLANE_Y] + y * stride_y;
				const uint8	*su	= planes[VPX_PLANE_U] + (y >> y_chroma_shift) * stride_u; 
				const uint8	*sv	= planes[VPX_PLANE_V] + (y >> y_chroma_shift) * stride_v;

				YUYV	*d	= (YUYV*)dest;
				for (int x = in.x, x1 = x + width; x < x1; x += 2)
					*d++ = YUYV(sy[x], su[x >> x_chroma_shift], sy[x + 1], sv[x >> x_chroma_shift]);
			}
			return true;
		} else {
			if (planes[VPX_PLANE_U] && planes[VPX_PLANE_V]) {
				for (int y = in.y, y1 = y + height; y < y1; y++, dest = (uint8*)dest + stride) {
					const uint8	*sy	= planes[VPX_PLANE_Y] + y * stride_y;
					const uint8	*su	= planes[VPX_PLANE_U] + (y >> y_chroma_shift) * stride_u; 
					const uint8	*sv	= planes[VPX_PLANE_V] + (y >> y_chroma_shift) * stride_v; 
					ISO_rgba	*d	= (ISO_rgba*)dest;
					for (int x = in.x, x1 = x + width; x < x1; x++)
						*d++ = iso::YUV(sy[x], su[x >> x_chroma_shift], sv[x >> x_chroma_shift]);
				}
			} else {
				for (int y = in.y, y1 = y + height; y < y1; y++, dest = (uint8*)dest + stride) {
					const uint8	*sy	= planes[VPX_PLANE_Y] + y * stride_y;
					ISO_rgba	*d	= (ISO_rgba*)dest;
					for (int x = in.x, x1 = x + width; x < x1; x++)
						*d++ = sy[x];
				}
			}
		}
		return true;
	}
	bool	set(const vpx_image &img) {
		set(img.d_w, img.d_h, img.x_chroma_shift, img.x_chroma_shift, img.bit_depth);
		for (int i = 0; i < 4; i++) {
			planes[i]	= img.planes[i];
			strides[i]	= img.stride[i];
		}
		return true;
	}
	bool	set(int _width, int _height, int _x_chroma_shift, int _y_chroma_shift, int _bitdepth) {
		free(data);
		data			= 0;//img.fb_priv;

		width			= _width;
		height			= _height;
		x_chroma_shift	= _x_chroma_shift;
		y_chroma_shift	= _y_chroma_shift;
		bitdepth		= _bitdepth;
		clear(planes);
		return true;
	}
	void	*get_raw(uint32 plane, vbitmap_format *fmt, uint32 *stride, uint32 *width, uint32 *height) {
		if (plane < 4) {
			*stride = strides[plane];
			*width	= this->width  >> (plane ? x_chroma_shift : 0);
			*height	= this->height >> (plane ? y_chroma_shift : 0);
			*fmt	= bitdepth;
			return planes[plane];
		}
		return 0;
	}

};
ISO_DEFSAME(vbitmap_yuv, vbitmap);


//-----------------------------------------------------------------------------
//	Y4M
//-----------------------------------------------------------------------------

struct yuv_input {
	struct decimation {
		uint8 h, v;
		void	init(int _h, int _v) {
			h = _h;
			v = _v;
		}
		uint32	size(int width, int height) const {
			return width * height + (h && v ? 2 * div_round_up(width, h) * div_round_up(height, v) : 0);
		}
	};
	uint16		width, height;
	uint8		bits_per_sample;
	uint8		bit_depth;
	decimation	src_dec, dst_dec;

	void	init(int _width, int _height, int _bit_depth, int _bps, int src_h, int src_v, int dst_h, int dst_v) {
		width			= _width;
		height			= _height;
		bit_depth		= _bit_depth;
		src_dec.init(src_h, src_v);
		dst_dec.init(dst_h, dst_v);
		bits_per_sample	= dst_dec.size(4, 4) * bit_depth / 16;
	}

	uint32	src_size() { return src_dec.size(width, height) * div_round_up(bit_depth, 8); }
	uint32	dst_size() { return dst_dec.size(width, height) * div_round_up(bit_depth, 8); }

};

inline uint8 scale(int x) { return (uint8)clamp((x + 64) >> 7, 0, 255); }

/*
All anti-aliasing filters in the following conversion functions are based on one of two window functions:

The 6-tap Lanczos window (for down-sampling and shifts):
   sinc(pi*t)*sinc(pi*t/3), |t|<3
   0,                       |t|>=3

The 4-tap Mitchell window (for up-sampling):
   7|t|^3 - 12|t|^2 + 16/3,               |t|<1
   -(7/3)|t|^3 + 12|t|^2 - 20|t| + 32/3,  |t|<2
   0,                                     |t|>=2
*/

/*420jpeg chroma samples are sited like:
	Y-------Y-------Y-------Y-------
	|       |       |       |
	|   BR  |       |   BR  |
	|       |       |       |
	Y-------Y-------Y-------Y-------
	|       |       |       |
	|       |       |       |
	|       |       |       |
	Y-------Y-------Y-------Y-------
	|       |       |       |
	|   BR  |       |   BR  |
	|       |       |       |
	Y-------Y-------Y-------Y-------
	|       |       |       |
	|       |       |       |
	|       |       |       |

	420mpeg2 chroma samples are sited like:
	Y-------Y-------Y-------Y-------
	|       |       |       |
	BR      |       BR      |
	|       |       |       |
	Y-------Y-------Y-------Y-------
	|       |       |       |
	|       |       |       |
	|       |       |       |
	Y-------Y-------Y-------Y-------
	|       |       |       |
	BR      |       BR      |
	|       |       |       |
	Y-------Y-------Y-------Y-------
	|       |       |       |
	|       |       |       |
	|       |       |       |

We use a resampling filter to shift the site locations one quarter pixel (at the chroma plane's resolution) to the right.
The 4:2:2 modes look exactly the same, except there are twice as many chroma lines, and they are vertically co-sited with the luma samples in both the mpeg2 and jpeg cases (thus requiring no vertical resampling).
*/
static void y4m_42xmpeg2_42xjpeg_helper(uint8 *dst, const uint8 *src, int w, int h) {
	for (int y = 0; y < h; y++) {
		//Filter: [4 -17 114 35 -9 1]/128, derived from a 6-tap Lanczos window
		int x;
		for (x = 0; x < min(w, 2); x++) {
			dst[x] = scale(
				+   4 * src[0]
				-  17 * src[max(x - 1, 0)]
				+ 114 * src[x]
				+  35 * src[min(x + 1, w - 1)]
				-   9 * src[min(x + 2, w - 1)]
				+   1 * src[min(x + 3, w - 1)]
			);
		}
		for (; x < w - 3; x++) {
			dst[x] = scale(
				+   4 * src[x - 2]
				-  17 * src[x - 1]
				+ 114 * src[x]
				+  35 * src[x + 1]
				-   9 * src[x + 2]
				+   1 * src[x + 3]
			);
		}
		for (; x < w; x++) {
			dst[x] = scale(
				+   4 * src[x - 2]
				-  17 * src[x - 1]
				+ 114 * src[x]
				+  35 * src[min(x + 1, w - 1)]
				-   9 * src[min(x + 2, w - 1)]
				+   1 * src[w - 1]
			);
		}
		dst += w;
		src += w;
	}
}

//Handles both 422 and 420mpeg2 to 422jpeg and 420jpeg, respectively
static void y4m_convert_42xmpeg2_42xjpeg(yuv_input *p, uint8 *dst, uint8 *aux) {
	//Skip past the luma data
	dst += p->width * p->height;
	//Compute the size of each chroma plane
	int	w		= div_round_up(p->width, p->dst_dec.h);
	int	h		= div_round_up(p->height, p->dst_dec.v);
	int	sz		= w * h;

	y4m_42xmpeg2_42xjpeg_helper(dst, aux, w, h);
	y4m_42xmpeg2_42xjpeg_helper(dst+ sz, aux + sz, w, h);
}

/*This format is only used for interlaced content, but is included for completeness.

  420jpeg chroma samples are sited like:
  Y-------Y-------Y-------Y-------
  |       |       |       |
  |   BR  |       |   BR  |
  |       |       |       |
  Y-------Y-------Y-------Y-------
  |       |       |       |
  |       |       |       |
  |       |       |       |
  Y-------Y-------Y-------Y-------
  |       |       |       |
  |   BR  |       |   BR  |
  |       |       |       |
  Y-------Y-------Y-------Y-------
  |       |       |       |
  |       |       |       |
  |       |       |       |

  420paldv chroma samples are sited like:
  YR------Y-------YR------Y-------
  |       |       |       |
  |       |       |       |
  |       |       |       |
  YB------Y-------YB------Y-------
  |       |       |       |
  |       |       |       |
  |       |       |       |
  YR------Y-------YR------Y-------
  |       |       |       |
  |       |       |       |
  |       |       |       |
  YB------Y-------YB------Y-------
  |       |       |       |
  |       |       |       |
  |       |       |       |

  We use a resampling filter to shift the site locations one quarter pixel (at the chroma plane's resolution) to the right.
  Then we use another filter to move the C_r location down one quarter pixel, and the C_b location up one quarter pixel.
*/
static void y4m_convert_42xpaldv_42xjpeg(yuv_input *p, uint8 *dst, uint8 *aux) {
	//Skip past the luma data
	dst += p->width * p->height;
	//Compute the size of each chroma plane
	int		w		= div_round_up(p->width, 2);
	int		h		= div_round_up(p->height, p->dst_dec.h);
	int		sz		= w * h;
	uint8	*tmp	= aux + 2 * sz;

	y4m_42xmpeg2_42xjpeg_helper(tmp, aux, w, h);
	//Slide C_b up a quarter-pel. This is the same filter used above, but in the other order
	for (int x = 0, y; x < w; x++) {
		for (y = 0; y < min(h, 3); y++) {
			dst[y * w] = scale(
				+   1 * tmp[0]
				-   9 * tmp[max(y - 2, 0) * w]
				+  35 * tmp[max(y - 1, 0) * w]
				+ 114 * tmp[y * w]
				-  17 * tmp[min(y + 1, h - 1) * w]
				+   4 * tmp[min(y + 2, h - 1) * w]
			);
		}
		for (; y < h - 2; y++) {
			dst[y * w] = scale(
				+   1 * tmp[(y - 3) * w]
				-   9 * tmp[(y - 2) * w]
				+  35 * tmp[(y - 1) * w]
				+ 114 * tmp[y * w]
				-  17 * tmp[(y + 1) * w]
				+   4 * tmp[(y + 2) * w]
			);
		}
		for (; y < h; y++) {
			dst[y * w] = scale(
				+   1 * tmp[(y - 3) * w]
				-   9 * tmp[(y - 2) * w]
				+  35 * tmp[(y - 1) * w]
				+ 114 * tmp[y * w]
				-  17 * tmp[min(y + 1, h - 1) * w]
				+   4 * tmp[(h - 1) * w]
			);
		}
		dst++;
		tmp++;
	}

	dst += sz - w;
	tmp -= w;

	y4m_42xmpeg2_42xjpeg_helper(tmp, aux + sz, w, h);
	//Slide C_r down a quarter-pel. This is the same as the horizontal filter
	for (int x = 0, y; x < w; x++) {
		for (y = 0; y < min(h, 2); y++) {
			dst[y * w] = scale(
				+   4 * tmp[0]
				-  17 * tmp[max(y - 1, 0) * w]
				+ 114 * tmp[y * w]
				+  35 * tmp[min(y + 1, h - 1) * w]
				-   9 * tmp[min(y + 2, h - 1) * w]
				+   1 * tmp[min(y + 3, h - 1) * w]
			);
		}
		for (; y < h - 3; y++) {
			dst[y * w] = scale(
				+   4 * tmp[(y - 2) * w]
				-  17 * tmp[(y - 1) * w]
				+ 114 * tmp[y * w]
				+  35 * tmp[(y + 1) * w]
				-   9 * tmp[(y + 2) * w]
				+   1 * tmp[(y + 3) * w]
			);
		}
		for (; y < h; y++) {
			dst[y * w] = scale(
				+   4 * tmp[(y - 2) * w]
				-  17 * tmp[(y - 1) * w]
				+ 114 * tmp[y * w]
				+  35 * tmp[min(y + 1, h - 1) * w]
				-   9 * tmp[min(y + 2, h - 1) * w]
				+   1 * tmp[(h - 1) * w]
			);
		}
		dst++;
		tmp++;
	}
	/*	For actual interlaced material, this would have to be done separately on  each field, and the shift amounts would be different.
		C_r moves down 1/8, C_b up 3/8 in the top field, and C_r moves down 3/8, C_b up 1/8 in the bottom field.
		The corresponding filters would be:
		Down 1/8 (reverse order for up): [3 -11 125 15 -4 0]/128
		Down 3/8 (reverse order for up): [4 -19 98 56 -13 2]/128
	*/
}

//Perform vertical filtering to reduce a single plane from 4:2:2 to 4:2:0. This is used as a helper by several converation routines
static void y4m_422jpeg_420jpeg_helper(uint8 *dst, const uint8 *src, int w, int h) {
	//Filter: [3 -17 78 78 -17 3]/128, derived from a 6-tap Lanczos window.
	for (int x = 0, y; x < w; x++) {
		for (y = 0; y < min(h, 2); y += 2) {
			dst[(y >> 1)*w] = scale(
				+ 64 * src[0]
				+ 78 * src[min(1, h - 1) * w]
				- 17 * src[min(2, h - 1) * w]
				+  3 * src[min(3, h - 1) * w]
			);
		}
		for (; y < h - 3; y += 2) {
			dst[(y >> 1)*w] = scale(
				+  3 * (src[(y - 2) * w] + src[(y + 3) * w])
				- 17 * (src[(y - 1) * w] + src[(y + 2) * w])
				+ 78 * (src[y * w] + src[(y + 1) * w])
			);
		}
		for (; y < h; y += 2) {
			dst[(y >> 1)*w] = scale(
				+  3 * (src[(y - 2) * w] + src[(h - 1) * w])
				- 17 * (src[(y - 1) * w] + src[min(y + 2, h - 1) * w])
				+ 78 * (src[y * w] + src[min(y + 1, h - 1) * w])
			);
		}
		src++;
		dst++;
	}
}

/*420jpeg chroma samples are sited like:
  Y-------Y-------Y-------Y-------
  |       |       |       |
  |   BR  |       |   BR  |
  |       |       |       |
  Y-------Y-------Y-------Y-------
  |       |       |       |
  |       |       |       |
  |       |       |       |
  Y-------Y-------Y-------Y-------
  |       |       |       |
  |   BR  |       |   BR  |
  |       |       |       |
  Y-------Y-------Y-------Y-------
  |       |       |       |
  |       |       |       |
  |       |       |       |

  422jpeg chroma samples are sited like:
  Y---BR--Y-------Y---BR--Y-------
  |       |       |       |
  |       |       |       |
  |       |       |       |
  Y---BR--Y-------Y---BR--Y-------
  |       |       |       |
  |       |       |       |
  |       |       |       |
  Y---BR--Y-------Y---BR--Y-------
  |       |       |       |
  |       |       |       |
  |       |       |       |
  Y---BR--Y-------Y---BR--Y-------
  |       |       |       |
  |       |       |       |
  |       |       |       |
 */

//  We use a resampling filter to decimate the chroma planes by two in the vertical direction.
static void y4m_convert_422jpeg_420jpeg(yuv_input *p, uint8 *dst, uint8 *aux) {
	//Skip past the luma data
	dst += p->width * p->height;
	//Compute the size of each chroma plane
	int		w		= div_round_up(p->width, p->src_dec.h);
	int		h		= p->height;
	int		sz		= w * h;
	int		dsz		= div_round_up(p->width, p->dst_dec.h) * div_round_up(h, p->dst_dec.v);

	y4m_422jpeg_420jpeg_helper(dst, aux, w, h);
	y4m_422jpeg_420jpeg_helper(dst + dsz, aux + sz, w, h);
}

/*420jpeg chroma samples are sited like:
  Y-------Y-------Y-------Y-------
  |       |       |       |
  |   BR  |       |   BR  |
  |       |       |       |
  Y-------Y-------Y-------Y-------
  |       |       |       |
  |       |       |       |
  |       |       |       |
  Y-------Y-------Y-------Y-------
  |       |       |       |
  |   BR  |       |   BR  |
  |       |       |       |
  Y-------Y-------Y-------Y-------
  |       |       |       |
  |       |       |       |
  |       |       |       |

  422 chroma samples are sited like:
  YBR-----Y-------YBR-----Y-------
  |       |       |       |
  |       |       |       |
  |       |       |       |
  YBR-----Y-------YBR-----Y-------
  |       |       |       |
  |       |       |       |
  |       |       |       |
  YBR-----Y-------YBR-----Y-------
  |       |       |       |
  |       |       |       |
  |       |       |       |
  YBR-----Y-------YBR-----Y-------
  |       |       |       |
  |       |       |       |
  |       |       |       |
*/
// We use a resampling filter to shift the original site locations one quarter  pixel (at the original chroma resolution) to the right.
// Then we use a second resampling filter to decimate the chroma planes by two in the vertical direction.
static void y4m_convert_422_420jpeg(yuv_input *p, uint8 *dst, uint8 *aux) {
	//Skip past the luma data
	dst += p->width * p->height;
	//Compute the size of each chroma plane
	int		w		= div_round_up(p->width, p->src_dec.h);
	int		h		= p->height;
	int		dst_c_h = div_round_up(p->height, p->dst_dec.v);
	int		sz		= w * h;
	int		dsz		= w * dst_c_h;
	uint8	*tmp	= aux + 2 * sz;

	//In reality, the horizontal and vertical steps could be pipelined, for less memory consumption and better cache performance, but we do them separately for simplicity
	y4m_42xmpeg2_42xjpeg_helper(tmp, aux, w, h);		//First do horizontal filtering (convert to 422jpeg)
	y4m_422jpeg_420jpeg_helper(dst, tmp, w, h);			//Now do the vertical filtering

	y4m_42xmpeg2_42xjpeg_helper(tmp, aux + sz, w, h);	//First do horizontal filtering (convert to 422jpeg)
	y4m_422jpeg_420jpeg_helper(dst + dsz, tmp, w, h);	//Now do the vertical filtering
}

/*420jpeg chroma samples are sited like:
  Y-------Y-------Y-------Y-------
  |       |       |       |
  |   BR  |       |   BR  |
  |       |       |       |
  Y-------Y-------Y-------Y-------
  |       |       |       |
  |       |       |       |
  |       |       |       |
  Y-------Y-------Y-------Y-------
  |       |       |       |
  |   BR  |       |   BR  |
  |       |       |       |
  Y-------Y-------Y-------Y-------
  |       |       |       |
  |       |       |       |
  |       |       |       |

  411 chroma samples are sited like:
  YBR-----Y-------Y-------Y-------
  |       |       |       |
  |       |       |       |
  |       |       |       |
  YBR-----Y-------Y-------Y-------
  |       |       |       |
  |       |       |       |
  |       |       |       |
  YBR-----Y-------Y-------Y-------
  |       |       |       |
  |       |       |       |
  |       |       |       |
  YBR-----Y-------Y-------Y-------
  |       |       |       |
  |       |       |       |
  |       |       |       |
*/
// We use a filter to resample at site locations one eighth pixel (at the source chroma plane's horizontal resolution) and five eighths of a pixel to the right.
// Then we use another filter to decimate the planes by 2 in the vertical direction.
static void y4m_convert_411_420jpeg(yuv_input *p, uint8 *dst, uint8 *aux) {
	//Skip past the luma data
	dst += p->width * p->height;
	//Compute the size of each chroma plane
	int		w		= div_round_up(p->width, p->src_dec.h);
	int		h		= p->height;
	int		dst_c_w = div_round_up(p->width, p->dst_dec.h);
	int		dst_c_h = div_round_up(p->height, p->dst_dec.v);
	int		sz		= w * h;
	int		dsz		= dst_c_w * dst_c_h;
	int		tsz		= dst_c_w * h;
	uint8	*tmp	= aux + 2 * sz;
	for (int i = 1; i < 3; i++) {
		// In reality, the horizontal and vertical steps could be pipelined, for  less memory consumption and better cache performance, but we do them separately for simplicity
		// First do horizontal filtering (convert to 422jpeg)
		for (int y = 0, x; y < h; y++) {
			// Filters: [1 110 18 -1]/128 and [-3 50 86 -5]/128, both derived from a 4-tap Mitchell window.
			for (x = 0; x < min(w, 1); x++) {
				tmp[x << 1] = scale(
					 111 * aux[0]
					+ 18 * aux[min(1, w - 1)]
					-  1 * aux[min(2, w - 1)]
				);
				tmp[x << 1 | 1] = scale(
					+ 47 * aux[0]
					+ 86 * aux[min(1, w - 1)]
					-  5 * aux[min(2, w - 1)]
				);
			}
			for (; x < w - 2; x++) {
				tmp[x << 1] = scale(
					+   1 * aux[x - 1]
					+ 110 * aux[x]
					+  18 * aux[x + 1]
					-   1 * aux[x + 2]
				);
				tmp[x << 1 | 1] = scale(
					-  3 * aux[x - 1]
					+ 50 * aux[x]
					+ 86 * aux[x + 1]
					-  5 * aux[x + 2]
				);
			}
			for (; x < w; x++) {
				tmp[x << 1] = scale(
					+   1 * aux[x - 1]
					+ 110 * aux[x]
					+  18 * aux[min(x + 1, w - 1)]
					-   1 * aux[w - 1]
				);
				if ((x << 1 | 1) < dst_c_w) {
					tmp[x << 1 | 1] = scale(
						-  3 * aux[x - 1]
						+ 50 * aux[x]
						+ 86 * aux[min(x + 1, w - 1)]
						-  5 * aux[w - 1]
					);
				}
			}
			tmp += dst_c_w;
			aux += w;
		}
		tmp -= tsz;
		// Now do the vertical filtering
		y4m_422jpeg_420jpeg_helper(dst, tmp, dst_c_w, h);
		dst += dsz;
	}
}

/*Convert 444 to 420jpeg.*/
static void y4m_convert_444_420jpeg(yuv_input *p, uint8 *dst, uint8 *aux) {
	//Skip past the luma data
	dst += p->width * p->height;
	//Compute the size of each chroma plane
	int		w		= div_round_up(p->width, p->src_dec.h);
	int		h		= p->height;
	int		dst_c_w = div_round_up(p->width, p->dst_dec.h);
	int		dst_c_h = div_round_up(p->height, p->dst_dec.v);
	int		sz		= w * h;
	int		dsz		= dst_c_w * dst_c_h;
	int		tsz		= dst_c_w * h;
	uint8	*tmp	= aux + 2 * sz;
	for (int i = 1; i < 3; i++) {
		// Filter: [3 -17 78 78 -17 3]/128, derived from a 6-tap Lanczos window
		for (int y = 0; y < h; y++) {
			int	x;
			for (x = 0; x < min(w, 2); x += 2) {
				tmp[x >> 1] = scale(
					+ 64 * aux[0]
					+ 78 * aux[min(1, w - 1)]
					- 17 * aux[min(2, w - 1)]
					+  3 * aux[min(3, w - 1)]
				);
			}
			for (; x < w - 3; x += 2) {
				tmp[x >> 1] = scale(
					+  3 * (aux[x - 2] + aux[x + 3])
					- 17 * (aux[x - 1] + aux[x + 2])
					+ 78 * (aux[x] + aux[x + 1])
				);
			}
			for (; x < w; x += 2) {
				tmp[x >> 1] = scale(
					+  3 * (aux[x - 2] + aux[w - 1])
					- 17 * (aux[x - 1] + aux[min(x + 2, w - 1)])
					+ 78 * (aux[x] + aux[min(x + 1, w - 1)])
				);
			}
			tmp += dst_c_w;
			aux += w;
		}
		tmp -= tsz;
		// Now do the vertical filtering
		y4m_422jpeg_420jpeg_helper(dst, tmp, dst_c_w, h);
		dst += dsz;
	}
}

// The image is padded with empty chroma components at 4:2:0
static void y4m_convert_mono_420jpeg(yuv_input *p, uint8 *dst, uint8 *aux) {
	memset(dst + p->width * p->height, 128, div_round_up(p->width, p->dst_dec.h) * div_round_up(p->height, p->dst_dec.v) * 2);
}

// No conversion function needed
static void y4m_convert_null(yuv_input *p, uint8 *dst, uint8 *aux) {
	(void)p;
	(void)dst;
	(void)aux;
}

struct yuv_reader : yuv_input {
	typedef void convert_func(yuv_input *y4m, uint8 *dst, uint8 *src);
	char			interlace;
	malloc_block	dst_buf, aux_buf;
	size_t			dst_buf_read_sz;	//The amount to read directly into the converted frame buffer.
	size_t			aux_buf_read_sz;	//The amount to read into the auxilliary buffer.
	vpx_img_fmt		vpx_fmt;
	convert_func	*convert;

	yuv_reader() : convert(y4m_convert_null), dst_buf_read_sz(0), aux_buf_read_sz(0) {}

	bool	init(int _width, int _height, const char *_chroma_type);
	bool	decode_frame(istream &file, vpx_image *img);
};

bool yuv_reader::init(int width, int height, const char *_chroma_type) {
	auto	chroma_type	= str(_chroma_type);
	aux_buf_read_sz	= 0;

	if (chroma_type == "420" || chroma_type == "420jpeg") {
		vpx_fmt = VPX_IMG_FMT_I420;
		yuv_input::init(width, height, 8, 12, 2,2,2,2);

	} else if (chroma_type == "420p10") {
		vpx_fmt = VPX_IMG_FMT_I42016;
		yuv_input::init(width, height, 10, 15, 2,2,2,2);

	} else if (chroma_type == "420p12") {
		vpx_fmt = VPX_IMG_FMT_I42016;
		yuv_input::init(width, height, 12, 18, 2,2,2,2);

	} else if (chroma_type == "420mpeg2") {
		vpx_fmt = VPX_IMG_FMT_I420; 
		yuv_input::init(width, height, 8, 12, 2,2,2,2);
		//Chroma filter required: read into the aux buf first
		aux_buf.create(aux_buf_read_sz = 2 * ((width + 1) / 2) * ((height + 1) / 2));
		convert = y4m_convert_42xmpeg2_42xjpeg;

	} else if (chroma_type == "420paldv") {
		vpx_fmt = VPX_IMG_FMT_I420;
		yuv_input::init(width, height, 8, 12, 2,2,2,2);
		//Chroma filter required: read into the aux buf first
		aux_buf.create(3 * ((width + 1) / 2) * ((height + 1) / 2));
		aux_buf_read_sz = 2 * ((width + 1) / 2) * ((height + 1) / 2);
		convert = y4m_convert_42xpaldv_42xjpeg;

	} else if (chroma_type == "422jpeg") {
		vpx_fmt = VPX_IMG_FMT_I420;
		yuv_input::init(width, height, 8, 12, 2,1,2,2);
		//Chroma filter required: read into the aux buf first
		aux_buf.create(aux_buf_read_sz = 2 * ((width + 1) / 2) * height);
		convert = y4m_convert_422jpeg_420jpeg;

	} else if (chroma_type == "422") {
		vpx_fmt = VPX_IMG_FMT_I422;
		yuv_input::init(width, height, 8, 16, 2,1,2,1);

	} else if (chroma_type == "422p10") {
		vpx_fmt = VPX_IMG_FMT_I42216;
		yuv_input::init(width, height, 10, 20, 2,1,2,1);

	} else if (chroma_type == "422p12") {
		vpx_fmt = VPX_IMG_FMT_I42216;
		yuv_input::init(width, height, 12, 24, 2,1,2,1);

	} else if (chroma_type == "411") {
		vpx_fmt = VPX_IMG_FMT_I420;
		yuv_input::init(width, height, 8, 12, 4,1,2,2);
		//Chroma filter required: read into the aux buf first
		aux_buf_read_sz = 2 * ((width + 3) / 4) * height;
		aux_buf.create(aux_buf_read_sz + ((width + 1) / 2) * height);
		convert = y4m_convert_411_420jpeg;

	} else if (chroma_type == "444") {
		vpx_fmt = VPX_IMG_FMT_I444;
		yuv_input::init(width, height, 8, 24, 1,1,1,1);

	} else if (chroma_type == "444p10") {
		vpx_fmt = VPX_IMG_FMT_I44416;
		yuv_input::init(width, height, 10, 30, 1,1,1,1);

	} else if (chroma_type == "444p12") {
		vpx_fmt = VPX_IMG_FMT_I44416;
		yuv_input::init(width, height, 12, 36, 1,1,1,1);

	} else if (chroma_type == "444alpha") {
		vpx_fmt = VPX_IMG_FMT_444A;
		yuv_input::init(width, height, 8, 32, 1,1,1,1);

	} else if (chroma_type == "mono") {
		vpx_fmt = VPX_IMG_FMT_I420;
		yuv_input::init(width, height, 8, 12, 0,0,2,2);
		convert = y4m_convert_mono_420jpeg;

	} else if (chroma_type == "440") {
		vpx_fmt = VPX_IMG_FMT_I440;
		yuv_input::init(width, height, 8, 12, 1,2,1,2);

	} else {
		return false;
	}
	dst_buf_read_sz = src_size() - aux_buf_read_sz;
	dst_buf.create(dst_size());
	return true;
}

bool yuv_reader::decode_frame(istream &file, vpx_image *img) {
	file.readbuff(dst_buf, dst_buf_read_sz);
	file.readbuff(aux_buf, aux_buf_read_sz);
	(*convert)(this, dst_buf, aux_buf);

	clear(*img);
	img->fmt			= vpx_fmt;
	img->w				= img->d_w = width;
	img->h				= img->d_h = height;
	img->x_chroma_shift = dst_dec.h >> 1;
	img->y_chroma_shift = dst_dec.v >> 1;
	img->bps			= bits_per_sample;

	//Set up the buffer pointers
	int		bytes_per_sample = bit_depth > 8 ? 2 : 1;
	int		y_stride	= width * bytes_per_sample;
	int		y_size		= y_stride * height;
	int		uv_stride	= div_round_up(width, dst_dec.h) * bytes_per_sample;
	int		uv_h		= div_round_up(height, dst_dec.v);
	int		uv_size		= uv_stride * uv_h;

	img->stride[VPX_PLANE_Y]		= img->stride[VPX_PLANE_ALPHA]	= y_stride;
	img->stride[VPX_PLANE_U]		= img->stride[VPX_PLANE_V]		= uv_stride;

	img->planes[VPX_PLANE_Y]		= dst_buf;
	img->planes[VPX_PLANE_U]		= img->planes[VPX_PLANE_Y] + y_size;
	img->planes[VPX_PLANE_V]		= img->planes[VPX_PLANE_U] + uv_size;
	img->planes[VPX_PLANE_ALPHA]	= img->planes[VPX_PLANE_V] + uv_size;
	return true;
}

class Y4M_frames : public ISO_virtual_defaults, yuv_reader {
	auto_ptr<istream>		file;
	uint16					aspect_w, aspect_h;
	rational<int>			fps;
	streamptr				first_frame;
	int						num_frames;
	int						last_frame;
	ISO_ptr<vbitmap_yuv>	bm;

	bool	ReadFrameHeader() {
		string			s;
		s.read_line(*file);
		string_scan		ss(s);
		return ss.get_token() == "FRAME";
	}
	
public:
	Y4M_frames(istream *_file) : file(_file), num_frames(0), last_frame(-1), bm("Y4M") {}
	int					Count()		const	{ return num_frames; }
	ISO_browser2		Index(int i);
	int					Width()		const	{ return width; }
	int					Height()	const	{ return height; }
	float				FrameRate()	const	{ return fps; }
	bool				Init() {
		string			s;
		s.read_line(*file);

		string_scan		ss(s);
		if (ss.get_token() != "YUV4MPEG2")
			return false;

		fixed_string<16>	chroma_type;
		int		width, height, fps_n, fps_d;

		while (char c = ss.skip_whitespace().getc()) {
			switch (c) {
				case 'W':	ss >> width;						break;
				case 'H':	ss >> height;						break;
				case 'F':	ss >> fps_n >> ':' >> fps_d;		break;
				case 'I':	interlace = ss.getc();				break;
				case 'A':	ss >> aspect_w >> ':' >> aspect_h;	break;
				case 'C':	chroma_type = ss.get_token();		break;
				case 'X':	ss.get_token();						break;
				default:	return false;
			}
		}
		fps = rational<int>(fps_n, fps_d);

		if (!yuv_reader::init(width, height, chroma_type))
			return false;

		first_frame = file->tell();
		while (ReadFrameHeader()) {
			file->seek_cur(dst_buf_read_sz + aux_buf_read_sz);
			++num_frames;
		}
		file->seek(first_frame);
		return true;
	}
};

ISO_browser2 Y4M_frames::Index(int i) {
	if (i == last_frame)
		return bm;

	if (i < last_frame) {
		file->seek(first_frame);
		last_frame = -1;
	}

	while (last_frame < i - 1) {
		if (!ReadFrameHeader())
			return ISO_NULL;
		file->seek_cur(dst_buf_read_sz + aux_buf_read_sz);
		++last_frame;
	}

	vpx_image img;
	if (!ReadFrameHeader() || !decode_frame(*file, &img))
		return ISO_NULL;

	++last_frame;
	bm->set(img);
	return bm;
}

ISO_DEFVIRT(Y4M_frames);

class Y4MFileHandler : public FileHandler {
	const char*		GetExt() override { return "Y4M"; }
	const char*		GetDescription() override { return "YUV4MPEG2"; }
	int				Check(istream &file) override {
		file.seek(0);
		return memcmp(file.get<fixed_array<char, 10> >(), "YUV4MPEG2 ", 10) == 0 ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}

	ISO_ptr<void>	ReadWithFilename(tag id, const filename &fn) override {
		ISO_ptr<Y4M_frames>	frames(0, new FileInput(fn));
		if (frames->Init())
			return ISO_ptr<movie>(id, frames);
		return ISO_NULL;
	}
} Y4M;

//-----------------------------------------------------------------------------
//	YUV
//-----------------------------------------------------------------------------

class YUV_frames : public ISO_virtual_defaults, yuv_reader {
	auto_ptr<istream>	file;
	int					bits;
	int					num_frames;
	int					last_frame;
	ISO_ptr<vbitmap_yuv>		bm;
public:
	YUV_frames(const filename &fn);
	int					Count()		const	{ return num_frames; }
	ISO_browser2		Index(int i);
	int					Width()		const	{ return width; }
	int					Height()	const	{ return height; }
	float				FrameRate()	const	{ return 30; }
	bool				Valid()		const	{ return num_frames != 0; }
};

YUV_frames::YUV_frames(const filename &fn) : file(new FileInput(fn)), last_frame(-1), bm("YUV") {
	int	width = 0, height = 0, bits = 8;

	if (const char *p = fn.find('_')) {
		string_scan			ss(p + 1);
		fixed_string<16>	chroma_type;
		while (ss.remaining()) {
			const char *save	= ss.getp();
			if (is_digit(ss.peekc())) {
				int	n = ss.get();
				switch (ss.peekc()) {
					case 'p':
						height	= n;
						width	= 16 * height / 9;
						ss.move(1);
						break;
					default:
						if (ss.getp() - save < 3) {
							bits	= n;
						} else {
							chroma_type = ss.move(save - ss.getp()).get_token(char_set::alphanum);
						}
						break;
				}
			} else switch (ss.getc()) {
				case 'w':	width	= ss.get(); break;
				case 'h':	height	= ss.get(); break;
				case '_':	break;
			}
		}
		if (width || height) {
			if (height == 0)
				height = width * 9 / 16;
			if (width == 0)
				width = height * 16 / 9;
		}
		yuv_reader::init(width, height, chroma_type);
		num_frames = filelength(fn) / dst_buf_read_sz;
	}
}

ISO_browser2 YUV_frames::Index(int i) {
	if (i == last_frame)
		return bm;

	file->seek(dst_buf_read_sz * i);

	vpx_image img;
	if (!decode_frame(*file, &img))
		return ISO_NULL;

	last_frame = i;
	bm->set(img);
	return bm;
}

ISO_DEFVIRT(YUV_frames);

class YUVFileHandler : public FileHandler {
	const char*		GetExt() override { return "YUV"; }
	const char*		GetDescription() override { return "YUV4MPEG2"; }
	int				Check(istream &file) override {
		file.seek(0);
		return memcmp(file.get<fixed_array<char, 10> >(), "YUV4MPEG2 ", 10) == 0 ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}

	ISO_ptr<void>	ReadWithFilename(tag id, const filename &fn) override {
		ISO_ptr<YUV_frames>	frames(0, fn);
		if (frames->Valid())
			return ISO_ptr<movie>(id, frames);
		return ISO_NULL;
	}
} YUV;

//-----------------------------------------------------------------------------
//	IVF
//-----------------------------------------------------------------------------

#define VP8_FOURCC 0x30385056
#define VP9_FOURCC 0x30395056
#define VP10_FOURCC 0x303a5056

struct IVF_header {
	enum { SIGNATURE = 'DKIF' };
	uint32be			signature;		//'DKIF'
	uint16				version;		// (should be 0)
	uint16				header_size;	//length of header in bytes
	uint32				codec;			// FourCC (e.g., 'VP80')
	uint16				width;
	uint16				height;
	rational<uint32>	fps;
	uint32				num_frames;
	uint32				unused;

	IVF_header()	{}
	bool	valid() const {
		return signature == SIGNATURE && version == 0;
	}
};

struct IVF_frame {
	uint32			size; // of frame in bytes (not including the 12-byte header)
	packed<uint64>	timestamp;
};
class IVF_frames : public ISO_virtual_defaults, IVF_header {
	auto_ptr<istream>	file;
	vpx_codec_ctx_t		decoder;
	int					last_frame;
	ISO_ptr<vbitmap_yuv>	bm;
public:
	IVF_frames(istream *_file) : file(_file), last_frame(-1), bm("IVF") { file->read<IVF_header>(*this); }
	int					Count()		const	{ return num_frames; }
	ISO_browser2		Index(int i);

	int					Width()		const	{ return width; }
	int					Height()	const	{ return height & 0x3fff; }
	float				FrameRate()	const	{ return fps; }
	bool				Init() {
		if (!valid())
			return false;

		vpx_codec_iface_t	*iface		= 0;
		vpx_codec_dec_cfg_t	cfg			= { 0, 0, 0 };
		int					dec_flags	= 0;//(postproc ? VPX_CODEC_USE_POSTPROC : 0) | (ec_enabled ? VPX_CODEC_USE_ERROR_CONCEALMENT : 0) | (frame_parallel ? VPX_CODEC_USE_FRAME_THREADING : 0);

		switch (codec) {
			case VP8_FOURCC:	iface = vpx_codec_vp8_dx(); break;
			case VP9_FOURCC:	iface = vpx_codec_vp9_dx(); break;
			//case VP10_FOURCC:	iface = vpx_codec_vp10_dx(); break;
		}

		return iface && vpx_codec_dec_init(&decoder, iface, &cfg, dec_flags) == 0;
	}
};

ISO_browser2 IVF_frames::Index(int i) {
	if (i == last_frame)
		return bm;

	if (i < last_frame) {
		file->seek(sizeof(IVF_header));
		last_frame = -1;
	}

	IVF_frame	frame;
	while (last_frame < i - 1) {
		file->read(frame);
		file->seek_cur(frame.size);
		++last_frame;
	}

	file->read(frame);
	malloc_block	m(*file, frame.size);
	++last_frame;

	if (vpx_codec_decode(&decoder, m, m.length(), NULL, 0)) {
		ISO_TRACEF("Failed to decode frame %d: %s\n", i, vpx_codec_error(&decoder));
	}

	vpx_codec_iter_t	iter = 0;
	if (vpx_image_t *img = vpx_codec_get_frame(&decoder, &iter))
		bm->set(*img);
	return bm;
}

ISO_DEFVIRT(IVF_frames);
class IVFFileHandler : public FileHandler {
	const char*		GetExt() override { return "ivf"; }
	const char*		GetDescription() override { return "Raw VP8 data"; }
	int				Check(istream &file) override {
		file.seek(0);
		return file.get<IVF_header>().valid() ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}

	ISO_ptr<void>	ReadWithFilename(tag id, const filename &fn) override {
		ISO_ptr<IVF_frames>	frames(0, new FileInput(fn));
		if (frames->Init())
			return ISO_ptr<movie>(id, frames);
		return ISO_NULL;
	}
} IVF;

//-----------------------------------------------------------------------------
//	WEBM
//-----------------------------------------------------------------------------

//#define FFVP9
//#define ISO_VP9

struct VP9Decoder {
	virtual bool init(const matroska::VideoTrack* video_track, int threads)	= 0;
	virtual bool decode_frame(const void *buffer, size_t size, int f)		= 0;
	virtual bool get_frame(vbitmap_yuv *bm)									= 0;
};

struct VP9DecoderVPX : VP9Decoder {
	vpx_codec_ctx_t	decoder;
	vpx_img_fmt		fmt;
	vpx_bit_depth	bit_depth;

	bool init(const matroska::VideoTrack* video_track, int threads) {
		vpx_codec_iface_t	*iface		= 0;
		vpx_codec_dec_cfg_t	cfg			= { threads, 0, 0 };
		int					dec_flags	= 0;//(postproc ? VPX_CODEC_USE_POSTPROC : 0) | (ec_enabled ? VPX_CODEC_USE_ERROR_CONCEALMENT : 0) | (frame_parallel ? VPX_CODEC_USE_FRAME_THREADING : 0);

		if (video_track->codec_id.begins("V_VP8")) {
			iface = vpx_codec_vp8_dx();
		} else if (video_track->codec_id.begins("V_VP9")) {
			iface = vpx_codec_vp9_dx();
	//	} else if (video_track->codec_id.begins("V_VP10")) {
	//		iface = vpx_codec_vp10_dx();
		}
		return iface && vpx_codec_dec_init(&decoder, iface, &cfg, dec_flags) == 0;
	}
	bool decode_frame(const void *buffer, size_t size, int f) {
		int		err = vpx_codec_decode(&decoder, (const uint8*)buffer, (uint32)size, NULL, 0);
		if (err)
			ISO_TRACEF("Failed to decode frame %d: %s\n", f, vpx_codec_error(&decoder));
		return err == 0;
	}
	bool get_frame(vbitmap_yuv *bm) {
		vpx_codec_iter_t	iter = 0;
		if (vpx_image_t *img = vpx_codec_get_frame(&decoder, &iter)) {
			bm->set(*img);
			return true;
		}
		return false;
	}
};

struct VP9DecoderFF : VP9Decoder {
	AVCodecContext	*ctx;
	AVFrame			*frame;
	VP9DecoderFF() : ctx(0), frame(0) {}
	~VP9DecoderFF()  {
		avcodec_close(ctx);
		av_free(ctx);
		av_frame_free(&frame);
	}

	bool init(const matroska::VideoTrack* video_track, int threads) {
		const AVCodec	*codec		= &ff_vp9_decoder;
		AVDictionary	**options	= 0;

		frame		= av_frame_alloc();
		ctx			= avcodec_alloc_context3(codec);
		if (codec->capabilities & AV_CODEC_CAP_TRUNCATED)
			ctx->flags |= AV_CODEC_FLAG_TRUNCATED; // we do not send complete frames

		ctx->thread_count = threads;
		int		err	= avcodec_open2(ctx, codec, options);
		return err == 0;
	}
	bool decode_frame(const void *buffer, size_t size, int f) {
		AVPacket	pkt;
		av_init_packet(&pkt);

		pkt.data	= (uint8*)buffer;
		pkt.size	= size;
		int		got_frame;
		int		len = avcodec_decode_video2(ctx, frame, &got_frame, &pkt);
		if (len < 0)
			ISO_TRACEF("Failed to decode frame %d: %i\n", f, len);
		return len >= 0;
	}
	bool get_frame(vbitmap_yuv *bm) {
		bm->set(frame->width, frame->height, 1, 1, 8);
		for (int i = 0; i < 4; i++) {
			bm->planes[i]	= frame->data[i];
			bm->strides[i]	= frame->linesize[i];
		}
		return true;
	}
};

struct VP9DecoderISO : VP9Decoder {
	vp9::Decoder	decoder;
	bool init(const matroska::VideoTrack* video_track, int threads) {
		decoder.threads	= threads;
		decoder.use_gpu	= false;
		return video_track->codec_id.begins("V_VP9");
	}
	bool decode_frame(const void *buffer, size_t size, int f) {
		int		ret = decoder.decode_frame((uint8*)buffer, (uint8*)buffer + size);
		if (ret < 0)
			ISO_TRACEF("Failed to decode frame %d: %i\n", f, ret);
		return ret >= 0;
	}
	bool get_frame(vbitmap_yuv *bm) {
		bm->set(decoder.width, decoder.height, decoder.cs.subsampling_x, decoder.cs.subsampling_y, decoder.cs.bit_depth);
		for (int i = 0; i < 3; i++) {
			bm->planes[i] = decoder.cur_frame->buffer(i).buffer;
			bm->strides[i] = decoder.cur_frame->buffer(i).stride;
		}
		return true;
	}
};

struct VP9DecoderBoth : VP9Decoder {
	VP9DecoderVPX	d1;
	VP9DecoderISO	d2;
	Semaphore		sem1;
	Semaphore		sem2;

	VP9DecoderBoth() : sem1(0), sem2(0) {}

	bool init(const matroska::VideoTrack* video_track, int threads) {
		return d1.init(video_track, threads) && d2.init(video_track, threads);
	}
	bool decode_frame(const void *buffer, size_t size, int f) {
		ConcurrentJobs::Get().add([this, buffer, size, f]() {
			d1.decode_frame(buffer, size, f);
			sem1.UnLock();
		});
		ConcurrentJobs::Get().add([this, buffer, size, f]() {
			d2.decode_frame(buffer, size, f);
			sem2.UnLock();
		});
		sem1.Lock();
		sem2.Lock();
		return true;
	}
	bool get_frame(vbitmap_yuv *bm) {
		return d2.get_frame(bm);
	}
};

struct WEBM_frames : public ISO_virtual_defaults {
	istream						&file;
	malloc_block				buffer;
	uint64						timestamp_ns;
	matroska::TrackReader		video;

	uint32			width;
	uint32			height;
	rational<int>	pixel_aspect_ratio;

	float			framerate;
	float			duration;
	int				last_frame;
	ISO_ptr<vbitmap_yuv>	bm;

	VP9Decoder		*decoder;

	WEBM_frames(istream *_file) : file(*_file), pixel_aspect_ratio(1), last_frame(-1), bm("WEBM") {
		use_tester = false;
		switch (root["variables"]["decoder"].GetInt()) {
			default:	decoder = new VP9DecoderVPX; break;
			case 1:		decoder = new VP9DecoderFF; break;
			case 2:		decoder = new VP9DecoderISO; break;
			case 3:		decoder = new VP9DecoderBoth; use_tester = true; break;
		}
	}
	~WEBM_frames() {
		delete decoder;
	}
	bool				Init();
	int					Count()		const	{ return duration * framerate; }
	ISO_browser2		Index(int i);
	int					Width()		const	{ return width; }
	int					Height()	const	{ return height; }
	float				FrameRate()	const	{ return framerate; }
};

bool WEBM_frames::Init() {
	EBMLHeader			header;
	matroska::Segment	*segment;

	if (!header.read(file)
	|| matroska::Segment::CreateInstance(file, file.tell(), segment)
	|| segment->Load() < 0
	)
		return false;

	const matroska::Tracks *const tracks	= segment->tracks;
	const matroska::VideoTrack* video_track = NULL;
	const matroska::AudioTrack* audio_track = NULL;
	for (uint32 i = 0; i < tracks->size32(); ++i) {
		const matroska::Track* const track = (*tracks)[i];
		switch (track->type) {
			case matroska::Track::kVideo: video_track = static_cast<const matroska::VideoTrack*>(track); break;
			case matroska::Track::kAudio: audio_track = static_cast<const matroska::AudioTrack*>(track); break;
		}
	}

	if (!video_track || !video_track->codec_id)
		return false;

	width		= video_track->width;
	height		= video_track->height;
	framerate	= video_track->rate;
	duration	= segment->info->GetDuration() / 1e9;

	video.Init(segment, video_track->number);
	video.Rewind();
	if (framerate == 0) {
		uint32 i				= 0;
		while (i < 50 && video.GetNextFrame() && video.GetTimestamp() < 1000000000)
			++i;
		framerate = i * 1e9 / video.GetTimestamp();
		video.Rewind();
	}
#if 0
	if (decoder->init(video_track, 8)) {
		Time	t;
		for (int i = 0;; ++i) {
			Index(i);
			ISO_OUTPUTF("Frame: %i fps:%f\n", i, i / float(t));
		}
		return true;
	}
	return false;
#else
	return decoder->init(video_track, 0);
#endif
}

ISO_browser2 WEBM_frames::Index(int i) {
	if (i == last_frame)
		return bm;

	i = last_frame + 1;

	if (i < last_frame) {
		video.Rewind();
		last_frame	= -1;
	}

	while (last_frame < i) {
		if (matroska::Block::Frame *frame = video.GetNextFrame()) {
			timestamp_ns	= video.GetTimestamp();
			size_t	bytes_in_buffer	= frame->len;
			if (bytes_in_buffer > buffer.length32())
				buffer.create(bytes_in_buffer);

			file.seek(frame->pos);
			file.readbuff(buffer, bytes_in_buffer);
			decoder->decode_frame(buffer, bytes_in_buffer, i);
		} else {
			decoder->decode_frame(0, 0, i);
		}
		++last_frame;
	}

	decoder->get_frame(bm);
	return bm;
}

ISO_DEFVIRT(WEBM_frames);
class WebmMFileHandler : public FileHandler {
	const char*		GetExt() override { return "Webm"; }
	const char*		GetDescription() override { return "google webm movie"; }
	int				Check(istream &file) override {
		file.seek(0);

		int		first = file.getc();
		if (first <= 0)
			return CHECK_DEFINITE_NO;

		uint32	id = first;
		while (!(first & 0x80)) {
			id = (id << 8) | file.getc();
			first <<= 1;
		}
		return id == EBMLHeader::ID ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}
	ISO_ptr<void>	ReadWithFilename(tag id, const filename &fn) override {
		ISO_ptr<WEBM_frames>	frames(0, new FileInput(fn));
		if (frames->Init())
			return ISO_ptr<movie>(id, frames);
		return ISO_NULL;
	}
} webm;
