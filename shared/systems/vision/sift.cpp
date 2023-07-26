#include "base/vector.h"
#include "base/block.h"
#include "maths/sparse.h"
#include "extra/random.h"
#include "extra/filters.h"

using namespace iso;

typedef dynamic_matrix<double>	MatD;
typedef dynamic_vector<double>	VecD;
typedef double2					Vec2D;//soft_vector<2, double>	Vec2D;
typedef double3					Vec3D;//soft_vector<3, double>	Vec3D;
typedef block<float, 2>			Image;

typedef rng<simple_random>		Random;

// holds feature data relevant to detection
struct DetectionData {
	int		r;
	int		c;
	int		octv;
	int		intvl;
	double	subintvl;
	double	scl_octv;
};

#define SIFT_INTVLS            3		// default number of sampled intervals per octave
#define SIFT_SIGMA             1.6		// default sigma for initial gaussian smoothing
#define SIFT_CONTR_THR         0.04		// default threshold on keypoint contrast |D(x)|
#define SIFT_CURV_THR          10		// default threshold on keypoint ratio of principle curvatures
#define SIFT_IMG_DBL           1		// double image size before pyramid construction?
#define SIFT_DESCR_WIDTH       4		// default width of descriptor histogram array
#define SIFT_DESCR_HIST_BINS   8		// default number of bins per histogram in descriptor array
#define SIFT_INIT_SIGMA        0.5		// assumed gaussian blur for input image
#define SIFT_IMG_BORDER        5		// width of border in which to ignore keypoints
#define SIFT_MAX_INTERP_STEPS  5		// maximum steps of keypoint interpolation before failure
#define SIFT_ORI_HIST_BINS     36		// default number of bins in histogram for orientation assignment
#define SIFT_ORI_SIG_FCTR      1.5		// determines gaussian sigma for orientation assignment
#define SIFT_ORI_RADIUS        3.0 * SIFT_ORI_SIG_FCTR	// determines the radius of the region used in orientation assignment
#define SIFT_ORI_SMOOTH_PASSES 2		// number of passes of orientation histogram smoothing
#define SIFT_ORI_PEAK_RATIO    0.8		// orientation magnitude relative to max that results in new feature
#define SIFT_DESCR_SCL_FCTR    3.0		// determines the size of a single descriptor orientation histogram
#define SIFT_DESCR_MAG_THR     0.2		// threshold on magnitude of elements of descriptor vector
#define SIFT_INT_DESCR_FCTR    512.0	// factor used to convert floating-point descriptor to unsigned char

enum FeatureMatchType {
	FEATURE_FWD_MATCH,
	FEATURE_BCK_MATCH,
	FEATURE_MDL_MATCH,
};

struct Feature {
	enum {FEATURE_MAX_D = 128};

	double			x, y;					// coords
	double			scl;					// scale of a Lowe-style feature
	double			ori;					// orientation of a Lowe-style feature
	int				d;						// descriptor length
	double			descr[FEATURE_MAX_D];	// descriptor

	Feature*		fwd_match;				// matching feature from forward image
	Feature*		bck_match;				// matching feature from backmward image
	Feature*		mdl_match;				// matching feature from model

	Vec2D			img_pt;					// location in image
	Vec2D			mdl_pt;					// location in model
	DetectionData	data;
	bool			sampled;

	Feature() {
		clear(*this);
	}

	void normalize() {
		double	len_sq = 0;
		for (int i = 0; i < d; i++)
			len_sq += square(descr[i]);

		double len_inv = rsqrt(len_sq);
		for (int i = 0; i < d; i++)
			descr[i] *= len_inv;
	}

	Feature* get_match(FeatureMatchType mtype) {
		return	mtype == FEATURE_MDL_MATCH ? mdl_match
			:	mtype == FEATURE_BCK_MATCH ? bck_match
			:	mtype == FEATURE_FWD_MATCH ? fwd_match
			:	0;
	}
};

typedef MatD (*ransac_xform_fn)(Vec2D* pts, Vec2D* mpts, int n);
typedef double (*ransac_err_fn)(Vec2D pt, Vec2D mpt, MatD &T);

#define RANSAC_ERR_TOL			3		// RANSAC error tolerance in pixels
#define RANSAC_INLIER_FRAC_EST	0.25	// pessimistic estimate of fraction of inliers for RANSAC
#define RANSAC_PROB_BAD_SUPP	0.10	// estimate of the probability that a correspondence supports a bad model

/*
  For a given model and error function, finds a consensus from a set of  feature correspondences.

  @param features	set of pointers to features; every feature is assumed to have a match of type mtype
  @param mtype		determines the match field of each feature against which to measure error;
	if this is FEATURE_MDL_MATCH, correspondences are assumed to be between the feature's img_pt field and the match's mdl_pt field;
	otherwise matches are assumed to be between img_pt and img_pt
  @param M			model for which a consensus set is being found
  @param err_fn		error function used to measure distance from M
  @param err_tol	correspondences within this distance of M are added to the consensus set
  @param consensus	output as an array of pointers to features in the consensus set

  @return Returns the number of points in the consensus set
*/
dynamic_array<Feature*> find_consensus(dynamic_array<Feature*> &features, FeatureMatchType mtype, MatD &M, ransac_err_fn err_fn, double err_tol) {
	dynamic_array<Feature*>	consensus;

	if (mtype == FEATURE_MDL_MATCH) {
		for (int i = 0; i < features.size32(); i++) {
			Feature	*match = features[i]->get_match(mtype);
			if (!match)
				break;//feature does not have match of mtype
			Vec2D	pt	= features[i]->img_pt;
			Vec2D	mpt	= match->mdl_pt;
			double	err	= err_fn(pt, mpt, M);
			if (err <= err_tol)
				consensus.push_back(features[i]);
		}

	} else {
		for (auto i : features) {
			Feature	*match = i->get_match(mtype);
			if (!match)
				break;//feature does not have match of mtype

			if (err_fn(i->img_pt, match->img_pt, M) <= err_tol)
				consensus.push_back(i);
		}
	}
	return consensus;
}

