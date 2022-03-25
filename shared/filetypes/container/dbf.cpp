#include "iso/iso_files.h"
#include "extra/date.h"

using namespace iso;

/*
00h /   0| Version number      *1|  ^
         |-----------------------|  |
01h /   1| Date of last update   |  |
02h /   2|      YYMMDD        *21|  |
03h /   3|                    *14|  |
         |-----------------------|  |
04h /   4| Number of records     | Record
05h /   5| in data file          | header
06h /   6| ( 32 bits )        *14|  |
07h /   7|                       |  |
         |-----------------------|  |
08h /   8| Length of header   *14|  |
09h /   9| structure ( 16 bits ) |  |
         |-----------------------|  |
0Ah /  10| Length of each record |  |
0Bh /  11| ( 16 bits )     *2 *14|  |
         |-----------------------|  |
0Ch /  12| ( Reserved )        *3|  |
0Dh /  13|                       |  |
         |-----------------------|  |
0Eh /  14| Incomplete transac.*12|  |
         |-----------------------|  |
0Fh /  15| Encryption flag    *13|  |
         |-----------------------|  |
10h /  16| Free record thread    |  |
11h /  17| (reserved for LAN     |  |
12h /  18|  only )               |  |
13h /  19|                       |  |
         |-----------------------|  |
14h /  20| ( Reserved for        |  |            _        |=======================| ______
         |   multi-user dBASE )  |  |           / 00h /  0| Field name in ASCII   |  ^
         : ( dBASE III+ - )      :  |          /          : (terminated by 00h)   :  |
         :                       :  |         |           |                       |  |
1Bh /  27|                       |  |         |   0Ah / 10|                       |  |
         |-----------------------|  |         |           |-----------------------| For
1Ch /  28| MDX flag (dBASE IV)*14|  |         |   0Bh / 11| Field type (ASCII) *20| each
         |-----------------------|  |         |           |-----------------------| field
1Dh /  29| Language driver     *5|  |        /    0Ch / 12| Field data address    |  |
         |-----------------------|  |       /             |                     *6|  |
1Eh /  30| ( Reserved )          |  |      /              | (in memory !!!)       |  |
1Fh /  31|                     *3|  |     /       0Fh / 15| (dBASE III+)          |  |
         |=======================|__|____/                |-----------------------|  | <-
20h /  32|                       |  |  ^          10h / 16| Field length       *22|  |   |
         |- - - - - - - - - - - -|  |  |                  |-----------------------|  |   | *7
         |                    *19|  |  |          11h / 17| Decimal count      *23|  |   |
         |- - - - - - - - - - - -|  |  Field              |-----------------------|  | <-
         |                       |  | Descriptor  12h / 18| ( Reserved for        |  |
         :. . . . . . . . . . . .:  |  |array     13h / 19|   multi-user dBASE)*18|  |
         :                       :  |  |                  |-----------------------|  |
      n  |                       |__|__v_         14h / 20| Work area ID       *16|  |
         |-----------------------|  |    \                |-----------------------|  |
      n+1| Terminator (0Dh)      |  |     \       15h / 21| ( Reserved for        |  |
         |=======================|  |      \      16h / 22|   multi-user dBASE )  |  |
      m  | Database Container    |  |       \             |-----------------------|  |
         :                    *15:  |        \    17h / 23| Flag for SET FIELDS   |  |
         :                       :  |         |           |-----------------------|  |
    / m+263                      |  |         |   18h / 24| ( Reserved )          |  |
         |=======================|__v_ ___    |           :                       :  |
         :                       :    ^       |           :                       :  |
         :                       :    |       |           :                       :  |
         :                       :    |       |   1Eh / 30|                       |  |
         | Record structure      |    |       |           |-----------------------|  |
         |                       |    |        \  1Fh / 31| Index field flag    *8|  |
         |                       |    |         \_        |=======================| _v_____
         |                       | Records
         |-----------------------|    |
         |                       |    |          _        |=======================| _______
         |                       |    |         / 00h /  0| Record deleted flag *9|  ^
         |                       |    |        /          |-----------------------|  |
         |                       |    |       /           | Data               *10|  One
         |                       |    |      /            : (ASCII)            *17: record
         |                       |____|_____/             |                       |  |
         :                       :    |                   |                       | _v_____
         :                       :____|_____              |=======================|
         :                       :    |
         |                       |    |
         |                       |    |
         |                       |    |
         |                       |    |
         |                       |    |
         |=======================|    |
         |__End_of_File__________| ___v____  End of file ( 1Ah )  *11
*/


