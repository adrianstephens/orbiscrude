#include "samplefile.h"
#include "iso/iso_convert.h"
#include "filetypes/iff.h"
#include "base/bits.h"

using namespace iso;

//-----------------------------------------------------------------------------
//
//	GENERATE CODEBOOK
//
//-----------------------------------------------------------------------------

#define TINY			1e-10

#define DEFAULT_FSIZE	16      // Larger frame makes things quicker
#define DEFAULT_ORDER	2
#define DEFAULT_THRESH	10.0
#define DEFAULT_SIZE	2
#define DEFAULT_REFINE	2

#define VECTORSIZE		8
#define VSCALE			2048
#define MINCOEF			-32768
#define MAXCOEF			32767

#define FRAMEBYTES		9
#define FRAMESIZE		16
#define SAMPLEBITS		4
#define MAXSCALE		12

#define MAXCLIP			1
#define MAXITER			2

struct ALADPCMloop {
	uint32		start;
	uint32		end;
	uint32		count;
	short		state[FRAMESIZE];
};

//inline double abs(double a) { return a < 0 ? -a : a; }

// Calculate autocorrelation of a signal
void acf(short *sig, int len, double *ac, int nlags)
{
	for (int i = 0; i < nlags; i++){
		double m = 0;
		for (int j = 0; j < len-i; j++)
			m += sig[j] * sig[j+i];
		ac[i] = m;
	}
}

// Levinson-Durbin method for solving for the Reflection coefficients from the autocorrelation coefficients.
// Stability is checked. Return value is >0 if solution is unstable. Tap values are also provided.
int durbin(double *ac, int order, double *ref, double *taps, double *e2)
{
	int		stable	= 0;
	double	e		= ac[0];

	taps[0] = 1.0;

	for (int i = 1; i <= order; i++ ){
		double m = 0;
		for (int j = 1; j < i; j++)
			m += taps[j] * ac[i - j];

		// Calculate reflection coefficient value - which is taps[i] in this case.
		ref[i] = taps[i] = e > 0 ? -(ac[i] + m) / e : 0;

		if (abs(ref[i]) > 1.0)
			stable++;

		 // Recalculate the tap values
		for (int j = 1; j < i; j++)
			taps[j] = taps[j] + taps[i] * taps[i-j];

		// Calculate forward error after predictor
		e *= (1 - taps[i] * taps[i]);

	}
	*e2 = e;
	return stable;
}

// Get the tap values from reflecion coefficients.
void afromk(double *ref, double *taps, int order)
{
	taps[0] = 1.0;
	for (int i = 1; i <= order; i++ ){
		taps[i] = ref[i];
		for (int j = 1; j < i; j++)
			taps[j] += taps[i] * taps[i-j];
	}
}

// Get the reflecion coefficients from the tap values.
int kfroma(double *taps, double *ref, int order)
{
	int		stable	= 0;
	double	*tmp	= new double[order + 1];

	ref[order] = taps[order];

	for (int i = order - 1; i > 0; i--){
		for (int j = 0; j <= i; j++){
			double den = 1-ref[i+1]*ref[i+1];
			// Check here for unstable filter
			if (den == 0) {
				delete[] tmp;
				return 1;
			}
			tmp[j] = (taps[j] - ref[i + 1] * taps[i + 1 - j]) / den;
		}

		for (int j = 0; j <= i; j++)
			taps[j] = tmp[j];

		ref[i] = tmp[i];
		if (abs(ref[i]) > 1.0)
			stable++;
	}
	delete[] tmp;
	return stable;
}

// Calculate the signal autocorrelation from the model tap values
// See Atal and Hanauer, Journal of the Acoustical Society of America, Vol 50, 1971, pp. 637-655.
void rfroma(double *a, int n, double *r)
{
	double **aa	= new double*[n + 1];
	aa[n]		= new double[n + 1];
	aa[n][0]	= 1.0;

	for (int i = 1; i <= n; i++)
		aa[n][i] = -a[i];

	for (int i = n; i >= 1; i--) {
		aa[i - 1] = new double[i];
		double	ref = 1 / (1 - aa[i][i] * aa[i][i]);

		// Calculate the tap values for this i
		for (int j = 1; j <= i - 1; j++)
			aa[i - 1][j] = (aa[i][j] + aa[i][i] * aa[i][i-j]) * ref;
	}

	// Now calculate the autocorrelation coefficients
	r[0] = 1.0;
	for (int i = 1; i <= n; i++){
		r[i] = 0;
		for (int j = 1; j <= i; j++)
			r[i] += aa[i][j] * r[i-j];
	}

	// Free storage
	for (int i = 0; i < n + 1; i++)
		delete[] aa[i];
	delete[] aa;
}

// Calculate the normalized model distance
// See, Gray, et al IEEE Transacations, ASSP-28, No. 4 Aug. 1980, pp. 367-376.
double model_dist(double *ta, double *sa, int order)
{
	double *r	= new double[order+1];
	double *ra	= new double[order+1];

	// Calculate signal autocorrelation coefficients from model sa
	rfroma(sa, order, r);

	// Calculate the coefficient autocorrelations from model ta
	for (int i = 0; i <= order; i++){
		ra[i] = 0;
		for (int j = 0; j <= order-i; j++)
			ra[i] += ta[j] * ta[j+i];
	}

	// Model distance
	double d = r[0] * ra[0];
	for (int i = 1; i <= order; i++)
		d += 2 * r[i] * ra[i];

	delete[] r;
	delete[] ra;
	return d;
}

