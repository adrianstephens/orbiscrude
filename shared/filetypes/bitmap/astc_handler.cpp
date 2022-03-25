#include "base/vector.h"
#include "codec/texels/astc.h"
#include "iso/iso_files.h"
#include "iso/iso_convert.h"

using namespace iso;

class ASTCFileHandler : FileHandler {
	const char*		GetExt() override { return "astc"; }
	ISO_ptr<void>	Read(tag id, istream_ref file) override;
	bool			Write(ISO_ptr<void> p, ostream_ref file) override;
	bool	WriteLDR(ISO_ptr<bitmap> p, ostream_ref file);
	bool	WriteHDR(ISO_ptr<HDRbitmap> p, ostream_ref file);
} astc;

struct astc_header {
	enum {MAGIC_FILE_CONSTANT = 0x5CA1AB13};
	uint32le		magic;
	uint8			xdim,	ydim,	zdim;
	uintn<3, false>	xsize,	ysize,	zsize;

	bool	valid() const {
		return magic == MAGIC_FILE_CONSTANT
			&& xdim >= 3 && xdim <= 12
			&& ydim >= 3 && ydim <= 12
			&& (zdim >= 3 || zdim == 1) && zdim <= 12;
	}
};


ISO_ptr<void> ASTCFileHandler::Read(tag id, istream_ref file) {
	astc_header h;
	if (!file.read(h) || !h.valid())
		return ISO_NULL;

	uint32	xsize = h.xsize;
	uint32	ysize = h.ysize;
	uint32	zsize = h.zsize;

	uint32	xblocks = div_round_up(xsize, h.xdim);
	uint32	yblocks = div_round_up(ysize, h.ydim);
	uint32	zblocks = div_round_up(zsize, h.zdim);

	ISO_ptr<bitmap>	p(id);
	p->Create(xsize, ysize, 0, zsize);
	block<ISO_rgba, 3>	dest	= p->All3D();

	for (int z = 0; z < zblocks; z++) {
		for (int y = 0; y < yblocks; y++) {
			for (int x = 0; x < xblocks; x++)
				file.get<ASTC>().Decode(h.xdim, h.ydim, h.zdim, dest.template sub<1>(x * h.xdim, h.xdim).template sub<2>(y * h.ydim, h.ydim).template sub<3>(z * h.zdim, h.zdim));
		}
	}

	return p;
}

bool ASTCFileHandler::Write(ISO_ptr<void> p, ostream_ref file) {
	ISO_ptr<bitmap2> p2 = ISO_conversion::convert<bitmap2>(FileHandler::ExpandExternals(p), ISO_conversion::RECURSE | ISO_conversion::CHECK_INSIDE);
	return p2 && (p2->IsType<bitmap>() ? WriteLDR(*p2, file) : WriteHDR(*p2, file));
}

bool ASTCFileHandler::WriteLDR(ISO_ptr<bitmap> p, ostream_ref file) {
	uint32	xsize	= p->Width();
	uint32	ysize	= p->BaseHeight();
	uint32	zsize	= p->Depth();

	astc_header	h;

	h.magic	= h.MAGIC_FILE_CONSTANT;
	h.xsize	= xsize;
	h.ysize	= ysize;
	h.zsize	= zsize;
	h.xdim	= 4;
	h.ydim	= 4;
	h.zdim	= 1;

	file.write(h);

	uint32	xblocks = div_round_up(xsize, h.xdim);
	uint32	yblocks = div_round_up(ysize, h.ydim);
	uint32	zblocks = div_round_up(zsize, h.zdim);

	block<ISO_rgba, 3>	srce	= p->All3D();
	CompressionParams	params;

	for (int z = 0; z < zblocks; z++) {
		for (int y = 0; y < yblocks; y++) {
			for (int x = 0; x < xblocks; x++) {
				ASTC	astc;
				astc.Encode(h.xdim, h.ydim, h.zdim, srce.template sub<1>(x * h.xdim, h.xdim).template sub<2>(y * h.ydim, h.ydim).template sub<3>(z * h.zdim, h.zdim), &params);
				file.write(astc);
			}
		}
	}
	return true;
}

bool ASTCFileHandler::WriteHDR(ISO_ptr<HDRbitmap> p, ostream_ref file) {
	return false;
}


#if 0
enum roundmode {
	SF_UP = 0,				/* round towards positive infinity */
	SF_DOWN = 1,			/* round towards negative infinity */
	SF_TOZERO = 2,			/* round towards zero */
	SF_NEARESTEVEN = 3,		/* round toward nearest value; if mid-between, round to even value */
	SF_NEARESTAWAY = 4		/* round toward nearest value; if mid-between, round away from zero */
};

