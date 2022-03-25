#include "base/defs.h"
#include "packed_types.h"

using namespace iso;

namespace unity {

struct Class {
	enum id {
		GameObject					= 1,
		Component					= 2,
		LevelGameManager			= 3,
		Transform					= 4,
		TimeManager					= 5,
		GlobalGameManager			= 6,
		Behaviour					= 8,
		GameManager					= 9,
		AudioManager				= 11,
		ParticleAnimator			= 12,
		InputManager				= 13,
		EllipsoidParticleEmitter	= 15,
		Pipeline					= 17,
		EditorExtension				= 18,
		Physics2DSettings			= 19,
		Camera						= 20,
		Material					= 21,
		MeshRenderer				= 23,
		Renderer					= 25,
		ParticleRenderer			= 26,
		Texture						= 27,
		Texture2D					= 28,
		SceneSettings				= 29,
		GraphicsSettings			= 30,
		MeshFilter					= 33,
		OcclusionPortal				= 41,
		Mesh						= 43,
		Skybox						= 45,
		QualitySettings				= 47,
		Shader						= 48,
		TextAsset					= 49,
		Rigidbody2D					= 50,
		Physics2DManager			= 51,
		Collider2D					= 53,
		Rigidbody					= 54,
		PhysicsManager				= 55,
		Collider					= 56,
		Joint						= 57,
		CircleCollider2D			= 58,
		HingeJoint					= 59,
		PolygonCollider2D			= 60,
		BoxCollider2D				= 61,
		PhysicsMaterial2D			= 62,
		MeshCollider				= 64,
		BoxCollider					= 65,
		SpriteCollider2D			= 66,
		EdgeCollider2D				= 68,
		ComputeShader				= 72,
		AnimationClip				= 74,
		ConstantForce				= 75,
		WorldParticleCollider		= 76,
		TagManager					= 78,
		AudioListener				= 81,
		AudioSource					= 82,
		AudioClip					= 83,
		RenderTexture				= 84,
		MeshParticleEmitter			= 87,
		ParticleEmitter				= 88,
		Cubemap						= 89,
		Avatar						= 90,
		AnimatorController			= 91,
		GUILayer					= 92,
		RuntimeAnimatorController	= 93,
		ScriptMapper				= 94,
		Animator					= 95,
		TrailRenderer				= 96,
		DelayedCallManager			= 98,
		TextMesh					= 102,
		RenderSettings				= 104,
		Light						= 108,
		CGProgram					= 109,
		BaseAnimationTrack			= 110,
		Animation					= 111,
		MonoBehaviour				= 114,
		MonoScript					= 115,
		MonoManager					= 116,
		Texture3D					= 117,
		NewAnimationTrack			= 118,
		Projector					= 119,
		LineRenderer				= 120,
		Flare						= 121,
		Halo						= 122,
		LensFlare					= 123,
		FlareLayer					= 124,
		HaloLayer					= 125,
		NavMeshAreas				= 126,
		HaloManager					= 127,
		Font						= 128,
		PlayerSettings				= 129,
		NamedObject					= 130,
		GUITexture					= 131,
		GUIText						= 132,
		GUIElement					= 133,
		PhysicMaterial				= 134,
		SphereCollider				= 135,
		CapsuleCollider				= 136,
		SkinnedMeshRenderer			= 137,
		FixedJoint					= 138,
		RaycastCollider				= 140,
		BuildSettings				= 141,
		AssetBundle					= 142,
		CharacterController			= 143,
		CharacterJoint				= 144,
		SpringJoint					= 145,
		WheelCollider				= 146,
		ResourceManager				= 147,
		NetworkView					= 148,
		NetworkManager				= 149,
		PreloadData					= 150,
		MovieTexture				= 152,
		ConfigurableJoint			= 153,
		TerrainCollider				= 154,
		MasterServerInterface		= 155,
		TerrainData					= 156,
		LightmapSettings			= 157,
		WebCamTexture				= 158,
		EditorSettings				= 159,
		InteractiveCloth			= 160,
		ClothRenderer				= 161,
		EditorUserSettings			= 162,
		SkinnedCloth				= 163,
		AudioReverbFilter			= 164,
		AudioHighPassFilter			= 165,
		AudioChorusFilter			= 166,
		AudioReverbZone				= 167,
		AudioEchoFilter				= 168,
		AudioLowPassFilter			= 169,
		AudioDistortionFilter		= 170,
		SparseTexture				= 171,
		AudioBehaviour				= 180,
		AudioFilter					= 181,
		WindZone					= 182,
		Cloth						= 183,
		SubstanceArchive			= 184,
		ProceduralMaterial			= 185,
		ProceduralTexture			= 186,
		OffMeshLink					= 191,
		OcclusionArea				= 192,
		Tree						= 193,
		NavMeshObsolete				= 194,
		NavMeshAgent				= 195,
		NavMeshSettings				= 196,
		LightProbesLegacy			= 197,
		ParticleSystem				= 198,
		ParticleSystemRenderer		= 199,
		ShaderVariantCollection		= 200,
		LODGroup					= 205,
		BlendTree					= 206,
		Motion						= 207,
		NavMeshObstacle				= 208,
		TerrainInstance				= 210,
		SpriteRenderer				= 212,
		Sprite						= 213,
		CachedSpriteAtlas			= 214,
		ReflectionProbe				= 215,
		ReflectionProbes			= 216,
		Terrain						= 218,
		LightProbeGroup				= 220,
		AnimatorOverrideController	= 221,
		CanvasRenderer				= 222,
		Canvas						= 223,
		RectTransform				= 224,
		CanvasGroup					= 225,
		BillboardAsset				= 226,
		BillboardRenderer			= 227,
		SpeedTreeWindAsset			= 228,
		AnchoredJoint2D				= 229,
		Joint2D						= 230,
		SpringJoint2D				= 231,
		DistanceJoint2D				= 232,
		HingeJoint2D				= 233,
		SliderJoint2D				= 234,
		WheelJoint2D				= 235,
		NavMeshData					= 238,
		AudioMixer					= 240,
		AudioMixerController		= 241,
		AudioMixerGroupController	= 243,
		AudioMixerEffectController	= 244,
		AudioMixerSnapshotController= 245,
		PhysicsUpdateBehaviour2D	= 246,
		ConstantForce2D				= 247,
		Effector2D					= 248,
		AreaEffector2D				= 249,
		PointEffector2D				= 250,
		PlatformEffector2D			= 251,
		SurfaceEffector2D			= 252,
		LightProbes					= 258,
		SampleClip					= 271,
		AudioMixerSnapshot			= 272,
		AudioMixerGroup				= 273,
		AssetBundleManifest			= 290,

		// Editor classIDs are above 100
		Prefab						= 1001,
		EditorExtensionImpl			= 1002,
		AssetImporter				= 1003,
		AssetDatabase				= 1004,
		Mesh3DSImporter				= 1005,
		TextureImporter				= 1006,
		ShaderImporter				= 1007,
		ComputeShaderImporter		= 1008,
		AvatarMask					= 1011,
		AudioImporter				= 1020,
		HierarchyState				= 1026,
		GUIDSerializer				= 1027,
		AssetMetaData				= 1028,
		DefaultAsset				= 1029,
		DefaultImporter				= 1030,
		TextScriptImporter			= 1031,
		SceneAsset					= 1032,
		NativeFormatImporter		= 1034,
		MonoImporter				= 1035,
		AssetServerCache			= 1037,
		LibraryAssetImporter		= 1038,
		ModelImporter				= 1040,
		FBXImporter					= 1041,
		TrueTypeFontImporter		= 1042,
		MovieImporter				= 1044,
		EditorBuildSettings			= 1045,
		DDSImporter					= 1046,
		InspectorExpandedState		= 1048,
		AnnotationManager			= 1049,
		PluginImporter				= 1050,
		EditorUserBuildSettings		= 1051,
		PVRImporter					= 1052,
		ASTCImporter				= 1053,
		KTXImporter					= 1054,
		AnimatorStateTransition		= 1101,
		AnimatorState				= 1102,
		HumanTemplate				= 1105,
		AnimatorStateMachine		= 1107,
		PreviewAssetType			= 1108,
		AnimatorTransition			= 1109,
		SpeedTreeImporter			= 1110,
		AnimatorTransitionBase		= 1111,
		SubstanceImporter			= 1112,
		LightmapParameters			= 1113,
		LightmapSnapshot			= 1120,

		//OutOfHierarchy
		OutOfHierarchy 				= 100000,
		Int							= OutOfHierarchy,
		Bool,
		Float,
		MonoObject,
		Collision,
		Vector3f,
		RootMotionData,
	};
};

struct GUID {
	uint32 data[4];
	GUID(uint32 a, uint32 b, uint32 c, uint32 d) { data[0] = a; data[1] = b; data[2] = c; data[3] = d; }
	GUID()  { data[0] = 0; data[1] = 0; data[2] = 0; data[3] = 0; }

	inline friend int compare(const GUID& a, const GUID& b) {
		for (int i = 0; i < 4; i++) {
			if (a.data[i] < b.data[i])
				return -1;
			if (a.data[i] > b.data[i])
				return 1;
		}
		return 0;
	}

	bool operator==(const GUID& b)	const { return compare(*this, b) == 0; }
	bool operator!=(const GUID& b)	const { return compare(*this, b) != 0; }
	bool operator<(const GUID& b)	const { return compare(*this, b) < 0; }
	bool IsValid()					const { return data[0] || data[1] || data[2] || data[3]; }
};

enum TextureTypes {
	kTexFormatAlpha8			= 1,
	kTexFormatARGB4444			= 2,
	kTexFormatRGB24				= 3,
	kTexFormatRGBA32			= 4,
	kTexFormatARGB32			= 5,
	kTexFormatARGBFloat			= 6,
	kTexFormatRGB565			= 7,
	kTexFormatBGR24				= 8,
	kTexFormatAlphaLum16		= 9,
	kTexFormatDXT1				= 10,
	kTexFormatDXT3				= 11,
	kTexFormatDXT5				= 12,
	kTexFormatRGBA4444			= 13,

	kTexReserved1				= 14,
	kTexReserved2				= 15,
	kTexReserved3				= 16,
	kTexReserved4				= 17,
	kTexReserved5				= 18,
	kTexReserved6				= 19,
	kTexReserved11				= 28,
	kTexReserved12				= 29,

	kTexFormatPVRTC_RGB2		= 30,
	kTexFormatPVRTC_RGBA2		= 31,
	kTexFormatPVRTC_RGB4		= 32,
	kTexFormatPVRTC_RGBA4		= 33,
	kTexFormatETC_RGB4			= 34,
	kTexFormatATC_RGB4			= 35,
	kTexFormatATC_RGBA8			= 36,
	kTexFormatBGRA32			= 37,

