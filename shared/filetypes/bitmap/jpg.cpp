#include "jpg.h"
#include "bitmapfile.h"
#include "maths/dct.h"
#include "utilities.h"	// for thread_temp_allocator

using namespace iso;

inline int	get16(istream_ref file)				{ return file.get<uint16be>();	}
inline void	put16(ostream_ref file, uint16be i)	{ file.write(i);				}

//-----------------------------------------------------------------------------
//
//	HUFFMAN ROUTINES
//
//-----------------------------------------------------------------------------

void JPG::HuffTable::Set(const uint8 *_bits, const uint8 *_val) {
	memcpy(bits, _bits, sizeof(bits));
	memcpy(val,  _val,  sizeof(val));
	FixDecode();
}

void JPG::HuffTable::FixDecode() {
	int		code = 0, sum = 0;

	for (int len = 1, i = 0; len <= 8; len++) {
		sum += bits[len];
		for (int j = 0; j < bits[len]; j++) {
			uint8 v = val[i++];
			for (int k = 0; k < 1 << (8 - len); k++, code++) {
				qval[code] = v;
				qlen[code] = len;
			}
		}
	}

	sum8	= sum;
	code8	= code;

	while (code < 256)
		qlen[code++] = 0;
}

int JPG::HuffTable::Slow(VLCin &vlc) const {
	const uint8	*p	= bits + 8 + 1;
	int		codeoff	= sum8;
	int		code	= vlc.get(8 + 1) - code8 * 2;

	while (code >= *p && p < end(bits)) {
		codeoff += *p;
		code	 = ((code - *p++) << 1) | int(vlc.get_bit());
	}

	if (p == end(bits) || codeoff + code > 255)
		return -1;
	return val[codeoff + code];
}

inline int JPG::HuffTable::Decode(VLCin &vlc) const {
	int	look = vlc.peek(8);
	if (int nb = qlen[look]) {
		vlc.discard(nb);
		return qval[look];
	}
	return Slow(vlc);
}

void JPG::HuffTable::FixEncode() {
	uint8	*pv = val;
	clear(qcode);
	for (int i = 1, code = 0; i <= 16; i++, code <<= 1) {
		for (int j = 0; j < bits[i]; j++, code++) {
			uint8 v		= *pv++;
			qcode[v]	= code;
			qlen[v]		= i;
		}
	}
}

inline void JPG::HuffTable::Encode(VLCout &vlc, int v) {
	vlc.put(qcode[v], qlen[v]);
}

const uint8 JPG::ZAG[DCTSIZE2+16] = {
	  0,  1,  8, 16,  9,  2,  3, 10,
	 17, 24, 32, 25, 18, 11,  4,  5,
	 12, 19, 26, 33, 40, 48, 41, 34,
	 27, 20, 13,  6,  7, 14, 21, 28,
	 35, 42, 49, 56, 57, 50, 43, 36,
	 29, 22, 15, 23, 30, 37, 44, 51,
	 58, 59, 52, 45, 38, 31, 39, 46,
	 53, 60, 61, 54, 47, 55, 62, 63,
	  0,  0,  0,  0,  0,  0,  0,  0, // extra entries in case k>63
	  0,  0,  0,  0,  0,  0,  0,  0
};

//-----------------------------------------------------------------------------
//
//	DECODE
//
//-----------------------------------------------------------------------------

bool JPG::BLKdecode(VLCin &vlc, const HuffTable &AC, const HuffTable &DC, const QuantTable	&quant, short &lastDC, DCTBLOCK &block) {
	// decode the DC coefficient difference
	int	s	= DC.Decode(vlc);
	if (s < 0)
		return false;
	block[0] = (lastDC += (s ? vlc.get_signed(s) : 0)) * quant.val[0];

	// decode the AC coefficients
	for (int k = 1; k < 64; k++) {
		int	s = AC.Decode(vlc);
		if (s < 0)
			return false;
		int	r = s >> 4;		// r = run
		if (s &= 15) {		// s = # bits in VLI
			k += r;
			block[ZAG[k]] = vlc.get_signed(s) * quant.val[k];
		} else {
			if (r != 15)
				break;
			k += 15;
		}
	}
	return true;
}

//	Progressive, DC Initial scan
bool JPG::BLKdecode_DC0(VLCin &vlc, const HuffTable &ht, short &lastDC, DCTBLOCK &block, int Ss, int Se, int A) {
	int				s		= ht.Decode(vlc);
	if (s < 0)
		return false;
	block[0] = (lastDC += (s ? vlc.get_signed(s) : 0)) << A;
	return true;
}

//	Progressive, DC Refine
bool JPG::BLKdecode_DC1(VLCin &vlc, const HuffTable &ht, short &lastDC, DCTBLOCK &block, int Ss, int Se, int A) {
	if (vlc.get_bit())
		block[0] |= 1 << A;
	return true;
}

//	Progressive, AC Initial scan
bool JPG::BLKdecode_AC0(VLCin &vlc, const HuffTable &ht, short &EOBrun, DCTBLOCK &block, int Ss, int Se, int A) {
	if (EOBrun) {
		EOBrun--;
	} else {
		for (int k = Ss; k <= Se; k++) {
			int	s = ht.Decode(vlc);
			if (s < 0)
				return false;
			int	r = s >> 4;		// r = run
			if (s &= 15) {		// s = # bits in VLI
				k += r;
				block[k] = vlc.get_signed(s) << A;
			} else {
				if (r == 15)
					k += 15;
				else {
					EOBrun = 1 << r;
					if (r)
						EOBrun += vlc.get(r);
					EOBrun--;
					break;
				}
			}
		}
	}
	return true;
}

