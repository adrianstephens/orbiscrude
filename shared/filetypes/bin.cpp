#include "iso/iso_files.h"
#include "iso/iso_binary.h"
#include "base/bits.h"
#include "base/algorithm.h"
#include "vm.h"
#include "bin.h"

using namespace iso;

namespace iso {
TLS<BINwriter*>	bin_exporter;
}

void BINwriter::AddDeferred(ISO_ptr<void> p, int priority, uint32 offset) {
	if (current) {
		deferred_item	*d = 0;
		for (auto i = deferred.begin(), e = deferred.end(); !d && i != e; ++i) {
			if (i->p == p)
				d = i;
		}
		if (!d)
			d = new(deferred) deferred_item(p, priority);

		location	*loc = new(d->locs) location;
		loc->base			= base - offset;
		loc->current		= current;
		loc->pointer_size	= pointer_size ? pointer_size : 32;
	}
}

struct SizeAlign {
	uint32	size, align;

	SizeAlign() : size(0), align(1) {}
	SizeAlign(uint32 _size, uint32 _align) : size(_size), align(_align) {}

	SizeAlign(const ISO::Type *type) : size(type->GetSize()), align(type->GetAlignment()) {}

	SizeAlign&	operator+=(const SizeAlign &b) {
		align	= iso::align(align, b.align);
		size	= iso::align(size, b.align) + b.size;
		return *this;
	}
	SizeAlign	operator*(int n) {
		return SizeAlign(size * n, align);
	}
	SizeAlign	operator+(int n) {
		return SizeAlign(size + n, align);
	}
};

class BINwriterImp : BINwriter {
	ostream_ref	file;
	bool			flat;
	bool			big;

	struct name {
		ISO::WeakRef<void>	p;
		uint64				o;
		name() : o(0)	{}
		name(const ISO_ptr<void> &_p, uint64 _o) : p(_p), o(_o)	{}
	};
	order_array<name>	names;

	void		DumpReference(const ISO::Browser &b, bool flip);
	void		DumpData(const ISO::Browser &b, bool flip);

	SizeAlign	CalcSizeAlignRef(const ISO::Browser &b);
	SizeAlign	CalcSizeAlign(const ISO::Browser &b);
	uint32		CalcSize(const ISO::Browser &b)		{ return CalcSizeAlignRef(b).size; };

	bool		WriteOffset(uint64 v, uint32 pointer_size) const {
		if (this->pointer_size)
			pointer_size = this->pointer_size;
		if (v) {
			v -= base;
			if (big != iso_bigendian)
				v = swap_endian(v);
		}
		return check_writebuff(file, big ? (char*)&v + (64 - pointer_size) / 8 : (char*)&v, pointer_size / 8);
	}

public:
	BINwriterImp(ostream_ref _file, ISO_ptr_machine<void> p) : file(_file) {
		bin_exporter	= this;
		ISO::Browser b	= ISO::Browser(p);

		big		= ISO::binary_data.IsBigEndian();
		flat	= b.Is("BigBin");

		while (b.IsVirtPtr()) {
			b		= *b;
			flat	= flat || b.Is("BigBin");
		}

		if (!flat) {
			b		= b.SkipUser();
			flat	= (b.GetType() == ISO::OPENARRAY && b.GetTypeDef()->SubType()->IsPlainData()) || b.GetType() == ISO::STRING;
		}

		base	= file.tell();
		offset	= flat ? base : base + CalcSize(b);

		DumpReference(b, !(p.Flags() & ISO::Value::ISBIGENDIAN) == big);
	}
	~BINwriterImp() {
		sort(deferred);
		pointer_size	= 0;
		offset			= align(offset, 16);

		for (auto i = deferred.begin(), i1 = deferred.end(); i != i1; ++i) {
			ISO::Browser	b2(i->p);
			SizeAlign	sa			= CalcSizeAlignRef(b2);
			uint64		location	= align(offset, sa.align);

			offset = location + sa.size;
			file.seek(location);
			DumpReference(b2, !(i->p.Flags() & ISO::Value::ISBIGENDIAN) == big);

			for (auto j = i->locs.begin(), j1 = i->locs.end(); j != j1; ++j) {
				file.seek(j->current);
				base			= j->base;
				WriteOffset(location, j->pointer_size);
			}
		}
		if (!flat)
			file.seek_end(0);

		bin_exporter	= 0;
	}
};

