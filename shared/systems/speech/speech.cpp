#include "base/defs.h"
#include "base/vector.h"
#include "base/list.h"

using namespace iso;

#define DEFAULT_SAMPLING_RATE	16000		// Default number of samples per second.
#define DEFAULT_FRAME_RATE		100			// Default number of frames per second.
#define DEFAULT_FRAME_SHIFT		160			// Default spacing between frame starts (equal to DEFAULT_SAMPLING_RATE/DEFAULT_FRAME_RATE)
#define DEFAULT_WINDOW_LENGTH	0.025625f	// Default size of each frame (410 samples @ 16000Hz).
#define DEFAULT_FFT_SIZE		512			// Default number of FFT points.
#define DEFAULT_NUM_CEPSTRA		13			// Default number of MFCC coefficients in output.
#define DEFAULT_NUM_FILTERS		40			// Default number of filter bands used to generate MFCCs.
#define DEFAULT_LOWER_FILT_FREQ	133.33334f	// Default lower edge of mel filter bank.
#define DEFAULT_UPPER_FILT_FREQ 6855.4976	// Default upper edge of mel filter bank.
#define DEFAULT_PRE_EMPHASIS_ALPHA	0.97	// Default pre-emphasis filter coefficient.
#define DEFAULT_WARP_TYPE "inverse_linear"	// Default type of frequency warping to use for VTLN.
#define SEED	-1							// Default random number seed to use for dithering.

typedef double frame_t;
typedef double powspec_t;
typedef double window_t;
struct complex { double r, i; };

union anytype {
	void		*ptr;
	long		i;
	uint32		ui;
	double		fl;
};

//-----------------------------------------------------------------------------
//	multi-dimensional callocs
//-----------------------------------------------------------------------------

void** calloc_2d(size_t d1, size_t d2, size_t elemsize) {
	void	*mem	= (void*)calloc(d1 * d2, elemsize);
	void	**ref	= new void*[d1];

	for (size_t i = 0, offset = 0; i < d1; i++, offset += d2 * elemsize)
		ref[i] = (char*)mem + offset;

	return ref;
}

void free_2d(void *tmpptr) {
	if (void **ptr = (void**)tmpptr) {
		iso::free(ptr[0]);
		iso::free(ptr);
	}
}

void*** calloc_3d(size_t d1, size_t d2, size_t d3, size_t elemsize) {
	void*	mem		= calloc(d1 * d2 * d3, elemsize);
	void**	ref1	= (void**)iso::malloc(d1 * d2 * sizeof(void*));
	void***	ref2	= (void***)iso::malloc(d1 * sizeof(void**));

	for (size_t i = 0, offset = 0; i < d1; i++, offset += d2)
		ref2[i] = ref1 + offset;

	for (size_t i = 0, offset = 0; i < d1; i++) {
		for (size_t j = 0; j < d2; j++, offset += d3 * elemsize)
			ref2[i][j] = (char*)mem + offset;
	}

	return ref2;
}

void free_3d(void *inptr) {
	if (void ***ptr = (void***)inptr) {
		if (ptr[0]) {
			iso::free(ptr[0][0]);
			iso::free(ptr[0]);
		}
		iso::free(ptr);
	}
}

void ****calloc_4d(size_t d1, size_t d2, size_t d3, size_t d4, size_t elem_size) {
	void*	mem		= calloc(d1 * d2 * d3 * d4, elem_size);
	void**	ref1	= (void**)iso::malloc(d1 * d2 * d3 * sizeof(void*));
	void***	ref2	= (void***)iso::malloc(d1 * d2 * sizeof(void**));
	void****ref3	= (void****)iso::malloc(d1 * sizeof(void***));

	for (size_t i = 0, j = 0; i < d1 * d2 * d3; i++, j += d4)
		ref1[i] = (char*)mem + j * elem_size;

	for (size_t i = 0, j = 0; i < d1 * d2; i++, j += d3)
		ref2[i] = ref1 + j;

	for (size_t i = 0, j = 0; i < d1; i++, j += d2)
		ref3[i] = ref2 + j;

	return ref3;
}

void free_4d(void *inptr) {
	if (void ****ptr = (void****)inptr) {
		if (ptr[0]) {
			if (ptr[0][0]) {
				iso::free(ptr[0][0][0]);
				iso::free(ptr[0][0]);
			}
			iso::free(ptr[0]);
		}
		iso::free(ptr);
	}
}

template<typename T> T* calloc_1d(size_t d1) {
	return (T*)calloc(d1, sizeof(T));
}
template<typename T> T** calloc_2d(size_t d1, size_t d2) {
	return (T**)calloc_2d(d1, d2, sizeof(T));
}
template<typename T> T*** calloc_2d(size_t d1, size_t d2, size_t d3) {
	return (T***)calloc_3d(d1, d2, d3, sizeof(T));
}
template<typename T> T**** calloc_2d(size_t d1, size_t d2, size_t d3, size_t d4) {
	return (T****)calloc_4d(d1, d2, d3, d4, sizeof(T));
}

// Layers a 3d array access structure over a preallocated storage area
void *alloc_3d_ptr(size_t d1, size_t d2,	size_t d3, void *mem, size_t elem_size) {
	void **tmp1 = (void**)iso::malloc(d1 * d2 * sizeof(void*));
	void ***out = (void***)iso::malloc(d1 * sizeof(void**));

	for (size_t i = 0, j = 0; i < d1*d2; i++, j += d3)
		tmp1[i] = (char*)mem + j * elem_size;

	for (size_t i = 0, j = 0; i < d1; i++, j += d2)
		out[i] = tmp1 + j;

	return out;
}

void *alloc_2d_ptr(size_t d1, size_t d2, void *mem, size_t elem_size) {
	void **out = (void**)iso::malloc(d1 * sizeof(void*));

	for (size_t i = 0, j = 0; i < d1; i++, j += d2)
		out[i] = (char*)mem + j * elem_size;

	return out;
}

//-----------------------------------------------------------------------------
//	fixed point + logs
//-----------------------------------------------------------------------------

typedef fixed<20,12>	fixed32;

#define MIN_FIXLOG	-2829416	// log(1e-300)
#define MIN_FIXLOG2	-4081985	// log2(1e-300)

#define FIXLN_2		fixed32(0.693147180559945f)
#define FIXLN(x)	(fixlog(x) - FIXLN_2 * 12)

// Table of log2(x/128)
static fixed32 logtable[] = {
	0.00561362771162706f,	0.0167975342258543f,	0.0278954072829524f,	0.0389085604479519f,
	0.0498382774298215f,	0.060685812979481f,		0.0714523937543017f,	0.0821392191505851f,
	0.0927474621054331f,	0.103278269869348f,		0.113732764750838f,		0.124112044834237f,
	0.13441718467188f,		0.144649235951738f,		0.154809228141536f,		0.164898169110351f,
	0.174917045728623f,		0.184866824447476f,		0.194748451858191f,		0.204562855232657f,
	0.214310943045556f,		0.223993605479021f,		0.23361171491048f,		0.243166126384332f,
	0.252657678068119f,		0.262087191693777f,		0.271455472984569f,		0.280763312068243f,
	0.290011483876938f,		0.299200748534365f,		0.308331851730729f,		0.317405525085859f,
	0.32642248650099f,		0.335383440499621f,		0.344289078557851f,		0.353140079424581f,
	0.36193710943195f,		0.37068082279637f,		0.379371861910488f,		0.388010857626406f,
	0.396598429530472f,		0.405135186209943f,		0.4136217255118f,		0.422058634793998f,
	0.430446491169411f,		0.438785861742727f,		0.447077303840529f,		0.455321365234813f,
	0.463518584360147f,		0.471669490524698f,		0.479774604115327f,		0.487834436796966f,
	0.49584949170644f,		0.503820263640951f,		0.511747239241369f,		0.519630897170528f,
	0.527471708286662f,		0.535270135812172f,		0.543026635497834f,		0.550741655782637f,
	0.558415637949355f,		0.56604901627601f,		0.573642218183348f,		0.581195664378452f,
	0.588709768994618f,		0.596184939727604f,		0.603621577968369f,		0.61102007893241f,
	0.618380831785792f,		0.625704219767993f,		0.632990620311629f,		0.640240405159187f,
	0.647453940476827f,		0.654631586965362f,		0.661773699968486f,		0.668880629578336f,
	0.675952720738471f,		0.682990313344332f,		0.689993742341272f,		0.696963337820209f,
	0.703899425110987f,		0.710802324873503f,		0.717672353186654f,		0.724509821635192f,
	0.731315037394519f,		0.738088303313493f,		0.744829917995304f,		0.751540175876464f,
	0.758219367303974f,		0.764867778610703f,		0.77148569218905f,		0.778073386562917f,
	0.784631136458046f,		0.791159212870769f,		0.797657883135205f,		0.804127410988954f,
	0.810568056637321f,		0.816980076816112f,		0.823363724853051f,		0.829719250727828f,
	0.836046901130843f,		0.84234691952066f,		0.848619546180216f,		0.854865018271815f,
	0.861083569890926f,		0.867275432118842f,		0.873440833074202f,		0.879579997963421f,
	0.88569314913005f,		0.891780506103101f,		0.897842285644346f,		0.903878701794633f,
	0.90988996591924f,		0.915876286752278f,		0.921837870440188f,		0.927774920584334f,
	0.933687638282728f,		0.939576222170905f,		0.945440868461959f,		0.951281770985776f,
	0.957099121227478f,		0.962893108365084f,		0.968663919306429f,		0.974411738725344f,
	0.980136749097113f,		0.985839130733238f,		0.991519061815512f,		0.997176718429429f,
//	1.00281227459694f,
};

fixed32 fixlog2(fixed32 x) {
	if (x == 0)
	return -996.5784285f;//log2(1e-300)
//	-690.7755279f;	//log(1e-300)

	uint32 y = highest_set_index(x.i);
	return y + logtable[(x.i << (31 - y) >> 24) & 0x7f];
}

fixed32 fixlog(fixed32 x) {
	return fixlog2(x) * FIXLN_2;
}

//-----------------------------------------------------------------------------
//	Random numbers
//-----------------------------------------------------------------------------

struct genrand {
	enum {
		N			= 624,
		M			= 397,
		MATRIX_A	= 0x9908b0dfUL, // constant vector a
		UPPER_MASK	= 0x80000000UL, // most significant w-r bits
		LOWER_MASK	= 0x7fffffffUL, // least significant r bits
	};
	uint32		mt[N];
	int			i;
	genrand() : i(N + 1) {}

	void	init(uint32 s);
	uint32	next();

	operator uint32()	{ return next(); }
	operator int()		{ return int(next() >> 1); }
	operator float()	{ uint32 i = (next() & 0x7fffff) | 0x3f800000; return (float&)i - 1.0f; }
	operator double()	{ return next() / 4294967296.0; }
};

// initializes mt[N] with a seed
void genrand::init(uint32 s) {
	mt[0] = s & 0xffffffffUL;
	for (i = 1; i < N; i++) {
		mt[i] = (1812433253UL * (mt[i - 1] ^ (mt[i - 1] >> 30)) + i);
		mt[i] &= 0xffffffffUL;
	}
}

// generates a random number on [0,0xffffffff]-interval
uint32 genrand::next() {
	static uint32 mag01[2] = { 0u, MATRIX_A };
	// mag01[x] = x * MATRIX_A	for x=0,1

	if (i >= N) {	// generate N words at one time
		if (i == N + 1)		// if init_genrand() has not been called,
			init(5489UL);	// a default initial seed is used

		for (int k = 0; k < N - M; k++) {
			uint32	y = (mt[k] & UPPER_MASK) | (mt[k + 1] & LOWER_MASK);
			mt[k] = mt[k + M] ^ (y >> 1) ^ mag01[y & 0x1UL];
		}
		for (int k = N - M; k < N - 1; k++) {
			uint32	y = (mt[k] & UPPER_MASK) | (mt[k + 1] & LOWER_MASK);
			mt[k] = mt[k + (M - N)] ^ (y >> 1) ^ mag01[y & 0x1UL];
		}
		uint32	y = (mt[N - 1] & UPPER_MASK) | (mt[0] & LOWER_MASK);
		mt[N - 1] = mt[M - 1] ^ (y >> 1) ^ mag01[y & 0x1UL];

		i = 0;
	}

	uint32 y = mt[i++];

	// Tempering
	y ^= (y >> 11);
	y ^= (y << 7) & 0x9d2c5680UL;
	y ^= (y << 15) & 0xefc60000UL;
	y ^= (y >> 18);

	return y;
}


//-----------------------------------------------------------------------------
//	Various forms of automatic gain control (AGC)
//-----------------------------------------------------------------------------

struct agc {
	enum type {
		NONE = 0,
		MAX,
		EMAX,
		NOISE
	};

	float	max;			// Estimated max for current utterance (for AGC_EMAX)
	float	obs_max;		// Observed max in current utterance
	int		obs_frame;		// Whether any data was observed after prev update
	int		obs_utt;		// Whether any utterances have been observed
	float	obs_max_sum;
	float	noise_thresh;	// Noise threshold (for AGC_NOISE only)

	agc() {
		clear(*this);
		noise_thresh = 2.0f;
	}

	float	emax_get()		const				{ return max; }
	void	emax_set(float m)					{ max = m; }
	float	get_threshold()	const				{ return noise_thresh; }
	void	set_threshold(float threshold)		{ noise_thresh = threshold; }

	void	calc_max(float **mfc, int n_frame);
	void	emax(float **mfc, int n_frame);
	void	emax_update();
	void	noise(float **mfc, int n_frame);
};

// Normalize c0 for all frames such that max(c0) = 0.
void agc::calc_max(float **mfc, int n_frame) {
	if (n_frame <= 0)
		return;
	obs_max = mfc[0][0];
	for (int i = 1; i < n_frame; i++) {
		if (mfc[i][0] > obs_max) {
			obs_max		= mfc[i][0];
			obs_frame	= 1;
		}
	}

	for (int i = 0; i < n_frame; i++)
		mfc[i][0] -= obs_max;
}


void agc::emax(float **mfc, int n_frame) {
	if (n_frame <= 0)
		return;
	for (int i = 0; i < n_frame; ++i) {
		if (mfc[i][0] > obs_max) {
			obs_max = mfc[i][0];
			obs_frame = 1;
		}
		mfc[i][0] -= max;
	}
}

// Update estimated max for next utterance
void agc::emax_update() {
	if (obs_frame) {	// Update only if some data observed
		obs_max_sum += obs_max;
		obs_utt++;

		// Re-estimate max over past history; decay the history
		max = obs_max_sum / obs_utt;
		if (obs_utt == 8) {
			obs_max_sum /= 2;
			obs_utt = 4;
		}
	}

	// Reset the accumulators for the next utterance.
	obs_frame	= 0;
	obs_max		= -1000.0; // Less than any real C0 value (hopefully!!)
}

void agc::noise(float **cep, int nfr) {
	// Determine minimum log-energy in utterance
	float min_energy = cep[0][0];
	for (int i = 0; i < nfr; ++i) {
		if (cep[i][0] < min_energy)
			min_energy = cep[i][0];
	}

	// Average all frames between min_energy and min_energy + noise_thresh
	min_energy += noise_thresh;

	float	noise_level	= 0;	// Average noise_level
	int		noise_frames = 0;	// Number of noise frames
	for (int i = 0; i < nfr; ++i) {
		if (cep[i][0] < min_energy) {
			noise_level += cep[i][0];
			noise_frames++;
		}
	}
	noise_level /= noise_frames;

	// Subtract noise_level from all log_energy values
	for (int i = 0; i < nfr; ++i)
		cep[i][0] -= noise_level;
}

//-----------------------------------------------------------------------------
//	Various forms of cepstral mean normalization (CMN)
//-----------------------------------------------------------------------------

struct cmn {
	enum type {
		NONE,
		CURRENT,
		PRIOR
	};
	float	*cmn_mean;	// Temporary variable: current means
	float	*cmn_var;	// Temporary variables: stored the cmn variance
	float	*sum;		// The sum of the cmn frames
	int		nframe;		// Number of frames
	int		veclen;		// Length of cepstral vector

	cmn(int _veclen) : veclen(_veclen) {
		cmn_mean	= calloc_1d<float>(veclen);
		cmn_var		= calloc_1d<float>(veclen);
		sum			= calloc_1d<float>(veclen);
		cmn_mean[0] = 12.0f;		// A front-end dependent magic number
		nframe = 0;
	}
	~cmn() {
		iso::free(cmn_var);
		iso::free(cmn_mean);
		iso::free(sum);
	}
	void calc(float **mfc, int varnorm, int n_frame);	// CMN for the whole sentence
	void prior(float **incep, int varnorm, int nfr);	// CMN for one block of data, using prior mean
	void prior_shiftwin();
	void prior_update();								// Update prior mean based on observed data
	void prior_set(float const *vec);					// Set the prior mean.
	void prior_get(float *vec);							// Get the prior mean.
};

