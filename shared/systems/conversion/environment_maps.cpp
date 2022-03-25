#include "base/vector.h"
#include "iso/iso_files.h"
#include "iso/iso_convert.h"
#include "utilities.h"
#include "filetypes/bitmap/bitmap.h"
#include "extra/perlin_noise.h"
#include "maths/spherical_harmonics.h"
#include "jobs.h"

using namespace iso;

typedef array_vec<double, 4> Drgba;

//-----------------------------------------------------------------------------
//	Cubemaps/Spheremaps
//-----------------------------------------------------------------------------

enum CUBE_FACE { X_POS, X_NEG, Y_POS, Y_NEG, Z_POS, Z_NEG};

int GetCubeFace(param(float3) n, float &px, float &py) {
	float3	a = abs(n);
	switch (max_component_index(a)) {
		case 0:
			px = -n.y / n.x;
			py = n.z / a.x;
			return n.x < 0 ? 1 : 0;
		case 1:
			px = n.x / n.y;
			py = n.z / a.y;
			return n.y < 0 ? 5 : 4;
		default:
			px =  n.x / a.z;
			py = -n.y / n.z;
			return n.z < 0 ? 2 : 3;
	}
}

float3 GetCubeDir(int f, float x, float y, float h = 1) {
	switch (f) {
		default:
		case 0:	return float3{ h, -x,  y};
		case 1:	return float3{-h,  x,  y};
		case 2:	return float3{ x,  y, -h};
		case 3:	return float3{ x, -y,  h};
		case 4:	return float3{ x,  h,  y};
		case 5:	return float3{-x, -h,  y};
	}
}
float3 GetCubeDir(int size, int f, int x, int y) {
	float	h = size / 2.f;
	return GetCubeDir(f, x - h, y - h, h);
}

float3 GetCubeFaceDir(int f, float x, float y, float z) {
	switch (f) {
		default:
		case 0:	return float3{-y,  z,  x};
		case 1:	return float3{ y,  z, -x};
		case 2:	return float3{ x,  y, -z};
		case 3:	return float3{ x, -y,  z};
		case 4:	return float3{ x,  z,  y};
		case 5:	return float3{-x,  z, -y};
	}
}
float3 GetCubeFaceDir(int f, param(float3) dir) {
	return GetCubeFaceDir(f, dir.x, dir.y, dir.z);
}

float GetCubeSolidAngle(int size, int x, int y) {
	float	h	= size / 2.f;
	float	r2	= square(x / h - 1) + square(y / h - 1) + 1;
	return 1 / (r2 * sqrt(r2) * square(h));
//	return 1 / ((1 + square(x / h - 1)) * (1 + square(y / h - 1)) * square(h));
}

float AreaElement(float x, float y) {
	return atan2(x * y, sqrt(square(x) + square(y) + 1));
}

// Get projected area for a texel
float TexelCoordSolidAngle(int size, int x, int y) {
	// (+ 0.5f is for texel center addressing)
	float inv_size = 1.0f / size;
	float u = (x + 0.5f) * 2 * inv_size - 1;
	float v = (y + 0.5f) * 2 * inv_size - 1;

	float x0 = u - inv_size;
	float y0 = v - inv_size;
	float x1 = u + inv_size;
	float y1 = v + inv_size;
	return AreaElement(x0, y0) - AreaElement(x0, y1) - AreaElement(x1, y0) + AreaElement(x1, y1);
}

struct EnvMapping {
	int		size;
	EnvMapping(int _size)	: size(_size)		{}
	int		Size()			const	{ return size; }
	int		Depth()			const	{ return 1; }
	int		BitmapFlags()	const	{ return 0; }
};

struct CubemapMapping : EnvMapping {
	CubemapMapping(int _w, int _h) : EnvMapping(_w)	{}
	CubemapMapping(int _s) : EnvMapping(_s)			{}
	int		Width()			const	{ return size; }
	int		Height()		const	{ return size * 6; }
	int		Depth()			const	{ return 6; }
	int		BitmapFlags()	const	{ return BMF_CUBE; }

	bool TexelToDir(int x, int y, int z, float3 &d) const {
		d = GetCubeDir(size, z, x, y);
		return true;
	}
	bool DirToTexel(param(float3) d, int &x, int &y, int &z) const {
		float	px, py;
		z = GetCubeFace(d, px, py);
		x = int((px + 1) * size / 2);
		y = int((py + 1) * size / 2);
		return true;
	}
	float GetSolidAngle(int x, int y, int z) const {
		return GetCubeSolidAngle(size, x, y);
	}
};

struct CrossmapMapping : EnvMapping {
	CrossmapMapping(int _w, int _h) : EnvMapping(_h / 4)	{}
	CrossmapMapping(int _s) : EnvMapping(_s)				{}
	int		Width()			const	{ return size * 3; }
	int		Height()		const	{ return size * 4; }

