#ifndef	JPG_H
#define	JPG_H

#include "bitmap.h"
#include "stream.h"
#include "codec/vlc.h"

class JPG : iso::native {
	typedef	iso::uint16be		uint16be;
	typedef	iso::ostream_ref	ostream_ref;
	typedef	iso::istream_ref	istream_ref;
	typedef	iso::bitmap			bitmap;
	typedef	iso::tag			tag;

	enum {
		NUM_QUANT_TBLS		= 4,
		NUM_HUFF_TBLS		= 4,
		MAX_COMPS_IN_SCAN	= 4,	// JPEG limit on # of components in one scan
		MAX_BLOCKS_IN_MCU	= 10,	// JPEG limit on # of blocks in an MCU
		DCTSIZE				= 8,	// The basic DCT block is 8x8 samples
		DCTSIZE2			= DCTSIZE * DCTSIZE,
	};

	typedef int16	DCTELEM;
	typedef DCTELEM DCTBLOCK[DCTSIZE2];

	class JPGout {
		ostream_ref stream;
	public:
		JPGout(ostream_ref _stream) : stream(_stream) {}
		int		putc(int c) {
			stream.putc(c);
			if (c == 0xff)
				stream.putc(0);
			return c;
		}
	};
	class VLCout : public iso::vlc_out<uint32, true, JPGout> {
	public:
		VLCout(JPGout &_out) : iso::vlc_out<uint32, true, JPGout>(_out)	{}
	};

	class JPGin {
		istream_ref stream;
		uint8	*marker;
	public:
		JPGin(istream_ref stream, uint8 *marker) : stream(stream), marker(marker) {}
		int		getc() {
			if (!*marker) {
				int c = stream.getc();
				if (c == 0xff) {
					if (*marker = stream.getc())
						return EOF;
				}
	//			if (c != 0xff || !(marker = stream.getc()))
				return c;
			}
			return EOF;
		}
	};
	class VLCin : public iso::vlc_in<uint32, true, JPGin> {
		static int		sign_extend(int x, int s)	{
			int	b = (~x << 1) & (1 << s);
			return x - b + (b >> s);
		}
	public:
		VLCin(istream_ref in, uint8 *marker) : iso::vlc_in<uint32, true, JPGin>(JPGin(in, marker))	{}
		inline int		get_signed(int n)	{ return sign_extend(get(n), n);	}
	};

	struct HuffTable {
		uint8	bits[17];		// bits[k] = # of symbols with codes of length k bits; bits[0] is unused
		uint8	val[256];		// The symbols, in order of incr code length
		union {
			uint8	qval[256];
			short	qcode[256];
		};
		uint8	qlen[256];
		uint8	sum8;
		uint8	code8;

		void	FixDecode();
		int		Slow(VLCin &vlc)	const;
		int		Decode(VLCin &vlc)	const;

		void	FixEncode();
		void	Encode(VLCout &vlc, int v);

		void	Set(const uint8 *_bits, const uint8 *_val);
	};

	struct QuantTable {
		short	val[DCTSIZE2];
	};

	struct Component {
		uint8	id, block, hSamp, vSamp, nSamp;
		uint8	quant;
		uint8	huff;		//top 4 bits = DC table, bottom 4 bits = AC table
		short	lastDC;
		void	set(uint8 _id, uint8 _hSamp, uint8 _vSamp, uint8 _quant) {
			id		= _id;
			hSamp	= _hSamp;
			vSamp	= _vSamp;
			nSamp	= _hSamp * _vSamp;
			quant	= _quant;
		}
		void	set(uint8 _id, uint8 _hSamp, uint8 _vSamp, uint8 _quant, uint8 _huff) {
			set(_id, _hSamp, _vSamp, _quant);
			huff	= _huff;
		}
	};

public:
	enum JPEG_MARKER {
		M_SOF0	= 0xc0,
		M_SOF1	= 0xc1,
		M_SOF2	= 0xc2,
		M_SOF3	= 0xc3,

		M_SOF5	= 0xc5,
		M_SOF6	= 0xc6,
		M_SOF7	= 0xc7,

		M_JPG	= 0xc8,
		M_SOF9	= 0xc9,
		M_SOF10	= 0xca,
		M_SOF11	= 0xcb,

		M_SOF13	= 0xcd,
		M_SOF14	= 0xce,
		M_SOF15	= 0xcf,

		M_DHT	= 0xc4,

		M_DAC	= 0xcc,

		M_RST0	= 0xd0,
		M_RST1	= 0xd1,
		M_RST2	= 0xd2,
		M_RST3	= 0xd3,
		M_RST4	= 0xd4,
		M_RST5	= 0xd5,
		M_RST6	= 0xd6,
		M_RST7	= 0xd7,

