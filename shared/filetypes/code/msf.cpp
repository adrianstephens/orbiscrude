#include "msf.h"

using namespace iso;

#define MSF_PARMS(log2_page_size, max_pn, growth, fpm_size) { log2_page_size, max_pn, growth, max_pn, fpm_size, 1, 1+fpm_size+fpm_size }

const MSF::Params  rgmsfparms[] = {
	MSF_PARMS(10, 0x10000, 8, 8),	// gives 64meg
	MSF_PARMS(11, 0x10000, 4, 4),	// gives 128meg
	MSF_PARMS(12,  0x8000, 2, 1)	// gives 128meg
};

const MSF::Params  rgmsfparms_hc[] = {
	MSF_PARMS( 9, 0x100000, 4, 1),	// gives .5GB, note this is used primarily for small pdbs, so keep growth rate low (2K).
	MSF_PARMS(10, 0x100000, 8, 1),	// gives 1.0GB
	MSF_PARMS(11, 0x100000, 4, 1),	// gives 2.0GB
	MSF_PARMS(12, 0x100000, 2, 1)	// gives 4.0GB
};
//-----------------------------------------------------------------------------
//	base
//-----------------------------------------------------------------------------

const MSF::Params *MSF::base::GetParams(bool big, uint32 page_size) {
	const MSF::Params	*p;
	int				n;

	if (big) {
		p	= rgmsfparms_hc;
		n	= num_elements32(rgmsfparms_hc);
	} else {
		p	= rgmsfparms;
		n	= num_elements32(rgmsfparms);
	}

	while (n--) {
		if (page_size == (1 << p->log2_page_size))
			return p;
		++p;
	}

	return nullptr;
}

//-----------------------------------------------------------------------------
//	MSF::reader
//-----------------------------------------------------------------------------

MSF::EC MSF::reader::create_small(SI &si, uint32 cb) {
	uint32	log_page	= params.log2_page_size;

	if (!si.allocForCb(cb, log_page))
		return EC_OUT_OF_MEMORY;

	// Have to copy PNs into an array of UPNs
	rcopy(si, h.hdr.mpspnpn);
	return EC_OK;
}

MSF::EC MSF::reader::create_big(SI &si, uint32 cb) {
	uint32	log_page	= params.log2_page_size;
	PN32	cpn			= ceil_pow2(cb, log_page);
	uint32	cbpn		= cpn * sizeof(PN32);

	if (!siPnList.allocForCb(cbpn, log_page))
		return EC_OUT_OF_MEMORY;

	if (siPnList.size() > num_elements(h.bighdr.mpspnpn))
		return EC_CORRUPT;

	rcopy(siPnList, h.bighdr.mpspnpn);

	// siPnList now contains the list of pages that contain the mpspnpn for the stream table
	return	!si.allocForCb(cb, log_page)						? EC_OUT_OF_MEMORY
		:	internalReadStream(siPnList, 0, si, cbpn) != cbpn	? EC_CORRUPT
		:	EC_OK;
}

MSF::reader::reader(istream_ref file, EC* pec) : file(file.clone()) {
	if (!file.read(h.hdr)) {
		if (pec)
			*pec = EC_FILE_SYSTEM;
		return;
	}

	bool	valid = false;
	switch (h.type()) {
		case 1:	valid = init(false, h.hdr.page_size);	break;
		case 2:	valid = init(true, h.bighdr.page_size);	break;
	}
	if (!valid) {
		if (pec)
			*pec = EC_FORMAT;
		return;
	}

	if (auto ec = deserialize_FPM()) {
		if (pec)
			*pec = ec;
		return;
	}

	// Build the stream table stream info from the header, then load the stream table stream and deserialize it
	uint32 cb = cbSt();
	if (cb < 0) {
		if (pec)
			*pec = EC_CORRUPT;
		return;
	}

	SI siSt;
	if (auto ec = big ? create_big(siSt, cb) : create_small(siSt, cb)) {
		if (pec)
			*pec = ec;
		return;
	}

	malloc_block	pbSt(cb);
	if (!pbSt) {
		if (pec)
			*pec = EC_OUT_OF_MEMORY;
		return;
	}

	if (internalReadStream(siSt, 0, pbSt, cb) != cb) {
		if (pec)
			*pec = EC_CORRUPT;
		return;
	}

	if (auto ec = big ? st.DeSerializeBig(pbSt) : st.DeSerializeSmall(pbSt)) {
		if (pec)
			*pec = ec;
		return;
	}

	// The st[0] just loaded is bogus: it is the StreamTable stream in effect prior to the previous Commit
	// Replace it with the good copy saved in the MSF hdr
	st[0] = siSt;

	reset();
	if (pec)
		*pec = EC_OK;
}