/*
  Extracts raw point correspondence locations from a set of features

  @param features array of features from which to extract points and match points; each of these is assumed to have a match of type mtype
  @param mtype match type; if FEATURE_MDL_MATCH correspondences are assumed to be between each feature's img_pt field and it's match's mdl_pt field, otherwise, correspondences are assumed to be between img_pt and img_pt
  @param pts output as an array of raw point locations from features
  @param mpts output as an array of raw point locations from features' matches
*/
static void extract_corresp_pts(dynamic_array<Feature*> &features, FeatureMatchType mtype, dynamic_array<Vec2D> &pts, dynamic_array<Vec2D> &mpts) {
	int		n	= features.size32();
	Vec2D	*p	= pts.resize(n);
	Vec2D	*m	= mpts.resize(n);

	for (auto i : features) {
		Feature	*match = i->get_match(mtype);
		ISO_ASSERT(match);// feature does not have match
		*p++	= i->img_pt;
		*m++	= match->img_pt;
	}
}

static inline double log_factorial(int i, int n) {
	double f = 0;
	i = max(i, 2);
	while (i < n)
		f += log(double(i++));
	return f;
}

static inline double log_factorial(int n) {
	double f = 0;
	for (int i = 2; i <= n; i++)
		f += log(double(i));
	return f;
}

/*
  Calculates the minimum number of inliers as a function of the number of putative correspondences.
  Based on equation (7) in  Chum, O. and Matas, J.  Matching with PROSAC -- Progressive Sample Consensus. In <EM>Conference on Computer Vision and Pattern Recognition (CVPR)</EM>, (2005), pp. 220--226.

  @param n number of putative correspondences
  @param m min number of correspondences to compute the model in question
  @param p_badsupp prob. that a bad model is supported by a correspondence
  @param p_badxform desired prob. that the final transformation returned is bad

  @return Returns the minimum number of inliers required to guarantee, based on p_badsupp, that the probability that the final transformation returned by RANSAC is less than p_badxform
*/
static int calc_min_inliers(int n, int m, double p_badsupp, double p_badxform) {
	int j;
	for (j = m + 1; j <= n; j++) {
		double	sum = 0;
		for (int i = j; i <= n; i++) {
			double	pi = (i - m) * log(p_badsupp) + (n - i + m) * log(1 - p_badsupp)
				+ log_factorial(n - m) - log_factorial(i - m) - log_factorial(n - i);		// equivalent to log(n - m C i - m)
			sum += exp(pi);
		}
		if (sum < p_badxform)
			break;
	}
	return j;
}


/*
  Draws a RANSAC sample from a set of features.

  @param features array of pointers to features from which to sample
  @param m size of the sample

  @return Returns an array of pointers to the sampled features; the sampled	field of each sampled feature's RANSAC is set to 1
*/
static dynamic_array<Feature*> draw_ransac_sample(const dynamic_array<Feature*> &features, int m, Random &random) {
	for (auto i : features)
		i->sampled = false;

	dynamic_array<Feature*> sample(m);
	for (int i = 0; i < m; i++) {
		Feature	*feat;
		do
			feat	= random.fromc(features);
		while (feat->sampled);

		sample[i]		= feat;
		feat->sampled	= true;
	}

	return sample;
}

/*
  Calculates a best-fit image transform from image feature correspondences using RANSAC.

  For more information refer to:
  Fischler, M. A. and Bolles, R. C.  Random sample consensus: a paradigm for model fitting with applications to image analysis and automated cartography.
  <EM>Communications of the ACM, 24</EM>, 6 (1981), pp. 381--395.

  @param features an array of features; only features with a non-NULL match	of type mtype are used in homography computation
  @param n number of features in feat
  @param mtype determines which of each feature's match fields to use for model computation; should be one of FEATURE_FWD_MATCH, FEATURE_BCK_MATCH, or FEATURE_MDL_MATCH;
	if this is FEATURE_MDL_MATCH, correspondences are assumed to be between a feature's img_pt field and its match's mdl_pt field,
	otherwise correspondences are assumed to be between the the feature's img_pt field and its match's img_pt field
  @param xform_fn pointer to the function used to compute the desired transformation from feature correspondences
  @param m minimum number of correspondences necessary to instantiate the model computed by xform_fn
  @param p_badxform desired probability that the final transformation returned by RANSAC is corrupted by outliers (i.e. the probability that no samples of all inliers were drawn)
  @param err_fn pointer to the function used to compute a measure of error between putative correspondences and a computed model
  @param err_tol correspondences within this distance of a computed model are considered as inliers
  @param inliers if not NULL, output as an array of pointers to the final set of inliers
  @param n_in if not NULL and \a inliers is not NULL, output as the final number of inliers

  @return Returns a transformation matrix computed using RANSAC or NULL on error or if an acceptable transform could not be computed.
*/
MatD ransac_xform(dynamic_array<Feature> &features, FeatureMatchType mtype, ransac_xform_fn xform_fn, int m, double p_badxform, ransac_err_fn err_fn, double err_tol, dynamic_array<Feature*> *inliers, Random &random) {
	dynamic_array<Feature*> consensus_max, sample;
	dynamic_array<Vec2D>	pts, mpts;
	MatD	M;

	dynamic_array<Feature*> matched(features.size());
	for (auto &i : features) {
		if (i.get_match(mtype)) {
			matched.push_back(&i);
			matched.back()->sampled = false;
		}
	}

	if (matched.size() < m) {
		// not enough matches to compute xform, %s line %d\n", __FILE__, __LINE__);
		return M;
	}

//	srandom(time(NULL));

	int		in_min	= calc_min_inliers(matched.size32(), m, RANSAC_PROB_BAD_SUPP, p_badxform);
	int		k		= 0;
	double	in_frac = RANSAC_INLIER_FRAC_EST;
	double	p		= pow(1 - pow(in_frac, m), k);
	while (p > p_badxform) {
		sample = draw_ransac_sample(matched, m, random);
		extract_corresp_pts(sample, mtype, pts, mpts);
		if (MatD M = xform_fn(pts, mpts, m)) {
			auto	consensus = find_consensus(matched, mtype, M, err_fn, err_tol);
			if (consensus.size() > consensus_max.size()) {
				consensus_max = move(consensus);
				in_frac = (double)consensus_max.size() / matched.size();
			}
		}
		p = pow(1 - pow(in_frac, m), ++k);
	}

	// calculate final transform based on best consensus set
	if (consensus_max.size() >= in_min) {
		extract_corresp_pts(consensus_max, mtype, pts, mpts);
		M				= xform_fn(pts, mpts, consensus_max.size32());
		auto consensus	= find_consensus(matched, mtype, M, err_fn, err_tol);
		extract_corresp_pts(consensus, mtype, pts, mpts);
		M = xform_fn(pts, mpts, consensus.size32());
		if (inliers) {
			swap(*inliers, consensus);
			consensus.clear();
		}

	} else if (consensus_max) {
		if (inliers)
			inliers->reset();
	}

	return M;
}