enum astc_decode_mode {
	DECODE_LDR_SRGB,
	DECODE_LDR,
	DECODE_HDR
};

// the entries here : 0=red, 1=green, 2=blue, 3=alpha, 4=0.0, 5=1.0
struct swizzlepattern {
	uint8 r, g, b, a;
};

#define MAX_TEXELS_PER_BLOCK		216
#define MAX_WEIGHTS_PER_BLOCK 64

int rgb_force_use_of_hdr = 0;
int alpha_force_use_of_hdr = 0;

uint16 float_to_sf16(float f, roundmode rm) {
	float16	h = f;
	return (uint16&)h;
}
float sf16_to_float(uint16 p) {
	return (float16&)p;
}

struct symbolic_compressed_block {
	int		error_block;			// 1 marks error block, 0 marks non-error-block.
	int		block_mode;				// 0 to 2047. Negative value marks constant-color block (-1: FP16, -2:UINT16)
	int		partition_count;		// 1 to 4; Zero marks a constant-color block.
	int		partition_index;		// 0 to 1023
	int		color_formats[4];		// color format for each endpoint color pair.
	int		color_formats_matched;	// color format for all endpoint pairs are matched.
	int		color_values[4][12];	// quantized endpoint color pairs.
	int		color_quantization_level;
	uint8	plane1_weights[MAX_WEIGHTS_PER_BLOCK];	// quantized and decimated weights
	uint8	plane2_weights[MAX_WEIGHTS_PER_BLOCK];
	int		plane2_color_component;	// color component for the secondary plane of weights
	int		constant_color[4];		// constant-color, as FP16 or UINT16. Used for constant-color blocks only.
};

struct physical_compressed_block {
	uint8 data[16];
};

struct imageblock {
	float	orig_data[MAX_TEXELS_PER_BLOCK * 4];	// original input data
	float	work_data[MAX_TEXELS_PER_BLOCK * 4];	// the data that we will compress, either linear or LNS (0..65535 in both cases)
	float	deriv_data[MAX_TEXELS_PER_BLOCK * 4];	// derivative of the conversion function used, used ot modify error weighting

	bool	rgb_lns[MAX_TEXELS_PER_BLOCK * 4];		// 1 if RGB data are being trated as LNS
	bool	alpha_lns[MAX_TEXELS_PER_BLOCK * 4];	// 1 if Alpha data are being trated as LNS
	bool	nan_texel[MAX_TEXELS_PER_BLOCK * 4];	// 1 if the texel is a NaN-texel.

	float	red_min, red_max;
	float	green_min, green_max;
	float	blue_min, blue_max;
	float	alpha_min, alpha_max;
	bool	grayscale;

	int		xpos, ypos, zpos;

	void update_flags(int xdim, int ydim, int zdim) {
		red_min		= 1e38f; red_max	= -1e38f;
		green_min	= 1e38f; green_max	= -1e38f;
		blue_min	= 1e38f; blue_max	= -1e38f;
		alpha_min	= 1e38f; alpha_max	= -1e38f;
		grayscale	= true;

		for (int i = 0, n = xdim * ydim * zdim; i < n; i++) {
			float red	= work_data[4 * i + 0];
			float green = work_data[4 * i + 1];
			float blue	= work_data[4 * i + 2];
			float alpha = work_data[4 * i + 3];
			if (red < red_min)
				red_min = red;
			if (red > red_max)
				red_max = red;
			if (green < green_min)
				green_min = green;
			if (green > green_max)
				green_max = green;
			if (blue < blue_min)
				blue_min = blue;
			if (blue > blue_max)
				blue_max = blue;
			if (alpha < alpha_min)
				alpha_min = alpha;
			if (alpha > alpha_max)
				alpha_max = alpha;

			grayscale = grayscale && red == green && red == blue;
		}
	}
};

typedef float4 double4;

struct astc_codec_image {
	uint8	***imagedata8;
	uint16	***imagedata16;
	int		xsize, ysize, zsize, padding;
	astc_codec_image(int bitness, int xsize, int ysize, int zsize, int padding);
	~astc_codec_image();
	void initialize_image();
	void fill_padding();
	void fetch_imageblock(imageblock * pb, int xdim, int ydim, int zdim, int xpos, int ypos, int zpos, swizzlepattern swz, bool srgb) const;
	void write_imageblock(const imageblock * pb, int xdim, int ydim, int zdim, int xpos, int ypos, int zpos, swizzlepattern swz, bool srgb) const;
};

