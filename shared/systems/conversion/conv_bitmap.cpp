#include "base/vector.h"
#include "channeluse.h"
#include "extra/filters.h"
#include "filetypes/bitmap/bitmap.h"
#include "codec/texels/dxt.h"
#include "iso/iso_convert.h"
#include "iso/iso_files.h"
#include "systems/vision/contours.h"
#include "systems/vision/grabcut.h"
#include "vq.h"

using namespace iso;

class ArrayTexture_conversion : public ISO_conversion {
public:
	ISO_ptr<void> operator()(ISO_ptr_machine<void> p, const ISO::Type* type, bool recurse) {
		//		if (type->SameAs<bitmap>() && p.GetType()->SameAs<ISO_openarray<ISO_ptr<bitmap> > >()) {
		if ((recurse || type->SameAs<bitmap>()) && p.GetType()->SameAs<ISO_openarray<ISO_ptr<bitmap> > >()) {
			ISO_openarray<ISO_ptr<bitmap> >* array = p;
			if (int n = array->Count()) {
				ISO_ptr<bitmap> bm0 = ISO_conversion::convert<bitmap>((*array)[0]);
				int				w   = bm0->Width();
				int				h   = bm0->Height();

				ISO_ptr<bitmap> bm(p.ID());
				bm->Create(w, h * n, 0, n);

				for (int i = 0; i < n; i++) {
					if (ISO_ptr<bitmap> bm1 = ISO_conversion::convert<bitmap>((*array)[i]))
						copy(bm1->All(), bm->Slice(i));
				}
				return bm;
			}
		}
		return ISO_NULL;
	}
	ArrayTexture_conversion() : ISO_conversion(this) {}
} arraytexture_conversion;

bitmap MakeBitmap(int width, int height, ISO_rgba colour) {
	bitmap bm(width, height);
	fill(bm.All(), colour);
	return bm;
}

bitmap MakeGradient(int width, int height, ISO_rgba colour1, ISO_rgba colour2) {
	bitmap	bm(width, height);
	ISO_rgba* p = bm.ScanLine(0);
	for (int x = 0; x < width; ++x) {
		p[x].r = (colour1.r * (width - x) + colour2.r * x) / width;
		p[x].g = (colour1.g * (width - x) + colour2.g * x) / width;
		p[x].b = (colour1.b * (width - x) + colour2.b * x) / width;
		p[x].a = (colour1.a * (width - x) + colour2.a * x) / width;
	}
	for (int y = 1; y < height; ++y)
		memcpy(bm.ScanLine(y), p, width * sizeof(ISO_rgba));

	return bm;
}

ISO_ptr<bitmap> NoMip(holder<ISO_ptr<bitmap> > bm) {
	if (bm.t)
		bm.t->SetFlags(BMF_NOMIP);
	return bm;
}

ISO_ptr<bitmap> ClampUV(holder<ISO_ptr<bitmap> > bm) {
	if (bm.t)
		bm.t->SetFlags(BMF_UVCLAMP);
	return bm;
}

ISO_ptr<bitmap2> Cubemap(holder<ISO_ptr<void> > p) {
	ISO_ptr<bitmap2> p2 = ISO_conversion::convert<bitmap2>(get(p));
	if (p2->GetType()->SameAs<HDRbitmap>()) {
		ISO_ptr<HDRbitmap> bm = *p2;
		bm->SetDepth(6);
	} else {
		ISO_ptr<bitmap> bm = *p2;
		bm->SetDepth(6);
	}
	return p2;
}

ISO_ptr<bitmap> VolumeBitmap(holder<ISO_ptr<bitmap> > bm) {
	if (bm.t)
		bm.t->SetFlags(BMF_VOLUME);
	return bm;
}

ISO_ptr<bitmap> ArrayBitmap(ISO_ptr<bitmap> bm, int depth) {
	if (bm)
		bm->SetDepth(depth);
	return bm;
}

