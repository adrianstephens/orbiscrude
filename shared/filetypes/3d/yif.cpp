#include "iso/iso_files.h"
#include "base/vector.h"
#include "base/algorithm.h"
#include "model_utils.h"

using namespace iso;

struct SYifVertex {
	float3p		vPos;
	float3p		vNorm;
	float4p		vTangent;
	float4p		vColor;
	float2p		vTex;
};

struct SYifSkinVertInfo {
	float4p		vWeights0;
	float4p		vWeights1;
	uint16		bone_mats0[4];
	uint16		bone_mats1[4];
};

class CYifMaterial {
public:
private:
	string	m_pMaterialName;
};

enum eYifPrimType {
	YIF_TRIANGLES		= 1,
	YIF_QUADS			= 2,
	YIF_FANS			= 3,
	YIF_STRIPS			= 4,
	YIF_LINES			= 5,
	YIF_CONNECTED_LINES	= 6,
};

class CYifSurface {
	// type
	int				m_iNumPrims;
	int				*m_piNumVerts;			// only set for fans and strips
	int				m_iOffset;				// offset into the index list
	eYifPrimType	m_ePrimType;
	int				m_iMatIndex;			// index to a material

	// patch stuff
	int				m_iIndicesOffsetRegularPatches, m_iOffsetRegularPatches, m_iNrRegularPatches;
	int				m_iIndicesOffsetIrregularPatches, m_iOffsetIrregularPatches, m_iNrIrregularPatches;
public:
	void			SetPrimType(eYifPrimType eType)						{ m_ePrimType = eType;}
	void			SetNumPrims(int iNrNumPrims)						{ m_iNumPrims = iNrNumPrims;}
	void			SetNumVertsPerPrim(int *piNumVerts)					{ if (m_piNumVerts) delete[] m_piNumVerts; m_piNumVerts = piNumVerts;}
	void			SetIndexBufferOffset(int iOffset)					{ m_iOffset = iOffset; }

	eYifPrimType	GetPrimType()								const	{ return m_ePrimType;}
	int				GetNumPrims()								const	{ return m_iNumPrims;}
	int				GetMaterialIndex()							const	{ return m_iMatIndex;}
	const int*		GetNumVertsBuffer()							const	{ ISO_ASSERT(m_ePrimType==YIF_FANS || m_ePrimType==YIF_STRIPS || m_ePrimType==YIF_CONNECTED_LINES ? !!m_piNumVerts : !m_piNumVerts); return m_piNumVerts;}
	int				GetNumVertsOnPrim(const int index)			const;
	int				GetIndexBufferOffset()						const	{ return m_iOffset;}

	void			SetRegularPatchIndicesOffset(const int offset)		{ m_iIndicesOffsetRegularPatches = offset;}
	void			SetRegularPatchOffset(const int offset)				{ m_iOffsetRegularPatches = offset;}
	void			SetIrregularPatchIndicesOffset(const int offset)	{ m_iIndicesOffsetIrregularPatches = offset;}
	void			SetIrregularPatchOffset(const int offset)			{ m_iOffsetIrregularPatches = offset;}

	void			SetNrIrregularPatches(const int n)					{ m_iNrIrregularPatches = n;}
	void			SetNrRegularPatches(const int n)					{ m_iNrRegularPatches = n;}

	int				GetRegularPatchIndicesOffset()				const	{ return m_iIndicesOffsetRegularPatches;}
	int				GetRegularPatchOffset()						const	{ return m_iOffsetRegularPatches;}
	int				GetIrregularPatchIndicesOffset()			const	{ return m_iIndicesOffsetIrregularPatches;}
	int				GetIrregularPatchOffset()					const	{ return m_iOffsetIrregularPatches;}
	int				GetNrIrregularPatches()						const	{ return m_iNrIrregularPatches;}
	int				GetNrRegularPatches()						const	{ return m_iNrRegularPatches;}

	CYifSurface();
	~CYifSurface();
};

class CYifMesh {
	string			m_pMeshName;

	int				m_iNumSurfaces;
	CYifSurface		*m_pSurfaces;

	SYifVertex		*m_pVertices;	// array of unique vertices
	SYifSkinVertInfo *m_pWeightVertsInfo;
	int				m_iNumOfIndices;
	int				*m_piIndices;

	int				*m_piPatchesIndices;		// contains both regular and irregular patches of all surfaces
	uint32			*m_puPrefixes;	// 4 per uint
	uint32			*m_puValences;	// 4 per uint

	float4x4		*m_pfInvMats;
	int				*m_piOffsToNameTable;
	char			*m_pcNamesData;
	int				m_iNrBones;
public:
	bool			SetNumSurfaces(const int iNumSurfaces);
	CYifSurface*	GetSurfRef(const int index)				{ return index>=0 && index<m_iNumSurfaces ? &m_pSurfaces[index] : 0; }

	void			SetVertices(SYifVertex *p)				{ m_pVertices = p;}
	void			SetSkinVertInfo(SYifSkinVertInfo *p)	{ m_pWeightVertsInfo = p;}
	void			SetIndices(int *p, const int n)			{ m_piIndices = p;	m_iNumOfIndices = n;}
	void			SetName(const char *pName)				{ m_pMeshName = pName; }

	int				GetNumIndices() const					{ return m_iNumOfIndices;}
	int				GetNumSurfaces() const					{ return m_iNumSurfaces;}

	const char*		GetName()						const	{ return m_pMeshName;}
	const SYifVertex* GetVertices()					const	{ return m_pVertices;}
	const SYifSkinVertInfo * GetSkinVertices()		const	{ return m_pWeightVertsInfo;}
	const int*		GetIndices()					const	{ return m_piIndices;}

	int				GetNumBones()					const	{ return m_iNrBones;}
	const char*		GetBoneName(const int index)	const	{ return m_pcNamesData+m_piOffsToNameTable[index];}
	const float4x4*	GetInvMats()					const;

	void			SetBonesInvMats(float4x4 *p)			{ ISO_ASSERT(!m_pfInvMats); m_pfInvMats = p;}
	void			SetBonesOffsToNameTable(int *p)			{ ISO_ASSERT(!m_piOffsToNameTable); m_piOffsToNameTable = p;}
	void			SetNamesData(char *p)					{ ISO_ASSERT(!m_pcNamesData); m_pcNamesData = p;}
	void			SetNumBones(const int n)				{ ISO_ASSERT(m_iNrBones==0 && n>0); m_iNrBones = n;}
	void			SetPatchIndices(int *p)					{ m_piPatchesIndices = p;}
	void			SetPatchPrefixes(uint32 *p)				{ m_puPrefixes = p;}
	void			SetPatchValences(uint32 *p)				{ m_puValences = p;}
	int*			GetPatchIndices()				const	{ return m_piPatchesIndices;}
	uint32*			GetPatchPrefixes()				const	{ return m_puPrefixes;}
	uint32*			GetPatchValences()				const	{ return m_puValences;}

	void CleanUp();
	CYifMesh();
	~CYifMesh();
};

class CYifTransformation {
	friend class CYifSceneReader;

	float			m_fDuration;
	int				m_iNrKeys;
	float4x4		*m_pLocToParent;		// only initialized when m_iNrKeys==1
	quaternion		*m_pQuaternions;
	float3p			*m_pPositions;
	float3p			*m_pScaleValues;

	int				m_iIndexToFirstChild;	// -1 means no child
	int				m_iIndexToNextSibling;	// -1 means no siblings
public:
	bool			SetNumKeys(const int iNrKeys);
	int				GetNumKeys() const{
	return m_iNrKeys;
}

