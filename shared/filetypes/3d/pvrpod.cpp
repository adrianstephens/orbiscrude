#include "iso/iso_files.h"
#include "iso/iso_convert.h"
#include "scenegraph.h"

using namespace iso;

template<typename T> bool SafeAlloc(T* &ptr, size_t cnt) {
	if (cnt) {
		ptr = (T*)iso::malloc(cnt * sizeof(T));
		if (!ptr)
			return false;
		memset(ptr, 0, cnt * sizeof(T));
	}
	return true;
}
template<typename T> void SafeFree(T* &ptr) {
	if (ptr)
		iso::free(ptr);
	ptr = 0;
}

#define VERTTYPE	float

#define PVRTMODELPOD_VERSION	"AB.POD.2.0"	/*!< POD file version string */
#define PVRTMODELPOD_TAG_MASK	0x80000000
#define PVRTMODELPOD_TAG_START	0x00000000
#define PVRTMODELPOD_TAG_END	0x80000000
#define PVRTMODELPODSF_FIXED	0x00000001		/*!< PVRTMODELPOD Fixed-point 16.16 data (otherwise float) flag */

class PVRTMATRIX {
public:
    VERTTYPE* operator [] ( const int Row ) { return &f[Row<<2]; }
	VERTTYPE f[16];
};

enum EPVRTError {
	PVR_SUCCESS = 0,
	PVR_FAIL = 1,
	PVR_OVERFLOW = 2
};

enum EPVRTDataType {
	EPODDataNone,
	EPODDataFloat,
	EPODDataInt,
	EPODDataUnsignedShort,
	EPODDataRGBA,
	EPODDataARGB,
	EPODDataD3DCOLOR,
	EPODDataUBYTE4,
	EPODDataDEC3N,
	EPODDataFixed16_16,
	EPODDataUnsignedByte,
	EPODDataShort,
	EPODDataShortNorm,
	EPODDataByte,
	EPODDataByteNorm,
	EPODDataUnsignedByteNorm,
	EPODDataUnsignedShortNorm,
	EPODDataUnsignedInt
};

enum EPODLightType{
	ePODPoint=0,	 /*!< Point light */
	ePODDirectional, /*!< Directional light */
	ePODSpot,		 /*!< Spot light */
	eNumPODLightTypes
};

enum EPODPrimitiveType {
	ePODTriangles=0, /*!< Triangles */
	eNumPODPrimitiveTypes
};

enum EPODAnimationData {
	ePODHasPositionAni	= 0x01,	/*!< Position animation */
	ePODHasRotationAni	= 0x02, /*!< Rotation animation */
	ePODHasScaleAni		= 0x04, /*!< Scale animation */
	ePODHasMatrixAni	= 0x08  /*!< Matrix animation */
};

enum EPODMaterialFlag {
	ePODEnableBlending	= 0x01	/*!< Enable blending for this material */
};

enum EPODBlendFunc {
	ePODBlendFunc_ZERO=0,
	ePODBlendFunc_ONE,
	ePODBlendFunc_BLEND_FACTOR,
	ePODBlendFunc_ONE_MINUS_BLEND_FACTOR,

	ePODBlendFunc_SRC_COLOR = 0x0300,
	ePODBlendFunc_ONE_MINUS_SRC_COLOR,
	ePODBlendFunc_SRC_ALPHA,
	ePODBlendFunc_ONE_MINUS_SRC_ALPHA,
	ePODBlendFunc_DST_ALPHA,
	ePODBlendFunc_ONE_MINUS_DST_ALPHA,
	ePODBlendFunc_DST_COLOR,
	ePODBlendFunc_ONE_MINUS_DST_COLOR,
	ePODBlendFunc_SRC_ALPHA_SATURATE,

	ePODBlendFunc_CONSTANT_COLOR = 0x8001,
	ePODBlendFunc_ONE_MINUS_CONSTANT_COLOR,
	ePODBlendFunc_CONSTANT_ALPHA,
	ePODBlendFunc_ONE_MINUS_CONSTANT_ALPHA
};

enum EPODBlendOp {
	ePODBlendOp_ADD = 0x8006,
	ePODBlendOp_MIN,
	ePODBlendOp_MAX,
	ePODBlendOp_SUBTRACT = 0x800A,
	ePODBlendOp_REVERSE_SUBTRACT
};

class CPODData {
public:
	void Reset();
public:
	EPVRTDataType	eType;		/*!< Type of data stored */
	uint32			n;			/*!< Number of values per vertex */
	uint32			nStride;	/*!< Distance in bytes from one array entry to the next */
	uint8			*pData;		/*!< Actual data (array of values); if mesh is interleaved, this is an OFFSET from pInterleaved */
};

struct SPODCamera {
	int32			nIdxTarget;			/*!< Index of the target object */
	VERTTYPE		fFOV;				/*!< Field of view */
	VERTTYPE		fFar;				/*!< Far clip plane */
	VERTTYPE		fNear;				/*!< Near clip plane */
	VERTTYPE		*pfAnimFOV;			/*!< 1 VERTTYPE per frame of animation. */
};

struct SPODLight {
	int32			nIdxTarget;		/*!< Index of the target object */
	VERTTYPE		pfColour[3];	/*!< Light colour (0.0f -> 1.0f for each channel) */
	EPODLightType	eType;			/*!< Light type (point, directional, spot etc.) */
	float			fConstantAttenuation;	/*!< Constant attenuation */
	float			fLinearAttenuation;		/*!< Linear atternuation */
	float			fQuadraticAttenuation;	/*!< Quadratic attenuation */
	float			fFalloffAngle;			/*!< Falloff angle (in radians) */
	float			fFalloffExponent;		/*!< Falloff exponent */
};

class CPVRTBoneBatches {
public:
	int	*pnBatches;			/*!< Space for nBatchBoneMax bone indices, per batch */
	int	*pnBatchBoneCnt;	/*!< Actual number of bone indices, per batch */
	int	*pnBatchOffset;		/*!< Offset into triangle array, per batch */
	int nBatchBoneMax;		/*!< Stored value as was passed into Create() */
	int	nBatchCnt;			/*!< Number of batches to render */

	void Release() {
		delete[] pnBatches;
		delete[] pnBatchBoneCnt;
		delete[] pnBatchOffset;
		nBatchCnt = 0;
	}
};

