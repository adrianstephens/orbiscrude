#include "flash.h"
#include "comms/zlib_stream.h"
#include "filetypes/bitmap/jpg.h"
#include "filetypes/sound/sample.h"
#include "packed_types.h"
#include "vector_iso.h"
#include "iso/iso_files.h"
#include "base/bits.h"
#include "codec/vlc.h"
#include "extra/xml.h"

void			RemoveDupes(iso::ISO_ptr<void> p, int flags);

namespace iso { namespace flash {

struct ABC;
ABC*			DecodeABC(void *p);
ISO_ptr<void>	ISO_ABC(ABC *abc);

//-----------------------------------------------------------------------------
//	basic types
//-----------------------------------------------------------------------------
typedef int8				SI8;
typedef int16le				SI16;
typedef int32le				SI32;
typedef uint8				UI8;
typedef uint16le			UI16;
typedef uintn<3>			UI24;
typedef uint32le			UI32;
typedef uint64le			UI64;
typedef fixed<16,16>		FIXED;
typedef fixed<8,8>			FIXED8;
typedef soft_float<10,5,1>	FLOAT16;
typedef floatle				FLOAT, F32;
typedef doublele			DOUBLE;

enum SWF_TAG {
	tag_End					= 0,	// yes
	tag_ShowFrame			= 1,	// yes
	tag_DefineShape			= 2,	// yes
	tag_PlaceObject			= 4,	// yes
	tag_RemoveObject		= 5,	// yes
	tag_DefineBits			= 6,	// yes
	tag_DefineButton		= 7,
	tag_JPEGTables			= 8,	// yes
	tag_SetBackgroundColor	= 9,	// yes
	tag_DefineFont			= 10,	// yes
	tag_DefineText			= 11,	// read
	tag_DoAction			= 12,
	tag_DefineFontInfo		= 13,	// read
	tag_DefineSound			= 14,
	tag_StartSound			= 15,
	tag_DefineButtonSound	= 17,
	tag_SoundStreamHead		= 18,
	tag_SoundStreamBlock	= 19,
	tag_DefineBitsLossless	= 20,	// yes
	tag_DefineBitsJPEG2		= 21,	// yes
	tag_DefineShape2		= 22,	// yes
	tag_DefineButtonCxform	= 23,
	tag_Protect				= 24,
	tag_PlaceObject2		= 26,	// yes
	tag_RemoveObject2		= 28,	// yes
	tag_DefineShape3		= 32,	// yes
	tag_DefineText2			= 33,	// read
	tag_DefineButton2		= 34,
	tag_DefineBitsJPEG3		= 35,	// yes
	tag_DefineBitsLossless2	= 36,	// yes
	tag_DefineEditText		= 37,
	tag_DefineSprite		= 39,
	tag_FrameLabel			= 43,
	tag_SoundStreamHead2	= 45,
	tag_DefineMorphShape	= 46,
	tag_DefineFont2			= 48,	// yes
	tag_ExportAssets		= 56,	// yes
	tag_ImportAssets		= 57,
	tag_EnableDebugger		= 58,
	tag_DoInitAction		= 59,
	tag_DefineVideoStream	= 60,
	tag_VideoFrame			= 61,
	tag_DefineFontInfo2		= 62,	// read
	tag_EnableDebugger2		= 64,
	tag_ScriptLimits		= 65,
	tag_SetTabIndex			= 66,
	tag_FileAttributes		= 69,	// yes
	tag_PlaceObject3		= 70,	// yes
	tag_ImportAssets2		= 71,
	tag_DefineFontAlignZones= 73,
	tag_CSMTextSettings		= 74,
	tag_DefineFont3			= 75,	// yes
	tag_SymbolClass			= 76,
	tag_Metadata			= 77,
	tag_DefineScalingGrid	= 78,
	tag_DoABC				= 82,
	tag_DefineShape4		= 83,	// yes
	tag_DefineMorphShape2	= 84,
	tag_DefineSceneAndFrameLabelData = 86,
	tag_DefineBinaryData	= 87,
	tag_DefineFontName		= 88,
	tag_StartSound2			= 89,
	tag_DefineBitsJPEG4		= 90,	// yes
	tag_DefineFont4			= 91,	// yes
};


enum SWF_samples {
	smp_Uncompressed	= 0,
	smp_ADPCM			= 1,
	smp_MP3				= 2,
	smp_Uncompressed_le	= 3,
	smp_Nellymoser16kHz	= 4,
	smp_Nellymoser8kHz	= 5,
	smp_Nellymoser6kHz	= 6,
	smp_Speex			= 11,
};

//-----------------------------------------------------------------------------
//	vlc
//-----------------------------------------------------------------------------
class vlc : public vlc_in<uint32, true> {
public:
	vlc(istream_ref file) : vlc_in<uint32, true>(file)	{}

	uint32		operator()(int nbits) {
		return this->get(nbits);
	}

	bool		get_bit() {
		return !!operator()(1);
	}

	int			get_signed(int nbits) {
		if (nbits == 0)
			return 0;
		return sign_extend(this->get(nbits), nbits);
	}

	FIXED		get_fixed(int nbits) {
		FIXED	f;
		(uint32&)f = get_signed(nbits);
		return f;
	}

	FIXED8		get_fixed8(int nbits) {
		FIXED8	f;
		(uint16&)f = get_signed(nbits);
		return f;
	}
};

//-----------------------------------------------------------------------------
//	ReadArray
//-----------------------------------------------------------------------------

template<typename T> T* ReadArray(istream_ref file, size_t n) {
	T	*p	= new T[n];
	file.readbuff(p, n * sizeof(T));
	return p;
}

//-----------------------------------------------------------------------------
//	structures
//-----------------------------------------------------------------------------

struct SWF_header {
	UI8		Signature[3];	// Signature byte: F indicates uncompressed, C indicates compressed (SWF 6 and later only)
	UI8		Version;		// Single byte file version (for example, 0x06 for SWF 6)
	UI32	FileLength;		// Length of entire file in bytes
//	RECT	FrameSize;		// Frame size in twips
//	UI16	FrameRate;		// Frame delay in 8.8 fixed number of frames per second
//	UI16	FrameCount;		// Total number of frames in file
};

struct STRING : string {
	STRING() {}
	STRING(istream_ref file) {
		dynamic_array<char>	a;
		int		c;
		while ((c = file.getc()) && c != EOF)
			a.push_back(c);
		a.push_back(0);
		init(a.begin(), a.end());
	}
};

struct STRING2 : fixed_string<256> {
	STRING2() {}
	STRING2(istream_ref file) {
		UI8		len = file.get<UI8>();
		file.readbuff(begin(), len);
		begin()[len] = 0;
	}
};

struct EncodedU32 {
	uint32	v;
	EncodedU32(istream_ref file) {
		v = file.getc();
		if (v & 0x80) {
			v = (v & 0x7f) | (file.getc() << 7);
			if (v & 0x4000) {
				v = (v & 0x3fff) | (file.getc() << 14);
				if (v & 0x200000) {
					v = (v & 0x1fffff) | (file.getc() << 21);
					if (v & 0x10000000)
						v = (v & 0x0fffffff) | (file.getc() << 28);
				}
			}
		}
	}
	operator uint32() const { return v; }
};

struct TAG	{uint16 length:6, type:10; };
struct RGB	{uint8 r, g, b;		operator ISO_rgba() const { return ISO_rgba(r,g,b); } };
struct RGBA	{uint8 r, g, b, a;	operator ISO_rgba() const { return ISO_rgba(r,g,b,a); } };
struct ARGB	{uint8 a, r, g, b;	operator ISO_rgba() const { return ISO_rgba(r,g,b,a); } };

typedef Texel<TexelFormat<16, 10,5, 5,5, 0,5>	> PIX15;
typedef Texel<TexelFormat<32, 8,8, 16,8, 24,8>	> PIX24;

struct RECT	{
	int x0, x1, y0, y1;
	RECT()	{}
	RECT(istream_ref file) {
		vlc	v = file;
		int n = v(5);
		x0 = v.get_signed(n);
		x1 = v.get_signed(n);
		y0 = v.get_signed(n);
		y1 = v.get_signed(n);
	}

