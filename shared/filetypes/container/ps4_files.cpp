#include "iso/iso_files.h"
//#include "../../platforms/ps4/shared/gpu_shaders.h"
#include "../../platforms/ps4/shared/graphics_defs.h"
#include "common/shader.h"
#include "iso/iso_binary.h"
#include "comms/zlib_stream.h"
#include "base/algorithm.h"

using namespace iso;
using namespace ps4::shaders;

//-----------------------------------------------------------------------------
//	Shader binary
//-----------------------------------------------------------------------------

#ifdef ISO_EDITOR
struct PS4SB : anything {};
ISO_DEFUSER(PS4SB, anything);

ISO_DEFSAME(rel_string, uint32);
ISO_DEFUSERCOMPV(iorf, i, f);
ISO_DEFUSERCOMPV(MetaData::Buffer, resource_index,stride_size,pssl_type,internal_type,element_type,num_elements,element_offset,name);
ISO_DEFUSERCOMPV(MetaData::Constant, element_offset,register_index,constantbuffer_index,element_type,buffer_type,default_value,name);
ISO_DEFUSERCOMPV(MetaData::Element, type,used,byte_offset,size,array_size,default_value_offset,num_elements,element_offset, name, type_name);
ISO_DEFUSERCOMPV(MetaData::SamplerState, states, resource_index, name);
ISO_DEFUSERCOMPV(MetaData::Attribute, type,pssl_semantic,semantic_index,resource_index,interp_type,stream_number,name,semantic_name);
ISO_DEFUSERCOMPV(MetaData::StreamOut, stride,offset,semantic_index,slot,stream,pssl_semantic,components,semantic_name);

#else
struct PS4SB : ISO_openarray<uint8> {};
namespace ISO {
ISO_DEFUSER(PS4SB, ISO_openarray<uint8>);
}
#endif

class PS4SBFileHandler : public FileHandler {
	bool					WriteShader(ps4::shaders::ShaderHeader *sh, ps4::shaders::FileHeader::ShaderType type, ostream_ref file);

	const char*		GetExt() override { return "sb"; }
	int				Check(istream_ref file) override;
	ISO_ptr<void>	Read(tag id, istream_ref file) override;
	bool			Write(ISO_ptr<void> p, ostream_ref file) override;
} ps4sb;

int PS4SBFileHandler::Check(istream_ref file) {
	file.seek(0);
	ps4::shaders::Container		cont;//	= file.get();
	ps4::shaders::FileHeader	sfh;//		= file.get();
	return file.read(cont) && file.read(sfh) && sfh.fileHeader == ps4::shaders::FileHeader::HeaderId ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
}