astc_codec_image::~ astc_codec_image() {
	if (imagedata8) {
		delete[] imagedata8[0][0];
		delete[] imagedata8[0];
		delete[] imagedata8;
	}
	if (imagedata16) {
		delete[] imagedata16[0][0];
		delete[] imagedata16[0];
		delete[] imagedata16;
	}
}

astc_codec_image::astc_codec_image(int bitness, int _xsize, int _ysize, int _zsize, int _padding) : xsize(_xsize), ysize(_ysize), zsize(_zsize), padding(_padding) {
	int exsize = xsize + 2 * padding;
	int eysize = ysize + 2 * padding;
	int ezsize = zsize == 1 ? 1 : zsize + 2 * padding;

	if (bitness == 8) {
		imagedata8			= new uint8 **[ezsize];
		imagedata8[0]		= new uint8 *[ezsize * eysize];
		imagedata8[0][0]	= new uint8[4 * ezsize * eysize * exsize];
		for (int i = 1; i < ezsize; i++) {
			imagedata8[i]		= imagedata8[0] + i * eysize;
			imagedata8[i][0]	= imagedata8[0][0] + 4 * i * exsize * eysize;
		}
		for (int i = 0; i < ezsize; i++)
			for (int j = 1; j < eysize; j++)
				imagedata8[i][j] = imagedata8[i][0] + 4 * j * exsize;

		imagedata16 = NULL;
	} else if (bitness == 16) {
		imagedata16			= new uint16 **[ezsize];
		imagedata16[0]		= new uint16 *[ezsize * eysize];
		imagedata16[0][0]	= new uint16[4 * ezsize * eysize * exsize];
		for (int i = 1; i < ezsize; i++) {
			imagedata16[i] = imagedata16[0] + i * eysize;
			imagedata16[i][0] = imagedata16[0][0] + 4 * i * exsize * eysize;
		}
		for (int i = 0; i < ezsize; i++)
			for (int j = 1; j < eysize; j++)
				imagedata16[i][j] = imagedata16[i][0] + 4 * j * exsize;

		imagedata8 = NULL;
	}
}

void astc_codec_image::initialize_image() {
	int exsize = xsize + 2 * padding;
	int eysize = ysize + 2 * padding;
	int ezsize = (zsize == 1) ? 1 : zsize + 2 * padding;

	if (imagedata8) {
		for (int z = 0; z < ezsize; z++)
			for (int y = 0; y < eysize; y++)
				for (int x = 0; x < exsize; x++) {
					imagedata8[z][y][4 * x] = 0;
					imagedata8[z][y][4 * x + 1] = 0;
					imagedata8[z][y][4 * x + 2] = 0;
					imagedata8[z][y][4 * x + 3] = 0xFF;
				}
	} else if (imagedata16) {
		for (int z = 0; z < ezsize; z++)
			for (int y = 0; y < eysize; y++)
				for (int x = 0; x < exsize; x++) {
					imagedata16[z][y][4 * x] = 0;
					imagedata16[z][y][4 * x + 1] = 0;
					imagedata16[z][y][4 * x + 2] = 0;
					imagedata16[z][y][4 * x + 3] = 0x3C00;
				}
	}
}

void astc_codec_image::fill_padding() {
	if (padding == 0)
		return;

	int x, y, z, i;
	int exsize = xsize + 2 * padding;
	int eysize = ysize + 2 * padding;
	int ezsize = (zsize == 1) ? 1 : (zsize + 2 * padding);

	int xmin = padding;
	int ymin = padding;
	int zmin = (zsize == 1) ? 0 : padding;
	int xmax = xsize + padding - 1;
	int ymax = ysize + padding - 1;
	int zmax = zsize == 1 ? 0 : zsize + padding - 1;

	if (imagedata8) {
		for (z = 0; z < ezsize; z++) {
			int zc = min(max(z, zmin), zmax);
			for (y = 0; y < eysize; y++) {
				int yc = min(max(y, ymin), ymax);
				for (x = 0; x < exsize; x++) {
					int xc = min(max(x, xmin), xmax);
					for (i = 0; i < 4; i++)
						imagedata8[z][y][4 * x + i] = imagedata8[zc][yc][4 * xc + i];
				}
			}
		}
	} else if (imagedata16) {
		for (z = 0; z < ezsize; z++) {
			int zc = min(max(z, zmin), zmax);
			for (y = 0; y < eysize; y++) {
				int yc = min(max(y, ymin), ymax);
				for (x = 0; x < exsize; x++) {
					int xc = min(max(x, xmin), xmax);
					for (i = 0; i < 4; i++)
						imagedata16[z][y][4 * x + i] = imagedata16[zc][yc][4 * xc + i];
				}
			}
		}
	}
}

