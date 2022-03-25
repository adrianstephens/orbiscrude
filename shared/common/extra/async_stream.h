#ifndef ASYNC_STREAM_H
#define ASYNC_STREAM_H

#include "stream.h"
#include "filename.h"
#include "thread.h"

namespace iso {

//-----------------------------------------------------------------------------
//	generic
//-----------------------------------------------------------------------------

template<uint32 _SIZE = 65536> class async_buffers {
	uint8	*buffer, *p, *g;
protected:
	enum {SIZE = _SIZE};

	async_buffers() : buffer(0)		{}
	~async_buffers()				{ free(buffer); }

	void	init_read()				{ p = g = buffer = (uint8*)malloc(SIZE * 2); }
	void	init_write()			{ p = (g = buffer = (uint8*)malloc(SIZE * 2)) + SIZE; }
	void	clear_buffers()			{ free(buffer); buffer = 0; }

	bool	exists()		const	{ return !!buffer; }
	uint8	*put()			const	{ return p; }
	uint8	*get()			const	{ return g; }

	void	move_put(size_t n)		{ p	+= n; if (p - buffer >= SIZE * 2) p = buffer; }
	void	move_get(size_t n)		{ g	+= n; if (g - buffer >= SIZE * 2) g = buffer; }

	bool	need_more()	const		{ return g == p; }
	size_t	available() const		{ return p < g ? buffer + SIZE * 2 - g : p - g; }

	uint8	get1()									{ uint8 c = *g; move_get(1); return c; }
	size_t	get_data(void *dest, size_t size)		{ size = min(size, available()); memcpy(dest, g, size); move_get(size); return size; }

	void	put1(uint8 c)							{ *p = c; move_put(1); }
	size_t	put_data(const void *dest, size_t size)	{ size = min(size, SIZE * 2 - available()); memcpy(p, dest, size); move_put(size); return size; }
};

//-----------------------------------------------------------------------------
//	_async_reader	- wrapper around any reader to give it async protocol
//-----------------------------------------------------------------------------

template<typename R> class _async_reader : public Thread, protected async_buffers<> {
	Semaphore		sem;
	R				r;
protected:
	volatile int	read;
	bool			GetMore();
public:
	int				operator()() {
		for (;;) {
			sem.lock();
			if (read == -1) {
				read = -2;
				return 0;
			}
			auto	read0 = r.readbuff(put(), SIZE);
			read = read0 ? (int)read0 : -1;
		}
	}
	template<typename I> _async_reader(const I &i) : Thread(this, "async_reader"), r(i), read(0) {
		init_read();
		Start();
	}
	streamptr 	length() 		{ return r.length(); }
	auto		_clone() const 	{ return r._clone(); }
	~_async_reader();
};

template<typename R> _async_reader<R>::~_async_reader() {
	while (read == 0)
		yield();

	read = -1;
	sem.unlock();
	while (read == -1)
		yield();
	sem.unlock();
}

template<typename R> bool _async_reader<R>::GetMore() {
	while (read == 0)
		yield();

	if (read == -1)
		return false;

	move_put(read);
	read = 0;
	sem.unlock();
	return true;
}

//-----------------------------------------------------------------------------
//	_async_writer	- wrapper around any writer to give it async protocol
//-----------------------------------------------------------------------------

template<typename W> class _async_writer : public Thread, protected async_buffers<> {
	Semaphore		sem;
	W				&w;
protected:
	volatile int	wrote;
	bool			PutMore();
public:
	int				operator()();
	_async_writer(W &_w) : Thread(this, "async_writer"), w(_w) { init_write(); }
	~_async_writer();
};

template<typename W> _async_writer<W>::~_async_writer() {
	while (wrote == 0)
		yield();

	wrote = 0xffffffff;
	sem.unlock();
	w.writebuff(get(), available());
	while (wrote == 0xffffffff)
		yield();
	sem.unlock();
}

template<typename W> int _async_writer<W>::operator()() {
	for (;;) {
		sem.lock();
		if (wrote == 0xffffffff) {
			wrote = 0xfffffffe;
			return 0;
		}
		uint32	wrote0 = w.writebuff(get(), SIZE);
		wrote = wrote0 ? wrote0 : 0xffffffff;
	}
}

template<typename W> bool _async_writer<W>::PutMore() {
	while (wrote == 0)
		yield();

	if (wrote == 0xffffffff)
		return false;

	move_get(wrote);
	wrote = 0;
	sem.unlock();
	return true;
}

//-----------------------------------------------------------------------------
//	mixins
//-----------------------------------------------------------------------------

template<typename R> class async_reader_mixout : R, public stream_defaults<async_reader_mixout<R> > {
public:
	template<typename I> async_reader_mixout(const I &i) : R(i) {}

	size_t		readbuff(void *buffer, size_t size)	{
		size_t total = 0;
		while (size) {
			if (R::need_more() && !R::GetMore())
				break;
			size_t	x	= this->get_data(buffer, size);
			buffer		= (uint8*)buffer + x;
			size		-= x;
			total		+= x;
		}
		return total;
	}
	int			getc()				{ return R::need_more() && !R::GetMore() ? EOF : R::get1(); }
	bool		exists() 	const	{ return R::exists(); }
	streamptr	length() 			{ return R::length(); }
	auto		_clone()	const	{ return R::_clone(); }
};

template<typename R> class async_writer_mixout : R, public stream_defaults<async_writer_mixout<R> > {
public:
	template<typename I> async_writer_mixout(const I &i) : R(i) {}

	size_t		writebuff(const void *buffer, size_t size)	{
		size_t total = 0;
		while (size) {
			if (R::need_more() && !R::GetMore())
				break;
			size_t	x	= R::put_data(buffer, size);
			buffer		= (uint8*)buffer + x;
			size		-= x;
			total		+= x;
		}
		return total;
	}
	int			putc(int c)			{ uint8 v = c; return writebuff(&v, 1) ? v : EOF; }
	bool		exists() const		{ return R::exists(); }
};

} // namespace iso

