#include "iso/iso_files.h"
#include "hashes/crc.h"
#include "archive_help.h"

using namespace iso;

struct DataFileFixup {
	uint32	m_SourceOffset;		// The offset into the file where the fixup should be applied
	uint32	m_TargetOffset;		// The offset into the file where the fixup is pointing to
};

struct DataBlockHeader {
	uint32	m_NameHash;			// The hash of the name string
	uint32	m_Offset;			// The offset to the start of the data block within the data file
	uint32	m_Size;				// The size of the data block in bytes
};

struct DataFile {
	enum {
		kDataFileId			= 0x44415431,
		kDataFileIdReversed	= 0x31544144,
	};

	uint32	m_DataFileId;		// A key value that identifies this as a data file
	uint32	m_VersionNumber;	// A version number for the contents of the file
	uint32	m_FileSize;			// The total size of the data file
	uint16	m_BlockCount;		// The number of buffers in the file
	uint16	m_FixupCount;		// The number of data fixups in the file
	DataBlockHeader	blocks[];

	inline const DataBlockHeader*	GetDataBlockHeaderArray()	const { return blocks; }
	inline const DataFileFixup*		GetDataFileFixupArray()		const { return (const DataFileFixup*)(blocks + m_BlockCount); }

	bool	Fix() {
		if (m_DataFileId != kDataFileId && m_DataFileId != kDataFileIdReversed)
			return false;

		// Do endian flip if necessary
		bool	swap = m_DataFileId == kDataFileIdReversed;
		if (swap) {
			swap_endian_inplace(m_DataFileId, m_VersionNumber, m_FileSize, m_BlockCount, m_FixupCount);

			DataBlockHeader	*block_header = blocks;
			for (int i = m_BlockCount; i--; block_header++) {
				swap_endian_inplace(block_header->m_Offset, block_header->m_Size, block_header->m_NameHash);
			}

			DataFileFixup* fixup = (DataFileFixup*)block_header;
			for (int i = m_FixupCount; --i; fixup++) {
				swap_endian_inplace(fixup->m_SourceOffset, fixup->m_TargetOffset);
			}
		}

		const DataFileFixup	*fixup	= GetDataFileFixupArray();
		uint8				*base	= (uint8*)this;
		for (int i = m_FixupCount; i--; fixup++)
			*(uint64*)(base + fixup->m_SourceOffset) = uint64(base + fixup->m_TargetOffset);

		return true;
	}
};

#define	CRC32_poly	0xEDB88320
//namespace iso {
//template<> const uint32 *get_crc_table<uint32, CRC32_poly, true>();
//}

uint32 StringCrc32(const char* data) {
	return data && data[0] ? CRC_def<uint32, CRC32_poly, true, false>::calc(data, CRC32_poly) : 0;
}

enum TextureFormatFlags {
	kIsSrgbToLinear		= 1 << 0,
	kIsSignedExpand		= 1 << 1,
	kIsSwizzled			= 1 << 2,
	kIs1D				= 1 << 3,
	kIs2D				= 1 << 4,
	kIs3D				= 1 << 5,
	kIsCube				= 1 << 6,
	kIsColor			= 1 << 7,
	kIsNormal			= 1 << 8,
	kIsFillLight		= 1 << 10,
	kIgnoreMismatch		= 1 << 11,	// If set, this should allow textures to be loaded that don't match the format of their default texture
	kNoMipStream		= 1 << 12,	// If set, this texture does not use texture streaming
	kUnused				= 1 << 13,
	kHasFormatOverride	= 1 << 14
};

