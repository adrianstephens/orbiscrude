#include "ssl_win.h"
#include "base/array.h"

#pragma comment(lib, "Secur32.lib")
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "crypt32.lib")

using namespace iso;


struct Buffer : SecBuffer {
	void	Set(uint32 type, uint32 length, void *p) {
		BufferType	= type;
		cbBuffer	= length;
		pvBuffer	= p;
	}
	memory_block	GetBuffer() {
		return memory_block(pvBuffer, cbBuffer);
	}
};

template<int N> struct Buffers : SecBufferDesc, fixed_array<Buffer, N> {
	Buffers() {
		ulVersion	= SECBUFFER_VERSION;
		pBuffers	= *this;
		cBuffers	= N;
	}
};

int SSL_SOCKET::s_recv(char* b, int sz) {
	if (pending_data_size) {
		int	n = min(pending_data_size, sz);
		pending_data_size -= n;
		memcpy(b, pending_data, n);
		memcpy(pending_data, pending_data + n, pending_data_size);
		return sz;
	}

	SecPkgContext_StreamSizes	sizes;
	SECURITY_STATUS				ss = QueryContextAttributes(&hCtx, SECPKG_ATTR_STREAM_SIZES, &sizes);
	if (FAILED(ss))
		return -1;

	int				TotalR	= 0;
	int				pI		= 0;
	malloc_block	mmsg(sizes.cbMaximumMessage * 10);
	Buffers<4>		buffs;

	ss = SEC_E_INCOMPLETE_MESSAGE;
	while (ss != SEC_E_OK) {
		if (extra_data_size) {
			memcpy(mmsg + pI, extra_data, extra_data_size);
			pI += extra_data_size;
			extra_data_size = 0;
		} else {
			int	rval = socket_receive(sock, mmsg + pI, sizes.cbMaximumMessage);
			if (rval == 0 || rval == -1)
				return rval;
			pI += rval;
		}

		buffs[0].Set(SECBUFFER_DATA, pI, mmsg);
		buffs[1].BufferType	= SECBUFFER_EMPTY;
		buffs[2].BufferType	= SECBUFFER_EMPTY;
		buffs[3].BufferType	= SECBUFFER_EMPTY;

		ss = DecryptMessage(&hCtx, &buffs, 0, NULL);

		if (ss == SEC_I_RENEGOTIATE) {
			ss = Wait();
			if (FAILED(ss))
				return -1;
		}

		if (ss != SEC_E_INCOMPLETE_MESSAGE && ss != SEC_E_OK && ss != SEC_I_CONTEXT_EXPIRED)
			return -1;
	}

	for (auto &i : buffs) {
		if (i.BufferType == SECBUFFER_DATA) {
			TotalR = i.cbBuffer;
			if (TotalR <= sz) {
				memcpy(b, i.pvBuffer, TotalR);
			} else {
				TotalR = sz;
				memcpy(b, i.pvBuffer, TotalR);
				pending_data_size = i.cbBuffer - TotalR;
				pending_data.resize(pending_data_size + 100);
				memcpy(pending_data, (char*)i.pvBuffer + TotalR, pending_data_size);
			}
		}
		if (i.BufferType == SECBUFFER_EXTRA) {
			extra_data_size = i.cbBuffer;
			extra_data.resize(extra_data_size + 10);
			memcpy(extra_data, i.pvBuffer, extra_data_size);
			pI = 0;
		}
	}

	return TotalR;
}

int SSL_SOCKET::s_ssend(char* b, int sz) {
	SecPkgContext_StreamSizes sizes;
	SECURITY_STATUS ss = QueryContextAttributes(&hCtx, SECPKG_ATTR_STREAM_SIZES, &sizes);
	if (FAILED(ss))
		return -1;

	malloc_block mmsg(sizes.cbMaximumMessage);
	malloc_block mhdr(sizes.cbHeader);
	malloc_block mtrl(sizes.cbTrailer);

	int mPos = 0;
	while (mPos != sz) {
		uint32 dwMessage = min(sz - mPos, sizes.cbMaximumMessage);
		memcpy(mmsg, b + mPos, dwMessage);

		Buffers<4>	buffs;
		buffs[0].Set(SECBUFFER_STREAM_HEADER, sizes.cbHeader, mhdr);
		buffs[1].Set(SECBUFFER_DATA, dwMessage, mmsg);
		buffs[2].Set(SECBUFFER_STREAM_TRAILER, sizes.cbTrailer, mtrl);
		buffs[3].Set(SECBUFFER_EMPTY, 0, 0);

		ss = EncryptMessage(&hCtx, 0, &buffs, 0);
		if (FAILED(ss))
			return -1;

		// Send this message
		int	rval;
		rval = socket_send_all(sock, buffs[0].GetBuffer());
		if (rval != buffs[0].cbBuffer)
			break;
		rval = socket_send_all(sock, buffs[1].GetBuffer());
		if (rval != buffs[1].cbBuffer)
			break;
		rval = socket_send_all(sock, buffs[2].GetBuffer());
		if (rval != buffs[2].cbBuffer)
			break;

		mPos += dwMessage;
	}

	return mPos;
}

