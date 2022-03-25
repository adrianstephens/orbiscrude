#ifndef SSL_H
#define SSL_H

#include "ip.h"
#include "base/array.h"
#include "allocators/allocator.h"
#include "x509.h"
#include "extra/random.h"

#define SSL_SIGN_RSA
#define SSL_SIGN_DSA
#define SSL_SIGN_ECDSA
#define SSL_SIGN_FORTEZZA

#define SSL_KEYEX_DH
#define SSL_KEYEX_RSA
#define SSL_KEYEX_ECDH
#define SSL_KEYEX_FORTEZZA
#define SSL_KEYEX_SRP
#define SSL_KEYEX_GOST
//#define SSL_KEYEX_PSK

#define SSL_ENCRYPT_AES
#define SSL_ENCRYPT_DES
#define SSL_ENCRYPT_RC
//#define SSL_ENCRYPT_GOST
//#define SSL_ENCRYPT_CAMELLIA
#define SSL_ENCRYPT_CHACHA

#define SSL_HASH_MD5
#define SSL_HASH_SHA
//#define SSL_HASH_GOST

namespace iso {

struct inc_random {
	uint8	i;
	static const int	BITS = 8;
	int		next()				{ return i++; }
	void	seed(int64 _x)		{}
	inc_random(int64 _x) : i(0) {}
};

//Acronyms
//--------
// DSA	Digital Signature Algorithm
// DSS	a DSA that refers to the NIST standard
// CBC	BulkEncryption Block Chaining
// MAC	Message Authentication Code
// HMAC a MAC which is based on a hash function
// PRF	pseudorandom function: takes as input a secret, a seed, and an identifying label and produces an output of arbitrary length

// GCM	Galois/Counter Mode - a block cipher mode of operation that uses universal hashing over a binary Galois field to provide authenticated encryption
// CCM	Counter with CBC-MAC

//-----------------------------------------------------------------------------
//	packets
//-----------------------------------------------------------------------------

typedef memory_reader			PacketReader;
typedef dynamic_memory_writer	PacketBuilder;

template<int N> struct PacketReaderBuffer : PacketReader {
	uint8			data[N];
	PacketReaderBuffer() : PacketReader(const_memory_block(data, size_t(0))) {}
	const_memory_block	fill(istream_ref sock, size_t size) {
		size_t	total = 0;
		while (total < size) {
			size_t	read = sock.readbuff(data + total, size - total);
			if (read == 0)
				break;
			total += read;
		}
		p		= data;
		return b = const_memory_block(data, total);
	}
};

struct PacketBuilderSSL : PacketBuilder {
	void	flush(ostream_ref sock) {
		sock.writebuff(b, p);
		seek(0);
	}
};

template<typename T> struct patch_write {
	PacketBuilder	&w;
	size_t			offset;
	patch_write(PacketBuilder &w) : w(w), offset(w.alloc_offset(sizeof(T))) {}
	size_t			length()	const { return w.tell() - offset; }
	T				*get()		const { return (T*)w.get_data(offset); }
	operator T*()				const { return get(); }
	memory_block	all()		const { return memory_block(get(), length()); }
};

template<int S> struct chunk_writer : patch_write<uintn<S / 8, true> > {
	chunk_writer(PacketBuilder &w) : patch_write<uintn<S / 8, true> >(w) {}
	~chunk_writer() { *this->get() = this->length() - S / 8; }
};

template<int S> struct chunk_reader : PacketReader::_with_len {
	chunk_reader(PacketReader &r) : PacketReader::_with_len(r, r.get<uintn<S / 8, true> >()) {}
};

template<typename T> bool read(PacketReader &r, optional<T> &t) {
	return !r.remaining() || iso::read(r, put(t));
};

template<typename T> bool write(PacketBuilder &w, const optional<T> &t) {
	return !t.exists() || iso::write(w, get(t));
}


//-----------------------------------------------------------------------------
//	SSL
//-----------------------------------------------------------------------------

namespace SSL {
//-----------------------------------------------------------------------------
//	enums
//-----------------------------------------------------------------------------

typedef PacketReaderBuffer<(1 << 15)>	PacketReaderSSL;

enum Version : uint16 {
	VER_BAD		= 0xffff,
	VER_SSL3	= 0x0300,
	VER_TLS1	= 0x0301,
	VER_TLS1_1	= 0x0302,
	VER_TLS1_2	= 0x0303,
	VER_TLS1_3	= 0x0304,

//	VER_DTLS1	= 0xfeff,
//	VER_DTLS1_2	= 0xfefd,
//	BAD_DTLS1	= 0x0100,

	VER_CURRENT	= VER_TLS1_2,
	VER_MIN		= VER_SSL3,
	VER_MAX		= VER_TLS1_2,
};
//typedef BE(Version) Version2;
typedef compact<BE(Version), 16> Version2;

enum {
	MAX_MD_SIZE			= 64,	// longest known is SHA512
	MAX_KEY_LENGTH		= 64,
	MAX_IV_LENGTH		= 16,
	MAX_BLOCK_LENGTH	= 32,
};

enum Compression : uint8 {
	NO_COMP		= 0,
	DEFLATE		= 1,
	LZS			= 64,
};

enum SignatureAlgorithm : uint8 {
	sig_anonymous	= 0,
	sig_rsa			= 1,
	sig_dsa			= 2,
	sig_ecdsa		= 3,
};

enum HashAlgorithm : uint8 {
	hash_none		= 0,
	hash_md5		= 1,
	hash_sha1		= 2,
	hash_sha224		= 3,
	hash_sha256		= 4,
	hash_sha384		= 5,
	hash_sha512		= 6,
	//extended:
	// only for mac/prf
	hash_gost89, hash_gost89_12, hash_gost94, hash_gost12_256, hash_gost12_512,
	// special
	hash_md5_sha1, hash_md2,
};

enum KeyExchangeAlgorithm {
	xchg_none,
	xchg_rsa,
	xchg_dh,
	xchg_dhe,
	xchg_srp,
	xchg_psk,
	xchg_ecdh,
	xchg_fortezza,
	xchg_gost01,
	xchg_gost12,
};

enum MACAlgorithm {
	MAC_NONE		= hash_none,