struct TextureHeaderBuiltIndependent {
	enum TextureAddressMode {
		kWrapHash			= 0xDD1EF27BU, // "kWrap"
		kMirrorHash			= 0x3743221AU, // "kMirror"
		kClampHash			= 0x4DC5BA88U, // "kClamp"
	};
	enum TextureFormat {
		kA8R8G8B8Hash		= 0x50692430U, // "kA8R8G8B8"
		kR5G6B5Hash			= 0x40D413CDU, // "kR5G6B5"
		kA1R5G5B5Hash		= 0x4B084ABCU, // "kA1R5G5B5"
		kR8Hash				= 0x095415B2U, // "kR8"
		kR8G8Hash			= 0x2C1440D9U, // "kR8G8"
		kR16Hash			= 0x3B1EA865U, // "kR16"
		kR16G16Hash			= 0xAEFF0B1CU, // "kR16G16"
		kDXT1Hash			= 0x09827A8CU, // "kDXT1"
		kDXT3Hash			= 0xE78C1BA0U, // "kDXT3"
		kDXT5Hash			= 0x0EEFBE95U, // "kDXT5"
		kBC4Hash			= 0xD43F714DU, // "kBC4"
		kBC5Hash			= 0xA33841DBU, // "kBC5"
		kBC6HHash			= 0x4288A97CU, // "kBC6H"
		kBC7Hash			= 0x4D3620F7U, // "kBC7"
		kG16R16FHash		= 0xD9315325U, // "kG16R16F"
		kA16B16G16R16FHash	= 0x70684114U, // "kA16B16G16R16F"
		kR32FHash			= 0xC5B47EB0U, // "kR32F"
		kA32B32G32R32FHash	= 0x59969CD4U, // "kA32B32G32R32F"
		kD24S8Hash			= 0x8782208DU, // "kD24S8"
		kD16Hash			= 0x23B577A7U, // "kD16"
		kR16FHash			= 0xA25C6FDAU, // "kR16F"
		kG32R32FHash		= 0x06914584U, // "kG32R32F"
		kNoneHash			= 0x0EB42269U, // "kNone"
	};

	uint32	m_TotalSize;	// The total size in bytes of the texel data
	uint16	m_Width;		// The width of the top-level image in texels
	uint16	m_Height;		// The height of the top-level image in texels
	uint16	m_Depth;		// The depth of the texture (1 for 2D, 1D and Cube textures, variable size for Volumes)
	uint16	m_FormatFlags;	// See enum TextureFormatFlags
	uint32	m_SourceFormat;	// The PID data format: See ddl select TextureFormat
	uint32	m_TargetFormat;	// The PDD data format: See ddl select TextureFormat
	uint32	m_AddressModeU;	// See ddl select TextureAddressMode
	uint32	m_AddressModeV;	// See ddl select TextureAddressMode
	uint32	m_AddressModeW;	// See ddl select TextureAddressMode
	float	m_MipBias;		// [-1 1] mip bias
	uint16	m_MipCount;		// The number of mips in the texture including the top-level mip
	uint16	m_Filter;		// Texture filter
};

struct TextureHeaderBuiltWin {
	uint32	m_TotalSize;	// The total size in bytes of the texel data
	uint16	m_Width;		// The width of the top-level image in texels
	uint16	m_Height;		// The height of the top-level image in texels
	uint16	m_Depth;		// The depth of the texture (1 for 2D, 1D and Cube textures, variable size for Volumes)
	uint16	m_FormatFlags;	// See enum TextureFormatFlags
	uint32	m_Format;		// See enum DXGI_FORMAT

	float	m_MipBias;
	uint16	m_MipCount;		// The number of mips in the texture including the top-level mip
	uint16	m_Filter;

	uint8	m_AddressModeU;	// See enum D3D11_TEXTURE_ADDRESS_MODE
	uint8	m_AddressModeV;	// See enum D3D11_TEXTURE_ADDRESS_MODE
	uint8	m_AddressModeW;	// See enum D3D11_TEXTURE_ADDRESS_MODE
	uint8	m_Pad;
};

