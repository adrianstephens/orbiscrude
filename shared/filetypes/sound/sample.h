#ifndef SAMPLE_H
#define SAMPLE_H

#include "iso/iso.h"

namespace iso {

struct sample {
	enum {
		LOOP		= 1 << 0,
		NOCOMPRESS	= 1 << 1,
		MUSIC		= 1 << 2,
	};
	ISO_openarray<int16>samples;
	float				frequency;
	uint8				channels, bits;
	uint16				flags;

	sample()	{}
	sample(float frequency, int channels = 1, int bits = 16, uint16 flags = 0) : frequency(frequency), channels(channels), bits(bits), flags(flags) {}
	int16*			Create(int length, int _channels, int _bits)	{ channels = _channels; bits = _bits; flags = 0; return samples.Create(length * channels, false);	}
	int16*			Extend(int extra)								{ uint32 size = samples.Count(); return &samples.Resize(size + extra * channels)[size];	}
	void			SetFrequency(float _frequency)					{ frequency = _frequency;		}

	int16*			Samples()			const	{ return samples; }
	float			Frequency()			const	{ return frequency; }
	uint8			Channels()			const	{ return channels; }
	uint8			Bits()				const	{ return bits; }
	uint16			Flags()				const	{ return flags; }

	uint32			BytesPerSample()	const	{ return (bits + 7) / 8 * channels;	}
	uint32			Length()			const	{ return channels ? samples.Count() / channels : 0;}
};

ISO_openarray<int16>	resample(sample *sm, float newfreq);
ISO_ptr<sample>			resample(ISO_ptr<sample> &sm, float newfreq);

}//namespace iso

ISO_DEFUSERCOMPV(iso::sample, samples, frequency, channels, bits, flags);

#endif	// SAMPLE_H