ISO_ptr<void> PS4SBFileHandler::Read(tag id, istream_ref file) {
	malloc_block	mem(file, file.length());
	ps4::shaders::Container		*cont	= mem;
	ps4::shaders::FileHeader	*fh		= cont->GetFileHeader();
	if (fh->fileHeader != ps4::shaders::FileHeader::HeaderId)
		return ISO_NULL;

	ISO_ptr<PS4SB>	p(id);

#ifdef ISO_EDITOR
	ps4::shaders::ShaderHeader	*sh		= fh->GetShaderHeader();

	ISO_ptr<ISO_openarray<xint32> >	header("header", fh->shaderHeaderSizeInDW);
	memcpy(header->begin(), sh, fh->shaderHeaderSizeInDW * 4);
	p->Append(header);

	memory_block	ucode	= fh->GetUcode();
	ISO_ptr<ISO_openarray<xint32> >	code("code", ucode.size32() / 4);
	memcpy(code->begin(), ucode, ucode.length());
	p->Append(code);

	ISO_ptr<xint32[7]>	bin("binaryinfo");
	memcpy(*bin, GetShaderBinaryInfo(fh->GetVRAM()), sizeof(ps4::shaders::ShaderBinaryInfo));
	p->Append(bin);

	ps4::shaders::MetaData	*meta = fh->GetMetaData();
	if (int n = meta->num_buffers) {
		ISO_ptr<anything>	p2("Buffers", n);
		const ps4::shaders::MetaData::Buffer	*b = meta->Buffers();
		for (int i = 0; i < n; i++, b++)
			(*p2)[i] = ISO::MakePtr(b->name, *b);
		p->Append(p2);
	}

	if (int n = meta->num_constants) {
		ISO_ptr<anything>	p2("Constants", n);
		const ps4::shaders::MetaData::Constant	*b = meta->Constants();
		for (int i = 0; i < n; i++, b++)
			(*p2)[i] = ISO::MakePtr(b->name, *b);
		p->Append(p2);
	}

	if (int n = meta->num_elements) {
		ISO_ptr<anything>	p2("Elements", n);
		const ps4::shaders::MetaData::Element	*b = meta->Elements();
		for (int i = 0; i < n; i++, b++)
			(*p2)[i] = ISO::MakePtr(b->name, *b);
		p->Append(p2);
	}

	if (int n = meta->num_samplers) {
		ISO_ptr<anything>	p2("Samplers", n);
		const ps4::shaders::MetaData::SamplerState	*b = meta->SamplerStates();
		for (int i = 0; i < n; i++, b++)
			(*p2)[i] = ISO::MakePtr(b->name, *b);
		p->Append(p2);
	}

	if (int n = meta->num_input_attr) {
		ISO_ptr<anything>	p2("InputAttributes", n);
		const ps4::shaders::MetaData::Attribute	*b = meta->InputAttributes();
		for (int i = 0; i < n; i++, b++)
			(*p2)[i] = ISO::MakePtr(b->name, *b);
		p->Append(p2);
	}
	if (int n = meta->num_output_attr) {
		ISO_ptr<anything>	p2("OutputAttributes", n);
		const ps4::shaders::MetaData::Attribute	*b = meta->OutputAttributes();
		for (int i = 0; i < n; i++, b++)
			(*p2)[i] = ISO::MakePtr(b->name, *b);
		p->Append(p2);
	}
	if (int n = meta->num_stream_outs) {
		ISO_ptr<anything>	p2("StreamOuts", n);
		const ps4::shaders::MetaData::StreamOut	*b = meta->StreamOuts();
		for (int i = 0; i < n; i++, b++)
			(*p2)[i] = ISO::MakePtr(b->semantic_name, *b);
		p->Append(p2);
	}
#else
	memcpy(p->Create(mem.size32(), false), mem, mem.length());
#endif
	return p;
}

const uint32 pssl_crc_table[256] = {
	0x00000000,
	0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f, 0xe963a535,
	0x77073096,	0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f, 0xe963a535,
	0x9e6495a3, 0xee0e612c, 0x990951ba,	0x076dc419, 0x706af48f, 0xe963a535,
	0x9e6495a3, 0x0edb8832, 0x990951ba, 0x076dc419, 0x706af48f,	0xe963a535,
	0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0x076dc419, 0x706af48f, 0xe963a535,
	0x9e6495a3,	0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x706af48f, 0xe963a535,
	0x9e6495a3, 0x0edb8832, 0x79dcb8a4,	0xe0d5e91e, 0x97d2d988, 0xe963a535,
	0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,	0x09b64c2b,
	0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988, 0x09b64c2b,
	0x7eb17cbd,	0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988, 0x09b64c2b,
	0x7eb17cbd, 0xe7b82d07, 0x79dcb8a4,	0xe0d5e91e, 0x97d2d988, 0x09b64c2b,
	0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0xe0d5e91e, 0x97d2d988,	0x09b64c2b,
	0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x97d2d988, 0x09b64c2b,
	0x7eb17cbd,	0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2, 0x09b64c2b,
	0x7eb17cbd, 0xe7b82d07, 0x90bf1d91,	0x1db71064, 0x6ab020f2, 0xf3b97148,
	0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,	0xf3b97148,
	0x84be41de, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2, 0xf3b97148,
	0x84be41de,	0x1adad47d, 0x90bf1d91, 0x1db71064, 0x6ab020f2, 0xf3b97148,
	0x84be41de, 0x1adad47d, 0x6ddde4eb,	0x1db71064, 0x6ab020f2, 0xf3b97148,
	0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x6ab020f2,	0xf3b97148,
	0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7, 0xf3b97148,
	0x84be41de,	0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7, 0x136c9856,
	0x84be41de, 0x1adad47d, 0x6ddde4eb,	0xf4d4b551, 0x83d385c7, 0x136c9856,
	0x646ba8c0, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,	0x136c9856,
	0x646ba8c0, 0xfd62f97a, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7, 0x136c9856,
	0x646ba8c0,	0xfd62f97a, 0x8a65c9ec, 0xf4d4b551, 0x83d385c7, 0x136c9856,
	0x646ba8c0, 0xfd62f97a, 0x8a65c9ec,	0x14015c4f, 0x83d385c7, 0x136c9856,
	0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,	0x136c9856,
	0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9, 0xfa0f3d63,
	0x646ba8c0,	0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9, 0xfa0f3d63,
	0x8d080df5, 0xfd62f97a, 0x8a65c9ec,	0x14015c4f, 0x63066cd9, 0xfa0f3d63,
	0x8d080df5, 0x3b6e20c8, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,	0xfa0f3d63,
	0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0x14015c4f, 0x63066cd9, 0xfa0f3d63,
	0x8d080df5,	0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0x63066cd9, 0xfa0f3d63,
	0x8d080df5, 0x3b6e20c8, 0x4c69105e,	0xd56041e4, 0xa2677172, 0xfa0f3d63,
	0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,	0x3c03e4d1,
	0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172, 0x3c03e4d1,
	0x4b04d447,	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000,	0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,	0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000,	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000,
};