#define CMN_WIN_HWM	800	// #frames after which window shifted
#define CMN_WIN	500

void cmn::calc(float ** mfc, int varnorm, int n_frame) {
	if (n_frame <= 0)
		return;

	// If cmn_mean wasn't NULL, we need to zero the contents
	memset(cmn_mean, 0, veclen * sizeof(float));

	// Find mean cep vector for this utterance
	for (int f = 0; f < n_frame; f++) {
		float	*p = mfc[f];
		for (int i = 0; i < veclen; i++)
			cmn_mean[i] += p[i];
	}

	for (int i = 0; i < veclen; i++)
		cmn_mean[i] /= n_frame;

	if (!varnorm) {
		// Subtract mean from each cep vector
		for (int f = 0; f < n_frame; f++) {
			float	*p = mfc[f];
			for (int i = 0; i < veclen; i++)
				p[i] -= cmn_mean[i];
		}
	}
	else {
		// Scale cep vectors to have unit variance along each dimension, and subtract means
		// If cmn_var wasn't NULL, we need to zero the contents
		memset(cmn_var, 0, veclen * sizeof(float));

		for (int f = 0; f < n_frame; f++) {
			float	*p = mfc[f];
			for (int	i = 0; i < veclen; i++)
				cmn_var[i] += square(p[i] - cmn_mean[i]);
		}
		// Inverse Std. Dev, RAH added type case from sqrt
		for (int i = 0; i < veclen; i++)
			cmn_var[i] = sqrt((double)n_frame / cmn_var[i]);

		for (int f = 0; f < n_frame; f++) {
			float	*p = mfc[f];
			for (int i = 0; i < veclen; i++)
				p[i] = (p[i] - cmn_mean[i]) * cmn_var[i];
		}
	}
}

void cmn::prior_set(const float *vec) {
	for (int i = 0; i < veclen; i++) {
		cmn_mean[i] = vec[i];
		sum[i] = vec[i] * CMN_WIN;
	}
	nframe = CMN_WIN;
}

void cmn::prior_get(float * vec) {
	for (int i = 0; i < veclen; i++)
		vec[i] = cmn_mean[i];
}

void cmn::prior_shiftwin() {
	float sf = 1.f / nframe;
	for (int i = 0; i < veclen; i++)
		cmn_mean[i] = sum[i] / nframe; // sum[i] * sf

	// Make the accumulation decay exponentially
	if (nframe >= CMN_WIN_HWM) {
		sf = CMN_WIN * sf;
		for (int i = 0; i < veclen; i++)
			sum[i] = sum[i] * sf;
		nframe = CMN_WIN;
	}
}

void cmn::prior_update() {
	if (nframe <= 0)
		return;

	// Update mean buffer
	float sf = 1.f / nframe;
	for (int i = 0; i < veclen; i++)
		cmn_mean[i] = sum[i] / nframe; // sum[i] * sf;

	// Make the accumulation decay exponentially
	if (nframe > CMN_WIN_HWM) {
		sf = CMN_WIN * sf;
		for (int i = 0; i < veclen; i++)
			sum[i] = sum[i] * sf;
		nframe = CMN_WIN;
	}
}

void cmn::prior(float **incep, int varnorm, int nfr) {
//	if (varnorm)
//		E_FATAL("Variance normalization not implemented in live mode decode\n");

	if (nfr <= 0)
		return;

	for (int i = 0; i < nfr; i++) {
		for (int j = 0; j < veclen; j++) {
			sum[j] += incep[i][j];
			incep[i][j] -= cmn_mean[j];
		}
		++nframe;
	}

	// Shift buffer down if we have more than CMN_WIN_HWM frames
	if (nframe > CMN_WIN_HWM)
		prior_shiftwin();
}

//-----------------------------------------------------------------------------
//	YIN
// "YIN, a fundamental frequency estimator for speech and music".
// Alain de Cheveigné and Hideki Kawahara.	Journal of the Acoustical Society of America, 111 (4), April 2002.
//-----------------------------------------------------------------------------

struct yin {
	typedef ufixed<1,15>	Q15;

	uint16	frame_size;			// Size of analysis frame.
	Q15		search_threshold;	// Threshold for finding period, in Q15
	Q15		search_range;		// Range around best local estimate to search, in Q15
	uint16	num_frames;				// Number of frames read so far.

	uint8		wsize;			// Size of smoothing window.
	uint8		wstart;			// First frame in window.
	uint8		wcur;			// Current frame of analysis
	bool		endut;			// Hoch Hech! Are we at the utterance end?

	fixed32**	diff_window;	// Window of difference function outputs
	uint16*		period_window;	// Window of best period estimates

	yin(int _frame_size, float _search_threshold, float _search_range, int _smooth_window)
		: frame_size(_frame_size), search_threshold(_search_threshold), search_range(_search_range)
		, num_frames(0), wstart(0), wcur(0), endut(false)
	{
		wsize			= _smooth_window * 2 + 1;
		diff_window		= calloc_2d<fixed32>(wsize, frame_size / 2);
		period_window	= calloc_1d<uint16>(wsize);
	}

	~yin() {
		free_2d(diff_window);
		iso::free(period_window);
	}

	void start() { // Reset the circular window pointers.
		wstart = 0;
		num_frames = 0;
		endut = false;
	}

	void end() {
		endut = true;
	}
	bool	read(uint16 *out_period, uint16 *out_bestdiff);
	void	write(const int16 *frame);

};

// The core of YIN: cumulative mean normalized difference function.
static void cmn_diff(int16 const *signal, fixed32 *out_diff, int ndiff) {
	out_diff[0] = 32768;

	uint32	cum		= 0;
	uint32	cshift	= 0;

	// Determine how many bits we can scale t up by below.
	int tscale;
	for (tscale = 0; tscale < 32; ++tscale)
		if (ndiff & (1<<(31 - tscale)))
			break;

	--tscale;

	// Somewhat elaborate block floating point implementation
	// The fp implementation of this is really a lot simpler
	for (uint32 t = 1; t < ndiff; ++t) {
		uint32 dd = 0, dshift = 0;
		for (int j = 0; j < ndiff; ++j) {
			int diff = signal[j] - signal[t + j];
			// Guard against overflows.
			if (dd > (1UL << tscale)) {
				dd >>= 1;
				++dshift;
			}
			dd += (diff * diff) >> dshift;
		}
		// Make sure the diffs and cum are shifted to the same scaling factor (usually dshift will be zero)
		if (dshift > cshift)
			cum += dd << (dshift-cshift);
		else
			cum += dd >> (cshift-dshift);

		// Guard against overflows and also ensure that (t<<tscale) > cum
		while (cum > (1UL << tscale)) {
			cum >>= 1;
			++cshift;
		}
		// Avoid divide-by-zero!
		if (cum == 0)
			cum = 1;
		// Calculate the normalizer in high precision
		uint32	norm = (t << tscale) / cum;
		// Do a long multiply and shift down to Q15
		out_diff[t].i = (int)((int64(dd) * norm) >> (tscale - 15 + cshift - dshift));
	}
}

int thresholded_search(fixed32 *diff_window, fixed32 threshold, int start, int end) {
	int	min		= INT_MAX;
	int	argmin	= 0;

	for (int i = start; i < end; ++i) {
		fixed32 diff = diff_window[i];
		if (diff < min) {
			min		= diff;
			argmin	= i;
			if (diff < threshold)
				break;
		}
	}
	return argmin;
}

void yin::write(const int16 *frame) {
	// Rotate the window one frame forward, but fill in the frame before wstart
	int	outptr = wstart++;
	// Wrap around the window pointer
	if (wstart == wsize)
		wstart = 0;

	// Now calculate normalized difference function
	int	difflen = frame_size / 2;
	cmn_diff(frame, diff_window[outptr], difflen);

	// Find the first point under threshold. If not found, then use the absolute minimum
	period_window[outptr] = thresholded_search(diff_window[outptr], search_threshold, 0, difflen);
	++num_frames;
}

bool yin::read(uint16 *out_period, uint16 *out_bestdiff) {

	int	half_wsize = (wsize - 1) / 2;
	// Without any smoothing, just return the current value (don't need to do anything to the current poitner either)
	if (half_wsize == 0) {
		if (endut)
			return false;
		*out_period		= period_window[0];
		*out_bestdiff	= diff_window[0][period_window[0]];
		return true;
	}

	// We can't do anything unless we have at least (wsize-1)/2 + 1 frames, unless we're at the end of the utterance
	if (endut == 0 && num_frames < half_wsize + 1)
		return false;

	// Establish the smoothing window
	int	start, len;
	if (endut) {						// End of utterance
		// We are done (no more data) when pe->wcur = pe->wstart
		if (wcur == wstart)
			return false;
		// I.e. pe->wcur (circular minus) half_wsize
		start	= (wcur + wsize - half_wsize) % wsize;
		len		= wstart - start;
		if (len < 0)
			len += wsize;
	} else if (num_frames < wsize) {	// Beginning of utterance
		start	= 0;
		len		= num_frames;
	} else {							// Normal case, it is what it is
		start	= wstart;
		len		= wsize;
	}

	// Now (finally) look for the best local estimate
	int	best		= period_window[wcur];
	int	best_diff	= diff_window[wcur][best];
	for (int i = 0; i < len; ++i) {
		int j		= (wstart + i) % wsize;
		int diff	= diff_window[j][period_window[j]];
		if (diff < best_diff) {
			best_diff = diff;
			best = period_window[j];
		}
	}
	// If it's the same as the current one then return it
	if (best == period_window[wcur]) {
		// Increment the current pointer
		if (++wcur == wsize)
			wcur = 0;
		*out_period		= best;
		*out_bestdiff	= best_diff;
		return true;
	}

	// Otherwise, redo the search inside a narrower range
	int	search_width	= max(best * search_range / 32768, 1);
	int	low_period		= max(best - search_width, 0);
	int	high_period		= min(best + search_width, frame_size / 2);

	best		= thresholded_search(diff_window[wcur], search_threshold, low_period, high_period);
	best_diff	= diff_window[wcur][best];

	if (out_period)
		*out_period = min(best, 65535);
	if (out_bestdiff)
		*out_bestdiff = min(best_diff, 65535);

	// Increment the current pointer
	if (++wcur == wsize)
		wcur = 0;
	return true;
}

//-----------------------------------------------------------------------------
//	Continuous A/D listening and silence filtering
//-----------------------------------------------------------------------------

#define AD_SAMPLE_SIZE			sizeof(int16)
#define DEFAULT_SAMPLES_PER_SEC	16000

// Return codes
#define AD_OK			0
#define AD_EOF			-1
#define AD_ERR_GEN		-1
#define AD_ERR_NOT_OPEN	-2
#define AD_ERR_WAVE		-3

struct	ad_wbuf {
	HGLOBAL		h_whdr;
	LPWAVEHDR	p_whdr;
	HGLOBAL		h_buf;
	LPSTR		p_buf;
};

struct ad_rec {
	HWAVEIN		h_wavein;	// "HANDLE" to the audio input device
	ad_wbuf*	wi_buf;		// Recording buffers provided to system
	int			n_buf;		// #Recording buffers provided to system
	int			opened;		// Flag; A/D opened for recording
	int			recording;
	int			curbuf;		// Current buffer with data for application
	int			curoff;		// Start of data for application in curbuf
	int			curlen;		// #samples of data from curoff in curbuf
	int			lastbuf;	// Last buffer containing data after recording stopped
	int			sps;		// Samples/sec
	int			bps;		// Bytes/sample
};

ad_rec *ad_open_dev (const char *dev, int samples_per_sec);
ad_rec *ad_open_sps (int samples_per_sec);
ad_rec *ad_open ( void );


int ad_start_rec(ad_rec*);
int ad_stop_rec(ad_rec*);
int ad_close(ad_rec*);
int ad_read(ad_rec*, int16 *buf, int max);

struct ad_play {
	HWAVEOUT h_waveout;	// "HANDLE" to the audio output device
	ad_wbuf *wo_buf;	// Playback buffers given to the system
	int		opened;		// Flag; A/D opened for playback
	int		playing;
	char	*busy;		// flags [N_WO_BUF] indicating whether given to system
	int		nxtbuf;		// Next buffer [0..N_WO_BUF-1] to be used for playback data
	int		sps;		// Samples/sec
	int		bps;		// Bytes/sample
};

ad_play	*ad_open_play_sps(int samples_per_sec);
ad_play	*ad_open_play();
int		ad_start_play(ad_play *);
int		ad_stop_play(ad_play *);
int		ad_close_play(ad_play *);
int		ad_write(ad_play*, int16 *buf, int len);


// Convert mu-law data to int16 linear PCM format.
void ad_mu2li (int16 *outbuf, uint8 *inbuf, int n_samp);

// Various parameters, including defaults for many cont_ad_t member variables
#define CONT_AD_ADFRMSIZE		256		// #Frames of internal A/D buffer maintained
#define CONT_AD_POWHISTSIZE		98		// #Powhist bins: ~ FRMPOW(65536^2*CONT_AD_SPF) Maximum level is 96.3 dB full-scale; 97 for safety, plus 1 for zero-based
#define CONT_AD_CALIB_FRAMES	(CONT_AD_POWHISTSIZE * 2)
#define CONT_AD_THRESH_UPDATE	100		// Update thresholds approx every so many frames PWP: update was 200 frames, or 3.2 seconds.	Now about every 1.6 sec.
#define CONT_AD_ADAPT_RATE		0.2f	// Interpolation of new and old noiselevel
#define CONT_AD_SPS				16000
#define CONT_AD_DEFAULT_NOISE	30		// Default background noise power level
#define CONT_AD_DELTA_SIL		10		// Initial default for delta_sil
#define CONT_AD_DELTA_SPEECH	17		// Initial default for delta_speech
#define CONT_AD_MIN_NOISE		2		// Expected minimum background noise level
#define CONT_AD_MAX_NOISE		70		// Maximum background noise level
#define CONT_AD_HIST_INERTIA	3		// Used in decaying the power histogram
#define CONT_AD_WINSIZE			21		// Analysis window for state transitions rkm had 16
#define CONT_AD_SPEECH_ONSET	9		// Min #speech frames in analysis window for SILENCE -> SPEECH state transition
#define CONT_AD_SIL_ONSET		18		// Min #silence frames in analysis window for SPEECH -> SILENCE state transition MUST BE <= CONT_AD_WINSIZE
#define CONT_AD_LEADER			5		// On transition to SPEECH state, so many frames BEFORE window included in speech data (>0)
#define CONT_AD_TRAILER			10		// On transition to SILENCE state, so many frames of silence included in speech data (>0)

// Continuous listening module or object
// An application can open and maintain several such objects, if necessary.
struct cont_ad {
	// Data structure for maintaining speech (non-silence) segments not yet consumed by the application.
	struct spseg {
		int		startfrm;	// Frame-id in adbuf (see below) of start of this segment
		int		nfrm;		// Number of frames in segment (may wrap around adbuf)
		spseg	*next;		// Next speech segment (with some intervening silence)
		spseg(int _startfrm, int _nfrm) : startfrm(_startfrm < 0 ? _startfrm + CONT_AD_ADFRMSIZE : _startfrm), nfrm(_nfrm), next(0) {}
	};

	typedef int func_t(ad_rec*, int16*, int);

	enum STATE {
		SILENCE		= 0,
		SPEECH		= 1,
	};

	func_t	*adfunc;		// Function to be called for obtaining A/D data (see prototype for ad_read in ad.h)
	ad_rec	*ad;			// A/D device argument for adfunc.	Also, ad->sps used to determine frame size (spf, see below)
	bool	rawmode;		// Pass all input data through, without filtering silence
	int16	*adbuf;			// Circular buffer for maintaining A/D data read until consumed

	STATE	state;			// State of data returned by most recent cont_ad_read call; SILENCE or SPEECH.
	int		read_ts;		// Absolute timestamp (total no. of raw samples consumed upto the most recent cont_ad_read call, starting from the very beginning).
	int		seglen;			// Total no. of raw samples consumed in the segment returned by the most recent cont_ad_read call
	int		siglvl;			// Max signal level for the data consumed by the most recent cont_ad_read call (dB range: 0-99)

	int		sps;			// Samples/sec; moved from ad->sps to break dependence on ad by N. Roy
	bool	eof;			// Whether the source ad device has encountered EOF
	int		spf;			// Samples/frame; audio level is analyzed within frames
	int		adbufsize;		// Buffer size (Number of samples)
	int		prev_sample;	// For pre-emphasis filter
	int		headfrm;		// Frame number in adbuf with unconsumed A/D data
	int		n_frm;			// Number of complete frames of unconsumed A/D data in adbuf
	int		n_sample;		// Number of samples of unconsumed data in adbuf
	int		tot_frm;		// Total number of frames of A/D data read, including consumed ones
	int		noise_level;	// PWP: what we claim as the "current" noise level

