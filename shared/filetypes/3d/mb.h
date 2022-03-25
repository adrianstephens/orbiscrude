#ifndef _FILETYPE_3D_MB
#define _FILETYPE_3D_MB

#include "iso/iso_files.h"
#include "iff.h"


namespace maya
{

struct MAYA_IFF_ID {
	char c[4];
	MAYA_IFF_ID()			{}
	MAYA_IFF_ID(iso::uint32be i)	{ memcpy(c, &i, 4); }
	MAYA_IFF_ID& operator=(char* _c) { memcpy(c, _c, 4); return *this; }
	bool operator==(iso::uint32 i) const { return *(iso::uint32be*)c == i; }
	operator iso::tag2() const 
	{ 
		return tostring(); 
	}//str(c, 4); }

	iso::string tostring() const; 

};



struct STRING
{
	iso::string value;
};

struct FINF
{
	iso::string name;
	iso::string value;
};

struct PLUG
{
	iso::string name;
	iso::string description;
};

struct CREA
{
	//bad guesses
/*	enum Flags
	{
		f_parented = 0x4,
		f_parent = 0x8,
	};*/

	iso::xint8 flags;
	iso::string name;
	iso::string parent;
};

struct ATTR 
{
	MAYA_IFF_ID id;
	iso::xint16 flags;
	iso::uint32 value_count; //number of children
	iso::dynamic_array<MAYA_IFF_ID> type_id; // some kind of supported ids for compound types ?
	iso::string   name_a;
	iso::string   name_b;
	iso::string	 parent;
	iso::string	 enum_name;
	MAYA_IFF_ID id_after_names;
	iso::float64 min;
	iso::float64 max;
	iso::float64 unk1;
	iso::float64 unk2;
	iso::float64 def;

/*
aBOL
	MAYA_IFF_ID _unknown_id;
	uint8   _somebytes[16];
	MAYA_IFF_ID type_id;
	uint8 default_value[8];//????
*/
/*
aTYP
	string short_name;
*/
	/*
	flags
	- 0x0002 = min
	- 0x0004 = max
	- 0x0008 = default
	- 0x0010 = parent
	- 0x0020 = parented
	- 0x0040 = cached internally
	- 0x0080 = internal set (-is)?
	- 0x0200 = enum value
	- 0x0400 = use as colour
	- 0x4000 = keyable

	*/

	enum flags
	{
		f_min			= 0x0002,
		f_max			= 0x0004,
		f_default		= 0x0008,
		f_parent		= 0x0010,
		f_parented		= 0x0020,
		f_cached		= 0x0040,
		f_internal_set	= 0x0080,
		f_enum			= 0x0200,
		f_colour		= 0x0400,
		f_unknown1		= 0x0800,
		f_unknown2		= 0x1000,
		f_keyable		= 0x4000
	};

	void Print();
};

struct DBLE
{
	iso::string name;
	iso::xint8 _flags; // I thought this was value bit count, but it isn't
	iso::dynamic_array<iso::uint32> _wtf;//sometimes there is a leading int?
	iso::dynamic_array<iso::float64> values;
};

struct CMPD
{
	iso::string name;
	iso::xint8 _flags; // I thought this was value bit count, but it isn't
	iso::dynamic_array<iso::xint8> values;
};

struct CMP_list
{
	iso::string name;
	iso::xint8 _flags; // I thought this was value bit count, but it isn't
	iso::uint32 count; //number in the list
	iso::dynamic_array<MAYA_IFF_ID> type_id; // some kind of supported ids for compound types ?
	iso::dynamic_array<iso::xint8> values;
};


struct LNG2
{
	iso::string name;
	iso::xint8 bits; // I thought this was value bit count, but it isn't
	iso::dynamic_array<iso::int64> values;
};

struct FLT3
{
	iso::string name;
	iso::xint8 bits; // I thought this was value bit count, but it isn't
	iso::dynamic_array<iso::float64> values;
};
typedef FLT3 FLT2;

struct DBL3
{
	iso::string name;
	iso::xint8 bits; // I thought this was value bit count, but it isn't
	iso::dynamic_array<iso::float64> values;
};
typedef DBL3 DBL2;

struct MATR
{
	iso::string name;
	iso::uint8 some_byte; //0x20?
	iso::dynamic_array<iso::float64> values;
};

struct FLGS
{
	iso::string name;
	iso::dynamic_array<iso::xint8> flags;
};

struct STR
{
	iso::string name;
	iso::string value;
};

struct STR_list
{
	iso::string name;
	iso::xint8 bits; // bit count for the length value? - probably not
	iso::dynamic_array<iso::string> values;
};

struct SLCT
{
	MAYA_IFF_ID id;
	iso::string target;
};

struct CWFL
{
	enum Flags
	{
		f_next_available = 0x01,
		f_lock = 0x02 //seen from _LightRadius -> sx (and sy, sz)
		//unseen options that may need a flag
		//force(f)
		//referenceDest(rd)
	};
	iso::xint8 flags;
	iso::string from_node;
	iso::string from_attribute;

