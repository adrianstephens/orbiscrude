#include "base/defs.h"
#include "base/array.h"
#include "base/algorithm.h"
#include "base/maths.h"
#include "dynamic_vector.h"

using namespace iso;

//typedef float F;
#define TOL F(0.00000000000000000001)

// Enum type that describes the two types of data used to perform permutations: permutation matrices or permutation vectors (explained later)
enum PermutationFormat {
	PF_MATRIX,
	PF_VECTOR
};

template<typename F> class AnonymousMatrix;

template<typename F> class LPmatrix : dynamic_matrix<F> {
	typedef	dynamic_matrix<F>	B;
	friend AnonymousMatrix;
protected:
	mutable F		det;
	mutable bool	determinant_up_to_date;

public:
	LPmatrix(int r, int c)		: B(r, c),	determinant_up_to_date(false)	{}
	LPmatrix(int r, int c, F v)	: B(r, c, v),	determinant_up_to_date(false)	{}
	LPmatrix()					: LPmatrix(0, 0) {}
	LPmatrix(int n)				: LPmatrix(n, n) {}
	LPmatrix(int n, F v)		: LPmatrix(n, n, v) {}
	LPmatrix(AnonymousMatrix m);

	pair<int, int> dim()		const { return make_pair(rows, columns); }
	bool	is_square()			const { return (rows == columns); }
	bool	more_equal_than(F value, F tol) const;
	bool	less_equal_than(F value, F tol) const;

	//Mathematical and manipulation operators
	AnonymousMatrix operator-(AnonymousMatrix m) const;
	AnonymousMatrix operator+(AnonymousMatrix m) const;
	void	swap_columns(int r, int w);
	void	swap_rows(int r, int w);
	void	set_identity();
	void	transpose();

	void	resize(int r, int c)	{ values = storage((rows = r) * (columns = c)); determinant_up_to_date = false; }
	void	empty()					{ values = storage(rows * columns); }
	F		determinant() const		{ if (!determinant_up_to_date) { LPmatrix l, u, p; get_lupp(l, u, p, PF_VECTOR); } return det; }

	//Factorizations and inverses

	void		get_lupp(LPmatrix& l, LPmatrix& u, LPmatrix& p, PermutationFormat pf = PF_MATRIX) const;
	void		get_inverse(LPmatrix& inverse) const;
	static void get_inverse_with_column(LPmatrix const& old_inverse, LPmatrix const& new_column, int column_index, LPmatrix& new_inverse);
	void		solve(LPmatrix& x, LPmatrix const& b) const;

	//Multiplication operators
	AnonymousMatrix operator*(AnonymousMatrix m) const;
	AnonymousMatrix operator*(LPmatrix const& m);
	LPmatrix&		operator*=(LPmatrix const& m);
	LPmatrix&		operator*=(AnonymousMatrix m);

	//Assignments
	LPmatrix&		operator=(LPmatrix const& m);
	LPmatrix&		operator=(AnonymousMatrix m);

	//Retrieval and cast operators
	F&			operator()(int i)				{ ISO_ASSERT(rows == 1 || columns == 1); return values[i]; }
	F const&	operator()(int i) const			{ ISO_ASSERT(rows == 1 || columns == 1); return values[i]; }
	F&			operator()(int r, int c)		{ ISO_ASSERT(r < rows && c < columns); return values[r * columns + c]; }
	F const&	operator()(int r, int c) const	{ ISO_ASSERT(r < rows && c < columns); return values[r * columns + c]; }
	F&			at(int r, int c)				{ return values[r * columns + c]; }
	F const&	at(int r, int c) const			{ return values[r * columns + c]; }
	operator F()						const	{ ISO_ASSERT(rows == 1 && columns == 1); return values[0];}
};

template<typename F> class AnonymousMatrix : public LPmatrix<F> {
	typedef	LPmatrix<F>	B;
public:
	AnonymousMatrix(int r, int c) : B(r, c, 0) {}
	AnonymousMatrix(const AnonymousMatrix& m) : B(m.n, m.m) { swap(LPmatrix::values, const_cast<storage&>(b.values)); }
	AnonymousMatrix(const LPmatrix& b) : B(b.n, b.m)		{ memcpy(values, b.values, b.rows * b.columns * sizeof(F)); }
	AnonymousMatrix operator*(const B& b) {
		ISO_ASSERT(m == b.n);
		// Allocate return matrix filled with zeroes
		AnonymousMatrix r(rows, b.columns);
		for (int i = 0; i < rows; ++i)
			for (int j = 0; j < b.columns; ++j)
				for (int h = 0; h < columns; ++h)
					r(i, j) += at(i, h) * b(h, j);

		rows		= r.rows;
		columns		= r.columns;
		swap(values, r.values);
		return *this;
	}
};

LPmatrix::LPmatrix(AnonymousMatrix m) : rows(m.rows), columns(m.columns), determinant_up_to_date(false) { swap(values, m.values); }

AnonymousMatrix LPmatrix::operator*(LPmatrix const& m) {
	ISO_ASSERT(columns == m.rows);
	AnonymousMatrix r(rows, m.columns);
	for (int i = 0; i < rows; ++i)
		for (int j = 0; j < m.columns; ++j)
			for (int h = 0; h < columns; ++h)
				r(i, j) += at(i, h) * m(h, j);
	return r;
}

LPmatrix& LPmatrix::operator*=(LPmatrix const& m) {
	ISO_ASSERT(columns == m.rows);
	// No fear to change matrix size
	LPmatrix r(rows, m.columns);
	for (int i = 0; i < rows; ++i)
		for (int j = 0; j < m.columns; ++j)
			for (int h = 0; h < columns; ++h)
				r(i, j) += at(i, h) * m(h, j);

	swap(values, r.values);
	rows	= r.rows;
	columns = r.columns;
	return *this;
}

LPmatrix& LPmatrix::operator*=(AnonymousMatrix m) {
	ISO_ASSERT(columns == m.rows);
	// No fear to change matrix size
	LPmatrix r(rows, m.columns);
	for (int i = 0; i < rows; ++i)
		for (int j = 0; j < m.columns; ++j)
			for (int h = 0; h < columns; ++h)
				r(i, j) += at(i, h) * m(h, j);

	swap(values, r.values);
	rows	= r.rows;
	columns = r.columns;
	return *this;
}

AnonymousMatrix LPmatrix::operator*(AnonymousMatrix m) const {
	ISO_ASSERT(columns == m.rows);
	// Allocate return matrix filled with zeroes
	AnonymousMatrix r(rows, m.columns);
	for (int i = 0; i < rows; ++i)
		for (int j = 0; j < m.columns; ++j)
			for (int h = 0; h < columns; ++h)
				r(i, j) += at(i, h) * m(h, j);

	swap(m.values, r.values);
	m.rows		= r.rows;
	m.columns	= r.columns;
	return m;
}

LPmatrix& LPmatrix::operator=(LPmatrix const& m) {
	if (&m != this) {
		rows	= m.rows;
		columns = m.columns;
		values	= m.values;
	}
	return *this;
}

LPmatrix& LPmatrix::operator=(AnonymousMatrix m) {
	// Swap values, m will destroy old LPmatrix values
	swap(values, m.values);
	rows	= m.rows;
	columns = m.columns;
	return *this;
}

AnonymousMatrix LPmatrix::operator-(AnonymousMatrix m) const {
	ISO_ASSERT(dim() == m.dim());
	AnonymousMatrix r(rows, columns);
	for (int i = 0; i < rows; ++i)
		for (int j = 0; j < columns; ++j)
			r(i, j) = at(i, j) - m(i, j);
	return r;
}

AnonymousMatrix LPmatrix::operator+(AnonymousMatrix m) const {
	ISO_ASSERT(dim() == m.dim());
	AnonymousMatrix r(rows, columns);
	for (int i = 0; i < rows; ++i)
		for (int j = 0; j < columns; ++j)
			r(i, j) = at(i, j) + m(i, j);
	return r;
}

bool LPmatrix::more_equal_than(F value, F tol) const {
	for (int i = 0; i < rows; ++i)
		for (int j = 0; j < columns; ++j)
			if (at(i, j) + tol < value)
				return false;
	return true;
}

bool LPmatrix::less_equal_than(F value, F tol) const {
	for (int i = 0; i < rows; ++i)
		for (int j = 0; j < columns; ++j)
			if (at(i, j) - tol > value)
				return false;
	return true;
}

void LPmatrix::swap_columns(int r, int w) {
	for (int i = 0; i < rows; ++i)
		swap(values[i * columns + r], values[i * columns + w]);
	det = -det;
}

void LPmatrix::swap_rows(int r, int w) {
	for (int i = 0; i < columns; ++i)
		swap(values[r * columns + i], values[w * columns + i]);
	det = -det;
}

void LPmatrix::set_identity() {
	ISO_ASSERT(is_square());
	for (int i = 0; i < rows; ++i)
		for (int j = 0; j < columns; ++j)
			at(i, j) = i == j;
	det = 1;
	determinant_up_to_date = true;
}