	int		*pow_hist;		// Histogram of frame power, moving window, decayed
	char	*frm_pow;		// Frame power

	int		auto_thresh;	// Do automatic threshold adjustment or not
	int		delta_sil;		// Max silence power/frame ABOVE noise level
	int		delta_speech;	// Min speech power/frame ABOVE noise level
	int		min_noise;		// noise lower than this we ignore
	int		max_noise;		// noise higher than this signals an error
	int		winsize;		// how many frames to look at for speech det
	int		speech_onset;	// start speech on >= these many frames out of winsize, of >= delta_speech
	int		sil_onset;		// end speech on >= these many frames out of winsize, of <= delta_sil
	int		leader;			// pad beggining of speech with this many extra frms
	int		trailer;		// pad end of speech with this many extra frms

	int		thresh_speech;	// Frame considered to be speech if power >= thresh_speech (for transitioning from SILENCE to SPEECH state)
	int		thresh_sil;		// Frame considered to be silence if power <= thresh_sil (for transitioning from SPEECH to SILENCE state)
	int		thresh_update;	// Number of frames before next update to pow_hist/thresholds
	float	adapt_rate;		// Linear interpolation constant for rate at which noise level adapted to each estimate;

	STATE	tail_state;		// State at the end of its internal buffer (internal use): SILENCE or SPEECH
	int		win_startfrm;	// Where next analysis window begins
	int		win_validfrm;	// Number of frames currently available from win_startfrm for analysis
	int		n_other;		// If in SILENCE state, number of frames in analysis window considered to be speech; otherwise number of frames considered to be silence
	spseg	*spseg_head;	// First of unconsumed speech segments
	spseg	*spseg_tail;	// Last of unconsumed speech segments

	int		n_calib_frame;	// Number of frames of calibration data seen so far.

	static int frame_pow(int16 * buf, int * prev, int spf);
	void	compute_frame_pow(int frm);
	void	decay_hist();
	int		find_thresh();
	void	sil2speech_transition(int frm);
	void	speech2sil_transition(int frm);
	void	boundary_detect(int frm);
	int		max_siglvl(int startfrm, int nfrm);
	int		buf_copy(int sf, int nf, int16 * buf);
	int		read_internal(int16 *buf, int max);
	int		classify(int len);

public:
	cont_ad(ad_rec *_ad, func_t *_adfunc, bool raw = false);
	~cont_ad();
	void	reset();
	int		buffer_space()			const			{ return adbufsize - n_sample; }
	int		calibrate_size()		const			{ return spf * CONT_AD_CALIB_FRAMES; }
	void	detach()								{ ad = NULL; adfunc	= NULL; }
	void	attach(ad_rec *_ad, func_t *_adfunc)	{ ad = _ad; adfunc = _adfunc; eof = false; }

	int		read(int16 *buf, int max);
	int		calibrate();
	int		calibrate_loop(int16 *buf, int max);

	int		set_thresh1(int silence, int speech);
	int		set_thresh2(int silence, int speech);
	int		set_params(int delta_sil, int delta_speech, int min_noise, int max_noise, int winsize, int speech_onset, int sil_onset, int leader, int trailer, float adapt_rate);
	int		get_params(int *delta_sil, int *delta_speech, int *min_noise, int *max_noise, int *winsize, int *speech_onset, int *sil_onset, int *leader, int *trailer, float *adapt_rate);
};

cont_ad::cont_ad(ad_rec *_ad, func_t *_adfunc, bool raw) : rawmode(raw) {
	attach(_ad, _adfunc);

	sps				= ad ? ad->sps : CONT_AD_SPS;

	// Set samples/frame such that when sps=16000, spf=256
	spf				= (sps * 256) / CONT_AD_SPS;
	adbufsize		= CONT_AD_ADFRMSIZE * spf;

	adbuf			= (int16*)iso::malloc(adbufsize * sizeof(int16));
	pow_hist		= calloc_1d<int>(CONT_AD_POWHISTSIZE);
	frm_pow			= calloc_1d<char>(CONT_AD_ADFRMSIZE);

	state			= SILENCE;
	read_ts			= 0;
	seglen			= 0;
	siglvl			= 0;
	prev_sample		= 0;
	tot_frm			= 0;
	noise_level		= CONT_AD_DEFAULT_NOISE;

	auto_thresh		= 1;
	delta_sil		= CONT_AD_DELTA_SIL;
	delta_speech	= CONT_AD_DELTA_SPEECH;
	min_noise		= CONT_AD_MIN_NOISE;
	max_noise		= CONT_AD_MAX_NOISE;
	winsize			= CONT_AD_WINSIZE;
	speech_onset	= CONT_AD_SPEECH_ONSET;
	sil_onset		= CONT_AD_SIL_ONSET;
	leader			= CONT_AD_LEADER;
	trailer			= CONT_AD_TRAILER;

	thresh_sil		= noise_level + delta_sil;
	thresh_speech	= noise_level + delta_speech;
	thresh_update	= CONT_AD_THRESH_UPDATE;
	adapt_rate		= CONT_AD_ADAPT_RATE;

	tail_state		= SILENCE;

	spseg_head		= NULL;
	spseg_tail		= NULL;

	n_calib_frame	= 0;

	reset();
}

cont_ad::~cont_ad() {
	while (spseg *seg = spseg_head) {
		spseg_head = seg->next;
		delete seg;
	}
	iso::free(adbuf);
	iso::free(pow_hist);
	iso::free(frm_pow);
}

// Reset, discarded any accumulated speech.
void cont_ad::reset() {
	while (spseg *seg = spseg_head) {
		spseg_head = seg->next;
		delete seg;
	}
	spseg_tail		= NULL;
	headfrm			= 0;
	n_frm			= 0;
	n_sample		= 0;
	win_startfrm	= 0;
	win_validfrm	= 0;
	n_other			= 0;
	tail_state		= SILENCE;
}

int cont_ad::frame_pow(int16 * buf, int * prev, int spf) {
	double	sumsq	= 0;
	int		p		= *prev;
	for (int i = 0; i < spf; i++) {
		// Note: pre-emphasis done to remove low-frequency noise.
		sumsq += square(double(buf[i] - p));
		p = buf[i];
	}
	*prev = p;

	if (sumsq < spf)	// Make sure FRMPOW(sumsq) >= 0
		sumsq = spf;

	return max(int((10.0 * (log10(sumsq) - log10(double(spf)))) + 0.5), 0);
}


// Classify frame (id=frm, starting at sample position s) as sil/nonsil
void cont_ad::compute_frame_pow(int frm) {
	int i = frame_pow(adbuf + (frm * spf), &(prev_sample), spf);
	frm_pow[frm] = (char)i;
	pow_hist[i]++;
	thresh_update--;
}


void cont_ad::decay_hist() {
	for (int i = 0; i < CONT_AD_POWHISTSIZE; i++)
		pow_hist[i] -= (pow_hist[i] >> CONT_AD_HIST_INERTIA);
}

// Find silence threshold from power histogram.
int cont_ad::find_thresh() {
	int old_noise_level, old_thresh_sil, old_thresh_speech;

	if (!auto_thresh)
		return 0;

	// Find smallest non-zero histogram entry, but starting at some minimum power.
	// Power lower than CONT_AD_MIN_NOISE indicates bad A/D input (eg, mic off...)
	int	i;
	for (i = min_noise; i < CONT_AD_POWHISTSIZE && pow_hist[i] == 0; i++);
	if (i > max_noise)	// Bad signal?
		return -1;

	// This method of detecting the noise level is VERY unsatisfactory
	int	max = 0;
	int th	= i;
	for (int j = i; j < CONT_AD_POWHISTSIZE && j < i + 20; j++) {
		if (max < pow_hist[j]) {
			max = pow_hist[j];
			th = j;
		}
	}

	// Don't change the threshold too fast
	old_noise_level		= noise_level;
	old_thresh_sil		= thresh_sil;
	old_thresh_speech	= thresh_speech;
	// noise_level		= int(th * adapt_rate + noise_level * (1.0 - adapt_rate));
	noise_level			= int(noise_level + adapt_rate * (th - noise_level) + 0.5);

	// update thresholds
	thresh_sil			= noise_level + delta_sil;
	thresh_speech		= noise_level + delta_speech;

	return 0;
}


// Silence to speech transition
void cont_ad::sil2speech_transition(int frm) {
	spseg	*seg	= new spseg(win_startfrm - leader, leader + winsize);
	if (!spseg_head)
		spseg_head = seg;
	else
		spseg_tail->next = seg;
	spseg_tail = seg;

	tail_state = SPEECH;

	// Now in SPEECH state; want to look for silence from end of this window
	win_validfrm = 1;
	win_startfrm = frm;

	// Count #sil frames remaining in reduced window (of 1 frame)
	n_other = frm_pow[frm] <= thresh_sil ? 1 : 0;
}

// Speech to silence transition
void cont_ad::speech2sil_transition(int frm) {
	// End of speech detected; speech->sil transition
	spseg_tail->nfrm += trailer;

	tail_state = SILENCE;

	// Now in SILENCE state; start looking for speech trailer+leader frames later
	win_validfrm -= trailer + leader - 1;
	win_startfrm += trailer + leader - 1;
	if (win_startfrm >= CONT_AD_ADFRMSIZE)
		win_startfrm -= CONT_AD_ADFRMSIZE;

	// Count #speech frames remaining in reduced window
	n_other = 0;
	for (int f = win_startfrm;;) {
		if (frm_pow[f] >= thresh_speech)
			n_other++;

		if (f == frm)
			break;

		f++;
		if (f >= CONT_AD_ADFRMSIZE)
			f = 0;
	}
}


// Main silence/speech region detection routine.
// If in SILENCE state, switch to SPEECH state if window is mostly non-silence.
// If in SPEECH state, switch to SILENCE state if the window is mostly silence.
void cont_ad::boundary_detect(int frm) {
	win_validfrm++;
	if (tail_state == SILENCE) {
		if (frm_pow[frm] >= thresh_speech)
			n_other++;
	} else {
		if (frm_pow[frm] <= thresh_sil)
			n_other++;
	}

	if (win_validfrm < winsize)	// Not reached full analysis window size
		return;

	if (tail_state == SILENCE) {	// Currently in SILENCE state
		if (n_frm >= winsize + leader && n_other >= speech_onset)
			sil2speech_transition(frm);
	} else {
		if (n_other >= sil_onset)
			speech2sil_transition(frm);
		else
			spseg_tail->nfrm++;	// In speech state, and staying there; add this frame to segment
	}

	// Get rid of oldest frame in analysis window
	if (tail_state == SILENCE) {
		if (frm_pow[win_startfrm] >= thresh_speech) {
			if (n_other > 0)
				n_other--;
		}
	} else {
		if (frm_pow[win_startfrm] <= thresh_sil) {
			if (n_other > 0)
				n_other--;
		}
	}
	win_validfrm--;
	win_startfrm++;
	if (win_startfrm >= CONT_AD_ADFRMSIZE)
		win_startfrm = 0;
}

int cont_ad::max_siglvl(int startfrm, int nfrm) {
	int siglvl = 0;
	if (nfrm > 0) {
		for (int i = 0, f = startfrm; i < nfrm; i++, f++) {
			if (f >= CONT_AD_ADFRMSIZE)
				f -= CONT_AD_ADFRMSIZE;
			if (frm_pow[f] > siglvl)
				siglvl = frm_pow[f];
		}
	}
	return siglvl;
}

// Copy data from adbuf[sf], for nf frames, into buf.
int cont_ad::buf_copy(int sf, int nf, int16 * buf) {
	if (sf + nf > CONT_AD_ADFRMSIZE) {
		// Amount to be copied wraps around adbuf; copy in two stages
		int	f = CONT_AD_ADFRMSIZE - sf;
		int	l = f * spf;
		memcpy(buf, adbuf + sf * spf, l * sizeof(int16));

		buf	+= l;
		sf	= 0;
		nf	-= f;
	}

	if (nf > 0) {
		int	l = nf * spf;
		memcpy(buf, adbuf + (sf * spf), l * sizeof(int16));
	}

	return sf + nf >= CONT_AD_ADFRMSIZE ? 0 : sf + nf;
}

int cont_ad::read_internal(int16 *buf, int max) {
	// First read as much of raw A/D as possible and available.	adbuf is not really a circular buffer, so may have to read in two steps for wrapping around.
	int	head	= headfrm * spf;
	int	tail	= head + n_sample;
	int	len		= n_sample - (n_frm * spf);	// #partial frame samples at the tail

	int	l;
	if (tail < adbufsize && (!eof)) {
		if (adfunc) {
			if ((l = (*adfunc)(ad, adbuf + tail, adbufsize - tail)) < 0) {
				eof = true;
				l	= 0;
			}
		} else {
			l = adbufsize - tail;
			if (l > max) {
				l = max;
				max = 0;
			} else {
				max -= l;
			}
			memcpy(adbuf + tail, buf, l * sizeof(int16));
			buf += l;
		}

		tail		+= l;
		len			+= l;
		n_sample	+= l;
	}
	if (tail >= adbufsize && !eof) {
		tail -= adbufsize;
		if (tail < head) {
			if (adfunc) {
				if ((l = (*adfunc)(ad, adbuf + tail, head - tail)) < 0) {
					eof	= true;
					l	= 0;
				}
			} else {
				l = head - tail;
				if (l > max)
					l = max;
				memcpy(adbuf + tail, buf, l * sizeof(int16));
			}
			tail += l;
			len += l;
			n_sample += l;
		}
	}

	return len;
}

// Classify incoming frames as silence or speech.
int cont_ad::classify(int len) {
	int tailfrm = headfrm + n_frm;	// Next free frame slot to be filled
	if (tailfrm >= CONT_AD_ADFRMSIZE)
		tailfrm -= CONT_AD_ADFRMSIZE;

	for (; len >= spf; len -= spf) {
		compute_frame_pow(tailfrm);
		n_frm++;
		tot_frm++;

		// Find speech/sil state change, if any. Also, if staying in speech state add this frame to current speech segment.
		boundary_detect(tailfrm);

		if (++tailfrm >= CONT_AD_ADFRMSIZE)
			tailfrm = 0;

		// Update thresholds if time to do so
		if (thresh_update <= 0) {
			find_thresh();
			decay_hist();
			thresh_update = CONT_AD_THRESH_UPDATE;

#if 1
			// Since threshold has been updated, recompute n_other.
			n_other = 0;
			if (tail_state == SILENCE) {
				for (int i = win_validfrm, f = win_startfrm; i > 0; --i) {
					if (frm_pow[f] >= thresh_speech)
						n_other++;
					f++;
					if (f >= CONT_AD_ADFRMSIZE)
						f = 0;
				}
			} else {
				for (int i = win_validfrm, f = win_startfrm; i > 0; --i) {
					if (frm_pow[f] <= thresh_sil)
						n_other++;
					f++;
					if (f >= CONT_AD_ADFRMSIZE)
						f = 0;
				}
			}
#endif
		}
	}

	return tail_state;
}

