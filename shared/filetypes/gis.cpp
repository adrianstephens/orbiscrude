#include "iso/iso_files.h"
#include "iso/iso_convert.h"
#include "filetypes/bitmap/bitmap.h"
#include "base/array.h"
#include "base/block.h"
#include "base/algorithm.h"
#include "extra/geo.h"
#include "codec/fax3.h"
#include "vector_iso.h"
#include "vm.h"
#include "bin.h"

using namespace iso;

//googlemapsapi: AIzaSyBR9lHZc0ONHLGnbeCL-18aJzLmM09hrYo

//https://maps.googleapis.com/maps/api/staticmap?center=Brooklyn+Bridge,New+York,NY&zoom=13&size=600x300&maptype=roadmap&markers=color:blue%7Clabel:S%7C40.702147,-74.015794&markers=color:green%7Clabel:G%7C40.711614,-74.012318&markers=color:red%7Clabel:C%7C40.718217,-73.998284&key=AIzaSyBR9lHZc0ONHLGnbeCL-18aJzLmM09hrYo

/* -------------------------------------------------------------------- */
/*      Shape types (nSHPType)                                          */
/* -------------------------------------------------------------------- */
enum SHAPE_TYPE {
	SHPT_NULL			= 0,
	SHPT_POINT			= 1,
	SHPT_ARC			= 3,
	SHPT_POLYGON		= 5,
	SHPT_MULTIPOINT		= 8,
	SHPT_POINTZ			= 11,
	SHPT_ARCZ			= 13,
	SHPT_POLYGONZ		= 15,
	SHPT_MULTIPOINTZ	= 18,
	SHPT_POINTM			= 21,
	SHPT_ARCM			= 23,
	SHPT_POLYGONM		= 25,
	SHPT_MULTIPOINTM	= 28,
	SHPT_MULTIPATCH		= 31,
};

const char *GetName(SHAPE_TYPE t) {
	switch (t) {
		case SHPT_NULL:			return "NullShape";
		case SHPT_POINT:		return "Point";
		case SHPT_ARC:			return "Arc";
		case SHPT_POLYGON:		return "Polygon";
		case SHPT_MULTIPOINT:	return "MultiPoint";
		case SHPT_POINTZ:		return "PointZ";
		case SHPT_ARCZ:			return "ArcZ";
		case SHPT_POLYGONZ:		return "PolygonZ";
		case SHPT_MULTIPOINTZ:	return "MultiPointZ";
		case SHPT_POINTM:		return "PointM";
		case SHPT_ARCM:			return "ArcM";
		case SHPT_POLYGONM:		return "PolygonM";
		case SHPT_MULTIPOINTM:	return "MultiPointM";
		case SHPT_MULTIPATCH:	return "MultiPatch";
		default:				return "UnknownShapeType";
	}
}

/* -------------------------------------------------------------------- */
/*      Part types - everything but SHPT_MULTIPATCH just uses           */
/*      SHPP_RING.                                                      */
/* -------------------------------------------------------------------- */

enum PART_TYPE {
	SHPP_TRISTRIP		= 0,
	SHPP_TRIFAN			= 1,
	SHPP_OUTERRING		= 2,
	SHPP_INNERRING		= 3,
	SHPP_FIRSTRING		= 4,
	SHPP_RING			= 5,
};

const char *GetName(PART_TYPE t) {
	switch (t) {
		case SHPP_TRISTRIP:		return "TriangleStrip";
		case SHPP_TRIFAN:		return "TriangleFan";
		case SHPP_OUTERRING:	return "OuterRing";
		case SHPP_INNERRING:	return "InnerRing";
		case SHPP_FIRSTRING:	return "FirstRing";
		case SHPP_RING:			return "Ring";
		default:				return "UnknownPartType";
	}
}

struct SHPheader {
	enum {MAGIC = 0x0a270000, VERSION = 1000};
	struct bound	{ doublele	min, max; };
	struct boundxy	{ doublele	minx, miny, maxx, maxy; };

	struct bound_vals : bound { doublele vals[]; };

	uint32le			magic;
	uint32be			unused[5];
	uint32be			file_size;
	uint32le			version;
	uint32le			shape_type;
	packed<boundxy>		bound_xy;
	packed<bound>		boundz;
	packed<bound>		boundm;

	struct ShapeRecord {
		uint32be	id;
		uint32be	size;	//(size - 8) / 2
		uint32le	type;	//SHAPE_TYPE
	};

