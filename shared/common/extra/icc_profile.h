#ifndef ICC_PROFILE_H
#define ICC_PROFILE_H

#include "colour.h"
#include "base/array.h"

namespace iso {

//-----------------------------------------------------------------------------
// ICC_Profile
//-----------------------------------------------------------------------------

// see https://www.color.org/specification/ICC1v43_2010-12.pdf
struct ICC_Profile {
	enum PrimaryPlatform {
		Apple					= "APPL"_u32,
		Microsoft				= "MSFT"_u32,
		SiliconGraphics			= "SGI "_u32,
		SunMicrosystems			= "SUNW"_u32,
		Taligent				= "TGNT"_u32,
	};
	enum ProfileClass {
		ColorEncodingSpace		= "cenc"_u32,
		DeviceLink				= "link"_u32,
		MultiplexIdentification	= "mid "_u32,
		MultiplexLink			= "mlnk"_u32,
		DisplayDevice			= "mntr"_u32,
		MultiplexVisualization	= "mvis"_u32,
		NikonInputDevice		= "nkpf"_u32,
		NamedColor				= "nmcl"_u32,
		OutputDevice			= "prtr"_u32,
		InputDevice				= "scnr"_u32,
		ColorSpaceConversion	= "spac"_u32,
	};
	enum ColourSpace {
		nCIEXYZ					= "CSXY"_u32,
		CIELAB					= "Lab "_u32,
		CIELUV					= "Luv "_u32,
		YCbCr					= "YCbr"_u32,
		CIEYxy					= "Yxy "_u32,
		RGB						= "RGB "_u32,
		Gray					= "GRAY"_u32,
		HSV						= "HSV "_u32,
		HLS						= "HLS "_u32,
		CMYK					= "CMYK"_u32,
		CMY						= "CMY "_u32,
		colour2					= "2CLR"_u32,
		colour3					= "3CLR"_u32,
		colour4					= "4CLR"_u32,
		colour5					= "5CLR"_u32,
		colour6					= "6CLR"_u32,
		colour7					= "7CLR"_u32,
		colour8					= "8CLR"_u32,
		colour9					= "9CLR"_u32,
		colour10				= "ACLR"_u32,
		colour11				= "BCLR"_u32,
		colour12				= "CCLR"_u32,
		colour13				= "DCLR"_u32,
		colour14				= "ECLR"_u32,
		colour15				= "FCLR"_u32,
	};
	enum RenderingIntent {
		RI_Perceptual					= 0,
		RI_MediaRelativeColorimetric	= 1,
		RI_Saturation					= 2,
		RI_ICCAbsoluteColorimetric		= 3,
	};
	enum ChromaticityColorant {
		COL_Unknown						= 0,
		COL_ITU_R_BT709					= 1,
		COL_SMPTE_RP145_1994			= 2,
		COL_EBU_Tech_3213_E				= 3,
		COL_P22							= 4,
	};

	//basic types
	struct date_time {
		uint16be	year;
		uint16be	month;
		uint16be	day;
		uint16be	hours;
		uint16be	minutes;
		uint16be	seconds;
	};
	struct u16fixed16 :	endian_t<ufixed<16, 16>, true> {
		operator float() const { return get(); }
	};
	struct s15fixed16 :	endian_t<fixed<16, 16>, true> {
		operator float() const { return get(); }
	};
	struct XY : array<s15fixed16, 2> {
	};
	struct XYZ : array<s15fixed16, 3> {
		operator colour_XYZ() const { return {(*this)[0], (*this)[1], (*this)[2]}; }
	};
	struct PCS : array<uint16be, 3> {};

	//tagged types
	struct tagged_entry {
		uint32		signature;
		uint32be	offset;	//Offset to beginning of tag data element
		uint32be	size;	//Size of tag data element
	};
	struct tagged {
		uint32		signature;
		uint32		reserved;
	};
	template<uint32 SIG> struct taggedT : tagged {
		bool	valid() const	{ return signature == SIG; }
	};
	template<uint32 SIG, typename T> struct tagged_array : taggedT<SIG> {
		T	values[];
	};

	typedef tagged_array<"text"_u32, char>			Text;
	typedef tagged_array<"ui08"_u32, uint8>			uint8Array;
	typedef tagged_array<"ui16"_u32, uint16be>		uint16Array;
	typedef tagged_array<"ui32"_u32, uint32be>		uint32Array;
	typedef tagged_array<"ui64"_u32, uint64be>		uint64Array;
	typedef tagged_array<"sf32"_u32, s15fixed16>	s15Fixed16Array;
	typedef tagged_array<"uf32"_u32, u16fixed16>	u16fixed16Array;
	typedef tagged_array<"XYZ "_u32, XYZ>			XYZArray;

	struct Signature : taggedT<"sig "_u32> {
		uint32		sig;
	};
	struct Data : taggedT<"data"_u32> {
		uint32be	flag;//0->ascii, 1->binary
		uint8		data[];
	};
	struct DateTime : taggedT<"dtim"_u32>, date_time {};

