#include "iso/iso_files.h"

using namespace iso;

class IFOFileHandler : public FileHandler {
	const char*		GetExt() override { return "ifo";	}
	ISO_ptr<void>	Read(tag id, istream_ref file) override;
	bool			Write(ISO_ptr<void> p, ostream_ref file) override {
		return false;
	}
} ifo;

#pragma pack(1)

//Video Manager VMG tables (contents of VIDEO_TS.IFO)
//VMGM_MAT			Video Manager Information Management		Required
//First-Play PGC	First-Play Program Chain (PGC)				Required
//VMG_TT_SRPT		Video Manager Title Map Search				Required
//VMGM_PGCI_UT		Video Manager Menu Program Chain Units		Optional
//LU Menu			Language Units								Optional
//VMG_PTL_MAIT		Parental Management Information				Optional
//VMG_VTS_ATRT		Video Title Set Attributes					Required
//VMG_TXTDT_MG		Text Data Manager Information				Optional
//VMGM_C_ADT		Video Manager Menu Cell Address				Required
//VMGM_VOBU_ADMAP	Video Manager Menu VOB Unit Address			Required

//12.1. Video Manager Information Management (VMGM_MAT)

struct VMGM_MAT : bigendian_types {
	char				vmid[12];						//Video Manager Identifier "DVDVIDEO_VMG"
	uint32				vmg_lastsector;					//Last sector of VMG Total size of VIDEO_TS.IFO+VIDEO_TS.VOB+VIDEO_TS.BUP
	uint8				padding1[12];
	uint32				vmgi_lastsector;				//Last sector of VMGI Total size of VIDEO_TS.IFO
	uint16				dvd_spec_version;				//DVD Specifications version V1.0
	uint32				vmg_category;					//VMG Category and Mask for Regional Codes
	uint16				num_volumes;					//Number of volumes of this DVD 1
	uint16				cur_volume;						//Current volume 1
	uint8				disc_side;						//Disc side 1
	uint8				padding2[19];
	uint16				num_titlesets;					//Number of video title sets 1
	char				provider_id[32];				//Provider ID
	uint64				vmg_poscode;					//VMG Pos Code
	uint8				padding3[24];
	uint32				vmgimat_end;					//End byte address of VMGI_MAT
	uint32				firstplay;						//First-Play PGC Start Byte 1024
	uint8				padding4[56];
	uint32				vmgm_vobs_sector;				//Start sector of VMGM_VOBS 0
	uint32				vmg_ptt_srpt_sector;			//Start sector of VMG_PTT_SRPT
	uint32				vmgm_pgci_ut_sector;			//Start sector of VMGM_PGCI_UT
	uint32				vmg_ptl_mait_sector;			//Start sector of VMG_PTL_MAIT 0
	uint32				vmg_vts_atrt_sector;			//208 4 Start sector of VMG_VTS_ATRT
	uint32				vmg_txtdt_mg_sector;			//212 4 Start sector of VMG_TXTDT_MG 0
	uint32				vmgm_c_adt_sector;				//216 4 Start sector of VMGM_C_ADT 0
	uint32				vmgm_vobu_admap_sector;			//220 4 Start sector of VMGM_VOBU_ADMAP 0
	uint32				vts_c_adt_sector;
	uint32				vts_vobu_admap_sector;
	uint8				padding5[24];
};
struct VideoAttribs {
	uint8	bytes[2];
};
struct AudioAttribs {
	uint8	bytes[8];
};
struct SubPictureAttribs {
	uint8	bytes[6];
};
struct VOBAttribs {
	VideoAttribs		vmgm_vobs_video_attribs;		//256 2 Video attributes of VMGM_VOBS
	uint8				padding6;						//258 1 Padding byte
	uint8				vmgm_vobs_num_audio_streams;	//259 1 Number of audio streams in VMGM_VOBS 0
	AudioAttribs		vmgm_audio_attribs[8];			//260 8 VMGM Audio 1-8 attributes
	uint8				padding7[17];					//324 17 Padding bytes
	uint8				vmgm_vobs_num_subpic_streams;	//341 1 Number of subpicture streams in VMGM_VOBS 0
	SubPictureAttribs	vmgm_subpic_attribs[32];		//342 1 VMGM Subpicture 1-32 attributes
};

//12.2. Video Manager Title Map Search (VMG_TT_SRPT)
struct TitleDescription : bigendian_types {
	uint8				playback_type;					//8 1 Playback type
	uint8				num_angles;						//9 1 Number of video angles 1
	uint16				num_chapters;					//10 2 Number of chapters 1
	uint8				parental_mask_vmg;				//12 1 Parental management mask for VMG 0
	uint8				parental_mask_vts;				//13 1 Parental management mask for VTS 0
	uint8				title_set;						//14 1 Video title set having this title entry 1
	uint8				title_num;						//15 1 Title number within its video title set (VTS) 1
	uint32				start_sector;					//16 4 Video title set starting sector address
};

struct VMG_TT_SRPT : bigendian_types {
	uint16				num_titles;						//0 2 Number of titles 0
	uint8				padding[2];						//2 2 Padding bytes
	uint32				table_len;						//4 4 Address of last byte (length of table-1)
	TitleDescription	title_descriptions[1];			//One or more 12-byte title descriptions following for each title defined above
};

