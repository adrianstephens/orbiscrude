#include "main.h"
#include "iso/iso_files.h"
#include "iso/iso_script.h"
#include "iso/iso_binary.h"
#include "iso/iso_convert.h"
#include "base/algorithm.h"

#include "maths/geometry.h"
#include "directory.h"
#include "graphics.h"
#include "shader.h"
#include "thread.h"
#include "plugin.h"
#include "vm.h"

#include "filetypes/bitmap/bitmap.h"
#include "filetypes/container/archive_help.h"
#include "systems/conversion/platformdata.h"
#include "systems/communication/connection.h"
#include "devices/device.h"
#include "extra/date.h"
#include "bin.h"

#ifdef PLAT_PC
#include <ShellScalingApi.h>
#include <shellapi.h>
#include <sapi.h>
#include <ole2.h>
#endif

using namespace iso;
using namespace win;

extern ISO_ptr<void>	GetRemote(tag2 id, const char *target, const char *spec);
extern ISO_ptr<void>	GetRemote(tag2 id, ISO_ptr<void> p, const char *spec);

size_t get_crc_string(crc32 crc, char *dest, size_t maxlen) {
	if (ISO::Browser2 b = ISO::root("variables")["dictionary"]) {
		struct DictionaryEntry {
			crc32 crc;
			const char *symbol;
			bool operator<(crc32 _crc) const { return crc < _crc; }
		};
		typedef ISO_openarray<DictionaryEntry> Dictionary;
		ISO_ptr<Dictionary> dictionary = b;
		Dictionary::iterator i = lower_bound(dictionary->begin(), dictionary->end(), crc);
		if (i != dictionary->end() && i->crc == crc) {
			size_t	len = min(strlen(i->symbol), maxlen - 1);
			memcpy(dest, i->symbol, len);
			dest[len] = 0;
			return len;
		}
	}
	return _format(dest, maxlen, "crc_%08x", crc);
}