//	Progressive, AC Refine
bool JPG::BLKdecode_AC1(VLCin &vlc, const HuffTable &ht, short &EOBrun, DCTBLOCK &block, int Ss, int Se, int A) {
	int	p1	= 1 << A;
	int	k	= Ss;
	if (EOBrun == 0) {
		for ( ; k <= Se; k++) {
			int	s = ht.Decode(vlc);
			if (s < 0)
				return false;
			int	r = s >> 4;		// r = run
			if (s &= 15) {		// s = # bits in VLI
				s = vlc.get_signed(s) << A;
			} else if (r != 15) {
				EOBrun = 1 << r;
				if (r)
					EOBrun += vlc.get(r);
				break;
			}

			do {
				short	*coef = block + k;
				if (*coef) {
					if (vlc.get_bit()) {
						if ((*coef & p1) == 0) // do nothing if already set it
							*coef += (*coef >= 0) ? p1 : -p1;
					}
				} else if (--r < 0)
					break;
				k++;
			} while (k <= Se);

			if (s)
				block[k] = s;				// Output newly nonzero coefficient
		}
	}

	if (EOBrun > 0) {
		// Scan any remaining coefficient positions after the end-of-band (the last newly nonzero coefficient, if any).  Append a correction
		// bit to each already-nonzero coefficient.  A correction bit is 1 if the absolute value of the coefficient must be increased.
		for (; k <= Se; k++) {
			short	*coef = block + k;
			if (*coef != 0 && vlc.get_bit()) {
				if ((*coef & p1) == 0) // do nothing if already changed it
					*coef += (*coef >= 0) ? p1 : -p1;
			}
		}
		// Count one block completed in EOB run
		EOBrun--;
	}
	return true;
}

void JPG::GetRestart(istream_ref file) {
	JPEG_MARKER	c = GetMarker(file);	// Scan for next JPEG marker

//	if (c != (RST0 + restart_num)) {
	// The restart markers have been messed up
//	}

	// Re-initialize DC predictions to 0
	for (int i = 0; i < nComponents; i++)
		component[i].lastDC = 0;

	// Update restart state
	restart_num		= (restart_num + 1) & 7;
}

void JPG::ConvertBlock(const block<ISO_rgba, 2> &block, DCTBLOCK *blocks) {
	for (int j = 0; j < MCUheight; j++) {
		ISO_rgba	*dest	= block[j];
		switch (nBlocksInMCUF) {
			case 7: {
				DCTELEM	*ys		= blocks[0] + (j + (j & DCTSIZE)) * DCTSIZE;
				DCTELEM	*uvs	= blocks[4] + j/2 * DCTSIZE;
				DCTELEM	*as		= blocks[6] + j/2 * DCTSIZE;
				for (int i = 0; i < 4; i++, dest+=2, uvs++, ys+=2, as++) {
					dest[0]				= ISO_rgba::YCbCr(ys[0]			+ 128, uvs[0], uvs[DCTSIZE2]);
					dest[1]				= ISO_rgba::YCbCr(ys[1]			+ 128, uvs[0], uvs[DCTSIZE2]);
					dest[DCTSIZE+0]		= ISO_rgba::YCbCr(ys[DCTSIZE2]	+ 128, uvs[4], uvs[DCTSIZE2+4]);
					dest[DCTSIZE+1]		= ISO_rgba::YCbCr(ys[DCTSIZE2+1]+ 128, uvs[4], uvs[DCTSIZE2+4]);
					dest[0].a			= dest[1].a				= clamp(as[0] + 128, 0, 255);
					dest[DCTSIZE+0].a	= dest[DCTSIZE+1].a		= clamp(as[4] + 128, 0, 255);
				}
				break;
			}
			case 6: {
				if (MCUwidth == 16) {
					DCTELEM	*ys		= blocks[0] + (j + (j & DCTSIZE)) * DCTSIZE;
					DCTELEM	*uvs	= blocks[4] + j/2 * DCTSIZE;
					for (int i = 0; i < 4; i++, dest+=2, uvs++, ys+=2) {
						dest[0]			= ISO_rgba::YCbCr(ys[0]			+ 128, uvs[0], uvs[DCTSIZE2]);
						dest[1]			= ISO_rgba::YCbCr(ys[1]			+ 128, uvs[0], uvs[DCTSIZE2]);
						dest[DCTSIZE+0]	= ISO_rgba::YCbCr(ys[DCTSIZE2]	+ 128, uvs[4], uvs[DCTSIZE2+4]);
						dest[DCTSIZE+1]	= ISO_rgba::YCbCr(ys[DCTSIZE2+1]+ 128, uvs[4], uvs[DCTSIZE2+4]);
					}
				} else {
					DCTELEM	*srce	= blocks[0] + j * DCTSIZE;
					for (int i = 0; i < DCTSIZE; i++, dest++, srce++) {
						*dest = ISO_rgba::YCbCr(srce[0] + 128, srce[DCTSIZE2*2], srce[DCTSIZE2*4]);
					}
				}
				break;
			}
			case 4:
				if (MCUwidth == 16) {
					DCTELEM	*ys		= blocks[0] + j * DCTSIZE;
					DCTELEM	*uvs	= blocks[2] + j * DCTSIZE;
					for (int i = 0; i < 4; i++, dest+=2, uvs++, ys+=2) {
						dest[0]			= ISO_rgba::YCbCr(ys[0]			+ 128, uvs[0], uvs[DCTSIZE2]);
						dest[1]			= ISO_rgba::YCbCr(ys[1]			+ 128, uvs[0], uvs[DCTSIZE2]);
						dest[DCTSIZE+0]	= ISO_rgba::YCbCr(ys[DCTSIZE2]	+ 128, uvs[4], uvs[DCTSIZE2+4]);
						dest[DCTSIZE+1]	= ISO_rgba::YCbCr(ys[DCTSIZE2+1]+ 128, uvs[4], uvs[DCTSIZE2+4]);
					}
				} else {
					DCTELEM	*ys		= blocks[0] + j * DCTSIZE;
					DCTELEM	*uvs	= blocks[2] + j / 2 * DCTSIZE;
					for (int i = 0; i < 8; i++, dest++, uvs++, ys++) {
						dest[0]			= ISO_rgba::YCbCr(ys[0]			+ 128, uvs[0], uvs[DCTSIZE2]);
					}
				}
				break;

			case 3: {
				DCTELEM	*srce	= blocks[0] + j * DCTSIZE;
				for (int i = 0; i < DCTSIZE; i++, dest++, srce++)
					*dest = ISO_rgba::YCbCr(srce[0] + 128, srce[DCTSIZE2], srce[DCTSIZE2*2]);
				break;
			}
			case 1: {
				DCTELEM	*srce	= blocks[0] + j * DCTSIZE;
				for (int i = 0; i < DCTSIZE; i++, dest++, srce++) {
					int	s = *srce + 128;
					*dest = s < 0 ? 0 : s > 255 ? 255 : s;
				}
				break;
			}
		}
	}
}

