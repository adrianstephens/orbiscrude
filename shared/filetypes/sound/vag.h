#ifndef VAG_H
#define VAG_H

#include "sample.h"
#include "stream.h"

#define  VAG_BLOCKSIZE  	28	/* block size */

typedef iso::int16 VAGraw[VAG_BLOCKSIZE];

enum VAG_mode {
	VAG_MODE_NORMAL	= 1,
	VAG_MODE_HIGH	= 2,
	VAG_MODE_LOW	= 3,
	VAG_MODE_4BIT	= 4,
};

enum VAG_blockattr {
	VAG_1_SHOT		= 0,
	VAG_1_SHOT_END	= 1,
	VAG_LOOP_START	= 2,
	VAG_LOOP_BODY	= 3,
	VAG_LOOP_END	= 4,
};

struct VAGheader : iso::bigendian_types {
	uint32		format;		/* always 'VAGp' for identifying*/
	uint32		ver;		/* format version (2) */
	uint32		ssa;		/* Source Start Address, always 0 (reserved for VAB format) */
	uint32		size;		/* Sound Data Size in byte */

	uint32		fs;			/* sampling frequency, 44100(>pt1000), 32000(>pt), 22000(>pt0800)... */
	uint16		volL;		/* base volume for Left channel */
	uint16		volR;		/* base volume for Right channel */
	uint16		pitch;		/* base pitch (includes fs modulation)*/
	uint16		ADSR1;		/* base ADSR1 (see SPU manual) */
	uint16		ADSR2;		/* base ADSR2 (see SPU manual) */
	uint16		reserved;	/* not in use */

	char		name[16];

	bool validate() const {
		return format == 'VAGp';
	}
};

struct VAGBLOCK {
	static double	f[8][2];

	iso::uint8	pack_info;
	iso::uint8	flags;
	iso::uint8	packed[VAG_BLOCKSIZE / 2];

	VAGBLOCK()									{ iso::clear(*this);				}
	VAGBLOCK&	Clear()							{ iso::clear(*this); return *this;	}

	iso::int16*	Unpack(iso::int16 *dest, double &s1, double &s2, int chans = 1);
	void		Pack(iso::int16 *samples, int _flags, double &s1, double &s2);
	int			FindPredict(iso::int16 *samples, double s1, double s2);
	void		Final()	{ memset(packed, 0x77, sizeof(packed)); pack_info = 7; flags = 0; }
};

struct VAGBLOCK2 {
	VAGBLOCK		block;
	double			s1, s2;

	VAGBLOCK2&	Read(iso::istream_ref file)				{ file.read(block);	return *this;	}
	void		Init()									{ s1 = s2 = 0;					}
	char*		Data()									{ return (char*)&block;				}
	int			Flags()									{ return block.flags;				}
	bool		Write(iso::ostream_ref file)				{ return file.write(block);			}

	iso::int16*	Unpack(iso::int16 *dest, int chans = 1)	{ return block.Unpack(dest, s1, s2, chans); }
	VAGBLOCK2&	Pack(iso::int16 *samples, int _flags)	{ block.Pack(samples, _flags, s1, s2); return *this; }
	int			FindPredict(iso::int16 *samples)		{ return block.FindPredict(samples, s1, s2); }
	VAGBLOCK2&	Final()									{ block.Final(); return *this; }
};

void WriteVAGData(iso::sample *sm, iso::ostream_ref file);

#endif	//VAG_H