namespace app {

//-----------------------------------------------------------------------------
//	Misc
//-----------------------------------------------------------------------------

fixed_string<256> get_id(tag2 id) {
	fixed_string<256>	t;
	if (id) {
		if (const char *p = id.get_tag())
			t = p;
		else
			get_crc_string(id, t, 256);
	}
	return t;
}

fixed_string<256> get_id(ISO_ptr<void> p) {
	return get_id(p.ID());
}

ISO::Browser2 VirtualDeref(ISO::Browser2 b) {
	ISO::Browser2 b2;
	while (b.SkipUser().IsVirtPtr() && (b2 = *b))
		b = b2;
	return b;
}

ISO::Browser2 SkipPointer(ISO::Browser2 b) {
	ISO::Browser2 b2;
	while ((b.SkipUser().GetType() == ISO::REFERENCE || b.SkipUser().IsVirtPtr()) && (b2 = *b))
		b = b2;
	return b;
}

ISO_ptr<anything> UnVirtualise(const ISO::Browser &b0) {
	ISO_ptr<anything>	a(0);
	for (ISO::Browser::iterator i = b0.begin(), e = b0.end(); i != e; ++i) {
		if (i->GetType() == ISO::REFERENCE)
			a->Append(**i);
		else
			a->Append(i->Duplicate(i.GetName()));
	}
	return a;
}

ISO_ptr<void>	RemoteFix(const tag2 &id, const ISO::Type *type, ISO::Browser &v, const char *spec) {
#ifdef PLAT_WIN32
	static map<crc32, ISO::TypeUser*>	m;
	ISO_ptr<void>	p = GetRemote(id, (ISO_ptr<void>)v, spec);
	if (type && type->GetType() == ISO::USER) {
		tag2	id = ((ISO::TypeUser*)type)->ID();
		ISO::TypeUser *&user = m[id];
		if (!user)
			user = new ISO::TypeUser(id, p.GetType());
		p.Header()->type = user;
	}
	return  p;
#else
	return v;
#endif
}

//-----------------------------------------------------------------------------
//	File
//-----------------------------------------------------------------------------

const char *WriteFileExt(const ISO::Browser2 &b, tag id) {
	if (const char *ext = id.rfind('.'))
		return ext;

	if (IsRawData2(b))
		return "bin";

	if (b.SkipUser().GetType() == ISO::STRING)
		return "txt";

	if (b.Is<sample>())
		return "wav";

	if (b.Is<bitmap>())
		return "png";

//	if (FileHandler *fh = FileHandler::Get(filename(tag(p.ID())).ext()))
//		return tag(p.ID()).rfind('.');
	return "ix";
}

const char *WriteFilesExt(const ISO::Browser2 &b, tag id) {
	if (b.GetTypeDef()->SameAs<anything>())
		return "";

	if (IsRawData2(b))
		return "bin";

	if (b.SkipUser().GetType() == ISO::VIRTUAL) {
		switch (b.Count()) {
			case 0:		return WriteFileExt(SkipPointer(*b), id);
			case 1:		return WriteFileExt(SkipPointer(b[0]), id);
			default:	return "";
		}
	}

	return WriteFileExt(b, id);
}

void WriteFile(const filename &fn, const ISO::Browser2 &b) {
#ifdef HAS_FILE_WRITER
	create_dir(fn.dir());
	FileOutput		file(fn);
	if (file.exists()) {
		if (FileHandler *fh = FileHandler::Get(fn.ext())) {
			if (fh->Write(b, file))
				return;
		}

		if (IsRawData2(b)) {
			FileHandler::Get("bin")->Write(b, file);

		} else if (b.SkipUser().GetType() == ISO::STRING) {
			file.writebuff(b[0], b.Count() * b[0].GetSize());

		} else {
			if (FileHandler *fh = FileHandler::Get(filename(b.GetName().get_tag()).ext())) {
				if (fh->Write(b, file))
					return;
			}

			if (b.SkipUser().GetType() == ISO::VIRTUAL) {
				auto	b2 = SkipPointer(*b);
				if (IsRawData2(b2)) {
					FileHandler::Get("bin")->Write(b2, file);
					return;
				}
			}

			FileOutput			file(fn);
			ISO::ScriptWriter	writer(file);
			writer.SetFlags(ISO::SCRIPT_IGNORE_DEFER);
			writer.DumpDefs(b);
			writer.DumpData(b);
			//ISO::ScriptWriter(FileOutput(fn).me()).SetFlags(ISO::SCRIPT_VIRTUALS).DumpData(b);
		}
	}
#endif
}

void WriteFile(const filename &fn, const ISO::Browser2 &b, HierarchyProgress &progress) {
	WriteFile(fn, b);
	progress.Next();
}

void WriteDirectory(const filename &dir, const ISO::Browser2 &b, HierarchyProgress &progress, int depth) {
	ISO_ASSERT(depth < 100);
	create_dir(dir);
	uint32	n = b.Count();
	
	auto	saver	= save(progress.prog, n);
//	progress.StartChildren(n);
	for (int i = 0; i < n; i++) {
		tag				id	= b.GetName(i);
//		filename		fn	= filename(dir).add_dir(filename(id).name()).set_ext(WriteFilesExt(b2, id));
		filename		fn	= filename(dir).add_dir(filename(id).name_ext());
		WriteFiles(fn, SkipPointer(*b[i]).SkipUser(), progress, depth + 1);
	}
//	progress.EndChildren(n);
}

void WriteFiles(const filename &fn, const ISO::Browser2 &b, HierarchyProgress &progress, int depth) {
	if (b.GetTypeDef()->SameAs<anything>()) {
		WriteDirectory(fn, b, progress, depth);

	} else if (IsRawData2(b)) {
		WriteFile(fn, b);

	} else if (b.GetType() == ISO::VIRTUAL) {
		auto	b2	= *b;
		if (IsRawData2(b2)) {
			WriteFile(fn, b2);
		} else {
			auto	v = (ISO::Virtual*)b.GetTypeDef();
			switch (v->Count(v, b)) {
				case 0:		WriteFile(fn, SkipPointer(*b)); break;
				case 1:		WriteFile(fn, SkipPointer(b[0])); break;
				default:	WriteDirectory(fn, b, progress, depth); break;
			}
		}
	} else {
		WriteFile(fn, b);
	}
	progress.Next();
}

string GetFileFilter() {
	string_builder	p;
//	p << "Isopod Files (*.ix,*.ib,*.ibz)\0*.ix;*.ib;*.ibz\0All Files (*.*)\0*.*\0";
	p << "All Files (*.*)\0*.*\0Isopod Files (*.ix,*.ib,*.ibz)\0*.ix;*.ib;*.ibz\0";
	for (FileHandler::iterator i = FileHandler::begin(); i != FileHandler::end(); ++i) {
		if (const char *ext = i->GetExt()) {
			if (const char *des = i->GetDescription()) {
				p.format("%s (*.%s)", des, ext) << "\0";
				p.format("*.%s", ext) << "\0";
			}
		}
	}
	p << "\0";
	return p;
}

//-----------------------------------------------------------------------------
//	TreeControl operations
//-----------------------------------------------------------------------------
extern ImageListBitmap GetTreeIcon(const ISO::Browser2 &b);

bool IsString(const ISO::Type *type) {
	if (type && (type = type->SkipUser())) {
		switch (type->GetType()) {
			case ISO::STRING:
				return true;

			case ISO::ARRAY:
			case ISO::OPENARRAY:
				return (type = type->SubType()) && type->GetType() == ISO::INT && (type->flags & ISO::TypeInt::CHR);

			default:
				return false;
		}
	}
	return false;
}

ISOTree::ADD_FLAGS ISOTree::GetFlags(const ISO::Browser2 &b) {
	if (const ISO::Type *type = b.SkipUser().GetTypeDef()) {
		switch (b.SkipUser().GetType()) {
			case ISO::COMPOSITE:
			case ISO::ARRAY:
			case ISO::OPENARRAY:
				return HAS_CHILDREN;
			case ISO::REFERENCE:
				return !b.ISO::Browser::External() && !b.HasCRCType() ? GetFlags(*b) : NONE;
			case ISO::VIRTUAL:
				if (type->flags & ISO::Virtual::DEFER)
					return HAS_CHILDREN;
				if (b.Count() == 0 || IsString((*b).GetTypeDef()))
					return PRE_EXPAND;
				return HAS_CHILDREN;
			default:
				break;
		}
	}
	return NONE;
}

fixed_string<256> ISOTree::ItemName(const ISO::Browser2 &b, int i) {
	if (tag2 id = b.GetName())
		return get_id(id);
	return format_string("[%i]", i);
};

fixed_string<256> ISOTree::ItemNameI(const ISO::Browser &b, int i) {
	if (tag2 id = b.GetName(i))
		return get_id(id);
	return format_string("[%i]", i);
};

int ISOTree::GetIndex(HTREEITEM hItem) {
	return GetItemParam(hItem);
}

void ISOTree::SetItem(HTREEITEM hItem, const ISO::Browser2 &b, const char *name) {
	TreeControl::Item	i(hItem);
	if (name)
		i.Text(name);
	ADD_FLAGS	flags = GetFlags(b);
#ifdef PLAT_WIN32
	i.ClearState(TVIS_EXPANDED).SetState(TVIS_EXPANDEDONCE, !!(flags & PRE_EXPAND)).Children(flags & HAS_CHILDREN).Set(*this);
#endif
}

HTREEITEM ISOTree::AddItem(HTREEITEM hParent, HTREEITEM hAfter, const char *name, int i, ADD_FLAGS flags) {
	int	state = flags & PRE_EXPAND ? Item::EXPANDEDONCE : 0;
	return Item(name).Param(i).SetState(state).Children(flags & HAS_CHILDREN).Insert(*this, hParent, hAfter);
}

HTREEITEM ISOTree::AddItem(HTREEITEM hParent, HTREEITEM hAfter, const char *name, int i, ADD_FLAGS flags, ImageListBitmap image) {
	int	state = flags & PRE_EXPAND ? Item::EXPANDEDONCE : 0;
	return Item(name).Image(image).Image2(image).Param(i).SetState(state).Children(flags & HAS_CHILDREN).Insert(*this, hParent, hAfter);
}

HTREEITEM ISOTree::InsertItem(HTREEITEM hParent, HTREEITEM hAfter, int i, const ISO::Browser2 &b) {
	HTREEITEM	h	= AddItem(hParent, hAfter, ItemName(b, i), i, GetFlags(b), GetTreeIcon(b));
	HTREEITEM	hi	= h;
	while (hi = GetNextItem(hi)) {
		Item(hi).Param(++i).Set(*this);
	}
	return h;
}

void ISOTree::RemoveItem(const ISO::Browser2 &b, HTREEITEM hParent, int index) {
	HTREEITEM hChild = GetChildItem(hParent);
	for (int i = 0; i < index; i++)
		hChild = GetNextItem(hChild);

	HTREEITEM hSel = GetNextItem(hChild);
	if (hSel) {
		HTREEITEM h = hSel;
		for (int i = index; h; i++, h = GetNextItem(h))
			TreeControl::Item(h).Get(*this).Text(ItemNameI(b, i)).Param(i).Set(*this);
	} else if (index == 0) {
		hSel = hParent;
	} else {
		hSel = GetChildItem(hParent);
		for (int i = 0; i < index - 1; i++)
			hSel = GetNextItem(hSel);
	}
	DeleteItem(hChild);
	if (hSel != TVI_ROOT)
		SetSelectedItem(hSel);
}

int ISOTree::Setup(const ISO::Browser2 &b, HTREEITEM hParent, int maximum) {
	switch (b.GetType()) {
		case ISO::COMPOSITE:
		case ISO::ARRAY:
		case ISO::OPENARRAY: {
			int	c = b.Count();
			if (maximum && c > maximum)
				return c;
			EnableRedraws(false);
			for (int i = 0; i < c; i++)
				AddItem(hParent, TVI_LAST, ItemNameI(b, i), i, GetFlags(b[i]));
			EnableRedraws(true);
			break;
		}

		case ISO::REFERENCE:
			if (!b.ISO::Browser::External() && !b.HasCRCType())
				return Setup(*b, hParent, maximum);
			break;

		case ISO::VIRTUAL: {
			if (b.IsVirtPtr())
				return Setup(*b, hParent, maximum);

			int	c = b.Count();
#if 0//def _DEBUG
			c = maximum ? min(c, maximum) : c;	//TEMP!
#endif
			if (maximum && c > maximum)
				return c;

			EnableRedraws(false);
			for (int i = 0; i < c; i++)
				AddItem(hParent, TVI_LAST, ItemNameI(b, i), i, GetFlags(b[i]));
			EnableRedraws(true);
			break;
		}

		case ISO::USER:
			return Setup(b.SkipUser(), hParent, maximum);
	}
	return 0;
}

ISO::Browser2 ISOTree::GetChild(ISO::Browser2 &b, HTREEITEM hItem) {
//	TreeControl::Item i = GetItem(hItem, TVIF_PARAM | TVIF_STATE);
//	if (i.state & TVIS_USERMASK)
//		return (ISO_ptr<void>&)i.Param();
	return b[(int)GetItem(hItem).Param()];
}

HTREEITEM ISOTree::Locate(HTREEITEM hItem, ISO::Browser2 b, const ISO_ptr<void> &p) {
	if (b) {
		if (b.SkipUser().GetType() == ISO::REFERENCE && p == *(ISO_ptr<void>*)b)
			return hItem;
		for (HTREEITEM h = GetChildItem(hItem); h; h = GetNextItem(h)) {
			if (HTREEITEM h2 = Locate(h, GetChild(b, h), p))
				return h2;
		}
	}
	return 0;
}

void ISOTree::GetRoute(string_accum &spec, HTREEITEM hItem) {
	if (hItem) {
		GetRoute(spec, GetParentItem(hItem));
		spec << '[' << GetIndex(hItem) << ']';
	}
}

struct TreeIterator {
	callback<void(ISO::Browser2, int i)> cb;
	ISO::Browser2	b;