// Non Toeplitz autocorrelation matrix
void acmat(short *in_buffer, int order, int length, double **a)
{
	for (int i = 1; i <= order; i++) {
		for (int j = 1; j <= order; j++) {
			a[i][j] = 0;
			for (int n = 0; n < length; n++)
				a[i][j] += in_buffer[n-i]*in_buffer[n-j];
		}
	}
}

void acvect(short *in_buffer, int order, int length, double *a)
{
	for (int i = 0; i <= order; i++) {
		a[i] = 0;
		for (int n = 0; n < length; n++)
			a[i] -= in_buffer[n - i] * in_buffer[n];
	}
}

bool lud(double **a, int n, int *indx, int *d)
{
	int		imax	= 0;
	double	*vv		= new double[n+1];
	*d = 1;

	for (int i = 1; i <= n; i++){
		double	big = 0.0;
		for (int j = 1; j <= n; j++) {
			double temp = abs(a[i][j]);
			if (temp > big)
				big = temp;
		}
		if (big == 0.0)
			return true;
		vv[i] = 1.0 / big;
	}
	for (int j = 1; j <= n; j++){
		for (int i = 1; i < j; i++) {
			double sum = a[i][j];
			for (int k = 1; k < i; k++)
				sum -= a[i][k] * a[k][j];
			a[i][j] = sum;
		}
		double big = 0.0;
		for (int i = j; i <= n; i++){
			double sum = a[i][j];
			for (int k = 1; k < j; k++)
				sum -= a[i][k] * a[k][j];
			a[i][j] = sum;
			double	dum = vv[i] * abs(sum);
			if (dum >= big){
				big		= dum;
				imax	= i;
			}
		}
		if (j != imax){
			for (int k = 1; k <= n; k++){
				double	dum = a[imax][k];
				a[imax][k]	= a[j][k];
				a[j][k]		= dum;
			}
			*d = -(*d);
			vv[imax] = vv[j];
		}
		indx[j] = imax;
		if (a[j][j] == 0.0)
			return true;// Matrix is singular

		if (j != n){
			double	dum = 1.0 / a[j][j];
			for (int i = j + 1; i <= n; i++)
				a[i][j] *= dum;
		}
	}
	delete[] vv;

	// Search for maximum and minimum magnitude pivots
	double pmax = 0, pmin = 1/TINY;
	for (int j = 1; j <= n; j++){
		double	mag = abs(a[j][j]);
		if (mag < pmin)
			pmin = mag;
		if (mag > pmax)
			pmax = mag;
	}
	return pmin / pmax < TINY; // Ad hoc singularity check. If the ratio of pmin/pmax is less than TINY call it singular
}

void lubksb(double **a, int n, int *indx, double b[])
{
	for (int i = 1, ii = 0; i <= n; i++){
		int		ip	= indx[i];
		double	sum = b[ip];
		b[ip] = b[i];
		if (ii) {
			for (int j = ii; j <= i-1; j++)
				sum -= a[i][j] * b[j];
		} else if (sum) {
			ii = i;
		}
		b[i] = sum;
	}
	for (int i = n; i >= 1; i--) {
		double sum = b[i];
		for (int j = i + 1; j <= n; j++)
			sum -= a[i][j] * b[j];
		b[i] = sum / a[i][i];
	}
}

//-----------------------------------------------------------------------------

// Double the size of the codebook by splitting each entry along the direction 'dir'
void split(double **codebook, double *dir, int order, int n_entries, double delta)
{
	for (int i = 0; i < n_entries; i++)
		for (int j = 0; j <= order; j++)
			codebook[i + n_entries][j] = codebook[i][j] + delta * dir[j];
}

// Standard Lloyd algorithm for iterative refinement of the codebook
void refine(double **codebook, int order, int n_entries, double **training, int nframes, int iterations, double converge)
{
	int		*count		= new int[n_entries];
	double	**centroids	= new double*[n_entries];
	double	*ac			= new double[order+1];

	for (int i = 0; i < n_entries; i++)
		centroids[i] = new double[order+1];

	for (int iter = 0; iter < iterations; iter++){
		// Go through each training vector and find the nearest neighbour
		for (int j = 0; j < n_entries; j++){
			count[j] = 0;
			for (int i = 0; i <= order; i++)
				centroids[j][i] = 0.0;
		}

		for (int tv = 0; tv < nframes; tv++){
			int		ne	= 0;
			double	min	= 1e30;
			for (int i = 0; i < n_entries; i++){
				double dist = model_dist(codebook[i], training[tv], order);
				if (dist < min){
					min	= dist;
					ne	= i;
				}
			}

			// Add the autocorrelation of this training vector to the centroid
			count[ne] += 1;
			rfroma(training[tv], order, ac);
			for (int i = 0; i <= order; i++)
				centroids[ne][i] += ac[i];
		}

		// Get the average
		for (int i = 0; i < n_entries; i++) {
			if (count[i])
				for (int j = 0; j <= order; j++)
					centroids[i][j] /= count[i];
		}

		// Redefine the codebook
		for (int i = 0; i < n_entries; i++) {
			double	e2;
			durbin(centroids[i], order, ac, codebook[i], &e2);
			// Stabilize - could put this in durbin
			for (int j = 1; j <= order; j++){
				if (ac[j] >= +1.0)
					ac[j] = +1.0 - TINY;
				if (ac[j] <= -1.0)
					ac[j] = -1.0 + TINY;
			}
			afromk(ac, codebook[i], order);
		}
	}

	delete[] count;
	for (int i = 0; i < n_entries; i++)
		delete[] centroids[i];
	delete[] centroids;
	delete[] ac;
}