// Read a stream, cluster reads contiguous pages
uint32 MSF::reader::internalReadStream(const SI &si, int32 off, void* buffer, uint32 cb) {
	// ensure off and *pcbBuf remain within the stream
	if (off < 0 || off > si.cb)
		return 0;

	if (cb > si.cb - off)
		cb = si.cb - off;

	uint32	log_page	= params.log2_page_size;
	uint32	page_size	= 1 << log_page;
	SPN32	spn			= SPN32(off >> log_page);
	uint32  total		= 0;

	// first partial page, if any
	if (int32 offPg = off & bits(log_page)) {
		uint32 first = min(page_size - offPg, cb);
		if (!read(file, si[spn], offPg, first, buffer))
			return total;

		total	+= first;
		cb		-= first;
		buffer	= (uint8*)buffer + first;
		spn++;
	}

	// intermediate full pages, if any
	while (cb > 0) {
		// accumulate contiguous pages into one big read
		PN32	pnStart = si[spn], pnLim = pnStart;
		uint32	cont	= 0;
		do {
			spn++;
			pnLim++;
			uint32	t = min(page_size, cb);
			cont	+= t;
			cb		-= t;
		} while (cb > 0 && si[spn] == pnLim);

		if (!read(file, pnStart, 0, cont, buffer))
			break;

		total	+= cont;
		buffer	= (uint8*)buffer + cont;
	}

	return total;
}

MSF::EC MSF::reader::deserialize_FPM() {
	uint32	log_page	= params.log2_page_size;

	if (!big) {
		if (fpm.EnsureRoom(params.fpm_maxbits))	// we know the free page map is contiguous
			return read(file, h.hdr.fpm_pn, 0, params.fpm_size << log_page, fpm.raw().begin()) ? EC_OK : EC_FILE_SYSTEM;

		return EC_OUT_OF_MEMORY;
	}

	PN32	first	= h.bighdr.fpm_pn;
	if (first != 1 && first != 2)
		return EC_FORMAT;

	// Calc number of pages needed
	PN32	num		= shift_right_ceil(align(max_pn(), BIT_COUNT<uint64>), log_page + 3);
	if (!fpm.EnsureRoom(num << (log_page + 3)))
		return EC_OUT_OF_MEMORY;

	fpm.setAll();
	uint8	*p			= (uint8*)fpm.raw().begin();
	uint32	page_size	= 1 << log_page;
	for (PN32 next = first; num--; next += page_size, p += page_size) {
		if (!read(file, next, 0, page_size, p))
			return EC_FILE_SYSTEM;
	}
	return EC_OK;
}

//-----------------------------------------------------------------------------
//	writer
//-----------------------------------------------------------------------------
#if 1
MSF::writer::writer(iostream_ref file, EC* pec, uint32 page_size) : file(file) {
	if (!init(true, page_size)) {
		if (pec)
			*pec = EC_FORMAT;
		return;
	}

	// init hdr; when creating a new MSF, always create the Big variant.
	h.bighdr.init(params);

	fpm.setAll();			// mark all non-special pages free
	fpmCommitted.setAll();
	fpmFreed.clearAll();	// no pages freed yet

	// store it!
	if (auto ec = Commit())
		*pec = ec;
}

