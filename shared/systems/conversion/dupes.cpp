#include "iso/iso_files.h"
#include "iso/iso_convert.h"
#include "directory.h"
#include "base/tree.h"
#include "base/bits.h"

//#define DEBUG_DUPES

using namespace iso;

//-----------------------------------------------------------------------------
//	RemoveDupes
//-----------------------------------------------------------------------------

#ifdef DEBUG_DUPES
#include "stream.h"
#endif

enum {
	DUPES_DONTMERGE_NULLID	= 1,
};

struct ISO_route : buffer_accum<1024> {
	struct node {
		ISO_route	&r;
		char		*p;
		node(ISO_route &_r, int i)			: r(_r)	{ p = r.getp(); r << '[' << i << ']'; }
		node(ISO_route &_r, const char *s)	: r(_r)	{ p = r.getp(); r << onlyif(p[-1] != ';', '.') << s; }
		~node()										{ r.move(int(p - r.getp()));		}
		operator ISO_route&()						{ return r;		}
	};
	node	add_node(int i)			{ return node(*this, i); }
	node	add_node(const char *s)	{ return node(*this, s); }
	ISO_route()						{}
	ISO_route(const char *prefix)	{ *this << prefix << ';'; }
};

class ISO_dupes {
	int										tempids;
	int										flags;
	dynamic_array<ISO_ptr_machine<void> >	ptrs;
	map<ISO_ptr_machine<void>, ISO_ptr_machine<void> >	mapping;

	tag		GenerateID();
	bool	CheckInside(const ISO::Type *type, void *data);
	bool	CheckInside(const ISO::Type *type, void *data, ISO_route &route);

#ifdef DEBUG_DUPES
	struct DebugFile : FileOutput {
		DebugFile() : FileOutput(root("variables")["dumpdupes"].GetString("dumpdupes.log")) {}
		void	output(ISO_ptr<void> p1, ISO_ptr<void> p2) {
			if (exists())
				write(buffer_accum<256>() << "Merging " << p1.ID() << " and " << p2.ID() << " type:" << p1.GetType() << '\n');
		}
	} debug;
#endif

public:
	void*			CheckPtr(const ISO_ptr_machine<void> &p);
	void*			CheckPtr(const ISO_ptr_machine<void> &p, ISO_route &route);
	bool			CheckIDs(const ISO_ptr<void> &p1, const ISO_ptr<void> &p2);
	void			ClearMap()			{ mapping.clear();		}
	size_t			Count()		const	{ return ptrs.size();	}
	ISO_ptr_machine<void>	&operator[](int i)	{ return ptrs[i];		}

	ISO_dupes(int _flags = 0) : tempids(0), flags(_flags)	{}
};

tag ISO_dupes::GenerateID() {
	return format_string("_dup%i", tempids++);
}

bool ISO_dupes::CheckIDs(const ISO_ptr<void> &p1, const ISO_ptr<void> &p2) {
	bool	blank1	= !p1.ID() || (p1.Flags() & ISO::Value::ALWAYSMERGE);
	bool	blank2	= !p2.ID() || (p2.Flags() & ISO::Value::ALWAYSMERGE);

	return	(!(flags & DUPES_DONTMERGE_NULLID) && (blank1 || blank2))
		||	p1.ID() == p2.ID();
/*
	return	(p1.Flags() & ISO::Value::ALWAYSMERGE)
		||	(p2.Flags() & ISO::Value::ALWAYSMERGE)
		||	(!(flags & DUPES_DONTMERGE_NULLID) && (!p1.ID() || !p2.ID()))
		||	p1.ID() == p2.ID();
*/}

bool ISO_dupes::CheckInside(const ISO::Type *type, void *data) {
	if (type && (type = type->SkipUser())) switch (type->GetType()) {
		case ISO::COMPOSITE: {
			ISO::TypeComposite	&comp	= *(ISO::TypeComposite*)type;
			bool				need	= false;
			for (auto &e : comp) {
				if (CheckInside(e.type, (char*)data + e.offset))
					need = true;
			}
			return need;
		}

		case ISO::ARRAY: {
			ISO::TypeArray	*array = (ISO::TypeArray*)type;
			for (int i = 0, c = array->Count(); i < c; i++) {
				if (!CheckInside(array->subtype, (char*)data + array->subsize * i))
					return false;
			}
			return true;
		}

		case ISO::OPENARRAY:
			if (data = type->ReadPtr(data)) {
				ISO::TypeOpenArray			*array	= (ISO::TypeOpenArray*)type;
				const ISO::OpenArrayHead	*h		= ISO::OpenArrayHead::Get(data);
				size_t						stride	= array->subsize;
				for (int c = GetCount(h); c--; data = (uint8*)data + stride) {
					if (!CheckInside(array->subtype, data))
						return false;
				}
			}
			return true;

		case ISO::REFERENCE:
			if (void *pdata = type->ReadPtr(data)) {
				auto	ref = (ISO::TypeReference*)type;
				auto	p 	= ISO::GetPtr<64>(pdata);
				if (!p.IsExternal()) {
					if (mapping.find(p)) {
						ref->set(data, mapping[p]);

					} else if (void *p2 = CheckPtr(p)) {
						ref->set(data, ISO::GetPtr<64>(p2));
						if (!p.ID()) {
							p.SetID(GenerateID());
							p.SetFlags(ISO::Value::ALWAYSMERGE);
						}
					}
				}
			}
			return true;
	};
	return false;
}