// Main function called by the application to filter out silence regions.
// Maintains a linked list of speech segments pointing into adbuf and feeds data to application from them.
int cont_ad::read(int16 * buf, int max) {
	if (!buf)
		return -1;

	if (max < spf)
		return -1;

	// Read data from adfunc or from buf.
	int	len = read_internal(buf, max);

	// Compute frame power for unprocessed+new data and find speech/silence boundaries
	classify(len);

	// If eof on input data source, cleanup the final segment.
	if (eof) {
		if (tail_state == SPEECH) {
			// Still inside a speech segment when input data got over.	Absort any remaining frames into the final speech segment.
			// Absorb frames still in analysis window into final speech seg
			spseg_tail->nfrm += win_validfrm;
			tail_state	= SILENCE;
		}

		win_startfrm += win_validfrm;
		if (win_startfrm >= CONT_AD_ADFRMSIZE)
			win_startfrm -= CONT_AD_ADFRMSIZE;
		win_validfrm = 0;
		n_other = 0;
	}

	// At last ready to copy speech data, if any, into caller's buffer
	// Raw speech data is segmented into alternating speech and silence segments.
	// But any single call to cont_ad_read will never cross a speech/silence boundary.

	spseg	*seg = spseg_head;	// first speech segment available, if any
	int		flen, retval;
	STATE	newstate;
	if (seg == NULL || headfrm != seg->startfrm) {
		// Either no speech data available, or inside a silence segment. Find length of silence segment.
		if (seg == NULL) {
			flen = eof ? n_frm : n_frm - (winsize + leader - 1);
			if (flen < 0)
				flen = 0;
		} else {
			flen = seg->startfrm - headfrm;
			if (flen < 0)
				flen += CONT_AD_ADFRMSIZE;
		}

		if (rawmode) {
			// Restrict silence segment to user buffer size, integral #frames
			int f = max / spf;
			if (flen > f)
				flen = f;
		}

		newstate = SILENCE;
	} else {
		flen = max / spf;		// truncate read-size to integral #frames
		if (flen > seg->nfrm)
			flen = seg->nfrm;	// truncate further to this segment size

		newstate = SPEECH;
	}

	len		= flen * spf;	// #samples being consumed
	siglvl	= max_siglvl(headfrm, flen);

	if (newstate == SILENCE && !rawmode) {
		// Skip silence data
		headfrm += flen;
		if (headfrm >= CONT_AD_ADFRMSIZE)
			headfrm -= CONT_AD_ADFRMSIZE;

		retval = 0;
	} else {
		// Copy speech/silence(in rawmode) data
		headfrm	= buf_copy(headfrm, flen, buf);
		retval	= len;	// #samples being copied/returned
	}

	n_frm		-= flen;
	n_sample	-= len;

	if (state == newstate)
		seglen += len;
	else
		seglen = len;
	state = newstate;

	if (newstate == SPEECH) {
		seg->startfrm	= headfrm;
		seg->nfrm		-= flen;

		// Free seg if empty and not recording into it
		if (seg->nfrm == 0 && (seg->next || tail_state == SILENCE)) {
			spseg_head = seg->next;
			if (seg->next == NULL)
				spseg_tail = NULL;
			delete seg;
		}
	}

	// Update timestamp. Total raw A/D read - those remaining to be consumed
	read_ts = (tot_frm - n_frm) * spf;

	if (retval == 0)
		retval = eof && spseg_head == NULL ? -1 : 0;

	return retval;
}

// Calibrate input channel for silence threshold.
int cont_ad::calibrate() {
	// clear histogram
	for (int i = 0; i < CONT_AD_POWHISTSIZE; i++)
		pow_hist[i] = 0;

	int	tailfrm = headfrm + n_frm;
	if (tailfrm >= CONT_AD_ADFRMSIZE)
		tailfrm -= CONT_AD_ADFRMSIZE;

	int s = (tailfrm * spf);
	for (n_calib_frame = 0; n_calib_frame < CONT_AD_CALIB_FRAMES; ++n_calib_frame) {
		int	len = spf;
		while (len > 0) {
			//Trouble
			int k = (*adfunc)(ad, adbuf + s, len);
			if (k < 0)
				return -1;
			len -= k;
			s	+= k;
		}
		s -= spf;
		compute_frame_pow(tailfrm);
	}

	thresh_update = CONT_AD_THRESH_UPDATE;
	return find_thresh();
}

int cont_ad::calibrate_loop(int16 * buf, int max) {
	if (n_calib_frame == CONT_AD_CALIB_FRAMES) {
		// If calibration previously succeeded, then this is a recalibration, so start again.
		n_calib_frame = 0;
		// clear histogram
		for (int i = 0; i < CONT_AD_POWHISTSIZE; i++)
			pow_hist[i] = 0;
	}

	int	tailfrm = headfrm + n_frm;
	if (tailfrm >= CONT_AD_ADFRMSIZE)
		tailfrm -= CONT_AD_ADFRMSIZE;

	int	s	= tailfrm * spf;
	int	len	= spf;
	for (; n_calib_frame < CONT_AD_CALIB_FRAMES;
		++n_calib_frame) {
			if (max < len)
				return 1;
			memcpy(adbuf + s, buf, len * sizeof(int16));
			max -= len;
			buf += len;
			compute_frame_pow(tailfrm);
	}

	thresh_update = CONT_AD_THRESH_UPDATE;
	return find_thresh();
}


int cont_ad::set_thresh1(int silence, int speech) {
	if (silence < 0 || speech < 0)
		return -1;
	delta_sil		= (3 * silence) / 2;
	delta_speech	= (3 * speech) / 2;
	return 0;
}

int cont_ad::set_thresh2(int silence, int speech) {
	thresh_speech	= speech;
	thresh_sil		= silence;

	// Since threshold has been updated, recompute n_other
	n_other = 0;
	if (tail_state == SILENCE) {
		for (int i = win_validfrm, f = win_startfrm; i > 0; --i) {
			if (frm_pow[f] >= thresh_speech)
				n_other++;

			f++;
			if (f >= CONT_AD_ADFRMSIZE)
				f = 0;
		}
	} else if (tail_state == SPEECH) {
		for (int i = win_validfrm, f = win_startfrm; i > 0; --i) {
			if (frm_pow[f] <= thresh_sil)
				n_other++;

			f++;
			if (f >= CONT_AD_ADFRMSIZE)
				f = 0;
		}
	}

	return 0;
}

int cont_ad::set_params(
	int _delta_sil, int _delta_speech,
	int _min_noise,	int _max_noise,
	int _winsize,
	int _speech_onset, int _sil_onset,
	int _leader, int _trailer,
	float _adapt_rate)
{
	if (_delta_sil < 0 || _delta_speech < 0 || _min_noise < 0 || _max_noise < 0)
		return -1;

	if (_speech_onset > _winsize || _speech_onset <= 0 || _winsize <= 0)
		return -1;

	if (_sil_onset > _winsize || _sil_onset <= 0 || _winsize <= 0)
		return -1;

	if (_leader + _trailer > _winsize || _leader <= 0 || _trailer <= 0)
		return -1;

	if (_adapt_rate < 0 || _adapt_rate > 1)
		return -1;

	delta_sil		= _delta_sil;
	delta_speech	= _delta_speech;
	min_noise		= _min_noise;
	max_noise		= _max_noise;

	winsize			= _winsize;
	speech_onset	= _speech_onset;
	sil_onset		= _sil_onset;
	leader			= _leader;
	trailer			= _trailer;

	adapt_rate		= _adapt_rate;

	if (win_validfrm >= winsize)
		win_validfrm = winsize - 1;

	return 0;
}

int cont_ad::get_params(int *_delta_sil,
	int *_delta_speech, int *_min_noise,
	int *_max_noise, int *_winsize,
	int *_speech_onset, int *_sil_onset,
	int *_leader, int *_trailer, float *_adapt_rate)
{
	if (!_delta_sil || !_delta_speech || !_min_noise || !_max_noise
	|| !_winsize || !_speech_onset || !_sil_onset || !_leader
	|| !_trailer || !_adapt_rate)
		return -1;

	*_delta_sil		= delta_sil;
	*_delta_speech	= delta_speech;
	*_min_noise		= min_noise;
	*_max_noise		= max_noise;

	*_winsize		= winsize;
	*_speech_onset	= speech_onset;
	*_sil_onset		= sil_onset;
	*_leader		= leader;
	*_trailer		= trailer;

	*_adapt_rate	= adapt_rate;

	return 0;
}

//-----------------------------------------------------------------------------
//	noise removal
// Computationally Efficient Speech Enchancement by Spectral Minina Tracking by G. Doblinger
// Power-Normalized Cepstral Coefficients (PNCC) for Robust Speech Recognition
// For the recent research and state of art see papers about IMRCA and
// A Minimum-Mean-Square-Error Noise Reduction Algorithm On Mel-Frequency Cepstra For Robust Speech Recognition by Dong Yu and others
//-----------------------------------------------------------------------------

// Noise supression constants
#define SMOOTH_WINDOW	4
#define LAMBDA_POWER	0.7f
#define LAMBDA_A		0.999f
#define LAMBDA_B		0.5f
#define LAMBDA_T		0.85f
#define MU_T			0.2f
#define MAX_GAIN		20

struct noise_stats {
	powspec_t	*power;			// Smoothed power
	powspec_t	*noise;			// Noise estimate
	powspec_t	*floor;			// Signal floor estimate
	powspec_t	*peak;			// Peak for temporal masking

	bool		undefined;		// Initialize it next time
	uint32		num_filters;	// Number of items to process

	// Precomputed constants
	powspec_t	lambda_power;
	powspec_t	comp_lambda_power;
	powspec_t	lambda_a;
	powspec_t	comp_lambda_a;
	powspec_t	lambda_b;
	powspec_t	comp_lambda_b;
	powspec_t	lambda_t;
	powspec_t	mu_t;
	powspec_t	max_gain;
	powspec_t	inv_max_gain;

	powspec_t	smooth_scaling[2 * SMOOTH_WINDOW + 3];

	void		low_envelope(powspec_t *buf, powspec_t *floor_buf, int num_filt);
	void		temp_masking(powspec_t *buf, powspec_t *peak, int num_filt);
	void		weight_smooth(powspec_t *buf, powspec_t *coefs, int num_filt);
public:
	void		remove_noise(powspec_t *mfspec);
	void		reset() { undefined = true; }

	noise_stats(int _num_filters);
	~noise_stats();
};


noise_stats::noise_stats(int _num_filters) {
	clear(*this);
	power		= calloc_1d<powspec_t>(num_filters);
	noise		= calloc_1d<powspec_t>(num_filters);
	floor		= calloc_1d<powspec_t>(num_filters);
	peak		= calloc_1d<powspec_t>(num_filters);

	undefined	= true;
	num_filters	= _num_filters;

	lambda_power		= LAMBDA_POWER;
	comp_lambda_power	= 1 - LAMBDA_POWER;
	lambda_a			= LAMBDA_A;
	comp_lambda_a		= 1 - LAMBDA_A;
	lambda_b			= LAMBDA_B;
	comp_lambda_b		= 1 - LAMBDA_B;
	lambda_t			= LAMBDA_T;
	mu_t				= 1 - LAMBDA_T;
	max_gain			= MAX_GAIN;
	inv_max_gain		= 1.0 / MAX_GAIN;

	for (int i = 0; i < 2 * SMOOTH_WINDOW + 1; i++)
		smooth_scaling[i] = 1.0 / i;
}

noise_stats::~noise_stats() {
	iso::free(power);
	iso::free(noise);
	iso::free(floor);
	iso::free(peak);
}

void noise_stats::low_envelope(powspec_t * buf, powspec_t * floor_buf, int num_filt) {
	for (int i = 0; i < num_filt; i++) {
		floor_buf[i] = buf[i] >= floor_buf[i]
			? lambda_a * floor_buf[i] + comp_lambda_a * buf[i]
			: lambda_b * floor_buf[i] + comp_lambda_b * buf[i];
	}
}

// temporal masking
void noise_stats::temp_masking(powspec_t * buf, powspec_t * peak, int num_filt) {
	for (int i = 0; i < num_filt; i++) {
		powspec_t cur_in = buf[i];
		peak[i] *= lambda_t;
		if (buf[i] < lambda_t * peak[i])
			buf[i] = peak[i] * mu_t;
		if (cur_in > peak[i])
			peak[i] = cur_in;
	}
}

// spectral weight smoothing
void noise_stats::weight_smooth(powspec_t * buf, powspec_t * coefs, int num_filt) {

	for (int i = 0; i < num_filt; i++) {
		int	l1 = i - SMOOTH_WINDOW > 0				? i - SMOOTH_WINDOW : 0;
		int	l2 = i + SMOOTH_WINDOW < num_filt - 1	? i + SMOOTH_WINDOW : num_filt - 1;

		powspec_t coef = 0;
		for (int j = l1; j <= l2; j++)
			coef += coefs[j];
		buf[i] = buf[i] * (coef / (l2 - l1 + 1));
	}
}


// For fixed point we are doing the computation in a fixlog domain, so we have to add many processing cases.
void noise_stats::remove_noise(powspec_t * mfspec) {
	int			n		= num_filters;
	powspec_t	*signal	= calloc_1d<powspec_t>(n);
	powspec_t	*gain	= calloc_1d<powspec_t>(n);

	if (undefined) {
		for (int i = 0; i < n; i++) {
			power[i] = mfspec[i];
			noise[i] = mfspec[i];
			floor[i] = mfspec[i] / max_gain;
			peak[i] = 0.0;
		}
		undefined = false;
	}

	// Calculate smoothed power
	for (int i = 0; i < n; i++)
		power[i] = lambda_power * power[i] + comp_lambda_power * mfspec[i];

	// Noise estimation
	low_envelope(power, noise, n);

	for (int i = 0; i < n; i++) {
		signal[i] = power[i] - noise[i];
		if (signal[i] < 0)
			signal[i] = 0;
	}

	low_envelope(signal, floor, n);
	temp_masking(signal, peak, n);

	for (int i = 0; i < n; i++) {
		if (signal[i] < floor[i])
			signal[i] = floor[i];
	}

	for (int i = 0; i < n; i++) {
		gain[i] = signal[i] < max_gain * power[i] ?	signal[i] / power[i] : max_gain;
		if (gain[i] < inv_max_gain)
			gain[i] = inv_max_gain;
	}

	// Weight smoothing and time frequency normalization
	weight_smooth(mfspec, gain, n);

	iso::free(signal);
	iso::free(gain);
}

//-----------------------------------------------------------------------------
//	warp
//-----------------------------------------------------------------------------

// Values for the 'logspec' field
enum {
	RAW_LOG_SPEC	= 1,
	SMOOTH_LOG_SPEC	= 2
};

// Values for the 'transform' field
enum {
	LEGACY_DCT	= 0,
	DCT_II		= 1,
	DCT_HTK		= 2
};

struct warp {
	virtual uint32	n_param();
	virtual void	set_parameters(const float *p, float sampling_rate);
	virtual float	warped_to_unwarped(float nonlinear);
	virtual float	unwarped_to_warped(float linear);
};

struct ringbuf {
	powspec_t**	bufs;
	int16		buf_num;
	int			buf_len;
	int16		start;
	int16		end;
	int			recs;
};

// sqrt(1/2), also used for unitary DCT-II/DCT-III
#define SQRT_HALF FLOAT2MFCC(0.707106781186548)

#define BB_SAMPLING_RATE			16000
#define DEFAULT_BB_FFT_SIZE			512
#define DEFAULT_BB_FRAME_SHIFT		160
#define DEFAULT_BB_NUM_FILTERS		40
#define DEFAULT_BB_LOWER_FILT_FREQ	133.33334
#define DEFAULT_BB_UPPER_FILT_FREQ	6855.4976

#define NB_SAMPLING_RATE			8000
#define DEFAULT_NB_FFT_SIZE			256
#define DEFAULT_NB_FRAME_SHIFT		80
#define DEFAULT_NB_NUM_FILTERS		31
#define DEFAULT_NB_LOWER_FILT_FREQ	200
#define DEFAULT_NB_UPPER_FILT_FREQ	3500

// Apply 1/2 bit noise to a buffer of audio.
int fe_dither(int16 *buffer, int nsamps);

fixed32 fe_log_add(fixed32 x, fixed32 y);
fixed32 fe_log_sub(fixed32 x, fixed32 y);

//-----------------------------------------------------------------------------
//	Warps
//-----------------------------------------------------------------------------

// 	Warp the frequency axis according to an inverse_linear function, i.e.:	w' = w / a
struct warp_inverse_linear : warp {
	enum {N_PARAM = 1};
	float	params[N_PARAM];

	bool	is_neutral;
	float	nyquist_frequency;

	uint32	n_param()							{ return N_PARAM; }
	void	set_parameters(const float *p, float sampling_rate);
	float	warped_to_unwarped(float nonlinear) { return is_neutral ? nonlinear : nonlinear * params[0]; }
	float	unwarped_to_warped(float linear)	{ return is_neutral ? linear : linear / params[0]; }

	warp_inverse_linear() : is_neutral(true), nyquist_frequency(0) {
		params[0] = 1;
	}
};

void warp_inverse_linear::set_parameters(const float *p, float sampling_rate) {
	nyquist_frequency = sampling_rate / 2;
	if (p == NULL) {
		is_neutral = true;
		return;
	}
	memcpy(params, p, N_PARAM * sizeof(float));
	is_neutral = params[0] == 0;
}

// Warp the frequency axis according to an affine function, i.e.: w' = a * w + b
struct warp_affine : warp {
	enum {N_PARAM = 2};
	float	params[N_PARAM];

	bool	is_neutral;
	float	nyquist_frequency;

	uint32	n_param()							{ return N_PARAM; }
	void	set_parameters(const float *p, float sampling_rate);
	float	warped_to_unwarped(float nonlinear) { return is_neutral ? nonlinear : (nonlinear - params[1]) / params[0]; }
	float	unwarped_to_warped(float linear)	{ return is_neutral ? linear : linear * params[0] + params[1]; }

	warp_affine() : is_neutral(true), nyquist_frequency(0) {
		params[0] = 1;
		params[1] = 0;
	}
};
void warp_affine::set_parameters(const float *p, float sampling_rate) {
	nyquist_frequency = sampling_rate / 2;
	if (p == NULL) {
		is_neutral = true;
		return;
	}
	memcpy(params, p, N_PARAM * sizeof(float));
	is_neutral = params[0] == 1 && params[1] == 0;
}