	MAC_MD5			= hash_md5,
	MAC_SHA			= hash_sha1,
	MAC_SHA224		= hash_sha224,
	MAC_SHA256		= hash_sha256,
	MAC_SHA384		= hash_sha384,
	MAC_SHA512		= hash_sha512,

	MAC_GOST89		= hash_gost89,
	MAC_GOST89_12	= hash_gost89_12,
	MAC_GOST94		= hash_gost94,
	MAC_GOST12_256	= hash_gost12_256,
	MAC_GOST12_512	= hash_gost12_512,
	MAC_MD5_SHA1	= hash_md5_sha1,

	MAC_AEAD		= 16,
};
constexpr HashAlgorithm	get_hash(MACAlgorithm x)		{ return HashAlgorithm(x & 15); }

enum CertificateType : uint8 {
	rsa_sign						= 1,
	dss_sign						= 2,
	rsa_fixed_dh					= 3,
	dss_fixed_dh					= 4,
	rsa_ephemeral_dh_RESERVED		= 5,
	dss_ephemeral_dh_RESERVED		= 6,
	fortezza_dms_RESERVED			= 20,
	ecdsa_sign						= 64,
	rsa_fixed_ecdh					= 65,
	ecdsa_fixed_ecdh				= 66,
};

#define KX(X,S)	xchg_##X | (sig_##S << 4)
enum KeyExchange {	//	xchg		sig			cert.pub.key	cert.sig	ServerKeyExchange
	KX_NONE			= KX(none,		anonymous	),	//
	KX_RSA			= KX(rsa,		rsa			),	//
//	KX_RSA_EXPORT	= KX(rsa,		rsa			),	//
	KX_DHE_DSS		= KX(dhe,		dsa			),	//
	KX_DHE_RSA		= KX(dhe,		rsa			),	//
	KX_DH_ANON		= KX(dh,		anonymous	),	//
	KX_DH_DSS		= KX(dh,		dsa			),	//
	KX_DH_RSA		= KX(dh,		rsa			),	//
	KX_PSK			= KX(rsa,		anonymous	),	//
	KX_RSA_PSK		= KX(rsa,		anonymous	),	//
	KX_DHE_PSK		= KX(dhe,		anonymous	),	//
	KX_ECDHE_RSA	= KX(ecdh,		rsa			),	//	rsa			ecdsa			yes
	KX_ECDHE_PSK	= KX(ecdh,		anonymous	),	//
	KX_ECDHE_ECDSA	= KX(ecdh,		ecdsa		),	//	ecds-cap	ecdsa			yes
	KX_ECDH_ANON	= KX(ecdh,		anonymous	),	//	none						yes
	KX_ECDH_ECDSA	= KX(ecdh,		ecdsa		),	//	ecdh		ecdsa			no
	KX_ECDH_RSA		= KX(ecdh,		rsa			),	//	ecdh		rsa				no
	KX_SRP			= KX(srp,		anonymous	),	//
	KX_SRP_RSA		= KX(srp,		rsa			),	//
	KX_SRP_DSS		= KX(srp,		anonymous	),	//
	KX_GOST_GOST01	= KX(gost01,	anonymous	),	//
	KX_GOST_GOST12	= KX(gost12,	anonymous	),	//
	KX_FORTEZZA_DMS	= KX(fortezza,	anonymous	),	//
};
#undef KX
constexpr KeyExchangeAlgorithm	get_xchg(KeyExchange x)		{ return KeyExchangeAlgorithm(x & 15); }
constexpr SignatureAlgorithm	get_sig(KeyExchange x)		{ return SignatureAlgorithm(x >> 4); }

enum BulkEncryption {
	NO_ENCRYPT,
	FORTEZZA_CBC,
	IDEA_CBC,
	RC2_CBC_40,
	RC4_40,
	RC4_128,
	DES_40_CBC,
	DES_64_CBC,
	DES_192_EDE_CBC,
	AES_128_CBC,
	AES_256_CBC,
	AES_128_GCM,
	AES_256_GCM,
	AES_128_CCM,
	AES_256_CCM,
	AES_128_CCM_8,
	AES_256_CCM_8,
	CAMELLIA_128_CBC,
	CAMELLIA_256_CBC,
	SEED,
	GOST_89_CNT,
	GOST_89_CNT_12,
	CHACHA_20_POLY_1305,
};

enum CipherSuite : uint16 {
	NULL_WITH_NULL_NULL							= 0x0000,
	RSA_WITH_NULL_MD5							= 0x0001,
	RSA_WITH_NULL_SHA							= 0x0002,
	RSA_EXPORT_WITH_RC4_40_MD5					= 0x0003,  //not DTLS
	RSA_WITH_RC4_128_MD5						= 0x0004,  //not DTLS
	RSA_WITH_RC4_128_SHA						= 0x0005,  //not DTLS
	RSA_EXPORT_WITH_RC2_CBC_40_MD5				= 0x0006,
	RSA_WITH_IDEA_CBC_SHA						= 0x0007,
	RSA_EXPORT_WITH_DES40_CBC_SHA				= 0x0008,
	RSA_WITH_DES_CBC_SHA						= 0x0009,
	RSA_WITH_3DES_EDE_CBC_SHA					= 0x000A,
	DH_DSS_EXPORT_WITH_DES40_CBC_SHA			= 0x000B,
	DH_DSS_WITH_DES_CBC_SHA						= 0x000C,
	DH_DSS_WITH_3DES_EDE_CBC_SHA				= 0x000D,
	DH_RSA_EXPORT_WITH_DES40_CBC_SHA			= 0x000E,
	DH_RSA_WITH_DES_CBC_SHA						= 0x000F,
	DH_RSA_WITH_3DES_EDE_CBC_SHA				= 0x0010,
	DHE_DSS_EXPORT_WITH_DES40_CBC_SHA			= 0x0011,
	DHE_DSS_WITH_DES_CBC_SHA					= 0x0012,
	DHE_DSS_WITH_3DES_EDE_CBC_SHA				= 0x0013,
	DHE_RSA_EXPORT_WITH_DES40_CBC_SHA			= 0x0014,
	DHE_RSA_WITH_DES_CBC_SHA					= 0x0015,
	DHE_RSA_WITH_3DES_EDE_CBC_SHA				= 0x0016,
	DH_anon_EXPORT_WITH_RC4_40_MD5				= 0x0017,  //not DTLS
	DH_anon_WITH_RC4_128_MD5					= 0x0018,  //not DTLS
	DH_anon_EXPORT_WITH_DES40_CBC_SHA			= 0x0019,
	DH_anon_WITH_DES_CBC_SHA					= 0x001A,
	DH_anon_WITH_3DES_EDE_CBC_SHA				= 0x001B,
	KRB5_WITH_DES_CBC_SHA						= 0x001E,
	KRB5_WITH_3DES_EDE_CBC_SHA					= 0x001F,
	KRB5_WITH_RC4_128_SHA						= 0x0020,  //not DTLS
	KRB5_WITH_IDEA_CBC_SHA						= 0x0021,
	KRB5_WITH_DES_CBC_MD5						= 0x0022,
	KRB5_WITH_3DES_EDE_CBC_MD5					= 0x0023,
	KRB5_WITH_RC4_128_MD5						= 0x0024,  //not DTLS
	KRB5_WITH_IDEA_CBC_MD5						= 0x0025,
	KRB5_EXPORT_WITH_DES_CBC_40_SHA				= 0x0026,
	KRB5_EXPORT_WITH_RC2_CBC_40_SHA				= 0x0027,
	KRB5_EXPORT_WITH_RC4_40_SHA					= 0x0028,  //not DTLS
	KRB5_EXPORT_WITH_DES_CBC_40_MD5				= 0x0029,
	KRB5_EXPORT_WITH_RC2_CBC_40_MD5				= 0x002A,
	KRB5_EXPORT_WITH_RC4_40_MD5					= 0x002B,  //not DTLS
	PSK_WITH_NULL_SHA							= 0x002C,
	DHE_PSK_WITH_NULL_SHA						= 0x002D,
	RSA_PSK_WITH_NULL_SHA						= 0x002E,
	RSA_WITH_AES_128_CBC_SHA					= 0x002F,
	DH_DSS_WITH_AES_128_CBC_SHA					= 0x0030,
	DH_RSA_WITH_AES_128_CBC_SHA					= 0x0031,
	DHE_DSS_WITH_AES_128_CBC_SHA				= 0x0032,
	DHE_RSA_WITH_AES_128_CBC_SHA				= 0x0033,
	DH_anon_WITH_AES_128_CBC_SHA				= 0x0034,
	RSA_WITH_AES_256_CBC_SHA					= 0x0035,
	DH_DSS_WITH_AES_256_CBC_SHA					= 0x0036,
	DH_RSA_WITH_AES_256_CBC_SHA					= 0x0037,
	DHE_DSS_WITH_AES_256_CBC_SHA				= 0x0038,
	DHE_RSA_WITH_AES_256_CBC_SHA				= 0x0039,
	DH_anon_WITH_AES_256_CBC_SHA				= 0x003A,
	RSA_WITH_NULL_SHA256						= 0x003B,
	RSA_WITH_AES_128_CBC_SHA256					= 0x003C,
	RSA_WITH_AES_256_CBC_SHA256					= 0x003D,
	DH_DSS_WITH_AES_128_CBC_SHA256				= 0x003E,
	DH_RSA_WITH_AES_128_CBC_SHA256				= 0x003F,
	DHE_DSS_WITH_AES_128_CBC_SHA256				= 0x0040,
	RSA_WITH_CAMELLIA_128_CBC_SHA				= 0x0041,
	DH_DSS_WITH_CAMELLIA_128_CBC_SHA			= 0x0042,
	DH_RSA_WITH_CAMELLIA_128_CBC_SHA			= 0x0043,
	DHE_DSS_WITH_CAMELLIA_128_CBC_SHA			= 0x0044,
	DHE_RSA_WITH_CAMELLIA_128_CBC_SHA			= 0x0045,
	DH_anon_WITH_CAMELLIA_128_CBC_SHA			= 0x0046,
	DHE_RSA_WITH_AES_128_CBC_SHA256				= 0x0067,
	DH_DSS_WITH_AES_256_CBC_SHA256				= 0x0068,
	DH_RSA_WITH_AES_256_CBC_SHA256				= 0x0069,
	DHE_DSS_WITH_AES_256_CBC_SHA256				= 0x006A,
	DHE_RSA_WITH_AES_256_CBC_SHA256				= 0x006B,
	DH_anon_WITH_AES_128_CBC_SHA256				= 0x006C,
	DH_anon_WITH_AES_256_CBC_SHA256				= 0x006D,
	RSA_WITH_CAMELLIA_256_CBC_SHA				= 0x0084,
	DH_DSS_WITH_CAMELLIA_256_CBC_SHA			= 0x0085,
	DH_RSA_WITH_CAMELLIA_256_CBC_SHA			= 0x0086,
	DHE_DSS_WITH_CAMELLIA_256_CBC_SHA			= 0x0087,
	DHE_RSA_WITH_CAMELLIA_256_CBC_SHA			= 0x0088,
	DH_anon_WITH_CAMELLIA_256_CBC_SHA			= 0x0089,
	PSK_WITH_RC4_128_SHA						= 0x008A,  //not DTLS
	PSK_WITH_3DES_EDE_CBC_SHA					= 0x008B,
	PSK_WITH_AES_128_CBC_SHA					= 0x008C,
	PSK_WITH_AES_256_CBC_SHA					= 0x008D,
	DHE_PSK_WITH_RC4_128_SHA					= 0x008E,  //not DTLS
	DHE_PSK_WITH_3DES_EDE_CBC_SHA				= 0x008F,
	DHE_PSK_WITH_AES_128_CBC_SHA				= 0x0090,
	DHE_PSK_WITH_AES_256_CBC_SHA				= 0x0091,
	RSA_PSK_WITH_RC4_128_SHA					= 0x0092,  //not DTLS
	RSA_PSK_WITH_3DES_EDE_CBC_SHA				= 0x0093,
	RSA_PSK_WITH_AES_128_CBC_SHA				= 0x0094,
	RSA_PSK_WITH_AES_256_CBC_SHA				= 0x0095,
	RSA_WITH_SEED_CBC_SHA						= 0x0096,
	DH_DSS_WITH_SEED_CBC_SHA					= 0x0097,
	DH_RSA_WITH_SEED_CBC_SHA					= 0x0098,
	DHE_DSS_WITH_SEED_CBC_SHA					= 0x0099,
	DHE_RSA_WITH_SEED_CBC_SHA					= 0x009A,
	DH_anon_WITH_SEED_CBC_SHA					= 0x009B,
	RSA_WITH_AES_128_GCM_SHA256					= 0x009C,
	RSA_WITH_AES_256_GCM_SHA384					= 0x009D,
	DHE_RSA_WITH_AES_128_GCM_SHA256				= 0x009E,
	DHE_RSA_WITH_AES_256_GCM_SHA384				= 0x009F,
	DH_RSA_WITH_AES_128_GCM_SHA256				= 0x00A0,
	DH_RSA_WITH_AES_256_GCM_SHA384				= 0x00A1,
	DHE_DSS_WITH_AES_128_GCM_SHA256				= 0x00A2,
	DHE_DSS_WITH_AES_256_GCM_SHA384				= 0x00A3,
	DH_DSS_WITH_AES_128_GCM_SHA256				= 0x00A4,
	DH_DSS_WITH_AES_256_GCM_SHA384				= 0x00A5,
	DH_anon_WITH_AES_128_GCM_SHA256				= 0x00A6,
	DH_anon_WITH_AES_256_GCM_SHA384				= 0x00A7,
	PSK_WITH_AES_128_GCM_SHA256					= 0x00A8,
	PSK_WITH_AES_256_GCM_SHA384					= 0x00A9,
	DHE_PSK_WITH_AES_128_GCM_SHA256				= 0x00AA,
	DHE_PSK_WITH_AES_256_GCM_SHA384				= 0x00AB,
	RSA_PSK_WITH_AES_128_GCM_SHA256				= 0x00AC,
	RSA_PSK_WITH_AES_256_GCM_SHA384				= 0x00AD,
	PSK_WITH_AES_128_CBC_SHA256					= 0x00AE,
	PSK_WITH_AES_256_CBC_SHA384					= 0x00AF,
	PSK_WITH_NULL_SHA256						= 0x00B0,
	PSK_WITH_NULL_SHA384						= 0x00B1,
	DHE_PSK_WITH_AES_128_CBC_SHA256				= 0x00B2,
	DHE_PSK_WITH_AES_256_CBC_SHA384				= 0x00B3,
	DHE_PSK_WITH_NULL_SHA256					= 0x00B4,
	DHE_PSK_WITH_NULL_SHA384					= 0x00B5,
	RSA_PSK_WITH_AES_128_CBC_SHA256				= 0x00B6,
	RSA_PSK_WITH_AES_256_CBC_SHA384				= 0x00B7,
	RSA_PSK_WITH_NULL_SHA256					= 0x00B8,
	RSA_PSK_WITH_NULL_SHA384					= 0x00B9,
	RSA_WITH_CAMELLIA_128_CBC_SHA256			= 0x00BA,
	DH_DSS_WITH_CAMELLIA_128_CBC_SHA256			= 0x00BB,
	DH_RSA_WITH_CAMELLIA_128_CBC_SHA256			= 0x00BC,
	DHE_DSS_WITH_CAMELLIA_128_CBC_SHA256		= 0x00BD,
	DHE_RSA_WITH_CAMELLIA_128_CBC_SHA256		= 0x00BE,
	DH_anon_WITH_CAMELLIA_128_CBC_SHA256		= 0x00BF,
	RSA_WITH_CAMELLIA_256_CBC_SHA256			= 0x00C0,
	DH_DSS_WITH_CAMELLIA_256_CBC_SHA256			= 0x00C1,
	DH_RSA_WITH_CAMELLIA_256_CBC_SHA256			= 0x00C2,
	DHE_DSS_WITH_CAMELLIA_256_CBC_SHA256		= 0x00C3,
	DHE_RSA_WITH_CAMELLIA_256_CBC_SHA256		= 0x00C4,
	DH_anon_WITH_CAMELLIA_256_CBC_SHA256		= 0x00C5,
	EMPTY_RENEGOTIATION_INFO_SCSV				= 0x00FF,
	FALLBACK_SCSV								= 0x5600,
	ECDH_ECDSA_WITH_NULL_SHA					= 0xC001,
	ECDH_ECDSA_WITH_RC4_128_SHA					= 0xC002,  //not DTLS
	ECDH_ECDSA_WITH_3DES_EDE_CBC_SHA			= 0xC003,
	ECDH_ECDSA_WITH_AES_128_CBC_SHA				= 0xC004,
	ECDH_ECDSA_WITH_AES_256_CBC_SHA				= 0xC005,
	ECDHE_ECDSA_WITH_NULL_SHA					= 0xC006,
	ECDHE_ECDSA_WITH_RC4_128_SHA				= 0xC007,  //not DTLS
	ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA			= 0xC008,
	ECDHE_ECDSA_WITH_AES_128_CBC_SHA			= 0xC009,
	ECDHE_ECDSA_WITH_AES_256_CBC_SHA			= 0xC00A,
	ECDH_RSA_WITH_NULL_SHA						= 0xC00B,
	ECDH_RSA_WITH_RC4_128_SHA					= 0xC00C,  //not DTLS
	ECDH_RSA_WITH_3DES_EDE_CBC_SHA				= 0xC00D,
	ECDH_RSA_WITH_AES_128_CBC_SHA				= 0xC00E,
	ECDH_RSA_WITH_AES_256_CBC_SHA				= 0xC00F,
	ECDHE_RSA_WITH_NULL_SHA						= 0xC010,
	ECDHE_RSA_WITH_RC4_128_SHA					= 0xC011,  //not DTLS
	ECDHE_RSA_WITH_3DES_EDE_CBC_SHA				= 0xC012,
	ECDHE_RSA_WITH_AES_128_CBC_SHA				= 0xC013,
	ECDHE_RSA_WITH_AES_256_CBC_SHA				= 0xC014,
	ECDH_anon_WITH_NULL_SHA						= 0xC015,
	ECDH_anon_WITH_RC4_128_SHA					= 0xC016,  //not DTLS
	ECDH_anon_WITH_3DES_EDE_CBC_SHA				= 0xC017,
	ECDH_anon_WITH_AES_128_CBC_SHA				= 0xC018,
	ECDH_anon_WITH_AES_256_CBC_SHA				= 0xC019,

