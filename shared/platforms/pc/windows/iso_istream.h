#ifndef ISO_ISTREAM_H
#define ISO_ISTREAM_H

#include <objidl.h>
#include "stream.h"
#include "com.h"

//-----------------------------------------------------------------------------
//
// IStream wrapper
//
//-----------------------------------------------------------------------------

namespace iso {
class ISO_IStreamI : public com<IStream> {
	istream_ref		file;
public:
	ISO_IStreamI(istream_ref file) : file(file)	{}
	operator IStream*()  { return this; }
	//
	// Methods of IStream
	//
	HRESULT STDMETHODCALLTYPE	Read(void *pv, ULONG cb, ULONG *pcbRead) {
		*pcbRead = file.readbuff(pv, cb);
		return S_OK;
	}
	HRESULT STDMETHODCALLTYPE	Seek(LARGE_INTEGER dlibMove, DWORD dwOrigin, ULARGE_INTEGER *plibNewPosition) {
		switch (dwOrigin) {
			case SEEK_SET:	file.seek(dlibMove.QuadPart); break;
			case SEEK_CUR:	file.seek_cur(dlibMove.QuadPart); break;
			case SEEK_END:	file.seek_end(dlibMove.QuadPart); break;
		}
		if (plibNewPosition)
			plibNewPosition->QuadPart = file.tell();
		return S_OK;
	}
	HRESULT STDMETHODCALLTYPE	Stat(STATSTG *pstatstg, DWORD grfStatFlag) {
		if (!pstatstg || grfStatFlag != STATFLAG_NONAME)
			return E_INVALIDARG;

		memset(pstatstg, 0, sizeof(STATSTG));
		pstatstg->type				= STGTY_STREAM;
		pstatstg->cbSize.QuadPart	= file.length();
		return S_OK;
	}

	//
	// Unimplemented methods of IStream
	//
	HRESULT STDMETHODCALLTYPE Write(void const *pv, ULONG cb, ULONG *pcbWritten)												{ return E_NOTIMPL; }
	HRESULT STDMETHODCALLTYPE SetSize(ULARGE_INTEGER libNewSize)																{ return E_NOTIMPL; }
	HRESULT STDMETHODCALLTYPE CopyTo(IStream *pstm, ULARGE_INTEGER cb, ULARGE_INTEGER *pcbRead, ULARGE_INTEGER *pcbWritten)		{ return E_NOTIMPL; }
	HRESULT STDMETHODCALLTYPE Commit(DWORD grfCommitFlags)																		{ return E_NOTIMPL; }
	HRESULT STDMETHODCALLTYPE Revert()																							{ return E_NOTIMPL; }
	HRESULT STDMETHODCALLTYPE LockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType)							{ return E_NOTIMPL; }
	HRESULT STDMETHODCALLTYPE UnlockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType)						{ return E_NOTIMPL; }
	HRESULT STDMETHODCALLTYPE Clone(IStream **ppstm)																			{ return E_NOTIMPL; }
};

class ISO_IStreamO : public com<IStream> {
	ostream_ref		file;
public:
	ISO_IStreamO(ostream_ref file) : file(file)	{}
	~ISO_IStreamO()				{}
	//
	// Methods of IStream
	//
	HRESULT STDMETHODCALLTYPE Write(void const *pv, ULONG cb, ULONG *pcbWritten) {
		*pcbWritten = file.writebuff(pv, cb);
		return S_OK;
	}
	HRESULT STDMETHODCALLTYPE	Seek(LARGE_INTEGER dlibMove, DWORD dwOrigin, ULARGE_INTEGER *plibNewPosition) {
		switch (dwOrigin) {
			case SEEK_SET:	file.seek(dlibMove.QuadPart); break;
			case SEEK_CUR:	file.seek_cur(dlibMove.QuadPart); break;
			case SEEK_END:	file.seek_end(dlibMove.QuadPart); break;
		}
		if (plibNewPosition)
			plibNewPosition->QuadPart = file.tell();
		return S_OK;
	}
	HRESULT STDMETHODCALLTYPE	Stat(STATSTG *pstatstg, DWORD grfStatFlag) {
		if (!pstatstg || grfStatFlag != STATFLAG_NONAME)
			return E_INVALIDARG;

		memset(pstatstg, 0, sizeof(STATSTG));
		pstatstg->type				= STGTY_STREAM;
		pstatstg->cbSize.QuadPart	= file.length();
		return S_OK;
	}