	kTexFormatFlashATF_RGB_DXT1	= 38,
	kTexFormatFlashATF_RGBA_JPG	= 39,
	kTexFormatFlashATF_RGB_JPG	= 40,
};

enum TextureFilter { Nearest, Bilinear, Trilinear };
enum TextureAddress { Repeat, Clamp };

typedef rgba8	ColorRGBA;
typedef float2p Vector2f;
typedef float3p Vector3f;
typedef float4p Vector4f, Quaternionf;

struct AABB			{ Vector3f m_Center, m_Extent; };

struct Rectf {
	float	x, y, w, h;
};
struct RectInt {
	int		x, y, w, h;
};
struct BitField {
	unsigned int m_Bits;
};

struct Matrix4x4f : float4x4p {};
struct Matrix3x4f : float4x3p {};

struct FastPropertyName {
	string name;
};

// unknown:

typedef anything Component;
typedef anything LightmapParameters;
typedef anything Font;
typedef anything MonoScript;
typedef anything AudioMixerGroup;
typedef anything AudioClip;
typedef anything Sprite;
typedef anything TextAsset;
typedef anything Texture;
typedef anything Shader;
typedef anything Renderer;
typedef anything OcclusionPortal;
typedef anything Cubemap;
typedef anything Flare;
typedef anything NavMeshData;
typedef anything RenderTexture;
typedef anything PhysicsMaterial2D;

struct Ref {
	int		m_FileID;
	int64	m_PathID;
};
template<typename T> struct PPtr : Ref {};
template<typename T> struct vector : ISO_openarray<T> {};
template<typename K, typename V> struct map : ISO_openarray<pair<K, V> > {};

// forward:

struct Object {
	unsigned int m_ObjectHideFlags;
};

struct MonoManager {
	unsigned int m_ObjectHideFlags;
};

struct Prefab;

struct Hash128 {
	unsigned char bytes[16];
};

struct GLTextureSettings {
	int m_FilterMode;
	int m_Aniso;
	float m_MipBias;
	int m_WrapMode;
};

struct StreamingInfo {
	unsigned int offset;
	unsigned int size;
	string path;
};

struct ChannelInfo {
	unsigned char stream;
	unsigned char offset;
	unsigned char format;
	unsigned char dimension;
};

struct BoneInfluence {
	float weight[4];
	int boneIndex[4];
};
struct VertexData {
	unsigned int m_CurrentChannels;
	unsigned int m_VertexCount;
	vector<ChannelInfo> m_Channels;
//	iso::pointer32<unsigned char> m_DataSize;
	vector<unsigned char> m_Data;

};

struct PackedBitVector {
	unsigned int m_NumItems;
	float m_Range;
	float m_Start;
	vector<unsigned char> m_Data;
	unsigned char m_BitSize;
};

struct CompressedMesh {
	PackedBitVector m_Vertices;
	PackedBitVector m_UV;
	PackedBitVector m_Normals;
	PackedBitVector m_Tangents;
	PackedBitVector m_Weights;
	PackedBitVector m_NormalSigns;
	PackedBitVector m_TangentSigns;
	PackedBitVector m_FloatColors;
	PackedBitVector m_BoneIndices;
	PackedBitVector m_Triangles;
	unsigned int m_UVInfo;
};

struct OcclusionBakeSettings {
	float smallestOccluder;
	float smallestHole;
	float backfaceThreshold;
};

struct SceneSettings : Object {
	vector<unsigned char> m_PVSData;
	vector<PPtr<Renderer> > m_PVSObjectsArray;
	vector<PPtr<OcclusionPortal> > m_PVSPortalsArray;
	OcclusionBakeSettings m_OcclusionBakeSettings;
};

struct EditorExtension {
	PPtr<EditorExtension> m_PrefabParentObject;
	PPtr<Prefab> m_PrefabInternal;
};

struct Texture2D : Object, EditorExtension {
	string m_Name;
	Hash128 m_ImageContentsHash;
	int m_Width;
	int m_Height;
	int m_CompleteImageSize;
	int m_TextureFormat;
	int m_MipCount;
	bool m_IsReadable;
	bool m_ReadAllowed;
	bool m_AlphaIsTransparency;
	int m_ImageCount;
	int m_TextureDimension;
	GLTextureSettings m_TextureSettings;
	int m_LightmapFormat;
	int m_ColorSpace;
	iso::pointer32<unsigned char> image_data;
	StreamingInfo m_StreamData;
};

struct GameObject : Object, EditorExtension {
	vector<pair<int, PPtr<Component> > > m_Component;
	unsigned int m_Layer;
	string m_Name;
	string m_TagString;
	PPtr<Texture2D> m_Icon;
	unsigned int m_NavMeshLayer;
	unsigned int m_StaticEditorFlags;
	bool m_IsActive;
};

struct UnityTexEnv {
	PPtr<Texture> m_Texture;
	Vector2f m_Scale;
	Vector2f m_Offset;
};

struct UnityPropertySheet {
	map<FastPropertyName, UnityTexEnv> m_TexEnvs;
	map<FastPropertyName, float> m_Floats;
	map<FastPropertyName, ColorRGBA> m_Colors;
};

struct MeshBlendShapeChannel {
	string name;
	unsigned int nameHash;
	int frameIndex;
	int frameCount;
};

struct BlendShapeVertex {
	Vector3f vertex;
	Vector3f normal;
	Vector3f tangent;
	unsigned int index;
};

struct MeshBlendShape {
	unsigned int firstVertex;
	unsigned int vertexCount;
	bool hasNormals;
	bool hasTangents;
};

struct BlendShapeData {
	vector<BlendShapeVertex> vertices;
	vector<MeshBlendShape> shapes;
	vector<MeshBlendShapeChannel> channels;
	vector<float> fullWeights;
};

struct SubMesh {
	unsigned int firstByte;
	unsigned int indexCount;
	int topology;
	unsigned int firstVertex;
	unsigned int vertexCount;
	AABB localAABB;
};

struct Mesh : Object, EditorExtension {
	string m_Name;
	vector<SubMesh> m_SubMeshes;
	BlendShapeData m_Shapes;
	vector<Matrix4x4f> m_BindPose;
	vector<unsigned int> m_BoneNameHashes;
	unsigned int m_RootBoneNameHash;
	unsigned char m_MeshCompression;
	bool m_IsReadable;
	bool m_KeepVertices;
	bool m_KeepIndices;
	vector<unsigned char> m_IndexBuffer;
	vector<BoneInfluence> m_Skin;
	VertexData m_VertexData;
	CompressedMesh m_CompressedMesh;
	AABB m_LocalAABB;
	int m_MeshUsageFlags;
	vector<unsigned char> m_BakedConvexCollisionMesh;
	vector<unsigned char> m_BakedTriangleCollisionMesh;
	bool m_MeshOptimized;
};

struct Material : Object, EditorExtension {
	string m_Name;
	PPtr<Shader> m_Shader;
	string m_ShaderKeywords;
	unsigned int m_LightmapFlags;
	int m_CustomRenderQueue;
	map<string,string> stringTagMap;
	UnityPropertySheet m_SavedProperties;
};
struct ShadowSettings {
	int m_Type;
	int m_Resolution;
	int m_CustomResolution;
	float m_Strength;
	float m_Bias;
	float m_NormalBias;
	float m_NearPlane;
};

struct Light : Object, EditorExtension {
	PPtr<GameObject> m_GameObject;
	unsigned char m_Enabled;
	int m_Type;
	ColorRGBA m_Color;
	float m_Intensity;
	float m_Range;
	float m_SpotAngle;
	float m_CookieSize;
	ShadowSettings m_Shadows;
	PPtr<Texture> m_Cookie;
	bool m_DrawHalo;
	PPtr<Flare> m_Flare;
	int m_RenderMode;
	BitField m_CullingMask;
	int m_Lightmapping;
	Vector2f m_AreaSize;
	float m_BounceIntensity;
	float m_ShadowRadius;
	float m_ShadowAngle;
};

struct RenderSettings : Object {
	bool m_Fog;
	ColorRGBA m_FogColor;
	int m_FogMode;
	float m_FogDensity;
	float m_LinearFogStart;
	float m_LinearFogEnd;
	ColorRGBA m_AmbientSkyColor;
	ColorRGBA m_AmbientEquatorColor;
	ColorRGBA m_AmbientGroundColor;
	float m_AmbientIntensity;
	int m_AmbientMode;
	PPtr<Material> m_SkyboxMaterial;
	float m_HaloStrength;
	float m_FlareStrength;
	float m_FlareFadeSpeed;
	PPtr<Texture2D> m_HaloTexture;
	PPtr<Texture2D> m_SpotCookie;
	int m_DefaultReflectionMode;
	int m_DefaultReflectionResolution;
	int m_ReflectionBounces;
	float m_ReflectionIntensity;
	PPtr<Cubemap> m_CustomReflection;
	PPtr<Light> m_Sun;
	ColorRGBA m_IndirectSpecularColor;
};

struct GISettings {
	float m_BounceScale;
	float m_IndirectOutputScale;
	float m_AlbedoBoost;
	float m_TemporalCoherenceThreshold;
	unsigned int m_EnvironmentLightingMode;
	bool m_EnableBakedLightmaps;
	bool m_EnableRealtimeLightmaps;
};

struct LightmapEditorSettings {
	float m_Resolution;
	float m_BakeResolution;
	int m_TextureWidth;
	int m_TextureHeight;
	bool m_AO;
	float m_AOMaxDistance;
	float m_CompAOExponent;
	float m_CompAOExponentDirect;
	int m_Padding;
	PPtr<LightmapParameters> m_LightmapParameters;
	int m_LightmapsBakeMode;
	bool m_TextureCompression;
	bool m_DirectLightInLightProbes;
	bool m_FinalGather;
	bool m_FinalGatherFiltering;
	int m_FinalGatherRayCount;
	int m_ReflectionCompression;
};

struct NavMeshBuildSettings {
	float agentRadius;
	float agentHeight;
	float agentSlope;
	float agentClimb;
	float ledgeDropHeight;
	float maxJumpAcrossDistance;
	bool accuratePlacement;
	float minRegionArea;
	float cellSize;
	bool manualCellSize;
};

struct NavMeshSettings : Object {
	NavMeshBuildSettings m_BuildSettings;
	PPtr<NavMeshData> m_NavMeshData;
};

struct Transform : Object, EditorExtension {
	PPtr<GameObject> m_GameObject;
	Quaternionf m_LocalRotation;
	Vector3f m_LocalPosition;
	Vector3f m_LocalScale;
	Vector3f m_LocalEulerAnglesHint;
	vector<PPtr<Transform> > m_Children;
	PPtr<Transform> m_Father;
	int m_RootOrder;
};

struct RectTransform : Object, EditorExtension {
	PPtr<GameObject> m_GameObject;
	Quaternionf m_LocalRotation;
	Vector3f m_LocalPosition;
	Vector3f m_LocalScale;
	Vector3f m_LocalEulerAnglesHint;
	vector<PPtr<Transform> > m_Children;
	PPtr<Transform> m_Father;
	int m_RootOrder;
	Vector2f m_AnchorMin;
	Vector2f m_AnchorMax;
	Vector2f m_AnchoredPosition;
	Vector2f m_SizeDelta;
	Vector2f m_Pivot;
};

struct PropertyModification {
	PPtr<Object> target;
	string propertyPath;
	string value;
	PPtr<Object> objectReference;
};

struct PrefabModification {
	PPtr<Transform> m_TransformParent;
	vector<PropertyModification> m_Modifications;
	vector<PPtr<Object> > m_RemovedComponents;
};

struct Prefab : Object {
	PrefabModification m_Modification;
	PPtr<Prefab> m_ParentPrefab;
	PPtr<GameObject> m_RootGameObject;
	bool m_IsPrefabParent;
};

struct MeshRenderer : Object, EditorExtension {
	PPtr<GameObject> m_GameObject;
	bool m_Enabled;
	unsigned char m_CastShadows;
	unsigned char m_ReceiveShadows;
	unsigned char m_MotionVectors;
	unsigned char m_LightProbeUsage;
	unsigned char m_ReflectionProbeUsage;
	vector<PPtr<Material> > m_Materials;
	vector<unsigned int> m_SubsetIndices;
	PPtr<Transform> m_StaticBatchRoot;
	PPtr<Transform> m_ProbeAnchor;
	PPtr<GameObject> m_LightProbeVolumeOverride;
	float m_ScaleInLightmap;
	bool m_PreserveUVs;
	bool m_IgnoreNormalsForChartDetection;
	bool m_ImportantGI;
	bool m_SelectedWireframeHidden;
	int m_MinimumChartSize;
	float m_AutoUVMaxDistance;
	float m_AutoUVMaxAngle;
	PPtr<LightmapParameters> m_LightmapParameters;
	int m_SortingLayerID;
	short m_SortingOrder;
};

struct SkinnedMeshRenderer : Object, EditorExtension {
	PPtr<GameObject> m_GameObject;
	bool m_Enabled;
	unsigned char m_CastShadows;
	unsigned char m_ReceiveShadows;
	unsigned char m_MotionVectors;
	unsigned char m_LightProbeUsage;
	unsigned char m_ReflectionProbeUsage;
	vector<PPtr<Material>> m_Materials;
	vector<unsigned int> m_SubsetIndices;
	PPtr<Transform> m_StaticBatchRoot;
	PPtr<Transform> m_ProbeAnchor;
	PPtr<GameObject> m_LightProbeVolumeOverride;
	float m_ScaleInLightmap;
	bool m_PreserveUVs;
	bool m_IgnoreNormalsForChartDetection;
	bool m_ImportantGI;
	bool m_SelectedWireframeHidden;
	int m_MinimumChartSize;
	float m_AutoUVMaxDistance;
	float m_AutoUVMaxAngle;
	PPtr<LightmapParameters> m_LightmapParameters;
	int m_SortingLayerID;
	short m_SortingOrder;
	int m_Quality;
	bool m_UpdateWhenOffscreen;
	bool m_SkinnedMotionVectors;
	PPtr<Mesh> m_Mesh;
	vector<PPtr<Transform>> m_Bones;
	vector<float> m_BlendShapeWeights;
	PPtr<Transform> m_RootBone;
	AABB m_AABB;
	bool m_DirtyAABB;
};

struct MeshFilter : Object, EditorExtension {
	PPtr<GameObject> m_GameObject;
	PPtr<Mesh> m_Mesh;
};

struct Rigidbody : Object, EditorExtension {
	PPtr<GameObject> m_GameObject;
	float m_Mass;
	float m_Drag;
	float m_AngularDrag;
	bool m_UseGravity;
	bool m_IsKinematic;
	unsigned char m_Interpolate;
	int m_Constraints;
	int m_CollisionDetection;
};

struct TextMesh : Object, EditorExtension {
	PPtr<GameObject> m_GameObject;
	string m_Text;
	float m_OffsetZ;
	float m_CharacterSize;
	float m_LineSpacing;
	short m_Anchor;
	short m_Alignment;
	float m_TabSize;
	int m_FontSize;
	int m_FontStyle;
	bool m_RichText;
	PPtr<Font> m_Font;
	ColorRGBA m_Color;
};

struct Behaviour : Object, EditorExtension {
	PPtr<GameObject> m_GameObject;
	unsigned char m_Enabled;
};

struct MonoBehaviour : Object, EditorExtension {
	PPtr<GameObject> m_GameObject;
	unsigned char m_Enabled;
	unsigned int m_EditorHideFlags;
	PPtr<MonoScript> m_Script;
	string m_Name;
	string m_EditorClassIdentifier;
};

struct Camera : Object, EditorExtension {
	PPtr<GameObject> m_GameObject;
	unsigned char m_Enabled;
	unsigned int m_ClearFlags;
	ColorRGBA m_BackGroundColor;
	Rectf m_NormalizedViewPortRect;
	float near_clip_plane;
	float far_clip_plane;
	float field_of_view;
	bool orthographic;
	float orthographic_size;
	float m_Depth;
	BitField m_CullingMask;
	int m_RenderingPath;
	PPtr<RenderTexture> m_TargetTexture;
	int m_TargetDisplay;
	int m_TargetEye;
	bool m_HDR;
	bool m_OcclusionCulling;
	float m_StereoConvergence;
	float m_StereoSeparation;
	bool m_StereoMirrorMode;
};

struct PhysicMaterial {
	unsigned int m_ObjectHideFlags;
	PPtr<EditorExtension> m_PrefabParentObject;
	PPtr<Prefab> m_PrefabInternal;
	string m_Name;
	float dynamicFriction;
	float staticFriction;
	float bounciness;
	int frictionCombine;
	int bounceCombine;
};

struct Collider : Object, EditorExtension {
	PPtr<GameObject> m_GameObject;
	PPtr<PhysicMaterial> m_Material;
	bool m_IsTrigger;
	bool m_Enabled;
};

struct SphereCollider : Collider {
	float m_Radius;
	Vector3f m_Center;
};

struct BoxCollider : Collider {
	Vector3f m_Size;
	Vector3f m_Center;
};

struct CapsuleCollider : Collider {
	float m_Radius;
	float m_Height;
	int m_Direction;
	Vector3f m_Center;
};
struct MeshCollider : Collider {
	bool m_Convex;
	PPtr<Mesh> m_Mesh;
};

struct GradientNEW {
	ColorRGBA key0;
	ColorRGBA key1;
	ColorRGBA key2;
	ColorRGBA key3;
	ColorRGBA key4;
	ColorRGBA key5;
	ColorRGBA key6;
	ColorRGBA key7;
	unsigned short ctime0;
	unsigned short ctime1;
	unsigned short ctime2;
	unsigned short ctime3;
	unsigned short ctime4;
	unsigned short ctime5;
	unsigned short ctime6;
	unsigned short ctime7;
	unsigned short atime0;
	unsigned short atime1;
	unsigned short atime2;
	unsigned short atime3;
	unsigned short atime4;
	unsigned short atime5;
	unsigned short atime6;
	unsigned short atime7;
	unsigned char m_NumColorKeys;
	unsigned char m_NumAlphaKeys;
};

struct MinMaxGradient {
	GradientNEW maxGradient;
	GradientNEW minGradient;
	ColorRGBA minColor;
	ColorRGBA maxColor;
	short minMaxState;
};

struct Keyframe {
	float time;
	float value;
	float inSlope;
	float outSlope;
	int tangentMode;
};

struct AnimationCurve {
	vector<Keyframe> m_Curve;
	int m_PreInfinity;
	int m_PostInfinity;
	int m_RotationOrder;
};

struct MinMaxCurve {
	float scalar;
	AnimationCurve maxCurve;
	AnimationCurve minCurve;
	short minMaxState;
};

struct InitialModule {
	bool enabled;
	MinMaxCurve startLifetime;
	MinMaxCurve startSpeed;
	MinMaxGradient startColor;
	MinMaxCurve startSize;
	MinMaxCurve startSizeY;
	MinMaxCurve startSizeZ;
	MinMaxCurve startRotationX;
	MinMaxCurve startRotationY;
	MinMaxCurve startRotation;
	float randomizeRotationDirection;
	float gravityModifier;
	int maxNumParticles;
	bool size3D;
	bool rotation3D;
};

struct InheritVelocityModule {
	bool enabled;
	int m_Mode;
	MinMaxCurve m_Curve;
};

struct ExternalForcesModule {
	bool enabled;
	float multiplier;
};

struct SizeBySpeedModule {
	bool enabled;
	MinMaxCurve curve;
	MinMaxCurve y;
	MinMaxCurve z;
	Vector2f range;
	bool separateAxes;
};

struct ClampVelocityModule {
	bool enabled;
	MinMaxCurve x;
	MinMaxCurve y;
	MinMaxCurve z;
	MinMaxCurve magnitude;
	bool separateAxis;
	bool inWorldSpace;
	float dampen;
};

struct RotationBySpeedModule {
	bool enabled;
	MinMaxCurve x;
	MinMaxCurve y;
	MinMaxCurve curve;
	bool separateAxes;
	Vector2f range;
};

struct ColorBySpeedModule {
	bool enabled;
	MinMaxGradient gradient;
	Vector2f range;
};

struct EmissionModule {
	bool enabled;
	int m_Type;
	MinMaxCurve rate;
	int cnt0;
	int cnt1;
	int cnt2;
	int cnt3;
	int cntmax0;
	int cntmax1;
	int cntmax2;
	int cntmax3;
	float time0;
	float time1;
	float time2;
	float time3;
	int m_BurstCount;
};

struct ShapeModule {
	bool enabled;
	int type;
	float radius;
	float angle;
	float length;
	float boxX;
	float boxY;
	float boxZ;
	float arc;
	int placementMode;
	PPtr<Mesh> m_Mesh;
	PPtr<MeshRenderer> m_MeshRenderer;
	PPtr<SkinnedMeshRenderer> m_SkinnedMeshRenderer;
	int m_MeshMaterialIndex;
	float m_MeshNormalOffset;
	bool m_UseMeshMaterialIndex;
	bool m_UseMeshColors;
	bool randomDirection;
};

struct SizeModule {
	bool enabled;
	MinMaxCurve curve;
	MinMaxCurve y;
	MinMaxCurve z;
	bool separateAxes;
};

struct UVModule {
	bool enabled;
	MinMaxCurve frameOverTime;
	MinMaxCurve startFrame;
	int tilesX;
	int tilesY;
	int animationType;
	int rowIndex;
	float cycles;
	int uvChannelMask;
	bool randomRow;
};

struct ColorModule {
	bool enabled;
	MinMaxGradient gradient;
};

struct VelocityModule {
	bool enabled;
	MinMaxCurve x;
	MinMaxCurve y;
	MinMaxCurve z;
	bool inWorldSpace;
};

struct ForceModule {
	bool enabled;
	MinMaxCurve x;
	MinMaxCurve y;
	MinMaxCurve z;
	bool inWorldSpace;
	bool randomizePerFrame;
};

struct RotationModule {
	bool enabled;
	MinMaxCurve x;
	MinMaxCurve y;
	MinMaxCurve curve;
	bool separateAxes;
};

struct TriggerModule {
	bool enabled;
	PPtr<Component> collisionShape0;
	PPtr<Component> collisionShape1;
	PPtr<Component> collisionShape2;
	PPtr<Component> collisionShape3;
	PPtr<Component> collisionShape4;
	PPtr<Component> collisionShape5;
	int inside;
	int outside;
	int enter;
	int exit;
	float radiusScale;
};

struct CollisionModule {
	bool enabled;
	int type;
	int collisionMode;
	PPtr<Transform> plane0;
	PPtr<Transform> plane1;
	PPtr<Transform> plane2;
	PPtr<Transform> plane3;
	PPtr<Transform> plane4;
	PPtr<Transform> plane5;
	MinMaxCurve m_Dampen;
	MinMaxCurve m_Bounce;
	MinMaxCurve m_EnergyLossOnCollision;
	float minKillSpeed;
	float maxKillSpeed;
	float radiusScale;
	BitField collidesWith;
	int maxCollisionShapes;
	int quality;
	float voxelSize;
	bool collisionMessages;
	bool collidesWithDynamic;
	bool interiorCollisions;
};

struct ParticleSystem;
struct SubModule {
	bool enabled;
	PPtr<ParticleSystem> subEmitterBirth;
	PPtr<ParticleSystem> subEmitterBirth1;
	PPtr<ParticleSystem> subEmitterCollision;
	PPtr<ParticleSystem> subEmitterCollision1;
	PPtr<ParticleSystem> subEmitterDeath;
	PPtr<ParticleSystem> subEmitterDeath1;
};

struct ParticleSystem : Object, EditorExtension {
	PPtr<GameObject> m_GameObject;
	float lengthInSec;
	MinMaxCurve startDelay;
	float speed;
	unsigned int randomSeed;
	bool looping;
	bool prewarm;
	bool playOnAwake;
	bool moveWithTransform;
	int scalingMode;
	InitialModule InitialModule;
	ShapeModule ShapeModule;
	EmissionModule EmissionModule;
	SizeModule SizeModule;
	RotationModule RotationModule;
	ColorModule ColorModule;
	UVModule UVModule;
	VelocityModule VelocityModule;
	InheritVelocityModule InheritVelocityModule;
	ForceModule ForceModule;
	ExternalForcesModule ExternalForcesModule;
	ClampVelocityModule ClampVelocityModule;
	SizeBySpeedModule SizeBySpeedModule;
	RotationBySpeedModule RotationBySpeedModule;
	ColorBySpeedModule ColorBySpeedModule;
	CollisionModule CollisionModule;
	TriggerModule TriggerModule;
	SubModule SubModule;
};

struct ParticleSystemRenderer : Object, EditorExtension {
	PPtr<GameObject> m_GameObject;
	bool m_Enabled;
	unsigned char m_CastShadows;
	unsigned char m_ReceiveShadows;
	unsigned char m_MotionVectors;
	unsigned char m_LightProbeUsage;
	unsigned char m_ReflectionProbeUsage;
	vector<PPtr<Material> > m_Materials;
	vector<unsigned int> m_SubsetIndices;
	PPtr<Transform> m_StaticBatchRoot;
	PPtr<Transform> m_ProbeAnchor;
	PPtr<GameObject> m_LightProbeVolumeOverride;
	float m_ScaleInLightmap;
	bool m_PreserveUVs;
	bool m_IgnoreNormalsForChartDetection;
	bool m_ImportantGI;
	bool m_SelectedWireframeHidden;
	int m_MinimumChartSize;
	float m_AutoUVMaxDistance;
	float m_AutoUVMaxAngle;
	PPtr<LightmapParameters> m_LightmapParameters;
	int m_SortingLayerID;
	short m_SortingOrder;
	unsigned short m_RenderMode;
	unsigned short m_SortMode;
	float m_MinParticleSize;
	float m_MaxParticleSize;
	float m_CameraVelocityScale;
	float m_VelocityScale;
	float m_LengthScale;
	float m_SortingFudge;
	float m_NormalDirection;
	int m_RenderAlignment;
	Vector3f m_Pivot;
	PPtr<Mesh> m_Mesh;
	PPtr<Mesh> m_Mesh1;
	PPtr<Mesh> m_Mesh2;
	PPtr<Mesh> m_Mesh3;
};

struct AudioSource : Object, EditorExtension {
	PPtr<GameObject> m_GameObject;
	unsigned char m_Enabled;
	PPtr<AudioMixerGroup> OutputAudioMixerGroup;
	PPtr<AudioClip> m_audioClip;
	bool m_PlayOnAwake;
	float m_Volume;
	float m_Pitch;
	bool Loop;
	bool Mute;
	bool Spatialize;
	int Priority;
	float DopplerLevel;
	float MinDistance;
	float MaxDistance;
	float Pan2D;
	int rolloffMode;
	bool BypassEffects;
	bool BypassListenerEffects;
	bool BypassReverbZones;
	AnimationCurve rolloffCustomCurve;
	AnimationCurve panLevelCustomCurve;
	AnimationCurve spreadCustomCurve;
	AnimationCurve reverbZoneMixCustomCurve;
};

struct AudioListener : Object, EditorExtension {
	PPtr<GameObject> m_GameObject;
	unsigned char m_Enabled;
};

struct Halo : Object, EditorExtension {
	PPtr<GameObject> m_GameObject;
	unsigned char m_Enabled;
	ColorRGBA m_Color;
	float m_Size;
};

struct LensFlare : Object, EditorExtension {
	PPtr<GameObject> m_GameObject;
	unsigned char m_Enabled;
	PPtr<Flare> m_Flare;
	ColorRGBA m_Color;
	float m_Brightness;
	float m_FadeSpeed;
	BitField m_IgnoreLayers;
	bool m_Directional;
};

struct DetailPatch {
	AABB bounds;
	vector<unsigned char> layerIndices;
	vector<unsigned char> numberOfObjects;
};

struct SplatPrototype {
	PPtr<Texture2D> texture;
	PPtr<Texture2D> normalMap;
	Vector2f tileSize;
	Vector2f tileOffset;
	Vector4f specularMetallic;
	float smoothness;
};

struct DetailPrototype {
	PPtr<GameObject> prototype;
	PPtr<Texture2D> prototypeTexture;
	float minWidth;
	float maxWidth;
	float minHeight;
	float maxHeight;
	float noiseSpread;
	float bendFactor;
	ColorRGBA healthyColor;
	ColorRGBA dryColor;
	float lightmapFactor;
	int renderMode;
	int usePrototypeMesh;
};

struct TreeInstance {
	Vector3f position;
	float widthScale;
	float heightScale;
	float rotation;
	ColorRGBA color;
	ColorRGBA lightmapColor;
	int index;
};

struct TreePrototype {
	PPtr<GameObject> prefab;
	float bendFactor;
};

struct DetailDatabase {
	vector<DetailPatch> m_Patches;
	vector<DetailPrototype> m_DetailPrototypes;
	int m_PatchCount;
	int m_PatchSamples;
	vector<Vector3f> m_RandomRotations;
	ColorRGBA WavingGrassTint;
	float m_WavingGrassStrength;
	float m_WavingGrassAmount;
	float m_WavingGrassSpeed;
	vector<TreeInstance> m_TreeInstances;
	vector<TreePrototype> m_TreePrototypes;
	vector<PPtr<Texture2D> > m_PreloadTextureAtlasData;
};

struct SplatDatabase {
	vector<SplatPrototype> m_Splats;
	vector<PPtr<Texture2D> > m_AlphaTextures;
	int m_AlphamapResolution;
	int m_BaseMapResolution;
	int m_ColorSpace;
	bool m_MaterialRequiresMetallic;
	bool m_MaterialRequiresSmoothness;
};

struct Heightmap {
	vector<short> m_Heights;
	vector<float> m_PrecomputedError;
	vector<float> m_MinMaxPatchHeights;
	int m_Width;
	int m_Height;
	float m_Thickness;
	int m_Levels;
	Vector3f m_Scale;
};

struct TerrainData : Object, EditorExtension {
	string m_Name;
	SplatDatabase m_SplatDatabase;
	DetailDatabase m_DetailDatabase;
	Heightmap m_Heightmap;
};

struct Terrain : Object, EditorExtension {
	PPtr<GameObject> m_GameObject;
	unsigned char m_Enabled;
	PPtr<TerrainData> m_TerrainData;
	float m_TreeDistance;
	float m_TreeBillboardDistance;
	float m_TreeCrossFadeLength;
	int m_TreeMaximumFullLODCount;
	float m_DetailObjectDistance;
	float m_DetailObjectDensity;
	float m_HeightmapPixelError;
	float m_SplatMapDistance;
	int m_HeightmapMaximumLOD;
	bool m_CastShadows;
	bool m_DrawHeightmap;
	bool m_DrawTreesAndFoliage;
	int m_ReflectionProbeUsage;
	int m_MaterialType;
	ColorRGBA m_LegacySpecular;
	float m_LegacyShininess;
	PPtr<Material> m_MaterialTemplate;
	bool m_BakeLightProbesForTrees;
	float m_ScaleInLightmap;
	PPtr<LightmapParameters> m_LightmapParameters;
};

struct TerrainCollider : Object, EditorExtension {
	PPtr<GameObject> m_GameObject;
	PPtr<PhysicMaterial> m_Material;
	bool m_Enabled;
	PPtr<TerrainData> m_TerrainData;
	bool m_EnableTreeColliders;
};

struct SpriteRenderer : Object, EditorExtension {
	PPtr<GameObject> m_GameObject;
	bool m_Enabled;
	unsigned char m_CastShadows;
	unsigned char m_ReceiveShadows;
	unsigned char m_MotionVectors;
	unsigned char m_LightProbeUsage;
	unsigned char m_ReflectionProbeUsage;
	vector<PPtr<Material>> m_Materials;
	vector<unsigned int> m_SubsetIndices;
	PPtr<Transform> m_StaticBatchRoot;
	PPtr<Transform> m_ProbeAnchor;
	PPtr<GameObject> m_LightProbeVolumeOverride;
	float m_ScaleInLightmap;
	bool m_PreserveUVs;
	bool m_IgnoreNormalsForChartDetection;
	bool m_ImportantGI;
	bool m_SelectedWireframeHidden;
	int m_MinimumChartSize;
	float m_AutoUVMaxDistance;
	float m_AutoUVMaxAngle;
	PPtr<LightmapParameters> m_LightmapParameters;
	int m_SortingLayerID;
	short m_SortingOrder;
	PPtr<Sprite> m_Sprite;
	ColorRGBA m_Color;
	bool m_FlipX;
	bool m_FlipY;
};

struct Canvas : Object, EditorExtension {
	PPtr<GameObject> m_GameObject;
	unsigned char m_Enabled;
	int m_RenderMode;
	PPtr<Camera> m_Camera;
	float m_PlaneDistance;
	bool m_PixelPerfect;
	bool m_ReceivesEvents;
	bool m_OverrideSorting;
	bool m_OverridePixelPerfect;
	float m_SortingBucketNormalizedSize;
	int m_SortingLayerID;
	short m_SortingOrder;
	char m_TargetDisplay;
};
struct CanvasRenderer : Object, EditorExtension {
	PPtr<GameObject> m_GameObject;
};

struct MeshParticleEmitter : Object, EditorExtension {
	PPtr<GameObject> m_GameObject;
	bool m_Enabled;
	bool m_Emit;
	float minSize;
	float maxSize;
	float minEnergy;
	float maxEnergy;
	float minEmission;
	float maxEmission;
	Vector3f worldVelocity;
	Vector3f localVelocity;
	Vector3f rndVelocity;
	float emitterVelocityScale;
	Vector3f tangentVelocity;
	float angularVelocity;
	float rndAngularVelocity;
	bool rndRotation;
	bool Simulate_in_Worldspace;
	bool m_OneShot;
	bool m_InterpolateTriangles;
	bool m_Systematic;
	float m_MinNormalVelocity;
	float m_MaxNormalVelocity;
	PPtr<Mesh> m_Mesh;
};

struct Projector : Object, EditorExtension {
	PPtr<GameObject> m_GameObject;
	unsigned char m_Enabled;
	float m_NearClipPlane;
	float m_FarClipPlane;
	float m_FieldOfView;
	float m_AspectRatio;
	bool m_Orthographic;
	float m_OrthographicSize;
	PPtr<Material> m_Material;
	BitField m_IgnoreLayers;
};

struct LineParameters {
	float startWidth;
	float endWidth;
	ColorRGBA m_StartColor;
	ColorRGBA m_EndColor;
};

struct LineRenderer : Object, EditorExtension {
	PPtr<GameObject> m_GameObject;
	bool m_Enabled;
	unsigned char m_CastShadows;
	unsigned char m_ReceiveShadows;
	unsigned char m_MotionVectors;
	unsigned char m_LightProbeUsage;
	unsigned char m_ReflectionProbeUsage;
	vector<PPtr<Material>> m_Materials;
	vector<unsigned int> m_SubsetIndices;
	PPtr<Transform> m_StaticBatchRoot;
	PPtr<Transform> m_ProbeAnchor;
	PPtr<GameObject> m_LightProbeVolumeOverride;
	float m_ScaleInLightmap;
	bool m_PreserveUVs;
	bool m_IgnoreNormalsForChartDetection;
	bool m_ImportantGI;
	bool m_SelectedWireframeHidden;
	int m_MinimumChartSize;
	float m_AutoUVMaxDistance;
	float m_AutoUVMaxAngle;
	PPtr<LightmapParameters> m_LightmapParameters;
	int m_SortingLayerID;
	short m_SortingOrder;
	vector<Vector3f> m_Positions;
	LineParameters m_Parameters;
	bool m_UseWorldSpace;
};

struct Gradient {
	ColorRGBA m_Color[5];
};

struct TrailRenderer : Object, EditorExtension {
	PPtr<GameObject> m_GameObject;
	bool m_Enabled;
	unsigned char m_CastShadows;
	unsigned char m_ReceiveShadows;
	unsigned char m_MotionVectors;
	unsigned char m_LightProbeUsage;
	unsigned char m_ReflectionProbeUsage;
	vector<PPtr<Material>> m_Materials;
	vector<unsigned int> m_SubsetIndices;
	PPtr<Transform> m_StaticBatchRoot;
	PPtr<Transform> m_ProbeAnchor;
	PPtr<GameObject> m_LightProbeVolumeOverride;
	float m_ScaleInLightmap;
	bool m_PreserveUVs;
	bool m_IgnoreNormalsForChartDetection;
	bool m_ImportantGI;
	bool m_SelectedWireframeHidden;
	int m_MinimumChartSize;
	float m_AutoUVMaxDistance;
	float m_AutoUVMaxAngle;
	PPtr<LightmapParameters> m_LightmapParameters;
	int m_SortingLayerID;
	short m_SortingOrder;
	float m_Time;
	float m_StartWidth;
	float m_EndWidth;
	Gradient m_Colors;
	float m_MinVertexDistance;
	bool m_Autodestruct;
};

struct UVAnimation {
	int x_Tile;
	int y_Tile;
	float cycles;
};

struct ParticleRenderer : Object, EditorExtension {
	PPtr<GameObject> m_GameObject;
	bool m_Enabled;
	unsigned char m_CastShadows;
	unsigned char m_ReceiveShadows;
	unsigned char m_MotionVectors;
	unsigned char m_LightProbeUsage;
	unsigned char m_ReflectionProbeUsage;
	vector<PPtr<Material>> m_Materials;
	vector<unsigned int> m_SubsetIndices;
	PPtr<Transform> m_StaticBatchRoot;
	PPtr<Transform> m_ProbeAnchor;
	PPtr<GameObject> m_LightProbeVolumeOverride;
	float m_ScaleInLightmap;
	bool m_PreserveUVs;
	bool m_IgnoreNormalsForChartDetection;
	bool m_ImportantGI;
	bool m_SelectedWireframeHidden;
	int m_MinimumChartSize;
	float m_AutoUVMaxDistance;
	float m_AutoUVMaxAngle;
	PPtr<LightmapParameters> m_LightmapParameters;
	int m_SortingLayerID;
	short m_SortingOrder;
	float m_CameraVelocityScale;
	int m_StretchParticles;
	float m_LengthScale;
	float m_VelocityScale;
	float m_MaxParticleSize;
	UVAnimation UV_Animation;
};

struct WorldParticleCollider : Object, EditorExtension {
	PPtr<GameObject> m_GameObject;
	float m_BounceFactor;
	float m_CollisionEnergyLoss;
	BitField m_CollidesWith;
	bool m_SendCollisionMessage;
	float m_MinKillVelocity;
};

struct ParticleAnimator : Object, EditorExtension {
	PPtr<GameObject> m_GameObject;
	bool Does_Animate_Color;
	ColorRGBA colorAnimation[5];
	Vector3f worldRotationAxis;
	Vector3f localRotationAxis;
	float sizeGrow;
	Vector3f rndForce;
	Vector3f force;
	float damping;
	bool stopSimulation;
	bool autodestruct;
};

struct EllipsoidParticleEmitter : Object, EditorExtension {
	PPtr<GameObject> m_GameObject;
	bool m_Enabled;
	bool m_Emit;
	float minSize;
	float maxSize;
	float minEnergy;
	float maxEnergy;
	float minEmission;
	float maxEmission;
	Vector3f worldVelocity;
	Vector3f localVelocity;
	Vector3f rndVelocity;
	float emitterVelocityScale;
	Vector3f tangentVelocity;
	float angularVelocity;
	float rndAngularVelocity;
	bool rndRotation;
	bool Simulate_in_Worldspace;
	bool m_OneShot;
	Vector3f m_Ellipsoid;
	float m_MinEmitterRange;
};

struct JointSpring {
	float spring;
	float damper;
	float targetPosition;
};

struct JointLimits {
	float min;
	float max;
	float bounciness;
	float bounceMinVelocity;
	float contactDistance;
};

struct JointMotor {
	float targetVelocity;
	float force;
	int freeSpin;
};

struct SoftJointLimit {
	float limit;
	float bounciness;
	float contactDistance;
};

struct JointDrive {
	float positionSpring;
	float positionDamper;
	float maximumForce;
};

struct ConstantForce : Object, EditorExtension {
	PPtr<GameObject> m_GameObject;
	unsigned char m_Enabled;
	Vector3f m_Force;
	Vector3f m_RelativeForce;
	Vector3f m_Torque;
	Vector3f m_RelativeTorque;
};
struct HingeJoint : Object, EditorExtension {
	PPtr<GameObject> m_GameObject;
	PPtr<Rigidbody> m_ConnectedBody;
	Vector3f m_Anchor;
	Vector3f m_Axis;
	bool m_AutoConfigureConnectedAnchor;
	Vector3f m_ConnectedAnchor;
	bool m_UseSpring;
	JointSpring m_Spring;
	bool m_UseMotor;
	JointMotor m_Motor;
	bool m_UseLimits;
	JointLimits m_Limits;
	float m_BreakForce;
	float m_BreakTorque;
	bool m_EnableCollision;
	bool m_EnablePreprocessing;
};
struct ClothConstrainCoefficients {
	float maxDistance;
	float collisionSphereDistance;
};
struct WheelFrictionCurve {
	float m_ExtremumSlip;
	float m_ExtremumValue;
	float m_AsymptoteSlip;
	float m_AsymptoteValue;
	float m_Stiffness;
};
struct SoftJointLimitSpring {
	float spring;
	float damper;
};
struct ConfigurableJoint : Object, EditorExtension {
	PPtr<GameObject> m_GameObject;
	PPtr<Rigidbody> m_ConnectedBody;
	Vector3f m_Anchor;
	Vector3f m_Axis;
	bool m_AutoConfigureConnectedAnchor;
	Vector3f m_ConnectedAnchor;
	Vector3f m_SecondaryAxis;
	int m_XMotion;
	int m_YMotion;
	int m_ZMotion;
	int m_AngularXMotion;
	int m_AngularYMotion;
	int m_AngularZMotion;
	SoftJointLimitSpring m_LinearLimitSpring;
	SoftJointLimit m_LinearLimit;
	SoftJointLimitSpring m_AngularXLimitSpring;
	SoftJointLimit m_LowAngularXLimit;
	SoftJointLimit m_HighAngularXLimit;
	SoftJointLimitSpring m_AngularYZLimitSpring;
	SoftJointLimit m_AngularYLimit;
	SoftJointLimit m_AngularZLimit;
	Vector3f m_TargetPosition;
	Vector3f m_TargetVelocity;
	JointDrive m_XDrive;
	JointDrive m_YDrive;
	JointDrive m_ZDrive;
	Quaternionf m_TargetRotation;
	Vector3f m_TargetAngularVelocity;
	int m_RotationDriveMode;
	JointDrive m_AngularXDrive;
	JointDrive m_AngularYZDrive;
	JointDrive m_SlerpDrive;
	int m_ProjectionMode;
	float m_ProjectionDistance;
	float m_ProjectionAngle;
	bool m_ConfiguredInWorldSpace;
	bool m_SwapBodies;
	float m_BreakForce;
	float m_BreakTorque;
	bool m_EnableCollision;
	bool m_EnablePreprocessing;
};

struct CharacterController : Object, EditorExtension {
	PPtr<GameObject> m_GameObject;
	PPtr<PhysicMaterial> m_Material;
	bool m_IsTrigger;
	bool m_Enabled;
	float m_Height;
	float m_Radius;
	float m_SlopeLimit;
	float m_StepOffset;
	float m_SkinWidth;
	float m_MinMoveDistance;
	Vector3f m_Center;
};

struct CharacterJoint : Object, EditorExtension {
	PPtr<GameObject> m_GameObject;
	PPtr<Rigidbody> m_ConnectedBody;
	Vector3f m_Anchor;
	Vector3f m_Axis;
	bool m_AutoConfigureConnectedAnchor;
	Vector3f m_ConnectedAnchor;
	Vector3f m_SwingAxis;
	SoftJointLimitSpring m_TwistLimitSpring;
	SoftJointLimit m_LowTwistLimit;
	SoftJointLimit m_HighTwistLimit;
	SoftJointLimitSpring m_SwingLimitSpring;
	SoftJointLimit m_Swing1Limit;
	SoftJointLimit m_Swing2Limit;
	bool m_EnableProjection;
	float m_ProjectionDistance;
	float m_ProjectionAngle;
	float m_BreakForce;
	float m_BreakTorque;
	bool m_EnableCollision;
	bool m_EnablePreprocessing;
};

struct SpringJoint : Object, EditorExtension {
	PPtr<GameObject> m_GameObject;
	PPtr<Rigidbody> m_ConnectedBody;
	Vector3f m_Anchor;
	bool m_AutoConfigureConnectedAnchor;
	Vector3f m_ConnectedAnchor;
	float m_Spring;
	float m_Damper;
	float m_MinDistance;
	float m_MaxDistance;
	float m_Tolerance;
	float m_BreakForce;
	float m_BreakTorque;
	bool m_EnableCollision;
	bool m_EnablePreprocessing;
};

struct WheelCollider : Object, EditorExtension {
	PPtr<GameObject> m_GameObject;
	Vector3f m_Center;
	float m_Radius;
	JointSpring m_SuspensionSpring;
	float m_SuspensionDistance;
	float m_ForceAppPointDistance;
	float m_Mass;
	float m_WheelDampingRate;
	WheelFrictionCurve m_ForwardFriction;
	WheelFrictionCurve m_SidewaysFriction;
	bool m_Enabled;
};

struct FixedJoint : Object, EditorExtension {
	PPtr<GameObject> m_GameObject;
	PPtr<Rigidbody> m_ConnectedBody;
	float m_BreakForce;
	float m_BreakTorque;
	bool m_EnableCollision;
	bool m_EnablePreprocessing;
};

struct Cloth : Object, EditorExtension {
	PPtr<GameObject> m_GameObject;
	unsigned char m_Enabled;
	float m_StretchingStiffness;
	float m_BendingStiffness;
	bool m_UseTethers;
	bool m_UseGravity;
	float m_Damping;
	Vector3f m_ExternalAcceleration;
	Vector3f m_RandomAcceleration;
	float m_WorldVelocityScale;
	float m_WorldAccelerationScale;
	float m_Friction;
	float m_CollisionMassScale;
	bool m_UseContinuousCollision;
	bool m_UseVirtualParticles;
	float m_SolverFrequency;
	float m_SleepThreshold;
	vector<ClothConstrainCoefficients> m_Coefficients;
	vector<PPtr<CapsuleCollider>> m_CapsuleColliders;
	vector<pair<PPtr<SphereCollider>, PPtr<SphereCollider>>> m_SphereColliders;
};

struct Rigidbody2D : Object, EditorExtension {
	PPtr<GameObject> m_GameObject;
	bool m_UseAutoMass;
	float m_Mass;
	float m_LinearDrag;
	float m_AngularDrag;
	float m_GravityScale;
	bool m_IsKinematic;
	unsigned char m_Interpolate;
	unsigned char m_SleepingMode;
	unsigned char m_CollisionDetection;
	int m_Constraints;
};

struct PointEffector2D : Object, EditorExtension {
	PPtr<GameObject> m_GameObject;
	unsigned char m_Enabled;
	bool m_UseColliderMask;
	BitField m_ColliderMask;
	float m_ForceMagnitude;
	float m_ForceVariation;
	float m_DistanceScale;
	float m_Drag;
	float m_AngularDrag;
	unsigned char m_ForceSource;
	unsigned char m_ForceTarget;
	unsigned char m_ForceMode;
};
struct SpringJoint2D : Object, EditorExtension {
	PPtr<GameObject> m_GameObject;
	unsigned char m_Enabled;
	bool m_EnableCollision;
	PPtr<Rigidbody2D> m_ConnectedRigidBody;
	float m_BreakForce;
	float m_BreakTorque;
	bool m_AutoConfigureConnectedAnchor;
	Vector2f m_Anchor;
	Vector2f m_ConnectedAnchor;
	bool m_AutoConfigureDistance;
	float m_Distance;
	float m_DampingRatio;
	float m_Frequency;
};

struct JointMotor2D {
	float m_MotorSpeed;
	float m_MaximumMotorForce;
};

struct JointAngleLimits2D {
	float m_LowerAngle;
	float m_UpperAngle;
};

struct HingeJoint2D : Object, EditorExtension {
	PPtr<GameObject> m_GameObject;
	unsigned char m_Enabled;
	bool m_EnableCollision;
	PPtr<Rigidbody2D> m_ConnectedRigidBody;
	float m_BreakForce;
	float m_BreakTorque;
	bool m_AutoConfigureConnectedAnchor;
	Vector2f m_Anchor;
	Vector2f m_ConnectedAnchor;
	bool m_UseMotor;
	JointMotor2D m_Motor;
	bool m_UseLimits;
	JointAngleLimits2D m_AngleLimits;
};

struct DistanceJoint2D : Object, EditorExtension {
	PPtr<GameObject> m_GameObject;
	unsigned char m_Enabled;
	bool m_EnableCollision;
	PPtr<Rigidbody2D> m_ConnectedRigidBody;
	float m_BreakForce;
	float m_BreakTorque;
	bool m_AutoConfigureConnectedAnchor;
	Vector2f m_Anchor;
	Vector2f m_ConnectedAnchor;
	bool m_AutoConfigureDistance;
	float m_Distance;
	bool m_MaxDistanceOnly;
};

struct ConstantForce2D : Object, EditorExtension {
	PPtr<GameObject> m_GameObject;
	unsigned char m_Enabled;
	Vector2f m_Force;
	Vector2f m_RelativeForce;
	float m_Torque;
};

struct AreaEffector2D : Object, EditorExtension {
	PPtr<GameObject> m_GameObject;
	unsigned char m_Enabled;
	bool m_UseColliderMask;
	BitField m_ColliderMask;
	bool m_UseGlobalAngle;
	float m_ForceAngle;
	float m_ForceMagnitude;
	float m_ForceVariation;
	float m_Drag;
	float m_AngularDrag;
	unsigned char m_ForceTarget;
};

struct JointSuspension2D {
	float m_DampingRatio;
	float m_Frequency;
	float m_Angle;
};

struct WheelJoint2D : Object, EditorExtension {
	PPtr<GameObject> m_GameObject;
	unsigned char m_Enabled;
	bool m_EnableCollision;
	PPtr<Rigidbody2D> m_ConnectedRigidBody;
	float m_BreakForce;
	float m_BreakTorque;
	bool m_AutoConfigureConnectedAnchor;
	Vector2f m_Anchor;
	Vector2f m_ConnectedAnchor;
	JointSuspension2D m_Suspension;
	bool m_UseMotor;
	JointMotor2D m_Motor;
};

struct JointTranslationLimits2D {
	float m_LowerTranslation;
	float m_UpperTranslation;
};

struct SliderJoint2D : Object, EditorExtension {
	PPtr<GameObject> m_GameObject;
	unsigned char m_Enabled;
	bool m_EnableCollision;
	PPtr<Rigidbody2D> m_ConnectedRigidBody;
	float m_BreakForce;
	float m_BreakTorque;
	bool m_AutoConfigureConnectedAnchor;
	Vector2f m_Anchor;
	Vector2f m_ConnectedAnchor;
	bool m_AutoConfigureAngle;
	float m_Angle;
	bool m_UseMotor;
	JointMotor2D m_Motor;
	bool m_UseLimits;
	JointTranslationLimits2D m_TranslationLimits;
};

struct EdgeCollider2D : Object, EditorExtension {
	PPtr<GameObject> m_GameObject;
	unsigned char m_Enabled;
	float m_Density;
	PPtr<PhysicsMaterial2D> m_Material;
	bool m_IsTrigger;
	bool m_UsedByEffector;
	Vector2f m_Offset;
	vector<Vector2f> m_Points;
};

struct BoxCollider2D : Object, EditorExtension {
	PPtr<GameObject> m_GameObject;
	unsigned char m_Enabled;
	float m_Density;
	PPtr<PhysicsMaterial2D> m_Material;
	bool m_IsTrigger;
	bool m_UsedByEffector;
	Vector2f m_Offset;
	Vector2f m_Size;
};

struct BuoyancyEffector2D : Object, EditorExtension {
	PPtr<GameObject> m_GameObject;
	unsigned char m_Enabled;
	bool m_UseColliderMask;
	BitField m_ColliderMask;
	float m_SurfaceLevel;
	float m_Density;
	float m_LinearDrag;
	float m_AngularDrag;
	float m_FlowAngle;
	float m_FlowMagnitude;
	float m_FlowVariation;
};

struct PlatformEffector2D : Object, EditorExtension {
	PPtr<GameObject> m_GameObject;
	unsigned char m_Enabled;
	bool m_UseColliderMask;
	BitField m_ColliderMask;
	bool m_UseOneWay;
	bool m_UseOneWayGrouping;
	float m_SurfaceArc;
	bool m_UseSideFriction;
	bool m_UseSideBounce;
	float m_SideArc;
};

struct Polygon2D {
	vector<vector<Vector2f>> m_Paths;
};

struct PolygonCollider2D : Object, EditorExtension {
	PPtr<GameObject> m_GameObject;
	unsigned char m_Enabled;
	float m_Density;
	PPtr<PhysicsMaterial2D> m_Material;
	bool m_IsTrigger;
	bool m_UsedByEffector;
	Vector2f m_Offset;
	Polygon2D m_Points;
};

struct CircleCollider2D : Object, EditorExtension {
	PPtr<GameObject> m_GameObject;
	unsigned char m_Enabled;
	float m_Density;
	PPtr<PhysicsMaterial2D> m_Material;
	bool m_IsTrigger;
	bool m_UsedByEffector;
	Vector2f m_Offset;
	float m_Radius;
};

struct SurfaceEffector2D : Object, EditorExtension {
	PPtr<GameObject> m_GameObject;
	unsigned char m_Enabled;
	bool m_UseColliderMask;
	BitField m_ColliderMask;
	float m_Speed;
	float m_SpeedVariation;
	float m_ForceScale;
	bool m_UseContactForce;
	bool m_UseFriction;
	bool m_UseBounce;
};

struct FrictionJoint2D : Object, EditorExtension {
	PPtr<GameObject> m_GameObject;
	unsigned char m_Enabled;
	bool m_EnableCollision;
	PPtr<Rigidbody2D> m_ConnectedRigidBody;
	float m_BreakForce;
	float m_BreakTorque;
	bool m_AutoConfigureConnectedAnchor;
	Vector2f m_Anchor;
	Vector2f m_ConnectedAnchor;
	float m_MaxForce;
	float m_MaxTorque;
};

struct FixedJoint2D : Object, EditorExtension {
	PPtr<GameObject> m_GameObject;
	unsigned char m_Enabled;
	bool m_EnableCollision;
	PPtr<Rigidbody2D> m_ConnectedRigidBody;
	float m_BreakForce;
	float m_BreakTorque;
	bool m_AutoConfigureConnectedAnchor;
	Vector2f m_Anchor;
	Vector2f m_ConnectedAnchor;
	float m_DampingRatio;
	float m_Frequency;
};

struct TargetJoint2D : Object, EditorExtension {
	PPtr<GameObject> m_GameObject;
	unsigned char m_Enabled;
	bool m_EnableCollision;
	PPtr<Rigidbody2D> m_ConnectedRigidBody;
	float m_BreakForce;
	float m_BreakTorque;
	Vector2f m_Anchor;
	Vector2f m_Target;
	bool m_AutoConfigureTarget;
	float m_MaxForce;
	float m_DampingRatio;
	float m_Frequency;
};

struct RelativeJoint2D : Object, EditorExtension {
	PPtr<GameObject> m_GameObject;
	unsigned char m_Enabled;
	bool m_EnableCollision;
	PPtr<Rigidbody2D> m_ConnectedRigidBody;
	float m_BreakForce;
	float m_BreakTorque;
	float m_MaxForce;
	float m_MaxTorque;
	float m_CorrectionScale;
	bool m_AutoConfigureOffset;
	Vector2f m_LinearOffset;
	float m_AngularOffset;
};

struct AssetTimeStamp {
	unsigned int modificationDate[2];
	unsigned int metaModificationDate[2];
};

struct Image {
	int m_Format;
	int m_Width;
	int m_Height;
	int m_RowBytes;
	iso::pointer32<unsigned char> image_data;
};

struct LibraryRepresentation {
	string name;
	Image thumbnail;
	GUID guid;
	string path;
	int64 localIdentifier;
	short thumbnailClassID;
	unsigned short flags;
	string scriptClassName;
};

struct AssetLabels {
	vector<string> m_Labels;
};

struct Asset {
	LibraryRepresentation mainRepresentation;
	vector<LibraryRepresentation> representations;
	vector<GUID> children;
	GUID parent;
	int type;
	AssetLabels labels;
	int importerClassId;
	unsigned int importerVersionHash;
	Hash128 hash;
	int assetBundleIndex;
};

struct AssetBundleFullName {
	string m_AssetBundleName;
	string m_AssetBundleVariant;
};

struct AssetDatabaseMetrics {
	int totalAssetCount;
	int nonProAssetCount;
	int nonProAssetsCreatedAfterProLicense;
};

struct AssetDatabase {
	unsigned int m_ObjectHideFlags;
	map<string, AssetTimeStamp> m_AssetTimeStamps;
	map<GUID, Asset> m_Assets;
	int m_UnityShadersVersion;
	map<int, unsigned int> m_lastValidVersionHashes;
	map<int, AssetBundleFullName> m_AssetBundleNames;
	AssetDatabaseMetrics m_Metrics;
};

struct Item {
	bool markedForRemoval;
	int intdownloadResolution;
	int intnameConflictResolution;
};

struct DeletedItem {
	int changeset;
	GUID guid;
	GUID parent;
	string fullPath;
	int type;
	Hash128 digest;
};

struct CachedAssetMetaData {
	GUID guid;
	string pathName;
	unsigned int originalChangeset;
	string originalName;
	GUID originalParent;
	Hash128 originalDigest;
};

struct AssetServerCache {
	unsigned int m_ObjectHideFlags;
	map<GUID, Item> m_Items;
	map<GUID, DeletedItem> m_DeletedItems;
	string m_LastCommitMessage;
	set<pointer32<GUID> > m_CommitItemSelection;
	map<GUID, CachedAssetMetaData> m_WorkingItemMetaData;
	int m_LatestServerChangeset;
	int m_CachesInitialized;
	map<GUID, Item> m_ModifiedItems;
};
struct EditorUserBuildSettings {
	unsigned int m_ObjectHideFlags;
	vector<string> m_BuildLocation;
	int m_ActiveBuildTarget;
	int m_SelectedBuildTargetGroup;
	int m_SelectedStandaloneTarget;
	int m_ArchitectureFlags;
	int m_SelectedPSP2Subtarget;
	int m_SelectedPS4Subtarget;
	int m_SelectedPS3Subtarget;
	int m_SelectedPSMSubtarget;
	int m_SelectedWiiUDebugLevel;
	int m_SelectedWiiUBuildOutput;
	int m_SelectedWiiUBootMode;
	int m_SelectedXboxSubtarget;
	int m_SelectedXboxRunMethod;
	bool m_Development;
	bool m_ConnectProfiler;
	bool m_AllowDebugging;
	bool m_WebPlayerStreamed;
	bool m_WebPlayerOfflineDeployment;
	bool m_InstallInBuildFolder;
	bool m_SymlinkLibraries;
	bool m_SymlinkTrampoline;
	int m_SelectedIOSBuildType;
	bool m_NeedSubmissionMaterials;
	bool m_CompressWithPsArc;
	bool m_ExplicitNullChecks;
	bool m_EnableHeadlessMode;
	bool m_BuildScriptsOnly;
	bool m_WiiUEnableNetAPI;
	int m_WebGLOptimizationLevel;
	int m_SelectedAndroidSubtarget;
	int m_SelectedWSASDK;
	int m_SelectedWSAUWPBuildType;
	int m_SelectedWSABuildAndRunDeployTarget;
	bool m_GenerateWSAReferenceProjects;
	vector<bool> m_WSADotNetNativeEnabled;
	int m_SelectedBlackBerrySubtarget;
	int m_SelectedBlackBerryBuildType;
	int m_SelectedTizenSubtarget;
	int m_XboxOneStreamingInstallLaunchChunkRange;
	int m_SelectedXboxOneDeployMethod;
	string m_XboxOneUsername;
	string m_XboxOneNetworkSharePath;
	bool m_ForceOptimizeScriptCompilation;
};

struct EditorUserSettings {
	unsigned int m_ObjectHideFlags;
	map<string, string> m_ConfigValues;
	bool m_VCAutomaticAdd;
	bool m_VCDebugCom;
	bool m_VCDebugCmd;
	bool m_VCDebugOut;
	int m_SemanticMergeMode;
	bool m_VCShowFailedCheckout;
};

struct ExpandedData {
	bool m_InspectorExpanded;
	int m_ClassID;
	string m_ScriptClass;
	vector<string> m_ExpandedProperties;
};

struct InspectorExpandedState {
	unsigned int m_ObjectHideFlags;
	vector<ExpandedData> m_ExpandedData;
};

struct AudioManager {
	unsigned int m_ObjectHideFlags;
	float m_Volume;
	float Rolloff_Scale;
	float Doppler_Factor;
	int Default_Speaker_Mode;
	int m_SampleRate;
	int m_DSPBufferSize;
	int m_VirtualVoiceCount;
	int m_RealVoiceCount;
	string m_SpatializerPlugin;
	bool m_DisableAudio;
	bool m_VirtualizeEffects;
};
struct PhysicsManager {
	unsigned int m_ObjectHideFlags;
	Vector3f m_Gravity;
	PPtr<PhysicMaterial> m_DefaultMaterial;
	float m_BounceThreshold;
	float m_SleepThreshold;
	float m_DefaultContactOffset;
	int m_SolverIterationCount;
	int m_SolverVelocityIterations;
	bool m_QueriesHitTriggers;
	bool m_EnableAdaptiveForce;
	vector<unsigned int> m_LayerCollisionMatrix;
};

struct EditorSettings {
	unsigned int m_ObjectHideFlags;
	string m_ExternalVersionControlSupport;
	int m_SerializationMode;
	int m_WebSecurityEmulationEnabled;
	string m_WebSecurityEmulationHostUrl;
	int m_DefaultBehaviorMode;
	int m_SpritePackerMode;
	int m_SpritePackerPaddingPower;
	string m_ProjectGenerationIncludedExtensions;
	string m_ProjectGenerationRootNamespace;
};

struct BuiltinShaderSettings {
	int m_Mode;
	PPtr<Shader> m_Shader;
};

struct PlatformShaderSettings {
	bool useCascadedShadowMaps;
	bool useSinglePassStereoRendering;
	int standardShaderQuality;
	bool useReflectionProbeBoxProjection;
	bool useReflectionProbeBlending;
};

struct BuildTargetShaderSettings {
	string m_BuildTarget;
	int m_Tier;
	PlatformShaderSettings m_ShaderSettings;
	bool m_Automatic;
};

struct ShaderVariantCollection;

struct GraphicsSettings {
	unsigned int m_ObjectHideFlags;
	BuiltinShaderSettings m_Deferred;
	BuiltinShaderSettings m_DeferredReflections;
	BuiltinShaderSettings m_ScreenSpaceShadows;
	BuiltinShaderSettings m_LegacyDeferred;
	BuiltinShaderSettings m_DepthNormals;
	BuiltinShaderSettings m_MotionVectors;
	BuiltinShaderSettings m_LightHalo;
	BuiltinShaderSettings m_LensFlare;
	vector<PPtr<Shader>> m_AlwaysIncludedShaders;
	vector<PPtr<ShaderVariantCollection>> m_PreloadedShaders;
	PlatformShaderSettings m_ShaderSettings_Tier1;
	PlatformShaderSettings m_ShaderSettings_Tier2;
	PlatformShaderSettings m_ShaderSettings_Tier3;
	vector<BuildTargetShaderSettings> m_BuildTargetShaderSettings;
	int m_LightmapStripping;
	int m_FogStripping;
	bool m_LightmapKeepPlain;
	bool m_LightmapKeepDirCombined;
	bool m_LightmapKeepDirSeparate;
	bool m_LightmapKeepDynamicPlain;
	bool m_LightmapKeepDynamicDirCombined;
	bool m_LightmapKeepDynamicDirSeparate;
	bool m_FogKeepLinear;
	bool m_FogKeepExp;
	bool m_FogKeepExp2;
};

struct InputAxis {
	string m_Name;
	string descriptiveName;
	string descriptiveNegativeName;
	string negativeButton;
	string positiveButton;
	string altNegativeButton;
	string altPositiveButton;
	float gravity;
	float dead;
	float sensitivity;
	bool snap;
	bool invert;
	int type;
	int axis;
	int joyNum;
};

struct InputManager {
	unsigned int m_ObjectHideFlags;
	vector<InputAxis> m_Axes;
};

struct NavMeshAreaData {
	string name;
	float cost;
};

struct NavMeshAreas {
	unsigned int m_ObjectHideFlags;
	vector<NavMeshAreaData> areas;
};
struct NetworkManager {
	unsigned int m_ObjectHideFlags;
	int m_DebugLevel;
	float m_Sendrate;
	map<GUID, PPtr<GameObject>> m_AssetToPrefab;
};

struct Physics2DSettings {
	unsigned int m_ObjectHideFlags;
	Vector2f m_Gravity;
	PPtr<PhysicsMaterial2D> m_DefaultMaterial;
	int m_VelocityIterations;
	int m_PositionIterations;
	float m_VelocityThreshold;
	float m_MaxLinearCorrection;
	float m_MaxAngularCorrection;
	float m_MaxTranslationSpeed;
	float m_MaxRotationSpeed;
	float m_MinPenetrationForPenalty;
	float m_BaumgarteScale;
	float m_BaumgarteTimeOfImpactScale;
	float m_TimeToSleep;
	float m_LinearSleepTolerance;
	float m_AngularSleepTolerance;
	bool m_QueriesHitTriggers;
	bool m_QueriesStartInColliders;
	bool m_ChangeStopsCallbacks;
	bool m_AlwaysShowColliders;
	bool m_ShowColliderSleep;
	bool m_ShowColliderContacts;
	float m_ContactArrowScale;
	ColorRGBA m_ColliderAwakeColor;
	ColorRGBA m_ColliderAsleepColor;
	ColorRGBA m_ColliderContactColor;
	vector<unsigned int> m_LayerCollisionMatrix;
};


struct QualitySetting {
	string name;
	int pixelLightCount;
	int shadows;
	int shadowResolution;
	int shadowProjection;
	int shadowCascades;
	float shadowDistance;
	float shadowNearPlaneOffset;
	float shadowCascade2Split;
	Vector3f shadowCascade4Split;
	int blendWeights;
	int textureQuality;
	int anisotropicTextures;
	int antiAliasing;
	bool softParticles;
	bool softVegetation;
	bool realtimeReflectionProbes;
	bool billboardsFaceCameraPosition;
	int vSyncCount;
	float lodBias;
	int maximumLODLevel;
	int particleRaycastBudget;
	int asyncUploadTimeSlice;
	int asyncUploadBufferSize;
	vector<string> excludedTargetPlatforms;
};

struct QualitySettings {
	unsigned int m_ObjectHideFlags;
	int m_CurrentQuality;
	vector<QualitySetting> m_QualitySettings;
	map<string, int> m_PerPlatformDefaultQuality;
};

struct SortingLayerEntry {
	string name;
	unsigned int uniqueID;
	bool locked;
};

struct TagManager {
	vector<string> tags;
	vector<string> layers;
	vector<SortingLayerEntry> m_SortingLayers;
};

struct TimeManager {
	unsigned int m_ObjectHideFlags;
	float Fixed_Timestep;
	float Maximum_Allowed_Timestep;
	float m_TimeScale;
};
struct Annotation {
	bool m_IconEnabled;
	bool m_GizmoEnabled;
	int m_ClassID;
	string m_ScriptClass;
	int m_Flags;
};

struct AnnotationManager {
	unsigned int m_ObjectHideFlags;
	vector<Annotation> m_CurrentPreset_m_AnnotationList;
	vector<Annotation> m_RecentlyChanged;
	float m_WorldIconSize;
	bool m_Use3dGizmos;
	bool m_ShowGrid;
};

struct RuntimeAnimatorController;
struct Avatar;
struct Animator {
	unsigned int m_ObjectHideFlags;
	PPtr<EditorExtension> m_PrefabParentObject;
	PPtr<Prefab> m_PrefabInternal;
	PPtr<GameObject> m_GameObject;
	unsigned char m_Enabled;
	PPtr<Avatar> m_Avatar;
	PPtr<RuntimeAnimatorController> m_Controller;
	int m_CullingMode;
	int m_UpdateMode;
	bool m_ApplyRootMotion;
	bool m_LinearVelocityBlending;
	string m_WarningMessage;
	bool m_HasTransformHierarchy;
	bool m_AllowConstantClipSamplingOptimization;
};

struct EnlightenSystemAtlasInformation {
	int atlasSize;
	Hash128 atlasHash;
	int firstSystemId;
};

struct SceneObjectIdentifier {
	int64 targetObject;
	int64 targetPrefab;
};

struct RendererData {
	PPtr<Mesh> uvMesh;
	Vector4f terrainDynamicUVST;
	Vector4f terrainChunkDynamicUVST;
	unsigned short lightmapIndex;
	unsigned short lightmapIndexDynamic;
	Vector4f lightmapST;
	Vector4f lightmapSTDynamic;
};

struct EnlightenRendererInformation {
	PPtr<Object> renderer;
	Vector4f dynamicLightmapSTInSystem;
	int systemId;
	Hash128 instanceHash;
	Hash128 geometryHash;
};

struct EnlightenSystemInformation {
	unsigned int rendererIndex;
	unsigned int rendererSize;
	int atlasIndex;
	int atlasOffsetX;
	int atlasOffsetY;
	Hash128 inputSystemHash;
	Hash128 radiositySystemHash;
};

struct EnlightenTerrainChunksInformation {
	int firstSystemId;
	int numChunksInX;
	int numChunksInY;
};

struct EnlightenSceneMapping {
	vector<EnlightenRendererInformation> m_Renderers;
	vector<EnlightenSystemInformation> m_Systems;
	vector<Hash128> m_Probesets;
	vector<EnlightenSystemAtlasInformation> m_SystemAtlases;
	vector<EnlightenTerrainChunksInformation> m_TerrainChunks;
};

struct LightmapData {
	PPtr<Texture2D> m_Lightmap;
	PPtr<Texture2D> m_IndirectLightmap;
};

struct SphericalHarmonicsL2 {
	float sh[27];
};
struct Tetrahedron {
	int indices[4];
	int neighbors[4];
	Matrix3x4f matrix;
};

struct ProbeSetTetrahedralization {
	vector<Tetrahedron> m_Tetrahedra;
	vector<Vector3f> m_HullRays;
};

struct ProbeSetIndex {
	Hash128 m_Hash;
	int m_Offset;
	int m_Size;
};

struct LightProbeData {
	ProbeSetTetrahedralization m_Tetrahedralization;
	vector<ProbeSetIndex> m_ProbeSets;
	vector<Vector3f> m_Positions;
	map<Hash128, int> m_NonTetrahedralizedProbeSetIndexMap;
};

struct LightProbeOcclusion {
	vector<int> m_BakedLightIndex;
	vector<int> m_Occlusion;
};

struct LightProbes {
	unsigned int m_ObjectHideFlags;
	PPtr<EditorExtension> m_PrefabParentObject;
	PPtr<Prefab> m_PrefabInternal;
	string m_Name;
	LightProbeData m_Data;
	vector<SphericalHarmonicsL2> m_BakedCoefficients;
	vector<LightProbeOcclusion> m_BakedLightOcclusion;
};
struct LightingDataAsset {
	unsigned int m_ObjectHideFlags;
	PPtr<EditorExtension> m_PrefabParentObject;
	PPtr<Prefab> m_PrefabInternal;
	string m_Name;
	vector<LightmapData> m_Lightmaps;
	PPtr<LightProbes> m_LightProbes;
	int m_LightmapsMode;
	SphericalHarmonicsL2 m_BakedAmbientProbeInLinear;
	vector<RendererData> m_LightmappedRendererData;
	vector<SceneObjectIdentifier> m_LightmappedRendererDataIDs;
	EnlightenSceneMapping m_EnlightenSceneMapping;
	vector<SceneObjectIdentifier> m_EnlightenSceneMappingRendererIDs;
	vector<SceneObjectIdentifier> m_Lights;
	vector<int> m_BakedLightIndices;
	vector<PPtr<Texture>> m_BakedReflectionProbeCubemaps;
	vector<SceneObjectIdentifier> m_BakedReflectionProbes;
	vector<uint8> m_EnlightenData;
	int m_EnlightenDataVersion;
};

struct LightmapSettings : Object {
	int m_GIWorkflowMode;
	GISettings m_GISettings;
	LightmapEditorSettings m_LightmapEditorSettings;
	PPtr<LightingDataAsset> m_LightingDataAsset;
	int m_RuntimeCPUUsage;
};

typedef anything
LevelGameManager,
GlobalGameManager,
GameManager,
Pipeline,
Skybox,
Physics2DManager,
Collider2D,
Joint,
ComputeShader,
AnimationClip,
ParticleEmitter,
AnimatorController,
GUILayer,
ScriptMapper,
DelayedCallManager,
CGProgram,
BaseAnimationTrack,
Animation,
Texture3D,
NewAnimationTrack,
FlareLayer,
HaloLayer,
HaloManager,
PlayerSettings,
NamedObject,
GUITexture,
GUIText,
GUIElement,
RaycastCollider,
BuildSettings,
AssetBundle,
ResourceManager,
NetworkView,
PreloadData,
MovieTexture,
MasterServerInterface,
WebCamTexture,
InteractiveCloth,
ClothRenderer,
SkinnedCloth,
AudioReverbFilter,
AudioHighPassFilter,
AudioChorusFilter,
AudioReverbZone,
AudioEchoFilter,
AudioLowPassFilter,
AudioDistortionFilter,
SparseTexture,
AudioBehaviour,
AudioFilter,
WindZone,
SubstanceArchive,
ProceduralMaterial,
ProceduralTexture,
OffMeshLink,
OcclusionArea,
Tree,
NavMeshObsolete,
NavMeshAgent,
LightProbesLegacy,
LODGroup,
BlendTree,
Motion,
NavMeshObstacle,
TerrainInstance,
CachedSpriteAtlas,
ReflectionProbe,
ReflectionProbes,
LightProbeGroup,
AnimatorOverrideController,
CanvasGroup,
BillboardAsset,
BillboardRenderer,
SpeedTreeWindAsset,
AnchoredJoint2D,
Joint2D,
AudioMixer,
AudioMixerController,
AudioMixerGroupController,
AudioMixerEffectController,
AudioMixerSnapshotController,
PhysicsUpdateBehaviour2D,
Effector2D,
SpriteCollider2D,
SampleClip,
AudioMixerSnapshot,
AssetBundleManifest,
EditorExtensionImpl,
AssetImporter,
Mesh3DSImporter,
TextureImporter,
ShaderImporter,
ComputeShaderImporter,
AvatarMask,
AudioImporter,
HierarchyState,
GUIDSerializer,
AssetMetaData,
DefaultAsset,
DefaultImporter,
TextScriptImporter,
SceneAsset,
NativeFormatImporter,
MonoImporter,
LibraryAssetImporter,
ModelImporter,
FBXImporter,
TrueTypeFontImporter,
MovieImporter,
EditorBuildSettings,
DDSImporter,
PluginImporter,
PVRImporter,
ASTCImporter,
KTXImporter,
AnimatorStateTransition,
AnimatorState,
HumanTemplate,
AnimatorStateMachine,
PreviewAssetType,
AnimatorTransition,
SpeedTreeImporter,
AnimatorTransitionBase,
SubstanceImporter,
LightmapSnapshot,
Int,
Bool,
Float,
MonoObject,
Collision,
RootMotionData
;

}	//namespace unity