bool HasAccessor(const ISO::TypeComposite &comp);

bool IsAccessor(const ISO::Type *type) {
	if (type = type->SkipUser()) {
		switch (type->GetType()) {
			case ISO::COMPOSITE: return HasAccessor(*(const ISO::TypeComposite*)type);
			case ISO::ARRAY:	return IsAccessor(type->SubType());
			case ISO::VIRTUAL:	return true;
			default:			break;
		}
	}
	return false;
}

bool IsAccessor(const ISO::Element *element) {
	return element->size == 0 || IsAccessor(element->type);
}

bool HasAccessor(const ISO::TypeComposite &comp) {
	for (const ISO::Element *i = comp.begin(), *e = comp.end(); i != e; ++i) {
		if (IsAccessor(i))
			return true;
	}
	return false;
}

SizeAlign BINwriterImp::CalcSizeAlign(const ISO::Browser &b) {
	switch (b.GetType()) {
		case ISO::STRING:
			if (const char *s = b.GetString())
				return SizeAlign(uint32(strlen(s) + 1), 1);
			return SizeAlign(1, 1);

		case ISO::COMPOSITE: {
			const ISO::TypeComposite &comp = *(ISO::TypeComposite*)b.GetTypeDef();
			if (comp.flags & ISO::TypeComposite::FORCEOFFSETS)
				return SizeAlign(&comp);
			bool		accessor	= false;
			SizeAlign	sa(0, 1 << comp.param1);
			for (const ISO::Element *i = comp.begin(), *e = comp.end(); i != e; ++i) {
				accessor	= accessor || IsAccessor(i);
				if (accessor)
					sa += CalcSizeAlign(ISO::Browser(i->type, (char*)b + i->offset));
				else
					sa += SizeAlign(i->type);
			}
			return sa;
		}
		case ISO::ARRAY:
			return CalcSizeAlign(b[0]) * b.Count();

		case ISO::VIRTUAL: {
			ISO::Virtual	*virt	= (ISO::Virtual*)b.GetTypeDef();
			if (ISO::Browser2 b2 = virt->Deref(virt, b))
				return CalcSizeAlign(b2);

			SizeAlign	sa;
			uint32		n		= virt->Count(virt, b);
			for (int i = 0; i < n; i++)
				sa += CalcSizeAlign(b[i]);
			return sa;
		}
		case ISO::USER:
			return CalcSizeAlign(b.SkipUser());

		case ISO::REFERENCE:
			if (pointer_size)
				return SizeAlign(pointer_size / 8, pointer_size / 8);

		default:
			return SizeAlign(b.GetTypeDef());
	}
}

SizeAlign BINwriterImp::CalcSizeAlignRef(const ISO::Browser &b) {
	switch (b.GetType()) {
		case ISO::OPENARRAY:
			if (int count = b.Count())
				return CalcSizeAlign(b[0]) * count + 4;
			return SizeAlign();

		case ISO::REFERENCE:
			return CalcSizeAlign(*b);

		case ISO::USER:
			return CalcSizeAlignRef(b.SkipUser());

		default:
			return CalcSizeAlign(b);
	}
};

void BINwriterImp::DumpReference(const ISO::Browser &b, bool flip) {
	switch (b.GetType()) {
		case ISO::STRING:
			file.write(b.GetString());
			break;

		case ISO::OPENARRAY:
			if (int count = b.Count()) {
				ISO::TypeOpenArray	*array = (ISO::TypeOpenArray*)b.GetTypeDef();
				if (!flat) {
					if (big)
						file.write(uint32be(count));
					else
						file.write(uint32le(count));
				}
				if (array->subtype->IsPlainData(flip)) {
					file.writebuff(array->ReadPtr(b), count * array->subsize);
					//file.writebuff(*(iso_ptr32<void>*)b, count * array->subsize);
				} else {
					for (int i = 0; i < count; i++)
						DumpData(b[i], flip);
				}
			}
			break;

		case ISO::REFERENCE: {
			ISO::Browser		b2	= *b;
			ISO_ptr<void>	p	= b2;
			DumpData(b2, !(p.Flags() & ISO::Value::ISBIGENDIAN) == big);
			break;
		}

		default:
			DumpData(b, flip);
			break;
	}
}

