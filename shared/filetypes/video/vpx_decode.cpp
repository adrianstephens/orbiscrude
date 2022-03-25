#include "vpx_decode.h"
#include "maths/dct.h"
#include "base/strings.h"
#include "base/algorithm.h"
#include "base/atomic.h"
#include "allocators/allocator.h"
#include "jobs.h"
#include "graphics.h"
#include "iso/iso.h"

using namespace vp9;

force_inline uint8	clip_pixel(int val)							{ return clamp(val, 0, 255); }
//inline void			clip_pixel_add(uint8 &dest, int trans)	{ dest = clip_pixel(trans); }//clamp(dest + trans, 0, 255); }
inline void			clip_pixel_add(uint8 &dest, int trans)		{ dest = clamp(dest + trans, 0, 255); }
force_inline int8	signed_char_clamp(int t)					{ return (int8)clamp(t, -128, 127); }

extern const ScaleFactors::kernel *filter_kernels[4];


//-------------------------------------
//	data
//-------------------------------------

const uint8 tx_mode_to_biggest_tx_size[TX_MODES] = {
	TX_4X4,			// ONLY_4X4
	TX_8X8,			// ALLOW_8X8
	TX_16X16,		// ALLOW_16X16
	TX_32X32,		// ALLOW_32X32
	TX_32X32,		// TX_MODE_SELECT
};

enum {
	NEED_ABOVE		= 1 << 1,
	NEED_LEFT		= 1 << 2,
	NEED_RIGHT		= 1 << 3,
};
static const uint8 extend_modes[INTRA_MODES+3] = {
	NEED_ABOVE | NEED_LEFT,		// DC
	NEED_ABOVE,					// V
	NEED_LEFT,					// H
	NEED_ABOVE | NEED_RIGHT,	// D45
	NEED_LEFT | NEED_ABOVE,		// D135
	NEED_LEFT | NEED_ABOVE,		// D117
	NEED_LEFT | NEED_ABOVE,		// D153
	NEED_LEFT,					// D207
	NEED_ABOVE | NEED_RIGHT,	// D63
	NEED_LEFT | NEED_ABOVE,		// TM

	NEED_ABOVE,					// DC_NO_LEFT
	NEED_LEFT,					// DC_NO_ABOVE,
	0,							// DC_NO_ADJ,
};

const uint8	vp9::b_width_log2_lookup[BLOCK_SIZES]		= {0, 0, 1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4};
const uint8	vp9::b_height_log2_lookup[BLOCK_SIZES]		= {0, 1, 0, 1, 2, 1, 2, 3, 2, 3, 4, 3, 4};

//-------------------------------------
//	GPU
//-------------------------------------

//#define PLAT_PS4
#define DEBUG_GPU	0

//#define DCT_FLOAT
#define SORT_PRED
//#define DCT_ALLOC

#ifdef PLAT_PS4
#include "D:\dev\orbiscrude\lib\liborbiscrude.h"
//#else
//typedef ComputeContext	ComputeQueue;
#endif

bool check(void *buffer0, uint32 stride0, void *buffer1, uint32 stride1, uint32 x, uint32 y, uint32 w, uint32 h) {
	uint8	*p0 = (uint8*)buffer0 + y * stride0 + x;
	uint8	*p1 = (uint8*)buffer1 + y * stride1 + x;
	for (int i = 0; i < h; i++) {
		if (memcmp(p0, p1, w) != 0) {
			for (int j = 0; j < w; j++) {
				if (p0[j] != p1[j])
					return false;
			}
			return false;
		}
		p0 += stride0;
		p1 += stride1;
	}
	return true;
}

struct chunked_allocator : public checking_linear_allocator {
	enum {MIN_SIZE = 16};
	struct chunk {
		chunk	*prev;
		void	*end;
		void	*data()	{ return this + 1; }
		size_t	size()	{ return (char*)end - (char*)data(); }
	};
	chunk	*prev;

	chunked_allocator() : prev(0) {}

	void		expand(void *_p, size_t size) {
		chunk	*c = (chunk*)_p;
		p		= (uint8*)c->data();
		end		= (uint8*)(c->end = (uint8*)_p + size);
		c->prev	= prev;
		prev	= c;
	}
	chunk*	flush() {
		chunk	*last = prev;
		if (last && p > last->data()) {
			last->end	= p;
			prev		= 0;

			size_t	remaining = (char*)end - (char*)p;
			if (remaining > sizeof(chunk) + MIN_SIZE)
				expand(p, remaining);
			else
				end = p = 0;
			return last;
		}
		return 0;
	}
};

namespace vp9 {
struct GPU {
	enum {
		DC_128	= -1,
		DC_NO_L	= INTRA_MODES,
		DC_NO_A,
		DC_127,
		DC_129,
		D45_NO_R,
		D135_NO_L,
		D135_NO_A,
		D117_NO_L,
		D117_NO_A,
		D153_NO_L,
		D153_NO_A,
		D63_NO_R,
		ALL_MODES,
	};
	enum TRANSFORM {
		TX_4_WHT_WHT,
		TX_4_DCT_DCT,
		TX_4_DCT_ADST,
		TX_4_ADST_DCT,
		TX_4_ADST_ADST,

		TX_8_DCT_DCT,
		TX_8_DCT_ADST,
		TX_8_ADST_DCT,
		TX_8_ADST_ADST,

		TX_16_DCT_DCT,
		TX_16_DCT_ADST,
		TX_16_ADST_DCT,
		TX_16_ADST_ADST,

		TX_32_DCT_DCT,

		TX_NUM,
	};

	#ifdef DCT_FLOAT
	typedef norm16			dct1;
	#else
	typedef rawint<int16>	dct1;
	#endif

	typedef simple_vec<dct1,4>	dct4;

	struct DCTinfo {
		uint32	eob:10, q0:11, q1:11;
		uint16	x, y;
		uint32	coeffs;
		DCTinfo(int _q0, int _q1, int _eob, uint32 _coeffs, int _x, int _y) : eob(_eob - 1), q0(_q0), q1(_q1), x(_x/4), y(_y), coeffs(_coeffs) {}
	};

	struct PREDinfo {
		uint32	mode:6, x:13, y:13;
		PREDinfo(uint8 _mode, int _x, int _y) : mode(_mode), x(_x), y(_y) {}
	};
	struct PREDinfo2 : PREDinfo {
		uint32	key;
		PREDinfo2(TX_SIZE tx_size, uint8 mode, int x, int y, int dependency) : PREDinfo(mode, x, y), key(dependency * 4 + tx_size) {}
		friend bool operator<(const PREDinfo2 &a, const PREDinfo2 &b) { return a.key < b.key; }
	};
	struct INTERinfo {
		uint32	filter:2, ref0:2, ref1:2, split:2, mvs:24;
	};

#ifdef PLAT_PS4
	struct DCTbuffers {
		Buffer<DCTinfo>		info;
		_Buffer				coeffs;
		_Texture			output;
	};
	struct PREDbuffers {
		Buffer<PREDinfo>	info;
		_Texture			dest;
	};
	struct PREDbuffers2 {
		Buffer<PREDinfo2>	info;
		_Texture			dest;
	};
	struct INTERbuffers {
		struct Textures {
			_Texture		dest;
			_Texture		src[3];
		};
		Buffer<INTERinfo>	info;
		_Buffer				mv;
		Textures			*textures;
	};
	struct YUVbuffers {
		_Texture			dest;
		_Texture			srce;
	};
#endif

	struct Label {
		volatile uint64 *label;
		uint64			value;

		Label() : label(0) {}

		void	Next(ComputeQueue &ctx) {
			++value;
		#ifdef PLAT_PS4
			ctx.WriteOnDone(label, value, FENCE_NONE);
			ctx.WaitLabel(label, value);
		#endif
		}

		void	Init() {
		#ifdef PLAT_PS4
			label	= (volatile uint64*)graphics.onion().alloc(sizeof(uint64), sizeof(uint64));
			*label	= value = 0;
		#endif
		}
		bool	Done(uint64 v) {
		#ifdef PLAT_PS4
			return *label >= v;
		#else
			return true;
		#endif
		}
	};

	struct Frame {
	#ifndef DCT_ALLOC
		dynamic_array<DCTinfo>		tx_buckets[TX_NUM];	//0x00000 0x10000 0x04000 0x08000 0x08000 0x10000 0x02000 0x08000 0x01000 0x04000 0x00800 0x01000 0x00400 0x01000
		spinlock					dct_mutex;
	#endif
	#ifndef SORT_PRED
		dynamic_array<dynamic_array<PREDinfo> >	pred_buckets[TX_SIZES];
	#endif
		dynamic_array<INTERinfo>	inter_info;			//0x2000
		uint32						coeffs_end;
		uint32						mvs_end;
		uint32						preds_end;
		uint32						dcts_end;
		uint64						done_gpu;

		Frame() : coeffs_end(0), mvs_end(0), dcts_end(0), done_gpu(0) {}

		void	Begin(GPU *gpu, int mi_rows, int mi_cols);
		void	End(GPU *gpu, int mi_rows, int mi_cols, _Texture *dest, _Texture *src0, _Texture *src1, _Texture *src2, PREDinfo2 *preds_begin);

	#ifndef DCT_ALLOC
		void	OutputDCT(TRANSFORM tx, int q0, int q1, int eob, uint32 coeffs, int x, int y) {
			with(dct_mutex), new(tx_buckets[tx]) DCTinfo(q0, q1, eob, coeffs, x, y);
		}
	#endif
	#ifndef SORT_PRED
		void	OutputIntraPred(TX_SIZE tx_size, uint8 mode, int x, int y, int dependency) {
			auto	w(with(pred_mutex));
			if (pred_buckets[tx_size].size() <= dependency)
				pred_buckets[tx_size].resize(dependency + 1);
			new(pred_buckets[tx_size][dependency]) PREDinfo(mode, x, y);
		}
	#endif
		void	OutputInterPred(INTERinfo &info, int mi_col, int mi_row, int mi_cols, int nx, int ny) {
			INTERinfo	*dest = inter_info + mi_row * mi_cols + mi_col;
			for (int y = 0; y < ny; ++y, dest += mi_cols)
				for (int x = 0; x < nx; ++x)
					dest[x] = info;
		}
	};

	static const int num_frames	= 4;
	static const int max_coeffs	= 2 * 1024 * 1024 * num_frames;
	static const int max_mvs	= 64 * 1024 * num_frames;
	static const int max_preds1	= 512 * 1024;
	static const int max_preds	= max_preds1 * num_frames;
	static const int max_dcts	= 128 * 1024 * num_frames;

	bool		init;
	Frame		frame[num_frames];
	int			frame_index;
	uint32		preds_begin;
	uint32		used_modes[ALL_MODES];

	dct1								*coeff_block;
	atomic<circular_allocator>			coeff_alloc;
	Buffer<dct1>						coeff_buffer;

	MotionVector						*mv_block;
	atomic<circular_allocator>			mv_alloc;
	Buffer<MotionVector>				mv_buffer;

	memory_block						pred_block;
	atomic<circular_allocator>			pred_alloc;

#ifdef DCT_ALLOC
	memory_block						dct_block;
	atomic<circular_allocator>			dct_alloc;
	chunked_allocator					dct_buckets[TX_NUM];	//0x00000 0x10000 0x04000 0x08000 0x08000 0x10000 0x02000 0x08000 0x01000 0x04000 0x00800 0x01000 0x00400 0x01000
#endif

	ref_ptr<FrameBuffer>				frame_refs[3];	//keep ref for gpu

	ComputeQueue		ctx;
	Label				label;

	GPU() : init(false), frame_index(0)
	#ifdef DCT_ALLOC
		, dct_block(0, 0), dct_alloc(dct_block)
	#endif
	{
		clear(used_modes);
	}

	Frame	&GetFrame() {
		return frame[frame_index];
	}

	void	Init();
	void	BeginFrame(Decoder *dec);
	void	EndFrame(Decoder *dec);

	void	OutputDCT(const int16 quant[2], int eob, tran_low_t *coeffs, TX_TYPE tx_type, TX_SIZE tx_size, bool lossless, int x, int y);
	void	OutputIntraPred(TX_SIZE tx_size, PREDICTION_MODE mode, int x, int y, int p,
		bool up_available, bool left_available, bool right_available,
		int mi_cols, int mi_rows,
		int *above_dependency, int *left_dependency
	);
	void	OutputInterPred(ModeInfo *mi, int mi_col, int mi_row, int mi_cols);

	tran_low_t	*AllocCoeffs(size_t n) {
//		ISO_OUTPUTF("alloc coeffs:%i\n", n);
		return coeff_alloc.alloc<tran_low_t>(n);
	}
};
}

#if DEBUG_GPU
void GetTexture(_Texture *tex, FrameBuffer *fb) {
	static _Texture	gpu_shadow[Common::FRAME_BUFFERS * 2];
	if (fb) {
		int	id = fb->id;
		ISO_ASSERT(id < num_elements(gpu_shadow));
		_Texture &tex0	= gpu_shadow[id];
		int		width	= fb->y.crop_width, height = fb->y.crop_height * 3 / 2;
		if (width != tex0.getWidth() || height != tex0.getHeight()) {
			SizeAlign	sa = Init(&tex0, GetTexFormat<rawint<uint8> >(), width, height, 1, 0, 1, MEM_LINEAR|MEM_SYSTEM|MEM_WRITABLE);
			graphics.free((void*)tex0.getBase());
			tex0.setBase(uintptr_t(graphics.alloc(sa.size, sa.align)));
		}
		*tex	= tex0;
	}
}
#elif defined(PLAT_PS4)
void GetTexture(Texture &tex, FrameBuffer *fb) {
	if (fb) {
		//Init(tex, GetTexFormat<rawint<uint8> >(), fb->y.width, fb->y.height * 3 / 2, 1, 0, 1, MEM_LINEAR|MEM_SYSTEM|MEM_WRITABLE);
		//tex->setBase(uintptr_t(fb->mem_buffer));
		tex = (const Texture&)fb->texture;
		ISO_ASSERT(tex.Data<void>());
	}
}
#else
void GetTexture(Texture &tex, FrameBuffer *fb) {
	if (fb) {
//		tex.Init(GetTexFormat<rawint<uint8> >(), fb->y.width, fb->y.height * 3 / 2, 1, 1, MEM_SYSTEM|MEM_WRITABLE);
		tex = (const Texture&)fb->texture;
	}
}
#endif

void GPU::Init() {
#ifdef PLAT_PS4
	ctx.Init(1024 * 1024 * 2, 1, 0);
	label.Init();

	coeff_buffer.Init(max_coeffs, MEM_SYSTEM);
	coeff_block	= coeff_buffer.Begin(ctx);
	coeff_alloc	= memory_block(coeff_block, max_coeffs * sizeof(dct1));

	mv_buffer.Init(max_mvs, MEM_SYSTEM);
	mv_block	= mv_buffer.Begin(ctx);
	mv_alloc	= memory_block(mv_block, max_mvs * sizeof(MotionVector));

	pred_block	= memory_block(graphics.alloc(MEM_SYSTEM, max_preds * sizeof(PREDinfo2), 4), max_preds * sizeof(PREDinfo2));
	pred_alloc	= pred_block;

#ifdef DCT_ALLOC
	dct_block	= memory_block(graphics.onion().alloc(max_dcts * sizeof(DCTinfo), 4), max_dcts * sizeof(DCTinfo));
	dct_alloc	= dct_block;
#endif

#else
//	ctx.Begin();

	coeff_buffer.Init(max_coeffs);
	coeff_block	= coeff_buffer.WriteData(ctx);
	coeff_alloc	= memory_block(coeff_block, max_coeffs * sizeof(dct1));

	mv_buffer.Init(max_mvs);
	mv_block	= mv_buffer.WriteData(ctx);
	mv_alloc	= memory_block(mv_block, max_mvs * sizeof(MotionVector));

	pred_block	= memory_block(aligned_alloc(max_preds * sizeof(PREDinfo2), 4), max_preds * sizeof(PREDinfo2));
	pred_alloc	= pred_block;

#ifdef DCT_ALLOC
	dct_block	= memory_block(aligned_alloc(max_dcts * sizeof(DCTinfo), 4), max_dcts * sizeof(DCTinfo));
	dct_alloc	= dct_block;
#endif

//	ctx.End();
#endif
}

void GPU::BeginFrame(Decoder *dec) {
	if (!init) {
		init = true;
		Init();
	}

#ifndef PLAT_PS4
	mv_alloc.relocate(mv_buffer.Begin(ctx));
	ptrdiff_t	diff = coeff_alloc.relocate(coeff_buffer.Begin(ctx)) / sizeof(tran_low_t);
	for (auto &td : dec->tile_data) {
		td.mb.dqcoeff		+= diff;
		td.mb.dqcoeff_end	+= diff;
	}
#endif

	for (int i = 0; i < 3; i++)
		frame_refs[i] = dec->frame_refs[i].buf;

#ifdef SORT_PRED
	preds_begin = pred_alloc.get_offset();
#endif

	GetFrame().Begin(this, dec->mi_rows, dec->mi_cols);
}

void GPU::EndFrame(Decoder *dec) {
	Texture		dest, tex_refs[3];
#ifdef PLAT_PS4
//	_Texture	_dest, _tex_refs[3];
//	_Texture	*dest = &_dest, *tex_refs[3] = {_tex_refs + 0, _tex_refs + 1, _tex_refs + 2};
#else
	coeff_buffer.End(ctx);
	mv_buffer.End(ctx);
#endif

	GetTexture(dest, dec->cur_frame);
	if (dec->frame_is_intra_only()) {
		clear(tex_refs);
	} else {
		for (int i = 0; i < 3; i++)
			GetTexture(tex_refs[i], frame_refs[i]);
	}
	frame[frame_index].End(this, dec->mi_rows, dec->mi_cols,
		dest,
		tex_refs[0],
		tex_refs[1],
		tex_refs[2],
		(PREDinfo2*)pred_alloc.to_pointer(preds_begin)
	);

#if DEBUG_GPU
	int	w	= dec->mi_cols * 8;
	int	h	= dec->mi_rows * (8 + 4);
	int	h0	= dec->mi_rows * 8;

	FrameBuffer	*fb		= dec->cur_frame;
	ctx.DingDong();
	Thread::sleep(.25f);
#if 1
	check(fb->y_buffer.buffer, fb->y_buffer.stride, (void*) dest.getBase(),   dest.getPitch(), 0, 0, w, dec->mi_rows * 8);
	check(fb->u_buffer.buffer, fb->u_buffer.stride, (void*)(dest.getBase() + dest.getPitch() * dec->mi_rows * 8),			dest.getPitch(), 0, 0, w / 2, dec->mi_rows * 4);
	check(fb->v_buffer.buffer, fb->v_buffer.stride, (void*)(dest.getBase() + dest.getPitch() * dec->mi_rows * 8 + w / 2),	dest.getPitch(), 0, 0, w / 2, dec->mi_rows * 4);

#elif 0
	FrameBuffer	*fb		= dec->cur_frame;
	ctx.DingDong();
	Thread::sleep(.25f);

	auto	&pred_buckets	= frame[frame_index].pred_buckets;
	int		num_submits		= 0;
	for (int i = 0; i < TX_SIZES; i++)
		num_submits = max(num_submits, pred_buckets[i].size());

	for (int i = 0; i < num_submits; i++) {
		for (int j = TX_SIZES; j--;) {
			if (size_t num = i < pred_buckets[j].size() ? pred_buckets[j][i].size() : 0) {
				for (auto k : pred_buckets[j][i]) {
					if (k.y < dec->mi_rows * 8)
						ISO_ASSERT(check(fb->y_buffer.buffer, fb->y_buffer.stride, (void*)dest->getBase(), dest->getPitch(), k.x, k.y, 4 << j, 4 << j));
				}
			}
		}
	}
#endif

	static _Texture	final;
	if (final.getWidth() != w || final.getHeight() != h0) {
		graphics.free((void*)final.getBase());
		SizeAlign	size	= iso::Init(&final, TEXF_R8G8B8A8, w, h0, 1, 0, 1, MEM_LINEAR|MEM_SYSTEM|MEM_WRITABLE);
		final.setBase(uintptr_t(graphics.alloc(size.size, size.align)));
	}

	PS4Shader	*yuv_conversion	= *root("data")["dct"]["yuv_conversion"][0];
	YUVbuffers	yuv_buffers;
	yuv_buffers.dest = final;
	yuv_buffers.srce = dest;

	ctx.SetShader(*yuv_conversion);
	ctx.SetSRT(&yuv_buffers);
	ctx.Dispatch(dec->mi_cols, dec->mi_rows, 1);

#if 0
	Surface		dst(dest);
	ctx.Blit(dest, Surface(GetTexFormat<rawint<uint8> >(), fb->y.crop_width, fb->y.crop_height, fb->y_buffer.stride, sce::Gnm::kTileModeDisplay_LinearAligned, fb->y_buffer.buffer));
	ctx.Blit(dest, Surface(GetTexFormat<rawint<uint8> >(), fb->uv.crop_width, fb->uv.crop_height, fb->u_buffer.stride, sce::Gnm::kTileModeDisplay_LinearAligned, fb->u_buffer.buffer), Point(0, dec->mi_rows * 8));
	ctx.Blit(dest, Surface(GetTexFormat<rawint<uint8> >(), fb->uv.crop_width, fb->uv.crop_height, fb->u_buffer.stride, sce::Gnm::kTileModeDisplay_LinearAligned, fb->v_buffer.buffer), Point(dec->mi_cols * 4, dec->mi_rows * 8));
#endif

#endif	// DEBUG_GPU

	ctx.DingDong();
#ifdef PLAT_PS4
	orbiscrude::flip();
#endif

	frame_index = (frame_index + 1) % num_elements(frame);
}

void GPU::Frame::Begin(GPU *gpu, int mi_rows, int mi_cols) {
	//ISO_OUTPUTF("label=%i; done=%i\n", *label.label, done_gpu);
	while (!gpu->label.Done(done_gpu))
		Thread::sleep(0.01f);
//	ISO_ALWAYS_ASSERT(gpu->label.Done(done_gpu));

	inter_info.resize(mi_rows * mi_cols);
	memset(inter_info, 0, inter_info.size() * 4);

#ifndef DCT_ALLOC
	for (int i = 0; i < TX_NUM; i++)
		tx_buckets[i].clear();
#endif

#ifndef SORT_PRED
	for (int i = 0; i < TX_SIZES; i++)
		pred_buckets[i].clear();
#endif
//	inter_mv.clear();
}

void GPU::Frame::End(GPU *gpu, int mi_rows, int mi_cols, _Texture *dest, _Texture *src0, _Texture *src1, _Texture *src2, PREDinfo2 *preds_begin) {
	static ISO::Browser2		dctb	= ISO::root("data")["vpx_dct"]["transforms"];
	static ISO::Browser2		interb	= ISO::root("data")["vpx_inter"]["inter_predictions"];
	static ISO::Browser2		intrab	= ISO::root("data")["vpx_intra"]["intra_predictions2"];

	if (coeffs_end)
		gpu->coeff_alloc.set_get_offset(coeffs_end);
	coeffs_end = gpu->coeff_alloc.get_offset();

	if (mvs_end)
		gpu->mv_alloc.set_get_offset(mvs_end);
	mvs_end = gpu->mv_alloc.get_offset();

	if (preds_end)
		gpu->pred_alloc.set_get_offset(preds_end);
	preds_end = gpu->pred_alloc.get_offset();

	ComputeQueue		&ctx	= gpu->ctx;
	Label				&label	= gpu->label;

	ctx.PushMarker("VP9 decode");

	//	CLEAR

	ctx.PushMarker("clear");
#ifdef PLAT_PS4
	_Texture	dest4		= *dest;
	dest4.setWidth(dest->getWidth() / 4);
	dest4.setPitch(dest->getPitch() / 4);
#if DCT_FLOAT
	dest4.setFormat(GetComponentType<unorm8[4]>());
	ctx.Fill(Surface(&dest4), colour(.5f,.5f,.5f,.5f));
#else
	dest4.setFormat(GetComponentType<rawint<uint8>[4]>());
	ctx.Fill(Surface(&dest4), force_cast<colour>(int32x4{128,128,128,128}));
#endif
	label.Next(ctx);
#else
	//dx11
#endif

	ctx.PopMarker();

	//	DCT

	ctx.PushMarker("dct");

#ifdef PLAT_PS4
	DCTbuffers		dct_buffers;
	dct_buffers.coeffs	= gpu->coeff_buffer;
	dct_buffers.output	= dest4;
#else
	ctx.SetBuffer(gpu->coeff_buffer, 0);
	ctx.SetRWTexture(dest, 0);
#endif

#ifdef DCT_ALLOC
	if (dcts_end)
		gpu->dct_alloc.set_get(dcts_end);
	dcts_end = gpu->dct_alloc.getp();
#endif

	for (int i = 0; i < TX_NUM; i++) {
	#ifdef DCT_ALLOC
		if (void *p = gpu->dct_buckets[i].getp()) {
			if (p < dcts_end) {
				if (dcts_end <= gpu->dct_alloc.getp())
					dcts_end = p;
			} else {
				if (p > gpu->dct_alloc.getp())
					dcts_end = p;
			}
		}
		if (chunked_allocator::chunk *a = gpu->dct_buckets[i].flush()) {
			PS4Shader	*cp	= *dctb[i];
			ctx.SetShader(*cp);
			do {
				int		num = a->size() / sizeof(DCTinfo);
				dct_buffers.info.Init((DCTinfo*)a->data(), num, MEM_USER | MEM_WRITABLE);
				ctx.SetSRT(&dct_buffers);
				ctx.Dispatch(num, 1, 1);
			} while (a = a->prev);
		}
	#else
		if (size_t num = tx_buckets[i].size()) {
			pass	*cp	= *dctb[i];
			ctx.SetShader(*cp);
		#ifdef PLAT_PS4
			dct_buffers.info.Init(tx_buckets[i], num, MEM_USER | MEM_WRITABLE);
			ctx.SetSRT(&dct_buffers);
		#else
			Buffer<DCTinfo>	info;
			info.Init(tx_buckets[i], num);
			ctx.SetBuffer(info, 1);
		#endif
			ctx.Dispatch(num, 1, 1);
		}
	#endif
	}

	label.Next(ctx);
	ctx.PopMarker();

	//	MOTION VECTORS

	if (src0) {
		ctx.PushMarker("inter prediction");

		pass	*inter_y	= *interb[0];
		pass	*inter_uv	= *interb[1];

		ctx.SetShader(*inter_y);

	#ifdef PLAT_PS4
		INTERbuffers	inter_buffers;
	//	inter_buffers.width		= cols;
		inter_buffers.mv		= gpu->mv_buffer;
		inter_buffers.info.Init(inter_info, mi_cols * mi_rows, MEM_USER);

		inter_buffers.textures	= ctx.allocator().get();
		inter_buffers.textures->dest	= *dest;
		inter_buffers.textures->src[0]	= *src0;
		inter_buffers.textures->src[1]	= *src1;
		inter_buffers.textures->src[2]	= *src2;
		ctx.SetSRT(&inter_buffers);
	#else
		//dx11
		Buffer<INTERinfo>	info(inter_info, mi_cols * mi_rows);
		ctx.SetBuffer(gpu->mv_buffer, 0);
		ctx.SetBuffer(info, 1);
		ctx.SetTexture(src0, 2);
		ctx.SetTexture(src1, 3);
		ctx.SetTexture(src2, 4);
		ctx.SetRWTexture(dest, 0);
	#endif
		ctx.Dispatch(mi_cols, mi_rows, 1);

		ctx.SetShader(*inter_uv);
	#ifdef PLAT_PS4
		ctx.SetSRT(&inter_buffers);
	#endif
		ctx.Dispatch(mi_cols, mi_rows, 1);
		label.Next(ctx);
		ctx.PopMarker();
	}

	//	INTRA PREDICTION

	ctx.PushMarker("intra prediction");
#ifdef SORT_PRED
	PREDinfo2	*start	= preds_begin;
	PREDinfo2	*end	= (PREDinfo2*)gpu->pred_alloc.to_pointer(preds_end);
	if (start != end) {
		pass		*pred_shader[4];
		for (int i = 0; i < 4; i++)
			pred_shader[i] = *intrab[i];

	#ifdef PLAT_PS4
		PREDbuffers2	pred_buffers;
		pred_buffers.dest = *dest;
	#else
		ctx.SetRWTexture(dest, 0);
	#endif

		if (start < end) {
			sort(start, end);
			int			key		= start->key;
			for (PREDinfo2 *i = start; i != end; ++i) {
				if (i->key != key) {
					ctx.SetShader(*pred_shader[key & 3]);
				#ifdef PLAT_PS4
					pred_buffers.info.Init(start, i - start, MEM_USER);
					ctx.SetSRT(&pred_buffers);
				#else
					Buffer<PREDinfo2>	info(start, i - start);
					ctx.SetBuffer(info, 0);
				#endif
					ctx.Dispatch(i - start, 1, 1);

					if ((key ^ i->key) >> 2)
						label.Next(ctx);

					key		= i->key;
					start	= i;
				}
			}
			if (start < end) {
				ctx.SetShader(*pred_shader[key & 3]);
			#ifdef PLAT_PS4
				pred_buffers.info.Init(start, end - start, MEM_USER);
				ctx.SetSRT(&pred_buffers);
			#else
				Buffer<PREDinfo2>	info(start, end - start);
				ctx.SetBuffer(info, 0);
			#endif
				ctx.Dispatch(end - start, 1, 1);
			}
		} else {
			PREDinfo2	*end1	= (PREDinfo2*)gpu->pred_block.end();
			PREDinfo2	*start2	= gpu->pred_block;
			sort(start, end1);
			sort(start2, end);
			int			key	= 0;

			while (start != end1 || start2 != end) {
				int	prev_key	= key;
				key				= start == end1 ? start2->key : start2 == end ? start->key : min(start->key, start2->key);

				if ((key ^ prev_key) >> 2)
					label.Next(ctx);

				PREDinfo2	*i1	= start;
				while (i1 != end1 && i1->key == key)
					++i1;

				PREDinfo2	*i2 = start2;
				while (i2 != end  && i2->key == key)
					++i2;

				ctx.SetShader(*pred_shader[key & 3]);

				if (int n1 = i1 - start) {
				#ifdef PLAT_PS4
					pred_buffers.info.Init(start, n1, MEM_USER);
					ctx.SetSRT(&pred_buffers);
				#else
					Buffer<PREDinfo2>	info(start, n1);
					ctx.SetBuffer(info, 0);
				#endif
					ctx.Dispatch(n1, 1, 1);
				}

				if (int n2 = i2 - start2) {
				#ifdef PLAT_PS4
					pred_buffers.info.Init(start2, n2, MEM_USER);
					ctx.SetSRT(&pred_buffers);
				#else
					Buffer<PREDinfo2>	info(start2, n2);
					ctx.SetBuffer(info, 0);
				#endif
					ctx.Dispatch(n2, 1, 1);
				}

				start	= i1;
				start2	= i2;
			}
		}
	}
#else
	PS4Shader				*pred_shader[4];
	for (int i = 0; i < 4; i++)
		pred_shader[i] = *intrab[i];

	int	num_submits = 0;
	for (int i = 0; i < TX_SIZES; i++)
		num_submits = max(num_submits, pred_buckets[i].size());

	PREDbuffers	pred_buffers;
	pred_buffers.dest = *dest;

	for (int i = 0; i < num_submits; i++) {
		ctx.PushMarker(format_string("step %i", i));
		for (int j = TX_SIZES; j--;) {
			if (size_t num = i < pred_buckets[j].size() ? pred_buckets[j][i].size() : 0) {
				pred_buffers.info.Init(pred_buckets[j][i], num, MEM_USER);
				ctx.SetShader(*pred_shader[j]);
				ctx.SetSRT(&pred_buffers);
				ctx.Dispatch(num, 1, 1);
			}
		}
		label.Next(ctx);
		ctx.PopMarker();
	}
#endif
	ctx.PopMarker();

	ctx.PopMarker();

	done_gpu	= label.value;
}

void GPU::OutputDCT(const int16 quant[2], int eob, tran_low_t *coeffs, TX_TYPE tx_type, TX_SIZE tx_size, bool lossless, int x, int y) {
	TRANSFORM	tx	= lossless ? TX_4_WHT_WHT : TRANSFORM(min(tx_size * 4 + tx_type + 1, TX_NUM - 1));
	int			q0	= quant[0];
	int			q1	= quant[1];

#ifdef DCT_FLOAT
	q0	*= (8 >> tx_size);
	q1	*= (8 >> tx_size);
#endif

#ifdef DCT_ALLOC
	chunked_allocator	&a = dct_buckets[tx];
	if (a.remaining() < sizeof(DCTinfo))
		a.expand(dct_alloc.alloc(8192), 8192);
	new(a) DCTinfo(q0, q1, eob, coeffs - coeff_block, x, y);
#else
	GetFrame().OutputDCT(tx, q0, q1, eob, coeffs - (tran_low_t*)coeff_block, x, y);
#endif
}

void GPU::OutputIntraPred(TX_SIZE tx_size, PREDICTION_MODE mode, int x, int y, int p,
	bool up_available, bool left_available, bool right_available,
	int mi_cols, int mi_rows,
	int *above_dependency, int *left_dependency
) {
	static const int8 no_leftright[INTRA_MODES] = {
		DC_NO_L,		// DC
		V_PRED,			// V
		DC_129,			// H
		D45_NO_R,		// D45
		D135_NO_L,		// D135
		D117_NO_L,		// D117
		D153_NO_L,		// D153
		DC_129,			// D207
		D63_NO_R,		// D63
		V_PRED,			// TM
	};

	static const int8 no_above[ALL_MODES] = {
		DC_NO_A,		// DC
		DC_127,			// V
		H_PRED,			// H
		DC_127,			// D45
		D135_NO_A,		// D135
		D117_NO_A,		// D117
		D153_NO_A,		// D153
		D207_PRED,		// D207
		DC_127,			// D63
		H_PRED,			// TM

		DC_128,			// DC_NO_L
		DC_128,			// DC_NO_A,
		DC_127,			// DC_127,
		DC_129,			// DC_129,
		DC_127,			// D45_NO_R,
		DC_127,			// D135_NO_L,
		DC_127,			// D135_NO_A,
		DC_127,			// D117_NO_L,
		DC_127,			// D117_NO_A,
		DC_127,			// D153_NO_L,
		DC_127,			// D153_NO_A,
		DC_127,			// D63_NO_R,
	};

	//ISO_ASSERT(!(x==2576 && y==932));

	left_dependency		+= (y & 63) / 4;
	above_dependency	+= x / 4;

	int		dependency	= 0;
	int		bs			= 1 << tx_size;
	int		mode2		= mode;
	uint8	needs		= extend_modes[mode];

	if (needs & NEED_LEFT) {
		if (left_available) {
			for (int i = 0; i < bs; i++)
				dependency = max(dependency, left_dependency[i]);
		} else {
			mode2 = no_leftright[mode2];
		}
	}

	if (needs & NEED_ABOVE) {
		if (up_available) {
			for (int i = 0; i < bs; i++)
				dependency = max(dependency, above_dependency[i]);
			if (needs & NEED_RIGHT) {
				if (right_available)
					dependency = max(dependency, above_dependency[bs]);	// just one more
				else
					mode2 = no_leftright[mode2];
			}
		} else {
			mode2 = no_above[mode2];
		}
	}

	if (mode2 < 0)
		return;

	used_modes[mode2]++;

	if (up_available && (extend_modes[mode] & NEED_ABOVE)) {
		for (int i = 0; i < bs; i++)
			dependency = max(dependency, above_dependency[i]);
		if (right_available && (extend_modes[mode] & NEED_RIGHT))
			dependency = max(dependency, above_dependency[bs]);	// just one more
	}

	for (int i = 0; i < bs; i++)
		above_dependency[i] = left_dependency[i] = dependency + 1;

	new(pred_alloc) PREDinfo2(tx_size, mode2, x + (p == 2 ? mi_cols * 4 : 0), y + (p != 0 ? mi_rows * 8 : 0), dependency);
}

void GPU::OutputInterPred(ModeInfo *mi, int mi_col, int mi_row, int mi_cols) {
	static const struct {uint8 w, h, n, s;} block_info[] {
		{1,	1, 4, 3},	//BLOCK_4X4			= 0,
		{1,	1, 2, 2},	//BLOCK_4X8			= 1,
		{1,	1, 2, 1},	//BLOCK_8X4			= 2,
		{1,	1, 1, 0},	//BLOCK_8X8			= 3,
		{1,	2, 1, 0},	//BLOCK_8X16		= 4,
		{2,	1, 1, 0},	//BLOCK_16X8		= 5,
		{2,	2, 1, 0},	//BLOCK_16X16		= 6,
		{2,	4, 1, 0},	//BLOCK_16X32		= 7,
		{4,	2, 1, 0},	//BLOCK_32X16		= 8,
		{4,	4, 1, 0},	//BLOCK_32X32		= 9,
		{4,	8, 1, 0},	//BLOCK_32X64		= 10,
		{8,	4, 1, 0},	//BLOCK_64X32		= 11,
		{8,	8, 1, 0},	//BLOCK_64X64		= 12,
	};

	const BLOCK_SIZE	bsize	= mi->sb_type;
	bool				has2	= mi->has_second_ref();
	int					n		= block_info[bsize].n << int(has2);
	MotionVector		*mv		= mv_alloc.alloc<MotionVector>(n);

	INTERinfo	info;
	info.filter	= mi->interp_filter;
	info.ref0	= mi->ref_frame[0];
	info.ref1	= mi->ref_frame[1];
	info.split	= block_info[bsize].s;
	info.mvs	= mv - mv_block;

	GetFrame().OutputInterPred(info, mi_col, mi_row, mi_cols, block_info[bsize].w, block_info[bsize].h);

	if (bsize < BLOCK_8X8) {
		*mv++ = mi->sub_mv[0].mv[0];
		if (has2)
			*mv++ = mi->sub_mv[0].mv[1];
		if (!(bsize & 1)) {
			*mv++ = mi->sub_mv[1].mv[0];
			if (has2)
				*mv++ = mi->sub_mv[1].mv[1];
		}
		if (!(bsize & 2)) {
			*mv++ = mi->sub_mv[2].mv[0];
			if (has2)
				*mv++ = mi->sub_mv[2].mv[1];
		}
	}
	*mv++ = mi->sub_mv[3].mv[0];
	if (has2)
		*mv++ = mi->sub_mv[3].mv[1];

}

