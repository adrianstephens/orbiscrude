#ifndef XMA_H
#define XMA_H

#include "iso/iso.h"

namespace iso {

struct XMA {
	enum {
		LOOP	= 1 << 0,
	};
	struct stream {
		float	frequency;
		int		channels;
	};
	ISO_openarray<stream>	streams;
	ISO_openarray<xint8>	data;
	int						numsamples;
	int						flags;
	void			Create(int _size, int _streams)	{ streams.Create(_streams); data.Create(_size);	}
};

}//namespace iso

ISO_DEFUSERCOMPV(iso::XMA::stream, frequency, channels);
ISO_DEFUSERCOMPV(iso::XMA, streams, data, numsamples, flags);

#endif	// XMA_H