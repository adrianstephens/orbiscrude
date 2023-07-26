#include "base/vector.h"
#include "directory.h"
#include "codec/base64.h"
#include "codec/apple_compression.h"
#include "hashes/md5.h"
#include "filetypes/bitmap/bitmap.h"
#include "filetypes/sound/sample.h"
#include "iso/iso_binary.h"
#include "iso/iso_convert.h"
#include "iso/iso_files.h"
#include "platformdata.h"

using namespace iso;

struct crc32b {
	uint32 v;
	crc32b(uint32 _v) : v(_v) {}
	operator uint32() const { return v; }
};

ISO_DEFUSER(crc32b, xint32);
ISO_TYPEDEF(meters, float);

//-----------------------------------------------------------------------------
//	Trivial mappings
//-----------------------------------------------------------------------------

ISO_ptr<void> External(ISO_ptr<string> p) { return FileHandler::CachedRead((const char*)*p); }

ISO_ptr<void> DontMessWith(holder<ISO_ptr<void> > p) {
	if (p.t)
		p.t.SetFlags(ISO::Value::SPECIFIC);
	return p;
}

ISO_openarray<char> RepeatByte(int size, int value) {
	ISO_openarray<char> array(size);
	memset(array, value, size);
	return array;
}

ISO_ptr<void> SubArray(ISO_ptr<void> p, int start, int size) {
	const ISO::Type* type = p.GetType();
	if (TypeType(type) == ISO::OPENARRAY) {
		const int subsize = ((ISO::TypeOpenArray*)type)->subsize;
		const int align   = ((ISO::TypeOpenArray*)type)->SubAlignment();

		ISO_ptr<void> p2 = MakePtr(type, p.ID());

		ISO_openarray<void>* a0	= p;
		ISO_openarray<void>* a1	= p2;
		int					 size0 = a0->Count();

		if (size == 0)
			size = size0 - start;

		if (size > 0) {
			a1->Create(subsize, align, size);
			memcpy(a1->GetElement(0, subsize), a0->GetElement(start, subsize), subsize * size);
		}
		return p2;
	}
	return ISO_NULL;
}

ISO_ptr<void> UniqueArray(holder<ISO_ptr<void> > _p) {
	ISO_ptr<void>&  p	= _p;
	const ISO::Type* type = p.GetType()->SkipUser();
	if (TypeType(type) == ISO::OPENARRAY) {
		const ISO::Type* subtype = ((ISO::TypeOpenArray*)type)->subtype;
		const int		subsize = ((ISO::TypeOpenArray*)type)->subsize;
		const int		align   = ((ISO::TypeOpenArray*)type)->SubAlignment();

		ISO_ptr<void>		 p1   = Duplicate(p);
		ISO_openarray<void>* a1   = p1;
		int					 size = a1->Count();
		uint8*				 ed   = *a1;

		for (uint8 *e1 = *a1, *eend = e1 + size * subsize; e1 < eend; e1 += subsize) {
			bool match = false;
			for (uint8* e2 = *a1; !match && e2 < e1; e2 += subsize)
				match = CompareData(subtype, e1, e2);
			if (!match) {
				if (ed != e1)
					memcpy(ed, e1, subsize);
				ed += subsize;
			}
		}
		a1->Resize(subsize, align, uint32((ed - *a1) / subsize));
		return p1;
	}
	return ISO_NULL;
}

ISO_ptr<void> AppendArray(ISO_ptr<void> p1, ISO_ptr<void> p2) {
	const ISO::Type* type = p1.GetType()->SkipUser();
	if (TypeType(type) == ISO::OPENARRAY && p2.GetType()->SameAs(type)) {
		ISO_ptr<void> p3 = MakePtr(type, p1.ID());

		ISO_openarray<void>* a1	= p1;
		ISO_openarray<void>* a2	= p2;
		ISO_openarray<void>* a3	= p3;
		int					 size1 = a1->Count();
		int					 size2 = a2->Count();

		const int subsize = ((ISO::TypeOpenArray*)type)->subsize;
		const int align   = ((ISO::TypeOpenArray*)type)->SubAlignment();

		a3->Create(subsize, align, size1 + size2);
		memcpy(a3->GetElement(0, subsize), a1->GetElement(0, subsize), subsize * size1);
		memcpy(a3->GetElement(size1, subsize), a2->GetElement(0, subsize), subsize * size2);
		return p3;
	}
	return ISO_NULL;
}