uint32 pssl_crc(void *p, size_t len, uint32 crc) {
	crc = ~crc;
	for (uint8 *b = (uint8*)p; len--; ++b)
		crc = pssl_crc_table[(crc ^ *b) & 0xff] ^ (crc >> 8);
	return ~crc;
}

bool PS4SBFileHandler::Write(ISO_ptr<void> p, ostream_ref file) {
	ISO::Browser	b(p);
	if (b.Is("PS4Shader")) {

		if (ISO::Browser b2 = *b[0])
			return WriteShader(b2[0], *b[1] ? ps4::shaders::FileHeader::PS : ps4::shaders::FileHeader::CS, file);

		if (ISO::Browser b2 = *b[1])
			return WriteShader(b2[0], ps4::shaders::FileHeader::VS, file);

		if (ISO::Browser b2 = *b[2])
			return WriteShader(b2[0], ps4::shaders::FileHeader::GS, file);

		return false;
	}
	void	*data	= b[0];
	size_t	size	= b.Count() * b[0].GetSize();
	size_t	asize	= ((size + 15) & ~15);

	uint32	associationHash0	= 0xca01e1a2;
	uint32	associationHash1	= 0x7bfffd07;
	uint32	crc					= pssl_crc(data, size, 0);

	ps4::shaders::Container		cont;
	ps4::shaders::FileHeader	sfh;
	ps4::shaders::Container::ShaderType		ctype;
	ps4::shaders::FileHeader::ShaderType	type;

#if 1
	VsShader					sh;
	clear(sh);
	sh.shaderSize				= asize + sizeof(ShaderBinaryInfo);
	sh.registers.rsrc1			= 0x000c0082;
	sh.registers.rsrc2			= 0x00000018;
	sh.registers.posFormat		= 4;
	ctype	= ps4::shaders::Container::VS;
	type	= ps4::shaders::FileHeader::VS;
#else
	PsShader					sh;
	clear(sh);
	sh.shaderSize				= asize + sizeof(ShaderBinaryInfo);
	sh.registers.rsrc1			= 0x00000082;
	sh.registers.rsrc2			= 0x00000018;
	sh.registers.zFormat		= 0;
	sh.registers.colFormat		= 4;
	sh.registers.inputEna		= 2;
	sh.registers.inputAddr		= 2;
	sh.registers.inControl		= 1;
	sh.registers.barycCntl		= 0;
	sh.registers.dbShaderControl= 0x10;
	sh.registers.cbShaderMask	= 0x0f;
	ctype	= ps4::shaders::Container::FS;
	type	= ps4::shaders::FileHeader::kPixelShader;
#endif

	uint32		shsize			= sh.computeSize();
	sh.registers.addr_lo		= shsize;
	sh.registers.addr_hi		= 0xffffffff;

	ShaderBinaryInfo			sbi;
	clear(sbi);
	memcpy(sbi.signature, "OrbShdr", 7);
	sbi.pssl					= 1;
	sbi.cached					= 1;
	sbi.type					= type;
	sbi.length					= size;
	sbi.chunk_offset			= 1;
//	sbi.num_input;
	sbi.associationHash0		= associationHash0;
	sbi.associationHash1		= associationHash1;
	sbi.crc						= pssl_crc(&sbi, 0x18, crc);

	clear(sfh);
	sfh.fileHeader				= ps4::shaders::FileHeader::HeaderId;
	sfh.majorVersion			= 6;
	sfh.type					= type;
	sfh.shaderHeaderSizeInDW	= shsize / 4;

	clear(cont);
	cont.formatVersionMinor		= 3;
	cont.compilerRevision		= 0x139b;
	cont.associationHash0		= associationHash0;
	cont.associationHash1		= associationHash1;
	cont.shader_type			= ctype;
	cont.code_type				= ps4::shaders::Container::ISA;
	cont.code_size				= uint32(sizeof(ps4::shaders::FileHeader) + shsize + asize + sizeof(ShaderBinaryInfo));

	file.write(cont);
	file.write(sfh);
	file.write(sh);
	file.writebuff(data, size);

	while (size++ & 15)
		file.putc(0);

	file.write(sbi);
	return true;
}

