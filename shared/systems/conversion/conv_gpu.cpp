#include "graphics.h"
#include "extra/filters.h"
#include "filetypes/bitmap/bitmap.h"
#include "iso/iso_convert.h"
#include "platformdata.h"

using namespace iso;

extern "C" ISO::Value tensor;

auto_block<float, 2> Gaussian(float rho, float eps) {
	float  g1[256];
	int	n  = gaussian(g1, 256, rho, 1, eps);
	int	n2 = 2 * n - 1;
	float* g2 = alloc_auto(float, n2);
	for (int x = 0; x < n; x++)
		g2[n - 1 - x] = g2[n - 1 + x] = g1[x];

	normalise_samples(g2, n2);

	auto gauss = make_auto_block<float>(n2, n2);
	for (int y = 0; y < n2; y++)
		for (int x = 0; x < n2; x++)
			gauss[y][x] = g2[x] * g2[y];

	return gauss;
}

pass*	GetTensorShader(const char* name, int i) {
	static PtrBrowser2 fx(tensor);
	return fx[name][i];
}

struct common_consts_t {
	point4	size;
	float2p	minmax;
	float   beta;
};


ISO_ptr<HDRbitmap> Average(ISO_ptr<HDRbitmap> bm, int w, int h) {
	int				   width = bm->Width(), height = bm->Height();
	int				   width2 = width / w, height2 = height / h;
	ISO_ptr<HDRbitmap> bm2(0, width2, height2);

	ComputeContext cc;
	cc.Begin();

	ConstBufferT<common_consts_t> consts;

	TextureT<float4> src(element_cast<float4>(bm->All()), MEM_WRITABLE | MEM_FORCE2D);
	TextureT<float4> dst(width2, height2, 1, 1, MEM_WRITABLE | MEM_FORCE2D);
	SurfaceT<float4> staging(width2, height2, MEM_STAGING | MEM_CPU_READ | MEM_FORCE2D);

	cc.SetShader(*GetTensorShader("average", 0));

	consts->size = int32x4{-w / 2, -h / 2, w, h};
	cc.SetConstBuffer(consts, 0);
	cc.SetTexture(src, 0);
	cc.SetRWTexture(dst, 0);
	cc.Dispatch(width2, height2);
	cc.Blit(staging, dst);

	cc.DingDong();
	cc.PutFence().Wait();
	copy(staging.Data(cc), element_cast<float4>(bm2->All()));
	return bm2;
}


ISO_ptr<HDRbitmap> SoftMax(ISO_ptr<HDRbitmap> bm, float beta) {
	int				   width = bm->Width(), height = bm->Height();
	ISO_ptr<HDRbitmap> bm2(0, width, height);

	int n = log2_ceil(width);

	auto in  = element_cast<float4>(bm->All());
	auto out = element_cast<float4>(bm2->All());

	ComputeContext cc;
	cc.Begin();

	ConstBufferT<common_consts_t> consts;
	consts->beta = beta;

#if 0
	cc.SetShader(*GetTensorShader("softmax", 7 + max(n - 6, 0)));

//	ConstBufferT<common_consts_t>	consts;
//	consts->beta = 1;

	TextureT<float4>	src(in, MEM_DEFAULT|MEM_FORCE2D);
	TextureT<float4>	dst(width, height, 1, 1, MEM_WRITABLE|MEM_FORCE2D);
	SurfaceT<float4>	staging(width, height, MEM_STAGING|MEM_CPU_READ|MEM_FORCE2D);

//	cc.SetConstBuffer(consts, 0);

	cc.SetTexture(src, 0);
	cc.SetRWTexture(dst, 0);
	cc.Dispatch(1, height);
	cc.Blit(staging, dst);
	cc.DingDong();
	cc.Wait(cc.PutFence());

	copy(staging.Data(cc), out);
	return bm2;

#elif 0
	uint32 group = 64;
	uint32 group2d = 8;

	uint32 dimx = width, dimy = height;

	TextureT<float4> src(in, MEM_WRITABLE | MEM_FORCE2D);
	SurfaceT<float4> staging(1, 1, MEM_STAGING | MEM_CPU_READ | MEM_FORCE2D);

#if 0
	TextureT<float4> dst(width, height, 1, 1, MEM_WRITABLE | MEM_FORCE2D);
	cc.SetShader(*GetTensorShader("getmax", 0));
	while (dimx > 1) {
		dimx = div_round_up(dimx, group);

		cc.SetTexture(src, 0);
		cc.SetRWTexture(dst, 0);
		cc.Dispatch(dimx, dimy);
		swap(src, dst);
	}

	cc.SetShader(*GetTensorShader("getmax", 1));
	while (dimy > 1) {
		dimy = div_round_up(dimy, group);

		cc.SetTexture(src, 0);
		cc.SetRWTexture(dst, 0);
		cc.Dispatch(dimx, dimy);
		swap(src, dst);
	}
#else
	TextureT<float4> dst(div_round_up(width, group2d), div_round_up(height, group2d), 1, 1, MEM_WRITABLE | MEM_FORCE2D);
	cc.SetShader(*GetTensorShader("getmax", 2));
	while (dimx > 1 || dimy > 1) {
		dimx = div_round_up(dimx, group2d);
		dimy = div_round_up(dimy, group2d);

		cc.SetTexture(src, 0);
		cc.SetRWTexture(dst, 0);
		cc.Dispatch(dimx, dimy);
		swap(src, dst);
	}
#endif

	cc.Blit(staging, src, Rect(0, 0, 1, 1));
	cc.DingDong();
	cc.Wait(cc.PutFence());
	copy(staging.Data(cc), out);
	return bm2;
#else
	uint32 group   = 64;
	uint32 group2d = 8;

	uint32 dimx = div_round_up(width, group2d);
	uint32 dimy = div_round_up(height, group2d);

	TextureT<float4> src(in, MEM_WRITABLE | MEM_FORCE2D);
	TextureT<float4> dst(width, height, 1, 1, MEM_WRITABLE | MEM_FORCE2D);
	TextureT<float4> temp(dimx * 2, dimy, 1, 1, MEM_WRITABLE | MEM_FORCE2D);
	TextureT<float4> pre(dimx * 2, dimy, 1, 1, MEM_WRITABLE | MEM_FORCE2D);
	SurfaceT<float4> staging(width, height, MEM_STAGING | MEM_CPU_READ | MEM_FORCE2D);

	// pre0
	cc.SetShader(*GetTensorShader("softmax", 2));

	consts->beta = beta;
	cc.SetConstBuffer(consts, 0);
	cc.SetTexture(src, 0);
	cc.SetRWTexture(pre, 0);
	cc.Dispatch(dimx, dimy);

	// pre1
	cc.SetShader(*GetTensorShader("softmax", 5));
	while (dimx > 1 || dimy > 1) {
		consts->size.z = dimx;
		consts->size.w = dimy;
		consts->beta   = beta;

		dimx = div_round_up(dimx, group2d);
		dimy = div_round_up(dimy, group2d);

		cc.SetConstBuffer(consts, 0);
		cc.SetTexture(pre, 0);
		cc.SetRWTexture(temp, 0);
		cc.Dispatch(dimx, dimy);
		swap(temp, pre);
	}

	cc.SetShader(*GetTensorShader("softmax", 6));
	cc.SetTexture(src, 0);
	cc.SetTexture(pre, 1);
	cc.SetRWTexture(dst, 0);
	cc.Dispatch(width, height);

	cc.Blit(staging, dst);

	cc.DingDong();
	cc.PutFence().Wait();
	copy(staging.Data(cc), out);
	return bm2;

#endif
}

