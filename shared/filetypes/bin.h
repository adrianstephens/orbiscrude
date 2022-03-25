#ifndef BIN_H
#define BIN_H

#include "iso/iso.h"
#include "thread.h"	//for TLS
#include "stream.h"
#include "filename.h"
#include "vm.h"

//-----------------------------------------------------------------------------
//	type wrappers to help direct bin writer
//-----------------------------------------------------------------------------

namespace iso {
	struct BINwriter {
		uint64		base;
		uint64		offset;
		uint64		current;
		uint32		pointer_size;

		struct location {
			uint64		base;
			uint64		current;
			uint32		pointer_size;
		};
		struct deferred_item {
			ISO_ptr<void>			p;
			int						pri;
			dynamic_array<location>	locs;
			bool operator<(const deferred_item &b) const { return pri < b.pri; };
			deferred_item(ISO_ptr<void> _p, int _pri) : p(_p), pri(_pri) {}
		};
		dynamic_array<deferred_item> deferred;

		void	AddDeferred(ISO_ptr<void> p, int priority, uint32 offset = 0);
		bool	Writing()	const { return current != 0; }

		BINwriter() : base(0), offset(0), current(0), pointer_size(0) {}
	};

	extern TLS<BINwriter*>	bin_exporter;


	struct bin_relative_base {
		uint64	save;
		bin_relative_base()		{
			if (BINwriter *b = bin_exporter) {
				save	= b->base;
				b->base	= b->current;
			}
		}
		~bin_relative_base()	{
			if (BINwriter *b = bin_exporter)
				b->base = save;
		}
	};

	template<int N> struct iso_nil2 {
		static ISO_ptr<void, N>	_nil;
	};
	template<int N> ISO_ptr<void, N> iso_nil2<N>::_nil;

	//---------------------------------
	// bin_callback
	//---------------------------------

	template<typename T> struct bin_callback : T {};


	//---------------------------------
	// return_pointer
	//---------------------------------
	template<typename T, int N = 32> struct return_pointer : T {
		using T::operator=;
		return_pointer() {}
		template<typename T2> return_pointer(const T2 &t2) : T(t2) {}
		T	*operator->()	{ return this; }
		T	*get()			{ return this; }
	};
	template<typename T, int N> struct return_pointer<T*, N> : pointer<T> {
		using pointer<T>::operator=;
		return_pointer(T *_t) : pointer<T>(_t) {}
	};
	template<typename T, int N> struct return_pointer<pointer<T>, N> : pointer<T> {
		using pointer<T>::operator=;
	};
	template<typename T, typename B, int N> struct return_pointer<soft_pointer<T,B>, N> : soft_pointer<T,B> {
		using soft_pointer<T,B>::operator=;
	};
	template<int N, typename T> return_pointer<T,N> make_return_pointer(const T &t) {
		return t;
	}
	template<typename T> return_pointer<T,32> make_return_pointer(const T &t) {
		return t;
	}


	//---------------------------------
	// pointer_size
	//---------------------------------

	template<typename T, int N> struct pointer_size : T {
		using T::operator=;
		pointer_size() {}
		template<typename T2> pointer_size(const T2 &t2) : T(t2) {}
	};
	template<int N, typename T> pointer_size<T, N> make_pointer_size(const T &t) {
		return t;
	}
	template<int N> struct bin_pointer_size {
		int	save;
		bin_pointer_size()	{ if (bin_exporter) {save = bin_exporter->pointer_size; bin_exporter->pointer_size = N; } }
		~bin_pointer_size()	{ if (bin_exporter) bin_exporter->pointer_size = save; }
	};

	//---------------------------------
	// deferred
	//---------------------------------

	template<typename T, int N> struct deferred_bin {
		ISO_ptr<T>	t;
		deferred_bin() : t(0)		{}
		template<typename T2> deferred_bin(const T2 &t2) : t(0, t2) {}
		operator T()	const { return *t; }
		T&		get()	const { return *t; }
		void	add_reference(uint32 offset = 0) {
			if (bin_exporter)
				bin_exporter->AddDeferred(t, N, offset);
		}
	};
	template<int N> struct deferred_bin<void,N> {
		ISO_ptr<void>	t;
		deferred_bin() : t(0)			{}	// dummy
		void	add_reference(uint32 offset = 0) {
			if (bin_exporter)
				bin_exporter->AddDeferred(ISO_NULL, N, offset);
		}
	};
	template<int N, typename T> deferred_bin<T, N> make_deferred_bin(const T &t) {
		return t;
	}

