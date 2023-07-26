#include "iso/iso_files.h"
#include "hashes/sha.h"
#include "codec/aes.h"
#include "codec/cbc.h"
#include "comms/rsa.h"
#include "archive_help.h"
#include "filetypes/sound/sample.h"

#pragma pack(1)

namespace wii {

typedef iso::uint8		u8;
typedef iso::uint16be	u16;
typedef iso::uint32be	u32;
typedef iso::uint64be	u64;
typedef iso::int8		s8;
typedef iso::int16be	s16;
typedef iso::int32be	s32;
typedef iso::int64be	s64;
typedef iso::float32be	f32;
typedef iso::float64be	f64;

template<int N, char C=0> struct pad_string {
	char	buffer[N];
	void	operator=(const char *s)	{ int len = iso::min(int(s ? strlen(s) : 0), N); memcpy(buffer, s, len); memset(buffer + len, C, N - len); }
};

struct disc_header {
	enum {size = 0x2440};

	pad_string<4>	game_name;
	pad_string<2>	company;
	u8				disc_no;
	u8				disc_ver;
	u8				audio_streaming;
	u8				stream_size;
	u8				_unused[14];
	u32				wii_magic;	// 0x5d1c9ea3
	u32				gc_magic;	// 0xc2339f3d
	pad_string<64>	title;
	u8				hash_disable;
	u8				encyption_disable;
	u8				_unused2[0x39e];
	//0x400
	u32				dm_offset;
	u32				dm_loadaddr;
	u32				_unused3[6];
	//0x420
	u32				dol_offset;	// >>2
	u32				fst_offset;	// >>2
	u32				fst_size;
	u32				fst_max_size;
	u32				user_pos;
	u32				user_len;
	u32				_unknown;
	u32				_unused4;
};

struct disc_header_pad : disc_header {
	u8	_[size - sizeof(disc_header)];
};

struct fst_entry {
	enum {FILE = 0, DIR = 1};
	u32		name_type;
	u32		offset;
	u32		length;
};

#define DATA_BASE_OFFSET		0x3000

// partitions

struct partition_table {
	u32		num_parts;
	u32		offset;
};

struct partition_info {
	u32		offset;
	u32		type;
};

// tickets

enum CERT {
	CERT_RSA4096	= 0,
	CERT_RSA2048	= 1,
	CERT_ECDSA		= 2,
};
enum KEY {
	KEY_COMMON		= 4,
	KEY_SDCARD		= 6,
};

typedef u8		sha1key[20];
typedef u8		aeskey[16];
typedef char	sig_issuer[0x40];

struct sig {
	enum SIG {
		RSA4096		= 0x10000,
		RSA2048		= 0x10001,
		ECDSA		= 0x10002,
	};
	u32	type;
};
struct sig_rsa4096 : sig {
	u8	sig[512];
	u8	fill[60];
};
struct sig_rsa2048 : sig {
	u8	sig[256];
	u8	fill[60];
};
struct sig_ecdsa : sig {
	u8	sig[60];
	u8	fill[64];
};

struct tik_limit {
	u32	tag;
	u32	value;
};

struct tik_view {
	u32			view;
	u64			ticket_id;
	u32			device_type;
	u64			title_id;
	u16			access_mask;
	u8			reserved[0x3c];
	u8			cidx_mask[0x40];
	u16			padding;
	tik_limit	limits[8];
};

struct tik {
	sig_issuer	issuer;
	u8			unknown[63]; //TODO: not really fill
	aeskey		cipher_title_key;
	u8			fill2;
	u64			ticket_id;
	u32			device_type;
	u64			title_id;
	u16			access_mask;
	u8			reserved[0x3c];
	u8			cidx_mask[0x40];
	u16			padding;
	tik_limit	limits[8];
};

struct sig_tik : sig_rsa2048, tik {};

struct tmd_content {
	u32		cid;				// content id
	u16		index;				// # number of the file
	u16		type;				// normal: 0x0001; shared: 0x8001
	u64		size;
	sha1key	hash;
};

struct tmd {
	sig_rsa2048	sig;
	sig_issuer	issuer;
	u8			version;
	u8			ca_crl_version;
	u8			signer_crl_version;
	u8			fill2;
	u64			sys_version;
	u64			title_id;
	u32			title_type;
	u16			group_id;		// publisher
	u16			zero;			//0x19a
	u16			region;			//0x19c
	u8			ratings[16];	//0x19e
	u8			reserved[12];	//0x1ae
	u8			ipc_mask[12];
	u8			reserved2[18];
	u32			access_rights;
	u16			title_version;
	u16			num_contents;
	u16			boot_index;
	u16			fill3;
	tmd_content	contents[1];//num_contents];
};

struct partition_header {
	sig_tik		cert;
	u32			tmd_size;
	u32			tmd_offset;			// >>2
	u32			certchain_size;
	u32			certchain_offset;	// >>2
	u32			h3_offset;			// >>2
	u32			data_offset;		// >>2
	u32			data_size;			// >>2
};

struct partition_header2 : partition_header {
	tmd			master;
	sig_tik		certs[4];
};

struct cert {
//	sig_rsa2048	sig;
	sig_issuer	issuer;
	u32			cert_id;
	sig_issuer	child;
	u32			unknown;
	u8			hash[256];
};

#pragma pack()
}

//-----------------------------------------------------------------------------
//	keys
//-----------------------------------------------------------------------------

using namespace iso;
using namespace wii;

