#include "base/defs.h"
#include "base/functions.h"
#include "base/strings.h"
#include "base/atomic.h"
#include "thread.h"
#include "graphics.h"
#include "object.h"
#include "render.h"
#include "postprocess/post.h"
#include "jobs.h"
#include "filename.h"
#include "_sound.h"

using namespace iso;

// BT.601, which is the standard for SDTV.
static const float kColorConversion601[] = {
	1.164f,  1.164f, 1.164f, 0,
	  0.0f, -0.392f, 2.017f, 0,
	1.596f, -0.813f,   0.0f, 0,
};

// BT.709, which is the standard for HDTV.
static const float kColorConversion709[] = {
	1.164f,  1.164f, 1.164f,
	  0.0f, -0.213f, 2.112f,
	1.793f, -0.533f,   0.0f
};

//-----------------------------------------------------------------------------
//	AVTexture
//-----------------------------------------------------------------------------

struct AVTexture  {
	Texture		luma, chroma, chroma2;

	struct ChannelBuffer {
		uint8	*data;
		uint32	stride;
		void	set(uint8 *_data, uint32 _stride) { data = _data; stride = _stride; }
	};

	AVTexture() {
//		luma.Init(TEXF_R8, 1, 1, 1, 1, MEM_WRITABLE);
//		chroma.Init(TEXF_R8G8, 1, 1, 1, 1, MEM_WRITABLE);
	}

	void	Set(uint32 width, uint32 height, void *luma_data, uint32 luma_pitch, void *chroma_data, uint32 chroma_pitch) {
		luma.Init(TEXF_R8, width, height, 1, 1, MEM_CPU_WRITE, luma_data, luma_pitch);
		chroma.Init(TEXF_R8G8, width / 2, height / 2, 1, 1, MEM_CPU_WRITE, chroma_data, chroma_pitch);
	}

	void	Set3(uint32 width, uint32 height, ChannelBuffer &y, ChannelBuffer &u, ChannelBuffer &v) {
		luma.Init(TEXF_R8, width, height, 1, 1, MEM_CPU_WRITE, y.data, y.stride);
		chroma.Init(TEXF_R8, width / 2, height / 2, 1, 1, MEM_CPU_WRITE, u.data, u.stride);
		chroma2.Init(TEXF_R8, width / 2, height / 2, 1, 1, MEM_CPU_WRITE, v.data, v.stride);
	}

	void	Set2(uint32 width, uint32 height, void *luma_data, uint32 luma_pitch, void *chroma_data, uint32 chroma_pitch) {
		luma.Init(TEXF_R8, width, height, 1, 1, MEM_CPU_WRITE);
		chroma.Init(TEXF_R8, width / 2, height + 32, 1, 1, MEM_CPU_WRITE);
	}
};

//-----------------------------------------------------------------------------
//	MovieTexture
//-----------------------------------------------------------------------------

struct MovieTexture  : AVTexture {
	enum State {
		PREPARING,
		WAITING,
		BUFFERING,
		READY,
		PAUSED,
		PLAYING,
		ENDED,
		FAILED,
		STOPPED,
	};
	atomic<State>	state;
	uint64		file_offset, file_size;

	MovieTexture(uint64 offset, uint64 size) : state(PREPARING), file_offset(offset), file_size(size) {
	}
};

//-----------------------------------------------------------------------------
//	MovieTextureVP9
//-----------------------------------------------------------------------------

#include "filetypes\video\matroska.h"
#include "filetypes\video\vpx_decode.h"

template<typename T, int N> struct Pipeline {
	Semaphore			used_count, free_count;
	T					buffer[N];
	circular_buffer<T*>	queue;
	bool				stop;

	Pipeline() : used_count(0), free_count(N), queue(buffer), stop(false) {}

	void	StartProduce()	{ free_count.lock(); }
	T&		Produce()		{ return queue.push_back(); }
	void	DoneProduce()	{ used_count.unlock(); }

	void	StartConsume()	{ used_count.lock(); }
	T&		Consume()		{ return queue.front(); }
	void	DoneConsume()	{ queue.pop_front(); free_count.unlock(); }
};