MSF::EC MSF::writer::Commit() {
	// write the new stream table to disk as a special stream
	uint32			cbSt = big ? st.SizeBig() : st.SizeSmall();
	malloc_block	pbSt(cbSt);

	if (!pbSt)
		return EC_OUT_OF_MEMORY;

	uint32	log_page	= params.log2_page_size;

	// copy the stream table stream info into the header
	if (big) {
		if (auto ec = st.SerializeBig(pbSt))
			return ec;

		if (auto ec = serialize_MSF(pbSt, cbSt))
			return ec;

		// don't copy the stream table's page numbers, copy the list of pages of these numbers
		h.bighdr.siSt = st[0];
		memcpy(h.bighdr.mpspnpn, siPnList, ceil_pow2(siPnList.cb, log_page) * sizeof(PN32));

	} else {
		if (auto ec = st.SerializeSmall(pbSt))
			return ec;

		if (!internalReplaceStream(st[0], pbSt, cbSt))
			return EC_FILE_SYSTEM;

		h.hdr.siSt = st[0];
		for (SPN32 ispn = 0; ispn < h.hdr.siSt.spnMax(log_page); ispn++)
			h.hdr.mpspnpn[ispn] = PN16(st[0][ispn]);
	}

	// mark pages that have been freed to the next FPM as free.
	if (!fpm.add(fpmFreed))
		return EC_OUT_OF_MEMORY;

	if (auto ec = serialize_FPM())
		return ec;

	// at this point, all pages but hdr safely reside on disk
	if (!write(0, 0, 1 << log_page, &h))
		return EC_FILE_SYSTEM;

	reset();
	return EC_OK;
}

// Read or write a piece of a stream.
uint32 MSF::writer::internalWriteStream(const SI &si, int32 off, const void* buffer, uint32 cb) {
	// ensure off and *pcbBuf remain within the stream
	if (off < 0 || off > si.cb)
		return 0;

	if (off + cb > si.cb)
		cb = si.cb - off;

	uint32	log_page	= params.log2_page_size;
	SPN32	spn			= SPN32(off >> log_page);
	uint32	page_size	= 1 << log_page;
	uint32  total		= 0;

	// first partial page, if any
	if (int32 offPg = off & bits(log_page)) {
		uint32 first = min(page_size - offPg, cb);
		if (!replace(&si[spn], offPg, first, buffer))
			return total;

		total	+= first;
		cb		-= first;
		buffer	= (uint8*)buffer + first;
		spn++;
	}

	// intermediate full pages, if any
	while (cb >= page_size) {
		if (!writeNewPn(&si[spn], (uint8*)buffer))
			return total;

		total	+= page_size;
		cb		-= page_size;
		buffer	= (uint8*)buffer + page_size;
		spn++;
	}

	// last partial page, if any
	if (cb > 0 && replace(&si[spn], 0, cb, buffer))
		total += cb;

	return total;
}

bool MSF::writer::AppendStream(SN sn, const void* buffer, uint32 cb) {
	if (!validUserSn(sn) || !extantSn(sn) || cb < 0)
		return false;

	if (cb == 0)
		return true;

	SI		&si			= st[sn];
	uint32	log_page	= params.log2_page_size;

	if (si.spnMax(log_page) < ceil_pow2(si.cb + cb, log_page)) {
		// allocate a new SI, copied from the old one
		SI siNew;
		if (!siNew.allocForCb(si.cb + cb, log_page))
			return false;

		// copy the old stuff over
		PN32   spnForSiMax = si.spnMax(log_page);
		memcpy(siNew, si, spnForSiMax * sizeof(PN32));

		// initialize the new
		PN32   spnForSiNewMax = siNew.spnMax(log_page);
		for (PN32 spn = spnForSiMax; spn < spnForSiNewMax; spn++)
			siNew[spn] = pnNil;

		siNew.cb	= si.cb;   // so far, nothing has been appended
		si			= siNew;
	}

	if (int32 offLast = si.cb & bits(log_page)) {
		// Fill any space on the last page of the stream
		// Writes to the current (likely nontransacted) page which is safe on most "extend-stream" type Append scenarios: if the transaction aborts, the stream info is not updated and no preexisting data is overwritten
		// This is a dangerous optimization which (to guard) we now incur overhead elsewhere; see comment on Truncate()/Append() interaction in TruncateStream()
		PN32	pnLast	= si[si.spnMax(log_page) - 1];
		uint32	first	= min((1 << log_page) - offLast, cb);
		if (!write(pnLast, offLast, first, buffer))
			return false;

		si.cb	+= first;
		cb		-= first;
		buffer	= (uint8*)buffer + first;
	}

	if (cb > 0) {
		// append additional data and update the stream map
		if (!writeNewDataPgs(si, SPN32(si.spnMax(log_page)), buffer, cb))
			return false;
		si.cb += cb;
	}

	st[sn] = si;	// store back the new one
	return true;
}

