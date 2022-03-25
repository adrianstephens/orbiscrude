#include "base/defs.h"
#include "base/strings.h"
#include "bignum.h"
#include "sparse.h"

namespace iso { namespace sym {

struct __infinity;

constant<__sqrt<__int<6> > >	sqrt6;
constant<__infinity>			infinity;

enum TYPE {
	ANY,

	OP_CONST,
	CONST_0		= OP_CONST,
	CONST_1,
	CONST_m1,
	CONST_2,
	CONST_e,
	CONST_pi,
	CONST_i,
	CONST_infinity,

	OP_VALUE,
	SYMBOL	= OP_VALUE,
	NUM,
	RATIONAL,
	COMPLEX,
	VECTOR,
	MATRIX,
	SPARSEMATRIX,
	POLYNOMIAL,

	OP_UNARY,
	OP_ABS	= OP_UNARY,
	OP_NEG,
	OP_SIGN,
	OP_SQRT,
	OP_LN,
	OP_EXP,
	OP_SIN,
	OP_COS,
	OP_TAN,
	OP_ASIN,
	OP_ACOS,
	OP_ATAN,
	OP_SINH,
	OP_COSH,
	OP_TANH,

	OP_BINARY,
	OP_ADD	= OP_BINARY,
	OP_SUB,
	OP_MUL,
	OP_DIV,
	OP_MOD,
	OP_POW,
	OP_LOG,
	OP_ATAN2,
};

class _Symbolic : public refs<_Symbolic> {
public:
	TYPE	type;
	_Symbolic(TYPE _type) : type(_type) {}
	virtual ~_Symbolic() {}
};

typedef	mpi					Num;
typedef	rational<Num>		Rational;

struct Symbolic : ref_ptr<_Symbolic> {
	template<typename T> struct contents : _Symbolic {
		T	t;
		template<typename T2> contents(TYPE type, const T2 &t2) : _Symbolic(type), t(t2) {}
	};
	typedef contents<Symbolic>					unary;
	typedef contents<pair<Symbolic, Symbolic> > binary;

	template<typename T> static _Symbolic *Make(TYPE type, const T &t) {
		return new Symbolic::contents<T>(type, t);
	}
	static _Symbolic *Make(TYPE type, const Symbolic &a, const Symbolic &b) {
		return Make(type, make_pair(a, b));
	}

	template<typename T> struct MakeConst;
	template<typename T> static _Symbolic	*GetConst()	{ static _Symbolic *s = MakeConst<T>::f(); return s; }

	Symbolic(_Symbolic *p)				: ref_ptr<_Symbolic>(p)						{}
	Symbolic(const mpi &a)				: ref_ptr<_Symbolic>(Make(NUM, a))		{}
	Symbolic(int i)						: ref_ptr<_Symbolic>(
			i == 0 ? GetConst<__int<0> >()
		:	i == 1 ? GetConst<__int<1> >()
		:	i == 2 ? GetConst<__int<2> >()
		:	i == -1 ? GetConst<__neg<__int<1> > >()
		:	Make(NUM, mpi(i))
	)	{}
	Symbolic(float f);
	Symbolic(const rational<mpi> &a)	: ref_ptr<_Symbolic>(a.d == 1 ? Make(NUM, a.n) : Make(RATIONAL, a))	{}
	template<typename T> Symbolic(const constant<T>&)	: ref_ptr<_Symbolic>(GetConst<T>())	{}

