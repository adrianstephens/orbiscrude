#include "reed_solomon.h"

using namespace iso;

// generate GF(2^mm) from the irreducible polynomial p(X) in pp[0]..pp[mm] lookup tables:
// index form -> polynomial form	alpha_to[]				= j = alpha**i;
// polynomial form -> index form	index_of[j = alpha**i]	= i
// alpha = 2 is the primitive element of GF(2 ^ mm)
void RScodec::generate_gf(uint32 pp) {
	int		mask = 1;
	alpha_to[mm] = 0;
	for (int i = 0; i < mm; i++) {
		alpha_to[i]		= mask;
		index_of[mask]	= i;
		if ((pp >> (mm - 1 - i)) & 1)
			alpha_to[mm] ^= mask;
		mask <<= 1;
	}

	index_of[alpha_to[mm]] = mm;
	mask >>= 1;
	for (int i = mm + 1; i < nn; i++) {
		alpha_to[i] = alpha_to[i - 1] >= mask
			? alpha_to[mm] ^ ((alpha_to[i - 1] ^ mask) << 1)
			: alpha_to[i - 1] << 1;
		index_of[alpha_to[i]] = i;
	}
	index_of[0] = -1;
}


// Obtain the generator polynomial of the tt-error correcting, length nn=(2^mm -1) Reed Solomon code from the product of (X+alpha**i), i=1..2*tt
bool RScodec::generate_poly() {
	gg[0] = 2;	// primitive element alpha = 2 for GF(2**mm)
	gg[1] = 1;	// g(x) = (X+alpha) initially
	int		bb	= nn - kk;; // length of parity data
	for (int i = 2; i <= bb; i++) {
		gg[i] = 1;
		for (int j = i; j--; ) {
			if (gg[j] < 0)
				return false;

			if (gg[j] == 0) {
				gg[j] = gg[j - 1];

			} else {
				int tmp = (index_of[gg[j]] + i) % nn;
				if (tmp < 0)
					return false;
				gg[j] = gg[j - 1] ^ alpha_to[tmp];
			}
		}
		gg[0] = alpha_to[(index_of[gg[0]] + i) % nn];	// gg[0] can never be zero
	}
	
	// convert gg[] to index form for quicker encoding
	for (int i = 0; i <= bb; i++)
		gg[i] = index_of[gg[i]];

	return true;
}

//assume we have received bits grouped into mm-bit symbols in recd[i], i=0..(nn-1), and recd[i] is index form (ie as powers of alpha).
//We first compute the 2*tt syndromes by substituting alpha**i into rec(X) and evaluating, storing the syndromes in s[i], i=1..2tt (leave s[0] zero).
//Then we use the Berlekamp iteration to find the error location polynomial elp[i].
//If the degree of the elp is >tt, we cannot correct all the errors and output the information symbols uncorrected.
//If the degree of elp is <=tt, we substitute alpha**i , i=1..n into the elp to get the roots, hence the inverse roots, the error location numbers.
//If the number of errors located does not equal the degree of the elp, we have more than tt errors and cannot correct them.
//Otherwise, we then solve for the error value at the error location and correct the error.
//
//returns number of corrected errors or -1 if can't correct it