//GPU gpu;


//-------------------------------------
//	scans
//-------------------------------------

// Neighborhood 5-tuples for various scans and blocksizes, in {top, left, topleft, topright, bottomleft} order for each position in raster scan order.
// -1 indicates the neighbor does not exist.

#define MAX_NEIGHBORS 2

DECLARE_ALIGNED(16, static const int16, default_scan_4x4[16]) = {
	 0,  4,  1,  5,
	 8,  2, 12,  9,
	 3,  6, 13, 10,
	 7, 14, 11, 15,
};
DECLARE_ALIGNED(16, static const int16, default_iscan_4x4[16]) = {
	 0,  2,  5,  8,
	 1,  3,  9, 12,
	 4,  7, 11, 14,
	 6, 10, 13, 15,
};
DECLARE_ALIGNED(16, static int16, default_scan_4x4_neighbors[17 * MAX_NEIGHBORS]) = {
	 0,  0,  0,  0,  0,  0,  1,  4,
	 4,  4,	 1,  1,  8,  8,  5,  8,
	 2,  2,	 2,  5,  9, 12,  6,  9,
	 3,  6,	10, 13,  7, 10, 11, 14,
	 0,  0,
};

DECLARE_ALIGNED(16, static const int16, col_scan_4x4[16]) = {
	 0,  4,  8,  1,
	12,  5,  9,  2,
	13,  6, 10,  3,
	 7, 14, 11, 15,
};
DECLARE_ALIGNED(16, static const int16, col_iscan_4x4[16]) = {
	 0,  3,  7, 11,
	 1,  5,  9, 12,
	 2,  6, 10, 14,
	 4,  8, 13, 15,
};
DECLARE_ALIGNED(16, static int16, col_scan_4x4_neighbors[17 * MAX_NEIGHBORS]) = {
	0,  0,  0,  0,  4,  4,  0,  0,
	8,  8,  1,  1,  5,  5,  1,  1,
	9,  9,  2,  2,  6,  6,  2,  2,
	3,	3, 10, 10,  7,  7, 11, 11,
	0,  0,
};

DECLARE_ALIGNED(16, static const int16, row_scan_4x4[16]) = {
	 0,  1,  4,  2,
	 5,  3,  6,  8,
	 9,  7, 12, 10,
	13, 11, 14, 15,
};
DECLARE_ALIGNED(16, static const int16, row_iscan_4x4[16]) = {
	 0,  1,  3,  5,
	 2,  4,  6,  9,
	 7,  8, 11, 13,
	10, 12, 14, 15,
};
DECLARE_ALIGNED(16, static int16, row_scan_4x4_neighbors[17 * MAX_NEIGHBORS]) = {
	 0,  0,  0,  0,  0,  0,  1,  1,
	 4,  4,  2,  2,  5,  5,  4,  4,
	 8,  8,  6,  6,  8,  8,  9,  9,
	12, 12, 10, 10, 13, 13, 14, 14,
	 0,  0,
};

DECLARE_ALIGNED(16, static const int16, default_scan_8x8[64]) = {
	 0,  8,  1, 16,  9,  2, 17, 24,
	10,  3, 18, 25, 32, 11,  4, 26,
	33, 19, 40, 12, 34, 27,  5, 41,
	20, 48, 13, 35, 42, 28, 21,  6,
	49, 56, 36, 43, 29,  7, 14, 50,
	57, 44, 22, 37, 15, 51, 58, 30,
	45, 23, 52, 59, 38, 31, 60, 53,
	46, 39, 61, 54, 47, 62, 55, 63,
};
DECLARE_ALIGNED( 16, static const int16, default_iscan_8x8[64]) = {
	 0,  2,  5,  9, 14, 22, 31, 37,
	 1,  4,  8, 13, 19, 26, 38, 44,
	 3,  6, 10, 17, 24, 30, 42, 49,
	 7, 11, 15, 21, 29, 36, 47, 53,
	12, 16, 20, 27, 34, 43, 52, 57,
	18, 23, 28, 35, 41, 48, 56, 60,
	25, 32, 39, 45, 50, 55, 59, 62,
	33, 40, 46, 51, 54, 58, 61, 63,
};
DECLARE_ALIGNED(16, static int16, default_scan_8x8_neighbors[65 * MAX_NEIGHBORS]) = {
	 0,  0,  0,  0,  0,  0,  8,  8,  1,  8,  1,  1,  9, 16, 16, 16,
	 2,  9,  2,  2, 10, 17, 17, 24, 24, 24,  3, 10,  3,  3, 18, 25,
	25, 32, 11, 18, 32, 32,  4, 11, 26, 33, 19, 26,  4,  4, 33, 40,
	12, 19, 40, 40,  5, 12, 27, 34, 34, 41, 20, 27, 13, 20,  5,  5,
	41, 48, 48, 48, 28, 35, 35, 42, 21, 28,  6,  6,  6, 13, 42, 49,
	49, 56, 36, 43, 14, 21, 29, 36,  7, 14, 43, 50, 50, 57, 22, 29,
	37, 44, 15, 22, 44, 51, 51, 58, 30, 37, 23, 30, 52, 59, 45, 52,
	38, 45, 31, 38, 53, 60, 46, 53, 39, 46, 54, 61, 47, 54, 55, 62,
	 0,  0,
};

DECLARE_ALIGNED(16, static const int16, col_scan_8x8[64]) = {
	 0,  8, 16,  1, 24,  9, 32, 17,
	 2, 40, 25, 10, 33, 18, 48,  3,
	26, 41, 11, 56, 19, 34,  4, 49,
	27, 42, 12, 35, 20, 57, 50, 28,
	 5, 43, 13, 36, 58, 51, 21, 44,
	 6, 29, 59, 37, 14, 52, 22,  7,
	45, 60, 30, 15, 38, 53, 23, 46,
	31, 61, 39, 54, 47, 62, 55, 63,
};
DECLARE_ALIGNED( 16, static const int16, col_iscan_8x8[64]) = {
	 0,  3,  8, 15, 22, 32, 40, 47,
	 1,  5, 11, 18, 26, 34, 44, 51,
	 2,  7, 13, 20, 28, 38, 46, 54,
	 4, 10, 16, 24, 31, 41, 50, 56,
	 6, 12, 21, 27, 35, 43, 52, 58,
	 9, 17, 25, 33, 39, 48, 55, 60,
	14, 23, 30, 37, 45, 53, 59, 62,
	19, 29, 36, 42, 49, 57, 61, 63,
};
DECLARE_ALIGNED(16, static int16, col_scan_8x8_neighbors[65 * MAX_NEIGHBORS]) = {
	 0,  0,  0,  0,  8,  8,  0,  0, 16, 16,  1,  1, 24, 24,  9,  9,
	 1,  1, 32, 32, 17, 17,  2,  2, 25, 25, 10, 10, 40, 40,  2,  2,
	18, 18, 33, 33,  3,  3, 48, 48, 11, 11, 26, 26,  3,  3, 41, 41,
	19, 19, 34, 34,  4,  4, 27, 27, 12, 12, 49, 49, 42, 42, 20, 20,
	 4,  4, 35, 35,  5,  5, 28, 28, 50, 50, 43, 43, 13, 13, 36, 36,
	 5,  5, 21, 21, 51, 51, 29, 29,  6,  6, 44, 44, 14, 14,  6,  6,
	37, 37, 52, 52, 22, 22,  7,  7, 30, 30, 45, 45, 15, 15, 38, 38,
	23, 23, 53, 53, 31, 31, 46, 46, 39, 39, 54, 54, 47, 47, 55, 55,
	 0,  0,
};

DECLARE_ALIGNED(16, static const int16, row_scan_8x8[64]) = {
	 0,  1,  2,  8,  9,  3, 16, 10,
	 4, 17, 11, 24,  5, 18, 25, 12,
	19, 26, 32,  6, 13, 20, 33, 27,
	 7, 34, 40, 21, 28, 41, 14, 35,
	48, 42, 29, 36, 49, 22, 43, 15,
	56, 37, 50, 44, 30, 57, 23, 51,
	58, 45, 38, 52, 31, 59, 53, 46,
	60, 39, 61, 47, 54, 55, 62, 63,
};
DECLARE_ALIGNED( 16, static const int16, row_iscan_8x8[64]) = {
	 0,  1,  2,  5,  8, 12, 19, 24,
	 3,  4,  7, 10, 15, 20, 30, 39,
	 6,  9, 13, 16, 21, 27, 37, 46,
	11, 14, 17, 23, 28, 34, 44, 52,
	18, 22, 25, 31, 35, 41, 50, 57,
	26, 29, 33, 38, 43, 49, 55, 59,
	32, 36, 42, 47, 51, 54, 60, 61,
	40, 45, 48, 53, 56, 58, 62, 63,
};
DECLARE_ALIGNED(16, static int16, row_scan_8x8_neighbors[65 * MAX_NEIGHBORS]) = {
	 0,  0,  0,  0,  1,  1,  0,  0,  8,  8,  2,  2,  8,  8,  9,  9,
	 3,  3, 16, 16, 10, 10, 16, 16,  4,  4, 17, 17, 24, 24, 11, 11,
	18, 18, 25, 25, 24, 24,  5,  5, 12, 12, 19, 19, 32, 32, 26, 26,
	 6,  6, 33, 33, 32, 32, 20, 20, 27, 27, 40, 40, 13, 13, 34, 34,
	40, 40, 41, 41, 28, 28, 35, 35, 48, 48, 21, 21, 42, 42, 14, 14,
	48, 48, 36, 36, 49, 49, 43, 43, 29, 29, 56, 56, 22, 22, 50, 50,
	57, 57, 44, 44, 37, 37, 51, 51, 30, 30, 58, 58, 52, 52, 45, 45,
	59, 59, 38, 38, 60, 60, 46, 46, 53, 53, 54, 54, 61, 61, 62, 62,
	 0,  0,
};

DECLARE_ALIGNED(16, static const int16, default_scan_16x16[256]) = {
	  0,  16,   1,  32,  17,   2,  48,  33,  18,   3,  64,  34,  49,  19,  65,  80,
	 50,   4,  35,  66,  20,  81,  96,  51,   5,  36,  82,  97,  67, 112,  21,  52,
	 98,  37,  83, 113,   6,  68, 128,  53,  22,  99, 114,  84,   7, 129,  38,  69,
	100, 115, 144, 130,  85,  54,  23,   8, 145,  39,  70, 116, 101, 131, 160, 146,
	 55,  86,  24,  71, 132, 117, 161,  40,   9, 102, 147, 176, 162,  87,  56,  25,
	133, 118, 177, 148,  72, 103,  41, 163,  10, 192, 178,  88,  57, 134, 149, 119,
	 26, 164,  73, 104, 193,  42, 179, 208,  11, 135,  89, 165, 120, 150,  58, 194,
	180,  27,  74, 209, 105, 151, 136,  43,  90, 224, 166, 195, 181, 121, 210,  59,
	 12, 152, 106, 167, 196,  75, 137, 225, 211, 240, 182, 122,  91,  28, 197,  13,
	226, 168, 183, 153,  44, 212, 138, 107, 241,  60,  29, 123, 198, 184, 227, 169,
	242,  76, 213, 154,  45,  92,  14, 199, 139,  61, 228, 214, 170, 185, 243, 108,
	 77, 155,  30,  15, 200, 229, 124, 215, 244,  93,  46, 186, 171, 201, 109, 140,
	230,  62, 216, 245,  31, 125,  78, 156, 231,  47, 187, 202, 217,  94, 246, 141,
	 63, 232, 172, 110, 247, 157,  79, 218, 203, 126, 233, 188, 248,  95, 173, 142,
	219, 111, 249, 234, 158, 127, 189, 204, 250, 235, 143, 174, 220, 205, 159, 251,
	190, 221, 175, 236, 237, 191, 206, 252, 222, 253, 207, 238, 223, 254, 239, 255,
};
DECLARE_ALIGNED( 16, static const int16, default_iscan_16x16[256]) = {
	  0,   2,   5,   9,  17,  24,  36,  44,  55,  72,  88, 104, 128, 143, 166, 179,
	  1,   4,   8,  13,  20,  30,  40,  54,  66,  79,  96, 113, 141, 154, 178, 196,
	  3,   7,  11,  18,  25,  33,  46,  57,  71,  86, 101, 119, 148, 164, 186, 201,
	  6,  12,  16,  23,  31,  39,  53,  64,  78,  92, 110, 127, 153, 169, 193, 208,
	 10,  14,  19,  28,  37,  47,  58,  67,  84,  98, 114, 133, 161, 176, 198, 214,
	 15,  21,  26,  34,  43,  52,  65,  77,  91, 106, 120, 140, 165, 185, 205, 221,
	 22,  27,  32,  41,  48,  60,  73,  85,  99, 116, 130, 151, 175, 190, 211, 225,
	 29,  35,  42,  49,  59,  69,  81,  95, 108, 125, 139, 155, 182, 197, 217, 229,
	 38,  45,  51,  61,  68,  80,  93, 105, 118, 134, 150, 168, 191, 207, 223, 234,
	 50,  56,  63,  74,  83,  94, 109, 117, 129, 147, 163, 177, 199, 213, 228, 238,
	 62,  70,  76,  87,  97, 107, 122, 131, 145, 159, 172, 188, 210, 222, 235, 242,
	 75,  82,  90, 102, 112, 124, 138, 146, 157, 173, 187, 202, 219, 230, 240, 245,
	 89, 100, 111, 123, 132, 142, 156, 167, 180, 189, 203, 216, 231, 237, 246, 250,
	103, 115, 126, 136, 149, 162, 171, 183, 194, 204, 215, 224, 236, 241, 248, 252,
	121, 135, 144, 158, 170, 181, 192, 200, 209, 218, 227, 233, 243, 244, 251, 254,
	137, 152, 160, 174, 184, 195, 206, 212, 220, 226, 232, 239, 247, 249, 253, 255,
};
DECLARE_ALIGNED(16, static int16, default_scan_16x16_neighbors[257 * MAX_NEIGHBORS]) = {
	  0,   0,   0,   0,   0,   0,  16,  16,   1,  16,   1,   1,  32,  32,  17,  32,   2,  17,   2,   2,  48,  48,  18,  33,  33,  48,   3,  18,  49,  64,  64,  64,
	 34,  49,   3,   3,  19,  34,  50,  65,   4,  19,  65,  80,  80,  80,  35,  50,   4,   4,  20,  35,  66,  81,  81,  96,  51,  66,  96,  96,   5,  20,  36,  51,
	 82,  97,  21,  36,  67,  82,  97, 112,   5,   5,  52,  67, 112, 112,  37,  52,   6,  21,  83,  98,  98, 113,  68,  83,   6,   6, 113, 128,  22,  37,  53,  68,
	 84,  99,  99, 114, 128, 128, 114, 129,  69,  84,  38,  53,   7,  22,   7,   7, 129, 144,  23,  38,  54,  69, 100, 115,  85, 100, 115, 130, 144, 144, 130, 145,
	 39,  54,  70,  85,   8,  23,  55,  70, 116, 131, 101, 116, 145, 160,  24,  39,   8,   8,  86, 101, 131, 146, 160, 160, 146, 161,  71,  86,  40,  55,   9,  24,
	117, 132, 102, 117, 161, 176, 132, 147,  56,  71,  87, 102,  25,  40, 147, 162,   9,   9, 176, 176, 162, 177,  72,  87,  41,  56, 118, 133, 133, 148, 103, 118,
	 10,  25, 148, 163,  57,  72,  88, 103, 177, 192,  26,  41, 163, 178, 192, 192,  10,  10, 119, 134,  73,  88, 149, 164, 104, 119, 134, 149,  42,  57, 178, 193,
	164, 179,  11,  26,  58,  73, 193, 208,  89, 104, 135, 150, 120, 135,  27,  42,  74,  89, 208, 208, 150, 165, 179, 194, 165, 180, 105, 120, 194, 209,  43,  58,
	 11,  11, 136, 151,  90, 105, 151, 166, 180, 195,  59,  74, 121, 136, 209, 224, 195, 210, 224, 224, 166, 181, 106, 121,  75,  90,  12,  27, 181, 196,  12,  12,
	210, 225, 152, 167, 167, 182, 137, 152,  28,  43, 196, 211, 122, 137,  91, 106, 225, 240,  44,  59,  13,  28, 107, 122, 182, 197, 168, 183, 211, 226, 153, 168,
	226, 241,  60,  75, 197, 212, 138, 153,  29,  44,  76,  91,  13,  13, 183, 198, 123, 138,  45,  60, 212, 227, 198, 213, 154, 169, 169, 184, 227, 242,  92, 107,
	 61,  76, 139, 154,  14,  29,  14,  14, 184, 199, 213, 228, 108, 123, 199, 214, 228, 243,  77,  92,  30,  45, 170, 185, 155, 170, 185, 200,  93, 108, 124, 139,
	214, 229,  46,  61, 200, 215, 229, 244,  15,  30, 109, 124,  62,  77, 140, 155, 215, 230,  31,  46, 171, 186, 186, 201, 201, 216,  78,  93, 230, 245, 125, 140,
	 47,  62, 216, 231, 156, 171,  94, 109, 231, 246, 141, 156,  63,  78, 202, 217, 187, 202, 110, 125, 217, 232, 172, 187, 232, 247,  79,  94, 157, 172, 126, 141,
	203, 218,  95, 110, 233, 248, 218, 233, 142, 157, 111, 126, 173, 188, 188, 203, 234, 249, 219, 234, 127, 142, 158, 173, 204, 219, 189, 204, 143, 158, 235, 250,
	174, 189, 205, 220, 159, 174, 220, 235, 221, 236, 175, 190, 190, 205, 236, 251, 206, 221, 237, 252, 191, 206, 222, 237, 207, 222, 238, 253, 223, 238, 239, 254,
	  0,   0,
};

DECLARE_ALIGNED( 16, static const int16, col_scan_16x16[256]) = {
	  0,  16,  32,  48,   1,  64,  17,  80,  33,  96,  49,   2,  65, 112,  18,  81,
	 34, 128,  50,  97,   3,  66, 144,  19, 113,  35,  82, 160,  98,  51, 129,   4,
	 67, 176,  20, 114, 145,  83,  36,  99, 130,  52, 192,   5, 161,  68, 115,  21,
	146,  84, 208, 177,  37, 131, 100,  53, 162, 224,  69,   6, 116, 193, 147,  85,
	 22, 240, 132,  38, 178, 101, 163,  54, 209, 117,  70,   7, 148, 194,  86, 179,
	225,  23, 133,  39, 164,   8, 102, 210, 241,  55, 195, 118, 149,  71, 180,  24,
	 87, 226, 134, 165, 211,  40, 103,  56,  72, 150, 196, 242, 119,   9, 181, 227,
	 88, 166,  25, 135,  41, 104, 212,  57, 151, 197, 120,  73, 243, 182, 136, 167,
	213,  89,  10, 228, 105, 152, 198,  26,  42, 121, 183, 244, 168,  58, 137, 229,
	 74, 214,  90, 153, 199, 184,  11, 106, 245,  27, 122, 230, 169,  43, 215,  59,
	200, 138, 185, 246,  75,  12,  91, 154, 216, 231, 107,  28,  44, 201, 123, 170,
	 60, 247, 232,  76, 139,  13,  92, 217, 186, 248, 155, 108,  29, 124,  45, 202,
	233, 171,  61,  14,  77, 140,  15, 249,  93,  30, 187, 156, 218,  46, 109, 125,
	 62, 172,  78, 203,  31, 141, 234,  94,  47, 188,  63, 157, 110, 250, 219,  79,
	126, 204, 173, 142,  95, 189, 111, 235, 158, 220, 251, 127, 174, 143, 205, 236,
	159, 190, 221, 252, 175, 206, 237, 191, 253, 222, 238, 207, 254, 223, 239, 255,
};
DECLARE_ALIGNED( 16, static const int16, col_iscan_16x16[256]) = {
	  0,   4,  11,  20,  31,  43,  59,  75,  85, 109, 130, 150, 165, 181, 195, 198,
	  1,   6,  14,  23,  34,  47,  64,  81,  95, 114, 135, 153, 171, 188, 201, 212,
	  2,   8,  16,  25,  38,  52,  67,  83, 101, 116, 136, 157, 172, 190, 205, 216,
	  3,  10,  18,  29,  41,  55,  71,  89, 103, 119, 141, 159, 176, 194, 208, 218,
	  5,  12,  21,  32,  45,  58,  74,  93, 104, 123, 144, 164, 179, 196, 210, 223,
	  7,  15,  26,  37,  49,  63,  78,  96, 112, 129, 146, 166, 182, 200, 215, 228,
	  9,  19,  28,  39,  54,  69,  86, 102, 117, 132, 151, 170, 187, 206, 220, 230,
	 13,  24,  35,  46,  60,  73,  91, 108, 122, 137, 154, 174, 189, 207, 224, 235,
	 17,  30,  40,  53,  66,  82,  98, 115, 126, 142, 161, 180, 197, 213, 227, 237,
	 22,  36,  48,  62,  76,  92, 105, 120, 133, 147, 167, 186, 203, 219, 232, 240,
	 27,  44,  56,  70,  84,  99, 113, 127, 140, 156, 175, 193, 209, 226, 236, 244,
	 33,  51,  68,  79,  94, 110, 125, 138, 149, 162, 184, 202, 217, 229, 241, 247,
	 42,  61,  77,  90, 106, 121, 134, 148, 160, 173, 191, 211, 225, 238, 245, 251,
	 50,  72,  87, 100, 118, 128, 145, 158, 168, 183, 204, 222, 233, 242, 249, 253,
	 57,  80,  97, 111, 131, 143, 155, 169, 178, 192, 214, 231, 239, 246, 250, 254,
	 65,  88, 107, 124, 139, 152, 163, 177, 185, 199, 221, 234, 243, 248, 252, 255,
};
DECLARE_ALIGNED(16, static int16, col_scan_16x16_neighbors[257 * MAX_NEIGHBORS]) = {
	  0,   0,   0,   0,  16,  16,  32,  32,   0,   0,  48,  48,   1,   1,  64,  64,  17,  17,  80,  80,  33,  33,   1,   1,  49,  49,  96,  96,   2,   2,  65,  65,
	 18,  18, 112, 112,  34,  34,  81,  81,   2,   2,  50,  50, 128, 128,   3,   3,  97,  97,  19,  19,  66,  66, 144, 144,  82,  82,  35,  35, 113, 113,   3,   3,
	 51,  51, 160, 160,   4,   4,  98,  98, 129, 129,  67,  67,  20,  20,  83,  83, 114, 114,  36,  36, 176, 176,   4,   4, 145, 145,  52,  52,  99,  99,   5,   5,
	130, 130,  68,  68, 192, 192, 161, 161,  21,  21, 115, 115,  84,  84,  37,  37, 146, 146, 208, 208,  53,  53,   5,   5, 100, 100, 177, 177, 131, 131,  69,  69,
	  6,   6, 224, 224, 116, 116,  22,  22, 162, 162,  85,  85, 147, 147,  38,  38, 193, 193, 101, 101,  54,  54,   6,   6, 132, 132, 178, 178,  70,  70, 163, 163,
	209, 209,   7,   7, 117, 117,  23,  23, 148, 148,   7,   7,  86,  86, 194, 194, 225, 225,  39,  39, 179, 179, 102, 102, 133, 133,  55,  55, 164, 164,   8,   8,
	 71,  71, 210, 210, 118, 118, 149, 149, 195, 195,  24,  24,  87,  87,  40,  40,  56,  56, 134, 134, 180, 180, 226, 226, 103, 103,   8,   8, 165, 165, 211, 211,
	 72,  72, 150, 150,   9,   9, 119, 119,  25,  25,  88,  88, 196, 196,  41,  41, 135, 135, 181, 181, 104, 104,  57,  57, 227, 227, 166, 166, 120, 120, 151, 151,
	197, 197,  73,  73,   9,   9, 212, 212,  89,  89, 136, 136, 182, 182,  10,  10,  26,  26, 105, 105, 167, 167, 228, 228, 152, 152,  42,  42, 121, 121, 213, 213,
	 58,  58, 198, 198,  74,  74, 137, 137, 183, 183, 168, 168,  10,  10,  90,  90, 229, 229,  11,  11, 106, 106, 214, 214, 153, 153,  27,  27, 199, 199,  43,  43,
	184, 184, 122, 122, 169, 169, 230, 230,  59,  59,  11,  11,  75,  75, 138, 138, 200, 200, 215, 215,  91,  91,  12,  12,  28,  28, 185, 185, 107, 107, 154, 154,
	 44,  44, 231, 231, 216, 216,  60,  60, 123, 123,  12,  12,  76,  76, 201, 201, 170, 170, 232, 232, 139, 139,  92,  92,  13,  13, 108, 108,  29,  29, 186, 186,
	217, 217, 155, 155,  45,  45,  13,  13,  61,  61, 124, 124,  14,  14, 233, 233,  77,  77,  14,  14, 171, 171, 140, 140, 202, 202,  30,  30,  93,  93, 109, 109,
	 46,  46, 156, 156,  62,  62, 187, 187,  15,  15, 125, 125, 218, 218,  78,  78,  31,  31, 172, 172,  47,  47, 141, 141,  94,  94, 234, 234, 203, 203,  63,  63,
	110, 110, 188, 188, 157, 157, 126, 126,  79,  79, 173, 173,  95,  95, 219, 219, 142, 142, 204, 204, 235, 235, 111, 111, 158, 158, 127, 127, 189, 189, 220, 220,
	143, 143, 174, 174, 205, 205, 236, 236, 159, 159, 190, 190, 221, 221, 175, 175, 237, 237, 206, 206, 222, 222, 191, 191, 238, 238, 207, 207, 223, 223, 239, 239,
	  0,   0,
};

DECLARE_ALIGNED( 16, static const int16, row_scan_16x16[256]) = {
	  0,   1,   2,  16,   3,  17,   4,  18,  32,   5,  33,  19,   6,  34,  48,  20,
	 49,   7,  35,  21,  50,  64,   8,  36,  65,  22,  51,  37,  80,   9,  66,  52,
	 23,  38,  81,  67,  10,  53,  24,  82,  68,  96,  39,  11,  54,  83,  97,  69,
	 25,  98,  84,  40, 112,  55,  12,  70,  99, 113,  85,  26,  41,  56, 114, 100,
	 13,  71, 128,  86,  27, 115, 101, 129,  42,  57,  72, 116,  14,  87, 130, 102,
	144,  73, 131, 117,  28,  58,  15,  88,  43, 145, 103, 132, 146, 118,  74, 160,
	 89, 133, 104,  29,  59, 147, 119,  44, 161, 148,  90, 105, 134, 162, 120, 176,
	 75, 135, 149,  30,  60, 163, 177,  45, 121,  91, 106, 164, 178, 150, 192, 136,
	165, 179,  31, 151, 193,  76, 122,  61, 137, 194, 107, 152, 180, 208,  46, 166,
	167, 195,  92, 181, 138, 209, 123, 153, 224, 196,  77, 168, 210, 182, 240, 108,
	197,  62, 154, 225, 183, 169, 211,  47, 139,  93, 184, 226, 212, 241, 198, 170,
	124, 155, 199,  78, 213, 185, 109, 227, 200,  63, 228, 242, 140, 214, 171, 186,
	156, 229, 243, 125,  94, 201, 244, 215, 216, 230, 141, 187, 202,  79, 172, 110,
	157, 245, 217, 231,  95, 246, 232, 126, 203, 247, 233, 173, 218, 142, 111, 158,
	188, 248, 127, 234, 219, 249, 189, 204, 143, 174, 159, 250, 235, 205, 220, 175,
	190, 251, 221, 191, 206, 236, 207, 237, 252, 222, 253, 223, 238, 239, 254, 255,
};
DECLARE_ALIGNED( 16, static const int16, row_iscan_16x16[256]) = {
	  0,   1,   2,   4,   6,   9,  12,  17,  22,  29,  36,  43,  54,  64,  76,  86,
	  3,   5,   7,  11,  15,  19,  25,  32,  38,  48,  59,  68,  84,  99, 115, 130,
	  8,  10,  13,  18,  23,  27,  33,  42,  51,  60,  72,  88, 103, 119, 142, 167,
	 14,  16,  20,  26,  31,  37,  44,  53,  61,  73,  85, 100, 116, 135, 161, 185,
	 21,  24,  30,  35,  40,  47,  55,  65,  74,  81,  94, 112, 133, 154, 179, 205,
	 28,  34,  39,  45,  50,  58,  67,  77,  87,  96, 106, 121, 146, 169, 196, 212,
	 41,  46,  49,  56,  63,  70,  79,  90,  98, 107, 122, 138, 159, 182, 207, 222,
	 52,  57,  62,  69,  75,  83,  93, 102, 110, 120, 134, 150, 176, 195, 215, 226,
	 66,  71,  78,  82,  91,  97, 108, 113, 127, 136, 148, 168, 188, 202, 221, 232,
	 80,  89,  92, 101, 105, 114, 125, 131, 139, 151, 162, 177, 192, 208, 223, 234,
	 95, 104, 109, 117, 123, 128, 143, 144, 155, 165, 175, 190, 206, 219, 233, 239,
	111, 118, 124, 129, 140, 147, 157, 164, 170, 181, 191, 203, 224, 230, 240, 243,
	126, 132, 137, 145, 153, 160, 174, 178, 184, 197, 204, 216, 231, 237, 244, 246,
	141, 149, 156, 166, 172, 180, 189, 199, 200, 210, 220, 228, 238, 242, 249, 251,
	152, 163, 171, 183, 186, 193, 201, 211, 214, 218, 227, 236, 245, 247, 252, 253,
	158, 173, 187, 194, 198, 209, 213, 217, 225, 229, 235, 241, 248, 250, 254, 255,
};
DECLARE_ALIGNED(16, static int16, row_scan_16x16_neighbors[257 * MAX_NEIGHBORS]) = {
	  0,   0,   0,   0,   1,   1,   0,   0,   2,   2,  16,  16,   3,   3,  17,  17,  16,  16,   4,   4,  32,  32,  18,  18,   5,   5,  33,  33,  32,  32,  19,  19,
	 48,  48,   6,   6,  34,  34,  20,  20,  49,  49,  48,  48,   7,   7,  35,  35,  64,  64,  21,  21,  50,  50,  36,  36,  64,  64,   8,   8,  65,  65,  51,  51,
	 22,  22,  37,  37,  80,  80,  66,  66,   9,   9,  52,  52,  23,  23,  81,  81,  67,  67,  80,  80,  38,  38,  10,  10,  53,  53,  82,  82,  96,  96,  68,  68,
	 24,  24,  97,  97,  83,  83,  39,  39,  96,  96,  54,  54,  11,  11,  69,  69,  98,  98, 112, 112,  84,  84,  25,  25,  40,  40,  55,  55, 113, 113,  99,  99,
	 12,  12,  70,  70, 112, 112,  85,  85,  26,  26, 114, 114, 100, 100, 128, 128,  41,  41,  56,  56,  71,  71, 115, 115,  13,  13,  86,  86, 129, 129, 101, 101,
	128, 128,  72,  72, 130, 130, 116, 116,  27,  27,  57,  57,  14,  14,  87,  87,  42,  42, 144, 144, 102, 102, 131, 131, 145, 145, 117, 117,  73,  73, 144, 144,
	 88,  88, 132, 132, 103, 103,  28,  28,  58,  58, 146, 146, 118, 118,  43,  43, 160, 160, 147, 147,  89,  89, 104, 104, 133, 133, 161, 161, 119, 119, 160, 160,
	 74,  74, 134, 134, 148, 148,  29,  29,  59,  59, 162, 162, 176, 176,  44,  44, 120, 120,  90,  90, 105, 105, 163, 163, 177, 177, 149, 149, 176, 176, 135, 135,
	164, 164, 178, 178,  30,  30, 150, 150, 192, 192,  75,  75, 121, 121,  60,  60, 136, 136, 193, 193, 106, 106, 151, 151, 179, 179, 192, 192,  45,  45, 165, 165,
	166, 166, 194, 194,  91,  91, 180, 180, 137, 137, 208, 208, 122, 122, 152, 152, 208, 208, 195, 195,  76,  76, 167, 167, 209, 209, 181, 181, 224, 224, 107, 107,
	196, 196,  61,  61, 153, 153, 224, 224, 182, 182, 168, 168, 210, 210,  46,  46, 138, 138,  92,  92, 183, 183, 225, 225, 211, 211, 240, 240, 197, 197, 169, 169,
	123, 123, 154, 154, 198, 198,  77,  77, 212, 212, 184, 184, 108, 108, 226, 226, 199, 199,  62,  62, 227, 227, 241, 241, 139, 139, 213, 213, 170, 170, 185, 185,
	155, 155, 228, 228, 242, 242, 124, 124,  93,  93, 200, 200, 243, 243, 214, 214, 215, 215, 229, 229, 140, 140, 186, 186, 201, 201,  78,  78, 171, 171, 109, 109,
	156, 156, 244, 244, 216, 216, 230, 230,  94,  94, 245, 245, 231, 231, 125, 125, 202, 202, 246, 246, 232, 232, 172, 172, 217, 217, 141, 141, 110, 110, 157, 157,
	187, 187, 247, 247, 126, 126, 233, 233, 218, 218, 248, 248, 188, 188, 203, 203, 142, 142, 173, 173, 158, 158, 249, 249, 234, 234, 204, 204, 219, 219, 174, 174,
	189, 189, 250, 250, 220, 220, 190, 190, 205, 205, 235, 235, 206, 206, 236, 236, 251, 251, 221, 221, 252, 252, 222, 222, 237, 237, 238, 238, 253, 253, 254, 254,
	  0,   0,
};