	const quaternion* GetQuaternionBuffer()	const				{ return m_pQuaternions;}
	const float3p*	GetPositionBuffer()		const				{ return m_pPositions;}
	const float3p*	GetScaleValueBuffer()	const				{ return m_pScaleValues;}

	float4x4		EvalTransform(float t)	const;

	void			SetQuaternion(int key, const quaternion &q)	{ ISO_ASSERT(m_pQuaternions && key>=0 && key<m_iNrKeys); m_pQuaternions[key]=q;}
	quaternion		GetQuaternion(int key)	const				{ ISO_ASSERT(m_pQuaternions && key>=0 && key<m_iNrKeys); return m_pQuaternions[key];}
	void			SetPosition(int key, const float3 &v0)		{ ISO_ASSERT(m_pPositions && key>=0 && key<m_iNrKeys); m_pPositions[key] = v0;}
	float3			GetPosition(int key)	const				{ ISO_ASSERT(m_pPositions && key>=0 && key<m_iNrKeys); return float3(m_pPositions[key]);}
	void			SetScaleValue(int key, const float3 &s0)	{ ISO_ASSERT(m_pScaleValues && key>=0 && key<m_iNrKeys); m_pScaleValues[key] = s0;}

	float3			GetScaleValue(int key)	const				{ ISO_ASSERT(m_pScaleValues && key>=0 && key<m_iNrKeys); return float3(m_pScaleValues[key]);}
	void			SetDuration(float f)						{ m_fDuration = f;}
	float			GetDuration()			const				{ return m_fDuration;}

	// only works when m_iNrKeys==1
	void			SetLocalToParent(const float4x4 &mat)		{ ISO_ASSERT(m_pLocToParent); *m_pLocToParent = mat;}
	float4x4		GetLocalToParent()		const				{ return *m_pLocToParent;}

	void			ClearRot()									{ delete[] m_pQuaternions; m_pQuaternions=NULL; }
	void			ClearPositions()							{ delete[] m_pPositions; m_pPositions=NULL; }
	void			ClearScaleValues()							{ delete[] m_pScaleValues; m_pScaleValues=NULL; }

	void			SetFirstChild(int index)					{ m_iIndexToFirstChild = index;}
	void			SetNextSibling(int index)					{ m_iIndexToNextSibling = index;}

	int				GetFirstChild()			const				{ return m_iIndexToFirstChild;}
	int				GetNextSibling()		const				{ return m_iIndexToNextSibling;}

	void			CleanUp();
	CYifTransformation();
	~CYifTransformation();
};

class CYifMeshInstance {
	string			m_pMeshInstName;
	int				m_iMeshIndex;
	int				m_iTransformIndex;

	// length is determined by GetNumBones() on the referenced mesh
	int				*m_piBoneToTransform;					// reference to a transformation node
public:
	void			SetMeshIndex(int i)							{ m_iMeshIndex = i;}

	void			SetNodeTransformIndex(int i)				{ m_iTransformIndex = i;}
	void			SetMeshInstName(const char *pcName)			{ m_pMeshInstName = pcName; }
	bool			SetNumBones(int iNrBones);
	void			SetBoneNodeTransformIndex(int b, int i)		{ m_piBoneToTransform[b] = i;}

	int				GetBoneNodeTransformIndex(int i)	const	{ return m_piBoneToTransform[i];}
	const char*		GetName()							const	{ return m_pMeshInstName;}
	int				GetMeshIndex()						const	{ return m_iMeshIndex;}

	int				GetTransformIndex()					const	{ return m_iTransformIndex;}
	const int*		GetBoneToTransformIndices()			const	{ return m_piBoneToTransform;}

	void			CleanUp();
	CYifMeshInstance();
	~CYifMeshInstance();
};

class CYifScene {
	int				m_iNumMeshes;
	int				m_iNumMeshInstances;
	int				m_iNumTransformations;
	int				m_iNumLights;
	int				m_iNumCameras;

	CYifMesh			*m_pMeshes;
	CYifMeshInstance	*m_pMeshInstances;
	CYifTransformation	*m_pTransformations;	// transformation tree stored in pre-order
public:

	int					GetNumMeshes()			const { return m_iNumMeshes;}
	int					GetNumMeshInstances()	const { return m_iNumMeshInstances;}
	int					GetNumTransformations() const { return m_iNumTransformations;}
	int					GetNumCameras()			const { return m_iNumCameras;}
	int					GetNumLights()			const { return m_iNumLights;}

	bool				SetNumMeshes(int iNrMeshes);		// allocates memory
	CYifMesh*			GetMeshRef(int index);				// must be less than m_iNumMeshes

	bool				SetNumTransformations(int iNumTransformations);	// allocates memory
	CYifTransformation*	GetTransformationRef(int index);	// must be less than m_iNumTransformations

	bool				SetNumMeshInstances(int iNrMeshes);	// allocates memory
	CYifMeshInstance*	GetMeshInstanceRef(int index);		// must be less than m_iNumMeshInstances

	void				CleanUp();
	CYifScene();
	~CYifScene();
};

int CYifSurface::GetNumVertsOnPrim(const int index) const {
	int iRes = 0;
	switch(GetPrimType()) {
		case YIF_FANS:
		case YIF_STRIPS:
		case YIF_CONNECTED_LINES:
			if (m_piNumVerts && index>=0 && index<GetNumPrims())
				iRes = m_piNumVerts[index];
			break;
		case YIF_TRIANGLES:
			iRes = 3;
			break;
		case YIF_QUADS:
			iRes = 4;
			break;
		case YIF_LINES:
			iRes = 2;
			break;
		default:
			ISO_ASSERT(false);
	}
	return iRes;
}

CYifSurface::CYifSurface() {
	m_iMatIndex = 0;			// reference to material
	m_piNumVerts = NULL;

	m_iNumPrims = 0;
	m_iOffset = 0;

	m_iIndicesOffsetRegularPatches=0;
	m_iOffsetRegularPatches=0;
	m_iNrRegularPatches=0;
	m_iIndicesOffsetIrregularPatches=0;
	m_iOffsetIrregularPatches=0;
	m_iNrIrregularPatches=0;
}

CYifSurface::~CYifSurface() {
}

bool CYifMesh::SetNumSurfaces(const int iNumSurfaces) {
	ISO_ASSERT(iNumSurfaces>0 && !m_pSurfaces);
	if (iNumSurfaces<=0 || m_pSurfaces) {
		CleanUp();
		return false;
	}
	m_pSurfaces = new CYifSurface[iNumSurfaces];
	if (!m_pSurfaces) {
		CleanUp();
		return false;
	}
	m_iNumSurfaces = iNumSurfaces;
	return true;
}

void CYifMesh::CleanUp() {
	if (m_pfInvMats) {
		delete[] m_pfInvMats;
		m_pfInvMats=NULL;
	}

	if (m_piOffsToNameTable) {
		delete[] m_piOffsToNameTable;
		m_piOffsToNameTable=NULL;
	}

	if (m_pcNamesData) {
		delete[] m_pcNamesData;
		m_pcNamesData=NULL;
	}

	m_iNrBones = 0;
}

CYifMesh::CYifMesh() {
	m_iNumSurfaces = 0;
	m_pSurfaces = NULL;

	m_pVertices = NULL;			// array of unique vertices
	m_pWeightVertsInfo = NULL;
	m_iNumOfIndices = 0;
	m_piIndices = NULL;

	m_piPatchesIndices = NULL;
	m_puPrefixes = NULL;
	m_puValences = NULL;

	m_pfInvMats = NULL;
	m_piOffsToNameTable = NULL;
	m_pcNamesData = NULL;
	m_iNrBones = 0;
}

