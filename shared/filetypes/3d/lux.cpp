#include "iff.h"
#include "base/array.h"
#include "base/list.h"
#include "base/vector.h"
#include "maths/geometry.h"
#include "filetypes/bitmap/bitmap.h"
#include "model_utils.h"
#include "extra/indexer.h"
#include "iso/iso_files.h"

using namespace iso;

#define	DECOUPLE_NORMALS
#define COLLISION_HIERARCHY

#define LUXTIME_TICKSPERSEC	60
#define	LUXTIME(t)	((t)*LUXTIME_TICKSPERSEC/TIME_TICKSPERSEC)
#define	MAXTIME(t)	((t)*TIME_TICKSPERSEC/LUXTIME_TICKSPERSEC)

//#define LUXPARTICLE
//#define LUXANIMATION

typedef int LuxTime;
class Lux;
class LuxHud;

class RefCount {
protected:
	int	refcount;
public:
	RefCount() : refcount(0) {}
	void addref() { refcount++; }
	int  release() { return --refcount; }
	int	 RefCounts() { return refcount; }
};

template<typename T> using	RCLink = link<ref_ptr<T>>;
template<typename T> using	RCList = list<ref_ptr<T>>;

typedef bitmap LuxBitmap;

template<typename R> uint32 read_str(R &r, char *p, uint32 n) {
	uint32	i = 0;
	while (i < n && (p[i++] = r.getc()));
	return i;
}

template<class C, class P> inline int get_index_if(const C &c, const P pred) {
	int	x = 0;
	for (auto i = begin(c), e = end(c);; ++i, ++x) {
		if (i == e)
			return -1;
		if (pred(*i))
			break;
	}
	return x;
}

template<class C, class T> inline int get_index(const C &c, const T &t) {
	return get_index_if(c, [t](const T &a) { return a == t;});
}

//----------------------------------------------------------------------
//	Substitutions
//----------------------------------------------------------------------

namespace substitute {
	char path[_MAX_PATH];
}

const char* _Substitute(char *path, const char *orig, const char *sub) {
	char	drive[_MAX_DRIVE], dir[_MAX_DIR], fname[_MAX_FNAME], ext[_MAX_EXT], fname2[_MAX_FNAME], ext2[_MAX_EXT];

	_splitpath(orig, drive, dir, fname, ext);
	_splitpath(fname, nullptr, nullptr, fname2, ext2);

	_makepath(path, drive, dir, fname2, sub);
	strcat(path, ext);
	return path;
}

const char* _MultiSubstitute(char *path, const char *orig, const char *sub) {
	char	drive[_MAX_DRIVE], dir[_MAX_DIR], fname[_MAX_FNAME], ext[_MAX_EXT], fname2[_MAX_FNAME], ext2[_MAX_EXT];

	_splitpath(orig, drive, dir, fname, ext);
	_splitpath(fname, nullptr, nullptr, fname2, ext2);

	const char	*dots[8];
	int			ndots = 0;
	const char	*dot = sub;
	do {
		dots[ndots++] = dot;
		dot = strchr(dot + 1, '.');
	} while (dot);
	dots[ndots] = sub + strlen(sub);

	for (int i = 1 << ndots; i--;) {
		_makepath(path, drive, dir, fname2, nullptr);
		char	*end = path + strlen(path);
		for (int j = 0; j < ndots; j++) {
			if (i & (1 << j)) {
				int	len = dots[j + 1] - dots[j];
				if (dots[j][0] != '.')
					*end++ = '.';
				memcpy(end, dots[j], len);
				end += len;
			}
		}
		strcpy(end, ext);
		if (_access(path, 0) == 0)
			return path;

	}

	return orig;
}

const char* _Substitute(const char *orig, const char *sub) { return _Substitute(substitute::path, orig, sub); }

const char* Substitute(char *path, const char *orig, const char *sub) { return orig && sub ? _MultiSubstitute(path, orig, sub) : orig; }
const char* Substitute(const char *orig, const char *sub) { return Substitute(substitute::path, orig, sub); }

//------------------------------------------------------
//
// LuxSound
//
//------------------------------------------------------

#define LUXSND_ATTACK	(1<<0)

class LuxSound : public RefCount {
public:
	int			flags	= 0;
	string		filename;
	LuxSound(const char *filename = nullptr) : filename(filename) {
#ifdef TWO_SOUNDS
		if (_filename && AddSubExists(_filename, "atk"))
			flags |= LUXSND_ATTACK;
#endif
	}
	LuxSound(Lux *lux, IFF_chunk &chunk);
	void		SetFilename(char *_filename) { filename = _filename; }
	bool		operator==(LuxSound &sound) { return filename == sound.filename; }
#ifdef TWO_SOUNDS
	bool		HasAttack() { return (flags & LUXSND_ATTACK) != 0; }
#endif
};

//------------------------------------------------------
//
// LuxSpline
//
//------------------------------------------------------

class LuxSpline : public RefCount, public dynamic_array<float3> {
public:
	int			offset;
	LuxSpline() : offset(-1) {};
	LuxSpline(IFF_chunk &chunk) {
		chunk.read(offset);
		while (chunk.remaining())
			push_back(chunk.get<float3p>());
	}
};

//------------------------------------------------------
//
// LuxString
//
//------------------------------------------------------

class LuxString : public RefCount {
	string		s;
public:
	int			offset;
	LuxString(IFF_chunk &chunk) {
		s.read(chunk, chunk.remaining());
	}
	LuxString(const char *_s, int len = 0) : offset(-1) {
		s = string(_s, len ? len : strlen(_s));
	};
	operator char*() { return s; }
	int Length() const { return s.size32(); }
};

//------------------------------------------------------
//
// LuxExtent
//
//------------------------------------------------------

class LuxExtent : public RefCount {
public:
	float3x4	mat;
	float3		ext;
	LuxExtent() : mat(identity) { clear(ext); }
	LuxExtent(const float3x4 &_mat, const float3 &_ext) : mat(_mat), ext(_ext) {}
};

//------------------------------------------------------
//
// LuxTint
//
//------------------------------------------------------

class LuxTint : public RefCount {
public:
	string		name;
	bool		dble, set2;
	dynamic_array<float3> colours;
	LuxTint(char *_name = nullptr) : name(_name), dble(true), set2(false) {}
	LuxTint(Lux *lux, IFF_chunk &chunk) {
		char	buffer[32];
		if (read_str(chunk, buffer, sizeof(buffer)))
			name = buffer + (set2 = buffer[0] == '*');

		for (int i = 0; chunk.remaining(); i++)
			colours[i] = chunk.get<float3p>();
	}
	void		SetDouble(bool _dble) {
		if (dble && !_dble) {
			for (int i = 0; i < colours.size(); i++)
				colours[i] /= 2;
			dble = false;
		}
	}
	bool		operator==(LuxTint &tint) {
		return stricmp(name, tint.name) == 0;
	}
};

//------------------------------------------------------
//
// LuxTexture
//
//------------------------------------------------------

#define LUXTEX_ANIM     		(1<<0)  // Animating texture
#define LUXTEX_INTENSITYALPHA	(1<<1)	// Calc alpha from intensity
#define LUXTEX_NAME				(1<<2)	// Has name (should be output to def file)
#define LUXTEX_BUMPMAP			(1<<3)	// This texture is a bumpmap
#define LUXTEX_SIGN				(1<<4)	// used as a sign
#define LUXTEX_MULTI			(1<<5)
#define LUXTEX_MIPMAP_1			(1<<8)	// Mipmap it
#define LUXTEX_MIPMAP			(15<<8)
#define LUXTEX_BLENDTEX			(1<<16) // Texture is a blend texture (Need to use variable alpha)
#define LUXTEX_PUNCHTHROUGH		(1<<17) // Texture has some transparent textures
#define LUXTEX_USED_ANIM		(1<<18)	// Texture is actually used in an animation
#define LUXTEX_HASBUMPMAP		(1<<19) // Has an asoociated bump map
#define LUXTEX_FADEOUT			(1<<22) // Fade it out
#define LUXTEX_CROP				(1<<23) // Texture is a crop of another texture.
#define LUXTEX_COL_PLUS_A0		(1<<24) // This texture is used with tint.
#define LUXTEX_ALPHA_PROCESSED	(1<<25) // The alpha flag for this texture has been updated
#define LUXTEX_CROP_TEXTURE     (1<<26) // This is texture is of LuxCropTexture type
#define LUXTEX_FORCE_32BIT      (1<<27) // This texture has to have 32 bit color format (for best quality)
#define LUXTEX_SHARED			(1<<31)	// This texture is shared

class LuxTexture : public RefCount {
public:
	virtual ISO_ptr<iso::bitmap>	_GetBitmap(char *sub = nullptr, float scale = 1.0f);
	ISO_ptr<bitmap>		bitmap;

	int					index	= -1;
	int					flags	= 0;
	string				name;
	string				diffuse;
	string				opacity;
	RCList<LuxTexture>	anim;

	LuxTexture(const char *di, const char *op = nullptr) : diffuse(di), opacity(op) {}

	LuxTexture(Lux *lux, IFF_chunk &chunk);
	virtual ~LuxTexture() {}
	void				SetName(char *_name) {
		name = _name;
		if (_name) {
			flags |= LUXTEX_NAME;
		} else {
			flags &= ~LUXTEX_NAME;
		}
	}
	void				SetFlags(int f) {
		flags |= f;
		if (flags & LUXTEX_ANIM) {
			for (auto i : anim)
				i->flags |= f;
		}
	}
	void				ClearFlags(int f) {
		flags &= ~f;
		if (flags & LUXTEX_ANIM) {
			for (auto &i : anim)
				i->flags &= ~f;
		}
	}
	ISO_ptr<iso::bitmap>		GetBitmap(char *sub = nullptr, float scale = 1.0f) { return bitmap ? bitmap : _GetBitmap(sub, scale); }
	int					Width() { return GetBitmap()->Width(); }
	int					Height() { return GetBitmap()->Height(); }
	void				Uncache() { bitmap = ISO_NULL; }
	bool				IsPowerOfTwo() { return is_pow2(Width()) && is_pow2(Height()); }
	virtual bool		operator==(LuxTexture &tex);
};

class LuxCropTexture : public LuxTexture {
	ISO_ptr<iso::bitmap>	_GetBitmap(char *sub = nullptr, float scale = 1.0f) override;

	float2				uvmin;
	float2				uvsize;

	float2				uvScale;
	float2				uvTrans;
public:
	LuxCropTexture(const LuxTexture& tex, const float2& uvmin, const float2& uvsize);

	bool		operator==(LuxTexture &tex) override;
	float2				UncropUV(const float2&) const;
	float2				Uvmin() const { return uvmin; }
	float2				Uvsize() const { return uvsize; }
};

ISO_ptr<iso::bitmap>	LuxTexture::_GetBitmap(char *sub, float scale) {
	if (flags & LUXTEX_ANIM)
		return !anim.empty() ? anim.front()->GetBitmap(sub) : (ISO_ptr<iso::bitmap>)ISO_NULL;

	bitmap = FileHandler::CachedRead(Substitute(diffuse, sub));
	if (!bitmap)
		bitmap = FileHandler::CachedRead(filename(diffuse).set_ext("ti3"));
	return bitmap;
}

bool LuxTexture::operator==(LuxTexture &tex) {
	// Bad programming:
	if ((tex.flags & LUXTEX_CROP_TEXTURE) && !(flags & LUXTEX_CROP_TEXTURE))
		return *(LuxCropTexture*)&tex == *this;

	return (diffuse ? (tex.diffuse && diffuse == tex.diffuse) : !tex.diffuse)
		&& (opacity ? ((tex.opacity && opacity == tex.opacity) || opacity == tex.diffuse) : !tex.opacity || tex.opacity == diffuse);
	//		&& ((flags ^ tex.flags) & (LUXTEX_INTENSITYALPHA)) == 0;
}

LuxCropTexture::LuxCropTexture(const LuxTexture& tex, const float2& _uvmin, const float2& _uvsize) : LuxTexture(tex.diffuse, tex.opacity), uvmin(_uvmin), uvsize(_uvsize) {
	//_debugprintf("NEW LuxCropTexture %08X tex=%08X (%s) [%.2f,%.2f %.2f,%.2f]\n", this, &tex, tex.diffuse ? tex.diffuse : "<nullptr>",
	//    uvmin.x, uvmin.y, uvsize.x, uvsize.y);
	index = -1;
	flags = tex.flags | LUXTEX_CROP_TEXTURE;
	name = tex.name;

	if (flags & LUXTEX_ANIM) {
		for (auto i : tex.anim) {
			anim.push_back(new LuxCropTexture(*i, uvmin, uvsize));
		}
	}

	uvScale = float2{1.0f / uvsize.x, 1.0f / uvsize.y};
	uvTrans = float2{uvmin.x, 1.0f - uvmin.y - uvsize.y} * uvScale;
}

float2 LuxCropTexture::UncropUV(const float2& uv) const {
	return uv * uvScale + uvTrans;
}

bool LuxCropTexture::operator==(LuxTexture &tex) {
	if (LuxTexture::operator==(tex)) {
		if (tex.flags & LUXTEX_CROP_TEXTURE) {
			LuxCropTexture* oCrop = (LuxCropTexture*)&tex;

			if (all(oCrop->uvmin == uvmin) && all(oCrop->uvsize == uvsize)) {
				return true;
			}
		}
	}

	return false;
}

ISO_ptr<iso::bitmap> LuxCropTexture::_GetBitmap(char *sub, float scale) {
	return LuxTexture::_GetBitmap(sub, scale);
}

//------------------------------------------------------
//
// LuxMaterial
//
//------------------------------------------------------

