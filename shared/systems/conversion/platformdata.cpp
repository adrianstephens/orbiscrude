#include "platformdata.h"
#include "filename.h"
#include "iso/iso_files.h"
#include "iso/iso_binary.h"
#include "scenegraph.h"
#include "systems/mesh/patch.h"
#include "systems/mesh/model_iso.h"

#ifdef ISO_EDITOR
#include "systems/communication/connection.h"
#endif

ISO::ptr<iso::Model3> OptimiseSkinModel(ISO::ptr<iso::Model3> model, float weight_threshold, float verts_threshold);
ISO::ptr<void> ReadDX11FX(ISO::tag id, iso::istream_ref file, const iso::filename *fn);

namespace iso {

struct SampleBuffer;
#if !defined ISO_EDITOR || defined(PLAT_MAC)
ISO_INIT(SampleBuffer)		{}
ISO_DEINIT(SampleBuffer)	{}
ISO_INIT(PatchModel3)		{}
ISO_DEINIT(PatchModel3)		{}
ISO_INIT(Model3)			{}
ISO_DEINIT(Model3)			{}
#endif
#if defined(ISO_EDITOR) && (defined(PLAT_METAL) || defined(PLAT_MAC))
ISO_INIT(DataBuffer)		{}
ISO_DEINIT(DataBuffer)		{}
ISO_INIT(Texture)			{}
ISO_DEINIT(Texture)			{}
ISO_INIT(pass)				{}
ISO_DEINIT(pass)			{}
#endif

//-----------------------------------------------------------------------------
//	Platform
//-----------------------------------------------------------------------------

ISO_ptr<Model3> OptimiseModel(ISO_ptr<Model3> p) {
	return FileHandler::ExpandExternals(OptimiseSkinModel(p, ISO::root("variables")["weight_threshold"].Get(0.01f), ISO::root("variables")["verts_threshold"].Get(0.5f)));
}

Platform	*Platform::current;

class PlatformDefault : public Platform {
public:
	PlatformDefault() : Platform(0) {}
} platform_default;

Platform::type Platform::Set(const char *_platform) {
	if (_platform) {
		for (iterator i = begin(); i != end(); ++i) {
			if (str(_platform) == i->platform) {
				ISO_get_conversion(OptimiseModel);
//				ISO::getdef<Model3>()->flags |= ISO::TypeUser::CHANGE | ISO::TypeUser::WRITETOBIN;
				current = i;
				return i->Set();
			}
		}
	}
//	ISO::getdef<Model3>()->flags &= ~(ISO::TypeUser::CHANGE | ISO::TypeUser::WRITETOBIN);
	Redirect<DataBuffer,	void>();
	Redirect<Texture,		void>();
	Redirect<SampleBuffer,	void>();
	Redirect<SubMeshPtr,	void>();
	current = NULL;
	return PT_NOTFOUND;
}

Platform *Platform::Get(const char *_platform) {
	if (_platform) {
		for (iterator i = begin(); i != end(); ++i) {
			if (str(_platform) ==  i->platform)
				return i;
		}
	}
	return &platform_default;
}

ISO_ptr<void> Platform::ReadFX(tag id, istream_ref file, const filename *fn) {
#if defined ISO_EDITOR && !defined PLAT_MAC
#ifdef USE_DX11
	return ReadDX11FX(id, file, fn);
#else
	ISO_ptr<void> ReadDX9FX(tag id, istream_ref file, const filename *fn);
	return ReadDX9FX(id, file, fn);
#endif
#else
	return ISO_NULL;
#endif
}

ISO_ptr<bitmap2> Texture2Bitmap(ISO_ptr<bitmap> bm) {
	if (bm.IsExternal())
		return ISO_ptr<bitmap2>(0, FileHandler::ReadExternal(bm.External()));
	return ISO_ptr<bitmap2>(0, bm);
}

ISO_ptr<bitmap2> HDRTexture2Bitmap(ISO_ptr<HDRbitmap> bm) {
	if (bm.IsExternal())
		return ISO_ptr<bitmap2>(0, FileHandler::ReadExternal(bm.External()));
	return ISO_ptr<bitmap2>(0, bm);
}

ISO_ptr<void> ForPlatform(const char *platform, ISO_ptr<void> p) {
	ISO::Browser	vars	= ISO::root("variables");
	const char *exportfor	= vars["exportfor"].GetString();
	int		save_mode		= ISO::binary_data.SetMode(1);

	vars.SetMember("exportfor", platform);

	const ISO::Type	*type = p.GetType();
	if (type->SameAs<string>()) {
		p = FileHandler::ReadExternal((char*)p.GetType()->ReadPtr(p));
		type = p.GetType();
	}

	p = FileHandler::ExpandExternals(p);

	Platform::Set(platform);
	p = ISO_conversion::convert(p, type, ISO_conversion::RECURSE | ISO_conversion::FULL_CHECK | ISO_conversion::EXPAND_EXTERNALS);

	vars.SetMember("exportfor", exportfor);
	Platform::Set(0);
	ISO::binary_data.SetMode(save_mode);
	return p;
}

//-----------------------------------------------------------------------------
//	PlatformMemory
//-----------------------------------------------------------------------------

PlatformMemory::PlatformMemory(uint32 offset, uint32 size) {
#if defined ISO_EDITOR && !defined PLAT_MAC
	if (const char *target = ISO::binary_data.RemoteTarget()) {
		memory_block::operator=(memory_block(GetTargetMemory(target, size, offset).detach(), size));
	} else
#endif
	{
		void	*e		= ISO::iso_bin_allocator().alloc(0, 1);
		uint32	total	= vram_offset(e);
		ISO_ASSERT(offset + size <= total);
		memory_block::operator=(memory_block((uint8*)e - total + offset, size));
	}
}

PlatformMemory::~PlatformMemory() {
#if defined ISO_EDITOR && !defined PLAT_MAC
	if (const char *target = ISO::binary_data.RemoteTarget())
		free(p);
#endif
}

//-----------------------------------------------------------------------------
//	cache
//-----------------------------------------------------------------------------

fixed_string<256> cache_name(bitmap *bm) {
	uint32	hash = 0, *p = (uint32*)bm->ScanLine(0);
	for (uint32 n = bm->Width() * bm->Height(); n--; hash += *p++);
	return format_string<256>("%04x_%04x_%08x", bm->BaseWidth(), bm->BaseHeight(), hash);
}

cache_filename::cache_filename(ISO_ptr_machine<void> p) {
	if (const char *cache = ISO::root("variables")["cache"].GetString()) {
		cached = filename(cache).cleanup();
		if (const char *s = FileHandler::FindInCache(p)) {
			source	= s;
			filename fn = filename(s).add_ext(Platform::GetCurrent()).add_ext("ib").relative_to(cached);
			const char *d = fn;
			while (str(d).begins(".."))
				d += 3;
			cached.add_dir(d);
		} else {
			if (p.IsType<bitmap>())
				cached.add_dir(cache_name(p)).add_ext(Platform::GetCurrent()).add_ext("ib");
			else
				cached.clear();
		}
	}
}

cache_filename::cache_filename(const filename &_source) {
	if (const char *cache = ISO::root("variables")["cache"].GetString()) {
		cached	= filename(cache).cleanup();
		source	= _source;
		filename fn = filename(source).add_ext(Platform::GetCurrent()).add_ext("ib").relative_to(cached);
		const char *d = fn;
		while (str(d).begins(".."))
			d += 3;
		cached.add_dir(d);
	}
}

//-----------------------------------------------------------------------------
//	bitmap tests
//-----------------------------------------------------------------------------

bool IsMonochrome(const block<ISO_rgba, 2> &b, float tolerance, ISO_rgba *col) {
	uint32		bestcomp = 0, comp;
	ISO_rgba	c;
	for (block<ISO_rgba, 2>::iterator i = b.begin(), e = b.end(); i != e; ++i) {
		for (ISO_rgba *p = i.begin(), *pe = i.end(); p != pe; ++p) {
			if (p->a && (comp = max(max(p->r, p->g), p->b)) > bestcomp) {
				bestcomp = comp;
				c	= *p;
			}
		}
	}

	int		m0 = c.MaxComponent(), m1 = (m0 + 1) % 3, m2 = (m0 + 2) % 3;
	int		c0 = c[m0], c1 = c[m1], c2 = c[m2];
	int		t0 = tolerance * c0 * c0;
//	int		t1 = tolerance * c0 * c1, t2 = tolerance * c0 * c2;

	for (block<ISO_rgba, 2>::iterator i = b.begin(), e = b.end(); i != e; ++i) {
		for (ISO_rgba *p = i.begin(), *pe = i.end(); p != pe; ++p) {
			if (p->a) {
				if (abs(c0 * (*p)[m1] - c1 * (*p)[m0]) > t0)
					return false;
				if (abs(c0 * (*p)[m2] - c2 * (*p)[m0]) > t0)
					return false;
			}
		}
	}

	bool	alpha = true;
//	int		ta = tolerance * c0 * c.a;
	for (block<ISO_rgba, 2>::iterator i = b.begin(), e = b.end(); alpha && i != e; ++i) {
		for (ISO_rgba *p = i.begin(), *pe = i.end(); p != pe; ++p) {
			if (abs(c0 * p->a - c.a * (*p)[m0]) > t0) {
				alpha = false;
				break;
			}
		}
	}

	if (col) {
		int		v = c[alpha ? c.MaxComponentA() : c.MaxComponent()];
		for (int i = 0; i < 4; i++)
			c[i] = c[i] * 255 / v;
		if (!alpha)
			c[3] = 0;
		*col = c;
	}
	return true;
}

void MakeMonochrome(bitmap &bm, const ISO_rgba &col) {
	int			comp	= col.MaxComponentA();
	int			num		= bm.Width() * bm.Height();
	ISO_rgba	*p		= bm.ScanLine(0);
	if (col.a == 0) {
		while (num--) {
			p->r = p->g = p->b = (*p)[comp];
			p++;
		}
	} else {
		while (num--) {
			p->r = p->g = p->b = p->a = (*p)[comp];
			p++;
		}
	}
}

uint32	from_morton(uint32 v, uint32 b0, uint32 b1) {
	uint16	x, y;
	if (b0 < b1) {
		x = even_bits(v) & bits(b0);
		y = (even_bits(v >> 1) & bits(b0)) | ((v >> b0) & ~bits(b0));
	} else if (b0 > b1) {
		x = (even_bits(v) & bits(b1)) | ((v >> b1) & ~bits(b1));
		y = even_bits(v >> 1) & bits(b1);
	} else {
		x = even_bits(v);
		y = even_bits(v >> 1);
	}
	return (y << 16) | x;
}

rectangles<256> FindRegions(const bitmap &bm, int loglimit) {
	uint16	w	= bm.Width(),	h	= bm.Height();
	uint16	w2	= next_pow2(w),	h2	= next_pow2(h);
	uint32	xs	= count_bits(w2 - 1), ys = count_bits(h2 - 1);

	uint16	a	= min(w2, h2),	b	= max(w2, h2), c = b / a;
	uint32	n	= (a * a * 4 / 3 + 1) * c - 1;

	uint8	*morton = (uint8*)malloc(n);
	memset(morton, 0, n);

	const ISO_rgba	*p = bm.ScanLine(0);
	int				ny = h;

	for (masked_number<uint32> ym((bit_every<uint32, 2> & bits(ys * 2)) << 1, 0); ny--; ++ym) {
		int	nx = w;
		for (masked_number<uint32> xm(bit_every<uint32, 2> & bits(xs * 2), ym.i); nx--; ++xm)
			morton[xm.i] = p++->a ? 2 : 1;
	}

	uint8	*s = morton, *d = s + a * b;
	for (uint32 i = a * a / 3 * c; i--; d++, s += 4)
		*d = s[0] | s[1] | s[2] | s[3];

	for (uint32 i = c - 1; i--; d++, s += 2)
		*d = s[0] | s[1];

	rectangles<256>	rects(rectangle(position2(0, 0), position2(w, h)));
	for (;;) {
		while (*--d == 3);

		uint32	o = uint32(d - morton);
		int		t = 0;
		uint32	m = a * b;
		while (o & m) {
			m >>= 2;
			t++;
		}
		if (t < loglimit)
			break;

		uint32	o2 = (o & -(m << 4)) + ((o & (m - 1)) << 2);
		morton[o2 + 0] |= 4;
		morton[o2 + 1] |= 4;
		morton[o2 + 2] |= 4;
		morton[o2 + 3] |= 4;

		if (*d == 1) {
			o	= (o & (m - 1)) << (t * 2);
			o	= from_morton(o, xs, ys);
			int	x = o & 0xffff, y = o >> 16, s = 1 << t;
			rects -= rectangle(position2(x, y), position2(x + s, y + s));
			fill(bm.Block(x, y, s, s), ISO_rgba(255));
		}
	}
	return rects;
}

float AspectRatio(int w, int h) {
	return max(w, h) / float(min(w, h));
}

bool CheckBlankLine(const ISO_rgba *p, uint32 n, uint32 s) {
	while (n--) {
		if (p->a)
			return false;
		p += s;
	}
	return true;
}

rect FindBitmapExtent(const bitmap &bm, int x, int y, int w, int h) {
	uint32	s = bm.Width();
	int		x1 = x + w, y1 = y + h;

	while (y < y1 && CheckBlankLine(bm.ScanLine(y) + x, w, 1))
		++y;
	while (y1 > y && CheckBlankLine(bm.ScanLine(--y1) + x, w, 1));
	h	= y1 - y + 1;

	while (x < x1 && CheckBlankLine(bm.ScanLine(y) + x, h, s))
		x++;
	while (x1 > x && CheckBlankLine(bm.ScanLine(y) + --x1, h, s));
	w	= x1 - x + 1;
	return rect::with_length(point{x, y}, point{w, h});
}

rect FindBitmapExtent(const bitmap &bm) {
	return FindBitmapExtent(bm, 0, 0, bm.Width(), bm.Height());
}

rect *Subdivide(const bitmap &bm, int x, int y, int w, int h, rect *rects) {
	uint32	s = bm.Width();
	int		x1 = x + w, y1 = y + h;

	while (y < y1 && CheckBlankLine(bm.ScanLine(y) + x, w, 1))
		++y;
	while (y1 > y && CheckBlankLine(bm.ScanLine(--y1) + x, w, 1));
	h	= y1 - y + 1;

	while (x < x1 && CheckBlankLine(bm.ScanLine(y) + x, h, s))
		x++;
	while (x1 > x && CheckBlankLine(bm.ScanLine(y) + --x1, h, s));
	w	= x1 - x + 1;

	if (AspectRatio(w, h) > 2) {
		if (w > h) {
			int	m = w / 2, e = h, i;
			for (i = 0; i < e; i++) {
				if (CheckBlankLine(bm.ScanLine(y) + x + m + i, h, s))
					break;
				if (i && CheckBlankLine(bm.ScanLine(y) + x + m - i, h, s)) {
					i = -i;
					break;
				}
			}
			if (i < e) {
				rects = Subdivide(bm, x, y, i + m, h, rects);
				return Subdivide(bm, x + i + m, y, w - m - i, h, rects);
			}
		} else {
			int	m = h / 2, e = w, i;
			for (i = 0; i < e; i++) {
				if (CheckBlankLine(bm.ScanLine(y + m + i) + x, w, 1))
					break;
				if (i && CheckBlankLine(bm.ScanLine(y + m - i) + x, w, 1)) {
					i = -i;
					break;
				}
			}
			if (i < e) {
				rects = Subdivide(bm, x, y, w, i + m, rects);
				return Subdivide(bm, x, y + i + m, w, h - m - i, rects);
			}
		}
	}
	rects->a = point{x, y};
	rects->b = rects->a + point{w, h};
	return rects + 1;
}

int FindBitmapRegions(const bitmap &bm, rect *rects) {
	return int(Subdivide(bm, 0, 0, bm.Width(), bm.Height(), rects) - rects);
}

ISO_openarray<rect> BitmapToRegions2(holder<ISO_ptr<bitmap> > bm) {
	rect rects[256];
	int	num	= int(Subdivide(*bm.t, 0, 0, bm->Width(), bm->Height(), rects) - rects);
	ISO_openarray<rect>	array(num);
	copy_n(rects, array.begin(), num);
	return array;
}

//-----------------------------------------------------------------------------
//	init
//-----------------------------------------------------------------------------

static initialise init(
	ISO::getdef<PatchModel3>(),
	ISO::getdef<Scene>(),
	ISO::getdef<Children>(),
	ISO::getdef<BasePose>(),
	ISO::getdef<Model3>(),
	ISO::getdef<Animation>(),
	ISO::getdef<AnimationHierarchy>(),
	ISO::getdef<Collision_OBB>(),
	ISO::getdef<Collision_Sphere>(),
	ISO::getdef<Collision_Cylinder>(),
	ISO::getdef<Collision_Cone>(),
	ISO::getdef<Collision_Capsule>(),
	ISO::getdef<Collision_Patch>(),
	ISO::getdef<ent::Light2>(),
	ISO::getdef<ent::Attachment>(),
	ISO::getdef<ent::Cluster>(),
	ISO::getdef<ent::Spline>(),
	ISO::getdef<sample>(),
	ISO::getdef<SampleBuffer>(),
	ISO_get_operation_external(ForPlatform),
	ISO_get_cast(Texture2Bitmap),
	ISO_get_cast(HDRTexture2Bitmap)
);


} //namespace iso

//ISO_DEFUSERCOMPV(iso::BitmapRect, x, y, w, h);