//12.3. Video Manager Menu Program Chain Units (VMGM_PGCI_UT)
struct LanguageUnits : bigendian_types {
	uint8				code[2];						//8 2 Two-character language code (ISO 639-1)
	uint8				code_ext;						//10 1 Language code extension (ISO 639-2)
	uint8				menu;							//11 1 Menu existence flag
	uint32				start;							//12 4 Language unit start byte (points to a LU)
	uint16				num_menus;						//16 2 Number of menus in this language unit
	uint8				padding[2];						//18 2 Padding bytes
	uint32				table_len;						//20 4 Address of last byte of this language unit
};
struct VMGM_PGCI_UT : bigendian_types {
	uint16				num_lang_units;					//0 2 Number of language units 0
	uint8				padding[2];						//2 2 Padding bytes
	uint32				table_len;						//4 4 Address of last byte (length of table-1)
	LanguageUnits		language_units[1];				//One or more language units following for each language
};

//12.3.1 Menu Language Unit (LU)
struct LU : bigendian_types {
	uint8				menu_type;						//0 1 Menu type
	uint8				padding;						//1 1 Padding byte
	uint8				parental_mask_vmg;				//2 1 Parental ID mask for VMG
	uint8				parental_mask_vts;				//3 1 Parental ID mask for VTS
	uint32				start;							//4 4 PGC unit start byte (points to a menu PGC)
};

//12.4. Video Manager Parental Management Information (VMGM_PTL_MAIT)
struct CountryCode : bigendian_types {
	uint16				country_code;					//8 2 Country code
	uint8				padding[2];						//10 2 Padding bytes
	uint32				start_parental;					//12 4 Start byte of parental management table
};

struct VMG_PTL_MAIT : bigendian_types {
	uint16				num_countries;					//0 2 Number of countries
	uint16				num_titlesets;					//2 2 Number of title sets to manage (NTS)
	uint32				table_len;						//4 4 Address of last byte (length of table-1)
	CountryCode			country_codes[1];
};


//12.5. Parental Management Table (PTL_MAIT)
/*
0 2*(NTS+1) 16-bit masks (VMG + all title sets) for level 8 8000
2*(NTS+1) 2*(NTS+1) 16-bit masks (VMG + all title sets) for level 7 4000
4*(NTS+1) 2*(NTS+1) 16-bit masks (VMG + all title sets) for level 6 2000
6*(NTS+1) 2*(NTS+1) 16-bit masks (VMG + all title sets) for level 5 1000
8*(NTS+1) 2*(NTS+1) 16-bit masks (VMG + all title sets) for level 4 0800
10*(NTS+1) 2*(NTS+1) 16-bit masks (VMG + all title sets) for level 3 0400
12*(NTS+1) 2*(NTS+1) 16-bit masks (VMG + all title sets) for level 2 0200
14*(NTS+1) 2*(NTS+1) 16-bit masks (VMG + all title sets) for level 1 0100
*/

//First-Play Program Chain (PGC)
struct PGC {
	uint16	reserved;			//0 2 Reserved
	uint8	num_programs;		//2 1 Number of programs in this PGC 0
	uint8	num_cells;			//3 1 Number of cells in this PGC 0
	uint8	dur_hours;			//4 1 Playback time (hour) 0
	uint8	dur_minutes;		//5 1 Playback time (minute) 0
	uint8	dur_seconds;		//6 1 Playback time (second) 0
	uint8	dur_frames;			//7 1 Playback time (frame) 192 (0/30 fps)
	uint32	prohibited;			//8 4 Prohibited operations 0
	uint16	audio_status[8];	//12 2 Audio stream 1-8 status 0
	uint32	subpic_status[32];	//28 4 Sub-picture stream 1-32 status 0
	uint16	next_pgc;			//156 2 Next PGC number 0
	uint16	prev_pgc;			//158 2 Previous PGC number 0
	uint16	goup_pgc;			//160 2 GoUp PGC number 0
	uint8	still_seconds;		//162 1 Still time in seconds 0
	uint8	playback_mode;		//163 1 Program playback mode 0, sequential
	uint32	colour[16];			//164 4 Color 0-15 (0, Y, Cr, Cb) 0
	uint16	commands;			//228 2 Start byte of PGC command table 236
	uint16	program_map;		//230 2 Start byte of PGC program map 0
	uint16	cell_playback;		//232 2 Start byte of PGC cell playback information 0
	uint16	cell_position;		//234 2 Start byte of PGC cell position information 0
};
//PGC command table
struct PGC_commands {
	uint16	num_pre_commands;
	uint16	num_post_commands;
	uint16	num_cell_commands;
	uint16	end_commands;
};

//PGC cell playback
struct Cell_Playback {
	uint8	type;				//256 1 Cell type 2
	uint8	restricted;			//257 1 Restricted flag 0
	uint8	still_seconds;		//258 1 Cell still time in seconds, 255 = infinite 0
	uint8	end_command;		//259 1 Cell command number to execute at end 0
	uint8	dur_hours;			//260 1 Cell playback time (hour) 0
	uint8	dur_minutes;		//261 1 Cell playback time (minute) 0
	uint8	dur_seconds;		//262 1 Cell playback time (second) 6
	uint8	dur_frames;			//263 1 Cell playback time (frame) 25/30 fps
	uint32	entry_sector;		//264 4 Entry point sector address 0
	uint32	end_sector;			//268 4 First interleaving VOBU end sector address 0
	uint32	start_last;			//272 4 Start sector address of last VOBU 1639
	uint32	last_sector;		//276 4 Last sector address 1893
};

//PGC cell position
struct Cell_Position {
	uint16	VOB_id;
	uint8	padding;
	uint8	cell_id;
};

#pragma pack()

ISO_ptr<void> IFOFileHandler::Read(tag id, istream_ref file) {
//	VMGM_MAT	vmgmmat = file.get();
	return ISO_NULL;
}