// Warp the frequency axis according to an piecewise linear function.
// The function is linear up to a frequency F, where the slope changes so that the Nyquist frequency in the warped axis maps to the Nyquist frequency in the unwarped.
// w' = a * w, w < F
// w' = a' * w + b, W > F
// w'(0) = 0
// w'(F) = F
// w'(Nyq) = Nyq
struct warp_piecewise_linear : warp {
	enum {N_PARAM = 2};
	float	params[N_PARAM];

	bool	is_neutral;
	float	nyquist_frequency;
	float	final_piece[2];

	uint32	n_param()							{ return N_PARAM; }
	void	set_parameters(const float *p, float sampling_rate);
	float	warped_to_unwarped(float nonlinear) { return is_neutral ? nonlinear : nonlinear < params[0] * params[1] ? nonlinear / params[0] : (nonlinear - final_piece[1]) / final_piece[0]; }
	float	unwarped_to_warped(float linear)	{ return is_neutral ? linear : linear < params[1] ? linear * params[0] : final_piece[0] * linear + final_piece[1]; }

	warp_piecewise_linear() : is_neutral(true), nyquist_frequency(0) {
		params[0] = 1;
		params[1] = 6800;
	}
};

void warp_piecewise_linear::set_parameters(const float *p, float sampling_rate) {
	nyquist_frequency = sampling_rate / 2;
	if (p == NULL) {
		is_neutral = true;
		return;
	}
	memcpy(params, p, N_PARAM * sizeof(float));
	memset(final_piece, 0, 2 * sizeof(float));
	if (params[1] < sampling_rate) {
		// Precompute these. These are the coefficients of a straight line that contains the points (F, aF) and (N, N), where a = params[0], F = params[1], N = Nyquist frequency.
		if (params[1] == 0)
			params[1] = sampling_rate * 0.85f;
		final_piece[0] = (nyquist_frequency - params[0] * params[1]) / (nyquist_frequency - params[1]);
		final_piece[1] = nyquist_frequency * params[1] * (params[0] - 1.0f) / (nyquist_frequency - params[1]);
	} else {
		memset(final_piece, 0, 2 * sizeof(float));
	}
	is_neutral = params[0] == 0;
}

//-----------------------------------------------------------------------------
//	Signal Processing
//-----------------------------------------------------------------------------

// Base Struct to hold all structure for MFCC computation
struct melfb {
	float	sampling_rate;
	int		num_cepstra;
	int		num_filters;
	int		fft_size;
	float	lower_filt_freq;
	float	upper_filt_freq;
	// DCT coefficients
	float	**mel_cosine;
	// Filter coefficients
	float	*filt_coeffs;
	int16	*spec_start;
	int16	*filt_start;
	int16	*filt_width;
	int		doublewide;

	warp	*warper;

	// Precomputed normalization constants for unitary DCT-II/DCT-III
	float	sqrt_inv_n, sqrt_inv_2n;
	// Value and coefficients for HTK-style liftering
	int		lifter_val;
	float	*lifter;
	// Normalize filters to unit area
	int		unit_area;
	// Round filter frequencies to DFT points (hurts accuracy, but is useful for legacy purposes)
	int		round_filters;
public:
	melfb()					{ clear(*this); }
	~melfb();
	float	mel(float x)	{ float warped = warper->unwarped_to_warped(x); return 2595 * log10(1 + warped / 700); }
	float	melinv(float x)	{ float warped = 700 * (pow(10.f, x / 2595) - 1); return warper->warped_to_unwarped(warped); }

	bool	build_filters();
	void	compute_melcosine();
};

melfb::~melfb() {
	if (mel_cosine)
		free_2d(mel_cosine);
	iso::free(lifter);
	iso::free(spec_start);
	iso::free(filt_start);
	iso::free(filt_width);
	iso::free(filt_coeffs);
}

bool melfb::build_filters() {
	// Filter coefficient matrix, in flattened form.
	spec_start = new int16[num_filters];
	filt_start = new int16[num_filters];
	filt_width = new int16[num_filters];

	// First calculate the widths of each filter.
	// Minimum and maximum frequencies in mel scale.
	float	melmin = mel(lower_filt_freq);
	float	melmax = mel(upper_filt_freq);

	// Width of filters in mel scale
	float	melbw = (melmax - melmin) / (num_filters + 1);
	if (doublewide) {
		melmin -= melbw;
		melmax += melbw;
		if (melinv(melmin) < 0 || melinv(melmax) > sampling_rate / 2)
			return false;
	}

	// DFT point spacing
	float	fftfreq = sampling_rate / fft_size;

	// Count and place filter coefficients.
	int	n_coeffs = 0;
	for (int i = 0; i < num_filters; ++i) {
		float freqs[3];

		// Left, center, right frequencies in Hertz
		for (int j = 0; j < 3; ++j) {
			if (doublewide)
				freqs[j] = melinv((i + j * 2) * melbw + melmin);
			else
				freqs[j] = melinv((i + j) * melbw + melmin);
			// Round them to DFT points if requested
			if (round_filters)
				freqs[j] = int(freqs[j] / fftfreq + 0.5f) * fftfreq;
		}

		// spec_start is the start of this filter in the power spectrum.
		spec_start[i] = -1;
		// There must be a better way...
		for (int j = 0; j < fft_size / 2 + 1; ++j) {
			float hz = j * fftfreq;
			if (hz < freqs[0])
				continue;
			else if (hz > freqs[2] || j == fft_size / 2) {
				// filt_width is the width in DFT points of this filter.
				filt_width[i] = j - spec_start[i];
				// filt_start is the start of this filter in the filt_coeffs array.
				filt_start[i] = n_coeffs;
				n_coeffs += filt_width[i];
				break;
			}
			if (spec_start[i] == -1)
				spec_start[i] = j;
		}
	}

	// Now go back and allocate the coefficient array.
	filt_coeffs = new float[n_coeffs];

	// And now generate the coefficients.
	n_coeffs = 0;
	for (int i = 0; i < num_filters; ++i) {
		float freqs[3];

		// Left, center, right frequencies in Hertz
		for (int j = 0; j < 3; ++j) {
			if (doublewide)
				freqs[j] = melinv((i + j * 2) * melbw + melmin);
			else
				freqs[j] = melinv((i + j) * melbw + melmin);
			// Round them to DFT points if requested
			if (round_filters)
				freqs[j] = int(freqs[j] / fftfreq + 0.5f) * fftfreq;
		}

		for (int j = 0; j < filt_width[i]; ++j) {
			float	hz = (spec_start[i] + j) * fftfreq;
			if (hz < freqs[0] || hz > freqs[2])
				return false;
			float	loslope = (hz - freqs[0]) / (freqs[1] - freqs[0]);
			float	hislope = (freqs[2] - hz) / (freqs[2] - freqs[1]);
			if (unit_area) {
				loslope *= 2 / (freqs[2] - freqs[0]);
				hislope *= 2 / (freqs[2] - freqs[0]);
			}
			if (loslope < hislope)
				filt_coeffs[n_coeffs] = loslope;
			else
				filt_coeffs[n_coeffs] = hislope;
			++n_coeffs;
		}
	}

	return true;
}

void melfb::compute_melcosine() {
	mel_cosine = calloc_2d<float>(num_cepstra, num_filters);

	double	freqstep = pi / num_filters;
	// NOTE: The first row vector is actually unnecessary but we leave * it in to avoid confusion.
	for (int i = 0; i < num_cepstra; i++) {
		for (int j = 0; j < num_filters; j++)
			mel_cosine[i][j] = cos(freqstep * i * (j + 0.5));
	}

	// Also precompute normalization constants for unitary DCT.
	sqrt_inv_n	= rsqrt(float(num_filters));
	sqrt_inv_2n = rsqrt(num_filters * 0.5);

	// And liftering weights
	if (lifter_val) {
		lifter = calloc_1d<float>(num_cepstra);
		for (int i = 0; i < num_cepstra; ++i)
			lifter[i] = 1 + sin(pi * i / lifter_val) * lifter_val / 2;
	}
}

void pre_emphasis(int16 const *in, frame_t * out, int len, float factor, int16 prior) {
	out[0] = (frame_t) in[0] - (frame_t) prior *factor;
	for (int i = 1; i < len; i++)
		out[i] = (frame_t) in[i] - (frame_t) in[i - 1] * factor;
}

void short_to_frame(int16 const *in, frame_t * out, int len) {
	for (int i = 0; i < len; i++)
		out[i] = (frame_t) in[i];
}

void do_hamming_window(frame_t * in, window_t * window, int in_len, bool remove_dc) {
	if (remove_dc) {
		frame_t mean = 0;

		for (int i = 0; i < in_len; i++)
			mean += in[i];
		mean /= in_len;
		for (int i = 0; i < in_len; i++)
			in[i] -= (frame_t) mean;
	}

	for (int i = 0; i < in_len / 2; i++) {
		in[i] = in[i] * window[i];
		in[in_len - 1 - i] = in[in_len - 1 - i] * window[i];
	}
}

//-----------------------------------------------------------------------------
//	Front end
//-----------------------------------------------------------------------------

struct cmd_ln_t;

/// Structure for the front-end computation.
struct fe {
//	cmd_ln_t *config;
	int			refcount;

	float		sampling_rate;
	int16		frame_rate;
	int16		frame_shift;

	float		window_length;
	int16		frame_size;
	int16		fft_size;

	uint8		fft_order;
	uint8		feature_dimension;
	uint8		num_cepstra;
	bool		remove_dc;
	uint8		log_spec;
	bool		dither;
	uint8		transform;
	bool		remove_noise;

	float		pre_emphasis_alpha;
	int			seed;

	int16		frame_counter;
	uint8		start_flag;
	uint8		reserved;

	// Twiddle factors for FFT.
	frame_t		*ccc, *sss;
	// Mel filter parameters.
	melfb		*mel_fb;
	// Half of a Hamming Window.
	window_t	*hamming_window;
	// Storage for noise removal
	noise_stats	*noise;

	genrand		rand;

	// Temporary buffers for processing.
	int16		*spch;
	frame_t		*frame;
	powspec_t	*spec, *mfspec;
	int16		*overflow_samps;
	int16		num_overflow_samps;
	int16		prior;

	int		floato_float(float ** input, float ** output, int nframes);
	int		float_to_mfcc(float ** input, float ** output, int nframes);
	int		logspec_to_mfcc(const float * fr_spec, float * fr_cep);
	int		logspec_dct2(const float * fr_spec, float * fr_cep);
	int		mfcc_dct3(const float * fr_cep, float * fr_spec);

	int		spch_to_frame(int len);
	int		fft_real();
	void	spec_magnitude();
	void	mel_spec();
	void	mel_cep(float * mfcep);
	void	lifter(float * mfcep);
	int		read_frame(int16 const *in, int len);
	int		shift_frame(int16 const *in, int len);
	bool	write_frame(float *fea);
	void	spec2cep(const powspec_t * mflogspec, float * mfcep);
	void	dct2(const powspec_t *mflogspec, float *mfcep, bool htk);
	void	dct3(const float *mfcep, powspec_t *mflogspec);
public:
	fe() : refcount(1) {}
	~fe();

	bool	init(cmd_ln_t *config);
	void	init_dither(int seed)	{ rand.init(seed); }
	fe*		retain()				{ ++refcount; return this; }
	int		release()				{ if (--refcount > 0) return refcount; delete this; return 0; }

	void	start_utt();
	int		get_output_size() const	{ return feature_dimension; }
	void	get_input_size(int *out_frame_shift, int *out_frame_size);
	bool	process_frame(const int16 *spch, int nsamps, float *fr_cep);
	bool	process_frames(const int16 **inout_spch,size_t *inout_nsamps,float **buf_cep,int *inout_nframes);
	int		process_utt(const int16 * spch, size_t nsamps, float *** cep_block, int * nframes);
	void	end_utt(float * cepvector, int * nframes);
};

fe::~fe() {
	// kill FE instance - iso::free everything...
	delete mel_fb;
	iso::free(spch);
	iso::free(frame);
	iso::free(ccc);
	iso::free(sss);
	iso::free(spec);
	iso::free(mfspec);
	iso::free(overflow_samps);
	iso::free(hamming_window);

	delete noise;
	//cmd_ln_free_r(config);
}

bool fe::init(cmd_ln_t *config) {
	// transfer params to front end
//	if (fe_parse_general_params(cmd_ln_retain(config), fe) < 0)
//		return false;

	// compute remaining fe parameters
	frame_shift		= int(sampling_rate / frame_rate + 0.5);
	frame_size		= int(window_length * sampling_rate + 0.5);
	prior			= 0;
	frame_counter	= 0;

	if (frame_size > fft_size)
		return false;

	if (dither)
		init_dither(seed);

	// establish buffers for overflow samps and hamming window
	overflow_samps = calloc_1d<int16>(frame_size);

	// Symmetric, so we only create the first half of it.
	hamming_window = calloc_1d<window_t>(frame_size / 2);
	for (int i = 0; i < frame_size / 2; i++)
		hamming_window[i] = (0.54 - 0.46 * cos(two * pi * i / ((double)frame_size - 1)));

	// init and fill appropriate filter structure
	mel_fb = new melfb;

	// transfer params to mel fb
	//fe_parse_melfb_params(config, fe, mel_fb);
	mel_fb->build_filters();
	mel_fb->compute_melcosine();

	if (remove_noise)
		noise = new noise_stats(mel_fb->num_filters);

	// Create temporary FFT, spectrum and mel-spectrum buffers.
	spch	= calloc_1d<int16>(frame_size);
	frame	= calloc_1d<frame_t>(fft_size);
	spec	= calloc_1d<powspec_t>(fft_size);
	mfspec	= calloc_1d<powspec_t>(mel_fb->num_filters);

	// create twiddle factors
	ccc		= calloc_1d<frame_t>(fft_size / 4);
	sss		= calloc_1d<frame_t>(fft_size / 4);
	for (int i = 0; i < fft_size / 4; ++i) {
		double a = 2 * pi * i / fft_size;
		ccc[i] = cos(a);
		sss[i] = sin(a);
	}

	// Initialize the overflow buffers
	start_utt();
	return true;
}

void fe::start_utt() {
	num_overflow_samps = 0;
	memset(overflow_samps, 0, frame_size * sizeof(int16));
	start_flag = 1;
	prior = 0;

	if (remove_noise)
		noise->reset();
}

void fe::get_input_size(int *out_frame_shift, int *out_frame_size) {
	if (out_frame_shift)
		*out_frame_shift = frame_shift;
	if (out_frame_size)
		*out_frame_size = frame_size;
}

bool fe::process_frame(const int16 *spch, int nsamps, float *fr_cep) {
	read_frame(spch, nsamps);
	return write_frame(fr_cep);
}

