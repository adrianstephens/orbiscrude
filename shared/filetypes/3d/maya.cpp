#define USING_VSMATH
#define REQUIRE_IOSTREAM

#include "base/defs.h"
#include <math.h>

#include "base/maths.h"

#ifdef PLAT_MAC
#define OSMac_
#elif define PLAT_PC
#define NT_APP
#endif

#include "maya/MLibrary.h"
#include "maya/MFileIO.h"
#include "maya/MPointArray.h"
#include "maya/MFloatVectorArray.h"
#include "maya/MDoubleArray.h"
#include "maya/MFloatMatrix.h"
#include "maya/MMatrix.h"

#include "maya/MObject.h"
#include "maya/MDagPath.h"

#include "maya/MItDag.h"
#include "maya/MItDependencyNodes.h"
#include "maya/MItDependencyGraph.h"

#include "maya/MFnDagNode.h"
#include "maya/MFnTransform.h"

#include "maya/MFnMesh.h"
#include "maya/MItMeshPolygon.h"
#include "maya/MItMeshVertex.h"

#include "maya/MFnNurbsSurface.h"
#include "maya/MFnNurbsCurve.h"
#include "maya/MTrimBoundaryArray.h"

#undef NAN

#include "iso/iso_files.h"
#include "model_utils.h"
#include "systems/mesh/patch.h"
#include "systems/mesh/nurbs.h"
#include "extra/indexer.h"
#include "maths/geometry.h"
#include "filetypes/iff.h"
#include "filetypes/bitmap/bitmap.h"
#include "vector_iso.h"


#ifdef WIN32
#pragma comment(lib,"Foundation.lib")
#pragma comment(lib,"OpenMaya.lib")
#pragma comment(lib,"Image.lib")
#endif

namespace iso {
	inline size_t	to_string(char *s, const MString &m)	{ return string_copy(s, m.asChar()); }

	float4 to_iso(const MPoint &p) {
		return float4{(float)p.x, (float)p.y, (float)p.z, (float)p.w};
	}
}

using namespace iso;