ISO_ptr<bitmap2> PreMipped(ISO_ptr<void> p, int mips) {
	ISO_ptr<bitmap2> p2 = ISO_conversion::convert<bitmap2>(get(p));
	if (p2->GetType()->SameAs<HDRbitmap>()) {
		ISO_ptr<HDRbitmap> bm = *p2;
		bm->SetMips(mips ? mips : bm->MaxMips());
	} else {
		ISO_ptr<bitmap> bm = *p2;
		bm->SetMips(mips ? mips : bm->MaxMips());
	}
	return p2;
}

ISO_ptr<bitmap> PreMip(ISO_ptr<bitmap> bm, int mips) {
	int w = bm->BaseWidth(), h = bm->Height(), w2 = w * 2, m = bm->Mips();
	mips = min(mips ? mips : 1000, MaxMips(w, h));

	ISO_ptr<bitmap> bm2(NULL);
	bm2->Create(w2, h, bm->Flags());
	bm2->SetMips(mips);

	copy(bm->All(), bm2->All());
	if (m == 0)
		fill(bm2->Block(w, 0, w, h), 0);

	while (m < mips) {
		BoxFilter(bm2->Mip(m), bm2->Mip(m + 1), false);
		++m;
	}

	return bm2;
}

ISO_ptr<bitmap> SetMips(ISO_ptr<bitmap> bm0, ISO_ptr<bitmap> bm1) {
	int w0 = bm0->BaseWidth(), h0 = bm0->Height(), mips0 = bm0->Mips();
	int w1 = bm1->BaseWidth(), h1 = bm1->Height(), mips1 = bm1->Mips();

	int mip = log2(w0 / w1);
	if ((w1 << mip) != w0)
		return ISO_NULL;

	if (!mips0) {
		ISO_ptr<bitmap> bm2(NULL);
		bm2->Create(w0 * 2, h0, bm0->Flags());
		copy(bm0->All(), bm2->Mip(0));
		bm0 = bm2;
	}

	for (int i = 0; i <= mips1; ++i)
		copy(bm1->Mip(i), bm0->Mip(mip + i));

	return bm0;
}

bitmap BlendBitmap(holder<ISO_ptr<bitmap> > bm) {
	if (bm.t) {
		bitmap	bm2(bm->Width(), bm->Height(), bm->Flags(), bm->Depth());
		ISO_rgba* p = bm->ScanLine(0);
		ISO_rgba* d = bm2.ScanLine(0);
		for (int n = bm->Width() * bm->Height(); n--; p++, d++)
			*d = *p, *d *= p->a;
		return bm2;
	}
	return bitmap();
}

ISO_ptr<bitmap> AdditiveBitmap(holder<ISO_ptr<bitmap> > bm) {
	ISO_ptr<bitmap> bm2;
	if (bm.t) {
		bm2.Create(bm.t.ID())->Create(bm->Width(), bm->Height(), bm->Flags(), bm->Depth());
		ISO_rgba* p = bm->ScanLine(0);
		ISO_rgba* d = bm2->ScanLine(0);
		for (int n = bm->Width() * bm->Height(); n--; p++, d++) {
			*d = *p;
			*d *= d->a;
			d->a = 0;
		}
	}
	return bm2;
}

ISO_ptr<bitmap> MultiplyBitmap2(holder<ISO_ptr<bitmap> > bm) {
	if (bm.t) {
		ISO_rgba* p = bm->ScanLine(0);
		for (int n = bm->Width() * bm->Height(); n--; p++) {
			p->a = p->a * (255 - p->r) / 255;
			p->r = p->g = p->b = 0;
		}
	}
	return bm;
}
ISO_ptr<bitmap> MultiplyBitmap(holder<ISO_ptr<bitmap> > bm) {
	if (bm.t) {
		ISO_rgba* p = bm->ScanLine(0);
		for (int n = bm->Width() * bm->Height(); n--; p++) {
			p->a = (255 - p->a * p->r) / 255;
			p->r = p->g = p->b = 0;
		}
	}
	return bm;
}

ISO_ptr<bitmap> PreMultBitmap(holder<ISO_ptr<bitmap> > bm) {
	if (bm.t) {
		ISO_rgba* p = bm->ScanLine(0);
		for (int n = bm->Width() * bm->Height(); n--; p++) {
			if (p->a)
				*p /= p->a;
		}
	}
	return bm;
}