mpi_const<uint32,
	0xf8246c58, 0xbae75003, 0x01fbb7c2, 0xebe00105,	0x71da9223, 0x78f0514e, 0xc0031dd0, 0xd21ed3d0,
	0x7efc8520, 0x69b5de9b, 0xb951a8bc, 0x90a24492,	0x6d379295, 0xae9436aa, 0xa6a30251, 0x0c7b1ded,
	0xd5fb2086, 0x9d7f3016, 0xf6be65d3, 0x83a16db3,	0x321b9535, 0x1890b170, 0x02937ee1, 0x93f57e99,
	0xa2474e9d, 0x3824c7ae, 0xe38541f5, 0x67e7518c,	0x7a0e38e7, 0xebaf4119, 0x1bcff17b, 0x42a6b4ed,
	0xe6ce8de7, 0x318f7f52, 0x04b3990e, 0x226745af,	0xd485b244, 0x93008b08, 0xc7f6b7e5, 0x6b02b3e8,
	0xfe0c9d85, 0x9cb8b682, 0x23b8ab27, 0xee5f6538,	0x078b2db9, 0x1e2a153e, 0x85818072, 0xa23b6dd9,
	0x3281054f, 0x6fb0f6f5, 0xad283eca, 0x0b7af354,	0x55e03da7, 0xb68326f3, 0xec834af3, 0x14048ac6,
	0xdf20d285, 0x08673cab, 0x62a2c7bc, 0x131a533e,	0x0b66806b, 0x1c30664b, 0x372331bd, 0xc4b0cad8,
	0xd11ee7bb, 0xd9285548, 0xaaec1f66, 0xe821b3c8,	0xa0476900, 0xc5e688e8, 0x0cce3c61, 0xd69cbba1,
	0x37c6604f, 0x7a72dd8c, 0x7b3e3d51, 0x290daa6a,	0x597b081f, 0x9d3633a3, 0x467a3561, 0x09aca7dd,
	0x7d2e2fb2, 0xc1aeb8e2, 0x0f4892d8, 0xb9f8b46f,	0x4e3c11f4, 0xf47d8b75, 0x7dfefea3, 0x899c3359,
	0x5c5efdeb, 0xcbabe841, 0x3e3a9a80, 0x3c69356e,	0xb2b2ad5c, 0xc4c85845, 0x5ef5f7b3, 0x0644b47c,
	0x64068cdf, 0x809f7602, 0x5a2db446, 0xe03d7cf6,	0x2f34e702, 0x457b02a4, 0xcf5d9dd5, 0x3ca53a7c,
	0xa629788c, 0x67ca08bf, 0xecca43a9, 0x57ad16c9,	0x4e1cd875, 0xca107dce, 0x7e0118f0, 0xdf6bfee5,
	0x1ddbd991, 0xc26e60cd, 0x4858aa59, 0x2c820075,	0xf29f526c, 0x917c6fe5, 0x403ea7d4, 0xa50cec3b,
	0x7384de88, 0x6e82d2eb, 0x4d4e42b5, 0xf2b149a8,	0x1ea7ce71, 0x44dc2994, 0xcfc44e1f, 0x91cbd495
> root_key_mod;
mpi_const<uint32, 0x010001> root_key_exp;

wii::aeskey	common		= {0xeb, 0xe4, 0x2a, 0x22, 0x5e, 0x85, 0x93, 0xe4, 0x48, 0xd9, 0xc5, 0x45, 0x73, 0x81, 0xaa, 0xf7};		//common key
wii::aeskey	common_dev	= {0xa1, 0x60, 0x4a, 0x6a, 0x71, 0x23, 0xb5, 0x29, 0xae, 0x8b, 0xec, 0x32, 0xc8, 0x16, 0xfc, 0xaa};		//common key for devs
wii::aeskey	sdkey		= {0xab, 0x01, 0xb9, 0xd8, 0xe1, 0x62, 0x2b, 0x08, 0xaf, 0xba, 0xd8, 0x4d, 0xbf, 0xc2, 0xa5, 0x5d}; 	//sd key
wii::aeskey	sdiv		= {0x21, 0x67, 0x12, 0xe6, 0xaa, 0x1f, 0x68, 0x9f, 0x95, 0xc5, 0xa2, 0x23, 0x24, 0xdc, 0x6a, 0x98}; 	//SD IV
wii::aeskey	md5blanker	= {0x0e, 0x65, 0x37, 0x81, 0x99, 0xbe, 0x45, 0x17, 0xab, 0x06, 0xec, 0x22, 0x45, 0x1a, 0x57, 0x93}; 	//MD5 blanker

//xs_dpki.eccPubKey
mpi_const<uint32,
	0x01d5a23c, 0xe8e9df8c, 0x0aa5aae1, 0x76e1247c,	0x9097a6fe, 0x2ad38cb4, 0xde7432de, 0x5b8401ac,
	0xb213719d, 0x4b9cebd4, 0x13c22766, 0xc55f8397,	0xc483a39e, 0x3bfce8a9, 0xb5103a7b
> xs_dpki_eccPubKey;

