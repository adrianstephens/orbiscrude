#ifndef SPARSE_H
#define SPARSE_H

#include "base/defs.h"
#include "base/array.h"
#include "base/block.h"
#include "base/algorithm.h"
#include "base/vector.h"
#include "dynamic_vector.h"

namespace iso {

//-----------------------------------------------------------------------------
//	permutations
//-----------------------------------------------------------------------------

void invert_permute(const int *s, int *d, int n);

template<typename T> void permute(const T *s, T *d, const int *p, int n) {
	for (int i = 0; i < n; i++)
		d[i] = s[p ? p[i] : i];
}

template<typename T> void inv_permute(const T *s, T *d, const int *p, int n) {
	for (int i = 0; i < n; i++)
		d[p ? p[i] : i] = s[i];
}

struct op_recip	{ template<typename A> A operator()(const A &a) const { return reciprocal(a); } };

//-----------------------------------------------------------------------------
//	sparse_vector (really a reference to a column of a sparse matrix)
//-----------------------------------------------------------------------------

struct sparse_layout_vector {
	int		nz;
	int		*i;
	sparse_layout_vector(int _nz, int *_i) : nz(_nz), i(_i) {}
	void	permute(const int *p);
	void	inv_permute(const int *p);
	int		add_pattern(int *mask, int mark, int *out);
};

template<typename T> struct sparse_vector : sparse_layout_vector {
	T		*v;
	sparse_vector(int nz, int *i, T *v) : sparse_layout_vector(nz, i), v(v) {}
	void	scatter(T *p);
	void	scatter(T *p, T beta);
	int		scatter(T *p, T beta, int *w, int mark, int *iout);
};

template<typename T> void sparse_vector<T>::scatter(T *p) {
	for (int j = 0; j < nz; j++)
		p[i[j]] = v[j];
}

template<typename T> void sparse_vector<T>::scatter(T *p, T beta) {
	for (int j = 0; j < nz; j++)
		p[i[j]] += v[j] * beta;
}

template<typename T> int sparse_vector<T>::scatter(T *p, T beta, int *w, int mark, int *iout) {
	int	nz = 0;
	for (int j = 0; j < nz; j++) {
		int	k = i[j];
		if (w[k] < mark) {
			w[k]		= mark;
			p[k]		= v[j] * beta;
			iout[nz++]	= k;	// add k to pattern
		}
		p[k] += v[j] * beta;
	}
	return nz;
}

template<typename A, typename B> A dot(const A *a, const sparse_vector<B> &b) {
	B	*pb	= b.v;
	int	*jb	= b.i;

	A	t	= 0;
	for (int i = 0, n = b.nz; i < n; i++)
		t += a[jb[i]] * pb[i];
	return t;
}

template<typename A, typename B> inline A dot(const sparse_vector<A> &a, const B *b) {
	return dot(b, a);
}

template<typename A, typename B> A dot(const sparse_vector<A> &a, const sparse_vector<B> &b) {
	return dot(dynamic_vector<A>(a), b);
}

//-----------------------------------------------------------------------------
//	sparse_coord(s) - for building sparse matrices
//-----------------------------------------------------------------------------

struct sparse_coord {
	int	r, c;
	sparse_coord() {}
	sparse_coord(int r, int c) : r(r), c(c) {}
	bool	operator<(const sparse_coord &b)	const { return c == b.c ? r < b.r : c < b.c; }
	bool	operator==(const sparse_coord &b)	const { return c == b.c && r == b.r; }
	bool	operator!=(const sparse_coord &b)	const { return c != b.c || r != b.r; }
};

struct sparse_layout_coords : dynamic_array<sparse_coord> {
	int		m, n;

	sparse_layout_coords() : m(0), n(0)	{}
	sparse_layout_coords(int m, int n) : m(m), n(n)	{}
	sparse_layout_coords&	compact();

	void add(int i, int j) {
		m = max(m, i + 1);
		n = max(n, j + 1);
		new(*this) sparse_coord(i, j);
	}
};

template<typename T> struct sparse_entry : sparse_coord {
	T	v;
	sparse_entry() {}
	sparse_entry(int r, int c, const T &v) : sparse_coord(r, c), v(v) {}
};

template<typename T> struct sparse_matrix_coords : dynamic_array<sparse_entry<T> > {
	int		m, n;

	sparse_matrix_coords() : m(0), n(0)	{}
	sparse_matrix_coords(int m, int n) : m(m), n(n)	{}
	sparse_matrix_coords&	compact();

	void add(int i, int j, const T &v) {
		m = max(m, i + 1);
		n = max(n, j + 1);
		if (v)
			new(*this) sparse_entry<T>(i, j, v);
	}
};

template<typename T> sparse_matrix_coords<T> &sparse_matrix_coords<T>::compact() {
	sort(*this);

	sparse_entry<T>	*d = this->begin();
	for (sparse_entry<T> *i = d + 1, *e = this->end(); i != e; ++i) {
		if (*d == *i) {
			d->v += i->v;
		} else {
			if (d->v)
				++d;
			*d = *i;
		}
	}
	this->resize(d + 1 - this->begin());
	return *this;
}

// filter predicates

struct is_diagonal;
template<> struct _not<is_diagonal>	{
	bool operator()(int i, int j)		{ return i != j; }
};
struct is_diagonal {
	bool operator()(int i, int j)		{ return i == j; }
	_not<is_diagonal> operator!() const	{ return _not<is_diagonal>(); }
};

struct is_upper_triangular;
template<> struct _not<is_upper_triangular> {
	bool operator()(int i, int j)		{ return i < j; }
};
struct is_upper_triangular {
	bool operator()(int i, int j)		{ return i >= j; }
	_not<is_upper_triangular> operator!() const	{ return _not<is_upper_triangular>(); }
};

//-----------------------------------------------------------------------------
//	sparse_layout
//-----------------------------------------------------------------------------

struct sparse_layout {
	enum FLAGS {
		PATTERN_SYMMETRIC	= 1<<0,
		DIAGONAL			= 1<<1,
		LOWER				= 1<<2,
		UPPER				= 1<<3,
		HAS_DIAGONAL		= 1<<4,