#if 0
/*
  Calculates a planar homography from point correspondeces using the direct linear transform.  Intended for use as a ransac_xform_fn.

  @param pts array of points
  @param mpts array of corresponding points; each pts[i], i=0..n-1, corresponds to mpts[i]
  @param n number of points in both pts and mpts; must be at least 4

  @return Returns the 3x3 planar homography matrix that transforms points in pts to their corresponding points in mpts or NULL if fewer than 4 correspondences were provided
*/
MatD dlt_homog(Vec2D* pts, Vec2D* mpts, int n) {
	MatD	h, v9;
	double _h[9];

	if (n < 4)
		return MatD();

	// set up matrices so we can unstack homography into h; Ah = 0
	MatD	A(2 * n, 9);
	A.clear();
	for (int i = 0; i < n; i++) {
		A[2 * i][3]		= -pts[i].x;
		A[2 * i][4]		= -pts[i].y;
		A[2 * i][5]		= -1.0;
		A[2 * i][6]		= mpts[i].y * pts[i].x;
		A[2 * i][7]		= mpts[i].y * pts[i].y;
		A[2 * i][8]		= mpts[i].y;
		A[2 * i + 1][0] = pts[i].x;
		A[2 * i + 1][1] = pts[i].y;
		A[2 * i + 1][2] = 1.0;
		A[2 * i + 1][6] = -mpts[i].x * pts[i].x;
		A[2 * i + 1][7] = -mpts[i].x * pts[i].y;
		A[2 * i + 1][8] = -mpts[i].x;
	}
	MatD	D(9, 9);
	MatD	VT(9, 9);
	cvSVD(A, D, NULL, VT, CV_SVD_MODIFY_A + CV_SVD_V_T);
	v9 = cvMat(1, 9, CV_64FC1, NULL);
	cvGetRow(VT, &v9, 8);
	h = cvMat(1, 9, CV_64FC1, _h);
	cvCopy(&v9, &h, NULL);
	h = cvMat(3, 3, CV_64FC1, _h);
	MatD	H(3, 3);
	cvConvert(&h, H);

	return H;
}
#endif

/*
  Calculates a least-squares planar homography from point correspondeces.

  @param pts array of points
  @param mpts array of corresponding points; each pts[i], i=1..n, corresponds to mpts[i]
  @param n number of points in both pts and mpts; must be at least 4

  @return Returns the 3 x 3 least-squares planar homography matrix that transforms points in pts to their corresponding points in mpts or NULL if fewer than 4 correspondences were provided
*/
MatD lsq_homog(Vec2D* pts, Vec2D* mpts, int n) {
	if (n < 4)
		return MatD();//Warning: too few points in lsq_homog(), %s line %d\n",

	// set up matrices so we can unstack homography into X; AX = B
	MatD	A(2 * n, 8);
	VecD	B(2 * n);
	A = zero;
	for (int i = 0; i < n; i++) {
		A[i][0]			= pts[i].x;
		A[i + n][3]		= pts[i].x;
		A[i][1]			= pts[i].y;
		A[i + n][4]		= pts[i].y;
		A[i][2]			= 1;
		A[i + n][5]		= 1;
		A[i][6]			= -pts[i].x * mpts[i].x;
		A[i][7]			= -pts[i].y * mpts[i].x;
		A[i + n][6]		= -pts[i].x * mpts[i].y;
		A[i + n][7]		= -pts[i].y * mpts[i].y;
		B[i]			= mpts[i].x;
		B[i + n]		= mpts[i].y;
	}

	MatD	X(3, 3);
	//vector_view<double>	V(X, 8, 1);
	//V	= svd_solve(A, B);
	X[2][2] = 1;
	return X;
}

/*
  Calculates the transfer error between a point and its correspondence for a given homography, i.e. for a point x, it's correspondence x', and homography H, computes d(x', Hx)^2.

  @param pt a point
  @param mpt pt's correspondence
  @param H a homography matrix

  @return Returns the transfer error between pt and mpt given H
*/
double homog_xfer_err(const Vec2D &pt, const Vec2D &mpt, const MatD &H) {
	Vec3D	hpt	= concat(pt, one);
	auto	xpt = H * hpt;
	Vec2D	pt2{xpt[0] / xpt[2], xpt[1] / xpt[2]};
	return sqrt(len2(pt2 - mpt));
}