	operator float4p() const {
		return {x0 / 20.f, y0 / 20.f, x1 / 20.f, y1 / 20.f};
	}
};

struct MATRIX {
	FIXED	scalex, scaley, rotskew0, rotskew1;
	SI32	transx, transy;
	MATRIX() : scalex(1), scaley(1), rotskew0(0), rotskew1(0), transx(0), transy(0)	{}
	MATRIX(istream_ref file) {
		vlc		v = file;
		if (v.get_bit()) {
			int	n	= v(5);
			scalex	= v.get_fixed(n);
			scaley	= v.get_fixed(n);
		} else {
			scalex	= scaley = 1;
		}
		if (v.get_bit()) {
			int	n	= v(5);
			rotskew0 = v.get_fixed(n);
			rotskew1 = v.get_fixed(n);
		} else {
			rotskew0 = rotskew1 = 0;
		}
		int	n	= v(5);
		transx	= v.get_signed(n);
		transy	= v.get_signed(n);
	}
	operator float2x3p() const {
		float2x3p	p;
		p.x = to<float>(scalex, rotskew0);
		p.y = to<float>(rotskew1, scaley);
		p.z = to<float>(transx / 20.f, transy / 20.f);
		return p;
	}
};

struct CXFORM {
	array_vec<FIXED8, 4>	mul;
	array_vec<FIXED8, 4>	add;
	CXFORM()	{}
	CXFORM(istream_ref file, bool alpha) {
		vlc		v = file;
		bool	hasadd	= v.get_bit();
		bool	hasmul	= v.get_bit();
		int		n		= v(4);
		if (hasmul) {
			mul.x = v.get_fixed8(n);
			mul.y = v.get_fixed8(n);
			mul.z = v.get_fixed8(n);
			mul.w = alpha ? v.get_fixed8(n) : FIXED8(1);
		} else {
			mul = one;
		}
		if (hasadd) {
			add.x = v.get_fixed8(n);
			add.y = v.get_fixed8(n);
			add.z = v.get_fixed8(n);
			add.w = alpha ? v.get_fixed8(n) : FIXED8(0);
		} else {
			add = zero;
		}
	}
	operator simple_vec<float4p,2>() const {
		simple_vec<float4p,2>	r;
		r.x = mul;
		r.y = add;
		return r;
	}
};

#pragma pack(1)

struct COLORMATRIXFILTER {
	FLOAT	m[20];
	COLORMATRIXFILTER(istream_ref file)	{ file.read(*this); }
	operator ISO_ptr<void>()	const	{
		return ISO::MakePtr("ColorMatrixFilter", m);
	}
};

struct CONVOLUTIONFILTER {
	UI8		MatrixX;			//Horizontal matrix size
	UI8		MatrixY;			//Vertical matrix size
	FLOAT	Divisor;			//Divisor applied to the matrix values
	FLOAT	Bias;				//Bias applied to the matrix values
	FLOAT	*Matrix;			//[MatrixX * MatrixY] Matrix values
	RGBA	DefaultColor;		//Default color for pixels outside the image
	union {
		UI8			Flags;
		struct {UI8	PreserveAlpha:1,
					Clamp:1,
					Reserved:6;
		};
	};
	CONVOLUTIONFILTER(istream_ref file)	{
		MatrixX			= file.get();
		MatrixY			= file.get();
		Divisor			= file.get();
		Bias			= file.get();
		Matrix			= ReadArray<FLOAT>(file, MatrixX * MatrixY);
		DefaultColor	= file.get();
		Flags			= file.get();
		file.read(*this);
	}
	operator ISO_ptr<void>()	const	{ return ISO_NULL; }
};

struct BLURFILTER {
	FIXED	BlurX;				//Horizontal blur amount
	FIXED	BlurY;				//Vertical blur amount
	union {
		UI8			Flags;
		struct {UI8 Reserved:3,
					Passes:5;	//Number of blur passes
		};
	};
	BLURFILTER(istream_ref file)	{ file.read(*this); }
	operator ISO_ptr<void>()	const	{ return ISO_NULL; }
};

struct DROPSHADOWFILTER	{
	RGBA	DropShadowColor;	//Color of the shadow
	FIXED	BlurX;				//Horizontal blur amount
	FIXED	BlurY;				//Vertical blur amount
	FIXED	Angle;				//Radian angle of the drop shadow
	FIXED	Distance;			//Distance of the drop shadow
	FIXED8	Strength;			//Strength of the drop shadow
	union {
		UI8			Flags;
		struct {UI8 Passes:5,			//Number of blur passes
					CompositeSource:1,	//Composite source Always 1
					Knockout:1,			//Knockout mode
					InnerShadow:1;		//Inner shadow mode
		};
	};
	DROPSHADOWFILTER(istream_ref file)	{ file.read(*this); }
	operator ISO_ptr<void>()	const	{ return ISO_NULL; }
};

struct GLOWFILTER {
	RGBA	GlowColor;			//Color of the shadow
	FIXED	BlurX;				//Horizontal blur amount
	FIXED	BlurY;				//Vertical blur amount
	FIXED8	Strength;			//Strength of the glow
	union {
		UI8			Flags;
		struct {UI8 Passes:5,			//Number of blur passes
					CompositeSource:1,	//Composite source Always	1
					Knockout:1,			//Knockout mode
					InnerGlow:1;		//Inner glow mode
		};
	};
	GLOWFILTER(istream_ref file)	{ file.read(*this); }
	operator ISO_ptr<void>()	const	{
		float	size[2] = {BlurX, BlurY};
		ISO_ptr<anything>	p("GlowFilter");
		p->Append(ISO::MakePtr("colour", (rgba8&)GlowColor));
		p->Append(ISO::MakePtr("size", size));
		p->Append(ISO_ptr<float>("strength", Strength));
		return p;
	}
};

struct BEVELFILTER {
	RGBA	ShadowColor;		//Color of the shadow
	RGBA	HighlightColor;		//Color of the highlight
	FIXED	BlurX;				//Horizontal blur amount
	FIXED	BlurY;				//Vertical blur amount
	FIXED	Angle;				//Radian angle of the drop shadow
	FIXED	Distance;			//Distance of the drop shadow
	FIXED8	Strength;			//Strength of the drop shadow
	union {
		UI8			Flags;
		struct {UI8 Passes:4,			//Number of blur passes
					OnTop:1,			//OnTop mode
					CompositeSource:1,	//Composite source Always 1
					Knockout:1,			//Knockout mode
					InnerShadow:1;		//Inner shadow mode
		};
	};
	BEVELFILTER(istream_ref file)	{ file.read(*this); }
	operator ISO_ptr<void>()	const	{ return ISO_NULL; }
};

struct GRADIENTGLOWFILTER {
	UI8		NumColors;			//Number of colors in the gradient
	RGBA	*GradientColors;	//[NumColors]	Gradient colors
	UI8		*GradientRatio;		//[NumColors]	Gradient ratios
	FIXED	BlurX;				//Horizontal blur amount
	FIXED	BlurY;				//Vertical blur amount
	FIXED	Angle;				//Radian angle of the gradient glow
	FIXED	Distance;			//Distance of the gradient glow
	FIXED8	Strength;			//Strength of the gradient glow
	union {
		UI8			Flags;
		struct {UI8 Passes:4,			//Number of blur passes
					OnTop:1,			//OnTop mode
					CompositeSource:1,	//Composite source Always 1
					Knockout:1,			//Knockout mode
					InnerShadow:1;		//Inner glow mode
		};
	};
	GRADIENTGLOWFILTER(istream_ref file) {
		NumColors		= file.get();
		GradientColors	= ReadArray<RGBA>(file, NumColors);
		GradientRatio	= ReadArray<UI8>(file, NumColors);
		BlurX			= file.get();
		BlurY			= file.get();
		Angle			= file.get();
		Distance		= file.get();
		Strength		= file.get();
		Flags			= file.get();
	}
	operator ISO_ptr<void>()	const	{

		return ISO_NULL;
	}
};

struct GRADIENTBEVELFILTER {
	UI8		NumColors;			//Number of colors in the gradient
	RGBA	*GradientColors;	//[NumColors]	Gradient colors
	UI8		*GradientRatio;		//[NumColors]	Gradient ratios
	FIXED	BlurX;				//Horizontal blur amount
	FIXED	BlurY;				//Vertical blur amount
	FIXED	Angle;				//Radian angle of the gradient bevel
	FIXED	Distance;			//Distance of the gradient bevel
	FIXED8	Strength;			//Strength of the gradient bevel
	union {
		UI8			Flags;
		struct {UI8 Passes:4,			//Number of blur passes
					OnTop:1,			//OnTop mode
					CompositeSource:1,	//Composite source Always 1
					Knockout:1,			//Knockout mode
					InnerShadow:1;		//Inner bevel mode
		};
	};
	GRADIENTBEVELFILTER(istream_ref file) {
		NumColors		= file.get();
		GradientColors	= ReadArray<RGBA>(file, NumColors);
		GradientRatio	= ReadArray<UI8>(file, NumColors);
		BlurX			= file.get();
		BlurY			= file.get();
		Angle			= file.get();
		Distance		= file.get();
		Strength		= file.get();
		Flags			= file.get();
	}
	operator ISO_ptr<void>()	const	{ return ISO_NULL; }
};

#pragma pack()

struct ZONEDATA {
	FLOAT16	AlignmentCoordinate;	// X (left) or Y (baseline) coordinate of the alignment zone.
	FLOAT16	Range;					// Width or height of the alignment zone.
	ZONEDATA(istream_ref file) : AlignmentCoordinate(file.get<FLOAT16>()), Range(file.get<FLOAT16>())	{}
};

struct SCENE {
	EncodedU32	offset;
	STRING		name;
	SCENE(istream_ref file) : offset(file), name(file)	{}
};

typedef UI8	LANGCODE;
} } //namespace iso::flash