const char *known[] = {
	// Anim Clip
	"Anim Set Built",
	"Anim Strings",
	"Anim Clip Lookup",
	"Anim Clip Data",

	"Anim Clip Group",
	"Anim Clip Group Data",

	"Anim Driver Class lookup",
	"Anim Driver Var Info",

	"Anim Clip Built",
	"Anim Clip Base State",
	"Anim Clip Sample Elem",
	"Anim Clip Sample Data",
	"Anim Clip Joint Hashes",
	"Anim Clip Motion Samples",
	"Anim Clip Custom Tracks",
	"Anim Clip Custom Track Data",
	"Anim Clip Trigger Data",

	"Anim Clip Path",                  // only used for PID
	"Anim Clip Trigger Joint Names",   // only used for PID

	"Anim Clip Curves",
	"Anim Clip Curves Data",

	"Anim Clip Pose Phoneme Visems Map",
	"Anim Clip Pose Expression Id Map",
	"Anim Clip Pose Bind Ids",

	"Anim Set Stream Lookup",
	"Anim Set Stream Sample Data",

	"Anim Driver Class built",
	"Anim Driver Class data",

	// Material
	"samplerCUBE",
	"sampler3D",
	"sampler2D",
	"sampler1D",

	"float4x4",
	"float4x3",
	"float3x4",
	"float3x3",
	"float4",
	"float3",
	"float2",
	"float",
	"int",

	"ViewProjection",
	"Orthographic",
	"ViewInverse",
	"Viewport",
	"Time",

	"Shader GPU LoD Descriptors",
	"Shader GPU LoDs",

	"Material Template PreShaders",
	"Material Template Samplers",
	"Material Template Globals",
	"Material Template Constants",
	"Material Template Constants Content",
	"Material Template LoD Descriptors",

	"Material Template LoD Shader 0",
	"Material Template LoD Shader 1",

	"Material Template LoD Win shader Data",
	"Material Template LoD Win shader Info",

	"Material Header",
	"Material Override Samplers",
	"Material Override Constants",
	"Material Override Constants Content",
	"Material Override LoD Descriptors",

	"Gloss Color",
	"Gloss Intensity",
	"Gloss Dirtiness",

	"BaseMap2D_Texture",
	"NormalMap2D_Texture",
	"GlossMap2D_Texture",
	//Texture
	"Texture Header",
	"Texel Data",

	//Model
	"m_DaeFileName",

	"Model Built",
	"Model Subset",
	"Model Look",
	"Model Index",
	"Model Std Vert",
	"Model Tex Vert",
	"Model Col Vert",
	"Model Material",
	"Model Skin Data",
	"Model Skin Batch",
	"Model Skin Joint Remap",
	"Model Joint",
	"Model Parent Ids",
	"Model Mirror Ids",
	"Model Joint Bspheres",
	"Model Joint Lookup",
	"Model Bind Pose",
	"Model Locator",
	"Model Locator Lookup",
	"Model Physics Data",
	"Model's Ragdoll meta data",
	"Model's Cloth meta data",
	"Model's physics av materials",
	"Model's ik setup data",
	//Visual Effect
	"Visual Effect KeyFrames Data",
	"Visual Effect KeyFrames Info",
	"Visual Effect Renderer Aux Data",
	"Visual Effect OnCollision Data",
	"Visual Effect Material Data",
	"Visual Effect Renderer Data",
	"Visual Effect Spawn Styles",
	"Visual Effect Emitter Data",
	"Visual Effect Asset Data",
	"Cone",
	"Disc",
	//Atmosphere
	"Atmosphere Built Struct",
	//Localization
	"Localization Built",
	//Zone
	"Zone Scene Objects",
	"Zone Visual Effect Names",
	"Zone Visual Effect Insts",
	"Zone Model Names",
	"Zone Model Insts",
	"Zone Lights",
	"Zone Volumes",
	"Zone Decals",
	"Zone Curves",
	"Zone Static Data",

	"Zone Decal Geometry",
	"Zone Decal Vertex Data",

	"Zone Actor Asset Paths",
	"Zone Actor Names",
	"Zone Actors",

	"Zone Actor Priuses",
	"Zone Actor Prius Data",

	"Zone Actor Groups",
	"Zone Actor Group Data",
	"Zone Actor Group Names",

	"Zone Script Strings",
	"Zone Script Vars",
	"Zone Script Plugs",
	"Zone Script Actions",
	"Zone Script Priuses",

	"Zone Texture 2D Asset Paths",
	"Zone Texture Cube Asset Paths",
	"Zone Texture Fill Asset Paths",
	"Zone Texture Misc Asset Paths",

	"Zone Material Asset Descriptions",

	"Zone built mesh information for present groups",
	"Zone individual navigation group data",
	"Zone Nav Data",
	"Zone instance header information",
	"Zone moving volume header information",
	"Zone moving volume information",
	"Zone moving volume data information",
	"Zone navigation override mesh data",

	"Zone built custom clue header information",
	"Zone built custom clue information",
	"Data for the custom clue information",

	"Zone Asset References",
	"Zone Zone References",

	"Zone Atmosphere Name",

	"Zone LighLink Data",

	"Zone Physics Data of mopp for static collision",
	//Actor
	"Actor Built",
	"Actor Object Built",
	"Actor Prius Built",
	"Actor Prius Built Data",
	"Actor Asset Refs",
	//Config

	"Config Type",
	"Config Built",
	"Config Asset Refs",

	//Sound
	"Sound Bank Built",
	"Sound Bank Built Stream Lookup",
	"Sound Bank Built Event Lookup",
	"Sound Bank Built Event Aux Lookup",
	"Sound Bank Built Event Aux Ids",

	"Sound Init Bank Built",
	"Sound Init Bank Built Trigger Lookup",
	"Sound Init Bank Built State Group Lookup",
	"Sound Init Bank Built State Lookup",
	"Sound Init Bank Built Switch Group Lookup",
	"Sound Init Bank Built Switch State Lookup",
	"Sound Init Bank Built Game Parameter Lookup",
	"Sound Init Bank Built Environment Lookup",

	"Sound Empty Bank Built",
	"Sound Stream",

	"Sound Bank Info",
	"Sound Bank Strings",
	//Conduit
	"Conduit Built",
	"Conduit Asset Refs",
	//Cinematic
	"Cinematic Type",
	"Cinematic Built",
	"Cinematic",
	"Cinematic Triggers",
	"Cinematic Events",
	"TriggerLocator",
	"XFOV",
	"DOFFocusDistance",
	"DOFFStop",
	"Cinematic Light Groups",
	"Cinematic Lights",
	"Cinematic Lighting Zones",
	"Cinematic Preview Zones",
	"KNone",
	"Cinematic Camera Cut Gap",
};