	// SRP ciphersuites from RFC 5054
	SRP_SHA_WITH_3DES_EDE_CBC_SHA				= 0xC01A,
	SRP_SHA_RSA_WITH_3DES_EDE_CBC_SHA			= 0xC01B,
	SRP_SHA_DSS_WITH_3DES_EDE_CBC_SHA			= 0xC01C,
	SRP_SHA_WITH_AES_128_CBC_SHA				= 0xC01D,
	SRP_SHA_RSA_WITH_AES_128_CBC_SHA			= 0xC01E,
	SRP_SHA_DSS_WITH_AES_128_CBC_SHA			= 0xC01F,
	SRP_SHA_WITH_AES_256_CBC_SHA				= 0xC020,
	SRP_SHA_RSA_WITH_AES_256_CBC_SHA			= 0xC021,
	SRP_SHA_DSS_WITH_AES_256_CBC_SHA			= 0xC022,

	ECDHE_ECDSA_WITH_AES_128_CBC_SHA256			= 0xC023,
	ECDHE_ECDSA_WITH_AES_256_CBC_SHA384			= 0xC024,
	ECDH_ECDSA_WITH_AES_128_CBC_SHA256			= 0xC025,
	ECDH_ECDSA_WITH_AES_256_CBC_SHA384			= 0xC026,
	ECDHE_RSA_WITH_AES_128_CBC_SHA256			= 0xC027,
	ECDHE_RSA_WITH_AES_256_CBC_SHA384			= 0xC028,
	ECDH_RSA_WITH_AES_128_CBC_SHA256			= 0xC029,
	ECDH_RSA_WITH_AES_256_CBC_SHA384			= 0xC02A,
	ECDHE_ECDSA_WITH_AES_128_GCM_SHA256			= 0xC02B,
	ECDHE_ECDSA_WITH_AES_256_GCM_SHA384			= 0xC02C,
	ECDH_ECDSA_WITH_AES_128_GCM_SHA256			= 0xC02D,
	ECDH_ECDSA_WITH_AES_256_GCM_SHA384			= 0xC02E,
	ECDHE_RSA_WITH_AES_128_GCM_SHA256			= 0xC02F,
	ECDHE_RSA_WITH_AES_256_GCM_SHA384			= 0xC030,
	ECDH_RSA_WITH_AES_128_GCM_SHA256			= 0xC031,
	ECDH_RSA_WITH_AES_256_GCM_SHA384			= 0xC032,
	ECDHE_PSK_WITH_RC4_128_SHA					= 0xC033,  //not DTLS
	ECDHE_PSK_WITH_3DES_EDE_CBC_SHA				= 0xC034,
	ECDHE_PSK_WITH_AES_128_CBC_SHA				= 0xC035,
	ECDHE_PSK_WITH_AES_256_CBC_SHA				= 0xC036,
	ECDHE_PSK_WITH_AES_128_CBC_SHA256			= 0xC037,
	ECDHE_PSK_WITH_AES_256_CBC_SHA384			= 0xC038,
	ECDHE_PSK_WITH_NULL_SHA						= 0xC039,
	ECDHE_PSK_WITH_NULL_SHA256					= 0xC03A,
	ECDHE_PSK_WITH_NULL_SHA384					= 0xC03B,
	RSA_WITH_ARIA_128_CBC_SHA256				= 0xC03C,
	RSA_WITH_ARIA_256_CBC_SHA384				= 0xC03D,
	DH_DSS_WITH_ARIA_128_CBC_SHA256				= 0xC03E,
	DH_DSS_WITH_ARIA_256_CBC_SHA384				= 0xC03F,
	DH_RSA_WITH_ARIA_128_CBC_SHA256				= 0xC040,
	DH_RSA_WITH_ARIA_256_CBC_SHA384				= 0xC041,
	DHE_DSS_WITH_ARIA_128_CBC_SHA256			= 0xC042,
	DHE_DSS_WITH_ARIA_256_CBC_SHA384			= 0xC043,
	DHE_RSA_WITH_ARIA_128_CBC_SHA256			= 0xC044,
	DHE_RSA_WITH_ARIA_256_CBC_SHA384			= 0xC045,
	DH_anon_WITH_ARIA_128_CBC_SHA256			= 0xC046,
	DH_anon_WITH_ARIA_256_CBC_SHA384			= 0xC047,
	ECDHE_ECDSA_WITH_ARIA_128_CBC_SHA256		= 0xC048,
	ECDHE_ECDSA_WITH_ARIA_256_CBC_SHA384		= 0xC049,
	ECDH_ECDSA_WITH_ARIA_128_CBC_SHA256			= 0xC04A,
	ECDH_ECDSA_WITH_ARIA_256_CBC_SHA384			= 0xC04B,
	ECDHE_RSA_WITH_ARIA_128_CBC_SHA256			= 0xC04C,
	ECDHE_RSA_WITH_ARIA_256_CBC_SHA384			= 0xC04D,
	ECDH_RSA_WITH_ARIA_128_CBC_SHA256			= 0xC04E,
	ECDH_RSA_WITH_ARIA_256_CBC_SHA384			= 0xC04F,
	RSA_WITH_ARIA_128_GCM_SHA256				= 0xC050,
	RSA_WITH_ARIA_256_GCM_SHA384				= 0xC051,
	DHE_RSA_WITH_ARIA_128_GCM_SHA256			= 0xC052,
	DHE_RSA_WITH_ARIA_256_GCM_SHA384			= 0xC053,
	DH_RSA_WITH_ARIA_128_GCM_SHA256				= 0xC054,
	DH_RSA_WITH_ARIA_256_GCM_SHA384				= 0xC055,
	DHE_DSS_WITH_ARIA_128_GCM_SHA256			= 0xC056,
	DHE_DSS_WITH_ARIA_256_GCM_SHA384			= 0xC057,
	DH_DSS_WITH_ARIA_128_GCM_SHA256				= 0xC058,
	DH_DSS_WITH_ARIA_256_GCM_SHA384				= 0xC059,
	DH_anon_WITH_ARIA_128_GCM_SHA256			= 0xC05A,
	DH_anon_WITH_ARIA_256_GCM_SHA384			= 0xC05B,
	ECDHE_ECDSA_WITH_ARIA_128_GCM_SHA256		= 0xC05C,
	ECDHE_ECDSA_WITH_ARIA_256_GCM_SHA384		= 0xC05D,
	ECDH_ECDSA_WITH_ARIA_128_GCM_SHA256			= 0xC05E,
	ECDH_ECDSA_WITH_ARIA_256_GCM_SHA384			= 0xC05F,
	ECDHE_RSA_WITH_ARIA_128_GCM_SHA256			= 0xC060,
	ECDHE_RSA_WITH_ARIA_256_GCM_SHA384			= 0xC061,
	ECDH_RSA_WITH_ARIA_128_GCM_SHA256			= 0xC062,
	ECDH_RSA_WITH_ARIA_256_GCM_SHA384			= 0xC063,
	PSK_WITH_ARIA_128_CBC_SHA256				= 0xC064,
	PSK_WITH_ARIA_256_CBC_SHA384				= 0xC065,
	DHE_PSK_WITH_ARIA_128_CBC_SHA256			= 0xC066,
	DHE_PSK_WITH_ARIA_256_CBC_SHA384			= 0xC067,
	RSA_PSK_WITH_ARIA_128_CBC_SHA256			= 0xC068,
	RSA_PSK_WITH_ARIA_256_CBC_SHA384			= 0xC069,
	PSK_WITH_ARIA_128_GCM_SHA256				= 0xC06A,
	PSK_WITH_ARIA_256_GCM_SHA384				= 0xC06B,
	DHE_PSK_WITH_ARIA_128_GCM_SHA256			= 0xC06C,
	DHE_PSK_WITH_ARIA_256_GCM_SHA384			= 0xC06D,
	RSA_PSK_WITH_ARIA_128_GCM_SHA256			= 0xC06E,
	RSA_PSK_WITH_ARIA_256_GCM_SHA384			= 0xC06F,
	ECDHE_PSK_WITH_ARIA_128_CBC_SHA256			= 0xC070,
	ECDHE_PSK_WITH_ARIA_256_CBC_SHA384			= 0xC071,
	ECDHE_ECDSA_WITH_CAMELLIA_128_CBC_SHA256	= 0xC072,
	ECDHE_ECDSA_WITH_CAMELLIA_256_CBC_SHA384	= 0xC073,
	ECDH_ECDSA_WITH_CAMELLIA_128_CBC_SHA256		= 0xC074,
	ECDH_ECDSA_WITH_CAMELLIA_256_CBC_SHA384		= 0xC075,
	ECDHE_RSA_WITH_CAMELLIA_128_CBC_SHA256		= 0xC076,
	ECDHE_RSA_WITH_CAMELLIA_256_CBC_SHA384		= 0xC077,
	ECDH_RSA_WITH_CAMELLIA_128_CBC_SHA256		= 0xC078,
	ECDH_RSA_WITH_CAMELLIA_256_CBC_SHA384		= 0xC079,
	RSA_WITH_CAMELLIA_128_GCM_SHA256			= 0xC07A,
	RSA_WITH_CAMELLIA_256_GCM_SHA384			= 0xC07B,
	DHE_RSA_WITH_CAMELLIA_128_GCM_SHA256		= 0xC07C,
	DHE_RSA_WITH_CAMELLIA_256_GCM_SHA384		= 0xC07D,
	DH_RSA_WITH_CAMELLIA_128_GCM_SHA256			= 0xC07E,
	DH_RSA_WITH_CAMELLIA_256_GCM_SHA384			= 0xC07F,
	DHE_DSS_WITH_CAMELLIA_128_GCM_SHA256		= 0xC080,
	DHE_DSS_WITH_CAMELLIA_256_GCM_SHA384		= 0xC081,
	DH_DSS_WITH_CAMELLIA_128_GCM_SHA256			= 0xC082,
	DH_DSS_WITH_CAMELLIA_256_GCM_SHA384			= 0xC083,
	DH_anon_WITH_CAMELLIA_128_GCM_SHA256		= 0xC084,
	DH_anon_WITH_CAMELLIA_256_GCM_SHA384		= 0xC085,
	ECDHE_ECDSA_WITH_CAMELLIA_128_GCM_SHA256	= 0xC086,
	ECDHE_ECDSA_WITH_CAMELLIA_256_GCM_SHA384	= 0xC087,
	ECDH_ECDSA_WITH_CAMELLIA_128_GCM_SHA256		= 0xC088,
	ECDH_ECDSA_WITH_CAMELLIA_256_GCM_SHA384		= 0xC089,
	ECDHE_RSA_WITH_CAMELLIA_128_GCM_SHA256		= 0xC08A,
	ECDHE_RSA_WITH_CAMELLIA_256_GCM_SHA384		= 0xC08B,
	ECDH_RSA_WITH_CAMELLIA_128_GCM_SHA256		= 0xC08C,
	ECDH_RSA_WITH_CAMELLIA_256_GCM_SHA384		= 0xC08D,
	PSK_WITH_CAMELLIA_128_GCM_SHA256			= 0xC08E,
	PSK_WITH_CAMELLIA_256_GCM_SHA384			= 0xC08F,
	DHE_PSK_WITH_CAMELLIA_128_GCM_SHA256		= 0xC090,
	DHE_PSK_WITH_CAMELLIA_256_GCM_SHA384		= 0xC091,
	RSA_PSK_WITH_CAMELLIA_128_GCM_SHA256		= 0xC092,
	RSA_PSK_WITH_CAMELLIA_256_GCM_SHA384		= 0xC093,
	PSK_WITH_CAMELLIA_128_CBC_SHA256			= 0xC094,
	PSK_WITH_CAMELLIA_256_CBC_SHA384			= 0xC095,
	DHE_PSK_WITH_CAMELLIA_128_CBC_SHA256		= 0xC096,
	DHE_PSK_WITH_CAMELLIA_256_CBC_SHA384		= 0xC097,
	RSA_PSK_WITH_CAMELLIA_128_CBC_SHA256		= 0xC098,
	RSA_PSK_WITH_CAMELLIA_256_CBC_SHA384		= 0xC099,
	ECDHE_PSK_WITH_CAMELLIA_128_CBC_SHA256		= 0xC09A,
	ECDHE_PSK_WITH_CAMELLIA_256_CBC_SHA384		= 0xC09B,
	RSA_WITH_AES_128_CCM						= 0xC09C,
	RSA_WITH_AES_256_CCM						= 0xC09D,
	DHE_RSA_WITH_AES_128_CCM					= 0xC09E,
	DHE_RSA_WITH_AES_256_CCM					= 0xC09F,
	RSA_WITH_AES_128_CCM_8						= 0xC0A0,
	RSA_WITH_AES_256_CCM_8						= 0xC0A1,
	DHE_RSA_WITH_AES_128_CCM_8					= 0xC0A2,
	DHE_RSA_WITH_AES_256_CCM_8					= 0xC0A3,
	PSK_WITH_AES_128_CCM						= 0xC0A4,
	PSK_WITH_AES_256_CCM						= 0xC0A5,
	DHE_PSK_WITH_AES_128_CCM					= 0xC0A6,
	DHE_PSK_WITH_AES_256_CCM					= 0xC0A7,
	PSK_WITH_AES_128_CCM_8						= 0xC0A8,
	PSK_WITH_AES_256_CCM_8						= 0xC0A9,
	PSK_DHE_WITH_AES_128_CCM_8					= 0xC0AA,
	PSK_DHE_WITH_AES_256_CCM_8					= 0xC0AB,
	ECDHE_ECDSA_WITH_AES_128_CCM				= 0xC0AC,
	ECDHE_ECDSA_WITH_AES_256_CCM				= 0xC0AD,
	ECDHE_ECDSA_WITH_AES_128_CCM_8				= 0xC0AE,
	ECDHE_ECDSA_WITH_AES_256_CCM_8				= 0xC0AF,


	ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256		= 0xCCA8,
	ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256	= 0xCCA9,
	DHE_RSA_WITH_CHACHA20_POLY1305_SHA256		= 0xCCAA,
	PSK_WITH_CHACHA20_POLY1305_SHA256			= 0xCCAB,
	ECDHE_PSK_WITH_CHACHA20_POLY1305_SHA256		= 0xCCAC,
	DHE_PSK_WITH_CHACHA20_POLY1305_SHA256		= 0xCCAD,
	RSA_PSK_WITH_CHACHA20_POLY1305_SHA256		= 0xCCAE,

