#include "ssl.h"
#include "asn1.h"
#include "codec/base64.h"
#include "base/algorithm.h"
#include "extra/date.h"

// key exchange algorithms
#if defined SSL_KEYEX_RSA || defined SSL_SIGN_RSA
#include "rsa.h"
#endif
#ifdef SSL_SIGN_DSA
#include "dsa.h"
#endif
#if defined SSL_SIGN_ECDSA || defined SSL_KEYEX_ECDH 
#include "elliptic-curves.h"
#endif
#ifdef SSL_KEYEX_DH
#include "diffie-hellman.h"
#endif
#ifdef SSL_KEYEX_SRP
#include "srp.h"
#endif

// hashes
#ifdef SSL_HASH_MD5
#include "hashes/md5.h"
#endif
#ifdef SSL_HASH_SHA
#include "hashes/SHA.h"
#endif

// encryption
#ifdef SSL_ENCRYPT_AES
#include "codec/aes.h"
#endif
#ifdef SSL_ENCRYPT_DES
#include "codec/des.h"
#endif
#ifdef SSL_ENCRYPT_RC
#include "codec/rc.h"
#endif
#if defined(SSL_ENCRYPT_GOST) || defined(SSL_HASH_GOST) || defined(SSL_KEYEX_GOST)
#include "codec/gost.h"
#endif
#ifdef SSL_ENCRYPT_CAMELLIA
#include "codec/camellia.h"
#endif
#ifdef SSL_ENCRYPT_CHACHA
#include "codec/chacha.h"
#endif

// modes
#include "codec/cbc.h"
#include "codec/ccm.h"
#include "codec/gcm.h"
#include "codec/ctr.h"


