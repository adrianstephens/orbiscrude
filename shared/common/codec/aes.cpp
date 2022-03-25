#include "aes.h"
#include "base/bits.h"

using namespace iso;

// Forward S-box & tables
static uint8	FSb[256];
static uint32	FT[4][256];

// Reverse S-box & tables
static uint8	RSb[256];
static uint32	RT[4][256];

// Round constants
static uint32 RCON[10];

// Tables generation code
static bool aes_init_done = false;

static void aes_gen_tables() {

	struct GF2_8 {
		uint8 pow[256];
		uint8 log[256];

		static inline constexpr uint8 xtime(uint8 x)	{ return ((x << 1) ^ ((x & 0x80) ? 0x1B : 0x00)); }
		inline constexpr uint8	mul(uint8 x, uint8 y)	{ return (x && y) ? pow[(log[x] + log[y]) % 255] : 0; }
		inline constexpr uint8	rcp(uint8 x)			{ return pow[255 - log[x]]; }

		GF2_8() {
			// compute pow and log tables over GF(2^8)
			for (int i = 0, x = 1; i < 256; i++) {
				pow[i] = x;
				log[x] = i;
				x ^= xtime(x);
			}
		}
	} gf;

	// calculate the round constants
	for (int i = 0, x = 1; i < 10; i++) {
		RCON[i] = (uint32)x;
		x = gf.xtime(x);
	}

	// generate the forward and reverse S-boxes
	FSb[0x00] = 0x63;
	RSb[0x63] = 0x00;

	for (int i = 1; i < 256; i++) {
		uint8	x = gf.rcp(i);
		FSb[i] = x ^ rotate_left<1>(x) ^ rotate_left<2>(x) ^ rotate_left<3>(x) ^ rotate_left<4>(x) ^ 0x63;
		RSb[x] = (uint8)i;
	}

	// generate the forward and reverse tables
	for (int i = 0; i < 256; i++) {
		uint32	x = FSb[i];
		uint32	y = gf.xtime(x);
		uint32	t = y ^ (x <<  8) ^ (x << 16) ^ ((y ^ x) << 24);

		FT[3][i] = rotate_left<8>(FT[2][i] = rotate_left<8>(FT[1][i] = rotate_left<8>(FT[0][i] = t)));

		x	=	RSb[i];
		t	=	(uint32)gf.mul(0x0E, x)
			^	((uint32)gf.mul(0x09, x) <<  8)
			^	((uint32)gf.mul(0x0D, x) << 16)
			^	((uint32)gf.mul(0x0B, x) << 24);

		RT[3][i] = rotate_left<8>(RT[2][i] = rotate_left<8>(RT[1][i] = rotate_left<8>(RT[0][i] = t)));
	}
}

inline uint32 AES_SPLIT(const uint32 T[4][256], uint32 Y0, uint32 Y1, uint32 Y2, uint32 Y3) {
	return	T[0][(Y0	  ) & 0xFF]
		^	T[1][(Y1 >> 8 ) & 0xFF]
		^	T[2][(Y2 >> 16) & 0xFF]
		^	T[3][(Y3 >> 24) & 0xFF];
}

inline uint32 AES_COMBINE0(const uint8 T[256], uint8 Y0, uint8 Y1, uint8 Y2, uint8 Y3) {
	return	((uint32)T[Y0]      )
		^	((uint32)T[Y1] <<  8)
		^	((uint32)T[Y2] << 16)
		^	((uint32)T[Y3] << 24);
}
inline uint32 AES_COMBINE1(const uint8 T[256], uint32 Y) {
	return	AES_COMBINE0(T,
		uint8(Y >>  8),
		uint8(Y >> 16),
		uint8(Y >> 24),
		uint8(Y      )
	);
}
inline uint32 AES_COMBINE(const uint8 T[256], uint32 Y0, uint32 Y1, uint32 Y2, uint32 Y3) {
	return	AES_COMBINE0(T,
		uint8(Y0      ),
		uint8(Y1 >>  8),
		uint8(Y2 >> 16),
		uint8(Y3 >> 24)
	);
}