ISO_ptr<HDRbitmap> LogSoftMax(ISO_ptr<HDRbitmap> bm, float beta) {
	int				   width = bm->Width(), height = bm->Height();
	ISO_ptr<HDRbitmap> bm2(0, width, height);

	int n = log2_ceil(width);

	auto in  = element_cast<float4>(bm->All());
	auto out = element_cast<float4>(bm2->All());

	ComputeContext cc;
	cc.Begin();

	ConstBufferT<common_consts_t> consts;
	consts->beta = beta;

	uint32 group   = 64;
	uint32 group2d = 8;

	uint32 dimx = div_round_up(width, group2d);
	uint32 dimy = div_round_up(height, group2d);

	TextureT<float4> src(in, MEM_WRITABLE | MEM_FORCE2D);
	TextureT<float4> dst(width, height, 1, 1, MEM_WRITABLE | MEM_FORCE2D);
	TextureT<float4> temp(dimx * 2, dimy, 1, 1, MEM_WRITABLE | MEM_FORCE2D);
	TextureT<float4> pre(dimx * 2, dimy, 1, 1, MEM_WRITABLE | MEM_FORCE2D);
	SurfaceT<float4> staging(width, height, MEM_STAGING | MEM_CPU_READ | MEM_FORCE2D);

	// pre0
	cc.SetShader(*GetTensorShader("softmax", 2));

	consts->beta = beta;
	cc.SetConstBuffer(consts, 0);
	cc.SetTexture(src, 0);
	cc.SetRWTexture(pre, 0);
	cc.Dispatch(dimx, dimy);

	// pre1
	cc.SetShader(*GetTensorShader("softmax", 5));
	while (dimx > 1 || dimy > 1) {
		consts->size.z = dimx;
		consts->size.w = dimy;
		consts->beta   = beta;

		dimx = div_round_up(dimx, group2d);
		dimy = div_round_up(dimy, group2d);

		cc.SetConstBuffer(consts, 0);
		cc.SetTexture(pre, 0);
		cc.SetRWTexture(temp, 0);
		cc.Dispatch(dimx, dimy);
		swap(temp, pre);
	}

	cc.SetShader(*GetTensorShader("logsoftmax", 0));
	cc.SetTexture(src, 0);
	cc.SetTexture(pre, 1);
	cc.SetRWTexture(dst, 0);
	cc.Dispatch(width, height);

	cc.Blit(staging, dst);

	cc.DingDong();
	cc.PutFence().Wait();
	copy(staging.Data(cc), out);
	return bm2;
}


//-----------------------------------------------------------------------------
//	GPU FFT
//-----------------------------------------------------------------------------

struct fft_consts {
	static const int	max_slm   = 9;
	static const uint32 uav_group = 16;

	uint32 log2block;
	uint32 log2size;
	point  offset;
	bool   want_imag;
};

void GPU_FFT_UAV(ComputeContext& cc, ConstBufferT<fft_consts>& consts, Texture& in_r, Texture& in_i, Texture& out_r, Texture& out_i, int log2size, bool inv) {
	auto size = out_r.Size();

	for (int i = fft_consts::max_slm; i < log2size; i++) {
		consts->log2size	= log2size;
		consts->log2block	= i;
		consts->offset		= zero;

		cc.SetConstBuffer(consts, 0);
		cc.SetTexture(in_r, 0);
		cc.SetTexture(in_i, 1);
		cc.SetRWTexture(out_r, 0);
		cc.SetRWTexture(out_i, 1);

		cc.Dispatch(size.x / (fft_consts::uav_group * 2), size.y / fft_consts::uav_group);

		swap(in_r, out_r);
		swap(in_i, out_i);
	}
}

void GPU_FFT(ComputeContext& cc, const Texture& in, Texture& out_r, int x0, int y0, bool inv) {
	auto info   = out_r.GetInfo();
	int  width  = info.width;
	int  height = info.height;

	Texture in_r(info, MEM_WRITABLE | MEM_FORCE2D);
	Texture in_i(info, MEM_WRITABLE | MEM_FORCE2D);
	Texture out_i(info, MEM_WRITABLE | MEM_FORCE2D);

	ConstBufferT<fft_consts> consts;

	// horizontal
	cc.SetShader(*GetTensorShader("butterfly_slm2", inv ? 2 : 0));

	int log2size	  = log2_ceil(width);
	consts->log2size  = log2size;
	consts->want_imag = true;
	consts->offset = int32x2{x0, y0};

	cc.SetConstBuffer(consts, 0);
	cc.SetTexture(in, 0);
	cc.SetTexture(0, 1);
	cc.SetRWTexture(out_r, 0);
	cc.SetRWTexture(out_i, 1);

	cc.Dispatch(ceil_pow2(width, fft_consts::max_slm), height);
	swap(in_r, out_r);
	swap(in_i, out_i);

	if (log2size > fft_consts::max_slm) {
		cc.SetShader(*GetTensorShader("butterfly_uav2", inv ? 2 : 0));
		GPU_FFT_UAV(cc, consts, in_r, in_i, out_r, out_i, log2size, inv);
	}

	// vertical
	cc.SetShader(*GetTensorShader("butterfly_slm2", inv ? 3 : 1));

	log2size			= log2_ceil(height);
	consts->log2size	= log2size;
	consts->want_imag	= log2size > fft_consts::max_slm;
	consts->offset		= zero;

	cc.SetConstBuffer(consts, 0);
	cc.SetTexture(in_r, 0);
	cc.SetTexture(in_i, 1);
	cc.SetRWTexture(out_r, 0);
	cc.SetRWTexture(out_i, 1);

	cc.Dispatch(ceil_pow2(height, fft_consts::max_slm), width);

	if (log2size > fft_consts::max_slm) {
		cc.SetShader(*GetTensorShader("butterfly_uav2", inv ? 3 : 1));
		swap(in_r, out_r);
		swap(in_i, out_i);
		GPU_FFT_UAV(cc, consts, in_r, in_i, out_r, out_i, log2size, inv);
		swap(in_r, out_r);
		swap(in_i, out_i);
	}
}

