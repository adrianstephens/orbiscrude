#ifndef MOVIE_H
#define MOVIE_H

#include "../bitmap/bitmap.h"

namespace iso {

class movie {
public:
	ISO_ptr<void>	frames;
	int				width, height;
	float			fps;
	movie()	{}
	movie(ISO_ptr<void> frames, int width, int height, float fps) : frames(frames), width(width), height(height), fps(fps) {}
	template<typename T> movie(ISO_ptr<T> frames) : frames(frames), width(frames->Width()), height(frames->Height()), fps(frames->FrameRate()) {}
};

ISO_DEFUSERCOMPV(iso::movie, width, height, fps, frames);

struct planar_format {
	struct plane {
		uint8	hshift, vshift;
		int8	hoffset, voffset;
		plane(int hshift = 0, int vshift = 0, int hoffset = 0, int voffset = 0) :
			hshift(hshift),		vshift(vshift),
			hoffset(hoffset),	voffset(voffset)
		{}
		auto	size(int width, int height, int bitdepth) const {
			return shift_right_round(width, hshift) * shift_right_round(height, vshift) * shift_right_round(bitdepth, 3);
		}
	};
	
	plane	planes[4];
	uint8	interlace	= 0;

	auto	size(vbitmap_format format, int width, int height) const {
		return planes[0].size(width, height, format.bitdepth(0))
			+  planes[1].size(width, height, format.bitdepth(1))
			+  planes[2].size(width, height, format.bitdepth(2))
			+  planes[3].size(width, height, format.bitdepth(3));
	}

};

struct vbitmap_yuv : vbitmap {
	planar_format	planes;
	uint8			*texels[4];
	uint32			strides[4];

	vbitmap_yuv() : vbitmap(this)	{}

	bool	get(const vbitmap_loc &in, vbitmap_format fmt, void *dest, uint32 stride, uint32 width, uint32 height) {
		if (in.m)
			return false;

		int		hshift		= planes.planes[1].hshift;
		int		vshift		= planes.planes[1].vshift;

		int		x_uv		= in.x >> hshift;
		int		y_uv		= in.y >> vshift;
		int		stride_y	= strides[0];
		int		stride_u	= strides[1];
		int		stride_v	= strides[2];

		if (fmt.is<YUYV>()) {
			for (int y = in.y, y1 = y + height; y < y1; y++, dest = (uint8*)dest + stride) {
				const uint8	*sy	= texels[0] + y * stride_y;
				const uint8	*su	= texels[1] + (y >> vshift) * stride_u; 
				const uint8	*sv	= texels[2] + (y >> vshift) * stride_v;

				YUYV	*d	= (YUYV*)dest;
				for (int x = in.x, x1 = x + width; x < x1; x += 2)
					*d++ = YUYV(sy[x], su[x >> hshift], sy[x + 1], sv[x >> vshift]);
			}
			return true;
		} else {
		#if 0
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
		#endif
			return false;
		}
	}

	void	*get_raw(uint32 plane, vbitmap_format *_fmt, uint32 *_stride, uint32 *_width, uint32 *_height) {
		if (plane < 4) {
			auto&	pfmt = planes.planes[plane];
			*_stride	= strides[plane];
			*_width		= width  >> pfmt.hshift;
			*_height	= height >> pfmt.vshift;
			*_fmt		= format.bitdepth(plane);
			return texels[plane];
		}
		return 0;
	}
};

ISO_DEFSAME(vbitmap_yuv, vbitmap);

}//namespace iso

#endif	//MOVIE_H