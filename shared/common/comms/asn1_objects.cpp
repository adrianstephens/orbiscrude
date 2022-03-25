#include "base/algorithm.h"
#include "asn1.h"

using namespace iso;
using namespace ASN1;

OID	*OID::get_child(int i) {
	auto	c = find(children, i);
	return c == children.end() ? nullptr : c.get();
}

OID	*OID::get_child_make(int i) {
	auto	c = find(children, i);
	if (c == children.end()) {
		auto	p = new OID(i);
		attach(p);
		return p;
	}
	return c.get();
}

void OID::attach_check(OID *t) {
	if (OID *o2 = get_child(t->id)) {
		ISO_ASSERT(!o2->name || !t->name);
		o2->children.append(move(t->children));
		if (t->name)
			o2->name = t->name;
		return;
	}
	attach(t);
}

const OID *ASN1::find_objectid(const const_memory_block &b) {
	const uint8	*p	= b, *e = (const uint8*)b.end();
	uint8	id01	= *p++;
	uint32	id0		= id01 / 40, id1 = id01 - id0 * 40;

	OID	*o = OID_root()->get_child(id0);
	if (o) {
		o = o->get_child(id1);
		while (o && p < e) {
			int	i = 0;
			for (uint8 x = 0x80; x & 0x80; ) {
				x = *p++;
				i = (i << 7) | (x & 127);
			};
			o = o->get_child(i);
		}
	}
	return o;
}

void ASN1::Value::set_objectid(const OID *o) {
	uint8		temp[256], *p = temp + 256;

	while (o->parent) {
		uint32	id	= o->id;
		*--p	= id & 0x7f;
		while (id > 127) {
			id >>= 7;
			*--p = (id & 0x7f) | 0x80;
		}
		o = o->parent;
	}
	p[1] += p[0] * 40;
	++p;
	set(TYPE_OBJECT_ID, temp + 256 - p).copy_from(p);
}

const OID *ASN1::Value::get_objectid() const {
	return find_objectid(get_buffer());
}

//-----------------------------------------------------------------------------
// hierarchy creation
//-----------------------------------------------------------------------------

template<int... II> struct OID_ID : OID {};

template<int I> void attach_oid(OID *o, OID_ID<I> *t) {
	t->id = I;
	o->attach_check(t);
}

template<int I1, int I2, int... II> void attach_oid(OID *o, OID_ID<I1,I2,II...> *t) {
	OID	*o2	= o->get_child_make(I1);
	attach_oid(o2, (OID_ID<I2,II...>*)t);
}

void attach_oids(OID *o) {}

template<typename T> void attach_oids(OID *o, T *t)	{
	attach_oid(o, t);
}

template<typename T, typename... TT> void attach_oids(OID *o, T t, TT... tt) {
	attach_oid(o, t);
	attach_oids(o, tt...);
}

template<int... II, typename... TT> OID_ID<II...> *oid(const char *name, TT... tt) {
	OID	*o = new OID(name);
	attach_oids(o, tt...);
	return (OID_ID<II...>*)o;
}

template<int... II, typename... TT> OID_ID<II...> *oid(TT... tt) {
	OID	*o = new OID;
	attach_oids(o, tt...);
	return (OID_ID<II...>*)o;
}