#define LUXMATF_BLEND     		(1<<0)  // Blended
#define LUXMATF_2SIDE			(1<<1)	// 2-sided material
#define LUXMATF_U_MIRROR		(1<<2)	// Texture is mirrored in U
#define LUXMATF_V_MIRROR		(1<<3)	// Texture is mirrored in V
#define LUXMATF_U_TILE			(1<<4)	// Texture is tiled in U
#define LUXMATF_V_TILE			(1<<5)	// Texture is tiled in V
#define LUXMATF_TEXTURED		(1<<6)	//
#define LUXMATF_OPACITY_MAP		(1<<7)	//
#define LUXMATF_SPECULAR_MAP	(1<<8)	//
#define LUXMATF_REFLECTION_MAP	(1<<9)	//
#define LUXMATF_UNLIT			(1<<10)	//
#define LUXMATF_SUBTRACT		(1<<11)
#define LUXMATF_ADD				(1<<12)
#define LUXMATF_DOUBLE			(1<<13)
#define LUXMATF_PUNCHTHROUGH	(1<<14)	// Opaque texture except partially completely transparent
#define LUXMATF_TRANS_BLEND		(1<<15)	// Translucent texture blended using opacity value.
#define LUXMATF_TEXTURE_ANIM	(1<<16)	// Animated texture
#define LUXMATF_TILEIRRELEVANT	(1<<17)	// Tiling flags irrelevant because no UVs cross tiling boundary
#define LUXMATF_REFRACTION_MAP	(1<<18)	//
#define LUXMATF_TINTMASK		(1<<19)	// Use alpha as a tint mask
#define LUXMATF_MIPMAP			(1<<20)	// Enable mipmapping
#define LUXMATF_TINTDOUBLE		(1<<21)
#define LUXMATF_NOALPHA			(1<<22)
#define LUXMATF_FADEOUT			(1<<23)
#define LUXMATF_TINT			(1<<24)
#define LUXMATF_SHININESS_MAP	(1<<25)	// Shininess map toggle in max material properties
#define LUXMATF_SIGN			(1<<26)
#define LUXMATF_NOVC			(1<<27)
#define LUXMATF_UV_FROM_POS		(1<<28)
#define LUXMATF_TEXHASALPHA		(1<<29)
#define LUXMATF_ANIMUV			(1<<30)
#define LUXMATF_USED			(1<<31)

class LuxMaterial : public RefCount {
public:
	int			flags;
	int			mask;
	int			eor;
	int			uv_channel;
	int			col_channel;
	float3		ambient;
	float3		diffuse;
	float3		specular;
	float		spec_str;
	float		spec_exp;
	float		opacity;
	float		bump_scale;
	float		shininess_scale;
	ref_ptr<LuxMaterial>	blendmat;
	ref_ptr<LuxMaterial>	bottommat;
	ref_ptr<LuxTexture>		texture;
	ref_ptr<LuxTexture>		bumpmap;
	ref_ptr<LuxTint>		tint;
	float2		uvcrop[2];
	float3x4	uvtrans;
	float		mm_scale, mm_offset;

	LuxMaterial() : flags(0), mask(0), eor(0), uv_channel(0), col_channel(0), blendmat(nullptr), bottommat(nullptr), texture(nullptr), bumpmap(nullptr), tint(nullptr) {}
	LuxMaterial(Lux *lux, IFF_chunk &chunk);
	~LuxMaterial() {}
	void		GetTextures(list<LuxTexture*> &list);

	bool		IsCropTexture() const { return any(uvcrop[0] != float2(zero)) || any(uvcrop[1] != float2(one)); }

	bool		operator==(const LuxMaterial &mat) const;
	// Compares the current materials without their sub-materials (blendmat and bottommat)
	bool		IsSame(const LuxMaterial &mat) const;
};

void LuxMaterial::GetTextures(list<LuxTexture*> &list) {
	if (texture && !find_check(list, texture))
		list.push_back(texture);
	if (blendmat)
		blendmat->GetTextures(list);
}

bool LuxMaterial::operator==(const LuxMaterial &mat) const {
	return
		mat.flags == flags
		&&	mat.opacity == opacity
		&& (!(mat.flags & LUXMATF_MIPMAP) || mat.mm_offset == mm_offset)
		&& (!mat.blendmat ? !blendmat : blendmat  && *mat.blendmat == *blendmat)
		&& (!mat.bottommat ? !bottommat : bottommat && *mat.bottommat == *bottommat)
		&& mat.texture == texture
		&&	mat.bumpmap == bumpmap
		&&	mat.tint == tint
		&&	mat.mask == mask
		&&	mat.eor == eor
		&& (texture || all(mat.diffuse == diffuse))
		&& (!texture || ((flags | mat.flags) & LUXMATF_TILEIRRELEVANT) || (all(mat.uvcrop[0] == uvcrop[0]) && all(mat.uvcrop[1] == uvcrop[1])))
		&& (!(flags & LUXMATF_SPECULAR_MAP) || (all(mat.specular == specular) && mat.spec_exp == spec_exp && mat.spec_str == spec_str))
		&& !(flags & LUXMATF_SIGN);
}

bool LuxMaterial::IsSame(const LuxMaterial &mat) const {
	return
		mat.flags == flags
		&&	mat.opacity == opacity
		&& (!(mat.flags & LUXMATF_MIPMAP) || mat.mm_offset == mm_offset)
		// &&	mat.blendmat	== blendmat
		// &&	mat.bottommat	== bottommat
		&& mat.texture == texture
		&&	mat.bumpmap == bumpmap
		&&	mat.tint == tint
		&&	mat.mask == mask
		&&	mat.eor == eor
		&& (texture || all(mat.diffuse == diffuse))
		&& (!texture || ((flags | mat.flags) & LUXMATF_TILEIRRELEVANT) || (all(mat.uvcrop[0] == uvcrop[0]) && all(mat.uvcrop[1] == uvcrop[1])))
		&& (!(flags & LUXMATF_SPECULAR_MAP) || (all(mat.specular == specular) && mat.spec_exp == spec_exp && mat.spec_str == spec_str))
		&& !(flags & LUXMATF_SIGN);
}

//------------------------------------------------------
//
// LuxLensflare
//
//------------------------------------------------------

class LuxLensflarePart {
public:
	float		offset;
	LuxTexture	*texture;
	float3		colour1;
	float3		colour2;

	LuxLensflarePart(float _offset, LuxTexture *_texture) : offset(_offset), texture(_texture), colour1(zero), colour2(zero) {}
};

class LuxLensflare : public RefCount {
public:
	float3		pos;
	float3		colour;
	dynamic_array<LuxLensflarePart>	parts;

	LuxLensflare() {}
	LuxLensflare(Lux *lux, IFF_chunk &chunk);
};

//------------------------------------------------------
//
// LuxVertex KEEP FOR BACKWARDS COMPATIBILTY
//
//------------------------------------------------------

class LuxVertex {
public:
	float3		pos;
	float3		normal;
	int			mapping;

	LuxVertex(float3 &_pos, float3 &_normal, int _mapping) : pos(_pos), normal(_normal), mapping(_mapping) {}
	LuxVertex(IFF_chunk &chunk) { read(chunk, pos, normal); }	//relies on order definition of pos, normal!
	bool		operator ==(const LuxVertex& v) { return all(v.pos == pos) && all(v.normal == normal); }
};



//------------------------------------------------------
//
// LuxUvSet
//
//------------------------------------------------------
#define LUXUVSET_UV_SETS 8
typedef float2 LuxUvSet[LUXUVSET_UV_SETS];


//------------------------------------------------------
//
// LuxColourSet
//
//------------------------------------------------------
#define LUXCOLSET_COL_SETS 8
typedef float3 LuxColSet[LUXCOLSET_COL_SETS];


//------------------------------------------------------
//
// LuxFace
//
//------------------------------------------------------

#define LUXFACE_UV	(1<<14)
#define LUXFACE_COL	(1<<15)
#define LUXFACE_MAT	(LUXFACE_UV-1)

class LuxFace {
public:
	int			flags;		// material ID
	int			v[3];		// vertex indices
#ifdef DECOUPLE_NORMALS
	float3		normal[3];
#endif
	LuxUvSet	uv[3];
	LuxColSet	col[3];
	LuxFace() {}
#ifdef DECOUPLE_NORMALS
	LuxFace(IFF_chunk &file, bool norms);
#else
	LuxFace(IFF_chunk &file);
#endif
	int			Mat() { return flags & LUXFACE_MAT; }
	void		ReadUvChannel(IFF_chunk &file, int nChannel);
	void		ReadColChannel(IFF_chunk &file, int nChannel);
};

#ifdef DECOUPLE_NORMALS
LuxFace::LuxFace(IFF_chunk &file, bool norms) {
	flags = file.get<uint16>();
	v[0] = file.get<uint16>();
	v[1] = file.get<uint16>();
	v[2] = file.get<uint16>();
	if (norms) {
		normal[0] = file.get<float3p>();
		normal[1] = file.get<float3p>();
		normal[2] = file.get<float3p>();
	}
	if (flags & LUXFACE_UV) {
		uv[0][0] = file.get<float2p>();
		uv[1][0] = file.get<float2p>();
		uv[2][0] = file.get<float2p>();
	} else
		uv[0][0] = uv[1][0] = uv[2][0] = zero;

	if (flags & LUXFACE_COL) {
		col[0][0] = file.get<float3p>();
		col[1][0] = file.get<float3p>();
		col[2][0] = file.get<float3p>();
	} else
		col[0][0] = col[1][0] = col[2][0] = zero;

	for (int i = 1; i < LUXUVSET_UV_SETS; i++) {
		uv[0][i] = uv[1][i] = uv[2][i] = zero;
	}
	for (int i = 1; i < LUXCOLSET_COL_SETS; i++) {
		col[0][i] = col[1][i] = col[2][i] = zero;
	}

}
#else
LuxFace::LuxFace(IFF_chunk &file) {
	flags = file.get<uint16>();
	v[0] = file.get<uint16>();
	v[1] = file.get<uint16>();
	v[2] = file.get<uint16>();
	if (flags & LUXFACE_UV) {
		uv[0] = file;
		uv[1] = file;
		uv[2] = file;
	}
	if (flags & LUXFACE_COL) {
		col[0] = file;
		col[1] = file;
		col[2] = file;
	}
}
#endif

void LuxFace::ReadUvChannel(IFF_chunk &file, int nChannel) {
	if (flags & LUXFACE_UV) {
		uv[0][nChannel] = file.get<float2p>();
		uv[1][nChannel] = file.get<float2p>();
		uv[2][nChannel] = file.get<float2p>();
	}
}

void LuxFace::ReadColChannel(IFF_chunk &file, int nChannel) {
	if (flags & LUXFACE_COL) {
		col[0][nChannel] = file.get<float3p>();
		col[1][nChannel] = file.get<float3p>();
		col[2][nChannel] = file.get<float3p>();
	}
}

//------------------------------------------------------
//
// LuxPatch
//
//------------------------------------------------------

class LuxPatch {
public:
	int			flags;		// material ID
	int			v[16];		// vertex indices
	float2		uv[4];
	float3		col[4];
	LuxPatch() {}
	LuxPatch(IFF_chunk &file);
	int			Mat() { return flags & LUXFACE_MAT; }
};

LuxPatch::LuxPatch(IFF_chunk &file) {
	flags = file.get<uint16>();
	for (int i = 0; i < 16; i++)
		v[i] = file.get<uint16>();

	if (flags & LUXFACE_UV) {
		uv[0] = file.get<float2p>();
		uv[1] = file.get<float2p>();
		uv[2] = file.get<float2p>();
		uv[3] = file.get<float2p>();
	}
	if (flags & LUXFACE_COL) {
		col[0] = file.get<float3p>();
		col[1] = file.get<float3p>();
		col[2] = file.get<float3p>();
		col[3] = file.get<float3p>();
	}
}

//------------------------------------------------------
//
// LuxModel
//
//------------------------------------------------------

#define LUXMOD_VERTEXANIM		(1<<0)
#define LUXMOD_LOWRES			(1<<1)
#define LUXMOD_NOFOG			(1<<2)
#define LUXMOD_FOG2				(1<<3)
#define LUXMOD_NOAA				(1<<4)
#define LUXMOD_NOZB				(1<<5)
#define LUXMOD_VERTEXCOLS		(1<<6)
#define LUXMOD_SHADEVERTEXCOLS	(1<<7)
#define LUXMOD_NOCMP			(1<<8)
#define LUXMOD_NOSORT			(1<<9)
#define LUXMOD_NOINTRATRANS		(1<<10)
#define LUXMOD_SHADOW			(1<<11)
#define LUXMOD_DOUBLECOLSETS	(1<<12)
#define LUXMOD_NOSHADE2NDSET	(1<<13)
#define LUXMOD_FORCEBFCULL		(1<<14)
#define LUXMOD_NOSHADOWS		(1<<15)
#define LUXMOD_NORADSHADOWS		(1<<16)
#define LUXMOD_SKIN				(1<<31)

class LuxModel : public RefCount {
public:
	int						flags;
#ifdef DECOUPLE_NORMALS
	dynamic_array<float3>		vertices;
#else
	dynamic_array<LuxVertex>		vertices;
#endif
	dynamic_array<LuxFace>		faces;
	dynamic_array<LuxPatch>		patches;
	dynamic_array<ref_ptr<LuxMaterial> >	materials;
	RCList<LuxLensflare>	lensflares;

	LuxModel() : flags(0) {}
	LuxModel(Lux *lux, IFF_chunk &chunk);
	~LuxModel() {}

	void		AddLensflare(Lux *lux, IFF_chunk &chunk) { lensflares.push_back(new LuxLensflare(lux, chunk)); }
#ifdef DECOUPLE_NORMALS
	int			AddVertex(float3 &v) { int i = vertices.size32(); vertices.push_back(v); return i; }
#else
	int			AddVertex(LuxVertex &v);
#endif
	float		CalcRadius(float3 &minext, float3 &maxext);
	void		InteriorBox(float3 &minext, float3 &maxext);
	void		GetTextures(list<LuxTexture> &list);
	void		GetAnimTextures(Lux *lux, list<LuxTexture> &list);
	int			NumFaces()	const { return faces.size32(); }
	int			NumPatches()const { return patches.size32(); }
	int			NumVerts()	const { return vertices.size32(); }
	int			NumMats()	const { return materials.size32(); }
	int			NumLF()		const { return lensflares.size32(); }
	int			NumSigns()	const;
};