//void test_ransac() {
//	Image* xformed;
//	MatD H = ransac_xform(feat1, n1, FEATURE_FWD_MATCH, lsq_homog, 4, 0.01, homog_xfer_err, 3.0, NULL, NULL);
//}

/*
  Converts an image to 8-bit grayscale and Gaussian-smooths it.  The image is optionally doubled in size prior to smoothing.

  @param img input image
  @param img_dbl if true, image is doubled in size prior to smoothing
  @param sigma total std of Gaussian smoothing
*/
static Image create_init_img(Image &img, bool img_dbl, double sigma) {
	if (img_dbl) {
		double	sig_diff = sqrt(sigma * sigma - SIFT_INIT_SIGMA * SIFT_INIT_SIGMA * 4);
		auto_block<float, 2>	dbl = make_auto_block<float>(img.size<1>(), img.size<2>());
		resample(dbl.get(), img);

		float	coeffs[16];
		gaussian(coeffs, 16, sig_diff, 1);

		auto_block<float, 2>	dbl2 = make_auto_block<float>(img.size<1>(), img.size<2>());
		hfilter(dbl2, dbl, coeffs, 16, 8);
		vfilter(dbl, dbl2, coeffs, 16, 8);
		return move(dbl);
	}

	double	sig_diff = sqrt(sigma * sigma - SIFT_INIT_SIGMA * SIFT_INIT_SIGMA);
	float	coeffs[16];
	gaussian(coeffs, 16, sig_diff, 1);

	auto_block<float, 2>	img2 = make_auto_block<float>(img.size<1>(), img.size<2>());
	hfilter(img2, img, coeffs, 16, 8);
	vfilter(img, img2, coeffs, 16, 8);
	return img;
}

/*
  Determines whether a pixel is a scale-space extremum by comparing it to it's 3x3x3 pixel neighborhood.

  @param dog_pyr DoG scale space pyramid
  @param octv pixel's scale space octave
  @param intvl pixel's within-octave interval
  @param r pixel's image row
  @param c pixel's image col

  @return Returns 1 if the specified pixel is an extremum (max or min) among
	it's 3x3x3 pixel neighborhood.
*/
static bool is_extremum(dynamic_array<dynamic_array<Image> > &dog_pyr, int octv, int intvl, int r, int c) {
	double val = dog_pyr[octv][intvl][r][c];

	// check for maximum
	if (val > 0) {
		for (int i = -1; i <= 1; i++)
			for (int j = -1; j <= 1; j++)
				for (int k = -1; k <= 1; k++)
					if (val < dog_pyr[octv][intvl + i][r + j][c + k])
						return false;
	}

	// check for minimum
	else {
		for (int i = -1; i <= 1; i++)
			for (int j = -1; j <= 1; j++)
				for (int k = -1; k <= 1; k++)
					if (val > dog_pyr[octv][intvl + i][r + j][c + k])
						return false;
	}

	return true;
}


/*
  Computes the partial derivatives in x, y, and scale of a pixel in the DoG  scale space pyramid.

  @param dog_pyr DoG scale space pyramid
  @param octv pixel's octave in dog_pyr
  @param intvl pixel's interval in octv
  @param r pixel's image row
  @param c pixel's image col

  @return Returns the vector of partial derivatives for pixel I
	{ dI/dx, dI/dy, dI/ds }^T
*/
static Vec3D deriv_3D(dynamic_array<dynamic_array<Image> > &dog_pyr, int octv, int intvl, int r, int c) {
	return {
		(dog_pyr[octv][intvl][r][c + 1] - dog_pyr[octv][intvl][r][c - 1]) / 2,
		(dog_pyr[octv][intvl][r + 1][c] - dog_pyr[octv][intvl][r - 1][c]) / 2,
		(dog_pyr[octv][intvl + 1][r][c] - dog_pyr[octv][intvl - 1][r][c]) / 2
	};
}
/*
  Calculates interpolated pixel contrast.  Based on Eqn. (3) in Lowe's paper.

  @param dog_pyr	difference of Gaussians scale space pyramid
  @param octv		octave of scale space
  @param intvl		within-octave interval
  @param r			pixel row
  @param c			pixel column
  @param xi			interpolated subpixel increment to interval
  @param xr			interpolated subpixel increment to row
  @param xc			interpolated subpixel increment to col

  @param Returns interpolated contrast.
*/
static double interp_contr(dynamic_array<dynamic_array<Image> > &dog_pyr, int octv, int intvl, int r, int c, double xi, double xr, double xc) {
	Vec3D	x		= { xc, xr, xi };
	Vec3D	dD		= deriv_3D(dog_pyr, octv, intvl, r, c);
	double	t		= dot(dD, x);
	return dog_pyr[octv][intvl][r][c] + t * 0.5;
}

