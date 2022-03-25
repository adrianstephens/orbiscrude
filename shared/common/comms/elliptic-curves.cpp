#include "elliptic-curves.h"

using namespace iso;

/*------------------------------------------
Curve names chosen by
different standards organizations
------------+---------------+-------------
SECG        |  ANSI X9.62   |  NIST
------------+---------------+-------------
sect163k1   |               |   NIST K-163
sect163r1   |               |
sect163r2   |               |   NIST B-163
sect193r1   |               |
sect193r2   |               |
sect233k1   |               |   NIST K-233
sect233r1   |               |   NIST B-233
sect239k1   |               |
sect283k1   |               |   NIST K-283
sect283r1   |               |   NIST B-283
sect409k1   |               |   NIST K-409
sect409r1   |               |   NIST B-409
sect571k1   |               |   NIST K-571
sect571r1   |               |   NIST B-571
secp160k1   |               |
secp160r1   |               |
secp160r2   |               |
secp192k1   |               |
secp192r1   |  prime192v1   |   NIST P-192
secp224k1   |               |
secp224r1   |               |   NIST P-224
secp256k1   |               |
secp256r1   |  prime256v1   |   NIST P-256
secp384r1   |               |   NIST P-384
secp521r1   |               |   NIST P-521
------------+---------------+-------------*/