void LPmatrix::transpose() {
	// If matrix is square just swap the elements
	if (rows == columns) {
		for (int j = 1; j < columns; ++j)
			for (int i = 0; i < j; ++i)
				swap(at(i, j), at(j, i));
	} else {
		AnonymousMatrix tps(columns, rows);
		for (int i = 0; i < rows; ++i)
			for (int j = 0; j < columns; ++j)
				tps(j, i) = at(i, j);

		*this = tps;
	}
}

// LU factorization with Gaussian Elimination and Partial Pivoting
// stores the l, u matrices and permutation data respective into l, u and p
// The third parameter determines the format of permutation data p (vector or matrix)
void LPmatrix::get_lupp(LPmatrix& l, LPmatrix& u, LPmatrix& p, PermutationFormat pf) const {
	ISO_ASSERT(is_square());

	F determinant = 1;

	// Initialize passed u
	u = AnonymousMatrix(*this);												// u will evolve from the original matrix

	// Resize passed l
	l.resize(rows, columns);												// total l
	l.set_identity();

	// Initialize permutation matrix (if needed)
	if (pf == PF_MATRIX) {													// total p
		p.resize(rows, columns);
		p.set_identity();
	}

	// p_vector and o_vector for efficient permutation handling
	AnonymousMatrix p_vector(rows, 1);
	AnonymousMatrix o_vector(rows, 1);

	// Initialize p_vector, o_vector
	for (int i = 0; i < rows; ++i) {
		p_vector(i) = i;
		o_vector(i) = i;
	}

	for (int j = 0; j < columns; ++j) {
		// Reset tem and tpm elements
		LPmatrix tem(rows, 1, 0);
		tem(j) = 1;

		// Write tpm:
		//	*	find absolute maximum element j in column i
		//	*	swap row i with row j in p and u, swap columns in l

		int	column_max_position = j;
		F	max = u(column_max_position, j);

		// Partial pivoting process
		for (int i = j; i < rows; ++i)
			if (abs(u(i, j)) > abs(max)) {
				column_max_position = i;
				max = u(i, j);
			}

		// If matrix is not singular proceed ..
		ISO_ASSERT(max != 0);

		// Update U and P with TPM only if necessary
		if (j != column_max_position) {

			// Update determinant sign
			determinant = -determinant;

			// Effects of permutation on l and u
			u.swap_rows(j, column_max_position);
			l.swap_columns(j, column_max_position);

			// Effects on permutation on p and p_vector
			if (pf == PF_MATRIX)
				p.swap_rows(j, column_max_position);
			p_vector.swap_rows(j, column_max_position);

			// If we're returning the PF_MATRIX set its determinant
			if (pf == PF_MATRIX) {
				p.det	= determinant;
				p.determinant_up_to_date = true;
			}
		}

		// Write tem vector for current column
		for (int i = j + 1; i < rows; ++i)
			tem(i) = -(u(i, j) / max);

		// Optimization of l * tem that takes into account the shape of l and tem
		for (int i = 0; i < rows; ++i) {
			F inv_product = l(i, j);	// because tem(j,0) == 1

			for (int k = j + 1; k < columns; ++k)
				inv_product += l(i, k) * -tem(k);

			l(i, j) = inv_product;
		}

		// Optimization of tem * u that takes into account the shape of tem and u
		for (int i = j + 1; i < rows; ++i) {
			LPmatrix r(1, columns, 0);

			for (int o = j; o < columns; ++o)
				r(o) = tem(i) * u(j, o) + u(i, o);

			for (int k = j; k < columns; ++k)
				u(i, k) = r(k);
		}
	}

	// Optimized way to calculate p * l, a permutation vector is used to swap the rows of l
	for (int i = 0; i < rows; ++i)
		while (p_vector(i) != o_vector(i)) {
			int		k = i + 1;
			while (p_vector(k) != o_vector(i))
				k++;

			o_vector.swap_rows(i, k);
			l.swap_rows(i, k);
		}

	// Return PF_VECTOR in p
	if (pf == PF_VECTOR)
		p = p_vector;

	// Compute and set determinant
	for (int i = 0; i < rows; ++i)
		determinant *= u(i, i);
	determinant_up_to_date = true;
}

void LPmatrix::get_inverse(LPmatrix& inverse) const {
	ISO_ASSERT(is_square());

	// Adjust inverse size
	inverse.resize(rows, columns);

	// Temporary matrices to hold factorization products
	LPmatrix l_inverse, u_inverse, p_vector;

	// Compute and store LUPP
	get_lupp(l_inverse, u_inverse, p_vector, PF_VECTOR);

	// Set original permutation vector
	LPmatrix o_vector(rows, 1);
	for (int i = 0; i < rows; ++i)
		o_vector(i) = i;

	// Copy transposed l
	l_inverse.transpose();

	// Set reciprocals on the diagonal of u, useless in l since they are ones
	for (int i = 0; i < rows; ++i)
		u_inverse(i, i) = 1 / u_inverse(i, i);

	// Calculate inverse of l
	for (int i = 1; i < rows; ++i)
		for (int j = i - 1; j >= 0; --j) {
			F dot_product = 0;
			for (int k = i; k > 0; --k)
				dot_product += l_inverse(i, k) * l_inverse(j, k);
			l_inverse(i, j) = -dot_product;			// Optimization of dot_product * - l_inverse.at(j,j)
		}

	// Set zeroes on the upper half of l^-1
	for (int i = 0; i < rows; ++i)
		for (int j = i + 1; j < columns; ++j)
			l_inverse(i, j) = 0;

	// Calculate inverse of u
	for (int i = 1; i < rows; ++i)
		for (int j = i - 1; j >= 0; --j) {
			F dot_product = 0;
			for (int k = i; k > 0; --k) {
				dot_product += u_inverse(i, k) * u_inverse(j, k);
			}
			u_inverse(i, j) = dot_product * -u_inverse(j, j);
		}

	u_inverse.transpose();

	// Set zeroes on the lower half of u^-1
	for (int j = 0; j < columns; ++j)
		for (int i = j + 1; i < rows; ++i)
			u_inverse(i, j) = 0;

	// Optimization of u^-1 * l^-1 that takes into account the shape of the two matrices
	for (int i = 0; i < rows; ++i)
		for (int j = 0; j < columns; ++j)
			for (int h = columns - 1; h >= min(i, j); --h)
				inverse(i, j) += u_inverse(i, h) * l_inverse(h, j);

	// Smart way to translate a row permutation vector to obtain a column permutation vector
	LPmatrix p_vector_t(rows, 1);
	for (int i = 0; i < rows; ++i)
		p_vector_t(p_vector(i)) = i;

	// Optimization of columns permutation
	for (int i = 0; i < rows; ++i)
		while (p_vector_t(i) != o_vector(i)) {
			int		k = i + 1;
			while (p_vector_t(k) != o_vector(i))
				k++;

			o_vector.swap_rows(i, k);
			inverse.swap_columns(i, k);
		}
}

void LPmatrix::get_inverse_with_column(LPmatrix const& old_inverse, LPmatrix const& new_column, int q, LPmatrix& new_inverse) {
	new_inverse.resize(old_inverse.rows, old_inverse.columns);
	LPmatrix a_tilde(old_inverse * new_column);

	for (int i = 0; i < old_inverse.rows; ++i)
		for (int j = 0; j < old_inverse.columns; ++j)
			if (i != q)
				new_inverse(i, j) = old_inverse(i, j) - old_inverse(q, j) * a_tilde(i) / a_tilde(q);
			else
				new_inverse(i, j) = old_inverse(q, j) / a_tilde(q);
}