static const char *maya_ids[] = {
"kInvalid",								"kBase",
"kNamedObject",							"kModel",								"kDependencyNode",						"kAddDoubleLinear",						"kAffect",								"kAnimCurve",							"kAnimCurveTimeToAngular",				"kAnimCurveTimeToDistance",
"kAnimCurveTimeToTime",					"kAnimCurveTimeToUnitless",				"kAnimCurveUnitlessToAngular",			"kAnimCurveUnitlessToDistance",			"kAnimCurveUnitlessToTime",				"kAnimCurveUnitlessToUnitless",			"kResultCurve",							"kResultCurveTimeToAngular",
"kResultCurveTimeToDistance",			"kResultCurveTimeToTime",				"kResultCurveTimeToUnitless",			"kAngleBetween",						"kAudio",								"kBackground",							"kColorBackground",						"kFileBackground",
"kRampBackground",						"kBlend",								"kBlendTwoAttr",						"kBlendWeighted",						"kBlendDevice",							"kBlendColors",							"kBump",								"kBump3d",
"kCameraView",							"kChainToSpline",						"kChoice",								"kCondition",							"kContrast",							"kClampColor",							"kCreate",								"kAlignCurve",
"kAlignSurface",						"kAttachCurve",							"kAttachSurface",						"kAvgCurves",							"kAvgSurfacePoints",					"kAvgNurbsSurfacePoints",				"kBevel",								"kBirailSrf",
"kDPbirailSrf",							"kMPbirailSrf",							"kSPbirailSrf",							"kBoundary",							"kCircle",								"kCloseCurve",							"kClosestPointOnSurface",				"kCloseSurface",
"kCurveFromSurface",					"kCurveFromSurfaceBnd",					"kCurveFromSurfaceCoS",					"kCurveFromSurfaceIso",					"kCurveInfo",							"kDetachCurve",							"kDetachSurface",						"kExtendCurve",
"kExtendSurface",						"kExtrude",								"kFFblendSrf",							"kFFfilletSrf",							"kFilletCurve",							"kFitBspline",							"kFlow",								"kHardenPointCurve",
"kIllustratorCurve",					"kInsertKnotCrv",						"kInsertKnotSrf",						"kIntersectSurface",					"kNurbsTesselate",						"kNurbsPlane",							"kNurbsCube",							"kOffsetCos",
"kOffsetCurve",							"kPlanarTrimSrf",						"kPointOnCurveInfo",					"kPointOnSurfaceInfo",					"kPrimitive",							"kProjectCurve",						"kProjectTangent",						"kRBFsurface",
"kRebuildCurve",						"kRebuildSurface",						"kReverseCurve",						"kReverseSurface",						"kRevolve",								"kRevolvedPrimitive",					"kCone",								"kRenderCone",
"kCylinder",							"kSphere",								"kSkin",								"kStitchSrf",							"kSubCurve",							"kSurfaceInfo",							"kTextCurves",							"kTrim",
"kUntrim",								"kDagNode",								"kProxy",								"kUnderWorld",							"kTransform",							"kAimConstraint",						"kLookAt",								"kGeometryConstraint",
"kGeometryVarGroup",					"kAnyGeometryVarGroup",					"kCurveVarGroup",						"kMeshVarGroup",						"kSurfaceVarGroup",						"kIkEffector",							"kIkHandle",							"kJoint",
"kManipulator3D",						"kArrowManip",							"kAxesActionManip",						"kBallProjectionManip",					"kCircleManip",							"kScreenAlignedCircleManip",			"kCircleSweepManip",					"kConcentricProjectionManip",
"kCubicProjectionManip",				"kCylindricalProjectionManip",			"kDiscManip",							"kFreePointManip",						"kCenterManip",							"kLimitManip",							"kEnableManip",							"kFreePointTriadManip",
"kPropMoveTriadManip",					"kTowPointManip",						"kPolyCreateToolManip",					"kPolySplitToolManip",					"kGeometryOnLineManip",					"kCameraPlaneManip",					"kToggleOnLineManip",					"kStateManip",
"kIsoparmManip",						"kLineManip",							"kManipContainer",						"kAverageCurveManip",					"kBarnDoorManip",						"kBevelManip",							"kBlendManip",							"kButtonManip",
"kCameraManip",							"kCoiManip",							"kCpManip",								"kCreateCVManip",						"kCreateEPManip",						"kCurveEdManip",						"kCurveSegmentManip",					"kDirectionManip",
"kDofManip",							"kDropoffManip",						"kExtendCurveDistanceManip",			"kExtrudeManip",						"kIkSplineManip",						"kIkRPManip",							"kJointClusterManip",					"kLightManip",
"kMotionPathManip",						"kOffsetCosManip",						"kOffsetCurveManip",					"kProjectionManip",						"kPolyProjectionManip",					"kProjectionUVManip",					"kProjectionMultiManip",				"kProjectTangentManip",
"kPropModManip",						"kQuadPtOnLineManip",					"kRbfSrfManip",							"kReverseCurveManip",					"kReverseCrvManip",						"kReverseSurfaceManip",					"kRevolveManip",						"kRevolvedPrimitiveManip",
"kSpotManip",							"kSpotCylinderManip",					"kTriplanarProjectionManip",			"kTrsManip",							"kDblTrsManip",							"kPivotManip2D",						"kManip2DContainer",					"kPolyMoveUVManip",
"kPolyMappingManip",					"kPolyModifierManip",					"kPolyMoveVertexManip",					"kPolyVertexNormalManip",				"kTexSmudgeUVManip",					"kTexLatticeDeformManip",				"kTexLattice",							"kTexSmoothManip",
"kTrsTransManip",						"kTrsInsertManip",						"kTrsXformManip",						"kManipulator2D",						"kTranslateManip2D",					"kPlanarProjectionManip",				"kPointOnCurveManip",					"kTowPointOnCurveManip",
"kMarkerManip",							"kPointOnLineManip",					"kPointOnSurfaceManip",					"kTranslateUVManip",					"kRotateBoxManip",						"kRotateManip",							"kHandleRotateManip",					"kRotateLimitsManip",
"kScaleLimitsManip",					"kScaleManip",							"kScalingBoxManip",						"kScriptManip",							"kSphericalProjectionManip",			"kTextureManip3D",						"kToggleManip",							"kTranslateBoxManip",
"kTranslateLimitsManip",				"kTranslateManip",						"kTrimManip",							"kJointTranslateManip",					"kManipulator",							"kCirclePointManip",					"kDimensionManip",						"kFixedLineManip",
"kLightProjectionGeometry",				"kLineArrowManip",						"kPointManip",							"kTriadManip",							"kNormalConstraint",					"kOrientConstraint",					"kPointConstraint",						"kParentConstraint",
"kPoleVectorConstraint",				"kScaleConstraint",						"kTangentConstraint",					"kUnknownTransform",					"kWorld",								"kShape",								"kBaseLattice",							"kCamera",
"kCluster",								"kSoftMod",								"kCollision",							"kDummy",								"kEmitter",								"kField",								"kAir",									"kDrag",
"kGravity",								"kNewton",								"kRadial",								"kTurbulence",							"kUniform",								"kVortex",								"kGeometric",							"kCurve",
"kNurbsCurve",							"kNurbsCurveGeom",						"kDimension",							"kAngle",								"kAnnotation",							"kDistance",							"kArcLength",							"kRadius",
"kParamDimension",						"kDirectedDisc",						"kRenderRect",							"kEnvFogShape",							"kLattice",								"kLatticeGeom",							"kLocator",								"kDropoffLocator",
"kMarker",								"kOrientationMarker",					"kPositionMarker",						"kOrientationLocator",					"kTrimLocator",							"kPlane",								"kSketchPlane",							"kGroundPlane",
"kOrthoGrid",							"kSprite",								"kSurface",								"kNurbsSurface",						"kNurbsSurfaceGeom",					"kMesh",								"kMeshGeom",							"kRenderSphere",
"kFlexor",								"kClusterFlexor",						"kGuideLine",							"kLight",								"kAmbientLight",						"kNonAmbientLight",						"kAreaLight",							"kLinearLight",
"kNonExtendedLight",					"kDirectionalLight",					"kPointLight",							"kSpotLight",							"kParticle",							"kPolyToolFeedbackShape",				"kRigidConstraint",						"kRigid",
"kSpring",								"kUnknownDag",							"kDefaultLightList",					"kDeleteComponent",						"kDispatchCompute",						"kShadingEngine",						"kDisplacementShader",					"kDistanceBetween",
"kDOF",									"kDummyConnectable",					"kDynamicsController",					"kGeoConnectable",						"kExpression",							"kExtract",								"kFilter",								"kFilterClosestSample",
"kFilterEuler",							"kFilterSimplify",						"kGammaCorrect",						"kGeometryFilt",						"kBendLattice",							"kBlendShape",							"kBulgeLattice",						"kFFD",
"kFfdDualBase",							"kRigidDeform",							"kSculpt",								"kTweak",								"kWeightGeometryFilt",					"kClusterFilter",						"kSoftModFilter",						"kJointCluster",
"kWire",								"kGroupId",								"kGroupParts",							"kGuide",								"kHsvToRgb",							"kHyperGraphInfo",						"kHyperLayout",							"kHyperView",
"kIkSolver",							"kMCsolver",							"kPASolver",							"kSCsolver",							"kRPsolver",							"kSplineSolver",						"kIkSystem",							"kImagePlane",
"kLambert",								"kReflect",								"kBlinn",								"kPhong",								"kPhongExplorer",						"kLayeredShader",						"kLightInfo",							"kLeastSquares",
"kLightFogMaterial",					"kEnvFogMaterial",						"kLightList",							"kLightSource",							"kLuminance",							"kMakeGroup",							"kMaterial",							"kDiffuseMaterial",
"kLambertMaterial",						"kBlinnMaterial",						"kPhongMaterial",						"kLightSourceMaterial",					"kMaterialInfo",						"kMatrixAdd",							"kMatrixHold",							"kMatrixMult",
"kMatrixPass",							"kMatrixWtAdd",							"kMidModifier",							"kMidModifierWithMatrix",				"kPolyBevel",							"kPolyTweak",							"kPolyAppend",							"kPolyChipOff",
"kPolyCloseBorder",						"kPolyCollapseEdge",					"kPolyCollapseF",						"kPolyCylProj",							"kPolyDelEdge",							"kPolyDelFacet",						"kPolyDelVertex",						"kPolyExtrudeFacet",
"kPolyMapCut",							"kPolyMapDel",							"kPolyMapSew",							"kPolyMergeEdge",						"kPolyMergeFacet",						"kPolyMoveEdge",						"kPolyMoveFacet",						"kPolyMoveFacetUV",
"kPolyMoveUV",							"kPolyMoveVertex",						"kPolyMoveVertexUV",					"kPolyNormal",							"kPolyPlanProj",						"kPolyProj",							"kPolyQuad",							"kPolySmooth",
"kPolySoftEdge",						"kPolySphProj",							"kPolySplit",							"kPolySubdEdge",						"kPolySubdFacet",						"kPolyTriangulate",						"kPolyCreator",							"kPolyPrimitive",
"kPolyCone",							"kPolyCube",							"kPolyCylinder",						"kPolyMesh",							"kPolySphere",							"kPolyTorus",							"kPolyCreateFacet",						"kPolyUnite",
"kMotionPath",							"kMultilisterLight",					"kMultiplyDivide",						"kOldGeometryConstraint",				"kOpticalFX",							"kParticleAgeMapper",					"kParticleCloud",						"kParticleColorMapper",
"kParticleIncandecenceMapper",			"kParticleTransparencyMapper",			"kPartition",							"kPlace2dTexture",						"kPlace3dTexture",						"kPluginDependNode",					"kPluginLocatorNode",					"kPlusMinusAverage",
"kPointMatrixMult",						"kPolySeparate",						"kPostProcessList",						"kProjection",							"kRecord",								"kRenderUtilityList",					"kReverse",								"kRgbToHsv",
"kRigidSolver",							"kSet",									"kTextureBakeSet",						"kVertexBakeSet",						"kSetRange",							"kShaderGlow",							"kShaderList",							"kShadingMap",
"kSamplerInfo",							"kShapeFragment",						"kSimpleVolumeShader",					"kSl60",								"kSnapshot",							"kStoryBoard",							"kSummaryObject",						"kSuper",
"kControl",								"kSurfaceLuminance",					"kSurfaceShader",						"kTextureList",							"kTextureEnv",							"kEnvBall",								"kEnvCube",								"kEnvChrome",
"kEnvSky",								"kEnvSphere",							"kTexture2d",							"kBulge",								"kChecker",								"kCloth",								"kFileTexture",							"kFractal",
"kGrid",								"kMountain",							"kRamp",								"kStencil",								"kWater",								"kTexture3d",							"kBrownian",							"kCloud",
"kCrater",								"kGranite",								"kLeather",								"kMarble",								"kRock",								"kSnow",								"kSolidFractal",						"kStucco",
"kTxSl",								"kWood",								"kTime",								"kTimeToUnitConversion",				"kRenderSetup",							"kRenderGlobals",						"kRenderGlobalsList",					"kRenderQuality",
"kResolution",							"kHardwareRenderGlobals",				"kArrayMapper",							"kUnitConversion",						"kUnitToTimeConversion",				"kUseBackground",						"kUnknown",								"kVectorProduct",
"kVolumeShader",						"kComponent",							"kCurveCVComponent",					"kCurveEPComponent",					"kCurveKnotComponent",					"kCurveParamComponent",					"kIsoparmComponent",					"kPivotComponent",
"kSurfaceCVComponent",					"kSurfaceEPComponent",					"kSurfaceKnotComponent",				"kEdgeComponent",						"kLatticeComponent",					"kSurfaceRangeComponent",				"kDecayRegionCapComponent",				"kDecayRegionComponent",
"kMeshComponent",						"kMeshEdgeComponent",					"kMeshPolygonComponent",				"kMeshFrEdgeComponent",					"kMeshVertComponent",					"kMeshFaceVertComponent",				"kOrientationComponent",				"kSubVertexComponent",
"kMultiSubVertexComponent",				"kSetGroupComponent",					"kDynParticleSetComponent",				"kSelectionItem",						"kDagSelectionItem",					"kNonDagSelectionItem",					"kItemList",							"kAttribute",
"kNumericAttribute",					"kDoubleAngleAttribute",				"kFloatAngleAttribute",					"kDoubleLinearAttribute",				"kFloatLinearAttribute",				"kTimeAttribute",						"kEnumAttribute",						"kUnitAttribute",
"kTypedAttribute",						"kCompoundAttribute",					"kGenericAttribute",					"kLightDataAttribute",					"kMatrixAttribute",						"kFloatMatrixAttribute",				"kMessageAttribute",					"kPlugin",
"kData",								"kComponentListData",					"kDoubleArrayData",						"kIntArrayData",						"kLatticeData",							"kMatrixData",							"kMeshData",							"kNurbsSurfaceData",
"kNurbsCurveData",						"kNumericData",							"kData2Double",							"kData2Float",							"kData2Int",							"kData2Short",							"kData3Double",							"kData3Float",
"kData3Int",							"kData3Short",							"kPluginData",							"kPointArrayData",						"kSphereData",							"kStringData",							"kStringArrayData",						"kVectorArrayData",
"kSelectionList",						"kTransformGeometry",					"kCommEdgePtManip",						"kCommEdgeOperManip",					"kCommEdgeSegmentManip",				"kCommCornerManip",						"kCommCornerOperManip",					"kPluginDeformerNode",
"kTorus",								"kPolyBoolOp",							"kSingleShadingSwitch",					"kDoubleShadingSwitch",					"kTripleShadingSwitch",					"kNurbsSquare",							"kAnisotropy",							"kNonLinear",
"kDeformFunc",							"kDeformBend",							"kDeformTwist",							"kDeformSquash",						"kDeformFlare",							"kDeformSine",							"kDeformWave",							"kDeformBendManip",
"kDeformTwistManip",					"kDeformSquashManip",					"kDeformFlareManip",					"kDeformSineManip",						"kDeformWaveManip",						"kSoftModManip",						"kDistanceManip",						"kScript",
"kCurveFromMeshEdge",					"kCurveCurveIntersect",					"kNurbsCircular3PtArc",					"kNurbsCircular2PtArc",					"kOffsetSurface",						"kRoundConstantRadius",					"kRoundRadiusManip",					"kRoundRadiusCrvManip",
"kRoundConstantRadiusManip",			"kThreePointArcManip",					"kTwoPointArcManip",					"kTextButtonManip",						"kOffsetSurfaceManip",					"kImageData",							"kImageLoad",							"kImageSave",
"kImageNetSrc",							"kImageNetDest",						"kImageRender",							"kImageAdd",							"kImageDiff",							"kImageMultiply",						"kImageOver",							"kImageUnder",
"kImageColorCorrect",					"kImageBlur",							"kImageFilter",							"kImageDepth",							"kImageDisplay",						"kImageView",							"kImageMotionBlur",						"kViewColorManager",
"kMatrixFloatData",						"kSkinShader",							"kComponentManip",						"kSelectionListData",					"kObjectFilter",						"kObjectMultiFilter",					"kObjectNameFilter",					"kObjectTypeFilter",
"kObjectAttrFilter",					"kObjectRenderFilter",					"kObjectScriptFilter",					"kSelectionListOperator",				"kSubdiv",								"kPolyToSubdiv",						"kSkinClusterFilter",					"kKeyingGroup",
"kCharacter",							"kCharacterOffset",						"kDagPose",								"kStitchAsNurbsShell",					"kExplodeNurbsShell",					"kNurbsBoolean",						"kStitchSrfManip",						"kForceUpdateManip",
"kPluginManipContainer",				"kPolySewEdge",							"kPolyMergeVert",						"kPolySmoothFacet",						"kSmoothCurve",							"kGlobalStitch",						"kSubdivCVComponent",					"kSubdivEdgeComponent",
"kSubdivFaceComponent",					"kUVManip2D",							"kTranslateUVManip2D",					"kRotateUVManip2D",						"kScaleUVManip2D",						"kPolyTweakUV",							"kMoveUVShellManip2D",					"kPluginShape",
"kGeometryData",						"kSingleIndexedComponent",				"kDoubleIndexedComponent",				"kTripleIndexedComponent",				"kExtendSurfaceDistanceManip",			"kSquareSrf",							"kSquareSrfManip",						"kSubdivToPoly",
"kDynBase",								"kDynEmitterManip",						"kDynFieldsManip",						"kDynBaseFieldManip",					"kDynAirManip",							"kDynNewtonManip",						"kDynTurbulenceManip",					"kDynSpreadManip",
"kDynAttenuationManip",					"kDynArrayAttrsData",					"kPluginFieldNode",						"kPluginEmitterNode",					"kPluginSpringNode",					"kDisplayLayer",						"kDisplayLayerManager",					"kPolyColorPerVertex",
"kCreateColorSet",						"kDeleteColorSet",						"kCopyColorSet",						"kBlendColorSet",						"kPolyColorMod",						"kPolyColorDel",						"kCharacterMappingData",				"kDynSweptGeometryData",
"kWrapFilter",							"kMeshVtxFaceComponent",				"kBinaryData",							"kAttribute2Double",					"kAttribute2Float",						"kAttribute2Short",						"kAttribute2Int",						"kAttribute3Double",
"kAttribute3Float",						"kAttribute3Short",						"kAttribute3Int",						"kReference",							"kBlindData",							"kBlindDataTemplate",					"kPolyBlindData",						"kPolyNormalPerVertex",
"kNurbsToSubdiv",						"kPluginIkSolver",						"kInstancer",							"kMoveVertexManip",						"kStroke",								"kBrush",								"kStrokeGlobals",						"kPluginGeometryData",
"kLightLink",							"kDynGlobals",							"kPolyReduce",							"kLodThresholds",						"kChooser",								"kLodGroup",							"kMultDoubleLinear",					"kFourByFourMatrix",
"kTowPointOnSurfaceManip",				"kSurfaceEdManip",						"kSurfaceFaceComponent",				"kClipScheduler",						"kClipLibrary",							"kSubSurface",							"kSmoothTangentSrf",					"kRenderPass",
"kRenderPassSet",						"kRenderLayer",							"kRenderLayerManager",					"kPassContributionMap",					"kPrecompExport",						"kRenderTarget",						"kRenderedImageSource",					"kImageSource",
"kPolyFlipEdge",						"kPolyExtrudeEdge",						"kAnimBlend",							"kAnimBlendInOut",						"kPolyAppendVertex",					"kUvChooser",							"kSubdivCompId",						"kVolumeAxis",
"kDeleteUVSet",							"kSubdHierBlind",						"kSubdBlindData",						"kCharacterMap",						"kLayeredTexture",						"kSubdivCollapse",						"kParticleSamplerInfo",					"kCopyUVSet",
"kCreateUVSet",							"kClip",								"kPolySplitVert",						"kSubdivData",							"kSubdivGeom",							"kUInt64ArrayData",						"kPolySplitEdge",						"kSubdivReverseFaces",
"kMeshMapComponent",					"kSectionManip",						"kXsectionSubdivEdit",					"kSubdivToNurbs",						"kEditCurve",							"kEditCurveManip",						"kCrossSectionManager",					"kCreateSectionManip",
"kCrossSectionEditManip",				"kDropOffFunction",						"kSubdBoolean",							"kSubdModifyEdge",						"kModifyEdgeCrvManip",					"kModifyEdgeManip",						"kScalePointManip",						"kTransformBoxManip",
"kSymmetryLocator",						"kSymmetryMapVector",					"kSymmetryMapCurve",					"kCurveFromSubdivEdge",					"kCreateBPManip",						"kModifyEdgeBaseManip",					"kSubdExtrudeFace",						"kSubdivSurfaceVarGroup",
"kSfRevolveManip",						"kCurveFromSubdivFace",					"kUnused1",								"kUnused2",								"kUnused3",								"kUnused4",								"kUnused5",								"kUnused6",
"kPolyTransfer",						"kPolyAverageVertex",					"kPolyAutoProj",						"kPolyLayoutUV",						"kPolyMapSewMove",						"kSubdModifier",						"kSubdMoveVertex",						"kSubdMoveEdge",
"kSubdMoveFace",						"kSubdDelFace",							"kSnapshotShape",						"kSubdivMapComponent",					"kJiggleDeformer",						"kGlobalCacheControls",					"kDiskCache",							"kSubdCloseBorder",
"kSubdMergeVert",						"kBoxData",								"kBox",									"kRenderBox",							"kSubdSplitFace",						"kVolumeFog",							"kSubdTweakUV",							"kSubdMapCut",
"kSubdLayoutUV",						"kSubdMapSewMove",						"kOcean",								"kVolumeNoise",							"kSubdAutoProj",						"kSubdSubdivideFace",					"kNoise",								"kAttribute4Double",
"kData4Double",							"kSubdPlanProj",						"kSubdTweak",							"kSubdProjectionManip",					"kSubdMappingManip",					"kHardwareReflectionMap",				"kPolyNormalizeUV",						"kPolyFlipUV",
"kHwShaderNode",						"kPluginHardwareShader",				"kPluginHwShaderNode",					"kSubdAddTopology",						"kSubdCleanTopology",					"kImplicitCone",						"kImplicitSphere",						"kRampShader",
"kVolumeLight",							"kOceanShader",							"kBevelPlus",							"kStyleCurve",							"kPolyCut",								"kPolyPoke",							"kPolyWedgeFace",						"kPolyCutManipContainer",
"kPolyCutManip",						"kPolyPokeManip",						"kFluidTexture3D",						"kFluidTexture2D",						"kPolyMergeUV",							"kPolyStraightenUVBorder",				"kAlignManip",							"kPluginTransformNode",
"kFluid",								"kFluidGeom",							"kFluidData",							"kSmear",								"kStringShadingSwitch",					"kStudioClearCoat",						"kFluidEmitter",						"kHeightField",
"kGeoConnector",						"kSnapshotPath",						"kPluginObjectSet",						"kQuadShadingSwitch",					"kPolyExtrudeVertex",					"kPairBlend",							"kTextManip",							"kViewManip",
"kXformManip",							"kMute",								"kConstraint",							"kTrimWithBoundaries",					"kCurveFromMeshCoM",					"kFollicle",							"kHairSystem",							"kRemapValue",
"kRemapColor",							"kRemapHsv",							"kHairConstraint",						"kTimeFunction",						"kMentalRayTexture",					"kObjectBinFilter",						"kPolySmoothProxy",						"kPfxGeometry",
"kPfxHair",								"kHairTubeShader",						"kPsdFileTexture",						"kKeyframeDelta",						"kKeyframeDeltaMove",					"kKeyframeDeltaScale",					"kKeyframeDeltaAddRemove",				"kKeyframeDeltaBlockAddRemove",
"kKeyframeDeltaInfType",				"kKeyframeDeltaTangent",				"kKeyframeDeltaWeighted",				"kKeyframeDeltaBreakdown",				"kPolyMirror",							"kPolyCreaseEdge",						"kHikEffector",							"kHikIKEffector",
"kHikFKJoint",							"kHikSolver",							"kHikHandle",							"kProxyManager",						"kPolyAutoProjManip",					"kPolyPrism",							"kPolyPyramid",							"kPolySplitRing",
"kPfxToon",								"kToonLineAttributes",					"kPolyDuplicateEdge",					"kFacade",								"kMaterialFacade",						"kEnvFacade",							"kAISEnvFacade",						"kLineModifier",
"kPolyArrow",							"kPolyPrimitiveMisc",					"kPolyPlatonicSolid",					"kPolyPipe",							"kHikFloorContactMarker",				"kHikGroundPlane",						"kPolyComponentData",					"kPolyHelix",
"kCacheFile",							"kHistorySwitch",						"kClosestPointOnMesh",					"kTransferAttributes",					"kDynamicConstraint",					"kNComponent",							"kPolyBridgeEdge",						"kCacheableNode",
"kNucleus",								"kNBase",								"kCacheBase",							"kCacheBlend",							"kCacheTrack",							"kKeyframeRegionManip",					"kCurveNormalizerAngle",				"kCurveNormalizerLinear",
"kHyperLayoutDG",						"kPluginImagePlaneNode",				"kNCloth",								"kNParticle",							"kNRigid",								"kPluginParticleAttributeMapperNode",	"kCameraSet",							"kPluginCameraSet",
"kContainer",							"kFloatVectorArrayData",				"kNObjectData",							"kNObject",								"kPluginConstraintNode",				"kAsset",								"kPolyEdgeToCurve",						"kAnimLayer",
"kBlendNodeBase",						"kBlendNodeBoolean",					"kBlendNodeDouble",						"kBlendNodeDoubleAngle",				"kBlendNodeDoubleLinear",				"kBlendNodeEnum",						"kBlendNodeFloat",						"kBlendNodeFloatAngle",
"kBlendNodeFloatLinear",				"kBlendNodeInt16",						"kBlendNodeInt32",						"kBlendNodeAdditiveScale",				"kBlendNodeAdditiveRotation",			"kPluginManipulatorNode",				"kNIdData",								"kNId",
"kFloatArrayData",						"kMembrane",							"kMergeVertsToolManip",					"kUint64SingleIndexedComponent",		"kPolyToolFeedbackManip",				"kPolySelectEditFeedbackManip",			"kWriteToFrameBuffer",					"kWriteToColorBuffer",
"kWriteToVectorBuffer",					"kWriteToDepthBuffer",					"kWriteToLabelBuffer",					"kStereoCameraMaster",					"kSequenceManager",						"kSequencer",							"kShot",
"kBlendNodeTime",						"kCreateBezierManip",					"kBezierCurve",							"kBezierCurveData",						"kNurbsCurveToBezier",					"kBezierCurveToNurbs",
"kPolySpinEdge",						"kPolyHoleFace",						"kPointOnPolyConstraint",				"kPolyConnectComponents",				"kSkinBinding",							"kVolumeBindManip",						"kVertexWeightSet",						"kNearestPointOnCurve",
"kColorProfile",						"kAdskMaterial",						"kContainerBase",						"kDagContainer",						"kPolyUVRectangle",						"kHardwareRenderingGlobals",			"kPolyProjectCurve",					"kRenderingList",
"kPolyExtrudeManip",					"kPolyExtrudeManipContainer",			"kThreadedDevice",						"kClientDevice",						"kPluginClientDevice",					"kPluginThreadedDevice",				"kTimeWarp",							"kClipGhostShape",
"kClipToGhostData",						"kMandelbrot",							"kMandelbrot3D",
};