int SSL_SOCKET::Disconnect() {
	DWORD			dwType		= SCHANNEL_SHUTDOWN;
	Buffers<1>		buffs;

	buffs[0].Set(SECBUFFER_TOKEN, sizeof(dwType), &dwType);

	for (;;) {
		SECURITY_STATUS ss = ApplyControlToken(&hCtx, &buffs);
		if (FAILED(ss))
			return -1;

		DWORD	flags = ISC_REQ_SEQUENCE_DETECT | ISC_REQ_REPLAY_DETECT | ISC_REQ_CONFIDENTIALITY | ISC_RET_EXTENDED_ERROR | ISC_REQ_ALLOCATE_MEMORY | ISC_REQ_STREAM;

		buffs[0].pvBuffer = NULL;
		buffs[0].cbBuffer = 0;

		ss = server_side
			? AcceptSecurityContext(&hCred, &hCtx, NULL, flags, SECURITY_NATIVE_DREP, NULL, &buffs, &flags, 0)
			: InitializeSecurityContext(&hCred, &hCtx, NULL, flags, 0, SECURITY_NATIVE_DREP, NULL, 0, &hCtx, &buffs, &flags, 0);

		if (FAILED(ss))
			return -1;

		if (buffs[0].pvBuffer) {
			int rval = socket_send_all(sock, buffs[0].GetBuffer());
			FreeContextBuffer(buffs[0].pvBuffer);
			return rval;
		}
		break;
	}
	return 1;
}

bool SSL_SOCKET::Init() {
	if (!external_certificate)
		our_certificate = Certificate::Create();

	if (our_certificate) {
		schannel_cred.cCreds = 1;
		schannel_cred.paCred = &our_certificate;
	}

	schannel_cred.cSupportedAlgs	= algs.size();
	schannel_cred.palgSupportedAlgs = algs;
	return SUCCEEDED(AcquireCredentialsHandle(0, SCHANNEL_NAME, server_side ? SECPKG_CRED_INBOUND : SECPKG_CRED_OUTBOUND, 0, &schannel_cred, 0, 0, &hCred, 0));
}

int SSL_SOCKET::Wait() {
	// Loop AcceptSecurityContext
	SECURITY_STATUS ss = SEC_I_CONTINUE_NEEDED;
	malloc_block	t(0x11000);
	Buffers<2>		buffsi, buffso;

	int				pt	= 0;
	buffso[0].Set(SECBUFFER_TOKEN, 0, 0);

	// Loop using InitializeSecurityContext until success
	while (ss != S_OK) {
		DWORD flags = 0;

		if (server_side || init_context) {
			// Add also extradata?
			if (extra_data_size) {
				memcpy(t, extra_data, extra_data_size);
				pt += extra_data_size;
				extra_data_size = 0;
			}
			int rval = socket_receive(sock, t + pt, 0x10000);
			if (rval == 0 || rval == -1)
				return rval;
			pt += rval;
		}

		// Put this data into the buffer so InitializeSecurityContext will do
		buffsi[0].Set( SECBUFFER_TOKEN, pt, t);
		buffsi[1].Set(SECBUFFER_EMPTY, 0, 0);
		buffsi.cBuffers		= 2;
				
		if (server_side) {
			buffso[1].Set(SECBUFFER_EMPTY, 0, 0);
			buffso.cBuffers		= 2;
			ss = AcceptSecurityContext(
				&hCred,
				init_context ? &hCtx : 0,
				&buffsi,
				ASC_REQ_ALLOCATE_MEMORY, 0,
				init_context ? 0 : &hCtx,
				&buffso,
				&flags,
				0);

			init_context = true;

		} else {

			buffso.cBuffers		= 1;
			ss = InitializeSecurityContext(
				&hCred,
				init_context ? &hCtx : 0,
				destination_name,
				ISC_REQ_SEQUENCE_DETECT | ISC_REQ_REPLAY_DETECT | ISC_REQ_CONFIDENTIALITY | ISC_RET_EXTENDED_ERROR | ISC_REQ_ALLOCATE_MEMORY | ISC_REQ_STREAM | ISC_REQ_MANUAL_CRED_VALIDATION,
				0,
				0,//SECURITY_NATIVE_DREP,
				init_context ? &buffsi : 0,
				0,
				init_context ? 0 : &hCtx,
				&buffso,
				&flags,
				0);
		}

		if (ss == SEC_E_INCOMPLETE_MESSAGE)
			continue; // allow more

		pt = 0;

		if (FAILED(ss) || (!init_context && ss != SEC_I_CONTINUE_NEEDED))
			return -1;

		// Pass data to the remote site
		int rval = socket_send_all(sock, buffso[0].GetBuffer());
		FreeContextBuffer(buffso[0].pvBuffer);
		if (rval != buffso[0].cbBuffer)
			return -1;
	
		init_context = true;
	}
	return 0;
}

