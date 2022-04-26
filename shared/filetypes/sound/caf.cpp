#include "samplefile.h"

namespace caf {

typedef	iso::packed<iso::uint16be>	uint16;
typedef	iso::packed<iso::uint32be>	uint32;
typedef	iso::packed<iso::uint64be>	uint64;
typedef	iso::packed<iso::int16be>	int16;
typedef	iso::packed<iso::int32be>	int32;
typedef	iso::packed<iso::int64be>	int64;
typedef	iso::packed<iso::float32be>	float32;
typedef	iso::packed<iso::float64be>	float64;
using iso::int8;
using iso::uint8;

enum {
	kAudioFormatLinearPCM					= 'lpcm',
	kAudioFormatAppleIMA4					= 'ima4',
	kAudioFormatMPEG4AAC					= 'aac ',
	kAudioFormatMACE3						= 'MAC3',
	kAudioFormatMACE6						= 'MAC6',
	kAudioFormatULaw						= 'ulaw',
	kAudioFormatALaw						= 'alaw',
	kAudioFormatMPEGLayer1					= '.mp1',
	kAudioFormatMPEGLayer2					= '.mp2',
	kAudioFormatMPEGLayer3					= '.mp3',
	kAudioFormatAppleLossless				= 'alac'
};

enum {
	kCAFLinearPCMFormatFlagIsFloat			= 1 << 0,
	kCAFLinearPCMFormatFlagIsLittleEndian	= 1 << 1
};
enum {
	kMP4Audio_AAC_LC_ObjectType				= 2
};
enum {
	kCAFChannelBit_Left						= 1 << 0,
	kCAFChannelBit_Right					= 1 << 1,
	kCAFChannelBit_Center					= 1 << 2,
	kCAFChannelBit_LFEScreen				= 1 << 3,
	kCAFChannelBit_LeftSurround				= 1 << 4,	// WAVE: "Back Left"
	kCAFChannelBit_RightSurround			= 1 << 5,	// WAVE: "Back Right"
	kCAFChannelBit_LeftCenter				= 1 << 6,
	kCAFChannelBit_RightCenter				= 1 << 7,
	kCAFChannelBit_CenterSurround			= 1 << 8,	// WAVE: "Back Center"
	kCAFChannelBit_LeftSurroundDirect		= 1 << 9,	// WAVE: "Side Left"
	kCAFChannelBit_RightSurroundDirect		= 1 << 10,	// WAVE: "Side Right"
	kCAFChannelBit_TopCenterSurround		= 1 << 11,
	kCAFChannelBit_VerticalHeightLeft		= 1 << 12,	// WAVE: "Top Front Left"
	kCAFChannelBit_VerticalHeightCenter		= 1 << 13,	// WAVE: "Top Front Center"
	kCAFChannelBit_VerticalHeightRight		= 1 << 14,	// WAVE: "Top Front Right"
	kCAFChannelBit_TopBackLeft				= 1 << 15,
	kCAFChannelBit_TopBackCenter			= 1 << 16,
	kCAFChannelBit_TopBackRight				= 1 << 17
};

enum {
	kCAFChannelLayoutTag_UseChannelDescriptions = (0<<16) | 0, 		// use the array of AudioChannelDescriptions to define the mapping.
	kCAFChannelLayoutTag_UseChannelBitmap		= (1<<16) | 0, 		// use the bitmap to define the mapping.

// 1 Channel Layout
	kCAFChannelLayoutTag_Mono					= (100<<16) | 1,	// a standard mono stream

// 2 Channel layouts
	kCAFChannelLayoutTag_Stereo					= (101<<16) | 2,	// a standard stereo stream (L R)
	kCAFChannelLayoutTag_StereoHeadphones		= (102<<16) | 2, 	// a standard stereo stream (L R) - implied headphone playback
	kCAFChannelLayoutTag_MatrixStereo			= (103<<16) | 2,	// a matrix encoded stereo stream (Lt, Rt)
	kCAFChannelLayoutTag_MidSide				= (104<<16) | 2, 	// mid/side recording
	kCAFChannelLayoutTag_XY						= (105<<16) | 2, 	// coincident mic pair (often 2 figure 8's)
	kCAFChannelLayoutTag_Binaural				= (106<<16) | 2, 	// binaural stereo (left, right)

// Symmetric arrangements - same distance between speaker locations
	kCAFChannelLayoutTag_Ambisonic_B_Format		= (107<<16) | 4, 	// W, X, Y, Z
	kCAFChannelLayoutTag_Quadraphonic			= (108<<16) | 4, 	// front left, front right, back left, back right
	kCAFChannelLayoutTag_Pentagonal				= (109<<16) | 5, 	// left, right, rear left, rear right, center
	kCAFChannelLayoutTag_Hexagonal				= (110<<16) | 6, 	// left, right, rear left, rear right, center, rear
	kCAFChannelLayoutTag_Octagonal				= (111<<16) | 8, 	// front left, front right, rear left, rear right, front center, rear center, side left, side right
	kCAFChannelLayoutTag_Cube					= (112<<16) | 8, 	// left, right, rear left, rear right top left, top right, top rear left, top rear right

//	MPEG defined layouts
	kCAFChannelLayoutTag_MPEG_1_0				= kCAFChannelLayoutTag_Mono,	//	C
	kCAFChannelLayoutTag_MPEG_2_0				= kCAFChannelLayoutTag_Stereo,	//	L R
	kCAFChannelLayoutTag_MPEG_3_0_A				= (113<<16) | 3,	// L R C
	kCAFChannelLayoutTag_MPEG_3_0_B				= (114<<16) | 3,	// C L R
	kCAFChannelLayoutTag_MPEG_4_0_A				= (115<<16) | 4,	// L R C Cs
	kCAFChannelLayoutTag_MPEG_4_0_B				= (116<<16) | 4,	// C L R Cs
	kCAFChannelLayoutTag_MPEG_5_0_A				= (117<<16) | 5,	// L R C Ls Rs
	kCAFChannelLayoutTag_MPEG_5_0_B				= (118<<16) | 5,	// L R Ls Rs C
	kCAFChannelLayoutTag_MPEG_5_0_C				= (119<<16) | 5,	// L C R Ls Rs
	kCAFChannelLayoutTag_MPEG_5_0_D				= (120<<16) | 5,	// C L R Ls Rs
	kCAFChannelLayoutTag_MPEG_5_1_A				= (121<<16) | 6,	// L R C LFE Ls Rs
	kCAFChannelLayoutTag_MPEG_5_1_B				= (122<<16) | 6,	// L R Ls Rs C LFE
	kCAFChannelLayoutTag_MPEG_5_1_C				= (123<<16) | 6,	// L C R Ls Rs LFE
	kCAFChannelLayoutTag_MPEG_5_1_D				= (124<<16) | 6,	// C L R Ls Rs LFE
	kCAFChannelLayoutTag_MPEG_6_1_A				= (125<<16) | 7,	// L R C LFE Ls Rs Cs
	kCAFChannelLayoutTag_MPEG_7_1_A				= (126<<16) | 8,	// L R C LFE Ls Rs Lc Rc
	kCAFChannelLayoutTag_MPEG_7_1_B				= (127<<16) | 8,	// C Lc Rc L R Ls Rs LFE
	kCAFChannelLayoutTag_MPEG_7_1_C				= (128<<16) | 8,	// L R C LFE Ls R Rls Rrs
	kCAFChannelLayoutTag_Emagic_Default_7_1		= (129<<16) | 8, 	// L R Ls Rs C LFE Lc Rc
	kCAFChannelLayoutTag_SMPTE_DTV				= (130<<16) | 8, 	// L R C LFE Ls Rs Lt Rt 	(kCAFChannelLayoutTag_ITU_5_1 plus a matrix encoded stereo mix)

//	ITU defined layouts
	kCAFChannelLayoutTag_ITU_1_0				= kCAFChannelLayoutTag_Mono,		// C
	kCAFChannelLayoutTag_ITU_2_0				= kCAFChannelLayoutTag_Stereo,		// L R
	kCAFChannelLayoutTag_ITU_2_1				= (131<<16) | 3,					// L R Cs
	kCAFChannelLayoutTag_ITU_2_2				= (132<<16) | 4,					// L R Ls Rs
	kCAFChannelLayoutTag_ITU_3_0				= kCAFChannelLayoutTag_MPEG_3_0_A,	// L R C
	kCAFChannelLayoutTag_ITU_3_1				= kCAFChannelLayoutTag_MPEG_4_0_A,	// L R C Cs
	kCAFChannelLayoutTag_ITU_3_2				= kCAFChannelLayoutTag_MPEG_5_0_A,	// L R C Ls Rs
	kCAFChannelLayoutTag_ITU_3_2_1				= kCAFChannelLayoutTag_MPEG_5_1_A, 	// L R C LFE Ls Rs
	kCAFChannelLayoutTag_ITU_3_4_1				= kCAFChannelLayoutTag_MPEG_7_1_C, 	// L R C LFE Ls Rs Rls Rrs

// DVD defined layouts
	kCAFChannelLayoutTag_DVD_0					= kCAFChannelLayoutTag_Mono,		// C (mono)
	kCAFChannelLayoutTag_DVD_1					= kCAFChannelLayoutTag_Stereo,		// L R
	kCAFChannelLayoutTag_DVD_2					= kCAFChannelLayoutTag_ITU_2_1,		// L R Cs
	kCAFChannelLayoutTag_DVD_3					= kCAFChannelLayoutTag_ITU_2_2,		// L R Ls Rs
	kCAFChannelLayoutTag_DVD_4					= (133<<16) | 3,					// L R LFE
	kCAFChannelLayoutTag_DVD_5					= (134<<16) | 4,					// L R LFE Cs
	kCAFChannelLayoutTag_DVD_6					= (135<<16) | 5,					// L R LFE Ls Rs
	kCAFChannelLayoutTag_DVD_7					= kCAFChannelLayoutTag_MPEG_3_0_A,	// L R C
	kCAFChannelLayoutTag_DVD_8					= kCAFChannelLayoutTag_MPEG_4_0_A,	// L R C Cs
	kCAFChannelLayoutTag_DVD_9					= kCAFChannelLayoutTag_MPEG_5_0_A,	// L R C Ls Rs
	kCAFChannelLayoutTag_DVD_10					= (136<<16) | 4,					// L R C LFE
	kCAFChannelLayoutTag_DVD_11					= (137<<16) | 5,					// L R C LFE Cs
	kCAFChannelLayoutTag_DVD_12					= kCAFChannelLayoutTag_MPEG_5_1_A,	// L R C LFE Ls Rs
// 13 through 17 are duplicates of 8 through 12.
	kCAFChannelLayoutTag_DVD_13					= kCAFChannelLayoutTag_DVD_8,		// L R C Cs
	kCAFChannelLayoutTag_DVD_14					= kCAFChannelLayoutTag_DVD_9,		// L R C Ls Rs
	kCAFChannelLayoutTag_DVD_15					= kCAFChannelLayoutTag_DVD_10,		// L R C LFE
	kCAFChannelLayoutTag_DVD_16					= kCAFChannelLayoutTag_DVD_11,		// L R C LFE Cs
	kCAFChannelLayoutTag_DVD_17					= kCAFChannelLayoutTag_DVD_12,		// L R C LFE Ls Rs
	kCAFChannelLayoutTag_DVD_18					= (138<<16) | 5,					// L R Ls Rs LFE
	kCAFChannelLayoutTag_DVD_19					= kCAFChannelLayoutTag_MPEG_5_0_B,	// L R Ls Rs C
	kCAFChannelLayoutTag_DVD_20					= kCAFChannelLayoutTag_MPEG_5_1_B,	// L R Ls Rs C LFE

// These layouts are recommended for audio unit use
// These are the symmetrical layouts
	kCAFChannelLayoutTag_AudioUnit_4			= kCAFChannelLayoutTag_Quadraphonic,
	kCAFChannelLayoutTag_AudioUnit_5			= kCAFChannelLayoutTag_Pentagonal,
	kCAFChannelLayoutTag_AudioUnit_6			= kCAFChannelLayoutTag_Hexagonal,
	kCAFChannelLayoutTag_AudioUnit_8			= kCAFChannelLayoutTag_Octagonal,
// These are the surround-based layouts
	kCAFChannelLayoutTag_AudioUnit_5_0			= kCAFChannelLayoutTag_MPEG_5_0_B, 	// L R Ls Rs C
	kCAFChannelLayoutTag_AudioUnit_6_0			= (139<<16) | 6,					// L R Ls Rs C Cs
	kCAFChannelLayoutTag_AudioUnit_7_0			= (140<<16) | 7,					// L R Ls Rs C Rls Rrs
	kCAFChannelLayoutTag_AudioUnit_5_1			= kCAFChannelLayoutTag_MPEG_5_1_A, 	// L R C LFE Ls Rs
	kCAFChannelLayoutTag_AudioUnit_6_1			= kCAFChannelLayoutTag_MPEG_6_1_A, 	// L R C LFE Ls Rs Cs
	kCAFChannelLayoutTag_AudioUnit_7_1			= kCAFChannelLayoutTag_MPEG_7_1_C, 	// L R C LFE Ls Rs Rls Rrs

// These layouts are used for AAC Encoding within the MPEG-4 Specification
	kCAFChannelLayoutTag_AAC_Quadraphonic		= kCAFChannelLayoutTag_Quadraphonic,// L R Ls Rs
	kCAFChannelLayoutTag_AAC_4_0				= kCAFChannelLayoutTag_MPEG_4_0_B,	// C L R Cs
	kCAFChannelLayoutTag_AAC_5_0				= kCAFChannelLayoutTag_MPEG_5_0_D,	// C L R Ls Rs
	kCAFChannelLayoutTag_AAC_5_1				= kCAFChannelLayoutTag_MPEG_5_1_D,	// C L R Ls Rs Lfe
	kCAFChannelLayoutTag_AAC_6_0				= (141<<16) | 6,					// C L R Ls Rs Cs
	kCAFChannelLayoutTag_AAC_6_1				= (142<<16) | 7,					// C L R Ls Rs Cs Lfe
	kCAFChannelLayoutTag_AAC_7_0				= (143<<16) | 7,					// C L R Ls Rs Rls Rrs
	kCAFChannelLayoutTag_AAC_7_1				= kCAFChannelLayoutTag_MPEG_7_1_B, 	// C Lc Rc L R Ls Rs Lfe
	kCAFChannelLayoutTag_AAC_Octagonal			= (144<<16) | 8,					// C L R Ls Rs Rls Rrs Cs