void BINwriterImp::DumpData(const ISO::Browser &_b, bool flip) {
	ISO::Browser	b = _b.SkipUser();
	if (b && b.GetTypeDef()->IsPlainData(flip)) {
		file.writebuff(b, b.GetSize());
		return;
	}

	current	= file.tell();

	switch (b.GetType()) {
		case ISO::INT:
		case ISO::FLOAT:
			switch (b.GetSize()) {
				case 2:	file.write(swap_endian(*(uint16*)b)); break;
				case 4:	file.write(swap_endian(*(uint32*)b)); break;
				case 8:	file.write(swap_endian(*(uint64*)b)); break;
			}
			break;

		case ISO::STRING:
			if (flat) {
				if (const char *s = b.GetString())
					file.writebuff(s, strlen(s));
			} else if (const char *s = b.GetString()) {
				WriteOffset(offset, b.GetTypeDef()->Is64Bit() ? 64 : 32);

				streamptr	here = file.tell();
				file.seek(offset);
				file.writebuff(s, strlen(s) + 1);
				offset = (uint32)file.tell();
				file.seek(here);
			} else {
				WriteOffset(0, b.GetTypeDef()->Is64Bit() ? 64 : 32);
			}
			break;

		case ISO::COMPOSITE: {
			const ISO::TypeComposite	&comp	= *(ISO::TypeComposite*)b.GetTypeDef();
			streamptr	start		= current;
			uint64		savebase	= base;
			bool		accessor	= false;
			bool		force_offs	= !!(comp.flags & ISO::TypeComposite::FORCEOFFSETS);
			uint32		offset		= 0;

			if (comp.flags & ISO::TypeComposite::RELATIVEBASE)
				base = start;

			for (const ISO::Element *i = comp.begin(), *e = comp.end(); i != e; ++i) {
				ISO::Browser	f(i->type, (char*)b + i->offset);
				uint32		size = i->size;
				if (!flat) {
					if (!force_offs)
						accessor = accessor || IsAccessor(i);

					if (accessor || size == 0) {
						current		= 0;
						SizeAlign	sa = CalcSizeAlign(f);
						offset		= align(offset, sa.align);
						size		= sa.size;
					} else {
						offset		= i->offset;
					}
				}
				file.seek(current = start + offset);
				if (i->type)
					DumpData(f, flip);
				else
					file.writebuff(f, size);
				offset		+= size;
			}
			base = savebase;
			break;
		}
		case ISO::VIRTUAL: {
			ISO::Virtual	*virt = (ISO::Virtual*)b.GetTypeDef();
			if (ISO::Browser2 b2 = virt->Deref(virt, b)) {
				DumpData(b2, flip);
				break;
			}

			uint32		count = virt->Count(virt, b);
			if (flat) {
				ISO::Browser		b2	= b[0];
				if (b2.GetTypeDef()->IsPlainData(flip)) {
					file.writebuff(b2, b2.GetSize() * count);
				} else {
					for (int i = 0; i < count; i++)
						DumpData(b[i], flip);
				}

			} else {
				for (int i = 0; i < count; i++) {
					current	= 0;
					file.align(CalcSizeAlign(b[i]).align);
					current = file.tell();
					DumpData(b[i], flip);
				}
			}
			break;
		}

		case ISO::ARRAY:
			for (int i = 0, c = b.Count(); i < c; i++)
				DumpData(b[i], flip);
			break;

		case ISO::OPENARRAY:
			if (flat) {
				DumpReference(b, flip);
			} else {
				uint32	pointer_size = b.GetTypeDef()->Is64Bit() ? 64 : 32;
				if (int count = b.Count()) {
					current			= 0;
					SizeAlign	sa	= CalcSizeAlign(b[0]);
					offset			= align(offset, sa.align);
					WriteOffset(offset, pointer_size);

					streamptr	here = file.tell();
					file.seek(offset);
					offset			+= sa.size * count;

					DumpReference(b, flip);
					file.seek(here);
				} else {
					WriteOffset(0, pointer_size);
				}
			}
			break;

		case ISO::REFERENCE: {
			uint32	pointer_size = b.GetTypeDef()->Is64Bit() ? 64 : 32;
			ISO::Browser2	b2	= (*b).SkipUser();
			if (ISO_ptr<void> p = b2) {
				if (flat) {
					DumpReference(b2, !(p.Flags() & ISO::Value::ISBIGENDIAN) == big);
				} else {
					uint64		location = 0;
					const ISO::Type	*subtype = ((ISO::TypeReference*)b.GetTypeDef())->subtype;
					if (!subtype)
						subtype = p.GetType();
					for (int i = 0; i < names.size(); i++) {
						if (names[i].p == p) {// && names[i].t == subtype) {
							location = names[i].o;
							break;
						}
					}
					if (!location) {
	//					p = FileHandler::ExpandExternals(p);
	//					p = ISO_conversion::convert(p, subtype);

						if (p) {
							current	= 0;
							SizeAlign	sa = CalcSizeAlignRef(b2);
							location = align(offset, sa.align);

							names.emplace_back(p, location);

							streamptr	here	= file.tell();
							file.seek(location);
							offset = location + sa.size;
							DumpReference(b2, !(p.Flags() & ISO::Value::ISBIGENDIAN) == big);
							file.seek(here);
						}
					}
					WriteOffset(location, pointer_size);
				}
			} else if (!flat) {
				WriteOffset(0, pointer_size);
			}
			break;
		}

	}
}