void LPmatrix::solve(LPmatrix& x, LPmatrix const& b) const {
	// Invert L and U
	LPmatrix l_inverse, u_inverse, p_vector;

	// Calculate LUPP factorization
	get_lupp(l_inverse, u_inverse, p_vector, PF_VECTOR);

	// Copy transposed l
	l_inverse.transpose();

	// Set reciprocals on the diagonal of u (useless in l since diagonal elements are ones)
	for (int i = 0; i < rows; ++i)
		u_inverse(i, i) = 1 / u_inverse(i, i);

	// Calculate inverse of l
	for (int i = 1; i < rows; ++i)
		for (int j = i - 1; j >= 0; --j) {
			F dot_product = 0;
			for (int k = i; k > 0; --k)
				dot_product += l_inverse(i, k) * l_inverse(j, k);
			l_inverse(i, j) = -dot_product;			// Optimization due to ones on diagonal
		}

	// Set zeroes on the upper half of l^-1
	for (int i = 0; i < rows; ++i)
		for (int j = i + 1; j < columns; ++j)
			l_inverse(i, j) = 0;

	// Calculate inverse of u
	for (int i = 1; i < rows; ++i)
		for (int j = i - 1; j >= 0; --j) {
			F dot_product = 0;
			for (int k = i; k > 0; --k) {
				dot_product += u_inverse(i, k) * u_inverse(j, k);
			}
			u_inverse(i, j) = dot_product * -u_inverse(j, j);
		}

	u_inverse.transpose();

	// Set zeroes on the lower half of u^-1
	for (int j = 0; j < columns; ++j)
		for (int i = j + 1; i < rows; ++i)
			u_inverse(i, j) = 0;

	// Optimization of p * b
	LPmatrix pb(rows, 1);
	for (int i = 0; i < rows; ++i)
		pb(i) = b(p_vector(i));

	// Set x shape
	x.resize(rows, 1);

	// Optimization of x = l_inverse * pb;
	for (int i = 0; i < rows; ++i) {
		F dot_product = pb(i);
		for (int j = 0; j < i; ++j) {
			dot_product += l_inverse(i, j) * pb(j);
		}
		x(i) = dot_product;
	}

	// Optimization of x = u_inverse * x
	for (int i = 0; i < rows; ++i) {
		F dot_product = 0;
		for (int j = columns - 1; j >= i; --j)
			dot_product += u_inverse(i, j) * x(j);
		x(i) = dot_product;
	}
}

//-----------------------------------------------------------------------------
//	ColumnSet
//-----------------------------------------------------------------------------

class Simplex;

class ColumnSet {
	// A int vector stores the indices of the columns in the set.
	dynamic_array<int> columns;
	friend Simplex;
public:
	void insert(int column) {
		columns.push_back(column);
	}
	void remove(int column) {
		if (find(columns.begin(), columns.end(), column) != columns.end())
			columns.erase(find(columns.begin(), columns.end(), column));
	}
	void substitute(int old_column, int new_column) {
		if (find(columns.begin(), columns.end(), old_column) != columns.end())
			*(find(columns.begin(), columns.end(), old_column)) = new_column;
	}
	bool contains(int n) const {
		return find(columns.begin(), columns.end(), n) != columns.end();
	}
	int& column(int idx) {
		return columns.at(idx);
	}
	int	index_of(int column) {
		int pos = 0;
		for (auto it = columns.begin(); it != columns.end(); ++it)
			if (*it != column)
				++pos;
			else
				return pos;
		return -1;
	}
	uint32 size() const {
		return columns.size32();
	}
};

//-----------------------------------------------------------------------------
//	Constraint	- A_i * x_j = b_i.
//-----------------------------------------------------------------------------

class Constraint {
	friend Simplex;

	enum Type {
		CT_LESS_EQUAL,
		CT_MORE_EQUAL,
		CT_EQUAL,
		CT_NON_NEGATIVE,
		CT_BOUNDS
	};
	Type	type;
	LPmatrix	coefficients;
	F		value;
	F		upper;
	F		lower;

public:
	Constraint(LPmatrix const & coefficients, Type type, F value) : type(type), coefficients(coefficients), value(value) {
		// Coefficients must be a row vector
		ISO_ASSERT(coefficients.dim().a == 1);
	}

	Constraint(LPmatrix const & coefficients, Type type, F lower, F upper) : type(type), coefficients(coefficients), lower(lower), upper(upper) {
		// Coefficients must be a row vector
		ISO_ASSERT(type == CT_BOUNDS && coefficients.dim().a == 1);
	}

	void add_column(F value) {
		AnonymousMatrix row(1, coefficients.dim().b + 1);
		for (int i = 0; i < coefficients.dim().b; ++i)
			row(i) = coefficients(i);

		row(coefficients.dim().b) = value;
		coefficients = row;
	}
	int size() const {
		return coefficients.dim().b;
	}
};

//-----------------------------------------------------------------------------
// ObjectiveFunction
//-----------------------------------------------------------------------------

class ObjectiveFunction {
	friend Simplex;
	enum Type {
		OFT_MAXIMIZE,
		OFT_MINIMIZE
	};
	Type	type;
	LPmatrix	costs;
public:
	ObjectiveFunction() {}
	ObjectiveFunction(Type type, LPmatrix const &costs) : type(type), costs(costs) {}
	ObjectiveFunction& operator=(ObjectiveFunction const &objective_function) {
		type	= objective_function.type;
		costs	= objective_function.costs;
		return *this;
	}
	// Solution value
	LPmatrix const & get_value(LPmatrix const & x) const {
		return costs * x;
	}

	// Manipulation
	void add_column(F value) {
		AnonymousMatrix row(1, costs.dim().b + 1);
		for (int i = 0; i < costs.dim().b; ++i)
			row(i) = costs(i);

		row(costs.dim().b) = value;
		costs = row;
	}
};

//-----------------------------------------------------------------------------
// Variable
//-----------------------------------------------------------------------------

class Variable {
	friend Simplex;
	Simplex * creator;

public:
	Variable(Simplex *creator) : creator(creator) {}
	virtual ~Variable()	{}
	virtual void process(LPmatrix& calculated_solution, LPmatrix& solution, int index) {
		solution(index) = calculated_solution(index);
	}
};

//-----------------------------------------------------------------------------
//	AuxiliaryVariable
//	created when transforming a variable in a splitted variable
//	The relation: x = x+ - x- holds between the original variable, the SplittedVariable and the AuxiliaryVariable
//-----------------------------------------------------------------------------
class AuxiliaryVariable : public Variable {
	friend Simplex;
	friend class SplittedVariable;
	int index;
public:
	AuxiliaryVariable(Simplex* creator, int index) : Variable(creator), index(index) {}
	void process(LPmatrix& calculated_solution, LPmatrix& solution, int index) {}
};

//-----------------------------------------------------------------------------
//	SplittedVariable
//	Variables that are splitted in two (one) AuxiliaryVariables during the translation in standard form
//-----------------------------------------------------------------------------
class SplittedVariable : public Variable {
	friend Simplex;
	AuxiliaryVariable* aux;
public:
	SplittedVariable(Simplex* creator, AuxiliaryVariable* aux) : Variable(creator), aux(aux) {}
	void process(LPmatrix& calculated_solution, LPmatrix& solution, int index) {
		solution(index) = calculated_solution(index) - calculated_solution(aux->index);
	}
};

//-----------------------------------------------------------------------------
//	SlackVariable
//	Type of variable added when transforming a <= or >= constraint into a = constraint
//-----------------------------------------------------------------------------
class SlackVariable : public Variable {
	friend Simplex;
public:
	SlackVariable(Simplex * creator) : Variable(creator) {}
	void process(LPmatrix& calculated_solution, LPmatrix& solution, int index)	{}
};

//-----------------------------------------------------------------------------
//	Simplex
//-----------------------------------------------------------------------------

class Simplex {
protected:
	// Column sets
	ColumnSet suggested_base;
	ColumnSet current_base;
	ColumnSet current_out_of_base;

	// Data
	ObjectiveFunction			objective_function;
	dynamic_array<Constraint>	constraints;
	dynamic_array<Constraint>	nn_constraints;
	dynamic_array<Variable*>	variables;

	// Processed data
	LPmatrix	costs;
	LPmatrix	coefficients_matrix;
	LPmatrix	constraints_vector;
	LPmatrix	base_inverse;
	LPmatrix	dual_variables;
	LPmatrix	column_p;
	int		solution_dimension, old_column;

	// Results
	LPmatrix	base_solution;
	LPmatrix	solution;
	LPmatrix	reduced_cost;
	F		solution_value;

	bool	optimal, unlimited, overconstrained, has_to_be_fixed, changed_sign;
	int		inverse_recalculation_rate;

	// Preprocessing
	void process_to_standard_form();
	void process_to_artificial_problem();

	// Solving
	void solve_with_base(ColumnSet const& base);

public:
	Simplex() : solution_dimension(0), changed_sign(false), inverse_recalculation_rate(10) {}
	~Simplex() {
		for (auto &i : variables)
			if (i->creator == this)
				delete &i;
	}

	// Settings
	void add_variable(Variable* variable) {
		variables.push_back(variable);
	}
	void add_constraint(Constraint const & constraint) {
		if (constraints.size() != 0)
			ISO_ASSERT(solution_dimension == constraint.coefficients.dim().b);
		else
			solution_dimension = constraint.size();

		if (constraint.type == Constraint::CT_NON_NEGATIVE)
			nn_constraints.push_back(constraint);
		else
			constraints.push_back(constraint);
	}
	void set_objective_function(ObjectiveFunction const &of) {
		ISO_ASSERT(solution_dimension == of.costs.dim().b);
		objective_function = of;
	}

	// Solving procedures
	void solve();

	bool is_unlimited()					const { return unlimited; }
	bool has_solutions()				const { return !overconstrained; }
	bool must_be_fixed()				const { return has_to_be_fixed; }
	LPmatrix const &get_dual_variables()	const { return dual_variables; }
};