const CERT_CONTEXT *Certificate::Create() {
	// CertCreateSelfSignCertificate(0,&SubjectName,0,0,0,0,0,0);
	BYTE			cb[1000];
	CERT_NAME_BLOB	sib		= { sizeof(cb), cb };
	HCRYPTPROV		hProv	= NULL;
	HCRYPTKEY		hKey	= 0;

	// Step by step to create our own certificate

	// Create the subject
	if (!CertStrToName(CRYPT_ASN_ENCODING, "CN=Certificate", 0, 0, sib.pbData, &sib.cbData, NULL))
		return 0;

	// Acquire Context
	if (!CryptAcquireContext(&hProv, "Container", MS_DEF_PROV, PROV_RSA_FULL, CRYPT_NEWKEYSET | CRYPT_MACHINE_KEYSET)) {
		if (GetLastError() != NTE_EXISTS || !CryptAcquireContext(&hProv, "Container", MS_DEF_PROV, PROV_RSA_FULL, CRYPT_MACHINE_KEYSET))
			return 0;
	}

	// Generate KeyPair
	if (!CryptGenKey(hProv, AT_KEYEXCHANGE, CRYPT_EXPORTABLE, &hKey))
		return 0;

	// Generate the certificate
	CRYPT_KEY_PROV_INFO kpi = { 0 };
	kpi.pwszContainerName	= L"Container";
	kpi.pwszProvName		= MS_DEF_PROV_W;
	kpi.dwProvType			= PROV_RSA_FULL;
	kpi.dwFlags				= CERT_SET_KEY_CONTEXT_PROP_ID;
	kpi.dwKeySpec			= AT_KEYEXCHANGE;

	SYSTEMTIME et;
	GetSystemTime(&et);
	et.wYear += 1;

	CERT_EXTENSIONS exts = { 0 };
	const CERT_CONTEXT *ctx = CertCreateSelfSignCertificate(hProv, &sib, 0, &kpi, NULL, NULL, &et, &exts);

	bool provider = CryptFindCertificateKeyProvInfo(ctx, CRYPT_FIND_MACHINE_KEYSET_FLAG, NULL);

	if (hKey)
		CryptDestroyKey(hKey);

	if (hProv)
		CryptReleaseContext(hProv, 0);
	return ctx;
}

SECURITY_STATUS Certificate::Verify() {
	if (ctx == 0)
		return SEC_E_WRONG_PRINCIPAL;

	// Time
	if (CertVerifyTimeValidity(NULL, ctx->pCertInfo) != 0)
		return SEC_E_CERT_EXPIRED;

	// Chain
	CERT_CHAIN_PARA			chain_para = { 0 };
	PCCERT_CHAIN_CONTEXT	chain_ctx = NULL;
	chain_para.cbSize	= sizeof(chain_para);
	if (!CertGetCertificateChain(0, ctx, 0, 0, &chain_para, 0, 0, &chain_ctx))
		return SEC_E_INVALID_TOKEN;

	CERT_REVOCATION_STATUS cs = { 0 };
	cs.cbSize = sizeof(cs);
	SECURITY_STATUS ss = CertVerifyRevocation(X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, CERT_CONTEXT_REVOCATION_TYPE, 1, (void**)&ctx, 0, 0, &cs);
	
	if (chain_ctx)
		CertFreeCertificateChain(chain_ctx);

	return ss;
}

struct CryptoBlob : _CRYPTOAPI_BLOB {
	CryptoBlob() {}
	CryptoBlob(const char16 *p) {
		pbData = (BYTE*)p;
		cbData = (string_len(p) + 1) * sizeof(char16);
	}
	CryptoBlob(void *p, uint32 size) {
		cbData = size;
		pbData = (BYTE*)p;
	}
	CryptoBlob(const memory_block &b) {
		cbData = b.length32();
		pbData = b;
	}
	operator _CRYPTOAPI_BLOB*() { return this; }
};

// based on a sample found at:
// http://blogs.msdn.com/b/alejacma/archive/2009/03/16/how-to-create-a-self-signed-certificate-with-cryptoapi-c.aspx
// Create a self-signed certificate and store it in the machine personal store

