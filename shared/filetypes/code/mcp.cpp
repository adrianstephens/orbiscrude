#include "iso/iso_files.h"

//-----------------------------------------------------------------------------
//	Codewarrior project files
//-----------------------------------------------------------------------------

using namespace iso;

class MCPFileHandler : public FileHandler {
	const char*		GetExt() override { return "mcp";					}
	const char*		GetDescription() override { return "Codewarrior file";	}
	ISO_ptr<void>	Read(tag id, istream_ref file) override;
} mcp;

struct MCPheader {
	uint32	cool;		//'cool'
	uint32	a;			//3
	uint32	offset1;	//0x128
	uint32	size1;
	uint32	offset2;
	uint32	size2;
	uint32	count;	//?
	uint32	magic;	//2,3,4,0
	uint32	b, c;	//0,0
	char	description[256];	// 'CodeWarrior Project', or 'Windows Project Settings', or 'Windows Target Data'
};

struct MCPentry {
	uint32	offset;
	uint32	size;
	uint32	tag;
	uint32	id;		// groups mstr, mstl, mstn with id of 0, 1-n, 1000, 1001, 1005; (msti, mtsi have 1005, mtfi has 1001, mtdc, mtgl, mpsi has 1000)
	uint32	index;	// prefs start at 1
	uint32	unused;	// always 0
};

/* tags

 mstr - string table	128-c00, 1889c-c00
 mstl - lengths			{offset, index, 0}
 mstn - ?				1328-200
 msti -

 pref	1ef9-8
 mtlo	b778-234
 mtsl
 mtpl
 mtps
 mtpi
 mtgl
 moti
 mpsi
 mall
 mapl
 PLst	files
 pref ids: 1ae79c5-1aef325; 58c439-594963

CWSettingsWindows:
 pref ids: 5f2eab5, 621aafa, 621bcb2, 621dae0, 621ab8b, 621fa16

TargetDataWindows:
 mtfl
 mtfs
 mtfp
 mtfd
 msti
 mtfi
 mtdc
 mtsi
 msti
 head
 depg
 pref ids: 5f352b4, 5f350c5 (5f2eceb-5f35626)

*/

uint32	prefs[] = {
// CodeWarrior Project
	// elf debug
	0x01AE7D40,0x01AEBAD8,0x01AE861B,0x01AE84E3,0x01AED14C,0x01AECF2D,0x01AEDFA6,0x01AEE682,
	0x01AE9B42,0x01AECF58,0x01AEE35C,0x01AE7BF6,0x01AEBD82,0x01AEBD83,0x01AEF076,0x01AEA726,
	0x01AECE23,0x01AEEBFE,0x01AEC06F,0x01AEC897,0x01AE79C5,0x01AEB623,0x01AEA4BA,0x01AEE9A9,
	0x01AEC715,0x01AEEAE9,0x01AEF325,0x01AECE97,0x01AE8E5A,0x01AEAD40,0x01AE7D4C,0x01AE926B,

	// GROUPS
	0x01AE8DE9,

	// elf profile
	0x00593911,0x00592259,0x00590BE1,0x00593C4E,0x0058C241,0x00590A1B,0x00590E01,0x0058D682,
	0x00590751,0x0059270E,0x005910B4,0x00593C4D,0x005915F6,0x00593C53,0x0058ED58,0x0058EA61,
	0x0058C0F9,0x0058E638,0x00590320,0x00592B87,0x00593AEA,0x0058DD76,0x00591F9E,0x0058C439,
	0x00592F84,0x00593231,0x0058E414,0x00593C01,0x005907DF,0x00593AAC,0x0059337A,0x0058E08F,

	// elf release
	0x0058DD58,0x005939CC,0x0058CFAE,0x00594088,0x00593D6D,0x0058D5FB,0x0058FEF7,0x0059069C,
	0x005946EF,0x00594963,0x0058D10D,0x00590CAB,0x0058E629,0x00590734,0x0058D386,0x00591FDF,
	0x0058CFF9,0x00593C73,0x0058D47D,0x0058DB8F,0x0058D65F,0x0058F797,0x005947FB,0x00594039,
	0x0058F9C7,0x00592DE3,0x00591822,0x00593E3E,0x00594316,0x00593A86,0x0058CC1B,0x00593060,

// Windows Project Settings
	0x05F2EAB5,0x0621AAFA,0x0621BCB2,0x0621DAE0,0x0621AB8B,0x0621FA16,
// Windows Target Data
	0x05F352B4,0x05F350C5,0x05F3031F,0x05F31E2B,0x05F34AA0,0x05F2F841,0x05F35626,0x05F2F105,
	0x05F30601,0x05F2FE7F,0x05F2FC5E,0x05F3196D,0x05F33A76,0x05F2EE61,0x05F3097D,0x05F2F4A1,
	0x05F32E39,0x05F33CBE,0x05F35DC8,0x05F3458F,0x05F307F8,0x05F35113,0x05F33F4D,0x05F33B6B,
	0x05F34075,0x05F2ECEB,0x05F34AE5,0x05F354F2,0x05F311DC,0x05F31E09,0x05F2F8ED,0x05F32C7B,
};

ISO_ptr<void> MCPFileHandler::Read(tag id, istream_ref file) {
	MCPheader	header;
	if (!file.read(header) || header.cool != 'cool')
		return ISO_NULL;

	ISO_ptr<anything>	t(id);

	int			num		= header.count;
	MCPentry	*entries = new MCPentry[num];
	file.seek(header.offset2);
	file.readbuff(entries, sizeof(MCPentry) * num);

	for (int i = 0; i < num; i++) {
		MCPentry	&e = entries[i];
		tag			id = format_string("%c%c%c%c_%x", e.tag>>24, e.tag>>16, e.tag>>8, e.tag, e.id);

		if (e.tag == 'mstr') {
			file.seek(e.offset);
			malloc_block	temp(e.size);
			file.readbuff(temp, e.size);
			ISO_ptr<ISO_openarray<string> >	p(id);
			for (char *s = (char*)temp; *s && s < (char*)temp + e.size; s += strlen(s) + 1)
				p->Append(s);
			t->Append(p);
		} else {
			ISO_ptr<ISO_openarray<uint8> >	p(id);
			file.seek(e.offset);
			file.readbuff(p->Create(e.size), e.size);
			t->Append(p);
		}
	}
	return t;
}