struct MovieBuffer : malloc_block {
	size_t	bytes_in_buffer;
	uint64	timestamp;
	MovieBuffer() : bytes_in_buffer(0)	{}
	void	needs(size_t _size)		{ if (size() < _size) create(_size); };
	void	addref()				{ ++bytes_in_buffer; }
	void	release()				{ ISO_ASSERT(bytes_in_buffer!=0); --bytes_in_buffer; }
};

struct MovieTextureVP9 : MovieTexture, HandlesGlobal<MovieTextureVP9, RenderEvent> {
	struct FrameBuffer {
		uint32			w, h;
		ChannelBuffer	y, u, v;
		int64			timestamp;
	#ifdef VPX_DECODE_H
		ref_ptr<vp9::FrameBuffer>	buffer;
	#else
		ref_ptr<MovieBuffer>		buffer;
	#endif
		FrameBuffer() {}
	#ifdef VPX_DECODE_H
		FrameBuffer(vp9::FrameBuffer *img) {
			buffer		= img;
			w		= img->y.crop_width;
			h		= img->y.crop_height;
			y.set(img->y_buffer.buffer, img->y_buffer.stride);
			u.set(img->u_buffer.buffer, img->u_buffer.stride);
			v.set(img->v_buffer.buffer, img->v_buffer.stride);
		}
		void	release()	{ buffer = 0; }
	#else
		FrameBuffer(vpx_image_t *img) {
			buffer		= (MovieBuffer*)img->fb_priv;
			w			= img->w;
			h			= img->h;
			y.set(img->planes[0], img->stride[0]);
			u.set(img->planes[1], img->stride[1]);
			v.set(img->planes[2], img->stride[2]);
		}
		//void	release()	{ buffer->release(); }
		void	release()	{ buffer = 0; }
	#endif
		void AdjustTimestamp(int64 duration) {
			timestamp -= duration;
		}
		int64 GetTimestamp() const {
			return timestamp;
		}
	};

	FileInput					file;
	uint32						width, height;
	float						framerate;
	int64						duration;
	int64						start_time;
	int64						frame_time	= 0;
	float						play_speed	= 0;
	bool						loop		= false;

	//vpx decoding
#ifdef VPX_DECODE_H
	vp9::Decoder				decoder;
	typedef	vp9::FrameBuffer	raw_image;
	typedef	vp9::FrameBuffer	disp_buffer;
#else
	vpx_codec_ctx_t				decoder;
	typedef	vpx_image_t			raw_image;
	typedef MovieBuffer			disp_buffer;
	MovieBuffer					out_buffer[32];
#endif

	//threading
	Pipeline<MovieBuffer, 3>	video_pipeline;
	Pipeline<MovieBuffer, 8>	audio_pipeline;

	FrameBuffer					frame_buffer[16];
	circular_buffer<FrameBuffer*>frame_queue		= frame_buffer;
	Semaphore					frame_free_count	= num_elements32(frame_buffer);
	ref_ptr<disp_buffer>		displayed[2];

	//matroska parsing
	matroska::MultiTrackReader	reader;

	struct ReadThread : Thread {
		MovieTextureVP9 *me;
		int operator()() {
			for (;;) {
				int		track = 0;
				matroska::Block::Frame *frame = me->reader.GetNextFrame(&track);
				if (frame) {
					if (track == 1) {
						me->video_pipeline.StartProduce();
						me->ReadFrame(me->video_pipeline.Produce(), frame, me->reader.GetTimestamp());
						me->video_pipeline.DoneProduce();
					} else {
						me->audio_pipeline.StartProduce();
						me->ReadFrame(me->audio_pipeline.Produce(), frame, me->reader.GetTimestamp());
						me->audio_pipeline.DoneProduce();
					}
				} else {
					me->video_pipeline.StartProduce();
					me->video_pipeline.Produce().bytes_in_buffer = 0;
					me->video_pipeline.DoneProduce();
					if (me->loop) {
						me->reader.Rewind();
					} else {
						break;
					}
				}
			}
			return 0;
		}
		ReadThread(MovieTextureVP9 *_me) : Thread(this, "VP9Read"), me(_me) {}
	} read_thread;