void JPG::ConvertBitmap(bitmap &bm, DCTBLOCK *fullscreen) {
	temp_array<DCTBLOCK>	blocks(nBlocksInMCUF);
//	DCTBLOCK	*blocks	= new DCTBLOCK[nBlocksInMCUF];
	DCTBLOCK	*sblock	= fullscreen;

	for (int y = 0; y < height; y += MCUheight) {
		for (int x = 0; x < width; x += MCUwidth) {
			for (int i = 0; i < nBlocksInMCUF; i++) {
				DCTBLOCK	&block	= blocks[i];
				QuantTable	&quant	= qt[component[BlockCompsF[i]].quant];
				for (int j = 0; j < DCTSIZE2; j++)
					block[ZAG[j]] = (*sblock)[j] * quant.val[j];
				jpeg_idct(block);
				++sblock;
			}
			ConvertBlock(bm.Block(x, y, MCUwidth, MCUheight), blocks);
		}
	}

//	delete[] blocks;
}

bool JPG::Decode(istream_ref file, bitmap &bm) {
	marker		= 0;
	VLCin		vlc(file, &marker);

	int			restart_count = restart_interval;
	restart_num	= 0;
	temp_array<DCTBLOCK>	blocks(nBlocksInMCUF);
//	DCTBLOCK *blocks	= new DCTBLOCK[nBlocksInMCU];
	for (int y = 0; y < height; y += MCUheight) {
		for (int x = 0; x < width; x += MCUwidth) {
			if (restart_interval) {
				if (restart_count == 0) {
					vlc.reset();	// Throw away any unused bits remaining in bit buffer
					GetRestart(file);
					restart_count = restart_interval;
				}
				restart_count--;
			}

			memset(blocks, 0, nBlocksInMCU * sizeof(DCTBLOCK));

			for (int i = 0; i < nBlocksInMCU; i++) {
				auto	&comp =component[BlockComps[i]];
				if (!BLKdecode(vlc, htAC[comp.huff & 0xf], htDC[comp.huff >> 4], qt[comp.quant], comp.lastDC, blocks[i]))
					return false;
			}

			for (int i = 0; i < nBlocksInMCU; i++)
				jpeg_idct(blocks[i]);

			ConvertBlock(bm.Block(x, y, MCUwidth, MCUheight), blocks);
		}
	}
//	delete[] blocks;
	vlc.reset();
	return true;
}

bool JPG::DecodeProgressive(istream_ref file, bitmap &bm, DCTBLOCK *fullscreen, int Ss, int Se, int A) {
	marker		= 0;
	VLCin		vlc(file, &marker);

	int			restart_count = restart_interval;
	short		EOBrun = 0;
	restart_num	= 0;

	bool	(*decode)(VLCin&, const HuffTable&, short&, DCTBLOCK&, int Ss, int Se, int A) = (A < 16)
		?	(Ss == 0 ? &JPG::BLKdecode_DC0 : &JPG::BLKdecode_AC0)
		:	(Ss == 0 ? &JPG::BLKdecode_DC1 : &JPG::BLKdecode_AC1);

	A		&= 15;

	if (nCompsInScan == 1) {
		Component		&comp	= component[BlockComps[0]];
		const HuffTable	&ht		= Ss == 0 ? htDC[comp.huff >> 4] : htAC[comp.huff & 0xf];
		short			*iparam	= Ss == 0 ? &comp.lastDC : &EOBrun;
		int				hSamp	= comp.hSamp;
		int				vSamp	= comp.vSamp;
		DCTBLOCK		*sblock	= fullscreen + comp.block;

		for (int y = 0; y < height; y += MCUheight) {
			DCTBLOCK	*sblock0 = sblock;
			for (int yb = 0; yb < vSamp && y + yb * DCTSIZE < height; yb++) {
				DCTBLOCK	*sblock1 = sblock0;
				for (int x = 0; x < width; x += MCUwidth) {
					DCTBLOCK	*sblock2 = sblock1;
					for (int xb = 0; xb < hSamp && x + xb * DCTSIZE < width; xb++) {
						if (restart_interval) {
							if (restart_count == 0) {
								vlc.reset();	// Throw away any unused bits remaining in bit buffer
								GetRestart(file);
								restart_count = restart_interval;
							}
							restart_count--;
						}

						if (!(*decode)(vlc, ht, *iparam, *sblock2, Ss, Se, A))
							return false;
						sblock2++;
					}
					sblock1 += nBlocksInMCUF;
				}
				sblock0 += hSamp;
			}
			sblock += div_round_up(width, MCUwidth) * nBlocksInMCUF;
		}

	} else {
		DCTBLOCK	*sblock	= fullscreen;
		for (int y = 0; y < height; y += MCUheight) {
			for (int x = 0; x < width; x += MCUwidth) {
				if (restart_interval) {
					if (restart_count == 0) {
						vlc.reset();	// Throw away any unused bits remaining in bit buffer
						GetRestart(file);
						restart_count = restart_interval;
					}
					restart_count--;
				}

				for (int i = 0; i < nCompsInScan; i++) {
					Component		&comp		= component[i];
					const HuffTable	&ht			= Ss == 0 ? htDC[comp.huff >> 4] : htAC[comp.huff & 0xf];
					short			*iparam		= Ss == 0 ? &comp.lastDC : &EOBrun;
					DCTBLOCK		*sblock1	= sblock + comp.block;

					for (int j = 0; j < comp.nSamp; j++, sblock1++)
						if (!(*decode)(vlc, ht, *iparam, *sblock1, Ss, Se, A))
							return false;
				}

				sblock += nBlocksInMCUF;
			}
		}
	}

	vlc.reset();
	return true;
}

