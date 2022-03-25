#ifndef RIFF_H
#define RIFF_H

#include "stream.h"

namespace iso {
/*
struct RIFF_ID {
	char c[4];
	RIFF_ID()			{}
	RIFF_ID(uint32be i)	{ memcpy(c, &i, 4); }
	operator tag2() const { return str(c, 4); }
};
*/
//-----------------------------------------------------------------------------
//	RIFF Reader
//-----------------------------------------------------------------------------

template<bool be> class _RIFF_chunk : public istream_chain {
	streamptr	endofchunk;
public:
	uint32	id;

	_RIFF_chunk(istream_ref stream) : istream_chain(stream) {
		istream_chain::read(id);
		endofchunk = istream_chain::get<typename endian_types0<be>::int32>();
		endofchunk += istream_chain::tell();
	}
	~_RIFF_chunk()					{ if (endofchunk) istream_chain::seek((endofchunk + 1) & ~1);	}

	uint32		remaining()			{ return (uint32)max(int(endofchunk - istream_chain::tell()), 0); }
	istream_ref	istream()	const	{ return istream_chain::t;	}
	void		unhook()			{ endofchunk = 0;	}
};

typedef _RIFF_chunk<false>	RIFF_chunk;
typedef _RIFF_chunk<true>	RIFX_chunk;

//-----------------------------------------------------------------------------
//	RIFF Writer
//-----------------------------------------------------------------------------

template<bool be> class _RIFF_Wchunk : public ostream_chain {
	streamptr	startofchunk;
public:
	_RIFF_Wchunk(ostream_ref stream, uint32 id)	: ostream_chain(stream) {
		startofchunk = tell();
		write(id);
		write(0);
	}
	~_RIFF_Wchunk()	{
		streamptr end = tell();
		seek(startofchunk + 4);
		write(typename endian_types0<be>::int32(uint32(end - startofchunk) - 8));
		seek(end);
		align(2, 0);
	}
};

typedef _RIFF_Wchunk<false>	RIFF_Wchunk;
typedef _RIFF_Wchunk<true>	RIFX_Wchunk;


} //namespace iso

#endif	// RIFF_H