DECLARE_ALIGNED( 16,static const int16,default_scan_32x32[1024]) = {
	  0,  32,   1,  64,  33,   2,  96,  65,  34, 128,   3,  97,  66, 160, 129,  35,  98,   4,  67, 130, 161, 192,  36,  99, 224,   5, 162, 193,  68, 131,  37, 100,
	225, 194, 256, 163,  69, 132,   6, 226, 257, 288, 195, 101, 164,  38, 258,   7, 227, 289, 133, 320,  70, 196, 165, 290, 259, 228,  39, 321, 102, 352,   8, 197,
	 71, 134, 322, 291, 260, 353, 384, 229, 166, 103,  40, 354, 323, 292, 135, 385, 198, 261,  72,   9, 416, 167, 386, 355, 230, 324, 104, 293,  41, 417, 199, 136,
	262, 387, 448, 325, 356,  10,  73, 418, 231, 168, 449, 294, 388, 105, 419, 263,  42, 200, 357, 450, 137, 480,  74, 326, 232,  11, 389, 169, 295, 420, 106, 451,
	481, 358, 264, 327, 201,  43, 138, 512, 482, 390, 296, 233, 170, 421,  75, 452, 359,  12, 513, 265, 483, 328, 107, 202, 514, 544, 422, 391, 453, 139,  44, 234,
	484, 297, 360, 171,  76, 515, 545, 266, 329, 454,  13, 423, 203, 108, 546, 485, 576, 298, 235, 140, 361, 330, 172, 547,  45, 455, 267, 577, 486,  77, 204, 362,
	608,  14, 299, 578, 109, 236, 487, 609, 331, 141, 579,  46,  15, 173, 610, 363,  78, 205,  16, 110, 237, 611, 142,  47, 174,  79, 206,  17, 111, 238,  48, 143,
	 80, 175, 112, 207,  49,  18, 239,  81, 113,  19,  50,  82, 114,  51,  83, 115, 640, 516, 392, 268, 144,  20, 672, 641, 548, 517, 424, 393, 300, 269, 176, 145,
	 52,  21, 704, 673, 642, 580, 549, 518, 456, 425, 394, 332, 301, 270, 208, 177, 146,  84,  53,  22, 736, 705, 674, 643, 612, 581, 550, 519, 488, 457, 426, 395,
	364, 333, 302, 271, 240, 209, 178, 147, 116,  85,  54,  23, 737, 706, 675, 613, 582, 551, 489, 458, 427, 365, 334, 303, 241, 210, 179, 117,  86,  55, 738, 707,
	614, 583, 490, 459, 366, 335, 242, 211, 118,  87, 739, 615, 491, 367, 243, 119, 768, 644, 520, 396, 272, 148,  24, 800, 769, 676, 645, 552, 521, 428, 397, 304,
	273, 180, 149,  56,  25, 832, 801, 770, 708, 677, 646, 584, 553, 522, 460, 429, 398, 336, 305, 274, 212, 181, 150,  88,  57,  26, 864, 833, 802, 771, 740, 709,
	678, 647, 616, 585, 554, 523, 492, 461, 430, 399, 368, 337, 306, 275, 244, 213, 182, 151, 120,  89,  58,  27, 865, 834, 803, 741, 710, 679, 617, 586, 555, 493,
	462, 431, 369, 338, 307, 245, 214, 183, 121,  90,  59, 866, 835, 742, 711, 618, 587, 494, 463, 370, 339, 246, 215, 122,  91, 867, 743, 619, 495, 371, 247, 123,
	896, 772, 648, 524, 400, 276, 152,  28, 928, 897, 804, 773, 680, 649, 556, 525, 432, 401, 308, 277, 184, 153,  60,  29, 960, 929, 898, 836, 805, 774, 712, 681,
	650, 588, 557, 526, 464, 433, 402, 340, 309, 278, 216, 185, 154,  92,  61,  30, 992, 961, 930, 899, 868, 837, 806, 775, 744, 713, 682, 651, 620, 589, 558, 527,
	496, 465, 434, 403, 372, 341, 310, 279, 248, 217, 186, 155, 124,  93,  62,  31, 993, 962, 931, 869, 838, 807, 745, 714, 683, 621, 590, 559, 497, 466, 435, 373,
	342, 311, 249, 218, 187, 125,  94,  63, 994, 963, 870, 839, 746, 715, 622, 591, 498, 467, 374, 343, 250, 219, 126,  95, 995, 871, 747, 623, 499, 375, 251, 127,
	900, 776, 652, 528, 404, 280, 156, 932, 901, 808, 777, 684, 653, 560, 529, 436, 405, 312, 281, 188, 157, 964, 933, 902, 840, 809, 778, 716, 685, 654, 592, 561,
	530, 468, 437, 406, 344, 313, 282, 220, 189, 158, 996, 965, 934, 903, 872, 841, 810, 779, 748, 717, 686, 655, 624, 593, 562, 531, 500, 469, 438, 407, 376, 345,
	314, 283, 252, 221, 190, 159, 997, 966, 935, 873, 842, 811, 749, 718, 687, 625, 594, 563, 501, 470, 439, 377, 346, 315, 253, 222, 191, 998, 967, 874, 843, 750,
	719, 626, 595, 502, 471, 378, 347, 254, 223, 999, 875, 751, 627, 503, 379, 255, 904, 780, 656, 532, 408, 284, 936, 905, 812, 781, 688, 657, 564, 533, 440, 409,
	316, 285, 968, 937, 906, 844, 813, 782, 720, 689, 658, 596, 565, 534, 472, 441, 410, 348, 317, 286,1000, 969, 938, 907, 876, 845, 814, 783, 752, 721, 690, 659,
	628, 597, 566, 535, 504, 473, 442, 411, 380, 349, 318, 287,1001, 970, 939, 877, 846, 815, 753, 722, 691, 629, 598, 567, 505, 474, 443, 381, 350, 319,1002, 971,
	878, 847, 754, 723, 630, 599, 506, 475, 382, 351,1003, 879, 755, 631, 507, 383, 908, 784, 660, 536, 412, 940, 909, 816, 785, 692, 661, 568, 537, 444, 413, 972,
	941, 910, 848, 817, 786, 724, 693, 662, 600, 569, 538, 476, 445, 414,1004, 973, 942, 911, 880, 849, 818, 787, 756, 725, 694, 663, 632, 601, 570, 539, 508, 477,
	446, 415,1005, 974, 943, 881, 850, 819, 757, 726, 695, 633, 602, 571, 509, 478, 447,1006, 975, 882, 851, 758, 727, 634, 603, 510, 479,1007, 883, 759, 635, 511,
	912, 788, 664, 540, 944, 913, 820, 789, 696, 665, 572, 541, 976, 945, 914, 852, 821, 790, 728, 697, 666, 604, 573, 542,1008, 977, 946, 915, 884, 853, 822, 791,
	760, 729, 698, 667, 636, 605, 574, 543,1009, 978, 947, 885, 854, 823, 761, 730, 699, 637, 606, 575,1010, 979, 886, 855, 762, 731, 638, 607,1011, 887, 763, 639,
	916, 792, 668, 948, 917, 824, 793, 700, 669, 980, 949, 918, 856, 825, 794, 732, 701, 670,1012, 981, 950, 919, 888, 857, 826, 795, 764, 733, 702, 671,1013, 982,
	951, 889, 858, 827, 765, 734, 703,1014, 983, 890, 859, 766, 735,1015, 891, 767, 920, 796, 952, 921, 828, 797, 984, 953, 922, 860, 829, 798,1016, 985, 954, 923,
	892, 861, 830, 799,1017, 986, 955, 893, 862, 831,1018, 987, 894, 863,1019, 895, 924, 956, 925, 988, 957, 926,1020, 989, 958, 927,1021, 990, 959,1022, 991,1023,
};
DECLARE_ALIGNED( 16, static const int16, default_iscan_32x32[1024]) = {
	  0,   2,   5,  10,  17,  25,  38,  47,  62,  83, 101, 121, 145, 170, 193, 204, 210, 219, 229, 233, 245, 257, 275, 299, 342, 356, 377, 405, 455, 471, 495, 527,
	  1,   4,   8,  15,  22,  30,  45,  58,  74,  92, 112, 133, 158, 184, 203, 215, 222, 228, 234, 237, 256, 274, 298, 317, 355, 376, 404, 426, 470, 494, 526, 551,
	  3,   7,  12,  18,  28,  36,  52,  64,  82, 102, 118, 142, 164, 189, 208, 217, 224, 231, 235, 238, 273, 297, 316, 329, 375, 403, 425, 440, 493, 525, 550, 567,
	  6,  11,  16,  23,  31,  43,  60,  73,  90, 109, 126, 150, 173, 196, 211, 220, 226, 232, 236, 239, 296, 315, 328, 335, 402, 424, 439, 447, 524, 549, 566, 575,
	  9,  14,  19,  29,  37,  50,  65,  78,  95, 116, 134, 157, 179, 201, 214, 223, 244, 255, 272, 295, 341, 354, 374, 401, 454, 469, 492, 523, 582, 596, 617, 645,
	 13,  20,  26,  35,  44,  54,  72,  85, 105, 123, 140, 163, 182, 205, 216, 225, 254, 271, 294, 314, 353, 373, 400, 423, 468, 491, 522, 548, 595, 616, 644, 666,
	 21,  27,  33,  42,  53,  63,  80,  94, 113, 132, 151, 172, 190, 209, 218, 227, 270, 293, 313, 327, 372, 399, 422, 438, 490, 521, 547, 565, 615, 643, 665, 680,
	 24,  32,  39,  48,  57,  71,  88, 104, 120, 139, 159, 178, 197, 212, 221, 230, 292, 312, 326, 334, 398, 421, 437, 446, 520, 546, 564, 574, 642, 664, 679, 687,
	 34,  40,  46,  56,  68,  81,  96, 111, 130, 147, 167, 186, 243, 253, 269, 291, 340, 352, 371, 397, 453, 467, 489, 519, 581, 594, 614, 641, 693, 705, 723, 747,
	 41,  49,  55,  67,  77,  91, 107, 124, 138, 161, 177, 194, 252, 268, 290, 311, 351, 370, 396, 420, 466, 488, 518, 545, 593, 613, 640, 663, 704, 722, 746, 765,
	 51,  59,  66,  76,  89,  99, 119, 131, 149, 168, 181, 200, 267, 289, 310, 325, 369, 395, 419, 436, 487, 517, 544, 563, 612, 639, 662, 678, 721, 745, 764, 777,
	 61,  69,  75,  87, 100, 114, 129, 144, 162, 180, 191, 207, 288, 309, 324, 333, 394, 418, 435, 445, 516, 543, 562, 573, 638, 661, 677, 686, 744, 763, 776, 783,
	 70,  79,  86,  97, 108, 122, 137, 155, 242, 251, 266, 287, 339, 350, 368, 393, 452, 465, 486, 515, 580, 592, 611, 637, 692, 703, 720, 743, 788, 798, 813, 833,
	 84,  93, 103, 110, 125, 141, 154, 171, 250, 265, 286, 308, 349, 367, 392, 417, 464, 485, 514, 542, 591, 610, 636, 660, 702, 719, 742, 762, 797, 812, 832, 848,
	 98, 106, 115, 127, 143, 156, 169, 185, 264, 285, 307, 323, 366, 391, 416, 434, 484, 513, 541, 561, 609, 635, 659, 676, 718, 741, 761, 775, 811, 831, 847, 858,
	117, 128, 136, 148, 160, 175, 188, 198, 284, 306, 322, 332, 390, 415, 433, 444, 512, 540, 560, 572, 634, 658, 675, 685, 740, 760, 774, 782, 830, 846, 857, 863,
	135, 146, 152, 165, 241, 249, 263, 283, 338, 348, 365, 389, 451, 463, 483, 511, 579, 590, 608, 633, 691, 701, 717, 739, 787, 796, 810, 829, 867, 875, 887, 903,
	153, 166, 174, 183, 248, 262, 282, 305, 347, 364, 388, 414, 462, 482, 510, 539, 589, 607, 632, 657, 700, 716, 738, 759, 795, 809, 828, 845, 874, 886, 902, 915,
	176, 187, 195, 202, 261, 281, 304, 321, 363, 387, 413, 432, 481, 509, 538, 559, 606, 631, 656, 674, 715, 737, 758, 773, 808, 827, 844, 856, 885, 901, 914, 923,
	192, 199, 206, 213, 280, 303, 320, 331, 386, 412, 431, 443, 508, 537, 558, 571, 630, 655, 673, 684, 736, 757, 772, 781, 826, 843, 855, 862, 900, 913, 922, 927,
	240, 247, 260, 279, 337, 346, 362, 385, 450, 461, 480, 507, 578, 588, 605, 629, 690, 699, 714, 735, 786, 794, 807, 825, 866, 873, 884, 899, 930, 936, 945, 957,
	246, 259, 278, 302, 345, 361, 384, 411, 460, 479, 506, 536, 587, 604, 628, 654, 698, 713, 734, 756, 793, 806, 824, 842, 872, 883, 898, 912, 935, 944, 956, 966,
	258, 277, 301, 319, 360, 383, 410, 430, 478, 505, 535, 557, 603, 627, 653, 672, 712, 733, 755, 771, 805, 823, 841, 854, 882, 897, 911, 921, 943, 955, 965, 972,
	276, 300, 318, 330, 382, 409, 429, 442, 504, 534, 556, 570, 626, 652, 671, 683, 732, 754, 770, 780, 822, 840, 853, 861, 896, 910, 920, 926, 954, 964, 971, 975,
	336, 344, 359, 381, 449, 459, 477, 503, 577, 586, 602, 625, 689, 697, 711, 731, 785, 792, 804, 821, 865, 871, 881, 895, 929, 934, 942, 953, 977, 981, 987, 995,
	343, 358, 380, 408, 458, 476, 502, 533, 585, 601, 624, 651, 696, 710, 730, 753, 791, 803, 820, 839, 870, 880, 894, 909, 933, 941, 952, 963, 980, 986, 994,1001,
	357, 379, 407, 428, 475, 501, 532, 555, 600, 623, 650, 670, 709, 729, 752, 769, 802, 819, 838, 852, 879, 893, 908, 919, 940, 951, 962, 970, 985, 993,1000,1005,
	378, 406, 427, 441, 500, 531, 554, 569, 622, 649, 669, 682, 728, 751, 768, 779, 818, 837, 851, 860, 892, 907, 918, 925, 950, 961, 969, 974, 992, 999,1004,1007,
	448, 457, 474, 499, 576, 584, 599, 621, 688, 695, 708, 727, 784, 790, 801, 817, 864, 869, 878, 891, 928, 932, 939, 949, 976, 979, 984, 991,1008,1010,1013,1017,
	456, 473, 498, 530, 583, 598, 620, 648, 694, 707, 726, 750, 789, 800, 816, 836, 868, 877, 890, 906, 931, 938, 948, 960, 978, 983, 990, 998,1009,1012,1016,1020,
	472, 497, 529, 553, 597, 619, 647, 668, 706, 725, 749, 767, 799, 815, 835, 850, 876, 889, 905, 917, 937, 947, 959, 968, 982, 989, 997,1003,1011,1015,1019,1022,
	496, 528, 552, 568, 618, 646, 667, 681, 724, 748, 766, 778, 814, 834, 849, 859, 888, 904, 916, 924, 946, 958, 967, 973, 988, 996,1002,1006,1014,1018,1021,1023,
};
DECLARE_ALIGNED(16, static int16, default_scan_32x32_neighbors[1025 * MAX_NEIGHBORS]) = {
	  0,   0,   0,   0,   0,   0,  32,  32,   1,  32,   1,   1,  64,  64,  33,  64,   2,  33,  96,  96,   2,   2,  65,  96,  34,  65, 128, 128,  97, 128,   3,  34,
	 66,  97,   3,   3,  35,  66,  98, 129, 129, 160, 160, 160,   4,  35,  67,  98, 192, 192,   4,   4, 130, 161, 161, 192,  36,  67,  99, 130,   5,  36,  68,  99,
	193, 224, 162, 193, 224, 224, 131, 162,  37,  68, 100, 131,   5,   5, 194, 225, 225, 256, 256, 256, 163, 194,  69, 100, 132, 163,   6,  37, 226, 257,   6,   6,
	195, 226, 257, 288, 101, 132, 288, 288,  38,  69, 164, 195, 133, 164, 258, 289, 227, 258, 196, 227,   7,  38, 289, 320,  70, 101, 320, 320,   7,   7, 165, 196,
	 39,  70, 102, 133, 290, 321, 259, 290, 228, 259, 321, 352, 352, 352, 197, 228, 134, 165,  71, 102,   8,  39, 322, 353, 291, 322, 260, 291, 103, 134, 353, 384,
	166, 197, 229, 260,  40,  71,   8,   8, 384, 384, 135, 166, 354, 385, 323, 354, 198, 229, 292, 323,  72, 103, 261, 292,   9,  40, 385, 416, 167, 198, 104, 135,
	230, 261, 355, 386, 416, 416, 293, 324, 324, 355,   9,   9,  41,  72, 386, 417, 199, 230, 136, 167, 417, 448, 262, 293, 356, 387,  73, 104, 387, 418, 231, 262,
	 10,  41, 168, 199, 325, 356, 418, 449, 105, 136, 448, 448,  42,  73, 294, 325, 200, 231,  10,  10, 357, 388, 137, 168, 263, 294, 388, 419,  74, 105, 419, 450,
	449, 480, 326, 357, 232, 263, 295, 326, 169, 200,  11,  42, 106, 137, 480, 480, 450, 481, 358, 389, 264, 295, 201, 232, 138, 169, 389, 420,  43,  74, 420, 451,
	327, 358,  11,  11, 481, 512, 233, 264, 451, 482, 296, 327,  75, 106, 170, 201, 482, 513, 512, 512, 390, 421, 359, 390, 421, 452, 107, 138,  12,  43, 202, 233,
	452, 483, 265, 296, 328, 359, 139, 170,  44,  75, 483, 514, 513, 544, 234, 265, 297, 328, 422, 453,  12,  12, 391, 422, 171, 202,  76, 107, 514, 545, 453, 484,
	544, 544, 266, 297, 203, 234, 108, 139, 329, 360, 298, 329, 140, 171, 515, 546,  13,  44, 423, 454, 235, 266, 545, 576, 454, 485,  45,  76, 172, 203, 330, 361,
	576, 576,  13,  13, 267, 298, 546, 577,  77, 108, 204, 235, 455, 486, 577, 608, 299, 330, 109, 140, 547, 578,  14,  45,  14,  14, 141, 172, 578, 609, 331, 362,
	 46,  77, 173, 204,  15,  15,  78, 109, 205, 236, 579, 610, 110, 141,  15,  46, 142, 173,  47,  78, 174, 205,  16,  16,  79, 110, 206, 237,  16,  47, 111, 142,
	 48,  79, 143, 174,  80, 111, 175, 206,  17,  48,  17,  17, 207, 238,  49,  80,  81, 112,  18,  18,  18,  49,  50,  81,  82, 113,  19,  50,  51,  82,  83, 114,
	608, 608, 484, 515, 360, 391, 236, 267, 112, 143,  19,  19, 640, 640, 609, 640, 516, 547, 485, 516, 392, 423, 361, 392, 268, 299, 237, 268, 144, 175, 113, 144,
	 20,  51,  20,  20, 672, 672, 641, 672, 610, 641, 548, 579, 517, 548, 486, 517, 424, 455, 393, 424, 362, 393, 300, 331, 269, 300, 238, 269, 176, 207, 145, 176,
	114, 145,  52,  83,  21,  52,  21,  21, 704, 704, 673, 704, 642, 673, 611, 642, 580, 611, 549, 580, 518, 549, 487, 518, 456, 487, 425, 456, 394, 425, 363, 394,
	332, 363, 301, 332, 270, 301, 239, 270, 208, 239, 177, 208, 146, 177, 115, 146,  84, 115,  53,  84,  22,  53,  22,  22, 705, 736, 674, 705, 643, 674, 581, 612,
	550, 581, 519, 550, 457, 488, 426, 457, 395, 426, 333, 364, 302, 333, 271, 302, 209, 240, 178, 209, 147, 178,  85, 116,  54,  85,  23,  54, 706, 737, 675, 706,
	582, 613, 551, 582, 458, 489, 427, 458, 334, 365, 303, 334, 210, 241, 179, 210,  86, 117,  55,  86, 707, 738, 583, 614, 459, 490, 335, 366, 211, 242,  87, 118,
	736, 736, 612, 643, 488, 519, 364, 395, 240, 271, 116, 147,  23,  23, 768, 768, 737, 768, 644, 675, 613, 644, 520, 551, 489, 520, 396, 427, 365, 396, 272, 303,
	241, 272, 148, 179, 117, 148,  24,  55,  24,  24, 800, 800, 769, 800, 738, 769, 676, 707, 645, 676, 614, 645, 552, 583, 521, 552, 490, 521, 428, 459, 397, 428,
	366, 397, 304, 335, 273, 304, 242, 273, 180, 211, 149, 180, 118, 149,  56,  87,  25,  56,  25,  25, 832, 832, 801, 832, 770, 801, 739, 770, 708, 739, 677, 708,
	646, 677, 615, 646, 584, 615, 553, 584, 522, 553, 491, 522, 460, 491, 429, 460, 398, 429, 367, 398, 336, 367, 305, 336, 274, 305, 243, 274, 212, 243, 181, 212,
	150, 181, 119, 150,  88, 119,  57,  88,  26,  57,  26,  26, 833, 864, 802, 833, 771, 802, 709, 740, 678, 709, 647, 678, 585, 616, 554, 585, 523, 554, 461, 492,
	430, 461, 399, 430, 337, 368, 306, 337, 275, 306, 213, 244, 182, 213, 151, 182,  89, 120,  58,  89,  27,  58, 834, 865, 803, 834, 710, 741, 679, 710, 586, 617,
	555, 586, 462, 493, 431, 462, 338, 369, 307, 338, 214, 245, 183, 214,  90, 121,  59,  90, 835, 866, 711, 742, 587, 618, 463, 494, 339, 370, 215, 246,  91, 122,
	864, 864, 740, 771, 616, 647, 492, 523, 368, 399, 244, 275, 120, 151,  27,  27, 896, 896, 865, 896, 772, 803, 741, 772, 648, 679, 617, 648, 524, 555, 493, 524,
	400, 431, 369, 400, 276, 307, 245, 276, 152, 183, 121, 152,  28,  59,  28,  28, 928, 928, 897, 928, 866, 897, 804, 835, 773, 804, 742, 773, 680, 711, 649, 680,
	618, 649, 556, 587, 525, 556, 494, 525, 432, 463, 401, 432, 370, 401, 308, 339, 277, 308, 246, 277, 184, 215, 153, 184, 122, 153,  60,  91,  29,  60,  29,  29,
	960, 960, 929, 960, 898, 929, 867, 898, 836, 867, 805, 836, 774, 805, 743, 774, 712, 743, 681, 712, 650, 681, 619, 650, 588, 619, 557, 588, 526, 557, 495, 526,
	464, 495, 433, 464, 402, 433, 371, 402, 340, 371, 309, 340, 278, 309, 247, 278, 216, 247, 185, 216, 154, 185, 123, 154,  92, 123,  61,  92,  30,  61,  30,  30,
	961, 992, 930, 961, 899, 930, 837, 868, 806, 837, 775, 806, 713, 744, 682, 713, 651, 682, 589, 620, 558, 589, 527, 558, 465, 496, 434, 465, 403, 434, 341, 372,
	310, 341, 279, 310, 217, 248, 186, 217, 155, 186,  93, 124,  62,  93,  31,  62, 962, 993, 931, 962, 838, 869, 807, 838, 714, 745, 683, 714, 590, 621, 559, 590,
	466, 497, 435, 466, 342, 373, 311, 342, 218, 249, 187, 218,  94, 125,  63,  94, 963, 994, 839, 870, 715, 746, 591, 622, 467, 498, 343, 374, 219, 250,  95, 126,
	868, 899, 744, 775, 620, 651, 496, 527, 372, 403, 248, 279, 124, 155, 900, 931, 869, 900, 776, 807, 745, 776, 652, 683, 621, 652, 528, 559, 497, 528, 404, 435,
	373, 404, 280, 311, 249, 280, 156, 187, 125, 156, 932, 963, 901, 932, 870, 901, 808, 839, 777, 808, 746, 777, 684, 715, 653, 684, 622, 653, 560, 591, 529, 560,
	498, 529, 436, 467, 405, 436, 374, 405, 312, 343, 281, 312, 250, 281, 188, 219, 157, 188, 126, 157, 964, 995, 933, 964, 902, 933, 871, 902, 840, 871, 809, 840,
	778, 809, 747, 778, 716, 747, 685, 716, 654, 685, 623, 654, 592, 623, 561, 592, 530, 561, 499, 530, 468, 499, 437, 468, 406, 437, 375, 406, 344, 375, 313, 344,
	282, 313, 251, 282, 220, 251, 189, 220, 158, 189, 127, 158, 965, 996, 934, 965, 903, 934, 841, 872, 810, 841, 779, 810, 717, 748, 686, 717, 655, 686, 593, 624,
	562, 593, 531, 562, 469, 500, 438, 469, 407, 438, 345, 376, 314, 345, 283, 314, 221, 252, 190, 221, 159, 190, 966, 997, 935, 966, 842, 873, 811, 842, 718, 749,
	687, 718, 594, 625, 563, 594, 470, 501, 439, 470, 346, 377, 315, 346, 222, 253, 191, 222, 967, 998, 843, 874, 719, 750, 595, 626, 471, 502, 347, 378, 223, 254,
	872, 903, 748, 779, 624, 655, 500, 531, 376, 407, 252, 283, 904, 935, 873, 904, 780, 811, 749, 780, 656, 687, 625, 656, 532, 563, 501, 532, 408, 439, 377, 408,
	284, 315, 253, 284, 936, 967, 905, 936, 874, 905, 812, 843, 781, 812, 750, 781, 688, 719, 657, 688, 626, 657, 564, 595, 533, 564, 502, 533, 440, 471, 409, 440,
	378, 409, 316, 347, 285, 316, 254, 285, 968, 999, 937, 968, 906, 937, 875, 906, 844, 875, 813, 844, 782, 813, 751, 782, 720, 751, 689, 720, 658, 689, 627, 658,
	596, 627, 565, 596, 534, 565, 503, 534, 472, 503, 441, 472, 410, 441, 379, 410, 348, 379, 317, 348, 286, 317, 255, 286, 969,1000, 938, 969, 907, 938, 845, 876,
	814, 845, 783, 814, 721, 752, 690, 721, 659, 690, 597, 628, 566, 597, 535, 566, 473, 504, 442, 473, 411, 442, 349, 380, 318, 349, 287, 318, 970,1001, 939, 970,
	846, 877, 815, 846, 722, 753, 691, 722, 598, 629, 567, 598, 474, 505, 443, 474, 350, 381, 319, 350, 971,1002, 847, 878, 723, 754, 599, 630, 475, 506, 351, 382,
	876, 907, 752, 783, 628, 659, 504, 535, 380, 411, 908, 939, 877, 908, 784, 815, 753, 784, 660, 691, 629, 660, 536, 567, 505, 536, 412, 443, 381, 412, 940, 971,
	909, 940, 878, 909, 816, 847, 785, 816, 754, 785, 692, 723, 661, 692, 630, 661, 568, 599, 537, 568, 506, 537, 444, 475, 413, 444, 382, 413, 972,1003, 941, 972,
	910, 941, 879, 910, 848, 879, 817, 848, 786, 817, 755, 786, 724, 755, 693, 724, 662, 693, 631, 662, 600, 631, 569, 600, 538, 569, 507, 538, 476, 507, 445, 476,
	414, 445, 383, 414, 973,1004, 942, 973, 911, 942, 849, 880, 818, 849, 787, 818, 725, 756, 694, 725, 663, 694, 601, 632, 570, 601, 539, 570, 477, 508, 446, 477,
	415, 446, 974,1005, 943, 974, 850, 881, 819, 850, 726, 757, 695, 726, 602, 633, 571, 602, 478, 509, 447, 478, 975,1006, 851, 882, 727, 758, 603, 634, 479, 510,
	880, 911, 756, 787, 632, 663, 508, 539, 912, 943, 881, 912, 788, 819, 757, 788, 664, 695, 633, 664, 540, 571, 509, 540, 944, 975, 913, 944, 882, 913, 820, 851,
	789, 820, 758, 789, 696, 727, 665, 696, 634, 665, 572, 603, 541, 572, 510, 541, 976,1007, 945, 976, 914, 945, 883, 914, 852, 883, 821, 852, 790, 821, 759, 790,
	728, 759, 697, 728, 666, 697, 635, 666, 604, 635, 573, 604, 542, 573, 511, 542, 977,1008, 946, 977, 915, 946, 853, 884, 822, 853, 791, 822, 729, 760, 698, 729,
	667, 698, 605, 636, 574, 605, 543, 574, 978,1009, 947, 978, 854, 885, 823, 854, 730, 761, 699, 730, 606, 637, 575, 606, 979,1010, 855, 886, 731, 762, 607, 638,
	884, 915, 760, 791, 636, 667, 916, 947, 885, 916, 792, 823, 761, 792, 668, 699, 637, 668, 948, 979, 917, 948, 886, 917, 824, 855, 793, 824, 762, 793, 700, 731,
	669, 700, 638, 669, 980,1011, 949, 980, 918, 949, 887, 918, 856, 887, 825, 856, 794, 825, 763, 794, 732, 763, 701, 732, 670, 701, 639, 670, 981,1012, 950, 981,
	919, 950, 857, 888, 826, 857, 795, 826, 733, 764, 702, 733, 671, 702, 982,1013, 951, 982, 858, 889, 827, 858, 734, 765, 703, 734, 983,1014, 859, 890, 735, 766,
	888, 919, 764, 795, 920, 951, 889, 920, 796, 827, 765, 796, 952, 983, 921, 952, 890, 921, 828, 859, 797, 828, 766, 797, 984,1015, 953, 984, 922, 953, 891, 922,
	860, 891, 829, 860, 798, 829, 767, 798, 985,1016, 954, 985, 923, 954, 861, 892, 830, 861, 799, 830, 986,1017, 955, 986, 862, 893, 831, 862, 987,1018, 863, 894,
	892, 923, 924, 955, 893, 924, 956, 987, 925, 956, 894, 925, 988,1019, 957, 988, 926, 957, 895, 926, 989,1020, 958, 989, 927, 958, 990,1021, 959, 990, 991,1022,
	0,   0,
};

const ScanOrder scan_orders[TX_SIZES][TX_TYPES] = {
  {  // TX_4X4																		//  V   H
	{default_scan_4x4,		default_iscan_4x4,		default_scan_4x4_neighbors},	// DCT_DCT
	{row_scan_4x4,			row_iscan_4x4,			row_scan_4x4_neighbors},		// ADST_DCT
	{col_scan_4x4,			col_iscan_4x4,			col_scan_4x4_neighbors},		// DCT_ADST
	{default_scan_4x4,		default_iscan_4x4,		default_scan_4x4_neighbors}		// ADST_ADST
  }, {  // TX_8X8
	{default_scan_8x8,		default_iscan_8x8,		default_scan_8x8_neighbors},	// DCT_DCT
	{row_scan_8x8,			row_iscan_8x8,			row_scan_8x8_neighbors},		// ADST_DCT
	{col_scan_8x8,			col_iscan_8x8,			col_scan_8x8_neighbors},		// DCT_ADST
	{default_scan_8x8,		default_iscan_8x8,		default_scan_8x8_neighbors}		// ADST_ADST
  }, {  // TX_16X16
	{default_scan_16x16,	default_iscan_16x16,	default_scan_16x16_neighbors},	// DCT_DCT
	{row_scan_16x16,		row_iscan_16x16,		row_scan_16x16_neighbors},	  	// ADST_DCT
	{col_scan_16x16,		col_iscan_16x16,		col_scan_16x16_neighbors},	  	// DCT_ADST
	{default_scan_16x16,	default_iscan_16x16,	default_scan_16x16_neighbors} 	// ADST_ADST
  }, {  // TX_32X32
	{default_scan_32x32,	default_iscan_32x32,	default_scan_32x32_neighbors},	// DCT_DCT
	{default_scan_32x32,	default_iscan_32x32,	default_scan_32x32_neighbors},	// ADST_DCT
	{default_scan_32x32,	default_iscan_32x32,	default_scan_32x32_neighbors},	// DCT_ADST
	{default_scan_32x32,	default_iscan_32x32,	default_scan_32x32_neighbors},	// ADST_ADST
  }
};

static struct FixNeighbours {
	void	fix(const ScanOrder &sc, TX_SIZE size) {
		const int16 *scan		= sc.iscan;
		int16		*neighbors	= unconst(sc.neighbors);
		for (int i = 0, n = ((16 << (size * 2)) + 1) * 2; i < n; i++)
			neighbors[i] = scan[neighbors[i]];

//		for (int i = 0, n = 16 << (size * 2); i < n; i++)
//			ISO_ASSERT(sc.iscan[sc.scan[i]]== i);

/*
		int	s = 4 << size;
		for (int i = 0; i < s; i++) {
			for (int j = 0; j < s; j++)
				ISO_OUTPUTF("%4i,", sc.iscan[j * s + i]);
			ISO_OUTPUT("\n");
		}
*/
	}
	FixNeighbours() {
		static const int8 unique[] = {0,1,2,4,5,6,8,9,10,12};
		for (int i = 0; i < num_elements(unique); i++) {
			int	u = unique[i];
			fix(scan_orders[u >> 2][u & 3], TX_SIZE(u >> 2));
		}
	}
} fix_neighbours;

//-------------------------------------
//	probability trees
//-------------------------------------

const tree_index coef_con_tree[] = {
	2,							6,
	-TWO_TOKEN,					4,
	-THREE_TOKEN,				-FOUR_TOKEN,
	8,							10,
	-CATEGORY1_TOKEN,			-CATEGORY2_TOKEN,
	12,							14,
	-CATEGORY3_TOKEN,			-CATEGORY4_TOKEN,
	-CATEGORY5_TOKEN,			-CATEGORY6_TOKEN
};

const tree_index inter_mode_tree[] = {
	-(ZEROMV - NEARESTMV),		2,
	-(NEARESTMV - NEARESTMV),	4,
	-(NEARMV - NEARESTMV),		-(NEWMV - NEARESTMV)
};

const tree_index partition_tree[] = {
	-PARTITION_NONE,			2,
	-PARTITION_HORZ,			4,
	-PARTITION_VERT,			-PARTITION_SPLIT
};

const tree_index switchable_interp_tree[] = {
	-INTERP_8TAP,				2,
	-INTERP_8TAP_SMOOTH,		-INTERP_8TAP_SHARP
};

const tree_index intra_mode_tree[] = {
	-DC_PRED,					2,
	-TM_PRED,					4,
	-V_PRED,					6,
	8,							12,
	-H_PRED,					10,
	-D135_PRED,					-D117_PRED,
	-D45_PRED,					14,
	-D63_PRED,					16,
	-D153_PRED,					-D207_PRED
};

const tree_index segment_tree[] = {
	2,							4,
	6,							8,
	10,							12,
	0,							-1,
	-2,							-3,
	-4,							-5,
	-6,							-7
};

const tree_index mv_joint_tree[] = {
	-MotionVector::JOINT_ZERO,	2,
	-MotionVector::JOINT_HNZVZ,	4,
	-MotionVector::JOINT_HZVNZ,	-MotionVector::JOINT_HNZVNZ
};

const tree_index mv_class_tree[] = {
	-MotionVector::CLASS_0,		2,
	-MotionVector::CLASS_1,		4,
	6, 8,
	-MotionVector::CLASS_2,		-MotionVector::CLASS_3,
	10, 12,
	-MotionVector::CLASS_4,		-MotionVector::CLASS_5,
	-MotionVector::CLASS_6,		14,
	16, 18,
	-MotionVector::CLASS_7,		-MotionVector::CLASS_8,
	-MotionVector::CLASS_9,		-MotionVector::CLASS_10,
};

const tree_index mv_class0_tree[] = {
	-0,							-1,
};

const tree_index mv_fp_tree[] = {
	-0,							2,
	-1,							4,
	-2,							-3
};

//-------------------------------------
//	probabilities
//-------------------------------------

// Model obtained from a 2-sided zero-centerd distribuition derived from a Pareto distribution. The cdf of the distribution is:
// cdf(x) = 0.5 + 0.5 * sgn(x) * [1 - {alpha/(alpha + |x|)} ^ beta]		(beta = 8)
// For a given beta and a given probablity of the 1-node, the alpha is first solved, and then the {alpha, beta} pair is used to generate the probabilities for the rest of the nodes.
// Every odd line in this table can be generated from the even lines by averaging:
// pareto8_full[l][node] = (pareto8_full[l-1][node] + pareto8_full[l+1][node] ) >> 1;