/*
  Computes the 3D Hessian matrix for a pixel in the DoG scale space pyramid.

  @param dog_pyr DoG scale space pyramid
  @param octv pixel's octave in dog_pyr
  @param intvl pixel's interval in octv
  @param r pixel's image row
  @param c pixel's image col

  @return Returns the Hessian matrix (below) for pixel I

  / Ixx  Ixy  Ixs \
  | Ixy  Iyy  Iys |
  \ Ixs  Iys  Iss /
*/
static MatD hessian_3D(dynamic_array<dynamic_array<Image> > &dog_pyr, int octv, int intvl, int r, int c) {
	double	v	= dog_pyr[octv][intvl][r][c];
	double	dxx	= (dog_pyr[octv][intvl][r][c + 1]		+ dog_pyr[octv][intvl][r][c - 1] - 2 * v);
	double	dyy	= (dog_pyr[octv][intvl][r + 1][c]		+ dog_pyr[octv][intvl][r - 1][c] - 2 * v);
	double	dss	= (dog_pyr[octv][intvl + 1][r][c]		+ dog_pyr[octv][intvl - 1][r][c] - 2 * v);
	double	dxy	= (dog_pyr[octv][intvl][r + 1][c + 1]	- dog_pyr[octv][intvl][r + 1][c - 1] - dog_pyr[octv][intvl][r - 1][c + 1] + dog_pyr[octv][intvl][r - 1][c - 1]) / 4.0;
	double	dxs	= (dog_pyr[octv][intvl + 1][r][c + 1]	- dog_pyr[octv][intvl + 1][r][c - 1] - dog_pyr[octv][intvl - 1][r][c + 1] + dog_pyr[octv][intvl - 1][r][c - 1]) / 4.0;
	double	dys	= (dog_pyr[octv][intvl + 1][r + 1][c]	- dog_pyr[octv][intvl + 1][r - 1][c] - dog_pyr[octv][intvl - 1][r + 1][c] + dog_pyr[octv][intvl - 1][r - 1][c]) / 4.0;

	MatD	H(3, 3);
	H[0][0]	= dxx;
	H[0][1]	= dxy;
	H[0][2]	= dxs;
	H[1][0]	= dxy;
	H[1][1]	= dyy;
	H[1][2]	= dys;
	H[2][0]	= dxs;
	H[2][1]	= dys;
	H[2][2]	= dss;
	return H;
}
/*
  Performs one step of extremum interpolation.
  Based on Eqn. (3) in Lowe's paper.

  @param dog_pyr difference of Gaussians scale space pyramid
  @param octv octave of scale space
  @param intvl interval being interpolated
  @param r row being interpolated
  @param c column being interpolated
  @param xi output as interpolated subpixel increment to interval
  @param xr output as interpolated subpixel increment to row
  @param xc output as interpolated subpixel increment to col
*/
static void interp_step(dynamic_array<dynamic_array<Image> > &dog_pyr, int octv, int intvl, int r, int c, double* xi, double* xr, double* xc) {
	Vec3D	dD		= deriv_3D(dog_pyr, octv, intvl, r, c);
	MatD	H		= hessian_3D(dog_pyr, octv, intvl, r, c);
	MatD	H_inv	= svd_inverse(H);
	VecD	X		= -(H_inv * dD);

	*xi = X[2];
	*xr = X[1];
	*xc = X[0];
}
/*
  Interpolates a scale-space extremum's location and scale to subpixel accuracy to form an image feature.  Rejects features with low contrast.
  Based on Section 4 of Lowe's paper.

  @param dog_pyr	DoG scale space pyramid
  @param octv		feature's octave of scale space
  @param intvl		feature's within-octave interval
  @param r			feature's image row
  @param c			feature's image column
  @param intvls		total intervals per octave
  @param contr_thr	threshold on feature contrast

  @return Returns the feature resulting from interpolation of the given parameters or NULL if the given location could not be interpolated or if contrast at the interpolated loation was too low.
	If a feature is returned, its scale, orientation, and descriptor are yet to be determined.
*/
static bool interp_extremum(dynamic_array<dynamic_array<Image> > &dog_pyr, int octv, int intvl, int r, int c, int intvls, double contr_thr, Feature &feat) {
	double xi, xr, xc, contr;
	int i = 0;

	while (i < SIFT_MAX_INTERP_STEPS) {
		interp_step(dog_pyr, octv, intvl, r, c, &xi, &xr, &xc);
		if (abs(xi) < 0.5  &&  abs(xr) < 0.5  &&  abs(xc) < 0.5)
			break;

		c		+= round(xc);
		r		+= round(xr);
		intvl	+= round(xi);

		if (intvl < 1
		||	intvl > intvls
		||	c < SIFT_IMG_BORDER
		||	r < SIFT_IMG_BORDER
		||	c >= dog_pyr[octv][0].size<1>() - SIFT_IMG_BORDER
		||	r >= dog_pyr[octv][0].size<2>() - SIFT_IMG_BORDER
		)
			return false;

		i++;
	}

	// ensure convergence of interpolation
	if (i >= SIFT_MAX_INTERP_STEPS)
		return false;

	contr = interp_contr(dog_pyr, octv, intvl, r, c, xi, xr, xc);
	if (abs(contr) < contr_thr / intvls)
		return false;

	feat.img_pt.x	= feat.x = (c + xc) * pow(2.0, octv);
	feat.img_pt.y	= feat.y = (r + xr) * pow(2.0, octv);
	feat.data.r	= r;
	feat.data.c	= c;
	feat.data.octv = octv;
	feat.data.intvl = intvl;
	feat.data.subintvl = xi;

	return true;
}

/*
  Determines whether a feature is too edge like to be stable by computing the ratio of principal curvatures at that feature.
  Based on Section 4.1 of Lowe's paper.

  @param dog_img image from the DoG pyramid in which feature was detected
  @param r feature row
  @param c feature col
  @param curv_thr high threshold on ratio of principal curvatures

  @return Returns 0 if the feature at (r,c) in dog_img is sufficiently corner-like or 1 otherwise.
*/
static bool is_too_edge_like(Image &dog_img, int r, int c, int curv_thr) {
	// principal curvatures are computed using the trace and det of Hessian
	double	d	= dog_img[r][c];
	double	dxx	= dog_img[r][c + 1] + dog_img[r][c - 1] - 2 * d;
	double	dyy	= dog_img[r + 1][c] + dog_img[r - 1][c] - 2 * d;
	double	dxy	= (dog_img[r + 1][c + 1] - dog_img[r + 1][c - 1] - dog_img[r - 1][c + 1] + dog_img[r - 1][c - 1]) / 4.0;
	double	tr	= dxx + dyy;
	double	det	= dxx * dyy - dxy * dxy;

	// negative determinant -> curvatures have different signs; reject feature
	return det <= 0 || tr * tr / det >= square(curv_thr + 1) / curv_thr;
}

