#include "device.h"
#include <wincrypt.h>
#include <cryptuiapi.h>

#pragma comment (lib, "crypt32.lib")
#pragma comment (lib, "cryptui.lib")

using namespace iso;

enum CERT_PROP {};

ISO_DEFUSERENUM(CERT_PROP, 96) {
ISO_SETENUMS(0,
	CERT_KEY_PROV_HANDLE_PROP_ID,
	CERT_KEY_PROV_INFO_PROP_ID,
	CERT_SHA1_HASH_PROP_ID,
	CERT_MD5_HASH_PROP_ID,
	CERT_HASH_PROP_ID,
	CERT_KEY_CONTEXT_PROP_ID,
	CERT_KEY_SPEC_PROP_ID,
	CERT_IE30_RESERVED_PROP_ID,
	CERT_PUBKEY_HASH_RESERVED_PROP_ID,
	CERT_ENHKEY_USAGE_PROP_ID,
	CERT_CTL_USAGE_PROP_ID,
	CERT_NEXT_UPDATE_LOCATION_PROP_ID,
	CERT_FRIENDLY_NAME_PROP_ID,
	CERT_PVK_FILE_PROP_ID,
	CERT_DESCRIPTION_PROP_ID,
	CERT_ACCESS_STATE_PROP_ID,
	CERT_SIGNATURE_HASH_PROP_ID,
	CERT_SMART_CARD_DATA_PROP_ID,
	CERT_EFS_PROP_ID,
	CERT_FORTEZZA_DATA_PROP_ID,
	CERT_ARCHIVED_PROP_ID,
	CERT_KEY_IDENTIFIER_PROP_ID,
	CERT_AUTO_ENROLL_PROP_ID,
	CERT_PUBKEY_ALG_PARA_PROP_ID,
	CERT_CROSS_CERT_DIST_POINTS_PROP_ID,
	CERT_ISSUER_PUBLIC_KEY_MD5_HASH_PROP_ID,
	CERT_SUBJECT_PUBLIC_KEY_MD5_HASH_PROP_ID,
	CERT_ENROLLMENT_PROP_ID,
	CERT_DATE_STAMP_PROP_ID,
	CERT_ISSUER_SERIAL_NUMBER_MD5_HASH_PROP_ID,
	CERT_SUBJECT_NAME_MD5_HASH_PROP_ID,
	CERT_EXTENDED_ERROR_INFO_PROP_ID
);
ISO_SETENUMS(32,
	CERT_RENEWAL_PROP_ID,
	CERT_ARCHIVED_KEY_HASH_PROP_ID,
	CERT_AUTO_ENROLL_RETRY_PROP_ID,
	CERT_AIA_URL_RETRIEVED_PROP_ID,
	CERT_AUTHORITY_INFO_ACCESS_PROP_ID,
	CERT_BACKED_UP_PROP_ID,
	CERT_OCSP_RESPONSE_PROP_ID,
	CERT_REQUEST_ORIGINATOR_PROP_ID,
	CERT_SOURCE_LOCATION_PROP_ID,
	CERT_SOURCE_URL_PROP_ID,
	CERT_NEW_KEY_PROP_ID,
	CERT_OCSP_CACHE_PREFIX_PROP_ID,
	CERT_SMART_CARD_ROOT_INFO_PROP_ID,
	CERT_NO_AUTO_EXPIRE_CHECK_PROP_ID,
	CERT_NCRYPT_KEY_HANDLE_PROP_ID,
	CERT_HCRYPTPROV_OR_NCRYPT_KEY_HANDLE_PROP_ID,
	CERT_SUBJECT_INFO_ACCESS_PROP_ID,
	CERT_CA_OCSP_AUTHORITY_INFO_ACCESS_PROP_ID,
	CERT_CA_DISABLE_CRL_PROP_ID,
	CERT_ROOT_PROGRAM_CERT_POLICIES_PROP_ID,
	CERT_ROOT_PROGRAM_NAME_CONSTRAINTS_PROP_ID,
	CERT_SUBJECT_OCSP_AUTHORITY_INFO_ACCESS_PROP_ID,
	CERT_SUBJECT_DISABLE_CRL_PROP_ID,
	CERT_CEP_PROP_ID,
	CERT_SIGN_HASH_CNG_ALG_PROP_ID,
	CERT_SCARD_PIN_ID_PROP_ID,
	CERT_SCARD_PIN_INFO_PROP_ID,
	CERT_SUBJECT_PUB_KEY_BIT_LENGTH_PROP_ID,
	CERT_PUB_KEY_CNG_ALG_BIT_LENGTH_PROP_ID,
	CERT_ISSUER_PUB_KEY_BIT_LENGTH_PROP_ID,
	CERT_ISSUER_CHAIN_SIGN_HASH_CNG_ALG_PROP_ID,
	CERT_ISSUER_CHAIN_PUB_KEY_CNG_ALG_BIT_LENGTH_PROP_ID
);
ISO_SETENUMS(64,
	CERT_NO_EXPIRE_NOTIFICATION_PROP_ID,
	CERT_AUTH_ROOT_SHA256_HASH_PROP_ID,
	CERT_NCRYPT_KEY_HANDLE_TRANSFER_PROP_ID,
	CERT_HCRYPTPROV_TRANSFER_PROP_ID,
	CERT_SMART_CARD_READER_PROP_ID,
	CERT_SEND_AS_TRUSTED_ISSUER_PROP_ID,
	CERT_KEY_REPAIR_ATTEMPTED_PROP_ID,
	CERT_DISALLOWED_FILETIME_PROP_ID,
	CERT_ROOT_PROGRAM_CHAIN_POLICIES_PROP_ID,
	CERT_SMART_CARD_READER_NON_REMOVABLE_PROP_ID,
	CERT_SHA256_HASH_PROP_ID,
	CERT_SCEP_SERVER_CERTS_PROP_ID,
	CERT_SCEP_RA_SIGNATURE_CERT_PROP_ID,
	CERT_SCEP_RA_ENCRYPTION_CERT_PROP_ID,
	CERT_SCEP_CA_CERT_PROP_ID	,
	CERT_SCEP_SIGNER_CERT_PROP_ID,
	CERT_SCEP_NONCE_PROP_ID,
	CERT_SCEP_ENCRYPT_HASH_CNG_ALG_PROP_ID,
	CERT_SCEP_FLAGS_PROP_ID,
	CERT_SCEP_GUID_PROP_ID,
	CERT_SERIALIZABLE_KEY_CONTEXT_PROP_ID,
	CERT_ISOLATED_KEY_PROP_ID,
	CERT_SERIAL_CHAIN_PROP_ID,
	CERT_KEY_CLASSIFICATION_PROP_ID,
	CERT_OCSP_MUST_STAPLE_PROP_ID,
	CERT_DISALLOWED_ENHKEY_USAGE_PROP_ID,
	CERT_NONCOMPLIANT_ROOT_URL_PROP_ID,
	CERT_PIN_SHA256_HASH_PROP_ID,
	CERT_CLR_DELETE_KEY_PROP_ID,
	CERT_NOT_BEFORE_FILETIME_PROP_ID,
	CERT_NOT_BEFORE_ENHKEY_USAGE_PROP_ID,
	CERT_FIRST_RESERVED_PROP_ID
);
}};