const prob vp9::pareto8_full[COEFF_PROB_MODELS][MODEL_NODES] = {
	{  3,  86, 128,   6,  86,  23,  88,  29},
	{  6,  86, 128,  11,  87,  42,  91,  52},
	{  9,  86, 129,  17,  88,  61,  94,  76},
	{ 12,  86, 129,  22,  88,  77,  97,  93},
	{ 15,  87, 129,  28,  89,  93, 100, 110},
	{ 17,  87, 129,  33,  90, 105, 103, 123},
	{ 20,  88, 130,  38,  91, 118, 106, 136},
	{ 23,  88, 130,  43,  91, 128, 108, 146},
	{ 26,  89, 131,  48,  92, 139, 111, 156},
	{ 28,  89, 131,  53,  93, 147, 114, 163},
	{ 31,  90, 131,  58,  94, 156, 117, 171},
	{ 34,  90, 131,  62,  94, 163, 119, 177},
	{ 37,  90, 132,  66,  95, 171, 122, 184},
	{ 39,  90, 132,  70,  96, 177, 124, 189},
	{ 42,  91, 132,  75,  97, 183, 127, 194},
	{ 44,  91, 132,  79,  97, 188, 129, 198},
	{ 47,  92, 133,  83,  98, 193, 132, 202},
	{ 49,  92, 133,  86,  99, 197, 134, 205},
	{ 52,  93, 133,  90, 100, 201, 137, 208},
	{ 54,  93, 133,  94, 100, 204, 139, 211},
	{ 57,  94, 134,  98, 101, 208, 142, 214},
	{ 59,  94, 134, 101, 102, 211, 144, 216},
	{ 62,  94, 135, 105, 103, 214, 146, 218},
	{ 64,  94, 135, 108, 103, 216, 148, 220},
	{ 66,  95, 135, 111, 104, 219, 151, 222},
	{ 68,  95, 135, 114, 105, 221, 153, 223},
	{ 71,  96, 136, 117, 106, 224, 155, 225},
	{ 73,  96, 136, 120, 106, 225, 157, 226},
	{ 76,  97, 136, 123, 107, 227, 159, 228},
	{ 78,  97, 136, 126, 108, 229, 160, 229},
	{ 80,  98, 137, 129, 109, 231, 162, 231},
	{ 82,  98, 137, 131, 109, 232, 164, 232},
	{ 84,  98, 138, 134, 110, 234, 166, 233},
	{ 86,  98, 138, 137, 111, 235, 168, 234},
	{ 89,  99, 138, 140, 112, 236, 170, 235},
	{ 91,  99, 138, 142, 112, 237, 171, 235},
	{ 93, 100, 139, 145, 113, 238, 173, 236},
	{ 95, 100, 139, 147, 114, 239, 174, 237},
	{ 97, 101, 140, 149, 115, 240, 176, 238},
	{ 99, 101, 140, 151, 115, 241, 177, 238},
	{101, 102, 140, 154, 116, 242, 179, 239},
	{103, 102, 140, 156, 117, 242, 180, 239},
	{105, 103, 141, 158, 118, 243, 182, 240},
	{107, 103, 141, 160, 118, 243, 183, 240},
	{109, 104, 141, 162, 119, 244, 185, 241},
	{111, 104, 141, 164, 119, 244, 186, 241},
	{113, 104, 142, 166, 120, 245, 187, 242},
	{114, 104, 142, 168, 121, 245, 188, 242},
	{116, 105, 143, 170, 122, 246, 190, 243},
	{118, 105, 143, 171, 122, 246, 191, 243},
	{120, 106, 143, 173, 123, 247, 192, 244},
	{121, 106, 143, 175, 124, 247, 193, 244},
	{123, 107, 144, 177, 125, 248, 195, 244},
	{125, 107, 144, 178, 125, 248, 196, 244},
	{127, 108, 145, 180, 126, 249, 197, 245},
	{128, 108, 145, 181, 127, 249, 198, 245},
	{130, 109, 145, 183, 128, 249, 199, 245},
	{132, 109, 145, 184, 128, 249, 200, 245},
	{134, 110, 146, 186, 129, 250, 201, 246},
	{135, 110, 146, 187, 130, 250, 202, 246},
	{137, 111, 147, 189, 131, 251, 203, 246},
	{138, 111, 147, 190, 131, 251, 204, 246},
	{140, 112, 147, 192, 132, 251, 205, 247},
	{141, 112, 147, 193, 132, 251, 206, 247},
	{143, 113, 148, 194, 133, 251, 207, 247},
	{144, 113, 148, 195, 134, 251, 207, 247},
	{146, 114, 149, 197, 135, 252, 208, 248},
	{147, 114, 149, 198, 135, 252, 209, 248},
	{149, 115, 149, 199, 136, 252, 210, 248},
	{150, 115, 149, 200, 137, 252, 210, 248},
	{152, 115, 150, 201, 138, 252, 211, 248},
	{153, 115, 150, 202, 138, 252, 212, 248},
	{155, 116, 151, 204, 139, 253, 213, 249},
	{156, 116, 151, 205, 139, 253, 213, 249},
	{158, 117, 151, 206, 140, 253, 214, 249},
	{159, 117, 151, 207, 141, 253, 215, 249},
	{161, 118, 152, 208, 142, 253, 216, 249},
	{162, 118, 152, 209, 142, 253, 216, 249},
	{163, 119, 153, 210, 143, 253, 217, 249},
	{164, 119, 153, 211, 143, 253, 217, 249},
	{166, 120, 153, 212, 144, 254, 218, 250},
	{167, 120, 153, 212, 145, 254, 219, 250},
	{168, 121, 154, 213, 146, 254, 220, 250},
	{169, 121, 154, 214, 146, 254, 220, 250},
	{171, 122, 155, 215, 147, 254, 221, 250},
	{172, 122, 155, 216, 147, 254, 221, 250},
	{173, 123, 155, 217, 148, 254, 222, 250},
	{174, 123, 155, 217, 149, 254, 222, 250},
	{176, 124, 156, 218, 150, 254, 223, 250},
	{177, 124, 156, 219, 150, 254, 223, 250},
	{178, 125, 157, 220, 151, 254, 224, 251},
	{179, 125, 157, 220, 151, 254, 224, 251},
	{180, 126, 157, 221, 152, 254, 225, 251},
	{181, 126, 157, 221, 152, 254, 225, 251},
	{183, 127, 158, 222, 153, 254, 226, 251},
	{184, 127, 158, 223, 154, 254, 226, 251},
	{185, 128, 159, 224, 155, 255, 227, 251},
	{186, 128, 159, 224, 155, 255, 227, 251},
	{187, 129, 160, 225, 156, 255, 228, 251},
	{188, 130, 160, 225, 156, 255, 228, 251},
	{189, 131, 160, 226, 157, 255, 228, 251},
	{190, 131, 160, 226, 158, 255, 228, 251},
	{191, 132, 161, 227, 159, 255, 229, 251},
	{192, 132, 161, 227, 159, 255, 229, 251},
	{193, 133, 162, 228, 160, 255, 230, 252},
	{194, 133, 162, 229, 160, 255, 230, 252},
	{195, 134, 163, 230, 161, 255, 231, 252},
	{196, 134, 163, 230, 161, 255, 231, 252},
	{197, 135, 163, 231, 162, 255, 231, 252},
	{198, 135, 163, 231, 162, 255, 231, 252},
	{199, 136, 164, 232, 163, 255, 232, 252},
	{200, 136, 164, 232, 164, 255, 232, 252},
	{201, 137, 165, 233, 165, 255, 233, 252},
	{201, 137, 165, 233, 165, 255, 233, 252},
	{202, 138, 166, 233, 166, 255, 233, 252},
	{203, 138, 166, 233, 166, 255, 233, 252},
	{204, 139, 166, 234, 167, 255, 234, 252},
	{205, 139, 166, 234, 167, 255, 234, 252},
	{206, 140, 167, 235, 168, 255, 235, 252},
	{206, 140, 167, 235, 168, 255, 235, 252},
	{207, 141, 168, 236, 169, 255, 235, 252},
	{208, 141, 168, 236, 170, 255, 235, 252},
	{209, 142, 169, 237, 171, 255, 236, 252},
	{209, 143, 169, 237, 171, 255, 236, 252},
	{210, 144, 169, 237, 172, 255, 236, 252},
	{211, 144, 169, 237, 172, 255, 236, 252},
	{212, 145, 170, 238, 173, 255, 237, 252},
	{213, 145, 170, 238, 173, 255, 237, 252},
	{214, 146, 171, 239, 174, 255, 237, 253},
	{214, 146, 171, 239, 174, 255, 237, 253},
	{215, 147, 172, 240, 175, 255, 238, 253},
	{215, 147, 172, 240, 175, 255, 238, 253},
	{216, 148, 173, 240, 176, 255, 238, 253},
	{217, 148, 173, 240, 176, 255, 238, 253},
	{218, 149, 173, 241, 177, 255, 239, 253},
	{218, 149, 173, 241, 178, 255, 239, 253},
	{219, 150, 174, 241, 179, 255, 239, 253},
	{219, 151, 174, 241, 179, 255, 239, 253},
	{220, 152, 175, 242, 180, 255, 240, 253},
	{221, 152, 175, 242, 180, 255, 240, 253},
	{222, 153, 176, 242, 181, 255, 240, 253},
	{222, 153, 176, 242, 181, 255, 240, 253},
	{223, 154, 177, 243, 182, 255, 240, 253},
	{223, 154, 177, 243, 182, 255, 240, 253},
	{224, 155, 178, 244, 183, 255, 241, 253},
	{224, 155, 178, 244, 183, 255, 241, 253},
	{225, 156, 178, 244, 184, 255, 241, 253},
	{225, 157, 178, 244, 184, 255, 241, 253},
	{226, 158, 179, 244, 185, 255, 242, 253},
	{227, 158, 179, 244, 185, 255, 242, 253},
	{228, 159, 180, 245, 186, 255, 242, 253},
	{228, 159, 180, 245, 186, 255, 242, 253},
	{229, 160, 181, 245, 187, 255, 242, 253},
	{229, 160, 181, 245, 187, 255, 242, 253},
	{230, 161, 182, 246, 188, 255, 243, 253},
	{230, 162, 182, 246, 188, 255, 243, 253},
	{231, 163, 183, 246, 189, 255, 243, 253},
	{231, 163, 183, 246, 189, 255, 243, 253},
	{232, 164, 184, 247, 190, 255, 243, 253},
	{232, 164, 184, 247, 190, 255, 243, 253},
	{233, 165, 185, 247, 191, 255, 244, 253},
	{233, 165, 185, 247, 191, 255, 244, 253},
	{234, 166, 185, 247, 192, 255, 244, 253},
	{234, 167, 185, 247, 192, 255, 244, 253},
	{235, 168, 186, 248, 193, 255, 244, 253},
	{235, 168, 186, 248, 193, 255, 244, 253},
	{236, 169, 187, 248, 194, 255, 244, 253},
	{236, 169, 187, 248, 194, 255, 244, 253},
	{236, 170, 188, 248, 195, 255, 245, 253},
	{236, 170, 188, 248, 195, 255, 245, 253},
	{237, 171, 189, 249, 196, 255, 245, 254},
	{237, 172, 189, 249, 196, 255, 245, 254},
	{238, 173, 190, 249, 197, 255, 245, 254},
	{238, 173, 190, 249, 197, 255, 245, 254},
	{239, 174, 191, 249, 198, 255, 245, 254},
	{239, 174, 191, 249, 198, 255, 245, 254},
	{240, 175, 192, 249, 199, 255, 246, 254},
	{240, 176, 192, 249, 199, 255, 246, 254},
	{240, 177, 193, 250, 200, 255, 246, 254},
	{240, 177, 193, 250, 200, 255, 246, 254},
	{241, 178, 194, 250, 201, 255, 246, 254},
	{241, 178, 194, 250, 201, 255, 246, 254},
	{242, 179, 195, 250, 202, 255, 246, 254},
	{242, 180, 195, 250, 202, 255, 246, 254},
	{242, 181, 196, 250, 203, 255, 247, 254},
	{242, 181, 196, 250, 203, 255, 247, 254},
	{243, 182, 197, 251, 204, 255, 247, 254},
	{243, 183, 197, 251, 204, 255, 247, 254},
	{244, 184, 198, 251, 205, 255, 247, 254},
	{244, 184, 198, 251, 205, 255, 247, 254},
	{244, 185, 199, 251, 206, 255, 247, 254},
	{244, 185, 199, 251, 206, 255, 247, 254},
	{245, 186, 200, 251, 207, 255, 247, 254},
	{245, 187, 200, 251, 207, 255, 247, 254},
	{246, 188, 201, 252, 207, 255, 248, 254},
	{246, 188, 201, 252, 207, 255, 248, 254},
	{246, 189, 202, 252, 208, 255, 248, 254},
	{246, 190, 202, 252, 208, 255, 248, 254},
	{247, 191, 203, 252, 209, 255, 248, 254},
	{247, 191, 203, 252, 209, 255, 248, 254},
	{247, 192, 204, 252, 210, 255, 248, 254},
	{247, 193, 204, 252, 210, 255, 248, 254},
	{248, 194, 205, 252, 211, 255, 248, 254},
	{248, 194, 205, 252, 211, 255, 248, 254},
	{248, 195, 206, 252, 212, 255, 249, 254},
	{248, 196, 206, 252, 212, 255, 249, 254},
	{249, 197, 207, 253, 213, 255, 249, 254},
	{249, 197, 207, 253, 213, 255, 249, 254},
	{249, 198, 208, 253, 214, 255, 249, 254},
	{249, 199, 209, 253, 214, 255, 249, 254},
	{250, 200, 210, 253, 215, 255, 249, 254},
	{250, 200, 210, 253, 215, 255, 249, 254},
	{250, 201, 211, 253, 215, 255, 249, 254},
	{250, 202, 211, 253, 215, 255, 249, 254},
	{250, 203, 212, 253, 216, 255, 249, 254},
	{250, 203, 212, 253, 216, 255, 249, 254},
	{251, 204, 213, 253, 217, 255, 250, 254},
	{251, 205, 213, 253, 217, 255, 250, 254},
	{251, 206, 214, 254, 218, 255, 250, 254},
	{251, 206, 215, 254, 218, 255, 250, 254},
	{252, 207, 216, 254, 219, 255, 250, 254},
	{252, 208, 216, 254, 219, 255, 250, 254},
	{252, 209, 217, 254, 220, 255, 250, 254},
	{252, 210, 217, 254, 220, 255, 250, 254},
	{252, 211, 218, 254, 221, 255, 250, 254},
	{252, 212, 218, 254, 221, 255, 250, 254},
	{253, 213, 219, 254, 222, 255, 250, 254},
	{253, 213, 220, 254, 222, 255, 250, 254},
	{253, 214, 221, 254, 223, 255, 250, 254},
	{253, 215, 221, 254, 223, 255, 250, 254},
	{253, 216, 222, 254, 224, 255, 251, 254},
	{253, 217, 223, 254, 224, 255, 251, 254},
	{253, 218, 224, 254, 225, 255, 251, 254},
	{253, 219, 224, 254, 225, 255, 251, 254},
	{254, 220, 225, 254, 225, 255, 251, 254},
	{254, 221, 226, 254, 225, 255, 251, 254},
	{254, 222, 227, 255, 226, 255, 251, 254},
	{254, 223, 227, 255, 226, 255, 251, 254},
	{254, 224, 228, 255, 227, 255, 251, 254},
	{254, 225, 229, 255, 227, 255, 251, 254},
	{254, 226, 230, 255, 228, 255, 251, 254},
	{254, 227, 230, 255, 229, 255, 251, 254},
	{255, 228, 231, 255, 230, 255, 251, 254},
	{255, 229, 232, 255, 230, 255, 251, 254},
	{255, 230, 233, 255, 231, 255, 252, 254},
	{255, 231, 234, 255, 231, 255, 252, 254},
	{255, 232, 235, 255, 232, 255, 252, 254},
	{255, 233, 236, 255, 232, 255, 252, 254},
	{255, 235, 237, 255, 233, 255, 252, 254},
	{255, 236, 238, 255, 234, 255, 252, 254},
	{255, 238, 240, 255, 235, 255, 252, 255},
	{255, 239, 241, 255, 235, 255, 252, 254},
	{255, 241, 243, 255, 236, 255, 252, 254},
	{255, 243, 245, 255, 237, 255, 252, 254},
	{255, 246, 247, 255, 239, 255, 253, 255},
};

const prob vp9::kf_y_mode_prob[INTRA_MODES][INTRA_MODES][INTRA_MODES - 1] = {
  {  // above = dc
    { 137,  30,  42, 148, 151, 207,  70,  52,  91 },	// left = dc
    {  92,  45, 102, 136, 116, 180,  74,  90, 100 },	// left = v
    {  73,  32,  19, 187, 222, 215,  46,  34, 100 },	// left = h
    {  91,  30,  32, 116, 121, 186,  93,  86,  94 },	// left = d45
    {  72,  35,  36, 149,  68, 206,  68,  63, 105 },	// left = d135
    {  73,  31,  28, 138,  57, 124,  55, 122, 151 },	// left = d117
    {  67,  23,  21, 140, 126, 197,  40,  37, 171 },	// left = d153
    {  86,  27,  28, 128, 154, 212,  45,  43,  53 },	// left = d207
    {  74,  32,  27, 107,  86, 160,  63, 134, 102 },	// left = d63
    {  59,  67,  44, 140, 161, 202,  78,  67, 119 }		// left = tm
  }, {  // above = v
    {  63,  36, 126, 146, 123, 158,  60,  90,  96 },	// left = dc
    {  43,  46, 168, 134, 107, 128,  69, 142,  92 },	// left = v
    {  44,  29,  68, 159, 201, 177,  50,  57,  77 },	// left = h
    {  58,  38,  76, 114,  97, 172,  78, 133,  92 },	// left = d45
    {  46,  41,  76, 140,  63, 184,  69, 112,  57 },	// left = d135
    {  38,  32,  85, 140,  46, 112,  54, 151, 133 },	// left = d117
    {  39,  27,  61, 131, 110, 175,  44,  75, 136 },	// left = d153
    {  52,  30,  74, 113, 130, 175,  51,  64,  58 },	// left = d207
    {  47,  35,  80, 100,  74, 143,  64, 163,  74 },	// left = d63
    {  36,  61, 116, 114, 128, 162,  80, 125,  82 }		// left = tm
  }, {  // above = h
    {  82,  26,  26, 171, 208, 204,  44,  32, 105 },	// left = dc
    {  55,  44,  68, 166, 179, 192,  57,  57, 108 },	// left = v
    {  42,  26,  11, 199, 241, 228,  23,  15,  85 },	// left = h
    {  68,  42,  19, 131, 160, 199,  55,  52,  83 },	// left = d45
    {  58,  50,  25, 139, 115, 232,  39,  52, 118 },	// left = d135
    {  50,  35,  33, 153, 104, 162,  64,  59, 131 },	// left = d117
    {  44,  24,  16, 150, 177, 202,  33,  19, 156 },	// left = d153
    {  55,  27,  12, 153, 203, 218,  26,  27,  49 },	// left = d207
    {  53,  49,  21, 110, 116, 168,  59,  80,  76 },	// left = d63
    {  38,  72,  19, 168, 203, 212,  50,  50, 107 }		// left = tm
  }, {  // above = d45
    { 103,  26,  36, 129, 132, 201,  83,  80,  93 },	// left = dc
    {  59,  38,  83, 112, 103, 162,  98, 136,  90 },	// left = v
    {  62,  30,  23, 158, 200, 207,  59,  57,  50 },	// left = h
    {  67,  30,  29,  84,  86, 191, 102,  91,  59 },	// left = d45
    {  60,  32,  33, 112,  71, 220,  64,  89, 104 },	// left = d135
    {  53,  26,  34, 130,  56, 149,  84, 120, 103 },	// left = d117
    {  53,  21,  23, 133, 109, 210,  56,  77, 172 },	// left = d153
    {  77,  19,  29, 112, 142, 228,  55,  66,  36 },	// left = d207
    {  61,  29,  29,  93,  97, 165,  83, 175, 162 },	// left = d63
    {  47,  47,  43, 114, 137, 181, 100,  99,  95 }		// left = tm
  }, {  // above = d135
    {  69,  23,  29, 128,  83, 199,  46,  44, 101 },	// left = dc
    {  53,  40,  55, 139,  69, 183,  61,  80, 110 },	// left = v
    {  40,  29,  19, 161, 180, 207,  43,  24,  91 },	// left = h
    {  60,  34,  19, 105,  61, 198,  53,  64,  89 },	// left = d45
    {  52,  31,  22, 158,  40, 209,  58,  62,  89 },	// left = d135
    {  44,  31,  29, 147,  46, 158,  56, 102, 198 },	// left = d117
    {  35,  19,  12, 135,  87, 209,  41,  45, 167 },	// left = d153
    {  55,  25,  21, 118,  95, 215,  38,  39,  66 },	// left = d207
    {  51,  38,  25, 113,  58, 164,  70,  93,  97 },	// left = d63
    {  47,  54,  34, 146, 108, 203,  72, 103, 151 }		// left = tm
  }, {  // above = d117
    {  64,  19,  37, 156,  66, 138,  49,  95, 133 },	// left = dc
    {  46,  27,  80, 150,  55, 124,  55, 121, 135 },	// left = v
    {  36,  23,  27, 165, 149, 166,  54,  64, 118 },	// left = h
    {  53,  21,  36, 131,  63, 163,  60, 109,  81 },	// left = d45
    {  40,  26,  35, 154,  40, 185,  51,  97, 123 },	// left = d135
    {  35,  19,  34, 179,  19,  97,  48, 129, 124 },	// left = d117
    {  36,  20,  26, 136,  62, 164,  33,  77, 154 },	// left = d153
    {  45,  18,  32, 130,  90, 157,  40,  79,  91 },	// left = d207
    {  45,  26,  28, 129,  45, 129,  49, 147, 123 },	// left = d63
    {  38,  44,  51, 136,  74, 162,  57,  97, 121 }		// left = tm
  }, {  // above = d153
    {  75,  17,  22, 136, 138, 185,  32,  34, 166 },	// left = dc
    {  56,  39,  58, 133, 117, 173,  48,  53, 187 },	// left = v
    {  35,  21,  12, 161, 212, 207,  20,  23, 145 },	// left = h
    {  56,  29,  19, 117, 109, 181,  55,  68, 112 },	// left = d45
    {  47,  29,  17, 153,  64, 220,  59,  51, 114 },	// left = d135
    {  46,  16,  24, 136,  76, 147,  41,  64, 172 },	// left = d117
    {  34,  17,  11, 108, 152, 187,  13,  15, 209 },	// left = d153
    {  51,  24,  14, 115, 133, 209,  32,  26, 104 },	// left = d207
    {  55,  30,  18, 122,  79, 179,  44,  88, 116 },	// left = d63
    {  37,  49,  25, 129, 168, 164,  41,  54, 148 }		// left = tm
  }, {  // above = d207
    {  82,  22,  32, 127, 143, 213,  39,  41,  70 },	// left = dc
    {  62,  44,  61, 123, 105, 189,  48,  57,  64 },	// left = v
    {  47,  25,  17, 175, 222, 220,  24,  30,  86 },	// left = h
    {  68,  36,  17, 106, 102, 206,  59,  74,  74 },	// left = d45
    {  57,  39,  23, 151,  68, 216,  55,  63,  58 },	// left = d135
    {  49,  30,  35, 141,  70, 168,  82,  40, 115 },	// left = d117
    {  51,  25,  15, 136, 129, 202,  38,  35, 139 },	// left = d153
    {  68,  26,  16, 111, 141, 215,  29,  28,  28 },	// left = d207
    {  59,  39,  19, 114,  75, 180,  77, 104,  42 },	// left = d63
    {  40,  61,  26, 126, 152, 206,  61,  59,  93 }		// left = tm
  }, {  // above = d63
    {  78,  23,  39, 111, 117, 170,  74, 124,  94 },	// left = dc
    {  48,  34,  86, 101,  92, 146,  78, 179, 134 },	// left = v
    {  47,  22,  24, 138, 187, 178,  68,  69,  59 },	// left = h
    {  56,  25,  33, 105, 112, 187,  95, 177, 129 },	// left = d45
    {  48,  31,  27, 114,  63, 183,  82, 116,  56 },	// left = d135
    {  43,  28,  37, 121,  63, 123,  61, 192, 169 },	// left = d117
    {  42,  17,  24, 109,  97, 177,  56,  76, 122 },	// left = d153
    {  58,  18,  28, 105, 139, 182,  70,  92,  63 },	// left = d207
    {  46,  23,  32,  74,  86, 150,  67, 183,  88 },	// left = d63
    {  36,  38,  48,  92, 122, 165,  88, 137,  91 }		// left = tm
  }, {  // above = tm
    {  65,  70,  60, 155, 159, 199,  61,  60,  81 },	// left = dc
    {  44,  78, 115, 132, 119, 173,  71, 112,  93 },	// left = v
    {  39,  38,  21, 184, 227, 206,  42,  32,  64 },	// left = h
    {  58,  47,  36, 124, 137, 193,  80,  82,  78 },	// left = d45
    {  49,  50,  35, 144,  95, 205,  63,  78,  59 },	// left = d135
    {  41,  53,  52, 148,  71, 142,  65, 128,  51 },	// left = d117
    {  40,  36,  28, 143, 143, 202,  40,  55, 137 },	// left = d153
    {  52,  34,  29, 129, 183, 227,  42,  35,  43 },	// left = d207
    {  42,  44,  44, 104, 105, 164,  64, 130,  80 },	// left = d63
    {  43,  81,  53, 140, 169, 204,  68,  84,  72 }		// left = tm
  }
};

const prob vp9::kf_uv_mode_prob[INTRA_MODES][INTRA_MODES - 1] = {
	{ 144,  11,  54, 157, 195, 130,  46,  58, 108 },	// y = dc
	{ 118,  15, 123, 148, 131, 101,  44,  93, 131 },	// y = v
	{ 113,  12,  23, 188, 226, 142,  26,  32, 125 },	// y = h
	{ 120,  11,  50, 123, 163, 135,  64,  77, 103 },	// y = d45
	{ 113,   9,  36, 155, 111, 157,  32,  44, 161 },	// y = d135
	{ 116,   9,  55, 176,  76,  96,  37,  61, 149 },	// y = d117
	{ 115,   9,  28, 141, 161, 167,  21,  25, 193 },	// y = d153
	{ 120,  12,  32, 145, 195, 142,  32,  38,  86 },	// y = d207
	{ 116,  12,  64, 120, 140, 125,  49, 115, 121 },	// y = d63
	{ 102,  19,  66, 162, 182, 122,  35,  59, 128 }		// y = tm
};

static const FrameContext default_frame_probs = {
{//static const prob default_if_y_probs[BLOCK_SIZE_GROUPS][INTRA_MODES - 1] = {
	{  65,  32,  18, 144, 162, 194,  41,  51,  98 },	// block_size < 8x8
	{ 132,  68,  18, 165, 217, 196,  45,  40,  78 },	// block_size < 16x16
	{ 173,  80,  19, 176, 240, 193,  64,  35,  46 },	// block_size < 32x32
	{ 221, 135,  38, 194, 248, 121,  96,  85,  29 }		// block_size >= 32x32
},
{//static const prob default_if_uv_probs[INTRA_MODES][INTRA_MODES - 1] = {
	{ 120,   7,  76, 176, 208, 126,  28,  54, 103 },	// y = dc
	{  48,  12, 154, 155, 139,  90,  34, 117, 119 },	// y = v
	{  67,   6,  25, 204, 243, 158,  13,  21,  96 },	// y = h
	{  97,   5,  44, 131, 176, 139,  48,  68,  97 },	// y = d45
	{  83,   5,  42, 156, 111, 152,  26,  49, 152 },	// y = d135
	{  80,   5,  58, 178,  74,  83,  33,  62, 145 },	// y = d117
	{  86,   5,  32, 154, 192, 168,  14,  22, 163 },	// y = d153
	{  85,   5,  32, 156, 216, 148,  19,  29,  73 },	// y = d207
	{  77,   7,  64, 116, 132, 122,  37, 126, 120 },	// y = d63
	{ 101,  21, 107, 181, 192, 103,  19,  67, 125 }		// y = tm
},
{//static const prob default_partition_probs[PARTITION_CONTEXTS][PARTITION_TYPES - 1] = {
	// 8x8 -> 4x4
	{ 199, 122, 141 },	// a/l both not split
	{ 147,  63, 159 },	// a split, l not split
	{ 148, 133, 118 },	// l split, a not split
	{ 121, 104, 114 },	// a/l both split
	// 16x16 -> 8x8
	{ 174,  73,  87 },	// a/l both not split
	{  92,  41,  83 },	// a split, l not split
	{  82,  99,  50 },	// l split, a not split
	{  53,  39,  39 },	// a/l both split
	// 32x32 -> 16x16
	{ 177,  58,  59 },	// a/l both not split
	{  68,  26,  63 },	// a split, l not split
	{  52,  79,  25 },	// l split, a not split
	{  17,  14,  12 },	// a/l both split
	// 64x64 -> 32x32
	{ 222,  34,  30 },	// a/l both not split
	{  72,  16,  44 },	// a split, l not split
	{  58,  32,  12 },	// l split, a not split
	{  10,   7,   6 },	// a/l both split
},
{//static const prob default_inter_mode_probs[INTER_MODE_CONTEXTS][INTER_MODES - 1] = {
	{2,		173,	34},	// 0 = both zero mv
	{7,		145,	85},	// 1 = one zero mv + one a predicted mv
	{7,		166,	63},	// 2 = two predicted mvs
	{7,		94,		66},	// 3 = one predicted/zero and one new mv
	{8,		64,		46},	// 4 = two new mvs
	{17,	81,		31},	// 5 = one intra neighbour + x
	{25,	29,		30},	// 6 = two intra neighbours
},
{//const prob default_switchable_interp_prob[SWITCHABLE_FILTER_CONTEXTS][SWITCHABLE_FILTERS - 1] = {
	{ 235, 162, },
	{ 36, 255, },
	{ 34, 3, },
	{ 149, 144, },
},
{//const prob default_intra_inter_p[INTRA_INTER_CONTEXTS] = {
  9, 102, 187, 225
},
{//const prob default_comp_inter_p[COMP_INTER_CONTEXTS] = {
  239, 183, 119,  96,  41
},
{//const prob default_single_ref_p[REF_CONTEXTS][2] = {
  {  33,  16 },
  {  77,  74 },
  { 142, 142 },
  { 172, 170 },
  { 238, 247 }
},
{//const prob default_comp_ref_p[REF_CONTEXTS] = {
  50, 126, 123, 221, 226
},
{//const prob default_skip_probs[SKIP_CONTEXTS] = {
	192, 128, 64
},
//const struct tx_probs default_tx_probs = {
  { { 3, 136, 37 },	{ 5, 52,  13 } },	//tx_32x32[TX_SIZE_CONTEXTS][TX_SIZES - 1];
  { { 20, 152 },	{ 15, 101 } },		//tx_16x16[TX_SIZE_CONTEXTS][TX_SIZES - 2];
  { { 100 },		{ 66  } },			//tx_8x8[TX_SIZE_CONTEXTS][TX_SIZES - 3];

//coef_probs
	{//const coeff_probs_model default_coef_probs[][PLANE_TYPES] = {
		{//TX_4x4
		  {  // Y plane
			{	// Intra
				{{ 195,  29, 183 }, {  84,  49, 136 }, {   8,  42,  71 }}, {														// Band 0
				{{  31, 107, 169 }, {  35,  99, 159 }, {  17,  82, 140 }, {   8,  66, 114 }, {   2,  44,  76 }, {   1,  19,  32 }},	// Band 1
				{{  40, 132, 201 }, {  29, 114, 187 }, {  13,  91, 157 }, {   7,  75, 127 }, {   3,  58,  95 }, {   1,  28,  47 }},	// Band 2
				{{  69, 142, 221 }, {  42, 122, 201 }, {  15,  91, 159 }, {   6,  67, 121 }, {   1,  42,  77 }, {   1,  17,  31 }},	// Band 3
				{{ 102, 148, 228 }, {  67, 117, 204 }, {  17,  82, 154 }, {   6,  59, 114 }, {   2,  39,  75 }, {   1,  15,  29 }},	// Band 4
				{{ 156,  57, 233 }, { 119,  57, 212 }, {  58,  48, 163 }, {  29,  40, 124 }, {  12,  30,  81 }, {   3,  12,  31 }}}	// Band 5
			}, {	// Inter
				{{ 191, 107, 226 }, { 124, 117, 204 }, {  25,  99, 155 }}, {
				{{  29, 148, 210 }, {  37, 126, 194 }, {   8,  93, 157 }, {   2,  68, 118 }, {   1,  39,  69 }, {   1,  17,  33 }},	// Band 1
				{{  41, 151, 213 }, {  27, 123, 193 }, {   3,  82, 144 }, {   1,  58, 105 }, {   1,  32,  60 }, {   1,  13,  26 }},	// Band 2
				{{  59, 159, 220 }, {  23, 126, 198 }, {   4,  88, 151 }, {   1,  66, 114 }, {   1,  38,  71 }, {   1,  18,  34 }},	// Band 3
				{{ 114, 136, 232 }, {  51, 114, 207 }, {  11,  83, 155 }, {   3,  56, 105 }, {   1,  33,  65 }, {   1,  17,  34 }},	// Band 4
				{{ 149,  65, 234 }, { 121,  57, 215 }, {  61,  49, 166 }, {  28,  36, 114 }, {  12,  25,  76 }, {   3,  16,  42 }}}	// Band 5
			}
		  }, {  // UV plane
			{	// Intra
				{{ 214,  49, 220 }, { 132,  63, 188 }, {  42,  65, 137 }}, {														// Band 0
				{{  85, 137, 221 }, { 104, 131, 216 }, {  49, 111, 192 }, {  21,  87, 155 }, {   2,  49,  87 }, {   1,  16,  28 }},	// Band 1
				{{  89, 163, 230 }, {  90, 137, 220 }, {  29, 100, 183 }, {  10,  70, 135 }, {   2,  42,  81 }, {   1,  17,  33 }},	// Band 2
				{{ 108, 167, 237 }, {  55, 133, 222 }, {  15,  97, 179 }, {   4,  72, 135 }, {   1,  45,  85 }, {   1,  19,  38 }},	// Band 3
				{{ 124, 146, 240 }, {  66, 124, 224 }, {  17,  88, 175 }, {   4,  58, 122 }, {   1,  36,  75 }, {   1,  18,  37 }},	// Band 4
				{{ 141,  79, 241 }, { 126,  70, 227 }, {  66,  58, 182 }, {  30,  44, 136 }, {  12,  34,  96 }, {   2,  20,  47 }}}	// Band 5
			}, { // Inter
				{{ 229,  99, 249 }, { 143, 111, 235 }, {  46, 109, 192 }}, {														// Band 0
				{{  82, 158, 236 }, {  94, 146, 224 }, {  25, 117, 191 }, {   9,  87, 149 }, {   3,  56,  99 }, {   1,  33,  57 }},	// Band 1
				{{  83, 167, 237 }, {  68, 145, 222 }, {  10, 103, 177 }, {   2,  72, 131 }, {   1,  41,  79 }, {   1,  20,  39 }},	// Band 2
				{{  99, 167, 239 }, {  47, 141, 224 }, {  10, 104, 178 }, {   2,  73, 133 }, {   1,  44,  85 }, {   1,  22,  47 }},	// Band 3
				{{ 127, 145, 243 }, {  71, 129, 228 }, {  17,  93, 177 }, {   3,  61, 124 }, {   1,  41,  84 }, {   1,  21,  52 }},	// Band 4
				{{ 157,  78, 244 }, { 140,  72, 231 }, {  69,  58, 184 }, {  31,  44, 137 }, {  14,  38, 105 }, {   8,  23,  61 }}}	// Band 5
			}
		  }
		},

		{//TX_8X8
		  {  // Y plane
			{		// Intra
				{{ 125,  34, 187 }, {  52,  41, 133 }, {   6,  31,  56 }}, {														// Band 0
				{{  37, 109, 153 }, {  51, 102, 147 }, {  23,  87, 128 }, {   8,  67, 101 }, {   1,  41,  63 }, {   1,  19,  29 }},	// Band 1
				{{  31, 154, 185 }, {  17, 127, 175 }, {   6,  96, 145 }, {   2,  73, 114 }, {   1,  51,  82 }, {   1,  28,  45 }},	// Band 2
				{{  23, 163, 200 }, {  10, 131, 185 }, {   2,  93, 148 }, {   1,  67, 111 }, {   1,  41,  69 }, {   1,  14,  24 }},	// Band 3
				{{  29, 176, 217 }, {  12, 145, 201 }, {   3, 101, 156 }, {   1,  69, 111 }, {   1,  39,  63 }, {   1,  14,  23 }},	// Band 4
				{{  57, 192, 233 }, {  25, 154, 215 }, {   6, 109, 167 }, {   3,  78, 118 }, {   1,  48,  69 }, {   1,  21,  29 }}}	// Band 5
			}, {	// Inter
				{{ 202, 105, 245 }, { 108, 106, 216 }, {  18,  90, 144 }}, {														// Band 0
				{{  33, 172, 219 }, {  64, 149, 206 }, {  14, 117, 177 }, {   5,  90, 141 }, {   2,  61,  95 }, {   1,  37,  57 }},	// Band 1
				{{  33, 179, 220 }, {  11, 140, 198 }, {   1,  89, 148 }, {   1,  60, 104 }, {   1,  33,  57 }, {   1,  12,  21 }},	// Band 2
				{{  30, 181, 221 }, {   8, 141, 198 }, {   1,  87, 145 }, {   1,  58, 100 }, {   1,  31,  55 }, {   1,  12,  20 }},	// Band 3
				{{  32, 186, 224 }, {   7, 142, 198 }, {   1,  86, 143 }, {   1,  58, 100 }, {   1,  31,  55 }, {   1,  12,  22 }},	// Band 4
				{{  57, 192, 227 }, {  20, 143, 204 }, {   3,  96, 154 }, {   1,  68, 112 }, {   1,  42,  69 }, {   1,  19,  32 }}}	// Band 5
			}
		  }, {  // UV plane
			{		// Intra
				{{ 212,  35, 215 }, { 113,  47, 169 }, {  29,  48, 105 }}, {														// Band 0
				{{  74, 129, 203 }, { 106, 120, 203 }, {  49, 107, 178 }, {  19,  84, 144 }, {   4,  50,  84 }, {   1,  15,  25 }},	// Band 1
				{{  71, 172, 217 }, {  44, 141, 209 }, {  15, 102, 173 }, {   6,  76, 133 }, {   2,  51,  89 }, {   1,  24,  42 }},	// Band 2
				{{  64, 185, 231 }, {  31, 148, 216 }, {   8, 103, 175 }, {   3,  74, 131 }, {   1,  46,  81 }, {   1,  18,  30 }},	// Band 3
				{{  65, 196, 235 }, {  25, 157, 221 }, {   5, 105, 174 }, {   1,  67, 120 }, {   1,  38,  69 }, {   1,  15,  30 }},	// Band 4
				{{  65, 204, 238 }, {  30, 156, 224 }, {   7, 107, 177 }, {   2,  70, 124 }, {   1,  42,  73 }, {   1,  18,  34 }}}	// Band 5
			}, {	// Inter
				{{ 225,  86, 251 }, { 144, 104, 235 }, {  42,  99, 181 }}, {														// Band 0
				{{  85, 175, 239 }, { 112, 165, 229 }, {  29, 136, 200 }, {  12, 103, 162 }, {   6,  77, 123 }, {   2,  53,  84 }},	// Band 1
				{{  75, 183, 239 }, {  30, 155, 221 }, {   3, 106, 171 }, {   1,  74, 128 }, {   1,  44,  76 }, {   1,  17,  28 }},	// Band 2
				{{  73, 185, 240 }, {  27, 159, 222 }, {   2, 107, 172 }, {   1,  75, 127 }, {   1,  42,  73 }, {   1,  17,  29 }},	// Band 3
				{{  62, 190, 238 }, {  21, 159, 222 }, {   2, 107, 172 }, {   1,  72, 122 }, {   1,  40,  71 }, {   1,  18,  32 }},	// Band 4
				{{  61, 199, 240 }, {  27, 161, 226 }, {   4, 113, 180 }, {   1,  76, 129 }, {   1,  46,  80 }, {   1,  23,  41 }}}	// Band 5
			}
		  }
		},

		{//TX_16X16
		  {  // Y plane
			{		// Intra
				{{   7,  27, 153 }, {   5,  30,  95 }, {   1,  16,  30 }}, {														// Band 0
				{{  50,  75, 127 }, {  57,  75, 124 }, {  27,  67, 108 }, {  10,  54,  86 }, {   1,  33,  52 }, {   1,  12,  18 }},	// Band 1
				{{  43, 125, 151 }, {  26, 108, 148 }, {   7,  83, 122 }, {   2,  59,  89 }, {   1,  38,  60 }, {   1,  17,  27 }},	// Band 2
				{{  23, 144, 163 }, {  13, 112, 154 }, {   2,  75, 117 }, {   1,  50,  81 }, {   1,  31,  51 }, {   1,  14,  23 }},	// Band 3
				{{  18, 162, 185 }, {   6, 123, 171 }, {   1,  78, 125 }, {   1,  51,  86 }, {   1,  31,  54 }, {   1,  14,  23 }},	// Band 4
				{{  15, 199, 227 }, {   3, 150, 204 }, {   1,  91, 146 }, {   1,  55,  95 }, {   1,  30,  53 }, {   1,  11,  20 }}}	// Band 5
			}, {	// Inter
				{{  19,  55, 240 }, {  19,  59, 196 }, {   3,  52, 105 }}, {														// Band 0
				{{  41, 166, 207 }, { 104, 153, 199 }, {  31, 123, 181 }, {  14, 101, 152 }, {   5,  72, 106 }, {   1,  36,  52 }},	// Band 1
				{{  35, 176, 211 }, {  12, 131, 190 }, {   2,  88, 144 }, {   1,  60, 101 }, {   1,  36,  60 }, {   1,  16,  28 }},	// Band 2
				{{  28, 183, 213 }, {   8, 134, 191 }, {   1,  86, 142 }, {   1,  56,  96 }, {   1,  30,  53 }, {   1,  12,  20 }},	// Band 3
				{{  20, 190, 215 }, {   4, 135, 192 }, {   1,  84, 139 }, {   1,  53,  91 }, {   1,  28,  49 }, {   1,  11,  20 }},	// Band 4
				{{  13, 196, 216 }, {   2, 137, 192 }, {   1,  86, 143 }, {   1,  57,  99 }, {   1,  32,  56 }, {   1,  13,  24 }}}	// Band 5
			}
		  }, {  // UV plane
			{		// Intra
				{{ 211,  29, 217 }, {  96,  47, 156 }, {  22,  43,  87 }}, {														// Band 0
				{{  78, 120, 193 }, { 111, 116, 186 }, {  46, 102, 164 }, {  15,  80, 128 }, {   2,  49,  76 }, {   1,  18,  28 }},	// Band 1
				{{  71, 161, 203 }, {  42, 132, 192 }, {  10,  98, 150 }, {   3,  69, 109 }, {   1,  44,  70 }, {   1,  18,  29 }},	// Band 2
				{{  57, 186, 211 }, {  30, 140, 196 }, {   4,  93, 146 }, {   1,  62, 102 }, {   1,  38,  65 }, {   1,  16,  27 }},	// Band 3
				{{  47, 199, 217 }, {  14, 145, 196 }, {   1,  88, 142 }, {   1,  57,  98 }, {   1,  36,  62 }, {   1,  15,  26 }},	// Band 4
				{{  26, 219, 229 }, {   5, 155, 207 }, {   1,  94, 151 }, {   1,  60, 104 }, {   1,  36,  62 }, {   1,  16,  28 }}}	// Band 5
			}, {	// Inter
				{{ 233,  29, 248 }, { 146,  47, 220 }, {  43,  52, 140 }}, {														// Band 0
				{{ 100, 163, 232 }, { 179, 161, 222 }, {  63, 142, 204 }, {  37, 113, 174 }, {  26,  89, 137 }, {  18,  68,  97 }},	// Band 1
				{{  85, 181, 230 }, {  32, 146, 209 }, {   7, 100, 164 }, {   3,  71, 121 }, {   1,  45,  77 }, {   1,  18,  30 }},	// Band 2
				{{  65, 187, 230 }, {  20, 148, 207 }, {   2,  97, 159 }, {   1,  68, 116 }, {   1,  40,  70 }, {   1,  14,  29 }},	// Band 3
				{{  40, 194, 227 }, {   8, 147, 204 }, {   1,  94, 155 }, {   1,  65, 112 }, {   1,  39,  66 }, {   1,  14,  26 }},	// Band 4
				{{  16, 208, 228 }, {   3, 151, 207 }, {   1,  98, 160 }, {   1,  67, 117 }, {   1,  41,  74 }, {   1,  17,  31 }}}	// Band 5
			}
		  }
		},

		{//TX_32X32
		  {  // Y plane
			{		// Intra
				{{  17,  38, 140 }, {   7,  34,  80 }, {   1,  17,  29 }}, {														// Band 0
				{{  37,  75, 128 }, {  41,  76, 128 }, {  26,  66, 116 }, {  12,  52,  94 }, {   2,  32,  55 }, {   1,  10,  16 }},	// Band 1
				{{  50, 127, 154 }, {  37, 109, 152 }, {  16,  82, 121 }, {   5,  59,  85 }, {   1,  35,  54 }, {   1,  13,  20 }},	// Band 2
				{{  40, 142, 167 }, {  17, 110, 157 }, {   2,  71, 112 }, {   1,  44,  72 }, {   1,  27,  45 }, {   1,  11,  17 }},	// Band 3
				{{  30, 175, 188 }, {   9, 124, 169 }, {   1,  74, 116 }, {   1,  48,  78 }, {   1,  30,  49 }, {   1,  11,  18 }},	// Band 4
				{{  10, 222, 223 }, {   2, 150, 194 }, {   1,  83, 128 }, {   1,  48,  79 }, {   1,  27,  45 }, {   1,  11,  17 }}}	// Band 5
			}, {	// Inter
				{{  36,  41, 235 }, {  29,  36, 193 }, {  10,  27, 111 }}, {														// Band 0
				{{  85, 165, 222 }, { 177, 162, 215 }, { 110, 135, 195 }, {  57, 113, 168 }, {  23,  83, 120 }, {  10,  49,  61 }},	// Band 1
				{{  85, 190, 223 }, {  36, 139, 200 }, {   5,  90, 146 }, {   1,  60, 103 }, {   1,  38,  65 }, {   1,  18,  30 }},	// Band 2
				{{  72, 202, 223 }, {  23, 141, 199 }, {   2,  86, 140 }, {   1,  56,  97 }, {   1,  36,  61 }, {   1,  16,  27 }},	// Band 3
				{{  55, 218, 225 }, {  13, 145, 200 }, {   1,  86, 141 }, {   1,  57,  99 }, {   1,  35,  61 }, {   1,  13,  22 }},	// Band 4
				{{  15, 235, 212 }, {   1, 132, 184 }, {   1,  84, 139 }, {   1,  57,  97 }, {   1,  34,  56 }, {   1,  14,  23 }}}	// Band 5
			}
		  }, {  // UV plane
			{		// Intra
				{{ 181,  21, 201 }, {  61,  37, 123 }, {  10,  38,  71 }}, {														// Band 0
				{{  47, 106, 172 }, {  95, 104, 173 }, {  42,  93, 159 }, {  18,  77, 131 }, {   4,  50,  81 }, {   1,  17,  23 }},	// Band 1
				{{  62, 147, 199 }, {  44, 130, 189 }, {  28, 102, 154 }, {  18,  75, 115 }, {   2,  44,  65 }, {   1,  12,  19 }},	// Band 2
				{{  55, 153, 210 }, {  24, 130, 194 }, {   3,  93, 146 }, {   1,  61,  97 }, {   1,  31,  50 }, {   1,  10,  16 }},	// Band 3
				{{  49, 186, 223 }, {  17, 148, 204 }, {   1,  96, 142 }, {   1,  53,  83 }, {   1,  26,  44 }, {   1,  11,  17 }},	// Band 4
				{{  13, 217, 212 }, {   2, 136, 180 }, {   1,  78, 124 }, {   1,  50,  83 }, {   1,  29,  49 }, {   1,  14,  23 }}}	// Band 5
			}, {	// Inter
				{{ 197,  13, 247 }, {  82,  17, 222 }, {  25,  17, 162 }}, {														// Band 0
				{{ 126, 186, 247 }, { 234, 191, 243 }, { 176, 177, 234 }, { 104, 158, 220 }, {  66, 128, 186 }, {  55,  90, 137 }},	// Band 1
				{{ 111, 197, 242 }, {  46, 158, 219 }, {   9, 104, 171 }, {   2,  65, 125 }, {   1,  44,  80 }, {   1,  17,  91 }},	// Band 2
				{{ 104, 208, 245 }, {  39, 168, 224 }, {   3, 109, 162 }, {   1,  79, 124 }, {   1,  50, 102 }, {   1,  43, 102 }},	// Band 3
				{{  84, 220, 246 }, {  31, 177, 231 }, {   2, 115, 180 }, {   1,  79, 134 }, {   1,  55,  77 }, {   1,  60,  79 }},	// Band 4
				{{  43, 243, 240 }, {   8, 180, 217 }, {   1, 115, 166 }, {   1,  84, 121 }, {   1,  51,  67 }, {   1,  16,   6 }}}	// Band 5
			}
		  }
		}
	},

// mv
	{//const nmv_context default_nmv_context = {
	  {32, 64, 96},
	  {
		{ // Vertical component
		  128,													// sign
		  {224, 144, 192, 168, 192, 176, 192, 198, 198, 245},	// class
		  {216},												// class0
		  {136, 140, 148, 160, 176, 192, 224, 234, 234, 240},	// bits
		  {{128, 128, 64}, {96, 112, 64}},						// class0_fp
		  {64, 96, 64},											// fp
		  160,													// class0_hp bit
		  128,													// hp
		},
		{ // Horizontal component
		  128,													// sign
		  {216, 128, 176, 160, 176, 176, 192, 198, 198, 208},	// class
		  {208},												// class0
		  {136, 140, 148, 160, 176, 192, 224, 234, 234, 240},	// bits
		  {{128, 128, 64}, {96, 112, 64}},						// class0_fp
		  {64, 96, 64},											// fp
		  160,													// class0_hp bit
		  128,													// hp
		}
	  },
	},
	// initialised
	true
};