class BuiltFileHandler : public FileHandler {
	const char*		GetExt() override { return "built"; }
//	const char*		GetDescription() override;

	int				Check(istream_ref file) override {
		file.seek(0);
		uint32	t = file.get<uint32>();
		return t == DataFile::kDataFileId || t == DataFile::kDataFileIdReversed ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}

	ISO_ptr<void>	Read(tag id, istream_ref file) override;
} built;

ISO_ptr<void> BuiltFileHandler::Read(tag id, istream_ref file) {
	malloc_block	block(file, file.length());
	DataFile		*data	= block;
	if (!data->Fix())
		return ISO_NULL;

	ISO_ptr<anything>	p(id);
	for (int i = 0, n = data->m_BlockCount; i < n; i++) {
		DataBlockHeader	&block	= data->blocks[i];
		const char		*name	= 0;
		for (int j = 0; j < num_elements(known); j++) {
			if (StringCrc32(known[j]) == block.m_NameHash) {
				name = known[j];
				break;
			}
		}
		ISO_ptr<ISO_openarray<uint8> >	b(name ? name : (char*)to_string(hex(block.m_NameHash)), block.m_Size);
		memcpy(*b, (uint8*)data + block.m_Offset, block.m_Size);
		p->Append(b);
	}
	return p;
}
