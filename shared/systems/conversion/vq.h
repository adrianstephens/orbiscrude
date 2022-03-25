#ifndef VQ_H
#define VQ_H

#include "base/block.h"

namespace iso {

//#define USE_NORMS

#ifdef USE_NORMS
int min_sup(int first, int last, float norm, const float *norms) {
	while (first != last) {
		int	m = (first + last) / 2;
		if (norm > norms[m])
			first = m + 1;
		else
			last = m;
	}
	return first;
}

int max_inf(int first, int last, float norm, const float *norms) {
	while (first != last) {
		int	m = (first + last + 1) / 2;
		if (norm > norms[m])
			first = m;
		else
			last = m - 1;
	}
	return first;
}
#endif

template<typename T> class vq {
	typedef	typename T::element		E;
	typedef	typename T::welement	W;
public:
	auto_block<W,1>	codebook;
	float			*counts;
#ifdef USE_NORMS
	float			*norms;
	uint32			*indices;
#endif

	vq(uint32 size) : codebook(make_auto_block<W>(size)) {
		counts		= new float[size];
#ifdef USE_NORMS
		norms		= new float[size];
		indices		= new uint32[size];
#endif
	}

	~vq() {
		delete[] counts;
#ifdef USE_NORMS
		delete[] norms;
		delete[] indices;
#endif
	}

	uint32		size()				const	{ return codebook.size(); }
//	const W&	operator[](int i)	const	{ return codebook[i];		}
	W&			operator[](int i)			{ return codebook[i];		}

#ifdef USE_NORMS
	void	make_norms(T &t, uint32 cur_size) {
		for (uint32 i = 0; i < cur_size; ++i) {
			norms[i]		= t.norm(codebook[i]);
			indices[i]		= i;
		}

		// reorder the codebook by ascending norm
		for (uint32 i = 0; i < cur_size; i++) {
			uint32	smallest	= i;
			float		minnorm		= norms[i];
			for (int j = i; j < cur_size; j++) {
				float	norm = norms[j];
				if (norm < minnorm) {
					smallest = j;
					minnorm	= norm;
				}
			}
			if (i != smallest) {
				uint32	ti = indices[i];
				indices[i] = indices[smallest];
				indices[smallest] = ti;
				float		tn = norms[i];
				norms[i] = norms[smallest];
				norms[smallest] = tn;
			}
		}
	}
	uint32 find_closest(T &t, const E &d, uint32 cur_size, float &rmindist) {
		float		mindist		= 1e30f; // keep convention that ties go to lower index
		float		norm		= t.norm(d);
		uint32		closest		= indices[min_sup(0, cur_size - 1, norm, norms)];

		float		dist		= sqrt(t.distsquared(d, codebook[closest]));
		uint32		imin		= min_sup(0,	cur_size - 1, norm - dist, norms);
		uint32		imax		= max_inf(imin,	cur_size - 1, norm + dist, norms);

		for (uint32 j = imin; j <= imax; j++) { // find the best codeword
			uint32	cb		= indices[j];
			float	dist	= t.distsquared(d, codebook[cb], mindist);
			if (dist < mindist) {
				mindist = dist;
				closest = cb;
			}
		}

		rmindist = mindist;
		return closest;
	}

#else
	uint32 find_closest(T &t, const E &d, uint32 cur_size, float &rmindist) {
		float		mindist	= t.distsquared(d, codebook[0]);
		uint32		closest	= 0;
		for (uint32 cb = 1; cb < cur_size; ++cb) {
			float dist = t.distsquared(d, codebook[cb], mindist);
			if (dist < mindist) {
				mindist = dist;
				closest = cb;
				if (dist == 0)
					break;
			}

		}
		rmindist = mindist;
		return closest;
	}
#endif

	uint32 build(T &t, float thresh = .001f, bool fast = true) {
		uint32	req_size	= codebook.size();
		uint32	data_size	= uint32(t.size());

		// If data fits in codebook, just copy it to code book.
		if (data_size <= req_size) {
			for (uint32 i = 0; i < data_size; ++i)
				codebook[i] = t.data(i);
			return req_size = data_size;
		}

		W		*sums		= new W[req_size];
		float	*diameters	= new float[req_size];
		W		*furthests	= new W[req_size];
		float	*variances	= new float[req_size];

		// Initialize codebook[0] to average data value
		W		data_sum;
		float	w_sum		= 0;
		t.reset(data_sum);
		for (uint32 i = 0; i < data_size; ++i)
			w_sum += t.weightedsum(data_sum, i);

		codebook[0]		= data_sum;
		t.scale(codebook[0], 1.f / w_sum);

		uint32	split_target	= 0;
		uint32	cur_size = 1;
		while (cur_size < req_size) {
			// Split codebook entries
			if (fast) {
				uint32	codebook_add = req_size - cur_size;
				if (codebook_add > cur_size)
					codebook_add = cur_size;
				for (uint32 i = 0; i < codebook_add; ++i) {
					codebook[i + cur_size] = codebook[i];
					t.scale(codebook[i + cur_size], 1.0001f);
					t.scale(codebook[i], .9999f);
				}
				cur_size += codebook_add;
			} else {
				codebook[cur_size]	= codebook[split_target];
				t.scale(codebook[cur_size], 1.0001f);
				t.scale(codebook[split_target], .9999f);
				cur_size++;
			}

			float	variance = 0;
			for (;;) {
			#ifdef USE_NORMS
				make_norms(t, cur_size);
			#endif
				for (uint32 i = 0; i < cur_size; ++i) {
					t.reset(sums[i]);
					counts[i]		= 0.0f;
					variances[i]	= 0.0f;
					diameters[i]	= 0.0f;
				}

				float sum_distsq = 0.0f;
				for (uint32 i = 0; i < data_size; ++i) {
					float		mindist;
					const E		&d		= t.data(i);
					uint32		closest = find_closest(t, d, cur_size, mindist);
					float		weight	= t.weightedsum(sums[closest], i);

					if (mindist >= diameters[closest]) {
						diameters[closest]	= mindist;
						furthests[closest]	= d;
					}
					mindist				*= weight;
					sum_distsq			+= mindist;
					variances[closest]	+= mindist;
					counts[closest]		+= weight;
				}
				for (uint32 i = 0; i < cur_size; ++i) {
					if (counts[i])	{
						float	rc	= 1.f / counts[i];
						codebook[i] = sums[i];
						t.scale(codebook[i], rc);
						variances[i] *= rc;
					} else {
						// Count is zero, split the entry with largest diameter, replacing this one with its furthest element.
						float	maxdiam		= diameters[0];
						uint32	furthest	= 0;
						for (uint32 cb = 1; cb < cur_size; ++cb) {
							if (diameters[cb] > maxdiam) {
								maxdiam		= diameters[cb];
								furthest	= cb;
							}
						}
						if (maxdiam > 0.0f) {
							codebook[i] = furthests[furthest];
							diameters[furthest] = 0.0f;
							variances[furthest] = 0.0f;
						}
					}
				}

				if (!fast) {
					float max_codevar = variances[0];
					split_target = 0;
					for (uint32 i = 1; i < cur_size; ++i) {
						if (variances[i] > max_codevar) {
							max_codevar		= variances[i];
							split_target	= i;
						}
					}
				}

				float new_variance = sum_distsq / w_sum;
				if (variance && (variance - new_variance) < thresh * variance)
					break;
				variance = new_variance;
			}
		}
		delete[] furthests;
		delete[] diameters;
		delete[] sums;
		delete[] variances;

		return cur_size;
	}

	void generate_indices(T &t, uint32 *index) {
		uint32	data_size	= uint32(t.size());

	#ifdef USE_NORMS
		make_norms(t, req_size);
	#endif
		for (uint32 i = 0; i < codebook.size(); ++i)
			counts[i]	= 0;

		for (uint32 i = 0; i < data_size; ++i) {
			const E		&d		= t.data(i);
			float		mindist;
			uint32		closest = find_closest(t, d, codebook.size(), mindist);
			index[i] = closest;
			++counts[closest];
		}
	}

};

// Nonlinear optimization: find n-dimensional vector x that minimize a function f(x) : R^n -> R
// Given:
//  - a container vector x,
//  - a function eval that evaluates both f(x) and \grad f(x)==(df/dx_1, df/dx_2, ..., df/dx_n);
//     its signature is: double eval(ptr_array<double> ret_grad), where the return value is f(x).
// The member function solve() iteratively calls eval to minimizes f, starting from some initial guess x_0 provided in x, and places the obtained minimum in x.  It returns false if the solution fails to converge.
// The optimization iterates until machine-precision convergence, or until a maximum number of evaluations provided using set_max_neval().

template<typename T> struct dotc_s {
	typename T_noconst<T>::type	v;
	dotc_s() : v()			{}
	template<typename A, typename B> void operator()(const A &a, const B &b)	{ v += a * b; } 
	operator T() const			{ return v; }
};

template<typename C1, typename C2> typename container_traits<C1>::element_type dotc(const C1 &c1, const C2 &c2) {
	return for_each2(c1, c2, dotc_s<typename container_traits<C1>::element_type>());
}
template<typename C> typename container_traits<C>::element_type len2c(const C &c) {
	return for_each2(c, c, dotc_s<typename container_traits<C>::element_type>());
}
template<typename C> typename container_traits<C>::element_type lenc(const C &c) {
    return sqrt(len2c(c));
}

#if 0
class NonlinearOptimization {
	typedef double(&FUNC)(ptr_array<double>);
	// Approach-independent:
	ptr_array<double>	x;					// view of user-supplied vector; stores initial estimate and final solution
	const int			n;					// x.num()
	int					max_neval;			// -1 is infinity