bool PS4SBFileHandler::WriteShader(ps4::shaders::ShaderHeader *sh, ps4::shaders::FileHeader::ShaderType	type, ostream_ref file) {
	ps4::shaders::Container		cont;
	clear(cont);
	cont.formatVersionMinor			= 3;
	cont.compilerRevision			= 0x139b;
	cont.code_type					= ps4::shaders::Container::ISA;
	cont.usesShaderResourceTable	= !!sh->shaderIsUsingSrt;

	ps4::shaders::FileHeader	fh;
	clear(fh);
	fh.fileHeader			= FileHeader::HeaderId;
	fh.majorVersion			= FileHeader::MajorVersion;
	fh.minorVersion			= FileHeader::MinorVersion;
	fh.type					= type;

	switch (type) {
		case ps4::shaders::FileHeader::VS: {
			VsShader	*vs			= (VsShader*)sh;
			fh.shaderHeaderSizeInDW	= vs->computeSize() / 4;
			cont.shader_type		= ps4::shaders::Container::VS;
			break;
		}
		case ps4::shaders::FileHeader::PS:
			cont.shader_type		= ps4::shaders::Container::FS;
			break;
		case ps4::shaders::FileHeader::GS:
			cont.shader_type		= ps4::shaders::Container::GS;
			break;
		case ps4::shaders::FileHeader::CS: {
			CsShader	*cs			= (CsShader*)sh;
			fh.shaderHeaderSizeInDW	= cs->computeSize() / 4;
			cont.shader_type		= ps4::shaders::Container::CS;
			break;
		}
		case ps4::shaders::FileHeader::ES:
			cont.shader_type		= ps4::shaders::Container::VS;
			break;
		case ps4::shaders::FileHeader::LS:
			cont.shader_type		= ps4::shaders::Container::VS;
			break;
		case ps4::shaders::FileHeader::HS:
			cont.shader_type		= ps4::shaders::Container::HS;
			break;
	}

	void	*bin = ISO::binary_data.unfix(sh->base_regs()->addr_lo);

	ShaderBinaryInfo	*info	= (ShaderBinaryInfo*)((uint8*)bin + sh->shaderSize) - 1;
	cont.associationHash0		= info->associationHash0;
	cont.associationHash1		= info->associationHash1;
	cont.code_size				= uint32(sizeof(ps4::shaders::FileHeader) + fh.shaderHeaderSizeInDW * 4 + sh->shaderSize + sh->embeddedConstantBufferSizeInDQW * 16);

	file.write(cont);
	file.write(fh);
	file.writebuff(sh, fh.shaderHeaderSizeInDW * 4);
	file.writebuff(bin, sh->shaderSize + sh->embeddedConstantBufferSizeInDQW * 16);

	//no meta
	MetaData	meta;
	clear(meta);
	file.write(meta);
	return true;
}

//-----------------------------------------------------------------------------
//	Razor Files
//-----------------------------------------------------------------------------

