#include "gost.h"

using namespace iso;

// Add by mod 2^32 - 1:
static uint32 ADD_MOD32_1(uint32 x, uint32 y) {
	uint32 res = x + y;
	if (res < x || res < y)
		++res;
	return res;
}

//-----------------------------------------------------------------------------
//	GOST28147	(aka GOST89)
//-----------------------------------------------------------------------------

// Full encrypt procedure
void gost28147::State::ALLe(const uint32 *key, uint32 Nh, uint32 Nl, uint32 *Nh_out, uint32 *Nl_out) const {
	Nl = STEP(key[0], Nh, Nl);
	Nh = STEP(key[1], Nl, Nh);
	Nl = STEP(key[2], Nh, Nl);
	Nh = STEP(key[3], Nl, Nh);
	Nl = STEP(key[4], Nh, Nl);
	Nh = STEP(key[5], Nl, Nh);
	Nl = STEP(key[6], Nh, Nl);
	Nh = STEP(key[7], Nl, Nh);
	Nl = STEP(key[0], Nh, Nl);
	Nh = STEP(key[1], Nl, Nh);
	Nl = STEP(key[2], Nh, Nl);
	Nh = STEP(key[3], Nl, Nh);
	Nl = STEP(key[4], Nh, Nl);
	Nh = STEP(key[5], Nl, Nh);
	Nl = STEP(key[6], Nh, Nl);
	Nh = STEP(key[7], Nl, Nh);
	Nl = STEP(key[0], Nh, Nl);
	Nh = STEP(key[1], Nl, Nh);
	Nl = STEP(key[2], Nh, Nl);
	Nh = STEP(key[3], Nl, Nh);
	Nl = STEP(key[4], Nh, Nl);
	Nh = STEP(key[5], Nl, Nh);
	Nl = STEP(key[6], Nh, Nl);
	Nh = STEP(key[7], Nl, Nh);
	Nl = STEP(key[7], Nh, Nl);
	Nh = STEP(key[6], Nl, Nh);
	Nl = STEP(key[5], Nh, Nl);
	Nh = STEP(key[4], Nl, Nh);
	Nl = STEP(key[3], Nh, Nl);
	Nh = STEP(key[2], Nl, Nh);
	Nl = STEP(key[1], Nh, Nl);
	Nh = STEP(key[0], Nl, Nh);

	*Nh_out = Nh;
	*Nl_out = Nl;
}

// Full decrypt procedure
void gost28147::State::ALLd(const uint32 *key, uint32 Nh, uint32 Nl, uint32 *Nh_out, uint32 *Nl_out) const {
	Nl = STEP(key[0], Nh, Nl);
	Nh = STEP(key[1], Nl, Nh);
	Nl = STEP(key[2], Nh, Nl);
	Nh = STEP(key[3], Nl, Nh);
	Nl = STEP(key[4], Nh, Nl);
	Nh = STEP(key[5], Nl, Nh);
	Nl = STEP(key[6], Nh, Nl);
	Nh = STEP(key[7], Nl, Nh);
	Nl = STEP(key[7], Nh, Nl);
	Nh = STEP(key[6], Nl, Nh);
	Nl = STEP(key[5], Nh, Nl);
	Nh = STEP(key[4], Nl, Nh);
	Nl = STEP(key[3], Nh, Nl);
	Nh = STEP(key[2], Nl, Nh);
	Nl = STEP(key[1], Nh, Nl);
	Nh = STEP(key[0], Nl, Nh);
	Nl = STEP(key[7], Nh, Nl);
	Nh = STEP(key[6], Nl, Nh);
	Nl = STEP(key[5], Nh, Nl);
	Nh = STEP(key[4], Nl, Nh);
	Nl = STEP(key[3], Nh, Nl);
	Nh = STEP(key[2], Nl, Nh);
	Nl = STEP(key[1], Nh, Nl);
	Nh = STEP(key[0], Nl, Nh);
	Nl = STEP(key[7], Nh, Nl);
	Nh = STEP(key[6], Nl, Nh);
	Nl = STEP(key[5], Nh, Nl);
	Nh = STEP(key[4], Nl, Nh);
	Nl = STEP(key[3], Nh, Nl);
	Nh = STEP(key[2], Nl, Nh);
	Nl = STEP(key[1], Nh, Nl);
	Nh = STEP(key[0], Nl, Nh);

	*Nh_out = Nh;
	*Nl_out = Nl;
}

