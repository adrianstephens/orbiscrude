#include "movie.h"
#include "iso/iso_convert.h"

namespace iso {

class Array_frames : public ISO::VirtualDefaults {
	ISO_ptr<anything>	frames;
	int					width, height;

	ISO_ptr<bitmap>	GetFrame(int i) {
		return  ISO_conversion::convert<bitmap>((*frames)[i]);
	}
public:
	Array_frames(const ISO_ptr<anything> &_frames) : frames(_frames) {
		if (ISO_ptr<bitmap> bm = GetFrame(0)) {
			width	= bm->Width();
			height	= bm->Height();
		} else {
			width	= height = 0;
		}
	}
	int				Count()			{ return frames->Count();	}
	ISO::Browser2	Index(int i)	{ return GetFrame(i);		}
	int				Width()			{ return width;				}
	int				Height()		{ return height;			}
	float			FrameRate()		{ return 1;					}
};

ISO_ptr<movie> MakeArrayMovie(ISO_ptr<anything> a) {
	if (a->Count() > 0) {
		ISO_ptr<Array_frames>	frames(0, a);
		if (frames->Width())
			return ISO_ptr<movie>(NULL, frames);
	}
	return ISO_NULL;
}

ISO_ptr<bitmap> QuickResize(const ISO_ptr<bitmap> &bm, int destw, int desth) {
	int	srcew = bm->Width(), srceh = bm->Height(), srced = bm->Depth();
	if (srcew == destw && srceh == desth)
		return bm;

	ISO_ptr<bitmap>	bm2;
	bm2.Create()->Create(destw, desth);

	for (int y = 0; y < desth; y++) {
		ISO_rgba	*d = bm2->ScanLine(y);
		ISO_rgba	*s = bm->ScanLine(y * srceh / desth);
		for (int x = 0; x < destw; x++)
			d[x] = s[x * srcew / destw];
	}
	return bm2;
}

ISO_ptr<bitmap> BlendBitmaps(const ISO_ptr<bitmap> &bm1, const ISO_ptr<bitmap> &bm2, float t) {
	int			w	= bm1->Width(), h = bm1->Height();
	ISO_ptr<bitmap> bm3 = QuickResize(bm2, w, h);

	ISO_ptr<bitmap>	bmd;
	bmd.Create()->Create(w, h);

	ISO_rgba	*s1 = bm1->ScanLine(0);
	ISO_rgba	*s2 = bm3->ScanLine(0);
	ISO_rgba	*d	= bmd->ScanLine(0);

	float	s	= 1 - t;
	for (int n = w * h; n--; s1++, s2++, d++) {
		d->r = s1->r * t + s2->r * s;
		d->g = s1->g * t + s2->g * s;
		d->b = s1->b * t + s2->b * s;
		d->a = s1->a * t + s2->a * s;
	}
	return bmd;
}

ISO_ptr<bitmap> ScaleColours(const ISO_ptr<bitmap> &bm, float t) {
	int			w	= bm->Width(), h = bm->Height();

	ISO_ptr<bitmap>	bmd;
	bmd.Create()->Create(w, h);

	ISO_rgba	*s	= bm->ScanLine(0);
	ISO_rgba	*d	= bmd->ScanLine(0);

	for (int n = w * h; n--; s++, d++) {
		d->r = s->r * t;
		d->g = s->g * t;
		d->b = s->b * t;
		d->a = s->a * t;
	}
	return bmd;
}

class AdjustMovie_frames : public ISO::VirtualDefaults {
	ISO_ptr<movie>		mv;
	float				fps;
	float				width;
	float				height;
	bool				resize;
	bool				interpolate;

	ISO_ptr<bitmap>	GetFrame(int i) {
		ISO_ptr<bitmap>	bm;
		if (fps > 0) {
			float	f	= i * mv->fps / fps;
			bm = ISO::Browser(mv->frames)[int(f)];
			if (interpolate && f != float(int(f)))
				bm = BlendBitmaps(bm, ISO::Browser(mv->frames)[int(f) + 1], int(f) + 1 - f);
		} else {
			bm = ISO::Browser(mv->frames)[i];
		}
		if (resize)
			bm = QuickResize(bm, width, height);
		return bm;
	}

public:
	void			Init(ISO_ptr<movie> _mv, float _fps, float _width, float _height) {
		mv			= _mv;
		fps			= _fps;
		width		= _width;
		height		= _height;
		resize		= width != mv->width || height != mv->height;
		interpolate	= fps > mv->fps;
	}
	int				Count()				{ return ISO::Browser(mv->frames).Count() * (fps < 0 ? 1 : fps / mv->fps);	}
	ISO::Browser2	Index(int i)		{ return GetFrame(i);	}

	int				Width()		const	{ return width; }
	int				Height()	const	{ return height; }
	int				FrameRate()	const	{ return abs(fps); }
};


ISO_ptr<movie> AdjustMovie(ISO_ptr<movie> mv, float fps, float width, float height) {
	if (width == 0) {
		if (height == 0) {
			width	= mv->width;
			height	= mv->height;
		} else {
			width	= mv->width * height / mv->height;
		}
	} else if (height == 0) {
		height = mv->height * width / mv->width;
	}
	if (fps == 0)
		fps = mv->fps;

	ISO_ptr<AdjustMovie_frames>	frames(0);
	frames->Init(mv, fps, width, height);

	return ISO_ptr<movie>(0, frames);
}

class BlendMovie_frames : public ISO::VirtualDefaults {
	ISO_ptr<movie>	m1, m2;
	int				c1, c2;
	float			t;

	ISO_ptr<bitmap>	GetFrame(int i) {
		int	i2 = i * m2->fps / m1->fps;
		if (i > c1) {
			return ScaleColours(QuickResize(ISO::Browser(m2->frames)[i2], m1->width, m1->height), 1 - t);

		} else if (i2 > c2) {
			return ScaleColours(ISO::Browser(m1->frames)[i], t);

		} else {
			ISO_ptr<bitmap> bm1 = ISO::Browser(m1->frames)[i], bm2 = ISO::Browser(m2->frames)[i2];
			return BlendBitmaps(bm1, bm2, t);
		}
	}

public:
	int				Count()				{ return max(c1, int(c2 * m1->fps / m2->fps));	}
	ISO::Browser2	Index(int i)		{ return GetFrame(i);			}

	void			Init(ISO_ptr<movie> &_m1, ISO_ptr<movie> &_m2, float _t) {
		m1	= _m1;
		m2	= _m2;
		t	= _t;
		c1	= ISO::Browser(m1->frames).Count();
		c2	= ISO::Browser(m2->frames).Count();
	}
	int				Width()		const	{ return m1->width; }
	int				Height()	const	{ return m1->height; }
	int				FrameRate()	const	{ return m1->fps; }
};

ISO_ptr<movie> BlendMovie(ISO_ptr<movie> m1, ISO_ptr<movie> m2, float t) {
	ISO_ptr<BlendMovie_frames>	frames(NULL);
	frames->Init(m1, m2, t);
	return ISO_ptr<movie>(NULL, frames);
}

static initialise init(
	ISO_get_operation(AdjustMovie),
	ISO_get_cast(MakeArrayMovie),
	ISO_get_operation(BlendMovie)
);

}//namespace iso


ISO_DEFVIRT(iso::Array_frames);
ISO_DEFVIRT(iso::AdjustMovie_frames);
ISO_DEFVIRT(iso::BlendMovie_frames);