enum SIGNATURE {
	SIG_FoxBase				= 0x02, //FoxBase
	SIG_NoDBT				= 0x03, //File without DBT
	SIG_dBASEIV				= 0x04, //dBASE IV w/o memo file
	SIG_dBASEV				= 0x05, //dBASE V w/o memo file
	SIG_VISUAL_OBJECTS		= 0x07, //VISUAL OBJECTS (first 1.0 versions) for the Dbase III files w/o memo file

	SIG_Visual_FoxPro		= 0x30, //Visual FoxPro
	SIG_Visual_FoxPro_DBC	= 0x30, //Visual FoxPro w. DBC
	SIG_Visual_FoxPro_Inc	= 0x31, //Visual FoxPro w. AutoIncrement field
	SIG_Memo				= 0x43, //.dbv memo var size (Flagship)
	SIG_dBASEIV_memo		= 0x7B, //dBASE IV with memo
	SIG_DBT					= 0x83, //File with DBT
	SIG_dBASEIII_memo		= 0x83, //dBASE III+ with memo file
	SIG_VISUAL_OBJECTS_memo	= 0x87, //VISUAL OBJECTS (first 1.0 versions) for the Dbase III files (NTX clipper driver) with memo file
	SIG_dBASEIV_memo2		= 0x8B, //dBASE IV w. memo
	SIG_dBASEIV_SQL			= 0x8E, //dBASE IV w. SQL table
	SIG_DBV_memo			= 0xB3, //.dbv and .dbt memo (Flagship)
	SIG_ClipperSIX_memo		= 0xE5, //Clipper SIX driver w. SMT memo file.
	SIG_FoxPro_mem			= 0xF5, //FoxPro w. memo file
	SIG_FoxPro				= 0xFB, //FoxPro ???

	// dBASE IV bit flags
	SIG_VersionMask			= 7,
	SIG_MemoMask			= 8,
	SIG_SQLMask				= 0x70,
	SIG_DBTMask				= 0x80,
};