namespace iso { namespace SSL {

typedef X509::DistinguishedName	CertificateAuthority;

//GREASE: Generate Random Extensions And Sustain Extensibility

//The following values are reserved as GREASE values for cipher suites and Application-Layer Protocol Negotiation (ALPN) [RFC7301], extensions, named groups, signature algorithms, and versions:
uint16 grease16[] = {
	0x0A0A,
	0x1A1A,
	0x2A2A,
	0x3A3A,
	0x4A4A,
	0x5A5A,
	0x6A6A,
	0x7A7A,
	0x8A8A,
	0x9A9A,
	0xAAAA,
	0xBABA,
	0xCACA,
	0xDADA,
	0xEAEA,
	0xFAFA,
};
//The following values are reserved as GREASE values for PskKeyExchangeModes:
uint8 grease8[] = {
	0x0B,
	0x2A,
	0x49,
	0x68,
	0x87,
	0xA6,
	0xC5,
	0xE4,
};

#if 1
//-----------------------------------------------------------------------------
//	CipherSuite
//-----------------------------------------------------------------------------

struct CipherSuiteInfo {
	CipherSuite			suite;
	KeyExchange			key_exchange;
	BulkEncryption		bulk_encryption;
	MACAlgorithm		mac_algorithm;
	HashAlgorithm		prf_hash;
	uint8				flags;					// strength and export flags
	uint16				min_tls, max_tls;		// min/max SSL/TLS protocol version
	uint16				min_dtls, max_dtls;		// min/max TLS protocol version
	bool	operator==(uint16 i) const { return suite == i; }
};

enum Flags {
	STRONG_MASK		= 0x1f,
	LOW				= 0x01,
	MEDIUM			= 0x02,
	HIGH			= 0x03,
	FIPS			= 0x10,
	NOT_DEFAULT		= 0x20,
	STREAM_MAC		= 0x40,		//Stream MAC for GOST ciphersuites from cryptopro draft
};
CipherSuiteInfo ciphers[] = {
//		suite										key_exchange	bulk_encryption			mac_algorithm	prf_hash		flags							min_tls		max_tls			min_dtls	max_dtls
	{	NULL_WITH_NULL_NULL,						KX_NONE,		NO_ENCRYPT,				MAC_NONE,		hash_none,		0,								VER_SSL3,	VER_TLS1_2,		VER_BAD,	VER_TLS1_2,		},
	{	RSA_WITH_NULL_MD5,							KX_RSA,			NO_ENCRYPT,				MAC_MD5,		hash_md5_sha1,	0,								VER_SSL3,	VER_TLS1_2,		VER_BAD,	VER_TLS1_2,		},
	{	RSA_WITH_NULL_SHA,							KX_RSA,			NO_ENCRYPT,				MAC_SHA,		hash_md5_sha1,	FIPS,							VER_SSL3,	VER_TLS1_2,		VER_BAD,	VER_TLS1_2,		},
	{	RSA_WITH_3DES_EDE_CBC_SHA,					KX_RSA,			DES_192_EDE_CBC,		MAC_SHA,		hash_md5_sha1,	NOT_DEFAULT	| MEDIUM | FIPS,	VER_SSL3,	VER_TLS1_2,		VER_BAD,	VER_TLS1_2,		},
	{	DHE_DSS_WITH_3DES_EDE_CBC_SHA,				KX_DHE_DSS,		DES_192_EDE_CBC,		MAC_SHA,		hash_md5_sha1,	NOT_DEFAULT	| MEDIUM | FIPS,	VER_SSL3,	VER_TLS1_2,		VER_BAD,	VER_TLS1_2,		},
	{	DHE_RSA_WITH_3DES_EDE_CBC_SHA,				KX_DHE_RSA,		DES_192_EDE_CBC,		MAC_SHA,		hash_md5_sha1,	NOT_DEFAULT	| MEDIUM | FIPS,	VER_SSL3,	VER_TLS1_2,		VER_BAD,	VER_TLS1_2,		},
	{	DH_anon_WITH_3DES_EDE_CBC_SHA,				KX_DH_ANON,		DES_192_EDE_CBC,		MAC_SHA,		hash_md5_sha1,	NOT_DEFAULT	| MEDIUM | FIPS,	VER_SSL3,	VER_TLS1_2,		VER_BAD,	VER_TLS1_2,		},
	{	RSA_WITH_AES_128_CBC_SHA,					KX_RSA,			AES_128_CBC,			MAC_SHA,		hash_md5_sha1,	HIGH | FIPS,					VER_SSL3,	VER_TLS1_2,		VER_BAD,	VER_TLS1_2,		},
	{	DHE_DSS_WITH_AES_128_CBC_SHA,				KX_DHE_DSS,		AES_128_CBC,			MAC_SHA,		hash_md5_sha1,	NOT_DEFAULT	| HIGH | FIPS,		VER_SSL3,	VER_TLS1_2,		VER_BAD,	VER_TLS1_2,		},
	{	DHE_RSA_WITH_AES_128_CBC_SHA,				KX_DHE_RSA,		AES_128_CBC,			MAC_SHA,		hash_md5_sha1,	HIGH | FIPS,					VER_SSL3,	VER_TLS1_2,		VER_BAD,	VER_TLS1_2,		},
	{	DH_anon_WITH_AES_128_CBC_SHA,				KX_DH_ANON,		AES_128_CBC,			MAC_SHA,		hash_md5_sha1,	NOT_DEFAULT	| HIGH | FIPS,		VER_SSL3,	VER_TLS1_2,		VER_BAD,	VER_TLS1_2,		},
	{	RSA_WITH_AES_256_CBC_SHA,					KX_RSA,			AES_256_CBC,			MAC_SHA,		hash_md5_sha1,	HIGH | FIPS,					VER_SSL3,	VER_TLS1_2,		VER_BAD,	VER_TLS1_2,		},
	{	DHE_DSS_WITH_AES_256_CBC_SHA,				KX_DHE_DSS,		AES_256_CBC,			MAC_SHA,		hash_md5_sha1,	NOT_DEFAULT	| HIGH | FIPS,		VER_SSL3,	VER_TLS1_2,		VER_BAD,	VER_TLS1_2,		},
	{	DHE_RSA_WITH_AES_256_CBC_SHA,				KX_DHE_RSA,		AES_256_CBC,			MAC_SHA,		hash_md5_sha1,	HIGH | FIPS,					VER_SSL3,	VER_TLS1_2,		VER_BAD,	VER_TLS1_2,		},
	{	DH_anon_WITH_AES_256_CBC_SHA,				KX_DH_ANON,		AES_256_CBC,			MAC_SHA,		hash_md5_sha1,	NOT_DEFAULT	| HIGH | FIPS,		VER_SSL3,	VER_TLS1_2,		VER_BAD,	VER_TLS1_2,		},
	{	RSA_WITH_NULL_SHA256,						KX_RSA,			NO_ENCRYPT,				MAC_SHA256,		hash_md5_sha1,	FIPS,							VER_TLS1_2,	VER_TLS1_2,		VER_TLS1_2,	VER_TLS1_2,		},
	{	RSA_WITH_AES_128_CBC_SHA256,				KX_RSA,			AES_128_CBC,			MAC_SHA256,		hash_md5_sha1,	HIGH | FIPS,					VER_TLS1_2,	VER_TLS1_2,		VER_TLS1_2,	VER_TLS1_2,		},
	{	RSA_WITH_AES_256_CBC_SHA256,				KX_RSA,			AES_256_CBC,			MAC_SHA256,		hash_md5_sha1,	HIGH | FIPS,					VER_TLS1_2,	VER_TLS1_2,		VER_TLS1_2,	VER_TLS1_2,		},
	{	DHE_DSS_WITH_AES_128_CBC_SHA256,			KX_DHE_DSS,		AES_128_CBC,			MAC_SHA256,		hash_md5_sha1,	NOT_DEFAULT	| HIGH | FIPS,		VER_TLS1_2,	VER_TLS1_2,		VER_TLS1_2,	VER_TLS1_2,		},
	{	DHE_RSA_WITH_AES_128_CBC_SHA256,			KX_DHE_RSA,		AES_128_CBC,			MAC_SHA256,		hash_md5_sha1,	HIGH | FIPS,					VER_TLS1_2,	VER_TLS1_2,		VER_TLS1_2,	VER_TLS1_2,		},
	{	DHE_DSS_WITH_AES_256_CBC_SHA256,			KX_DHE_DSS,		AES_256_CBC,			MAC_SHA256,		hash_md5_sha1,	NOT_DEFAULT	| HIGH | FIPS,		VER_TLS1_2,	VER_TLS1_2,		VER_TLS1_2,	VER_TLS1_2,		},
	{	DHE_RSA_WITH_AES_256_CBC_SHA256,			KX_DHE_RSA,		AES_256_CBC,			MAC_SHA256,		hash_md5_sha1,	HIGH | FIPS,					VER_TLS1_2,	VER_TLS1_2,		VER_TLS1_2,	VER_TLS1_2,		},
	{	DH_anon_WITH_AES_128_CBC_SHA256,			KX_DH_ANON,		AES_128_CBC,			MAC_SHA256,		hash_md5_sha1,	NOT_DEFAULT	| HIGH | FIPS,		VER_TLS1_2,	VER_TLS1_2,		VER_TLS1_2,	VER_TLS1_2,		},
	{	DH_anon_WITH_AES_256_CBC_SHA256,			KX_DH_ANON,		AES_256_CBC,			MAC_SHA256,		hash_md5_sha1,	NOT_DEFAULT	| HIGH | FIPS,		VER_TLS1_2,	VER_TLS1_2,		VER_TLS1_2,	VER_TLS1_2,		},
	{	RSA_WITH_AES_128_GCM_SHA256,				KX_RSA,			AES_128_GCM,			MAC_AEAD,		hash_sha256,	HIGH | FIPS,					VER_TLS1_2,	VER_TLS1_2,		VER_TLS1_2,	VER_TLS1_2,		},
	{	RSA_WITH_AES_256_GCM_SHA384,				KX_RSA,			AES_256_GCM,			MAC_AEAD,		hash_sha384,	HIGH | FIPS,					VER_TLS1_2,	VER_TLS1_2,		VER_TLS1_2,	VER_TLS1_2,		},
	{	DHE_RSA_WITH_AES_128_GCM_SHA256,			KX_DHE_RSA,		AES_128_GCM,			MAC_AEAD,		hash_sha256,	HIGH | FIPS,					VER_TLS1_2,	VER_TLS1_2,		VER_TLS1_2,	VER_TLS1_2,		},
	{	DHE_RSA_WITH_AES_256_GCM_SHA384,			KX_DHE_RSA,		AES_256_GCM,			MAC_AEAD,		hash_sha384,	HIGH | FIPS,					VER_TLS1_2,	VER_TLS1_2,		VER_TLS1_2,	VER_TLS1_2,		},
	{	DHE_DSS_WITH_AES_128_GCM_SHA256,			KX_DHE_DSS,		AES_128_GCM,			MAC_AEAD,		hash_sha256,	NOT_DEFAULT	| HIGH | FIPS,		VER_TLS1_2,	VER_TLS1_2,		VER_TLS1_2,	VER_TLS1_2,		},
	{	DHE_DSS_WITH_AES_256_GCM_SHA384,			KX_DHE_DSS,		AES_256_GCM,			MAC_AEAD,		hash_sha384,	NOT_DEFAULT	| HIGH | FIPS,		VER_TLS1_2,	VER_TLS1_2,		VER_TLS1_2,	VER_TLS1_2,		},
	{	DH_anon_WITH_AES_128_GCM_SHA256,			KX_DH_ANON,		AES_128_GCM,			MAC_AEAD,		hash_sha256,	NOT_DEFAULT	| HIGH | FIPS,		VER_TLS1_2,	VER_TLS1_2,		VER_TLS1_2,	VER_TLS1_2,		},
	{	DH_anon_WITH_AES_256_GCM_SHA384,			KX_DH_ANON,		AES_256_GCM,			MAC_AEAD,		hash_sha384,	NOT_DEFAULT	| HIGH | FIPS,		VER_TLS1_2,	VER_TLS1_2,		VER_TLS1_2,	VER_TLS1_2,		},
	{	RSA_WITH_AES_128_CCM,						KX_RSA,			AES_128_CCM,			MAC_AEAD,		hash_sha256,	NOT_DEFAULT	| HIGH,				VER_TLS1_2,	VER_TLS1_2,		VER_TLS1_2,	VER_TLS1_2,		},
	{	RSA_WITH_AES_256_CCM,						KX_RSA,			AES_256_CCM,			MAC_AEAD,		hash_sha256,	NOT_DEFAULT	| HIGH,				VER_TLS1_2,	VER_TLS1_2,		VER_TLS1_2,	VER_TLS1_2,		},
	{	DHE_RSA_WITH_AES_128_CCM,					KX_DHE_RSA,		AES_128_CCM,			MAC_AEAD,		hash_sha256,	NOT_DEFAULT	| HIGH,				VER_TLS1_2,	VER_TLS1_2,		VER_TLS1_2,	VER_TLS1_2,		},
	{	DHE_RSA_WITH_AES_256_CCM,					KX_DHE_RSA,		AES_256_CCM,			MAC_AEAD,		hash_sha256,	NOT_DEFAULT	| HIGH,				VER_TLS1_2,	VER_TLS1_2,		VER_TLS1_2,	VER_TLS1_2,		},
	{	RSA_WITH_AES_128_CCM_8,						KX_RSA,			AES_128_CCM_8,			MAC_AEAD,		hash_sha256,	NOT_DEFAULT	| HIGH,				VER_TLS1_2,	VER_TLS1_2,		VER_TLS1_2,	VER_TLS1_2,		},
	{	RSA_WITH_AES_256_CCM_8,						KX_RSA,			AES_256_CCM_8,			MAC_AEAD,		hash_sha256,	NOT_DEFAULT	| HIGH,				VER_TLS1_2,	VER_TLS1_2,		VER_TLS1_2,	VER_TLS1_2,		},
	{	DHE_RSA_WITH_AES_128_CCM_8,					KX_DHE_RSA,		AES_128_CCM_8,			MAC_AEAD,		hash_sha256,	NOT_DEFAULT	| HIGH,				VER_TLS1_2,	VER_TLS1_2,		VER_TLS1_2,	VER_TLS1_2,		},
	{	DHE_RSA_WITH_AES_256_CCM_8,					KX_DHE_RSA,		AES_256_CCM_8,			MAC_AEAD,		hash_sha256,	NOT_DEFAULT	| HIGH,				VER_TLS1_2,	VER_TLS1_2,		VER_TLS1_2,	VER_TLS1_2,		},
	{	PSK_WITH_AES_128_CCM,						KX_PSK,			AES_128_CCM,			MAC_AEAD,		hash_sha256,	NOT_DEFAULT	| HIGH,				VER_TLS1_2,	VER_TLS1_2,		VER_TLS1_2,	VER_TLS1_2,		},
	{	PSK_WITH_AES_256_CCM,						KX_PSK,			AES_256_CCM,			MAC_AEAD,		hash_sha256,	NOT_DEFAULT	| HIGH,				VER_TLS1_2,	VER_TLS1_2,		VER_TLS1_2,	VER_TLS1_2,		},
	{	DHE_PSK_WITH_AES_128_CCM,					KX_DHE_PSK,		AES_128_CCM,			MAC_AEAD,		hash_sha256,	NOT_DEFAULT	| HIGH,				VER_TLS1_2,	VER_TLS1_2,		VER_TLS1_2,	VER_TLS1_2,		},
	{	DHE_PSK_WITH_AES_256_CCM,					KX_DHE_PSK,		AES_256_CCM,			MAC_AEAD,		hash_sha256,	NOT_DEFAULT	| HIGH,				VER_TLS1_2,	VER_TLS1_2,		VER_TLS1_2,	VER_TLS1_2,		},
	{	PSK_WITH_AES_128_CCM_8,						KX_PSK,			AES_128_CCM_8,			MAC_AEAD,		hash_sha256,	NOT_DEFAULT	| HIGH,				VER_TLS1_2,	VER_TLS1_2,		VER_TLS1_2,	VER_TLS1_2,		},
	{	PSK_WITH_AES_256_CCM_8,						KX_PSK,			AES_256_CCM_8,			MAC_AEAD,		hash_sha256,	NOT_DEFAULT	| HIGH,				VER_TLS1_2,	VER_TLS1_2,		VER_TLS1_2,	VER_TLS1_2,		},
	{	PSK_DHE_WITH_AES_128_CCM_8,					KX_DHE_PSK,		AES_128_CCM_8,			MAC_AEAD,		hash_sha256,	NOT_DEFAULT	| HIGH,				VER_TLS1_2,	VER_TLS1_2,		VER_TLS1_2,	VER_TLS1_2,		},
	{	PSK_DHE_WITH_AES_256_CCM_8,					KX_DHE_PSK,		AES_256_CCM_8,			MAC_AEAD,		hash_sha256,	NOT_DEFAULT	| HIGH,				VER_TLS1_2,	VER_TLS1_2,		VER_TLS1_2,	VER_TLS1_2,		},
	{	ECDHE_ECDSA_WITH_AES_128_CCM,				KX_ECDHE_ECDSA,	AES_128_CCM,			MAC_AEAD,		hash_sha256,	NOT_DEFAULT	| HIGH,				VER_TLS1_2,	VER_TLS1_2,		VER_TLS1_2,	VER_TLS1_2,		},
	{	ECDHE_ECDSA_WITH_AES_256_CCM,				KX_ECDHE_ECDSA,	AES_256_CCM,			MAC_AEAD,		hash_sha256,	NOT_DEFAULT	| HIGH,				VER_TLS1_2,	VER_TLS1_2,		VER_TLS1_2,	VER_TLS1_2,		},
	{	ECDHE_ECDSA_WITH_AES_128_CCM_8,				KX_ECDHE_ECDSA,	AES_128_CCM_8,			MAC_AEAD,		hash_sha256,	NOT_DEFAULT	| HIGH,				VER_TLS1_2,	VER_TLS1_2,		VER_TLS1_2,	VER_TLS1_2,		},
	{	ECDHE_ECDSA_WITH_AES_256_CCM_8,				KX_ECDHE_ECDSA,	AES_256_CCM_8,			MAC_AEAD,		hash_sha256,	NOT_DEFAULT	| HIGH,				VER_TLS1_2,	VER_TLS1_2,		VER_TLS1_2,	VER_TLS1_2,		},
	{	AES_128_GCM_SHA256,							KX_RSA,			AES_128_GCM,			MAC_AEAD,		hash_sha256,	HIGH,							VER_TLS1_3,	VER_TLS1_3,		0,			0,				},

	{	ECDHE_ECDSA_WITH_NULL_SHA,					KX_ECDHE_ECDSA,	NO_ENCRYPT,				MAC_SHA,		hash_md5_sha1,	FIPS,							VER_SSL3,	VER_TLS1_2,		VER_BAD,	VER_TLS1_2,		},
	{	ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA,			KX_ECDHE_ECDSA,	DES_192_EDE_CBC,		MAC_SHA,		hash_md5_sha1,	NOT_DEFAULT	| MEDIUM | FIPS,	VER_SSL3,	VER_TLS1_2,		VER_BAD,	VER_TLS1_2,		},
	{	ECDHE_ECDSA_WITH_AES_128_CBC_SHA,			KX_ECDHE_ECDSA,	AES_128_CBC,			MAC_SHA,		hash_md5_sha1,	HIGH | FIPS,					VER_SSL3,	VER_TLS1_2,		VER_BAD,	VER_TLS1_2,		},
	{	ECDHE_ECDSA_WITH_AES_256_CBC_SHA,			KX_ECDHE_ECDSA,	AES_256_CBC,			MAC_SHA,		hash_md5_sha1,	HIGH | FIPS,					VER_SSL3,	VER_TLS1_2,		VER_BAD,	VER_TLS1_2,		},
	{	ECDHE_RSA_WITH_NULL_SHA,					KX_ECDHE_RSA,	NO_ENCRYPT,				MAC_SHA,		hash_md5_sha1,	FIPS,							VER_SSL3,	VER_TLS1_2,		VER_BAD,	VER_TLS1_2,		},
	{	ECDHE_RSA_WITH_3DES_EDE_CBC_SHA,			KX_ECDHE_RSA,	DES_192_EDE_CBC,		MAC_SHA,		hash_md5_sha1,	NOT_DEFAULT	| MEDIUM | FIPS,	VER_SSL3,	VER_TLS1_2,		VER_BAD,	VER_TLS1_2,		},
	{	ECDHE_RSA_WITH_AES_128_CBC_SHA,				KX_ECDHE_RSA,	AES_128_CBC,			MAC_SHA,		hash_md5_sha1,	HIGH | FIPS,					VER_SSL3,	VER_TLS1_2,		VER_BAD,	VER_TLS1_2,		},
	{	ECDHE_RSA_WITH_AES_256_CBC_SHA,				KX_ECDHE_RSA,	AES_256_CBC,			MAC_SHA,		hash_md5_sha1,	HIGH | FIPS,					VER_SSL3,	VER_TLS1_2,		VER_BAD,	VER_TLS1_2,		},
	{	ECDH_anon_WITH_NULL_SHA,					KX_ECDH_ANON,	NO_ENCRYPT,				MAC_SHA,		hash_md5_sha1,	FIPS,							VER_SSL3,	VER_TLS1_2,		VER_BAD,	VER_TLS1_2,		},
	{	ECDH_anon_WITH_3DES_EDE_CBC_SHA,			KX_ECDH_ANON,	DES_192_EDE_CBC,		MAC_SHA,		hash_md5_sha1,	NOT_DEFAULT	| MEDIUM | FIPS,	VER_SSL3,	VER_TLS1_2,		VER_BAD,	VER_TLS1_2,		},
	{	ECDH_anon_WITH_AES_128_CBC_SHA,				KX_ECDH_ANON,	AES_128_CBC,			MAC_SHA,		hash_md5_sha1,	NOT_DEFAULT	| HIGH | FIPS,		VER_SSL3,	VER_TLS1_2,		VER_BAD,	VER_TLS1_2,		},
	{	ECDH_anon_WITH_AES_256_CBC_SHA,				KX_ECDH_ANON,	AES_256_CBC,			MAC_SHA,		hash_md5_sha1,	NOT_DEFAULT	| HIGH | FIPS,		VER_SSL3,	VER_TLS1_2,		VER_BAD,	VER_TLS1_2,		},
	{	ECDHE_ECDSA_WITH_AES_128_CBC_SHA256,		KX_ECDHE_ECDSA,	AES_128_CBC,			MAC_SHA256,		hash_sha256,	HIGH | FIPS,					VER_TLS1_2,	VER_TLS1_2,		VER_TLS1_2,	VER_TLS1_2,		},
	{	ECDHE_ECDSA_WITH_AES_256_CBC_SHA384,		KX_ECDHE_ECDSA,	AES_256_CBC,			MAC_SHA384,		hash_sha384,	HIGH | FIPS,					VER_TLS1_2,	VER_TLS1_2,		VER_TLS1_2,	VER_TLS1_2,		},
	{	ECDHE_RSA_WITH_AES_128_CBC_SHA256,			KX_ECDHE_RSA,	AES_128_CBC,			MAC_SHA256,		hash_sha256,	HIGH | FIPS,					VER_TLS1_2,	VER_TLS1_2,		VER_TLS1_2,	VER_TLS1_2,		},
	{	ECDHE_RSA_WITH_AES_256_CBC_SHA384,			KX_ECDHE_RSA,	AES_256_CBC,			MAC_SHA384,		hash_sha384,	HIGH | FIPS,					VER_TLS1_2,	VER_TLS1_2,		VER_TLS1_2,	VER_TLS1_2,		},
	{	ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,		KX_ECDHE_ECDSA,	AES_128_GCM,			MAC_AEAD,		hash_sha256,	HIGH | FIPS,					VER_TLS1_2,	VER_TLS1_2,		VER_TLS1_2,	VER_TLS1_2,		},
	{	ECDHE_ECDSA_WITH_AES_256_GCM_SHA384,		KX_ECDHE_ECDSA,	AES_256_GCM,			MAC_AEAD,		hash_sha384,	HIGH | FIPS,					VER_TLS1_2,	VER_TLS1_2,		VER_TLS1_2,	VER_TLS1_2,		},
	{	ECDHE_RSA_WITH_AES_128_GCM_SHA256,			KX_ECDHE_RSA,	AES_128_GCM,			MAC_AEAD,		hash_sha256,	HIGH | FIPS,					VER_TLS1_2,	VER_TLS1_2,		VER_TLS1_2,	VER_TLS1_2,		},
	{	ECDHE_RSA_WITH_AES_256_GCM_SHA384,			KX_ECDHE_RSA,	AES_256_GCM,			MAC_AEAD,		hash_sha384,	HIGH | FIPS,					VER_TLS1_2,	VER_TLS1_2,		VER_TLS1_2,	VER_TLS1_2,		},
//
	{	PSK_WITH_NULL_SHA,							KX_PSK,			NO_ENCRYPT,				MAC_SHA,		hash_md5_sha1,	FIPS,							VER_SSL3,	VER_TLS1_2,		VER_BAD,	VER_TLS1_2,		},
	{	DHE_PSK_WITH_NULL_SHA,						KX_DHE_PSK,		NO_ENCRYPT,				MAC_SHA,		hash_md5_sha1,	FIPS,							VER_SSL3,	VER_TLS1_2,		VER_BAD,	VER_TLS1_2,		},
	{	RSA_PSK_WITH_NULL_SHA,						KX_RSA_PSK,		NO_ENCRYPT,				MAC_SHA,		hash_md5_sha1,	FIPS,							VER_SSL3,	VER_TLS1_2,		VER_BAD,	VER_TLS1_2,		},
	{	PSK_WITH_3DES_EDE_CBC_SHA,					KX_PSK,			DES_192_EDE_CBC,		MAC_SHA,		hash_md5_sha1,	NOT_DEFAULT	| MEDIUM | FIPS,	VER_SSL3,	VER_TLS1_2,		VER_BAD,	VER_TLS1_2,		},
	{	PSK_WITH_AES_128_CBC_SHA,					KX_PSK,			AES_128_CBC,			MAC_SHA,		hash_md5_sha1,	HIGH | FIPS,					VER_SSL3,	VER_TLS1_2,		VER_BAD,	VER_TLS1_2,		},
	{	PSK_WITH_AES_256_CBC_SHA,					KX_PSK,			AES_256_CBC,			MAC_SHA,		hash_md5_sha1,	HIGH | FIPS,					VER_SSL3,	VER_TLS1_2,		VER_BAD,	VER_TLS1_2,		},
	{	DHE_PSK_WITH_3DES_EDE_CBC_SHA,				KX_DHE_PSK,		DES_192_EDE_CBC,		MAC_SHA,		hash_md5_sha1,	NOT_DEFAULT	| MEDIUM | FIPS,	VER_SSL3,	VER_TLS1_2,		VER_BAD,	VER_TLS1_2,		},
	{	DHE_PSK_WITH_AES_128_CBC_SHA,				KX_DHE_PSK,		AES_128_CBC,			MAC_SHA,		hash_md5_sha1,	HIGH | FIPS,					VER_SSL3,	VER_TLS1_2,		VER_BAD,	VER_TLS1_2,		},
	{	DHE_PSK_WITH_AES_256_CBC_SHA,				KX_DHE_PSK,		AES_256_CBC,			MAC_SHA,		hash_md5_sha1,	HIGH | FIPS,					VER_SSL3,	VER_TLS1_2,		VER_BAD,	VER_TLS1_2,		},
	{	RSA_PSK_WITH_3DES_EDE_CBC_SHA,				KX_RSA_PSK,		DES_192_EDE_CBC,		MAC_SHA,		hash_md5_sha1,	NOT_DEFAULT	| MEDIUM | FIPS,	VER_SSL3,	VER_TLS1_2,		VER_BAD,	VER_TLS1_2,		},
	{	RSA_PSK_WITH_AES_128_CBC_SHA,				KX_RSA_PSK,		AES_128_CBC,			MAC_SHA,		hash_md5_sha1,	HIGH | FIPS,					VER_SSL3,	VER_TLS1_2,		VER_BAD,	VER_TLS1_2,		},
	{	RSA_PSK_WITH_AES_256_CBC_SHA,				KX_RSA_PSK,		AES_256_CBC,			MAC_SHA,		hash_md5_sha1,	HIGH | FIPS,					VER_SSL3,	VER_TLS1_2,		VER_BAD,	VER_TLS1_2,		},
	{	PSK_WITH_AES_128_GCM_SHA256,				KX_PSK,			AES_128_GCM,			MAC_AEAD,		hash_sha256,	HIGH | FIPS,					VER_TLS1_2,	VER_TLS1_2,		VER_TLS1_2,	VER_TLS1_2,		},
	{	PSK_WITH_AES_256_GCM_SHA384,				KX_PSK,			AES_256_GCM,			MAC_AEAD,		hash_sha384,	HIGH | FIPS,					VER_TLS1_2,	VER_TLS1_2,		VER_TLS1_2,	VER_TLS1_2,		},
	{	DHE_PSK_WITH_AES_128_GCM_SHA256,			KX_DHE_PSK,		AES_128_GCM,			MAC_AEAD,		hash_sha256,	HIGH | FIPS,					VER_TLS1_2,	VER_TLS1_2,		VER_TLS1_2,	VER_TLS1_2,		},
	{	DHE_PSK_WITH_AES_256_GCM_SHA384,			KX_DHE_PSK,		AES_256_GCM,			MAC_AEAD,		hash_sha384,	HIGH | FIPS,					VER_TLS1_2,	VER_TLS1_2,		VER_TLS1_2,	VER_TLS1_2,		},
	{	RSA_PSK_WITH_AES_128_GCM_SHA256,			KX_RSA_PSK,		AES_128_GCM,			MAC_AEAD,		hash_sha256,	HIGH | FIPS,					VER_TLS1_2,	VER_TLS1_2,		VER_TLS1_2,	VER_TLS1_2,		},
	{	RSA_PSK_WITH_AES_256_GCM_SHA384,			KX_RSA_PSK,		AES_256_GCM,			MAC_AEAD,		hash_sha384,	HIGH | FIPS,					VER_TLS1_2,	VER_TLS1_2,		VER_TLS1_2,	VER_TLS1_2,		},
	{	PSK_WITH_AES_128_CBC_SHA256,				KX_PSK,			AES_128_CBC,			MAC_SHA256,		hash_md5_sha1,	HIGH | FIPS,					VER_TLS1,	VER_TLS1_2,		VER_BAD,	VER_TLS1_2,		},
	{	PSK_WITH_AES_256_CBC_SHA384,				KX_PSK,			AES_256_CBC,			MAC_SHA384,		hash_sha384,	HIGH | FIPS,					VER_TLS1,	VER_TLS1_2,		VER_BAD,	VER_TLS1_2,		},
	{	PSK_WITH_NULL_SHA256,						KX_PSK,			NO_ENCRYPT,				MAC_SHA256,		hash_md5_sha1,	FIPS,							VER_TLS1,	VER_TLS1_2,		VER_BAD,	VER_TLS1_2,		},
	{	PSK_WITH_NULL_SHA384,						KX_PSK,			NO_ENCRYPT,				MAC_SHA384,		hash_sha384,	FIPS,							VER_TLS1,	VER_TLS1_2,		VER_BAD,	VER_TLS1_2,		},
	{	DHE_PSK_WITH_AES_128_CBC_SHA256,			KX_DHE_PSK,		AES_128_CBC,			MAC_SHA256,		hash_md5_sha1,	HIGH | FIPS,					VER_TLS1,	VER_TLS1_2,		VER_BAD,	VER_TLS1_2,		},
	{	DHE_PSK_WITH_AES_256_CBC_SHA384,			KX_DHE_PSK,		AES_256_CBC,			MAC_SHA384,		hash_sha384,	HIGH | FIPS,					VER_TLS1,	VER_TLS1_2,		VER_BAD,	VER_TLS1_2,		},
	{	DHE_PSK_WITH_NULL_SHA256,					KX_DHE_PSK,		NO_ENCRYPT,				MAC_SHA256,		hash_md5_sha1,	FIPS,							VER_TLS1,	VER_TLS1_2,		VER_BAD,	VER_TLS1_2,		},
	{	DHE_PSK_WITH_NULL_SHA384,					KX_DHE_PSK,		NO_ENCRYPT,				MAC_SHA384,		hash_sha384,	FIPS,							VER_TLS1,	VER_TLS1_2,		VER_BAD,	VER_TLS1_2,		},
	{	RSA_PSK_WITH_AES_128_CBC_SHA256,			KX_RSA_PSK,		AES_128_CBC,			MAC_SHA256,		hash_md5_sha1,	HIGH | FIPS,					VER_TLS1,	VER_TLS1_2,		VER_BAD,	VER_TLS1_2,		},
	{	RSA_PSK_WITH_AES_256_CBC_SHA384,			KX_RSA_PSK,		AES_256_CBC,			MAC_SHA384,		hash_sha384,	HIGH | FIPS,					VER_TLS1,	VER_TLS1_2,		VER_BAD,	VER_TLS1_2,		},
	{	RSA_PSK_WITH_NULL_SHA256,					KX_RSA_PSK,		NO_ENCRYPT,				MAC_SHA256,		hash_md5_sha1,	FIPS,							VER_TLS1,	VER_TLS1_2,		VER_BAD,	VER_TLS1_2,		},
	{	RSA_PSK_WITH_NULL_SHA384,					KX_RSA_PSK,		NO_ENCRYPT,				MAC_SHA384,		hash_sha384,	FIPS,							VER_TLS1,	VER_TLS1_2,		VER_BAD,	VER_TLS1_2,		},
//																											hash_
	{	ECDHE_PSK_WITH_3DES_EDE_CBC_SHA,			KX_ECDHE_PSK,	DES_192_EDE_CBC,		MAC_SHA,		hash_md5_sha1,	NOT_DEFAULT	| MEDIUM | FIPS,	VER_SSL3,	VER_TLS1_2,		VER_BAD,	VER_TLS1_2,		},
	{	ECDHE_PSK_WITH_AES_128_CBC_SHA,				KX_ECDHE_PSK,	AES_128_CBC,			MAC_SHA,		hash_md5_sha1,	HIGH | FIPS,					VER_SSL3,	VER_TLS1_2,		VER_BAD,	VER_TLS1_2,		},
	{	ECDHE_PSK_WITH_AES_256_CBC_SHA,				KX_ECDHE_PSK,	AES_256_CBC,			MAC_SHA,		hash_md5_sha1,	HIGH | FIPS,					VER_SSL3,	VER_TLS1_2,		VER_BAD,	VER_TLS1_2,		},
	{	ECDHE_PSK_WITH_AES_128_CBC_SHA256,			KX_ECDHE_PSK,	AES_128_CBC,			MAC_SHA256,		hash_md5_sha1,	HIGH | FIPS,					VER_TLS1,	VER_TLS1_2,		VER_BAD,	VER_TLS1_2,		},
	{	ECDHE_PSK_WITH_AES_256_CBC_SHA384,			KX_ECDHE_PSK,	AES_256_CBC,			MAC_SHA384,		hash_sha384,	HIGH | FIPS,					VER_TLS1,	VER_TLS1_2,		VER_BAD,	VER_TLS1_2,		},
	{	ECDHE_PSK_WITH_NULL_SHA,					KX_ECDHE_PSK,	NO_ENCRYPT,				MAC_SHA,		hash_md5_sha1,	FIPS,							VER_SSL3,	VER_TLS1_2,		VER_BAD,	VER_TLS1_2,		},
	{	ECDHE_PSK_WITH_NULL_SHA256,					KX_ECDHE_PSK,	NO_ENCRYPT,				MAC_SHA256,		hash_md5_sha1,	FIPS,							VER_TLS1,	VER_TLS1_2,		VER_BAD,	VER_TLS1_2,		},
	{	ECDHE_PSK_WITH_NULL_SHA384,					KX_ECDHE_PSK,	NO_ENCRYPT,				MAC_SHA384,		hash_sha384,	FIPS,							VER_TLS1,	VER_TLS1_2,		VER_BAD,	VER_TLS1_2,		},
//																											hash_
	{	SRP_SHA_WITH_3DES_EDE_CBC_SHA,				KX_SRP,			DES_192_EDE_CBC,		MAC_SHA,		hash_md5_sha1,	NOT_DEFAULT	| MEDIUM,			VER_SSL3,	VER_TLS1_2,		VER_BAD,	VER_TLS1_2,		},
	{	SRP_SHA_RSA_WITH_3DES_EDE_CBC_SHA,			KX_SRP_RSA,		DES_192_EDE_CBC,		MAC_SHA,		hash_md5_sha1,	NOT_DEFAULT	| MEDIUM,			VER_SSL3,	VER_TLS1_2,		VER_BAD,	VER_TLS1_2,		},
	{	SRP_SHA_DSS_WITH_3DES_EDE_CBC_SHA,			KX_SRP_DSS,		DES_192_EDE_CBC,		MAC_SHA,		hash_md5_sha1,	NOT_DEFAULT	| MEDIUM,			VER_SSL3,	VER_TLS1_2,		VER_BAD,	VER_TLS1_2,		},
	{	SRP_SHA_WITH_AES_128_CBC_SHA,				KX_SRP,			AES_128_CBC,			MAC_SHA,		hash_md5_sha1,	HIGH,							VER_SSL3,	VER_TLS1_2,		VER_BAD,	VER_TLS1_2,		},
	{	SRP_SHA_RSA_WITH_AES_128_CBC_SHA,			KX_SRP_RSA,		AES_128_CBC,			MAC_SHA,		hash_md5_sha1,	HIGH,							VER_SSL3,	VER_TLS1_2,		VER_BAD,	VER_TLS1_2,		},
	{	SRP_SHA_DSS_WITH_AES_128_CBC_SHA,			KX_SRP_DSS,		AES_128_CBC,			MAC_SHA,		hash_md5_sha1,	NOT_DEFAULT	| HIGH,				VER_SSL3,	VER_TLS1_2,		VER_BAD,	VER_TLS1_2,		},
	{	SRP_SHA_WITH_AES_256_CBC_SHA,				KX_SRP,			AES_256_CBC,			MAC_SHA,		hash_md5_sha1,	HIGH,							VER_SSL3,	VER_TLS1_2,		VER_BAD,	VER_TLS1_2,		},
	{	SRP_SHA_RSA_WITH_AES_256_CBC_SHA,			KX_SRP_RSA,		AES_256_CBC,			MAC_SHA,		hash_md5_sha1,	HIGH,							VER_SSL3,	VER_TLS1_2,		VER_BAD,	VER_TLS1_2,		},
	{	SRP_SHA_DSS_WITH_AES_256_CBC_SHA,			KX_SRP_DSS,		AES_256_CBC,			MAC_SHA,		hash_md5_sha1,	NOT_DEFAULT	| HIGH,				VER_SSL3,	VER_TLS1_2,		VER_BAD,	VER_TLS1_2,		},
//																											hash_
	{	DHE_RSA_WITH_CHACHA20_POLY1305,				KX_DHE_RSA,		CHACHA_20_POLY_1305,	MAC_AEAD,		hash_sha256,	HIGH,							VER_TLS1_2,	VER_TLS1_2,		VER_TLS1_2,	VER_TLS1_2,		},
	{	ECDHE_RSA_WITH_CHACHA20_POLY1305,			KX_ECDHE_RSA,	CHACHA_20_POLY_1305,	MAC_AEAD,		hash_sha256,	HIGH,							VER_TLS1_2,	VER_TLS1_2,		VER_TLS1_2,	VER_TLS1_2,		},
	{	ECDHE_ECDSA_WITH_CHACHA20_POLY1305,			KX_ECDHE_ECDSA,	CHACHA_20_POLY_1305,	MAC_AEAD,		hash_sha256,	HIGH,							VER_TLS1_2,	VER_TLS1_2,		VER_TLS1_2,	VER_TLS1_2,		},
	{	PSK_WITH_CHACHA20_POLY1305,					KX_PSK,			CHACHA_20_POLY_1305,	MAC_AEAD,		hash_sha256,	HIGH,							VER_TLS1_2,	VER_TLS1_2,		VER_TLS1_2,	VER_TLS1_2,		},
	{	ECDHE_PSK_WITH_CHACHA20_POLY1305,			KX_ECDHE_PSK,	CHACHA_20_POLY_1305,	MAC_AEAD,		hash_sha256,	HIGH,							VER_TLS1_2,	VER_TLS1_2,		VER_TLS1_2,	VER_TLS1_2,		},
	{	DHE_PSK_WITH_CHACHA20_POLY1305,				KX_DHE_PSK,		CHACHA_20_POLY_1305,	MAC_AEAD,		hash_sha256,	HIGH,							VER_TLS1_2,	VER_TLS1_2,		VER_TLS1_2,	VER_TLS1_2,		},
	{	RSA_PSK_WITH_CHACHA20_POLY1305,				KX_RSA_PSK,		CHACHA_20_POLY_1305,	MAC_AEAD,		hash_sha256,	HIGH,							VER_TLS1_2,	VER_TLS1_2,		VER_TLS1_2,	VER_TLS1_2,		},
//																											hash_
	{	RSA_WITH_CAMELLIA_128_CBC_SHA256,			KX_RSA,			CAMELLIA_128_CBC,		MAC_SHA256,		hash_sha256,	NOT_DEFAULT	| HIGH,				VER_TLS1_2,	VER_TLS1_2,		VER_TLS1_2,	VER_TLS1_2,		},
	{	DHE_DSS_WITH_CAMELLIA_128_CBC_SHA256,		KX_DHE_DSS,		CAMELLIA_128_CBC,		MAC_SHA256,		hash_sha256,	NOT_DEFAULT	| HIGH,				VER_TLS1_2,	VER_TLS1_2,		VER_TLS1_2,	VER_TLS1_2,		},
	{	DHE_RSA_WITH_CAMELLIA_128_CBC_SHA256,		KX_DHE_RSA,		CAMELLIA_128_CBC,		MAC_SHA256,		hash_sha256,	NOT_DEFAULT	| HIGH,				VER_TLS1_2,	VER_TLS1_2,		VER_TLS1_2,	VER_TLS1_2,		},
	{	DH_anon_WITH_CAMELLIA_128_CBC_SHA256,		KX_DH_ANON,		CAMELLIA_128_CBC,		MAC_SHA256,		hash_sha256,	NOT_DEFAULT	| HIGH,				VER_TLS1_2,	VER_TLS1_2,		VER_TLS1_2,	VER_TLS1_2,		},
	{	RSA_WITH_CAMELLIA_256_CBC_SHA256,			KX_RSA,			CAMELLIA_256_CBC,		MAC_SHA256,		hash_sha256,	NOT_DEFAULT	| HIGH,				VER_TLS1_2,	VER_TLS1_2,		VER_TLS1_2,	VER_TLS1_2,		},
	{	DHE_DSS_WITH_CAMELLIA_256_CBC_SHA256,		KX_DHE_DSS,		CAMELLIA_256_CBC,		MAC_SHA256,		hash_sha256,	NOT_DEFAULT	| HIGH,				VER_TLS1_2,	VER_TLS1_2,		VER_TLS1_2,	VER_TLS1_2,		},
	{	DHE_RSA_WITH_CAMELLIA_256_CBC_SHA256,		KX_DHE_RSA,		CAMELLIA_256_CBC,		MAC_SHA256,		hash_sha256,	NOT_DEFAULT	| HIGH,				VER_TLS1_2,	VER_TLS1_2,		VER_TLS1_2,	VER_TLS1_2,		},
	{	DH_anon_WITH_CAMELLIA_256_CBC_SHA256,		KX_DH_ANON,		CAMELLIA_256_CBC,		MAC_SHA256,		hash_sha256,	NOT_DEFAULT	| HIGH,				VER_TLS1_2,	VER_TLS1_2,		VER_TLS1_2,	VER_TLS1_2,		},
	{	RSA_WITH_CAMELLIA_256_CBC_SHA,				KX_RSA,			CAMELLIA_256_CBC,		MAC_SHA,		hash_md5_sha1,	NOT_DEFAULT	| HIGH,				VER_SSL3,	VER_TLS1_2,		VER_BAD,	VER_TLS1_2,		},
	{	DHE_DSS_WITH_CAMELLIA_256_CBC_SHA,			KX_DHE_DSS,		CAMELLIA_256_CBC,		MAC_SHA,		hash_md5_sha1,	NOT_DEFAULT	| HIGH,				VER_SSL3,	VER_TLS1_2,		VER_BAD,	VER_TLS1_2,		},
	{	DHE_RSA_WITH_CAMELLIA_256_CBC_SHA,			KX_DHE_RSA,		CAMELLIA_256_CBC,		MAC_SHA,		hash_md5_sha1,	NOT_DEFAULT	| HIGH,				VER_SSL3,	VER_TLS1_2,		VER_BAD,	VER_TLS1_2,		},
	{	DH_anon_WITH_CAMELLIA_256_CBC_SHA,			KX_DH_ANON,		CAMELLIA_256_CBC,		MAC_SHA,		hash_md5_sha1,	NOT_DEFAULT	| HIGH,				VER_SSL3,	VER_TLS1_2,		VER_BAD,	VER_TLS1_2,		},
	{	RSA_WITH_CAMELLIA_128_CBC_SHA,				KX_RSA,			CAMELLIA_128_CBC,		MAC_SHA,		hash_md5_sha1,	NOT_DEFAULT	| HIGH,				VER_SSL3,	VER_TLS1_2,		VER_BAD,	VER_TLS1_2,		},
	{	DHE_DSS_WITH_CAMELLIA_128_CBC_SHA,			KX_DHE_DSS,		CAMELLIA_128_CBC,		MAC_SHA,		hash_md5_sha1,	NOT_DEFAULT	| HIGH,				VER_SSL3,	VER_TLS1_2,		VER_BAD,	VER_TLS1_2,		},
	{	DHE_RSA_WITH_CAMELLIA_128_CBC_SHA,			KX_DHE_RSA,		CAMELLIA_128_CBC,		MAC_SHA,		hash_md5_sha1,	NOT_DEFAULT	| HIGH,				VER_SSL3,	VER_TLS1_2,		VER_BAD,	VER_TLS1_2,		},
	{	DH_anon_WITH_CAMELLIA_128_CBC_SHA,			KX_DH_ANON,		CAMELLIA_128_CBC,		MAC_SHA,		hash_md5_sha1,	NOT_DEFAULT	| HIGH,				VER_SSL3,	VER_TLS1_2,		VER_BAD,	VER_TLS1_2,		},
	{	ECDHE_ECDSA_WITH_CAMELLIA_128_CBC_SHA256,	KX_ECDHE_ECDSA,	CAMELLIA_128_CBC,		MAC_SHA256,		hash_sha256,	NOT_DEFAULT	| HIGH,				VER_TLS1_2,	VER_TLS1_2,		VER_TLS1_2,	VER_TLS1_2,		},
	{	ECDHE_ECDSA_WITH_CAMELLIA_256_CBC_SHA384,	KX_ECDHE_ECDSA,	CAMELLIA_256_CBC,		MAC_SHA384,		hash_sha384,	NOT_DEFAULT	| HIGH,				VER_TLS1_2,	VER_TLS1_2,		VER_TLS1_2,	VER_TLS1_2,		},
	{	ECDHE_RSA_WITH_CAMELLIA_128_CBC_SHA256,		KX_ECDHE_RSA,	CAMELLIA_128_CBC,		MAC_SHA256,		hash_sha256,	NOT_DEFAULT	| HIGH,				VER_TLS1_2,	VER_TLS1_2,		VER_TLS1_2,	VER_TLS1_2,		},
	{	ECDHE_RSA_WITH_CAMELLIA_256_CBC_SHA384,		KX_ECDHE_RSA,	CAMELLIA_256_CBC,		MAC_SHA384,		hash_sha384,	NOT_DEFAULT	| HIGH,				VER_TLS1_2,	VER_TLS1_2,		VER_TLS1_2,	VER_TLS1_2,		},
//																											hash_
	{	PSK_WITH_CAMELLIA_128_CBC_SHA256,			KX_PSK,			CAMELLIA_128_CBC,		MAC_SHA256,		hash_md5_sha1,	NOT_DEFAULT	| HIGH,				VER_TLS1,	VER_TLS1_2,		VER_BAD,	VER_TLS1_2,		},
	{	PSK_WITH_CAMELLIA_256_CBC_SHA384,			KX_PSK,			CAMELLIA_256_CBC,		MAC_SHA384,		hash_sha384,	NOT_DEFAULT	| HIGH,				VER_TLS1,	VER_TLS1_2,		VER_BAD,	VER_TLS1_2,		},
	{	DHE_PSK_WITH_CAMELLIA_128_CBC_SHA256,		KX_DHE_PSK,		CAMELLIA_128_CBC,		MAC_SHA256,		hash_md5_sha1,	NOT_DEFAULT	| HIGH,				VER_TLS1,	VER_TLS1_2,		VER_BAD,	VER_TLS1_2,		},
	{	DHE_PSK_WITH_CAMELLIA_256_CBC_SHA384,		KX_DHE_PSK,		CAMELLIA_256_CBC,		MAC_SHA384,		hash_sha384,	NOT_DEFAULT	| HIGH,				VER_TLS1,	VER_TLS1_2,		VER_BAD,	VER_TLS1_2,		},
	{	RSA_PSK_WITH_CAMELLIA_128_CBC_SHA256,		KX_RSA_PSK,		CAMELLIA_128_CBC,		MAC_SHA256,		hash_md5_sha1,	NOT_DEFAULT	| HIGH,				VER_TLS1,	VER_TLS1_2,		VER_BAD,	VER_TLS1_2,		},
	{	RSA_PSK_WITH_CAMELLIA_256_CBC_SHA384,		KX_RSA_PSK,		CAMELLIA_256_CBC,		MAC_SHA384,		hash_sha384,	NOT_DEFAULT	| HIGH,				VER_TLS1,	VER_TLS1_2,		VER_BAD,	VER_TLS1_2,		},
	{	ECDHE_PSK_WITH_CAMELLIA_128_CBC_SHA256,		KX_ECDHE_PSK,	CAMELLIA_128_CBC,		MAC_SHA256,		hash_md5_sha1,	NOT_DEFAULT	| HIGH,				VER_TLS1,	VER_TLS1_2,		VER_BAD,	VER_TLS1_2,		},
	{	ECDHE_PSK_WITH_CAMELLIA_256_CBC_SHA384,		KX_ECDHE_PSK,	CAMELLIA_256_CBC,		MAC_SHA384,		hash_sha384,	NOT_DEFAULT	| HIGH,				VER_TLS1,	VER_TLS1_2,		VER_BAD,	VER_TLS1_2,		},
//																											hash_
	{	GOST2001_GOST89_GOST89,						KX_GOST_GOST01,	GOST_89_CNT,			MAC_GOST89,		hash_gost94,	HIGH | STREAM_MAC,				VER_TLS1,	VER_TLS1_2,		0,			0,				},
	{	GOST2001_NULL_GOST94,						KX_GOST_GOST01,	NO_ENCRYPT,				MAC_GOST94,		hash_gost94,	0,								VER_TLS1,	VER_TLS1_2,		0,			0,				},
	{	GOST2012_GOST8912_GOST8912,					KX_GOST_GOST12,	GOST_89_CNT_12,			MAC_GOST89_12,	hash_gost89,	HIGH | STREAM_MAC,				VER_TLS1,	VER_TLS1_2,		0,			0,				},
	{	GOST2012_NULL_GOST12,						KX_GOST_GOST12,	NO_ENCRYPT,				MAC_GOST12_256,	hash_gost12_256,STREAM_MAC,						VER_TLS1,	VER_TLS1_2,		0,			0,				},
//																											hash_
	{	RSA_WITH_IDEA_CBC_SHA,						KX_RSA,			IDEA_CBC,				MAC_SHA,		hash_md5_sha1,	NOT_DEFAULT	| MEDIUM,			VER_SSL3,	VER_TLS1_1,		VER_BAD,	VER_TLS1,		},
//																											hash_
	{	RSA_WITH_SEED_CBC_SHA,						KX_RSA,			SEED,					MAC_SHA,		hash_md5_sha1,	NOT_DEFAULT	| MEDIUM,			VER_SSL3,	VER_TLS1_2,		VER_BAD,	VER_TLS1_2,		},
	{	DHE_DSS_WITH_SEED_CBC_SHA,					KX_DHE_DSS,		SEED,					MAC_SHA,		hash_md5_sha1,	NOT_DEFAULT	| MEDIUM,			VER_SSL3,	VER_TLS1_2,		VER_BAD,	VER_TLS1_2,		},
	{	DHE_RSA_WITH_SEED_CBC_SHA,					KX_DHE_RSA,		SEED,					MAC_SHA,		hash_md5_sha1,	NOT_DEFAULT	| MEDIUM,			VER_SSL3,	VER_TLS1_2,		VER_BAD,	VER_TLS1_2,		},
	{	DH_anon_WITH_SEED_CBC_SHA,					KX_DH_ANON,		SEED,					MAC_SHA,		hash_md5_sha1,	NOT_DEFAULT	| MEDIUM,			VER_SSL3,	VER_TLS1_2,		VER_BAD,	VER_TLS1_2,		},
//																											hash_
	{	RSA_WITH_RC4_128_MD5,						KX_RSA,			RC4_128,				MAC_MD5,		hash_md5_sha1,	NOT_DEFAULT	| MEDIUM,			VER_SSL3,	VER_TLS1_2,		0,			0,				},
	{	RSA_WITH_RC4_128_SHA,						KX_RSA,			RC4_128,				MAC_SHA,		hash_md5_sha1,	NOT_DEFAULT	| MEDIUM,			VER_SSL3,	VER_TLS1_2,		0,			0,				},
	{	DH_anon_WITH_RC4_128_MD5,					KX_DH_ANON,		RC4_128,				MAC_MD5,		hash_md5_sha1,	NOT_DEFAULT	| MEDIUM,			VER_SSL3,	VER_TLS1_2,		0,			0,				},
	{	ECDHE_PSK_WITH_RC4_128_SHA,					KX_ECDHE_PSK,	RC4_128,				MAC_SHA,		hash_md5_sha1,	NOT_DEFAULT	| MEDIUM,			VER_SSL3,	VER_TLS1_2,		0,			0,				},
	{	ECDH_anon_WITH_RC4_128_SHA,					KX_ECDH_ANON,	RC4_128,				MAC_SHA,		hash_md5_sha1,	NOT_DEFAULT	| MEDIUM,			VER_SSL3,	VER_TLS1_2,		0,			0,				},
	{	ECDHE_ECDSA_WITH_RC4_128_SHA,				KX_ECDHE_ECDSA,	RC4_128,				MAC_SHA,		hash_md5_sha1,	NOT_DEFAULT	| MEDIUM,			VER_SSL3,	VER_TLS1_2,		0,			0,				},
	{	ECDHE_RSA_WITH_RC4_128_SHA,					KX_ECDHE_RSA,	RC4_128,				MAC_SHA,		hash_md5_sha1,	NOT_DEFAULT	| MEDIUM,			VER_SSL3,	VER_TLS1_2,		0,			0,				},
//																											hash_
	{	PSK_WITH_RC4_128_SHA,						KX_PSK,			RC4_128,				MAC_SHA,		hash_md5_sha1,	NOT_DEFAULT	| MEDIUM,			VER_SSL3,	VER_TLS1_2,		0,			0,				},
	{	RSA_PSK_WITH_RC4_128_SHA,					KX_RSA_PSK,		RC4_128,				MAC_SHA,		hash_md5_sha1,	NOT_DEFAULT	| MEDIUM,			VER_SSL3,	VER_TLS1_2,		0,			0,				},
	{	DHE_PSK_WITH_RC4_128_SHA,					KX_DHE_PSK,		RC4_128,				MAC_SHA,		hash_md5_sha1,	NOT_DEFAULT	| MEDIUM,			VER_SSL3,	VER_TLS1_2,		0,			0,				},
};

//mode,		blocksize,	ivlen,		eivlen
//cbc,		16,			16,			ivlen
//ecb,		16,			0,			0
//ofb128,	1,			16,			0
//cfb128,	1,			16,			0
//cfb1,		1,			16,			0
//cfb8,		1,			16,			0
//ctr,		1,			16,			0
//GCM:								GCM_EXPLICIT_IV_LEN;
//CCM:								CCM_EXPLICIT_IV_LEN;

struct BulkEncryptionInfo {
	uint8				block, key_bytes, fixed_iv, record_iv, auth_tag;
};
static const BulkEncryptionInfo bulk_encryption_info[] = {
//								block	key_bytes	fixed_iv	record_iv		auth_tag
	/*NO_ENCRYPT			*/	{0,		0,			0,			0,				0,		},
	/*FORTEZZA_CBC			*/	{8,		0,			0,			8,				0,		},
	/*IDEA_CBC				*/	{8,		16,			8,			8,				0,		},
	/*RC2_CBC_40			*/	{8,		5,			0,			8,				0,		},//?
	/*RC4_40				*/	{0,		5,			0,			0,				0,		},//?
	/*RC4_128				*/	{0,		16,			0,			0,				0,		},
	/*DES_40_CBC			*/	{8,		5,			0,			8,				0,		},//?
	/*DES_64_CBC			*/	{8,		8,			0,			8,				0,		},//?
	/*DES_192_EDE_CBC		*/	{8,		24,			8,			8,				0,		},
	/*AES_128_CBC			*/	{16,	16,			16,			16,				0,		},
	/*AES_256_CBC			*/	{32,	32,			16,			16,				0,		},
	/*AES_128_GCM			*/	{0,		16,			4,			8,				16,		},
	/*AES_256_GCM			*/	{0,		32,			4,			8,				16,		},
	/*AES_128_CCM			*/	{0,		16,			4,			8,				16,		},
	/*AES_256_CCM			*/	{0,		32,			4,			8,				16,		},
	/*AES_128_CCM_8			*/	{0,		16,			4,			8,				8,		},
	/*AES_256_CCM_8			*/	{0,		32,			4,			8,				8,		},
	/*CAMELLIA_128_CBC		*/	{16,	16,			16,			16,				0,		},
	/*CAMELLIA_256_CBC		*/	{16,	32,			16,			16,				0,		},
	/*SEED					*/	{0,		16,			16,			16,				0,		},
	/*GOST_89_CNT			*/	{0,		0,			0,			0,				0,		},//?
	/*GOST_89_CNT_12		*/	{0,		0,			0,			0,				0,		},//?
	/*CHACHA_20_POLY_1305	*/	{0,		32,			12,			0,				16,		},
};

static const struct { uint8 digest; const ASN1::OID *oid; } hash_info[] = {
	/*none			*/		{0,													},
	/*md5			*/		{16,	ASN1::find_objectid(1,2,840,113549,2,5),	},
	/*sha1			*/		{20,	ASN1::find_objectid(1,3,14,3,2,26),			},
	/*sha224		*/		{28,	ASN1::find_objectid(2,16,840,1,101,3,4,2,4),},
	/*sha256		*/		{32,	ASN1::find_objectid(2,16,840,1,101,3,4,2,1),},
	/*sha384		*/		{48,	ASN1::find_objectid(2,16,840,1,101,3,4,2,2),},
	/*sha512		*/		{64,	ASN1::find_objectid(2,16,840,1,101,3,4,2,3),},
	/*gost89		*/		{8,													},
	/*gost94		*/		{32,												},
	/*gost12		*/		{0,													},
	/*md5_sha1		*/		{16 + 20,											},
};

inline uint32 get_unix_time() {
//	return 0x58f24c40;
	return uint32(DateTime::Now().ToUnixTime());
}

//-----------------------------------------------------------------------------
//	Record structs
//-----------------------------------------------------------------------------

template<typename T, int S> struct chunk : T {
	typedef uintn<S / 8, true> ST;

	chunk() {}
	template<typename P> chunk(const P &p) : T(p) {}

	bool write(PacketBuilder &w) const {
		patch_write<ST>	len(w);
		uint32	start = w.tell();
		if (w.write(*(const T*)this)) {
			*len	= w.tell() - start;
			return true;
		}
		return false;
	}
	bool read(PacketReader &r) {
		return chunk_reader<S>(r), r.read(*(T*)this);
	}
};

template<typename T, int S, bool X> struct chunk_array : dynamic_array<T> {
	typedef dynamic_array<T>	B;
	typedef uintn<S / 8, true> ST;

	bool write(PacketBuilder &w) const {
		w.write(ST(B::size32() * uint32(sizeof(T))));
		iso::writen(w, B::begin(), B::size32());
		return true;
	}

	bool read(PacketReader &r) {
		uint32	n = r.get<ST>() / uint32(sizeof(T));
		B::resize(n);
		iso::readn(r, B::begin(), n);
		return true;
	}
};

template<typename T, int S> struct chunk_array<T, S, true> : dynamic_array<T> {
	typedef dynamic_array<T>	B;
	typedef uintn<S / 8, true>	ST;

	bool write(PacketBuilder &w) const {
		patch_write<ST>	len(w);
		uint32	start	= w.tell32();
		iso::writen(w, B::begin(), B::size32());
		*len			= w.tell32() - start;
		return true;
	}
	bool read(PacketReader &r) {
		uint32 len = r.get<ST>();
		for (streamptr end = r.tell() + len; r.tell() < end;) {
			if (!r.read(B::push_back()))
				return false;
		}
		return true;
	}
};

template<typename T> struct ASN1T : T {
	ASN1T() {}
	template<typename P> ASN1T(const P &p) : T(p) {}
	bool write(PacketBuilder &b) const	{ return ASN1::Write(b, *(const T*)this); }
	bool read(PacketReader &r)			{ return ASN1::Read(r, *(T*)this); }
};
template<typename T, int S> struct chunk<dynamic_array<T>, S> : chunk_array<T, S, write_type_v<PacketBuilder,T> == 2> {};

struct Record {
	enum Type : uint8 {
		change_cipher_spec				= 20,
		alert							= 21,
		handshake						= 22,
		application_data				= 23,
		heartbeat						= 24,
	};
	static const uint32 MAX_RECORD = 1 << 14;
	Type				type;		// higher level protocol used to process the enclosed fragment
	Version2			version;	// version of protocol being employed
	packed<uint16be>	length;		// length(in bytes) of the following fragment (should not exceed 2^14)
//	uint8				data[];
	Record()	{}
	Record(Type _type, Version _version) : type(_type), version(_version) {}
	Record(Type _type, Version _version, uint16 _length) : type(_type), version(_version), length(_length) {}
	bool	valid() const {
		return between(type, change_cipher_spec, heartbeat)
			&& between(version, VER_MIN, VER_MAX);
	}