#if defined PLAT_X360 || defined PLAT_PC
//-----------------------------------------------------------------------------
//	PC & X360
//-----------------------------------------------------------------------------

namespace iso {

	class win32_async_filereader : OVERLAPPED, protected async_buffers<> {
	protected:
		HANDLE			hFile;
		volatile int	read;

		void completion(DWORD code, DWORD bytes) {
			read	= bytes ? bytes : 0xffffffff;
		}
		static VOID CALLBACK _completion(DWORD code, DWORD bytes, OVERLAPPED *overlapped) {
			((win32_async_filereader*)overlapped)->completion(code, bytes);
		}
		bool	GetMore();
	public:
		win32_async_filereader(HANDLE hFile);
		win32_async_filereader(const filename &fn);
		~win32_async_filereader();
		streamptr	length() const {
			LARGE_INTEGER size;
			return GetFileSizeEx(hFile, &size) ? size.QuadPart : 0;
		}
		HANDLE		_clone() const {
			HANDLE	h2;
			return DuplicateHandle(GetCurrentProcess(), hFile, GetCurrentProcess(), &h2, 0, FALSE, DUPLICATE_SAME_ACCESS) ? h2 : INVALID_HANDLE_VALUE;
		}
	};

	typedef async_reader_mixout<win32_async_filereader>	async_filereader;

#ifdef PLAT_PC
	class win32_async_filewriter : OVERLAPPED, protected async_buffers<> {
	protected:
		HANDLE			hFile;
		volatile int	wrote;

		void completion(DWORD code, DWORD bytes) {
			wrote	= bytes ? bytes : 0xffffffff;
		}
		static VOID CALLBACK _completion(DWORD code, DWORD bytes, OVERLAPPED *overlapped) {
			((win32_async_filewriter*)overlapped)->completion(code, bytes);
		}
		bool	PutMore();
	public:
		win32_async_filewriter(const filename &fn);
		~win32_async_filewriter();
	};

	typedef async_writer_mixout<win32_async_filewriter>		async_filewriter;
	typedef writer_mixout<async_filewriter>				async_ostream;
#endif
} // namespace iso

#elif defined PLAT_PS3
//-----------------------------------------------------------------------------
//	PS3
//-----------------------------------------------------------------------------

#include <sys/timer.h>
#include <cell/cell_fs.h>

namespace iso {

	class ps3_async_filereader : CellFsAio, protected async_buffers<> {
		int				id;
		volatile uint32	read;

		void completion(CellFsErrno error, uint64_t bytes) {
			read	= errno == CELL_OK && bytes ? bytes : 0xffffffff;
		}
		static void _completion(CellFsAio *xaio, CellFsErrno error, int xid, uint64_t size) {
			((ps3_async_filereader*)xaio)->completion(error, size);
		}
		bool	GetMore();
	public:
		ps3_async_filereader(const filename &fn);
		~ps3_async_filereader();
	};

	typedef async_reader_mixout<ps3_async_filereader>	async_filereader;

} // namespace iso

#elif defined PLAT_WII
//-----------------------------------------------------------------------------
//	WII
//-----------------------------------------------------------------------------

#include "app.h"

namespace iso {

	class wii_async_filereader : DVDFileInfo, protected async_buffers<> {
		enum State {
			STATE_READING,
			STATE_DONE,
			STATE_RETRY
		};
		uint32			offset, read;
		volatile State	state;

		void callback(s32 result) {
			if (result < 0) {
	//			ISO_TRACE("DVD error %d\n", result);
				state	= STATE_RETRY;
				Application::SetError(result);
			} else {
				switch (int status = DVDGetFileInfoStatus(this)) {
					case DVD_STATE_END:
	//					ISO_TRACE("DVD ok\n");
						state	= STATE_DONE;
						break;
					case DVD_STATE_NO_DISK:
					case DVD_STATE_WRONG_DISK:
					case DVD_STATE_RETRY:
					case DVD_STATE_FATAL_ERROR:
	//					ISO_TRACE("DVD error %d\n", status);
						state	= STATE_RETRY;
						Application::SetError(status);
						break;
				}
			}
		}

		static void _callback(s32 result, DVDFileInfo *info) {
			((wii_async_filereader*)info)->callback(result);
		}

		bool	GetMore();
	public:
		wii_async_filereader(const filename &fn);
		~wii_async_filereader();
	};

	typedef async_reader_mixout<wii_async_filereader>	async_filereader;

} // namespace iso

#else

namespace iso {
typedef async_reader_mixout<_async_reader<filereader> > async_filereader;
}

#endif

namespace iso {
typedef reader_mixout<async_filereader>	async_istream;
}

#endif	// ASYNC_STREAM_H