template<typename T> void GPU_FFT(ComputeContext& cc, const block<T, 2>& in, const block<T, 2>& out, int x0, int y0, bool inv) {
	int width  = next_pow2(out.template size<1>());
	int height = next_pow2(out.template size<2>());

	TextureT<T> in_r0(in, MEM_FORCE2D);
	TextureT<T> out_r(width, height, 1, 1, MEM_WRITABLE | MEM_FORCE2D);
	SurfaceT<T> staging(width, height, MEM_STAGING | MEM_CPU_READ | MEM_FORCE2D);

	GPU_FFT(cc, in_r0, out_r, x0, y0, inv);

	cc.Blit(staging, out_r);
	cc.DingDong();
	cc.PutFence().Wait();

	copy(staging.Data(cc).template slice<1>(0, out.template size<1>()).template slice<2>(0, out.template size<2>()), out);
}

ISO_ptr<HDRbitmap> FFT(ISO_ptr<HDRbitmap> bm, int width, int height) {
	if (width == 0)
		width = bm->Width();

	if (height == 0)
		height = bm->Height();

	ComputeContext cc;
	cc.Begin();

	ISO_ptr<HDRbitmap> bm2(0, width, height);
	GPU_FFT(cc, element_cast<float4>(bm->All()), element_cast<float4>(bm2->All()), 0, 0, false);
	return bm2;
}

//-----------------------------------------------------------------------------
//	GPU Convolve
//-----------------------------------------------------------------------------

ISO_ptr<HDRbitmap> Convolve(ISO_ptr<HDRbitmap> bm, const block<float, 2>& kernel) {
	int				   width = bm->Width(), height = bm->Height();
	ISO_ptr<HDRbitmap> bm2(0, width, height);

	int	n2 = kernel.size(), n = n2 / 2;
	float* col = alloc_auto(float, n2);
	int	row;
	bool   seperable = is_seperable(kernel, col, &row);

	auto in  = element_cast<float4>(bm->All());
	auto out = element_cast<float4>(bm2->All());

#if 1
	// convolution theorem

	ComputeContext cc;
	cc.Begin();

	uint32 width2  = next_pow2(width + n2);
	uint32 height2 = next_pow2(height + n2);

	TextureT<float4> fft_bm(width2, height2, 1, 1, MEM_WRITABLE | MEM_FORCE2D);
	TextureT<float>  fft_kernel(width2, height2, 1, 1, MEM_WRITABLE | MEM_FORCE2D);
	TextureT<float4> mult(width2, height2, 1, 1, MEM_WRITABLE | MEM_FORCE2D);
	SurfaceT<float4> staging(width2, height2, MEM_STAGING | MEM_CPU_READ | MEM_FORCE2D);

	GPU_FFT(cc, make_texture(in), fft_bm, -n, -n, false);
	GPU_FFT(cc, make_texture(kernel), fft_kernel, 0, 0, false);

	cc.SetShader(*GetTensorShader("multiply", 1));

	ConstBufferT<common_consts_t> consts;
	consts->minmax = float2{-1e38f, +1e38f};

	cc.SetConstBuffer(consts, 0);
	cc.SetTexture(fft_bm, 0);
	cc.SetTexture(fft_kernel, 1);
	cc.SetRWTexture(mult, 0);

	cc.Dispatch(width2, height2);

	GPU_FFT(cc, mult, fft_bm, 0, 0, true);
	cc.Blit(staging, fft_bm);

	cc.DingDong();
	cc.PutFence().Wait();

	copy(staging.Data(cc).slice<1>(0, width).slice<2>(0, height), out);
	return bm2;

#elif 1

	// gpu convolve

	ComputeContext cc;
	cc.Begin();
	cc.SetShader(*GetTensorShader("convolve", 0));

	ConstBufferT<common_consts_t> consts;

	if (seperable) {
		TextureT<float4> src(in, MEM_WRITABLE | MEM_FORCE2D);
		TextureT<float4> dst(width, height, 1, 1, MEM_WRITABLE | MEM_FORCE2D);
		DataBufferT<float> filterh(kernel[row]);
		DataBufferT<float> filterv(make_block(col, n2));
		SurfaceT<float4> dst2(width, height, MEM_STAGING | MEM_CPU_READ | MEM_FORCE2D);

		consts->size = int4(-n, 0, n2, 1);
		consts->minmax = float2(0, 1e8f);

		cc.SetConstBuffer(consts, 0);
		cc.SetTexture(src, 0);
		cc.SetRWTexture(dst, 0);
		cc.SetBuffer(filterh, 1);
		cc.Dispatch(width, height);

		consts->size = int4(0, -n, 1, n2);
		consts->minmax = float2(0, 1e8f);

		cc.SetConstBuffer(consts, 0);
		cc.SetTexture(dst, 0);
		cc.SetRWTexture(src, 0);
		cc.SetBuffer(filterv, 1);
		cc.Dispatch(width, height);

		cc.Blit(dst2, src);
		cc.DingDong();
		cc.Wait(cc.PutFence());

		copy(dst2.Data(cc), out);

	} else {
		TextureT<float4> src(in, MEM_DEFAULT | MEM_FORCE2D);
		TextureT<float4> dst(width, height, 1, 1, MEM_WRITABLE | MEM_FORCE2D);
		SurfaceT<float4> dst2(width, height, MEM_STAGING | MEM_CPU_READ | MEM_FORCE2D);
		DataBufferT<float> filter(flatten<2>(kernel));

		consts->size = int4(-n, -n, n2, n2);
		consts->minmax = float2(0, 1e8f);

		cc.SetConstBuffer(consts, 0);
		cc.SetTexture(src, 0);
		cc.SetRWTexture(dst, 0);
		cc.SetBuffer(filter, 1);
		cc.Dispatch(width, height);
		cc.Blit(dst2, dst);
		cc.DingDong();
		cc.Wait(cc.PutFence());

		copy(dst2.Data(cc), out);
	}

#else

	conv_params params;
	params.input_offset[0] = -n;
	params.input_offset[1] = -n;
	params.min			   = minimum;
	params.max			   = maximum;

	// cpu convolve
	if (seperable) {
		auto temp = make_auto_block<float4>(width, height);
		convolve(temp, in, kernel[row], params);
		convolve(out, temp, make_block(col, 1, n2), params);  // sideways
	} else {
		convolve(out, in, kernel, params);
	}
#endif

	return bm2;
}