void* ISO_dupes::CheckPtr(const ISO_ptr_machine<void> &p) {
//	ISO_ptr<void>	p = rp.Flags() & ISO::Value::REDIRECT ? *ISO_ptr<ISO_ptr<void> >(rp) : rp;

	if (p.Flags() & ISO::Value::CRCTYPE)
		return nullptr;

	mapping[p] = p;
	const ISO::Type	*type	= p.GetType();
	CheckInside(type, p);

	if (TypeType(type) != ISO::VIRTUAL) {
		for (size_t i = 0, n = ptrs.size(); i < n; i++) {
			if (ptrs[i].GetType()->SameAs(type, ISO::MATCH_NOUSERRECURSE) && CompareData(type, p, ptrs[i], 0) && CheckIDs(p, ptrs[i])) {
#ifdef DEBUG_DUPES
				debug.output(p, ptrs[i]);
#endif
				mapping[p] = ptrs[i];
				if (p.ID() && (!ptrs[i].ID() || ((ptrs[i].Flags() & ISO::Value::ALWAYSMERGE) && !(p.Flags() & ISO::Value::ALWAYSMERGE))))
					ptrs[i].SetID(p.ID());
				return p;/* = ptrs[i];
				if (!p.ID()) {
					p.SetID(GenerateID());
					p.SetFlags(ISO::Value::ALWAYSMERGE);
				}
				return;
				*/
			}
		}
	}
	ptrs.push_back(p);
	return nullptr;
}

bool ISO_dupes::CheckInside(const ISO::Type *type, void *data, ISO_route &route) {
	if (!type || !(type = type->SkipUser()))
		return false;

	switch (type->GetType()) {
		case ISO::COMPOSITE: {
			ISO::TypeComposite	&comp	= *(ISO::TypeComposite*)type;
			bool				need	= false;
			for (auto &e : comp) {
				if (CheckInside(e.type, (char*)data + e.offset, route.add_node(comp.index_of(e))))
					need = true;
			}
			return need;
		}

		case ISO::ARRAY: {
			ISO::TypeArray	*array = (ISO::TypeArray*)type;
			for (int i = 0, c = array->Count(); i < c; i++) {
				if (!CheckInside(array->subtype, (char*)data + array->subsize * i, route.add_node(i)))
					return false;
			}
			return true;
		}

		case ISO::OPENARRAY:
			if (data = type->ReadPtr(data)) {
				ISO::TypeOpenArray	*array	= (ISO::TypeOpenArray*)type;
				auto				a		= ISO::OpenArrayHead::Get(data)->Data<char>(array->subsize);
#if 1
				for (auto &i : a) {
					const char *id = TypeType(array->subtype) == ISO::REFERENCE ? ((const ISO_ptr<void>*)&i)->ID().get_tag() : 0;
					if (id
					?	!CheckInside(array->subtype, &i, route.add_node(id))
					:	!CheckInside(array->subtype, &i, route.add_node(i))
					)
						return false;
				}
#else
				ISO_openarray<char>	*a		= (ISO_openarray<char>*)data;
				for (int i = 0, c = a->Count(); i < c; i++) {
					const char *id = TypeType(array->subtype) == ISO::REFERENCE ? ((const ISO_ptr<void>*)(*a + array->subsize * i))->ID().get_tag() : 0;
					if (id
					?	!CheckInside(array->subtype, *a + array->subsize * i, route.add_node(id))
					:	!CheckInside(array->subtype, *a + array->subsize * i, route.add_node(i))
					)
						return false;
				}
#endif
			}
			return true;

		case ISO::REFERENCE:
			if (void *pdata = type->ReadPtr(data)) {
				auto	ref = (ISO::TypeReference*)type;
				auto	p 	= ISO::GetPtr<64>(pdata);
				if (!p.IsExternal()) {
					if (mapping.find(p)) {
						ref->set(data, mapping[p]);
					} else if (void *p2 = CheckPtr(p, route)) {
						ref->set(data, ISO::GetPtr<64>(p2));
						p.User() = (void*)ISO::store_string(route);
					}
				}
			}
//			if (*(iso_ptr32<void>*)data)
//				CheckPtr(*(ISO_ptr<void>*)data, route);
			return true;
	};
	return false;
}