// conversion functions between the LNS representation and the FP16 representation.
float float_to_lns(float p) {
	if (is_nan(p) || p <= 1.0f / 67108864.0f)
		return 0;

	if (abs(p) >= 65536.0f)
		return 65535;

	iorf	t(p);
	int		e	= t.get_exp();
	if (e < -13) {
		p	= p * 33554432.0f;
		e	= 0;
	} else {
		e	+= 14;
		p	= (t.get_mant() - 0.5f) * 4096.0f;
	}

	return (p < 384.0f ? p * 4.0f / 3.0f : p <= 1408.0f ? p + 128.0f :  (p + 512.0f) * (4.0f / 5.0f))
		+ e * 2048.0f + 1.0f;
}

uint16 lns_to_sf16(uint16 p) {
	uint16 mc	= p & 0x7FF;
	uint16 ec	= p >> 11;
	uint16 mt	= mc < 512 ? 3 * mc : mc < 1536 ? 4 * mc - 512 : 5 * mc - 2048;
	return min((ec << 10) | (mt >> 3), 0x7BFF);
}

uint16 unorm16_to_sf16(uint16 p) {
	if (p == 0xFFFF)
		return 0x3C00;			// value of 1.0
	if (p < 4)
		return p << 8;

	int lz = 16 - highest_set_index(p);
	return ((p << lz) >> 6) | ((15 - lz) << 10);
}

uint16 unorm16_to_sf16(uint16 u, bool lns) {
	return lns ? lns_to_sf16(u) : unorm16_to_sf16(u);
}

void imageblock_initialize_deriv_from_work_and_orig(imageblock * pb, int pixelcount) {
	const float *fptr = pb->orig_data;
	const float *wptr = pb->work_data;
	float *dptr = pb->deriv_data;

	for (int i = 0; i < pixelcount; i++) {
		// compute derivatives for RGB first
		if (pb->rgb_lns[i]) {
			float r = max(fptr[0], 6e-5f);
			float g = max(fptr[1], 6e-5f);
			float b = max(fptr[2], 6e-5f);

			float rderiv = (float_to_lns(r * 1.05f) - float_to_lns(r)) / (r * 0.05f);
			float gderiv = (float_to_lns(g * 1.05f) - float_to_lns(g)) / (g * 0.05f);
			float bderiv = (float_to_lns(b * 1.05f) - float_to_lns(b)) / (b * 0.05f);

			// the derivative may not actually take values smaller than 1/32 or larger than 2^25;
			// if it does, we clamp it.
			if (rderiv < (1.0f / 32.0f))
				rderiv = (1.0f / 32.0f);
			else if (rderiv > 33554432.0f)
				rderiv = 33554432.0f;

			if (gderiv < (1.0f / 32.0f))
				gderiv = (1.0f / 32.0f);
			else if (gderiv > 33554432.0f)
				gderiv = 33554432.0f;

			if (bderiv < (1.0f / 32.0f))
				bderiv = (1.0f / 32.0f);
			else if (bderiv > 33554432.0f)
				bderiv = 33554432.0f;

			dptr[0] = rderiv;
			dptr[1] = gderiv;
			dptr[2] = bderiv;
		} else {
			dptr[0] = 65535.0f;
			dptr[1] = 65535.0f;
			dptr[2] = 65535.0f;
		}

		// then compute derivatives for Alpha
		if (pb->alpha_lns[i]) {
			float a = max(fptr[3], 6e-5f);
			float aderiv = (float_to_lns(a * 1.05f) - float_to_lns(a)) / (a * 0.05f);
			// the derivative may not actually take values smaller than 1/32 or larger than 2^25; if it does, we clamp it.
			if (aderiv < (1.0f / 32.0f))
				aderiv = (1.0f / 32.0f);
			else if (aderiv > 33554432.0f)
				aderiv = 33554432.0f;

			dptr[3] = aderiv;
		} else {
			dptr[3] = 65535.0f;
		}

		fptr += 4;
		wptr += 4;
		dptr += 4;
	}
}

// helper function to initialize the work-data from the orig-data
void imageblock_initialize_work_from_orig(imageblock * pb, int pixelcount) {
	float *fptr = pb->orig_data;
	float *wptr = pb->work_data;

	for (int i = 0; i < pixelcount; i++) {
		if (pb->rgb_lns[i]) {
			wptr[0] = float_to_lns(fptr[0]);
			wptr[1] = float_to_lns(fptr[1]);
			wptr[2] = float_to_lns(fptr[2]);
		} else {
			wptr[0] = fptr[0] * 65535.0f;
			wptr[1] = fptr[1] * 65535.0f;
			wptr[2] = fptr[2] * 65535.0f;
		}

		if (pb->alpha_lns[i]) {
			wptr[3] = float_to_lns(fptr[3]);
		} else {
			wptr[3] = fptr[3] * 65535.0f;
		}
		fptr += 4;
		wptr += 4;
	}
	imageblock_initialize_deriv_from_work_and_orig(pb, pixelcount);
}