// AES key schedule (encryption)
// param ctx		AES context to be initialized
// param key		encryption key
// param keysize	must be 128, 192 or 256
bool AES::setkey_enc(const uint8 *key, int keysize) {
	if (!aes_init_done) {
		aes_gen_tables();
		aes_init_done = true;
	}

	uint32 *RK = rk;
	for (int i = 0; i < (keysize >> 5); i++)
		RK[i] = ((packed<uint32le>*)key)[i];

	switch (keysize) {
		case 128:
			nr = 10;
			for (int i = 0; i < 10; i++, RK += 4) {
				RK[4] = RK[0] ^ RCON[i] ^ AES_COMBINE1(FSb, RK[3]);
				RK[5] = RK[1] ^ RK[4];
				RK[6] = RK[2] ^ RK[5];
				RK[7] = RK[3] ^ RK[6];
			}
			break;

		case 192:
			nr = 12;
			for (int i = 0; i < 8; i++, RK += 6) {
				RK[6]  = RK[0] ^ RCON[i] ^ AES_COMBINE1(FSb, RK[5]);
				RK[7]  = RK[1] ^ RK[6];
				RK[8]  = RK[2] ^ RK[7];
				RK[9]  = RK[3] ^ RK[8];
				RK[10] = RK[4] ^ RK[9];
				RK[11] = RK[5] ^ RK[10];
			}
			break;

		case 256:
			nr = 14;
			for (int i = 0; i < 7; i++, RK += 8) {
				RK[8]  = RK[0] ^ RCON[i] ^ AES_COMBINE1(FSb, RK[7]);
				RK[9]  = RK[1] ^ RK[8];
				RK[10] = RK[2] ^ RK[9];
				RK[11] = RK[3] ^ RK[10];

				RK[12] = RK[4] ^ AES_COMBINE1(FSb, RK[11]);
				RK[13] = RK[5] ^ RK[12];
				RK[14] = RK[6] ^ RK[13];
				RK[15] = RK[7] ^ RK[14];
			}
			break;

		default:
			return false;
	}
	return true;
}

// AES key schedule (decryption)
// param ctx      AES context to be initialized
// param key      decryption key
// param keysize  must be 128, 192 or 256
bool AES::setkey_dec(const uint8 *key, int keysize) {
	switch (keysize) {
		case 128: nr = 10; break;
		case 192: nr = 12; break;
		case 256: nr = 14; break;
		default : return false;
	}

	AES cty;
	if (!cty.setkey_enc(key, keysize))
		return false;

	uint32	*RK = rk;
	uint32	*SK = cty.rk + cty.nr * 4;

	*RK++ = *SK++;
	*RK++ = *SK++;
	*RK++ = *SK++;
	*RK++ = *SK++;

	SK -= 8;
	for (int i = nr; i--; SK -= 8) {
		for (int j = 0; j < 4; j++, SK++) {
			*RK++ = RT[0][FSb[(*SK      ) & 0xFF]] ^
					RT[1][FSb[(*SK >>  8) & 0xFF]] ^
					RT[2][FSb[(*SK >> 16) & 0xFF]] ^
					RT[3][FSb[(*SK >> 24) & 0xFF]];
		}
	}

	*RK++ = *SK++;
	*RK++ = *SK++;
	*RK++ = *SK++;
	*RK++ = *SK++;

	clear(cty);
	return true;
}