#include "vm.h"

class BigBin_mapped : public ISO::VirtualDefaults {
	enum {BLOCK = 65536};//uint32			block;
	typedef	ISO::Array<uint8,BLOCK> block_t;
	mapped_file		map;
public:
	BigBin_mapped(const filename &fn) : map(fn, 0, 0, true) {
		if (!map)
			map = mapped_file(fn, 0, 0, false);
	}
	uint32			Count() const {
		return uint32(div_round_up(map.length(), BLOCK));
	}
	ISO::Browser2	Index(int i) const {
		uint8	*p	= map;

		if (i == map.length() / BLOCK)
			return ISO::MakePtr(none, memory_block(p + i * uint64(BLOCK), map.length() % BLOCK));

		return ISO::MakeBrowser(*(block_t*)(p + i * uint64(BLOCK)));
	}
};
ISO_DEFUSERVIRTX(BigBin_mapped, "BigBin");

class BINFileHandler : public FileHandler {
	const char*		GetExt() override { return "bin";			}
	const char*		GetDescription() override { return "Raw Binary";	}
	int				Check(istream_ref file) override { return CHECK_POSSIBLE;}

	ISO_ptr<void>	ReadWithFilename(tag id, const filename &fn) override {
		uint64	length = filelength(fn);
#ifdef ISO_EDITOR
		if (length >= 1024 * 1024) {
			//return ISO_ptr<BigBin>(id, fn);
			ISO_ptr<BigBin_mapped>	p(id, fn);
			if (p->Count())
				return p;
			return ISO_NULL;
		}
#endif
		ISO_ptr<ISO_openarray_machine<xint8> >	p(id);
		FileInput(fn).readbuff(p->Create(uint32(length)), uint32(length));
		return p;
	}

	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		uint32	length = file.size32();
		ISO_ptr<ISO_openarray_machine<xint8> >	p(id);
		file.readbuff(p->Create(length), length);
		return p;
	}
	bool			Write(ISO_ptr<void> p, ostream_ref file) override {
		BINwriterImp(file, p);
		return true;
	}
	bool			Write64(ISO_ptr64<void> p, ostream_ref file) override {
		BINwriterImp(file, p);
		return true;
	}
} bin;