	bool TexelToDir(int x, int y, int z, float3 &d) const {
		int		f = 0;
		if (x >= size && x < size * 2) {
			int	t = y / size;
			y -= t * size;
			x -= size;
			switch (t) {
				case 0: f = 2; break;
				case 1: f = 4; break;
				case 2: f = 3; break;
				case 3: f = 5; break;
			}
		} else {
			if (y < size || y >= size * 2)
				return false;
			y -= size;
			if (x < size)
				f = 1;
			else
				x -= size * 2;
		}
		d = GetCubeDir(size, f, x, y);
		return true;
	}
	bool DirToTexel(param(float3) d, int &x, int &y, int &z) const {
		float	px, py;
		int		dx, dy;
		switch (GetCubeFace(d, px, py)) {
			case 0:	dx = size * 2;	dy = size;		break;
			case 1:	dx = 0;		dy = size;		break;
			case 2:	dx = size;		dy = 0;		break;
			case 3:	dx = size;		dy = size * 2; break;
			case 4:	dx = size;		dy = size;		break;
			case 5:	dx = size;		dy = size * 3; break;
		}
		x = dx + int(size * (px + 1) / 2);
		y = dy + int(size * (py + 1) / 2);
		z = 0;
		return true;
	}
	float GetSolidAngle(int x, int y, int z) const {
		return GetCubeSolidAngle(size, x % size, y % size);
	}
};

struct SkydomeMapping : EnvMapping {
	SkydomeMapping(int _w, int _h) : EnvMapping(_w / 4)	{}
	SkydomeMapping(int _s) : EnvMapping(_s)				{}
	int		Width()			const	{ return size * 4; }
	int		Height()		const	{ return size * 2; }

	bool TexelToDir(int x, int y, int z, float3 &d) const {
		float	ax	= (float(x) / (size * 2) - 1)   * pi;
		float	ay	= (float(y) / (size * 2) - .5f) * pi;
		float	c	= cos(ay);
		d = float3{c * sin(ax), c * cos(ax), sin(ay)};
		return true;
	}
	bool DirToTexel(param(float3) d, int &x, int &y, int &z) const {
		float	ax	= atan2(d.xy);
		float	ay	= atan2(d.z, len(d.xy));
		x	= int((ax / pi + 1) * size * 2);
		y	= int((ay / pi + .5f) * size * 2);
		z	= 0;
		return true;
	}
	float GetSolidAngle(int x, int y, int z) const {
		return cos(float(y) / (size * 2) - .5f) * pi / (4 * size * size);
	}
};

struct ProbemapMapping : EnvMapping {
	ProbemapMapping(int _w, int _h) : EnvMapping(_w)		{}
	ProbemapMapping(int _s) : EnvMapping(_s)				{}
	int		Width()			const	{ return size; }
	int		Height()		const	{ return size; }

	bool TexelToDir(int x, int y, int z, float3 &d) const {
		float	h	= size / 2.f;
		float	fx	= (x - h) / h;
		float	fy	= (h - y) / h;
		float	r2	= square(fx) + square(fy);
		if (r2 >= 1)
			return false;
		float	r	= sqrt(r2);
		float	dy	= cos(r * pi);
		float	t	= r ? sqrt(1 - square(dy)) / r : 0;
		d = float3{t * fx, dy, t * fy};
		return true;
	}
	bool DirToTexel(param(float3) d, int &x, int &y, int &z) const {
		float	r	= acos(d.y / len(d)) / len(d.xz) * (one / pi);
		float	h	= size / 2.f;
		x	= int((d.x * r + 1) * h);
		y	= int((d.z * r + 1) * h);
		z	= 0;
		return true;
	}
	float GetSolidAngle(int x, int y, int z) const {
		float	h	= size / 2.f;
		float	fx	= (x - h) / h;
		float	fy	= (h - y) / h;
		float	r	= sqrt(square(fx) + square(fy));
		return four * pi * pi / (size * size) * sinc(pi * r);
	}
};

struct SpheremapMapping : EnvMapping {
	SpheremapMapping(int _w, int _h) : EnvMapping(_w)	{}
	SpheremapMapping(int _s) : EnvMapping(_s)			{}
	int		Width()			const	{ return size; }
	int		Height()		const	{ return size; }

	bool TexelToDir(int x, int y, int z, float3 &d) const {
		float	h	= size / 2.f;
		float	fx	= (x - h) / h;
		float	fy	= (h - y) / h;
		float	r2	= square(fx) + square(fy);
		if (r2 >= 1)
			return false;
		float3	n{fx, fy, -sqrt(1 - r2)};
		d = float3{zero, zero, len2(n)} - n * n.y * 2;
		return true;
	}
	bool DirToTexel(param(float3) d, int &x, int &y, int &z) const {
		float3	n	= normalise(float3{0, 0, -1} + normalise(d));
		float	h	= size / 2.f;
		x	= int((n.x + 1) * h);
		y	= int((n.y + 1) * h);
		z	= 0;
		return true;
	}
	float GetSolidAngle(int x, int y, int z) const {
		return 1;	//dummy
	}
};

struct HemiSpheremapMapping : EnvMapping {
	HemiSpheremapMapping(int _w, int _h) : EnvMapping(_w)	{}
	HemiSpheremapMapping(int _s) : EnvMapping(_s)			{}
	int		Width()			const	{ return size; }
	int		Height()		const	{ return size; }