	// SHPT_POLYGON
	// SHPT_POLYGONZ
	// SHPT_POLYGONM
	// SHPT_ARC
	// SHPT_ARCZ
	// SHPT_ARCM
	// SHPT_MULTIPATCH
	struct PolygonRecord : ShapeRecord {
		packed<boundxy>	bounds;
		uint32	nparts;
		uint32	npoints;// @48
		// int part_start[nparts];
		// if SHPT_MULTIPATCH, PART_TYPE part_type[nparts];
		// double xy[npoints][2];
		// if z, {bound bz; double z[npoints]; }
		// if m, {bound bm; double m[npoints]; }
	};//52

	// SHPT_MULTIPOINT
	// SHPT_MULTIPOINTZ
	// SHPT_MULTIPOINTM
	struct MultiPointRecord : ShapeRecord {
		packed<boundxy>	bounds;
		uint32	npoints;// @44
		// double xy[][2];
		// if z, {bound bz; double z[]; }
		// if m, {bound bm; double m[]; }
	};//48

	// SHPT_POINT
	// SHPT_POINTZ
	// SHPT_POINTM
	struct PointRecord : ShapeRecord {
		packed<doublele>	x, y;
		//if z, double z
		//if m, double m;
	};//28

	//SHX
	struct IndexRecord {
		int32be		offset, size;
	};

	bool validate() const {
		return magic == MAGIC && version == VERSION;
	}
};

ISO_DEFUSERCOMPV(SHPheader::ShapeRecord, id, size, type);
ISO_DEFUSERCOMPBV(SHPheader::PolygonRecord,		SHPheader::ShapeRecord, bounds, nparts, npoints);
ISO_DEFUSERCOMPBV(SHPheader::MultiPointRecord,	SHPheader::ShapeRecord, bounds, npoints);
ISO_DEFUSERCOMPBV(SHPheader::PointRecord,		SHPheader::ShapeRecord, x, y);

struct SHP_Vector3_iterator {
	typedef random_access_iterator_t	iterator_category;
	typedef array_vec<double,3>		element, reference;
	const array_vec<double,2>	*xy;
	const double				*z;

	SHP_Vector3_iterator(const array_vec<double,2> *_xy, const double *_z) : xy(_xy), z(_z) {}
	SHP_Vector3_iterator&	operator++()					{ ++xy; ++z; return *this; }
	SHP_Vector3_iterator	operator+(size_t n)		const	{ return SHP_Vector3_iterator(xy + n, z + n); }
	intptr_t operator-(const SHP_Vector3_iterator &b) const	{ return xy - b.xy; }
	bool	operator==(const SHP_Vector3_iterator &b) const { return xy == b.xy; }
	bool	operator!=(const SHP_Vector3_iterator &b) const { return xy != b.xy; }

	element	operator*()	const { return element{xy->x, xy->y, *z}; }
};

struct SHP_Vector4_iterator {
	typedef random_access_iterator_t	iterator_category;
	typedef array_vec<double,4>		element, reference;
	const array_vec<double,2>	*xy;
	const double				*z;
	const double				*m;

	SHP_Vector4_iterator(const array_vec<double,2> *_xy, const double *_z, const double *_m) : xy(_xy), z(_z), m(_m) {}
	SHP_Vector4_iterator&	operator++()					{ ++xy; ++z; ++m; return *this; }
	SHP_Vector4_iterator	operator+(size_t n)		const	{ return SHP_Vector4_iterator(xy + n, z + n, m + n); }
	intptr_t operator-(const SHP_Vector4_iterator &b) const	{ return xy - b.xy; }
	bool	operator==(const SHP_Vector4_iterator &b) const { return xy == b.xy; }
	bool	operator!=(const SHP_Vector4_iterator &b) const { return xy != b.xy; }

	element	operator*()	const { return element{xy->x, xy->y, *z, *m}; }
};

struct PolygonRecord : SHPheader::PolygonRecord {
	size_t			true_size() const { return size * 2 + 8; }
	size_t			part_size() const { return sizeof(uint32) * (1 + int(type == SHPT_MULTIPATCH)); }
	bool			has_z()		const { return type == SHPT_POLYGONZ || type == SHPT_ARCZ || type == SHPT_MULTIPATCH; }