//-----------------------------------------------------------------------------
//
//	READ JFIF
//
//-----------------------------------------------------------------------------

JPG::JPEG_MARKER JPG::GetMarker(istream_ref file) {
	int c = marker;
	while (c == 0) {
		do c = file.getc(); while (c != -1 && c != 0xFF);	// skip any non-FF bytes
		do c = file.getc(); while (c == 0xFF);	// skip any duplicate FFs
	};											// repeat if it was a stuffed FF/00
	marker = 0;
	return (JPEG_MARKER)c;
}

JPG::JPEG_MARKER JPG::GetMarker(istream_ref file, int &length) {
	auto	m = GetMarker(file);
	switch (m) {
		case M_SOI:
		case M_EOI:
		case M_RST0:
		case M_RST1:
		case M_RST2:
		case M_RST3:
		case M_RST4:
		case M_RST5:
		case M_RST6:
		case M_RST7:
		case M_TEM:
			length	= 0;
			break;
		default:
			length	= get16(file) - 2;
			break;
	}
	return m;
}

void JPG::GetSOI(istream_ref file)	{	// Process an SOI marker
	density_unit		= 0;
	X_density			= 1;
	Y_density			= 1;
	restart_interval	= 0;
}

struct jrgb8 {
	uint8 r, g, b;
	operator ISO_rgba() const { return ISO_rgba(r,g,b); }
};

struct Thumbnail_0x11 {
	uint8		w, h;			//Width, Height of embedded thumbnail
	jrgb8		palette[256];
	uint8		data[];
	void		Read(istream_ref file, ISO_ptr<bitmap> &bm) {
		if (w && h) {
			bm.Create();
			uint32	n	= w * h;
//			uint8	*s	= new uint8[n];
//			file.readbuff(s, n);
			temp_array<uint8>	s(file, n);
			copy(s, bm->Create(w, h));
			copy(palette, bm->CreateClut(256));
//			delete[] s;
		}
	}
};
struct Thumbnail_0x13 {
	uint8		w, h;			//Width, Height of embedded thumbnail
	jrgb8		data[];
	void		Read(istream_ref file, ISO_ptr<bitmap> &bm) {
		if (w && h) {
			bm.Create();
			uint32	n	= w * h;
//			jrgb8	*s	= new jrgb8[n];
//			file.readbuff(s, n * 3);
			temp_array<jrgb8>	s(file, n);
			copy(s, bm->Create(w, h));
//			delete[] s;
		}
	}
};

struct JFIF {
//	char		id[5];					//Always equals "JFIF" (with zero following) (0x4A46494600)
	uint8		ver_major, ver_minor;	//Version 2 First byte is major version (currently 0x01), Second byte is minor version (currently 0x02)
	uint8		density_units;			//Units for pixel density fields 0 - No units, aspect ratio only specified, 1 - Pixels per inch, 2 - Pixels per centimetre
	uint16be	X_density;				//Integer horizontal pixel density
	uint16be	Y_density;				//Integer vertical pixel density
};

struct JFXX {
//	char		id[5];	//Always equals "JFXX" (with zero following) (0x4A46585800)
	uint8		format;	//Thumbnail format: 0x10 - JPEG, 0x11 - 1 byte per pixel palettised, 0x13 - 3 byte per pixel RGB format
};

ISO_ptr<bitmap> JPG::GetAPP0(istream_ref file, int length, bool makebm) {		// Process an APP0 marker
	ISO_ptr<bitmap>	bm;
	char	id[5];

	if (file.read(id)) {
		if (str(id) == "JFIF") {
			JFIF	jfif	= file.get();
			if (jfif.ver_major == 1) {
				density_unit	= jfif.density_units;
				X_density		= jfif.X_density;
				Y_density		= jfif.Y_density;
				if (makebm && length > 5 + sizeof(JFIF) + sizeof(Thumbnail_0x13))
					file.get<Thumbnail_0x13>().Read(file, bm);
			}
		} else if (makebm && str(id) == "JFXX") {
			switch (file.getc()) {
				case 0x10:
					break;
				case 0x11:
					file.get<Thumbnail_0x11>().Read(file, bm);
					break;
				case 0x13: {
					file.get<Thumbnail_0x13>().Read(file, bm);
					break;
				}
			}
		}
	}
	return bm;
}