	bool TexelToDir(int px, int py, int pz, float3 &d) {
		float	h	= size / 2.f;
		float	x	= (px - h) / h;
		float	y	= (h - py) / h;
#if 1
		if (float t = max(abs(x), abs(y))) {
			float2	sc	= sincos(t * pi * half);
			d	= concat(float2{x, y} * (sc.y * rsqrt(square(x) + square(y))), sc.x);
		} else {
			d	= float3{0, 0, 1};
		}
#else
		float	x2	= square(x);
		float	y2	= square(y);
		float	t2	= max(x2, y2);
		d	= float3(float2(x, y) * sqrt(t2 / (x2 + y2)), sqrt(1 - t2));
#endif
		return true;
	}
	bool DirToTexel(param(float3) d, int &x, int &y, int &z) {
		float3	n	= normalise(d);
		float	h	= size / 2.f;
		x	= int((n.x + 1) * h);
		y	= int((n.z + 1) * h);
		z	= 0;
		return true;
	}
	float GetSolidAngle(int px, int py, int pz) {
		float	h	= size / 2.f;
		float	x	= (px - h) / h;
		float	y	= (h - py) / h;
		if (float t = max(abs(x), abs(y))) {
#if 0
			float	x2	= square(x);
			float	y2	= square(y);
			float2	sc	= sincos(t * pi * half);
			return pi / 2 * t * sc.y / square(x2 + y2) * sqrt(square(sc.y * (x2 + y2)) + square(sc.x * (x2 + y2)));
#else
			return t * pi / 2 * sin(t * pi / 2) / (square(x) + square(y)) / (h * h);
#endif
		}
		return 0;
	}
};

struct WiiCubemapMapping : EnvMapping {
	WiiCubemapMapping(int _w, int _h) : EnvMapping(_w / 4)	{}
	WiiCubemapMapping(int _s) : EnvMapping(_s)				{}
	int		Width()			const	{ return size * 4; }
	int		Height()		const	{ return size * 2; }

	bool TexelToDir(int x, int y, int z, float3 &d) const {
		if (y > size) {
			static uint8 remap[] = {0,5,1,4};
			int	f	= x / size;
			d = GetCubeDir(size, remap[f], x - f * size, y - size);
		} else {
			float	f;
			float	fx	= float(x) / size;
			float	fy	= float(y) / size;
			if (2 * fy < abs(2 - fx)) {
				if (fx > 2) fx -= 4;
				fy	= one - fy;
				f	= floor(fx / fy);
				fx	= (fx - (f + half) * fy) * 2;
				switch (int(f)) {
					case -2: d = float3{-fy, +fx, +one}; break;
					case -1: d = float3{+fx, +fy, +one}; break;
					case  0: d = float3{+fy, -fx, +one}; break;
					case +1: d = float3{-fx, -fy, +one}; break;
				}
			} else {
				fx	-= 2;
				f	= floor(fx / fy);
				fx	= (fx - (f + half) * fy) * 2;
				switch (int(f)) {
					case -2: d = float3{+fy, -fx, -one}; break;
					case -1: d = float3{-fx, -fy, -one}; break;
					case  0: d = float3{-fy, +fx, -one}; break;
					case +1: d = float3{+fx, +fy, -one}; break;
				}
			}
		}
		return true;
	}
};

template<typename M> struct OffsetEnvMapping : M {
	int		ox, oy, oz;
	OffsetEnvMapping(const M &m, int _ox, int _oy, int _oz = 0) : M(m), ox(_ox), oy(_oy), oz(_oz) {}

	bool TexelToDir(int x, int y, int z, float3 &d) const {
		return M::TexelToDir(x + ox, y + oy, z + oz, d);
	}
	bool DirToTexel(param(float3) d, int &x, int &y, int &z) const {
		if (M::DirToTexel(d, x, y, z)) {
			x -= ox;
			y -= oy;
			z -= oz;
			return true;
		}
		return false;
	}
	float GetSolidAngle(int x, int y, int z) const {
		return M::GetSolidAngle(x + ox, y + oy, z + oz);
	}
};

template<typename M> OffsetEnvMapping<M> make_offset_env_mapping(const M &m, int ox, int oy, int oz = 0) {
	return OffsetEnvMapping<M>(m, ox, oy, oz);
}

template<typename S, typename D, typename ST, typename DT> void EnvironmentMap(block<DT,3> dblock, block<ST,3> sblock) {
	int		sw	= sblock.template size<1>();
	int		sh	= sblock.template size<2>();
//	int		sd	= sblock.template size<3>();
	S		srce(sw, sh);

	int		dw	= dblock.template size<1>();
	int		dh	= dblock.template size<2>();
	int		dd	= dblock.template size<3>();
	D		dest(dw, dh);

	float3	dir;
	int		xs, ys, zs;

	for (int z = 0; z < dd; z++) {
		for (int y = 0; y < dh; y++) {
			auto d = dblock[z][y].begin();
			for (int x = 0; x < dw; ++x, ++d) {
				if (dest.TexelToDir(x, y, z, dir) && srce.DirToTexel(dir, xs, ys, zs))
					*d = sblock[zs][clamp(ys, 0, sh - 1)][clamp(xs, 0, sw - 1)];
			}
		}
	}
}