		M_SOI	= 0xd8,
		M_EOI	= 0xd9,
		M_SOS	= 0xda,
		M_DQT	= 0xdb,
		M_DNL	= 0xdc,
		M_DRI	= 0xdd,
		M_DHP	= 0xde,
		M_EXP	= 0xdf,

		M_APP0	= 0xe0,
		M_APP1	= 0xe1,
		M_APP2	= 0xe2,
		M_APP3	= 0xe3,
		M_APP4	= 0xe4,
		M_APP5	= 0xe5,
		M_APP6	= 0xe6,
		M_APP7	= 0xe7,
		M_APP8	= 0xe8,
		M_APP9	= 0xe9,
		M_APP10	= 0xea,
		M_APP11	= 0xeb,
		M_APP12	= 0xec,
		M_APP13	= 0xed,
		M_APP14	= 0xee,
		M_APP15	= 0xef,

		M_JPG0	= 0xf0,
		M_JPG13	= 0xfd,
		M_COM	= 0xfe,

		M_TEM	= 0x01,
	};
private:
	static const uint8 ZAG[DCTSIZE2 + 16];

	uint16		width, height;
	uint8		MCUwidth, MCUheight;
	int			X_density, Y_density;

	uint8		marker;
	uint8		density_unit;
	uint8		restart_num;
	uint8		precision;
	uint8		nComponents, nCompsInScan;

	int			restart_interval;

	HuffTable	htDC[NUM_HUFF_TBLS];
	HuffTable	htAC[NUM_HUFF_TBLS];
	QuantTable	qt[NUM_QUANT_TBLS];
	Component	component[MAX_COMPS_IN_SCAN];

	uint8		BlockComps[MAX_BLOCKS_IN_MCU], BlockCompsF[MAX_BLOCKS_IN_MCU];
	uint8		nBlocksInMCU, nBlocksInMCUF;

public:
// Read

	JPEG_MARKER				GetMarker(istream_ref file);
	JPEG_MARKER				GetMarker(istream_ref file, int &length);
	void					GetSOI(istream_ref file);

	iso::ISO_ptr<bitmap>	GetAPP0(istream_ref file, int length, bool makebm);
	bool					GetDHT(istream_ref file, int length);
	bool					GetDQT(istream_ref file, int length);
	void					GetDRI(istream_ref file, int length);
	bool					GetSOF(istream_ref file, int length);
	bool					GetSOS(istream_ref file, int length);

//	bool					BLKdecode(VLCin &vlc,		Component &comp, DCTBLOCK &block);
	static bool				BLKdecode(VLCin &vlc, const HuffTable &AC, const HuffTable &DC, const QuantTable	&quant, short &lastDC, DCTBLOCK &block);

	static bool				BLKdecode_DC0(VLCin &vlc,	const HuffTable &ht, short &lastDC, DCTBLOCK &block, int Ss, int Se, int A);
	static bool				BLKdecode_DC1(VLCin &vlc,	const HuffTable &ht, short &lastDC, DCTBLOCK &block, int Ss, int Se, int A);
	static bool				BLKdecode_AC0(VLCin &vlc,	const HuffTable &ht, short &EOBrun, DCTBLOCK &block, int Ss, int Se, int A);
	static bool				BLKdecode_AC1(VLCin &vlc,	const HuffTable &ht, short &EOBrun, DCTBLOCK &block, int Ss, int Se, int A);
	void					GetRestart(istream_ref file);
	void					ConvertBlock(const iso::block<iso::ISO_rgba, 2> &block, DCTBLOCK *blocks);
	void					ConvertBitmap(bitmap &bm, DCTBLOCK *fullscreen);
	bool					Decode(istream_ref file, bitmap &bm);
	bool					DecodeProgressive(istream_ref file, bitmap &bm, DCTBLOCK *fullscreen, int Ss, int Se, int A);

// Write

	void					PutMarker(ostream_ref file, JPEG_MARKER mark)		{ file.putc(0xFF); file.putc(mark);			}
	bool					PutDQT(ostream_ref file, int index);
	void					PutSOF(ostream_ref file, JPEG_MARKER code);
	void					PutDHT(ostream_ref file, int index, bool ac);
	void					PutDRI(ostream_ref file);
	void					PutSOS(ostream_ref file);

	static void				SetQuantTable(QuantTable &qtd, const QuantTable &qts, int scale_factor, bool force_baseline);
	void					StandardHuffTables();

	void					MCUencode(VLCout &vlc, DCTBLOCK *block);
	void					PutRestart(ostream_ref file);
	void					Encode(ostream_ref file, bitmap &bm);

public:
	bool					ReadBitmap(istream_ref file, bitmap &bm, bool thumbnail = false);
	bool					WriteBitmap(ostream_ref file, bitmap &bm);
	void					SetQuality(int quality, bool force_baseline);

	JPG() : marker(0) {}
	JPG(int quality, bool force_baseline = false);
};

#endif	//JPG_H