struct SPODMesh {
	uint32				nNumVertex;		/*!< Number of vertices in the mesh */
	uint32				nNumFaces;		/*!< Number of triangles in the mesh */
	uint32				nNumUVW;		/*!< Number of texture coordinate channels per vertex */
	CPODData			sFaces;			/*!< List of triangle indices */
	uint32				*pnStripLength;	/*!< If mesh is stripped: number of tris per strip. */
	uint32				nNumStrips;		/*!< If mesh is stripped: number of strips, length of pnStripLength array. */
	CPODData			sVertex;		/*!< List of vertices (x0, y0, z0, x1, y1, z1, x2, etc...) */
	CPODData			sNormals;		/*!< List of vertex normals (Nx0, Ny0, Nz0, Nx1, Ny1, Nz1, Nx2, etc...) */
	CPODData			sTangents;		/*!< List of vertex tangents (Tx0, Ty0, Tz0, Tx1, Ty1, Tz1, Tx2, etc...) */
	CPODData			sBinormals;		/*!< List of vertex binormals (Bx0, By0, Bz0, Bx1, By1, Bz1, Bx2, etc...) */
	CPODData			*psUVW;			/*!< List of UVW coordinate sets; size of array given by 'nNumUVW' */
	CPODData			sVtxColours;	/*!< A colour per vertex */
	CPODData			sBoneIdx;		/*!< nNumBones*nNumVertex ints (Vtx0Idx0, Vtx0Idx1, ... Vtx1Idx0, Vtx1Idx1, ...) */
	CPODData			sBoneWeight;	/*!< nNumBones*nNumVertex floats (Vtx0Wt0, Vtx0Wt1, ... Vtx1Wt0, Vtx1Wt1, ...) */
	uint8				*pInterleaved;	/*!< Interleaved vertex data */
	CPVRTBoneBatches	sBoneBatches;	/*!< Bone tables */
	EPODPrimitiveType	ePrimitiveType;	/*!< Primitive type used by this mesh */
	PVRTMATRIX			mUnpackMatrix;	/*!< A matrix used for unscaling scaled vertex data created with PVRTModelPODScaleAndConvertVtxData*/
};

struct SPODNode {
	int32			nIdx;				/*!< Index into mesh, light or camera array, depending on which object list contains this Node */
	char			*pszName;			/*!< Name of object */
	int32			nIdxMaterial;		/*!< Index of material used on this mesh */
	int32			nIdxParent;		/*!< Index into MeshInstance array; recursively apply ancestor's transforms after this instance's. */
	uint32			nAnimFlags;		/*!< Stores which animation arrays the POD Node contains */
	uint32			*pnAnimPositionIdx;
	VERTTYPE		*pfAnimPosition;	/*!< 3 floats per frame of animation. */
	uint32			*pnAnimRotationIdx;
	VERTTYPE		*pfAnimRotation;	/*!< 4 floats per frame of animation. */
	uint32			*pnAnimScaleIdx;
	VERTTYPE		*pfAnimScale;		/*!< 7 floats per frame of animation. */
	uint32			*pnAnimMatrixIdx;
	VERTTYPE		*pfAnimMatrix;		/*!< 16 floats per frame of animation. */
	uint32			nUserDataSize;
	char			*pUserData;
};

struct SPODTexture {
	char			*pszName;			/*!< File-name of texture */
};

struct SPODMaterial {
	char			*pszName;				/*!< Name of material */
	int32			nIdxTexDiffuse;			/*!< Idx into pTexture for the diffuse texture */
	int32			nIdxTexAmbient;			/*!< Idx into pTexture for the ambient texture */
	int32			nIdxTexSpecularColour;	/*!< Idx into pTexture for the specular colour texture */
	int32			nIdxTexSpecularLevel;	/*!< Idx into pTexture for the specular level texture */
	int32			nIdxTexBump;			/*!< Idx into pTexture for the bump map */
	int32			nIdxTexEmissive;		/*!< Idx into pTexture for the emissive texture */
	int32			nIdxTexGlossiness;		/*!< Idx into pTexture for the glossiness texture */
	int32			nIdxTexOpacity;			/*!< Idx into pTexture for the opacity texture */
	int32			nIdxTexReflection;		/*!< Idx into pTexture for the reflection texture */
	int32			nIdxTexRefraction;		/*!< Idx into pTexture for the refraction texture */
	VERTTYPE		fMatOpacity;			/*!< Material opacity (used with vertex alpha ?) */
	VERTTYPE		pfMatAmbient[3];		/*!< Ambient RGB value */
	VERTTYPE		pfMatDiffuse[3];		/*!< Diffuse RGB value */
	VERTTYPE		pfMatSpecular[3];		/*!< Specular RGB value */
	VERTTYPE		fMatShininess;			/*!< Material shininess */
	char			*pszEffectFile;			/*!< Name of effect file */
	char			*pszEffectName;			/*!< Name of effect in the effect file */

	EPODBlendFunc	eBlendSrcRGB;		/*!< Blending RGB source value */
	EPODBlendFunc	eBlendSrcA;			/*!< Blending alpha source value */
	EPODBlendFunc	eBlendDstRGB;		/*!< Blending RGB destination value */
	EPODBlendFunc	eBlendDstA;			/*!< Blending alpha destination value */
	EPODBlendOp		eBlendOpRGB;		/*!< Blending RGB operation */
	EPODBlendOp		eBlendOpA;			/*!< Blending alpha operation */
	VERTTYPE		pfBlendColour[4];	/*!< A RGBA colour to be used in blending */
	VERTTYPE		pfBlendFactor[4];	/*!< An array of blend factors, one for each RGBA component */

	uint32			nFlags;				/*!< Stores information about the material e.g. Enable blending */

	uint32			nUserDataSize;
	char			*pUserData;
};

struct SPODScene {
	VERTTYPE		pfColourBackground[3];		/*!< Background colour */
	VERTTYPE		pfColourAmbient[3];			/*!< Ambient colour */
	uint32			nNumCamera;				/*!< The length of the array pCamera */
	SPODCamera		*pCamera;				/*!< Camera nodes array */
	uint32			nNumLight;				/*!< The length of the array pLight */
	SPODLight		*pLight;				/*!< Light nodes array */
	uint32			nNumMesh;				/*!< The length of the array pMesh */
	SPODMesh		*pMesh;					/*!< Mesh array. Meshes may be instanced several times in a scene; i.e. multiple Nodes may reference any given mesh. */
	uint32			nNumNode;		/*!< Number of items in the array pNode */
	uint32			nNumMeshNode;	/*!< Number of items in the array pNode which are objects */
	SPODNode		*pNode;			/*!< Node array. Sorted as such: objects, lights, cameras, Everything Else (bones, helpers etc) */
	uint32			nNumTexture;	/*!< Number of textures in the array pTexture */
	SPODTexture		*pTexture;		/*!< Texture array */
	uint32			nNumMaterial;	/*!< Number of materials in the array pMaterial */
	SPODMaterial	*pMaterial;		/*!< Material array */
	uint32			nNumFrame;		/*!< Number of frames of animation */
	uint32			nFPS;			/*!< The frames per second the animation should be played at */
	uint32			nFlags;			/*!< PVRTMODELPODSF_* bit-flags */
	uint32			nUserDataSize;
	char			*pUserData;
};