	struct VideoThread : Thread {
		MovieTextureVP9 *me;
		int operator()() {
			for (;;) {
				me->video_pipeline.StartConsume();
				if (me->state == STOPPED)
					break;

				MovieBuffer	&buffer = me->video_pipeline.Consume();
				if (buffer.bytes_in_buffer == 0) {
					me->video_pipeline.DoneConsume();

					// flush
					while (raw_image *img = me->GetDecodedFrame())
						me->SubmitFrame(img, buffer.timestamp);

					if (!me->loop) {
						me->state = ENDED;
						break;
					}

				} else if (me->DecodeFrame(buffer)) {
					me->video_pipeline.DoneConsume();

					if (raw_image *img = me->GetDecodedFrame())
						me->SubmitFrame(img, buffer.timestamp);

				} else {
					me->state = FAILED;
					break;
				}
			}
			return 0;
		}
		VideoThread(MovieTextureVP9 *_me) : Thread(this, "VP9Video"), me(_me) {}
	} video_thread;

	bool	TransitionState(State from, State to) {
		while (state == from && !state.cas(from, to));
		return state == to;
	}
	void		ReadFrame(MovieBuffer &read_buffer, matroska::Block::Frame *frame, uint64 timestamp) {
		read_buffer.needs(frame->len);
		file.seek(frame->pos);
		file.readbuff(read_buffer, frame->len);

		read_buffer.bytes_in_buffer	= frame->len;
		read_buffer.timestamp		= timestamp;
	}

	bool		DecodeFrame(const MovieBuffer &read_buffer) {
	#ifdef VPX_DECODE_H
		return decoder.decode_frame(read_buffer, read_buffer + read_buffer.bytes_in_buffer) >= 0;
	#else
		if (vpx_codec_decode(&decoder, read_buffer.bytes_in_buffer ? read_buffer : (const uint8*)0, (uint32)read_buffer.bytes_in_buffer, NULL, 0)) {
			ISO_TRACEF("Failed to decode frame: %s\n", vpx_codec_error(&decoder));
			return false;
		}
		return true;
	#endif
	}
	raw_image*	GetDecodedFrame() {
	#ifdef VPX_DECODE_H
		return decoder.get_frame();
	#else
		vpx_codec_iter_t iter	= 0;
		return vpx_codec_get_frame(&decoder, &iter);
	#endif
	}

	void	SubmitFrame(raw_image *img, uint64 timestamp) {
	#ifdef VPX_DECODE_H
		//if (!img->y_buffer.buffer)
		//	return;
	#endif

		if (timestamp == 0) {
			start_time += duration;
			for (int i = 0, n = frame_queue.size(); i < n; i++)
				frame_queue[i].AdjustTimestamp(duration);
		}

		frame_free_count.lock();
		if (frame_queue.push_back(img)) {
			frame_queue.back().timestamp = timestamp;
			if (!TransitionState(PREPARING, READY)) {
				if (state == WAITING) {
					start_time = time::now() * 1000;
					TransitionState(WAITING, play_speed == 0 ? PAUSED : PLAYING);
				}
			}
		}
	}

public:
#ifndef VPX_DECODE_H
	int get_buffer(size_t min_size, vpx_codec_frame_buffer_t *fb) {
		for (MovieBuffer *i = out_buffer, *e = end(out_buffer); i != e; ++i) {
			if (i->bytes_in_buffer == 0) {
				i->needs(min_size);
				i->bytes_in_buffer = 1;
				fb->priv = i;
				fb->data = *i;
				fb->size = i->size;
				return 0;
			}
		}
		return -1;
	}
	int release_buffer(vpx_codec_frame_buffer_t *fb) {
		MovieBuffer	*i = (MovieBuffer*)fb->priv;
		--i->bytes_in_buffer;
		return 0;
	}
#endif
	void operator()(RenderEvent &re);

	MovieTextureVP9(const char *filename, uint64 offset, uint64 size);
	~MovieTextureVP9()	{}
	bool	Init();