// AES-ECB block encryption/decryption
void AES::encrypt(const block &in, block &out) {
	uint32	*RK	= rk;
	uint32	X0	= in[0] ^ *RK++;
	uint32	X1	= in[1] ^ *RK++;
	uint32	X2	= in[2] ^ *RK++;
	uint32	X3	= in[3] ^ *RK++;

	for (int i = (nr >> 1) - 1; i > 0; i--) {
		uint32	Y0 = *RK++ ^ AES_SPLIT(FT, X0, X1, X2, X3);
		uint32	Y1 = *RK++ ^ AES_SPLIT(FT, X1, X2, X3, X0);
		uint32	Y2 = *RK++ ^ AES_SPLIT(FT, X2, X3, X0, X1);
		uint32	Y3 = *RK++ ^ AES_SPLIT(FT, X3, X0, X1, X2);

		X0 = *RK++ ^ AES_SPLIT(FT, Y0, Y1, Y2, Y3);
		X1 = *RK++ ^ AES_SPLIT(FT, Y1, Y2, Y3, Y0);
		X2 = *RK++ ^ AES_SPLIT(FT, Y2, Y3, Y0, Y1);
		X3 = *RK++ ^ AES_SPLIT(FT, Y3, Y0, Y1, Y2);
	}

	uint32	Y0 = *RK++ ^ AES_SPLIT(FT, X0, X1, X2, X3);
	uint32	Y1 = *RK++ ^ AES_SPLIT(FT, X1, X2, X3, X0);
	uint32	Y2 = *RK++ ^ AES_SPLIT(FT, X2, X3, X0, X1);
	uint32	Y3 = *RK++ ^ AES_SPLIT(FT, X3, X0, X1, X2);

	out[0] = *RK++ ^ AES_COMBINE(FSb, Y0, Y1, Y2, Y3);
	out[1] = *RK++ ^ AES_COMBINE(FSb, Y1, Y2, Y3, Y0);
	out[2] = *RK++ ^ AES_COMBINE(FSb, Y2, Y3, Y0, Y1);
	out[3] = *RK++ ^ AES_COMBINE(FSb, Y3, Y0, Y1, Y2);
}

void AES::decrypt(const block &in, block &out) {
	uint32	*RK	= rk;
	uint32	X0	= in[0] ^ *RK++;
	uint32	X1	= in[1] ^ *RK++;
	uint32	X2	= in[2] ^ *RK++;
	uint32	X3	= in[3] ^ *RK++;

	for (int i = (nr >> 1) - 1; i > 0; i--) {
		uint32	Y0 = *RK++ ^ AES_SPLIT(RT, X0, X3, X2, X1);
		uint32	Y1 = *RK++ ^ AES_SPLIT(RT, X1, X0, X3, X2);
		uint32	Y2 = *RK++ ^ AES_SPLIT(RT, X2, X1, X0, X3);
		uint32	Y3 = *RK++ ^ AES_SPLIT(RT, X3, X2, X1, X0);

		X0 = *RK++ ^ AES_SPLIT(RT, Y0, Y3, Y2, Y1);
		X1 = *RK++ ^ AES_SPLIT(RT, Y1, Y0, Y3, Y2);
		X2 = *RK++ ^ AES_SPLIT(RT, Y2, Y1, Y0, Y3);
		X3 = *RK++ ^ AES_SPLIT(RT, Y3, Y2, Y1, Y0);
	}

	uint32	Y0 = *RK++ ^ AES_SPLIT(RT, X0, X3, X2, X1);
	uint32	Y1 = *RK++ ^ AES_SPLIT(RT, X1, X0, X3, X2);
	uint32	Y2 = *RK++ ^ AES_SPLIT(RT, X2, X1, X0, X3);
	uint32	Y3 = *RK++ ^ AES_SPLIT(RT, X3, X2, X1, X0);

	out[0] = *RK++ ^ AES_COMBINE(RSb, Y0, Y3, Y2, Y1);
	out[1] = *RK++ ^ AES_COMBINE(RSb, Y1, Y0, Y3, Y2);
	out[2] = *RK++ ^ AES_COMBINE(RSb, Y2, Y1, Y0, Y3);
	out[3] = *RK++ ^ AES_COMBINE(RSb, Y3, Y2, Y1, Y0);
}