	const array_vec<double,2>* get_xy() const {
		size_t	offset		= sizeof(SHPheader::PolygonRecord) + nparts * part_size();
		return (const array_vec<double,2>*)((const uint8*)this + offset);
	}

	const SHPheader::bound_vals*	get_z() const {
		if (!has_z())
			return 0;
		size_t	point_size	= sizeof(double) * 2;
		size_t	offset		= sizeof(SHPheader::PolygonRecord) + nparts * part_size() + npoints * point_size;
		return (const SHPheader::bound_vals*)((const uint8*)this + offset);
	}
	const SHPheader::bound_vals*	get_m(bool z) const {
		if (z || type == SHPT_POLYGONM || type == SHPT_ARCM) {
			size_t	point_size	= sizeof(double) * (2 + int(z));
			size_t	offset		= sizeof(SHPheader::PolygonRecord) + (z ? sizeof(SHPheader::bound) : 0) + nparts * part_size() + npoints * point_size;
			if (offset < true_size())
				return (const SHPheader::bound_vals*)((const uint8*)this + offset);
		}
		return 0;
	}
	const SHPheader::bound*	get_m() const {
		return get_m(has_z());
	}

	ISO::Browser2	extent() const	{
		auto	z	= get_z();
		auto	m	= get_m(!!z);
		SHPheader::boundxy	ext	= bounds;
		if (z) {
			if (m) {
				double4	a, b;
				a = {ext.minx, ext.miny, z->min, m->min};
				b = {ext.maxx, ext.maxy, z->max, m->max};
				return ISO::MakeBrowser(make_interval(a, b));
			} else {
				double3	a, b;
				a = {ext.minx, ext.miny, z->min};
				b = {ext.maxx, ext.maxy, z->max};
				return ISO::MakeBrowser(make_interval(a, b));
			}
		} else {
			if (m) {
				double3	a, b;
				a = {ext.minx, ext.miny, m->min};
				b = {ext.maxx, ext.maxy, m->max};
				return ISO::MakeBrowser(make_interval(a, b));
			} else {
				double2	a, b;
				a = {ext.minx, ext.miny};
				b = {ext.maxx, ext.maxy};
				return ISO::MakeBrowser(make_interval(a, b));
			}
		}
	}

	ISO::Browser2	points() const	{
		auto	z	= get_z();
		auto	m	= get_m(!!z);
		if (z) {
			if (m) {
				return ISO::MakeBrowser(make_range_n(SHP_Vector4_iterator(get_xy(), z->vals, m->vals), npoints));
			} else {
				return ISO::MakeBrowser(make_range_n(SHP_Vector3_iterator(get_xy(), z->vals), npoints));
			}
		} else {
			if (m) {
				return ISO::MakeBrowser(make_range_n(SHP_Vector3_iterator(get_xy(), m->vals), npoints));
			} else {
				return ISO::MakeBrowser(make_array_unspec(get_xy(), npoints));
			}
		}
	}
};
ISO_DEFUSERCOMPV(PolygonRecord, id, extent, points);

struct SHP : mapped_file, ISO::VirtualDefaults {
	dynamic_array<SHPheader::IndexRecord>	indices;

	SHP(const char *fn)	: mapped_file(fn) {}

	memory_block	operator[](int i)	{ return slice(indices[i].offset * 2, indices[i].size * 2 + 8); }
	uint32			Count()				{ return indices.size32(); }
	ISO::Browser2	Index(int i) {
		memory_block	block = operator[](i);
		SHPheader::ShapeRecord	*rec = block;
		switch (rec->type) {
			case SHPT_POLYGON:
			case SHPT_POLYGONZ:
			case SHPT_POLYGONM:
			case SHPT_ARC:
			case SHPT_ARCZ:
			case SHPT_ARCM:
			case SHPT_MULTIPATCH: {
				PolygonRecord	*rec2 = block;
				return ISO::MakeBrowser(*rec2);
			}
			case SHPT_MULTIPOINT:
			case SHPT_MULTIPOINTZ:
			case SHPT_MULTIPOINTM: {
				SHPheader::MultiPointRecord	*rec2 = block;
				return ISO::MakeBrowser(*rec2);
			}
			case SHPT_POINT:
			case SHPT_POINTZ:
			case SHPT_POINTM: {
				SHPheader::PointRecord		*rec2 = block;
				return ISO::MakeBrowser(*rec2);
			}
		}
		return ISO::MakeBrowser(block);
	}
};

ISO_DEFUSERVIRT(SHP);

