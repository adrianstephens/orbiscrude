#include "base/algorithm.h"
#include "base/maths.h"
#include "base/array.h"
#include "base/bits.h"
#include "maths/spherical_harmonics.h"
#include "maths/fft.h"

using namespace iso;

#define DEFAULT_ORDER					1
#define DEFAULT_HEIGHT					true
#define DEFAULT_BFORMAT_SAMPLECOUNT		512
#define DEFAULT_SAMPLERATE				44100
#define DEFAULT_BLOCKSIZE				512
#define DEFAULT_HRTFSET_DIFFUSED		false

static const float fSqrt3		= sqrt(3.f);
static const float fSqrt6		= sqrt(6.f);
static const float fSqrt10		= sqrt(10.f);
static const float fSqrt15		= sqrt(15.f);

typedef complex<float> fft_complex;

//-----------------------------------------------------------------------------
// HRTF (head-relative transform function)
//-----------------------------------------------------------------------------

struct MIT_HRTF {
	enum {
		AZI_00		= 37,
		AZI_10		= 37,
		AZI_20		= 37,
		AZI_30		= 31,
		AZI_40		= 29,
		AZI_50		= 23,
		AZI_60		= 19,
		AZI_70		= 13,
		AZI_80		= 7,
		AZI_90		= 1,
		TAPS_44		= 128,
		TAPS_48		= 140,
		TAPS_88		= 256,
		TAPS_96		= 279,
	};
};

template<int TAPS> struct HRTF_filter {
	short left[TAPS];
	short right[TAPS];
};


template<int TAPS> struct HRTF_filter_set {
	HRTF_filter<TAPS> e_10[MIT_HRTF::AZI_10];
	HRTF_filter<TAPS> e_20[MIT_HRTF::AZI_20];
	HRTF_filter<TAPS> e_30[MIT_HRTF::AZI_30];
	HRTF_filter<TAPS> e_40[MIT_HRTF::AZI_40];
	HRTF_filter<TAPS> e00[MIT_HRTF::AZI_00];
	HRTF_filter<TAPS> e10[MIT_HRTF::AZI_10];
	HRTF_filter<TAPS> e20[MIT_HRTF::AZI_20];
	HRTF_filter<TAPS> e30[MIT_HRTF::AZI_30];
	HRTF_filter<TAPS> e40[MIT_HRTF::AZI_40];
	HRTF_filter<TAPS> e50[MIT_HRTF::AZI_50];
	HRTF_filter<TAPS> e60[MIT_HRTF::AZI_60];
	HRTF_filter<TAPS> e70[MIT_HRTF::AZI_70];
	HRTF_filter<TAPS> e80[MIT_HRTF::AZI_80];
	HRTF_filter<TAPS> e90[MIT_HRTF::AZI_90];

	int get(int elevation, int azimuth_index, bool switch_leftright, const int16 *&left, const int16 *&right) const {
		switch (elevation) {
			case -1:	left = e_10[azimuth_index].left;	right = e_10[azimuth_index].right;	break;
			case -2:	left = e_20[azimuth_index].left;	right = e_20[azimuth_index].right;	break;
			case -3:	left = e_30[azimuth_index].left;	right = e_30[azimuth_index].right;	break;
			case -4:	left = e_40[azimuth_index].left;	right = e_40[azimuth_index].right;	break;
			case 0:		left = e00[azimuth_index].left;		right = e00[azimuth_index].right;	break;
			case 1:		left = e10[azimuth_index].left;		right = e10[azimuth_index].right;	break;
			case 2:		left = e20[azimuth_index].left;		right = e20[azimuth_index].right;	break;
			case 3:		left = e30[azimuth_index].left;		right = e30[azimuth_index].right;	break;
			case 4:		left = e40[azimuth_index].left;		right = e40[azimuth_index].right;	break;
			case 5:		left = e50[azimuth_index].left;		right = e50[azimuth_index].right;	break;
			case 6:		left = e60[azimuth_index].left;		right = e60[azimuth_index].right;	break;
			case 7:		left = e70[azimuth_index].left;		right = e70[azimuth_index].right;	break;
			case 8:		left = e80[azimuth_index].left;		right = e80[azimuth_index].right;	break;
			case 9:		left = e90[azimuth_index].left;		right = e90[azimuth_index].right;	break;
		}

		if (switch_leftright)
			swap(left, right);

		return TAPS;
	}
};

#include "ambisonic_44.h"

uint32 mit_hrtf_get(float azimuth, float elevation, uint32 samplerate, const int16 *&left, const int16 *&right) {
	//Snap elevation to the nearest available elevation in the filter set
	int		ielevation = round(elevation * 18 / pi);

	//Determine array index for azimuth based on elevation
	int azimuth_index = 0;
	switch (abs(ielevation)) {
		case 0:	azimuth_index = int(abs(azimuth) * (MIT_HRTF::AZI_00 - 1) / pi + .5f);	break;
		case 1:	azimuth_index = int(abs(azimuth) * (MIT_HRTF::AZI_10 - 1) / pi + .5f); break;
		case 2:	azimuth_index = int(abs(azimuth) * (MIT_HRTF::AZI_20 - 1) / pi + .5f); break;
		case 3:	azimuth_index = int(abs(azimuth) * (MIT_HRTF::AZI_30 - 1) / pi + .5f); break;
		case 4: {
			int	table[][2] = {
				{4, 0},
				{10, 6},
				{17, 13},
				{23, 19},
				{30, 26},
				{36, 32},
				{43, 39},
				{49, 45},
				{55, 51},
				{62, 58},
				{68, 64},
				{75, 71},
				{81, 77},
				{88, 84},
				{94, 90},
				{100, 96},
				{107, 103},
				{113, 109},
				{120, 116},
				{126, 122},
				{133, 129},
				{139, 135},
				{145, 141},
				{152, 148},
				{158, 154},
				{165, 161},
				{171, 167},
				{178, 174},
				{180, 180}
			};
			int		iazimuth = int(abs(azimuth) * 180 / pi + .5f);
			auto	low = lower_boundc(table, iazimuth, [](int a[2], int b) { return a[0] < b; });
			azimuth_index = low - table;
			break;
		}
		case 5: {
			// Elevation of 50 has a maximum 176 in the azimuth plane so we need to handle that.
			float	limit = 176.f * pi / 180;
			azimuth_index = int(min(abs(azimuth), limit) * (MIT_HRTF::AZI_50 - 1) / limit + .5f);
			break;
		}
		case 6:	azimuth_index = int(abs(azimuth) * (MIT_HRTF::AZI_60 - 1) / pi + .5f);    break;
		case 7:	azimuth_index = int(abs(azimuth) * (MIT_HRTF::AZI_70 - 1) / pi + .5f);    break;
		case 8:	azimuth_index = int(abs(azimuth) * (MIT_HRTF::AZI_80 - 1) / pi + .5f);    break;
		case 9:	azimuth_index = int(abs(azimuth) * (MIT_HRTF::AZI_90 - 1) / pi + .5f);    break;
	};

	switch (samplerate) {
		case 44100:	return normal_44.get(ielevation, azimuth_index, azimuth < 0, left, right);
			//		case 48000:	return normal_48.get(ielevation, azimuth_index, azimuth < 0, left, right);
			//		case 88200:	return normal_88.get(ielevation, azimuth_index, azimuth < 0, left, right);
			//		case 96000:	return normal_96.get(ielevation, azimuth_index, azimuth < 0, left, right);
		default: return 0;
	}
}

class HRTF {
	uint32 sample_rate;
public:
	HRTF(uint32 _sample_rate) : sample_rate(_sample_rate) {}

	uint32	size() const {
		switch (sample_rate) {
			case 44100:	return MIT_HRTF::TAPS_44;
			case 48000:	return MIT_HRTF::TAPS_48;
			case 88200:	return MIT_HRTF::TAPS_88;
			case 96000:	return MIT_HRTF::TAPS_96;
			default: return 0;
		}
	}
	uint32 get(float azimuth, float elevation, float** hrtf) const {
		//Get HRTFs for given position
		const int16	*left, *right;
		if (uint32 taps = mit_hrtf_get(azimuth, elevation, sample_rate, left, right)) {
			for (uint32 t = 0; t < taps; t++) {
				hrtf[0][t] = left[t] / 32767.f;
				hrtf[1][t] = right[t] / 32767.f;
			}
			return taps;
		}
		return 0;
	}
};

//-----------------------------------------------------------------------------
// Ambisonic base class
//-----------------------------------------------------------------------------

//TODO
enum BFormatChannels3D {
	kW,
	kY, kZ, kX,
	kV, kT, kR, kS, kU,
	kQ, kO, kM, kK, kL, kN, kP,
	kNumOfBformatChannels3D
};

/*enum BFormatChannels2D
{
	kW,
	kX, kY,
	kU, kV,
	kP, kQ,
	kNumOfBformatChannels2D
};*/

enum SpeakerSetUp {
    SpeakerCustomSpeakerSetUp = -1,
    ///2D Speaker Setup
    SpeakerMono, SpeakerStereo, SpeakerLCR, SpeakerQuad, Speaker50,
    SpeakerPentagon, SpeakerHexagon, SpeakerHexagonWithCentre, SpeakerOctagon,
    SpeakerDecadron, SpeakerDodecadron,
    ///3D Speaker Setup
    SpeakerCube,
    SpeakerDodecahedron,
    SpeakerCube2,
    SpeakerMonoCustom,
    SpeakerNumOfSpeakerSetUps
};

class AmbisonicBase {
protected:
	uint32	order;
	uint32	channels;
	bool	is3D;
public:
	static uint32 OrderToComponents(uint32 order, bool is3D) {
		return is3D ? square(order + 1) : order * 2 + 1;
	}
	static uint32 OrderToSpeakers(uint32 order, bool is3D) {
		return is3D ? (order * 2 + 2) * 2 : order * 2 + 2;
	}
	static uint32 OrderToComponentPosition(uint32 order, bool is3D) {
		static const uint8	comp[2][4] = {
			{0, 1, 4, 10},
			{0, 1, 3, 5},
		};
		return comp[is3D][order];
	}