//cp_dpki.rsa
RSA::parameters cp_dpki_rsa = {
	mpi_const<uint32, 0x010001>(),
	mpi_const<uint32,
		0x32b5c323, 0x677b04f2, 0xbb009fbc, 0x374d9d2f,	0x71db8cc7, 0x06b4ef0e, 0x434edcdd, 0x77c016d8,
		0x1610e35f, 0x0e680d08, 0xaf0e8efb, 0x53ddabe0,	0x9c759efc, 0xb86c8e62, 0xbb6e6b99, 0x63d876b1,
		0x9805cac4, 0x92be2f5a, 0x76e8caf4, 0xaca2b032,	0x8ab7bbeb, 0xb3f1c2b1, 0x809d17ba, 0x4ff65b23,
		0xf65dad43, 0xa6f1e447, 0x7c4f932e, 0x0475ee2e,	0x096eca17, 0x0e584c99, 0x7c4507d8, 0x7091de89,
		0x13b9ba47, 0x334f4f79, 0x58163f86, 0x11359148,	0x9ba7f777, 0xb0f048e4, 0x1141f425, 0x7de5ac71,
		0x6fcd9b6e, 0xd230ff1d, 0x38e92fe0, 0xd9a98968,	0xb6ac45c8, 0x3b9f4930, 0x933b696b, 0xbe20684b,
		0x871f7cea, 0xbcc277ed, 0xcc36cda0, 0xc6ad46f2,	0xfd2db955, 0x2f81d5a7, 0xdf4e5be3, 0xea92b4bf,
		0x22aa05a2, 0x867b1a06, 0xfb580064, 0xc0e0f89e,	0x1b1c3c23, 0x20a403ec, 0x277a9d8b, 0xd271abfd
	>(),
	mpi_const<uint32,
		0xede7d9f1, 0x315a89c3, 0x4a361fb8, 0x4767b3bb,	0x7c51fb74, 0x07dd6f97, 0xe38fe1b3, 0xbb3d1cc2,
		0x2849906b, 0x8a3ae273, 0x5277cda7, 0x5c5ab2e1,	0xd64035ad, 0x450f42fd, 0x8736958d, 0x1096bd25,
		0x207d5575, 0x65a620a8, 0xc99def15, 0xa06a14f6,	0x2219c12b, 0xad8dc2d5, 0x7d71e30b, 0xdbc6bacc,
		0x9d8a651c, 0xd21c20e8, 0x98ef8980, 0x3c3d5449,	0x77aa145e, 0xc6e8b922, 0xe66a68dd, 0x4f547d27
	>(),
	mpi_const<uint32,
		0xd2bef45d, 0xcc5bdd86, 0x6653039c, 0x75611ce4,	0x19aa1f8e, 0xc0bbc6c3, 0x7084536b, 0xcdbd5a63,
		0xf620a346, 0xff515d31, 0x7591c672, 0x8cb958ee,	0x6c101529, 0xf6d69c8c, 0xec4c47b8, 0x83c2f4d6,
		0x4e95868c, 0x1fb94e77, 0xb0d942b5, 0xc83b5346,	0xf90d33a0, 0xbe68139e, 0xff98f358, 0xc4e102b9,
		0x0b175c1b, 0xc351003d, 0x047d8575, 0x0530ccc4,	0x409df1b7, 0x6d30fead, 0x570ce93e, 0x0f077c7b
	>(),
};

//xs_dpki.rsa
RSA::parameters xs_dpki_rsa = {
	mpi_const<uint32, 0x010001>(),
	mpi_const<uint32,
		0x5300393f, 0x1fa621fa, 0x9eb90c94, 0xf8f3485e,	0x73acecfd, 0x34744630, 0x01d5ef1d, 0x3988b226,
		0x51c7d112, 0xc2aaae3b, 0x10124818, 0x5460eac6,	0x638fae8e, 0x5e62ae7f, 0x6c8a61b2, 0x7f250db5,
		0x6215af56, 0x71bfc68c, 0x531d70d3, 0x36e63712,	0x260c61b6, 0x11252c7c, 0x492a2eae, 0xbf01ba7f,
		0x8243ded6, 0x627c6147, 0xb8ada764, 0xd5e5af1d,	0xe6a90d83, 0x2d7400d5, 0x8645eaca, 0x31f10bfb,
		0xcf92ffb9, 0x9518b4ba, 0xac495b04, 0x45396a3e,	0xb952a947, 0x18a46018, 0x3dfde243, 0x5e642c48,
		0xc465c575, 0x81b4c912, 0x647e7e9e, 0x6e3da282,	0x47bdab40, 0x765f3950, 0x3c933a0d, 0xd7009bca,
		0x5063dd1c, 0x8669e19d, 0xd1ff9903, 0x7082cfe2,	0xbb709e51, 0xfc9c533f, 0x3f45a72f, 0x9dec050b,
		0xe75d89d4, 0xc42f50b6, 0x91f581a5, 0x83c7079c,	0x82649cd7, 0x664a1b4f, 0x347e3cfc, 0x6b730151
	>(),
	mpi_const<uint32,
		0xff003796, 0x71e193fb, 0x2a90edc7, 0x465b7be2,	0xfd0309e5, 0xc5a3ae2f, 0x4f014b03, 0x5d933605,
		0xe8f02f8a, 0xf7e51a2e, 0x2ea59b06, 0x79b89098,	0x5405cfdf, 0x6aebbb71, 0x4feb164a, 0x7d25a6a9,
		0xef8e7687, 0xe4c7c863, 0x54f4c36d, 0x0e7e0fc5,	0x19bd9d9d, 0x71503eba, 0x47ef0003, 0xf8b8b90a,
		0x6f40028d, 0x71963fa2, 0xd3b39c8c, 0xca094d0f,	0x7b14dea4, 0x01d5f1cb, 0x7eba5626, 0x5aa6c09b
	>(),
	mpi_const<uint32,
		0xcca6fd18, 0xa4fefc1c, 0xb1fab537, 0x380ebd87,	0xb0c3d65f, 0xf4cb46ef, 0x0d521faa, 0x66fb96e2,
		0xd7df6066, 0x32364f97, 0x06ad3a94, 0x8c4e6892,	0x4fbfd8a7, 0x128ddc1f, 0x2dd7c093, 0xd7f97c76,
		0x165b0d4c, 0x4e02f628, 0xb9e16882, 0x9e506bae,	0x64fa24a5, 0xcbe320e2, 0x6964815c, 0x528d5d64,
		0x09d02ac8, 0xb2edc59d, 0x3eed6252, 0xb5569ca2,	0xbd107a5b, 0x6d008faa, 0x964f5fc3, 0x67ead70f
	>(),
};