const EC_curve *EC_curve::get_named(const char *name) {

	static EC_curve prime192v1(
		EC_curve::SECP_R1,
		mpi_const<uint32, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFE, 0xFFFFFFFF, 0xFFFFFFFF>(),
		mpi_const<uint32, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFE, 0xFFFFFFFF, 0xFFFFFFFC>(),
		mpi_const<uint32, 0x64210519, 0xE59C80E7, 0x0FA7E9AB, 0x72243049, 0xFEB8DEEC, 0xC146B9B1>(),
		mpi_const<uint32, 0x188DA80E, 0xB03090F6, 0x7CBF20EB, 0x43A18800, 0xF4FF0AFD, 0x82FF1012>(),
		mpi_const<uint32, 0x07192B95, 0xFFC8DA78, 0x631011ED, 0x6B24CDD5, 0x73F977A1, 0x1E794811>(),
		mpi_const<uint32, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x99DEF836, 0x146BC9B1, 0xB4D22831>(),
		1,
		[](mpi &a, param(mpi) p) {
			mpi	s(6, false);
			mpi	t(6, false);
			a.grow(12);
			t.copy_elements(a, 0, 0, 6);
			s.copy_elements(a, 0, 6, 2);
			s.copy_elements(a, 2, 6, 2);
			s.clear_elements(4, 2);
			t = t + s;
			s.clear_elements(0, 2);
			s.copy_elements(a, 2, 8, 2);
			s.copy_elements(a, 4, 8, 2);
			t = t + s;
			s.copy_elements(a, 0, 10, 2);
			s.copy_elements(a, 2, 10, 2);
			s.copy_elements(a, 4, 10, 2);
			t = t + s;
			while (t >= p)
				t = t - p;
			a = t;
		}
	),	prime256v1(
		EC_curve::SECP_R1,
		mpi_const<uint32, 0xFFFFFFFF, 0x00000001, 0x00000000, 0x00000000, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF>(),
		mpi_const<uint32, 0xFFFFFFFF, 0x00000001, 0x00000000, 0x00000000, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFC>(),
		mpi_const<uint32, 0x5AC635D8, 0xAA3A93E7, 0xB3EBBD55, 0x769886BC, 0x651D06B0, 0xCC53B0F6, 0x3BCE3C3E, 0x27D2604B>(),
		mpi_const<uint32, 0x6B17D1F2, 0xE12C4247, 0xF8BCE6E5, 0x63A440F2, 0x77037D81, 0x2DEB33A0, 0xF4A13945, 0xD898C296>(),
		mpi_const<uint32, 0x4FE342E2, 0xFE1A7F9B, 0x8EE7EB4A, 0x7C0F9E16, 0x2BCE3357, 0x6B315ECE, 0xCBB64068, 0x37BF51F5>(),
		mpi_const<uint32, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF, 0xBCE6FAAD, 0xA7179E84, 0xF3B9CAC2, 0xFC632551>(),
		1,
		[](mpi &a, param(mpi) p) {
			mpi	s(8, false), t(8, false);
			a.grow(16);
			t.copy_elements(a, 0, 0, 8);
			s.clear_elements(0, 3);
			s.copy_elements(a, 3, 11, 5);
			t = t + s;
			t = t + s;
			s.clear_elements(0, 3);
			s.copy_elements(a, 3, 12, 4);
			s.clear_elements(7, 1);
			t = t + s;
			t = t + s;
			s.copy_elements(a, 0, 8, 3);
			s.clear_elements(3, 3);
			s.copy_elements(a, 6, 14, 2);
			t = t + s;
			s.copy_elements(a, 0, 9, 3);
			s.copy_elements(a, 3, 13, 3);
			s.copy_elements(a, 6, 13, 1);
			s.copy_elements(a, 7, 8, 1);
			t = t + s;
			s.copy_elements(a, 0, 11, 3);
			s.clear_elements(3, 3);
			s.copy_elements(a, 6, 8, 1);
			s.copy_elements(a, 7, 10, 1);
			t = t - s;
			s.copy_elements(a, 0, 12, 4);
			s.clear_elements(4, 2);
			s.copy_elements(a, 6, 9, 1);
			s.copy_elements(a, 7, 11, 1);
			t = t - s;
			s.copy_elements(a, 0, 13, 3);
			s.copy_elements(a, 3, 8, 3);
			s.clear_elements(6, 1);
			s.copy_elements(a, 7, 12, 1);
			t = t - s;
			s.copy_elements(a, 0, 14, 2);
			s.clear_elements(2, 1);
			s.copy_elements(a, 3, 9, 3);
			s.clear_elements(6, 1);
			s.copy_elements(a, 7, 13, 1);
			t = t - s;
			while (t >= p)
				t = t - p;
			while (t < 0)
				t = t + p;
			a = t;
		}
	), prime384v1(
		EC_curve::SECP_R1,
		mpi_const<uint32, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFE, 0xFFFFFFFF, 0x00000000, 0x00000000, 0xFFFFFFFF>(),
		mpi_const<uint32, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFE, 0xFFFFFFFF, 0x00000000, 0x00000000, 0xFFFFFFFC>(),
		mpi_const<uint32, 0xB3312FA7, 0xE23EE7E4, 0x988E056B, 0xE3F82D19, 0x181D9C6E, 0xFE814112, 0x0314088F, 0x5013875A, 0xC656398D, 0x8A2ED19D, 0x2A85C8ED, 0xD3EC2AEF>(),
		mpi_const<uint32, 0xAA87CA22, 0xBE8B0537, 0x8EB1C71E, 0xF320AD74, 0x6E1D3B62, 0x8BA79B98, 0x59F741E0, 0x82542A38, 0x5502F25D, 0xBF55296C, 0x3A545E38, 0x72760AB7>(),
		mpi_const<uint32, 0x3617DE4A, 0x96262C6F, 0x5D9E98BF, 0x9292DC29, 0xF8F41DBD, 0x289A147C, 0xE9DA3113, 0xB5F0B8C0, 0x0A60B1CE, 0x1D7E819D, 0x7A431D7C, 0x90EA0E5F>(),
		mpi_const<uint32, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xC7634D81, 0xF4372DDF, 0x581A0DB2, 0x48B0A77A, 0xECEC196A, 0xCCC52973>(),
		1,
		[](mpi &a, param(mpi) p) {
			mpi	s(12, false), t(12, false);
			a.grow(24);
			t.copy_elements(a, 0, 0, 12);
			s.clear_elements(0, 4);
			s.copy_elements(a, 4, 21, 3);
			s.clear_elements(7, 5);
			t = t + s;
			t = t + s;
			s.copy_elements(a, 0, 12, 12);
			t = t + s;
			s.copy_elements(a, 0, 21, 3);
			s.copy_elements(a, 3, 12, 9);
			t = t + s;
			s.clear_elements(0, 1);
			s.copy_elements(a, 1, 23, 1);
			s.clear_elements(2, 1);
			s.copy_elements(a, 3, 20, 1);
			s.copy_elements(a, 4, 12, 8);
			t = t + s;
			s.clear_elements(0, 4);
			s.copy_elements(a, 4, 20, 4);
			s.clear_elements(8, 4);
			t = t + s;
			s.copy_elements(a, 0, 20, 1);
			s.clear_elements(1, 2);
			s.copy_elements(a, 3, 21, 3);
			s.clear_elements(6, 6);
			t = t + s;
			s.copy_elements(a, 0, 23, 1);
			s.copy_elements(a, 1, 12, 11);
			t = t - s;
			s.clear_elements(0, 1);
			s.copy_elements(a, 1, 20, 4);
			s.clear_elements(5, 7);
			t = t - s;
			s.clear_elements(0, 3);
			s.copy_elements(a, 3, 23, 1);
			s.copy_elements(a, 4, 23, 1);
			s.clear_elements(5, 7);
			t = t - s;
			while (t >= p)
				t = t - p;
			while (t < 0)
				t = t + p;
			a = t;
		}
	), prime512v1(
		EC_curve::SECP_R1,
		mpi_const<uint32, 0x000001FF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF>(),
		mpi_const<uint32, 0x000001FF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFC>(),
		mpi_const<uint32, 0x00000051, 0x953EB961, 0x8E1C9A1F, 0x929A21A0, 0xB68540EE, 0xA2DA725B, 0x99B315F3, 0xB8B48991, 0x8EF109E1, 0x56193951, 0xEC7E937B, 0x1652C0BD, 0x3BB1BF07, 0x3573DF88, 0x3D2C34F1, 0xEF451FD4, 0x6B503F00>(),
		mpi_const<uint32, 0x000000C6, 0x858E06B7, 0x0404E9CD, 0x9E3ECB66, 0x2395B442, 0x9C648139, 0x053FB521, 0xF828AF60, 0x6B4D3DBA, 0xA14B5E77, 0xEFE75928, 0xFE1DC127, 0xA2FFA8DE, 0x3348B3C1, 0x856A429B, 0xF97E7E31, 0xC2E5BD66>(),
		mpi_const<uint32, 0x00000118, 0x39296A78, 0x9A3BC004, 0x5C8A5FB4, 0x2C7D1BD9, 0x98F54449, 0x579B4468, 0x17AFBD17, 0x273E662C, 0x97EE7299, 0x5EF42640, 0xC550B901, 0x3FAD0761, 0x353C7086, 0xA272C240, 0x88BE9476, 0x9FD16650>(),
		mpi_const<uint32, 0x000001FF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFA, 0x51868783, 0xBF2F966B, 0x7FCC0148, 0xF709A5D0, 0x3BB5C9B8, 0x899C47AE, 0xBB6FB71E, 0x91386409>(),
		1,
		[](mpi &a, param(mpi) p) {
			mpi	t(17, false);
			a.grow(18);
			t.copy_elements(a, 0, 0, 17);
			t.elements()[16] &= 0x000001FF;
			a >>= 521;
			a += t;
			while (a >= p)
				a = a - p;
		}
	);


	static named<EC_curve> curves[] = {
	named<EC_curve>(
		"secp112r1",
		EC_curve::SECP_R1,
		mpi_const<uint32, 0x0000DB7C, 0x2ABF62E3, 0x5E668076, 0xBEAD208B>(),
		mpi_const<uint32, 0x0000DB7C, 0x2ABF62E3, 0x5E668076, 0xBEAD2088>(),
		mpi_const<uint32, 0x0000659E, 0xF8BA0439, 0x16EEDE89, 0x11702B22>(),
		mpi_const<uint32, 0x00000948, 0x7239995A, 0x5EE76B55, 0xF9C2F098>(),
		mpi_const<uint32, 0x0000A89C, 0xE5AF8724, 0xC0A23E0E, 0x0FF77500>(),
		mpi_const<uint32, 0x0000DB7C, 0x2ABF62E3, 0x5E7628DF, 0xAC6561C5>(),
		1
	),

	named<EC_curve>(
		"secp112r2",
		EC_curve::SECP_R2,
		mpi_const<uint32, 0x0000DB7C, 0x2ABF62E3, 0x5E668076, 0xBEAD208B>(),
		mpi_const<uint32, 0x00006127, 0xC24C05F3, 0x8A0AAAF6, 0x5C0EF02C>(),
		mpi_const<uint32, 0x000051DE, 0xF1815DB5, 0xED74FCC3, 0x4C85D709>(),
		mpi_const<uint32, 0x00004BA3, 0x0AB5E892, 0xB4E1649D, 0xD0928643>(),
		mpi_const<uint32, 0x0000ADCD, 0x46F5882E, 0x3747DEF3, 0x6E956E97>(),
		mpi_const<uint32, 0x000036DF, 0x0AAFD8B8, 0xD7597CA1, 0x0520D04B>(),
		4
	),

	named<EC_curve>(
		"secp128r1",
		EC_curve::SECP_R1,
		mpi_const<uint32, 0xFFFFFFFD, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF>(),
		mpi_const<uint32, 0xFFFFFFFD, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFC>(),
		mpi_const<uint32, 0xE87579C1, 0x1079F43D, 0xD824993C, 0x2CEE5ED3>(),
		mpi_const<uint32, 0x161FF752, 0x8B899B2D, 0x0C28607C, 0xA52C5B86>(),
		mpi_const<uint32, 0xCF5AC839, 0x5BAFEB13, 0xC02DA292, 0xDDED7A83>(),
		mpi_const<uint32, 0xFFFFFFFE, 0x00000000, 0x75A30D1B, 0x9038A115>(),
		1,
		[](mpi &a, param(mpi) p) {
			mpi	t(8, false);
			a.grow(8);
			do {
				t.copy_elements(a, 0, 4, 4);
				t.clear_elements(4, 4);
				a.clear_elements(4, 4);
				a += t;
				t <<= 97;
				a += t;
			} while (a > p);
		}
	),

	named<EC_curve>(
		"secp128r2",
		EC_curve::SECP_R2,
		mpi_const<uint32, 0xFFFFFFFD, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF>(),
		mpi_const<uint32, 0xD6031998, 0xD1B3BBFE, 0xBF59CC9B, 0xBFF9AEE1>(),
		mpi_const<uint32, 0x5EEEFCA3, 0x80D02919, 0xDC2C6558, 0xBB6D8A5D>(),
		mpi_const<uint32, 0x7B6AA5D8, 0x5E572983, 0xE6FB32A7, 0xCDEBC140>(),
		mpi_const<uint32, 0x27B6916A, 0x894D3AEE, 0x7106FE80, 0x5FC34B44>(),
		mpi_const<uint32, 0x3FFFFFFF, 0x7FFFFFFF, 0xBE002472, 0x0613B5A3>(),
		4,
		[](mpi &a, param(mpi) p) {
			mpi	t(8, false);
			a.grow(8);
			do {
				t.copy_elements(a, 0, 4, 4);
				t.clear_elements(4, 4);
				a.clear_elements(4, 4);
				a += t;
				t <<= 97;
				a += t;
			} while (a > p);
		}
	),

	named<EC_curve>(
		"secp160k1",
		EC_curve::SECP_K1,
		mpi_const<uint32, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFE, 0xFFFFAC73>(),
		zero,
		mpi_const1<7>(),
		mpi_const<uint32, 0x3B4C382C, 0xE37AA192, 0xA4019E76, 0x3036F4F5, 0xDD4D7EBB>(),
		mpi_const<uint32, 0x938CF935, 0x318FDCED, 0x6BC28286, 0x531733C3, 0xF03C4FEE>(),
		mpi_const<uint32, 0x00000001, 0x00000000, 0x00000000, 0x0001B8FA, 0x16DFAB9A, 0xCA16B6B3>(),
		1,
		[](mpi &a, param(mpi) p) {
			mpi	t(6, false);
			a.grow(10);
			do {
				t.clear_elements(0, 1);
				t.copy_elements(a, 1, 5, 5);
				a.clear_elements(5, 5);
				a += t;
				t = (t >> 32) * 21389;
				a += t;
			} while (a > p);
		}
	),

	named<EC_curve>(
		"secp160r1",
		EC_curve::SECP_R1,
		mpi_const<uint32, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x7FFFFFFF>(),
		mpi_const<uint32, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x7FFFFFFC>(),
		mpi_const<uint32, 0x1C97BEFC, 0x54BD7A8B, 0x65ACF89F, 0x81D4D4AD, 0xC565FA45>(),
		mpi_const<uint32, 0x4A96B568, 0x8EF57328, 0x46646989, 0x68C38BB9, 0x13CBFC82>(),
		mpi_const<uint32, 0x23A62855, 0x3168947D, 0x59DCC912, 0x04235137, 0x7AC5FB32>(),
		mpi_const<uint32, 0x00000001, 0x00000000, 0x00000000, 0x0001F4C8, 0xF927AED3, 0xCA752257>(),
		1,
		[](mpi &a, param(mpi) p) {
			mpi	t(6, false);
			a.grow(10);
			do {
				t.copy_elements(a, 0, 5, 5);
				t.clear_elements(5, 1);
				a.clear_elements(5, 5);
				a += t;
				t <<= 31;
				a += t;
			} while (a > p);
		}
	),

	named<EC_curve>(
		"secp160r2",
		EC_curve::SECP_R2,
		mpi_const<uint32, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFE, 0xFFFFAC73>(),
		mpi_const<uint32, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFE, 0xFFFFAC70>(),
		mpi_const<uint32, 0xB4E134D3, 0xFB59EB8B, 0xAB572749, 0x04664D5A, 0xF50388BA>(),
		mpi_const<uint32, 0x52DCB034, 0x293A117E, 0x1F4FF11B, 0x30F7199D, 0x3144CE6D>(),
		mpi_const<uint32, 0xFEAFFEF2, 0xE331F296, 0xE071FA0D, 0xF9982CFE, 0xA7D43F2E>(),
		mpi_const<uint32, 0x00000001, 0x00000000, 0x00000000, 0x0000351E, 0xE786A818, 0xF3A1A16B>(),
		1,
		[](mpi &a, param(mpi) p) {
			mpi	t(6, false);
			a.grow(10);
			do {
				t.clear_elements(0, 1);
				t.copy_elements(a, 1, 5, 5);
				a.clear_elements(5, 5);
				a += t;
				t = (t >> 32) * 21389;
				a += t;
			} while (a > p);
		}
	),

	named<EC_curve>(
		"secp192k1",
		EC_curve::SECP_K1,
		mpi_const<uint32, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFE, 0xFFFFEE37>(),
		zero,
		mpi_const1<3>(),
		mpi_const<uint32, 0xDB4FF10E, 0xC057E9AE, 0x26B07D02, 0x80B7F434, 0x1DA5D1B1, 0xEAE06C7D>(),
		mpi_const<uint32, 0x9B2F2F6D, 0x9C5628A7, 0x844163D0, 0x15BE8634, 0x4082AA88, 0xD95E2F9D>(),
		mpi_const<uint32, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFE, 0x26F2FC17, 0x0F69466A, 0x74DEFD8D>(),
		1,
		[](mpi &a, param(mpi) p) {
			mpi	t(7, false);
			a.grow(12);
			do {
				t.clear_elements(0, 1);
				t.copy_elements(a, 1, 6, 6);
				a.clear_elements(6, 6);
				a += t;
				t = (t >> 32) * 4553;
				a += t;
			} while (a > p);
		}
	),

	named<EC_curve>("secp192r1",prime192v1),

	named<EC_curve>(
		"secp224k1",
		EC_curve::SECP_K1,
		mpi_const<uint32, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFE, 0xFFFFE56D>(),
		zero,
		mpi_const1<5>(),
		mpi_const<uint32, 0xA1455B33, 0x4DF099DF, 0x30FC28A1, 0x69A467E9, 0xE47075A9, 0x0F7E650E, 0xB6B7A45C>(),
		mpi_const<uint32, 0x7E089FED, 0x7FBA3442, 0x82CAFBD6, 0xF7E319F7, 0xC0B0BD59, 0xE2CA4BDB, 0x556D61A5>(),
		mpi_const<uint32, 0x00000001, 0x00000000, 0x00000000, 0x00000000, 0x0001DCE8, 0xD2EC6184, 0xCAF0A971, 0x769FB1F7>(),
		1,
		[](mpi &a, param(mpi) p) {
			mpi	t(8, false);
			a.grow(14);
			do {
				t.clear_elements(0, 1);
				t.copy_elements(a, 1, 7, 7);
				a.clear_elements(7, 7);
				a += t;
				t = (t >> 32) * 6803;
				a += t;
			} while (a > p);
		}
	),

	named<EC_curve>(
		"secp224r1",
		EC_curve::SECP_R1,
		mpi_const<uint32, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000001>(),
		mpi_const<uint32, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFE, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFE>(),
		mpi_const<uint32, 0xB4050A85, 0x0C04B3AB, 0xF5413256, 0x5044B0B7, 0xD7BFD8BA, 0x270B3943, 0x2355FFB4>(),
		mpi_const<uint32, 0xB70E0CBD, 0x6BB4BF7F, 0x321390B9, 0x4A03C1D3, 0x56C21122, 0x343280D6, 0x115C1D21>(),
		mpi_const<uint32, 0xBD376388, 0xB5F723FB, 0x4C22DFE6, 0xCD4375A0, 0x5A074764, 0x44D58199, 0x85007E34>(),
		mpi_const<uint32, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFF16A2, 0xE0B8F03E, 0x13DD2945, 0x5C5C2A3D>(),
		1,
		[](mpi &a, param(mpi) p) {
			mpi	s(7, false);
			mpi	t(7, false);
			a.grow(14);
			t.copy_elements(a, 0, 0, 7);
			s.clear_elements(0, 3);
			s.copy_elements(a, 3, 7, 4);
			t = t + s;
			s.clear_elements(0, 3);
			s.copy_elements(a, 3, 11, 3);
			s.clear_elements(6, 1);
			t = t + s;
			s.copy_elements(a, 0, 7, 7);
			t = t - s;
			s.copy_elements(a, 0, 11, 3);
			s.clear_elements(3, 4);
			t = t - s;
			while (t >= p)
				t = t - p;
			while (t < 0)
				t = t + p;
			a = t;
		}
	),

	named<EC_curve>(
		"secp256k1",
		EC_curve::SECP_K1,
		mpi_const<uint32, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFE, 0xFFFFFC2F>(),
		zero,
		mpi_const1<7>(),
		mpi_const<uint32, 0x79BE667E, 0xF9DCBBAC, 0x55A06295, 0xCE870B07, 0x029BFCDB, 0x2DCE28D9, 0x59F2815B, 0x16F81798>(),
		mpi_const<uint32, 0x483ADA77, 0x26A3C465, 0x5DA4FBFC, 0x0E1108A8, 0xFD17B448, 0xA6855419, 0x9C47D08F, 0xFB10D4B8>(),
		mpi_const<uint32, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFE, 0xBAAEDCE6, 0xAF48A03B, 0xBFD25E8C, 0xD0364141>(),
		1,
		[](mpi &a, param(mpi) p) {
			mpi	t(9, false);
			a.grow(16);
			do {
				t.clear_elements(0, 1);
				t.copy_elements(a, 1, 8, 8);
				a.clear_elements(8, 8);
				a += t;
				t = (t >> 32) * 977;
				a += t;
			} while (a > p);
		}
	),

	named<EC_curve>("secp256r1", prime256v1),
	named<EC_curve>("secp384r1", prime384v1),
	named<EC_curve>("secp521r1", prime512v1),
	named<EC_curve>("prime192v1", prime192v1),
	named<EC_curve>("prime256v1", prime256v1),
	named<EC_curve>("prime384v1", prime384v1),
	named<EC_curve>("prime512v1", prime512v1),

	named<EC_curve>(
		"brainpoolP160r1",
		EC_curve::BRAINPOOLP_R1,
		mpi_const<uint32, 0xE95E4A5F, 0x737059DC, 0x60DFC7AD, 0x95B3D813, 0x9515620F>(),
		mpi_const<uint32, 0x340E7BE2, 0xA280EB74, 0xE2BE61BA, 0xDA745D97, 0xE8F7C300>(),
		mpi_const<uint32, 0x1E589A85, 0x95423412, 0x134FAA2D, 0xBDEC95C8, 0xD8675E58>(),
		mpi_const<uint32, 0xBED5AF16, 0xEA3F6A4F, 0x62938C46, 0x31EB5AF7, 0xBDBCDBC3>(),
		mpi_const<uint32, 0x1667CB47, 0x7A1A8EC3, 0x38F94741, 0x669C9763, 0x16DA6321>(),
		mpi_const<uint32, 0xE95E4A5F, 0x737059DC, 0x60DF5991, 0xD4502940, 0x9E60FC09>(),
		1
	),

	named<EC_curve>(
		"brainpoolP192r1",
		EC_curve::BRAINPOOLP_R1,
		mpi_const<uint32, 0xC302F41D, 0x932A36CD, 0xA7A34630, 0x93D18DB7, 0x8FCE476D, 0xE1A86297>(),
		mpi_const<uint32, 0x6A911740, 0x76B1E0E1, 0x9C39C031, 0xFE8685C1, 0xCAE040E5, 0xC69A28EF>(),
		mpi_const<uint32, 0x469A28EF, 0x7C28CCA3, 0xDC721D04, 0x4F4496BC, 0xCA7EF414, 0x6FBF25C9>(),
		mpi_const<uint32, 0xC0A0647E, 0xAAB6A487, 0x53B033C5, 0x6CB0F090, 0x0A2F5C48, 0x53375FD6>(),
		mpi_const<uint32, 0x14B69086, 0x6ABD5BB8, 0x8B5F4828, 0xC1490002, 0xE6773FA2, 0xFA299B8F>(),
		mpi_const<uint32, 0xC302F41D, 0x932A36CD, 0xA7A3462F, 0x9E9E916B, 0x5BE8F102, 0x9AC4ACC1>(),
		1
	),

	named<EC_curve>(
		"brainpoolP224r1",
		EC_curve::BRAINPOOLP_R1,
		mpi_const<uint32, 0xD7C134AA, 0x26436686, 0x2A183025, 0x75D1D787, 0xB09F0757, 0x97DA89F5, 0x7EC8C0FF>(),
		mpi_const<uint32, 0x68A5E62C, 0xA9CE6C1C, 0x299803A6, 0xC1530B51, 0x4E182AD8, 0xB0042A59, 0xCAD29F43>(),
		mpi_const<uint32, 0x2580F63C, 0xCFE44138, 0x870713B1, 0xA92369E3, 0x3E2135D2, 0x66DBB372, 0x386C400B>(),
		mpi_const<uint32, 0x0D9029AD, 0x2C7E5CF4, 0x340823B2, 0xA87DC68C, 0x9E4CE317, 0x4C1E6EFD, 0xEE12C07D>(),
		mpi_const<uint32, 0x58AA56F7, 0x72C0726F, 0x24C6B89E, 0x4ECDAC24, 0x354B9E99, 0xCAA3F6D3, 0x761402CD>(),
		mpi_const<uint32, 0xD7C134AA, 0x26436686, 0x2A183025, 0x75D0FB98, 0xD116BC4B, 0x6DDEBCA3, 0xA5A7939F>(),
		1
	),

	named<EC_curve>(
		"brainpoolP256r1",
		EC_curve::BRAINPOOLP_R1,
		mpi_const<uint32, 0xA9FB57DB, 0xA1EEA9BC, 0x3E660A90, 0x9D838D72, 0x6E3BF623, 0xD5262028, 0x2013481D, 0x1F6E5377>(),
		mpi_const<uint32, 0x7D5A0975, 0xFC2C3057, 0xEEF67530, 0x417AFFE7, 0xFB8055C1, 0x26DC5C6C, 0xE94A4B44, 0xF330B5D9>(),
		mpi_const<uint32, 0x26DC5C6C, 0xE94A4B44, 0xF330B5D9, 0xBBD77CBF, 0x95841629, 0x5CF7E1CE, 0x6BCCDC18, 0xFF8C07B6>(),
		mpi_const<uint32, 0x8BD2AEB9, 0xCB7E57CB, 0x2C4B482F, 0xFC81B7AF, 0xB9DE27E1, 0xE3BD23C2, 0x3A4453BD, 0x9ACE3262>(),
		mpi_const<uint32, 0x547EF835, 0xC3DAC4FD, 0x97F8461A, 0x14611DC9, 0xC2774513, 0x2DED8E54, 0x5C1D54C7, 0x2F046997>(),
		mpi_const<uint32, 0xA9FB57DB, 0xA1EEA9BC, 0x3E660A90, 0x9D838D71, 0x8C397AA3, 0xB561A6F7, 0x901E0E82, 0x974856A7>(),
		1
	),

	named<EC_curve>(
		"brainpoolP320r1",
		EC_curve::BRAINPOOLP_R1,
		mpi_const<uint32, 0xD35E4720, 0x36BC4FB7, 0xE13C785E, 0xD201E065, 0xF98FCFA6, 0xF6F40DEF, 0x4F92B9EC, 0x7893EC28, 0xFCD412B1, 0xF1B32E27>(),
		mpi_const<uint32, 0x3EE30B56, 0x8FBAB0F8, 0x83CCEBD4, 0x6D3F3BB8, 0xA2A73513, 0xF5EB79DA, 0x66190EB0, 0x85FFA9F4, 0x92F375A9, 0x7D860EB4>(),
		mpi_const<uint32, 0x52088394, 0x9DFDBC42, 0xD3AD1986, 0x40688A6F, 0xE13F4134, 0x9554B49A, 0xCC31DCCD, 0x88453981, 0x6F5EB4AC, 0x8FB1F1A6>(),
		mpi_const<uint32, 0x43BD7E9A, 0xFB53D8B8, 0x5289BCC4, 0x8EE5BFE6, 0xF20137D1, 0x0A087EB6, 0xE7871E2A, 0x10A599C7, 0x10AF8D0D, 0x39E20611>(),
		mpi_const<uint32, 0x14FDD055, 0x45EC1CC8, 0xAB409324, 0x7F77275E, 0x0743FFED, 0x117182EA, 0xA9C77877, 0xAAAC6AC7, 0xD35245D1, 0x692E8EE1>(),
		mpi_const<uint32, 0xD35E4720, 0x36BC4FB7, 0xE13C785E, 0xD201E065, 0xF98FCFA5, 0xB68F12A3, 0x2D482EC7, 0xEE8658E9, 0x8691555B, 0x44C59311>(),
		1
	),

	named<EC_curve>(
		"brainpoolP384r1",
		EC_curve::BRAINPOOLP_R1,
		mpi_const<uint32, 0x8CB91E82, 0xA3386D28, 0x0F5D6F7E, 0x50E641DF, 0x152F7109, 0xED5456B4, 0x12B1DA19, 0x7FB71123, 0xACD3A729, 0x901D1A71, 0x87470013, 0x3107EC53>(),
		mpi_const<uint32, 0x7BC382C6, 0x3D8C150C, 0x3C72080A, 0xCE05AFA0, 0xC2BEA28E, 0x4FB22787, 0x139165EF, 0xBA91F90F, 0x8AA5814A, 0x503AD4EB, 0x04A8C7DD, 0x22CE2826>(),
		mpi_const<uint32, 0x04A8C7DD, 0x22CE2826, 0x8B39B554, 0x16F0447C, 0x2FB77DE1, 0x07DCD2A6, 0x2E880EA5, 0x3EEB62D5, 0x7CB43902, 0x95DBC994, 0x3AB78696, 0xFA504C11>(),
		mpi_const<uint32, 0x1D1C64F0, 0x68CF45FF, 0xA2A63A81, 0xB7C13F6B, 0x8847A3E7, 0x7EF14FE3, 0xDB7FCAFE, 0x0CBD10E8, 0xE826E034, 0x36D646AA, 0xEF87B2E2, 0x47D4AF1E>(),
		mpi_const<uint32, 0x8ABE1D75, 0x20F9C2A4, 0x5CB1EB8E, 0x95CFD552, 0x62B70B29, 0xFEEC5864, 0xE19C054F, 0xF9912928, 0x0E464621, 0x77918111, 0x42820341, 0x263C5315>(),
		mpi_const<uint32, 0x8CB91E82, 0xA3386D28, 0x0F5D6F7E, 0x50E641DF, 0x152F7109, 0xED5456B3, 0x1F166E6C, 0xAC0425A7, 0xCF3AB6AF, 0x6B7FC310, 0x3B883202, 0xE9046565>(),
		1
	),

	named<EC_curve>(
		"brainpoolP512r1",
		EC_curve::BRAINPOOLP_R1,
		mpi_const<uint32, 0xAADD9DB8, 0xDBE9C48B, 0x3FD4E6AE, 0x33C9FC07, 0xCB308DB3, 0xB3C9D20E, 0xD6639CCA, 0x70330871, 0x7D4D9B00, 0x9BC66842, 0xAECDA12A, 0xE6A380E6, 0x2881FF2F, 0x2D82C685, 0x28AA6056, 0x583A48F3>(),
		mpi_const<uint32, 0x7830A331, 0x8B603B89, 0xE2327145, 0xAC234CC5, 0x94CBDD8D, 0x3DF91610, 0xA83441CA, 0xEA9863BC, 0x2DED5D5A, 0xA8253AA1, 0x0A2EF1C9, 0x8B9AC8B5, 0x7F1117A7, 0x2BF2C7B9, 0xE7C1AC4D, 0x77FC94CA>(),
		mpi_const<uint32, 0x3DF91610, 0xA83441CA, 0xEA9863BC, 0x2DED5D5A, 0xA8253AA1, 0x0A2EF1C9, 0x8B9AC8B5, 0x7F1117A7, 0x2BF2C7B9, 0xE7C1AC4D, 0x77FC94CA, 0xDC083E67, 0x984050B7, 0x5EBAE5DD, 0x2809BD63, 0x8016F723>(),
		mpi_const<uint32, 0x81AEE4BD, 0xD82ED964, 0x5A21322E, 0x9C4C6A93, 0x85ED9F70, 0xB5D916C1, 0xB43B62EE, 0xF4D0098E, 0xFF3B1F78, 0xE2D0D48D, 0x50D1687B, 0x93B97D5F, 0x7C6D5047, 0x406A5E68, 0x8B352209, 0xBCB9F822>(),
		mpi_const<uint32, 0x7DDE385D, 0x566332EC, 0xC0EABFA9, 0xCF7822FD, 0xF209F700, 0x24A57B1A, 0xA000C55B, 0x881F8111, 0xB2DCDE49, 0x4A5F485E, 0x5BCA4BD8, 0x8A2763AE, 0xD1CA2B2F, 0xA8F05406, 0x78CD1E0F, 0x3AD80892>(),
		mpi_const<uint32, 0xAADD9DB8, 0xDBE9C48B, 0x3FD4E6AE, 0x33C9FC07, 0xCB308DB3, 0xB3C9D20E, 0xD6639CCA, 0x70330870, 0x553E5C41, 0x4CA92619, 0x41866119, 0x7FAC1047, 0x1DB1D381, 0x085DDADD, 0xB5879682, 0x9CA90069>(),
		1
	),

	};
	for (auto &i : curves) {
		if (i == name)
			return &i;
	}

	return 0;
}