const CERT_CONTEXT *Certificate::Create(bool machine_cert, const string16 &subject, const string16 &friendly_name, const string16 &description) {
	// CREATE KEY PAIR FOR SELF-SIGNED CERTIFICATE

	HCRYPTPROV	hCryptProv		= NULL;
	HCRYPTKEY	hKey			= NULL;
	WCHAR*		container_name	= L"SSLTestKeyContainer";
	DWORD		key_flags		= machine_cert ? CRYPT_MACHINE_KEYSET : 0;

	// Acquire key container
	if (!CryptAcquireContextW(&hCryptProv, container_name, NULL, PROV_RSA_FULL, key_flags)) {
		// Try to create a new key container
		if (!CryptAcquireContextW(&hCryptProv, container_name, NULL, PROV_RSA_FULL, key_flags | CRYPT_NEWKEYSET))
			return 0;
	}

	// Generate new key pair
	if (!CryptGenKey(hCryptProv, AT_SIGNATURE, 0x08000000 /*RSA-2048-BIT_KEY*/, &hKey))
		return 0;

	// Clean up  
	if (hKey)
		CryptDestroyKey(hKey);

	if (hCryptProv)
		CryptReleaseContext(hCryptProv, 0);

	// CREATE SELF-SIGNED CERTIFICATE AND ADD IT TO PERSONAL STORE IN MACHINE PROFILE

	// Encode certificate Subject
	string16	X500 = "CN=" + subject;

	DWORD	length = 0;
	// Find out how many bytes are needed to encode the certificate
	if (!CertStrToNameW(X509_ASN_ENCODING, X500, CERT_X500_NAME_STR, NULL, 0, &length, NULL))
		return 0;

	// Allocate the required space
	malloc_block	encoded(length);
	
	// Encode the certificate
	if (!CertStrToNameW(X509_ASN_ENCODING, X500, CERT_X500_NAME_STR, NULL, encoded, &length, NULL))
		return 0;

	// Prepare key provider structure for certificate
	CRYPT_KEY_PROV_INFO			provider;
	clear(provider);
	provider.pwszContainerName	= container_name; // The key we made earlier
	provider.pwszProvName		= NULL;
	provider.dwProvType			= PROV_RSA_FULL;
	provider.dwFlags			= key_flags;
	provider.cProvParam			= 0;
	provider.rgProvParam		= NULL;
	provider.dwKeySpec			= AT_SIGNATURE;

	// Prepare algorithm structure for certificate
	CRYPT_ALGORITHM_IDENTIFIER	algorithm;
	clear(algorithm);
	algorithm.pszObjId			= szOID_RSA_SHA1RSA;

	// Prepare Expiration date for certificate
	SYSTEMTIME					end_time;
	GetSystemTime(&end_time);
	end_time.wYear				+= 5;

	// Create certificate
	const CERT_CONTEXT	*cert_ctx = CertCreateSelfSignCertificate(NULL, CryptoBlob(encoded), 0, &provider, &algorithm, 0, &end_time, 0);
	if (!cert_ctx)
		return 0;

	// Specify the allowed usage of the certificate (client or server authentication)
	if (!CertAddEnhancedKeyUsageIdentifier(cert_ctx, machine_cert ? szOID_PKIX_KP_SERVER_AUTH : szOID_PKIX_KP_CLIENT_AUTH))
		return 0;

	// Give the certificate a friendly name
	if (!CertSetCertificateContextProperty(cert_ctx, CERT_FRIENDLY_NAME_PROP_ID, 0, CryptoBlob(friendly_name)))
		return 0;

	// Give the certificate a description
	if (!CertSetCertificateContextProperty(cert_ctx, CERT_DESCRIPTION_PROP_ID, 0, CryptoBlob(description)))
		return 0;

	// Open Personal cert store in machine or user profile
	HCERTSTORE	hStore	= CertOpenStore(CERT_STORE_PROV_SYSTEM, 0, 0, machine_cert ? CERT_SYSTEM_STORE_LOCAL_MACHINE : CERT_SYSTEM_STORE_CURRENT_USER, L"My");
	if (!hStore)
		return 0;

	// Add the cert to the store
	if (!CertAddCertificateContextToStore(hStore, cert_ctx, CERT_STORE_ADD_REPLACE_EXISTING, 0))
		return 0;

	// Just for testing, verify that we can access cert's private key
	DWORD							key_spec;
	BOOL							caller_free = FALSE;
	HCRYPTPROV_OR_NCRYPT_KEY_HANDLE hCryptProvOrNCryptKey = NULL;
	if (!CryptAcquireCertificatePrivateKey(cert_ctx, 0, NULL, &hCryptProvOrNCryptKey, &key_spec, &caller_free))
		return 0;

	// Clean up
	if (hCryptProvOrNCryptKey)
		CryptReleaseContext(hCryptProvOrNCryptKey, 0);

	if (hStore)
		CertCloseStore(hStore, 0);

	return cert_ctx;
}

// Utility function to get the hostname of the host I am running on
string GetHostName(COMPUTER_NAME_FORMAT WhichName) {
	DWORD length = 0;
	if (GetComputerNameEx(WhichName, NULL, &length) == ERROR_SUCCESS) {
		string name(length);
		if (GetComputerNameEx(WhichName, name, &length))
			return name;
	}
	return "";
}

// Utility function to return the user name I'm runng under
string GetUserName() {
	DWORD length = 0;
	if (GetUserName(NULL, &length) == ERROR_SUCCESS) {
		string name(length);
		if (GetUserName(name, &length))
			return name;
	}
	return "";
}