//-----------------------------------------------------------------------------
//	Certificates
//-----------------------------------------------------------------------------

wii::sig *FindCert(wii::cert *sub, uint8 *cert, size_t cert_size) {
	char	parent[64], *child;

	strncpy(parent, (char*)sub, sizeof(parent));
	parent[sizeof parent - 1] = 0;
	child = strrchr(parent, '-');
	if (child) {
		*child++ = 0;
	} else {
		*parent = 0;
		child = (char*)sub;
	}

	uint32		sig_len, sub_len;
	for (uint8 *p = cert; p < cert + cert_size; p += sig_len + sub_len) {
		wii::sig	*sig = (wii::sig*)p;
		switch (sig->type) {
			case wii::sig::RSA4096:	sig_len = sizeof(wii::sig_rsa4096); break;
			case wii::sig::RSA2048:	sig_len = sizeof(wii::sig_rsa2048); break;
			case wii::sig::ECDSA:	sig_len = sizeof(wii::sig_rsa4096); break;
			default:				return 0;
		}
		char	*issuer = (char*)p + sig_len;
		switch (*(uint32be*)(issuer + 0x40)) {
			case wii::CERT_RSA4096:	sub_len = 0x2c0; break;
			case wii::CERT_RSA2048:	sub_len = 0x1c0; break;
			case wii::CERT_ECDSA:	sub_len = 0x100; break;
			default:				return 0;
		}
		if (strcmp(parent, issuer) == 0 && strcmp(child, issuer + 0x44) == 0)
			return sig;
	}

	return 0;
}

bool CheckRSA(void *hash, uint32 hash_len, uint8 *sig, param(mpi) mod, param(mpi) exp) {
	uint32	n = mod.num_bits() / 8;
	uint8 correct[0x200];
	static const uint8 ber[] = "\x00\x30\x21\x30\x09\x06\x05\x2b\x0e\x03\x02\x1a\x05\x00\x04\x14";
	correct[0] = 0;
	correct[1] = 1;
	memset(correct + 2, 0xff, n - 38);
	memcpy(correct + n - 36, ber, 16);
	memcpy(correct + n - 20, hash, hash_len);

	mpi	rr;
	mpi	x	= exp_mod(mpi(sig, n), exp, mod, rr);

	return memcmp(correct, x.elements_ptr(), n) == 0;
}


int CheckCerts(void *data, size_t data_size, uint8 *certs, size_t cert_size) {
	wii::sig	*sig	= (wii::sig*)data;
	uint32		sig_len;
	switch (sig->type) {
		case wii::sig::RSA4096:	sig_len = sizeof(wii::sig_rsa4096); break;
		case wii::sig::RSA2048:	sig_len = sizeof(wii::sig_rsa2048); break;
		case wii::sig::ECDSA:	sig_len = sizeof(wii::sig_rsa4096); break;
		default:				return -1;
	}

	wii::cert*	sub		= (wii::cert*)((uint8*)sig + sig_len);
	size_t		sub_len = data_size - sig_len;
	if (sub_len == 0)
		return -2;

	SHA1::CODE	sha1key;

	for (;;) {
		if (strcmp(sub->issuer, "Root") == 0) {
			sha1key	= SHA1(sub, sub_len);
			if (sig->type != wii::sig::RSA4096)
				return -8;
			return CheckRSA(&sha1key, sizeof(sha1key), ((wii::sig_rsa4096*)sig)->sig, root_key_mod, root_key_exp) ? 0 : -5;
		}

		wii::sig	*key_cert = FindCert(sub, certs, cert_size);
		if (!key_cert)
			return -3;

		switch (key_cert->type) {
			case wii::sig::RSA4096:	sig_len = sizeof(wii::sig_rsa4096); break;
			case wii::sig::RSA2048:	sig_len = sizeof(wii::sig_rsa2048); break;
			case wii::sig::ECDSA:	sig_len = sizeof(wii::sig_rsa4096); break;
		}

		sha1key	= SHA1(sub, sub_len);

		wii::cert	*key = (wii::cert*)((uint8*)key_cert + sig_len);
		if (sig->type - 0x10000 != key->cert_id)
			return -6;

		switch (sig->type) {
			case wii::sig::RSA2048:
				if (!CheckRSA(&sha1key, sizeof(sha1key), ((wii::sig_rsa4096*)sig)->sig, mpi(key->hash), mpi(0x10001)))
					return -5;
				break;
			default:
				return -7;
		}

		sub = (wii::cert*)((uint8*)sig + sig_len);
		switch (sub->cert_id) {
			case wii::CERT_RSA4096:	sub_len = sizeof(wii::sig_rsa4096) + sizeof(wii::sig_issuer) * 2; break;
			case wii::CERT_RSA2048:	sub_len = sizeof(wii::sig_rsa2048) + sizeof(wii::sig_issuer) * 2; break;
			case wii::CERT_ECDSA:	sub_len = sizeof(wii::sig_rsa4096) + sizeof(wii::sig_issuer) * 2; break;
			default:				return -5;
		}
	}
}

//-----------------------------------------------------------------------------
//	WiiDisk
//-----------------------------------------------------------------------------