struct ISO_zipper : ISO::VirtualDefaults {
	ISO_ptr<void>	a;
	ISO_ptr<void>	b;
	uint32			Count()			{ return ISO::Browser(a).Count(); }
	ISO::Browser2	Index(int i)	{
		return ISO::MakeBrowser(ISO::combine(0, ISO::Browser(a)[i], ISO::Browser(b)[i]));
	}
};

ISO_DEFUSERVIRT(ISO_zipper);

class SHPFileHandler : public FileHandler {

	const char*		GetExt() override { return "shp";	}
	const char*		GetDescription() override { return "Shape file"; }
	int				Check(istream_ref file) override {
		file.seek(0);
		SHPheader	header;
		return file.read(header) && header.validate() ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}
	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		SHPheader	header;
		if (file.read(header) && header.validate()) {
		}
		return ISO_NULL;
	}
	ISO_ptr<void>	ReadWithFilename(tag id, const filename &fn) override {
		filename	fnx = filename(fn).set_ext("shx");
		filename	dbf = filename(fn).set_ext("dbf");

		if (fnx.exists()) {
			FileInput	filex(fnx);
			SHPheader	header;
			if (filex.read(header) && header.validate()) {
				ISO_ptr<SHP>	p(id, fn);
				p->indices.read(filex, (header.file_size * 2 - sizeof(header)) / sizeof(SHPheader::IndexRecord));

				if (dbf.exists()) {
					ISO_ptr<ISO_zipper>	p2(id);
					p2->a = p;
					p2->b = FileHandler::Read(0, dbf);
					return p2;
				}
				return p;
			}
		}
		FileInput	file(fn);
		SHPheader	header;
		if (file.read(header) && header.validate()) {
		}
		return ISO_NULL;
	}
} shp;

//-----------------------------------------------------------------------------
//	raster grid
//-----------------------------------------------------------------------------

struct AIGInfo {
	static const int NO_DATA = -2147483647;
	enum TYPE {
		TYPE_INT		= 1,
		TYPE_FLOAT		= 2,
	};
	struct Tile {
		dynamic_array<SHPheader::IndexRecord>	indices;
		bool ReadIndex(const char *pszCoverName, const char *pszBasename);
	};
	struct Stats {
		double		min, max, mean, stddev;
	};

	dynamic_array<Tile> tiles;

	TYPE		cell_type;
	bool		compressed;

	int			nBlockXSize, nBlockYSize, nBlocksPerRow, nBlocksPerColumn;
	int			nTileXSize, nTileYSize, nTilesPerRow, nTilesPerColumn;

	SHPheader::boundxy		bound_xy;
	array_vec<double,2>	cell_size;

	int			nPixels, nLines;
	Stats		stats;

	bool ReadHeader(const char * pszCoverName);
	bool ReadBounds(const char * pszCoverName);
	bool ReadStatistics(const char * pszCoverName);
} ;



/************************************************************************/
/*                          AIGProcessBlock()                           */
/*                                                                      */
/*      Process a block using ``D7'', ``E0'' or ``DF'' compression.     */
/************************************************************************/