	struct Message : patch_write<Record> {
		Connection	&con;
		Message(Connection &_con, PacketBuilderSSL &w, Type type, Version version = VER_CURRENT) : patch_write<Record>(w), con(_con) {
			Record	*h	= *this;
			h->type		= type;
			h->version	= version;
			if (con.local_encrypted)
				w.alloc_offset(con.record_iv_length);
		}
		~Message() {
			con.write_record(*this);
		}
	};
};

struct ChangeCipherSpec {
	static const Record::Type record_type = Record::change_cipher_spec;
	enum Type : uint8 {
		change_cipher_spec	= 1,
	};
	Type	type;
	ChangeCipherSpec() : type(change_cipher_spec) {}
};

struct Alert {
	static const Record::Type record_type = Record::alert;
	enum Level : uint8 {
		warning = 1, fatal = 2
	};
	enum Description : uint8 {
		close_notify					= 0,
		unexpected_message				= 10,
		bad_record_mac					= 20,
		decryption_failed				= 21,
		record_overflow					= 22,
		decompression_failure			= 30,
		handshake_failure				= 40,
		no_certificate_RESERVED			= 41,
		bad_certificate					= 42,
		unsupported_certificate			= 43,
		certificate_revoked				= 44,
		certificate_expired				= 45,
		certificate_unknown				= 46,
		illegal_parameter				= 47,
		unknown_ca						= 48,
		access_denied					= 49,
		decode_error					= 50,
		decrypt_error					= 51,
		export_restriction_RESERVED		= 60,
		protocol_version				= 70,
		insufficient_security			= 71,
		internal_error					= 80,
		inappropriate_fallback			= 86,
		user_canceled					= 90,
		no_renegotiation				= 100,
		unsupported_extension			= 110,
		certificate_unobtainable		= 111,
		unrecognized_name				= 112,
		bad_certificate_status_response	= 113,
		bad_certificate_hash_value		= 114,
		unknown_psk_identity			= 115,
	};
	Level		level;
	Description	description;
	Alert() {}
	Alert(Level level, Description description) : level(level), description(description) {}