CYifMesh::~CYifMesh() {
}

bool CYifMeshInstance::SetNumBones(const int iNrBones) {
	bool bRes = false;
	ISO_ASSERT(!m_piBoneToTransform);
	m_piBoneToTransform = new int[iNrBones];
	if (m_piBoneToTransform) {
		for (int i = 0; i < iNrBones; i++)
			m_piBoneToTransform[i] = -1;
		bRes = true;
	}
	return bRes;
}

void CYifMeshInstance::CleanUp() {
	if (m_piBoneToTransform) {
		delete[] m_piBoneToTransform;
		m_piBoneToTransform=NULL;
	}
	m_iMeshIndex = 0;
	m_iTransformIndex = 0;
}

CYifMeshInstance::CYifMeshInstance() {
	m_piBoneToTransform = NULL;
	m_iMeshIndex = 0;
	m_iTransformIndex = 0;
}

CYifMeshInstance::~CYifMeshInstance(){
}

float4x4 CYifTransformation::EvalTransform(float t) const {
	if (m_iNrKeys<=1)
		return m_pLocToParent[0];

	position3	vP(zero);
	float3		vS(one);
	quaternion	Q(identity);

	// let's locate keys
	const float t_clmp = t < 0 ? 0 : t > 1 ? 1 : t;
	const int iNumKeysSubOne = GetNumKeys()-1;

	int i2 = (int) (t_clmp * iNumKeysSubOne);
	if (i2 == iNumKeysSubOne)
		--i2;		// too far, rewind
	int i3 = i2 + 1;
	int i1 = i2 == 0 ? i2 : i2 - 1;
	int i4 = i3 == iNumKeysSubOne ? i3 : i3 + 1;

	// generate interpolated result (Hermite)
	float t1 = t*iNumKeysSubOne - i2;	// time, relative to chosen polynomial
	float t2 = t1*t1;
	float t3 = t1*t2;

	float b = 3*t2 - 2*t3;
	float d = t3 - t2;
	float a = 1 - b;
	float c = d - t2 + t1;

	if (GetQuaternionBuffer())
		Q = slerp(GetQuaternion(i2), GetQuaternion(i3), t1);

	if (GetPositionBuffer()) {
		float3 P1 = GetPosition(i2);
		float3 P2 = GetPosition(i3);
		float3 R1 = P2 - GetPosition(i1);
		float3 R2 = GetPosition(i4) - P1;
		vP = position3(a*P1 + b*P2 + c*R1 + d*R2);
	}

	if (GetScaleValueBuffer()) {
		float3 S1 = GetScaleValue(i2);
		float3 S2 = GetScaleValue(i3);
		float3 R1 = S2 - GetScaleValue(i1);
		float3 R2 = GetScaleValue(i4) - S1;
		vS = a*S1 + b*S2 + c*R1 + d*R2;
	}

	return float4x4(translate(vP) * scale(vS) * Q);
}

void CYifTransformation::CleanUp() {
	ClearRot();
	ClearPositions();
	ClearScaleValues();

	if (m_pLocToParent) {
		delete[] m_pLocToParent;
		m_pLocToParent = NULL;
	}

	m_fDuration=0.0f;
	m_iNrKeys = 1;
	m_iIndexToFirstChild=-1;		// -1 means no child
	m_iIndexToNextSibling=-1;		// -1 means no siblings
}

bool CYifTransformation::SetNumKeys(const int iNrKeys) {	// allocates memory
	ISO_ASSERT(iNrKeys>0 && !m_pQuaternions && !m_pPositions && !m_pScaleValues && !m_pLocToParent);

	if (!(iNrKeys>0 && !m_pQuaternions && !m_pPositions && !m_pScaleValues && !m_pLocToParent)) {
		CleanUp();
		return false;
	}

	if (iNrKeys==1) {
		m_pLocToParent = new float4x4[1];
		if (!m_pLocToParent) {
			CleanUp();
			return false;
		}
	} else {
		m_pQuaternions	= new quaternion[iNrKeys];
		m_pPositions	= new float3p[iNrKeys];
		m_pScaleValues	= new float3p[iNrKeys];
		if (!m_pQuaternions || !m_pPositions || !m_pScaleValues) {
			CleanUp();
			return false;
		}
	}
	m_iNrKeys = iNrKeys;
	return true;
}

CYifTransformation::CYifTransformation() {
	m_fDuration=0.0f;
	m_pQuaternions=NULL;
	m_pPositions=NULL;
	m_pScaleValues=NULL;
	m_pLocToParent=NULL;
	m_iNrKeys=0;
	m_iIndexToFirstChild=-1;		// -1 means no child
	m_iIndexToNextSibling=-1;		// -1 means no siblings
}

CYifTransformation::~CYifTransformation() {
}

// allocates memory
bool CYifScene::SetNumMeshes(int iNrMeshes) {
	ISO_ASSERT(iNrMeshes>0 && !m_pMeshes);
	if (iNrMeshes<=0 || m_pMeshes) {
		CleanUp();
		return false;
	}
	m_pMeshes = new CYifMesh[iNrMeshes];
	if (!m_pMeshes) {
		CleanUp();
		return false;
	}
	m_iNumMeshes = iNrMeshes;
	return true;
}

CYifMesh *CYifScene::GetMeshRef(int index) {
	ISO_ASSERT(index>=0 && index<m_iNumMeshes);
	return index>=0 && index<m_iNumMeshes ? &m_pMeshes[index] : 0;
}

bool CYifScene::SetNumMeshInstances(int iNumMeshInstances) {
	ISO_ASSERT(iNumMeshInstances>0 && !m_pMeshInstances);
	if (iNumMeshInstances<=0 || m_pMeshInstances) {
		CleanUp();
		return false;
	}
	m_pMeshInstances = new CYifMeshInstance[iNumMeshInstances];
	if (!m_pMeshInstances) {
		CleanUp();
		return false;
	}
	m_iNumMeshInstances = iNumMeshInstances;
	return true;
}

CYifMeshInstance *CYifScene::GetMeshInstanceRef(int index) {
	ISO_ASSERT(index>=0 && index<m_iNumMeshInstances);
	return index>=0 && index<m_iNumMeshInstances ? &m_pMeshInstances[index] : 0;
}

bool CYifScene::SetNumTransformations(int iNumTransformations) {
	ISO_ASSERT(iNumTransformations>0 && !m_pTransformations);
	if (iNumTransformations<=0 || m_pTransformations) {
		CleanUp();
		return false;
	}
	m_pTransformations = new CYifTransformation[iNumTransformations];
	if (!m_pTransformations) {
		CleanUp();
		return false;
	}
	m_iNumTransformations = iNumTransformations;
	return true;
}

CYifTransformation *CYifScene::GetTransformationRef(int index) {
	ISO_ASSERT(index>=0 && index<m_iNumTransformations);
	return index>=0 && index<m_iNumTransformations ? &m_pTransformations[index] : 0;
}

void CYifScene::CleanUp() {
	for (int m=0; m<m_iNumMeshes; m++)
		m_pMeshes[m].CleanUp();

	for (int m=0; m<m_iNumMeshInstances; m++)
		m_pMeshInstances[m].CleanUp();

	delete[] m_pMeshes;
	m_pMeshes=NULL;

	delete[] m_pMeshInstances;
	m_pMeshInstances=NULL;

	delete[] m_pTransformations;
	m_pTransformations=NULL;

	m_iNumMeshes = 0;
	m_iNumMeshInstances = 0;
	m_iNumLights = 0;
	m_iNumCameras = 0;
	m_iNumTransformations = 0;
}