void gost28147::State::ALLmac(const uint32 *key, uint32 Nh, uint32 Nl, uint32 *Nh_out, uint32 *Nl_out) const {
	Nl = STEP(key[0], Nh, Nl);
	Nh = STEP(key[1], Nl, Nh);
	Nl = STEP(key[2], Nh, Nl);
	Nh = STEP(key[3], Nl, Nh);
	Nl = STEP(key[4], Nh, Nl);
	Nh = STEP(key[5], Nl, Nh);
	Nl = STEP(key[6], Nh, Nl);
	Nh = STEP(key[7], Nl, Nh);
	Nl = STEP(key[0], Nh, Nl);
	Nh = STEP(key[1], Nl, Nh);
	Nl = STEP(key[2], Nh, Nl);
	Nh = STEP(key[3], Nl, Nh);
	Nl = STEP(key[4], Nh, Nl);
	Nh = STEP(key[5], Nl, Nh);
	Nl = STEP(key[6], Nh, Nl);
	Nh = STEP(key[7], Nl, Nh);

	*Nh_out = Nh;
	*Nl_out = Nl;
}

void gost28147::State::init(const uint8 *params) {
	clear(*this);
	for (int j = 0; j < 256; j++) {
		// Transformation of byte in position i
		int	bh = (j >> 4) & 0x0F;
		int	bl = j & 0x0f;
		sbox0[j] = (((params[bh * 4 + 3] << 4) | (params[bl * 4 + 3] >> 4)) & 0xFF) << 24;
		sbox1[j] = (((params[bh * 4 + 2] << 4) | (params[bl * 4 + 2] >> 4)) & 0xFF) << 16;
		sbox2[j] = (((params[bh * 4 + 1] << 4) | (params[bl * 4 + 1] >> 4)) & 0xFF) << 8;
		sbox3[j] = (((params[bh * 4 + 0] << 4) | (params[bl * 4 + 0] >> 4)) & 0xFF) << 0;

		sbox0[j] = rotate_left(sbox0[j], 11);
		sbox1[j] = rotate_left(sbox1[j], 11);
		sbox2[j] = rotate_left(sbox2[j], 11);
		sbox3[j] = rotate_left(sbox3[j], 11);
	}
}

void gost28147_gamma::update() {
	static const uint32 C1 = 0x01010104;
	static const uint32 C2 = 0x01010101;

	N1 += C2;
	N2 = ADD_MOD32_1(N2, C1);

	uint32le N[2] = {N1, N2};
	state.encrypt(key, (uint8*)N, gamma.begin(), 8);
	used = 0;
}

void gost28147_gamma::do_gamma(const uint8 *data, uint8 *r, size_t size) {
	if (used < 0) {
		uint32le buf[2];
		state.encrypt(key, gamma.begin(), (uint8*)buf, 8);
		N1 = buf[0];
		N2 = buf[1];
		used = 8;
	}

	for (int i = 0; i < size; i++) {
		if (used >= 8)
			update();
		r[i] = data[i] ^ gamma[used];
		++used;
	}
}

void gost28147_gamma::encrypt(const uint8 *data, uint8 *r, size_t size) {
	if (used < 0)
		used = 8;

	for (int i = 0; i < size; i++) {
		if (used >= 8) {
			uint8	buf[8];
			state.encrypt(key, gamma.begin(), buf, 8);
			memcpy(gamma.begin(), buf, 8);
			used = 0;
		}
		r[i] = data[i] ^ gamma[used];
		gamma[used] = r[i];
		++used;
	}
}