	bool	SetSpeed(float speed)	{
		if (!TransitionState(PREPARING, WAITING)) {
			int64 time	= time::now() * 1000;
			start_time	= play_speed ? time - (time - start_time) * (speed / play_speed) : time - frame_time;
			state		= speed ? PLAYING : PAUSED;
		}
		play_speed = speed;
		return true;
	}
	bool	Play()					{ return SetSpeed(1); }
	bool	Pause()					{ return SetSpeed(0); }
	bool	Stop()					{ return false;	}
	float	GetTime()				{ return (time::now() * 1000 - start_time) * play_speed * 1e-9f; }

	bool	SetTime(float t)		{
		if (play_speed)
			start_time = time::now() * 1000 - t * 1e9f / play_speed;
		frame_time = int64(t * 1e9);
		TransitionState(PREPARING, WAITING);
		return true;
	}
	bool	SetLoop(bool _loop)		{ loop = _loop; return true; }
};

MovieTextureVP9::MovieTextureVP9(const char *filename, uint64 offset, uint64 size) : MovieTexture(offset, size), file(filename), read_thread(this), video_thread(this) {
	clear(displayed);
	if (file_size == 0)
		file_size = file.length();

	ConcurrentJobs::Get<ThreadPriority::LOW>().add([this]() {
		EBMLHeader			header;
		matroska::Segment	*segment;

		if (!header.read(file)
		|| matroska::Segment::CreateInstance(file, file.tell(), segment)
		|| segment->Load() < 0
		) {
			state = FAILED;
			return;
		}

		const matroska::Tracks *const tracks = segment->tracks;
		const matroska::VideoTrack* video_track = NULL;
		const matroska::AudioTrack* audio_track = NULL;
		for (uint32 i = 0; i < tracks->size32(); ++i) {
			const matroska::Track* const track = (*tracks)[i];
			switch (track->type) {
				case matroska::Track::kVideo: video_track = static_cast<const matroska::VideoTrack*>(track); break;
				case matroska::Track::kAudio: audio_track = static_cast<const matroska::AudioTrack*>(track); break;
			}
		}

		if (!video_track || !video_track->codec_id) {
			state = FAILED;
			return;
		}

		width		= video_track->width;
		height		= video_track->height;
		framerate	= video_track->rate;
		duration	= segment->info->GetDuration();

		if (framerate == 0) {
			this->reader.Init(segment, bit64(video_track->number));
			uint32	i = 0;
			int		track;
			while (i < 50 && reader.GetNextFrame(&track) && reader.GetTimestamp() < 1000000000)
				++i;
			framerate = i * 1e9 / reader.GetTimestamp();
		}

	#ifdef FFVP9
		const AVCodec	*codec		= &ff_vp9_decoder;
		AVDictionary	**options	= 0;

		frame		= av_frame_alloc();
		ctx			= avcodec_alloc_context3(codec);
		if (codec->capabilities & AV_CODEC_CAP_TRUNCATED)
			ctx->flags |= AV_CODEC_FLAG_TRUNCATED; // we do not send complete frames

		bool	valid = avcodec_open2(ctx, codec, options) == 0;
	#elif defined(VPX_DECODE_H)
		decoder.buffer_alignment	= 256;
		decoder.stride_alignment	= 64;
		decoder.threads				= 5;
		//decoder.use_gpu			= false;
		bool	valid = video_track->codec_id.begins("V_VP9");
	#else
		vpx_codec_iface_t	*iface		= 0;
		vpx_codec_dec_cfg_t	cfg			= { 5, 0, 0 };
		int					dec_flags	= 0;//(postproc ? VPX_CODEC_USE_POSTPROC : 0) | (ec_enabled ? VPX_CODEC_USE_ERROR_CONCEALMENT : 0) | (frame_parallel ? VPX_CODEC_USE_FRAME_THREADING : 0);

		if (video_track->codec_id.begins("V_VP8")) {
			iface = vpx_codec_vp8_dx();
		} else if (video_track->codec_id.begins("V_VP9")) {
			iface = vpx_codec_vp9_dx();
	//	} else if (video_track->codec_id.begins("V_VP10")) {
	//		iface = vpx_codec_vp10_dx();
		}
		bool	valid = !!iface;
		if (iface) {
			int	err;
			err = vpx_codec_dec_init(&decoder, iface, &cfg, dec_flags);
			err = vpx_codec_control_(&decoder, VP9_SET_BYTE_ALIGNMENT, 256);
			err = vpx_codec_set_frame_buffer_functions(
				&decoder,
				make_staticfunc(&MovieTextureVP9::get_buffer),
				make_staticfunc(&MovieTextureVP9::release_buffer),
				this
			);
		}
	#endif
		if (valid) {
			video_thread.Start();
			uint32	tracks = 1 << video_track->number;
			this->reader.Init(segment, tracks);
			read_thread.Start();

		} else {
			state = FAILED;
		}
	});
}

