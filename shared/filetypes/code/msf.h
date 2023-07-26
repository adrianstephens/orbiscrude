#ifndef MSF_H
#define MSF_H

#include "stream.h"
#include "base/strings.h"
#include "base/bits.h"
#include "base/array.h"
#include "base/algorithm.h"

//-----------------------------------------------------------------------------
// msf see "The Multistream File API" for more information
//-----------------------------------------------------------------------------

namespace MSF {
using namespace iso;

//-----------------------------------------------------------------------------
//	constants
//-----------------------------------------------------------------------------

enum EC {
	EC_OK,
	EC_OUT_OF_MEMORY,
	EC_NOT_FOUND,
	EC_FILE_SYSTEM,
	EC_FORMAT,
	EC_ACCESS_DENIED,
	EC_CORRUPT,
};

typedef uint16	SN;			// stream number
typedef uint32	UNSN;		// unified stream number

typedef uint16	PN16;		// page number			(small)
typedef uint16	SPN16;		// stream page number	(small)
typedef uint32	PN32;		// page number			(big)
typedef uint32	SPN32;		// stream page number	(big)

struct Params {
	static const uint32	max_page_size	= 0x1000;
	static const uint32	min_page_size	= 0x200;
	static const PN16	max_max_pn		= 0xffff;	// max no of pgs in any msf
	static const PN32	max_max_upn		= 0x100000;	// 2^20 pages

	static const SN		max_sn			= 0x1000;	// max no of streams in msf
	static const UNSN	max_unsn		= 0x10000;

	uint32		log2_page_size;
	uint32		fpm_maxbits;
	uint32		file_growth;
	PN32		max_pn;
	PN32		fpm_size;
	PN32		fpm_start;
	PN32		data_start;
};

static constexpr PN32	pnNil		= PN32(~0);
static constexpr SN		snNil		= SN(~0);

//-----------------------------------------------------------------------------
//	SI	stream info
//-----------------------------------------------------------------------------

struct SI_base {
	static const uint32 invalid = ~0u;
	uint32		cb; // length of stream, invalid if stream does not exist

	bool	isValid()				const { return cb != invalid; }
	SPN32	spnMax(uint32 lgcbPg)	const { return SPN32(ceil_pow2(cb, lgcbPg)); }
};

struct SI : SI_base, dynamic_array<PN32> {
	SI() { cb = invalid; }
	bool alloc(uint32 log_page) {
		resize(spnMax(log_page), pnNil);
		return true;
	}
	bool allocForCb(uint32 cb_, uint32 log_page) {
		cb		= cb_;
		return alloc(log_page);
	}
};

struct SI_PERSIST: SI_base {
	int32	dummy;
	void	operator=(const SI_base &si) { cb = si.cb; dummy = 0; }
};

//-----------------------------------------------------------------------------
//	FPM	free page map (in-memory version)
//-----------------------------------------------------------------------------

struct FPM : dynamic_bitarray<uint64> {
	static const uint32 cpnBigReserved = 3;// first three pages in a big MSF are the header and two FPMs

	uint32		max_size	= 0;
	uint32		block		= 0;
	PN32		last		= 0;		// last allocated
	bool		fill		= false;	// keep track of the last fill type, as any appending we do to the array needs to be this type

	void init(uint32 _max_size, uint32 _block) {
		max_size	= _max_size;
		block		= _block;
		last		= 0;
		fill		= false;
	}

	bool add(const FPM& fpm) {
		max_size = max(max_size, fpm.max_size);
		uint32    iwMacThis	= size32();
		uint32    iwMacThat	= fpm.size32();

		if (auto x = max(iwMacThis, iwMacThat)) {
			resize(x);

			// handle all of the entries in fpm that are not in this one, (i.e. use our "virtual" fill
			grow(fpm.size32(), fill);
			dynamic_bitarray<uint64>::operator+=(fpm);

			// handle all of the entries in this one that are not actually in fpm (i.e. use their "virtual" setting in fpm.fill).
			if (fpm.fill && iwMacThis > iwMacThat)
				slice(iwMacThat, iwMacThis - iwMacThat).set_all();

			// now do all of the virtual entries from both us and them.
			fill |= fpm.fill;
			last = 0;
			return true;
		}
		return false;
	}