//-------------------------------------
//	lookups
//-------------------------------------

const uint8 intra_mode_to_tx_type_lookup[INTRA_MODES] = {
	DCT_DCT,		// DC
	ADST_DCT,		// V
	DCT_ADST,		// H
	DCT_DCT,		// D45
	ADST_ADST,		// D135
	ADST_DCT,		// D117
	DCT_ADST,		// D153
	DCT_ADST,		// D207
	ADST_DCT,		// D63
	ADST_ADST,		// TM
};

force_inline BLOCK_SIZE get_subsize(TX_SIZE tx_size, PARTITION_TYPE partition) {
	static const uint8 subsize_lookup[][5] = {
		//TX_4X4			TX_8X8			TX_16X16		TX_32X32		TX_64X64
		{ BLOCK_4X4,		BLOCK_8X8,		BLOCK_16X16,	BLOCK_32X32,	BLOCK_64X64},	// PARTITION_NONE
		{ BLOCK_INVALID,	BLOCK_8X4,		BLOCK_16X8,		BLOCK_32X16,	BLOCK_64X32},	// PARTITION_HORZ
		{ BLOCK_INVALID,	BLOCK_4X8,		BLOCK_8X16,		BLOCK_16X32,	BLOCK_32X64},	// PARTITION_VERT
		{ BLOCK_INVALID,	BLOCK_4X4,		BLOCK_8X8,		BLOCK_16X16,	BLOCK_32X32},	// PARTITION_SPLIT
	};
	return (BLOCK_SIZE)subsize_lookup[partition][tx_size];
}

force_inline BLOCK_SIZE get_blocksize(int bwl, int bhl) {
	static const uint8 table[5][5] = {
		//TX_4X4		TX_8X8			TX_16X16		TX_32X32		TX_64X64
		{BLOCK_4X4,		BLOCK_8X4,		BLOCK_INVALID,	BLOCK_INVALID,	BLOCK_INVALID},	//TX_4X4
		{BLOCK_4X8,		BLOCK_8X8,		BLOCK_16X8,		BLOCK_INVALID,	BLOCK_INVALID},	//TX_8X8
		{BLOCK_INVALID,	BLOCK_8X16,		BLOCK_16X16,	BLOCK_32X16,	BLOCK_INVALID},	//TX_16X16
		{BLOCK_INVALID,	BLOCK_INVALID,	BLOCK_16X32,	BLOCK_32X32,	BLOCK_64X32},	//TX_32X32
		{BLOCK_INVALID,	BLOCK_INVALID,	BLOCK_INVALID,	BLOCK_32X64,	BLOCK_64X64},	//TX_64X64
	};
	return (BLOCK_SIZE)table[bhl][bwl];
}

BLOCK_SIZE vp9::get_plane_block_size(BLOCK_SIZE bsize, int xss, int yss) {
	static const uint8 ss_size_lookup[BLOCK_SIZES][2][2] = {
	//  x == 0, y = 0		x == 0, y == 1		x == 1, y == 0		x == 1, y == 1
		{{BLOCK_4X4,		BLOCK_INVALID},		{BLOCK_INVALID,		BLOCK_INVALID}},
		{{BLOCK_4X8,		BLOCK_4X4},			{BLOCK_INVALID,		BLOCK_INVALID}},
		{{BLOCK_8X4,		BLOCK_INVALID},		{BLOCK_4X4,			BLOCK_INVALID}},
		{{BLOCK_8X8,		BLOCK_8X4},			{BLOCK_4X8,			BLOCK_4X4}},
		{{BLOCK_8X16,		BLOCK_8X8},			{BLOCK_INVALID,		BLOCK_4X8}},
		{{BLOCK_16X8,		BLOCK_INVALID},		{BLOCK_8X8,			BLOCK_8X4}},
		{{BLOCK_16X16,		BLOCK_16X8},		{BLOCK_8X16,		BLOCK_8X8}},
		{{BLOCK_16X32,		BLOCK_16X16},		{BLOCK_INVALID,		BLOCK_8X16}},
		{{BLOCK_32X16,		BLOCK_INVALID},		{BLOCK_16X16,		BLOCK_16X8}},
		{{BLOCK_32X32,		BLOCK_32X16},		{BLOCK_16X32,		BLOCK_16X16}},
		{{BLOCK_32X64,		BLOCK_32X32},		{BLOCK_INVALID,		BLOCK_16X32}},
		{{BLOCK_64X32,		BLOCK_INVALID},		{BLOCK_32X32,		BLOCK_32X16}},
		{{BLOCK_64X64,		BLOCK_64X32},		{BLOCK_32X64,		BLOCK_32X32}},
	};
	return (BLOCK_SIZE)ss_size_lookup[bsize][xss][yss];
}

//-----------------------------------------------------------------------------
//	FrameBuffer
//-----------------------------------------------------------------------------

void copy_buffer(const Buffer2D &dest, const Buffer2D &srce, int w, int h) {
	const uint8 *s = srce.buffer;
	uint8		*d = dest.buffer;
	while (h--) {
		memcpy(d, s, w);
		d += dest.stride;
		s += srce.stride;
	}
}

int FrameBuffer::resize(int width, int height, const ColorSpace &cs, int border, int buffer_alignment, int stride_alignment, bool gpu) {
	*(ColorSpace*)this = cs;
	render_width	= width;
	render_height	= height;

	if (gpu) {
		if (width != y.width || height != y.height) {
			int		stride	= align(width, stride_alignment);
			y.init(width, height, 0, 0);
			uv.init(width >> cs.subsampling_x, height >> cs.subsampling_y, 0, 0);

			Texture	&tex	= (Texture&)texture;
		#ifdef PLAT_PS4
			tex.Init(GetTexFormat<rawint<uint8> >(), width, height * 3 / 2, 1, 1, MEM_LINEAR|MEM_WRITABLE|MEM_SYSTEM);
			mem_buffer		= tex.WriteData<void>();
			y_buffer.init((uint8*)mem_buffer, stride);
			u_buffer.init((uint8*)mem_buffer + stride * height, stride);
			v_buffer.init((uint8*)mem_buffer + stride * height + uv.width, stride);
		#else
			tex.Init(GetTexFormat<rawint<uint8[4]> >(), width / 4, height * 3 / 2, 1, 1, MEM_WRITABLE | MEM_CASTABLE);
		#endif
		}
	} else {
		int	alloc_width	= width + border * 2;
		int	y_stride	= align(alloc_width, stride_alignment);
		int	uv_stride	= align(alloc_width >> cs.subsampling_x, stride_alignment);

		y.init(width, height, border, border);
		uv.init(width >> cs.subsampling_x, height >> cs.subsampling_y, border >> cs.subsampling_x, border >> cs.subsampling_y);

		size_t	y_size	= y.height * y_stride;
		size_t	uv_size	= uv.height * uv_stride;
		size_t	size	= y_size + uv_size * 2;

		if (size > mem_size || intptr_t(mem_buffer) % buffer_alignment) {
			aligned_free(mem_buffer);
			mem_buffer = aligned_alloc(mem_size = size, buffer_alignment);
		}

		y_buffer.init(align((uint8*)mem_buffer + y.offset(y_stride), buffer_alignment), y_stride);
		u_buffer.init(align(y_buffer.buffer - y.offset(y_stride) + y_size + uv.offset(uv_stride), buffer_alignment), uv_stride);
		v_buffer.init(align(u_buffer.buffer + uv_size, buffer_alignment), uv_stride);
	}
	return 0;
}

void FrameBuffer::Plane::extend(const Buffer2D &src, int extend_top, int extend_left, int extend_bottom, int extend_right) const {
	// copy the left and right most columns out
	uint8	*src_ptr	= src.buffer;
	int		stride		= src.stride;
	for (int i = 0; i < crop_height; ++i) {
		memset(src_ptr - extend_left, src_ptr[0], extend_left);
		memset(src_ptr + crop_width, src_ptr[crop_width - 1], extend_right);
		src_ptr += stride;
	}

	// Now copy the top and bottom lines into each line of the respective borders
	const int linesize = extend_left + extend_right + crop_width;
	src_ptr = src.buffer - extend_left;
	uint8 *dst_ptr = src.row(-extend_top) - extend_left;
	for (int i = 0; i < extend_top; ++i) {
		memcpy(dst_ptr, src_ptr, linesize);
		dst_ptr += stride;
	}

	src_ptr = src.row(crop_height - 1) - extend_left;
	dst_ptr = src.row(crop_height) - extend_left;
	for (int i = 0; i < extend_bottom; ++i) {
		memcpy(dst_ptr, src_ptr, linesize);
		dst_ptr += stride;
	}
}

void FrameBuffer::Plane::copy_extend(const Buffer2D &src, const Buffer2D &dst, int extend_top, int extend_left, int extend_bottom, int extend_right) const {
	// copy the left and right most columns out
	const uint8 *src_ptr1 = src.buffer;
	const uint8 *src_ptr2 = src.buffer + crop_width - 1;
	uint8 *dst_ptr1 = dst.buffer - extend_left;
	uint8 *dst_ptr2 = dst.buffer + crop_width;

	for (int i = 0; i < crop_height; i++) {
		memset(dst_ptr1, src_ptr1[0], extend_left);
		memcpy(dst_ptr1 + extend_left, src_ptr1, crop_width);
		memset(dst_ptr2, src_ptr2[0], extend_right);
		src_ptr1 += src.stride;
		src_ptr2 += src.stride;
		dst_ptr1 += dst.stride;
		dst_ptr2 += dst.stride;
	}

	// Now copy the top and bottom lines into each line of the respective borders
	src_ptr1 = dst.buffer - extend_left;
	src_ptr2 = dst.buffer + dst.stride * (crop_height - 1) - extend_left;
	dst_ptr1 = dst.buffer + dst.stride * (-extend_top) - extend_left;
	dst_ptr2 = dst.buffer + dst.stride * crop_height - extend_left;

	int	linesize = extend_left + extend_right + crop_width;

	for (int i = 0; i < extend_top; i++) {
		memcpy(dst_ptr1, src_ptr1, linesize);
		dst_ptr1 += dst.stride;
	}

	for (int i = 0; i < extend_bottom; i++) {
		memcpy(dst_ptr2, src_ptr2, linesize);
		dst_ptr2 += dst.stride;
	}
}

void FrameBuffer::extend() const {
	y.extend(y_buffer);
	uv.extend(u_buffer);
	uv.extend(v_buffer);
}

void FrameBuffer::copy_extend(const FrameBuffer &src) const {
	y.copy_extend(src.y_buffer, y_buffer);
	uv.copy_extend(src.u_buffer, u_buffer);
	uv.copy_extend(src.v_buffer, v_buffer);
}

void scaled_2d(const uint8 *src, ptrdiff_t src_stride, uint8 *dst, ptrdiff_t dst_stride, const ScaleFactors::kernel *hfilters, int x_step_q4, const ScaleFactors::kernel *vfilters, int y_step_q4, int w, int h);

void vp9::scale_and_extend_frame(const FrameBuffer &src, FrameBuffer &dst) {
	const int src_w = src.y.crop_width;
	const int src_h = src.y.crop_height;
	const int dst_w = dst.y.crop_width;
	const int dst_h = dst.y.crop_height;
	const ScaleFactors::kernel	*kernel = filter_kernels[INTERP_8TAP];

	for (int i = 0; i < 3; ++i) {
		const FrameBuffer::Plane	&ps	= src.plane(i);
		const Buffer2D				&bs	= src.buffer(i);
		const Buffer2D				&bd	= dst.buffer(i);

		const int factor		= i == 0 || i == 3 ? 1 : 2;
		const int src_stride	= bs.stride;
		const int dst_stride	= bd.stride;
		for (int y = 0; y < dst_h; y += 16) {
			const int y_q4 = y * (16 / factor) * src_h / dst_h;
			for (int x = 0; x < dst_w; x += 16) {
				const int x_q4 = x * (16 / factor) * src_w / dst_w;
				const uint8 *src_ptr = bs.row(y / factor * src_h / dst_h) + x / factor * src_w / dst_w;
				uint8		*dst_ptr = bd.row(y / factor) + x / factor;
				scaled_2d(src_ptr, bs.stride, dst_ptr, bd.stride,
					kernel + (x_q4 & 0xf), 16 * src_w / dst_w,
					kernel + (y_q4 & 0xf), 16 * src_h / dst_h,
					16 / factor, 16 / factor
				);
			}
		}
	}

	//extend_frame(dst, dst.border);
}

void resize_plane(const uint8 *const input, int height, int width, int in_stride, uint8 *output, int height2, int width2, int out_stride);

void vp9::scale_and_extend_frame_nonnormative(const FrameBuffer &src, FrameBuffer &dst) {
	for (int i = 0; i < 3; ++i) {
		const FrameBuffer::Plane	&ps	= src.plane(i);
		const FrameBuffer::Plane	&pd	= dst.plane(i);
		const Buffer2D				&bs	= src.buffer(i);
		const Buffer2D				&bd	= dst.buffer(i);
		resize_plane(bs.buffer, ps.crop_height, ps.crop_width, bs.stride, bd.buffer, pd.crop_height, pd.crop_width, bd.stride);
	}
	//extend_frame(dst, dst.border);
}

//-----------------------------------------------------------------------------
//	TileDecoder
//-----------------------------------------------------------------------------

//-------------------------------------
//	coefficient decoding
//-------------------------------------

enum {
	CAT1_MIN_VAL	= 5,
	CAT2_MIN_VAL	= 7,
	CAT3_MIN_VAL	= 11,
	CAT4_MIN_VAL	= 19,
	CAT5_MIN_VAL	= 35,
	CAT6_MIN_VAL	= 67,
};
static const prob	cat1_prob[]			= { 159 };
static const prob	cat2_prob[]			= { 165, 145 };
static const prob	cat3_prob[]			= { 173, 148, 140 };
static const prob	cat4_prob[]			= { 176, 155, 140, 135 };
static const prob	cat5_prob[]			= { 180, 157, 141, 134, 130 };
static const prob	cat6_prob[]			= { 254, 254, 254, 252, 249, 243, 230, 196, 177, 153, 140, 133, 130, 129 };
static const uint8	pt_energy_class[]	= { 0, 1, 2, 3, 3, 4, 4, 5, 5, 5, 5, 5};

// beyond MAXBAND_INDEX(=21)+1 all values are filled as 5
static const uint8 coefband_trans_8x8plus[1024] = {
	0, 1, 1, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4,	4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
};
static const uint8 coefband_trans_4x4[16] = {
	0, 1, 1, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 5, 5, 5,
};

force_inline int get_coef_context(const int16 *neighbors, const uint8 *token_cache, int c) {
	return (1 + token_cache[neighbors[MAX_NEIGHBORS * c + 0]] + token_cache[neighbors[MAX_NEIGHBORS * c + 1]]) >> 1;
}

//with zigzag & dequant
int decode_coefs(vp9::reader &r, TX_SIZE tx_size, tran_low_t *dqcoeff, const int16 dq[2], int ctx, const int16 *scan, const int16 *neighbors,
	const Bands<prob[UNCONSTRAINED_NODES]>	&coef_probs,
	Bands<uint32[UNCONSTRAINED_NODES+1]>	*coef_counts,
	Bands<uint32>							*eob_branch_count
) {
	const int		max_eob			= 16 << (tx_size << 1);
	const uint8		*band_translate = tx_size == TX_4X4 ? coefband_trans_4x4 : coefband_trans_8x8plus;
	const int		dq_shift		= tx_size == TX_32X32;

	uint8			token_cache[32 * 32];
	int16			dqv				= dq[0];
	int				c				= 0;
	while (c < max_eob) {
		int	band			= *band_translate++;
		const prob	*prob	= coef_probs[band][ctx];
		if (eob_branch_count)
			++(*eob_branch_count)[band][ctx];

		if (!r.read(prob[EOB_CONTEXT_NODE])) {
			if (coef_counts)
				++(*coef_counts)[band][ctx][EOB_MODEL_TOKEN];
			return c;
		}

		while (!r.read(prob[ZERO_CONTEXT_NODE])) {
			if (coef_counts)
				++(*coef_counts)[band][ctx][ZERO_TOKEN];
			dqv = dq[1];
			token_cache[c] = 0;
			++c;
			if (c >= max_eob)
				return c;  // zero tokens at the end (no eob token)
			ctx		= get_coef_context(neighbors, token_cache, c);
			band	= *band_translate++;
			prob	= coef_probs[band][ctx];
		}

		int		val;
		int		token;
		if (!r.read(prob[ONE_CONTEXT_NODE])) {
			if (coef_counts)
				++(*coef_counts)[band][ctx][ONE_TOKEN];
			token	= ONE_TOKEN;
			val		= 1;
		} else {
			if (coef_counts)
				++(*coef_counts)[band][ctx][TWO_TOKEN];
			token = r.read_tree(coef_con_tree, pareto8_full[prob[PIVOT_NODE] - 1]);
			switch (token) {
				default:				val = token; break;
				case CATEGORY1_TOKEN:	val = CAT1_MIN_VAL + r.read(cat1_prob); break;
				case CATEGORY2_TOKEN:	val = CAT2_MIN_VAL + r.read(cat2_prob); break;
				case CATEGORY3_TOKEN:	val = CAT3_MIN_VAL + r.read(cat3_prob); break;
				case CATEGORY4_TOKEN:	val = CAT4_MIN_VAL + r.read(cat4_prob); break;
				case CATEGORY5_TOKEN:	val = CAT5_MIN_VAL + r.read(cat5_prob); break;
				case CATEGORY6_TOKEN:	val = CAT6_MIN_VAL + r.read(cat6_prob); break;
			}
		}
		int		v				= (val * dqv) >> dq_shift;
		dqcoeff[scan[c]]		= r.read_bit() ? -v : v;
		token_cache[c]			= pt_energy_class[token];

		++c;
		ctx		= get_coef_context(neighbors, token_cache, c);
		dqv		= dq[1];
	}

	return c;
}

//no zigzag or dequant
int decode_coefs(vp9::reader &r, TX_SIZE tx_size, tran_low_t *dqcoeff, int ctx, const int16 *neighbors,
	const Bands<prob[UNCONSTRAINED_NODES]>	&coef_probs,
	Bands<uint32[UNCONSTRAINED_NODES+1]>	*coef_counts,
	Bands<uint32>							*eob_branch_count
) {
	uint8			token_cache[32 * 32];
	const uint8		*band_translate = tx_size == TX_4X4 ? coefband_trans_4x4 : coefband_trans_8x8plus;
	const int		max_eob			= 16 << (tx_size << 1);
	int				c				= 0;

	while (c < max_eob) {
		int	band			= *band_translate++;
		const prob	*prob	= coef_probs[band][ctx];
		if (eob_branch_count)
			++(*eob_branch_count)[band][ctx];

		if (!r.read(prob[EOB_CONTEXT_NODE])) {
			if (coef_counts)
				++(*coef_counts)[band][ctx][EOB_MODEL_TOKEN];
			return c;
		}

		while (!r.read(prob[ZERO_CONTEXT_NODE])) {
			if (coef_counts)
				++(*coef_counts)[band][ctx][ZERO_TOKEN];
			dqcoeff[c]		= 0;
			token_cache[c]	= 0;
			++c;
			if (c >= max_eob)
				return c;  // zero tokens at the end (no eob token)
			ctx		= get_coef_context(neighbors, token_cache, c);
			band	= *band_translate++;
			prob	= coef_probs[band][ctx];
		}

		int		val;
		int		token;
		if (!r.read(prob[ONE_CONTEXT_NODE])) {
			if (coef_counts)
				++(*coef_counts)[band][ctx][ONE_TOKEN];
			token	= ONE_TOKEN;
			val		= 1;
		} else {
			if (coef_counts)
				++(*coef_counts)[band][ctx][TWO_TOKEN];
			token = r.read_tree(coef_con_tree, pareto8_full[prob[PIVOT_NODE] - 1]);
			switch (token) {
				default:				val = token; break;
				case CATEGORY1_TOKEN:	val = CAT1_MIN_VAL + r.read(cat1_prob); break;
				case CATEGORY2_TOKEN:	val = CAT2_MIN_VAL + r.read(cat2_prob); break;
				case CATEGORY3_TOKEN:	val = CAT3_MIN_VAL + r.read(cat3_prob); break;
				case CATEGORY4_TOKEN:	val = CAT4_MIN_VAL + r.read(cat4_prob); break;
				case CATEGORY5_TOKEN:	val = CAT5_MIN_VAL + r.read(cat5_prob); break;
				case CATEGORY6_TOKEN:	val = CAT6_MIN_VAL + r.read(cat6_prob); break;
			}
		}
		dqcoeff[c]		= r.read_bit() ? -val : val;
		token_cache[c]	= pt_energy_class[token];

		++c;
		ctx		= get_coef_context(neighbors, token_cache, c);
	}

	return c;
}

int coeff_alloc_size	= 65536;

int TileDecoder::decode_block_tokens(vp9::reader &r, int p, int entropy_ctx, TX_SIZE tx_size, TX_TYPE tx_type, int seg_id, const FrameContext &fc, GPU *gpu) {
	const PLANE_TYPE	type	= PLANE_TYPE(p > 0);
	const bool			ref		= mi[0]->is_inter_block();
	const ScanOrder		&sc		= scan_orders[tx_size][tx_type];

	if (gpu) {
		if (dqcoeff + (16 << (tx_size * 2)) > dqcoeff_end) {
			dqcoeff		= gpu->AllocCoeffs(coeff_alloc_size);
			dqcoeff_end = dqcoeff + coeff_alloc_size;
		}
		return decode_coefs(r, tx_size,
			dqcoeff,
			entropy_ctx,
			sc.neighbors,
			fc.coef_probs[tx_size][type][ref],
			counts ? &counts->coef[tx_size][type][ref] : 0,
			counts ? &counts->eob_branch[tx_size][type][ref] : 0
		);
	} else {
		return decode_coefs(r, tx_size,
			dqcoeff, seg_dequant[type][seg_id],//plane[p].seg_dequant[seg_id],
			entropy_ctx,
			sc.scan, sc.neighbors,
			fc.coef_probs[tx_size][type][ref],
			counts ? &counts->coef[tx_size][type][ref] : 0,
			counts ? &counts->eob_branch[tx_size][type][ref] : 0
		);
	}
}

//-------------------------------------
//	idct
//-------------------------------------

#define DCT_CONST_BITS		14

template<typename T> struct DCT_BITS {
	enum {BITS = 14};
};
template<> struct DCT_BITS<float> {
	enum {BITS = 0};
};

template<int N> struct DCT_FINAL_BITS;
template<> struct DCT_FINAL_BITS<4>		{ enum { BITS = 10	};};
template<> struct DCT_FINAL_BITS<8>		{ enum { BITS = 9	};};
template<> struct DCT_FINAL_BITS<16>	{ enum { BITS = 8	};};
template<> struct DCT_FINAL_BITS<32>	{ enum { BITS = 8	};};


// idct
template<int N, typename T> void idctn_add(const T *input, uint8 *dest, int stride, int nonzero_rows = N) {
	typedef dct<T, DCT_BITS<T>::BITS, N>	DCT;
#if 0
	transformRC<N>(input,
		DCT::idct,
		DCT::idct,
		[dest, stride, shift](int i, int j, T a)	{ clip_pixel_add(dest[i + j * stride], DCT::K::result(a, bits)); },
		nonzero_rows
	);
#else
	transformCR<T, N>(
		DCT::idct,
		DCT::idct,
		[input](int c, int r)				{ return input[r * N + c]; },
		[](T a)								{ return a; },
		[dest, stride](int c, int r, T a)	{ clip_pixel_add(dest[r * stride + c], DCT::K::result(a, DCT_FINAL_BITS<N>::BITS)); }
	);
#endif
}

template<int N, typename T> void idct1_add(const T dc, uint8 *dest, int stride) {
	typedef	dct_consts<T, DCT_BITS<T>::BITS>	K;
	int		a = K::result(K::idc(dc), DCT_FINAL_BITS<N>::BITS);
	for (int j = 0; j < N; ++j) {
		for (int i = 0; i < N; ++i)
			clip_pixel_add(dest[i + j * stride], a);
	}
}

template<int N> struct idct_add;

template<> struct idct_add<4> { template<typename T> static void f(const T *input, uint8 *dest, int stride, int eob) {
	if (eob <= 1)
		idct1_add<4>(input[0], dest, stride);
	else
		idctn_add<4>(input, dest, stride);
}};

template<> struct idct_add<8> {template<typename T> static void f(const T *input, uint8 *dest, int stride, int eob) {
	if (eob == 1)
		idct1_add<8>(input[0], dest, stride);
	else
		idctn_add<8>(input, dest, stride, eob <= 12 ? 4 : 8);
}};

template<> struct idct_add<16> {template<typename T> static void f(const T *input, uint8 *dest, int stride, int eob) {
	if (eob == 1)
		idct1_add<16>(input[0], dest, stride);
	else
		idctn_add<16>(input, dest, stride, eob <= 10 ? 4 : 16);
}};

template<> struct idct_add<32> {template<typename T> static void f(const T *input, uint8 *dest, int stride, int eob) {
	if (eob == 1)
		idct1_add<32>(input[0], dest, stride);
	else
		idctn_add<32>(input, dest, stride, eob <= 34 ? 8 : eob <= 135 ? 16 : 32);
}};

// iht
template<int N, typename T> void iht_add(const T *input, uint8 *dest, int stride, int eob, int tx_type) {
	if (tx_type == 0) {
		idct_add<N>::f(input, dest, stride, eob);
	} else {
		typedef dct<T, DCT_BITS<T>::BITS, N>	DCT;
	#if 0
		transformRC<N>(input,
			tx_type & 2 ? DCT::iadst : DCT::idct,
			tx_type & 1 ? DCT::iadst : DCT::idct,
			[dest, stride](int i, int j, T a) { clip_pixel_add(dest[i + j * stride], DCT::K::result(a, DCT_FINAL_BITS<N>::BITS)); }
		);
	#else
		transformCR<T, N>(
			tx_type & 1 ? DCT::iadst : DCT::idct,
			tx_type & 2 ? DCT::iadst : DCT::idct,
			[input](int c, int r)				{ return input[r * N + c]; },
			[](T a)								{ return a; },
			[dest, stride](int c, int r, T a)	{ clip_pixel_add(dest[r * stride + c], DCT::K::result(a, DCT_FINAL_BITS<N>::BITS)); }
		);
	#endif
	}
}

template<typename T> void iwht4x4_16_add(const T *input, uint8 *dest, int stride) {
	transformRC<4>(
		input,
		iwht4<T>,
		iwht4<T>,
		[dest, stride](int i, int j, int a) { clip_pixel_add(dest[i + j * stride], a); }
	);
}

template<typename T> void iwht4x4_1_add(const T dc, uint8 *dest, int stride) {
	T a1 = dc;
	T e1 = a1 / 2;
	a1 -= e1;

	const T tmp[4] = {a1, e1, e1, e1};

	for (int i = 0; i < 4; i++) {
		e1 = tmp[i] / 2;
		a1 = tmp[i] - e1;
		clip_pixel_add(dest[stride * 0], a1);
		clip_pixel_add(dest[stride * 1], e1);
		clip_pixel_add(dest[stride * 2], e1);
		clip_pixel_add(dest[stride * 3], e1);
		dest++;
	}
}

template<typename T> void iwht4x4_add(const T *input, uint8 *dest, int stride, int eob) {
	if (eob > 1)
		iwht4x4_16_add(input, dest, stride);
	else
		iwht4x4_1_add(input[0] / 4, dest, stride);
}

template<typename T> void clear_used_coeffs(T *dqcoeff, const TX_TYPE tx_type, const TX_SIZE tx_size, int eob) {
	int	n = eob == 1 ? 1
		: tx_type == DCT_DCT && tx_size <= TX_16X16 && eob <= 10 ? 4 * (4 << tx_size)
		: tx_size == TX_32X32 && eob <= 34 ? 256
		: 16 << (tx_size << 1);
	memset(dqcoeff, 0, n * sizeof(T));
}

template<typename T> void inverse_transform_block_inter(T *const dqcoeff, const TX_SIZE tx_size, uint8 *dst, int stride, int eob, bool lossless) {
	if (lossless) {
		iwht4x4_add(dqcoeff, dst, stride, eob);
	} else {
		switch (tx_size) {
			case TX_4X4:	idct_add< 4>::f(dqcoeff, dst, stride, eob);	break;
			case TX_8X8:	idct_add< 8>::f(dqcoeff, dst, stride, eob);	break;
			case TX_16X16:	idct_add<16>::f(dqcoeff, dst, stride, eob);	break;
			case TX_32X32:	idct_add<32>::f(dqcoeff, dst, stride, eob);	break;
		}
	}
}

template<typename T> void inverse_transform_block_intra(T *const dqcoeff, const TX_TYPE tx_type, const TX_SIZE tx_size, uint8 *dst, int stride, int eob, bool lossless) {
	if (lossless) {
		iwht4x4_add(dqcoeff, dst, stride, eob);
	} else {
		switch (tx_size) {
			case TX_4X4:	iht_add< 4>(dqcoeff, dst, stride, eob, tx_type);	break;
			case TX_8X8:	iht_add< 8>(dqcoeff, dst, stride, eob, tx_type);	break;
			case TX_16X16:	iht_add<16>(dqcoeff, dst, stride, eob, tx_type);	break;
			case TX_32X32:	idct_add<32>::f(dqcoeff, dst, stride, eob);			break;
		}
	}
}

template<typename T> T *dequant_zigzag(T *out, tran_low_t *const dqcoeff, const int16 dq[2], const TX_TYPE tx_type, const TX_SIZE tx_size, int eob) {
	const int16		*scan		= scan_orders[tx_size][tx_type].scan;

	memset(out, 0, (16 * sizeof(out[0])) << (tx_size * 2));

#if 0
	const float		dq_scale	= 1 << (DCT_CONST_BITS + (tx_size == TX_32X32));
	const float		dq0			= dq[0] / dq_scale;
	const float		dq1			= dq[1] / dq_scale;

	out[scan[0]] = dqcoeff[0] * dq0;
	for (int i = 1; i < eob; i++)
		out[scan[i]] = dqcoeff[i] * dq1;
#else
	const int		dq_shift	= tx_size == TX_32X32;
	const int		dq0			= dq[0];
	const int		dq1			= dq[1];

	out[scan[0]] = (dqcoeff[0] * dq0) >> dq_shift;
	for (int i = 1; i < eob; i++)
		out[scan[i]] = (dqcoeff[i] * dq1) >> dq_shift;
#endif

	return out;
}

//-------------------------------------
//	intra prediction
//-------------------------------------
template<typename T> void pred(PREDICTION_MODE M, TX_SIZE S, T *dst, ptrdiff_t stride, const T *above, const T *left);

