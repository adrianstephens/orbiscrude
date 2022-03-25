#include "codec/texels/dxt.h"

namespace iso {

struct vbitmap_GEN : vbitmap {
	malloc_block	data;
	vbitmap_GEN(const pair<vbitmap_format, bitmap*> &p);
	vbitmap_GEN(const pair<vbitmap_format, HDRbitmap*> &p);
	template<typename T> void	make_data(const block<T, 2> &src);
	bool	get(const vbitmap_loc &in, vbitmap_format fmt, void *dest, uint32 stride, uint32 width, uint32 height);
	void*	get_raw(uint32 plane, vbitmap_format *fmt, uint32 *stride, uint32 *width, uint32 *height) { return 0; }
};

uint32 mips_size(uint32	bpp, uint32 w, uint32 h, uint32 d, uint32 m) {
	uint32	total = 0;
	for (uint32 i = 0; i < m; i++) {
		uint32	wm = max(w >> i, 1), hm = max(h >> i, 1), dm = max(d >> i, 1);
		total += (wm * bpp + 7) /8 * hm * dm;;
	}
	return total;
}

force_inline uint32 get_component(uint8 v, uint32 scale) {
	return clamp(v * scale / 255, 0, scale);
}

force_inline uint32 get_component(float v, uint32 scale) {
	return clamp(int(v * scale), 0, scale);
}

template<typename T> void fill_data(vbitmap_format format, const block<T, 2> &src, const memory_block &dest) {
	uint32	w		= src.template size<1>(), h = src.template size<2>();
	uint32	bpp		= format.bits();
	uint32	stride	= (bpp * w + 7) / 8;

	uint8	rshift	= 0;
	uint8	gshift	= rshift + format.channel_bits(0);
	uint8	bshift	= gshift + format.channel_bits(1);
	uint8	ashift	= bshift + format.channel_bits(2);

	uint32	rscale	= bits(format.channel_bits(0));
	uint32	gscale	= bits(format.channel_bits(1));
	uint32	bscale	= bits(format.channel_bits(2));
	uint32	ascale	= bits(format.channel_bits(3));

	for (int y = 0; y < h; y++) {
		void	*d	= (uint8*)dest + stride * y;
		const T	*s	= src[y];

		if (bpp <= 32) {
			for (int x = 0; x < w; x++) {
				uint32	a = get_component(s->r, rscale) << rshift
						|	get_component(s->g, gscale) << gshift
						|	get_component(s->b, bscale) << bshift
						|	get_component(s->a, ascale) << ashift;

				write_bits(d, a, x * bpp, bpp);
			}
		} else {
			for (int x = 0; x < w; x++) {
				uint64	a = (uint64)get_component(s->r, rscale) << rshift
						|	(uint64)get_component(s->g, gscale) << gshift
						|	(uint64)get_component(s->b, bscale) << bshift
						|	(uint64)get_component(s->a, ascale) << ashift;

				write_bits(d, a, x * bpp, bpp);
			}
		}
	}
}

void fill_data2(vbitmap_format format, const block<ISO_rgba, 2> &src, const memory_block &dest) {
	uint32	w	= src.size<1>(), h = src.size<2>();
	switch (format) {
		case vbitmap_format::DXT1:
			w = (w + 3) / 4;
			copy(src, make_block((BC<1>*)dest, w, (h + 3) / 4));
			break;
		case vbitmap_format::DXT3:
			w = (w + 3) / 4;
			copy(src, make_block((BC<2>*)dest, w, (h + 3) / 4));
			break;
		case vbitmap_format::DXT5:
			w = (w + 3) / 4;
			copy(src, make_block((BC<3>*)dest, w, (h + 3) / 4));
			break;
		default:
			fill_data(format, src, dest);
			break;
	}
}

vbitmap_GEN::vbitmap_GEN(const pair<vbitmap_format, bitmap*> &p) : vbitmap(this, p.a, p.b->BaseWidth(), p.b->BaseHeight(), p.b->Depth()) {
	flags		= p.b->Flags();
	mips		= p.b->Mips();

	uint32	bpp	= format.bits();
	uint32	w	= width, h = height;
	if (format.is_compressed()) {
		w = (w + 3) / 4;
		h = (h + 3) / 4;
	}

	uint32	total	= mips_size(bpp, w, h, depth, mips + 1);
	data.create(total);

	if (mips) {
		for (int m = 0; m <= mips; m++) {
			uint32	offset	= mips_size(bpp, w, h, depth, m);
			fill_data2(format, p.b->Mip(m), data.slice(offset));
		}
	} else {
		fill_data2(format, p.b->All(), data);
	}
}

vbitmap_GEN::vbitmap_GEN(const pair<vbitmap_format, HDRbitmap*> &p) : vbitmap(this, p.a, p.b->BaseWidth(), p.b->BaseHeight(), p.b->Depth()) {
	flags		= p.b->Flags();
	mips		= p.b->Mips();

	uint32	bpp		= format.bits();
	uint32	total	= mips_size(bpp, width, height, depth, mips + 1);
	data.create(total);

	if (mips) {
		for (int m = 0; m <= mips; m++) {
			uint32	offset	= mips_size(bpp, width, height, depth, m);
			fill_data(format, p.b->Mip(m), data.slice(offset));
		}
	} else {
		fill_data(format, p.b->All(), data);
	}
}

bool vbitmap_GEN::get(const vbitmap_loc &in, vbitmap_format fmt, void *dest, uint32 stride, uint32 width, uint32 height) {
	uint32	x	= in.x;
	uint32	y	= in.y;
	uint32	z	= in.z;
	uint32	w	= vbitmap::width;
	uint32	h	= vbitmap::height;
	uint32	d	= flags & BMF_VOLUME ? depth : 1;

	uint32	bpp	= format.bits();
	if (format.is_compressed()) {
		x	/= 4;
		y	/= 4;
		w	= (w + 3) / 4;
		h	= (h + 3) / 4;
	}

	uint32	offset	= z ? mips_size(bpp, w, h, d, mips + 1) * z : 0;

	if (in.m) {
		offset += mips_size(bpp, w, h, d, in.m);
		w		= max(w >> in.m, 1);
	}

	uint32	s	= (w * format.bits() + 7) / 8;
	offset		+= x + y * s;

	if (!fmt || fmt == vbitmap::format) {
		uint32	dest_width = fmt ? (width * fmt.bits() + 7) / 8 : width;
		for (int y = 0; y < height; y++, offset += s, dest = (uint8*)dest + stride)
			memcpy(dest, (uint8*)data + offset, dest_width);
		return true;

	} else {
		return false;
	}
}

ISO_ptr<vbitmap> MakeVBitmap(ISO_ptr<void> p, vbitmap_format fmt) {
	if (p.IsType<HDRbitmap>())
		return ISO_ptr<vbitmap_GEN>(p.ID(), make_pair(move(fmt), (HDRbitmap*)p));
	else if (p.IsType<bitmap>())
		return ISO_ptr<vbitmap_GEN>(p.ID(), make_pair(move(fmt), (bitmap*)p));
	else
		return ISO_NULL;
}


}//namespace iso

ISO_DEFSAME(iso::vbitmap_GEN, iso::vbitmap);