	bool isFree(PN32 upn) {
		return	upn < size()	? test(upn)
			:	upn < max_size	? fill
			:	false;
	}

	void free(PN32 upn) {
		if (upn != pnNil)
			set(upn);
		last = 0;
	}

	void setAll() {
		set_all();
		fill	= true;
		last	= 0;
	}

	void clearAll() {
		clear_all();
		fill	= false;
		last	= 0;
	}

	PN32 alloc() {
		for (;;) {
			int i = next(last, true);
			if (i == size32() && i < max_size)
				resize(i + 64, true);

			if (block) {
				// For a bigMsf, the page map pages are scattered at regular intervals throughout the file
				// Allow the first three special pages to be allocated
				if (i >= cpnBigReserved) {
					// Every other pair of page map pages starts at offsets 1 and 2 from the start of their page range
					PN32 t = i & (block - 1);
					if (t == 1 || t == 2) {
						// We've hit the pages needed for the next page map So reserve them, and search again
						grow(i + 64, true);
						clear(i);
						clear(i + 1);
						continue;
					}
				}
			}

			clear(i);
			return last = i;
		}
	}

	bool EnsureRoom(PN32 upnMax) {
		return (bool)resize(upnMax, fill);
	}
};

//-----------------------------------------------------------------------------
//	StreamTable (in memory) stream table
//-----------------------------------------------------------------------------

struct StreamTable : dynamic_array<SI> {
	static const uint32 snUserMin	= 1,		// first valid user sn
						unsnNil		= ~0;

	uint32	log_page	= 0;
	UNSN	max_unsn	= 0;

	void init(uint32 _log_page, UNSN _max_unsn) {
		log_page	= _log_page;
		max_unsn	= _max_unsn;
	}

	StreamTable() {
		reserve(256);	// start with a reasonable allocation
		resize(5);		// actual size needed for bootstrapping
	}

	UNSN MaxValid() const {
		auto	b	= begin(), i = end();
		while (i != b && !i[-1].isValid())
			--i;
		return index_of(i);
	}

	UNSN NextValid(UNSN sn) const {
		for (auto i = begin() + sn, e = begin() + MaxValid(); i < e; ++i) {
			if (i->isValid())
				return index_of(i);
		}
		return unsnNil;
	}

	UNSN MinFree() {
		for (auto i = begin() + snUserMin, e = begin() + MaxValid(); i < e; ++i) {
			if (!i->isValid())
				return index_of(i);
		}
		if (max_unsn < unsnNil && size() < max_unsn) {
			push_back(SI());
			return size32() - 1;
		}
		return unsnNil;
	}

	EC SerializeBig(uint8* pb) {
		UNSN	max	= MaxValid();
		*((SPN32*&)pb)++ = max;

		uint32* pcb = (uint32*)pb;
		for (auto& i : slice_to(max))
			*pcb++ = i.cb;

		// serialize each valid SI
		PN32*	ppn = (PN32*)pcb;
		for (auto& i : slice_to(max)) {
			if (i.isValid()) {
				copy(i, ppn);
				ppn += i.size();
			}
		}
		return EC_OK;
	}
	EC DeSerializeBig(const uint8* pb) {
		UNSN	max	= *((SPN32*&)pb)++;

		if (max >= max_unsn)
			return EC_CORRUPT;

		if (max > size() && !resize(max))
			return EC_OUT_OF_MEMORY;

		// De-serialize the CBs into SIs.
		const uint32 *pcb	= (const uint32*)pb;
		for (auto& i : slice_to(max)) {
			if (!i.allocForCb(*pcb++, log_page))
				return EC_OUT_OF_MEMORY;
		}
		for (auto& i : slice(max))
			i = SI();

		// deserialize each valid SI
		const PN32*	ppn = (const PN32*)pcb;
		for (auto& i : slice_to(max)) {
			if (i.isValid()) {
				rcopy(i, ppn);
				ppn += i.size();
			}
		}
		return EC_OK;
	}
	uint32 SizeBig() {
		UNSN	max		= MaxValid();
		uint32	size	= sizeof(SPN32) + max * sizeof(uint32);
		for (auto& i : slice_to(max)) {
			if (i.isValid())
				size += i.size() * sizeof(PN32);
		}
		return size;
	}