	// draft-ietf-tls-chacha20-poly1305-03
	ECDHE_RSA_WITH_CHACHA20_POLY1305			= 0xCCA8,
	ECDHE_ECDSA_WITH_CHACHA20_POLY1305			= 0xCCA9,
	DHE_RSA_WITH_CHACHA20_POLY1305				= 0xCCAA,
	PSK_WITH_CHACHA20_POLY1305					= 0xCCAB,
	ECDHE_PSK_WITH_CHACHA20_POLY1305			= 0xCCAC,
	DHE_PSK_WITH_CHACHA20_POLY1305				= 0xCCAD,
	RSA_PSK_WITH_CHACHA20_POLY1305				= 0xCCAE,

	// TLS v1.3 ciphersuites
	AES_128_GCM_SHA256							= 0x1301,
	AES_256_GCM_SHA384							= 0x1302,
	CHACHA20_POLY1305_SHA256					= 0x1303,
	ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256_OLD	= 0xcc13,
	ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256_OLD= 0xcc14,
	CECPQ1_RSA_WITH_CHACHA20_POLY1305_SHA256	= 0x16b7,
	CECPQ1_ECDSA_WITH_CHACHA20_POLY1305_SHA256	= 0x16b8,
	CECPQ1_RSA_WITH_AES_256_GCM_SHA384			= 0x16b9,
	CECPQ1_ECDSA_WITH_AES_256_GCM_SHA384		= 0x16ba,