template<typename S, typename D> ISO_ptr<HDRbitmap> EnvironmentMap(ISO_ptr<HDRbitmap> bm, int size) {
	if (size == 0)
		size = S(size).Size();

	D					dest(size ? size : 256);
	ISO_ptr<HDRbitmap>	bm2(0);
	bm2->Create(dest.Width(), dest.Height(), dest.BitmapFlags(), dest.Depth());
	EnvironmentMap<S,D>(bm2->All3D(), bm->All3D());
	return bm2;
}

//-----------------------------------------------------------------------------
//	Filtering
//-----------------------------------------------------------------------------

template<typename M> double FilterTexelBlock(
	Drgba &accum, const M &env, const block<HDRpixel, 3> &src,
	param(float3) centre_dir, float dot_thresh, float *filter_coeffs, int num_filter_coeffs
) {
	double	weightAccum = 0;

	int	nx	= src.size<1>();
	int	ny	= src.size<2>();
	int	nz	= src.size<3>();

	float3	texel_dir;
	float	tap;

	for (int z = 0; z < nz; ++z) {
		for (int y = 0; y < ny; ++y) {
			auto	i = src[z][y].begin();
			for (int x = 0; x < nx; ++x, ++i) {
				if (env.TexelToDir(x, y, z, texel_dir) && (tap = dot(normalise(texel_dir), centre_dir)) >= dot_thresh) {
					float weight = env.GetSolidAngle(x, y, z) * filter_coeffs[int(tap * (num_filter_coeffs - 1) + .5f)];
					accum		+= to<double>(*i) * weight;
					weightAccum	+= weight;
				}
			}
		}
	}
	return weightAccum;
}

HDRpixel FilterTexel(const CubemapMapping &env, param(float3) centre_dir, float dot_thresh,
	const block<HDRpixel, 3> &sblock,
	float *filter_coeffs, int num_filter_coeffs
) {
	int		size		= env.size;
	float	filter_tan	= sqrt(1 - square(dot_thresh)) / dot_thresh;
	int		filter_size	= (int)ceil(filter_tan * size);

	Drgba	dstAccum(0,0,0,0);
	double	weightAccum = 0;

	for (int face = 0; face < 6; face++) {
		float3 face_dir = GetCubeFaceDir(face, centre_dir);
		if (face_dir.z <= 0)
			continue;

		int		u	= (face_dir.x / face_dir.z + 1) * size * half;
		int		v	= (face_dir.y / face_dir.z + 1) * size * half;
		int		u0	= max(u - filter_size, 0);
		int		v0	= max(v - filter_size, 0);
		int		u1	= min(u + filter_size, size - 1);
		int		v1	= min(v + filter_size, size - 1);

		weightAccum += FilterTexelBlock(dstAccum,
			make_offset_env_mapping(env, u0, v0, face),
			sblock.sub<1>(u0, u1 - u0 + 1).sub<2>(v0, v1 - v0 + 1),
			centre_dir, dot_thresh, filter_coeffs, num_filter_coeffs
		);
	}

	if (weightAccum)
		return to<float>(dstAccum / weightAccum);

	//otherwise sample nearest
	int		u, v, w;
	env.DirToTexel(centre_dir, u, v, w);
	return sblock[w][min(v, size - 1)][min(u, size - 1)];
}

HDRpixel FilterTexel(const SkydomeMapping &env, param(float3) centre_dir, float dot_thresh,
	const block<HDRpixel, 3> &sblock,
	float *filter_coeffs, int num_filter_coeffs
) {
	float3	perp_dir[2];
	perp_dir[0]			= perp(centre_dir);
	perp_dir[1]			= cross(centre_dir, perp_dir[0]);
	float	sin_filter	= sqrt(1 - square(dot_thresh));

//	int		size	= env.Size();
	Drgba	dstAccum(0,0,0,0);
	double	weightAccum = 0;

	int		u, v, w;
	if (!env.DirToTexel(centre_dir, u, v, w))
		return HDRpixel(0,0,0,0);

	int		u0 = u, v0 = v, w0 = w;
	int		u1 = u, v1 = v, w1 = w;
	for (int i = 0; i < 4; i++) {
		float3	dir = centre_dir + perp_dir[i / 2] * (i & 1 ? sin_filter : -sin_filter);
		int		u, v, w;
		env.DirToTexel(dir, u, v, w);
		u0 = min(u0, u);
		u1 = max(u1, u);
		v0 = min(v0, v);
		v1 = max(v1, v);
		w0 = min(w0, w);
		w1 = max(w1, w);
	}

	weightAccum += FilterTexelBlock(dstAccum,
		make_offset_env_mapping(env, u0, v0, 0),
		sblock.sub<1>(u0, u1 - u0 + 1).sub<2>(v0, v1 - v0 + 1),
		centre_dir, dot_thresh, filter_coeffs, num_filter_coeffs
	);

	if (weightAccum)
		return to<float>(dstAccum / weightAccum);

	//otherwise sample nearest
	return sblock[0][clamp(v, 0, sblock.size<2>() - 1)][clamp(u, 0, sblock.size<1>() - 1)];
}

enum FILTER_TYPE { DISC, CONE, GAUSSIAN };