bool fe::process_frames(const int16 **inout_spch, size_t *inout_nsamps, float **buf_cep, int *inout_nframes) {
	// In the special case where there is no output buffer, return the maximum number of frames which would be generated.
	if (buf_cep == NULL) {
		if (*inout_nsamps + num_overflow_samps < (size_t)frame_size)
			*inout_nframes = 0;
		else
			*inout_nframes = 1 + ((*inout_nsamps + num_overflow_samps - frame_size) / frame_shift);
		return *inout_nframes;
	}

	// Are there not enough samples to make at least 1 frame?
	if (*inout_nsamps + num_overflow_samps < (size_t)frame_size) {
		if (*inout_nsamps > 0) {
			// Append them to the overflow buffer.
			memcpy(overflow_samps + num_overflow_samps, *inout_spch, *inout_nsamps * (sizeof(int16)));
			num_overflow_samps += int16(*inout_nsamps);
			// Update input-output pointers and counters.
			*inout_spch += *inout_nsamps;
			*inout_nsamps = 0;
		}
		// We produced no frames of output, sorry!
		*inout_nframes = 0;
		return 0;
	}

	// Can't write a frame?	Then do nothing!
	if (*inout_nframes < 1) {
		*inout_nframes = 0;
		return 0;
	}

	// Keep track of the original start of the buffer.
	const int16	*orig_spch		= *inout_spch;
	int			orig_n_overflow = num_overflow_samps;
	// How many frames will we be able to get?
	int	frame_count = 1 + ((*inout_nsamps + num_overflow_samps - frame_size) / frame_shift);
	// Limit it to the number of output frames available.
	if (frame_count > *inout_nframes)
		frame_count = *inout_nframes;
	// Index of output frame.
	int	outidx = 0;

	// Start processing, taking care of any incoming overflow.
	if (num_overflow_samps) {
		int offset = frame_size - num_overflow_samps;

		// Append start of spch to overflow samples to make a full frame.
		memcpy(overflow_samps + num_overflow_samps, *inout_spch, offset * sizeof(**inout_spch));
		read_frame(overflow_samps, frame_size);
		int	n = write_frame(buf_cep[outidx]);
		if (n < 0)
			return false;
		outidx += n;
		// Update input-output pointers and counters.
		*inout_spch			+= offset;
		*inout_nsamps		-= offset;
		num_overflow_samps	-= frame_shift;
	} else {
		read_frame(*inout_spch, frame_size);
		int	n = write_frame(buf_cep[outidx]);
		if (n < 0)
			return false;
		outidx			+= n;
		// Update input-output pointers and counters.
		*inout_spch		+= frame_size;
		*inout_nsamps	-= frame_size;
	}

	// Process all remaining frames.
	for (int i = 1; i < frame_count; ++i) {
		shift_frame(*inout_spch, frame_shift);
		int	n = write_frame(buf_cep[outidx]);
		if (n < 0)
			return false;
		outidx			+= n;
		// Update input-output pointers and counters.
		*inout_spch		+= frame_shift;
		*inout_nsamps	-= frame_shift;
		// Amount of data behind the original input which is still needed.
		if (num_overflow_samps > 0)
			num_overflow_samps -= frame_shift;
	}

	// How many relevant overflow samples are there left?
	if (num_overflow_samps <= 0) {
		// Maximum number of overflow samples past *inout_spch to save.
		int	n_overflow = *inout_nsamps;
		if (n_overflow > frame_shift)
			n_overflow = frame_shift;
		num_overflow_samps = frame_size - frame_shift;
		// Make sure this isn't an illegal read!
		if (num_overflow_samps > *inout_spch - orig_spch)
			num_overflow_samps = *inout_spch - orig_spch;
		num_overflow_samps += n_overflow;
		if (num_overflow_samps > 0) {
			memcpy(overflow_samps,
				*inout_spch - (frame_size - frame_shift),
				num_overflow_samps * sizeof(**inout_spch));
			// Update the input pointer to cover this stuff.
			*inout_spch		+= n_overflow;
			*inout_nsamps	-= n_overflow;
		}
	} else {
		// There is still some relevant data left in the overflow buffer.
		// Shift existing data to the beginning.
		memmove(overflow_samps, overflow_samps + orig_n_overflow - num_overflow_samps, num_overflow_samps * sizeof(*overflow_samps));
		// Copy in whatever we had in the original speech buffer.
		int	n_overflow = *inout_spch - orig_spch + *inout_nsamps;
		if (n_overflow > frame_size - num_overflow_samps)
			n_overflow = frame_size - num_overflow_samps;
		memcpy(overflow_samps + num_overflow_samps, orig_spch, n_overflow * sizeof(*orig_spch));
		num_overflow_samps += n_overflow;
		// Advance the input pointers.
		if (n_overflow > *inout_spch - orig_spch) {
			n_overflow		-= (*inout_spch - orig_spch);
			*inout_spch		+= n_overflow;
			*inout_nsamps	-= n_overflow;
		}
	}

	// Finally update the frame counter with the number of frames we procesed.
	*inout_nframes = outidx; // FIXME: Not sure why I wrote it this way...
	return 0;
}

int fe::process_utt(const int16 * spch, size_t nsamps, float *** cep_block, int * nframes) {
	// Figure out how many frames we will need.
	process_frames(NULL, &nsamps, NULL, nframes);
	// Create the output buffer (it has to exist, even if there are no output frames).
	float **cep = calloc_2d<float>(*nframes ? *nframes : 1, feature_dimension);
	// Now just call fe_process_frames() with the allocated buffer.
	int	r = process_frames(&spch, &nsamps, cep, nframes);
	*cep_block = cep;
	return r;
}

void fe::end_utt(float * cepvector, int * nframes) {
	// Process any remaining data.
	if (num_overflow_samps > 0) {
		read_frame(overflow_samps, num_overflow_samps);
		*nframes = write_frame(cepvector);
	} else {
		*nframes = 0;
	}

	// reset overflow buffers...
	num_overflow_samps = 0;
	start_flag = 0;
}

// Convert a block of float to float (can be done in-place)
int fe::floato_float(float **input, float **output, int nframes) {
	int	n = nframes * feature_dimension;
	if (input != output) {
		for (int i = 0; i < n; ++i)
			output[0][i] = input[0][i];
	}
	return n;
}

// Convert a block of float to float (can be done in-place)
int fe::float_to_mfcc(float ** input, float ** output, int nframes) {
	int	n = nframes * feature_dimension;
	if (input != output) {
		for (int i = 0; i < nframes * feature_dimension; ++i)
			output[0][i] = input[0][i];
	}
	return n;
}

int fe::logspec_to_mfcc(const float * fr_spec, float * fr_cep) {
	powspec_t *powspec = new powspec_t[mel_fb->num_filters];
	for (int i = 0; i < mel_fb->num_filters; ++i)
		powspec[i] = (powspec_t)fr_spec[i];
	spec2cep(powspec, fr_cep);
	delete[] powspec;
	return 0;
}

int fe::logspec_dct2(const float * fr_spec, float * fr_cep) {
	powspec_t *powspec = new powspec_t[mel_fb->num_filters];
	for (int i = 0; i < mel_fb->num_filters; ++i)
		powspec[i] = (powspec_t) fr_spec[i];
	dct2(powspec, fr_cep, 0);
	delete[] powspec;
	return 0;
}

int fe::mfcc_dct3(const float * fr_cep, float * fr_spec) {
	powspec_t *powspec = new powspec_t[mel_fb->num_filters];
	dct3(fr_cep, powspec);
	for (int i = 0; i < mel_fb->num_filters; ++i)
		fr_spec[i] = (float) powspec[i];
	delete[] powspec;
	return 0;
}

int fe::spch_to_frame(int len) {
	// Copy to the frame buffer.
	if (pre_emphasis_alpha != 0.0) {
		pre_emphasis(spch, frame, len, pre_emphasis_alpha, prior);
		if (len >= frame_shift)
			prior = spch[frame_shift - 1];
		else
			prior = spch[len - 1];
	} else {
		short_to_frame(spch, frame, len);
	}

	// Zero pad up to FFT size.
	memset(frame + len, 0, (fft_size - len) * sizeof(*frame));

	// Window.
	do_hamming_window(frame, hamming_window, frame_size, remove_dc);
	return len;
}

int fe::read_frame(int16 const *in, int len) {
	if (len > frame_size)
		len = frame_size;

	// Read it into the raw speech buffer.
	memcpy(spch, in, len * sizeof(*in));
	// dither if necessary.
	if (dither) {
		for (int i = 0; i < len; ++i)
			spch[i] += int(rand.next() % 4 == 0);
	}

	return spch_to_frame(len);
}

int fe::shift_frame(int16 const *in, int len) {
	if (len > frame_shift)
		len = frame_shift;
	int	offset = frame_size - frame_shift;

	// Shift data into the raw speech buffer.
	memmove(spch, spch + frame_shift, offset * sizeof(*spch));
	memcpy(spch + offset, in, len * sizeof(*spch));
	// dither if necessary.
	if (dither) {
		for (int i = 0; i < len; ++i)
			spch[offset + i] += int(rand.next() % 4 == 0);
	}

	return spch_to_frame(offset + len);
}

// Translated from the FORTRAN from "Real-Valued Fast Fourier Transform Algorithms" by Henrik V. Sorensen et al.
// IEEE Transactions on Acoustics, Speech, and Signal Processing, vol. 35, no.6.
// The 16-bit version does a version of "block floating point" in order to avoid rounding errors.
int fe::fft_real() {
	frame_t *x = frame;
	int		m = fft_order;
	int		n = fft_size;

	// Bit-reverse the input.
	int j = 0;
	for (int i = 0; i < n - 1; ++i) {
		if (i < j) {
			frame_t	xt = x[j];
			x[j] = x[i];
			x[i] = xt;
		}
		int k = n / 2;
		while (k <= j) {
			j -= k;
			k /= 2;
		}
		j += k;
	}

	// Basic butterflies (2-point FFT, real twiddle factors):
	// x[i]	= x[i] +	1 * x[i+1]
	// x[i+1] = x[i] + -1 * x[i+1]

	for (int i = 0; i < n; i += 2) {
		frame_t	xt = x[i];
		x[i] = (xt + x[i + 1]);
		x[i + 1] = (xt - x[i + 1]);
	}

	// The rest of the butterflies, in stages from 1..m
	for (int k = 1; k < m; ++k) {
		int	n4 = k - 1;
		int	n2 = k;
		int	n1 = k + 1;
		// Stride over each (1 << (k+1)) points
		for (int i = 0; i < n; i += (1 << n1)) {
			// Basic butterfly with real twiddle factors:
			// x[i]	= x[i] +	1 * x[i + (1<<k)]
			// x[i + (1<<k)] = x[i] + -1 * x[i + (1<<k)]

			frame_t	xt = x[i];
			x[i] = (xt + x[i + (1 << n2)]);
			x[i + (1 << n2)] = (xt - x[i + (1 << n2)]);

			// The other ones with real twiddle factors:
			// x[i + (1<<k) + (1<<(k-1))]	= 0 * x[i + (1<<k-1)] + -1 * x[i + (1<<k) + (1<<k-1)]
			// x[i + (1<<(k-1))]			= 1 * x[i + (1<<k-1)] +	0 * x[i + (1<<k) + (1<<k-1)]

			x[i + (1 << n2) + (1 << n4)] = -x[i + (1 << n2) + (1 << n4)];
			x[i + (1 << n4)] = x[i + (1 << n4)];

			// Butterflies with complex twiddle factors. There are (1<<k-1) of them.

			for (int j = 1; j < (1 << n4); ++j) {
				int	i1 = i + j;
				int	i2 = i + (1 << n2) - j;
				int	i3 = i + (1 << n2) + j;
				int	i4 = i + (1 << n2) + (1 << n2) - j;

				// cc = real(W[j * n / (1<<(k+1))])
				// ss = imag(W[j * n / (1<<(k+1))])
				frame_t cc = ccc[j << (m - n1)];
				frame_t ss = sss[j << (m - n1)];

				// There are some symmetry properties which allow us to get away with only four multiplications here.
				frame_t t1 = x[i3] * cc + x[i4] * ss;
				frame_t t2 = x[i3] * ss - x[i4] * cc;

				x[i4] = (x[i2] - t2);
				x[i3] = (-x[i2] - t2);
				x[i2] = (x[i1] - t1);
				x[i1] = (x[i1] + t1);
			}
		}
	}

	// This isn't used, but return it for completeness.
	return m;
}

void fe::spec_magnitude() {
	// Do FFT and get the scaling factor back (only actually used in fixed-point).	Note the scaling factor is expressed in bits.
	int			scale	= fft_real();
	// We need to scale things up the rest of the way to N.
	scale = fft_order - scale;

	// The first point (DC coefficient) has no imaginary part
	spec[0] = frame[0] * frame[0];

	for (int j = 1; j <= fft_size / 2; j++)
		spec[j] = frame[j] * frame[j] + frame[fft_size - j] * frame[fft_size - j];
}

void fe::mel_spec() {
	for (int whichfilt = 0; whichfilt < mel_fb->num_filters; whichfilt++) {
		int	spec_start = mel_fb->spec_start[whichfilt];
		int	filt_start = mel_fb->filt_start[whichfilt];

 		mfspec[whichfilt] = 0;
		for (int i = 0; i < mel_fb->filt_width[whichfilt]; i++)
			mfspec[whichfilt] += spec[spec_start + i] * mel_fb->filt_coeffs[filt_start + i];
	}
}

#define LOG_FLOOR 1e-4

void fe::mel_cep(float *mfcep) {
	for (int i = 0; i < mel_fb->num_filters; ++i)
		mfspec[i] = ln(mfspec[i] + LOG_FLOOR);

	// If we are doing LOG_SPEC, then do nothing.
	if (log_spec == RAW_LOG_SPEC) {
		for (int i = 0; i < feature_dimension; i++)
			mfcep[i] = mfspec[i];
	} else if (log_spec == SMOOTH_LOG_SPEC) {
		// For smoothed spectrum, do DCT-II followed by (its inverse) DCT-III
		dct2(mfspec, mfcep, 0);
		dct3(mfcep, mfspec);
		for (int i = 0; i < feature_dimension; i++)
			mfcep[i] = mfspec[i];
	} else if (transform == DCT_II) {
		dct2(mfspec, mfcep, FALSE);
	} else if (transform == DCT_HTK) {
		dct2(mfspec, mfcep, TRUE);
	} else {
		spec2cep(mfspec, mfcep);
	}
}

void fe::spec2cep(const powspec_t *mflogspec, float *mfcep) {
	// Compute C0 separately (its basis vector is 1) to avoid costly multiplications.
	mfcep[0] = mflogspec[0] / 2;	// beta = 0.5

	for (int j = 1; j < mel_fb->num_filters; j++)
		mfcep[0] += mflogspec[j];	// beta = 1.0
	mfcep[0] /= (frame_t) mel_fb->num_filters;

	for (int i = 1; i < num_cepstra; ++i) {
		mfcep[i] = 0;
		for (int j = 0; j < mel_fb->num_filters; j++)
			mfcep[i] += mflogspec[j] * mel_fb->mel_cosine[i][j] * (j == 0 ? 0.5f : 1);
		mfcep[i] /= mel_fb->num_filters;
	}
}

void fe::dct2(const powspec_t * mflogspec, float * mfcep, bool htk) {
	// Compute C0 separately (its basis vector is 1) to avoid costly multiplications.
	mfcep[0] = mflogspec[0];
	for (int j = 1; j < mel_fb->num_filters; j++)
		mfcep[0] += mflogspec[j];
	mfcep[0] = mfcep[0] * (htk ? mel_fb->sqrt_inv_2n : mel_fb->sqrt_inv_n);

	for (int i = 1; i < num_cepstra; ++i) {
		mfcep[i] = 0;
		for (int j = 0; j < mel_fb->num_filters; j++)
			mfcep[i] += mflogspec[j] * mel_fb->mel_cosine[i][j];
		mfcep[i] = mfcep[i] * mel_fb->sqrt_inv_2n;
	}
}

void fe::lifter(float * mfcep) {
	if (mel_fb->lifter_val != 0) {
		for (int i = 0; i < num_cepstra; ++i)
			mfcep[i] = mfcep[i] * mel_fb->lifter[i];
	}
}

void fe::dct3(const float * mfcep, powspec_t *mflogspec) {
	for (int i = 0; i < mel_fb->num_filters; ++i) {
		mflogspec[i] = mfcep[0] * rsqrt2;
		for (int j = 1; j < num_cepstra; j++)
			mflogspec[i] += mfcep[j] * mel_fb->mel_cosine[j][i];
		mflogspec[i] = mflogspec[i] * mel_fb->sqrt_inv_2n;
	}
}

bool fe::write_frame(float * fea) {
	spec_magnitude();
	mel_spec();
	if (remove_noise)
		noise->remove_noise(mfspec);
	mel_cep(fea);
	lifter(fea);
	return true;
}

//-----------------------------------------------------------------------------
//	Features
//-----------------------------------------------------------------------------
#define LIVEBUFBLOCKSIZE	256	//* Blocks of 256 vectors allocated for livemode decoder
#define S3_MAX_FRAMES		15000	// RAH, I believe this is still too large, but better than before
#define FEAT_VERSION	"1.0"
#define FEAT_DCEP_WIN	2

// Structure for describing a speech feature type (no. of streams and stream widths), as well as the computation for converting the input speech (e.g., Sphinx-II format MFC cepstra) into this type of feature vectors.
struct feat {
	typedef	void func(feat *fcb, float **input, float **feat);

	int			refcount;		// Reference count.
	int			cepsize;		// Size of input speech vector (typically, a cepstrum vector)
	int			n_stream;		// Number of feature streams; e.g., 4 in Sphinx-II
	uint32*		stream_len;		// Vector length of each feature stream
	int			window_size;	// Number of extra frames around given input frame needed to compute corresponding output feature (so total = window_size*2 + 1)
	int			n_sv;			// Number of subvectors
	uint32*		sv_len;			// Vector length of each subvector
	int**		subvecs;		// Subvector specification (or NULL for none)
	float*		sv_buf;			// Temporary copy buffer for subvector projection
	int			sv_dim;			// Total dimensionality of subvector (length of sv_buf)

	cmn::type	cmn_type;		// Type of CMN to be performed on each utterance
	agc::type	agc_type;		// Type of AGC to be performed on each utterance
	int			varnorm;		// Whether variance normalization is to be performed on each utt;	Irrelevant if no CMN is performed

	func*		compute_feat;
	cmn*		cmn_struct;		// Structure that stores the temporary variables for cepstral means normalization
	agc*		agc_struct;		// Structure that stores the temporary variables for acoustic	gain control