enum EPODFileName {
	ePODFileVersion				= 1000,
	ePODFileScene,
	ePODFileExpOpt,
	ePODFileHistory,
	ePODFileEndiannessMisMatch  = uint32(-402456576),

	ePODFileColourBackground	= 2000,
	ePODFileColourAmbient,
	ePODFileNumCamera,
	ePODFileNumLight,
	ePODFileNumMesh,
	ePODFileNumNode,
	ePODFileNumMeshNode,
	ePODFileNumTexture,
	ePODFileNumMaterial,
	ePODFileNumFrame,
	ePODFileCamera,		// Will come multiple times
	ePODFileLight,		// Will come multiple times
	ePODFileMesh,		// Will come multiple times
	ePODFileNode,		// Will come multiple times
	ePODFileTexture,	// Will come multiple times
	ePODFileMaterial,	// Will come multiple times
	ePODFileFlags,
	ePODFileFPS,
	ePODFileUserData,

	ePODFileMatName				= 3000,
	ePODFileMatIdxTexDiffuse,
	ePODFileMatOpacity,
	ePODFileMatAmbient,
	ePODFileMatDiffuse,
	ePODFileMatSpecular,
	ePODFileMatShininess,
	ePODFileMatEffectFile,
	ePODFileMatEffectName,
	ePODFileMatIdxTexAmbient,
	ePODFileMatIdxTexSpecularColour,
	ePODFileMatIdxTexSpecularLevel,
	ePODFileMatIdxTexBump,
	ePODFileMatIdxTexEmissive,
	ePODFileMatIdxTexGlossiness,
	ePODFileMatIdxTexOpacity,
	ePODFileMatIdxTexReflection,
	ePODFileMatIdxTexRefraction,
	ePODFileMatBlendSrcRGB,
	ePODFileMatBlendSrcA,
	ePODFileMatBlendDstRGB,
	ePODFileMatBlendDstA,
	ePODFileMatBlendOpRGB,
	ePODFileMatBlendOpA,
	ePODFileMatBlendColour,
	ePODFileMatBlendFactor,
	ePODFileMatFlags,
	ePODFileMatUserData,

	ePODFileTexName				= 4000,

	ePODFileNodeIdx				= 5000,
	ePODFileNodeName,
	ePODFileNodeIdxMat,
	ePODFileNodeIdxParent,
	ePODFileNodePos,
	ePODFileNodeRot,
	ePODFileNodeScale,
	ePODFileNodeAnimPos,
	ePODFileNodeAnimRot,
	ePODFileNodeAnimScale,
	ePODFileNodeMatrix,
	ePODFileNodeAnimMatrix,
	ePODFileNodeAnimFlags,
	ePODFileNodeAnimPosIdx,
	ePODFileNodeAnimRotIdx,
	ePODFileNodeAnimScaleIdx,
	ePODFileNodeAnimMatrixIdx,
	ePODFileNodeUserData,

	ePODFileMeshNumVtx			= 6000,
	ePODFileMeshNumFaces,
	ePODFileMeshNumUVW,
	ePODFileMeshFaces,
	ePODFileMeshStripLength,
	ePODFileMeshNumStrips,
	ePODFileMeshVtx,
	ePODFileMeshNor,
	ePODFileMeshTan,
	ePODFileMeshBin,
	ePODFileMeshUVW,			// Will come multiple times
	ePODFileMeshVtxCol,
	ePODFileMeshBoneIdx,
	ePODFileMeshBoneWeight,
	ePODFileMeshInterleaved,
	ePODFileMeshBoneBatches,
	ePODFileMeshBoneBatchBoneCnts,
	ePODFileMeshBoneBatchOffsets,
	ePODFileMeshBoneBatchBoneMax,
	ePODFileMeshBoneBatchCnt,
	ePODFileMeshUnpackMatrix,

	ePODFileLightIdxTgt			= 7000,
	ePODFileLightColour,
	ePODFileLightType,
	ePODFileLightConstantAttenuation,
	ePODFileLightLinearAttenuation,
	ePODFileLightQuadraticAttenuation,
	ePODFileLightFalloffAngle,
	ePODFileLightFalloffExponent,

	ePODFileCamIdxTgt			= 8000,
	ePODFileCamFOV,
	ePODFileCamFar,
	ePODFileCamNear,
	ePODFileCamAnimFOV,

	ePODFileDataType			= 9000,
	ePODFileN,
	ePODFileStride,
	ePODFileData
};

struct SPVRTPODImpl {
	VERTTYPE	fFrame;			/*!< Frame number */
	VERTTYPE	fBlend;			/*!< Frame blend	(AKA fractional part of animation frame number) */
	int			nFrame;			/*!< Frame number (AKA integer part of animation frame number) */
	VERTTYPE	*pfCache;		/*!< Cache indicating the frames at which the matrix cache was filled */
	PVRTMATRIX	*pWmCache;		/*!< Cache of world matrices */
	PVRTMATRIX	*pWmZeroCache;	/*!< Pre-calculated frame 0 matrices */
	bool		bFromMemory;	/*!< Was the mesh data loaded from memory? */
};

class CPVRTModelPOD : public SPODScene {
public:
	CPVRTModelPOD() : m_pImpl(NULL)	{}
	~CPVRTModelPOD()	{ Destroy(); }

	EPVRTError	InitImpl();
	void		DestroyImpl();
	bool		IsLoaded();
	void		Destroy();

private:
	SPVRTPODImpl	*m_pImpl;	/*!< Internal implementation data */
};