	GOST2001_GOST89_GOST89						= 0x0081,
	GOST2001_NULL_GOST94						= 0x0083,
	GOST2012_GOST8912_GOST8912					= 0xff85,
	GOST2012_NULL_GOST12						= 0xff87,

	//obsolete
	RSA_EXPORT_WITH_DES_40_CBC_SHA				= 0x0008,
	RSA_WITH_DES_64_CBC_SHA						= 0x0009,

	DH_DSS_EXPORT_WITH_DES_40_CBC_SHA			= 0x000B,
	DH_DSS_WITH_DES_64_CBC_SHA					= 0x000C,
	DH_RSA_EXPORT_WITH_DES_40_CBC_SHA			= 0x000E,
	DH_RSA_WITH_DES_64_CBC_SHA					= 0x000F,
	DHE_DSS_EXPORT_WITH_DES_40_CBC_SHA			= 0x0011,
	DHE_DSS_WITH_DES_64_CBC_SHA					= 0x0012,
	DHE_RSA_EXPORT_WITH_DES_40_CBC_SHA			= 0x0014,
	DHE_RSA_WITH_DES_64_CBC_SHA					= 0x0015,

	FORTEZZA_DMS_WITH_NULL_SHA					= 0x001C,
	FORTEZZA_DMS_WITH_FORTEZZA_CBC_SHA			= 0x001D,
};

enum UserMapping : uint8 {
	upn_domain_hint = 64
};

//-----------------------------------------------------------------------------
//	basic types
//-----------------------------------------------------------------------------

typedef	uint8			opaque;

template<typename T> malloc_block serialise(const T &t) {
	PacketBuilder	w;
	w.write(t);
	return w;
}

template<int S> struct opaque_array : malloc_block {
	typedef uintn<S / 8, true> ST;

	opaque_array()								{}
	opaque_array(size_t n)						: malloc_block(n) {}
	opaque_array(const const_memory_block &w)	: malloc_block(w) {}
	opaque_array(param(mpi) t)					: malloc_block((t.num_bits() + 7) / 8) { t.save(*this); }
	opaque_array(malloc_block &&t)				: malloc_block(move(t)) {}

	void	operator=(malloc_block &t)	{
		malloc_block::operator=(t);
	}
	bool write(PacketBuilder &w) const {
		w.write(ST(size32()));
		w.writebuff(begin(), size32());
		return true;
	}

	bool read(PacketReader &r) {
		ST	len;
		r.read(len);
		create(len);
		r.readbuff(begin(), len);
		return true;
	}
};

typedef opaque_array<8>			SessionID;
typedef array<opaque, 12>		VerificationData;

struct TimeRandom {
	uint32be	gmt_unix_time;
	//int32		random[7];
	uint8		random[28];
};

struct SignatureAndHashAlgorithm {
	HashAlgorithm		hash;
	SignatureAlgorithm	sig;
	bool operator==(const SignatureAndHashAlgorithm &b) const { return hash == b.hash && sig == b.sig; }
};

//-----------------------------------------------------------------------------
//	SSL interface
//-----------------------------------------------------------------------------

struct CipherSuiteInfo;
struct Record;

struct Connection {
	Version				version;
	SessionID			session_id;
	CipherSuiteInfo*	cipher_info;
	Compression			compression:8;
	MACAlgorithm		mac_algorithm;
	HashAlgorithm		prf_hash;