int RScodec::decode(uint8 *data) {
	if (!isOk)
		return -1;

	int		bb			= nn - kk;; // length of parity data

	temp_array<int>		recd(nn);
	temp_array<int>		s(bb + 1);

	for (int i = 0, j = bb; i < kk; i++, j++)
		recd[j] = index_of[data[j]];		// put data in recd[i] into index form

	for (int i = kk, j = 0; i < nn; i++, j++)
		recd[j] = index_of[data[j]];		// put data in recd[i] into index form

	// first form the syndromes
	bool	syn_error	= false;
	for (int i = 1; i <= bb; i++) {
		s[i] = 0;
		for (int j = 0; j < nn; j++) {
			if (recd[j] != -1)
				s[i] ^= alpha_to[(recd[j] + i * j) % nn];	// recd[j] in index form
		}
		// convert syndrome from polynomial form to index form 
		if (s[i] != 0)
			syn_error = true;	// set flag if non-zero syndrome => error
		s[i] = index_of[s[i]];
	}

	if (!syn_error)	// no non-zero syndromes => no errors: output is received codeword
		return 0;

	// errors are present, try and correct

	// compute the error location polynomial via the Berlekamp iterative algorithm, following the terminology of Lin and Costello:
	// d[u]		is the 'mu'th discrepancy, where u='mu'+1 and 'mu' is the step number ranging from -1 to 2*tt (see L&C),
	// l[u]		is the degree of the elp at that step, and
	// u_l[u]	is the difference between the step number and the degree of the elp

	temp_array<temp_array<int>>	elp(bb + 2, bb);
	temp_array<int>		d(bb + 2);
	temp_array<int>		l(bb + 2);
	temp_array<int>		u_lu(bb + 2);
	temp_array<int>		root(tt);
	temp_array<int>		loc(tt);
	temp_array<int>		z(tt+1);
	temp_array<int>		err(nn);
	temp_array<int>		reg(tt + 1);

	// initialise table entries
	d[0] = 0;			// index form
	d[1] = s[1];		// index form
	elp[0][0] = 0;		// index form
	elp[1][0] = 1;		// polynomial form
	for (int i = 1; i < bb; i++) {
		elp[0][i] = -1;	// index form
		elp[1][i] = 0;	// polynomial form
	}
	l[0]	= 0;
	l[1]	= 0;
	u_lu[0] = -1;
	u_lu[1] = 0;

	int	u	= 0;

	do {
		u++;
		if (d[u] == -1) {
			l[u + 1] = l[u];
			for (int i = 0; i <= l[u]; i++) {
				elp[u + 1][i]	= elp[u][i];
				elp[u][i]		= index_of[elp[u][i]];
			}

		} else {
			// search for words with greatest u_lu[q] for which d[q]!=0
			int q = u - 1;
			while (q > 0 && d[q] == -1)
				--q;

			// have found first non-zero d[q]
			if (q > 0) {
				int j = q;
				do {
					--j;
					if (d[j] != -1 && u_lu[q] < u_lu[j])
						q = j;
				} while (j > 0);
			}

			// have now found q such that d[u] != 0 and u_lu[q] is maximum store degree of new elp polynomial
			l[u + 1] = max(l[u], l[q] + u - q);

			// form new elp(x)
			for (int i = 0; i < bb; i++)
				elp[u + 1][i] = 0;

			for (int i = 0; i <= l[q]; i++) {
				if (elp[q][i] != -1)
					elp[u + 1][i + u - q] = alpha_to[(d[u] + nn - d[q] + elp[q][i]) % nn];
			}
			for (int i = 0; i <= l[u]; i++) {
				elp[u + 1][i] ^= elp[u][i];
				elp[u][i] = index_of[elp[u][i]];	// convert old elp value to index
			}
		}
		u_lu[u + 1] = u - l[u + 1];

		// form (u+1)th discrepancy
		if (u < bb) {	// no discrepancy computed on last iteration
			d[u + 1] = s[u + 1] != -1 ? alpha_to[s[u + 1]] : 0;
			
			for (int i = 1; i <= l[u + 1]; i++) {
				if (s[u + 1 - i] != -1 && elp[u + 1][i] != 0)
					d[u + 1] ^= alpha_to[(s[u + 1 - i] + index_of[elp[u + 1][i]]) % nn];
			}
			d[u + 1] = index_of[d[u + 1]];	// put d[u+1] into index form
		}
	} while (u < bb && l[u + 1] <= tt);

	++u;
	if (l[u] > tt)		// elp has degree has degree >tt hence cannot solve
		return -1;

	// correct the error:

	// put elp into index form
	for (int i = 0; i <= l[u]; i++)
		elp[u][i] = index_of[elp[u][i]];

	// find roots of the error location polynomial
	for (int i = 1; i <= l[u]; i++)
		reg[i] = elp[u][i];

	int	count = 0;
	for (int i = 1; i <= nn; i++) {
		int	q = 1;
		for (int j = 1; j <= l[u]; j++) {
			if (reg[j] != -1) {
				reg[j] = (reg[j] + j) % nn;
				q ^= alpha_to[reg[j]];
			}
		}
		if (!q) {	// store root and error location number indices
			root[count]	= i;
			loc[count]	= nn - i;
			count++;
		}
	}

	if (count != l[u])		// no. roots != degree of elp => >tt errors and cannot solve
		return -1;

	// no. roots = degree of elp hence <= tt errors

	// form polynomial z(x)
	for (int i = 1; i <= l[u]; i++) {	// Z[0] = 1 always - do not need
		z[i]	= (s[i]			== -1 ? 0 : alpha_to[s[i]])
				^ (elp[u][i]	== -1 ? 0 : alpha_to[elp[u][i]]);

		for (int j = 1; j < i; j++) {
			if (s[j] != -1 && elp[u][i - j] != -1)
				z[i] ^= alpha_to[(elp[u][i - j] + s[j]) % nn];
		}
		z[i] = index_of[z[i]];		// put into index form
	}

	// evaluate errors at locations given by error location numbers loc[i]
	for (int i = 0; i < nn; i++)
		err[i] = 0;

	// compute numerator of error term first
	for (int i = 0; i < l[u]; i++) {
		err[loc[i]] = 1;	// accounts for z[0]
		for (int j = 1; j <= l[u]; j++) {
			if (z[j] != -1)
				err[loc[i]] ^= alpha_to[(z[j] + j*root[i]) % nn];
		}
		if (err[loc[i]] != 0) {
			err[loc[i]] = index_of[err[loc[i]]];
			int	q = 0;	// form denominator of error term
			for (int j = 0; j < l[u]; j++) {
				if (j != i)
					q += index_of[1 ^ alpha_to[(loc[j] + root[i]) % nn]];
			}
			q = q % nn;
			err[loc[i]] = alpha_to[(err[loc[i]] - q + nn) % nn];
			data[loc[i]] ^= err[loc[i]];	//change errors by correct data, in polynomial form
		}
	}
	return count;
}

// take the string of symbols in data[i], i=0..(k-1) and encode systematically to produce 2*tt parity symbols in parity[0]..parity[2*tt-1]
// data[] is input and parity[] is output in polynomial form.
// Encoding is done by using a feedback shift register with appropriate connections specified by the elements of gg[], which was generated above.
// Codeword is c(X) = data(X)*X**(nn-kk)+ b(X)
bool RScodec::encode(uint8 *data, uint8 *parity) {
	if (!isOk)
		return false;

	int		bb		= nn - kk; // length of parity data
	for (int i = 0; i < bb; i++)
		parity[i] = 0;

	for (int i = kk - 1; i >= 0; i--) {
		int	feedback = index_of[data[i]^parity[bb-1]];
		if (feedback != -1) {
			for (int j = bb - 1; j > 0; j--)
				parity[j] = parity[j - 1] ^ (gg[j] == -1 ? 0 : alpha_to[(gg[j] + feedback) % nn]);
			parity[0] = alpha_to[(gg[0] + feedback) % nn];

		} else {
			for (int j = bb - 1; j > 0; j--)
				parity[j] = parity[j - 1];
			parity[0] = 0;
		}
	}
	return true;
}