EC_point EC_curve::load(const uint8 *data, size_t length) const {
	int	k = p.num_bytes();
	switch (data[0]) {
		case 0:
			ISO_ASSERT(length == 1);
			return EC_point();
		case 2:
		case 3: {
			ISO_ASSERT(length == k + 1);
			mpi	x	= mpi(data + 1, k);
			mpi y2	= add(add(mul(square(x), x), mul(a, x)), b);
			mpi	y	= sqrt(y2);
			return EC_point(x, y);
		}
		case 4:
			// uncompressed
			ISO_ASSERT(length == k * 2 + 1);
			return EC_point(mpi(data + 1, k), mpi(data + k + 1, k));
		default:
			return EC_point();
	}
}

int EC_curve::save(const EC_point &a, uint8 *data, bool compress) const {
	if (!a.z) {
		if (data)
			*data = 0;
		return 1;
	}

	int	k = p.num_bytes();
	if (compress) {
		if (data) {
			mpi	r = mul(inv_mod(a.x, q), a.y);
			data[0] = 2 + r.is_odd();
			a.x.save(data + 1, k);
		}
		return k + 1;
	} else {
		if (data) {
			data[0] = 0x04;
			a.x.save(data + 1, k);
			a.y.save(data + k + 1, k);
		}
		return k * 2 + 1;
	}
}