	bool				local_encrypted;
	bool				peer_encrypted;

	uint8				enc_key_length;
	uint8				block_length;
	uint8				fixed_iv_length;
	uint8				record_iv_length;
	uint8				mac_key_length;
	uint8				auth_tag_length;

	malloc_block		pre_master_secret;
	array<opaque, 48>	master_secret;
	union {
		struct {
			TimeRandom	client_random;
			TimeRandom	server_random;
		};
		TimeRandom	both_random[2];
	};

	//generated
	opaque				key_block_data[(MAX_MD_SIZE + MAX_KEY_LENGTH + MAX_IV_LENGTH) * 2];
	memory_block		write_MAC;
	memory_block		read_MAC;
	memory_block		write_key;
	memory_block		read_key;
	memory_block		write_iv;
	memory_block		read_iv;

	uint64				read_sequence_number;
	uint64				write_sequence_number;

//	rng<jsr_random>		rand;
	rng<inc_random>		rand;
	string				server_name;

	dynamic_array<CipherSuite>					cipher_suites;
	dynamic_array<SignatureAndHashAlgorithm>	sig_hash;
//	dynamic_array<X509::Certificate>			trusted_ca;
//	dynamic_array<CertificateDesc>				certificates;

	void		set_cipher(CipherSuiteInfo *_cipher_info, bool server, bool extended_master_secret, const const_memory_block &handshake_messages);
	void		set_peer_encryption();
	VerificationData calculate_verification(bool server, const const_memory_block &handshake_messages);

public:
	Connection() : cipher_info(0), local_encrypted(false), peer_encrypted(false), record_iv_length(0), auth_tag_length(0) {}
	bool		ClientConnect(iostream_ref io);//istream_ref in, ostream_ref out);
	bool		ServerConnect(iostream_ref io);//istream_ref in, ostream_ref out);
	streamptr	read_record(Record &r, PacketReaderSSL &rr, istream_ref in);
	void		write_record(patch_write<Record> &patch);
};


struct SSL_input {
	PacketReaderSSL	pr;
	streamptr		data_end;
	streamptr		record_end;

