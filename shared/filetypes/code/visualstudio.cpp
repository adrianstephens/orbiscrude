#include "iso/iso_files.h"
#include "iso/iso_convert.h"
#include "extra/xml.h"
#include "base/algorithm.h"
#include <Objbase.h>

using namespace iso;

//-----------------------------------------------------------------------------
//	VS solution
//-----------------------------------------------------------------------------

struct GUID_map { const GUID guid; const char *name; } guid_map[] = {
	{{0x04D02946,0x0DBE,0x48F9,{0x83,0x83,0x8B,0x75,0xA5,0xB7,0xBA,0x34}},	"Visual Studio Tools for Office Visual C# Add-in Project"							},
	{{0x059D6162,0xCD51,0x11D0,{0xAE,0x1F,0x00,0xA0,0xC9,0x0F,0xFF,0xC3}},	"Project Converter"																	},
	{{0x06A35CCD,0xC46D,0x44D5,{0x98,0x7B,0xCF,0x40,0xFF,0x87,0x22,0x67}},	"Visual Studio Deployment Merge Module Project"										},
	{{0x07CD18B1,0x3BA1,0x11d2,{0x89,0x0A,0x00,0x60,0x08,0x31,0x96,0xC6}},	"Macro Solution"																	},
	{{0x147FB6A7,0xF239,0x4523,{0xAE,0x65,0xB6,0xA4,0xE4,0x9B,0x36,0x1F}},	"Visual Studio Tools for Office Visual C# Add-in Project w/ Ribbon Support"			},
	{{0x14822709,0xB5A1,0x4724,{0x98,0xCA,0x57,0xA1,0x01,0xD1,0xB0,0x79}},	"Workflow Visual C# Project"														},
	{{0x1BD78563,0x6D94,0x4D25,{0x89,0xDE,0xAE,0xA0,0xC9,0x41,0xED,0x74}},	"Visual Studio Tools for Office VB SharePoint Project"								},
	{{0x1BD78563,0x6D94,0x4D25,{0x89,0xDE,0xAE,0xA0,0xC9,0x41,0xED,0x75}},	"Visual Studio Tools for Office C# SharePoint Project"								},
	{{0x1d6d3185,0xe7ba,0x44a5,{0x8d,0xbe,0x00,0x48,0x5e,0xf2,0x80,0x94}},	"WPF Project Flavor CSharp Templates"												},
	{{0x20D4826A,0xC6FA,0x45DB,{0x90,0xF4,0xC7,0x17,0x57,0x0B,0x9F,0x32}},	"Legacy Smart Device C# Project"													},
	{{0x2150E333,0x8FDC,0x42A3,{0x94,0x74,0x1A,0x39,0x56,0xD4,0x6D,0xE8}},	"Solution Folder Project"															},
	{{0x21547BD6,0x3592,0x4EAB,{0x86,0xC8,0x4A,0xCB,0x56,0x47,0x97,0xE4}},	"Visual Studio Tools for Office Visual C# Projects"									},
	{{0x23162FF1,0x3C3F,0x11d2,{0x89,0x0A,0x00,0x60,0x08,0x31,0x96,0xC6}},	"Macro Project"																		},
	{{0x2606E7C9,0x5071,0x4B63,{0x9A,0x83,0xC6,0x6A,0x32,0xB1,0x66,0x9F}},	"Visual Studio Tools for Office Visual Basic Add-in Project w/ Ribbon Support"		},
	{{0x2A80E3CB,0x96C2,0x4EF3,{0xB0,0x99,0xDB,0x33,0x16,0xC9,0x5E,0xB3}},	"Visual Studio Tools for Office Visual Basic Add-in Project"						},
	{{0x3114F5B0,0xE435,0x4bc5,{0xA0,0x3D,0x16,0x8E,0x20,0xD9,0xBF,0x83}},	"Smart Device Visual Basic Virtual Project for .Net Compact Framework v2.0"			},
	{{0x32F31D43,0x81CC,0x4C15,{0x9D,0xE6,0x3f,0xC5,0x45,0x35,0x62,0xB6}},	"WorkflowProjectFactory"															},
	{{0x349C5851,0x65DF,0x11DA,{0x93,0x84,0x00,0x06,0x5B,0x84,0x6F,0x21}},	"Web Application Project Factory"													},
	{{0x349C5853,0x65DF,0x11DA,{0x93,0x84,0x00,0x06,0x5B,0x84,0x6F,0x21}},	"C# Web Application Project Templates"												},
	{{0x349C5854,0x65DF,0x11DA,{0x93,0x84,0x00,0x06,0x5B,0x84,0x6F,0x21}},	"VB Web Application Project Templates"												},
	{{0x349c5855,0x65df,0x11da,{0x93,0x84,0x00,0x06,0x5b,0x84,0x6f,0x21}},	"J# Web Application Project Templates"												},
	{{0x39d444fd,0xb490,0x1554,{0x52,0x74,0x2d,0x61,0x2a,0x16,0x52,0x98}},	"CSharp Test Project"																},
	{{0x35F59093,0x63D9,0x4a89,{0xB6,0x99,0xB6,0x4F,0xAA,0xDA,0xEE,0xFB}},	"Smart Device Visual Basic Virtual Project for .Net Compact Framework v1.0"			},
	{{0x3AC096D0,0xA1C2,0xE12C,{0x13,0x90,0xA8,0x33,0x58,0x01,0xFD,0xAB}},	"Test Project"																		},
	{{0x3b0d21b2,0x944d,0x4169,{0xb2,0xd5,0xf8,0x59,0x48,0x27,0x65,0x07}},	"VDT Project Flavor CS Templates"													},
	{{0x3d9ad99f,0x2412,0x4246,{0xb9,0x0b,0x4e,0xaa,0x41,0xc6,0x46,0x99}},	"WcfProjectFactory"																	},
	{{0x3EA9E505,0x35AC,0x4774,{0xB4,0x92,0xAD,0x17,0x49,0xC4,0x94,0x3A}},	"Visual Studio Deployment Cab Project"												},
	{{0x4D628B5B,0x2FBC,0x4aa6,{0x8C,0x16,0x19,0x72,0x42,0xAE,0xB8,0x84}},	"Smart Device C# Project"															},
	{{0x4DC42137,0x97F6,0x45AB,{0x9B,0x1C,0x3F,0x16,0x2C,0x91,0x85,0x25}},	"Visual Studio Tools for Office Visual Basic Add-in Project"						},
	{{0x4F174C21,0x8C12,0x11D0,{0x83,0x40,0x00,0x00,0xF8,0x02,0x70,0xF8}},	"#6025"																				},
	{{0x4fd007e8,0x1a56,0x7e75,{0x70,0xca,0x04,0x66,0x48,0x4d,0x4f,0x98}},	"VisualBasic Test Project"															},
	{{0x51063C3A,0xE220,0x4D12,{0x89,0x22,0xBD,0xA9,0x15,0xAC,0xD7,0x83}},	"Visual Studio Tools for Office Visual C# Add-in Project w/ Ribbon Support"			},
	{{0x54435603,0xDBB4,0x11D2,{0x87,0x24,0x00,0xA0,0xC9,0xA8,0xB9,0x0C}},	"Visual Studio Deployment Project"													},
	{{0x58839851,0x977a,0x4c1b,{0xa0,0xd3,0x79,0x15,0x7d,0x5f,0x2b,0xbf}},	"WPF Project Flavor Visual Basic Templates"											},
	{{0x5D898164,0xAEB5,0x470F,{0x97,0xBA,0x92,0x53,0xF0,0x22,0xFD,0x71}},	"Visual Studio Tools for Office Visual Basic Add-in Project for Outlook"			},
	{{0x603c0e0b,0xdb56,0x11dc,{0xbe,0x95,0x00,0x0d,0x56,0x10,0x79,0xb0}},	"Web MVC Upgrade Project Factory"													},
	{{0x603c0e0c,0xdb56,0x11dc,{0xbe,0x95,0x00,0x0d,0x56,0x10,0x79,0xb0}},	"C# Web MVC Project Templates"														},
	{{0x603c0e0d,0xdb56,0x11dc,{0xbe,0x95,0x00,0x0d,0x56,0x10,0x79,0xb0}},	"VB Web MVC Project Templates"														},
	{{0x66355F20,0xA65B,0x11D0,{0xBF,0xB5,0x00,0xA0,0xC9,0x1E,0xBF,0xA0}},	"VJ6 Project System"																},
	{{0x66A26720,0x8FB5,0x11D2,{0xAA,0x7E,0x00,0xC0,0x4F,0x68,0x8D,0xDE}},	"Old Solution Folder"																},
	{{0x66E200F7,0x60E8,0x4616,{0x90,0xF5,0xE5,0x2B,0x2F,0x2F,0x82,0x98}},	"C# Silverlight Project Templates"													},
	{{0x66FE057A,0x6BD5,0x4A46,{0x80,0x60,0x3C,0x3E,0x59,0x65,0x74,0xA0}},	"Visual Studio Tools for Office Visual C# Add-in Project for Outlook"				},
	{{0x68B1623D,0x7FB9,0x47D8,{0x86,0x64,0x7E,0xCE,0xA3,0x29,0x7D,0x4F}},	"Smart Device Visual Basic Project"													},
	{{0x6E5EA054,0x14B1,0x4B94,{0xB5,0x72,0xEC,0x51,0x5A,0xE2,0x4E,0x91}},	"Visual Studio Tools for Office Visual Basic Add-in Project for Outlook"			},
	{{0x76B279E8,0x36ED,0x494E,{0xB1,0x45,0x53,0x44,0xF8,0xDE,0xFC,0xB6}},	"F# Silverlight Project Templates"													},
	{{0x7AC1401C,0xCFF2,0x419B,{0xB7,0x45,0xEA,0x8E,0xDD,0x12,0xEE,0x70}},	"Visual Studio Tools for Office Visual C# Add-in Project"							},
	{{0x7B4E6693,0x4209,0x4ACF,{0xA8,0xBA,0xD0,0x3A,0x89,0x01,0x33,0xA4}},	"Visual Studio Tools for Office Visual Basic Projects"								},
	{{0x7C153452,0xB4EB,0x470e,{0x9A,0xD4,0x14,0x19,0xB7,0x78,0x98,0x9C}},	"Xbox 360 Project"																	},
	{{0x7C3490A3,0x8632,0x43C5,{0x8A,0x60,0x07,0xDC,0x2F,0x45,0x08,0x70}},	"#118"																				},
	{{0x7D6034C3,0xAFB8,0x05CB,{0x2A,0x75,0xDA,0xA6,0x5E,0x89,0xBE,0x83}},	"Visucal C++ Test Project"															},
	{{0x8040559B,0x8C3E,0x411d,{0xBB,0x63,0x67,0x4D,0xC5,0x35,0x50,0xCD}},	"Smart Device C# Virtual Project for .Net Compact Framework v2.0"					},
	{{0x82c448b6,0x9d85,0x47d0,{0xa1,0xbc,0x01,0xc5,0xd8,0x80,0x2d,0x46}},	"VDT Project Flavor VB Templates"													},
	{{0x84F32C53,0x93DA,0x46fd,{0x99,0x3C,0x0C,0x18,0x75,0xCB,0xF4,0xE6}},	"Smart Device C# Virtual Project for .Net Compact Framework v1.0"					},
	{{0x8BC9CEB8,0x8B4A,0x11D0,{0x8D,0x11,0x00,0xA0,0xC9,0x1B,0xC9,0x42}},	"Visual C++ Project"																},
	{{0x8BC9CEB9,0x8B4A,0x11D0,{0x8D,0x11,0x00,0xA0,0xC9,0x1B,0xC9,0x42}},	"Exe Projects"																		},
	{{0x8BC9CEB9,0x9B4A,0x11D0,{0x8D,0x11,0x00,0xA0,0xC9,0x1B,0xC9,0x42}},	"Crash Dump Projects"																},
	{{0x8BC9CEBA,0x8B4A,0x11D0,{0x8D,0x11,0x00,0xA0,0xC9,0x1B,0xC9,0x42}},	"#10008"																			},
	{{0x911E67C6,0x3D85,0x4fce,{0xB5,0x60,0x20,0xA9,0xC3,0xE3,0xFF,0x48}},	"Exe Projects"																		},
	{{0x978C614F,0x708E,0x4E1A,{0xB2,0x01,0x56,0x59,0x25,0x72,0x5D,0xBA}},	"Visual Studio Deployment Setup Project"											},
	{{0x999D2CB9,0x9277,0x4465,{0xA9,0x02,0x16,0x04,0xED,0x36,0x86,0xA3}},	"Report Model Project"																},
	{{0xA1591282,0x1198,0x4647,{0xA2,0xB1,0x27,0xE5,0xFF,0x5F,0x6F,0x3B}},	"Microsoft Silverlight Project Factory"												},
	{{0xA2FE74E1,0xB743,0x11D0,{0xAE,0x1A,0x00,0xA0,0xC9,0x0F,0xFF,0xC3}},	"Miscellaneous Files Project"														},
	{{0xA58A78EB,0x1C92,0x4DDD,{0x80,0xCF,0xE8,0xBD,0x87,0x2A,0xBF,0xC4}},	"Visual Studio Tools for Office Visual C# Add-in Project for Outlook"				},
	{{0xA860303F,0x1F3F,0x4691,{0xB5,0x7E,0x52,0x9F,0xC1,0x01,0xA1,0x07}},	"Visual Studio Tools for Applications Project"										},
	{{0xAB322303,0x2255,0x48EF,{0xA4,0x96,0x59,0x04,0xEB,0x18,0xDA,0x55}},	"Visual Studio WinCE Deployment Project"											},
	{{0xB11A6AD6,0x2D76,0x4C13,{0xBD,0xD7,0x78,0xED,0x70,0x04,0xC3,0x64}},	"Xml Project"																		},
	{{0xB25BF3F7,0xF4D4,0x4247,{0x92,0xCC,0x4A,0x05,0xFD,0xE7,0x29,0xDB}},	"Visual Studio Tools for Office Visual Basic Projects"								},
	{{0xB900F1C2,0x3D47,0x4FEC,{0x85,0xB3,0x04,0xAA,0xF1,0x8C,0x36,0x34}},	"Visual Studio Smart Device Cab Project"											},
	{{0xBAA0C2D2,0x18E2,0x41B9,{0x85,0x2F,0xF4,0x13,0x02,0x0C,0xAA,0x33}},	"Visual Studio Tools for Office Project"											},
	{{0xBB1F664B,0x9266,0x4fd6,{0xB9,0x73,0xE1,0xE4,0x49,0x74,0xB5,0x11}},	"Visual Studio Tools for Office SharePoint Project"									},
	{{0xBDD4A1A1,0x7A1F,0x11D0,{0xAC,0x13,0x00,0xA0,0xC9,0x1E,0x29,0xD5}},	""																					},
	{{0xc252feb5,0xa946,0x4202,{0xb1,0xd4,0x99,0x16,0xa0,0x59,0x03,0x87}},	"VDT Project Flavor"																},
	{{0xCB3A5A90,0x6728,0x4E73,{0x81,0xD5,0x7F,0x71,0xC8,0xEA,0x4A,0x2F}},	"Visual Studio Tools for Office Visual C# Projects"									},
	{{0xCB4CE8C6,0x1BDB,0x4DC7,{0xA4,0xD3,0x65,0xA1,0x99,0x97,0x72,0xF8}},	"Legacy Smart Device Visual Basic Project"											},
	{{0xd183a3d8,0x5fd8,0x494b,{0xb0,0x14,0x37,0xf5,0x7b,0x35,0xe6,0x55}},	"Data Transformations Project"														},
	{{0xD1DCDB85,0xC5E8,0x11D2,{0xBF,0xCA,0x00,0xC0,0x4F,0x99,0x02,0x35}},	"Solution Items Project"															},
	{{0xD236E6CA,0xA73B,0x4ede,{0x86,0xF5,0x9B,0xC2,0x0A,0x84,0xEF,0x90}},	"VB Silverlight Project Templates"													},
	{{0xD24BA5B7,0x7536,0x40BB,{0xA6,0x37,0x53,0x37,0xAA,0x82,0x18,0x3D}},	"Visual Studio Deployment Tier Project"												},
	{{0xd2abab84,0xbf74,0x430a,{0xb6,0x9e,0x9d,0xc6,0xd4,0x0d,0xda,0x17}},	"Analysis Services Project"															},
	{{0xD59BE175,0x2ED0,0x4C54,{0xBE,0x3D,0xCD,0xAA,0x9F,0x32,0x14,0xC8}},	"Workflow Visual Basic Project"														},
	{{0xDCFE8D25,0x4715,0x4C33,{0x9E,0xAB,0xA3,0x4A,0x9E,0xBC,0x95,0x44}},	"Visual Studio Tools for Office Visual Basic Add-in Project w/ Ribbon Support"		},
	{{0xE24C65DC,0x7377,0x472b,{0x9A,0xBA,0xBC,0x80,0x3B,0x73,0xC6,0x1A}},	"#Web Project"																		},
	{{0xE502E3C0,0xBAAE,0x11D0,{0x88,0xBF,0x00,0xA0,0xC9,0x11,0x00,0x49}},	""																					},
	{{0xE6FDF86B,0xF3D1,0x11D4,{0x85,0x76,0x00,0x02,0xA5,0x16,0xEC,0xE8}},	"Visual J# Project"																	},
	{{0xf088123c,0x0e9e,0x452a,{0x89,0xe6,0x6b,0xa2,0xf2,0x1d,0x5c,0xac}},	"ModelingProjectFactory"															},
	{{0xF14B399A,0x7131,0x4c87,{0x9E,0x4B,0x11,0x86,0xC4,0x5E,0xF1,0x2D}},	"#5001"																				},
	{{0xF184B08F,0xC81C,0x45f6,{0xA5,0x7F,0x5A,0xBD,0x99,0x91,0xF2,0x8F}},	"Visual Basic Project"																},
	{{0xf2a71f9b,0x5d33,0x465a,{0xa7,0x02,0x92,0x0d,0x77,0x27,0x97,0x86}},	"FSharpProjectFactory"																},
	{{0xF85E285D,0xA4E0,0x4152,{0x93,0x32,0xAB,0x1D,0x72,0x4D,0x33,0x25}},	"Web MVC Project Factory"															},
	{{0xF85E285E,0xA4E0,0x4152,{0x93,0x32,0xAB,0x1D,0x72,0x4D,0x33,0x25}},	"C# Web MVC Project Templates"														},
	{{0xF85E285F,0xA4E0,0x4152,{0x93,0x32,0xAB,0x1D,0x72,0x4D,0x33,0x25}},	"VB Web MVC Project Templates"														},
	{{0xF8810EC1,0x6754,0x47fc,{0xA1,0x5F,0xDF,0xAB,0xD2,0xE3,0xFA,0x90}},	"Visual Studio Tools for Office SharePoint Workflow Project"						},
	{{0xFAE04EC0,0x301F,0x11d3,{0xBF,0x4B,0x00,0xC0,0x4F,0x79,0xEF,0xBC}},	"Visual C# Project"																	},
	{{0xFB4431D6,0x6095,0x4d6f,{0x8D,0x01,0x61,0x35,0xC9,0x78,0xF7,0x00}},	"Enterprise Template Project"														},
	{{0xFE3BBBB6,0x72D5,0x11d2,{0x9A,0xCE,0x00,0xC0,0x4F,0x79,0xA2,0xA4}},	"Design Time Policy Project Aggregator"												},
};