ISO_openarray<xint32> Permutations(xint32 fixed, xint32 mask) {
	uint8				  nz = count_bits(~mask);
	ISO_openarray<xint32> v(1 << nz);
	uint32				  n = 0;
	for (auto& i : v) {
		i = (n & ~mask) | fixed;
		n = (n | mask) + 1;
	}
	return v;
}

ISO_ptr<ISO_openarray<uint8> > EmbedAs(ISO_ptr<void> p, const char* ext) {
	ISO_ptr<ISO_openarray<uint8> > a(p.ID());
	dynamic_memory_writer				   mo;
	FileHandler*				   fh = FileHandler::Get(ext);

	Platform::type t = Platform::Set(ISO::root("variables")["exportfor"].GetString());
	ISO::binary_data.SetBigEndian(!!ISO::root("variables")["bigendian"].GetInt(t & Platform::_PT_BE));

	fh->Write(p, mo);
	mo.data().copy_to(a->Create(mo.size32()));
	return a;
}

ISO_ptr<void> LoadAs(const ISO::unescaped<ISO::ptr_string<char,32>>& fn, const ISO::unescaped<ISO::ptr_string<char,32>>& ext) {
	if (FileHandler* fh = FileHandler::Get(ext)) {
		filename f(fn);
		return fh->ReadWithFilename(f.name_ext(), f);
		//		return FileHandler::AddToCache(fn.t, fh->ReadWithFilename(0, fn.t));
	}
	return ISO_NULL;
}

ISO_ptr<sample> AsSample(ISO_ptr<void> p, int channels, int bits, float frequency) {
	ISO::Browser b(p);
	if (b.GetType() == ISO::OPENARRAY) {
		uint32 length = b.Count() * b[0].GetTypeDef()->GetSize();
		uint32 bps	= (bits + 7) / 8 * channels;

		ISO_ptr<sample> s(0);
		memcpy(s->Create(length / bps, channels, bits), *b, length);
		s->SetFrequency(frequency);
		return s;
	}
	return ISO_NULL;
}

ISO_ptr<crc32b> MakeCRC32(ISO_ptr<string> p) { return ISO_ptr<crc32b>(p.ID(), CRC32::calc(*p)); }

bool RelativePathsRecurse(ISO::Browser b, const char* from, const char* to) {
	bool changes = false;
	for (ISO::Browser::iterator i = b.begin(), e = b.end(); i != e; ++i) {
		if (i->SkipUser().GetType() == ISO::REFERENCE) {
			ISO_ptr<void>* pp = *i;
			if (pp->IsExternal()) {
				string   spec = (const char*)*pp;
				filename fn;
				if (spec.begins(istr(from))) {
					spec = str(to) + spec.slice(uint32(strlen(from)));
					pp->CreateExternal(spec);
					changes = true;
				}
				if (const char* sc = spec.find(';')) {
					memcpy(fn, spec, sc - spec);
					fn[sc - spec] = 0;
				} else {
					fn = filename(spec);
				}

				if (fn.ext() == ".ib" || fn.ext() == ".ibz" || fn.ext() == ".ix") {
					if (ISO_ptr<void> p = FileHandler::Read(none, fn)) {
						if (p.Flags() & ISO::Value::HASEXTERNAL) {
							if (RelativePathsRecurse(ISO::Browser(p), from, to) && (fn.ext() == ".ib" || fn.ext() == ".ibz")) {
								if (!ISO::binary_data.Write(p, FileOutput(fn).me(), fn, ISO::BIN_RELATIVEPATHS | ISO::BIN_WRITEALLTYPES | ISO::BIN_DONTCONVERT | ISO::BIN_STRINGIDS))
									throw_accum("Couldn't write " << fn);
							}
						}
					}
				}
			} else if (pp->Flags() & ISO::Value::HASEXTERNAL) {
				changes |= RelativePathsRecurse(ISO::Browser(*pp), from, to);
			}
		} else if (!i->GetTypeDef()->IsPlainData()) {
			changes |= RelativePathsRecurse(*i, from, to);
		}
	}
	return changes;
}

ISO_ptr<void> RelativePaths(ISO_ptr<void> p, const char* from, const char* to) {
	ISO_ptr<void> save_keepexternals = ISO::root("variables")["keepexternals"];
	ISO::root("variables").SetMember("keepexternals", 1);
	if (p.IsExternal())
		p = FileHandler::ReadExternal(p);

	RelativePathsRecurse(ISO::Browser(p), from, to);
	ISO::root("variables").SetMember("keepexternals", save_keepexternals);
	return p;
}