bool JPG::GetDHT(istream_ref file, int length)	{	// Process a DHT marker
	while (length > 0) {
		int			index	= file.getc();
		if (index >= 0x20 || (index & 15) >= NUM_HUFF_TBLS)
			return false;
		HuffTable	&huff	= index & 0x10 ? htAC[index - 0x10] : htDC[index];
		int			count	= 0;

		huff.bits[0] = 0;
		for (int i = 1; i <= 16; i++)
			count += (huff.bits[i] = file.getc());

		file.readbuff(huff.val, count);
		huff.FixDecode();
		length -= 1 + 16 + count;
	}
	return true;
}

bool JPG::GetDQT(istream_ref file, int length)	{	// Process a DQT marker
	while (length > 0) {
		int			n		= file.getc();
		bool		prec	= n >= 16;
		if ((n & 15) >= NUM_QUANT_TBLS)
			return false;

		QuantTable	&quant	= qt[n & 15];

		for (int i = 0; i < 64; i++)
			quant.val[i] = prec ? get16(file) : file.getc();

		length -= (prec ? 128 : 64) + 1;
	}
	return true;
}

void JPG::GetDRI(istream_ref file, int length) {	// Process a DRI marker
	restart_interval = get16(file);
}

bool JPG::GetSOF(istream_ref file, int length) {	// Process a SOFn marker
	uint8	*blockcomp	= BlockCompsF;

	MCUwidth		= 0;
	MCUheight		= 0;
	nBlocksInMCUF	= 0;

	precision	= file.getc();
	height		= get16(file);
	width		= get16(file);
	nComponents	= file.getc();

	if (nComponents > MAX_COMPS_IN_SCAN)
		return false;

	for (int i = 0; i < nComponents; i++) {
		Component	&comp	= component[i];
		uint8		id		= file.getc();
		uint8		c		= file.getc();
		comp.set(id, c >> 4, c & 15, file.getc());
		comp.block = blockcomp - BlockCompsF;
		comp.lastDC = 0;
		for (int j = 0; j < comp.nSamp; j++)
			*blockcomp++ = i;

		nBlocksInMCUF += comp.nSamp;
		if (MCUwidth  < comp.hSamp)
			MCUwidth  = comp.hSamp;
		if (MCUheight < comp.vSamp)
			MCUheight = comp.vSamp;
	}

	MCUwidth	*= 8;
	MCUheight	*= 8;
	return true;
}

bool JPG::GetSOS(istream_ref file, int length) {	// Process a SOS marker
	uint8	*blockcomp	= BlockComps;

	nBlocksInMCU	= 0;
	nCompsInScan	= file.getc();	// Number of components

	if (nCompsInScan > MAX_COMPS_IN_SCAN)
		return false;

	for (int i = 0; i < nCompsInScan; i++) {
		int			cc = file.getc();
		int			ci = 0;
		while (ci < nComponents && cc != component[ci].id)
			ci++;
		if (ci == nComponents)
			return false;

		Component	&comp	= component[ci];
		comp.huff			= file.getc();

		for (int j = 0; j < comp.nSamp; j++)
			*blockcomp++ = ci;

		nBlocksInMCU += comp.nSamp;
	}

	return true;
}

bool JPG::ReadBitmap(istream_ref file, bitmap &bm, bool thumbnail) {
	DCTBLOCK	*fullscreen	= 0;
	bool		progressive	= false;
	bool		got			= false;

	if (file.getc() != 0xff || file.getc() != M_SOI)
		return false;

	GetSOI(file);

	for (;;) {
		int			length;
		JPEG_MARKER	m	= GetMarker(file, length);
		streamptr	end	= file.tell() + length;

		switch (m) {
			case M_SOI:
				GetSOI(file);
				continue;

			case M_EOI:
				if (got) {
					if (progressive) {
						ConvertBitmap(bm, fullscreen);
						delete[] fullscreen;
					}
					bm.Crop(0, 0, width, height);
				}
				return got;

			case M_APP0:
				if (thumbnail)
					return GetAPP0(file, length, true);
				GetAPP0(file, length, false);
				break;

			case M_SOS:
				if (!GetSOS(file, length))
					return got;

				if (progressive) {
					int	Ss	= file.getc();
					int	Se	= file.getc();
					int	A	= file.getc();
					file.seek(end);
					if (!DecodeProgressive(file, bm, fullscreen, Ss, Se, A))
						return got;

				} else {
					file.seek(end);
					if (!Decode(file, bm))
						return got;
				}
				continue;

			case M_SOF2:
				progressive = true;
			case M_SOF0:
			case M_SOF1:
				if (!GetSOF(file, length))
					return got;

				bm.Create(align(width, MCUwidth), align(height, MCUheight));
				got = true;
				if (progressive) {
					int	total_blocks = nBlocksInMCUF * div_round_up(width, MCUwidth) * div_round_up(height, MCUheight);
					fullscreen = new DCTBLOCK[total_blocks];
					memset(fullscreen, 0, total_blocks * sizeof(DCTBLOCK));
				}
				break;

			case M_DHT:
				if (!GetDHT(file, length))
					return got;
				break;

			case M_DQT:
				if (!GetDQT(file, length))
					return got;
				break;

			case M_DRI:
				GetDRI(file, length);
				break;

			case M_DHP:			//ignore
			case M_EXP:
			case M_DNL:
			case M_APP1:
			case M_APP2:
			case M_APP3:
			case M_APP4:
			case M_APP5:
			case M_APP6:
			case M_APP7:
			case M_APP8:
			case M_APP9:
			case M_APP10:
			case M_APP11:
			case M_APP12:
			case M_APP13:
			case M_APP14:
			case M_APP15:
			case M_JPG0:
			case M_JPG13:
			case M_COM:
				break;

			default:			//errors
				return got;
		}
		file.seek(end);
	}
}

//-----------------------------------------------------------------------------
//
//	ENCODE
//
//-----------------------------------------------------------------------------