void init_ticket(wii::sig_tik &t, uint64 title_id, const char *issuer) {
	strcpy(t.issuer, "Root-CA00000002-XS00000006");
	t.type			= wii::sig::RSA2048;
	t.title_id		= title_id;
	t.access_mask	= 0xffff;
	memset(t.cidx_mask, 0xff, 64);
}

class WiiDiscInput : public reader_mixin<WiiDiscInput> {
	istream_ref		stream;
	streamptr	start, total, current, cluster_start;
	AES::block	key;
	malloc_block	cluster;
public:
	WiiDiscInput(istream_ref stream, streamptr start, streamptr total, AES::block &key)
		: stream(move(stream)), start(start), total(total), key(key), current(0), cluster_start(total), cluster(0x7c00) {}

	size_t		readbuff(void *buffer, size_t size) {
		size_t	size2	= 0;
		while (size) {
			if (current < cluster_start || current >= cluster_start + 0x7c00) {
				uint32	cluster_num	= uint32(current / 0x7c00);
				cluster_start		= cluster_num * 0x7c00;
				stream.seek(start + cluster_num * 0x8000LL);

				uint8		h12[0x400];
				stream.readbuff(h12, 0x400);

				CBC_decrypt<AES_decrypt, 16, istream_ref> aes(const_memory_block(&key), stream, h12 + 0x3d0);
				aes.readbuff(cluster, 0x7c00);
			}
			uint32	a	= uint32(current - cluster_start);
			uint32	b	= min(0x7c00 - a, size);
			memcpy(buffer, cluster + a, b);
			size	-= b;
			size2	+= b;
			current	+= b;
			buffer	= (uint8*)buffer + b;
		}
		return size2;
	}

	void		seek(streamptr offset)		{ current = offset; }
	void		seek_cur(streamptr offset)	{ current += offset;  }
	void		seek_end(streamptr offset)	{ current = total + offset;  }
	streamptr	tell(void)					{ return current;	}
	streamptr	length(void)				{ return total;		}
};

struct WiiDiskPartition {
	const filename	fn;
	streamptr	start, total;
	AES::block	key;
	WiiDiskPartition(const filename &fn, streamptr start, streamptr total, AES::block key) : fn(fn), start(start), total(total), key(key) {}
};

class WiiDiskFile : public ISO::VirtualDefaults {
	WiiDiskPartition	*part;
	streamptr			offset, length;
	ISO::WeakRef<void>	weak;
//	ISO_ptr<void>		p;
public:
	void	Init(WiiDiskPartition *_part, streamptr _offset, streamptr _length) {
		part	= _part;
		offset	= _offset;
		length	= _length;
	}
	ISO::Browser2	Defref() {
		ISO_ptr<void>	p = weak;
		if (!p) {
			FileInput		file(part->fn);
			WiiDiscInput	wdi(file, part->start, part->total, part->key);
			wdi.seek(offset);
			p		= ReadRaw(none, wdi, uint32(length));
			weak	= p;
		}
		return p;
	}
};

ISO_DEFVIRT(WiiDiskFile);

class WiiDiskFileHandler : public FileHandler {
	const char*		GetExt() override { return "rvm"; }
	ISO_ptr<void>	Read(tag id, istream_ref file) override;
	ISO_ptr<void>	ReadWithFilename(tag id, const filename &fn) override;
	bool			Write(ISO_ptr<void> p, ostream_ref file) override;
} wiidisk;

ISO_ptr<void> WiiDiskFileHandler::ReadWithFilename(tag id, const filename &fn) {
	FileInput	file(fn);
	uint8	sector0[0x800];
	file.read(sector0);
	uint32	disc_offset	= sector0[0] == 0xff ? 0x8000 : 0;

	partition_table	part_table[4];
	file.seek(0x40000 + disc_offset);
	file.read(part_table);

	int				num_parts	= part_table[0].num_parts;
	partition_info	*parts		= new partition_info[num_parts];
	file.seek(part_table[0].offset * 4 + disc_offset);
	file.readbuff(parts, sizeof(partition_info) * num_parts);

	malloc_block		h3(0x18000);
	ISO_ptr<anything>	result(id);

	for (int i = 0; i < num_parts; i++) {
		partition_info		&part		= parts[i];
		streamptr			part_start	= part.offset * 4 + disc_offset;

		file.seek(part_start);
		partition_header	part_head	= file.get();

		file.seek(part_start + part_head.tmd_offset * 4);
		malloc_block		tmd_buffer(file, part_head.tmd_size);
		wii::tmd			*tmd = tmd_buffer;

		file.seek(part_start + part_head.h3_offset * 4);
		file.readbuff(h3, 0x18000);

		file.seek(part_start + part_head.certchain_offset * 4);
		uint32				cert_size = part_head.certchain_size;
		malloc_block		certs(file, cert_size);

		CheckCerts(&part_head, sizeof(part_head), certs, cert_size);
		CheckCerts(tmd, part_head.tmd_size, certs, cert_size);

		SHA1::CODE			code_be = SHA1(h3, 0x18000);
		if (memcmp(&code_be, tmd->contents[0].hash, sizeof(code_be)) != 0)
			throw("Bad TMD hash");

		AES::block	iv;
		memcpy(&iv, &part_head.cert.title_id, 8);

		AES	aes;
		aes.setkey_dec(disc_offset ? common_dev : common, sizeof(aeskey) * 8);
		AES::block	key	= force_cast<AES::block>(part_head.cert.cipher_title_key);
		aes.decrypt(key);
		key ^= iv;

		if (part.type != 0)
			continue;

		WiiDiskPartition	*wdp	= new WiiDiskPartition(fn,part_start + part_head.data_offset * 4LL, part_head.data_size * 4LL, key);
		WiiDiscInput		wdi(file, part_start + part_head.data_offset * 4LL, part_head.data_size * 4LL, key);
		disc_header			dh		= wdi.get();

		wdi.seek(dh.fst_offset * 4);
		malloc_block		fst_buffer(file, dh.fst_size * 4);
		fst_entry			*fst	= fst_buffer;

		char				*names	= (char*)(fst + fst->length);
		ISO_ptr<anything>	dir		= result;
		int					end		= fst->length;
		pair<ISO_ptr<anything>,int> stack[32], *sp = stack;

		for (int i = 1; ; i++) {
			while (i == end) {
				if (sp == stack)
					goto done;
				--sp;
				dir		= sp->a;
				end		= sp->b;
			}
			fst_entry	&entry	= fst[i];
			uint32	name_type	= entry.name_type;
			tag		id			= names + (name_type & 0xfffff);
			if (name_type & 0xff000000) {
				*sp++	= make_pair(dir, end);
				end		= entry.length;
				result->Append(dir.Create(id));
			} else {
				ISO_ptr<WiiDiskFile>	p(id);
				p->Init(wdp, entry.offset * 4, entry.length);
				dir->Append(p);
			}
		}
done:;
	}
	return result;
}