//-----------------------------------------------------------------------------

int **make_coeff(double *entry, int order)
{
	double	**cmat	= new double*[VECTORSIZE];
	int		**coeff	= new int*[VECTORSIZE];

	for (int i = 0; i < VECTORSIZE; i++) {
		cmat[i]		= new double[order];
		coeff[i]	= new int[VECTORSIZE + order];
	}

	// Initialize matrix
	for (int i = 0; i < order; i++){
		for (int j = 0; j < i; j++)
			cmat[i][j] = 0.0;
		for (int j = i; j < order; j++)
			cmat[i][j] = -entry[order-(j-i)];
	}
	for (int i = order; i < VECTORSIZE; i++)
		for (int j = 0; j < order; j++)
			cmat[i][j] = 0.0;

	// Now recursively compute the VECTORSIZE rows
	for (int i = 1; i < VECTORSIZE; i++) {
		for (int j = 1; j <= order; j++)
			if (i >= j)
				for (int k = 0; k < order; k++)
					cmat[i][k] -= entry[j] * cmat[i - j][k];
	}


	// Now print them out in columns
	int		overflow = 0;
	for (int i = 0; i < order; i++) {
		for (int j = 0; j < VECTORSIZE; j++) {
			double	e = VSCALE * cmat[j][i];
			int		r;
			if (e < 0){
				r = int(e - 0.5);
				if (r < MINCOEF)
					overflow++;
			} else {
				r = int(e + 0.5);
				if (r > MAXCOEF)
					overflow++;
			}
			coeff[j][i] = r;
		}
	}

	for (int i = 0; i < VECTORSIZE; i++)
		delete[] cmat[i];
	delete[] cmat;

	// Now copy the right stuff to the rest of the matrix
	for (int k = 1; k < VECTORSIZE; k++)
		coeff[k][order] = coeff[k-1][order-1];
	coeff[0][order] = VSCALE;
	for (int k = 1; k < VECTORSIZE; k++)
		for (int j = 0; j < VECTORSIZE; j++)
			coeff[j][k + order] = j < k ? 0 : coeff[j-k][order];

	return coeff;
}
//-----------------------------------------------------------------------------

class ADPCMcodebook {
	int		order;
	int		frame_size;
	int		n_entries;
	int		***coefTable;
	int		state[FRAMESIZE];
public:
	ADPCMcodebook(sample &sm
		, int _order		= DEFAULT_ORDER
		, int _frame_size	= DEFAULT_FSIZE
		, int table_size	= DEFAULT_SIZE
		, int refine_iter	= DEFAULT_REFINE
		, double thresh		= DEFAULT_THRESH
	);
	ADPCMcodebook(istream_ref file);
	~ADPCMcodebook();

	void	DecodeFrame(uint8 *adpcm, int *outp);
	uint8	*EncodeFrame(uint8 *outBuffer, short *inBuffer, int nsam);

	void	Write(ostream_ref file);
	void	GetState(short *_state);

	int		Order()			{ return order;		}
	int		NumEntries()	{ return n_entries;	}
	int***	Coeffs()		{ return coefTable;	}
};


//-----------------------------------------------------------------------------