class PS4RazorFileHandler : public FileHandler {
	const char*		GetExt() override { return "razor"; }
	int				Check(istream_ref file) override;
	ISO_ptr<void>	Read(tag id, istream_ref file) override;
} ps4razor;

int PS4RazorFileHandler::Check(istream_ref file) {
	file.seek(0);
	return file.get<uint32>() == "SMC "_u32 ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
}

ISO_ptr<void> PS4RazorFileHandler::Read(tag id, istream_ref file) {
	if (file.get<uint32>() != "SMC "_u32)
		return ISO_NULL;

	ISO_ptr<anything>	p(id);
	while (!file.eof()) {
		struct CHUNK {
			uint32	id;
			uint32	length;
		};
		CHUNK chunk = file.get();
		if (chunk.id == "ENDS"_u32)
			break;

		ISO_ptr<ISO_openarray<uint8> >	raw(str((const char*)&chunk.id, 4));
		file.readbuff(raw->Create(chunk.length, false), chunk.length);
		p->Append(raw);
	}
	return p;
#if 0
	RIFF_chunk	riff(file);
	switch (riff.id) {
		case "GDS "_u32:
		case "MMAP"_u32:
		case "CNST"_u32:
		case "DRAW"_u32:
		case "DATA"_u32:
		case "KICK"_u32:
		case "ENDS"_u32:
	}
#endif
}

//-----------------------------------------------------------------------------
//	Razor GPU 2
//-----------------------------------------------------------------------------

class RZRXFileHandler : public FileHandler {
	const char*		GetExt() override { return "rzrx"; }
	//int				Check(istream_ref file) override;
	ISO_ptr<void>	Read(tag id, istream_ref file) override;
} rzrx;

struct header {
	enum {MAGIC = "rzrx"_u32};
	uint32		magic;
	uint32		zero;
	uint64		size;
};

struct packet_header {
	enum {
		HOST	= 6,
		USER	= 7,
	};
	uint32	id;		//0-10
	//uint8	b;		//10
	//uint8	c;		//10 or 50
	//uint8	d;		//01 or 02
	uint32	format;	//00, 80, 81
	uint64	size;
};

struct smc_gds {
	uint32	sms, gds;
	uint8	unknown[8];// 05 00 00 00 90 09 90 09
};

dynamic_array<uint32>	unique_ids;

ISO_ptr<void> ReadLeaf(uint32 id, istream_ref file, uint64 size) {
	switch (id) {
		case 0x1501003:
			return ISO_NULL;
		case 0x1302001: {
			ISO_ptr<anything>	a(to_string(hex(id)));
			uint64		end = file.tell() + size;
			smc_gds		h	= file.get();
			while (file.tell() < end) {
				uint32	id2		= file.get();
				uint32	size2	= file.get();
				ISO_ptr<ISO_openarray<uint8> >	b(count_string((char*)&id2, 4), size2);
				file.readbuff(*b, size2);
				a->Append(b);
				if (find(unique_ids, id2) == unique_ids.end())
					unique_ids.push_back(id2);
			}
			return a;
		}
		default: {
			ISO_ptr<ISO_openarray<uint8> >	a(to_string(hex(id)), size);
			file.readbuff(*a, size);
			return a;
		}
	}
}

ISO_ptr<void> ReadPacket(istream_ref file) {
	packet_header	p	= file.get();
	uint64			end = file.tell() + p.size;

	if (p.format & 0x80) {
		// not group
		ISO_ptr<void> a;
		if (p.format & 1) {// compressed?
			uint64	size	= file.get();
			zlib_reader	z(file, size);
			a = ReadLeaf(p.id, z, size);
		} else {
			a = ReadLeaf(p.id, file, p.size);
		}
		file.seek(end);
		return a;

	} else {
		// group
		ISO_ptr<anything>	a(to_string(hex(p.id)));
		while (file.tell() < end)
			a->Append(ReadPacket(file));

		file.seek(end);
		return a;
	}
	return ISO_NULL;
}

ISO_ptr<void> RZRXFileHandler::Read(tag id, istream_ref file) {
	header	h = file.get();
	if (h.magic != header::MAGIC)
		return ISO_NULL;

	ISO_ptr<anything>	a(id);
	uint64		end = h.size + 0x10;
	while (file.tell() < end) {
		a->Append(ReadPacket(file));
	}
	return a;
}