OID	*ASN1::OID_root() {
	static OID *o = oid<0>("root"
		, oid<0>("itu-t"
		//0.0	- ITU-T Recommendation
		//0.1	- ITU-T Question
		//0.2	- ITU-T Administration
		//0.3	- ITU-T Network Operator
		//0.4	- ITU-T Identified Organization
		//0.9	- "Data" - used in some RFCs

			, oid<3>(	//Network Operator
				//DNIC	Network Name		Carrier							Country					Within Remove	Outside Add
				//2000-2999	Europe

				//2022		HELPAK			Hellenic Telecom Org.			GR	Greece
				//2041		DATANET											NL	Netherlands				204			0
				//2042		EURONET											NL	Netherlands
				//2044		DABAS			PTT								NL	Netherlands
				//2062		DCS				RTT								BE	Belgium					206			0
				//2063		EURONET											BE	Belgium
				//2064		DCS				RTT								BE	Belgium					206			0
				//2080		TRANSPAC		PTT								FR	France					2080
				//2081		NTI				PTT								FR	France
				//2083		EURONET			PTT								FR	France
				//2160		PCTO			PTT								HU	Hungary
				//2141		IBERPAC			CTNE							ES	Spain
				//2145		IBERBAC			CTNE							ES	Spain					214
				//2220						Italcable						IT	Italy
				//2222		DARDO			Italcable						IT	Italy
				//2222		ITAPAC											IT	Italy					222
				//2223		EURONET											IT	Italy
				//2284		TELEPAC			Suisse PTT						CH	Switzerland				228			0
				//2289		DATALINK		Radio Suisse					CH	Switzerland
				//2320						Radio Austria					AT	Austria
				//2322		DATEX-P											AT	Austria					232
				//2329		RADAUS			Radio Austria					AT	Austria
				//2341		IPSS			BTI								GB	United Kingdom
				//2342		PSS				BT								GB	United Kingdom			234
				//2343		EURONET											GB	United Kingdom
				//2350		MERCURY			Mercury Comm. Ltd.				GB	United Kingdom
				//2381		Datex											DK	Denmark
				//2382		DATAPAK			PTT								DK	Denmark					238
				//2401		TELEPAK			government						SE	Sweden
				//2402		DATAPAK			Swedish Telecom					SE	Sweden					2402
				//2403														SE	Sweden
				//2405		TELEPAK			government						SE	Sweden
				//2421		Datex			PTT								NO	Norway
				//2422		DATAPAK			PTT								NO	Norway					242
				//2441		Datex											FI	Finland
				//2442		DATAPAK			PTT								FI	Finland					244			0
				//2443		Digipak											FI	Finland					None		None
				//2623		EURONET											DE	Germany
				//2624		DATEX-P			DBP								DE	Germany					262			0
				//2680		TELEPAK											PT	Portugal				268
				//2682		SAABD			CPMR Telcom Int'l				PT	Portugal
				//2703		EURONET											LU	Luxembourg
				//2704		LUXPAC			PTT								LU	Luxembourg				270			0
				//2721		IPSS											IE	Ireland
				//2723		EURONET											IE	Ireland
				//2724		EIRPAC			Telcom Eireann					IE	Ireland					272
				//2740		ICEP			PTT								IS	Iceland

				//3000-3999	North America and Caribbean islands
				//3020		Datapac			TCTS							CA	Canada					3020		1
				//3024		GLOBDAT			Teleglobe Ca.					CA	Canada
				//3025		GLOBDAT			Teleglobe Ca.					CA	Canada
				//3028		INFOSWITCH										CA	Canada					308
				//3029		INFOSWITCH		CNCP							CA	Canada
				//3101		WUT				Western Union Digital Data		US	United States			3101		0
				//3102		WUI				WUI Digital Datel				US	United States
				//3103		UDTS			ITT								US	United States			310
				//3104		WUI				WUI Database Access				US	United States
				//3104		IDAR											TH	Thailand
				//3104		Impac											US	United States			3104
				//3105		WUI				WUI Leased Channel				US	United States
				//3106		TYMNET			Tymenet							US	United States			3106
				//3107		UDTS I			ITT Datel						US	United States			310
				//3108		ITT				ITT Short Term Voice/Data		US	United States
				//3108		ITT				ITT Short Term Voice/Data		US	United States
				//3109		DATEL I			RCA								US	United States
				//3110		Telenet			Telenet Communications			US	United States			None		None
				//3111		DATEL II		RCA								US	United States
				//3112		WUT				Western Union Broadband			US	United States
				//3113		LSDS			RCA								US	United States
				//3114		INFOMASTER		Western Union					US	United States
				//3115		GRAPHNET		Graphnet Interactive			US	United States
				//3116		GRAPHNET		Graphnet Store & Forward		US	United States
				//3117		WUI Telex		WUI								US	United States
				//3118		GRAPHNET		Graphnet Facsimile				US	United States
				//3119		TRT				TRT Packet Switching			US	United States
				//3120		ITT				ITT Low Speed					US	United States
				//3121		FTCC			FTCC Datel						US	United States
				//3122		FTCC			FTCC Telex						US	United States
				//3123		FTCC			FTCC Leased Channel				US	United States
				//3124		FTCC			FTCC Packet Switching			US	United States
				//3125		UNINET			ITT Uninet						US	United States
				//3126		Autonet			ADP Network Services			US	United States			312
				//3127		GTE Telenet		GTE Telenet						US	United States
				//3128		TRT				TRT Tekex						US	United States
				//3129		TRT				TRT Leased Channel				US	United States
				//3130		TRT				TRT Digital Data				US	United States
				//3131		RCA				RCAG Telex						US	United States
				//3132		CompuServe		CompuServe						US	United States			313
				//3134		ACCUNET			AT&T							US	United States			313
				//3135		ALASKANET		Alascom							US	United States
				//3136		Marknet											US	United States			3136
				//3137		Infonet			Computer Sciences Corporation	US	United States			3137
				//3140		ConnNet			Southern New England Telephone	US	United States			314
				//3141		Bellatlantic									US	United States			3141
				//3142		Pulselink										US	United States			314
				//3144		Infopath		NYNEX							US	United States			3144		1
				//3300		UDTS			ITT								PR	Puerto Rico	U.S.A.
				//3300		RCA/PR			RCA								PR	Puerto Rico	U.S.A.
				//3301		PRTC			Puerto Rico TelCo				PR	Puerto Rico	U.S.A.
				//3320		UDTS			ITT								VI	Virgin Is.	U.S.A.
				//3340		TELEPAC			PTT								MX	Mexico					334
				//3380		Jamantel		Jamaica Int'l Telecom Ltd.		JM	Jamaica
				//3400		TRANSPAC		French PTT						GP	Fr. Antilles
				//3400		DOMPAC			Ag. Comm. des Telecom			GP	Fr. Guadeloupe
				//3400		DOMPAC			Ag. Comm. des Telecom			MQ	Fr. Martinique
				//3423		IDAS			Barbados Ext'l Telecom Ltd.		BB	Barbados
				//3443		AGANET			C&W (West Indies)				AN	Antigua
				//3463		C&W CAYMAN		C&W CAYMEN						KY	Cayman Islands
				//3503		C&W				C&W (West Indies)				BM	Bermuda
				//3640		BATELCO			Bahamas Telecom Corp.			BS	Bahamas
				//3700		UDTS			ITT								DO	Dominican Republic
				//3740		TEXTEL			T&T External					TT	Trinidad & Tobago

				//4000-4399	Middle East
				//4180		IDAS			BTC								IQ	Iraq
				//4200		IDAS			BTC								SA	Saudi Arabia
				//4270		IDAS			BTC								QA	Qatar
				//4251		ISRANET			PTT								IL	Israel
				//4263		ODAS (BAHNET)	Bahrain Telecom Co. (BTC)		BH	Bahrain
				//4310		TEDAS			Emirates Telcom Corp.			AE	United Arab Emirates

				//4400-4999 Asia
				//4401		DDX-P			NTT								JP	Japan					4401		1
				//4406		NIS/TYMNET		Network Information Service		JP	Japan					4406
				//4408		VENUS-P			KDD								JP	Japan					440
				//4501		DACOMNET		Data Comm. Corp					KR	Korea S. (ROK)			450
				//4503		DACOM											KR	Korea S. (ROK)
				//4542		IDAS/ITS		C&W								HK	Hong Kong
				//4542		INTELPAK		C&W								HK	Hong Kong				454
				//4544		DAS				C&W								HK	Hong Kong
				//4545		DATAPAK											HK	Hong Kong				454
				//4600		PKTELCOM		Beijing Telecom Adm.			CH	China
				//4872		Pacnet											TW	Taiwan					487
				//4877		UDAS			ITA								TW	Taiwan

				//5000-5999	Philippines@ South East Asia@ and Pacific islands
				//5021		MAYPAC											MA	Malaysia				502
				//5052		AUSTPAC			Telecom Australia				AU	Australia				505
				//5053		MIDAS			OTC								AU	Australia
				//5101		SKDP			PTT								ID	Indonesia				510			0
				//5150		ETPI			Eastern Telecom					PH	Philippines
				//5150		UDTS			Globe Macay C&R					PH	Philippines
				//5150		PCG				Phil. Global Comm. Inc.			PH	Philippines
				//5200		CAT				CAT								TH	Thailand
				//5252		TELEPAC			Telecoms Singapore				SG	Singapore				525			0
				//5301		IPSS			PTT								NZ	New Zealand				530
				//5301		PACNET											NZ	New Zealand				530
				//5350		RCA				RCA								GU	Guam U.S.A.

				//6000-6999 Africa
				//6020		ARENTO			Telecom Org. of Egypt			EG	Egypt
				//6122		SYTRANPAK		Intelci							CI	Ivory Coast
				//6282		GABONPAC		Telecom Int'l					GA	Gabon
				//6470		DOMPAC			Ag. Comm. des Telecom			RE	Fr. Reunion
				//6550		SAPONET			SAPO							ZA	South Africa

				//7000-7499	South America
				//7080		HONDUTEK		Empresa Hondurena de Telecom	HN	Honduras
				//7120		RACSAPAC		Radiografica Costarricense		CR	Costa Rica
				//7141		INTEL			Instituto Nac. de Telecom		PA	Panama
				//7160		ENTEL			Empresa Nac. de Telecom			PE	Peru
				//7220		ENTEL			Empresa Nac. de Telecom			AR	Argentina
				//7222		ARPC											AR	Argentina				722
				//7240		INTERDATA		EMBRATEL						BR	Brazil
				//7241		RENPAC											BR	Brazil					724
				//7320		DATAPAK			Empresa Nac. de Telecom			CO	Colombia
				//7420														GF	Fr. Guyana

				oid<4401,5>(	//ntt-ds
					oid<3,1,9>(	//camellia
						// Definitions for Camellia cipher - ECB, CFB, OFB MODE
						  oid<1>("camellia-128-ecb")
						, oid<3>("camellia-128-ofb")
						, oid<4>("camellia-128-cfb")
						, oid<6>("camellia-128-gcm")
						, oid<7>("camellia-128-ccm")
						, oid<9>("camellia-128-ctr")
						, oid<10>("camellia-128-cmac")
						, oid<21>("camellia-192-ecb")
						, oid<23>("camellia-192-ofb")
						, oid<24>("camellia-192-cfb")
						, oid<26>("camellia-192-gcm")
						, oid<27>("camellia-192-ccm")
						, oid<29>("camellia-192-ctr")
						, oid<30>("camellia-192-cmac")
						, oid<41>("camellia-256-ecb")
						, oid<43>("camellia-256-ofb")
						, oid<44>("camellia-256-cfb")
						, oid<46>("camellia-256-gcm")
						, oid<47>("camellia-256-ccm")
						, oid<49>("camellia-256-ctr")
						, oid<50>("camellia-256-cmac")
					)
				)
			)
		// from ITU-T. Most of this is defined in RFC 1274.  A couple of them are also mentioned in RFC 2247
			, oid<9>("data"
				, oid<2342>("pss"
					, oid<19200300>("ucl"
						, oid<100>("pilot"
							, oid<1>("pilotAttributeType"
								, oid<1>("userId")
								, oid<2>("textEncodedORAddress")
								, oid<3>("rfc822Mailbox")
								, oid<4>("info")
								, oid<5>("favouriteDrink")
								, oid<6>("roomNumber")
								, oid<7>("photo")
								, oid<8>("userClass")
								, oid<9>("host")
								, oid<10>("manager")
								, oid<11>("documentIdentifier")
								, oid<12>("documentTitle")
								, oid<13>("documentVersion")
								, oid<14>("documentAuthor")
								, oid<15>("documentLocation")
								, oid<20>("homeTelephoneNumber")
								, oid<21>("secretary")
								, oid<22>("otherMailbox")
								, oid<23>("lastModifiedTime")
								, oid<24>("lastModifiedBy")
								, oid<25>("domainComponent")
								, oid<26>("aRecord")
								, oid<27>("pilotAttributeType27")
								, oid<28>("mXRecord")
								, oid<29>("nSRecord")
								, oid<30>("sOARecord")
								, oid<31>("cNAMERecord")
								, oid<37>("associatedDomain")
								, oid<38>("associatedName")
								, oid<39>("homePostalAddress")
								, oid<40>("personalTitle")
								, oid<41>("mobileTelephoneNumber")
								, oid<42>("pagerTelephoneNumber")
								, oid<43>("friendlyCountryName")
								, oid<44>("uniqueIdentifier")
								, oid<45>("organizationalStatus")
								, oid<46>("janetMailbox")
								, oid<47>("mailPreferenceOption")
								, oid<48>("buildingName")
								, oid<49>("dSAQuality")
								, oid<50>("singleLevelQuality")
								, oid<51>("subtreeMinimumQuality")
								, oid<52>("subtreeMaximumQuality")
								, oid<53>("personalSignature")
								, oid<54>("dITRedirect")
								, oid<55>("audio")
								, oid<56>("documentPublisher")
							)
							, oid<3>("pilotAttributeSyntax"
								, oid<4>("iA5StringSyntax")
								, oid<5>("caseIgnoreIA5StringSyntax")
							)
							, oid<4>("pilotObjectClass"
								, oid<3>("pilotObject")
								, oid<4>("pilotPerson")
								, oid<5>("account")
								, oid<6>("document")
								, oid<7>("room")
								, oid<9>("documentSeries")
								, oid<13>("Domain")
								, oid<14>("rFC822localPart")
								, oid<15>("dNSDomain")
								, oid<17>("domainRelatedObject")
								, oid<18>("friendlyCountry")
								, oid<19>("simpleSecurityObject")
								, oid<20>("pilotOrganization")
								, oid<21>("pilotDSA")
								, oid<22>("qualityLabelledData")
							)
							, oid<10>("pilotGroups")
						)
					)
				)
			)
		)	//itu-t

		, oid<1>("iso"
			, oid<0,10118,3,0,55>("whirlpool")

			, oid<2>("ISO Member Body"
				// 36	- Australia
				// 40	- Austria (Öenorm Institut)
				//110	- Vietnam
				//156	- China
				//158	- Taiwan
				//203	- Czech Republic
				//246	- Finland
				//250	- France
				//276	- Germany (DIN)
				//344	- Hong Kong
				//372	- Ireland (NSAI)
				//392	- Japan
				//398	- Kazakhstan
				//410	- Korea
				//444	- India (Encryption)
				//528	- Nederlands Normalisatie Instituut
				//578	- Norway ISO member
				//616	- Poland (National Registration Authority, KRIO)
				//643	- Russian federation
				//702	- Singapore
				//752	- Sweden
				//777	- India
				//804	- Ukraine
				//826	- Great Britain (GB/UK)
				//840	- USA
				//860	- Uzbekistan
				//862	- Venezuela - via standards Venezuela
				//886	- Taiwan

				// Definitions for Camellia cipher - CBC MODE
				, oid<392,200011,61,1,1,1,2>("camellia-128-cbc")
				, oid<392,200011,61,1,1,1,3>("camellia-192-cbc")
				, oid<392,200011,61,1,1,1,4>("camellia-256-cbc")
				, oid<392,200011,61,1,1,3,2>("camellia128-wrap")
				, oid<392,200011,61,1,1,3,3>("camellia192-wrap")
				, oid<392,200011,61,1,1,3,4>("camellia256-wrap")

				// Definitions for SEED cipher - ECB, CBC, OFB mode
				, oid<410,200004>("kisa"
					, oid<1,3>("seed-ecb")
					, oid<1,4>("seed-cbc")
					, oid<1,5>("seed-cfb")
					, oid<1,6>("seed-ofb")
				)

				// GOST oids
				, oid<643,2,2>("cryptopro"
					, oid<3>("GOST R 34.11-94 with GOST R 34.10-2001")
					, oid<4>("GOST R 34.11-94 with GOST R 34.10-94"
						, oid<1>("GostR3410-94-a")
						, oid<2>("GostR3410-94-aBis")
						, oid<3>("GostR3410-94-b")
						, oid<4>("GostR3410-94-bBis")
					)
					, oid<9>("GOST R 34.11-94")
					, oid<10>("HMAC GOST 34.11-94")
					, oid<19>("GOST R 34.10-2001")
					, oid<20>("GOST R 34.10-94")
					, oid<21>("GOST 28147-89")
					, oid<22>("GOST 28147-89 MAC")
					, oid<23>("GOST R 34.11-94 PRF")
					, oid<98>("GOST R 34.10-2001 DH")
					, oid<99>("GOST R 34.10-94 DH")
					, oid<14,1>("Gost28147-89-CryptoPro-KeyMeshing")
					, oid<14,0>("Gost28147-89-None-KeyMeshing")
					// GOST parameter set oids
					, oid<30,0>("GostR3411-94-TestParamSet")
					, oid<30,1>("GostR3411-94-CryptoProParamSet")
					, oid<31,0>("Gost28147-89-TestParamSet")
					, oid<31,1>("Gost28147-89-CryptoPro-A-ParamSet")
					, oid<31,2>("Gost28147-89-CryptoPro-B-ParamSet")
					, oid<31,3>("Gost28147-89-CryptoPro-C-ParamSet")
					, oid<31,4>("Gost28147-89-CryptoPro-D-ParamSet")
					, oid<31,5>("Gost28147-89-CryptoPro-Oscar-1-1-ParamSet")
					, oid<31,6>("Gost28147-89-CryptoPro-Oscar-1-0-ParamSet")
					, oid<31,7>("Gost28147-89-CryptoPro-RIC-1-ParamSet")
					, oid<32,0>("GostR3410-94-TestParamSet")
					, oid<32,2>("GostR3410-94-CryptoPro-A-ParamSet")
					, oid<32,3>("GostR3410-94-CryptoPro-B-ParamSet")
					, oid<32,4>("GostR3410-94-CryptoPro-C-ParamSet")
					, oid<32,5>("GostR3410-94-CryptoPro-D-ParamSet")
					, oid<33,1>("GostR3410-94-CryptoPro-XchA-ParamSet")
					, oid<33,2>("GostR3410-94-CryptoPro-XchB-ParamSet")
					, oid<33,3>("GostR3410-94-CryptoPro-XchC-ParamSet")
					, oid<35,0>("GostR3410-2001-TestParamSet")
					, oid<35,1>("GostR3410-2001-CryptoPro-A-ParamSet")
					, oid<35,2>("GostR3410-2001-CryptoPro-B-ParamSet")
					, oid<35,3>("GostR3410-2001-CryptoPro-C-ParamSet")
					, oid<36,0>("GostR3410-2001-CryptoPro-XchA-ParamSet")
					, oid<36,1>("GostR3410-2001-CryptoPro-XchB-ParamSet")
				)
				, oid<643,2,9>("cryptocom"
					// Cryptocom LTD GOST oids
					, oid<1,6,1>("GOST 28147-89 Cryptocom ParamSet")
					, oid<1,5,3>("GOST 34.10-94 Cryptocom")
					, oid<1,5,4>("GOST 34.10-2001 Cryptocom")
					, oid<1,3,3>("GOST R 34.11-94 with GOST R 34.10-94 Cryptocom")
					, oid<1,3,4>("GOST R 34.11-94 with GOST R 34.10-2001 Cryptocom")
					, oid<1,8,1>("GOST R 3410-2001 Parameter Set Cryptocom")
				)
				, oid<643,7,1>("tc26"
					// TC26 GOST oids
					, oid<1>("tc26-algorithms"
						, oid<1>("tc26-sign"
							, oid<1>("gost2012_256: GOST R 34.10-2012 with 256 bit modulus")
							, oid<2>("gost2012_512: GOST R 34.10-2012 with 512 bit modulus")
						)
						, oid<2>("tc26-digest"
							, oid<2>("md_gost12_256: GOST R 34.11-2012 with 256 bit hash")
							, oid<3>("md_gost12_512: GOST R 34.11-2012 with 512 bit hash")
						)
						, oid<3>("tc26-signwithdigest"
							, oid<2>("GOST R 34.10-2012 with GOST R 34.11-2012 (256 bit)")
							, oid<3>("GOST R 34.10-2012 with GOST R 34.11-2012 (512 bit)")
						)
						, oid<4>("tc26-mac"
							, oid<1>("HMAC GOST 34.11-2012,256 bit")
							, oid<2>("HMAC GOST 34.11-2012,512 bit")
						)
						, oid<5>("tc26-cipher")
						, oid<6>("tc26-agreement"
							, oid<1>("tc26-agreement-gost-3410-2012-256")
							, oid<2>("tc26-agreement-gost-3410-2012-512")
						)
					)
					, oid<2>("tc26-constants"
						, oid<1>("tc26-sign-constants"
							, oid<2>("tc26-gost-3410-2012-512-constants"
								, oid<0>("tc26-gost-3410-2012-512-paramSetTest: GOST R 34.10-2012 (512 bit) testing parameter set")
								, oid<1>("tc26-gost-3410-2012-512-paramSetA: GOST R 34.10-2012 (512 bit) ParamSet A")
								, oid<2>("tc26-gost-3410-2012-512-paramSetB: GOST R 34.10-2012 (512 bit) ParamSet B")
							)
						)
						, oid<2>("tc26-digest-constants")
						, oid<5>("tc26-cipher-constants"
							, oid<1>("tc26-gost-28147-constants"
								, oid<1>("GOST 28147-89 TC26 parameter set")
							)
						)
					)
				)

				, oid<643,3,131,1,1>("INN")
				, oid<643,100,1>("OGRN")
				, oid<643,100,3>("SNILS")
				, oid<643,100,111>("Signing Tool of Subject")
				, oid<643,100,112>("Signing Tool of Issuer")

				, oid<840>("ISO US Member Body"
					//1		- US organizations?
					//10003	- ANSI-standard-Z39.50 (10003)
					//10004	- ISO/IEEE 11073 Health informatics - point-of-care medical device communication
					//10005	- ANSI T1
					//10006	- ieee802dot3
					//10008	- DICOM Standard
					//10017	- IEEE-1224
					//10022	- IEEE802Dot10
					//10036	- IEEE802Dot11
					//10040	- X9-57
					//10045	- ansi-x9-62
					//10065	- ASTM International, Subcommittee E31.20 Data and System Security for Health Information
					//10066	- ANSI C12.19 Application context
					//11359	- RSA
					//113533- Nortel Networks
					//113536- Sun Microsystem
					//113548- cisco
					//113549- RSADSI
					//113554- MIT
					//113556- Microsoft
					//113560- COLUMBIA UNIVERSITY
					//113564- Carestream (ex Kodak)
					//113572- Unisys Corp.
					//113612- ESnet
					//113618- The University of Texas System
					//113619- General Electric Company (GE)
					//113658- XAPIA
					//113669- Merge Technologies
					//113681- Hologic
					//113699- WordPerfect
					//113704- Philips
					//113741- Intel Corporation
					//114021- Identrus
					//114050- Cognos
					//114191- Sorna Corp.
					//114222- Centers for Disease Control
					//114283- globalPlatform

					, oid<10040>("X9.57"
						, oid<4>("X9.57 CM ?"
							, oid<1>("dsaEncryption")
							, oid<3>("dsaWithSHA1")
						)
					)
					, oid<10045>("ANSI X9.62"
						, oid<1>(
							  oid<1>("prime-field")
							, oid<2>("characteristic-two-field"
								, oid<3>("characteristic-two-basis"
									, oid<1>("onBasis")
									, oid<2>("tpBasis")
									, oid<3>("ppBasis")
								)
							)
						)
						, oid<2>(
							oid<1>("ecPublicKey")
						)
						, oid<3>(
								oid<0>(
								  oid<1>("c2pnb163v1")
								, oid<2>("c2pnb163v2")
								, oid<3>("c2pnb163v3")
								, oid<4>("c2pnb176v1")
								, oid<5>("c2tnb191v1")
								, oid<6>("c2tnb191v2")
								, oid<7>("c2tnb191v3")
								, oid<8>("c2onb191v4")
								, oid<9>("c2onb191v5")
								, oid<10>("c2pnb208w1")
								, oid<11>("c2tnb239v1")
								, oid<12>("c2tnb239v2")
								, oid<13>("c2tnb239v3")
								, oid<14>("c2onb239v4")
								, oid<15>("c2onb239v5")
								, oid<16>("c2pnb272w1")
								, oid<17>("c2pnb304w1")
								, oid<18>("c2tnb359v1")
								, oid<19>("c2pnb368w1")
								, oid<20>("c2tnb431r1")
							)
							, oid<1>(
								  oid<1>("prime192v1")	//secp192r1
								, oid<2>("prime192v2")
								, oid<3>("prime192v3")
								, oid<4>("prime239v1")	//secp192r1
								, oid<5>("prime239v2")
								, oid<6>("prime239v3")
								, oid<7>("prime256v1")	//secp256r1
							)
						)
						, oid<4>(
							  oid<1>("ecdsaWithSHA1")
							, oid<2>("ecdsaWithRecommended")
							, oid<3>("ecdsaWithSpecified"
								, oid<1>("ecdsaWithSHA224")
								, oid<2>("ecdsaWithSHA256")
								, oid<3>("ecdsaWithSHA384")
								, oid<4>("ecdsaWithSHA512")
							)
						)
					)
					, oid<10046,2,1>("X9.42 DH")

					, oid<113533>( 	//NORTEL
						  oid<7,66,10>("cast5-cbc")
						, oid<7,66,12>("pbeWithMD5AndCast5CBC")
						// Macs for CMP and CRMF
						, oid<7,66,13>("password based MAC")
						, oid<7,66,30>("Diffie-Hellman based MAC")
					)

					, oid<113549>("RSA Data Security, Inc."
						, oid<1>("RSA Data Security, Inc. PKCS"
							, oid<1>("pkcs1"
								, oid<1>("rsaEncryption")
								, oid<2>("md2WithRSAEncryption")
								, oid<3>("md4WithRSAEncryption")
								, oid<4>("md5WithRSAEncryption")
								, oid<5>("sha1WithRSAEncryption")
								, oid<6>("rsaOAEPEncryptionSET")
								// According to PKCS 1 version 2.1
								, oid<7>("rsaesOaep")
								, oid<8>("mgf1")
								, oid<9>("pSpecified")
								, oid<10>("rsassaPss")
								, oid<11>("sha256WithRSAEncryption")
								, oid<12>("sha384WithRSAEncryption")
								, oid<13>("sha512WithRSAEncryption")
								, oid<14>("sha224WithRSAEncryption")
							)
							, oid<3>("pkcs3"
								, oid<1>("dhKeyAgreement")
							)
							, oid<5>("pkcs5"
								, oid<1>("pbeWithMD2AndDES-CBC")
								, oid<3>("pbeWithMD5AndDES-CBC")
								, oid<4>("pbeWithMD2AndRC2-CBC")
								, oid<6>("pbeWithMD5AndRC2-CBC")
								, oid<10>("pbeWithSHA1AndDES-CBC")
								, oid<11>("pbeWithSHA1AndRC2-CBC")
								, oid<12>("PBKDF2")
								, oid<13>("PBES2")
								, oid<14>("PBMAC1")
							)
							, oid<7>("pkcs7"
								, oid<1>("pkcs7-data")
								, oid<2>("pkcs7-signedData")
								, oid<3>("pkcs7-envelopedData")
								, oid<4>("pkcs7-signedAndEnvelopedData")
								, oid<5>("pkcs7-digestData")
								, oid<6>("pkcs7-encryptedData")
							)
							, oid<9>("pkcs9"
								, oid<1>("emailAddress")
								, oid<2>("unstructuredName")
								, oid<3>("contentType")
								, oid<4>("messageDigest")
								, oid<5>("signingTime")
								, oid<6>("countersignature")
								, oid<7>("challengePassword")
								, oid<8>("unstructuredAddress")
								, oid<9>("extendedCertificateAttributes")
								, oid<14>("Extension Request")
								, oid<15>("S/MIME Capabilities")
								// S/MIME
								, oid<16>("S/MIME"
									, oid<0>("smime-mod"
										// S/MIME Modules
										, oid<1>("smime-mod-cms")
										, oid<2>("smime-mod-ess")
										, oid<3>("smime-mod-oid")
										, oid<4>("smime-mod-msg-v3")
										, oid<5>("smime-mod-ets-eSignature-88")
										, oid<6>("smime-mod-ets-eSignature-97")
										, oid<7>("smime-mod-ets-eSigPolicy-88")
										, oid<8>("smime-mod-ets-eSigPolicy-97")
									)
									, oid<1>("smime-ct"
										// S/MIME Content Types
										, oid<1>("smime-ct-receipt")
										, oid<2>("smime-ct-authData")
										, oid<3>("smime-ct-publishCert")
										, oid<4>("smime-ct-TSTInfo")
										, oid<5>("smime-ct-TDTInfo")
										, oid<6>("smime-ct-contentInfo")
										, oid<7>("smime-ct-DVCSRequestData")
										, oid<8>("smime-ct-DVCSResponseData")
										, oid<9>("smime-ct-compressedData")
										, oid<19>("smime-ct-contentCollection")
										, oid<23>("smime-ct-authEnvelopedData")
										, oid<27>("ct-asciiTextWithCRLF")
										, oid<28>("ct-xml")
									)
									, oid<2>("smime-aa"
										// S/MIME Attributes
										, oid<1>("smime-aa-receiptRequest")
										, oid<2>("smime-aa-securityLabel")
										, oid<3>("smime-aa-mlExpandHistory")
										, oid<4>("smime-aa-contentHint")
										, oid<5>("smime-aa-msgSigDigest")
										// obsolete
										, oid<6>("smime-aa-encapContentType")
										, oid<7>("smime-aa-contentIdentifier")
										// obsolete
										, oid<8>("smime-aa-macValue")
										, oid<9>("smime-aa-equivalentLabels")
										, oid<10>("smime-aa-contentReference")
										, oid<11>("smime-aa-encrypKeyPref")
										, oid<12>("smime-aa-signingCertificate")
										, oid<13>("smime-aa-smimeEncryptCerts")
										, oid<14>("smime-aa-timeStampToken")
										, oid<15>("smime-aa-ets-sigPolicyId")
										, oid<16>("smime-aa-ets-commitmentType")
										, oid<17>("smime-aa-ets-signerLocation")
										, oid<18>("smime-aa-ets-signerAttr")
										, oid<19>("smime-aa-ets-otherSigCert")
										, oid<20>("smime-aa-ets-contentTimestamp")
										, oid<21>("smime-aa-ets-CertificateRefs")
										, oid<22>("smime-aa-ets-RevocationRefs")
										, oid<23>("smime-aa-ets-certValues")
										, oid<24>("smime-aa-ets-revocationValues")
										, oid<25>("smime-aa-ets-escTimeStamp")
										, oid<26>("smime-aa-ets-certCRLTimestamp")
										, oid<27>("smime-aa-ets-archiveTimeStamp")
										, oid<28>("smime-aa-signatureType")
										, oid<29>("smime-aa-dvcs-dvc")
									)
									, oid<3>("smime-alg"
										// S/MIME Algorithm Identifiers
										// obsolete
										, oid<1>("smime-alg-ESDHwith3DES")
										// obsolete
										, oid<2>("smime-alg-ESDHwithRC2")
										// obsolete
										, oid<3>("smime-alg-3DESwrap")
										// obsolete
										, oid<4>("smime-alg-RC2wrap")
										, oid<5>("smime-alg-ESDH")
										, oid<6>("smime-alg-CMS3DESwrap")
										, oid<7>("smime-alg-CMSRC2wrap")
										// RFC 3274
										, oid<8>("zlib compression")
										, oid<9>("alg-PWRI-KEK")
									)
									, oid<4>("smime-cd"
										// S/MIME Certificate Distribution
										, oid<1>("smime-cd-ldap")
									)
									, oid<5>("smime-spq"
										// S/MIME Signature Policy Qualifier
										, oid<1>("smime-spq-ets-sqt-uri")
										, oid<2>("smime-spq-ets-sqt-unotice")
									)
									, oid<6>("smime-cti"
										// S/MIME Commitment Type Identifier
										, oid<1>("smime-cti-ets-proofOfOrigin")
										, oid<2>("smime-cti-ets-proofOfReceipt")
										, oid<3>("smime-cti-ets-proofOfDelivery")
										, oid<4>("smime-cti-ets-proofOfSender")
										, oid<5>("smime-cti-ets-proofOfApproval")
										, oid<6>("smime-cti-ets-proofOfCreation")
									)
								)
								, oid<20>("friendlyName")
								, oid<21>("localKeyID")
								, oid<22>(
									  oid<1>("x509Certificate")
									, oid<2>("sdsiCertificate")
								)
								, oid<23>(
									  oid<1>("x509Crl")
								)
							, oid<12>(
								  oid<1>(
									  oid<1>("pbeWithSHA1And128BitRC4")
									, oid<2>("pbeWithSHA1And40BitRC4")
									, oid<3>("pbeWithSHA1And3-KeyTripleDES-CBC")
									, oid<4>("pbeWithSHA1And2-KeyTripleDES-CBC")
									, oid<5>("pbeWithSHA1And128BitRC2-CBC")
									, oid<6>("pbeWithSHA1And40BitRC2-CBC")
								)
								, oid<10>(
									  oid<1>(
										  oid<1>("keyBag")
										, oid<2>("pkcs8ShroudedKeyBag")
										, oid<3>("certBag")
										, oid<4>("crlBag")
										, oid<5>("secretBag")
										, oid<6>("safeContentsBag")
									)
								)
							)
						)
						)
						, oid<2,2>("md2")
						, oid<2,4>("md4")
						, oid<2,5>("md5")
						, oid<2,6>("hmacWithMD5")
						, oid<2,7>("hmacWithSHA1")
						// From RFC4231
						, oid<2,8>("hmacWithSHA224")
						, oid<2,9>("hmacWithSHA256")
						, oid<2,10>("hmacWithSHA384")
						, oid<2,11>("hmacWithSHA512")
						, oid<3,2>("rc2-cbc")
						, oid<3,4>("rc4")
						, oid<3,7>("des-ede3-cbc")
						, oid<3,8>("rc5-cbc")
						, oid<3,10>("des-cdmf")
					)
				)
			)
			, oid<3>("identified-organization"
				, oid<6>("dod"
					// HMAC oids
					, oid<1>("iana"
						, oid<1>("Directory")
						, oid<2>("Management")
						, oid<3>("Experimental")
						, oid<4>("Private"
							, oid<1>("Enterprises"
								// RFC 2247
								, oid<1466,344>("dcObject")
								// RFC 6962 Extension oids (see http://www.ietf.org/rfc/rfc6962.txt)
								, oid<11129,2,4,2>("CT Precertificate SCTs")
								, oid<11129,2,4,3>("CT Precertificate Poison")
								, oid<11129,2,4,4>("CT Precertificate Signer")
								, oid<11129,2,4,5>("CT Certificate SCTs")

								// SCRYPT algorithm
								, oid<11591,4,11>("scrypt")
								, oid<311>("Microsoft"
									, oid<17,1>("Microsoft CSP Name")
									, oid<17,2>("Microsoft Local Key set")
									, oid<2,1,14>("Microsoft Extension Request")
									, oid<2,1,21>("Microsoft Individual Code Signing")
									, oid<2,1,22>("Microsoft Commercial Code Signing")
									, oid<10,3,1>("Microsoft Trust List Signing")
									, oid<10,3,3>("Microsoft Server Gated Crypto")
									, oid<10,3,4>("Microsoft Encrypted File System")
									, oid<20,2,2>("Microsoft Smartcardlogin")
									, oid<20,2,3>("Microsoft Universal Principal Name")
									// CABForum EV SSL Certificate Guidelines (see https://cabforum.org/extended-validation/) for Subject Jurisdiction of Incorporation or Registration
									, oid<60,2,1,1>("jurisdictionLocalityName")
									, oid<60,2,1,2>("jurisdictionStateOrProvinceName")
									, oid<60,2,1,3>("jurisdictionCountryName")
								)
								, oid<188,7,1,1,2>("idea-cbc")
								, oid<3029,1,2>("bf-cbc")
							)
						)

						, oid<5>("Security" 	//security
							// RFC 4556
							, oid<2,3>("pkinit"
								, oid<4>("PKINIT Client Auth")
								, oid<5>("Signing KDC Response"
									, oid<8,1,1>("hmac-md5")
									, oid<8,1,2>("hmac-sha1")
								)
							)
							, oid<5,7>("PKIX"
								// PKIX Arcs
								, oid<0>("pkix-mod"
									// PKIX Modules
									, oid<1>("pkix1-explicit-88")
									, oid<2>("pkix1-implicit-88")
									, oid<3>("pkix1-explicit-93")
									, oid<4>("pkix1-implicit-93")
									, oid<5>("mod-crmf")
									, oid<6>("mod-cmc")
									, oid<7>("mod-kea-profile-88")
									, oid<8>("mod-kea-profile-93")
									, oid<9>("mod-cmp")
									, oid<10>("mod-qualified-cert-88")
									, oid<11>("mod-qualified-cert-93")
									, oid<12>("mod-attribute-cert")
									, oid<13>("mod-timestamp-protocol")
									, oid<14>("mod-ocsp")
									, oid<15>("mod-dvcs")
									, oid<16>("mod-cmp2000")
								)
								, oid<1>("pe"
									// PKIX Private Extensions
									, oid<1>("Authority Information Access")
									, oid<2>("Biometric Info")
									, oid<3>("qcStatements")
									, oid<4>("ac-auditEntity")
									, oid<5>("ac-targeting")
									, oid<6>("aaControls")
									, oid<7>("sbgp-ipAddrBlock")
									, oid<8>("sbgp-autonomousSysNum")
									, oid<9>("sbgp-routerIdentifier")
									, oid<10>("ac-proxying")
									, oid<11>("Subject Information Access")
									, oid<14>("Proxy Certificate Information")
									, oid<24>("TLS Feature")
								)
								, oid<2>("qt"
									// PKIX policyQualifiers for Internet policy qualifiers
									, oid<1>("Policy Qualifier CPS")
									, oid<2>("Policy Qualifier User Notice")
									, oid<3>("textNotice")
								)
								, oid<3>("kp"
									// PKIX key purpose identifiers
									, oid<1>("TLS Web Server Authentication")
									, oid<2>("TLS Web Client Authentication")
									, oid<3>("Code Signing")
									, oid<4>("E-mail Protection")
									, oid<5>("IPSec End System")
									, oid<6>("IPSec Tunnel")
									, oid<7>("IPSec User")
									, oid<8>("Time Stamping")
									// From OCSP spec RFC2560
									, oid<9>("OCSP Signing")
									, oid<10>("dvcs")
									, oid<17>("ipsec Internet Key Exchange")
									, oid<18>("Ctrl/provision WAP Access")
									, oid<19>("Ctrl/Provision WAP Termination")
									, oid<21>("SSH Client")
									, oid<22>("SSH Server")
									, oid<23>("Send Router")
									, oid<24>("Send Proxied Router")
									, oid<25>("Send Owner")
									, oid<26>("Send Proxied Owner")
								)
								, oid<4>("it"
									// CMP information types
									, oid<1>("it-caProtEncCert")
									, oid<2>("it-signKeyPairTypes")
									, oid<3>("it-encKeyPairTypes")
									, oid<4>("it-preferredSymmAlg")
									, oid<5>("it-caKeyUpdateInfo")
									, oid<6>("it-currentCRL")
									, oid<7>("it-unsupportedoids")
									// obsolete
									, oid<8>("it-subscriptionRequest")
									// obsolete
									, oid<9>("it-subscriptionResponse")
									, oid<10>("it-keyPairParamReq")
									, oid<11>("it-keyPairParamRep")
									, oid<12>("it-revPassphrase")
									, oid<13>("it-implicitConfirm")
									, oid<14>("it-confirmWaitTime")
									, oid<15>("it-origPKIMessage")
									, oid<16>("it-suppLangTags")
								)
								, oid<5>("pkip"
									// CRMF registration
									, oid<1>("regCtrl"
										// CRMF registration controls
										, oid<1>("regCtrl-regToken")
										, oid<2>("regCtrl-authenticator")
										, oid<3>("regCtrl-pkiPublicationInfo")
										, oid<4>("regCtrl-pkiArchiveOptions")
										, oid<5>("regCtrl-oldCertID")
										, oid<6>("regCtrl-protocolEncrKey")
									)
									, oid<2>("regInfo"
										// CRMF registration information
										, oid<1>("regInfo-utf8Pairs")
										, oid<2>("regInfo-certReq")
									)
								)
								, oid<6>("alg"
									// algorithms
									, oid<1>("alg-des40")
									, oid<2>("alg-noSignature")
									, oid<3>("alg-dh-sig-hmac-sha1")
									, oid<4>("alg-dh-pop")
								)
								, oid<7>("cmc"
									// CMC controls
									, oid<1>("cmc-statusInfo")
									, oid<2>("cmc-identification")
									, oid<3>("cmc-identityProof")
									, oid<4>("cmc-dataReturn")
									, oid<5>("cmc-transactionId")
									, oid<6>("cmc-senderNonce")
									, oid<7>("cmc-recipientNonce")
									, oid<8>("cmc-addExtensions")
									, oid<9>("cmc-encryptedPOP")
									, oid<10>("cmc-decryptedPOP")
									, oid<11>("cmc-lraPOPWitness")
									, oid<15>("cmc-getCert")
									, oid<16>("cmc-getCRL")
									, oid<17>("cmc-revokeRequest")
									, oid<18>("cmc-regInfo")
									, oid<19>("cmc-responseInfo")
									, oid<21>("cmc-queryPending")
									, oid<22>("cmc-popLinkRandom")
									, oid<23>("cmc-popLinkWitness")
									, oid<24>("cmc-confirmCertAcceptance")
								)
								, oid<8>("on"
									// other names
									, oid<1>("on-personalData")
									, oid<3>("Permanent Identifier")
								)
								, oid<9>("pda"
									// personal data attributes
									, oid<1>("pda-dateOfBirth")
									, oid<2>("pda-placeOfBirth")
									, oid<3>("pda-gender")
									, oid<4>("pda-countryOfCitizenship")
									, oid<5>("pda-countryOfResidence")
								)
								, oid<10>("aca"
									// attribute certificate attributes
									, oid<1>("aca-authenticationInfo")
									, oid<2>("aca-accessIdentity")
									, oid<3>("aca-chargingIdentity")
									, oid<4>("aca-group")
									// attention : the following seems to be obsolete, replace by 'role'
									, oid<5>("aca-role")
									, oid<6>("aca-encAttrs")
								)
								, oid<11>("qcs"
									// qualified certificate statements
									, oid<1>("qcs-pkixQCSyntax-v1")
								)
								, oid<12>("cct"
									// CMC content types
									, oid<1>("cct-crs")
									, oid<2>("cct-PKIData")
									, oid<3>("cct-PKIResponse")
								)
								, oid<21>("ppl"
									// Predefined Proxy Certificate policy languages
									, oid<0>("Any language")
									, oid<1>("Inherit all")
									, oid<2>("Independent")
								)
								, oid<48>("ad"
									// access descriptors for authority info access extension
									, oid<1>("OCSP" 	//OCSP
										, oid<1>("Basic OCSP Response")
										, oid<2>("OCSP Nonce")
										, oid<3>("OCSP CRL ID")
										, oid<4>("Acceptable OCSP Responses")
										, oid<5>("OCSP No Check")
										, oid<6>("OCSP Archive Cutoff")
										, oid<7>("OCSP Service Locator")
										, oid<8>("Extended OCSP Status")
										, oid<9>("valid")
										, oid<10>("path")
										, oid<11>("Trust Root")
									)
									, oid<2>("CA Issuers")
									, oid<3>("AD Time Stamping")
									, oid<4>("ad dvcs")
									, oid<5>("CA Repository")
								)
							)//PKIX
						)
						, oid<6>("SNMPv2")
						, oid<7>("Mail"
							// RFC 1495
							, oid<1>("MIME MHS"
								, oid<1>("mime-mhs-headings"
									, oid<1>("hex-partial-message")
									, oid<2>("hex-multipart-message")
								)
								, oid<2>("mime-mhs-bodies")
							)
						)
					)//iana
				)//dod

				// RFC 5639 curve oids
				, oid<36,3,3,2,8,1,1>(
					  oid<1>("brainpoolP160r1")
					, oid<2>("brainpoolP160t1")
					, oid<3>("brainpoolP192r1")
					, oid<4>("brainpoolP192t1")
					, oid<5>("brainpoolP224r1")
					, oid<6>("brainpoolP224t1")
					, oid<7>("brainpoolP256r1")
					, oid<8>("brainpoolP256t1")
					, oid<9>("brainpoolP320r1")
					, oid<10>("brainpoolP320t1")
					, oid<11>("brainpoolP384r1")
					, oid<12>("brainpoolP384t1")
					, oid<13>("brainpoolP512r1")
					, oid<14>("brainpoolP512t1")
				)

				// New curves from draft-ietf-curdle-pkix-00
				, oid<101,110>("X25519")
				, oid<101,111>("X448")

				, oid<132>("certicom-arc"
					, oid<0>(
						// SECG curve oids from "SEC 2: Recommended Elliptic Curve Domain Parameters" (http://www.secg.org/) SECG prime curves oids
						  oid<6>("secp112r1")
						, oid<7>("secp112r2")
						, oid<28>("secp128r1")
						, oid<29>("secp128r2")
						, oid<9>("secp160k1")
						, oid<8>("secp160r1")
						, oid<30>("secp160r2")
						, oid<31>("secp192k1")
						// NOTE: the curve secp192r1 is the same as prime192v1 defined above and is therefore omitted
						, oid<32>("secp224k1")
						, oid<33>("secp224r1")
						, oid<10>("secp256k1")
						// NOTE: the curve secp256r1 is the same as prime256v1 defined above and is therefore omitted
						, oid<34>("secp384r1")
						, oid<35>("secp521r1")
						// SECG characteristic two curves oids
						, oid<4>("sect113r1")
						, oid<5>("sect113r2")
						, oid<22>("sect131r1")
						, oid<23>("sect131r2")
						, oid<1>("sect163k1")
						, oid<2>("sect163r1")
						, oid<15>("sect163r2")
						, oid<24>("sect193r1")
						, oid<25>("sect193r2")
						, oid<26>("sect233k1")
						, oid<27>("sect233r1")
						, oid<3>("sect239k1")
						, oid<16>("sect283k1")
						, oid<17>("sect283r1")
						, oid<36>("sect409k1")
						, oid<37>("sect409r1")
						, oid<38>("sect571k1")
						, oid<39>("sect571r1")
					)
					, oid<1>(
						  oid<11,0>("dhSinglePass-stdDH-sha224kdf-scheme")
						, oid<11,1>("dhSinglePass-stdDH-sha256kdf-scheme")
						, oid<11,2>("dhSinglePass-stdDH-sha384kdf-scheme")
						, oid<11,3>("dhSinglePass-stdDH-sha512kdf-scheme")
						, oid<14,0>("dhSinglePass-cofactorDH-sha224kdf-scheme")
						, oid<14,1>("dhSinglePass-cofactorDH-sha256kdf-scheme")
						, oid<14,2>("dhSinglePass-cofactorDH-sha384kdf-scheme")
						, oid<14,3>("dhSinglePass-cofactorDH-sha512kdf-scheme")
					)
				)

				// ECDH schemes from RFC5753
				, oid<133,16,840,63,0>(
					  oid<2>("dhSinglePass-stdDH-sha1kdf-scheme")
					, oid<3>("dhSinglePass-cofactorDH-sha1kdf-scheme")
				)

				, oid<14,3,2>("algorithm"
					, oid<3>("md5WithRSA")
					, oid<6>("des-ecb")
					, oid<7>("des-cbc")
					, oid<8>("des-ofb")
					, oid<9>("des-cfb")
					, oid<11>("rsaSignature")
					, oid<12>("dsaEncryption-old")
					, oid<13>("dsaWithSHA")
					, oid<15>("shaWithRSAEncryption")
					, oid<17>("des-ede")
					, oid<18>("sha")
					, oid<26>("sha1")
					, oid<27>("dsaWithSHA1-old")
					, oid<29>("sha1WithRSA")
				)

				, oid<36,3,2,1>("ripemd160")
				, oid<36,3,3,1,2>("ripemd160WithRSA")
				, oid<6,1,4,1,1722,12,2,1,16>("blake2b512")
				, oid<6,1,4,1,1722,12,2,2,8>("blake2s256")
				, oid<101,1,4,1>("Strong Extranet ID")
			)//org
		)//iso

		, oid<2>("joint-iso-itu-t"

			, oid<5>("directory services (X.500)"
				, oid<1,5>("Selected Attribute Types"
					, oid<55>("clearance")
				)
				, oid<4>("X509"
					, oid<3>("commonName")
					, oid<4>("surname")
					, oid<5>("serialNumber")
					, oid<6>("countryName")
					, oid<7>("localityName")
					, oid<8>("stateOrProvinceName")
					, oid<9>("streetAddress")
					, oid<10>("organizationName")
					, oid<11>("organizationalUnitName")
					, oid<12>("title")
					, oid<13>("description")
					, oid<14>("searchGuide")
					, oid<15>("businessCategory")
					, oid<16>("postalAddress")
					, oid<17>("postalCode")
					, oid<18>("postOfficeBox")
					, oid<19>("physicalDeliveryOfficeName")
					, oid<20>("telephoneNumber")
					, oid<21>("telexNumber")
					, oid<22>("teletexTerminalIdentifier")
					, oid<23>("facsimileTelephoneNumber")
					, oid<24>("x121Address")
					, oid<25>("internationaliSDNNumber")
					, oid<26>("registeredAddress")
					, oid<27>("destinationIndicator")
					, oid<28>("preferredDeliveryMethod")
					, oid<29>("presentationAddress")
					, oid<30>("supportedApplicationContext")
					, oid<31>("member")
					, oid<32>("owner")
					, oid<33>("roleOccupant")
					, oid<34>("seeAlso")
					, oid<35>("userPassword")
					, oid<36>("userCertificate")
					, oid<37>("cACertificate")
					, oid<38>("authorityRevocationList")
					, oid<39>("certificateRevocationList")
					, oid<40>("crossCertificatePair")
					, oid<41>("name")
					, oid<42>("givenName")
					, oid<43>("initials")
					, oid<44>("generationQualifier")
					, oid<45>("x500UniqueIdentifier")
					, oid<46>("dnQualifier")
					, oid<47>("enhancedSearchGuide")
					, oid<48>("protocolInformation")
					, oid<49>("distinguishedName")
					, oid<50>("uniqueMember")
					, oid<51>("houseIdentifier")
					, oid<52>("supportedAlgorithms")
					, oid<53>("deltaRevocationList")
					, oid<54>("dmdName")
					, oid<65>("pseudonym")
					, oid<72>("role")
				)
				, oid<8>("directory services - algorithms"
					, oid<1,1>("rsa")
					, oid<3,100>("mdc2WithRSA")
					, oid<3,101>("mdc2")
				)
				, oid<29>("ce"
					, oid<9>("X509v3 Subject Directory Attributes")
					, oid<14>("X509v3 Subject Key Identifier")
					, oid<15>("X509v3 Key Usage")
					, oid<16>("X509v3 Private Key Usage Period")
					, oid<17>("X509v3 Subject Alternative Name")
					, oid<18>("X509v3 Issuer Alternative Name")
					, oid<19>("X509v3 Basic Constraints")
					, oid<20>("X509v3 CRL Number")
					, oid<21>("X509v3 CRL Reason Code")
					// Hold instruction CRL entry extension
					, oid<23>("Hold Instruction Code"
						, oid<1>("Hold Instruction None")
						, oid<2>("Hold Instruction Call Issuer")
						, oid<3>("Hold Instruction Reject")
					)
					, oid<24>("Invalidity Date")
					, oid<27>("X509v3 Delta CRL Indicator")
					, oid<28>("X509v3 Issuing Distribution point")
					, oid<29>("X509v3 Certificate Issuer")
					, oid<30>("X509v3 Name Constraints")
					, oid<31>("X509v3 CRL Distribution Points")
					, oid<32>("X509v3 Certificate Policies"
						, oid<0>("X509v3 Any Policy")
					)
					, oid<33>("X509v3 Policy Mappings")
					, oid<35>("X509v3 Authority Key Identifier")
					, oid<36>("X509v3 Policy Constraints")
					, oid<37>("X509v3 Extended Key Usage"
						// From RFC5280
						, oid<0>("Any Extended Key Usage")
					)
					, oid<46>("X509v3 Freshest CRL")
					, oid<54>("X509v3 Inhibit Any Policy")
					, oid<55>("X509v3 AC Targeting")
					, oid<56>("X509v3 No Revocation Available")
				)
			)

			, oid<16>(	//country
				//56 - Belgium object identifier
				//60 - Bermuda
				//76 - Brazil
				//124 - Canada
				//156 - China
				//158 - Taiwan
				//180 - Congo
				//208 - DENMARK
				//250 - France
				//276 - Germany
				//344 - Hong Kong
				//348 - Hungary
				//356 - India Object Identifiers
				//364 - ir
				//380 - Italy
				//528 - Nederland
				//578 - Norway
				//620 - Portugal
				//724 - Spain
				//756 - Switzerland
				//764 - Thailand
				//792 - Turkey
				//840 - USA
				//862 - The oid of Venezuela
				//886 - Taiwan

				  oid<840,1,113730>("Netscape Communications Corp."
					, oid<1>("Netscape Certificate Extension"
						, oid<1>("Netscape Cert Type")
						, oid<2>("Netscape Base Url")
						, oid<3>("Netscape Revocation Url")
						, oid<4>("Netscape CA Revocation Url")
						, oid<7>("Netscape Renewal Url")
						, oid<8>("Netscape CA Policy Url")
						, oid<12>("Netscape SSL Server Name")
						, oid<13>("Netscape Comment")
					)
					, oid<2>("Netscape Data Type"
						, oid<5>("Netscape Certificate Sequence")
					)
					, oid<4,1>("Netscape Server Gated Crypto")
				)

				, oid<840,1,101,3>(
					  oid<4>(
						  oid<1>(
							// AES aka Rijndael
							  oid<1>("aes-128-ecb")
							, oid<2>("aes-128-cbc")
							, oid<3>("aes-128-ofb")
							, oid<4>("aes-128-cfb")
							, oid<5>("aes128-wrap")
							, oid<6>("aes-128-gcm")
							, oid<7>("aes-128-ccm")
							, oid<8>("aes128-wrap-pad")
							, oid<21>("aes-192-ecb")
							, oid<22>("aes-192-cbc")
							, oid<23>("aes-192-ofb")
							, oid<24>("aes-192-cfb")
							, oid<25>("aes192-wrap")
							, oid<26>("aes-192-gcm")
							, oid<27>("aes-192-ccm")
							, oid<28>("aes192-wrap-pad")
							, oid<41>("aes-256-ecb")
							, oid<42>("aes-256-cbc")
							, oid<43>("aes-256-ofb")
							, oid<44>("aes-256-cfb")
							, oid<45>("aes256-wrap")
							, oid<46>("aes-256-gcm")
							, oid<47>("aes-256-ccm")
							, oid<48>("aes256-wrap-pad")
						)
						, oid<2>(
							  oid<1>("sha256")
							, oid<2>("sha384")
							, oid<3>("sha512")
							, oid<4>("sha224")
						)
						, oid<3>(
							  oid<1>("dsaWithSHA224")
							, oid<2>("dsaWithSHA256")
							, oid<3>("dsaWithSHA384")
							, oid<4>("dsaWithSHA512")
							, oid<5>("dsaWithSHA-3-224")
							, oid<6>("dsaWithSHA-3-256")
							, oid<7>("dsaWithSHA-3-384")
							, oid<8>("dsaWithSHA-3-512")
							, oid<9>( "ecdsaWithSHA-3-224")
							, oid<10>("ecdsaWithSHA-3-256")
							, oid<11>("ecdsaWithSHA-3-384")
							, oid<12>("ecdsaWithSHA-3-512")
							, oid<13>("rsaWithSHA-3-224")
							, oid<14>("rsaWithSHA-3-256")
							, oid<15>("rsaWithSHA-3-384")
							, oid<16>("rsaWithSHA-3-512")
						)
					)
				)
			)	//country

			, oid<23>("International Organizations"
				, oid<43>("wap"
					, oid<1>("wap-wsg")
					, oid<4>(
						// WAP/TLS curve oids (http://www.wapforum.org/)
						  oid<1>("wap-wsg-idm-ecid-wtls1")
						, oid<3>("wap-wsg-idm-ecid-wtls3")
						, oid<4>("wap-wsg-idm-ecid-wtls4")
						, oid<5>("wap-wsg-idm-ecid-wtls5")
						, oid<6>("wap-wsg-idm-ecid-wtls6")
						, oid<7>("wap-wsg-idm-ecid-wtls7")
						, oid<8>("wap-wsg-idm-ecid-wtls8")
						, oid<9>("wap-wsg-idm-ecid-wtls9")
						, oid<10>("wap-wsg-idm-ecid-wtls10")
						, oid<11>("wap-wsg-idm-ecid-wtls11")
						, oid<12>("wap-wsg-idm-ecid-wtls12")
					)
				)

				, oid<42>("Secure Electronic Transactions"
					, oid<0>("content types"
						, oid<0>("setct-PANData")
						, oid<1>("setct-PANToken")
						, oid<2>("setct-PANOnly")
						, oid<3>("setct-oidata")
						, oid<4>("setct-PI")
						, oid<5>("setct-PIData")
						, oid<6>("setct-PIDataUnsigned")
						, oid<7>("setct-HODInput")
						, oid<8>("setct-AuthResBaggage")
						, oid<9>("setct-AuthRevReqBaggage")
						, oid<10>("setct-AuthRevResBaggage")
						, oid<11>("setct-CapTokenSeq")
						, oid<12>("setct-PInitResData")
						, oid<13>("setct-PI-TBS")
						, oid<14>("setct-PResData")
						, oid<16>("setct-AuthReqTBS")
						, oid<17>("setct-AuthResTBS")
						, oid<18>("setct-AuthResTBSX")
						, oid<19>("setct-AuthTokenTBS")
						, oid<20>("setct-CapTokenData")
						, oid<21>("setct-CapTokenTBS")
						, oid<22>("setct-AcqCardCodeMsg")
						, oid<23>("setct-AuthRevReqTBS")
						, oid<24>("setct-AuthRevResData")
						, oid<25>("setct-AuthRevResTBS")
						, oid<26>("setct-CapReqTBS")
						, oid<27>("setct-CapReqTBSX")
						, oid<28>("setct-CapResData")
						, oid<29>("setct-CapRevReqTBS")
						, oid<30>("setct-CapRevReqTBSX")
						, oid<31>("setct-CapRevResData")
						, oid<32>("setct-CredReqTBS")
						, oid<33>("setct-CredReqTBSX")
						, oid<34>("setct-CredResData")
						, oid<35>("setct-CredRevReqTBS")
						, oid<36>("setct-CredRevReqTBSX")
						, oid<37>("setct-CredRevResData")
						, oid<38>("setct-PCertReqData")
						, oid<39>("setct-PCertResTBS")
						, oid<40>("setct-BatchAdminReqData")
						, oid<41>("setct-BatchAdminResData")
						, oid<42>("setct-CardCInitResTBS")
						, oid<43>("setct-MeAqCInitResTBS")
						, oid<44>("setct-RegFormResTBS")
						, oid<45>("setct-CertReqData")
						, oid<46>("setct-CertReqTBS")
						, oid<47>("setct-CertResData")
						, oid<48>("setct-CertInqReqTBS")
						, oid<49>("setct-ErrorTBS")
						, oid<50>("setct-PIDualSignedTBE")
						, oid<51>("setct-PIUnsignedTBE")
						, oid<52>("setct-AuthReqTBE")
						, oid<53>("setct-AuthResTBE")
						, oid<54>("setct-AuthResTBEX")
						, oid<55>("setct-AuthTokenTBE")
						, oid<56>("setct-CapTokenTBE")
						, oid<57>("setct-CapTokenTBEX")
						, oid<58>("setct-AcqCardCodeMsgTBE")
						, oid<59>("setct-AuthRevReqTBE")
						, oid<60>("setct-AuthRevResTBE")
						, oid<61>("setct-AuthRevResTBEB")
						, oid<62>("setct-CapReqTBE")
						, oid<63>("setct-CapReqTBEX")
						, oid<64>("setct-CapResTBE")
						, oid<65>("setct-CapRevReqTBE")
						, oid<66>("setct-CapRevReqTBEX")
						, oid<67>("setct-CapRevResTBE")
						, oid<68>("setct-CredReqTBE")
						, oid<69>("setct-CredReqTBEX")
						, oid<70>("setct-CredResTBE")
						, oid<71>("setct-CredRevReqTBE")
						, oid<72>("setct-CredRevReqTBEX")
						, oid<73>("setct-CredRevResTBE")
						, oid<74>("setct-BatchAdminReqTBE")
						, oid<75>("setct-BatchAdminResTBE")
						, oid<76>("setct-RegFormReqTBE")
						, oid<77>("setct-CertReqTBE")
						, oid<78>("setct-CertReqTBEX")
						, oid<79>("setct-CertResTBE")
						, oid<80>("setct-CRLNotificationTBS")
						, oid<81>("setct-CRLNotificationResTBS")
						, oid<82>("setct-BCIDistributionTBS")
					)
					, oid<1>("message extensions"
						, oid<1>("generic cryptogram")
						, oid<3>("merchant initiated auth")
						, oid<4>("setext-pinSecure")
						, oid<5>("setext-pinAny")
						, oid<7>("setext-track2")
						, oid<8>("additional verification")
					)
					, oid<3>("set-attr"
						, oid<0>("setAttr-Cert"
							, oid<0>("set-rootKeyThumb")
							, oid<1>("set-addPolicy")
						)
						, oid<1>("payment gateway capabilities")
						, oid<2>("setAttr-TokenType"
							, oid<1>("setAttr-Token-EMV")
							, oid<2>("setAttr-Token-B0Prime")
						)
						, oid<3>("issuer capabilities"
							, oid<3>("setAttr-IssCap-CVM"
								, oid<1>("generate cryptogram")
							)
							, oid<4>("setAttr-IssCap-T2"
								, oid<1>("encrypted track 2")
								, oid<2>("cleartext track 2")
							)
							, oid<5>("setAttr-IssCap-Sig"
								, oid<1>("ICC or token signature")
								, oid<2>("secure device signature")
							)
						)
					)
					, oid<5>("set-policy"
						, oid<0>("set-policy-root")
					)
					, oid<7>("certificate extensions"
						, oid<0>("setCext-hashedRoot")
						, oid<1>("setCext-certType")
						, oid<2>("setCext-merchData")
						, oid<3>("setCext-cCertRequired")
						, oid<4>("setCext-tunneling")
						, oid<5>("setCext-setExt")
						, oid<6>("setCext-setQualf")
						, oid<7>("setCext-PGWYcapabilities")
						, oid<8>("setCext-TokenIdentifier")
						, oid<9>("setCext-Track2Data")
						, oid<10>("setCext-TokenType")
						, oid<11>("setCext-IssuerCapabilities")
					)
					, oid<8>("set-brand"
						, oid<1>("set-brand-IATA-ATA")
						, oid<30>("set-brand-Diners")
						, oid<34>("set-brand-AmericanExpress")
						, oid<35>("set-brand-JCB")
						, oid<4>("set-brand-Visa")
						, oid<5>("set-brand-MasterCard")
						, oid<6011>("set-brand-Novus")
					)
				)
			)
		)
	);
	return o;
}