	void	Up(TreeControl tree, HTREEITEM hItem) {
		if (hItem) {
			Up(tree, tree.GetParentItem(hItem));
			int	i = tree.GetItemParam(hItem);
			cb(b, i);
			b = b[i];
		}
	}

	template<typename T> TreeIterator(T &&t, const ISO::Browser2 &_b, TreeControl tree, HTREEITEM hItem) : cb(&t), b(_b) {
		if (hItem != TVI_ROOT)
			Up(tree, hItem);
	}
	operator ISO::Browser2()	const { return b; }
};

ISO::Browser2 ISOTree::GetBrowser(const ISO::Browser2 &b, HTREEITEM hItem) {
	struct handler {
		void	operator()(ISO::Browser2 b, int i) {}
		handler() {}
	};
	return TreeIterator(handler(), b, *this, hItem);
}

#ifdef PLAT_WIN32
ISO::Browser2 ISOTree::GetBrowser(const ISO::Browser2 &b, TVITEMA &item) {
	return GetBrowser(b, GetParentItem(item.hItem))[int(item.lParam)];
}
#endif

ISO_VirtualTarget ISOTree::GetVirtualTarget(const ISO::Browser2 &b, HTREEITEM hItem) {
	struct handler {
		ISO::Browser		v;
		buffer_accum<256>	spec;
		uint32				bin;

		void	operator()(ISO::Browser2 b, int i) {
			while (b.SkipUser().GetType() == ISO::REFERENCE) {
				b = *b;
				auto	p = b.GetPtr();
				if (p.Header()->flags & ISO::Value::ROOT)
					bin = p.UserInt();
			}
			if (b.SkipUser().GetType() == ISO::VIRTUAL) {
				v		= b;
				spec.reset();
			}
			spec << '[' << i << ']';
		}
		handler() : bin(0) {}
	};

	handler			h;
	ISO::Browser2	b1 = TreeIterator(h, b, *this, hItem);
	ISO::Browser2	b2 = b1;

	while (b2.SkipUser().GetType() == ISO::REFERENCE)
		b2 = *b2;
	if (b2.SkipUser().GetType() == ISO::VIRTUAL) {
		h.v		= b1 = b2;
		h.spec.reset();
	}
	return ISO_VirtualTarget(b1, h.v, h.spec.term(), h.bin);
}

//-----------------------------------------------------------------------------
//	Differences
//-----------------------------------------------------------------------------
#if 0
ISO::Browser2 Differences(const ISO::Browser2 &b1, const ISO::Browser2 &b2) {

	const ISO::Type *type	= b1.GetTypeDef();
	if (!type->SameAs(b2.GetTypeDef()))
		return b2;

#if 0
	if (b1.IsPtr()) {
		if (b1.GetName() != b2.GetName())
			return b1;
	}
#endif
	if (type && type->GetType() == ISO::REFERENCE || b1.IsVirtPtr())
		return Differences(*b1, *b2);

	if (!type || (type->GetType() != ISO::VIRTUAL && CompareData(type, b1, b2)))
		return ISO::Browser2();

	int	n1	= b1.Count();
	int	n2	= b2.Count();
	int	n	= max(n1, n2);

	ISO_ptr<anything>	p(0);
	for (int i = 0; i < n; i++) {
		ISO::Browser2 d;
		if (i < n1 && i < n2) {
			d = Differences(b1[i], b2[i]);
			if (!d)
				continue;
		} else if (i < n1) {
			d = b1[i];
		} else {
			d = b2[i];
		}
		p->Append(d);
	}
	if (p->Count())
		return p;

	return ISO::Browser2();
}
#endif
//-----------------------------------------------------------------------------
//	Helpers
//-----------------------------------------------------------------------------

ISO_ptr<void> ExpandChild(tag2 name, istream_ref file, int mode) {
	ISO_ptr<void>	p;

	if (FileHandler *fh = FileHandler::Get(filename(name.get_tag()).ext()))
		p = fh->Read(name, file);

	if (!p && (mode & 1)) {
		if (FileHandler *fh = FileHandler::Identify(file))
			p = fh->Read(name, file);
	}
	return p;
}

void ExpandChildren(const ISO::Browser &b0, string &errors, int mode) {
//	for (auto &i : b0) {
	for (auto i = b0.begin(), e = b0.end(); i != e; ++i) {
		if (!i->IsPtr())
			continue;

		ISO::Browser2	b2 = *i;
		if (int bin = FindRawData(b2)) {
			tag	name = b2.GetName();
			try {
				if (ISO_ptr<void> p = ExpandChild(name,  bin == 3 ? istream_ref(BigBinStream(b2)) : istream_ref(memory_reader(GetRawData(b2))), mode))
					//b2.Set(p);
					(*i).Set(p);

			} catch (const char *s) {
				errors << s << " in " << name << '\n';
			}

		} else {
			ISO::Browser2	b3	= b2[0].SkipUser();
			if (b3.GetType() == ISO::REFERENCE) {
				if (mode & 2)
					ExpandChildren(b2, errors, mode);
				continue;
			}
		}
	}
}

//-----------------------------------------------------------------------------
//	Editor
//-----------------------------------------------------------------------------

Editor* Editor::Find(ISO::Browser2 &b) {
	for (;;) {
		if (!b || b.HasCRCType())
			break;

		for (auto &i : all()) {
			if (i.Matches(b))
				return &i;
		}

		if (b.GetType() == ISO::USER) {
			b = ISO::Browser2(b.GetTypeDef()->SubType(), b, b.GetPtr());
			//b = b.SkipUser();

		} else if (b.GetType() == ISO::OPENARRAY) {
			break;

		} else {
			b = *b;
/*
		} else if (b.IsVirtPtr()) {
			b = *b;

		} else if (b.GetType() == ISO::VIRTUAL && b.Count() == 1) {
			b = b[0];

		} else if (b.GetType() == ISO::REFERENCE) {
			b = *b;

		} else {
			break;
		*/
		}
	}
	return NULL;
}

#ifdef PLAT_WIN32
Control Editor::FindOpen(const char *name) {
	class Finder : public win::WindowEnumerator<Finder> {
		const char *name;
		Control	control;
	public:
		bool operator()(Control c) {
			if (c.GetText() == str(name)) {
				control = (HWND)c;
				return false;
			} else if (!c.Parent()) {
				enum_children(c);
			}
			return true;
		};
		Finder(const char *_name) : name(_name)	{ enum_this_thread();	}
		operator Control()	const				{ return control;	}
	};
	return Finder(name);
}
#endif

//-----------------------------------------------------------------------------
//	Ops
//-----------------------------------------------------------------------------

void BrowserAssign(const ISO::Browser &dest, const ISO::Browser &srce) {
	ISO_ASSERT(dest && srce && srce.GetTypeDef()->SameAs(dest.GetTypeDef(), ISO::MATCH_MATCHNULLS));
	CopyData(srce.GetTypeDef(), srce, dest);
}

struct ModifyDataOp : Operation {
	ISO::Browser	b;
	ISO_ptr<void>	p;
	bool			undone;
	ModifyDataOp(const ISO::Browser &_b) : b(_b), p(b.Duplicate()), undone(false) {}
	bool			Do()	{
		return true;
	}
	bool			Undo()	{
		if (!undone) {
			undone = true;
			CopyData(b.GetTypeDef(), p, b);
		}
		return true;
	}
};

Operation* MakeModifyDataOp(const ISO::Browser &_b) {
	return new ModifyDataOp(_b);
}

struct DeleteOp : Operation {
	ISOTree				tree;
	HTREEITEM			hp;
	ISO_VirtualTarget	v;
	int					index;
	ISO_ptr<void>		p;