//-----------------------------------------------------------------------------
//	ISO
//-----------------------------------------------------------------------------

#define ENUM(x) #x, unity::Class::x
template<> struct ISO::def<unity::Class::id> : public ISO::TypeUserEnumN<200, 32> {
	def() : ISO::TypeUserEnumN<200, 32>("unity::classID", NONE,
		ENUM(GameObject),
		ENUM(Component),
		ENUM(LevelGameManager),
		ENUM(Transform),
		ENUM(TimeManager),
		ENUM(GlobalGameManager),
		ENUM(Behaviour),
		ENUM(GameManager),
		ENUM(AudioManager),
		ENUM(ParticleAnimator),
		ENUM(InputManager),
		ENUM(EllipsoidParticleEmitter),
		ENUM(Pipeline),
		ENUM(EditorExtension),
		ENUM(Physics2DSettings),
		ENUM(Camera),
		ENUM(Material),
		ENUM(MeshRenderer),
		ENUM(Renderer),
		ENUM(ParticleRenderer),
		ENUM(Texture),
		ENUM(Texture2D),
		ENUM(SceneSettings),
		ENUM(GraphicsSettings),
		ENUM(MeshFilter),
		ENUM(OcclusionPortal),
		ENUM(Mesh),
		ENUM(Skybox),
		ENUM(QualitySettings),
		ENUM(Shader),
		ENUM(TextAsset),
		ENUM(Rigidbody2D),
		ENUM(Physics2DManager),
		ENUM(Collider2D),
		ENUM(Rigidbody),
		ENUM(PhysicsManager),
		ENUM(Collider),
		ENUM(Joint),
		ENUM(CircleCollider2D),
		ENUM(HingeJoint),
		ENUM(PolygonCollider2D),
		ENUM(BoxCollider2D),
		ENUM(PhysicsMaterial2D),
		ENUM(MeshCollider),
		ENUM(BoxCollider),
		ENUM(SpriteCollider2D),
		ENUM(EdgeCollider2D),
		ENUM(ComputeShader),
		ENUM(AnimationClip),
		ENUM(ConstantForce),
		ENUM(WorldParticleCollider),
		ENUM(TagManager),
		ENUM(AudioListener),
		ENUM(AudioSource),
		ENUM(AudioClip),
		ENUM(RenderTexture),
		ENUM(MeshParticleEmitter),
		ENUM(ParticleEmitter),
		ENUM(Cubemap),
		ENUM(Avatar),
		ENUM(AnimatorController),
		ENUM(GUILayer),
		ENUM(RuntimeAnimatorController),
		ENUM(ScriptMapper),
		ENUM(Animator),
		ENUM(TrailRenderer),
		ENUM(DelayedCallManager),
		ENUM(TextMesh),
		ENUM(RenderSettings),
		ENUM(Light),
		ENUM(CGProgram),
		ENUM(BaseAnimationTrack),
		ENUM(Animation),
		ENUM(MonoBehaviour),
		ENUM(MonoScript),
		ENUM(MonoManager),
		ENUM(Texture3D),
		ENUM(NewAnimationTrack),
		ENUM(Projector),
		ENUM(LineRenderer),
		ENUM(Flare),
		ENUM(Halo),
		ENUM(LensFlare),
		ENUM(FlareLayer),
		ENUM(HaloLayer),
		ENUM(NavMeshAreas),
		ENUM(HaloManager),
		ENUM(Font),
		ENUM(PlayerSettings),
		ENUM(NamedObject),
		ENUM(GUITexture),
		ENUM(GUIText),
		ENUM(GUIElement),
		ENUM(PhysicMaterial),
		ENUM(SphereCollider),
		ENUM(CapsuleCollider),
		ENUM(SkinnedMeshRenderer),
		ENUM(FixedJoint),
		ENUM(RaycastCollider),
		ENUM(BuildSettings),
		ENUM(AssetBundle),
		ENUM(CharacterController),
		ENUM(CharacterJoint),
		ENUM(SpringJoint),
		ENUM(WheelCollider),
		ENUM(ResourceManager),
		ENUM(NetworkView),
		ENUM(NetworkManager),
		ENUM(PreloadData),
		ENUM(MovieTexture),
		ENUM(ConfigurableJoint),
		ENUM(TerrainCollider),
		ENUM(MasterServerInterface),
		ENUM(TerrainData),
		ENUM(LightmapSettings),
		ENUM(WebCamTexture),
		ENUM(EditorSettings),
		ENUM(InteractiveCloth),
		ENUM(ClothRenderer),
		ENUM(EditorUserSettings),
		ENUM(SkinnedCloth),
		ENUM(AudioReverbFilter),
		ENUM(AudioHighPassFilter),
		ENUM(AudioChorusFilter),
		ENUM(AudioReverbZone),
		ENUM(AudioEchoFilter),
		ENUM(AudioLowPassFilter),
		ENUM(AudioDistortionFilter),
		ENUM(SparseTexture),
		ENUM(AudioBehaviour),
		ENUM(AudioFilter),
		ENUM(WindZone),
		ENUM(Cloth),
		ENUM(SubstanceArchive),
		ENUM(ProceduralMaterial),
		ENUM(ProceduralTexture),
		ENUM(OffMeshLink),
		ENUM(OcclusionArea),
		ENUM(Tree),
		ENUM(NavMeshObsolete),
		ENUM(NavMeshAgent),
		ENUM(NavMeshSettings),
		ENUM(LightProbesLegacy),
		ENUM(ParticleSystem),
		ENUM(ParticleSystemRenderer),
		ENUM(ShaderVariantCollection),
		ENUM(LODGroup),
		ENUM(BlendTree),
		ENUM(Motion),
		ENUM(NavMeshObstacle),
		ENUM(TerrainInstance),
		ENUM(SpriteRenderer),
		ENUM(Sprite),
		ENUM(CachedSpriteAtlas),
		ENUM(ReflectionProbe),
		ENUM(ReflectionProbes),
		ENUM(Terrain),
		ENUM(LightProbeGroup),
		ENUM(AnimatorOverrideController),
		ENUM(CanvasRenderer),
		ENUM(Canvas),
		ENUM(RectTransform),
		ENUM(CanvasGroup),
		ENUM(BillboardAsset),
		ENUM(BillboardRenderer),
		ENUM(SpeedTreeWindAsset),
		ENUM(AnchoredJoint2D),
		ENUM(Joint2D),
		ENUM(SpringJoint2D),
		ENUM(DistanceJoint2D),
		ENUM(HingeJoint2D),
		ENUM(SliderJoint2D),
		ENUM(WheelJoint2D),
		ENUM(NavMeshData),
		ENUM(AudioMixer),
		ENUM(AudioMixerController),
		ENUM(AudioMixerGroupController),
		ENUM(AudioMixerEffectController),
		ENUM(AudioMixerSnapshotController),
		ENUM(PhysicsUpdateBehaviour2D),
		ENUM(ConstantForce2D),
		ENUM(Effector2D),
		ENUM(AreaEffector2D),
		ENUM(PointEffector2D),
		ENUM(PlatformEffector2D),
		ENUM(SurfaceEffector2D),
		ENUM(LightProbes),
		ENUM(SampleClip),
		ENUM(AudioMixerSnapshot),
		ENUM(AudioMixerGroup),
		ENUM(AssetBundleManifest),
		ENUM(Prefab),
		ENUM(EditorExtensionImpl),
		ENUM(AssetImporter),
		ENUM(AssetDatabase),
		ENUM(Mesh3DSImporter),
		ENUM(TextureImporter),
		ENUM(ShaderImporter),
		ENUM(ComputeShaderImporter),
		ENUM(AvatarMask),
		ENUM(AudioImporter),
		ENUM(HierarchyState),
		ENUM(GUIDSerializer),
		ENUM(AssetMetaData),
		ENUM(DefaultAsset),
		ENUM(DefaultImporter),
		ENUM(TextScriptImporter),
		ENUM(SceneAsset),
		ENUM(NativeFormatImporter),
		ENUM(MonoImporter),
		ENUM(AssetServerCache),
		ENUM(LibraryAssetImporter),
		ENUM(ModelImporter),
		ENUM(FBXImporter),
		ENUM(TrueTypeFontImporter),
		ENUM(MovieImporter),
		ENUM(EditorBuildSettings),
		ENUM(DDSImporter),
		ENUM(InspectorExpandedState),
		ENUM(AnnotationManager),
		ENUM(PluginImporter),
		ENUM(EditorUserBuildSettings),
		ENUM(PVRImporter),
		ENUM(ASTCImporter),
		ENUM(KTXImporter),
		ENUM(AnimatorStateTransition),
		ENUM(AnimatorState),
		ENUM(HumanTemplate),
		ENUM(AnimatorStateMachine),
		ENUM(PreviewAssetType),
		ENUM(AnimatorTransition),
		ENUM(SpeedTreeImporter),
		ENUM(AnimatorTransitionBase),
		ENUM(SubstanceImporter),
		ENUM(LightmapParameters),
		ENUM(LightmapSnapshot),
		ENUM(Int),
		ENUM(Bool),
		ENUM(Float),
		ENUM(MonoObject),
		ENUM(Collision),
		ENUM(Vector3f),
		ENUM(RootMotionData)
	)
#undef ENUM
	{}
};
ISO_DEFUSER(unity::GUID, uint32[4]);
ISO_DEFUSERCOMPV(unity::Rectf, x, y, w, h);
ISO_DEFUSERCOMPV(unity::RectInt, x, y, w, h);