ADPCMcodebook::ADPCMcodebook(sample &sm, int _order, int _frame_size, int table_size, int refine_iter, double thresh)
	: order(_order), frame_size(_frame_size)
	, coefTable(NULL)
	, n_entries(0)
{
	clear(state);

	if (sm.Channels() != 1 || sm.Bits() != 16)
		return; //only 1 channel, 16 bit supported

	// Initialize codebook storage
	double	**codebook	= new double*[uint32(1 << table_size)];
	for (int i = 0; i < (1 << table_size); i++)
		codebook[i] = new double[order + 1];

	// Splitting direction
	double	*dir		= new double[order + 1];
	short	*in_buffer	= new short[2 * frame_size];
	double	*ac			= new double[order + 1];
	double	*ref		= new double[order + 1];

	// For matrix method
	double	**a			= new double*[order + 1];
	int		*indx		= new int[order + 1];
	for (int i = 0; i <= order; i++)
		a[i] = new double[order+1];

	long	nsamples	= sm.Length();
	long	s			= 0;
	short	*samples	= (short*)sm.Samples();

	// Reserve storage for the training data
	double	**training	= new double*[(nsamples + frame_size - 1) / frame_size];
	int		nframes		= 0;

	while (s + frame_size < nsamples){
		memcpy(in_buffer + frame_size, samples + s, frame_size * sizeof(short));

		acvect(in_buffer+frame_size, order, frame_size, ac);
		if (abs(ac[0]) > thresh){
			acmat(in_buffer+frame_size, order, frame_size, a);

			// Lower-upper decomposition
			int	d;
			if (!lud(a, order, indx, &d)){

				// Put solution in ac
				lubksb(a, order, indx, ac);
				ac[0] = 1.0;

				// Convert to reflection coefficients - reject unstable vectors
				if (!kfroma(ac, ref, order)){
					//The training data is stored as tap values
					training[nframes]		= new double[order + 1];
					training[nframes][0]	= 1.0;
					for (int i = 1; i <= order; i++){
						// Stabilize the filter
						if (ref[i] >= 1.0)
							ref[i] = 1.0 - TINY;
						if (ref[i] <= -1.0)
							ref[i] = -1.0 + TINY;
					}
					afromk(ref, training[nframes], order);
					nframes++;
				}
			}
		}
		for (int i = 0; i < frame_size; i++)
			in_buffer[i] = in_buffer[i + frame_size];
		s += frame_size;
	}

	// To start things off find the average auto-correlation over the complete data set.
	ac[0] = 1.0;
	for (int j = 1; j <= order; j++)
		ac[j] = 0;
	for (int i = 0; i < nframes; i++){
		rfroma(training[i], order, codebook[0]);
		for (int j = 1; j <= order; j++)
			ac[j] += codebook[0][j];
	}
	for (int j = 1; j <= order; j++)
		ac[j] = ac[j] / nframes;

	// The average model
	double	e2;
	durbin(ac, order, ref, codebook[0], &e2);

	// Stabilize - could put this in durbin
	for (int j = 1; j <= order; j++){
		if (ref[j] >= +1.0)
			ref[j] = +1.0 - TINY;
		if (ref[j] <= -1.0)
			ref[j] = -1.0 + TINY;
	}
	afromk(ref, codebook[0], order);

	int	actual_size = 0;
	while (actual_size < table_size){
		n_entries = 1 << actual_size;
		// Split each codebook template into two - the original and a shifted version
		for (int i = 0; i <= order; i++)
			dir[i] = 0;
		dir[order - 1] = -1.0;

		split(codebook,dir,order,n_entries,0.01);

		// Iterative refinement of templates
		actual_size++;
		refine(codebook, order, 1 << actual_size, training, nframes, refine_iter, 0);
	}
	n_entries = 1 << actual_size;

	coefTable = new int**[n_entries];
	for (int i = 0; i < n_entries; i++)
		coefTable[i] = make_coeff(codebook[i], order);

	//-----------------------------------
	// Free
	for (int i = 0; i < (1 << table_size); i++)
		delete[] codebook[i];
	delete[] codebook;
	delete[] dir;
	delete[] in_buffer;
	delete[] ac;
	delete[] ref;
	for (int i = 0; i <= order; i++)
		delete[] a[i];
	delete[] a;
	delete[] indx;
	for (int i =0; i < nframes; i++ )
		delete[] training[i];
	delete[] training;
}

ADPCMcodebook::ADPCMcodebook(istream_ref file)
{
	order		= file.get<uint16be>();
	n_entries	= file.get<uint16be>();
	coefTable	= new int**[n_entries];

	for (int i = 0; i < n_entries; i++){
		int	**coeff = new int*[VECTORSIZE];
		coefTable[i] = coeff;
		for (int j = 0; j < VECTORSIZE; j++)
			coeff[j] = new int[VECTORSIZE + order];

		for (int j = 0; j < order; j++)
			for (int k = 0; k < VECTORSIZE; k++)
				coeff[k][j] = file.get<int16be>();

		// Now copy the right stuff to the rest of the matrix
		for (int k = 1; k < VECTORSIZE; k++)
			coeff[k][order] = coeff[k-1][order-1];
		coeff[0][order] = VSCALE;
		for (int k = 1; k < VECTORSIZE; k++)
			for (int j = 0; j < VECTORSIZE; j++)
				coeff[j][k + order] = j < k ? 0 : coeff[j-k][order];
	}
}

void ADPCMcodebook::Write(ostream_ref file)
{
	file.write(uint16be(order));
	file.write(uint16be(n_entries));
	for (int i = 0; i < n_entries; i++)
		for (int j = 0; j < order; j++)
			for (int k = 0; k < VECTORSIZE; k++)
				file.write(int16be(coefTable[i][k][j]));
}

void ADPCMcodebook::GetState(short *_state)
{
	for (int i = 0; i < FRAMESIZE; i++) {
		if (state[i] > +32767)
			state[i] =  32767;
		if (state[i] < -32767)
			state[i] = -32767;
		_state[i] = (short)state[i];
	}
}

ADPCMcodebook::~ADPCMcodebook()
{
	for (int i = 0; i < n_entries; i++) {
		for (int j = 0; j < VECTORSIZE; j++)
			delete[] coefTable[i][j];
		delete[] coefTable[i];
	}
	delete[] coefTable;
}

//-----------------------------------------------------------------------------
//	ENCODE DATA
//-----------------------------------------------------------------------------

short qsample(float x, int scale) {
	return short(x / float(scale) + (x > 0 ? 0.4999999 :  - 0.4999999));
}

void clamp(int fs, float *e, int *ie, int bits) {
	float	llevel = -float(1L << (bits - 1));
	float	ulevel = -llevel - 1;

	for (int i = 0; i < fs; i++){
		if (e[i] > ulevel)
			e[i] = ulevel;
		if (e[i] < llevel)
			e[i] = llevel;
		ie[i] = e[i] > 0 ? int(e[i] + 0.5) : int(e[i] - 0.5);
	}
}

int inner_product(int length, int *v1, int *v2)
{
	int	out	= 0;
	for (int j = 0; j < length; j++)
		out += *v1++ * *v2++;

	// Mimic truncation on the RSP - ie always towards more negative
	int	dout	= int(out) / VSCALE;
	return out < VSCALE * dout ? dout - 1 : dout;
}

