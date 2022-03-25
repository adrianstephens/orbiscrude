#include "iso/iso_files.h"

using namespace iso;

void write(ostream_ref file, const ISO::Type *type, void *data, char separator) {
	char	buffer[256];
	switch (type->GetType()) {
		case ISO::INT:
			sprintf(buffer, "%i", ((ISO::TypeInt*)type)->get(data));
			file.writebuff(buffer, strlen(buffer));
			break;

		case ISO::FLOAT:
			sprintf(buffer, "%f", ((ISO::TypeFloat*)type)->get(data));
			file.writebuff(buffer, strlen(buffer));
			break;

		case ISO::STRING:
			if (const char *s = (const char*)type->ReadPtr(data))
				file.writebuff(s, strlen(s));
			break;

		case ISO::COMPOSITE: {
			const ISO::TypeComposite	*comp = (const ISO::TypeComposite*)type;
			for (int i = 0; i < comp->Count(); i++) {
				if (i)
					file.putc(separator);
				write(file, (*comp)[i].type, (char*)data + (*comp)[i].offset, separator);
			}
			break;
		}
	}
}


bool write(ISO_ptr<void> p, ostream_ref file, char separator) {
	const ISO::Type	*type = p.GetType();
	if (type->GetType() == ISO::OPENARRAY) {
		ISO_openarray<char>	*array	= p;
		const ISO::Type	*subtype	= ((ISO::TypeOpenArray*)type)->subtype;
		int				subsize		= ((ISO::TypeOpenArray*)type)->subsize;

		for (int i = 0, n = array->Count(); i < n; i++) {
			write(file, subtype, *array + subsize * i, separator);
			file.putc('\n');
		}
		return true;
	}
	return false;
}

class PsionDBFFileHandler : public FileHandler {
	const char*		GetExt() override { return "dbf";	}
	ISO_ptr<void>	Read(tag id, istream_ref file) override;
	bool			Write(ISO_ptr<void> p, ostream_ref file) override { return false;	}
} psion_dbf;

class CSVFileHandler : public FileHandler {
	const char*		GetExt() override { return "csv";	}
	ISO_ptr<void>	Read(tag id, istream_ref file) override { return ISO_NULL;	}
	bool			Write(ISO_ptr<void> p, ostream_ref file) override { return write(p, file, ','); }
} csv;

class TSVFileHandler : public FileHandler {
	const char*		GetExt() override { return "tsv";	}
	ISO_ptr<void>	Read(tag id, istream_ref file) override { return ISO_NULL;	}
	bool			Write(ISO_ptr<void> p, ostream_ref file) override { return write(p, file, '\t'); }
} tsv;

#define DBF_IDSTRING "OPLDatabaseFile"
#define DBF_VERCREAT 4111
#define DBF_VERREAD  4111

// DBF record types
#define DBF_RECORD_DELETED			0
#define DBF_RECORD_DATA				1
#define DBF_RECORD_FIELDINFO		2
#define DBF_RECORD_DESCRIPTOR		3
#define DBF_RECORD_PRIVATESTART		4
#define DBF_RECORD_PRIVATEEND		7
#define DBF_RECORD_VOICEDATA		14
#define DBF_RECORD_RESERVED			15

// DBF subrecord types
#define DBF_SUBRECORD_TABSIZE		1
#define DBF_SUBRECORD_FIELDLABELS	4
#define DBF_SUBRECORD_DISPLAY		5
#define DBF_SUBRECORD_PRINTER		6
#define DBF_SUBRECORD_PRINTERDRIVER	7
#define DBF_SUBRECORD_HEADER		8
#define DBF_SUBRECORD_FOOTER		9
#define DBF_SUBRECORD_DIAMOND		10
#define DBF_SUBRECORD_SEARCH		11
#define DBF_SUBRECORD_AGENDA		15