//ISO_DEFUSER(unity::ColorRGBA,		rgba8);
//ISO_DEFUSER(unity::Vector2f,		float2p);
//ISO_DEFUSER(unity::Vector3f,		float3p);
//ISO_DEFUSER(unity::Quaternionf,		float4p);
ISO_DEFUSER(unity::Matrix4x4f,		float4x4p);
ISO_DEFUSER(unity::FastPropertyName,string);

ISO_DEFUSERCOMPV(unity::AABB,		m_Center, m_Extent);
ISO_DEFUSERCOMPV(unity::BitField,	m_Bits);
ISO_DEFUSERCOMPV(unity::Ref,		m_FileID, m_PathID);

template<typename T> _ISO_DEFUSER(unity::PPtr<T>, unity::Ref, "PPtr", NONE);
template<typename T> _ISO_DEFUSER(unity::vector<T>, ISO_openarray<T>, "vector", NONE);
template<typename K, typename V> struct ISO::def<unity::map<K,V> > : ISO::TypeUserSave { def() : ISO::TypeUserSave("map", ISO::getdef<ISO_openarray<pair<K, V> > >()) {} };

ISO_DEFUSERCOMPV(unity::Hash128, bytes);
ISO_DEFUSERCOMPV(unity::GLTextureSettings, m_FilterMode, m_Aniso, m_MipBias, m_WrapMode);
ISO_DEFUSERCOMPV(unity::StreamingInfo, offset, size, path);