	void handle() {
		static const char *levels[] = {"?", "warning", "fatal"};
		ISO_TRACEF("SSL Alert:(") << levels[level] << "): " << description << '\n';
	}
};

struct Heartbeat {
	static const Record::Type record_type = Record::heartbeat;
	enum Type : uint8 {
		request						= 1,
		response					= 2,
	};
	enum Mode {
		peer_allowed_to_send		= 1,
		peer_not_allowed_to_send	= 2,
	};
	Type	type;
};

struct Handshake {
	static const Record::Type record_type = Record::handshake;
	enum Type : uint8 {
		hello_request					= 0,
		client_hello					= 1,
		server_hello					= 2,
		hello_verify_request			= 3,
		NewSessionTicket				= 4,
		certificate						= 11,
		server_key_exchange				= 12,
		certificate_request				= 13,
		server_hello_done				= 14,
		certificate_verify				= 15,
		client_key_exchange				= 16,
		finished						= 20,
		certificate_url					= 21,
		certificate_status				= 22,
		supplemental_data				= 23,
	};

	Type				type;		// type of handshake message
	uint24be			length;		// # bytes in handshake message body

	Handshake()	{}
	Handshake(Type type) : type(type) {}
	Handshake(Type type, uint32 length) : type(type), length(length) {}
};

struct HandshakeMessage : patch_write<Handshake> {
	malloc_block		&messages;
	HandshakeMessage(malloc_block &messages, PacketBuilderSSL &w, Handshake::Type type) : patch_write<Handshake>(w), messages(messages) {
		get()->type = type;
	}
	~HandshakeMessage() {
		get()->length	= uint32(length() - sizeof(Handshake));
		messages += all();
	}
};

//-----------------------------------------------------------------------------
//	Handshake structs
//-----------------------------------------------------------------------------

struct Extension {
	enum Type : uint16 {
		server_name						= 0,
		max_fragment_length				= 1,
		client_certificate_url			= 2,
		trusted_ca_keys					= 3,
		truncated_hmac					= 4,
		status_request					= 5,
		user_mapping					= 6,
		client_authz					= 7,
		server_authz					= 8,
		cert_type						= 9,
		supported_groups				= 10,
		ec_point_formats				= 11,
		srp								= 12,
		signature_algorithms			= 13,
		use_srtp						= 14,
		heartbeat						= 15,
		application_layer_protocol_negotiation = 16,
		status_request_v2				= 17,
		signed_certificate_timestamp	= 18,
		client_certificate_type			= 19,
		server_certificate_type			= 20,
		padding							= 21,
		encrypt_then_mac				= 22,
		extended_master_secret			= 23,
		token_binding					= 24,//(TEMPORARY - expires 2018-02-04)
		cached_info						= 25,
		SessionTicket_TLS				= 35,
		renegotiation_info				= 0xff01,
		elliptic_curves					= supported_groups,
	};
	BE(Type)				type;
	opaque_array<16>		data;
	template<Type T> struct Struct		{
		bool write(PacketBuilder &w) const { return true; }
	};//undefined -> empty

	Extension()							{}
	Extension(Type _type) : type(_type)	{}
	template<Type T> Extension(const Struct<T> &s) : type(T), data(serialise(s)) {}

	bool write(PacketBuilder &w) const	{ return w.write(type, data); }
	bool read(PacketReader &r)			{ return r.read(type, data); }
};

struct Extensions : chunk<dynamic_array<Extension>, 16> {
	const Extension	*get(Extension::Type type) const;
	template<Extension::Type T> const Extension::Struct<T> *get() const {
		if (const Extension *ext = get(T))
			return ext->data;
		return 0;
	}
};

typedef BE(CipherSuite) CipherSuite2;

struct Hello {
	Version2		version;
	TimeRandom		random;
	SessionID		session_id;
	Hello(Version _version) : version(_version)	{}
	bool write(PacketBuilder &w) const	{ return w.write(version, random, session_id); }
	bool read(PacketReader &r)			{ return r.read(version, random, session_id); }
};

struct ClientHello : Hello {
	chunk<dynamic_array<CipherSuite2>, 16>			cipher_suites;
	chunk<dynamic_array<Compression>, 8>			compressions;
	optional<Extensions>							extensions;
	ClientHello(Version _version = VER_CURRENT) : Hello(_version) {}
	bool write(PacketBuilder &w) const	{ return w.write((const Hello&)*this, cipher_suites, compressions, extensions); }
	bool read(PacketReader &r)			{ return r.read((Hello&)*this, cipher_suites, compressions, extensions); }
};

struct ServerHello : Hello {
	CipherSuite2									cipher_suite;
	Compression										compression;
	optional<Extensions>							extensions;
	ServerHello(Version _version = VER_CURRENT) : Hello(_version)	{}
	bool write(PacketBuilder &w) const	{ return w.write((const Hello&)*this, cipher_suite, compression, extensions); }
	bool read(PacketReader &r)			{ return r.read(*(Hello*)this, cipher_suite, compression, extensions); }
};

struct CertificateRequest {
	chunk<dynamic_array<CertificateType>, 8>			types;
	chunk<dynamic_array<SignatureAndHashAlgorithm>, 16>	sig_hash;
	chunk<dynamic_array<chunk<ASN1T<CertificateAuthority>, 16> >, 16>	authorities;
	bool write(PacketBuilder &w) const	{ return w.write(types, sig_hash, authorities); }
	bool read(PacketReader &r)			{ return r.read(types, sig_hash, authorities); }
};

struct SupplementalData {
	enum Type : uint16 {
		user_mapping_data	= 0,
		authz_data			= 16386,
	};
	BE(Type)			type;
	opaque_array<16>	data;
};

struct AuthorizationData {
	enum Format : uint8 {
		x509_attr_cert					= 0,	//X509AttrCert;
		saml_assertion					= 1,	//SAMLAssertion;
		x509_attr_cert_url				= 2,	//URLandHash;
		saml_assertion_url				= 3,	//URLandHash;
		keynote_assertion_list			= 64,	//
		keynote_assertion_list_url		= 65,	//
		dtcp_authorization				= 66,	//
	};

	struct URLandHash {
		opaque_array<16>	url;
		HashAlgorithm		hash_alg;
		malloc_block		hash;
	};

	struct Entry {
		Format				format;
		opaque_array<16>	data;
	};

	chunk<dynamic_array<Entry>, 16>	data;
};

//-----------------------------------------------------------------------------
//	compatibility
//-----------------------------------------------------------------------------

//Note that this message MUST be sent directly on the wire, not wrapped as a TLS record
struct V2ClientHello {
	struct V2CipherSpec {
		enum E {
			RC4_128_WITH_MD5					= 0x800001,
			RC4_128_EXPORT40_WITH_MD5			= 0x800002,
			RC2_CBC_128_CBC_WITH_MD5			= 0x800003,
			RC2_CBC_128_CBC_EXPORT40_WITH_MD5	= 0x800004,
			IDEA_128_CBC_WITH_MD5				= 0x800005,
			DES_64_CBC_WITH_MD5					= 0x400006,
			DES_192_EDE3_CBC_WITH_MD5			= 0xC00007,
		};
		uintn<3, true>	i;
		operator E() const					{ return E((uint32)i); }
		const E&	operator=(const E &e)	{ i = (uint32)e; return e; }
	};

	uint16be		msg_length;
	uint8			msg_type;			//1 - this field, in conjunction with the version field, identifies a version 2 client hello message
	Version2		version;
	uint16be		cipher_spec_length;	//total length of the field cipher_specs
	uint16be		session_id_length;	//must be zero
	uint16be		challenge_length;	//length in bytes of the client's challenge to the server to authenticate itself (must be 32)
	//V2CipherSpec	cipher_specs[cipher_spec_length];	// list of all CipherSpecs the client is willing and able to use
	//uint8			challenge[challenge_length];		// client's challenge to the server for the server to identify itself. Version 3.0 server right justifies the challenge data to become the ClientHello.random data (padded with leading zeroes, if necessary)
};

//-----------------------------------------------------------------------------
//	Extensions
//-----------------------------------------------------------------------------

template<> struct Extension::Struct<Extension::server_name> {
	enum Type : uint8 {
		host_name = 0,
	};
	struct Entry {
		Type				type;
		opaque_array<16>	name;
		Entry(Type type, const char *name) : type(type), name(const_memory_block(name, strlen(name))) {}
		bool write(PacketBuilder &w) const { return w.write(type, name); }
	};
	chunk<dynamic_array<Entry>, 16>	names;
	Struct(const char *name) { names.emplace_back(host_name, name); }
	bool write(PacketBuilder &w) const { return w.write(names); }
};

template<> struct Extension::Struct<Extension::max_fragment_length> {
	enum Length : uint8 {
		pow2_9 = 1, pow2_10 = 2, pow2_11 = 3, pow2_12 = 4
	};
	Length	length;
};

template<> struct Extension::Struct<Extension::trusted_ca_keys> {
	struct TrustedAuthority {
		enum Type : uint8 {
			pre_agreed = 0, key_sha1_hash = 1, x509_name = 2, cert_sha1_hash = 3
		};
		Type		type;
		union {
			SHA1::CODE			sha1;	// key_sha1_hash or cert_sha1_hash
			opaque_array<16>	name;	// x509_name
		};
		~TrustedAuthority() {
			if (type == x509_name)
				name.~opaque_array<16>();
		}
	};
	chunk<dynamic_array<TrustedAuthority>, 16> trusted_authorities_list;
};

template<> struct Extension::Struct<Extension::status_request> {
	uint16	responder_id_list;
	chunk<dynamic_array<Extension>, 16> request_extensions;
};

template<> struct Extension::Struct<Extension::user_mapping> : chunk<dynamic_array<UserMapping>, 8> {};

template<> struct Extension::Struct<Extension::cert_type> {
	enum Type : uint8 {
		X509 = 0,
		OpenPGP = 1,
	};
	chunk<dynamic_array<Type>, 8> types;
};

template<> struct Extension::Struct<Extension::signature_algorithms> : chunk<dynamic_array<SignatureAndHashAlgorithm>, 16> {
	Struct()								{}
	template<typename R> Struct(const R &r)	{ append(r); }
};

template<> struct Extension::Struct<Extension::renegotiation_info> {
	uint8	length_in_bytes;
//	optional [|length_in_bytes > 0|] binary renegotiated_connection
//	with BinaryEncoding{Length = length_in_bytes};
};

template<> struct Extension::Struct<Extension::application_layer_protocol_negotiation> {
	struct name {
		uint8	length;
		string	s;
	};
	chunk<dynamic_array<name>, 8> protocol_name_list;
};

template<> struct Extension::Struct<Extension::heartbeat> {
	enum Mode : uint8 {
		peer_allowed_to_send		= 0x01,
		peer_not_allowed_to_send	= 0x02,
	};
	Mode	mode;
};

const Extension	*Extensions::get(Extension::Type type) const {
	for (auto &i : *this) {
		if (i.type == type) {
			return &i;
		}
	}
	return 0;
}

//-----------------------------------------------------------------------------
//	Hashes
//-----------------------------------------------------------------------------

struct Interface {
	void		*p;
	callback_ref<void(void*)> _delete;//		(*_delete)(void*);

//	template<typename T> Interface(T *t) : p(t), _delete((void(*)(void*))&deleter<T>)	{}
	template<typename T> Interface(T *t) : p(t), _delete(deleter<T>())	{}
	~Interface() { _delete(p); }
};

struct EncryptInterface : Interface, writer_mixin<EncryptInterface> {
	size_t		(*_writebuff)(void*, const void*, size_t);
	template<typename T> EncryptInterface(T *t) : Interface(t), _writebuff(make_staticfunc2(&T::writebuff,T))	{}
	size_t		writebuff(const void *buffer, size_t size)		{ return _writebuff(p, buffer, size); }
};

struct EncryptInterfaceDigest : EncryptInterface {
	malloc_block	(*_digest)(void*);
	template<typename T> EncryptInterfaceDigest(T *t) : EncryptInterface(t), _digest(make_staticfunc2(&T::digest,T)) {}
	malloc_block	digest()									{ return _digest(p); }
};

struct DecryptInterface : Interface, reader_mixin<DecryptInterface> {
	size_t		(*_readbuff)(void*, void*, size_t);
	template<typename T> DecryptInterface(T *t) : Interface(t), _readbuff(make_staticfunc2(&T::readbuff,T))	{}
	size_t		readbuff(void *buffer, size_t size)				{ return _readbuff(p, buffer, size); }
};

struct DecryptInterfaceDigest : DecryptInterface {
	malloc_block	(*_digest)(void*);
	template<typename T> DecryptInterfaceDigest(T *t) : DecryptInterface(t), _digest(make_staticfunc2(&T::digest,T)) {}
	malloc_block	digest()									{ return _digest(p); }
};

template<typename W> struct NoEncrypt {
	W		writer;
	template<typename WP> NoEncrypt(WP wp) : writer(wp) {}
	size_t			writebuff(const void *buffer, size_t size)	{ return writer.writebuff(buffer, size); }
	malloc_block	digest()									{ return malloc_block(); }
};

template<typename R> struct NoDecrypt {
	R		reader;
	template<typename RP> NoDecrypt(RP rp) : reader(rp) {}
	size_t			readbuff(void *buffer, size_t size)			{ return reader.readbuff(buffer, size); }
	malloc_block	digest()									{ return malloc_block(); }
};

struct HashInterface : EncryptInterface {
	malloc_block	(*_digest)(void*);
	uint32			block_size;

	template<typename T> static malloc_block digest_thunk(void *me) {
		malloc_block	r(const_memory_block(addr(((T*)me)->digest())));
		((T*)me)->reset();
		return r;
	}

