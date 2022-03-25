#include "samplefile.h"

using namespace iso;

typedef signed char int8_t;
typedef signed short int16_t;
typedef signed int int32_t;
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef signed long long int64_t;
typedef unsigned long long uint64_t;

extern "C" {
#include "a52dec-0.7.4\include\a52.h"
}

struct AC3Buf {
	AC3Buf		*next;
	size_t		size;
	void		*data;

	AC3Buf(size_t _size)				: size(_size), next(NULL)	{ data = malloc(size); }
	AC3Buf(void *_data, size_t _size)	: size(_size), next(NULL)	{ data = malloc(size); memcpy(data, _data, size); }
	~AC3Buf()	{ free(data);}
};

class AC3FileHandler : public SampleFileHandler {
	const char*		GetExt()			{ return "ac3";						}
	const char*		GetDescription()	{ return "AC-3 (Dolby Digital)";	}

	ISO_ptr<void> Read(tag id, istream_ref file) override {
		malloc_chain	ch;
		a52_state_t	*state		= a52_init(0);
		sample_t	*samples	= a52_samples(state);
		int			flags		= 0;
		int			sample_rate;
		int			bit_rate;
		int			num_chans;

		uint8_t		buf[3840];

		while (file.readbuff(buf, 7) == 7) {
			int	length;
			while (!(length = a52_syncinfo(buf, &flags, &sample_rate, &bit_rate))) {
				uint8_t *bufptr;
				for (bufptr = buf; bufptr < buf + 6; bufptr++)
					bufptr[0] = bufptr[1];
				bufptr[0] = file.getc();
			}
			file.readbuff(buf + 7, length - 7);

			sample_t level, bias = 0;
//			flags |= A52_ADJUST_LEVEL;
			if (a52_frame(state, buf, &flags, &level, bias))
				break;

			switch (flags & A52_CHANNEL_MASK) {
				case A52_CHANNEL:	num_chans = 0; break;	//?
				case A52_MONO:		num_chans = 1; break;
				case A52_STEREO:	num_chans = 2; break;
				case A52_3F:		num_chans = 3; break;
				case A52_2F1R:		num_chans = 3; break;
				case A52_3F1R:		num_chans = 4; break;
				case A52_2F2R:		num_chans = 4; break;
				case A52_3F2R:		num_chans = 5; break;
				case A52_CHANNEL1:	num_chans = 1; break;
				case A52_CHANNEL2:	num_chans = 1; break;
				case A52_DOLBY:		num_chans = 5; break;
			}
			num_chans += flags & A52_LFE ? 1 : 0;

			for (int i = 0; i < 6; i++) {
				if (a52_block(state))
					break;
			}

			auto		t	= ch.push_back(sizeof(uint16) * num_chans * 256);
			sample_t	*s	= samples;
			for (int i = 0; i < num_chans; i++) {
				int16	*p	= (int16*)t + i;
				for (int j = 0; j < 256; j++, p += num_chans)
					*p = int(*s++ * 32767);
			}
		}

		a52_free(state);

		ISO_ptr<sample> sm(id);
		sm->SetFrequency(sample_rate);
		ch.copy_to(sm->Create(ch.total() / (num_chans * sizeof(int16)), num_chans, 16));
		return sm;
	}
} ac3;