class VSsolutionFileHandler : public FileHandler {
	struct Project {
		GUID				guid;
		GUID				type;
		string				name;
		string				filename;
		Project				*parent;
		ISO_ptr<anything>	p;
		dynamic_array<pair<string,string> > configs;

		Project() : parent(0)	{}
		bool operator==(const GUID &g) const { return guid == g; }
	};

	const char*		GetExt() override { return "sln"; }
	const char*		GetDescription() override { return "Visual Studio Solution";	}
	int				Check(istream_ref file) override {
		file.seek(0);
		text_mode_reader<istream_ref>	text(file);
		fixed_string<1024>	s;
		while (text.read_line(s) && s.blank());
		return s.begins("Microsoft Visual Studio Solution File") ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}

	static count_string	get_value(string_scan &ss) {
		if (ss.skip_whitespace().peekc() == '"') {
			ss.move(1);
			count_string v = ss.get_token(~char_set('"'));
			ss.move(1);
			return v;
		}
		return ss.get_token(~char_set::whitespace - char_set("(),"));
	}

	ISO_ptr<void>	Read(tag id, istream_ref file) override;
//	bool			Write(ISO_ptr<void> p, ostream_ref file) override;
} vs_sln;

class solution_reader : public text_mode_reader<istream_ref> {
public:
	void expect(string_scan &ss, char c) {
		if (ss.skip_whitespace().getc() != c)
			throw_accum("missing '" << c << "' at line " << line_number);
	}
	solution_reader(istream_ref file) : text_mode_reader<istream_ref>(file) {}
};