	Symbolic	Simplify() const;
};

template<int N>						struct Symbolic::MakeConst<__int<N>			> { static _Symbolic *f() { return Make(NUM, Num(N)); } };
template<int N>						struct Symbolic::MakeConst<__neg<__int<N> >	> { static _Symbolic *f() { return Make(NUM, Num(-N));} };
template<int D, int N>	struct Symbolic::MakeConst<__ratio<__int<D>, __int<N> >	> { static _Symbolic *f() { return Symbolic(Rational(Num(D), Num(N))); } };
template<typename D, typename N>	struct Symbolic::MakeConst<__ratio<D, N>	> { static _Symbolic *f() { return Make(OP_DIV, GetConst<D>(), GetConst<N>()); } };
template<typename T>				struct Symbolic::MakeConst<__sqrt<T>		> { static _Symbolic *f() { return Make(OP_SQRT, GetConst<T>()); } };
template<typename T>				struct Symbolic::MakeConst<__neg<T>			> { static _Symbolic *f() { return Make(OP_NEG, GetConst<T>()); } };
template<>							struct Symbolic::MakeConst<__e				> { static _Symbolic *f() { return new _Symbolic(CONST_e); } };
template<>							struct Symbolic::MakeConst<__pi				> { static _Symbolic *f() { return new _Symbolic(CONST_pi); } };
template<>							struct Symbolic::MakeConst<__int<0>			> { static _Symbolic *f() { return new _Symbolic(CONST_0); } };
template<>							struct Symbolic::MakeConst<__int<1>			> { static _Symbolic *f() { return new _Symbolic(CONST_1); } };
template<>							struct Symbolic::MakeConst<__int<2>			> { static _Symbolic *f() { return new _Symbolic(CONST_2); } };
template<>							struct Symbolic::MakeConst<__neg<__int<1> >	> { static _Symbolic *f() { return new _Symbolic(CONST_m1); } };
template<>				struct Symbolic::MakeConst<__sqrt<__neg<__int<1> > >	> { static _Symbolic *f() { return new _Symbolic(CONST_i); } };
template<>							struct Symbolic::MakeConst<__infinity		> { static _Symbolic *f() { return new _Symbolic(CONST_infinity); } };

template<TYPE type, typename T> struct SymbolicWrapper : Symbolic {
	contents<T>	*operator->() const { return (contents<T>*)get(); }
	template<typename T2> SymbolicWrapper(const T2 &t) : Symbolic(new contents<T>(type, t)) {}
};

//values
typedef SymbolicWrapper<SYMBOL, string>											Symbol;
typedef SymbolicWrapper<NUM, mpi>											SymNum;
typedef SymbolicWrapper<RATIONAL, pair<Symbolic, Symbolic> >					SymRational;
typedef SymbolicWrapper<COMPLEX, pair<Symbolic, Symbolic> >						SymComplex;
typedef SymbolicWrapper<VECTOR, dynamic_array<Symbolic> >						SymVector;
typedef SymbolicWrapper<MATRIX, pair<int,dynamic_array<Symbolic> > >			SymMatrix;
typedef SymbolicWrapper<SPARSEMATRIX, sparse_matrix<Symbolic> >					SymSparseMatrix;
typedef SymbolicWrapper<POLYNOMIAL, dynamic_array<pair<Symbolic, Symbolic> > >	SymPolynomial;

//unary
Symbolic	operator+(const Symbolic &a)	{ return a; }
Symbolic	operator-(const Symbolic &a)	{ return Symbolic::Make(OP_NEG,		a); }
Symbolic	abs(const Symbolic &a)			{ return Symbolic::Make(OP_ABS,		a); }
Symbolic	sqrt(const Symbolic &a)			{ return Symbolic::Make(OP_SQRT,	a); }
Symbolic	sign(const Symbolic &a)			{ return Symbolic::Make(OP_SIGN,	a); }
Symbolic	ln(const Symbolic &a)			{ return Symbolic::Make(OP_LN,		a); }
Symbolic	sin(const Symbolic &a)			{ return Symbolic::Make(OP_SIN,		a); }
Symbolic	cos(const Symbolic &a)			{ return Symbolic::Make(OP_COS,		a); }
Symbolic	tan(const Symbolic &a)			{ return Symbolic::Make(OP_TAN,		a); }
Symbolic	atan(const Symbolic &a)			{ return Symbolic::Make(OP_ATAN,	a); }
Symbolic	asin(const Symbolic &a)			{ return Symbolic::Make(OP_ASIN,	a); }
Symbolic	acos(const Symbolic &a)			{ return Symbolic::Make(OP_ACOS,	a); }
Symbolic	exp(const Symbolic &a)			{ return Symbolic::Make(OP_EXP,		a); }

//biary
Symbolic	operator+(const Symbolic &a, const Symbolic &b)	{ return Symbolic::Make(OP_ADD,		a, b); }
Symbolic	operator-(const Symbolic &a, const Symbolic &b)	{ return Symbolic::Make(OP_SUB,		a, b); }
Symbolic	operator*(const Symbolic &a, const Symbolic &b)	{ return Symbolic::Make(OP_MUL,		a, b); }
Symbolic	operator/(const Symbolic &a, const Symbolic &b)	{ return Symbolic::Make(OP_DIV,		a, b); }
Symbolic	operator%(const Symbolic &a, const Symbolic &b)	{ return Symbolic::Make(OP_MOD,		a, b); }
Symbolic	pow(const Symbolic &a, const Symbolic &b)		{ return Symbolic::Make(OP_POW,		a, b); }
Symbolic	log(const Symbolic &a, const Symbolic &b)		{ return Symbolic::Make(OP_LOG,		a, b); }
Symbolic	atan2(const Symbolic &a, const Symbolic &b)		{ return Symbolic::Make(OP_ATAN2,	a, b); }


struct Rule {
	enum RuleType {
		TERMINAL, SUB, SUB1, SUB2, STOP,
	};
	typedef	Symbolic (function)(const _Symbolic *s);