EC_point EC_curve::affinify(param(EC_point) s) const {
	mpi a	= inv_mod(s.z, p);
	mpi b	= square(a);
	return EC_point(mul(b, s.x), mul(mul(b, a), s.y));
}

bool EC_curve::is_affine(param(EC_point) s) const {
	// t1 = (Sx^3 + a * Sx + b) mod p
	mpi t1 = add(add(mul(square(s.x), s.x), mul(a, s.x)), b);
	// t2 = Sy^2
	mpi t2 = square(s.y);
	//Check whether the point is on the elliptic curve
	return t1 == t2;
}

EC_point EC_curve::twice(param(EC_point) s) const {
	//at infinity?
	if (!s.z)
		return EC_point(one, one, zero);

	mpi t1 = s.x;
	mpi t2 = s.y;
	mpi t3 = s.z;
	mpi t4;
	mpi t5;

	if (type == SECP_K1) {
		t5 = square(t1);						// t5 = t1^2
		t4 = add(twice(t5), t5);				// t4 = 3 * t5

	} else if (type == SECP_R1) {
		t4 = square(t3);						// t4 = t3^2
		t5 = sub(t1, t4);						// t5 = t1 - t4
		t4 = add(t1, t4);						// t4 = t1 + t4
		t5 = mul(t4, t5);						// t5 = t4 * t5
		t4 = add(twice(t5), t5);				// t4 = 3 * t5

	} else {
		t4 = mul(square(square(t3)), a);		// t4 = a * t3^4
		t5 = square(t1);						// t5 = t1^2
		t4 = add(add(add(t4, t5), t5), t5);		// t4 = t4 + 3 * t5
	}

	t3 = twice(mul(t3, t2));					// t3 = 2 * t3 * t2
	t2 = square(t2);							// t2 = t2^2
	t5 = twice(twice(mul(t1, t2)));				// t5 = 4 * t1 * t2
	t1 = square(t4);							// t1 = t4^2
	t1 = sub(sub(t1, t5), t5);					// t1 = t1 - 2 * t5
	t2 = twice(twice(twice(square(t2))));		// t2 = 8 * t2^2
	t2 = sub(mul(t4, sub(t5, t1)), t2);			// t2 = t4 * (t5 - t1) - t2

	return EC_point(t1, t2, t3);
}