ISO_DEFUSERCOMPV(unity::Object, m_ObjectHideFlags);

ISO_DEFUSERCOMPV(unity::Texture2D
	, m_ObjectHideFlags, m_PrefabParentObject, m_PrefabInternal, m_Name, m_ImageContentsHash, m_Width, m_Height, m_CompleteImageSize
	, m_TextureFormat, m_MipCount, m_IsReadable, m_ReadAllowed, m_AlphaIsTransparency, m_ImageCount, m_TextureDimension, m_TextureSettings
	, m_LightmapFormat, m_ColorSpace, image_data, m_StreamData
);

ISO_DEFUSERCOMPV(unity::UnityTexEnv, m_Texture, m_Scale, m_Offset);
ISO_DEFUSERCOMPV(unity::UnityPropertySheet, m_TexEnvs, m_Floats, m_Colors);

ISO_DEFUSERCOMPV(unity::GameObject
	, m_ObjectHideFlags, m_PrefabParentObject, m_PrefabInternal, m_Component, m_Layer, m_Name, m_TagString, m_Icon
	, m_NavMeshLayer, m_StaticEditorFlags, m_IsActive
);


ISO_DEFUSERCOMPV(unity::Transform
	, m_ObjectHideFlags, m_PrefabParentObject, m_PrefabInternal, m_GameObject, m_LocalRotation, m_LocalPosition, m_LocalScale, m_LocalEulerAnglesHint
	, m_Children, m_Father, m_RootOrder
);