void gost28147_gamma::decrypt(const uint8 *data, uint8 *r, size_t size) {
	if (used < 0)
		used = 8;

	for (int i = 0; i < size; i++) {
		if (used >= 8) {
			uint8	buf[8];
			state.encrypt(key, gamma.begin(), buf, 8);
			memcpy(gamma.begin(), buf, 8);
			used = 0;
		}
		uint8	tmp	= data[i];
		r[i]		= data[i] ^ gamma[used];
		gamma[used] = tmp;
		++used;
	}
}

//-----------------------------------------------------------------------------
//	GOST3411	(AKA GOST94)
//-----------------------------------------------------------------------------

// A(y4 || y3 || y2 || y1) = (y1 ^ y2) || y4 || y3 || y2
static void A(gostr3411::hash res, const gostr3411::hash y) {
	for (int i = 0; i < 8; i++) {
		res[i] = y[i + 8];
		res[i + 8] = y[i + 16];
		res[i + 16] = y[i + 24];
		res[i + 24] = y[i] ^ y[i + 8];
	}
}

static gostr3411::hash C2 = {
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0
};

static gostr3411::hash C3 = {
	0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff,
	0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00,
	0x00, 0xff, 0xff, 0x00, 0xff, 0x00, 0x00, 0xff,
	0xff, 0x00, 0x00, 0x00, 0xff, 0xff, 0x00, 0xff
};

static gostr3411::hash C4 = {
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0
};

// phi(i + 1 + 4*(k - 1)) = 8*i + k, i=0,1,2,3; k=1...8.
static int phi[] = {
	0, 8, 16, 24, 1, 9, 17, 25,
	2, 10, 18, 26, 3, 11, 19, 27,
	4, 12, 20, 28, 5, 13, 21, 29,
	6, 14, 22, 30, 7, 15, 23, 31
};

static void P(gostr3411::hash res, const gostr3411::hash y) {
	for (int i = 0; i < gostr3411::BLOCK_SIZE; i++)
		res[i] = y[phi[i]];
}

static void psi(gostr3411::hash r, const gostr3411::hash a) {
	uint8 h1 = a[0] ^ a[2] ^ a[4] ^ a[6] ^ a[24] ^ a[30];
	uint8 h2 = a[1] ^ a[3] ^ a[5] ^ a[7] ^ a[25] ^ a[31];

	for (int i = 0; i < 30; i++)
		r[i] = a[i + 2];

	r[30] = h1;
	r[31] = h2;
}

// Add 2 256-bit integers by 2^256 module
static void hash_add(gostr3411::hash a, const gostr3411::hash b) {
	uint32	r = 0;
	for (int i = 0; i < gostr3411::BLOCK_SIZE; i++) {
		uint32	A = a[i], B = b[i];
		r	+= A + B;
		a[i] = r % 256;
		r	/= 256;
	}
}

void gostr3411::update(const hash data) {
	hash W, U, V, T, S;
	hash K1, K2, K3, K4;

	// U = Hin
	// V = m
	// W = U ^ V
	for (int i = 0; i < BLOCK_SIZE; i++) {
		U[i] = H[i];
		V[i] = data[i];
		W[i] = U[i] ^ V[i];
	}

	// K1 = P(W)
	P(K1, W);

	A(T, U);
	for (int i = 0; i < BLOCK_SIZE; i++)
		U[i] = T[i] ^ C2[i];
	A(T, V); A(V, T);
	for (int i = 0; i < BLOCK_SIZE; i++)
		W[i] = U[i] ^ V[i];
	P(K2, W);

	A(T, U);
	for (int i = 0; i < BLOCK_SIZE; i++)
		U[i] = T[i] ^ C3[i];
	A(T, V); A(V, T);
	for (int i = 0; i < BLOCK_SIZE; i++)
		W[i] = U[i] ^ V[i];
	P(K3, W);

	A(T, U);
	for (int i = 0; i < BLOCK_SIZE; i++)
		U[i] = T[i] ^ C4[i];
	A(T, V); A(V, T);
	for (int i = 0; i < BLOCK_SIZE; i++)
		W[i] = U[i] ^ V[i];
	P(K4, W);

	state.encrypt(K1, H, S, 8);
	state.encrypt(K2, H + 8, S + 8, 8);
	state.encrypt(K3, H + 16, S + 16, 8);
	state.encrypt(K4, H + 24, S + 24, 8);

	// Hout = psi^61(Hin ^ psi(m^psi^12(S)))
	// psi^12(S):
	for (int i = 0; i < 6; i++) {
		psi(T, S);
		psi(S, T);
	}

	// m ^ psi^12(S):
	for (int i = 0; i < BLOCK_SIZE; i++)
		S[i] ^= data[i];

	// psi(m ^ psi^12(S)):
	psi(T, S);

	// Hin ^ psi(m^psi^12(S)):
	for (int i = 0; i < BLOCK_SIZE; i++)
		T[i] ^= H[i];

	// psi^60(Hin ^ psi(m^psi^12(S)))
	for (int i = 0; i < 30; i++) {
		psi(S, T);
		psi(T, S);
	}

	// Result:
	psi(H, T);
}