	EC SerializeSmall(uint8* pb) {
		UNSN	max			= MaxValid();
		*((SN*&)pb)++		= SN(max);
		*((uint16*&)pb)++	= 0;

		SI_PERSIST* psi = (SI_PERSIST*)pb;
		for (auto& i : slice_to(max))
			*psi++ = i;

		// serialize each valid SI
		PN16	*ppn = (PN16*)psi;
		for (auto& i : slice_to(max)) {
			if (i.isValid()) {
				copy(i, ppn);
				ppn += i.size();
			}
		}
		return EC_OK;
	}
	EC DeSerializeSmall(const uint8* pb) {
		UNSN	max	= *((SN*&)pb)++;
		((uint16*&)pb)++;

		if (max >= Params::max_sn)
			return EC_CORRUPT;

		if (max > size() && !resize(max))
			return EC_OUT_OF_MEMORY;


		SI_PERSIST*	psi	= (SI_PERSIST*)pb;
		for (auto& i : slice_to(max)) {
			if (!i.allocForCb(psi++->cb, log_page))
				return EC_OUT_OF_MEMORY;
		}
		for (auto& i : slice(max))
			i = SI();

		// deserialize each valid SI
		PN16	*ppn = (PN16*)psi;
		for (auto& i : slice_to(max)) {
			rcopy(i, ppn);
			ppn += i.size();
		}
		return EC_OK;
	}
	uint32 SizeSmall() {
		UNSN	max		= MaxValid();
		uint32  size	= sizeof(SN) + sizeof(uint16) + max * sizeof(SI_PERSIST);
		for (auto& i : slice_to(max)) {
			if (i.isValid())
				size += i.size() * sizeof(PN16);
		}
		return size;
	}
};

//-----------------------------------------------------------------------------
//	base
//-----------------------------------------------------------------------------

struct PG {
	uint8	data[Params::max_page_size];
};

struct HDR {
	static constexpr auto magic				= meta::make_array("Microsoft C/C++ program database 2.00\r\n\x1a\x4a\x47");
	static const uint32 max_serialization	= Params::max_sn * sizeof(SI_PERSIST) + sizeof(SN) + sizeof(uint16) + Params::max_max_pn * sizeof(PN16);

	char		szMagic[0x2b];
	uint32		page_size;	// page size
	PN16		fpm_pn;		// page no. of valid FPM
	PN16		max_pn;		// current no. of pages
	SI_PERSIST	siSt;		// stream table stream info
	PN16		mpspnpn[div_round_up(max_serialization, Params::min_page_size)];

	void	init(const Params &params) {
		clear(*this);
		fstr(szMagic)	= magic;
		page_size		= 1 << params.log2_page_size;
		fpm_pn			= 1;
		max_pn			= params.data_start;
	}
	void	reset(const Params &params) {
		fpm_pn = PN16(params.fpm_start + params.fpm_start + params.fpm_size - fpm_pn);
	}
	bool	valid() const {
		return fstr(szMagic) == magic;
	}
};

struct BIGHDR {
	static constexpr auto	magic				= meta::make_array("Microsoft C/C++ MSF 7.00\r\n\x1a\x44\x53");
	static const uint32		max_serialization	= Params::max_unsn * sizeof(SI_PERSIST) + sizeof(UNSN) + Params::max_max_upn * sizeof(PN32);

	char		szMagic[0x1e];
	uint32		page_size;		// page size
	PN32		fpm_pn;			// page no. of valid FPM
	PN32		max_pn;			// current no. of pages
	SI_PERSIST	siSt;			// stream table stream info
	PN32		mpspnpn[div_round_up(div_round_up(max_serialization, Params::min_page_size) * sizeof(PN32), Params::min_page_size)];

