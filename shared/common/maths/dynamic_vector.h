#ifndef DYNAMIC_VECTOR_H
#define DYNAMIC_VECTOR_H

#include "base/vector.h"
#include "base/array.h"

namespace iso {

//-----------------------------------------------------------------------------
//	vector_view
//-----------------------------------------------------------------------------

template<typename T> class vector_view {
protected:
	T				*p;

public:
	uint32			n, stride;

	vector_view(T *p, uint32 n, uint32 stride = 1) : p(p), n(n), stride(stride) {}

	auto	begin()	const	{ return make_stride_iterator(p, stride); }
	auto	end()	const	{ return begin() + n; }
	auto	begin()			{ return make_stride_iterator(p, stride); }
	auto	end()			{ return begin() + n; }

	vector_view		slice(int i, int len)	const	{ return vector_view(p + i * stride, len, stride); }
	vector_view		slice(int i)			const	{ return slice(i, n - i); }

	T&			operator[](int i) {
		return p[i * stride];
	}
	T&	operator[](int i) const {
		return p[i * stride];
	}

	vector_view&	operator=(const T& t) {
		for (int i = 0; i < n; i++)
			p[i * stride] = t;
		return *this;
	}
	template<typename V> enable_if_t<is_vec<V>, vector_view&>	operator=(const V &v) {
		ISO_ASSERT(num_elements_v<V> == n);
		if (stride == 1) {
			*(packed_vec<T, num_elements_v<V>>*)p = to<T>(v);
		} else {
			for (int i = 0; i < n; i++)
				p[i * stride] = v[i];
		}
		return *this;
	}

	template<typename V, typename=enable_if_t<is_vec<V>>>	operator V() const {
		static const int N = num_elements_v<V>;
		ISO_ASSERT(N == n);
		if (stride == 1)
			return to<element_type<V>>(*(packed_vec<T,N>*)p);
		V	v;
		for (int i = 0; i < n; i++)
			v[i] = p[i * stride];
		return v;
	}

	vector_view&		operator=(const vector_view &b) {
		ISO_ASSERT(n == b.n);
		for (int i = 0; i < n; i++)
			(*this)[i] = b[i];
		return *this;
	}
	vector_view&		operator+=(const vector_view &b) {
		ISO_ASSERT(n == b.n);
		for (uint32 i = 0; i < n; i++)
			(*this)[i] += b[i];
		return *this;
	}
	vector_view&		operator-=(const vector_view &b) {
		ISO_ASSERT(n == b.n);
		for (uint32 i = 0; i < n; i++)
			(*this)[i] -= b[i];
		return *this;
	}
	vector_view&		operator*=(T b) {
		for (uint32 i = 0; i < n; i++)
			(*this)[i] *= b;
		return *this;
	}
	vector_view&		operator/=(T b) { return operator*=(1 / b); }

	friend vector_view operator*(T a, const vector_view &b) { return b * a; }
	friend vector_view operator/(const vector_view &a, T b) { return a * (1 / b); }
	friend T	len2(const vector_view &a) {
		T		r	= 0;
		for (int i = 0, n = a.n; i < n; i++)
			r += square(a[i]);
		return r;
	}
	friend T	dot(const vector_view &a, const vector_view &b) {
		ISO_ASSERT(a.n == b.n);
		T		r	= 0;
		for (int i = 0, n = a.n; i < n; i++)
			r += a[i] * b[i];
		return r;
	}
	friend T	dist2(const vector_view &a, const vector_view &b) {
		ISO_ASSERT(a.n == b.n);
		T		r	= 0;
		for (int i = 0, n = a.n; i < n; i++)
			r += square(a[i] - b[i]);
		return r;
	}
	friend int	max_component_index(const vector_view &a) {
		int	m	= 0;
		for (uint32 i = 0; i < a.n; i++) {
			if (a[i] > a[m])
				m = i;
		}
		return m;
	}
	friend int	reduce_max(const vector_view &a)	{
		return a[max_component_index(a)];
	}
	friend int	max_abs_component_index(const vector_view &a) {
		int	m	= 0;
		for (uint32 i = 0; i < a.n; i++) {
			if (abs(a[i]) > abs(a[m]))
				m = i;
		}
		return m;
	}
	friend int	reduce_max_abs(const vector_view &a)	{
		return a[max_abs_component_index(a)];
	}

	friend void swap(const vector_view& a, const vector_view& b) {
		ISO_ASSERT(a.n == b.n);
		for (int i = 0; i < a.n; i++)
			swap(a[i], b[i]);
	}
};

//-----------------------------------------------------------------------------
//	dynamic_vector
//-----------------------------------------------------------------------------

template<typename T> class dynamic_vector : public ref_array<T>, public vector_view<T> {
protected:
	typedef vector_view<T>	B;
public:
	dynamic_vector() : B(nullptr, 0, 0) {}
	dynamic_vector(uint32 n) : ref_array<T>(n), B(*this, n) {}
	dynamic_vector(uint32 n, T value) : ref_array<T>(n), B(*this, n) { for (auto &i : *this) i = value; }
	dynamic_vector(const ref_array<T> &ref, T *p, uint32 n, uint32 stride) : ref_array<T>(ref), B(p, n, stride) {}
	template<typename V, typename=enable_if_t<is_vec<V>>> dynamic_vector(const V &v) : dynamic_vector(num_elements_v<V>) { *(packed_vec<T, num_elements_v<V>>*)this->p = to<T>(v); }

	template<typename X> dynamic_vector& operator=(X &&x)	{ B::operator=(forward<X>(x)); return *this; }
	dynamic_vector&		operator+=(const dynamic_vector &b) { B::operator+=(b); return *this; }
	dynamic_vector&		operator-=(const dynamic_vector &b) { B::operator-=(b); return *this; }
	dynamic_vector&		operator*=(T b)						{ B::operator*=(b); return *this; }
	dynamic_vector&		operator/=(T b)						{ return operator*=(1 / b); }

	friend dynamic_vector operator*(T a, const dynamic_vector &b) { return b * a; }
	friend dynamic_vector operator/(const dynamic_vector &a, T b) { return a * (1 / b); }
	friend void swap(const dynamic_vector& a, const dynamic_vector& b) {
		swap((const B&)a, (const B&)b);
	}

	friend T	dot(const dynamic_vector &a, const dynamic_vector &b) { return dot((const B&)a, (const B&)b); }

};

template<typename T> dynamic_vector<T> reciprocal(const vector_view<T> &a) {
	dynamic_vector<T>	r(a.n);
	for (uint32 i = 0; i < a.n; i++)
		r[i] = 1 / a[i];
	return r;
}
template<typename T> dynamic_vector<T> operator-(const vector_view<T> &a) {
	dynamic_vector<T>	r(a.n);
	for (uint32 i = 0; i < a.n; i++)
		r[i] = -a[i];
	return r;
}
template<typename T> dynamic_vector<T> operator+(const vector_view<T> &a, const vector_view<T> &b) {
	ISO_ASSERT(a.n == b.n);
	uint32	n	= a.n;
	dynamic_vector<T>	r(n);
	for (uint32 i = 0; i < n; i++)
		r[i] = a[i] + b[i];
	return r;
}
template<typename T> dynamic_vector<T> operator-(const vector_view<T> &a, const vector_view<T> &b) {
	ISO_ASSERT(a.n == b.n);
	uint32	n	= a.n;
	dynamic_vector<T>	r(n);
	for (uint32 i = 0; i < n; i++)
		r[i] = a[i] - b[i];
	return r;
}
template<typename T> dynamic_vector<T> operator*(const vector_view<T> &a, const vector_view<T> &b) {
	ISO_ASSERT(a.n == b.n);
	dynamic_vector<T>	r(a.n);
	for (uint32 i = 0; i < a.n; i++)
		r[i] = a[i] * b[i];
	return r;
}
template<typename T> dynamic_vector<T> operator*(const vector_view<T> &a, T b) {
	dynamic_vector<T>	r(a.n);
	for (uint32 i = 0; i < a.n; i++)
		r[i] = a[i] * b;
	return r;
}

//-----------------------------------------------------------------------------
//	dynamic_matrix
//-----------------------------------------------------------------------------

template<typename T, bool=is_vec<T>> class dynamic_matrix;
template<typename T, bool B> static constexpr bool is_mat<dynamic_matrix<T,B>>	= true;

template<typename T> class dynamic_matrix<T, false> {
	template<typename, bool> friend class dynamic_matrix;
protected:
	ref_array<T>	ref;
	T				*p;

	auto		qcolumn(int i)			{ return p + i * stride; }
	auto		qrow(int i)				{ return stride_iterator<T*>(p + i, stride * sizeof(T)); }
	auto		qcolumn(int i)	const	{ return p + i * stride; }
	auto		qrow(int i)		const	{ return stride_iterator<T*>(p + i, stride * sizeof(T)); }

	void		inplace_transpose() {
		ISO_ASSERT(m == n);
		for (int i = 0; i < m; i++) {
			auto	ci = qcolumn(i);
			for (int j = i + 1; j < n; j++)
				swap(ci[j], qcolumn(j)[i]);
		}
	}

public:
	uint32			n, m, stride;
	dynamic_matrix() : p(nullptr), n(0), m(0), stride(0) {}
	dynamic_matrix(uint32 n, uint32 m) : ref(m * n), p(ref), n(n), m(m), stride(n) {}
	dynamic_matrix(const ref_array<T> &ref, T *p, uint32 n, uint32 m, uint32 stride) : ref(ref), p(ref), n(n), m(m), stride(stride) {}

	explicit operator bool() const	{ return !!p; }

	dynamic_matrix&		operator=(const _identity&)	{
		for (uint32 i = 0; i < m; i++) {
			T	*col = qcolumn(i);
			for (uint32 j = 0; j < n; j++)
				col[j] = T(i == j);
		}
		return *this;
	}
	dynamic_matrix&		operator=(const _zero&) {
		for (uint32 i = 0; i < m; i++)
			memset(qcolumn(i), 0, sizeof(T) * n);
		return *this;
	}

	constexpr uint32		rows()					const	{ return n; }
	constexpr uint32		cols()					const	{ return m; }
	const dynamic_vector<T>	column(int i)			const	{ return dynamic_vector<T>(ref, qcolumn(i), n, 1); }
	const dynamic_vector<T>	row(int i)				const	{ return dynamic_vector<T>(ref, p + i, m, stride); }
	const dynamic_vector<T>	diagonal(int i = 0)		const	{ return dynamic_vector<T>(ref, i < 0 ? qcolumn(-i) : p + i, min(m, n) - abs(i), stride + 1); }
	const dynamic_vector<T>	operator[](int i)		const	{ return column(i); }
	dynamic_vector<T>		column(int i)					{ return dynamic_vector<T>(ref, qcolumn(i), n, 1); }
	dynamic_vector<T>		row(int i)						{ return dynamic_vector<T>(ref, p + i, m, stride); }
	dynamic_vector<T>		diagonal(int i = 0)				{ return dynamic_vector<T>(ref, i < 0 ? qcolumn(-i) : p + i, min(m, n) - abs(i), stride + 1); }
	dynamic_vector<T>		operator[](int i)				{ return column(i); }
	T&						operator()(int c, int r)		{ return qcolumn(c)[r]; }
	T						operator()(int c, int r) const	{ return qcolumn(c)[r]; }

	dynamic_matrix			sub_matrix(int i, int j, int n1, int n2)			{ return dynamic_matrix(ref, qcolumn(i) + j, n1, n2, stride); }
	dynamic_matrix			sub_matrix(int i, int j)							{ return sub_matrix(i, j, m - i, n - j); }
	const dynamic_matrix	sub_matrix(int i, int j, int n1, int n2)	const	{ return dynamic_matrix(ref, qcolumn(i) + j, n1, n2, stride); }
	const dynamic_matrix	sub_matrix(int i, int j)					const	{ return sub_matrix(i, j, m - i, n - j); }

	T det() const {
		ISO_ASSERT(n == m);
		if (n == 1)
			return *p;
		if (n == 2)
			return (*this)[0][0] * (*this)[1][1] - (*this)[0][1] * (*this)[1][0];
		T det = 0;
		for (int i = 0; i < m; i++)
			det += plus_minus((*this)[0][i], i) * minor(0, i).det();
		return det;
	}
	dynamic_matrix minor(int col, int row) const {
//		if ((col == 0 || col == m - 1) && (row == 0 || row == n - 1))
//			return sub_matrix(col == 0 ? 1 : 0, row == 0 ? 1 : 0, m - 1, n - 1);

		dynamic_matrix	minor(n - 1, m - 1);
		for (int i = 0; i < m; i++) {
			if (i != col) {
				auto	dcol	= minor.qcolumn(i < col ? i : i - 1);
				auto	scol	= qcolumn(i);
				for (int j = 0; j < row; j++)
					dcol[j] = scol[j];
				for (int j = row + 1; j < n; j++)
					dcol[j - 1] = scol[j];
			}
		}
		return minor;
	}

	friend dynamic_matrix get_transpose(const dynamic_matrix &m) {
		dynamic_matrix	t(m.m, m.n);
		for (int i = 0; i < m.m; i++)
			t.row(i) = m.column(i);
		return t;
	}

	friend dynamic_matrix get_inverse(const dynamic_matrix &m) {
		ISO_ASSERT(m.m == m.n);
		T		rdet	= one / m.det();
		int		n		= m.n;
		dynamic_matrix	inv(n, n);
		for (int i = 0; i < n; i++) {
			for (int j = 0; j < n; j++)
				inv[j][i] = plus_minus(m.minor(i, j).det(), i + j) * rdet;
		}
		return inv;
	}

	template<typename B> friend auto operator+(const dynamic_matrix &a, const dynamic_matrix<B> &b) {
		ISO_ASSERT(a.n == b.n && a.minor == b.m);
		dynamic_matrix	r(a.n, a.m);
		for (int i = 0; i < b.m; i++)
			r[i] = a[i] + b[i];
		return r;
	}
	template<typename B> friend auto operator-(const dynamic_matrix &a, const dynamic_matrix<B> &b) {
		ISO_ASSERT(a.n == b.n && a.m == b.m);
		dynamic_matrix	r(a.n, a.m);
		for (int i = 0; i < b.m; i++)
			r[i] = a[i] - b[i];
		return r;
	}
	template<typename B> friend auto operator*(const dynamic_matrix &m, const dynamic_vector<B> &v) {
		ISO_ASSERT(v.n == m.m);
		dynamic_vector<B>	r(m.n);
		for (int i = 0; i < m.n; i++)
			r[i] = dot(m.row(i), v);
		return r;
	}
	template<typename B> friend auto operator*(const dynamic_vector<B> &v, const dynamic_matrix &m) {
		ISO_ASSERT(v.n == m.n);
		dynamic_vector<B>	r(m.m);
		for (int i = 0; i < m.m; i++)
			r[i] = dot(m.column(i), v);
		return r;
	}
	template<typename B, typename=enable_if_t<is_vec<B>>> friend auto operator*(const dynamic_matrix &m, const B &v) {
		ISO_ASSERT(num_elements_v<B> == m.m);
		dynamic_vector<T>	r(m.n);
		for (int i = 0; i < m.n; i++)
			r[i] = dot((B)m.row(i), v);
		return r;
	}
	template<typename B, typename=enable_if_t<is_vec<B>>> friend auto operator*(const B &v, const dynamic_matrix &m) {
		ISO_ASSERT(num_elements_v<B> == m.n);
		dynamic_vector<T>	r(m.m);
		for (int i = 0; i < m.m; i++)
			r[i] = dot((B)m.column(i), v);
		return r;
	}
	template<typename B> friend auto operator*(const dynamic_matrix &a, const dynamic_matrix<B> &b) {
		ISO_ASSERT(a.m == b.n);
		dynamic_matrix		r(a.n, b.m);
		for (int i = 0; i < b.m; i++)
			r[i] = a * b[i];
		return r;
	}
	template<typename B> friend auto operator*(const dynamic_matrix &a, const scale_s<B> &b) {
		ISO_ASSERT(a.m == b.t.n);
		dynamic_matrix	r(a.n, a.m);
		for (int i = 0; i < a.m; i++)
			r[i] = a[i] * b.t[i];
		return r;
	}

	friend dynamic_matrix operator*(const dynamic_matrix &a, T b) {
		dynamic_matrix	r(a.n, a.m);
		for (uint32 i = 0; i < a.m; i++)
			r[i] = a[i] * b;
		return r;
	}
	friend dynamic_matrix operator*(T a, const dynamic_matrix &b) { return b * a; }
	friend dynamic_matrix operator/(const dynamic_matrix &a, T b) { return a * (1 / b); }
	template<typename B> friend auto to(const dynamic_matrix& a) {
		dynamic_matrix<B>	r(a.n, a.m);
		for (int i = 0; i < a.m; i++)
			r[i] = a[i];
		return r;
	}
	template<typename S> friend auto	operator*(const scale_s<S> &a, const dynamic_matrix &b) {
		dynamic_matrix	r(b.n, b.m);
		for (int i = 0; i < b.m; i++)
			r[i] = b[i] * a.t;
		return r;
	}

};

template<typename T> class dynamic_matrix<T, true> {
protected:
	ref_array<T>	ref;
	T				*p;
	typedef element_type<T>	E;

	auto		qcolumn(int i)			{ return p + i * stride; }
	auto		qrow(int i)				{ return stride_iterator<T*>(p + i, stride * sizeof(T)); }
	auto		qcolumn(int i)	const	{ return p + i * stride; }
	auto		qrow(int i)		const	{ return stride_iterator<T*>(p + i, stride * sizeof(T)); }

public:
	static constexpr uint32 n = num_elements_v<T>;
	uint32			m, stride;
	dynamic_matrix(uint32 m) : ref(m), p(ref), m(m), stride(1) {}
	dynamic_matrix(const ref_array<T> &ref, T *p, uint32 m, uint32 stride) : ref(ref), p(ref), m(m), stride(stride) {}

	dynamic_matrix&		operator=(const _identity&)	{
		T	t = x_axis;
		for (uint32 i = 0; i < m; i++) {
			*qcolumn(i) = t;
			t >>= 1;
		}
		return *this;
	}
	dynamic_matrix&		operator=(const _zero&) {
		for (uint32 i = 0; i < m; i++)
			*qcolumn(i) = zero;
	}

	constexpr uint32		rows()				const	{ return n; }
	constexpr uint32		cols()				const	{ return m; }
	T						column(int i)		const	{ return *qcolumn(i); }
	const dynamic_vector<E>	row(int i)			const	{ return dynamic_vector<E>(ref, (E*)p + i, m, stride * sizeof(T) / sizeof(E)); }
	const dynamic_vector<T>	diagonal(int i = 0)	const	{ return dynamic_vector<T>(ref, i < 0 ? (E*)qcolumn(-i) : (E*)p + i, min(m, n) - abs(i), stride * sizeof(T) / sizeof(E) + 1); }
	T						operator[](int i)	const	{ return column(i); }
	T&						column(int i)				{ return *qcolumn(i); }
	dynamic_vector<E>		row(int i)					{ return dynamic_vector<E>(ref, (E*)p + i, m, stride * sizeof(T) / sizeof(E)); }
	dynamic_vector<T>		diagonal(int i = 0)			{ return dynamic_vector<T>(ref, i < 0 ? (E*)qcolumn(-i) : (E*)p + i, min(m, n) - abs(i), stride * sizeof(T) / sizeof(E) + 1); }
	T&						operator[](int i)			{ return column(i); }

	//dynamic_matrix		sub_matrix(int i, int j, int n1, int n2)			{ return dynamic_matrix(ref, qcolumn(i) + j, n1, n2, stride); }
	//dynamic_matrix		sub_matrix(int i, int j)							{ return submatrix(i, j, m - i, n - j); }
	//const dynamic_matrix	sub_matrix(int i, int j, int n1, int n2)	const	{ return dynamic_matrix(ref, qcolumn(i) + j, n1, n2, stride); }
	//const dynamic_matrix	sub_matrix(int i, int j)					const	{ return submatrix(i, j, m - i, n - j); }

	E det() const {
		ISO_ASSERT(n == m);
		E det = 0;
		for (int i = 0; i < m; i++)
			det += plus_minus((*this)[0][i], i) * minor(0, i).det();
		return det;
	}

	auto minor(int col, int row) const {
		dynamic_matrix<E>	minor(n - 1, m - 1);
		for (int i = 0; i < m; i++) {
			if (i != col) {
				auto	dcol	= minor.qcolumn(i < col ? i : i - 1);
				auto	scol	= (E*)qcolumn(i);
				for (int j = 0; j < row; j++)
					dcol[j] = scol[j];
				for (int j = row + 1; j < n; j++)
					dcol[j - 1] = scol[j];
			}
		}
		return minor;
	}

	friend auto get_transpose(const dynamic_matrix &m) {
		dynamic_matrix<E>	t(m.m, m.n);
		for (int i = 0; i < m.m; i++)
			t.row(i) = m.column(i);
		return t;
	}

	friend dynamic_matrix get_inverse(const dynamic_matrix &m) {
		ISO_ASSERT(m.m == m.n);
		auto	rdet	= one / m.det();
		int		n		= m.n;
		dynamic_matrix	inv(n);
		// minors and cofactors
		for (int i = 0; i < n; i++) {
			for (int j = 0; j < n; j++)
				inv[j][i] = plus_minus(m.minor(i, j).det(), i + j) * rdet;
		}
		return inv;
	}

	template<typename B> friend auto operator+(const dynamic_matrix &a, const dynamic_matrix<B> &b) {
		ISO_ASSERT(a.n == b.n && a.minor == b.m);
		dynamic_matrix	r(a.m);
		for (int i = 0; i < b.m; i++)
			r[i] = a[i] + b[i];
		return r;
	}
	template<typename B> friend auto operator-(const dynamic_matrix &a, const dynamic_matrix<B> &b) {
		ISO_ASSERT(a.n == b.n && a.m == b.m);
		dynamic_matrix	r(a.m);
		for (int i = 0; i < b.m; i++)
			r[i] = a[i] - b[i];
		return r;
	}
	
	template<typename B, typename=enable_if_t<is_vec<B>>> friend T operator*(const dynamic_matrix &m, const B &v) {
		ISO_ASSERT(num_elements_v<B> == m.n);
		T	r = zero;
		for (int i = 0; i < m.n; i++)
			r += m[i] * v[i];
		return r;
	}
	friend auto operator*(const T &v, const dynamic_matrix &m) {
		dynamic_vector<E>	r(m.m);
		for (int i = 0; i < m.m; i++)
			r[i] = dot(m.column(i), v);
		return r;
	}
	template<typename B> friend T operator*(const dynamic_matrix &m, const dynamic_vector<B> &v) {
		ISO_ASSERT(v.n == m.m);
		T	r	= zero;
		for (int i = 0; i < v.n; i++)
			r += m[i] * v[i];
		return r;
	}

	template<typename B> friend auto operator*(const dynamic_matrix &a, const dynamic_matrix<B> &b) {
		ISO_ASSERT(a.m == b.n);
		dynamic_matrix		r(b.m);
		for (int i = 0; i < b.m; i++)
			r[i] = a * b[i];
		return r;
	}
	template<typename B> friend auto operator*(const dynamic_matrix &a, const scale_s<B> &b) {
		ISO_ASSERT(a.m == b.t.n);
		dynamic_matrix	r(a.m);
		for (int i = 0; i < a.m; i++)
			r[i] = a[i] * b.t[i];
		return r;
	}

	friend dynamic_matrix operator*(const dynamic_matrix &a, E b) {
		dynamic_matrix	r(a.m);
		for (uint32 i = 0; i < a.n; i++)
			r[i] = a[i] * b;
		return r;
	}
	friend dynamic_matrix operator*(E a, const dynamic_matrix &b) { return b * a; }
	friend dynamic_matrix operator/(const dynamic_matrix &a, E b) { return a * (1 / b); }

	template<typename B> friend auto to(const dynamic_matrix& a) {
		dynamic_matrix<vec<B, num_elements_v<T>>>	r(a.m);
		for (int i = 0; i < a.m; i++)
			r[i] = to<B>(a[i]);
		return r;
	}
};


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
		S[i] = len(U.column(i));		// singular value is norm of column vector of U
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
			swap_ranges(U.column(i0),  U.column(i1));
			swap_ranges(VT.column(i0), VT.column(i1));
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

} //namespace iso
#endif // DYNAMIC_VECTOR_H