void* ISO_dupes::CheckPtr(const ISO_ptr_machine<void> &p, ISO_route &route) {
	if (p.Flags() & ISO::Value::CRCTYPE)
		return nullptr;

	mapping[p] = p;
	const ISO::Type	*type	= p.GetType();
	CheckInside(type, p, route);

	if (TypeType(type) != ISO::VIRTUAL) {
		for (size_t i = 0, n = ptrs.size(); i < n; i++) {
			if (ptrs[i].GetType()->SameAs(type, ISO::MATCH_NOUSERRECURSE) && CompareData(type, p, ptrs[i], 0) && CheckIDs(p, ptrs[i])) {
				mapping[p] = ptrs[i];
				if (p.ID() && (!ptrs[i].ID() || ((ptrs[i].Flags() & ISO::Value::ALWAYSMERGE) && !(p.Flags() & ISO::Value::ALWAYSMERGE))))
					ptrs[i].SetID(p.ID());
				return ptrs[i];
//				p.User() = (void*)ISO::store_string(route);
//				return;
			}
		}
	}
	ptrs.push_back(p);
	return nullptr;
}

void RemoveDupes(ISO_ptr<void> p, int flags) {
	if (!p.IsExternal()) {
		ISO_dupes(flags).CheckPtr(p);
	}
}

//-----------------------------------------------------------------------------
//	Patching
//-----------------------------------------------------------------------------
class Patching {
	ISO_route	route;
	bool	CheckInside(const ISO::Type *type, void *data1, void *data2);
public:
	Patching(tag prefix) : route(prefix)	{}
	bool	CheckPtr(ISO_ptr<void> &p1, ISO_ptr<void> &p2);
};

bool Patching::CheckInside(const ISO::Type *type, void *data1, void *data2) {
	if (!type || !(type = type->SkipUser()))
		return false;

	if (type->IsPlainData())
		return memcmp(data1, data2, type->GetSize()) == 0;

	switch (type->GetType()) {
		case ISO::STRING:
			return *((ISO::TypeString*)type)->get_memory(data1) == *((ISO::TypeString*)type)->get_memory(data2);

		case ISO::COMPOSITE: {
			ISO::TypeComposite	&comp	= *(ISO::TypeComposite*)type;
			bool				same	= true;
			for (auto &e : comp)
				same &= (route.add_node(comp.index_of(e)), CheckInside(e.type, (char*)data1 + e.offset, (char*)data2 + e.offset));
			return same;
		}

		case ISO::ARRAY: {
			ISO::TypeArray		*array	= (ISO::TypeArray*)type;
			bool				same	= true;
			for (int i = 0, c = array->Count(); i < c; i++)
				same &= (route.add_node(i), CheckInside(array->subtype, (char*)data1 + array->subsize * i, (char*)data2 + array->subsize * i));
			return same;
		}

		case ISO::OPENARRAY: {
			ISO_openarray<char>	*a1		= (ISO_openarray<char>*)data1;
			ISO_openarray<char>	*a2		= (ISO_openarray<char>*)data2;
			ISO::TypeOpenArray	*array	= (ISO::TypeOpenArray*)type;
			bool				same	= a1->Count() == a2->Count();
			int					c		= min(a1->Count(), a2->Count());

			if (array->subtype->IsPlainData())
				return same & (memcmp(*a1, *a2, array->subsize * c) == 0);

			for (int i = 0; i < c; i++)
				same &= (route.add_node(i), CheckInside(array->subtype, *a1 + array->subsize * i, *a2 + array->subsize * i));
			return same;
		}

		case ISO::REFERENCE:
			return CheckPtr(*(ISO_ptr<void>*)data1, *(ISO_ptr<void>*)data2);
	};
	return false;
}