//-----------------------------------------------------------------------------
//	FlashState
//-----------------------------------------------------------------------------

using namespace iso;
using namespace iso::flash;

class FlashState {
	flash::RECT			framesize;	// Frame size in twips
	FIXED8				framerate;	// Frame delay in 8.8 fixed number of frames per second
	RGB					background;
	uint32				fileattributes;
	int					version;
	anything			dictionary;

	void				AddCharacter(UI16 id, ISO_ptr<void> p);
	void				PlaceObject(const ISO_ptr<flash::Object> &obj, istream_ref file, int version, uint32 flags);

	HDRpixel			GetColour(istream_ref file, bool alpha) {
		return alpha ? (ISO_rgba)file.get<RGBA>() : (ISO_rgba)file.get<RGB>();
	}

	ISO_ptr<void>		GetFillStyle(istream_ref file, int version);
	void				GetLineStyle(istream_ref file, int version);
	uint32				GetClipEventFlags(istream_ref file);
	anything			GetFilterList(istream_ref file);

	ISO_ptr<void>		DefineShape(istream_ref file, int version);
	ISO_ptr<void>		DefineButton(istream_ref file, int version);
	ISO_ptr<void>		DefineFont(istream_ref file, int version);
	void				DefineFontInfo(istream_ref file, ISO_ptr<void> &font, int version);
	void				DefineFontAlignZones(istream_ref file, ISO_ptr<void> font);
	ISO_ptr<void>		DefineText(istream_ref file, bool alpha);
	ISO_ptr<void>		DefineEditText(istream_ref file);
	ISO_ptr<void>		DefineSound(istream_ref file);
	void				ExportAssets(istream_ref file);

	tag					MakeID(uint32 id)	{ return format_string("_%i", id);	}
	void				ProcessTags(flash::Movie &frames, istream_ref file);
public:
	dynamic_array<SCENE>	scenes;

	FlashState(int version) : version(version), dictionary(0) {}

	void				Read(flash::Movie &frames, istream_ref file)		{
		framesize		= file;			// Frame size in twips
		framerate		= file.get();	// Frame delay in 8.8 fixed number of frames per second
		UI16	count	= file.get();	// Total number of frames in file
		ProcessTags(frames, file);
	}
	ISO_ptr<void>&		GetCharacter(UI16 id) {
		return dictionary[id];
	}
	ISO_ptr<void>		GetCharacterTest(UI16 id) const {
		return id < dictionary.Count() ? dictionary[id] : ISO_NULL;
	}
	const anything&		GetDictionary()	const	{ return dictionary;}
	float4p				GetFrameSize()	const	{ return framesize; }
	float				GetFrameRate()	const	{ return framerate;	}
	const RGB&			GetBackground()	const	{ return background; }
	int					GetVersion()	const	{ return version;	}

};

void FlashState::AddCharacter(UI16 id, ISO_ptr<void> p) {
	if (dictionary.Count() <= id)
		dictionary.Resize(id + 1);
	if (!p.ID()) {
		p.SetID(MakeID(id));
		p.SetFlags(ISO::Value::ALWAYSMERGE);
	}
	dictionary[id] = p;
}

ISO_ptr<flash::Object> AddObject(flash::Frame &objects, int depth) {
	if (objects.Count() <= depth)
		objects.Resize(depth + 1);

	ISO_ptr<flash::Object>	&obj = objects[depth];
	return !obj ? obj.Create() : (obj = Duplicate(obj));
}