struct MayaLib {
	MayaLib(const char *maya_dir) {
		if (!getenv("MAYA_LOCATION"))
			_putenv(buffer_accum<256>("MAYA_LOCATION=") << maya_dir);

		string	path = getenv("path");
		_putenv(buffer_accum<256>("path=") << filename(maya_dir).add_dir("bin"));
		HMODULE h = LoadLibraryA("openmaya.dll");
		_putenv(buffer_accum<256>("path=") << path);

		ISO_VERIFY(MLibrary::initialize("Isopod"));
	}
	~MayaLib() {
		//MLibrary::cleanup();
	}
};

void PrintMayaDep(const MObject &node, int depth = 0) {
	for (MItDependencyGraph	i(unconst(node)); !i.isDone(); i.next()) {
		MObject 	p		= i.currentItem();
		if (p != node) {
			MString		name	= MFnDagNode(p).name();
			ISO_TRACEF() << repeat(' ', depth) << name.asChar() << " (" << maya_ids[p.apiType()] << ")\n";
			PrintMayaDep(p, depth + 1);
		}
	}
}

void PrintMayaDag(const MFnDagNode &node, int depth = 0) {
	for (int i = 0, n = node.childCount(); i < n; i++) {
		MObject 	p		= node.child(i);
		MString		name	= MFnDagNode(p).name();
		ISO_TRACEF() << repeat(' ', depth) << name.asChar() << " (" << maya_ids[p.apiType()] << ")\n";
		PrintMayaDag(p, depth + 1);
		PrintMayaDep(p, depth + 1);
	}
}