	void	init(const Params &params) {
		clear(*this);
		fstr(szMagic)	= magic;
		page_size		= 1 << params.log2_page_size;
		fpm_pn			= params.fpm_start;
		max_pn			= params.data_start;
	}
	void	reset(const Params &params) {
		fpm_pn = params.fpm_start + params.fpm_start + params.fpm_size - fpm_pn;
	}
	bool	valid() const {
		return fstr(szMagic) == magic;
	}
};

union header {
	HDR		hdr;
	BIGHDR	bighdr;
	PG		pg;

	int		type() const {
		return	hdr.valid()		? 1
			:	bighdr.valid()	? 2
			:	0;
	}
	void	reset(bool big, const Params &params) {
		if (big)
			bighdr.reset(params);
		else
			hdr.reset(params);
	}

	uint32		cbSt(bool big)		const	{ return big ? bighdr.siSt.cb : hdr.siSt.cb; }
	PN32		max_pn(bool big)	const	{ return big ? bighdr.max_pn : hdr.max_pn; }
};

class base {
protected:
	static const Params *GetParams(bool big, uint32 page_size);

	header		h;
	bool		big;
	Params		params;
	FPM			fpm, fpmFreed, fpmCommitted;
	StreamTable	st;
	SI			siPnList;

	bool		validSn(UNSN sn)		const	{ return sn < st.max_unsn;	}
	bool		validUserSn(UNSN sn)	const	{ return sn && validSn(sn);	}
	bool		extantSn(UNSN sn)		const	{ return sn && sn < st.size() && st[sn].isValid();	}
	bool		validPn(PN32 pn)		const	{ return pn < params.max_pn;	}
	bool		extantPn(PN32 pn)		const	{ return pn < max_pn();	}

	uint32		cbSt()					const	{ return h.cbSt(big); }
	PN32		max_pn()				const	{ return h.max_pn(big); }

	void	max_pn(PN32 pn) {
		if (big)
			h.bighdr.max_pn = pn;
		else
			h.hdr.max_pn = PN16(pn);
	}

	bool	setStreamSize(DWORD cbNewSize) {
		return false;
	}

	PN32	allocPage() {
		PN32 pn = fpm.alloc();
		if (pn != pnNil) {
			if (pn < max_pn())
				return pn;

			PN32 pnMacMax	= max(max_pn(), pn);
			PN32 upnMacNew	= min(pnMacMax + params.file_growth, PN32(params.max_pn - 1));

			if (upnMacNew > max_pn() && setStreamSize(upnMacNew << params.log2_page_size)) {
				max_pn(upnMacNew);
				return pn;
			}

			fpm.free(pn); // back out
		}
		return pnNil;
	}

	void	freePage(PN32 pn) {
		if (fpmCommitted.isFree(pn)) {
			// if the page was not used in the previous committed set of pages, it is available again immediately in this transaction
			fpm.free(pn);
		} else {
			// otherwise, add to the standard free list
			fpmFreed.EnsureRoom(pn + 1);
			fpmFreed.free(pn);
		}
	}

	bool	seek(const common_intf &file, PN32 pn, int32 off) const {
		if (pn >= params.max_pn)
			return false;
		file.seek(off + (streamptr(pn) << params.log2_page_size));
		return true;
	}

	bool	read(istream_ref file, PN32 pn, int32 off, uint32 cb, void* buf) const {
		return extantPn(pn)
			&& (cb == 0 || extantPn(pn + ceil_pow2(cb, params.log2_page_size) - 1))
			&& seek(file, pn, off)
			&& check_readbuff(file, buf, cb);
	}
	bool	read(istream_ref file, PN32 pn, void* buf) const {
		return read(file, pn, 0, 1 << params.log2_page_size, buf);
	}