namespace iso {
	template<> struct string_getter<const CERT_CONTEXT*> {
		const CERT_CONTEXT *ctx;
		string_getter(const CERT_CONTEXT *_ctx) : ctx(_ctx) {}
		size_t	len()							const	{ return CertGetNameStringW(ctx, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, NULL, 0, 0) - 1; }
		size_t	get(char *s, size_t len)		const	{ return CertGetNameStringA(ctx, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, NULL, s, uint32(len + 1)) - 1; }
		size_t	get(char16 *s, size_t len)		const	{ return CertGetNameStringW(ctx, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, NULL, s, uint32(len + 1)) - 1; }
	};
}

struct Certificate {
	const CERT_CONTEXT   *ctx;

	struct iterator {
		typedef forward_iterator_t iterator_category;
		typedef malloc_block element, reference;
		const CERT_CONTEXT	*ctx;
		DWORD				prop;
		iterator(const CERT_CONTEXT	*_ctx) : ctx(_ctx), prop(_ctx ? CertEnumCertificateContextProperties(ctx, 0) : 0) {}
		iterator& operator++()	{ prop = CertEnumCertificateContextProperties(ctx, prop); return *this; }
		pair<CERT_PROP, malloc_block>	operator*() const {
			malloc_block	mem;
			DWORD			size;
			if (CertGetCertificateContextProperty(ctx, prop, 0, &size)) {
				mem.create(size);
				CertGetCertificateContextProperty(ctx, prop, mem, &size);
			}
			return pair<CERT_PROP, malloc_block>((CERT_PROP)prop, mem);
		}
		bool	operator==(const iterator &b) const { return prop == b.prop; }
		bool	operator!=(const iterator &b) const { return prop != b.prop; }
	};