void Simplex::process_to_standard_form() {
	// Constraint iterator

	// Process non-negative constraints
	int initial_solution_dimension = solution_dimension;
	for (int i = 0; i < initial_solution_dimension; ++i) {	// For each component of x
		bool has_constraint = false;

		// Find an x that doesn't have a non-negativity constraint on it
		for (auto it = nn_constraints.begin(); it != nn_constraints.end() && !has_constraint; ++it)
			if (it->coefficients(i) == 1)
				has_constraint = true;

		if (!has_constraint) {
			// Add a non-negativity constraint
			LPmatrix eye(1, solution_dimension);
			eye(i) = 1;
			add_constraint(Constraint(eye, Constraint::CT_NON_NEGATIVE, 0));

			++solution_dimension;

			// Add a column to all constraints
			for (Constraint *mit = nn_constraints.begin(); mit != nn_constraints.end(); ++mit)
				mit->add_column(0);

			// Add another non-negativity constraint
			LPmatrix n_eye(1, solution_dimension);
			n_eye(solution_dimension - 1) = 1;

			this->add_constraint(Constraint(n_eye, Constraint::CT_NON_NEGATIVE, 0));

			// Add a regular constraint
			for (Constraint *mit = constraints.begin(); mit != constraints.end(); ++mit)
				mit->add_column(-mit->coefficients(i));

			objective_function.add_column(-objective_function.costs(i));

			// Update variables status
			Variable* auxiliary = new AuxiliaryVariable(this, variables.size32());
			Variable* splitted	= new SplittedVariable(this, (AuxiliaryVariable*)auxiliary);

			// Modify variables
			variables.at(i) = splitted;
			variables.push_back(auxiliary);
		}
	}

	// Process regular constraints
	for (auto it = constraints.begin(); it != constraints.end(); ++it) {
		if (it->type == Constraint::CT_MORE_EQUAL) {
			// Add empty column to all regular constraints except the current
			for (Constraint *mit = constraints.begin(); mit != constraints.end(); ++mit)
				if (mit != it)
					mit->add_column(0);

			for (Constraint *mit = nn_constraints.begin(); mit != nn_constraints.end(); ++mit)
				mit->add_column(0);

			// Add a 1 column to the current
			it->add_column(-1);
			it->type = Constraint::CT_EQUAL;
			objective_function.add_column(0);
			++solution_dimension;

			// Add constraint
			LPmatrix eye(1, solution_dimension);
			eye(solution_dimension - 1) = 1;
			add_constraint(Constraint(eye, Constraint::CT_NON_NEGATIVE, 0));

			// Update variables vector
			variables.push_back(new SlackVariable(this));

		} else if (it->type == Constraint::CT_LESS_EQUAL) {
			// Add empty column to all regular constraints except the current
			for (Constraint *mit = constraints.begin(); mit != constraints.end(); ++mit)
				if (mit != it)
					mit->add_column(0);

			for (Constraint *mit = nn_constraints.begin(); mit != nn_constraints.end(); ++mit)
				mit->add_column(0);

			// Add a 1 column to the current
			it->add_column(1);
			it->type = Constraint::CT_EQUAL;
			objective_function.add_column(0);
			++solution_dimension;

			// Add constraint
			LPmatrix eye(1, solution_dimension);
			eye(solution_dimension - 1) = 1;
			add_constraint(Constraint(eye, Constraint::CT_NON_NEGATIVE, 0));

			// Update variables vector
			variables.push_back(new SlackVariable(this));
		}
	}

	// Manipulate objective function
	if (objective_function.type == ObjectiveFunction::OFT_MAXIMIZE) {
		objective_function.type = ObjectiveFunction::OFT_MINIMIZE;
		LPmatrix zero(1, solution_dimension, 0);
		changed_sign = true;
		objective_function.costs = zero - objective_function.costs;
	}
}

void Simplex::process_to_artificial_problem() {
	ColumnSet identity;

	// Scan all the columns, when I find a column that is an eye for i put it in the base at position i
	for (uint32 i = 0; i < constraints.size(); ++i) {
		bool column_not_found = true;

		for (int c = solution_dimension - 1; c > -1 && column_not_found; --c) {
			bool column_match = true;
			for (uint32 j = 0; j < constraints.size() && column_match; ++j)
				column_match = !constraints.at(j).coefficients(c) != (i == j);

			if (column_match) {
				identity.insert(c);
				column_not_found = false;
			}
		}

		if (column_not_found)
			identity.insert(-1);
	}

	// If artificial variables are needed
	objective_function.costs.empty();

	if (identity.contains(-1)) {
		for (uint32 i = 0; i < identity.size(); ++i) {
			if (identity.column(i) == -1) {

				// Add column 1 to constraint i
				for (uint32 k = 0; k < constraints.size(); ++k)
					constraints.at(k).add_column(k == i);

				// Solution vector is bigger
				identity.column(i) = solution_dimension;
				++solution_dimension;

				// Create non-negative constraint for new variable
				LPmatrix eye(1, solution_dimension);
				eye(solution_dimension - 1) = 1;

				for (uint32 k = 0; k < nn_constraints.size(); ++k)
					nn_constraints.at(k).add_column(0);

				// Objective function costs updated
				objective_function.add_column(1);
				add_constraint(Constraint(eye, Constraint::CT_NON_NEGATIVE, 0));
			}
		}
	}
	suggested_base = identity;
}

void Simplex::solve_with_base(ColumnSet const& initial_base) {
	// Preprocess constraints data to lead to matrices

	// A
	coefficients_matrix.resize(
		constraints.size32(),
		solution_dimension
	);

	constraints_vector.resize(constraints.size32(), 1);

	// b
	for (uint32 i = 0; i < constraints.size(); ++i) {
		constraints_vector(i) = constraints.at(i).value;
		for (int j = 0; j < solution_dimension; ++j)
			coefficients_matrix(i, j) = constraints.at(i).coefficients(j);
	}

	// copy costs
	costs = objective_function.costs;

	ISO_ASSERT(initial_base.size() == constraints.size());
	current_base = initial_base;

	// exported variables
	optimal		= false;
	unlimited	= false;

	int		step = 0;
	while (!optimal && !unlimited) {
		LPmatrix	base_costs(1, (int)current_base.size());	// Costs of base

		// populate current_out_of_base
		current_out_of_base.columns.clear();
		for (int i = 0; i < solution_dimension; ++i)
			if (!current_base.contains(i))
				current_out_of_base.insert(i);

		// every inverse_recalculation steps recompute inverse from scratch
		if (step % inverse_recalculation_rate == 0) {
			LPmatrix	base_matrix((int)current_base.size());

			// unpack current base and objective costs
			for (uint32 j = 0; j < current_base.size(); ++j) {
				base_costs(j) = costs(current_base.column(j));
				for (uint32 i = 0; i < current_base.size(); ++i)
					base_matrix(i, j) = coefficients_matrix(i, current_base.column(j));
			}

			// compute inverse
			base_matrix.get_inverse(base_inverse);

		} else {
			// unpack objective costs
			for (uint32 j = 0; j < current_base.size(); ++j)
				base_costs(j) = costs(current_base.column(j));

			LPmatrix old_inverse = base_inverse;

			// compute inverse
			LPmatrix::get_inverse_with_column(old_inverse, column_p, old_column, base_inverse);

		}

		++step;

		// Compute x_B = B^-1 * b
		base_solution = base_inverse * constraints_vector;

		// Compute u = c_B * A
		LPmatrix	u = base_costs * base_inverse;

		// Compute reduced cost
		reduced_cost	= costs - u * coefficients_matrix;
		optimal			= reduced_cost.more_equal_than(0, TOL);

		bool degenerate = false;
		for (uint32 i = 0; i < current_base.size(); ++i)
			if (approx_equal(base_solution(i), 0, TOL)) {
				degenerate = true;
				break;
			}

		if (!optimal) {
			// column of reduced cost with min value (one of the policies)
			column_p.resize(constraints.size32(), 1);

			// Bland's strategy
			int		p = -1;
			for (uint32 i = 0; i < current_out_of_base.size(); ++i)
				if (reduced_cost(current_out_of_base.column(i)) < 0) {
					p = current_out_of_base.column(i);
					break;
				}

			for (uint32 i = 0; i < constraints.size(); ++i)
				column_p(i) = coefficients_matrix(i, p);

			LPmatrix a_tilde	= base_inverse * column_p;
			unlimited		= a_tilde.less_equal_than(0, TOL);

			if (!unlimited) {
				// Bland's strategy
				int q_position = -1;
				for (uint32 i = 0; i < current_base.size(); ++i) {
					if (a_tilde(i) > 0 && (q_position == -1 || base_solution(i) / a_tilde(i) < base_solution(q_position) / a_tilde(q_position)))
						q_position = i;
				}

				int q = current_base.column(q_position);
				old_column = q_position;

				// Take off q, push in p
				current_base.substitute(q, p);
			}

		} else {
			LPmatrix objective_function_base(1, (int)current_base.size(), 0);
			LPmatrix full_solution(solution_dimension, 1, 0);

			// update dual variables
			dual_variables = u;

			for (uint32 i = 0; i < constraints.size(); ++i)
				objective_function_base(i) = costs(current_base.column(i));

			for (int i = 0; i < solution_dimension; ++i)
				if (current_base.contains(i))
					full_solution(i) = base_solution(current_base.index_of(i));

			// saves some flops
			solution_value = (objective_function_base * base_solution);

			if (changed_sign)
				solution_value = -solution_value;

			solution = full_solution;
		}
	}
}