	AmbisonicBase() : order(0), channels(0), is3D(0)  {}
	virtual ~AmbisonicBase()	{}
	uint32	GetOrder()			{ return order; }
	uint32	GetChannelCount()	{ return channels; }
	bool	Is3D()				{ return is3D; }

	bool Configure(uint32 _order, bool b3D) {
		order		= _order;
		is3D		= b3D;
		channels	= OrderToComponents(order, is3D);
		return true;
	}
	virtual void Reset()	{}
	virtual void Refresh()	{}
};

//-----------------------------------------------------------------------------
// BFormat
//-----------------------------------------------------------------------------

class BFormat : public AmbisonicBase {
public:
	uint32					samples;
	dynamic_array<float>	sample_data;
	dynamic_array<float*>	channel_starts;
public:
	BFormat() : samples(0) {}
	uint32 GetSampleCount() { return samples; }

	bool Configure(uint32 order, bool b3D, uint32 nSampleCount) {
		if (!AmbisonicBase::Configure(order, b3D))
			return false;

		samples		= nSampleCount;
		sample_data.resize(samples * channels);
		memset(sample_data, 0, samples * channels * sizeof(float));
		channel_starts.resize(channels);

		for (uint32 i = 0; i < channels; i++)
			channel_starts[i] = &sample_data[i * samples];

		return true;
	}
	void	Reset()					{ memset(sample_data, 0, sample_data.size() * sizeof(float)); }
	float	*Channel(int c)			{ return channel_starts[c]; }
	void	InsertStream(const float* src, uint32 c, uint32 nSamples)	{ memcpy(Channel(c), src, nSamples * sizeof(float)); }
	void	ExtractStream(float* dst, uint32 c, uint32 nSamples)		{ memcpy(dst, Channel(c), nSamples * sizeof(float)); }

	void operator=(const BFormat &bf)	{ memcpy(sample_data, bf.sample_data, sample_data.size() * sizeof(float)); }
	bool operator==(const BFormat &bf)	{ return is3D == bf.is3D && order == bf.order && sample_data.size() == bf.sample_data.size(); }
	bool operator!=(const BFormat &bf)	{ return !(*this == bf); }

//	BFormat& operator+=(const BFormat &bf) { for (size_t i = 0, n = sample_data.size(); i < n; i++) sample_data[i] += bf.sample_data[i]; return *this; }
//	BFormat& operator-=(const BFormat &bf) { for (size_t i = 0, n = sample_data.size(); i < n; i++) sample_data[i] -= bf.sample_data[i]; return *this; }
//	BFormat& operator*=(const BFormat &bf) { for (size_t i = 0, n = sample_data.size(); i < n; i++) sample_data[i] *= bf.sample_data[i]; return *this; }
//	BFormat& operator/=(const BFormat &bf) { for (size_t i = 0, n = sample_data.size(); i < n; i++) sample_data[i] /= bf.sample_data[i]; return *this; }
//	BFormat& operator+=(const float fValue) { for (size_t i = 0, n = sample_data.size(); i < n; i++) sample_data[i] += fValue; return *this; }
//	BFormat& operator-=(const float fValue) { for (size_t i = 0, n = sample_data.size(); i < n; i++) sample_data[i] -= fValue; return *this; }
//	BFormat& operator*=(const float fValue) { for (size_t i = 0, n = sample_data.size(); i < n; i++) sample_data[i] *= fValue; return *this; }
//	BFormat& operator/=(const float fValue) { for (size_t i = 0, n = sample_data.size(); i < n; i++) sample_data[i] /= fValue; return *this; }
};

//-----------------------------------------------------------------------------
// Ambisonic source
//-----------------------------------------------------------------------------

struct PolarPoint {
	float azimuth, elevation, distance;
	PolarPoint(float _azimuth, float _elevation, float _distance) : azimuth(_azimuth), elevation(_elevation), distance(_distance) {}
};

class AmbisonicSource : public AmbisonicBase {
protected:
	dynamic_array<float>	coeffs;
	dynamic_array<float>	order_weights;
	PolarPoint				polar;
	float					gain;
public:
	AmbisonicSource() : polar(0, 0, 1), gain(1) {}

	bool Configure(uint32 order, bool b3D) {
		if (!AmbisonicBase::Configure(order, b3D))
			return false;

		coeffs.resize(channels, 0);
		order_weights.resize(order + 1, 1);
		return true;
	}

	virtual void Refresh() {
		float cos_azim = cos(polar.azimuth);
		float sin_azim = sin(polar.azimuth);
		float cos_elev = cos(polar.elevation);
		float sin_elev = sin(polar.elevation);

		float cos_2azim = cos(2 * polar.azimuth);
		float sin_2azim = sin(2 * polar.azimuth);
		float sin_2elev = sin(2 * polar.elevation);

		if (is3D) {
			// Uses ACN channel ordering and SN3D normalization scheme (AmbiX format)
			if (order >= 0) {
				coeffs[ 0] = 1 * order_weights[0];						// W
			}
			if (order >= 1) {
				coeffs[ 1] = (sin_azim * cos_elev) * order_weights[1];	// Y
				coeffs[ 2] = (sin_elev)* order_weights[1];				// Z
				coeffs[ 3] = (cos_azim * cos_elev) * order_weights[1];	// X
			}
			if (order >= 2) {
				coeffs[ 4] = fSqrt3 / 2 * (sin_2azim * square(cos_elev)) * order_weights[2];						// V
				coeffs[ 5] = fSqrt3 / 2 * (sin_azim * sin_2elev) * order_weights[2];								// T
				coeffs[ 6] = (1.5f * square(sin_elev) - 0.5f) * order_weights[2];									// R
				coeffs[ 7] = fSqrt3 / 2 * (cos_azim * sin_2elev) * order_weights[2];								// S
				coeffs[ 8] = fSqrt3 / 2 * (cos_2azim * square(cos_elev)) * order_weights[2];						// U
			}
			if (order >= 3) {
				coeffs[ 9] = fSqrt10 / 4 * (sin(3 * polar.azimuth) * cube(cos_elev)) * order_weights[3];			// Q
				coeffs[10] = fSqrt15 / 2 * (sin_2azim * sin_elev * square(cos_elev)) * order_weights[3];			// O
				coeffs[11] = fSqrt6 / 4 * (sin_azim * cos_elev * (5 * square(sin_elev) - 1)) * order_weights[3];	// M
				coeffs[12] = (sin_elev * (5 * square(sin_elev) - 3) * 0.5f) * order_weights[3];						// K
				coeffs[13] = fSqrt6 / 4 * (cos_azim * cos_elev * (5 * square(sin_elev) - 1)) * order_weights[3];	// L
				coeffs[14] = fSqrt15 / 2 * (cos_2azim * sin_elev * square(cos_elev)) * order_weights[3];			// N
				coeffs[15] = fSqrt10 / 4 * (cos(3 * polar.azimuth) * cube(cos_elev)) * order_weights[3];			// P

			}
		} else {
			if (order >= 0) {
				coeffs[0] = order_weights[0];
			}
			if (order >= 1) {
				coeffs[1] = (cos_azim * cos_elev) * order_weights[1];
				coeffs[2] = (sin_azim * cos_elev) * order_weights[1];
			}
			if (order >= 2) {
				coeffs[3] = (cos_2azim * square(cos_elev)) * order_weights[2];
				coeffs[4] = (sin_2azim * square(cos_elev)) * order_weights[2];
			}
			if (order >= 3) {
				coeffs[5] = (cos(3 * polar.azimuth) * cube(cos_elev)) * order_weights[3];
				coeffs[6] = (sin(3 * polar.azimuth) * cube(cos_elev)) * order_weights[3];
			}
		}

		for (uint32 i = 0; i < channels; i++)
			coeffs[i] *= gain;
	}

	void		SetPosition(const PolarPoint &_polar)			{ polar = _polar; }
	PolarPoint&	GetPosition()									{ return polar;	}
	void		SetOrderWeight(uint32 order, float weight)		{ order_weights[order] = weight;	}
	void		SetOrderWeightAll(float weight)					{ for (uint32 i = 0; i < order + 1; i++) order_weights[i] = weight; }
	void		SetCoefficient(uint32 channel, float coeff)		{ coeffs[channel] = coeff; }
	float		GetOrderWeight(uint32 order)					{ return order_weights[order];	}
	float		GetCoefficient(uint32 channel)					{ return coeffs[channel];	}
	void		SetGain(float _gain)							{ gain = _gain;	}
	float		GetGain()										{ return gain;	}
};

//-----------------------------------------------------------------------------
// Ambisonic speaker
//-----------------------------------------------------------------------------

class AmbisonicSpeaker : public AmbisonicSource {
public:
    void Process(BFormat *src, uint32 nSamples, float *dst) {
		memset(dst, 0, nSamples * sizeof(float));
		for (uint32 c = 0; c < channels; c++) {
			float	*chan = src->Channel(c);
			if (is3D) {
				// coefficients are multiplied by (2*order + 1) to provide the correct decoder for SN3D normalised Ambisonic inputs
				float	scaler = 2 * floor(sqrt(c)) + 1;
				for (uint32 s = 0; s < nSamples; s++)
					dst[s] += chan[s] * coeffs[c] * scaler;
			} else {
				// coefficients are multiplied by 2 to provide the correct decoder for SN3D normalised Ambisonic inputs decoded to a horizontal loudspeaker array
				for(uint32 s = 0; s < nSamples; s++)
					dst[s] += chan[s] * coeffs[c] * 2;
			}
		}
	}
};

//-----------------------------------------------------------------------------
// Ambisonic microphone
//-----------------------------------------------------------------------------

class AmbisonicMicrophone : public AmbisonicSource {
	float directivity;
public:
	AmbisonicMicrophone() : directivity(1) {}

