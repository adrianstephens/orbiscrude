#ifndef FLASH_H
#define FLASH_H

#include "filetypes/bitmap/bitmap.h"

namespace iso {
namespace flash {

typedef pair<ISO_ptr<void>, ISO_openarray<float2p> >	Shape;
typedef pair<ISO_ptr<bitmap>, float2x3p>				Bitmap;
typedef float4p											Solid;

struct Gradient {
	enum FLAGS {
		radial			= 1 << 0,
		linear_interp	= 1 << 4,
		reflect			= 1 << 6,
		repeat			= 1 << 7,
	};
	typedef	pair<float, float4p>	entry;
	uint32					flags;
	float2x3p				matrix;
	float					focal_point;
	ISO_openarray<entry>	entries;

	Gradient() {
		clear(*this);
		matrix.x.x = matrix.y.y = 1;
	}
};

struct Object {
	enum BLEND {
		normal		= 0,/// or 1,
		layer		= 2,
		multiply	= 3,
		screen		= 4,
		lighten		= 5,
		darken		= 6,
		difference	= 7,
		add			= 8,
		subtract	= 9,
		invert		= 10,
		alpha		= 11,
		erase		= 12,
		overlay		= 13,
		hardlight	= 14,

		//buttons:
		up			= 1 << 8,
		over		= 1 << 9,
		down		= 1 << 10,
		hittest		= 1 << 11,
	};

	ISO_ptr<void>			character;
	float2x3p				trans;
	simple_vec<float4p,2>	col_trans;
	float					morph_ratio;
	uint32					clip_depth;
	xint32					flags;
	anything				filters;

	Object() {
		clear(*this);
		trans.x.x = trans.y.y = 1;
		col_trans.x = float4p{1,1,1,1};
	}
};

struct Text {
	float4p					bounds;
	float4p					colour;
	ISO_ptr<void>			font;
	float					size;
	uint8					align;
	float4p					margins;
	string					text;
};

struct Frame : ISO_openarray<ISO_ptr<Object> >	{};
struct Movie : ISO_openarray<ISO_ptr<Frame> >	{};
struct File {
	float4p					rect;
	float4p					background;
	float					framerate;
	Movie					movie;
};
}	// namespace flash
}	// namespace iso

ISO_DEFUSERCOMPV(iso::flash::Gradient, flags, matrix, focal_point, entries);
ISO_DEFUSERCOMPV(iso::flash::Object, character, trans, col_trans, morph_ratio, clip_depth, flags, filters);
ISO_DEFUSERCOMPV(iso::flash::Text, bounds, colour, font, size, align, margins, text);
ISO_DEFUSER(iso::flash::Frame, ISO_openarray<ISO_ptr<flash::Object> >);
ISO_DEFUSER(iso::flash::Movie, ISO_openarray<ISO_ptr<flash::Frame> >);
ISO_DEFUSERCOMPXV(iso::flash::File, "FlashFile", rect, background, framerate, movie);


#endif