	// Approach-dependent:
	// Simple version of Limited-memory Broyden-Fletcher-Goldfarb-Shanno algorithm
	// See: https://en.wikipedia.org/wiki/Limited-memory_BFGS
	// Liu, D. C.; Nocedal, J. (1989). "On the limited memory method for large scale optimization"
	// Nocedal (1980).  "Updating quasi-Newton matrices with limited storage"

	static const int		NPRIOR	= 6;	// number of stored prior gradients and differences; recommended range 3..7
	const double			eps		= 1e-6;	// len(g) < eps * max(1, len(x)); default 1e-5 in github.com/chokkan/liblbfgs
	dynamic_array<double>	g;				// current gradient
	dynamic_array<double>	tmp;			// temporary storage
	dynamic_array<double>	xinit;			// x at start of line search
	double					rho[NPRIOR];	// scalars \rho from [Nocedal 1980]
	double					alphak[NPRIOR];	// used in the formula that computes H*g
	array_2d<double>		as;				// last NPRIOR search step          (s_k = x_k - x_{k-1})
	array_2d<double>		ay;				// last NPRIOR gradient differences (y_k = g_k - g_{k-1})

	// Backtracking line search to find approximate minimum of f=eval() along direction p, with initial step size alpha
	// See: https://en.wikipedia.org/wiki/Backtracking_line_search