enum TYPE {
	//Type				Code				Length							Description
	TYPE_Character			= 'C',			//< 254							ASCII text < 254 characters long in dBASE. Character fields can be up to 32 KB long (in Clipper and FoxPro) using decimal count as high byte in field length. It's possible to use up to 64KB long fields by reading length as unsigned. Only fields <= 100 characters can be indexed.
	TYPE_Number				= 'N',			//<18							ASCII text up till 18 characters long (include sign and decimal point). Valid characters: "0" - "9" and "-". Number fields can be up to 20 characters long in FoxPro and Clipper.
	TYPE_Logical			= 'L',			//1								Boolean/byte (8 bit) Legal values:  Y,y,N,n,T,t,F,f,?(unintialised)
	TYPE_Date				= 'D',			//8								Date in format YYYYMMDD. A date like 0000-00- 00 is *NOT* valid.
	TYPE_Memo				= 'M',			//10							Pointer to ASCII text field in memo file 10 digits representing a pointer to a DBT block (default is blanks).
	TYPE_Float				= 'F',			//20							(dBASE IV and later, FoxPro, Clipper) 20 digits
	TYPE_Binary				= 'B',			//??							(dBASE V) Like Memo fields, but not for text processing; (FoxPro/FoxBase) Double integer *NOT* a memo field
	TYPE_General			= 'G',			//??							(dBASE V: like Memo) OLE Objects in MS Windows versions
	TYPE_Picture			= 'P',			//??							(FoxPro) Like Memo fields, but not for text processing.
	TYPE_Currency			= 'Y',			//??							(FoxPro)
	TYPE_DateTime			= 'T',			//8 bytes (two longs)			(FoxPro) The first 4 bytes are a 32-bit little-endian integer representation of the Julian date, where Oct. 15, 1582 = 2299161 per www.nr.com/julian.html The last 4 bytes are a 32-bit little-endian integer time of day represented as milliseconds since midnight.
	TYPE_Integer			= 'I',			//4 byte little endian integer	(FoxPro)
	TYPE_VariField			= 'V',			//2-10 bytes					There are weakly-typed and strongly-typed VariFields.
	TYPE_Variant 			= 'X',			//								(CLIP)
	TYPE_Timestamp			= '@',			//8 bytes (two longs)			First long repecents date and second long time. Date is the number of days since January 1st, 4713 BC. Time is hours * 3600000L + minutes * 60000L + seconds * 1000L.
	TYPE_Double				= 'O',			//8 bytes						(no conversion)
	TYPE_Autoincrement		= '+',			//long							(no conversion)
	//Character name variable	= N/A		//< 254							1-254 characters (64 KB in FoxBase and Clipper)
};
/*
Weakly-typed VariFields allow to store a portion of any character data in the .DBF field itself, with any additional amount, if any, being automatically stored in the MEMO file. This substantially reduces the amount of disk space required to store the data. This is sort of a cross between using a CHARACTER field and a MEMO field at the same time.
A 6 byte pointer is maintained at the end of the weakly-typed "V" field entry within the DBF file. When you define the field width for a new database file, keep in mind that weakly-typed "V" fields require this additional 6 bytes at the end.
Strongly-typed VariFields store DATE values in only three bytes, instead of eight, and LONG INTEGER values (up to ~2G) in only 4 bytes. Any "V" field defined with 4 bytes is automatically considered a strongly-typed integer, and "V" fields defined with 3 bytes are automatically considered strongly-typed DATE values. No additional space is required (6-byte pointer) for strongly typed VariFields.
FlagShip has additional types

V 10   Variable  Variable, bin/asc data in .dbv      (.dbf type = 0xB3)
					4bytes bin= start pos in memo
					4bytes bin= block size
					1byte     = subtype
					1byte     = reserved (0x1a)
					10spaces if no entry in .dbv

2 2    short int binary int max +/- 32767       (.dbf type = 0xB3)
4 4    long int  binary int max +/- 2147483647  (.dbf type = 0xB3)
8 8    double    binary signed double IEEE      (.dbf type = 0xB3)
*/

template<typename T, typename S> struct as_string {
	S	s;
	operator T() const { return from_string<T>(s); }
};

template<typename T, typename S> struct ISO::def<as_string<T, S> > : ISO::VirtualT2<as_string<T, S> > {
	static T	Deref(const as_string<T, S> &a)	{ return a; }
};

template<typename T> struct ISO::def<as_string<T, embedded_string> > : ISO::VirtualT2<as_string<T, embedded_string> > {
	def(uint32 len) { param16 = len; }
	ISO::Browser2	Deref(const as_string<T, embedded_string> &a)	{ return ISO::MakeBrowser(from_string<T>(a.s.begin(), a.s.begin() + param16)); }
};