	DeleteOp(TreeControl _tree, HTREEITEM _hp, int _index, ISO_VirtualTarget &_v)
		: tree(_tree), hp(_hp), v(_v), index(_index) {}

	bool			Do() {
		Busy		bee;
		int			n	= v.Count();
		p = v[index].Duplicate();
		v.Remove(index);

		if (!v.Update())
			return false;
		tree.RemoveItem(v, hp, index);
		return true;
	}

	bool			Undo() {
		Busy		bee;
		v.Insert(index).UnsafeSet(p);
		v.Update();

		tree.DeleteChildren(hp);
		tree.Setup(v, hp, 0);
		return true;
	}
};

Operation* MakeDeleteOp(TreeControl _tree, HTREEITEM _hp, int _index, ISO_VirtualTarget &_v) {
	return new DeleteOp(_tree, _hp, _index, _v);
}

template<int B> struct ModifyPtrOp : Operation {
	ISOTree				tree;
	HTREEITEM			h;
	ISO_ptr<void, B>	&p0, p1;

	ModifyPtrOp(TreeControl tree, HTREEITEM h, ISO_ptr<void, B> &p0, ISO_ptr<void, B> &p1)
		: tree(tree), h(h), p0(p0), p1(p1)
	{}
	void			Swap() {
		swap(p0, p1);
		tree.DeleteChildren(h);
		tree.SetItem(h, p0, p0.ID() ? (const char*)get_id(p0.ID()) : 0);
		tree.Invalidate();
	}
	bool			Do()	{ Swap(); return true; }
	bool			Undo()	{ Swap(); return true; }
};

template<int B> Operation* MakeModifyPtrOp(TreeControl tree, HTREEITEM h, ISO_ptr<void, B> &p0, ISO_ptr<void, B> &p1) {
	return new ModifyPtrOp<B>(tree, h, p0, p1);
}
template Operation* MakeModifyPtrOp<64>(TreeControl tree, HTREEITEM h, ISO_ptr<void, 64> &p0, ISO_ptr<void, 64> &p1);
template Operation* MakeModifyPtrOp<32>(TreeControl tree, HTREEITEM h, ISO_ptr<void, 32> &p0, ISO_ptr<void, 32> &p1);


struct ModifyValueOp : Operation {
	ISOTree			tree;
	HTREEITEM		h;
	ISO::Browser	b;
	ISO_ptr<void>	p;
	bool			undone;