static
bool AIGProcessBlock(uint8 *pabyCur, int data_size, int nMin, int nMagic, int block_size, int * panData) {
	int	nPixels = 0;

	while (nPixels < block_size && data_size > 0) {
		int	nMarker = *(pabyCur++);

		data_size--;

		/* -------------------------------------------------------------------- */
		/*      Repeat data - four byte data block (0xE0)                       */
		/* -------------------------------------------------------------------- */
		if (nMagic == 0xE0) {
			int	nValue;

			if (nMarker + nPixels > block_size) {
				return false;
			}

			if (data_size < 4) {
				return false;
			}

			nValue = *(int32be*)pabyCur;
			pabyCur += 4;
			data_size -= 4;

			nValue = nValue + nMin;
			for (int i = 0; i < nMarker; i++)
				panData[nPixels++] = nValue;
		}

		/* -------------------------------------------------------------------- */
		/*      Repeat data - two byte data block (0xF0)                        */
		/* -------------------------------------------------------------------- */
		else if (nMagic == 0xF0) {
			int	nValue;

			if (nMarker + nPixels > block_size) {
				return false;
			}

			if (data_size < 2) {
				return false;
			}

			nValue = (pabyCur[0] * 256 + pabyCur[1]) + nMin;
			pabyCur += 2;
			data_size -= 2;

			for (int i = 0; i < nMarker; i++)
				panData[nPixels++] = nValue;
		}

		/* -------------------------------------------------------------------- */
		/*      Repeat data - one byte data block (0xFC)                        */
		/* -------------------------------------------------------------------- */
		else if (nMagic == 0xFC || nMagic == 0xF8) {
			int	nValue;

			if (nMarker + nPixels > block_size) {
				return false;
			}

			if (data_size < 1) {
				return false;
			}

			nValue = *(pabyCur++) + nMin;
			data_size--;

			for (int i = 0; i < nMarker; i++)
				panData[nPixels++] = nValue;
		}

		/* -------------------------------------------------------------------- */
		/*      Repeat data - no actual data, just assign minimum (0xDF)        */
		/* -------------------------------------------------------------------- */
		else if (nMagic == 0xDF && nMarker < 128) {
			if (nMarker + nPixels > block_size)
				return false;

			for (int i = 0; i < nMarker; i++)
				panData[nPixels++] = nMin;
		}

		/* -------------------------------------------------------------------- */
		/*      Literal data (0xD7): 8bit values.                               */
		/* -------------------------------------------------------------------- */
		else if (nMagic == 0xD7 && nMarker < 128) {
			if (nMarker + nPixels > block_size)
				return false;

			while (nMarker > 0 && data_size > 0) {
				panData[nPixels++] = *(pabyCur++) + nMin;
				nMarker--;
				data_size--;
			}
		}

		/* -------------------------------------------------------------------- */
		/*      Literal data (0xCF): 16 bit values.                             */
		/* -------------------------------------------------------------------- */
		else if (nMagic == 0xCF && nMarker < 128) {
			int	nValue;

			if (nMarker + nPixels > block_size)
				return false;

			while (nMarker > 0 && data_size >= 2) {
				nValue = pabyCur[0] * 256 + pabyCur[1] + nMin;
				panData[nPixels++] = nValue;
				pabyCur += 2;

				nMarker--;
				data_size -= 2;
			}
		}

		/* -------------------------------------------------------------------- */
		/*      Nodata repeat                                                   */
		/* -------------------------------------------------------------------- */
		else if (nMarker > 128) {
			nMarker = 256 - nMarker;

			if (nMarker + nPixels > block_size)
				return false;

			while (nMarker > 0) {
				panData[nPixels++] = AIGInfo::NO_DATA;
				nMarker--;
			}
		}

		else {
			return false;
		}
	}

	if (nPixels < block_size || data_size < 0)
		return false;

	return true;
}

/************************************************************************/
/*                            AIGReadBlock()                            */
/*                                                                      */
/*      Read a single block of integer grid data.                       */
/************************************************************************/

template<typename D, typename S> inline bool copy_block(D *d, const S *s, size_t num, size_t src_size, D offset) {
	if (src_size < num * sizeof(S))
		return false;
	while (num--)
		*d++ = *s++ + offset;
	return true;
}

