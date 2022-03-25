#include "iso/iso_files.h"
#include "extra/date.h"
#include "extra/disk.h"
#include "extra/FAT.h"
#include "base/algorithm.h"
#include "vm.h"
#include "stream.h"

using namespace iso;

//-----------------------------------------------------------------------------
//	FileHandler
//-----------------------------------------------------------------------------

template<> struct ISO::def<DateTime> : ISO::VirtualT2<DateTime> {
	static ISO_ptr<void>	Deref(const DateTime &t) {
		return ISO_ptr<string>(0, to_string(t));
	}
};

struct FATholder {
	FAT::BPB32	*bpb;
	FAT::TYPE	type;
	uint32		total_clusters;

	FATholder(FAT::BPB32 *_bpb) : bpb(_bpb) {
		total_clusters	= bpb->ClusterCount();
		type			= FAT::Type(total_clusters);
	}
	uint32		num_clusters(uint32 s) const {
		return div_round_up(s, bpb->ClusterSize());
	}
	int		get_cluster_entry(int i) const {
		return bpb->ClusterNext(type, i);
	}
	memory_block	get_cluster_data(uint32 n) const { return bpb->Cluster((n & 0xffffff) - 2); }
};

struct FATmeta : FAT::Entry {
	static DateTime	get_date(Date d) {
		return DateTime::Days(JulianDate::Days(d.year + 1980, d.month, d.day));
	}
	static DateTime	get_time(Time t) {
		return DateTime::Hours(t.hours) + DateTime::Mins(t.mins) + DateTime::Secs(t.seconds2 * 2);
	}

	FATmeta(const Entry &e) : Entry(e) {}

	uint32			FirstCluster()	const { return FstClusLO | (FstClusHI << 16); }
	DateTime		Creation()		const { return get_date(CrtDate) + get_time(CrtTime) + DateTime::Secs(CrtTimeTenth / 10.f);	}
	DateTime		LastAccess()	const { return get_date(LstAccDate); }
	DateTime		Write()			const { return get_date(WrtDate) + get_time(WrtTime); }
};

ISO_DEFUSERCOMPV(FATmeta, Attr, Creation, LastAccess, Write);

struct FATdir_entries : ISO::VirtualDefaults {
	const FATholder	&fat;
	int32		first_cluster;

	FATdir_entries(const FATholder &_fat, int32 _first_cluster = 0) : fat(_fat), first_cluster(_first_cluster) {}

	const FAT::Entry *get_entry(int i) const {
		int32				c	= first_cluster;
		memory_block		m	= c ? empty : fat.bpb->Root();
		const FAT::Entry	*p	= m;

		while (i) {
			if (p >= m.end()) {
				m	= fat.get_cluster_data(c);
				c	= fat.get_cluster_entry(c);
				p	= m;
			}
			if (p->used())
				--i;
			p = p->next();
		}
		if (p >= m.end()) {
			m	= fat.get_cluster_data(c);
			p	= m;
		}
		return p;
	}

	ISO::Browser2 Index(int i) const;

	tag2 GetName(int i)	const {
		const FAT::Entry *p = get_entry(i);
		if (p->is_longname()) {
			char16		name[256];
			clear(name);
			for (FAT::LongEntry *s = (FAT::LongEntry*)p; s->is_longname(); ++s)
				s->get_chars(name);
			return str(name);
		}
		return p->name();
	}
	uint32 Count() const {
		uint32			n	= 0;
		int32			c	= first_cluster;
		memory_block	m	= c ? empty : fat.bpb->Root();
		const FAT::Entry	*p	= m;

		for (;;) {
			if (p >= m.end()) {
				if (c == 0 || c == FAT::FINAL)
					break;
				m	= fat.get_cluster_data(c);
				c	= fat.get_cluster_entry(c);
				p	= m;
			}
			if (p->used())
				n++;
			p = p->next();
		}
		return n;
	}
};
ISO_DEFVIRT(FATdir_entries);

struct FATdata {
	const FATholder	&fat;
	uint32		length;
	dynamic_array<uint32>	clusters;

	FATdata(const FATholder &_fat, int32 first, uint32 _length) : fat(_fat), length(_length) {
		if (length == 0 && first) {
			for (int32 c = first; c != FAT::FINAL; c = fat.get_cluster_entry(c))
				clusters.push_back(c);
			length = clusters.size32() * fat.bpb->ClusterSize();
			return;
		}
		int32	c	= first;
		uint32	n	= fat.num_clusters(length);
		clusters.resize(n);
		for (int i = 0; i < n; i++) {
			clusters[i] = c;
			c = fat.get_cluster_entry(c);
		}
	}
	memory_block operator[](int i) const {
		if (i < clusters.size()) {
			memory_block	m = fat.get_cluster_data(clusters[i]);
			if (i == clusters.size() - 1)
				m = m.slice(intptr_t(0), length % fat.bpb->ClusterSize());
			return m;
		}
		return empty;
	}
};

struct FATfile_contents : FATdata, ISO::VirtualDefaults {
	FATfile_contents(const FATholder &fat, int32 first, uint32 length) : FATdata(fat, first, length) {}
	uint32			Count()			const { return uint32(clusters.size());	}
	ISO::Browser2	Index(int i)	const { return ISO::MakePtr(0, (*this)[i]);	}
};
ISO_DEFUSERVIRTX(FATfile_contents, "BigBin");

struct FATfile {
	FATmeta				meta;
	FATfile_contents	contents;
	FATfile(const FATholder &fat, const FAT::Entry &_meta) : meta(_meta), contents(fat, meta.FirstCluster(), meta.FileSize) {}
};
ISO_DEFUSERCOMPV(FATfile, meta, contents);

struct FATdir {
	FATmeta				meta;
	FATdir_entries		entries;
	FATdir(const FATholder &fat, const FAT::Entry &_meta) : meta(_meta), entries(fat, meta.FirstCluster()) {}
};
ISO_DEFUSERCOMPV(FATdir, meta, entries);

ISO::Browser2 FATdir_entries::Index(int i) const {
	const FAT::Entry	*p	= get_entry(i);
	if (p->is_longname())
		p = ((FAT::LongEntry*)p)->get_entry();
	tag		id	= p->name();
	if (p->is_dir())
		return MakePtr(id, FATdir(fat, *p));
	return MakePtr(id, FATfile(fat, *p));
}

struct mapped_FAT : ISO::VirtualDefaults {
	mapped_file		map;
	FATholder		fat;

	mapped_FAT(const char *fn) : map(fn), fat(map.slice(0x2000)) {}
	ISO::Browser2	Index(int i)	const { return FATdir_entries(fat).Index(i); }
	tag2			GetName(int i)	const { return FATdir_entries(fat).GetName(i); }
	uint32			Count()			const { return FATdir_entries(fat).Count(); }
};

ISO_DEFUSERVIRTX(mapped_FAT, "FAT");

class FAT_FileHandler : public FileHandler {
	const char*		GetDescription() override { return "FAT";}
	int				Check(istream_ref file) override {
		FAT::BPB32	bpb;
		file.seek(0x2000);
		if (file.read(bpb)) {
			if (bpb.jmpBoot[0] == 0xe9 || (bpb.jmpBoot[0] == 0xeb && bpb.jmpBoot[2] == 0x90))
				return CHECK_POSSIBLE;
		}
		return CHECK_DEFINITE_NO;
	}

//	ISO_ptr<void>	Read(tag id, istream_ref file) override;
	ISO_ptr<void>	ReadWithFilename(tag id, const filename &fn) override {
		return ISO_ptr<mapped_FAT>(id, fn);
	}
} fat_wad;