ISO_ptr<bitmap> SetChannel(ISO_ptr<bitmap> bm, int channel, int value) {
	if (bm) {
		ISO_ptr<bitmap> bm2(NULL);
		bm2->Create(bm->Width(), bm->Height(), bm->Flags());
		memcpy(bm2->ScanLine(0), bm->ScanLine(0), bm->Width() * bm->Height() * sizeof(ISO_rgba));
		uint8* p = (uint8*)bm2->ScanLine(0) + channel;
		for (int n = bm->Width() * bm->Height(); n--; p += 4)
			*p = value;
		return bm2;
	}
	return bm;
}

bitmap AddBlendBitmap(ISO_ptr<bitmap> add, ISO_ptr<bitmap> blend) {
	if (blend && add) {
		int		  w = add->Width(), h = add->Height();
		bitmap	bm(w, h, add->Flags(), add->Depth());
		ISO_rgba* a = add->ScanLine(0);
		ISO_rgba* b = blend->ScanLine(0);
		ISO_rgba* d = bm.ScanLine(0);
		for (int n = w * h; n--; a++, b++, d++) {
			*d   = *a * a->a + *b * b->a;
			d->a = b->a;
		}
		return bm;
	}
	return bitmap();
}

ISO_ptr<bitmap> NormalMap(ISO_ptr<bitmap> bm, float scale, int sphere) {
	if (scale == 0)
		scale = 1;

	int				width = bm->Width(), height = bm->Height();
	ISO_ptr<bitmap> bm2(NULL);
	bm2->Create(width, height, bm->Flags());

	for (int y = 0; y < height; y++) {
		ISO_rgba* line	 = bm->ScanLine(y);
		ISO_rgba* prevline = bm->ScanLine((y + height - 1) % height);
		ISO_rgba* nextline = bm->ScanLine((y + 1) % height);
		ISO_rgba* newline  = bm2->ScanLine(y);

		for (int x = 0; x < width; x++) {
			float dcx = (line[(x + width - 1) % width].r - line[(x + 1) % width].r) * scale;
			float dcy = (nextline[x].r - prevline[x].r) * scale;

			float r  = 1.0f / sqrt(dcx * dcx + dcy * dcy + 255 * 255);
			float nx = dcx * r;
			float ny = dcy * r;
			float nz = 255.0f * r;

			if (sphere) {
				float sx = 2 * (float(x) / width - .5f), sy = -2 * (float(y) / height - .5f);
				nx += sx;
				ny += sy;
				float t = square(nx) + square(ny);
				if (t < 1)
					nz = sqrt(1 - t);
			}

			newline[x] = ISO_rgba((unsigned char)(128 + 127 * nx), (unsigned char)(128 + 127 * ny), (unsigned char)(128 + 127 * nz), line[x].a);
		}
	}
	return bm2;
}

ISO_ptr<bitmap> AlphaThreshold(ISO_ptr<bitmap> bm, int a) {
	int				w = bm->Width(), h = bm->Height();
	ISO_ptr<bitmap> bm2(NULL);
	bm2->Create(w, h, bm->Flags());

	ISO_rgba* s = bm->ScanLine(0);
	ISO_rgba* d = bm2->ScanLine(0);
	for (int n = w * h; n--; s++, d++)
		*d = ISO_rgba(s->r, s->g, s->b, s->a < a ? 0 : 255);
	return bm2;
}

ISO_ptr<bitmap> Crop(ISO_ptr<bitmap> bm, int x, int y, int w, int h, ISO_rgba c) {
	int w0 = bm->Width(), h0 = bm->Height();

	if (w < 0) {
		w = -w;
		x += (w0 - w) / 2;
	}

	if (h < 0) {
		h = -h;
		y += (h0 - h) / 2;
	}

	ISO_ptr<bitmap> bm2(NULL);
	bm2->Create(w, h, bm->Flags());

	for (int yd = 0; yd < h; yd++) {
		ISO_rgba* dest = bm2->ScanLine(yd);
		int		  ys   = yd + y;
		if (ys < 0 || ys >= h0) {
			for (int xd = 0; xd < w; xd++)
				*dest++ = c;
		} else {
			int		  xs = max(x, 0), xd = 0;
			ISO_rgba* srce = bm->ScanLine(ys) + xs;
			for (; xd + x < 0; xd++)
				*dest++ = c;
			for (; xd < w && xd + x < w0; xs++, xd++)
				*dest++ = *srce++;
			for (; xd < w; xd++)
				*dest++ = c;
		}
	}
	return bm2;
}