	template<typename T> HashInterface(T *t) : EncryptInterface(t), _digest(&digest_thunk<T>), block_size(T::BLOCK_SIZE) {}
	malloc_block	digest()											{ return _digest(p); }
	template<typename...TT> malloc_block operator()(const TT&... tt)	{ iso::write(*this, tt...); return digest(); }
};

struct NoHash {
	enum {BLOCK_SIZE = 0};
	size_t				writebuff(const void *buffer, size_t size) { return int(size); }
	const_memory_block	digest() { return const_memory_block(); }
	void				reset() {}
};

template<typename A, typename B> struct Hash2 {
	enum {BLOCK_SIZE = A::BLOCK_SIZE};
	A	a;
	B	b;

	struct CODE {
		typename A::CODE	a;
		typename B::CODE	b;
		CODE(const typename A::CODE &_a, const typename B::CODE &_b) : a(_a), b(_b) {}
	};

	size_t	writebuff(const void *buffer, size_t size) {
		a.writebuff(buffer, size);
		b.writebuff(buffer, size);
		return int(size);
	}
	CODE	digest() {
		return CODE(a, b);
	}
	void	reset() {
		a.reset();
		b.reset();
	}
};

template<typename T>	size_t	block_size1(const T &t)						{ return sizeof(T); }
template<int N>			size_t	block_size1(const char (&t)[N])				{ return N - 1; }
size_t							block_size1(const char *t)					{ return strlen(t); }
size_t							block_size1(const const_memory_block &t)	{ return t.length(); }
size_t							block_size1(const memory_block &t)			{ return t.length(); }
size_t							block_size1(const malloc_block &t)			{ return t.length(); }

template<typename T> size_t block_size(const T &t) {
	return block_size1(t);
}
template<typename T, typename... TT> size_t block_size(const T &t, const TT&... tt) {
	return block_size1(t) + block_size(tt...);
}

template<typename T>	void	block_copy1(uint8 *out, const T &t)						{ memcpy(out, &t, sizeof(T)); }
template<int N>			void	block_copy1(uint8 *out, const char (&t)[N])				{ memcpy(out, t, N); }
void							block_copy1(uint8 *out, const char *t)					{ strcpy((char*)out, t); }
void							block_copy1(uint8 *out, const const_memory_block &t)	{ t.copy_to(out); }
void							block_copy1(uint8 *out, const memory_block &t)			{ t.copy_to(out); }
void							block_copy1(uint8 *out, const malloc_block &t)			{ t.copy_to(out); }

template<typename T> void block_copy(uint8 *out, const T &t) {
	block_copy1(out, t);
}
template<typename T, typename... TT> void block_copy(uint8 *out, const T &t, const TT&... tt) {
	block_copy1(out, t);
	block_copy(out + block_size1(t), tt...);
}


template<typename... TT> malloc_block block_concatenate(const TT&...tt) {
	malloc_block	b(block_size(tt...));
	block_copy(b, tt...);
	return b;
}

void block_copy_inc(memory_block &out, const const_memory_block &in) {
	uint32	n = min(out.size32(), in.size32());
	memcpy(out, in, n);
	out = out + n;
}
void block_xor_inc(memory_block &out, const const_memory_block &in) {
	uint32	n	= min(out.size32(), in.size32());
	uint8	*d	= out;
	for (const uint8 *s = in, *e = s + n; s != e;)
		*d++ ^= *s++;
	out = out + n;
}

/*
* the HMAC transform looks like:	T(K XOR opad, T(K XOR ipad, text))
* where K is an n byte key
* ipad is the byte 0x36 repeated 64 times
* opad is the byte 0x5c repeated 64 times
* text is the data being protected
*/

template<typename... TT> malloc_block HMAC(HashInterface &&h, const const_memory_block &key, const TT&...tt) {
	// if key is inter than 64 bytes reset it to key=MD5(key)
	if (key.length() > h.block_size) {
		h.write(key);
		return HMAC(move(h), h.digest(), tt...);
	}

	// store key in pad
	uint8	pad[256];
	clear(pad);
	key.copy_to(pad);

	// perform inner MD5
	for (int i = 0; i < h.block_size; i++)	// XOR key with ipad
		pad[i] ^= 0x36;

	h.writebuff(pad, h.block_size);		// start with inner pad
	h.write(tt...);					// then text of datagram
	malloc_block code = h.digest();		// finish up 1st pass

	// perform outer MD5
	for (int i = 0; i < h.block_size; i++)	// XOR key with opad
		pad[i] ^= 0x36 ^ 0x5c;

	h.writebuff(pad, h.block_size);		// start with outer pad
	h.write(code);						// then results of 1st hash
	return h.digest();					// finish up 2nd pass
}

void P_hash(HashInterface &&h, const memory_block &out, const const_memory_block &secret, const const_memory_block &seed) {
	malloc_block	A = HMAC(move(h), secret, seed);
	for (memory_block i = out; i.length(); ) {
		block_copy_inc(i, HMAC(move(h), secret, A, seed));
		A = HMAC(move(h), secret, A);
	}
}
void P_hash_xor(HashInterface &&h, const memory_block &out, const const_memory_block &secret, const const_memory_block &seed) {
	malloc_block	A = HMAC(move(h), secret, seed);
	for (memory_block i = out; i.length(); ) {
		block_xor_inc(i, HMAC(move(h), secret, A, seed));
		A = HMAC(move(h), secret, A);
	}
}

template<typename... TT> void PRF(HashInterface &&h, const memory_block &out, const const_memory_block &secret, const TT&...tt) {
	P_hash(move(h), out, secret, block_concatenate(tt...));
}

void PRF_MD5_SHA1(const memory_block &out, const const_memory_block &secret, const const_memory_block &seed) {
	//The secret is partitioned into two halves S1 and S2 with the possibility of one shared byte
	uint32				slen	= (secret.size32() + 1) / 2;

	P_hash(new MD5, out, secret.slice_to(slen), seed);
	P_hash_xor(new SHA1, out, secret.slice(secret.length() - slen), seed);
}

template<typename... TT> void PRF_MD5_SHA1(const memory_block &out, const const_memory_block &secret, const TT&...tt) {
	PRF_MD5_SHA1(out, secret, const_memory_block(block_concatenate(tt...)));
}

template<typename... TT> void ExpandKey(const memory_block &out, const const_memory_block &secret, const TT&...tt) {
	char	pad[16];
	SHA1	sha1;
	MD5		md5;

	memory_block p	= out;
	for (uint32 i = 0; p.length(); i++) {
		memset(pad, 'A' + i, i + 1);

		//SHA(pad + secret + random)
		sha1.reset();
		sha1.writebuff(pad, i + 1);
		sha1.write(secret);
		write(sha1, tt...);

		//MD5(secret + SHA(pad + secret + random))
		md5.reset();
		md5.write(secret);
		md5.write(sha1.digest());

		block_copy_inc(p, const_memory_block(addr(md5.digest())));
	}
}

HashInterface get_hash_context(HashAlgorithm mac) {
	switch (mac) {
		case hash_md5:			return new MD5;
		case hash_sha1:			return new SHA1;
		case hash_sha224:		return new SHA224;
		case hash_sha256:		return new SHA256;
		case hash_sha384:		return new SHA384;
		case hash_sha512:		return new SHA512;
	#ifdef SSL_HASH_GOST
//		case hash_gost89:		return new gost28147;
//		case hash_gost89_12:	return new gost28147;
		case hash_gost94:		return new gostr3411;
//		case hash_gost12_256:	return new SHA512;
//		case hash_gost12_512:	return new SHA512;
	#endif
			ISO_ASSERT(0);
		case hash_md5_sha1:		return new Hash2<MD5, SHA1>;
		default:				return new NoHash;
	}
}

EncryptInterface get_encrypt_context(const memory_block &out, BulkEncryption enc, const const_memory_block &secret, const const_memory_block &nonce) {
	switch (enc) {
		case NO_ENCRYPT:			return new NoEncrypt<memory_writer>(out);
	#ifdef SSL_ENCRYPT_RC
		case RC2_CBC_40:			return new CBC_encrypt<encryptor<RC2>, 8, memory_writer>(secret, out, nonce);
		case RC4_40:
		case RC4_128:				return new Stream_encrypt<RC4, 1024, memory_writer>(secret, out);
	#endif
	#ifdef SSL_ENCRYPT_DES
		case DES_40_CBC:
		case DES_64_CBC:			return new CBC_encrypt<encryptor<DES>, 8, memory_writer>(secret, out, nonce);
		case DES_192_EDE_CBC:		return new CBC_encrypt<encryptor<DESx3>, 8, memory_writer>(secret, out, nonce);
	#endif
	#ifdef SSL_ENCRYPT_AES
		case AES_128_CBC:
		case AES_256_CBC:			return new CBC_encrypt<AES_encrypt, 16, memory_writer>(secret, out, nonce);
	#endif
	#ifdef SSL_ENCRYPT_CAMELLIA
		case CAMELLIA_128_CBC:
		case CAMELLIA_256_CBC:		return new CBC_encrypt<encryptor<CAMELLIA>, 16, memory_writer>(secret, out, nonce);
	#endif
	#ifdef SSL_ENCRYPT_GOST
		case GOST_89_CNT:			return new CTR_encrypt<GOST28147_encrypt, 8, memory_writer>(secret, nonce, out);
		case GOST_89_CNT_12:		return new NoEncrypt<memory_writer>(out);
	#endif
		default:					ISO_ASSERT(0); return new NoEncrypt<memory_writer>(out);
	}
};

DecryptInterface get_decrypt_context(const const_memory_block &in, BulkEncryption enc, const const_memory_block &secret, const const_memory_block &nonce) {
	switch (enc) {
		case NO_ENCRYPT:			return new NoDecrypt<memory_reader>(in);
	#ifdef SSL_ENCRYPT_RC
		case RC2_CBC_40:			return new CBC_decrypt<decryptor<RC2>, 8, memory_reader>(secret, in, nonce);
		case RC4_40:
		case RC4_128:				return new Stream_decrypt<RC4, 1024, memory_reader>(secret, in);
	#endif
	#ifdef SSL_ENCRYPT_DES
		case DES_40_CBC:
		case DES_64_CBC:			return new CBC_decrypt<decryptor<DES>, 8, memory_reader>(secret, in, nonce);
		case DES_192_EDE_CBC:		return new CBC_decrypt<decryptor<DESx3>, 8, memory_reader>(secret, in, nonce);
	#endif
	#ifdef SSL_ENCRYPT_AES
		case AES_128_CBC:
		case AES_256_CBC:			return new CBC_decrypt<AES_decrypt, 16, memory_reader>(secret, in, nonce);
	#endif
	#ifdef SSL_ENCRYPT_CAMELLIA
		case CAMELLIA_128_CBC:
		case CAMELLIA_256_CBC:		return new CBC_decrypt<decryptor<CAMELLIA>, 16, memory_reader>(secret, in, nonce);
	#endif
	#ifdef SSL_ENCRYPT_GOST
		case GOST_89_CNT:			return new CTR_decrypt<GOST28147_decrypt, 8, memory_reader>(secret, nonce, in);
		case GOST_89_CNT_12:		return new NoDecrypt<memory_reader>(in);
	#endif
		default:					ISO_ASSERT(0); return new NoDecrypt<memory_reader>(in);
	}
};

EncryptInterfaceDigest get_encrypt_context(const memory_block &out, BulkEncryption enc, const const_memory_block &secret, const const_memory_block &nonce, const const_memory_block &additional, uint32 mac_size, streamptr length) {
	switch (enc) {
	#ifdef SSL_ENCRYPT_AES
		case AES_128_GCM:
		case AES_256_GCM:			return new GCM_encrypt<AES_encrypt, 16, memory_writer>(secret, out, nonce, additional, mac_size, length);
		case AES_128_CCM:
		case AES_256_CCM:			return new CCM_encrypt<AES_encrypt, 16, memory_writer>(secret, out, nonce, additional, mac_size, length);
		case AES_128_CCM_8:
		case AES_256_CCM_8:			return new CCM_encrypt<AES_encrypt, 8, memory_writer>(secret, out, nonce, additional, mac_size, length);
	#endif
	#ifdef SSL_ENCRYPT_CHACHA
		case CHACHA_20_POLY_1305:	return new NoEncrypt<memory_writer>(out);
	#endif
		default:					ISO_ASSERT(0); return new NoEncrypt<memory_writer>(out);
	}
};

DecryptInterfaceDigest get_decrypt_context(const const_memory_block &in, BulkEncryption enc, const const_memory_block &secret, const const_memory_block &nonce, const const_memory_block &additional, uint32 mac_size, streamptr length) {
	switch (enc) {
	#ifdef SSL_ENCRYPT_AES
		case AES_128_GCM:
		case AES_256_GCM:			return new GCM_decrypt<AES_encrypt, 16, memory_reader>(secret, in, nonce, additional, mac_size, length);
		case AES_128_CCM:
		case AES_256_CCM:			return new CCM_decrypt<AES_encrypt, 16, memory_reader>(secret, in, nonce, additional, mac_size, length);
		case AES_128_CCM_8:
		case AES_256_CCM_8:			return new CCM_decrypt<AES_encrypt, 8, memory_reader>(secret, in, nonce, additional, mac_size, length);
	#endif
	#ifdef SSL_ENCRYPT_CHACHA
		case CHACHA_20_POLY_1305:	return new NoDecrypt<memory_reader>(in);
	#endif
		default:					ISO_ASSERT(0); return new NoDecrypt<memory_reader>(in);
	}
};

//-----------------------------------------------------------------------------
//	Certificates
//-----------------------------------------------------------------------------

struct PemEntity : malloc_block {
	string			type;
//	PemEntity() {}
//	PemEntity(string &&type, malloc_block&& data) : malloc_block(move(data)), type(move(type)) {}
};

struct CertificateDesc {
	dynamic_array<PemEntity>	chain;
	PemEntity					priv_key;
	CertificateType				type;	// End entity certificate type
	SignatureAndHashAlgorithm	alg;

	CertificateDesc()	{}
	CertificateDesc(const char *_chain, const char *_priv_key);
	bool Valid(CertificateType _type, const dynamic_array<SignatureAndHashAlgorithm> &algs) const;
	bool Valid(const dynamic_array<CertificateType> &types, const dynamic_array<SignatureAndHashAlgorithm> &algs, const dynamic_array<CertificateAuthority> &ca) const;
	bool Verify(const dynamic_array<CertificateAuthority> &ca) const;

	void init();
	bool write(PacketBuilder &w) const;
	bool read(PacketReader &r);
};

bool ReadPem(PemEntity &entity, string_scan &&ss) {
	if (!ss.scan_skip("-----BEGIN "))
		return false;

	count_string	type	= ss.get_token(~char_set('-'));
	const char*		begin	= ss.scan_skip('\n');
	const char		*end	= ss.scan_skip("-----END ");

	if (!end)
		return false;

	(malloc_block&)entity = transcode(base64_decoder(), const_memory_block(begin, end));
	entity.type	= type;
	return true;
}

void ReadPem(dynamic_array<PemEntity> &array, string_scan &&ss) {
	while (ReadPem(array.push_back(), move(ss)));
	array.pop_back();
}

CertificateDesc::CertificateDesc(const char *_chain, const char *_priv_key) {
	ReadPem(chain, _chain);
	ReadPem(priv_key, _priv_key);

	X509::Certificate	c;
	ASN1::Read(memory_reader(chain[0]), c);
	#if 0
	dynamic_memory_writer	mo;
	ASN1::Write(mo, c);
	const_memory_block	m = mo;
	ISO_ASSERT(*chain[0] == *m);
	#endif
	init();
}

void CertificateDesc::init() {
	static struct {const char *name; HashAlgorithm alg;} hash_algorithms[] = {
		{"none",		hash_none},
		{"md5",			hash_md5},
		{"sha1",		hash_sha1},
		{"sha224",		hash_sha224},
		{"sha256",		hash_sha256},
		{"sha384",		hash_sha384},
		{"sha512",		hash_sha512},
		{"gost89",		hash_gost89},
		{"gost94",		hash_gost94},
		{"md5_sha1",	hash_md5_sha1},
		{"md2",			hash_md2},
	};

	static struct {const char *name; SignatureAlgorithm alg;} signature_algorithms[] = {
		{"rsa",			sig_rsa},
		{"dsa",			sig_dsa},
		{"ecdsa",		sig_ecdsa},
	};

	static struct {const char *name; CertificateType alg;} exchange_algorithms[] = {
		{"rsaEncryption",	rsa_sign},
		{"dsaEncryption",	dss_sign},
		{"ecPublicKey",		ecdsa_sign},
	};

	X509::Certificate	cert;
	ASN1::Read(memory_reader(chain[0]), cert);
	const char *s = cert.TbsCertificate.SubjectPublicKeyInfo.Algorithm.Algorithm.oid->name;

	for (auto &i : exchange_algorithms) {
		if (istr(s) == i.name) {
			type = i.alg;
			break;
		}
	}

	alg.hash	= hash_none;
	alg.sig		= sig_anonymous;

	s	= cert.TbsCertificate.signature.Algorithm.oid->name;
	if (const char *w = string_find(s, "With")) {
		auto	hash	= istr(s, w);
		auto	sig		= istr(w + 4, string_end(w));
		for (auto &i : hash_algorithms) {
			if (hash == i.name) {
				alg.hash = i.alg;
				break;
			}
		}
		if (alg.hash == hash_none) {
			swap(hash, sig);
			for (auto &i : hash_algorithms) {
				if (hash == i.name) {
					alg.hash = i.alg;
					break;
				}
			}
		}

		for (auto &i : signature_algorithms) {
			if (sig.begins(i.name)) {
				alg.sig = i.alg;
				break;
			}
		}
	}
}

bool CertificateDesc::Verify(const dynamic_array<CertificateAuthority> &ca) const {
	X509::Certificate	cert;
	if (!ASN1::Read(memory_reader(chain.back()), cert))
		return false;

	for (auto &i : ca) {
		if (*i.get_buffer() == *cert.TbsCertificate.Issuer.get_buffer())
			return true;
	}
	return false;
}

bool CertificateDesc::Valid(CertificateType _type, const dynamic_array<SignatureAndHashAlgorithm> &algs) const {
	return	type == _type && find_check(algs, alg);
}

bool CertificateDesc::Valid(const dynamic_array<CertificateType> &types, const dynamic_array<SignatureAndHashAlgorithm> &algs, const dynamic_array<CertificateAuthority> &ca) const {
	if (!find_check(types, type) || !find_check(algs, alg))
		return false;

	return Verify(ca);
}

bool CertificateDesc::write(PacketBuilder &w) const {
	chunk_writer<24>	h(w);
	for (auto &i : chain) {
		uint24be	len = i.size32();
		w.write(len);
		w.writebuff(i, i.length());
	}
	return true;
}
bool CertificateDesc::read(PacketReader &r) {
	chunk_reader<24>	h(r);
	while (r.remaining()) {
		uint32	len = r.get<uint24be>();
		r.readbuff(chain.push_back().create(len), len);
	}
	init();
	return true;
}

bool check_host_names(const char *a, const char *b) {
	for (const char	*i = a + strlen(a), *j = b + strlen(b); i >= a && j >= b; --i, --j) {
		//Wildcard certificate found?
		if (*i == '*' && i == a)
			return true;

		//Perform case insensitive character comparison
		if (to_lower(*i) != *j)
			return false;
	}
	return true;
}

struct StaticCertificate : CertificateDesc, static_list<StaticCertificate> {
	StaticCertificate(const char *_chain, const char *_priv_key) : CertificateDesc(_chain, _priv_key) {}

	static CertificateDesc *find(CertificateType type, const dynamic_array<SignatureAndHashAlgorithm> &algs) {
		for (auto i = begin(); i; ++i) {
			if (i->Valid(type, algs))
				return i;
		}
		return 0;
	}
	static CertificateDesc *find(SignatureAlgorithm sig, const dynamic_array<SignatureAndHashAlgorithm> &algs) {
		CertificateType type;
		switch (sig) {
			case sig_rsa:	type	= rsa_sign; break;
			case sig_dsa:	type	= dss_sign;	break;
			case sig_ecdsa:	type	= ecdsa_sign; break;
			default: return 0;
		}
			return find(type, algs);
	}
	static CertificateDesc *find(const dynamic_array<CertificateType> &types, const dynamic_array<SignatureAndHashAlgorithm> &algs, const dynamic_array<CertificateAuthority> &ca) {
		for (auto i = begin(); i; ++i) {
			if (i->Valid(types, algs, ca))
				return i;
		}
		return 0;
	}
};

//-----------------------------------------------------------------------------
//	Signing
//-----------------------------------------------------------------------------

struct signature {
	SignatureAndHashAlgorithm	algorithm;
	opaque_array<16>			data;
	signature()								{}
	signature(SignatureAlgorithm sig, HashAlgorithm hash, malloc_block &&_data)	: data(move(_data)) { algorithm.sig = sig; algorithm.hash = hash; }
	bool	write(PacketBuilder &w) const	{ return w.write(algorithm, data); }
	bool	read(PacketReader &r)			{ return r.read(algorithm, data); }
};

struct Signer {
	virtual ~Signer()	{}
	virtual	signature	sign(HashAlgorithm hash, const const_memory_block &digest, vrng &&rng) = 0;
	virtual	bool		verify(const signature &sig, const const_memory_block &digest) = 0;
};

template<SignatureAlgorithm X> struct SignerT : Signer {};

#ifdef SSL_SIGN_RSA
template<> struct SignerT<sig_rsa> : Signer, RSA {
	SignerT(Connection *con, const CertificateDesc *certificate) {
		if (certificate->priv_key) {
			X509::RSAPrivateKey	private_key;
			ASN1::Read(memory_reader(certificate->priv_key), private_key);
			RSA::init(private_key.N, private_key.E);
			RSA::set_private(
				private_key.D,
				private_key.P,
				private_key.Q,
				private_key.DP,
				private_key.DQ,
				private_key.QP
			);
		} else {
			X509::Certificate	cert;
			X509::RSAPublicKey	public_key;
			ASN1::Read(memory_reader(certificate->chain[0]), cert);
			ASN1::Read(memory_reader(cert.TbsCertificate.SubjectPublicKeyInfo.SubjectPublicKey.get_buffer()), public_key);
			RSA::init(public_key.N, public_key.E);
			//generate_private(con->rand);
		}
	}
	virtual	SSL::signature	sign(HashAlgorithm hash, const const_memory_block &digest, vrng &&rng) {
		X509::Signature	a;
		a.Algorithm.Algorithm	= hash_info[hash].oid;
		a.signature				= digest;

		dynamic_memory_writer	mo;
		ASN1::Write(mo, a);
		return SSL::signature(sig_rsa, hash, encode_signature(mo.data()));
	}
	virtual	bool		verify(const SSL::signature &sig, const const_memory_block &digest) {
		X509::Signature	a;
		return sig.algorithm.sig == sig_rsa
			&& ASN1::Read(memory_reader(decode_signature(sig.data)), a)
			&& a.Algorithm.Algorithm == hash_info[sig.algorithm.hash].oid
			&& *a.signature.get_buffer() == *digest;
	}
};
#endif

#ifdef SSL_SIGN_DSA
template<> struct SignerT<sig_dsa> : Signer, DSA {
	SignerT(Connection *con, const CertificateDesc *certificate) {
		if (certificate->priv_key) {
			X509::DSAPrivateKey	private_key;
			ASN1::Read(memory_reader(certificate->priv_key), private_key);
			P			= private_key.P;
			Q			= private_key.Q;
			G			= private_key.G;
			pub_key		= private_key.pub_key;
			priv_key	= private_key.priv_key;
		} else {
			generate_private(con->rand);

		}
	}
	virtual	SSL::signature	sign(HashAlgorithm hash, const const_memory_block &digest, vrng &&rng) {
		DSA::signature		sig1 = DSA::sign(digest, move(rng));
		X509::DSASignature	sig2;
		sig2.R	=	sig1.r;
		sig2.S	=	sig1.s;

		dynamic_memory_writer	mo;
		ASN1::Write(mo, sig2);
		return SSL::signature(sig_dsa, hash, mo);
	}
	virtual	bool		verify(const SSL::signature &sig, const const_memory_block &digest) {
		X509::DSASignature	sig2;
		return sig.algorithm.sig == sig_dsa
			&& ASN1::Read(memory_reader(sig.data), sig2)
			&& DSA::verify(digest, DSA::signature(sig2.R, sig2.S));	}
};
#endif

#ifdef SSL_SIGN_ECDSA
template<> struct SignerT<sig_ecdsa> : Signer {
	const EC_curve		*curve;
	mpi					priv_key;
	EC_point			pub_key;

	SignerT(Connection *con, const CertificateDesc *certificate) {
		X509::Certificate	cert;
		X509::ECParameters	parameters;

		ASN1::Read(memory_reader(certificate->chain[0]), cert);
		ASN1::Read(cert.TbsCertificate.SubjectPublicKeyInfo.Algorithm.Parameters, parameters);

		curve		= EC_curve::get_named(parameters.namedcurve);
		ISO_ASSERT(curve);
		pub_key		= curve->load(cert.TbsCertificate.SubjectPublicKeyInfo.SubjectPublicKey.get_buffer());

		if (certificate->priv_key) {
			X509::ECPrivateKey	private_key;
			ASN1::Read(memory_reader(certificate->priv_key), private_key);
			priv_key	= private_key.privateKey.get_buffer();
		} else {
			priv_key	= mpi::random(con->rand, curve->q.num_bits());
			if (priv_key >= curve->q)
				priv_key >>= 1;
		}
	}
	virtual	signature	sign(HashAlgorithm hash, const const_memory_block &digest, vrng &&rng)  {
		ECDSA_signature		sig1(curve, move(rng), priv_key, digest);
		X509::DSASignature	sig2;
		sig2.R	= sig1.r;
		sig2.S	= sig1.s;

		dynamic_memory_writer	mo;
		ASN1::Write(mo, sig2);
		return signature(sig_ecdsa, hash, mo);
	}
	virtual	bool		verify(const signature &sig, const const_memory_block &digest) {
		X509::DSASignature	sig2;
		return sig.algorithm.sig == sig_ecdsa
			&& ASN1::Read(memory_reader(sig.data), sig2)
			&& ECDSA_signature(sig2.R, sig2.S).verify(curve, pub_key, digest);
	}
};
#endif

Signer *GetSigner(Connection *con, SignatureAlgorithm sig, const CertificateDesc *certificate) {
	switch (sig) {
		case sig_rsa:	return new SignerT<sig_rsa>(con, certificate);
		case sig_dsa:	return new SignerT<sig_dsa>(con, certificate);
		case sig_ecdsa:	return new SignerT<sig_ecdsa>(con, certificate);
		default:		return 0;
	}
}
Signer *GetSigner(Connection *con, const CertificateDesc *certificate) {
	if (!certificate)
		return 0;

	switch (certificate->type) {
		case rsa_sign:
		case rsa_fixed_dh:
		case rsa_ephemeral_dh_RESERVED:
		case rsa_fixed_ecdh:
			return new SignerT<sig_rsa>(con, certificate);
		case dss_sign:
		case dss_fixed_dh:
		case dss_ephemeral_dh_RESERVED:
			return new SignerT<sig_dsa>(con, certificate);
		case ecdsa_sign:
		case ecdsa_fixed_ecdh:
			return new SignerT<sig_ecdsa>(con, certificate);
		case fortezza_dms_RESERVED:
		default:
			return 0;
	}
}
//-----------------------------------------------------------------------------
//	key exchange algorithms
//-----------------------------------------------------------------------------

struct KeyExchanger {
	// run on client
	virtual bool	client_begin(Connection *con, CipherSuiteInfo *info, CertificateDesc *_certificate) { return true; }
	virtual bool	read_server (PacketReader  &r, Connection *con)	{ return false; }
	virtual bool	write_client(PacketBuilder &w, Connection *con)	{ return false; }