struct DBFDate3 {
	uint8	year, month, day;
	operator DateTime() const { return DateTime(1900 + year, month, day); }
};
struct DBFDate {
	char	val[8];
	operator DateTime() const { return DateTime(from_string<int>(val, val + 4), from_string<int>(val + 4, val + 6), from_string<int>(val + 6, val + 8)); }
};
struct DBFDateTime {
	uint32le	date, time;
	operator DateTime() const {
		return DateTime::Days(date) + DateTime::Secs(time) / 1000;
	}
};
struct DBFTimeStamp {
	uint32le	date, time;
	operator DateTime() const {
		static DateTime offset = DateTime(-4713, 0);
		return offset + DateTime::Days(date) + DateTime::Secs(time) / 1000;
	}
};

struct DBFVariField10 {
	uint32	offset;		// start pos in memo
	uint32	size;		// block size
	char	type;		// subtype
	uint8	reserved;	// reserved (0x1a)
};

template<> struct ISO::def<DateTime> : ISO::VirtualT2<DateTime> {
	static ISO_ptr<void>	Deref(const DateTime &t) {
		return ISO_ptr<string>(0, to_string(t));
	}
};
template<> struct ISO::def<DBFDate3> : ISO::VirtualT2<DBFDate3> {
	static ISO_ptr<void>	Deref(const DBFDate3 &t)		{ return ISO_ptr<DateTime>(tag2(), t); }
};
template<> struct ISO::def<DBFDate> : ISO::VirtualT2<DBFDate> {
	static ISO_ptr<void>	Deref(const DBFDate &t)			{ return ISO_ptr<DateTime>(tag2(), t); }
};
template<> struct ISO::def<DBFDateTime> : ISO::VirtualT2<DBFDateTime> {
	static ISO_ptr<void>	Deref(const DBFDateTime &t)		{ return ISO_ptr<DateTime>(tag2(), t); }
};
template<> struct ISO::def<DBFTimeStamp> : ISO::VirtualT2<DBFTimeStamp> {
	static ISO_ptr<void>	Deref(const DBFTimeStamp &t)	{ return ISO_ptr<DateTime>(tag2(), t); }
};
ISO_DEFUSERCOMPV(DBFVariField10, offset, size, type);

enum Language {
	DOS_USA						= 0x01,	//code page 437
	DOS_Multilingual			= 0x02,	//code page 850
	Windows_ANSI				= 0x03,	//code page 1252
	Standard_Macintosh			= 0x04,	//
	EE_MSDOS					= 0x64,	//code page 852
	Nordic_MSDOS				= 0x65,	//code page 865
	Russian_MSDOS				= 0x66,	//code page 866
	Icelandic_MSDOS				= 0x67,	//
	Czech_MSDOS					= 0x68,	//
	Polish_MSDOS				= 0x69,	//
	Greek_MSDOS					= 0x6A,	//
	Turkish_MSDOS				= 0x6B,	//
	Russian_Macintosh			= 0x96,	//
	Eastern_European_Macintosh	= 0x97,	//
	Greek_Macintosh				= 0x98,	//
	Windows_EE					= 0xC8,	//code page 1250
	Russian_Windows				= 0xC9,	//
	Turkish_Windows				= 0xCA,	//
	Greek_Windows				= 0xCB,	//
};

struct DBFheader {
	uint8	version;
	DBFDate3	date;
	uint32	num_records;
	uint16	header_len;
	uint16	record_len;
	uint16	reserved;
	uint8	incomplete_transaction;
	uint8	ecryption;
	uint32	free_thread;		// (reserved for LAN)
	uint8	multi_user[8];		// (reserved for multi-user dBASE dBASE III+ -)
	uint8	MDX;				// (dBASE IV)
	uint8	language;
	uint16	reserved2;
};

struct FieldDescriptor {
	char	name[11];
	char	type;
	uint32	address;
	uint8	length;
	uint8	decimal_count;
	uint16	multi_user;			// (reserved for multi-user dBASE)
	uint8	work_area_id;
	uint8	multi_user2[2];		// (reserved for multi-user dBASE)
	uint8	flag;				// for SET FIELDS
	uint8	reserved[7];
	uint8	index_field_flag;
};