ISO_ptr<bitmap> Combine(ISO_ptr<bitmap> bm1, ISO_ptr<bitmap> bm2, int dir) {
	int w1 = bm1->Width(), h1 = bm1->Height();
	int w2 = bm2->Width(), h2 = bm2->Height();
	int w, h, x, y;

	ISO_ptr<bitmap> bm3(NULL);

	if (dir == 0) {
		x = w1;
		y = 0;
		w = w1 + w2;
		h = max(h1, h2);
	} else {
		x = 0;
		y = h1;
		w = max(w1, w2);
		h = h1 + h2;
	}

	bm3->Create(w, h, bm1->Flags());
	copy(bm1->All(), bm3->All());
	copy(bm2->All(), bm3->All().sub<1>(x, w2).sub<2>(y, h2));
	return bm3;
}

struct VQ_rgba {
	typedef HDRpixel element, welement;
	HDRpixel*	vec;
	uint32		count;

	HDRpixel&	data(size_t i)		{ return vec[i]; }
	uint32		size()				{ return count; }

	float		weightedsum(HDRpixel& s, size_t i) {
		s += vec[i];
		return 1;
	}

	static void  reset(HDRpixel& s)				{ clear(s); }
	static void  scale(HDRpixel& e, float f)	{ e *= f; }
	static float norm(const HDRpixel& e)		{ return sqrt(e.r * e.r + e.g * e.g + e.b * e.b); }

	static float distsquared(const HDRpixel& a, const HDRpixel& b, float max = 1e38f) { return (square(a.r - b.r) + square(a.g - b.g) + square(a.b - b.b)); }

	VQ_rgba(HDRpixel* vec, uint32 size) : vec(vec), count(size) {}
};

ISO_ptr<bitmap> VQbitmap(ISO_ptr<HDRbitmap> bm, int n) {
	if (n == 0)
		n = 256;

	int			width		= bm->Width();
	int			height		= bm->Height();
	int			num_vecs	= width * height;
	HDRpixel*	vectors		= bm->ScanLine(0);

	vq<VQ_rgba> vqt(n);
	VQ_rgba		t(vectors, num_vecs);
	vqt.build(t);

	ISO_ptr<bitmap> bm2(none, width, height);
	copy(vqt.codebook, bm2->CreateClut(n));

	temp_array<uint32> index(num_vecs);
	vqt.generate_indices(t, index);
	auto	d	= bm2->ScanLine(0);
	for (int i = 0; i < num_vecs; i++)
		*d++ = int(index[i]);

	return bm2;
}

#if 0
double _kmeans(block<float,2> data, int K, auto_block<int,1> &labels, auto_block<float, 2> &centres, int maxCount, int attempts, int flags);
double _kmeans(block<array_vec<float,4>,1> data, int K, auto_block<int,1> &labels, auto_block<array_vec<float,4>, 1> &centres, int maxCount, int attempts, int flags);

