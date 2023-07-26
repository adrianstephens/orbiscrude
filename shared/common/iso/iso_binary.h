#ifndef ISO_BINARY_H
#define ISO_BINARY_H

#include "iso.h"
#include "events.h"
#include "stream.h"

namespace ISO {

struct FileValue {
	uint32le		id;
	uint32le		type;
	uint32le		user;
	uint16			flags;
	uint16le		refs;
	FileValue() { clear(*this); }
};

struct FILE_HEADER : FileValue {
	enum { MAGIC = "ISOP"_u32 };
	FILE_HEADER()			{ id = MAGIC; }
	bool	Check() const	{ return id == MAGIC; }
};

enum BIN_FLAGS {
	BIN_EXPANDEXTERNALS	= 1 << 0,
	BIN_DONTCONVERT		= 1 << 1,
	BIN_STRINGIDS		= 1 << 2,
	BIN_WRITEREADTYPES	= 1 << 3,
	BIN_WRITEALLTYPES	= 1 << 4,
//	BIN_CLEARRAMBUFF	= 1 << 5,
	BIN_BIGENDIAN		= 1 << 6,
	BIN_RELATIVEPATHS	= 1 << 7,
	BIN_CHUNKEDREAD		= 1 << 8,
	BIN_ENUMS			= 1 << 9,
};

class BinaryData {
protected:
	void		*ram_buffer;
	size_t		ram_total;
public:
	BinaryData(void *p = 0, uint32 size = 0) : ram_buffer(p), ram_total(size)	{}
	size_t		Size()					const	{ return ram_total; }
	size_t		Write(ostream_ref file)	const;
	bool Write(const ptr_machine<void> &p, ostream_ref file, const char *fn = 0, uint32 flags = 0);
};

class BinaryData2 : public BinaryData {
	const char	*remote;
	bool		bigendian;
	int			mode;

	struct Save {
		BinaryData2	*bin;
		size_t		ram_total;
		Save(BinaryData2 *_bin) : bin(_bin), ram_total(_bin->ram_total) {}
		~Save() { bin->ram_buffer = iso::realloc(bin->ram_buffer, bin->ram_total = ram_total); }
	};
	static ptr_machine<void> _Fixup(Value *header, uint32 flags, const char *fn, void *phys_ram, uint32 phys_size);
	static Value *_ReadRaw(istream_ref file, vallocator &allocator, uint32 &phys_size, void *&phys_ram, bool directread);

public:

	Save	save()	{ return Save(this);	}

	void	*alloc(size_t size, size_t a) {
		size_t	start	= ram_total;
		size_t	offset	= align(start, (uint32)a);
		if (ram_total = offset + size) {
			ram_buffer	= iso::realloc(ram_buffer, ram_total);
			memset((char*)ram_buffer + start, 0, ram_total - start);
		}
		return (char*)ram_buffer + offset;
	}
	void	*realloc(void *p, size_t size, size_t a)		{ return 0;	}
	bool	free	(void *p)								{ return false; }
	bool	free	(void *p, size_t size)					{ return false; }
	void	transfer(void *d, const void *s, size_t size)	{ memcpy(d, s, size); }
	uint32	fix		(void *p, size_t size)					{ return uint32((char*)p - (char*)ram_buffer); }
	void*	unfix	(uint32 p)								{ return (char*)ram_buffer + p; }

	BinaryData2() : remote(0), bigendian(false), mode(0)	{ iso_bin_allocator().set(this); }

	void			SetRemoteTarget(const char *_remote)	{ remote = _remote;							}
	const char*		RemoteTarget()							{ return remote;							}

	void			SetBigEndian(bool _bigendian)			{ bigendian = _bigendian;					}
	bool			IsBigEndian()							{ return bigendian;							}

	int				SetMode(int _mode)						{ int m = mode; mode = _mode; return m;		}
	int				GetMode()								{ return mode;								}

	template<int B> static ptr<void,B>	Fixup(Value *header, tag2 id, uint32 flags = BIN_EXPANDEXTERNALS, void *phys_ram = 0, uint32 phys_size = 0);
	template<int B> static bool			Read(ptr<void, B> &p, tag id, istream_ref file, const char *fn = 0, uint32 flags = BIN_EXPANDEXTERNALS);
	template<int B> static ptr<void,B>	Read(tag id, istream_ref file, const char *fn = 0, uint32 flags = BIN_EXPANDEXTERNALS) {
		ptr<void,B>	p;
		Read(p, id, file, fn, flags);
		return p;
	}

	static ptr<void>	Read(tag id, istream_ref file, const char *fn = 0, uint32 flags = BIN_EXPANDEXTERNALS) {
		ptr<void>	p;
		Read(p, id, file, fn, flags);
		return p;
	}
};

template<int B> ptr<void,B>	BinaryData2::Fixup(Value *header, tag2 id, uint32 flags, void *phys_ram, uint32 phys_size) {
	if (header->flags & Value::PROCESSED)
		return GetPtr<B,void>(header + 1);
	if (phys_ram && phys_size == 0) {
		void	*iso_end	= !header->user ? phys_ram : (char*)header + (uint32)(uint32le&)header->user;
		phys_size 	= uint32((char*)phys_ram - (char*)iso_end);
		phys_ram	= iso_end;
	}
	header->id		= tag1();
	header->refs	= 0;
	header->user	= 0;
	header->flags	|= Value::ROOT;
	auto p = _Fixup(header, flags, 0, phys_ram, phys_size);
	p.SetID(id);
	return p;
}

template<int B> bool BinaryData2::Read(ptr<void, B> &p, tag id, istream_ref file, const char *fn, uint32 flags) {
	void	*phys_ram;
	uint32	phys_size;
	if (Value *v = _ReadRaw(file, allocate<B>::allocator(), phys_size, phys_ram, !(flags & BIN_CHUNKEDREAD))) {
		if (B == 32)
			v->flags |= Value::MEMORY32;
		p = _Fixup(v, flags, fn, phys_ram, phys_size);
		p.SetID(id);
		return true;
	}
	return false;
}

extern BinaryData2 binary_data;

class PtrBrowser2 : public PtrBrowser {
public:
	PtrBrowser2(Value &header, void *phys_ram = 0, uint32 phys_size = 0) : PtrBrowser(BinaryData2::Fixup<32>(&header, 0, 0, phys_ram, phys_size)) {
		header.flags -= Value::ROOT;
		--header.refs;
	}
};

}//namespace ISO

namespace iso {
	using ISO::PtrBrowser2;
	using ISO_binary = ISO::BinaryData;

} // namespace iso

#endif// ISO_BINARY_H