void BuildAngleWeightLUT(FILTER_TYPE type, float filter_angle, float *coeffs, int num_coeffs) {
	switch (type) {
		case DISC:
			for (int i = 0; i < num_coeffs; i++)
				coeffs[i] = 1;
			break;

		case CONE: {
			//cone centered around the center tap and falls off to zero over the filtering radius
			for (int i = 0; i < num_coeffs; i++) {
				float angle	= acos((float)i / (float)(num_coeffs - 1));
				coeffs[i]	= max((filter_angle - angle) / filter_angle, 0);
			}
			break;
		}
		case GAUSSIAN: {
			//fit 3 standard deviations within angular extent of filter
			float stdDev = filter_angle / 3;
			float inv2Variance = 0.5f / square(stdDev);

			for (int i = 0; i < num_coeffs; i++) {
				float angle	= acos((float)i / (float)(num_coeffs - 1));
				coeffs[i]	= exp(-square(angle) * inv2Variance);
			}
			break;
		}
	}
}

#ifdef ISO_EDITOR
template<typename S, typename D, typename ST, typename DT> struct FilterEnvironmentJob {
	D					dest;
	S					srce;
	block<DT,3>			dblock;
	block<ST,3>			sblock;
	float				dot_thresh;
	ref_array<float>	filter_coeffs;
	int					num_filter_coeffs;

	void operator()() {
		int	nx	= sblock.template size<1>();
		int	ny	= sblock.template size<2>();
		int	nz	= sblock.template size<3>();

		float3	dir;
		for (int z = 0; z < nz; ++z) {
			for (int y = 0; y < ny; ++y) {
				auto i = dblock[z][y].begin();
				for (int x = 0; x < nx; ++x, ++i) {
					if (dest.TexelToDir(x, y, z, dir))
						*i = FilterTexel(srce, normalise(dir), dot_thresh, sblock, filter_coeffs, num_filter_coeffs);
				}
			}
		}
		delete this;
	}

	FilterEnvironmentJob(
		const D				&_dest,			const S				&_srce,
		const block<DT,3>	&_dblock,		const block<ST,3>	&_sblock,
		float				_dot_thresh,	ref_array<float>	_filter_coeffs,	int					_num_filter_coeffs
	) :	dest(_dest), srce(_srce)
		, dblock(_dblock), sblock(_sblock)
		, dot_thresh(_dot_thresh), filter_coeffs(_filter_coeffs), num_filter_coeffs(_num_filter_coeffs)
	{}
};

template<typename S, typename D, typename ST, typename DT> void FilterEnvironmentMap(block<DT,3> dblock, block<ST,3> sblock, FILTER_TYPE type, float filter_angle, int num_filter_coeffs) {
	int		dw	= dblock.template size<1>();
	int		dh	= dblock.template size<2>();
	int		dd	= dblock.template size<3>();
	D		dest(dw, dh);
	S		srce(sblock.template size<1>(), sblock.template size<2>());

	filter_angle				= min(filter_angle, pi / 2);
	float	dot_thresh			= cos(filter_angle);
	ref_array<float>	filter_coeffs(num_filter_coeffs);
	BuildAngleWeightLUT(type, filter_angle, filter_coeffs, num_filter_coeffs);

	for (int z = 0; z < dd; z++) {
		ConcurrentJobs::Get().add(
			new FilterEnvironmentJob<S, OffsetEnvMapping<D>, ST, DT>(
				OffsetEnvMapping<D>(dest,0,0,z), srce,
				dblock.template sub<3>(z, 1), sblock,
				dot_thresh, filter_coeffs, num_filter_coeffs
			)
		);
	}
}
#else

template<typename S, typename D, typename ST, typename DT> void FilterEnvironmentMap(block<DT,3> dblock, block<ST,3> sblock, FILTER_TYPE type, float filter_angle, int num_filter_coeffs) {
	int		dw	= dblock.template size<1>();
	int		dh	= dblock.template size<2>();
	int		dd	= dblock.template size<3>();
	D		dest(dw, dh);
	S		srce(sblock.template size<1>(), sblock.template size<2>());

	filter_angle				= min(filter_angle, pi / 2);
	float	dot_thresh			= cos(filter_angle);
	float	*filter_coeffs		= new float[num_filter_coeffs];
	BuildAngleWeightLUT(type, filter_angle, filter_coeffs, num_filter_coeffs);

	float3	dir;
	for (int z = 0; z < dd; z++) {
		for (int y = 0; y < dh; y++) {
			auto d = dblock[z][y].begin();
			for (int x = 0; x < dw; ++x, ++d) {
				if (dest.TexelToDir(x, y, z, dir))
					*d = FilterTexel(srce, normalise(dir), dot_thresh, sblock, filter_coeffs, num_filter_coeffs);
			}
		}
	}
	delete[] filter_coeffs;
}

#endif