ISO_ptr<bitmap> kmeans_bitmap(ISO_ptr<bitmap> bm, int n) {
	if (n == 0)
		n = 256;

	int			width		= bm->Width();
	int			height		= bm->Height();
	int			num_vecs	= width * height;

	auto_block<int,1>		labels;

#if 1
	auto_block<float, 2>	data = make_auto_block<float>(4, num_vecs);
	copy(make_block((uint8*)bm->ScanLine(0), 4, num_vecs), data);

	auto_block<float, 2>	centres;
	_kmeans(data, n, labels, centres, 10, 0, 0);

	ISO_ptr<bitmap>	bm2(0);
	bm2->Create(width, height);
	bm2->CreateClut(n);
	for (int i = 0; i < n; i++)
		bm2->Clut(i) = ISO_rgba(*(HDRpixel*)centres[i].begin() / 255.f);

#else
	auto_block<HDRpixel, 1>	data = make_auto_block<HDRpixel>(num_vecs);
	copy(make_block(bm->ScanLine(0), num_vecs), data);

	auto_block<HDRpixel, 1>	centres;
	_kmeans((block<array_vec<float,4>, 1>&)data, n, labels, (auto_block<array_vec<float,4>, 1>&)centres, 10, 0, 0);

	ISO_ptr<bitmap>	bm2(0);
	bm2->Create(width, height);
	bm2->CreateClut(n);
	for (int i = 0; i < n; i++)
		bm2->Clut(i) = centres[i];
#endif

	for (int i = 0; i < num_vecs; i++)
		bm2->ScanLine(0)[i] = labels[i];

	return bm2;
}
#endif

ISO_ptr<void> grabcut(ISO_ptr<bitmap> bm, int x, int y, int w, int h, int iterCount) {
	iso::grabCut(force_cast<block<rgbx8, 2> >(bm->All()), x, y, w, h, iterCount);

	static const uint8 lookup[] = {
		0x00,  // an obvious background pixel
		0xff,  // an obvious foreground pixel
		0x00,  // a possible background pixel
		0xff,  // a possible foreground pixel
	};
	for (auto *p = bm->ScanLine(0), *e = p + bm->Width() * bm->Height(); p != e; ++p)
		p->a = lookup[p->a];

	auto_block<int, 2> image = make_auto_block<int>(bm->Width(), bm->Height());
	for_each2(bm->All(), (block<int, 2>&)image, [](const ISO_rgba& s, int& d) { d = s.a != 0; });

#if 0
	contour<point> root	= SuzukiContour(image);
	int				maxv	= 0;
	contour<point>	*maxc	= 0;
	for (auto &i : root.children) {
		if (i.points.size() > maxv) {
			maxv = i.points.size32();
			maxc = &i;
		}
	}

	dynamic_array<position2> pos = maxc->points;
	int	n = optimise_polyline(pos, pos.size32(), 1.0f, true);

	ISO_ptr<pair<ISO_ptr<bitmap>, ISO_openarray<float2p> > > r(0);
	r->a = bm;
	copy_n(pos.begin(), r->b.Create(n).begin(), n);
	return r;
#else
	return bm;
#endif
}

typedef float2p float2c;
template<typename T> ISO_DEFUSERCOMPVT(contour, T, children, points);


#if 0
ISO_ptr<ISO_openarray<float2p> > suzuki(ISO_ptr<bitmap> bm, float eps) {
	auto_block<int,2>	image = make_auto_block<int>(bm->Width(), bm->Height());
	for_each2(bm->All(), (block<int,2>&)image, [](const ISO_rgba &s, int &d) {
		d = s.r < 128;
	});

	contour<point> *root	= SuzukiContour(image);
	int				maxv	= 0;
	contour<point>	*maxc	= 0;
	for (auto &i : root->children()) {
		if (i.points.size() > maxv) {
			maxv = i.points.size();
			maxc = &i;
		}
	}

	dynamic_array<position2> pos = maxc->points;
	int	n = optimise_polyline(pos, pos.size(), eps, true);

	ISO_ptr<ISO_openarray<float2p> >	p(0, n);
	copy_n(pos.begin(), p->begin(), n);
	return p;
}
#else

ISO_ptr<void> suzuki(ISO_ptr<bitmap> bm, float eps) {
	auto_block<int, 2> image = make_auto_block<int>(bm->Width(), bm->Height());
	for_each2(bm->All(), (block<int, 2>&)image, [](const ISO_rgba& s, int& d) { d = s.r < 128; });

	contour<point> root = SuzukiContour(image);
	contour<float2c> root2 = root;

	return ISO::MakePtr(0, root2);
}
#endif