	struct Chromaticity : taggedT<"chrm"_u32> {
		uint16be	count;
		uint16be	colorant;
		XY			channels[];
	};
	struct ColorantOrder : taggedT<"clro"_u32> {
		uint32be	count;
		uint8		colorant[];
	};
	struct ColorantTable : tagged {
		struct entry {
			char	name[32];
			PCS		pcs;
		};
		uint32be	count;
		entry		entries[];
	};
	struct Curve: taggedT<"curv"_u32> {
		uint32be	count;		//if 0, identity; if 1, it's a 1 byte gamma
		unorm16be	entries[];
	};
	struct ParametricCurve: taggedT<"para"_u32> {
		enum FUNC : uint16 {//	fn				if			else
			POW,			//	X^g
			LIN_POW,		//	(aX+b)^g		X>=-b/a		0
			LIN_POW_OFF,	//	(aX+b)^g+c		X>=-b/a		c
			LIN_POW2,		//	(aX+b)^g		X>=d		cX
			LIN_POW_OFF2	//	(aX+b)^g+c		X>=d		cX+f
		};
		BE(FUNC)	func;
		uint16		reserved;
		s15fixed16	g, a, b, c, d, e, f;// gabcdef	s15Fixed16Number [...]
		auto		apply(float X) {
			switch (func) {
				case POW:			return pow(X, g);
				case LIN_POW:		return X >= -b / a	? pow(a * X + b, g) 		: 0;
				case LIN_POW_OFF:	return X >= -b / a	? pow(a * X + b, g) + c		: c;
				case LIN_POW2:		return X >= d		? pow(a * X + b, g) 		: c * X;
				case LIN_POW_OFF2:	return X >= d		? pow(a * X + b, g) + c		: c * X + f;
			}
		}
	};
	struct Lut16  : taggedT<"mft2"_u32> {// multi-function table with 2-byte precision
		uint8		num_input;
		uint8		num_output;
		uint8		num_grid_points;//	identical for each side)
		uint8		reserved;
		s15fixed16	matrix[3][3];		//row major
		uint16be	num_input_entries;
		uint16be	num_output_entries;
		uint16be	tables[];
		//uint16be	input_tables[];		//num_input_entries * num_input
		//uint16be	clut[];				//num_grid_points^num_input * num_output
		//uint16be	output_tables[];	//num_output_entries * num_output

		auto		input_tables()	const	{ return make_range_n(tables, num_input_entries * num_input); }
		auto		clut()			const	{ return make_range_n(input_tables().end(), pow(num_grid_points, num_input) * num_output); }
		auto		output_tables()	const	{ return make_range_n(clut().end(), num_output_entries * num_output); }
	};
	struct Lut8  : taggedT<"mft1"_u32> {// multi-function table with 1-byte precision
		uint8		num_input;
		uint8		num_output;
		uint8		num_grid_points;//	identical for each side)
		uint8		reserved;
		s15fixed16	matrix[3][3];		//row major
		uint16be	tables[];
		//uint16be	input_tables[];		//num_input * 256
		//uint16be	clut[];				//num_grid_points^num_input * num_output
		//uint16be	output_tables[];	//num_output * 256

		auto		input_tables()	const	{ return make_range_n(tables, num_input * 256); }
		auto		clut()			const	{ return make_range_n(input_tables().end(), pow(num_grid_points, num_input) * num_output); }
		auto		output_tables()	const	{ return make_range_n(clut().end(), num_output * 256); }
	};
	struct Measurement : taggedT<"meas"_u32> {
		enum Observer {
			CIE_1931		= 1,
			CIE_1964		= 2,
		};
		enum Geometry {
			GEOM_Unknown	= 0,
			GEOM_45			= 1,
			GEOM_d			= 2,
		};
		enum lluminant {
			ILLUM_D50		= 1,
			ILLUM_D65		= 2,
			ILLUM_D93		= 3,
			ILLUM_F2		= 4,
			ILLUM_D55		= 5,
			ILLUM_A			= 6,
			ILLUM_EquiPower	= 7,
			ILLUM_F8		= 8,
		};
		uint32be	Observer;
		uint32be	Backing[3];
		uint32be	Geometry;
		u16fixed16	Flare;
		uint32be	Illuminant;
	};

	struct header {
		uint32be			size;
		uint32				cmmtype;
		uint32be			version;
		ProfileClass		profileclass;
		ColourSpace			colourspace;
		ColourSpace			connectionspace;
		date_time			creation;
		uint32				signature;					//'acsp'
		PrimaryPlatform		primaryplatform;
		uint32be			cmmflags;
		uint32be			devicemanufacturer;
		uint32be			devicemodel;
		uint64be			deviceattributes;
		BE(RenderingIntent)	renderingintent;
		XYZ					connectionspace_illuminant;
		uint32be			creator;
		uint32be			id[4];
		uint8				reserved[28];
	};

	header			h;
	uint32be		tag_count;
	tagged_entry	tags[];

	auto	entries()	const {
		return make_range_n(tags, tag_count);
	}
	checked_memory_block	find(uint32 sig) const {
		for (auto &i : entries()) {
			if (i.signature == sig)
				return {(const uint8*)this + i.offset, i.size};
		}
		return none;
	}
	template<typename T> const T* findT(uint32 sig) const {
		const T	*p = find(sig);
		ISO_ASSERT(!p || p->valid());
		return p;
	}
};

static_assert(sizeof(ICC_Profile::header) == 128, "oops");

} // namespace iso

#endif // ICC_PROFILE_H