EC_point EC_curve::add(param(EC_point) s, param(EC_point) t) const {
	mpi t1	= s.x;
	mpi t2	= s.y;
	mpi t3	= s.z;
	mpi t4	= t.x;
	mpi t5	= t.y;
	mpi t7;

	if (t.z != 1) {
		t7 = square(t.z);
		t1 = mul(t1, t7);						// t1 *= t.z^2
		t2 = mul(t2, mul(t.z, t7));				// t2 *= t.z^3
	}

	t7 = square(t3);
	t4 = sub(t1, mul(t4, t7));					// t4 = t1 - t4 * t3^2
	t5 = sub(t2, mul(t5, mul(t3, t7)));			// t5 = t2 - t5 * t3^3

	if (!t4) {
		return !t5
			? EC_point(zero, zero, zero)
			: EC_point(one, one, zero);
	}

	t1 = sub(twice(t1), t4);					// t1 = 2 * t1 - t4
	t2 = sub(twice(t2), t5);					// t2 = 2 * t2 - t5

	if (t.z != 1)
		t3 = mul(t3, t.z);						// t3 = t3 * t.z

	t3 = mul(t3, t4);							// t3 = t3 * t4
	t7 = square(t4);							// t7 = t4^2
	t4 = mul(t4, t7);							// t4 = t4 * t7
	t7 = mul(t1, t7);							// t7 = t1 * t7

	t1 = sub(square(t5), t7);					// t1 = t5^2 - t7
	t7 = sub(t7, twice(t1));					// t7 = t7 - 2 * t1
	t2 = sub(mul(t5, t7), mul(t2, t4));			// t2 = t5 * t7 - t2 * t4

	// t2 = t2 / 2
	if (t2.is_odd())
		t2 += p;
	t2 >>= 1;
	return EC_point(t1, t2, t3);
}