gostr3411::gostr3411(const gost28147::State *_state, const hash iv) {
	static uint8 default_params[] = {
		0x93, 0xEE, 0xB3, 0x1B, 0x67, 0x47, 0x5A, 0xDA, 0x3E, 0x6A, 0x1D, 0x2F, 0x29, 0x2C, 0x9C, 0x95,
		0x88, 0xBD, 0x81, 0x70, 0xBA, 0x31, 0xD2, 0xAC, 0x1F, 0xD3, 0xF0, 0x6E, 0x70, 0x89, 0x0B, 0x08,
		0xA5, 0xC0, 0xE7, 0x86, 0x42, 0xF2, 0x45, 0xC2, 0xE6, 0x5B, 0x29, 0x43, 0xFC, 0xA4, 0x34, 0x59,
		0xCB, 0x0F, 0xC8, 0xF1, 0x04, 0x78, 0x7F, 0x37, 0xDD, 0x15, 0xAE, 0xBD, 0x51, 0x96, 0x66, 0xE4
	};

	// Init Gost28147 params:
	if (_state)
		state = *_state;
	else
		state.init(default_params);

	// Set initial vector and 0 length/checksum:
	clear(H);
	clear(sum);
	clear(len);
	if (iv)
		memcpy(H, iv, sizeof(H));
}


void gostr3411::process(const hash data) {
	static const hash one = {
		0, 1, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0
	};

	hash_add(sum, data);
	hash_add(len, one);
	update(data);
}

void gostr3411::terminate() {
	if (uint32 o = p % BLOCK_SIZE) {
		len[0] = 8 * o;
		memset(block + o, 0, BLOCK_SIZE - o);
		hash_add(sum, block);
		update(block);
	}
	update(len);
	update(sum);
}

//-----------------------------------------------------------------------------
//	GOST_RANDOM
//-----------------------------------------------------------------------------

const uint8 gost_random::init_params[] = {
	0x4E, 0x57, 0x64, 0xD1, 0xAB, 0x8D, 0xCB, 0xBF, 0x94, 0x1A, 0x7A, 0x4D, 0x2C, 0xD1, 0x10, 0x10,
	0xD6, 0xA0, 0x57, 0x35, 0x8D, 0x38, 0xF2, 0xF7, 0x0F, 0x49, 0xD1, 0x5A, 0xEA, 0x2F, 0x8D, 0x94,
	0x62, 0xEE, 0x43, 0x09, 0xB3, 0xF4, 0xA6, 0xA2, 0x18, 0xC6, 0x98, 0xE3, 0xC1, 0x7C, 0xE5, 0x7E,
	0x70, 0x6B, 0x09, 0x66, 0xF7, 0x02, 0x3C, 0x8B, 0x55, 0x95, 0xBF, 0x28, 0x39, 0xB3, 0x2E, 0xCC
};

void gost_random::init() {
	uint8	raw_key[32];

	memcpy(buffer, "12345678", 8);
	rand.fill(raw_key);
	rand.fill(buffer);

	state.init(init_params);
	key.set(raw_key);

	len = 0;
}