		SYMMETRIC			= 1<<5,
		SKEW				= 1<<6,
		HERMITIAN			= 1<<7,
		UNDIRECTED			= 1<<8,
	};
	flags<FLAGS>	flags;
	int				m, n;
	ref_array<int>	indices;

	sparse_layout()							: m(0), n(0) {}
	sparse_layout(int m, int n, int nz)		: m(m), n(n), indices(n + 1 + nz) {}
	sparse_layout(sparse_layout_coords &coords);
	template<typename T> sparse_layout(dynamic_matrix<T> &A);

	void			create(int _m, int _n, int nz)	{ m = _m, n = _n; indices.create(_n + 1 + nz); }
	void			make_unique()				{ if (indices.shared()) indices = indices.dup(n + 1 + nz()); }
	void			reserve(int nz, int nzmax)	{ ref_array<int> indices2(n + 1 + nzmax); copy_n(indices.begin(), indices2.begin(), n + 1 + nz); indices = indices2; }
	int*			ia()		const	{ return indices; }
	int*			ja()		const	{ return indices + n + 1; }
	int				nz()		const	{ return indices[n]; }

	int				rows()		const	{ return m; }
	int				cols()		const	{ return n; }

	sparse_layout	permute(const int *pinv, const int *q)	const;
	sparse_layout	permute_symmetric(const int *pinv)		const;
	int				categorise();

	template<typename P> int filter(P p);

	friend sparse_layout get_transpose(const sparse_layout &A);
};

sparse_layout operator+(const sparse_layout &A, const sparse_layout &B);
sparse_layout operator*(const sparse_layout &A, const sparse_layout &B);

template<typename T> sparse_layout::sparse_layout(dynamic_matrix<T> &A) {
	int	nz = 0;
	for (int c = 0; c < A.cols(); ++c) {
		for (int r = 0; r < A.rows(); ++r) {
			if (A(c, r))
				++nz;
		}
	}

	create(A.rows(), A.cols(), nz);

	int	*ia	= this->ia(), *ja = this->ja();
	nz = 0;
	for (int c = 0; c < n; ++c) {
		ia[c] = nz;
		for (int r = 0; r < m; ++r) {
			if (A(c, r))
				ja[nz++] = r;
		}
	}

	ia[n] = nz;
	categorise();
}

template<typename P> int sparse_layout::filter(P p) {
	int		*ia	= this->ia(), *ja = this->ja();
	int		nz = 0;
	for (int i = 0, j = 0; i < n; i++) {
		ia[i] = nz;
		for (int j1 = ia[i + 1]; j < j1; j++) {
			if (p(ja[j], i))
				ja[nz++] = ja[j];
		}
	}
	ia[n] = nz;
	return nz;
}

//-----------------------------------------------------------------------------
//	symbolic
//-----------------------------------------------------------------------------

// symbolic Cholesky, LU, or QR analysis
struct sparse_symbolic {
	enum ORDER { NATURAL = 0, CHOL_ORDER = 1, LU_ORDER = 2, QR_ORDER = 3 };

	ref_array<int>	pinv;		// inverse row perm. for QR, fill red. perm for Chol
	ref_array<int>	q;			// fill-reducing column permutation for LU and QR

	ref_array<int>	parent;		// elimination tree for Cholesky and QR
	ref_array<int>	cp;			// column pointers for Cholesky, row counts for QR
	ref_array<int>	leftmost;	// leftmost[i] = min(find(A(i,:))), for QR
	int				m2;			// # of rows for QR, after adding fictitious rows
	int				lnz;		// # entries in L for LU or Cholesky; in V for QR
	int				unz;		// # entries in U for LU; in R for QR

	void					init(const sparse_layout &A);