void CPVRTModelPOD::Destroy() {
	unsigned int	i;
	if (m_pImpl) {
		for(i = 0; i < nNumCamera; ++i)
			SafeFree(pCamera[i].pfAnimFOV);
		SafeFree(pCamera);

		SafeFree(pLight);

		for(i = 0; i < nNumMaterial; ++i) {
			SafeFree(pMaterial[i].pszName);
			SafeFree(pMaterial[i].pszEffectFile);
			SafeFree(pMaterial[i].pszEffectName);
			SafeFree(pMaterial[i].pUserData);
		}
		SafeFree(pMaterial);

		for(i = 0; i < nNumMesh; ++i) {
			SafeFree(pMesh[i].sFaces.pData);
			SafeFree(pMesh[i].pnStripLength);
			if(pMesh[i].pInterleaved) {
				SafeFree(pMesh[i].pInterleaved);
			} else {
				SafeFree(pMesh[i].sVertex.pData);
				SafeFree(pMesh[i].sNormals.pData);
				SafeFree(pMesh[i].sTangents.pData);
				SafeFree(pMesh[i].sBinormals.pData);
				for(unsigned int j = 0; j < pMesh[i].nNumUVW; ++j)
					SafeFree(pMesh[i].psUVW[j].pData);
				SafeFree(pMesh[i].sVtxColours.pData);
				SafeFree(pMesh[i].sBoneIdx.pData);
				SafeFree(pMesh[i].sBoneWeight.pData);
			}
			SafeFree(pMesh[i].psUVW);
			pMesh[i].sBoneBatches.Release();
		}
		SafeFree(pMesh);

		for(i = 0; i < nNumNode; ++i) {
			SafeFree(pNode[i].pszName);
			SafeFree(pNode[i].pfAnimPosition);
			SafeFree(pNode[i].pnAnimPositionIdx);
			SafeFree(pNode[i].pfAnimRotation);
			SafeFree(pNode[i].pnAnimRotationIdx);
			SafeFree(pNode[i].pfAnimScale);
			SafeFree(pNode[i].pnAnimScaleIdx);
			SafeFree(pNode[i].pfAnimMatrix);
			SafeFree(pNode[i].pnAnimMatrixIdx);
			SafeFree(pNode[i].pUserData);
			pNode[i].nAnimFlags = 0;
		}

		SafeFree(pNode);

		for(i = 0; i < nNumTexture; ++i)
			SafeFree(pTexture[i].pszName);
		SafeFree(pTexture);
		SafeFree(pUserData);
		// Free the working space used by the implementation
		DestroyImpl();
	}
}

class CSource {
	istream_ref	file;
public:
	CSource(istream_ref _file) : file(_file)	{}
	bool Read(void* lpBuffer, const unsigned int dwNumberOfBytesToRead)	{ return file.readbuff(lpBuffer, dwNumberOfBytesToRead) == dwNumberOfBytesToRead; }
	bool Skip(const unsigned int nBytes)	{ file.seek_cur(nBytes); return true; }

	template <typename T> bool Read(T &n)	{ return Read(&n, sizeof(T)); }
	template <typename T> bool Read32(T &n)	{ uint32le v; if (file.read(v)) { n = (T)v; return true; } return false; }
	template <typename T> bool Read16(T &n)	{ uint16le v; if (file.read(v)) { n = (T)v; return true; } return false; }

	bool ReadMarker(unsigned int &nName, unsigned int &nLen) {
		return Read32(nName) && Read32(nLen);
	}
	template <typename T> bool ReadAfterAlloc(T* &lpBuffer, const unsigned int dwNumberOfBytesToRead) {
		return SafeAlloc(lpBuffer, dwNumberOfBytesToRead) && Read(lpBuffer, dwNumberOfBytesToRead);
	}
	template <typename T> bool ReadAfterAlloc32(T* &lpBuffer, const unsigned int dwNumberOfBytesToRead) {
		return SafeAlloc(lpBuffer, dwNumberOfBytesToRead/4) && ReadArray32((unsigned int*)lpBuffer, dwNumberOfBytesToRead / 4);
	}
	bool ReadArray32(unsigned int *pn, unsigned int i32Size) {
		bool bRet = true;
		for(unsigned int i = 0; i < i32Size; ++i)
			bRet &= Read32(pn[i]);
		return bRet;
	}
	template <typename T> bool ReadAfterAlloc16(T* &lpBuffer, const unsigned int dwNumberOfBytesToRead) {
		return SafeAlloc(lpBuffer,dwNumberOfBytesToRead/2) && ReadArray16((unsigned short*) lpBuffer, dwNumberOfBytesToRead / 2);
	}
	bool ReadArray16(unsigned short* pn, unsigned int i32Size) {
		bool bRet = true;
		for(unsigned int i = 0; i < i32Size; ++i)
			bRet &= Read16(pn[i]);
		return bRet;
	}
};

size_t PVRTModelPODDataTypeSize(const EPVRTDataType type) {
	switch(type) {
		default:						return 0;
		case EPODDataFloat:				return sizeof(float);
		case EPODDataInt:
		case EPODDataUnsignedInt:		return sizeof(int);
		case EPODDataShort:
		case EPODDataShortNorm:
		case EPODDataUnsignedShort:
		case EPODDataUnsignedShortNorm:	return sizeof(unsigned short);
		case EPODDataRGBA:				return sizeof(unsigned int);
		case EPODDataARGB:				return sizeof(unsigned int);
		case EPODDataD3DCOLOR:			return sizeof(unsigned int);
		case EPODDataUBYTE4:			return sizeof(unsigned int);
		case EPODDataDEC3N:				return sizeof(unsigned int);
		case EPODDataFixed16_16:		return sizeof(unsigned int);
		case EPODDataUnsignedByte:
		case EPODDataUnsignedByteNorm:
		case EPODDataByte:
		case EPODDataByteNorm:			return sizeof(unsigned char);
	}
}


static bool ReadCPODData(CPODData &s, CSource &src, const unsigned int nSpec, const bool bValidData) {
	unsigned int nName, nLen, nBuff;

	while(src.ReadMarker(nName, nLen)) {
		if(nName == (nSpec | PVRTMODELPOD_TAG_END))
			return true;

		switch(nName) {
		case ePODFileDataType:	if (!src.Read32(s.eType))	return false; break;
		case ePODFileN:			if (!src.Read32(s.n))		return false; break;
		case ePODFileStride:	if (!src.Read32(s.nStride)) return false; break;
		case ePODFileData:
			if(bValidData) {
				switch(PVRTModelPODDataTypeSize(s.eType)) {
					case 1: if(!src.ReadAfterAlloc(s.pData, nLen)) return false; break;
					case 2:	{ // reading 16bit data but have 8bit pointer
							uint16 *p16Pointer=NULL;
							if(!src.ReadAfterAlloc16(p16Pointer, nLen)) return false;
							s.pData = (unsigned char*)p16Pointer;
							break;
						}
					case 4:	{ // reading 32bit data but have 8bit pointer
							uint32 *p32Pointer=NULL;
							if(!src.ReadAfterAlloc32(p32Pointer, nLen)) return false;
							s.pData = (unsigned char*)p32Pointer;
							break;
						}
					default:
						ISO_ASSERT(false);
				}
			} else {
				if(src.Read32(nBuff))
					s.pData = (unsigned char*) (size_t) nBuff;
				else
					return false;
			}
		 break;

		default:
			if(!src.Skip(nLen)) return false;
		}
	}
	return false;
}