void JPG::MCUencode(VLCout &vlc, DCTBLOCK *block) {
	for (int i = 0; i < nBlocksInMCU; i++, block++) {
		short		zblock[DCTSIZE2];
		Component	&comp	= component[BlockComps[i]];
		HuffTable	&AC		= htAC[comp.huff & 0xf];
		HuffTable	&DC		= htDC[comp.huff >> 4 ];
		QuantTable	&quant	= qt[comp.quant];

		// Quantise and zag
		for (int j = 0; j < DCTSIZE2; j++) {
			int	q		= quant.val[j];
			zblock[j]	= (block[0][ZAG[j]] + q / 2) / q;
		}

		int	t	= zblock[0] - comp.lastDC;
		int	t2	= t;
		comp.lastDC = zblock[0];

		// Encode the DC coefficient difference
		if (t < 0) {
			t = -t;
			t2--;								// For a negative input, want t2 = bitwise complement of abs(input)
		}
		int	n = 0;
		while (t) {								// Find the number of bits needed for the magnitude of the coefficient
			n++;
			t >>= 1;
		}
		DC.Encode(vlc, n);						// Emit the Huffman-coded symbol for the number of bits
		if (n)
			vlc.put(t2, n);						// Emit that number of bits of the value

		// Encode the AC coefficients
		int	r = 0;
		for (int k = 1; k < DCTSIZE2; k++) {
			if (t = zblock[k]) {
				while (r > 15) {				// if run length > 15, must emit special run-length-16 codes (0xF0)
					AC.Encode(vlc, 0xF0);
					r -= 16;
				}
				if ((t2 = t) < 0) {
					t = -t;
					t2--;
				}
				for (n = 1; t >>= 1; n++);		// Find the number of bits needed for the magnitude of the coefficient (at least 1)
				AC.Encode(vlc, (r << 4) + n);	// Emit Huffman symbol for run length / number of bits
				vlc.put(t2, n);					// Emit that number of bits of the value
				r = 0;
			} else
				r++;
		}

		// If the last coef(s) were zero, emit an end-of-block code
		if (r > 0)
			AC.Encode(vlc,0);
	}
}

void JPG::PutRestart(ostream_ref file) {
	PutMarker(file, JPEG_MARKER(M_RST0 + restart_num));

	// Re-initialize DC predictions to 0
	for (int i = 0; i < nComponents; i++)
		component[i].lastDC = 0;

	// Update restart state
	restart_num	= (restart_num + 1) & 7;
}

void JPG::Encode(ostream_ref file, bitmap &bm) {
	int			restart_count	= restart_interval;
//	DCTBLOCK	*blocks			= new DCTBLOCK[nBlocksInMCU];
	temp_array<int8>		compdata_data(nCompsInScan * MCUwidth * MCUheight);
	temp_array<DCTBLOCK>	blocks(nBlocksInMCU);
	JPGout		out(file);
	VLCout		vlc(out);

	int8		*compdata[4];
	for (int i = 0; i < nCompsInScan; i++)
		compdata[i] = compdata_data.begin() + i * MCUwidth * MCUheight;
	restart_num	= 0;

	for (int y = 0; y < height; y += MCUheight) {
		for (int x = 0; x < width; x += MCUwidth) {
			if (restart_interval) {
				if (restart_count == 0) {
					vlc.flush(0xff);
					PutRestart(file);
					restart_count = restart_interval;
				}
				restart_count--;
			}

			for (int i = 0; i < nCompsInScan; i++)
				memset(compdata[i], 0, MCUwidth * MCUheight);

			for (int j = 0; j < MCUheight; j++) {
				int			d	= j * MCUwidth;
				if (y + j < height) {
					ISO_rgba	*s	= bm.ScanLine(y + j) + x;
					int			w	= min(MCUwidth, width - x);
					if (bm.IsIntensity()) {
						for (int i = 0; i < w; i++)
							compdata[0][d++] = (*s++).r - 128;
					} else {
						for (int i = 0; i < w; i++, d++) {
							ISO_rgba	c = *s++;
							switch (nCompsInScan) {
								case 4:
									compdata[3][d] = c.a - 128;													// alpha-128
								case 3:
									compdata[2][d] = int( 0.50000 * c.r - 0.41869 * c.g - 0.08131 * c.b);		// Cr
									compdata[1][d] = int(-0.16874 * c.r - 0.33126 * c.g + 0.50000 * c.b);		// Cb
								case 1:
									compdata[0][d] = int( 0.29900 * c.r + 0.58700 * c.g + 0.11400 * c.b)-128;	// Y-128
							}
						}
					}
				}
			}

			DCTELEM		*block = *blocks;
			for (int i = 0; i < nCompsInScan; i++) {
				Component	&comp	= component[i];
				int8		*cd		= compdata[comp.id-1];
				for (int j = 0; j < comp.nSamp; j++) {
					int8 *cdp = cd + (j % comp.vSamp) * DCTSIZE + (j / comp.vSamp) * DCTSIZE * MCUwidth;
					if (comp.hSamp * DCTSIZE == MCUwidth && comp.vSamp * DCTSIZE == MCUheight) {
						for (int y = 0; y < DCTSIZE; y++, cdp += MCUwidth, block += DCTSIZE)
							for (int x = 0; x < DCTSIZE; x++)
								block[x] = cdp[x];
					} else {
						for (int y = 0; y < DCTSIZE; y++, cdp += MCUwidth * 2, block += DCTSIZE)
							for (int x = 0; x < DCTSIZE; x++)
								block[x] = (cdp[x * 2] + cdp[x * 2+1] + cdp[x * 2 + MCUwidth] + cdp[x * 2 + MCUwidth + 1] + 2) / 4;
					}
				}
			}

			for (int i = 0; i < nBlocksInMCU; i++)
				jpeg_fdct(blocks[i]);

			MCUencode(vlc, blocks);
		}
	}

	vlc.flush(0xff);

//	delete[] blocks;
//	for (int i = 0; i < nCompsInScan; i++)
//		delete[] compdata[i];
}