ISO_DEFUSERCOMPV(unity::PropertyModification, target, propertyPath, value, objectReference);
ISO_DEFUSERCOMPV(unity::PrefabModification, m_TransformParent, m_Modifications, m_RemovedComponents);
ISO_DEFUSERCOMPV(unity::Prefab, m_ObjectHideFlags, m_Modification, m_ParentPrefab, m_RootGameObject, m_IsPrefabParent);

ISO_DEFUSERCOMPV(unity::Material, m_ObjectHideFlags, m_PrefabParentObject, m_PrefabInternal, m_Name, m_Shader, m_ShaderKeywords, m_LightmapFlags, m_CustomRenderQueue, stringTagMap, m_SavedProperties);

ISO_DEFUSERCOMPV(unity::MeshRenderer
	, m_ObjectHideFlags, m_PrefabParentObject, m_PrefabInternal, m_GameObject, m_Enabled, m_CastShadows, m_ReceiveShadows, m_MotionVectors
	, m_LightProbeUsage, m_ReflectionProbeUsage, m_Materials, m_SubsetIndices, m_StaticBatchRoot, m_ProbeAnchor, m_LightProbeVolumeOverride, m_ScaleInLightmap
	, m_PreserveUVs, m_IgnoreNormalsForChartDetection, m_ImportantGI, m_SelectedWireframeHidden, m_MinimumChartSize, m_AutoUVMaxDistance, m_AutoUVMaxAngle, m_LightmapParameters
	, m_SortingLayerID, m_SortingOrder
);