	kCAFChannelLayoutTag_TMH_10_2_std			= (145<<16) | 16,					// L R C Vhc Lsd Rsd Ls Rs Vhl Vhr Lw Rw Csd Cs LFE1 LFE2
	kCAFChannelLayoutTag_TMH_10_2_full			= (146<<16) | 21,					// TMH_10_2_std plus: Lc Rc HI VI Haptic

	kCAFChannelLayoutTag_RESERVED_DO_NOT_USE	= (147<<16)
};
enum {
	kCAFChannelLabel_Unknown				= 0xFFFFFFFF, // unknown role or unspecified other use for channel
	kCAFChannelLabel_Unused					= 0,	// channel is present, but has no intended role or destination
	kCAFChannelLabel_UseCoordinates			= 100,	// channel is described solely by the mCoordinates fields

	kCAFChannelLabel_Left					= 1,
	kCAFChannelLabel_Right					= 2,
	kCAFChannelLabel_Center					= 3,
	kCAFChannelLabel_LFEScreen				= 4,
	kCAFChannelLabel_LeftSurround			= 5,	// WAVE (.wav files): "Back Left"
	kCAFChannelLabel_RightSurround			= 6,	// WAVE: "Back Right"
	kCAFChannelLabel_LeftCenter				= 7,
	kCAFChannelLabel_RightCenter			= 8,
	kCAFChannelLabel_CenterSurround			= 9,	// WAVE: "Back Center or plain "Rear Surround"
	kCAFChannelLabel_LeftSurroundDirect		= 10,	// WAVE: "Side Left"
	kCAFChannelLabel_RightSurroundDirect	= 11,	// WAVE: "Side Right"
	kCAFChannelLabel_TopCenterSurround		= 12,
	kCAFChannelLabel_VerticalHeightLeft		= 13,	// WAVE: "Top Front Left"
	kCAFChannelLabel_VerticalHeightCenter	= 14,	// WAVE: "Top Front Center"
	kCAFChannelLabel_VerticalHeightRight	= 15,	// WAVE: "Top Front Right"
	kCAFChannelLabel_TopBackLeft			= 16,
	kCAFChannelLabel_TopBackCenter			= 17,
	kCAFChannelLabel_TopBackRight			= 18,
	kCAFChannelLabel_RearSurroundLeft		= 33,
	kCAFChannelLabel_RearSurroundRight		= 34,
	kCAFChannelLabel_LeftWide				= 35,
	kCAFChannelLabel_RightWide				= 36,
	kCAFChannelLabel_LFE2					= 37,
	kCAFChannelLabel_LeftTotal				= 38,	// matrix encoded 4 channels
	kCAFChannelLabel_RightTotal				= 39,	// matrix encoded 4 channels
	kCAFChannelLabel_HearingImpaired		= 40,
	kCAFChannelLabel_Narration				= 41,
	kCAFChannelLabel_Mono					= 42,
	kCAFChannelLabel_DialogCentricMix		= 43,
	kCAFChannelLabel_CenterSurroundDirect	= 44,	// back center, non diffuse

// first order ambisonic channels
	kCAFChannelLabel_Ambisonic_W			= 200,
	kCAFChannelLabel_Ambisonic_X			= 201,
	kCAFChannelLabel_Ambisonic_Y			= 202,
	kCAFChannelLabel_Ambisonic_Z			= 203,

// Mid/Side Recording
	kCAFChannelLabel_MS_Mid					= 204,
	kCAFChannelLabel_MS_Side				= 205,

// X-Y Recording
	kCAFChannelLabel_XY_X					= 206,
	kCAFChannelLabel_XY_Y					= 207,

// other
	kCAFChannelLabel_HeadphonesLeft			= 301,
	kCAFChannelLabel_HeadphonesRight		= 302,
	kCAFChannelLabel_ClickTrack				= 304,
	kCAFChannelLabel_ForeignLanguage		= 305
};
enum {
	kCAFChannelFlags_AllOff					= 0,
	kCAFChannelFlags_RectangularCoordinates	= 1<<0,
	kCAFChannelFlags_SphericalCoordinates	= 1<<1,
	kCAFChannelFlags_Meters					= 1<<2
};
enum {
	kCAFMarkerType_Generic					= 0,
	kCAFMarkerType_ProgramStart				= 'pbeg',
	kCAFMarkerType_ProgramEnd				= 'pend',
	kCAFMarkerType_TrackStart				= 'tbeg',
	kCAFMarkerType_TrackEnd					= 'tend',
	kCAFMarkerType_Index					= 'indx',
	kCAFMarkerType_RegionStart				= 'rbeg',
	kCAFMarkerType_RegionEnd				= 'rend',
	kCAFMarkerType_RegionSyncPoint			= 'rsyc',
	kCAFMarkerType_SelectionStart			= 'sbeg',
	kCAFMarkerType_SelectionEnd				= 'send',
	kCAFMarkerType_EditSourceBegin			= 'cbeg',
	kCAFMarkerType_EditSourceEnd			= 'cend',
	kCAFMarkerType_EditDestinationBegin		= 'dbeg',
	kCAFMarkerType_EditDestinationEnd		= 'dend',
	kCAFMarkerType_SustainLoopStart			= 'slbg',
	kCAFMarkerType_SustainLoopEnd			= 'slen',
	kCAFMarkerType_ReleaseLoopStart			= 'rlbg',
	kCAFMarkerType_ReleaseLoopEnd			= 'rlen'
};
enum {
	kCAF_SMPTE_TimeTypeNone					= 0,
	kCAF_SMPTE_TimeType24					= 1,
	kCAF_SMPTE_TimeType25					= 2,
	kCAF_SMPTE_TimeType30Drop				= 3,
	kCAF_SMPTE_TimeType30					= 4,
	kCAF_SMPTE_TimeType2997					= 5,
	kCAF_SMPTE_TimeType2997Drop				= 6,
	kCAF_SMPTE_TimeType60					= 7,
	kCAF_SMPTE_TimeType5994					= 8
};

struct CAFFileHeader {
	uint32	mFileType;
	uint16	mFileVersion;
	uint16	mFileFlags;
};
struct CAFChunkHeader {
	uint32	mChunkType;
	int64	mChunkSize;
};
struct CAFAudioFormat {
	float64 mSampleRate;
	uint32	mFormatID;
	uint32	mFormatFlags;
	uint32	mBytesPerPacket;
	uint32	mFramesPerPacket;
	uint32	mChannelsPerFrame;
	uint32	mBitsPerChannel;
};
struct CAFData {
	uint32	mEditCount;	// initially set to 0
	uint8	mData [];
};
struct CAFPacketTableHeader {
	int64	mNumberPackets;
	int64	mNumberValidFrames;
	int32	mPrimingFrames;
	int32	mRemainderFrames;
};

struct CAFChannelDescription {
	uint32	mChannelLabel;
	uint32	mChannelFlags;
	float32	mCoordinates[3];
};
struct CAFChannelLayout {
	uint32	mChannelLayoutTag;
	uint32	mChannelBitmap;
	uint32	mNumberChannelDescriptions;
//	CAFChannelDescription	mChannelDescriptions[];
};

struct CAFStringID {
	uint32	mStringID;
	int64	mStringStartByteOffset;
};
struct CAFStrings {
	uint32	mNumEntries;
//	CAFStringID	mStringsIDs[];
//	uint8	mStrings[];
};

struct CAF_SMPTE_Time {
	int8	mHours;
	int8	mMinutes;
	int8	mSeconds;
	int8	mFrames;
	uint32	mSubFrameSampleOffset;
};
struct CAFMarker {
	uint32	mType;
	float64	mFramePosition;
	uint32	mMarkerID;
	CAF_SMPTE_Time	mSMPTETime;
	uint32	mChannel;
};

struct CAFMarkerChunk {
	uint32	mSMPTE_TimeType;
	uint32	mNumberMarkers;
//	CAFMarker	mMarkers[];
};

struct CAFRegion {
	uint32	mRegionID;
	uint32	mFlags;
	uint32	mNumberMarkers;
//	CAFMarker mMarkers[];
};
struct CAFRegionChunk {
	uint32	mSMPTE_TimeType;
	uint32	mNumberRegions;
//	CAFRegion	mRegions[];
};

enum {
	kCAFRegionFlag_LoopEnable	= 1,
	kCAFRegionFlag_PlayForward	= 2,
	kCAFRegionFlag_PlayBackward	= 4
};
struct CAFInstrumentChunk {
	float32	mBaseNote;
	uint8	mMIDILowNote;
	uint8	mMIDIHighNote;
	uint8	mMIDILowVelocity;
	uint8	mMIDIHighVelocity;
	float32	mdBGain;
	uint32	mStartRegionID;
	uint32	mSustainRegionID;
	uint32	mReleaseRegionID;
	uint32	mInstrumentID;
};
struct CAFOverviewSample {
	int16	mMinValue;
	int16	mMaxValue;
};
struct CAFOverview {
	uint32	mEditCount;
	uint32	mNumFramesPerOVWSample;
//	CAFOverviewSample	mData[];
};

struct CAFPositionPeak {
	float32	mValue;
	uint64	mFrameNumber;
};
struct CAFPeakChunk {
	uint32	mEditCount;
//	CAFPositionPeak	mPeaks[];
};

struct CAFCommentStringsChunk {
	uint32	mNumEntries;
//	CAFStringID	mStrings[];
};
struct editCommment {
//	uint8	mKey[];
//	uint8	mValue[];
};
struct CAFStringsChunk {
	uint32	mNumEntries;
//	CAFStringID	mStrings[];
};
struct CAFInformation {
//	uint8	mKey[];
//	uint8	mValue[];
};
struct CAFUMIDChunk {
	uint8 mBytes[64];
};
struct CAF_UUID_ChunkHeader : CAFChunkHeader {
	uint8	mUUID[16];
};
}