static bool ReadCamera(SPODCamera &s, CSource &src) {
	unsigned int nName, nLen;
	s.pfAnimFOV = 0;

	while(src.ReadMarker(nName, nLen)) {
		switch(nName) {
		case ePODFileCamera | PVRTMODELPOD_TAG_END:
			return true;

		case ePODFileCamIdxTgt:		if(!src.Read32(s.nIdxTarget))	return false; break;
		case ePODFileCamFOV:		if(!src.Read32(s.fFOV))			return false; break;
		case ePODFileCamFar:		if(!src.Read32(s.fFar))			return false; break;
		case ePODFileCamNear:		if(!src.Read32(s.fNear))		return false; break;
		case ePODFileCamAnimFOV:	if(!src.ReadAfterAlloc32(s.pfAnimFOV, nLen)) return false;	break;

		default:
			if(!src.Skip(nLen)) return false;
		}
	}
	return false;
}

static bool ReadLight(SPODLight &s, CSource &src) {
	unsigned int nName, nLen;

	while(src.ReadMarker(nName, nLen)) {
		switch(nName) {
		case ePODFileLight | PVRTMODELPOD_TAG_END:
			return true;

		case ePODFileLightIdxTgt:					if(!src.Read32(s.nIdxTarget)) return false;	break;
		case ePODFileLightColour:					if(!src.ReadArray32((unsigned int*) s.pfColour, 3)) return false; break;
		case ePODFileLightType:						if(!src.Read32(s.eType))				return false; break;
		case ePODFileLightConstantAttenuation: 		if(!src.Read32(s.fConstantAttenuation))	return false; break;
		case ePODFileLightLinearAttenuation:		if(!src.Read32(s.fLinearAttenuation))	return false; break;
		case ePODFileLightQuadraticAttenuation:		if(!src.Read32(s.fQuadraticAttenuation))return false; break;
		case ePODFileLightFalloffAngle:				if(!src.Read32(s.fFalloffAngle))		return false; break;
		case ePODFileLightFalloffExponent:			if(!src.Read32(s.fFalloffExponent))		return false; break;
		default:
			if(!src.Skip(nLen)) return false;
		}
	}
	return false;
}

static bool ReadMaterial(SPODMaterial &s, CSource &src) {
	unsigned int nName, nLen;

	// Set texture IDs to -1
	s.nIdxTexDiffuse = -1;
	s.nIdxTexAmbient = -1;
	s.nIdxTexSpecularColour = -1;
	s.nIdxTexSpecularLevel = -1;
	s.nIdxTexBump = -1;
	s.nIdxTexEmissive = -1;
	s.nIdxTexGlossiness = -1;
	s.nIdxTexOpacity = -1;
	s.nIdxTexReflection = -1;
	s.nIdxTexRefraction = -1;

	// Set defaults for blend modes
	s.eBlendSrcRGB = s.eBlendSrcA = ePODBlendFunc_ONE;
	s.eBlendDstRGB = s.eBlendDstA = ePODBlendFunc_ZERO;
	s.eBlendOpRGB  = s.eBlendOpA  = ePODBlendOp_ADD;

	memset(s.pfBlendColour, 0, sizeof(s.pfBlendColour));
	memset(s.pfBlendFactor, 0, sizeof(s.pfBlendFactor));

	// Set default for material flags
	s.nFlags = 0;

	// Set default for user data
	s.pUserData = 0;
	s.nUserDataSize = 0;

	while(src.ReadMarker(nName, nLen)) {
		switch(nName) {
		case ePODFileMaterial | PVRTMODELPOD_TAG_END:			return true;

		case ePODFileMatFlags:					if(!src.Read32(s.nFlags)) return false;						break;
		case ePODFileMatName:					if(!src.ReadAfterAlloc(s.pszName, nLen)) return false;		break;
		case ePODFileMatIdxTexDiffuse:			if(!src.Read32(s.nIdxTexDiffuse)) return false;				break;
		case ePODFileMatIdxTexAmbient:			if(!src.Read32(s.nIdxTexAmbient)) return false;				break;
		case ePODFileMatIdxTexSpecularColour:	if(!src.Read32(s.nIdxTexSpecularColour)) return false;		break;
		case ePODFileMatIdxTexSpecularLevel:	if(!src.Read32(s.nIdxTexSpecularLevel)) return false;		break;
		case ePODFileMatIdxTexBump:				if(!src.Read32(s.nIdxTexBump)) return false;				break;
		case ePODFileMatIdxTexEmissive:			if(!src.Read32(s.nIdxTexEmissive)) return false;			break;
		case ePODFileMatIdxTexGlossiness:		if(!src.Read32(s.nIdxTexGlossiness)) return false;			break;
		case ePODFileMatIdxTexOpacity:			if(!src.Read32(s.nIdxTexOpacity)) return false;				break;
		case ePODFileMatIdxTexReflection:		if(!src.Read32(s.nIdxTexReflection)) return false;			break;
		case ePODFileMatIdxTexRefraction:		if(!src.Read32(s.nIdxTexRefraction)) return false;			break;
		case ePODFileMatOpacity:				if(!src.Read32(s.fMatOpacity)) return false;				break;
		case ePODFileMatAmbient:				if(!src.ReadArray32((unsigned int*) s.pfMatAmbient,  sizeof(s.pfMatAmbient) / sizeof(*s.pfMatAmbient))) return false;		break;
		case ePODFileMatDiffuse:				if(!src.ReadArray32((unsigned int*) s.pfMatDiffuse,  sizeof(s.pfMatDiffuse) / sizeof(*s.pfMatDiffuse))) return false;		break;
		case ePODFileMatSpecular:				if(!src.ReadArray32((unsigned int*) s.pfMatSpecular, sizeof(s.pfMatSpecular) / sizeof(*s.pfMatSpecular))) return false;		break;
		case ePODFileMatShininess:				if(!src.Read32(s.fMatShininess)) return false;					break;
		case ePODFileMatEffectFile:				if(!src.ReadAfterAlloc(s.pszEffectFile, nLen)) return false;	break;
		case ePODFileMatEffectName:				if(!src.ReadAfterAlloc(s.pszEffectName, nLen)) return false;	break;
		case ePODFileMatBlendSrcRGB:			if(!src.Read32(s.eBlendSrcRGB))	return false;	break;
		case ePODFileMatBlendSrcA:				if(!src.Read32(s.eBlendSrcA))	return false;	break;
		case ePODFileMatBlendDstRGB:			if(!src.Read32(s.eBlendDstRGB))	return false;	break;
		case ePODFileMatBlendDstA:				if(!src.Read32(s.eBlendDstA))	return false;	break;
		case ePODFileMatBlendOpRGB:				if(!src.Read32(s.eBlendOpRGB))	return false;	break;
		case ePODFileMatBlendOpA:				if(!src.Read32(s.eBlendOpA))	return false;	break;
		case ePODFileMatBlendColour:			if(!src.ReadArray32((unsigned int*) s.pfBlendColour, sizeof(s.pfBlendColour) / sizeof(*s.pfBlendColour)))	return false;	break;
		case ePODFileMatBlendFactor:			if(!src.ReadArray32((unsigned int*) s.pfBlendFactor, sizeof(s.pfBlendFactor) / sizeof(*s.pfBlendFactor)))	return false;	break;

		case ePODFileMatUserData:
			if(!src.ReadAfterAlloc(s.pUserData, nLen))
				return false;
			s.nUserDataSize = nLen;
			break;

		default:
			if(!src.Skip(nLen)) return false;
		}
	}
	return false;
}

