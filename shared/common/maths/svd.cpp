#include "dynamic_vector.h"

namespace iso {

//-----------------------------------------------------------------------------
//	Householder
//-----------------------------------------------------------------------------

// Compute a householder transformation (tau,v) of a vector x so that P x = [ I - tau*v*v' ] x annihilates x(1:n-1)
// v[0] = -sign(x[0])*||x|| so that P x = v[0] * e_1
template<typename T> T householder_transform(vector_view<T> x) {
	if (x.n == 1)
		return 0;

	T	alpha	= x[0];
	T	beta	= sign(alpha) * len(x);

	x	/= alpha - beta;
	x[0] = beta;
	return (beta - alpha) / beta;
}

// apply a householder transformation v to vector w
template<typename T> void householder_hv(T tau, vector_view<T> v, vector_view<T> w) {
	T		d	= tau * (w[0] + dot(v.slice(1), w.slice(1)));
	w[0]		-= d;
	w.slice(1)	-= v.slice(1) * d;
}

// applies a householder transformation v,tau to matrix A
template<typename T> void householder_hm(T tau, vector_view<T> v, const dynamic_matrix<T> &A) {
	if (tau) {
		for (uint32 j = 0; j < A.m; j++)
			householder_hv(tau, v, A.column(j));
	}
}

// apply a householder transformation v,tau to matrix A from the right hand side in order to zero out rows
template<typename T> void householder_mh(T tau, vector_view<T> v, const dynamic_matrix<T> &A) {
	if (tau) {
		for (uint32 j = 0; j < A.n; j++)
			householder_hv(tau, v, A.row(j));
	}
}


// applies a householder transformation v,tau to a matrix being built up from the identity matrix, using the first column of A as a householder vector
template<typename T> void householder_hm_inplace(T tau, const dynamic_matrix<T> &A) {
	if (tau == 0) {
		A.column(0).slice(1)	= zero;
		A.row(0).slice(1)		= zero;
		A[0][0]					= 1;

	} else {
		// w = A' v
		for (uint32 j = 1; j < A.m; j++) {
			T	wj = dot(A[j].slice(1), A[0].slice(1)) * tau;   // A0j * v0
			// A = A - tau v w'
			A[j][0]			= -wj;
			A[j].slice(1)	-= A[0].slice(1) * wj;
		}

		A[0].slice(1)	*= -tau;
		A[0][0]			= 1 - tau;
	}
}

//-----------------------------------------------------------------------------
//	Givens
//-----------------------------------------------------------------------------

template<typename T> vec<T,2> givens(T a, T b) {
	if (b == 0) {
		return {1, 0};	//c,s
	} else if (abs(b) > abs(a)) {
		T	t	= -a / b;
		T	s1	= rsqrt(1 + square(t));
		return {s1 * t, s1};
	} else {
		T	t	= -b / a;
		T	c1	= rsqrt(1 + square(t));
		return {c1, c1 * t};
	}
}

template<typename T> vec<T,2> schur(T d0, T f0, T d1) {
	if (d0 == 0 || f0 == 0) {
		return {1, 0};
	} else {
		T	tau = (square(f0) + (d1 + d0) * (d1 - d0)) / (2 * d0 * f0);
		T	t	= sign(tau) / (abs(tau) + sqrt(1 + square(tau)));
		T	c1	= rsqrt(1 + square(t));
		return {c1, c1 * t};
	}
}

template<typename T> inline void apply_givens(T &a, T &b, vec<T,2> cs) {
	// Apply rotation to dynamic_vector v' = G^T v
	T	va = a;
	T	vb = b;
	a = cs.x * va - cs.y * vb;
	b = cs.y * va + cs.x * vb;
}

template<typename T> inline void apply_givens(vector_view<T> a, vector_view<T> b, vec<T,2> cs) {
	auto t	= cs.x * a - cs.y * b;
	b		= cs.y * a + cs.x * b;
	a		= t;
}

//-----------------------------------------------------------------------------
//	BIDIAG
//-----------------------------------------------------------------------------

template<typename T> bool bidiag_decomp(dynamic_matrix<T> &A, vector_view<T> tau_U, vector_view<T> tau_V) {
	const int M = A.m;
	const int N = A.n;

	if (N < M || tau_U.n != M || tau_V.n + 1 != M)
		return false;

	for (uint32 i = 0; i < M; i++) {
		// Apply Householder transformation to current column
		auto	v	= A.column(i).slice(i, M - i);
		T	tau	= householder_transform(v);

		// Apply the transformation to the remaining columns
		if (i + 1 < M)
			householder_hm(tau, v, A.sub_matrix(i + 1, i));

		tau_U[i] = tau;

		// Apply Householder transformation to current row
		if (i + 1 < M) {
			auto	v	= A.row(i).slice(i + 1);
			T	tau	= householder_transform(v);

			// Apply the transformation to the remaining rows
			if (i + 1 < N)
				householder_mh(tau, v, A.sub_matrix(i + 1, i + 1));

			tau_V[i] = tau;
		}
	}

	return true;
}

// Form the orthogonal matrices U, V, diagonal d and superdiagonal sd from the packed bidiagonal matrix A
template<typename T> bool bidiag_unpack(const dynamic_matrix<T> &A, const dynamic_vector<T> &tau_U, dynamic_matrix<T> &U, const dynamic_vector<T> &tau_V, dynamic_matrix<T> &V, dynamic_vector<T> &diag, dynamic_vector<T> &superdiag) {
	const uint32 M = A.m;
	const uint32 N = A.n;
	const uint32 K = min(M, N);

	if (N < M || diag.n != K || superdiag.n + 1 != K || tau_U.n != K || tau_V.n + 1 != K || U.m != M || U.n != N || V.m != M || V.n != M)
		return false;

	// Copy diagonals
	diag		= A.diagonal(0);
	superdiag	= A.diagonal(1);

	V = identity;
	for (uint32 i = M - 1; i-- > 0;)
		householder_hm(tau_V[i], A.row(i).slice(i + 1), V.sub_matrix(V, i + 1, i + 1));

	U = identity;
	for (uint32 j = M; j-- > 0;)
		householder_hm(tau_U[j], A.column(j).slice(j), U.sub_matrix(j, j));

	return true;
}

// U in A
// diag in tau_U
// superdiag in tau_v
template<typename T> bool bidiag_unpack_inplace(dynamic_matrix<T> &A, vector_view<T> tau_U, vector_view<T> tau_V, dynamic_matrix<T> &V) {
	const uint32 M = A.m;
	const uint32 N = A.n;
	const uint32 K = min(M, N);

	if (N < M || tau_U.n != K || tau_V.n + 1 != K || V.m != M || V.n != M)
		return false;

	V = identity;
	for (uint32 i = M - 1; i-- > 0;)
		householder_hm(tau_V[i], A.row(i).slice(i + 1), V.sub_matrix(i + 1, i + 1));

	// Copy superdiagonal into tau_v
	tau_V = A.diagonal(1);

	for (uint32 j = M; j-- > 0;) {
		T			tj	= tau_U[j];
		tau_U[j]		= A[j][j];
		householder_hm_inplace(tj, A.sub_matrix(j, j));
	}
	return true;
}

//-----------------------------------------------------------------------------
//	SVD
//-----------------------------------------------------------------------------

#define GSL_DBL_EPSILON	1e-8

template<typename T> void chop_small_elements(vector_view<T> d, vector_view<T> f) {
	T	d_i = d[0];
	for (uint32 i = 0; i < d.n - 1; i++) {
		T	f_i		= f[i];
		T	d_ip1	= d[i + 1];

		if (abs(f_i) < GSL_DBL_EPSILON * (abs(d_i) + abs(d_ip1)))
			f[i] = 0;

		d_i = d_ip1;
	}
}

template<typename T> T trailing_eigenvalue(vector_view<T> d, vector_view<T> f) {
	const uint32 n = d.n;

	T	da = d[n - 2];
	T	db = d[n - 1];
	T	fa = n > 2 ? f[n - 3] : 0;
	T	fb = f[n - 2];

	T	ta = square(da) + square(fa);
	T	tb = square(db) + square(fb);
	T	tab = da * fb;

	T	dt = (ta - tb) / 2;

#if GOLUB_VAN_LOAN_8_3_2
	// Golub and van Loan, Algorithm 8.3.2 (The full SVD algorithm is described in section 8.6.2)
	return dt >= 0
		? tb - (tab * tab) / ( dt + hypot(dt, tab))
		: tb + (tab * tab) / (-dt + hypot(dt, tab));
#else
	// We can compute mu more accurately than using the formula above since we know the roots cannot be negative
	// This also avoids the possibility of NaNs in the formula above.
	// The dynamic_matrix is [ da^2 + fa^2,  da fb      ;
	//					da fb      , db^2 + fb^2 ]
	// and mu is the eigenvalue closest to the bottom right element.
	T	S	= ta + tb;
	T	P	= square(da * db) + square(fa) * (square(db) + square(fb));
	T	D	= hypot(dt, tab);
	T	r1	= S / 2 + D;

	return dt >= 0
		? (r1 > 0 ? P / r1 : 0)		// tb < ta, choose smaller root
		: r1;						// tb > ta, choose larger root
#endif
}

template<typename T> void svd2(vector_view<T> d, vector_view<T> f, dynamic_matrix<T> &U, dynamic_matrix<T> &V) {
	const uint32 M = U.m;
	const uint32 N = V.m;

	T	d0 = d[0];
	T	f0 = f[0];
	T	d1 = d[1];

	if (d0 == 0) {
		// Eliminate off-diagonal element in [0,f0;0,d1] to make [d,0;0,0]
		auto	cs = givens(f0, d1);

		// compute B <= G^T B X,  where X = [0,1;1,0]
		d[0]	= cs.x * f0 - cs.y * d1;
		f[0]	= cs.y * f0 + cs.x * d1;
		d[1]	= 0;

		// Compute U <= U G
		apply_givens(U[0], U[1], cs);

		// Compute V <= V X
		swap(V[0], V[1]);

	} else if (d1 == 0) {
		// Eliminate off-diagonal element in [d0,f0;0,0]
		auto	cs = givens(d0, f0);

		// compute B <= B G
		d[0]	= d0 * cs.x - f0 * cs.y;
		f[0]	= 0;

		// Compute V <= V G
		apply_givens(V[0], V[1], cs);

	} else {
		// Make columns orthogonal, A = [d0, f0; 0, d1] * G
		auto	cs = schur(d0, f0, d1);

		// compute B <= B G
		T	a11 = cs.x * d0 - cs.y * f0;
		T	a21 = -cs.y * d1;
		T	a12 = cs.y * d0 + cs.x * f0;
		T	a22 = cs.x * d1;

		// Compute V <= V G
		apply_givens(V[0], V[1], cs);

		// Eliminate off-diagonal elements, bring column with largest norm to first column
		if (hypot(a11, a21) < hypot(a12, a22)) {
			// B <= B X
			swap(a11, a12);
			swap(a21, a22);
			// V <= V X
			swap(V[0], V[1]);
		}

		cs = givens(a11, a21);

		// compute B <= G^T B
		d[0]	= cs.x * a11 - cs.y * a21;
		f[0]	= cs.x * a12 - cs.y * a22;
		d[1]	= cs.y * a12 + cs.x * a22;

		// Compute U <= U G
		apply_givens(U[0], U[1], cs);
	}
}

template<typename T> void qrstep(vector_view<T> d, vector_view<T> f, dynamic_matrix<T> &U, dynamic_matrix<T> &V) {
	const uint32 M = U.m;
	const uint32 N = V.m;
	const uint32 n = d.n;

	if (n == 1)
		return;  // shouldn't happen

	// Compute 2x2 svd directly
	if (n == 2) {
		svd2(d, f, U, V);
		return;
	}

	// Chase out any zeroes on the diagonal
	for (uint32 i = 0; i < n - 1; i++) {
		if (d[i] == 0) {
			//chase_out_intermediate_zero(d, f, U, i);
			T	x = f[i];
			T	y = d[i + 1];

			for (uint32 k = i; k < n - 1; k++) {
				auto	cs = givens(y, -x);

				// Compute U <= U G
				apply_givens(U[i], U[k + 1], cs);

				// compute B <= G^T B
				d[k + 1]	= cs.y * x + cs.x * y;

				if (k == i)
					f[k]	= cs.x * x - cs.y * y;

				if (k < n - 2) {
					T	z		= f[k + 1];
					f[k + 1]	= cs.x * z;
					x			= -cs.y * z;
					y			= d[k + 2];
				}
			}
			return;
		}
	}

	// Chase out any zeroes at the end of the diagonal
	if (d[n - 1] == 0) {
		//chase_out_trailing_zero(d, f, V);
		const uint32 N = V.m;
		const uint32 n = d.n;

		T	x = d[n - 2];
		T	y = f[n - 2];

		for (uint32 k = n - 1; k-- > 0;) {
			auto	cs = givens(x, y);

			// Compute V <= V G where G = [c, s ; -s, c]
			apply_givens(V[k], V[n - 1], cs);

			// compute B <= B G
			d[k]	= cs.x * x - cs.y * y;

			if (k == n - 2)
				f[k]	= cs.y * x + cs.x * y;

			if (k > 0) {
				T	z		= f[k - 1];
				f[k - 1]	= cs.x * z;
				x			= d[k - 1];
				y			= cs.y * z;
			}
		}
		return;
	}

	// Apply QR reduction steps to the diagonal and offdiagonal
	T	d0	= d[0],	f0 = f[0];
	T	d1	= d[1],	f1 = f[1];

	T	mu	= trailing_eigenvalue(d, f);
	T	y	= d0 * d0 - mu;
	T	z	= d0 * f0;

	// Set up the recurrence for Givens rotations on a bidiagonal dynamic_matrix
	T	bk = 0;
	T	ap = d0;
	T	bp = f0;
	T	aq = d1;

	for (uint32 k = 0; k < n - 1; k++) {
		auto	cs = givens(y, z);

		// Compute V <= V G
		apply_givens(V[k], V[k + 1], cs);

		// compute B <= B G
		if (k > 0)
			f[k - 1] = cs.x * bk - cs.y * z;

		bk	= cs.y * ap + cs.x * bp;
		y	= cs.x * ap - cs.y * bp;
		z	= -cs.y * aq;

		ap	= cs.x * aq;
		bp	= k < n - 2 ? f[k + 1] : 0;

		cs = givens(y, z);

		// Compute U <= U G
		apply_givens(U[k], U[k + 1], cs);

		// compute B <= G^T B
		d[k] = cs.x * y - cs.y * z;
		y	= cs.x * bk - cs.y * ap;
		z	= -cs.y * bp;

		ap	= cs.y * bk + cs.x * ap;
		bp	= cs.x * bp;

		bk	= y;
		aq	= k < n - 2 ? d[k + 2] : 0;
	}

	f[n - 2]	= bk;
	d[n - 1]	= ap;
}

/* Factorise a general M x N dynamic_matrix A into,
 *
 *   A = U D V^T
 *
 * where U is a column-orthogonal M x N matrix (U^T U = I), D is a diagonal N x N matrix, and V is an N x N orthogonal dynamic_matrix (V^T V = V V^T = I)
 *
 * U is stored in the original matrix A, which has the same size
 * V is stored as a separate matrix (not V^T). You must take the transpose to form the product above.
 * The diagonal dynamic_matrix D is stored in the vector S
 */

template<typename T> bool svd(dynamic_matrix<T> &A, dynamic_matrix<T> &V, vector_view<T> &S) {
	const uint32 M = A.m;
	const uint32 N = A.n;
	const uint32 K = min(M, N);

	if (N < M || V.m != M || V.m != V.n || S.n != M)
		return false;

	// Handle the case of N = 1 (SVD of a column dynamic_vector)
	if (N == 1) {
		dynamic_vector<T>	column	= A.column(0);
		T	norm	= len(column);

		S[0]	= norm;
		V[0][0] = 1;

		if (norm)
			column /= norm;

		return true;
	}

	dynamic_vector<T> f(K - 1);

	// bidiagonalize matrix A, unpack A into U S V
	bidiag_decomp(A, S, f);
	bidiag_unpack_inplace(A, S, f, V);

	// apply reduction steps to B=(S,Sd)
	chop_small_elements(S, f);

	// Progressively reduce the dynamic_matrix until it is diagonal
	for (uint32	b = N - 1, iter = 0; b > 0;) {
		T	fbm1 = f[b - 1];

		if (fbm1 == 0 || is_nan(fbm1)) {
			b--;
			continue;
		}

		if (++iter > 100 * N)
			return false;//GSL_ERROR("SVD decomposition failed to converge", GSL_EMAXITER);

		// Find the largest unreduced block (a,b) starting from b and working backwards
		uint32	a;
		for (a = b - 1; a > 0; --a) {
			if (f[a - 1] == 0 || is_nan(f[a - 1]))
				break;
		}

		const uint32		n_block = b - a + 1;
		auto	S_block = S.slice(a, n_block);
		auto	f_block = f.slice(a, n_block - 1);

		dynamic_matrix<T>	U_block = A.sub_matrix(a, 0, n_block, A.n);
		dynamic_matrix<T>	V_block = V.sub_matrix(a, 0, n_block, V.n);

		static constexpr T	sqrt_max	= sqrt(maximum);
		//static constexpr T	sqrt_min	= sqrt(minimum);

		// Temporarily scale the submatrix if necessary
		T	norm	= max(reduce_max_abs(S_block), reduce_max_abs(f_block));
		T	scale	= norm > sqrt_max					? norm / sqrt_max
					: norm * sqrt_max < 1 && norm > 0	? norm * sqrt_max
					:	0;
		if (scale) {
			S_block /= scale;
			f_block	/= scale;
		}

		// Perform the implicit QR step
		qrstep(S_block, f_block, U_block, V_block);
		// remove any small off-diagonal elements
		chop_small_elements(S_block, f_block);

		// Undo the scaling if needed
		if (scale) {
			S_block *= scale;
			f_block	*= scale;
		}

	}

	// Make singular values positive by reflections if necessary
	for (uint32 j = 0; j < K; j++) {
		T	Sj = S[j];
		if (Sj < 0) {
			V[j] = -V[j];
			S[j] = -Sj;
		}
	}

	// Sort singular values into decreasing order
	for (uint32 i = 0; i < K; i++) {
		T		S_max = S[i];
		uint32	i_max = i;

		for (uint32 j = i + 1; j < K; j++) {
			T Sj = S[j];
			if (Sj > S_max) {
				S_max = Sj;
				i_max = j;
			}
		}

		if (i_max != i) {
			// swap eigenvalues
			swap(S[i], S[i_max]);

			// swap eigenvectors
			swap(A[i], A[i_max]);
			swap(V[i], V[i_max]);
		}
	}

	return true;
}

}	// namespace iso

#ifdef ISO_TEST

#include "extra/random.h"
using namespace iso;

static struct test {
	test() {
		dynamic_matrix<float>	A(3, 3);
		rng<simple_random>	random;

		for (int i = 0; i < 3; i++)
			for (int j = 0; j < 3; j++)
				A[i][j] = random.to(10);
		
		dynamic_matrix<float> V(3,3);
		dynamic_vector<float> S(3);
		svd(A, V, S);

	}
} _test;

#endif