	static sparse_symbolic	LU(const sparse_layout &a, ORDER order);
	static sparse_symbolic	QR(const sparse_layout &a, ORDER order);
	static sparse_symbolic	CHOL(const sparse_layout &a, ORDER order);
};

ref_array<int> col_counts(const sparse_layout &A, const int *parent, const int *post, bool ata = false);
ref_array<int> elimination_tree(const sparse_layout &A, bool ata = false);
ref_array<int> post_order(const int *parent, int n);
ref_array<int> amd(sparse_layout &A, int dense = 0);

//-----------------------------------------------------------------------------
//	sparse_matrix
//-----------------------------------------------------------------------------

template<typename T> class sparse_matrix : public sparse_layout {
public:
	ref_array<T>	values;
	using sparse_layout::m;
	using sparse_layout::n;
	using sparse_layout::nz;

	sparse_matrix() {}
	sparse_matrix(int m, int n, int nz) : sparse_layout(m, n, nz), values(nz) {}
	sparse_matrix(sparse_matrix_coords<T> &coords);
	sparse_matrix(sparse_layout &layout, ref_array<T> &_values) : sparse_layout(layout), values(_values) {}
	sparse_matrix(dynamic_matrix<T> &A);

	void			create(int m, int n, int nz) {
		sparse_layout::create(m, n, nz);
		values.create(nz);
	}
	void			make_unique() {
		sparse_layout::make_unique();
		if (values.shared())
			values = values.dup(nz());
	}
	void			reserve(int nz, int nzmax) {
		sparse_layout::reserve(nz, nzmax);
		ref_array<T> values2(nzmax);
		copy_n(values.get(), values2.get(), nz);
		values = values2;
	}

	T*				pa()		const	{ return values; }

	sparse_vector<T> operator[](int c) const {
		int	*ia	= this->ia(), *ja = this->ja();
		int	i	= ia[c];
		return sparse_vector<T>(ia[c + 1] - i, ja + i, values + i);
	}

	template<typename P> int	filter(P p);

	void			sort();
	T				norm();
	sparse_matrix&	operator*=(const T &t);
	sparse_matrix&	scale_rows(const T *t);
	sparse_matrix&	scale_cols(const T *t);

	sparse_matrix&	remove_diagonal() {
		filter(!is_diagonal());
		return *this;
	}
	sparse_matrix&	remove_upper() {
		filter(!is_upper_triangular());
		flags.clear_all(PATTERN_SYMMETRIC | SYMMETRIC | SKEW | HERMITIAN);
		flags.set(LOWER);
		return *this;
	}

	sparse_matrix	permute(const int *pinv, const int *q)		const;
	bool			within(const sparse_matrix &b, T epsilon)	const;
	bool			is_symmetric(T epsilon = 0.0000001f);
};
template<typename T> constexpr bool is_mat<sparse_matrix<T>> = true;

template<typename T> sparse_matrix<T>::sparse_matrix(dynamic_matrix<T> &A) : sparse_layout(A), values(nz()) {
	int		*ia	= this->ia(), *ja = this->ja();
	T		*pa	= values;
	for (int i = 0, j = 0; i < n; i++) {
		for (int j1 = ia[i + 1]; j < j1; j++)
			pa[j]	= A[i][ja[j]];
	}
}

template<typename T> sparse_matrix<T>	transpose(const sparse_matrix<T> &A);
template<typename T> sparse_matrix<T>	transpose(sparse_matrix<T> &&A)	{ return transpose((const sparse_matrix<T>&)A); }
template<typename T> sparse_matrix<T>	symmetrize(sparse_matrix<T> &A);

template<typename TM, typename TV> dynamic_vector<TV>	operator*(const sparse_matrix<TM> &M, const TV *v);
template<typename TM, typename TV> dynamic_vector<TV>	operator*(const sparse_matrix<TM> &M, const dynamic_vector<TV> &v) { return M * v.begin(); }
template<typename TM, typename TV> dynamic_vector<TV>	operator*(const TV *v, const sparse_matrix<TM> &M);
template<typename TA, typename TB> sparse_matrix<TA>	operator+(const sparse_matrix<TA> &A, const sparse_matrix<TB> &B);
template<typename TA, typename TB> sparse_matrix<TA>	operator*(const sparse_matrix<TA> &A, const sparse_matrix<TB> &B);
template<typename TA, typename TB, typename TC> sparse_matrix<TA> multiply3(const sparse_matrix<TA> &A, const sparse_matrix<TB> &B, const sparse_matrix<TC> &C);
template<typename TV, typename TM> dynamic_vector<TV>&	operator/=(dynamic_vector<TV> &v, const sparse_matrix<TM> &M);

// structure for numeric Cholesky, LU, or QR factorization
template<typename T> struct sparse_numeric {
	sparse_matrix<T>	L;		// L for LU and Cholesky, V for QR
	sparse_matrix<T>	U;		// U for LU, R for QR, not used for Cholesky
	ref_array<int>		pinv;	// partial pivoting for LU
	ref_array<T>		B;		// beta[0..n-1] for QR

	static sparse_numeric	LU(const sparse_matrix<T> &A, const sparse_symbolic &S, T tol = 0);
	static sparse_numeric	QR(const sparse_matrix<T> &A, const sparse_symbolic &S);
	static sparse_numeric	CHOL(const sparse_matrix<T> &A, const sparse_symbolic &S);
};

//-----------------------------------------------------------------------------
//	sparse_matrix members and operators
//-----------------------------------------------------------------------------

template<typename T> sparse_matrix<T>::sparse_matrix(sparse_matrix_coords<T> &coords) {
	coords.compact();
	if (coords.size() == 0)
		return;

	sparse_layout::create(coords.m, coords.n, int(coords.size()));
	values.create(int(coords.size()));

	int	c	= 0;
	T	*p	= values, *p0 = p;
	int	*ia	= this->ia(), *ja = this->ja();

	for (sparse_entry<T> *i = coords.begin(), *e = coords.end(); i != e; ++i) {
		while (c <= i->c)
			ia[c++] = int(p - p0);
		*p++	= i->v;
		*ja++	= i->r;
	}
	while (c <= coords.n)
		ia[c++] = int(p - p0);

	categorise();
}

template<typename T> template<typename P> int sparse_matrix<T>::filter(P p) {
	make_unique();
	int		*ia	= this->ia(), *ja = this->ja();
	T		*pa	= values;
	int		nz	= 0;
	for (int i = 0, j = 0; i < n; i++) {
		ia[i] = nz;
		for (int j1 = ia[i + 1]; j < j1; j++) {
			if (p(ja[j], i)) {
				pa[nz]	= pa[j];
				ja[nz]	= ja[j];
				++nz;
			}
		}
	}
	ia[n] = nz;
	categorise();
	return nz;
}

template<typename T> sparse_matrix<T> sparse_matrix<T>::permute(const int *pinv, const int *q) const {
	sparse_matrix<T>	B(m, n, nz());
	int		*ia	= this->ia(), *ja = this->ja(), *ib = B.ia(), *jb = B.ja();
	T		*pa	= this->pa(), *pb = B.pa();

	int	nz = 0;
	for (int i = 0; i < n ;i++) {
		ib[i] = nz;
		int	i2 = q ? q[i] : i;
		for (int j = ia[i2], j1 = ia[i2 + 1]; j < j1; j++, nz++) {
			pb[nz]	= pa[j];
			jb[nz]	= pinv ? pinv[ja[j]] : ja[j];
		}
	}
	ib[n] = nz;
	return B;
}

template<typename T> void sparse_matrix<T>::sort() {
	*this = transpose(transpose(*this));
}

template<typename T> T sparse_matrix<T>::norm() {
	int		*ia		= this->ia();
	T		*pa		= this->pa();
	T		norm	= 0;

	for (int i = 0, j = 0; i < n; i++) {
		T	s = 0;
		for (int j1 = ia[i + 1]; j < j1; j++)
			s += abs(pa[j]);
		norm = max(norm, s);
	}
	return norm;
}

template<typename T> sparse_matrix<T> &sparse_matrix<T>::operator*=(const T &t) {
	make_unique();
	for (T *p = values, *e = p + nz(); p != e; ++p)
		*p *= t;
	return *this;
}

template<typename T> sparse_matrix<T> &sparse_matrix<T>::scale_rows(const T *v) {
	T	*pa	= values;
	int	*ia	= this->ia();

	for (int i = 0, j = 0; i < n; i++) {
		if (v[i] != 0) {
			for (int j1 = ia[i + 1]; j < j1; j++)
				pa[j] *= v[i];
		}
	}
	flags.clear(sparse_layout::SYMMETRIC);
	return *this;
}

template<typename T> sparse_matrix<T> &sparse_matrix<T>::scale_cols(const T *v) {
	T	*pa	= values;
	int	*ia	= this->ia(), *ja = this->ja();

	for (int i = 0, j = 0, i1 = cols(); i < i1; i++) {
		for (int j1 = ia[i + 1]; j < j1; j++)
			pa[j] *= v[ja[j]];
	}
	flags.clear(sparse_layout::SYMMETRIC);
	return *this;
}

template<typename T> sparse_matrix<T> transpose(const sparse_matrix<T> &A) {
	int		m	= A.m, n = A.n;
	sparse_matrix<T> B(n, m, A.nz());

	int		*ia	= A.ia(), *ja = A.ja();
	int		*ib	= B.ia(), *jb = B.ja();

	for (int i = 0; i <= m; i++)
		ib[i] = 0;

	for (int i = 0, j = 0; i < n; i++) {
		for (int j1 = ia[i + 1]; j < j1; j++)
			ib[ja[j] + 1]++;
	}

	for (int i = 0; i < m; i++)
		ib[i + 1] += ib[i];

	T	*pa = A.pa();
	T	*pb = B.pa();
	for (int i = 0, j = 0; i < n; i++) {
		for (int j1 = ia[i + 1]; j < j1; j++) {
			jb[ib[ja[j]]]	= i;
			pb[ib[ja[j]]++]	= pa[j];
		}
	}

	for (int i = m; i > 0; i--)
		ib[i] = ib[i - 1];
	ib[0] = 0;

	B.flags	= A.flags;
	B.flags.swap(sparse_layout::LOWER, sparse_layout::UPPER);
	return B;
}

template<typename T> sparse_matrix<T> symmetrize(sparse_matrix<T> &A) {
	if (A.is_symmetric())
		return A;

	sparse_matrix<T> B = A + transpose(A);
	B.flags.set_all(sparse_layout::PATTERN_SYMMETRIC | sparse_layout::SYMMETRIC);
	return B;
}

template<typename T> bool sparse_matrix<T>::within(const sparse_matrix<T> &B, T epsilon) const {
	if (m != B.m || n != B.n)
		return false;

	int		*ia	= this->ia(), *ja = this->ja();
	int		*ib	= B.ia(), *jb = B.ja();

	for (int i = 0; i <= n; i++) {
		if (ia[i] != ib[i])
			return false;
	}

	int	*mask = alloc_auto(int, m);
	for (int i = 0; i < m; i++)
		mask[i] = -1;

	T	*pa = values;
	T	*pb = B.values;
	for (int i = 0; i < n; i++) {
		for (int j = ia[i], j1 = ia[i + 1]; j < j1; j++)
			mask[ja[j]] = j;

		for (int j = ib[i], j1 = ib[i + 1]; j < j1; j++) {
			if (mask[jb[j]] < ia[i])
				return false;
		}
		for (int j = ib[i], j1 = ib[i + 1]; j < j1; j++) {
			if (abs(pb[j] - pa[mask[jb[j]]]) > epsilon)
				return false;
		}
	}

	return true;
}

template<typename T> bool sparse_matrix<T>::is_symmetric(T epsilon) {
	if (flags.test(sparse_layout::SYMMETRIC))
		return true;

	if (!flags.test(sparse_layout::PATTERN_SYMMETRIC))
		return false;

	if (within(transpose(*this), epsilon)) {
		flags.set(sparse_layout::SYMMETRIC);
		return true;
	}

	return false;
}

template<typename TM, typename TV> dynamic_vector<TV> operator*(const sparse_matrix<TM> &M, const TV *v) {
	dynamic_vector<TV>	u(M.n);
	u.clear();

#if 1
	for (int i = 0, i1 = M.cols(); i < i1; i++)
		M[i].scatter(u, v[i]);
#else
	TM	*pa	= M.values;
	int	*ia	= M.ia();
	int	*ja	= M.ja();

	for (int i = 0, j = 0, i1 = M.cols(); i < i1; i++) {
		for (int j1 = ia[i + 1]; j < j1; j++)
			u[ja[j]] += pa[j] * v[i];
	}
#endif
	return u;
}

template<typename TM, typename TV> dynamic_vector<TV> operator*(const TV *v, const sparse_matrix<TM> &M) {
	int					n = M.n;
	dynamic_vector<TV>	u(n);

	TM	*pa	= M.values;
	int	*ia	= M.ia();
	int	*ja	= M.ja();

	for (int i = 0, j = 0; i < n; i++) {
		u[i] = 0;
		for (int j1 = ia[i + 1]; j < j1; j++)
			u[i] += pa[j] * v[ja[j]];
	}
	return u;
}

template<typename TA, typename TB> sparse_matrix<TA> operator+(const sparse_matrix<TA> &A, const sparse_matrix<TB> &B) {
	int	mmax	= max(A.m, B.m);
	int	na		= A.n, nb = B.n, nmax = max(na, nb);
	int	nzmax	= A.nz() + B.nz();// just assume that no entries overlaps for speed

	sparse_matrix<TA>	C(mmax, nmax, nzmax);
	int				*ia	= A.ia(), *ja = A.ja();
	int				*ib	= B.ia(), *jb = B.ja();
	int				*ic	= C.ia(), *jc = C.ja();
	TA				*pa = A.values;
	TB				*pb = B.values;
	TA				*pc = C.values;

	int		*mask = alloc_auto(int, mmax);
	for (int i = 0; i < mmax; i++)
		mask[i] = -1;

	int		nz = 0;
	for (int i = 0; i < nmax; i++) {
		int	start = ic[i] = nz;
		if (i < na) {
			for (int j = ia[i], j1 = ia[i + 1]; j < j1; j++) {
				jc[nz]	= ja[j];
				pc[nz]	= pa[j];
				mask[ja[j]] = nz++;
			}
		}
		if (i < nb) {
			for (int j = ib[i], j1 = ib[i + 1]; j < j1; j++) {
				if (mask[jb[j]] < start) {
					jc[nz]		= jb[j];
					pc[nz++]	= pb[j];
				} else {
					pc[mask[jb[j]]] += pb[j];
				}
			}
		}
	}
	ic[nmax] = nz;
	return C;
}

template<typename TA, typename TB> sparse_matrix<TA> operator*(const sparse_matrix<TA> &A, const sparse_matrix<TB> &B) {
	if (A.n != B.m)
		return sparse_matrix<TA>();

	int		m	= A.m;
	int		n	= B.n;

	int		*mask	= alloc_auto(int, m);
	for (int i = 0; i < m; i++)
		mask[i] = -1;

	int		*ia	= A.ia(), *ja = A.ja();
	int		*ib	= B.ia(), *jb = B.ja();
	int		nz = 0;
	for (int i = 0, j = 0; i < n; i++) {
		for (int j1 = ib[i + 1]; j < j1; j++) {
			for (int k = ia[jb[j]], k1 = ia[jb[j] + 1]; k < k1; k++) {
				if (mask[ja[k]] != -i - 2) {
					mask[ja[k]] = -i - 2;
					nz++;
				}
			}
		}
	}

	sparse_matrix<TA>	C(m, n, nz);
	int		*ic	= C.ia(), *jc = C.ja();
	TA		*pa = A.values;
	TB		*pb = B.values;
	TA		*pc = C.values;

	nz = 0;
	for (int i = 0, j = 0; i < n; i++) {
		ic[i] = nz;
		for (int j1 = ib[i + 1]; j < j1; j++) {
			for (int k = ia[jb[j]], k1 = ia[jb[j] + 1]; k < k1; k++) {
				int	t = ja[k];
				if (mask[t] < ic[i]) {
					mask[t] = nz;
					jc[nz]	= t;
					pc[nz]	= pb[j] * pa[k];
					nz++;
				} else {
					pc[mask[t]] += pb[j] * pa[k];
				}
			}
		}
	}
	ic[n] = nz;
	return C;
}

template<typename TA, typename TB, typename TC> sparse_matrix<TA> multiply3(const sparse_matrix<TA> &A, const sparse_matrix<TB> &B, const sparse_matrix<TC> &C) {
	if (A.n != B.m || B.n != C.m)
		return sparse_matrix<TA>();

	int		m	= A.m;
	int		n	= C.n;

	int		*mask	= alloc_auto(int, m);
	for (int i = 0; i < m; i++)
		mask[i] = -1;

	int		*ia	= A.ia(), *ja = A.ja();
	int		*ib	= B.ia(), *jb = B.ja();
	int		*ic	= C.ia(), *jc = C.ja();
	int		nz = 0;
	for (int i = 0, j = 0; i < n; i++) {
		for (int j1 = ic[i + 1]; j < j1; j++) {
			for (int l = ib[ja[j]], l1 = ib[ja[j] + 1]; l < l1; l++) {
				for (int k = ia[jb[l]], k1 = ia[jb[l] + 1]; k < k1; k++) {
					if (mask[ja[k]] != -i - 2) {
						mask[ja[k]] = -i - 2;
						nz++;
					}
				}
			}
		}
	}

	sparse_matrix<TA>	D(m, n, nz);
	int		*id	= D.ia(), *jd = D.ja();
	TA		*pa = A.values;
	TB		*pb = B.values;
	TC		*pc = C.values;
	TA		*pd = D.values;

	nz = 0;
	for (int i = 0, j = 0; i < n; i++){
		ic[i] = nz;
		for (int j1 = ic[i + 1]; j < j1; j++){
			for (int l = ib[jc[j]], l1 = ib[jc[j] + 1]; l < l1; l++){
				for (int k = ic[jb[l]], k1 = ic[jb[l] + 1]; k < k1; k++){
					int	t = ja[k];
					if (mask[t] < id[i]){
						mask[t]	= nz;
						jd[nz]	= t;
						pd[nz]	= pc[j] * pb[l] * pa[k];
						nz++;
					} else {
						pd[mask[t]] += pc[j] * pb[l] * pa[k];
					}
				}
			}
		}
	}
	id[n] = nz;
	return D;
}

// create a Householder reflection [v,beta,s]=house(x), overwrite x with v, where (I-beta*v*v')*x = s*e1
template<typename T> T house(T *x, T &beta, int n) {
	T	sigma = 0;
	for (int i = 1; i < n; i++)
		sigma += x[i] * x[i];

	if (sigma == 0) {
		beta = x[0] <= 0 ? 2 : 0;
		x[0] = 1;
		return abs(x[0]);
	} else {
		T	s	= sqrt(x[0] * x[0] + sigma);	// s = norm(x)
		x[0]	= x[0] <= 0 ? x[0] - s : -sigma / (x[0] + s);
		beta	= -1 / (s * x [0]);
		return s;
	}
}

// apply the a Householder vector to x
template<typename TV, typename TM> void applyhh(const sparse_vector<TM> &c, TV *v, TV beta) {
	c.scatter(v, -dot(v, c) * beta);
}

template<typename TV, typename TM> void lsolve(const sparse_matrix<TM> &M, TV *v) {
	int		*ia		= M.ia(), *ja = M.ja();
	TM		*pa		= M.pa();

	for (int i = 0, j = 0, n = M.n; i < n; i++) {
		v[i] /= pa[j++];
		for (int j1 = ia[i + 1]; j < j1; j++)
			v[ja[j]] -= pa[j] * v[i];
	}
}

template<typename TV, typename TM> void ltsolve(const sparse_matrix<TM> &M, TV *v) {
	int		*ia		= M.ia(), *ja = M.ja();
	TM		*pa		= M.pa();

	for (int i = M.n, j = M.nz() - 1; i > 0; i--) {
		for (int j1 = ia[i - 1]; j > j1; j--)
			v[i] -= pa[j] * v[ja[j]];
		v[i] /= pa[j--];
	}
}

template<typename TV, typename TM> void usolve(const sparse_matrix<TM> &M, TV *v) {
	int		*ia		= M.ia(), *ja = M.ja();
	TM		*pa		= M.pa();

	for (int i = M.n, j = M.nz() - 1; i > 0; i--) {
		v[i] /= pa[j--];
		for (int j1 = ia[i - 1]; j > j1; j--)
			v[ja[j]] -= pa[j] * v[i];
	}
}

template<typename TV, typename TM> void utsolve(const sparse_matrix<TM> &M, TV *v) {
	int		*ia		= M.ia(), *ja = M.ja();
	TM		*pa		= M.pa();

	for (int i = 0, j = 0, n = M.n; i < n; i++) {
		for (int j1 = ia[i + 1] - 1; j < j1; j++)
			v[i] -= pa[j] * v[ja[j]];
		v[i] /= pa[j++];
	}
}

// x=A\b where A is unsymmetric
template<typename TV, typename TM> void lusolve(const sparse_matrix<TM> &M, dynamic_vector<TV> &v, sparse_symbolic::ORDER order, TM tol = 0) {
	sparse_symbolic		S	= sparse_symbolic::LU(M, order);
	sparse_numeric<TM>	N	= sparse_numeric<TM>::LU(M, S, tol);
	TV					*x	= alloc_auto(TV, v.n);

	inv_permute(v.get(), x, N.pinv, v.n);
	lsolve(N.L, x);
	usolve(N.U, x);
	inv_permute(x, v.get(), S.q, v.n);
}

template<typename TV, typename TM> void qrsolve(const sparse_matrix<TM> &M, dynamic_vector<TV> &v, sparse_symbolic::ORDER order) {
	int	m = M.m, n = M.n;

	if (m >= n) {
		sparse_symbolic		S = sparse_symbolic::QR(M, order);
		sparse_numeric<TM>	N = sparse_numeric<TM>::QR(M, S);
		dynamic_vector<TV>	x = v.inv_permute(S.pinv);	//x = b(p)

		for (int k = 0; k < n; k++)						// apply Householder refl. to x
			applyhh(N.L[k], x, N.B[k]);

		usolve(N.U, x);
		inv_permute(x, v, S.q, v.n);

	} else {
		sparse_matrix<TM>	AT = transpose(M);			// Ax=b is underdetermined
		sparse_symbolic		S = sparse_symbolic::QR(AT, order);
		sparse_numeric<TM>	N = QR(AT, S);
		dynamic_vector<TV>	x = v.permute(S.q);

		utsolve(N.U, x);								// x = R'\x
		for (int k = m - 1; k >= 0; k--)				// apply Householder refl. to x
			applyhh(N.L[k], x, N.B[k]);
		permute(x, v, S.pinv, v.n);
	}
}

// x=A\b where A is symmetric positive definite; b overwritten with solution
template<typename TV, typename TM> void cholsolve(const sparse_matrix<TM> &M, dynamic_vector<TV> &v, sparse_symbolic::ORDER order) {
	sparse_symbolic		S = sparse_symbolic::CHOL(M, order);
	sparse_numeric<TM>	N = sparse_numeric<TM>::CHOL(M, S);
	dynamic_vector<TV>	x = v.inv_permute(S.pinv);	//x = b(p)

	lsolve(N.L, x);					// x = L\x
	ltsolve(N.L, x);				// x = L'\x
	permute(x, v, S.pinv, v.n);		// b = P'*x
}

template<typename TV, typename TM> dynamic_vector<TV>& operator/=(dynamic_vector<TV> &v, const sparse_matrix<TM> &M) {
	if (M.flags.test(sparse_layout::LOWER)) {
		lsolve(M, v);

	} else if (M.flags.test(sparse_layout::UPPER)) {
		usolve(M, v);

	} else if (M.is_symmetric()) {
		qrsolve(M, v, sparse_symbolic::NATURAL);

	} else {
		lusolve(M, v, sparse_symbolic::NATURAL, 0);
	}

	return v;
}

//-----------------------------------------------------------------------------
//	LDL
//-----------------------------------------------------------------------------
// Find elimination tree and the number of nonzeros in each column of L
// parent[i] = k	if k is the parent of i in the tree
sparse_layout ldl_symbolic(const sparse_layout &A, const ref_array<int> &P, ref_array<int> &Pinv, ref_array<int> &parent);

// Given a sparse matrix A and its symbolic analysis (Ls, parent, P and Pinv), compute the numeric LDL' factorization of A or PAP'
// The outputs of this routine are arguments Lx, and D
// returns n if successful, k if D (k,k) is zero
template<typename T> int ldl_numeric(
	const sparse_matrix<T>	&A,
	const ref_array<int>	&P, const ref_array<int> &Pinv,
	const ref_array<int>	&parent,
	sparse_layout			&Ls,
	dynamic_vector<T>		&Lx,
	dynamic_vector<T>		&D
) {
	int		n		= A.rows();
	int		*Ap		= A.ia();
	int		*Ai		= A.ja();
	T		*Ax		= A.pa();

	int		*Lp		= Ls.ia();
	int		*Li		= Ls.ja();
	int		*nz		= alloc_auto(int, n);

	int		*Y			= alloc_auto(int, n);
	int		*pattern	= alloc_auto(int, n);
	int		*flag		= alloc_auto(int, n);

	for (int k = 0; k < n; k++) {
		// compute nonzero pattern of kth row of L, in topological order
		int	top = n;				// stack for pattern is empty
		Y[k]	= 0;				// Y(0:k) is now all zero
		flag[k] = k;				// mark node k as visited
		nz[k]	= 0;				// count of nonzeros in column k of L
		int	kk	= P[k];				// kth permuted, column
		for (int p = Ap[kk], p2 = Ap[kk + 1]; p < p2; p++) {
			int	i = Pinv[Ai[p]];
			if (i <= k) {
				Y[i] += Ax[p];		// scatter A(i,k) into Y (sum duplicates)
				int	len;
				for (len = 0; flag[i] != k; i = parent[i]) {
					pattern[len++]	= i;	// L(k,i) is nonzero
					flag[i]			= k;	// mark i as visited
				}
				while (len > 0)
					pattern[--top] = pattern[--len];
			}
		}

		// compute numerical values kth row of L (a sparse triangular solve)
		D[k] = Y[k];				// get D(k,k) and clear Y(k)
		Y[k] = 0;
		for (; top < n; top++) {
			int	i = pattern[top];	// pattern [top:n-1] is pattern of L(:,k)
			T	yi = Y[i];			// get and clear Y(i)
			Y[i] = 0;

			int	p2 = Lp[i] + nz[i];
			for (int p = Lp[i]; p < p2; p++)
				Y[Li[p]] -= Lx[p] * yi;

			T	l_ki = yi / D[i];	// the nonzero entry L(k,i)
			D[k]	-= l_ki * yi;
			Li[p2]	= k;			// store L(k,i) in column form of L
			Lx[p2]	= l_ki;
			nz[i]++;				// increment count of nonzeros in col i
		}
		if (D[k] == 0)
			return k;				// failure, D(k,k) is zero
	}
	return n;	// success, diagonal of D is all nonzero
}

//solve Lx=b
// X is solution on output
template<typename TM, typename TV> void ldl_lsolve(sparse_matrix<TM> &L, dynamic_vector<TV> &X) {
	int		*ia		= L.ia(), *ja = L.ja();
	TM		*pa		= L.pa();
	TV		*v		= X;

	for (int i = 0, j = 0, n = L.n; i < n; i++) {
		for (int j1 = ia[i + 1]; j < j1; j++)
			v[ja[j]] -= pa[j] * v[i];
	}
}

// solve Dx=b
// X is solution on output
template<typename TM, typename TV> void ldl_dsolve(dynamic_vector<TM> &D, dynamic_vector<TV> &X) {
	for (int j = 0, n = X.n; j < n; j++)
		X[j] /= D[j];
}

// solve L'x=b
// X is solution on output
template<typename TM, typename TV> void ldl_ltsolve(sparse_matrix<TM> &L, dynamic_vector<TV> &X) {
	int		*ia		= L.ia(), *ja = L.ja();
	TM		*pa		= L.pa();
	TV		*v		= X;

	for (int i = L.n, j = L.nz() - 1; i > 0; i--) {
		for (int j1 = ia[i - 1]; j > j1; j--)
			v[i] -= pa[j] * v[ja[j]];
	}
}

//-----------------------------------------------------------------------------
//	singular_value_decomposition
//-----------------------------------------------------------------------------
#if 0

template<typename T> bool svd(const dynamic_matrix<T> &A, dynamic_matrix<T> &U, dynamic_vector<T> &S, dynamic_matrix<T> &VT, int max_iter = 50) {
	const int m = A.rows(), n = A.cols();
	U	= A;
	VT	= identity;

	for (int iter = 0; ; ++iter) {
		T	max_e = 0;
		for (int j = 1; j < n; j++) {
			for (int i = 0; i < j; ++i) {			// for indices i < j of columns of U
				T	a = 0, b = 0, c = 0;
				for (int k = 0; k < m; ++k) {		// construct 2x2 submatrix [ a, c; c, b ] of column inner products on U
					a += square(U(k, i));
					b += square(U(k, j));
					c += U(k, i) * U(k, j);
				}

				if (c == 0)
					continue;	// columns are already orthogonal

				max_e = max(max_e, abs(c) / sqrt(a * b));	// measure non-orthogonality of pair of columns

				// compute Jacobi rotation parameters: cos(theta), sin(theta)
				T	z	= (b - a) / (c * 2);
				T	t	= sign1(z) / (abs(z) + hypot(1.f, z)); // tan(theta)
				T	cs	= rhypot(t, one);
				T	sn	= cs * t;

				for (int k = 0; k < m; ++k) {	// apply Jacobi rotation to U
					T vlk = U(k, i);
					U(k, i) = cs * vlk - sn * U(k, j);
					U(k, j) = sn * vlk + cs * U(k, j);
				}
				for (int k = 0; k < n; ++k) {	// apply Jacobi rotation to VT
					T vlk = VT(k, i);
					VT(k, i) = cs * vlk - sn * VT(k, j);
					VT(k, j) = sn * vlk + cs * VT(k, j);
				}
			}
		}
		if (max_e < T(epsilon))
			break;

		if (iter == max_iter)
			return false;			//singular_value_decomposition convergence failure
	}
	for (int i = 0; i < n; ++i) {
		// normalize the column vector
		S[i] = len(U.col(i));		// singular value is norm of column vector of U
		if (S[i]) {
			T recip = reciprocal(S[i]);
			for (int j = 0; j < m; ++j)
				U(j, i) *= recip;
		}
	}
	return true;
}

template<typename T> void sort_singular_values(dynamic_matrix<T> U, dynamic_vector<T> &S, dynamic_matrix<T> &VT) {
	const int n = U.cols();
	// Insertion sort
	for (int i0 = 0; i0 < n - 1; ++i0) {
		int	i1 = argmax(S.get_view().sub(i0, n)) - S.get_view();
		if (i0 != i1) {
			swap(S[i0], S[i1]);
			swap_ranges(U.col(i0),  U.col(i1));
			swap_ranges(VT.col(i0), VT.col(i1));
		}
	}
}

template<typename T> dynamic_vector<T> svd_solve(const dynamic_matrix<T> &A, dynamic_vector<T> &X, int max_iter = 50) {
	dynamic_matrix<T> U, VT;
	dynamic_vector<T> S;
	if (svd(A, U, S, VT, max_iter)) {
		//A = U.S.V, where V = transpose(V), and U, V are unitary (i.e. their transposes are their inverses)
		return VT * (transpose(U) * dynamic_vector<T>(X / S));
//		return VT * (U * dynamic_vector<T>(X / S));
	}
	return dynamic_vector<T>();
}

template<typename T> dynamic_matrix<T> svd_inverse(const dynamic_matrix<T> &A, int max_iter = 50) {
	dynamic_matrix<T> U, VT;
	dynamic_vector<T> S;
	if (svd(A, U, S, VT, max_iter)) {
		//A = U.S.V, where V = transpose(V), and U, V are unitary (i.e. their transposes are their inverses)
		return VT * (transpose(U) / scale(S));
		//return VT * (U / scale(S));
	}
	return dynamic_matrix<T>();
}

//-----------------------------------------------------------------------------
//	dynamic_vector
//-----------------------------------------------------------------------------

//expects ones on diagonal
template<typename T> void linvert(dynamic_matrix<T> &L) {
	L = transpose(L);

	for (int i = 1, rows = L.rows(); i < rows; ++i)
		for (int j = i - 1; j >= 0; --j)
			L(j, i) = -dot(L.row(i).sub(0, i + 1), L.row(j));

	L.clear_upper();
}

template<typename T> void uinvert(dynamic_matrix<T> &U) {
	for_each(U.diagonal(), [](T &t) {t = reciprocal(t); });

	for (int i = 0, rows = U.rows(); i < rows; ++i)
		for (int j = i - 1; j >= 0; --j)
			U(j, i) = -U(j, j) * dot(U.row(i).sub(0, i + 1), U.row(j));

	U.clear_upper();
	U = transpose(U);
}

// Smart way to translate a row permutation vector to obtain a column permutation vector
template<typename T> void permute_cols(dynamic_matrix<T> &M, const int *p) {
	int	m		= M.cols();
	int	*o		= alloc_auto(int, m);

	for (int i = 0; i < m; ++i)
		o[i] = i;

	for (int i = 0; i < m; ++i) {
		while (p[i] != o[i]) {
			int		k = i + 1;
			while (p[k] != o[i])
				k++;
			swap(o[i], o[k]);
			swap_ranges(M.col(i), M.col(k));
		}
	}
}

// Optimized way to calculate p * L, a permutation vector is used to swap the rows of L
template<typename T> void permute_rows(dynamic_matrix<T> &M, const int *p) {
	int	m		= M.rows();
	int	*o		= alloc_auto(int, m);

	for (int i = 0; i < m; ++i)
		o[i] = i;

	for (int i = 0; i < m; ++i) {
		while (p[i] != o[i]) {
			int		k = i + 1;
			while (p[k] != o[i])
				k++;
			swap(o[i], o[k]);
			swap_ranges(M.row(i), M.row(k));
		}
	}
}

// LU factorization with Gaussian Elimination and Partial Pivoting
// return 0 for singular, -1 for odd swaps, 1 for even swaps
template<typename T> int lupp(const dynamic_matrix<T> &A, dynamic_matrix<T> &L, dynamic_matrix<T> &U, int *p) {
	ISO_ASSERT(A.is_square());

	U = A;
	U.make_unique();
	L.resize(A.m, A.n);
	L = identity;

	dynamic_vector<T>	t(A.m);
	dynamic_vector<T>	r(A.n);
	for (int i = 0; i < A.m; ++i)
		p[i] = i;

	int	determinant	= 1;

	for (int j = 0; j < A.n; ++j) {
		// Partial pivoting process
		auto	pmax	= argmax(U.col(j).sub(j));
		T		max		= *pmax;
		if (max == 0)
			return 0;	// matrix is singular

		int	column_max_position = pmax - U.col(j).begin();
		if (j != column_max_position) {
			determinant = -determinant;
			swap_ranges(U.row(j), U.row(column_max_position));
			swap_ranges(L.col(j), L.col(column_max_position));
			swap(p[j], p[column_max_position]);
		}

		for (int i = j + 1; i < A.m; ++i)
			t[i] = U(j, i) / max;

		for (int i = 0; i < A.m; ++i)
			L(j, i) += dot(L.row(i).sub(j + 1), t.get_view().sub(j + 1));

		for (int i = j + 1; i < A.m; ++i) {
			for (int k = j; k < A.n; ++k)
				r[k] = U(k, i) - t[i] * U(k, j);

			for (int k = j; k < A.n; ++k)
				U(k, i) = r[k];
		}
	}

	permute_rows(L, p);

	// calc determinant
	//for (auto &i : U.diagonal())
	//	determinant *= i;

	return determinant;
}

template<typename T> dynamic_vector<T>& dynamic_vector<T>::operator/=(const dynamic_matrix<T> &m) {
	int		n	= m.rows();
	int		*p	= alloc_auto(int, n);

	dynamic_matrix<T> L, U;
	m.lupp(L, U, p);
	linvert(L);
	uinvert(U);

	T		*pb	= alloc_auto(T, n);
	permute(this->get(), pb, p, n);

	this->resize(n);
	T		*v	= *this;
	// x = L * pb;
	for (int i = 0; i < n; ++i)
		v[i] = pb[i] + dot(L.row(i).sub(0, i), pb);

	// x = U * x
	for (int i = 0; i < n; ++i)
		v[i] = dot(U.row(i).sub(i), pb + i);
	return *this;
}

template<typename T> dynamic_matrix<T> get_inverse(const dynamic_matrix<T> &m) {
	int		n	= m.rows();
	int		*p	= alloc_auto(int, n);

	dynamic_matrix<T> L, U;
	lupp(m, L, U, p);
	linvert(L);
	uinvert(U);

	// Optimization of u^-1 * l^-1 that takes into account the shape of the two matrices
	dynamic_matrix<T>	inv(n, n);
	for (int i = 0; i < n; ++i)
		for (int j = 0; j < n; ++j)
			inv(j, i) = dot(U.row(i).sub(min(i, j)), L.col(j).sub(min(i, j)));

	int		*pinv	= alloc_auto(int, n);
	invert_permute(p, pinv, n);
	permute_cols(inv, pinv);

	return inv;
}
#endif
} // namespace iso

#endif // SPARSE_H