ISO_ptr<void> VSsolutionFileHandler::Read(tag id, istream_ref file) {
	string	s;
	solution_reader	r(file);
	while (r.read_line(s) && !s);
	if (!s.begins("Microsoft Visual Studio Solution File"))
		return ISO_NULL;

	string_scan			ss(s);

	float	version = 0;
	if (ss.scan_skip("Format Version"))
		ss >> version;

	enum STATE{
		ROOT,
		GLOBALS,
		GLOBAL_SECTION,
		GLOBAL_NESTEDPROJECTS,
		GLOBAL_PROJECTCONFIGS,
	};

	STATE	state = ROOT;

	ISO_ptr<anything>	a(id);
	ISO_ptr<anything>	projsect("projects");
	a->Append(projsect);

	dynamic_array<Project>	projects;
	ISO_ptr<anything>		section;

	while (r.read_line(s)) {
		string_scan			ss(s);
		switch (state) {
			case ROOT: {
				count_string	tok	= get_value(ss);
				if (tok == "Project") {
					Project	*proj	= new (projects) Project;

					r.expect(ss, '(');
					string_scan(get_value(ss)) >> proj->type;
					r.expect(ss, ')');
					r.expect(ss, '=');

					proj->name = get_value(ss);
					r.expect(ss, ',');
					proj->filename = get_value(ss);
					r.expect(ss, ',');
					string_scan(get_value(ss)) >> proj->guid;

				} else if (tok == "Global") {
					state = GLOBALS;
				}
				break;
			}

			case GLOBALS: {
				count_string	tok	= get_value(ss);
				if (tok == "GlobalSection") {
					r.expect(ss, '(');
					count_string	sect = get_value(ss);
					r.expect(ss, ')');
					r.expect(ss, '=');
					count_string	val = get_value(ss);	// preSolution or postSolution

					if (sect == "NestedProjects") {
						state = GLOBAL_NESTEDPROJECTS;

					} else if (sect == "ProjectConfigurationPlatforms") {
						state = GLOBAL_PROJECTCONFIGS;

					} else {
						section.Create(sect);
						a->Append(section);
						state	= GLOBAL_SECTION;
					}
				} else if (tok == "EndGlobal") {
					state	= ROOT;
				}
				break;
			}

			default: {
				const char *s	=	ss.skip_whitespace().getp();
				if (str(s) == "EndGlobalSection") {
					state = GLOBALS;

				} else if (ss.scan('=')) {
					const char *eq = ss.getp();
					while (is_whitespace(*--eq));
					count_string	tok = str(s, eq + 1);
					ss.move(1).skip_whitespace();

					if (state == GLOBAL_NESTEDPROJECTS) {
						if (Project *pproj = iso::find(projects, string_scan(tok).get<GUID>()))
							pproj->parent = iso::find(projects, ss.get<GUID>());

					} else if (state == GLOBAL_PROJECTCONFIGS) {
						string_scan	ss2(tok);
						if (Project *pproj = iso::find(projects, ss2.get<GUID>()))
							pproj->configs.push_back(make_pair(ss2.move(1).remainder(), ss.getp()));

					} else {
						section->Append(ISO_ptr<string>(tok, ss.getp()));
					}
				} else {
					throw("missing '=' at line %i", r.line_number);
				}
				break;
			}
		}
	}

	for (auto &i : projects) {
		buffer_accum<256>	ba;
		ba << i.guid;

		i.p.Create(i.name);
		i.p->Append(ISO_ptr<string>("guid", ba));

		const char *type = 0;
		for (int j = 0; j < num_elements(guid_map); j++) {
			if (guid_map[j].guid == i.type)
				type = guid_map[j].name;
		}

		if (type) {
			i.p->Append(ISO_ptr<string>("type", type));
		} else {
			ba.reset() << i.type;
			i.p->Append(ISO_ptr<string>("type", id));
		}
		if (i.configs) {
			ISO_ptr<anything>	configs("configs");
			i.p->Append(configs);
			for (auto &ci : i.configs)
				configs->Append(ISO_ptr<string>(ci.a, ci.b));
		}

		if (type != str("Solution Folder Project"))
			i.p->Append(ISO_ptr<void>().CreateExternal(FindAbsolute(i.filename), "project"));
	}
	for (auto &i : projects)
		(i.parent ? i.parent->p : projsect)->Append(i.p);

	return a;
}