using namespace iso;

class CAFFileHandler : public SampleFileHandler {

	class chunk : public istream_chain {
		streamptr	endofchunk;
		void		GetHeader()	{
			caf::CAFChunkHeader	c;
			if (read(c)) {
				id = (uint32be&)c.mChunkType;
				if (streamptr size = c.mChunkSize)
					endofchunk = size + istream_chain::tell();
				else
					endofchunk = istream_chain::length();
			} else {
				id = 0;
				endofchunk = istream_chain::length();
			}
		}
	public:
		uint32be	id;

		chunk(istream_ref stream) : istream_chain(stream)		{ GetHeader();	}
		~chunk()					{ istream_chain::seek(endofchunk);					}
		streamptr	length()		{ return int(endofchunk - istream_chain::tell());	}
	};

	const char*		GetExt() override			{ return "caf";				}
	const char*		GetDescription() override	{ return "Core Audio File";	}

	ISO_ptr<void> Read(tag id, istream_ref file) override;

} caf_filehandler;

struct IMA4 {
	static int8		index_table[16];
	static int16	step_table[89];

	uint16be	header;
	uint8		data[32];

	int16*	decode(int16 *d) {
		int	step_index	= header & 0x7f;
		int	predictor	= int16(header & ~0x7f);

		for (int i = 0; i < 32; i++) {
			uint8	byte	= data[i];

			uint8	nibble	= byte >> 4;
			predictor	+= (nibble * 2 + 1) * step_table[step_index] / 8;
			step_index	= clamp(step_index + index_table[nibble], 0, 88);
			*d++		= clamp(predictor, -32768, 32767);

			nibble		= byte & 0xf;
			predictor	+= (nibble * 2 + 1) * step_table[step_index] / 8;
			step_index	= clamp(step_index + index_table[nibble], 0, 88);
			*d++		= clamp(predictor, -32768, 32767);
		}
		return d;
	}
};