// helper function to initialize the orig-data from the work-data
void imageblock_initialize_orig_from_work(imageblock * pb, int pixelcount) {
	float *fptr = pb->orig_data;
	float *wptr = pb->work_data;

	for (int i = 0; i < pixelcount; i++) {
		fptr[0] = sf16_to_float(unorm16_to_sf16((uint16)wptr[0], pb->rgb_lns[i]));
		fptr[1] = sf16_to_float(unorm16_to_sf16((uint16)wptr[1], pb->rgb_lns[i]));
		fptr[2] = sf16_to_float(unorm16_to_sf16((uint16)wptr[2], pb->rgb_lns[i]));
		fptr[3] = sf16_to_float(unorm16_to_sf16((uint16)wptr[3], pb->alpha_lns[i]));
		fptr += 4;
		wptr += 4;
	}

	imageblock_initialize_deriv_from_work_and_orig(pb, pixelcount);
}

float srgb_to_linear(float r) {
	return	r <= 0.04045f	? r / 12.92f
		:	r <= 1			?  pow((r + 0.055f) / 1.055f, 2.4f)
		:	r;
}

float linear_to_srgb(float r) {
	return r <= 0.0031308f	? r * 12.92f
		:	r <= 1			? 1.055f * pow(r, (1.0f / 2.4f)) - 0.055f
		:	r;
}

// fetch an imageblock from the input file.
void astc_codec_image::fetch_imageblock(imageblock * pb, int xdim, int ydim, int zdim, int xpos, int ypos, int zpos, swizzlepattern swz, bool srgb) const {
	float	*fptr = pb->orig_data;
	int		xsize = this->xsize + 2 * padding;
	int		ysize = this->ysize + 2 * padding;
	int		zsize = this->zsize == 1 ? 1 : this->zsize + 2 * padding;

	pb->xpos = xpos;
	pb->ypos = ypos;
	pb->zpos = zpos;

	xpos += padding;
	ypos += padding;
	if (zsize > 1)
		zpos += padding;

	float data[6];
	data[4] = 0;
	data[5] = 1;

	if (imagedata8) {
		for (int z = 0; z < zdim; z++) {
			for (int y = 0; y < ydim; y++) {
				for (int x = 0; x < xdim; x++) {
					int xi = clamp(xpos + x, 0, xsize - 1);
					int yi = clamp(ypos + y, 0, ysize - 1);
					int zi = clamp(zpos + z, 0, zsize - 1);

					data[0] = imagedata8[zi][yi][4 * xi + 0] / 255.0f;
					data[1] = imagedata8[zi][yi][4 * xi + 1] / 255.0f;
					data[2] = imagedata8[zi][yi][4 * xi + 2] / 255.0f;
					data[3] = imagedata8[zi][yi][4 * xi + 3] / 255.0f;

					fptr[0] = data[swz.r];
					fptr[1] = data[swz.g];
					fptr[2] = data[swz.b];
					fptr[3] = data[swz.a];
					fptr += 4;
				}
			}
		}
	} else if (imagedata16) {
		for (int z = 0; z < zdim; z++) {
			for (int y = 0; y < ydim; y++) {
				for (int x = 0; x < xdim; x++) {
					int xi = clamp(xpos + x, 0, xsize - 1);
					int yi = clamp(ypos + y, 0, ysize - 1);
					int zi = clamp(zpos + z, 0, zsize - 1);

					data[0] = max(sf16_to_float(imagedata16[zi][yi][4 * xi + 0]), 1e-8f);
					data[1] = max(sf16_to_float(imagedata16[zi][yi][4 * xi + 1]), 1e-8f);
					data[2] = max(sf16_to_float(imagedata16[zi][yi][4 * xi + 2]), 1e-8f);
					data[3] = max(sf16_to_float(imagedata16[zi][yi][4 * xi + 3]), 1e-8f);

					fptr[0] = data[swz.r];
					fptr[1] = data[swz.g];
					fptr[2] = data[swz.b];
					fptr[3] = data[swz.a];
					fptr += 4;
				}
			}
		}
	}

	int pixelcount = xdim * ydim * zdim;

	// perform sRGB-to-linear transform on input data, if requested.
	if (srgb) {
		fptr = pb->orig_data;
		for (int i = 0; i < pixelcount; i++) {
			fptr[0] = srgb_to_linear(fptr[0]);
			fptr[1] = srgb_to_linear(fptr[1]);
			fptr[2] = srgb_to_linear(fptr[2]);
			fptr += 4;
		}
	}

	// collect color max-value, in order to determine whether to use LDR or HDR interpolation.
	float max_red = 0, max_green = 0, max_blue = 0, max_alpha = 0;
	fptr = pb->orig_data;
	for (int i = 0; i < pixelcount; i++) {
		max_red		= max(fptr[0], max_red);
		max_green	= max(fptr[1], max_green);
		max_blue	= max(fptr[2], max_blue);
		max_alpha	= max(fptr[3], max_alpha);
		fptr += 4;
	}

	float max_rgb = max(max_red, max(max_green, max_blue));

	// use LNS if:
	// * RGB-maximum is less than 0.15
	// * RGB-maximum is greater than 1
	// * Alpha-maximum is greater than 1
	bool rgb_lns	= max_rgb < 0.15f || max_rgb > 1.0f || max_alpha > 1.0f;
	bool alpha_lns	= rgb_lns && (max_alpha > 1.0f || max_alpha < 0.15f);

	// not yet though; for the time being, just obey the commandline.
	//rgb_lns		= rgb_force_use_of_hdr;
	//alpha_lns	= alpha_force_use_of_hdr;

	// impose the choice on every pixel when encoding.
	for (int i = 0; i < pixelcount; i++) {
		pb->rgb_lns[i]		= rgb_lns;
		pb->alpha_lns[i]	= alpha_lns;
		pb->nan_texel[i]	= false;
	}
	imageblock_initialize_work_from_orig(pb, pixelcount);
	pb->update_flags(xdim, ydim, zdim);
}