//-----------------------------------------------------------------------------
//	VS project
//-----------------------------------------------------------------------------

struct VSProject : anything {
	ISO_ptr<void>	find(tag2 task, const char *include) {
		for (auto &i : *this) {
			if (i.IsID("ItemGroup")) {
				for (auto &j : *(anything*)i) {
					if (j.IsID(task)) {
						ISO_ptr<string> p = (*(anything*)j)["Include"];
						if (p && (*p) == include)
							return j;
					}
				}
			}
		}
		return ISO_NULL;
	}
};

ISO_DEFUSER(VSProject, anything);

class VSprojectFileHandler : public FileHandler {
	const char*		GetExt() override { return "vcxproj"; }
	const char*		GetDescription() override { return "Visual Studio Project";	}
	int				Check(istream_ref file) override {
		file.seek(0);
		XMLreader	xml(file);
		XMLreader::Data	data;
		return xml.CheckVersion()	>= 0
			&& xml.ReadNext(data)	== XMLreader::TAG_BEGIN
			&& data.Is("Project")
			&& data.Find("xmlns")	== "http://schemas.microsoft.com/developer/msbuild/2003"
			? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}

	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		ISO_ptr<anything>	p = Get("xml")->Read(none, file);
		p = (*p)["Project"];
		p.SetID(id);
		p.Header()->type = ISO::getdef<VSProject>();
		return p;
	}

	void	FixImport(anything &import, const filename &fn) {
		if (ISO_ptr<void> &proj = import["Project"]) {
			if (proj.IsType<string>() && !((string*)proj)->begins("$"))
				proj = ISO::MakePtrExternal<void>(fn.relative(*(string*)proj), "Project");
		}
	}

	ISO_ptr<void>	ReadWithFilename(tag id, const filename &fn) override {
		ISO_ptr<VSProject>	p1 = Read(id, FileInput(fn).me());

		if (ISO_ptr<anything> p2 = Get("xml")->Read(none, FileInput(filename(fn).add_ext("filters")).me())) {
			if ((p2 = (*p2)["Project"])) {
				ISO_ptr<anything>	filters("ItemGroup");
				for (auto &i : *p2) {
					if (i.IsID("ItemGroup")) {
						for (ISO_ptr<anything> j : *(anything*)i) {
							ISO_ptr<string>		include	= (*j)["Include"];
							if (ISO_ptr<anything>	filter	= (*j)["Filter"]) {
								if (ISO_ptr<anything> d = p1->find(j.ID(), *include)) {
									ISO_ptr<string>	filter2 = (*filter)[0];
									filter2.SetID("Filter");
									d->Append(filter2);
								}
							} else {
								filters->Append(j);
							}
						}
					}
				}
			}
		}

		for (auto &i : *p1) {
			if (i.IsID("ItemGroup")) {
				for (auto &j : *(anything*)i) {
					if (!j.IsType<anything>() || j.IsID("ProjectConfiguration") || j.IsID("Filter"))
						continue;
					if (ISO_ptr<string> include = (*(anything*)j)["Include"])
						(*include) = fn.relative(*include);
				}
			} else if (i.IsID("ImportGroup")) {
				for (auto &j : *(anything*)i) {
					if (j.IsID("Import"))
						FixImport(*(anything*)j, fn);
				}
			} else if (i.IsID("Import")) {
				FixImport(*(anything*)i, fn);
			}
		}
		/*
		ISO::Browser	b	= ISO::Browser(p1);
		for (ISO::Browser::iterator i = b.begin(), e = b.end(); i != e; ++i) {
			if (i.GetName() == "ItemGroup") {
				for (ISO::Browser::iterator i2 = i->begin(), e2 = i->end(); i2 != e2; ++i2) {
					tag2	id	= i2.GetName();
					if (id == "ProjectConfiguration" || id == "Filter")
						continue;
					if (ISO::Browser b = (*i2)["Include"])
						b.Set((const char*)fn.relative(b.GetString()));
				}
			}
		}
		*/
		p1->Append(ISO_ptr<string>("ProjectPath", fn));

		return p1;
	}

	template<typename T> static ISO_ptr<anything>	MakeValue(const char *name, const T &value) {
		ISO_ptr<anything>	p(name);
		p->Append(ISO_ptr<string>(tag(), str(buffer_accum<256>() << value)));
		return p;
	}
	static ISO_ptr<anything>	MakeProject() {
		ISO_ptr<anything>	project("Project");
		project->Append(ISO::MakePtr("DefaultTargets", str("Build")));
		project->Append(ISO::MakePtr("ToolsVersion", str("4.0")));
		project->Append(ISO::MakePtr("xmlns", str("http://schemas.microsoft.com/developer/msbuild/2003")));
		return project;
	}

	static ISO_ptr<anything>	MakeConfiguration(const char *config, const char *plat) {
		ISO_ptr<anything>	pc("ProjectConfiguration");
		pc->Append(ISO::MakePtr("Include", string(format_string("%s|%s", config, plat))));
		pc->Append(MakeValue("Configuration",	config));
		pc->Append(MakeValue("Platform",		plat));
		return pc;
	}

	static ISO_ptr<anything>	MakeImport(const char *import) {
		ISO_ptr<anything>	p("Import");
		p->Append(ISO::MakePtr("Project", import));
		return p;
	}

	bool Addfiles(const filename &dir, const ISO::Browser2 &srce, anything &proj, anything &filtered, anything &filters) {
		string	fname(dir);
		bool	any = false;

		for (auto i = srce.begin(), e = srce.end(); i != e; ++i) {
			tag			id = i->GetName();
			filename	fn	= filename(dir).add_dir(id);
			auto		ext	= fn.ext();

			if (i->Is("Directory")) {
				any |= Addfiles(fn, *i, proj, filtered, filters);
				continue;
			}

			if (!ext) {
				if (ISO_ptr<void> p2 = ISO_conversion::convert<anything>(*i)) {
					any |= Addfiles(fn, p2, proj, filtered, filters);
					continue;
				}
			}

			const char *type
				=	ext == ".cpp" || ext == ".c" || ext == ".cc" || ext == ".cxx" ? "ClCompile"
				:	ext == ".hpp" || ext == ".h" || ext == ".inl" || ext == ".inc" || ext == ".tli" || ext == ".tlh" || ext == ".hh" ? "ClInclude"
				:	0;

			if (type) {
				ISO_ptr<anything>	entry1(type);
				entry1->Append(ISO::MakePtr("Include", string(fn)));
				proj.Append(entry1);

				ISO_ptr<anything>	entry2(type);
				entry2->Append(ISO::MakePtr("Include", string(fn)));
				if (fname)
					entry2->Append(MakeValue("Filter", fname));
				filtered.Append(entry2);

				any = true;
			}
		}

		if (any && fname) {
			ISO_ptr<anything>	filter("Filter");
			filter->Append(ISO::MakePtr("Include", fname));

			GUID guid;
			if (SUCCEEDED(CoCreateGuid(&guid)))
				filter->Append(MakeValue("UniqueIdentifier", guid));

			filters.Append(filter);
		}
		return any;
	}

	bool WriteWithFilename(ISO_ptr<void> p, const filename &fn) override {
		if (ISO_ptr<void> p2 = ISO_conversion::convert<anything>(p)) {
			p = p2;
		} else if (!p.IsType("Directory")) {
			return false;
		}

		ISO_ptr<anything>	vcxproj(0);
		ISO_ptr<anything>	filters(0);

		ISO_ptr<anything>	project0 = MakeProject();
		vcxproj->Append(project0);

		ISO_ptr<anything>	project1 = MakeProject();
		filters->Append(project1);

		ISO_ptr<anything>	configs("ItemGroup");
		configs->Append(ISO::MakePtr("Label", str("ProjectConfigurations")));
		configs->Append(MakeConfiguration("Debug", "Win32"));
		configs->Append(MakeConfiguration("Release", "Win32"));
		configs->Append(MakeConfiguration("Debug", "x64"));
		configs->Append(MakeConfiguration("Release", "x64"));
		project0->Append(configs);

		ISO_ptr<anything>	propgroup("PropertyGroup");
		propgroup->Append(ISO::MakePtr("Label", str("Globals")));
		propgroup->Append(MakeValue("ProjectName", p.ID().get_tag()));
		propgroup->Append(MakeValue("Keyword", "Win32Proj"));
		project0->Append(propgroup);

		project0->Append(MakeImport("$(VCTargetsPath)\\Microsoft.Cpp.Default.props"));
		project0->Append(MakeImport("$(VCTargetsPath)\\Microsoft.Cpp.props"));

		ISO_ptr<anything>	propgroup2("PropertyGroup");
		propgroup2->Append(ISO::MakePtr("Label", str("Configuration")));
		propgroup2->Append(MakeValue("ConfigurationType", "Application"));
		project0->Append(propgroup2);

		ISO_ptr<anything>	filtergroup("ItemGroup");
		project1->Append(filtergroup);

		ISO_ptr<anything>	itemgroup0("ItemGroup");
		ISO_ptr<anything>	itemgroup1("ItemGroup");
		project0->Append(itemgroup0);
		project1->Append(itemgroup1);

		Addfiles("", p, *itemgroup0, *itemgroup1, *filtergroup);

		project0->Append(MakeImport("$(VCTargetsPath)\\Microsoft.Cpp.targets"));

		return	Get("xml")->WriteWithFilename(vcxproj, fn)
		&&		Get("xml")->WriteWithFilename(filters, filename(fn).add_ext("filters"));
	}
} vs_proj;