ISO_ptr<void> WiiDiskFileHandler::Read(tag id, istream_ref file) {
	uint8	sector0[0x800];
	file.read(sector0);
	uint32	disc_offset	= sector0[0] == 0xff ? 0x8000 : 0;

	partition_table	part_table[4];
	file.seek(0x40000 + disc_offset);
	file.read(part_table);

	int				num_parts	= part_table[0].num_parts;
	partition_info	*parts		= new partition_info[num_parts];
	file.seek(part_table[0].offset * 4 + disc_offset);
	file.readbuff(parts, sizeof(partition_info) * num_parts);

	malloc_block			h3(0x18000);
	ISO_ptr<anything>	result(id);

	for (int i = 0; i < num_parts; i++) {

		partition_info		&part		= parts[i];
		streamptr			part_start	= part.offset * 4 + disc_offset;

		file.seek(part_start);
		partition_header	part_head	= file.get();

		file.seek(part_start + part_head.tmd_offset * 4);
		malloc_block		tmd_buffer(file, part_head.tmd_size);
		wii::tmd			*tmd = tmd_buffer;

		file.seek(part_start + part_head.h3_offset * 4);
		file.readbuff(h3, 0x18000);

		SHA1::CODE			code_be = SHA1(h3, 0x18000);
		if (memcmp(&code_be, tmd->contents[0].hash, sizeof(code_be)) != 0)
			throw("Bad TMD hash");

		AES::block	iv;
		memcpy(&iv, &part_head.cert.title_id, 8);

		AES	aes;
		aes.setkey_dec(disc_offset ? common_dev : common, sizeof(aeskey) * 8);
		AES::block	key = force_cast<AES::block>(part_head.cert.cipher_title_key);
		aes.decrypt(key);
		key ^= iv;

		if (part.type != 0)
			continue;

		WiiDiscInput	wdi(file, part_start + part_head.data_offset * 4LL, part_head.data_size * 4LL, key);
		disc_header		dh		= wdi.get();

		wdi.seek(dh.fst_offset * 4);
		malloc_block	fst_buffer(file, dh.fst_size * 4);
		fst_entry		*fst	= fst_buffer;

		char			*names	= (char*)(fst + fst->length);

		ISO_ptr<anything>	dir		= result;
		int					end		= fst->length;
		pair<ISO_ptr<anything>,int> stack[32], *sp = stack;

		for (int i = 1; ; i++) {
			while (i == end) {
				if (sp == stack)
					goto done;
				--sp;
				dir		= sp->a;
				end		= sp->b;
			}
			fst_entry	&entry	= fst[i];
			uint32	name_type	= entry.name_type;
			tag		id			= names + (name_type & 0xfffff);
			if (name_type & 0xff000000) {
				*sp++	= make_pair(dir, end);
				end		= entry.length;
				result->Append(dir.Create(id));
			} else {
				wdi.seek(entry.offset * 4);
				dir->Append(ReadRaw(id, wdi, entry.length));
//				dir->Append(ISO_ptr<int>(id, entry.length));
			}
		}
done:;
#if 0
		uint32	nclusters = (part_head.data_size + 0x1fff) / 0x2000;

		for (uint32 i = 0; i < nclusters; i++) {
			file.readbuff(h12, 0x400);
			AES_decrypt		aes(const_memory_block(key), h12 + 0x3d0);
			aes.read(file, data, 0x7c00);
			out.writebuff(data, 0x7c00);
		}
#endif
	}

	return result;
}