	RuleType	ruletype;
	TYPE		symtype, sub1, sub2;
	union {
		const void	*p;
		Rule		*subrule;
		function	*f;
	};

	Symbolic ApplyRules(Symbolic s) const;
	function *Find(Symbolic s) const;
};

template<typename A, Symbolic(*F)(const A &a)> struct unary_op {
	static Symbolic thunk(const _Symbolic *s) {
		Symbolic::unary	*u = (Symbolic::unary*)s;
		return F(((Symbolic::contents<A>*)(u->t.get()))->t);
	}
	operator Rule::function*() const { return thunk; }
};

template<typename A, typename B, Symbolic(*F)(const A &a, const B &b)> struct binary_op {
	static Symbolic thunk(const _Symbolic *s) {
		Symbolic::binary	*b = (Symbolic::binary*)s;
		return F(((Symbolic::contents<A>*)(b->t.a.get()))->t, ((Symbolic::contents<B>*)(b->t.b.get()))->t);
	}
	operator Rule::function*() const { return thunk; }
};

Symbolic add_num(const mpi &a, const mpi &b) {	return a + b; }
Symbolic sub_num(const mpi &a, const mpi &b) {	return a - b; }
Symbolic mul_num(const mpi &a, const mpi &b) {	return a * b; }
Symbolic div_num(const mpi &a, const mpi &b) {	return rational<mpi>::normalised(a, b); }
Symbolic mod_num(const mpi &a, const mpi &b) {	return a % b; }

const Rule	num_rules[] = {
	{Rule::TERMINAL, OP_ADD, ANY, ANY, binary_op<mpi, mpi, add_num>()},
	{Rule::TERMINAL, OP_SUB, ANY, ANY, binary_op<mpi, mpi, sub_num>()},
	{Rule::TERMINAL, OP_MUL, ANY, ANY, binary_op<mpi, mpi, mul_num>()},
	{Rule::TERMINAL, OP_DIV, ANY, ANY, binary_op<mpi, mpi, div_num>()},
	{Rule::TERMINAL, OP_MOD, ANY, ANY, binary_op<mpi, mpi, mod_num>()},
	Rule::STOP
};

Symbolic add_rational(const rational<mpi> &a, const rational<mpi> &b) {	return a + b; }
Symbolic sub_rational(const rational<mpi> &a, const rational<mpi> &b) {	return a - b; }
Symbolic mul_rational(const rational<mpi> &a, const rational<mpi> &b) {	return a * b; }
Symbolic div_rational(const rational<mpi> &a, const rational<mpi> &b) {	return a / b; }
Symbolic mod_rational(const rational<mpi> &a, const rational<mpi> &b) {	return mod(a, b); }

const Rule	rational_rules[] = {
	{Rule::TERMINAL, OP_ADD, ANY, ANY, binary_op<rational<mpi>, rational<mpi>, add_rational>()},
	{Rule::TERMINAL, OP_SUB, ANY, ANY, binary_op<rational<mpi>, rational<mpi>, sub_rational>()},
	{Rule::TERMINAL, OP_MUL, ANY, ANY, binary_op<rational<mpi>, rational<mpi>, mul_rational>()},
	{Rule::TERMINAL, OP_DIV, ANY, ANY, binary_op<rational<mpi>, rational<mpi>, div_rational>()},
	{Rule::TERMINAL, OP_MOD, ANY, ANY, binary_op<rational<mpi>, rational<mpi>, mod_rational>()},
	Rule::STOP
};

Symbolic right_rational(const _Symbolic *s) {
	Symbolic::binary	*b = (Symbolic::binary*)s;
	return Symbolic::Make(s->type, b->t.a, rational<mpi>(((SymNum&)(b->t.b))->t));
}

Symbolic left_rational(const _Symbolic *s) {
	Symbolic::binary	*b = (Symbolic::binary*)s;
	return Symbolic::Make(s->type, rational<mpi>(((SymNum&)(b->t.a))->t), b->t.b);
}

Symbolic trig_lookup24(int i) {
	static const Symbolic table[24] = {
		one,	(Symbolic( sqrt2) + Symbolic(sqrt6)) / 4,	sqrt3 * half,	one / sqrt2,	half,			(Symbolic(-sqrt2) + Symbolic(sqrt6)) / four,
		zero,	(Symbolic( sqrt2) - Symbolic(sqrt6)) / 4,	-half,			-one / sqrt2,	-sqrt3 * half,	(Symbolic(-sqrt2) - Symbolic(sqrt6)) / four,
		-one,	(Symbolic(-sqrt2) - Symbolic(sqrt6)) / 4,	-sqrt3 * half,	-one / sqrt2,	-half,			(Symbolic(sqrt2)  - Symbolic(sqrt6)) / four,
		zero,	(Symbolic(-sqrt2) + Symbolic(sqrt6)) / 4,	half,			one / sqrt2,	sqrt3 * half,	(Symbolic(sqrt2)  + Symbolic(sqrt6)) / four,
	};
	return table[i];
}
Symbolic cos_int(const mpi &a) {
	return a & 1 ? -1 : 1;
}
Symbolic cos_rational(const rational<mpi> &a) {
	if (a.d <= 12) {
		uint32	d = a.d.p[0];
		if (12 % d == 0)
			return trig_lookup24((a.n % (d * 2)) * (12 / d));
	}
	return a;
}
Symbolic sin_int(const mpi &a) {
	return zero;
}
Symbolic sin_rational(const rational<mpi> &a) {
	if (a.d <= 24) {
		uint32	d = a.d.p[0];
		if (12 % d == 0)
			return trig_lookup24(((a.n % (d * 2)) * (12 / d) + 6) % 24);
	}
	return a;
}

const Rule	cos_rules[] = {
	{Rule::TERMINAL,	OP_MUL, NUM,		CONST_pi,	unary_op<mpi,cos_int>()},
	{Rule::TERMINAL,	OP_MUL, RATIONAL,	CONST_pi,	unary_op<rational<mpi>, cos_rational>()},
	Rule::STOP
};

const Rule	divmul_rules[] = {
	{Rule::TERMINAL,	OP_MUL, NUM,		CONST_pi,	unary_op<mpi,cos_int>()},
};

const Rule	rules[] = {
	{Rule::SUB,			ANY,	NUM,		NUM,		num_rules},
	{Rule::TERMINAL,	ANY,	NUM,		RATIONAL,	left_rational},
	{Rule::TERMINAL,	ANY,	RATIONAL,	NUM,		right_rational},
	{Rule::SUB,			ANY,	RATIONAL,	RATIONAL,	rational_rules},
	{Rule::SUB1,		OP_COS, ANY,		ANY,		cos_rules},

	{Rule::TERMINAL,	OP_MUL, CONST_0,	ANY,		(Rule::function*)[](const _Symbolic *s)->Symbolic { return zero;}},
	{Rule::TERMINAL,	OP_ADD, CONST_0,	ANY,		(Rule::function*)[](const _Symbolic *s)->Symbolic { return ((Symbolic::binary*)s)->t.b;}},
	{Rule::TERMINAL,	OP_SUB, CONST_0,	ANY,		(Rule::function*)[](const _Symbolic *s)->Symbolic { return ((Symbolic::binary*)s)->t.b;}},
	{Rule::TERMINAL,	OP_DIV, CONST_0,	ANY,		(Rule::function*)[](const _Symbolic *s)->Symbolic { return infinity; }},
	{Rule::TERMINAL,	OP_MUL, ANY,		CONST_0,	(Rule::function*)[](const _Symbolic *s)->Symbolic { return zero;}},
	{Rule::TERMINAL,	OP_ADD, ANY,		CONST_0,	(Rule::function*)[](const _Symbolic *s)->Symbolic { return ((Symbolic::binary*)s)->t.a;}},
	{Rule::TERMINAL,	OP_SUB, ANY,		CONST_0,	(Rule::function*)[](const _Symbolic *s)->Symbolic { return -((Symbolic::binary*)s)->t.a;}},
	{Rule::TERMINAL,	OP_DIV, ANY,		CONST_0,	(Rule::function*)[](const _Symbolic *s)->Symbolic { return zero; }},

	{Rule::TERMINAL,	OP_MUL, CONST_1,	ANY,		(Rule::function*)[](const _Symbolic *s)->Symbolic { return ((Symbolic::binary*)s)->t.b;}},
	{Rule::TERMINAL,	OP_MUL, ANY,		CONST_1,	(Rule::function*)[](const _Symbolic *s)->Symbolic { return ((Symbolic::binary*)s)->t.a;}},
	{Rule::TERMINAL,	OP_DIV, ANY,		CONST_1,	(Rule::function*)[](const _Symbolic *s)->Symbolic { return ((Symbolic::binary*)s)->t.a;}},

	{Rule::TERMINAL,	OP_SQRT, CONST_m1,	ANY,		(Rule::function*)[](const _Symbolic *s)->Symbolic { return sqrt(-one); }},

//	{Rule::SUB2,		OP_DIV, OP_MUL,		ANY,		divmul_rules},
	Rule::STOP
};

//NUM * pi / NUM			->	RATIONAL * pi
//RATIONAL * pi / RATIONAL	->	RATIONAL * pi

#if 0
Rule::function *Rule::Find(Symbolic s) const {
	for (const Rule *rule = this; rule->ruletype != STOP; ++rule) {
		if (rule->symtype != ANY && rule->symtype != s->type)
			continue;
		if (rule->sub1 != ANY && rule->sub1 != ((Symbolic::unary*)s.get())->t->type)
			continue;
		if (rule->sub2 != ANY && (s->type < OP_BINARY || rule->sub2 != ((Symbolic::binary*)s.get())->t.b->type))
			continue;
		switch (rule->ruletype) {
			case TERMINAL:
				return rule->f;
			case SUB:
				return rule->subrule->Find(s);
			case SUB1:
				return rule->subrule->Find(((Symbolic::unary*)s.get())->t);
			case SUB2: 
				return rule->subrule->Find(((Symbolic::binary*)s.get())->t.b);
		}
	}
	return 0;

}
Symbolic Symbolic::Simplify() const {
	_Symbolic	*p = get();

	Rule::function *f = 0;
	if (p->type >= OP_UNARY) {
		Symbolic	a = ((Symbolic::unary*)p)->t.Simplify();
		if (p->type >= OP_BINARY) {
			Symbolic	b = ((Symbolic::binary*)p)->t.b.Simplify();
			f = rules[0].Find(Symbolic::Make(p->type, a, b));
		} else {
			f = rules[0].Find(Symbolic::Make(p->type, a));
		}
	} else {
		f = rules[0].Find(*this);
	}
	if (f)
		return f(p);
	return *this;
}

#else

Symbolic Rule::ApplyRules(Symbolic s) const {
	for (const Rule *rule = this; rule->ruletype != STOP; ++rule) {
		if (rule->symtype != ANY && rule->symtype != s->type)
			continue;
		if (rule->sub1 != ANY && (s->type < OP_VALUE || rule->sub1 != ((Symbolic::unary*)s.get())->t->type))
			continue;
		if (rule->sub2 != ANY && (s->type < OP_BINARY || rule->sub2 != ((Symbolic::binary*)s.get())->t.b->type))
			continue;
		switch (rule->ruletype) {
			case TERMINAL:
				s = rule->f(s);
				break;
			case SUB:
				s = rule->subrule->ApplyRules(s);
				break;
			case SUB1:
				s = rule->subrule->ApplyRules(((Symbolic::unary*)s.get())->t);
				break;
			case SUB2: 
				s = rule->subrule->ApplyRules(((Symbolic::binary*)s.get())->t.b);
				break;
		}
	}
	return s;
}
Symbolic Symbolic::Simplify() const {
	_Symbolic	*p = get();

	if (p->type >= OP_UNARY) {
		Symbolic	a = ((Symbolic::unary*)p)->t.Simplify();
		if (p->type >= OP_BINARY) {
			Symbolic	b = ((Symbolic::binary*)p)->t.b.Simplify();
			return rules[0].ApplyRules(Symbolic::Make(p->type, a, b));
		}
		return rules[0].ApplyRules(Symbolic::Make(p->type, a));
	}
	return rules[0].ApplyRules(*this);
}

#endif

struct tester {
	tester() {
		Symbol		a("a");
		Symbolic	b = cos(Symbolic(3) / Symbolic(12) * pi);
		Symbolic	t = b.Simplify();

	}
};// _tester;

} } // namespace iso::sym