ISO_ptr<bitmap2> RawBitmap(ISO_ptr<void> p, string format, int width, int height, int depth) {
	const ISO::Type* type = p.GetType()->SkipUser();
	if (TypeType(type) == ISO::STRING) {
		filename fn = FileHandler::FindAbsolute(ISO::Browser2(p).GetString());
		if (!fn.exists())
			return ISO_NULL;

		uint64						   len = filelength(fn);
		ISO_ptr<ISO_openarray<uint8> > p2(0);
		FileInput(fn).readbuff(p2->Create(len, false), len);
		p = p2;

	} else if (TypeType(type) != ISO::OPENARRAY || ((ISO::TypeOpenArray*)type)->subtype->GetType() != ISO::INT) {
		return ISO_NULL;
	}

	ChannelUse			cu(format);
	ChannelUse::chans	bits = cu.analog & cu.all(channels::SIZE_MASK);
	uint32				bpp  = cu.IsCompressed() ? (bits.a == 1 ? 4 : 8) : bits.r + bits.g + bits.b + bits.a;

	ISO_openarray<void>* array = p;
	void*				srce  = *array;
	uint32				size  = array->Count() * ((ISO::TypeOpenArray*)type)->subsize;
	uint32				pitch = width * bpp / 8;

	if (!depth)
		depth = 1;

	if (!height)
		height = size / pitch;
	else
		height *= depth;

	if (cu.IsFloat()) {
		ISO_ptr<HDRbitmap> bm;
		bm.Create()->Create(width, height, depth);
		if (cu.IsCompressed()) {
			copy(make_strided_block((const BC<6>*)srce, width, pitch, height), bm->All());
		} else {
			HDRChannelUseIterator i(cu, srce);
			copy_n(i, bm->ScanLine(0), width * height);
		}
		return ISO_ptr<bitmap2>(0, bm);

	} else {
		ISO_ptr<bitmap> bm;
		bm.Create()->Create(width, height, depth);

		if (cu.IsCompressed()) {
			width >>= 2;
			height >>= 2;
			switch (bits.a) {
				case 0:
					if (cu.NumChans() == 1)
						copy(make_block((const BC<4>*)srce, width, height), bm->All());
					else
						copy(make_block((const BC<5>*)srce, width, height), bm->All());
					break;
				case 1: copy(make_block((const BC<1>*)srce, width, height), bm->All()); break;
				case 4: copy(make_block((const BC<2>*)srce, width, height), bm->All()); break;
				case 5: copy(make_block((const BC<7>*)srce, width, height), bm->All()); break;
				case 8: copy(make_block((const BE(BC<3>)*)srce, width, height), bm->All()); break;
			}
		} else {
			ChannelUseIterator i(cu, srce);
			copy_n(i, bm->ScanLine(0), width * height);
		}
		return ISO_ptr<bitmap2>(0, bm);
	}
}

ISO_ptr<bitmap> ContactSheet(const ISO_openarray<ISO_ptr<bitmap>> &bms, int w, int h, ISO_rgba c) {
	int				n  = bms.Count();
	int				nx = sqrt(n), ny = (n + nx - 1) / nx;
	ISO_ptr<bitmap> bm(0);
	bm->Create(nx * w, ny * h);
	fill(bm->All(), c);

	for (int i = 0; i < n; i++) {
		bitmap* b0 = bms[i];
		int		y0 = i / nx, x0 = i - y0 * nx;
		resample(bm->Block(x0 + w / 8, y0 + h / 8, w * 3 / 4, h * 3 / 4), b0->All());
	}

	return bm;
}

//-----------------------------------------------------------------------------
//	HDR manipulation
//-----------------------------------------------------------------------------

float gamma_component(float f, float gamma) {
	return f == 0 ? 0 : f < 0 ? -pow(-f, gamma) : pow(f, gamma);
}

ISO_ptr<HDRbitmap> Gamma(ISO_ptr<HDRbitmap> bm, float gamma) {
	if (gamma != 1) {
		HDRpixel* p = bm->ScanLine(0);
		for (int n = bm->Width() * bm->Height(); n--; p++) {
			p->r = gamma_component(p->r, gamma);
			p->g = gamma_component(p->g, gamma);
			p->b = gamma_component(p->b, gamma);
		}
	}
	return bm;
}