void predict_intra_block(TX_SIZE tx_size, PREDICTION_MODE mode,
	const Buffer2D ref, const Buffer2D dst,
	int x0, int y0, bool up_available, bool left_available, bool right_available,
	int frame_width, int frame_height
) {
	DECLARE_ALIGNED(16, uint8, left_col[32]);
	DECLARE_ALIGNED(16, uint8, above_data[64 + 16]);
	uint8			*above_row	= above_data + 16;
	const int		bs			= 4 << tx_size;

	// 127 127 127 .. 127 127 127 127 127 127
	// 129  A   B  ..  Y   Z
	// 129  C   D  ..  W   X
	// 129  E   F  ..  U   V
	// 129  G   H  ..  S   T   T   T   T   T
	// ..

	if (mode == DC_PRED) {
		if (!left_available) {
			mode = DC_NO_L;
			if (!up_available)
				mode = DC_128;
		} else if (!up_available) {
			mode = DC_NO_A;
		}
	}

	//mode = DC_128;

	bool	need_above	= !!(extend_modes[mode] & NEED_ABOVE);
	bool	need_left	= !!(extend_modes[mode] & NEED_LEFT);
	bool	need_right	= !!(extend_modes[mode] & NEED_RIGHT);

	// NEED_LEFT
	if (need_left) {
		if (left_available) {
			if (y0 + bs > frame_height) {
				// slower path if the block needs border extension
				const int extend_bottom = frame_height - y0;
				int	i;
				for (i = 0; i < extend_bottom; ++i)
					left_col[i] = ref.row(y0 + i)[x0 - 1];
				for (; i < bs; ++i)
					left_col[i] = ref.row(frame_height - 1)[x0 - 1];
			} else {
				// faster path if the block does not need extension
				for (int i = 0; i < bs; ++i)
					left_col[i] = ref.row(y0 + i)[x0 - 1];
			}
		} else {
			memset(left_col, 129, bs);
		}
	}

	// NEED_ABOVE
	if (need_above) {
		if (up_available) {
			const uint8 *above_ref = ref.row(y0 - 1) + x0;
			if ((!need_left || left_available) && (!need_right || right_available)) {
				above_row = unconst(above_ref);

			} else {
				memcpy(above_row, above_ref, bs);
				above_row[-1] = left_available ? above_ref[-1] : 129;

				if (need_right) {
					if (right_available) {
						memcpy(above_row + bs, above_ref + bs, bs);
					} else {
						memset(above_row + bs, above_row[bs - 1], bs);
					}
				}
			}
		} else {
			memset(above_row - 1, 127, need_right ? bs * 2 + 1 : bs + 1);
		}
	}

	// predict
	pred(mode, tx_size, dst.row(y0) + x0, dst.stride, above_row, left_col);
}

//-------------------------------------
//	inter prediction
//-------------------------------------

void build_mc_border(const uint8 *src, int src_stride, uint8 *dst, int dst_stride, int x, int y, int b_w, int b_h, int w, int h) {
	// Get a pointer to the start of the real data for this row.
	const uint8 *ref_row = src - x - y * src_stride;
	if (y >= h)
		ref_row += (h - 1) * src_stride;
	else if (y > 0)
		ref_row += y * src_stride;

	int left	= clamp(-x, 0, b_w);
	int	right	= clamp(x + b_w - w, 0, b_w);
	int	copy	= b_w - left - right;

	while (b_h--) {
		if (left)
			memset(dst, ref_row[0], left);
		if (copy)
			memcpy(dst + left, ref_row + x + left, copy);
		if (right)
			memset(dst + left + copy, ref_row[w - 1], right);
		dst += dst_stride;
		++y;
		if (y > 0 && y < h)
			ref_row += src_stride;
	}
}

void build_inter_predictors(
	int xoffset, int yoffset, int x, int y, int w, int h,
	const ScaleFactors::kernel *kernel, const ScaleFactors &sf,	MotionVector32 mv,
	int frame_width, int frame_height,
	uint8 *ref_frame, int ref_stride,
	uint8 *dst_frame, int dst_stride,
	int ref
) {
	const bool		is_scaled = sf.is_scaled();

	// Co-ordinate of the block to 1/16th pixel precision.
	int				x0_16	= (xoffset + x) << ScaleFactors::SUBPEL_BITS;
	int				y0_16	= (yoffset + y) << ScaleFactors::SUBPEL_BITS;

	if (is_scaled) {
		mv		= mv.scale(sf);
		x0_16	= sf.scaled_x(x0_16);
		y0_16	= sf.scaled_y(y0_16);

		//transfer subpel to mv
		mv.row	+= y0_16 & ScaleFactors::SUBPEL_MASK;
		mv.col	+= x0_16 & ScaleFactors::SUBPEL_MASK;
		x0_16	&= ~ScaleFactors::SUBPEL_MASK;
		y0_16	&= ~ScaleFactors::SUBPEL_MASK;
	}

	x0_16	+= mv.col;
	y0_16	+= mv.row;

	int		subpel_x	= mv.col & ScaleFactors::SUBPEL_MASK;
	int		subpel_y	= mv.row & ScaleFactors::SUBPEL_MASK;
	int		x0			= x0_16 >> ScaleFactors::SUBPEL_BITS;
	int		y0			= y0_16 >> ScaleFactors::SUBPEL_BITS;

	// Get reference block pointer.
	uint8 *const	dst_ptr		= dst_frame + (yoffset + y) * dst_stride + xoffset + x;
	uint8 *			ref_ptr		= ref_frame + y0 * ref_stride + x0;

	// Do border extension if there is motion or the width/height is not a multiple of 8 pixels.
	if (is_scaled || mv.col || mv.row || ((frame_width | frame_height) & 0x7)) {
		const int	xs	= sf.x_step_q4();
		const int	ys	= sf.y_step_q4();
		int			x1	= ((x0_16 + (w - 1) * xs) >> ScaleFactors::SUBPEL_BITS) + 1;
		int			y1	= ((y0_16 + (h - 1) * ys) >> ScaleFactors::SUBPEL_BITS) + 1;

		bool	x_pad = subpel_x || xs != ScaleFactors::SUBPEL_SHIFTS;
		if (x_pad) {
			x0 -= ScaleFactors::SUBPEL_TAPS / 2 - 1;
			x1 += ScaleFactors::SUBPEL_TAPS / 2;
		}

		bool	y_pad = subpel_y || ys != ScaleFactors::SUBPEL_SHIFTS;
		if (y_pad) {
			y0 -= ScaleFactors::SUBPEL_TAPS / 2 - 1;
			y1 += ScaleFactors::SUBPEL_TAPS / 2;
		}

		// Skip border extension if block is inside the frame
		if (x0 < 0 || x1 > frame_width - 1 || y0 < 0 || y1 > frame_height - 1) {
			// Extend the border
			const int b_w	= x1 - x0 + 1;
			const int b_h	= y1 - y0 + 1;

			uint8	mc_buf[80 * 2 * 80 * 2];
			build_mc_border(ref_frame + y0 * ref_stride + x0, ref_stride, mc_buf, b_w, x0, y0, b_w, b_h, frame_width, frame_height);

			sf.inter_predictor(
				mc_buf + y_pad * 3 * b_w + x_pad * 3, b_w,
				dst_ptr, dst_stride,
				subpel_x, subpel_y, w, h, ref, kernel
			);
			return;
		}
	}
	sf.inter_predictor(ref_ptr, ref_stride, dst_ptr, dst_stride, subpel_x, subpel_y, w, h, ref, kernel);
}

//-------------------------------------
//	contexts
//-------------------------------------

// Returns a context number for the given MB prediction signal
// The mode info data structure has a one element border above and to the left of the entries corresponding to real blocks.
// The prediction flags in these dummy entries are initialized to 0.
int TileDecoder::get_tx_size_context(TX_SIZE max_tx_size) const {
	TX_SIZE above_ctx	= up_available		&& !above_mi->skip	? above_mi->tx_size	: max_tx_size;
	TX_SIZE left_ctx	= left_available	&& !left_mi->skip	? left_mi->tx_size	: max_tx_size;
	if (!left_available)
		left_ctx	= above_ctx;
	if (!up_available)
		above_ctx	= left_ctx;
	return (above_ctx + left_ctx) > max_tx_size;
}

int TileDecoder::get_pred_context_switchable_interp() const {
	// Note:
	// The mode info data structure has a one element border above and to the left of the entries corresponding to real macroblocks.
	// The prediction flags in these dummy entries are initialized to 0.
	const int left_type		= left_available && left_mi->is_inter_block() ?	left_mi->interp_filter : SWITCHABLE_FILTERS;
	const int above_type	= up_available && above_mi->is_inter_block() ? above_mi->interp_filter : SWITCHABLE_FILTERS;
	return left_type == above_type ? left_type
		: left_type == SWITCHABLE_FILTERS && above_type != SWITCHABLE_FILTERS ? above_type
		: left_type != SWITCHABLE_FILTERS && above_type == SWITCHABLE_FILTERS ? left_type
		: SWITCHABLE_FILTERS;
}

// The mode info data structure has a one element border above and to the left of the entries corresponding to real macroblocks.
// The prediction flags in these dummy entries are initialized to 0.
// 0 - inter/inter, inter/--, --/inter, --/--
// 1 - intra/inter, inter/intra
// 2 - intra/--, --/intra
// 3 - intra/intra
int TileDecoder::get_intra_inter_context() const {
	if (up_available && left_available) {  // both edges available
		return above_mi->is_inter_block() ? (left_mi->is_inter_block() ? 0 : 1) : (left_mi->is_inter_block() ? 1 : 3);
	} else if (up_available || left_available) {  // one edge available
		return 2 * !(up_available ? above_mi : left_mi)->is_inter_block();
	} else {
		return 0;
	}
}

int TileDecoder::get_reference_mode_context(const FrameContext &fc, REFERENCE_FRAME comp_ref) const {
	// Note:
	// The mode info data structure has a one element border above and to the left of the entries corresponding to real macroblocks.
	// The prediction flags in these dummy entries are initialized to 0.
	int ctx;
	if (up_available && left_available) {  // both edges available
		if (!above_mi->has_second_ref() && !left_mi->has_second_ref())
			// neither edge uses comp pred (0/1)
			ctx = (above_mi->ref_frame[0] == comp_ref) ^ (left_mi->ref_frame[0] == comp_ref);
		else if (!above_mi->has_second_ref())
			// one of two edges uses comp pred (2/3)
			ctx = 2 + (above_mi->ref_frame[0] == comp_ref || !above_mi->is_inter_block());
		else if (!left_mi->has_second_ref())
			// one of two edges uses comp pred (2/3)
			ctx = 2 + (left_mi->ref_frame[0] == comp_ref || !left_mi->is_inter_block());
		else  // both edges use comp pred (4)
			ctx = 4;
	} else if (up_available || left_available) {  // one edge available
		const ModeInfo *edge_mi = up_available ? above_mi : left_mi;
		if (!edge_mi->has_second_ref())
			// edge does not use comp pred (0/1)
			ctx = edge_mi->ref_frame[0] == comp_ref;
		else
			// edge uses comp pred (3)
			ctx = 3;
	} else {  // no edges available (1)
		ctx = 1;
	}
//	assert(ctx >= 0 && ctx < COMP_INTER_CONTEXTS);
	return ctx;
}

// Returns a context number for the given MB prediction signal
int TileDecoder::get_pred_context_comp_ref_p(const FrameContext &fc, const REFERENCE_FRAME comp_ref[3], bool var_ref_idx) const {
	// Note:
	// The mode info data structure has a one element border above and to the left of the entries corresponding to real macroblocks.
	// The prediction flags in these dummy entries are initialized to 0.
	int pred_context;
	if (up_available && left_available) {  // both edges available
		const bool above_intra	= !above_mi->is_inter_block();
		const bool left_intra	= !left_mi->is_inter_block();

		if (above_intra && left_intra) {  // intra/intra (2)
			pred_context = 2;
		} else if (above_intra || left_intra) {  // intra/inter
			const ModeInfo *edge_mi = above_intra ? left_mi : above_mi;

			if (!edge_mi->has_second_ref())  // single pred (1/3)
				pred_context = 1 + 2 * (edge_mi->ref_frame[0] != comp_ref[2]);
			else  // comp pred (1/3)
				pred_context = 1 + 2 * (edge_mi->ref_frame[var_ref_idx] != comp_ref[2]);
		} else {  // inter/inter
			const bool l_sg = !left_mi->has_second_ref();
			const bool a_sg = !above_mi->has_second_ref();
			const REFERENCE_FRAME vrfa = a_sg ? above_mi->ref_frame[0] : above_mi->ref_frame[var_ref_idx];
			const REFERENCE_FRAME vrfl = l_sg ? left_mi->ref_frame[0] : left_mi->ref_frame[var_ref_idx];

			if (vrfa == vrfl && comp_ref[2] == vrfa) {
				pred_context = 0;
			} else if (l_sg && a_sg) {  // single/single
				if ((vrfa == comp_ref[0] && vrfl == comp_ref[1]) || (vrfl == comp_ref[0] && vrfa == comp_ref[1]))
					pred_context = 4;
				else if (vrfa == vrfl)
					pred_context = 3;
				else
					pred_context = 1;
			} else if (l_sg || a_sg) {  // single/comp
				const REFERENCE_FRAME vrfc	= l_sg ? vrfa : vrfl;
				const REFERENCE_FRAME rfs	= a_sg ? vrfa : vrfl;
				if (vrfc == comp_ref[2] && rfs != comp_ref[2])
					pred_context = 1;
				else if (rfs == comp_ref[2] && vrfc != comp_ref[2])
					pred_context = 2;
				else
					pred_context = 4;
			} else if (vrfa == vrfl) {  // comp/comp
				pred_context = 4;
			} else {
				pred_context = 2;
			}
		}
	} else if (up_available || left_available) {  // one edge available
		const ModeInfo *edge_mi = up_available ? above_mi : left_mi;
		if (!edge_mi->is_inter_block()) {
			pred_context = 2;
		} else {
			if (edge_mi->has_second_ref())
				pred_context = 4 * (edge_mi->ref_frame[var_ref_idx] != comp_ref[2]);
			else
				pred_context = 3 * (edge_mi->ref_frame[0] != comp_ref[2]);
		}
	} else {  // no edges available (2)
		pred_context = 2;
	}
//	assert(pred_context >= 0 && pred_context < REF_CONTEXTS);
	return pred_context;
}

int TileDecoder::get_pred_context_single_ref_p1() const {
	int pred_context;
	// Note:
	// The mode info data structure has a one element border above and to the left of the entries corresponding to real macroblocks.
	// The prediction flags in these dummy entries are initialized to 0.
	if (up_available && left_available) {  // both edges available
		const bool above_intra	= !above_mi->is_inter_block();
		const bool left_intra	= !left_mi->is_inter_block();

		if (above_intra && left_intra) {  // intra/intra
			pred_context = 2;
		} else if (above_intra || left_intra) {  // intra/inter or inter/intra
			const ModeInfo *edge_mi = above_intra ? left_mi : above_mi;
			if (!edge_mi->has_second_ref())
				pred_context = 4 * (edge_mi->ref_frame[0] == REFFRAME_LAST);
			else
				pred_context = 1 + (edge_mi->ref_frame[0] == REFFRAME_LAST || edge_mi->ref_frame[1] == REFFRAME_LAST);
		} else {  // inter/inter
			const bool above_has_second = above_mi->has_second_ref();
			const bool left_has_second	= left_mi->has_second_ref();
			const REFERENCE_FRAME above0	= above_mi->ref_frame[0];
			const REFERENCE_FRAME above1	= above_mi->ref_frame[1];
			const REFERENCE_FRAME left0		= left_mi->ref_frame[0];
			const REFERENCE_FRAME left1		= left_mi->ref_frame[1];

			if (above_has_second && left_has_second) {
				pred_context = 1 + (above0 == REFFRAME_LAST || above1 == REFFRAME_LAST || left0 == REFFRAME_LAST || left1 == REFFRAME_LAST);
			} else if (above_has_second || left_has_second) {
				const REFERENCE_FRAME rfs	= !above_has_second ? above0 : left0;
				const REFERENCE_FRAME crf1	= above_has_second ? above0 : left0;
				const REFERENCE_FRAME crf2	= above_has_second ? above1 : left1;
				if (rfs == REFFRAME_LAST)
					pred_context = 3 + (crf1 == REFFRAME_LAST || crf2 == REFFRAME_LAST);
				else
					pred_context = (crf1 == REFFRAME_LAST || crf2 == REFFRAME_LAST);
			} else {
				pred_context = 2 * (above0 == REFFRAME_LAST) + 2 * (left0 == REFFRAME_LAST);
			}
		}
	} else if (up_available || left_available) {  // one edge available
		const ModeInfo *edge_mi = up_available ? above_mi : left_mi;
		if (!edge_mi->is_inter_block()) {  // intra
			pred_context = 2;
		} else {  // inter
			if (!edge_mi->has_second_ref())
				pred_context = 4 * (edge_mi->ref_frame[0] == REFFRAME_LAST);
			else
				pred_context = 1 + (edge_mi->ref_frame[0] == REFFRAME_LAST || edge_mi->ref_frame[1] == REFFRAME_LAST);
		}
	} else {  // no edges available
		pred_context = 2;
	}

//	assert(pred_context >= 0 && pred_context < REF_CONTEXTS);
	return pred_context;
}

int TileDecoder::get_pred_context_single_ref_p2() const {
	// Note:
	// The mode info data structure has a one element border above and to the left of the entries corresponding to real macroblocks.
	// The prediction flags in these dummy entries are initialized to 0.
	int pred_context;
	if (up_available && left_available) {  // both edges available
		const bool above_intra	= !above_mi->is_inter_block();
		const bool left_intra	= !left_mi->is_inter_block();
		if (above_intra && left_intra) {  // intra/intra
			pred_context = 2;
		} else if (above_intra || left_intra) {  // intra/inter or inter/intra
			const ModeInfo *edge_mi = above_intra ? left_mi : above_mi;
			if (!edge_mi->has_second_ref()) {
				if (edge_mi->ref_frame[0] == REFFRAME_LAST)
					pred_context = 3;
				else
					pred_context = 4 * (edge_mi->ref_frame[0] == REFFRAME_GOLDEN);
			} else {
				pred_context = 1 + 2 * (edge_mi->ref_frame[0] == REFFRAME_GOLDEN || edge_mi->ref_frame[1] == REFFRAME_GOLDEN);
			}
		} else {  // inter/inter
			const bool above_has_second		= above_mi->has_second_ref();
			const bool left_has_second		= left_mi->has_second_ref();
			const REFERENCE_FRAME above0	= above_mi->ref_frame[0];
			const REFERENCE_FRAME above1	= above_mi->ref_frame[1];
			const REFERENCE_FRAME left0		= left_mi->ref_frame[0];
			const REFERENCE_FRAME left1		= left_mi->ref_frame[1];

			if (above_has_second && left_has_second) {
				if (above0 == left0 && above1 == left1)
					pred_context = 3 * (above0 == REFFRAME_GOLDEN || above1 == REFFRAME_GOLDEN || left0 == REFFRAME_GOLDEN || left1 == REFFRAME_GOLDEN);
				else
					pred_context = 2;
			} else if (above_has_second || left_has_second) {
				const REFERENCE_FRAME rfs	= !above_has_second ? above0 : left0;
				const REFERENCE_FRAME crf1	= above_has_second ? above0 : left0;
				const REFERENCE_FRAME crf2	= above_has_second ? above1 : left1;

				if (rfs == REFFRAME_GOLDEN)
					pred_context = 3 + (crf1 == REFFRAME_GOLDEN || crf2 == REFFRAME_GOLDEN);
				else if (rfs == REFFRAME_ALTREF)
					pred_context = crf1 == REFFRAME_GOLDEN || crf2 == REFFRAME_GOLDEN;
				else
					pred_context = 1 + 2 * (crf1 == REFFRAME_GOLDEN || crf2 == REFFRAME_GOLDEN);
			} else {
				if (above0 == REFFRAME_LAST && left0 == REFFRAME_LAST) {
					pred_context = 3;
				} else if (above0 == REFFRAME_LAST || left0 == REFFRAME_LAST) {
					const REFERENCE_FRAME edge0 = (above0 == REFFRAME_LAST) ? left0 : above0;
					pred_context = 4 * (edge0 == REFFRAME_GOLDEN);
				} else {
					pred_context = 2 * (above0 == REFFRAME_GOLDEN) + 2 * (left0 == REFFRAME_GOLDEN);
				}
			}
		}
	} else if (up_available || left_available) {  // one edge available
		const ModeInfo *edge_mi = up_available ? above_mi : left_mi;

		if (!edge_mi->is_inter_block() || (edge_mi->ref_frame[0] == REFFRAME_LAST && !edge_mi->has_second_ref()))
			pred_context = 2;
		else if (!edge_mi->has_second_ref())
			pred_context = 4 * (edge_mi->ref_frame[0] == REFFRAME_GOLDEN);
		else
			pred_context = 3 * (edge_mi->ref_frame[0] == REFFRAME_GOLDEN || edge_mi->ref_frame[1] == REFFRAME_GOLDEN);
	} else {  // no edges available (2)
		pred_context = 2;
	}
//	assert(pred_context >= 0 && pred_context < REF_CONTEXTS);
	return pred_context;
}

#if 0
void TileDecoder::set_contexts(const Plane *pd, TX_SIZE tx_size, bool has_eob, int col, int row, int max_cols, int max_rows) const {
#if 1
	ENTROPY_CONTEXT *const a = pd->above_context + col;
	ENTROPY_CONTEXT *const l = pd->left_context + row;
	switch (tx_size) {
		default:
		case TX_4X4:	*a = *l = has_eob; break;
		case TX_8X8:	*(uint16*)a = *(uint16*)l = has_eob ? 0x0101 : 0; break;
		case TX_16X16:	*(uint32*)a = *(uint32*)l = has_eob ? 0x01010101 : 0; break;
		case TX_32X32:	*(uint64*)a = *(uint64*)l = has_eob ? 0x0101010101010101ul : 0; break;
	}
#else
	const int n = 1 << tx_size;

	// above
	ENTROPY_CONTEXT *const a = pd->above_context + col;
	if (has_eob && n > max_cols) {
		memset(a, has_eob, max_cols);
		memset(a + max_cols, 0, n - max_cols);
	} else {
		switch (tx_size) {}
		memset(a, has_eob, n);
	}

	// left
	ENTROPY_CONTEXT *const l = pd->left_context + row;
	if (has_eob && n > max_rows) {
		memset(l, has_eob, max_rows);
		memset(l + max_rows, 0, n - max_rows);
	} else {
		memset(l, has_eob, n);
	}
#endif
}
#endif

//-----------------------------------------------------------------------------
//	FrameContext
//-----------------------------------------------------------------------------
static const prob factors[] = {0, 6, 12, 19, 25, 32, 38, 44, 51, 57, 64, 70, 76, 83, 89, 96, 102, 108, 115, 121};

template<int A, int B, uint32 N> void merge_probs(const tree_index *tree, const prob (&prev)[A][B], const uint32 (&counts)[A][B+1], prob (&probs)[A][B], const prob (&factors)[N]) {
	for (int i = 0; i < A; i++)
		prob_code::merge_probs(tree, prev[i], counts[i], probs[i], factors);
}

template<int A, uint32 N> force_inline void merge_probs(prob (&probs)[A], prob (&prev)[A], const uint32 (&counts)[A][2], const prob (&factors)[N]) {
	prob_code::merge_probs(probs, prev, counts, A, factors, N);
}

void FrameContext::adapt_mode_probs(FrameContext &prev, FrameCounts &counts, bool interp_switchable, bool tx_select) {
	merge_probs(intra_inter_prob, prev.intra_inter_prob, counts.intra_inter, factors);
	merge_probs(comp_inter_prob, prev.comp_inter_prob, counts.comp_inter, factors);
	merge_probs(comp_ref_prob, prev.comp_ref_prob, counts.comp_ref, factors);

	prob_code::merge_probs(single_ref_prob[0], prev.single_ref_prob[0], counts.single_ref[0], REF_CONTEXTS * 2, factors, num_elements(factors));

//	for (int i = 0; i < REF_CONTEXTS; i++) {
//		for (int j = 0; j < 2; j++)
//			single_ref_prob[i][j] = prob_code::merge_probs(prev.single_ref_prob[i][j], counts.single_ref[i][j], factors);
//	}

	merge_probs(inter_mode_tree, prev.inter_mode_probs, counts.inter_mode, inter_mode_probs, factors);
	merge_probs(intra_mode_tree, prev.y_mode_prob, counts.y_mode, y_mode_prob, factors);
	merge_probs(intra_mode_tree, prev.uv_mode_prob, counts.uv_mode, uv_mode_prob, factors);
	merge_probs(partition_tree, prev.partition_prob, counts.partition, partition_prob, factors);

	if (interp_switchable)
		merge_probs(switchable_interp_tree, prev.switchable_interp_prob, counts.switchable_interp, switchable_interp_prob, factors);

	if (tx_select) {//tx_mode == TX_MODE_SELECT) {
		for (int i = 0; i < TX_SIZE_CONTEXTS; ++i) {
			tx_8x8[i][0]	= prob_code::merge_probs(prev.tx_8x8[i][0],		counts.tx_8x8[i][TX_4X4],		counts.tx_8x8[i][TX_8X8],																	factors);
			tx_16x16[i][0]	= prob_code::merge_probs(prev.tx_16x16[i][0],	counts.tx_16x16[i][TX_4X4],		counts.tx_16x16[i][TX_8X8] + counts.tx_16x16[i][TX_16X16],									factors);
			tx_16x16[i][1]	= prob_code::merge_probs(prev.tx_16x16[i][1],	counts.tx_16x16[i][TX_8X8],		counts.tx_16x16[i][TX_16X16],																factors);
			tx_32x32[i][0]	= prob_code::merge_probs(prev.tx_32x32[i][0],	counts.tx_32x32[i][TX_4X4],		counts.tx_32x32[i][TX_8X8] + counts.tx_32x32[i][TX_16X16] + counts.tx_32x32[i][TX_32X32],	factors);
			tx_32x32[i][1]	= prob_code::merge_probs(prev.tx_32x32[i][1],	counts.tx_32x32[i][TX_8X8],		counts.tx_32x32[i][TX_16X16] + counts.tx_32x32[i][TX_32X32],								factors);
			tx_32x32[i][2]	= prob_code::merge_probs(prev.tx_32x32[i][2],	counts.tx_32x32[i][TX_16X16],	counts.tx_32x32[i][TX_32X32],																factors);
		}
	}

	merge_probs(skip_probs, prev.skip_probs, counts.skip, factors);
}

void FrameContext::adapt_mv_probs(FrameContext &prev, FrameCounts &counts, bool allow_high_precision_mv) {
	prob_code::merge_probs(mv_joint_tree, prev.mv.joints, counts.mv.joints, mv.joints, factors);

	for (int i = 0; i < 2; ++i) {
		mvs::component						&comp		= mv.comps[i];
		const mvs::component				&pre_comp	= prev.mv.comps[i];
		const FrameCounts::mvs::component	&cnt		= counts.mv.comps[i];

		comp.sign = prob_code::merge_probs(pre_comp.sign, cnt.sign, factors);
		prob_code::merge_probs(mv_class_tree, pre_comp.classes, cnt.classes, comp.classes, factors);
		prob_code::merge_probs(mv_class0_tree, pre_comp.class0, cnt.class0, comp.class0, factors);

		for (int j = 0; j < MotionVector::OFFSET_BITS; ++j)
			comp.bits[j] = prob_code::merge_probs(pre_comp.bits[j], cnt.bits[j], factors);

		merge_probs(mv_fp_tree, pre_comp.class0_fp, cnt.class0_fp, comp.class0_fp, factors);

		prob_code::merge_probs(mv_fp_tree, pre_comp.fp, cnt.fp, comp.fp, factors);

		if (allow_high_precision_mv) {
			comp.class0_hp = prob_code::merge_probs(pre_comp.class0_hp, cnt.class0_hp, factors);
			comp.hp = prob_code::merge_probs(pre_comp.hp, cnt.hp, factors);
		}
	}
}