/*
  Calculates the gradient magnitude and orientation at a given pixel.

  @param img image
  @param r pixel row
  @param c pixel col
  @param mag output as gradient magnitude at pixel (r,c)
  @param ori output as gradient orientation at pixel (r,c)

  @return Returns 1 if the specified pixel is a valid one and sets mag and ori accordingly; otherwise returns 0
*/
static bool calc_grad_mag_ori(Image &img, int r, int c, double* mag, double* ori) {
	if (r > 0 && r < img.size<2>() - 1 && c > 0 && c < img.size<1>() - 1) {
		double dx = img[r][c + 1] - img[r][c - 1];
		double dy = img[r - 1][c] - img[r + 1][c];
		*mag = sqrt(square(dx) + square(dy));
		*ori = atan2(dy, dx);
		return true;
	}
	return false;
}
/*
  Computes a gradient orientation histogram at a specified pixel.

  @param img image
  @param r pixel row
  @param c pixel col
  @param n number of histogram bins
  @param rad radius of region over which histogram is computed
  @param sigma std for Gaussian weighting of histogram entries

  @return Returns an n-element array containing an orientation histogram representing orientations between 0 and 2 PI.
*/
static double* ori_hist(Image &img, int r, int c, int n, int rad, double sigma) {
	double* hist = new double[n];
	double	exp_denom = 2 * sigma * sigma;
	for (int i = -rad; i <= rad; i++) {
		for (int j = -rad; j <= rad; j++) {
			double mag, ori;
			if (calc_grad_mag_ori(img, r + i, c + j, &mag, &ori)) {
				double	w = exp(-(i*i + j*j) / exp_denom);
				int bin = round(n * (ori +  pi) / (two * pi));
				bin = (bin < n) ? bin : 0;
				hist[bin] += w * mag;
			}
		}
	}
	return hist;
}

/*
  Gaussian smooths an orientation histogram.

  @param hist an orientation histogram
  @param n number of bins
*/
static void smooth_ori_hist(double* hist, int n) {
	double h0	= hist[0];
	double prev = hist[n - 1];
	for (int i = 0; i < n; i++) {
		double tmp = hist[i];
		hist[i] = 0.25 * prev + 0.5 * hist[i] + 0.25 * ((i + 1 == n) ? h0 : hist[i + 1]);
		prev = tmp;
	}
}

/*
  Finds the magnitude of the dominant orientation in a histogram

  @param hist an orientation histogram
  @param n number of bins

  @return Returns the value of the largest bin in hist
*/
static double dominant_ori(double* hist, int n) {
	double omax = hist[0];
	int	maxbin = 0;
	for (int i = 1; i < n; i++)
		if (hist[i] > omax) {
			omax = hist[i];
			maxbin = i;
		}
	return omax;
}

/*
  Interpolates an entry into the array of orientation histograms that form the feature descriptor.

  @param hist 2D array of orientation histograms
  @param rbin sub-bin row coordinate of entry
  @param cbin sub-bin column coordinate of entry
  @param obin sub-bin orientation coordinate of entry
  @param mag size of entry
  @param d width of 2D array of orientation histograms
  @param n number of bins per orientation histogram
*/
static void interp_hist_entry(auto_block<double,3> &hist, double rbin, double cbin, double obin, double mag, int d, int n) {
	int		r0	= floor(rbin);
	int		c0	= floor(cbin);
	int		o0	= floor(obin);
	double	d_r	= rbin - r0;
	double	d_c	= cbin - c0;
	double	d_o	= obin - o0;

	//  The entry is distributed into up to 8 bins.
	//  Each entry into a bin is multiplied by a weight of 1 - d for each dimension, where d is the distance from the center value of the bin measured in bin units

	for (int r = 0; r <= 1; r++) {
		int rb = r0 + r;
		if (rb >= 0 && rb < d) {
			double v_r = mag * ((r == 0) ? 1.0 - d_r : d_r);
			auto	row = hist[rb];
			for (int c = 0; c <= 1; c++) {
				int cb = c0 + c;
				if (cb >= 0 && cb < d) {
					int v_c = v_r * ((c == 0) ? 1.0 - d_c : d_c);
					double *h = row[cb].begin();
					for (int o = 0; o <= 1; o++) {
						int ob = (o0 + o) % n;
						double v_o = v_c * ((o == 0) ? 1.0 - d_o : d_o);
						h[ob] += v_o;
					}
				}
			}
		}
	}
}