// Select, and return a handle to a client certificate
// We take a best guess at a certificate to be used as the SSL certificate for this client 
PCCERT_CONTEXT CertFindClient(const char *subject_name) {
	HCERTSTORE  hCS = CertOpenSystemStore(NULL, "MY");
	if (!hCS)
		return 0;

	char	*serverauth = szOID_PKIX_KP_CLIENT_AUTH;
	CERT_ENHKEY_USAGE	eku;
	eku.cUsageIdentifier		= 1;
	eku.rgpszUsageIdentifier	= &serverauth;
	// Find a client certificate.
	// Note that this code just searches for a certificate that has the required enhanced key usage for server authentication it then selects the best one (ideally one that contains the client name somewhere in the subject name)

	PCCERT_CONTEXT	cert_ctx = NULL;
	for (PCCERT_CONTEXT cert_ctx_curr = 0; cert_ctx_curr = CertFindCertificateInStore(hCS, X509_ASN_ENCODING, CERT_FIND_OPTIONAL_ENHKEY_USAGE_FLAG, CERT_FIND_ENHKEY_USAGE, &eku, cert_ctx_curr);) {
		char		friendly_name[128];
		if (!CertGetNameString(cert_ctx_curr, CERT_NAME_FRIENDLY_DISPLAY_TYPE, 0, NULL, friendly_name, sizeof(friendly_name)))
			continue;

		char		name_string[128];
		if (!CertGetNameString(cert_ctx_curr, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, NULL, name_string, sizeof(name_string)))
			continue;

		// We must be able to access cert's private key
		HCRYPTPROV_OR_NCRYPT_KEY_HANDLE hCryptProvOrNCryptKey = NULL;
		BOOL	caller_free = FALSE;
		DWORD	key_spec;
		if (!CryptAcquireCertificatePrivateKey(cert_ctx_curr, 0, NULL, &hCryptProvOrNCryptKey, &key_spec, &caller_free))
			continue; // Since it has no private key it is useless, just go on to the next one

		// The minimum requirements are now met, 
		if (cert_ctx)
			CertFreeCertificateContext(cert_ctx);

		cert_ctx = CertDuplicateCertificateContext(cert_ctx_curr);
		if (!subject_name || str(name_string) == str(subject_name))
			break;
	}

	return cert_ctx;
}

PCCERT_CONTEXT CertFindFromIssuerList(SecPkgContext_IssuerListInfoEx &info) {
	// Enumerate possible client certificates.
	CERT_CHAIN_FIND_BY_ISSUER_PARA	find = { 0 };
	find.cbSize				= sizeof(find);
	find.pszUsageIdentifier = szOID_PKIX_KP_CLIENT_AUTH;
	find.dwKeySpec			= 0;
	find.cIssuer			= info.cIssuers;
	find.rgIssuer			= info.aIssuers;

	PCCERT_CHAIN_CONTEXT chain_ctx	= NULL;
	HCERTSTORE			hCS			= CertOpenSystemStore(NULL, "MY");

	for (;;) {
		// Find a certificate chain.
		chain_ctx = CertFindChainInStore(hCS,
			X509_ASN_ENCODING,
			0,
			CERT_CHAIN_FIND_BY_ISSUER,
			&find,
			chain_ctx
		);
		if (!chain_ctx)
			return 0;

		// Get pointer to leaf certificate context.
		PCCERT_CONTEXT	cert_ctx = CertDuplicateCertificateContext(chain_ctx->rgpChain[0]->rgpElement[0]->pCertContext);
		CertFreeCertificateChain(chain_ctx);
		return cert_ctx;
	}
	return 0;
}

PCCERT_CONTEXT FindCertificateByName(const string &subject_name) {
	HCERTSTORE  hCS = CertOpenSystemStore(NULL, "MY");
	if (!hCS)
		return 0;

	return CertFindCertificateInStore(hCS,
		X509_ASN_ENCODING,
		0,
		CERT_FIND_SUBJECT_STR,
		subject_name,
		NULL);
}