	void Refresh() {
		AmbisonicSource::Refresh();
		coeffs[0] *= (2 - directivity) * sqrt(2.f);
	}

	void Process(BFormat* src, uint32 nSamples, float *dst) {
		for (uint32 s = 0; s < nSamples; s++) {
			float	tempa = src->Channel(0)[s] * coeffs[0];
			float	tempb = 0;
			for (uint32 c = 1; c < channels; c++)
				tempb += src->Channel(c)[s] * coeffs[c];
			dst[s] = (tempa + tempb * directivity) / 2;
		}
	}

	void	SetDirectivity(float _directivity)	{ directivity = _directivity; }
	float	GetDirectivity()					{ return directivity; }
};

//-----------------------------------------------------------------------------
// Ambisonic decoder
//-----------------------------------------------------------------------------

class AmbisonicDecoder : public AmbisonicBase {
	dynamic_array<AmbisonicSpeaker>	speakers;

	void SetSpeakerSetUp(SpeakerSetUp _setup, uint32 nSpeakers);
public:
	AmbisonicDecoder() {}

	bool Configure(uint32 order, bool b3D, SpeakerSetUp setup, uint32 nSpeakers = 0) {
		if (!AmbisonicBase::Configure(order, b3D))
			return false;
		SetSpeakerSetUp(setup, nSpeakers);
		Refresh();
		return true;
	}
	void Reset() {
		for (auto &i : speakers)
			i.Reset();
	}
	void Refresh() {
		for (auto &i : speakers)
			i.Refresh();
	}
	void Process(BFormat *src, uint32 nSamples, float **dst) {
		for (auto &i : speakers)
			i.Process(src, nSamples, *dst++);
	}
	uint32		GetSpeakerCount()												{ return speakers.size32();	}
	void		SetPosition(uint32 nSpeaker, PolarPoint _polar)					{ speakers[nSpeaker].SetPosition(_polar); }
	PolarPoint	GetPosition(uint32 nSpeaker)									{ return speakers[nSpeaker].GetPosition(); }
	void		SetOrderWeight(uint32 nSpeaker, uint32 order, float fWeight)	{ speakers[nSpeaker].SetOrderWeight(order, fWeight); }
	float		GetOrderWeight(uint32 nSpeaker, uint32 order)					{ return speakers[nSpeaker].GetOrderWeight(order); }
	float		GetCoefficient(uint32 nSpeaker, uint32 nChannel)				{ return speakers[nSpeaker].GetCoefficient(nChannel); }
	void		SetCoefficient(uint32 nSpeaker, uint32 nChannel, float fCoeff)	{ speakers[nSpeaker].SetCoefficient(nChannel, fCoeff); }
};

void AmbisonicDecoder::SetSpeakerSetUp(SpeakerSetUp setup, uint32 nSpeakers) {
	PolarPoint	polar(0, 0, 1);
	int			x = 0;

	switch (setup) {
		case SpeakerCustomSpeakerSetUp:
			speakers.resize(nSpeakers);
			for (auto &i : speakers)
				i.Configure(order, is3D);
			break;
		case SpeakerMono:
			speakers.resize(1);
			speakers[0].Configure(order, is3D);
			speakers[0].SetPosition(polar);
			break;
		case SpeakerStereo:
			speakers.resize(2);
			polar.azimuth = pi / 6;
			speakers[0].Configure(order, is3D);
			speakers[0].SetPosition(polar);
			polar.azimuth = -pi / 6;
			speakers[1].Configure(order, is3D);
			speakers[1].SetPosition(polar);
			break;
		case SpeakerLCR:
			speakers.resize(3);
			polar.azimuth = pi / 6;
			speakers[0].Configure(order, is3D);
			speakers[0].SetPosition(polar);
			polar.azimuth = 0;
			speakers[1].Configure(order, is3D);
			speakers[1].SetPosition(polar);
			polar.azimuth = -pi / 6;
			speakers[2].Configure(order, is3D);
			speakers[2].SetPosition(polar);
			break;
		case SpeakerQuad:
			speakers.resize(4);
			polar.azimuth = pi / 4;
			speakers[0].Configure(order, is3D);
			speakers[0].SetPosition(polar);
			polar.azimuth = -pi / 4;
			speakers[1].Configure(order, is3D);
			speakers[1].SetPosition(polar);
			polar.azimuth = pi *3 / 4;
			speakers[2].Configure(order, is3D);
			speakers[2].SetPosition(polar);
			polar.azimuth = -pi * 3 / 4;
			speakers[3].Configure(order, is3D);
			speakers[3].SetPosition(polar);
			break;
		case Speaker50:
			speakers.resize(5);
			polar.azimuth = pi / 6;
			speakers[0].Configure(order, is3D);
			speakers[0].SetPosition(polar);
			polar.azimuth = -pi / 6;
			speakers[1].Configure(order, is3D);
			speakers[1].SetPosition(polar);
			polar.azimuth = 0;
			speakers[2].Configure(order, is3D);
			speakers[2].SetPosition(polar);
			polar.azimuth = degrees(110.f);
			speakers[3].Configure(order, is3D);
			speakers[3].SetPosition(polar);
			polar.azimuth = degrees(-110.f);
			speakers[4].Configure(order, is3D);
			speakers[4].SetPosition(polar);
			break;
		case SpeakerPentagon:
			speakers.resize(5);
			for (auto &i : speakers) {
				polar.azimuth = -degrees(x++ * 360.f / 5);
				i.Configure(order, is3D);
				i.SetPosition(polar);
			}
			break;
		case SpeakerHexagon:
			speakers.resize(6);
			for (auto &i : speakers) {
				polar.azimuth = -degrees(x++ * 360.f / 6 + 30.f);
				i.Configure(order, is3D);
				i.SetPosition(polar);
			}
			break;
		case SpeakerHexagonWithCentre:
			speakers.resize(6);
			for (auto &i : speakers) {
				polar.azimuth = -degrees(x++ * 360.f / 6);
				i.Configure(order, is3D);
				i.SetPosition(polar);
			}
			break;
		case SpeakerOctagon:
			speakers.resize(8);
			for (auto &i : speakers) {
				polar.azimuth = -degrees(x++ * 360.f / 8);
				i.Configure(order, is3D);
				i.SetPosition(polar);
			}
			break;
		case SpeakerDecadron:
			speakers.resize(10);
			for (auto &i : speakers) {
				polar.azimuth = -degrees(x++ * 360.f / 10);
				i.Configure(order, is3D);
				i.SetPosition(polar);
			}
			break;
		case SpeakerDodecadron:
			speakers.resize(12);
			for (auto &i : speakers) {
				polar.azimuth = -degrees(x++ * 360.f / 12);
				i.Configure(order, is3D);
				i.SetPosition(polar);
			}
			break;
		case SpeakerCube:
			speakers.resize(8);
			for (auto &i : speakers) {
				polar.elevation = x < 4 ? degrees(45.f) : degrees(-45.f);
				polar.azimuth = x < 4
					? -degrees(x * 360.f / (8 / 2) + 45.f)
					: -degrees((x - 4) * 360.f / (8 / 2) + 45.f);
				i.Configure(order, is3D);
				i.SetPosition(polar);
				++x;
			}
			break;
		case SpeakerDodecahedron:
			// This arrangement is used for second and third orders
			speakers.resize(20);
			// Loudspeaker 1
			polar.elevation = degrees(-69.1f);
			polar.azimuth = degrees(90.f);
			speakers[0].Configure(order, is3D);
			speakers[0].SetPosition(polar);
			// Loudspeaker 2
			polar.azimuth = degrees(-90.f);
			speakers[1].Configure(order, is3D);
			speakers[1].SetPosition(polar);

			// Loudspeaker 3
			polar.elevation = degrees(-35.3f);
			polar.azimuth = degrees(45.f);
			speakers[2].Configure(order, is3D);
			speakers[2].SetPosition(polar);
			// Loudspeaker 4
			polar.azimuth = degrees(135.f);
			speakers[3].Configure(order, is3D);
			speakers[3].SetPosition(polar);
			// Loudspeaker 5
			polar.azimuth = degrees(-45.f);
			speakers[4].Configure(order, is3D);
			speakers[4].SetPosition(polar);
			// Loudspeaker 6
			polar.azimuth = degrees(-135.f);
			speakers[5].Configure(order, is3D);
			speakers[5].SetPosition(polar);

			// Loudspeaker 7
			polar.elevation = degrees(-20.9f);
			polar.azimuth = degrees(180.f);
			speakers[6].Configure(order, is3D);
			speakers[6].SetPosition(polar);
			// Loudspeaker 8
			polar.azimuth = 0;
			speakers[7].Configure(order, is3D);
			speakers[7].SetPosition(polar);

			// Loudspeaker 9
			polar.elevation = 0;
			polar.azimuth = degrees(69.1f);
			speakers[8].Configure(order, is3D);
			speakers[8].SetPosition(polar);
			// Loudspeaker 10
			polar.azimuth = degrees(110.9f);
			speakers[9].Configure(order, is3D);
			speakers[9].SetPosition(polar);
			// Loudspeaker 11
			polar.azimuth = degrees(-69.1f);
			speakers[10].Configure(order, is3D);
			speakers[10].SetPosition(polar);
			// Loudspeaker 12
			polar.azimuth = degrees(-110.9f);
			speakers[11].Configure(order, is3D);
			speakers[11].SetPosition(polar);

			// Loudspeaker 13
			polar.elevation = degrees(20.9f);
			polar.azimuth = degrees(180.f);
			speakers[12].Configure(order, is3D);
			speakers[12].SetPosition(polar);
			// Loudspeaker 14
			polar.azimuth = 0;
			speakers[13].Configure(order, is3D);
			speakers[13].SetPosition(polar);

			// Loudspeaker 15
			polar.elevation = degrees(35.3f);
			polar.azimuth = degrees(45.f);
			speakers[14].Configure(order, is3D);
			speakers[14].SetPosition(polar);
			// Loudspeaker 16
			polar.azimuth = degrees(135.f);
			speakers[15].Configure(order, is3D);
			speakers[15].SetPosition(polar);
			// Loudspeaker 17
			polar.azimuth = degrees(-45.f);
			speakers[16].Configure(order, is3D);
			speakers[16].SetPosition(polar);
			// Loudspeaker 18
			polar.azimuth = degrees(-135.f);
			speakers[17].Configure(order, is3D);
			speakers[17].SetPosition(polar);

			// Loudspeaker 19
			polar.elevation = degrees(69.1f);
			polar.azimuth = degrees(90.f);
			speakers[18].Configure(order, is3D);
			speakers[18].SetPosition(polar);
			// Loudspeaker 20
			polar.azimuth = degrees(-90.f);
			speakers[19].Configure(order, is3D);
			speakers[19].SetPosition(polar);
			break;
		case SpeakerCube2:
			// This configuration is a standard for first order decoding
			speakers.resize(8);
			for (auto &i : speakers) {
				polar.elevation = x < 4 ? degrees(35.2f) : degrees(-35.2f);
				polar.azimuth = x < 4
					? -degrees(x * 360.f / (8 / 2) + 45.f)
					: -degrees((x - 4) * 360.f / (8 / 2) + 45.f);
				i.Configure(order, is3D);
				i.SetPosition(polar);
				++x;
			}
			break;
		case SpeakerMonoCustom:
			speakers.resize(17);
			polar.azimuth = 0;
			polar.elevation = 0;
			polar.distance = 1;
			for (auto &i : speakers) {
				polar.azimuth = 0;
				i.Configure(order, is3D);
				i.SetPosition(polar);
			}
			break;
		default:
			speakers.resize(1);
			speakers[0].Configure(order, is3D);
			speakers[0].SetPosition(polar);
			break;
	};

	float	gain = rsqrt(float(speakers.size()));
	for (auto &i : speakers)
		i.SetGain(gain);
}