#include "filetypes\container\flatbuffer.h"

namespace tflite {
using namespace flatbuffers;

enum BuiltinOperator {
	ADD = 0,
	AVERAGE_POOL_2D = 1,
	CONCATENATION = 2,
	CONV_2D = 3,
	DEPTHWISE_CONV_2D = 4,
	DEQUANTIZE = 6,
	EMBEDDING_LOOKUP = 7,
	FLOOR = 8,
	FULLY_CONNECTED = 9,
	HASHTABLE_LOOKUP = 10,
	L2_NORMALIZATION = 11,
	L2_POOL_2D = 12,
	LOCAL_RESPONSE_NORMALIZATION = 13,
	LOGISTIC = 14,
	LSH_PROJECTION = 15,
	LSTM = 16,
	MAX_POOL_2D = 17,
	MUL = 18,
	RELU = 19,
	RELU_N1_TO_1 = 20,
	RELU6 = 21,
	RESHAPE = 22,
	RESIZE_BILINEAR = 23,
	RNN = 24,
	SOFTMAX = 25,
	SPACE_TO_DEPTH = 26,
	SVDF = 27,
	TANH = 28,
	CONCAT_EMBEDDINGS = 29,
	SKIP_GRAM = 30,
	CALL = 31,
	CUSTOM = 32,
	EMBEDDING_LOOKUP_SPARSE = 33,
	PAD = 34,
	UNIDIRECTIONAL_SEQUENCE_RNN = 35,
	GATHER = 36,
	BATCH_TO_SPACE_ND = 37,
	SPACE_TO_BATCH_ND = 38,
	TRANSPOSE = 39,
	MEAN = 40,
	SUB = 41,
	DIV = 42,
	SQUEEZE = 43,
	UNIDIRECTIONAL_SEQUENCE_LSTM = 44,
	STRIDED_SLICE = 45,
	BIDIRECTIONAL_SEQUENCE_RNN = 46,
	EXP = 47,
	TOPK_V2 = 48,
	SPLIT = 49,
	LOG_SOFTMAX = 50,
	DELEGATE = 51,
	BIDIRECTIONAL_SEQUENCE_LSTM = 52,
	CAST = 53,
	PRELU = 54,
	MAXIMUM = 55,
	ARG_MAX = 56,
	MINIMUM = 57,
	LESS = 58,
	NEG = 59,
	PADV2 = 60,
	GREATER = 61,
	GREATER_EQUAL = 62,
	LESS_EQUAL = 63,
	SELECT = 64,
	SLICE = 65,
	SIN = 66,
	TRANSPOSE_CONV = 67,
	SPARSE_TO_DENSE = 68,
	TILE = 69,
	EXPAND_DIMS = 70,
	EQUAL = 71,
	NOT_EQUAL = 72,
	LOG = 73,
	SUM = 74,
	SQRT = 75,
	RSQRT = 76,
	SHAPE = 77,
	POW = 78,
	ARG_MIN = 79,
	FAKE_QUANT = 80,
	REDUCE_PROD = 81,
	REDUCE_MAX = 82,
	PACK = 83,
	LOGICAL_OR = 84,
	ONE_HOT = 85,
	LOGICAL_AND = 86,
	LOGICAL_NOT = 87,
	UNPACK = 88,
	REDUCE_MIN = 89,
	FLOOR_DIV = 90,
	REDUCE_ANY = 91,
	SQUARE = 92,
	ZEROS_LIKE = 93,
	FILL = 94,
	FLOOR_MOD = 95,
	RANGE = 96,
	RESIZE_NEAREST_NEIGHBOR = 97,
	LEAKY_RELU = 98,
	SQUARED_DIFFERENCE = 99,
	MIRROR_PAD = 100,
	ABS = 101,
	SPLIT_V = 102,
};

enum TensorType {
	TensorType_FLOAT32 = 0,
	TensorType_FLOAT16 = 1,
	TensorType_INT32 = 2,
	TensorType_UINT8 = 3,
	TensorType_INT64 = 4,
	TensorType_STRING = 5,
	TensorType_BOOL = 6,
	TensorType_INT16 = 7,
	TensorType_COMPLEX64 = 8,
	TensorType_INT8 = 9,
	TensorType_MIN = TensorType_FLOAT32,
	TensorType_MAX = TensorType_INT8
};
enum QuantizationDetails {
	QuantizationDetails_NONE = 0,
	QuantizationDetails_CustomQuantization = 1,
	QuantizationDetails_MIN = QuantizationDetails_NONE,
	QuantizationDetails_MAX = QuantizationDetails_CustomQuantization
};

enum Padding {
	Padding_SAME = 0,
	Padding_VALID = 1,
	Padding_MIN = Padding_SAME,
	Padding_MAX = Padding_VALID
};

enum ActivationFunctionType {
	ActivationFunctionType_NONE = 0,
	ActivationFunctionType_RELU = 1,
	ActivationFunctionType_RELU_N1_TO_1 = 2,
	ActivationFunctionType_RELU6 = 3,
	ActivationFunctionType_TANH = 4,
	ActivationFunctionType_SIGN_BIT = 5,
	ActivationFunctionType_MIN = ActivationFunctionType_NONE,
	ActivationFunctionType_MAX = ActivationFunctionType_SIGN_BIT
};

enum BuiltinOptions {
	BuiltinOptions_NONE = 0,
	BuiltinOptions_Conv2DOptions = 1,
	BuiltinOptions_DepthwiseConv2DOptions = 2,
	BuiltinOptions_ConcatEmbeddingsOptions = 3,
	BuiltinOptions_LSHProjectionOptions = 4,
	BuiltinOptions_Pool2DOptions = 5,
	BuiltinOptions_SVDFOptions = 6,
	BuiltinOptions_RNNOptions = 7,
	BuiltinOptions_FullyConnectedOptions = 8,
	BuiltinOptions_SoftmaxOptions = 9,
	BuiltinOptions_ConcatenationOptions = 10,
	BuiltinOptions_AddOptions = 11,
	BuiltinOptions_L2NormOptions = 12,
	BuiltinOptions_LocalResponseNormalizationOptions = 13,
	BuiltinOptions_LSTMOptions = 14,
	BuiltinOptions_ResizeBilinearOptions = 15,
	BuiltinOptions_CallOptions = 16,
	BuiltinOptions_ReshapeOptions = 17,
	BuiltinOptions_SkipGramOptions = 18,
	BuiltinOptions_SpaceToDepthOptions = 19,
	BuiltinOptions_EmbeddingLookupSparseOptions = 20,
	BuiltinOptions_MulOptions = 21,
	BuiltinOptions_PadOptions = 22,
	BuiltinOptions_GatherOptions = 23,
	BuiltinOptions_BatchToSpaceNDOptions = 24,
	BuiltinOptions_SpaceToBatchNDOptions = 25,
	BuiltinOptions_TransposeOptions = 26,
	BuiltinOptions_ReducerOptions = 27,
	BuiltinOptions_SubOptions = 28,
	BuiltinOptions_DivOptions = 29,
	BuiltinOptions_SqueezeOptions = 30,
	BuiltinOptions_SequenceRNNOptions = 31,
	BuiltinOptions_StridedSliceOptions = 32,
	BuiltinOptions_ExpOptions = 33,
	BuiltinOptions_TopKV2Options = 34,
	BuiltinOptions_SplitOptions = 35,
	BuiltinOptions_LogSoftmaxOptions = 36,
	BuiltinOptions_CastOptions = 37,
	BuiltinOptions_DequantizeOptions = 38,
	BuiltinOptions_MaximumMinimumOptions = 39,
	BuiltinOptions_ArgMaxOptions = 40,
	BuiltinOptions_LessOptions = 41,
	BuiltinOptions_NegOptions = 42,
	BuiltinOptions_PadV2Options = 43,
	BuiltinOptions_GreaterOptions = 44,
	BuiltinOptions_GreaterEqualOptions = 45,
	BuiltinOptions_LessEqualOptions = 46,
	BuiltinOptions_SelectOptions = 47,
	BuiltinOptions_SliceOptions = 48,
	BuiltinOptions_TransposeConvOptions = 49,
	BuiltinOptions_SparseToDenseOptions = 50,
	BuiltinOptions_TileOptions = 51,
	BuiltinOptions_ExpandDimsOptions = 52,
	BuiltinOptions_EqualOptions = 53,
	BuiltinOptions_NotEqualOptions = 54,
	BuiltinOptions_ShapeOptions = 55,
	BuiltinOptions_PowOptions = 56,
	BuiltinOptions_ArgMinOptions = 57,
	BuiltinOptions_FakeQuantOptions = 58,
	BuiltinOptions_PackOptions = 59,
	BuiltinOptions_LogicalOrOptions = 60,
	BuiltinOptions_OneHotOptions = 61,
	BuiltinOptions_LogicalAndOptions = 62,
	BuiltinOptions_LogicalNotOptions = 63,
	BuiltinOptions_UnpackOptions = 64,
	BuiltinOptions_FloorDivOptions = 65,
	BuiltinOptions_SquareOptions = 66,
	BuiltinOptions_ZerosLikeOptions = 67,
	BuiltinOptions_FillOptions = 68,
	BuiltinOptions_BidirectionalSequenceLSTMOptions = 69,
	BuiltinOptions_BidirectionalSequenceRNNOptions = 70,
	BuiltinOptions_UnidirectionalSequenceLSTMOptions = 71,
	BuiltinOptions_FloorModOptions = 72,
	BuiltinOptions_RangeOptions = 73,
	BuiltinOptions_ResizeNearestNeighborOptions = 74,
	BuiltinOptions_LeakyReluOptions = 75,
	BuiltinOptions_SquaredDifferenceOptions = 76,
	BuiltinOptions_MirrorPadOptions = 77,
	BuiltinOptions_AbsOptions = 78,
	BuiltinOptions_SplitVOptions = 79,
	BuiltinOptions_MIN = BuiltinOptions_NONE,
	BuiltinOptions_MAX = BuiltinOptions_SplitVOptions
};

interval<float>	get_activation_interval(ActivationFunctionType fused_activation_function) {
	switch (fused_activation_function) {
		case ActivationFunctionType_RELU:			return interval<float>(0, maximum);
		case ActivationFunctionType_RELU_N1_TO_1:	return interval<float>(-1, 1);
		case ActivationFunctionType_RELU6:			return interval<float>(0, 6);
		default:									return interval<float>(minimum, maximum);
	}
}
inline int get_offset(int stride, int dilate, int in_size, int filter_size, int out_size) {
	int		effective_filter_size = (filter_size - 1) * dilate + 1;
	return min((in_size - (out_size - 1) * stride - effective_filter_size) / 2, 0);
//	return -max(((out_size - 1) * stride + effective_filter_size - in_size) / 2, 0);
}

inline int get_size(TensorType type) {
	static const uint8 sizes[] = {
		4,	//TensorType_FLOAT32 = 0,
		2,	//TensorType_FLOAT16 = 1,
		4,	//TensorType_INT32 = 2,
		1,	//TensorType_UINT8 = 3,
		8,	//TensorType_INT64 = 4,
		4,	//TensorType_STRING = 5,
		1,	//TensorType_BOOL = 6,
		2,	//TensorType_INT16 = 7,
		16,	//TensorType_COMPLEX64 = 8,
		1,	//TensorType_INT8 = 9,
	};
	return sizes[type];
}

template<typename T> struct TensorType_s;
template<> struct TensorType_s<float32>			{ static const TensorType type = TensorType_FLOAT32;	};
template<> struct TensorType_s<float16>			{ static const TensorType type = TensorType_FLOAT16;	};
template<> struct TensorType_s<int32>			{ static const TensorType type = TensorType_INT32;		};
template<> struct TensorType_s<uint8>			{ static const TensorType type = TensorType_UINT8;		};
template<> struct TensorType_s<int64>			{ static const TensorType type = TensorType_INT64;		};
template<> struct TensorType_s<string>			{ static const TensorType type = TensorType_STRING;		};
template<> struct TensorType_s<bool>			{ static const TensorType type = TensorType_BOOL;		};
template<> struct TensorType_s<int16>			{ static const TensorType type = TensorType_INT16;		};
template<> struct TensorType_s<complex<float64>>{ static const TensorType type = TensorType_COMPLEX64;	};
template<> struct TensorType_s<int8>			{ static const TensorType type = TensorType_INT8;		};


struct Conv2DOptions : private Table {
	enum { VT_PADDING = 4, VT_STRIDE_W = 6, VT_STRIDE_H = 8, VT_FUSED_ACTIVATION_FUNCTION = 10, VT_DILATION_W_FACTOR = 12, VT_DILATION_H_FACTOR = 14 };
	union {
		Field<VT_PADDING,					Padding>							padding;
		Field<VT_STRIDE_W,					int32>								stride_w;
		Field<VT_STRIDE_H,					int32>								stride_h;
		Field<VT_FUSED_ACTIVATION_FUNCTION,	As<ActivationFunctionType, uint8>>	fused_activation_function;
		Field<VT_DILATION_W_FACTOR,			Default<int32,1> >					dilation_w_factor;
		Field<VT_DILATION_H_FACTOR,			Default<int32,1> >					dilation_h_factor;
	};
};

struct DepthwiseConv2DOptions : private Table {
	enum {VT_PADDING = 4, VT_STRIDE_W = 6, VT_STRIDE_H = 8, VT_DEPTH_MULTIPLIER = 10, VT_FUSED_ACTIVATION_FUNCTION = 12, VT_DILATION_W_FACTOR = 14, VT_DILATION_H_FACTOR = 16 };
	union {
		Field<VT_PADDING,					Padding>							padding;
		Field<VT_STRIDE_W,					int32>								stride_w;
		Field<VT_STRIDE_H,					int32>								stride_h;
		Field<VT_DEPTH_MULTIPLIER,			int32>								depth_multiplier;
		Field<VT_FUSED_ACTIVATION_FUNCTION,	As<ActivationFunctionType, uint8>>	fused_activation_function;
		Field<VT_DILATION_W_FACTOR,			Default<int32,1> >					dilation_w_factor;
		Field<VT_DILATION_H_FACTOR,			Default<int32,1> >					dilation_h_factor;
	};
};

struct OperatorCode : private Table {
	enum { VT_BUILTIN_CODE = 4, VT_CUSTOM_CODE = 6, VT_VERSION = 8 };
	union {
		Field<VT_BUILTIN_CODE,		As<BuiltinOperator,int8>>			builtin_code;
		Field<VT_CUSTOM_CODE,		const String*>	custom_code;
		Field<VT_VERSION,			int32>			version;
	};
};

struct Buffer : private Table {
	enum { VT_DATA = 4 };
	union {
		Field<VT_DATA,				Vector<uint8>*>	data;
	};
};

struct CustomQuantization : private Table {
	enum { VT_CUSTOM = 4 };
	union {
		Field<VT_CUSTOM,			Vector<int8>>	custom;
	};
};

struct QuantizationParameters : private Table {
	enum { VT_MIN = 4, VT_MAX = 6, VT_SCALE = 8, VT_ZERO_POINT = 10, VT_DETAILS_TYPE = 12, VT_DETAILS = 14 };
	union {
		Field<VT_MIN,				Vector<float>>	min;
		Field<VT_MAX,				Vector<float>>	max;
		Field<VT_SCALE,				Vector<float>>	scale;
		Field<VT_ZERO_POINT,		Vector<int64>>	zero_point;
		Field<VT_DETAILS_TYPE,		uint8>			details_type;
		Field<VT_DETAILS,			const void*>	details;
	};
};

struct Tensor : private Table {
	enum { VT_SHAPE = 4, VT_TYPE = 6, VT_BUFFER = 8, VT_NAME = 10, VT_QUANTIZATION = 12, VT_IS_VARIABLE = 14 };
	union {
		Field<VT_SHAPE,				Vector<int32>>					shape;
		Field<VT_TYPE,				As<TensorType,int8>>			type;
		Field<VT_BUFFER,			uint32>							buffer;
		Field<VT_NAME,				const String*>					name;
		Field<VT_QUANTIZATION,		const QuantizationParameters*>	quantization;
		Field<VT_IS_VARIABLE,		uint8>							is_variable;
	};
};


struct Operator : private Table {
	enum { VT_OPCODE_INDEX = 4, VT_INPUTS = 6, VT_OUTPUTS = 8, VT_BUILTIN_OPTIONS_TYPE = 10, VT_BUILTIN_OPTIONS = 12, VT_CUSTOM_OPTIONS = 14, VT_CUSTOM_OPTIONS_FORMAT = 16, VT_MUTATING_VARIABLE_INPUTS = 18 };
	union {
		Field<VT_OPCODE_INDEX,				uint32>					opcode_index;
		Field<VT_INPUTS,					Vector<int32>>			inputs;
		Field<VT_OUTPUTS,					Vector<int32>>			outputs;
		Field<VT_BUILTIN_OPTIONS_TYPE, As<BuiltinOptions,uint8>>	builtin_options_type;
		Field<VT_BUILTIN_OPTIONS,			const void*>			builtin_options;
		Field<VT_CUSTOM_OPTIONS,			Vector<uint8>>			custom_options;
		Field<VT_CUSTOM_OPTIONS_FORMAT,		int8>					custom_options_format;
		Field<VT_MUTATING_VARIABLE_INPUTS,	Vector<uint8>>			mutating_variable_inputs;
	};
};

struct SubGraph : private Table {
	enum { VT_TENSORS = 4, VT_INPUTS = 6, VT_OUTPUTS = 8, VT_OPERATORS = 10, VT_NAME = 12 };
	union {
		Field<VT_TENSORS,			Vector<Offset<Tensor>>>			tensors;
		Field<VT_INPUTS,			Vector<int32>>					inputs;
		Field<VT_OUTPUTS,			Vector<int32>>					outputs;
		Field<VT_OPERATORS,			Vector<Offset<Operator>>>		operators;
		Field<VT_NAME,				const String*>					name;
	};
};

struct Model : private Table {
	enum { VT_VERSION = 4, VT_OPERATOR_CODES = 6, VT_SUBGRAPHS = 8, VT_DESCRIPTION = 10, VT_BUFFERS = 12, VT_METADATA_BUFFER = 14 };
	union {
		Field<VT_VERSION,			uint32>							version;
		Field<VT_OPERATOR_CODES,	Vector<Offset<OperatorCode>>>	operator_codes;
		Field<VT_SUBGRAPHS,			Vector<Offset<SubGraph>>>		subgraphs;
		Field<VT_DESCRIPTION,		const String*>					description;
		Field<VT_BUFFERS,			Vector<Offset<Buffer>>>			buffers;
		Field<VT_METADATA_BUFFER,	Vector<int32>>					metadata_buffer;
	};
};

} // namespace tflite