void PVRTFixInterleavedEndiannessUsingCPODData(unsigned char* pInterleaved, CPODData &data, unsigned int ui32Size) {
	if(!data.n)
		return;

	size_t ui32TypeSize = PVRTModelPODDataTypeSize(data.eType);

	unsigned char ub[4];
	unsigned char *pData = pInterleaved + (size_t) data.pData;

	switch(ui32TypeSize) {
		case 1: return;
		case 2:
			for(unsigned int i = 0; i < ui32Size; ++i) {
				for(unsigned int j = 0; j < data.n; ++j) {
					ub[0] = pData[ui32TypeSize * j + 0];
					ub[1] = pData[ui32TypeSize * j + 1];
					((unsigned short*) pData)[j] = (unsigned short) ((ub[1] << 8) | ub[0]);
				}
				pData += data.nStride;
			}
			break;
		case 4:
			for(unsigned int i = 0; i < ui32Size; ++i) {
				for(unsigned int j = 0; j < data.n; ++j) {
					ub[0] = pData[ui32TypeSize * j + 0];
					ub[1] = pData[ui32TypeSize * j + 1];
					ub[2] = pData[ui32TypeSize * j + 2];
					ub[3] = pData[ui32TypeSize * j + 3];
					((unsigned int*) pData)[j] = (unsigned int) ((ub[3] << 24) | (ub[2] << 16) | (ub[1] << 8) | ub[0]);
				}

				pData += data.nStride;
			}
			break;
		default: ISO_ASSERT(false);
	};
}

void PVRTFixInterleavedEndianness(SPODMesh &s) {
	if(!s.pInterleaved || !iso_bigendian)
		return;

	PVRTFixInterleavedEndiannessUsingCPODData(s.pInterleaved, s.sVertex, s.nNumVertex);
	PVRTFixInterleavedEndiannessUsingCPODData(s.pInterleaved, s.sNormals, s.nNumVertex);
	PVRTFixInterleavedEndiannessUsingCPODData(s.pInterleaved, s.sTangents, s.nNumVertex);
	PVRTFixInterleavedEndiannessUsingCPODData(s.pInterleaved, s.sBinormals, s.nNumVertex);

	for(unsigned int i = 0; i < s.nNumUVW; ++i)
		PVRTFixInterleavedEndiannessUsingCPODData(s.pInterleaved, s.psUVW[i], s.nNumVertex);

	PVRTFixInterleavedEndiannessUsingCPODData(s.pInterleaved, s.sVtxColours, s.nNumVertex);
	PVRTFixInterleavedEndiannessUsingCPODData(s.pInterleaved, s.sBoneIdx, s.nNumVertex);
	PVRTFixInterleavedEndiannessUsingCPODData(s.pInterleaved, s.sBoneWeight, s.nNumVertex);
}

static bool ReadMesh(SPODMesh &s, CSource &src) {
	unsigned int	nName, nLen;
	unsigned int	nUVWs=0;

	for (int i = 0; i < 16; i++)
		s.mUnpackMatrix[0][i] = float((i & 3) == (i >> 2));

	while(src.ReadMarker(nName, nLen)) {
		switch(nName) {
		case ePODFileMesh | PVRTMODELPOD_TAG_END:
			if(nUVWs != s.nNumUVW)
				return false;
			PVRTFixInterleavedEndianness(s);
			return true;

		case ePODFileMeshNumVtx:			if(!src.Read32(s.nNumVertex)) return false;													break;
		case ePODFileMeshNumFaces:			if(!src.Read32(s.nNumFaces)) return false;													break;
		case ePODFileMeshNumUVW:			if(!src.Read32(s.nNumUVW)) return false;	if(!SafeAlloc(s.psUVW, s.nNumUVW)) return false;	break;
		case ePODFileMeshStripLength:		if(!src.ReadAfterAlloc32(s.pnStripLength, nLen)) return false;								break;
		case ePODFileMeshNumStrips:			if(!src.Read32(s.nNumStrips)) return false;													break;
		case ePODFileMeshInterleaved:		if(!src.ReadAfterAlloc(s.pInterleaved, nLen)) return false;									break;
		case ePODFileMeshBoneBatches:		if(!src.ReadAfterAlloc32(s.sBoneBatches.pnBatches, nLen)) return false;						break;
		case ePODFileMeshBoneBatchBoneCnts:	if(!src.ReadAfterAlloc32(s.sBoneBatches.pnBatchBoneCnt, nLen)) return false;					break;
		case ePODFileMeshBoneBatchOffsets:	if(!src.ReadAfterAlloc32(s.sBoneBatches.pnBatchOffset, nLen)) return false;					break;
		case ePODFileMeshBoneBatchBoneMax:	if(!src.Read32(s.sBoneBatches.nBatchBoneMax)) return false;									break;
		case ePODFileMeshBoneBatchCnt:		if(!src.Read32(s.sBoneBatches.nBatchCnt)) return false;										break;
		case ePODFileMeshUnpackMatrix:		if(!src.ReadArray32((unsigned int*)&s.mUnpackMatrix.f[0], 16)) return false;										break;

		case ePODFileMeshFaces:				if(!ReadCPODData(s.sFaces, src, ePODFileMeshFaces, true)) return false;							break;
		case ePODFileMeshVtx:				if(!ReadCPODData(s.sVertex, src, ePODFileMeshVtx, s.pInterleaved == 0)) return false;			break;
		case ePODFileMeshNor:				if(!ReadCPODData(s.sNormals, src, ePODFileMeshNor, s.pInterleaved == 0)) return false;			break;
		case ePODFileMeshTan:				if(!ReadCPODData(s.sTangents, src, ePODFileMeshTan, s.pInterleaved == 0)) return false;			break;
		case ePODFileMeshBin:				if(!ReadCPODData(s.sBinormals, src, ePODFileMeshBin, s.pInterleaved == 0)) return false;			break;
		case ePODFileMeshUVW:				if(!ReadCPODData(s.psUVW[nUVWs++], src, ePODFileMeshUVW, s.pInterleaved == 0)) return false;		break;
		case ePODFileMeshVtxCol:			if(!ReadCPODData(s.sVtxColours, src, ePODFileMeshVtxCol, s.pInterleaved == 0)) return false;		break;
		case ePODFileMeshBoneIdx:			if(!ReadCPODData(s.sBoneIdx, src, ePODFileMeshBoneIdx, s.pInterleaved == 0)) return false;		break;
		case ePODFileMeshBoneWeight:		if(!ReadCPODData(s.sBoneWeight, src, ePODFileMeshBoneWeight, s.pInterleaved == 0)) return false;	break;

		default:
			if(!src.Skip(nLen)) return false;
		}
	}
	return false;
}