LuxModel::LuxModel(Lux *lux, IFF_chunk &form) {
#ifdef DECOUPLE_NORMALS
	dynamic_array<LuxVertex>	oldverts;
	bool				hasmap = false;
#endif
	LuxMaterial *blendparent = nullptr, *bottomparent = nullptr;
	flags = 0;
	int nUVChan = 0;
	int nColChan = 0;

	while (form.remaining()) {
		IFF_chunk	chunk(form.istream());
		switch (chunk.id) {
			case 'HEAD':
				flags = chunk.get<uint32>();
				break;

#ifdef DECOUPLE_NORMALS
			case 'VERT': {
				while (chunk.remaining())
					oldverts.push_back(chunk);
				break;
			}

			case 'MAP ': {
				int	i = 0;
				while (chunk.remaining())
					oldverts[i++].mapping = chunk.get<uint16>();
				hasmap = true;
				break;
			}

			case 'VECT':
				while (chunk.remaining())
					vertices.push_back(chunk.get<float3p>());
				break;

#else
			case 'VERT': {
				while (chunk.remaining())
					vertices.push_back(1, &LuxVertex(chunk));
				for (int i = 0; i < vertices.Count(); i++)
					vertices[i].mapping = i;
				break;
			}

			case 'MAP ': {
				int	i = 0;
				while (chunk.remaining())
					vertices[i++].mapping = chunk.get<uint16>();
				break;
			}
#endif
			case 'MATL':
			case 'MAT1': {
				LuxMaterial *mat = new LuxMaterial(lux, chunk);
				if (blendparent)
					blendparent->blendmat = mat;
				else {
					if (bottomparent) {
						bottomparent->bottommat = mat;
						bottomparent = nullptr;
					} else {
						materials.push_back(mat);
						if (mat->flags & LUXMATF_DOUBLE)
							bottomparent = mat;
					}
				}
				blendparent = mat->flags & LUXMATF_BLEND ? mat : nullptr;
				break;
			}
			case 'LNFL':
				AddLensflare(lux, chunk);
				break;

			case 'FAUV': {
				int		n = chunk.get<uint16>();
				faces.resize(n);
				nUVChan++;
				for (int f = 0; f < n; f++)
					faces[f].ReadUvChannel(chunk, nUVChan);
				break;
			}

			case 'FACO': {
				int		n = chunk.get<uint16>();
				faces.resize(n);
				nColChan++;
				for (int f = 0; f < n; f++)
					faces[f].ReadColChannel(chunk, nColChan);
				break;
			}

			case 'FACE': {
				int		n = chunk.get<uint16>();
				faces.resize(n);
				nColChan = 0;
				nUVChan = 0;
#ifdef DECOUPLE_NORMALS
				if (oldverts.size()) {
					if (hasmap) {
						for (int i = 0; i < oldverts.size(); i++)
							vertices[oldverts[i].mapping] = oldverts[i].pos;
					} else {
						for (int i = 0; i < oldverts.size(); i++) {
							float3	pos = oldverts[i].pos;
							int		n = vertices.size32();
							int		j;
							for (j = 0; j < n && any(vertices[j] != pos); j++);
							if (j == n) vertices[j] = pos;
							oldverts[i].mapping = j;
						}
					}
					for (int f = 0; f < n; f++) {
						faces[f] = LuxFace(chunk, false);
						faces[f].normal[0] = oldverts[faces[f].v[0]].normal;
						faces[f].normal[1] = oldverts[faces[f].v[1]].normal;
						faces[f].normal[2] = oldverts[faces[f].v[2]].normal;
						faces[f].v[0] = oldverts[faces[f].v[0]].mapping;
						faces[f].v[1] = oldverts[faces[f].v[1]].mapping;
						faces[f].v[2] = oldverts[faces[f].v[2]].mapping;
					}
				} else {
					for (int f = 0; f < n; f++)
						faces[f] = LuxFace(chunk, true);
				}
#else
				for (int f = 0; f < n; f++)
					faces[f] = LuxFace(chunk);
#endif
				break;
			}
			case 'PTCH': {
				int		n = chunk.get<uint16>();
				patches.resize(n);
				for (int f = 0; f < n; f++)
					patches[f] = LuxPatch(chunk);
				break;
			}
		}
	}

}

float LuxModel::CalcRadius(float3 &minext, float3 &maxext) {
	float	radius = 0;
	minext = maxext = zero;
	for (int i = 0; i < vertices.size(); i++) {
#ifdef DECOUPLE_NORMALS
		float	len = iso::len(vertices[i]);
		if (len > radius) radius = len;
		minext = min(minext, vertices[i]);
		maxext = max(maxext, vertices[i]);
#else
		float	len = Length(vertices[i].pos);
		if (len > radius) radius = len;
		minext = Min(minext, vertices[i].pos);
		maxext = Max(maxext, vertices[i].pos);
#endif
	}
	return radius;
}

//------------------------------------------------------
//
// LuxCollision
//
//------------------------------------------------------

typedef enum { LUXCOLL_BOX, LUXCOLL_MESH, LUXCOLL_SOUP, LUXCOLL_SPHERE, LUXCOLL_CAPSULE } LUXCOLL_TYPE;

#ifdef COLLISION_HIERARCHY
class LuxCollision : public RefCount {
public:
	LUXCOLL_TYPE		type;
	ref_ptr<LuxCollision>	child, sibling;
	LuxCollision(LUXCOLL_TYPE _type) : type(_type), child(nullptr), sibling(nullptr) {}
	virtual ~LuxCollision() {}
	virtual bool Test(float3 &pos) = 0;
};
#else
class LuxCollision : public RefCount {
public:
	LUXCOLL_TYPE		type;
	LuxCollision(LUXCOLL_TYPE _type) : type(_type) {}
	~LuxCollision() override {}
	virtual bool Test(float3 &pos) = 0;
};
#endif

class LuxCollisionSphere : public LuxCollision {
public:
	int			extra;
	float3		centre;
	float		radius;

	LuxCollisionSphere(int _extra, float r, float3 &c) : LuxCollision(LUXCOLL_SPHERE), extra(_extra), centre(c), radius(r) {}
	LuxCollisionSphere(IFF_chunk &chunk) : LuxCollision(LUXCOLL_SPHERE) { extra = chunk.remaining() >= 20 ? chunk.get<uint32>() : chunk.get<uint16>(); centre = chunk.get<float3p>(); radius = chunk.get<float>(); }
	bool		Test(float3 &pos);
};

class LuxCollisionBox : public LuxCollision {
public:
	int			extra;
	float3		minext, maxext;

	LuxCollisionBox(int _extra, float3 &v1, float3 &v2) : LuxCollision(LUXCOLL_BOX), extra(_extra), minext(min(v1, v2)), maxext(max(v1, v2)) {}
	LuxCollisionBox(IFF_chunk &chunk) : LuxCollision(LUXCOLL_BOX) { extra = chunk.remaining() > 26 ? chunk.get<uint32>() : chunk.get<uint16>(); minext = chunk.get<float3p>(); maxext = chunk.get<float3p>(); }
	bool		Test(float3 &pos);
};

class LuxCollisionPlane {
public:
	float3		normal;
	float		dist;
	int			id;
	LuxCollisionPlane(int _id, float3 &_normal, float _dist) : normal(_normal), dist(_dist), id(_id) {}
	LuxCollisionPlane(int _id, float3 &p0, float3 &p1, float3 &p2) : normal(cross(normalise(p1 - p0), p2 - p1)), id(_id) { dist = dot(normal, p1); }
	bool		Inside(float3 &pos) { return dot(pos, normal) < dist; }
};

class LuxCollisionMesh : public LuxCollision, public dynamic_array<LuxCollisionPlane> {
public:
	LuxCollisionMesh() : LuxCollision(LUXCOLL_MESH) {}
	LuxCollisionMesh(IFF_chunk &chunk);
	~LuxCollisionMesh() {}
	int			Add(const LuxCollisionPlane &plane, bool compare = true);
	bool		Test(float3 &pos);
};

class LuxCollisionTri {
public:
	int			v[3];
	int			id;
	LuxCollisionTri(int _id, int v0, int v1, int v2) : id(_id) { v[0] = v0; v[1] = v1; v[2] = v2; }
};

class LuxCollisionSoup : public LuxCollision, public dynamic_array<LuxCollisionTri> {
public:
	dynamic_array<float3>	vertices;
	LuxCollisionSoup() : LuxCollision(LUXCOLL_SOUP) {}
	LuxCollisionSoup(IFF_chunk &chunk);
	~LuxCollisionSoup() {}
	int			Add(const LuxCollisionTri &tri, bool compare = true);
	bool		Test(float3 &pos);
};

class LuxCollisionCapsule : public LuxCollision {
public:
	int			extra;
	float		radius;
	float3		pt1, pt2;
	LuxCollisionCapsule(int _extra, float r, float3 &a, float3 &b) : LuxCollision(LUXCOLL_CAPSULE), extra(_extra), radius(r), pt1(a), pt2(b) {}
	LuxCollisionCapsule(IFF_chunk &chunk);
	bool		Test(float3 &pos);
};

bool LuxCollisionBox::Test(float3 &pos) {
	return	pos.x >= minext.x && pos.x <= maxext.x
		&&	pos.y >= minext.y && pos.y <= maxext.y
		&&	pos.z >= minext.z && pos.z <= maxext.z;
}

bool LuxCollisionSphere::Test(float3 &pos) {
	return	len2(pos - centre) < radius * radius;
}

bool LuxCollisionCapsule::Test(float3 &pos) {
	return	false;//len2(pos - centre) < radius * radius;
}

LuxCollisionCapsule::LuxCollisionCapsule(IFF_chunk &chunk) : LuxCollision(LUXCOLL_CAPSULE) {
	extra = chunk.get<uint32>();
	pt1 = chunk.get<float3p>();
	pt2 = chunk.get<float3p>();
	radius = chunk.get<float>();
}

LuxCollisionMesh::LuxCollisionMesh(IFF_chunk &chunk) : LuxCollision(LUXCOLL_MESH) {
	int	n = chunk.get<uint16>();
	for (int i = 0; i < n; i++) {
		int		id = chunk.get<uint16>();
		float3	Vec(chunk.get<float3p>());
		float	dist(chunk.get());
		Add(LuxCollisionPlane(id, Vec, dist), false);
	}
}

int LuxCollisionMesh::Add(const LuxCollisionPlane &plane, bool compare) {
	int	n = size32();
	if (compare)
		for (int i = 0; i < n; i++)
			if (dot(plane.normal, (*this)[i].normal) > 0.999 && fabs(plane.dist - (*this)[i].dist) < 1)
				return i;

	push_back(plane);
	return n;
}

bool LuxCollisionMesh::Test(float3 &pos) {
	for (int i = 0; i < size32(); i++)
		if (!(*this)[i].Inside(pos)) return false;
	return true;
}

LuxCollisionSoup::LuxCollisionSoup(IFF_chunk &form) : LuxCollision(LUXCOLL_SOUP) {
	while (form.remaining()) {
		IFF_chunk	chunk(form.istream());
		switch (chunk.id) {
			case 'VECT':
				while (chunk.remaining())
					vertices.push_back(chunk.get<float3p>());
				break;

			case 'FACE':
				while (chunk.remaining()) {
					int		id = chunk.get<uint16>();
					int		v0 = chunk.get<uint16>(), v1 = chunk.get<uint16>(), v2 = chunk.get<uint16>();
					Add(LuxCollisionTri(id, v0, v1, v2), false);
				}
				break;
			case 'FAC2':
				while (chunk.remaining()) {
					int		id = chunk.get<uint32>();
					int		v0 = chunk.get<uint16>(), v1 = chunk.get<uint16>(), v2 = chunk.get<uint16>();
					Add(LuxCollisionTri(id, v0, v1, v2), false);
				}
				break;
		}
	}
}

int LuxCollisionSoup::Add(const LuxCollisionTri &tri, bool compare) {
	int n = size32();
	/*	if (compare)
			for (int i = 0; i < size(); i++)
				if ( dot( plane.normal, (*this)[i].normal ) > 0.999 && fabs(plane.dist - (*this)[i].dist) < 1 )
					return i;
	*/
	push_back(tri);
	return n;
}

bool LuxCollisionSoup::Test(float3 &pos) {
	return false;
}

//------------------------------------------------------
//
// LuxPart
//
//------------------------------------------------------

class LuxPart : public RefCount {
	void				Invalid() { tex = nullptr; nFrames = 0; }
public:
	ref_ptr<LuxTexture>	tex;
	int					nFrames;

	LuxPart(Lux *lux, const char *filename, int frames);
	LuxPart(Lux *lux, IFF_chunk &chunk);

	bool				operator==(LuxPart &part);
};

//------------------------------------------------------
//
// LuxParam
//
//------------------------------------------------------

class LuxProxy : public RefCount {
public:
	int			id;
	string		filename;

	LuxProxy(int _id, const char *filename) : id(_id), filename(filename) {}
	LuxProxy(IFF_chunk &chunk) {
		id = chunk.get<uint32>();
		filename = 0;
		if (auto n = chunk.remaining())
			filename.read(chunk, n);
	}
};

typedef enum {
	LUXPARAM_INT, LUXPARAM_FLOAT, LUXPARAM_NODE, LUXPARAM_RECT, LUXPARAM_SPLINE, LUXPARAM_SOUND,
	LUXPARAM_PART, LUXPARAM_ENT, LUXPARAM_LUX, LUXPARAM_ENUM, LUXPARAM_BOOL, LUXPARAM_COLOR,
	LUXPARAM_RECTSPAWN, LUXPARAM_STRING, LUXPARAM_HUD, LUXPARAM_TEX, LUXPARAM_ANIM
} LUXPARAM_TYPE;

class LuxParam : public RefCount {
public:
	string		name;
	void			SetName(const char *_name) { name = _name; }

	virtual	LUXPARAM_TYPE	Type() = 0;
};

class LuxParamInt : public LuxParam {
public:
	int				value;
	int				size;
	LuxParamInt(int _value, int _size = 32) : value(_value), size(_size) {}
	LuxParamInt(IFF_chunk &chunk) : value(chunk.get<uint32>()), size(chunk.get<uint32>()) {}
	LUXPARAM_TYPE	Type() { return LUXPARAM_INT; }
};

class LuxParamFloat : public LuxParam {
public:
	float			value;
	LuxParamFloat(float _value) : value(_value) {}
	LuxParamFloat(IFF_chunk &chunk) : value(chunk.get<float>()) {}
	LUXPARAM_TYPE	Type() { return LUXPARAM_FLOAT; }
};

class LuxParamNode : public LuxParam {
public:
	class LuxNode*	value;
	LuxParamNode(class LuxNode *_value) : value(_value) {}
	LuxParamNode(Lux *lux, IFF_chunk &chunk);
	LUXPARAM_TYPE	Type() { return LUXPARAM_NODE; }
};

class LuxParamExtent : public LuxParam {
public:
	LuxExtent			value;
	LuxParamExtent(const LuxExtent &_value) : value(_value) {}
	LuxParamExtent(IFF_chunk &chunk) {}
	LUXPARAM_TYPE		Type() { return LUXPARAM_RECT; }
};

class LuxParamSpline : public LuxParam {
public:
	ref_ptr<LuxSpline>	value;
	LuxParamSpline(LuxSpline *_value) : value(_value) {}
	LuxParamSpline(Lux *lux, IFF_chunk &chunk);
	LUXPARAM_TYPE		Type() { return LUXPARAM_SPLINE; }
};

class LuxParamSound : public LuxParam {
public:
	ref_ptr<LuxSound>		value;
	LuxParamSound(LuxSound *_value) : value(_value) {}
	LuxParamSound(IFF_chunk &chunk) { char filename[_MAX_PATH]; read_str(chunk, filename, _MAX_PATH); value = *filename ? new LuxSound(filename) : nullptr; }
	LUXPARAM_TYPE		Type() { return LUXPARAM_SOUND; }
};

class LuxParamPart : public LuxParam {
public:
	ref_ptr<LuxPart>		value;
	LuxParamPart(LuxPart *_value) : value(_value) {}
	LuxParamPart(Lux *lux, const char *_filename);
	LuxParamPart(Lux *lux, IFF_chunk &chunk);
	LUXPARAM_TYPE		Type() { return LUXPARAM_PART; }
};