void PrintMayaConnections(const MFnDependencyNode &node, int depth = 0) {
	ISO_TRACEF() << repeat(' ', depth) << "node=" << node.name().asChar() << '\n';

	MPlugArray			plugs;
	node.getConnections(plugs);

	for (int i = 0; i < plugs.length(); i++) {
		MPlug			&plug = plugs[i];
		ISO_TRACEF() << repeat(' ', depth + 1) << "plug=" << plug.name().asChar() << '\n';

		MPlugArray		plugs2;
		plug.connectedTo(plugs2, true, false);
		for (int u = 0; u < plugs2.length(); u++)
			PrintMayaConnections(plugs2[u].node(), depth + 2);
	}
}

//-----------------------------------------------------------------------------
//	Mesh
//-----------------------------------------------------------------------------

struct MayaVertex {
	float3p		position;
	float3p		normal;
	float4p		colour;
	float2p		texcoord;
};

ISO_DEFCOMPV(MayaVertex, position, normal, colour, texcoord);

const char *GetTextureFilename(MObject node) {
	MString		filename;
	MFnDependencyNode(node).findPlug("ftn").getValue(filename);
	return filename.asChar();
}

bool AddParameter(anything &params, const MPlug &plug) {
//	PrintMayaConnections(plug.node(), 0);
	tag	name = plug.partialName(false, false, false, false, false, true).asChar();
	if (name == "outColor" || name == "message")
		return false;


	MPlugArray plugs;
	plug.connectedTo(plugs, true, true);

	const char *texname = 0;
	for (int i = 0; i != plugs.length(); ++i) {
		// if file texture found
		if (plugs[i].node().apiType() == MFn::kFileTexture) {
			texname = GetTextureFilename(plugs[i].node());

			ISO_ptr<void>	p = MakePtr(ISO::getdef<Texture>(), name);
			*(ISO_ptr<void>*)p	= MakePtrExternal(ISO::getdef<bitmap>(), texname);

			params.Append(p);
			return true;
		}
	}
	params.Append(ISO_ptr<int>(name));
	return true;
}