CYifScene::CYifScene() {
	m_iNumMeshes = 0;
	m_iNumMeshInstances = 0;
	m_iNumLights = 0;
	m_iNumCameras = 0;
	m_iNumTransformations = 0;

	m_pMeshes = NULL;
	m_pMeshInstances = NULL;
	m_pTransformations = NULL;
}

CYifScene::~CYifScene() {
}

class CYifSceneReader : public CYifScene {
	bool ParseTransformations(istream_ref fptr);
	bool ParseTransformation(CYifTransformation *pTransf, istream_ref fptr);

	bool ParseMeshInstances(istream_ref fptr);
	bool ParseMeshInstance(CYifMeshInstance *pMeshInst, istream_ref fptr);

	bool ParseMeshes(istream_ref fptr);
	bool ParseMesh(CYifMesh *pMesh, istream_ref fptr);
	bool ParseSurfaces(CYifMesh *pMesh, istream_ref fptr);
	bool ParseSurface(CYifSurface *pSurf, istream_ref fptr);
public:
	bool ReadScene(istream_ref fptr);
};

#define MAKE_ID(a,b,c,d)	( (((uint32) d)<<24) | (((uint32) c)<<16) | (((uint32) b)<<8) | (((uint32) a)<<0) )

enum eCHUNK_TYPE {
	CHUNK_UNKNOWN	= 0,
	CHUNK_YIF_		= MAKE_ID('Y','I','F','_'),
	CHUNK_MSHS		= MAKE_ID('M','S','H','S'),
	CHUNK_MCNT		= MAKE_ID('M','C','N','T'),
	CHUNK_MESH		= MAKE_ID('M','E','S','H'),
	CHUNK_MSTS		= MAKE_ID('M','S','T','S'),
	CHUNK_MINS		= MAKE_ID('M','I','N','S'),
	CHUNK_TRNS		= MAKE_ID('T','R','N','S'),
	CHUNK_TCNT		= MAKE_ID('T','C','N','T'),
	CHUNK_TRAN		= MAKE_ID('T','R','A','N'),
	CHUNK_NAME		= MAKE_ID('N','A','M','E'),
	CHUNK_VRTS		= MAKE_ID('V','R','T','S'),
	CHUNK_WGTS		= MAKE_ID('W','G','T','S'),
	CHUNK_BCNT		= MAKE_ID('B','C','N','T'),
	CHUNK_MINV		= MAKE_ID('M','I','N','V'),
	CHUNK_BNME		= MAKE_ID('B','N','M','E'),
	CHUNK_INDX		= MAKE_ID('I','N','D','X'),
	CHUNK_SRFS		= MAKE_ID('S','R','F','S'),
	CHUNK_SCNT		= MAKE_ID('S','C','N','T'),
	CHUNK_SURF		= MAKE_ID('S','U','R','F'),
	CHUNK_PCNT		= MAKE_ID('P','C','N','T'),
	CHUNK_PTYP		= MAKE_ID('P','T','Y','P'),
	CHUNK_IOFF		= MAKE_ID('I','O','F','F'),
	CHUNK_NVRT		= MAKE_ID('N','V','R','T'),
	CHUNK_MAIX		= MAKE_ID('M','A','I','X'),
	CHUNK_MEIX		= MAKE_ID('M','E','I','X'),
	CHUNK_TRIX		= MAKE_ID('T','R','I','X'),
	CHUNK_BTIX		= MAKE_ID('B','T','I','X'),
	CHUNK_KCNT		= MAKE_ID('K','C','N','T'),
	CHUNK_PMTX		= MAKE_ID('P','M','T','X'),
	CHUNK_QBUF		= MAKE_ID('Q','B','U','F'),
	CHUNK_PBUF		= MAKE_ID('P','B','U','F'),
	CHUNK_SBUF		= MAKE_ID('S','B','U','F'),
	CHUNK_CHIX		= MAKE_ID('C','H','I','X'),
	CHUNK_SIIX		= MAKE_ID('S','I','I','X'),
	CHUNK_DURA		= MAKE_ID('D','U','R','A'),
	CHUNK_PAIN		= MAKE_ID('P','A','I','N'),
	CHUNK_PPFX		= MAKE_ID('P','P','F','X'),
	CHUNK_PVLC		= MAKE_ID('P','V','L','C'),
	CHUNK_PRIO		= MAKE_ID('P','R','I','O'),
	CHUNK_PROF		= MAKE_ID('P','R','O','F'),
	CHUNK_PIIO		= MAKE_ID('P','I','I','O'),
	CHUNK_PIOF		= MAKE_ID('P','I','O','F'),
	CHUNK_PRCT		= MAKE_ID('P','R','C','T'),
	CHUNK_PICT		= MAKE_ID('P','I','C','T')
};

#define MAX_CHUNK_DEPTH		1024

static int g_iCurDepth;
static int g_piChunkStack[MAX_CHUNK_DEPTH];

static eCHUNK_TYPE PushChunk(istream_ref fptr, int *piChunk=NULL);
static void PopChunk(istream_ref fptr);
static void SkipChunk(istream_ref fptr);
static bool IsChunkDone(istream_ref fptr);

bool CYifSceneReader::ReadScene(istream_ref fptr) {
	// init chunk stack
	memset(g_piChunkStack, 0, sizeof(g_piChunkStack));
	g_iCurDepth = 0;

	// continue parsing
	eCHUNK_TYPE eChunkID = PushChunk(fptr);
	ISO_ASSERT(eChunkID==CHUNK_YIF_);
	while (!IsChunkDone(fptr)) {
		eChunkID = PushChunk(fptr);
		switch(eChunkID) {
			case CHUNK_MSHS:
				if (!ParseMeshes(fptr)) {
					CleanUp();
					return false;
				}
				break;
			case CHUNK_MSTS:
				if (!ParseMeshInstances(fptr)) {
					CleanUp();
					return false;
				}
				break;
			case CHUNK_TRNS:
				if (!ParseTransformations(fptr)) {
					CleanUp();
					return false;
				}
				break;
			default:
				SkipChunk(fptr);
		}
		PopChunk(fptr);
	}
	PopChunk(fptr);
	return true;
}

//---------------------------------------------------------------------
//------------------------ Parsing Meshes -----------------------------

bool CYifSceneReader::ParseMeshes(istream_ref fptr) {
	int iMeshIndex = 0;
	while (!IsChunkDone(fptr)) 	{
		eCHUNK_TYPE eChunkID = PushChunk(fptr);
		switch(eChunkID) {
			case CHUNK_MCNT: {
				int iNumMeshes;
				fptr.readbuff(&iNumMeshes, sizeof(int));
				if (!SetNumMeshes(iNumMeshes))
					return false;
				break;
			}
			case CHUNK_MESH: {
				ISO_ASSERT(iMeshIndex<GetNumMeshes());
				if (iMeshIndex<GetNumMeshes()) {
					CYifMesh *pMesh = GetMeshRef(iMeshIndex);	// must be less than m_iNumMeshes
					if (!ParseMesh(pMesh, fptr))
						return false;
					++iMeshIndex;
				}
				break;
			}
			default:
				SkipChunk(fptr);
		}
		PopChunk(fptr);
	}
	return true;
}