//-----------------------------------------------------------------------------
// Ambisonic encoder
//-----------------------------------------------------------------------------

class AmbisonicEncoder : public AmbisonicSource {
public:
	void Process(float *src, uint32 nSamples, BFormat *dst) {
		for (uint32 c = 0; c < channels; c++) {
			float	*chan = dst->Channel(c);
			for (uint32 s = 0; s < nSamples; s++)
				chan[s] = src[s] * coeffs[c];
		}
	}
};

//-----------------------------------------------------------------------------
// Ambisonic encoder with distance cues
//-----------------------------------------------------------------------------

class AmbisonicEncoderDist : public AmbisonicEncoder {
	static const uint32 knSpeedOfSound	= 344;
	static const uint32 knMaxDistance	= 150;

	uint32	sample_rate;
	int		num_in, num_outa, num_outb;
	float	delay;
	float	room_radius;
	float	interior_gain, exterior_gain;
	dynamic_array<float> delay_buffer;
public:
	AmbisonicEncoderDist() : sample_rate(0), num_in(0), num_outa(0), num_outb(0), delay(0), room_radius(5), interior_gain(0), exterior_gain(0) {
		Configure(DEFAULT_ORDER, DEFAULT_HEIGHT, DEFAULT_SAMPLERATE);
	}

	bool Configure(uint32 order, bool b3D, uint32 nSampleRate) {
		if (!AmbisonicEncoder::Configure(order, b3D))
			return false;
		sample_rate = nSampleRate;
		delay_buffer.resize((uint32)((float)knMaxDistance / knSpeedOfSound * sample_rate + 0.5f));
		Reset();
		return true;
	}

	virtual void Reset() {
		memset(delay_buffer, 0, delay_buffer.size() * sizeof(float));
		delay		= polar.distance / knSpeedOfSound * sample_rate + 0.5f;
		num_in		= 0;
		num_outa	= (num_in - int(delay) + delay_buffer.size32()) % delay_buffer.size32();
		num_outb	= (num_outa + 1) % delay_buffer.size32();
	}

	virtual void Refresh() {
		AmbisonicEncoder::Refresh();

		delay		= abs(polar.distance) / knSpeedOfSound * sample_rate;
		num_outa	= (num_in - int(delay) + delay_buffer.size32()) % delay_buffer.size32();
		num_outb	= (num_outa + 1) % delay_buffer.size32();

		//Source is outside speaker array
		if (abs(polar.distance) >= room_radius) {
			interior_gain = (room_radius / abs(polar.distance)) / 2;
			exterior_gain = interior_gain;
		} else {
			interior_gain = (2 - abs(polar.distance) / room_radius) / 2;
			exterior_gain = (abs(polar.distance) / room_radius) / 2;
		}
	}

	void Process(float *src, uint32 nSamples, BFormat *dst) {
		float	f0	= frac(delay), f1 = 1 - f0;
		for (uint32 s = 0; s < nSamples; s++) {
			//Store
			delay_buffer[num_in] = src[s];
			//Read
			float	sample = delay_buffer[num_outa] * f1 + delay_buffer[num_outb] * f0;

			dst->Channel(kW)[s] = sample * interior_gain * coeffs[kW];

			sample *= exterior_gain;
			for (uint32 c = 1; c < channels; c++)
				dst->Channel(c)[s] = sample * coeffs[c];

			num_in		= (num_in   + 1) % delay_buffer.size32();
			num_outa	= (num_outa + 1) % delay_buffer.size32();
			num_outb	= (num_outb + 1) % delay_buffer.size32();
		}
	}

	void	SetRoomRadius(float radius) { room_radius = radius; }
	float	GetRoomRadius()				{ return room_radius; }
};

//-----------------------------------------------------------------------------
// B-Format to binaural decode
//-----------------------------------------------------------------------------

class AmbisonicBinauralizer : public AmbisonicBase {
	AmbisonicDecoder decoder;

	uint32		block_size;
	uint32		taps;
	uint32		fft_size;
	uint32		fft_bins;
	float		fft_scaler;
	uint32		overlap;
	bool		low_cpu;

	fftr_state<float>		FFT;
	inv_fftr_state<float>	IFFT;

	dynamic_array<dynamic_array<fft_complex> >	filters[2];
	dynamic_array<float>						overlap_buffer[2];

public:
	AmbisonicBinauralizer() : block_size(0), taps(0), fft_size(0), fft_bins(0), fft_scaler(0), overlap(0), low_cpu(true) {}

	bool Configure(uint32 order, bool b3D, const HRTF &hrtf, uint32 _block_size);

	void Reset() {
		memset(overlap_buffer[0], 0, overlap * sizeof(float));
		memset(overlap_buffer[1], 0, overlap * sizeof(float));
	}

	void Process(BFormat* src, float **dst);
};

bool AmbisonicBinauralizer::Configure(uint32 order, bool b3D, const HRTF &hrtf, uint32 _block_size) {
	if (!AmbisonicBase::Configure(order, b3D))
		return false;

	taps		= hrtf.size();
	block_size	= _block_size;
	overlap		= block_size < taps ? block_size - 1 : taps - 1;
	fft_size	= next_pow2(block_size + taps + overlap);
	fft_bins	= fft_size / 2 + 1;
	fft_scaler	= 1.f / fft_size;

	//Position speakers and recalculate coefficients
	decoder.Configure(order, is3D, order == 1 ? SpeakerCube2 : SpeakerDodecahedron, OrderToSpeakers(order, is3D));
	decoder.Refresh();

	uint32 speakers = decoder.GetSpeakerCount();

	//Allocate overlap-add buffers
	overlap_buffer[0].resize(overlap);
	overlap_buffer[1].resize(overlap);

	//Allocate FFT and iFFT for new size
	FFT.init(fft_size);
	IFFT.init(fft_size);

	//Allocate the FFTBins for each channel, for each ear
	for (uint32 ear = 0; ear < 2; ear++) {
		filters[ear].resize(channels);
		for (uint32 c = 0; c < channels; c++)
			filters[ear][c].resize(fft_bins);
	}

	//Allocate buffers for HRTF accumulators
	dynamic_array<dynamic_array<float> > accumulator[2];
	for (uint32 ear = 0; ear < 2; ear++) {
		accumulator[ear].resize(channels);
		for (uint32 c = 0; c < channels; c++)
			accumulator[ear][c].resize(taps, 0);
	}

	float		*pfHRTF[2] = {
		alloc_auto(float, taps),
		alloc_auto(float, taps),
	};

	for (uint32 c = 0; c < channels; c++) {
		//coefficients are multiplied by (2*order + 1) to provide the correct decoder for SN3D normalised Ambisonic inputs
		float	scaler = 2 * floor(sqrt(c)) + 1;
		for (uint32 s = 0; s < speakers; s++) {
			//What is the position of the current speaker
			PolarPoint	position = decoder.GetPosition(s);
			if (!hrtf.get(position.azimuth, position.elevation, pfHRTF))
				return false;

			//Scale the HRTFs by the coefficient of the current channel/component
			float coeff = decoder.GetCoefficient(s, c) * scaler;

			//Accumulate channel/component HRTF
			for (uint32 i = 0; i < taps; i++) {
				accumulator[0][c][i] += pfHRTF[0][i] * coeff;
				accumulator[1][c][i] += pfHRTF[1][i] * coeff;
			}
		}
	}

	AmbisonicEncoder myEncoder;
	myEncoder.Configure(order, true);

	myEncoder.SetPosition(PolarPoint(degrees(90.f), 0, 5));
	myEncoder.Refresh();

	dynamic_array<float> left_ear_90(taps, 0);
	for (uint32 c = 0; c < channels; c++)
		for (uint32 i = 0; i < taps; i++)
			left_ear_90[i] += myEncoder.GetCoefficient(c) * accumulator[0][c][i];

	//Find the maximum value for a source encoded at 90 degrees
	float fmax = 0;
	for (uint32 i = 0; i < taps; i++)
		fmax = max(fmax, abs(left_ear_90[i]));

	//Normalize to pre-defined value
	float scaler = 0.35f / fmax;
	for (uint32 ear = 0; ear < 2; ear++) {
		for (uint32 c = 0; c < channels; c++) {
			for (uint32 i = 0; i < taps; i++)
				accumulator[ear][c][i] *= scaler;
		}
	}

	//Convert frequency domain filters
	auto	scratch_a = alloc_auto(float, fft_size);
	for (uint32 ear = 0; ear < 2; ear++) {
		for (uint32 c = 0; c < channels; c++) {
			memcpy(scratch_a, accumulator[ear][c], taps * sizeof(float));
			memset(scratch_a + taps, 0, (fft_size - taps) * sizeof(float));
			FFT.process(scratch_a, filters[ear][c]);
		}
	}

	return true;
}