uint8 *ADPCMcodebook::EncodeFrame(uint8 *outBuffer, short *inBuffer, int nsam)
{
	short		ix[FRAMESIZE];
	int			prediction[FRAMESIZE], inVector[FRAMESIZE], saveState[FRAMESIZE], ie[FRAMESIZE];
	float		e[FRAMESIZE];

	//Clear input for nsam less than FRAMESIZE
	while (nsam < FRAMESIZE)
		inBuffer[nsam++] = 0;

	// Maximum and minimum allowable levels
	int	llevel = -(short)(1L << (SAMPLEBITS - 1));
	int	ulevel = -llevel - 1;

	// Find the prediction error for each possible predictor. Uses the vector method.
	float	min = 1e30f;
	int		optimalp = 0;
	for (int k = 0; k < n_entries; k++){
		// Set up previous frame outputs - use quantized outputs
		for (int i = 0; i < order; i++)
			inVector[i] = state[FRAMESIZE-order+i];

		for (int i = 0; i < VECTORSIZE; i++){
			prediction[i]				= inner_product(order+i, coefTable[k][i], inVector);
			inVector[i+order]			= inBuffer[i] - prediction[i];
			e[i]						= (float)inVector[i+order];
		}

		// Set up previous vector outputs, for next lot of VECTORSIZE
		for (int i = 0; i < order; i++)
			inVector[i] = prediction[VECTORSIZE-order+i] + inVector[VECTORSIZE+i];

		for (int i = 0; i < VECTORSIZE; i++){
			prediction[i+VECTORSIZE]	= inner_product(order+i, coefTable[k][i], inVector);
			inVector[i+order]			= inBuffer[i+VECTORSIZE] - prediction[i+VECTORSIZE];
			e[i+VECTORSIZE]				= (float)inVector[i+order];
		}

		// Now find the error measure for this frame and check against the current best.
		float	se = 0;
		for (int j = 0; j < FRAMESIZE; j++)
			se += e[j] * e[j];

		if (se < min) {
			min		 = se;
			optimalp = k;
		}
	}

	// Re-calculate final prediction error using optimal predictor
	for (int i = 0; i < order; i++)
		inVector[i] = state[FRAMESIZE-order+i];

	for (int i = 0; i < VECTORSIZE; i++){
		prediction[i]				= inner_product(order+i, coefTable[optimalp][i], inVector);
		inVector[i+order]			= inBuffer[i] - prediction[i];
		e[i]						= (float)inVector[i+order];
	}
	for (int i = 0; i < order; i++)
		inVector[i] = prediction[VECTORSIZE-order+i] + inVector[VECTORSIZE+i];

	for (int i = 0; i < VECTORSIZE; i++){
		prediction[i+VECTORSIZE]	= inner_product(order+i, coefTable[optimalp][i], inVector);
		inVector[i+order]			= inBuffer[VECTORSIZE+i] - prediction[i+VECTORSIZE];
		e[i+VECTORSIZE]				= (float)inVector[i+order];
	}

	// Find range value
	clamp(FRAMESIZE, e, ie, 16);
	int	max = 0, scale = 0;
	for (int i = 0; i < FRAMESIZE; i++)
		if (abs(ie[i])>abs(max))
			max = ie[i];

	while (scale <= MAXSCALE && (max > ulevel || max > llevel)) {
		scale++;
		max /= 2;
	}

	// Final prediction error with a quantizer in the loop
	for (int i = 0; i < FRAMESIZE; i++)
		saveState[i] = state[i];

	scale--;
	for (int nIter = 0; nIter < MAXITER; nIter++) {
		int	maxClip = 0;
		scale++;
		if (scale > MAXSCALE)
			scale = MAXSCALE;

		for (int i = 0; i < order; i++)
			inVector[i] = saveState[FRAMESIZE-order+i];

		for (int i = 0; i < VECTORSIZE; i++){
			prediction[i]		= inner_product(order+i, coefTable[optimalp][i], inVector);
			float	se			= (float)inBuffer[i] - prediction[i];
			ix[i]				= qsample(se, 1 << scale);
			int		cV			= (short)clamp((int)ix[i], llevel, ulevel) - ix[i];
			ix[i]				+= cV;
			inVector[i+order]	= ix[i] << scale;
			state[i]			= prediction[i] + inVector[i+order];		// Decoder output
			if (abs(cV) > maxClip)
				maxClip = abs(cV);
		}

		for (int i = 0; i < order; i++)
			inVector[i] = state[VECTORSIZE-order+i];

		for (int i = 0; i < VECTORSIZE; i++){
			prediction[i+VECTORSIZE] = inner_product(order+i, coefTable[optimalp][i], inVector);
			float	se			= (float)inBuffer[VECTORSIZE+i] - prediction[VECTORSIZE+i];
			ix[VECTORSIZE + i]	= qsample(se, 1 << scale);
			int		cV			= (short)clamp((int) ix[i+VECTORSIZE], llevel, ulevel) - ix[i+VECTORSIZE];
			ix[i+VECTORSIZE]	+= cV;
			inVector[i+order]	= ix[VECTORSIZE+i] << scale;
			state[VECTORSIZE+i]	= prediction[i+VECTORSIZE] + inVector[i+order];
			if (abs(cV) > maxClip)
				maxClip = abs(cV);
		}
		if (maxClip <= MAXCLIP)
			break;
	}

	outBuffer[0] = (scale << 4) | (optimalp & 0xf);
	for (int i = 0; i < FRAMESIZE; i += 2)
		outBuffer[i / 2 + 1] = (ix[i] << 4) | (ix[i + 1] & 0xf);

	return outBuffer;
}

//-----------------------------------------------------------------------------
//	DECODE DATA
//-----------------------------------------------------------------------------