int8 IMA4::index_table[16] = {
	-1, -1, -1, -1, 2, 4, 6, 8,
	-1, -1, -1, -1, 2, 4, 6, 8
};
int16 IMA4::step_table[89] = {
	7, 8, 9, 10, 11, 12, 13, 14, 16, 17,
	19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
	50, 55, 60, 66, 73, 80, 88, 97, 107, 118,
	130, 143, 157, 173, 190, 209, 230, 253, 279, 307,
	337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
	876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066,
	2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
	5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
	15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
};

ISO_ptr<void> CAFFileHandler::Read(tag id, istream_ref file) {
	caf::CAFFileHeader	head = file.get();
	if (head.mFileType != 'caff' || head.mFileVersion != 1)
		return ISO_NULL;

	caf::CAFAudioFormat			fmt;
	caf::CAFPacketTableHeader	pakt;
	streamptr	data_start;
	uint64		data_len;

	for (bool exit = false; !exit;) {
		chunk	c(file);
		switch (c.id) {
			case 'desc':
				fmt			= c.get();
				break;
			case 'pakt':
				pakt = c.get();
				break;
			case 'data':
				data_start	= file.tell();
				data_len	= c.length();
				break;
			case 0:
				exit		= true;
				break;
		}
	}

	file.seek(data_start);
	uint32	edit = file.get<uint32be>();

	ISO_ptr<sample> s(id);
	if (uint32 bpp = fmt.mBytesPerPacket) {
		int16	*p = s->Create(data_len / bpp * fmt.mFramesPerPacket, fmt.mChannelsPerFrame, 16);

		if (fmt.mFormatID == 'ima4') {
			for (int n = data_len / sizeof(IMA4); n--;) {
				IMA4	ima4 = file.get();
				p = ima4.decode(p);
			}
		}
//		for (int n = s->Length() * s->Channels(); --n; p++)
//			*p	= int16(clamp(file.get<floatle>(), -1.f, +1.f) * 32767);
	} else {
		switch (fmt.mFormatID) {
			case 'aac ':
				return Get("aac")->Read(id, file);
				break;
		}
	}

	s->SetFrequency(float(fmt.mSampleRate));
	return s;
}