template<typename S, typename D> ISO_ptr<HDRbitmap> EnvironmentMapGauss(ISO_ptr<HDRbitmap> bm, int size, float angle) {
	if (size == 0)
		size = S(size).Size();

	if (angle == 0)
		angle = 90.f / size;

	D	dest(size);

	ISO_ptr<HDRbitmap>	bm2(0);
	bm2->Create(dest.Width(), dest.Height(), dest.BitmapFlags(), dest.Depth());
	FilterEnvironmentMap<S,D>(bm2->All3D(), bm->All3D(), GAUSSIAN, degrees(angle), 4096);
	return bm2;
}

template<typename S, typename D> ISO_ptr<HDRbitmap> EnvironmentGaussMips(ISO_ptr<HDRbitmap> bm, int size, float angle) {
	if (size == 0)
		size = S(size).Size();

	if (angle == 0)
		angle = 90.f / size;

	D		dest(size);
	int		mips	= MaxMips(size, size);

	ISO_ptr<HDRbitmap>	bm2(0);
	bm2->Create(dest.Width() * 2, dest.Height(), dest.BitmapFlags(), dest.Depth());
	bm2->SetMips(mips);

	for (int i = 0; i < mips; i++, angle *= 2)
		FilterEnvironmentMap<S,D>(bm2->MipArray(i), bm->All3D(), GAUSSIAN, degrees(angle), 4096);

	return bm2;
}

ISO_ptr<HDRbitmap> GaussianCubeMips(ISO_ptr<HDRbitmap> bm, int size, float angle) {
	return bm->IsCube()
		? EnvironmentGaussMips<CubemapMapping,CubemapMapping>(bm, size, angle)
		: EnvironmentGaussMips<SkydomeMapping,CubemapMapping>(bm, size, angle);
}

#ifdef ISO_TEST

struct testenv {
	testenv() {
		CrossmapMapping	m(256);
		int d = m.Depth();
		int h = m.Height() / d;
		int w = m.Width();
		float3	dir;
		int		x2, y2, z2;
		for (int z = 0; z < d; z++) {
			for (int y = 0; y < h; y++) {
				for (int x = 0; x < w; x++) {
					if (m.TexelToDir(x, y, z, dir)) {
						ISO_VERIFY(m.DirToTexel(dir, x2, y2, z2));
						if (!(abs(x-x2)<2 && abs(y-y2)<2 && abs(z-z2)<2))
							ISO_OUTPUTF("x = ") << x << "; y = " << y << '\n';
					}
				}
			}
		}
	}
} _testenv;

#endif

//-----------------------------------------------------------------------------
//	SphericalHarmonics
//-----------------------------------------------------------------------------

sh::rotation<2>		testshrot(rotate_in_z(pi * half));
sh::harmonics<2>	testsh;
sh::harmonics<2>	testsh2 = testshrot * testsh;

template<typename S, typename T> void _ToSH(const block<T,3> &sblock, int level, Drgba *coeffs) {
	int		w = sblock.template size<1>(), h = sblock.template size<2>(), d = sblock.template size<3>();
	S		mapping(w, h);

	for (int z = 0; z < d; z++) {
		for (int y = 0; y < h; y++) {
			auto	p = sblock[z][y].begin();
			for (int x = 0; x < w; x++, p++) {
				float3	dir;
				if (mapping.TexelToDir(x, y, z, dir)) {
	#if 1
					double	theta	= atan2(len(dir.xy), dir.z);
					double	phi		= atan2(dir.yx);
	#else
					spherical_coordinate	sph(dir);
					double	theta	= float(sph.theta);
					double	phi		= float(sph.phi);
	#endif
					Drgba	col		= to<double>(*p) * double(mapping.GetSolidAngle(x, y, z));
					for (int l = 0, i = 0; l < level; l++) {
						for (int m = -l; m <= l; m++, i++)
							coeffs[i] += col * sh::SH(l, m, theta, phi);
					}
				}
			}
		}
	}
}

template<typename D, typename T> void _FromSH(const block<T,3> &dblock, int level, Drgba *coeffs) {
	int		w	= dblock.template size<1>(), h = dblock.template size<2>(), d = dblock.template size<3>();
	D		mapping(w, h);

	for (int z = 0; z < d; z++) {
		for (int y = 0; y < h; y++) {
			auto	p = dblock[z][y].begin();
			for (int x = 0; x < w; x++, p++) {
				float3	dir;
				if (mapping.TexelToDir(x, y, z, dir)) {
					double	theta	= atan2(len(dir.xy), dir.z);
					double	phi		= atan2(dir.yx);
					Drgba	col(0,0,0,0);
					for (int l = 0, i = 0; l < level; l++) {
						for (int m = -l; m <= l; m++, i++)
							col += coeffs[i] * sh::SH(l, m, theta, phi);
					}
					*p = to<float>(col);
				}
			}
		}
	}
}

ISO_openarray<float3p> ToSH(ISO_ptr<HDRbitmap> bm, int level) {
	if (level == 0)
		level = 3;

	Drgba	*coeffs2	= new Drgba[level * level];
	memset(coeffs2, 0, level * level * sizeof(Drgba));

	if (bm->IsCube()) {
		_ToSH<CubemapMapping>(bm->All3D(), level, coeffs2);
	} else {
		_ToSH<ProbemapMapping>(bm->All3D(), level, coeffs2);
	}

	ISO_openarray<float3p>	coeffs(level * level);
	for (int l = 0, i = 0; l < level; l++) {
		for (int m = -l; m <= l; m++, i++) {
			float	k = sh::K(l, abs(m));
			coeffs[i] = float3{float(coeffs2[i].r) * k, float(coeffs2[i].g) * k, float(coeffs2[i].b) * k};
		}
	}
	delete[] coeffs2;
	return coeffs;
}