template<typename N> class Partition {
public:
	enum Type {
		kTfUnexplored = 0,			// temporarily used during creation
		kTfPartition,
		kTfNonPartition
	};
	enum {
		kEpochNotReady = -1,		// The node or edge is not ready to be assigned an epoch. e.g. a node's inputs have not all been assigned epochs.
		kEpochAlwaysReady = -2		// Used for edge_epochs. This means that the edge is always ready. e.g. an input to the whole model or a constant that has no dependencies.
	};

	struct NodeSubset {
		Type type = kTfUnexplored;
		dynamic_array<int> nodes, inputs, outputs;

		void uniqueify() {
			sort(inputs);
			inputs.erase(unique(inputs.begin(), inputs.end()), inputs.end());
			sort(outputs);
			outputs.erase(unique(outputs.begin(), outputs.end()), outputs.end());
		}
	};

	dynamic_array<NodeSubset>	subsets;

private:
	struct NodeInfo {
		Type	type	= kTfNonPartition;
		int		epoch	= kEpochNotReady;
	};

	dynamic_array<NodeInfo>	node_info;
	dynamic_array<int>		edge_epochs;	// Maps from edge index to the epoch in which it is assigned. Also special negative values of kEpochNotAssigned if not assigned, kEpochNotReady if it is an input or constant.

	// Updates the  node and returns true if it is assigned to an epoch
	// False is returned if the node is already set to an epoch, its inputs are not all assigned to epochs, or if it cannot be assigned to the current epoch since the epoch's node_type doesn't match
	bool UpdateNode(const N &node, int index) {
		// Check if node is already done
		if (node_info[index].epoch != kEpochNotReady)
			return false;

		// See if all dependencies of this node are already assigned to a node sub set
		for (int i : node.inputs) {
			if (edge_epochs[i] == kEpochNotReady)
				return false;
		}

		NodeSubset&	subset	= subsets.back();
		int			epoch	= subsets.size32() - 1;

		// When we are starting a new epoch, the first ready node defines the type of that epoch.
		if (subset.type == kTfUnexplored)
			subset.type = node_info[index].type;

		// The node gets assigned to this epoch if it is the same type as the epoch's assigned type
		if (subset.type != node_info[index].type)
			return false;

		node_info[index].epoch = epoch;
		subset.nodes.push_back(index);

		// All outputs of this node now are assigned to this epoch as well
		for (int i : node.outputs)
			edge_epochs[i] = epoch;

		// Look at our inputs one more time to update that edge's epochs' outputs
		for (int i : node.inputs) {
			int	edge_epoch = edge_epochs[i];
			if (edge_epoch != epoch) {
				subset.inputs.push_back(i);
				// Set inputs to be outputs of the NodeSubset where they reside; make sure inputs to the whole computation are not included
				if (edge_epoch >= 0)
					subsets[edge_epoch].outputs.push_back(i);
			}
		}
		return true;
	}

	// Completely populates the current node_subset by doing graph traversal
	bool BuildNodeSubset(const dynamic_array<N> &nodes) {
		auto&	 subset = subsets.push_back();
		// loop until no more nodes can be updated
		for (;;) {
			bool did_something = false;
			for (auto &i : nodes) {
				if (UpdateNode(i, nodes.index_of(i)))
					did_something = true;
			}
			if (!did_something)
				return !subset.nodes.empty();
		}
	}

public:
	Partition(const dynamic_array<N> &nodes, int num_edges, const dynamic_array<int> &nodes_to_partition, const dynamic_array<int> &outputs) : node_info(nodes.size()), edge_epochs(num_edges, kEpochAlwaysReady) {
		// Populate the node_type map
		for (auto i : nodes_to_partition)
			node_info[i].type = kTfPartition;

		// Set computed edges to be kEpochNotReady
		for (auto &i : nodes) {
			for (int j : i.outputs)
				edge_epochs[j] = kEpochNotReady;
		}

		while (BuildNodeSubset(nodes))
			;

		subsets.pop_back();

		// Mark model outputs as node sub set outputs - all the rest have already been identified
		for (int i : outputs)
			subsets[edge_epochs[i]].outputs.push_back(i);

		for (auto &i : subsets)
			i.uniqueify();
	}

};