	iso::string to_node;
	iso::string to_attribute;


};

}
using namespace iso;

ISO_DEFUSERCOMP(maya::MAYA_IFF_ID, 1) {
	ISO_SETFIELD(0, c);
}};

ISO_DEFUSERCOMP(maya::STRING, 1) {
	ISO_SETFIELD(0, value);
}};


ISO_DEFUSERCOMP(maya::FINF, 2) {
	ISO_SETFIELD(0, name);
	ISO_SETFIELD(1, value);
}};

ISO_DEFUSERCOMP(maya::PLUG, 2) {
	ISO_SETFIELD(0, name);
	ISO_SETFIELD(1, description);
}};

ISO_DEFUSERCOMP(maya::CREA, 2) {
	ISO_SETFIELD(0, flags);
	ISO_SETFIELD(1, name);
}};

ISO_DEFUSERCOMP(maya::ATTR, 6) {
	ISO_SETFIELD(0, id);
	ISO_SETFIELD(1, flags);
	ISO_SETFIELD(2, value_count);
	ISO_SETFIELD(3, type_id);
	ISO_SETFIELD(4, name_a);
	ISO_SETFIELD(5, name_b);
}};

ISO_DEFUSERCOMP(maya::FLGS, 2) {
	ISO_SETFIELD(0, name);
	ISO_SETFIELD(1, flags);
}};

ISO_DEFUSERCOMP(maya::DBLE, 4) {
	ISO_SETFIELD(0, name);
	ISO_SETFIELD(1, _flags);
	ISO_SETFIELD(2, _wtf);
	ISO_SETFIELD(3, values);
}};

ISO_DEFUSERCOMP(maya::LNG2, 3) {
	ISO_SETFIELD(0, name);
	ISO_SETFIELD(1, bits);
	ISO_SETFIELD(2, values);
}};


ISO_DEFUSERCOMP(maya::FLT3, 3) {
	ISO_SETFIELD(0, name);
	ISO_SETFIELD(1, bits);
	ISO_SETFIELD(2, values);
}};

ISO_DEFUSERCOMP(maya::DBL3, 3) {
	ISO_SETFIELD(0, name);
	ISO_SETFIELD(1, bits);
	ISO_SETFIELD(2, values);
}};

ISO_DEFUSERCOMP(maya::MATR, 2) {
	ISO_SETFIELD(0, name);
	ISO_SETFIELD(1, values);
}};

ISO_DEFUSERCOMP(maya::CMPD, 3) {
	ISO_SETFIELD(0, name);
	ISO_SETFIELD(1, _flags);
	ISO_SETFIELD(2, values);
}};

ISO_DEFUSERCOMP(maya::CMP_list, 5) {
	ISO_SETFIELD(0, name);
	ISO_SETFIELD(1, _flags);
	ISO_SETFIELD(2, count);
	ISO_SETFIELD(3, type_id);
	ISO_SETFIELD(4, values);
}};

ISO_DEFUSERCOMP(maya::STR, 2) {
	ISO_SETFIELD(0, name);
	ISO_SETFIELD(1, value);
}};

ISO_DEFUSERCOMP(maya::STR_list, 3) {
	ISO_SETFIELD(0, name);
	ISO_SETFIELD(1, bits);
	ISO_SETFIELD(2, values);
}};

ISO_DEFUSERCOMP(maya::SLCT, 2) {
	ISO_SETFIELD(0, id);
	ISO_SETFIELD(1, target);
}};

ISO_DEFUSERCOMP(maya::CWFL, 5) {
	ISO_SETFIELD(0, flags);
	ISO_SETFIELD(1, from_node);
	ISO_SETFIELD(2, from_attribute);
	ISO_SETFIELD(3, to_node);
	ISO_SETFIELD(4, to_attribute);
}};


#endif