class LuxParamEnt : public LuxParam {
public:
	int					value;
	LuxParamEnt(int _value) : value(_value) {}
	LuxParamEnt(IFF_chunk &chunk) : value(chunk.get<uint32>()) {}
	LUXPARAM_TYPE		Type() { return LUXPARAM_ENT; }
};

class LuxParamLux : public LuxParam {
public:
	char				nodeName[_MAX_PATH];
	LuxParamLux(char *_nodeName) { strcpy(nodeName, _nodeName); }
	LuxParamLux(IFF_chunk &chunk) { read_str(chunk, nodeName, _MAX_PATH); }
	LUXPARAM_TYPE		Type() { return LUXPARAM_LUX; }
};

class LuxParamEnum : public LuxParam {
public:
	int					value;
	LuxParamEnum(int _value) : value(_value) {}
	LuxParamEnum(IFF_chunk &chunk) : value(chunk.get<uint32>()) {}
	LUXPARAM_TYPE		Type() { return LUXPARAM_ENUM; }
};

class LuxParamBool : public LuxParam {
public:
	int					value;
	LuxParamBool(int _value) : value(_value) {}
	LuxParamBool(IFF_chunk &chunk) : value(chunk.get<uint32>()) {}
	LUXPARAM_TYPE		Type() { return LUXPARAM_BOOL; }
};

class LuxParamColor : public LuxParam {
public:
	int					value;
	LuxParamColor(int _value) : value(_value) {}
	LuxParamColor(IFF_chunk &chunk) : value(chunk.get<uint32>()) {}
	LUXPARAM_TYPE		Type() { return LUXPARAM_COLOR; }
};

class LuxParamExtentSpawn : public LuxParam {
public:
	LuxExtent			value;
	LuxParamExtentSpawn(const LuxExtent &_value) : value(_value) {}
	LuxParamExtentSpawn(IFF_chunk &chunk) {}//		: value(...);
	LUXPARAM_TYPE		Type() { return LUXPARAM_RECTSPAWN; }
public:
};

class LuxParamString : public LuxParam {
public:
	ref_ptr<LuxString>	value;
	LuxParamString(const char *_value, int _len = 0) : value(_value ? new LuxString(_value, _len) : nullptr) {}
	LuxParamString(IFF_chunk &chunk);
	LUXPARAM_TYPE		Type() { return LUXPARAM_STRING; }

};

class LuxParamHud : public LuxParam {
public:
	ref_ptr<LuxHud>		value;
	char				filename[_MAX_PATH];
	LuxParamHud(Lux *lux, char *_filename);
	LuxParamHud(Lux *lux, IFF_chunk &chunk);
	LUXPARAM_TYPE		Type() { return LUXPARAM_HUD; }
	void				Load(Lux *lux);
};

class LuxParamTex : public LuxParam {
public:
	ref_ptr<LuxTexture>	value;
	char				filename[_MAX_PATH];
	LuxParamTex(Lux *lux, const char *_filename);
	LuxParamTex(Lux *lux, IFF_chunk &chunk);
	LUXPARAM_TYPE		Type() { return LUXPARAM_TEX; }
	void				Load(Lux *lux);
};

class LuxParamAnim : public LuxParam {
public:
	char				filename[_MAX_PATH];
	LuxParamAnim(Lux *lux, char *_filename);
	LuxParamAnim(Lux *lux, IFF_chunk &chunk);
	LUXPARAM_TYPE		Type() { return LUXPARAM_ANIM; }
	void				Load(Lux *lux);
};


//------------------------------------------------------
//
// LuxAnimation
//
//------------------------------------------------------

enum LUXANIM_TYPE {
	LUXANIM_ROTATION,
	LUXANIM_POSITION,
	LUXANIM_SCALE,
	LUXANIM_VERTEX,
	LUXANIM_TEXTURE,
	LUXANIM_ENDMARKER,
	LUXANIM_ZOOM,
	LUXANIM_QUAT,
	LUXANIM_NOTE,
	LUXANIM_SOUND,
	LUXANIM_BLUR,
	LUXANIM_CAMFX,
	LUXANIM_COLOUR,
	LUXANIM_NEARZ,
	LUXANIM_OPACITY,
	LUXANIM_UV,
	LUXANIM_STARTMARKER		// so the start time is actually saved
};

class LuxAnimation : public RefCount {
public:
	LuxTime		time;
	LUXANIM_TYPE type;
	LuxAnimation(LuxTime t, LUXANIM_TYPE _type) : time(t), type(_type) { ISO_ASSERT(time >= 0); }
	LuxAnimation(IFF_chunk &chunk, LUXANIM_TYPE _type) : time(chunk.get<uint32>()), type(_type) { ISO_ASSERT(time >= 0); }
	LuxAnimation() {}
	virtual	~LuxAnimation() {}
	virtual LuxAnimation* dup(LuxTime offset = 0) = 0;
};

class LuxAnimFloat : public LuxAnimation {
public:
	float		value;
	LuxAnimFloat() {}
	LuxAnimFloat(LuxTime t, LUXANIM_TYPE _type) : LuxAnimation(t, _type) {}
	LuxAnimFloat(IFF_chunk &chunk, LUXANIM_TYPE _type) : LuxAnimation(chunk, _type) {}
};

class LuxAnimRotation : public LuxAnimation {
public:
	float3		rotation;
	LuxAnimRotation(LuxTime t, float3 &v) : LuxAnimation(t, LUXANIM_ROTATION), rotation(v) {}
	LuxAnimRotation(IFF_chunk &chunk) : LuxAnimation(chunk, LUXANIM_ROTATION), rotation(chunk.get<float3p>()) {}
	LuxAnimation* dup(LuxTime offset) { return new LuxAnimRotation(time + offset, rotation); }
};

class LuxAnimQuaternion : public LuxAnimation {
public:
	float4		quaternion;
	LuxAnimQuaternion(LuxTime t, float4 &q) : LuxAnimation(t, LUXANIM_QUAT), quaternion(q) {}
	LuxAnimQuaternion(IFF_chunk &chunk) : LuxAnimation(chunk, LUXANIM_QUAT), quaternion(chunk.get<float4p>()) {}
	LuxAnimation* dup(LuxTime offset) { return new LuxAnimQuaternion(time + offset, quaternion); }
};

class LuxAnimPosition : public LuxAnimation {
public:
	float3		position;
	LuxAnimPosition(LuxTime t, float3 &v) : LuxAnimation(t, LUXANIM_POSITION), position(v) {}
	LuxAnimPosition(IFF_chunk &chunk) : LuxAnimation(chunk, LUXANIM_POSITION), position(chunk.get<float3p>()) {}
	LuxAnimation* dup(LuxTime offset) { return new LuxAnimPosition(time + offset, position); }
};

class LuxAnimScale : public LuxAnimation {
public:
	float3		scale;
	LuxAnimScale(LuxTime t, float3 &v) : LuxAnimation(t, LUXANIM_SCALE), scale(v) {}
	LuxAnimScale(IFF_chunk &chunk) : LuxAnimation(chunk, LUXANIM_SCALE), scale(chunk.get<float3p>()) {}
	LuxAnimation* dup(LuxTime offset) { return new LuxAnimScale(time + offset, scale); }
};

#ifdef DECOUPLE_NORMALS

class LuxAnimVertex : public LuxAnimation {
public:
	dynamic_array<float3>	vertices;
	LuxAnimVertex(LuxTime t) : LuxAnimation(t, LUXANIM_VERTEX) {}
	LuxAnimVertex(IFF_chunk &chunk);
	~LuxAnimVertex() {}
	LuxAnimation* dup(LuxTime offset) { return new LuxAnimVertex(time + offset); }
};

#else

class LuxAnimVertex : public LuxAnimation {
public:
	dynamic_array<LuxVertex>	vertices;
	LuxAnimVertex(LuxTime t) : LuxAnimation(t, LUXANIM_VERTEX) {}
	LuxAnimVertex(IFF_chunk &chunk);
	~LuxAnimVertex() {}
	LuxAnimation* dup(LuxTime offset) { return new LuxAnimVertex(time + offset); }
};

#endif

class LuxAnimTexture : public LuxAnimation {
public:
	LuxTexture	*animtex, *tex;
	LuxAnimTexture(LuxTime t, LuxTexture *_animtex, LuxTexture *_tex) : LuxAnimation(t, LUXANIM_TEXTURE), animtex(_animtex), tex(_tex) {}
	LuxAnimTexture(Lux *lux, IFF_chunk &chunk);
	LuxAnimation* dup(LuxTime offset) { return new LuxAnimTexture(time + offset, animtex, tex); }
};

class LuxAnimEndMarker : public LuxAnimation {
public:
	int			flags;
	LuxAnimEndMarker(LuxTime t) : LuxAnimation(t, LUXANIM_ENDMARKER), flags(0) {}
	LuxAnimEndMarker(LuxTime t, int _flags) : LuxAnimation(t, LUXANIM_ENDMARKER), flags(_flags) {}
	LuxAnimEndMarker(IFF_chunk &chunk) : LuxAnimation(chunk, LUXANIM_ENDMARKER), flags(chunk.remaining() ? chunk.get<uint32>() : 0) {}
	LuxAnimation* dup(LuxTime offset) { return new LuxAnimEndMarker(time + offset, flags); }
};

class LuxAnimStartMarker : public LuxAnimation {
public:
	LuxAnimStartMarker(LuxTime t) : LuxAnimation(t, LUXANIM_STARTMARKER) {}
	LuxAnimStartMarker(IFF_chunk &chunk) : LuxAnimation(chunk, LUXANIM_STARTMARKER) {}
	LuxAnimation* dup(LuxTime offset) { return new LuxAnimStartMarker(time + offset); }
};

class LuxAnimZoom : public LuxAnimFloat {
public:
	LuxAnimZoom(LuxTime t, float _zoom) : LuxAnimFloat(t, LUXANIM_ZOOM) { value = _zoom; }
	LuxAnimZoom(IFF_chunk &chunk) : LuxAnimFloat(chunk, LUXANIM_ZOOM) { chunk.read(value); }
	LuxAnimation* dup(LuxTime offset) { return new LuxAnimZoom(time + offset, value); }
};

class LuxAnimNote : public LuxAnimation {
public:
	char		note[256];
	LuxAnimNote(LuxTime t, char *_note) : LuxAnimation(t, LUXANIM_NOTE) { strcpy(note, _note); }
	LuxAnimNote(IFF_chunk &chunk) : LuxAnimation(chunk, LUXANIM_NOTE) { read_str(chunk, note, 256); }
	LuxAnimation* dup(LuxTime offset) { return new LuxAnimNote(time + offset, note); }
};

class LuxAnimSound : public LuxAnimation {
public:
	LuxSound	*sound;
	float		minVol, minVolAt, maxVol, maxVolAt;
	float		pitch;
	float		randVol, randPitch;
	void		SetDefaults() {
		minVol = 0.0f;
		minVolAt = 100.0f;
		maxVol = 100.0f;
		maxVolAt = 0.0f;
		pitch = 100.0f;
		randVol = 0.0f;
		randPitch = 0.0f;
	}
	LuxAnimSound(LuxTime t, LuxSound *_sound) : LuxAnimation(t, LUXANIM_SOUND), sound(_sound) { SetDefaults(); }
	LuxAnimSound(Lux *lux, IFF_chunk &chunk);
	LuxAnimation* dup(LuxTime offset) { return new LuxAnimSound(time + offset, sound); }
};

class LuxAnimBlur : public LuxAnimation {
public:
	int			alpha;
	int			scale;
	LuxAnimBlur(LuxTime t, float _alpha, int _scale) : LuxAnimation(t, LUXANIM_BLUR) {
		if (_alpha >= 100.0f)
			alpha = 0x8000;
		else if (_alpha <= 0.0f)
			alpha = 0x0;
		else
			alpha = int(0x8000 * _alpha / 100.0f);
		if (_scale <= 0)
			scale = 0;
		else
			scale = _scale;
	}
	LuxAnimBlur(IFF_chunk &chunk) : LuxAnimation(chunk, LUXANIM_BLUR) { read(chunk, alpha, scale); }
	LuxAnimation* dup(LuxTime offset) { return new LuxAnimBlur(time + offset, alpha, scale); }
};

class LuxAnimCamFx : public LuxAnimation {
public:
	float		focus;
	float		radius;
	int			alpha;
	int			blur;
	LuxAnimCamFx(LuxTime t, float _alpha, float _focus, float _radius, float _blur) : LuxAnimation(t, LUXANIM_CAMFX) {
		if (_alpha >= 100.0f)
			alpha = 0x8000;
		else if (_alpha <= 0.0f)
			alpha = 0x0;
		else
			alpha = int(0x8000 * _alpha / 100.0f);
		blur = max(0, int(_blur * 0x100));
		focus = _focus;
		radius = _radius;
	}
	LuxAnimCamFx(IFF_chunk &chunk) : LuxAnimation(chunk, LUXANIM_BLUR) {
		read(chunk, alpha, focus, radius, blur);
	}
	LuxAnimation* dup(LuxTime offset) { return new LuxAnimCamFx(time + offset, alpha, focus, radius, blur); }
};

class LuxAnimColour : public LuxAnimation {
public:
	float		r, g, b, a;
	LuxAnimColour(LuxTime t, float _r, float _g, float _b, float _a = 0.0f) : LuxAnimation(t, LUXANIM_COLOUR),
		r(_r), g(_g), b(_b), a(_a) {
	}
	LuxAnimColour(IFF_chunk &chunk) : LuxAnimation(chunk, LUXANIM_COLOUR) {
		r = chunk.get<float>();
		g = chunk.get<float>();
		b = chunk.get<float>();
		a = chunk.get<float>();
	}
	LuxAnimation* dup(LuxTime offset) { return new LuxAnimColour(time + offset, r, g, b, a); }
};

class LuxAnimNearZ : public LuxAnimFloat {
public:
	LuxAnimNearZ(LuxTime t, float _zoom) : LuxAnimFloat(t, LUXANIM_NEARZ) { value = _zoom; }
	LuxAnimNearZ(IFF_chunk &chunk) : LuxAnimFloat(chunk, LUXANIM_NEARZ) { chunk.read(value); }
	LuxAnimation* dup(LuxTime offset) { return new LuxAnimNearZ(time + offset, value); }
};

class LuxAnimOpacity : public LuxAnimFloat {
public:
	LuxAnimOpacity(LuxTime t, float _zoom) : LuxAnimFloat(t, LUXANIM_OPACITY) { value = _zoom; }
	LuxAnimOpacity(IFF_chunk &chunk) : LuxAnimFloat(chunk, LUXANIM_OPACITY) { chunk.read(value); }
	LuxAnimation* dup(LuxTime offset) { return new LuxAnimOpacity(time + offset, value); }
};