	//---------------------------------
	// relative_base
	//---------------------------------

	template<typename T> struct relative_base : T {
		using T::operator=;
		relative_base() {}
		template<typename T2> relative_base(const T2 &t2) : T(t2) {}
	};
	template<typename T> relative_base<T> make_relative_base(const T &t) {
		return t;
	}
	template<int N, typename T> relative_base<return_pointer<T,N> > make_selfrelative(const T &t) {
		return t;
	}
	template<typename T> relative_base<return_pointer<T> > make_selfrelative(const T &t) {
		return t;
	}

	//---------------------------------
	// output_as
	//---------------------------------
	template<typename I, typename O> struct output_as {
		I		i;
		void	operator=(I _i)	{ i = _i; }
		operator I() const		{ return i; }
	};

	template<typename I, typename O> struct output_as<T_swap_endian<I>, O> {
		T_swap_endian<I>	i;
		void	operator=(I _i)	{ i = _i; }
		operator I() const		{ return i; }
	};


	//---------------------------------
	// export_as
	//---------------------------------
	template<typename I, typename O> struct export_as {
		I		i;
	};

	//---------------------------------
	// array_unspec
	//---------------------------------
	template<typename T> struct array_unspec : ISO::VirtualDefaults {
		T				*t;
		ISO::TypeArray	type;
		ISO::Browser		Deref()				{ return ISO::Browser(&type, (void*)t); }
		array_unspec(T *_t, uint32 n)		: t(_t), type(ISO::getdef<T>(), n, uint32(sizeof(T))) {}
		array_unspec(const range<T*> &r)	: t(r.begin()), type(ISO::getdef<T>(), uint32(r.size()), uint32(sizeof(T))) {}

		uint32			length()	const	{ return type.Count(); }
		operator T*()				const	{ return t; }
	};
	template<typename T> array_unspec<T> make_array_unspec(T *t, uint32 n) {
		return array_unspec<T>(t, n);
	}

	template<typename T> struct array_unspec_alloc : array_unspec<T> {
		using array_unspec<T>::t;
		using array_unspec<T>::type;
		array_unspec_alloc()				: array_unspec<T>(0, 0)				{}
		array_unspec_alloc(const array_unspec_alloc &b) : array_unspec<T>(b)	{ unconst(b).t = 0; }
		array_unspec_alloc(uint32 n)		: array_unspec<T>(new T[n], n)			{}
		~array_unspec_alloc()				{ delete[] t; }
		T*				create(uint32 n)	{ delete[] t; type.count = n; return t = new T[n]; }
	};

	template<typename T> struct array_unspec2 : array_unspec<T> {
		using array_unspec<T>::t;
		using array_unspec<T>::type;
		ISO::TypeArray	type2;
		ISO::Browser		Deref()		const	{ return ISO::Browser(&type2, t); }
		array_unspec2(T *_t, uint32 m, uint32 n)	: array_unspec<T>(_t, m), type2(&type, n, uint32(sizeof(T) * m)) {}
		array_unspec2(const array_unspec2 &b) : array_unspec<T>(b), type2(&type, b.type2.count) {}
		uint32			length()	const	{ return type2.Count(); }
	};
	template<typename T> array_unspec2<T> make_array_unspec2(T *t, uint32 m, uint32 n) {
		return array_unspec2<T>(t, m, n);
	}

	//---------------------------------
	// BinStream
	//---------------------------------

	struct BinStream : memory_reader {//memory_reader {
		ISO::Browser2	b;
		BinStream(const ISO::Browser2 &b) : memory_reader(memory_block(b[0], b.Count() * b[0].GetSize())), b(b) {}
		const ISO::Browser2&	_clone()	const { return b; }
//		istream*	clone() override { return new BinStream(b); }
	};

	//---------------------------------
	// BigBin
	//---------------------------------

	class BigBin : public ISO::VirtualDefaults {
		istream_ptr			file;
		uint64				length;
		uint32				block;
		dynamic_array<ISO::WeakRef<malloc_block> >	blocks;

		BigBin(istream_ptr &&file, uint64 length)	: file(move(file)), length(length) {
			block	= 1 << (log2(length * sizeof(ISO::weak)) / 2);
			blocks.resize((length + block - 1) / block);
		}
	public:
		BigBin(istream_ref f)		: BigBin(f.clone(), f.length())				{}
		BigBin(const filename &fn)	: BigBin(new FileInput(fn), filelength(fn)) {}