void GetVertex(MItMeshPolygon &poly, int i, int x, int *vt, int *vp, int *vn) {
	poly.getUVIndex(i, vt[x]);
	vp[x] = poly.vertexIndex(i);
	vn[x] = poly.normalIndex(i);
}

void GetTri(MItMeshPolygon &poly, int i0, int i1, int i2, int x, int *vt, int *vp, int *vn) {
	GetVertex(poly, i0, x + 0, vt, vp, vn);
	GetVertex(poly, i1, x + 1, vt, vp, vn);
	GetVertex(poly, i2, x + 2, vt, vp, vn);
}

ISO_ptr<Model3> GetMesh(const MFnMesh &fnMesh) {
	ISO_ptr<Model3>			model(fnMesh.name().asChar());
	cuboid					extall(empty);

	MObjectArray			shaders;
	MIntArray				indices;
	fnMesh.getConnectedShaders(0, shaders, indices);

	for (int i = 0; i < shaders.length(); i++) {
		MFnDependencyNode	shader(shaders[i]);
		ISO_ptr<SubMesh>	submesh(shader.name().asChar());

		submesh->technique	= ISO::root("data")["default"]["tex_specular"];

		anything			params;
		MPlugArray			connections;
		shader.findPlug("surfaceShader").connectedTo(connections, true, false);

		for (int u = 0; u < connections.length(); u++)  {
			MPlugArray			plugs;
			MFnDependencyNode(connections[u].node()).getConnections(plugs);
			for (int i = 0; i < plugs.length(); i++)
				AddParameter(params, plugs[i]);
		}

		submesh->parameters	= AnythingToStruct(params);
		model->submeshes.Append(submesh);
	}

	if (!model->submeshes) {
		ISO_ptr<SubMesh>	submesh(0);
		submesh->technique	= ISO::root("data")["default"]["tex_specular"];
		model->submeshes.Append(submesh);
	}

	int	nv	= fnMesh.numVertices();
	int	np	= fnMesh.numPolygons();
	int	nt	= 0;

	// count triangles
	for (MItMeshPolygon poly(fnMesh.object()); !poly.isDone(); poly.next()) {
		uint32 vcount = poly.polygonVertexCount();
		nt += vcount - 2;
	}

	ISO_ptr<SubMesh>		submesh = model->submeshes[0];
	SubMesh::face			*faces = submesh->indices.Create(nt);
	Indexer<uint32>			indexer(nt * 3);
	MPointArray				vertices;
	MFloatVectorArray		normals;

	fnMesh.getPoints(vertices);
	fnMesh.getNormals(normals);

	cuboid					ext(empty);
	for (int i = 0; i < nv; i++) {
		MPoint v = vertices[i];
		ext		|=	position3(float(v.x), float(v.y), float(v.z));
	}
	submesh->minext = ext.a;
	submesh->maxext = ext.b;

	extall	|= ext;

	int						f = 0;
	dynamic_array<int>		vt(nt * 3);
	dynamic_array<int>		vp(nt * 3);
	dynamic_array<int>		vn(nt * 3);
	for (MItMeshPolygon poly(fnMesh.object()); !poly.isDone(); poly.next()) {
		uint32 vcount = poly.polygonVertexCount();

		for (int i = 0; i < (vcount - 2) / 2; i++, f += 6) {
			int	j = vcount - 1 - i;
			GetTri(poly, i, j, i + 1,		f + 0,	vt, vp, vn);
			GetTri(poly, j - 1, i + 1, j,	f + 3,	vt, vp, vn);
//			GetTri(poly, i + 1,	j - 1, j,	f + 3,	vt, vp, vn);
		}

		if (vcount & 1) {
			int	i = (vcount - 2) / 2, j = vcount - 1 - i;
			GetTri(poly, i, j, i + 1,	f + 0,	vt, vp, vn);
			f += 3;
		}
	}

	indexer.SetIndices(vp.begin(), nv);
	indexer.Process(vt.begin(), equal_to());
	indexer.Process(vn.begin(), equal_to());

	// copy indices
	copy(indexer.Indices(), faces[0]);

	int	num_unique		= indexer.NumUnique();
	ISO_ptr<ISO_openarray<MayaVertex> >	pverts(0, num_unique);
	submesh->verts		= pverts;

	MFloatArray u_coords, v_coords;
	fnMesh.getUVs(u_coords, v_coords);

	MayaVertex			*verts	= *pverts;
	for (int i = 0; i < num_unique; i++) {
		int			j	= indexer.RevIndex(i);
		MPoint		&mv	= vertices[vp[j]];
		verts[i].position	= position3(float(mv.x), float(mv.y), float(mv.z));

		MFloatVector n = normals[vn[j]];
		n.normalize();
		verts[i].normal	= iso::float3{n.x, n.y, n.z};

		verts[i].texcoord.x	= u_coords[vt[j]];
		verts[i].texcoord.y	= v_coords[vt[j]];
		verts[i].colour		= float4(one);
	}

	model->minext = extall.a;
	model->maxext = extall.b;
	return model;
}