class LuxAnimUV : public LuxAnimation {
public:
	float2		trans;
	float2		scale;
	float		rot;
	LuxAnimUV(LuxTime t, float2 &_trans, float2 &_scale, float _rot) : LuxAnimation(t, LUXANIM_UV), trans(_trans), scale(_scale), rot(_rot) {}
	LuxAnimUV(IFF_chunk &chunk) : LuxAnimation(chunk, LUXANIM_UV), trans(chunk.get<float2p>()), scale(one), rot(0) { if (chunk.remaining()) { scale = chunk.get<float2p>(); rot = chunk.get<float>(); }; }
	LuxAnimation* dup(LuxTime offset) { return new LuxAnimUV(time + offset, trans, scale, rot); }
};

//------------------------------------------------------
//
// LuxSkin
//
//------------------------------------------------------

#define LUX_MAX_WEIGHTS	4

class LuxSkinVertexNode {
public:
	float3			pos;
	float			weight;
	class LuxNode*	node;
};

class LuxSkinVertex {//: public dynamic_array<LuxSkinVertexNode> {
public:
	int				n;
	LuxSkinVertexNode	nodes[LUX_MAX_WEIGHTS];
	int				Count() { return n; }
	LuxSkinVertexNode &operator[](int i) { return nodes[i]; }
	void		Read(Lux *lux, IFF_chunk &chunk);
};

class LuxSkin : public RefCount, public dynamic_array<LuxSkinVertex> {
public:
	LuxSkin() {}
	LuxSkin(Lux *lux, IFF_chunk &form);
};

//------------------------------------------------------
//
// LuxLight
//
//------------------------------------------------------
typedef enum {
	LUXLIGHT_AMB,
	LUXLIGHT_DIR,
	LUXLIGHT_POS,
	LUXLIGHT_SPOT,
	LUXLIGHT_POS_STATIC,
	LUXLIGHT_SPOT_STATIC,
	LUXLIGHT_BG = 11,
	LUXLIGHT_FOG = 12,
} LUXLIGHT_TYPE;

class LuxLight {
public:
	LUXLIGHT_TYPE	type;
	float3			colour;
	float			range;
	float			attenuate;
	float			hotspot;
	float			falloff;

	LuxLight() {}
	LuxLight(IFF_chunk &file) {
		type = (LUXLIGHT_TYPE)file.get<uint16>();
		colour = file.get<float3p>();
		range = file.get<float>();
		attenuate = file.get<float>();
		hotspot = file.get<float>();
		falloff = file.get<float>();
	}

};


//------------------------------------------------------
//
// LuxShadow
//
//------------------------------------------------------
class LuxShadow;

class LuxShadowPoly : public e_link<LuxShadowPoly> {
public:
	float3						normal;
	dynamic_array<int>				vertIndexes;

	LuxShadowPoly() {}
	LuxShadowPoly(IFF_chunk &chunk);
	void						GetNormal(LuxShadow *shadow);
	void						Merge(LuxShadowPoly *poly, int myEdge, int hisEdge);
	LuxShadowPoly				&operator=(const LuxShadowPoly &poly);
};

class LuxShadow : public RefCount {
protected:
	void						Optimize();

public:
	dynamic_array<float3>			verts;
	e_list<LuxShadowPoly>		polys;

	LuxShadow() {}
	LuxShadow(IFF_chunk &form) { Load(form); }
	void						Load(IFF_chunk &form);
	float						GetRadius(float3 *center);
	LuxModel					*MakeModel(LuxModel *pAppend = nullptr);
};

class LuxShadowList : public RefCount, public RCList<LuxShadow> {
};


//------------------------------------------------------
//
// LuxOccluder
//
//------------------------------------------------------
class LuxOccluder : public LuxShadow {
public:
	LuxOccluder() {}
	LuxOccluder(IFF_chunk &form) { Load(form); }
};

class LuxOccluderList : public RefCount, public RCList<LuxOccluder> {
};


//------------------------------------------------------
//
// LuxNode
//
//------------------------------------------------------
typedef enum {
	LUXNODE_NORMAL,
	LUXNODE_ATTACH,
	LUXNODE_LOWRES,
	LUXNODE_DAMAGE,
	LUXNODE_DEBRIS,
	LUXNODE_TOPPLE,
	LUXNODE_ICON,
	LUXNODE_SFX,
	LUXNODE_SUBSTITUTE,
	LUXNODE_CAMERA,
	LUXNODE_LIGHT,
	LUXNODE_REFERENCE,
	LUXNODE_REFERENCED,
	LUXNODE_OCCLUDER,
	LUXNODE_SHADOW,
	LUXNODE_CUTOUT,
	LUXNODE_SEAM,
} LUXNODE_TYPE;

#define LUXNODE_NOLOOP			0x001000
#define	LUXNODE_SPRITE			0x002000
#define LUXNODE_BONE			0x004000
#define LUXNODE_FLAT			0x008000
#define LUXNODE_USED			0x010000
#define LUXNODE_NOANIMATE		0x040000
#define LUXNODE_NOKEYS			0x080000
#define LUXNODE_ALLOWCUTS		0x100000
#define LUXNODE_FORCEANIMATE	0x200000
#define LUXNODE_NOCOMPRESS		0x400000
#define LUXNODE_NOOPTANIM		0x800000

#ifdef COLLISION_HIERARCHY
class LuxCollisionList : public RefCount {
public:
	ref_ptr<LuxCollision>	root;
	int				size();
	LuxCollision	*operator[](int i);
	void			push_back(LuxCollision *col);
};

int LuxCollisionList::size() {
	int	n = 0;
	for (LuxCollision *col = root; col; col = col->sibling)
		n++;
	return n;
}

LuxCollision *LuxCollisionList::operator[](int i) {
	LuxCollision *col;
	for (col = root; col && i--; col = col->sibling);
	return col;
}

void LuxCollisionList::push_back(LuxCollision *col) {
	LuxCollision *child = root;
	if (child) {
		while (LuxCollision *sib = child->sibling)
			child = sib;
		child->sibling = col;
	} else
		root = col;
}

#else
class LuxCollisionList : public RefCount, public RCList<LuxCollision> {};
#endif

class LuxParamList : public RefCount, public RCList<LuxParam> {
public:
	LuxParam*		Find(const char *name) const {
		for (auto &i : *this)
			if (stricmp(i->name, name) == 0)
				return i;
		return nullptr;
	}

};

class LuxAnimationList : public RefCount, public RCList<LuxAnimation> {
	iterator	insert;
	void		*controller;
public:
	LuxAnimationList() : insert(nullptr), controller(nullptr) {}
	LuxAnimationList(void *c) : insert(nullptr), controller(c) {}
	void	Insert(LuxAnimation *anim);
	void	Sort();
	void	*Controller() { return controller; }
	int		NumAnimationLoops();
	LuxTime	StartTime() { return empty() ? 0 : front()->time; }
	LuxTime	EndTime() { return empty() ? 0 : back()->time; }
	bool	IsLooping();
	void	ClearInsert() { insert = iterator(); }
};

class LuxAnimListList : public RefCount, public RCList<LuxAnimationList> {
public:
	LuxAnimationList	*NewAnimList(void *c);
	LuxAnimationList	*FindAnimList(void *c);
	int					LoopIndex(LuxAnimationList *animList);
	int					CountLoops();
};


class LuxNode : public RefCount {
public:
	string			name;
	int				flags;
	float3			position, rotation;
	LuxNode			*parent, *child, *sibling;
	LuxModel		*model;
	LuxLight		light;
	float			zoom;
	int				entity_type;
	string			entity_name;
	int				proxy;
	ref_ptr<LuxSkin>			skin;
	ref_ptr<LuxCollisionList>	collisions;
	ref_ptr<LuxAnimationList>	animations;
	ref_ptr<LuxParamList>		params;
	ref_ptr<LuxShadowList>		shadows;
	ref_ptr<LuxOccluderList>	occluders;

	LuxNode(LuxNode *_parent, const char *_name);
	LuxNode(Lux *lux, IFF_chunk &form);
	LuxNode(LuxNode &node);

	void		SetParent(LuxNode *_parent);
	void		SetName(const char *_name);
	void		SetEntity(const char *_entName);
	void		SetType(LUXNODE_TYPE type) { flags = (flags & ~0xff) | type; }
	LUXNODE_TYPE Type()			const { return LUXNODE_TYPE(flags & 0xff); }
	bool		Flat()			const { return !!(flags & LUXNODE_FLAT); }
	bool		IsBone()		const { return !!(flags & LUXNODE_BONE); }
	int			Extra();

	int			NumCollisions() { return collisions->size(); }
	LuxCollision* Collision(int i) { return (*collisions)[i]; }
#ifdef COLLISION_HIERARCHY
	LuxCollision* CollisionRoot() { return collisions->root; }
	void		AddCollision(LuxCollision *col) { collisions->push_back(col); }
	bool		HasCollisions() { return collisions && collisions->root; }
#else
	RCLink<LuxCollision>* CollisionHead() { return collisions->Head(); }
	void		AddCollision(LuxCollision *col) { collisions->push_back(col); }
	bool		HasCollisions() { return collisions && !collisions->empty(); }
#endif

	int			NumAnimations() { return animations ? animations->size32() : 0; }
	//	RCLink<LuxAnimation>* AnimationHead() { return animations ? animations->begin() : nullptr; }
	void		NewAnimList(Lux *lux, void *controller);
	void		AddAnimation0(LuxAnimation *anim) {
		ISO_ASSERT(animations);
		animations->push_back(anim);
	}
	void		AddAnimation(LuxAnimation *anim) {
		ISO_ASSERT(animations);
		animations->Insert(anim);
	}
	void		SortAnimation() { if (animations) animations->Sort(); }
	bool		HasAnimation() { return animations && !animations->empty(); }
	bool		HasAnimHier(bool checkSib = false, Lux *pLux = nullptr, LuxNode **ppResultNode = nullptr);
	LuxTime		AnimStart() { return HasAnimation() ? animations->StartTime() : 0; }
	LuxTime		AnimEnd() { return HasAnimation() ? animations->EndTime() : 0; }
	bool		IsLoopingAnim() { return HasAnimation() ? animations->IsLooping() : false; }
	int			NumAnimationLoops() { return animations ? animations->NumAnimationLoops() : 0; }
	void		OptimiseAnimation(float posCompress = 1.0f, float quatCompress = 1.0f, float scaleCompress = 1.0f);
	void		OptimiseAnimation(LuxNode *pBaseNode);

	bool		HasParams()				const { return params && !params->empty(); }
	int			NumParams()				const { return params->size32(); }
	LuxParam*	Param(int i)			const { return (*params)[i]; }
	//	RCLink<LuxParam>* ParamHead() { return params->begin(); }
	void		AddParam(LuxParam *param) { params->push_back(param); }

	bool		HasShadow()				const { return shadows && !shadows->empty(); }
	int			NumShadows()			const { return shadows->size32(); }
	//	RCLink<LuxShadow>	*ShadowHead() { return shadows->Head(); }
	void		AddShadow(LuxShadow *shad) { shadows->push_back(shad); }
	LuxShadow	*Shadow(int i)			const { return (*shadows)[i]; }

	bool		HasOccluder()			const { return occluders && !occluders->empty(); }
	int			NumOccluder()			const { return occluders->size32(); }
	//	RCLink<LuxOccluder>	*OccluderHead() { return occluders->Head(); }
	void		AddOccluder(LuxOccluder *occ) { occluders->push_back(occ); }
	LuxOccluder	*GetOccluder(int i)		const { return (*occluders)[i]; }

	float3x4	GetMatrix()				const { return translate(position) *  quaternion::from_euler(rotation); }
	float3x4	GetWorldMatrix()		const;
	void		SetMatrix(float3x4 &mat) { position = get_trans(mat); rotation = get_euler(mat); }
	float		CalcRadius(float3 &minext, float3 &maxext);
	float		CalcRadiusHierarchy(float3 &min, float3 &max);
	bool		CollisionTest(float3 &pos);
	int			HierarchyIndex(LuxNode *root);
};


//------------------------------------------------------
//
// Lux
//
//------------------------------------------------------

typedef RCList<LuxNode>		LuxNodeList;
typedef RCList<LuxModel>	LuxModelList;

class LuxProxyList : public RCList<LuxProxy> {
};

class LuxTintList : public RCList<LuxTint> {
public:
	LuxTint*		Find(const char *name) const;
};

class LuxTextureList : public  RCList<LuxTexture> {
public:
	LuxTexture*		Find(const char *filename) {
		for (auto &i : *this) {
			if (i->diffuse && i->diffuse == filename)
				return i;
		}
		return nullptr;
	}
	LuxTexture*		Find(LuxTexture *tex) {
		for (auto &i : *this) {
			if (*i == *tex)
				return i;
		}
		return nullptr;
	};

	LuxTexture *Add(const char *filename) {
		LuxTexture	*texture = Find(filename);
		if (!texture)
			push_back(texture = new LuxTexture(filename));
		return texture;
	}

	int			Add(LuxTexture* tex) { push_back(tex); return size32() - 1; }
};



class LuxSoundList : public RCList<LuxSound> {
public:
	LuxSound*		Find(const char *filename)	const;
	LuxSound*		Find(LuxSound *sound)		const;
#ifdef TWO_SOUNDS
	int				GetIndex2(LuxSound *sound)	const;
	int				Count2()					const;
#endif
};

class LuxSplineList : public RCList<LuxSpline> {
};

class LuxStringList : public RCList<LuxString> {
};

#ifdef LUXHUD
class LuxHudAnim : public RefCount {
private:
	void			Init() {
		type = HUD_TYPE_SPRITE;
		minColour = float3(1.0f, 1.0f, 1.0f);
		maxColour = float3(1.0f, 1.0f, 1.0f);
		min = 0.0f;
		max = 0.0f;
		rate = 15.0f;
		animMode = HUD_ANIM_LOOP;
	}

public:
	string			name;
	HudType			type;
	Vector3			minColour, maxColour;
	float			min, max;
	float			rate;
	HudAnimMode		animMode;
	LuxTextureList	textures;
	LuxHudAnim(const char *_name) { Init(); name = _name; }
	LuxHudAnim(Lux *lux, IFF_chunk &chunk);
};

class LuxHudAnimList : public RCList<LuxHudAnim> {
};
#endif

class LuxAnimChar : public RefCount {
public:
	string		name;

	LuxAnimChar(const char *_name) : name(_name) {}
};

class LuxAnimCharList : public RCList<LuxAnimChar> {
public:
	LuxAnimChar		*Find(const char *name) const {
		for (auto &i : *this) {
			if (strcmp(i->name, name) == 0)
				return i;
		}
		return nullptr;
	}
};