bool CYifSceneReader::ParseMesh(CYifMesh *pMesh, istream_ref fptr) {
	while (!IsChunkDone(fptr)) {
		int iChunkSize = 0;
		eCHUNK_TYPE eChunkID = PushChunk(fptr, &iChunkSize);
		switch(eChunkID) {
			case CHUNK_SRFS:
				if (!ParseSurfaces(pMesh, fptr))
					return false;
				break;
			case CHUNK_BCNT: {
				int iNumBones;
				fptr.readbuff(&iNumBones, sizeof(int));
				pMesh->SetNumBones(iNumBones);
				break;
			}
			case CHUNK_VRTS: {
				bool bRes=false;
				if (iChunkSize>0) {
					const int iNrVerts = iChunkSize / sizeof(SYifVertex);
					SYifVertex *pvVerts = new SYifVertex[iNrVerts];
					if (pvVerts) {
						fptr.readbuff(pvVerts, sizeof(SYifVertex) *iNrVerts);

						// normalize the vertex positions to a bounding box from [-1,-1,-1] to [1,1,1]

						float3 mini = float3(pvVerts[0].vPos);
						float3 maxi = float3(pvVerts[0].vPos);
						for (uint32 i=1; i<iNrVerts; ++i) {
							mini = min(mini, float3(pvVerts[i].vPos));
							maxi = max(mini, float3(pvVerts[i].vPos));
						}
						const float3 size = maxi - mini;
						const float factor = 2.f / max(size.x, max(size.y, size.z));
						for (uint32 i=0; i<iNrVerts; ++i)
							pvVerts[i].vPos = float3(pvVerts[i].vPos) *factor;

						// normalization done.

						pMesh->SetVertices(pvVerts);
						bRes=true;
					}
				}
				if (!bRes)
					SkipChunk(fptr);
				break;
			}
			case CHUNK_INDX: {
				bool bRes=false;
				if (iChunkSize>0) {
					const int iNrIndices = iChunkSize / sizeof(int);
					int *piIndices = new int[iNrIndices];
					if (piIndices) {
						fptr.readbuff(piIndices, sizeof(int) * iNrIndices);
						pMesh->SetIndices(piIndices, iNrIndices);
						bRes=true;
					}
				}
				if (!bRes)
					SkipChunk(fptr);
				break;
			}
			case CHUNK_NAME: {
				bool bRes=false;
				if (iChunkSize>0) {
					const int iNrCharacters = iChunkSize / sizeof(char);	// includes '\0'
					char *pcName = new char[iNrCharacters];
					if (pcName) {
						fptr.readbuff(pcName, sizeof(char) * iNrCharacters);
						pMesh->SetName(pcName);
						delete[] pcName;
						bRes=true;
					}
				}
				if (!bRes)
					SkipChunk(fptr);
				break;
			}
			case CHUNK_WGTS: {
				bool bRes=false;
				if (iChunkSize>0) {
					const int iNrVertices = iChunkSize / sizeof(SYifSkinVertInfo);
					SYifSkinVertInfo *pvWeightVerts = new SYifSkinVertInfo[iNrVertices];
					if (pvWeightVerts) {
						fptr.readbuff(pvWeightVerts, sizeof(SYifSkinVertInfo) * iNrVertices);
						pMesh->SetSkinVertInfo(pvWeightVerts);
						bRes=true;
					}
				}
				if (!bRes)
					SkipChunk(fptr);
				break;
			}
			case CHUNK_MINV: {
				bool bRes=false;
				if (iChunkSize>0) {
					const int iNrMats = iChunkSize / sizeof(float4x4);
					float4x4 *pfInvMats = new float4x4[iNrMats];
					if (pfInvMats) {
						fptr.readbuff(pfInvMats, sizeof(float4x4) * iNrMats);
						pMesh->SetBonesInvMats(pfInvMats);
						bRes=true;
					}
				}
				if (!bRes)
					SkipChunk(fptr);
				break;
			}
			case CHUNK_BNME: {
				bool bRes=false;
				if (iChunkSize>0) {
					const int iStringSize = iChunkSize / sizeof(char);
					char *pcNames = new char[iStringSize];
					if (pcNames) {
						// set names
						fptr.readbuff(pcNames, sizeof(char) * iStringSize);
						pMesh->SetNamesData(pcNames);

						// Create offset to name table
						int iNrNames, iOffset;
						iNrNames=0; iOffset = 0;
						while (iOffset<iChunkSize) {
							const int iNameLen = (const int) strlen(pcNames+iOffset)+1;
							iOffset += iNameLen; ++iNrNames;
						}
						ISO_ASSERT(iOffset==iChunkSize);
						int *piOffsetToName = new int[iNrNames];
						if (piOffsetToName) {
							iOffset = 0;
							for (int i=0; i<iNrNames; i++) {
								piOffsetToName[i]=iOffset;
								iOffset += ((const int) strlen(pcNames+iOffset)+1);
							}
							pMesh->SetBonesOffsToNameTable(piOffsetToName);
							const int iNrBones = pMesh->GetNumBones();
							ISO_ASSERT(iNrBones==0 || iNrBones==iNrNames);

							bRes=true;
						}
					}
				}
				if (!bRes)
					SkipChunk(fptr);
				break;
			}
			case CHUNK_PAIN: {
				bool bRes=false;
				if (iChunkSize>0) {
					const int iNrPatchIndices = iChunkSize / sizeof(int);
					int *piPatchIndices = new int[iNrPatchIndices];
					if (piPatchIndices) {
						fptr.readbuff(piPatchIndices, sizeof(int) * iNrPatchIndices);
						pMesh->SetPatchIndices(piPatchIndices);
						bRes=true;
					}
				}
				if (!bRes)
					SkipChunk(fptr);
				break;
			}
			case CHUNK_PPFX: {
				bool bRes=false;
				if (iChunkSize>0) {
					const int iNrPatchPrefixes = iChunkSize / sizeof(uint32);
					uint32 *puPatchPrefixes = new uint32[iNrPatchPrefixes];
					if (puPatchPrefixes) {
						fptr.readbuff(puPatchPrefixes, sizeof(uint32) * iNrPatchPrefixes);
						pMesh->SetPatchPrefixes(puPatchPrefixes);
						bRes=true;
					}
				}
				if (!bRes)
					SkipChunk(fptr);
				break;
			}
			case CHUNK_PVLC: {
				bool bRes=false;
				if (iChunkSize>0) {
					const int iNrPatchValences = iChunkSize / sizeof(uint32);
					uint32 *puPatchValences = new uint32[iNrPatchValences];
					if (puPatchValences) {
						fptr.readbuff(puPatchValences, sizeof(uint32) * iNrPatchValences);
						pMesh->SetPatchValences(puPatchValences);
						bRes=true;
					}
				}
				if (!bRes)
					SkipChunk(fptr);
				break;
			}
			default:
				SkipChunk(fptr);
		}
		PopChunk(fptr);
	}

	return true;
}

bool CYifSceneReader::ParseSurfaces(CYifMesh *pMesh, istream_ref fptr) {
	int iSurfIndex = 0;
	while (!IsChunkDone(fptr)) {
		eCHUNK_TYPE eChunkID = PushChunk(fptr);
		switch(eChunkID) {
			case CHUNK_SCNT: {
				int iNumSurfaces;
				fptr.readbuff(&iNumSurfaces, sizeof(int));
				if (!pMesh->SetNumSurfaces(iNumSurfaces)) return false;
				break;
			}
			case CHUNK_SURF: {
				ISO_ASSERT(iSurfIndex<pMesh->GetNumSurfaces());
				if (iSurfIndex<pMesh->GetNumSurfaces()) {
					CYifSurface *pSurf = pMesh->GetSurfRef(iSurfIndex);	// must be less than m_iNumMeshes
					if (!ParseSurface(pSurf, fptr)) return false;
					++iSurfIndex;
				}
				break;
			}
			default:
				SkipChunk(fptr);
		}
		PopChunk(fptr);
	}
	return true;
}