void gost_random::generate(uint8 *buf, size_t buflen) {
	uint8	data[8];
	size_t	c = 0;
	for (size_t l = 0; l < buflen; l += c) {
		if (len >= SEC_LEN)
			init();

		state.encrypt(key, buffer, data, 8);

		c = len - l;
		if (c > 8)
			c = 8;

		for (int i = 0; i < c; i++)
			buf[l + i] = data[i];

		for (int i = 0; i < 8; i++)
			buffer[i] ^= data[i];

		len += c;
	}
}

void gost_random::seed(const uint8 *buf, size_t buflen) {
	for (int i = 0; i < buflen; i++)
		buffer[i % 8] ^= buf[i];
}

//-----------------------------------------------------------------------------
//	GOST3410	(aka GOST2001)
//-----------------------------------------------------------------------------


EC_curve gostr3410::load(const void *P, const void *Q, const void *a, const void *b, const void *A_X, const void *A_Y) {
	return EC_curve(EC_curve::NONE,
		mpi((const uint8*)P, 32),
		mpi((const uint8*)a, 32),
		mpi((const uint8*)b, 32),
		mpi((const uint8*)A_X, 32),
		mpi((const uint8*)A_Y, 32),
		mpi((const uint8*)Q, 32)
	);
}

void gostr3410::sign(const EC_curve &ec, gost_random *rnd, const uint8 *h, uint8 *res) {
	uint8 k_buf[32] = {
		0x77, 0x10, 0x5c, 0x9b, 0x20, 0xbc, 0xd3, 0x12, 0x28, 0x23, 0xc8, 0xcf, 0x6f, 0xcc, 0x7b, 0x95,
		0x6d, 0xe3, 0x38, 0x14, 0xe9, 0x5b, 0x7f, 0xe6, 0x4f, 0xed, 0x92, 0x45, 0x94, 0xdc, 0xea, 0xb3
	};

	mpi H(h, 32);
	mpi	q = ec.q;
	mpi	e = H % q;

	if (!e)
		e = one;

	mpi r, s;
	do {
		// Generate new k:
	#ifndef BUILD_TEST_FROM_GOST
		rnd->generate(k_buf, 32);
	#endif
		mpi			k(k_buf, 32);
		EC_point	C = ec.mul(k, ec.base());

		r = C.x % q;
		if (!r)
			continue;

		s = (r * d + k * e) % q;

	} while (!s);

	// Signature is pair: r, s
	r.save(res, 32);
	s.save(res + 32, 32);
}

// And verify:
bool gostr3410::verify(const EC_curve &ec, const uint8 *H, const uint8 *s_buf) {
	mpi		r(s_buf, 32), s(s_buf + 32, 32), h(H, 32);

	if (s >= ec.q || r >= ec.q)
		return false;

	mpi		e = h % ec.q;
	if (!e)
		e = one;

	mpi		v = inv_mod(e, ec.q);
	mpi		z1 = (s * v) % ec.q;
	mpi		z2 = ((ec.q - r) * v) % ec.q;

	EC_point	C = ec.add(ec.mul(z1, ec.base()), ec.mul(z2, Q));

	return C.x % ec.q == r;
}

// Generate private and public keys:
void gostr3410::generate_private_key(const EC_curve &ec, struct gost_random *rnd) {
	uint8 buf[32];
	do
		rnd->generate(buf, 32);
	while (buf[0] == 0);
	d.load(buf, 32);
}

void gostr3410::generate_public_key(const EC_curve &ec) {
	Q = ec.mul(d, ec.base());
}

void gostr3410::generate_mutual_key(const EC_curve &ec, const void *Px, const void *Py, void *Mx, void *My) {
	// EC Diffi-Hellman:
	EC_point P(mpi((const uint8*)Px, 32), mpi((const uint8*)Py, 32));
	EC_point M = ec.mul(d, ec.base());

	M.x.save((uint8*)Mx, 32);
	M.y.save((uint8*)My, 32);
}