static bool ReadNode(SPODNode &s, CSource &src) {
	unsigned int nName, nLen;
	bool bOldNodeFormat = false;
	VERTTYPE fPos[3]   = {0,0,0};
	VERTTYPE fQuat[4]  = {0,0,0,1};
	VERTTYPE fScale[7] = {1,1,1,0,0,0,0};

	// Set default for user data
	s.pUserData = 0;
	s.nUserDataSize = 0;

	while(src.ReadMarker(nName, nLen)) {
		switch(nName) {
		case ePODFileNode | PVRTMODELPOD_TAG_END:
			if(bOldNodeFormat) {
				if(s.pfAnimPosition) {
					s.nAnimFlags |= ePODHasPositionAni;
				} else {
					s.pfAnimPosition = (VERTTYPE*)iso::malloc(sizeof(fPos));
					memcpy(s.pfAnimPosition, fPos, sizeof(fPos));
				}

				if(s.pfAnimRotation) {
					s.nAnimFlags |= ePODHasRotationAni;
				} else {
					s.pfAnimRotation = (VERTTYPE*)iso::malloc(sizeof(fQuat));
					memcpy(s.pfAnimRotation, fQuat, sizeof(fQuat));
				}

				if(s.pfAnimScale) {
					s.nAnimFlags |= ePODHasScaleAni;
				} else {
					s.pfAnimScale = (VERTTYPE*)iso::malloc(sizeof(fScale));
					memcpy(s.pfAnimScale, fScale, sizeof(fScale));
				}
			}
			return true;

		case ePODFileNodeIdx:			if(!src.Read32(s.nIdx)) return false;							break;
		case ePODFileNodeName:			if(!src.ReadAfterAlloc(s.pszName, nLen)) return false;			break;
		case ePODFileNodeIdxMat:		if(!src.Read32(s.nIdxMaterial)) return false;					break;
		case ePODFileNodeIdxParent:		if(!src.Read32(s.nIdxParent)) return false;						break;
		case ePODFileNodeAnimFlags:		if(!src.Read32(s.nAnimFlags))return false;						break;

		case ePODFileNodeAnimPosIdx:	if(!src.ReadAfterAlloc32(s.pnAnimPositionIdx, nLen)) return false;	break;
		case ePODFileNodeAnimPos:		if(!src.ReadAfterAlloc32(s.pfAnimPosition, nLen)) return false;	break;

		case ePODFileNodeAnimRotIdx:	if(!src.ReadAfterAlloc32(s.pnAnimRotationIdx, nLen)) return false;	break;
		case ePODFileNodeAnimRot:		if(!src.ReadAfterAlloc32(s.pfAnimRotation, nLen)) return false;	break;

		case ePODFileNodeAnimScaleIdx:	if(!src.ReadAfterAlloc32(s.pnAnimScaleIdx, nLen)) return false;	break;
		case ePODFileNodeAnimScale:		if(!src.ReadAfterAlloc32(s.pfAnimScale, nLen)) return false;		break;

		case ePODFileNodeAnimMatrixIdx:	if(!src.ReadAfterAlloc32(s.pnAnimMatrixIdx, nLen)) return false;	break;
		case ePODFileNodeAnimMatrix:	if(!src.ReadAfterAlloc32(s.pfAnimMatrix, nLen)) return false;	break;

		case ePODFileNodeUserData:
			if(!src.ReadAfterAlloc(s.pUserData, nLen))
				return false;
			s.nUserDataSize = nLen;
			break;

		// Parameters from the older pod format
		case ePODFileNodePos:		if(!src.ReadArray32((unsigned int*) fPos, 3))   return false;		bOldNodeFormat = true;		break;
		case ePODFileNodeRot:		if(!src.ReadArray32((unsigned int*) fQuat, 4))  return false;		bOldNodeFormat = true;		break;
		case ePODFileNodeScale:		if(!src.ReadArray32((unsigned int*) fScale,3)) return false;		bOldNodeFormat = true;		break;

		default:
			if(!src.Skip(nLen)) return false;
		}
	}

	return false;
}

static bool ReadTexture(SPODTexture &s, CSource &src) {
	unsigned int nName, nLen;

	while(src.ReadMarker(nName, nLen)) {
		switch(nName) {
		case ePODFileTexture | PVRTMODELPOD_TAG_END:			return true;
		case ePODFileTexName:		if(!src.ReadAfterAlloc(s.pszName, nLen)) return false;			break;
		default:
			if(!src.Skip(nLen)) return false;
		}
	}
	return false;
}