ISO_ptr<HDRbitmap> Exposure(ISO_ptr<HDRbitmap> bm, float scale) {
	HDRpixel* p = bm->ScanLine(0);

	for (int n = bm->Width() * bm->Height(); n--; p++) {
		p->r = p->r * scale;
		p->g = p->g * scale;
		p->b = p->b * scale;
	}
	return bm;
}

ISO_ptr<bitmap> HDR2AlphaExponent(ISO_ptr<HDRbitmap> bm, float p, int dxt) {
	if (p == 0)
		p = 2;

	ISO_ptr<bitmap> bm2(NULL);
	int				width = bm->Width(), height = bm->Height();
	bm2->Create(width, height, bm->Flags(), bm->Depth());

	HDRpixel* s = bm->ScanLine(0);
	ISO_rgba* d = bm2->ScanLine(0);
#if 0
	for (int n = bm->Width() * bm->Height(); n--; s++, d++) {
		iorf	m	= max(max(s->r, s->g), s->b);
		for (int i = 0; i < 3; i++) {
			iorf	a((*s)[i]);
			(*d)[i] = (a.m | (1<<23)) >> (m.e - a.e + 16);
		}
		d->a = m.e + 2;//- 128 + COLXS + 8;
	}
#else
	float rlogp = 1.0f / ln(p);
	for (int n = width * height; n--; s++, d++) {
		int e = ceil(ln(max(max(s->r, s->g), s->b)) * rlogp);
		if (e > 127)
			e = 127;
		float m = 255 * pow(p, -e);
		for (int i = 0; i < 3; i++)
			(*d)[i] = min(int((*s)[i] * m), 255);
		d->a = e + 128;
	}
#endif
#ifndef ISO_EDITOR
	if (dxt) {
		for (int y = 0; y < height; y += 4) {
			for (int x = 0; x < width; x += 4) {
				uint8 alpha[16], alpha2[16];

				block<ISO_rgba, 2> block = bm2->Block(x, y, 4, 4);
				for (uint32 i = 0, m = block_mask<4, 4>(block.size<1>(), block.size<2>()); m; i++, m >>= 1)
					alpha[i] = m & 1 ? block[i >> 2][i & 3].a : 0;

				DXTa dxta;
				dxta.Encode(alpha);
				dxta.Decode(alpha2);

				for (int i = 0; i < 16; i++) {
					ISO_rgba& c = block[i >> 2][i & 3];
					int		  d = (int)alpha2[i] - (int)alpha[i];
					if (d > 0) {
						for (int i = 0; i < 3; i++)
							c[i] >>= d;
					} else if (d < 0) {
						for (int i = 0; i < 3; i++)
							c[i] <<= -d;
					}
				}
			}
		}
	}
#endif
	return bm2;
}

ISO_ptr<HDRbitmap> AlphaExponent2HDR(ISO_ptr<bitmap> bm, float p) {
	if (p == 0)
		p = 2;
	ISO_ptr<HDRbitmap> bm2(NULL);
	bm2->Create(bm->Width(), bm->Height(), bm->Flags(), bm->Depth());

	ISO_rgba* s = bm->ScanLine(0);
	HDRpixel* d = bm2->ScanLine(0);
#if 0
	for (int n = bm->Width() * bm->Height(); n--; s++, d++) {
		int	e	= s->a - 128 - 8;
		for (int i = 0; i < 3; i++) {
			iorf	a((float)(*s)[i]);
			a.e += e;
			(*d)[i] = a.f;
		}
		d->a = 1;
	}
#else
	for (int n = bm->Width() * bm->Height(); n--; s++, d++) {
		float m = pow(p, s->a - 128) / 255;
		for (int i = 0; i < 3; i++)
			(*d)[i] = (*s)[i] * m;
		d->a = 1;
	}
#endif
	return bm2;
}