	ModifyValueOp(TreeControl tree, HTREEITEM h, ISO::Browser &b)
		: tree(tree), h(h), b(b), p(b.Duplicate()), undone(false)
	{}
	bool			Do()	{ return true; }
	bool			Undo()	{
		if (!undone) {
			undone = true;
			CopyData(b.GetTypeDef(), p, b);
			tree.DeleteChildren(h);
			tree.SetItem(h, b, 0);
			tree.Invalidate();
		}
		return true;
	}
};

Operation* MakeModifyValueOp(TreeControl tree, HTREEITEM h, ISO::Browser &b) {
	return new ModifyValueOp(tree, h, b);
}

struct AddEntryOp : Operation {
	ISOTree			tree;
	HTREEITEM		hParent;
	ISO::Browser2	b;

	AddEntryOp(TreeControl _tree, HTREEITEM _hParent, const ISO::Browser2 &_b)
		: tree(_tree), hParent(_hParent), b(_b)
	{}
	bool			Do() {
		int		i = b.Count();
		if (!b.Resize(i + 1))
			return false;
		if (!hParent || tree.ExpandedOnce(hParent))
			tree.AddItem(hParent, TVI_LAST, tree.ItemNameI(b, i), i, tree.GetFlags(b[i]));
		return true;
	}
	bool			Undo() {
		int		i = b.Count();
		b.Resize(i - 1);
		tree.RemoveItem(b, hParent, i - 1);
		return true;
	}
};

Operation* MakeAddEntryOp(TreeControl _tree, HTREEITEM _hParent, const ISO::Browser2 &_b) {
	return new AddEntryOp(_tree, _hParent, _b);
}

struct InsertEntryOp : Operation {
	ISOTree			tree;
	HTREEITEM		hParent, hAfter;
	ISO::Browser	a;
	ISO_ptr<void>	b;