const char *SplitSpec(filename &fn, const char *spec) {
	const char	*sc		= strchr(spec, ';');
	if (sc) {
		memcpy(fn, spec, sc - spec);
		fn[sc - spec]	= 0;
		sc++;
	} else {
		fn = spec;
	}
	if (!exists(fn) && fn.ext().blank()) {
		for (directory_iterator dir(filename(fn).set_ext("*")); dir; ++dir) {
			if (!dir.is_dir() && FileHandler::Get(filename(dir).ext())) {
				fn.set_ext(filename(dir).ext());
				break;
			}
		}
	}

	return sc;
}

bool SameFileContents(istream_ref f1, istream_ref f2, uint32 block = 1024 * 1024) {
	malloc_block	buffer1(block);
	malloc_block	buffer2(block);
	for (;;) {
		size_t read1 = f1.readbuff(buffer1, block);
		size_t read2 = f2.readbuff(buffer2, block);
		if (read1 != read2 || memcmp(buffer1, buffer2, block) != 0)
			return false;
		if (read1 == 0)
			return true;
	}
}

bool SameFileContents(const filename &fn1, const filename &fn2) {
	return SameFileContents(FileInput(fn1).me(), FileInput(fn2).me());
}

bool Patching::CheckPtr(ISO_ptr<void> &p1, ISO_ptr<void> &p2) {
	bool	same = p1 == p2;

	if (same) {
		if (!p1)
			return true;
		if (str((const char*)route, string_find((const char*)route, ';')) == p1.External()) {
			p1.Header()->addref();
			return true;
		}
	}

	if (!same && p1.IsExternal()) {
		if (!(same = p2.IsExternal() && istr((const char*)p1) == (const char*)p2)) {
			filename	fn1, fn2;
			if (!(same = str(SplitSpec(fn1, p1)) == SplitSpec(fn2, p2) && SameFileContents(fn1, fn2))) {
				ISO_ptr<void>	d1 = FileHandler::ReadExternal(p1);
				ISO_ptr<void>	d2 = p2.IsExternal() ? (ISO_ptr<void>&)FileHandler::ReadExternal(p2).GetPtr() : p2;
				if (!(same = CheckPtr(d1, d2))) {
					if (!d1.IsType("bitmap") && !d1.IsType("sample"))
						p1 = d1;
					return false;
				}
			}
		}
	}

	if (!same) {
		same = !((p1.Flags() | p2.Flags()) & ISO::Value::CRCTYPE)
			&&	p2.GetType()->SameAs(p1.GetType()) && CheckInside(p1.GetType(), p1, p2);
		if (!same) {
			ISO_TRACE("Different\n");
			return false;
		}
	}

	if (!p1.GetType()->IsPlainData() || p1.GetType()->GetSize() > 16) {
		p1.CreateExternal(route);
		p2 = p1;
	}
	return true;
}

ISO_ptr<void> PatchFile(ISO_ptr<void> p, ISO_ptr<void> target) {
	Patching(target.ID()).CheckPtr(p, target);
	return p;
}

//-----------------------------------------------------------------------------
//	SharedExternals2
//-----------------------------------------------------------------------------

struct SharingCache {
	struct entry : pair<string, ISO_ptr<void> >	{
		entry()	{}
		entry(const filename &fn) { a = fn; }
	};
	order_array<entry>	cache;

	ISO_ptr<void> &operator()(const filename &fn) {
		for (int i = 0; i < cache.size(); i++) {
			if (istr(cache[i].a) == fn)
				return cache[i].b;
		}
		entry	&e = cache.emplace_back(fn);
		return e.b;
	}
	const char*		operator()(ISO_ptr<void> p) {
		return 0;
	}
};

#if 0
ISO_ptr<anything> Gather(dynamic_array<dynamic_array<ISO_ptr<void> > > &buckets, uint32 mask) {
	if (is_pow2(mask))
		return ISO_NULL;

	int	have = 0;
	for (uint32 i = 0, n = buckets.size(); i < n; i++) {
		if ((mask & i) == i && !buckets[i].empty())
			have |= i;
	}
	if (have == 0)
		return ISO_NULL;

//	if (have == mask) {
	ISO_ptr<anything>	p(to_string(bin<uint32>(have)));

	for (uint32 i = 0, n = buckets[have].size(); i < n; i++)
		p->Append(buckets[have][i]);

	buckets[have].clear();

	for (uint32 i = 1; i < have; i <<= 1) {
		if (i & have) {
			if (ISO_ptr<anything> p2 = Gather(buckets, have & ~i))
				p->Append(p2);
		}
	}
	return p;
}