void ADPCMcodebook::DecodeFrame(uint8 *adpcm, int *outp) {
	int		optimalp	= adpcm[0] & 0xf;
	int		shift		= adpcm[0] >> 4;
	int		in[FRAMESIZE], ix[FRAMESIZE];

	for (int i = 0; i < FRAMESIZE; i += 2){
		uint8 c = adpcm[i / 2 + 1];
		ix[i + 0] = sign_extend(c >> 4,  SAMPLEBITS - 1) << shift;
		ix[i + 1] = sign_extend(c & 0xf, SAMPLEBITS - 1) << shift;
	}

	for (int j = 0; j < FRAMESIZE; j += VECTORSIZE){
		for (int i = 0; i < VECTORSIZE; i++)
			in[i + order] = ix[j + i];
		if (j == 0) {
			for (int i = 0; i < order; i++)
				in[i] = outp[FRAMESIZE - order + i];
		} else {
			for (int i = 0; i < order; i++)
				in[i] = outp[j - order + i];
		}
		for (int i = 0; i < VECTORSIZE; i++)
			outp[i+j] = inner_product(VECTORSIZE + order, coefTable[optimalp][i], in);
	}
}

//-----------------------------------------------------------------------------
//
//	AIFC FORMAT
//
//-----------------------------------------------------------------------------

#define CODE_NAME               "VADPCMCODES"
#define LOOP_NAME               "VADPCMLOOPS"
#define VERSION                 1

class ADPCMFileHandler : public SampleFileHandler {
	const char*			GetExt() override { return "aif";	}
	const char*			GetDescription() override { return "ADPCM compressed sound file";	}
	ISO_ptr<void>		Read(tag id, istream_ref file) override;
	bool				Write(ISO_ptr<void> p, ostream_ref file) override;
} adpcm;

ISO_ptr<void> ADPCMFileHandler::Read(tag id, istream_ref file) {
	IFF_chunk	iff(file);
	if (iff.id != "FORM"_u32 || file.get<uint32>() != "AIFC"_u32)
		return ISO_NULL;

	ISO_ptr<sample> s(id);

	streamptr		ssnd_pos	= 0;
	ADPCMcodebook	*codebook	= NULL;
	int				outp[FRAMESIZE] = {0};

	while (iff.remaining()) {
		IFF_chunk	chunk(file);
		switch (chunk.id) {
			case "COMM"_u32: {
				aiff_structs::COMMchunk	comm = file.get();
				s->Create(comm.frames, comm.channels, comm.bits);
				s->SetFrequency(float(comm.samplerate));
//				if (file.get<uint32>() != "VAPC"_u32)
//					return ISO_NULL;
//				if (ReadPascal(file) != "VADPCM ~4-1") return false;
				break;
			}

			case "APPL"_u32:
				if (file.get<uint32>() == "stoc"_u32) {
					pascal_string	name;
					read(file, name);
					if (name == CODE_NAME) {
						if (file.get<uint16be>() == VERSION)
							codebook = new ADPCMcodebook(file);
					} else if (name == LOOP_NAME) {
						if (file.get<uint16be>() == VERSION) {
							uint16be	n		= file.get();
							uint32be	start	= file.get(), end = file.get();
//							sm.Mark(0).Set(0, start, end - start);
						}
					}
				}
				break;
/*
			case MakeID('I','N','S','T'): {
				INSTchunk		inst;
				chunk.Read(&inst, sizeof(INSTchunk));
				if ( inst.sustainLoop.playMode != 0 )
					sm.flags |= SMF_LOOP;
				break;
			}
*/
			case "SSND"_u32: {
				aiff_structs::SSNDchunk	ssnd = file.get();
				ssnd_pos		= file.tell();
				break;
			}

			case "MARK"_u32: {
				uint16be	n0 = file.get();
				for (int i = 0, n = min(n0, 2); i < n; i++) {
					uint16be	id	= file.get();
					uint32be	pos = file.get();
//					sm.Mark(id).Set(id, pos);
					file.seek_cur(file.getc() | 1);	// skip name
				}
				break;
			}
		}
	}
	if (codebook) {
		file.seek(ssnd_pos);
		for (uint32 currentPos = 0, length = s->Length(); currentPos < length; currentPos += FRAMESIZE) {
			uint8	adpcm[FRAMEBYTES];
			file.read(adpcm);
			codebook->DecodeFrame(adpcm, outp);
			short	*p = (short*)s->Samples() + currentPos;
			for (int i = 0; i < FRAMESIZE; i++)
				p[i] = outp[i];
		}

		delete codebook;
	}
	return s;
}