	float**		cepbuf;			// Circular buffer of MFCC frames for live feature computation.
	float**		tmpcepbuf;		// Array of pointers into cepbuf to handle border cases.
	int			bufpos;			// Write index in cepbuf.
	int			curpos;			// Read index in cepbuf.

	float***	lda;			// Array of linear transformations (for LDA, MLLT, or whatever)
	uint32		n_lda;			// Number of linear transformations in lda.
	uint32		out_dim;		// Output dimensionality


	void		subvec_project(float ***inout_feat, uint32 nfr);
	float***	array_alloc(int nfr);
	float***	array_realloc(float ***old_feat, int ofr, int nfr);
	/*
	void		s2_4x_cep2feat(float ** mfc, float ** feat);
	void		s3_1x39_cep2feat(float ** mfc, float ** feat);
	void		s3_cep(float ** mfc, float ** feat);
	void		s3_cep_dcep(float ** mfc, float ** feat);
	void		_1s_c_d_dd_cep2feat(float ** mfc, float ** feat);
	void		_1s_c_d_ld_dd_cep2feat(float ** mfc, float ** feat);
	void		copy(float ** mfc, float ** feat);
	*/
	void		do_cmn(float **mfc, int nfr, int beginutt, int endutt);
	void		do_agc(float **mfc, int nfr, int beginutt, int endutt);
	void		compute_utt(float **mfc, int nfr, int win, float ***feat);
	int			s2mfc_read_norm_pad(char *file, int win, int sf, int ef, float ***out_mfc, int maxfr, int cepsize);
	int			s2mfc2feat(const char *file, const char *dir, const char *cepext, int sf, int ef, float *** feat, int maxfr);
	int			s2mfc2feat_block_utt(float ** uttcep, int nfr, float *** ofeat);
	int			s2mfc2feat_live(float ** uttcep, int *inout_ncep, int beginutt, int endutt, float *** ofeat);
	void		lda_transform(float ***inout_feat, uint32 nfr);
public:

	feat(char const *type, cmn::type _cmn, int _varnorm, agc::type _agc, int _cepsize);
	~feat();

	feat*		retain()			{ ++refcount; return this; }
	int			release()			{ if (--refcount > 0) return refcount; delete this; return 0; }

	uint32		dimension1()		{ return n_sv ? n_sv : n_stream; }
	uint32		dimension2(int i)	{ return lda ? out_dim : (sv_len ? sv_len[i] : stream_len[i]); }
	uint32		dimension()			{ return out_dim; }
	uint32		*stream_lengths()	{ return lda ? &out_dim : sv_len ? sv_len : stream_len; }

	int			read_lda(const char *ldafile, int dim);
	int			set_subvecs(int **subvecs);
};

feat::~feat() {
	free_2d(cepbuf);
	iso::free(tmpcepbuf);
	free_3d(lda);

	iso::free(stream_len);
	iso::free(sv_len);
	iso::free(sv_buf);

	for (int **sv = subvecs; sv && *sv; ++sv)
		iso::free(*sv);
	iso::free(subvecs);

	delete cmn_struct;
	delete agc_struct;
}

int feat::set_subvecs(int **_subvecs) {
	if (_subvecs == NULL) {
		for (int **sv = subvecs; sv && *sv; ++sv)
			iso::free(*sv);
		iso::free(subvecs);

		iso::free(sv_buf);
		iso::free(sv_len);
		n_sv	= 0;
		subvecs	= NULL;
		sv_len	= NULL;
		sv_buf	= NULL;
		sv_dim	= 0;
		return 0;
	}

	if (n_stream != 1)
		return -1;

	int	n	= 0;
	int	d	= 0;
	for (int **sv = _subvecs; sv && *sv; ++sv) {
		for (int *d = *sv; d && *d != -1; ++d)
			++d;
		++n;
	}
	if (d > dimension())
		return -1;

	n_sv	= n;
	subvecs	= _subvecs;
	sv_len	= calloc_1d<uint32>(n);
	sv_buf	= calloc_1d<float>(d);
	sv_dim	= d;
	for (int i = 0; i < n_sv; ++i) {
		for (int *d = subvecs[i]; d && *d != -1; ++d)
			++sv_len[i];
	}

	return 0;
}

// Project feature components to subvectors (if any).
void feat::subvec_project(float ***inout_feat, uint32 nfr) {
	if (subvecs == NULL)
		return;

	for (uint32 i = 0; i < nfr; ++i) {
		float *out = sv_buf;
		for (int j = 0; j < n_sv; ++j) {
			for (int *d = subvecs[j]; d && *d != -1; ++d)
				*out++ = inout_feat[i][0][*d];
		}
		memcpy(inout_feat[i][0], sv_buf, sv_dim * sizeof(*sv_buf));
	}
}

float ***feat::array_alloc(int nfr) {
	// Make sure to use the dimensionality of the features before LDA and subvector projection.
	int	k = 0;
	for (int i = 0; i < n_stream; ++i)
		k += stream_len[i];

	float ***feat = calloc_2d<float*>(nfr, dimension1());
	float	*data = calloc_1d<float>(nfr * k);

	for (int i = 0; i < nfr; i++) {
		float	*d = data + i * k;
		for (int j = 0; j < dimension1(); j++) {
			feat[i][j] = d;
			d += dimension2(j);
		}
	}
	return feat;
}

float ***feat::array_realloc(float ***old_feat, int ofr, int nfr) {
	// Make sure to use the dimensionality of the features *before* LDA and subvector projection.
	int k = 0;
	for (int i = 0; i < n_stream; ++i)
		k += stream_len[i];

	float***new_feat	= array_alloc(nfr);
	int		cf			= min(nfr, ofr);
	memcpy(new_feat[0][0], old_feat[0][0], cf * k * sizeof(float));

	iso::free(old_feat[0][0]);
	free_2d(old_feat);

	return new_feat;
}

void feat::do_cmn(float **mfc, int nfr, int beginutt, int endutt) {
	cmn::type	t = cmn_type;
	if (!(beginutt && endutt) && t != cmn::NONE) // Only cmn_prior in block computation mode.
		t = cmn::PRIOR;

	switch (t) {
		case cmn::CURRENT:
			cmn_struct->calc(mfc, varnorm, nfr);
			break;
		case cmn::PRIOR:
			cmn_struct->prior(mfc, varnorm, nfr);
			if (endutt)
				cmn_struct->prior_update();
			break;
		default:
			;
	}
}

void feat::do_agc(float **mfc, int nfr, int beginutt, int endutt) {
	agc::type t = agc_type;

	if (!(beginutt && endutt) && t != agc::NONE) // Only agc_emax in block computation mode.
		t = agc::EMAX;

	switch (t) {
		case agc::MAX:
			agc_struct->calc_max(mfc, nfr);
			break;
		case agc::EMAX:
			agc_struct->emax(mfc, nfr);
			if (endutt)
				agc_struct->emax_update();
			break;
		case agc::NOISE:
			agc_struct->noise(mfc, nfr);
			break;
		default:
			;
	}
}

void feat::lda_transform(float ***inout_feat, uint32 nfr) {
	float *tmp = calloc_1d<float>(stream_len[0]);
	for (int i = 0; i < nfr; ++i) {
		// Do the matrix multiplication inline here since fcb->lda is transposed (eigenvectors in rows not columns).
		memset(tmp, 0, sizeof(float) * stream_len[0]);
		for (int j = 0; j < dimension(); ++j) {
			for (int k = 0; k < stream_len[0]; ++k)
				tmp[j] += inout_feat[i][0][k] * lda[0][j][k];
		}
		memcpy(inout_feat[i][0], tmp, stream_len[0] * sizeof(float));
	}
	iso::free(tmp);
}

void feat::compute_utt(float **mfc, int nfr, int win, float ***feat) {
	// Create feature vectors
	for (int i = win; i < nfr - win; i++)
		compute_feat(this, mfc + i, feat[i - win]);

	if (lda)
		lda_transform(feat, nfr - win * 2);

	if (subvecs)
		subvec_project(feat, nfr - win * 2);
}

int feat::s2mfc2feat_block_utt(float ** uttcep, int nfr, float *** ofeat) {
	// Copy and pad out the utterance (this requires that the feature computation functions always access the buffer via the frame pointers, which they do)
	float **cepbuf = calloc_1d<float*>(nfr + window_size * 2);
	memcpy(cepbuf + window_size, uttcep, nfr * sizeof(float*));

	// Do normalization before we interpolate on the boundary
	do_cmn(cepbuf + window_size, nfr, 1, 1);
	do_agc(cepbuf + window_size, nfr, 1, 1);

	// Now interpolate
	for (int i = 0; i < window_size; ++i) {
		cepbuf[i] = cepbuf[i];
		memcpy(cepbuf[i], uttcep[0], cepsize * sizeof(float));
		cepbuf[nfr + window_size + i] = cepbuf[window_size + i];
		memcpy(cepbuf[nfr + window_size + i], uttcep[nfr - 1], cepsize * sizeof(float));
	}
	// Compute as usual.
	compute_utt(cepbuf, nfr + window_size * 2, window_size, ofeat);
	iso::free(cepbuf);
	return nfr;
}

int feat::s2mfc2feat_live(float **uttcep, int *inout_ncep, int beginutt, int endutt, float ***ofeat) {
	int zero = 0;

	// Avoid having to check this everywhere.
	if (inout_ncep == NULL)
		inout_ncep = &zero;

	// Special case for entire utterances.
	if (beginutt && endutt && *inout_ncep > 0)
		return s2mfc2feat_block_utt(uttcep, *inout_ncep, ofeat);

	// Empty the input buffer on start of utterance.
	if (beginutt)
		bufpos = curpos;

	// Calculate how much data is in the buffer already.
	int	nbufcep = bufpos - curpos;
	if (nbufcep < 0)
		nbufcep = bufpos + LIVEBUFBLOCKSIZE - curpos;

	// Add any data that we have to replicate.
	if (beginutt && *inout_ncep > 0)
		nbufcep += window_size;
	if (endutt)
		nbufcep += window_size;

	// Only consume as much input as will fit in the buffer.
	if (nbufcep + *inout_ncep > LIVEBUFBLOCKSIZE) {
		// We also can't overwrite the trailing window, hence the reason why window_size is subtracted here.
		*inout_ncep = LIVEBUFBLOCKSIZE - nbufcep - window_size;
		// Cancel end of utterance processing.
		endutt = FALSE;
	}

	// FIXME: Don't modify the input!
	do_cmn(uttcep, *inout_ncep, beginutt, endutt);
	do_agc(uttcep, *inout_ncep, beginutt, endutt);

	// Replicate first frame into the first window_size frames if we're at the beginning of the utterance and there was some actual input to deal with.
	if (beginutt && *inout_ncep > 0) {
		for (int i = 0; i < window_size; i++) {
			memcpy(cepbuf[bufpos++], uttcep[0], cepsize * sizeof(float));
			bufpos %= LIVEBUFBLOCKSIZE;
		}
		// Move the current pointer past this data.
		curpos = bufpos;
		nbufcep -= window_size;
	}

	// Copy in frame data to the circular buffer.
	for (int i = 0; i < *inout_ncep; ++i) {
		memcpy(cepbuf[bufpos++], uttcep[i], cepsize * sizeof(float));
		bufpos %= LIVEBUFBLOCKSIZE;
		++nbufcep;
	}

	// Replicate last frame into the last window_size frames if we're at the end of the utterance (even if there was no input, so we can flush the output).
	if (endutt) {
		int tpos	= bufpos == 0 ? LIVEBUFBLOCKSIZE - 1 : bufpos - 1; // Index of last input frame.
		for (int i = 0; i < window_size; ++i) {
			memcpy(cepbuf[bufpos++], cepbuf[tpos], cepsize * sizeof(float));
			bufpos %= LIVEBUFBLOCKSIZE;
		}
	}

	// We have to leave the trailing window of frames.
	int	nfeatvec = nbufcep - window_size;
	if (nfeatvec <= 0)
		return 0; // Do nothing.

	for (int i = 0; i < nfeatvec; ++i) {
		// Handle wraparound cases.
		if (curpos - window_size < 0 || curpos + window_size >= LIVEBUFBLOCKSIZE) {
			// Use tmpcepbuf for this case.	Actually, we just need the pointers.
			for (int j = -window_size; j <= window_size; ++j)
				tmpcepbuf[window_size + j] = cepbuf[(curpos + j + LIVEBUFBLOCKSIZE) % LIVEBUFBLOCKSIZE];
			compute_feat(this, tmpcepbuf + window_size, ofeat[i]);
		} else {
			compute_feat(this, cepbuf + curpos, ofeat[i]);
		}
		// Move the read pointer forward.
		++curpos;
		curpos %= LIVEBUFBLOCKSIZE;
	}

	if (lda)
		lda_transform(ofeat, nfeatvec);

	if (subvecs)
		subvec_project(ofeat, nfeatvec);

	return nfeatvec;
}

void feat_s2_4x_cep2feat(feat *fsb, float ** mfc, float ** feat) {
	// CEP; skip C0
	int	cepsize	= fsb->cepsize;
	memcpy(feat[0], mfc[0] + 1, (cepsize - 1) * sizeof(float));

	// DCEP(SHORT): mfc[2] - mfc[-2]
	// DCEP(LONG):	mfc[4] - mfc[-4]
	float	*w	= mfc[2] + 1;	// +1 to skip C0
	float	*_w = mfc[-2] + 1;

	float	*f	= feat[1];
	for (int i = 0; i < cepsize - 1; i++) // Short-term
		f[i] = w[i] - _w[i];

	w	= mfc[4] + 1;	// +1 to skip C0
	_w	= mfc[-4] + 1;

	for (int j = 0, i = cepsize - 1; j < cepsize - 1; i++, j++)	// Long-term
		f[i] = w[j] - _w[j];

	// D2CEP: (mfc[3] - mfc[-1]) - (mfc[1] - mfc[-3])
	float	*w1		= mfc[3] + 1;	// Final +1 to skip C0
	float	*_w1	= mfc[-1] + 1;
	float	*w_1	= mfc[1] + 1;
	float	*_w_1	= mfc[-3] + 1;

	f = feat[3];
	for (int i = 0; i < cepsize - 1; i++) {
		float	d1 = w1[i] - _w1[i];
		float	d2 = w_1[i] - _w_1[i];
		f[i] = d1 - d2;
	}

	// POW: C0, DC0, D2C0; differences computed as above for rest of cep
	f = feat[2];
	f[0] = mfc[0][0];
	f[1] = mfc[2][0] - mfc[-2][0];

	float	d1 = mfc[3][0] - mfc[-1][0];
	float	d2 = mfc[1][0] - mfc[-3][0];
	f[2] = d1 - d2;
}


void feat_s3_1x39_cep2feat(feat *fsb, float ** mfc, float ** feat) {
	// CEP; skip C0
	int	cepsize	= fsb->cepsize;
	memcpy(feat[0], mfc[0] + 1, (cepsize - 1) * sizeof(float));
	// DCEP: mfc[2] - mfc[-2];

	float	*f	= feat[0] + cepsize - 1;
	float	*w	= mfc[2] + 1;	// +1 to skip C0
	float	*_w	= mfc[-2] + 1;

	for (int i = 0; i < cepsize - 1; i++)
		f[i] = w[i] - _w[i];

	// POW: C0, DC0, D2C0
	f += cepsize - 1;

	f[0] = mfc[0][0];
	f[1] = mfc[2][0] - mfc[-2][0];

	float	d1 = mfc[3][0] - mfc[-1][0];
	float	d2 = mfc[1][0] - mfc[-3][0];
	f[2] = d1 - d2;

	// D2CEP: (mfc[3] - mfc[-1]) - (mfc[1] - mfc[-3])
	f += 3;

	float	*w1 = mfc[3] + 1;	// Final +1 to skip C0
	float	*_w1 = mfc[-1] + 1;
	float	*w_1 = mfc[1] + 1;
	float	*_w_1 = mfc[-3] + 1;

	for (int i = 0; i < cepsize - 1; i++) {
		d1 = w1[i] - _w1[i];
		d2 = w_1[i] - _w_1[i];
		f[i] = d1 - d2;
	}
}

void feat_s3_cep(feat *fsb, float ** mfc, float ** feat) {
	// CEP
	int	cepsize	= fsb->cepsize;
	memcpy(feat[0], mfc[0], cepsize * sizeof(float));
}

void feat_s3_cep_dcep(feat *fsb, float ** mfc, float ** feat) {
	// CEP
	int	cepsize	= fsb->cepsize;
	memcpy(feat[0], mfc[0], cepsize * sizeof(float));

	// DCEP: mfc[2] - mfc[-2];
	float *f = feat[0] + cepsize;
	float *w = mfc[2];
	float *_w = mfc[-2];

	for (int i = 0; i < cepsize; i++)
		f[i] = w[i] - _w[i];
}