bool CYifSceneReader::ParseSurface(CYifSurface *pSurf, istream_ref fptr) {
	while (!IsChunkDone(fptr)) {
		int iChunkSize = 0;
		eCHUNK_TYPE eChunkID = PushChunk(fptr, &iChunkSize);
		switch(eChunkID) {
			case CHUNK_PCNT: {
				int iPrimCount;
				fptr.readbuff(&iPrimCount, sizeof(int));
				pSurf->SetNumPrims(iPrimCount);
				break;
			}
			case CHUNK_PTYP: {
				eYifPrimType prim_type;
				fptr.readbuff(&prim_type, sizeof(eYifPrimType));
				pSurf->SetPrimType(prim_type);
				break;
			}
			case CHUNK_IOFF: {
				int iOffset;	// offset into mesh index buffer
				fptr.readbuff(&iOffset, sizeof(int));
				pSurf->SetIndexBufferOffset(iOffset);
				break;
			}
			case CHUNK_MAIX: {
				int iMatIndex;	// material index
				fptr.readbuff(&iMatIndex, sizeof(int));
				//pSurf->SetMaterialIndex();
				break;
			}
			case CHUNK_NVRT: {
				bool bRes=false;
				if (iChunkSize>0) {
					const int iNrNums = iChunkSize / sizeof(int);
					int *piNumVertsOnPrim = new int[iNrNums];
					if (piNumVertsOnPrim) {
						fptr.readbuff(piNumVertsOnPrim, sizeof(int) * iNrNums);
						pSurf->SetNumVertsPerPrim(piNumVertsOnPrim);
						bRes=true;
					}
				}
				if (!bRes) SkipChunk(fptr);
				break;
			}
			case CHUNK_PRIO: {
				int iOffset=0;
				fptr.readbuff(&iOffset, sizeof(int));
				pSurf->SetRegularPatchIndicesOffset(iOffset);
				break;
			}
			case CHUNK_PROF: {
				int iOffset=0;
				fptr.readbuff(&iOffset, sizeof(int));
				pSurf->SetRegularPatchOffset(iOffset);
				break;
			}
			case CHUNK_PIIO: {
				int iOffset=0;
				fptr.readbuff(&iOffset, sizeof(int));
				pSurf->SetIrregularPatchIndicesOffset(iOffset);
				break;
			}
			case CHUNK_PIOF: {
				int iOffset=0;
				fptr.readbuff(&iOffset, sizeof(int));
				pSurf->SetIrregularPatchOffset(iOffset);
				break;
			}
			case CHUNK_PRCT: {
				int iCount=0;
				fptr.readbuff(&iCount, sizeof(int));
				pSurf->SetNrRegularPatches(iCount);
				break;
			}
			case CHUNK_PICT: {
				int iCount=0;
				fptr.readbuff(&iCount, sizeof(int));
				pSurf->SetNrIrregularPatches(iCount);
				break;
			}
			default:
				SkipChunk(fptr);
		}
		PopChunk(fptr);
	}
	return true;
}

//---------------------------------------------------------------------
//-------------------- Parsing Mesh Instances -------------------------

bool CYifSceneReader::ParseMeshInstances(istream_ref fptr) {
	int iMeshInstIndex = 0;
	while (!IsChunkDone(fptr)) {
		eCHUNK_TYPE eChunkID = PushChunk(fptr);
		switch(eChunkID) {
			case CHUNK_MCNT: {
				int iNumMeshInstances;
				fptr.readbuff(&iNumMeshInstances, sizeof(int));
				if (!SetNumMeshInstances(iNumMeshInstances)) return false;
				break;
			}
			case CHUNK_MINS: {
				ISO_ASSERT(iMeshInstIndex<GetNumMeshInstances());
				if (iMeshInstIndex<GetNumMeshInstances()) {
					CYifMeshInstance *pMeshInst = GetMeshInstanceRef(iMeshInstIndex);	// must be less than m_iNumMeshes
					if (!ParseMeshInstance(pMeshInst, fptr)) return false;
					++iMeshInstIndex;
				}
				break;
			}
			default:
				SkipChunk(fptr);
		}
		PopChunk(fptr);
	}
	return true;
}

bool CYifSceneReader::ParseMeshInstance(CYifMeshInstance *pMeshInst, istream_ref fptr) {
	while (!IsChunkDone(fptr)) {
		int iChunkSize=0;
		eCHUNK_TYPE eChunkID = PushChunk(fptr, &iChunkSize);
		switch(eChunkID) {
			case CHUNK_NAME: {
				bool bRes=false;
				if (iChunkSize>0) {
					const int iNrCharacters = iChunkSize / sizeof(char);	// includes '\0'
					char *pcName = new char[iNrCharacters];
					if (pcName) {
						fptr.readbuff(pcName, iNrCharacters);
						pMeshInst->SetMeshInstName(pcName);
						delete[] pcName;
						bRes=true;
					}
				}
				if (!bRes) SkipChunk(fptr);
				break;
			}
			case CHUNK_MEIX: {
				int iMeshIndex = 0;
				fptr.readbuff(&iMeshIndex, sizeof(int));
				pMeshInst->SetMeshIndex(iMeshIndex);
				break;
			}
			case CHUNK_TRIX: {
				int iTransformIndex = 0;
				fptr.readbuff(&iTransformIndex, sizeof(int));
				pMeshInst->SetNodeTransformIndex(iTransformIndex);
				break;
			}
			case CHUNK_BTIX: {
				bool bRes=false;
				if (iChunkSize>0) {
					const int iNrIndices = iChunkSize / sizeof(int);
					if (pMeshInst->SetNumBones(iNrIndices)==true) {
						const int iNR_MAX = 16;
						int piIndices[iNR_MAX];
						int iOffs = 0;
						while (iOffs<iNrIndices) {
							const int iReadIn = iNrIndices-iOffs;
							const int iClampedReadIn = iReadIn>iNR_MAX?iNR_MAX:iReadIn;
							fptr.readbuff(piIndices, sizeof(int) * iClampedReadIn);

							for (int i=0; i<iClampedReadIn; i++)
								pMeshInst->SetBoneNodeTransformIndex(iOffs+i, piIndices[i]);

							iOffs += iClampedReadIn;
						}
					}
				}
				if (!bRes) SkipChunk(fptr);
				break;
			}
			default:
				SkipChunk(fptr);
		}
		PopChunk(fptr);
	}
	return true;
}

//---------------------------------------------------------------------
//---------------- Parsing Transformation Instances -------------------

bool CYifSceneReader::ParseTransformations(istream_ref fptr) {
	int iNodeIndex = 0;
	while (!IsChunkDone(fptr)) {
		eCHUNK_TYPE eChunkID = PushChunk(fptr);
		switch(eChunkID) {
			case CHUNK_TCNT: {
				int iNumTransformationNodes;
				fptr.readbuff(&iNumTransformationNodes, sizeof(int));
				if (!SetNumTransformations(iNumTransformationNodes))
					return false;
				break;
			}
			case CHUNK_TRAN: {
				ISO_ASSERT(iNodeIndex<GetNumTransformations());
				if (iNodeIndex<GetNumTransformations()) {
					CYifTransformation *pTransf = GetTransformationRef(iNodeIndex);	// must be less than m_iNumMeshes
					if (!ParseTransformation(pTransf, fptr))
						return false;
					++iNodeIndex;
				}
				break;
			}
			default:
				SkipChunk(fptr);
		}
		PopChunk(fptr);
	}
	return true;
}