	template<typename EVAL = FUNC> bool line_search(EVAL eval, double &f, ptr_array<double> p, double &alpha, int &neval) {
		const double finit	= f;		// initial f
		const double c		= .5;		// factor for Armijo-Goldstein condition
		const double tau	= .5;		// backtracking factor (used in geometric sequence on alpha)
		const double m		= dotc(g, p);
		ISO_ASSERT(m < 0);				// must be a descent direction
		
		for (xinit = x;;) {
			for (int i = 0; i < n; i++)
				x[i] = xinit[i] + alpha * p[i];

			f = eval(g);				// evaluate both objective f and its gradient g at x
			neval++;
			
			if (finit - f >= -alpha * (c * m))
				return true;			// Armijo-Goldstein condition satisfied

			alpha *= tau;
			if (alpha < 1e-10)			// line_search fails to converge
				return false;
		}
	}

public:
	NonlinearOptimization(ptr_array<double> _x) : x(_x), n(x.size()), g(n), tmp(n), xinit(n), as(NPRIOR, n), ay(NPRIOR, n), max_neval(-1) {}

	void set_max_neval(int _max_neval)	{
		max_neval = _max_neval;
	}

	template<typename EVAL = FUNC> bool solve(EVAL eval) {
		int			ic		= 0;			// index into circular buffers as, ay
		double		f		= eval(g);		// evaluate both the objective f and its gradient g at x
		int			neval	= 1;			// number of times that eval is called

		// initial line search direction
		for (int i = 0; i < n; i++)
			as[ic][i] = -g[i];

		// initial step size
		double alpha = rsqrt(len2c(g));

		for (int iter = 0; iter < 1000; ++iter) {
			// archive the current gradient
			tmp = g;

			if (!line_search(eval, f, as[ic], alpha, neval))
				return false;

			// reached maximum number of function evaluations?
			if (max_neval >= 0 && neval >= max_neval)
				return true;

			// numerically converged?
			if (lenc(g) / max(lenc(x), 1) <= eps)
				return true;

			// record the search step
			as[ic] *= alpha;
			// record the gradient difference
			for (int i = 0; i < n; i++)
				ay[ic][i] = g[i] - tmp[i];

			const double ys = dotc(ay[ic], as[ic]);
			const double yy = dotc(ay[ic], ay[ic]);

			// Compute -H * g using [Nocedal 1980].
			rho[ic] = 1 / ys;

			// roll the circular buffers
			ic = wrap(ic + 1, NPRIOR);
			for (int i = 0; i < n; i++)
				tmp[i] = -g[i];

			const int mm = min(iter + 1, NPRIOR);
			for (int i = 0; i < mm; i++) {
				ic			= wrap(ic - 1, NPRIOR);
				alphak[ic]	= rho[ic] * dotc(as[ic], tmp);
				tmp			+= -alphak[ic] * ay[ic];
			}

			tmp *= ys / yy;
			for (int i = 0; i < mm; i++) {
				const double beta = alphak[ic] - rho[ic] * dotc(ay[ic], tmp);
				tmp += beta * as[ic];
				ic	= wrap(ic + 1, NPRIOR);
			}
			as[ic]	= tmp;		// new search direction
			alpha	= 1;		// new step size
		}
	}
};
#endif

} // namespace iso

#endif // VQ_H