//-----------------------------------------------------------------------------
//	VSGLOG
//-----------------------------------------------------------------------------

#include "comms/zip.h"
#include "container/compound_doc.h"


class VSGLOGprojectFileHandler : public FileHandler {
	const char*		GetExt() override { return "vsglog"; }
	const char*		GetDescription() override { return "Visual Studio Graphics Session";	}

	struct Header {
		enum { MAGIC = 'GFXA'};
		uint32be	magic;
		uint32		unk1;	//0x1f
		uint32		unk2;	//0xa
		uint32		unk3;	//0x7a295a (uncomp size?)
		bool	valid() const {
			return magic == MAGIC;
		}
	};
	struct Block {
		char	type[8];
		uint32	unk1;	//0x100000 (version?)
		uint32	len;
	};
	struct CMPRBLK1 {
		uint64	unk1;	//0A 51 E5 C0 18 00 A4 03
		uint64	unk2;	//0x100000
		uint64	unk3;	//0x100000
	};

public:
	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		Header	h;
		Block	b;
		if (!file.read(h) || !h.valid())
			return ISO_NULL;

		ISO_ptr<anything>	p(id);
		stream_skip(file, 0x1000 - sizeof(h));

		while (file.read(b) && b.type[0]) {
			char	name[9];
			memcpy(name, b.type, 8);
			name[8] = 0;
			ISO_ptr<ISO_openarray<uint8> >	i(name, b.len);
			file.readbuff(*i, b.len);
			p->Append(i);
		}
		return p;
	}

} vsglog;