struct MayaPatch {
	float3p		position[16];
};

ISO_DEFUSERCOMPF(MayaPatch, 1, WRITETOBIN) { ISO_SETFIELD(0, position); }};
//namespace ISO {
//ISO_ptr<void> MakePtr(tag2 id, param(colour) c) { return MakePtr<float4p>(id, c); }
//}

#if 0
ISO_ptr<NurbsModel> GetPatch(MFnNurbsSurface nurbs) {
	ISO_ptr<PatchModel3>	model(nurbs.name().asChar());
	cuboid					ext(empty);

	ISO_ptr<ISO_openarray<MayaPatch> >	data(0, 1);

	SubPatch	&patch = model->subpatches.Create(1)[0];
	patch.flags	= 0;
	patch.verts = data;
	patch.technique	= MakePtrExternal<technique>("D:\\dev\\shared\\assets\\shaders\\nurbs.fx;bezierpatch");
//		iso::root("data")["default"]["bezierpatch"];

	MayaPatch	&p = (*data)[0];
	for (int i = 0; i < 16; i++) {
		int	x = i % 4, y = i / 4;
		p.position[i].set(x - 1.5f, 1.5f - y, x == 2 && y == 2);
		ext	|= position3(p.position[i]);
	}

	model->minext = ext.pt0();
	model->maxext = ext.pt1();
	return model;
}
#endif