// DBF field types
#define DBF_FIELD_WORD				0
#define DBF_FIELD_LONG				1
#define DBF_FIELD_REAL				2
#define DBF_FIELD_QSTR				3
#define DBF_FIELD_QSTR_CONTINUATION	20
#define DBF_FIELD_QSTR_LINEFEED		21

static string readstr(istream_ref file)
{
	char	buffer[4096], *p = buffer;
	int		c;
	while ((c = file.getc()) && c != EOF)
		*p++ = c;
	*p = 0;
	return string(buffer);
}

struct PsionRecord {
	uint16	size:12;
	uint16	type:4;
};

ISO_ptr<void> PsionDBFFileHandler::Read(tag id, istream_ref file) {
	if (readstr(file) != DBF_IDSTRING)
		return ISO_NULL;

	uint16le	w1		= file.get();
	uint16le	hdrsize	= file.get();
	uint16le	w2		= file.get();
	file.seek_cur(hdrsize - 22);

	int			fieldtypes[32];
	int			numfields	= 0;
	int			total		= 0;
	bool		allstrings	= true;

	ISO_ptr<void>		p;
	ISO_openarray<void>	*array;
	ISO::TypeComposite	*comp;

	while (!file.eof()) {
		int			pos = file.tell();
		PsionRecord	r = file.get();
		switch (r.type) {

			case DBF_RECORD_FIELDINFO: {
				static ISO::Type *iso_types[] = {
					ISO::getdef<int16>(),
					ISO::getdef<int32>(),
					ISO::getdef<float>(),
					ISO::getdef<string>(),
				};
				iso::array<ISO::Element,32>	elements;
				int		offset	= 0;
				for (int recsize = r.size; recsize > 0; recsize--) {
					int	c = file.getc();
					if (c == EOF) break;

					elements[numfields].type	= iso_types[c];
					elements[numfields].offset	= offset;
					offset += (elements[numfields].size = iso_types[c]->GetSize());

					fieldtypes[numfields++] = c;
					if (c != DBF_FIELD_QSTR)
						allstrings = false;
				}
				comp	= new(numfields) ISO::TypeComposite(numfields);
				for (int i = 0; i < numfields; i++)
					(*comp)[i] = elements[i];
				array	= p = MakePtr(new ISO::TypeOpenArray(comp, offset), id);
				break;
			}

			case DBF_RECORD_DESCRIPTOR:
//				DBF_DisplayDescriptorRecord(file, r.size);
//				break;

			case DBF_RECORD_VOICEDATA:
			case DBF_RECORD_RESERVED:
			case DBF_RECORD_PRIVATESTART:
			case DBF_RECORD_PRIVATESTART+1:
			case DBF_RECORD_PRIVATESTART+2:
			case DBF_RECORD_PRIVATEEND:
			case DBF_RECORD_DELETED:
				file.seek_cur(r.size);
				break;

			case DBF_RECORD_DATA:
			default: {
				int		recsize	= r.size;
				void	*data	= array->Append(comp);
				total++;
				for (int i = 0; recsize && i < numfields; i++)	{
					ISO::Element	&e	= (*comp)[i];
					if (e.type->GetType() == ISO::STRING) {
						int		c	= file.getc();
//						if (c == 0) {
//							file.seek_cur(recsize - 1);
//							recsize = 0;
//							break;
//						}
						malloc_block	s(c + 1);
						file.readbuff(s, c);
						((char*)s)[c]	= 0;
						e.type->WritePtr((char*)data + e.offset, s);
						recsize	-= c + 1;
					} else {
						file.readbuff((char*)data + e.offset, e.size);
						recsize	-= e.size;
					}
					if (recsize < 0) {
						int	pos = file.tell();
						file.seek(pos + recsize);
						break;
//						return p;
					}
				}
				/* Any extra fields are qstr (EK) */
				while (recsize > 0)	{
					int		c	= file.getc();
					if (c == EOF)
						return p;
					malloc_block	s(c + 1);
					file.readbuff(s, c);
					((char*)s)[c]	= 0;
					recsize -= c + 1;
				}
				break;
			}
		}
	}
	return p;
}