struct chunk_header {
	uint32	unknown0;
	uint32	raw_size;
	uint32	unknown1;
	uint32	compressed_size;
	const_memory_block	data()	const	{ return const_memory_block(this + 1, compressed_size); }
	const chunk_header	*next() const	{ return (const chunk_header*)((char*)(this + 1) + compressed_size); }
};
struct file_header {
	uint32			raw_size, compressed_size;
	chunk_header	chunk[0];
	auto	chunks(arbitrary_ptr end) const {
		return make_next_range(chunk, (const chunk_header*)end);
	}
};

class DiagSessionprojectFileHandler : public FileHandler {
	const char*		GetExt() override { return "diagsession"; }
	const char*		GetDescription() override { return "Visual Studio Graphics Session";	}

	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		ZIPreader	zip(file);
		ZIPfile		zf;
		if (zip.Next(zf))
			return vsglog.Read(id, zf.Reader(file));

		file.seek(0);
		CompDocHeader	header = file.get();
		if (!header.valid())
			return ISO_NULL;

		CompDocReader	reader(file, header);

		auto		root	= reader.find("Root Entry");
		auto		meta	= reader.find("metadata.xml", root->root);
		auto		data	= reader.read(file, meta);
		memory_reader	file2(data);
		XMLreader	xml(file2);
		XMLreader::Data	d;

//    <Resource CompressionOption="norm" Id="{B605F219-C76C-43B0-AC93-E3A4E37C3E35}" IsDirectoryOnDisk="false" Name="sc.user_aux.etl" ResourcePackageUriPrefix="B605F219-C76C-43B0-AC93-E3A4E37C3E35" TimeAddedUTC="131935199847269229" Type="DiagnosticsHub.Resource.EtlFile" />
		ISO_ptr<anything>		p(id);
		int			i		= 0;
		for (XMLiterator it(xml, d); it.Next();) {
			if (d.Is("Package"))
				it.Enter();
			if (d.Is("Content"))
				it.Enter();
			if (d.Is("Resource")) {
				string	CompressionOption			= d.Find("CompressionOption");
				GUID	Id							= d["Id"];
				bool	IsDirectoryOnDisk			= d["IsDirectoryOnDisk"];
				string	Name						= d["Name"];
				string	ResourcePackageUriPrefix	= d["ResourcePackageUriPrefix "];
				int64	TimeAddedUTC				= d["TimeAddedUTC"];
				string	Type						= d["Type"];

				if (auto entry	= reader.find(format_string("E%i", i + 1), root->root)) {
#if 0
					auto	block	= reader.read(file, entry);
					file_header	*fh	= block;
					uint32	total	= 0;
					for (auto &c : fh->chunks(block.end()))
						total += c.compressed_size;

					ISO_ptr<ISO_openarray<uint8> >	f(Name, total);
					uint8	*dest = f->begin();
					for (auto &c : fh->chunks(block.end())) {
						c.data().copy_to(dest);
						dest += c.compressed_size;
					}
#else
					ISO_ptr<ISO_openarray<uint8> >	f(Name, entry->size);
					reader.read(file, entry, *f);
#endif
					p->Append(f);
				}
				++i;

			}
		}

		return p;
	}

} diagsession;
