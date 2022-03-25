#ifndef ELLIPTIC_CURVES_H
#define ELLIPTIC_CURVES_H

#include "stream.h"
#include "maths/bignum.h"
#include "extra/random.h"

namespace iso {

struct EC_point {
	mpi		x, y, z;
	EC_point() {}
	EC_point(param(mpi) x, param(mpi) y, param(mpi) z = one) : x(x), y(y), z(z) {}
	bool	is_zero() const { return !x && !y && !z; }
	friend EC_point projectify(param(EC_point) v) { return EC_point(v.x, v.y, one); }
};

struct EC_curve : mod_context {
	typedef mod_context B;
	enum TYPE {
		NONE		= 0,
		SECT_K1		= 1,
		SECT_R1		= 2,
		SECT_R2		= 3,
		SECP_K1		= 4,
		SECP_R1		= 5,
		SECP_R2		= 6,
		BRAINPOOLP_R1 = 7
	};

	TYPE		type;
//	mpi			p;		// Prime modulus p (in mod_context)
	mpi			a;		// Curve parameter a
	mpi			b;		// Curve parameter b
	EC_point	g;		// x, y of the  base point G
	mpi			q;		// Order of the base point G
	uint32		h;		// Cofactor h
	void(*fast_mod)(mpi &a, param(mpi) p);

	EC_curve()	{}
	EC_curve(
		TYPE		type,
		param(mpi)	p, param(mpi) a, param(mpi) b,
		param(mpi)	x, param(mpi) y,
		param(mpi)	q,
		uint32		h = 0,
		void(*fast_mod)(mpi &a, param(mpi) p) = 0
	) : mod_context(p), type(type), a(a), b(b), g(x, y), q(q), h(h), fast_mod(fast_mod)	{}

	EC_point	base() const { return g; }

#if 0
	mpi		mul_mod(param(mpi) a, param(mpi) b) const {
		mpi	t = a * b;
//		if (fast_mod) {
//			fast_mod(t, p);
//			return t;
//		}
		return t % p;
	}
	EC_point	neg(param(EC_point) s) const {
		return EC_point(s.x, p - s.y, s.z);
	}
#endif
	using B::mul;
	using B::add;
	using B::twice;

	EC_point		load(const uint8 *data, size_t length) const;
	EC_point		load(const const_memory_block &b) const {
		return load(b, b.length());
	}
	int				save(const EC_point &a, uint8 *data, bool compress = false) const;
	malloc_block	save(const EC_point &a, bool compress = false) const {
		malloc_block	b(save(a, 0, compress));
		save(a, b, compress);
		return b;
	}
	bool			is_affine(param(EC_point) s) const;
	EC_point		affinify(param(EC_point) s) const;
	EC_point		twice(param(EC_point) s) const;
	EC_point		add(param(EC_point) s, param(EC_point) t) const;
	EC_point		full_add(param(EC_point) s, param(EC_point) t) const;
	EC_point		full_sub(param(EC_point) s, param(EC_point) t) const;
	EC_point		mul(param(mpi) d, param(EC_point) s) const;
	EC_point		twin_mult(param(mpi) d0, param(EC_point) s, param(mpi) d1, param(EC_point) t) const;

	static	const EC_curve *get_named(const char *name);
};

struct EC_group {
	struct vtable {
		void	(*group_init)(EC_group*);
		void	(*group_finish)(EC_group*);
		bool	(*group_copy)(EC_group*, const EC_group*);
		bool	(*group_set_curve)(EC_group*, const mpi &p, const mpi &a, const mpi &b);
		bool	(*point_get_affine_coordinates)(const EC_group*, const EC_point&, mpi &x, mpi &y);

		// Computes |r = g_scalar*generator + p_scalar*p| if |g_scalar| and |p_scalar| are both non-null
		// Computes |r = g_scalar*generator| if |p_scalar| is null
		// Computes |r = p_scalar*p| if g_scalar is null.
		// At least one of |g_scalar| and |p_scalar| must be non-null, and |p| must be non-null if |p_scalar| is non-null
		EC_point (*mul)(const EC_group *group, const mpi &g_scalar, const EC_point &p, const mpi &p_scalar);

		// 'field_mul' and 'field_sqr' can be used by 'add' and 'dbl' so that the same implementations of point operations can be used with different optimized implementations of expensive field operations:
		mpi		(*field_mul)(const EC_group*, const mpi &a, const mpi &b);
		mpi		(*field_sqr)(const EC_group*, const mpi& a);
		mpi		(*field_encode)(const EC_group*, const mpi &a); // e.g. to Montgomery
		mpi		(*field_decode)(const EC_group*, const mpi &a); // e.g. from Montgomery
	};

	const vtable *vt;
	EC_point	*generator;
	mpi			order;
	const montgomery_context *mont_data; // for ECDSA inverse