bool WiiDiskFileHandler::Write(ISO_ptr<void> p, ostream_ref file) {
	ISO::Browser	vars	= ISO::root("variables");
	const char	*game_name	= vars["game_name"].GetString("ABCD");
	const char	*company	= vars["company"].GetString("XX");
	const char	*title		= vars["title"].GetString("Sample");
	const char	*firmware	= vars["firmware"].GetString();
	uint16		ver			= vars["version"].GetInt(1);
	uint64		title_id	= *(uint32be*)game_name + (uint64(ver) << 48);

	uint8	sector[2048];
	clear(sector);
	sector[0] = sector[1] = 0xff;
	file.write(sector);

	clear(sector);
	sector[46] = 0xe0;
	sector[47] = 0x06;
	sector[50] = 0xbc;
	sector[51] = 0x4d;
	file.write(sector);

	clear(sector);
	for (int i = 2; i < 16; i++)
		file.write(sector);

	disc_header		&dh = *(disc_header*)sector;
	dh.game_name	= game_name;
	dh.company		= company;
	dh.wii_magic	= 0x5d1c9ea3;
	dh.title		= title;
	file.write(sector);


	clear(sector);
	for (int i = 1; i < 128; i++)
		file.write(sector);

	partition_table	*part_table = (partition_table*)sector;
	partition_info	*parts		= (partition_info*)(sector + 0x20);

	if (firmware) {
		FileInput	fw(firmware);
		fw.read(sector);
#if 0
		part_table[0].num_parts		= 2;
		part_table[0].offset		= 0x40020 >> 2;
		parts[0].offset				= 0x50000 >> 2;
		parts[0].type				= 1;
		parts[1].offset				= 0xf800000 >> 2;
		parts[1].type				= 0;
#endif
		file.write(sector);
		malloc_block	temp(1024 * 1024);
		while (uint32 read = fw.readbuff(temp, 1024 * 1024))
			file.writebuff(temp, read);

	} else {
		part_table[0].num_parts		= 1;
		part_table[0].offset		= 0x40020 >> 2;
		parts[0].offset				= 0x50000 >> 2;
		parts[0].type				= 0;
		file.write(sector);

		clear(sector);
		for (int i = 1; i < 31; i++)
			file.write(sector);

		((uint32be*)(sector + 0x800))[-1] = 0xc3f81a8e;
		file.write(sector);
	}

	wii::partition_header2	head;
	clear(head);
	init_ticket(head.cert, title_id, "Root-CA00000002-XS00000006");

	strcpy(head.master.issuer, "Root-CA00000002-XS00000006");

	init_ticket(head.certs[0], title_id, "Root-CA00000002-XS00000006");
	init_ticket(head.certs[1], title_id, "Root-CA00000002-XS00000006");
	init_ticket(head.certs[2], title_id, "Root-CA00000002-XS00000006");
	init_ticket(head.certs[3], title_id, "Root-CA00000002-XS00000006");

	file.write(head);

#if 0
DvdRoot="wii/data"
BootFileName="wii/xracing.dol"
LDRFileName="$(REVOLUTION_SDK_ROOT)RVL\boot\apploader.img"
BI2FileName="wii/bi2.bin"

;FSTMaxLength=0x080000
FSTMaxLength=0
FSTEndAddress=0x80400000
;FSTuserTop=0x00100000
FSTuserEnd=0x100000000

CountryCode=us
#endif

	return false;
}

//-----------------------------------------------------------------------------
//	WiiDisk - RPF
//-----------------------------------------------------------------------------

class RPFFileHandler : public FileHandler {
	const char*		GetExt() override { return "rpf"; }
	ISO_ptr<void>	Read(tag id, istream_ref file) override;
} rpf;

ISO_ptr<void> RPFFileHandler::Read(tag id, istream_ref file) {
	return ISO_NULL;
}

//-----------------------------------------------------------------------------
//	Wii archives
//-----------------------------------------------------------------------------

class WiiARCFileHandler : public FileHandler {
	ISO_ptr<void>			ReadYAZ0(tag id, istream_ref file);
	ISO_ptr<void>			ReadRARC(tag id, istream_ref file);
	ISO_ptr<void>			ReadDARCH(tag id, istream_ref file);

	const char*		GetExt() override { return "arc"; }
	const char*		GetDescription() override { return "Nintendo YAZ0 or RARC archive";	}
	ISO_ptr<void>	Read(tag id, istream_ref file) override;
} wii_arc;

#define DARCH_MAGIC 0x55aa382d

ISO_ptr<void> WiiARCFileHandler::Read(tag id, istream_ref file) {
	uint32	magic	= file.get<uint32be>();
	if (magic == 'Yaz0')
		return ReadYAZ0(id, file);
	else if (magic == 'RARC')
		return ReadRARC(id, file);
	else if (magic == DARCH_MAGIC)
		return ReadDARCH(id, file);
	return ISO_NULL;
}

ISO_ptr<void> WiiARCFileHandler::ReadYAZ0(tag id, istream_ref file) {
	uint32	size = file.get<uint32be>();

	file.seek(16);
	ISO_ptr<ISO_openarray<uint8> >	p(id, size);

	uint32	code = 0x10000;
	for (uint8 *dst = *p, *end = dst + size; dst < end; code <<= 1) {
		//read new "code" byte if the current one is used up
		if (code & 0x10000)
			code = file.getc() | 0x100;

		if ((code & 0x80) != 0) {
			//straight copy
			*dst++ = file.getc();
		} else {
			//RLE part
			uint8	byte1	= file.getc();
			uint8	byte2	= file.getc();
			uint32	dist	= ((byte1 & 0xF) << 8) + byte2;
			uint8	*copy	= dst - (dist + 1);
			uint32	numBytes = byte1 >> 4;
			if (numBytes == 0)
				numBytes = file.getc() + 0x12;
			else
				numBytes += 2;
			//copy run
			for (int i = 0; i < numBytes; ++i)
				*dst++ = *copy++;
		}
	}
	return p;
}