static bool ReadScene(SPODScene &s, CSource &src) {
	unsigned int nName, nLen;
	unsigned int nCameras=0, nLights=0, nMaterials=0, nMeshes=0, nTextures=0, nNodes=0;
	s.nFPS = 30;

	// Set default for user data
	s.pUserData = 0;
	s.nUserDataSize = 0;

	while(src.ReadMarker(nName, nLen)) {
		switch(nName) {
		case ePODFileScene | PVRTMODELPOD_TAG_END:
			if(nCameras		!= s.nNumCamera) return false;
			if(nLights		!= s.nNumLight) return false;
			if(nMaterials	!= s.nNumMaterial) return false;
			if(nMeshes		!= s.nNumMesh) return false;
			if(nTextures	!= s.nNumTexture) return false;
			if(nNodes		!= s.nNumNode) return false;
			return true;

		case ePODFileColourBackground:	if(!src.ReadArray32((unsigned int*) s.pfColourBackground, sizeof(s.pfColourBackground) / sizeof(*s.pfColourBackground))) return false;	break;
		case ePODFileColourAmbient:		if(!src.ReadArray32((unsigned int*) s.pfColourAmbient, sizeof(s.pfColourAmbient) / sizeof(*s.pfColourAmbient))) return false;		break;
		case ePODFileNumCamera:			if(!src.Read32(s.nNumCamera)) return false;			if(!SafeAlloc(s.pCamera, s.nNumCamera)) return false;		break;
		case ePODFileNumLight:			if(!src.Read32(s.nNumLight)) return false;			if(!SafeAlloc(s.pLight, s.nNumLight)) return false;			break;
		case ePODFileNumMesh:			if(!src.Read32(s.nNumMesh)) return false;				if(!SafeAlloc(s.pMesh, s.nNumMesh)) return false;			break;
		case ePODFileNumNode:			if(!src.Read32(s.nNumNode)) return false;				if(!SafeAlloc(s.pNode, s.nNumNode)) return false;			break;
		case ePODFileNumMeshNode:		if(!src.Read32(s.nNumMeshNode)) return false;			break;
		case ePODFileNumTexture:		if(!src.Read32(s.nNumTexture)) return false;			if(!SafeAlloc(s.pTexture, s.nNumTexture)) return false;		break;
		case ePODFileNumMaterial:		if(!src.Read32(s.nNumMaterial)) return false;			if(!SafeAlloc(s.pMaterial, s.nNumMaterial)) return false;	break;
		case ePODFileNumFrame:			if(!src.Read32(s.nNumFrame)) return false;			break;
		case ePODFileFPS:				if(!src.Read32(s.nFPS))	return false;				break;
		case ePODFileFlags:				if(!src.Read32(s.nFlags)) return false;				break;

		case ePODFileCamera:	if(!ReadCamera(s.pCamera[nCameras++], src)) return false;		break;
		case ePODFileLight:		if(!ReadLight(s.pLight[nLights++], src)) return false;			break;
		case ePODFileMaterial:	if(!ReadMaterial(s.pMaterial[nMaterials++], src)) return false;	break;
		case ePODFileMesh:		if(!ReadMesh(s.pMesh[nMeshes++], src)) return false;			break;
		case ePODFileNode:		if(!ReadNode(s.pNode[nNodes++], src)) return false;				break;
		case ePODFileTexture:	if(!ReadTexture(s.pTexture[nTextures++], src)) return false;	break;

		case ePODFileUserData:
			if(!src.ReadAfterAlloc(s.pUserData, nLen))
				return false;
			s.nUserDataSize = nLen;
			break;
		default:
			if(!src.Skip(nLen)) return false;
		}
	}
	return false;
}

static bool Read(SPODScene * const pS, CSource &src, char * const pszExpOpt, const size_t count, char * const pszHistory, const size_t historyCount) {
	unsigned int	nName, nLen;
	bool			bVersionOK = false, bDone = false;
	bool			bNeedOptions = pszExpOpt != 0;
	bool			bNeedHistory = pszHistory != 0;
	bool			bLoadingOptionsOrHistory = bNeedOptions || bNeedHistory;

	while(src.ReadMarker(nName, nLen)) {
		switch(nName) {
		case ePODFileVersion: {
				char *pszVersion = NULL;
				if(nLen != strlen(PVRTMODELPOD_VERSION)+1) return false;
				if(!SafeAlloc(pszVersion, nLen)) return false;
				if(!src.Read(pszVersion, nLen)) return false;
				if(strcmp(pszVersion, PVRTMODELPOD_VERSION) != 0) return false;
				bVersionOK = true;
				SafeFree(pszVersion);
			}
			continue;

		case ePODFileScene:
			if(pS) {
				if(!ReadScene(*pS, src))
					return false;
				bDone = true;
			}
			continue;

		case ePODFileExpOpt:
			if(bNeedOptions) {
				if(!src.Read(pszExpOpt, min(nLen, (unsigned int) count)))
					return false;

				bNeedOptions = false;

				if(count < nLen)
					nLen -= (unsigned int) count ; // Adjust nLen as the read has moved our position
				else
					nLen = 0;
			}
			break;

		case ePODFileHistory:
			if(bNeedHistory) {
				if(!src.Read(pszHistory, min(nLen, (unsigned int) historyCount)))
					return false;

				bNeedHistory = false;
				if(count < nLen)
					nLen -= (unsigned int) historyCount; // Adjust nLen as the read has moved our position
				else
					nLen = 0;
			}
			break;

		case ePODFileScene | PVRTMODELPOD_TAG_END:
			return bVersionOK == true && bDone == true;

		case ePODFileEndiannessMisMatch:
//			PVRTErrorOutputDebug("Error: Endianness mismatch between the .pod file and the platform.\n");
			return false;

		}

		if (bLoadingOptionsOrHistory && !bNeedOptions && !bNeedHistory)
			return true; // The options and/or history has been loaded

		// Unhandled data, skip it
		if(!src.Skip(nLen))
			return false;
	}

	if(bLoadingOptionsOrHistory)
		return true;

//	if(pS->nFlags & PVRTMODELPODSF_FIXED)
//		PVRTModelPODToggleFixedPoint(*pS);

	return bVersionOK == true && bDone == true;
}

EPVRTError CPVRTModelPOD::InitImpl() {
	// Allocate space for implementation data
	delete m_pImpl;
	m_pImpl = new SPVRTPODImpl;
	if(!m_pImpl)
		return PVR_FAIL;

	// Zero implementation data
	memset(m_pImpl, 0, sizeof(*m_pImpl));

	// Allocate world-matrix cache
	m_pImpl->pfCache		= new VERTTYPE[nNumNode];
	m_pImpl->pWmCache		= new PVRTMATRIX[nNumNode];
	m_pImpl->pWmZeroCache	= new PVRTMATRIX[nNumNode];

	return PVR_SUCCESS;
}

void CPVRTModelPOD::DestroyImpl() {
	if(m_pImpl) {
		if(m_pImpl->pfCache)		delete [] m_pImpl->pfCache;
		if(m_pImpl->pWmCache)		delete [] m_pImpl->pWmCache;
		if(m_pImpl->pWmZeroCache)	delete [] m_pImpl->pWmZeroCache;

		delete m_pImpl;
		m_pImpl = 0;
	}
}

class PODFileHandler : FileHandler {
	const char*		GetExt() override { return "pod";		}
	const char*		GetDescription() override { return "PVR POD";	}
	ISO_ptr<void>	Read(tag id, istream_ref file) override;
//	bool			Write(ISO_ptr<void> p, ostream_ref file) override;
} pod;

ISO_ptr<void> PODFileHandler::Read(tag id, istream_ref file) {
	CPVRTModelPOD	pod;
	clear(pod);
	char	options[256];
	char	history[256];
	CSource	src(file);
	if (!::Read(&pod, src, options, file.length(), history, sizeof(history)) || pod.InitImpl() != PVR_SUCCESS)
		return ISO_NULL;
	return ISO_NULL;
}