void MovieTextureVP9::operator()(RenderEvent &re) {
	if (play_speed)
		frame_time = int64((time::now() * 1000 - start_time) * play_speed);

	while (frame_queue.size() > 1 && frame_queue[1].GetTimestamp() < frame_time) {
		frame_queue.front().release();
		frame_queue.pop_front();
		frame_free_count.unlock();
	}

	if (displayed[1])
		displayed[1] = 0;

	if (frame_queue.empty())
		return;

	FrameBuffer &fb = frame_queue.front();

	if (fb.GetTimestamp() <= frame_time) {
		displayed[1] = displayed[0];
		displayed[0] = fb.buffer;
		frame_queue.pop_front();
		frame_free_count.unlock();

		if (fb.buffer->texture) {
			luma = ((Texture&)fb.buffer->texture).As(TEXF_R8G8B8A8);
		} else {
			Set3(width, height, fb.y, fb.u, fb.v);
		}

		int64	time = int64(time::now() * 1000 - start_time);
		ISO_OUTPUTF("Framerate: %f\n", fb.GetTimestamp() / float(time));
	}
}

//-----------------------------------------------------------------------------
//	MovieBackground
//-----------------------------------------------------------------------------

struct MovieBackground : HandlesWorld<MovieBackground, RenderEvent> {
	MovieTexture	*movietex;
	bool			vp9;
	int				last_frame;

	void	operator()(RenderEvent *re, uint32 extra) {
		float4x4	uv_matrix	= diagonal4(float4{1024, 2048, 1, 1});
		AddShaderParameter("uv_matrix", uv_matrix);
		AddShaderParameter("yuv_conversion", kColorConversion601);
		AddShaderParameter("yuv_samp", movietex->luma);
		AddShaderParameter("yuv_tex", movietex->luma);

		re->ctx.SetBackFaceCull(BFC_NONE);
		re->ctx.SetDepthTestEnable(false);
		re->ctx.SetDepthWriteEnable(false);

		technique	&t = *(vp9 ? PostEffects::shaders->yuv3 : PostEffects::shaders->yuv);
		if (re->flags & RF_STEREOSCOPIC) {
			Set(re->ctx, t[1]);
			PostEffects(re->ctx).DrawRect(float2(-one), float2(one), float2(-one), float2(one));
		} else {
			PostEffects(re->ctx).FullScreenQuad(t[0]);
		}
	}
	void	operator()(RenderEvent &re) {
		if (movietex->state == MovieTexture::PLAYING || movietex->state == MovieTexture::PAUSED)
			re.AddRenderItem(this, MakeKey(RS_PRE, 0), 0);
	}

	MovieBackground(const char *fn, uint64 offset, uint64 size) : last_frame(0) {
		if (vp9 = (filename(fn).ext() == ".webm")) {
			MovieTextureVP9	*m = new MovieTextureVP9(fn, offset, size);
			m->SetLoop(true);
			m->SetSpeed(1);///30.f);
			//m->SetTime(30.f);
			movietex	= m;
		} else {
			//MovieTextureAV *m = new MovieTextureAV(fn, offset, size);
			//m->Play();
			//movietex	= m;
		}
	}
};

void init_movie_bg(const char *fn, uint64 offset, uint64 size) {
	new MovieBackground(fn, offset, size);
}