ISO_DEFUSERCOMPV(unity::MeshFilter, m_ObjectHideFlags, m_PrefabParentObject, m_PrefabInternal, m_GameObject, m_Mesh);

ISO_DEFUSERCOMPV(unity::Rigidbody
	, m_ObjectHideFlags, m_PrefabParentObject, m_PrefabInternal, m_GameObject, m_Mass, m_Drag, m_AngularDrag, m_UseGravity
	, m_IsKinematic, m_Interpolate, m_Constraints, m_CollisionDetection
);

ISO_DEFUSERCOMPV(unity::TextMesh
	, m_ObjectHideFlags, m_PrefabParentObject, m_PrefabInternal, m_GameObject, m_Text, m_OffsetZ, m_CharacterSize, m_LineSpacing
	, m_Anchor, m_Alignment, m_TabSize, m_FontSize, m_FontStyle, m_RichText, m_Font, m_Color
);

ISO_DEFUSERCOMPV(unity::MonoBehaviour
	, m_ObjectHideFlags, m_PrefabParentObject, m_PrefabInternal, m_GameObject, m_Enabled, m_EditorHideFlags, m_Script, m_Name
	, m_EditorClassIdentifier
);

ISO_DEFUSERCOMPV(unity::Collider, m_ObjectHideFlags, m_PrefabParentObject, m_PrefabInternal, m_GameObject, m_Material, m_IsTrigger, m_Enabled);

ISO_DEFUSERCOMPV(unity::SphereCollider
	, m_ObjectHideFlags, m_PrefabParentObject, m_PrefabInternal, m_GameObject, m_Material, m_IsTrigger, m_Enabled, m_Radius
	, m_Center
);

ISO_DEFUSERCOMPV(unity::CapsuleCollider
	, m_ObjectHideFlags, m_PrefabParentObject, m_PrefabInternal, m_GameObject, m_Material, m_IsTrigger, m_Enabled, m_Radius
	, m_Height, m_Direction, m_Center
);

ISO_DEFUSERCOMPV(unity::GradientNEW
	, key0, key1, key2, key3, key4, key5, key6, key7
	, ctime0, ctime1, ctime2, ctime3, ctime4, ctime5, ctime6, ctime7
	, atime0, atime1, atime2, atime3, atime4, atime5, atime6, atime7
	, m_NumColorKeys, m_NumAlphaKeys
);

ISO_DEFUSERCOMPV(unity::MinMaxGradient, maxGradient, minGradient, minColor, maxColor, minMaxState);
ISO_DEFUSERCOMPV(unity::Keyframe, time, value, inSlope, outSlope, tangentMode);
ISO_DEFUSERCOMPV(unity::AnimationCurve, m_Curve, m_PreInfinity, m_PostInfinity, m_RotationOrder);
ISO_DEFUSERCOMPV(unity::MinMaxCurve, scalar, maxCurve, minCurve, minMaxState);

ISO_DEFUSERCOMPV(unity::InitialModule
	, enabled, startLifetime, startSpeed, startColor, startSize, startSizeY, startSizeZ
	, startRotationX, startRotationY, startRotation, randomizeRotationDirection, gravityModifier, maxNumParticles, size3D, rotation3D
);

ISO_DEFUSERCOMPV(unity::InheritVelocityModule, enabled, m_Mode, m_Curve);
ISO_DEFUSERCOMPV(unity::ExternalForcesModule, enabled, multiplier);
ISO_DEFUSERCOMPV(unity::SizeBySpeedModule, enabled, curve, y, z, range, separateAxes);
ISO_DEFUSERCOMPV(unity::ClampVelocityModule, enabled, x, y, z, magnitude, separateAxis, inWorldSpace, dampen);
ISO_DEFUSERCOMPV(unity::RotationBySpeedModule, enabled, x, y, curve, separateAxes, range);
ISO_DEFUSERCOMPV(unity::ColorBySpeedModule, enabled, gradient, range);

ISO_DEFUSERCOMPV(unity::EmissionModule
	, enabled, m_Type, rate, cnt0, cnt1, cnt2, cnt3, cntmax0
	, cntmax1, cntmax2, cntmax3, time0, time1, time2, time3, m_BurstCount
);

ISO_DEFUSERCOMPV(unity::ShapeModule
	, enabled, type, radius, angle, length, boxX, boxY, boxZ
	, arc, placementMode, m_Mesh, m_MeshRenderer, m_SkinnedMeshRenderer, m_MeshMaterialIndex, m_MeshNormalOffset, m_UseMeshMaterialIndex
	, m_UseMeshColors, randomDirection
);

ISO_DEFUSERCOMPV(unity::SizeModule, enabled, curve, y, z, separateAxes);

ISO_DEFUSERCOMPV(unity::UVModule
	, enabled, frameOverTime, startFrame, tilesX, tilesY, animationType, rowIndex, cycles
	, uvChannelMask, randomRow
);

ISO_DEFUSERCOMPV(unity::ColorModule, enabled, gradient);
ISO_DEFUSERCOMPV(unity::VelocityModule, enabled, x, y, z, inWorldSpace);
ISO_DEFUSERCOMPV(unity::ForceModule, enabled, x, y, z, inWorldSpace, randomizePerFrame);
ISO_DEFUSERCOMPV(unity::RotationModule, enabled, x, y, curve, separateAxes);

ISO_DEFUSERCOMPV(unity::TriggerModule
	, enabled, collisionShape0, collisionShape1, collisionShape2, collisionShape3, collisionShape4, collisionShape5, inside
	, outside, enter, exit, radiusScale
);

ISO_DEFUSERCOMPV(unity::CollisionModule
	, enabled, type, collisionMode, plane0, plane1, plane2, plane3, plane4
	, plane5, m_Dampen, m_Bounce, m_EnergyLossOnCollision, minKillSpeed, maxKillSpeed, radiusScale, collidesWith
	, maxCollisionShapes, quality, voxelSize, collisionMessages, collidesWithDynamic, interiorCollisions
);