#ifdef LUXHUD

class LuxHud : public RefCount {
	friend class LuxHudList;

private:
	string			filename;		// this is used to identify duplicates of the same hud

	void			Init() {
		slot = 0;
		alphaMode = HUD_ALPHA_NONE;
		alpha = 100.0f;
		align = HUD_ALIGN_LEFT | HUD_ALIGN_TOP;
		x = 0.0f;
		y = 0.0f;
		parent = nullptr;
		child = nullptr;
		sibling = nullptr;
	}

public:
	string			name;
	int				slot;
	HudAlphaMode	alphaMode;
	float			alpha;			// in %
	unsigned int	align;
	float			x, y;			// in pixels on the standarad PS2 format
	LuxHudAnimList	anims;
	LuxHud			*parent, *child, *sibling;
	LuxHud(const char *_name) : name(_name) { Init(); }
	LuxHud(Lux *lux, RForm &form);
	~LuxHud() { delete filename; delete name; }
	void			SetFileName(const char *fname) { filename = fname; }
	void			AddSibling(LuxHud *pSibling);
	void			AddChild(LuxHud *pChild);
};

class LuxHudList : public RCList<LuxHud> {
public:
	LuxHud			*Find(const char *filename);
	LuxHud*			Find(const LuxHud *hud);
};
#endif

#define LUXF_MERGEDANIMS	(1<<0)

class Lux {
protected:
	RCLink<LuxNode>*	_ReorderNodes(LuxNode *node, RCLink<LuxNode> *insert);

public:
	int					flags;
	filename			path;
	LuxNodeList			nodes;
	LuxModelList		models;
	LuxTextureList		textures;
	LuxTintList			tints;
	LuxSoundList		sounds;
	LuxSplineList		splines;
	LuxStringList		strings;
	//	LuxPartList			parts;
	LuxAnimListList		anims;
#ifdef LUXHUD
	LuxHudList			huds;
#endif
	LuxAnimCharList		animChars;
	LuxProxyList		proxies;

	void		Load(istream_ref stream, Lux *sharedlux = nullptr);

	Lux() : flags(0) {}
	Lux(Lux &lux);
	Lux(istream_ref stream, Lux *sharedlux = nullptr) : flags(0) {
		Load(stream, sharedlux);
	}
	Lux(const char *filename, Lux *sharedlux = nullptr) : flags(0), path(filename) {
		Load(lvalue(FileInput(filename)), sharedlux);
	}

	~Lux() {}

	Lux&		operator+=(Lux &lux2);

	LuxTexture*	FindTexture(const char *filename) { return textures.Find(filename); }
	LuxTexture*	FindTexture(LuxTexture *tex) { return textures.Find(tex); }
	LuxTexture*	AddTexture(const char *filename) { return textures.Add(filename); }

	void		Add(LuxNode *node) { nodes.push_back(node); }
	void		Add(LuxModel *model) { models.push_back(model); }
	void		Add(LuxTint *tint) { tints.push_back(tint); }
	void		Add(LuxSound *sound) { sounds.push_back(sound); }

#ifdef LUXPARTICLE
	void		Add(LuxPart *part) { parts += part; }
#endif

	void		AddAnimChar(const char *filename) {
		if (!animChars.Find(filename))
			animChars.push_back(new LuxAnimChar(filename));
	}

	void AddAnimCharList(const LuxAnimCharList &list) {
		for (auto &i : list)
			AddAnimChar(i->name);
	}

	int AnimCharIndex(const char *filename) {
		if (LuxAnimChar *pAnim = animChars.Find(filename))
			return get_index(animChars, pAnim);
		return -1;
	}

	void		Remove(LuxNode *node);
	void		Remove(LuxModel *model);
	int			FindNode(const char *name, bool rootonly = true);
	//	int			FindNode(const char *name, const char *parentname);
	LuxNode*	FindNodeHierarchy(LuxNode *node2);
	LuxNode*	FindEntity(int ent_type);
	void		LinkRootNodes(void);
	void		UncacheTextures(void)		const;
	void		CropTextures(Lux *sharedlux = nullptr);
	void        CropTextures(int matId, LuxModel* mod, LuxMaterial* mat, Lux* sharedlux);

	int			Index(LuxNode *node)		const { return node ? get_index(nodes, node) + 1 : 0; }
	int			Index(LuxModel *model)		const { return model ? get_index(models, model) + 1 : 0; }
	int			Index(LuxTexture *texture)	const { return texture ? get_index(textures, texture) + 1 : 0; }
	int			Index(LuxTint *tint)		const { return tint ? get_index(tints, tint) + 1 : 0; }
	int			Index(LuxSound *snd) { return snd ? get_index(sounds, sounds.Find(snd->filename)) + 1 : 0; }
#ifdef TWO_SOUNDS
	int			Index2(LuxSound *snd) { return snd ? sounds.GetIndex2(sounds.Find(snd->filename)) + 1 : 0; }
#else
	int			Index2(LuxSound *snd) { return snd ? get_index(sounds, sounds.Find(snd->filename)) + 1 : 0; }
#endif
#ifdef LUXHUD
	int			Index(LuxHud *hud)			const { return hud ? huds.GetIndex(hud) + 1 : 0; }
#endif
	int			Index(LuxSpline *spline)	const { return spline ? get_index(splines, spline) + 1 : 0; }

	LuxNode*	GetNode(int i)				const { return i ? nodes[i - 1] : nullptr; }
	LuxModel*	GetModel(int i)				const { return i ? models[i - 1] : nullptr; }
	LuxTexture*	GetTexture(int i)			const { return i ? textures[i - 1] : nullptr; }
	LuxTint*	GetTint(int i)				const { return i ? tints[i - 1] : nullptr; }
	LuxTint*	GetTint(const char *name)	const { return tints.Find(name); }
	LuxSound*	GetSound(int i)				const { return i ? sounds[i - 1] : nullptr; }
	LuxSpline*	GetSpline(int i)			const { return i ? splines[i - 1] : nullptr; }

	LuxNode*	Root()						const { return nodes.empty() ? nullptr : nodes.front().get(); }

	bool		HasAnimation()				const;
	bool		HasSounds()					const { return !sounds.empty(); }
	bool		HasTints()					const { return !tints.empty(); }

	int			NumNodes()					const { return nodes.size32(); }
	int			NumModels()					const { return models.size32(); }
	int			NumTextures()				const { return textures.size32(); }
	int			NumSounds()					const { return sounds.size32(); }
#ifdef TWO_SOUNDS
	int			NumSounds2()				const { return sounds.Count2(); }
#else
	int			NumSounds2()				const { return sounds.size32(); }
#endif

	Lux&		ReorderNodes() { _ReorderNodes(Root(), nullptr); return *this; }
	//	Lux&		DupNodes();
	//	Lux&		SortNodes();
	//	Lux&		Filter(char *type);
	//	Lux&		RemoveLights();
	LuxAnimationList	*NewAnimList(void *c) { return anims.NewAnimList(c); }
	LuxAnimationList	*FindAnimList(void *c) { return anims.FindAnimList(c); }
	//	void		OptimiseAnimation(Lux *pBase);
	//	void		MergeProxies();
};

//Lux::Lux(Lux &lux) : flags(lux.flags), nodes(lux.nodes), models(lux.models), textures(lux.textures), tints(lux.tints), anims(lux.anims)
//{}

void Lux::Load(istream_ref stream, Lux *sharedlux) {
	LuxTextureList	animTexList;

	IFF_chunk	iff(stream);
	if (iff.id == 'FORM' && stream.get<uint32be>() == 'LUXO') while (iff.remaining()) {
		IFF_chunk	chunk(stream);
		switch (chunk.id) {
			case 'HEAD': {
				//				int	n = chunk.get<uint16>();
				break;
			}
			case 'PRXY': {
				LuxProxy	*proxy = new LuxProxy(chunk);
				proxies.push_back(proxy);
				break;
			}
			case 'TINT': {
				LuxTint	*tint = new LuxTint(this, chunk);
				if (sharedlux) {
					if (auto tint2 = find_check(sharedlux->tints, tint)) {
						delete tint;
						tint = *tint2;
					} else
						sharedlux->tints.push_back(tint);
				}
				tints.push_back(tint);
				//				Add(new LuxTint(this, chunk));
				break;
			}
			case 'SPLN':
				splines.push_back(new LuxSpline(chunk));
				break;

			case 'SND ': {
				LuxSound	*sound = new LuxSound(this, chunk);
				if (sharedlux) {
					if (auto sound2 = find_check(sharedlux->sounds, sound)) {
						delete sound;
						sound = *sound2;
					} else
						sharedlux->sounds.push_back(sound);
				}
				sounds.push_back(sound);
				//				Add(sound);
				break;
			}

			case 'TEXT': {
				LuxTexture	*texture = new LuxTexture(this, chunk);

				if (sharedlux) {
					if (auto texture2 = find_check(sharedlux->textures, texture)) {
						delete texture;
						texture = *texture2;
						texture->flags |= LUXTEX_SHARED;
					} else {
						sharedlux->textures.push_back(texture);
					}
				}

				textures.push_back(texture);

				if (filename(texture->diffuse).ext() == "ifl") {
					animTexList.push_back(texture);
				}
				break;
			}
			case 'PART': {
				//				parts.push_back(new LuxPart(this,chunk));
				break;
			}
			case 'FORM':
				switch (stream.get<uint32be>()) {
					case 'MODL':
						models.push_back(new LuxModel(this, chunk));
						break;
					case 'NODE': {
						LuxNode	*node = new LuxNode(this, chunk);
						nodes.push_back(node);
						//						animated |= node->HasAnimation();
						break;
					}
					case 'SKIN': {
						LuxNode	*node = GetNode(IFF_chunk(chunk).get<uint16>());	//assume HEAD chunk
						node->skin = new LuxSkin(this, chunk);
						break;
					}
#ifdef LUXHUD
					case 'HUD ': {
						LuxHud	*hud = new LuxHud(this, stream);
						if (sharedlux)
							sharedlux->huds.AddTail(hud);
						huds.AddTail(hud);
						break;
					}
#endif
				}
				break;
		}
	}
#if 0
	// resolve node references
	for (LuxNode *node : nodes) {

		if (node->params) {
			for (RCLink<LuxParam> *link = node->params->Head(), *next; next = link->Next(); link = next) {
				LuxParamNode *param = (LuxParamNode*)(LuxParam*)link->Data();
				if (param->GetType() == LUXPARAM_NODE && param->value)
					param->value = GetNode((int)(LuxNode*)param->value);
			}
		}
	}

	// remove unused textures
	for (auto &i : textures) {
		if (sharedlux) {
			if (i->refs == 2) {
				link->Remove();
				sharedlux->textures.Remove(tex);
			}
		} else {
			if (tex->RefCounts() == 1)
				link->Remove();
		}
	}
#endif
	LinkRootNodes();
}

void Lux::LinkRootNodes(void) {
	LuxNodeList::iterator link, next;
	LuxNode	*bigbrother = nullptr;

	for (auto link = nodes.begin(); link != nodes.end(); ++link) {
		LuxNode	*node = *link;
		if (!node->parent) {
			if (bigbrother)
				bigbrother->sibling = node;
			else if (link != nodes.begin()) {
				nodes.begin().insert_before(link);
			}
			bigbrother = node;
			bigbrother->sibling = nullptr;
		}
	}
}

void LuxSkinVertex::Read(Lux *lux, IFF_chunk &chunk) {
	for (n = 0; chunk.remaining() > 0; n++) {
		nodes[n].node = lux->GetNode(chunk.get<uint16>());
		nodes[n].pos = chunk.get<float3p>();
		nodes[n].weight = chunk.get<float>();
	}
}

LuxSkin::LuxSkin(Lux *lux, IFF_chunk &form) {
	for (int i = 0; form.remaining() > 0; i++)
		(*this)[i].Read(lux, lvalue(IFF_chunk(form.istream())));
}

LuxSound::LuxSound(Lux *lux, IFF_chunk &chunk) : flags(0) {
	filename = lux->path.relative(read_string(chunk, chunk.remaining()));
}

LuxTexture::LuxTexture(Lux *lux, IFF_chunk &chunk) {
	char	buffer[_MAX_PATH];

	flags = chunk.get<uint16>();

	if (flags & LUXTEX_NAME) {
		read_str(chunk, buffer, sizeof(buffer));
		name = buffer;
	} else
		name = nullptr;

	read_str(chunk, buffer, sizeof(buffer));
	diffuse = lux->path.relative(buffer);

	if (chunk.remaining()) {
		read_str(chunk, buffer, sizeof(buffer));
		opacity = buffer[0] ? lux->path.relative(buffer) : "";
		if (flags & LUXTEX_ANIM) {
			while (chunk.remaining()) {
				LuxTexture	*tex = lux->GetTexture(chunk.get<uint16>());
				if (tex) {
					anim.push_back(tex);
					tex->flags |= flags & ~LUXTEX_ANIM;
				}
			}
		}
	} else
		opacity = nullptr;
}

LuxMaterial::LuxMaterial(class Lux *lux, IFF_chunk &chunk) {
	flags = chunk.get<uint32>();
	ambient = chunk.get<float3p>();
	diffuse = chunk.get<float3p>();
	specular = chunk.get<float3p>();
	spec_exp = chunk.get<float>();
	opacity = chunk.get<float>();
	texture = lux->GetTexture(chunk.get<uint16>());
	blendmat = bottommat = nullptr;
	if (chunk.remaining())
		spec_str = chunk.get<float>();
	if (chunk.remaining())
		bumpmap = lux->GetTexture(chunk.get<uint16>());
	else
		bumpmap = nullptr;
	if (chunk.remaining()) {
		uvcrop[0] = chunk.get<float2p>();
		uvcrop[1] = chunk.get<float2p>();
		tint = lux->GetTint(chunk.get<uint16>());
		if (chunk.remaining()) {
			if (flags & LUXMATF_MIPMAP) {
				mm_offset = chunk.get<float>();
				mm_scale = chunk.get<float>();
			}
		}
	} else {
		uvcrop[0] = uvcrop[1] = zero;
		tint = nullptr;
	}
	if ((flags & LUXMATF_MIPMAP) && texture && !(texture->flags & LUXTEX_MIPMAP))
		texture->flags |= LUXTEX_MIPMAP;

	switch (chunk.id) {
		case 'MATL':
			col_channel = 0;
			uv_channel = 0;
			mask = 0;
			eor = 0;
			break;
		case 'MAT1':
			uv_channel = chunk.get<uint32>();
			col_channel = chunk.get<uint32>();
			mask = chunk.get<uint32>();
			eor = (mask >> 16) & 0xFFFF;
			mask &= 0xffff;
			break;
	}
}

