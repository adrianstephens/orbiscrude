#ifndef GOST_H
#define GOST_H

#include "base/defs.h"
#include "extra/random.h"
#include "comms/elliptic-curves.h"
#include "hashes/hash_stream.h"

namespace iso {

//-----------------------------------------------------------------------------
//	GOST28147	(aka GOST89)
//-----------------------------------------------------------------------------

struct gost28147 {
	typedef array<uint8, 8> CODE;

	struct Key {
		uint32 k[8];
		Key()						{}
		Key(const uint8 *raw_key)	{ set(raw_key); }
		void	set(const uint8 *raw_key) {
			for (int i = 0; i < 8; i++)
				k[i] = ((uint32le*)raw_key)[i];
		}
		void	get(uint8 *raw_key) {
			for (int i = 0; i < 8; i++)
				((uint32le*)raw_key)[i] = k[i];
		}
		operator const uint32*() const { return k; }
	};
	struct State {
		uint32 sbox0[256];
		uint32 sbox1[256];
		uint32 sbox2[256];
		uint32 sbox3[256];

		inline uint32 STEP(uint32 x, uint32 Nl, uint32 Nh) const {
			uint32 S = x + Nl;
			return Nh ^ (sbox0[(S >> 24)] | sbox1[(S >> 16) & 255] | sbox2[(S >> 8) & 255] | sbox3[S & 255]);
		}
		State()						{}
		State(const uint8 *params)	{ init(params); }
		void	init(const uint8 *params);
		void	ALLe(const uint32 *key, uint32 Nh, uint32 Nl, uint32 *Nh_out, uint32 *Nl_out) const;
		void	ALLd(const uint32 *key, uint32 Nh, uint32 Nl, uint32 *Nh_out, uint32 *Nl_out) const;
		void	ALLmac(const uint32 *key, uint32 Nh, uint32 Nl, uint32 *Nh_out, uint32 *Nl_out) const;

		void encrypt(const Key &key, const uint8 *data, uint8 *edata, size_t data_len) const {
			for (int i = 0; i < data_len / 8; i++) {
				uint32 E1, E2;
				ALLe(key, ((uint32le*)data)[i * 2], ((uint32le*)data)[i * 2 + 1], &E1, &E2);
				((uint32le*)edata)[i * 2 + 1] = E1;
				((uint32le*)edata)[i * 2 + 0] = E2;
			}
		}
	
		void decrypt(const Key &key, const uint8 *edata, uint8 *data, size_t data_len) const {
			for (int i = 0; i < data_len / 8; i++) {
				uint32 E1, E2;
				ALLd(key, ((uint32le*)data)[i * 2], ((uint32le*)data)[i * 2 + 1], &E1, &E2);
				((uint32le*)edata)[i * 2 + 1] = E1;
				((uint32le*)edata)[i * 2 + 0] = E2;
			}
		}
		void mac_encrypt(const Key &key, const uint8 *data, uint8 *edata) const {
			uint32 E1, E2;
			ALLmac(key, ((uint32le*)data)[0], ((uint32le*)data)[1], &E1, &E2);
			((uint32le*)edata)[0] = E1;
			((uint32le*)edata)[1] = E2;
		}
	};
	Key		key;
	State	state;

	gost28147(const uint8 *_key, const uint8 *iv) : key(_key), state(iv) {}
};

struct GOST28147_encrypt : gost28147 {
	GOST28147_encrypt(const uint8 *key, const uint8 *iv) : gost28147(key, iv) {}
	void	process(void *data) {
		uint32le	*p = (uint32le*)data;
		uint32 E1, E2;
		state.ALLe(key, p[0], p[1], &E1, &E2);
		p[1] = E1;
		p[0] = E2;
	}
};
struct GOST28147_decrypt : gost28147 {
	GOST28147_decrypt(const uint8 *key, const uint8 *iv) : gost28147(key, iv) {}
	void	process(void *data) {
		uint32le	*p = (uint32le*)data;
		uint32 E1, E2;
		state.ALLd(key, p[0], p[1], &E1, &E2);
		p[1] = E1;
		p[0] = E2;
	}
};

struct gost28147_gamma : gost28147 {
	CODE	gamma;
	uint32	N1, N2;
	int		used;