EC_point EC_curve::full_add(param(EC_point) s, param(EC_point) t) const {
	if (!s.z)
		return t;

	if (!t.z)
		return s;

	EC_point	r = add(s, t);
	if (r.is_zero())
		r = twice(s);
	return r;
}

EC_point EC_curve::full_sub(param(EC_point) s, param(EC_point) t) const {
	return full_add(s, EC_point(t.x, p - t.y, t.z));
}

EC_point EC_curve::mul(param(mpi) d, param(EC_point) s) const {
	if (d == 1)
		return s;

	if (!d || !s.z)
		return EC_point(one, one, zero);

	EC_point	r = s.z == 1 ? s : projectify(affinify(s));
	mpi			h = d + d + d;

	for (int i = h.num_bits() - 2; i >= 1; --i) {
		r = twice(r);
		if (h.test_bit(i) && !d.test_bit(i))
			r = full_add(r, s);
		else if (!h.test_bit(i) && d.test_bit(i))
			r = full_sub(r, s);
	}
	return r;
}

static uint32 twin_mult_helper(uint32 t) {
	return	18 <= t && t < 22 ? 9
		:	14 <= t && t < 18 ? 10
		:	22 <= t && t < 24 ? 11
		:	4  <= t && t < 12 ? 14
		:	12;
}