float3x4p GetTransform(const MFnTransform &t) {
	MMatrix		m1 = t.transformationMatrix();
	float3x4p	m2;
	for (int i = 0; i < 4; i++)
		for (int j = 0; j < 3; j++)
			m2[i][j] = float(m1.matrix[j][i]);
	return m2;
}

cuboid GetNurbsPatch(SubPatch &patch, const MFnNurbsSurface &nurbs, param(float3x4) space) {
	cuboid				ext(empty);

	patch.flags		= 0;
	patch.technique	= ISO::MakePtrExternal<technique>("D:\\dev\\shared\\assets\\shaders\\nurbs.fx;nurbspatch");

	double startU, endU, startV, endV;
	nurbs.getKnotDomain(startU, endU, startV, endV);

	uint32	u_degree	= nurbs.degreeU();
	uint32	v_degree	= nurbs.degreeV();
	uint32	u_knots		= nurbs.numKnotsInU();
	uint32	v_knots		= nurbs.numKnotsInV();

	ISO_ptr<ISO_openarray<float>> knots("knots", u_knots + v_knots);

	MDoubleArray	knotsInU;
	nurbs.getKnotsInU(knotsInU);
	transform(&knotsInU[0], &knotsInU[u_knots], knots->begin(), [=](double d) {
		return (d - startU) / (endU - startU);
	});

	MDoubleArray	knotsInV;
	nurbs.getKnotsInV(knotsInV);
	transform(&knotsInV[0], &knotsInV[v_knots], knots->begin() + u_knots, [=](double d) {
		return (d - startV) / (endV - startV);
	});

	MPointArray		cvArray;
	nurbs.getCVs(cvArray);

	uint32	nu	= u_knots - u_degree + 1;
	uint32	nv	= v_knots - v_degree + 1;

	uint32	numCVs	= cvArray.length();
	ISO_ASSERT(numCVs == nu * nv);
	ISO_ptr<ISO_openarray<float4p> >	control(0, numCVs);
	patch.verts		= control;

	auto	*d		= control->begin();
	for (int i = 0; i < numCVs; i++) {
		float4	v	= to_iso(cvArray[(i % nu) * nv + nv - 1 - (i / nu)]);
		v.xyz = space * position3(v.xyz);
		ext |= position3(v.xyz);
		d[i] = v;
	}

	anything			params;
	params.Append(ISO::MakePtr("diffuse_colour", colour(1,0,0)));
	params.Append(ISO::MakePtr("glossiness", 60.f));
	params.Append(ISO::MakePtr("u_count", nu));

	ISO_ptr<void>	knots2	= MakePtr(ISO::getdef<DataBuffer>(), "knots");
	*(ISO_ptr<void>*)knots2	= knots;
	params.Append(knots2);

	patch.parameters	= AnythingToStruct(params);

	return ext;
}