	mpi			field;			// For curves over GF(p), this is the modulus
	mpi			a, b;			// Curve coefficients
	bool		a_is_minus3;	// enable optimized point arithmetics for special case
	montgomery_context	*mont;	// Montgomery structure
	mpi			one;			// The value one

	//default imps
	void		group_init()							{}
	void		group_finish()							{}
	mpi			field_encode(const mpi &a)		const	{ return a; }
	mpi			field_decode(const mpi &a)		const	{ return a; }
	bool		point_get_affine_coordinates(const EC_point &p, mpi &x, mpi &y)		const	{ return false; }
	EC_point	mul(const mpi &g_scalar, const EC_point &p, const mpi &p_scalar)	const	{ return p; }

	EC_group(const vtable *vt) : vt(vt), a_is_minus3(false), mont(nullptr) {}
};

template<typename T, typename BASE = EC_group> struct EC_groupT : BASE {
	typedef BASE	B;
	static EC_group::vtable	static_vt;
	EC_groupT()  : BASE(&static_vt) {}
};

template<typename T, typename BASE> EC_group::vtable EC_groupT<T,BASE>::static_vt = {
	make_staticfunc(&T::group_init),
	make_staticfunc(&T::group_finish),
	make_staticfunc(&T::group_copy),
	make_staticfunc(&T::group_set_curve),
	make_staticfunc(&T::point_get_affine_coordinates),
	make_staticfunc(&T::mul),
	make_staticfunc(&T::field_mul),
	make_staticfunc(&T::field_sqr),
	make_staticfunc(&T::field_encode),
	make_staticfunc(&T::field_decode)
};

struct EC_group_simple : EC_groupT<EC_group_simple> {
	bool	group_copy(const EC_group* x) {
		field		= x->field;
		a			= x->a;
		b			= x->b;
		one			= x->one;
		a_is_minus3 = x->a_is_minus3;
		return true;
	}
	bool	group_set_curve(const mpi& p, const mpi& _a, const mpi& _b) {
		if (p.num_bits() <= 2 || !p.is_odd())
			return false;

		field = abs(p);

		// group->a
		mpi	tmp_a = _a % field;
		a	= vt->field_encode(this, tmp_a);
		b	= vt->field_encode(this, _b % p);

		a_is_minus3 = tmp_a == -3;
		one = vt->field_encode(this, iso::one);

		return true;
	}

	bool		point_get_affine_coordinates(const EC_point &p, mpi &x, mpi &y)		const;
	mpi			field_mul(const mpi &a, const mpi &b)	const	{ return mul_mod(a, b, field); }
	mpi			field_sqr(const mpi &a)					const	{ return square_mod(a, field); }
};

struct EC_group_montgomery : EC_groupT<EC_group_montgomery, EC_group_simple> {
	void	group_init()		{ mont = nullptr; }
	void	group_finish()		{ delete mont; }
	bool	group_copy(const EC_group* x) {
		B::group_copy(x);
		if (x->mont)
			mont = x->mont;	//dup
		return true;
	}
	bool	group_set_curve(const mpi& p, const mpi& _a, const mpi& _b) {
		B::group_set_curve(p, _a, _b);
		mont = new montgomery_context(p);
		return true;
	}
	bool		point_get_affine_coordinates(const EC_point &p, mpi &x, mpi &y)		const;
	mpi			field_mul(const mpi &a, const mpi &b)	const	{ return mont->mul(a, b); }
	mpi			field_sqr(const mpi &a)					const	{ return mont->square(a); }
	mpi			field_encode(const mpi &a)				const	{ return mont->to(a); }
	mpi			field_decode(const mpi &a)				const	{ return mont->from(a); }
};

struct ECDH : EC_curve {
	mpi			priv_key;

	ECDH() {}
	ECDH(const EC_curve &c) : EC_curve(c) {}
	void	init(const EC_curve &c) { EC_curve::operator=(c); }

	void	generate_private(vrng &&rng) {
		priv_key	= mpi::random(rng, q.num_bits());
		if (priv_key >= q)
			priv_key >>= 1;
	}
	malloc_block	get_public() const {
		return save(affinify(mul(priv_key, g)));
	}
	bool			check_public_key(param(EC_point) pub_key) {
		return pub_key.x >= zero && pub_key.x < p && pub_key.y >= zero && pub_key.y < p && is_affine(pub_key);
	}
	malloc_block	shared_secret(const const_memory_block &peer_pub_key) {
		EC_point		pt = load(peer_pub_key);
		malloc_block	output(p.num_bytes());
		affinify(mul(priv_key, projectify(pt))).x.save(output);
		return output;
	}
};

struct ECDSA_signature {
	mpi r, s;
	ECDSA_signature(param(mpi) _r, param(mpi) _s) : r(_r), s(_s) {}
	ECDSA_signature(const EC_curve *curve, vrng &&rng, param(mpi) priv_key, const const_memory_block &digest);
	bool verify(const EC_curve *curve, param(EC_point) pub_key, const const_memory_block &digest) const;
};

}

#endif // ELLIPTIC_CURVES_H