bool CYifSceneReader::ParseTransformation(CYifTransformation *pTransf, istream_ref fptr) {
	bool bGotPositions = false;
	bool bGotQuaternions = false;
	bool bGotScaleValues = false;
	while (!IsChunkDone(fptr)) {
		eCHUNK_TYPE eChunkID = PushChunk(fptr);
		switch(eChunkID) {
			case CHUNK_KCNT: {
				int iNumKeys;
				fptr.readbuff(&iNumKeys, sizeof(int));
				if (!pTransf->SetNumKeys(iNumKeys)) return false;
				break;
			}
			case CHUNK_DURA: {
				float fDuration=0.0f;
				fptr.readbuff(&fDuration, sizeof(float));
				pTransf->SetDuration(fDuration);
				break;
			}
			case CHUNK_PMTX: {
				ISO_ASSERT(pTransf->m_pLocToParent);
				if (pTransf->m_pLocToParent)
					fptr.readbuff(pTransf->m_pLocToParent, sizeof(float)*16);
				break;
			}
			case CHUNK_QBUF: {
				int iNumKeys = pTransf->GetNumKeys();
				ISO_ASSERT(pTransf->m_pQuaternions && iNumKeys>0);
				if (pTransf->m_pQuaternions && iNumKeys>0) {
					fptr.readbuff(pTransf->m_pQuaternions, sizeof(quaternion) * iNumKeys);
					bGotQuaternions = true;
				}
				break;
			}
			case CHUNK_PBUF: {
				int iNumKeys = pTransf->GetNumKeys();
				ISO_ASSERT(pTransf->m_pPositions && iNumKeys>0);
				if (pTransf->m_pPositions && iNumKeys>0) {
					fptr.readbuff(pTransf->m_pPositions, sizeof(float3p) * iNumKeys);
					bGotPositions = true;
				}
				break;
			}
			case CHUNK_SBUF: {
				int iNumKeys = pTransf->GetNumKeys();
				ISO_ASSERT(pTransf->m_pScaleValues && iNumKeys>0);
				if (pTransf->m_pScaleValues && iNumKeys>0) {
					fptr.readbuff(pTransf->m_pScaleValues, sizeof(float3p) * iNumKeys);
					bGotScaleValues = true;
				}
				break;
			}
			case CHUNK_CHIX: {
				int iIndexToFirstChild = 0;
				fptr.readbuff(&iIndexToFirstChild, sizeof(int));
				pTransf->SetFirstChild(iIndexToFirstChild);
				break;
			}
			case CHUNK_SIIX: {
				int iIndexToNextSibling = 0;
				fptr.readbuff(&iIndexToNextSibling, sizeof(int));
				pTransf->SetNextSibling(iIndexToNextSibling);
				break;
			}
			default:
				SkipChunk(fptr);
		}
		PopChunk(fptr);
	}

	if (pTransf->GetNumKeys()>1) {
		if (!bGotPositions) pTransf->ClearPositions();
		if (!bGotQuaternions) pTransf->ClearRot();
		if (!bGotScaleValues) pTransf->ClearScaleValues();
	}
	return true;
}

//---------------------------------------------------------------------
eCHUNK_TYPE IdentifyChunk(const char cChunk[]);

static eCHUNK_TYPE PushChunk(istream_ref fptr, int *piChunk) {
	eCHUNK_TYPE eChunkID = CHUNK_UNKNOWN;

	ISO_ASSERT(g_iCurDepth<MAX_CHUNK_DEPTH);
	if (g_iCurDepth<MAX_CHUNK_DEPTH) {
		char cChunk[4];
		fptr.readbuff(cChunk, 4);

		// identify chunk
		eChunkID = IdentifyChunk(cChunk);

		int iChunkSize;		// not including the chunkID nor the 4 bytes to the size_t itself
		fptr.readbuff(&iChunkSize, 4);
		if (piChunk) *piChunk = iChunkSize;

		g_piChunkStack[g_iCurDepth]=fptr.tell()+iChunkSize;		// predicted end of chunk
		++g_iCurDepth;
	}

	return eChunkID;
}

static void PopChunk(istream_ref fptr) {
	ISO_ASSERT(g_iCurDepth>0);
	ISO_ASSERT(IsChunkDone(fptr));
	if (g_iCurDepth>0) {
		--g_iCurDepth;
		int iPredictedLocation = g_piChunkStack[g_iCurDepth];
		ISO_ASSERT(iPredictedLocation == fptr.tell());
	}
}

static bool IsChunkDone(istream_ref fptr) {
	ISO_ASSERT(g_iCurDepth>0);
	if (g_iCurDepth<1) return true;
	const int iEndOfChunk = g_piChunkStack[g_iCurDepth-1];
	const int iCurLocation = fptr.tell();
	ISO_ASSERT(iCurLocation<=iEndOfChunk);
	return iCurLocation>=iEndOfChunk;
}

static void SkipChunk(istream_ref fptr) {
	ISO_ASSERT(g_iCurDepth>0);
	if (g_iCurDepth>0) {
		const int iEndOfChunk = g_piChunkStack[g_iCurDepth-1];
		fptr.seek(iEndOfChunk);
	}
}


eCHUNK_TYPE IdentifyChunk(const char cChunk[]) {
	return (eCHUNK_TYPE) MAKE_ID(cChunk[0], cChunk[1], cChunk[2], cChunk[3]);
}

//-----------------------------------------------------------------------------
//	convert to iso
//-----------------------------------------------------------------------------

template<int C> struct SYifVertexC;
template<> struct SYifVertexC<1> { float3p	vNorm;		void	operator=(const SYifVertex &v) { vNorm		= v.vNorm;		} };
template<> struct SYifVertexC<2> { float4p	vTangent;	void	operator=(const SYifVertex &v) { vTangent	= v.vTangent;	} };
template<> struct SYifVertexC<4> { float4p	vColor;		void	operator=(const SYifVertex &v) { vColor		= v.vColor;		} };
template<> struct SYifVertexC<8> { float2p	vTex;		void	operator=(const SYifVertex &v) { vTex		= v.vTex;		} };

template<int F> struct SYifVertexF	: SYifVertexF<clear_lowest(F)>, SYifVertexC<lowest_set(F)> {
	void	operator=(const SYifVertex &v) {
		SYifVertexC<T_lowest_set<F>::value>::operator=(v);
		SYifVertexF<T_clear_lowest<F>::value>::operator=(v);
	}
};
template<> struct SYifVertexF<0>	{
	float3p vPos;
	void	operator=(const SYifVertex &v) { vPos = v.vPos; }
};

ISO_DEFCOMP(SYifVertex, 5) {
	ISO_SETFIELDX(0, vPos,		"position");
	ISO_SETFIELDX(1, vNorm,		"normal");
	ISO_SETFIELDX(2, vTangent,	"tangent");
	ISO_SETFIELDX(3, vColor,	"colour");
	ISO_SETFIELDX(4, vTex,		"texcoord");
} };

ISO_DEFUSERX(SYifVertexC<1>, float3p, "normal");
ISO_DEFUSERX(SYifVertexC<2>, float4p, "tangent");
ISO_DEFUSERX(SYifVertexC<4>, float4p, "colour");
ISO_DEFUSERX(SYifVertexC<8>, float2p, "texcoord");

#define ISO_SETFIELDX2(i,e,n)		fields[i].set<_S>(tag(n), &_S::e)