bool AIGReadBlock(istream_ref fp, uint32 nBlockOffset, int data_size, int nBlockXSize, int nBlockYSize, int *panData, int nCellType, bool compressed) {
	int	block_size = nBlockXSize * nBlockYSize;
	// If the block has zero size it is all dummies
	if (data_size == 0) {
		for (int i = 0; i < block_size; i++)
			panData[i] = AIGInfo::NO_DATA;
		return true;
	}

	// Read the block into memory
	if (data_size <= 0 || data_size > 65535 * 2)
		return false;

	malloc_block	mem(data_size + 2);
	uint8			*pabyRaw = mem;

	fp.seek(nBlockOffset);
	if (!check_readbuff(fp, pabyRaw, data_size + 2)) {
		memset(panData, 0, block_size * 4);
		return false;
	}

	//      Verify the block size
	if (data_size != (pabyRaw[0] * 256 + pabyRaw[1]) * 2) {
		memset(panData, 0, block_size * 4);
		return false;
	}

	// The first 2 bytes that give the block size are not included in data_size and have already been safely read
	uint8	*pabyCur = pabyRaw + 2;

	// Handle float files and uncompressed integer files directly
	if (nCellType == AIGInfo::TYPE_FLOAT)
		return copy_block((float*)panData, (floatle*)pabyCur, block_size, data_size, 0.f);

	if (nCellType == AIGInfo::TYPE_INT && !compressed)
		return copy_block(panData, (int32be*)pabyCur, block_size, data_size, 0);

	// Collect minimum value

	// Need at least 2 byte to read the nMinSize and the nMagic
	if (data_size < 2)
		return false;

	int	nMagic		= pabyCur[0];
	int	nMinSize	= pabyCur[1];
	pabyCur		+= 2;
	data_size	-= 2;

	// Need at least nMinSize bytes to read the nMin value
	if (data_size < nMinSize)
		return false;

	if (nMinSize > 4) {
		memset(panData, 0, block_size * 4);
		return false;
	}

	int 	nMin = 0;
	if (nMinSize == 4) {
		nMin	= *(int32be*)pabyCur;
		pabyCur += 4;
	} else {
		nMin = 0;
		for (int i = 0; i < nMinSize; i++)
			nMin = nMin * 256 + *pabyCur++;

		// If nMinSize = 0, then we might have only 4 bytes in pabyRaw don't try to read the 5th one then */
		if (nMinSize != 0 && pabyRaw[4] > 127) {
			if (nMinSize == 2)
				nMin = nMin - 65536;
			else if (nMinSize == 1)
				nMin = nMin - 256;
			else if (nMinSize == 3)
				nMin = nMin - 256 * 256 * 256;
		}
	}

	data_size -= nMinSize;

	switch (nMagic) {
		case 0x00:// const format
			for (int i = 0; i < block_size; i++)
				panData[i] = nMin;
			return true;

		case 0x08:// 8 bit raw format
			return copy_block(panData, pabyCur, block_size, data_size, nMin);

		case 0x04:// 4 bit raw format
			if (data_size < (block_size + 1) / 2)
				return false;
			for (int i = 0; i < block_size; i++)
				panData[i] = nMin + (i & 1 ? (*pabyCur++ & 0xf) : ((*pabyCur & 0xf0) >> 4));
			return true;

		case 0x01:// 1 bit raw format
			if (data_size < (block_size + 7) / 8)
				return false;
			for (int i = 0; i < block_size; i++)
				panData[i] = nMin + ((pabyCur[i >> 3] >> (i & 0x7)) & 1);
			return true;

		case 0x10:// 16 bit raw format
			return copy_block(panData, (int16be*)pabyCur, block_size, data_size, nMin);

		case 0x20:// 32 bit raw format
			return copy_block(panData, (int32be*)pabyCur, block_size, data_size, nMin);

		case 0xFF: {// CCITT compressed bitstream -> 1bit raw data
			bitmatrix_aligned_own<uint32>	dest(nBlockXSize, nBlockYSize);
			if (!FaxDecode(const_memory_block(pabyCur, data_size), dest))
				return false;

			uint8	*p = mem;
			for (int i = 0; i < nBlockYSize; i++) {
				for (int j = 0; j < nBlockXSize; j++)
					panData[i * nBlockXSize + j] = nMin + dest[i][j];
			}
			return true;
		}

		default:
			if (!AIGProcessBlock(pabyCur, data_size, nMin, nMagic, block_size, panData)) {
				for (int i = 0; i < block_size; i++)
					panData[i] = AIGInfo::NO_DATA;
				return false;
			}
			return true;
	}
}

template<typename T> bool AIGReadBlock(istream_ref fp, uint32 data_offset, int data_size, const block<T, 2> &b) {
	// If the block has zero size it is all dummies
	if (data_size == 0) {
		fill(b, AIGInfo::NO_DATA);
		return true;
	}

	fp.seek(data_offset);
	uint32	size = fp.get<uint16be>() * 2;
	if (size != data_size || data_size < b.template size<1>() * b.template size<2>() * sizeof(float)) {
		fill(b, 0);
		return false;
	}

	// Read the block into memory
	malloc_block	mem(fp, data_size);
	copy(make_block((floatbe*)mem, b.template size<1>(), b.template size<2>()), b);
	return true;
}

struct AIGheader {
	uint8		_[16];
	uint32be	nCellType;			//16, 4);
	uint32be	bCompressed;		//20, 4);
	uint8		_2[256-24];
	packed<array_vec<doublebe,2> >	cell_size;		//256, 16);
	uint8		_4[288-272];
	uint32be	nBlocksPerRow;		//288, 4);
	uint32be	nBlocksPerColumn;	//292, 4);
	uint32be	nBlockXSize;		//296, 4);
	uint32		_3;
	uint32be	nBlockYSize;		//304, 4);
};