LuxLensflare::LuxLensflare(Lux *lux, IFF_chunk &chunk) {
	colour = chunk.get<float3p>();
	pos = chunk.get<float3p>();
	while (chunk.remaining()) {
		float offset = chunk.get<float>();
		parts.push_back(LuxLensflarePart(offset, lux->GetTexture(chunk.get<uint16>())));
	}
}

//------------------------------------------------------
//
// LuxNode
//
//------------------------------------------------------

void LuxNode::SetEntity(const char *entName) {
	entity_name = entName;
}

void LuxNode::SetParent(LuxNode *_parent) {
	if (parent != _parent) {
		if (parent) {
			for (LuxNode **sib = &parent->child; *sib; sib = &(*sib)->sibling) {
				if (*sib == this) {
					*sib = sibling;
					break;
				}
			}
		}
		sibling = nullptr;
		if (parent = _parent) {
			LuxNode **sib;
			for (sib = &parent->child; *sib; sib = &(*sib)->sibling);
			*sib = this;
		}
	}
}

void LuxNode::SetName(const char *_name) {
	name = _name;
}


//LuxNode::LuxNode(LuxNode &node) :
//	name(node.name),
//	flags(node.flags),
//	position(node.position), rotation(node.rotation),
//	parent(nullptr), child(nullptr), sibling(nullptr),
//	model(node.model),
//	light(node.light),
//	zoom(node.zoom),
//	entity_type(node.entity_type),
//	entity_name(node.entity_name),
//	skin(node.skin),
//	collisions(node.collisions), 
//	animations(node.animations),
//	params(node.params),
//	shadows(node.shadows),
//	occluders(node.occluders),
//	proxy(node.proxy) {}

LuxNode::LuxNode(LuxNode *_parent, const char *_name) :
	position(zero), rotation(zero),
	parent(nullptr), child(nullptr), sibling(nullptr),
	model(nullptr),
	entity_type(-1),
	proxy(0),
	skin(nullptr),
	collisions(new LuxCollisionList),
	animations(nullptr),
	shadows(new LuxShadowList),
	occluders(new LuxOccluderList)
{
	const char	*p, *p2 = nullptr;
	bool		flat;
	bool		sprite;

	SetParent(_parent);

	if ((_name[0] == '+' || _name[0] == '-') && (p2 = strchr(_name, '_'))) {			// node substitute
		p = p2 + 1;
	} else
		p = _name;

	if (strnicmp("dcol_", p, 5) == 0)
		p += 5;

	if (flat = (strnicmp("flat_", p, 5) == 0))
		p += 5;

	if (strnicmp("dcol_", p, 5) == 0)
		p += 5;

	if (sprite = (strnicmp("sprite_", p, 7) == 0))
		p += 7;

	if (p2) {
		int	len1 = p2 - _name + 1, len2 = int(strlen(p)) + 1;
		name = str(_name, p2 + 1) + p;
	} else
		name = p;

	if (p[0] == '#' || p[0] == '!')			// geometry substitute - this node won't be used
		flags = LUXNODE_SUBSTITUTE;
	else if (strnicmp("lowres_", p, 7) == 0)
		flags = LUXNODE_LOWRES;
	else if (strnicmp("damage_", p, 7) == 0)
		flags = LUXNODE_DAMAGE;
	else if (strnicmp("attach_", p, 7) == 0)
		flags = LUXNODE_ATTACH;
	else if (strnicmp("debris_", p, 7) == 0)
		flags = LUXNODE_DEBRIS;
	else if (strnicmp("topple_", p, 7) == 0)
		flags = LUXNODE_TOPPLE;
	else if (strnicmp("icon_", p, 5) == 0)
		flags = LUXNODE_ICON;
	else if (strnicmp("sfx_", p, 4) == 0)
		flags = LUXNODE_SFX;
	else if (strnicmp("stencil_", p, 8) == 0)
		flags = LUXNODE_SHADOW;
	else if (strnicmp("shadow_", p, 7) == 0)
		flags = LUXNODE_SHADOW;
	else if (strnicmp("cutout_", p, 7) == 0)
		flags = LUXNODE_CUTOUT;
	else if (strnicmp("seam_", p, 5) == 0)
		flags = LUXNODE_SEAM;
	else
		flags = LUXNODE_NORMAL;

	if (flat) flags |= LUXNODE_FLAT;
	if (sprite) {
		flags |= LUXNODE_SPRITE;
	}

}

void ReadCollisions(LuxCollision *col, IFF_chunk &form) {
	LuxCollision	*prev = nullptr;
	while (form.remaining()) {
		IFF_chunk	chunk(form.istream());
		switch (chunk.id) {
			case 'COLS':
				prev = (prev ? prev->sibling : col->child) = new LuxCollisionSphere(chunk);
				break;

			case 'COLB':
				prev = (prev ? prev->sibling : col->child) = new LuxCollisionBox(chunk);
				break;

			case 'COLC':
				prev = (prev ? prev->sibling : col->child) = new LuxCollisionCapsule(chunk);
				break;

			case 'COLM':
				prev = (prev ? prev->sibling : col->child) = new LuxCollisionMesh(chunk);
				break;

			case 'FORM':
				switch (chunk.get<uint32be>()) {
					case 'COLL': {
						if (prev)
							ReadCollisions(prev, chunk);
						break;
					}
					case 'SOUP':
						prev = (prev ? prev->sibling : col->child) = new LuxCollisionSoup(chunk);
						break;
				}
				break;
		}
	}
}


LuxNode::LuxNode(Lux *lux, IFF_chunk &form)
	: parent(nullptr), child(nullptr), sibling(nullptr), model(nullptr)
	, entity_type(-1), proxy(0)
	, skin(nullptr)
	, collisions(new LuxCollisionList)
	, animations(nullptr)
	, shadows(new LuxShadowList), occluders(new LuxOccluderList)
{
	LuxCollision	*col = nullptr;
	while (form.remaining()) {
		IFF_chunk	chunk(form.istream());
		switch (chunk.id) {
			case 'NAME':
				name.read(chunk, chunk.remaining());
				break;

			case 'DATA':
				flags = chunk.get<uint16>();
				SetParent(lux->GetNode(chunk.get<uint16>()));
				model = lux->GetModel(chunk.get<uint16>());
				position = chunk.get<float3p>();
				rotation = chunk.get<float3p>();
				if (chunk.remaining())
					entity_type = chunk.get<uint16>();
				if (auto n = chunk.remaining())
					entity_name.read(chunk, n);
				break;

			case 'PRXY':
				proxy = chunk.get<uint32>();
				break;

			case 'LITE':
				light = chunk;
				break;

			case 'ZOOM':
				zoom = chunk.get<float>();
				break;

			case 'COLS':
				AddCollision(col = new LuxCollisionSphere(chunk));
				break;

			case 'COLB':
				AddCollision(col = new LuxCollisionBox(chunk));
				break;

			case 'COLM':
				AddCollision(col = new LuxCollisionMesh(chunk));
				break;

			case 'FORM':
				switch (chunk.get<uint32be>()) {
					case 'ANIM': {
						bool	vertanim = false;
						NewAnimList(lux, nullptr);
						while (chunk.remaining()) {
							IFF_chunk	chunk(form.istream());
							switch (chunk.id) {
								case 'ROT ':	AddAnimation0(new LuxAnimRotation(chunk));		break;
								case 'QUAT':	AddAnimation0(new LuxAnimQuaternion(chunk));	break;
								case 'POS ':	AddAnimation0(new LuxAnimPosition(chunk));		break;
								case 'SCLE':	AddAnimation0(new LuxAnimScale(chunk));			break;
#ifdef DECOUPLE_NORMALS
								case 'VECT':
#else
								case 'VERT':
#endif
									AddAnimation0(new LuxAnimVertex(chunk));
									vertanim = true;
									break;

								case 'TEXT':	AddAnimation0(new LuxAnimTexture(lux, chunk));	break;
								case 'ENDM': {
									LuxAnimEndMarker	*anim = new LuxAnimEndMarker(chunk);
									if (!animations->empty() && anim->time == animations->front()->time)
										animations->push_back(anim);
									else
										AddAnimation0(anim);
									break;
								}
								case 'ZOOM':	AddAnimation0(new LuxAnimZoom(chunk));			break;
								case 'NOTE':	AddAnimation0(new LuxAnimNote(chunk));			break;
								case 'SND ':	AddAnimation0(new LuxAnimSound(lux, chunk));	break;
								case 'BLUR':	AddAnimation0(new LuxAnimBlur(chunk));			break;
								case 'COLR':	AddAnimation0(new LuxAnimColour(chunk));		break;
								case 'OPAC':	AddAnimation0(new LuxAnimOpacity(chunk));		break;
								case 'UV  ':	AddAnimation0(new LuxAnimUV(chunk));			break;
							}
						}
						if (vertanim && model)
							model->flags |= LUXMOD_VERTEXANIM;
						break;
					}

					case 'SKIN':
						skin = new LuxSkin(lux, chunk);
						break;

					case 'SOUP':
						AddCollision(new LuxCollisionSoup(chunk));
						break;

					case 'PARM': {
						char		name[256];
						LuxParam	*param = nullptr;
						name[0] = 0;
						if (!HasParams())
							params = new LuxParamList;
						while (chunk.remaining()) {
							IFF_chunk	chunk(form.istream());
							switch (chunk.id) {
								case 'NAME':	name[chunk.readbuff(name, chunk.remaining())] = 0;	break;
								case 'INT ':	param = new LuxParamInt(chunk);				break;
								case 'FLOT':	param = new LuxParamFloat(chunk);			break;
								case 'NODE':	param = new LuxParamNode(lux, chunk);		break;
								case 'RECT':	param = new LuxParamExtent(chunk);			break;
								case 'SPLN':	param = new LuxParamSpline(lux, chunk);		break;
								case 'SND ':	param = new LuxParamSound(chunk);			break;
#ifdef LUXPARTICLE
								case 'PART':	param = new LuxParamPart(lux, chunk);		break;
#endif
								case 'ENT ':	param = new LuxParamEnt(chunk);				break;
								case 'LUX ':	param = new LuxParamLux(chunk);				break;
								case 'ENUM':	param = new LuxParamEnum(chunk);			break;
								case 'BOOL':	param = new LuxParamBool(chunk);			break;
								case 'COLR':	param = new LuxParamColor(chunk);			break;
								case 'RSPN':	param = new LuxParamExtentSpawn(chunk);		break;
#ifdef LUXHUD
								case 'HUD ':	param = new LuxParamHud(lux, chunk);		break;
#endif
								case 'TEX ':	param = new LuxParamTex(lux, chunk);		break;
								case 'ANIM':	param = new LuxParamAnim(lux, chunk);		break;
								case 'STRG': {
									param = new LuxParamString(chunk);
									if (LuxString *p = ((LuxParamString*)param)->value)
										lux->strings.push_back(p);
									break;
								}
							}
							if (param) {
								if (name[0])
									param->SetName(name);
								AddParam(param);
								param = nullptr;
								name[0] = 0;
							}
						}
						break;
					}

					case 'SHAD':
						//AddShadow(new LuxShadow(chunk));
						break;

					case 'OCLD':
						//AddOccluder(new LuxOccluder(chunk));
						break;

					case 'COLL': {
						ReadCollisions(col, chunk);
						break;
					}
				}
				break;
		}
	}

}

float3x4 LuxNode::GetWorldMatrix() const {
	float3x4	mat = GetMatrix();
	for (LuxNode *node = parent; node; node = node->parent)
		mat = node->GetMatrix() * mat;
	return mat;
}

float LuxNode::CalcRadius(float3 &minext, float3 &maxext) {
	float	radius = 0;
	if (model) {
		radius = model->CalcRadius(minext, maxext);
	} else {
		radius = 0;
		minext = maxext = zero;
	}
#ifdef COLLISION_HIERARCHY
	for (LuxCollision *col = CollisionRoot(); col; col = col->sibling) {
		if (col->type == LUXCOLL_BOX) {
			LuxCollisionBox	*coll = (LuxCollisionBox*)col;
			minext = min(minext, coll->minext);
			maxext = max(maxext, coll->maxext);
		}
	}
#else
	for (RCLink<LuxCollision> *link = CollisionHead(); !link->IsEOList(); link = link->Next()) {
		if (link->Data()->type == LUXCOLL_BOX) {
			LuxCollisionBox	*coll = (LuxCollisionBox*)link->Data();
			minext = Min(minext, coll->minext);
			maxext = Max(maxext, coll->maxext);
		}
	}
#endif
	return radius;
}

float LuxNode::CalcRadiusHierarchy(float3 &min, float3 &max) {
	float radius = CalcRadius(min, max);
	for (LuxNode *ch = child; ch; ch = ch->sibling) {
		if (ch->Type() == LUXNODE_NORMAL) {
			float3	chmin, chmax;
			float	chradius = ch->CalcRadiusHierarchy(chmin, chmax);
			cuboid	box = (translate(ch->position) * quaternion::from_euler(ch->rotation) * cuboid(position3(chmin), position3(chmax))).get_box();
			min = iso::min(min, chmin);
			max = iso::max(max, chmax);
			radius = ::max(radius, len(ch->position) + chradius);
		}
	}
	return radius;
}

// check the node for animation, also it may be a proxy so look in the lux to find the proxy nodes
bool LuxNode::HasAnimHier(bool checkSib, Lux *pLux, LuxNode **ppResultNode) {
	if (HasAnimation()) {
		if (ppResultNode)
			*ppResultNode = this;
		return true;
	} else {
		if (Type() == LUXNODE_REFERENCE && pLux) {	// check through the references
			int	id = proxy < 0 ? proxy : entity_type;
			for (int n = 0; n < pLux->nodes.size(); n++) {
				LuxNode *pn = pLux->nodes[n];
				if (pn->Type() == LUXNODE_REFERENCED && pn->entity_type == id) {
					if (pLux->nodes[n]->HasAnimHier(true, pLux, ppResultNode))
						return true;
				}
			}
		}
		if (child) {
			if (child->HasAnimHier(true, pLux, ppResultNode))
				return true;
		}
		if (checkSib && sibling && (Type() != LUXNODE_REFERENCED))
			return sibling->HasAnimHier(checkSib, pLux, ppResultNode);
	}
	return false;
}

bool LuxNode::CollisionTest(float3 &pos) {
#ifdef COLLISION_HIERARCHY
	for (LuxCollision *col = CollisionRoot(); col; col = col->sibling)
		if (col->Test(pos)) return true;
#else
	for (RCLink<LuxCollision> *link = CollisionHead(); !link->IsEOList(); link = link->Next())
		if (link->Data()->Test(pos)) return true;
#endif
	return false;
}