EC_point EC_curve::twin_mult(param(mpi) d0, param(EC_point) s, param(mpi) d1, param(EC_point) t) const {
	EC_point spt	= full_add(s, t);
	EC_point smt	= full_sub(s, t);

	uint32	m0		= d0.num_bits();
	uint32	m1		= d1.num_bits();
	uint32	m		= max(m0, m1);

	//Let c be a 2 x 6 binary matrix
	uint32	c0	= int(d0.test_bit(m - 4))
				| (int(d0.test_bit(m - 3)) << 1)
				| (int(d0.test_bit(m - 2)) << 2)
				| (int(d0.test_bit(m - 1)) << 3);

	uint32	c1	= int(d1.test_bit(m - 4))
				| (int(d1.test_bit(m - 3)) << 1)
				| (int(d1.test_bit(m - 2)) << 2)
				| (int(d1.test_bit(m - 1)) << 3);

	//Set R = (1, 1, 0)
	EC_point	r(one, one, zero);

	for (int k = m; k >= 0; k--) {
		uint32	h0	= (c0 & 0x1F) ^ (c0 & 0x20 ? 0x1f : 0);
		uint32	h1	= (c1 & 0x1F) ^ (c1 & 0x20 ? 0x1f : 0);

		int		u0	= h0 < twin_mult_helper(h1) ? 0 : c0 & 0x20 ? -1 : 1;
		int		u1	= h1 < twin_mult_helper(h0) ? 0 : c1 & 0x20	? -1 : 1;

		r = twice(r);

		//Check u(0) and u(1)
		if (u0 || u1) {
			r	= u0 == -1 ? (u1 == -1 ? full_sub(r, spt) : u1 == 1 ? full_sub(r, smt) : full_sub(r, s))
				: u0 ==  1 ? (u1 == -1 ? full_add(r, smt) : u1 == 1 ? full_add(r, spt) : full_add(r, s))
				: u1 == -1 ? full_sub(r, t)	: full_add(r, t);
		}

		//Update c matrix
		c0 = ((c0 << 1) | int(d0.test_bit(k - 5))) ^ (u0 ? 0x20 : 0);
		c1 = ((c1 << 1) | int(d1.test_bit(k - 5))) ^ (u1 ? 0x20 : 0);
	}
	return r;
}