	//
	// Unimplemented methods of IStream
	//
	HRESULT STDMETHODCALLTYPE Read(void *pv, ULONG cb, ULONG *pcbRead)															{ return E_NOTIMPL; }
	HRESULT STDMETHODCALLTYPE SetSize(ULARGE_INTEGER libNewSize)																{ return E_NOTIMPL; }
	HRESULT STDMETHODCALLTYPE CopyTo(IStream *pstm, ULARGE_INTEGER cb, ULARGE_INTEGER *pcbRead, ULARGE_INTEGER *pcbWritten)		{ return E_NOTIMPL; }
	HRESULT STDMETHODCALLTYPE Commit(DWORD grfCommitFlags)																		{ return E_NOTIMPL; }
	HRESULT STDMETHODCALLTYPE Revert()																							{ return E_NOTIMPL; }
	HRESULT STDMETHODCALLTYPE LockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType)							{ return E_NOTIMPL; }
	HRESULT STDMETHODCALLTYPE UnlockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType)						{ return E_NOTIMPL; }
	HRESULT STDMETHODCALLTYPE Clone(IStream **ppstm)																			{ return E_NOTIMPL; }
};

class IStream_adapter {
protected:
	com_ptr2<IStream>	istream;
public:
	IStream_adapter(IStream *istream)	: istream(istream)	{}
	bool		exists()					{ return istream; }
	bool		eof()						{ return tell() >= length(); }
	streamptr	length()					{
		STATSTG	stat;
		istream->Stat(&stat, STATFLAG_NONAME);
		return stat.cbSize.QuadPart;
	}
	streamptr	tell()						{ streamptr pos = 0; istream->Seek((const LARGE_INTEGER&)pos, SEEK_CUR, (ULARGE_INTEGER*)&pos); return pos; }
	void		seek(streamptr offset)		{ istream->Seek((const LARGE_INTEGER&)offset, SEEK_SET, nullptr); }
	void		seek_cur(streamptr offset)	{ istream->Seek((const LARGE_INTEGER&)offset, SEEK_CUR, nullptr); }
	void		seek_end(streamptr offset)	{ istream->Seek((const LARGE_INTEGER&)offset, SEEK_END, nullptr); }

	IStream*	_clone() const				{ return istream; }

	const wchar_t*	get_filename() const {
		STATSTG	stat;
		istream->Stat(&stat, STATFLAG_DEFAULT);
		return stat.pwcsName;

	}
};


struct IStream_reader0 : public IStream_adapter {
	using IStream_adapter::IStream_adapter;
	size_t	readbuff(void *buffer, size_t size) {
		ULONG	read;
		istream->Read(buffer, size, &read);
		return read;
	}
	int		getc() { uint8 c; return readbuff(&c, 1) ? c : EOF; }
};
typedef reader_mixout<IStream_reader0>	IStream_reader;

struct IStream_writer0 : public IStream_adapter {
	using IStream_adapter::IStream_adapter;
	size_t	writebuff(const void *buffer, size_t size) {
		ULONG	written;
		istream->Write(buffer, size, &written);
		return written;
	}
};
typedef writer_mixout<IStream_writer0>	IStream_writer;

struct IStream_readwriter0 : public IStream_reader0 {
	using IStream_reader0::IStream_reader0;
	size_t	writebuff(const void *buffer, size_t size) {
		ULONG	written;
		istream->Write(buffer, size, &written);
		return written;
	}
};
typedef readwriter_mixout<IStream_readwriter0>	IStream_readwriter;

}// namesapce iso
#endif // ISO_ISTREAM_H