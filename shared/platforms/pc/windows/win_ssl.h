#ifndef SSL_WIN_H
#define SSL_WIN_H

#include "base/strings.h"
#include "base/array.h"
#include "sockets.h"

#define SECURITY_WIN32
#include <sspi.h>

#include <wincrypt.h>
#include <security.h>
#include <schnlsp.h>

namespace iso {

struct Certificate {
	const CERT_CONTEXT	*ctx;
	Certificate() : ctx(0) {}
	Certificate(CtxtHandle &hCtx, bool remote) : ctx(0) {
		QueryContextAttributes(&hCtx, remote ? SECPKG_ATTR_REMOTE_CERT_CONTEXT : SECPKG_ATTR_LOCAL_CERT_CONTEXT, (void*)&ctx);
	}
	~Certificate() {
		if (ctx)
			CertFreeCertificateContext(ctx);
	}
	operator const CERT_CONTEXT*() { return ctx; }

	string GetInfoString() {
		char	temp[1000];
		CertGetNameString(ctx, CERT_NAME_FRIENDLY_DISPLAY_TYPE, 0, NULL, temp, 1000);
		return temp;
	}
	SECURITY_STATUS		Verify();
	bool				ShowInfo(HWND hWnd = 0, const string16 &title = "Certificate");
	bool				DumpInfo();
	HRESULT				Trusted();

	static const CERT_CONTEXT*	Create();

	static const CERT_CONTEXT*	Create(const char *oid, int type) {
		HCERTSTORE	hCS = CertOpenSystemStore(0, "MY");
		if (!hCS)
			return 0;

		CERT_RDN		cert_rdn;
		CERT_RDN_ATTR	cert_rdn_attr;
		cert_rdn.cRDNAttr	= 1;
		cert_rdn.rgRDNAttr	= &cert_rdn_attr;

		cert_rdn_attr.pszObjId		= szOID_COMMON_NAME;
		cert_rdn_attr.dwValueType	= CERT_RDN_ANY_TYPE;
		cert_rdn_attr.Value.cbData	= uint32(strlen(oid));
		cert_rdn_attr.Value.pbData	= (BYTE*)oid;
		return CertFindCertificateInStore(hCS, X509_ASN_ENCODING | PKCS_7_ASN_ENCODING ,0, type, &cert_rdn, NULL);
	}
	static const CERT_CONTEXT *Create(bool machine_cert, const string16 &subject = L"localuser", const string16 &friendly_name = L"SSLStream", const string16 &description = L"SSLStream Test");

};

class SSL_SOCKET {
	SOCKET			sock;
	SCHANNEL_CRED	schannel_cred;
	dynamic_array<ALG_ID>	algs;
	CredHandle		hCred;
	CtxtHandle		hCtx;
	string			destination_name;
	const CERT_CONTEXT	*our_certificate;
	bool			server_side;
	bool			init_context;
	bool			external_certificate;
	malloc_block	extra_data;
	size_t			extra_data_size;
	malloc_block	pending_data;
	size_t			pending_data_size;

public:
	SSL_SOCKET(SOCKET _sock, bool _server_side)
		: sock(_sock), our_certificate(0), server_side(_server_side), init_context(false), external_certificate(false)
		, extra_data_size(0), pending_data_size(0)
	{
		clear(hCred);
		clear(hCtx);
		clear(schannel_cred);
		schannel_cred.dwVersion	= SCHANNEL_CRED_VERSION;
		schannel_cred.dwFlags	= SCH_CRED_NO_DEFAULT_CREDS | SCH_CRED_NO_DEFAULT_CREDS | SCH_CRED_NO_SYSTEM_MAPPER | SCH_CRED_REVOCATION_CHECK_CHAIN;
	}
	~SSL_SOCKET() {
		Disconnect();

		if (hCtx.dwLower)
			DeleteSecurityContext(&hCtx);
		if (hCred.dwLower)
			FreeCredentialHandle(&hCred);
		if (our_certificate && !external_certificate)
			CertFreeCertificateContext(our_certificate);
		if (schannel_cred.hRootStore)
			CertCloseStore(schannel_cred.hRootStore, 0);
	}
	void	AddAlgorithm(ALG_ID alg) {
		algs.push_back(alg);
	}
	void	SetCertificate(const CERT_CONTEXT *_cert) {
		our_certificate			= _cert;
		external_certificate	= true;
	}
	void	SetCertificateStore(HCERTSTORE hCS) {
		schannel_cred.hRootStore	= hCS;
	}
	void	SetMinCipherStrength(int strength) {
		schannel_cred.dwMinimumCipherStrength = strength;
	}
	void	SetDestinationName(const char *n) {
		destination_name = n;
	}
	bool	Init();
	int		Wait();
	int		Disconnect();

	int		s_ssend(char* b, int sz);
	int		s_recv(char *b, int sz);

	Certificate	GetSessionCertificate() { return Certificate(hCtx, true); }

	SECURITY_STATUS	VerifySessionCertificate() { return GetSessionCertificate().Verify(); }

	bool GetInfo(SecPkgContext_NegotiationInfo &info) {
		return SUCCEEDED(QueryContextAttributes(&hCtx, SECPKG_ATTR_NEGOTIATION_INFO, &info));
	}

};

} //namespace iso

#endif // SSL_WIN_H