bool ADPCMFileHandler::Write(ISO_ptr<void> p, ostream_ref file) {
	if (ISO_ptr<sample> s = ISO_conversion::convert<sample>(p)) {
		ADPCMcodebook	codebook(*s);
		int				loopstart = s->Bits() & sample::LOOP ? 0 : -1;//s->Mark(0).frame : -1;
		int16			loopstate[FRAMESIZE];

		if (codebook.NumEntries() == 0)
			return false;

		int16	inBuffer[FRAMESIZE];
		uint8	outBuffer[FRAMEBYTES];

		IFF_Wchunk	iff(file, "FORM"_u32);
		iff.write("AIFC"_u32);

		{
			IFF_Wchunk	chunk(file, "COMM"_u32);
			aiff_structs::COMMchunk	comm;
			comm.channels		= s->Channels();
			comm.frames			= align(s->Length(), FRAMESIZE);
			comm.bits			= s->Bits();
			comm.samplerate		= s->Frequency();
			chunk.write(comm);
			chunk.write("VAPC");
			write(chunk, pascal_string("VADPCM ~4-1"));
		}

		{
			IFF_Wchunk	chunk(file, "APPL"_u32);
			chunk.write("stoc"_u32);
			write(chunk, pascal_string(CODE_NAME));
			chunk.write(uint16be(VERSION));
			codebook.Write(chunk);
		}

		{
			IFF_Wchunk	chunk(file, "SSND"_u32);
			aiff_structs::SSNDchunk	ssnd;
			ssnd.blocksize	= 0;
			ssnd.offset		= 0;
			chunk.write(ssnd);
			for (uint32 currentPos = 0, length = s->Length(); currentPos < length; currentPos += FRAMESIZE) {
				int	n  = min(FRAMESIZE, length - currentPos);
				memcpy(inBuffer, (short*)s->Samples() + currentPos, n * sizeof(short));
				if (currentPos >= loopstart) {
					codebook.GetState(loopstate);
					loopstart = -1;
				}
				codebook.EncodeFrame(outBuffer, inBuffer, n);
				file.write(outBuffer);
			}
		}
#if 0
		if (s->Looping()) {
			Chunk	chunk(aifc,"APPL");
			chunk.Put("stoc");
			chunk.PutPascal(LOOP_NAME);
			chunk.PutW(VERSION);
			chunk.PutW(1);		// # loops
			chunk.PutBEL(s->Mark(0).frame);
			chunk.PutBEL(s->Mark(1).frame);
			chunk.PutBEL(-1);
			chunk.WriteBEW(loopstate,FRAMESIZE);
		}
#endif
		return true;
	}
	return false;
}

#if 0
//-----------------------------------------------------------------------------
//
//	G72
//
//-----------------------------------------------------------------------------
//G.722[1] is a ITU-T standard 7 kHz wideband speech codec operating at 48, 56 and 64 kbit/s.

static short power2[15] = {1, 2, 4, 8, 0x10, 0x20, 0x40, 0x80, 0x100, 0x200, 0x400, 0x800, 0x1000, 0x2000, 0x4000};

//quantizes the input val against the table of size short integers.
// It returns i if table[i - 1] <= val < table[i].
static int quan(int val, short *table, int size) {
	int		i;
	for (i = 0; i < size; i++)
		if (val < *table++)
			break;
	return i;
}

// Given a raw sm, 'd', of the difference signal and a quantization step size scale factor, 'y', this routine returns the
// ADPCM codeword to which that sm gets quantized.  The step size scale factor division operation is done in the log base 2 domain
// as a subtraction.
int quantize(
 int d,	/* Raw difference signal sm */
 int y,	/* Step size multiplier */
 short *table,	/* quantization table */
 int size)	/* table size of short integers */
{
	// Compute base 2 log of 'd', and store in 'dl'.
	short	dqm	= abs(d);
	short	exp	= quan(dqm >> 1, power2, 15);
	short	mant = ((dqm << 7) >> exp) & 0x7F;	/* Fractional portion. */
	short	dl = (exp << 7) + mant;

	// "Divide" by step size multiplier.
	short	dln = dl - (y >> 2);

	// Obtain codword i for 'd'.
	int	i = quan(dln, table, size);
	return d < 0 ? ((size << 1) + 1 - i) : i == 0 ? ((size << 1) + 1) : i;
}
// Returns reconstructed difference signal 'dq' obtained from codeword 'i' and quantization step size scale factor 'y'.
// Multiplication is performed in log base 2 domain as addition.
int reconstruct(
int sign,	/* 0 for non-negative value */
int	dqln,	/* G.72x codeword */
int	y)	/* Step size multiplier */
{
	short	dql = dqln + (y >> 2);
	if (dql < 0) {
		return 0;
	} else {
		short	dex = (dql >> 7) & 15;
		short	dqt = 128 + (dql & 127);
		short	dq = (dqt << 7) >> (14 - dex);
		return sign ? -dq : dq;
	}
}