void Simplex::solve() {
	ColumnSet initial_base;

	// Create problem to work on
	Simplex standard_form_problem = *this;
	has_to_be_fixed = false;

	// Preprocessing
	standard_form_problem.process_to_standard_form();

	// Generate and solve artificial problem
	{
		// Create copy of standard form problem to create artificial problem
		Simplex artificial_problem = standard_form_problem;

		//std::cout << "Generating artificial problem ...";
		artificial_problem.process_to_artificial_problem();

		// Use artificial problem suggested base to solve it
		//std::cout << "Solving artificial problem ..." << std::endl;
		artificial_problem.solve_with_base(artificial_problem.suggested_base);

		overconstrained = artificial_problem.solution_value != 0;
		if (overconstrained)
			return;

		// If initial base doesn't contain artificial variables I can just use it, otherwise it may contain an artificial variable
		// Check for existence of a column index related to an artificial variable by reading costs vector
		int		artificial_variable = -1;
		for (int i = 0; i < artificial_problem.solution_dimension; ++i)
			if (artificial_problem.objective_function.costs(i) == 1 && artificial_problem.current_base.contains(i))
				artificial_variable = i;

		// if index is still -1 (no artificial variables)
		if (artificial_variable == -1) {
			//std::cout << "Base is clear about artificial variables, proceed ..." << std::endl;
			standard_form_problem.suggested_base = artificial_problem.current_base;

		} else {
			//	If an artificial variable exists ... can change the i (artificial) column with a j column in current_out_of base so that:
			//		*	j is not an auxiliary variable
			//		*	(B^-1)_q * A^j != 0

			int		q = artificial_problem.current_base.index_of(artificial_variable);
			LPmatrix	bi_row_q(1, (int)artificial_problem.current_base.size());

			for (uint32 k = 0; k < artificial_problem.current_base.size(); ++k)
				bi_row_q(k) = artificial_problem.base_inverse(q, k);

			// Find j
			int j = -1;
			for (uint32 i = 0; i < standard_form_problem.current_out_of_base.size(); ++i) {

				// Pick the ones that doesn't refer to an artificial variable
				if (artificial_problem.costs(i) == 0) {
					LPmatrix column_j(standard_form_problem.current_base.size(), 1);

					for (uint32 k = 0; k < standard_form_problem.current_base.size(); ++k)
						column_j(k) = artificial_problem.coefficients_matrix(k, i);

					if ((double)(bi_row_q * column_j) != 0) {
						j = i;
						break;
					}
				}
			}

			if (j != -1) {
				// Found a j, substitute artificial_value with j
				standard_form_problem.suggested_base = artificial_problem.current_base;
				standard_form_problem.suggested_base.substitute(artificial_variable, j);

			} else {
				// I didn't find a j which respected the requirements
				// It may happen that for each j we have (B^-1)_q * A^j = 0, this means that the rows of A are linearly dependent and we can eliminate one of them
				// Let d be d = e_q * B^-1
				// We have to eliminate a row for which d is non-zero

				//std::cout << "Constraints are linearly dependent!" << std::endl;

				// Find a constraint to eliminate (change)
				int change = -1;
				for (uint32 i = 0; i < standard_form_problem.constraints.size() && change == -1; ++i)
					if (bi_row_q(i) != 0)
						change = i;

				//std::cout << "Constraint #" << change << " must be eliminated." << std::endl;
				has_to_be_fixed = true;
				return;
			}
		}
	}

	standard_form_problem.solve_with_base(standard_form_problem.suggested_base);

	// The solution of the standard form problem must be transposed to	the original problem
	unlimited = standard_form_problem.unlimited;
	if (!unlimited) {
		solution.resize(solution_dimension, 1);

		int index = 0;
		for (auto &i : standard_form_problem.variables)
			i->process(standard_form_problem.solution, solution, index++);

		solution_value		= standard_form_problem.solution_value;
		dual_variables		= standard_form_problem.dual_variables;
		constraints_vector	= standard_form_problem.constraints_vector;
		changed_sign		= standard_form_problem.changed_sign;
	}
}

//-----------------------------------------------------------------------------
///	INTERIOR POINT METHOD
//-----------------------------------------------------------------------------

#include "sparse.h"

// sparse matrix-matrix multiply for the product A.D.AT where D is a diagonal matrix
// used to form the so-called normal equations in the interior-point method for linear programming

struct ADAT {
	const sparse_matrix<F>	&a, &at;
	sparse_matrix<F>		adat;

	ADAT(const sparse_matrix<F> &A, const sparse_matrix<F> &AT);

	//Computes ADAT, where D is a diagonal matrix.This function can be called repeatedly to update ADAT for fixed A.
	void updateD(const dynamic_vector<F> &D);
	operator sparse_matrix<F>&() { return adat; }
};

//Allocates compressed column storage for A.AT, where A and AT are input in compressed column format, and fills in values of ia() and ja()
//Each column must be in sorted order in input matrices. LPmatrix is output with each column sorted
ADAT::ADAT(const sparse_matrix<F> &A, const sparse_matrix<F> &AT) : a(A), at(AT) {
	int		m		= AT.cols();
	int		*done	= alloc_auto(int, m);

	// first pass : determine number of nonzeros
	int	nvals = 0;
	for (int i = 0; i < m; i++)
		done[i] = -1;

	for (int j = 0; j < m; j++) {
		for (int i = AT.ia()[j]; i < AT.ia()[j + 1]; i++) {
			int k = AT.ja()[i];			//AT[k,j] != 0. Find column k in first matrix, A
			for (int l = A.ia()[k]; l < A.ia()[k + 1]; l++) {
				int h = A.ja()[l];		//A[h,l] != 0
				if (done[h] != j) {		//test if contribution already included
					done[h] = j;
					nvals++;
				}
			}
		}
	}

	adat.create(m, m, nvals);

	// second pass : determine columns of adat
	nvals = 0;
	for (int i = 0; i < m; i++)
		done[i] = -1;

	for (int j = 0; j < m; j++) {
		adat.ia()[j] = nvals;
		for (int i = AT.ia()[j]; i < AT.ia()[j + 1]; i++) {
			int k = AT.ja()[i];
			for (int l = A.ia()[k]; l < A.ia()[k + 1]; l++) {
				int h = A.ja()[l];
				if (done[h] != j) {
					done[h] = j;
					adat.ja()[nvals++] = h;
				}
			}
		}
	}
	adat.ia()[m] = nvals; //Set last value

	//sort columns
	for (int j = 0; j < m; j++) {
		int		i		= adat.ia()[j];
		int		size	= adat.ia()[j + 1] - i;
		if (size > 1)
			sort(adat.ja() + i, adat.ja() + i + size);
	}
}

void ADAT::updateD(const dynamic_vector<F> &D) {
	int		m = a.rows(), n = a.cols();
	dynamic_vector<F>	temp(n), temp2(m, 0);

	 //loop over columns of AT
	for (int i = 0; i < m; i++) {
		for (int j = at.ia()[i]; j < at.ia()[i + 1]; j++) {
			//scale elements of each column with D and store in temp
			int		k	= at.ja()[j];
			temp[k]		= at.pa()[j] * D[k];
		}
		//go down column again.
		for (int j = at.ia()[i]; j < at.ia()[i + 1]; j++) {
			int k = at.ja()[j];
			for (int l = a.ia()[k]; l < a.ia()[k + 1]; l++) {
				//go down column k in A
				int		h	= a.ja()[l];
				temp2[h]	+= temp[k] * a.pa()[l]; //All terms from temp[k] used here.
			}
		}
		for (int j = adat.ia()[i]; j < adat.ia()[i + 1]; j++) {
			//store temp2 in column of answer
			int		k		= adat.ja()[j];
			adat.pa()[j]	= temp2[k];
			temp2[k]		= 0; //Restore temp2.
		}
	}
}

struct LDL {
	sparse_matrix<F>	&A;
	sparse_matrix<F>	L;
	sparse_layout		Ls;
	int					n;
	ref_array<int>		P, Pinv, parent;
	dynamic_vector<F>		D, Lx;

	//AMD ordering and LDL symbolic factorization
	LDL(sparse_matrix<F> &A) : A(A), n(A.cols()), Pinv(n), parent(n), D(n) {
		P	= amd(A, sparse_symbolic::NATURAL);
		Ls	= ldl_symbolic(A, P, Pinv, parent);
		Lx	= dynamic_vector<F>(Ls.nz());
		L	= sparse_matrix<F>(Ls, Lx);
	}