ISO_ptr<anything> GatherFlat(dynamic_array<dynamic_array<ISO_ptr<void> > > &buckets) {
	ISO_ptr<anything>	p1(0);
	for (uint32 i = 1, n = buckets.size(); i < n; i++) {
		if (!buckets[i].empty() && !is_pow2(i)) {
			char	name[64], *pn = name;
			for (uint32 j = i, b = 0; j; j >>= 1, b++) {
				if (j & 1)
					*pn++ = 'A' + b;
			}
			*pn = 0;
			ISO_ptr<anything>	p2(name);
			p1->Append(p2);

			for (uint32 j = 0, m = buckets[i].size(); j < m; j++) {
				ISO_ptr<void>	p = buckets[i][j];
				p2->Append(Duplicate(p));
				p.SetFlags(ISO::Value::REDIRECT);
				if (p.GetType()->GetType() != ISO::REFERENCE)
					p.Header()->type = new CISO_type_reference(p.GetType());
				((ISO_ptr<void>*)p)->CreateExternal("whatever", p.ID());
			}
		}
	}
	return p1;
}

#else

typedef map<uint32, dynamic_array<ISO_ptr<void> > > bucket_map;

ISO_ptr<anything> GatherFlat(bucket_map &buckets) {
	ISO_ptr<anything>	p1(0);
	for (bucket_map::iterator i = buckets.begin(); i != buckets.end(); ++i) {
		uint32	key = i.key();
		if (!is_pow2(key)) {
			 dynamic_array<ISO_ptr<void> >	&bucket	= *i;
			char	name[64], *pn = name;
			for (uint32 j = key, b = 0; j; j >>= 1, b++) {
				if (j & 1)
					*pn++ = 'A' + b;
			}
			*pn = 0;
			ISO_ptr<anything>	p2(name);
			p1->Append(p2);

			for (size_t j = 0, m = bucket.size(); j < m; j++) {
				ISO_ptr<void>	p = bucket[j];
				p2->Append(Duplicate(p));
//				p.SetFlags(ISO::Value::REDIRECT);
//				if (p.GetType()->GetType() != ISO::REFERENCE)
//					p.Header()->type = new CISO_type_reference(p.GetType());
//				((ISO_ptr<void>*)p)->CreateExternal("whatever", p.ID());
//				((ISO_ptr<void>*)p)->CreateExternal((char*)p.User(), p.ID());
			}
		}
	}
	return p1;
}
#endif

ISO_ptr<void> SharedExternals2(const ISO_openarray<string> &filenames) {
	ISO::Browser vars = ISO::root("variables");
	vars.SetMember("exportfor", "none");
//	vars.SetMember("keepexternals", 1);
	vars.SetMember("dontconvert", 1);

	ISO_dupes	dupes;
	int			nf = filenames.Count();
	dynamic_array<dynamic_array<uint16> > counts(nf);
	{
		SharingCache	cache;
		auto			save	= FileHandler::PushCache(&cache);

		for (int f = 0; f < nf; f++) {
			{
				ISO_ptr<void>	p = FileHandler::ReadExternal(filenames[f]);
				ISO_dupes().CheckPtr(p);
				dupes.CheckPtr(p);
				dupes.ClearMap();
			}

			size_t	nd	= dupes.Count();
			counts[f].resize(nd);
			uint16	*c	= counts[f];
			for (int i = 0, n = int(dupes.Count()); i < n; i++)
				c[i] = dupes[i].Header()->refs;
		}
	}

	map<uint32, dynamic_array<ISO_ptr<void> > >	buckets;
	for (int i = 0, n = int(dupes.Count()); i < n; i++) {
		int	bits = 0;
		for (int f = 0, r = 0; f < nf; f++) {
			if (i < counts[f].size() && counts[f][i] > r) {
				bits |= 1 << f;
				r = counts[f][i];
			}
		}
		buckets[bits].push_back(dupes[i]);
	}
	ISO_ptr<void> p = GatherFlat(buckets);
	CheckHasExternals(p, ISO::DUPF_DEEP);

	ISO_route	route("");
	dupes.CheckPtr(p, route);
	for (int i = 0, n = int(dupes.Count()); i < n; i++) {
		ISO_ptr<void>	p = dupes[i];
		if (const char *ext = (const char*)p.User().get()) {
			p.SetFlags(ISO::Value::REDIRECT);
			if (TypeType(p.GetType()) != ISO::REFERENCE)
				p.Header()->type = new ISO::TypeReference(p.GetType());
			*(uint32*)p = 0;
			((ISO_ptr<void>*)p)->CreateExternal(ext, p.ID());
		}
	}
	return p;
}

static initialise init(
	ISO_get_operation(PatchFile),
	ISO_get_operation(SharedExternals2)
);