bool AIGInfo::ReadHeader(const char * pszCoverName) {
	FileInput	fp(filename(pszCoverName).add_dir("hdr.adf"));

	AIGheader	header;
	if (!fp.read(header))
		return false;

	cell_type			= (TYPE)(int)header.nCellType;
	compressed			= !header.bCompressed;
	nBlocksPerRow		= header.nBlocksPerRow;
	nBlocksPerColumn	= header.nBlocksPerColumn;
	nBlockXSize			= header.nBlockXSize;
	nBlockYSize			= header.nBlockYSize;
	cell_size			= get(header.cell_size);

	return true;
}

// Read the dblbnd.adf file for the georeferenced bounds

bool AIGInfo::ReadBounds(const char * pszCoverName) {
	FileInput	fp(filename(pszCoverName).add_dir("dblbnd.adf"));

	doublebe	adfBound[4];
	if (!fp.read(adfBound))
		return false;

	bound_xy.minx = adfBound[0];
	bound_xy.miny = adfBound[1];
	bound_xy.maxx = adfBound[2];
	bound_xy.maxy = adfBound[3];
	return true;
}

//  Read the sta.adf file for the layer statistics

bool AIGInfo::ReadStatistics(const char * pszCoverName) {
	FileInput	fp(filename(pszCoverName).add_dir("sta.adf"));

	doublebe	adfStats[4];
	clear(adfStats);

	if (fp.readbuff(adfStats, sizeof(adfStats)) < 24)
		return false;

	stats.min		= adfStats[0];
	stats.max		= adfStats[1];
	stats.mean		= adfStats[2];
	stats.stddev	= adfStats[3];
	return true;
}


// Read the w001001x.adf file

bool AIGInfo::Tile::ReadIndex(const char *pszCoverName, const char *pszBasename) {

	FileInput	fp(filename(pszCoverName).add_dir(format_string("%sx.adf", pszBasename)));

	SHPheader	header;
	if (!fp.read(header) || header.magic != header.MAGIC || (header.unused[0] >> 16) != 0xffff)
		return false;

	return indices.read(fp, (header.file_size * 2 - sizeof(header)) / sizeof(SHPheader::IndexRecord));
}


ISO_DEFUSERCOMPV(SHPheader::boundxy, minx, miny, maxx, maxy);
ISO_DEFUSERENUMQV(AIGInfo::TYPE, TYPE_INT, TYPE_FLOAT);
ISO_DEFUSERCOMPV(AIGInfo::Stats, min, max, mean, stddev);
ISO_DEFUSERCOMPV(AIGInfo, cell_type, nBlockXSize, nBlockYSize, nBlocksPerRow, nBlocksPerColumn, bound_xy, cell_size, stats);

class ArcInfoGridFileHandler : public FileHandler {

	const char*		GetExt() override { return "adf";	}
	const char*		GetDescription() override { return "ArcInfo Grid file"; }

	ISO_ptr<void>	ReadWithFilename(tag id, const filename &fn) override {
		filename	dir	= fn.dir();

		ISO_ptr<AIGInfo>	p(id);
		if (!p->ReadHeader(dir)
		|| !p->ReadBounds(dir)
		|| !p->ReadStatistics(dir)
		)
			return ISO_NULL;

		AIGInfo::Tile	tile;
		tile.ReadIndex(dir, fn.name());

		uint32	width	= p->nBlockXSize * p->nBlocksPerRow;
		uint32	height	= p->nBlockYSize * p->nBlocksPerColumn;

		ISO_ptr<HDRbitmap64>	bm(id);
		bm->Create(width, height);
//		auto	pixels = make_auto_block<float>(width, height);

		FileInput	fp(fn);
		auto		*ix	= tile.indices.begin();
		for (int y = 0; y < p->nBlocksPerColumn; y++) {
			for (int x = 0; x < p->nBlocksPerRow; x++, ix++) {
				//auto	dest	= pixels.sub<1>(x * p->nBlockXSize, p->nBlockXSize).sub<2>(y * p->nBlockYSize, p->nBlockYSize);
				auto	dest	= bm->Block(x * p->nBlockXSize, y * p->nBlockYSize, p->nBlockXSize, p->nBlockYSize);
				AIGReadBlock(fp, ix->offset * 2, ix->size * 2, dest);
			}
		}

		return bm;
	}
} aig;