ISO_DEFUSERCOMPV(unity::SubModule, enabled, subEmitterBirth, subEmitterBirth1, subEmitterCollision, subEmitterCollision1, subEmitterDeath, subEmitterDeath1);

ISO_DEFUSERCOMPV(unity::ParticleSystem
	, m_ObjectHideFlags, m_PrefabParentObject, m_PrefabInternal, m_GameObject, lengthInSec, startDelay, speed, randomSeed
	, looping, prewarm, playOnAwake, moveWithTransform, scalingMode, InitialModule, ShapeModule, EmissionModule
	, SizeModule, RotationModule, ColorModule, UVModule, VelocityModule, InheritVelocityModule, ForceModule, ExternalForcesModule
	, ClampVelocityModule, SizeBySpeedModule, RotationBySpeedModule, ColorBySpeedModule, CollisionModule, TriggerModule, SubModule
);

ISO_DEFUSERCOMPV(unity::ParticleSystemRenderer
	, m_ObjectHideFlags, m_PrefabParentObject, m_PrefabInternal, m_GameObject, m_Enabled, m_CastShadows, m_ReceiveShadows, m_MotionVectors
	, m_LightProbeUsage, m_ReflectionProbeUsage, m_Materials, m_SubsetIndices, m_StaticBatchRoot, m_ProbeAnchor, m_LightProbeVolumeOverride, m_ScaleInLightmap
	, m_PreserveUVs, m_IgnoreNormalsForChartDetection, m_ImportantGI, m_SelectedWireframeHidden, m_MinimumChartSize, m_AutoUVMaxDistance, m_AutoUVMaxAngle, m_LightmapParameters
	, m_SortingLayerID, m_SortingOrder, m_RenderMode, m_SortMode, m_MinParticleSize, m_MaxParticleSize, m_CameraVelocityScale, m_VelocityScale
	, m_LengthScale, m_SortingFudge, m_NormalDirection, m_RenderAlignment, m_Pivot, m_Mesh, m_Mesh1, m_Mesh2
	, m_Mesh3
);

ISO_DEFUSERCOMPV(unity::AudioSource
	, m_ObjectHideFlags, m_PrefabParentObject, m_PrefabInternal, m_GameObject, m_Enabled, OutputAudioMixerGroup, m_audioClip, m_PlayOnAwake
	, m_Volume, m_Pitch, Loop, Mute, Spatialize, Priority, DopplerLevel, MinDistance
	, MaxDistance, Pan2D, rolloffMode, BypassEffects, BypassListenerEffects, BypassReverbZones, rolloffCustomCurve, panLevelCustomCurve
	, spreadCustomCurve, reverbZoneMixCustomCurve
);

ISO_DEFUSERCOMPV(unity::Halo, m_ObjectHideFlags, m_PrefabParentObject, m_PrefabInternal, m_GameObject, m_Enabled, m_Color, m_Size);


ISO_DEFUSERCOMPV(unity::Behaviour, m_ObjectHideFlags, m_PrefabParentObject, m_PrefabInternal, m_GameObject, m_Enabled);
ISO_DEFUSERCOMPV(unity::EditorExtension, m_PrefabParentObject, m_PrefabInternal);
ISO_DEFUSERCOMPV(unity::Camera
	, m_ObjectHideFlags, m_PrefabParentObject, m_PrefabInternal, m_GameObject, m_Enabled, m_ClearFlags, m_BackGroundColor, m_NormalizedViewPortRect
	, near_clip_plane, far_clip_plane, field_of_view, orthographic, orthographic_size, m_Depth, m_CullingMask, m_RenderingPath
	, m_TargetTexture, m_TargetDisplay, m_TargetEye, m_HDR, m_OcclusionCulling, m_StereoConvergence, m_StereoSeparation, m_StereoMirrorMode
);
ISO_DEFUSERCOMPV(unity::SceneSettings, m_ObjectHideFlags, m_PVSData, m_PVSObjectsArray, m_PVSPortalsArray, m_OcclusionBakeSettings);

ISO_DEFUSERCOMPV(unity::SubMesh, firstByte, indexCount, topology, firstVertex, vertexCount, localAABB);

ISO_DEFUSERCOMPV(unity::Mesh
	, m_ObjectHideFlags, m_PrefabParentObject, m_PrefabInternal, m_Name, m_SubMeshes, m_Shapes, m_BindPose, m_BoneNameHashes
	, m_RootBoneNameHash, m_MeshCompression, m_IsReadable, m_KeepVertices, m_KeepIndices, m_IndexBuffer, m_Skin, m_VertexData
	, m_CompressedMesh, m_LocalAABB, m_MeshUsageFlags, m_BakedConvexCollisionMesh, m_BakedTriangleCollisionMesh, m_MeshOptimized
);
ISO_DEFUSERCOMPV(unity::MeshCollider
	, m_ObjectHideFlags, m_PrefabParentObject, m_PrefabInternal, m_GameObject, m_Material, m_IsTrigger, m_Enabled, m_Convex
	, m_Mesh
);
ISO_DEFUSERCOMPV(unity::BoxCollider
	, m_ObjectHideFlags, m_PrefabParentObject, m_PrefabInternal, m_GameObject, m_Material, m_IsTrigger, m_Enabled, m_Size
	, m_Center
);
ISO_DEFUSERCOMPV(unity::AudioListener, m_ObjectHideFlags, m_PrefabParentObject, m_PrefabInternal, m_GameObject, m_Enabled);

ISO_DEFUSERCOMPV(unity::RenderSettings
	, m_ObjectHideFlags, m_Fog, m_FogColor, m_FogMode, m_FogDensity, m_LinearFogStart, m_LinearFogEnd, m_AmbientSkyColor
	, m_AmbientEquatorColor, m_AmbientGroundColor, m_AmbientIntensity, m_AmbientMode, m_SkyboxMaterial, m_HaloStrength, m_FlareStrength, m_FlareFadeSpeed
	, m_HaloTexture, m_SpotCookie, m_DefaultReflectionMode, m_DefaultReflectionResolution, m_ReflectionBounces, m_ReflectionIntensity, m_CustomReflection, m_Sun
	, m_IndirectSpecularColor
);
ISO_DEFUSERCOMPV(unity::Light
	, m_ObjectHideFlags, m_PrefabParentObject, m_PrefabInternal, m_GameObject, m_Enabled, m_Type, m_Color, m_Intensity
	, m_Range, m_SpotAngle, m_CookieSize, m_Shadows, m_Cookie, m_DrawHalo, m_Flare, m_RenderMode
	, m_CullingMask, m_Lightmapping, m_AreaSize, m_BounceIntensity, m_ShadowRadius, m_ShadowAngle
);
ISO_DEFUSERCOMPV(unity::LensFlare
	, m_ObjectHideFlags, m_PrefabParentObject, m_PrefabInternal, m_GameObject, m_Enabled, m_Flare, m_Color, m_Brightness
	, m_FadeSpeed, m_IgnoreLayers, m_Directional
);
ISO_DEFUSERCOMPV(unity::TerrainCollider, m_ObjectHideFlags, m_PrefabParentObject, m_PrefabInternal, m_GameObject, m_Material, m_Enabled, m_TerrainData, m_EnableTreeColliders);
ISO_DEFUSERCOMPV(unity::LightmapSettings, m_ObjectHideFlags, m_GIWorkflowMode, m_GISettings, m_LightmapEditorSettings, m_LightingDataAsset, m_RuntimeCPUUsage);

ISO_DEFUSERCOMPV(unity::NavMeshBuildSettings
	, agentRadius, agentHeight, agentSlope, agentClimb, ledgeDropHeight, maxJumpAcrossDistance, accuratePlacement, minRegionArea
	, cellSize, manualCellSize
);
ISO_DEFUSERCOMPV(unity::NavMeshSettings, m_ObjectHideFlags, m_BuildSettings, m_NavMeshData);

ISO_DEFUSERCOMPV(unity::Terrain
	, m_ObjectHideFlags, m_PrefabParentObject, m_PrefabInternal, m_GameObject, m_Enabled, m_TerrainData, m_TreeDistance, m_TreeBillboardDistance
	, m_TreeCrossFadeLength, m_TreeMaximumFullLODCount, m_DetailObjectDistance, m_DetailObjectDensity, m_HeightmapPixelError, m_SplatMapDistance, m_HeightmapMaximumLOD, m_CastShadows
	, m_DrawHeightmap, m_DrawTreesAndFoliage, m_ReflectionProbeUsage, m_MaterialType, m_LegacySpecular, m_LegacyShininess, m_MaterialTemplate, m_BakeLightProbesForTrees
	, m_ScaleInLightmap, m_LightmapParameters
);
ISO_DEFUSERCOMPV(unity::RectTransform
	, m_ObjectHideFlags, m_PrefabParentObject, m_PrefabInternal, m_GameObject, m_LocalRotation, m_LocalPosition, m_LocalScale, m_LocalEulerAnglesHint
	, m_Children, m_Father, m_RootOrder, m_AnchorMin, m_AnchorMax, m_AnchoredPosition, m_SizeDelta, m_Pivot
);

ISO_DEFUSERCOMPV(unity::MeshBlendShapeChannel, name, nameHash, frameIndex, frameCount);
ISO_DEFUSERCOMPV(unity::BlendShapeVertex, vertex, normal, tangent, index);
ISO_DEFUSERCOMPV(unity::MeshBlendShape, firstVertex, vertexCount, hasNormals, hasTangents);
ISO_DEFUSERCOMPV(unity::BlendShapeData, vertices, shapes, channels, fullWeights);
ISO_DEFUSERCOMPV(unity::ChannelInfo, stream, offset, format, dimension);
ISO_DEFUSERCOMPV(unity::BoneInfluence, weight, boneIndex);
ISO_DEFUSERCOMPV(unity::VertexData, m_CurrentChannels, m_VertexCount, m_Channels, m_Data);
ISO_DEFUSERCOMPV(unity::PackedBitVector, m_NumItems, m_Range, m_Start, m_Data, m_BitSize);

ISO_DEFUSERCOMPV(unity::CompressedMesh
	, m_Vertices, m_UV, m_Normals, m_Tangents, m_Weights, m_NormalSigns, m_TangentSigns, m_FloatColors
	, m_BoneIndices, m_Triangles, m_UVInfo
);

ISO_DEFUSERCOMPV(unity::ShadowSettings, m_Type, m_Resolution, m_CustomResolution, m_Strength, m_Bias, m_NormalBias, m_NearPlane);
ISO_DEFUSERCOMPV(unity::GISettings, m_BounceScale, m_IndirectOutputScale, m_AlbedoBoost, m_TemporalCoherenceThreshold, m_EnvironmentLightingMode, m_EnableBakedLightmaps, m_EnableRealtimeLightmaps);

ISO_DEFUSERCOMPV(unity::LightmapEditorSettings
	, m_Resolution, m_BakeResolution, m_TextureWidth, m_TextureHeight, m_AO, m_AOMaxDistance, m_CompAOExponent, m_CompAOExponentDirect
	, m_Padding, m_LightmapParameters, m_LightmapsBakeMode, m_TextureCompression, m_DirectLightInLightProbes, m_FinalGather, m_FinalGatherFiltering, m_FinalGatherRayCount
	, m_ReflectionCompression
);
ISO_DEFUSERCOMPV(unity::OcclusionBakeSettings, smallestOccluder, smallestHole, backfaceThreshold);

ISO_DEFUSER(unity::SkinnedMeshRenderer, anything);
ISO_DEFUSER(unity::TerrainData, anything);
ISO_DEFUSER(unity::MeshParticleEmitter, anything);
ISO_DEFUSER(unity::TrailRenderer, anything);
ISO_DEFUSER(unity::Projector, anything);
ISO_DEFUSER(unity::LineRenderer, anything);
ISO_DEFUSER(unity::SpriteRenderer, anything);
ISO_DEFUSER(unity::CanvasRenderer, anything);
ISO_DEFUSER(unity::Canvas, anything);
ISO_DEFUSER(unity::ParticleAnimator, anything);
ISO_DEFUSER(unity::EllipsoidParticleEmitter, anything);
ISO_DEFUSER(unity::WorldParticleCollider, anything);
ISO_DEFUSER(unity::ParticleRenderer, anything);
ISO_DEFUSER(unity::HingeJoint, anything);
ISO_DEFUSER(unity::ConstantForce, anything);
ISO_DEFUSER(unity::FixedJoint, anything);
ISO_DEFUSER(unity::CharacterController, anything);
ISO_DEFUSER(unity::CharacterJoint, anything);
ISO_DEFUSER(unity::SpringJoint, anything);
ISO_DEFUSER(unity::WheelCollider, anything);
ISO_DEFUSER(unity::ConfigurableJoint, anything);
ISO_DEFUSER(unity::Cloth, anything);
ISO_DEFUSER(unity::Rigidbody2D, anything);
ISO_DEFUSER(unity::CircleCollider2D, anything);
ISO_DEFUSER(unity::PolygonCollider2D, anything);
ISO_DEFUSER(unity::BoxCollider2D, anything);
ISO_DEFUSER(unity::EdgeCollider2D, anything);
ISO_DEFUSER(unity::SpringJoint2D, anything);
ISO_DEFUSER(unity::DistanceJoint2D, anything);
ISO_DEFUSER(unity::HingeJoint2D, anything);
ISO_DEFUSER(unity::SliderJoint2D, anything);
ISO_DEFUSER(unity::WheelJoint2D, anything);
ISO_DEFUSER(unity::ConstantForce2D, anything);
ISO_DEFUSER(unity::AreaEffector2D, anything);
ISO_DEFUSER(unity::PointEffector2D, anything);
ISO_DEFUSER(unity::PlatformEffector2D, anything);
ISO_DEFUSER(unity::SurfaceEffector2D, anything);
ISO_DEFUSER(unity::AssetDatabase, anything);
ISO_DEFUSER(unity::AssetServerCache, anything);
ISO_DEFUSER(unity::InspectorExpandedState, anything);
ISO_DEFUSER(unity::MonoManager, anything);
ISO_DEFUSER(unity::EditorUserSettings, anything);
ISO_DEFUSER(unity::EditorUserBuildSettings, anything);
ISO_DEFUSER(unity::TimeManager, anything);
ISO_DEFUSER(unity::AudioManager, anything);
ISO_DEFUSER(unity::InputManager, anything);
ISO_DEFUSER(unity::Physics2DSettings, anything);
ISO_DEFUSER(unity::GraphicsSettings, anything);
ISO_DEFUSER(unity::QualitySettings, anything);
ISO_DEFUSER(unity::PhysicsManager, anything);
ISO_DEFUSER(unity::TagManager, anything);
ISO_DEFUSER(unity::Avatar, anything);
ISO_DEFUSER(unity::RuntimeAnimatorController, anything);
ISO_DEFUSER(unity::Animator, anything);
ISO_DEFUSER(unity::NavMeshAreas, anything);
ISO_DEFUSER(unity::PhysicMaterial, anything);
ISO_DEFUSER(unity::NetworkManager, anything);
ISO_DEFUSER(unity::EditorSettings, anything);
ISO_DEFUSER(unity::ShaderVariantCollection, anything);
ISO_DEFUSER(unity::LightProbes, anything);
ISO_DEFUSER(unity::AnnotationManager, anything);