template<typename D> ISO_ptr<HDRbitmap> FromSH(ISO_ptr<ISO_openarray<float[3]> > coeffs, int size) {
	int		level		= int(sqrt(float(coeffs->Count())));

	Drgba	*coeffs2	= new Drgba[level * level];
	for (int l = 0, i = 0; l < level; l++) {
		for (int m = -l; m <= l; m++, i++) {
			double k		= sh::K(l, abs(m));
			coeffs2[i].r	= (*coeffs)[i][0] * k;
			coeffs2[i].g	= (*coeffs)[i][1] * k;
			coeffs2[i].b	= (*coeffs)[i][2] * k;
			coeffs2[i].a	= 0;//coeffs[i][3] * k;
		}
	}

	D		mapping(size ? size : 256);
	ISO_ptr<HDRbitmap>	bm(0);
	bm->Create(mapping.Width(), mapping.Height(), mapping.BitmapFlags(), mapping.Depth());

	_FromSH<D>(bm->All3D(), level, coeffs2);
	delete[] coeffs2;
	return bm;
}

template<typename S, typename T> void _ToHSH(const block<T,3> &sblock, int level, Drgba *coeffs) {
	int		w = sblock.template size<1>(), h = sblock.template size<2>(), d = sblock.template size<3>();
	S		mapping(w, h);

	for (int z = 0; z < d; z++) {
		for (int y = 0; y < h; y++) {
			auto	p = sblock[z][y].begin();
			for (int x = 0; x < w; x++, p++) {
				float3	dir;
				if (mapping.TexelToDir(x, y, z, dir)) {
					double	theta	= atan2(len(dir.xy), dir.z);
					double	phi		= atan2(dir.yx);
					double	domega	= mapping.GetSolidAngle(x, y, z);
					Drgba	col		= to<double>(*p) * domega;
					for (int l = 0, i = 0; l < level; l++) {
						for (int m = -l; m <= l; m++, i++)
							coeffs[i] += col * sh::HSH(l, m, theta, phi);
					}
				}
			}
		}
	}
}

template<typename D, typename T> void FromHSH(const block<T,3> &dblock, int level, Drgba *coeffs) {
	int		w	= dblock.template size<1>(), h = dblock.template size<2>(), d = dblock.template size<3>();
	D		mapping(w, h);

	for (int z = 0; z < d; z++) {
		for (int y = 0; y < h; y++) {
			auto	p = dblock[z][y].begin();
			for (int x = 0; x < w; x++, p++) {
				float3	dir;
				if (mapping.TexelToDir(x, y, z, dir)) {
					double	theta	= atan2(len(dir.xy), dir.z);
					double	phi		= atan2(dir.yx);
					Drgba	col(0,0,0,0);
					for (int l = 0, i = 0; l < level; l++) {
						for (int m = -l; m <= l; m++, i++)
							col += coeffs[i] *sh:: HSH(l, m, theta, phi);
					}
					p->r = float(col.r);
					p->g = float(col.g);
					p->b = float(col.b);
					p->a = 1.f;
				}
			}
		}
	}
}

ISO_openarray<float3p> ToHSH(ISO_ptr<HDRbitmap> bm, int level) {
	if (level == 0)
		level = 3;

	Drgba	*coeffs2	= new Drgba[level * level];
	memset(coeffs2, 0, level * level * sizeof(Drgba));

	if (bm->IsCube()) {
		_ToHSH<CubemapMapping>(bm->All3D(), level, coeffs2);
	} else {
		_ToHSH<HemiSpheremapMapping>(bm->All3D(), level, coeffs2);
	}

	ISO_openarray<float3p>	coeffs(level * level);
	for (int l = 0, i = 0; l < level; l++) {
		for (int m = -l; m <= l; m++, i++) {
			float	k = sh::HK(l, abs(m));
			coeffs[i] = float3{float(coeffs2[i].r) * k, float(coeffs2[i].g) * k, float(coeffs2[i].b) * k};
		}
	}
	delete[] coeffs2;
	return coeffs;
}

template<typename D> ISO_ptr<HDRbitmap> FromHSH(ISO_ptr<ISO_openarray<float[3]> > coeffs, int size) {
	int		level		= int(sqrt(float(coeffs->Count())));

	Drgba	*coeffs2	= new Drgba[level * level];
	for (int l = 0, i = 0; l < level; l++) {
		for (int m = -l; m <= l; m++, i++) {
			double k		= sh::HK(l, abs(m));
			coeffs2[i].r	= (*coeffs)[i][0] * k;
			coeffs2[i].g	= (*coeffs)[i][1] * k;
			coeffs2[i].b	= (*coeffs)[i][2] * k;
			coeffs2[i].a	= 0;//coeffs[i][3] * k;
		}
	}

	D		mapping(size ? size : 256);
	ISO_ptr<HDRbitmap>	bm(0);
	bm->Create(mapping.Width(), mapping.Height(), mapping.BitmapFlags(), mapping.Depth());

	FromHSH<D>(bm->All3D(), level, coeffs2);
	delete[] coeffs2;
	return bm;
}