		uint32			Count() const {
			return uint32(blocks.size());
		}
		ISO_ptr<void>	Index(int i) {
			ISO_ptr_machine<malloc_block>	p;
			if (!(p = blocks[i])) {
				uint64	offset	= uint64(i) * block;
				uint32	size	= uint32(min(length - offset, block));
				file.seek(offset);
				file.readbuff(*p.Create(tag(), size), size);
				blocks[i] = p;
			}
			return p;
		}
	};

	struct BigBinCache {
		struct Block {
			uint32			index;
			ISO::Browser2	data;

			memory_block	get(const ISO::Browser2 &b, uint32 i) {
				if (index != i) {
					index	= i;
					data	= b[i];
				}
				return data ? memory_block(data[0], data.Count()) : empty;
			}
			Block() : index(~0u) {}
		};
		Block			blocks[2];
		uint32			block_size;

		BigBinCache(uint32 _block_size = 0) : block_size(_block_size) {}

		void			flush() {
			blocks[0].index = blocks[1].index = ~0u;
		}
		void			reset(uint32 _block_size) {
			block_size = _block_size;
			flush();
		}
		memory_block	get_mem(ISO::Browser2 &b, uint64 address) {
			uint32	i	= uint32(address / block_size);
			return blocks[i & 1].get(b, i).slice(address % block_size);
		}
	};

	struct BigBinStream0 : BigBinCache, stream_defaults<BigBinStream0> {
		ISO::Browser2	b;
		uint64			pos, end;

		BigBinStream0(const ISO::Browser2 &_b) : BigBinCache(_b[0][0].GetSize() * _b[0].Count()), b(_b), pos(0) {
			uint32	num	= b.Count();
			end	= uint64(block_size) * (num - 1) + b[num - 1][0].GetSize() * b[num - 1].Count();
		}
		size_t			readbuff(void *buffer, size_t size) {
			size_t	total = 0;
			while (total < size) {
				if (pos >= end)
					break;
				memory_block m	= get_mem(b, pos);
				size_t		n	= min(size - total, m.length());
				if (n == 0)
					break;
				memcpy((uint8*)buffer + total, m, n);
				total	+= n;
				pos		+= n;
			}
			return int(total);
		}
		int				getc() {
			return pos < end ? *(uint8*)get_mem(b, pos++) : EOF;
		}
		void			seek(streamptr offset)	{ pos = offset; }
		streamptr		tell()					{ return pos; }
		streamptr		length()				{ return end; }

		const_memory_block_own	get_block(size_t size) {
			size = min(size, end - pos);
			memory_block m	= get_mem(b, pos);
			if (m.length() >= size) {
				pos += size;
				return m.slice_to(size);
			}
			return malloc_block(*this, size);
		}
	};

//	typedef reader_mixout<BigBinStream0> BigBinStream;
	typedef BigBinStream0 BigBinStream;


	struct mapped_anything : ISO::VirtualDefaults {
		mapped_file	m;
		anything	a;

		mapped_anything(const filename &fn)	: m(fn) {}
		mapped_anything(istream_ref file)	: m(read_all(unconst(file))) {}

		uint32			Count()								{ return a.Count();		}
		tag2			GetName(int i)						{ return a[i].ID();		}
		int				GetIndex(const tag2 &id, int from)	{ return a.GetIndex(id, from);}
		ISO::Browser2	Index(int i)						{ return a[i];			}
	};

	inline bool IsRawData(const ISO::Type *type) {
		return type && type->GetType() == ISO::OPENARRAY && type->SubType()->IsPlainData();
	}
	inline bool IsRawData(const ISO::Browser2 &b) {
		return !b.External() && IsRawData(b.GetTypeDef());
	}
	inline memory_block GetRawData(const ISO::Browser &b) {
		return memory_block(b[0], b.Count() * b[0].GetSize());
	}
	inline memory_block GetRawData(ISO_ptr<void> p) {
		return IsRawData(p.GetType()) ? GetRawData(ISO::Browser(p)) : empty;
	}

	inline int IsRawData2(const ISO::Browser2 &b) {
		return IsRawData(b) ? 1 : b.Is("Bin") ? 2 : b.Is("BigBin") ? 3 : 0;
	}