void astc_codec_image::write_imageblock(const imageblock * pb, int xdim, int ydim, int zdim, int xpos, int ypos, int zpos, swizzlepattern swz, bool srgb) const {
	const float	*fptr = pb->orig_data;
	const bool	*nptr = pb->nan_texel;

	float data[7];
	data[4] = 0;
	data[5] = 1;

	if (imagedata8) {
		for (int z = 0; z < zdim; z++) {
			for (int y = 0; y < ydim; y++) {
				for (int x = 0; x < xdim; x++) {
					int xi = xpos + x;
					int yi = ypos + y;
					int zi = zpos + z;

					if (xi >= 0 && yi >= 0 && zi >= 0 && xi < xsize && yi < ysize && zi < zsize) {
						if (*nptr) {
							// NaN-pixel, but we can't display it. Display purple instead.
							imagedata8[zi][yi][4 * xi + 0] = 0xFF;
							imagedata8[zi][yi][4 * xi + 1] = 0x00;
							imagedata8[zi][yi][4 * xi + 2] = 0xFF;
							imagedata8[zi][yi][4 * xi + 3] = 0xFF;
						} else {
							// apply swizzle
							if (srgb) {
								data[0] = linear_to_srgb(fptr[0]);
								data[1] = linear_to_srgb(fptr[1]);
								data[2] = linear_to_srgb(fptr[2]);
							} else {
								data[0] = fptr[0];
								data[1] = fptr[1];
								data[2] = fptr[2];
							}
							data[3] = fptr[3];

							float x = data[0] * 2 - 1;
							float y = data[3] * 2 - 1;
							float z = max(1 - x * x - y * y, 0);
							data[6] = sqrt(z) * 0.5f + 0.5f;

							imagedata8[zi][yi][4 * xi + 0] = static_cast <int>(floor(min(data[swz.r], 1) * 255.0f + 0.5f));
							imagedata8[zi][yi][4 * xi + 1] = static_cast <int>(floor(min(data[swz.g], 1) * 255.0f + 0.5f));
							imagedata8[zi][yi][4 * xi + 2] = static_cast <int>(floor(min(data[swz.b], 1) * 255.0f + 0.5f));
							imagedata8[zi][yi][4 * xi + 3] = static_cast <int>(floor(min(data[swz.a], 1) * 255.0f + 0.5f));
						}
					}
					fptr += 4;
					nptr++;
				}
			}
		}
	} else if (imagedata16) {
		for (int z = 0; z < zdim; z++) {
			for (int y = 0; y < ydim; y++) {
				for (int x = 0; x < xdim; x++) {
					int xi = xpos + x;
					int yi = ypos + y;
					int zi = zpos + z;

					if (xi >= 0 && yi >= 0 && zi >= 0 && xi < xsize && yi < ysize && zi < zsize) {
						if (*nptr) {
							imagedata16[zi][yi][4 * xi + 0] = 0xFFFF;
							imagedata16[zi][yi][4 * xi + 1] = 0xFFFF;
							imagedata16[zi][yi][4 * xi + 2] = 0xFFFF;
							imagedata16[zi][yi][4 * xi + 3] = 0xFFFF;
						} else {
							// apply swizzle
							if (srgb) {
								data[0] = linear_to_srgb(fptr[0]);
								data[1] = linear_to_srgb(fptr[1]);
								data[2] = linear_to_srgb(fptr[2]);
							} else {
								data[0] = fptr[0];
								data[1] = fptr[1];
								data[2] = fptr[2];
							}
							data[3] = fptr[3];

							float x = data[0] * 2 - 1;
							float y = data[3] * 2 - 1;
							float z = max(1 - x * x - y * y, 0);
							data[6] = sqrt(z) * 0.5f + 0.5f;

							imagedata16[zi][yi][4 * xi + 0] = float_to_sf16(data[swz.r], SF_NEARESTEVEN);
							imagedata16[zi][yi][4 * xi + 1] = float_to_sf16(data[swz.g], SF_NEARESTEVEN);
							imagedata16[zi][yi][4 * xi + 2] = float_to_sf16(data[swz.b], SF_NEARESTEVEN);
							imagedata16[zi][yi][4 * xi + 3] = float_to_sf16(data[swz.a], SF_NEARESTEVEN);
						}
					}
					fptr += 4;
					nptr++;
				}
			}
		}
	}
}

