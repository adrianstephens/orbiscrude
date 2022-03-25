#include "iso/iso_files.h"
#include "extra/xml.h"
#include "container/compound_doc.h"

using namespace iso;

//-----------------------------------------------------------------------------
//	Excel
//-----------------------------------------------------------------------------

class XLSXFileHandler : public FileHandler {

	struct SharedStrings : dynamic_array<string> {
		int			count, unique;

		SharedStrings(istream_ref in) {
			XMLreader::Data	data;
			XMLreader				xml(in);

			while (xml.ReadNext(data, XMLreader::TAG_BEGIN) == XMLreader::TAG_BEGIN) {
				if (data.Is("sst")) {
					count	= string_scan(data.Find("count")).get<int>();
					unique	= string_scan(data.Find("uniqueCount")).get<int>();

				} else if (data.Is("si")) {
					string	*current = new(expand()) string;
					while (xml.ReadNext(data, XMLreader::TAG_CONTENT) == XMLreader::TAG_CONTENT)
						(*current) += data.Content();
				}
			}
		}
	};

	ISO_ptr<ISO_openarray<anything> > ReadWorkSheet(tag id, istream_ref in, SharedStrings &ss) {
		XMLreader::Data	data;
		XMLreader				xml(in);
		anything				*row;
		int						state = 0;

		ISO_ptr<ISO_openarray<anything> >	p(id);

		while (xml.ReadNext(data, XMLreader::TAG_BEGIN) == XMLreader::TAG_BEGIN) {
			switch (state) {
				case 0:
					if (data.Is("sheetData"))
						state = 1;
					break;
				case 1:
					if (data.Is("row")) {
						row = &p->Append();
					} else if (data.Is("c")) {
						while (xml.ReadNext(data, XMLreader::TAG_CONTENT) == XMLreader::TAG_CONTENT) {
							int	i = string_scan(data.Content()).get<int>();
							row->Append(MakePtr(tag(), (const char*)ss[i]));
						}
					}
					break;
			}
		}
		return p;
	}

	const char*		GetExt() override { return "xlsx"; }
	const char*		GetDescription() override { return "Excel Workbook"; }
	ISO_ptr<void>	Read(tag id, istream_ref file) override;
} xlsx;

class XLSMFileHandler : public XLSXFileHandler {
	const char*		GetExt() override { return "xlsm"; }
} xlsm;

ISO_ptr<void> XLSXFileHandler::Read(tag id, istream_ref file) {
	FileHandler		*zip	= Get("zip");
	ISO_ptr<void>	z		= zip->Read(id, file);
	ISO::Browser		xl		= ISO::Browser(z)["xl"];

	ISO::Browser		ssb		= xl["sharedStrings.xml"];
	SharedStrings	ss(memory_reader(ssb).me());

	ISO_ptr<anything>	p(id);

	ISO::Browser		ws		= xl["worksheets"];
	for (int i = 0, n = ws.Count(); i < n; i++) {
		tag	name = ws.GetName(i);
		if (const char *ext = name.find(".xml")) {
			ISO::Browser	w	= ws[i];
			p->Append(ReadWorkSheet(str(name.begin(), ext), memory_reader(w).me(), ss));
		}
	}

	return p;
}

//-----------------------------------------------------------------------------
//	OpenOffice spreadsheet
//-----------------------------------------------------------------------------

class ODSFileHandler : public FileHandler {
	const char*		GetExt() override { return "ods"; }
	const char*		GetDescription() override { return "OpenDocument Spreadsheet"; }
	ISO_ptr<void>	Read(tag id, istream_ref file) override;
} ods;

ISO_ptr<void> ODSFileHandler::Read(tag id, istream_ref file) {
	FileHandler		*zip	= Get("zip");
	ISO_ptr<void>	z		= zip->Read(id, file);
	ISO::Browser		c		= ISO::Browser(z)["content.xml"];

	XMLreader::Data	data;
	memory_reader				mi(c);
	XMLreader				xml(mi);

	ISO_ptr<anything>		p(id);

	while (xml.ReadNext(data, XMLreader::TAG_BEGIN) == XMLreader::TAG_BEGIN) {
		if (data.Is("table:table")) {
			ISO_ptr<ISO_openarray<anything> >	table(data.Find("table:name"));
			p->Append(table);

			while (xml.ReadNext(data, XMLreader::TAG_BEGIN) == XMLreader::TAG_BEGIN) {
				if (data.Is("table:table-row")) {
					anything	*row = &table->Append();

					while (xml.ReadNext(data, XMLreader::TAG_BEGIN) == XMLreader::TAG_BEGIN) {
						if (data.Is("table:table-cell")) {
							while (xml.ReadNext(data, XMLreader::TAG_CONTENT) == XMLreader::TAG_CONTENT)
								row->Append(ISO_ptr<string>(0, data.Content()));
						}
					}
				}
			}
		}
	}
	return p;
}

//-----------------------------------------------------------------------------
//	Microsoft Compound Document
//-----------------------------------------------------------------------------

class MCDFileHandler : public FileHandler {
	const char*		GetDescription() override { return "Microsoft Compound Document"; }
	int				Check(istream_ref file) override { file.seek(0); return file.get<uint64be>() == CompDocHeader::MAGIC ? CHECK_PROBABLE : CHECK_DEFINITE_NO; }
	ISO_ptr<void>	Read(tag id, istream_ref file) override;
} mcd;

ISO_ptr<void> MCDFileHandler::Read(tag id, istream_ref file) {
	CompDocHeader	header = file.get();
	if (!header.valid())
		return ISO_NULL;

	CompDocMaster	master(file, header);
	size_t			dir_size	= master.chain_length(file, header.first_sector);
	malloc_block	dir_buff(dir_size);
	master.read_chain(file, header.first_sector, dir_buff, dir_size);

	uint32				stack[32], *sp = stack;
	pair<ISO_ptr<anything>, uint32>	dir_stack[32], *dir_sp = dir_stack;

	ISO_ptr<anything>	root(id);

	ISO_ptr<anything>	dir	= root;
	int					i	= 0;
	for (;;) {
		CompDocDirEntry &e	= ((CompDocDirEntry*)dir_buff)[i];
		string			name= e.get_name();

		switch (e.type) {
			case CompDocDirEntry::UserStream: {
				ISO_ptr<ISO_openarray<uint8> >	p(name);
				master.read_chain2(file, e.sec_id, p->Create(e.size), e.size);
				dir->Append(p);
				break;
			}

			case CompDocDirEntry::RootStorage:
			case CompDocDirEntry::UserStorage:
				dir->Append(dir_sp->a.Create(name));
				dir_sp->b = e.root;
				++dir_sp;
				break;

		}
		if (e.right != CompDocDirEntry::UNUSED)
			*sp++ = e.right;

		i = e.left;
		if (i == CompDocDirEntry::UNUSED) {
			if (sp > stack) {
				i = *--sp;
			} else {
				if (dir_sp == dir_stack)
					break;
				--dir_sp;
				dir	= dir_sp->a;
				i	= dir_sp->b;
			}
		}
	}
	return root;
}