ECDSA_signature::ECDSA_signature(const EC_curve *curve, vrng &&rng, param(mpi) priv_key, const const_memory_block &digest) {
	uint32	n = curve->q.num_bits();
	mpi		k = mpi::random(rng, n);

	//Make sure that 0 < k < q
	if (k >= curve->q)
		k >>= 1;

	n = min(n, digest.size32() * 8);
	mpi	z = digest.slice_to((n + 7) / 8);

	//Keep the leftmost N bits of the hash value
	if (n % 8)
		z >>= 8 - (n % 8);

	EC_point	r1 = curve->affinify(curve->mul(k, curve->g));

	r = r1.x % curve->q;
	s = (priv_key * r + z) % curve->q;
	s = (s * inv_mod(k, curve->q)) % curve->q;
}

bool ECDSA_signature::verify(const EC_curve *curve, param(EC_point) pub_key, const const_memory_block &digest) const {
	if (r <= zero || r >= curve->q || s <= zero || s >= curve->q)
		return false;

	uint32	n = min(curve->q.num_bits(), digest.size32() * 8);
	mpi		z = digest.slice_to((n + 7) / 8);

	//Keep the leftmost N bits of the hash value
	if (n % 8)
		z >>= 8 - (n % 8);

	mpi	w	= inv_mod(s, curve->q);
	mpi	u1	= mul_mod(z, w, curve->q);
	mpi	u2	= mul_mod(r, w, curve->q);

	EC_point	v1	= projectify(pub_key);
	EC_point	v0	= curve->affinify(curve->twin_mult(u1, curve->g, u2, v1));

	mpi	v = v0.x % curve->q;
	return v == r;
}

bool EC_group_simple::point_get_affine_coordinates(const EC_point &point, mpi &x, mpi &y) const {
	auto	Z = vt->field_decode(this, point.z);

	if (Z == iso::one) {
		x = vt->field_decode(this, point.x);
		y = vt->field_decode(this, point.y);

	} else {
		auto	Z1 = inv_mod(Z, field);
		auto	Z2 = square_mod(Z1, field);

		x = vt->field_mul(this, point.x, Z2);
		y = vt->field_mul(this, point.y, mul_mod(Z2, Z1, field));
	}

	return true;
}

// montgomery

bool EC_group_montgomery::point_get_affine_coordinates(const EC_point &point, mpi &x, mpi &y) const {
	if (point.z == one) {
		x = mont->from(point.x);
		y = mont->from(point.y);

	} else {
		// instead of:	|BN_from_montgomery| + invert + |BN_to_montgomery|
		// This is more efficient, because |BN_from_montgomery| is more efficient (at least in theory) than |BN_to_montgomery|, since it doesn't have to do the multiplication before the reduction.
		// Use Fermat's Little Theorem with |BN_mod_exp_mont_consttime| instead of |BN_mod_inverse_odd| since this inversion may be done as the final step of private key operations.
		// Unfortunately, this is suboptimal for ECDSA verification

		mpi	Z1 = mont->exp(mont->from(mont->from(point.z)), field - 2);
		mpi Z2 = mont->square(Z1);

		// Instead of using |BN_from_montgomery| to convert the |x| coordinate and then calling |BN_from_montgomery| again to convert the |y| coordinate below, convert the common factor |Z_2| once now, saving one reduction
		Z2	= mont->from(Z2);
		x	= mont->mul(point.x, Z2);
		y	= mont->mul(point.y, mont->mul(Z2, Z1));
	}
	return true;
}