// Helper functions for various error-metric calculations

double clampx(double p) {
	return is_nan(p) || p < 0.0f ? 0
		: p > 65504.0f ? 65504.0f
		: p;
}

// logarithm-function, linearized from 2^-14.
double xlog2(double p) {
	return p >= 0.00006103515625 ? log(p) * 1.44269504088896340735 : -15.44269504088896340735 + p * 23637.11554992477646609062;
}

// mPSNR tone-mapping operator
double mpsnr_operator(double v, int fstop) {
	double	x = pow(v * (double)(1LL << (fstop + 32)) / 4294967296.0, (1.0 / 2.2)) * 255;
	return is_nan(x) || x < 0	? 0.0
		:	x > 255				? 255.0
		:	x;
}

double mpsnr_sumdiff(double v1, double v2, int low_fstop, int high_fstop) {
	double summa = 0.0;
	for (int i = low_fstop; i <= high_fstop; i++)
		summa += square(mpsnr_operator(v1, i) - mpsnr_operator(v2, i));
	return summa;
}

#if 0
// Compute psnr and other error metrics between input and output image
void compute_error_metrics(int compute_hdr_error_metrics, int input_components, const astc_codec_image * img1, const astc_codec_image * img2, int low_fstop, int high_fstop, int psnrmode) {
	static const int channelmasks[5] ={ 0x00, 0x07, 0x0C, 0x07, 0x0F };
	int channelmask	= channelmasks[input_components];

	double4 errorsum(zero);
	double4 alpha_scaled_errorsum(zero);
	double4 log_errorsum(zero);
	double4 mpsnr_errorsum(zero);

	int xsize = min(img1->xsize, img2->xsize);
	int ysize = min(img1->ysize, img2->ysize);
	int zsize = min(img1->zsize, img2->zsize);

	int img1pad = img1->padding;
	int img2pad = img2->padding;

	double rgb_peak = 0.0f;

	for (int z = 0; z < zsize; z++) {
		for (int y = 0; y < ysize; y++) {
			int ze1 = img1->zsize == 1 ? z : z + img1pad;
			int ze2 = img2->zsize == 1 ? z : z + img2pad;

			int ye1 = y + img1pad;
			int ye2 = y + img2pad;

			for (int x = 0; x < xsize; x++) {
				double4 input_color1;
				double4 input_color2;

				int xe1 = 4 * x + 4 * img1pad;
				int xe2 = 4 * x + 4 * img2pad;

				if (img1->imagedata8) {
					input_color1 = double4(
						img1->imagedata8[ze1][ye1][xe1 + 0] / 255.0f,
						img1->imagedata8[ze1][ye1][xe1 + 1] / 255.0f,
						img1->imagedata8[ze1][ye1][xe1 + 2] / 255.0f,
						img1->imagedata8[ze1][ye1][xe1 + 3] / 255.0f
					);
				} else {
					input_color1 = double4(
						clampx(sf16_to_float(img1->imagedata16[ze1][ye1][xe1 + 0])),
						clampx(sf16_to_float(img1->imagedata16[ze1][ye1][xe1 + 1])),
						clampx(sf16_to_float(img1->imagedata16[ze1][ye1][xe1 + 2])),
						clampx(sf16_to_float(img1->imagedata16[ze1][ye1][xe1 + 3]))
					);
				}

				if (img2->imagedata8) {
					input_color2 = double4(
						img2->imagedata8[ze2][ye2][xe2 + 0] / 255.0f,
						img2->imagedata8[ze2][ye2][xe2 + 1] / 255.0f,
						img2->imagedata8[ze2][ye2][xe2 + 2] / 255.0f,
						img2->imagedata8[ze2][ye2][xe2 + 3] / 255.0f
					);
				} else {
					input_color2 = double4(
						clampx(sf16_to_float(img2->imagedata16[ze2][ye2][xe2 + 0])),
						clampx(sf16_to_float(img2->imagedata16[ze2][ye2][xe2 + 1])),
						clampx(sf16_to_float(img2->imagedata16[ze2][ye2][xe2 + 2])),
						clampx(sf16_to_float(img2->imagedata16[ze2][ye2][xe2 + 3]))
					);
				}

				rgb_peak = max(max(input_color1.x, input_color1.y), max(input_color1.z, rgb_peak));

				double4 diffcolor = input_color1 - input_color2;
				errorsum = errorsum + square(diffcolor);

				double4 alpha_scaled_diffcolor = double4(diffcolor.xyz * input_color1.w, diffcolor.w);
				alpha_scaled_errorsum = alpha_scaled_errorsum + alpha_scaled_diffcolor * alpha_scaled_diffcolor;

				if (compute_hdr_error_metrics) {
					double4 log_input_color1(
						xlog2(input_color1.x),
						xlog2(input_color1.y),
						xlog2(input_color1.z),
						xlog2(input_color1.w)
					);

					double4 log_input_color2(
						xlog2(input_color2.x),
						xlog2(input_color2.y),
						xlog2(input_color2.z),
						xlog2(input_color2.w)
					);

					log_errorsum += square(log_input_color1 - log_input_color2);

					mpsnr_errorsum += double4(
						mpsnr_sumdiff(input_color1.x, input_color2.x, low_fstop, high_fstop),
						mpsnr_sumdiff(input_color1.y, input_color2.y, low_fstop, high_fstop),
						mpsnr_sumdiff(input_color1.z, input_color2.z, low_fstop, high_fstop),
						mpsnr_sumdiff(input_color1.w, input_color2.w, low_fstop, high_fstop)
					);
				}
			}
		}
	}

	double pixels		= xsize * ysize * zsize;
	double num			= 0;
	double alpha_num	= 0;
	double log_num		= 0;
	double mpsnr_num	= 0;
	double samples		= 0;

	if (channelmask & 1) {
		num			+= errorsum.x;
		alpha_num	+= alpha_scaled_errorsum.x;
		log_num		+= log_errorsum.x;
		mpsnr_num	+= mpsnr_errorsum.x;
		samples		+= pixels;
	}
	if (channelmask & 2) {
		num			+= errorsum.y;
		alpha_num	+= alpha_scaled_errorsum.y;
		log_num		+= log_errorsum.y;
		mpsnr_num	+= mpsnr_errorsum.y;
		samples		+= pixels;
	}
	if (channelmask & 4) {
		num			+= errorsum.z;
		alpha_num	+= alpha_scaled_errorsum.z;
		log_num		+= log_errorsum.z;
		mpsnr_num	+= mpsnr_errorsum.z;
		samples		+= pixels;
	}
	if (channelmask & 8) {
		num			+= errorsum.w;
		alpha_num	+= alpha_scaled_errorsum.w;	/* log_num += log_errorsum.w; mpsnr_num += mpsnr_errorsum.w; */
		samples		+= pixels;
	}

	double denom	= samples;
	double mpsnr_denom = pixels * 3.0 * (high_fstop - low_fstop + 1) * 255.0f * 255.0f;

	double psnr		= num == 0 ? 999.0 : 10.0 * log10((double)denom / (double)num);
	double rgb_psnr = psnr;

	if (psnrmode == 1) {
		if (channelmask & 8) {
			double alpha_psnr;
			if (alpha_num == 0)
				alpha_psnr = 999.0;
			else
				alpha_psnr = 10.0 * log10((double)denom / (double)alpha_num);

			double rgb_num = errorsum.x + errorsum.y + errorsum.z;
			if (rgb_num == 0)
				rgb_psnr = 999.0;
			else
				rgb_psnr = 10.0 * log10((double)pixels * 3 / (double)rgb_num);
		}


		if (compute_hdr_error_metrics) {
			double mpsnr;
			if (mpsnr_num == 0)
				mpsnr = 999.0;
			else
				mpsnr = 10.0 * log10((double)mpsnr_denom / (double)mpsnr_num);

			double logrmse = sqrt((double)log_num / (double)pixels);
		}
	}
}
#endif
#endif