#if 0
struct GraphNode {
	const flatbuffers::Vector<int>	&inputs, &outputs;
	GraphNode(tflite::Operator *op) : inputs(op->inputs), outputs(op->outputs) {}
};

void TFAveragePool2D(const block<float, 4> &out, const block<float, 4> &in) {
}

void TFSoftMax(const block<float, 2> &out, const block<float, 2> &in) {
}

void TFReshape(const block<float, 2> &out, const block<float, 4> &in, const block<int, 1> &b) {
}

void TFConv2D(const block<float, 4> &out, const block<float, 4> &in, const block<float, 4> &kernel, const block<float, 1> &bias, const tflite::Conv2DOptions *opts) {
	int		stride[3] = {opts->stride_w, opts->stride_h, 1};
	int		dilate[3] = {opts->dilation_w_factor, opts->dilation_h_factor, 1};
	int		offset[3] = {
		tflite::get_offset(stride[0], dilate[0], in.size<2>(), kernel.size<2>(), out.size<2>()),
		tflite::get_offset(stride[1], dilate[1], in.size<3>(), kernel.size<3>(), out.size<3>()),
		0
	};

	conv_params params;
	params.offset		= offset;
	params.stride		= stride;
	params.dilate		= dilate;
	params.activation	= get_activation_interval(opts->fused_activation_function);
	params.bias			= bias;

	int		c = 0;
	for (auto k : kernel) {
//		convolve<0>(out.slice<1>(c, 1), in, k, params);
		convolve_test(out.template slice<1>(c, 1));
		++c;
		++params.bias;
	}
}