// Return an indication of whether a certificate is trusted by asking Windows to validate the trust chain
HRESULT Certificate::Trusted() {
	LPSTR usages[] = {
		szOID_PKIX_KP_SERVER_AUTH,
		szOID_SERVER_GATED_CRYPTO,
		szOID_SGC_NETSCAPE
	};

	// Build certificate chain.
	CERT_CHAIN_PARA          chain_para;
	clear(chain_para);
	chain_para.cbSize										= sizeof(chain_para);
	chain_para.RequestedUsage.dwType						= USAGE_MATCH_TYPE_OR;
	chain_para.RequestedUsage.Usage.cUsageIdentifier		= num_elements(usages);;
	chain_para.RequestedUsage.Usage.rgpszUsageIdentifier	= usages;

	PCCERT_CHAIN_CONTEXT     chain_ctx = NULL;
	if (!CertGetCertificateChain(NULL, ctx, NULL, ctx->hCertStore, &chain_para, 0, NULL, &chain_ctx))
		return GetLastError();

	// Validate certificate chain.
	HTTPSPolicyCallbackData  polHttps;
	clear(polHttps);
	polHttps.cbStruct		= sizeof(HTTPSPolicyCallbackData);
	polHttps.dwAuthType		= AUTHTYPE_SERVER;
	polHttps.fdwChecks		= 0;    // dwCertFlags;
	polHttps.pwszServerName = NULL; // ServerName - checked elsewhere

	CERT_CHAIN_POLICY_PARA   policy_para;
	clear(policy_para);
	policy_para.cbSize = sizeof(policy_para);
	policy_para.pvExtraPolicyPara = &polHttps;

	CERT_CHAIN_POLICY_STATUS policy_status;
	clear(policy_status);
	policy_status.cbSize = sizeof(policy_status);

	HRESULT		status = !CertVerifyCertificateChainPolicy(CERT_CHAIN_POLICY_SSL, chain_ctx, &policy_para, &policy_status)
		? GetLastError()
		: policy_status.dwError ? S_FALSE :  SEC_E_OK;

	if (chain_ctx)
		CertFreeCertificateChain(chain_ctx);

	return status;
}