	gost28147_gamma(const uint8 *key, const uint8 *iv, const void *g) : gost28147(key, iv), used(-1) {
		if (g)
			memcpy(gamma.begin(), g, sizeof(gamma));
		else
			clear(gamma);
	}
	void update();
	void do_gamma(const uint8 *data, uint8 *r, size_t size);
	void encrypt(const uint8 *data, uint8 *r, size_t size);
	void decrypt(const uint8 *data, uint8 *r, size_t size);
	void encrypt(uint8 *data, size_t size) { encrypt(data, data, size); }
	void decrypt(uint8 *data, size_t size) { decrypt(data, data, size); }
};

struct gost28147_mac : gost28147, public block_writer<gost28147_mac, 8> {
	CODE	mac;

	gost28147_mac(const uint8 *key, const uint8 *iv, const uint8 *sync) : gost28147(key, iv) {
		if (sync)
			memcpy(mac.begin(), sync, 8);
		else
			clear(mac);
	}
	void	process(const uint8 *data) {
		state.mac_encrypt(key, mac.begin(), mac.begin());
		mac = make_deferred(mac) ^ *(CODE*)data;
	}
	CODE digest() {
		if (uint32 o = p % BLOCK_SIZE) {
			for (int i = 0; i < o; i++)
				mac[i] ^= block[i];
			state.mac_encrypt(key, mac.begin(), mac.begin());
		}
		return mac;
	}
};

//-----------------------------------------------------------------------------
//	GOST3411	(AKA GOST94)
//-----------------------------------------------------------------------------

struct gostr3411 : block_writer<gostr3411, 32> {
	typedef uint8		hash[BLOCK_SIZE];
	typedef array<uint8, BLOCK_SIZE> CODE;

	gost28147::State	state;
	hash				H, sum, len;

	gostr3411(const gost28147::State *_state = 0, const hash iv = 0);
	void	update(const hash data);
	void	process(const hash data);
	void	terminate();
	CODE	digest()	{ terminate(); return &H[0]; }
	void	reset()		{
		clear(H);
		clear(sum);
		clear(len);
	}
};

//-----------------------------------------------------------------------------
//	GOST_RANDOM
//-----------------------------------------------------------------------------

struct gost_random {
	enum {SEC_LEN = 512};
	static const uint8	init_params[];

	vrng				&rand;
	gost28147::Key		key;
	gost28147::State	state;
	uint8				buffer[8];
	size_t				len;

	gost_random(vrng &_rand) : rand(_rand) {}

	void	init();
	void	generate(uint8 *buf, size_t buflen);
	void	seed(const uint8 *buf, size_t buflen);
};

//-----------------------------------------------------------------------------
//	GOST3410	(aka GOST2001)
//-----------------------------------------------------------------------------

struct gostr3410 {
	mpi			mask;
	mpi			d; // Private key 
	EC_point	Q; // Public key 

	static EC_curve	load(const void *P, const void *Q, const void *a, const void *b, const void *A_X, const void *A_Y);

	void	sign(const EC_curve &ec, struct gost_random *rnd, const uint8 *h, uint8 *res);
	bool	verify(const EC_curve &ec, const uint8 *H, const uint8 *s_buf);
	void	generate_private_key(const EC_curve &ec, struct gost_random *rnd);
	void	generate_public_key(const EC_curve &ec);
	void	generate_mutual_key(const EC_curve &params, const void *Px, const void *Py, void *Mx, void *My);

	void	set_private_key(const void *key)					{ d.load((uint8*)key, 32); }
	void	get_private_key(void *key)							{ d.save((uint8*)key, 32); }
	void	set_public_key(const void *keyX, const void *keyY)	{ Q.x.load((const uint8*)keyX, 32); Q.y.load((const uint8*)keyY, 32);	}
	void	get_public_key(void *keyx, void *keyy)				{ Q.x.save((uint8*)keyx, 32); Q.y.save((uint8*)keyy, 32);	}
};
} //namespace iso

#endif //GOST_H