void feat_1s_c_d_dd_cep2feat(feat *fsb, float ** mfc, float ** feat) {
	// CEP
	int	cepsize	= fsb->cepsize;
	memcpy(feat[0], mfc[0], cepsize * sizeof(float));

	// DCEP: mfc[w] - mfc[-w], where w = FEAT_DCEP_WIN;
	float *f = feat[0] + cepsize;
	float *w = mfc[FEAT_DCEP_WIN];
	float *_w = mfc[-FEAT_DCEP_WIN];

	for (int i = 0; i < cepsize; i++)
		f[i] = w[i] - _w[i];

	// D2CEP: (mfc[w+1] - mfc[-w+1]) - (mfc[w-1] - mfc[-w-1]),  where w = FEAT_DCEP_WIN
	f += cepsize;

	float *w1 = mfc[FEAT_DCEP_WIN + 1];
	float *_w1 = mfc[-FEAT_DCEP_WIN + 1];
	float *w_1 = mfc[FEAT_DCEP_WIN - 1];
	float *_w_1 = mfc[-FEAT_DCEP_WIN - 1];

	for (int i = 0; i < cepsize; i++) {
		float	d1 = w1[i] - _w1[i];
		float	d2 = w_1[i] - _w_1[i];
		f[i] = d1 - d2;
	}
}

void feat_1s_c_d_ld_dd_cep2feat(feat *fsb, float ** mfc, float ** feat) {
	// CEP
	int	cepsize	= fsb->cepsize;
	memcpy(feat[0], mfc[0], cepsize * sizeof(float));

	// DCEP: mfc[w] - mfc[-w], where w = FEAT_DCEP_WIN;
	float *f	= feat[0] + cepsize;
	float *w	= mfc[FEAT_DCEP_WIN];
	float *_w	= mfc[-FEAT_DCEP_WIN];

	for (int i = 0; i < cepsize; i++)
		f[i] = w[i] - _w[i];

	// LDCEP: mfc[w] - mfc[-w], where w = FEAT_DCEP_WIN * 2;

	f	+= cepsize;
	w	= mfc[FEAT_DCEP_WIN * 2];
	_w	= mfc[-FEAT_DCEP_WIN * 2];

	for (int i = 0; i < cepsize; i++)
		f[i] = w[i] - _w[i];

	// D2CEP: (mfc[w+1] - mfc[-w+1]) - (mfc[w-1] - mfc[-w-1]), where w = FEAT_DCEP_WIN
	f += cepsize;

	float *w1	= mfc[FEAT_DCEP_WIN + 1];
	float *_w1	= mfc[-FEAT_DCEP_WIN + 1];
	float *w_1	= mfc[FEAT_DCEP_WIN - 1];
	float *_w_1	= mfc[-FEAT_DCEP_WIN - 1];

	for (int i = 0; i < cepsize; i++) {
		float	d1 = w1[i] - _w1[i];
		float	d2 = w_1[i] - _w_1[i];
		f[i] = d1 - d2;
	}
}

void feat_copy(feat *fsb, float ** mfc, float ** feat) {
	int	cepsize	= fsb->cepsize;
	int	win		= fsb->window_size;

	// Concatenate input features
	for (int i = -win; i <= win; ++i) {
		uint32 spos = 0;

		for (int j = 0; j < fsb->n_stream; ++j) {
			// Unscale the stream length by the window.
			uint32 len = fsb->stream_len[j] / (2 * win + 1);
			memcpy(feat[j] + ((i + win) * len), mfc[i] + spos, len * sizeof(float));
			spos += len;
		}
	}
}

feat::feat(char const *type, cmn::type _cmn, int _varnorm, agc::type _agc, int _cepsize) {
	clear(*this);
	if (_cepsize == 0)
		_cepsize = 13;

	refcount	= 1;
	if (strcmp(type, "s2_4x") == 0) {
		// Sphinx-II format 4-stream feature (Hack!! hardwired constants below)
		cepsize			= 13;
		n_stream		= 4;
		stream_len		= calloc_1d<uint32>(4);
		stream_len[0]	= 12;
		stream_len[1]	= 24;
		stream_len[2]	= 3;
		stream_len[3]	= 12;
		out_dim			= 51;
		window_size		= 4;
		compute_feat	= feat_s2_4x_cep2feat;

	} else if ((strcmp(type, "s3_1x39") == 0) || (strcmp(type, "1s_12c_12d_3p_12dd") == 0)) {
		// 1-stream cep/dcep/pow/ddcep (Hack!! hardwired constants below)
		cepsize			= 13;
		n_stream		= 1;
		stream_len		= calloc_1d<uint32>(1);
		stream_len[0]	= 39;
		out_dim			= 39;
		window_size		= 3;
		compute_feat	= feat_s3_1x39_cep2feat;

	} else if (strncmp(type, "1s_c_d_dd", 9) == 0) {
		cepsize			= _cepsize;
		n_stream		= 1;
		stream_len		= calloc_1d<uint32>(1);
		stream_len[0]	= cepsize * 3;
		out_dim			= cepsize * 3;
		window_size		= FEAT_DCEP_WIN + 1; // ddcep needs the extra 1
		compute_feat	= feat_1s_c_d_dd_cep2feat;

	} else if (strncmp(type, "1s_c_d_ld_dd", 12) == 0) {
		cepsize			= _cepsize;
		n_stream		= 1;
		stream_len		= calloc_1d<uint32>(1);
		stream_len[0]	= cepsize * 4;
		out_dim			= cepsize * 4;
		window_size		= FEAT_DCEP_WIN * 2;
		compute_feat	= feat_1s_c_d_ld_dd_cep2feat;

	} else if (strncmp(type, "cep_dcep", 8) == 0 || strncmp(type, "1s_c_d", 6) == 0) {
		// 1-stream cep/dcep
		cepsize			= _cepsize;
		n_stream		= 1;
		stream_len		= calloc_1d<uint32>(1);
		stream_len[0]	= cepsize * 2;
		out_dim			= stream_len[0];
		window_size		= 2;
		compute_feat	= feat_s3_cep_dcep;

	} else if (strncmp(type, "cep", 3) == 0 || strncmp(type, "1s_c", 4) == 0) {
		// 1-stream cep
		cepsize			= _cepsize;
		n_stream		= 1;
		stream_len		= calloc_1d<uint32>(1);
		stream_len[0]	= cepsize;
		out_dim			= stream_len[0];
		window_size		= 0;
		compute_feat	= feat_s3_cep;

	} else if (strncmp(type, "1s_3c", 5) == 0 || strncmp(type, "1s_4c", 5) == 0) {
		// 1-stream cep with frames concatenated, so called cepwin features
		window_size		= strncmp(type, "1s_3c", 5) == 0 ? 3 : 4;
		cepsize			= _cepsize;
		n_stream		= 1;
		stream_len		= calloc_1d<uint32>(1);
		stream_len[0]	= cepsize * (2 * window_size + 1);
		out_dim			= stream_len[0];
		compute_feat	= feat_copy;

	} else {
#if 0
		char *mtype	= ckd_salloc(type);
		char *wd	= ckd_salloc(type);
		// Generic definition: Format should be %d,%d,%d,...,%d (i.e., comma separated list of feature stream widths; #items = #streams).
		// An optional window size (frames will be concatenated) is also allowed, which can be specified with a colon after the list of feature streams.
		int	l = strlen(mtype);
		int	k = 0;
		for (int i = 1; i < l - 1; i++) {
			if (mtype[i] == ',') {
				mtype[i] = ' ';
				k++;
			}
			else if (mtype[i] == ':') {
				mtype[i] = '\0';
				window_size = atoi(mtype + i + 1);
				break;
			}
		}
		k++;	// Presumably there are (#commas+1) streams
		n_stream	= k;
		stream_len	= calloc_1d<uint32>(k);

		// Scan individual feature stream lengths
		char	*strp = mtype;
		int		i = 0;
		out_dim = 0;
		cepsize = 0;
		while (sscanf(strp, "%s%n", wd, &l) == 1) {
			strp += l;
			if (i >= n_stream || (sscanf(wd, "%u", &stream_len[i]) != 1) || stream_len[i] <= 0)
				E_FATAL("Bad feature type argument\n");
			// Input size before windowing
			cepsize += stream_len[i];
			if (window_size > 0)
				stream_len[i] *= (window_size * 2 + 1);
			// Output size after windowing
			out_dim += stream_len[i];
			i++;
		}
		if (i != n_stream)
			E_FATAL("Bad feature type argument\n");
		if (cepsize != cepsize)
			E_FATAL("Bad feature type argument\n");

		// Input is already the feature stream
		compute_feat = feat_copy;
		iso::free(mtype);
		iso::free(wd);
#endif
	}

	cmn_type	= _cmn;
	agc_type	= _agc;
	varnorm		= _varnorm;

	if (_cmn != cmn::NONE)
		cmn_struct = new cmn(cepsize);

	if (_agc != agc::NONE) {
		agc_struct = new agc;
		// HACK: hardwired initial estimates based on use of CMN (from Sphinx2)
		agc_struct->emax_set(_cmn != cmn::NONE ? 5.0 : 10.0);
	}

	// Make sure this buffer is large enough to be used in feat_s2mfc2feat_block_utt()
	cepbuf		= calloc_2d<float>(max(LIVEBUFBLOCKSIZE, window_size * 2), cepsize);
	// This one is actually just an array of pointers to "flatten out" wraparounds.
	tmpcepbuf	= calloc_1d<float*>(2 * window_size + 1);
}

#if 0
int	feat::read_lda(const char *ldafile, int dim) {
	FILE *fh;
	int byteswap;
	uint32 i, m, n;
	char **argname, **argval;

	if (n_stream != 1) {
		return -1;
	}

	if ((fh = fopen(ldafile, "rb")) == NULL)
		return -1;

	if (bio_readhdr(fh, &argname, &argval, &byteswap) < 0) {
		fclose(fh);
		return -1;
	}

	bio_hdrarg_free(argname, argval);
	argname = argval = NULL;

	uint32	chksum = 0;

	if (feat->lda)
		ckd_free_3d((void ***)feat->lda);

	{
		// Use a temporary variable to avoid strict-aliasing problems
		void ***outlda;
		if (bio_fread_3d(&outlda, sizeof(float32),
			&feat->n_lda, &m, &n,
			fh, byteswap, &chksum) < 0) {
				E_ERROR_SYSTEM("%s: bio_fread_3d(lda) failed\n", ldafile);
				fclose(fh);
				return -1;
		}
		feat->lda = (void *)outlda;
	}
	fclose(fh);

	// Note that SphinxTrain stores the eigenvectors as row vectors.
	if (n != feat->stream_len[0])
		E_FATAL("LDA matrix dimension %d doesn't match feature stream size %d\n", n, feat->stream_len[0]);

	// Override dim from file if it is 0 or greater than m.
	if (dim > m || dim <= 0) {
		dim = m;
	}
	feat->out_dim = dim;

	return 0;
}

// Read Sphinx-II format mfc file (s2mfc = Sphinx-II format MFC data).
// If out_mfc is NULL, no actual reading will be done, and the number of frames (plus padding) that would be read is returned.
// It's important that normalization is done before padding because frames outside the data we are interested in shouldn't be taken
// into normalization stats.

int feat::s2mfc_read_norm_pad(char *file, int win, int sf, int ef, float ***out_mfc, int maxfr, int cepsize) {
	FILE *fp;
	int n_float32;
	float *float_feat;
	struct stat statbuf;
	int i, n;
	int start_pad, end_pad;
	float **mfc;

	// Initialize the output pointer to NULL, so that any attempts to free() it if we fail before allocating it will not segfault!
	if (out_mfc)
		*out_mfc = NULL;
	if (ef >= 0 && ef <= sf)
		return -1;

	// Find filesize; HACK!! To get around intermittent NFS failures, use stat_retry
	if (stat_retry(file, &statbuf) < 0 || (fp = fopen(file, "rb")) == NULL)
		return -1;

	// Read #floats in header
	if (fread_retry(&n_float32, sizeof(int), 1, fp) != 1) {
		fclose(fp);
		return -1;
	}

	// Check if n_float32 matches file size
	int	byterev = 0;
	if ((int) (n_float32 * sizeof(float) + 4) != (int) statbuf.st_size) { // RAH, typecast both sides to remove compile warning
		n = n_float32;
		SWAP_INT32(&n);

		if ((int) (n * sizeof(float) + 4) != (int) (statbuf.st_size)) {	// RAH, typecast both sides to remove compile warning
			fclose(fp);
			return -1;
		}

		n_float32 = n;
		byterev = 1;
	}
	if (n_float32 <= 0) {
		fclose(fp);
		return -1;
	}

	// Convert n to #frames of input
	n = n_float32 / cepsize;
	if (n * cepsize != n_float32) {
		fclose(fp);
		return -1;
	}

	// Check start and end frames
	if (sf > 0) {
		if (sf >= n) {
			fclose(fp);
			return -1;
		}
	}
	if (ef < 0)
		ef = n-1;
	else if (ef >= n)
		ef = n-1;

	// Add window to start and end frames
	sf -= win;
	ef += win;
	if (sf < 0) {
		start_pad = -sf;
		sf = 0;
	}
	else
		start_pad = 0;
	if (ef >= n) {
		end_pad = ef - n + 1;
		ef = n - 1;
	}
	else
		end_pad = 0;

	// Limit n if indicated by [sf..ef]
	if (ef - sf + 1 < n)
		n = (ef - sf + 1);

	if (maxfr > 0 && n + start_pad + end_pad > maxfr) {
		fclose(fp);
		return -1;
	}

	// If no output buffer was supplied, then skip the actual data reading.
	if (out_mfc != NULL) {
		// Position at desired start frame and read actual MFC data
		mfc = calloc_2d<float>(n + start_pad + end_pad, cepsize);
		if (sf > 0)
			fseek_cur(fp, sf * cepsize * sizeof(float));
		n_float32 = n * cepsize;
		float_feat = mfc[start_pad];
		if (fread_retry(float_feat, sizeof(float), n_float32, fp) != n_float32) {
			free_2d(mfc);
			fclose(fp);
			return -1;
		}
		if (byterev) {
			for (i = 0; i < n_float32; i++)
				SWAP_FLOAT32(&float_feat[i]);
		}

		// Normalize
		cmn(mfc + start_pad, n, 1, 1);
		agc(mfc + start_pad, n, 1, 1);

		// Replicate start and end frames if necessary.
		for (int i = 0; i < start_pad; ++i)
			memcpy(mfc[i], mfc[start_pad], cepsize * sizeof(float));
		for (int i = 0; i < end_pad; ++i)
			memcpy(mfc[start_pad + n + i], mfc[start_pad + n - 1],
			cepsize * sizeof(float));

		*out_mfc = mfc;
	}

	fclose(fp);
	return n + start_pad + end_pad;
}

int feat::s2mfc2feat(const char *file, const char *dir, const char *cepext, int sf, int ef, float *** feat, int maxfr) {
	char *path;
	char *ps = "/";
	int win, nfr;
	int file_length, cepext_length, path_length = 0;
	float **mfc;

	if (cepsize <= 0) {
		return -1;
	}

	if (cepext == NULL)
		cepext = "";

	// Create mfc filename, combining file, dir and extension if necessary
	// First we decide about the path. If dir is defined, then use it. Otherwise assume the filename already contains the path.

	if (dir == NULL) {
		dir = "";
		ps = "";
		// This is not true but some 3rd party apps may parse the output explicitly checking for this line
	} else {
		// Do not forget the path separator!
		path_length += strlen(dir) + 1;
	}

	// Include cepext, if it's not already part of the filename.

	file_length = strlen(file);
	cepext_length = strlen(cepext);
	if ((file_length > cepext_length)
		&& (strcmp(file + file_length - cepext_length, cepext) == 0)) {
			cepext = "";
			cepext_length = 0;
	}

	// Do not forget the '\0'

	path_length += file_length + cepext_length + 1;
	path = (char*) ckd_calloc(path_length, sizeof(char));

	sprintf(path, "%s%s%s%s", dir, ps, file, cepext);

	win = window_size();
	// Pad maxfr with win, so we read enough raw feature data to calculate the requisite number of dynamic features.
	if (maxfr >= 0)
		maxfr += win * 2;

	if (feat != NULL) {
		// Read mfc file including window or padding if necessary.
		nfr = feat_s2mfc_read_norm_pad(fcb, path, win, sf, ef, &mfc, maxfr, cepsize);
		free(path);
		if (nfr < 0) {
			free_2d(mfc);
			return -1;
		}

		// Actually compute the features
		compute_utt(mfc, nfr, win, feat);
		free_2d(mfc);
	} else {
		// Just calculate the number of frames we would need.
		nfr = s2mfc_read_norm_pad(path, win, sf, ef, NULL, maxfr, cepsize);
		free(path);
		if (nfr < 0)
			return nfr;
	}

	return nfr - win * 2;
}
#endif