bool Certificate::DumpInfo() {
	static struct {int id; const char *name;} prop_ids[] = {
		{CERT_KEY_PROV_HANDLE_PROP_ID,							"Key provider handle"						},
		{CERT_KEY_PROV_INFO_PROP_ID,							"Key provider info"							},
		{CERT_SHA1_HASH_PROP_ID,								"SHA1 hash"									},
		{CERT_MD5_HASH_PROP_ID,									"MD5 hash"									},
		{CERT_KEY_CONTEXT_PROP_ID,								"Key context"								},
		{CERT_KEY_SPEC_PROP_ID,									"Key spec"									},
		{CERT_IE30_RESERVED_PROP_ID,							"Ie30 reserved"								},
		{CERT_PUBKEY_HASH_RESERVED_PROP_ID,						"Pubkey hash reserved"						},
		{CERT_ENHKEY_USAGE_PROP_ID,								"Enhkey usage"								},
		{CERT_NEXT_UPDATE_LOCATION_PROP_ID,						"Next update location"						},
		{CERT_FRIENDLY_NAME_PROP_ID,							"Friendly name"								},
		{CERT_PVK_FILE_PROP_ID,									"Pvk file"									},
		{CERT_DESCRIPTION_PROP_ID,								"Description"								},
		{CERT_ACCESS_STATE_PROP_ID,								"Access state"								},
		{CERT_SIGNATURE_HASH_PROP_ID,							"Signature hash"							},
		{CERT_SMART_CARD_DATA_PROP_ID,							"Smart card data"							},
		{CERT_EFS_PROP_ID,										"Efs"										},
		{CERT_FORTEZZA_DATA_PROP_ID,							"Fortezza data"								},
		{CERT_ARCHIVED_PROP_ID,									"Archived"									},
		{CERT_KEY_IDENTIFIER_PROP_ID,							"Key identifier"							},
		{CERT_AUTO_ENROLL_PROP_ID,								"Auto enroll"								},
		{CERT_PUBKEY_ALG_PARA_PROP_ID,							"Pubkey alg parameter"						},
		{CERT_CROSS_CERT_DIST_POINTS_PROP_ID,					"Cross cert dist points"					},
		{CERT_ISSUER_PUBLIC_KEY_MD5_HASH_PROP_ID,				"Issuer public key MD5 hash"				},
		{CERT_SUBJECT_PUBLIC_KEY_MD5_HASH_PROP_ID,				"Subject public key MD5 hash"				},
		{CERT_ENROLLMENT_PROP_ID,								"Enrollment"								},
		{CERT_DATE_STAMP_PROP_ID,								"Date stamp"								},
		{CERT_ISSUER_SERIAL_NUMBER_MD5_HASH_PROP_ID,			"Issuer serial number MD5 hash"				},
		{CERT_SUBJECT_NAME_MD5_HASH_PROP_ID,					"Subject name MD5 hash"						},
		{CERT_EXTENDED_ERROR_INFO_PROP_ID,						"Extended error info"						},
		{CERT_RENEWAL_PROP_ID,									"Renewal"									},
		{CERT_ARCHIVED_KEY_HASH_PROP_ID,						"Archived key hash"							},
		{CERT_AUTO_ENROLL_RETRY_PROP_ID,						"Auto enroll retry"							},
		{CERT_AIA_URL_RETRIEVED_PROP_ID,						"Aia url retrieved"							},
		{CERT_AUTHORITY_INFO_ACCESS_PROP_ID,					"Authority info access"						},
		{CERT_BACKED_UP_PROP_ID,								"Backed up"									},
		{CERT_OCSP_RESPONSE_PROP_ID,							"OCSP response"								},
		{CERT_REQUEST_ORIGINATOR_PROP_ID,						"Request originator"						},
		{CERT_SOURCE_LOCATION_PROP_ID,							"Source location"							},
		{CERT_SOURCE_URL_PROP_ID,								"Source url"								},
		{CERT_NEW_KEY_PROP_ID,									"New key"									},
		{CERT_OCSP_CACHE_PREFIX_PROP_ID,						"Ocsp cache prefix"							},
		{CERT_SMART_CARD_ROOT_INFO_PROP_ID,						"Smart card root info"						},
		{CERT_NO_AUTO_EXPIRE_CHECK_PROP_ID,						"No auto expire check"						},
		{CERT_NCRYPT_KEY_HANDLE_PROP_ID,						"Ncrypt key handle"							},
		{CERT_HCRYPTPROV_OR_NCRYPT_KEY_HANDLE_PROP_ID,			"Hcryptprovider or ncrypt key handle"		},
		{CERT_SUBJECT_INFO_ACCESS_PROP_ID,						"Subject info access"						},
		{CERT_CA_OCSP_AUTHORITY_INFO_ACCESS_PROP_ID,			"CA OCSP authority info access"				},
		{CERT_CA_DISABLE_CRL_PROP_ID,							"CA disable crl"							},
		{CERT_ROOT_PROGRAM_CERT_POLICIES_PROP_ID,				"Root program cert policies"				},
		{CERT_ROOT_PROGRAM_NAME_CONSTRAINTS_PROP_ID,			"Root program name constraints"				},
		{CERT_SUBJECT_OCSP_AUTHORITY_INFO_ACCESS_PROP_ID,		"Subject OCSP authority info access"		},
		{CERT_SUBJECT_DISABLE_CRL_PROP_ID,						"Subject disable crl"						},
		{CERT_CEP_PROP_ID,										"Cep"										},
		{CERT_SIGN_HASH_CNG_ALG_PROP_ID,						"Sign hash cng alg"							},
		{CERT_SCARD_PIN_ID_PROP_ID,								"Scard PIN id"								},
		{CERT_SCARD_PIN_INFO_PROP_ID,							"Scard PIN info"							},
		{CERT_SUBJECT_PUB_KEY_BIT_LENGTH_PROP_ID,				"Subject pub key bit length"				},
		{CERT_PUB_KEY_CNG_ALG_BIT_LENGTH_PROP_ID,				"Pub key cng alg bit length"				},
		{CERT_ISSUER_PUB_KEY_BIT_LENGTH_PROP_ID,				"Issuer pub key bit length"					},
		{CERT_ISSUER_CHAIN_SIGN_HASH_CNG_ALG_PROP_ID,			"Issuer chain sign hash cng alg"			},
		{CERT_ISSUER_CHAIN_PUB_KEY_CNG_ALG_BIT_LENGTH_PROP_ID,	"Issuer chain pub key cng alg bit length"	},
		{CERT_NO_EXPIRE_NOTIFICATION_PROP_ID,					"No expire notification"					},
		{CERT_AUTH_ROOT_SHA256_HASH_PROP_ID,					"Auth root SHA256 hash"						},
		{CERT_NCRYPT_KEY_HANDLE_TRANSFER_PROP_ID,				"Ncrypt key handle transfer"				},
		{CERT_HCRYPTPROV_TRANSFER_PROP_ID,						"Hcryptprovider transfer"					},
		{CERT_SMART_CARD_READER_PROP_ID,						"Smart card reader"							},
		{CERT_SEND_AS_TRUSTED_ISSUER_PROP_ID,					"Send as trusted issuer"					},
		{CERT_KEY_REPAIR_ATTEMPTED_PROP_ID,						"Key repair attempted"						},
		{CERT_DISALLOWED_FILETIME_PROP_ID,						"Disallowed filetime"						},
		{CERT_ROOT_PROGRAM_CHAIN_POLICIES_PROP_ID,				"Root program chain policies"				},
		{CERT_SMART_CARD_READER_NON_REMOVABLE_PROP_ID,			"Smart card reader non removable"			},
		{CERT_SHA256_HASH_PROP_ID,								"SHA256 hash"								},
		{CERT_SCEP_SERVER_CERTS_PROP_ID,						"SCEP server certs"							},
		{CERT_SCEP_RA_SIGNATURE_CERT_PROP_ID,					"SCEP ra signature cert"					},
		{CERT_SCEP_RA_ENCRYPTION_CERT_PROP_ID,					"SCEP ra encryption cert"					},
		{CERT_SCEP_CA_CERT_PROP_ID,								"SCEP CA cert"								},
		{CERT_SCEP_SIGNER_CERT_PROP_ID,							"SCEP signer cert"							},
		{CERT_SCEP_NONCE_PROP_ID,								"SCEP nonce"								},
		{CERT_SCEP_ENCRYPT_HASH_CNG_ALG_PROP_ID,				"SCEP encrypt hash cng alg"					},
		{CERT_SCEP_FLAGS_PROP_ID,								"SCEP flags"								},
		{CERT_SCEP_GUID_PROP_ID,								"SCEP guid"									},
		{CERT_SERIALIZABLE_KEY_CONTEXT_PROP_ID,					"Serializable key context"					},
		{CERT_ISOLATED_KEY_PROP_ID,								"Isolated key"								},
		{CERT_SERIAL_CHAIN_PROP_ID,								"Serial chain"								},
		{CERT_FIRST_RESERVED_PROP_ID,							"First reserved"							},
	};

	char		name_string[256];
	CertGetNameString(
		ctx,
		CERT_NAME_SIMPLE_DISPLAY_TYPE,
		0,
		NULL,
		name_string,
		sizeof(name_string)
	);

	// Loop to find all of the property identifiers for the specified certificate
	for (DWORD id = 0; id = CertEnumCertificateContextProperties(ctx, id); ) {
		DWORD	length;
		if (!CertGetCertificateContextProperty(ctx, id, NULL, &length))
			return false;

		malloc_block	block(length);
		if (!CertGetCertificateContextProperty(ctx, id, block, &length))
			return false;

		const char *name = 0;
		for (auto &i : prop_ids) {
			if (i.id == id) {
				name = i.name;
				break;
			}
		}
		string			s;
		string_builder	b(s);

		if (name)
			b << name;
		else
			b << "Property 0x" << hex(id);

		b << ':';
		const uint8		*p	= block, *p0 = p;
		while (length--) {
			if (p == p0) {
				b << '\n';
				p0 = p + 16;
			}
			b << rightjustify(3,'0') << hex(*p++) << ' ';
		}
		b << '\n';
		b.flush();
		ISO_TRACE(s);
	}
	return true;
}