void FlashState::ProcessTags(flash::Movie &frames, istream_ref file) {
	flash::Frame				objects;
	STRING					frame_label;
	dynamic_array<SCENE>	frame_labels;
	JPG						jpg;
#ifdef ISO_EDITOR
	dynamic_array<flash::ABC*> scripts;
#endif

	for (;;) {
		TAG		tg = file.get();
		if (tg.type == tag_End)
			break;

		uint32	len = tg.length;
		if (len == 63)
			len = file.get();

		streamptr	tag_start	= file.tell();
		streamptr	tag_end		= tag_start + len;

		try {switch (tg.type) {
			case tag_FileAttributes:
				fileattributes = file.get();
				break;
			case tag_SetBackgroundColor:
				background = file.get();
				break;
			case tag_ExportAssets:
				ExportAssets(file);
				break;

			case tag_FrameLabel:
				frame_label	= STRING(file);
				break;

			case tag_ShowFrame: {
				ISO_ptr<flash::Frame>	frame(frame_label);
				frame_label.clear();
				for (int i = 0, n = objects.Count(); i < n; i++) {
					if (ISO_ptr<void> p = objects[i])
						frame->Append(p);
				}
#ifdef ISO_EDITOR
				for (size_t i = 0, n = scripts.size(); i < n; i++)
					frame->Append(ISO_ABC(scripts[i]));
				scripts.clear();
#endif
				frames.Append(frame);
				break;
			}

			case tag_PlaceObject: {
				ISO_ptr<void>	chr		= GetCharacter(file.get<UI16>());
				flash::Object	*obj	= AddObject(objects, file.get<UI16>());
				obj->character	= chr;
				obj->trans		= MATRIX(file);
				if (file.tell() < tag_end)
					obj->col_trans = (simple_vec<float4p, 2>)CXFORM(file, false);	// shouldn't need cast
				break;
			}
			case tag_PlaceObject2: {
				UI8		flags = file.get();
				PlaceObject(AddObject(objects, file.get<UI16>()), file, 2, flags);
				break;
			}
			case tag_PlaceObject3: {
				UI16	flags = file.get();
				PlaceObject(AddObject(objects, file.get<UI16>()), file, 3, flags);
				break;
			}

			case tag_RemoveObject:
				file.get<UI16>();
			case tag_RemoveObject2:
				objects[file.get<UI16>()].Clear();
				break;

			//shapes
			case tag_DefineShape: {
				UI16 id = file.get();
				AddCharacter(id, DefineShape(file, 1));
				break;
			}
			case tag_DefineShape2: {
				UI16 id = file.get();
				AddCharacter(id, DefineShape(file, 2));
				break;
			}
			case tag_DefineShape3: {
				UI16 id = file.get();
				AddCharacter(id, DefineShape(file, 3));
				break;
			}
			case tag_DefineShape4: {
				UI16 id = file.get();
				AddCharacter(id, DefineShape(file, 4));
				break;
			}

			//buttons
			case tag_DefineButton: {
				UI16 id = file.get();
				AddCharacter(id, DefineButton(file, 1));
				break;
			}
			case tag_DefineButton2: {
				UI16 id = file.get();
				AddCharacter(id, DefineButton(file, 2));
				break;
			}
			case tag_DefineButtonCxform: {
				ISO_ptr<flash::Frame> p = GetCharacter(file.get());
				CXFORM	cx(file, false);
				for (int i = 0, n = p->Count(); i < n; i++)
					(*p)[i]->col_trans = (simple_vec<float4p, 2>)cx;
				break;
			}

			//bitmaps
			case tag_JPEGTables:
				if (len) {
					bitmap bm;
					jpg.ReadBitmap(file, bm);
				}
				break;
			case tag_DefineBits: {
				UI16	id = file.get();
				ISO_ptr<bitmap>	bm(MakeID(id));
				if (jpg.ReadBitmap(file, *bm)) {
					SetBitmapFlags(bm.get());
					AddCharacter(id, bm);
				}
				break;
			}
			case tag_DefineBitsJPEG2:
			case tag_DefineBitsJPEG3:
			case tag_DefineBitsJPEG4: {
				UI16	id		= file.get();
				UI32	alpha	= tg.type != tag_DefineBitsJPEG2 ? file.get<UI32>() : 0;
				UI16	deblock	= tg.type == tag_DefineBitsJPEG4 ? file.get<UI16>() : 0;
				streamptr	col	= file.tell();
				ISO_ptr<bitmap>	bm(MakeID(id));
				if (!jpg.ReadBitmap(file, *bm))
					jpg.ReadBitmap(file, *bm);
				SetBitmapFlags(bm.get());
				if (alpha) {
					int				size	= bm->Width() * bm->Height();
					malloc_block	alphas(size);
					try {
						file.seek(col + alpha);
						zlib_reader(make_reader_offset(file, tag_end - file.tell()), size).readbuff(alphas, size);
					} catch_all() {
						ISO_TRACE("Bad JPEG alpha\n");
					}
					ISO_rgba	*p = bm->ScanLine(0);
					for (uint8 *a = alphas; size--;)
						p++->a = *a++;
				}
				AddCharacter(id, bm);
				break;
			}
			case tag_DefineBitsLossless: {
				UI16	id		= file.get();
				UI8		format	= file.get();
				UI16	width	= file.get();
				UI16	height	= file.get();
				ISO_ptr<bitmap>	bm(MakeID(id));
				bm->Create(width, height);
				istream_offset	chunk(copy(file), tag_end - file.tell());
				switch (format) {
					case 3: {
						RGB		table[256];
						UI8		tablesize	= file.get();
						int		scan		= align(width, 4);
						malloc_block	map(scan * height);
						zlib_reader	z(chunk, tablesize * sizeof(RGB) + scan * height);
						z.readbuff(table, tablesize * sizeof(RGB));
						z.readbuff(map, scan * height);
						copy(table, bm->CreateClut(tablesize));
						for (int y = 0; y < height; y++)
							copy_n((UI8*)map + scan * y, bm->ScanLine(y), width);
						break;
					}
					case 4: {
						int		scan	= align(width * 2, 4);
						malloc_block	map(scan * height);
						zlib_reader(chunk, scan * height).readbuff(map, scan * height);
						for (int y = 0; y < height; y++)
							copy_n((PIX15*)map + scan * y, bm->ScanLine(y), width);
						break;
					}
					case 5: {
						PIX24	*map	= ReadArray<PIX24>(zlib_reader(chunk, tag_end - file.tell()).me(), width * height);
						for (int y = 0; y < height; y++)
							copy_n(map + width * y, bm->ScanLine(y), width);
						delete[] map;
						break;
					}
				}
				SetBitmapFlags(bm.get());
				AddCharacter(id, bm);
				break;
			}
			case tag_DefineBitsLossless2: {
				UI16	id		= file.get();
				UI8		format	= file.get();
				UI16	width	= file.get();
				UI16	height	= file.get();
				ISO_ptr<bitmap>	bm(MakeID(id));
				bm->Create(width, height);
				istream_offset	chunk(copy(file), tag_end - file.tell());
				switch (format) {
					case 3: {
						RGBA	table[256];
						UI8		tablesize	= file.get();
						int		scan		= align(width, 4);
						malloc_block	map(scan * height);
						zlib_reader	z(chunk, tablesize * sizeof(RGB) + scan * height);
						z.readbuff(table, tablesize * sizeof(RGB));
						z.readbuff(map, scan * height);
						copy(table, bm->CreateClut(tablesize));
						for (int y = 0; y < height; y++)
							copy_n((UI8*)map + scan * y, bm->ScanLine(y), width);
						break;
					}
					case 5: {
						int		scan		= width * 4;
						malloc_block	map(scan * height);
						zlib_reader(chunk, scan * height).readbuff(map, scan * height);
						for (int y = 0; y < height; y++)
							copy_n((ARGB*)map + width * y, bm->ScanLine(y), width);
						break;
					}
				}
				SetBitmapFlags(bm.get());
				AddCharacter(id, bm);
				break;
			}

			//fonts
			case tag_DefineFont: {
				UI16 id = file.get();
				AddCharacter(id, DefineFont(file, 1));
				break;
			}
			case tag_DefineFont2: {
				UI16 id = file.get();
				AddCharacter(id, DefineFont(file, 2));
				break;
			}
			case tag_DefineFont3: {
				UI16 id = file.get();
				AddCharacter(id, DefineFont(file, 3));
				break;
			}
			case tag_DefineFont4: {
				enum {
					FF_HASFONTDATA	= 2,
					FF_ITALIC		= 1,
					FF_BOLD			= 0,
				};

				UI16	id		= file.get();
				uint8	flags	= file.get();
				STRING	name	= file;
				if (FileHandler *fh = FileHandler::Get("ttf"))
					AddCharacter(id, fh->Read(none, make_reader_offset(file, tag_end - file.tell())));
//				ISO_ptr<ISO_openarray<uint8> >	p(0);
//				file.readbuff(p->Create(len), len);
//				AddCharacter(id, p);
				break;
			}
			case tag_DefineFontInfo:
				DefineFontInfo(file, GetCharacter(file.get()), 1);
				break;
			case tag_DefineFontInfo2:
				DefineFontInfo(file, GetCharacter(file.get()), 2);
				break;
			case tag_DefineFontAlignZones:
				DefineFontAlignZones(file, GetCharacter(file.get()));
				break;
			case tag_CSMTextSettings: {
				ISO_ptr<void>	text = GetCharacter(file.get());
				struct CSMText_Opts { UI8 unused:3, GridFit:3, UseFlashType:2; };
				CSMText_Opts	opts = file.get();
				F32		Thickness	= file.get();
				F32		Sharpness	= file.get();
				UI8		Reserved	= file.get();// Must be 0.
				break;
			}
			case tag_DefineFontName: {
				ISO_ptr<void>	p = GetCharacter(file.get());
				p.SetID(STRING(file));
				break;
			}

			//text
			case tag_DefineText: {
				UI16 id = file.get();
				AddCharacter(id, DefineText(file, false));
				break;
			}
			case tag_DefineText2: {
				UI16 id = file.get();
				AddCharacter(id, DefineText(file, true));
				break;
			}
			case tag_DefineEditText: {
				UI16 id = file.get();
				AddCharacter(id, DefineEditText(file));
				break;
			}

			case tag_DefineBinaryData: {
				UI16	id			= file.get();
				UI32	reserved	= file.get();
				ISO_ptr<ISO_openarray<uint8> >	p(0);
				file.readbuff(p->Create(len - 6), len - 6);

				memory_reader	m(memory_block(*p, len - 6));
				uint32		t = m.get<uint32le>() & 0xffffff;
				if (t == 'SWF' || t == 'SWC') {
					m.seek(0);
					AddCharacter(id, FileHandler::Get("swf")->Read(none, m));
				} else {
					AddCharacter(id, p);
				}
				break;
			}

			case tag_DefineSound: {
				UI16 id = file.get();
				AddCharacter(id, DefineSound(file));
				break;
			}
			case tag_DefineSprite: {
				UI16 id	= file.get();
				UI16 nf	= file.get();
				ISO_ptr<flash::Movie>	p(0);
				ProcessTags(*p, file);
				AddCharacter(id, p);
				break;
			}

			case tag_DoABC: {
#ifdef ISO_EDITOR
				UI32	flags	= file.get();
				STRING	name	= file;
				uint32	len		= tag_end - file.tell();
				flash::ABC	*abc = DecodeABC(malloc_block(file, len));
				new (scripts) flash::ABC*(abc);
#endif
				break;
			}

			case tag_SymbolClass: {
				UI16	num = file.get();
				for (int i = 0; i < num; i++) {
					UI16	id	= file.get();
					STRING	s	= file;
				}
				break;
			}

			case tag_DefineSceneAndFrameLabelData: {
				if (uint32	SceneCount = EncodedU32(file)) {
					for (int i = 0; i < SceneCount; i++)
						scenes.emplace_back(file);
				}
				if (uint32 FrameLabelCount	= EncodedU32(file)) {
					for (int i = 0; i < FrameLabelCount; i++)
						frame_labels.emplace_back(file);
				}
				break;
			}

			case tag_Metadata:
				break;	// ignored
			default:
				ISO_TRACEF("Unhandled flash tag %i\n", tg.type);
				break;
		} } catch (const char *error) {
			throw_accum(error << " in tag " << tg.type << " at offset 0x" << hex(tag_start));
		}

		if (uint32 skipped = tag_end - file.tell())
			ISO_TRACEF("Tag %i skipped %i\n", tg.type, skipped);

		file.seek(tag_end);
	}
}

uint32 FlashState::GetClipEventFlags(istream_ref file) {
	return version >= 6 ? file.get<UI32>() : file.get<UI16>();
}