void TFDepthConv2D(const block<float, 4> &out, const block<float, 4> &in, const block<float, 4> &kernel, const block<float, 1> &bias, const tflite::DepthwiseConv2DOptions *opts) {
	int		stride[4] = {opts->stride_w, 1, opts->stride_h, 1};
	int		dilate[4] = {opts->dilation_w_factor, 1, opts->dilation_h_factor, 1};
	int		offset[4] = {
		tflite::get_offset(stride[0], dilate[0], in.size<2>(), kernel.size<2>(), out.size<2>()),
		0,
		tflite::get_offset(stride[1], dilate[1], in.size<3>(), kernel.size<3>(), out.size<3>()),
		0
	};

	conv_params params;
	params.offset		= offset;
	params.stride		= stride;
	params.dilate		= dilate;
	params.activation	= get_activation_interval(opts->fused_activation_function);
	params.bias			= bias;

	convolve<1>(out, in, kernel, params);
}

struct TensorFlowState {
	struct Tensor {
		const flatbuffers::Vector<int32>	&shape;
		tflite::TensorType					type;
		const char*							name;
		const tflite::QuantizationParameters *quantization;
		bool								is_variable;
		const_memory_block					data;

		Tensor(const param_element<tflite::Tensor*, TensorFlowState*> &x)
			: shape(x.t->shape)
			, type(x.t->type)
			, name(*x.t->name)
			, quantization(x.t->quantization)
			, is_variable(x.t->is_variable)
			, data(x.p->GetBufferData(x.t->buffer))
		{}