int LuxNode::Extra() {
	int	extra = 0xaaaa;
	sscanf(name, "%*[^,],%i", &extra);
	return extra;
}

void LuxNode::NewAnimList(Lux *lux, void *controller) {
	if (!animations)
		animations = lux->NewAnimList(controller);
}

//------------------------------------------------------
//
// LuxParam
//
//------------------------------------------------------

#ifdef LUXPARTICLE

LuxParamPart::LuxParamPart(Lux *lux, IFF_chunk &chunk) {
	int index;
	index = chunk.get<uint32>();
	value = lux->parts[index];
}

LuxParamPart::LuxParamPart(Lux *lux, const char *_filename) {
	if (_filename) {
		char	filename[_MAX_PATH];
		int		frames = 1;

		strcpy(filename, _filename);
		if (char *p = strchr(filename, ',')) {
			*p++ = 0;
			frames = strtol(p, nullptr, 0);
		}

		if (!(value = lux->parts.Find(filename)))
			lux->Add(value = new LuxPart(lux, filename, frames));
	}
}
#endif

LuxParamString::LuxParamString(IFF_chunk &chunk) {
	if (chunk.remaining())
		value = new LuxString(chunk);
}

#ifdef LUXHUD

LuxParamHud::LuxParamHud(Lux *lux, char *_filename) {
	if (_filename)
		strcpy(filename, _filename);
	else
		strcpy(filename, "<None>");
	Load(lux);
}

LuxParamHud::LuxParamHud(Lux *lux, IFF_chunk &chunk) {
	read_str(chunk, filename, _MAX_PATH);
	Load(lux);
}

void LuxParamHud::Load(Lux *lux) {
	LuxHud	*pHud = lux->huds.Find(filename);
	if (pHud)
		value = pHud;
	else {
		Lux	tempLux(filename, lux);
		if (!tempLux.huds.IsEmpty()) {
			value = tempLux.huds[0];
			value->SetFileName(filename);
		}
	}
}

#endif

LuxParamTex::LuxParamTex(Lux *lux, const char *_filename) {
	if (_filename) {
		strcpy(filename, _filename);
		value = lux->AddTexture(filename);
	} else {
		strcpy(filename, "<None>");
		value = nullptr;
	}
}

LuxParamTex::LuxParamTex(Lux *lux, IFF_chunk &chunk) {
	read_str(chunk, filename, _MAX_PATH);
	value = ::stricmp(filename, "<None>") != 0 ? lux->AddTexture(filename) : nullptr;
}

LuxParamAnim::LuxParamAnim(Lux *lux, char *_filename) {
	if (_filename)
		strcpy(filename, _filename);
	else
		strcpy(filename, "<None>");
	lux->AddAnimChar(filename);
}

LuxParamAnim::LuxParamAnim(Lux *lux, IFF_chunk &chunk) {
	read_str(chunk, filename, _MAX_PATH);
	lux->AddAnimChar(filename);
}

LuxParamNode::LuxParamNode(Lux *lux, IFF_chunk &chunk) {
	value = chunk.remaining() ? lux->GetNode(chunk.get<uint32>()) : nullptr;
}

LuxParamSpline::LuxParamSpline(Lux *lux, IFF_chunk &chunk) {
	value = lux->GetSpline(chunk.get<uint32>());
}


//------------------------------------------------------
//
// LuxAnimation
//
//------------------------------------------------------

#ifdef DECOUPLE_NORMALS

LuxAnimVertex::LuxAnimVertex(IFF_chunk &chunk) : LuxAnimation(chunk.get<uint32>(), LUXANIM_VERTEX) {
	while (chunk.remaining())
		vertices.push_back(chunk.get<float3p>());
}

#else

LuxAnimVertex::LuxAnimVertex(IFF_chunk &chunk) : LuxAnimation(chunk, LUXANIM_VERTEX) {
	while (chunk.remaining())
		vertices.Append(1, &LuxVertex(chunk));
}

void LuxAnimVertex::Write(Lux *lux, Form &form) {
	Chunk	chunk(form, "VERT");
	Write0(chunk);
	for (int i = 0; i < vertices.Count(); i++)
		vertices[i].Write(chunk);
}

#endif

LuxAnimTexture::LuxAnimTexture(Lux *lux, IFF_chunk &chunk) : LuxAnimation(chunk, LUXANIM_TEXTURE) {
	animtex = lux->GetTexture(chunk.get<uint16>());
	tex = lux->GetTexture(chunk.get<uint16>());
	animtex->flags |= LUXTEX_USED_ANIM;
	tex->flags |= LUXTEX_USED_ANIM;
}

LuxAnimSound::LuxAnimSound(Lux *lux, IFF_chunk &chunk) : LuxAnimation(chunk, LUXANIM_SOUND) {
	int	i = chunk.get<uint16>(), t = chunk.tell();
	sound = lux->GetSound(i);
	SetDefaults();
	if (chunk.remaining() >= 28) {
		minVol = chunk.get<float>();
		minVolAt = chunk.get<float>();
		maxVol = chunk.get<float>();
		maxVolAt = chunk.get<float>();
		pitch = chunk.get<float>();
		randVol = chunk.get<float>();
		randPitch = chunk.get<float>();
	}
}

void LuxAnimationList::Insert(LuxAnimation *anim) {
	if (!insert) {
		insert = begin();
		if (insert == end()) {
			push_back(anim);
			return;
		}
	}
	LuxTime		time = anim->time;
	iterator	same;

	if (time < (*insert)->time)
		insert = begin();

	for (same = insert; same != end(); ++same) {
		insert = same;
		if (time <= (*same)->time)
			break;
	}

	while (same != end() && time == (*same)->time) {
		if (anim->type != LUXANIM_TEXTURE && anim->type != LUXANIM_NOTE && anim->type == (*same)->type) {
			if (insert == same)
				++insert;
			same.remove();
		}
		++same;
	}

	insert_before(same, anim);
}

void LuxAnimationList::Sort() {
	return;

}

int LuxAnimationList::NumAnimationLoops() {
	int	n = 0;
	if (!empty()) {
		int	starttime = front()->time;
		for (auto &i : *this) {
			if (i->type == LUXANIM_ENDMARKER && i->time != starttime)
				n++;
		}
	}
	return n;
}

bool LuxAnimationList::IsLooping() {
	for (auto &i : reversed(*this)) {
		LuxAnimation	*anim = i;
		if (anim->type == LUXANIM_ENDMARKER)
			return ((LuxAnimEndMarker*)anim)->flags == 0;
	}
	return false;
}

LuxAnimationList *LuxAnimListList::FindAnimList(void *c) {
	if (c) {	// if controller == nullptr the animation list cannot be re-used
		LuxAnimationList			*anim;
		for (auto &i : *this) {
			anim = i;
			if (anim->Controller() == c)		// found the animation list with that controller
				return anim;
		}
	}

	return nullptr;
}

LuxAnimationList *LuxAnimListList::NewAnimList(void *c) {
	LuxAnimationList	*anim = FindAnimList(c);	// see if this controller is already in the list

	if (anim == nullptr) {		// if it is truly new, allocate it
		anim = new LuxAnimationList(c);
		push_back(anim);
	}

	return anim;
}

int LuxAnimListList::LoopIndex(LuxAnimationList *animList) {
	if (animList) {
		int	count = 0;
		for (auto &i : *this) {
			LuxAnimationList	*anim = i;
			if (anim == animList)		// found the animation list with that controller
				return count;
			count += anim->NumAnimationLoops();
		}
	}

	return -1;
}

int LuxAnimListList::CountLoops() {
	int		count = 0;
	for (auto &i : *this)
		count += i->NumAnimationLoops();

	return count;
}

//-----------------------------------------------------------------------------

char*	Legalise(char *name);

//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
//	LUXFileHandler
//-----------------------------------------------------------------------------

class LUXFileHandler : public FileHandler {
	const char*		GetExt() override { return "lux"; }
	const char*		GetDescription() override { return "Luxoflux model file"; }
	ISO_ptr<void>	ReadWithFilename(tag id, const filename &fn) override;
} lux;

float3 lux_fix(param(float3) v) {
	return float3{v.x, v.z, -v.y};
}

ISO_ptr<Model3> GetModel(Lux &lux, LuxModel *luxmodel) {
	ModelBuilder	model(none);

	int mi = 0;
	for (auto m : luxmodel->materials) {
		int nf = 0;
		for (auto &f : luxmodel->faces) {
			if ((f.flags & LUXFACE_MAT) == mi)
				++nf;
		}

		if (nf) {
			ISO::TypeCompositeN<64>	builder(0);
			bool	has_tex = !!(m->flags & LUXMATF_TEXTURED);
			bool	has_col = (luxmodel->flags & LUXMOD_VERTEXCOLS) && !(m->flags & LUXMATF_NOVC);

			uint32	pos_offset = builder.Add<float[3]>("position");
			uint32	norm_offset = builder.Add<float[3]>("normal");
			uint32	uv_offset = has_tex ? builder.Add<float[2]>("texcoord") : 0;
			uint32  col_offset = has_col ? builder.Add<float[3]>("colour") : 0;

			anything	params;

			params.Append(ISO_ptr<float3p>("light_ambient", (float3p&)m->ambient));
			//			params.Append(ISO_ptr<float3p>("light_diffuse", m->diffuse));
			if (has_tex) {
				ISO_ptr<void>	p = MakePtr(ISO::getdef<Texture>(), "DiffuseTexture");
				*(ISO_ptr<void>*)p = m->texture->GetBitmap();
				params.Append(p);
			}

			static const char *shaders[] = {
				"lite",
				"col_lit",
				"tex_lit",
				"tex_col_lit",
			};
			model.SetMaterial(ISO::root("data")["simple"][shaders[int(has_col) + int(has_tex) * 2]], AnythingToStruct(params, 0));

			Indexer<uint32>			indexer(nf * 3);
			dynamic_array<float3p>	pos(nf * 3);
			dynamic_array<float3p>	norm(nf * 3);
			dynamic_array<float2p>	uv(nf * 3);
			dynamic_array<float3p>	col(nf * 3);
			int		x = 0;

			for (auto &f : luxmodel->faces) {
				if ((f.flags & LUXFACE_MAT) == mi) {
					for (int i = 0; i < 3; i++, x++) {
						pos[x] = lux_fix(luxmodel->vertices[f.v[i]]);
						norm[x] = lux_fix(f.normal[i]);
					}
				}
			}
			indexer.Process(pos.begin(), equal_vec());
			indexer.Process(norm.begin(), equal_vec());

			if (has_tex) {
				LuxCropTexture	*crop = m->texture->flags & LUXTEX_CROP_TEXTURE ? (LuxCropTexture*)m->texture.get() : 0;
				x = 0;
				for (auto &f : luxmodel->faces) {
					if ((f.flags & LUXFACE_MAT) == mi) {
						for (int i = 0; i < 3; i++, x++) {
							float2	t = f.uv[i][m->uv_channel];
							if (crop)
								t = crop->UncropUV(t);
							uv[x] = t;
						}
					}
				}
				indexer.Process(uv.begin(), equal_vec());
			}

			if (has_col) {
				x = 0;
				for (auto &f : luxmodel->faces) {
					if ((f.flags & LUXFACE_MAT) == mi) {
						for (int i = 0; i < 3; i++, x++)
							col[x] = f.col[i][m->col_channel];
					}
				}
				indexer.Process(col.begin(), equal_vec());
			}


			SubMesh	*sm		= model.AddMesh(builder.Duplicate(), indexer.NumUnique(), nf);
			auto	indices	= &sm->indices[0][0];
			char	*verts	= sm->VertData();

//			copy(indexer.Indices(), indices);

			auto	*p = &indexer.Index(0);
			for (auto &i : sm->indices) {
				i[2] = *p++;
				i[1] = *p++;
				i[0] = *p++;
			}

			for (auto &i : indexer.RevIndices()) {
				*(float3p*)(verts + pos_offset) = pos[i];
				*(float3p*)(verts + norm_offset) = norm[i];
				if (has_tex)
					*(float2p*)(verts + uv_offset) = uv[i];
				if (has_col)
					*(float3p*)(verts + col_offset) = col[i];
				verts += model.vert_size;
			}
		}

		++mi;
	}
	model->UpdateExtents();
	return move(model);
}

ISO_ptr<void> GetNode(Lux &lux, LuxNode *luxnode) {
	switch (luxnode->Type()) {
		case LUXNODE_NORMAL: {
			ISO_ptr<Node>	node(luxnode->name);
			node->matrix			= luxnode->GetMatrix();
			anything	*children	= &node->children;

			for (LuxNode *child = luxnode->child; child; child = child->sibling) {
				if (child->Type() == LUXNODE_LOWRES) {
					ISO_ptr<ent::Splitter>	splitter(child->name);
					if (child->model)
						splitter->lorez	= GetModel(lux, child->model);
					splitter->split_decision	= ent::Splitter::Distance;
					from_string(child->name + 7, splitter->value);

					ISO_ptr<anything>	hirez(0);
					splitter->hirez	= hirez;
					children		= hirez;
					node->children.Append(splitter);
				}
			}

			if (luxnode->model)
				children->Append(GetModel(lux, luxnode->model));

			for (LuxNode *child = luxnode->child; child; child = child->sibling) {
				if (ISO_ptr<void> p = GetNode(lux, child))
					children->Append(p);
			}
			return node;
		}
		case LUXNODE_ATTACH: {
			ISO_ptr<ent::Attachment> node(luxnode->name);
			from_string(luxnode->name.find('_'), node->id);
			node->matrix = luxnode->GetMatrix();
			return node;
		}
		case LUXNODE_CAMERA: {
			ISO_ptr<ent::Camera> node(luxnode->name);
			return node;
		}
		case LUXNODE_LIGHT: {
			ISO_ptr<ent::Light2> node(luxnode->name);
			return node;
		}
		case LUXNODE_LOWRES:	// handled above
		//TBD
		case LUXNODE_DAMAGE:
		case LUXNODE_DEBRIS:
		case LUXNODE_TOPPLE:
		case LUXNODE_ICON:
		case LUXNODE_SFX:
		case LUXNODE_SUBSTITUTE:
		case LUXNODE_REFERENCE:
		case LUXNODE_REFERENCED:
		case LUXNODE_OCCLUDER:
		case LUXNODE_SHADOW:
		case LUXNODE_CUTOUT:
		case LUXNODE_SEAM:
		default:
			return ISO_NULL;
	}

}

ISO_ptr<void> LUXFileHandler::ReadWithFilename(tag id, const filename &fn) {
	Lux	lux(fn.begin());
	ISO_ptr<Scene>	p(id);

#if 1
	p->root = GetNode(lux, lux.nodes.front());
#else
	p->root.Create(0);
	for (auto i : lux.nodes)
		p->root->children.Append(GetNode(lux, i));
#endif
	return p;
}