ISO_ptr<void> FlashState::GetFillStyle(istream_ref file, int version) {
	enum {
		FS_SOLID				= 0x00,	// solid fill
		FS_GRADIENT_LINEAR		= 0x10,	// linear gradient fill
		FS_GRADIENT_RADIAL		= 0x12,	// radial gradient fill
		FS_GRADIENT_FOCAL		= 0x13,	// focal radial gradient fill
		FS_BITMAP_REPEAT		= 0x40,	// repeating bitmap fill
		FS_BITMAP_CLIP			= 0x41,	// clipped bitmap fill
		FS_BITMAP_POINT_REPEAT	= 0x42,	// non-smoothed repeating bitmap
		FS_BITMAP_POINT_CLIP	= 0x43,	// non-smoothed clipped bitmap
	};

	switch (UI8 type = file.get()) {
		case FS_SOLID: {
			ISO_ptr<flash::Solid>	p(0);
			*p = GetColour(file, version >= 3);
			return p;
		}
		case FS_GRADIENT_LINEAR:
		case FS_GRADIENT_RADIAL:
		case FS_GRADIENT_FOCAL: {
			ISO_ptr<flash::Gradient>	p(0);
			p->matrix	= MATRIX(file);
			p->matrix[0] *= 16384 / 20;
			p->matrix[1] *= 16384 / 20;
			uint8	f	= file.getc();
			p->flags	= (f & 0xf0) | (type != FS_GRADIENT_LINEAR ? flash::Gradient::radial : 0);
			int		n	= f & 15;
			for (flash::Gradient::entry *i = p->entries.Create(n), *e = i + n; i != e; ++i) {
				i->a = file.get<unorm8>();
				i->b = GetColour(file, version >= 3);
			}
			p->focal_point = type == FS_GRADIENT_FOCAL ? float(file.get<FIXED8>()) : 0.f;
			return p;
		}
		case FS_BITMAP_REPEAT:
		case FS_BITMAP_CLIP:
		case FS_BITMAP_POINT_REPEAT:
		case FS_BITMAP_POINT_CLIP: {
			UI16	bitmapid		= file.get();
			MATRIX	bitmapmatrix	= file;
			if (bitmapid == 0xffff)
				return ISO_NULL;
			ISO_ptr<flash::Bitmap>	p(0);
			p->a	= GetCharacterTest(bitmapid);
			p->b	= bitmapmatrix;
			return p;
//			return GetCharacter(bitmapid);
		}

	}
	return ISO_NULL;
}

void FlashState::GetLineStyle(istream_ref file, int version) {
	enum {
		LS2_STARTCAPSTYLE	= 14,	//2
		LS2_JOINSTYLE		= 12,	//2
		LS2_HASFILL			= 11,	//1
		LS2_NOHSCALE		= 10,	//1
		LS2_NOVSCALE		= 9,	//1
		LS2_PIXELHINTINT	= 8,	//1
		LS2_NOCLOSE			= 2,	//1
		LS2_ENDCAPSTYLE		= 0,	//2
	};

	int	width	= (UI16)file.get();
	if (version == 4) {
		UI16	flags = file.get();
		if (((flags >> LS2_JOINSTYLE) & 3) == 2) {
			FIXED8	miterlimitfactor = file.get();
		}
		if (flags & (1<<LS2_HASFILL)) {
			GetFillStyle(file, version);
		} else {
			RGBA	colour = file.get();
		}
	} else if (version == 3) {
		file.get<RGBA>();
	} else {
		file.get<RGB>();
	}
}


anything FlashState::GetFilterList(istream_ref file) {
	anything	a;
	for (int n = file.get<UI8>(); n--;) {
		switch (file.get<UI8>()) {
			case 0:	a.Append(DROPSHADOWFILTER(file));	break;		//dropshadow
			case 1:	{ BLURFILTER			f(file);	break; }	//blurfilter
			case 2:	a.Append(GLOWFILTER(file));			break;		//glowfilter
			case 3:	{ BEVELFILTER			f(file);	break; }	//bevelfilter
			case 4:	{ GRADIENTGLOWFILTER	f(file);	break; }	//gradientglowfilter
			case 5:	{ CONVOLUTIONFILTER		f(file);	break; }	//convolutionfilter
			case 6:	a.Append(COLORMATRIXFILTER(file));	break;		//colourmatrixfilter
			case 7:	{ GRADIENTBEVELFILTER	f(file);	break; }	//gradientbevelfilter
			default:
				throw_accum("Bad filter");
		}
	}
	return a;
};

ISO_ptr<void> FlashState::DefineShape(istream_ref file, int version) {
	ISO_ptr<anything>	p;

	flash::RECT	bounds	= file;
	if (version == 4) {
		flash::RECT	edgebounds	= file;
		UI8			flags		= file.get();	// bit 0 = 'uses scaling strokes', bit 1 = 'uses non scaling strokes'
	}

	dynamic_array<ISO_ptr<void> > fills;

	//fillstylearray
	int		fillstylecount = file.get<UI8>();
	if (version >= 2 && fillstylecount == 0xff)
		fillstylecount = file.get<UI16>();
	for (int i = 0; i < fillstylecount; i++)
		fills.push_back(GetFillStyle(file, version));

	//linestylearray
	int		linestylecount = file.get<UI8>();
	if (version >= 2 && linestylecount == 0xff)
		linestylecount = file.get<UI16>();
	for (int i = 0; i < linestylecount; i++)
		GetLineStyle(file, version);

	int	x = 0, y = 0;
	int	fillstyle0 = 0, fillstyle1 = 0, linestyle = 0;

	int	t = file.getc();
	int	numfillbits	= t >> 4;
	int	numlinebits	= t & 15;

	enum {
		TYPE			= 5,
		NE_NEWSTYLES	= 4,
		NE_LINESTYLE	= 3,
		NE_FILLSTYLE1	= 2,
		NE_FILLSTYLE0	= 1,
		NE_MOVETO		= 0,
		E_STRAIGHT		= 4,
	};

	ISO_ptr<flash::Shape>	shape(0);
	vlc	v(file);

	for (;;) {
		int	style;

		while ((style = v(6)) & (1 << TYPE)) {
			int	x0 = x, y0 = y;
			int	n = (style & 15) + 2;

			shape->b.Append(float2p{x / 20.f, y / 20.f});

			if (style & (1 << E_STRAIGHT)) {	// straight edge
				if (v.get_bit()) {
					x += v.get_signed(n);
					y += v.get_signed(n);
				} else {
					if (v.get_bit())
						y += v.get_signed(n);
					else
						x += v.get_signed(n);
				}
			} else {							// curved edge
				int	controldeltax	= x + v.get_signed(n);
				int	controldeltay	= y + v.get_signed(n);
				x = controldeltax + v.get_signed(n);
				y = controldeltay + v.get_signed(n);
			}
		}

		if (style == 0)
			break;

		if (shape->b) {
			if (!p)
				p.Create();
			p->Append(shape);
		}

		shape.Create();

		if (style & (1 << NE_MOVETO)) {
			if (style & (1 << NE_NEWSTYLES))
				x = y = 0;
			int	n = v(5);
			x += v.get_signed(n);
			y += v.get_signed(n);
		}
		if (style & (1 << NE_FILLSTYLE0))
			fillstyle0 = v(numfillbits);

		if (style & (1 << NE_FILLSTYLE1))
			fillstyle1 = v(numfillbits);

		if (style & (1 << NE_LINESTYLE))
			linestyle = v(numlinebits);

		if (style & (1 << NE_NEWSTYLES)) {
			v.reset();
			//fillstylearray
			fillstylecount = file.get<UI8>();
			if (fillstylecount == 0xff)
				fillstylecount = file.get<UI16>();
			fills.clear();
			for (int i = 0; i < fillstylecount; i++)
				fills.push_back(GetFillStyle(file, version));

			//linestylearray
			linestylecount = file.get<UI8>();
			if (linestylecount == 0xff)
				linestylecount = file.get<UI16>();
			for (int i = 0; i < linestylecount; i++)
				GetLineStyle(file, version);

			int	t = file.getc();
			numfillbits	= t >> 4;
			numlinebits	= t & 15;
		}
		if (fillstyle1)
			shape->a = fills[fillstyle1 - 1];
	}

	if (!p)
		return shape;
	p->Append(shape);
	return p;
}

ISO_ptr<void> FlashState::DefineButton(istream_ref file, int version) {
	ISO_ptr<flash::Frame>	p(0);
	enum {
		DB_UP			= 0,
		DB_OVER			= 1,
		DB_DOWN			= 2,
		DB_HITTEST		= 3,
		DB_FILTERLIST	= 4,
		DB_BLENDMODE	= 5,
	};

	if (version == 2) {
		UI8		menu	= file.get();
		UI16	actions = file.get();
	}

	while (UI8 flags = file.get()) {
		ISO_ptr<flash::Object>	button(0);
		clear(*button);

		button->col_trans.x = one;//{1,1,1,1};
		button->flags		= (flags & 15) << 8;
		button->character	= GetCharacter(file.get());
		button->clip_depth	= file.get<UI16>();
		button->trans		= MATRIX(file);
		if (version == 2) {
			button->col_trans = (simple_vec<float4p, 2>)CXFORM(file, true);
			if (flags & (1 << DB_FILTERLIST))
				button->filters = GetFilterList(file);
			if (flags & (1 << DB_BLENDMODE))
				button->flags |= file.get<UI8>();
		}
		p->Append(button);
	}

	return p;
}

