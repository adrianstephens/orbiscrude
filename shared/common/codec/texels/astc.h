#ifndef ASTC_H
#define ASTC_H

#include "bitmap/bitmap.h"
#include "base/array.h"

namespace iso {

struct CompressionParams;

struct ASTC {
	enum {
		MAX_TEXELS_PER_BLOCK		= 216,
		MAX_WEIGHTS_PER_BLOCK		= 64,
		MIN_WEIGHT_BITS_PER_BLOCK	= 24,
		MAX_WEIGHT_BITS_PER_BLOCK	= 96,

		TEXEL_WEIGHT_SUM			= 16,
		MAX_DECIMATION_MODES		= 87,
		MAX_WEIGHT_MODES			= 2048,
	};

	union {
		struct { uint32	mode:11, num_partitions:2, partition_id:10, mode_bits:6; };
		struct { uint32	:13, col_fmt:4;};
		struct {
			union {
				struct {uint64	mode:10, rsvd:2, x0:13, x1:13, y0:13, y1:13;}	vx2d;
				struct {uint64	mode:10, x0:9, x1:9, y0:9, y1:9, z0:9, z1:9;}	vx3d;
			};
			uint16	const_col[4];
		};
		uint8 data[16];
	};
	void	Decode(int X, int Y, const block<ISO_rgba, 2>& block) const;
	void	Decode(int X, int Y, int Z, const block<ISO_rgba, 3> &block) const;
	void	Encode(int X, int Y, int Z, const block<ISO_rgba, 3> &block, CompressionParams *params);
};

template<int X, int Y, int Z = 1> struct ASTCT : ASTC {
	void	Decode(const block<ISO_rgba, 3>& block)		const					{ ASTC::Decode(X, Y, Z, block); }
	void	Encode(const block<ISO_rgba, 3>& block, CompressionParams* params)	{ ASTC::Encode(X, Y, Z, block); }
	void	Decode(const block<ISO_rgba, 2>& block)		const					{ ASTC::Decode(X, Y, Z, make_block(block, 1)); }
	void	Encode(const block<ISO_rgba, 2>& block, CompressionParams* params)	{ ASTC::Encode(X, Y, Z, make_block(block, 1)); }
};

template<int X, int Y, typename D>			void copy(block<const ASTCT<X, Y>, 2> &srce, D& dest)			{ decode_blocked<X, Y>(srce, dest); }
template<typename S, int X, int Y>			void copy(block<S, 2> &srce, block<ASTCT<X, Y>, 2> &dest)		{ encode_blocked<X, Y>(srce, dest); }
template<int X, int Y, int Z, typename D>	void copy(block<const ASTCT<X, Y, Z>, 3> &srce, D& dest)		{ decode_blocked<X, Y, Z>(srce, dest); }
template<typename S, int X, int Y, int Z>	void copy(block<S, 3> &srce, block<ASTCT<X, Y, Z>, 3> &dest)	{ encode_blocked<X, Y, Z>(srce, dest); }

struct ColourParam {
	float	rgb, a;
	operator float4() const { return concat(float3(rgb), a); }
	ColourParam(float f) : rgb(f), a(f) {}
};

struct CompressionParams {
	enum FLAGS {
		ENABLE_RGB_SCALE_WITH_ALPHA	= 1 << 0,
		RA_NORMAL_ANGULAR_SCALE	= 1 << 1,
		PERFORM_SRGB_TRANSFORM	= 1 << 2,
	};

	flags<FLAGS>	flags;
	ColourParam	power;
	ColourParam	base_weight;
	ColourParam	mean_weight;
	ColourParam	stdev_weight;
	ColourParam	stdev_radius;

	float	rgb_mean_and_stdev_mixing;
	float	block_artifact_suppression;
	float	rgba_weights[4];

	float	block_artifact_suppression_expanded[ASTC::MAX_TEXELS_PER_BLOCK];

	// parameters that deal with heuristic codec speedups
	float	block_mode_cutoff;
	float	texel_avg_error_limit;
	float	partition_1_to_2_limit;
	float	lowest_correlation_cutoff;
	int		partition_search_limit;
	int		max_refinement_iters;

	auto_block<float4, 3>	input_averages;
	auto_block<float, 3>	input_alpha_averages;
	auto_block<float4, 3>	input_variances;

	CompressionParams()
		: power(1), base_weight(1), mean_weight(0), stdev_weight(0), stdev_radius(0)
		, rgb_mean_and_stdev_mixing(0), block_artifact_suppression(0)
	{
		rgba_weights[0] = rgba_weights[1]= rgba_weights[2] = rgba_weights[3] = 1;
	}

	void ComputeAverages(const block<HDRpixel, 3> &b);
	void ExpandBlockArtifactSuppression(int xdim, int ydim, int zdim);

	bool	AnyMeanStdevWeight() const {
		return base_weight.rgb != 1.0f || base_weight.a != 1.0f || mean_weight.rgb || stdev_weight.rgb || mean_weight.a || stdev_weight.a;
	}
};

}
#endif	// ASTC_H