	bool	init(bool _big, uint32 page_size) {
		auto	*p	= GetParams(_big, page_size);
		if (!p)
			return false;

		big		= _big;
		params	= *p;

		int	max_size	= params.fpm_maxbits;
		int	block		= big ? page_size : 0;
		fpm.init(max_size, block);
		fpmFreed.init(max_size, block);
		fpmCommitted.init(max_size, block);

		st.init(params.log2_page_size, big ? Params::max_unsn : Params::max_sn);
		return true;
	}

	void	reset() {
		h.reset(big, params);
		fpmFreed.clearAll();	// no pages recently freed
		fpmCommitted = fpm;		// cache current committed pages
	}

public:
	uint32	StreamLength(SN sn)	const {
		return validUserSn(sn) && extantSn(sn) ? st[sn].cb : 0;
	}
	SN		GetFreeStream() {
		return st.MinFree();
	}

	struct ValidStreams {
		const StreamTable	&st;
		struct iterator {
			const StreamTable	&st;
			UNSN				sn;
			iterator(const StreamTable &st, UNSN sn) : st(st), sn(sn) {}
			iterator& operator++()							{ sn = st.NextValid(sn + 1); return *this; }
			bool	operator==(const iterator &b)	const	{ return sn == b.sn; }
			bool	operator!=(const iterator &b)	const	{ return sn != b.sn; }
			UNSN	operator*()						const	{ return sn; }
		};
		iterator begin()	const { return iterator(st, st.NextValid(StreamTable::snUserMin)); }
		iterator end()		const { return iterator(st, StreamTable::unsnNil); }
		ValidStreams(const StreamTable &st) : st(st) {}
	};
	ValidStreams Streams() const {
		return st;
	}
};

//-----------------------------------------------------------------------------
//	reader
//-----------------------------------------------------------------------------

inline bool on_stack(const void* p) {
	int	dummy;
	if (p < (void*)&dummy)
		return false;
	uintptr_t	lo, hi;
	GetCurrentThreadStackLimits(&lo, &hi);
	return p < (void*)hi;
}

class reader : public base, public refs<reader> {
	istream_ptr	file;

	uint32	internalReadStream(const SI &si, int32 off, void* buffer, uint32 cb);
	EC		deserialize_FPM();
	EC		create_small(SI &si, uint32 cb);
	EC		create_big(SI &si, uint32 cb);

public:
	reader(istream_ref file, EC* pec = 0);
	void	release()		{ if (!--nrefs && !on_stack(this)) delete this; }

	uint32	ReadStream(SN sn, int32 off, void* buffer, uint32 cb) {
		return validUserSn(sn) && extantSn(sn) ? internalReadStream(st[sn], off, buffer, cb) : 0;
	}

	malloc_block GetStream(SN sn) {
		uint32	size	= StreamLength(sn);
		malloc_block	mem(size);
		uint32	read	= ReadStream(sn, 0, mem, size);
		return mem;
	}
};

struct stream_reader : reader_mixin<stream_reader> {
	ref_ptr<MSF::reader>	msf;
	SN			sn;
	streamptr	pos;

	streamptr	tell()						{ return pos;	}
	streamptr	length()					{ return msf->StreamLength(sn); }
	void		seek(streamptr offset)		{ pos = offset; }
	void		seek_cur(streamptr offset)	{ pos += offset; }
	void		seek_end(streamptr offset)	{ pos = length() + offset; }

	size_t		readbuff(void *buffer, size_t size)	{
		uint32	n = msf->ReadStream(sn, pos, buffer, (uint32)size);
		pos += n;
		return n;
	}

	stream_reader(MSF::reader *msf, SN sn, streamptr pos = 0) : msf(msf), sn(sn), pos(pos) {}
};

//-----------------------------------------------------------------------------
//	writer
//-----------------------------------------------------------------------------

#if 1
class writer : public base, public refs<writer> {
	iostream_ref file;

	bool	internalReplaceStream(SI &si, const void* buffer, uint32 cb) {
		return si.allocForCb(cb, params.log2_page_size) && writeNewDataPgs(si, 0, buffer, cb);
	}