	// run on server
	virtual bool	server_begin(Connection *con, CipherSuiteInfo *info, CertificateDesc *certificate) { return true; }
	virtual bool	write_server(PacketBuilder &w, Connection *con, const ClientHello *hello)	{ return false; }
	virtual bool	read_client (PacketReader  &r, Connection *con)	{ return false; }
};

template<KeyExchangeAlgorithm X> struct KeyExchangerT : KeyExchanger {};

#ifdef SSL_KEYEX_DH
//-----------------------------------------------------------------------------
//	diffie hellman
//-----------------------------------------------------------------------------

const char server_dh_params[] =
"-----BEGIN DH PARAMETERS-----\n"
"MIGHAoGBAKHqsQ+k2uy9n0fn0QW6xtPmPD5Hc5LupavUKRyAK6U7T7AznJfvqIwS\n"
"nxT0/kxc2V1rOEN9k3m4DycPkAMfFjW1E2O1FrOedqHTMcLf72d5LPC5tApgcG3T\n"
"KfMlDxen9kg7HiQySrT9jsW5VQ2PNUKQD+coOQrz0S1W76D83phrAgEC\n"
"-----END DH PARAMETERS-----\n"
;

template<> struct KeyExchangerT<xchg_dh> : KeyExchanger {
	DH	dh;

	virtual bool	client_begin(Connection *con, CipherSuiteInfo *info, CertificateDesc *certificate) {
		return true;
	}
	virtual bool	read_server(PacketReader &r, Connection *con)	{
		opaque_array<16>	p, g, peer_pub_key;
		if (!read(r, p, g, peer_pub_key))
			return false;
		dh.init(p, g);
		dh.generate_private(con->rand);
		con->pre_master_secret	= dh.shared_secret(peer_pub_key);
		return true;
	}
	virtual bool	write_client(PacketBuilder &w, Connection *con)	{
		opaque_array<16>	local_pub_key = dh.get_public();
		return write(w, local_pub_key);
	}

	virtual bool	server_begin(Connection *con, CipherSuiteInfo *info, CertificateDesc *certificate) {
		PemEntity			pem;
		X509::DHParameters	params;
		if (!ReadPem(pem, server_dh_params) || !ASN1::Read(memory_reader(pem), params))
			return false;
		dh.init(params.P, params.G);
		dh.generate_private(con->rand, params.length.exists() ? get(params.length) : 0);
		return true;
	}
	virtual bool	write_server(PacketBuilder &w, Connection *con, const ClientHello *hello)	{
		return w.write(
			opaque_array<16>(dh.p),
			opaque_array<16>(dh.g),
			opaque_array<16>(dh.get_public())
		);
	}
	virtual bool	read_client(PacketReader &r, Connection *con)	{
		opaque_array<16>	peer_pub_key;
		if (!read(r, peer_pub_key))
			return false;
		con->pre_master_secret	= dh.shared_secret(peer_pub_key);
		return true;
	}
};
#endif

#ifdef SSL_KEYEX_RSA
//-----------------------------------------------------------------------------
//	RSA
//-----------------------------------------------------------------------------

template<> struct KeyExchangerT<xchg_rsa> : KeyExchanger {
	struct PreMasterSecret {
		Version2	client_version;	//latest(newest) version supported by the client
		uint8		random[46];
	};

	RSA		rsa;

	virtual bool	client_begin(Connection *con, CipherSuiteInfo *info, CertificateDesc *certificate) {
		if (certificate) {
			X509::Certificate	cert;
			X509::RSAPublicKey	key;
			if (ASN1::Read(memory_reader(certificate->chain[0]), cert) && ASN1::Read(memory_reader(cert.TbsCertificate.SubjectPublicKeyInfo.SubjectPublicKey.get_buffer()), key))
				rsa.init(key.N, key.E);
		}
		return true;
	}
	virtual bool	read_server(PacketReader &r, Connection *con)	{
		opaque_array<16>	n;
		opaque_array<16>	e;
		if (!read(r, n, e))
			return false;
		rsa.init(n, e);
		return true;
	}
	virtual bool	write_client(PacketBuilder &w, Connection *con)	{
		PreMasterSecret	*pms = con->pre_master_secret.create(sizeof(PreMasterSecret));
		pms->client_version = VER_MAX;
		con->rand.fill(pms->random);

		opaque_array<16>	encrypted_pms(rsa.N.num_bytes());
		rsa.encrypt(RSA::PUBLIC, RSA::V15, con->pre_master_secret, encrypted_pms, con->rand);

		if (con->version > VER_SSL3)
			return write(w, encrypted_pms);
		return w.writebuff(encrypted_pms, encrypted_pms.length());
	}

	virtual bool	server_begin(Connection *con, CipherSuiteInfo *info, CertificateDesc *certificate) {
		if (!certificate)
			return false;

		X509::RSAPrivateKey	private_key;
		if (!ASN1::Read(memory_reader(certificate->priv_key), private_key))
			return false;

		rsa.init(private_key.N, private_key.E);
		rsa.D	= private_key.D;
		rsa.P	= private_key.P;
		rsa.Q	= private_key.Q;

		rsa.DP	= private_key.DP;
		rsa.DQ	= private_key.DQ;
		rsa.QP	= private_key.QP;
		return true;
	}
	virtual bool	write_server(PacketBuilder &w, Connection *con, const ClientHello *hello)	{
		return w.write(
			opaque_array<16>(rsa.N),
			opaque_array<16>(rsa.E)
		);
	}
	virtual bool	read_client(PacketReader &r, Connection *con)	{
		opaque_array<16>	encrypted_pms;
		if (!read(r, encrypted_pms))
			return false;

		PreMasterSecret	*pms = con->pre_master_secret.create(sizeof(PreMasterSecret));
		rsa.decrypt(RSA::PRIVATE, RSA::V15, encrypted_pms, con->pre_master_secret);
		return true;
	}
};
#endif

#ifdef SSL_KEYEX_SRP
//-----------------------------------------------------------------------------
//	SRP
//-----------------------------------------------------------------------------

dynamic_array<SRP_entry>	srp_entries;

template<> struct Extension::Struct<Extension::srp> {
	pascal_string	user;
};

template<> struct KeyExchangerT<xchg_srp> : KeyExchanger {
	SRP					srp;
	mpi					A, B, key;
	const SRP_entry		*entry;

	static const char *user;
	static const char *pass;

	virtual bool	read_server(PacketReader &r, Connection *con)	{
		opaque_array<16>	N;
		opaque_array<16>	g;
		opaque_array<8>		s;
		opaque_array<16>	_B;
		if (!read(r, N, g, s, _B))
			return false;

		srp = SRP(N, g);
		B	= _B;

		if (!srp.CheckNg() || !srp.Check(B))
			return false;
		
		A	= srp.ClientCalcA();
		key	= srp.ClientKey(A, B, s, user, pass);
		return true;
	}

	virtual bool	write_server(PacketBuilder &w, Connection *con, const ClientHello *hello)	{
		auto *ext = get(hello->extensions).get<Extension::srp>();
		if (!ext)
			return false;

		entry = find_if(srp_entries, [ext](const SRP_entry &e) { return e.user == ext->user; });
		if (!entry)
			return false;

		srp	= SRP(*SRP_group::Find("1024"));
		B	= srp.ServerCalcB(entry->verifier);

		return w.write(
			opaque_array<16>(srp.p.save_all()),
			opaque_array<16>(srp.g.save_all()),
			opaque_array<8>(entry->salt.save_all()),
			opaque_array<16>(B.save_all())
		);
	}

	virtual bool	read_client(PacketReader &r, Connection *con)	{
		opaque_array<16>	_A;
		if (!r.read(_A))
			return false;

		A	= _A;
		if (!srp.Check(A))
			return false;

		key	= srp.ServerKey(A, B, entry->verifier);
		return true;
	}
	virtual bool	write_client(PacketBuilder &w, Connection *con)	{
		return w.write(opaque_array<16>(A.save_all()));
	}
};

const char *KeyExchangerT<xchg_srp>::user;
const char *KeyExchangerT<xchg_srp>::pass;
#endif

#ifdef SSL_KEYEX_PSK
//-----------------------------------------------------------------------------
//	PSK
//-----------------------------------------------------------------------------
template<> struct KeyExchangerT<xchg_psk> : KeyExchanger {
	struct Secrets {
		PreMasterSecret		pms;
		opaque_array<32>	other;
	};
	Secrets	pre_master_secret;

	virtual bool	read_client(PacketReader &r, Connection *con)	{
		return false;
	}
	virtual bool	write_client(PacketBuilder &w, Connection *con)	{
		return write(w, pre_master_secret);
	}

	virtual bool	read_server(PacketReader &r, Connection *con)	{
		return false;
	}

	virtual bool	write_server(PacketBuilder &w, Connection *con, const ClientHello *hello)	{
		return false;
	}
};

#endif

#ifdef SSL_KEYEX_ECDH
//-----------------------------------------------------------------------------
//	ECDH
//-----------------------------------------------------------------------------

enum CurveName : uint16 {
	sect163k1						= 1,
	sect163r1						= 2,
	sect163r2						= 3,
	sect193r1						= 4,
	sect193r2						= 5,
	sect233k1						= 6,
	sect233r1						= 7,
	sect239k1						= 8,
	sect283k1						= 9,
	sect283r1						= 10,
	sect409k1						= 11,
	sect409r1						= 12,
	sect571k1						= 13,
	sect571r1						= 14,
	secp160k1						= 15,
	secp160r1						= 16,
	secp160r2						= 17,
	secp192k1						= 18,
	secp192r1						= 19,
	secp224k1						= 20,
	secp224r1						= 21,
	secp256k1						= 22,
	secp256r1						= 23,
	secp384r1						= 24,
	secp521r1						= 25,
	brainpoolP256r1					= 26,
	brainpoolP384r1					= 27,
	brainpoolP512r1					= 28,
	ecdh_x25519						= 29,	//TEMPORARY
	ecdh_x448						= 30,	//TEMPORARY
	ffdhe2048						= 0x0100,
	ffdhe3072						= 0x0101,
	ffdhe4096						= 0x0102,
	ffdhe6144						= 0x0103,
	ffdhe8192						= 0x0104,
	arbitrary_explicit_prime_curves	= 0xff01,
	arbitrary_explicit_char2_curves	= 0xff02,
};

enum ECPointFormat : uint8 {
	uncompressed					= 0,
	ansiX962_compressed_prime		= 1,
	ansiX962_compressed_char2		= 2,
};

static const struct NamedCurve {CurveName id; const char *name;} named_curves[] = {
//	{sect163k1,			"sect163k1"			},
//	{sect163r1,			"sect163r1"			},
//	{sect163r2,			"sect163r2"			},
//	{sect193r1,			"sect193r1"			},
//	{sect193r2,			"sect193r2"			},
//	{sect233k1,			"sect233k1"			},
//	{sect233r1,			"sect233r1"			},
//	{sect239k1,			"sect239k1"			},
//	{sect283k1,			"sect283k1"			},
//	{sect283r1,			"sect283r1"			},
//	{sect409k1,			"sect409k1"			},
//	{sect409r1,			"sect409r1"			},
//	{sect571k1,			"sect571k1"			},
//	{sect571r1,			"sect571r1"			},
	{secp160k1,			"secp160k1"			},
	{secp160r1,			"secp160r1"			},
	{secp160r2,			"secp160r2"			},
	{secp192k1,			"secp192k1"			},
	{secp192r1,			"prime192v1"		},
	{secp224k1,			"secp224k1"			},
	{secp224r1,			"secp224r1"			},
	{secp256k1,			"secp256k1"			},
	{secp256r1,			"prime256v1"		},
	{secp384r1,			"prime384v1"		},
	{secp521r1,			"prime512v1"		},
	{brainpoolP256r1,	"brainpoolP256r1"	},
	{brainpoolP384r1,	"brainpoolP384r1"	},
	{brainpoolP512r1,	"brainpoolP512r1"	},
};

static const CurveName allowed_curves[] = {
	secp160k1,
	secp160r1,
	secp160r2,
	secp192k1,
	secp192r1,
	secp224k1,
	secp224r1,
	secp256k1,
	secp256r1,
	secp384r1,
	secp521r1,
	brainpoolP256r1,
	brainpoolP384r1,
	brainpoolP512r1,
};

static const ECPointFormat allowed_ec_point_formats[] = {
	uncompressed,
};

const NamedCurve *get_curve(const char *name) {
	for (auto &i : named_curves) {
		if (str(i.name) == name)
			return &i;
	}
	return 0;
}

const NamedCurve *get_curve(CurveName id) {
	for (auto &i : named_curves) {
		if (i.id == id)
			return &i;
	}
	return 0;
}

template<> struct Extension::Struct<Extension::supported_groups> : chunk<dynamic_array<BE(CurveName)>, 16> {
	template<typename R> Struct(const R &r) { append(r); }
};
template<> struct Extension::Struct<Extension::ec_point_formats> : chunk<dynamic_array<ECPointFormat>, 8> {
	template<typename R> Struct(const R &r) { append(r); }
};

template<> struct KeyExchangerT<xchg_ecdh> : KeyExchanger {
	ECDH	ecdh;

	enum ECCurveType : uint8 {
		explicit_prime	= 1,
		explicit_char2	= 2,
		named_curve		= 3,
	};
	enum ECBasisType : uint8 {
		trinomial		= 0,
		pentanomial		= 1,
	};
	struct ECCurve {
		opaque_array<8>	a, b;
	};

	ECCurveType		type;
	BE(CurveName)	id;

	virtual bool	client_begin(Connection *con, CipherSuiteInfo *info, CertificateDesc *certificate) {
		return true;
	}

	virtual bool	read_server(PacketReader &r, Connection *con)	{
		if (!read(r, type))
			return false;
		if (type == named_curve) {
			read(r, id);
			ecdh.init(*EC_curve::get_named(get_curve(id)->name));
		} else {
			opaque_array<8>		p, a, b, g, q, h;
			if (type == explicit_prime) {
				read(r, p);
			} else {
				uint16be				m;
				ECBasisType				basis;
				opaque_array<8>			k[3];
				read(r, m, basis);
				readn(r, k, basis == pentanomial ? 3 : 1);
			}
			read(r, a, b, g, q, h);
		}

		ecdh.generate_private(con->rand);

		opaque_array<8> peer_pub_key;
		if (!r.read(peer_pub_key))
			return false;
		con->pre_master_secret	= ecdh.shared_secret(peer_pub_key);
		return true;
	}
	virtual bool	write_client(PacketBuilder &w, Connection *con)	{
		opaque_array<8>	local_pub_key = ecdh.get_public();
		return write(w, local_pub_key);
	}

	virtual bool	server_begin(Connection *con, CipherSuiteInfo *info, CertificateDesc *certificate) {
		if (!certificate)
			return false;

		X509::ECPrivateKey	priv_key;
		if (!ASN1::Read(memory_reader(certificate->priv_key), priv_key))
			return false;

		if (const NamedCurve *i = get_curve(priv_key.parameters->namedcurve)) {
			id		= i->id;
			type	= named_curve;
			ecdh.init(*EC_curve::get_named(priv_key.parameters->namedcurve));
			ecdh.priv_key	= priv_key.privateKey.get_buffer();
		}
		//public_key	= curve.load((const uint8*)priv_key.publicKey.t.data, priv_key.publicKey.t.N / 8);
		return true;
	}
	virtual bool	write_server(PacketBuilder &w, Connection *con, const ClientHello *hello)	{
		write(w, type);
		if (type == named_curve) {
			write(w, id);
		} else {
			if (type == explicit_prime) {
				opaque_array<8>		p = ecdh.p;
				write(w, p);
			} else {
				uint16be				m;
				ECBasisType				basis = trinomial;
				opaque_array<8>			k[3];
				w.write(m, basis);
				writen(w, k, basis == pentanomial ? 3 : 1);
			}
			opaque_array<8>		a = ecdh.a, b = ecdh.b, g = ecdh.save(ecdh.g), q = ecdh.q, h = ecdh.h;
			w.write(
				opaque_array<8>(ecdh.a),
				opaque_array<8>(ecdh.b),
				opaque_array<8>(ecdh.save(ecdh.g)),
				opaque_array<8>(ecdh.q),
				opaque_array<8>(ecdh.h)
			);
		}
		return write(w, opaque_array<8>(ecdh.get_public()));
	}
	virtual bool	read_client(PacketReader &r, Connection *con)	{
		opaque_array<8>	peer_pub_key;
		if (!read(r, peer_pub_key))
			return false;
		con->pre_master_secret	= ecdh.shared_secret(peer_pub_key);
		return true;
	}
};
#endif

#ifdef SSL_KEYEX_FORTEZZA
//-----------------------------------------------------------------------------
//	fortezza
//-----------------------------------------------------------------------------

template<> struct KeyExchangerT<xchg_fortezza> : KeyExchanger {
	//server sends
	uint8				r_s[128];						//Server random number for Fortezza KEA(Key Exchange Algorithm).
	//client sends
	opaque_array<8>		Yc;								//client's Yc value (public key) for the KEA calculation. If the client has sent a certificate, and its KEA public key is suitable, this value must be empty since the certificate already contains this value. If the client sent a certificate without a suitable public key, y_c is used and y_singnature is the KEA public key signed with the client's DSS private key.For this value to be used, it must be between 64 and 128 bytes.
	uint8				Rc[128];						//client's Rc value for the KEA calculation.
	uint8				y_signature[20];				//the signature of the KEA public key, signed with the client's DSS private key.
	uint8				wrapped_client_write_key[12];	//client's write key, wrapped by the TEK.
	uint8				wrapped_server_write_key[12];	//server's write key, wrapped by the TEK.
	uint8				client_write_iv[24];			//IV for the client write key.
	uint8				server_write_iv[24];			//IV for the server write key.
	uint8				master_secret_iv[24];			//IV for the TEK used to encrypt the pre-master secret.
	uint8				pre_master_secret[48];

	virtual bool	read_client(PacketReader &r, Connection *con)	{
		return read(r, Yc, Rc, y_signature
			, wrapped_client_write_key, wrapped_server_write_key
			, client_write_iv, server_write_iv, master_secret_iv, pre_master_secret
		);
	}
	virtual bool	write_client(PacketBuilder &w, Connection *con)	{
		return w.write(Yc, Rc, y_signature
			, wrapped_client_write_key, wrapped_server_write_key
			, client_write_iv, server_write_iv, master_secret_iv, pre_master_secret
		);
	}

	virtual bool	read_server(PacketReader &r, Connection *con)	{
		return read(r, r_s);
	}

	virtual bool	write_server(PacketBuilder &w, Connection *con, const ClientHello *hello)	{
		return write(w, r_s);
	}
};
#endif

KeyExchanger *GetKeyExchanger(KeyExchangeAlgorithm x) {
	switch (x) {
		case xchg_rsa:			return new KeyExchangerT<xchg_rsa>;
		case xchg_dh:			return new KeyExchangerT<xchg_dh>;
		case xchg_dhe:			return new KeyExchangerT<xchg_dh>;
		case xchg_srp:			return new KeyExchangerT<xchg_srp>;
		case xchg_psk:			return new KeyExchangerT<xchg_psk>;
		case xchg_ecdh:			return new KeyExchangerT<xchg_ecdh>;
		case xchg_gost01:		return new KeyExchangerT<xchg_gost01>;
		case xchg_gost12:		return new KeyExchangerT<xchg_gost12>;
		case xchg_fortezza:		return new KeyExchangerT<xchg_fortezza>;
		default:				return 0;
	}
}

//-----------------------------------------------------------------------------
//	defaults
//-----------------------------------------------------------------------------

CipherSuite default_cipher_suites[] = {
	AES_128_GCM_SHA256,
	AES_256_GCM_SHA384,
	CHACHA20_POLY1305_SHA256,
	ECDHE_ECDSA_WITH_AES_256_GCM_SHA384,
	ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
	ECDHE_RSA_WITH_AES_256_GCM_SHA384,
	ECDHE_RSA_WITH_AES_128_GCM_SHA256,
	DHE_RSA_WITH_AES_256_GCM_SHA384,
	DHE_RSA_WITH_AES_128_GCM_SHA256,
	ECDHE_ECDSA_WITH_AES_256_CBC_SHA384,
	ECDHE_ECDSA_WITH_AES_128_CBC_SHA256,
	ECDHE_RSA_WITH_AES_256_CBC_SHA384,
	ECDHE_RSA_WITH_AES_128_CBC_SHA256,
	ECDHE_ECDSA_WITH_AES_256_CBC_SHA,
	ECDHE_ECDSA_WITH_AES_128_CBC_SHA,
	ECDHE_RSA_WITH_AES_256_CBC_SHA,
	ECDHE_RSA_WITH_AES_128_CBC_SHA,
	DHE_RSA_WITH_AES_256_CBC_SHA,
	DHE_RSA_WITH_AES_128_CBC_SHA,
	RSA_WITH_AES_256_GCM_SHA384,
	RSA_WITH_AES_128_GCM_SHA256,
	RSA_WITH_AES_256_CBC_SHA256,
	RSA_WITH_AES_128_CBC_SHA256,
	RSA_WITH_AES_256_CBC_SHA,
	RSA_WITH_AES_128_CBC_SHA,
	RSA_WITH_3DES_EDE_CBC_SHA,
	DHE_DSS_WITH_AES_256_CBC_SHA256,
	DHE_DSS_WITH_AES_128_CBC_SHA256,
	DHE_DSS_WITH_AES_256_CBC_SHA,
	DHE_DSS_WITH_AES_128_CBC_SHA,
	DHE_DSS_WITH_3DES_EDE_CBC_SHA
};

SignatureAndHashAlgorithm default_sig_hash[] = {
	{hash_md5,		sig_rsa},
	{hash_sha1,		sig_rsa},
	{hash_sha224,	sig_rsa},
	{hash_sha256,	sig_rsa},
	{hash_sha384,	sig_rsa},
	{hash_sha512,	sig_rsa},

	{hash_sha1,		sig_dsa},
	{hash_sha224,	sig_dsa},
	{hash_sha256,	sig_dsa},

	{hash_sha1,		sig_ecdsa},
	{hash_sha224,	sig_ecdsa},
	{hash_sha256,	sig_ecdsa},
	{hash_sha384,	sig_ecdsa},
	{hash_sha512,	sig_ecdsa},
};


#if 1
Extension	extensions[] = {
//	Extension::Struct<Extension::server_name>("server"),
	Extension::status_request,
	Extension::elliptic_curves,
	Extension::ec_point_formats,
	Extension::signature_algorithms,
	Extension::extended_master_secret,
	Extension::application_layer_protocol_negotiation,
	Extension::extended_master_secret,
//	Extension::21760,
	Extension::renegotiation_info,
};
#endif

//Server's RSA certificate and private key
StaticCertificate rsa_cert(
"-----BEGIN CERTIFICATE-----\n"
"MIICfDCCAeWgAwIBAgIJANZoFs4ZGc85MA0GCSqGSIb3DQEBCwUAMEUxCzAJBgNV\n"
"BAYTAkZSMRYwFAYDVQQKDA1Pcnl4IEVtYmVkZGVkMR4wHAYDVQQDDBVPcnl4IEVt\n"
"YmVkZGVkIFRlc3QgQ0EwHhcNMTYwOTI4MTExOTM5WhcNMjEwOTI4MTExOTM5WjBG\n"
"MQswCQYDVQQGEwJGUjEWMBQGA1UECgwNT3J5eCBFbWJlZGRlZDEfMB0GA1UEAwwW\n"
"ZGVtby5vcnl4LWVtYmVkZGVkLmNvbTCBnzANBgkqhkiG9w0BAQEFAAOBjQAwgYkC\n"
"gYEAx3THjPmrcPeASFUuqK6FnF0Hfh5H5C+/p9JVoBlJ5Cyj9jRad1LgDUzGLUa5\n"
"Gjlcmn1oAzvIoIXXXE4hp5SxSEpFB0SaJOILfQ21WIe8FgEmdsAIRNkouoLdRdIh\n"
"G4Yhbou/Oygwe53wuBQKN6voclJFq9dkCwfxTx2Qw0NCxYUCAwEAAaNzMHEwEQYJ\n"
"YIZIAYb4QgEBBAQDAgZAMAwGA1UdEwEB/wQCMAAwDgYDVR0PAQH/BAQDAgWgMB0G\n"
"A1UdDgQWBBRWzHFrXrQTkwe7i8zV5AawpXEgBjAfBgNVHSMEGDAWgBT3bL6YYlq2\n"
"riaG4SeH5wQN8NYLJzANBgkqhkiG9w0BAQsFAAOBgQCr9bJu3UYK0KG1ctnqcFI4\n"
"+CAk4lhrNmssNONM0bQMzJ2HetruKAczS+rO4Ie6byBNqsfa9/2iXuUFYHamYOO4\n"
"QYrm5NvzV5xAQDzcpA+1nDNymLrjw1WKPzwLXLEv9iTYpKrhunJlISSA//lkRORf\n"
"3lTJDqdzhPuseHJFS2VD9g==\n"
"-----END CERTIFICATE-----\n"
	,
"-----BEGIN RSA PRIVATE KEY-----\n"
"MIICXgIBAAKBgQDHdMeM+atw94BIVS6oroWcXQd+HkfkL7+n0lWgGUnkLKP2NFp3\n"
"UuANTMYtRrkaOVyafWgDO8ighddcTiGnlLFISkUHRJok4gt9DbVYh7wWASZ2wAhE\n"
"2Si6gt1F0iEbhiFui787KDB7nfC4FAo3q+hyUkWr12QLB/FPHZDDQ0LFhQIDAQAB\n"
"AoGAPTr9c6rF1cU1TX9Q9pm1DL6GaVI0cbmy1Xs9rAt3YqPNpeyGhTEc9RhHkmiu\n"
"tH09j3PGNY/p1FWiOoUF0eNU1LikkhM5C4pH60G+r+gSq2v9+iaEcNLyS6OkWuci\n"
"X/gvo5PXraQxqQNjk3j0ECHT1muBSCkxffxgwLe1vwH1B6ECQQDodE9Si1alM5kD\n"
"yq/0Y0RcNAJaiWOfR7pKIQUZDhI9U+WTrZpA9CXHoeKjbuV/R1JOhPYAtSBDhhdR\n"
"J5hCk8t9AkEA26jOV8l5Vkk7XqyBOO78FYoM0H/XrY3NUAIW6txo2XHijfuyy5T7\n"
"mIlnybVHB3zRy0lCSITpchIOwbqu8dIwqQJBAJuPiq4A1YFE4HerIsl3zP2vSIvy\n"
"OZsUU1TceB7mTcqA5LhJi1tAiC/q5FLfGfJtdmVZkY+zpt3JVL1YtAqKAbkCQQCe\n"
"MIJxjEHa1yBvfPIO62UVqfayjO7pKQ7sCKUyfIrx1edfcx1/KYrLfmEFTYyaotR/\n"
"fwHCMh3grYp9EBF/S20JAkEAjPmvR8vdUv1D9Q4sZxEpY1Vfr0E8TC0lprq8UjYH\n"
"LqT6LJwJTERfGrOFEbJAR6vzFVzvRfr/8IwbCYge6GZusQ==\n"
"-----END RSA PRIVATE KEY-----\n"
);

//Server's DSA certificate and private key
StaticCertificate dsa_cert(
"-----BEGIN CERTIFICATE-----\n"
"MIIDPTCCAvugAwIBAgIJAP0R+QA3OyFgMAsGCWCGSAFlAwQDAjBFMQswCQYDVQQG\n"
"EwJGUjEWMBQGA1UECgwNT3J5eCBFbWJlZGRlZDEeMBwGA1UEAwwVT3J5eCBFbWJl\n"
"ZGRlZCBUZXN0IENBMB4XDTE2MDkyODExMjAzNFoXDTIxMDkyODExMjAzNFowRjEL\n"
"MAkGA1UEBhMCRlIxFjAUBgNVBAoMDU9yeXggRW1iZWRkZWQxHzAdBgNVBAMMFmRl\n"
"bW8ub3J5eC1lbWJlZGRlZC5jb20wggG2MIIBKwYHKoZIzjgEATCCAR4CgYEA7fva\n"
"V+Gd4wznvX1I++mqd8qt1SUhE4jd2BXaZY/Od+B0O0RBrGg3w71yiDVZybSfa1Jc\n"
"jA1LWUGNXgfITjzY9R3prrh+VZ8OkIid7qVz0qZaXE9m6Oc2mx/xmcQUOX9xvG2O\n"
"4o4p7xfl/q2uN4auZilB6kGdv0uM1CtJUpAH4dkCFQD81LrvpapUyWFCqbLW6J2k\n"
"TeZgxwKBgE0SeAI8h4XEvk1mhquuAOCtyyEGoRcwnxeuC+imsNm0TtRXn8cJzuaO\n"
"kgZp/hgKa7y57sHZpRPtqUi5CgCuYKJJMRaC0b3xYj/PfUrKHYb1qq40l1HyWUO0\n"
"YIFJ0cx3X6koxqzrjh1KT6Ni4Z73xIBuPXLbFcV2zVccC2FpTmWHA4GEAAKBgDJN\n"
"FCoC0spOILbCv4wxuF8mqGk03J/wt0548kUgZFsujNtmcBGjsLC/8qAyBoys/gwP\n"
"b1vqcRz98bxtmu5W5VPs/8/rK2VQ7tZe1OOVXN3zc/O+5saWdi6POldV0rILy488\n"
"WDa/iBWcABn7enXZpCsYYVuQ2NCFfVJ/t7Ll2ul6o3MwcTARBglghkgBhvhCAQEE\n"
"BAMCBkAwDAYDVR0TAQH/BAIwADAOBgNVHQ8BAf8EBAMCBaAwHQYDVR0OBBYEFD/4\n"
"qfI6VBk0NSommoIeyEox847FMB8GA1UdIwQYMBaAFNyP+yRzlSCVALAr6iZH7r9c\n"
"HRUhMAsGCWCGSAFlAwQDAgMvADAsAhQ1/0fqdScVoR5MKvtuUUHXCqRDwQIUCiDx\n"
"n/d8kqwhk2K0dbmLT0iMLAM=\n"
"-----END CERTIFICATE-----\n"
	,
"-----BEGIN DSA PRIVATE KEY-----\n"
"MIIBuwIBAAKBgQDt+9pX4Z3jDOe9fUj76ap3yq3VJSETiN3YFdplj8534HQ7REGs\n"
"aDfDvXKINVnJtJ9rUlyMDUtZQY1eB8hOPNj1HemuuH5Vnw6QiJ3upXPSplpcT2bo\n"
"5zabH/GZxBQ5f3G8bY7ijinvF+X+ra43hq5mKUHqQZ2/S4zUK0lSkAfh2QIVAPzU\n"
"uu+lqlTJYUKpstbonaRN5mDHAoGATRJ4AjyHhcS+TWaGq64A4K3LIQahFzCfF64L\n"
"6Kaw2bRO1FefxwnO5o6SBmn+GAprvLnuwdmlE+2pSLkKAK5gokkxFoLRvfFiP899\n"
"SsodhvWqrjSXUfJZQ7RggUnRzHdfqSjGrOuOHUpPo2LhnvfEgG49ctsVxXbNVxwL\n"
"YWlOZYcCgYAyTRQqAtLKTiC2wr+MMbhfJqhpNNyf8LdOePJFIGRbLozbZnARo7Cw\n"
"v/KgMgaMrP4MD29b6nEc/fG8bZruVuVT7P/P6ytlUO7WXtTjlVzd83PzvubGlnYu\n"
"jzpXVdKyC8uPPFg2v4gVnAAZ+3p12aQrGGFbkNjQhX1Sf7ey5drpegIVAJZzpK0h\n"
"aJ5VS7ashOaBWYkAKklV\n"
"-----END DSA PRIVATE KEY-----\n"
);

//Server's ECDSA certificate and private key
StaticCertificate ecdsa_cert(
"-----BEGIN CERTIFICATE-----\n"
"MIIB9TCCAZugAwIBAgIJAMRU/ykK34O6MAoGCCqGSM49BAMCMEUxCzAJBgNVBAYT\n"
"AkZSMRYwFAYDVQQKDA1Pcnl4IEVtYmVkZGVkMR4wHAYDVQQDDBVPcnl4IEVtYmVk\n"
"ZGVkIFRlc3QgQ0EwHhcNMTYwOTI4MTEyMTI4WhcNMjEwOTI4MTEyMTI4WjBGMQsw\n"
"CQYDVQQGEwJGUjEWMBQGA1UECgwNT3J5eCBFbWJlZGRlZDEfMB0GA1UEAwwWZGVt\n"
"by5vcnl4LWVtYmVkZGVkLmNvbTBZMBMGByqGSM49AgEGCCqGSM49AwEHA0IABHHW\n"
"GvqnK6jk+00H9RKipFVivq+dU23+/RZoUK6VDKvOueVv4hUzwcPzH8pFWM0R4XhC\n"
"w3FUYUGYJdPXyVjY6cKjczBxMBEGCWCGSAGG+EIBAQQEAwIGQDAMBgNVHRMBAf8E\n"
"AjAAMA4GA1UdDwEB/wQEAwIFoDAdBgNVHQ4EFgQUacUwqwme0qAtPCjPYCYPLA1o\n"
"6WgwHwYDVR0jBBgwFoAUeCEvxeiq3vGsTWdS/2/R7Caz3A0wCgYIKoZIzj0EAwID\n"
"SAAwRQIgBrxMF6Q0ncQtwcTztF9NzGByySiCdaZR0ikdD/OktrcCIQCMfez39MfW\n"
"dZGtnkBbaFehSOXpDR1ytKm7JuZAlUpoVg==\n"
"-----END CERTIFICATE-----\n"
	,
"-----BEGIN EC PRIVATE KEY-----\n"
"MHcCAQEEICNtoCxR+ilDPiH9LlJKLPCeLxxHOgKeIdlesrbEoHDWoAoGCCqGSM49\n"
"AwEHoUQDQgAEcdYa+qcrqOT7TQf1EqKkVWK+r51Tbf79FmhQrpUMq8655W/iFTPB\n"
"w/MfykVYzRHheELDcVRhQZgl09fJWNjpwg==\n"
"-----END EC PRIVATE KEY-----\n"
);

struct CertificateAuthorities : dynamic_array<CertificateAuthority> {
	CertificateAuthorities(const char *ca) { add(ca); }
	void add(const char *ca) {
		dynamic_array<PemEntity>	pem;
		ReadPem(pem, ca);
		for (auto &i : pem)
			ASN1::Read(memory_reader(i), push_back());
	}
};

//Trusted CA bundle
static CertificateAuthorities certificate_authorities(
"-----BEGIN CERTIFICATE-----\n"
"MIICWzCCAcSgAwIBAgIJAK69ylPHAlDCMA0GCSqGSIb3DQEBCwUAMEUxCzAJBgNV\n"
"BAYTAkZSMRYwFAYDVQQKDA1Pcnl4IEVtYmVkZGVkMR4wHAYDVQQDDBVPcnl4IEVt\n"
"YmVkZGVkIFRlc3QgQ0EwHhcNMTYwOTI4MTExOTIxWhcNMjEwOTI4MTExOTIxWjBF\n"
"MQswCQYDVQQGEwJGUjEWMBQGA1UECgwNT3J5eCBFbWJlZGRlZDEeMBwGA1UEAwwV\n"
"T3J5eCBFbWJlZGRlZCBUZXN0IENBMIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKB\n"
"gQDVu3s8jXuZy7Ko+a95V2RcfcZOnSXJ4TyODqoGSH+QRX9Dii1LQvwmDc/28CMe\n"
"IfHRbQWvYuyz8fYrllwdWEBIqGeFYgC1SEbdMhYsM6jwe0hpoE414v3NHgFu8roE\n"
"uGcZb+fOumP+KlcsduWEd2u/c3hs4nUS1dfXDOPOE+LsqwIDAQABo1MwUTAdBgNV\n"
"HQ4EFgQU92y+mGJatq4mhuEnh+cEDfDWCycwHwYDVR0jBBgwFoAU92y+mGJatq4m\n"
"huEnh+cEDfDWCycwDwYDVR0TAQH/BAUwAwEB/zANBgkqhkiG9w0BAQsFAAOBgQDM\n"
"y6nAyJd2Tt2/kfhbQ4thoqwXAK9A08RKrBn+z3u+CAMJJ/eyBGDSB7NAyU4hK/kC\n"
"x3T6vLLbQbj0UqlENZtpOY0Z29Mlob/sJoYdxkgw43/lKPyXaWr2iBy5O9oGflfB\n"
"SxgZKIs+pI4TKQ3qh+6AJYgFFreuIJYy4S1rfD/y7g==\n"
"-----END CERTIFICATE-----\n"
"\n"
"-----BEGIN CERTIFICATE-----\n"
"MIIDHDCCAtqgAwIBAgIJAO6Q6x3qbm+YMAsGCWCGSAFlAwQDAjBFMQswCQYDVQQG\n"
"EwJGUjEWMBQGA1UECgwNT3J5eCBFbWJlZGRlZDEeMBwGA1UEAwwVT3J5eCBFbWJl\n"
"ZGRlZCBUZXN0IENBMB4XDTE2MDkyODExMjAxNVoXDTIxMDkyODExMjAxNVowRTEL\n"
"MAkGA1UEBhMCRlIxFjAUBgNVBAoMDU9yeXggRW1iZWRkZWQxHjAcBgNVBAMMFU9y\n"
"eXggRW1iZWRkZWQgVGVzdCBDQTCCAbYwggErBgcqhkjOOAQBMIIBHgKBgQC8ZAE/\n"
"brjQXeZbMGuzadcZp1XuyKxegpcm+cKMptb+dw7m6SQz7Z2BNE8wRTd8yDglmuif\n"
"6Y/oT34zAnwsaYurpyNn98Ni0OFBqV/KavSp+JXgjYS8UwB/9QCVdjNKFpsZzevy\n"
"zzC/oIoFjVcQc90P3uurk6FhhjgAQ6vXzh1uUQIVAKwqTBIJx1B8ybnw8F/NbTot\n"
"P8JnAoGAJfi6fypwT5Ax0/sSiZXRTibjjeqzBs28cqNDd+p2Ik1Ur30EUh5eWC4G\n"
"BVltRVGrJR8UxLL5B1wHYGZzpr1T+lef2Co0gIc4Tu6pb+JSa5C1wLw01bHyFJ4D\n"
"elfdKwij7pYLcykYPVyov2vuKa+dPoVEoyxnwp6OoW8t6l2DDKoDgYQAAoGAWWfp\n"
"EYzcDBT+w2C40yt8J08r6HmTYqAZb08UvqhEnqOfWrcf89qf3UX+/DGZEdp+TqJ0\n"
"ds2fBLtAt9H/8xP6B9yuQq+XohPrvJmtx9mWMzWq63PD0c3SnINyb9z8WfIc3H1x\n"
"Q55PptF5K3VtPiXDkJmMNqmRF0ql3OXlNEwFiTGjUzBRMB0GA1UdDgQWBBTcj/sk\n"
"c5UglQCwK+omR+6/XB0VITAfBgNVHSMEGDAWgBTcj/skc5UglQCwK+omR+6/XB0V\n"
"ITAPBgNVHRMBAf8EBTADAQH/MAsGCWCGSAFlAwQDAgMvADAsAhRrWXyIeLLx9EtD\n"
"XIaUXLDyVHol2AIUJeUwWQAtesFBEg76qYUV/m7t1Ho=\n"
"-----END CERTIFICATE-----\n"
"\n"
"-----BEGIN CERTIFICATE-----\n"
"MIIB1TCCAXqgAwIBAgIJAIz+WJjU0pmuMAoGCCqGSM49BAMCMEUxCzAJBgNVBAYT\n"
"AkZSMRYwFAYDVQQKDA1Pcnl4IEVtYmVkZGVkMR4wHAYDVQQDDBVPcnl4IEVtYmVk\n"
"ZGVkIFRlc3QgQ0EwHhcNMTYwOTI4MTEyMTE0WhcNMjEwOTI4MTEyMTE0WjBFMQsw\n"
"CQYDVQQGEwJGUjEWMBQGA1UECgwNT3J5eCBFbWJlZGRlZDEeMBwGA1UEAwwVT3J5\n"
"eCBFbWJlZGRlZCBUZXN0IENBMFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEGA9f\n"
"TtPlJAIlR3AVoM8jBKOonto50bDEEmC0+X+QqOhIZXEpDeJryJgT8ccTVkFJs+zh\n"
"wh3iP6wXJjiHhvsxUqNTMFEwHQYDVR0OBBYEFHghL8Xoqt7xrE1nUv9v0ewms9wN\n"
"MB8GA1UdIwQYMBaAFHghL8Xoqt7xrE1nUv9v0ewms9wNMA8GA1UdEwEB/wQFMAMB\n"
"Af8wCgYIKoZIzj0EAwIDSQAwRgIhAIDUF9pYhGwSaaiM6uSjFdbNBbCFI6F0qk9J\n"
"tG7TXgr9AiEA+Amj22AiLi/pODB06mMxssIDOd2zBNmpNzMCiusvqbA=\n"
"-----END CERTIFICATE-----\n"
"\n"
//Baltimore CyberTrust Root
"-----BEGIN CERTIFICATE-----"
"MIIDdzCCAl+gAwIBAgIEAgAAuTANBgkqhkiG9w0BAQUFADBaMQswCQYDVQQGEwJJ\n"
"RTESMBAGA1UEChMJQmFsdGltb3JlMRMwEQYDVQQLEwpDeWJlclRydXN0MSIwIAYD\n"
"VQQDExlCYWx0aW1vcmUgQ3liZXJUcnVzdCBSb290MB4XDTAwMDUxMjE4NDYwMFoX\n"
"DTI1MDUxMjIzNTkwMFowWjELMAkGA1UEBhMCSUUxEjAQBgNVBAoTCUJhbHRpbW9y\n"
"ZTETMBEGA1UECxMKQ3liZXJUcnVzdDEiMCAGA1UEAxMZQmFsdGltb3JlIEN5YmVy\n"
"VHJ1c3QgUm9vdDCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAKMEuyKr\n"
"mD1X6CZymrV51Cni4eiVgLGw41uOKymaZN+hXe2wCQVt2yguzmKiYv60iNoS6zjr\n"
"IZ3AQSsBUnuId9Mcj8e6uYi1agnnc+gRQKfRzMpijS3ljwumUNKoUMMo6vWrJYeK\n"
"mpYcqWe4PwzV9/lSEy/CG9VwcPCPwBLKBsua4dnKM3p31vjsufFoREJIE9LAwqSu\n"
"XmD+tqYF/LTdB1kC1FkYmGP1pWPgkAx9XbIGevOF6uvUA65ehD5f/xXtabz5OTZy\n"
"dc93Uk3zyZAsuT3lySNTPx8kmCFcB5kpvcY67Oduhjprl3RjM71oGDHweI12v/ye\n"
"jl0qhqdNkNwnGjkCAwEAAaNFMEMwHQYDVR0OBBYEFOWdWTCCR1jMrPoIVDaGezq1\n"
"BE3wMBIGA1UdEwEB/wQIMAYBAf8CAQMwDgYDVR0PAQH/BAQDAgEGMA0GCSqGSIb3\n"
"DQEBBQUAA4IBAQCFDF2O5G9RaEIFoN27TyclhAO992T9Ldcw46QQF+vaKSm2eT92\n"
"9hkTI7gQCvlYpNRhcL0EYWoSihfVCr3FvDB81ukMJY2GQE/szKN+OMY3EU/t3Wgx\n"
"jkzSswF07r51XgdIGn9w/xZchMB5hbgF/X++ZRGjD8ACtPhSNzkE1akxehi/oCr0\n"
"Epn3o0WC4zxe9Z2etciefC7IpJ5OCBRLbf1wbWsaY71k5h+3zvDyny67G7fyUIhz\n"
"ksLi4xaNmjICq44Y3ekQEe5+NauQrz4wlHrQMz2nZQ/1/I6eYs9HRCwBXbsdtTLS\n"
"R9I4LtD+gdwyah617jzV/OeBHRnDJELqYzmp\n"
"-----END CERTIFICATE-----\n"
);

//-----------------------------------------------------------------------------
//	record
//-----------------------------------------------------------------------------

struct AdditionalData {
	packed<uint64be>	sequence_number;
	Record				record;
	AdditionalData(uint64 _sequence_number, const Record &_record) : sequence_number(_sequence_number), record(_record) {}
};

//#define THROUGH_RR
streamptr Connection::read_record(Record &r, PacketReaderSSL &rr, istream_ref in) {
#ifdef THROUGH_RR
//	if (rr.remaining() == 0)
//		rr.fill(in);

	if (!rr.read(r) || !r.valid())
		return  0;

	streamptr	end = rr.tell() + r.length;

#else
	ISO_ASSERT(rr.remaining() == 0);
	if (!in.read(r) || !r.valid())
		return  0;

	streamptr	end = r.length;
#endif

	if (peer_encrypted) {
		uint32	plain_len	= r.length - record_iv_length - auth_tag_length;
		r.length = plain_len;

		if (mac_algorithm == MAC_AEAD) {
			uint8	nonce[16];
			uint32	nonce_len	= fixed_iv_length + record_iv_length;

			read_iv.copy_to(nonce);
#ifdef THROUGH_RR
			rr.readbuff(nonce + fixed_iv_length, record_iv_length);
			const_memory_block		data	= rr.peek_block(in, plain_len + auth_tag_length);
			DecryptInterfaceDigest	cipher	= get_decrypt_context(data.slice_to(-int(auth_tag_length)), cipher_info->bulk_encryption,
				read_key,
				memory_block(nonce, nonce_len),
				const_memory_block(AdditionalData(read_sequence_number++, r)),
				auth_tag_length, plain_len
			);
			cipher.readbuff((void*)(const void*)data, plain_len);

			if (*cipher.digest() != *data.slice(-int(auth_tag_length)))
				return 0;
#else
			ISO_VERIFY(check_readbuff(in, nonce + fixed_iv_length, record_iv_length));
			const_memory_block		data	= rr.fill(in, plain_len);
			DecryptInterfaceDigest	cipher	= get_decrypt_context(data, cipher_info->bulk_encryption,
				read_key,
				memory_block(nonce, nonce_len),
				const_memory_block(addr(AdditionalData(read_sequence_number++, r))),
				auth_tag_length, plain_len
			);
			cipher.readbuff((void*)(const void*)data, plain_len);
			in.readbuff(nonce, auth_tag_length);
			if (*cipher.digest() != *memory_block(nonce, auth_tag_length))
				return 0;
#endif


		} else {
#ifdef THROUGH_RR
			const_memory_block	iv		= rr.get_block(record_iv_length);
#else
			uint8				iv_buffer[16];
			memory_block		iv(iv_buffer, record_iv_length);
			ISO_VERIFY(check_readbuff(in, iv_buffer, record_iv_length));
#endif
			const_memory_block	data	= rr.fill(in, plain_len);
			get_decrypt_context(data, cipher_info->bulk_encryption, read_key, iv).readbuff((void*)(const void*)data, plain_len);

			if (block_length) {
				int	padding = ((const uint8*)data)[plain_len - 1];
				plain_len	-= padding + 1;
			}

			plain_len -= mac_key_length;
			r.length = plain_len;
			malloc_block mac = HMAC(get_hash_context(get_hash(mac_algorithm)),
				read_MAC,
				addr(AdditionalData(read_sequence_number++, r)),
				data.slice_to(plain_len)
			);
			if (*mac != *data.slice(plain_len, mac_key_length))
				return 0;
		}
	} else {
		rr.fill(in, r.length);
	}
	return end;
}

void Connection::write_record(patch_write<Record> &patch) {
	Record	*record	= patch;

	if (local_encrypted) {
		memory_block	record_iv(record + 1, record_iv_length);
		void			*plain	= record_iv.end();
		uint32			len		= uint32((uint8*)patch.w.getp() - (uint8*)plain);

		record->length	= len;

		rand.fill(make_range<uint8>(record_iv));

		if (mac_algorithm == MAC_AEAD) {
			uint8	nonce[16];
			uint32	nonce_len	= fixed_iv_length + record_iv_length;
			write_iv.copy_to(nonce);
			record_iv.copy_to(nonce + fixed_iv_length);

			memory_block	mo(plain, len);
			EncryptInterfaceDigest	cipher		= get_encrypt_context(mo, cipher_info->bulk_encryption,
				write_key,
				memory_block(nonce, nonce_len),
				const_memory_block(addr(AdditionalData(write_sequence_number++, *record))),
				auth_tag_length, len
			);
			write(cipher, mo);
			patch.w.write(cipher.digest());

		} else {
			patch.w.write(HMAC(get_hash_context(get_hash(mac_algorithm)),
				write_MAC,
				addr(AdditionalData(write_sequence_number++, *record)),
				const_memory_block(plain, len)
			));
			len	+= mac_key_length;

			if (block_length) {
				int	padding = ~len % block_length;
				memset(patch.w.alloc(padding + 1), padding, padding + 1);
				len += padding + 1;
			}

			record		= patch;
			memory_block	mo(record + 1, len + record_iv_length);
			get_encrypt_context(mo, cipher_info->bulk_encryption, write_key, write_iv).write(
				mo
			);
		}
	}

	record->length	= (uint8*)patch.w.getp() - (uint8*)(record + 1);
}

//-----------------------------------------------------------------------------
//	Connection
//-----------------------------------------------------------------------------

VerificationData Connection::calculate_verification(bool server, const const_memory_block &handshake_messages) {
	VerificationData	verification;
	PRF(get_hash_context(prf_hash), memory_block(&verification),
		master_secret.raw_data(),
		server ? "server finished" : "client finished",
		get_hash_context(prf_hash)(handshake_messages)
	);
	return verification;
}

void Connection::set_cipher(CipherSuiteInfo *_cipher_info, bool server, bool extended_master_secret, const const_memory_block &handshake_messages) {
	cipher_info				= _cipher_info;
	mac_algorithm			= cipher_info->mac_algorithm;
	prf_hash				= cipher_info->prf_hash;

	if (version >= VER_TLS1_2 && prf_hash == hash_md5_sha1)
		prf_hash = hash_sha256;

	write_sequence_number	= 0;

	const BulkEncryptionInfo	&b = bulk_encryption_info[cipher_info->bulk_encryption];

	mac_key_length		= hash_info[get_hash(mac_algorithm)].digest;
	block_length		= b.block;
	enc_key_length		= b.key_bytes;
	fixed_iv_length		= b.fixed_iv;
	record_iv_length	= b.record_iv;
	auth_tag_length		= b.auth_tag;

	memory_block	key_block(key_block_data, (mac_key_length + enc_key_length + fixed_iv_length) * 2);

	if (extended_master_secret) {
		HashInterface	hash = get_hash_context(prf_hash);
		hash.write(handshake_messages);
		PRF(move(hash), master_secret.raw_data(), pre_master_secret, "extended master secret", hash.digest());
	} else {
		TimeRandom	flipped_random[2] = {server_random, client_random};
		switch (version) {
			case VER_SSL3:		//SSL 3.0 does not use a PRF, instead makes use abundantly of MD5
				ExpandKey(master_secret.raw_data(), pre_master_secret, both_random);
				ExpandKey(key_block, master_secret.raw_data(), flipped_random);
				break;
			case VER_TLS1:
			case VER_TLS1_1:	//TLS 1.0 and 1.1 use a PRF that combines MD5 and SHA-1
				PRF_MD5_SHA1(master_secret.raw_data(), pre_master_secret, "master secret", both_random);
				PRF_MD5_SHA1(key_block, master_secret.raw_data(), "key expansion", flipped_random);
				break;
			default: {			//TLS 1.2 PRF uses SHA-256 or a stronger hash algorithm as the core function in its construction
				HashInterface	hash = get_hash_context(prf_hash);
				PRF(move(hash), master_secret.raw_data(), pre_master_secret, "master secret", both_random);
				PRF(move(hash), key_block, master_secret.raw_data(), "key expansion", flipped_random);
				break;
			}
		}
	}

	if (server) {
		read_MAC	= memory_block(key_block_data, mac_key_length);
		write_MAC	= memory_block(read_MAC.end(), mac_key_length);
		read_key	= memory_block(write_MAC.end(), enc_key_length);
		write_key	= memory_block(read_key.end(), enc_key_length);
		read_iv		= memory_block(write_key.end(), fixed_iv_length);
		write_iv	= memory_block(read_iv.end(), fixed_iv_length);
	} else {
		write_MAC	= memory_block(key_block_data, mac_key_length);
		read_MAC	= memory_block(write_MAC.end(), mac_key_length);
		write_key	= memory_block(read_MAC.end(), enc_key_length);
		read_key	= memory_block(write_key.end(), enc_key_length);
		write_iv	= memory_block(read_key.end(), fixed_iv_length);
		read_iv		= memory_block(write_iv.end(), fixed_iv_length);
	}
	if (!fixed_iv_length) {
		read_iv		= empty;
		write_iv	= empty;
	}
}

void Connection::set_peer_encryption() {
	peer_encrypted			= true;
	read_sequence_number	= 0;
}

//-----------------------------------------------------------------------------
//	ClientConnect
//-----------------------------------------------------------------------------

bool Connection::ClientConnect(iostream_ref io) {//n, ostream_ref out) {
	if (!sig_hash)
		sig_hash = default_sig_hash;
	if (!cipher_suites)
		cipher_suites = default_cipher_suites;

	malloc_block		handshake_messages;
	PacketBuilderSSL	rb;

	// fill ClientHello
	ClientHello			client_hello;
	client_hello.random.gmt_unix_time = get_unix_time();
	rand.fill(client_hello.random.random);

	client_hello.cipher_suites.append(cipher_suites);
	client_hello.compressions.push_back(NO_COMP);

	put(client_hello.extensions).push_back(Extension::Struct<Extension::signature_algorithms>(sig_hash));
#ifdef SSL_KEYEX_ECDH
	put(client_hello.extensions).push_back(Extension::Struct<Extension::supported_groups>(allowed_curves));
	put(client_hello.extensions).push_back(Extension::Struct<Extension::ec_point_formats>(allowed_ec_point_formats));
#endif

	if (server_name)
		put(client_hello.extensions).push_back(Extension::Struct<Extension::server_name>(server_name));
//	client_hello.extensions.push_back(Extension::extended_master_secret);

	// send ClientHello
	Record::Message(*this, rb, Record::handshake),
		HandshakeMessage(handshake_messages, rb, Handshake::client_hello),
			write(rb, client_hello);
	rb.flush(io);

	ServerHello			server_hello;
	CertificateRequest	certificate_request;
	CertificateDesc		certificate;
	KeyExchanger		*exchange			= 0;
	Signer				*signer				= 0;
	CipherSuiteInfo*	pending_cipher_info = 0;

	client_random	= client_hello.random;

	PacketReaderSSL	rr;
	Record			r;
	while (streamptr record_end = read_record(r, rr, io)) {
		switch (r.type) {
			case Record::change_cipher_spec: {
				ChangeCipherSpec	m = rr.get();
				set_peer_encryption();
				break;
			}

			case Record::alert: {
				Alert	m = rr.get();
				m.handle();
				break;
			}

			case Record::handshake: {
				for (uint32 end = rr.tell32() + r.length; rr.tell32() < end;) {
					const Handshake	*p = rr.peek_block(sizeof(Handshake));
					handshake_messages += rr.peek_block(sizeof(Handshake) + p->length);
					Handshake	h		= rr.get();
					auto		ender	= rr.with_len(h.length);

					switch (h.type) {
						case Handshake::server_hello:
							rr.read(server_hello);
							server_random		= server_hello.random;
							pending_cipher_info	= find(ciphers, server_hello.cipher_suite);
							exchange			= GetKeyExchanger(get_xchg(pending_cipher_info->key_exchange));

							if (server_hello.extensions.exists()) {
								if (auto *ext = server_hello.extensions->get(Extension::signature_algorithms)) {
									Extension::Struct<Extension::signature_algorithms> peer_sig_hash;
									PacketReader(ext->data).read(peer_sig_hash);
									for (auto i = sig_hash.begin(), e = sig_hash.end(); i != e;) {
										if (!find_check(peer_sig_hash, *i))
											sig_hash.erase(i);
										else
											 ++i;
									}
								}
							}
							break;

						case Handshake::certificate:
							rr.read(certificate);
							if (!certificate.Verify(certificate_authorities))
								;//return false;
							signer	= GetSigner(this, &certificate);
							exchange->client_begin(this, pending_cipher_info, &certificate);
							break;

						case Handshake::server_key_exchange: {
							streamptr	offset	= rr.tell();
							exchange->read_server(rr, this);
							if (signer) {
								size_t			len		= rr.tell() - offset;
								signature		sig		= rr.get();
								HashInterface	ihash	= get_hash_context(sig.algorithm.hash);
								ihash.write(both_random);
								ihash.writebuff(rr.get_data(offset), len);
								if (!signer->verify(sig, ihash.digest()))
									return false;
							}
							break;
						}

						case Handshake::finished: {
							malloc_block	verify_data(rr, rr.remaining());
							return *const_memory_block(addr(calculate_verification(true, handshake_messages.slice_to(-int(sizeof(Handshake) + p->length))))) == *verify_data;
						}

						case Handshake::certificate_request:
							rr.read(certificate_request);
							break;

						case Handshake::server_hello_done:
							session_id		= move(server_hello.session_id);
							version			= server_hello.version;

							{
								Record::Message	record(*this, rb, Record::handshake);

								CertificateDesc	*client_certificate = 0;
								if (!certificate_request.types.empty()) {
									client_certificate = StaticCertificate::find(certificate_request.types, certificate_request.sig_hash, certificate_request.authorities);
									HandshakeMessage	hs(handshake_messages, rb, Handshake::certificate);
										client_certificate ? client_certificate->write(rb) : CertificateDesc().write(rb);
								}

								HandshakeMessage(handshake_messages, rb, Handshake::client_key_exchange),
									exchange->write_client(rb, this);

								//only if client certificate has signing capability (i.e. all certificate except those containing fixed Diffie-Hellman parameters)
								if (client_certificate) {
									HashInterface	ihash	= get_hash_context(client_certificate->alg.hash);
									Signer			*signer	= GetSigner(this, client_certificate);
									HandshakeMessage(handshake_messages, rb, Handshake::certificate_verify),
										signer->sign(client_certificate->alg.hash, ihash(handshake_messages), rand).write(rb);
									delete signer;
								}
							}

							rb.flush(io);

							Record::Message(*this, rb, Record::change_cipher_spec), write(rb, ChangeCipherSpec());
							rb.flush(io);

							set_cipher(pending_cipher_info, false, server_hello.extensions.exists() && server_hello.extensions->get<Extension::extended_master_secret>(), handshake_messages);
							local_encrypted = true;

							Record::Message(*this, rb, Record::handshake),
								HandshakeMessage(handshake_messages, rb, Handshake::finished),
									rb.write(const_memory_block(addr(calculate_verification(false, handshake_messages))));

							rb.flush(io);
							break;

					}
				}
				break;
			}
			case Record::application_data:
				break;

			case Record::heartbeat:
				break;
		}
		rr.seek(record_end);
	}
	return false;
}

//-----------------------------------------------------------------------------
//	ServerConnect
//-----------------------------------------------------------------------------

bool Connection::ServerConnect(iostream_ref io) {//n, ostream_ref out) {
	if (!sig_hash)
		sig_hash = default_sig_hash;

	malloc_block		handshake_messages;

	ServerHello			server_hello;
	ClientHello			client_hello;

	KeyExchanger*		exchange			= 0;
	Signer*				signer				= 0;
	CipherSuiteInfo*	pending_cipher_info = 0;
	CertificateDesc*	certificate			= 0;
	CertificateDesc		client_certificate;

	PacketBuilderSSL	rb;
	PacketReaderSSL		rr;
	Record				r;
	Handshake::Type		expect	= Handshake::client_hello;

	while (streamptr record_end = read_record(r, rr, io)) {
		switch (r.type) {
			case Record::change_cipher_spec: {
				ChangeCipherSpec	m = rr.get();
				set_peer_encryption();
				expect			= Handshake::finished;
				break;
			}

			case Record::alert: {
				Alert	m = rr.get();
				m.handle();
				break;
			}

			case Record::handshake: {
				for (uint32 end = rr.tell() + r.length; rr.tell() < end;) {
					const Handshake	*p = rr.peek_block(sizeof(Handshake));
					handshake_messages += rr.peek_block(sizeof(Handshake) + p->length);

					Handshake	h		= rr.get();
					auto		ender	= rr.with_len(h.length);
					if (h.type != expect)
						return false;

					switch (h.type) {
						case Handshake::client_hello: {
							rr.read(client_hello);

							if (client_hello.session_id) {
							}

							rand.fill(make_range<uint8>(session_id.create(32)));

							CipherSuite	*found = 0;
							for (auto &i : client_hello.cipher_suites) {
								if (found = cipher_suites ? find_check(cipher_suites, i) : find_check(default_cipher_suites, i))
									break;
							}
							if (!found)
								return false;

							pending_cipher_info	= find(ciphers, *found);
							certificate			= StaticCertificate::find(get_sig(pending_cipher_info->key_exchange), sig_hash);
							exchange			= GetKeyExchanger(get_xchg(pending_cipher_info->key_exchange));
							signer				= GetSigner(this, certificate);
							bool server_xkey	= exchange->server_begin(this, pending_cipher_info, certificate);

							server_hello.random.gmt_unix_time = get_unix_time();
							rand.fill(server_hello.random.random);
							server_hello.cipher_suite	= *found;
							server_hello.compression	= NO_COMP;
							server_hello.session_id		= move(session_id);

							server_random	= server_hello.random;
							client_random	= client_hello.random;
							version			= server_hello.version;

							{
								Record::Message		record(*this, rb, Record::handshake);

								HandshakeMessage(handshake_messages, rb, Handshake::server_hello),
									server_hello.write(rb);

								if (certificate) {
									HandshakeMessage(handshake_messages, rb, Handshake::certificate),
										certificate->write(rb);
								}

								if (server_xkey) {
									HandshakeMessage	hs(handshake_messages, rb, Handshake::server_key_exchange);
									streamptr			offset	= rb.tell();
									exchange->write_server(rb, this, &client_hello);

									HashAlgorithm		hash = hash_sha256;
									if (signer) {
										HashInterface	ihash = get_hash_context(hash);
										ihash.write(both_random);
										ihash.writebuff(rb.get_data(offset), rb.tell() - offset);
										signer->sign(hash, ihash.digest(), rand).write(rb);
									}
								}

								HandshakeMessage(handshake_messages, rb, Handshake::server_hello_done);
							}
							rb.flush(io);
							expect	= Handshake::client_key_exchange;	// or certificate
							break;
						}

						case Handshake::certificate:
							rr.read(client_certificate);
							break;

						case Handshake::client_key_exchange: {
							exchange->read_client(rr, this);
							set_cipher(pending_cipher_info, true, server_hello.extensions.exists() && server_hello.extensions->get<Extension::extended_master_secret>(), handshake_messages);
							break;
						}

						case Handshake::certificate_verify: {
							signature		sig		= rr.get();
							HashInterface	ihash	= get_hash_context(client_certificate.alg.hash);
							Signer			*signer	= GetSigner(this, &client_certificate);
							signer->verify(sig, ihash(handshake_messages));
							delete signer;
							break;
						}

						case Handshake::finished: {
							malloc_block	verify_data(rr, rr.remaining());
							if (*const_memory_block(addr(calculate_verification(false, handshake_messages.slice_to(-int(sizeof(Handshake) + p->length))))) != *verify_data)
								return false;

							Record::Message(*this, rb, Record::change_cipher_spec), write(rb, ChangeCipherSpec());
							rb.flush(io);
							local_encrypted = true;

							Record::Message(*this, rb, Record::handshake),
								HandshakeMessage(handshake_messages, rb, Handshake::finished),
									rb.write(const_memory_block(addr(calculate_verification(true, handshake_messages))));
							rb.flush(io);
							return true;
						}
					}
				}
				break;
			}
			case Record::application_data:
				break;

			case Record::heartbeat:
				break;
		}
		rr.seek(record_end);
	}
	return false;
}

//-----------------------------------------------------------------------------
//	streams
//-----------------------------------------------------------------------------

size_t SSL_input::readbuff(Connection &con, istream_ref stream, void *buffer, size_t size) {
	size_t	remaining	= data_end - pr.tell();
	while (remaining == 0) {
		Record	r;
		pr.seek(record_end);
		record_end = con.read_record(r, pr, stream);
		if (!record_end)
			break;

		switch (r.type) {
			case Record::change_cipher_spec:
			case Record::handshake:
				break;

			case Record::alert: {
				Alert	m = pr.get();
				break;
			}
			case Record::application_data:
				remaining	= r.length;
				data_end	= pr.tell() + remaining;
				break;
		}
	}
	return pr.readbuff(buffer, min(size, remaining));
}

size_t SSL_output::writebuff(Connection &con, ostream_ref stream, const void *buffer, size_t size) {
	int	r = (Record::Message(con, rb, Record::application_data),
		rb.writebuff(buffer, size));
//	rb.flush(stream);
	return r;
}
#endif

} }