//	If CPU load needs to be reduced then perform the convolution for each of the Ambisonics/spherical harmonic decompositions of the loudspeakers HRTFs for the left ear
//	For the left ear the results of these convolutions are summed to give the ear signal
//	For the right ear signal, the properties of the spherical harmonic decomposition can be used to to create the ear signal
//	This is done by either adding or subtracting the correct channels
//	Channels 1, 4, 5, 9, 10 and 11 are subtracted from the accumulated signal; all others are added
//	For example, for a first order signal the ears are generated from:
//		SignalL = W x HRTF_W + Y x HRTF_Y + Z x HRTF_Z + X x HRTF_X
//		SignalR = W x HRTF_W - Y x HRTF_Y + Z x HRTF_Z + X x HRTF_X
//	where 'x' is a convolution, W/Y/Z/X are the Ambisonic signal channels and HRTF_x are the spherical harmonic decompositions of the virtual loudspeaker array HRTFs
//	This has the effect of assuming a completely symmetric head

void AmbisonicBinauralizer::Process(BFormat* src, float** dst) {
	auto	scratch_z = alloc_auto(fft_complex, fft_bins);
	auto	scratch_a = alloc_auto(float, fft_size);
	auto	scratch_b = alloc_auto(float, fft_size);
	auto	scratch_c = alloc_auto(float, fft_size);

	if (low_cpu) {
		// Perform the convolutions for the left ear and generate the right ear from a modified accumulation of these channels

		memset(scratch_a, 0, fft_size * sizeof(float));
		memset(scratch_c, 0, fft_size * sizeof(float));

		for (uint32 c = 0; c < channels; c++) {
			memcpy(scratch_b, src->Channel(c), block_size * sizeof(float));
			memset(scratch_b + block_size, 0, (fft_size - block_size) * sizeof(float));
			FFT.process(scratch_b, scratch_z);

			for (uint32 i = 0; i < fft_bins; i++)
				scratch_z[i] = scratch_z[i] * filters[0][c][i];

			IFFT.process(scratch_z, scratch_b);
			for (uint32 i = 0; i < fft_size; i++)
				scratch_a[i] += scratch_b[i];

			if (c == 1 || c == 4 || c == 5 || c == 9 || c == 10 || c == 11) {
				// Subtract certain channels (such as Y) to generate right ear
				for (uint32 i = 0; i < fft_size; i++)
					scratch_c[i] -= scratch_b[i];
			} else {
				for (uint32 i = 0; i < fft_size; i++)
					scratch_c[i] += scratch_b[i];
			}

		}
		for (uint32 i = 0; i < fft_size; i++) {
			scratch_a[i] *= fft_scaler;
			scratch_c[i] *= fft_scaler;
		}

		memcpy(dst[0], scratch_a, block_size * sizeof(float));
		memcpy(dst[1], scratch_c, block_size * sizeof(float));

		for (uint32 i = 0; i < overlap; i++) {
			dst[0][i] += overlap_buffer[0][i];
			dst[1][i] += overlap_buffer[1][i];
		}

		memcpy(overlap_buffer[0], scratch_a + block_size, overlap * sizeof(float));
		memcpy(overlap_buffer[1], scratch_c + block_size, overlap * sizeof(float));

	} else {
		// Perform the convolution on both ears. Potentially more realistic results but requires double the number of convolutions
		for (uint32 ear = 0; ear < 2; ear++) {
			memset(scratch_a, 0, fft_size * sizeof(float));
			for (uint32 c = 0; c < channels; c++) {
				memcpy(scratch_b, src->Channel(c), block_size * sizeof(float));
				memset(scratch_b + block_size, 0, (fft_size - block_size) * sizeof(float));
				FFT.process(scratch_b, scratch_z);

				for (uint32 i = 0; i < fft_bins; i++)
					scratch_z[i] = scratch_z[i] * filters[ear][c][i];

				IFFT.process(scratch_z, scratch_b);
				for (uint32 i = 0; i < fft_size; i++)
					scratch_a[i] += scratch_b[i];
			}
			for (uint32 i = 0; i < fft_size; i++)
				scratch_a[i] *= fft_scaler;

			memcpy(dst[ear], scratch_a, block_size * sizeof(float));
			for (uint32 i = 0; i < overlap; i++)
				dst[ear][i] += overlap_buffer[ear][i];
			memcpy(overlap_buffer[ear], scratch_a + block_size, overlap * sizeof(float));
		}
	}
}

//-----------------------------------------------------------------------------
// Ambisonic zoomer - used to apply a zoom effect into BFormat soundfields
//-----------------------------------------------------------------------------

class AmbisonicZoomer : public AmbisonicBase {
	AmbisonicDecoder		decoderFront;
	dynamic_array<float>	front;
	dynamic_array<float>	front_weighted;
	dynamic_array<float>	a_m;
	float					zoom;
	float					front_mic;
public:
	AmbisonicZoomer() : zoom(0) {}

	bool Configure(uint32 order, bool b3D) {
		if (!AmbisonicBase::Configure(order, b3D))
			return false;

		decoderFront.Configure(order, 1, SpeakerMono, 1);

		//Calculate all the speaker coefficients
		decoderFront.Refresh();

		front.resize(channels);
		front_weighted.resize(channels);
		a_m.resize(order + 1);

		// These weights a_m are applied to the channels of a corresponding order within the Ambisonics signals
		// When applied to the encoded channels and decoded to a particular loudspeaker direction they will create a virtual microphone pattern with no rear lobes
		// When used for decoding this is known as in-phase decoding
		for (uint32 i = 0; i < order + 1; i++)
			a_m[i] = (2 * i + 1) * factorial(order) * factorial(order + 1) / (factorial(order + i + 1) * factorial(order - i));

		for (uint32 c = 0; c < channels; c++) {
			front[c] = decoderFront.GetCoefficient(0, c);
			uint32	degree = (int)floor(sqrt(c));
			front_weighted[c] = front[c] * a_m[degree];
			// Normalisation factor
			front_mic += front[c] * front_weighted[c];
		}

		return true;
	}

	void Process(BFormat* dst, uint32 nSamples) {
		float	zoom_blend	= 1 - zoom;
		float	zoom_red	= sqrt(1 - zoom * zoom);

		for (uint32 s = 0; s < nSamples; s++) {
			float mic = 0;

			// virtual microphone with polar pattern narrowing as Ambisonic order increases
			for (uint32 c = 0; c < channels; c++)
				mic += front_weighted[c] * dst->Channel(c)[s];

			for (uint32 c = 0; c < channels; c++) {
				if (abs(front[c]) > 1e-6) {
					// Blend original channel with the virtual microphone pointed directly to the front
					// Only do this for Ambisonics components that aren't zero for an encoded frontal source
					dst->Channel(c)[s] = (zoom_blend * dst->Channel(c)[s] + front[c] * zoom * mic) / (zoom_blend + abs(zoom) * front_mic);
				} else {
					// reduce the level of the Ambisonic components that are zero for a frontal source
					dst->Channel(c)[s] = dst->Channel(c)[s] * zoom_red;
				}
			}
		}
	}

	void SetZoom(float _zoom)	{ zoom = min(_zoom, 0.99f); }
	float GetZoom()				{ return zoom; }
};

//-----------------------------------------------------------------------------
// Ambisonic processor - used to rotate the BFormat signal around all three axes
//-----------------------------------------------------------------------------

class AmbisonicProcessor : public AmbisonicBase {
private:
	void ProcessOrder1_3D(BFormat* dst, uint32 nSamples);
	void ProcessOrder2_3D(BFormat* dst, uint32 nSamples);
	void ProcessOrder3_3D(BFormat* dst, uint32 nSamples);
	void ProcessOrder1_2D(BFormat* dst, uint32 nSamples);
	void ProcessOrder2_2D(BFormat* dst, uint32 nSamples);
	void ProcessOrder3_2D(BFormat* dst, uint32 nSamples);

	void ShelfFilterOrder(BFormat* dst, uint32 nSamples);

protected:
	float3		orientation;	// in ZYZ convention
	bool		optimise;

	fftr_state<float> FFT_psych;
	inv_fftr_state<float> IFFT_psych;

	uint32	fft_size;
	uint32	block_size;
	uint32	taps;
	uint32	overlap;
	uint32	fft_bins;
	float	fft_scaler;

	dynamic_array<dynamic_array<float> >		overlap_buffer;
	dynamic_array<dynamic_array<fft_complex> >	pych_filters;

	float3	cos1, sin1;
	float3	cos2, sin2;
	float3	cos3, sin3;

public:
	AmbisonicProcessor() : orientation(zero) {}
	bool Configure(uint32 order, bool b3D, uint32 nBlockSize);
	void Reset() {
		for (uint32 i = 0; i < channels; i++)
			memset(overlap_buffer[i], 0, overlap * sizeof(float));
	}
	void Refresh();
	void Process(BFormat* dst, uint32 nSamples);

	void SetOrientation(float3 _orientation)	{ orientation = _orientation; }
	void SetOrientation(const float3x3 &mat)	{ orientation = to_zyz(mat); }
	float3 GetOrientation()						{ return orientation; }
};