void _WildCard(ISO_ptr<anything>& p, const filename& fn) {
	if (fn.dir().is_wild()) {
		filename fn2 = filename(fn).rem_dir().rem_dir();
		_WildCard(p, filename(fn2).add_dir(fn.name_ext()));

		for (directory_iterator name(filename(fn2).add_dir("*.*")); name; ++name) {
			if (name.is_dir() && name[0] != '.')
				_WildCard(p, filename(fn2).add_dir((const char*)name).add_dir("*").add_dir(fn.name_ext()));
		}

	} else {
		for (directory_iterator name(fn); name; ++name) {
			if (!name.is_dir())
				p->Append(FileHandler::Read((const char*)name, filename(fn).rem_dir().add_dir((const char*)name)));
		}
	}
}

ISO_ptr<anything> WildCard(string spec) {
	ISO_ptr<anything> p(0);
	_WildCard(p, spec);
	return p;
}

ISO_ptr<void> NoCompress(holder<ISO_ptr<void> > wp) {
	if (ISO_ptr<void>& p = wp) {
		if (p.GetType()->SameAs<sample>()) {
			((sample*)p)->flags |= sample::NOCOMPRESS;
			return p;
		} else {
			if (ISO_ptr<bitmap> bm = ISO_conversion::convert<bitmap>(p)) {
				bm->SetFlags(BMF_NOCOMPRESS);
				return bm;
			}
		}
	}
	return ISO_NULL;
}

//-----------------------------------------------------------------------------
//	Compression/Encoding
//-----------------------------------------------------------------------------

auto DecodeBase64(const malloc_block &a) {
	return transcode(base64_decoder(), a);
}

auto EncodeLZVN(const malloc_block &a) {
	return transcode(LZVN::encoder(), a);
}

auto DecodeLZVN(const malloc_block &a) {
	return transcode(LZVN::decoder(), a);
}

auto EncodeLZFSE(const malloc_block &a) {
	return transcode(LZFSE::encoder(), a);
}

auto DecodeLZFSE(const malloc_block &a) {
	return transcode(LZFSE::decoder(), a);
}

array<xint8, 16> HashMD5(const malloc_block &a) {
	MD5	md5(a);
	return md5.terminate();
}


//-----------------------------------------------------------------------------
//	PlatformToggle
//-----------------------------------------------------------------------------

ISO_ptr<void> PlatformToggle(ISO_ptr<void> checked, ISO_ptr<void> unchecked, uint8 platforms) {
	const char*	exp = ISO::root("variables")["exportfor"].GetString();
	PLATFORM_INDEX i
		= exp == str("x360")? PLATFORM_INDEX_X360
		: exp == str("ps3") ? PLATFORM_INDEX_PS3
		: exp == str("wii") ? PLATFORM_INDEX_WII
		: exp == str("ios") ? PLATFORM_INDEX_IOS
		: exp == str("pc")	? PLATFORM_INDEX_PC
		: exp == str("ps4") ? PLATFORM_INDEX_PS4
		: PLATFORM_INDEX_NONE;

	if (i != PLATFORM_INDEX_NONE && (platforms & (1 << i))) {
		if (checked)
			return checked;
	} else {
		if (unchecked)
			return unchecked;
	}
	return ISO_ptr<anything>(0);
}

//-----------------------------------------------------------------------------
//	init
//-----------------------------------------------------------------------------

static initialise init(
	ISO::getdef<crc32>(),
	ISO::getdef<crc32b>(),

	ISO_get_cast(MakeCRC32),

	ISO_get_conversion(External),

	ISO_get_operation(DontMessWith),
	ISO_get_operation(RepeatByte),
	ISO_get_operation(SubArray),
	ISO_get_operation(UniqueArray),
	ISO_get_operation(AppendArray),
	ISO_get_operation(Permutations),

	ISO_get_operation(EmbedAs),
	ISO_get_operation(LoadAs),
	ISO_get_operation(AsSample),
	ISO_get_operation_external(RelativePaths),
	ISO_get_operation(WildCard),

	ISO_get_operation(NoCompress),
	ISO_get_operation(PlatformToggle),

	ISO_get_operation(DecodeBase64),
	ISO_get_operation(EncodeLZVN),
	ISO_get_operation(DecodeLZVN),
	ISO_get_operation(EncodeLZFSE),
	ISO_get_operation(DecodeLZFSE),

	ISO_get_operation(HashMD5)
);