struct KERN {
	UI16	code1, code2;
	SI16	adjust;
};

ISO_ptr<void> FlashState::DefineFont(istream_ref file, int version) {
	enum {
		FF_HASLAYOUT	= 7,
		FF_SHIFTJIS		= 6,
		FF_SMALLTEXT	= 5,
		FF_ANSI			= 4,
		FF_WIDEOFFSETS	= 3,
		FF_WIDECODES	= 2,
		FF_ITALIC		= 1,
		FF_BOLD			= 0,
	};

	int			flags	= 0, langcode = 0;
	uint16		*codes	= 0;
	KERN		*kerns	= 0;

	streamptr	offset;
	uint32		*offsets;
	int			n;

	if (version >= 2) {
		flags		= file.get<UI8>();
		langcode	= file.get<UI8>();
		STRING2		fontname(file);
		n			= (UI16)file.get();

		offset	= file.tell();
		offsets	= new uint32[n + 1];
		codes	= new uint16[n];
		for (int i = 0; i <= n; i++)
			offsets[i] = flags & (1<<FF_WIDEOFFSETS) ? file.get<UI32>() : file.get<UI16>();

	} else {
		offset	= file.tell();
		n		= file.get<UI16>() / 2;
		offsets	= new uint32[n];
		codes	= NULL;
		offsets[0] = n * 2;
		for (int i = 1; i < n; i++)
			offsets[i] = file.get<UI16>();
	}

	ISO_ptr<ISO_openarray<ISO_openarray<array<float,2> > > >	font(0);

	for (int i = 0; i < n; i++) {
		file.seek(offsets[i] + offset);
		ISO_openarray<array<float,2> > &shape = font->Append();

		int	t = file.getc();
		int	numfillbits	= t >> 4;
		int	numlinebits	= t & 15;
		int	x = 0, y = 0;
		int	fillstyle0 = 0, fillstyle1 = 0, linestyle = 0;

		enum {
			TYPE			= 5,
			NE_NEWSTYLES	= 4,
			NE_LINESTYLE	= 3,
			NE_FILLSTYLE1	= 2,
			NE_FILLSTYLE0	= 1,
			NE_MOVETO		= 0,
			E_STRAIGHT		= 4,
		};

		vlc	v(file);
		for (;;) {
			int style = v(6);

			if (style == 0)
				break;

			if (style & (1 << TYPE)) {
				int	x0 = x, y0 = y;
				int	n = (style & 15) + 2;

				shape.Append(make_array(x / 65536.f, y / 65536.f));

				if (style & (1 << E_STRAIGHT)) {	// straight edge
					if (v.get_bit()) {
						x += v.get_signed(n);
						y += v.get_signed(n);
					} else {
						if (v.get_bit())
							y += v.get_signed(n);
						else
							x += v.get_signed(n);
					}
				} else {							// curved edge
					int	controldeltax	= x + v.get_signed(n);
					int	controldeltay	= y + v.get_signed(n);
					x = controldeltax + v.get_signed(n);
					y = controldeltay + v.get_signed(n);
				}
			} else {
				if (style & (1 << NE_MOVETO)) {
					int	n = v(5);
					x += v.get_signed(n);
					y += v.get_signed(n);
				}
				if (style & (1 << NE_FILLSTYLE0))
					fillstyle0 = v(numfillbits);

				if (style & (1 << NE_FILLSTYLE1))
					fillstyle1 = v(numfillbits);

				if (style & (1 << NE_LINESTYLE))
					linestyle = v(numlinebits);

			}
		}
	}

	if (version >= 2) {
		file.seek(offsets[n] + offset);
		for (int i = 0; i < n; i++)
			codes[i] = flags & (1<<FF_WIDECODES) ? file.get<UI16>() : file.get<UI8>();

		if (flags & (1<<FF_HASLAYOUT)) {
			SI16	fontascent	= file.get();
			SI16	fontdescent	= file.get();
			SI16	fontleading	= file.get();

			SI16		*fontadvance	= new SI16[n];
			flash::RECT	*fontrects		= new flash::RECT[n];

			file.readbuff(fontadvance, n * sizeof(SI16));
			for (int i = 0; i < n; i++)
				fontrects[i] = file;

			UI16	kerningcount = file.get();
			kerns	= new KERN[kerningcount];
			for (int i = 0; i < kerningcount; i++) {
				kerns[i].code1 = flags & (1<<FF_WIDECODES) ? file.get<UI16>() : file.get<UI8>();
				kerns[i].code2 = flags & (1<<FF_WIDECODES) ? file.get<UI16>() : file.get<UI8>();
				kerns[i].adjust	= file.get<SI16>();
			}
		}
	}
	return font;
}

void FlashState::DefineFontInfo(istream_ref file, ISO_ptr<void> &font, int version) {
	enum {
		FF_SMALLTEXT	= 5,
		FF_SHIFTJIS		= 4,
		FF_ANSI			= 3,
		FF_ITALIC		= 2,
		FF_BOLD			= 1,
		FF_WIDECODES	= 0,
	};

	STRING2		fontname(file);
	UI8			flags	= file.get();
	LANGCODE	lang	= 0;
	if (version >= 2)
		lang = file.get();
}

void FlashState::DefineFontAlignZones(istream_ref file, ISO_ptr<void> font) {
	UI8		CSMTableHint	= file.get();
#if 0
	for (int i = 0; i < num_glyphs; i++) {
		int	n = file.get<UI8>();	//Always 2
		for (int i = 0; i < n; i++)
			ZONEDATA	zd = file.get();
		enum {
			ZoneMaskY	= 1 << 1,
			ZoneMaskX	= 1 << 0,
		};
		UI8	flags	= file.get();
	}
#endif
}


ISO_ptr<void> FlashState::DefineText(istream_ref file, bool alpha) {
	enum {
		SF_TYPE			= 7,
		SF_RESERVED		= 4,
		SF_HASFONT		= 3,
		SF_HASCOLOUR	= 2,
		SF_HASYOFFSET	= 1,
		SF_HASXOFFSET	= 0,
	};

	flash::RECT	bounds		= file;
	MATRIX		matrix		= file;
	UI8			glyphbits	= file.get();
	UI8			advancebits	= file.get();

	UI16		fontid;
	RGB			textcolour;
	RGBA		textcolour2;
	SI16		xoffset, yoffset;
	UI16		textheight;

	while (UI8 style = file.get()) {

		if (style & (1 << SF_HASFONT))
			fontid = file.get();

		if (style & (1 << SF_HASCOLOUR)) {
			if (alpha)
				textcolour2 = file.get();
			else
				textcolour = file.get();
		}

		if (style & (1 << SF_HASYOFFSET))
			yoffset = file.get();

		if (style & (1 << SF_HASXOFFSET))
			xoffset = file.get();

		if (style & (1 << SF_HASFONT))
			textheight = file.get();

		UI8	glyphcount = file.get();
		for (vlc v(file); glyphcount--;) {
			int	index	= v(glyphbits);
			xoffset		+= v(advancebits);
		}
	}
	return ISO_NULL;
}

ISO_ptr<void> FlashState::DefineEditText(istream_ref file) {
	enum {
		SF_HASTEXT		= 7,
		SF_WORDWRAP		= 6,
		SF_MULTILINE	= 5,
		SF_PASSWORD		= 4,
		SF_READONLY		= 3,
		SF_HASCOLOUR	= 2,
		SF_HASMAXLENGTH	= 1,
		SF_HASFONT		= 0,

		SF_HASFONTCLASS	= 15,
		SF_AUTOSIZE		= 14,
		SF_HASLAYOUT	= 13,
		SF_NOSELECT		= 12,
		SF_BORDER		= 11,
		SF_WASSTATIC	= 10,
		SF_HTML			= 9,
		SF_USEOUTLINES	= 8,
	};

	ISO_ptr<flash::Text>	p(0);
	clear(*p);

	p->bounds	= (float4p)flash::RECT(file);

	UI16	opts		= file.get();
	STRING	fontclass;
	UI16	maxlen;

	if (opts & (1 << SF_HASFONT))
		p->font = GetCharacter(file.get());

	if (opts & (1 << SF_HASFONTCLASS))
		fontclass = file;

	if (opts & (1 << SF_HASFONT))
		p->size = file.get<UI16>() / 20.f;

	if (opts & (1 << SF_HASCOLOUR))
		p->colour = GetColour(file, true);

	if (opts & (1 << SF_HASMAXLENGTH))
		maxlen = file.get();

	if (opts & (1 << SF_HASLAYOUT)) {
		p->align		= file.get<UI8>();	// 0 = Left 1 = Right 2 = Center 3 = Justify
		UI16	LeftMargin	= file.get();	// Left margin in twips.
		UI16	RightMargin	= file.get();	// Right margin in twips.
		UI16	Indent		= file.get();	// Indent in twips.
		SI16	Leading		= file.get();	// Leading in twips (vertical distance between bottom of descender of one line and top of ascender of the next).
		p->margins = float4{LeftMargin / 20.f, RightMargin / 20.f, Indent / 20.f, Leading / 20.f};
	}
	p.SetID(STRING(file));			// STRING Name of the variable where the contents of the text field are stored

	if (opts & (1 << SF_HASTEXT))
		p->text	= STRING(file);

	return p;
}