	inline int FindRawData(ISO::Browser2 &b) {
		int		bin	= IsRawData2(b);
		while (!bin /* && b.SkipUser().IsVirtPtr()*/ && (b = *b))
			bin	= IsRawData2(b);
		return bin;
	}
}

namespace ISO {
	template<typename T> struct def<bin_callback<T> > : public ISO::VirtualT2<bin_callback<T> > {};
		template<class C, class T> Browser2 MakeBrowserCallback(const T &t)	{
		return Browser2(MakeBrowser(t), ptr<bin_callback<C> >(tag2()));
	};

	template<typename T, int N> struct def<return_pointer<T, N> > : public ISO::VirtualT2<return_pointer<T, N> > {
		static Browser2	Deref(return_pointer<T, N> &a)	{
			if (a.get())
				return MakePtr(tag2(), MakePtr<N>(tag2(), a.get()));
			return MakePtr(tag2(), iso_nil2<N>::_nil);
		}
	};
	template<typename T, int N> inline Browser MakeBrowser(const return_pointer<T,N> &t) {
		if (&t)
			return Browser(getdef<return_pointer<T,N> >(), (void*)&t);
		return MakeBrowser(iso_nil2<N>::_nil);
	}

		template<class T, int N> struct def<pointer_size<T, N> > : ISO::VirtualT2<pointer_size<T, N> > {
		static Browser2	Deref(pointer_size<T, N> &a) {
			return MakeBrowserCallback<bin_pointer_size<N> >((T&)a);
		}
	};
	template<class T, int N> struct def<deferred_bin<T, N> > : ISO::VirtualT2<deferred_bin<T, N> > {
		static Browser2	Deref(deferred_bin<T, N> &a) {
			a.add_reference();
			if (bin_exporter)
				return ISO::Browser2();
			return a.t;
		}
	};
#if 1
	template<typename T> struct def<relative_base<T> > : TypeComposite {
		ISO::Element	element;
		def() : TypeComposite(1, RELATIVEBASE), element(0, getdef<T>(), 0, sizeof(T)) {}
	};
#else
	struct do_relative_base : virtual_defaults {
		do_relative_base()	{ if (bin_exporter) bin_exporter->pointer_size = 16; }
		~do_relative_base()	{ if (bin_exporter) bin_exporter->pointer_size = 32; }
	};
	DEFVIRT(do_relative_base);
	template<class T> struct def<relative_base<T> > : ISO::VirtualT2<relative_base<T> > {
		static Browser2	Deref(relative_base<T> &a) { return Browser2(MakeBrowser((T&)a), ptr<do_relative_base>(0)); }
	};
#endif
		template<typename I, typename O> struct def<output_as<I,O> > : ISO::VirtualT2<output_as<I,O> > {
		static Browser2	Deref(const output_as<I,O> &a)	{ return MakePtr(tag2(), (O)a.i); }
	};
			template<typename I, typename O> struct def<export_as<I,O> > : ISO::VirtualT2<export_as<I,O> > {
		static Browser2	Deref(const export_as<I,O> &a)	{
			if (bin_exporter)
				return MakeBrowser((O&)a.i);
			return MakeBrowser(a.i);
		}
	};

	template<typename T> struct def<array_unspec<T> > : ISO::VirtualT<array_unspec<T> >{};
	template<typename T> struct def<array_unspec_alloc<T> > : ISO::VirtualT<array_unspec_alloc<T> >{};
	template<typename T> struct def<array_unspec2<T> > : ISO::VirtualT<array_unspec2<T> >{};

	ISO_DEFUSERVIRT(BigBin);

		template<> struct def<mapped_file> : TypeUser {
		struct Virt : ISO::VirtualT1<mapped_file,Virt> {
			typedef uint8	block[1024];
			static uint32	Count(mapped_file &m)								{ return uint32(m.length() / sizeof(block)); }
			static Browser2	Index(mapped_file &m, int i)						{ return MakeBrowser(((block*)m)[i]); }

			static tag2		GetName(mapped_file &m, int i)						{ return tag2(); }
			static int		GetIndex(mapped_file &m, const tag2 &id, int from)	{ return -1;	}
			static Browser2	Deref(mapped_file &m)								{ return Browser();	}
			static void		Delete(mapped_file &m)								{}
			static bool		Update(mapped_file &m, const char *s, bool from)	{ return false; }
		} v;

		def() : TypeUser("BigBin", &v)	{}
	};

	ISO_DEFVIRT(mapped_anything);

}  // namespace ISO

#endif //BIN_H