	InsertEntryOp(TreeControl _tree, HTREEITEM _hParent, HTREEITEM _hAfter, const ISO::Browser &_a, const ISO_ptr<void> &_b)
		: tree(_tree), hParent(_hParent), hAfter(_hAfter), a(_a), b(_b)
	{}
	bool			Do() {
		int		i = hAfter == TVI_LAST ? a.Count() : hAfter == TVI_FIRST ? 0 : tree.FindChildIndex(hParent, hAfter) + 1;
		if (a.Insert(i).Set((ISO_ptr<void>)b)) {
			tree.InsertItem(hParent, hAfter, i, b);
			return true;
		}
		return false;
	}
	bool			Undo() {
		int		i = hAfter == TVI_LAST ? a.Count() - 1 : hAfter == TVI_FIRST ? 0 : tree.FindChildIndex(hParent, hAfter) + 1;
		a.Remove(i);
		tree.RemoveItem(a, hParent, i);
		return true;
	}
};

Operation *MakeInsertEntryOp(TreeControl _tree, HTREEITEM _hParent, HTREEITEM _hAfter, const ISO::Browser &_a, const ISO_ptr<void> &_b) {
	return new InsertEntryOp(_tree, _hParent, _hAfter, _a, _b);
}

//-----------------------------------------------------------------------------
//	IsoEditor
//-----------------------------------------------------------------------------

ISO::Browser2 IsoEditor::GetBrowser(HTREEITEM hItem) {
	ISO::binary_data.SetRemoteTarget(0);
	return Tree().GetBrowser(root_ptr, hItem);
}

#ifdef PLAT_WIN32
ISO::Browser2 IsoEditor::GetBrowser(TVITEMA &item) {
	ISO::binary_data.SetRemoteTarget(0);
	ISOTree tree	= treecolumn.GetTreeControl();
	return tree.GetBrowser(root_ptr, tree.GetParentItem(item.hItem))[int(item.lParam)];
}
#endif

ISO_VirtualTarget IsoEditor::GetVirtualTarget(HTREEITEM hItem) {
	ISO::binary_data.SetRemoteTarget(0);
	return Tree().GetVirtualTarget(root_ptr, hItem);
}

HTREEITEM IsoEditor::Locate(const ISO_ptr<void> &p) {
	return Tree().Locate(TVI_ROOT, ISO::Browser(root_ptr), p);
}

bool IsoEditor::LocateRoute(string_accum &spec, const ISO_ptr<void> &p) {
	if (HTREEITEM h = Locate(p)) {
		Tree().GetRoute(spec, h);
		return true;
	}
	return false;
}


void IsoEditor::AddEntry(const ISO_ptr<void> &p, bool test) {
	if (!p)
		return;

	ISOTree	tree	= Tree();
	if (test) {
		if (HTREEITEM h = Locate(p)) {
			tree.SetSelectedItem(h);
			return;
		}
	}

#ifdef PLAT_PC
	int		n		= root_ptr->Count();
	if (Do(new InsertEntryOp(tree, TVI_ROOT, TVI_LAST, ISO::Browser(root_ptr), p)))
		tree.SetSelectedItem(tree.GetChildItem(TVI_ROOT, n));
#else
	root_ptr->Append(p);
	int	i = root_ptr->Count() - 1;
	tree.AddItem(TVI_ROOT, TVI_LAST, p.ID() ? get_id(p) : format_string("[%i]", i), i, tree.GetFlags(p));
#endif
}

bool IsoEditor::Update(const ISO_ptr<void> &p) {
#ifdef PLAT_WIN32
	if (HTREEITEM h = Locate(p)) {
		ISOTree	tree = treecolumn.GetTreeControl();
		Rect	rect = treecolumn.GetItemRect(h, 2);
		if (TreeControl::Item(h, TVIF_STATE).Get(tree).state & TVIS_EXPANDED) {
			ISO::Browser2	b(p);
			if (b.GetType() == ISO::OPENARRAY) {
				int	n0 = 0;
				for (HTREEITEM hc = tree.GetChildItem(h); hc; hc = tree.GetNextItem(hc))
					n0++;
				int	n1 = b.Count();
				if (n1 < n0) {
					tree.DeleteChildren(h);
					tree.Setup(b, h, max_expand);
				}
				while (n0 < n1) {
					tree.AddItem(h, TVI_LAST, tree.ItemNameI(b, n0), n0, tree.GetFlags(b[n0]));
					n0++;
				}
			}
			if (HTREEITEM h2 = tree.GetNextItem(h))
				rect |= treecolumn.GetItemRect(h2, 2).TopLeft();
			else
				rect |= treecolumn.GetClientRect().BottomRight();
		}
		treecolumn.Invalidate(rect);
		GetVirtualTarget(h).Update();
		return true;
	}
#endif
	return false;
}

bool IsoEditor::Select(const ISO_ptr<void> &p) {
	if (HTREEITEM h = Locate(p)) {
		Tree().SetSelectedItem(h);
		return true;
	}
	return false;
}

bool IsoEditor::SelectRoute(const char *route) {
	uint32	i;
	size_t	n;
	ISOTree		tree	= Tree();
	HTREEITEM	h		= TVI_ROOT;
	while (*route == '[' && (n = from_string(route + 1, i))) {
		route = route + n + 2;

		save(max_expand, 0), tree.ExpandItem(h);

		h = tree.GetChildItem(h);
		while (i--)
			h = tree.GetNextItem(h);
	}
	tree.EnsureVisible(h);
	tree.SetSelectedItem(h);
	return true;
}

bool IsoEditor::Do(Operation *op) {
	if (!op->Do()) {
		delete op;
		return false;
	}

	while (!redo.empty())
		delete redo.pop_front();
	undo.push_front(op);
	return true;
}

ISO::Browser IsoEditor::GetSelection() {
	return GetBrowser(Tree().GetSelectedItem());
}

#ifdef PLAT_WIN32
void IsoEditor::SetEditWindow(Control c) {
	editwindow = c;
	c.Show();
	c.SetFocus();
}
#endif

//-----------------------------------------------------------------------------
//	Refresh
//-----------------------------------------------------------------------------

void Refresh(TreeControl &tree, ISO::Browser2 b, HTREEITEM h) {
	struct refresher {
		ISOTree				tree;
		ISO::Browser2		virt;
		fixed_string<512>	buffer;
		char				*start;

		void children(ISO::Browser2 b, HTREEITEM h) {
			char	*end = start + strlen(start);
			for (HTREEITEM hc = tree.GetChildItem(h); hc; hc = tree.GetNextItem(hc)) {
				int	i = tree.GetIndex(hc);
				sprintf(end, "[%i]", i);
				recurse(b[i], hc);
			}
			*end = 0;
		}

		void recurse(ISO::Browser2 b, HTREEITEM h) {
			if (b.GetType() == ISO::REFERENCE)
				b = *b;

			if (virt && b.SkipUser().GetType() != ISO::VIRTUAL)
				virt.Update(start, true);

			if (tree.ExpandedOnce(h)) {
				if (b.SkipUser().GetType() == ISO::VIRTUAL)
					save(virt, b), save(start, start + strlen(start)), children(b, h);
				else
					children(b, h);
			}
		}

		refresher(TreeControl &_tree) : tree(_tree) { start = buffer; }
	};
	refresher(tree).children(b, h);
}

}//namespace app