ISO_ptr<void> FlashState::DefineSound(istream_ref file) {
	struct { UI8 format:4, rate:2, size:1, chans:1; } sound = file.get();
	UI32	count	= file.get();
	float	freq	= 44100 >> (3 - sound.rate);

	ISO_ptr<sample>	sm(0);
	switch (sound.format) {
		case smp_Uncompressed:
		case smp_Uncompressed_le:
			file.readbuff(sm->Create(count >> (sound.chans + sound.size), sound.chans + 1, sound.size ? 16 : 8), count);
			break;
		case smp_ADPCM:
		case smp_MP3:
		case smp_Nellymoser16kHz:	freq = 16000;
		case smp_Nellymoser8kHz:	freq = 8000;
		case smp_Nellymoser6kHz:	freq = 6000;
		case smp_Speex:
			break;
	}
	return ISO_NULL;
}

void FlashState::PlaceObject(const ISO_ptr<flash::Object> &obj, istream_ref file, int version, uint32 flags) {
	enum {
		PF_HASIMAGE				= 12,
		PF_HASCLASSNAME			= 11,
		PF_HASCACHEASBITMAP		= 10,
		PF_HASBLENDMODE			= 9,
		PF_HASFILTERLIST		= 8,

		PF_HASCLIPACTIONS		= 7,
		PF_HASCLIPDEPTH			= 6,
		PF_HASNAME				= 5,
		PF_HASRATIO				= 4,
		PF_HASCOLOURTRANSFORM	= 3,
		PF_HASMATRIX			= 2,
		PF_HASCHARACTER			= 1,
		PF_MOVE					= 0,
	};

	if ((flags & (1<<PF_HASCLASSNAME)) || ((flags & (1<<PF_HASIMAGE)) && (flags & (1<<PF_HASCHARACTER)))) {
		STRING	classname(file);
	}
	if (flags & (1<<PF_HASCHARACTER))
		obj->character = GetCharacter(file.get<UI16>());

	if (flags & (1<<PF_HASMATRIX))
		obj->trans = MATRIX(file);

	if (flags & (1<<PF_HASCOLOURTRANSFORM))
		obj->col_trans = (simple_vec<float4p,2>)CXFORM(file, true);	// shouldn't need cast

	if (flags & (1<<PF_HASRATIO))
		obj->morph_ratio = file.get<UI16>() / 65535.f;

	if (flags & (1<<PF_HASNAME))
		obj.SetID(STRING(file));

	if (flags & (1<<PF_HASCLIPDEPTH))
		obj->clip_depth = file.get<UI16>();

	if (flags & (1<<PF_HASFILTERLIST))
		obj->filters = GetFilterList(file);

	if (flags & (1<<PF_HASBLENDMODE))
		obj->flags = file.get<UI8>();

	if (flags & (1<<PF_HASCLIPACTIONS)) {
		UI16	reserved	= file.get();
		uint32	allevents	= GetClipEventFlags(file);
		while (uint32 events = GetClipEventFlags(file)) {
			UI32	recsize = file.get();
			file.seek_cur(recsize);
		}
	}
}

void FlashState::ExportAssets(istream_ref file) {
	for (int n = file.get<UI16>(); n--;) {
		UI16	tag = file.get();
		STRING	name(file);
	}
}

//-----------------------------------------------------------------------------
//	SWF FileHandler
//-----------------------------------------------------------------------------

class SWFFileHandler : public FileHandler {
	const char*		GetExt() override { return "swf";		}
	const char*		GetDescription() override { return "Flash Movie";	}
	int				Check(istream_ref file) override {
		file.seek(0);
		uint32 t = file.get<uint32le>() & 0xffffff;
		return t == 'SWF' || t == 'SWC' ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}
	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		SWF_header	h			= file.get();
		bool		compressed	= h.Signature[0] == 'C';
		if ((!compressed && h.Signature[0] != 'F') || h.Signature[1] != 'W' || h.Signature[2] != 'S')
			return ISO_NULL;

		ISO_ptr<flash::File>	f(id);
		FlashState			flash(h.Version);
		if (compressed) {
#if 0
			void	*p = malloc(h.FileLength);
			h.Signature[0] = 'F';
			memcpy(p, &h, sizeof(h));
			zlib_reader(file, h.FileLength - sizeof(h)).readbuff((char*)p + sizeof(h), h.FileLength - sizeof(h));
			FileOutput("C:\\uncompressed.swf").writebuff(p, h.FileLength);
			free(p);
			file.seek(sizeof(h));
#endif
			flash.Read(f->movie, zlib_reader(file, h.FileLength - sizeof(h)).me());
		} else {
			flash.Read(f->movie, file);
		}

		RemoveDupes(f, 1);
#if 0
		ISO_ptr<anything>	all(id);
		anything	a = flash.GetDictionary();
		for (int i = 0, n = a.Count(); i < n; i++)
			all->Append(a[i]);

		if (flash.scenes.empty()) {
			all->Append(frames);

		} else for (int i = 0, n = flash.scenes.size(); i < n; i++) {
			ISO_ptr<flash::Movie>	scene(flash.scenes[i].name);
			for (int j = flash.scenes[i].offset, e = i < n - 1 ? flash.scenes[i + 1].offset : frames->Count(); j < e; j++)
				scene->Append((*frames)[j]);
			all->Append(scene);
		}
		return all;
#else
		f->rect			= flash.GetFrameSize();
		f->background	= HDRpixel((ISO_rgba)flash.GetBackground());
		f->framerate	= flash.GetFrameRate();
		return f;
#endif
	}
} swf;


//-----------------------------------------------------------------------------
//	FLA FileHandler
//-----------------------------------------------------------------------------

struct FLAreader {
	XMLreader				xml;
	XMLreader::Data	data;
	ISO_ptr<flash::Frame>	root;

	static void InitMat(float2x3p &m) {
		clear(m);
		m.x.x	= m.y.y = 1;
	}
	static void ReadPoint(const XMLreader::Data &data, float2p &p) {
		p = zero;
		p.x = data["x"];//, p.x);
		p.y = data["y"];//, p.x);
	}

	static void	ReadMatrix(const XMLreader::Data &data, float2x3p &m) {
		data.Read("a", m.x.x);
		data.Read("b", m.x.y);
		data.Read("c", m.y.x);
		data.Read("d", m.y.y);
		data.Read("tx", m.z.x);
		data.Read("ty", m.z.y);
	}

	static void ReadColour(const XMLreader::Data &data, float4p &col) {
		col = (float4)w_axis;
		if (const char *a = data.Find("color")) {
			if (a[0] == '#') {
				struct {unorm8 b, g, r, _; } h;
				from_string(a + 1, (xint32&)h);
				col = float4{h.r, h.g, h.b, 1};
			}
		}
		col.w = data["alpha"];
	}

	ISO_ptr<flash::Shape>	ReadShape();
	ISO_ptr<flash::Frame>	ReadTimeline(anything &lib2);

	FLAreader(istream_ref xml, const ISO::Browser &lib, anything &lib2);
};