struct impulse_response {
	int16 table[2 + 3 + 4][101];
	constexpr const int16*	get_row(int order, int i) const {
		return table[order * (order + 1) / 2 - 1 + i];
	}
};

const impulse_response ir3D = {{
//const int16 first_order_3D[][101] = {
	{3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 2, 1, 0, -1, -3, -5, -7, -10, -13, -17, -21, -25, -30, -35, -41, -47, -54, -60, -67, -75, -82, -90, -97, -105, -113, -120, -127, -134, -141, -147, -153, -158, -163, -167, -170, -173, -175, -176, 23040, -176, -175, -173, -170, -167, -163, -158, -153, -147, -141, -134, -127, -120, -113, -105, -97, -90, -82, -75, -67, -60, -54, -47, -41, -35, -30, -25, -21, -17, -13, -10, -7, -5, -3, -1, 0, 1, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
	{-2, -2, -2, -2, -2, -2, -2, -3, -2, -2, -2, -2, -2, -1, -1, 0, 1, 2, 3, 5, 7, 9, 11, 14, 16, 19, 23, 26, 30, 34, 38, 42, 46, 51, 55, 60, 64, 68, 73, 77, 80, 84, 87, 90, 93, 95, 97, 99, 100, 101, 13438, 101, 100, 99, 97, 95, 93, 90, 87, 84, 80, 77, 73, 68, 64, 60, 55, 51, 46, 42, 38, 34, 30, 26, 23, 19, 16, 14, 11, 9, 7, 5, 3, 2, 1, 0, -1, -1, -2, -2, -2, -2, -2, -3, -2, -2, -2, -2, -2, -2, -2},
//};
//const int16 second_order_3D[][101] = {
	{-5, -5, -6, -6, -7, -7, -7, -7, -7, -6, -5, -3, -1, 2, 6, 10, 15, 21, 26, 32, 38, 44, 49, 53, 55, 56, 54, 50, 42, 32, 19, 2, -19, -43, -70, -100, -133, -167, -203, -241, -278, -315, -350, -384, -414, -442, -465, -484, -497, -506, 25438, -506, -497, -484, -465, -442, -414, -384, -350, -315, -278, -241, -203, -167, -133, -100, -70, -43, -19, 2, 19, 32, 42, 50, 54, 56, 55, 53, 49, 44, 38, 32, 26, 21, 15, 10, 6, 2, -1, -3, -5, -6, -7, -7, -7, -7, -7, -6, -6, -5, -5},
	{-2, -2, -3, -3, -3, -3, -3, -3, -3, -3, -2, -2, -1, 0, 2, 4, 6, 8, 10, 12, 15, 17, 19, 20, 21, 21, 21, 19, 16, 12, 7, 0, -8, -17, -27, -39, -52, -65, -79, -94, -108, -123, -136, -149, -161, -172, -181, -188, -194, -197, 19884, -197, -194, -188, -181, -172, -161, -149, -136, -123, -108, -94, -79, -65, -52, -39, -27, -17, -8, 0, 7, 12, 16, 19, 21, 21, 21, 20, 19, 17, 15, 12, 10, 8, 6, 4, 2, 0, -1, -2, -2, -3, -3, -3, -3, -3, -3, -3, -3, -2, -2},
	{2, 3, 3, 3, 3, 4, 4, 4, 4, 3, 2, 1, 0, -2, -4, -7, -10, -14, -17, -21, -25, -28, -31, -34, -35, -36, -35, -32, -27, -21, -12, -2, 11, 26, 43, 62, 82, 104, 127, 150, 173, 196, 219, 240, 259, 276, 290, 302, 311, 316, 10659, 316, 311, 302, 290, 276, 259, 240, 219, 196, 173, 150, 127, 104, 82, 62, 43, 26, 11, -2, -12, -21, -27, -32, -35, -36, -35, -34, -31, -28, -25, -21, -17, -14, -10, -7, -4, -2, 0, 1, 2, 3, 4, 4, 4, 4, 3, 3, 3, 3, 2},
//};
//const int16 third_order_3D[][101] = {
	{1, 3, 4, 5, 6, 8, 8, 8, 8, 7, 5, 1, -3, -8, -15, -21, -28, -34, -38, -41, -41, -38, -31, -20, -5, 13, 33, 56, 78, 99, 117, 130, 136, 133, 120, 95, 57, 8, -54, -126, -206, -294, -384, -476, -564, -646, -718, -778, -823, -850, 26604, -850, -823, -778, -718, -646, -564, -476, -384, -294, -206, -126, -54, 8, 57, 95, 120, 133, 136, 130, 117, 99, 78, 56, 33, 13, -5, -20, -31, -38, -41, -41, -38, -34, -28, -21, -15, -8, -3, 1, 5, 7, 8, 8, 8, 8, 6, 5, 4, 3, 1},
	{1, 2, 2, 3, 4, 5, 5, 5, 5, 4, 3, 1, -2, -6, -10, -14, -18, -22, -25, -27, -27, -25, -20, -13, -4, 8, 22, 36, 51, 65, 77, 85, 89, 87, 78, 62, 37, 5, -35, -82, -135, -192, -252, -311, -369, -423, -470, -509, -538, -556, 23082, -556, -538, -509, -470, -423, -369, -311, -252, -192, -135, -82, -35, 5, 37, 62, 78, 87, 89, 85, 77, 65, 51, 36, 22, 8, -4, -13, -20, -25, -27, -27, -25, -22, -18, -14, -10, -6, -2, 1, 3, 4, 5, 5, 5, 5, 4, 3, 2, 2, 1},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, -1, -1, -1, -1, -2, -2, -2, -2, -2, -2, -1, -1, 0, 1, 1, 2, 3, 4, 4, 4, 4, 4, 3, 1, 0, -2, -5, -8, -11, -14, -17, -20, -23, -25, -27, -29, -30, 16773, -30, -29, -27, -25, -23, -20, -17, -14, -11, -8, -5, -2, 0, 1, 3, 4, 4, 4, 4, 4, 3, 2, 1, 1, 0, -1, -1, -2, -2, -2, -2, -2, -2, -1, -1, -1, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{-2, -3, -4, -5, -6, -6, -7, -7, -7, -6, -4, -2, 1, 5, 10, 15, 19, 24, 27, 29, 29, 27, 22, 14, 3, -10, -25, -42, -58, -74, -87, -96, -101, -98, -88, -70, -43, -7, 38, 91, 150, 214, 280, 347, 412, 472, 525, 568, 601, 621, 8977, 621, 601, 568, 525, 472, 412, 347, 280, 214, 150, 91, 38, -7, -43, -70, -88, -98, -101, -96, -87, -74, -58, -42, -25, -10, 3, 14, 22, 27, 29, 29, 27, 24, 19, 15, 10, 5, 1, -2, -4, -6, -7, -7, -7, -6, -6, -5, -4, -3, -2},
}};


const impulse_response ir2D = {{
//	const int16 first_order_2D[][101] = {
	{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, -1, -2, -3, -4, -5, -7, -9, -10, -13, -15, -18, -21, -24, -27, -30, -34, -37, -41, -45, -49, -52, -56, -60, -63, -67, -70, -73, -76, -79, -81, -83, -85, -86, -87, -88, 19968, -88, -87, -86, -85, -83, -81, -79, -76, -73, -70, -67, -63, -60, -56, -52, -49, -45, -41, -37, -34, -30, -27, -24, -21, -18, -15, -13, -10, -9, -7, -5, -4, -3, -2, -1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
	{-2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -1, -1, -1, 0, 1, 1, 2, 4, 5, 7, 8, 10, 12, 15, 17, 20, 23, 26, 29, 32, 35, 39, 42, 46, 49, 52, 55, 58, 61, 64, 67, 69, 71, 73, 74, 76, 76, 77, 14259, 77, 76, 76, 74, 73, 71, 69, 67, 64, 61, 58, 55, 52, 49, 46, 42, 39, 35, 32, 29, 26, 23, 20, 17, 15, 12, 10, 8, 7, 5, 4, 2, 1, 1, 0, -1, -1, -1, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2},
//};
//const int16 second_order_2D[][101] = {
	{-3, -3, -3, -3, -4, -4, -4, -4, -4, -3, -3, -2, -1, 1, 3, 5, 7, 10, 13, 16, 19, 22, 24, 26, 27, 28, 27, 25, 21, 16, 9, 1, -10, -22, -35, -50, -67, -84, -102, -121, -139, -158, -176, -192, -208, -222, -233, -243, -249, -254, 20905, -254, -249, -243, -233, -222, -208, -192, -176, -158, -139, -121, -102, -84, -67, -50, -35, -22, -10, 1, 9, 16, 21, 25, 27, 28, 27, 26, 24, 22, 19, 16, 13, 10, 7, 5, 3, 1, -1, -2, -3, -3, -4, -4, -4, -4, -4, -3, -3, -3, -3},
	{-1, -1, -2, -2, -2, -2, -2, -2, -2, -2, -1, -1, -1, 0, 1, 2, 3, 4, 5, 6, 8, 9, 10, 10, 11, 11, 11, 10, 8, 6, 3, 0, -4, -9, -15, -21, -28, -35, -42, -50, -58, -65, -72, -79, -86, -91, -96, -100, -103, -104, 18220, -104, -103, -100, -96, -91, -86, -79, -72, -65, -58, -50, -42, -35, -28, -21, -15, -9, -4, 0, 3, 6, 8, 10, 11, 11, 11, 10, 10, 9, 8, 6, 5, 4, 3, 2, 1, 0, -1, -1, -1, -2, -2, -2, -2, -2, -2, -2, -2, -1, -1},
	{2, 2, 3, 3, 3, 3, 4, 4, 3, 3, 2, 1, 0, -2, -4, -7, -10, -13, -17, -20, -24, -27, -30, -33, -34, -34, -33, -31, -26, -20, -12, -2, 11, 25, 41, 59, 79, 100, 122, 144, 166, 189, 210, 230, 249, 265, 279, 290, 298, 303, 10885, 303, 298, 290, 279, 265, 249, 230, 210, 189, 166, 144, 122, 100, 79, 59, 41, 25, 11, -2, -12, -20, -26, -31, -33, -34, -34, -33, -30, -27, -24, -20, -17, -13, -10, -7, -4, -2, 0, 1, 2, 3, 3, 4, 4, 3, 3, 3, 3, 2, 2},
//};
//const int16 third_order_2D[][101] = {
	{0, 1, 2, 2, 3, 3, 4, 4, 4, 3, 2, 0, -2, -4, -7, -10, -13, -16, -18, -20, -20, -18, -15, -10, -3, 6, 16, 26, 37, 47, 56, 62, 64, 63, 57, 45, 27, 3, -26, -60, -98, -140, -183, -226, -268, -307, -342, -370, -391, -404, 21262, -404, -391, -370, -342, -307, -268, -226, -183, -140, -98, -60, -26, 3, 27, 45, 57, 63, 64, 62, 56, 47, 37, 26, 16, 6, -3, -10, -15, -18, -20, -20, -18, -16, -13, -10, -7, -4, -2, 0, 2, 3, 4, 4, 4, 3, 3, 2, 2, 1, 0},
	{0, 1, 1, 1, 2, 2, 2, 2, 2, 2, 1, 0, -1, -3, -5, -7, -9, -11, -13, -14, -14, -13, -10, -7, -2, 4, 10, 18, 25, 32, 38, 42, 44, 43, 39, 30, 18, 2, -18, -41, -68, -96, -126, -155, -184, -211, -234, -254, -268, -277, 19741, -277, -268, -254, -234, -211, -184, -155, -126, -96, -68, -41, -18, 2, 18, 30, 39, 43, 44, 42, 38, 32, 25, 18, 10, 4, -2, -7, -10, -13, -14, -14, -13, -11, -9, -7, -5, -3, -1, 0, 1, 2, 2, 2, 2, 2, 2, 1, 1, 1, 0},
	{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0, 0, 1, 2, 2, 3, 3, 4, 4, 3, 3, 1, 0, -2, -4, -6, -8, -10, -12, -14, -14, -14, -12, -10, -6, -1, 5, 12, 20, 29, 38, 47, 56, 64, 71, 77, 82, 84, 15409, 84, 82, 77, 71, 64, 56, 47, 38, 29, 20, 12, 5, -1, -6, -10, -12, -14, -14, -14, -12, -10, -8, -6, -4, -2, 0, 1, 3, 3, 4, 4, 3, 3, 2, 2, 1, 0, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{-2, -3, -4, -5, -6, -6, -7, -7, -7, -6, -4, -2, 1, 5, 10, 15, 20, 24, 27, 29, 29, 27, 22, 14, 3, -10, -25, -42, -58, -74, -87, -97, -101, -99, -89, -71, -43, -7, 39, 92, 151, 215, 282, 350, 415, 475, 528, 572, 605, 625, 8926, 625, 605, 572, 528, 475, 415, 350, 282, 215, 151, 92, 39, -7, -43, -71, -89, -99, -101, -97, -87, -74, -58, -42, -25, -10, 3, 14, 22, 27, 29, 29, 27, 24, 20, 15, 10, 5, 1, -2, -4, -6, -7, -7, -7, -6, -6, -5, -4, -3, -2},
}};

bool AmbisonicProcessor::Configure(uint32 order, bool b3D, uint32 nBlockSize) {
	if (!AmbisonicBase::Configure(order, b3D))
		return false;

	// This bool should be set as a user option to turn optimisation on and off
	optimise	= true;

	// All optimisation filters have the same number of taps so take from the first order 3D impulse response arbitrarily
	taps		= num_elements32(ir3D.table[0]);
	block_size	= nBlockSize;
	overlap		= block_size < taps ? block_size - 1 : taps - 1;
	fft_size	= next_pow2(block_size + taps + overlap);
	fft_bins	= fft_size / 2 + 1;
	fft_scaler	= 1.f / fft_size;

	//Allocate buffers
	overlap_buffer.resize(channels);
	for (uint32 i = 0; i < channels; i++)
		overlap_buffer[i].resize(overlap);

//	scratch_a.resize(fft_size);
	pych_filters.resize(order + 1);
	for (uint32 i = 0; i <= order; i++)
		pych_filters[i].resize(fft_bins);

//	scratch_z.resize(fft_bins);

	//temporary buffers for retrieving taps of psychoacoustic opimisation filters
	dynamic_array<dynamic_array<float> > psych_ir(order + 1);
	for (uint32 i = 0; i <= order; i++)
		psych_ir[i].resize(taps);

	Reset();

	//Allocate FFT and iFFT for new size
	FFT_psych.init(fft_size);
	IFFT_psych.init(fft_size);

	//get impulse responses for psychoacoustic optimisation based on playback system (2D or 3D) and playback order (1 to 3)
	auto	scratch_a	= alloc_auto(float, fft_size);
	for (uint32 o = 0; o <= order; o++) {
		const int16 *ir = (is3D ? ir3D : ir2D).get_row(order, o);
		for (uint32 i = 0; i < taps; i++)
			psych_ir[o][i] = 2 * ir[i] / 32767.f;

		// Convert the impulse responses to the frequency domain
		memcpy(scratch_a, psych_ir[o], taps * sizeof(float));
		memset(scratch_a + taps, 0, (fft_size - taps) * sizeof(float));
		FFT_psych.process(scratch_a, pych_filters[o]);
	}

	return true;
}

void AmbisonicProcessor::Refresh() {
	// Trig terms used multiple times in rotation equations
	sincos(orientation, &sin1, &cos1);
	sincos(orientation * two, &sin2, &cos2);
	sincos(orientation * 3, &sin3, &cos3);
}

// Rotate the sound scene based on the rotation angle from the 360 video
void AmbisonicProcessor::Process(BFormat* dst, uint32 nSamples) {
	// Before the rotation we apply the psychoacoustic optimisation filters
	if (optimise)
		ShelfFilterOrder(dst, nSamples);

	// 3D Ambisonics input expected so perform 3D rotations
	if (order >= 1)
		ProcessOrder1_3D(dst, nSamples);
	if (order >= 2)
		ProcessOrder2_3D(dst, nSamples);
	if (order >= 3)
		ProcessOrder3_3D(dst, nSamples);
}

void AmbisonicProcessor::ProcessOrder1_3D(BFormat* dst, uint32 nSamples) {
//	Rotations are performed in the following order:
//		1 - rotation around the z-axis
//		2 - rotation around the *new* y-axis (y')
//		3 - rotation around the new z-axis (z'')
//	The rotation equations used here work for third order; however, for higher orders a recursive algorithm should be considered

	for (uint32 s = 0; s < nSamples; s++) {
		// alpha rotation
		float	tY = -dst->Channel(kX)[s] * sin1.x + dst->Channel(kY)[s] * cos1.x;
		float	tZ = dst->Channel(kZ)[s];
		float	tX = dst->Channel(kX)[s] * cos1.x + dst->Channel(kY)[s] * sin1.x;

		// beta rotation
		dst->Channel(kY)[s] = tY;
		dst->Channel(kZ)[s] = tZ * cos1.y + tX * sin1.y;
		dst->Channel(kX)[s] = tX * cos1.y - tZ * sin1.y;

		// gamma rotation
		tY = -dst->Channel(kX)[s] * sin1.z + dst->Channel(kY)[s] * cos1.z;
		tZ =  dst->Channel(kZ)[s];
		tX =  dst->Channel(kX)[s] * cos1.z + dst->Channel(kY)[s] * sin1.z;

		dst->Channel(kX)[s] = tX;
		dst->Channel(kY)[s] = tY;
		dst->Channel(kZ)[s] = tZ;
	}
}

void AmbisonicProcessor::ProcessOrder2_3D(BFormat* dst, uint32 nSamples) {
	for (uint32 s = 0; s < nSamples; s++) {
		// alpha rotation
		float	tV = -dst->Channel(kU)[s] * sin2.x + dst->Channel(kV)[s] * cos2.x;
		float	tT = -dst->Channel(kS)[s] * sin1.x + dst->Channel(kT)[s] * cos1.x;
		float	tR =  dst->Channel(kR)[s];
		float	tS =  dst->Channel(kS)[s] * cos1.x + dst->Channel(kT)[s] * sin1.x;
		float	tU =  dst->Channel(kU)[s] * cos2.x + dst->Channel(kV)[s] * sin2.x;

		// beta rotation
		dst->Channel(kV)[s] = -sin1.y * tT + cos1.y * tV;
		dst->Channel(kT)[s] = -cos1.y * tT + sin1.y * tV;
		dst->Channel(kR)[s] = (0.75f * cos2.y + 0.25f) * tR + (fSqrt3 / 2 * square(sin1.y)) * tU + (fSqrt3 * sin1.y * cos1.y) * tS;
		dst->Channel(kS)[s] = cos2.y * tS - fSqrt3 * cos1.y * sin1.y * tR + cos1.y * sin1.y * tU;
		dst->Channel(kU)[s] = (0.25f * cos2.y + 0.75f) * tU - cos1.y * sin1.y * tS + fSqrt3 / 2 * square(sin1.y) * tR;

		// gamma rotation
		tV = -dst->Channel(kU)[s] * sin2.z + dst->Channel(kV)[s] * cos2.z;
		tT = -dst->Channel(kS)[s] * sin1.z + dst->Channel(kT)[s] * cos1.z;
		tR =  dst->Channel(kR)[s];
		tS =  dst->Channel(kS)[s] * cos1.z + dst->Channel(kT)[s] * sin1.z;
		tU =  dst->Channel(kU)[s] * cos2.z + dst->Channel(kV)[s] * sin2.z;

		dst->Channel(kR)[s] = tR;
		dst->Channel(kS)[s] = tS;
		dst->Channel(kT)[s] = tT;
		dst->Channel(kU)[s] = tU;
		dst->Channel(kV)[s] = tV;
	}
}

void AmbisonicProcessor::ProcessOrder3_3D(BFormat* dst, uint32 nSamples) {
	for (uint32 s = 0; s < nSamples; s++) {
		// alpha rotation
		float	tQ = -dst->Channel(kP)[s] * sin3.x + dst->Channel(kQ)[s] * cos3.x;
		float	tO = -dst->Channel(kN)[s] * sin2.x + dst->Channel(kO)[s] * cos2.x;
		float	tM = -dst->Channel(kL)[s] * sin1.x + dst->Channel(kM)[s] * cos1.x;
		float	tK =  dst->Channel(kK)[s];
		float	tL =  dst->Channel(kL)[s] * cos1.x + dst->Channel(kM)[s] * sin1.x;
		float	tN =  dst->Channel(kN)[s] * cos2.x + dst->Channel(kO)[s] * sin2.x;
		float	tP =  dst->Channel(kP)[s] * cos3.x + dst->Channel(kQ)[s] * sin3.x;

		// beta rotation
		dst->Channel(kQ)[s] = 0.125f * tQ * (5 + 3 * cos2.y) - fSqrt6 / 2 * tO * cos1.y * sin1.y + fSqrt15 / 4 * tM * square(sin1.y);
		dst->Channel(kO)[s] = tO * cos2.y - fSqrt10 / 2 * tM * cos1.y * sin1.y + fSqrt6 / 2 * tQ * cos1.y * sin1.y;
		dst->Channel(kM)[s] = 0.125f * tM * (3 + 5 * cos2.y) - fSqrt10 / 2 * tO * cos1.y * sin1.y + fSqrt15 / 4 * tQ * square(sin1.y);
		dst->Channel(kK)[s] = 0.25f * tK * cos1.y * (-1 + 15 * cos2.y) + 2 * fSqrt15 / 4 * tN * cos1.y * square(sin1.y) + 0.5f * fSqrt10 / 2 * tP * cube(sin1.y) + 0.125f * fSqrt6 / 2 * tL * (sin1.y + 5 * sin3.y);
		dst->Channel(kL)[s] = 0.0625f * tL * (cos1.y + 15 * cos3.y) + 0.25f * fSqrt10 / 2 * tN * (1 + 3 * cos2.y) * sin1.y + fSqrt15 / 4 * tP * cos1.y * square(sin1.y) - 0.125 * fSqrt6 / 2 * tK * (sin1.y + 5 * sin3.y);
		dst->Channel(kN)[s] = 0.125f * tN * (5 * cos1.y + 3 * cos3.y) + 0.25f * fSqrt6 / 2 * tP * (3 + cos2.y) * sin1.y + 2 * fSqrt15 / 4 * tK * cos1.y * square(sin1.y) + 0.125 * fSqrt10 / 2 * tL * (sin1.y - 3 * sin3.y);
		dst->Channel(kP)[s] = 0.0625f * tP * (15 * cos1.y + cos3.y) - 0.25f * fSqrt6 / 2 * tN * (3 + cos2.y) * sin1.y + fSqrt15 / 4 * tL * cos1.y * square(sin1.y) - 0.5 * fSqrt10 / 2 * tK * cube(sin1.y);

		// gamma rotation
		tQ = -dst->Channel(kP)[s] * sin3.z + dst->Channel(kQ)[s] * cos3.z;
		tO = -dst->Channel(kN)[s] * sin2.z + dst->Channel(kO)[s] * cos2.z;
		tM = -dst->Channel(kL)[s] * sin1.z + dst->Channel(kM)[s] * cos1.z;
		tK =  dst->Channel(kK)[s];
		tL =  dst->Channel(kL)[s] * cos1.z + dst->Channel(kM)[s] * sin1.z;
		tN =  dst->Channel(kN)[s] * cos2.z + dst->Channel(kO)[s] * sin2.z;
		tP =  dst->Channel(kP)[s] * cos3.z + dst->Channel(kQ)[s] * sin3.z;

		dst->Channel(kQ)[s] = tQ;
		dst->Channel(kO)[s] = tO;
		dst->Channel(kM)[s] = tM;
		dst->Channel(kK)[s] = tK;
		dst->Channel(kL)[s] = tL;
		dst->Channel(kN)[s] = tN;
		dst->Channel(kP)[s] = tP;
	}
}

// ACN/SN3D is generally only ever produced for 3D Ambisonics.
// If 2D Ambisonics is required then these equations need to be modified (they can be found in the 3D code for the first Z-rotation).
// Generally, 2D-only rotations do not make sense for use with 360 degree videos.
/*
void AmbisonicProcessor::ProcessOrder1_2D(BFormat* dst, uint32 nSamples) {
	for(uint32 s = 0; s < nSamples; s++) {
		//Yaw
		float	tX = dst->Channel(kX)[s] * cos_yaw - dst->Channel(kY)[s] * sin_yaw;
		float	tY = dst->Channel(kX)[s] * sin_yaw + dst->Channel(kY)[s] * cos_yaw;

		dst->Channel(kX)[s] = tX;
		dst->Channel(kY)[s] = tY;
	}
}

void AmbisonicProcessor::ProcessOrder2_2D(BFormat* dst, uint32 nSamples) {
	for(uint32 s = 0; s < nSamples; s++) {
		//Yaw
		float	tS = dst->Channel(kS)[s] * cos_yaw - dst->Channel(kT)[s] * sin_yaw;
		float	tT = dst->Channel(kS)[s] * sin_yaw + dst->Channel(kT)[s] * cos_yaw;
		float	tU = dst->Channel(kU)[s] * cos_2Yaw - dst->Channel(kV)[s] * sin_2Yaw;
		float	tV = dst->Channel(kU)[s] * sin_2Yaw + dst->Channel(kV)[s] * cos_2Yaw;

		dst->Channel(kS)[s] = tS;
		dst->Channel(kT)[s] = tT;
		dst->Channel(kU)[s] = tU;
		dst->Channel(kV)[s] = tV;
	}
}

void AmbisonicProcessor::ProcessOrder3_2D(BFormat* dst, uint32 nSamples) {
	//TODO
}
*/

void AmbisonicProcessor::ShelfFilterOrder(BFormat* dst, uint32 nSamples) {
	// Filter the Ambisonics channels
	// All channels are filtered using linear phase FIR filters
	// In the case of the 0th order signal (W channel) this takes the form of a delay; for all other channels shelf filters are used

	auto	scratch_a		= alloc_auto(float, fft_size);
	auto	scratch_z	= alloc_auto(fft_complex, fft_bins);

	memset(scratch_a, 0, fft_size * sizeof(float));

	for (uint32 c = 0; c < channels; c++) {
		uint32	chan_order = int(sqrt(c));    //get the order of the current channel

		memcpy(scratch_a, dst->Channel(c), block_size * sizeof(float));
		memset(scratch_a + block_size, 0, (fft_size - block_size) * sizeof(float));

		FFT_psych.process(scratch_a, scratch_z);

		// Perform the convolution in the frequency domain
		for (uint32 ni = 0; ni < fft_bins; ni++) {
			scratch_z[ni] = fft_complex(
				scratch_z[ni].r * pych_filters[chan_order][ni].i + scratch_z[ni].i * pych_filters[chan_order][ni].r,
				scratch_z[ni].r * pych_filters[chan_order][ni].r - scratch_z[ni].i * pych_filters[chan_order][ni].i
			);
		}

		// Convert from frequency domain back to time domain
		IFFT_psych.process(scratch_z, scratch_a);
		for (uint32 ni = 0; ni < fft_size; ni++)
			scratch_a[ni] *= fft_scaler;

		memcpy(dst->Channel(c), scratch_a, block_size * sizeof(float));
		for (uint32 ni = 0; ni < overlap; ni++)
			dst->Channel(c)[ni] += overlap_buffer[c][ni];

		memcpy(overlap_buffer[c], scratch_a + block_size, overlap * sizeof(float));
	}
}

//-----------------------------------------------------------------------------
// Ambisonic test
//-----------------------------------------------------------------------------

struct AmbisonicTest {

	AmbisonicTest() {
		// Generation of mono test signal
		float sinewave[512];
		for (int ni = 0; ni < 512; ni++)
			sinewave[ni] = (float)sin((ni / 128.f) * (pi * 2));

		// BFormat as 1st order 3D, and 512 samples
		BFormat myBFormat;
		myBFormat.Configure(1, true, 512);

		// Ambisonic encoder, also 3rd order 3D
		AmbisonicEncoder myEncoder;
		myEncoder.Configure(1, true);

		// Set test signal's position in the soundfield
		myEncoder.SetPosition(PolarPoint(0, 0, 5));
		myEncoder.Refresh();

		// Encode test signal into BFormat buffer
		myEncoder.Process(sinewave, 512, &myBFormat);

		// Ambisonic decoder, also 1st order 3D, for a 5.0 setup
		AmbisonicDecoder myDecoder;
		myDecoder.Configure(1, true, Speaker50, 5);

		// Allocate buffers for speaker feeds
		float* ppfSpeakerFeeds[5];
		for (int niSpeaker = 0; niSpeaker < 5; niSpeaker++)
			ppfSpeakerFeeds[niSpeaker] = new float[512];

		// Decode to get the speaker feeds
		myDecoder.Process(&myBFormat, 512, ppfSpeakerFeeds);

		AmbisonicBinauralizer	myBin;
		myBin.Configure(1, true, HRTF(44100), 512);
		myBin.Process(&myBFormat, ppfSpeakerFeeds);

		// De-allocate speaker feed buffers
		for (int niSpeaker = 0; niSpeaker < 5; niSpeaker++)
			delete[] ppfSpeakerFeeds[niSpeaker];
	}
};// ambisonictest;