int block4l(short dl)
{
	static int sl = 0 ;
	int wd1, wd2, wd3, wd4;
	static int spl = 0;
	static int szl = 0;
	static int rlt[3] = { 0, 0, 0 };
	static int al [3] = { 0, 0, 0 };
	static int plt[3] = { 0, 0, 0 };
	static int dlt[7] = { 0, 0, 0, 0, 0, 0, 0 };
	static int bl [7] = { 0, 0, 0, 0, 0, 0, 0 };
	static int sg [7] = { 0, 0, 0, 0, 0, 0, 0 };

	/*************************************** BLOCK 4L, RECONS ***********/

	dlt[0] = dl;
	rlt[0] = sl + dl;
	plt[0] = dl + szl;

	/*****************************BLOCK 4L, UPPOL2*************************/
	sg[0] = plt[0] >> 15;
	sg[1] = plt[1] >> 15;
	sg[2] = plt[2] >> 15;

	wd1 = clamp(al[1] << 2, -32768, 32767);
	wd2 = min(sg[0] == sg[1] ?  -wd1 : wd1, 32767);
	wd3 = (wd2 >> 7) + (sg[0] == sg[2] ? 128: -128);
	wd4 = (al[2] * 32512) >> 15 ;

	al[2] = clamp(wd3 + wd4, -12288,  12288);

	/************************************* BLOCK 4L, UPPOL1 ***************/

	sg[0] = plt[0] >> 15;
	sg[1] = plt[1] >> 15;

	wd1 = sg[0] == sg[1] ? 192 : -192;
	wd2 = (al[1] * 32640) >> 15;
	wd3 = (15360 - al[2]);
	al[1] = clamp(wd1 + wd2, -wd3, wd3);

	/*************************************** BLOCK 4L, UPZERO ************/
	wd1		= dl == 0 ? 0 : 128;
	sg[0]	= dl >> 15;

	sg[1] = dlt[1] >> 15 ;
	bl[1] = ((bl[1] * 32640) >> 15) + (sg[1] == sg[0] ? wd1 : -wd1);

	sg[2] = dlt[2] >> 15 ;
	bl[2] = ((bl[2] * 32640) >> 15) + (sg[2] == sg[0] ? wd1 : -wd1);

	sg[3] = dlt[3] >> 15 ;
	bl[3] = ((bl[3] * 32640) >> 15) + (sg[3] == sg[0] ? wd1 : -wd1);

	sg[4] = dlt[4] >> 15 ;
	bl[4] = ((bl[4] * 32640) >> 15) + (sg[4] == sg[0] ? wd1 : -wd1);	// for g721_40, use 32704 instead of 32640

	sg[5] = dlt[5] >> 15 ;
	bl[5] = ((bl[5] * 32640) >> 15) + (sg[5] == sg[0] ? wd1 : -wd1);

	sg[6] = dlt[6] >> 15 ;
	bl[6] = ((bl[6] * 32640) >> 15) + (sg[6] == sg[0] ? wd1 : -wd1);

	/********************************* BLOCK 4L, DELAYA ******************/
	dlt[5] = dlt[4];
	dlt[4] = dlt[3];
	dlt[3] = dlt[2];
	dlt[2] = dlt[1];
	dlt[1] = dlt[0];

	rlt[2] = rlt[1];
	plt[2] = plt[1];
	rlt[1] = rlt[0];
	plt[1] = plt[0];

	/********************************* BLOCK 4L, FILTEP ******************/

	spl = ((al[1] * rlt[1]) >> 14) + ((al[2] * rlt[2]) >> 14);
	spl += (bl[1] * dlt[1]) >> 14;
	spl += (bl[2] * dlt[2]) >> 14;
	spl += (bl[3] * dlt[3]) >> 14;
	spl += (bl[4] * dlt[4]) >> 14;
	spl += (bl[5] * dlt[5]) >> 14;
	spl += (bl[6] * dlt[6]) >> 14;

	return spl;
}

// At the end of ADPCM decoding, it simulates an encoder which may be receiving the output of this decoder as a tandem process. If the output of the
// simulated encoder differs from the input to this decoder, the decoder output is adjusted by one level of A-law or u-law codes.
// Input:
//	sr	decoder output linear PCM sm,
//	se	predictor estimate sm
//	y	quantizer step size,
//	i	decoder input code,
//	sign	sign bit of code i
// Return:
//	adjusted A-law or u-law compressed sm.
int tandem_adjust_alaw(
int sr,	/* decoder output linear PCM sm */
int se,	/* predictor estimate sm */
int y,	/* quantizer step size */
int i,	/* decoder input code */
int sign,
short *qtab)
{
	if (sr <= -32768)
		sr = -1;
	uint8	sp = linear2alaw((sr >> 1) << 3);	/* short to A-law compression */
	short	dx = (alaw2linear(sp) >> 2) - se;	/* 16-bit prediction error */
	char	id = quantize(dx, y, qtab, sign - 1);

	if (id == i)			/* no adjustment on sp */
		return sp;

	// ADPCM codes : 8, 9, ... F, 0, 1, ... , 6, 7
	int	im	= i ^ sign;		// 2's complement to biased unsigned
	int	imx = id ^ sign;

	if (imx > im)	// sp adjusted to next lower value
		return sp & 0x80
			? (sp == 0xD5 ? 0x55 : ((sp ^ 0x55) - 1) ^ 0x55)
			: (sp == 0x2A ? 0x2A : ((sp ^ 0x55) + 1) ^ 0x55);
	else			// sp adjusted to next higher value
		return sp & 0x80
			? (sp == 0xAA ? 0xAA : ((sp ^ 0x55) + 1) ^ 0x55)
			: (sp == 0x55 ? 0xD5 : ((sp ^ 0x55) - 1) ^ 0x55);
}

int tandem_adjust_ulaw(
int sr,	/* decoder output linear PCM sm */
int se,	/* predictor estimate sm */
int y,	/* quantizer step size */
int i,	/* decoder input code */
int sign,
short *qtab)
{
	if (sr <= -32768)
		sr = 0;
	uint8	sp = linear2ulaw(sr << 2);			// short to u-law compression
	short	dx = (ulaw2linear(sp) >> 2) - se;	// 16-bit prediction error
	char	id = quantize(dx, y, qtab, sign - 1);
	if (id == i)
		return sp;

	// ADPCM codes : 8, 9, ... F, 0, 1, ... , 6, 7
	int	im	= i ^ sign;		/* 2's complement to biased unsigned */
	int	imx	= id ^ sign;
	if (imx > im)	// sp adjusted to next lower value
		return sp & 0x80
			? (sp == 0xFF	? 0x7E	: sp + 1)
			: (sp == 0		? 0		: sp - 1);
	else			// sp adjusted to next higher value
		return sp & 0x80
			? (sp == 0x80 ? 0x80 : sp - 1)
			: (sp == 0x7F ? 0xFE : sp + 1);
}
#endif