/*
  Computes the 2D array of orientation histograms that form the feature descriptor.
  Based on Section 6.1 of Lowe's paper.

  @param img image used in descriptor computation
  @param r row coord of center of orientation histogram array
  @param c column coord of center of orientation histogram array
  @param ori canonical orientation of feature whose descr is being computed
  @param scl scale relative to img of feature whose descr is being computed
  @param d width of 2d array of orientation histograms
  @param n bins per orientation histogram

  @return Returns a d x d array of n-bin orientation histograms.
*/
static auto_block<double,3> descr_hist(Image &img, int r, int c, double ori, double scl, int d, int n) {
	auto_block<double,3>	hist = make_auto_block<double>(n, d, d);
	fill(hist, 0);

	double cos_t		= cos(ori), sin_t = sin(ori);
	double bins_per_rad = n / (two * pi);
	double exp_denom = d * d * 0.5;
	double hist_width = SIFT_DESCR_SCL_FCTR * scl;
	int radius = hist_width * sqrt(2) * (d + 1.0) * 0.5 + 0.5;
	for (int i = -radius; i <= radius; i++)
		for (int j = -radius; j <= radius; j++) {
			//Calculate sample's histogram array coords rotated relative to ori.
			//Subtract 0.5 so samples that fall e.g. in the center of row 1 (i.e. r_rot = 1.5) have full weight placed in row 1 after interpolation.
			double c_rot = (j * cos_t - i * sin_t) / hist_width;
			double r_rot = (j * sin_t + i * cos_t) / hist_width;
			double rbin = r_rot + d / 2 - 0.5;
			double cbin = c_rot + d / 2 - 0.5;

			if (rbin > -1.0  &&  rbin < d  &&  cbin > -1.0  &&  cbin < d) {
				double grad_mag, grad_ori;
				if (calc_grad_mag_ori(img, r + i, c + j, &grad_mag, &grad_ori)) {
					grad_ori -= ori;
					while (grad_ori < 0.0)
						grad_ori += pi * two;
					while (grad_ori >= pi * two)
						grad_ori -= pi * 2;

					double obin = grad_ori * bins_per_rad;
					double w = exp(-(c_rot * c_rot + r_rot * r_rot) / exp_denom);
					interp_hist_entry(hist, rbin, cbin, obin, grad_mag * w, d, n);
				}
			}
		}

	return hist;
}

/*
  Converts the 2D array of orientation histograms into a feature's descriptor vector.

  @param hist 2D array of orientation histograms
  @param d width of hist
  @param n bins per histogram
  @param feat feature into which to store descriptor
*/
static void hist_to_descr(auto_block<double,3> &hist, int d, int n, Feature* feat) {
	int k = 0;
	for (int r = 0; r < d; r++)
		for (int c = 0; c < d; c++)
			for (int o = 0; o < n; o++)
				feat->descr[k++] = hist[r][c][o];

	feat->d = k;
	feat->normalize();
	for (int i = 0; i < k; i++)
		if (feat->descr[i] > SIFT_DESCR_MAG_THR)
			feat->descr[i] = SIFT_DESCR_MAG_THR;
	feat->normalize();

	// convert floating-point descriptor to integer valued descriptor
	for (int i = 0; i < k; i++)
		feat->descr[i] = min(255, SIFT_INT_DESCR_FCTR * feat->descr[i]);
}


/*
  Interpolates a histogram peak from left, center, and right values
*/
#define interp_hist_peak(l, c, r) (0.5 * ((l)-(r)) / ((l) - 2.0*(c) + (r)))