	//mumerical factorisation of matrix
	void	factorize() {
		int dd = ldl_numeric(A, P, Pinv, parent, Ls, Lx, D);
		ISO_ASSERT(dd == n);	// factorization failed because diagonal has zero
	}

	//solves for y given rhs. Can be invoked multiple times after a single call to factorize.
	dynamic_vector<F>	solve(dynamic_vector<F> &B) {
		dynamic_vector<F>	Y = B.permute(P);
		ldl_lsolve(L, Y);			// y = L\y
		ldl_dsolve(D, Y);			// y = D\y
		ldl_ltsolve(L, Y);			// y = L’\y
		return Y.inv_permute(P);	// x = P’y
	}
};

//-----------------------------------------------------------------------------
//	Interior-point method of linear programming
//
//	On input a contains the coefficient matrix for the constraints in the form A.x = b
//	The coefficients of the objective function to be minimized are in c
//	Note that c should generally be padded with zeros corresponding to the slack variables that extend the number of columns to be n
//	returns:
//	0 if an optimal solution is found
//	1 if the problem is infeasible
//	2 if the dual problem is infeasible, i.e., if the problem is unbounded or perhaps infeasible
//	3 if the number of iterations is exceeded
//	The solution is returned in x
//-----------------------------------------------------------------------------

int lp_interior_point(const sparse_matrix<F> &a, const dynamic_vector<F> &b, const dynamic_vector<F> &c, dynamic_vector<F> &x) {
	const int	MAXITS	= 200;				//maximum iterations
	const F		EPS		= F(1.0e-6);		//tolerance for optimality and feasibility
	const F		SIGMA	= F(0.9);			//stepsize reduction factor(conservative choice)
	const F		DELTA	= F(0.02);			//factor to set centrality parameter mu

	int			m		= a.rows();
	int			n		= a.cols();
	dynamic_vector<F>	dx(n), dy(m), dz(n);
	dynamic_vector<F>	rp(m), rd(n), d(n), tempn(n), tempm(m);

	sparse_matrix<F> at = transpose(a);
	ADAT		adat(a, at);				//setup for A.D.AT, where D=D.X.invZ
	LDL			solver(adat);

	//compute factors for convergence test
	F			rpfact	= 1 + sqrt(dot(b, b));
	F			rdfact	= 1 + sqrt(dot(c, c));

	//initial point
	for (int j = 0; j < n; j++)
		x[j] = 1000;
	dynamic_vector<F>	z(n, 1000);
	dynamic_vector<F>	y(m, 1000);

	F	normrp_old = maximum;
	F	normrd_old = maximum;

	// main loop
	for (int iter = 0; iter < MAXITS; iter++) {
		//compute normalized residuals rp and rd
		rp				= a * x - b;
		F	normrp		= sqrt(dot(rp, rp)) / rpfact;

		rd				= at * y + z - c;
		F	normrd		= sqrt(dot(rd, rd)) / rdfact;

		F	gamma		= dot(x, z);		//duality gap is x.z for feasible points
		F	mu			= DELTA * gamma / n;
		F	primal_obj	= dot(c, x);
		F	dual_obj	= dot(b, y);
		F	gamma_norm	= gamma / (1 + abs(primal_obj));

		if (normrp < EPS && normrd < EPS && gamma_norm < EPS)
			return 0;	//Optimal solution found

		if (normrp > 1000 * normrp_old && normrp > EPS)
			return 1;	//Primal infeasible

		if (normrd > 1000 * normrd_old && normrd > EPS)
			return 2;	//Dual infeasible

		//compute step directions

		//first form matrix A.X.invZ.AT
		d		= x / z;
		adat.updateD(d);
		solver.factorize();

		tempn	= x;// - mu / z;// - d * rd;
		tempn	= x - d * rd;
		tempm	= a * tempn - rp;

		dy		= solver.solve(tempm);		//solve for dy
		tempn	= at * dy;
		dz		= -rd - tempn;				//solve for dz
		dx		= mu / z - x - d * dz;		//solve for dx

		//find step length
		F	alpha_p = 1;
		for (int j = 0; j < n; j++)
			if (x[j] + alpha_p*dx[j] < 0)
				alpha_p = -x[j] / dx[j];

		F	alpha_d = 1;
		for (int j = 0; j < n; j++)
			if (z[j] + alpha_d * dz[j] < 0)
				alpha_d = -z[j] / dz[j];

		alpha_p = min(alpha_p * SIGMA, 1);
		alpha_d = min(alpha_d * SIGMA, 1);

		//step to new point
		x += alpha_p * dx;
		z += alpha_d * dz;
		y += alpha_d * dy;

		//update norms
		normrp_old = normrp;
		normrd_old = normrd;
	}

	return 3; //Maximum iterations exceeded
}

struct constraint : dynamic_vector<F> {
	enum Type {
		CT_LESS_EQUAL,
		CT_MORE_EQUAL,
		CT_EQUAL,
		CT_NON_NEGATIVE,
		CT_BOUNDS
	};
	Type	type;
	F		value1, value2;

	constraint(const dynamic_vector<F> &coefficients, Type type, F value1 = 0, F value2 = 0) : dynamic_vector<F>(coefficients), type(type), value1(value1), value2(value2) {}
	void add_column(F value) {
		int	oldn = n;
		resize(oldn + 1);
		begin()[oldn] = value;
	}
};

void process_to_standard_form(dynamic_array<constraint> &constraints, dynamic_vector<F> &costs, bool maximise) {
	enum VarType {
		VT_NORMAL,
		VT_SLACK,
		VT_SPLIT_POS,
		VT_SPLIT_NEG,
	};
	dynamic_array<VarType> variables;

	// process non-negative constraints
	int initial_solution_dimension = constraints.size32();
	int solution_dimension = initial_solution_dimension;

	for (int i = 0; i < initial_solution_dimension; ++i) {	// For each component of x
		bool has_constraint = false;

		// Find an x that doesn't have a non-negativity constraint on it
		for (auto &c : constraints) {
			if (c.type == constraint::CT_NON_NEGATIVE && c[i] == 1) {
				has_constraint = true;
				break;
			}
		}

		if (!has_constraint) {
			// Add a non-negativity constraint
			dynamic_vector<F>	eye(solution_dimension, 0);
			eye[i]	= 1;
			constraints.push_back(constraint(eye, constraint::CT_NON_NEGATIVE));

			// Add a column to all constraints
			for (auto &c : constraints) {
				if (c.type == constraint::CT_NON_NEGATIVE)
					c.add_column(0);
			}

			// Add another non-negativity constraint
			dynamic_vector<F> n_eye(solution_dimension, 0);
			n_eye[solution_dimension - 1] = 1;

			constraints.push_back(constraint(n_eye, constraint::CT_NON_NEGATIVE));

			// Add a regular constraint
			for (auto &c : constraints) {
				if (c.type != constraint::CT_NON_NEGATIVE)
					c.add_column(-c[i]);
			}

			int	oldn = costs.n;
			costs.resize(oldn + 1);
			costs[oldn] = -costs[i];

			// Update variables status
			variables[i] = VT_SPLIT_POS;
			variables.push_back(VT_SPLIT_NEG);
		}
	}

	// process regular constraints
	for (auto &c : constraints) {
		if (c.type == constraint::CT_MORE_EQUAL) {
			// Add empty column to all regular constraints except the current
			for (auto &c2 : constraints) {
				if (&c2 != &c)
					c2.add_column(0);
			}

			// Add a 1 column to the current
			c.add_column(-1);
			c.type = constraint::CT_EQUAL;

			int	oldn = costs.n;
			costs.resize(oldn + 1);
			costs[oldn] = 0;

			// Add constraint
			dynamic_vector<F>	eye(solution_dimension, 0);
			eye[solution_dimension - 1] = 1;
			constraints.push_back(constraint(eye, constraint::CT_NON_NEGATIVE));

			variables.push_back(VT_SLACK);

		} else if (c.type == constraint::CT_LESS_EQUAL) {
			// Add empty column to all regular constraints except the current
			for (auto &c2 : constraints) {
				if (&c2 != &c)
					c2.add_column(0);
			}

			// Add a 1 column to the current
			c.add_column(1);
			c.type = constraint::CT_EQUAL;

			int	oldn = costs.n;
			costs.resize(oldn + 1);
			costs[oldn] = 0;

			// Add constraint
			dynamic_vector<F>	eye(solution_dimension, 0);
			eye[solution_dimension - 1] = 1;
			constraints.push_back(constraint(eye, constraint::CT_NON_NEGATIVE));

			variables.push_back(VT_SLACK);
		}
	}

	// Manipulate objective function
	if (maximise)
		costs = -costs;
}