//-----------------------------------------------------------------------------
//
//	WRITE JFIF
//
//-----------------------------------------------------------------------------

void JPG::PutDHT(ostream_ref file, int index, bool ac) {
	HuffTable* huff = ac ? &htAC[index] : &htDC[index];
	int	length = 0;

	for (int i = 1; i <= 16; i++)
		length += huff->bits[i];

	PutMarker(file, M_DHT);
	put16(file, length + 2 + 1 + 16);
	file.putc(index | (ac<<4));

	file.writebuff(huff->bits+1, 16);
	file.writebuff(huff->val, length);

	huff->FixEncode();
}

bool JPG::PutDQT(ostream_ref file, int index) { // Emit a DQT marker - Returns the precision used (0 = 8bits, 1 = 16bits) for baseline checking
	short	*data	= qt[index].val;
	bool	prec	= false;

	for (int i = 0; i < DCTSIZE2; i++) {
		if (data[i] > 255)
			prec = true;
	}

	PutMarker(file, M_DQT);
	put16(file, prec ? DCTSIZE2 * 2 + 1 + 2 : DCTSIZE2 + 1 + 2);
	file.putc(index + (prec << 4));

	for (int i = 0; i < DCTSIZE2; i++) {
		if (prec)
			put16(file, data[i]);
		else
			file.putc(data[i]);
	}
	return prec;
}

void JPG::PutDRI(ostream_ref file) {
	PutMarker(file, M_DRI);
	put16(file, 4);
	put16(file, restart_interval);
}

void JPG::PutSOF(ostream_ref file, JPEG_MARKER code) {
	PutMarker(file, code);
	put16(file, 3 * nComponents + 2 + 5 + 1);

	file.putc(precision);
	put16(file, height);
	put16(file, width);

	file.putc(nComponents);

	for (int i = 0; i < nComponents; i++) {
		Component	&comp	= component[i];
		file.putc(comp.id);
		file.putc((comp.hSamp << 4) | comp.vSamp);
		file.putc(comp.quant);
		comp.lastDC = 0;
	}
}

void JPG::PutSOS(ostream_ref file) {
	PutMarker(file, M_SOS);
	put16(file, 2 * nCompsInScan + 2 + 1 + 3);
	file.putc(nCompsInScan);

	nBlocksInMCU	= 0;
	MCUwidth		= 0;
	MCUheight		= 0;

	for (int i = 0; i < nCompsInScan; i++) {
		Component	&comp	= component[i];
		file.putc(comp.id);
		file.putc(comp.huff);
		if (MCUwidth  < comp.hSamp)
			MCUwidth  = comp.hSamp;
		if (MCUheight < comp.vSamp)
			MCUheight = comp.vSamp;
		for (int j = 0; j < comp.nSamp; j++)
			BlockComps[nBlocksInMCU++] = i;
	}
	MCUwidth	*= 8;
	MCUheight	*= 8;

	file.putc(0);			// Spectral selection start
	file.putc(DCTSIZE2-1);	// Spectral selection end
	file.putc(0);			// Successive approximation
}

bool JPG::WriteBitmap(ostream_ref file, bitmap &bm) {
	char	in_use[NUM_QUANT_TBLS];
	bool	is_baseline, prec;

	width	= bm.Width();
	height	= bm.Height();

	if (bm.IsIntensity()) {
		nComponents = 1;
		component[0].hSamp = component[0].vSamp = 1;
	} else if (bm.HasAlpha()) {
//		nComponents = 4;
	}

	PutMarker(file, M_SOI);

	clear(in_use);
	for (int i = 0; i < nComponents; i++)
		in_use[component[i].quant] = 1;

	prec = false;
	for (int i = 0; i < NUM_QUANT_TBLS; i++) {
		if (in_use[i])
			prec |= PutDQT(file, i);
	}

	if (is_baseline = !prec) {
		for (int i = 0; i < nComponents; i++) {
			if (component[i].huff >= 0x20 || (component[i].huff&0xf) >= 2)
				is_baseline = false;
		}
	}

	PutSOF(file, is_baseline ? M_SOF0 : M_SOF1);

	nCompsInScan	= nComponents;

	clear(in_use);
	for (int i = 0; i < nComponents; i++) {
		in_use[component[i].huff&0xf] |= 1;
		in_use[component[i].huff>>4 ] |= 2;
	}
	for (int i = 0; i < NUM_HUFF_TBLS; i++) {
		if (in_use[i] & 1)
			PutDHT(file, i, false);
		if (in_use[i] & 2)
			PutDHT(file, i, true);
	}

	if (restart_interval)
		PutDRI(file);

	PutSOS(file);
	Encode(file, bm);
	PutMarker(file, M_EOI);
	return true;
}

void JPG::SetQuantTable(QuantTable &qtd, const QuantTable &qts, int scale_factor, bool force_baseline) {
	int	max = force_baseline ? 255 : 32767;
	for (int i = 0; i < DCTSIZE2; i++) {
		int	 temp = (qts.val[i] * scale_factor + 50) / 100;
		if (temp <= 0)
			temp = 1;
		if (temp > max)
			temp = max;
		qtd.val[i] = temp;
	}
}