bool MSF::writer::TruncateStream(SN sn, uint32 cb) {
	if (!validUserSn(SN(sn)) || !extantSn(SN(sn)))
		return false;

	SI	&si = st[sn];
	if (cb > si.cb || cb < 0)
		return false;

	uint32	log_page	= params.log2_page_size;
	SPN32	spnNewMax	= SPN32(ceil_pow2(cb, log_page));

	if (spnNewMax < si.spnMax(log_page)) {
		// The new stream length requires fewer pages...
		// Allocate a new SI, copied from the old one.
		SI siNew;
		if (!siNew.allocForCb(cb, log_page))
			return false;

		memcpy(siNew, si, spnNewMax * sizeof(PN32));

		// Free subsequent, unneeded pages.
		for (SPN32 spn = spnNewMax; spn < si.spnMax(log_page); spn++)
			freePage(si[spn]);

		st[sn] = SI();
		si = siNew;
	}
	si.cb = cb;

	// In case of Truncate(sn, cb) where cb > 0, and in case the Truncate() is followed by an Append(), we must copy the new last partial page
	// of the stream to a transacted page, because the subsequent Append() is optimized to write new stuff to the last (e.g. current,
	// nontransacted) page of the stream.  So, the scenario Truncate() then Append() may need this code or else on transacation abort
	// we could damage former contents of the stream
	if (si.cb & bits(log_page)) {
		PG pg;
		if (!read(file, si[si.spnMax(log_page) - 1], &pg) || !writeNewPn(&si[si.spnMax(log_page) - 1], &pg))
			return false;
	}

	st[sn] = si;
	return true;
}

bool MSF::writer::internalDeleteStream(SI &si, bool erase) {
	for (SPN32 spn = 0; spn < si.spnMax(params.log2_page_size); spn++) {
		if (erase && !zeroPage(si[spn]))
			return false;
		freePage(si[spn]);
	}

	si = SI();
	return true;
}

MSF::EC MSF::writer::serialize_MSF(uint8* pb, uint32 cb) {
	// Write the ST to disk
	if (!internalReplaceStream(st[0], pb, cb))
		return EC_FILE_SYSTEM;

	// Calculate how many pages will be needed to serialize this mess.
	// Reserve the pages we'll store this list in, and store the page numbers in siPnList (use a tmp first)
	// Write the list of page numbers in ST's SI to the list of pages

	uint32	log_page	= params.log2_page_size;
	PN32	cpnSerSt	= ceil_pow2(cb, log_page);
	uint32	cbPn		= cpnSerSt * sizeof(PN32);
	SI		siT;

	if (!siT.allocForCb(cbPn, log_page))
		return EC_OUT_OF_MEMORY;

	if (!writeNewDataPgs(siT, 0, st[0], cbPn))
		return EC_FILE_SYSTEM;

	// Free the pages we're no longer using in the siPnList
	for (PN32 ipn = 0, cpnOld = ceil_pow2(siPnList.cb, log_page); ipn < cpnOld; ipn++)
		freePage(siPnList[ipn]);

	siPnList = siT;
	return EC_OK;
}

MSF::EC MSF::writer::serialize_FPM() {
	uint32	log_page	= params.log2_page_size;
	if (!big) {
		if (fpm.EnsureRoom(params.fpm_maxbits))	// we know the free page map is contiguous
			return write(h.hdr.fpm_pn, 0, params.fpm_size << log_page, fpm.raw().begin()) ? EC_OK : EC_FILE_SYSTEM;

		return EC_OUT_OF_MEMORY;
	}

	PN32 first = h.bighdr.fpm_pn;
	if (first != 1 && first != 2)
		return EC_FORMAT;

	// Calc number of pages needed
	PN32	num		= shift_right_round(align(max_pn(), BIT_COUNT<uint64>), log_page + 3);
	if (!fpm.EnsureRoom(num << (log_page + 3)))
		return EC_OUT_OF_MEMORY;

	uint8	*p			= (uint8*)fpm.raw().begin();
	uint32	page_size	= 1 << log_page;
	for (PN32 next = first; num--; p += page_size, next += page_size) {
		if (!write(next, 0, page_size, p))
			return EC_FILE_SYSTEM;
	}
	return EC_OK;
}
#endif