ISO_ptr<HDRbitmap> TestMapping(int size) {
	HemiSpheremapMapping	mapping(size);
	//ProbemapMapping	mapping(size);
	//CubemapMapping	mapping(size);

	int		w	= mapping.Width();
	int		h	= mapping.Height();

	ISO_ptr<HDRbitmap>	bm(0);
	bm->Create(w, h, mapping.BitmapFlags(), mapping.Depth());
	float	area = 0;
	for (int y = 0; y < h; ++y) {
		HDRpixel	*p	= bm->ScanLine(y);
		for (int x = 0; x < w; ++x, ++p) {
			float3	d;
			if (mapping.TexelToDir(x, y, 0, d)) {
				assign(*p, d);
				area += (p->a = mapping.GetSolidAngle(x, y, 0));
			}
		}
	}
	return bm;
}

//-----------------------------------------------------------------------------
//	perlin noise
//-----------------------------------------------------------------------------

bitmap PerlinNoise2D(int width, int height, float scale, uint64 seed) {
	if (seed == 0)
		seed = random_seed();
	if (scale == 0)
		scale = 1;

	perlin_noise	perlin(seed);
	float			*noise	= new float[width * height], *p = noise;
	bool			tiling	= scale < 0;
	float			s		= abs(scale) / width;

	if (tiling) {
		for (int y0 = 0; y0 < height; y0++) {
			float	y1	= y0 - height;
			float	ty	= y0 / float(height);
			for (int x0 = 0; x0 < width; x0++) {
				float	x1	= x0 - width;
				float	tx	= x0 / float(width);
				float	a	= perlin.noise2(x0 * s, y0 * s);
				float	b	= perlin.noise2(x1 * s, y0 * s);
				float	c	= perlin.noise2(x0 * s, y1 * s);
				float	d	= perlin.noise2(x1 * s, y1 * s);
				*p++		= lerp(lerp(a, b, tx), lerp(c, d, tx), ty);
			}
		}
	} else {
		for (int y = 0; y < height; y++) {
			for (int x = 0; x < width; x++)
				*p++ = perlin.noise2(x * s, y * s);
		}
	}

	bitmap		bm(width, height);
	ISO_rgba	*d	= bm.ScanLine(0);
	p = noise;
	for (int i = width * height; i--; )
		*d++ = int((*p++ + 1) * 127);

	delete[] noise;
	return bm;
}

ISO_ptr<HDRbitmap> GGX_pre(ISO_ptr<HDRbitmap> hdr) {
	return hdr;
}


//-----------------------------------------------------------------------------
//	init
//-----------------------------------------------------------------------------

static initialise init(
	ISO_get_operation2(Skydome2Cubemap,		(EnvironmentMap<SkydomeMapping,		CubemapMapping>)),
	ISO_get_operation2(Cubemap2Skydome,		(EnvironmentMap<CubemapMapping,		SkydomeMapping>)),
	ISO_get_operation2(Cubemap2Cross,		(EnvironmentMap<CubemapMapping,		CrossmapMapping>)),
	ISO_get_operation2(Cross2Cubemap,		(EnvironmentMap<CrossmapMapping,	CubemapMapping>)),
	ISO_get_operation2(Probemap2Cubemap,	(EnvironmentMap<ProbemapMapping,	CubemapMapping>)),
	ISO_get_operation2(Cubemap2Probemap,	(EnvironmentMap<CubemapMapping,		ProbemapMapping>)),
	ISO_get_operation2(Spheremap2Cubemap,	(EnvironmentMap<SpheremapMapping,	CubemapMapping>)),
	ISO_get_operation2(Cubemap2Spheremap,	(EnvironmentMap<CubemapMapping,		SpheremapMapping>)),
	ISO_get_operation2(Skydome2WiiCubemap,	(EnvironmentMap<SkydomeMapping,		WiiCubemapMapping>)),
	ISO_get_operation2(Cubemap2WiiCubemap,	(EnvironmentMap<CubemapMapping,		WiiCubemapMapping>)),
	ISO_get_operation2(Skydome2Spheremap,	(EnvironmentMap<SkydomeMapping,		SpheremapMapping>)),

	ISO_get_operation2(Skydome2CubemapG,	(EnvironmentMapGauss<SkydomeMapping,	CubemapMapping>)),
	ISO_get_operation(GaussianCubeMips),

	ISO_get_operation2(SH2Probemap,			FromSH<ProbemapMapping>),
	ISO_get_operation2(SH2Cubemap,			FromSH<CubemapMapping>),
	ISO_get_operation(ToSH),
	ISO_get_operation(ToHSH),
	ISO_get_operation2(HSHToProbemap,		FromHSH<ProbemapMapping>),
	ISO_get_operation2(HSHToCubemap,		FromHSH<CubemapMapping>),
	ISO_get_operation(TestMapping),
	ISO_get_operation(PerlinNoise2D),
	ISO_get_operation(GGX_pre)
);