struct DBF : ISO::VirtualDefaults {
	malloc_block				data;
	size_t						stride;
	ISO::TypeComposite			*comp;
	dynamic_array<ISO::Type*>	types;

	DBF(int n) {
		comp = new(n) ISO::TypeComposite(n);
	}
	~DBF() {
		for (auto &i : types)
			delete i;
		delete (ISO::Type*)comp;
	}
	int				Count()			{ return int(data.length() / stride); }
	ISO::Browser2	Index(int i)	{ return ISO::Browser(comp, data + i * stride); }
};

ISO_DEFUSERVIRT(DBF);

class DBFFileHandler : public FileHandler {

	const char*		GetExt() override { return "dbf";	}
	const char*		GetDescription() override { return "dBASE file"; }

	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		DBFheader	header;
		file.read(header);

		dynamic_array<FieldDescriptor>	fields;
		uint32		nfields	= (header.header_len - sizeof(header)) / sizeof(FieldDescriptor);
		fields.read(file, nfields);

		ISO_ptr<DBF>	p(id, nfields);
		auto			*element	= p->comp->begin();
		uint32			offset		= 1;
		for (auto &i : fields) {
			const ISO::Type	*type = 0;
			switch (i.type) {
				case TYPE_Character:
					type = new ISO::TypeArray(ISO::getdef<char>(), i.length, 1);
					p->types.push_back(unconst(type));
					break;
				case TYPE_Number:
					if (i.length > 9)
						type = new ISO::def<as_string<int64, embedded_string> >(i.length);
					else
						type = new ISO::def<as_string<int32, embedded_string> >(i.length);
					p->types.push_back(unconst(type));
					break;
				case TYPE_Logical:
					type = new ISO::def<as_string<bool, embedded_string> >(i.length);
					p->types.push_back(unconst(type));
					break;
				case TYPE_Date:
					type = ISO::getdef<DBFDate>();
					break;
				case TYPE_Memo:
					break;
				case TYPE_Float:
					if (i.decimal_count > 7)
						type = new ISO::def<as_string<double, embedded_string> >(i.length);
					else
						type = new ISO::def<as_string<float, embedded_string> >(i.length);
					p->types.push_back(unconst(type));
					break;
				case TYPE_Binary:
					type = new ISO::TypeArray(ISO::getdef<uint8>(), i.length, 1);
					p->types.push_back(unconst(type));
					break;
				case TYPE_General:
					break;
				case TYPE_Picture:
					break;
				case TYPE_Currency:
					break;
				case TYPE_DateTime:
					type = ISO::getdef<DBFDateTime>();
					break;
				case TYPE_Integer:
					type = ISO::getdef<int>();
					break;
				case TYPE_VariField:
					switch (i.length) {
						case 2:		type = ISO::getdef<int16>(); break;
						case 3:		type = ISO::getdef<DBFDate3>(); break;
						case 4:		type = ISO::getdef<int32>(); break;
						case 8:		type = ISO::getdef<double>(); break;
						case 10:	type = ISO::getdef<DBFVariField10>(); break;
						default:	break;
					}
					break;
				case TYPE_Variant:
					break;
				case TYPE_Timestamp:
					type = ISO::getdef<DBFTimeStamp>();
					break;
				case TYPE_Double:
					type = ISO::getdef<double>();
					break;
				case TYPE_Autoincrement:
					break;
			}
			element++->set(tag(i.name), type, offset);
			offset += type->GetSize();
		}

		p->stride = header.record_len;
		size_t	size = header.num_records * p->stride;
		file.seek(header.header_len);
		file.readbuff(p->data.create(size), size);

//		ISO::Type	*type = new ISO::TypeArray(comp, header.num_records);
//		dbf->types.push_back(type);
/*
		ISO_ptr<void>	p	= MakePtr(type, id);
		file.seek(header.header_len);
		file.readbuff(p, type->GetSize());
*/
		return p;
	}
} dbf;