#if 0
#include "filename.h"
#include "base/algorithm.h"

#include <WinDNS.h>
#pragma comment(lib, "Dnsapi.lib")

bool HostNameMatches(const string &hostname, const string &DNSName) {
	if (DnsNameCompare_A(hostname, DNSName))
		return true;

	if (!DNSName.find('*'))
		return false;

	const char	*suffix_host	= hostname.find('.');
	const char	*suffix_dns		= DNSName.find('.');
	if (str(suffix_host) != str(suffix_dns))
		return false;
	
	return matches(hostname, DNSName);
}

// See http://etutorials.org/Programming/secure+programming/Chapter+10.+Public+Key+Infrastructure/10.8+Adding+Hostname+Checking+to+Certificate+Verification/
bool MatchCertHostName(PCCERT_CONTEXT cert_ctx, const string &hostname) {
	// Try SUBJECT_ALT_NAME2 first - it supercedes SUBJECT_ALT_NAME
	auto szOID		= szOID_SUBJECT_ALT_NAME2;
	auto pExtension = CertFindExtension(szOID, cert_ctx->pCertInfo->cExtension, cert_ctx->pCertInfo->rgExtension);

	if (!pExtension) {
		szOID		= szOID_SUBJECT_ALT_NAME;
		pExtension	= CertFindExtension(szOID, cert_ctx->pCertInfo->cExtension, cert_ctx->pCertInfo->rgExtension);
	}

	// Extract the SAN information (list of names) 
	DWORD length = -1;
	if (pExtension && CryptDecodeObject(X509_ASN_ENCODING, szOID, pExtension->Value.pbData, pExtension->Value.cbData, 0, 0, &length)) {
		malloc_block	block(length);
		CryptDecodeObject(X509_ASN_ENCODING, szOID, pExtension->Value.pbData, pExtension->Value.cbData, 0, block, &length);

		CERT_ALT_NAME_INFO *info = block;
		auto it = find_if(info->rgAltEntry, info->rgAltEntry + info->cAltEntry, [hostname](const _CERT_ALT_NAME_ENTRY &i) {
			return i.dwAltNameChoice == CERT_ALT_NAME_DNS_NAME && HostNameMatches(hostname, i.pwszDNSName);
		});
		return it != info->rgAltEntry + info->cAltEntry; // left pointing past the end if not found
	}

	// No SubjectAltName extension -- check CommonName
	length = CertGetNameString(cert_ctx, CERT_NAME_ATTR_TYPE, 0, szOID_COMMON_NAME, 0, 0);
	if (!length) // No CN found
		return false;

	string common_name(length);
	CertGetNameString(cert_ctx, CERT_NAME_ATTR_TYPE, 0, szOID_COMMON_NAME, common_name, length);
	return HostNameMatches(hostname, common_name);
}
#endif

#if 0

#include <Cryptuiapi.h>
#pragma comment(lib, "Cryptui.lib")

// Display a UI with the certificate info and also write it to the debug output
bool Certificate::ShowInfo(HWND hWnd, const string16 &title) {
	return CryptUIDlgViewContext(CERT_STORE_CERTIFICATE_CONTEXT, ctx, hWnd, title, 0, NULL);
}

#endif

