#ifndef IFF_H
#define IFF_H

#include "iso/iso.h"
#include "stream.h"

namespace iso {
/*
struct IFF_ID {
	char		c[4];
	IFF_ID()					{}
	IFF_ID(uint32be i)			{ memcpy(c, &i, 4); }
	operator tag2()		const	{ return str(c, 4); }
	operator uint32()	const	{ return *(uint32be*)c; }
};
*/
//-----------------------------------------------------------------------------
//	IFF Reader
//-----------------------------------------------------------------------------

class IFF_chunk : public istream_chain {
	streamptr	endofchunk;
	uint8		alignment;
public:
	uint32		id;

	IFF_chunk(istream_ref stream, uint8 _alignment = 2) : istream_chain(stream), alignment(_alignment) {
		istream_chain::read(id);
		endofchunk = istream_chain::get<uint32be>();
		endofchunk += istream_chain::tell();
	}
	~IFF_chunk()						{ seek(endofchunk); align(alignment); }
	uint32		remaining()				{ return (uint32)max(int(endofchunk - tell()), 0); }
	istream_ref	istream()		const	{ return istream_chain::t;	}

	bool	is(uint32 i)		const	{ return id == i; }
	int		is_ext(uint32 i)	const	{ return id == i ? 2 : is_digit(id & 0xff) && (id >> 8) == (i >> 8) ? id & 15 : 0; }

	template<typename T>bool	read(T &t)	{
		endian_t<T, true> t2;
		if (iso::read(*this, t2)) {
			t2 = t;
			return true;
		}
		return false;
	}
	template<typename T>T		get()		{
		endian_t<T, true> t2;
		iso::read(*this, t2);
		T	t;
		t = t2;
		return t;
	}
	getter<IFF_chunk>			get()		{ return *this;	}
};

//-----------------------------------------------------------------------------
//	IFF Writer
//-----------------------------------------------------------------------------

class IFF_Wchunk : public ostream_chain {
	streamptr	startofchunk;
	uint8		alignment;
public:
	IFF_Wchunk(ostream_ref stream, uint32 id, uint8 alignment = 2) : ostream_chain(stream), alignment(alignment) {
		startofchunk = tell();
		write(id);
		write(uint32(0));
	}
	~IFF_Wchunk()	{
		streamptr end = tell();
		seek(startofchunk + 4);
		write(uint32be(uint32(end - startofchunk) - 8));
		seek(end);
		align(2, 0);
	}
};

//-----------------------------------------------------------------------------
//	IEEE10
//-----------------------------------------------------------------------------

class IEEE10 {
	uint16be			sign_expon;
	packed<uint64be>	mant;
public:
	IEEE10()	{}
	IEEE10(double num) {
		if (num == 0) {
			sign_expon	= 0;
			mant		= 0;
		} else {
			iord		u(num);
			sign_expon	= (u.s ? 0x8000 : 0) | (u.is_special() ? 0x7fff : u.e - 1023 + 16383);
			mant		= u.m << (64 - 52);
		}
	}
	operator double() {
		return iord(mant, min((sign_expon & 0x7fff) - 16383 + 1023, 0x7ff), sign_expon >> 15).f();
	}
};

struct aiff_structs : packed_types<bigendian_types> {
	struct LOOP {
		uint16	playMode;
		uint16	beginLoop;
		uint16	endLoop;
	};

	struct INSTchunk {
		uint8	baseNote;
		uint8	detune;
		uint8	lowNote;
		uint8	highNote;
		uint8	lowVelocity;
		uint8	highVelocity;
		uint16	gain;
		LOOP	sustainLoop;
		LOOP	releaseLoop;
	};

	struct COMMchunk {
		uint16	channels;
		uint32	frames;
		uint16	bits;
		IEEE10	samplerate;
	};

	struct SSNDchunk {
		uint32	offset;
		uint32	blocksize;
	};
};

ISO_ptr<void> ReadForm(IFF_chunk &iff, int alignment = 2);

} //namespace iso

#endif	//IFF_H