/**
   Finds SIFT features in an image using user-specified parameter values.  All detected features are stored in the array pointed to by \a feat.

   @param img		the image in which to detect features
   @param feat		a pointer to an array in which to store detected features
   @param intvls	the number of intervals sampled per octave of scale space
   @param sigma		the amount of Gaussian smoothing applied to each image level before building the scale space representation for an octave
   @param cont_thr	a threshold on the value of the scale space function
	 \f$\left|D(\hat{x})\right|\f$, where \f$\hat{x}\f$ is a vector specifying feature location and scale, used to reject unstable features;  assumes pixel values in the range [0, 1]
   @param curv_thr	threshold on a feature's ratio of principle curvatures used to reject features that are too edge-like
   @param img_dbl	should be 1 if image doubling prior to scale space construction is desired or 0 if not
   @param descr_width the width, \f$n\f$, of the \f$n \times n\f$ array of orientation histograms used to compute a feature's descriptor
   @param descr_hist_bins the number of orientations in each of the histograms in the array used to compute a feature's descriptor

   @return Returns the number of keypoints stored in \a feat or -1 on failure
   @see sift_keypoints()
*/
dynamic_array<Feature> _sift_features(Image &img, int intvls, double sigma, double contr_thr, int curv_thr, bool img_dbl, int descr_width, int descr_hist_bins) {
	// build scale space pyramid; smallest dimension of top level is ~4 pixels
	Image	base	= create_init_img(img, img_dbl, sigma);
	int		octvs	= log2(min(base.size<1>(), base.size<2>())) - 2;

	// build Gaussian scale space pyramid from an image as an octvs x (intvls + 3) array
	const int	_intvls = intvls;
	double		*sig	= alloc_auto(double, _intvls + 3);

	dynamic_array<dynamic_array<Image> > gauss_pyr(octvs);
	for (int i = 0; i < octvs; i++)
		gauss_pyr[i].resize(intvls + 3);

	/*
	  precompute Gaussian sigmas using the following formula:
	  \sigma_{total}^2 = \sigma_{i}^2 + \sigma_{i-1}^2
	  sig[i] is the incremental sigma value needed to compute  the actual sigma of level i. Keeping track of incremental  sigmas vs. total sigmas keeps the gaussian kernel small.
	*/
	double	k = pow(2.0, 1.0 / intvls);
	sig[0] = sigma;
	sig[1] = sigma * sqrt(k*k - 1);
	for (int i = 2; i < intvls + 3; i++)
		sig[i] = sig[i - 1] * k;

	for (int o = 0; o < octvs; o++) {
		for (int i = 0; i < intvls + 3; i++) {
			if (o == 0 && i == 0) {
				gauss_pyr[o][i] = base;

			} else if (i == 0) {
				// base of new octvave is halved image from end of previous octave
				gauss_pyr[o][i] = downsample2x2(gauss_pyr[o - 1][intvls]);

			} else {
				// blur the current octave's last image to create the next one
				gauss_pyr[o][i] = make_auto_block_using(gauss_pyr[o][i - 1]);

				float	coeffs[16];
				gaussian(coeffs, 16, sig[i], 1);
				auto_block<float, 2>	temp = make_auto_block_using(gauss_pyr[o][i - 1]);
				hfilter(temp, gauss_pyr[o][i - 1], coeffs, 16, 8);
				vfilter(gauss_pyr[o][i], temp, coeffs, 16, 8);
			}
		}
	}

	// builds difference of Gaussians scale space pyramid by subtracting adjacent intervals of a Gaussian pyramid as an octvs x (intvls + 2) array
	dynamic_array<dynamic_array<Image> > dog_pyr(octvs);
	for (int i = 0; i < octvs; i++)
		dog_pyr[i].resize(intvls + 2);

	for (int o = 0; o < octvs; o++) {
		for (int i = 0; i < intvls + 2; i++) {
			dog_pyr[o][i] = make_auto_block_using(gauss_pyr[o][i]);
			dog_pyr[o][i].assign(gauss_pyr[o][i + 1] - gauss_pyr[o][i]);
		}
	}

	//detect features at extrema in DoG scale space.  Bad features are discarded based on contrast and ratio of principal curvatures.
	double prelim_contr_thr = 0.5 * contr_thr / intvls;
	dynamic_array<Feature> features;

	for (int o = 0; o < octvs; o++) {
		for (int i = 1; i <= intvls; i++) {
			for (int r = SIFT_IMG_BORDER; r < dog_pyr[o][0].size<2>() - SIFT_IMG_BORDER; r++) {
				for (int c = SIFT_IMG_BORDER; c < dog_pyr[o][0].size<1>() - SIFT_IMG_BORDER; c++) {
					// perform preliminary check on contrast
					if (abs(dog_pyr[o][i][r][c]) > prelim_contr_thr) {
						if (is_extremum(dog_pyr, o, i, r, c)) {
							Feature	feat;
							if (interp_extremum(dog_pyr, o, i, r, c, intvls, contr_thr, feat)) {
								if (!is_too_edge_like(dog_pyr[feat.data.octv][feat.data.intvl], feat.data.r, feat.data.c, curv_thr))
									features.push_back(feat);
							}
						}
					}
				}
			}
		}
	}

	// Calculates characteristic scale for each feature in an array.
	for (int i = 0, n = features.size32(); i < n; i++) {
		Feature			&feat	= features[i];
		double			intvl	= feat.data.intvl + feat.data.subintvl;
		feat.scl				= sigma * pow(2.0, feat.data.octv + intvl / intvls);
		feat.data.scl_octv		= sigma * pow(2.0, intvl / intvls);
	}

	if (img_dbl) {
		for (int i = 0, n = features.size32(); i < n; i++) {
			Feature	&feat	= features[i];
			feat.x			/= 2;
			feat.y			/= 2;
			feat.scl		/= 2;
			feat.img_pt.x	/= 2;
			feat.img_pt.y	/= 2;
		}
	}

	//Compute a canonical orientation for each image feature in an array
	//adds features to the array when there is more than one dominant orientation at a given feature location
	//Based on Section 5 of Lowe's paper
	dynamic_array<Feature> features2;
	for (int i = 0, n = features.size32(); i < n; i++) {
		Feature			&feat	= features[i];
		double			*hist	= ori_hist(gauss_pyr[feat.data.octv][feat.data.intvl], feat.data.r, feat.data.c, SIFT_ORI_HIST_BINS, round(SIFT_ORI_RADIUS * feat.data.scl_octv), SIFT_ORI_SIG_FCTR * feat.data.scl_octv);
		for (int j = 0; j < SIFT_ORI_SMOOTH_PASSES; j++)
			smooth_ori_hist(hist, SIFT_ORI_HIST_BINS);
		double omax = dominant_ori(hist, SIFT_ORI_HIST_BINS);

		//  Adds features to an array for every orientation in a histogram greater than a specified threshold.
		int	bins = SIFT_ORI_HIST_BINS;
		double mag_thr	= omax * SIFT_ORI_PEAK_RATIO;
		for (int i = 0; i < bins; i++) {
			int l = (i == 0) ? bins - 1 : i - 1;
			int r = (i + 1) % bins;

			if (hist[i] > hist[l] && hist[i] > hist[r] && hist[i] >= mag_thr) {
				double bin = i + interp_hist_peak(hist[l], hist[i], hist[r]);
				bin = (bin < 0) ? bins + bin : (bin >= bins) ? bin - bins : bin;

				auto &new_feat = features2.push_back(feat);
				new_feat.ori = ((two * pi * bin) / bins) - pi;
			}
		}
	}

	// Compute feature descriptors for features in an array
	// Based on Section 6 of Lowe's paper.
	for (int i = 0, n = features2.size32(); i < n; i++) {
		Feature			&feat	= features2[i];
		auto_block<double,3> hist = descr_hist(gauss_pyr[feat.data.octv][feat.data.intvl], feat.data.r, feat.data.c, feat.ori, feat.data.scl_octv, descr_width, descr_hist_bins);
		hist_to_descr(hist, descr_width, descr_hist_bins, &feat);
	}

	// sort features by decreasing scale and move from CvSeq to array
	sort(features2, [](Feature &f1, Feature &f2) { return f1.scl < f2.scl; });

	return features2;
}

dynamic_array<Feature> sift_features(Image &img) {
	return _sift_features(img, SIFT_INTVLS, SIFT_SIGMA, SIFT_CONTR_THR,  SIFT_CURV_THR, SIFT_IMG_DBL, SIFT_DESCR_WIDTH, SIFT_DESCR_HIST_BINS );
}
