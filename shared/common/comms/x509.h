#include "asn1.h"
#include "extra/date.h"
#include "maths/bignum.h"

namespace iso {

namespace X509 {
using namespace ASN1;

typedef optional<ExplicitTag<int, 0> >	Version;
typedef	Value							UniqueIdentifier;

struct AlgorithmIdentifier {
	ResolvedObjectID	Algorithm;
	RawValue			Parameters;
	bool	operator==(const char *s) const { return Algorithm == s; }
};


struct Attribute {
	ResolvedObjectID	Type;
	OctetString			Value;
	bool	operator==(const char *s) const { return Type == s; }
};

typedef SetOf<Attribute>		Name;
typedef	RawValue				DistinguishedName;
//typedef	dynamic_array<Name>	DistinguishedName;

inline bool operator==(const Name &name, const char *type) {
	return name[0] == type;
}

struct GeneralName {
	enum {
		OTHERNAME,
		EMAIL,
		DNS,
		X400,
		DIRECTORY,
		EDIPARTY,
		URI,
		IPADD,
		RID,
	};
	struct Other {
		ResolvedObjectID	type_id;
		Value				value;
	};
	struct EdiParty {
		String		assigner;
		String		party;
	};

	int type;
	union Choice {
		ImplicitTag<Other,			OTHERNAME>	other;
		ImplicitTag<string,			EMAIL>		email;
		ImplicitTag<string,			DNS>		dns;
		ImplicitTag<Value,			X400>		x400;
		ExplicitTag<Name,			DIRECTORY>	directory;
		ImplicitTag<EdiParty,		EDIPARTY>	edi_party;
		ImplicitTag<string,			URI>		uri;
		ImplicitTag<OctetString,	IPADD>		ip;
		ImplicitTag<ObjectID,		RID>		rid;
		Choice()	{ clear(*this); }
		~Choice()	{}
	} choice;
};

struct SubjectPublicKeyInfo {
	AlgorithmIdentifier	Algorithm;
	Value				SubjectPublicKey;
};

struct Extension {
	ResolvedObjectID	ExtnID;
	optional<bool>		Critical;
	OctetString			ExtnValue;
};
typedef dynamic_array<Extension>	Extensions;

struct Key {
	ResolvedObjectID	Type;
	Value				Value;
	SetOf<Attribute>	Attributes;
};

struct Validity {
	DateTime			NotBefore;
	DateTime			NotAfter;
};

struct TbsCertificate {
	optional<ExplicitTag<int, 0> >		Version;
	mpi									SerialNumber;
	AlgorithmIdentifier					signature;
	DistinguishedName					Issuer;
	Validity							Validity;
	DistinguishedName					Subject;
	SubjectPublicKeyInfo				SubjectPublicKeyInfo;
	optional<ImplicitTag<UniqueIdentifier,1> >	IssuerUniqueIdentifier;
	optional<ImplicitTag<UniqueIdentifier,2> >	SubjectUniqueIdentifier;
	optional<ExplicitTag<Extensions, 3> >		Extensions;
};

struct Signature {
	AlgorithmIdentifier			Algorithm;
	OctetString					signature;
};

struct Certificate {
	TbsCertificate				TbsCertificate;
	AlgorithmIdentifier			SignatureAlgorithm;
	dynamic_bitarray<>			signature;
};

struct Attribute2 {
	ResolvedObjectID	Type;
	SetOf<Value>		Set;
};
struct CertificationRequestInfo {
    optional<int>				Version;
	DistinguishedName			Subject;
	SubjectPublicKeyInfo		SubjectPublicKeyInfo;
	ImplicitTag<SetOf<Attribute2>,0>	attributes;
};

struct CertificationRequest {
    CertificationRequestInfo	req_info;
	AlgorithmIdentifier			SignatureAlgorithm;
	dynamic_bitarray<>			signature;
};

struct RSAPublicKey {
	mpi			N, E;
};

struct RSAPrivateKey {
	int			version;
	mpi			N, E, D, P, Q;
	mpi 		DP, DQ, QP;	//	D % (P - 1), D % (Q - 1), (Q^-1 mod P)
	optional<Value>	other;
};

struct DHPublicKey {
	mpi	y;	// y = g^x mod p
};

struct DHParameters {
	mpi				P;	//odd prime, p=jq +1
	mpi				G;	//generator, g
	optional<int>	length;
};

struct DHParametersExt {
	struct Validation {
		dynamic_bitarray<>	seed;
		mpi					counter;
	};
	mpi				P;	//odd prime, p=jq +1
	mpi				G;	//generator, g
	mpi				Q;	//factor of p-1
	optional<mpi>	J;	//subgroup factor
	optional<Validation>	validation;
};

struct DSAPublicKey {
	mpi		Y;
};

struct DSAPrivateKey {
	int		version;
	mpi		P;
	mpi		Q;
	mpi		G;
	mpi		pub_key;
	mpi		priv_key;
};

struct DSAParameters {
	mpi		P;
	mpi		Q;
	mpi		G;
};

struct DSASignature {
	mpi		R;
	mpi		S;
};

struct ECParameters {
	ResolvedObjectID	namedcurve;
};

struct ECPrivateKey {
	int		version;
	OctetString										privateKey;
	optional<ExplicitTag<ECParameters, 0> >			parameters;
	optional<ExplicitTag<dynamic_bitarray<>, 1> >	publicKey;
};

} // namespace X509

namespace ASN1 {

ASN1_STRUCT(X509::AlgorithmIdentifier, 2)
	, ASN1_FIELD(Algorithm)
	, ASN1_FIELD(Parameters)
ASN1_ENDSTRUCT

ASN1_STRUCT(X509::Validity, 2)
	, ASN1_FIELD(NotBefore)
	, ASN1_FIELD(NotAfter)
ASN1_ENDSTRUCT

ASN1_STRUCT(X509::Attribute, 2)
	, ASN1_FIELD(Type)
	, ASN1_FIELD(Value)
ASN1_ENDSTRUCT

ASN1_STRUCT(X509::Attribute2, 2)
	, ASN1_FIELD(Type)
	, ASN1_FIELD(Set)
ASN1_ENDSTRUCT

ASN1_STRUCT(X509::SubjectPublicKeyInfo, 2)
	, ASN1_FIELD(Algorithm)
	, ASN1_FIELD(SubjectPublicKey)
ASN1_ENDSTRUCT

ASN1_STRUCT(X509::Extension, 3)
	, ASN1_FIELD(ExtnID)
	, ASN1_FIELD(Critical)
	, ASN1_FIELD(ExtnValue)
ASN1_ENDSTRUCT

ASN1_STRUCT(X509::TbsCertificate, 10)
	, ASN1_FIELD(Version)
	, ASN1_FIELD(SerialNumber)
	, ASN1_FIELD(signature)
	, ASN1_FIELD(Issuer)
	, ASN1_FIELD(Validity)
	, ASN1_FIELD(Subject)
	, ASN1_FIELD(SubjectPublicKeyInfo)
	, ASN1_FIELD(IssuerUniqueIdentifier)
	, ASN1_FIELD(SubjectUniqueIdentifier)
	, ASN1_FIELD(Extensions)
ASN1_ENDSTRUCT

ASN1_STRUCT(X509::Signature, 2)
	, ASN1_FIELD(Algorithm)
	, ASN1_FIELD(signature)
ASN1_ENDSTRUCT

ASN1_STRUCT(X509::Certificate, 3)
	, ASN1_FIELD(TbsCertificate)
	, ASN1_FIELD(SignatureAlgorithm)
	, ASN1_FIELD(signature)
ASN1_ENDSTRUCT


ASN1_STRUCT(X509::CertificationRequestInfo, 4)
    , ASN1_FIELD(Version)
	, ASN1_FIELD(Subject)
	, ASN1_FIELD(SubjectPublicKeyInfo)
	, ASN1_FIELD(attributes)
ASN1_ENDSTRUCT

ASN1_STRUCT(X509::CertificationRequest, 3)
    , ASN1_FIELD(req_info)
	, ASN1_FIELD(SignatureAlgorithm)
	, ASN1_FIELD(signature)
ASN1_ENDSTRUCT

ASN1_STRUCT(X509::GeneralName::Other, 2)
	, ASN1_FIELD(type_id)
	, ASN1_FIELD(value)
ASN1_ENDSTRUCT

ASN1_STRUCT(X509::GeneralName::EdiParty, 2)
	, ASN1_FIELD(assigner)
	, ASN1_FIELD(party)
ASN1_ENDSTRUCT

ASN1_CHOICE(X509::GeneralName::Choice, 9)
	, ASN1_FIELD(other)
	, ASN1_FIELD(email)
	, ASN1_FIELD(dns)
	, ASN1_FIELD(x400)
	, ASN1_FIELD(directory)
	, ASN1_FIELD(edi_party)
	, ASN1_FIELD(uri)
	, ASN1_FIELD(ip)
	, ASN1_FIELD(rid)
ASN1_ENDSTRUCT

ASN1_STRUCT(X509::GeneralName, 1)
	, ASN1_FIELD(choice)
ASN1_ENDSTRUCT

ASN1_STRUCT(X509::RSAPublicKey, 2)
	, ASN1_FIELD(N)
	, ASN1_FIELD(E)
ASN1_ENDSTRUCT

ASN1_STRUCT(X509::RSAPrivateKey, 10)
	, ASN1_FIELD(version)
	, ASN1_FIELD(N)
	, ASN1_FIELD(E)
	, ASN1_FIELD(D)
	, ASN1_FIELD(P)
	, ASN1_FIELD(Q)
	, ASN1_FIELD(DP)
	, ASN1_FIELD(DQ)
	, ASN1_FIELD(QP)
	, ASN1_FIELD(other)
ASN1_ENDSTRUCT

ASN1_TYPEDEF(X509::DHPublicKey, mpi)

ASN1_STRUCT(X509::DHParameters, 3)
	, ASN1_FIELD(P)
	, ASN1_FIELD(G)
	, ASN1_FIELD(length)
ASN1_ENDSTRUCT

ASN1_STRUCT(X509::DHParametersExt::Validation, 2)
	, ASN1_FIELD(seed)
	, ASN1_FIELD(counter)
ASN1_ENDSTRUCT

ASN1_STRUCT(X509::DHParametersExt, 5)
	, ASN1_FIELD(P)
	, ASN1_FIELD(G)
	, ASN1_FIELD(Q)
	, ASN1_FIELD(J)
	, ASN1_FIELD(validation)
ASN1_ENDSTRUCT


ASN1_TYPEDEF(X509::DSAPublicKey, mpi)

ASN1_STRUCT(X509::DSAPrivateKey, 6)
	, ASN1_FIELD(version)
	, ASN1_FIELD(P)
	, ASN1_FIELD(Q)
	, ASN1_FIELD(G)
	, ASN1_FIELD(pub_key)
	, ASN1_FIELD(priv_key)
ASN1_ENDSTRUCT


ASN1_STRUCT(X509::DSAParameters, 3)
	, ASN1_FIELD(P)
	, ASN1_FIELD(Q)
	, ASN1_FIELD(G)
ASN1_ENDSTRUCT


ASN1_STRUCT(X509::DSASignature, 2)
	, ASN1_FIELD(R)
	, ASN1_FIELD(S)
ASN1_ENDSTRUCT

ASN1_CHOICE(X509::ECParameters, 1)
	, ASN1_FIELD(namedcurve)
ASN1_ENDSTRUCT

ASN1_STRUCT(X509::ECPrivateKey, 4)
	, ASN1_FIELD(version)
	, ASN1_FIELD(privateKey)
	, ASN1_FIELD(parameters)
	, ASN1_FIELD(publicKey)
ASN1_ENDSTRUCT

} // namespace ASN1

} // namespace iso