template<typename _S, int C> struct SetField;
template<typename _S> struct SetField<_S, 1> { static void f(ISO::Element *fields) { ISO_SETFIELDX2(0, vNorm,	"normal"); } };
template<typename _S> struct SetField<_S, 2> { static void f(ISO::Element *fields) { ISO_SETFIELDX2(0, vTangent,"tangent"); } };
template<typename _S> struct SetField<_S, 4> { static void f(ISO::Element *fields) { ISO_SETFIELDX2(0, vColor,	"colour"); } };
template<typename _S> struct SetField<_S, 8> { static void f(ISO::Element *fields) { ISO_SETFIELDX2(0, vTex,	"texcoord"); } };

template<typename _S, int F> struct SetFields {
	static ISO::Element *f(ISO::Element *fields) {
		fields = SetFields<_S, T_clear_lowest<F>::value>::f(fields);
		SetField<_S, T_lowest_set<F>::value>::f(fields);
		return fields + 1;
	}
};
template<typename _S> struct SetFields<_S, 0> {
	static ISO::Element *f(ISO::Element *fields) { ISO_SETFIELDX2(0, vPos,	"position"); return fields + 1; }
};
template<int F> struct ISO::def<SYifVertexF<F> > : public ISO::TypeCompositeN<count_bits_v<F> + 1> {
	def() : ISO::TypeCompositeN<count_bits_v<F> + 1>(NONE, log2alignment<SYifVertexF<F>>) {
		SetFields<SYifVertexF<F>, F>::f(fields);
	}
};

ISO_ptr<Model>	MakeModel(CYifMesh *mesh) {
	ISO_ptr<Model>	model(mesh->GetName());

	for (int s = 0, sn = mesh->GetNumSurfaces(); s < sn; s++) {
		CYifSurface			*surf	= mesh->GetSurfRef(s);
		eYifPrimType		prim	= surf->GetPrimType();

		anything			params;
		interval<int>		r;

		if (surf->GetNrIrregularPatches() || surf->GetNrRegularPatches()) {
			// subdivision
		#if 1
			if (int n = surf->GetNrRegularPatches()) {
				ISO_ptr<SubMeshN<16> >	sm(0);

				const int	*ix		= mesh->GetPatchIndices() + surf->GetRegularPatchIndicesOffset();
				r = get_extent(ix, ix + n * 16);

				SubMeshN<16>::face *f	= sm->indices.Create(n, false);
				for (int i = 0; i < n * 16; i++)
					f[0][i] = ix[i] - r.a;

				ISO_ptr<ISO_openarray<SYifVertexF<0> > >	v(0);
				sm->verts	= v;
				copy_n(mesh->GetVertices() + r.a, v->Create(r.extent()).begin(), r.extent());

				ISO_ptr<ISO_openarray<SYifVertexC<8> > >	uv_table("uv_table", r.extent());
				params.Append(MakePtrIndirect(uv_table, ISO::getdef<DataBuffer>()));
				copy_n(mesh->GetVertices() + r.a, uv_table->begin(), r.extent());

				sm->technique	= ISO::MakePtrExternal<technique>("D:\\dev\\shared\\assets\\shaders\\catmullclark.fx;bezier");
				sm->parameters	= AnythingToStruct(params);
				sm->SetVertsPerPrim(16);
				sm.SetFlags(ISO::Value::HASEXTERNAL);
				model.SetFlags(ISO::Value::HASEXTERNAL);
				model->submeshes.Append(sm);
			}
		#endif
		#if 1
			if (int n = surf->GetNrIrregularPatches()) {
				ISO_ptr<SubMeshN<32>>	sm(0);

				ISO_ptr<ISO_openarray<xint32[2]> >	preval("prefix_valency", n);
				params.Append(MakePtrIndirect(preval, ISO::getdef<DataBuffer>()));

				const uint32	*prefixes	= mesh->GetPatchPrefixes() + surf->GetIrregularPatchOffset();
				const uint32	*valencies	= mesh->GetPatchValences() + surf->GetIrregularPatchOffset();
				const int		*ix			= mesh->GetPatchIndices() + surf->GetIrregularPatchIndicesOffset();
				r = get_extent(ix, ix + n * 32);

				SubMeshN<32>::face	*f		= sm->indices.Create(n, false);
				for (int p = 0; p < n; p++) {
					for (int k = 0; k < 32; k++)
						f[0][p * 32 + k] = ix[p * 32 + k] - r.a;
					(*preval)[p][0] = prefixes[p];
					(*preval)[p][1] = valencies[p];
				}

				ISO_ptr<ISO_openarray<SYifVertexF<0> > >	v(0);
				sm->verts	= v;
				copy_n(mesh->GetVertices() + r.a, v->Create(r.extent()).begin(), r.extent());

				ISO_ptr<ISO_openarray<float2p> >	uv_table("uv_table", r.extent());
				params.Append(MakePtrIndirect(uv_table, ISO::getdef<DataBuffer>()));
				transform(mesh->GetVertices() + r.a, mesh->GetVertices() + r.b + 1, uv_table->begin(), [](const SYifVertex &v) {
					return v.vTex;
				});

				sm->technique	= ISO::MakePtrExternal<technique>("D:\\dev\\shared\\assets\\shaders\\catmullclark.fx;catmullclark");
				sm->parameters	= AnythingToStruct(params);
				sm->SetVertsPerPrim(32);
				sm.SetFlags(ISO::Value::HASEXTERNAL);
				model.SetFlags(ISO::Value::HASEXTERNAL);
				model->submeshes.Append(sm);
			}
		#endif
		} else {
			// normal mesh
			ISO_ptr<SubMesh>	sm(0);
			int					nprims	= surf->GetNumPrims();
			const int*			ix		= mesh->GetIndices() + surf->GetIndexBufferOffset();

			switch (prim) {
				case YIF_TRIANGLES: {
					SubMesh::face *f	= sm->indices.Create(nprims, false);
					r = get_extent(ix, ix + nprims * 3);
					for (int i = nprims; i--; f += 1, ix += 3) {
						f[0][0] = ix[0] - r.a;
						f[0][1] = ix[1] - r.a;
						f[0][2] = ix[2] - r.a;
					}
					break;
				}
				case YIF_QUADS: {
					SubMesh::face *f	= sm->indices.Create(nprims * 2, false);
					r = get_extent(ix, ix + nprims * 4);
					for (int i = nprims; i--; f += 2, ix += 4) {
						f[0][0] = ix[0] - r.a;
						f[0][1] = ix[1] - r.a;
						f[0][2] = ix[2] - r.a;
						f[1][0] = ix[0] - r.a;
						f[1][1] = ix[2] - r.a;
						f[1][2] = ix[3] - r.a;
					}
					break;
				}
				case YIF_FANS:				break;
				case YIF_STRIPS:			break;
				case YIF_LINES:				break;
				case YIF_CONNECTED_LINES:	break;
			}

			ISO_ptr<ISO_openarray<SYifVertex> >	v(0);
			sm->verts	= v;
			copy_n(mesh->GetVertices() + r.a, v->Create(r.extent()).begin(), r.extent());
			sm->technique	= ISO::root("data")["simple"]["lite"];
			model->submeshes.Append(sm);
		}
	}

	model->UpdateExtents();
	return model;
}

class YIFFileHandler : FileHandler {
	const char*		GetExt() override { return "yif";	}
	const char*		GetDescription() override { return "YIF";	}
	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		CYifSceneReader	scene;
		scene.ReadScene(file);

		for (int i = 0, n = scene.GetNumMeshes(); i < n; i++) {
			auto	m = MakeModel(scene.GetMeshRef(i));
			return m;
		}
		return ISO_NULL;
	}
//	ISO_ptr<void>	ReadWithFilename(tag id, const filename &fn) override;
} yif;