	bool	internalDeleteStream(SI &si, bool erase = false);
	uint32	internalWriteStream(const SI &si, int32 off, const void* buffer, uint32 cb);

	bool	zeroPage(PN32 pn) {
		PG pg;
		if (!read(file, pn, &pg))
			return false;
		uint32	page_size = 1 << params.log2_page_size;
		memset(&pg, 0, page_size);
		return write(pn, 0, page_size, &pg);
	}

	bool	write(PN32 pn, int32 off, uint32 cb, const void *buf) {
		return seek(file, pn, off) && check_writebuff(file, buf, cb);
	}

	bool	writeNewDataPgs(SI &si, SPN32 spn, const void* buffer, uint32 cb) {
		// allocate pages up front to see if we can cluster write them
		SPN32	spnT		= spn;
		uint32	page_size	= 1 << params.log2_page_size;

		for (uint32 cbWrite = cb; cbWrite > 0; cbWrite -= page_size) {
			PN32	pn = allocPage();
			if (pn == pnNil)
				return false;
			si[spnT++] = pn;
		}

		while (cb > 0) {
			PN32	pnStart = si[spn], pnLim = pnStart;
			uint32	cbWrite = 0;
			do {
				spn++;
				pnLim++;
				uint32	t = min(page_size, cb);
				cbWrite	+= t;
				cb		-= t;
			} while (cb > 0 && si[spn] == pnLim);

			if (!write(pnStart, 0, cbWrite, buffer))
				return false;

			buffer = (uint8*)buffer + cbWrite;
		}

		return cb == 0;
	}

	bool	writeNewPn(PN32 *ppn, const void* buffer) {
		PN32	pn	= allocPage();
		if (pn != pnNil && write(pn, 0, 1 << params.log2_page_size, buffer)) {
			freePage(*ppn);
			*ppn = pn;
			return true;
		}
		return false;
	}

	bool	replace(PN32 *ppn, int32 off, uint32 cb, const void* buffer) {
		PG	pg;
		if (!read(file, *ppn, &pg))
			return false;
		memcpy(pg.data + off, buffer, cb);
		return writeNewPn(ppn, &pg);
	}

	EC	serialize_FPM();
	EC	serialize_MSF(uint8* pb, uint32 c);

public:
	writer(iostream_ref file, EC* pec, uint32 page_size);

	// Overwrite a piece of a stream.  Will not grow the stream, will fail instead.
	uint32	WriteStream(SN sn, int32 off, const void* buffer, uint32 cb) {
		return validUserSn(sn) && extantSn(sn) && off + cb <= StreamLength(sn) ? internalWriteStream(st[sn], off, buffer, cb) : 0;
	}
	bool	ReplaceStream(SN sn, const void* buffer, uint32 cb) {
		return validUserSn(sn) && extantSn(sn) && internalReplaceStream(st[sn], buffer, cb);
	}
	bool	DeleteStream(SN sn) {
		return validUserSn(sn) && extantSn(sn) && internalDeleteStream(st[sn]);
	}
	bool	AppendStream(SN sn, const void* buffer, uint32 cb);
	bool	TruncateStream(SN sn, uint32 cb);

	EC		Commit();
};

struct stream_writer {
	ref_ptr<MSF::writer>	msf;
	SN			sn;
	streamptr	pos;

	streamptr	tell()						{ return pos;	}
	streamptr	length()					{ return msf->StreamLength(sn); }
	void		seek(streamptr offset)		{ pos = offset; }
	void		seek_cur(streamptr offset)	{ pos += offset; }
	void		seek_end(streamptr offset)	{ pos = length() + offset; }

	size_t		writebuff(const void *buffer, size_t size)	{
		uint32	n = msf->WriteStream(sn, pos, buffer, (uint32)size);
		pos += n;
		return n;
	}
	stream_writer(MSF::writer *msf, SN sn, streamptr pos = 0) : msf(msf), sn(sn), pos(pos) {}
	~stream_writer() { msf->Commit(); }
};

#endif

} // namespace MSF

#endif // MSF_H