	SSL_input() : data_end(0), record_end(0) {}
	size_t	readbuff(Connection &con, istream_ref stream, void *buffer, size_t size);
};

struct SSL_istream : public reader_mixin<SSL_istream>, SSL_input {
	Connection			&con;
	istream_ref			stream;
	SSL_istream(Connection &con, istream_ref stream) : con(con), stream(stream) {}
	size_t	readbuff(void *buffer, size_t size) { return SSL_input::readbuff(con, stream, buffer, size); }
};

struct SSL_output {
	PacketBuilderSSL	rb;
	SSL_output() {}
	void	flush(ostream_ref stream)	{ rb.flush(stream); }
	size_t	writebuff(Connection &con, ostream_ref stream, const void *buffer, size_t size);
};

struct SSL_ostream : public writer_mixin<SSL_ostream>, SSL_output {
	Connection			&con;
	ostream_ref			stream;
	SSL_ostream(Connection &con, ostream_ref stream) : con(con), stream(stream) {}
	~SSL_ostream()	{ flush(stream); }
	size_t	writebuff(const void *buffer, size_t size) { return SSL_output::writebuff(con, stream, buffer, size); }
};

struct SSL_iostream : public readwriter_mixin<SSL_iostream>, SSL_input, SSL_output {
	Connection			&con;
	iostream_ref		stream;
	SSL_iostream(Connection &con, iostream_ref stream) : con(con), stream(stream) {}
	~SSL_iostream()	{ flush(stream); }
	size_t	readbuff(void *buffer, size_t size)			{ return SSL_input::readbuff(con, stream, buffer, size); }
	size_t	writebuff(const void *buffer, size_t size)	{ return SSL_output::writebuff(con, stream, buffer, size); }
};


} // namespace SSL

} // namespace iso
#endif //SSL_H