		const int32 total_size() const {
			uint32	t = 1;
			for (auto &i : shape)
				t *= i;
			return t;
		}
		const int32 *check_shape(int dims) const {
			if (shape.size() < dims)
				return 0;
			const int32	*p = shape.begin();
			for (int n = shape.size() - dims; n--; ++p) {
				if (*p != 1)
					return 0;
			}
			return p;
		}
		memory_block make_data() {
			if (!data)
				data = malloc_block(total_size() * get_size(type)).detach();
			return memory_block((void*)(const void*)data, data.length());
		}

		template<typename T, int N> operator block<T, N>() {
			const int32	*p = check_shape(N);
			ISO_ASSERT(p && type == tflite::TensorType_s<T>::type);
			if (!data)
				data = malloc_block(total_size() * sizeof(T)).detach();
			return make_block_rev<N>((T*)make_data(), p);
		}
		template<typename T, int N, int M> operator block<array_vec<T,N>, M>() {
			const int32	*p = check_shape(M + 1);
			ISO_ASSERT(p && p[M] == N && type == tflite::TensorType_s<T>::type);
			return make_block_rev<M>((array_vec<T,N>*)make_data(), p);
		}
	};

	const tflite::Model		&model;
	const tflite::SubGraph	&subgraph;
	dynamic_array<Tensor>	tensors;

	const_memory_block	GetBufferData(const flatbuffers::Vector<uint8> *data) const {
		return data ? data->block() : none;
	}
	const_memory_block	GetBufferData(uint32 b) const {
		return b ? GetBufferData(model.buffers[b]->data) : none;
	}

	TensorFlowState(const tflite::Model	&model, const tflite::SubGraph &subgraph) : model(model), subgraph(subgraph), tensors(with_param(subgraph.tensors, this)) {
	}

	template<typename T> bool	SetInput(int i, const block<T, 2> &data) {
		if (i >= subgraph.inputs.size())
			return false;

		auto	&t	= tensors[subgraph.inputs[i]];

#if 1
		const int	wanted_width	= t.shape[2];
		const int	wanted_height	= t.shape[1];
		const int	wanted_channels = t.shape[3];
		const float input_mean		= 127.5f;
		const float input_std		= 127.5f;
		float*		out				= t.make_data();

		for (int y = 0; y < wanted_height; ++y) {
			auto		in_row	= data[(y * data.template size<2>()) / wanted_height];
			float*		out_row	= out + (y * wanted_width * wanted_channels);
			for (int x = 0; x < wanted_width; ++x) {
				auto		in_pixel	= in_row[(x * data.template size<1>()) / wanted_width];
				float*		out_pixel	= out_row + (x * wanted_channels);
				for (int c = 0; c < wanted_channels; ++c)
					out_pixel[c] = (in_pixel[c] - input_mean) / input_std;
			}
		}
#else
		resample((block<float3p, 2>)t, t.make_data());
#endif
		return true;
	}

	void	Process(const tflite::Operator &op) {
		auto	&opcode		= model.operator_codes[op.opcode_index];
		auto	&inputs		= op.inputs;
		auto	&outputs	= op.outputs;

		switch (opcode->builtin_code) {
			case tflite::AVERAGE_POOL_2D:
				TFAveragePool2D(tensors[outputs[0]], tensors[inputs[0]]);
				break;
			case tflite::CONV_2D:
				TFConv2D(tensors[outputs[0]], tensors[inputs[0]], tensors[inputs[1]], tensors[inputs[2]], op.builtin_options.as<tflite::Conv2DOptions>());
				break;
			case tflite::DEPTHWISE_CONV_2D:
				TFDepthConv2D(tensors[outputs[0]], tensors[inputs[0]], tensors[inputs[1]], tensors[inputs[2]], op.builtin_options.as<tflite::DepthwiseConv2DOptions>());
				break;
			case tflite::SOFTMAX:
				TFSoftMax(tensors[outputs[0]], tensors[inputs[0]]);
				break;
			case tflite::RESHAPE:
				TFReshape(tensors[outputs[0]], tensors[inputs[0]], tensors[inputs[1]]);
				break;
		}
	}

	void	Process() {
		for (auto &op : subgraph.operators)
			Process(*op);
	}
};

ISO_ptr<void> TensorFlow(ISO_ptr<void> tfl3, ISO_ptr<bitmap> bm) {
	if (!tfl3.IsType("TFL3"))
		return ISO_NULL;

	const_memory_block	*data	= tfl3;
	auto				&model	= *flatbuffers::GetRoot<tflite::Model>(*data);
	TensorFlowState	state(model, *model.subgraphs[0]);

	state.SetInput(0, bm->All());
	state.Process();

	return ISO::MakePtr(tag2(), (block<float,1>)state.tensors[state.subgraph.outputs[0]]);

//	Partition<GraphNode>	part(*subgraph.operators(), subgraph.tensors()->size(), int_range<int>(subgraph.operators()->size()), *subgraph.outputs());

//	auto	t0	= (*subgraph.tensors())[0]->buffer();
//	auto	b0	= &*(*model.buffers())[t0]->data();

	ISO::Browser2	b(tfl3);
	auto	t1	= b["subgraphs"][0]["tensors"][0]["buffer"].GetInt();
	auto	b1	= **b["buffers"][t1]["data"];
	b["buffers"][1];

	return bm;
}
#endif

//-----------------------------------------------------------------------------
//	init
//-----------------------------------------------------------------------------

static initialise init(
	ISO_get_operation(Gaussian),
	ISO_get_operation(Convolve),
	ISO_get_operation(FFT),
	ISO_get_operation(SoftMax),
	ISO_get_operation(LogSoftMax),
	ISO_get_operation(Average)
	//ISO_get_operation(TensorFlow)
);