void JPG::SetQuality(int quality, bool force_baseline) {
	static const QuantTable std_luminance_quant_tbl = {{
		16,  11,  12,  14,  12,  10,  16,  14,
		13,  14,  18,  17,  16,  19,  24,  40,
		26,  24,  22,  22,  24,  49,  35,  37,
		29,  40,  58,  51,  61,  60,  57,  51,
		56,  55,  64,  72,  92,  78,  64,  68,
		87,  69,  55,  56,  80, 109,  81,  87,
		95,  98, 103, 104, 103,  62,  77, 113,
		121, 112, 100, 120,  92, 101, 103,  99
	}};
	static const QuantTable std_chrominance_quant_tbl = {{
		17,  18,  18,  24,  21,  24,  47,  26,
		26,  47,  99,  66,  56,  66,  99,  99,
		99,  99,  99,  99,  99,  99,  99,  99,
		99,  99,  99,  99,  99,  99,  99,  99,
		99,  99,  99,  99,  99,  99,  99,  99,
		99,  99,  99,  99,  99,  99,  99,  99,
		99,  99,  99,  99,  99,  99,  99,  99,
		99,  99,  99,  99,  99,  99,  99,  99
	}};

	// Convert user 0-100 rating to percentage scaling
	if (quality <= 0)
		quality = 1;
	if (quality > 100)
		quality = 100;

	quality = quality < 50 ? 5000 / quality : 200 - quality * 2;

	SetQuantTable(qt[0], std_luminance_quant_tbl,	quality, force_baseline);
	SetQuantTable(qt[1], std_chrominance_quant_tbl, quality, force_baseline);
}

void JPG::StandardHuffTables() {
	static const uint8 dc_luminance_bits[17]	= { 0, 0, 1, 5, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0 };
	static const uint8 dc_luminance_val[]		= { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };

	static const uint8 dc_chrominance_bits[17]	= { 0, 0, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0 };
	static const uint8 dc_chrominance_val[]		= { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };

	static const uint8 ac_luminance_bits[17]	= { 0, 0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 0x7d };
	static const uint8 ac_luminance_val[]		= {
		0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12,
		0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07,
		0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xa1, 0x08,
		0x23, 0x42, 0xb1, 0xc1, 0x15, 0x52, 0xd1, 0xf0,
		0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0a, 0x16,
		0x17, 0x18, 0x19, 0x1a, 0x25, 0x26, 0x27, 0x28,
		0x29, 0x2a, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
		0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
		0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
		0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
		0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,
		0x7a, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
		0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98,
		0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
		0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6,
		0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5,
		0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4,
		0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xe1, 0xe2,
		0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea,
		0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
		0xf9, 0xfa
	};

	static const uint8 ac_chrominance_bits[17]	= { 0, 0, 2, 1, 2, 4, 4, 3, 4, 7, 5, 4, 4, 0, 1, 2, 0x77 };
	static const uint8 ac_chrominance_val[]		= {
		0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21,
		0x31, 0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71,
		0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91,
		0xa1, 0xb1, 0xc1, 0x09, 0x23, 0x33, 0x52, 0xf0,
		0x15, 0x62, 0x72, 0xd1, 0x0a, 0x16, 0x24, 0x34,
		0xe1, 0x25, 0xf1, 0x17, 0x18, 0x19, 0x1a, 0x26,
		0x27, 0x28, 0x29, 0x2a, 0x35, 0x36, 0x37, 0x38,
		0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
		0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
		0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
		0x69, 0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
		0x79, 0x7a, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
		0x88, 0x89, 0x8a, 0x92, 0x93, 0x94, 0x95, 0x96,
		0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5,
		0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4,
		0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3,
		0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2,
		0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda,
		0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9,
		0xea, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
		0xf9, 0xfa
	};

	htDC[0].Set(dc_luminance_bits,		dc_luminance_val	);
	htAC[0].Set(ac_luminance_bits,		ac_luminance_val	);
	htDC[1].Set(dc_chrominance_bits,	dc_chrominance_val	);
	htAC[1].Set(ac_chrominance_bits,	ac_chrominance_val	);
}

JPG::JPG(int quality, bool force_baseline) {
	precision		= 8;
	nComponents		= 3;
	restart_interval= 0;

	component[0].set(1,2,2,0,0x00);	// luminance		(2x2 subsamples, quant 0, huff 0,0)
	component[1].set(2,1,1,1,0x11);	// chrominance		(1x1 subsamples, quant 1, huff 1,1)
	component[2].set(3,1,1,1,0x11);	// chrominance		(1x1 subsamples, quant 1, huff 1,1)
	component[3].set(4,1,1,0,0x00);	// alpha component	(1x1 subsamples, quant 0, huff 1,1)

	SetQuality(quality, force_baseline);
	StandardHuffTables();
}

//-----------------------------------------------------------------------------
//
//	STUBS
//
//-----------------------------------------------------------------------------

class JPGFileHandler : public BitmapFileHandler {
	const char*		GetExt()	override { return "jpg";	}

	int				Check(istream_ref file) override {
		file.seek(0);
		return file.getc() == 0xff && file.getc() == JPG::M_SOI ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}
	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		ISO_ptr<bitmap>	bm(id);
		if (JPG().ReadBitmap(file, *bm, false))
			return bm;
		return ISO_NULL;
	}
	bool			Write(ISO_ptr<void> p, ostream_ref file) override {
		ISO_ptr<bitmap> bm = ISO_conversion::convert<bitmap>(p);
		return bm && JPG(ISO::root("variables")["quality"].GetInt(75)).WriteBitmap(file, *bm);
	}
public:
	JPGFileHandler()		{ ISO::getdef<bitmap>(); }
} jpg;

class JPEGFileHandler : public JPGFileHandler {
	const char*		GetExt()				override { return "jpeg"; }
	const char*		GetMIME()				override { return "image/jpeg"; }
	const char*		GetDescription()		override { return 0; }
	int				Check(istream_ref file)	override { return CHECK_DEFINITE_NO; }
} jpeg;