void FrameContext::adapt_coef_probs(FrameContext &prev, FrameCounts &counts, uint32 count_sat, uint32 update_factor) {
	for (int t = TX_4X4; t <= TX_32X32; t++) {
		coeffs				*const	probs		= coef_probs[t];
		const coeffs		*const	pre_probs	= prev.coef_probs[t];
		FrameCounts::coeffs *const	c			= counts.coef[t];
		Bands<uint32>	(*eob_counts)[REF_TYPES]	= counts.eob_branch[t];

		for (int i = 0; i < PLANE_TYPES; ++i) {
			for (int j = 0; j < REF_TYPES; ++j) {
				const uint32	(*cp)[UNCONSTRAINED_NODES + 1]	= c[i][j].all();
				const uint32	*eobp							= eob_counts[i][j].all();
				prob			(*pp)[UNCONSTRAINED_NODES]		= probs[i][j].all();
				const prob		(*ppre)[UNCONSTRAINED_NODES]	= pre_probs[i][j].all();
				for (int k = 0; k < Bands<uint32>::TOTAL; ++k) {
					const int n0	= cp[k][ZERO_TOKEN];
					const int n1	= cp[k][ONE_TOKEN];
					const int n2	= cp[k][TWO_TOKEN];
					const int neob	= cp[k][EOB_MODEL_TOKEN];

					pp[k][0] = prob_code::merge_probs(ppre[k][0], neob,	eobp[k] - neob,	count_sat, update_factor);
					pp[k][1] = prob_code::merge_probs(ppre[k][1], n0,	n1 + n2,		count_sat, update_factor);
					pp[k][2] = prob_code::merge_probs(ppre[k][2], n1,	n2,				count_sat, update_factor);
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
//	Common
//-----------------------------------------------------------------------------

Common::Common() {
#if 1//def PLAT_PS4
	gpu	= new GPU;
#endif
}

void Common::loop_filter_frame_init() {
	int filter_level	= lf.filter_level;
	// n_shift sis the multiplier for lf_deltas the multiplier is 1 for when filter_lvl is between 0 and 31; 2 when filter_lvl is between 32 and 63
	const int shift		= filter_level >> 5;

	// update limits if sharpness has changed
	if (lf.last_sharpness_level != lf.sharpness_level) {
		lf_info.update_sharpness(lf.sharpness_level);
		lf.last_sharpness_level = lf.sharpness_level;
	}

	for (int seg_id = 0; seg_id < MAX_SEGMENTS; seg_id++) {
		int lvl_seg = seg.get_data(seg_id, Segmentation::FEATURE_ALT_LF, filter_level);

		if (!lf.mode_ref_delta_enabled) {
			// we could get rid of this if we assume that deltas are set to zero when not in use; encoder always uses deltas
			memset(lf_info.level[seg_id], lvl_seg, sizeof(lf_info.level[seg_id]));

		} else {
			lf_info.level[seg_id][REFFRAME_INTRA][0] = clamp(lvl_seg + (lf.ref_deltas[REFFRAME_INTRA] << shift), 0, LoopFilter::MAX_LOOP_FILTER);

			for (int ref = REFFRAME_LAST; ref < REFFRAMES; ++ref) {
				for (int mode = 0; mode < LoopFilter::MAX_MODE_LF_DELTAS; ++mode)
					lf_info.level[seg_id][ref][mode] = clamp(lvl_seg + ((lf.ref_deltas[ref] + lf.mode_deltas[mode]) << shift), 0, LoopFilter::MAX_LOOP_FILTER);
			}
		}
	}

	memset(lfm, 0, lfm.size() * sizeof(LoopFilterMasks));
}

Common::~Common() {
	delete gpu;
}

void Common::loop_filter_rows(int start, int stop, bool y_only) {
	//ISO_OUTPUTF("filtering %i\n", start >> 3);
	const int	num_planes	= y_only ? 1 : 3;
	const int	ssx			= cur_frame->subsampling_x;
	const int	ssy			= cur_frame->subsampling_y;

	for (int mi_row = start; mi_row < stop; mi_row += MI_BLOCK_SIZE) {
		ModeInfo		**mi	= mi_grid + (mi_row + 1) * mi_stride + 1;
		LoopFilterMasks *lfm	= get_masks(mi_row, 0);

		for (int mi_col = 0; mi_col < mi_cols; mi_col += MI_BLOCK_SIZE, ++lfm) {
			lfm->adjust_mask(mi_row, mi_col, mi_rows, mi_cols);

			lf_info.filter_block_plane_ss00(cur_frame->y_buffer, mi_row, lfm, mi_rows, mi_cols, mi_stride);

			for (int p = 1; p < num_planes; ++p) {
				const Buffer2D	&buffer = cur_frame->buffer(p);
				if (ssx == 1 && ssy == 1)
					lf_info.filter_block_plane_ss11(buffer, mi_row, lfm, mi_rows, mi_cols, mi_stride);
				else if (ssx == 0 && ssy == 0)
					lf_info.filter_block_plane_ss00(buffer, mi_row, lfm, mi_rows, mi_cols, mi_stride);
				else
					lf_info.filter_block_plane_non420(buffer, ssx, ssy, mi + mi_col, mi_row, mi_col, mi_rows, mi_cols, mi_stride);
			}
		}
	}
}

void Common::setup_past_independence(RESET reset_frame_context) {
	// Reset the segment feature data to the default state
	seg.reset();
	seg_map[0].clear_contents();
	seg_map[1].clear_contents();

	// Reset the mode ref deltas for loop filter
	lf.reset();

	fc = default_frame_probs;

	if (reset_frame_context == RESET_ALL) {
		// Reset all frame contexts.
		for (int i = 0; i < FRAME_CONTEXTS; ++i)
			frame_contexts[i] = fc;

	} else if (reset_frame_context == RESET_THIS) {
		// Reset only the frame context specified in the frame header.
		*pre_fc = fc;
	}

	frame_context_idx	= 0;
	pre_fc				= &frame_contexts[0];

	clear(ref_frame_sign_bias);
}

int get_tile_offset(int idx, int mis, int log2) {
	const int sb_cols = (mis + (1 << MI_BLOCK_SIZE_LOG2) - 1) >> MI_BLOCK_SIZE_LOG2;
	const int offset = ((idx * sb_cols) >> log2) << MI_BLOCK_SIZE_LOG2;
	return min(offset, mis);
}

void Common::get_tile_offsets(int tile_col, int tile_row, int *mi_col, int *mi_row) const {
	*mi_row	= get_tile_offset(tile_row, mi_rows, log2_tile_rows);
	*mi_col	= get_tile_offset(tile_col, mi_cols, log2_tile_cols);
}

FrameBuffer &Common::scale_if_required(FrameBuffer &unscaled, FrameBuffer &scaled, int use_normative_scaler) {
	if (mi_cols * MI_SIZE != unscaled.y.width || mi_rows * MI_SIZE != unscaled.y.height) {
		if (use_normative_scaler && unscaled.y.width <= (scaled.y.width << 1) && unscaled.y.height <= (scaled.y.height << 1))
			scale_and_extend_frame(unscaled, scaled);
		else
			scale_and_extend_frame_nonnormative(unscaled, scaled);
		return scaled;
	} else {
		return unscaled;
	}
}

//-----------------------------------------------------------------------------
//	Decoder
//-----------------------------------------------------------------------------

Decoder::Decoder() : need_resync(true) {
}

int get_segment_id(const uint8 *segment_ids, int mi_cols, int mi_offset, int x_mis, int y_mis) {
	int	segment_id = INT_MAX;
	for (int y = 0; y < y_mis; y++)
		for (int x = 0; x < x_mis; x++)
			segment_id = min(segment_id, segment_ids[mi_offset + y * mi_cols + x]);
	return segment_id;
}
void set_segment_id(uint8 *segment_ids, int mi_cols, int mi_offset, int x_mis, int y_mis, int segment_id) {
	for (int y = 0; y < y_mis; y++)
		memset(segment_ids + mi_offset + y * mi_cols, segment_id, x_mis);
}
void copy_segment_id(const uint8 *from, uint8 *to, int mi_cols, int mi_offset, int x_mis, int y_mis) {
	for (int y = 0; y < y_mis; y++)
		memcpy(to + mi_offset + y * mi_cols, from + mi_offset + y * mi_cols, x_mis);
}

static PREDICTION_MODE read_intra_mode(vp9::reader &r, const prob *p) {
	return (PREDICTION_MODE)r.read_tree(intra_mode_tree, p);
}

static PREDICTION_MODE read_inter_mode(vp9::reader &r, const FrameContext &fc, FrameCounts *counts, int ctx) {
	const int mode = r.read_tree(inter_mode_tree, fc.inter_mode_probs[ctx]);
	if (counts)
		++counts->inter_mode[ctx][mode];

	return PREDICTION_MODE(NEARESTMV + mode);
}

static int read_segment_id(vp9::reader &r, const Segmentation *seg) {
	return r.read_tree(segment_tree, seg->tree_probs);
}

TX_SIZE read_tx_size(vp9::reader &r, TileDecoder *xd, const FrameContext &fc, TX_MODE tx_mode, bool allow_select) {
	BLOCK_SIZE		bsize		= xd->mi[0]->sb_type;
	const TX_SIZE	max_tx_size = ModeInfo::get_max_txsize(bsize);

	if (allow_select && tx_mode == TX_MODE_SELECT && bsize >= BLOCK_8X8) {
		const int	ctx			= xd->get_tx_size_context(max_tx_size);
		const prob *tx_probs	= fc.get_tx_probs(max_tx_size, ctx);
		int			tx_size		= r.read(tx_probs[0]);

		if (tx_size != TX_4X4 && max_tx_size >= TX_16X16) {
			tx_size += r.read(tx_probs[1]);
			if (tx_size != TX_8X8 && max_tx_size >= TX_32X32)
				tx_size += r.read(tx_probs[2]);
		}
		if (xd->counts)
			++xd->counts->get_tx_counts(max_tx_size, ctx)[tx_size];

		return (TX_SIZE)tx_size;
	}

	return (TX_SIZE)min(max_tx_size, tx_mode_to_biggest_tx_size[tx_mode]);
}

int Decoder::read_intra_segment_id(vp9::reader &r, int mi_offset, int x_mis, int y_mis) const {
	if (!seg.enabled)
		return 0;  // Default for disabled segmentation

	if (!seg.update_map) {
		copy_segment_id(seg_map[0], seg_map[1], mi_cols, mi_offset, x_mis, y_mis);
		return 0;
	}
	int	segment_id = read_segment_id(r, &seg);
	set_segment_id(seg_map[1], mi_cols, mi_offset, x_mis, y_mis, segment_id);
	return segment_id;
}

int Decoder::read_inter_segment_id(vp9::reader &r, TileDecoder *const xd, int mi_row, int mi_col) const {
	if (!seg.enabled)
		return 0;  // Default for disabled segmentation

	ModeInfo *const mi = xd->mi[0];
	const int mi_offset = mi_row * mi_cols + mi_col;

	// TODO(slavarnway): move x_mis, y_mis into xd ?????
	const int x_mis = min(mi_cols - mi_col, mi->width());
	const int y_mis = min(mi_rows - mi_row, mi->height());

	int predicted_segment_id = get_segment_id(seg_map[0], mi_cols, mi_offset, x_mis, y_mis);

	if (!seg.update_map) {
		copy_segment_id(seg_map[0], seg_map[1], mi_cols, mi_offset, x_mis, y_mis);
		return predicted_segment_id;
	}

	int segment_id;
	if (seg.temporal_update) {
		const prob pred_prob = seg.pred_probs[xd->get_pred_context_seg_id()];
		mi->seg_id_predicted = r.read(pred_prob);
		segment_id = mi->seg_id_predicted ? predicted_segment_id : read_segment_id(r, &seg);
	} else {
		segment_id = read_segment_id(r, &seg);
	}
	set_segment_id(seg_map[1], mi_cols, mi_offset, x_mis, y_mis, segment_id);
	return segment_id;
}

static bool read_skip(vp9::reader &r, const TileDecoder *xd, const FrameContext &fc) {
	const int	ctx		= xd->get_skip_context();
	const bool	skip	= r.read(fc.skip_probs[ctx]);
	if (xd->counts)
		++xd->counts->skip[ctx][skip];
	return skip;
}

int read_mv_component(vp9::reader &r, const FrameContext::mvs::component &comp, FrameCounts::mvs::component *counts, bool usehp) {
	int			mag;
	const bool	sign		= r.read(comp.sign);
	const int	mv_class	= r.read_tree(mv_class_tree, comp.classes);

	if (counts) {
		++counts->sign[sign];
		++counts->classes[mv_class];
	}

	if (mv_class == MotionVector::CLASS_0) {
		int		d	= r.read_tree(mv_class0_tree, comp.class0);	// Integer part
		int		fp	= r.read_tree(mv_fp_tree, comp.class0_fp[d]);// Fractional part
		bool	hp	= !usehp || r.read(comp.class0_hp);			// High precision part (if hp is not used, the default value of the hp is 1)

		if (counts) {
			++counts->class0[d];
			++counts->class0_fp[d][fp];
			++counts->class0_hp[hp];
		}
		mag = (d << 3) + (fp << 1) + (hp + 1);

	} else {
		// Integer part
		int		d	= 0;
		for (int i = 0, n = mv_class + MotionVector::CLASS0_BITS - 1; i < n; ++i)
			d |= r.read(comp.bits[i]) << i;
		int		fp	= r.read_tree(mv_fp_tree, comp.fp);			// Fractional part
		bool	hp	= !usehp || r.read(comp.hp);					// High precision part (if hp is not used, the default value of the hp is 1)

		if (counts) {
			for (int i = 0, n = mv_class + MotionVector::CLASS0_BITS - 1; i < n; ++i)
				++counts->bits[i][((d >> i) & 1)];
			++counts->fp[fp];
			++counts->hp[hp];
		}
		mag	= (d << 3) + (MotionVector::CLASS0_SIZE << (mv_class + 2)) + (fp << 1) + (hp + 1);
	}

	// Result
	return sign ? -mag : mag;
}

MotionVector read_mv(vp9::reader &r, const FrameContext::mvs &ctx, FrameCounts *counts, bool allow_hp) {
	const MotionVector::JOINT_TYPE joint_type = (MotionVector::JOINT_TYPE)r.read_tree(mv_joint_tree, ctx.joints);
	int		row = has_vertical(joint_type)		? read_mv_component(r, ctx.comps[0], counts ? counts->mv.comps + 0 : 0, allow_hp) : 0;
	int		col = has_horizontal(joint_type)	? read_mv_component(r, ctx.comps[1], counts ? counts->mv.comps + 1 : 0, allow_hp) : 0;
	return MotionVector(row, col);
}

bool read_mvs(vp9::reader &r, const FrameContext::mvs &ctx, FrameCounts *counts, MotionVectorPair &mv, MotionVectorPair &ref, int nrefs, bool allow_hp) {
	return is_valid(mv.mv[0] = ref.mv[0] + read_mv(r, ctx, counts, allow_hp && ref.mv[0].use_hp()))
		&& (nrefs == 1 || is_valid(mv.mv[1] = ref.mv[1] + read_mv(r, ctx, counts, allow_hp && ref.mv[1].use_hp())));
}


// Read the reference frame
void Decoder::read_ref_frames(vp9::reader &r, TileDecoder *const xd, int segment_id, REFERENCE_FRAME ref_frame[2]) const {
	if (seg.active(segment_id, Segmentation::FEATURE_REF_FRAME)) {
		ref_frame[0] = (REFERENCE_FRAME)seg.get_data(segment_id, Segmentation::FEATURE_REF_FRAME);
		ref_frame[1] = REFFRAME_INTRA;//REFFRAME_NONE;

	} else {
		REFERENCE_MODE mode = reference_mode;

		if (mode == REFMODE_SELECT) {
			const int ctx = xd->get_reference_mode_context(fc, comp_ref[0]);
			mode = (REFERENCE_MODE)r.read(fc.comp_inter_prob[ctx]);
			if (xd->counts)
				++xd->counts->comp_inter[ctx][mode];
		}

		// FIXME(rbultje) I'm pretty sure this breaks segmentation ref frame coding
		if (mode == REFMODE_COMPOUND) {
			const int idx = ref_frame_sign_bias[comp_ref[0]];
			const int ctx = xd->get_pred_context_comp_ref_p(fc, comp_ref, !ref_frame_sign_bias[comp_ref[0]]);
			const int bit = r.read(fc.comp_ref_prob[ctx]);
			if (xd->counts)
				++xd->counts->comp_ref[ctx][bit];
			ref_frame[idx] = comp_ref[0];
			ref_frame[!idx] = comp_ref[bit + 1];

		} else {
			const int ctx0 = xd->get_pred_context_single_ref_p1();
			const int bit0 = r.read(fc.single_ref_prob[ctx0][0]);
			if (xd->counts)
				++xd->counts->single_ref[ctx0][0][bit0];
			if (bit0) {
				const int ctx1 = xd->get_pred_context_single_ref_p2();
				const int bit1 = r.read(fc.single_ref_prob[ctx1][1]);
				if (xd->counts)
					++xd->counts->single_ref[ctx1][1][bit1];
				ref_frame[0] = bit1 ? REFFRAME_ALTREF : REFFRAME_GOLDEN;
			} else {
				ref_frame[0] = REFFRAME_LAST;
			}
			ref_frame[1] = REFFRAME_INTRA;//REFFRAME_NONE;
		}
	}
}


struct MotionVectorList {
	MotionVector	list[MotionVectorRef::MAX_CANDIDATES];
	int				count;

	force_inline bool add(MotionVector mv, bool early_break) {
		if (count) {
			if (mv != list[0]) {
				list[count++] = mv;
				return true;
			}
			return false;
		}
		list[count++] = mv;
		return early_break;
	}
	MotionVectorList() : count(0) {
	// Blank the reference vector list
		clear(list);
	}

	const MotionVector	&last() const { return list[count - 1]; }
	int find_mv_refs(ModeInfo **mi, int mi_stride, const TILE_INFO &tile, REFERENCE_FRAME ref_frame, const Position *const offsets, int mi_row, int mi_col, int block, bool early_break, const MotionVectorRef *prev_frame_mvs,	const bool *ref_frame_sign_bias);
};

// This function searches the neighborhood of a given MB/SB to try and find candidate reference vectors.
int MotionVectorList::find_mv_refs(ModeInfo **mi, int mi_stride, const TILE_INFO &tile, REFERENCE_FRAME ref_frame, const Position *const offsets, int mi_row, int mi_col, int block, bool early_break, const MotionVectorRef *prev_frame_mvs, const bool *ref_frame_sign_bias) {
	bool	different_ref_found = false;

	// If the size < 8x8 we get the mv from the bmi sliceucture for the nearest two blocks.
	for (int i = 0; i < MotionVectorRef::NEIGHBOURS; ++i) {
		const Position &off = offsets[i];
		if (tile.is_inside(mi_col, mi_row, off)) {
			const ModeInfo *const candidate = mi[off.col + off.row * mi_stride];
			if (candidate->ref_frame[0] == ref_frame) {
				if (add(block >= 0 && i < 2 ? candidate->get_sub_mv(0, off.col, block) : candidate->sub_mv[3].mv[0], early_break))
					return count;

			} else if (candidate->ref_frame[1] == ref_frame) {
				if (add(block >= 0 && i < 2 ? candidate->get_sub_mv(1, off.col, block) : candidate->sub_mv[3].mv[1], early_break))
					return count;
			}
			different_ref_found = true;
		}
	}

	// Check the last frame's mode and mv info.
	if (prev_frame_mvs) {
		if (prev_frame_mvs->ref_frame[0] == ref_frame) {
			if (add(prev_frame_mvs->mv[0], early_break))
				return count;

		} else if (prev_frame_mvs->ref_frame[1] == ref_frame) {
			if (add(prev_frame_mvs->mv[1], early_break))
				return count;
		}
	}

	// Since we couldn't find 2 mvs from the same reference frame go back through the neighbors and find motion vectors from different reference frames
	if (different_ref_found) {
		for (int i = 0; i < MotionVectorRef::NEIGHBOURS; ++i) {
			const Position &off = offsets[i];
			if (tile.is_inside(mi_col, mi_row, off)) {
				const ModeInfo *const candidate = mi[off.col + off.row * mi_stride];
				// If the candidate is INTRA we don't want to consider its mv.
				if (candidate->is_inter_block()) {
					// If either reference frame is different, not INTRA, and they are different from each other, scale and add the mv to our list
					if (candidate->ref_frame[0] != ref_frame)
						if (add(candidate->scale_mv(0, ref_frame, ref_frame_sign_bias), early_break))
							return count;

					if (candidate->has_second_ref() && candidate->ref_frame[1] != ref_frame && candidate->sub_mv[3].mv[1] != candidate->sub_mv[3].mv[0])
						if (add(candidate->scale_mv(1, ref_frame, ref_frame_sign_bias), early_break))
							return count;
				}
			}
		}
	}

	// Since we still don't have a candidate we'll try the last frame.
	if (prev_frame_mvs) {
		if (prev_frame_mvs->ref_frame[0] != ref_frame && prev_frame_mvs->ref_frame[0] > REFFRAME_INTRA) {
			if (add(flip(prev_frame_mvs->mv[0], ref_frame_sign_bias[prev_frame_mvs->ref_frame[0]] != ref_frame_sign_bias[ref_frame]), early_break))
				return count;
		}

		if (prev_frame_mvs->ref_frame[1] > REFFRAME_INTRA && prev_frame_mvs->ref_frame[1] != ref_frame && prev_frame_mvs->mv[1] != prev_frame_mvs->mv[0]) {
			if (add(flip(prev_frame_mvs->mv[1], ref_frame_sign_bias[prev_frame_mvs->ref_frame[1]] != ref_frame_sign_bias[ref_frame]), early_break))
				return count;
		}
	}

	return early_break ? 1 : MotionVectorRef::MAX_CANDIDATES;
}

MotionVector append_sub8x8_mvs_for_idx(MotionVectorList	&mv, MotionVectorPair *sub_mv, PREDICTION_MODE b_mode, int block, int ref) {
	switch (block) {
		default:
		case 0:
			return mv.last();
		case 1:
		case 2: {
			MotionVector	mv0	= sub_mv[0].mv[ref];
			return	b_mode == NEARESTMV			? mv0
				:	mv.list[0]			!= mv0	? mv.list[0]
				:	mv.list[1]			!= mv0	? mv.list[1]
				:	MotionVector(0, 0);
		}
		case 3: {
			MotionVector	mv2	= sub_mv[2].mv[ref];
			return	b_mode == NEARESTMV			? mv2
				:	sub_mv[1].mv[ref]	!= mv2	? sub_mv[1].mv[ref]
				:	sub_mv[0].mv[ref]	!= mv2	? sub_mv[0].mv[ref]
				:	mv.list[0]			!= mv2	? mv.list[0]
				:	mv.list[1]			!= mv2	? mv.list[1]
				:	MotionVector(0, 0);
		}
	}
}

uint8 get_mode_context(ModeInfo **mi, int mi_stride, const TILE_INFO &tile, const Position *const offsets, int mi_row, int mi_col) {
	enum {
		BOTH_ZERO				= 0,
		ZERO_PLUS_PREDICTED		= 1,
		BOTH_PREDICTED			= 2,
		NEW_PLUS_NON_INTRA		= 3,
		BOTH_NEW				= 4,
		INTRA_PLUS_NON_INTRA	= 5,
		BOTH_INTRA				= 6,
		INVALID_CASE			= 9
	};
	static const uint8 mode_2_counter[] = {
		9,  // DC_PRED
		9,  // V_PRED
		9,  // H_PRED
		9,  // D45_PRED
		9,  // D135_PRED
		9,  // D117_PRED
		9,  // D153_PRED
		9,  // D207_PRED
		9,  // D63_PRED
		9,  // TM_PRED
		0,  // NEARESTMVT
		0,  // NEARMV
		3,  // ZEROMV
		1,  // NEWMV
	};
	// There are 3^3 different combinations of 3 counts that can be either 0,1 or 2
	// However the actual count can never be greater than 2 so the highest counter we need is 18. 9 is an invalid counter that's never used.
	static const uint8 counter_to_context[19] = {
		BOTH_PREDICTED,
		NEW_PLUS_NON_INTRA,
		BOTH_NEW,
		ZERO_PLUS_PREDICTED,
		NEW_PLUS_NON_INTRA,
		INVALID_CASE,
		BOTH_ZERO,
		INVALID_CASE,
		INVALID_CASE,
		INTRA_PLUS_NON_INTRA,
		INTRA_PLUS_NON_INTRA,
		INVALID_CASE,
		INTRA_PLUS_NON_INTRA,
		INVALID_CASE,
		INVALID_CASE,
		INVALID_CASE,
		INVALID_CASE,
		INVALID_CASE,
		BOTH_INTRA
	};

	// Get mode count from nearest 2 blocks
	int context_counter = 0;
	for (int i = 0; i < 2; ++i) {
		if (tile.is_inside(mi_col, mi_row, offsets[i]))
			context_counter += mode_2_counter[mi[offsets[i].col + offsets[i].row * mi_stride]->mode];
	}

	return counter_to_context[context_counter];
}

static PREDICTION_MODE read_intra_mode_y(vp9::reader &r, const FrameContext &fc, FrameCounts *counts, int size_group) {
	const PREDICTION_MODE y_mode = read_intra_mode(r, fc.y_mode_prob[size_group]);
	if (counts)
		++counts->y_mode[size_group][y_mode];
	return y_mode;
}

RETURN read_intra_block_mode_info(vp9::reader &r, const FrameContext &fc, FrameCounts *counts, ModeInfo *mi) {
	mi->ref_frame[0] = REFFRAME_INTRA;
	mi->ref_frame[1] = REFFRAME_INTRA;//REFFRAME_NONE;

	switch (mi->sb_type) {
		case BLOCK_4X4:
			for (int i = 0; i < 4; ++i)
				mi->sub_mode[i] = read_intra_mode_y(r, fc, counts, 0);
			mi->mode = mi->sub_mode[3];
			break;
		case BLOCK_4X8:
			mi->sub_mode[0] = mi->sub_mode[2] = read_intra_mode_y(r, fc, counts, 0);
			mi->sub_mode[1] = mi->sub_mode[3] = mi->mode = read_intra_mode_y(r, fc, counts, 0);
			break;
		case BLOCK_8X4:
			mi->sub_mode[0] = mi->sub_mode[1] = read_intra_mode_y(r, fc, counts, 0);
			mi->sub_mode[2] = mi->sub_mode[3] = mi->mode = read_intra_mode_y(r, fc, counts, 0);
			break;
		default: {
			static const uint8 size_group_lookup[BLOCK_SIZES] = { 0, 0, 0, 1, 1, 1, 2, 2, 2, 3, 3, 3, 3 };
			mi->mode = read_intra_mode_y(r, fc, counts, size_group_lookup[mi->sb_type]);
			break;
		}
	}

	mi->uv_mode = read_intra_mode(r, fc.uv_mode_prob[mi->mode]);
	if (counts)
		++counts->uv_mode[mi->mode][mi->uv_mode];
	return RETURN_OK;
}

void Decoder::read_intra_frame_mode_info(vp9::reader &r, TileDecoder *const xd, int mi_row, int mi_col) const {
	ModeInfo *const		mi			= xd->mi[0];
	const ModeInfo		*above_mi	= xd->above_mi;
	const ModeInfo		*left_mi	= xd->left_mi;
	const int			mi_offset	= mi_row * mi_cols + mi_col;

	// TODO(slavarnway): move x_mis, y_mis into xd ?????
	const int x_mis		= min(mi_cols - mi_col, mi->width());
	const int y_mis		= min(mi_rows - mi_row, mi->height());

	mi->segment_id		= read_intra_segment_id(r, mi_offset, x_mis, y_mis);
	mi->skip			= seg.active(mi->segment_id, Segmentation::FEATURE_SKIP) || read_skip(r, xd, fc);
	mi->tx_size			= read_tx_size(r, xd, fc, tx_mode, true);
	mi->ref_frame[0]	= REFFRAME_INTRA;
	mi->ref_frame[1]	= REFFRAME_INTRA;//REFFRAME_NONE;

	switch (mi->sb_type) {
		case BLOCK_4X4:
			for (int i = 0; i < 4; ++i)
				mi->sub_mode[i] = read_intra_mode(r, mi->get_y_mode_probs(above_mi, left_mi, i));
			mi->mode = mi->sub_mode[3];
			break;
		case BLOCK_4X8:
			mi->sub_mode[0] = mi->sub_mode[2] = read_intra_mode(r, mi->get_y_mode_probs(above_mi, left_mi, 0));
			mi->sub_mode[1] = mi->sub_mode[3] = mi->mode = read_intra_mode(r, mi->get_y_mode_probs(above_mi, left_mi, 1));
			break;
		case BLOCK_8X4:
			mi->sub_mode[0] = mi->sub_mode[1] = read_intra_mode(r, mi->get_y_mode_probs(above_mi, left_mi, 0));
			mi->sub_mode[2] = mi->sub_mode[3] = mi->mode = read_intra_mode(r, mi->get_y_mode_probs(above_mi, left_mi, 2));
			break;
		default:
			mi->mode = read_intra_mode(r, mi->get_y_mode_probs(above_mi, left_mi, 0));
			break;
	}

	mi->uv_mode = read_intra_mode(r, kf_uv_mode_prob[mi->mode]);
}
RETURN Decoder::read_inter_block_mode_info(vp9::reader &r, TileDecoder *const xd, ModeInfo *const mi, int mi_row, int mi_col) const {
	static const Position offsets[BLOCK_SIZES][8] = {
	//1st two cols are closest
		{{-1,  0}, { 0, -1}, {-1, -1}, {-2,  0}, { 0, -2}, {-2, -1}, {-1, -2}, {-2, -2}},	// 4X4
		{{-1,  0}, { 0, -1}, {-1, -1}, {-2,  0}, { 0, -2}, {-2, -1}, {-1, -2}, {-2, -2}},	// 4X8
		{{-1,  0}, { 0, -1}, {-1, -1}, {-2,  0}, { 0, -2}, {-2, -1}, {-1, -2}, {-2, -2}},	// 8X4
		{{-1,  0}, { 0, -1}, {-1, -1}, {-2,  0}, { 0, -2}, {-2, -1}, {-1, -2}, {-2, -2}},	// 8X8
		{{ 0, -1}, {-1,  0}, { 1, -1}, {-1, -1}, { 0, -2}, {-2,  0}, {-2, -1}, {-1, -2}},	// 8X16
		{{-1,  0}, { 0, -1}, {-1,  1}, {-1, -1}, {-2,  0}, { 0, -2}, {-1, -2}, {-2, -1}},	// 16X8
		{{-1,  0}, { 0, -1}, {-1,  1}, { 1, -1}, {-1, -1}, {-3,  0}, { 0, -3}, {-3, -3}},	// 16X16
		{{ 0, -1}, {-1,  0}, { 2, -1}, {-1, -1}, {-1,  1}, { 0, -3}, {-3,  0}, {-3, -3}},	// 16X32
		{{-1,  0}, { 0, -1}, {-1,  2}, {-1, -1}, { 1, -1}, {-3,  0}, { 0, -3}, {-3, -3}},	// 32X16
		{{-1,  1}, { 1, -1}, {-1,  2}, { 2, -1}, {-1, -1}, {-3,  0}, { 0, -3}, {-3, -3}},	// 32X32
		{{ 0, -1}, {-1,  0}, { 4, -1}, {-1,  2}, {-1, -1}, { 0, -3}, {-3,  0}, { 2, -1}},	// 32X64
		{{-1,  0}, { 0, -1}, {-1,  4}, { 2, -1}, {-1, -1}, {-3,  0}, { 0, -3}, {-1,  2}},	// 64X32
		{{-1,  3}, { 3, -1}, {-1,  4}, { 4, -1}, {-1, -1}, {-1,  0}, { 0, -1}, {-1,  6}}, 	// 64X64
	};

	read_ref_frames(r, xd, mi->segment_id, mi->ref_frame);

	const int			xoffset	= mi_col << (MI_SIZE_LOG2 + 3);
	const int			yoffset	= mi_row << (MI_SIZE_LOG2 + 3);
	const int			nrefs	= mi->has_second_ref() ? 2 : 1;
	const BLOCK_SIZE	bsize	= mi->sb_type;
	MotionVectorPair	best_ref_mvs;

	if (seg.active(mi->segment_id, Segmentation::FEATURE_SKIP)) {
		mi->mode = ZEROMV;
		if (bsize < BLOCK_8X8)
			return RETURN_UNSUP_BITSTREAM;

	} else {
		if (bsize >= BLOCK_8X8) {
			uint8	ctx	= get_mode_context(xd->mi, mi_stride, xd->tile, offsets[bsize], mi_row, mi_col);
			mi->mode = read_inter_mode(r, fc, xd->counts, ctx);
		} else {
			// Sub 8x8 blocks use the nearestmv as a ref_mv if the b_mode is NEWMV.
			// Setting mode to NEARESTMV forces the search to stop after the nearestmv has been found.
			// After b_modes have been read, mode will be overwritten by the last b_mode.
			mi->mode = NEARESTMV;
		}

		if (mi->mode != ZEROMV) {
			for (int ref = 0; ref < nrefs; ++ref) {
				MotionVectorList	tmp_mvs;
				int					count = tmp_mvs.find_mv_refs(
					xd->mi, mi_stride, xd->tile, mi->ref_frame[ref], offsets[bsize],
					mi_row, mi_col, -1,  mi->mode != NEARMV,
					use_prev_frame_mvs ? prev_mvs + mi_row * mi_cols + mi_col : NULL,
					ref_frame_sign_bias
				);

				// Make sure all the candidates are properly clamped etc
				for (int i = 0; i < count; ++i) {
					tmp_mvs.list[i].lower_precision(allow_high_precision_mv);
					tmp_mvs.list[i].clamp(
						-MotionVectorRef::BORDER - xoffset, (mi_cols << (MI_SIZE_LOG2 + 3)) + MotionVectorRef::BORDER - xoffset,
						-MotionVectorRef::BORDER - yoffset, (mi_rows << (MI_SIZE_LOG2 + 3)) + MotionVectorRef::BORDER - yoffset
					);
					best_ref_mvs.mv[ref] = tmp_mvs.list[i];
				}
			}
		}
	}

	if (interp_filter == INTERP_SWITCHABLE) {
		const int ctx		= xd->get_pred_context_switchable_interp();
		mi->interp_filter	= (INTERP_FILTER)r.read_tree(switchable_interp_tree, fc.switchable_interp_prob[ctx]);
		if (xd->counts)
			++xd->counts->switchable_interp[ctx][mi->interp_filter];
	} else {
		mi->interp_filter = interp_filter;
	}

	if (bsize < BLOCK_8X8) {
		uint8	inter_mode_ctx	= get_mode_context(xd->mi, mi_stride, xd->tile, offsets[bsize], mi_row, mi_col);
		// calculate bmode block dimensions
		const int num_4x4_w = bsize == BLOCK_8X4 ? 2 : 1;
		const int num_4x4_h = bsize == BLOCK_4X8 ? 2 : 1;

		PREDICTION_MODE		b_mode;
		for (int idy = 0; idy < 2; idy += num_4x4_h) {
			for (int idx = 0; idx < 2; idx += num_4x4_w) {
				const int			block	= idy * 2 + idx;
				MotionVectorPair	&mv		= mi->sub_mv[block];
				b_mode = read_inter_mode(r, fc, xd->counts, inter_mode_ctx);

				switch (b_mode) {
					case NEWMV:
						if (!read_mvs(r, fc.mv, xd->counts, mv, best_ref_mvs, nrefs, allow_high_precision_mv))
							return RETURN_CORRUPT_FRAME;
						break;

					case NEARESTMV:
					case NEARMV:
						for (int ref = 0; ref < nrefs; ++ref) {
							MotionVectorList mv_list;
							int				count = mv_list.find_mv_refs(
								xd->mi, mi_stride, xd->tile, mi->ref_frame[ref], offsets[bsize],
								mi_row, mi_col, block, b_mode != NEARMV,
								use_prev_frame_mvs ? prev_mvs + mi_row * mi_cols + mi_col : NULL,
								ref_frame_sign_bias
							);
							// Clamp vectors
							for (int i = 0; i < count; ++i)
								mv_list.list[i].clamp(
									-MotionVectorRef::BORDER - xoffset, (mi_cols << (MI_SIZE_LOG2 + 3)) + MotionVectorRef::BORDER - xoffset,
									-MotionVectorRef::BORDER - yoffset, (mi_rows << (MI_SIZE_LOG2 + 3)) + MotionVectorRef::BORDER - yoffset
								);
							mv.mv[ref] = append_sub8x8_mvs_for_idx(mv_list, mi->sub_mv, b_mode, block, ref);
						}
						break;

					case ZEROMV:
						mv.clear();
						break;
				}

				if (bsize == BLOCK_4X8)
					mi->sub_mv[block + 2] = mv;
				if (bsize == BLOCK_8X4)
					mi->sub_mv[block + 1] = mv;
			}
		}
		mi->mode	= b_mode;

	} else {
		MotionVectorPair &mv	= mi->sub_mv[3];
		switch (mi->mode) {
			case NEWMV:
				if (!read_mvs(r, fc.mv, xd->counts, mv, best_ref_mvs, nrefs, allow_high_precision_mv))
					return RETURN_CORRUPT_FRAME;
				break;

			case NEARMV:
			case NEARESTMV:
				mv = best_ref_mvs;
				break;

			case ZEROMV:
				mv.clear();
				break;
		}
	}
	return RETURN_OK;
}

RETURN Decoder::read_inter_frame_mode_info(vp9::reader &r, TileDecoder *const xd, int mi_row, int mi_col) const {
	ModeInfo *const mi = xd->mi[0];
	mi->sub_mv[3].clear();
	mi->segment_id		= read_inter_segment_id(r, xd, mi_row, mi_col);
	mi->skip			= seg.active(mi->segment_id, Segmentation::FEATURE_SKIP) || read_skip(r, xd, fc);

	bool	inter_block;
	if (seg.active(mi->segment_id, Segmentation::FEATURE_REF_FRAME)) {
		inter_block		= seg.get_data(mi->segment_id, Segmentation::FEATURE_REF_FRAME) != REFFRAME_INTRA;

	} else {
		const int ctx	= xd->get_intra_inter_context();
		inter_block		= r.read(fc.intra_inter_prob[ctx]);
		if (xd->counts)
			++xd->counts->intra_inter[ctx][inter_block];
	}

	mi->tx_size	= read_tx_size(r, xd, fc, tx_mode, !mi->skip || !inter_block);

	return inter_block
		? read_inter_block_mode_info(r, xd, mi, mi_row, mi_col)
		: read_intra_block_mode_info(r, fc, xd->counts, mi);
}

RETURN Decoder::decode_block(vp9::reader &r, TileDecoder *const xd, int mi_row, int mi_col, BLOCK_SIZE bsize, int bwl, int bhl) const {
	const int bw		= 1 << (bwl - 1);
	const int bh		= 1 << (bhl - 1);
	const int x_mis		= min(bw, mi_cols - mi_col);
	const int y_mis		= min(bh, mi_rows - mi_row);
	const int offset	= (mi_row + 1) * mi_stride + mi_col + 1;
	ModeInfo *mi		= mi_array + offset;

	mi->sb_type			= bsize;
	for (int y = 0; y < y_mis; ++y)
		for (int x = 0; x < x_mis; ++x)
			mi_grid[offset + y * mi_stride + x] = mi;

	xd->set_mi_info(mi_grid + offset, mi_stride, mi_row, mi_col, bwl, bhl);

	if (frame_is_intra_only()) {
		read_intra_frame_mode_info(r, xd, mi_row, mi_col);

	} else {
		if (RETURN ret = read_inter_frame_mode_info(r, xd, mi_row, mi_col))
			return ret;

		for (int h = 0; h < y_mis; ++h) {
			MotionVectorRef *const mv = mvs + (mi_row + h) * mi_cols + mi_col;
			for (int w = 0; w < x_mis; ++w) {
				mv[w].ref_frame[0]	= mi->ref_frame[0];
				mv[w].ref_frame[1]	= mi->ref_frame[1];
				mv[w].mv[0]			= mi->sub_mv[3].mv[0];
				mv[w].mv[1]			= mi->sub_mv[3].mv[1];
			}
		}
	}

	if (mi->skip)
		xd->reset_skip_context(bwl, bhl);

	if (!mi->is_inter_block()) {
		for (int p = 0; p < 3; ++p) {
			const TileDecoder::Plane *const pd = &xd->plane[p];
			const TX_SIZE	tx_size			= p ? pd->get_uv_tx_size(mi) : mi->tx_size;
			const int		step			= 1 << tx_size;
			const int		max_blocks_wide	= min((mi_cols - mi_col) << (1 - pd->subsampling_x), 1 << (bwl - pd->subsampling_x));
			const int		max_blocks_high	= min((mi_rows - mi_row) << (1 - pd->subsampling_y), 1 << (bhl - pd->subsampling_y));

			const int		frame_width		= cur_frame->plane(p).crop_width;
			const int		frame_height	= cur_frame->plane(p).crop_height;
			const int		x0				= mi_col << (MI_SIZE_LOG2 - pd->subsampling_x);
			const int		y0				= mi_row << (MI_SIZE_LOG2 - pd->subsampling_y);
			const Buffer2D	&dst_buffer		= cur_frame->buffer(p);
			const int		stride			= dst_buffer.stride;
			uint8			*dst0			= dst_buffer.buffer + y0 * stride + x0;

			for (int row = 0; row < max_blocks_high; row += step)
				for (int col = 0; col < max_blocks_wide; col += step) {
					PREDICTION_MODE		mode	= p == 0 ? (bsize < BLOCK_8X8 ? mi->sub_mode[(row << 1) + col] : mi->mode) : mi->uv_mode;
					uint8				*dst	= dst0 + 4 * row * stride + 4 * col;

					if (gpu) {
						gpu->OutputIntraPred(tx_size, mode, x0 + 4 * col, y0 + 4 * row, p,
							row || xd->up_available, col || xd->left_available, tx_size == TX_4X4 && (col + step) < max_blocks_wide,
							mi_cols, mi_rows,
							xd->above_dependency[p], xd->left_dependency[p]
						);
					#if DEBUG_GPU
						predict_intra_block(tx_size, mode, dst_buffer, dst_buffer,
							x0 + 4 * col, y0 + 4 * row,
							row || xd->up_available, col || xd->left_available, tx_size == TX_4X4 && (col + step) < max_blocks_wide,
							frame_width, frame_height
						);
					#endif
					} else {
						predict_intra_block(tx_size, mode, dst_buffer, dst_buffer,
							x0 + 4 * col, y0 + 4 * row,
							row || xd->up_available, col || xd->left_available, tx_size == TX_4X4 && (col + step) < max_blocks_wide,
							frame_width, frame_height
						);
					}

					if (!mi->skip) {
						const TX_TYPE	tx_type = (p || lossless) ? DCT_DCT : (TX_TYPE)intra_mode_to_tx_type_lookup[mode];
						const int		eob		= xd->decode_block_tokens(r, p, pd->get_entropy_context(tx_size, col, row), tx_size, tx_type, mi->segment_id, fc, gpu);
						//xd->set_contexts(pd, tx_size, eob > 0, col, row, max_blocks_wide - col, max_blocks_high - row);
						pd->set_entropy_context(tx_size, col, row, eob > 0);

						if (eob) {
							if (gpu) {
								gpu->OutputDCT(xd->seg_dequant[p > 0][mi->segment_id], eob, xd->dqcoeff, tx_type, tx_size, lossless, x0 + 4 * col + (p == 2 ? mi_cols * 4 : 0), y0 + 4 * row + (p != 0 ? mi_rows * 8 : 0));
							#if DEBUG_GPU
								tran_low_t		temp[32 * 32];
								dequant_zigzag(temp, xd->dqcoeff, xd->seg_dequant[p > 0][mi->segment_id], tx_type, tx_size, eob);
								inverse_transform_block_intra(temp, tx_type, tx_size, dst, stride, eob, lossless);
							#endif
								xd->dqcoeff += eob;
							} else {
								inverse_transform_block_intra(xd->dqcoeff, tx_type, tx_size, dst, stride, eob, lossless);
								clear_used_coeffs(xd->dqcoeff, tx_type, tx_size, eob);
							}
						}
					}
				}
		}
	} else {
		// Prediction
		if (gpu) {
			gpu->OutputInterPred(mi, mi_col, mi_row, mi_cols);
		#if !DEBUG_GPU
		} else {
		#endif
			const ScaleFactors::kernel	*kernel = filter_kernels[mi->interp_filter];

			for (int ref = 0, n = mi->has_second_ref() ? 2 : 1; ref < n; ++ref) {
				const REFERENCE_FRAME	frame		= mi->ref_frame[ref];
				const FrameBufferRef	*ref_buf	= &frame_refs[frame - REFFRAME_LAST];
				const ScaleFactors		&sf			= ref_buf->sf;

				if (!sf.is_valid())
					return RETURN_UNSUP_BITSTREAM;

				if (bsize < BLOCK_8X8) {
					for (int p = 0; p < 3; ++p) {
						TileDecoder::Plane			*const pd	= &xd->plane[p];
						const FrameBuffer::Plane	&ref_plane	= ref_buf->buf->plane(p);
						const Buffer2D				&ref_buffer	= ref_buf->buf->buffer(p);
						const Buffer2D				&dst_buffer	= cur_frame->buffer(p);

						const int	w	= 4 << (bwl - pd->subsampling_x);
						const int	h	= 4 << (bhl - pd->subsampling_y);
						const int	ss	= pd->subsampling_x + pd->subsampling_y * 2;
						const int	xoffset	= mi_col << (MI_SIZE_LOG2 - pd->subsampling_y);
						const int	yoffset	= mi_row << (MI_SIZE_LOG2 - pd->subsampling_x);

						for (int y = 0, i = 0; y < h; y += 4) {
							for (int x = 0; x < w; x += 4) {
								build_inter_predictors(
									xoffset, yoffset, x, y, 4, 4,
									kernel, sf,
									shift(mi->average_mvs(ref, i++, ss), 1 - pd->subsampling_x, 1 - pd->subsampling_y),
									ref_plane.crop_width, ref_plane.crop_height, ref_buffer.buffer, ref_buffer.stride, dst_buffer.buffer, dst_buffer.stride,
									ref
								);
							}
						}
					}
				} else {
					const MotionVector mv = mi->sub_mv[3].mv[ref];
					for (int p = 0; p < 3; ++p) {
						TileDecoder::Plane			*const pd	= &xd->plane[p];
						const FrameBuffer::Plane	&ref_plane	= ref_buf->buf->plane(p);
						const Buffer2D				&ref_buffer	= ref_buf->buf->buffer(p);
						const Buffer2D				&dst_buffer	= cur_frame->buffer(p);

						build_inter_predictors(
							mi_col << (MI_SIZE_LOG2 - pd->subsampling_y),
							mi_row << (MI_SIZE_LOG2 - pd->subsampling_x),
							0, 0, 4 << (bwl - pd->subsampling_x), 4 << (bhl - pd->subsampling_y),
							kernel, sf,
							shift(mv, 1 - pd->subsampling_x, 1 - pd->subsampling_y),
							ref_plane.crop_width, ref_plane.crop_height, ref_buffer.buffer, ref_buffer.stride, dst_buffer.buffer, dst_buffer.stride,
							ref
						);
					}
				}
			}
		}

		// Reconstruction
		if (!mi->skip) {
			int eobtotal = 0;

			for (int p = 0; p < 3; ++p) {
				const TileDecoder::Plane *const pd = &xd->plane[p];
				const TX_SIZE	tx_size			= p ? pd->get_uv_tx_size(mi) : mi->tx_size;
				const int		step			= 1 << tx_size;
				const int		max_blocks_wide	= min((mi_cols - mi_col) << (1 - pd->subsampling_x), 1 << (bwl - pd->subsampling_x));
				const int		max_blocks_high	= min((mi_rows - mi_row) << (1 - pd->subsampling_y), 1 << (bhl - pd->subsampling_y));
				const int		x0				= mi_col << (MI_SIZE_LOG2 - pd->subsampling_x);
				const int		y0				= mi_row << (MI_SIZE_LOG2 - pd->subsampling_y);
				const Buffer2D	&dst_buffer		= cur_frame->buffer(p);
				const int		stride			= dst_buffer.stride;
				uint8			*dst0			= dst_buffer.buffer + y0 * stride + x0;

				for (int row = 0; row < max_blocks_high; row += step)
					for (int col = 0; col < max_blocks_wide; col += step) {
						const int	eob		= xd->decode_block_tokens(r, p, pd->get_entropy_context(tx_size, col, row), tx_size, DCT_DCT, mi->segment_id, fc, gpu);
//						xd->set_contexts(pd, tx_size, eob > 0, col, row, max_blocks_wide - col, max_blocks_high - row);
						pd->set_entropy_context(tx_size, col, row, eob > 0);
						if (eob) {
							if (gpu) {
								gpu->OutputDCT(xd->seg_dequant[p > 0][mi->segment_id], eob, xd->dqcoeff, DCT_DCT, tx_size, lossless, x0 + 4 * col + (p == 2 ? mi_cols * 4 : 0), y0 + 4 * row + (p != 0 ? mi_rows * 8 : 0));
							#if DEBUG_GPU
								tran_low_t		temp[32 * 32];
								dequant_zigzag(temp, xd->dqcoeff, xd->seg_dequant[p > 0][mi->segment_id], DCT_DCT, tx_size, eob);
								inverse_transform_block_inter(temp, tx_size, dst0 + (4 * row) * stride + 4 * col, stride, eob, lossless);
							#endif
								xd->dqcoeff += eob;
							} else {
								inverse_transform_block_inter(xd->dqcoeff, tx_size, dst0 + (4 * row) * stride + 4 * col, stride, eob, lossless);
								clear_used_coeffs(xd->dqcoeff, DCT_DCT, tx_size, eob);
							}
							eobtotal	+= eob;
						}
					}
			}

			if (bsize >= BLOCK_8X8 && eobtotal == 0)
				mi->skip = true;  // skip LoopFilter
		}
	}

	if (lf.filter_level)
		get_masks(mi_row, mi_col)->build_mask(lf_info.get_filter_level(mi), mi, mi_row & MI_MASK, mi_col & MI_MASK, bw, bh);

	return RETURN_OK;
}

RETURN Decoder::decode_partition(vp9::reader &r, TileDecoder *const xd, int mi_row, int mi_col, TX_SIZE n4x4_l2) {
	if (mi_row >= mi_rows || mi_col >= mi_cols)
		return RETURN_OK;

	const TX_SIZE	n8x8_l2		= (TX_SIZE)(n4x4_l2 - 1);
	const int		num_8x8_wh	= 1 << n8x8_l2;
	const int		hbs			= num_8x8_wh >> 1;

	const bool		has_rows	= (mi_row + hbs) < mi_rows;
	const bool		has_cols	= (mi_col + hbs) < mi_cols;

	const int ctx				= xd->get_partition_plane_context(mi_row, mi_col, n8x8_l2);
	const prob *const probs		= partition_probs[ctx];
	PARTITION_TYPE	partition	= has_rows && has_cols ? (PARTITION_TYPE)r.read_tree(partition_tree, probs)
		: has_cols	? r.read(probs[1]) ? PARTITION_SPLIT : PARTITION_HORZ
		: has_rows	? r.read(probs[2]) ? PARTITION_SPLIT : PARTITION_VERT
		: PARTITION_SPLIT;

	if (xd->counts)
		++xd->counts->partition[ctx][partition];

	BLOCK_SIZE		subsize		= get_subsize(n4x4_l2, partition);
	RETURN			ret;
	if (n4x4_l2 == TX_8X8) {
		ret = decode_block(r, xd, mi_row, mi_col, subsize, n4x4_l2, n4x4_l2);

	} else {
		switch (partition) {
			case PARTITION_NONE:
				ret = decode_block(r, xd, mi_row, mi_col, subsize, n4x4_l2, n4x4_l2);
				break;

			case PARTITION_HORZ:
				if (ret = decode_block(r, xd, mi_row, mi_col, subsize, n4x4_l2, n8x8_l2))
					break;
				ret = decode_block(r, xd, mi_row + hbs, mi_col, subsize, n4x4_l2, n8x8_l2);
				break;

			case PARTITION_VERT:
				if (ret = decode_block(r, xd, mi_row, mi_col, subsize, n8x8_l2, n4x4_l2))
					break;
				ret = decode_block(r, xd, mi_row, mi_col + hbs, subsize, n8x8_l2, n4x4_l2);
				break;

			case PARTITION_SPLIT:
				if (ret = decode_partition(r, xd, mi_row, mi_col, n8x8_l2))
					return ret;
				if (ret = decode_partition(r, xd, mi_row, mi_col + hbs, n8x8_l2))
					return ret;
				if (ret = decode_partition(r, xd, mi_row + hbs, mi_col, n8x8_l2))
					return ret;
				return decode_partition(r, xd, mi_row + hbs, mi_col + hbs, n8x8_l2);
		}
	}

	// update partition context
	xd->update_partition_context(mi_row, mi_col, subsize, num_8x8_wh);

	return ret;
}

#define THREAD_1

ptrdiff_t Decoder::decode_subframe(const uint8 *data, const uint8 *data_end) {
	const uint8 *start_data		= data;
	FRAME_TYPE	last_frame_type = frame_type;

	cur_frame	= get_free_fb();
	ISO_ASSERT(cur_frame);

	size_t	first_partition_size;
	{
		memory_reader	m(const_memory_block(data, data_end - data));
		bit_reader		r(m);
		first_partition_size = read_uncompressed_header(r);
		r.restore_unused();
		data	+= m.tell();
	}

	if (first_partition_size == 0) {
		// showing a frame directly
		return profile <= PROFILE_2 ? 1 : 2;
	}

	if (data + first_partition_size > data_end)
		return RETURN_CORRUPT_FRAME;

	if (!fc.initialized)
		return RETURN_CORRUPT_FRAME;

	{
		vp9::reader	r(const_memory_block(data, first_partition_size));
		if (r.read_bit())
			return RETURN_CORRUPT_FRAME;

		read_compressed_header(r);
	}
	data	+= first_partition_size;

	if (lf.filter_level)
		loop_filter_frame_init();

	const int aligned_cols	= align_pow2(mi_cols, MI_BLOCK_SIZE_LOG2);
	const int tile_cols		= 1 << log2_tile_cols;
	const int tile_rows		= 1 << log2_tile_rows;

	// Note: these memsets assume above_context[0], [1] and [2] are allocated as part of the same buffer.
	memset(above_context, 0, sizeof(ENTROPY_CONTEXT) * 3 * 2 * aligned_cols * tile_rows);
	memset(above_seg_context, 0, sizeof(PARTITION_CONTEXT) * aligned_cols * tile_rows);
	memset(above_dependency, 0, sizeof(int) * 3 * 2 * aligned_cols * tile_rows);

	partition_probs		= fc.get_partition_probs(frame_is_intra_only());

	mvs.resize(mi_rows * mi_cols);

	if (gpu)
		gpu->BeginFrame(this);

	tile_data.resize(tile_cols * tile_rows);
	for (int tile_row = 0; tile_row < tile_rows; ++tile_row) {
		for (int tile_col = 0; tile_col < tile_cols; ++tile_col) {
			TileWorkerData	&worker_data = tile_data[tile_cols * tile_row + tile_col];

			const uint8	*next;
			if (tile_row == tile_rows - 1 && tile_col == tile_cols - 1) {
				next	= data_end;
			} else {
				if (data_end - data < 4)
					return RETURN_CORRUPT_FRAME;
				uint32	size = *(uint32be*)data;
				data	+= 4;
				if (size > data_end - data)
					return RETURN_CORRUPT_FRAME;
				next	= data + size;
			}
			if (int ret = worker_data.init(this, data, next, tile_row, tile_col))
				return ret;

			data	= next;
		}
	}

	if (threads) {
		struct threading {
			CriticalSection				cs;
			ConditionVariable			cv;
			atomic<int>					pending_jobs;
			dynamic_array<atomic<int> >	pending_rows;
			int							error_code;
			threading(int jobs, size_t rows, int row_value) : pending_jobs(jobs), pending_rows(rows, row_value), error_code(0) {
				pending_rows.back() /= 2;
				cs.lock();
			}
			void	done() {
				if (--pending_jobs == 0)
					cv.unlock();
			}
			void	wait() {
				cv.lock(cs);
			}
			void	error(int code) {
				error_code = code;
			}
		};

		threading			th(tile_cols * tile_rows, round_pow2(mi_rows, MI_BLOCK_SIZE_LOG2), tile_cols * 2);
		ConcurrentJobs		&jobs	= ConcurrentJobs::Get();

	#ifdef THREAD_1
		struct process_row {
			atomic<int>		refs;
			Decoder			*dec;
			threading		&th;
			TileWorkerData	&data;
			int				row;
			process_row(Decoder *_dec, threading &_th, TileWorkerData &_data) : refs(1), dec(_dec), th(_th), data(_data) {
				ISO_ASSERT(intptr_t(&data) > 0x10000);
				row = data.mb.tile.mi_row_start;
			}

			void	operator()() {
				ISO_ASSERT(refs);
				int	mi_row	= row;
				if (int ret = data.process1row(dec, mi_row))
					th.error(ret);

				row += MI_BLOCK_SIZE;
				if (row < data.mb.tile.mi_row_end) {
					++refs;
					ConcurrentJobs::Get().add(this);
				}

				if (dec->lf.filter_level) {
					int	i = mi_row >> MI_BLOCK_SIZE_LOG2;
					if (i > 0 && --th.pending_rows[i - 1] == 0)
						dec->loop_filter_rows(mi_row - MI_BLOCK_SIZE, mi_row, false);
					if (--th.pending_rows[i] == 0)
						dec->loop_filter_rows(mi_row, min(mi_row + MI_BLOCK_SIZE, dec->mi_rows), false);
				}

				if (--refs == 0) {
					th.done();
					delete this;
				}
			}
		};

		for (int i = 0, n = tile_data.size32(); i < n; ++i)
			jobs.add(new process_row(this, th, tile_data[i]));
	#else
		for (int i = 0, n = tile_data.size(); i < n; ++i) {
			jobs.add([this, i, &th]() {
				TileWorkerData	&data = tile_data[i];
				if (int ret = data.process(this))
					return ret;
				th.done();
			});
		}
		if (lf.filter_level) {
			th.wait();
			th.pending_jobs	= mi_rows / MI_BLOCK_SIZE;
			for (int mi_row = 0; mi_row < mi_rows; mi_row += MI_BLOCK_SIZE) {
				jobs.add([this, mi_row, &th]() {
					loop_filter_rows(mi_row, mi_row + MI_BLOCK_SIZE, false);
					th.done();
				});
			}
		}
	#endif

		th.wait();

	} else {
		for (int tile_row = 0; tile_row < tile_rows; ++tile_row) {
			int	mi_row_start	= get_tile_offset(tile_row, mi_rows, log2_tile_rows);
			int	mi_row_end		= get_tile_offset(tile_row + 1, mi_rows, log2_tile_rows);

			for (int mi_row = mi_row_start; mi_row < mi_row_end; mi_row += MI_BLOCK_SIZE) {
				for (int tile_col = 0; tile_col < tile_cols; ++tile_col) {
					TileWorkerData	&worker_data = tile_data[tile_cols * tile_row + tile_col];
					if (int ret = worker_data.process1row(this, mi_row))
						return ret;
				}
				// Loopfilter one row.
				if (lf.filter_level && mi_row > 0 && mi_row + MI_BLOCK_SIZE < mi_rows)
					loop_filter_rows(mi_row - MI_BLOCK_SIZE, mi_row, false);
			}
		}
		// Loopfilter remaining rows in the frame.
		if (lf.filter_level)
			loop_filter_rows(mi_rows - MI_BLOCK_SIZE, mi_rows, false);
	}

	if (gpu)
		gpu->EndFrame(this);

	if (!error_resilient_mode && !frame_parallel_decoding_mode) {
		uint32 count_sat, update_factor;
		if (frame_is_intra_only()) {
			update_factor	= FrameContext::COEF_MAX_UPDATE_FACTOR_KEY;
			count_sat		= FrameContext::COEF_COUNT_SAT_KEY;
		} else if (last_frame_type == KEY_FRAME) {
			update_factor	= FrameContext::COEF_MAX_UPDATE_FACTOR_AFTER_KEY;  // adapt quickly
			count_sat		= FrameContext::COEF_COUNT_SAT_AFTER_KEY;
		} else {
			update_factor	= FrameContext::COEF_MAX_UPDATE_FACTOR;
			count_sat		= FrameContext::COEF_COUNT_SAT;
		}
		fc.adapt_coef_probs(*pre_fc, counts, update_factor, count_sat);

		if (!frame_is_intra_only()) {
			fc.adapt_mode_probs(*pre_fc, counts, interp_filter == INTERP_SWITCHABLE, tx_mode == TX_MODE_SELECT);
			fc.adapt_mv_probs(*pre_fc, counts, allow_high_precision_mv);
		}
	}

	// Non frame parallel update frame context here.
	if (refresh_frame_context)
		*pre_fc = fc;

	// Generate next_ref_frame_map.
	for (int i = 0, m = refresh_frame_flags; m; i++, m >>= 1) {
		if (m & 1)
			ref_frame_map[i] = cur_frame;
	}

	swap(mvs, prev_mvs);
	swap(seg_map[0], seg_map[1]);

	return tile_data[tile_cols * tile_rows - 1].end() - start_data;
}

ptrdiff_t Decoder::decode_frame(const uint8 *data, const uint8 *data_end) {
	if (!data || data == data_end) {
		cur_frame = 0;
		return 0;
	}

	// A chunk ending with a byte matching 0xc0 is an invalid chunk unless it is a super frame index. If the last byte of real video compression
	// data is 0xc0 the encoder must add a 0 byte. If we have the marker but not the associated matching marker byte at the front of the index we have
	// an invalid bitstream and need to return an error.
	uint8	marker	= data_end[-1];

	if ((marker & 0xe0) != 0xc0)
		return decode_subframe(data, data_end);

	const uint32	frames	= (marker & 0x7) + 1;
	const uint32	mag		= ((marker >> 3) & 0x3) + 1;
	const size_t	index_sz = 2 + mag * frames;
	const uint8		*x		= data_end - index_sz;

	// This chunk is marked as having a superframe index but doesn't have enough data for it, thus it's an invalid superframe index.
	// This chunk is marked as having a superframe index but doesn't have the matching marker byte at the front of the index therefore it's an invalid chunk.
	if (x < data || marker != *x++)
		return RETURN_CORRUPT_FRAME;

	// Frames has a maximum of 8 and mag has a maximum of 4.
	uint32	sizes[8];
	for (int i = 0; i < frames; ++i) {
		uint32	this_sz = 0;
		for (int j = 0; j < mag; ++j)
			this_sz |= *x++ << (j * 8);
		sizes[i] = this_sz;
	}

	const uint8 *start_data		= data;
	for (int i = 0; i < frames; ++i) {
		const uint8	*data_end2 = data + sizes[i];
		int ret = decode_subframe(data, data_end2);
		if (ret < 0)
			return ret;
		data = data_end2;
	}

	return data - start_data;
}


FrameBuffer *Decoder::get_frame() const {
	return show_frame ? cur_frame : nullptr;
}

//-----------------------------------------------------------------------------
//	uncompressed header
//-----------------------------------------------------------------------------

#define VP9_SYNC_CODE_0		0x49
#define VP9_SYNC_CODE_1		0x83
#define VP9_SYNC_CODE_2		0x42
#define VP9_FRAME_MARKER	0x2

bool read_sync_code(bit_reader &r) {
	return	r.get(8) == VP9_SYNC_CODE_0
		&&	r.get(8) == VP9_SYNC_CODE_1
		&&	r.get(8) == VP9_SYNC_CODE_2;
}

force_inline int read_signed_literal(bit_reader &r, int bits) {
	const int value = r.get(bits);
	return r.get_bit() ? -value : value;
}

force_inline int read_unsigned_max(bit_reader &r, int max) {
	return min(r.get(log2_ceil(max)), max);
}

BITSTREAM_PROFILE read_profile(bit_reader &r) {
	int profile		= r.get_bit();
	profile			|= r.get_bit() << 1;
	if (profile > 2)
		profile += r.get_bit();
	return (BITSTREAM_PROFILE)profile;
}

INTERP_FILTER read_interp_filter(bit_reader &r) {
	static const INTERP_FILTER literal_to_filter[] = {INTERP_8TAP_SMOOTH, INTERP_8TAP, INTERP_8TAP_SHARP, INTERP_BILINEAR };
	return r.get_bit() ? INTERP_SWITCHABLE : literal_to_filter[r.get(2)];
}

int read_colorspace(bit_reader &r, ColorSpace &cs, BITSTREAM_PROFILE profile) {
	if (profile >= PROFILE_2)
		cs.bit_depth = r.get_bit() ? 12 : 10;
	else
		cs.bit_depth = 8;

	cs.space = (ColorSpace::Space)r.get(3);

	if (cs.space != ColorSpace::SRGB) {
		// [16,235] (including xvycc) vs [0,255] range
		cs.range = (ColorSpace::Range)r.get_bit();
		if (profile == PROFILE_1 || profile == PROFILE_3) {
			cs.subsampling_x = r.get_bit();
			cs.subsampling_y = r.get_bit();
			if (cs.subsampling_x == 1 && cs.subsampling_y == 1)
				return RETURN_UNSUP_BITSTREAM;
			if (r.get_bit())
				return RETURN_UNSUP_BITSTREAM;
		} else {
			cs.subsampling_y = cs.subsampling_x = 1;
		}
	} else {
		if (profile == PROFILE_1 || profile == PROFILE_3) {
			// Note if colorspace is SRGB then 4:4:4 chroma sampling is assumed.
			// 4:2:2 or 4:4:0 chroma sampling is not allowed.
			cs.subsampling_y = cs.subsampling_x = 0;
			if (r.get_bit())
				return RETURN_UNSUP_BITSTREAM;
		} else {
			return RETURN_UNSUP_BITSTREAM;
		}
	}
	return 0;
}

void read_frame_size(bit_reader &r, int *width, int *height) {
	*width	= r.get(16) + 1;
	*height = r.get(16) + 1;
}

void read_segmentation(bit_reader &r, Segmentation *seg) {
	seg->update_map		= false;
	seg->update_data	= false;
	seg->enabled		= r.get_bit();

	if (seg->enabled) {
		// Segmentation map update
		if (seg->update_map = r.get_bit()) {
			for (int i = 0; i < num_elements(seg->tree_probs); i++)
				seg->tree_probs[i] = r.get_bit() ? r.get(8) : 255;

			if (seg->temporal_update = r.get_bit()) {
				for (int i = 0; i < PREDICTION_PROBS; i++)
					seg->pred_probs[i] = r.get_bit() ? r.get(8) : 255;
			} else {
				for (int i = 0; i < PREDICTION_PROBS; i++)
					seg->pred_probs[i] = 255;
			}
		}

		// Segmentation data update
		if (seg->update_data = r.get_bit()) {
			seg->abs_delta = r.get_bit();
			seg->clearall();

			for (int i = 0; i < MAX_SEGMENTS; i++) {
				for (int j = 0; j < Segmentation::FEATURES; j++) {
					Segmentation::FEATURE	f		= Segmentation::FEATURE(j);
					int						data	= 0;
					if (r.get_bit()) {
						seg->enable(i, f);
						data = read_unsigned_max(r, Segmentation::data_max(f));
						if (Segmentation::is_signed(f))
							data = r.get_bit() ? -data : data;
					}
					seg->set_data(i, f, data);
				}
			}
		}
	}
}

void read_loopfilter(bit_reader &r, LoopFilter *lf) {
	lf->filter_level	= r.get(6);
	lf->sharpness_level = r.get(3);

	// Read in loop filter deltas applied at the MB level based on mode or ref frame.
	lf->mode_ref_delta_update	= false;

	if (lf->mode_ref_delta_enabled	= r.get_bit()) {
		if (lf->mode_ref_delta_update = r.get_bit()) {
			for (int i = 0; i < LoopFilter::MAX_REF_LF_DELTAS; i++)
				if (r.get_bit())
					lf->ref_deltas[i] = read_signed_literal(r, 6);

			for (int i = 0; i < LoopFilter::MAX_MODE_LF_DELTAS; i++)
				if (r.get_bit())
					lf->mode_deltas[i] = read_signed_literal(r, 6);
		}
	}
}

force_inline int read_delta_q(bit_reader &r) {
	return r.get_bit() ? read_signed_literal(r, 4) : 0;
}

void read_quantization(bit_reader &r, Quantisation *q) {
	q->base_index	= r.get(QINDEX_BITS);
	q->y_dc_delta	= read_delta_q(r);
	q->uv_dc_delta	= read_delta_q(r);
	q->uv_ac_delta	= read_delta_q(r);
}

ptrdiff_t Decoder::read_uncompressed_header(bit_reader &r) {
	FRAME_TYPE	last_frame_type = frame_type;
	bool		last_intra_only = intra_only;
	bool		last_show_frame	= show_frame;
	int			last_width		= width;
	int			last_height		= height;

	if (r.get(2) != VP9_FRAME_MARKER)
		return RETURN_UNSUP_BITSTREAM;

	profile = read_profile(r);
	if (profile >= PROFILE_2)
		return RETURN_UNSUP_BITSTREAM;

	bool	show_existing_frame = r.get_bit();

	if (show_existing_frame) {
		// Show an existing frame directly.
		cur_frame = ref_frame_map[r.get(3)];
		if (!cur_frame)
			return RETURN_UNSUP_BITSTREAM;

		refresh_frame_flags = 0;
		lf.filter_level		= 0;
		show_frame			= true;
		return 0;
	}

	frame_type				= (FRAME_TYPE)r.get_bit();
	show_frame				= r.get_bit();
	error_resilient_mode	= r.get_bit();

	RESET	reset_frame_context;

#if DEBUG_GPU
	bool	use_gpu_fb = false;
#else
	bool	use_gpu_fb = !!gpu;
#endif


	if (frame_type == KEY_FRAME) {
		if (!read_sync_code(r))
			return RETURN_UNSUP_BITSTREAM;

		read_colorspace(r, cs, profile);
		refresh_frame_flags = (1 << REFERENCE_FRAMES) - 1;
		for (int i = 0; i < REFS_PER_FRAME; ++i)
			frame_refs[i].buf = NULL;

		read_frame_size(r, &width, &height);
		if (cur_frame->resize(width, height, cs, DEC_BORDER, buffer_alignment, stride_alignment, use_gpu_fb))
			return RETURN_MEM_ERROR;

		if (r.get_bit())
			read_frame_size(r, &cur_frame->render_width, &cur_frame->render_height);

		if (need_resync) {
			reset_frame_map();
			need_resync = false;
		}
		reset_frame_context = RESET_ALL;

	} else {
		intra_only			= !show_frame && r.get_bit();
		reset_frame_context = error_resilient_mode ? RESET_ALL : (RESET)r.get(2);

		if (intra_only) {
			if (!read_sync_code(r))
				return RETURN_UNSUP_BITSTREAM;

			if (profile > PROFILE_0)
				read_colorspace(r, cs, profile);
			else
				cs.init();
			refresh_frame_flags = r.get(REFERENCE_FRAMES);

			read_frame_size(r, &width, &height);
			if (cur_frame->resize(width, height, cs, DEC_BORDER, buffer_alignment, stride_alignment, use_gpu_fb))
				return RETURN_MEM_ERROR;

			if (r.get_bit())
				read_frame_size(r, &cur_frame->render_width, &cur_frame->render_height);

			if (need_resync) {
				reset_frame_map();
				need_resync = false;
			}

		} else if (!need_resync) {
			refresh_frame_flags = r.get(REFERENCE_FRAMES);

			for (int i = 0; i < REFS_PER_FRAME; ++i) {
				FrameBuffer *ref	= ref_frame_map[r.get(3)];
				if (!ref)
					return RETURN_UNSUP_BITSTREAM;

				frame_refs[i].buf	= ref;
				ref_frame_sign_bias[REFFRAME_LAST + i]	= r.get_bit();
			}

			bool	found	= false;
			for (int i = 0; i < REFS_PER_FRAME; ++i) {
				if (r.get_bit()) {
					width	= frame_refs[i].buf->y.crop_width;
					height	= frame_refs[i].buf->y.crop_height;
					found	= true;
					break;
				}
			}

			if (!found)
				read_frame_size(r, &width, &height);

			// Check to make sure at least one of frames that this frame references is valid
			bool	has_valid_ref_frame = false;
			for (int i = 0; i < REFS_PER_FRAME; ++i) {
				FrameBufferRef &ref = frame_refs[i];
				if (!ref.valid_fmt(cs))
					return RETURN_CORRUPT_FRAME;
				has_valid_ref_frame |= ref.valid_size(width, height);
				ref.sf.init(ref.buf->y.crop_width, ref.buf->y.crop_height, width, height);
			}

			if (!has_valid_ref_frame)
				return RETURN_CORRUPT_FRAME;

			if (cur_frame->resize(width, height, cs, DEC_BORDER, buffer_alignment, stride_alignment, use_gpu_fb))
				return RETURN_MEM_ERROR;

			if (r.get_bit())
				read_frame_size(r, &cur_frame->render_width, &cur_frame->render_height);

			allow_high_precision_mv = r.get_bit();
			interp_filter			= read_interp_filter(r);
		}
	}

	if (width != last_width || height != last_height) {
		const int old_mi_cols = mi_cols;
		const int old_mi_rows = mi_rows;

		mi_cols		= align_pow2(width, MI_SIZE_LOG2) >> MI_SIZE_LOG2;
		mi_rows		= align_pow2(height, MI_SIZE_LOG2) >> MI_SIZE_LOG2;
		mi_stride	= mi_cols + MI_BLOCK_SIZE;

		if (mi_cols > old_mi_cols || mi_rows > old_mi_rows) {
			int		mi_size = mi_stride * (mi_rows + MI_BLOCK_SIZE);
			mi_array.resize(mi_size);
			mi_grid.resize(mi_size);

			for (int i = 0; i < 2; ++i)
				seg_map[i].create(mi_rows * mi_cols);

			lfm_stride	= round_pow2(mi_cols, MI_BLOCK_SIZE_LOG2);
			lfm.resize(round_pow2(mi_rows,MI_BLOCK_SIZE_LOG2) * lfm_stride);
		}

		memset(mi_grid, 0, mi_stride * (mi_rows + 1) * sizeof(ModeInfo*));
		memset(seg_map[0], 0, mi_rows * mi_cols);//last_frame_seg_map
	}

	if (need_resync)
		return RETURN_CORRUPT_FRAME;

	use_prev_frame_mvs	= !error_resilient_mode && width == last_width && height == last_height && !last_intra_only && last_show_frame && last_frame_type != KEY_FRAME;

	if (!error_resilient_mode) {
		refresh_frame_context			= r.get_bit();
		frame_parallel_decoding_mode	= r.get_bit();
		if (!frame_parallel_decoding_mode)
			clear(counts);

	} else {
		refresh_frame_context			= false;
		frame_parallel_decoding_mode	= true;
	}

	// This flag will be overridden by the call to setup_past_independence below, forcing the use of context 0 for those frame types.
	frame_context_idx	= r.get(FRAME_CONTEXTS_LOG2);
	pre_fc				= &frame_contexts[frame_context_idx];
	fc					= *pre_fc;

	if (reset_frame_context)
		setup_past_independence(reset_frame_context);

	read_loopfilter(r, &lf);
	lf.filter_level = 0;

	read_quantization(r, &quant);
	read_segmentation(r, &seg);

	quant.bit_depth		= cs.bit_depth;
	lossless			= quant.is_lossless();

	// Build y/uv dequant values based on Segmentation.
	if (seg.enabled) {
		for (int i = 0; i < MAX_SEGMENTS; ++i)
			quant.set(i, seg.get_data(i, Segmentation::FEATURE_ALT_Q, quant.base_index), cs.bit_depth);
	} else {
		// When Segmentation is disabled, only the first value is used - the remaining are don't cares.
		quant.set(0, quant.base_index, cs.bit_depth);
	}

	// tile columns
	int	t			= log2_floor((mi_cols + (1 << MI_BLOCK_SIZE_LOG2) - 1) >> MI_BLOCK_SIZE_LOG2);
	log2_tile_cols	= max(t - 6, 0);
	for (int max_ones = max(t - 2, 0) - log2_tile_cols; max_ones && r.get_bit(); --max_ones)
		log2_tile_cols++;

	if (log2_tile_cols > 6)
		return RETURN_CORRUPT_FRAME;

	// tile rows
	log2_tile_rows = r.get_bit();
	if (log2_tile_rows)
		log2_tile_rows += r.get_bit();

	int		tiled_stride = align_pow2(mi_cols, MI_BLOCK_SIZE_LOG2) << log2_tile_rows;
	above_context.resize(tiled_stride * 3 * 2);
	above_dependency.resize(tiled_stride * 3 * 2);
	above_seg_context.resize(tiled_stride);

	return r.get(16);
}

//-----------------------------------------------------------------------------
//	compressed header
//-----------------------------------------------------------------------------

TX_MODE read_tx_mode(vp9::reader &r) {
	int mode = r.read_literal(2);
	if (mode == ALLOW_32X32)
		mode += r.read_bit();
	return (TX_MODE)mode;
}

REFERENCE_MODE read_frame_reference_mode(vp9::reader &r, bool allow_compound) {
	return allow_compound && r.read_bit()
		? (r.read_bit() ? REFMODE_SELECT : REFMODE_COMPOUND)
		: REFMODE_SINGLE;
}

force_inline int decode_uniform(vp9::reader &r) {
	const int l = 8;
	const int m = (1 << l) - 191;
	const int v = r.read_literal(l - 1);
	return v < m ? v : (v << 1) - m + r.read_bit();
}
force_inline int decode_term_subexp(vp9::reader &r) {
	return	!r.read_bit()	? r.read_literal(4)
		:	!r.read_bit()	? r.read_literal(4) + 16
		:	!r.read_bit()	? r.read_literal(5) + 32
		:	decode_uniform(r) + 64;
}

force_inline int inv_recenter_nonneg(int v, int m) {
	return	v > 2 * m
		?	v
		:	(v & 1)
		?	m - ((v + 1) >> 1)
		:	m + (v >> 1);
}
force_inline int inv_remap_prob(int v, int m) {
	static uint8 inv_map_table[255] = {
		  7,  20,  33,  46,  59,  72,  85,  98, 111, 124, 137, 150, 163, 176, 189,
		202, 215, 228, 241, 254,   1,   2,   3,   4,   5,   6,   8,   9,  10,  11,
		 12,  13,  14,  15,  16,  17,  18,  19,  21,  22,  23,  24,  25,  26,  27,
		 28,  29,  30,  31,  32,  34,  35,  36,  37,  38,  39,  40,  41,  42,  43,
		 44,  45,  47,  48,  49,  50,  51,  52,  53,  54,  55,  56,  57,  58,  60,
		 61,  62,  63,  64,  65,  66,  67,  68,  69,  70,  71,  73,  74,  75,  76,
		 77,  78,  79,  80,  81,  82,  83,  84,  86,  87,  88,  89,  90,  91,  92,
		 93,  94,  95,  96,  97,  99, 100, 101, 102, 103, 104, 105, 106, 107, 108,
		109, 110, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 125,
		126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 138, 139, 140, 141,
		142, 143, 144, 145, 146, 147, 148, 149, 151, 152, 153, 154, 155, 156, 157,
		158, 159, 160, 161, 162, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173,
		174, 175, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 190,
		191, 192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 203, 204, 205, 206,
		207, 208, 209, 210, 211, 212, 213, 214, 216, 217, 218, 219, 220, 221, 222,
		223, 224, 225, 226, 227, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238,
		239, 240, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 253
	};
	v = inv_map_table[v];
	m--;
	return (m << 1) <= 255
		? 1		+ inv_recenter_nonneg(v, m)
		: 255	- inv_recenter_nonneg(v, 255 - 1 - m);
}

void diff_update_probs(vp9::reader &r, prob &p, int M) {
	prob	*pp = &p;
	for (int i = 0; i < M; i++) {
		if (r.read(FrameContext::DIFF_UPDATE_PROB))
			pp[i] = (prob)inv_remap_prob(decode_term_subexp(r), pp[i]);
	}
}
template<typename T> force_inline void diff_update_probs(vp9::reader &r, Bands<T> &p, int M);

template<typename T, int N> force_inline void diff_update_probs(vp9::reader &r, T (&p)[N], int M) {
	diff_update_probs(r, p[0], M * N);
}
template<typename T> force_inline void diff_update_probs(vp9::reader &r, Bands<T> &p, int M) {
	diff_update_probs(r, p.band0[0], int(M * p.TOTAL));
}
template<typename T> force_inline void diff_update_probs(vp9::reader &r, T &p) {
	diff_update_probs(r, p, 1);
}

void mv_update_probs(vp9::reader &r, prob &p, int M) {
	prob	*pp = &p;
	for (int i = 0; i < M; ++i)
		if (r.read(FrameContext::MV_UPDATE_PROB))
			pp[i] = (r.read_literal(7) << 1) | 1;
}
template<typename T, int N> force_inline void mv_update_probs(vp9::reader &r, T (&p)[N], int M) {
	mv_update_probs(r, p[0], M * N);
}
template<typename T> force_inline void mv_update_probs(vp9::reader &r, T &p) {
	mv_update_probs(r, p, 1);
}

void Decoder::read_compressed_header(vp9::reader &r) {
	tx_mode = quant.is_lossless() ? ONLY_4X4 : read_tx_mode(r);
	if (tx_mode == TX_MODE_SELECT) {
		diff_update_probs(r, fc.tx_8x8);
		diff_update_probs(r, fc.tx_16x16);
		diff_update_probs(r, fc.tx_32x32);
	}
	for (int tx_size = TX_4X4, max_tx_size = tx_mode_to_biggest_tx_size[tx_mode]; tx_size <= max_tx_size; ++tx_size) {
		if (r.read_bit())
			diff_update_probs(r, fc.coef_probs[tx_size]);
	}

	diff_update_probs(r, fc.skip_probs);

	if (!frame_is_intra_only()) {
		FrameContext::mvs *const mv = &fc.mv;
		diff_update_probs(r, fc.inter_mode_probs);

		if (interp_filter == INTERP_SWITCHABLE)
			diff_update_probs(r, fc.switchable_interp_prob);

		diff_update_probs(r, fc.intra_inter_prob);


		bool	allow_compound	= false;
		for (int i = 1; i < REFS_PER_FRAME; ++i) {
			if (ref_frame_sign_bias[i + 1] != ref_frame_sign_bias[1]) {
				allow_compound = true;
				break;
			}
		}

		reference_mode = read_frame_reference_mode(r, allow_compound);

		if (reference_mode != REFMODE_SINGLE) {
			if (ref_frame_sign_bias[REFFRAME_LAST] == ref_frame_sign_bias[REFFRAME_GOLDEN]) {
				comp_ref[0]	= REFFRAME_ALTREF;
				comp_ref[1] = REFFRAME_LAST;
				comp_ref[2] = REFFRAME_GOLDEN;
			} else if (ref_frame_sign_bias[REFFRAME_LAST] == ref_frame_sign_bias[REFFRAME_ALTREF]) {
				comp_ref[0]	= REFFRAME_GOLDEN;
				comp_ref[1] = REFFRAME_LAST;
				comp_ref[2] = REFFRAME_ALTREF;
			} else {
				comp_ref[0]	= REFFRAME_LAST;
				comp_ref[1] = REFFRAME_GOLDEN;
				comp_ref[2] = REFFRAME_ALTREF;
			}
		}

		if (reference_mode == REFMODE_SELECT)
			diff_update_probs(r, fc.comp_inter_prob);

		if (reference_mode != REFMODE_COMPOUND)
			diff_update_probs(r, fc.single_ref_prob);

		if (reference_mode != REFMODE_SINGLE)
			diff_update_probs(r, fc.comp_ref_prob);

		diff_update_probs(r, fc.y_mode_prob);
		diff_update_probs(r, fc.partition_prob);

		mv_update_probs(r, mv->joints);

		for (int i = 0; i < 2; ++i) {
			FrameContext::mvs::component *const comp_ctx = &mv->comps[i];
			mv_update_probs(r, comp_ctx->sign);
			mv_update_probs(r, comp_ctx->classes);
			mv_update_probs(r, comp_ctx->class0);
			mv_update_probs(r, comp_ctx->bits);
		}

		for (int i = 0; i < 2; ++i) {
			FrameContext::mvs::component *const comp_ctx = &mv->comps[i];
			for (int j = 0; j < MotionVector::CLASS0_SIZE; ++j)
				mv_update_probs(r, comp_ctx->class0_fp[j]);
			mv_update_probs(r, comp_ctx->fp);
		}

		if (allow_high_precision_mv) {
			for (int i = 0; i < 2; ++i) {
				FrameContext::mvs::component *const comp_ctx = &mv->comps[i];
				mv_update_probs(r, comp_ctx->class0_hp);
				mv_update_probs(r, comp_ctx->hp);
			}
		}
	}
}

//-----------------------------------------------------------------------------
//	TileWorkerData
//-----------------------------------------------------------------------------

int Decoder::TileWorkerData::init(Decoder *const dec, const uint8 *data, const uint8 *data_end, int tile_row, int tile_col) {
	r.init(const_memory_block(data, data_end - data));
	if (r.read_bit())
		return RETURN_UNSUP_BITSTREAM;

	if (!dec->gpu) {
		clear(dqcoeff);
		mb.dqcoeff	= dqcoeff;
	}

	mb.counts	= dec->frame_parallel_decoding_mode ? NULL : &dec->counts;
	mb.plane[0].set_sampling(0, 0);
	mb.plane[1].set_sampling(dec->cs.subsampling_x, dec->cs.subsampling_y);
	mb.plane[2].set_sampling(dec->cs.subsampling_x, dec->cs.subsampling_y);

	dec->get_tile_offsets(tile_col, tile_row, &mb.tile.mi_col_start, &mb.tile.mi_row_start);
	dec->get_tile_offsets(tile_col + 1, tile_row + 1, &mb.tile.mi_col_end, &mb.tile.mi_row_end);

	const int aligned_cols	= align_pow2(dec->mi_cols, MI_BLOCK_SIZE_LOG2);
	for (int i = 0; i < 3; ++i) {
		mb.above_context[i] = dec->above_context + (tile_row + i) * 2 * aligned_cols;
		mb.above_dependency[i] = dec->above_dependency + (tile_row + i) * 2 * aligned_cols;
		//mb.plane[i].set_seg_dequant(i == 0 ? dec->quant.y_dequant : dec->quant.uv_dequant);
	}
	mb.above_seg_context	= dec->above_seg_context;

	memcpy(mb.seg_dequant, dec->quant.dequant, sizeof(dec->quant.dequant));

	return RETURN_OK;
}


int Decoder::TileWorkerData::process1row(Decoder *const dec, int mi_row) {
	clear(mb.left_context);
	clear(mb.left_seg_context);

	if (dec->gpu)
		clear(mb.left_dependency);

	for (int mi_col = mb.tile.mi_col_start; mi_col < mb.tile.mi_col_end; mi_col += MI_BLOCK_SIZE) {
		if (int ret = dec->decode_partition(r, &mb, mi_row, mi_col, TX_64X64))
			return ret;
	}

	return RETURN_OK;
}

int Decoder::TileWorkerData::process(Decoder *const dec) {
	for (int mi_row = mb.tile.mi_row_start; mi_row < mb.tile.mi_row_end; mi_row += MI_BLOCK_SIZE) {
		if (int ret = process1row(dec, mi_row))
			return ret;
	}
	return RETURN_OK;
}