ISO_ptr<flash::Shape> FLAreader::ReadShape() {
	ISO_ptr<flash::Shape>	shape(0);
	for (XMLiterator it(xml, data); it.Next();) {
		if (data.Is("fills")) {
			for (it.Enter(); it.Next();) {
				if (data.Is("FillStyle")) {
					int	index = data["index"];
					it.Enter(); it.Next();
					if (data.Is("RadialGradient")) {
						ISO_ptr<flash::Gradient>	g(0);
						g->flags	= flash::Gradient::radial;
						shape->a	= g;
						it.Enter();
						while (it.Next()) {
							if (data.Is("matrix")) {
								xml.ReadNext(data, XMLreader::TAG_BEGIN);
								ReadMatrix(data, g->matrix);
								g->matrix[0] *= 16384 / 20;
								g->matrix[1] *= 16384 / 20;
							} else if (data.Is("GradientEntry")) {
								flash::Gradient::entry	*e = new(g->entries) flash::Gradient::entry;
								data.Read("ratio", e->a);
								ReadColour(data, e->b);
							}
						}
					} else if (data.Is("SolidColor")) {
						ISO_ptr<flash::Solid>	s(0);
						shape->a = s;
						ReadColour(data, *s);
					}
					it.Next();
				}
			}

		} else if (data.Is("strokes")) {
			for (it.Enter(); it.Next();) {
				if (data.Is("StrokeStyle")) {
					int	index = data["index"];
					it.Enter(); it.Next();
					if (data.Is("SolidStroke")) {
						string	scalemode	= data.Find("scaleMode");
						float	weight		= data["weight"];
						xml.ReadNext(data, XMLreader::TAG_BEGIN);
						xml.ReadNext(data, XMLreader::TAG_BEGIN);
						float4p	col;
						ReadColour(data, col);
						xml.ReadNext(data, XMLreader::TAG_END);
					}
					it.Next();
				}
			}

		} else if (data.Is("edges")) {
			const char *fillstyle = 0, *strokestyle = 0;
			for (it.Enter(); it.Next();) {
				if (data.Is("Edge")) {
					if (const char *a = data.Find("fillStyle1"))
						fillstyle = a;
					if (const char *a = data.Find("strokeStyle"))
						strokestyle = a;
					if (const char *a = data.Find("edges")) {
						string_scan	ss(a);
						char		c;
						while (ss.getc() == '!') {
							float2p	*pt = new(shape->b) float2p;
							//ss >> pt->x >> pt->y;
							pt->x = ss.get();
							pt->y = ss.get();
							pt->x /= 20;
							pt->y /= 20;
							while ((c = ss.getc()) && c != '|') {
								if (c == 'S') {
									int n;
									ss >> n;
								}
							}
							if (c != '|')
								break;
							pt = new(shape->b) float2p;
							//ss >> pt->x >> pt->y;
							pt->x = ss.get();
							pt->y = ss.get();
							pt->x /= 20;
							pt->y /= 20;
						}
					}
					if (const char *a = data.Find("cubics")) {
						//cubics
					}
				}
			}
		}
	}
	return shape;
}

ISO_ptr<flash::Frame> FLAreader::ReadTimeline(anything &lib2) {
	ISO_ptr<flash::Frame>	timeline(data.Find("name"));

	xml.ReadNext(data, XMLreader::TAG_BEGIN);
	for (XMLiterator it(xml, data); it.Next();) {
		if (data.Is("DOMLayer")) {
			ISO_ptr<flash::Movie>		movie(data.Find("name"));
			ISO_ptr<flash::Object>	fo(0);
			fo->character = movie;
			timeline->Append(fo);

			it.Enter(); it.Next();
			for (it.Enter(); it.Next();) {
				if (data.Is("DOMFrame")) {
					uint32	duration = data.Get("duration", 1u);
					ISO_ptr<flash::Frame>	frame(0);
					while (duration--)
						movie->Append(frame);

					it.Enter();
					if (it.Next()) {
						for (it.Enter(); it.Next();) {
							if (data.Is("DOMShape")) {
								it.Enter();
								ISO_ptr<flash::Object>	fo(0);
								fo->character	= ReadShape();
								frame->Append(fo);

							} else if (data.Is("DOMBitmapInstance") || data.Is("DOMSymbolInstance")) {
								it.Enter();
								ISO_ptr<flash::Object>	fo(0);
								fo->character	= lib2[tag(data.Find("libraryItemName"))];
								frame->Append(fo);
								float2p			pivot;
								for (XMLiterator it(xml, data); it.Next();) {
									if (data.Is("matrix")) {
										xml.ReadNext(data, XMLreader::TAG_BEGIN);
										ReadMatrix(data, fo->trans);
									} else if (data.Is("transformationPoint")) {
										xml.ReadNext(data, XMLreader::TAG_BEGIN);
										ReadPoint(data, pivot);
									}
								}
							}
						}
						reverse(*frame);
						it.Next();
					}
				}
			}
			it.Next();
		}
	}
	xml.ReadNext(data, XMLreader::TAG_END);
	reverse(*timeline);
	return timeline;
}

FLAreader::FLAreader(istream_ref _xml, const ISO::Browser &lib, anything &lib2) : xml(_xml) {
	xml.ReadNext(data, XMLreader::TAG_BEGIN);

	for (XMLiterator it(xml, data); it.Next();) {
		if (data.Is("media")) {
			for (it.Enter(); it.Next();) {
				if (data.Is("DOMBitmapItem")) {
					const char	*name = data.Find("name");
					filename	fn;
					replace(fn.begin(), name, ",", "&#44");
					ISO_openarray<uint8>	mem	= (*lib.Parse(fn, ~char_set("/"))).Get();
					if (FileHandler *fh = FileHandler::Get(fn.ext())) {
						if (ISO_ptr<bitmap> bm = fh->Read(name, memory_reader(memory_block(mem, mem.Count())).me())) {
							ISO_ptr<flash::Shape>		fs(name);
							ISO_ptr<flash::Bitmap>	fbm(0);
							float	width	= bm->Width(), height = bm->Height();
							float	rect[4] = {0, 0, width, height};

							data.Read("frameLeft",		rect[0]);
							data.Read("frameTop",		rect[1]);
							data.Read("frameRight",		rect[2]);
							data.Read("frameBottom",	rect[3]);
							fbm->b.x = int32x2{int((rect[2] - rect[0]) / width), 0};
							fbm->b.y = int32x2{0, int((rect[3] - rect[1]) / height)};
							fbm->b.z = int32x2{(int)rect[0], (int)rect[1]};
							fbm->a	= bm;

							fs->a	= fbm;
							float2p	*pts = fs->b.Create(4);
							pts[0] = float2{rect[0] / 20, rect[1] / 20};
							pts[1] = float2{rect[2] / 20, rect[1] / 20};
							pts[2] = float2{rect[2] / 20, rect[3] / 20};
							pts[3] = float2{rect[0] / 20, rect[3] / 20};

							lib2.Append(fs);
						}
					}
				}
			}

		} else if (data.Is("symbols")) {
			for (it.Enter(); it.Next();) {
				if (data.Is("Include")) {
					const char	*name = data.Find("href");
					ISO_openarray<uint8>	doc	= (*lib.Parse(name, ~char_set("/"))).Get();
					memory_reader	mi(memory_block(doc, doc.Count()));
					FLAreader	fla(mi, lib, lib2);
					lib2.Append(fla.root);
				}
			}

		} else if (data.Is("timelines") || data.Is("timeline")) {
			for (it.Enter(); it.Next();) {
				if (data.Is("DOMTimeline")) {
					it.Enter();
					root = ReadTimeline(lib2);
				}
			}

		}
	}
}

#include "comms/zip.h"

class FLAFileHandler : public FileHandler {
	const char*		GetExt() override { return "fla"; }
	const char*		GetDescription() override { return "Flash Document"; }

	int				Check(istream_ref file) override {
		ZIPreader	zip(file);
		ZIPfile		zf;
		do {
			if (!zip.Next(zf))
				return CHECK_DEFINITE_NO;
		} while (zf.fn != "LIBRARY/");

		if (!zip.Next(zf) || zf.fn != "META-INF/")
			return CHECK_DEFINITE_NO;

		if (!zip.Next(zf) || zf.fn != "DOMDocument.xml")
			return CHECK_DEFINITE_NO;

		return CHECK_PROBABLE + 1;
	}

	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		FileHandler *zip = Get("zip");
		if (!zip)
			return ISO_NULL;

		ISO_ptr<anything>	p = zip->Read(id, file);
		if (!p)
			return ISO_NULL;

		ISO::Browser			b(p);
		ISO::Browser2			b2	= *b["DOMDocument.xml"];
		ISO_openarray<uint8>	doc = b2.Get();
		memory_reader				mi(memory_block(doc, doc.Count()));
		ISO_ptr<anything>		lib2(0);
		FLAreader				fla(mi, b["LIBRARY"], *lib2);

		ISO_ptr<flash::File>	flash(id);
		flash->movie.Append(fla.root);
		flash->background	= (float4)w_axis;
		flash->rect			= float4{0, 0, 1280, 720};
		flash->framerate	= 24;

		return flash;
	}
} fla;