// Solve a linear-least-squares system, i.e., for system A*x=b, find argmin_x |A*x-b|.
class LLS {
protected:
	int					m, n, nd;
	dynamic_matrix<float>	b;					// [nd][m]; transpose of client view
	dynamic_matrix<float>	x;					// [nd][n]; transpose of client view
	bool				solved;			// solve() can destroy A, so check
	LLS(int m, int n, int nd) : m(m), n(n), nd(nd), b(nd, m), x(nd, n), solved(false) { clear(); }
public:
	// virtual constructor
	LLS				*make(int m, int n, int nd, float nonzerofrac);	// A(m, n), x(n, nd), b(m, nd)
	virtual			~LLS() {}
	virtual void	clear()	{ b.clear(); x.clear(); solved = false; }

	// All entries will be zero unless entered as below.
	void			enter_a(const dynamic_matrix<float> mat) {	// [m][n]
		ISO_ASSERT(mat.rows()==m && mat.cols()==n);
		for (int r = 0; r < m; ++r) { enter_a_r(r, mat[r]); }
	}
	virtual void	enter_a_r(int r, vector_view<float> ar) = 0;	// r<m, ar.size()==n
	virtual void	enter_a_c(int c, vector_view<float> ar) = 0;	// c<n, ar.size()==m
	virtual void	enter_a_rc(int r, int c, float val) = 0;	// r<m, c<n
	void			enter_b(const dynamic_matrix<float> mat) {	// [m][nd]
		ISO_ASSERT(mat.rows()==m && mat.cols()==nd);
		for (int r = 0; r < m; ++r) enter_b_r(r, mat[r]);
	}
	void			enter_b_r(int r, vector_view<float> ar)		{ ISO_ASSERT(ar.size() == nd); for (int c = 0; c < nd; ++c) enter_b_rc(r, c, ar[c]); }
	void			enter_b_c(int c, vector_view<float> ar)		{ ISO_ASSERT(ar.size() == m);  for (int r = 0; r < m; ++r)  enter_b_rc(r, c, ar[r]); }
	void			enter_b_rc(int r, int c, float val)			{ b[c][r] = val; }
	void			enter_xest(const dynamic_matrix<float> mat) {	// [n][nd]
		ISO_ASSERT(mat.rows()==n && mat.cols()==nd);
		for (int r = 0; r < n; ++r) enter_xest_r(r, mat[r]);
	}
	void			enter_xest_r(int r, vector_view<float> ar)	{ ISO_ASSERT(ar.size() == nd); for (int c = 0; c < nd; ++c) enter_xest_rc(r, c, ar[c]); }
	void			enter_xest_c(int c, vector_view<float> ar)	{ ISO_ASSERT(ar.size() == n);  for (int r = 0; r < n; ++r)  enter_xest_rc(r, c, ar[r]); }
	void			enter_xest_rc(int r, int c, float val)		{ x[c][r] = val; }			// r<n, c<nd
	virtual bool	solve(double* rssb = nullptr, double* rssa = nullptr) = 0;	// ret: success
	void			get_x(const dynamic_matrix<float> mat) {		// [n][nd]
		ISO_ASSERT(mat.rows()==n && mat.cols()==nd);
		for (int r = 0; r < n; ++r) get_x_r(r, mat[r]);
	}
	void			get_x_r(int r, vector_view<float> ar)		{ ISO_ASSERT(ar.size() == nd); for (int c = 0; c < nd; ++c) ar[c] = get_x_rc(r, c); }
	void			get_x_c(int c, vector_view<float> ar)		{ ISO_ASSERT(ar.size() == n);  for (int r = 0; r < n; ++r)  ar[r] = get_x_rc(r, c); }
	float			get_x_rc(int r, int c)						{ return x[c][r]; } // r<n, c<nd
	int				num_rows() const							{ return m; }
};

// Sparse conjugate-gradient approach.
class SparseLLS : public LLS {
	struct Ival {
		int		_i;
		float	_v;
		Ival() = default;
		Ival(int i, float v) : _i(i), _v(v) {}
	};
	dynamic_array<dynamic_array<Ival> > _rows;
	dynamic_array<dynamic_array<Ival> > _cols;
	float	_tolerance;
	int		_max_iter{ INT_MAX };
	int		_nentries;
	dynamic_vector<float> mult_m_v(dynamic_vector<float> &vi) const;
	dynamic_vector<float> mult_mt_v(dynamic_vector<float> &vi) const;
	bool	do_cg(dynamic_vector<float>& x, dynamic_vector<float> &h, double* prssb, double* prssa) const;
public:
	SparseLLS(int m, int n, int nd) : LLS(m, n, nd), _rows(m), _cols(n), _tolerance(square(8e-7f) * m), _nentries(0) {}
	void	clear() override;
	void	enter_a_rc(int r, int c, float val) override;
	void	enter_a_r(int r, vector_view<float> ar) override;
	void	enter_a_c(int c, vector_view<float> ar) override;
	bool	solve(double* rssb = nullptr, double* rssa = nullptr) override;
	void	set_tolerance(float tolerance)	{ _tolerance = tolerance; }
	void	set_max_iter(int max_iter)		{ _max_iter = max_iter; }
};

// Base class for full (non-sparse) approaches.
class FullLLS : public LLS {
	double get_rss() {
		double rss = 0.;
		for (int di = 0; di < nd; ++di)
			for (int i = 0; i < m; ++i)
				rss += square(dot(a[i], x[di]) - b[di][i]);
		return rss;
	}
protected:
	dynamic_matrix<float> a;			// [m][n]
	virtual bool solve_aux() = 0;
public:
	FullLLS(int m, int n, int nd) : LLS(m, n, nd), a(m, n) { clear(); }
	void	clear() override { a.clear(); LLS::clear(); }
	void	enter_a_rc(int r, int c, float val) override { a[r][c] = val; }
	void	enter_a_r(int r, vector_view<float> ar) override { ISO_ASSERT(ar.size() == n); for (int c = 0; c < n; ++c) a[r][c] = ar[c]; }
	void	enter_a_c(int c, vector_view<float> ar) override { ISO_ASSERT(ar.size() == m); for (int r = 0; r < m; ++r) a[r][c] = ar[r]; }
	bool	solve(double* rssb = nullptr, double* rssa = nullptr) override {
		ISO_ASSERT(m>=n);
		ISO_ASSERT(!solved);
		solved = true;
		if (rssb)
			*rssb = get_rss();
		bool success = solve_aux();
		if (rssa)
			*rssa = get_rss();
		return success;
	}
};

// LU decomposition on A^t*A=A^t*b (slow).
class LudLLS : public FullLLS {
	bool solve_aux() override;
public:
	LudLLS(int m, int n, int nd) : FullLLS(m, n, nd) {}
};

// Givens substitution approach.
class GivensLLS : public FullLLS {
	bool solve_aux() override;
public:
	GivensLLS(int m, int n, int nd) : FullLLS(m, n, nd) {}
};

// Singular value decomposition.
class SvdLLS : public FullLLS {
	dynamic_vector<float> _fa, _fb, _s, work;
	dynamic_matrix<float> U;
	dynamic_vector<float> S;
	dynamic_matrix<float> VT;
	bool solve_aux() override;
public:
	SvdLLS(int m, int n, int nd) : FullLLS(m, n, nd), work(n), U(m, n), S(n), VT(n, n) {}
};

// QR decomposition.
class QrdLLS : public FullLLS {
	dynamic_vector<float> _fa, _fb, work;
	dynamic_matrix<float> U;
	dynamic_vector<float> S;
	dynamic_matrix<float> VT;
	bool solve_aux() override;
public:
	QrdLLS(int m, int n, int nd) : FullLLS(m, n, nd), work(n), U(m, n), S(n), VT(n, n) {}
};

LLS* LLS::make(int m, int n, int nd, float nonzerofrac) {
	int64 size = int64(m) * n;
	if (size < 1000 * 40) {		// small system
		return new QrdLLS(m, n, nd);
	} else {					// large system
		if (nonzerofrac < .3f)
			return new SparseLLS(m, n, nd);
		return new QrdLLS(m, n, nd);
	}
}

// *** SparseLLS

void SparseLLS::clear() {
	_rows.clear();
	_cols.clear();
	LLS::clear();
}

void SparseLLS::enter_a_rc(int r, int c, float val) {
	_rows[r].push_back(Ival(c, val));
	_cols[c].push_back(Ival(r, val));
	_nentries++;
}

void SparseLLS::enter_a_r(int r, vector_view<float> ar) {
	ISO_ASSERT(ar.size() == n);
	for (int c = 0; c < n; ++c) {
		if (ar[c])
			enter_a_rc(r, c, ar[c]);
	}
}

void SparseLLS::enter_a_c(int c, vector_view<float> ar) {
	ISO_ASSERT(ar.size() == m);
	for (int r = 0; r < m; ++r) {
		if (ar[r])
			enter_a_rc(r, c, ar[r]);
	}
}