namespace wii {
struct RARCheader {
	u32			size;				// size of file
	u32			headerSize;
	u32			fstSize;	// where does the actual data start? You have to add 0x20 to this value.
	u32			unknown0[4];
};
struct RARCnode {
	u32			type;
	u32			filenameOffset;		//directory name, offset into string table
	u16			unknown;
	u16			numFileEntries;		//how many files belong to this node?
	u32			firstFileEntryOffset;
};
struct RARfst {
	u32			numNodes;
	u32			unknown1[2];
	u32			fileEntriesOffset;
	u32			stringTableSize;	//unknown2;
	u32			stringTableOffset;	// where is the string table stored? You have to add 0x20 to this value.
	u32			unknown3[2];
	RARCnode	nodes[1];
};
struct RARCentry {
	u16	id;				// file id. If this is 0xFFFF, then this entry is a subdirectory link
	u16	unknown0;
	u16	unknown1;
	u16	filenameOffset;	// file/subdir name, offset into string table
	u32	dataOffset;		// offset to file data (for subdirs: index of Node representing the subdir)
	u32	dataSize;		// size of data
	u32	zero;			// seems to be always '0'
};
}

ISO_ptr<void> WiiARCFileHandler::ReadRARC(tag id, istream_ref file) {
	RARCheader	header	= file.get();
	uint32		dataStart = header.fstSize + 0x20;
	malloc_block	info(file, header.fstSize);

	RARfst		*fst	= (RARfst*)info;
	char		*names	= (char*)info + fst->stringTableOffset;
	RARCentry	*entries= (RARCentry*)((char*)info + fst->fileEntriesOffset);

	ISO_ptr<anything>	t(id);

	pair<RARCnode*, ISO_ptr<anything> >	stack[32], *sp = stack;
	sp->a = fst->nodes;
	sp->b = t;
	sp++;

	while (sp > stack) {
		--sp;
		RARCnode			*node	= sp->a;
		ISO_ptr<anything>	dir		= sp->b;

		int			nentry	= node->numFileEntries;
		int			first	= node->firstFileEntryOffset;
		for (int i = 0; i < nentry; i++) {
			RARCentry	&entry	= entries[first + i];
			const char	*name	= names + uint32(entry.filenameOffset);
			if (entry.id == 0xffff) {
				if (name[0] == '.' && (name[1] == 0 || name[1] == '.'))
					continue;
				ISO_ptr<anything>	t2(name);
				dir->Append(t2);
				sp->a = &node[entry.dataOffset];
				sp->b = t2;
				sp++;
			} else {
				file.seek(dataStart + entry.dataOffset);
				dir->Append(ReadRaw(name, file, entry.dataSize));
			}
		}
	}

	return t;
}

namespace wii {

struct ARCHeader {
//	u32			magic;
	u32			fstStart;
	u32			fstSize;
	u32			fileStart;
	u32			reserve[4];
};

struct FSTEntry {
	u32	isDirAndStringOff;	// the first byte is for isDir the next 3bytes: name offset
	u32	parentOrPosition;	// parent entry (dir entry) position (file entry)
	u32	nextEntryOrLength;	// next entry (dir entry) length (file entry)
};

#define entryIsDir(fstStart, i)		((fstStart[i].isDirAndStringOff & 0xff000000) != 0)
#define stringOff(fstStart, i)		(fstStart[i].isDirAndStringOff & 0x00ffffff)
#define parentDir(fstStart, i)		fstStart[i].parentOrPosition
#define nextDir(fstStart, i)		fstStart[i].nextEntryOrLength
#define filePosition(fstStart, i)	fstStart[i].parentOrPosition
#define fileLength(fstStart, i)		fstStart[i].nextEntryOrLength
}

ISO_ptr<void> WiiARCFileHandler::ReadDARCH(tag id, istream_ref file) {
	ARCHeader	header = file.get();

	file.seek(header.fstStart);
	malloc_block	fst_buffer(file, header.fstSize);
	FSTEntry		*fst	= fst_buffer;

	uint32	num_entries	= nextDir(fst, 0);
	char	*strings	= (char*)(fst + num_entries);

	ISO_ptr<anything>	t(id);

	pair<int, ISO_ptr<anything> >	stack[32], *sp = stack;
	sp->a = 0;
	sp->b = t;
	sp++;

	while (sp > stack) {
		--sp;
		int			index	= sp->a;
		ISO_ptr<anything>	dir		= sp->b;

		for (int i = index + 1, e = nextDir(fst, index); i < e; ) {
			const char	*name	= strings + stringOff(fst, i);
			if (entryIsDir(fst, i)) {
				ISO_ptr<anything>	t2(name);
				dir->Append(t2);
				sp->a = i;
				sp->b = t2;
				sp++;
				i = nextDir(fst, i);
			} else {
				file.seek(filePosition(fst, i));
				dir->Append(ReadRaw(name, file, fileLength(fst, i)));
				i++;
			}
		}
	}

	return t;
}

//-----------------------------------------------------------------------------
//	Wii files
//-----------------------------------------------------------------------------

class WiiASTFileHandler : public FileHandler {
	struct block : array<uint32be, 16>	{};
	const char*		GetExt() override { return "ast"; }
	const char*		GetDescription() override { return "Wii PCM";	}
	ISO_ptr<void>	Read(tag id, istream_ref file) override;
} wii_ast;


ISO_ptr<void> WiiASTFileHandler::Read(tag id, istream_ref file) {
	block	b;

	b = file.get();
	if (b[0] != 'STRM')
		return ISO_NULL;

//	ISO_ptr<ISO_openarray<ISO_ptr<sample> > >	s(id);

	b = file.get();
	if (b[0] != 'BLCK')
		return ISO_NULL;

	ISO_ptr<sample>	s(id);
	s->SetFrequency(41000);
	file.readbuff(s->Create(b[1], 2, 16), b[1] * 4);

	int16	*p = s->Samples();
	for (int n = b[1] * 2; n--; p++)
		*p = *(int16be*)p;
	return s;
}