	Certificate(const CERT_CONTEXT *_ctx) : ctx(_ctx) {}
	string_getter<const CERT_CONTEXT*>	name() { return ctx; }
	iterator	begin()	const	{ return ctx; }
	iterator	end()	const	{ return 0; }

	bool	DialogView(HWND hWnd, const char16 *title, uint32 flags) const {
		return CryptUIDlgViewContext(CERT_STORE_CERTIFICATE_CONTEXT, ctx, hWnd, title, flags, NULL);
	}
};

struct CertificateStore {
	HCERTSTORE       store;
	struct iterator {
		typedef forward_iterator_t iterator_category;
		typedef Certificate element, reference;
		HCERTSTORE			store;
		const CERT_CONTEXT	*ctx;
		iterator(HCERTSTORE _store) : store(_store), ctx(store ? CertEnumCertificatesInStore(store, 0) : 0) {}
//		~iterator()				{ CertFreeCertificateContext(ctx); }
		iterator&		operator++()							{ ctx = CertEnumCertificatesInStore(store, ctx); return *this; }
		Certificate		operator*()						const	{ return ctx; }
		ref_helper<Certificate>	operator->()			const	{ return ctx; }
		bool			operator==(const iterator &b)	const	{ return ctx == b.ctx; }
		bool			operator!=(const iterator &b)	const	{ return ctx != b.ctx; }
	};
	CertificateStore(const char *name)
	//	: store(CertOpenStore(CERT_STORE_PROV_SYSTEM_A, X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, 0, CERT_SYSTEM_STORE_LOCAL_MACHINE, name))
	//	: store(CertOpenSystemStore(NULL, name))
		: store(CertOpenStore(CERT_STORE_PROV_SYSTEM, 0, NULL, CERT_SYSTEM_STORE_CURRENT_USER, L"CA"))
	{}
	~CertificateStore()			{ CertCloseStore(store, 0); }
	iterator	begin()	const	{ return store; }
	iterator	end()	const	{ return 0; }

	const CERT_CONTEXT	*DialogOpen(HWND hWnd, const char16 *title, const char16 *display, uint32 dontuse, uint32 flags) {
		return CryptUIDlgSelectCertificateFromStore(store, hWnd, title, display, dontuse, flags, 0);
	}
};

template<> struct ISO::def<Certificate> : VirtualT2<Certificate> {
	static int		Count(Certificate &c)			{ return num_elements32(c);	}
	tag2			GetName(Certificate &c, int i)	{
		auto	*e = (const TypeEnum*)getdef<CERT_PROP>()->SkipUser();
		return e->biggest_factor((*nth(c.begin(), i)).a)->id.get_tag2(e->flags & e->CRCIDS);
	}
	ISO::Browser2	Index(Certificate &c, int i)	{ return MakeBrowser(*nth(c.begin(), i)); }
};

template<> struct ISO::def<CertificateStore> : VirtualT2<CertificateStore> {
	int				Count(CertificateStore &s)			{ return num_elements32(s);	}
	tag2			GetName(CertificateStore &s, int i)	{ return string16(nth(s.begin(), i)->name()); }
	ISO::Browser2	Index(CertificateStore &s, int i)	{ return MakeBrowser(*nth(s.begin(), i)); }
};

struct CertificatesDevice : app::DeviceT<CertificatesDevice>, app::DeviceCreateT<CertificatesDevice> {
	void			operator()(const app::DeviceAdd &add) { add("Certificates", this, app::LoadPNG("IDB_DEVICE_PROCESSES")); }
	ISO_ptr<void>	operator()(const win::Control &main) { return ISO_ptr<CertificateStore>("Certificates", "CA"); }
} certificates_device;