struct MayaExporter {
	ISO_ptr<Scene>		scene;
	ISO_ptr<NurbsModel>	nurbs;
	float3x4			nurbs_space;
	float3x4			current_space;

	MayaExporter(tag id) : scene(id), current_space(identity) {
		scene->root.Create();
	}
	bool	Open(const filename &fn) {
		return MFileIO::open(MString(fn.convert_to_fwdslash()), NULL, true) == MS::kSuccess;
	}
	void	ExportTree(Node *parent, const MFnDagNode &node) {
		for (int i = 0, n = node.childCount(); i < n; i++) {
			MObject 	obj		= node.child(i);
			MString		name	= MFnDagNode(obj).name();

			switch (obj.apiType()) {
				case MFn::kTransform: {
					ISO_ptr<Node>	p(name.asChar());
					p->matrix = GetTransform(obj);
					save(current_space, float3x4(p->matrix) * current_space), ExportTree(p, obj);
					if (p->children.Count())
						parent->children.Append(p);
					break;
				}
				case MFn::kMesh:
					if (auto p2 = GetMesh(obj))
						parent->children.Append(p2);
					break;

				case MFn::kNurbsSurface: {
					if (!nurbs) {
						nurbs_space = current_space;
						nurbs.Create(MFnDependencyNode(obj).name().asChar());
						parent->children.Append(nurbs);
					}
					cuboid	ext = GetNurbsPatch(nurbs->subpatches.Append(), obj, current_space / nurbs_space);
					ext |= cuboid(position3(nurbs->minext), position3(nurbs->maxext));
					nurbs->minext = ext.a;
					nurbs->maxext = ext.b;
					break;
				}
			}
		}
	}
};

//-----------------------------------------------------------------------------
//	MayaFileHandler
//-----------------------------------------------------------------------------

class MayaFileHandler : public FileHandler {
	ISO_ptr<void>	ReadWithFilename(tag id, const filename &fn) override;
};

ISO_ptr<void> MayaFileHandler::ReadWithFilename(tag id, const filename &fn) {
	static MayaLib mayalib("C:\\Program Files\\Autodesk\\Maya2018");

	MayaExporter	exp(id);

	if (exp.Open(fn)) {
		for (MItDependencyNodes i(MFn::kWorld); !i.isDone(); i.next())
			exp.ExportTree(exp.scene->root, i.item());
	}
	return exp.scene;
}

class MBFileHandler : public MayaFileHandler {
	const char*		GetExt() override { return "mb";		}
	const char*		GetDescription() override { return "Maya binary (mb) container"; }
	int	Check(istream_ref file) override {
		if (file.length() > 16) {
			file.seek(0);
			IFF_chunk	chunk(file);
			if (chunk.is_ext('FORM') && chunk.remaining() == file.length() - 8 && file.get<uint32be>() == 'MAYA')
				return CHECK_PROBABLE;
		}
		return CHECK_DEFINITE_NO;
	}
} maya_mb;

class MAFileHandler : public MayaFileHandler {
	const char*		GetExt() override { return "ma";		}
	const char*		GetDescription() override { return "Maya Ascii container"; }
} maya_ma;