dynamic_vector<float> SparseLLS::mult_m_v(dynamic_vector<float> &vi) const {
	dynamic_vector<float> vo(m);
	// vo[m] = a[m][n]*vi[n];
	for (int i = 0; i < m; ++i) {
		double sum = 0.;
		for (const Ival& ival : _rows[i])
			sum += double(ival._v)*vi[ival._i];
		vo[i] = float(sum);
	}
	return vo;
}

dynamic_vector<float> SparseLLS::mult_mt_v(dynamic_vector<float> &vi) const {
	dynamic_vector<float> vo(n);
	// vo[n] = uT[n][m]*vi[m];
	for (int j = 0; j < n; ++j) {
		double sum = 0.;
		for (const Ival& ival : _cols[j])
			sum += double(ival._v)*vi[ival._i];
		vo[j] = float(sum);
	}
	return vo;
}

bool SparseLLS::do_cg(dynamic_vector<float> &x, dynamic_vector<float> &h, double* prssb, double* prssa) const {
	// x(n), h(m)
	dynamic_vector<float> rc, gc, gp, dc, tc;
	rc = mult_m_v(x);
	for (int i = 0; i < m; ++i)
		rc[i] -= h[i];

	double rssb = dot(rc, rc);
	gc = mult_mt_v(rc);
	for (int i = 0; i < n; ++i)
		gc[i] = -gc[i];

	const int fudge_for_small_systems = 20;
	const int kmax = n + fudge_for_small_systems;
	float gm2;
	for (int k = 0; ; k++) {
		gm2 = float(len2(gc));
		if (gm2 < _tolerance)
			break;
		if (k == _max_iter)
			break;
		if (k == kmax)
			break;
		if (k > 0) {
			float bi = gm2 / len2(gp);
			for (int i = 0; i < n; ++i)
				dc[i] = gc[i] + bi*dc[i];
		} else {
			dc = gc;
		}
		tc = mult_m_v(dc);
		float ai = gm2 / len2(tc);
		for (int i = 0; i < n; ++i)
			x[i] += ai*dc[i];
		for (int i = 0; i < m; ++i)
			rc[i] += ai*tc[i];
		gp = gc;
		gc = mult_mt_v(rc);
		for (int i = 0; i < n; ++i)
			gc[i] = -gc[i];
	}
	double rssa = len2(rc);
	// Print final gradient norm squared and final residual norm squared.
	if (prssb)
		*prssb += rssb;
	if (prssa)
		*prssa += rssa;
	return gm2 < _tolerance;
}

bool SparseLLS::solve(double* prssb, double* prssa) {
	ISO_ASSERT(!solved); solved = true;
	if (prssb)
		*prssb = 0;
	if (prssa)
		*prssa = 0;

	dynamic_vector<float> y(n), rhv(m);
	bool success = true;

	for (int di = 0; di < nd; ++di) {
		for (int i = 0; i < m; ++i)
			rhv[i] = b[di][i];
		for (int j = 0; j < n; ++j)
			y[j] = x[di][j];
		if (!do_cg(y, rhv, prssb, prssa))
			success = false;
		for (int j = 0; j < n; ++j)
			x[di][j] = y[j];
	}
	return success;
}

// *** LudLLS

bool LudLLS::solve_aux() {
	dynamic_matrix<float>	ta;

	if (m != n)
		ta.resize(n, n);

	dynamic_matrix<float> a = m != n ? ta : a;
	if (m != n) {
		for (int i = 0; i < n; ++i) {
			for (int j = 0; j < n; ++j) {
				double s = 0;
				for (int k = 0; k < m; ++k)
					s += double(a[k][i])*a[k][j];
				a[i][j] = float(s);
			}
		}
	}
	dynamic_array<int> rindx(n);
	dynamic_vector<float> t(n);
	for (int i = 0; i < n; ++i) {
		float vmax = 0.f;
		for (int j = 0; j < n; ++j)
			vmax = max(vmax, abs(a[i][j]));
		if (!vmax)
			return false;
		t[i] = 1 / vmax;
	}
	int imax = 0;				// undefined
	for (int j = 0; j < n; ++j) {
		for (int i = 0; i < j; ++i) {
			double s = a[i][j];
			for (int k = 0; k < i; ++k)
				s -= double(a[i][k])*a[k][j];
			a[i][j] = float(s);
		}
		float vmax = 0.f;
		for (int i = j; i < n; ++i) {
			double s = a[i][j];
			for (int k = 0; k < j; ++k)
				s -= double(a[i][k])*a[k][j];
			a[i][j] = float(s);
			float v = t[i] * abs(a[i][j]);
			if (v >= vmax) { vmax = v; imax = i; }
		}
		if (imax != j) {
			swap_ranges(a[imax], a[j]);
			t[imax] = t[j];
		}
		rindx[j] = imax;
		if (!a[j][j])
			return false;
		if (j < n - 1) {
			float v = 1.f / a[j][j];
			for (int i = j + 1; i < n; ++i)
				a[i][j] *= v;
		}
	}
	for (int di = 0; di < nd; ++di) {
		if (m == n) {
			for (int j = 0; j < n; ++j)
				t[j] = b[di][j];
		} else {
			for (int j = 0; j < n; ++j) {
				double s = 0;
				for (int i = 0; i < m; ++i)
					s += double(a[i][j]) * b[di][i];
				t[j] = float(s);
			}
		}
		int ii = -1;
		for (int i = 0; i < n; ++i) {
			int ip = rindx[i];
			double s = t[ip];
			t[ip] = t[i];
			if (ii >= 0) {
				for (int j = ii; j < i; ++j)
					s -= double(a[i][j])*t[j];
			} else if (s) {
				ii = i;
			}
			t[i] = float(s);
		}
		for (int i = n - 1; i >= 0; --i) {
			double s = t[i];
			for (int j = i + 1; j < n; ++j)
				s -= double(a[i][j])*t[j];
			t[i] = float(s / a[i][i]);
		}
		for (int j = 0; j < n; ++j)
			x[di][j] = t[j];
	}
	return true;
}

// *** GivensLLS

bool GivensLLS::solve_aux() {
	int nposs = 0, ngivens = 0;
	for (int i = 0; i < n; ++i) {
		for (int k = i + 1; k < m; ++k) {
			nposs++;
			if (!a[k][i]) continue;
			ngivens++;
			float xi = a[i][i];
			float xk = a[k][i];
			float c, s;
			if (abs(xk) > abs(xi)) {
				float t = xi / xk;
				s = 1.f / sqrt(1.f + square(t));
				c = s * t;
			} else {
				float t = xk / xi;
				c = 1.f / sqrt(1.f + square(t));
				s = c*t;
			}
			for (int j = i; j < n; ++j) {
				float xij = a[i][j], xkj = a[k][j];
				a[i][j] = c*xij + s*xkj;
				a[k][j] = -s*xij + c*xkj;
			}
			for (int di = 0; di < nd; ++di) {
				float xij = b[di][i], xkj = b[di][k];
				b[di][i] = c*xij + s*xkj;
				b[di][k] = -s*xij + c*xkj;
			}
		}
		if (!a[i][i])
			return false;	// solution fails
	}
	// Backsubstitutions
	for (int di = 0; di < nd; ++di) {
		for (int i = n - 1; i >= 0; --i) {
			float sum = b[di][i];
			for (int j = i + 1; j < n; ++j)
				sum -= a[i][j] * b[di][j];
			b[di][i] = sum / a[i][i];
			x[di][i] = b[di][i];
		}
	}
	return true;
}

// *** SvdLLS

bool SvdLLS::solve_aux() {
	if (!svd(a, U, S, VT))
		return false;

	if (0)
		sort_singular_values(U, S, VT);

	if (!S.last())
		return false;

	for (float &s : S)
		s = 1 / s;

	for (int d = 0; d < nd; ++d) {
		work	= b[d] * U;
		work	*= S;
		x[d]	= VT * work;
	}
	return true;
}

bool QrdLLS::solve_aux() {
    if (!svd(a, U, S, VT))
		return false;

	if (0)
		sort_singular_values(U, S, VT);

	if (!S.last())
		return false;

	for (float &s : S)
		s = 1 / s;

	for (int d = 0; d < nd; ++d) {
		work	= b[d] * U;
		work	*= S;
		x[d]	= VT * work;
	}
	return true;
}

#ifdef ISO_TEST

#include "extra/random.h"
static struct test {
	test() {
		rng<simple_random>	random;
		dynamic_matrix<float>	m(4, 4);
		LPmatrix				x(4, 4), x1;
		for (int i = 0; i < 4; i++)
			for (int j = 0; j < 4; j++)
				m(i, j) = x(j, i) = random;

		x.get_inverse(x1);
		LPmatrix	x2 = x1 * x;

		dynamic_matrix<float>	m1 = inverse(m);
		dynamic_matrix<float>	m2 = m1 * m;

		sparse_matrix<float>	s = m;
	}
} _test;

#endif