ISO_ptr<bitmap> HDR2AlphaMult(holder<ISO_ptr<HDRbitmap> > bm) {
	ISO_ptr<bitmap> bm2(NULL);
	bm2->Create(bm->Width(), bm->Height(), bm->Flags(), bm->Depth());

	HDRpixel* s = bm->ScanLine(0);
	ISO_rgba* d = bm2->ScanLine(0);

	for (int n = bm->Width() * bm->Height(); n--; s++, d++) {
		int m = min(int(max(max(s->r, s->g), s->b) + 1.f), 255);
		for (int i = 0; i < 3; i++)
			(*d)[i] = min(int((*s)[i] * 255 / m), 255);
		d->a = m;
	}
	return bm2;
}

ISO_ptr<HDRbitmap> _Resize(ISO_ptr<HDRbitmap> bm, int destw, int desth, float gamma) {
	if (gamma)
		Gamma(bm, gamma);

	ISO_ptr<HDRbitmap> bm2(0);
	if (int mips = bm->Mips()) {
		bm2->Create(destw * 2, desth, bm->Flags(), bm->Depth());
		bm2->SetMips(mips);
		for (int i = 0; i < mips; i++)
			resample(bm2->Mip(i), bm->Mip(i));
	} else {
		bm2.Create()->Create(destw, desth, bm->Flags(), bm->Depth());
		resample(bm2->All(), bm->All());
	}

	if (gamma)
		Gamma(bm2, 1 / gamma);

	return bm2;
}

ISO_ptr<HDRbitmap> Resize(ISO_ptr<HDRbitmap> bm, int destw, int desth, float gamma) {
	int srcew = bm->Width(), srceh = bm->BaseHeight();

	if (destw < 0 && desth < 0) {
		if (destw * srceh < desth * srcew) {
			desth = -desth;
			destw = srcew * desth / srceh;
		} else {
			destw = -destw;
			desth = srceh * destw / srcew;
		}
	} else {
		if (destw <= 0) {
			if (destw == 0 || -destw * srceh < desth * srcew) {
				destw = srcew * abs(desth) / srceh;
			} else {
				destw = -destw;
				desth = srceh * destw / srcew;
			}
		}

		if (desth <= 0) {
			if (desth == 0 || -desth * srcew < destw * srceh) {
				desth = srceh * destw / srcew;
			} else {
				desth = -desth;
				destw = srcew * desth / srceh;
			}
		}
	}
	return _Resize(bm, destw, desth * bm->Depth(), gamma);
}

ISO_ptr<HDRbitmap> Scale(ISO_ptr<HDRbitmap> bm, float scale, float gamma) {
	return _Resize(bm, bm->Width() * scale, bm->Height() * scale, gamma);
}

//-----------------------------------------------------------------------------
//	init
//-----------------------------------------------------------------------------

static initialise init(
	ISO_get_operation(MakeBitmap),
	ISO_get_operation(MakeGradient),
	ISO_get_operation(NoMip),
	ISO_get_operation(ClampUV),
	ISO_get_operation(Cubemap),
	ISO_get_operation(VolumeBitmap),
	ISO_get_operation(ArrayBitmap),
	ISO_get_operation(PreMipped),
	ISO_get_operation(PreMip),
	ISO_get_operation(BlendBitmap),
	ISO_get_operation(AdditiveBitmap),
	ISO_get_operation(PreMultBitmap),
	ISO_get_operation(MultiplyBitmap),
	ISO_get_operation(MultiplyBitmap2),
	ISO_get_operation(SetChannel),
	ISO_get_operation(AddBlendBitmap),
	ISO_get_operation(NormalMap),
	ISO_get_operation(AlphaThreshold),
	ISO_get_operation(Crop),
	ISO_get_operation(Combine),
	ISO_get_operation(VQbitmap),
	//	ISO_get_operation(kmeans_bitmap),
	ISO_get_operation(grabcut),
	ISO_get_operation(suzuki),

	ISO_get_operation(RawBitmap),
	ISO_get_operation(ContactSheet),

	ISO_get_operation(Gamma),
	ISO_get_operation(Exposure),
	ISO_get_operation(HDR2AlphaExponent),
	ISO_get_operation(AlphaExponent2HDR),
	ISO_get_operation(HDR2AlphaMult),
	ISO_get_operation(Resize),
	ISO_get_operation(Scale)
);
